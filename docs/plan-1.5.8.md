# Rudolf OS 1.5.8: diagnosing a computer that does not start, and smoother recovery

Asked for on 2026-11-10, after the Vivobook could not start the 1.5.7 beta with the Lite desktop engine. The safeguards
that stop such a failure from leaving a computer unable to start went into 1.5.7 (priorities 1 to 3 below); this plan
is the rest (priorities 4 and 5). It starts once 1.5.7 is promoted. Added the same day: **mobile data** (Part 6, after
looking at a Verizon MiFi 7730L), **Android apps** (Part 7: the .apk work delayed from 1.5.7, with workarounds) and
**mobile form factors** (Part 8: x86 handhelds and tablets first, then a lean phone-capable Rudolf OS over the next
versions).

## What happened (2026-11-10)

1. Lite (WebKit) was chosen for the desktop. On the Vivobook's graphics it closed as it started, every 7 to 9 seconds;
   the session started it again without end, because its safety net only caught a desktop that hangs. The screen
   stayed grey and the computer never reached the sign-in screen. Lite had been tried on Xvfb and in Debian's own
   WebKit (cog), never on the Vivobook's real graphics.
2. Recovery mode skipped the installed system for the drive's own, and an installed computer's drive has boot files
   only: "this drive has boot files only (1.2.8)". The rescue stick at hand had boot files only too, and both drives
   showed the same message, so it took three tries to see which drive had started. Fast Boot also skipped the stick
   until it was chosen in the ASUS boot menu.
3. A full 1.5.6 stick (written with Rufus in DD mode from the Asus desktop) got through. Started in Recovery mode, it
   ran the stick's 1.5.6 with the Vivobook's 1.5.7 files: the desktop asked for the shell again and again and kept
   restarting. Its updater then put 1.5.7 in the spare slot taking the Chromium layer from the stick, and scheduled it;
   without the stick that slot could not start, and the start-up stopped at the same boot-files-only message (it went
   back to the good system at the following start only because the slot's tries had run out).
4. Through the devlink the Lite choice was removed, and the computer started 1.5.7 on the standard engine. The speaker
   chip then did not answer at start-up ("Codec #0 probe error", after the forced power-offs); a full shut-down fixed it.
5. During the failure, the devlink never answered while the screen was grey, and the session's and Lite's logs were
   in /run, lost at each restart.

## Done in 1.5.7 (priorities 1 to 3)

- **A desktop that cannot start is given up** (session: `desk_exited`, `webkit_trial_*`, `shell_switch_limit`;
  test qt/host/deskfall57_t.sh): Lite closing 3 times in a row as it starts, or not having answered before the
  computer was switched off, is turned off for good (Settings says when and why); the shell likewise until the next
  start; a desktop asking for the shell more than 3 times in 5 minutes is not obeyed.
- **A system that cannot be put together goes back to the previous one** (initramfs: a slot missing a layer is
  given up like a damaged one; test qt/host/startfall57_t.sh).
- **Recovery mode starts the installed system with safe settings** and changes nothing (initramfs; the session
  already leaves out Lite, the shell and the graphics driver in recovery). Needs 1.5.7 boot files.
- **Recovery and stick starts never change the installed system** (rudolf-update: no download in recovery, or from a
  disc that is not on RUDOLFDATA's drive; boot-ok does nothing in recovery; Settings says why; tests
  qt/host/updsafe57_t.sh and qt/shell2/updsafe57_t.js).
- **Updates are self-contained and checked complete** (rudolf-update: a layer shared with a removable disc is copied
  into the slot; a slot is only scheduled when every file is there at its listed size).

## Priority 4: diagnosing a computer whose desktop does not start

### 4.1 Start logs that outlast a restart
- Keep the session log, Lite's log (webkit-desktop.log), the start-up's choice of system (which slot, why, recovery or
  not) and the kernel's warnings of the last 4 starts in /persist/var/log/starts/ (rotated like rudolfd.log), written
  as they happen, not at shut-down.
- The initramfs writes its decisions (slot chosen, tries left, layer missing, slot given up) to RUDOLFDATA before
  kexec, so a start that never reaches rudolf-init still leaves a record.
- System health shows "Last starts" with the outcome of each; the self-test and Blitzen's diagnostics read them.

### 4.2 The devlink and remote access without the desktop
- Find why the devlink did not answer at the grey screen (it is turned on from the desktop; check whether rudolfd
  starts it at boot when its time is left, and whether it waits for the desktop or the network the desktop sets up).
- Start the devlink and remote access from rudolf-init when they were on (time left, or remote access allowed),
  independent of the desktop process, once the network is up.
- Remote access serves the X screen, so it shows a grey or broken desktop too: make sure it does not depend on the
  desktop page either.

### 4.3 Emergency keys in rudolf-display
- A key combination handled by rudolf-display itself (which reads the keyboard before X, as Ctrl+Alt+Del does) that
  switches the desktop back to the standard engine at once and restarts it: for example Ctrl+Alt+Shift+F12, with an
  on-screen line from rudolf-display saying it did.
- The same keys in Recovery mode do nothing (already safe).

## Priority 5: smoother recovery and fewer surprises

### 5.1 Rescue media that say what they are
- Make a stick writes a full rescue system by default; a boot-files-only stick stays possible but says so on its
  boot menu title and when it starts.
- Start-up messages name the drive they are about ("the USB stick SanDisk 64 GB" / "this computer's drive") and say
  what to do next. The "boot files only (1.2.8)" message drops the version (it is when the feature came, not the
  drive's version) and says whether a rescue stick was found.

### 5.2 Help: starting from a stick
- A Help page: Fast Boot (ASUS and others) can skip USB sticks; use the one-time boot menu (Esc or F8 on ASUS, F12 on
  many others) or turn Fast Boot off; write sticks in DD mode; Windows then calls the stick "not accessible" (do not
  format it); choose Recovery mode in Rudolf OS's menu.

### 5.3 A speaker chip that does not answer
- The self-test checks that every sound card's codec answered at start-up (the kernel's "no codecs initialized" /
  "Codec #N probe error"), and System health says "The sound chip did not answer at start-up: shut down fully (not
  restart), wait 10 seconds and turn the computer on".
- rudolf-init tries once to reset it (unbind and bind the HD Audio device) before the desktop starts.

### 5.4 Test where the risk is
- A rule for the repository: anything that changes how the desktop starts (engine, shell, display) has its fallback
  tested first, on the real graphics path (Xorg with the GPU, in QEMU with virtio-gpu and on the Vivobook), not only on
  the software display; it reaches the Vivobook only after that.
- Found on 2026-11-10 (devlink, Lite run by hand for 15 s): WebKit's web process aborted at once, "Couldn't open
  libGLESv2.so.2": WebKit opens it while running, so addwebkit.sh's ldd pass never saw it. Fixed in 1.5.7
  (addwebkit.sh copies it; the self-test checks it; webkit-check.yml runs Lite from the base layer in a chroot). With
  it, Lite ran, but GTK could not use the GPU ("No available configurations for the given pixel format", Xorg with
  modesetting on AMD): WebKit draws in software. Left for 1.5.8: GPU drawing for Lite (GTK's GL on this X server), and
  its effect on memory and speed.

## Staging toward 1.6.0, the first cellular release (added on 2026-11-10)
Parts 6 to 8 are staged across three versions (docs/plan-1.5.9.md, docs/plan-1.6.0.md):
- **1.5.8 (this plan):** the network groundwork: 6.1 the kernel's USB modem drivers, 6.2 adapters plugged in after
  start-up, 6.3 metered connections; 7.1 the .apk wizard's information, 7.2 scrcpy, the kernel side of 7.3 (binder);
  8.1 begun (the tablet layout in the desktop, with browser tests).
- **1.5.9:** ModemManager (read only), the WWAN kernel subsystem and the operators' database; 8.1 finished (tablet mode,
  on-screen keyboard, rotation); 8.2 the ARM64 build with QEMU checks on a test channel; 6.4 Share this connection;
  7.3 Waydroid in Debian; the first phone prepared.
- **1.6.0:** cellular: SIM and PIN, APNs, connecting, priorities and failover, data limits, signal and status, SMS
  (Messages), airplane mode, the FCC unlock for laptop modules; the first ARM64 release. Voice and emergency calls, and
  phones themselves, in 1.6.x (8.3).
- **6.5** (a hotspot's own status) stays as written: Rudolf OS shows the connection, never signs in to the hotspot.

## Part 6: mobile data (added on 2026-11-10)

Looked at on 2026-11-10: a used **Verizon Jetpack MiFi 7730L** (Novatel/Inseego, January 2017) plugged into the Asus
desktop. LTE Category 9 (3-carrier aggregation, 2x2 MIMO, nano SIM), Wi-Fi 802.11ac on 2.4 and 5 GHz at once (15
devices), a 4,400 mAh battery, a 2.4" 320x240 touchscreen, USB-C and USB-A, a Qualcomm modem (the chip is not
published), and 128 MB of memory with 256 MB of storage. Rudolf OS cannot run on it: far too little memory and storage,
and its firmware is signed and locked (no open firmware such as OpenWrt supports it). Its use is as the **cellular link
of a Rudolf OS laptop or tablet**, over USB or Wi-Fi. On the Asus, Windows listed it as a device but not as a network
adapter (USB tethering off in its menu, or no active SIM), so which USB protocol it uses is still to be seen.

What Rudolf OS lacks for that (checked in the code on 2026-11-10):

### 6.1 USB modems and tethered phones
- The kernel has the USB CDC Ethernet driver (`USB_NET_CDCETHER`) only. Add `USB_NET_RNDIS_HOST` (many hotspots and
  Android phones' USB tethering), `USB_NET_CDC_NCM` and `USB_NET_CDC_MBIM` (newer hotspots and phones), `USB_NET_QMI_WWAN`
  and `USB_SERIAL_OPTION` (cellular modules inside laptops and USB sticks), checked in kconfig.sh like the others.
- ModemManager (with libqmi and libmbim) for cellular modules that need to be told to connect, with the SIM's PIN asked
  by the desktop: **1.5.9** (read only) and **1.6.0** (connecting); hotspots and tethered phones need none of it (they
  hand out an address themselves), so 1.5.8 covers them fully.

### 6.2 A network adapter plugged in after start-up
- rudolf-init asks for an address (udhcpc) only on cables present at start-up. An adapter that appears later (a
  hotspot's USB cable, a phone's tethering switched on, a USB Ethernet dongle) gets none: mdev runs the same DHCP start
  for a new network interface, and stops it when it goes. Settings › Network lists it ("USB modem: MiFi 7730L").

### 6.3 Metered connections
- A connection can be **metered** (cellular data with a cap): set automatically for USB modems and tethered phones and
  for Wi-Fi networks that say so (Android's hotspots set the ANDROID_METERED option in DHCP), and by hand in Settings ›
  Network for any network.
- On a metered connection: no automatic system updates (sysupd25's daily check downloads nothing; Settings says why
  and offers it by hand), no automatic Library, Flathub or Blitzen downloads, no app store catalog refresh in the
  background; the browser and apps work as usual. rudolfd keeps the state (`net/metered`), the desktop reads it.
- Data used on each metered connection this month (from the interface's counters), in Settings › Network.

### 6.4 Rudolf OS as the hotspot ("Share this connection") (1.5.9)
- The Vivobook's Wi-Fi chip (rtw88 8821CE) supports access-point mode. Settings › Network › Share this connection:
  shares the connection Rudolf OS has (cable, USB modem, or Wi-Fi on a second adapter) over Wi-Fi with its own network
  name and password (hostapd, dnsmasq, and the firewall's NAT rules), off by default, administrators only.
- With one Wi-Fi chip, sharing a Wi-Fi connection over the same chip depends on the chip (concurrent station and access
  point): checked on the Vivobook first, otherwise offered only for cable and USB connections.

### 6.5 Battery and signal of a hotspot
- Hotspots and phones expose little over USB: what Rudolf OS can show without signing in to the hotspot is the
  connection itself (speed, data used). A hotspot's own admin page stays the hotspot's (its address in Settings ›
  Network, opened in Oracle); Rudolf OS does not sign in to it or keep its password.

## Part 7: Android apps (.apk), delayed from 1.5.7, with workarounds (added on 2026-11-10)

On 2026-11-10 .apk was moved out of 1.5.7: Waydroid needs the kernel's binder, Waydroid and its container tools in the
system layer, and a first download of about 1 GB (the Android system image), which weighs on a 3.4 GB computer while
the desktop is being made lighter. These are the ways to it, cheapest first; the .apk wizard (install56) offers what
this computer can do.

### 7.1 Always: say what the .apk is, and offer what already works
- Opening an .apk shows the app's name, icon, version and the permissions it asks for (read from the file with
  `aapt2 dump badging`, from Debian's Android tools, or a small reader of the APK's manifest in rudolfd), never
  running it.
- It offers what Rudolf OS already runs: the app's **web version** (many Android apps are also websites or web apps;
  Oracle can keep one as an app in the Start menu) and the **same app for Linux** (Flathub, by name, in the App Store),
  when there is one.

### 7.2 The phone's own apps on Rudolf OS (scrcpy)
- An Android phone plugged in by USB (or on the same Wi-Fi) shows its screen in a Rudolf OS window and is used with the
  mouse and keyboard (with its sound on Android 11 or later): scrcpy (from Debian), over ADB. The apps run on the phone, so nothing is
  installed on the computer and nothing is downloaded beyond scrcpy (a few MB).
- It needs "USB debugging" turned on in the phone's developer options, by the phone's owner: Rudolf OS explains how and
  never changes anything on the phone itself. (Normal ADB only: not the diagnostic or download modes of a phone's chip,
  which can damage it.)
- scrcpy works with Android 5 or later; the phones seen on the Asus (a REDMAGIC 9 Pro, a moto g play) are recent enough.

### 7.3 Android apps on the computer (Waydroid), for computers with room for it
- The kernel side costs almost nothing and can ship first, in 1.5.8: `ANDROID_BINDER_IPC`, `ANDROID_BINDERFS` (and
  `ANDROID_BINDER_DEVICES=""`), with the namespaces and cgroups Waydroid's LXC container uses (most are on already),
  checked in kconfig.sh. Then Waydroid can be added later without a new kernel.
- Waydroid itself in **Debian** (already a download on demand), not in the system layer: `apt install waydroid` in
  Debian's container, with binderfs mounted into it, so the system layer does not grow and computers that never open an
  .apk pay nothing.
- The Android image (LineageOS, about 800 MB) only when the person asks, behind a confirmation with the size, kept in
  the encrypted folder's storage like Debian, and removable in Storage. Offered only on computers with at least 4 GB of
  memory, or with the Lite desktop engine on (it frees roughly 300 to 500 MB), and the memory guard treats Waydroid's
  container as an app it may close.
- Each Android app gets its own window (Waydroid's multi-window mode, its Wayland display shown inside X with a small
  compositor), and a Start menu entry.
- Apps that need Google's services (most from Google Play) may not work; the wizard says so before the download. Waydroid
  with OpenGApps or microG is left to the person (a setting in Waydroid, not something Rudolf OS installs).

### 7.4 Lighter, experimental: Android Translation Layer
- The Android Translation Layer (used by postmarketOS) runs some simple Android apps as ordinary Linux programs, with no
  Android image at all. It runs few apps today; looked at again once 7.3 works, as a light option for the apps it
  runs.

### 7.5 Rudolf OS on a phone
- The desktop's browser build (iso/www/index.html) already runs in any web browser, without rudolfd. On a phone it could
  be added to the home screen as a web app (a manifest and a service worker, so it opens offline and full screen), with
  the touch layout the desktop has for tablets: Notepad, WordPro, the games, Blitzen's packs and the files kept in the
  browser. Rudolf OS as a system on phone hardware (its own kernel on a phone) is not planned: phones' boot loaders are
  locked and their drivers closed.

### 7.6 iPhone apps (.ipa)
- Unchanged from 1.5.6's plan: they cannot run outside Apple's devices; the wizard says so and looks for a web or Linux
  version.

## Part 8: mobile form factors, and a phone-capable Rudolf OS (added on 2026-11-10)

The aim: Rudolf OS on handhelds, tablets and, within the next few versions, phones, while staying lean (a phone has 4 to
8 GB of memory and runs on a battery). Two routes, the cheap one first.

### What stands in the way today (checked in the code on 2026-11-10)
- **x86 only:** the release builds x86-64 layers, and x86 paths are written into ci_release.sh, addwebkit.sh, addnvk.sh
  and the self-test (`/usr/lib/x86_64-linux-gnu`). Phones are ARM64.
- **One kernel for every PC**, started by UEFI (systemd-boot, a signed UKI, Secure Boot). Phones need a kernel and a
  device tree per model, started by the phone's own boot loader (Android boot images, or U-Boot on the PinePhone Pro).
- **A desktop for a mouse and keyboard:** touch works (rudolf-display reads touchscreens), but there is no on-screen
  keyboard, no portrait layout, no gestures, and windows are sized for a large screen.
- **No phone services:** no cellular modem handling, calls or SMS, no suspend that keeps the modem listening, no screen
  rotation from the sensors, no phone cameras.
- **Weight:** the Chromium layer (about 130 MB compressed) and the standard engine's memory (about 830 MB on the
  Vivobook); Debian, Wine and Steam are already downloads on demand.

### Leanness rules for every mobile step
1. **Lite (WebKit) is the desktop engine on mobile devices**, not Standard: about a third of Chromium's memory (the 1.5.7
   trial). Chromium becomes a download on demand there, for websites that need it.
2. **Nothing heavy in the system layer:** Debian, Wine, Steam, Waydroid, Blitzen's model, scrcpy and the like stay
   downloads on demand, behind a confirmation with the size, removable in Storage.
3. **Measured on the device, against budgets:** the system layers under 400 MB compressed; the desktop at the sign-in
   screen under 400 MB of memory with Lite (about 520 MB in the release check on 2026-11-10: WebKit's web process
   389 MB, its UI 94 MB, its network process 35 MB, on Xvfb; reducing it is part of 8.2); idle power measured over an hour with the screen off. A release that
   breaks a budget says so in the release check, like the Lite engine check.
4. **The same release, one system per architecture:** one version number and one CHANGELOG; the desktop (the web page)
   is identical everywhere; only the layers below it differ.

### 8.1 x86 handhelds and tablets (1.5.8 to 1.5.9): no port needed
- **Target devices:** Steam Deck, ASUS ROG Ally, Lenovo Legion Go, Microsoft Surface Go and other x86 tablets. They start
  Rudolf OS as it is (UEFI, x86-64); Secure Boot as on any PC.
- **Touch layout** (the desktop): a tablet mode chosen automatically when there is a touchscreen and no keyboard (and by
  hand in Settings › Display): bigger targets, the taskbar as a dock, windows full screen one at a time, swipe from the
  bottom for the Start menu and from the right for notifications, portrait and landscape.
- **On-screen keyboard:** part of the desktop page (it is a web page, so its own keyboard works in every built-in app
  and in Oracle's pages); for X programs (Linux and Windows apps), rudolf-display types its keys through XTest as it
  types the hardware keyboard's.
- **Rotation** from the accelerometer (iio-sensor-proxy, or reading /sys/bus/iio directly) through rudolf-display, which
  already rotates and scales the screen.
- **Mobile data** (Part 6) and **battery care** (1.4.4 on) apply as they are.
- **Tests:** browser tests at tablet and phone sizes (portrait 1080x2340 scaled, touch events); QEMU x86 with a
  touchscreen (`-device virtio-tablet` / `usb-tablet` with multitouch) and a portrait screen; then the Vivobook with the
  tablet mode forced, over the devlink and remote access.

### 8.2 The ARM64 build (built and checked in QEMU in 1.5.9; first released in 1.6.0): Rudolf OS on ARM
- **Build:** the release workflow builds a second set of layers on GitHub's ARM64 runners (ubuntu-24.04-arm): the base
  layer, Lite (WebKitGTK for arm64), rudolfd and the C helpers (they are portable C), and the desktop page unchanged.
  The x86 paths become `$(gcc -print-multiarch)`; addwebkit.sh and the self-test take the architecture from the
  system.
- **Not on ARM64 at first:** Steam and Windows programs (x86 only; Box64 or FEX as a later download, measured first),
  the NVIDIA (nvk) layer, Flash's Ruffle stays (it is WebAssembly).
- **Updates:** the same signed manifest format, with a list per architecture (`beta/arm64/manifest.txt`); the updater
  takes its architecture's list and never another's.
- **QEMU checks (what QEMU can confirm):** `qemu-system-aarch64 -M virt` with UEFI (edk2), virtio-gpu, a virtio touchscreen,
  3 to 4 GB of memory and a portrait screen: the system starts, the desktop runs in Lite, updates and rollback work,
  Recovery mode and the safeguards work, and a USB modem passed through to the virtual machine (the MiFi 7730L) gets
  an address. **What QEMU cannot confirm:** a phone's display, GPU, modem, cameras, sensors and audio drivers, its
  sleep and wake, battery life and heat, and how its boot loader starts the kernel. Those need the phone.
- **Generic ARM64 computers first:** ARM64 laptops and boards that start with UEFI (Snapdragon X laptops as their
  mainline support matures; the Raspberry Pi 5 with its UEFI firmware) run the same image as QEMU.

### 8.3 Phones (1.6.x): one well-supported phone first
- **Candidates** (boot loader unlockable, good mainline Linux support; checked against the postmarketOS device list when
  the work starts): **PinePhone Pro** (made for Linux; the easiest to work on, slow), **OnePlus 6/6T** (Snapdragon 845,
  long among the best-supported Android phones on mainline Linux; cheap used), **Google Pixel 3a or a newer Pixel bought
  unlocked from Google** (not one sold by Verizon), **Fairphone 4/5** (the maker supports unlocking and Linux ports),
  **Purism Librem 5** (Linux-first, expensive). The test phones at hand are not suitable: the moto g play XT2093DL is a
  carrier prepaid model, normally not unlockable; whether the REDMAGIC 9 Pro's boot loader unlocks is to be checked
  before relying on it. Only a phone that may be experimented on.
- **Per-phone kernel, shared everything else:** a device package per phone (its kernel config fragment, device tree,
  firmware list, boot image format), built from postmarketOS's mainline work where it exists (their kernel configs and
  patches are the reference); the base, Lite and desktop layers are the ARM64 build's.
- **Starting and updating:** the phone's boot loader starts a Rudolf OS boot image (kernel, device tree, the same
  initramfs) from the boot partition; the A/B system slots, the signed manifest, rollback and the safeguards are the same
  as on a PC, kept on the userdata partition as RUDOLFDATA. Installing: from a computer, with fastboot (a guided page in
  Rudolf OS on the PC, and the steps in Help); the phone's boot loader stays unlocked, so it has no Secure Boot: the
  dm-verity check of each layer against the signed list still runs, and Settings says plainly that the phone's boot
  loader is unlocked.
- **Phone services, each a small part:** the cellular modem and mobile data (ModemManager, Part 6), SMS and calls
  (ModemManager's messaging and voice, with a Phone and a Messages app in the desktop; emergency calls only once tested
  on that phone), suspend with the modem able to wake the phone (calls, messages), screen rotation and the proximity
  sensor (iio), the cameras (libcamera, where the phone's are supported), audio routing for calls (ALSA's UCM files from
  the phone's port).
- **The phone shell:** the tablet layout of 8.1 at phone size: one app at a time, the Start menu as an app grid, a
  status bar (signal, battery, time), a lock screen with notifications, the on-screen keyboard.
- **Phone budgets:** the desktop with Lite under 400 MB at the sign-in screen; the system layers for a phone under 400 MB
  compressed; idle drain with the screen off and the modem on measured over a night, compared with postmarketOS on the
  same phone.

### 8.4 Before any of it is offered
- The testing rule (docs/testing.md) applies: the fallback first (on a phone: the previous slot, Recovery mode from the
  boot menu, the safe desktop), tests from the built system, the real hardware path, and the device last with its
  logs readable (the devlink works on any Rudolf OS with a network).
- Each step ships as a test first (as Lite did), and only to devices that pass the release check for their
  architecture.

## Steps
1. 4.1 start logs (initramfs, rudolf-init, session), with a host test of the rotation. **Done** (branch
   v158-startlogs): initramfs `note`/`keeplog` (var/log/boot-decisions.log, last 300 lines, kept before every
   hand-over, restart and stop; /etc/rudolf/initramfs.log), rudolf-init `starts_keep` (var/log/starts/0..3: start.txt,
   initramfs.log, boot-decisions.log, session, Lite, Xorg, devlink and kernel logs; /run's logs link into 0), the
   session's `desk_up_mark`, /usr/lib/rudolf/starts (JSON), rudolfd GET system/starts, System health › Last starts,
   and Blitzen's facts (the start before, the start-up's decisions). Tests: qt/host/startlogs58_t.sh,
   qt/shell2/starts58_t.js. To merge once 1.5.7 is promoted.
2. 4.2 devlink and remote access at start-up; QEMU check with the desktop disabled.
3. 4.3 emergency keys in rudolf-display; host test of the key handling. **Done** (branch v158-startlogs):
   Ctrl+Alt+Backspace held for a second runs /usr/lib/rudolf/desktop-safe (standard engine, no shell, desktop started
   again; Lite off for good with the reason); Help and Settings › Desktop engine mention it. Test qt/host/desksafe58_t.sh.
   (No on-screen line from rudolf-display: with Xorg on the GPU it cannot draw over the screen; the desktop coming back
   and System health say it.)
4. 5.3 sound chip check and reset; 5.1 rescue media and messages; 5.2 Help page. 5.3 **done** (branch
   v158-startlogs): `sound codec-check` (run by rudolf-init 12 s after start-up) resets an HD Audio chip that did not
   answer, once, and says so in System health (hardware); the self-test's sound-chip check names one still silent with
   what to do. Test qt/host/codec58_t.sh.
   5.2 **done**: Help › Starting from a USB stick (rescue). 5.1 in part: the boot-files-only message names the drive
   (USB stick or this computer's drive, with its model) and says what to do; Make a stick writing a full system by
   default is left.
5. 5.4 the testing rule in docs, then the Lite investigation on the Vivobook (devlink plus remote access). The rule is
   **done**: docs/testing.md. The Lite cause was found and fixed in 1.5.7 (libGLESv2); GPU drawing for Lite remains.
6. Part 6, mobile data: 6.1 kernel drivers (kconfig.sh) and 6.2 adapters plugged in later (mdev, udhcpc), with host
   tests; then 6.3 metered connections (rudolfd net/metered, sysupd25, lib52, the download wizards), with browser
   tests. The MiFi 7730L is tried once USB tethering is on (which driver it needs). (6.4 moves to 1.5.9.)
7. Part 7, Android apps: 7.1 the .apk wizard's information and alternatives; 7.2 scrcpy (needs only Debian); the
   kernel side of 7.3 (binder) in the same release; 7.5 the browser build as a phone web app. (Waydroid in Debian:
   1.5.9.)
8. Part 8, mobile form factors: in 1.5.8, 8.1's tablet layout in the desktop with browser tests (the on-screen
   keyboard, rotation and the QEMU and Vivobook checks finish it in 1.5.9); 8.2 the ARM64 build in 1.5.9, first
   released in 1.6.0; 8.3 one phone in 1.6.x (a PinePhone Pro or a OnePlus 6/6T), then the phone services one at a
   time; every step measured against the leanness budgets.
