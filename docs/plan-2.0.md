# Rudolf OS 2.0: the apps become native (added on 2026-11-13)

Asked for on 2026-11-13: the whole of Rudolf OS as built so far converted to the native desktop. The shell first
(docs/plan-1.6.0.md Part 12: everything the web desktop's shell has, Native the default), then every app rewritten as a
native program, one at a time, in this plan. 2.0 is released when the apps in waves 1 to 3 are native.
**Its name: Rudolf Kryptonite** (the owner, 2026-11-20: "Rudolf's 2.0 version with the 7.3 kernel and Wayland will be
called Rudolf Kryptonite"): 2.0 on the newest kernel (7.3 and later) with Wayland. 1.6.x on the 6.18 LTS with X11 keeps
the name Rudolf OS.
2.0 also brings the phone's own build, on the hardware 1.6.1 proves ("The phone build", below).

**Two editions, one repository (the owner, 2026-11-22):** "The 6.18 kernel version with X11 is going to continue being
developed as the main version of the operating system", and "Rudolf Kryptonite is going to be a version of Rudolf
specifically built and optimized for gaming and mobile use. Rudolf as it currently is in 1.6.x is going to be the regular
home office version." So Kryptonite is an edition beside Rudolf OS, not its replacement:
- **Rudolf OS (1.6.x and on): the home and office edition, and the main one.** The 6.18 longterm kernel and X11, built
  for steadiness on everyday and older computers. It keeps the stable update channel, and the recovery system.
- **Rudolf Kryptonite: the gaming and mobile edition.** The newest stable kernel (7.3.x, then 7.4 and on: newer graphics
  drivers, handheld and controller drivers, HDR and variable refresh) with Wayland. It is tuned for games (game mode,
  the GPU drivers, Steam, Wine, emulators) and for phones and handhelds (the phone build, touch, battery). It has its
  own update channel ("next", below, becoming Kryptonite's channel), so neither edition ever receives the other's
  updates.
- **One repository, built from main.** Most of the code is shared (the desktop, Oracle, Blitzen, File Explorer,
  rudolfd, the security work), and Kryptonite's differences are additions: the kernel track, the Wayland session, and
  gaming and mobile tuning. They go into main behind build switches, and the release workflow builds each edition from
  the same tree. A fix made once reaches both, and the keys and their secrets stay in one place.
- **The `kryptonite` branch only for work that cannot share main yet** (today: the 7.3 kernel, while its kexec-handover
  crash at 1.5 GB in QEMU is understood). Each piece moves into main behind its switch as soon as it is sound, so the
  branch never drifts far from main. A separate repository only if Kryptonite ever stops sharing Rudolf's code.
- **Work on Rudolf OS's features continues on main as before.** Gaming and mobile work is marked for Kryptonite in its
  plan and PR, with the switch that keeps it out of the home and office edition where it does not belong there.

Why (1.5.9's measurements, docs/plan-1.5.9.md 12.3a): with nothing open Native uses 854 MB against Standard's 1,110 MB,
because no web page runs; each app opened today loads a WebKit page with the desktop's code (Settings: 195 MB). A native
app of the same kind is typically 15 to 40 MB.

## How a native app is built
- **C and GTK 3**, as rudolf-native: the system layer already has GTK 3, cairo, Pango and GStreamer (for WebKitGTK), so
  most apps need no new library. A library an app would add (a PDF renderer, a source-code view) is named in its wave
  with its size, and the base layer's budget is checked by the release build as for ModemManager.
- **One program per app** (`/usr/bin/rudolf-APP`), started by rudolf-native as any program, framed by its window
  manager. A small shared library, `librudolf-ui` (`iso/sys/ui/`), holds what every app needs and the page already has:
  the accent colour and its variants (acVariant, acInk: already ported to C in rudolf-native), light and dark, the
  fonts and sizes of Display Properties, the translations (the page's language files read as they are), dialogs (Open,
  Save, Confirm in Rudolf's look), the about box, and rudolfd's client (the token, HTTP, JSON: from rudolf-native).
- **Files:** a native app reads and writes the account's real home folder (opened at sign-in by rudolfd), the same
  files the web apps see, so a document opened in WordLite's web version and in its native one is one file. Paths are
  shown as the page shows them (C:\\Users\\NAME\\Documents).
- **Settings and an app's own state** go through rudolfd (`desktop/settings` and a new `apps/state?app=NAME`), never
  into the web page's storage directly, so the page (Standard and Lite, still there) and native apps stay in step
  while both exist.
- **The same look (the owner, 2026-11-14: Rudolf OS on Native appears and works exactly as on Standard):** a native
  app is drawn to look like its web version, not like a stock GTK app: librudolf-ui carries a GTK CSS theme made from
  the page's styles (its buttons, fields, menus, scrollbars, tabs, lists, the accent colour, light and dark), and each
  app's windows are screenshot-compared with the web version as in docs/plan-1.6.0.md Part 12's rule (under 2 % of
  pixels different, text anti-aliasing aside). An app is not switched over until it passes.
- **The same behaviour:** each app's native version passes a checklist made from its web version (its menus, its
  shortcuts, its files, its settings, what its browser tests check), screenshots compared side by side under Xvfb.
- **Until an app is native, its web version opens** in a one-app WebKit window as in 1.6.0. When the native version
  passes, the start menu, the pins, file associations and Open with point to it; its web version stays in the page for
  Standard and Lite until 2.0 decides their future (below).

## The order
Small and much used first, so the way of building is proved on easy apps before the large ones.

**Wave 1: small apps, proving librudolf-ui**
Clock (alarms, timers, world clock), Calculator, Notepad, Task Manager (from rudolfd's process list, as now),
Image Viewer, Screenshot, Characters & Emoji, Font Viewer, Trash.

**Wave 2: the everyday tools**
File Explorer (its drag and drop with the shell's desktop and windows), Media Player (GStreamer), PDF Viewer (with
printing, as 1.5.9's: a PDF renderer is the wave's one new library, poppler-glib, about 5 MB, budget checked), Archive
Manager, Command Prompt and Terminal (VTE if it fits the budget, else a terminal of our own on a pty), Code Editor,
Event Viewer, Calendar, Contacts, Weather, Voice Recorder, Camera, Help.

**Wave 3: the large apps**
Settings (every page, Search in Settings with its index, Display Properties' previews drawn natively), WordLite,
WordPro, SpreadsheetPro, PresentPro, Paint, App Store and Packages, Developer Console, Blitzen, Phone screen.
Settings goes first in this wave: it is opened most, and its page costs the most memory today.

**Staying web pages (decided per app, not rewritten by default)**
- **Oracle Browser:** a browser is a web engine; it stays Chromium (Standard) or WebKit, framed by the native window
  manager, its tabs and menus already its own.
- **The games and creative tools built on the web:** Board Games, Card Games, Emu, Flash Player, Quake Engine,
  GPU Lab, WOS Galaxy Map, BlockLua Builder, and the screen savers: they draw with canvas and WebGL, a native rewrite
  gains little, and they open in one-app windows. Each is reconsidered after wave 3 with its memory measured.

## Each app's pull request
One app per pull request (a large app may take several, each one working), based on main, the owner merging: the
program, its host test under Xvfb (`iso/qt/native/APP_t.sh`: opens, its checklist, screenshots), the start menu and
associations switched once it passes, a CHANGELOG line, and its memory at idle with a file open measured against its
web version (the PR says both). The release build compiles every native app and fails if one is missing from the base
layer or over its budget.

## What 2.0 decides when wave 3 is done
- Whether Standard and Lite (the web desktop) stay in the system layer as fallbacks, or move to a recovery-only role,
  or go. Until then they stay, and Native gives up to Standard as in 1.5.9.
- Whether the web desktop is kept as the browser build (Rudolf OS in a browser tab) and the phone web app, or retired.
- Wayland (docs/plan-1.5.9.md 12.2: X11 for now): planned below ("Wayland", added on 2026-11-15); its switch for
  new installations is decided at its stage W5.

## Measurements that close 2.0
On the Vivobook, at idle and with the five most used apps open: memory used, start to the desktop, power on battery;
against 1.5.9's table. Target: idle under 700 MB used, five apps open under 1 GB.

## Wayland (added on 2026-11-15)
Asked for on 2026-11-15: "Plan the Wayland migration for 2.0", after comparing Wayland with X11 for Rudolf OS. The
recommendation then: X11 through 1.6.x (Native finished, several monitors with RandR as docs/plan-1.6.1.md Part 1
says, cellular), Wayland in 2.0 with the native apps and the mobile build.

**Why:** apps of one account can no longer read each other's windows and keystrokes or send each other input (on X11
any client of the display can); tear-free drawing; per-display and fractional scaling; touch, gestures and the
on-screen keyboard as the phone and tablet need them (every Linux phone shell is Wayland); several monitors of
different sizes. Memory: about the same as X11 (the compositor replaces Xorg; Xwayland only while an X11 app runs), to
be measured (W0).

**What stays the same:** the owner's rule: Rudolf OS on Wayland appears and works exactly as on X11 (and as Standard),
checked the same way (same_t.sh's screenshots, the behaviour scripts). Accounts, files, rudolfd, updates, recovery and
the security model do not change. Standard, Lite and Native all run on it; X11 stays as a fallback session.

### What changes, part by part
- **The compositor: rudolf-native becomes it,** built on wlroots (C, as rudolf-native; small; used by Sway, labwc,
  Phosh's phoc). It takes over what Xorg, rudolf-display and the window manager do today: outputs (DRM/KMS), input
  (libinput), focus, the windows' frames and stacking, workspaces, full screen. The shell's own surfaces (taskbar,
  start menu, tray pop-ups, notifications, the lock screen, the screen saver) are layer-shell surfaces of GTK 3 with
  gtk-layer-shell, drawn as now, so they look the same.
- **rudolf-display's jobs move into the compositor:** keyboard and pointer, the brightness and volume keys, the
  idle count (ext-idle-notify), the screen turning off, and the emergency keys (Ctrl+Alt+Backspace held: the safe
  desktop), which must keep working when the shell hangs: a small watchdog outside the compositor reads them from
  the kernel's input as rudolf-display does today, and restarts the session.
- **The screen lock:** ext-session-lock, so a crashing shell can never leave the desktop unlocked.
- **Several monitors:** docs/plan-1.6.1.md Part 1's design (a workspace per display) carried over to wlroots' outputs;
  per-display scaling (125 %, 150 %) added.
- **Chromium (Standard's desktop, Oracle, its Tor windows):** Chromium on Wayland (--ozone-platform=wayland); the shell
  in Chromium (the Isolated Web App, real tabs) checked unchanged. **WebKit (Lite and the one-app windows):** WebKitGTK
  on Wayland.
- **X11 apps: Xwayland,** started when the first X11 app opens and stopped after the last: Wine programs, Steam and its
  games, older Linux apps. Each account gets its own Xwayland (its own display and authority), as each account has its
  own display access on X11 today (1.5.9 15.3).
- **Linux apps:** Flatpak apps get the account's Wayland socket (and Xwayland's when they ask for X11); Debian's
  programs the same through rudolf-debian. Each account its own Wayland socket, readable by its own Linux user only.
- **Screen capture: remote access, the devlink's screenshot, screen sharing in calls (Meet, Zoom), Screenshot:**
  through the compositor (wlr-screencopy, and the ScreenCast portal over PipeWire for apps). rudolf-portal answers
  ScreenCast and RemoteDesktop as it answers the other portals, asking the person as the desktop asks today; remote
  access's input goes through the virtual keyboard and pointer protocols (or libei).
- **Global shortcuts for apps:** the GlobalShortcuts portal (rudolf-portal).
- **Game mode, full screen, the controllers:** unchanged in rudolfd; the compositor's full screen and direct
  scan-out for games (lower latency on the Vivobook's GPU).
- **The on-screen keyboard and tablet mode:** input-method and virtual-keyboard protocols, so the keyboard types into
  every app, Xwayland's included.
- **Packages:** wlroots, Xwayland, gtk-layer-shell, xdg-desktop-portal's pieces Rudolf does not answer itself: their
  sizes printed in the release build.

### Tests
- The host tests that use Xvfb, xdotool and ImageMagick's import get Wayland equivalents: the compositor's headless
  backend (WLR_BACKENDS=headless) for a screen, wlrctl or the virtual input protocols for input, grim (screencopy) for
  screenshots. same_t.sh compares X11 and Wayland screens as it compares Native and Standard.
- QEMU (the t160 harness): a Wayland session starts, signs in, the account's apps get its Wayland socket and not
  another account's, Xwayland starts for a Wine program and stops after, the lock holds when the shell is killed.
- Each part's checks run on both sessions until X11 is retired.

### Stages
- **W0, measure and decide (before any code):** on the Vivobook, a wlroots compositor with the desktop in it against
  today's X11: memory at idle, with Oracle, with a Wine program (Xwayland), start to the desktop, battery; Chromium,
  WebKitGTK, Steam, Wine, Flatpak apps and the calls sites checked on Wayland; written into this plan with a
  go or no-go for W1.
  - **W0 (A), done 2026-11-22 on the Vivobook (1.6.1, kernel 6.18.64, Native on X11), with the owner present, over
    the devlink (commands 000555-000562):**
    - **Method:** Weston 14 was run nested in an X11 window (x11 backend, with Xwayland), inside the Debian sandbox,
      with Cage, Xwayland, Foot, Weston, wayland-utils, Chromium, Epiphany, the GTK 4 demos and the X11 test apps
      installed there (the owner allowed it; Rudolf itself was not changed).
    - **Every app tried ran on Wayland:**
      - Foot (Wayland-native)
      - GTK 4 (gtk4-demo)
      - WebKitGTK (Epiphany, the engine behind Lite and Native's windows)
      - Chromium with `--ozone-platform=wayland`
      - an X11 app (xeyes) and a Windows program (Wine's Notepad), both through Xwayland
    - **The owner looked and found it working fine.** The compositor offered wl_compositor, xdg_wm_base,
      zwp_linux_dmabuf_v1 and wp_viewporter. Only warnings appeared (no accessibility bus; no xdg-decoration in
      Weston, so the apps drew their own title bars).
    - **The X11 baseline:** 1,008 MB used of 3,416 MB with nothing open, 10 minutes after start-up.
    - **Not shown by A:**
      - Flatpak apps (Steam, Discord, Sober, Firefox, GIMP), because Flatpak runs outside Debian and cannot reach a
        compositor inside it.
      - Fair memory, start-up and battery numbers: nested, Weston and Xwayland add about 220 MB on top of X11.
    - The test's apps were closed afterwards. The packages stay in the owner's Debian sandbox and can be removed with
      apt.
  - **W0 (B), planned: a real Wayland session from a test stick**, so nothing is installed on the laptop.
    - **The build:** a Kryptonite test image built on the `kryptonite` branch (its kernel 7.3.x; or 6.18 for a
      like-for-like comparison, built both ways if it costs little). It adds a wlroots kiosk compositor (Cage) and
      Xwayland in the image, and a boot entry "Wayland test" that starts Standard's desktop page in Chromium with
      `--ozone-platform=wayland` under Cage instead of Xorg. Native's shell is X11-only until W1, so the comparison is
      **Standard on Wayland against Standard on X11**, from the same stick.
    - **The measurements**, each on X11 and on Wayland:
      - memory at idle, with Oracle, and with a Wine program
      - time to the desktop (the health log)
      - battery drain over 15 minutes idle and 15 minutes of video
      - Flatpak apps on Wayland: Steam, Discord, Sober, Firefox
      - a calls site
    - **The rules:** the stick never writes to the laptop's disk. The test session falls back to X11 if Cage does not
      start. Secure Boot with the test keys only.
    - **Done when:** the owner boots the stick and runs the checklist with Claude over the devlink, then the numbers
      and the go or no-go for W1 are written here.
- **W1, the compositor:** rudolf-native as a wlroots compositor showing Native's shell exactly as on X11, as a test
  session (Settings › System health › Display: Wayland (test)), falling back to X11 by itself if it does not start,
  as Native falls back to Standard today.
- **W2, the system around it:** input, the idle count, the lock, the emergency keys' watchdog, several monitors, the
  screen capture and remote access, the devlink's screenshot, the portals.
- **W3, the apps:** Chromium and WebKit on Wayland, Xwayland per account, Flatpak and Debian, Wine and Steam, game mode,
  the on-screen keyboard; same_t.sh and the behaviour scripts passing on both sessions.
- **W4, betas:** Wayland the session of new installations in betas (as Native's week of betas in 1.6.0); updated
  computers keep X11 and get the choice in Settings.
- **W5, the switch:** decided by the owner after the betas; X11 kept as the fallback session for at least two releases,
  then reconsidered. The mobile build is Wayland from its start.

### Order with the rest of 2.0
W0 can run at any time (it changes nothing). W1 to W3 come after wave 1 (librudolf-ui proved), so the native apps are
written once for both sessions, and alongside wave 2. W4 and W5 with wave 3's end, before 2.0's closing measurements.

## The newest kernel and the "next" channel (added on 2026-11-20)
The owner, 2026-11-20, after asking which kernel suits a high-end machine with Wayland, and whether mobile development can
be done on 1.6's 6.18 LTS with X11 and also on a 7.3 build with Wayland: "Keep the 7.3 kernel development and Wayland as
2.0 items. Add everything else to 1.6.1."

- **Two kernel tracks.** 1.6.x stays on the 6.18 longterm kernel (the newest LTS, `grep '^6\.18\.'` in
  rudolf-release.yml): everyday and older computers, the recovery system. 2.0's work runs on the newest stable kernel
  (7.3.x on 2026-11-20, then 7.4 once it has settled): newer graphics drivers (7.2: AMD HDMI 2.1 FRL, Intel Arc; 7.3: NVK
  video decode, Valve's AMD reset and memory work), new hardware earlier, the 2026 Steam Controller and MSI Claw drivers,
  and the Wayland compositors' HDR and variable-refresh needs.
- **The "next" channel (Rudolf Kryptonite's).** A third release channel beside stable and beta (gh-pages `next/`, its own manifest, signed with
  the same keys, joined the way beta is joined in Settings › Update and recovery). Built by the same workflow with two
  switches: the kernel track (LTS or newest stable) and the session started by default (X11 or Wayland). Recovery mode
  stays the official stable release (the owner's rule), so a computer on "next" always has its way back.
- **What "next" carries, in order:** first the newest kernel with X11 and the Wayland test session (W1, "Display:
  Wayland (test)"); then W2 and W3 as they land; Wayland by default there once W3 passes (before W4's betas on beta and
  stable). The kernel options for it: full preemption (`preempt=full`), 1000 Hz, sched_ext (a gaming scheduler such as
  `scx_lavd` loadable), NTSYNC for Proton, the newest firmware; measured as docs/plan-1.6.1.md Part 12 measures.
- **The phone:** stage 1 (its hardware, docs/plan-1.6.1.md Part 4) is done on 1.6's ARM64 build; the phone's own build
  and its interface (stage 2, below) are built on "next", Wayland from the start. The newer kernel helps the phones' own
  support (the PinePhone Pro, the Snapdragon 845 phones).
- **NVIDIA's own driver (docs/plan-1.6.1.md Part 19), on "next":** its module must build against each new kernel; the
  channel takes a new kernel only once it does (as Bazzite does), and says so in the build's notes.
- **Its logo** (the owner, 2026-11-20): Rudolf's logo in carbon-fibre off-black instead of white, the glowing nose green
  instead of red: iso/systools/kryptonite/ (kryptonite_logo.py draws it from the logo's own paths). Used wherever
  Kryptonite shows its own logo (its boot splash, About, the "next" channel's page); 1.6.x keeps the white logo.
- **The cost:** two release builds and two versions to test; the owner's rule for every session (same_t.sh's screenshots
  and the behaviour scripts) runs on both tracks.
- **Order:** the channel and the kernel switch first (nothing else changes for stable and beta); W0 measured on it; then
  the Wayland stages as above. When 2.0 (Rudolf Kryptonite) is released, "next" becomes stable and 1.6.x keeps the LTS for older computers
  for at least two releases.

## The phone build (added on 2026-11-15)
Decided on 2026-11-15 (docs/plan-1.6.1.md, "The phone, split in two"): the phone comes in two stages. Stage 1, in
1.6.1, makes one ARM64 phone's hardware work under today's native desktop (the modem's data, texts and calls,
emergency calls checked, the call's sound, sleep with the modem waking it, the battery). Stage 2, here, is the phone
people can carry: Rudolf OS's own phone interface, built once for Wayland and the native apps, so it is never written
for X11 and the web pages first and then again. The owner's rule holds: the phone's apps look and work as their
desktop versions do, laid out for a small touch screen.

**Why it waits for 2.0:** Wayland handles what a phone needs (touch gestures, rotation, scaling for a small sharp
screen, the input method for the on-screen keyboard, drawing only what changes, which saves power), and the native apps
fit a phone's memory and battery (a web app today: Settings 195 MB; a native one 15 to 40 MB). The phone build is
Wayland from its start (W1 to W3 below) and uses the native apps as each wave makes them.

### What it has
- **The phone's shell** (rudolf-native as the Wayland compositor, W1, in a phone layout chosen at start from the
  screen's size): the status bar (time, signal and operator, Wi-Fi, battery, the notification icons), the
  notification shade pulled down with quick settings (1.6.0's tiles), the home screen's **three panels** (the three
  workspaces, swiped left and right as on Android: docs/plan-1.6.1.md Part 2's groundwork), the app grid, the lock
  screen with the time, notifications and a call answered from it, the back and home gestures, rotation.
- **The phone's own apps, native from the start** (built with librudolf-ui's touch layouts): **Phone** (the dialler,
  calls, call history, voicemail, the in-call screen with the earpiece, speaker, headset and Bluetooth, the keypad,
  hold and two calls, emergency calls as 1.6.1 checked them), **Messages** (1.6.0's texts and picture messages, now
  native), **Contacts**. They come before wave 2's other apps on the phone.
- **The desktop's native apps on the phone:** each wave's app gets a touch layout in librudolf-ui (bigger targets, one
  column, its menus as a sheet), checked on the phone's screen size as each is checked against its web version:
  Clock, Calculator, Notepad, Settings, File Explorer, Media Player, Camera, Weather, Calendar, Help first.
- **The on-screen keyboard** as the Wayland input method (zwp_input_method_v2), in every app, Oracle and Linux apps
  included, with the account's language.
- **The camera and sensors used:** the Camera app with libcamera (photos and video, front and back), auto-rotation,
  the screen off at the ear during a call (proximity), auto-brightness, GPS for maps and weather, vibration for calls
  and notifications.
- **Battery:** suspend whenever the screen is off and nothing holds it, the modem waking it for calls and texts,
  apps in the background held back (as Android's Doze), measured against the same phone on Android (docs/plan-1.6.1.md
  Part 6), with targets set from 1.6.1's measurements.
- **Android apps through Waydroid,** integrated: their windows, notifications and the keyboard in Rudolf's own; the
  list of apps that work and that refuse kept in the docs.
- **Oracle** on the phone (WebKit or Chromium on Wayland, chosen by W0's measurements), with the phone's layout.

### Order with the rest of 2.0
1. 1.6.1's stage 1 done (the phone's hardware proven, its standby measured).
2. W1 to W3 (the compositor, the system around it, the apps on Wayland) on the desktop, with the phone layout's shell
   started alongside W1 on the phone itself.
3. Wave 1's native apps, then the phone's Phone, Messages and Contacts, then wave 2 with touch layouts.
4. The first phone beta: the shell, the phone's apps and wave 1 and 2's apps with touch layouts, Waydroid, the camera.
5. Whether the phone build is released with desktop 2.0 or after it: the owner's decision after its betas.

### Tests
- **QEMU (aarch64)** with a phone-sized screen, a touch screen and the stand-in modem (1.6.0's): the shell's gestures,
  the panels, the lock screen, a call and a text through the stand-in, rotation (a fake accelerometer).
- **Each app's touch layout** screenshot-compared with its desktop version for the same content (the rule's spirit:
  the same look, laid out for the screen).
- **On the phone (the owner):** a day's use measured (standby, screen-on, calls), calls and an emergency-call check on
  the carrier, the camera, the sensors, written in docs/testing.md.

### Measurements that close the phone build
On the first phone: memory at idle and with Phone, Messages, Oracle and two other apps open; standby drain overnight;
a day's mixed use; start to the lock screen; each against the same phone on Android where it can be.

## Oracle: website passwords, and perhaps blocking (added on 2026-11-16)
docs/plan-oracle.md: item 4 (a password manager for websites, in the account's keyring, with its own security review) is
suggested for 2.0, and item 5 (ad and tracker blocking) if it is not done in 1.6.1. Item 6 (Oracle's
extensions, from a reviewed list in the Library, signed with its key) is suggested for 2.0 too; its first stage could
start in 1.6.1. The owner decides.
