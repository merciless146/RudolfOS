# Rudolf OS 2.0: the apps become native (added on 2026-11-13)

Asked for on 2026-11-13: the whole of Rudolf OS as built so far converted to the native desktop. The shell first
(docs/plan-1.6.0.md Part 12: everything the web desktop's shell has, Native the default), then every app rewritten as a
native program, one at a time, in this plan. 2.0 is released when the apps in waves 1 to 3 are native.

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
