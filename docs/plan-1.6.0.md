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
Everything moved past 1.6.0 is gathered in docs/plan-1.6.1.md (2026-11-15).

## Leanness
- Cellular support adds about 6 to 10 MB to the base layer (ModemManager, libqmi, libmbim, libqrtr, the operators'
  database, the kernel drivers as modules): measured in the release check against a 15 MB budget. (2026-11-15: the
  owner dropped the size budgets for 1.6.0: the build prints the sizes, systools/ci_release.sh.)
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

**Done, first step (2026-11-15), on Part 13's signed system extensions:**
- **Kernel:** USB_PRINTER (usblp) a module, checked after olddefconfig in kconfig.sh and kconfig-arm64.sh.
- **Devices:** 90-rudolf.rules runs /usr/lib/rudolf/usbprint for a printer's interface (by ENV{INTERFACE}=="7/1/*", so
  also when it goes; add, remove and bind) and for usblp's /dev/usb/lpN; mdev's fallback has the same two lines
  (before its modalias line). udevadm verify passes on the rules.
- **/usr/lib/rudolf/usbprint** (hotplug, scan): every USB printer there is, written to /run/rudolf/usb-printers (kind,
  name, address, IEEE 1284 device ID, sysfs name): IPP over USB (an interface 7/1/4: usblp unbound from that printer's
  interfaces, ipp-usb started in its udev mode, which ends after the last one, and its port read from its state file:
  ipp://127.0.0.1:PORT/ipp/print; no address without the extension), PDF or PostScript (the device ID's command set:
  its /dev/usb/lpN), or "needs a driver" (PCL-only, host-based); plugged in and unplugged said in
  /run/rudolf/usbprint.log; the name from the device's strings or its device ID (short and long field names).
- **The first extension, `usbprint`** (iso/ext/usbprint: recipe.json, its start script, its filters' links): ipp-usb
  (from Ubuntu's /sbin, put in usr/sbin), Poppler's pdftops and pdftocairo (as /usr/lib/rudolf/print/pdftops and
  pdftocairo), libpoppler and Avahi's client libraries where the base lacks them (mkext.sh leaves out what the base has),
  ipp-usb's quirks; its start script writes ipp-usb's settings (loopback only, no DNS-SD: Rudolf has no Avahi, the
  address goes through /run/rudolf/usb-printers instead, ports from 60000, short logs). No budget in ci_release.sh (the
  base does not carry it); mkext.sh prints its size: about 9.9 MB of files with all its libraries, 2.2 MB to download
  where the base has libpoppler and Avahi's client (measured here on Ubuntu 24.04, 2026-11-15).
- **rudolf-print:** find adds the USB printers as "USB: name" (an IPP over USB one at ipp-usb's address, printed to as a
  network printer; a plain one as usb://lpN; one that cannot print as usb://KIND/SYSNAME); `rudolf-print usb` (JSON for
  Settings: kind, ready); info for usb:// (formats; a PostScript one only with the extension's pdftops; one needing a
  driver has no format, so the print dialog says it needs a driver; an IPP over USB one without the extension says to
  install it); send to usb://lpN writes the PDF as it is (the pages picked with pdftocairo when the extension is there,
  else all of them, said) or PostScript from pdftops (with the pages), once per copy.
- **rudolfd:** print/find as before (with them); print/usb; a printer's address may be usb://... (no "..", quotes or
  ports).
- **Settings › Devices › Printers plugged in by USB** (desktop/usbprint60.js): each one's name (as text) and kind (IPP
  over USB, reads PDF, reads PostScript, needs a driver), ready or needing the USB printers extension (a System
  extensions… button), read again with Refresh and when a USB device comes or goes.
- **The self-test:** a "usb-printer" line (each one and its kind; fails when an IPP over USB one has no address or
  ipp-usb does not answer there; skipped with none).
- Tests: qt/host/usbprint60_t.sh 31 (a stand-in sysfs: an IPP over USB printer, a PostScript one, a PCL-only one, a PDF
  one, a USB stick; ipp-usb's stand-in in front of qt/fakeprinter.py, printed to through its address; the jobs to
  stand-in /dev/usb/lpN; without the extension; unplugged; rudolfd's address check (fixtures/printuri.c); the rules,
  mdev's lines, the kernel, the self-test), qt/shell2/usbprint60_t.js 13 (and the print dialog's list), udev60_t.sh 13,
  pdfprint59_t.js 8; .github/workflows/sysext-check.yml runs both host tests (sysext60_t.sh, usbprint60_t.sh) on pull requests.

**Next:** extensions.yml's first run (the owner's) to publish `usbprint` for the first release with a base ABI; the QEMU
check with a USB printer passed through (or QEMU's usb-printer-like device) and ipp-usb itself; on the HP desktop's
user's printer (borrowed): first read only (its interfaces and device ID: which kind), then a test page; ARM64's recipe
(the file globs are by architecture already; the release ext-ABI16 differs per base).

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
**Item 7 decided (owner, 2026-11-15):** "Rudolf OS has three workspaces that can be selected in the bottom right of the
taskbar. These workspaces will be the basis for Rudolf being able to support up to three monitors/displays, as well as
having three panels for the mobile build that can be selected by swiping left/right similar to how Android phones
work." So several monitors are built on the three workspaces, on Standard and Native alike (the rule holds): up to
three displays, each showing one of the workspaces with its own wallpaper and taskbar; a window moved to another
workspace moves to that display. One display keeps today's behaviour (the three workspaces switched from the taskbar).
The mobile build's three panels are the same three workspaces, swiped left and right.
When (owner, 2026-11-15): after the 1.6.0 beta, in 1.6.x; it needs real monitors to test.
**Item 8 (2026-11-15, unattended session): the Phone screen and Blitzen's panel.** Both are page windows (apk58.js
phoneScreen, blitzen26.js blitzenApp; the page docks neither), so Native shows each in a one-app window opened from its pin.
Checked against Standard with same_t.sh, new cases phone-nolinux, phone-noscrcpy, phone-ready, phone-dark, blitzen-setup,
blitzen-chat and blitzen-oldcpu (the states rudolfd gives: Linux not set up, scrcpy missing, ready, the dark theme; Blitzen
not downloaded, ready, an old processor; same_rd.py answers debian/status and ai/status from the test's files). A new part,
'win', is the whole window: frame, title bar and the app's page, WebKit's against Chromium's. Found and fixed (in Native):
- WebKit's text was wider than Chromium's: without XSETTINGS, GTK gave WebKit full hinting. rudolf-native now sets grey
  anti-aliasing and slight hinting (as the shell's own Pango text already is) before its first window
  (RUDOLF_NATIVE_GTKHINT=1 leaves GTK's). The Phone screen's ready window went from 6.7 % to 1.6 % differing pixels with it.
- Drop-down lists were GTK's taller combo boxes, pushing Blitzen's bar and everything under it down 8 px; check boxes and
  radio buttons had WebKit's size and margins. The page's one-app CSS for WebKit windows (html.oawk, oneapp59.js, only when
  the native shell's message handler is there) draws a list as Chromium does (the page's border, padding and colours, the
  chevron, 4 px more room before the text and 20 after it, 2 px taller), and check boxes and radio buttons with Chromium's
  13 px and margins. A list given its own padding by a rule of its own keeps that padding (with the chevron).
- Blitzen's greeting (line height 1.5 at 13 px): WebKit makes each line 19 px and Chromium 20 px; given 20 px there.
The 'win' part allows a pixel's match one pixel away (pxdiff.py's new SHIFT, only for 'win'): the engines still round other
half pixels of line boxes differently (Blitzen's suggestions' text 1 px higher inside their buttons in WebKit). The frame
('title') and the taskbar are compared as every part. Results: 12/12 (win 0.12 % to 1.33 %, title 0.29 % to 1.41 %, bar
0.93 % to 1.05 %); the whole same_t.sh 116 of 117 in one run (ws2-dark's toast caught while it was still appearing, 3.6 %; 1.09 % run alone, with or without the hinting); native59_t.sh 70/70, wmnative59_t.sh 3/3, oneapp59_t.js 42/42. Known differences left (item 8): scrcpy's own window (an X window
of the phone, framed by the shell as a Linux app's, as on Standard) needs a phone, so it is a Vivobook check, not a host
test; selects given other padding by their own rules (games' and the media player's) have the chevron but not Chromium's
20 px after the text.
**Test findings (2026-11-15, unattended session):** Native compared with Standard across the desktop, the apps, video and sound, with Log video problems on: 19 findings (3 break: full-screen video black, the file dialog cut off inside the app's window, Task Manager lists only itself). See docs/native-findings-2026-11-15.md. A second run added 13 more (2 break: Log video problems' file emptied by each new app window, right-click menus inside apps cut off at the window's edge), in the same report's "Second run" section.
**Test findings fixed, first PR (2026-11-15, unattended session): the three that break, and the page's dialogs.**
- N2, N4, N10: the page's dialogs (.rmodal: Open and Save, Print, Allow access?, Close anyway?, Report a problem; .natdlg: a game's
  download) are over the whole desktop on Standard, dimmed and centred on the screen. On Native the app's window now covers the
  screen while one is open (full screen, above the taskbar), its page given a picture of the screen as it was (the page's own
  dimming over it), and goes back to its place and size after (oneapp59.js: op "modal", the dialog unseen until the picture is
  there; rudolf-native appwin_modal). An app asking before it has a window (the camera's Allow access?, a game's download) has no
  window or taskbar button of its own any more: its page is out of sight until its window comes. The dialog's button gets the
  focus (and Chromium's focus ring) as on Standard. A ticked box is drawn in Chromium's blue, and a number field's arrows show only
  under the pointer (the Print dialog's Pages and Copies), as Chromium's.
- N1: a video's ⛶ (and F11, a game's full screen) uses the shell's stand-in on Native too (shell26.js fsStandIn: the element fills
  the page) with the window covering the screen (op "fs"), as Standard's page is the screen: the picture shows (WebKit's own full
  screen showed it black under the native shell). Esc leaves, as on Standard.
- N3: Task Manager's Apps lists every window on Native, as Standard's one page does: the shell gives its list (op "wins": each
  window the taskbar would show, in every workspace, with its page's own weight, oneapp59.js __oaMe); Switch to and End task are
  done by the shell (op "winop"; End task closes the window as Standard's does, its files written first). The status line and the
  system report count every window too (tm26.js tmWins, the page's own list on Standard).
- N18: Log video problems no longer logs downloadbuffer's "Failed to remove temporary file" for every video (WebKit removes its own
  temporary file); GStreamer's underflow and clock-skew warnings stay (they are what a stutter looks like).
- N16, partly: with the full screen fixed, WebKit counts a video's frames while it plays as Chromium does (getVideoPlaybackQuality:
  51, 102, 153 at 1.7, 3.4 and 5.1 s); at the video's end WebKit stops counting (153 of 180 for a 6 s video, Chromium 180). Nothing in
  Rudolf OS reads it, and Oracle, where websites do, is Chromium on Native: for the owner (below).
- same_t.sh: new cases dlg-open, dlg-ask, dlg-print, dlg-camera, dlg-dark (the dialog and the whole screen under it), tm-apps (Task
  Manager's rows, read from the page on both engines: "read", run in Native's window through RUDOLF_NATIVE_TEST_EVAL) and video-fs
  (the Media Player's ⛶: the share of the screen showing the video, on both; the test's own video made with ffmpeg, served by
  same_rd.py at /tm/). videolog60_t.sh checks GST_DEBUG's downloadbuffer:1.
- For the owner: N16's end-of-video count (a WebKit counter; accept as a WebKit difference, or not); whether GStreamer's "Got underflow"
  at each video's end should be left out of Log video problems too (N18); N1 with real drawing on the Vivobook (--draw other than
  soft), and the dialogs with a touch screen.
Left of the findings (the next PRs): N5, N6, N7, N8, N9, N11, N12 (look wrong), N13, N14, N15, N17, N19 (minor).

**Test findings fixed, second PR (2026-11-15, unattended session): the nine that look wrong and the minor ones** (on the first PR's
branch: it needs its test cases' js, read and nowin).
- N5, N6, N9, N13 (the form controls in the native desktop's WebKit windows): Chromium's own controls, measured from its drawing and
  given as the browser's defaults (oneapp59.js, html.oawk, a cascade layer under the page's own rules, as Chromium's built-in styles
  are under them): a range is a 129×16 box with an 8 px track (#0075ff up to the knob's centre, #efefef after, each with its 1 px
  edge) and a 16 px knob, greyed when disabled (its filled part kept as the value changes: by the person, the page's script or its
  attributes); a colour box 50×27 with the swatch inset; radio buttons Chromium's ring and dot; a progress bar 10em by 1em with the
  range's track; a time field Chromium's clock icon and a date field its calendar, with Chromium's room for them; a number field's
  arrows only under the pointer (first PR). A list's chevron also when the page gives the list a background of its own (an inline
  style: WordLite's lists, File Explorer's sort and view, the Media Player's speed). rudolf-native gives WebKit Chromium's locale
  (en-US) when the system has none, so times are "07:00 AM" as on Standard.
- N7, N8: the Storage API on in WebKit (navigator.storage.estimate: File Explorer's status line and About's Storage used), with an
  origin's share of the disk as Chromium's (60 %); Folders on this device (Import a folder…, Export selection to a folder…) through
  a stand-in for Chromium's folder picker: the shell's folder dialog, and the folder's files read and written by the shell inside
  the folder chosen only (rudolf-native hfs_msg); About's Graphics the graphics chip's name from GL, as Chromium gives it ("ANGLE
  (VENDOR, RENDERER, OpenGL VERSION)"; WebKit says "Apple GPU" everywhere). About's Chromium row comes from rudolfd's sys/status on
  both engines (the test's stand-in now answers it).
- N11, N12: the dynamic backgrounds' moving parts drawn by the shell as the page draws them, over the picture and under the icons:
  Glacier Lake's northern lights at night (aurora58.js: the three curtains, the accent colour's rule, the lake's mask, 100 ms
  frames), Spirit Falls' petals and, at night, its sky lanterns (statue58.js: 66 ms frames), with the page's stops (a window over
  60 % of the screen, full screen, locked, the screen saver; half the frames on battery; still with Reduce motion). Spirit Falls'
  statue is the page's own picture of it (its animal, light or dark, glow), drawn by the page at the screen's size and given with
  the catalogue (oneapp59.js oaCatalog fx.statue; given again when it changes, op "cat").
- N14: a window whose own icon is not its app's (Flash Player's "not installed" window) shows its own picture in the frame and on
  the taskbar button (the page's icSvg, given with op "title").
- N17: an iframe's sandbox flags that WebKit does not know (allow-presentation) are left out instead of logged as errors.
- N19: native59_t.sh test 21 counts the web processes of its own rudolf-native only (its processes' tree): it counted every
  WebKit process on the host, so another WebKit program (another test running) made it 2. Not Rudolf's: one web process is shared.
- same_t.sh: new ctl-volume, ctl-mouse, ctl-media, ctl-wordlite, ctl-clock (the whole window), explorer (and what it says),
  about (its rows), dyn-lake, dyn-oasis-dark (the share of the screen moving in 3 s on both), dyn-oasis (the statue), title-icon,
  sandbox-flags (nothing in Native's log).
- For the owner: N15, the window opening animation (Standard's 0.18 s fade and scale from 96 %): fading a window needs a
  compositor, which the native shell does not have; it could be drawn by the shell over the screen for those 0.18 s from the
  window's picture (Composite, as the overview takes them) — a decision whether that is wanted for 0.18 s. N16's end-of-video count
  and N18's underflow lines (first PR). On the Vivobook: About's Graphics row on both engines (Chromium's ANGLE may use Vulkan there,
  which names the chip differently), the folder picker with a USB drive, the moving backgrounds' smoothness with real drawing.

**Test findings fixed, third PR (2026-11-16, unattended session): the second run's** (docs/native-findings-2026-11-15.md, "Second
run"; on the second PR's branch: its test steps).
- N20: Log video problems' file is opened to add to (GST_DEBUG_FILE=+…): each WebKit process (every app window's) opened it anew and
  emptied it.
- N21: an app's own right-click menus (the page's shared cmenu: File Explorer's, a text box's) are shown by the shell over the screen
  at the pointer, as the page's own on Standard, with all their rows (they were cut off at the window's edge); the row chosen is done
  by the page (oneapp59.js op "ctx", __oaCtxDo; rudolf-native ctx_msg with the taskbar menus' cm_open).
- N22, N24: Chromium's classic scrollbars in the WebKit windows (15 px, arrows, the grey thumb; measured from Chromium): GTK's overlay
  scrollbars took no room, so lines wrapped differently, and Display Properties' tiles were wider and so taller (N24).
- N23: same_t.sh starts Chromium with its scrollbars (Playwright hides them unless told), as Rudolf's Chromium shows them.
- N25: a text box the page does not style has Chromium's 1 px grey edge, square corners and grey placeholder.
- N27: the frame in dark mode as the page's html.dark .win: its edge #334155 and its other windows' title bars #1e293b.
- N29: a video's frame callbacks (requestVideoFrameCallback) are called on Native (at the first animation frame after a new frame
  was shown): WebKitGTK never called them, so the hardware check's camera frame rate read 0.
- N26 and N28 were done by the first and second PRs (the check box's blue; the graphics chip's name, from which GPU Lab says hardware or
  software).
- N30: the start menu, longer than the screen, has Chromium's scrollbar on its right as Standard's #menu (drawn by the shell; its rows
  15 px narrower); its search box has the page's focus ring on both engines in the test (Playwright's Chromium draws it as Native does).
- An app's menu has Chromium's focus ring only when nothing had the focus before it (not after a click in a list or a text box), as
  Chromium shows it. N31 (Oracle's WebKit stand-in, host only) and N32 (a desktop sound from an app window, not confirmed) are Vivobook
  checks. Night light and touch mode are items 9 and 10.
- same_t.sh: new app-menu (File Explorer's menu on a folder), title-dark, help (its scrollbar and search box); videolog60_t.sh checks
  the "+".

**Test findings, the rest (2026-11-16, unattended session; docs/native-findings-2026-11-15.md, "Status on 2026-11-16").**
- N15: a new window fades in and grows from 96 % in 0.18 s (ease-out) as Standard's .win.wopen: a window of the shell's lies over it
  for those 0.18 s and draws what is there from the windows' own pictures (Composite), the new one among them part of the way
  (rudolf-native wopen_start; not with Reduce motion). Started at the window's first place (appwin_size) or, for another program's
  window, when it is framed.
- The overview's pictures are the windows' pixmaps under Composite, named again 25 times a second (a video keeps moving under
  Super+Tab); a toast's background is drawn from the windows under it 20 times a second (comp_behind: each window redirected while a
  toast needs it, drawn again when redirected or given back; one raised over the toast since, as the taskbar, given back at once);
  a toast window going from two toasts to one is moved at once (it stayed 15 px off).
- N16 not reproduced (180 frames at the end on both engines); N28 and N29 checked; N19's test 70/70.
- same_t.sh: new cases wopen, overview-live, toast-live, video-frames, gpulab.
- The areas never compared: iso/qt/sb/t160n_t.py on the newest beta in QEMU (workflow native-areas.yml), and the owner's checks in
  docs/native-vivobook-checklist.md.

Found while doing the findings, for items 11 and 12 (not changed yet): every one-app page runs the page's module-level timers as
Standard's one page does, so with several app windows open some background work runs once per window: the Calendar's alarm check
(apps25b.js, every minute: an alarm could be said once per open window), game mode (games58.js gameTick, gamesleep59.js), the home
folder's refresh (home56.js, 15 s), Tor's keep-alive (tor59.js, 20 s), the session's save (shell25.js, 20 s) and the lag meter (500
ms); with no app window open, none of them runs (and rudolfd's events wait for a page, as item 3 noted). The way the plan's item 12
asks for: a first module (before core25.js) that, in one-app mode, holds back the timers the modules start while the page loads, and
starts in an app's page only those its window needs (the theme's and the icons' minute ticks); one hidden page of the shell's
(#app=do-svc), started at sign-in, runs the others once for the whole desktop (alarms, game mode, events/wait, Tor, the home folder),
as Standard's one page does. Measured on this host (WebKitGTK 2.52.6, --draw=soft): opening the first app adds about 230 MB of
WebKit web process, 60 MB of its network process and 90 MB in rudolf-native; the next app about 30 MB more.

**Next:** items 9 to 12 in order, one PR each, based on main, each passing same_t.sh for its part, and docs/plan-2.0.md
after them. Native is not
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
**Decided (owner, 2026-11-16): "Native is going to be the default for new installations."** The switch above (#384) is on
main; "Before stable" item 5.

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

**Done, first step (2026-11-15):**
- **rpk** (iso/sys/rpk.c): kind `sysext`. Installed only with `--system` (rudolfd gives it after checking an administrator's
  password; the App Store's `pkg/install` cannot install or remove one), only when signed by a key in
  /etc/rudolf/rpk-keys itself (the release key: not a Library or developer key, nor one added with `rpk key add`), with
  PKGINFO `base` equal to this base's /usr/share/rudolf/base-abi and `kernel` (when given) to the running kernel, holding
  only PKGINFO, licenses/, usr/, lib/firmware/ and lib/modules/KVER/extra/ (no device files) and at most one start
  script, usr/lib/rudolf/ext.d/NAME.start. Never mounted at /pkg; left out of `rpk status` (the App Store);
  `rpk sysext-status` (JSON for Settings: installed, what this start did, the list's available ones) and
  `rpk sysext-check NAME` (its file's signature again, base, kernel, revocation). `rpk update` updates an installed one
  as the apps. Health log lines of their own (kind "extension"). The extensions' list: /usr/share/rudolf/sysext-source.
- **The assembly** (/usr/lib/rudolf/sysext, run by rudolf-init before udev, from /persist/pkg): each installed extension
  that passes sysext-check mounted read-only at /run/rudolf/ext/NAME in name order; one with a file the base (or an
  earlier extension) has is left out, said in /run/rudolf/ext/state and the health log; /usr and /lib/firmware become
  read-only overlays (the extensions over a bind of the base); /lib/modules/KVER an overlay with a scratch upper layer in
  /run for depmod's lists (the base never written); start scripts as root, 20 s each (killed after), output in
  /run/rudolf/ext/NAME.log, a failure in the health log. The trial: what is used for the first time (or a new version)
  is written to /persist/pkg/sysext/trying; the session's watchdog marks the start good at the desktop's first heartbeat
  (`sysext good`); if it never came, the next start leaves those out (sysext/off/NAME, with Try again). None in Recovery
  mode (rudolf.recovery=1), with rudolf.noext=1, or once after "Start once without extensions".
- **rudolfd:** GET sysext/status; POST sysext/install, remove (as jobs), retry, skip-once, each only with an
  administrator's name and password in the body, checked by rudolfd (desk_admin_check, as the development link's).
- **Settings › Update and recovery › System extensions** (desktop/sysext60.js, after Security updates): each one's
  name, what it adds, version, size and state (in use, in use after a restart, left out and why), Remove (asks first),
  Try again, Install… (the list for this base), Check for extensions, Start once without extensions, Restart now; a
  standard account's buttons off; drawn as text.
- **The self-test:** an "extensions" line (those in use; fails on one left out, with why).
- **Building:** iso/systools/baseabi.sh (the base's libraries' hash), written by ci_release.sh into the base
  (usr/share/rudolf/base-abi, and sysext-source: the release ext-ABI16) and by mkrootfs.sh; iso/systools/mkext.sh (an
  extension from iso/ext/NAME/recipe.json: Ubuntu 24.04's packages, what the base has already left out, every library
  its programs need found, its size printed, signed); .github/workflows/extensions.yml (workflow_dispatch only, for one
  release's base and kernel: signs with RPK_SIGNING_KEY as the release does, publishes the extensions and a signed index
  in the release ext-ABI16, keeping those already there). rpk-tool.py: `sysext-recipe`.
- Tests: qt/host/sysext60_t.sh 47 (throw-away keys made by the test: the App Store's way refused; the system key
  accepted, a Library key and an added key refused; another base, another kernel, files outside usr/ and lib/, a start
  script not its own, kernel modules without the kernel line refused; two extensions laid over a stand-in root in a
  private mount namespace, in order, read-only, with depmod; the start script; the trial, Try again and good; a clash
  with the base and with another extension; a start script stopped after its time limit; Recovery mode, rudolf.noext=1,
  Start once without; a file changed after installing; another base after an update; a revocation; the list in
  Settings and not in the App Store; removal; the self-test's line; rudolfd's requests with an administrator's password
  only), qt/shell2/sysext60_t.js 16, rpklib_t.sh 15, devcert59_t.sh 14, selftest_t.sh 25.

**Done (2026-11-16), the QEMU checks of 13.7** ("Before stable" 6): qt/sb/t160x_t.py on the real release (beta
2026111611) in QEMU, 20/20 (Actions run 43914057850, workflow `Extensions QEMU check`, started by a push of
.github/ext160-run.txt). An extension must be signed by a key in the base's /etc/rudolf/rpk-keys, which only the
release key is, so the test makes TEST keys and two custom builds of the release (Developer mode with a TEST developer
key): the release's kernel, base, browser and drivers layers, and a desktop layer that adds the TEST system key to
rpk-keys and names a base ABI (build 1 in slot a, build 2 in slot b, "the new base"); the drive laid out as Install here
leaves it (the release's boot image in front). Small test extensions: hello 1.0 (ABI 1) and 2.0 (ABI 2), a program
and a start script; hang (its start script never ends); corrupt (its file changed once installed); badsig (its file
replaced by the same package signed by another key); breakdesk (its start script stops rudolfd, so the desktop never
answers). Checked: installed and in use after a restart (its program and start script, /usr a read-only overlay, the
base never written); one for another base and one signed by another key refused at install; the hanging start script
stopped after 20 s and the system started; the changed and the re-signed file left out with their reasons (Settings'
list, the self-test, the health log); the start that never reached the desktop left breakdesk out at the next start,
the rest in use; removal leaving nothing; an update to build 2 without hello's build for it (left out, "built for
another Rudolf OS base", the update kept, the desktop up), then with hello 2.0 installed (in use); Roll back system to
build 1 (hello 2.0 left out, the computer starts); Recovery mode without extensions.
- **Found and fixed:** what the assembly said (an extension left out, a start script stopped, the trial's leaving
  out) never reached System health: rudolf-init laid the extensions over before marking the data partition as in use
  (/run/rudolf/persistent), so `rudolfd --health` wrote those lines to /run/rudolf/health.log, which nothing reads.
  rudolf-init now marks it first (the run with this tree's rudolf-init in the custom builds, fix=1; the run on the
  release as it is had 16/20, the four the health log's). Host test: sysext60_t.sh 17.
- **Not changed:** an update to a new base leaves an extension out (said, the computer starts, the update is kept)
  until its build for that base is installed; rudolf-update does not fetch the builds before switching slots yet. Safe
  as it is (the update never fails because of an extension); the fetching is for 1.6.1, with the first published
  extension (`usbprint`, moved to 1.6.1).

**Next (1.6.1):** rudolf-update fetching each installed extension's build for the new base before switching slots;
extensions with kernel modules built in the kernel's own run (signed with its module key); the release check for
promised extensions; the first run of extensions.yml (the owner's, with `usbprint`).

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
    **Done (2026-11-16, "Before stable" item 9):** `iso/qt/native/same_oracle_t.sh` draws Oracle's window with both engines
    in network and mount namespaces of its own, one after the other, with one stand-in rudolfd (fixtures/native_rd.py):
    Native as oracle_native_t.sh runs it (rudolf-native, Oracle in Rudolf's Chromium with the shell built with a test key);
    Standard as the session starts it (the same shell full screen, --class=RudolfDesktop, the desktop page in it and Oracle
    a window of the page; keys through rudolf-display's own code, fixtures/dispkeys.c). The same settings on both (the
    page's, and Native's desktop/settings from them) and Standard's window put at Native's frame's place and size; each
    part cut out of both screenshots and compared (same_oracle.py with pxdiff.py: over 2 % of a part's pixels differing by
    more than 32 fails, a size more than 1 px apart fails): the title bar, the tabs, the toolbar, the address box, the
    bookmarks bar, the Home page, a local test site in a real tab, the ☰ menu, a download in about:downloads, dark and light
    mode, a green accent; the title bar's words in each; and the same steps done on both, their outcomes compared: a new
    tab (+), Ctrl+W in Oracle's page, a tab's ✕, Ctrl+T, Ctrl+W and Ctrl+N in a website, a link, ◀ and ▶, Alt+Left, a
    website's full screen and Esc, Alt+F4. 28/28 here (Playwright's Chromium 153) and in CI with Playwright's and with
    the beta's own Chromium 158 (oracle-same-check.yml; the pictures kept as its artifact). same_t.sh's header points to it
    (it needs root and the shell, as oracle_native_t.sh does).
    What was different, each fixed in Native or in what both engines share (Standard's own drawing unchanged):
    - Rudolf's Chromium 158 kept an Oracle window's own title at the shell's name ("Rudolf OS"), so on Native the frame and
      the taskbar said "Oracle" whatever the page (Standard: "Downloads - Oracle" and so on), and a Tor window's had no
      "(Tor)". The page now tells the native desktop its title through rudolf-launch's binding, which writes it with its
      window's place ("title X Y W H STRING"), and rudolf-native gives it to that window before Chromium's own name (with
      item 10's PR, where the Tor window's test found it too; DevTools' target info never carries a page's title here).
    - In every website tab but an account's first, Ctrl+T, Ctrl+W and Oracle's other keys did nothing, and the tab kept
      the address as its name: the shell's content scripts (PAGE_JS) reached only the first tab of a partition. On both
      engines; the shell now gives PAGE_JS at each page's commit too (once a page), and a tab opened with its address
      waits for the content scripts.
    On the Vivobook (owner): Oracle on Native and on Standard side by side: its title bar and taskbar button with a site
    open, a second tab's Ctrl+T and Ctrl+W, Ctrl+N, Alt+F4, a download, dark mode.
  - **The official recovery (1.5.9 8.4, 8.5):** the repair at each start (the official build checks recovery.efi and
    system/r and restores them from the official release); Return to official restoring the official rudolf.efi; on a
    computer that never ran the official build, system/r downloaded when Developer mode is turned on. The EFI image is
    near the 32 MB an El Torito entry can name: watch it as the kernel grows.
    **Done (2026-11-16), tested in QEMU on the real release** ("Before stable" 7): qt/sb/t160o_t.py (workflow
    `Official recovery QEMU check`, started by a push of .github/recovery160-run.txt) on beta 2026111611, a computer
    installed as Install here leaves it (the release's boot image in front of RUDOLFDATA) starting through its own boot
    menu, with QEMU's user network (the repairs download from GitHub, as on a computer). Part 1 (Secure Boot off, the
    official boot files), 13/13 in runs 43910756094 and 43917182147: system/r made at the first start; both start-up
    files official (sbverify with the Rudolf OS certificate); recovery.efi changed (one byte) and system/r's desktop layer
    deleted: at the next start system/r made again and the release's boot.iso downloaded, checked (checksum, both files
    signed by Rudolf OS) and staged, at the one after written before anything else, recovery.efi official again and the
    computer starting through the menu; recovery.efi deleted: the same; Developer mode (a TEST key), a custom build in
    slot b and a custom rudolf.efi (the official one unsigned): the custom build starts, its files left alone; Return to
    official (keep): system/r into slot a, the official boot files staged and written, Official, Developer mode off, the
    custom slot cleared. Part 2 (a computer that never ran the official build), 3/3 in run 43917182147: Developer mode
    turned on downloads system/r from the official address (the stable channel, 1.5.9.261115, checked with the official
    key; the browser layer it shares linked), and Recovery mode starts it. Part 3 (Secure Boot, below) 5/5.
    - **Found and fixed:** Return to official never cleared the custom build's slot. rudolf-update marks the return
      (ret=1 in os/system/state), but the start-up rewrote that file with its own five lines when it tried the official
      system, dropping the mark, so boot-ok never ran return_done. initramfs/init's st_write keeps the lines it does not
      own (startfall58_t.sh 8b; in QEMU the start after the return ran with this tree's start-up script and the slot was
      cleared). It needs a kernel built from this tree (the next beta).
    - **Made sturdier:** espcheck (the start-up files' repair) runs at the desktop's first heartbeat, often before
      Wi-Fi is connected; a failed download then waited a day. It now waits for the network (10 tries a minute apart)
      and counts only a downloaded image that was refused against the day (esprepair60_t.sh 23). In QEMU the network was
      up in time, so this path is host-tested only.
    - **The EFI image: 27.0 MiB** (55296 sectors) of the 32 MiB (65535 sectors) an El Torito entry can name, on
      2026111611. mkimage.py cut a larger entry short without a word; it now stops above 32 MiB, warns from 30 MiB and
      prints the size (efisize60_t.sh 3, and the size of the release's image in the QEMU workflow).
    - **The owner's checks on the Vivobook** (with the owner present, on the next beta): Settings › About says Official;
      the boot menu's Recovery mode starts (the official system, "checked" on the screen) and Restart goes back; with
      the network on, a normal start leaves System health with no "start-up files" line. Changing recovery.efi on the
      computer and Developer mode there only if the owner wants it; nothing else is needed on the hardware.
  - **Browse with Tor (1.5.9 16.2):** Ctrl+W, T, N and Q in a website in the Tor window on Standard are still
    Chromium's (rudolf-display remaps them only for RudolfDesktop), with a test of rudolf-display's keys.
    **Done (2026-11-16, "Before stable" item 10):** rudolf-display hands them to RudolfTor as to RudolfDesktop (since
    2026-11-15) and rudolf-native grabs them on RudolfTor as on RudolfOracle; tested now with rudolf-display's own key code
    (qt/host/fixtures/dispkeys.c: rudolf-display.c built with its main renamed; a keyboard's presses through its
    handle_event, by window: RudolfDesktop, RudolfTor and a window inside one get Ctrl+F20..F23 and Alt+F20, Shift kept;
    RudolfOracle and Linux apps' windows every key unchanged; Ctrl+A, Ctrl+Alt+W and plain letters unchanged everywhere) and
    in a website in the Tor window on both engines: typed letters reach the site as they are, Ctrl+T a new tab, Ctrl+N a
    new Tor window of the same Chromium (rudolfd's torwin/open; Native's op tor), Ctrl+Q nothing (the site sees Ctrl+F23
    let go: the key was handed on), Ctrl+W the tab closed with the window and Chromium kept. Note: Playwright's Chromium
    153 already gives a shell's page these keys unchanged, so there the website part passes with or without the remap; the
    remap is checked by step 0 and by the site seeing F23. With the beta's own Chromium 158 (tor-keys-check.yml) Native's
    Tor window kept "Rudolf OS" as its window's title, so its frame said "Oracle", not "... - Oracle (Tor)": fixed (the
    page's title through rudolf-launch's binding to rudolf-native, by the window's place). On the Vivobook (owner): in a
    Tor window on Standard and on Native, Ctrl+T, Ctrl+W, Ctrl+N and Ctrl+Q in a website; its title bar says "(Tor)".
  - **Lite (from 1.5.8, through 1.5.9):** the desktop's menus and windows behind Oracle's tab pages (the covered tab's
    still picture while a menu is open); stutter in Oracle's tabs; Google sign-in for GitHub (symptoms to be described
    first); GPU drawing for Lite measured; Oracle's WebKit tabs (YouTube, Google sign-in with FedCM, a download, full
    screen, memory with 5 tabs); the lock and sign-in screen settings and the shutting-down screen; the dynamic
    backgrounds' moving parts.
    **Decided (owner, 2026-11-15): not in 1.6.0.** Native replaces Lite as the desktop; Lite stays as a compatibility
    option in Settings › System health › Desktop engine, kept working as it is, and these are worked on and refined
    in a later release.
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
- **QEMU checks (CI)** (qt/sb/t160_t.py, .github/workflows/qemu-checks.yml: GitHub's runner with KVM, the newest
  beta release or a serial given; nothing published)
  - **Done (2026-11-16), 20/20 on release 2026111506 (1.6.0.261115), Actions run 43783263846:** 1.5.9 15.1 (an account's programs, in a sandbox
    as Flatpak starts one and in Debian, where Wine runs, refused the desktop's page and API); 15.3's third step (two
    accounts, the move of their folders on an existing data partition to their own Linux users, each refused the
    other's files and the display, signing out stops the account's programs); 15.4 (switching the development link on
    needs it allowed and an administrator's password; its commands read-only, as nobody); 8.7 without Secure Boot
    (Developer mode with a test key, a custom release from slot b, Return to official keeping the files); the official
    recovery system checked and made again; 1.5.8 4.2 (the link on again by itself after a restart, Ask first on
    again, a command answered "no desktop to ask" when the desktop sends no heartbeat, and run with Ask first off).
  - What the failures were (all in the test, none in Rudolf OS): the first sign-in of a start was lost to the desktop's
    page loading again a few seconds after it was ready (the test now waits for the same page for 20 s); QEMU's quit is
    a power cut, so the tunnel's binary and the link's state written just before were lost (written to the disk
    first now); the session is rudolf-init's own process and starts the browser again, so "the desktop not running"
    is now a desktop that sends no heartbeat; busybox ash cuts a command line at 1024 characters.
  - **Done (2026-11-16), 8.7 with Secure Boot:** t160o_t.py part 3, 5/5 (runs 43917182147 and 43920580896): OVMF with
    Microsoft's keys and a TEST certificate in db; the release's boot image with its menu, rudolf.efi and recovery.efi
    made again from the release's kernel and signed with the TEST key (recovery.efi carrying the official release key);
    the release starts locked down, every layer through dm-verity; Developer mode on; a custom build in slot b does not
    start under Secure Boot ("the release list is not signed by a trusted key ... only a checked system starts") and the
    computer goes back to the official system by itself; Return to official there (the official system from system/r,
    checked, Developer mode off); Recovery mode (recovery.efi) checks system/r with the official key and starts it under
    lockdown. A custom build under Secure Boot needs its own key enrolled (1.5.9 8.3, as decided); nothing was weakened.
  - Left: qt/sb/t140_t.py (a 1.4.0 test disc) still switches the link on with the token alone (t160's 17 and 18 cover
    15.4 in QEMU).
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

### 16. Beta versions opt-in (added on 2026-11-15)
Asked for by the owner after a computer on 1.5.7 was updated to the in-progress 1.6.0 beta instead of 1.5.9 (its
update address was the beta channel's): "Beta versions should be an opt-in checkbox on the update and recovery
settings section. It should require an administrator's username and password to turn on. If it's turned off, the next
time the computer is updated should bring it to the latest stable release instead."
**Done (2026-11-15):** Settings › Update and recovery › Get beta versions. On: the desktop's administrator prompt, and
rudolfd checks the name and password itself (sys/beta?on=1, as the development link does) before `rudolf-update set
channel beta` sets the official beta address. Off: `set channel stable` sets the official stable address and marks
"tostable": the next check and download take the latest stable release even though its number is lower than the
beta's (the beta's list already seen is forgotten, the stable one is checked with the official key as always), then
the mark is cleared. The official beta address can no longer be typed in as the update address; Developer mode keeps
its own address (refused while it is on). A computer already on the beta address shows the box ticked. Test:
qt/host/beta60_t.sh 12. Manual check: on the HP desktop, untick it and update: 1.5.9 comes next.

## Cellular and ARM64: QEMU and host tests first, hardware after (owner, 2026-11-15)
Asked for: "Set up the plan to cover whatever can be done in QEMU first. Everything that requires hardware testing can
come afterwards in that plan." This replaces the order of Steps 1 to 6 below for Parts 1 to 7. Each stage is one or
more PRs, each with its tests; none is published in a beta before the owner merges it. The first 1.6.0 beta is built
without this work (it waits only for the QEMU security checks of Part 15).

**Stage A: built and proven without hardware (host tests, browser tests and QEMU)**
1. **Modems (Part 1):** ModemManager with QMI, MBIM, QRTR and AT in the system layer (the sizes printed in the build);
   rudolfd's `mobile/*` reading its D-Bus API; the FCC-unlock offer and the partly supported modules' notes in
   Settings. Tested against a stand-in ModemManager on the session's own D-Bus (a modem with a SIM, locked, unlocked,
   one that disappears) and, in QEMU, ModemManager itself starting with udev and no modem (nothing runs, nothing
   logged as an error).
2. **SIM and network (Part 2):** Settings › Mobile network: the PIN and PUK (administrators, three tries, the PUK's
   warning), the APN from mobile-broadband-provider-info by the SIM's operator code (letters' case kept: docs/plan-
   1.6.1.md Part 7) or by hand, roaming off by default, the network type, airplane mode in quick settings. Host and
   browser tests against the stand-in.
3. **The connection (Part 3):** connect and disconnect, connect automatically, the priorities (cable, Wi-Fi, cellular)
   and the failover within seconds, the firewall on the cellular interface, IPv6-only with clat. Tested in QEMU with a
   stand-in cellular interface (a veth or usbnet-like link given the stand-in modem's addresses) beside a virtual
   Wi-Fi link (mac80211_hwsim) that is taken down and brought back: traffic moves to cellular and back, the
   notification, the firewall's rules on it.
4. **Data and money (Part 4):** metered by default, the month's data from the interface counters across a restart (in
   QEMU), the limit's warning at 80 % and data off at 100 %, no automatic update downloads on cellular. Host and
   browser tests.
5. **Signal and status (Part 5):** the taskbar's bars, type and operator, the signal's details in Settings, modem
   events in System health, the self-test's modem line, all from the stand-in's changing values.
6. **Messages (Part 6):** the Messages app (conversations, read and send, delivery reports, multi-part and Unicode,
   notifications, the locked desktop showing only the sender), kept in the account's folder and the encrypted folder.
   Tested with the stand-in's SMS interface (incoming and sent messages) and in the browser.
7. **ARM64 (Part 7):** the ARM64 build with Parts 1 to 6, its own update lists (`beta/arm64/`, `stable/arm64/`) and a
   UEFI disc image, started and checked in QEMU aarch64 (AAVMF): start-up, the desktop, an update between two test
   releases, the stand-in modem's Settings pages. Not published until stage B's ARM64 computer.

**Stage A's notes (what is done, its tests, what stage B checks)**
- **Item 1 (Part 1), done (PR "cell60-modems"):** ModemManager in the base layer with QMI, MBIM, QRTR and AT (its
  plug-ins, libqmi, libmbim, libqrtr-glib, qmi-proxy, mbim-proxy), its FCC unlock scripts and what they run (qmicli,
  mbimcli; bash for MediaTek's): systools/ci_release.sh prints the sizes (largest parts and the total; no budget, as the
  owner dropped them for 1.6.0). /usr/lib/rudolf/modem status reads each modem from ModemManager's D-Bus API (mmcli's
  key-value output: maker, model, firmware, drivers, plug-in, state, the lock and its tries, the SIM and its operator
  code, the operator, the registration, signal, network type, power, the module's USB or PCI id and its FCC unlock),
  and names the modules Linux supports only partly (Intel XMM7360 and XMM7560, MediaTek T700) found on the PCI or USB
  bus. rudolfd's mobile requests (rudolfd-mobile.inc): mobile/status, mobile/fcc (an administrator's name and password,
  checked by rudolfd; kept in /persist/etc/modem-fcc, linked into ModemManager's fcc-unlock.d at its start; System
  health says who). Settings › Network › Cellular modems: the offer, saying what it does, the unlock turned off again,
  the partly supported modules (also with no modem). **The stand-in ModemManager** for every later item: qt/mmstub.c
  (ModemManager's own generated D-Bus code from libmm-glib, on a private bus: modems with a SIM, PIN and PUK, the
  registration, signal and its details, the network types, bearers with the APN checked letter for letter, messages
  with delivery reports, an FCC-locked radio, a modem that disappears) with qt/host/mmstub.sh to run it.
  Tests: qt/host/modem60_t.sh (23 checks: the real mmcli against the stand-in, rudolfd's requests compiled from
  rudolfd.c), qt/host/modem59_t.sh (the 1.5.9 service, 10), qt/shell2/modem60_t.js (12) and modem59_t.js (5), and in
  QEMU qt/sb/cell60_q.py on the newest beta (ModemManager with udev and no modem: not started by itself, started by hand
  it finds no modem and logs nothing; its parts there; stopped), all in .github/workflows/cellular-check.yml. (The
  beta then, 2026111506, was made before Part 9: the check starts udev from the test machine, Ubuntu 24.04's as the base
  layer takes it, as rudolf-init does; with a newer beta it uses the release's own.) Found in QEMU and fixed: BusyBox's
  pgrep -x never matched ModemManager, so 1.5.9 started it again for each port and never stopped it; and it asked
  ModemManager's --help-all (unknown to it) for --test-no-udev, so it always ran with --no-auto-scan.
  Stage B checks: the USB stick and the MiFi in Settings › Network › Cellular modems (maker, model, SIM, operator),
  no "Invalid rule match" and no error in /run/rudolf/modem.log; on a laptop with an FCC-locked card (a ThinkPad with
  a Fibocom L850-GL or Quectel EM05-G), the offer shown, turned on by an administrator, the radio on after a restart,
  and turned off; a partly supported module named.

- **Item 2 (Part 2), done (PR "cell60-sim"):** /usr/lib/rudolf/modem pin, puk (the codes in a root-only file, never on
  a command line), settings, set, apn, airplane; rudolfd's mobile/pin and mobile/puk (an administrator's name and
  password checked by rudolfd; System health: unlocked by whom, a wrong code, the PIN blocked, the SIM blocked for good),
  mobile/settings (the APN: automatic or typed in, its user name and password, never given back; roaming, off by
  default; the network type, given to the modem at once), mobile/airplane. Each SIM's settings in /persist/etc/mobile/,
  a file named by a hash of its ICCID, root's only. The APN: Rudolf's corrections first (/usr/share/rudolf/apn-fixes:
  US Mobile's "PWG", which the database has as "pwg", and Simple Mobile's "Simple", docs/plan-1.6.1.md Part 7), then
  mobile-broadband-provider-info by the SIM's operator code, the operators sharing it told apart by the SIM's name,
  then the network's, then the network's owner; the letters' case kept everywhere. Settings › Mobile network
  (mobile60.js); quick settings' ninth switch, Airplane mode, on both engines (rudolf-native.c draws it and sets it
  through rudolfd; qt/native/qsbeh_t.js turns it on and off on both). Wi-Fi and Bluetooth stay off under it.
  Tests: qt/host/sim60_t.sh (33 checks, the stand-in ModemManager; the carriers' table), qt/shell2/mobile60_t.js
  (24), qt/native/same_t.sh's quick settings cases and qsbeh_t.js.
  Stage B checks: a real SIM with a PIN in the USB stick (unlocked from Settings; a wrong PIN shows the tries left; do
  not try the PUK on a SIM you need), the APN filled in from a prepaid T-Mobile-network SIM (Mint or Tello) and the
  connection made with it, roaming's setting, the network type changed and kept by the modem, airplane mode with the
  Vivobook's Wi-Fi and Bluetooth and the stick, and back.

- **Item 3 (Part 3), done (PR "cell60-connect"):** /usr/lib/rudolf/modem connect and disconnect (ModemManager's
  Simple.Connect with the SIM's APN, user and password, roaming allowed or not; the refusals said: the APN in the wrong
  capitals, roaming), the bearer's interface set up as the modem says (static addresses, or DHCP through udhcpc.sh,
  IPv6, MTU, DNS in RUN/dns/IF), its default route at metric 700 behind Wi-Fi's 600 and a cable's 100, routes on a link
  that went down ignored at once (ignore_routes_with_linkdown); modem watch (started with ModemManager): turns modems on,
  connects automatically ("Connect automatically", on by default; not while roaming unless allowed; not after
  Disconnect until Connect; not in airplane mode), undoes the interface of a modem gone, and notes when the connection
  in use moves to cellular or back (RUN/cell-notes; rudolfd's mobile/notes; the page's notify and the native desktop's
  toast and centre, every 5 s). The firewall: a cellular set (firewall cellular add|del IF): filtered as every other
  interface, never counted as the local network for remote access. IPv6-only: a CLAT (tayga, in the base layer now;
  the kernel's TUN and veth as modules), the NAT64 prefix from the operator's DNS (RFC 7050) or the well-known one.
  netplug leaves a modem's own interface to ModemManager (no DHCP on it). Settings › Mobile network: Connection
  (connected and in use, or in reserve; Connect, Disconnect), Connect automatically; quick settings: 📶 Mobile data
  (with a modem whose SIM is unlocked) on both engines.
  Tests: qt/host/conn60_t.sh (22 checks in network namespaces of its own, TUN links: connect, Wi-Fi in use, Wi-Fi
  down and cellular within seconds, the notices, Disconnect, reconnected after a drop, roaming, the APN's capitals,
  Connect automatically off, IPv6-only with the CLAT through a NAT64), qt/shell2/conn60_t.js (13), qt/native/qsbeh_t.js
  (Mobile data on both engines), and in QEMU (qt/sb/cell60_q.py items 8-14: mac80211_hwsim's two radios, one an access
  point in a far namespace, the stand-in cellular link on two QEMU network cards, the stand-in ModemManager on the
  guest's bus: Wi-Fi carries the traffic, taken down: cellular within seconds, counted on the far end, the notice;
  brought back; Disconnect). The CLAT runs in QEMU only from a release whose kernel has TUN (this PR adds it).
  Stage B checks: the USB stick on a T-Mobile-network SIM: connected by itself, the Vivobook's Wi-Fi turned off and
  on (the notices, the traffic moving), IPv4 sites on T-Mobile's IPv6-only network (the CLAT), the firewall's
  counters with the stick connected, sleep and wake, the stick unplugged and plugged in again.

- **Item 4 (Part 4), done (PR "cell60-data"):** metered by itself (netplug: any interface ModemManager connected,
  RUN/cellular, mhi_net modules too). Counted per SIM: netplug tick (every minute, from the interface's counters) writes
  "YYYY-MM-DD sim:HASH BYTES" beside the monthly lines in /persist/var/lib/rudolf/netusage (a SIM's last 70 days kept);
  /usr/lib/rudolf/modem usage: this SIM's month from its start day (1 to 28), the limit (MB) and how much of it; the
  watch every half minute: a notice at 80 % (once a month), at 100 % mobile data off if chosen ("offfor": not connected
  again by itself this month; Connect goes on, saying it may cost) or a notice; both desktops through mobile/notes.
  Settings › Mobile network › Data this month (the count, a bar amber at 80 %, red at 100 %, the limit, the start
  day, off at the limit). Updates on mobile data: never downloaded by themselves (1.5.8), offered with their size
  ("Download over mobile data: up to 42 MB", sysupd25.js).
  Tests: qt/host/data60_t.sh (13: real traffic counted from the counters, across a restart of the link, the start day,
  80 %, 100 % with and without data off, the next month), qt/shell2/data60_t.js (10), and in QEMU (qt/sb/cell60_q.py
  15-16: traffic over the stand-in cellular link counted for the SIM; the guest restarted, the count read back).
  Stage B checks: the stick's month compared with the operator's own count (T-Mobile's app or text), a small limit
  reached on purpose (the notice at 80 %, data off at 100 %), and an update offered on mobile data with its size.

- **Item 5 (Part 5), done (PR "cell60-signal"):** the watch writes RUN/cellinfo/IF ("operator|type|signal": 5G, LTE, 3G,
  2G from the access technologies) every 6 s; rudolfd's net list marks a cellular interface with its carrier, type and
  signal (also when it has no device, and an IPv6-only one whose default route is the CLAT's); the taskbar's network
  icon draws four bars from the signal (net31.js; rudolf-native.c ic_net the same), "Mobile data — connected (T-Mobile
  · LTE)", and the network pop-up's row (in use, or in reserve). /usr/lib/rudolf/modem signal and rudolfd's
  mobile/signal: ModemManager's Signal interface (asked to measure every 5 s), Settings › Mobile network › Signal (LTE's
  RSRP, RSRQ, SINR, RSSI with words; 5G's, 3G's; every 5 s). System health's "modem" kind (rudolfd --health modem; the
  page names it Mobile network): the SIM taken out and put back, the PIN blocked (wherever it was given), no network for
  a minute, the modem gone, and back within ten minutes (restarted). The self-test's modem line: the modem and its
  driver, the SIM, the network and the data connection (a failure saying where to look for each).
  Tests: qt/host/signal60_t.sh (14), qt/shell2/signal60_t.js (10), qt/host/selftest_t.sh, qt/native/same_t.sh's tray-cell
  and fly-net-cell cases (Native = Standard).
  Stage B checks: the bars and type against the stick's own LED or the operator's app, moving the Vivobook toward a
  window while watching RSRP/SINR, the SIM taken out while running (System health), the self-test's modem line passing.

- **Item 6 (Part 6), done (PR "cell60-sms"):** /usr/lib/rudolf/modem sms (mmcli's JSON in a UTF-8 locale: any
  language), sms-send (the number and text in a root-only file; ModemManager splits long texts; a delivery report asked
  for), sms-delete, sms-preview; the watch notes each new received message once ("💬 New message from NUMBER": the sender
  only, unless an administrator allowed the text: /persist/etc/sms-preview, System health says who). rudolfd: mobile/sms,
  mobile/sms/send, mobile/sms/delete, mobile/sms/preview. The Messages app (messages60.js, a page app: the same window on
  both engines): conversations by number (unread counts, the newest first), bubbles with Sent and Delivered, a new
  conversation by number, the characters and parts counted (GSM-7 160/153, Unicode 70/67); the modem's messages copied
  into the account's folder (.rudolf/messages.json through rudolfd's home/write: in the encrypted folder with "Encrypt my
  data"; the page's own storage for an account that is not on the drive), then deleted from the modem; synced every
  10 s while open and at each new message's notice. The notices of new messages are shown on the lock screen too (both
  engines), the others wait. MMS is left out.
  Tests: qt/host/sms60_t.sh (11), qt/shell2/messages60_t.js (13).
  Stage B checks: texts to and from a real phone through the stick or the MiFi (Unicode, a long one, the delivery
  report), a sign-in code from a service, the notice with the desktop locked, the messages still there after a restart
  (and in the encrypted folder with Encrypt my data on).

- **Item 7 (Part 7, ARM64): not started (next).** What is there to build on: arm64-check.yml (the kernel, the base layer
  from scratch by systools/arm64_base.sh, QEMU aarch64 to the sign-in screen, also started by the UEFI firmware and with
  Standard). What it needs, in this order: (a) arm64_base.sh to take what items 1-6 added to the x86 base in
  ci_release.sh (ModemManager with its plug-ins, libqmi/libmbim/libqrtr, qmi-proxy, mbim-proxy, the FCC scripts,
  qmicli, mbimcli, bash, tayga, mobile-broadband-provider-info), and kconfig-arm64.sh's TUN and VETH (added in item 3);
  (b) rudolf-update's official addresses on ARM64: beta/arm64/ and stable/arm64/ (compiled in by architecture), and the
  release workflow's publishing step prepared for them but not run (make_release.py's channel files laid out under
  arm64/ in a dry run); (c) a UEFI disc image for ARM64 (systools/mkdisc.sh's ESP with vmlinuz.efi, as boot-uefi starts
  it); (d) QEMU aarch64 (AAVMF, qemu-efi-aarch64's QEMU_EFI.fd) checks in arm64-check.yml: start-up and the desktop
  (as now), an update between two test releases signed with a throwaway key made in the test (make_release.py, served
  by a local http.server, the key added as a developer key as qt/sb/t160_t.py does), and qt/mmstub.c built on the ARM64
  runner and run in the guest with MMSTUB_TEST_MACHINE=1 (as qt/sb/cell60_q.py item 3 does) for Settings › Mobile
  network and Network's Cellular modems (screenshots through cdp-eval). Not published: the owner tests it on an ARM64
  computer first (stage B 4). Note: arm64-check's boot-uefi job timed out once (15 minutes to the Lite desktop) on
  2026-11-16 while boot and boot-standard passed; it had failed the same way on another branch the day before.

**Stage B: on hardware, with the owner (after stage A is merged and in a beta)**
**Moved (owner, 2026-11-16): items 1 to 3 and the release checks' real-hardware modem line go to 1.6.1** ("For now,
move the mobile/SIM testing to 1.6.1": docs/plan-1.6.1.md Part 8). 1.6.0 ships stage A's cellular work, proven in
host, browser and QEMU tests; item 4 (ARM64) stays below, see "Before stable".
1. The USB cellular stick (borrowed from the HP desktop's user) and the MiFi 7730L on the Vivobook: the modem found
   (Settings › Network › Cellular modems, no "Invalid rule match" in /run/rudolf/modem.log, the self-test's modem
   line), the SIM's PIN, the APN filled in, connected, Wi-Fi failover and back, data counted, sleep and wake,
   unplugged and plugged in again. With a prepaid T-Mobile-network SIM (Mint or Tello: docs/plan-1.6.1.md Part 7).
2. Messages to and from a real phone through the stick or the MiFi.
3. The FCC-unlock path on a laptop with a WWAN card (a used ThinkPad with a Fibocom L850-GL or Quectel EM05-G: the
   Vivobook has no slot).
4. One ARM64 computer that starts with UEFI, then the first ARM64 release published.
5. The release checks below that need devices ("On real hardware over the devlink").

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
- The leanness budgets (docs/plan-1.5.8.md Part 8) and the cellular budget (15 MB) in the release check (dropped for
  1.6.0 by the owner on 2026-11-15: the sizes are printed instead).

## Steps
0. Part 9 (udev instead of mdev) first, so the modem work stands on it; then Part 10 (a Linux user for each account)
   and Part 11 (elogind and polkit), which builds on Part 10's sessions; Part 14 (the keyring for Linux apps) with them.
1. (Order changed on 2026-11-15: see "Cellular and ARM64: QEMU and host tests first" above; steps 1 to 6 are now its stages A and B.) Parts 1 and 2 on the USB stick and the MiFi (1.5.9's ModemManager): modem found, SIM PIN, APN, connect. First
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

## Before stable (2026-11-16)
Asked for on 2026-11-16: "determine what else is to be done in 1.6.0 before promoting to stable". From the parts above,
Part 15's lists and the Vivobook's beta 2026111611 (the owner's reports of 2026-11-15 and 16).

**Must be done (blocking):**
1. **The Native desktop's open bugs from beta 2026111611** (Native is the default for new installations, Part 12):
   apps from the Library, Wine and Linux not opening from the start menu, icons and pins, their icons missing, Packages
   missing from the start menu, desktop icons slow or not changing, the window and taskbar sounds, quick settings
   (closing on an outside click, the tiles' state; k160_t.py green); the computer check (keys taken by the global
   shortcuts: arrows, F-keys, left Ctrl/Alt/Meta; sound and microphone in WebKit; the graphics step), the system
   report's renderer and desktop size, reports saved to the account's own Documents. Scheduled tasks of 2026-11-16.
2. **Sleep on the Vivobook:** the kernel without CONFIG_AMD_PMC does not really sleep (s2idle on AMD Ryzen 7020), and
   the check's sleep step passed without sleeping. With item 1's second task.
3. **3D Linux and Flatpak apps** (Unvanquished "Couldn't load a renderer.", SuperTuxKart not starting; worked in 1.5.x):
   a 1.6.0 regression (the display's authorization or the per-account Linux users). Scheduled task of 2026-11-16.
4. **A new beta with 1 to 3, then the owner's Vivobook pass on Native and Standard:** docs/native-vivobook-checklist.md
   (#412), the computer check passing, every tonight's report re-checked; the Native QEMU checks (k160) and every CI
   check on main green.
5. **Part 12's decision:** Native as the default for new installations after its week of betas (#384): the owner's
   call once 4 is clean; if not, new installations start on Standard for 1.6.0 and Native stays the choice in Settings.
   **Decided (owner, 2026-11-16): "Native is going to be the default for new installations."** As #384 made it: a new
   installation starts on Native (on trial, Standard if it does not come up); an update keeps the computer's engine and
   offers Native once.
6. **Part 13's QEMU checks (13.7):** an extension in use after a restart, a broken one left out, an update with and
   without its extension: the update path must be safe before stable. **Done (2026-11-16):** qt/sb/t160x_t.py 20/20 on
   the real release (Part 13's note); one fix (the extensions' lines in System health). Left for 1.6.1: rudolf-update
   fetching the extensions' builds for a new base before switching (an update leaves them out until then, safely).
7. **The official recovery (Part 15, 1.5.9 8.4 and 8.5):** the repair at each start (recovery.efi and system/r checked
   and restored from the official release), Return to official restoring rudolf.efi; then recovery.efi on the Vivobook
   with the owner present; 8.7 with Secure Boot in QEMU (OVMF with test keys). **Done in QEMU (2026-11-16):** t160o_t.py 21/21 on the
   real release (Part 15's note): the repair, Return to official, Developer mode on a computer that never ran the
   official build, and 8.7 with Secure Boot; one fix (Return to official now clears the custom slot; needs the next
   beta's kernel), espcheck waits for the network, the EFI image's size checked (27.0 of 32 MiB). Left: the owner's
   check of recovery.efi on the Vivobook (Part 15's list), on the next beta.
8. **The Vivobook checks carried from 1.5.9 (Part 15):** calls (Meet, Zoom, Teams, a real phone, a headset, memory over
   10 minutes; the owner does these); Tor at check.torproject.org and its window; the microphone Test; Save and Open;
   WordPro's links; a USB stick in File Explorer; the Wi-Fi radios; 15.2's test; game mode and memory.

**Decided by the owner on 2026-11-16:** "Oracle on Native and Tor window shortcuts can be in the 1.6.0 build. USB
printers and ARM64 can be delayed until 1.6.1."
9. **Part 12.3c:** same_t.sh's comparison of Oracle's window on Native against Standard's (toolbar, tabs, a site).
   **Done (2026-11-16):** qt/native/same_oracle_t.sh (with same_oracle.py) 28/28 here with Playwright's Chromium, and in
   CI (oracle-same-check.yml) with Playwright's and with the beta's own Chromium 158. What was different, fixed: with
   Rudolf's Chromium 158 Oracle's title bar and taskbar button on Native said "Oracle" for every page (and the Tor window's
   had no "(Tor)": fixed with item 10, whose Tor test found it too); in every website tab but an account's first, on both
   engines, Ctrl+T, Ctrl+W and Oracle's other keys did nothing and the tab's title was not shown (the shell's content
   scripts). Left: the owner's checks on the Vivobook (Part 15).
10. **Browse with Tor:** Ctrl+W, T, N and Q in a website in the Tor window on Standard are Rudolf's, as in its other
    windows (rudolf-display's keys), with a test. **Done (2026-11-16):** the keys were already handed on (rudolf-display:
    RudolfTor as RudolfDesktop; rudolf-native: RudolfTor as RudolfOracle, Part 15); the tests are new: qt/host/torwin59_t.sh
    17/17 (0: rudolf-display's own key code by window class, fixtures/dispkeys.c; 10: the keys in a website in Standard's Tor
    window) and qt/native/tor_native_t.sh 20/20 (8: the same on Native), with Playwright's Chromium and with the beta's own
    (tor-keys-check.yml). Found with the beta's Chromium 158 and fixed: on Native a Tor window's title (frame and taskbar)
    had no "(Tor)", and Oracle's said "Oracle" whatever the page (Chromium 158 keeps the window's own title at the
    shell's name): the page now tells the native desktop its title through rudolf-launch's binding ("title X Y W H ...",
    the window found by its place). Left: the owner's check on the Vivobook (Part 15).

**Asked for on 2026-11-16 (must be done before stable):** "Can a button be added to the update and recovery settings to
"Roll Back to Last Stable Version" that only appears when get beta versions is opted in or when an experimental build the
repository doesn't recognize as a stable/promoted version is detected to be running?" and, once the design was described:
"That's exactly the purpose it's going to serve for 1.6 onwards, is exactly how you described it."
11. **Roll back to the last stable version** (Settings › Update and recovery; the PR "Roll back to last stable version").
    - **Shown** only when Get beta versions is on, or when the running system is not a release the update server lists
      as published stable: official and its serial the stable channel's, or in `stable/history.txt` (every release ever
      promoted, signed with the release key by the release workflow's promotion and at each run, seeded from the pages'
      history: iso/systools/stable_history.py); both lists checked with the official key only, a history kept only when
      it still names every release the kept one names. Hidden otherwise.
    - **What it does:** checks for updates; says which version ("Rudolf OS 1.5.9.261115 (stable, released 2026-11-15)"),
      whether it is still on the computer or downloaded first, and what the older release does not use; an
      administrator's name and password (checked by rudolfd: `POST sys/tostable`); then that release from the other
      system slot when it holds exactly it (signed, complete), else downloaded into the spare slot as updates are (only
      the changes, every file checked against the signed list); Get beta versions off; Restart now. On a developer's
      build it is Return to official Rudolf OS. `rudolf-update stable-info` and `to-stable`.
    - **The data check:** each release list now says the layout of the data the release keeps and the layouts it reads
      (`datafmt 2`, `readsfmt 1-2`, from iso/datafmt); the computer keeps the newest layout that started on it
      (/persist/os/datafmt). Layout 1: every account's files the desktop's Linux user's (before 1.5.9); layout 2: each
      account its own Linux user, its folder and files its own (1.5.9, docs/plan-1.5.9.md 15.3). A release that cannot
      read this computer's data is refused with why, nothing changed. **1.6.0 -> 1.5.9: allowed.** 1.6.0 keeps the
      accounts as 1.5.9 does (Part 10 shipped in 1.5.9); what 1.6.0 adds lies beside them and 1.5.9 leaves it alone (the
      keyring's sign-ins: apps may ask again; system extensions: not used; Messages and the month's mobile data: kept,
      not shown), said in the prompt. 1.5.9 was published without the record: known by its version to read layouts 1
      and 2. No reverse step is needed for 1.5.9; 1.5.8 and older (layout 1 only) are refused.
    - **Tests:** qt/host/tostable60_t.sh, qt/shell2/tostable60_t.js, and in QEMU on the real releases
      qt/sb/t160r_t.py (workflow "Roll back to stable check", 12/12: run 43911026403): the beta rolled back to 1.5.9, the account made on the
      beta opened by 1.5.9, back up to the beta, the refusal with 1.5.8's real signed list.
    - **For the owner on the Vivobook:** the button on the beta; hidden with Get beta versions off on a stable release;
      its prompt. A real roll back to 1.5.9 only if the owner chooses (it replaces the beta until it is installed again).
- **Moved to 1.6.1:** Part 8 (USB printers: extensions.yml's first run to publish `usbprint`, then the HP desktop user's
  printer; docs/plan-1.6.1.md Part 9) and Part 7 (ARM64: its first release on one ARM64 computer that starts with UEFI;
  with the phone, docs/plan-1.6.1.md Part 4). 1.6.0 ships for x86 computers, and prints to network printers (IPP
  Everywhere and AirPrint through rudolf-print, as since 1.5.x) and to PDF.

**Moved or not in 1.6.0 (already decided):** USB printers and ARM64 (above); Lite's leftovers (1.6.1 Part 3); mobile data and the SIM on hardware
(1.6.1 Part 8); the hotspot over the MiFi (1.6.1 Part 8); Waydroid (1.6.1 Part 6); the first phone (1.6.1 Part 4);
several monitors (1.6.1 Part 1).

**Can wait (not blocking):** the lighter desktop page's measurements (1.5.9 10.3, 10.4); kernel tuning; rudolfd's root
work split into helpers; the page log and the Tor window's log kept across a restart; t140_t.py's link switch; the
keyring test's one-off failure (timing) watched.

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
