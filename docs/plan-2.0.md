# Rudolf OS 2.0: the apps become native (added on 2026-11-13)

Asked for on 2026-11-13: the whole of Rudolf OS as built so far converted to the native desktop. The shell first
(docs/plan-1.6.0.md Part 12: everything the web desktop's shell has, Native the default), then every app rewritten as a
native program, one at a time, in this plan. 2.0 is released when the apps in waves 1 to 3 are native.
**Its name: Rudolf Kryptonite** (the owner, 2026-11-20: "Rudolf's 2.0 version with the 7.3 kernel and Wayland will be
called Rudolf Kryptonite"): 2.0 on the newest kernel (7.3 and later) with Wayland. 1.6.x on the 6.18 LTS with X11 keeps
the name Rudolf OS.
2.0 also brings the phone's own build, on the hardware 1.6.1 proves ("The phone build", below).

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

### Status: the 7.3 kernel (2026-11-22, the kryptonite branch)
Asked for on 2026-11-21: the 7.3 kernel built now and held in reserve until 2.0 development starts. All of it is on the
long-lived `kryptonite` branch (pull requests based on it), never on main, the 1.6.x releases or the update channels.
- **Which kernel:** Linux **7.3.5** (the newest 7.3.y on 2026-11-22), pinned in `.github/kryptonite-kernel.txt`. Each newer
  7.3.y is taken by changing that line (the check warns when a newer point release is out); 7.4 once it has settled, as
  above. 1.6.x stays on 6.18.y (6.18.64 in the 2026112201 release).
- **Done:** iso/kconfig.sh moved over option by option (systools/kconfdiff.py: every option kconfig.sh and
  kconfig-required.txt name, compared between 6.18.64 and 7.3.5, and the options 7.3 added, dropped or changed): of 596
  options, two are gone (CRYPTO_HKDF: fscrypt's key derivation is in the kernel's crypto library now; SND_SOC_AMD_RPL_ACP6x:
  the Raptor Lake twin's ACP 6.2 is driven by ACP6x's own driver), two defaults changed and are kept as 1.6.x has them
  (the small HID drivers for keyboards and mice with quirks, now off by default: kept on; Intel TSX, now AUTO: kept off),
  one is new and kept off (Nova Lake's SOF: no firmware shipped, as the older platforms); 7.3's other changes taken as they
  are (preemption: PREEMPT_LAZY replaces VOLUNTARY, with PREEMPT_DYNAMIC so preempt=full stays a start-up option; HZ 1000
  unchanged; /proc/PID/mem writes need ptrace rights (PROC_MEM_FORCE_PTRACE), stricter than 6.18). Every Rudolf choice is
  checked as before (lockdown, module signing, the TPM, zram/MGLRU/PSI, ACPI_FPDT, AMD and Intel P-state, NVK's nouveau,
  IPU6/7, SOF, the Bluetooth chips) and the 79 required features (kconfig-required.txt) are there.
- **Built as the release builds it** (systools/testkernel.sh, the kernel part of ci_release.sh): 13 MB, modules 98 MB,
  every module signed with a key made for the build; the module-signing and Secure Boot design unchanged.
- **The firmware:** 7.3's drivers ask for 96 firmware names 6.18's did not (3 already listed, 78 in the base layer); none is in a family firmware-extra.txt takes
  (systools/kfwdiff.py), so the list and its R line (linux-firmware 0af0ecfd) stay as they are. Intel Wi-Fi asks for
  newer firmware (API 107 and 89) than the base layer has; the driver falls back to the older files, which load. The
  base layer's Wi-Fi firmware (wififw.py) is to be refreshed with the "next" channel's first build.
- **Out-of-tree:** NVIDIA's open kernel modules (595.112.07, the system extension nvidia-driver) build against 7.3.5.
  NVK itself is Mesa's (user space): nothing to move. No Library package builds against the kernel.
- **The checks** (`.github/workflows/kryptonite-kernel.yml`, on the kryptonite branch and its pull requests only,
  publishing nothing): the configuration (kconfdiff.py, kreq), the build, Secure Boot on in QEMU with test keys made in
  the job (qt/sb/recovery_sb_t.sh: shim, the menu and the kernel bundle signed with sbsign, lockdown, the handover to a
  kernel checked against the built-in certificate), the same check on the beta's own 6.18 kernel to compare, NVIDIA's
  modules, the firmware, and the test kernel in QEMU on the newest beta's layers next to the beta's kernel
  (qt/sb/kr20_t.py: Standard and Native reaching ready on X11, the self-test, the modules and firmware, kernel warnings).
- **Results (2026-11-22, the pull request's runs):** the configuration passes; the build passes; NVIDIA's modules build;
  nothing new for firmware-extra.txt. Secure Boot in QEMU with test keys: 6 of 7 (shim, the menu and the signed kernel
  bundle start the 7.3 kernel under Secure Boot, locked down; system/r is checked and handed over to by kexec, the
  signature checked against the built-in certificate; a changed layer, a list signed by another key and a changed
  recovery.efi are refused). The beta's own 6.18.64 passes all 7 on the same check.
- **Found and fixed (2026-11-22, #610, #611, #612): a 7.x kernel started other than by its kernel bundle stopped at once.**
  Two causes, one in the kernel and one in Rudolf's start-up:
  - **(a) After a kexec handover** ("Kernel panic - not syncing: ZSTD-compressed data is corrupt" at 1.5 GB; 7.0 to 7.3
    and 6.19 failed, 6.18.64 passed; only the receiving kernel mattered; 3 GB passed). The bisect
    (.github/kryptonite-bisect.txt, three rounds, the last run 45061622015) ends at one commit: **6e62d1c6f2c7 "efi: Wipe
    INITRD config table from memory after consumption"** (v6.19-rc6, in the efi-fixes-for-v6.19-2 merge bc08b658803):
    its parent passes, it fails, the merge fails and the merge's first parent passes, and the round's bad end with that
    one line taken out passes. Why: recovery.efi has an `.initrd`, so its kernel's EFI stub makes a LINUX_EFI_INITRD
    table, in EFI_LOADER_DATA, which that kernel then uses as free memory. The EFI tables go across kexec, and since that
    commit the new kernel writes zeros into the old table's place: 16 bytes in memory that, at 1.5 GB, held the new
    kernel's own built-in start-up image. **The fix:** a small kernel patch the 7.3 build carries
    (`iso/systools/kpatches/efi-initrd-table-kexec.patch`, named by .github/kryptonite-kernel.txt's `patch=` line,
    applied by testkernel.sh's KPATCHES): a kernel started by kexec (x86: the loader passed SETUP_EFI) neither uses nor
    wipes the old table; kexec passes any start-up image itself. Nothing of Secure Boot, lockdown, module signing or the
    handover's signature check changes (the handover still checks the kernel against the built-in certificate). A draft
    report for upstream, not sent: docs/kryptonite-upstream-efi-initrd.md.
  - **(b) Started directly by QEMU on the beta's layers** (kr20_t.py, 4 of 18): not the kernel. The test kernel's own
    modules layer does not match the beta's signed list, so with Secure Boot off the start-up script falls back to an
    unchecked mount of its loop device, which the checked device set up on it still held: "Device or resource busy",
    "the system layer 'modules' is damaged", back to the previous system, over and over. **The fix:** `rudolf-verity
    close NAME` removes the checked device first, on that unchecked path only (Secure Boot on and the official recovery
    still refuse such a layer); qt/host/verityfallback20_t.sh shows the busy loop device and the fix (8 of 8, as root on
    the runner). 1.6.x has the same fallback; it only matters there for a changed layer with Secure Boot off.
  - **Results (#612's runs, 7.3.5 with the patch):** Secure Boot in QEMU with test keys 7 of 7 at 1.5 GB; the cross
    handovers 6.18→6.18, 6.18→7.3, 7.3→6.18 and 7.3→7.3 all pass; nokaslr, 3 GB and 3 GB with nokaslr pass; the same kernel
    without the patch still fails the handover ("unpatched", for the record). kr20_t.py: Standard and Native reach ready
    on 7.3.5 as on the beta's 6.18.64, Native the engine, the self-test 22 and 21 pass 0 fail on both kernels, modules
    signed and enforced, no new kernel warnings, no missing firmware, every module loaded.
- **ARM64** (kconfig-arm64.sh, systools/arm64_kernel.sh, the ARM64 check): not moved yet; left for a later step.
- **The point releases to follow:** each 7.3.y as it comes (one line in .github/kryptonite-kernel.txt, the check warns),
  then 7.4 once it has settled; NVIDIA's modules must build on each before the "next" channel takes it.
- **Next:** Wayland, in stages (W0 to W5 above) on this kernel; the "next" channel's build switch (the kernel track), its
  release build carrying the same patches (kryptonite-kernel.txt's `patch=` lines); the upstream report, if the owner
  sends it, and the patch dropped once a 7.3.y or 7.4 has a fix; the handover tried on real hardware (the owner's
  checklist: an update's handover and Recovery mode on the Vivobook, with the owner present); ARM64's kexec
  (the patch is x86 only for now).

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
