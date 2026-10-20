# Rudolf OS 1.2.5 update plan

1.2.5 adds new features on top of 1.2.4 (which fixed what the first real-PC test found). Seven items, built and
released in four stages so that each stage is tested on its own. Version: `iso/VERSION` 1.2.5, `RVER` 1.2.5.

| # | Item | Size | Stage |
|---|------|------|-------|
| 7 | Blitzen: Lua, Luau and Pilot.lua | large | A |
| 4 | Linux apps over Wi-Fi | small | A |
| 5 | Games page in the App Store | small | A |
| 3 | Windows in the Rudolf OS boot menu | medium | B |
| 1 | Make a Rudolf OS USB stick | large | C |
| 6 | Backup to a second USB stick | medium | C |
| 2 | Hardware check in Task Manager | large | D |

Stage A is desktop and build work only (no new system programs); B touches the boot menu (Secure Boot tests);
C and D add system programs and share the drive-writing code. One pull request per stage, each built to beta
and tested in QEMU before the next starts; 1.2.5 goes to stable and gets a disc after stage D.

---

## 7. Blitzen: Lua, Luau and Pilot.lua

Blitzen's language list (`BLZ_LANGS` in `desktop/blitzen26.js`) is Auto, BlockLua, JavaScript, C++, Python. Lua
is reachable only as BlockLua, whose reference card pushes the model toward one-statement-per-line block shapes.
1.2.5 adds three languages:

| Language | What it is | Runs where | Checked by |
|---|---|---|---|
| **Lua** | standard Lua 5.3, any style | Fengari in the page (already shipped), offline, browser version too | running it |
| **Luau** | Roblox's Lua dialect: type annotations, `continue`, `+=`, string interpolation, generalized iteration | the Luau VM compiled to WebAssembly, offline, browser version too | `luau-analyze` (types), then running it |
| **Pilot.lua** | Luau microcontroller scripts for the Roblox game *Waste of Space* (parts, ports, screens, modems) | a simulator with stand-in parts (labelled "simulated"); the real thing runs only in the game | `luau-analyze` with the Pilot.lua type definitions, then the simulator |

### Runtimes
- **Lua**: reuse `blzRunLua` (Fengari, 10 s limit, `sys` and `gl` libraries) without the BlockLua constraints.
- **Luau**: build Luau (luau-lang/luau, MIT licence) with Emscripten in CI, pinned to a release tag and checksum:
  `lib/luau/luau.wasm` + `luau.js` (the VM and compiler, about 1 MB) and `luau-analyze.wasm` (the type checker).
  `blzRunLuau(code)` runs in a Web Worker with the same 10 s limit and print capture as JavaScript; `print`,
  `string`, `table`, `math`, `buffer`, `coroutine`, `utf8`, `os.clock`/`os.time` are available; `game`, `workspace`,
  `Instance` and other Roblox engine objects are not, and Blitzen says so when code needs them.
- **Roblox scripts**: code that uses Roblox engine APIs is type-checked against the Roblox definitions from
  luau-lsp (MIT licence, pinned file `lib/luau/roblox.d.luau`) and shown as "checked, not run" rather than failing.
- **Pilot.lua**: `lib/luau/pilot.d.luau`, the API definitions (Part, Screen, Speaker, Modem, Disk, Keyboard,
  Microphone, sensors, StarMap, Gyro, Servo…), written for Rudolf OS from the public Pilot.lua documentation and
  checked against the community Luau LSP definitions (their licence decides whether we can copy or must
  rewrite). The simulator (`blzPilotSim`) provides those objects with logging stand-ins: a Screen draws into a
  small canvas in the answer, Speaker:Chat and Modem calls print what they would do, sensors return values set in
  a "parts" panel, `GetPart`/`GetPartFromPort`-style lookups return the stand-ins. Every simulated run is marked
  "Simulated: Waste of Space is not running".

### Blitzen changes (`blitzen26.js`, `blitzpacks26.js`)
- `BLZ_LANGS`: Auto, BlockLua, **Lua**, **Luau**, **Pilot.lua**, JavaScript, C++, Python.
- `blzLang`: ```` ```lua ```` → Lua (BlockLua only when BlockLua is chosen), ```` ```luau ```` → Luau,
  ```` ```pilot ````, ```` ```pilotlua ````, ```` ```pilot.lua ```` → Pilot.lua; `BLZ_LNAME` and the run/check list follow.
- `blzRun`: `luau` → `blzRunLuau`; `pilot` → analyze, then `blzPilotSim`.
- Context packs (each under about 800 words, like the others):
  - `lua`: Lua 5.3 language card plus Rudolf OS's `sys` and `gl` libraries, no block rules.
  - `luau`: the differences from Lua 5.3 (types, `continue`, compound assignment, interpolation, `if` expressions,
    generalized iteration, `buffer`), common Roblox patterns (Instance.new, services, events, RunService), and what
    runs here versus only in Roblox Studio.
  - `pilot`: Pilot.lua microcontroller basics (how a microcontroller gets its parts and ports, events, the most used
    part methods), and the rule "never invent part methods that are not in this card".
- `BLZ_SYSTEM`: lists the new languages and fences (```` ```luau ````, ```` ```pilot ````).
- Fix-it loop: Luau type errors from `luau-analyze` go back to the model like runtime errors (up to 2 fixes).
- "Ask Blitzen" from the Code Editor: `.LUA` files ask Lua, a `--!strict`/`--!nonstrict` first line or `.LUAU` files
  ask Luau, a `-- pilot` first line asks Pilot.lua. The Code Editor gets `.LUAU` with Lua highlighting.

### Tests
- Host (`iso/qt/shell2/blz_t.js`, new): language list, fence detection, each runtime on a sample (Luau types,
  `continue`, interpolation; a Pilot.lua script that writes to a Screen and reads a sensor), the 10 s limit, a type
  error fed back to the fix loop (with a stand-in model reply).
- Model quality: 15 prompts per new language run through the real model in QEMU; record how many answers run or
  check clean on the first try and after fixes. A pack is rewritten if fewer than half pass.
- Browser version (the artifact): Lua and Luau run without rudolfd.

---

## 4. Linux apps over Wi-Fi
- rudolfd (`api_apps` install): drop the "needs a network cable" refusal; report the connection type and, for a
  download over 500 MB on Wi-Fi, the App Store asks first ("About 1 GB over Wi-Fi. Continue?").
- Settings › Network › a network can be marked **Metered**: then every large download asks.
- Flatpak installs that stop are resumed (`flatpak install` again picks up the parts already fetched).
- Tests: QEMU with the virtual Wi-Fi (mac80211_hwsim) used by the Wi-Fi tests: install a small Flathub app over
  Wi-Fi; cut the link mid-download and resume.

## 5. Games page in the App Store
- A **Games** tab in the App Store: Roblox (Sober), Steam (Flathub), Prism Launcher (Minecraft), RetroArch, and
  the built-in emulators and Quake, each with its size and a label **Needs the graphics card** or **Runs anywhere**.
- Uses 1.2.4's renderer check (Task Manager's "Drawn by"): when Rudolf OS draws with the processor, a note says
  which games will be slow and why, and links to Task Manager.
- Tests: host harness for the page and its labels; QEMU installs one game.

## 3. Windows in the Rudolf OS boot menu
- UEFI: at disc build time, `iso/boot/sb/loader/loader.conf` gets `auto-entries yes`, so systemd-boot lists
  **Windows Boot Manager** when it finds `\EFI\Microsoft\Boot\bootmgfw.efi` on any ESP, below the Rudolf OS
  entries. With Secure Boot on it is started by the firmware's own check (Microsoft-signed).
- BIOS: "Boot from hard disk" already exists in Rudolf Boot.
- Settings › Update and recovery › **Boot menu**: which entry starts by default and the wait (2–10 s), stored
  through systemd-boot's `LoaderEntryDefault` and `LoaderConfigTimeout` EFI variables (no file on the disc changes).
- Tests: QEMU (sb_t.py part G) with a second disk holding a stand-in "Windows" ESP whose `bootmgfw.efi` is a
  small EFI program signed by the Microsoft-keyed OVMF test key, with Secure Boot on and off; the default entry
  and timeout survive a restart.

## 1. Make a Rudolf OS USB stick
- Settings › Storage and backup › **Make a Rudolf OS USB stick…**
  - Source: the running Rudolf OS (its image on the drive it started from) or a downloaded `rudolf-os-*.iso` in
    Downloads, checked as in "Install from" (`rudolf-update verify-manifest`).
  - Target: a removable drive of 2 GB or more that is not the running drive, picked from a list with model and
    size; an administrator confirms by typing the size shown.
  - Writes the image, reads it back and compares checksums, then adds RUDOLFDATA in the rest of the stick
    (the partition code of `keepfiles.sh` moves into `/usr/lib/rudolf/partlib.sh`, shared with `install.sh`).
  - Option: **copy my files** (this session's home folder) onto the new stick.
- `mkstick.sh` (new) does the work; rudolfd starts it as a job (`POST mkstick?disk=…&image=…&files=0|1`).
- Tests: QEMU with two virtual USB sticks: make a stick, then start from it with BIOS and with UEFI Secure Boot
  on (key enrolled); check files were copied when asked and not otherwise.

## 6. Backup to a second USB stick
- Settings › Storage and backup › Back up everything › **To a USB stick**: writes the backup file (accounts' files
  still encrypted, settings, the apps list) onto a plugged-in drive as one file with a checksum file beside it.
- First start (no account yet): **Restore from a backup** finds backups on plugged-in drives.
- Tests: QEMU with a second virtual stick: back up, factory reset, restore; a damaged backup is refused.

## 2. Hardware check in Task Manager
Task Manager › Hardware › **Check this computer…**: a guided check of about a minute, one step per screen.
- **Sound**: a tone on each output (speakers, headphones, HDMI/DisplayPort): "Did you hear it?"
- **Microphone**: a 3-second level meter.
- **Network**: wired link speed (with the 1.2.4 cable hint), a Wi-Fi scan, a download test to the update server.
- **USB**: "plug in a USB stick" and watch it appear.
- **Graphics**: a WebGL scene with frames per second, the renderer and glamor state.
- **Sleep** (optional, laptops): suspend for 10 seconds and wake; check screen, network and sound come back.
- Result: a pass/fail list; "Report a problem" attaches it to the system report (no personal data).
- Tests: host harness for the flow; QEMU for sound (the virtual HDA card), USB and network.

---

## Release steps (each stage)
1. Branch `rudolf-1.2.5-<stage>`, implement, host tests, local release build from the published beta.
2. Test disc (debug, test keys) and the stage's QEMU tests; existing suites must still pass: `sb_t.py` (31),
   `t124_t.py` (18), `v120_t.py` (34), `tm_t.js`, `dl_t.js`.
3. Pull request; after merge, build to beta and check the published files.
4. After stage D: CHANGELOG 1.2.5, promote to stable, disc `disc-1.2.5` from `disc-1.2.4`.

## Risks
- **Luau in WebAssembly**: the Emscripten build must stay small and start fast; if `luau-analyze` is too large
  for the page, type checks run only on the installed system (a native `luau-analyze` in the base layer).
- **Pilot.lua accuracy**: the game changes its API; the definitions file carries its date and source, and
  Blitzen's answers say they are checked against that date's API.
- **Licences**: Luau and luau-lsp are MIT (fine to ship with their notices). Pilot.lua's community definitions need
  their licence checked first; without one, Rudolf OS writes its own definitions from the public documentation.
- **Wi-Fi downloads** of 1 GB on slow networks: resume and a clear size warning cover it.
- **Windows in the boot menu**: a firmware that lists Windows twice (its own entry and ours) is harmless.
