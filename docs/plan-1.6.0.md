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

## Hardware to test with
- **The MiFi 7730L** (USB tethering and Wi-Fi), with an active SIM (or a prepaid data SIM).
  First (moved from 1.5.9, where it could not be found): with USB tethering on, which USB network driver it needs
  (plan-1.5.8.md 6.1), before Parts 1 and 2 use it.
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
8. Betas on the Vivobook (with the USB stick and the MiFi) and the test laptop; promoted when the release checks pass.

## After 1.6.0: the native desktop decision (added on 2026-11-12)
(2026-11-13: the prototype moved into 1.5.9 as a third desktop engine, Native (test): docs/plan-1.5.9.md Part 12. The
decision below still waits for its measurements on the Vivobook.)
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
