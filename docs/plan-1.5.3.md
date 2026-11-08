# Rudolf OS 1.5.3: devices without a driver, and USB Wi-Fi

Released as stable 1.5.3.261108 (serial 2026110803) on 2026-11-08, with a disc image (disc-1.5.3). The other parts
first planned here (Bluetooth from Intel, MediaTek and Qualcomm, sound on newer laptops, Steam and Proton,
Thunderbolt and USB4, built-in Intel cameras, and the Library's follow-ups) moved to **docs/plan-1.5.4.md**.

## What 1.5.3 has

- **Devices without a driver** (section 0, #100): Settings › Devices › Not supported yet, the system report, and
  one `hardware` line in the health log on a computer's first start; USB speeds and the port's speed in Settings ›
  Devices. On the Vivobook it found the AMD audio processor (`1022:15e2`).
- **USB Wi-Fi with RT2800 chips** (section 7, #99, #101): the Vivobook's RT5370 dongle works; Settings › Network
  has an Adapter choice; every Wi-Fi driver kconfig.sh asks for is checked after olddefconfig; the CI build fetches
  exactly the firmware firmware-extra.txt pins.
- **Found while installing on the HP desktop** (2026-11-07 and 08):
  - Make a USB stick checks the stick can start before calling it ready (#102);
  - Install here copies the running system when the drive it runs from has boot files only (#103);
  - Shut down, Restart and Sleep refuse while a stick, an install, boot files or an update is being written, with
    "anyway" to go on (#104). A restart in the middle of Make a USB stick had twice left a stick without its system.

Tested: the RT5370 and the Adapter choice on the Vivobook; the devices list there; a stick made on the Vivobook
starting the HP, and Install here on the HP; QEMU `t150` 18/18 and `t150f` 5/5 on the release candidate.

## 0. Devices without a driver (the base for the rest)

Today, a part with no driver shows nothing. Nobody can tell whether it is missing or broken.
- rudolfd `GET devices/unsupported`: each PCI, USB and I2C/ACPI device that no driver claimed, with its ID and
  what kind it is (Bluetooth, sound, camera, Thunderbolt, Wi-Fi, other).
- Settings › Devices: **Not supported yet**, the list in plain words ("Intel Bluetooth (8087:0033): no driver yet").
- The system report (Report a problem) includes the list. Then a report from any laptop says what that laptop is
  missing.
- Health log: one `hardware` line at the first start-up on a computer, with what it found.

## 7. USB Wi-Fi adapters: Ralink and MediaTek RT2800 (found on the Vivobook, 2026-11-07)

**What was found** (Development link, commands 126-129, read-only):
- The adapter is a **Ralink RT5370** USB Wi-Fi dongle (`148f:5370`, "802.11 n WLAN"). It is a USB 2.0 device, so it
  runs at 480 Mbit/s (high speed) in any port. A SuperSpeed port cannot make it faster, and Rudolf OS reports that
  speed correctly.
- **Rudolf OS does not recognize it.** No driver claimed it, no network interface appeared, and Settings › Network
  shows only the built-in Wi-Fi (`wlan0`, rtw88_8821ce).
- **Cause 1: the driver is silently left out of the kernel.** `iso/kconfig.sh` asks for `RT2800USB` as a module, but
  `RT2800USB` depends on the `RT2X00` menu, which is not switched on. `make olddefconfig` drops it without a word.
  The RT5370's ID is also behind `RT2800USB_RT53XX`, which is not set either. Of the Wi-Fi drivers the file asks
  for, `rt2800usb` is the only one missing on the Vivobook (`mt7922e` is not a real driver: the MT7922 is handled by
  `mt7921e`).
- **Cause 2: its firmware is not included.** `rt2800usb` loads `rt2870.bin` for every chip it drives, and
  `iso/firmware-extra.txt` does not have it.
- **The port:** the Vivobook Go E1404FA's firmware describes the port the dongle is in (`usb3-port3`) as having **no
  SuperSpeed partner**, unlike `usb3-port1` and `usb3-port2`, which pair with `usb4-port1` and `usb4-port2` (10
  Gbit/s). So according to the laptop itself, that port is a USB 2.0 port. For a USB 2.0 dongle it makes no
  difference. Settings should still show it (below), so the question has an answer on screen.
- **Why nothing said so:** kconfig.sh checks the hotkey, Bluetooth and memory options after olddefconfig, but not the
  Wi-Fi drivers. And a device with no driver shows nothing anywhere: section 0 is for exactly this.

**The fix:**
- **Kernel (`iso/kconfig.sh`):**
  - `y RT2X00` (the menu: it only makes the drivers selectable);
  - `m RT2800USB` with every chip family: `RT2800USB_RT33XX`, `RT2800USB_RT35XX`, `RT2800USB_RT3573`,
    `RT2800USB_RT53XX`, `RT2800USB_RT55XX`, `RT2800USB_UNKNOWN`. That covers the RT2770, RT2870, RT3070, RT3071,
    RT3072, RT3370, RT3572, RT3573, RT5370, RT5372 and RT5572, the chips of most inexpensive USB Wi-Fi dongles sold
    since about 2010;
  - `m RT2800PCI` with `RT2800PCI_RT33XX`, `RT2800PCI_RT35XX`, `RT2800PCI_RT53XX` (the same chips as PCIe cards in
    older laptops; same firmware family, `rt3290.bin` for the RT3290);
  - remove `MT7922E` from the list (not a real option);
  - **after olddefconfig, check every Wi-Fi module the file asks for** (as is done for Bluetooth): the build fails
    if any of them was dropped. Then this cannot happen silently again.
- **Firmware (`iso/firmware-extra.txt`):** `rt2870.bin` (8 KB) and `rt3290.bin` (4 KB) from linux-firmware, pinned
  by SHA-256 and checked against the copy the CI build fetches, as with Realtek Bluetooth (#80, #81). The older
  `rt73.bin` and `rt2561*.bin` chips (RT73USB, RT61PCI) are left out: they are 802.11g from before 2008.
- **Settings › Network, more than one Wi-Fi adapter:**
  - today the Wi-Fi panel and start-up use the first adapter found (`wifi.sh` `firstif`), so a dongle next to the
    built-in Wi-Fi would never be used;
  - with two or more, the panel gets an **Adapter** choice ("Built-in: Realtek RTL8821CE" / "USB: Ralink RT5370"),
    passed to rudolfd as `if=` (`api_wifi` already accepts it);
  - the choice is saved (`/persist/etc/wifi-adapter`, by MAC address, since USB names change between ports); at
    start-up `wifi.sh boot` uses the saved adapter when it is plugged in, otherwise the first one;
  - unplugging the adapter in use falls back to the built-in Wi-Fi by itself, with a notification.
- **Settings › Devices, USB:** each USB device with its speed in plain words ("USB 2.0, 480 Mbit/s"), the port's
  own speed ("USB 3 port, 10 Gbit/s", from the port's SuperSpeed partner), and its driver or "no driver yet".
  It shares its code with section 0.

**Tests:**
- Host: the kconfig check fails a configuration where a requested Wi-Fi driver was dropped; `wifi.sh` with two
  stand-in adapters picks the saved one, and falls back when it is gone.
- Browser test: the Adapter choice appears only with two or more adapters, and is passed on.
- On the Vivobook:
  - the RT5370 appears (`rt2800usb`, `wlan1` or similar), scans and joins the home network;
  - choose it in Settings and restart: it is used again; unplug it: the built-in Wi-Fi takes over;
  - the built-in RTL8821CE and the Realtek Bluetooth are unchanged;
  - Settings › Devices shows the dongle as "USB 2.0, 480 Mbit/s" and the port it is in as USB 2.0 (and a SuperSpeed
    port as USB 3, 10 Gbit/s).

**Size:** about 0.2 MB in the modules layer (`rt2x00lib`, `rt2800lib`, `rt2800usb`, `rt2800pci` and two firmware
files).
