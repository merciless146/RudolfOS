//! Downloads: GitHub release look-ups, resumable downloads checked against a SHA-256, and unpacking.
use serde_json::Value;
use sha2::{Digest, Sha256};
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Write};
use std::path::Path;
use std::time::Duration;

const UA: &str = concat!("Blitzen-Windows/", env!("CARGO_PKG_VERSION"));

fn agent() -> ureq::Agent {
    ureq::AgentBuilder::new()
        .timeout_connect(Duration::from_secs(20))
        .timeout_read(Duration::from_secs(90))
        .user_agent(UA)
        .build()
}

fn err(e: ureq::Error) -> String {
    match e {
        ureq::Error::Status(c, r) => format!("{} answered {}", r.get_url(), c),
        ureq::Error::Transport(t) => format!("no connection ({})", t),
    }
}

pub fn get_text(url: &str) -> Result<String, String> {
    let r = agent().get(url).call().map_err(err)?;
    let mut s = String::new();
    r.into_reader().take(64 << 20).read_to_string(&mut s).map_err(|e| e.to_string())?;
    Ok(s)
}

pub fn get_bytes(url: &str, max: u64) -> Result<Vec<u8>, String> {
    let r = agent().get(url).call().map_err(err)?;
    let mut v = Vec::new();
    r.into_reader().take(max).read_to_end(&mut v).map_err(|e| e.to_string())?;
    Ok(v)
}

#[derive(Clone, Debug)]
pub struct Asset {
    pub name: String,
    pub url: String,
    pub size: u64,
    pub sha256: String,
}

/// A GitHub release (a tag, or the latest one when `tag` is None): its tag and files. Each file's SHA-256 comes from
/// GitHub's own digest, or from a SHA256SUMS file in the same release.
pub fn gh_release(repo: &str, tag: Option<&str>) -> Result<(String, Vec<Asset>), String> {
    let url = match tag {
        Some(t) => format!("https://api.github.com/repos/{repo}/releases/tags/{t}"),
        None => format!("https://api.github.com/repos/{repo}/releases/latest"),
    };
    let j: Value = serde_json::from_str(&get_text(&url)?).map_err(|e| e.to_string())?;
    let tag = j["tag_name"].as_str().unwrap_or("").to_string();
    let mut out = Vec::new();
    for a in j["assets"].as_array().cloned().unwrap_or_default() {
        let digest = a["digest"].as_str().unwrap_or("");
        out.push(Asset {
            name: a["name"].as_str().unwrap_or("").to_string(),
            url: a["browser_download_url"].as_str().unwrap_or("").to_string(),
            size: a["size"].as_u64().unwrap_or(0),
            sha256: digest.strip_prefix("sha256:").unwrap_or("").to_lowercase(),
        });
    }
    if out.iter().any(|a| a.sha256.is_empty()) {
        if let Some(sums) = out.iter().find(|a| a.name == "SHA256SUMS").cloned() {
            let text = String::from_utf8_lossy(&get_bytes(&sums.url, 4 << 20)?).to_string();
            for a in out.iter_mut().filter(|a| a.sha256.is_empty()) {
                for l in text.lines() {
                    let mut it = l.split_whitespace();
                    if let (Some(h), Some(n)) = (it.next(), it.next()) {
                        if n.trim_start_matches('*') == a.name && h.len() == 64 {
                            a.sha256 = h.to_lowercase();
                        }
                    }
                }
            }
        }
    }
    Ok((tag, out))
}

pub fn sha256_file(p: &Path) -> Result<String, String> {
    let mut f = File::open(p).map_err(|e| format!("{}: {}", p.display(), e))?;
    let mut h = Sha256::new();
    let mut buf = vec![0u8; 1 << 20];
    loop {
        let n = f.read(&mut buf).map_err(|e| e.to_string())?;
        if n == 0 {
            break;
        }
        h.update(&buf[..n]);
    }
    Ok(format!("{:x}", h.finalize()))
}

/// Downloads `url` to `dest`, resuming a partial `dest.part`, and refuses it unless its size and SHA-256 match.
/// `progress(done, total)` is called as it goes.
pub fn download(url: &str, dest: &Path, size: u64, sha256: &str, progress: &dyn Fn(u64, u64)) -> Result<(), String> {
    if sha256.len() != 64 {
        return Err(format!("{}: no SHA-256 to check it against", url));
    }
    let name = dest.file_name().map(|n| n.to_string_lossy().to_string()).unwrap_or_default();
    let part = dest.with_file_name(format!("{name}.part"));
    if let Some(d) = dest.parent() {
        fs::create_dir_all(d).map_err(|e| e.to_string())?;
    }
    for attempt in 0..3 {
        let have = fs::metadata(&part).map(|m| m.len()).unwrap_or(0);
        if size > 0 && have > size {
            let _ = fs::remove_file(&part);
            continue;
        }
        if size == 0 || have < size {
            let mut req = agent().get(url);
            if have > 0 {
                req = req.set("Range", &format!("bytes={have}-"));
            }
            let resp = match req.call() {
                Ok(r) => r,
                Err(e) => {
                    if attempt < 2 {
                        std::thread::sleep(Duration::from_secs(3));
                        continue;
                    }
                    return Err(err(e));
                }
            };
            let resume = resp.status() == 206;
            let total = if size > 0 {
                size
            } else {
                resp.header("Content-Length").and_then(|v| v.parse().ok()).unwrap_or(0)
            };
            let mut f = if resume {
                OpenOptions::new().append(true).open(&part)
            } else {
                File::create(&part)
            }
            .map_err(|e| e.to_string())?;
            let mut done = if resume { have } else { 0 };
            let mut rd = resp.into_reader();
            let mut buf = vec![0u8; 1 << 20];
            let mut failed = None;
            loop {
                match rd.read(&mut buf) {
                    Ok(0) => break,
                    Ok(n) => {
                        if let Err(e) = f.write_all(&buf[..n]) {
                            return Err(format!("could not write {}: {}", part.display(), e));
                        }
                        done += n as u64;
                        progress(done, total);
                    }
                    Err(e) => {
                        failed = Some(e.to_string());
                        break;
                    }
                }
            }
            drop(f);
            if let Some(e) = failed {
                if attempt < 2 {
                    continue;
                }
                return Err(format!("the download of {name} stopped ({e}); try again to resume it"));
            }
        }
        let got = sha256_file(&part)?;
        if got != sha256.to_lowercase() {
            let _ = fs::remove_file(&part);
            if attempt < 2 {
                continue;
            }
            return Err(format!(
                "{name}: its SHA-256 {got} does not match the published {sha256}, so it was deleted"
            ));
        }
        let _ = fs::remove_file(dest);
        fs::rename(&part, dest).map_err(|e| e.to_string())?;
        return Ok(());
    }
    Err(format!("could not download {name}"))
}

/// Unpacks a .zip (checking afterwards that every file is there with its size) or a .tar.gz.
pub fn unpack(archive: &Path, into: &Path) -> Result<(), String> {
    fs::create_dir_all(into).map_err(|e| e.to_string())?;
    let name = archive.display().to_string();
    let f = File::open(archive).map_err(|e| format!("{name}: {e}"))?;
    if name.to_lowercase().ends_with(".zip") {
        let mut zip = zip::ZipArchive::new(f).map_err(|e| format!("{name}: {e}"))?;
        let mut files = Vec::new();
        for i in 0..zip.len() {
            let mut e = zip.by_index(i).map_err(|e| format!("{name}: {e}"))?;
            let Some(rel) = e.enclosed_name() else { continue };
            let out = into.join(rel);
            if e.is_dir() {
                fs::create_dir_all(&out).map_err(|e| format!("{}: {e}", out.display()))?;
                continue;
            }
            if let Some(d) = out.parent() {
                fs::create_dir_all(d).map_err(|e| format!("{}: {e}", d.display()))?;
            }
            let mut w = File::create(&out).map_err(|e| format!("could not write {}: {e}", out.display()))?;
            std::io::copy(&mut e, &mut w).map_err(|e| format!("could not write {}: {e}", out.display()))?;
            files.push((out, e.size()));
        }
        for (p, size) in &files {
            let got = fs::metadata(p).map(|m| m.len()).ok();
            if got != Some(*size) {
                return Err(format!(
                    "{} is {} after unpacking (it should be {size} bytes). Windows Security may have removed it: see Protection history.",
                    p.display(),
                    got.map(|g| format!("{g} bytes")).unwrap_or_else(|| "missing".into())
                ));
            }
        }
    } else {
        tar::Archive::new(flate2::read::GzDecoder::new(f))
            .unpack(into)
            .map_err(|e| format!("could not unpack {name}: {e}"))?;
    }
    Ok(())
}
