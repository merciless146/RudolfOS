BLITZEN PLUS FOR WINDOWS - STAGE 1
==================================

Blitzen is an offline coding assistant. Blitzen Plus runs the Qwen2.5-Coder-7B model on your graphics
card with llama.cpp. After setup it needs no internet, and nothing you type leaves this PC.

What you need
  - Windows 10 or 11, 64-bit
  - About 7 GB free on the drive it goes on (about 5 GB once it is set up)
  - An NVIDIA card with 8 GB or more works best (the RTX 3080 is ideal). AMD or Intel cards use the
    Vulkan engine; without a graphics card it runs on the processor, much more slowly.
  - Internet for the setup only: about 5 GB from github.com and huggingface.co

Set it up
  1. Right-click the zip > Properties > tick "Unblock" > OK. Then right-click > Extract All.
  2. In the extracted folder, double-click "Setup Blitzen.cmd".
     If Windows shows "Windows protected your PC", click More info > Run anyway.
  3. Wait while it downloads the engine and the model (10-30 minutes, depending on your connection).
     Each file is checked against its published SHA-256 before it is used. If the download stops,
     run the setup again: it carries on where it stopped.
  4. It then measures the speed and checks Blitzen (about two minutes), and opens Blitzen in your browser.

  It installs to C:\Blitzen. For another drive, open PowerShell in the extracted folder and run:
    powershell -ExecutionPolicy Bypass -File setup-blitzen.ps1 -Dir D:\Blitzen
  Other options: -StartWithWindows (start Blitzen when you sign in), -Context 8192 (less graphics memory),
  -Engine vulkan|cpu, -SkipBench, -SkipCheck, -NoStart.

Use it
  - Start menu > Blitzen, or C:\Blitzen\start-blitzen.cmd. The chat opens at http://127.0.0.1:8099
  - Start menu > Stop Blitzen frees the graphics memory, for example before playing a game.
  - Ask in plain words. Choose a language at the top, or leave it on Auto.
  - With "Check code by running it" on, Blitzen runs its JavaScript and BlockLua answers in the page
    (no network, 10-second limit) and fixes them up to twice if they fail. Every code block has
    Run, Copy and Save.
  - Python and C++ answers are written and explained, but not run: the Blitzen app (Stage 2) runs them
    in a sandbox. Until then, Copy or Save them and run them yourself.
  - History reopens earlier chats (kept in this browser). New chat starts over.

Your results
  C:\Blitzen\blitzen-check.txt has the speed test and checks:
    - writing speed (tg128): passes at 60 tokens per second or more
    - first words within 1 second, and within 10 seconds from a cold start
    - graphics memory used by Blitzen: passes under 7.5 GB
    - not reachable from the network: it listens on 127.0.0.1 only
  If a check fails, the file says what was measured.

Files in C:\Blitzen
  engine\               llama.cpp (from github.com/ggml-org/llama.cpp, MIT licence)
  models\               the model (Qwen2.5-Coder-7B-Instruct, Apache-2.0 licence) and blitzen-model.txt
                        with its SHA-256
  ui\                   the Blitzen page (includes Fengari, MIT licence, for BlockLua)
  start-blitzen.cmd     start; stop-blitzen.cmd: stop
  blitzen-system.txt    Blitzen's system prompt and language packs, for use in other chat programs
  logs\                 engine and speed-test logs

Remove it
  Run Stop Blitzen, delete C:\Blitzen, and delete "Blitzen" and "Stop Blitzen" from
  %APPDATA%\Microsoft\Windows\Start Menu\Programs (and Blitzen from shell:startup if you added it).

Problems
  - "needs about 7 GB free": use -Dir on another drive.
  - The engine does not start: see C:\Blitzen\logs\engine-check.err.log. An old NVIDIA driver is the
    usual cause; update it, or run the setup with -Engine vulkan.
  - Slow answers: something else may be using the graphics card (a game, or a browser with many video
    tabs). Run Stop Blitzen and start it again.
