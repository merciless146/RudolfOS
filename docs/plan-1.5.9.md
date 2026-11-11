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

## Steps
1. Part 1 (ModemManager, read only), with the USB stick and the MiFi on the Vivobook over the devlink.
2. Part 2 (tablet mode, keyboard, rotation).
3. Part 3 (ARM64 build and its QEMU checks).
4. Part 4 (Share this connection), after the Vivobook's Wi-Fi chip is checked.
5. Part 5 (Waydroid in Debian), measured first.
6. Part 6 (the phone prepared).
