# Native vs Standard: test findings (2026-11-26)

A hardware session on the owner's Vivobook, following `docs/native-vivobook-checklist.md` through the development link.
**V58 passed**: the machine starts from its own drive with no stick attached. **N1 matches**: full-screen video on
Native's GPU path shows the picture, not a black screen. **N2 matches** on the clean attempt: Notepad's Open raises the
page's own file dialog, centred over a dimmed desktop, every control present. Two earlier clicks raised a blank "Rudolf
OS" window instead; that is recorded as an unexplained intermittent symptom, not as what Open does. **N3 matches**:
Task Manager's Apps list showed a row for each open window, with Switch to and End task. Every V item except V58 is
deferred, apart from **V1–V3**, run on 2026-11-27 (section below). Nothing below claims that something works on the machine unless the machine said so itself — and where this
session read the screen wrong, it says so.

## Header

| What | Value |
|---|---|
| Build | **Rudolf OS 1.6.2.261124**, serial **2026112422**, **Official Rudolf OS** (from the result's `system` field) |
| Kernel | `6.18.65-rudolf` (`#1 SMP PREEMPT_DYNAMIC Tue Nov 24 23:06:26 UTC 2026`) |
| Machine | ASUSTeK **Vivobook Go E1404FA_E1404FA** (board E1404FA) — the test laptop |
| Started | From **its own drive**, with **no stick attached**: `/live/media` is `/dev/loop0` backed by **`/dev/nvme0n1`**, no `/dev/sd*` exists, `rudolf.slot=b`, `rudolf.sb=1`, `rudolf.mediadisk=/dev/nvme0n1`, `lockdown=integrity`. That first boot was about 22:49Z (uptime 38 min at 23:27:51Z) |
| Persistent disk | `/dev/nvme0n1p3`, 116.4 GB, 41.2 GB used (36 %), mounted at `/persist` and `/home/rudolf` |
| Read-only | **On**, in the link's own words: the banner reads "can look at this computer (read-only)" and the Allow dialog "It runs read-only". **But** `sh` ran as `uid=0(root)`, where `DEVLINK.md` says it runs as `nobody` — recorded as a question in "Two things the banner and the command disagree on" below |
| Ask first | **On** until about 23:41Z: `000584`-`000590` all read `approved: allowed on the computer` (a command allowed with Ask first off reads `not asked (Ask first was off)`, which is the form from `000592` on). `000591` was **not answered** within 150 s and ran when the owner returned |
| Home folders | **Not established** — nothing in this session read a home folder |
| The link | `merciless146/rudolfos-devlink`, branch `link`, `iso/systools/devlink.py` |
| Today's `main` | `745fcf18`, 2026-11-26 19:43Z, "Merge pull request #712 from merciless146/newfile-name"; this branch is `ros-vivobook-findings` |

**The version matters.** The checklist was written for **1.6.0** (`docs/native-vivobook-checklist.md`). The machine runs
**1.6.2.261124**, and `main` is **1.6.3 in progress**. So the checklist is two versions behind the machine and three
behind `main`, and a few of its "Done in #410" notes have been overtaken by later work. Before the next hardware
session, the checklist needs re-reading against the build that machine actually runs, not against the one it was
written on.

## Summary

- **The link was dead for two days.** The last result before this session was `000583`, **2026-11-24T00:40:51Z**. This
  session's command `000584` was written at **2026-11-26T23:19:07Z** and sat unanswered for 8½ minutes, then ran at
  **23:27:51Z**. `DEVLINK.md` ends a link after 4 hours without a command, so the link had ended by inactivity long
  before this session started; it came back when the computer did (the machine's own start was about 22:49Z today).
- **N1–N32: every fix named in `docs/native-findings-2026-11-15.md` is in today's `main`,** and the harness cases that
  check them are still in `iso/qt/native/same_t.js`. That is a fact about the repository, not about the machine: not one
  of them was re-tested on the Vivobook.
- **V58 passed.** The machine restarted on the session's `restart` at **23:34:07Z** and was up **54 s later** on the same build, still with **no stick attached**: it starts from its own drive, no firmware setup screen. Evidence: `docs/native-findings-2026-11-26/V58-native.png`.
- **N1 matches**: on Native's GPU path (`rudolf-native --draw=gpu`) a full-screen video fills 1920x1080, the taskbar goes away and the picture keeps moving. Evidence: `N1-native.png`, `N1-fullscreen-a.png`.
- **N2 matches** on a clean attempt (`000644`-`000646`, identical across 15 s): the card `desktop/filedlg59.js` builds — centred, titled Open, path bar `C:\USERS\JOHN\DOCUMENTS`, a list with `REPORT 202511180011.TXT` and `WELCOME.TXT`, File name, Type "Text (.txt)", Cancel and Open — over a **dimmed** desktop. Evidence: `N2-native.png`.
- **A blank "Rudolf OS" window appeared twice and did not recur.** Two earlier Open clicks raised a ~1345 × 827 window with a pure white body (98.4 % near-white, 3 colours in a 1,180 × 700 crop), once as two stacked windows. The clean attempt produced the correct dialog, so this is an intermittent symptom, not N2's steady state: `N2-anomaly-blank-window.png`, `N2-anomaly-two-blank-windows.png`.
- **N3 matches**: Task Manager's Apps list carried **three rows for three windows** — `Task Manager`, `Notepad` (`Untitled - Notepad`) and `Settings` — each with **Switch to** and **End task**, instead of the one row for itself that 2026-11-15 recorded. Evidence: `N3-native.png`.
- **V59 was not needed** (the setup screen did not appear), and **the other 70 items are deferred**. No window was opened and no key was pressed by the session: it has no `xdotool`, `wmctrl`, `ydotool`, `import` or `ffmpeg` on that build, and `DISPLAY` is empty in its `sh`.
- **Nothing was written to the computer.** The single command that ran was a read (`uname`, `/proc/cmdline`, `/run/rudolf`,
  `df`); it changed nothing, and it was not a `put`, a `restart` or a firmware operation.

## How it was tested

- **One command, `000584`**, `kind: sh`, `timeout: 120`. Its script, as written in the mailbox:

  ```sh
  # Read-only: record the build, serial, hardware and the link's settings for this session's findings header. Reads only; writes nothing.
  uname -a; cat /etc/os-release; … /proc/version; uptime; cat /proc/cmdline; id
  ls -la /run/rudolf; df -h
  ```

- **Its result**: `status 0`, `statusText null`, `error null`, `approved allowed on the computer`,
  `started 2026-11-26T23:27:51Z`, `ended 2026-11-26T23:27:51Z` (same second: the whole script is reads).
- **What the host could not do**: this session's own machine is the Vivobook's Debian host, not Rudolf OS. It has no
  WebKitGTK, Xvfb, Playwright, Chromium or `rudolf-native` build, so none of the host harnesses
  (`same_t.sh`, `native59_t.sh`, `videolog60_t.sh`, `oneapp59_t.js`) were run here, and running them would not be
  evidence about the Vivobook anyway (`docs/testing.md`, rule 3). The machine itself is the subject; it was reachable
  only through the link.

### The link's record

| Command | Kind | Written | Ran | Status | Approved | Note |
|---|---|---|---|---|---|---|
| `000583` (previous session) | `sh` | 2026-11-24 | result **2026-11-24T00:40:51Z** | — | — | The last thing the computer ran before this session |
| `000584` (this session) | `sh` | **2026-11-26T23:19:07Z** | **2026-11-26T23:27:51Z** | `0` | `allowed on the computer` | Unanswered for 8 min 44 s; the script ran in the same second it started |

The 47½-hour gap between the two is not a fault of the link's protocol: it is the 4-hour inactivity rule, after which
the computer stops polling, and it polls again only once the link is turned on again.

This session's commands after the header read:

| Command | Kind | Written | Result | Status | Approved |
|---|---|---|---|---|---|
| `000585` | `sh` | 2026-11-26T23:33:27Z | same second | `0` | `allowed on the computer` |
| `000586` | `sh` | 2026-11-26T23:33:36Z | same second | `0` | `allowed on the computer` |
| `000587` | `restart` | 2026-11-26T23:34:00Z | 2026-11-26T23:34:07Z | `0` | `allowed on the computer` |
| `000588` | `sh` | 2026-11-26T23:35Z | same second | `0` | `allowed on the computer` |
| `000589` | `screenshot` | 2026-11-26T23:35Z | same second | `0` | `allowed on the computer` |
| `000590` | `sh` | 2026-11-26T23:36Z | same second | `0` | `allowed on the computer` |
| `000591` | `screenshot` | 2026-11-26T23:38Z | **none** | — | — (no answer: no result within 150 s) |

`000587`'s result carries `system: Rudolf OS 1.6.2.261124 (2026112422), Official Rudolf OS`, written seven seconds
before the machine went down. It came back at 23:35:15Z with `uptime` 54 s.

### Two things the banner and the command disagree on

Not findings with a checklist number, and not acted on: both are for the owner to decide.

1. **`read-only` and `uid=0`.** The red banner and the Allow dialog both say the link is read-only, and `DEVLINK.md`
   says `sh` then runs as `nobody`; the command ran as `uid=0(root)`. Either the read-only mode no longer drops to
   `nobody`, or the banner is wrong about what it guarantees. Nothing in this session wrote anything, so the machine is
   unchanged either way — but the promise and the behaviour should agree.
2. **When the banner's wording follows the setting.** The 23:35Z screenshot's banner read "can look at this computer
   (read-only)" although `restart` had been accepted a minute earlier, and `DEVLINK.md` says a read-only link refuses
   `restart`; from 23:41Z on, after the owner ticked changes and turned Ask first off, the same banner read "can control
   this computer · 8 h". So the banner does follow the setting, but it lagged it by at least a minute here. Recorded, not
   acted on.

## N1–N32 checked against today's `main`

Each row asks: is the fix that `docs/native-findings-2026-11-15.md` names actually an ancestor of today's `main`
(`745fcf18`), and is the harness case that checks it still there? "In 1.6.2.261124?" asks whether that fix predates the
machine's build (2026-11-24 22:xx) — if it does not, the machine cannot show it however carefully it is tested.
**No row was confirmed, refuted or re-measured on the machine today.**

| # | Finding | On today's `main` | Evidence | In 1.6.2.261124? | Confirmed on the Vivobook? |
|---|---|---|---|---|---|
| N1 | Full-screen video black | fixed | `d363693` 2026-11-15; case `video-fs` | yes | **yes — matches.** 2026-11-26 on `--draw=gpu`: the picture fills 1920x1080, no taskbar, 1,059-1,091 distinct colours, 61 % of the screen changed between two frames. `N1-native.png`, `N1-fullscreen-a.png` |
| N2 | File dialog cut off inside the window | fixed | `d363693`; case `dlg-open` | yes | **yes — matches** on the clean attempt: the page's own card, centred, dimmed desktop, path bar/list/File name/Type/Cancel/Open all present. `N2-native.png`. Two earlier clicks showed a blank "Rudolf OS" window; unexplained, not reproducible |
| N3 | Task Manager lists only itself | fixed | `d363693`; case `tm-apps` | yes | **yes — matches.** 2026-11-26: rows for Task Manager, Notepad and Settings, each with Switch to and End task. `N3-native.png` |
| N4 | The page's dialogs in a window of their own | fixed | `d363693`; cases `dlg-ask`, `dlg-camera`, `dlg-print`, `dlg-dark` | yes | **no evidence it reproduces**: the one dialog caught was a centred card over a dimmed desktop. The blank "Rudolf OS" windows had that shape but could not be reproduced; the camera and report dialogs were not tried |
| N5 | Sliders drawn by GTK | fixed | `6212d1f` 2026-11-15; cases `ctl-volume`, `ctl-mouse`, `ctl-media` | yes | no — deferred |
| N6 | Drop-down lists without chevrons | fixed, and the dark-mode chevron too | `6212d1f`; `a6da7aa8` 2026-11-16 (dark list); cases `ctl-wordlite`, `ctl-volume-dark` | yes | no — deferred |
| N7 | File Explorer: no "Folders on this device", 0 B used | fixed | `6212d1f`; case `explorer` | yes | no — deferred |
| N8 | About: "Apple GPU" | fixed | `068aa95` 2026-11-15; case `about` | yes | no — deferred |
| N9 | 24-hour time field in Clock | fixed | `6212d1f`; case `ctl-clock` | yes | no — deferred |
| N10 | Print dialog inside the app's window | fixed | `d363693`; case `dlg-print` | yes | no — deferred |
| N11 | Glacier Lake without northern lights | fixed | `36507ee` 2026-11-15; case `dyn-lake` | yes | no — deferred |
| N12 | Spirit Falls without its statue | fixed | `36507ee`; cases `dyn-oasis`, `dyn-oasis-dark` | yes | no — deferred |
| N13 | Colour box drawn by GTK | fixed | `6212d1f`; case `ctl-wordlite` | yes | no — deferred |
| N14 | Flash Player's own title-bar icon | fixed | `cb33ba5` 2026-11-15; case `title-icon` | yes | no — deferred |
| N15 | No window opening animation | fixed | `6254bc3f` 2026-11-17 (PR #412); case `wopen` | yes | no — deferred |
| N16 | Video statistics stop near the end | not reproduced | case `video-frames` present | — | no — a Vivobook check, deferred |
| N17 | Sandbox flag error (WebKit stand-in) | fixed | `cb33ba5`; case `sandbox-flags` | yes | no — deferred |
| N18 | Log video problems' noise | fixed in part (underflow lines kept by the owner's decision) | `d363693` | yes | no — deferred |
| N19 | Two WebKit web processes / memory | the test fixed; **memory with real use is a Vivobook check** | `cb33ba5`; `native59_t.sh` | yes | no — deferred |
| N20 | Log video problems emptied by each new window | fixed | `b83e625` 2026-11-16; `videolog60_t.sh` | yes | no — deferred |
| N21 | Right-click menus cut off inside apps | fixed | `b83e625`; case `app-menu` | yes | no — deferred |
| N22 | No scrollbars in the apps | fixed | `b83e625` | yes | no — deferred |
| N23 | The harness hid scrollbars | fixed | `b83e625` | yes | no — deferred |
| N24 | Display Properties' tiles 4-5 px taller | fixed | `b83e625`, with `510c0a3` | yes | no — deferred |
| N25 | Text boxes the page does not style | fixed | `b83e625` | yes | no — deferred |
| N26 | Check boxes | fixed | `d363693` | yes | no — deferred |
| N27 | Title bars in dark mode | fixed | `b83e625` (PR #409 finishes the frames); case `title-dark` | yes | no — deferred |
| N28 | GPU Lab says "Hardware" | fixed | `068aa95`; case `gpulab` | yes | no — deferred |
| N29 | Video frame callbacks never called | fixed | `b83e625`; case `video-frames` | yes | no — deferred |
| N30 | Start menu's search box and scrollbar | fixed | `510c0a3` 2026-11-16; case `menu` | yes | no — deferred |
| N31 | Oracle's focus on opening, Ctrl+T | **a Vivobook check** (real Oracle is Chromium) | — | — | no — deferred |
| N32 | Desktop sounds from app windows | fixed | CHANGELOG 1.6.0 ("on the Native desktop sound from the app windows … the desktop's own sounds play there too") | yes | no — deferred |

Also still open on `main`, and untouched by this session:

- **Night light tints only the app windows** (plan 1.6.3 / Part 12 item 9 series) — see checklist V40.
- **Touch mode's title bars** (Part 12 item 10) — see checklist V39.
- **Native's transitions** were changed again in `314e319b` (2026-11-25 07:00, "1.6.3 Part 8, item 2: Native's transitions
  drawn as Chromium's"), which is **after** the machine's build: the machine will not show it, so V47 and V66 on
  1.6.2.261124 are not a test of today's `main`.

## The checklist: V58 passed, V1–V3 fail on Native, the rest deferred

Every number in `docs/native-vivobook-checklist.md` (V1–V63 and V66–V74; there is no V64 or V65) is deferred. "Needs"
is what the machine alone can show; none of it was done.

| # | Area | Needs | Status |
|---|---|---|---|
| V1 | Sound Recorder (this build: Voice Recorder) | record 10 s, play back; 🎙️ in the taskbar | **fail (native)** — Record gives the click sound only; Chromium records; see 2026-11-27 below |
| V2 | Camera | first open, the "Allow access?" prompt, frame rate | **fail (native), pass in Chromium** — native shows "No camera is available or access is blocked"; no prompt on either engine; see below |
| V3 | Camera | a photo and a 5 s video saved and opened | **fail (native), pass in Chromium** — photo and video saved and opened in Chromium; native cannot start the camera |
| V4 | Calls | the call screen, 🎙️, mute, hang up | deferred |
| V5 | Microphone denied | the blocked message and how to allow it again | deferred |
| V6 | Media Player | video with sound, volume keys, the overlay | deferred |
| V7 | Media Player | another window opened while a video plays: gaps or clicks | deferred |
| V8 | Desktop sounds | Media Player › URL… › Add (N32) heard on Native | deferred |
| V9 | Bluetooth headphones | sound through them, and back when they are turned off | deferred |
| V10 | Sleep | Sleep, wait, wake: the desktop with its windows | deferred |
| V11 | Lid | close for 1 min, open: as V10 | deferred |
| V12 | Sleep with a video | paused or playing, sound back | deferred |
| V13 | Battery | unplug: the icon and percentage | deferred |
| V14 | Battery | the 20 % and 10 % warnings, once each | deferred |
| V15 | Power profiles | each profile applies at once; the taskbar battery's tip | deferred |
| V16 | Battery button | Settings' System health page | deferred |
| V17 | Battery saver | window opening animations off | deferred |
| V18 | Bluetooth | turn on, find, pair; the pairing code prompt | deferred |
| V19 | Bluetooth | unpair, turn off: list empty, switch off | deferred |
| V20 | Printing | WordPro › Print… to a real printer | **deferred — needs a printer** |
| V21 | Printing | Print to PDF from Notepad, opened from Documents | deferred |
| V22 | Second monitor | plug in HDMI: what each screen shows | **deferred — needs a second screen** |
| V23 | Second monitor | move a window towards it | **deferred — needs a second screen** |
| V24 | Touchpad | two-finger scroll: speed and direction | deferred |
| V25 | Touchpad | pinch to zoom in Image Viewer and Oracle | deferred |
| V26 | Touchpad | edge swipes | deferred |
| V27 | Debian container | a Linux app's window, title bar, taskbar button, Alt+Tab | deferred |
| V28 | Debian container | that app's File › Open dialog | deferred |
| V29 | Debian container | a Linux app with sound | deferred |
| V30 | Flatpak | open, use, close | deferred |
| V31 | Library games | download a game, launch, F11, sound | deferred |
| V32 | Gamepad | a game with a gamepad | deferred |
| V33 | Update | beta box ticked, Check for updates, Download, Restart | **deferred — needs the owner's agreement: updating changes the machine** |
| V34 | Update | beta unticked: "Up to date" or the release | **deferred — needs the owner's agreement** |
| V35 | Accounts | a second account, sign out, sign in as it | deferred |
| V36 | Accounts | Switch user from the start menu | deferred |
| V37 | Accounts | Lock (Super+L) with each account | deferred |
| V38 | Touch | on-screen keyboard into Notepad, Oracle and a Linux app | deferred |
| V39 | Touch | title bars at least 40 px, buttons 40×40 | deferred |
| V40 | Night light | the whole screen tinted, 8 PM-7 AM when scheduled | deferred |
| V41 | Long use | 30 minutes of mixed use, no restarts, no repeated notices | deferred |
| V42 | Memory | Task Manager at idle and with one app (N19) | deferred |
| V43 | Oracle | focus on opening, Ctrl+T (N31) | deferred |
| V44 | Video | frames counted to the end (N16) | deferred |
| V45 | Super+Tab | a video keeps moving in the overview | deferred |
| V46 | Notifications | a video moves round a notification | deferred |
| V47 | Windows | an app fades in and grows in 0.18 s | deferred |
| V48 | Files | drag to Notepad and Trash; copy between Notepad, Oracle, a Linux app | deferred |
| V49 | Screen saver | the saver, then the lock | deferred |
| V50 | Accessibility | magnifier, screen reader, high contrast | deferred |
| V51 | Alarms | an alarm with every window closed | deferred |
| V52 | Calendar | a reminder with every window closed | deferred |
| V53 | Calls and messages | 🎙️ and a notice with no window open (needs a phone or the SIM) | **deferred — needs a phone or SIM** |
| V54 | Alarms | an alarm with two windows open: once, not per window | deferred |
| V55 | Timers | a timer after Clock's window is closed | deferred |
| V56 | Task Manager | only it open: no extra row | deferred |
| V57 | Files | a drag from a Linux app's file manager onto the desktop, a folder, Trash, Notepad | deferred |
| V58 | Start-up entries | a restart without the stick | **passed** — the drive started on its own; see "V58" below |
| V59 | Start-up entries | Boot › Add New Boot Option if the setup screen appears | **not needed** — no setup screen appeared |
| V60 | Start-up entries | Secure Boot › Get new boot files… just opened, or a system report's start-up decisions | **deferred — needs the owner** (the entries below are the read-only half of it) |
| V61 | Start-up entries | the boot-entry fix's System health lines | **deferred — needs the owner** |
| V62 | Start-up entries | "Check this drive's start-up…" with the password | **deferred — needs the owner** |
| V63 | Start-up entries | "Put back the official start-up files…" on power | **deferred — needs the owner** |
| V66 | Windows | five apps from the start menu, a pin and a desktop icon | deferred |
| V67 | Windows | an app opened right after signing in | deferred |
| V68 | Menus | right-click menus replacing each other cleanly | deferred |
| V69 | Menus | the tray's pop-ups replacing each other | deferred |
| V70 | Icons | sign in: the icons with their pictures at once | deferred |
| V71 | Icons | New folder and New text document from the desktop menu | deferred |
| V72 | Icons | save to the Desktop, delete from it: the desktop at once | deferred |
| V73 | Backgrounds | each background at once, then Revert; again, then Discard | deferred |
| V74 | Display Properties | accent colour, dark mode, title-bar style, the screen saver's Preview | deferred |

V58 `passes`; V1, V2 and V3 were run on 2026-11-27 and **fail on Native** while **passing in Chromium on the same
build and machine** (next section). No other item is marked `matches`, `minor`, `looks wrong` or `breaks`, because this
session has no evidence for any of them.

## 2026-11-27: V1, V2 and V3 — Native's WebKit has no camera source and no MediaRecorder

Same build (1.6.2.261124, serial 2026112422, Official), **native engine** (`rudolf-native`: one GTK 3 process, the apps
in WebKitGTK 4.1 windows). The owner ran the items and photographed the screen, because the session could not reach the
machine for most of the run; the session's own probes are the devlink commands **`000649`** and **`000650`** (read-only)
and the code in this repository. **Chromium on the same machine, minutes apart, is the reference**: every item below
works there.

### V1 — Voice Recorder: fail

- The checklist calls it **Sound Recorder**; this build registers **Voice Recorder** (`regApp('recorder',…)`,
  `desktop/apps25b.js`). A Start search for "sound" returns only Settings › Sound ("system volume, desktop sounds, output
device") and some loose files; "voice" finds the app.
- **Record does nothing.** The only feedback is the desktop's UI click sound: no 0:00 clock, no clip, no notification.
  Mouse click and SPACE with the button focused (the focus style applies) behave the same.
- Why, from the code: `recorderApp()` awaits `getUserMedia({audio:true})` inside a `try`, then runs
  `new AudioContext()` → `createMediaStreamSource` → `createAnalyser` → **`new MediaRecorder(st)`** → `rec.start()`, and
  only then `t0=Date.now()`, the button's text and the `setInterval` that draws the clock. Everything after the `try` is
  outside any `catch`, so one throw there leaves exactly what was seen: no clock, no button change, no error message.
- **Corrected by `000651`, and the correction is part of the finding:** MediaRecorder *is* in this WebKit — the library
  carries `JSMediaRecorder`, `MediaRecorderPrivateGStreamer`, `MediaRecorderPrivateBackend` and `MediaRecorderEnabled`,
  so it is compiled in with a GStreamer backend. This session first read it as absent from a count of
  `WebKitMediaRecorder`, which is the Cocoa-era name; the `JS…` and `Private…` names are the ones that matter. So the
  throw is **not** a missing constructor, and where it happens is not established: everything from `new AudioContext()`
  to `rec.start()` is uncaught, so any of it would look exactly like this from outside. What the machine does say is that
  the audio track handed to the app carries only zeros (the check's −120 dB), so even a recorder that started would
  record silence. A `try`/`catch` around that block, showing the error in the window, is the smallest change that would
  name the cause instead of leaving a dead button.

### V2 — Camera: fail (native), and the "Allow access?" prompt never appears on Native

- Native: Camera opens, then its body is replaced by **"No camera is available or access is blocked."** — the `catch`
  of `getUserMedia({video:true})` in `cameraApp()`. The Settings › Privacy camera switch is **on**, so this is the "no
  camera" half of that sentence.
- Why: the image carries no GStreamer video-capture plug-in. `000650` lists the 39 plug-ins in
  `/usr/lib/x86_64-linux-gnu/gstreamer-1.0/` — `pulseaudio`, `autodetect`, `audioconvert` among them — and **no
  `libgstvideo4linux`**. `addwebkit.sh`'s copy list names `alsa` and `pulseaudio` and never `video4linux` — and, as **N35**
  found on 2026-11-27, this WebKit would never ask for one: it captures through PipeWire, which the image lacks entirely.
- The camera itself is there: `/dev/video0` and `/dev/video1`, `uvcvideo` bound to "USB2.0 HD UVC WebCam (3277:0034)"
  (`000649`).
- **No prompt is possible on Native**: the engine grants every permission request silently — `on_perm` calls
  `webkit_permission_request_allow` (`iso/sys/rudolf-native.c:576`), with `enable-media-stream` TRUE. In Chromium the
  prompt belongs to the privacy checkbox being **off**: with it on, the preview starts at once (V2's observed run).
- Chromium, same build and setting: `camera pass (640×480)` in Check this computer, step 8 of 11, and the app's first
  open shows a live preview.

### V3 — a photo and a 5 s video: fail (native), pass in Chromium

- Native: cannot start the camera (V2), so neither a photo nor a video.
- Chromium: both are saved and open — `C:\USERS\JOHN\PICTURES\PHOTO-2026-11-27_01.27.30.JPG` (23.6 KB) and
  `C:\USERS\JOHN\VIDEOS\VIDEO-2026-11-27_01.30.24.WEBM` (447.7 KB), in Pictures and Videos as the checklist expects.
  Evidence: `V3-pictures.jpg`, `V3-videos.jpg`.
- Two differences on their own: the **video's** "Video saved" notification does not open the file (the photo's does;
  both open from File Explorer), and the names and the Modified column read 11/27/2026 01:27 while the clock read
  2026/11/26 19:32 — files are named and dated in UTC, the clock shows local time.

### Reference run: the same check under Chromium, 2026-11-27

All eleven steps pass: Sound works · **Microphone works (−4 dB)** · Network works (wifi 27 networks, 12 Mb/s down) ·
USB works (57.3 GB) · Graphics works (59 fps) · Keyboard works (76 keys) · Touchpad or mouse works · **Camera works
(640×480)** · Battery works (health 93 %) · Screen works · Sleep works (woke after 9 s, s2idle).

The −4 dB is on the **default** source, and that retires an earlier reading of this session: Native's −120 dB is not a
wrong default source, it is Native. Compared with the saved report's check of 2026-11-17 (which names no engine): camera
identical (640×480), battery 93 %, sleep 9 s s2idle, keyboard 76 keys; graphics 61→59 fps and wifi 43→27 networks,
22→12 Mb/s (environment); microphone `fail (no microphone)` → `works (−4 dB)`; USB `skip` → `57.3 GB`.

### N33 — On Native, WebKit has no media capture to give: the microphone reads −120 dB and the camera is not there

| client | engine | result |
|---|---|---|
| Voice Recorder | native | inert: Record gives the click sound, no clock, no clip (V1) |
| Voice Recorder | Chromium | records and plays back |
| Camera | native | "No camera is available or access is blocked" (V2) |
| Camera | Chromium | live preview, photo and video saved |
| Check this computer › Microphone | native | "Hardly anything was heard (loudest −120 dB)" |
| Check this computer › Microphone | Chromium | works, −4 dB |
| Check this computer › Camera | native | never reached (N34) |
| Check this computer › Camera | Chromium | works, 640×480 |
| Settings › Sound › Microphones › Test | native | no level |

−120 dB is the check's own floor (`peak > 0 ? 20*Math.log10(peak) : -120`, `desktop/hwcheck30.js`), so Native hands
back all-zero samples: the audio stream resolves and never carries sound, while the video request is refused outright.
Chromium on the same machine does both. Two faults in Native's media path:

1. **No video capture source**: `libgstvideo4linux` is not in the image — the complete plug-in list from `000651` has
   `pulseaudio` as its only capture source, and `addwebkit.sh`'s copy list names `alsa` and `pulseaudio` but never
   `video4linux`. **Corrected 2026-11-27 (N35): adding it is not the answer.** This WebKit has no v4l2 capture path at all;
   it captures through PipeWire only, and the image carries none.
2. **Audio that reads all zeros, and an app that stops without a word**: Native captures through `pulsesrc` (the only
   capture plug-in there) while Chromium on the same machine reads −4 dB; why the two differ is open. And Voice
   Recorder's start block is uncaught, so its failure is invisible (V1) — wrapping it would name the cause in the window.

Not the hardware: `/dev/video0` is claimed by `uvcvideo`, the ALSA cards expose `ALC897 Analog` (capture 1) and
`DMIC capture dmic-hifi-0`, and both work in Chromium. Not the privacy switch: it is on, and Chromium uses it.

**Both faults are in `main` too** (`745fcf18`), so this is not a 1.6.2-only matter: `iso/systools/addwebkit.sh`'s copy
list carried no capture plug-in at all (1.6.3 added `video4linux2` on 2026-11-27 and took it out the same day, N35), and
`desktop/apps25b.js` matches `main` byte for byte at the recorder's start block.

Open here, and **answered by N35**: whether Native's silent audio and its missing camera share one cause. **They do not.**
The camera is refused because the PipeWire device provider is missing — its log says `0 devices` — while the microphone is
whose WebAudio path is stopped by a missing GStreamer element of its own (`deinterleave`, N35 point 3): PulseAudio and the
DMIC itself are fine.
So a build with `video4linux` added would have proved nothing, and putting PipeWire in the image will not bring the
microphone back. Also open: does `new MediaRecorder` on
Native throw, or construct and produce nothing? `MediaRecorderPrivateGStreamer` is present (`000651`), so both are
possible, and the answer needs the console or the `try`/`catch` above.

### N34 — Check this computer ends itself at step 3 of 11 on Native

On Native the check runs steps 1 (Sound) and 2 (Microphone, −120 dB), then **closes itself a few seconds into step 3
(Network)** — so steps 4–11, Camera at 8 included, never run and no `Hardware check:` line is produced. Under Chromium the
same check completes all eleven steps minutes later, so this is the engine, not the Wi-Fi or the update server.

Also worth recording: the report's `Hardware check:` line names no engine, which is what made the saved report's
2026-11-17 `camera pass (640×480)` and `microphone fail (no microphone)` look like Native results when they match
Chromium exactly. The line should say which engine produced it.

### N35 — The capture path is PipeWire-only, and the image has no PipeWire (2026-11-27, devlink 000652–000664)

Asked after #716 had merged: *before a build proves it, find which capture source this WebKit actually asks for.* Two
read-only devlink commands say it plainly.

| probe (`sh`, read-only) | result |
|---|---|
| `strings libwebkit2gtk-4.1.so.0 \| grep -c -Fx pipewiresrc` | **1** |
| `… pipewiredeviceprovider` | **1** |
| `… v4l2src` | **0** |
| `… v4l2deviceprovider` | **0** |
| symbols and file names | `PipeWireCaptureDeviceManager.cpp`, `OpenPipeWireRemote`, `WebKit PipeWire Capture Device Manager`, `webkitpipewirecapturedevicemanager` |
| a message inside the library | "PipeWire Device Provider is missing or too old. Please install PipeWire >= 0.3.64." |
| `/usr/bin/pipewire`, `/usr/bin/wireplumber` | absent |
| `libpipewire-0.3` anywhere under `/usr/lib` | absent |
| GStreamer plug-in files matching `pipe` or `v4l` | none |
| the sound server in use | `pulseaudio` |
| `/dev/video*` | `video0`, `video1` |

So Ubuntu 24.04's WebKitGTK 2.52, as this image carries it, has **no v4l2 capture path at all**: it enumerates and captures
through PipeWire, and says so in its own error message. Three consequences:

1. **`video4linux2` (PR #716) is inert.** `libgstvideo4linux2.so` is now in the image and nothing will ever request
   `v4l2src`. The copy is harmless; the claim that it fixed the camera was not, and the line went out of the CHANGELOG
   (1.6.3 is unreleased, so there was nothing to withdraw in public).
2. **A plug-in alone would not be enough either.** The capture path needs the `pipewire` GStreamer plug-in **and**
   `libpipewire-0.3` (≥ 0.3.64) **and** a running PipeWire with WirePlumber, which is what publishes the v4l2 camera as a
   node. The image has none of the three. docs/plan-1.6.3.md Part 9 is that work.
3. **The silent microphone is a different fault again — and not a device one.** This finding guessed wrong twice before the
   log's own first line was read. What follows is what the machine shows (`000652`, `000658`–`000664`), and — said so
   where it appears — what it only suggests.

   - **The source is `pulsesrc`.** WebKit never names an audio element itself: `pulsesrc`, `alsasrc` and `autoaudiosrc`
     appear nowhere in `libwebkit2gtk-4.1.so.0`. It takes a device from a `GstDeviceMonitor` filtered to `Audio/Source` with
     `audio/x-raw`, and `gst_device_create_element()` builds whatever that device's provider offers. In the image the only
     audio provider is PulseAudio's — `libgstpulseaudio.so` carries `pulsesrc` and `pulsedeviceprovider` — and
     **`libgstalsa.so` is not in the image at all** (39 plug-ins: no ALSA, no OSS, no PipeWire). So Native's microphone is
     `pulsesrc` into PulseAudio 16.1 (`pulseaudio --daemonize=yes -n -F /run/rudolf/pulse.pa`, as `rudolf`), whose
     `module-alsa-card device_id=2 name=card2` opens the DMIC — card 2 `acp6x`, PCM `DMIC capture dmic-hifi-0`, S32_LE,
     2 channels, 48 kHz — which PulseAudio holds and reads (`RUNNING`, its ring buffer advancing). Device and server are
     both fine, which is why Chromium reads −4 dB through the same PulseAudio.
   - **What stops it is a GStreamer element that is not in the image: `deinterleave`.** Each of the four attempts in
     `starts/3` begins, 65 ms before anything else:
     `WARN GST_ELEMENT_FACTORY gstelementfactory.c:765:gst_element_factory_make_valist: no such element factory
     "deinterleave"!` and `GStreamer element deinterleave not found. Please install it`, then
     `g_object_set: assertion 'G_IS_OBJECT (object)' failed` and
     `gst_element_link_pads_full: assertion 'GST_IS_ELEMENT (dest)' failed`. WebKit's `AudioSourceProviderGStreamer` creates
     `deinterleave` to split the capture into WebAudio's channels; it gets NULL, links NULL, and the
     `WebAudioProvider_MediaStreamTrack_*` pipeline reports `<capture-audiosrcN> … streaming stopped, reason not-linked
     (-1)`. **What was seen failing is the WebAudio path**, and only it: the check's level meter reads the microphone
     through an `AudioContext` (`hwcheck30.js`'s `hwcMicPeak`) and sat at −120 dB through all four attempts
     (0:01:54–0:03:00). That the **Recorder** fails for the same reason is **inference**: `MediaRecorder` builds its own
     pipeline, that pipeline is nowhere in this log, and only a build with the plug-in in it can show what the Recorder
     does.
   - **`deinterleave` ships in `libgstinterleave.so`** (gst-plugins-good's `interleave` plug-in), and the image has
     `libgstinterleave.so: absent` while `libgstinter.so` (the video interleaver) and `libgstaudiofx.so` are there.
     `iso/systools/addwebkit.sh`'s loop names `deinterlace`, `inter` and `audiofx` and **never `interleave`** — and the loop
     skips a plug-in it cannot find without a word. So this is the same shape as the `pipewire` omission: a name missing
     from the list, not a build that lost it. `interleave` is now in the list, with the same kind of warning after it.
   - **`error reading data -1` is the companion — by inference, not by record.** The timings are fact: it arrives 6 s after
     each `not-linked` (0:02:00, 0:02:08, 0:02:32, 0:03:00) and at no other moment, and PulseAudio's own log
     (`--log-target=file:/run/rudolf/pulse.log`) records no stream failure at all. What it *is* stays inference:
     `pulsesrc`'s ring-buffer thread reading into a pipeline that is being torn down. The line names no element behind the
     `-1`, and nothing in this session ran a capture outside the failing path to compare it with.

Nothing here was changed on the laptop: all ten commands were read-only and ran with `approved: not asked (Ask first was
off)`. The v4l2 entry came out of `addwebkit.sh` and `pipewire` went in its place, with a warning when the build host has
no `libgstpipewire.so` — the loop skips a missing plug-in without a word, which is how a release shipped with no capture
source at all. Seen in the same logs, and not this session's business: `Could not open log file
'+/run/rudolf/webkit-video.log'` — a stray `+` in the path the desktop hands to WebKit.

## V58: the restart without the stick

**Passed.** What was done, in order:

1. `000585`, read-only, before anything changed: `BootOrder` = **`0006, 0002, 0001, 0000`**; entries `Boot0000 Windows
   Boot Manager`, `Boot0001 Drive`, `Boot0002 Ubuntu`, `Boot0006 Rudolf OS`; drive `KINGSTON OM8PGP4128P-AB`
   (`nvme0n1p2`, `nvme0n1p3`); **no `efibootmgr` binary** on the build, so the entries were read from the raw variables.
2. `000586`, read-only: no `/dev/sd*`, so **no stick was attached**; `/live/media` is `/dev/loop0` over **`/dev/nvme0n1`**.
   The two drive entries disagree with each other: **`Rudolf OS` (0006) names LBA `0x5AC4`, size `0x36000`** — the current
   file location — while **`Drive` (0001) still names `0x52EC`, `0x1E000`**, the 1.4.1 location the checklist describes.
   `Rudolf OS` is first in `BootOrder`, which is why this start worked.
3. `000587`, `{"kind":"restart"}`: `status 0`, `approved allowed on the computer`, result written 23:34:07Z.
4. The desktop came back: `000588` at 23:35:15Z showed `uptime` 54 s, the same kernel and build, `rudolf.slot=b`, and
   `000589`'s screenshot (`V58-native.png`) shows the desktop, its taskbar (`Rudolf OS 1.6.2`, 65 % battery, 17:35
   local) and the red link banner — over an Allow dialog for the screenshot itself, which is part of the picture.

What this does **not** show: that a *cold* start works (this was a warm restart from a running system), that the
firmware's setup screen is never reached (one restart is one sample), or that `Drive` (0001) is harmless — it is stale
and, being third in the order, untested. V60–V63 still need the owner.

### N1, N2 and N3: where they stand

`000590` found `/run/rudolf/desktop-engine` = **`chromium`**, i.e. Standard, where N1–N3 prove nothing. The owner
switched Settings › System health › Desktop engine to **Native** and restarted; `000593` at 23:43Z confirmed
`desktop-engine` = `native` and `rudolf-native --draw=gpu --size=1920x1080` — the real graphics path, which is what N1
is for.

- **N1 matches.** Two full-screen frames (`000598`, `000599`) differ in 61 % of sampled cells, with 1,059 and 1,091
distinct colours and no taskbar in the bottom band, which is where the taskbar sits in the baseline shot. The old
failure — a black screen with only WebKit's control bar — does not reproduce. The owner's own observation agrees and adds
that **ESC returns the player to its previous display setting**. The three earlier frames (`N1-windowed-1..3.png`) are
the player *before* full screen was chosen: the taskbar is visible in them, which is how that was told apart.
- **N2 matches, on the third and clean attempt.** With one Notepad open, Freebuff minimised and Ctrl+O pressed once,
  Native raised exactly the card `desktop/filedlg59.js` builds (`000644`-`000646`, identical across 15 s): titled
  **Open**, centred, path bar `C:\USERS\JOHN\DOCUMENTS`, a list holding `REPORT 202511180011.TXT` and `WELCOME.TXT`,
  **File name**, **Type "Text (.txt)"**, and **Cancel** and **Open**. The desktop behind it is dimmed as Standard's is:
  mean luminance falls from 37.4 to 24.4 over the icons and from 47.7 to 36.2 over the wallpaper, against a dialog-free
  frame. Evidence: `N2-native.png`.

- **The blank windows are real and unexplained, and this session first read them as N2.** Two clicks (`000620`,
  `000624`) raised a **window titled "Rudolf OS", about 1345 × 827, in its own frame, with a pure white body** (98.4 %
  near-white, **3 distinct colours** in a 1,180 × 700 crop), once as two stacked windows, and the owner was closing them
  each time. That is N4's shape, and on that evidence N2 was written up as `breaks` — wrongly, as the clean attempt
  showed. It is kept as an intermittent symptom: `N2-anomaly-blank-window.png`, `N2-anomaly-two-blank-windows.png`. What
  preceded it — a page still loading, twelve Notepad windows and 1.1 GB of swap, or Open pressed while Freebuff held the
  focus — is not known, and nothing should be fixed against it until it recurs.

- **Why the earlier attempts saw nothing at all**: the first two captures had Freebuff's window over most of the screen,
  where a centred dialog is hidden; the third (`000603`) caught the desktop with Notepad at the top left and no dialog,
  and a 12-frame burst (`000605`-`000616`) was identical in every frame with **no Notepad window** at all
  (`N2-attempt-desktop-no-notepad.png`) — the app had been closed, so nothing was clicked.
- **N3 matches.** `000647` caught the state with Notepad (`Untitled - Notepad`), Settings (on Display Properties) and Task
  Manager all open. The Apps list had a row per window — `Task Manager`, `Notepad`, `Settings` — with **Switch to** and
  **End task** on each, and below it the "Apps and programs running" list of background Linux programs, each with End and
  Force quit. The 2026-11-15 failure (only Task Manager itself, at 0.0 MB) does not reproduce. Evidence:
  `N3-native.png`.

  One thing worth noting without calling it a finding: the three rows' Memory column read **0.0 MB, 0.0 MB and 0.5 MB**
  while the machine had Settings and Notepad fully drawn. The row values come from somewhere other than real RSS, which
  is what V42 will need to watch — but V42 is about the total at idle, and it was not measured.

**Memory while this happened** (`000643`): total **3,416 MB**, used **2,279 MB**, available **694 MB**, and **1,139 MB of
swap in use**. The owner reached that limit by opening **twelve Notepad windows**, not twelve dialog windows (the
taskbar's row of `Untit…` buttons in `000624` was those Notepads). So this figure is what twelve Native app windows cost
on a 3.4 GB machine — a datapoint for V42 and N19 — and **not** a measurement of the dialog. Nothing here says one
dialog window is expensive; only two of the blank ones were ever on screen at once.

The lesson for the next attempt: keep the app open, minimise Freebuff (not close — it is the session's host), take the
burst only while the app window is known to be on screen, press Open **once**, and then **read the result before writing
it down**. This session's first reading of N2 was wrong by a wide margin — a blank window seen twice was taken for what
Open does, when a single clean attempt showed the dialog working. An intermittent symptom is not a finding until it
repeats under the same conditions. The session cannot click — that build has no `xdotool`, `wmctrl`, `ydotool` or
`import`, and `DISPLAY` is empty in its `sh` — so every step is the owner's hands and the session's screenshots.

### Reading a screenshot without an image library

The session's own host has no `magick`, `convert`, `ffmpeg` or Python imaging library, so two small stdlib scripts did
the work: one decoded the PNGs to measure near-black share, colour variety and per-frame change, the other wrote a
downscaled copy (about 900 px wide, which is small enough to look at) and an ASCII luminance map. That is how "black
screen" was told from "the picture, with the taskbar still there" — a full-screen video must show no taskbar. Worth
keeping in the next session's notes: `docs/native-findings-2026-11-26/` keeps the full-size PNGs, and a downscale is
enough to judge them.

## What this session did not do

- No window was opened, no key was pressed, no item performed by the session itself. V58's restart was the one change,
  and it was the item.
- Everything except V58, N1, N2 and N3 is unrun (see the table above). For N2 the session saw
  the dialog but never clicked in it: whether choosing `WELCOME.TXT` and pressing Open actually opens the file was not
  tried, so "matches" covers the dialog's appearance, its controls and the dimmed desktop — not a file being opened.
- No `put`, no firmware operation, no update.
- **No fix was made and no code was changed** — this document only. So there is no `deadcheck` run and no page rebuilt
  (`AGENTS.md`'s "Before every PR" applies to code and to the desktop page, not to a findings document).
- Not checked: anything audible, anything on a second screen, printing on paper, a phone, a gamepad, a printer, an
  optical drive (the machine has none), and the 30-minute mixed-use pass.

## Where to start next time

1. **The rest of the N items on this build.** N1, N2 and N3 all now match, so nothing found today stops a person working
   and the checklist's own order applies again: V58-V63 are done or waiting on you, and the V items are next.
   V1–V3 are now done (2026-11-27, below), so **V4 — the in-desktop call screen — is next**, then V5 (deny the
   microphone) once Native has a capture path to deny.
2. **Watch for the blank "Rudolf OS" windows.** They appeared twice after Open clicks and not on a clean attempt. If they
   recur, note what came before: a page still loading, memory pressure, a second window, or Open pressed in a window that
   did not have the focus. That is the thread worth pulling — and until they recur, nothing should be changed for them.
3. **V60–V63** with the owner: the entries above are the read-only half of V60, and V61–V63 need the Secure Boot and
   start-up pages. Never the firmware's Key Management, F9, or anything that clears keys. V59 stays available as the
   repair for a start that lands in the setup screen.
4. Re-read the checklist against **1.6.2.261124** first: it was written for 1.6.0, and `314e319b` (Native's
   transitions, 2026-11-25) is newer than the installed build, so V47 and V66 must not be judged against `main`.
5. Take the screenshot **before** leaving each item, and name it `V##-native.png`. Minimise Freebuff first: a capture with
   it maximised sees only Freebuff, which is what hid N2 for three attempts.
