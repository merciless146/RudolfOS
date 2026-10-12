# Blitzen Plus for Windows

Blitzen, the offline coding assistant from Rudolf OS, on a Windows 10/11 PC. Blitzen Plus runs
Qwen2.5-Coder-7B-Instruct (4-bit, 4.7 GB) with llama.cpp, entirely on an NVIDIA card with 8 GB or more
(Vulkan on AMD or Intel cards, or the processor without one). Nothing typed leaves the PC.

Download both from the release **blitzen-plus**:

| File | What it is |
|---|---|
| `Blitzen-Setup.exe` | The Blitzen app (Stage 2). Installs for the current user. Settings > Download the model fetches the engine and the model and checks them. |
| `Blitzen-Plus-Stage1.zip` | The browser-only version (Stage 1): `Setup Blitzen.cmd` puts the engine, the model and the page in `C:\Blitzen` and runs a speed test. |

## What the app does

- **Model:** downloads the list `blitzen.txt` from the release **blitzen-win-plus**, refuses it unless it is signed
  with a key in `iso/trusted-keys.txt`, then checks every model file's SHA-256 against it. A file changed later is
  refused when Blitzen loads it. A Stage 1 download in `C:\Blitzen\models` is reused after the same check.
- **Engine:** llama.cpp `b11559` (CUDA, Vulkan or CPU build, picked from the hardware), checked against GitHub's
  SHA-256. It listens on 127.0.0.1 only, on a random port, and needs a random key for every request.
- **Graphics card:** the model unloads after 10 minutes without a question and when a full-screen game starts,
  and loads again (a few seconds) when you ask.
- **Checking code:** JavaScript and BlockLua run in the page. Python (python-build-standalone) and C++ (llvm-mingw)
  are downloaded once when first needed and run in a Windows sandbox: an AppContainer with no capabilities (no
  network, no access to your files) and a Job Object (10 seconds, 1 GB, 32 processes).
- **Windows:** tray icon (closing the window keeps Blitzen there), Win+Shift+B, and "Ask Blitzen about this file"
  in File Explorer (on Windows 11 under "Show more options"). Chats are saved in `Documents\Blitzen`.
- **Data:** `%LOCALAPPDATA%\io.github.merciless146.blitzen` (engine, model, Python, compiler, logs).

## Publishing

1. **Model list:** Actions > Blitzen model > Run workflow with release `blitzen-win-plus`, repository
   `Qwen/Qwen2.5-Coder-7B-Instruct-GGUF`, pattern `q4_k_m`, name `Blitzen Plus (Qwen2.5-Coder 7B)`, licence
   `Apache-2.0`. The model is over GitHub's 2 GB limit, so the signed list links to Hugging Face instead.
2. **App and zip:** the workflow "Blitzen for Windows" builds both on every pull request that changes `windows/`,
   and publishes them as the release `blitzen-plus` when that reaches `main` (or on Run workflow).

## Building on your own PC

Needs Rust (rustup.rs), Node.js 22 and Visual Studio Build Tools (C++ workload):

```
bash windows/sync-ui.sh            (or copy the four files it names into windows\ui)
cd windows\app\src-tauri
npx @tauri-apps/cli@2 build
```

The installer is written to `target\release\bundle\nsis`.

## Files

- `ui/`: the page, shared by both stages (`blitzen.js`, `index.html`, `style.css`; `sync-ui.sh` adds the context
  packs, the BlockLua converter and Fengari from `desktop/` and `iso/www/lib/`).
- `stage1/`: `setup-blitzen.ps1`, `Setup Blitzen.cmd`, the README and the system prompt file of the zip.
- `app/src-tauri/`: the app (`main.rs`: engine, model list, commands; `sandbox.rs`; `net.rs`; `sys.rs`).
