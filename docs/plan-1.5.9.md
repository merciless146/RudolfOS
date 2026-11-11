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
