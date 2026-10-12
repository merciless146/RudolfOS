//! Blitzen for Windows: the offline coding assistant. GPL-2.0-or-later.
//!
//! The window shows the Blitzen page (windows/ui). This host:
//!  - downloads llama.cpp (CUDA, Vulkan or CPU build, checked against GitHub's SHA-256) and the model named in the
//!    signed list "blitzen-win-plus" (checked against the release key in iso/trusted-keys.txt, then each file's SHA-256);
//!  - starts the engine on 127.0.0.1 with a random key when Blitzen is asked something, and unloads it after
//!    10 minutes idle or when a full-screen game starts;
//!  - runs Python and C++ answers in a sandbox (AppContainer + Job Object: no network, 10 seconds, 1 GB);
//!  - adds a tray icon, Win+Shift+B, and "Ask Blitzen about this file" in File Explorer.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod net;
mod sandbox;
mod sys;

use base64::Engine as _;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::fs;
use std::hash::{BuildHasher, Hasher};
use std::path::{Path, PathBuf};
use std::process::{Child, Stdio};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant, UNIX_EPOCH};
use tauri::{AppHandle, Emitter, Manager, State, WindowEvent};

const LLAMA_TAG: &str = "b11559";
const MODEL_LIST: &str = "https://github.com/merciless146/RudolfOS/releases/download/blitzen-win-plus/blitzen.txt";
const STAGE1_MODELS: &str = "C:\\Blitzen\\models";
const TRUSTED_KEYS: &str = include_str!("../../../../iso/trusted-keys.txt");
const CONTEXT: u32 = 16384;
const IDLE_UNLOAD: Duration = Duration::from_secs(600);

// ---------------------------------------------------------------------------------------------------------------
// state

#[derive(Serialize, Deserialize, Default, Clone)]
#[serde(default)]
struct EngineCfg {
    kind: String,
    tag: String,
    bin: String,
}

#[derive(Serialize, Deserialize, Default, Clone)]
#[serde(default)]
struct FileCfg {
    path: String,
    size: u64,
    sha256: String,
    mtime: u64,
}

#[derive(Serialize, Deserialize, Default, Clone)]
#[serde(default)]
struct ModelCfg {
    name: String,
    files: Vec<FileCfg>,
}

#[derive(Serialize, Deserialize, Default, Clone)]
#[serde(default)]
struct Config {
    engine: Option<EngineCfg>,
    model: Option<ModelCfg>,
    bench_tps: f64,
    granted: Vec<String>,
}

struct Engine {
    child: Child,
    port: u16,
    key: String,
    job: isize,
}

impl Drop for Engine {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
        if self.job != 0 {
            unsafe {
                windows_sys::Win32::Foundation::CloseHandle(self.job as windows_sys::Win32::Foundation::HANDLE);
            }
        }
    }
}

pub struct App {
    data: PathBuf,
    docs: PathBuf,
    cfg: Mutex<Config>,
    hw: Mutex<Option<sys::Hardware>>,
    eng: Mutex<Option<Engine>>,
    starting: Mutex<()>,
    setup: Mutex<()>,
    last_use: Mutex<Instant>,
    pending: Mutex<Option<Value>>,
    sbx: Mutex<Option<PathBuf>>,
}

impl App {
    fn new(app: &AppHandle) -> Result<App, String> {
        let data = app.path().app_local_data_dir().map_err(|e| e.to_string())?;
        let docs = app.path().document_dir().map_err(|e| e.to_string())?.join("Blitzen");
        for d in ["engine", "models", "downloads", "logs"] {
            fs::create_dir_all(data.join(d)).map_err(|e| e.to_string())?;
        }
        let cfg = fs::read_to_string(data.join("config.json"))
            .ok()
            .and_then(|s| serde_json::from_str(&s).ok())
            .unwrap_or_default();
        // version 0.1.0 kept Python, the compiler and the runs here, where the sandbox could not use them
        for old in ["runtimes", "runs"] {
            let o = data.join(old);
            if o.exists() {
                std::thread::spawn(move || {
                    let _ = fs::remove_dir_all(o);
                });
            }
        }
        Ok(App {
            data,
            docs,
            cfg: Mutex::new(cfg),
            hw: Mutex::new(None),
            eng: Mutex::new(None),
            starting: Mutex::new(()),
            setup: Mutex::new(()),
            last_use: Mutex::new(Instant::now()),
            pending: Mutex::new(None),
            sbx: Mutex::new(None),
        })
    }
    fn cfg(&self) -> Config {
        self.cfg.lock().unwrap().clone()
    }
    fn save(&self, f: impl FnOnce(&mut Config)) {
        let mut c = self.cfg.lock().unwrap();
        f(&mut c);
        if let Ok(s) = serde_json::to_string_pretty(&*c) {
            let _ = fs::write(self.data.join("config.json"), s);
        }
    }
    fn hardware(&self) -> sys::Hardware {
        let mut h = self.hw.lock().unwrap();
        if h.is_none() {
            *h = Some(sys::detect());
        }
        h.clone().unwrap()
    }
}

fn notice(app: &AppHandle, text: &str) {
    let _ = app.emit("engine://notice", text.to_string());
}

fn progress(app: &AppHandle, text: &str, done: u64, total: u64) {
    let pct = if total > 0 {
        (done as f64 * 100.0 / total as f64).min(100.0)
    } else {
        0.0
    };
    let t = if total > 0 {
        format!("{text}: {:.1} of {:.1} GB ({:.0}%)", done as f64 / 1e9, total as f64 / 1e9, pct)
    } else {
        format!("{text}: {:.1} GB", done as f64 / 1e9)
    };
    let _ = app.emit("download://progress", json!({"pct": pct, "text": t}));
}

/// A download with progress reported about once a second.
fn fetch(app: &AppHandle, label: &str, url: &str, dest: &Path, size: u64, sha: &str) -> Result<(), String> {
    let last = std::cell::Cell::new(Instant::now() - Duration::from_secs(5));
    net::download(url, dest, size, sha, &|d, t| {
        if last.get().elapsed() >= Duration::from_millis(800) || (t > 0 && d >= t) {
            last.set(Instant::now());
            progress(app, label, d, t);
        }
    })
}

fn find_file(dir: &Path, name: &str, depth: u32) -> Option<PathBuf> {
    let rd = fs::read_dir(dir).ok()?;
    let mut subs = Vec::new();
    for e in rd.flatten() {
        let p = e.path();
        if p.is_dir() {
            subs.push(p);
        } else if p.file_name().map(|n| n.to_string_lossy().eq_ignore_ascii_case(name)).unwrap_or(false) {
            return Some(p);
        }
    }
    if depth == 0 {
        return None;
    }
    subs.into_iter().find_map(|d| find_file(&d, name, depth - 1))
}

fn random_hex(n: usize) -> String {
    let mut s = String::new();
    while s.len() < n {
        let mut h = std::collections::hash_map::RandomState::new().build_hasher();
        h.write_usize(s.len());
        h.write_u32(std::process::id());
        s.push_str(&format!("{:016x}", h.finish()));
    }
    s.truncate(n);
    s
}

fn mtime(p: &Path) -> u64 {
    fs::metadata(p)
        .and_then(|m| m.modified())
        .ok()
        .and_then(|t| t.duration_since(UNIX_EPOCH).ok())
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

// ---------------------------------------------------------------------------------------------------------------
// the engine (llama.cpp)

fn kind_label(k: &str) -> &'static str {
    match k {
        "cuda" => "CUDA, on the NVIDIA card",
        "vulkan" => "Vulkan, on the graphics card",
        _ => "on the processor",
    }
}

fn pick_engine(kind: &str, assets: &[net::Asset]) -> Result<Vec<net::Asset>, String> {
    let named = |pred: &dyn Fn(&str) -> bool| assets.iter().find(|a| pred(&a.name)).cloned();
    match kind {
        "cuda" => {
            let mut builds: Vec<(u32, u32, String, net::Asset)> = assets
                .iter()
                .filter(|a| a.name.starts_with("llama-") && a.name.ends_with("-x64.zip") && a.name.contains("-bin-win-cuda-"))
                .filter_map(|a| {
                    let v = a.name.split("-cuda-").nth(1)?.strip_suffix("-x64.zip")?.to_string();
                    let (ma, mi) = v.split_once('.')?;
                    Some((ma.parse().ok()?, mi.parse().ok()?, v.clone(), a.clone()))
                })
                .collect();
            // CUDA 12 runs on every recent driver; the newest 12.x, else the newest build there is
            builds.sort_by(|a, b| ((a.0 == 12) as u8, a.0, a.1).cmp(&((b.0 == 12) as u8, b.0, b.1)).reverse());
            let (_, _, ver, a) = builds.into_iter().next().ok_or("this llama.cpp release has no Windows CUDA build")?;
            let rt = format!("cudart-llama-bin-win-cuda-{ver}-x64.zip");
            let r = named(&|n| n == rt).ok_or(format!("this llama.cpp release has no {rt}"))?;
            Ok(vec![a, r])
        }
        "vulkan" => Ok(vec![named(&|n| n.starts_with("llama-") && n.ends_with("-bin-win-vulkan-x64.zip"))
            .ok_or("this llama.cpp release has no Windows Vulkan build")?]),
        _ => {
            Ok(vec![named(&|n| n.starts_with("llama-") && n.ends_with("-bin-win-cpu-x64.zip"))
                .ok_or("this llama.cpp release has no Windows CPU build")?])
        }
    }
}

/// The folder with llama-server.exe, downloading the right build first if needed.
fn engine_install(app: &AppHandle, st: &App) -> Result<PathBuf, String> {
    let kind = st.hardware().engine_kind().to_string();
    if let Some(e) = st.cfg().engine {
        let bin = PathBuf::from(&e.bin);
        if e.kind == kind && bin.join("llama-server.exe").exists() {
            return Ok(bin);
        }
    }
    let _g = st.setup.lock().unwrap();
    notice(app, "Downloading the Blitzen engine (llama.cpp)…");
    let (tag, assets) = net::gh_release("ggml-org/llama.cpp", Some(LLAMA_TAG)).or_else(|_| net::gh_release("ggml-org/llama.cpp", None))?;
    let want = pick_engine(&kind, &assets)?;
    let dir = st.data.join("engine").join(format!("{tag}-{kind}"));
    let _ = fs::remove_dir_all(&dir);
    let mut bin: Option<PathBuf> = None;
    for a in &want {
        if a.sha256.is_empty() {
            return Err(format!("{} has no published SHA-256, so Blitzen will not use it", a.name));
        }
        let zip = st.data.join("downloads").join(&a.name);
        fetch(app, &format!("Engine ({})", a.name), &a.url, &zip, a.size, &a.sha256)?;
        let into = bin.clone().unwrap_or_else(|| dir.clone());
        net::unpack(&zip, &into)?;
        let _ = fs::remove_file(&zip);
        if bin.is_none() {
            let exe = find_file(&dir, "llama-server.exe", 3).ok_or("llama-server.exe was not in the engine download")?;
            bin = exe.parent().map(|p| p.to_path_buf());
        }
    }
    let bin = bin.ok_or("the engine download was empty")?;
    // remove other engine builds
    if let Ok(rd) = fs::read_dir(st.data.join("engine")) {
        for e in rd.flatten() {
            if e.path() != dir {
                let _ = fs::remove_dir_all(e.path());
            }
        }
    }
    st.save(|c| {
        c.engine = Some(EngineCfg {
            kind: kind.clone(),
            tag: tag.clone(),
            bin: bin.to_string_lossy().to_string(),
        })
    });
    Ok(bin)
}

fn running(st: &App) -> Option<Value> {
    let mut g = st.eng.lock().unwrap();
    let alive = match g.as_mut() {
        Some(e) => matches!(e.child.try_wait(), Ok(None)),
        None => return None,
    };
    if !alive {
        *g = None;
        return None;
    }
    g.as_ref().map(|e| json!({"port": e.port, "key": e.key}))
}

fn unload(st: &App) -> bool {
    st.eng.lock().unwrap().take().is_some()
}

fn job_for_engine(child: &Child) -> isize {
    use std::os::windows::io::AsRawHandle;
    use windows_sys::Win32::System::JobObjects::*;
    unsafe {
        let job = CreateJobObjectW(std::ptr::null(), std::ptr::null());
        if job.is_null() {
            return 0;
        }
        let mut lim: JOBOBJECT_EXTENDED_LIMIT_INFORMATION = std::mem::zeroed();
        lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(
            job,
            JobObjectExtendedLimitInformation,
            &lim as *const _ as *const std::ffi::c_void,
            std::mem::size_of::<JOBOBJECT_EXTENDED_LIMIT_INFORMATION>() as u32,
        );
        AssignProcessToJobObject(job, child.as_raw_handle() as _);
        job as isize
    }
}

fn engine_args(st: &App, model: &Path) -> Vec<String> {
    let hw = st.hardware();
    let gpu = st.cfg().engine.map(|e| e.kind != "cpu").unwrap_or(false);
    let threads = hw.cores.clamp(1, 16);
    vec![
        "-m".into(),
        model.to_string_lossy().to_string(),
        "-ngl".into(),
        if gpu { "99".into() } else { "0".into() },
        "-c".into(),
        CONTEXT.to_string(),
        "-fa".into(),
        "on".into(),
        "--cache-type-k".into(),
        "q8_0".into(),
        "--cache-type-v".into(),
        "q8_0".into(),
        "-t".into(),
        threads.to_string(),
        "--jinja".into(),
        "--alias".into(),
        "blitzen-plus".into(),
    ]
}

/// The model's first file, after checking that no file changed since it was verified.
fn model_ready(st: &App) -> Result<PathBuf, String> {
    let m = st
        .cfg()
        .model
        .ok_or("Blitzen's model is not downloaded yet: open Settings and choose Download the model.")?;
    let mut files = m.files.clone();
    for f in files.iter_mut() {
        let p = PathBuf::from(&f.path);
        let size = fs::metadata(&p).map(|m| m.len()).unwrap_or(0);
        if size != f.size {
            return Err(format!(
                "{} is missing or has changed: open Settings and download the model again.",
                p.display()
            ));
        }
        let t = mtime(&p);
        if t != f.mtime {
            if net::sha256_file(&p)? != f.sha256 {
                return Err(format!(
                    "{} has changed since it was checked, so Blitzen will not load it: download the model again in Settings.",
                    p.display()
                ));
            }
            f.mtime = t;
        }
    }
    st.save(|c| {
        if let Some(cm) = c.model.as_mut() {
            cm.files = files.clone();
        }
    });
    Ok(PathBuf::from(&files[0].path))
}

fn ensure(app: &AppHandle, st: &App) -> Result<Value, String> {
    *st.last_use.lock().unwrap() = Instant::now();
    if let Some(v) = running(st) {
        return Ok(v);
    }
    let _g = st.starting.lock().unwrap();
    if let Some(v) = running(st) {
        return Ok(v);
    }
    let model = model_ready(st)?;
    let bin = engine_install(app, st)?;
    notice(app, "Loading Blitzen's model…");
    let port = std::net::TcpListener::bind("127.0.0.1:0")
        .and_then(|l| l.local_addr())
        .map(|a| a.port())
        .map_err(|e| e.to_string())?;
    let key = random_hex(40);
    let log = st.data.join("logs").join("engine.log");
    let logf = fs::File::create(&log).map_err(|e| e.to_string())?;
    let mut args = engine_args(st, &model);
    args.extend(["--host".into(), "127.0.0.1".into(), "--port".into(), port.to_string()]);
    let child = sys::hidden(&bin.join("llama-server.exe").to_string_lossy())
        .args(&args)
        .env("LLAMA_API_KEY", &key)
        .current_dir(&bin)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::from(logf))
        .spawn()
        .map_err(|e| format!("could not start the engine: {e}"))?;
    let job = job_for_engine(&child);
    let mut eng = Engine {
        child,
        port,
        key: key.clone(),
        job,
    };
    let agent = ureq::AgentBuilder::new().timeout(Duration::from_secs(2)).build();
    let t0 = Instant::now();
    loop {
        if let Ok(Some(status)) = eng.child.try_wait() {
            let tail: String = fs::read_to_string(&log)
                .unwrap_or_default()
                .lines()
                .rev()
                .take(12)
                .collect::<Vec<_>>()
                .into_iter()
                .rev()
                .collect::<Vec<_>>()
                .join("\n");
            return Err(format!(
                "the engine stopped while loading ({status}). Last lines of {}:\n{tail}",
                log.display()
            ));
        }
        if let Ok(r) = agent.get(&format!("http://127.0.0.1:{port}/health")).call() {
            if r.status() == 200 {
                break;
            }
        }
        if t0.elapsed() > Duration::from_secs(180) {
            let _ = eng.child.kill();
            return Err(format!("the engine did not get ready within 3 minutes; see {}", log.display()));
        }
        std::thread::sleep(Duration::from_millis(300));
    }
    *st.eng.lock().unwrap() = Some(eng);
    *st.last_use.lock().unwrap() = Instant::now();
    notice(app, &format!("Blitzen is ready (loaded in {:.1} s).", t0.elapsed().as_secs_f64()));
    Ok(json!({"port": port, "key": key}))
}

/// Unloads after 10 minutes without a question, and when a full-screen game starts.
fn watcher(app: AppHandle, st: Arc<App>) {
    std::thread::spawn(move || loop {
        std::thread::sleep(Duration::from_secs(10));
        if st.eng.lock().unwrap().is_none() {
            continue;
        }
        if st.last_use.lock().unwrap().elapsed() >= IDLE_UNLOAD {
            if unload(&st) {
                notice(
                    &app,
                    "Blitzen unloaded its model after 10 minutes without a question; it loads again when you ask.",
                );
            }
        } else if sys::fullscreen_app() && !main_window_focused(&app) {
            if unload(&st) {
                notice(&app, "Blitzen unloaded its model to give the graphics card to a full-screen game.");
            }
        }
    });
}

fn main_window_focused(app: &AppHandle) -> bool {
    app.get_webview_window("main").and_then(|w| w.is_focused().ok()).unwrap_or(false)
}

// ---------------------------------------------------------------------------------------------------------------
// the signed model list

struct Part {
    name: String,
    size: u64,
    sha256: String,
    url: String,
}

fn verify_list(text: &[u8], sig: &[u8]) -> Result<(), String> {
    let sig: [u8; 64] = sig.try_into().map_err(|_| "the model list's signature is damaged".to_string())?;
    let sig = ed25519_dalek::Signature::from_bytes(&sig);
    for l in TRUSTED_KEYS.lines().map(str::trim).filter(|l| !l.is_empty() && !l.starts_with('#')) {
        let Ok(raw) = base64::engine::general_purpose::STANDARD.decode(l) else {
            continue;
        };
        let Ok(raw) = <[u8; 32]>::try_from(raw.as_slice()) else { continue };
        let Ok(k) = ed25519_dalek::VerifyingKey::from_bytes(&raw) else {
            continue;
        };
        if k.verify_strict(text, &sig).is_ok() {
            return Ok(());
        }
    }
    Err("the model list is not signed with the Rudolf OS release key, so Blitzen refused it".into())
}

fn parse_list(text: &str) -> Result<(String, Vec<Part>), String> {
    let mut kv = std::collections::HashMap::new();
    let mut parts = Vec::new();
    for l in text.lines() {
        let (k, v) = l.split_once(' ').unwrap_or((l, ""));
        if k == "part" {
            let f: Vec<&str> = v.split_whitespace().collect();
            if f.len() != 4 {
                return Err(format!("bad line in the model list: {l}"));
            }
            parts.push(Part {
                name: f[0].into(),
                size: f[1].parse().map_err(|_| "bad size in the model list")?,
                sha256: f[2].to_lowercase(),
                url: f[3].into(),
            });
        } else {
            kv.insert(k.to_string(), v.trim().to_string());
        }
    }
    if parts.is_empty() {
        let name = kv.get("file").cloned().ok_or("the model list names no file")?;
        let url = kv
            .get("url")
            .cloned()
            .unwrap_or_else(|| format!("{}/{}", MODEL_LIST.rsplit_once('/').unwrap().0, name));
        parts.push(Part {
            size: kv.get("size").and_then(|s| s.parse().ok()).unwrap_or(0),
            sha256: kv.get("sha256").cloned().unwrap_or_default(),
            name,
            url,
        });
    }
    for p in &parts {
        if p.name.is_empty() || !p.name.chars().all(|c| c.is_ascii_alphanumeric() || "._-".contains(c)) || !p.name.ends_with(".gguf") {
            return Err(format!("unexpected file name in the model list: {}", p.name));
        }
        if !p.url.starts_with("https://") || p.sha256.len() != 64 || p.size == 0 {
            return Err(format!("incomplete entry in the model list for {}", p.name));
        }
    }
    Ok((kv.get("model").cloned().unwrap_or_else(|| "Blitzen".into()), parts))
}

fn do_model_download(app: &AppHandle, st: &App) -> Result<Value, String> {
    let _g = st.setup.lock().unwrap();
    let text = net::get_bytes(MODEL_LIST, 1 << 20)
        .map_err(|e| format!("could not get the model list ({e}). Has the blitzen-win-plus release been published?"))?;
    let sig = net::get_bytes(&format!("{MODEL_LIST}.sig"), 4096)?;
    verify_list(&text, &sig)?;
    let (name, parts) = parse_list(&String::from_utf8_lossy(&text))?;
    let total: u64 = parts.iter().map(|p| p.size).sum();
    // already here and unchanged?
    if let Some(m) = st.cfg().model {
        if m.files.len() == parts.len()
            && m.files
                .iter()
                .zip(&parts)
                .all(|(f, p)| f.sha256 == p.sha256 && Path::new(&f.path).exists())
        {
            if model_ready(st).is_ok() {
                return Ok(json!({"model": name, "new": false}));
            }
        }
    }
    let mut files = Vec::new();
    // reuse the Stage 1 download (C:\Blitzen\models) when every file is there and matches
    let stage1 = Path::new(STAGE1_MODELS);
    if parts
        .iter()
        .all(|p| fs::metadata(stage1.join(&p.name)).map(|m| m.len() == p.size).unwrap_or(false))
    {
        let mut ok = true;
        for p in &parts {
            let _ = app.emit(
                "download://progress",
                json!({"pct": 0, "text": format!("Checking {} from C:\\Blitzen…", p.name)}),
            );
            if net::sha256_file(&stage1.join(&p.name))? != p.sha256 {
                ok = false;
                break;
            }
        }
        if ok {
            for p in &parts {
                let path = stage1.join(&p.name);
                files.push(FileCfg {
                    mtime: mtime(&path),
                    path: path.to_string_lossy().to_string(),
                    size: p.size,
                    sha256: p.sha256.clone(),
                });
            }
        }
    }
    if files.is_empty() {
        let mut done = 0u64;
        for p in &parts {
            let dest = st.data.join("models").join(&p.name);
            let ok = fs::metadata(&dest).map(|m| m.len() == p.size).unwrap_or(false) && net::sha256_file(&dest)? == p.sha256;
            if !ok {
                let base = done;
                let last = std::cell::Cell::new(Instant::now() - Duration::from_secs(5));
                net::download(&p.url, &dest, p.size, &p.sha256, &|d, _| {
                    if last.get().elapsed() >= Duration::from_millis(800) {
                        last.set(Instant::now());
                        progress(app, "Downloading the model", base + d, total);
                    }
                })?;
            }
            done += p.size;
            files.push(FileCfg {
                mtime: mtime(&dest),
                path: dest.to_string_lossy().to_string(),
                size: p.size,
                sha256: p.sha256.clone(),
            });
        }
        progress(app, "Model ready", total, total);
    }
    unload(st);
    st.save(|c| {
        c.model = Some(ModelCfg {
            name: name.clone(),
            files: files.clone(),
        })
    });
    // remove model files the list no longer names
    if let Ok(rd) = fs::read_dir(st.data.join("models")) {
        for e in rd.flatten() {
            let p = e.path().to_string_lossy().to_string();
            if !files.iter().any(|f| f.path == p) {
                let _ = fs::remove_file(e.path());
            }
        }
    }
    Ok(json!({"model": name, "new": true}))
}

// ---------------------------------------------------------------------------------------------------------------
// running code

/// The sandbox's own folder (%LOCALAPPDATA%\Packages\blitzen.sandbox\AC\Blitzen): Windows gives the AppContainer
/// read, write and run access there, at the low integrity level it runs at. Python, the compiler and every run's
/// temporary folder live here; nothing else on the PC is open to the sandbox.
fn sandbox_root(st: &App) -> Result<PathBuf, String> {
    let mut g = st.sbx.lock().unwrap();
    if let Some(p) = g.clone() {
        return Ok(p);
    }
    let sid = sandbox::container()?;
    let p = sandbox::folder(&sid)?.join("Blitzen");
    for d in ["runtimes", "runs"] {
        fs::create_dir_all(p.join(d)).map_err(|e| format!("could not create the sandbox folder {}: {e}", p.display()))?;
    }
    if let Ok(rd) = fs::read_dir(p.join("runs")) {
        for e in rd.flatten() {
            let _ = fs::remove_dir_all(e.path());
        }
    }
    *g = Some(p.clone());
    Ok(p)
}
fn runtimes(st: &App) -> Result<PathBuf, String> {
    Ok(sandbox_root(st)?.join("runtimes"))
}
fn python_exe(st: &App) -> Result<PathBuf, String> {
    Ok(runtimes(st)?.join("python").join("python.exe"))
}
fn clang_exe(st: &App) -> Result<Option<PathBuf>, String> {
    let rt = runtimes(st)?;
    Ok(fs::read_dir(rt).ok().and_then(|rd| {
        rd.flatten()
            .map(|e| e.path())
            .filter(|p| p.file_name().map(|n| n.to_string_lossy().starts_with("llvm-mingw")).unwrap_or(false))
            .map(|p| p.join("bin").join("clang++.exe"))
            .find(|p| p.exists())
    }))
}

/// clang-NN.exe, the real compiler next to llvm-mingw's clang++.exe launcher.
fn real_clang(bin: &Path) -> Option<PathBuf> {
    fs::read_dir(bin).ok()?.flatten().map(|e| e.path()).find(|p| {
        let n = p.file_name().map(|n| n.to_string_lossy().to_lowercase()).unwrap_or_default();
        n.strip_prefix("clang-")
            .and_then(|r| r.strip_suffix(".exe"))
            .map(|v| !v.is_empty() && v.chars().all(|c| c.is_ascii_digit()))
            .unwrap_or(false)
    })
}

fn sandbox_env(dir: &Path, path: &[&Path]) -> Vec<(String, String)> {
    let sysroot = std::env::var("SystemRoot").unwrap_or_else(|_| "C:\\Windows".into());
    let d = dir.to_string_lossy().to_string();
    let mut p: Vec<String> = path.iter().map(|x| x.to_string_lossy().to_string()).collect();
    p.push(format!("{sysroot}\\System32"));
    p.push(sysroot.clone());
    vec![
        ("SystemRoot".into(), sysroot.clone()),
        ("windir".into(), sysroot),
        ("PATH".into(), p.join(";")),
        ("PATHEXT".into(), ".COM;.EXE;.BAT;.CMD".into()),
        ("TEMP".into(), d.clone()),
        ("TMP".into(), d.clone()),
        ("USERPROFILE".into(), d.clone()),
        ("APPDATA".into(), d.clone()),
        ("LOCALAPPDATA".into(), d),
        ("NUMBER_OF_PROCESSORS".into(), num_cpus::get().to_string()),
        ("PYTHONIOENCODING".into(), "utf-8".into()),
        ("PYTHONUTF8".into(), "1".into()),
        ("PYTHONDONTWRITEBYTECODE".into(), "1".into()),
    ]
}

fn do_run_code(st: &App, lang: &str, code: &str) -> Result<Value, String> {
    let missing = |l: &str, what: &str| json!({"ok": false, "missing": l, "error": format!("{what} is not set up yet. Blitzen downloads it once and keeps it on this PC.")});
    let tool: PathBuf = match lang {
        "python" => {
            let p = python_exe(st)?;
            if !p.exists() {
                return Ok(missing("python", "Python"));
            }
            p
        }
        "cpp" => match clang_exe(st)? {
            Some(p) => p,
            None => return Ok(missing("cpp", "The C++ compiler")),
        },
        _ => return Ok(json!({"ok": false, "error": format!("Blitzen does not run {lang} in the app")})),
    };
    let sid = sandbox::container()?;
    let dir = sandbox_root(st)?.join("runs").join(random_hex(12));
    fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
    let result = (|| -> Result<Value, String> {
        let bin = tool.parent().unwrap();
        let env = sandbox_env(&dir, &[bin]);
        if lang == "python" {
            fs::write(dir.join("main.py"), code).map_err(|e| e.to_string())?;
            let o = sandbox::run(&sid, &tool, &["-I".into(), "-B".into(), "main.py".into()], &dir, &env, 10, 1 << 30, true)?;
            return Ok(json!({"ok": true, "code": o.code, "timeout": o.timeout, "out": o.out}));
        }
        fs::write(dir.join("main.cpp"), code).map_err(|e| e.to_string())?;
        // llvm-mingw's clang++.exe is a launcher that cannot find clang-NN.exe when the folder path contains a dot (as
        // ...\Packages\blitzen.sandbox\... does), so start the real compiler with the launcher's settings directly.
        let compiler = real_clang(tool.parent().unwrap()).unwrap_or_else(|| tool.clone());
        // the compiler's libraries: if one is gone (removed after unpacking), offer to set the compiler up again
        let bin_dir = tool.parent().unwrap();
        for need in ["libLLVM-", "libclang-cpp"] {
            let found = fs::read_dir(bin_dir)
                .map(|rd| {
                    rd.flatten().any(|e| {
                        let n = e.file_name().to_string_lossy().to_string();
                        n.starts_with(need) && n.to_lowercase().ends_with(".dll")
                    })
                })
                .unwrap_or(false);
            if !found {
                return Ok(
                    json!({"ok": false, "missing": "cpp", "error": format!("Part of the C++ compiler is missing ({need}….dll in {}). Windows Security may have removed it: check Protection history, then set the compiler up again.", bin_dir.display())}),
                );
            }
        }
        let args: Vec<String> = [
            "--driver-mode=g++",
            "-target",
            "x86_64-w64-mingw32",
            "-rtlib=compiler-rt",
            "-unwindlib=libunwind",
            "-stdlib=libc++",
            "-fuse-ld=lld",
            "-std=c++17",
            "-O1",
            "-static",
            "-o",
            "main.exe",
            "main.cpp",
        ]
        .iter()
        .map(|s| s.to_string())
        .collect();
        // compile outside the AppContainer (with the same time, memory and process limits); only the program runs inside
        let c = sandbox::run(&sid, &compiler, &args, &dir, &env, 60, 2 << 30, false)?;
        if c.code != 0 || !dir.join("main.exe").exists() {
            // the compiler itself failing (not the code): say so, so the page does not ask the model to "fix" good code
            let tool_root = tool
                .parent()
                .and_then(|p| p.parent())
                .map(|p| p.to_string_lossy().to_string())
                .unwrap_or_default();
            if c.out.contains("Permission denied") || (!tool_root.is_empty() && c.out.contains(&tool_root)) {
                return Ok(
                    json!({"ok": false, "error": format!("Blitzen could not run the C++ compiler, so this code was not checked:\n{}", c.out.trim())}),
                );
            }
            return Ok(
                json!({"ok": true, "code": if c.code == 0 { 1 } else { c.code }, "timeout": c.timeout, "out": format!("compile error:\n{}", c.out)}),
            );
        }
        let o = sandbox::run(&sid, &dir.join("main.exe"), &[], &dir, &env, 10, 1 << 30, true)?;
        Ok(json!({"ok": true, "code": o.code, "timeout": o.timeout, "out": o.out}))
    })();
    let _ = fs::remove_dir_all(&dir);
    result
}

fn do_runner_setup(app: &AppHandle, st: &App, lang: &str) -> Result<(), String> {
    let _g = st.setup.lock().unwrap();
    let rt = runtimes(st)?;
    let dl = st.data.join("downloads");
    let (into_name, archive, url, size, sha) = match lang {
        "python" => {
            // python-build-standalone: a full, relocatable Python; its SHA256SUMS lists every file
            let repo = "astral-sh/python-build-standalone";
            let j: Value =
                serde_json::from_str(&net::get_text(&format!("https://api.github.com/repos/{repo}/releases/latest"))?).map_err(|e| e.to_string())?;
            let tag = j["tag_name"].as_str().ok_or("could not find the latest Python build")?.to_string();
            let base = format!("https://github.com/{repo}/releases/download/{tag}");
            let sums = String::from_utf8_lossy(&net::get_bytes(&format!("{base}/SHA256SUMS"), 8 << 20)?).to_string();
            let mut pick = None;
            for minor in ["3.13.", "3.12."] {
                pick = sums
                    .lines()
                    .filter_map(|l| l.split_once(char::is_whitespace))
                    .map(|(h, n)| (h.trim().to_string(), n.trim().trim_start_matches('*').to_string()))
                    .find(|(_, n)| n.starts_with(&format!("cpython-{minor}")) && n.ends_with("-x86_64-pc-windows-msvc-install_only_stripped.tar.gz"));
                if pick.is_some() {
                    break;
                }
            }
            let (h, n) = pick.ok_or("no Windows Python build in the latest python-build-standalone release")?;
            ("python".to_string(), n.clone(), format!("{base}/{n}"), 0u64, h)
        }
        "cpp" => {
            let (_, assets) = net::gh_release("mstorsjo/llvm-mingw", None)?;
            let a = assets
                .into_iter()
                .find(|a| a.name.starts_with("llvm-mingw-") && a.name.ends_with("-ucrt-x86_64.zip"))
                .ok_or("no Windows build in the latest llvm-mingw release")?;
            (
                a.name.trim_end_matches(".zip").to_string(),
                a.name.clone(),
                a.url.clone(),
                a.size,
                a.sha256.clone(),
            )
        }
        _ => return Err(format!("nothing to set up for {lang}")),
    };
    let file = dl.join(&archive);
    fetch(
        app,
        if lang == "python" {
            "Downloading Python"
        } else {
            "Downloading the C++ compiler"
        },
        &url,
        &file,
        size,
        &sha,
    )?;
    let _ = fs::remove_dir_all(rt.join(&into_name));
    let _ = app.emit("download://progress", json!({"pct": 100, "text": "Unpacking…"}));
    net::unpack(&file, &rt)?;
    let _ = fs::remove_file(&file);
    let ready = if lang == "python" {
        python_exe(st)?.exists()
    } else {
        clang_exe(st)?.is_some()
    };
    if !ready {
        return Err("the download did not contain what Blitzen expected".into());
    }
    let _ = app.emit("download://progress", json!({"pct": 100, "text": "Ready."}));
    Ok(())
}

fn do_benchmark(app: &AppHandle, st: &App) -> Result<f64, String> {
    let model = model_ready(st)?;
    let bin = engine_install(app, st)?;
    unload(st);
    let gpu = st.cfg().engine.map(|e| e.kind != "cpu").unwrap_or(false);
    let threads = st.hardware().cores.clamp(1, 16).to_string();
    let m = model.to_string_lossy().to_string();
    let base: Vec<String> = [
        "-m",
        m.as_str(),
        "-ngl",
        if gpu { "99" } else { "0" },
        "-t",
        threads.as_str(),
        "-p",
        "512",
        "-n",
        "128",
        "-r",
        "3",
        "-o",
        "json",
    ]
    .iter()
    .map(|s| s.to_string())
    .collect();
    let tries: [&[&str]; 3] = [&["-fa", "on", "-ctk", "q8_0", "-ctv", "q8_0"], &["-fa", "1"], &[]];
    let mut last = String::new();
    for extra in tries {
        let out = sys::hidden(&bin.join("llama-bench.exe").to_string_lossy())
            .args(&base)
            .args(extra)
            .current_dir(&bin)
            .output()
            .map_err(|e| format!("llama-bench: {e}"))?;
        if !out.status.success() {
            last = String::from_utf8_lossy(&out.stderr).lines().rev().take(5).collect::<Vec<_>>().join(" | ");
            continue;
        }
        let rows: Value = serde_json::from_slice(&out.stdout).map_err(|e| format!("llama-bench output: {e}"))?;
        let tg = rows
            .as_array()
            .into_iter()
            .flatten()
            .find(|r| r["n_gen"].as_u64().unwrap_or(0) > 0)
            .and_then(|r| r["avg_ts"].as_f64())
            .unwrap_or(0.0);
        st.save(|c| c.bench_tps = tg);
        return Ok(tg);
    }
    Err(format!("the speed test failed: {last}"))
}

fn do_status(st: &App) -> Value {
    let hw = st.hardware();
    let c = st.cfg();
    let kind = c.engine.as_ref().map(|e| e.kind.clone()).unwrap_or_else(|| hw.engine_kind().to_string());
    let tag = c.engine.as_ref().map(|e| e.tag.clone()).unwrap_or_else(|| LLAMA_TAG.to_string());
    let ready = c
        .engine
        .as_ref()
        .map(|e| Path::new(&e.bin).join("llama-server.exe").exists())
        .unwrap_or(false);
    let size: u64 = c.model.as_ref().map(|m| m.files.iter().map(|f| f.size).sum()).unwrap_or(0);
    json!({
        "cpu": hw.cpu, "cores": hw.cores, "avx2": hw.avx2, "ram": hw.ram,
        "gpus": hw.gpus.iter().map(|g| json!({"name": g.name, "vram": g.vram})).collect::<Vec<_>>(),
        "engine": format!("llama.cpp {tag}, {}", kind_label(&kind)),
        "engine_ready": ready,
        "model": c.model.as_ref().map(|m| m.name.clone()),
        "model_size": size,
        "loaded": running(st).is_some(),
        "bench_tps": c.bench_tps,
    })
}

// ---------------------------------------------------------------------------------------------------------------
// chats, files, Explorer

fn chat_file(st: &App, name: &str) -> PathBuf {
    let safe: String = name
        .chars()
        .map(|c| if "\\/:*?\"<>|".contains(c) || c.is_control() { '_' } else { c })
        .collect();
    st.docs.join(format!("{}.txt", safe.trim().trim_end_matches('.')))
}

/// "--ask FILE" (from Explorer): the file as a question for the page.
fn ask_from_args(argv: &[String]) -> Option<Value> {
    let i = argv.iter().position(|a| a == "--ask")?;
    let path = PathBuf::from(argv.get(i + 1)?);
    let name = path.file_name()?.to_string_lossy().to_string();
    let bytes = fs::read(&path).ok()?;
    let ext = path.extension().map(|e| e.to_string_lossy().to_lowercase()).unwrap_or_default();
    let (lang, pack) = match ext.as_str() {
        "lua" | "blk" => ("lua", "blocklua"),
        "js" | "mjs" | "cjs" | "ts" | "html" | "htm" => ("js", "js"),
        "c" | "cc" | "cpp" | "cxx" | "h" | "hpp" => ("cpp", "cpp"),
        "py" | "pyw" => ("python", "python"),
        _ => ("", ""),
    };
    let text = if bytes.iter().take(8192).any(|&b| b == 0) {
        format!("({name} is a binary file, {} bytes; Blitzen can only read text files.)", bytes.len())
    } else if bytes.len() > 200_000 {
        format!(
            "{}\n… (the file continues; only the first 200 KB are shown)",
            String::from_utf8_lossy(&bytes[..200_000])
        )
    } else {
        String::from_utf8_lossy(&bytes).to_string()
    };
    Some(json!({"name": name, "text": text, "lang": lang, "pack": pack}))
}

fn register_explorer() {
    use winreg::enums::HKEY_CURRENT_USER;
    let Ok(exe) = std::env::current_exe() else { return };
    let exe = exe.to_string_lossy().to_string();
    let hk = winreg::RegKey::predef(HKEY_CURRENT_USER);
    if let Ok((k, _)) = hk.create_subkey("Software\\Classes\\*\\shell\\BlitzenAsk") {
        let _ = k.set_value("", &"Ask Blitzen about this file");
        let _ = k.set_value("Icon", &format!("\"{exe}\",0"));
        if let Ok((c, _)) = k.create_subkey("command") {
            let _ = c.set_value("", &format!("\"{exe}\" --ask \"%1\""));
        }
    }
}

fn show_main(app: &AppHandle) {
    if let Some(w) = app.get_webview_window("main") {
        let _ = w.unminimize();
        let _ = w.show();
        let _ = w.set_focus();
    }
}

fn toggle_main(app: &AppHandle) {
    if let Some(w) = app.get_webview_window("main") {
        if w.is_visible().unwrap_or(false) && w.is_focused().unwrap_or(false) {
            let _ = w.hide();
        } else {
            show_main(app);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// commands for the page

async fn blocking<T: Send + 'static>(f: impl FnOnce() -> Result<T, String> + Send + 'static) -> Result<T, String> {
    tauri::async_runtime::spawn_blocking(f).await.map_err(|e| e.to_string())?
}

type St<'a> = State<'a, Arc<App>>;

#[tauri::command]
async fn engine_ensure(app: AppHandle, st: St<'_>) -> Result<Value, String> {
    let s = st.inner().clone();
    blocking(move || ensure(&app, &s)).await
}

#[tauri::command]
async fn engine_unload(st: St<'_>) -> Result<bool, String> {
    Ok(unload(st.inner()))
}

#[tauri::command]
async fn status(st: St<'_>) -> Result<Value, String> {
    let s = st.inner().clone();
    blocking(move || Ok(do_status(&s))).await
}

#[tauri::command]
async fn hardware_recheck(st: St<'_>) -> Result<Value, String> {
    let s = st.inner().clone();
    blocking(move || {
        *s.hw.lock().unwrap() = Some(sys::detect());
        Ok(do_status(&s))
    })
    .await
}

#[tauri::command]
async fn model_download(app: AppHandle, st: St<'_>) -> Result<Value, String> {
    let s = st.inner().clone();
    blocking(move || do_model_download(&app, &s)).await
}

#[tauri::command]
async fn benchmark(app: AppHandle, st: St<'_>) -> Result<f64, String> {
    let s = st.inner().clone();
    blocking(move || do_benchmark(&app, &s)).await
}

#[tauri::command]
async fn run_code(st: St<'_>, lang: String, code: String) -> Result<Value, String> {
    let s = st.inner().clone();
    blocking(move || do_run_code(&s, &lang, &code)).await
}

#[tauri::command]
async fn runner_setup(app: AppHandle, st: St<'_>, lang: String) -> Result<(), String> {
    let s = st.inner().clone();
    blocking(move || do_runner_setup(&app, &s, &lang)).await
}

#[tauri::command]
async fn chat_save(st: St<'_>, name: String, text: String) -> Result<(), String> {
    fs::create_dir_all(&st.docs).map_err(|e| e.to_string())?;
    fs::write(chat_file(st.inner(), &name), text).map_err(|e| e.to_string())
}

#[tauri::command]
async fn chat_list(st: St<'_>) -> Result<Vec<String>, String> {
    let mut v: Vec<String> = fs::read_dir(&st.docs)
        .map(|rd| {
            rd.flatten()
                .filter_map(|e| e.file_name().to_string_lossy().strip_suffix(".txt").map(|s| s.to_string()))
                .collect()
        })
        .unwrap_or_default();
    v.sort();
    v.reverse();
    Ok(v)
}

#[tauri::command]
async fn chat_load(st: St<'_>, name: String) -> Result<String, String> {
    fs::read_to_string(chat_file(st.inner(), &name)).map_err(|e| e.to_string())
}

#[tauri::command]
async fn save_file(app: AppHandle, ext: String, text: String) -> Result<Option<String>, String> {
    use tauri_plugin_dialog::DialogExt;
    blocking(move || {
        let ext: String = ext.chars().filter(|c| c.is_ascii_alphanumeric()).take(8).collect();
        let picked = app
            .dialog()
            .file()
            .set_file_name(format!("blitzen.{ext}"))
            .add_filter("Code", &[ext.as_str()])
            .blocking_save_file();
        let Some(p) = picked else { return Ok(None) };
        let p = p.into_path().map_err(|e| e.to_string())?;
        fs::write(&p, text).map_err(|e| e.to_string())?;
        Ok(Some(p.to_string_lossy().to_string()))
    })
    .await
}

#[tauri::command]
async fn take_pending_ask(st: St<'_>) -> Result<Option<Value>, String> {
    Ok(st.pending.lock().unwrap().take())
}

fn tray(app: &tauri::App) -> tauri::Result<()> {
    use tauri::menu::{Menu, MenuItem};
    use tauri::tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent};
    let open = MenuItem::with_id(app, "open", "Open Blitzen (Win+Shift+B)", true, None::<&str>)?;
    let unl = MenuItem::with_id(app, "unload", "Unload the model (free the graphics card)", true, None::<&str>)?;
    let quit = MenuItem::with_id(app, "quit", "Quit Blitzen", true, None::<&str>)?;
    let menu = Menu::with_items(app, &[&open, &unl, &quit])?;
    let mut b = TrayIconBuilder::with_id("blitzen")
        .tooltip("Blitzen")
        .menu(&menu)
        .show_menu_on_left_click(false);
    if let Some(icon) = app.default_window_icon() {
        b = b.icon(icon.clone());
    }
    b.on_menu_event(|app, ev| match ev.id().as_ref() {
        "open" => show_main(app),
        "unload" => {
            let st = app.state::<Arc<App>>();
            if unload(st.inner()) {
                notice(app, "Blitzen unloaded its model; it loads again when you ask.");
            }
        }
        "quit" => {
            let st = app.state::<Arc<App>>();
            unload(st.inner());
            app.exit(0);
        }
        _ => {}
    })
    .on_tray_icon_event(|tray, ev| {
        if let TrayIconEvent::Click {
            button: MouseButton::Left,
            button_state: MouseButtonState::Up,
            ..
        } = ev
        {
            show_main(tray.app_handle());
        }
    })
    .build(app)?;
    Ok(())
}

fn main() {
    // no Windows error boxes (such as "a DLL was not found") from Blitzen or the programs it checks: errors are shown in the page
    unsafe {
        windows_sys::Win32::System::Diagnostics::Debug::SetErrorMode(
            windows_sys::Win32::System::Diagnostics::Debug::SEM_FAILCRITICALERRORS
                | windows_sys::Win32::System::Diagnostics::Debug::SEM_NOOPENFILEERRORBOX,
        );
    }
    let argv: Vec<String> = std::env::args().collect();
    let pending = ask_from_args(&argv);
    tauri::Builder::default()
        .plugin(tauri_plugin_single_instance::init(|app, argv, _cwd| {
            show_main(app);
            if let Some(f) = ask_from_args(&argv) {
                let _ = app.emit("ask-file", f);
            }
        }))
        .plugin(tauri_plugin_dialog::init())
        .plugin(
            tauri_plugin_global_shortcut::Builder::new()
                .with_handler(|app, _shortcut, ev| {
                    if ev.state() == tauri_plugin_global_shortcut::ShortcutState::Pressed {
                        toggle_main(app);
                    }
                })
                .build(),
        )
        .setup(move |app| {
            let st = Arc::new(App::new(app.handle())?);
            *st.pending.lock().unwrap() = pending;
            app.manage(st.clone());
            tray(app)?;
            {
                use tauri_plugin_global_shortcut::{Code, GlobalShortcutExt, Modifiers, Shortcut};
                let sc = Shortcut::new(Some(Modifiers::SUPER | Modifiers::SHIFT), Code::KeyB);
                if let Err(e) = app.global_shortcut().register(sc) {
                    eprintln!("Win+Shift+B is taken by another program: {e}");
                }
            }
            register_explorer();
            watcher(app.handle().clone(), st);
            Ok(())
        })
        .on_window_event(|w, ev| {
            if let WindowEvent::CloseRequested { api, .. } = ev {
                // closing the window keeps Blitzen in the tray; Quit is in the tray menu
                api.prevent_close();
                let _ = w.hide();
            }
        })
        .invoke_handler(tauri::generate_handler![
            engine_ensure,
            engine_unload,
            status,
            hardware_recheck,
            model_download,
            benchmark,
            run_code,
            runner_setup,
            chat_save,
            chat_list,
            chat_load,
            save_file,
            take_pending_ask
        ])
        .run(tauri::generate_context!())
        .expect("Blitzen could not start");
}
