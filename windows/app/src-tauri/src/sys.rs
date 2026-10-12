//! This PC: processor, memory, graphics cards, full-screen games; and starting programs without a console window.
use serde::Serialize;
use std::os::windows::process::CommandExt;
use std::process::Command;

const CREATE_NO_WINDOW: u32 = 0x0800_0000;

/// A command that runs without flashing a console window.
pub fn hidden(program: &str) -> Command {
    let mut c = Command::new(program);
    c.creation_flags(CREATE_NO_WINDOW);
    c
}

#[derive(Clone, Serialize, Default)]
pub struct Gpu {
    pub name: String,
    pub vram: u64,
    pub nvidia: bool,
}

#[derive(Clone, Serialize, Default)]
pub struct Hardware {
    pub cpu: String,
    pub cores: usize,
    pub threads: usize,
    pub avx2: bool,
    pub ram: u64,
    pub gpus: Vec<Gpu>,
}

impl Hardware {
    /// cuda, vulkan or cpu: the llama.cpp build that suits this PC.
    pub fn engine_kind(&self) -> &'static str {
        if self.gpus.iter().any(|g| g.nvidia && g.vram >= 6 << 30) {
            "cuda"
        } else if self.gpus.iter().any(|g| !g.nvidia && g.vram >= 4 << 30) {
            "vulkan"
        } else {
            "cpu"
        }
    }
}

fn cpu_name() -> String {
    use winreg::enums::HKEY_LOCAL_MACHINE;
    winreg::RegKey::predef(HKEY_LOCAL_MACHINE)
        .open_subkey("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0")
        .and_then(|k| k.get_value::<String, _>("ProcessorNameString"))
        .map(|s| s.trim().to_string())
        .unwrap_or_else(|_| "unknown processor".into())
}

fn ram() -> u64 {
    use windows_sys::Win32::System::SystemInformation::{GlobalMemoryStatusEx, MEMORYSTATUSEX};
    unsafe {
        let mut m: MEMORYSTATUSEX = std::mem::zeroed();
        m.dwLength = std::mem::size_of::<MEMORYSTATUSEX>() as u32;
        if GlobalMemoryStatusEx(&mut m) != 0 {
            m.ullTotalPhys
        } else {
            0
        }
    }
}

fn gpus() -> Vec<Gpu> {
    let mut v = Vec::new();
    // NVIDIA: nvidia-smi gives the real memory size
    if let Ok(o) = hidden("nvidia-smi")
        .args(["--query-gpu=name,memory.total", "--format=csv,noheader,nounits"])
        .output()
    {
        if o.status.success() {
            for l in String::from_utf8_lossy(&o.stdout).lines() {
                let f: Vec<&str> = l.split(',').map(|s| s.trim()).collect();
                if f.len() >= 2 {
                    v.push(Gpu {
                        name: f[0].to_string(),
                        vram: f[1].parse::<u64>().unwrap_or(0) << 20,
                        nvidia: true,
                    });
                }
            }
        }
    }
    // other cards: the display adapters Windows lists (their memory from the registry, as Task Manager shows it)
    let ps = "Get-CimInstance Win32_VideoController | ForEach-Object { $n = $_.Name; $m = [uint64]$_.AdapterRAM; try { $k = @(Get-ChildItem 'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}' -ErrorAction SilentlyContinue | Where-Object { (Get-ItemProperty $_.PSPath -Name DriverDesc -ErrorAction SilentlyContinue).DriverDesc -eq $n }); if ($k.Count) { $q = (Get-ItemProperty $k[0].PSPath -Name 'HardwareInformation.qwMemorySize' -ErrorAction Stop).'HardwareInformation.qwMemorySize'; if ($q) { $m = [uint64]$q } } } catch {}; $n + '|' + $m }";
    if let Ok(o) = hidden("powershell").args(["-NoProfile", "-NonInteractive", "-Command", ps]).output() {
        for l in String::from_utf8_lossy(&o.stdout).lines() {
            if let Some((name, mem)) = l.trim().rsplit_once('|') {
                let low = name.to_lowercase();
                if low.contains("nvidia") || low.contains("basic display") || low.contains("remote") || name.is_empty() {
                    continue;
                }
                v.push(Gpu {
                    name: name.to_string(),
                    vram: mem.parse().unwrap_or(0),
                    nvidia: false,
                });
            }
        }
    }
    v
}

pub fn detect() -> Hardware {
    Hardware {
        cpu: cpu_name(),
        cores: num_cpus::get_physical(),
        threads: num_cpus::get(),
        avx2: std::arch::is_x86_feature_detected!("avx2"),
        ram: ram(),
        gpus: gpus(),
    }
}

/// True while a full-screen game (or a presentation) is running, so Blitzen gives the graphics card back.
pub fn fullscreen_app() -> bool {
    use windows_sys::Win32::UI::Shell::{SHQueryUserNotificationState, QUNS_BUSY, QUNS_RUNNING_D3D_FULL_SCREEN};
    unsafe {
        let mut s = 0;
        SHQueryUserNotificationState(&mut s) >= 0 && (s == QUNS_RUNNING_D3D_FULL_SCREEN || s == QUNS_BUSY)
    }
}
