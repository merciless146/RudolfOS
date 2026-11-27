# Rudolf OS 2.0: the apps become native (added on 2026-11-13)

Asked for on 2026-11-13: the whole of Rudolf OS as built so far converted to the native desktop. The shell first
(docs/plan-1.6.0.md Part 12: everything the web desktop's shell has, Native the default), then every app rewritten as a
native program, one at a time, in this plan. 2.0 is released when the apps in waves 1 to 3 are native.
**Its name: WendigOS** (the owner, 2026-11-22: "Rudolf Kryptonite is going to receive a name-change. WendigOS will be the
new name, standing for Wayland-ENhanced Driver-Integrated Gaming Operating System"): Rudolf OS 2.0, code-named WendigOS.
The `kryptonite` branch, its workflows and the logo folder keep their names; a new logo comes in a step of its own.
Before: (the owner, 2026-11-20: "Rudolf's 2.0 version with the 7.3 kernel and Wayland will be
called Rudolf Kryptonite"): 2.0 on the newest kernel (7.3 and later) with Wayland. 1.6.x on the 6.18 LTS with X11 keeps
the name Rudolf OS.
2.0 also brings the phone's own build, on the hardware 1.6.1 proves ("The phone build", below).

**Two editions, one repository (the owner, 2026-11-22):** "The 6.18 kernel version with X11 is going to continue being
developed as the main version of the operating system", and "Rudolf Kryptonite is going to be a version of Rudolf
specifically built and optimized for gaming and mobile use. Rudolf as it currently is in 1.6.x is going to be the regular
home office version." So WendigOS is an edition beside Rudolf OS, not its replacement:
- **Rudolf OS (1.6.x and on): the home and office edition, and the main one.** The 6.18 longterm kernel and X11, built
  for steadiness on everyday and older computers. It keeps the stable update channel, and the recovery system.
- **WendigOS: the gaming and mobile edition.** The newest stable kernel (7.3.x, then 7.4 and on: newer graphics
  drivers, handheld and controller drivers, HDR and variable refresh) with Wayland. It is tuned for games (game mode,
  the GPU drivers, Steam, Wine, emulators) and for phones and handhelds (the phone build, touch, battery). It has its
  own update channel ("next", below, becoming WendigOS's channel), so neither edition ever receives the other's
  updates.
- **One repository, built from main.** Most of the code is shared (the desktop, Oracle, Blitzen, File Explorer,
  rudolfd, the security work), and WendigOS's differences are additions: the kernel track, the Wayland session, and
  gaming and mobile tuning. They go into main behind build switches, and the release workflow builds each edition from
  the same tree. A fix made once reaches both, and the keys and their secrets stay in one place.
- **The `kryptonite` branch only for work that cannot share main yet** (today: the 7.3 kernel, while its kexec-handover
  crash at 1.5 GB in QEMU is understood). Each piece moves into main behind its switch as soon as it is sound, so the
  branch never drifts far from main. A separate repository only if WendigOS ever stops sharing Rudolf's code.
- **Work on Rudolf OS's features continues on main as before.** Gaming and mobile work is marked for WendigOS in its
  plan and PR, with the switch that keeps it out of the home and office edition where it does not belong there.

Why (1.5.9's measurements, docs/plan-1.5.9.md 12.3a): with nothing open Native uses 854 MB against Chromium's (formerly Standard's) 1,110 MB,
because no web page runs; each app opened today loads a WebKit page with the desktop's code (Settings: 195 MB). A native
app of the same kind is typically 15 to 40 MB.

## How a native app is built
- **Shared with Rudolf OS from 1.6.2** (the owner, 2026-11-22: native rewrites "a core fix for 1.6.2 as well as 2.0"; plan-1.6.2.md
  Part 39): the native apps and librudolf-ui are built from main for both editions, with no build switch, ordered for Rudolf
  OS by the memory they save (librudolf-ui, Task Manager, then Settings, then the Office apps); WendigOS uses the same apps.
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
  into the web page's storage directly, so the page (Chromium and Lite, still there) and native apps stay in step
  while both exist.
- **The same look (the owner, 2026-11-14: Rudolf OS on Native appears and works exactly as on Chromium):** a native
  app is drawn to look like its web version, not like a stock GTK app: librudolf-ui carries a GTK CSS theme made from
  the page's styles (its buttons, fields, menus, scrollbars, tabs, lists, the accent colour, light and dark), and each
  app's windows are screenshot-compared with the web version as in docs/plan-1.6.0.md Part 12's rule (under 2 % of
  pixels different, text anti-aliasing aside). An app is not switched over until it passes.
- **The same behaviour:** each app's native version passes a checklist made from its web version (its menus, its
  shortcuts, its files, its settings, what its browser tests check), screenshots compared side by side under Xvfb.
- **Until an app is native, its web version opens** in a one-app WebKit window as in 1.6.0. When the native version
  passes, the start menu, the pins, file associations and Open with point to it; its web version stays in the page for
  Chromium and Lite until 2.0 decides their future (below).

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
- **Oracle Browser:** a browser is a web engine; it stays Chromium or WebKit, framed by the native window
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
- Whether Chromium and Lite (the web desktop) stay in the system layer as fallbacks, or move to a recovery-only role,
  or go. Until then they stay, and Native gives up to Chromium as in 1.5.9.
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

**What stays the same:** the owner's rule: Rudolf OS on Wayland appears and works exactly as on X11 (and as Chromium),
checked the same way (same_t.sh's screenshots, the behaviour scripts). Accounts, files, rudolfd, updates, recovery and
the security model do not change. Native, Chromium and Lite all run on it; X11 stays as a fallback session.

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
- **Chromium (the Chromium desktop, Oracle, its Tor windows):** Chromium on Wayland (--ozone-platform=wayland); the shell
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
  screenshots. same_t.sh compares X11 and Wayland screens as it compares Native and Chromium.
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
    - **The system logs over the test** (000563): no GPU fault, reset or timeout, no crash and no out-of-memory in
      the kernel's messages. The only entries were from start-up: the laptop's ACPI table warnings, and Xorg's
      input-driver fallbacks, as on every start.
    - **Not shown by A:**
      - Flatpak apps (Steam, Discord, Sober, Firefox, GIMP), because Flatpak runs outside Debian and cannot reach a
        compositor inside it.
      - Fair memory, start-up and battery numbers: nested, Weston and Xwayland add about 220 MB on top of X11.
    - The test's apps were closed afterwards. The packages stay in the owner's Debian sandbox and can be removed with
      apt.
  - **W0 (B), planned: a real Wayland session from a test stick**, so nothing is installed on the laptop.
    - **The build:** a WendigOS test image built on the `kryptonite` branch (its kernel 7.3.x; or 6.18 for a
      like-for-like comparison, built both ways if it costs little). It adds a wlroots kiosk compositor (Cage) and
      Xwayland in the image, and a boot entry "Wayland test" that starts the Chromium desktop's page with
      `--ozone-platform=wayland` under Cage instead of Xorg. Native's shell is X11-only until W1, so the comparison is
      **Chromium on Wayland against Chromium on X11**, from the same stick.
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
  as Native falls back to Chromium today.
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
- **The "next" channel (WendigOS's).** A third release channel beside stable and beta (gh-pages `next/`, its own manifest, signed with
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
  instead of red (made while 2.0 was Rudolf Kryptonite; WendigOS's own logo is a later step): iso/systools/kryptonite/ (kryptonite_logo.py draws it from the logo's own paths). Used wherever
  WendigOS shows its own logo (its boot splash, About, the "next" channel's page); 1.6.x keeps the white logo.
- **The cost:** two release builds and two versions to test; the owner's rule for every session (same_t.sh's screenshots
  and the behaviour scripts) runs on both tracks.
- **Order:** the channel and the kernel switch first (nothing else changes for stable and beta); W0 measured on it; then
  the Wayland stages as above. When 2.0 (WendigOS) is released, "next" becomes stable and 1.6.x keeps the LTS for older computers
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

## Moved from 1.6.2: the mobile and gaming parts (added on 2026-11-22)
Moved from docs/plan-1.6.2.md on 2026-11-22: WendigOS is the gaming and mobile edition ("Two editions, one
repository", above), and the owner said "Yes" to moving these parts here. Their text is as it stood in the 1.6.2 plan,
with numbers kept so references still point the right way. They are WendigOS's, built from main behind its build
switches, or on the `kryptonite` branch while they cannot share main.

**What they need:**
- **The MiFi 7730L with an active SIM:** Part 8, then Part 7 (T-Mobile-network prepaid SIM).
- **The first phone (an ARM64 phone; the PinePhone Pro through Tow-Boot) or an ARM64 computer with UEFI:** Part 4,
  Part 5 (eSIM and 5G hardware), Part 6's items measured on it, and 9b (the first ARM64 release).
- **The Vivobook and the Asus desktop "RuckXTrauma", with the owner:** 19b (gaming measured).

**Order:** Part 8 with the MiFi, then Part 7. Then the phone: Part 4, Part 6's items measured, 9b, and Part 5 when the
hardware is there. 19b with the owner whenever games are measured, on WendigOS's kernel too.

#### 4. The first phone, stage 1: the hardware (from 1.6.0 "Not in 1.6.0", and docs/plan-1.5.8.md 8.3)
The phone's hardware made to work, on today's native desktop; its interface is 2.0's (stage 2).
- **One well-supported phone first:** a used PinePhone Pro (discontinued in August 2025, but its hardware is the best
  supported by mainline Linux and its modem, the Quectel EG25-G, has working VoLTE: Part 7), with the OnePlus 6/6T
  after it (its VoLTE does not work yet in postmarketOS). Bought and chosen by the owner.
- **Starting:** the ARM64 release from 1.6.0's cellular and ARM64 plan (stage A in QEMU, stage B's first ARM64
  computer), on the phone through its UEFI-capable boot firmware (Tow-Boot or U-Boot with UEFI on the PinePhone Pro),
  so it starts as Rudolf OS starts on a computer: the boot menu, Secure Boot where the firmware allows, recovery. The
  kernel: Rudolf's own build with the phone's drivers (the device tree and options postmarketOS and mainline Linux use
  for it, written into kconfig-arm64.sh), never a vendor kernel.
- **The modem:** mobile data, texts and picture messages (1.6.0's Messages), **voice calls** (a call each way, hold,
  two calls, DTMF tones for menus), and **emergency calls**, which must be checked on each device and carrier before
  they are offered (Part 7 says how). A minimal call screen in today's desktop (answer, end, mute, speaker, keypad),
  enough to prove calls; the phone's real dialler is stage 2's.
- **The call's sound:** the earpiece, the speaker and a headset, with the modem's audio routed (ALSA UCM for the
  phone's sound card), the microphone, no echo.
- **Power:** sleep with the modem waking the phone for a call or a text (and the alarm clock), the battery's level,
  charging and the charger's limit, the screen's brightness, the power key.
- **The rest of the hardware, at driver level:** the touch screen, Wi-Fi, Bluetooth, the cameras detected by
  libcamera, the sensors (accelerometer, proximity, light) and GPS readable, vibration. Their apps and behaviour
  (auto-rotation, the screen off at the ear, a Camera app) are stage 2's.
- **Today's desktop on a phone screen:** Native on X11, scaled for the screen, the on-screen keyboard (1.6.0's), touch
  mode: usable for testing, not meant to be a phone's interface.
- **Done when:** the phone starts Rudolf OS from its storage, makes and takes a call on a T-Mobile-network SIM, sends
  and receives texts, sleeps and wakes for a call, and its standby battery drain is measured (Part 6), all written in
  docs/testing.md.
- **On 1.6's kernel and X11 (2026-11-20):** stage 1 is built on 1.6.x (the 6.18 LTS, X11, ARM64); the phone's own build
  and interface move to 2.0's "next" channel (the newest kernel, Wayland). A phone with 8 GB for the full desktop and
  Wayland testing: the OnePlus 6T (8 GB), Snapdragon 845 as the OnePlus 6, started through U-Boot's UEFI from its boot
  partition; its VoLTE (needed for calls on T-Mobile) does not work yet, so the PinePhone Pro stays the phone for calls.
  The Fairphone 5 (8 GB) to watch as its mainline support matures. Phones running on Android's drivers (Furi Labs FLX1s+,
  Volla Phone Quintus) do not fit "never a vendor kernel".
- Nothing on a phone is flashed or unlocked without the owner's decision.

#### 5. eSIM and 5G (from 1.6.0 "Not in 1.6.0")
- eSIM provisioning (downloading a carrier's eSIM profile);
- 5G standalone features beyond what the modem does by itself.

1.6.0 left these without a release; they are here so they are not lost. They need hardware with an eSIM and a 5G
modem.

#### 6. Closing the gap with Android (added on 2026-11-15)
Asked for on 2026-11-15, after comparing 1.6.1 with Android: the items that decide whether someone could carry a
Rudolf phone every day. Each is measured on the first phone (Part 4) and compared with the same phone on Android
where it can be. **Split (owner, 2026-11-15):** 1.6.1 does what the hardware decides (marked *1.6.1* below: the
measurements, calls with the carrier, the modem's sleep); the parts people use every day are 2.0's phone build
(marked *2.0*), since they are built on Wayland and the native apps.
- *2.0, measured in 1.6.1.* **Android apps through Waydroid, made dependable:** set up from Settings, apps from F-Droid and an app store the
  owner chooses, their windows and notifications in Rudolf's own; a list of apps that work and that refuse (banking
  apps that check the device), kept in the docs. Measured on the Vivobook first (docs/plan-1.6.0.md: "Waydroid
  measured on the Vivobook with Lite before it is offered").
- *1.6.1 measured and the modem's sleep; the rest 2.0.* **Battery life:** the phone's idle and screen-on drain measured, then: the modem, Wi-Fi and the screen put to sleep
  when not needed, suspend with the modem waking it for a call or a text, apps in the background held back as
  Android's Doze does; targets set from the measurements, shown in Settings › Battery.
- *1.6.1.* **Calls with the carrier:** voice calls over 4G (VoLTE) where the modem and carrier allow, Wi-Fi calling if the
  carrier offers it, a call to a real phone and an emergency-call check on each device (Part 4), the call's sound
  through the earpiece, speaker and a headset.
- *2.0 (the cameras detected in 1.6.1).* **The camera:** photos and video with the phone's cameras (libcamera), in a Camera app; quality compared with
  Android's on the same phone.
- *2.0 (readable in 1.6.1).* **The phone's sensors and hardware:** auto-rotation, proximity (the screen off at the ear), brightness, GPS for maps
  and weather, the fingerprint reader where the phone has one, vibration.
- *2.0.* **Messaging apps people need:** what works through Waydroid or the web (Signal, WhatsApp Web), and what does not,
  said plainly in the docs.
- **Tests:** each measured and written in docs/testing.md; the battery and call results on the owner's phone and
  carrier.

#### 7. T-Mobile and the prepaid services on its network (added on 2026-11-15)
Asked for on 2026-11-15: "Plan for T-Mobile and adjacent prepaid service coverage in 1.6.1." The US has no 3G left,
so calls need VoLTE, and carriers decide which phones get it. T-Mobile's network is the friendliest to Linux phones
(reports in the Pine64 wiki's carrier list and forum, 2020 to 2024); AT&T and Verizon are later.
- **The carriers covered:** T-Mobile (postpaid and prepaid), Mint Mobile, Tello, US Mobile on its T-Mobile network,
  Simple Mobile, Metro by T-Mobile, Ultra Mobile and Google Fi (data and calls; Fi's first activation needs an
  Android phone and its picture messages do not work: said in Settings). The list is checked again before work
  starts, since prepaid brands change networks.
- **The first phone: the PinePhone Pro** (Part 4). Its modem (Quectel EG25-G) has working VoLTE; the OnePlus 6's VoLTE
  does not work yet in postmarketOS (pmaports issue 1878), so it waits.
- **Set up by itself:** Settings › Mobile network recognises the carrier from the SIM (its MCC/MNC and its name) and
  fills in the APN exactly, letters' case included (US Mobile's "PWG" failed in lowercase on a PinePhone Pro; Simple
  Mobile's is "Simple"), with the picture messages' settings (MMSC, proxy). The APNs come from the operators'
  database 1.6.0 already ships, with Rudolf's own corrections for these carriers kept beside it.
- **VoLTE turned on:** the modem's IMS/VoLTE set up for T-Mobile (its carrier profile on the EG25-G), checked after
  every start and after a SIM change; Settings says whether calls go over 4G.
- **Wi-Fi calling** where T-Mobile offers it on the phone (VoWiFi through the modem's IMS), else said plainly.
- **Texts and picture messages:** SMS (1.6.0 Part 6) and MMS through the APN's MMSC, in the Messages app.
- **Activation help:** what each carrier needs (activate online, the phone's IMEI given to the carrier, support asked
  to turn VoLTE on for the line), written in the docs and linked from Settings when the SIM has no service.
- **5G:** not on the PinePhone Pro (an LTE modem). T-Mobile's 5G is tested later with a 5G modem card in a laptop
  (Part 5).
- **Tests:** a prepaid Mint or Tello SIM first (cheap, no contract), then T-Mobile itself: data, a call each way, a
  call to and from a landline, an emergency-call check as Part 4 says, texts and picture messages each way,
  Wi-Fi calling, roaming off, the APN filled in from a fresh SIM, and a call during a weak signal. Results in
  docs/testing.md, with the carrier and date. A host test of the APN table (each carrier's MCC/MNC and name gives the
  right APN, case kept).
- **Later:** AT&T's network (stricter about devices), then Verizon (approved phones only).

#### 8. Mobile data and the SIM on real hardware (moved from 1.6.0 on 2026-11-16)
Decided by the owner on 2026-11-16: "For now, move the mobile/SIM testing to 1.6.1." 1.6.0 ships the cellular work
proven without hardware (its stage A: ModemManager, Settings › Mobile network, the connection and failover, data and
the monthly limit, the taskbar's signal, Messages, all with host, browser and QEMU tests against a stand-in modem);
what needs a real modem and SIM is checked here, before 1.6.1's phone work relies on it. From docs/plan-1.6.0.md's
stage B (items 1 to 3) and its release checks:
- **The MiFi 7730L** with an active SIM (the owner's choice; the HP desktop user's USB stick only if it is lent again):
  found in Settings › Network › Cellular modems (maker, model, SIM, operator), no "Invalid rule match" in
  /run/rudolf/modem.log, the self-test's modem line; the SIM's PIN (never the PUK on a SIM someone needs); the APN filled
  in (a prepaid T-Mobile-network SIM, Part 7); connected; Wi-Fi failover and back; IPv4 sites on an IPv6-only network
  (the CLAT); data counted against the operator's own count; the 80 % notice and data off at 100 %; an update offered
  on mobile data with its size; the bars and type against the device's own; the SIM taken out while running; unplugged
  and plugged in again; sleep and wake (after 1.6.0's sleep fix for the Vivobook, CONFIG_AMD_PMC).
- **Messages** to and from a real phone (Unicode, a long one, the delivery report), a sign-in code, the notice with
  the desktop locked, kept across a restart and in the encrypted folder.
- **Share this connection** with the MiFi's USB tethering as the uplink (1.5.9 Part 4, waiting since then for a data
  plan).
- **The FCC-unlock path** on a laptop with a WWAN card (a used ThinkPad with a Fibocom L850-GL or Quectel EM05-G).
- Each checked on Chromium and Native (the owner's rule), results in docs/testing.md with the carrier and date.

#### 9b. The first ARM64 release (from 1.6.1 Part 9)
- **ARM64 (1.6.0 Part 7 and its stage B item 4):** the first ARM64 release, after one ARM64 computer that starts with
  UEFI (or the first phone, Part 4: the PinePhone Pro through Tow-Boot), its own update lists (beta/arm64/,
  stable/arm64/), the update and recovery checks on it.


#### 19b. Gaming measured (from 1.6.1 Part 19)
The NVIDIA driver choice and the game mode are built in 1.6.1; measuring them needs the owner's computers.
- **Measured:** the same games' frame rates and frame times on the open and NVIDIA drivers and with game mode on and
  off, on the owner's Asus desktop (live from a stick, as Part 12) and the Vivobook.


**The checklist is written (2026-11-21, branch game-checklist): below; the numbers are the owner's.** It follows the
  NVIDIA driver PR (#544, branch nvidia-driver) and the game mode PR (#546, branch game-mode): run it once both are in a beta.

##### 19b. The owner's checklist: games measured (the Asus desktop live from a stick, as Part 12, and the Vivobook)
What is measured: the same games' frame rates and frame times on the open driver (NVK) and on NVIDIA's driver, each with
game mode on and off, on the owner's **Asus desktop** (RuckXTrauma: its NVIDIA card must be GeForce 16 or newer for NVIDIA's
driver to be offered), Rudolf OS running live from a USB stick (nothing on its disk changed), and with game mode on and off
on the **Vivobook** (AMD graphics: no NVIDIA rows). Nothing here was measured by a session: no NVIDIA card or real game is
reachable from CI.

**Before (once).**
1. The stick: made with Settings › Update and recovery › Make a stick, with its RUDOLFDATA partition (system extensions and
   Steam's games are kept there; a stick without one cannot keep NVIDIA's driver). Start the Asus from it; Secure Boot as
   it is on that computer (with Secure Boot on, the first start asks to enroll the Rudolf OS key: that is the firmware's
   own list, the disk is not touched; or start with Secure Boot off for these tests).
2. NVIDIA's driver: after the owner ran **Actions › System extensions › Run workflow** (release = the release's serial,
   names = `nvidia-driver`), Settings › Update and recovery › System extensions › Install… › NVIDIA's graphics driver
   (an administrator's password), then restart. Settings › Display › Graphics driver shows the card's name.
3. The games, the same on both computers where they run (fill in): A ______ (a native Linux game in Steam),
   B ______ (a Windows game through Proton, DirectX 11/12), C ______ (a Vulkan game or a built-in benchmark), D ______
   (a browser game in Oracle, WebGL). Each with the same settings every time (resolution ____, quality ____, V-Sync **off**),
   the same 2-minute scene or the game's own benchmark.
4. Frame times: Steam (Flatpak) with MangoHud (`flatpak install flathub org.freedesktop.Platform.VulkanLayer.MangoHud`,
   the game's launch options `MANGOHUD=1 MANGOHUD_CONFIG=autostart_log=1,log_duration=120 %command%`): each run leaves a
   CSV in ~/.var/app/com.valvesoftware.Steam/ (its summary: average FPS, 1 % low, 0.1 % low). The browser game: Oracle's
   own frame meter (F12 › Rendering › Frame rendering stats) or the game's counter, average only.
5. Each line below: 3 runs, the median written; the computer on mains power; nothing else open (Oracle closed for A-C).

**Each run, also look at (write "ok" or what happened):**
- Game mode on: Settings › System health › Memory › Game mode says "On now" during the game (open it with the game
  in the background, then go back); on the Vivobook, System health › Power and battery shows the energy preference
  "performance" during the game and the usual one after; a notification sent during the game (a message, a reminder)
  is held, and "While you were playing" comes once the game leaves full screen.
- Game mode off (the same switch): none of that.
- After each game is closed: Task Manager › System (the desktop's processor time is back), no slow desktop.
- NVIDIA's driver: Task Manager › System › Graphics names NVIDIA's driver; once, start Recovery mode: it starts on the
  open driver (the firmware's screen), and the next normal start is back on NVIDIA's.
- Once, to see the switch back: with NVIDIA's driver chosen, remove the extension (System extensions › Remove) and
  restart: that start is on the open driver, and a notification says it went back by itself and why.

**The Asus desktop (RuckXTrauma), live from the stick.** CPU ______, memory ______, NVIDIA card ______ (its PCI ID from
Settings › Display › Graphics driver ______), screen ______ Hz.

| Game | Driver | Game mode | Average FPS | 1 % low FPS | 0.1 % low FPS | Frame time p99 (ms) | Notes (stutter, errors) |
|---|---|---|---|---|---|---|---|
| A | Open (NVK) | off | | | | | |
| A | Open (NVK) | on | | | | | |
| A | NVIDIA's | off | | | | | |
| A | NVIDIA's | on | | | | | |
| B | Open (NVK) | off | | | | | |
| B | Open (NVK) | on | | | | | |
| B | NVIDIA's | off | | | | | |
| B | NVIDIA's | on | | | | | |
| C | Open (NVK) | off | | | | | |
| C | Open (NVK) | on | | | | | |
| C | NVIDIA's | off | | | | | |
| C | NVIDIA's | on | | | | | |
| D | Open (NVK) | off | | | | | |
| D | Open (NVK) | on | | | | | |
| D | NVIDIA's | off | | | | | |
| D | NVIDIA's | on | | | | | |

Also on NVIDIA's driver, once: does DLSS (or its frame generation, GeForce 40 and newer) appear in B's or C's settings
(Proton gives games NVIDIA's DLSS files: the extension carries them), and does ray tracing switch on? ______

**The Vivobook (AMD graphics, amd_pstate).** The games that run on it (A, D, and B or C if they start):

| Game | Game mode | Average FPS | 1 % low FPS | 0.1 % low FPS | Frame time p99 (ms) | Energy preference during the game | Notes |
|---|---|---|---|---|---|---|---|
| A | off | | | | | | |
| A | on | | | | | | |
| D | off | | | | | | |
| D | on | | | | | | |
| B or C | off | | | | | | |
| B or C | on | | | | | | |

**What decides.** NVIDIA's driver stays an option (never the default) if it starts and runs the games on the Asus; it is
worth recommending in Settings only where it is clearly faster (more than 10 % average or 1 % low). Game mode stays on by
default if "on" is never slower than "off" on either computer (within the runs' own spread); if the desktop's
slowing behind a game ever makes the game itself stutter (a launcher or overlay in the desktop's group), say so: the
weight (20) is one line in /usr/lib/rudolf/gamemode.


## Game Mode: WendigOS's own console mode (added on 2026-11-22)
The owner, 2026-11-22, after comparing WendigOS with SteamOS, Bazzite, CachyOS, Nobara and ChimeraOS (all of which but CachyOS
and Nobara start into Steam's Game Mode under gamescope) and asking for gamescope's Game Mode to be described: of the two ways
offered (gamescope's Steam session as a separate session, or Rudolf's own console mode in its compositor), the owner asked to
see the second as a concept, then its cost, and answered "Alright, sounds good". The concept's three screens (home, the
in-game Quick menu, a game's page) are on the owner's design canvas "WendigOS Game Mode — concept".
- **The look: Version 2** (the owner, 2026-11-22: "Version 2 is ideal"), the canvas's "Home, version 2" artboard: the
  WendigOS brand leaned into (the wordmark set wide in capitals with "OS" in a glowing green, "Wayland-Enhanced ·
  Driver-Integrated · Gaming" beneath it, a carbon-fibre off-black ground, one green accent used throughout, a frost and
  night key art with aurora bands, no creature drawn) and what current game dashboards do: a full-bleed cinematic hero
  with the game's title in large capitals, its progress and Resume on A; shoulder-button tabs (LB/RB: Home, Library,
  Steam, Emu, Media); a shelf of cover-shaped cards with their source on each, the focused one larger, ringed and
  glowing; glass side cards for the performance profile (and the play time left on battery) and downloads; the
  controller, Wi-Fi, battery, the time and the account at the top; the controller's buttons along the bottom. The
  Quick menu and the game's page are to be redone in the same look when Game Mode is built. Type: Space Grotesk and
  Inter (both SIL Open Font License) unless fonts already in the system will do; the wordmark stays type until
  WendigOS's logo is made in its own step.
- **Its colours follow Display Properties** (the owner, 2026-11-22: "Have it so that the game-mode's color theme (the green
  highlights and artwork at the top of the screen) follows the color theme selected in display properties"): the
  account's Settings › Display › Colours and theme › Accent colour (`S.accent`: its eight swatches or any colour) sets every
  highlight (the tab underline, Resume, the progress bar, the focused card's ring and glow, the status dots, the account's
  ring, the Library's source chips) and tints the key art at the top (the aurora bands and the night sky). Text on a
  highlight turns dark or light by the accent's luminance; the accent used as text on the dark ground is lightened until
  it reaches 7:1, so every accent stays readable (slate and the dark blues included). Changing the accent in Display
  Properties changes Game Mode at once, as it changes the desktop. Steam's and Emu's chips keep their own colours (blue,
  amber) so the source stays recognisable. WendigOS's default accent is Version 2's green (#4ff0a0); Rudolf OS 1.6.x
  keeps its own default (blue). The canvas's Version 2 shows it with the accent as a tweak.
- **What it is:** a full-screen, controller-first session of WendigOS's compositor (not gamescope, not a web page): one
  launcher for the Library's games, Steam's (through Proton) and Emu's, with source tabs (All, Library, Steam, Emu,
  Recent), a game's page (continue, its save's state, its own settings) and "Switch to desktop" back to the WendigOS desktop.
- **The Quick menu** (one button, over the running game): a small performance overlay (FPS, frame time, temperatures, a
  frame-time graph), the power limit (TDP, per game, where the hardware allows it: AMD's and Intel's processor drivers),
  the frame limit, scaling (FSR 1, integer, off), the refresh rate and variable refresh, HDR where the display and the
  compositor can, screenshot, suspend and quit. Game mode (rudolfd's, docs/plan-1.6.1.md Part 19) is on for the game.
- **How it is built, lean:** native C and GTK (layer-shell surfaces of the compositor, as the shell's), never the desktop's
  web page; the compositor itself does the frame limit, scaling and direct scan-out. While a game runs the launcher sleeps
  and only the Quick menu stays ready. Steam is started only for Steam's games. Rudolf's own small overlay rather than
  MangoHud if it is smaller. Fonts already in the system where they will do.
- **Its cost, estimated on 2026-11-22 (to be measured, as everything here):** disk about 2-8 MB (the program under 1 MB, the
  compositor's part under 1 MB, fonts about 1 MB if new ones are needed, MangoHud 2-5 MB only if used; per-game artwork
  kept in the account's folder). Memory about 15-40 MB on the home screen, 5-15 MB during a game (plus 5-10 MB with an
  overlay). Steam's client, several hundred MB, only while a Steam game runs, as on every system that runs Steam. Against
  gamescope's Steam session, which keeps Steam's interface running throughout, it saves most of that when no Steam game is
  open.
- **Order:** after W3 (the apps on Wayland), with the handheld controls (the Steam Controller and handhelds' drivers in the
  7.x kernel; fan and TDP where the hardware allows) and the phone build's touch work. Not started.
- **Tests:** the compositor's headless backend with a stand-in game (full screen, the frame limit and scaling measured),
  controller-only navigation, the Quick menu over a game, suspend and resume, the switch to the desktop and back; memory
  and disk measured against the estimates above; on the owner's Vivobook and the Asus desktop (19b's games).

## Following 1.6.x's trimming (added on 2026-11-22)
The owner, 2026-11-22: "Keep track of the trimming work that the main session is doing for the 1.6.x versions of native and
apply whatever is beneficial to performance here as well." WendigOS is built from main, so most of 1.6.x's trimming
reaches it by itself; this section is where each trimming part is checked for what WendigOS needs beyond that.
- **The rule:** when a 1.6.x part that trims memory, disk or start-up time is planned or merged (on main), its entry is
  added below the same day: what reaches WendigOS by itself, and what WendigOS needs on top (measured again on Wayland,
  done in the compositor, or carried to the `kryptonite` branch while that branch exists). A trim that only helps X11,
  Chromium mode's page or 1.6.x's older computers is noted and not carried. The handoff (rudolfos-devlink) keeps the list
  of what is waiting.
- **The kernel:** while the 7.3 kernel lives on the `kryptonite` branch, every change to main's iso/kconfig.sh (options
  added, removed or made modules) is carried to that branch's kconfig.sh in a PR based on `kryptonite`, unless it is
  6.18-only.
- **1.6.2 Part 38, smaller pages for the native desktop's app windows** (planned 2026-11-22, branch plan-lean-pages: each
  Native app window loads the whole desktop page, about 3.5 MB of script and about 100 MB a window; a small page per app,
  about 35 MB): reaches WendigOS by itself (the same build19.py pages and the same native shell). On top: measured again
  with WebKitGTK on Wayland (W3) before and after, since its process floor may differ from X11's; the small pages are
  WendigOS's way until each app is native (this plan's waves), and a wave's native app replaces its small page, never both.
  Step 5 (Oracle's Chromium memory) applies as it is: Oracle runs in Rudolf's Chromium on both editions.
- **Already in 1.6.x and carried by main:** the native shell's memory work (1.5.9-1.6.1: lean61.js, the apps moved to files
  loaded when opened), zstd for the images (1.6.2 Part 31: measured per engine; its kernel side, SQUASHFS_ZSTD, is on the
  `kryptonite` branch since #617), game mode (1.6.1 Part 19).
- **1.6.3 Part 9, PipeWire and WirePlumber beside PulseAudio** (docs/plan-1.6.3.md; branch `pw-beside-pulse`): the
  Native desktop's camera and microphone capture, which WebKitGTK 2.52 does only through PipeWire. It reaches WendigOS by
  itself: the layers are built from main, `mkrootfs.sh` and `ci_release.sh` both run `systools/mkpipewire.sh`
  unconditionally (9 MB in the base layer; the ARM64 build, which goes through `mkrootfs.sh`, logs `pipewire: 9019 KB`),
  and no build switch holds it back. On top: measured again on Wayland (W3), where the camera and screen capture also pass
  through the portal (this plan's W0 notes). **The `kryptonite` branch has neither `mkpipewire.sh` nor the call** (last at
  `ec2edfa9`, 2026-11-23), so it must merge main before a layer built from it has the daemon -- the self-test's `capture`
  line fails there until it does, by design.
- **Waiting to carry to the `kryptonite` branch:** 1.6.2 Part 35's SMB client (CIFS as a signed module, the legacy dialects
  and SMB server off), in main's kconfig.sh since #636.
