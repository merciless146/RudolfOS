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
- **The MiFi 7730L** with USB tethering on: which USB network driver it needs (plan-1.5.8.md 6.1).
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
- **The graphics pool capped** (`ttm.page_pool_size`, 16384 pages = 64 MB, on the kernel's command line): its limit is
  now 1.7 GB, so it can creep up over a long session. Measured: the frame rate of a video and of FNF before and after.
- What the test kernel names, each with its own fix.

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
9. Part 9 (game mode memory): 9.2's sleeping tabs first (both engines), then the desktop's own savings, waiting
   background work, the tray offer and the memory guard's order; checked with FNF on the Vivobook before promotion.
10. Part 10 (a lighter page): pictures and fonts out first, then the large apps one at a time behind stubs, each
    measured on both engines on the Vivobook; then the engines' limits (10.3).
