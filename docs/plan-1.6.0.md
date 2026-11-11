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

## Hardware to test with
- **The MiFi 7730L** (USB tethering and Wi-Fi), with an active SIM (or a prepaid data SIM).
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
1. Parts 1 and 2 on the USB stick and the MiFi (1.5.9's ModemManager): modem found, SIM PIN, APN, connect.
2. Part 3 (priorities and failover) and part 4 (data and money).
3. Part 5 (signal and status) and the self-test.
4. Part 6 (Messages).
5. The FCC-unlock path on a laptop module.
6. Part 7: the ARM64 release with all of the above, after QEMU aarch64 and one ARM64 computer.
7. Betas on the Vivobook (with the USB stick and the MiFi) and the test laptop; promoted when the release checks pass.
