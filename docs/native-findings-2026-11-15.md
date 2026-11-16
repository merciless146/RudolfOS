# Native vs Standard: test findings (2026-11-15)

An unattended test session compared the native desktop (rudolf-native, with the apps in one-app WebKitGTK windows) with Standard (the desktop page in Chromium), following the owner's rule in docs/plan-1.6.0.md Part 12: "Rudolf OS on the native desktop should appear and function exactly as it does in Chromium mode." This report only records what was found. No code was changed, and fixes go through the session converting the native desktop (Part 12, items 9-12).

## Summary

- **19 findings:** 3 break something, 9 look wrong and 7 are minor. 17 are visual and none are audible: the sound matched on every file tested. The other 2 are about the log (Log video problems) and memory.
- **The five worst:**
  1. **N1:** Full-screen video is black on Native. The video keeps playing and only WebKit's control bar shows; Standard shows the picture.
  2. **N2:** The file dialog (Notepad › Open) is drawn inside the app's own window and cut off. In Notepad's window the path bar and the Cancel/Open buttons are not visible, so a file cannot be opened.
  3. **N3:** Task Manager on Native lists only its own window. Standard lists every open window, with Switch to and End task.
  4. **N4:** The page's dialogs open in a window or panel of their own: on Native, a 1344×827 "Rudolf OS" window or the app's window with a grey backdrop. This covers the camera's "Allow access?", Report a problem, a game's "needs a download" and "Close anyway?". On Standard each is a centred dialog over the dimmed desktop.
  5. **N5:** Every slider (`input type=range`) is GTK-drawn on Native: a thin white line with a large white knob instead of Chromium's blue filled track. This shows in the Media Player, Settings › Volume Controls, Mouse and touchpad, and WordPro's zoom.
- **What matched:**
  - The repo's own harness passes on this host:
    - `same_t.sh` 117/117
    - flybeh 23/23, keybeh 21/21, qsbeh 15/15, savbeh 7/7, tbbeh 10/10, toastbeh 9/9, wgbeh 12/12, wsbeh 11/11
    - `oneapp59_t.js` 42/42
    - `native59_t.sh` 69/70 (see N19)
  - Sound: 440 Hz, the same level within 0.5 %, and Native starting 30-60 ms later. This held for WebM/Opus, MP4/AAC, Ogg, MP3 and WAV.
  - Video playback was smooth on both engines: 16 of 16 screenshots taken 0.2 s apart were different frames. The same size, and colours within 5/255.
  - Also the same on both:
    - moving and resizing a window by its title bar and corner
    - the text right-click menu (Notepad)
    - Settings' 21 sections apart from the rows below
    - the windows of Calendar, Contacts, Weather, Characters, Event Viewer, PDF Viewer, Image Viewer, Code Editor, Archive Manager, App Store, Board Games, Card Games, Emu, Trash and Oracle (the WebKit stand-in), each under 1.3 % of pixels differing

## How it was tested

- **Host:**
  - Xvfb at 1920×1080
  - Playwright's Chromium (headless shell 1243), run with Standard's session flags `--autoplay-policy=no-user-gesture-required --use-fake-ui-for-media-stream`, sound not muted
  - rudolf-native built from main (e7ac34a) against WebKitGTK 2.52.6 and GTK 3.24.41 (installed for this run), `--draw=soft`
  - GStreamer 1.24 good, base and libav plugins
  - PulseAudio with a null sink, recorded with `parec` and analysed with numpy
  - the page built with `build19.py`
- **The state:** both engines were served one state by a copy of `same_rd.py`, kept outside the repo. The copy added only a `/tm/` route for the test media and `memory/settings`, so the Log video problems switch could be turned on.
- **The test media:** made with ffmpeg: a 6 s testsrc2 picture with a 440 Hz tone (WebM VP9/Opus and MP4 H.264/AAC), and 5 s of 440 Hz as Ogg, MP3 and WAV.
- **How each comparison was made:**
  1. Each app was opened on Standard with `launch()`, and on Native through Notepad's page (`launch()` asks the shell, op "open").
  2. On Native, scripts ran in an app's page through `RUDOLF_NATIVE_TEST_EVAL`.
  3. Each window was screenshotted and compared with `iso/qt/native/pxdiff.py` (tolerance 32, SHIFT 1).
- **Log video problems:**
  - It was turned on for both engines (`memory/settings?videoLog=1`).
  - Native was started as the session starts it with the switch on: `GST_DEBUG=*:2 GST_DEBUG_NO_COLOR=1 GST_DEBUG_FILE=…/webkit-video.log`, with its standard error kept as native-desktop.log.
  - Standard is left untouched by the switch (session: "Standard untouched"). To have something to compare, Chromium's own media log (`--enable-logging=stderr --vmodule=*/media/*=1`) was recorded for Standard as well.

## Findings

Severity is one of: breaks, looks wrong, minor. Pictures are in `docs/native-findings-2026-11-15/`. Each "-std-nat-diff" picture shows Standard, then Native, then the differing pixels in red.

| # | Area | Steps | Standard | Native | V/A | Severity | Log video problems (Native / Standard) | Pictures |
|---|---|---|---|---|---|---|---|---|
| N1 | Media Player, full screen | Open Media Player, add `t.webm` by URL, press ⛶ | The video fills the screen, with Chromium's control bar at the bottom | The window goes full screen (`full=1`) and `document.fullscreenElement` is set, but the screen is black: only WebKit's floating control bar shows, while `currentTime` keeps advancing. It is still black 1.2 s and 1.8 s later, and the same after the video ends. Leaving full screen restores the window. | V | breaks | Native: only `audiobasesink … correct clock skew` and `pulsesink … Got underflow` lines, nothing about the picture. Standard: nothing. | video-fullscreen-std-nat.png |
| N2 | File dialogs | Notepad › Open | The page's Open dialog is centred over the dimmed desktop, with the path bar, the list, File name, Type, Cancel and Open | The dialog is drawn inside Notepad's 520×326 window. The title, the path bar and the Cancel/Open buttons are cut off and cannot be reached: only part of the list, File name and Type show. | V | breaks | none / none | file-dialog-std-nat.png |
| N3 | Task Manager › Apps | Open Notepad, then Task Manager (Settings was also open on Standard) | One row per open window (Notepad, Settings, Task Manager), each with Switch to and End task | One row: Task Manager itself, at 0.0 MB. Windows of other apps are not listed, so they cannot be switched to or ended from here. | V | breaks | none / none | taskmgr-std-nat-diff.png |
| N4 | The page's dialogs (modal) | Open Camera (its "Allow access?"), Report a problem, Holiday Holdout or Fault Lines without their download, or close an edited Notepad | A centred dialog over the whole desktop, dimmed, with no taskbar button | A 1344×827 window titled "Rudolf OS", with its own taskbar button and the dialog on a grey background (camera, report, games). Notepad's "1 document has changes… Close anyway?" sits inside Notepad's own window over a grey backdrop. | V | looks wrong | none / none | camera-prompt-nat.png |
| N5 | Sliders (`input type=range`) | Media Player (seek bar, volume), Settings › Volume Controls (system volume, all 10 sounds), Mouse and touchpad (pointer speed, acceleration, double-click, scroll), WordPro (zoom) | Chromium's slider: a blue filled track and a small blue knob | A thin white or grey line with a large white round knob (GTK's). The rows are taller, so Volume Controls' list moves down a few pixels per row (7.5 % of the window differs). | V | looks wrong | none / none | settings-volume-std-nat-diff.png, mediaplayer-std-nat-diff.png |
| N6 | Drop-down lists (`select`) | WordLite (font, size), File Explorer (sort, view), WOS Galaxy Map ("Starts with"), Media Player (speed), Settings › Volume Controls (each sound's style), Mouse and touchpad (Primary button), the Print dialog | Chromium's chevron ⌄ at the right of each list | No chevron at all; the lists look like text boxes. The known item 8 difference is "the chevron but not Chromium's 20 px"; here the chevron is missing as well. | V | looks wrong | none / none | wordlite-std-nat-diff.png |
| N7 | File Explorer | Open File Explorer (C:\USERS\ANN) | The left pane ends with "FOLDERS ON THIS DEVICE"; the status bar says "6 item(s) · Storage: 148.0 KB of 2.0 GB used" | No "FOLDERS ON THIS DEVICE" section; the status bar says "Storage: 0 B used" | V | looks wrong | none / none | explorer-std-nat-diff.png |
| N8 | Settings › About | Settings › About | Graphics: the real renderer (ANGLE / SwiftShader on this host); Storage used 148.0 KB of 2.0 GB; a Chromium row | Graphics: "Apple GPU" (WebKit's masked renderer string), which is wrong on any Rudolf OS computer; Storage used 0 B; no Chromium row | V | looks wrong | none / none | settings-about-std-nat-diff.png |
| N9 | Clock › Alarms (`input type=time`) | Open Clock | "07:00 AM" with the clock-picker icon | "07:00": 24 hours, no AM/PM and no picker, although the account uses 12 hours | V | looks wrong | none / none | clock-std-nat-diff.png |
| N10 | Print dialog | WordPro › File › Print… | A centred dialog over the dimmed desktop; the number fields' arrows only show on hover; the lists have chevrons | Inside WordPro's window, the desktop not dimmed; the Pages and Copies fields always show WebKit's spin arrows; no chevrons (N6) | V | looks wrong | none / none | print-dialog-std-nat.png |
| N11 | Moving background: Glacier Lake | Display Properties: dynamic background Glacier Lake, dark mode | Northern lights (aurora58.js) move over the sky and the lake: 1.7 % of the screen changes in 3 s | A still picture: 0 % changes and there are no northern lights. rudolf-native draws only the picture (no aurora or statue code). | V | looks wrong | none / none | glacierlake-aurora-std-nat.png |
| N12 | Moving background: Spirit Falls | Dynamic background Spirit Falls | The stone statue on the right bank (statue58.js) | No statue | V | looks wrong | none / none | spiritfalls-statue-std-nat.png |
| N13 | Colour picker (`input type=color`) | WordLite, the text-colour box | A square swatch with a grey border | A white rounded pill with the colour inside (GTK's) | V | minor | none / none | wordlite-std-nat-diff.png |
| N14 | Title-bar icon | Open Flash Player (Ruffle's "not installed" window) | The window's own icon (purple) | The app list's ⚡ icon (orange) in the shell-drawn frame | V | minor | none / none | flash-titleicon-std-nat.png |
| N15 | Window opening animation | Open any app | `.win.wopen`: 0.18 s fade and scale from 96 % | The window appears at once: one-app CSS sets `animation:none`, and the shell has no compositor to fade it | V (animation) | minor | none / none | (none; timing) |
| N16 | Video statistics | Play `t.webm` or `t.mp4` for 7 s, then read `video.getVideoPlaybackQuality()` | totalVideoFrames 77 at 2.5 s, then 180 at the end (1-2 dropped) | 77 at 2.5 s and still 77 at the end (`t.mp4`: 76/76), although the screen shows new frames all the time. Not visible itself, but anything that reads it is wrong. | V (not shown) | minor | Native: nothing / Standard: nothing | (none) |
| N17 | Oracle without Chromium (host only) | Xbox Cloud Gaming or GeForce NOW opened from their pins | No console error | The page's WebKit browser logs "Error while parsing the 'sandbox' attribute: 'allow-presentation' is an invalid sandbox flag". Real Native uses Chromium for Oracle (12.3c), so this is the fallback only. | V (none shown) | minor | native-desktop.log: that line / none | (none) |
| N18 | Log video problems itself | Turn the switch on, restart, play the five files, full screen | Nothing is written. By design the switch covers "the Lite and Native desktops"; Chromium's own media log (recorded only for this test) had no playback messages. | 80 GStreamer lines for about 14 plays. For each file: "downloadbuffer … Failed to remove temporary file /var/tmp/WebKit-Media-XXXX: No such file or directory", "Got invalid stream-id from pad", and one "Got underflow" at each end. During full screen: "correct clock skew" bursts. None of these points at N1. The same action therefore gives log lines on Native and none on Standard, so the two cannot be compared through this switch; the temporary-file and underflow lines are noise in every report. | — | minor | (see the cell before) | (none) |
| N19 | Memory: one WebKit web process | `iso/qt/host/native59_t.sh` | — | 69/70. Test 21 ("both windows share one WebKit web process") fails: 2 web processes for two windows with WebKitGTK 2.52.6. Each app window may cost a process of its own (item 12's 120 MB). | — | minor | none | (none) |

### Known differences still there (from Part 12's "Known differences left")

These were seen during the run and are not counted above:

- the drop-down lists' padding (item 8), now also without chevrons: see N6
- the overview's and the toasts' pictures without a compositor (items 3 and 5); not looked at again
- `same_t.sh`: `ws2-dark`'s toast is timing-dependent. In this run all 117 parts passed, ws2 included.

### Seen during the test but not findings

These came from running both engines against one stand-in rudolfd:

- Both engines wrote the same storage, so an older app page on Native sometimes put back older settings, and Native's wallpaper switched between the picture and the plain colour. On a real computer only one engine runs.
- Leftover dialogs on Standard dimmed some of the batch screenshots (Fonts, Help, Terminal, Packages: 7-9 %). Seen again without them, they match.
- WordLite's Italic button looked pressed once on Native, but did not show again (the button states read the same).

## Not tested on this host: checks for the owner on the Vivobook

- N1 with real drawing (GPU, `--draw` other than soft): whether full-screen video is black there too.
- Sound through a real sound card and HDMI: delay, volume steps, the volume keys, Bluetooth.
- Oracle on Native with the real Chromium (`/run/rudolf/oracle-chrome`): its tabs, video, full screen, private windows. This host only had the page's WebKit stand-in, which is not the shipping Oracle.
- The Phone screen with a phone (scrcpy).
- Xbox Cloud Gaming and GeForce NOW.
- The emulators with games (the repo ships no ROMs).
- Quake and GPU Lab with a GPU.
- The camera and the microphone (Camera, Voice Recorder, calls).
- Touch, tablet mode and the on-screen keyboard (item 9), and accessibility (item 10).
- Several monitors (item 7 is waiting for the owner's decision).
- Memory at idle and with one app (item 12, N19).
- Printing to a real printer.
- Wine and Linux apps.

## What this host lacked

- No WebKitGTK headers, xdotool, faketime, PulseAudio or GStreamer tools at first. All were installed with apt for this run.
- No D-Bus session bus. WebKit warned once: "Unable to get session D-Bus address".
- No GPU (software drawing only), no sound card (a null sink) and no camera.

## Second run (2026-11-15, a later unattended session): further findings

A second scheduled session ran the same task in parallel, starting before the PR above was merged. It used
`same_t.sh`'s own machinery: a scratch copy of same_t.js, not committed, with these added cases:

- every app's window, and eight apps in dark mode
- all 18 Settings pages, in light mode and dark mode
- window handling, right-click menus inside apps, and the file and print dialogs
- the Media Player with generated WebM, MP4, Ogg and WAV media, recorded through a PulseAudio null sink
- the moving backgrounds, the screen savers and the sign-in screen
- night light and touch mode

Steps inside apps ran on Native through `RUDOLF_NATIVE_TEST_EVAL`, and clicks were real X input at Standard's
coordinates. Standard was run **without Playwright's `--hide-scrollbars`**, so it shows the scrollbars Rudolf's
Chromium shows.

It saw again, independently, findings N1 (black full-screen video), N2 (the file dialog cut off: also Save as), N4
(Report a problem in a "Rudolf OS" window; Print's Save as PDF inside Notepad's window), N5, N6, N7, N8, N11, N12 and
N16. The findings below are the ones the first run does not list.

| # | Area | Steps | Standard | Native | V/A | Severity | Log video problems (Native / Standard) | Pictures |
|---|---|---|---|---|---|---|---|---|
| N20 | Log video problems: the log is emptied | Switch on, play a video, then open any other app window | (Standard has no such log, N18) | Each app window's WebKitWebProcess opens `GST_DEBUG_FILE` itself, and GStreamer opens that file for writing (emptying it) unless its name starts with `+`. The session sets `GST_DEBUG_FILE=/run/rudolf/webkit-video.log` (no `+`), so every new app window empties the log. In this run it held 90 lines while the WebM played, and was 0 bytes at the end of the run. A video's warnings are gone as soon as another window opens, before anyone reads the report. | — | breaks (the switch loses what it is for) | (that is the finding) / none | (none) |
| N21 | Right-click menus inside apps | File Explorer: right-click a folder. Notepad: type, then right-click in the middle of the text. | The page's menu at the pointer. It may extend past the window's edge and keeps all its rows (Explorer's ends with Properties). | The menu is kept inside the app's window (its page's viewport). Explorer's moves up and to the right, and is cut off at the window's bottom: Properties and the rows above it are missing. Notepad's opens at the top of the window, not at the pointer, with a blue focus ring. (The first run saw Notepad's menu match: a click near the top of the text leaves room below.) | V | breaks (Explorer's last rows cannot be reached) | none / none | run2-explorer-right-click.png, run2-notepad-right-click.png |
| N22 | Scrollbars in the apps | Settings (any long page), File Explorer's left pane, WordPro (the page and Notey's bubble), Help, the start menu's list | Chromium's classic scrollbar: arrows and a grey thumb, dark on dark pages. It takes about 15 px of width. | None while not scrolling (GTK's overlay scrollbars). The content is wider, so lines wrap differently: "Errors & invalid input" and "Saves & notifications" in Volume Controls take one line, not two, and the rows below move. "Open websites as real browser tabs…" wraps differently in Settings › Oracle. WordPro's page is centred 8 px further right. | V | looks wrong | none / none | run2-scrollbars-wordpro.png, run2-settings-volume-sliders.png |
| N23 | The harness hides N22 | `iso/qt/native/same_t.js` | Playwright starts Chromium with `--hide-scrollbars` by default, so same_t.sh's Standard pictures have no scrollbars. Rudolf's Chromium shows them (no such flag in the session). | — | — | minor (the test cannot see N22: `chromium.launch({ignoreDefaultArgs: ['--hide-scrollbars']})` would make it see them) | — | (none) |
| N24 | Settings › Display Properties | Open Settings (it opens at Display Properties) | The background tiles at the page's height | Each picture tile is 4-5 px taller, so everything below moves down: 11.8 % of the window differs (10.2 % in dark mode, the same with another accent colour) | V | looks wrong | none / none | run2-settings-display-tiles.png |
| N25 | Text boxes the page does not style | Help's "Search help", Oracle's search box, the Media Player's URL box | A 1 px dark-grey border with square corners, and a grey placeholder | GTK's entry: a light grey, rounded border and a paler placeholder | V | looks wrong | none / none | run2-help-text-input.png |
| N26 | Check boxes | Settings (Volume, Mouse, System health, Display), light and dark | Chromium's check box; dark grey in dark mode | A different blue box and tick, 1-2 px higher; white boxes in dark mode | V | minor | none / none | run2-settings-volume-sliders.png, run2-dark-system-health.png |
| N27 | Title bars in dark mode | Any app with dark mode on (File Explorer, Settings, Task Manager, WordPro, Media Player, Prompt, App Store, Calendar) | A bold ✕; the frame's 1 px edge dark | A thinner ✕, and a light (white-ish) edge along the frame's top and right. 3.2-3.5 % of the title bar differs in every dark case (0.2-0.4 % in light mode). | V | looks wrong | none / none | run2-dark-title-bar.png (3× enlarged) |
| N28 | GPU Lab | GPU Lab › Graphics info | ANGLE (SwiftShader), Vendor Google, "Acceleration: Software rendering (no graphics card in use) — 3D will be slow" | Renderer "Apple GPU", Vendor "Apple Inc.", **"Acceleration: Hardware (graphics card)"**, although Native ran with `--draw=soft`. This is the same masked string as N8, but here it also tells the user the wrong thing about speed. | V | looks wrong | none / none | run2-gpu-lab-renderer.png |
| N29 | Video frame callbacks | Play test.webm and count `video.requestVideoFrameCallback` calls | 20, 55 and 90 calls after 1, 2 and 3 s | 0 calls: the callback never fires, although the video plays. `hwcheck30.js` counts a camera's frames this way, so the hardware check's camera frame rate would read 0 on Native. | — | looks wrong (the hardware check) | Native: the same GStreamer lines as any play (42-43 "invalid colorimetry, using default" / "Need to specify a color matrix when using YUV format (I420)" per video, 2-3 "Got underflow", 2 "Failed to remove temporary file") / none | (none) |
| N30 | Start menu's search box | Click Start | A grey border; a scrollbar beside the list | A blue focus ring round the search box; no scrollbar (N22) | V | minor | none / none | (none) |
| N31 | Oracle's WebKit stand-in (host only, as N17) | Open Oracle; press Ctrl+T | The page's search box has the focus; Ctrl+T leaves 1 tab | The address bar has the focus (blue ring); Ctrl+T opens a second tab | V | minor (Ctrl+T on Standard may be Chromium's own key, which Playwright does not pass to the page: a Vivobook check) | none / none | (none) |
| N32 | Desktop sounds from app windows | Media Player › URL… › Add, recorded from the null sink | A short 1.3 kHz UI sound before the tone | Not heard. The recording may have started late, so this is unconfirmed: a Vivobook check. | A | minor (unconfirmed) | none / none | (none) |

**Known differences, still there** (Part 12, items 9 and 10):

- **Night light:** Standard tints the whole screen (desktop, taskbar and windows). Native tints only the inside of
  the app's window. See run2-night-light.png.
- **Touch mode:** Standard's title bars get taller. Native's stay as they are and only the page inside changes. See
  run2-touch-mode.png.

**Second run's counts:** 13 findings not in the first run (2 break, 6 look wrong, 5 minor), plus the two known ones
above. With the first run's 19 that makes **32 findings: 5 break, 15 look wrong, 12 minor**.

**What matched in the second run:**

- Sound: WebM and MP4 at 440 Hz; Ogg and WAV at 880 Hz. Level -25.8 to -26.1 dBFS on both engines, at the same
  volume. On Native, under 2 % of the recording was anything other than the tone, with no gaps.
- In-window video: a frame paused at 2.0 s is the same picture on both engines.
- Window handling, each within 0.1-0.8 %: maximise, restore, moving, resizing by the corner, minimise and back from
  the taskbar, a title-bar double-click, typing and Ctrl+A.
- The screen savers: rudolf, rain, mystify, bubbles, aurora, fireworks, clock and stars.
- The sign-in screen with a wrong password (0.21 %), and in dark mode.
- 34 apps' first windows within 2 %. Only Settings (N24), WordPro (N22), GPU Lab (N28), Help (N22, N25) and WOS
  Galaxy Map (N6) were over.
- The same JavaScript answers on both engines for every Settings page.
- The harness:
  - `same_t.sh` 117/117
  - flybeh 23/23, keybeh 21/21, qsbeh 15/15, savbeh 7/7, tbbeh 10/10, toastbeh 9/9, wgbeh 12/12, wsbeh 11/11
  - `native59_t.sh` 70/70. Test 21 passed this time, so N19's failure comes and goes.
  - `wmnative59_t.sh` 3/3, `videolog60_t.sh` 9/9 and `oneapp59_t.js` 42/42.
  - `oracle_native_t.sh` skipped: the host has no `xclip` or `ip`.

**Not compared in the second run:** Sound Recorder and Camera. Playwright's Chromium hung on `getUserMedia` at launch
(also with fake devices). The Vivobook checks listed above stand.

## Status on 2026-11-16

An unattended session (2026-11-16) checked each finding against main (`d574670`, then `57babb5`), the open PRs and the
four tasks started at 20:10-20:25 (owner's time), on this host (Xvfb 1920×1080, WebKitGTK 2.52.6, `--draw=soft`, faketime) with
`same_t.sh`, its behaviour scripts and `native59_t.sh`, and on the newest beta (1.6.0.261116, serial 2026111602) in QEMU
(`iso/qt/sb/t160n_t.py`, workflow `native-areas.yml`). "Fixed here" means in this session's PR (see the table's last column).

| # | Finding | Status | Where | Checked on 2026-11-16 |
|---|---|---|---|---|
| N1 | Full-screen video black | fixed | #394 (`d363693`) | same_t `video-fs` passes; real drawing (GPU) is a Vivobook check |
| N2 | File dialog cut off inside the window | fixed | #394 (`d363693`) | `dlg-open` passes |
| N3 | Task Manager lists only itself | fixed | #394 (`d363693`) | `tm-apps` passes |
| N4 | Page dialogs in a window of their own | fixed | #394 (`d363693`) | `dlg-ask`, `dlg-camera`, `dlg-print`, `dlg-dark` pass |
| N5 | Sliders drawn by GTK | fixed | #394 (`6212d1f`) | `ctl-volume`, `ctl-mouse`, `ctl-media` pass |
| N6 | Drop-down lists without chevrons | fixed | #394 (`6212d1f`) | `ctl-wordlite` passes |
| N7 | File Explorer: Folders on this device, storage 0 B | fixed | #394 (`6212d1f`) | `explorer` passes |
| N8 | About: "Apple GPU" | fixed | #394 (`6212d1f`, `068aa95`) | `about` passes |
| N9 | 24-hour time field | fixed | #394 (`6212d1f`) | `ctl-clock` passes |
| N10 | Print dialog inside the window | fixed | #394 (`d363693`) | `dlg-print` passes |
| N11 | Glacier Lake without northern lights | fixed | #394 (`36507ee`) | `dyn-lake` passes |
| N12 | Spirit Falls without its statue | fixed | #394 (`36507ee`) | `dyn-oasis`, `dyn-oasis-dark` pass |
| N13 | Colour box drawn by GTK | fixed | #394 (`6212d1f`) | `ctl-wordlite` passes |
| N14 | Flash Player's own icon | fixed | #394 (`cb33ba5`) | `title-icon` passes |
| N15 | No window opening animation | **fixed here** | this session's PR (rudolf-native: `wopen_start`) | new same_t case `wopen`: Standard `.win.wopen` 0.18 s; Native's animation 183-190 ms, faded and at 96 % at its start. Fails on main (no animation) |
| N16 | Video statistics stop near the end | not reproduced | — | new same_t case `video-frames`: 180 frames at `ended` on both engines with main's page (Chromium 180), and 174-180 frame callbacks on each. The 153 of the first run is not seen with WebKitGTK 2.52.6 here; a Vivobook check |
| N17 | Sandbox flag error (WebKit stand-in) | fixed | #394 (`cb33ba5`) | `sandbox-flags` passes |
| N18 | Log video problems' noise | fixed in part | #394 (`d363693`) | downloadbuffer's line gone; "Got underflow" at each end stays (the owner's decision, plan Part 12) |
| N19 | Two WebKit web processes | fixed (the test) | #394 (`cb33ba5`) | `native59_t.sh` 70/70 with this session's binary, test 21 ("both windows share one WebKit web process") passes. Memory with real use is a Vivobook check |
| N20 | Log emptied by each new window | fixed | #405 (`b83e625`) | `videolog60_t.sh` checks the "+" |
| N21 | Right-click menus cut off | fixed | #405 (`b83e625`) | `app-menu` passes |
| N22 | No scrollbars | fixed | #405 (`b83e625`) | `help`, `ctl-*` pass |
| N23 | The harness hid scrollbars | fixed | #405 (`b83e625`) | same_t starts Chromium with its scrollbars |
| N24 | Display Properties' tiles 4-5 px taller | fixed | #405 (`b83e625`, with N22) | Settings' cases pass (`ctl-volume`, `about`) |
| N25 | Unstyled text boxes | fixed | #405 (`b83e625`) | `help` passes |
| N26 | Check boxes | fixed | #394 (`d363693`: Chromium's blue) | `ctl-*` pass |
| N27 | Title bars in dark mode | fixed; frames being finished | #405 (`b83e625`); task 20:20, PR #409 | `title-dark` passes |
| N28 | GPU Lab says "Hardware" | fixed | #394 (`068aa95`: the chip's name from GL) | new same_t case `gpulab`: "Software rendering (no graphics card in use) — 3D will be slow" on both |
| N29 | Video frame callbacks never called | fixed | #405 (`b83e625`) | new same_t case `video-frames`: 174-180 callbacks on both |
| N30 | Start menu's search box and scrollbar | fixed | #405 (`510c0a3`) | `menu`, `menu-left`, `menu-right` pass |
| N31 | Oracle's WebKit stand-in: focus, Ctrl+T | Vivobook check | — | real Native uses Chromium for Oracle; docs/native-vivobook-checklist.md |
| N32 | Desktop sounds from app windows | being fixed | task 20:10 ("no sound in native app windows") | a Vivobook check after that task's PR |
| — | The overview's pictures without a compositor (taken once when it opened) | **fixed here** | this session's PR (rudolf-native: `ov_grab`, `ov_live`) | new same_t case `overview-live` (a playing video under Super+Tab): 0.87-1.05 % of Standard's screen and 1.55 % of Native's change in 2 s; 0 % on main |
| — | The toasts' pictures without a compositor (what was behind when they came) | **fixed here** | this session's PR (rudolf-native: `comp_behind`, `toast_live`) | new same_t case `toast-live` (Notepad moved under a toast): 1.40 % differs; 20.9 % on main. Also fixed: a toast window left 15 px off when two toasts became one |
| — | Night light tints only the app windows | still open | plan Part 12, item 9 (its own PR series) | not changed here |
| — | Touch mode's title bars | still open | plan Part 12, item 10 | not changed here |

### The areas never compared: the beta in QEMU (2026-11-16)

`iso/qt/sb/t160n_t.py` (workflow `native-areas.yml` on branch native-areas-qemu, runs 43799708532, 43800749546, 43801599755, 43803406064)
starts the beta 1.6.0.261116 (serial 2026111602, which has every fix up to #405) at 1280×800. The machine has an Intel HD Audio
card (hda-duplex), two virtio-vga outputs and S3 sleep. It runs Standard first: an account is made and signed in, Settings'
pages are opened, the screen is locked and unlocked, and the machine sleeps and wakes. Then it starts Native on the same data
partition and does the same steps as a person would (QMP keyboard and pointer, at Standard's coordinates).

| Area | Standard | Native | Result |
|---|---|---|---|
| Engine switch, then sign-in (Ann, typed) | — | The shell's sign-in, then the desktop | same: 0.28 % of the screen differs |
| Desktop after signing in | icons, taskbar, tray | the same | same: 0.28 % |
| Lock (Super+L) and unlock with the password | lock screen, then desktop | the same | same: 0.16 % and 0.27 % |
| Second screen (virtio-vga output 2) | "disconnected" (DRM), nothing shown | the same | same (item 7 waits for the owner) |
| Sound card | `HDA-Intel` in ALSA; Volume Controls lists it (Built-in Audio Analog Stereo) | the same card | same (system view) |
| Printers | no CUPS in the release (`lpstat` missing); printing is the page's PDF and the USB printer extension | the same | same; printing on paper is a Vivobook check (V20) |
| Batteries | none in QEMU | none | same; V13-V17 |
| Sleep (S3) and waking (`system_wakeup`) | the page answers after waking; the display stays "not active" in QEMU (virtio-vga), even after input | rudolf-native still running, not restarted; the display the same | same on both. The display not coming back may be QEMU's virtio-gpu; check on the Vivobook (V10, V11) |
| Settings › Volume Controls in dark mode (run 4: the pages compared in their first 520 px) | the lists' chevrons light (#e2e8f0) | **no chevrons**: the N6 chevron was drawn dark (#111) on the dark list | **fixed here**: `oneapp59.js`, a light chevron with `html.dark`; new same_t case `ctl-volume-dark` (the host's light-mode cases could not show it) |
| Settings › Bluetooth with no adapter (run 4) | "This computer has no Bluetooth adapter Rudolf OS can use. A USB Bluetooth adapter works too." | the same | same: 0.94 % of the window differs |
| Settings › Devices (run 4) | — | — | same: 3.9 % (the window's height, below) |
| **App window size at 1280×800** | Settings 862×642 at (40, 20) | **Settings about 87 px shorter** (the page sized its window inside its own first, smaller window, not the screen's desktop) | **fixed here**: `oneapp59.js` `oaScreenSize`; host test `SAME_W=1280 SAME_H=800 sh same_t.sh OUT ctl-volume,explorer,help`: 14.4 % and 3.6 % differed, now 0.77 %, 0.56 % and 1.34 % |

Not compared in QEMU, so they are on the checklist: the camera and microphone, calls, Bluetooth pairing, printing on paper, the
touchpad, Linux and Flatpak apps, Library games, the updater's download and restart, several accounts and switching user, touch
mode with the on-screen keyboard, and long mixed use. The release has no camera loopback or Bluetooth stack that QEMU could
feed, and no network in the test.

Seen on Standard in the QEMU log, not changed (Standard is the reference): after the session restarts the browser, Chromium was
given `--window-size=1794800598,800` (the watchdog's clock as the width). The session's comment on SCW says this was fixed for
Native and Lite only. It has no visible effect in kiosk full screen.

Not counted above, found on this host: the `menu`, `title-*`, `desk-menu` and `file-*` cases fail when `faketime` is missing
(Native then takes the real hour, so automatic dark mode can differ from Standard's fixed 09:41); with faketime installed
they pass. `same_t.sh` should say SKIP rather than compare then (left as it is).
