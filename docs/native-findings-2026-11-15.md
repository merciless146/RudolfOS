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
