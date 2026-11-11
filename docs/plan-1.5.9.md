# Rudolf OS 1.5.9: the groundwork for cellular (1.6.0) and for mobile devices

Asked for on 2026-11-10, with docs/plan-1.6.0.md (the first cellular release): what 1.6.0 needs, staged between 1.5.8 and
1.5.9. 1.5.8 lays the network groundwork (docs/plan-1.5.8.md Part 6: USB modems' drivers, adapters plugged in later,
metered connections); 1.5.9 adds the modem service, the tablet layout and the ARM64 build; 1.6.0 builds the cellular
experience on them. It starts once 1.5.8 is promoted.

## From 1.5.8 (expected done there)
- Kernel drivers for USB networking from hotspots and phones (RNDIS, CDC-NCM, CDC-MBIM) and for cellular modules
  (QMI WWAN, the option serial driver), as modules.
- A network adapter plugged in after start-up gets an address; Settings › Network lists it.
- Metered connections, and what pauses on them.
- The kernel's Android binder (for Waydroid later), scrcpy, the .apk wizard's information.

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
- **Budget:** ModemManager and its libraries under 10 MB in the base layer (release check); nothing running without a
  modem.
- **Tests:** host tests with a stand-in ModemManager on a private D-Bus (rudolfd's parsing); the USB stick (1.6.0's test
  hardware) on the Vivobook over the devlink, read only.

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

## Part 4: Share this connection (docs/plan-1.5.8.md 6.4)
- Rudolf OS as the hotspot: hostapd and dnsmasq in the system layer (about 1 MB), the firewall's NAT rules, off by
  default, administrators only. First checked on the Vivobook's Wi-Fi chip over the devlink (access point alone, and
  with a Wi-Fi connection at the same time).

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

### 7.3 Phone numbers
- Settings › Apps › "Calls to phones": a short page on what each service needs (Google Voice: free in the US with a
  Google account; Zoom Phone, Teams Phone: paid), with links that open in Oracle. Nothing is installed or signed up for
  by Rudolf OS.
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
- Turning Developer mode off is 8.5.

### 8.4 Recovery mode is always the official Rudolf OS
Whatever the normal start runs, the boot menu's "Recovery mode" starts the official Rudolf OS:
- **An official recovery slot:** besides the two update slots (a and b), RUDOLFDATA keeps `system/r`: the newest
  official release that started successfully on this computer (kernel and layers), with its manifest and signature.
  rudolf-update fills and refreshes it only from releases signed by the official key, after they have started once
  (boot-ok), and never from a release signed by an extra key. On a computer that never ran the official build (a
  developer's own disc), it is downloaded from the official update address the first time Developer mode is on.
- **An official recovery start-up file:** the ESP keeps `/EFI/rudolf/recovery.efi`, the official UKI (signed with the
  official Secure Boot key), and the "Recovery mode" boot entry starts that file, not the normal one
  (`/EFI/rudolf/rudolf.efi`, which a custom build replaces). Its initramfs carries the official public key and starts
  only `system/r`, after checking its manifest's signature and every file's size and SHA-256; it ignores
  /persist/etc/rudolf/extra-keys and the update address entirely.
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

## Steps
1. Part 1 (ModemManager, read only), with the USB stick and the MiFi on the Vivobook over the devlink.
2. Part 2 (tablet mode, keyboard, rotation).
3. Part 3 (ARM64 build and its QEMU checks).
4. Part 4 (Share this connection), after the Vivobook's Wi-Fi chip is checked.
5. Part 5 (Waydroid in Debian), measured first.
6. Part 6 (the phone prepared).
7. Part 7 (calls): 7.1 WebKit's WebRTC plug-ins measured and added, the CI call test on both engines, 7.2 the
   in-a-call indicator, device switching and holding off sleep, 7.3 the calls page and tel: links; then the Vivobook
   checks (7.4) before 1.5.9 is promoted.
8. Part 8 (developers): 8.2's sandbox for outside apps first (before any outside package); 8.4 the official recovery
   slot and recovery.efi, and 8.5 Return to official, proven in QEMU with Secure Boot; then 8.3 Developer mode, 8.1 the
   keys in Settings, 8.2's certificates and revocation, and 8.6 the README with its CI check.
