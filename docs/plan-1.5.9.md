# Rudolf OS 1.5.9: the groundwork for cellular (1.6.0) and for mobile devices

Asked for on 2026-11-10, with docs/plan-1.6.0.md (the first cellular release): what 1.6.0 needs, staged between 1.5.8 and
1.5.9. 1.5.8 lays the network groundwork (docs/plan-1.5.8.md Part 6: USB modems' drivers, adapters plugged in later,
metered connections); 1.5.9 adds the modem service, the tablet layout and the ARM64 build; 1.6.0 builds the cellular
experience on them. It starts once 1.5.8 is promoted.

## Status (2026-11-13, after the unattended work session)
Done in code and tests (PRs #260–#281, rudolfos-library #31; each part's "Done" note says what): Part 1 up to the device
check (since #280 ModemManager runs without udev; since #281 the self-test has a modem line), Part 3 steps 1–5 (QEMU
starts the ARM64 build directly, through UEFI firmware and with Chromium as the desktop), Part 4 up to the device
check, Part 7 except the Vivobook call checks, Part 8 except 8.4's recovery.efi and the TPM and hardware keys, Part 9,
Part 10's pictures, fonts and lazy apps, Part 11. Part 12 (the native desktop engine, a prototype) is not started; Part 13 (USB printers) moved to 1.6.0. The
latest beta: **2026111402** (not promoted).

### What needs the Vivobook (or the person) before 1.5.9 is promoted
1. **A beta with everything:** 2026111401: install it; the self-test (its modem line says "no cellular modem").
2. **iw and the radios** (#267): Settings › Network › Wi-Fi › This computer's Wi-Fi radios lists 2.4 and 5 GHz.
3. ~~**The hotspot** (Part 4): with the phone's USB tethering as the connection, Share this connection on the RTL8821CE;
   a phone joins it and browses.~~ Moved to 1.6.0 (2026-11-13: the RTL8821CE offers AP mode and the hotspot says it
   needs another connection to share, both read on 2026111403; the MediaTek phone never offered USB tethering, only
   MTP or ADB, so there was no connection to share. The MiFi's USB tethering is the uplink in 1.6.0).
4. ~~**The modem service** (Part 1): the USB modem stick plugged in: Settings › Network › Cellular modems shows it
   (ModemManager without udev is the open question).~~ Moved to 1.6.0 (2026-11-13: the stick belongs to the HP
   desktop's user and is not available for 1.5.9; 1.5.9 ships ModemManager tested only on the host).
5. **Calls** (Part 7.4): Meet's test call in Oracle on Standard, and on Lite (it opens in Chromium's window); Zoom's
   test meeting; the 🎙️ indicator and its mute; no screen saver during a call; a tel: link.
6. **Game mode and memory** (Part 9, on 2026111308 or newer): FNF to the end; the idle reading; Task Manager's
   breakdown; Clear cache.
7. **Developer mode and Return to official** in QEMU with Secure Boot (8.7), and 8.4's recovery.efi: to be done with the
   person present (it changes how the computer starts).
8. **Part 5 (Waydroid)** measured on the Vivobook before it is offered; **Part 6** waits for the phone (the one plugged
   in on 2026-11-13 identifies as a MediaTek "S24Ultra": an S24 Ultra look-alike, see below).

### The phone plugged in on 2026-11-13
USB `0e8d:2008`, "MediaTek" / "S24Ultra", MTP only: a MediaTek phone presenting itself as a Galaxy S24 Ultra (the real
one is Snapdragon, Samsung's `04e8`). Useful for tethering, Phone screen and file transfer tests. Running Rudolf OS on
it is unlikely (no unlockable bootloader, kernel source or device tree known for such phones); nothing is flashed or
unlocked on it without the person's decision. Next: USB debugging on, then its chip, model, Android version and
bootloader read over the devlink (read only).

## From 1.5.8 (expected done there)
- Kernel drivers for USB networking from hotspots and phones (RNDIS, CDC-NCM, CDC-MBIM) and for cellular modules
  (QMI WWAN, the option serial driver), as modules.
- A network adapter plugged in after start-up gets an address; Settings › Network lists it.
- Metered connections, and what pauses on them.
- The kernel's Android binder (for Waydroid later), scrcpy, the .apk wizard's information.

## Moved from 1.5.8 (2026-11-12)
- **Lite (WebKit):** menus and windows drawn by the desktop behind Oracle's tab pages (branch litetabs: the still
  picture of a covered tab works, but the live page does not step aside under Xvfb; next: swap the page for its still
  picture while a menu is open); stutter in Oracle's tabs in Lite (GPU drawing for the tab views only, the desktop
  staying light); Google sign-in for GitHub in Lite (symptoms to be described first).
- **On the Vivobook:** GPU drawing for Lite measured against software and mixed (memory and speed); Oracle's WebKit
  tabs (YouTube, a Google sign-in with FedCM off and on, a download, fullscreen, memory with 5 tabs against Chromium's);
  the lock and sign-in screen settings and the shutting-down screen; the moving parts of the dynamic backgrounds
  (Spirit Falls' petals and lanterns, Glacier Lake's northern lights) in Lite.
- **QEMU:** the devlink answering with the desktop disabled (plan-1.5.8.md 4.2).
- ~~**The MiFi 7730L** with USB tethering on: which USB network driver it needs (plan-1.5.8.md 6.1).~~ Moved to 1.6.0
  (2026-11-13: the MiFi could not be found; docs/plan-1.6.0.md, Hardware to test with).
- **Kernel tuning** (proposed on 2026-11-12), if confirmed.

## Part 1: the modem service
- **ModemManager** (with libqmi, libmbim and libqrtr) in the system layer, started by mdev when a modem appears (a
  `wwan*` or `cdc-wdm*` device, or a USB modem's serial ports), stopped when the last one goes; D-Bus on the system bus
  (dbus-daemon already runs for the desktop session's needs).
- **rudolfd `mobile/status`:** modems (maker, model, firmware, driver), their SIM (present, locked, operator), state
  (registered, connected), signal, network type: read only in 1.5.9, shown in Settings › Network for each modem. No
  connecting yet: that is 1.6.0, once SIM, APN and metering are designed together.
- **The operators' database** (mobile-broadband-provider-info) in the system layer, for 1.6.0's APNs.
- **Kernel:** the WWAN subsystem (`WWAN`, `MHI_WWAN_CTRL`, `MHI_WWAN_MBIM`, `IOSM` for Intel's newer M.2 modems), as
  modules, checked in kconfig.sh.
- **Budget:** ModemManager and its libraries under 12 MB (10 MB until 2026-11-13: beta 2026111316 came to just over it;
  raised as asked) in the base layer (release check); nothing running without a
  modem.
- **Tests:** host tests with a stand-in ModemManager on a private D-Bus (rudolfd's parsing); the USB stick (1.6.0's test
  hardware) on the Vivobook over the devlink, read only.

- **Done (2026-11-13), up to the device check:** /usr/lib/rudolf/modem (start on mdev's cdc-wdm/wwan/ttyUSB of a modem
  driver, stop when the last goes; status from mmcli), the system bus shared with Bluetooth, rudolfd `mobile/status`,
  Settings › Network › Cellular modems; kconfig.sh's WWAN (MHI_WWAN_CTRL, MHI_WWAN_MBIM, IOSM, checked); ci_release.sh
  copies ModemManager, mmcli, qmi-proxy, mbim-proxy, the plug-ins and serviceproviders.xml with their libraries, and
  fails over 10 MB. Then (same day, without the device): ModemManager runs **without udev**, as on OpenWrt: started with
  `--test-no-udev` (`--no-auto-scan` on an older one), it applies its own udev rules (now in the base layer; the build
  fails without 80-mm-candidate.rules), and the modem service tells it of each port with `mmcli --report-kernel-event`:
  those already there when it starts (the network interfaces too, which mdev does not report), then each one mdev adds
  or removes. Udev-built with no udevd, it would have found no port marked ID_MM_CANDIDATE and seen no modem. To check on
  the Vivobook: the stick shows in Settings › Network › Cellular modems; /run/rudolf/modem.log has no "Invalid rule
  match" (ModemManager 1.22.0's u-blox rules broke its own rule reading, ModemManager issue 779).

## Part 2: tablet mode and the on-screen keyboard (docs/plan-1.5.8.md 8.1, finished)
- Tablet mode: chosen when there is a touchscreen and no keyboard, or by hand; dock, one window at a time, swipes.
- The on-screen keyboard: in the desktop page for its own apps and Oracle's pages; through rudolf-display (XTest) for
  Linux and Windows apps; it appears when a text field gets focus in tablet mode.
- Rotation from the accelerometer, through rudolf-display.
- **Tests:** browser tests at tablet and phone sizes with touch events; QEMU x86 with a touchscreen and a portrait
  screen; the Vivobook with tablet mode forced.

## Part 3: the ARM64 build (docs/plan-1.5.8.md 8.2)
- The release workflow builds ARM64 layers on GitHub's ARM64 runners, beside the x86 ones: base, Lite, rudolfd and the
  C helpers, the desktop page unchanged. The x86 library paths become the build machine's multiarch name.
- Not published yet: a **test channel** (`beta-arm64/`) used only by the QEMU checks.
- QEMU aarch64 checks in the release workflow (UEFI, virtio-gpu, a virtio touchscreen, 3 GB of memory, portrait): the
  system starts, the Lite desktop reaches the sign-in screen, an update installs and rolls back, Recovery mode and the
  safe desktop work.
- **Not on ARM64:** Steam, Windows programs, the nvk layer (see docs/plan-1.5.8.md 8.2).
- **Step 1 (2026-11-13):** the workflow *ARM64 check* (GitHub's ubuntu-24.04-arm): the C helpers and rudolfd build and
  run on ARM64 (rudolf-kexec with ARM64 system calls, Flathub's arch in rudolfd and rudolf-apps, ci_release.sh's
  library paths from the build machine's multiarch name); the kernel (kconfig-arm64.sh, on arm64's defconfig) and its
  modules build (systools/arm64_kernel.sh); the kernel starts in QEMU as far as Rudolf OS's start-up.
- **Step 2 (2026-11-13):** the base layer from scratch on the ARM64 runner (systools/arm64_base.sh, the *base* job):
  BusyBox, the X server, PulseAudio 16.1, the Wi-Fi tools, the system programs and WebKitGTK; mkrootfs.sh and the
  build scripts use the machine's multiarch name and program loader (x86 unchanged). Every program and library in the
  layer finds its libraries through the layer's own loader, and Xorg, PulseAudio, wpa_supplicant and the memory guard
  run inside it: base.sfs 214 MB (758 MB unpacked). Not in it yet: Chromium, Flatpak, speech, Blitzen's engine,
  firmware. Next: the layers started together in QEMU to the Lite sign-in screen, then the test channel.
- **Steps 3 and 4 (2026-11-13): the ARM64 system reaches the Lite sign-in screen in QEMU** (run 43445321597; the
  *boot* job: QEMU virt under TCG, virtio graphics, the four layers from a disc, Lite in 132 s). Chromium from the
  snap's arm64 build is in the layer and renders a page; that snap is built on core26 (glibc 2.43), so it brings its
  base's C library in /opt/chromium/glibc (only Chromium uses it). Fixed on the way: the session gave up before a
  display adapter's driver made /dev/fb0 (now waits up to 30 s); with rudolf.debug=1 the start-up and the session are
  traced.
- **Step 5 (2026-11-13):** the ARM64 check also starts the system **by the UEFI firmware** (QEMU_EFI.fd: the kernel's
  EFI application on an EFI system partition, started by the firmware's shell; Lite sign-in at 388 s under TCG) and
  with **Chromium (Standard) as the desktop** (sign-in at 166 s): runs 43466031954 and 43478068386. Next (needs your
  OK): the test channel `beta-arm64/`; then the update and recovery checks on it.

## Part 4: Share this connection (docs/plan-1.5.8.md 6.4)
- Rudolf OS as the hotspot: hostapd and dnsmasq in the system layer (about 1 MB), the firewall's NAT rules, off by
  default, administrators only. First checked on the Vivobook's Wi-Fi chip over the devlink (access point alone, and
  with a Wi-Fi connection at the same time).

- **Done (2026-11-13), up to the device check:** /usr/lib/rudolf/hotspot (status, start, stop): AP mode and "both at
  once" read from iw; the Wi-Fi itself shared only with a chip that can do both (a second interface on the
  connection's channel), otherwise a cable, a phone's tethering or a modem, with the Wi-Fi as the access point (its
  connection let go and given back); hostapd (WPA2/WPA3, the name as hex), dnsmasq on 10.42.0.0/24, NAT and a forward
  chain in its own table, the firewall's `hotspot` chain for DHCP and DNS; the password kept root-only for next time;
  wifi.sh does not start a connection on the access point. rudolfd `net/hotspot` (the request through a root-only
  file); Settings › Network › Share this connection. ci_release.sh adds hostapd, hostapd_cli and dnsmasq with a budget of
  their own, 6 MB (beta 2026111314 stopped: with ModemManager's they came to 13 MB, over the shared 10 MB; ModemManager
  keeps its 10 MB). Checked on the Vivobook (2026111403): the RTL8821CE offers AP mode, and not AP and a Wi-Fi
  connection at once, so a cable, USB tethering or a modem is shared. The full check (a device joins and browses)
  moved to 1.6.0, with the MiFi's USB tethering as the connection (the MediaTek phone does not tether over USB).

## Part 5: Android apps with Waydroid, in Debian (docs/plan-1.5.8.md 7.3)
- Waydroid installed into Debian on demand, binderfs mounted into Debian's container, the Android image downloaded
  only on request; measured on the Vivobook with Lite on before it is offered.

## Part 6: the first phone, prepared (no release)
- Choose and buy the phone for 1.6.x (a PinePhone Pro or a OnePlus 6/6T: docs/plan-1.5.8.md 8.3), and a USB cellular
  stick with a Quectel EG25-G (the PinePhone's own modem) for 1.6.0, so the modem work is tested on the phone's modem
  family before the phone runs Rudolf OS.
- Its device package started from postmarketOS's (kernel config, device tree, firmware list); it boots the ARM64 build
  in a test only.

## Part 7: video calls and calls to phones (added on 2026-11-11)
Asked for: whether Rudolf OS runs Skype or Zoom and can call real phones from the desktop. Skype closed in May 2025
(Microsoft moved its users to Teams). Calls work through websites in Oracle and through Linux apps; calls to phone
numbers need the service's own calling plan (Google Voice in the US, Zoom Phone, Teams Phone), not anything in Rudolf OS.

### 7.1 What has to work
- **In Oracle's tabs, Standard (Chromium):** Zoom's web client (app.zoom.us), Microsoft Teams (teams.microsoft.com,
  teams.live.com), Google Meet and Google Voice (voice.google.com: calls to US and Canada phones). Chromium has
  WebRTC, echo cancellation and noise suppression built in.
- **In Oracle's tabs, Lite (WebKit, 1.5.8 Part 9):** the same sites. WebKitGTK's WebRTC uses GStreamer
  (webrtcbin, libnice, DTLS/SRTP): addwebkit.sh copies only webrtcdsp today, so the WebRTC plug-ins (webrtc, nice,
  dtls, srtp, sctp, rtpmanager, rtp, opus, vpx) and their libraries are added and measured against Lite's memory and
  the base layer's size. A site that WebKit cannot run goes to Chromium's window (Open in Chromium, or added to the
  Chromium list like Widevine sites once shown to fail).
- **As Linux apps:** Zoom's Linux app (Flathub us.zoom.Zoom) from the App Store, checked to install and start on
  Rudolf OS's Flatpak, with its camera, microphone and screen sharing (X11).
- **Permissions:** the desktop's camera and microphone questions for each site (shell and rudolf-webkit: `perm`);
  remembered per site as Chromium does, and listed in Oracle's site settings with a way to take them back.

### 7.2 Sound and camera during a call
- **Devices:** the microphone and speakers chosen in the taskbar's sound menu are the ones a call uses (PulseAudio's
  default source and sink); switching during a call moves the call's streams (`pactl move-sink-input` /
  `move-source-output`) without dropping it. Headsets: USB and Bluetooth (HFP/HSP: the headset's microphone, not only
  its earphones; PulseAudio's Bluetooth module with the HFP profile, checked on the Vivobook's Bluetooth).
- **"In a call" in the taskbar:** while a site or app uses the microphone, a 🎙️ indicator with the app's name, and
  one tap to mute the microphone for everything (the source's mute), as phones do.
- **Echo:** Chromium cancels it itself; for Linux apps without their own, PulseAudio's echo-cancel module offered in
  Settings › Sound (off by default: it costs processor time).
- **No sleep or screen saver during a call:** a call (a microphone in use, or a WebRTC call in Oracle's tabs) holds
  off the screen saver, the lock and sleep, as video playing does (lock57's `busy`).
- **The camera:** the laptop's camera (UVC) seen by both Chromium and Linux apps; the self-test says when the camera
  is missing or in use by another app.

- **Done (2026-11-13):** the Lite engine check's call test found Ubuntu's WebKitGTK 2.52.6 **without WebRTC**
  ("Can't find variable: RTCPeerConnection", with `enable-webrtc` on), so with Lite the call sites (Meet, Voice, Zoom's
  web client, Teams, Skype, Whereby, Jitsi) open in Chromium's window (oracle58.js RT_CALLS, only while the engine has
  no RTCPeerConnection); no WebRTC plug-ins are added. The Standard call test (webrtc59_t.js, Chromium's stand-in
  camera and microphone) passes; the Lite one reports whether a later WebKitGTK has WebRTC. Also: the
  🎙️ indicator and mute (hotkey mic users|get|muted, rudolfd `mic`); a call holds off the saver, lock and sleep
  (`media`'s `mic`); the sound output change moves streams and the microphone (sound set_default), and
  module-switch-on-connect. Then (same day): a Bluetooth headset's microphone (bluealsa's Hands-Free and Headset
  profiles when it has them; the SCO microphone as PulseAudio's bt_in_MAC; while an app records from it, what plays
  moves to the headset's call output and back to A2DP after); the echo canceller in Settings › Sound › Calls (Adrian's,
  built into PulseAudio; off by default; follows the sound card; not on a Bluetooth headset, which cancels echo
  itself); the self-test's camera line (which app has it open; switched off; connected without its driver). Also
  fixed: a Bluetooth output whose name had a space ("Galaxy Buds") failed to load. Still to check on the Vivobook:
  a real headset's microphone in a call (7.4).
### 7.3 Phone numbers
- Settings › Apps › "Calls to phones": a short page on what each service needs (Google Voice: free in the US with a
  Google account; Zoom Phone, Teams Phone: paid), with links that open in Oracle. Nothing is installed or signed up for
  by Rudolf OS.
- **Done (2026-11-13):** the Settings › Apps page; `tel:` links: both engines' page scripts catch a click on one (a
  person's click, isTrusted) and send the tab event `tel`; the desktop asks: Google Voice, Teams, Phone screen (the
  number copied for the phone's dialler) or copy, with "Always use this" (S.telApp, undone in Settings › Apps).
- `tel:` links (a phone number on a web page): offered to the calling site the person chooses once (Google Voice,
  Zoom, Teams), or to Phone screen (1.5.8 Part 7) to call from the Android phone on the desk.

### 7.4 The call test
- **Automatic, in CI (both engines):** a test page with a WebRTC call between two tabs (a loopback peer connection
  with a fake camera and microphone: Chromium's `--use-fake-device-for-media-stream`, GStreamer's `videotestsrc` and
  `audiotestsrc` for WebKit): the connection comes up, audio and video arrive, and the permission question was asked.
  Added to webkit-check.yml (Lite) and to the shell's tests (Standard).
- **On the Vivobook (devlink and remote access), before 1.5.9 is promoted:**
  1. Google Meet test call (meet.google.com, "Check your audio and video") in Oracle, Standard: camera, microphone and
     speakers work; the 🎙️ indicator shows; mute works; the screen saver does not start during it.
  2. The same on Lite (WebKit tabs).
  3. Zoom's web client test meeting (zoom.us/test) on both engines; then the Zoom Linux app from the App Store.
  4. Teams (a free teams.live.com meeting) in Oracle.
  5. A call to a real phone: Google Voice from Oracle to the person's own phone (if they use Google Voice), or Zoom's
     or Teams' calling if they have it: both directions heard, no echo, switching to a headset during the call.
  6. Memory during a 10-minute call on each engine (the Vivobook has 4 GB: the call must leave room for the desktop).
- Results go into docs/testing.md; a site that fails on Lite is added to the Chromium list.

## Part 8: developers, custom builds, and the official recovery (added on 2026-11-11)
Asked for: a Rudolf OS account with keys of its own, for people who make games for the Library or publish their own
repository with their own build of Rudolf OS; a way back to the official, certified Rudolf OS at any time; Recovery
mode always the official Rudolf OS, even when the normal start runs a build from another repository; and a README
that explains how to make your own repository, keys and builds.

Today (1.5.8): Settings › Updates › Update address takes any https:// address, but rudolf-update installs only
releases signed by a key in /etc/rudolf/rpk-keys, which is in the system image (the official release key). A mirror of
the official releases works; a build signed by anyone else is refused. The Library is the same (rpk, /persist/pkg/sources).

### 8.1 Three keys, not one
- **The computer's key** (device key): made in the computer's TPM 2.0 when it has one, never leaving it. It protects
  "Encrypt my data" (sealed to the TPM, so the disk opens only on this computer with its own start-up) and says "this
  computer". Without a TPM: a key protected by the account's password (as Encrypt my data is now). Lost with the
  hardware, by design.
- **The publisher key** (per person): Ed25519, signs a person's games or builds. Not tied to one computer: kept on a
  hardware security key (FIDO2/PIV: YubiKey and similar) or as a passphrase-protected file with an offline backup, as
  the Rudolf OS keys are. Losing a computer must not lose the right to update one's own games.
- **The account:** links a person to their publisher key. Rudolf OS has no server of its own; the account is the
  person's GitHub account plus their publisher key registered in the Library (8.2). Settings › Accounts › Developer:
  makes or imports the publisher key, shows its public half and fingerprint, exports a backup, and (with a hardware
  key) signs with it without the private key ever being on the disk.
- **Done (2026-11-13), the publisher key:** User Settings › Developer makes one (WebCrypto Ed25519), keeps its private
  half only in a backup in Documents (PBKDF2-SHA-256 600 000 rounds, AES-GCM, a passphrase of 12 characters or more),
  the public half and fingerprint for the account; "Use a backup" brings it back; the private half is shown (60 s, after
  the passphrase) only to paste into a GitHub secret; its raw form is rpk-tool's. Not yet: hardware keys (FIDO2/PIV)
  and the computer's own key (TPM), which need the hardware to test.
- The three are separate so that a leaked signing key does not open anyone's files, and a broken laptop does not lock
  a developer out of their games.

### 8.2 Publishing to the Library: a chain of trust
- **Developer certificates:** the Library's key (rudolfos-library's `RPK_LIBRARY_KEY`) signs a short certificate per
  approved developer: name, GitHub account, public key, what they may publish (package names under a prefix of their
  own), dates. Published as `dev/<name>.cert` in the Library.
- **Packages** carry their developer's signature and certificate; rpk accepts a package whose signature leads to the
  Library key through a valid certificate, and whose name the certificate allows.
- **Revocation:** a signed `revoked.txt` in the Library (keys and packages); rpk checks it with the index, and a
  revoked package is disabled on every computer at the next check (the App Store says why).
- **Done (2026-11-13):** rpk-tool `cert` (the Library signs `rpk-cert 1`: name, github, key, prefix, expires) and
  `build --cert` (the package carries it: `RPKSIG02` trailer); rpk accepts it like a Library package (desktop files
  only), under the prefix, never replacing someone else's, with a certificate from a trusted key that has not expired.
  Revocations are "R" lines in the signed package list (from the Library's `revoked.txt`) rather than a separate
  `revoked.txt` download: refused to install, disabled at the next refresh (`db/NAME.disabled`, in status, not mounted).
  Not yet: the App Store showing the developer and a disabled app's reason; rudolfos-library's workflow publishing
  `dev/<name>.cert`.
- **Review:** a developer's first package, and any that asks for new permissions, is reviewed before it is listed
  (a pull request to rudolfos-library, as today). Until certificates exist, that pull request is the way to publish:
  the Library's own workflow signs.
- **The sandbox first (a requirement before any outside developer's app):** Library web apps are served today from
  the desktop's own address (127.0.0.1:8080/pkg/...), so a package has the desktop's whole access to rudolfd. That is
  fine for the project's own Quake and Emu, not for anyone else's. Outside apps run on an origin of their own
  (rudolfd serves /pkg/<name>/ on a separate port per package, or a sandboxed frame without `allow-same-origin`), with no
  rudolfd token, and talk to the desktop only through a small message API granting what their PKGINFO asks and the
  person allowed: saves (their own folder only), gamepads, full screen, notifications, network (on by default),
  nothing else. The games plan's `rudolf-gamekit` uses only this API.
  **Done (2026-11-13):** the web-app window's frame has no origin (rudolfd also sends /pkg/ pages with a sandbox
  policy, so one opened some other way is sandboxed too). The API is the game kit's messages (games58.js, rudolfGame:
  hello, load, save; the Library's lib/gamekit); the project's own Library apps get them with full screen and
  controllers as before; an outside developer's app gets what PKGINFO's `permissions` lists (saves, gamepad,
  fullscreen) and the person allowed, asked once per account. (A first version added a second API and game kit; they
  were removed the same day.) The network is not limited.

### 8.3 Developer mode: a custom build on one's own computer
- Settings › Updates › Developer mode (administrators, with a strong warning, the password, and a typed confirmation):
  adds one extra trusted key (the developer's publisher key) and an update address. Kept in
  /persist/etc/rudolf/extra-keys, never in the system image; the official key stays.
- rudolf-update then accepts releases signed by either key, and records which key signed the running system.
- **Official or Custom:** Settings › About, System health and the self-test show **Official Rudolf OS** when every
  running file (kernel, layers) is listed in a manifest signed by the official key, and **Custom build (signed by NAME,
  fingerprint ...)** otherwise. The Start menu's version shows a small "custom" mark. Blitzen's diagnostics and the
  devlink report it, so a problem report says which build it came from.
- **Secure Boot:** a custom build's start-up files (its UKI) are signed by the developer's own Secure Boot key. Rudolf
  OS shows how to enrol it in the firmware (MOK-style enrolment where the firmware allows it, or the firmware's own
  key menu), or to turn Secure Boot off; it never weakens Secure Boot by itself. The official Secure Boot key stays
  enrolled (8.4 depends on it).
- **Done (2026-11-13):** rudolf-update `developer on KEY URL | off` (the key in /persist/etc/rudolf/extra-keys, never
  the official one; the official keys alone count for "official" and for the recovery system), status's `signer`,
  `signerKey`, `developer`, `recoveryOfficial`; rudolfd `sys/developer`; Settings › Update and recovery › Developer mode
  (warning, typed "I TRUST THIS DEVELOPER", the password every time) and Official / Custom in About. Not yet: the
  "custom" mark in the Start menu, the self-test, Blitzen and the devlink reporting it; a custom build's own Secure
  Boot key (a custom kernel started by kexec with Secure Boot on needs it enrolled).
- Turning Developer mode off is 8.5.

### 8.4 Recovery mode is always the official Rudolf OS
Whatever the normal start runs, the boot menu's "Recovery mode" starts the official Rudolf OS:
- **An official recovery slot:** (1.5.8 already keeps `system/r` on every stick Make a stick makes, as the stick's
  rescue system, and the start-up falls back to it; this part makes it the official recovery everywhere.) Besides the
  two update slots (a and b), RUDOLFDATA keeps `system/r`: the newest
  official release that started successfully on this computer (kernel and layers), with its manifest and signature.
  rudolf-update fills and refreshes it only from releases signed by the official key, after they have started once
  (boot-ok), and never from a release signed by an extra key. On a computer that never ran the official build (a
  developer's own disc), it is downloaded from the official update address the first time Developer mode is on.
- **An official recovery start-up file:** the ESP keeps `/EFI/rudolf/recovery.efi`, the official UKI (signed with the
  official Secure Boot key), and the "Recovery mode" boot entry starts that file, not the normal one
  (`/EFI/rudolf/rudolf.efi`, which a custom build replaces). Its initramfs carries the official public key and starts
  only `system/r`, after checking its manifest's signature and every file's size and SHA-256; it ignores
  /persist/etc/rudolf/extra-keys and the update address entirely.
- **Done (2026-11-13), step 1:** rudolf-update keeps `system/r` (with an `official` mark) from the running slot at
  boot-ok when its release list is signed by the official key and is newer (hard links; a layer from this drive's disc
  is taken from there), never in recovery mode; Recovery mode starts it first. Next: recovery.efi on the ESP, the
  recovery initramfs's own checks, the repair at each start, and the QEMU Secure Boot test.
- **What it can do:** everything Recovery mode does now (safe settings, the devlink and remote access, Make a stick),
  plus "Return to official Rudolf OS" (8.5) in its first screen.
- **Who may change it:** only rudolf-update, with an official release. A custom build's updater is told by the official
  rules (and its own copy of them) to leave `system/r` and `recovery.efi` alone; the official build checks both at each
  start and repairs them from the official release if they were changed.
- **The honest limit:** an administrator, or a custom build running as root, can always overwrite their own disk,
  including these files. Secure Boot then refuses a recovery file not signed by an enrolled key, so the entry either
  starts the real official recovery or does not start at all; it cannot start a fake one unless the developer's own key
  signed it. The last way back is the same as on 2026-11-10: a rescue stick made from an official release (Make a
  stick, or the official boot image from GitHub, 1.5.7), which starts the official recovery from USB.

### 8.5 Return to official Rudolf OS
- In Settings › Updates (Developer mode › Turn off) and in Recovery mode's first screen:
  1. removes the extra key and the developer's update address, and sets the official address;
  2. starts from `system/r` (or downloads the newest official release, checked with the official key) into the spare
     slot, and makes it the one that starts; the slot the custom build was in is cleared;
  3. restores the official `rudolf.efi` from that release;
  4. asks: keep accounts, files and apps (a return), or erase everything (a factory reset, as Install here does,
     without a disc);
  5. after the next start, Settings › About says Official Rudolf OS again, and the self-test checks it.
- **Done (2026-11-13):** rudolf-update `return-official keep|erase` (Developer mode off; slot r, or the newest official
  release downloaded, into the spare slot, tried at the next start; once it started, boot-ok clears every slot that is
  not official; erase: the factory reset at that start, the system slots kept); rudolfd `sys/return` (a job); Settings ›
  Update and recovery's button and Recovery mode's notice. Not yet: restoring the official rudolf.efi (8.4's ESP work),
  and Library apps from outside developers checked against their certificates (8.2's certificates).
- If the official start fails, the start-up falls back to the previous slot as every update does, and Recovery mode is
  still there.
- Library apps from outside developers stay installed only if their certificates are still valid; apps from an extra
  source added in Developer mode are listed and offered for removal.

### 8.6 The README: making your own Rudolf OS
A new `docs/custom-builds.md`, linked from the README, for someone who forks Rudolf OS:
1. **Fork and build:** forking merciless146/RudolfOS, the release workflow, what each secret is for, building a disc
   and betas in their own GitHub Actions, and their own GitHub Pages update address (`beta/`, `stable/`).
2. **Keys:** making their release key (rpk-tool's Ed25519 key pair) and Secure Boot key (`openssl req` for the db
   certificate), what goes in the repository (public halves only, `trusted-keys.txt`), what goes in GitHub secrets
   (`RPK_SIGNING_KEY`, `RUDOLF_SB_KEY`), and keeping offline backups. Never committing a private key (`keys/`,
   `*.pem`, `*.key` are in .gitignore). How to replace a key that leaked.
3. **Their own Library:** forking rudolfos-library, its key (`RPK_LIBRARY_KEY`), publishing packages, and adding the
   Library to a computer (`/persist/pkg/sources`, with its key in Developer mode).
4. **Installing a custom build:** on a computer of their own (their disc, Secure Boot enrolment or off), or on an
   official Rudolf OS through Developer mode (8.3), and what changes (Custom in About; the official recovery stays).
5. **Going back:** Return to official Rudolf OS (8.5), Recovery mode, and a rescue stick.
6. **What the name means:** a custom build must not call itself the official Rudolf OS; how to name it, and that the
   official recovery and update addresses are the project's.
7. **Publishing games for the official Library:** the developer certificate (8.2), the sandbox's API, the review, and
   the games plan's rules (docs/plan-games.md).
- **Done (2026-11-13):** docs/custom-builds.md, linked from the README; workflow *Custom builds page* runs its `sh ci`
  blocks (iso/qt/host/custombuild_t.sh).
- Each command in it is run in CI against a throw-away fork layout (keys made, a package signed and verified, a
  manifest signed and checked by rudolf-update in its test mode), so the README cannot go stale.

### 8.7 Tests
- Host: rudolf-update with an extra key (accepts its releases only in Developer mode; never writes `system/r` from
  them); the recovery initramfs refuses a `system/r` whose manifest is signed by anything but the official key or
  whose files differ; Return to official from a custom slot (keep and erase); rpk with developer certificates, a
  package outside its prefix refused, a revoked key refused.
- Browser: Developer mode's warnings and confirmation; Official/Custom in About; the outside app sandbox (no rudolfd
  token, only the granted API).
- QEMU (with Secure Boot, OVMF and the official keys enrolled): an official install; Developer mode with a test key
  and a custom release; Recovery mode starts the official `system/r`; a tampered `system/r` and a tampered
  `recovery.efi` (refused by the initramfs and by Secure Boot); Return to official, keeping files and erasing them.

## Part 9: game mode frees memory for the game (added on 2026-11-11)
Found on the Vivobook (1.5.8 beta 2026111108, Standard desktop): Friday Night Funkin' on Newgrounds holds about
1.7 GB. While a song played, 400 MB of the 3.4 GB stayed free, the swap gave memory back in the first seconds of the
song, and the game stuttered, with the processor at about 21% overall (3.5-3.9 GHz, 67-71 °C) and the graphics chip
mostly idle: memory is what is short, not speed. Before 1.5.8 the memory guard ended the game outright (PR #203).

### 9.1 When it applies
- Game mode (game51.js, 1.5.1) already starts when a page is full screen or has the pointer captured; it also starts
  for a Steam game's window or a Flatpak game in front (Games category), and by hand from the taskbar (🎮).
- Settings › Games › Game mode › "Free memory for the game" (on by default), each part below with its own switch.
- It ends when the game leaves full screen or loses the front; what was paused comes back then, or when used.

### 9.2 What it does
1. **Oracle's other tabs sleep:** tabs not in front, not playing sound, not in a call (Part 7) and not loading a form
   the person typed into are unloaded (the shell and rudolf-webkit close their page and keep the address, title,
   icon and scroll position); selecting one loads it again. On Standard and on Lite. The desktop says how many slept.
2. **The desktop gets smaller:** animated wallpapers, the desktop's widgets and hidden windows' timers pause; the
   desktop's own caches (thumbnails, the App Store's catalog) are dropped.
3. **Background work waits:** update and Library downloads, the Linux apps' and Debian's background updates, the
   file index, Blitzen's model (if loaded, about 1-2 GB: unloaded, loaded again when Blitzen is opened).
4. **Apps in the tray:** apps with no window open (Discord, chat apps) are offered to be quit in one notification
   ("Free 600 MB for the game? Quit Discord, …"), never quit without asking; the memory guard still steps in when
   memory really runs out (its rules, PR #203).
5. **The game comes first:** while game mode is on, the memory guard ends background tabs and pages before the game's,
   the game's processes get a lower oom_score_adj than other pages', and the kernel is asked to move other programs'
   memory to the swap first (memory.reclaim on the desktop's and background apps' cgroups at the start, while the game
   loads), so the game's own loading does not wait on the swap.
6. **Measured, not assumed:** each step logs how much it freed (memguard.log, "game mode: 3 tabs slept, 410 MB").

### 9.2a Compressed memory as large as the memory (added on 2026-11-12)
Found on the Vivobook (final 1.5.8 beta 2026111218): FNF reached 1.9 GB when a song ended and the game went back to
song selection; with 143 MB available and the compressed swap (zram) all but full (287 of 1,707 MB free), the memory
guard ended the game. zram compressed about 4 to 1 there, so the swap filled long before the memory was used up. The
Vivobook's Settings › System health › Memory is on "small" (half the memory, 1.7 GB); the default since 1.5.7,
"auto", is as large as the memory (3.4 GB here, at most 8 GB).
- **Measure first, on the Vivobook:** zram on auto, the same FNF test (a whole week, back to song selection each
  time): the guard's events, the swap used and the stalls (PSI), and frame times, against "small".
- **Then, if auto keeps the game alive without long stalls:** when the guard ends a page or an app and zram is on
  "small", its notification offers to switch to auto (applies at the next restart); game mode (9.1) says the same once.
  If auto stalls more than it helps, the guard's thresholds are tuned with it instead (its "memory is low" point and
  the stall it waits for).
- Tests: host, the size zram is given for auto and small on 2, 3.4, 4 and 16 GB (the memory script); browser, the
  offer in the notification and the switch.

- **Measured on 2026-11-12 (beta 2026111222, zram on auto, 3.4 GB):** FNF was still ended, at 1,929 MiB: 130 MiB
  available with 2.4 GB of zram free and real stalls (8%). The game took memory faster than the kernel moved idle
  memory into zram. Chosen: swappiness 180 (was 100), so it moves sooner; the warning (9.2b) no longer waits for the
  swap to fill. Measured again with FNF on the next beta.

- **The file cache cleared when game mode starts** (2026-11-12, asked for): rudolfd memory/dropcache (clean cached
  files only, drop_caches 1, once every 10 s at most), called once per game by game mode and by Task Manager ›
  Performance › Clear cache. Linux gives the cache back by itself, but more slowly than a game can ask.

### 9.2b The memory guard warns before it ends the game (added on 2026-11-12)
On 2026-11-12 the guard ended FNF (about 2 GB) twice without a word before it: the screen left full screen and said
so afterwards (1.5.8). When memory is low but not yet critical and the largest page is the one in front (above all
full screen), the guard first asks the desktop to warn: a small notice over the full-screen page ("Memory is almost
full: close other tabs or apps to keep the game running", with the largest of them and a button for each), for at
least 10 seconds or until memory is critical, whichever comes first. A background page is still ended first, as
now; the warning is only for the page in front. Tests: host, the guard's new "low, warned" step and its timings;
browser, the notice over a full-screen page and its buttons.

### 9.3 Tests
- Browser: game mode on (full screen) puts background tabs to sleep and back on selection, on both engines (the
  shell's and rudolf-webkit's tab messages); tabs with sound, a call or a typed-in form stay; the tray notification's
  choices; everything restored when game mode ends.
- Host: the memory guard's victim order with game mode on (background pages before the game); memory.reclaim called
  on the right cgroups.
- On the Vivobook (devlink): FNF on Newgrounds with three other tabs and Discord open: free memory before and
  after game mode, swap activity at a song's start, and stutter (frame times from a small page script in the
  devlink's test tab, compared with 1.5.8). The goal: over 1 GB free during a song, no swap-in at its start.

## Part 10: a lighter desktop page (added on 2026-11-12)

Measured on the Vivobook with beta 2026111214 (devlink), Standard engine, nothing open: about 1.0 to 1.2 GB in use on
the System monitor (all memory minus what Linux calls available), of which the desktop's own processes about 500 to
550 MB, the kernel about 130 MB, about 200 to 250 MB that belongs to no process (steady at idle, after a video and
after a game: not a graphics pool that fills with use, so capping TTM's page_pool_size would gain little here), and
shared memory and file cache. Lite (1.5.7's check): the page's own process 389 MB of about 520 MB. Most of the
page's share is the page itself: 4.8 MB of code in 95 modules, all loaded and compiled at sign-in, with every dynamic
background (light and dark, about 1.5 MB as text), fonts and the CHANGELOG embedded. Slimming it helps both engines.

What the 200 to 250 MB is was read on 2026-11-12 with one start without lockdown (docs/plan-memdebug.md): 40 to 65 MB is
graphics (the driver's page pool, 21 to 46 MB and growing with use, and the system memory the graphics card uses);
140 to 180 MB is taken by the kernel or a driver outside every counter, to be named by a test kernel with allocation
profiling. From it:
- **The graphics pool capped** (`ttm` page_pool_size 16384 pages = 64 MB, in /etc/modules/ttm and by the memory script; done in 1.5.9): its limit is
  now 1.7 GB, so it can creep up over a long session. Measured: the frame rate of a video and of FNF before and after.
- What the test kernel names, each with its own fix.

- **Task Manager says where the memory goes** (asked for on 2026-11-12, done): rudolfd's memory/breakdown and
  Performance › "Where the memory goes": programs and web pages, the file cache, the kernel, graphics, the SSD's host
  memory buffer (noted by the memory script from the start-up messages), compressed memory, and "Other drivers and
  kernel" for what cannot be read with lockdown on (the graphics pool, network buffers, early start-up), saying so.

### 10.1 What moves out of the start-up page
- **Pictures and fonts first:** the dynamic backgrounds, their thumbnails and the sky masks (DYN), and the extra fonts,
  become files beside the page; only the background in use is loaded and decoded, the thumbnails when Display
  Properties opens, a font when chosen.
- **Self-contained apps:** WordPro, the spreadsheet and presentations, the games, Emu, BlockLua, Blitzen's packs, the
  Library apps' pages, Help and What's new: each its own file, loaded when first opened.
- **What stays:** the core (windows, taskbar, Settings' frame, Oracle and its tabs, the shell's messages) and every
  module that wraps a shared function (applySet, dispPanelS, themeApply, lxRun and the like: aurora58, statue58,
  accent56, lock58 among them), since the order they wrap in matters.

### 10.2 Without causing issues
- **A stub per app:** the name stays defined from the start; the first call loads the file, then runs the call (a
  .docx opened from File Explorer, a game from the Library, a Help link). A file that cannot load says so, with Try
  again, never a silent failure.
- **Versioned file names** (wordpro-1.5.9-ab12.js): a desktop open across an update never mixes versions; the old
  files stay until the next start.
- **Everywhere the page runs:** rudolfd serves the files for both engines (127.0.0.1:8080, and rudolf-webkit's own
  loading); the browser build on GitHub Pages ships them beside index.html, and the phone web app's offline cache
  (7.5) stores them.
- **The tools learn it:** build19.py writes the files and their list; deadcheck reads them all; the self-test checks
  every listed file is present; each app's browser tests load it through its stub.
- **Chromium cannot unload code:** the saving is in apps not opened in a session; the memory their documents and
  pictures use is still given back when they close (checked: no references kept after a window closes).

### 10.3 Then, measured
- The engines' own limits: Lite's JavaScript heap and, for the desktop page only, the JIT off as an option measured
  for speed; Standard's renderer limit and the desktop page's heap limit (site isolation stays on for websites).
- The ~200 MB that belongs to no process: what it is (kernel, graphics driver; debugfs is locked down with Secure Boot,
  so through the driver's sysfs counters and /proc), and whether any of it can be given back.
- WPE WebKit as a later Lite engine (no GTK layer), measured against WebKitGTK, once the page is slim.

### 10.4 Targets and tests
- Targets at the sign-in screen, nothing open: Lite about 300 to 350 MB for the desktop's processes (0.55 to 0.65 GB
  on the System monitor), Standard about 0.8 to 0.9 GB on the System monitor; start-up no slower.
- Tests: browser tests that each deferred app opens through its stub on both engines, that an update between loads
  keeps versions apart, and that a missing file is reported; on the Vivobook, memory at idle and after opening and
  closing each large app, Standard and Lite, before promotion.

## Part 11: fixes and safeguards (added on 2026-11-12)
### 11.5 The Wi-Fi's radios, and Blitzen's Wi-Fi diagnosis (asked for on 2026-11-13, done)
On 2026111302 the Vivobook left the router's 5 GHz radio for its 2.4 GHz one, which never answers it, and was off for
45 s. Now: a radio that did not answer is left out for a day (wifi-event, the kernel's own message counted);
Settings › Network › Wi-Fi › "Radios of this network" lists each radio (band, channel, signal, state) with a Use
switch (wifi.sh radios / radio; turned off by hand it stays off); "Why does the Wi-Fi drop?" (and a Wi-Fi problem
asked in Blitzen's chat) gives Blitzen the Wi-Fi's log, the kernel's Wi-Fi messages and the radios (ai/facts), with
a Wi-Fi pack that says how to read them and which settings exist. To check on the Vivobook: Blitzen's real answer.
- **This computer's Wi-Fi radios** (asked for on 2026-11-13): a drop-down menu between "Radios of this network" and
  "Why does the Wi-Fi drop?", listing the computer's own radios (its adapter's bands, 2.4 / 5 / 6 GHz) with a check by
  each one that is on and a blank space by one that is off. Only an administrator changes it, typing their password
  every time. Kept by the adapter's MAC (/persist/etc/wifi-bands-off), applied as wpa_supplicant's freq_list at every
  start and join (wifi.sh bands / band); nothing automatic writes it, a router's radio on a band that is off cannot be
  turned on in "Radios of this network", and Blitzen never recommends against it. One radio always stays on.

### 11.1 Chromium from one known source, never changed without being chosen
On 2026-11-12 the snap store could not be reached for four builds in a row, and each time the build fell back to
Playwright's Chromium without being asked. Newer Playwright brings Google's **Chrome for Testing** (158.0.8127.18),
which is what 1.5.8 shipped: a Google Chrome build made for automated tests, not for everyday browsing, under
Google's own terms rather than Chromium's open-source licence. Rudolf OS ships open-source Chromium.
- **One source: Ubuntu's chromium snap** (open-source Chromium, built for Ubuntu like the base layer). Debian's
  package is no alternative: it is built for Debian's newer C library and does not run on the Ubuntu base. The
  download is made sturdier: the store's two addresses (api.snapcraft.io and its CDN), longer retries.
- **No silent fallback:** when the snap cannot be fetched, the release keeps the previous release's chromium.sfs (as
  it already does when the start test fails) and says so in its notes and as a warning. Playwright stays only for
  local test builds and by explicit choice (CHROMIUM_FROM=playwright).
- **Checked before it is used:** `chrome --version` must say "Chromium" (not "Google Chrome"), and the version must
  not be lower than the release's: Chromium does not open a profile written by a newer version, so 1.5.8's Chrome
  for Testing 158 stays until the snap has 158 or newer. The release notes say when the Chromium was kept and why.
- Tests: host, chromium_fetch.sh against recorded store answers (a timeout, Chrome branding, a lower version).

### 11.2 Browser security updates
Chromium has security fixes every few weeks, and they reach Rudolf OS only with a release. A scheduled workflow, once
a day, compares the snap's stable Chromium with the stable release's and, when it is newer, opens (or updates) one
GitHub issue: "Chromium X is available (security fixes): the stable release has Y". A beta with it is then one
dispatch; promotion stays the person's choice.

### 11.3 A clear message when a system layer does not match its release
When a layer differs from the signed release list, rudolf-verity reports the device-mapper's error ("table: Argument
list too long") instead of saying what is wrong. It compares the layer's size with the list's blocks before loading
the table and says "modules.sfs does not match the release list (size)"; Task Manager and the start-up message show
that. Test: host, a layer of another size.

### 11.4 New features keep their logic in the system services
So that the desktop that draws them can change (the native desktop decision after 1.6.0), what 1.5.9 adds (the
modem service, Share this connection, calls, game mode, developer mode) keeps its logic and state in rudolfd and the
services, with the desktop only showing state and sending choices through rudolfd's API. Each part's API is written
down in docs/api.md with its first use.

## Part 12: the native desktop engine, a prototype (added on 2026-11-13)
Asked for: the native desktop moved into 1.5.9, as a prototype third desktop engine (chosen on 2026-11-13 over
"measurements only" and "the whole desktop rewritten"). The decision on a native Rudolf OS 2.0 still follows its
numbers (docs/plan-1.6.0.md, "the native desktop decision", now pointing here).

### 12.1 What it is
- **Settings › System health › Desktop engine:** Standard (Chromium), Lite (WebKit) and **Native (test)**, with Lite's
  safeguards: a trial mark, given up after a start that does not reach the sign-in screen (back to Standard, said in
  System health), never in Recovery mode.
- **rudolf-native** (C and GTK 3, which the system layer already has for Lite's WebKitGTK: no new libraries) draws:
  - the wallpaper: a drawn background (its light or dark picture), a picture of your own, or the colours;
  - the taskbar: the start menu, the pinned apps, the open windows, the clock, and the tray (network, sound, battery,
    notifications' count) from rudolfd's API;
  - every window's frame, as the window manager: the title bar (Part's title bar styles and colours: twstyle, twacm),
    minimise, maximise, close, moving and resizing, focus;
  - the sign-in and lock screens, with rudolfd checking the password (the accounts' hashes never reach rudolf-native).
- **The apps stay web pages:** an app opens in a WebKit window of its own showing the desktop page in one-app mode
  (`index.html#app=notepad`: the page starts that app alone, filling its window, without taskbar, icons or wallpaper).
  All app windows share one WebKit web process, so the page's code is loaded once, not once per window. Linux apps and
  Oracle's tabs (Chromium) are framed windows like any other.

### 12.2 Decisions (2026-11-13)
- **X11, not Wayland, for the prototype:** everything around the desktop is X11 today (rudolf-display's input and its
  idle count, remote access, the devlink's screenshot, Linux apps, game mode, Xwayland not in the system layer). A
  Wayland compositor would change all of them at once and the measurement would no longer compare engines alone.
  Wayland becomes part of the 2.0 decision.
- **State stays in rudolfd (11.4):** what the native shell needs that only the web page knows today (the desktop's
  settings, the pinned apps, the accounts) is read through rudolfd's API, from the copy of the page's storage it
  already keeps (lsync, /home/rudolf/.rudolf-ls.json); rudolf-native never parses the page's storage itself.
- **Not in the prototype:** desktop icons' editing, widgets, Phone screen, Blitzen's panel, drag and drop between
  windows, multiple workspaces. They come with 2.0 if it goes ahead.

### 12.3 Steps
1. The page's one-app mode (`#app=NAME`): one app, filling the window; closing it closes the page; on both engines.
2. rudolfd: GET desktop/settings (background, title bars, taskbar position, pins, clock, accent) and desktop/apps
   (the start menu's list, written by the page) from the lsync copy; POST login (checks a password, opens the
   account's encrypted folder as the page's sign-in does).
3. rudolf-native: wallpaper, taskbar, start menu, clock, tray.
4. rudolf-native: the window manager (frames, focus, minimise, maximise, close, move, resize) and the taskbar's list.
5. The session: Native (test) in Settings, its trial and give-up as Lite's.
6. Measured on the Vivobook against Standard and Lite: memory at idle, with Oracle open, with FNF; time from power on
   to the sign-in screen; battery drain at idle. Results in docs/testing.md; then the 2.0 decision.

### 12.3a Step 6's measurements (2026-11-13, on the Vivobook, beta 2026111405, on battery)
| | Standard | Lite | Native (test) |
|---|---|---|---|
| Memory used at idle (MemTotal − MemAvailable) | 1,110 MB | 952 MB | **854 MB** |
| The engine's processes (PSS) | 676 MB (Chromium) | 388 MB (page 244, window 80, network 64) | **194 MB** (rudolf-native 125, network 69) |
| The desktop's memory group | 885 MB | 455 MB | 437 MB |
| Power at idle | 5.6 W | 5.5 W | 5.4 W |
| Start to the desktop ("desktop up") | 31 s | 22 s | 22 s |
| One app open | 1,167 MB (Oracle) | not read yet | 965 MB (Settings: its page 195 MB) |

Native at idle runs no WebKit page at all (the shell is drawn by rudolf-native). With Settings open Native read 7.0 W
once, just after Settings opened (its animated previews): to be read again. Still to read: Lite with an app open, FNF
on each engine. **The decision (2026-11-13, the owner): Rudolf OS goes native**: the shell reaches the web desktop's
features and becomes the default in 1.6.0 (docs/plan-1.6.0.md Part 12), then the apps are rewritten natively one by
one for Rudolf OS 2.0 (docs/plan-2.0.md).

### 12.3b Step 7: Native (test)'s bugs seen on the Vivobook (2026-11-13), fixed in 1.5.9
1. At the first start the wallpaper is the colours (teal): the copy of a picture of your own
   (/home/rudolf/.rudolf-wallpaper/ID.jpg) is written only once a page has loaded the picture. The shell should ask
   rudolfd, which makes the copy itself from the account's picture when it is missing (or the sign-in screen draws the
   drawn background meanwhile, then the picture as soon as it appears, without a restart).
2. The root window shows X's bare "✕" pointer: set the root's cursor (left_ptr) when the window manager starts.
3. The taskbar is pale and small and does not follow the taskbar's style or the buttons' style (tbstyle, tbbstyle):
   draw it at the page's height and in its styles, compared with the page's taskbar in screenshots.
4. The development link's banner shows inside every app window (Settings): in one-app mode the banner is not drawn;
   the native shell shows it once, on its own.
5. Drop-downs (`<select>`) in app windows are blank yellow boxes (Display Properties' Appearance, the Northern lights
   colour, Glow): WebKitGTK's own pop-up menu for a select is not drawn under the native window manager (an
   override-redirect or a transient pop-up window the manager mishandles); fix in the manager, with a host test that
   opens a select under Xvfb.
6. Settings' search box is hidden under that banner (goes with 4).

**Done (2026-11-14), one pull request:** what each was, as found on the host (native59_t.sh tests 47 to 55,
deskapi59_t.sh 33 to 36, oneapp59_t.js 15b, 17 and 18):
1. rudolfd makes the copy itself: `desktop/settings` and `desktop/wallpaper` take the picture from the page's database
   mirror (idbm, the record `wp:PICTURE`) when no page has given it, or gave another picture (ID.img says which);
   before sign-in the last account's picture when the last settings show it (`wpacct`). The shell draws it as soon as
   rudolfd has it, without a restart.
2. The root window's cursor is set (the wallpaper's window inherited X's default).
3. On the host the styles were followed (screenshots side by side with the page's taskbar in Chromium); what differed
   was the size and the pictures: the shell ignored Display Properties' display scaling (`scale`, the page's zoom: a
   laptop at 125 % or 150 % got a small bar) and drew the apps' emoji, pale or missing without a colour emoji font,
   where the page draws its icons. Now the taskbar, title bars and start menu follow `scale`, and the page gives the
   shell its icons as PNGs (desktop/apps `icons`), drawn in outlined buttons as the page's; the version is on the
   start button. If the Vivobook still shows the styles ignored, a screenshot of both is needed.
4. and 6. The page draws no banner in one-app mode; with the native shell it does not ask about commands either. The
   shell draws the banner (24 px at the top, windows kept below it, End now) and asks (Refuse first).
5. Not the window manager: WebKitGTK's pop-up list works under it (tested with a plain page). The page only gave its
   own drop-down style (html.webkit, styles25.css, 1.5.7) to Lite: under Native it now does too.

### 12.3c Step 8: Oracle on Native runs on Chromium, as on Standard (added on 2026-11-14)
Asked for: Oracle on the native desktop runs on Chromium, so that it works exactly as on Standard (docs/plan-1.6.0.md
Part 12, "The rule"). Found on 2026-11-14: on Native, Oracle opens as any app does, its page in a WebKit one-app window,
and nothing there gives it real tabs (no `rudolfShell`: only rudolf-native's `rudolfNative` message handler), so its
websites fall back to frames, or to a separate Chromium window for sign-in sites. Many sites refuse frames; sign-ins
are not kept as on Standard; private tabs are not private (Part 14); games run in a frame of the WebKit page.
12.1's "Oracle's tabs (Chromium) are framed windows like any other" assumed a step that was never written: this one.

**Why Chromium and not something else (decided 2026-11-14):** Blink (drawing pages) and V8 (JavaScript) are parts of
Chromium and cannot be used alone; whatever embeds them (CEF, Qt WebEngine, Electron) ships a Chromium of its own, a
second copy (about 100 MB more) with a different build. WebKitGTK tabs, as Lite has, would be lighter but are Safari's
engine: sites and games behave differently. Rudolf's own Chromium with its tab shell is the same program as
Standard's: the same Blink and V8, the same partitions (an account's sign-ins are the same on both engines), private
tabs in memory, nothing more to download.

**How:**
1. **Oracle's window on Native is Chromium with the shell** (iso/shell, the Isolated Web App Standard starts), not a
   WebKit one-app window: when Oracle is opened (the start menu, a pin, a desktop icon, a link from another app, a
   file of a web type, a page's "open in Oracle"), rudolf-native starts
   `/opt/chromium/chrome --class=RudolfOracle --user-data-dir=PROFILE --app=isolated-app://SHELLID/#app=browser&url=…`
   with Standard's flags (the session's: GPU flags, features turned off, autoplay, the fake media UI for calls, the
   password store), as the account's Linux user. The shell passes its own `#…` on to the desktop frame
   (`DESK_URL + location.hash`), so the page inside starts Oracle alone in one-app mode (oneapp59.js), now with the
   shell's real tabs (rtOn() true): Standard's code path, unchanged.
2. **One Chromium for every Oracle window:** a second Oracle window, or a website's new window, is a message to the
   running one (the shell's `newwin` and oneapp59.js's "another app asked for" become windows of the same Chromium),
   not a second Chromium. The windows are framed by rudolf-native's window manager like the others (the page's title
   bars, the taskbar's button with Oracle's icon and title; `RudolfOracle` grouped as Oracle, not as a Linux app).
3. **Started only with Oracle, gone with it:** when the last Oracle window closes, Chromium exits (the shell closes
   itself when its page says "This app is closed"); idle Native stays at its measured 854 MB. Its start is shown (the
   pointer busy, the window's frame at once with the page's loading look) so the 1 to 3 s of a cold Chromium start do
   not look like nothing happened. Kept warm: no (memory is the point of Native).
4. **The profile is Standard's** (/home/rudolf/.rudolf-chromium or as the session names it): the shell's installed
   copy, its partitions (persist:web-ACCOUNT), Oracle's downloads, permissions and sign-ins. The engines never run at
   the same time, so the profile's lock is never contended; the session's shell checks (rudolf-shellfix, the bundle
   compared) run before Native starts too, so Oracle never meets an old shell.
5. **What the page in Chromium needs from the native desktop:** notifications, the file dialogs, Open with, the
   clipboard, downloads (__dlHook: into the account's Downloads, the native taskbar's progress), the volume and the
   🎙️ indicator during calls, game mode (Chromium's window full screen: the native taskbar and the effects stand
   still), the keys (keys56.js's rules, Ctrl+Shift+N for a private tab) and the devlink's screenshot. Each checked
   on both engines.
6. **Safeguards:** if Chromium is missing or will not start (the desktop layer without it, recovery), Oracle opens
   as now (the WebKit page with frames) and says once that websites will work better on Standard; Native is not given
   up for it.

**Tests:** a host test under Xvfb (`iso/qt/native/oracle_native_t.sh`): rudolf-native with Chromium and the shell;
Oracle opened → one Chromium (`--class=RudolfOracle`), framed, its page in one-app mode with rtOn() true, a local test
site in a real tab (a Controlled Frame), cookies kept in persist:web-ACCOUNT and seen again by Standard's run of the
same profile, a private tab's part in memory (Part 14), a second Oracle window in the same Chromium, the last window
closed → no Chromium left; same_t.sh's comparison of Oracle's window (toolbar, tabs, a site) against Standard's.
Measured on the Vivobook: Native with Oracle open (target: no more than Standard with Oracle open, 1,167 MB), Native
idle unchanged after Oracle closes, FNF in Oracle on both engines (frame rate the same).

**Status:** planned (2026-11-14), not started. It is built after the native session's item in progress, in its own
PR; it touches iso/shell/shell.js (so, as with Part 14, a beta with it replaces the shell in the profile once).

### 12.4 Tests
- Host, under Xvfb: rudolf-native draws its wallpaper and taskbar (screenshots compared), a window gets a frame,
  minimise, maximise and close work, a pinned app opens its one-app WebKit window.
- Browser: each app's one-app mode opens and closes cleanly, on both engines.
- The session's engine choice and give-up (host test, as desksafe58_t.sh for Lite); CI builds rudolf-native.

### 12.5 How it is built (the implementation design, 2026-11-14)
Five pull requests, one per step of 12.3, each stacked on the one before (the owner merges them in order).

**Files**
- `desktop/oneapp59.js`: the page's one-app mode (step 1).
- `iso/sys/rudolfd-desktop.inc` (included by rudolfd.c, as rudolfd-home.inc): the `desktop/*` endpoints, a small JSON
  reader for the lsync copy, and the sign-in's cryptography (SHA-256, SHA-512, HMAC, PBKDF2, HKDF, AES-256-GCM in plain
  C, checked against Node's WebCrypto: rudolfd keeps linking only zlib) (step 2). `docs/api.md` is started with them (11.4).
- `iso/sys/rudolf-native.c`: the native shell, one GTK 3 process: wallpaper, taskbar, start menu, clock, tray, sign-in
  and lock screens (step 3), the window manager and the app windows (step 4). Built like rudolf-webkit
  (`gcc … $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11`), installed by addwebkit.sh beside it.
- The session (`iso/sys/root/usr/lib/rudolf/session`), rudolf-display (it stops managing windows while rudolf-native
  does), rudolfd's `desktop/engine`, engine57.js (Settings) and the release build scripts (step 5).
- Tests: `iso/qt/shell2/oneapp59_t.js` and `iso/qt/webkit/oneapp59_ci.py` (step 1, Chromium and WebKit),
  `iso/qt/host/deskapi59_t.sh` (step 2), `iso/qt/host/native59_t.sh` (steps 3 and 4, under Xvfb),
  `iso/qt/host/engine59_t.sh` (step 5).

**Step 1: the page's one-app mode.** `index.html#app=NAME` (NAME an app's key in APPS, such as notepad or settings;
`&file=C:\PATH` opens a file with the app's openFile). The page:
- adds `html.oneapp`: no taskbar, desktop icons, widgets or wallpaper; no boot sound, no session restore (the
  windows of the last session are not reopened in each app window);
- signs in without a sign-in screen when rudolfd has a native session (GET `desktop/session`, step 2: the account's
  id, name and role, and its two keys as hex: `pk`, the password's key (PBKDF2, as `deriveKey`), and `fk`, the file
  key; `userLogin` takes them in place of the password). Without one (the page opened in a browser, or Standard and
  Lite by hand) it shows its usual sign-in screen, and the app alone after it;
- opens the app, its window filling the page without the page's own title bar (the native frame has one); the
  window's title becomes the page's title (`document.title`, which the native frame shows);
- when that window closes (its own close, or the native frame's close button calling `window.__oneAppClose()`, which
  asks the app as its ✕ does, so unsaved work is still asked about), waits for its files to be written (fsFlush, the
  home folder's writes, the lsync copy) and closes the page (`window.close()`, and a `close` message to the native
  shell); in a browser tab that cannot be closed by a script it shows "This app is closed" with Open again;
- asks the native shell to open another app in a window of its own (a `rudolfNative` message `{op:'open',app,file}`),
  when there is one; in a browser the other app opens in the same page as before;
- in one-app windows other than Settings, does not write the accounts (bd95user): several pages share the same
  storage, and only Settings changes accounts.
Both engines: Playwright's Chromium (oneapp59_t.js) and WebKitGTK (oneapp59_ci.py, run in the Lite engine check):
each of a list of apps opens alone, fills the window, has the app's title, and closes cleanly.

**Step 2: rudolfd.** All under `/api/` with the token, as the rest of the API; read from the lsync copy
(/home/rudolf/.rudolf-ls.json: `{"gen":N,"data":{key: string}}`), whose values the page wrote; rudolf-native never
reads it itself.
- `GET desktop/accounts`: the accounts for the sign-in screen: `[{id,n,r,pk}]` (pk: needs a security key), whether
  the guest may sign in (bd95sys's guest) and the last account; never a hash or a key.
- `POST desktop/login` (body `{"n":NAME,"p":PASSWORD}`): finds the account (bd95user), checks the password as the page
  does (`p2$salt$hash`: PBKDF2-SHA-256, 210 000 rounds), derives the password's key with the account's salt (ks),
  unwraps its file key (fk, AES-GCM), and for an account whose files are on the disk opens its encrypted folder with
  the key the page makes (HKDF-SHA-512 of the file key, "rudolf-os home folder", the account's id) and the session's
  binds, as `home/unlock?session=1`. The session is kept root-only in /run/rudolf/native-session (id, name, role, the
  two keys): each rudolfd connection is its own process, so state lives in files. Five wrong passwords: 15 s of
  "wait", as the page. Refused, with a reason the screen shows: an account with a security key, a password hash from
  before 1.4 (sign in once with Standard), no such account. The guest: no password, no keys. A failed sign-in goes in
  the health log as the page's do.
- `POST desktop/logout`: locks the account's folder (home/lock) and removes the session. `POST desktop/lock` and
  `POST desktop/unlock` (body `{"p":…}`, checked against the session's account) for the lock screen.
- `GET desktop/session`: the session for the one-app pages (above); 404 without one.
- `GET desktop/settings`: what the shell draws, for the session's account (bd95set:ID), before sign-in the last
  settings (bd95set): background (bs, b1, b2, dyn, dm, lat, wpimg, wpfit), taskbar (tb, tbstyle, tbac, tbacm, tbbstyle,
  tbbac, tbbacm), title bars (twstyle, twacm), accent, darkui, the clock (ck, h24, df, tz, doff), the pinned apps
  (bd95pins:ID), the notifications' count (bd95notif:ID) and `gen` (the copy's generation: the shell asks every 3 s and redraws when it changed). Raw values as
  the page keeps them; the shell computes the colours (acVariant, acInk and the shades, ported to C).
- `GET desktop/apps`, `POST desktop/apps`: the start menu's list (key, name, icon) and the drawn backgrounds' pictures
  (DYN: each one's light and dark file in www/a/), written by the page after each sign-in (any engine), kept in
  /home/rudolf/.rudolf-apps.json.
- `GET desktop/wallpaper?acct=ID`, `POST desktop/wallpaper?acct=ID`: a picture of your own. It lives in the page's
  IndexedDB, so the page also gives rudolfd a copy (the JPEG it already shrinks to the screen) whenever it loads or
  changes one; kept beside the account's settings in /home/rudolf/.rudolf-wallpaper/ID.jpg.
- `docs/api.md` describes these (11.4).

**Step 3: rudolf-native's wallpaper, taskbar, start menu, clock and tray.** One GTK 3 process, run as the desktop user,
started by the session (step 5): `rudolf-native --size=WxH [--draw=gpu|mixed|soft] [--base=http://127.0.0.1:8080]`.
It reads the token as the page gets it (rudolfd's page, `GET /`: nothing new is exposed), then talks to rudolfd over
HTTP on 127.0.0.1 (a small blocking client; the sign-in in a worker thread, its reply back to GTK's loop with g_idle_add).
JSON is read by a small reader of its own (found in step 3: a JavaScriptCore context made in the shell's own process
stopped WebKit's views from drawing).
- **The wallpaper:** a full-screen window (type hint DESKTOP, kept at the bottom): the picture of your own (fill, fit,
  stretch, centre, tile), else the drawn background's light or dark picture (dm: light, dark, or automatic by the hour
  of sunrise and sunset at the latitude, as the page's dynResolve; WebP through WebKit's own libwebp, opened with dlopen,
  so a layer without it falls back to the colours), else the colours (bs: solid or the gradients, b1 and b2).
- **The taskbar:** a window (type hint DOCK, on top, 40 px, at the bottom or the top as tb says; left and right are
  drawn at the bottom in the prototype) drawn with cairo in the five styles (Standard, Wireframe, Monochrome, Candy,
  Retro) and the accent colour (tbac, tbacm) for the bar, and the buttons' own style and colour (tbbstyle, tbbac,
  tbbacm), as accent56.js and tbbtn59.js draw them: the start button, the pinned apps, the open windows (step 4), the
  tray and the clock. Icons are the apps' emoji, drawn by Pango.
- **The start menu:** a pop-up with a search box and the apps of desktop/apps (pinned first), and Sign out, Lock,
  Restart and Shut down (rudolfd's power). Choosing an app opens its window (step 4); in step 3 a pinned app or a menu
  item opens it as a plain WebKit window.
- **The clock:** the time (12- or 24-hour, h24), the date (df), in the settings' time zone (tz) with the clock's offset
  (doff); updated each minute. Clicking it opens the Clock app.
- **The tray:** network (net), sound (volume), battery (power/status), the notifications' count (how many the page keeps in
  bd95notif:ID, given by desktop/settings); each opens its Settings page in one-app mode (`#app=settings`); polled every 10 s.
- **The sign-in screen:** the wallpaper, the accounts (desktop/accounts), name and password boxes, the guest button,
  the message rudolfd gives; POST desktop/login; and the lock screen (Lock in the start menu, and after the screen
  saver's time: not in the prototype) asks the password again (desktop/unlock). Once the sign-in screen is drawn the
  shell sends `heartbeat` every 10 s, as the page does (the session's trial and watchdog need it).

**Step 4: the window manager.** rudolf-native becomes the X window manager (SubstructureRedirect on the root, through a
GDK event filter, so one GTK process does both).
- Every top-level window that is not override-redirect (Linux apps, Chromium's window for websites and Oracle's real
  tabs, Wine) is put in a frame: an X window of ours with the title bar drawn by the same cairo code as the app
  windows' (the title bar styles: twstyle, twacm, the active window coloured, the others grey), minimise, maximise and
  close (WM_DELETE_WINDOW, else XKillClient). Moving (dragging the title bar), resizing (the 6 px edges and corners),
  double-click to maximise, focus on click (WM_TAKE_FOCUS and XSetInputFocus, as rudolf-display does), raising,
  WM_STATE, _NET_CLIENT_LIST and _NET_ACTIVE_WINDOW. Dialogs (WM_TRANSIENT_FOR) are centred over their window. A window
  asking for full screen (_NET_WM_STATE_FULLSCREEN, games) covers the whole screen, taskbar too, without its frame.
- **App windows:** a pinned app or a menu item opens a GTK window of our own with a WebKitWebView showing
  `http://127.0.0.1:8080/#app=NAME`, framed like the rest. The first app window starts the web process; every other one is
  opened by an open app window (the page's own `window.open`, kept as webNativeOpen), which WebKit keeps in that process,
  so they share one WebKit web process (found in step 3: a view merely made `related-view` to another, or to a hidden
  one, got a process of its own or drew nothing, WebKitGTK 2.52), and one web context (the page's storage, ~/.rudolf-native, as
  ~/.rudolf-webkit is Lite's). All app windows have one user content manager (WebKit gives a window opened by another its opener's anyway), so
  each page says which window it is (`#app=NAME&w=N`, in each `rudolfNative` message). The frame's title follows the
  page's title; ✕ calls `window.__oneAppClose()` (the app may
  ask to save), a second ✕ after 5 s closes it anyway; the page's `close` closes the window. A `rudolfNative` message
  `open` opens another app's window. A web process that ends is reported in each window with Reload.
- **The taskbar's list:** the open windows (ours and the others), the active one highlighted; a click focuses, or
  minimises the active one; a pinned app with a window open focuses it rather than opening another.
- rudolf-display keeps feeding input and copying the screen (software display) but stops managing windows while
  /run/rudolf/wm-native exists (the session makes it before starting rudolf-native and removes it after: rudolf-native
  runs as the desktop user, who cannot write /run/rudolf).

**Step 5: the session.** Settings › System health › Desktop engine gets **Native (test)**; rudolfd's desktop/engine
takes `engine=native` (when /usr/bin/rudolf-native is there), kept in /persist/etc/rudolf-desktop-engine as Lite's.
The session's `native_usable` (the choice is native, not Recovery mode, not given up this start, the binary is there)
comes before Lite's check; ENGINE=native starts `rudolf-native` as the desktop user in the desktop's memory group.
Its safeguards are Lite's, shared: the trial mark (.trying) made when it starts and removed at its first heartbeat; a
start that did not reach the sign-in screen gives it up at the next start; no heartbeat in 90 s, or closing three
times in a row as it starts, gives it up now. Given up means Standard (Chromium) from then on, said in System health
("the Native desktop did not start (…), so the standard engine is used from now on"), as Lite. The watchdog's other
rules stay. The release build (ci_release.sh, mkrootfs.sh, arm64_base.sh) builds rudolf-native and the webkit-check
workflow builds it and runs native59_t.sh.

**Not in the prototype** (12.2, and these): the taskbar at the left or right, Super+Tab and the other window keys,
the screen saver and its lock, notifications as pop-ups (the count only), several monitors, the Phone screen and
Blitzen's panel, workspaces, desktop icons. Each one-app page runs the whole page's code (its timers too); what that
costs is part of step 6's measurements.

**Test plan**
- Step 1: oneapp59_t.js (Chromium): for Notepad, Calculator, Settings, File Explorer, Paint and Task Manager: opened
  with `#app=`, signed in from a stand-in desktop/session, the window fills the page, no taskbar or icons, the title is
  the app's, `__oneAppClose()` closes it after its files are written (the stand-in sees the close), Notepad's
  unsaved-changes question still comes; without a session the sign-in screen comes first, then the app alone; another
  app asked for goes to the native shell's message. oneapp59_ci.py: the same in WebKitGTK.
- Step 2: deskapi59_t.sh: rudolfd built with the test hooks, an lsync copy made by the real page (Node's WebCrypto for
  the hashes and keys): accounts without hashes, a wrong password refused, the right one accepted and the home folder
  unlocked with the page's key (the key checked against hdKeyHex's), five failures wait, the session's keys equal the
  page's, settings and pins of the signed-in account, logout locks; the crypto against test vectors.
- Steps 3 and 4: native59_t.sh under Xvfb with a stand-in rudolfd: the wallpaper's colours and picture (pixels sampled
  from screenshots), the taskbar at the bottom and the top, a style and the accent colour on it, the clock's text, a
  sign-in with a wrong and a right password; a stand-in X client gets a frame, focus, is moved, maximised, minimised
  and restored from the taskbar and closed with WM_DELETE_WINDOW; a full-screen client covers the taskbar; a pinned
  app opens its one-app WebKit window, whose title reaches the frame, and two app windows share one web process.
- Step 5: engine59_t.sh (as desksafe58_t.sh and engine57_t.sh): the session chooses native, never in Recovery mode,
  the trial mark and its give-up, the 90 s and three-closings give-ups back to Standard with the health message;
  rudolfd's desktop/engine takes native; Settings offers Native (test).

## Part 13: USB printers (moved to 1.6.0 on 2026-11-13)
To be tested with the HP desktop's user, who has the USB printer: docs/plan-1.6.0.md Part 8. Nothing of it is in 1.5.9.

## Steps
0. Part 11 first: 11.1 (the Chromium source) and 11.3, then 11.2; 11.4 holds for every part.
1. Part 1 (ModemManager, read only); its device checks, with the USB stick and the MiFi, moved to 1.6.0.
2. Part 2 (tablet mode, keyboard, rotation).
3. Part 3 (ARM64 build and its QEMU checks).
4. Part 4 (Share this connection); its device check moved to 1.6.0 (the MiFi as the connection).
5. Part 5 (Waydroid in Debian), measured first.
6. Part 6 (the phone prepared).
7. Part 7 (calls): 7.1 WebKit's WebRTC plug-ins measured and added, the CI call test on both engines, 7.2 the
   in-a-call indicator, device switching and holding off sleep, 7.3 the calls page and tel: links; then the Vivobook
   checks (7.4) before 1.5.9 is promoted.
8. Part 8 (developers): 8.2's sandbox for outside apps first (before any outside package); 8.4 the official recovery
   slot and recovery.efi, and 8.5 Return to official, proven in QEMU with Secure Boot; then 8.3 Developer mode, 8.1 the
   keys in Settings, 8.2's certificates and revocation, and 8.6 the README with its CI check.
9. Part 9 (game mode memory): 9.2's sleeping tabs first (both engines), then the desktop's own savings, waiting
   background work, the tray offer, the memory guard's order and its warning (9.2b); checked with FNF on the Vivobook before promotion.
10. Part 10 (a lighter page): pictures and fonts out first, then the large apps one at a time behind stubs, each
    measured on both engines on the Vivobook; then the engines' limits (10.3).
11. Part 12 (the native desktop engine, a prototype): the one-app mode and rudolfd's settings first, then rudolf-native's
    wallpaper and taskbar, its window manager, the engine choice; measured on the Vivobook last.
    Then step 7 (the Vivobook's bugs, done: #311) and step 8 (Oracle on Chromium on Native, 12.3c), before promotion.
12. Part 13 (USB printers): moved to 1.6.0 (its Part 8).
13. Part 15 (security against a directed attack): 15.1 (only the desktop gets the page and its token) first, then
    15.2, 15.4, 15.5, 15.6, and 15.3 (a Linux user per account, from 1.6.0); before promotion.

## Part 14: private browsing in Oracle (added on 2026-11-14)
Asked for: private browsing in Oracle, with a private tab opened by right-clicking Oracle's icon. Done (desktop/private59.js):
New private tab in Oracle's ☰ menu, Ctrl+Shift+N (also inside a website's page: the shell and rudolf-webkit pass it on as
`cpn`), and in the right-click menus of Oracle's taskbar icon, its Start menu entry, its desktop icon and an open Oracle
window's taskbar button. A private window: dark violet, 🕶️ Private, "(Private)" in its title; no History; its tabs use a
part named `priv-ACCOUNT-SESSION`, which the Chromium shell gives a partition without `persist:` and rudolf-webkit an
ephemeral context (in memory only); cleared when the last private window closes, a new name for the next private
session; pop-ups from it stay private. Kept: bookmarks and downloads (said on its start page and in Help › Privacy).
Not private: websites shown in frames when Oracle's tabs are not running (the start page says so), and on Native until
its taskbar menus come (docs/plan-1.6.0.md Part 12, item 3) and Oracle runs on Chromium there (Part 12, step 8). Test: qt/shell2/private59_t.js. The shell changed, so the
first start of a beta with it replaces the shell in the profile (websites ask to sign in again, once).

## Part 15: security against a directed attack (added on 2026-11-14)
Asked for after a review of how Rudolf OS stands against malware aimed at its owner (2026-11-14). Below the desktop it is
strong: Secure Boot with Rudolf's own keys and a signed kernel image, read-only signed system layers, signed updates
and Library packages, the a/b slots and recovery, Chromium's sandbox for websites, sandboxed Library web apps and
downloads, the firewall, encrypted home folders. Above it, the review found five gaps; a directed attack would most
likely get a program run (a game, an .exe, a Linux app) and go through gap 1. In this order:

### 15.1 Only the desktop gets the page and its token (first: the most serious gap)
Found: rudolfd serves the desktop page, with its per-boot API token and the page's stored data (rudolf-ls), to any
program that connects to 127.0.0.1:8080 (serve_index has no check). A Linux app, a Wine program or a Flatpak with
network access, all running as uid 1000 like the desktop, can read the token and then drive the API, which runs as
root (disks, updates, factory reset, accounts).
- **Who is connecting:** for each connection to 8080 (and every other loopback listener of rudolfd: 8099 and the rest,
  listed in 15.5), rudolfd finds the socket's process: the peer's port in /proc/net/tcp (and tcp6) gives the socket's
  inode and uid; the inode is looked for only among the processes of the desktop's cgroup (cgroup.procs: Chromium's,
  rudolf-webkit's or rudolf-native's processes, and the Chromium of Oracle on Native, Part 12 step 8). Found there:
  the desktop. Else: not the desktop. Done once per connection (keep-alive keeps the answer).
- **What a program that is not the desktop gets:** not the page (403, "This page belongs to Rudolf OS's desktop"),
  no /api/ (403 even with the right token), no rudolf-ls. What stays open to everyone, as now: /lib/ and /pkg/
  (shared libraries with CORS for Oracle's sandboxed pages), /dlf/ (downloads, served sandboxed), the Library's web
  apps (sandboxed, no origin).
- **The other programs must not be in the desktop's cgroup:** every program rudolfd or the page starts (Linux apps,
  Flatpak, Wine, Debian, emulators, the Chromium window for websites on Lite) goes into an apps cgroup of its own,
  never inheriting the desktop's; checked by a test that starts each kind and reads its cgroup.
- **Once each account is its own Linux user (15.3):** the desktop's uid (rudolf, 1000) is required as well, from the
  same /proc/net/tcp line: a second check that costs nothing.
- **Tests:** host tests of rudolfd: the page and the API from a process in the desktop's cgroup (allowed), from one
  outside it with the right token (refused), the token never in a refused answer; QEMU: from a Flatpak app, a Wine
  program and a Debian terminal, `curl 127.0.0.1:8080/` and an API call with a token read some other way, both refused;
  the desktop on each engine unaffected (Standard, Lite, Native, Oracle on Native).

### 15.2 A file opened in an app cannot reach the rest of the desktop
Found: the built-in apps all run in the desktop's one page (one origin), so a scripting bug in how one app shows a file
(a document, an SVG, an HTML or Markdown file, an e-book) would reach every app's data and the token.
- **Every place a file's contents become HTML** (innerHTML, srcdoc, document.write of file-derived text, inline SVG,
  data: and blob: documents) is listed by a check (`iso/systools/htmlsinks.py`, run before each PR like deadcheck)
  and either escapes the text or shows it in a frame with `sandbox` and no `allow-same-origin` (no origin: no storage,
  no token, no parent), its scripts off unless the app needs them (the PDF viewer's PDF.js, already a sandboxed page).
  WordLite's and WordPro's .docx, the e-book reader, Markdown and HTML previews, SVG in the picture viewer (as <img>,
  never inline), File Explorer's previews, Notepad's HTML view, the Help pages from packages.
- **The page's own Content-Security-Policy**, sent by rudolfd: connect-src limited to rudolfd and the services the
  desktop names (Open-Meteo, the Library, the update server, the time servers' HTTPS fallback), object-src none,
  base-uri none, frame-ancestors the shell only; so injected code could not send anything out even if it ran. Inline
  scripts stay allowed for now (the page is built that way); moving them to hashes is 1.6.0's.
- **Tests:** a set of hostile files (scripts in every place each format allows them) opened in each app: nothing runs
  in the desktop's origin (a canary in the page is never read); the CSP blocks a fetch to an outside address.

### 15.3 Each account is its own Linux user (moved here from 1.6.0 Part 10)
1.6.0 Part 10, as written there, done in 1.5.9: each account's Linux apps, Wine and Debian run as the account's own
user (uid 1001 and up), so one account's programs cannot read another's files, and none is the desktop's user (15.1's
second check). 1.6.0 Part 11 (elogind and polkit) stays in 1.6.0.

### 15.4 The devlink is off unless it is being used
Found: the devlink runs commands sent through a GitHub repository; on the Vivobook it was on with "Ask first" off, so
anyone who got into that GitHub account could run commands on the computer.
- "Ask first" on by default, and on again after each restart (turning it off lasts until the next start).
- The devlink turns itself off after 4 hours with no command (said in a notification), and at every restart unless
  "Keep on after restarting" is ticked (off by default).
- Only an administrator can turn it on, with the administrator's password, and only in Developer mode (Part 8.3).
- System health lists it as on, with since when and its last command.

### 15.5 Fewer and harder doors
- **The list of everything that listens** (TCP and Unix sockets: rudolfd's 8080 and 8099, remote access, the portal,
  Bluetooth's agent, the devlink), each with who may use it and how it checks; a test fails if something listens that
  the list does not name.
- **rudolfd built hardened:** -fstack-protector-strong, -D_FORTIFY_SOURCE=3, PIE, full RELRO, -fcf-protection; the
  release check verifies the flags on the binary. Its HTTP and JSON parsing fuzzed on the host (libFuzzer or AFL with
  AddressSanitizer, a test corpus kept in the repository) before each release.
- **rudolfd's root work split out over time:** the API stays as it is, but what needs root (disks, updates, accounts,
  power) moves into small helpers it runs, so rudolfd itself can drop to the rudolf user after start (1.6.0 continues).

### 15.6 Security fixes arrive soon
- A scheduled check (GitHub Actions, daily) compares the Chromium and the kernel in the latest release with the newest
  ones Ubuntu has published; when either has a security update, it opens an issue "Security update available:
  Chromium 1xx.x" for the owner, who decides whether to build a beta. It never builds or promotes by itself.
- Settings › About shows the Chromium and kernel versions and the release's date.

**Status:** planned (2026-11-14). 15.1 first, then 15.2, 15.4, 15.5, 15.6; 15.3 when 15.1 is done (it changes the same
code). Each in its own PR.
