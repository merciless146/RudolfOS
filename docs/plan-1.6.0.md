# Rudolf OS 1.6.0: the first cellular release

Asked for on 2026-11-10: plan the first release of Rudolf OS that works with cellular networks, and stage what it
needs in 1.5.8 and 1.5.9. It follows the mobile work in docs/plan-1.5.8.md (Parts 6 to 8) and docs/plan-1.5.9.md.

## What "cellular" means in 1.6.0
- **Mobile data, by every common route, on x86 computers:** a hotspot or phone on Wi-Fi (works today), a hotspot or
  phone over USB (1.5.8), a USB cellular stick, and a cellular module built into a laptop or tablet (M.2 WWAN cards:
  Quectel, Fibocom, Sierra Wireless, Telit).
- **The SIM and the network handled in Rudolf OS:** the SIM's PIN (and PUK), the operator's settings (APN) found by
  themselves, roaming on or off, signal, operator and network type in the taskbar, airplane mode.
- **Living with a data plan:** metered connections (1.5.8), data used this month and a limit, Wi-Fi preferred over
  cellular, and cellular as the fallback when Wi-Fi drops.
- **Text messages, read and sent** through the modem (carrier messages, sign-in codes), in a Messages app.
- **ARM64:** the first ARM64 release, for computers that start with UEFI (and QEMU), with the same cellular support.

**Not in 1.6.0:** voice calls and emergency calls (a computer is not a phone, and emergency calling must be tested on
each device before it is offered: 1.6.x, with the first phone), phones themselves (1.6.x, docs/plan-1.5.8.md 8.3), eSIM
provisioning (downloading a carrier's eSIM profile), and 5G standalone features beyond what the modem does by itself.

## Leanness
- Cellular support adds about 6 to 10 MB to the base layer (ModemManager, libqmi, libmbim, libqrtr, the operators'
  database, the kernel drivers as modules): measured in the release check against a 15 MB budget.
- ModemManager starts only when a modem is present (mdev starts it on the first modem it sees), so a computer without
  one runs nothing more.
- The Messages app is part of the desktop page (no new engine), and keeps messages in the account's folder.

## Parts

### 1. Modems (needs 1.5.9's ModemManager)
- ModemManager, with QMI, MBIM, QRTR and AT support, in the system layer; its D-Bus API read by rudolfd (`mobile/*`).
- **FCC unlock:** many laptop modules (Lenovo's and Dell's Fibocom and Quectel cards) start with their radio locked
  until the computer unlocks them. ModemManager's unlock scripts ship but are off; Settings offers "Turn on this
  laptop's cellular module" for the modules that need it, saying what it does, administrators only.
- Modules that Linux supports only partly (Intel's XMM7360 in some ThinkPads) are named in Settings with what is known
  about them, rather than failing silently.

### 2. SIM and network (Settings › Mobile network)
- The SIM: present or not, locked with a PIN (asked once, by an administrator; three tries; the PUK after that, with
  the warning that a wrong PUK blocks the SIM for good), and the operator.
- The operator's settings (APN) from mobile-broadband-provider-info, by the SIM's operator code; a custom APN by hand.
- Roaming: off by default, with what it may cost.
- Preferred network type (5G, 4G, 3G) where the modem lets it be chosen.
- Airplane mode in the taskbar's quick settings: Wi-Fi, Bluetooth and the modem off together (rfkill and
  ModemManager), and back as they were.

### 3. The connection
- Connect and disconnect from the taskbar; connect automatically (on by default for the SIM's home network).
- Priority: cable, then Wi-Fi, then cellular; cellular stays connected but unused while Wi-Fi works, and carries the
  traffic within seconds when Wi-Fi drops (the routes' metrics), with a notification.
- The firewall applies to the cellular interface as to every other.
- IPv6 and IPv4 as the operator gives them (many operators are IPv6-only with NAT64/464XLAT: clat where needed).

### 4. Data and money
- Metered by default on cellular (1.5.8's metered connections).
- Data used this month on each SIM, from the interface counters, kept across restarts; a monthly limit with a warning
  at 80 % and, if chosen, cellular data off at 100 %; the month's start day set by the person.
- Updates on cellular: never automatic; offered with the download size ("Download over mobile data: 42 MB").

### 5. Signal and status
- The taskbar shows signal bars, the network type (5G, LTE, 3G) and the operator, from ModemManager; Settings shows
  the signal's details (RSRP, RSRQ, SINR) for placing the computer or an antenna.
- System health: modem events (SIM removed, PIN blocked, no network, the module restarted).
- The self-test: the modem found, its driver, the SIM, registration, a data connection.

### 6. Messages (SMS)
- A Messages app: conversations, read and send, delivery reports, notifications for new messages (also with the
  desktop locked: the sender and "new message", not the text, unless the person allows it).
- Messages kept in the account's folder (`.rudolf/messages`), and in the encrypted folder when "Encrypt my data" is on.
- Long messages and Unicode (multi-part SMS); MMS is left out (it needs the operator's MMS gateway and a separate APN).

### 7. ARM64 (from 1.5.9's build)
- The first published ARM64 release: its own update list (`beta/arm64/`, `stable/arm64/`), a disc image for UEFI ARM64
  computers, and the same parts 1 to 6. Proven in QEMU aarch64 and on one ARM64 computer before it is published.

### 8. USB printers (moved from 1.5.9 on 2026-11-13)
Asked for: printing to printers plugged in by USB, as Rudolf OS prints to network printers now (rudolf-print: IPP
Everywhere and AirPrint, no CUPS and no drivers). The same print dialog (WordPro, SpreadsheetPro, PresentPro, and the
PDF Viewer since 1.5.9) lists them as "USB: name".

#### 8.1 Which USB printers
- **IPP over USB** (the USB printer class's protocol 4, 7/1/4; most printers sold since about 2015, and every one with
  the IPP Everywhere or Mopria logo): the printer speaks IPP over its USB link exactly as over the network. **ipp-usb**
  (Debian's, GPL-2) turns it into a local IPP address; rudolf-print then prints to it as to a network printer
  (PDF, or PWG / Apple raster from pdf.js). Nothing new to write for the job itself.
- **Plain USB printers that read PDF or PostScript** (7/1/1 or 7/1/2, the IEEE 1284 device ID listing PDF or
  POSTSCRIPT in its command set): the job is written to /dev/usb/lpN as it is. Through the kernel's usblp driver.
- **Others** (PCL-only or host-based printers that need a driver): not in 1.5.9. The dialog names the printer and says
  it needs a driver Rudolf OS does not have; PCL raster from pdf.js (as PWG raster is made now) is a candidate for
  later.

#### 8.2 What changes
- **Kernel:** USB_PRINTER (usblp) as a module, checked in kconfig.sh (x86 and ARM64). usblp must not claim an IPP over
  USB printer's interface 7/1/4 (ipp-usb needs it): the modem service's way (mdev with the interface's class) unbinds
  it for those.
- **The system layer:** ipp-usb and its libraries, with a budget checked in ci_release.sh (about 10 MB expected; the
  build says the real size, as for ModemManager). Avahi is not added: ipp-usb's address is passed to rudolf-print in
  /run/rudolf/usb-printers instead of being announced on the network.
- **/usr/lib/rudolf/usbprint** (mdev hotplug, like the modem service): a printer plugged in starts ipp-usb for it (IPP
  over USB) or records its /dev/usb/lpN and device ID (plain); unplugged, it stops it and forgets it. rudolfd's
  print/find adds these to the network's printers; print/job sends to /dev/usb/lpN for the plain kind.
- **Settings › Devices** lists a USB printer with its kind (IPP over USB, PDF, PostScript, or "needs a driver").
- **The self-test:** a "usb-printer" line (found, its kind, ipp-usb answering).

#### 8.3 Tests
- Host: the hotplug script with stand-ins (an IPP over USB printer's interfaces, a plain PostScript one, a PCL-only
  one); rudolfd's print/find listing them; a job to the plain kind written to a stand-in /dev/usb/lp0.
- rudolf-print against ipp-usb's stand-in (its test IPP server) as for network printers.
- **On real hardware:** the HP desktop's user's USB printer (on the HP desktop, or on the Vivobook if it can be
  borrowed), first read only (its interfaces and device ID: which kind it is), then a test page.

### 9. udev instead of mdev (added on 2026-11-13)
Why: mdev makes device nodes and runs scripts, but keeps no device database and tells no program about devices. Linux
software finds hardware through udev (libudev): ModemManager (1.5.9 needed a workaround: #280), libinput, SDL and
Steam's controllers, PipeWire, Waydroid and many Flatpak apps. Each gap became a workaround of its own; 1.6.0's
modems and the plans after it (Waydroid, PipeWire, a native or Wayland desktop) lean on udev more.
- **Which udev:** systemd-udevd on its own (from Debian's udev package; no systemd as init) or eudev, whichever is
  smaller and keeps working in the build; measured against a budget in ci_release.sh. The hardware database (hwdb.bin)
  and the packages' rules (ModemManager's, libinput's, the uaccess/joystick ones) come with it.
- **Rudolf OS's own rules:** what /etc/mdev.conf does today (the modem service, devplug's USB and phone events, joystick
  permissions, drives, cameras, the netplug and thunderbolt scripts) becomes rules in /usr/lib/udev/rules.d/90-rudolf.rules
  calling the same scripts (their ACTION/MDEV environment mapped to udev's), so the scripts change little.
- **Start-up:** rudolf-init starts udevd early, triggers coldplug and waits for it to settle (with a time limit) before
  the display and the services start. Recovery mode keeps mdev as a fallback: if udevd does not start, mdev does, and
  System health says so.
- **What it lets go of:** the modem service's `--test-no-udev` and its port reporting (ModemManager finds its ports
  itself again), devplug's own guesses where udev's tags say what a device is.
- **Tests:** host tests of the rules with udevadm test on stand-in devices; QEMU (start-up time with udev against mdev,
  a USB drive, a keyboard and a controller plugged in later); on the Vivobook over the devlink: every device the
  self-test lists, the camera, Bluetooth, sound, a controller in Steam plugged in while it runs.

**Done (2026-11-15), in code; measured and checked on the Vivobook at its first beta** (asked for: "Implement the
transition to udev as we previously planned, and just plan checks and manual tests to ensure everything still works
properly"; no memory budget, as said):
- **Which udev:** systemd-udevd on its own and udevadm, from Ubuntu 24.04's udev package (the base layer's libraries
  come from it): its helpers (ata_id, cdrom_id, scsi_id, v4l_id, …), its rules, and the hardware database (hwdb.bin,
  built from its hwdb.d in ci_release.sh), with the libraries the base layer lacks (libsystemd-shared's at its own
  path). Left out: 71-seat and 73-seat-late (systemd-logind's), 90-vconsole and 99-systemd (systemd's), and
  80-net-setup-link (it would rename network interfaces; Rudolf OS's scripts know eth0, wlan0 and the adapters by the
  kernel's names). The groups its rules name (disk, lp, kmem, dialout, cdrom, tape, render, kvm, sgx) are in
  /etc/group (ci_release.sh for an existing base layer, mkrootfs.sh for a new one).
- **Rudolf OS's own rules:** /usr/lib/udev/rules.d/90-rudolf.rules, every line of /etc/mdev.conf that set an owner
  or ran a script: the same owners and modes (as numbers), the same scripts (sound, joyperm, rudolf-debian devices,
  modem, devplug, thunderbolt, netplug) through /usr/lib/rudolf/udev-run, which gives them mdev's MDEV (the name under
  /dev, or the interface) beside udev's ACTION, SUBSYSTEM, DEVTYPE, DEVPATH, and starts them in a session of their
  own so a slow one (mounting a drive, starting ModemManager) never holds up udev. Drivers are loaded by udev's
  80-drivers.rules (mdev's modprobe line). The scripts themselves did not change.
- **Start-up:** rudolf-init starts udevd (`--daemon --resolve-names=early`), checks it answers, announces the devices
  already there (subsystems, then devices) and waits for them, 15 s at most. If udevd does not start or answer, if
  udev is not in the build, or with `rudolf.udev=0` on the start-up line, mdev does as before. /run/rudolf/devices
  says which ("udev", or "mdev (why)"); the self-test's new **devices** line fails on the fallback (skipped when asked
  for), and when udevd stopped or its database is empty.
- **What it let go of:** the modem service starts ModemManager with udev as on other Linux (no `--test-no-udev`, no
  reports of each port: it finds them through udev itself); with mdev, as in 1.5.9. Debian gets udev's database
  (/run/udev, read only), so Steam and SDL find controllers as on other Linux; Flatpak binds it into its sandboxes
  itself. devplug's own guesses were left as they are.
- Tests: qt/host/udev60_t.sh (13 here: rudolf-init's own lines with stand-ins, its four ways to mdev, udev-run, every
  mdev.conf script in a rule, the modem service, the self-test's line; with udevadm in CI also udevadm verify and
  udevadm test on the runner's devices), .github/workflows/udev-check.yml.

**Checks and manual tests for the first 1.6.0 beta** (on the Vivobook over the devlink, and by the owner where said):
1. **Start-up:** /run/rudolf/devices says `udev`; the self-test's devices line passes; /run/rudolf/udev.log has no
   errors; start-up to the desktop no slower than 1.5.9's (System health › Last starts: 39 s on 2026111503).
2. **Every device the self-test lists:** its lines all as on 1.5.9 (sound, Wi-Fi, Bluetooth, the camera, the GPU,
   storage, the listeners); `ls -l /dev/input /dev/dri /dev/snd /dev/video* /dev/hidraw*`: the owners and modes as
   under mdev (input root 0640, controllers 1000, dri 0666, snd audio 0660, cameras 1000).
3. **Network:** the interfaces keep their names (wlan0, eth0, usb0); Wi-Fi connects; the MiFi over USB gets an
   address (netplug) and shows in Network Settings; unplugged, it goes.
4. **Sound:** speakers and microphone; headphones in the jack and out; a USB or Bluetooth headset comes and goes (sound
   hotplug).
5. **A USB stick** plugged in, mounted and shown in the File Explorer, safely removed; an SD card if the computer has a
   reader; another Rudolf OS stick (read only, 1.5.9's 14.4).
6. **The camera** in Settings and in Meet (owner); in a Linux app (Debian or Flatpak).
7. **A game controller** plugged in later: in Rudolf's games, and in Steam while it runs (owner); Steam Input.
8. **Keyboard, touchpad, a USB mouse** plugged in later; the brightness and volume keys (rudolf-display).
9. **The modem** (the USB stick when there is one): ModemManager started with udev, `mmcli -L` lists it, the
   self-test's modem line passes; unplugged, it stops.
10. **Thunderbolt or USB4** if the computer has it (the test laptop): a device approved or asked about.
11. **Sleep and wake** (the lid): devices back, Wi-Fi reconnects.
12. **The fallback:** one start with `rudolf.udev=0` (Developer mode's start-up options, or the boot menu's e): mdev,
    everything above still works as on 1.5.9, the self-test's devices line skipped.
13. **Memory at idle** on the Vivobook, read once with and once without (rudolf.udev=0), for the record only.

### 10. A Linux user for each account (added on 2026-11-13)
**Moved into 1.5.9 on 2026-11-14 (docs/plan-1.5.9.md Part 15.3), as part of its security work.**
Why: every Rudolf OS account runs as the same Linux user (rudolf, uid 1000). The desktop keeps the accounts apart, but
Linux apps, Wine and Debian run as that one user and can read every account's files (the vault's own note says so).
Conventional distributions give each person a Linux user of their own.
- **Users:** each account gets its own Linux user (uid 1001 and up), made when the account is made, removed when it is
  removed; its home folder (/home/NAME, still encrypted with the account's key) owned by it and readable by nobody
  else. The rudolf user stays for the desktop engine itself (the page and rudolfd's helpers).
- **What runs as whom:** at sign-in, rudolfd starts the account's session pieces as its user: Flatpak apps, Debian and
  Wine, the session D-Bus, PulseAudio (or PipeWire later), the portal; signing out stops them. Two accounts signed in
  one after the other never share a running app.
- **Shared things:** Downloads and the files the desktop page keeps per account move into each user's home; groups
  for the devices people use (video, audio, input for controllers, the ADB phone) replace today's chmod to uid 1000.
- **Moving existing computers:** the first start of 1.6.0 gives each existing account its user and moves its files
  (ownership only, no copying), checked and logged; anything that cannot move stays readable to its account and is
  named in System health.
- **Tests:** two accounts on the host and in QEMU: one cannot read the other's files from a Flatpak app, a Debian
  terminal or Wine; signing out stops the account's apps; the move on an existing data partition.

### 11. Sessions and admin prompts for Linux apps: elogind and polkit (added on 2026-11-13)
Why: Linux apps ask login1 (systemd-logind) whether to keep the computer awake (a game, a video, a download), who is
signed in and which devices the person may use; and they ask polkit for an administrator's password before doing
something to the system. Rudolf OS has neither, so these requests fail or are ignored.
- **elogind** (logind without systemd): one seat, the signed-in account's session (with Part 10, its own user); apps'
  inhibitors (sleep, idle, lid) are honoured by Rudolf OS's own power code, which already holds off sleep for video,
  calls and games; rudolfd reads them through elogind instead of guessing.
- **polkit**, with an agent drawn by the desktop: an app's request for an administrator shows the desktop's own
  password prompt (the one Settings uses), and only administrators' passwords are accepted. Rules kept short and
  read-only in the system layer.
- **Budget:** both together under 5 MB in the system layer, checked in ci_release.sh.
- **Tests:** an app's sleep inhibitor keeps the computer awake and the screen saver away until it is released; an
  admin request from a Flatpak or Debian app shows the prompt and is refused for a standard account.

**Done, first step (2026-11-15): apps keeping the screen on or the computer awake**, without elogind:
- **Where apps ask:** looked at before building: Flatpak apps ask the portal (org.freedesktop.portal.Inhibit); Chromium
  and Electron apps, SDL games, Steam and video players ask org.freedesktop.ScreenSaver or
  org.freedesktop.PowerManagement.Inhibit on the session bus. Debian's programs and Flatpak apps have no system bus
  on Rudolf OS, so login1's Inhibit (what elogind would add) is not where Linux apps here ask.
- **rudolf-portal** (on each account's session bus, and the desktop user's) answers all three: Inhibit, UnInhibit,
  the portal's Inhibit and its Request's Close (flags 4 sleep and 8 idle kept; log-out and user-switch are not held),
  ScreenSaver's GetActive and the rest. Each held request is a file in /run/rudolf/inhibit (rudolf-init: 1733),
  UID-PID-COOKIE with its flags, the app (a Flatpak app by its ID) and its reason; it goes when the app lets go or
  leaves the bus, and a portal started again clears its own.
- **What they hold:** rudolfd's `GET media` adds `idleHeld`, `sleepHeld` and `heldBy` (inhibit_scan: only a file that
  is its owner's and whose portal still runs). The screen held: no screen saver, no idle lock, no sleep (the page's
  idleCheck, desktop/inhibit60.js), and the screen stays on (rudolf-display); the page log says which app holds it.
  The computer held awake: no sleep, the saver and the lock as usual. Closing the lid still sleeps (the person's own
  choice, as on other desktops).
- Tests: qt/host/inhibit60_t.sh 12 (rudolf-portal on a private bus with fixtures/inhibitclient.c: each of the three,
  let go, an app leaving the bus, flags, a portal gone and started again, a file of the wrong owner; rudolfd's
  inhibit_scan, fixtures/inhibitscan.c), qt/shell2/inhibit60_t.js 6; call59_t.js, saver59_t.js, saver_t.js, portal_t.sh
  unchanged.

**For the owner to decide: elogind and polkit themselves.** Found while building the step above:
- elogind's other parts (sessions, seats, "who is signed in") are asked for by system services (GNOME's, NetworkManager,
  polkit), not by the Linux apps Rudolf OS runs: Flatpak apps and Debian's programs cannot reach the system bus here.
- polkit: a Linux app's request for an administrator goes, on Rudolf OS, to Debian's own world (Debian has its own
  user, password and sudo) or is not possible from a Flatpak sandbox; polkitd on the system itself would have nothing
  asking it. It would also need its agent to answer as root (polkit-agent-helper-1, setuid) with a password the
  accounts' Linux users do not have (theirs is the desktop's sign-in), so rudolfd would answer it.
- So the proposal: leave elogind and polkit out of 1.6.0 (and the 5 MB budget with them), and take them up only if an
  app or a later part (Waydroid, PipeWire, a Wayland desktop) needs them. Udev's seat rules (71-seat, 73-seat-late)
  stay out with them.
- **Decided (2026-11-15): left out**, as asked ("If leaving out elogind and polkit causes no issues or possible
  conflicts with Rudolf or its cross-distribution compatibility, then let's leave it out"). Checked first: nothing in
  Rudolf OS asks for either (no program here uses login1 or polkit; the system bus serves only BlueZ, bluealsa and
  ModemManager, which run without them); a Flatpak app with `--system-talk-name=org.freedesktop.login1` gets no
  answer, as it got none before, and such apps (a few use login1's sleep signal) carry on without it; Debian's
  programs never had a logind; nothing that ran before changes, and apps made for other distributions keep the
  standard ways they use here (the portals, the Secret Service, ScreenSaver and PowerManagement). Part 11 is done.

### 12. The native desktop becomes the default (added on 2026-11-13)
Decided on 2026-11-13 from 1.5.9's measurements (docs/plan-1.5.9.md 12.3a: at idle 854 MB used against Standard's
1,110 MB and Lite's 952 MB; the desktop up 9 s sooner than Standard). The shell (rudolf-native) gets everything the
web desktop's shell has, and Native becomes the engine a new installation and an update start with; Standard and Lite
stay, chosen in Settings › System health › Desktop engine, and Standard stays what a Native that gives up falls back
to. The apps stay web pages in one-app WebKit windows (1.5.9 Part 12) until 2.0 moves them (docs/plan-2.0.md).

**The rule (the owner, 2026-11-14): on Native, Rudolf OS appears and works exactly as it does on Standard
(Chromium).** Native is a cheaper way of drawing the same desktop, not a different desktop:
- **Looks:** every part the shell draws (wallpaper, taskbar and its buttons in each style, start menu, tray and its
  pop-ups, clock, desktop icons and their labels, menus, title bars and window frames, sign-in and lock screens,
  notifications) is compared with Standard's own drawing of the same state: the same page settings, the same screen
  size, both screenshotted under Xvfb (Standard: the page in Chromium; Native: rudolf-native with one-app windows).
  The same sizes, positions, colours, fonts (the page's font, sizes and weights: Pango given the same family), corner
  radii, shadows, gradients and icons. A test per part, `iso/qt/native/same_t.sh`, measures the difference: it fails
  over 2 % of pixels differing by more than a small tolerance (text anti-aliasing between Chromium and Pango is the
  only expected difference), and keeps both pictures and the difference picture for the PR.
- **Works:** each behaviour is checked against Standard's: the same clicks, right-click menus and their items in the
  same order, keyboard shortcuts, drag and drop, animations and their timing, hover and pressed states, what is
  remembered and where. A checklist per item, made from the page's code and its browser tests, goes in the PR with
  each line ticked on both engines.
- **Every difference is a bug**, fixed in Native (never by changing Standard to match), unless the owner agrees to it.
  Items already merged (1.5.9 step 7, item 1's first part) are checked against this rule too, before item 2.
- The apps already are the page in one-app windows; their remaining differences (WebKitGTK against Chromium: fonts,
  scrollbars, form controls, select pop-ups) are found the same way and fixed in the page's one-app CSS or in the
  window manager.

**What the shell gets (each against the page's behaviour, with screenshots compared under Xvfb):**
1. **The desktop:** icons (the page's bd95icons: arranging, renaming, opening, the right-click menu, Properties),
   drag and drop of files onto it and between windows, and the desktop's right-click menu (New, Display Properties).
2. **Widgets** (the page's widgets: clock, weather, notes, system), drawn natively, their settings read from rudolfd.
3. **The taskbar complete:** the pinned apps' and open windows' right-click menus as the page's, with each app's own
   entries (Oracle's New private tab, 1.5.9), left and right positions, auto-hide, the workspaces' buttons, the notifications'
   pop-ups and their centre, the volume and network pop-ups (not only their Settings pages), the calendar under the
   clock, the battery's menu, the 🎙️ indicator during calls.
4. **Workspaces:** as the page's (several, Super+Ctrl+arrows, a window moved between them, the taskbar's list per
   workspace).
5. **The window keys:** Super+Tab and Alt+Tab with previews, Super+arrows (snap left and right, maximise), Super+D,
   Alt+F4, Win+L; the page's global shortcuts (desktop/shortcuts from rudolfd, as the settings).
6. **The screen saver and its lock:** the idle count from rudolf-display, the screen savers themselves still the
   page's (one full-screen one-app window `#saver=NAME`), the lock screen native, not during a call or full screen.
7. **Several monitors:** the wallpaper and a taskbar on each, windows moved between them (RandR).
8. **The Phone screen and Blitzen's panel:** as windows of their own (one-app mode) docked as the page docks them.
9. **Tablet mode and the on-screen keyboard** (1.5.9 Part 2) under the native shell.
10. **Accessibility:** the screen reader reads the shell (ATK through GTK), the magnifier, high contrast, large
    pointer, keyboard-only use of the taskbar and the start menu.
11. **Game mode, remote access, the devlink's screenshot, Wine and Linux apps** checked unchanged under it.
12. **Memory:** each one-app page runs only its app's code (the page's other modules not started in one-app mode),
    measured again: one app open within 120 MB of idle.

**Status (2026-11-14, unattended sessions):** 1.5.9's step 7 (Native (test)'s six Vivobook bugs, PR #311) and item 1's
first part (the desktop's icons, PR #314) are merged. Then "The rule" for what was merged (the next PR): the comparison
test `iso/qt/native/same_t.sh` (with same_rd.py, a stand-in rudolfd serving one state to both engines; same_state.js;
pxdiff.py): Standard (the page in Playwright's Chromium) and Native (rudolf-native under Xvfb) at 1920x1080, each part
cut out of both screenshots and compared (fails over 2 % of its pixels differing by more than 32 in a colour; the
pictures kept). 39 parts in 28 cases pass: the sign-in screen, the wallpaper (colours, a gradient), the taskbar in the
five styles, the five button styles, the accent colour's three modes, at the top, hovered (standard and Candy), the
start button, the pinned apps, the tray and the clock (12 and 24 hours), the desktop's icons (the taskbar at the bottom
and the top), the start menu, the title bars in the five styles and the Negative mode (with the taskbar's window
button), the lock screen. Native now draws these with the page's CSS ported part by part (a box painter with the
page's colours, gloss, edges, corners and shadows; Pango with the page's font family and Chromium's text placement);
the start menu's list comes from the page (desktop/apps "menu"); an app's window gets the page's size and place
(oneapp59.js's "size"); the frames have the page's 1 px edge, round corners (X shapes), resize handles and their shadow
on the desktop; Meta+L locks; Start › Shut Down... is the page's Power dialog; rows the shell cannot do itself open the
page's action in a one-app window (#app=do-NAME). Known differences left (each a bug for its item): the start menu's
search filters the list (the page also searches settings and files, in a results list); the menu's opening animation;
the tray's pop-ups, the calendar and the workspaces (items 3 and 4); the right-click menus of the taskbar, the start
button and the window buttons; a window's shadow falls only on the desktop (no compositor), and Wireframe's blur
behind the taskbar only shows over a picture as no blur; the dark theme's menu is drawn but not yet compared.
The comparison test is merged (PR #316). Item 1's second part, first half (PR #317, on main): the
Desktop folder's files and folders as icons in the page's places; the page's right-click menus (the desktop's, a
file's, the Trash's, an app's: items, order, keys, disabled rows, hover, the focus ring as Chromium shows it) and its
Rename and Open with dialogs, drawn by the shell; what they do is done by the page itself in a hidden one-app window
(#app=do-desk&on=ICON&item=WORDS clicks the page's own menu item; #app=do-fs&op=rename|openwith), which answers the
shell ("done": an error to show, or a dialog to ask) and posts the catalogue again. An app's window (and a hidden page)
now lays the desktop's icons out at the screen's size, not its own (it used to squeeze the places kept for the shell).
same_t.sh: 44 parts in 33 cases pass (new: desk-files, desk-menu, file-menu, file-rename, file-openwith).
Item 1's second part, second half (PR #320, on main): dragging. A desktop icon follows the
pointer; a file dropped on an app's window opens in that app in a window of its own (a page asked by the shell,
#app=do-fs&op=drop, doing what the page's __dropFileOnWindow does), a file or an own shortcut dropped on the Trash goes
there; an Explorer window's files dragged onto another window (another page) are dropped there (they go with the drag
as text/x-rudolf-paths and become the target page's FC.drag). Found on the way and fixed: the windows' pages did not
share their files (each page has its own copy; a page that saves now tells the others through a BroadcastChannel and
they read the files again); an app's page kept the size GTK first asked for, larger than its window, and was not
repainted where a window uncovered it (the shell selected its own X events on its windows, replacing GDK's on the same
connection); the shell could crash when a window closed before its frame. native59_t.sh 70/70 (new 67-70).
Known differences left: files dropped from outside Rudolf OS onto the desktop (Standard saves them in the Desktop
folder; the shell does not take X drops yet); the desktop menu's Show all windows (item 5).
Item 2, the widgets (the PR after #320): the page's five widgets drawn by the shell (desktop/shell25.js WTYPES and
wgDraw, styles25.css .wgt: each into a picture of its own with its shadow and the desktop behind it blurred 6 px, laid
over the desktop), where the account left them (bd95widgets:ID through desktop/settings; what the shell changes kept by
rudolfd, desktop/widgets, with the page's value it replaced, until the page's own value changes: an app's page takes the
shell's list into the page's storage at sign-in, so Standard shows the same); dragged by the header, removed by the ✕,
added with the desktop menu's Add a widget… (the page's picker, drawn by the shell, the new one where the page puts it);
the clock and calendar (System Settings' time zone, weeks from Sunday), the system monitor every 3 s (rudolfd's stats;
the windows counted), the weather from Open-Meteo with the page's place (bd95wloc, else the time zone's city, looked up),
unit, codes, retries (20 s, 1, 2, 5 min) and 15-minute look-ups (a click opens Weather; RUDOLF_WX_BASE for tests), the
note as the page's textarea (typing through GTK's input method, Backspace, Delete, Enter, the arrows, Home and End with
Shift and Ctrl, Ctrl+A, Ctrl+C/X/V, Ctrl+Z/Y with what comes back selected, the mouse placing the caret and selecting, a
double-click a word, the right-click's menu as the page's textMenu, the resizer, scrolling to the caret; saved half a
second after the last change); the pointer's shapes. App windows' pages draw no widgets (no timers in every window).
same_t.sh: 56 parts in 39 cases pass (new: widgets (all five), widgets-ask (the weather asking where, an empty note),
widgets-top, widgets-over (over the icons and a gradient: the blur), widget-picker); the behaviour on both engines:
`SAME_SCRIPT=wgbeh_t.js sh same_t.sh` 12/12 (dragging and where it is saved, typing, Backspace and Enter, Ctrl+A, Ctrl+Z,
the note's menu and its greyed rows, no menu on a header, the ✕, the picker and where a new one goes, the weather's click,
the shell's list reaching the page's storage). deskapi59_t.sh 40/40 (new 38-40), native59_t.sh 70/70.
Known differences left (item 2): a long note scrolls without Chromium's scrollbar; a widget dragged over the desktop's
icons covers a dragged icon (the page draws a dragged icon above the widgets); the weather's guessed city (from the time
zone) is kept by the shell for the session only (the page saves it as bd95wloc); the textarea's height after resizing is
kept until the widgets are drawn again, as the page's.
Item 3, first part (the PR after item 2's): the taskbar's right-click menus, drawn by the shell as the page's cmenu,
row by row, placed as the page places them (above the pointer when they would not fit below; a window's menu where
the page put it before adding its Print row): the taskbar's (barMenu: Show desktop, Restore all windows, Cascade
windows, Close all windows, Taskbar settings, greyed as the page's), the start button's, a pinned app's (with the
app's own rows from desktop/apps "jumps", private59.js APP_JUMPS: Oracle's New private tab, opened as
#app=browser&jump=New private tab, its window the page's private one), a window's button's (read from the window's own
page, oneapp59.js __oaWinMenu: the page's rows, Print / Save as PDF… and Force quit among them; Restore, Minimize,
Maximize, Always on top (the window kept above the others, its tick), Center on screen, Close and Force quit done by
the shell, the page's own rows by the page, __oaWinMenuDo), the sound's (Mute, as #app=do-mute), the network's and the
clock's (Settings at the page's section: #app=settings&page=PAGE); unpinning as #app=do-pin. same_t.sh: 65 parts in 48
cases (new: tbm-bar, tbm-start, tbm-pin-notepad, tbm-pin-browser, tbm-vol, tbm-net, tbm-clock, tbm-win, tbm-bar-win);
the behaviour on both engines: `SAME_SCRIPT=tbbeh_t.js sh same_t.sh` 10/10 (Maximize, Restore, Minimize, Restore all
windows, Always on top and its tick, Unpin from taskbar, New private tab, Command Prompt, Close all windows: the
menus' rows and the windows and pins after each).
Known differences left (item 3): a window's Move to workspace N rows do nothing yet (item 4; done in the workspaces' PR).
Item 3, second part, first half (the PR after #325): the tray's pop-ups drawn by the shell as the page's, in a window of
their own (as the menus): the calendar under the clock (shell25.js #calp: the time, the long date, the month with ◀ and
▶, the weeks from Sunday with the columns as Chromium's automatic table layout shares them out, today in the accent
colour, days with the Calendar's events (rudolfd's desktop/settings now gives them: "cal", bd95cal:ID) in bold and
underlined, the next four events or "No upcoming events", Open Calendar, a day double-clicked opening the Calendar at it
(#app=calendar&day=DATE); the dark theme's colours; at the top with the taskbar there) and the volume's (osd31.js
sysVolPop: Chromium's range input and checkbox drawn as Chromium draws them, the slider setting rudolfd's volume as it
moves, Mute, Desktop sounds… opening Settings' Volume Controls, placed over its button as flyPlace places it). A click
outside closes them; one on the taskbar goes on to it (one pop-up gives way to the other); the clock again closes the
calendar, except on its date, where the page's calendar closes and opens again at this month (the page's flyClose
treats the date's <small> as outside the clock). same_t.sh: new cases fly-cal, fly-cal-next, fly-cal-dark, fly-cal-top,
fly-vol; the behaviour on both engines: `SAME_SCRIPT=flybeh_t.js sh same_t.sh` 23/23 (19 steps compared, with the
page's own outcomes). Known differences left: the slider's own sound when let go (sfx('notify')), and the slider and
the checkbox by the keyboard. (PR #328, merged.)
Item 3, second part, second half (the PR after #328): quick settings (☰, shell25.js #qsp: the page's eight switches as
tiles, three to a row, as tall as their names; the network's line (net31Desc); Lock, Settings, Power) and the
notifications' centre (🔔, #ncp: Do not disturb, Clear all, each notification with its time and ✕, a failure's red
edge and Blitzen's ⚡ Why? as bzWhyBtn decides; the list scrolled with the wheel past 70 % of the screen's height),
drawn by the shell as the other pop-ups; the battery opens Settings' System health (pwrShow). A switch, Do not disturb,
✕, Clear all and Why? are the page's own: a page does them (#app=do-qs&item=WORDS, #app=do-nc&op=...; Why? in a window
of its own), the shell shows the change at once and reads the page's storage back after. rudolfd's desktop/settings
gives the switches (nl, dnd, sr, mag, cb, vk, touch) and the notifications' list ("notif"). same_t.sh: new cases
fly-qs, fly-qs-on, fly-qs-dark, fly-nc, fly-nc-empty; the behaviour on both engines: `SAME_SCRIPT=qsbeh_t.js sh
same_t.sh` 15/15 (14 steps compared: the switches and what the page keeps, a notification dismissed, Do not disturb,
Clear all, one panel giving way to the other, Settings). Known differences left: what some switches do to the screen
(Night light's tint, Touch mode's sizes, High contrast, the magnifier, the screen reader, the on-screen keyboard: the
shell keeps and shows the switch; their effects are items 9 and 10); the centre's scrollbar; the notifications'
pop-ups (toasts) as they come; the network's pop-up (its Wi-Fi list), the taskbar at the left or right, auto-hide, the
workspaces' buttons and the 🎙️ indicator are item 3's last part.
Item 3's last part, first half (the PR after #340; #336 and #339 ported onto main, 2026-11-14: #336 was closed unmerged
because its quick settings and notifications' centre were a second implementation of #332's, and #339 was merged only
into #336's branch; their other parts are here, on #332's quick settings and centre, nothing of #336's second
implementation brought back): the network's pop-up (🌐, net31.js net31Pop, .np: the adapters, the one in use in the
accent colour, the others tinted when hovered, "This system has no Wi-Fi tools…", Network Settings…, placed by flyPlace
before the Wi-Fi part is filled in); the notifications' pop-ups (an app window's page sends its toast to the shell,
oneapp59.js wrapping __notifCentre.add: op "toast", and "note" so the centre and the bell have it at once, kept 15 s until
rudolfd's answer has it; drawn as the page's .ntoast in the screen's corner, 12 px from the right, 56 px from the bottom
or the top, the newest at the bottom, 8 px apart, ntin's .25 s in, .35 s out, in a window that takes clicks only on the
toasts; a click does the notification's own action in its page (__oaToastAct), else opens the centre; ⚡ Why? asks
Blitzen as the centre's own does); the taskbar's 🎙️ (call59.js: rudolfd's mic every 5 s, a click muting the microphone
for everything, 🔇); the bell as the page's (🔕 with Do not disturb; its count the page's ncUnread: those come since
signing in or since the centre was opened, not all kept); Meta+N and Meta+A open the centre and quick settings; a page's
settings saved in one window reach the other windows (the storage event: applySet, a11yApply, themeApply), as Standard's
one page. Fixed on the way: a notification dismissed or cleared in the centre came back with an older answer of
rudolfd's (kept gone for 15 s now); the shell's dark pop-ups and menus followed the dark theme's switch alone (S.darkui),
not the light or dark mode (the page's html.dark: darkui !== false and dynResolve's mode); Settings asked for at a page
by another app's window (a notification's action, settings('health')) opens Settings' own window there (op "open" with
"page"). same_t.sh: new fly-net, toast, toast-two (and the bell's count), toast-dark, toast-top, call, call-muted; the
behaviour on both engines: `SAME_SCRIPT=toastbeh_t.js sh same_t.sh` (a toast, its click doing its action, one going by
itself, none with Do not disturb but counted, the 🎙️ muting and back). Known differences left: with Wi-Fi tools (a real
computer's Wi-Fi), the network's pop-up does not have the page's Wi-Fi part (networks in range, joining one, saved
networks: Settings › Network has them); rudolfd's events (events/wait) go to the page that asks first, so with no app
window open a page's event-driven notifications (memory, Bluetooth, the power button's dialog) are not seen until one is
(a hidden page for the desktop's own events, item 11); the toasts' window shows what was behind it when it appeared (no
compositor).
Item 3's last part, second half (the PR after #344): the taskbar at the left or the right (Display Properties' Taskbar:
left, right), drawn by the shell as the page's body.left/.right #bar: a column from the screen's top to its bottom, 3 px of
padding, its parts 4 px apart and as wide as the column, the column as wide as its widest part (max-content: the pinned
apps' row, the version under the logo, the clock...; measured with the page's fonts, bar_measure); the start button the
logo over the version (10 px, centred); the pinned apps in a row; the open windows' buttons 22 px high one under the
other; at the bottom the battery, the workspaces, the network, the sound, the call's 🎙️, quick settings, the
notifications and the clock (its date hidden, as the page's). The desktop's icons, the widgets and the windows' first
places beside it (#desk's inset, var(--bw): the bar's width and 4 px), maximised windows and Cascade in the rest of the
screen, the start menu beside it at the top (left or right: var(--bw), top 4 px), the network's and the volume's pop-ups
beside their buttons (flyPlace), quick settings, the centre and the calendar where the page puts them (at the bottom
right, as the page does); clicks outside a pop-up reach the bar wherever it is. The page has no auto-hide, so Native has
none. same_t.sh: new bar-left, bar-right (and the icons beside them), bar-left-win (a window's button and its title bar),
bar-left-candy, bar-right-retro, menu-left, menu-right, fly-vol-left, fly-net-right, fly-qs-left. Item 3 is done.
Item 4, the workspaces (the PR after #346): three, as the page's (shell25.js wsGo):
the taskbar's 1 2 3 buttons (the one shown on, those with windows marked as the page's .has) switch, Ctrl+Alt+Left and
Right go to the one before or after (with Shift the window in front goes too), a window's menu's Move to workspace N
rows move it (rebuilt per window, the page's rows), the taskbar lists the shown workspace's windows only, a window
brought to the front (a toast's action, Alt+Tab later) switches to its workspace, a pinned app open elsewhere opens a
new window here (as the page's pin click), and "Workspace N" is said in a toast (none with Do not disturb). The
shell's state file has ws and clientws lines for the tests. Fixed with it: a toast with no body is 2 px taller, as
the page's empty body's margin makes it. same_t.sh: new ws2, ws2-dark (the bar and the toast), ws2-left; the behaviour on both
engines: `SAME_SCRIPT=wsbeh_t.js sh same_t.sh` 11/11 (the workspace shown, the windows shown, the taskbar's windows,
the workspaces with windows and the menu's rows after each of ten steps). The bar
at the left or the right (#346) lists and marks them the same way.
Item 5, the window keys (2026-11-15, unattended session): the page's keyboard shortcuts done by the shell (shell25.js SHORT,
in its order: Super+E File Explorer, Super+D the desktop (this workspace's windows minimised), Super+R Run, Super+L lock,
Super+I Settings (the one already open brought to the front), Ctrl+Esc and Super by itself the start menu (metaAlone: Super
let go with nothing pressed between), Super+Tab the overview, Alt+F4 the window in front closed (Oracle's own Alt+F4 kept
Chromium's, 12.3c), Super+Up maximise, Super+Down restore or minimise, Ctrl+Alt+arrows and with Shift the workspaces (item
4), Super+N and Super+A (item 3), F1 Help), each with the account's own key from Settings › Keyboard (shell25.js kbMap:
bd95kb:ID, which rudolfd's desktop/settings now gives as "kb"; an empty one is the default, as the page's kbOf); the keys
grabbed on the root window, Super and the switcher's with the keyboard sent there while held (GTK's own windows take keys
through XInput 2, so a key pressed with Super held otherwise went to the window in front). Alt+Tab's switcher (#altab: the
windows in the order last used (each client's last focus, the page's MRU), the second one chosen, Tab and Shift+Tab, Alt+` too,
Escape, chosen when Alt is let go or clicked; each window's picture, title and "workspace N", the chosen one in the accent
colour with its words white; the box on half a pixel where translate(-50%,-50%) puts it, its words on whole pixels, as
Chromium draws them). Super+Tab's overview (overview(): this workspace's windows, minimised ones too, in ceil(sqrt(n))
columns over #desk (42 px above a taskbar at the bottom, 48 px below one at the top), each scaled to its cell less 40 by 60
px, never larger, with the 3 px ring in the accent colour and its shadow, over the desktop shaded rgba(15,23,42,.55); a click
picks one, Escape, a click elsewhere or Super+Tab again goes back; the windows' pictures through the Composite extension
(libXcomposite, loaded when there: each window redirected off screen only while the overview is shown). The page has no
"snap to the left or right" (Super+Left and Right), and its Alt+Tab shows the windows' pictures, not previews: the shell
does as the page does. Fixed with it: the start menu closed itself when a key's grab took the keyboard for a moment (its
focus-out now checks where the keyboard's focus went). same_t.sh: new altab, altab-accent, overview (107 parts in 75
cases pass; ws2's toast is timing-dependent on Standard, as on main); the behaviour on both engines: `SAME_SCRIPT=keybeh_t.js
sh same_t.sh` 21/21 (20 steps and the start menu open by Ctrl+Esc compared: the window in front, the windows open and
minimised, the maximised one, the start menu). Known differences left (item 5): F11, the page's own full screen, stays with
the window in front on Native; a key pressed while Super is held that is not a shortcut goes nowhere (the page's other
Super keys, Super+V, Super+Space, Super+. and the magnifier's, are items 9 and 10); the overview's pictures are taken once
when it opens (the page's windows keep moving under it).
Item 6, the screen saver and its lock (2026-11-15, unattended session): the shell looks every 15 s as core25.js idleCheck
does: the time since the last input (rudolfd's idle: rudolf-display's count), Display Properties' screen saver and Power
settings (bd95sys, which rudolfd's desktop/settings now gives as "sys": on, after how long, the style, the clock on it, "lock
when it ends", lock after, sleep after) and what holds it off (rudolfd's media: sound playing, a microphone in use, an app
keeping the screen on (Part 11's Inhibit); a window full screen; a minute after unlocking, UNLOCKED_AT). The saver itself is
the page's (saver31.js ssRun) in a window of its own (#app=do-saver&style=NAME&time=0|1, oneapp59.js), which the shell shows
full screen above everything, the taskbar too, and leaves off the taskbar and Alt+Tab; input (rudolfd's idle under 1.5 s,
looked at every second, as ssRun) closes it, and the lock screen follows when it ran more than 3 s with "lock when the screen
saver ends" (saverStop). Lock after N minutes and sleep after the power source's time (Power and battery's, else suspmin)
as the page. Fixed with it: every app window's page ran idleCheck too, so on Native each would have started its own saver
(inside its window) and locked: one-app pages no longer do (the shell does it once). same_t.sh: new saver (the stars full
screen over a desktop: 1.18 %); the behaviour on both engines: `SAME_SCRIPT=savbeh_t.js sh same_t.sh` 7/7 (an hour idle with
sound playing, a microphone, an app keeping the screen on: no saver; then the saver; input after 4 s: the saver ends and
the screen locks; unlocked; idle again at once: no saver for a minute). Known differences left (item 6): Display
Properties' Preview button runs the saver inside Settings' own window on Native (the page's preview is full screen); lock
after N minutes and sleep are not in the behaviour test (they need a minute after unlocking each).
**Item 7 needs the owner's decision (2026-11-15):** the page has no several-monitor desktop (Standard shows one desktop on one
screen; rudolf-display gives Chromium one output), so "a taskbar and the wallpaper on each monitor" on Native would make Native
differ from Standard, against "The rule". Either Standard gets it too (a change to Standard, which the rule forbids without
the owner), or Native keeps one desktop as Standard and item 7 is dropped or moved to 2.0. Left undone until then.
**Next:** item 8 (the Phone screen and Blitzen's panel: both are already page windows, so Native shows them in one-app windows
and scrcpy's own window framed as a Linux app's; what is left is checking them against Standard with same_t.sh), then items 9
to 12 in order, one PR each, based on main, each passing same_t.sh for its part, and docs/plan-2.0.md after them. Native is not
the default yet. The security line's next steps (as of 2026-11-14): docs/plan-1.5.9.md 15.3's third step (the QEMU check
with two accounts, needing a host with QEMU and a built disc) and 12.3c's second part (Oracle on Native: step 5's rest).

**Becoming the default:** after items 1-12 pass on the host and on the Vivobook (a week of betas on Native), the
release's default engine is Native for new installations; an update keeps the engine the computer had, and Settings
offers the change once ("Rudolf OS's new desktop uses less memory and starts faster"). Native's safeguards stay
(1.5.9 Part 12 step 5). The release check measures Native at idle on the Vivobook and fails over 900 MB used.
**Prepared (2026-11-15, unattended session), for the owner's decision:** the switch, in its own PR, not to be merged before
the week of betas on Native above (items 1-12 are not all done yet: see the Status above). A new installation starts with
Native (install.sh writes the choice into the new data partition when the system has rudolf-native and Chromium; at its
first start it is on trial as any chosen engine, and the session goes back to Standard if it does not come up); an update
keeps the engine the computer had (nothing is written by rudolf-update; no choice is Standard, as before); a computer
updated on Standard is offered Native once, after an administrator signs in: a notification ("Rudolf OS's new desktop
uses less memory and starts faster…") whose click opens Settings at the desktop engine (rudolfd's desktop/engine "offer":
Native there, Standard in use, never given up on this computer, an installed Rudolf OS, not offered before; POST
offered=1, and any engine chosen in Settings, keep it as made in /persist/etc/rudolf-native-offered). Settings names it
Native (no longer "Native (test)"). Tests: qt/host/nativedefault60_t.sh 12, qt/shell2/engine57_t.js 20 (new 4f-4i).
Not done here: the release check's 900 MB measurement on the Vivobook.

### 13. Signed system extensions (added on 2026-11-14)
Asked for: a way to add system parts (a driver, firmware, a tool, a printer filter) to one computer without a new
release, instead of a conventional package manager. Decided (2026-11-14): not a package manager that changes the base
(apt or pacman style: every computer would end up different, updates could fail half-way, the base's signature would
no longer cover what runs, and rollback would stop being clean), but **signed system extensions**: read-only layers
over the unchanged base, as systemd's sysext and Fedora's image-based editions do. The base stays one signed image,
the same on every computer, with its a/b spare and recovery.

#### 13.1 What an extension is
- **An rpk package of a new kind, `sysext`** (iso/sys/rpk.c: the same file format, download, signature check,
  revocation and `rpk install/remove`), holding files for the system: usr/bin, usr/lib (programs and libraries),
  lib/firmware, lib/modules/KVER/extra (kernel modules), usr/lib/udev/rules.d (after Part 9), usr/lib/cups or
  usr/lib/rudolf/print filters (Part 8), and at most one start script, usr/lib/rudolf/ext.d/NAME.start.
- **Signed only by a system key** (/etc/rudolf/rpk-keys: the release key, RPK_SIGNING_KEY, used by the workflow that
  builds them). A Library or developer key can never sign one (rpk refuses, as it refuses Library programs today).
- **Tied to the base it was built for:** PKGINFO `base = ABI` (a hash of the base layer's libraries, written into the
  base as usr/share/rudolf/base-abi by ci_release.sh) and, for kernel modules, `kernel = KVER`. Kernel modules are
  built in the same run as the kernel, so they carry its module signature.

#### 13.2 How it is put in place
- **At each start, before the desktop** (the initramfs, or early in init): every installed, enabled extension that
  matches this base's ABI (and kernel) is mounted read-only at /run/rudolf/ext/NAME, and the system's /usr,
  /lib/firmware and /lib/modules/KVER/extra become read-only overlays (overlayfs, lower layers only: the extensions
  over the base, in a fixed order by name). Nothing is ever written into the base.
- **A file the base already has may not be replaced** (checked while assembling): an extension adds, it does not
  override, so one cannot change what the signed base does. A clash leaves that extension out, named in System
  health.
- **Start scripts** run as root once the overlays are up (only from system-signed extensions), each with a time
  limit, their output in the health log.
- **Added while running:** a new extension's files are available after a restart, said in the install dialog; a
  firmware or kernel module extension may offer "Restart now".

#### 13.3 Safety
- **A bad extension cannot stop Rudolf starting:** the first start after installing one is a trial (as Native's): if
  the desktop does not come up, the next start leaves out the extensions installed since the last good start and says
  so. The start menu's recovery entry and Recovery mode always start without extensions.
- **Updates:** before switching slots, rudolf-update fetches each installed extension's build for the new base (the
  extensions' repository has one per base ABI). One that has no build for it yet is kept but left out after the
  update, with a notice and System health's line, and comes back when its build appears. An update never fails
  because of an extension.
- **Revocation:** the signed list's R lines disable an extension at the next refresh, as for Library packages.
- **Only an administrator** installs or removes one (the administrator's password), and the self-test lists the
  extensions in use and fails on a clash or a mismatch.

#### 13.4 Where you see them
Settings › Update and recovery › **System extensions**: the ones installed (name, what they add, version, size, in use
or left out and why), Install… (from the extensions' list, with an administrator's password), Remove, and "Start once
without extensions". System health's line for each one left out. The App Store does not show them (they are not
apps).

#### 13.5 Building and publishing them
A workflow (`extensions.yml`, started by the owner, never by a session) builds each extension from its recipe in
iso/ext/NAME/ (where its files come from, the base ABI and kernel it is for) in the same environment as the release,
signs it with RPK_SIGNING_KEY and publishes it with the release's assets, with a signed index. The release check
fails when an extension that a release's notes promise has no build for that release.

#### 13.6 The first extensions
1. **USB printers (Part 8):** ipp-usb and the filters for printers that are not IPP Everywhere, so the base does not
   carry them for every computer. Part 8 is built on this part.
2. **Rarely needed firmware** now in the base (to be listed from the drivers layer), moved out to make the download
   smaller, after the extensions have run through a release.
3. **Developer tools** (curl, strace, tcpdump), for Developer mode.

#### 13.7 Tests
Host tests: an extension's signature (a Library key refused), the ABI and kernel checks, the overlay assembled in a
private mount namespace with two extensions in order, a clash refused, a start script with its time limit. QEMU: an
extension installed and in use after a restart; a broken one (its start script hangs, or it stops the desktop)
left out on the next start; an update to a base with and without the extension's build; Recovery mode without
extensions; removal leaving nothing behind.

### 14. A keyring for Linux apps: the Secret Service (added on 2026-11-14)
Found on the Vivobook (2026-11-14): signing in to the Claude app for Linux with an e-mail address showed a notice that
GNOME Keyring is not set up. Linux apps (the Claude app, VS Code, Chromium-based and Electron apps, mail and chat
apps) keep their sign-ins and tokens through the Secret Service (org.freedesktop.secrets, libsecret). Rudolf OS has
none, so they warn, keep the secret in a plain file (Electron's basic text store) or forget it and ask again after
every restart.
- **The service:** gnome-keyring-daemon's secrets component only (no SSH or GPG agent, no GUI of its own), in Debian's
  container and for Flatpak apps (the portal's Secret interface), started with the account's session pieces (1.5.9's
  Part 15.3: each account its own Linux user, so each account its own keyring, in its own encrypted home).
- **Unlocked by the sign-in:** the keyring's password is the account's own; the sign-in (and the lock screen) hand it
  to the daemon (as pam_gnome_keyring does) so no second password is asked; changing the account's password changes
  the keyring's; signing out locks it. An account without a password gets a keyring kept with the account's encryption
  key instead of a password.
- **Prompts:** when an app asks to unlock or create a keyring the desktop draws the prompt (the one Settings uses), with
  the app's name; never a GNOME window.
- **Settings › Privacy › Saved passwords for apps:** which apps keep secrets, and Remove for each (and all of them when
  the account is removed).
- **Budget:** under 3 MB in the Debian layer and the system layer together, checked in ci_release.sh.
- **Tests:** the Claude app (or a stand-in using libsecret) keeps its sign-in across a restart without a prompt; another
  account cannot read it; a Flatpak app through the portal; the password change; the locked keyring after sign-out.

**Done, first step (2026-11-15):**
- **The service:** /usr/lib/rudolf/keyring, started by rudolfd at sign-in (au_session_start, after the session bus and
  the portal) as the account's Linux user with its session's environment: gnome-keyring-daemon `--foreground
  --components=secrets` (no SSH or GPG agent, no window), on the account's session bus, which Debian's programs (its
  /run/user/1000/bus) and Flatpak apps with `--talk-name=org.freedesktop.secrets` reach. Signing out stops it with the
  account's other programs. Its log: /run/rudolf/keyring-UID.log; its socket ($XDG_RUNTIME_DIR/keyring/) is in the
  list of listeners.
- **Unlocked by the sign-in, without the password:** decided instead of handing the password to the daemon. The
  keyring is the account's "login" keyring in its own folder (~/.local/share/keyrings), made the first time as a
  keyring with no password of its own (gnome-keyring's text format) and the default. The folder is encrypted with
  the account's key, is only its user's (0700, the files 0600), and is there only while the account is signed in: so
  no second password is asked, a password change needs nothing, sign-out shuts it away, and removing the account
  removes it. A keyring already there (one made in Debian with a password, say) is never changed.
- **Electron and Chromium-based apps** keep sign-ins in the Secret Service only for desktops they know (otherwise a
  plain file): with the keyring, the account's session names the desktop "Rudolf:GNOME" (acct-session.env, and
  rudolf-debian takes it from there, only that name). The desktop's own processes keep "Rudolf".
- **Budget:** gnome-keyring-daemon and the libraries the base layer lacks, 3 MB, checked in ci_release.sh (the
  release workflow installs gnome-keyring on the build machine).
- Tests: qt/host/keyring60_t.sh (5 here; 8 with the real daemon and secret-tool in CI: a secret kept through libsecret
  with no prompt, still there after the daemon starts again, its files the user's only, another user refused),
  users59_t.sh 26 (6c: started as the account's user in its session; 7: stopped at sign-out), debacct59_t.sh 16 (3b),
  .github/workflows/keyring-check.yml.

**Done, second step (2026-11-15):**
- **The Secret portal for Flatpak apps** (rudolf-portal, org.freedesktop.portal.Secret version 1): libsecret inside a
  Flatpak sandbox keeps the app's secrets in a file of its own (~/.var/app/ID/data/keyrings), encrypted with a secret
  the portal gives it through RetrieveSecret(fd). That secret is the app's own (64 random bytes), made once and kept
  in the account's folder (~/.local/share/rudolf/app-secrets/ID, 0700/0600): so it is as safe as the keyring, the
  same each time, another app's different. A program outside a sandbox is refused (it uses the Secret Service).
- **Settings › Privacy › Saved passwords for apps** (desktop/keyring60.js), after the camera: who saved what (the app's
  name, or what kind of app from its schema; a Flatpak app by its ID), drawn as text; Remove after asking (the app is
  signed out of what it kept there), Remove all. rudolfd's `GET keyring/list` and `POST keyring/forget?id=` run
  `rudolf-portal --keyring-list` / `--keyring-forget ID` as the signed-in account's user in its session: the
  Secret Service's collections and items (labels and the app's attribute only, never a secret) and the Flatpak apps'
  secrets; forgetting deletes the item (Item.Delete; one that would need a prompt is said) or the app's secret (its
  keyring file can then no longer be read: the app starts again signed out). Nobody signed in: nothing to list. The
  health log says when one was removed.
- Tests: qt/host/keyportal60_t.sh 10 (an app in a sandbox, a root with /.flatpak-info, given its secret, the same
  again, another app another, outside a sandbox refused, 0700/0600, the list, forget and a new secret, bad ids
  refused), keyring60_t.sh 9 with the real daemon (the item named, Remove: gone, no prompt), users59_t.sh 29 (6d, the
  list and forget as the account's user, anything else refused, nobody signed in), qt/shell2/keyring60_t.js 11.

**Done, third step (2026-11-15): the prompts drawn by the desktop.**
- **rudolf-portal is gcr's prompter** (org.gnome.keyring.SystemPrompter on the account's session bus, which
  gnome-keyring-daemon asks; gcr-prompter, a GNOME window, is not in Rudolf OS): gcr's protocol as gcr-system-prompter.c
  speaks it (org.gnome.keyring.internal.Prompter's BeginPrompting, PerformPrompt and StopPrompting; the daemon's
  org.gnome.keyring.internal.Prompter.Callback PromptReady and PromptDone; one prompt at a time, the others waiting;
  the properties sent back as gcr sends them, a new password's strength among them, so the daemon asks "Store
  passwords unencrypted?" only for an empty one), and its secret exchange "[sx-aes-1]" as gcr-secret-exchange.c does it
  (Diffie-Hellman in IKE's 1536-bit group, HKDF-SHA256 to a 16-byte key, AES-128-CBC with PKCS#7 padding, its key-file
  messages), with OpenSSL's libcrypto, which the base layer already has (no new library; the portal is built with
  -lcrypto in mkrootfs.sh, ci_release.sh and the ARM64 check). Weak public keys (1, p-1, p) are refused.
- **Who asked:** the portal watches its own bus (the same user's: the bus's monitoring) for an app calling a Secret
  Service prompt's Prompt(), which the daemon's question follows, and names that app: its Flatpak ID (the App Store's
  name for it), else its program (claude: Claude, code: Visual Studio Code, ...), else "An app".
- **The question** goes to the desktop as the portal's others do (a request in /run/rudolf/portal/req, kind
  "keyring": the daemon's title, message, description and warning, a new password or not, its choice, its buttons'
  words), drawn in keyring60.js in Settings' prompt style (the administrator's prompt: 🔑, who asked, the daemon's words
  as text, the password fields that hide what is typed, a new one asked twice and a mismatch said, the choice as a tick
  box), in front of the app on the screen (promptFront). The answer (yes with the password, or no) goes back through
  portal/answer; rudolfd now writes answers 0600 and owned by the user whose portal asked (they were 0644). The portal
  reads it, wipes it and hands the password to the daemon encrypted. A question the daemon withdraws (the app went
  away) is removed, and the desktop closes it.
- Tests: qt/host/keyprompt60_t.sh 9 (1: the secret exchange against another implementation, fixtures/sxpeer.py with
  python3-cryptography, and against libgcr's own GcrSecretExchange, 13 checks; 2-3: the build and rudolfd's 0600; with the real gnome-keyring-daemon on a private
  bus that cannot start gcr's prompter, fixtures/krapp.py (python3-secretstorage) and fixtures/krdesk.py standing in
  for the desktop: 4 no prompter, the request fails; 5 a new keyring with its password, who asked; 6 unlocking, a wrong
  password then the right one, its secret read; 7 Cancel; 8 a keyring with no password and the daemon's confirmation;
  9 the app gone while asked, the question withdrawn, the next one answered), in .github/workflows/keyring-check.yml;
  qt/shell2/keyprompt60_t.js 17; portal_t.sh 8, keyportal60_t.sh 10, keyring60_t.sh 12 (its restart now waits for the
  old daemon to end: it failed now and then), keyring60_t.js 11.

**Next:** the Claude app checked on the Vivobook (its notice gone, signed in after a restart) and an app asking for a new
keyring there (the prompt drawn by the desktop on Standard and on Native).

### 15. Moved from 1.5.9 (2026-11-15)
1.5.9 was finished on 2026-11-15 for promotion to stable; what it had left moved here, as written in docs/plan-1.5.9.md
(each item's part there has the details).
- **Code**
  - **The native desktop (1.5.9 12.3c):** same_t.sh's comparison of Oracle's window on Native against Standard's
    (toolbar, tabs, a site): the rule "Native appears and functions exactly as Chromium mode". With Part 12 above.
    (2026-11-15: not done yet. Checked with item 5: oracle_native_t.sh gives the same results with and without it (37 of
    41 on a host without a sound server; 4g, 4h and 9 fail there on main too), Oracle's own Alt+F4 still Chromium's.
    The comparison needs Standard's Oracle drawn the same way in Playwright, the shell built with a test key, as
    oracle_native_t.sh does for Native.)
  - **The official recovery (1.5.9 8.4, 8.5):** the repair at each start (the official build checks recovery.efi and
    system/r and restores them from the official release); Return to official restoring the official rudolf.efi; on a
    computer that never ran the official build, system/r downloaded when Developer mode is turned on. The EFI image is
    near the 32 MB an El Torito entry can name: watch it as the kernel grows.
  - **Browse with Tor (1.5.9 16.2):** Ctrl+W, T, N and Q in a website in the Tor window on Standard are still
    Chromium's (rudolf-display remaps them only for RudolfDesktop), with a test of rudolf-display's keys.
  - **Lite (from 1.5.8, through 1.5.9):** the desktop's menus and windows behind Oracle's tab pages (the covered tab's
    still picture while a menu is open); stutter in Oracle's tabs; Google sign-in for GitHub (symptoms to be described
    first); GPU drawing for Lite measured; Oracle's WebKit tabs (YouTube, Google sign-in with FedCM, a download, full
    screen, memory with 5 tabs); the lock and sign-in screen settings and the shutting-down screen; the dynamic
    backgrounds' moving parts.
  - **A lighter desktop page (1.5.9 10.3, 10.4):** the engines' limits measured; the ~200 MB that belongs to no
    process; WPE WebKit as a later Lite engine; the 10.4 targets measured on the Vivobook.
  - **The native session's logs kept across a restart** (found on 2026-11-15: rudolf-native's output, the page log,
    the Tor window's log and rudolfd's log are in /run and were lost with the restart; only session.log, the kernel's
    and the health log were kept in /persist/var/log/starts).
    **Done for the native desktop's own log (2026-11-15):** /run/rudolf/native-desktop.log (rudolf-native, and Oracle's
    Chromium there) is kept in each start's folder by rudolf-init, under 4 MB, and the system report has it ("native",
    "nativeBefore"). Settings › System health › Log video problems now works on Native as on Lite: GStreamer's warnings
    from rudolf-native's WebKit to webkit-video.log, and Oracle's Chromium there with its media/ messages
    (--enable-logging=stderr --vmodule=*/media/*=1) in the native desktop's log; read at the next start of the desktop.
    Asked for: "the native desktop has a lot of stuff that isn't working or is still very buggy". Test:
    qt/host/videolog60_t.sh 9, qt/host/startlogs58_t.sh 11. Still in /run: the page log and the Tor window's log.
  - **Kernel tuning** (proposed on 2026-11-12), if confirmed.
  - **rudolfd's root work split into helpers** (1.5.9 15.5, already planned here).
- **QEMU checks (CI)**
  - 1.5.9 15.1: a Flatpak app, a Wine program and a Debian terminal refused the desktop's page and token.
  - 1.5.9 15.3, third step: two accounts (a Flatpak app, a Debian terminal and Wine refused the other's files; the
    display refused to the other account's program; sign-out; the move on an existing data partition).
  - 1.5.9 15.4: qt/sb/t140_t.py signs in as an administrator before it switches the devlink on.
  - 1.5.9 8.7: Developer mode and Return to official with Secure Boot.
  - From 1.5.8 (4.2): the devlink answering with the desktop disabled.
- **On the Vivobook**
  - **Calls (1.5.9 7.4):** Meet in Oracle on Standard and on Lite; Zoom's test meeting, then the Zoom app; Teams; a
    call to a real phone; a headset's microphone during a call; memory during a 10-minute call. Results in
    docs/testing.md.
  - **1.5.9's features on its stable release:** Tor at check.torproject.org (and the Tor window's sign-in and clicks,
    #369); the microphone Test; Save and Open; WordPro's links; a USB stick in the File Explorer; the Wi-Fi radios
    (2.4 and 5 GHz) in Settings; 15.2's test (a file opened in an app cannot reach the desktop).
  - **Game mode and memory (1.5.9 9):** the idle reading; Task Manager's breakdown; Clear cache.
  - **recovery.efi on the computer itself (1.5.9 8.4):** only with the owner present.
  - **The Native session of 2026-11-15:** a program refused the system API 8 s before the restart ("not the desktop
    (Linux user 1000)"), two "bad device" requests, the shell's Chromium restarted twice and exited at once, and the
    shutdown record "desktop:none". With what the owner saw.
- **Waiting**
  - **The hotspot (1.5.9 Part 4):** Share this connection with the MiFi's USB tethering as the connection: a MiFi with
    a data plan. (Also below, Hardware to test with.)
  - **ARM64 (1.5.9 Part 3):** the test channel beta-arm64/ (needs the owner's OK), then the update and recovery checks
    on it; Part 7 above.
  - **Waydroid (1.5.9 Part 5):** measured on the Vivobook with Lite before it is offered.
  - **The first phone (1.5.9 Part 6):** chosen and bought by the owner (a PinePhone Pro or a OnePlus 6/6T), its device
    package from postmarketOS's; the MediaTek "S24Ultra" is not flashed or unlocked without the owner's decision.

## Hardware to test with
- **The MiFi 7730L** (USB tethering and Wi-Fi), with an active SIM (or a prepaid data SIM). Found on 2026-11-14: its
  USB network driver and Share this connection with it as the uplink are back in 1.5.9 (docs/plan-1.5.9.md); here it
  is the modem for Parts 1 to 5 where it answers ModemManager.
- **A USB cellular stick** with an MBIM or QMI modem (for example a Quectel EC25 or EG25-G in a USB adapter: the same
  family as the PinePhone's modem, useful again in 1.6.x).
- **A laptop with a WWAN module, or an M.2 WWAN card in an adapter:** the Vivobook has no WWAN slot; a used ThinkPad
  with a Fibocom L850-GL or Quectel EM05-G card covers the FCC-unlock path.
- **QEMU** with the USB modems passed through (x86 and aarch64), for automated checks of everything above the radio.

## Release checks for 1.6.0
- Host tests for each script and helper (ModemManager answers from a stand-in D-Bus service: SIM locked, PIN accepted,
  roaming, signal changes, a modem that disappears).
- Browser tests for Settings › Mobile network, the taskbar's signal, Messages, and the data limit.
- QEMU with a passed-through modem: connect, fail over from Wi-Fi (a virtual Wi-Fi link taken down) and back, data
  counted, the firewall on the cellular interface.
- On real hardware over the devlink: each test device connects, survives sleep and wake, and reconnects after the
  modem is unplugged and plugged in again.
- The leanness budgets (docs/plan-1.5.8.md Part 8) and the cellular budget (15 MB) in the release check.

## Steps
0. Part 9 (udev instead of mdev) first, so the modem work stands on it; then Part 10 (a Linux user for each account)
   and Part 11 (elogind and polkit), which builds on Part 10's sessions; Part 14 (the keyring for Linux apps) with them.
1. Parts 1 and 2 on the USB stick and the MiFi (1.5.9's ModemManager): modem found, SIM PIN, APN, connect. First
   (moved from 1.5.9, where neither was available): the stick shows in Settings › Network › Cellular modems with
   ModemManager run without udev (#280), and /run/rudolf/modem.log has no "Invalid rule match"; the self-test's
   modem line passes. (The stick belongs to the HP desktop's user: it is borrowed for these checks.)
2. Part 3 (priorities and failover) and part 4 (data and money).
3. Part 5 (signal and status) and the self-test.
4. Part 6 (Messages).
5. The FCC-unlock path on a laptop module.
6. Part 7: the ARM64 release with all of the above, after QEMU aarch64 and one ARM64 computer.
7. Part 13 (signed system extensions), then Part 8 (USB printers) on it: usblp and the hotplug script, ipp-usb and
   the printer filters as the first extension, rudolfd's print/find and print/job, Settings › Devices; the HP
   desktop's user's printer last.
8. Part 12 (the native desktop, its shell items 1-12 in order), alongside the rest: it is the desktop's, the other
   parts the system's. Native becomes the default only after its week of betas.
9. Part 15 (moved from 1.5.9): its Vivobook checks on the first 1.6.0 beta; its code items alongside the parts they
   belong to (12.3c's comparison with Part 12, the recovery with Part 13's signed layers, the QEMU checks first).
10. Betas on the Vivobook (with the USB stick and the MiFi) and the test laptop; promoted when the release checks pass.

## After 1.6.0: the native desktop decision (added on 2026-11-12)
(2026-11-13: the prototype moved into 1.5.9 as a third desktop engine, Native (test): docs/plan-1.5.9.md Part 12.
**Decided on 2026-11-13 after its measurements: native.** The shell in 1.6.0 (Part 12 above), the apps in 2.0
(docs/plan-2.0.md). The reasoning below is kept as it was written.)
Before the first phone (1.6.x), decide whether Rudolf OS's shell becomes native (drawn by its own program, not by a
web engine). Not before 1.6.0: its work is mostly the system's (modems, the SIM, ARM64), its desktop parts (Messages,
the taskbar's signal, Settings › Mobile network) are planned in the desktop page and would otherwise be built twice,
and the web page runs the same on x86 and ARM64 while ARM64 is brought up.
- **What it rests on:** the hidden-memory reading (docs/plan-memdebug.md) and the lighter desktop page
  (docs/plan-1.5.9.md Part 10), both measured on the Vivobook: Standard's and Lite's memory at idle before and after.
- **The prototype:** a small native shell (taskbar, wallpaper, window frames) in a Wayland compositor, with the apps
  still web pages loaded when opened, measured on the Vivobook and on the ARM64 computer: memory at idle, with Oracle
  open, and with a game; start-up time; battery at idle.
- **Then:** if it saves enough on a 2 to 4 GB device, the phone's shell starts native, and the desktop can follow as
  Rudolf OS 2.0 (every app moved over, the browser build and the phone web app kept or retired). If not, the web
  desktop stays and the effort goes into slimming it further.
- For scale (2026-11-12): Standard at idle about 1.0 to 1.2 GB; light native Linux desktops (Xfce, LXQt) typically
  300 to 500 MB for the whole system. The desktop is about 4.9 MB of code in 97 files: a native desktop rewrites all
  of it, so it is a release of its own, never part of another.
