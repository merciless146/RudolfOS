//! Runs a program that Blitzen wrote, in a Windows sandbox:
//!  - an AppContainer with no capabilities: no network, and no access to the user's files except the folders
//!    granted here (the run's own temporary folder, read and run access to the Python or compiler folder);
//!  - a Job Object: at most 1 GB of memory and 32 processes, and everything in it is ended at the time limit.
use std::ffi::c_void;
use std::os::windows::ffi::OsStrExt;
use std::path::Path;
use std::ptr::{null, null_mut};
use windows_sys::Win32::Foundation::{CloseHandle, GetLastError, LocalFree, GENERIC_READ, GENERIC_WRITE, HANDLE, INVALID_HANDLE_VALUE, WAIT_TIMEOUT};
use windows_sys::Win32::Security::Authorization::{
    GetNamedSecurityInfoW, SetEntriesInAclW, SetNamedSecurityInfoW, EXPLICIT_ACCESS_W, GRANT_ACCESS, SE_FILE_OBJECT, TRUSTEE_IS_SID,
    TRUSTEE_IS_WELL_KNOWN_GROUP,
};
use windows_sys::Win32::Security::Isolation::{CreateAppContainerProfile, DeriveAppContainerSidFromAppContainerName};
use windows_sys::Win32::Security::{
    FreeSid, ACL, DACL_SECURITY_INFORMATION, PSECURITY_DESCRIPTOR, PSID, SECURITY_ATTRIBUTES, SECURITY_CAPABILITIES,
    SUB_CONTAINERS_AND_OBJECTS_INHERIT,
};
use windows_sys::Win32::Storage::FileSystem::{
    CreateFileW, CREATE_ALWAYS, FILE_ALL_ACCESS, FILE_ATTRIBUTE_NORMAL, FILE_GENERIC_EXECUTE, FILE_GENERIC_READ, FILE_SHARE_READ, FILE_SHARE_WRITE,
    OPEN_EXISTING,
};
use windows_sys::Win32::System::JobObjects::{
    AssignProcessToJobObject, CreateJobObjectW, JobObjectExtendedLimitInformation, SetInformationJobObject, TerminateJobObject,
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION, JOB_OBJECT_LIMIT_ACTIVE_PROCESS, JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION, JOB_OBJECT_LIMIT_JOB_MEMORY,
    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE,
};
use windows_sys::Win32::System::Threading::{
    CreateProcessW, DeleteProcThreadAttributeList, GetExitCodeProcess, InitializeProcThreadAttributeList, ResumeThread, UpdateProcThreadAttribute,
    WaitForSingleObject, CREATE_NO_WINDOW, CREATE_SUSPENDED, CREATE_UNICODE_ENVIRONMENT, EXTENDED_STARTUPINFO_PRESENT, LPPROC_THREAD_ATTRIBUTE_LIST,
    PROCESS_INFORMATION, STARTF_USESTDHANDLES, STARTUPINFOEXW,
};

const ATTR_HANDLE_LIST: usize = 0x0002_0002;
const ATTR_SECURITY_CAPABILITIES: usize = 0x0002_0009;
const CONTAINER: &str = "Blitzen.Sandbox";

fn wide(s: &std::ffi::OsStr) -> Vec<u16> {
    s.encode_wide().chain(std::iter::once(0)).collect()
}
fn ws(s: &str) -> Vec<u16> {
    wide(std::ffi::OsStr::new(s))
}

pub struct Sid(PSID);
impl Drop for Sid {
    fn drop(&mut self) {
        unsafe {
            FreeSid(self.0);
        }
    }
}

/// The sandbox's identity (created on first use).
pub fn container() -> Result<Sid, String> {
    let mut name = ws(CONTAINER);
    let mut disp = ws("Blitzen sandbox");
    let mut desc = ws("Runs the code Blitzen checks, with no network and no access to your files");
    let mut sid: PSID = null_mut();
    unsafe {
        if CreateAppContainerProfile(name.as_mut_ptr(), disp.as_mut_ptr(), desc.as_mut_ptr(), null_mut(), 0, &mut sid) >= 0 {
            return Ok(Sid(sid));
        }
        let hr = DeriveAppContainerSidFromAppContainerName(name.as_mut_ptr(), &mut sid);
        if hr >= 0 {
            Ok(Sid(sid))
        } else {
            Err(format!("could not set up the sandbox (error 0x{:08x})", hr as u32))
        }
    }
}

/// Lets the sandbox read and run (`write`: also change) a folder and everything in it.
pub fn grant(sid: &Sid, path: &Path, write: bool) -> Result<(), String> {
    let mut p = wide(path.as_os_str());
    unsafe {
        let mut old: *mut ACL = null_mut();
        let mut sd: PSECURITY_DESCRIPTOR = null_mut();
        let e = GetNamedSecurityInfoW(
            p.as_mut_ptr(),
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION,
            null_mut(),
            null_mut(),
            &mut old,
            null_mut(),
            &mut sd,
        );
        if e != 0 {
            return Err(format!("could not read the permissions of {} (error {})", path.display(), e));
        }
        let mut ea: EXPLICIT_ACCESS_W = std::mem::zeroed();
        ea.grfAccessPermissions = if write {
            FILE_ALL_ACCESS
        } else {
            FILE_GENERIC_READ | FILE_GENERIC_EXECUTE
        };
        ea.grfAccessMode = GRANT_ACCESS;
        ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea.Trustee.ptstrName = sid.0 as *mut u16;
        let mut new: *mut ACL = null_mut();
        let e = SetEntriesInAclW(1, &ea, old, &mut new);
        if e != 0 {
            LocalFree(sd as _);
            return Err(format!("could not build the permissions for {} (error {})", path.display(), e));
        }
        let e = SetNamedSecurityInfoW(
            p.as_mut_ptr(),
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION,
            null_mut(),
            null_mut(),
            new,
            null(),
        );
        LocalFree(new as _);
        LocalFree(sd as _);
        if e != 0 {
            return Err(format!("could not give the sandbox access to {} (error {})", path.display(), e));
        }
    }
    Ok(())
}

fn quote(a: &str) -> String {
    if !a.is_empty() && !a.contains(&[' ', '\t', '"'][..]) {
        return a.to_string();
    }
    let mut s = String::from("\"");
    let mut bs = 0;
    for c in a.chars() {
        match c {
            '\\' => bs += 1,
            '"' => {
                s.push_str(&"\\".repeat(bs * 2 + 1));
                s.push('"');
                bs = 0;
            }
            _ => {
                s.push_str(&"\\".repeat(bs));
                s.push(c);
                bs = 0;
            }
        }
    }
    s.push_str(&"\\".repeat(bs * 2));
    s.push('"');
    s
}

pub struct Output {
    pub code: i64,
    pub timeout: bool,
    pub out: String,
}

/// Runs `exe args` in `dir` inside the sandbox, with only the variables in `env`, for at most `secs` seconds.
pub fn run(sid: &Sid, exe: &Path, args: &[String], dir: &Path, env: &[(String, String)], secs: u32, mem: usize) -> Result<Output, String> {
    let out_path = dir.join("blitzen-output.txt");
    let mut cmdline = quote(&exe.to_string_lossy());
    for a in args {
        cmdline.push(' ');
        cmdline.push_str(&quote(a));
    }
    let mut cmd = ws(&cmdline);
    let mut exe_w = wide(exe.as_os_str());
    let mut dir_w = wide(dir.as_os_str());
    let mut envb: Vec<u16> = Vec::new();
    let mut env = env.to_vec();
    env.sort_by(|a, b| a.0.to_uppercase().cmp(&b.0.to_uppercase()));
    for (k, v) in &env {
        envb.extend(format!("{k}={v}").encode_utf16());
        envb.push(0);
    }
    envb.push(0);
    unsafe {
        let sa = SECURITY_ATTRIBUTES {
            nLength: std::mem::size_of::<SECURITY_ATTRIBUTES>() as u32,
            lpSecurityDescriptor: null_mut(),
            bInheritHandle: 1,
        };
        let mut op = wide(out_path.as_os_str());
        let hout: HANDLE = CreateFileW(
            op.as_mut_ptr(),
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            null_mut(),
        );
        if hout == INVALID_HANDLE_VALUE {
            return Err(format!("could not create {} (error {})", out_path.display(), GetLastError()));
        }
        let mut nul = ws("NUL");
        let hin: HANDLE = CreateFileW(
            nul.as_mut_ptr(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &sa,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            null_mut(),
        );
        if hin == INVALID_HANDLE_VALUE {
            CloseHandle(hout);
            return Err(format!("could not open NUL (error {})", GetLastError()));
        }

        let mut size: usize = 0;
        InitializeProcThreadAttributeList(null_mut(), 2, 0, &mut size);
        let mut buf = vec![0u64; size / 8 + 1];
        let list = buf.as_mut_ptr() as LPPROC_THREAD_ATTRIBUTE_LIST;
        if InitializeProcThreadAttributeList(list, 2, 0, &mut size) == 0 {
            CloseHandle(hout);
            CloseHandle(hin);
            return Err(format!("could not prepare the sandbox (error {})", GetLastError()));
        }
        let mut caps: SECURITY_CAPABILITIES = std::mem::zeroed();
        caps.AppContainerSid = sid.0;
        let handles: [HANDLE; 2] = [hout, hin];
        let ok1 = UpdateProcThreadAttribute(
            list,
            0,
            ATTR_SECURITY_CAPABILITIES,
            &caps as *const _ as *const c_void,
            std::mem::size_of::<SECURITY_CAPABILITIES>(),
            null_mut(),
            null(),
        );
        let ok2 = UpdateProcThreadAttribute(
            list,
            0,
            ATTR_HANDLE_LIST,
            handles.as_ptr() as *const c_void,
            std::mem::size_of_val(&handles),
            null_mut(),
            null(),
        );
        if ok1 == 0 || ok2 == 0 {
            let e = GetLastError();
            DeleteProcThreadAttributeList(list);
            CloseHandle(hout);
            CloseHandle(hin);
            return Err(format!("could not prepare the sandbox (error {e})"));
        }

        let mut si: STARTUPINFOEXW = std::mem::zeroed();
        si.StartupInfo.cb = std::mem::size_of::<STARTUPINFOEXW>() as u32;
        si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        si.StartupInfo.hStdInput = hin;
        si.StartupInfo.hStdOutput = hout;
        si.StartupInfo.hStdError = hout;
        si.lpAttributeList = list;
        let mut pi: PROCESS_INFORMATION = std::mem::zeroed();
        let ok = CreateProcessW(
            exe_w.as_mut_ptr(),
            cmd.as_mut_ptr(),
            null(),
            null(),
            1,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
            envb.as_ptr() as *const c_void,
            dir_w.as_mut_ptr(),
            &si.StartupInfo,
            &mut pi,
        );
        let e = GetLastError();
        DeleteProcThreadAttributeList(list);
        CloseHandle(hout);
        CloseHandle(hin);
        if ok == 0 {
            return Err(format!("could not start {} in the sandbox (error {e})", exe.display()));
        }

        let job = CreateJobObjectW(null(), null());
        let mut lim: JOBOBJECT_EXTENDED_LIMIT_INFORMATION = std::mem::zeroed();
        lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
            | JOB_OBJECT_LIMIT_JOB_MEMORY
            | JOB_OBJECT_LIMIT_ACTIVE_PROCESS
            | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
        lim.BasicLimitInformation.ActiveProcessLimit = 32;
        lim.JobMemoryLimit = mem;
        let limited = !job.is_null()
            && SetInformationJobObject(
                job,
                JobObjectExtendedLimitInformation,
                &lim as *const _ as *const c_void,
                std::mem::size_of::<JOBOBJECT_EXTENDED_LIMIT_INFORMATION>() as u32,
            ) != 0
            && AssignProcessToJobObject(job, pi.hProcess) != 0;
        if !limited {
            // never run it without its limits
            windows_sys::Win32::System::Threading::TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if !job.is_null() {
                CloseHandle(job);
            }
            return Err(format!("could not limit the program's time and memory (error {})", GetLastError()));
        }
        ResumeThread(pi.hThread);
        let w = WaitForSingleObject(pi.hProcess, secs.saturating_mul(1000));
        let timeout = w == WAIT_TIMEOUT;
        let mut code: u32 = 1;
        if !timeout {
            GetExitCodeProcess(pi.hProcess, &mut code);
        }
        TerminateJobObject(job, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(job);

        let mut out = String::new();
        if let Ok(b) = std::fs::read(&out_path) {
            let cut = b.len() > 64 * 1024;
            out = String::from_utf8_lossy(&b[..b.len().min(64 * 1024)]).to_string();
            if cut {
                out.push_str("\n… (output cut at 64 KB)\n");
            }
        }
        if timeout {
            if !out.is_empty() && !out.ends_with('\n') {
                out.push('\n');
            }
            out.push_str(&format!("time limit: the program ran for more than {secs} seconds\n"));
        }
        Ok(Output {
            code: if timeout { 124 } else { code as i32 as i64 },
            timeout,
            out,
        })
    }
}
