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

### 10. A Linux user for each account (added on 2026-11-13)
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
folder; the shell does not take X drops yet); the desktop menu's Show all windows (item 5) and Add a widget… (item 2).
**Next:** items 2 to 12 in order, one PR each, stacked, each passing same_t.sh for its part, and docs/plan-2.0.md after
them. Native is not the default yet.

**Becoming the default:** after items 1-12 pass on the host and on the Vivobook (a week of betas on Native), the
release's default engine is Native for new installations; an update keeps the engine the computer had, and Settings
offers the change once ("Rudolf OS's new desktop uses less memory and starts faster"). Native's safeguards stay
(1.5.9 Part 12 step 5). The release check measures Native at idle on the Vivobook and fails over 900 MB used.

## Hardware to test with
- **The MiFi 7730L** (USB tethering and Wi-Fi), with an active SIM (or a prepaid data SIM).
  First (moved from 1.5.9, where it could not be found): with USB tethering on, which USB network driver it needs
  (plan-1.5.8.md 6.1), before Parts 1 and 2 use it.
  Then (moved from 1.5.9) Share this connection on the Vivobook with the MiFi's USB tethering as the connection: the
  RTL8821CE as the access point, a phone joins it and browses (1.5.9 Part 4; its AP mode is already confirmed).
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
   and Part 11 (elogind and polkit), which builds on Part 10's sessions.
1. Parts 1 and 2 on the USB stick and the MiFi (1.5.9's ModemManager): modem found, SIM PIN, APN, connect. First
   (moved from 1.5.9, where neither was available): the stick shows in Settings › Network › Cellular modems with
   ModemManager run without udev (#280), and /run/rudolf/modem.log has no "Invalid rule match"; the self-test's
   modem line passes. (The stick belongs to the HP desktop's user: it is borrowed for these checks.)
2. Part 3 (priorities and failover) and part 4 (data and money).
3. Part 5 (signal and status) and the self-test.
4. Part 6 (Messages).
5. The FCC-unlock path on a laptop module.
6. Part 7: the ARM64 release with all of the above, after QEMU aarch64 and one ARM64 computer.
7. Part 8 (USB printers): usblp and the hotplug script, ipp-usb in the system layer with its budget, rudolfd's
   print/find and print/job, Settings › Devices; the HP desktop's user's printer last.
8. Part 12 (the native desktop, its shell items 1-12 in order), alongside the rest: it is the desktop's, the other
   parts the system's. Native becomes the default only after its week of betas.
9. Betas on the Vivobook (with the USB stick and the MiFi) and the test laptop; promoted when the release checks pass.

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
