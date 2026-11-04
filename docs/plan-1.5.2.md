# Rudolf OS 1.5.2: the Library, and more hardware

1.5.1 added Bluetooth for Realtek chips. In practice, though, it was tested on one laptop, the Vivobook: an AMD
processor and graphics with a Realtek Wi-Fi and Bluetooth card. 1.5.2 has two parts:
- **the Rudolf OS Library**, as planned in docs/plan-library.md;
- support for the hardware most other laptops have:
  - Bluetooth from Intel, MediaTek and Qualcomm;
  - sound on laptops from about 2020 on;
  - Thunderbolt and USB4 docks;
  - the built-in cameras of newer Intel laptops;
  - Windows games on Steam through Proton.

The kernel stays at 6.18, which has the drivers all of this needs. Drivers and firmware go only into the modules layer,
like 1.5.1's Bluetooth, so the system layer barely grows. Each firmware file comes from linux-firmware, pinned by its
SHA-256 in iso/firmware-extra.txt (`fwextra.py`). Each hash is checked against the copy the CI build fetches, as was
done for Realtek in 1.5.1 (#80, #81).

## 0. Devices without a driver (the base for the rest)

Today, a part with no driver shows nothing. Nobody can tell whether it is missing or broken.
- rudolfd `GET devices/unsupported`: each PCI, USB and I2C/ACPI device that no driver claimed, with its ID and
  what kind it is (Bluetooth, sound, camera, Thunderbolt, Wi-Fi, other).
- Settings › Devices: **Not supported yet**, the list in plain words ("Intel Bluetooth (8087:0033): no driver yet").
- The system report (Report a problem) includes the list. Then a report from any laptop says what that laptop is
  missing.
- Health log: one `hardware` line at the first start-up on a computer, with what it found.

## 1. Library

All of docs/plan-library.md. In short:
- Quake and Emu move out of the desktop layer into the Library (the `rudolfos-library` repository, signed with its own
  key);
- an App Store tab for the Library;
- "download it?" when Quake or Emu is opened without its package;
- reinstalling them on computers that used them;
- the disc's repository still carries both.

It waits for you: the repository, the Library key pair, the secret `RPK_LIBRARY_KEY`, the public key sent to me, and
Pages turned on.

## 2. Bluetooth: Intel, MediaTek, Qualcomm and built-in UART

The drivers for Intel and MediaTek (`BT_INTEL`, `BT_MTK`) are already in the kernel. Only their firmware is missing.

| Chips | Found in | What is added | Size (about) |
|---|---|---|---|
| Intel AX200, AX201, AX203, AX210, AX211, AX411, BE200, BE201 (the Bluetooth half of Intel Wi-Fi) | Most Intel laptops since 2019, and many AMD ones | firmware `intel/ibt-*.sfi` and `.ddc` for the chips the kernel's btintel asks for | 3–5 MB |
| MediaTek MT7921, MT7922, MT7925 | Many AMD laptops (Lenovo, ASUS, HP) | `mediatek/BT_RAM_CODE_MT79*` | 2 MB |
| Qualcomm QCA6174, QCA9377, WCN6855, WCN7850 (with ath10k and ath11k Wi-Fi) | Dell, Lenovo and HP laptops | `qca/rampatch_*` and `qca/nvm_*` for USB-attached chips; check that `BT_QCA` is built in kconfig.sh | 2–3 MB |
| UART-attached chips: Realtek RTL8723BS and RTL8723CS, Broadcom, Intel | Cheap laptops and tablets | `BT_HCIUART` with H5, BCM, INTEL and QCA; `SERIAL_DEV_BUS` | <1 MB |

- **Choosing the firmware:** `fwextra.py` gets a mode like wififw.py's: take the files the built Bluetooth modules ask
  for (MODULE_FIRMWARE), and the newest older version where an exact one is missing.
- **CHANGELOG:** Bluetooth "on computers with an Intel, MediaTek, Qualcomm or Realtek chip".
- **Tests:**
  - The host test checks the chosen firmware list against the modules.
  - QEMU: with a stand-in Bluetooth controller (`-device usb-bt-dongle` where QEMU has it, or a virtual HCI), the
    adapter appears and Settings works.
  - On the Vivobook (Realtek): nothing changes.
  - **Intel, MediaTek and Qualcomm can only be tested on hardware with those chips.** See "Testing" below.

## 3. Sound on newer laptops

The kernel has only the classic HDA driver. It plays sound on the Vivobook, but many laptops since about 2020 send
their speakers or built-in microphones through a sound processor (DSP). Without its driver they have no sound, or
no microphone.
- **Intel (SOF), Tiger Lake to Lunar Lake:**
  - kernel: `SND_SOC`, `SND_SOC_SOF_TOPLEVEL`, `SND_SOC_SOF_PCI`, `SND_SOC_SOF_INTEL_TOPLEVEL`, and the
    platforms (Tiger Lake, Alder Lake, Raptor Lake, Meteor Lake, Lunar Lake). Also the generic machine driver
    (sof-hda-dsp), `SND_SOC_DMIC`, and SoundWire (`SOUNDWIRE_INTEL` with its machine driver);
  - `SND_INTEL_DSP_CONFIG` chooses HDA or SOF per computer, as other systems do;
  - firmware: `intel/sof-ipc4/`, `intel/sof-ace-tplg/` and `intel/sof-tplg/` for those platforms only (10–15 MB:
    the largest addition; the older Skylake to Ice Lake platforms stay on HDA).
- **AMD (ACP):**
  - kernel: the digital microphones of Renoir, Cezanne, Rembrandt and Phoenix (`SND_SOC_AMD_RENOIR`, `SND_SOC_AMD_ACP6x`
    and `SND_SOC_AMD_PS` with their machine drivers), and SOF for AMD (`SND_SOC_SOF_AMD_TOPLEVEL`) where a
    laptop uses it;
  - the Vivobook may well use one of these for its microphone: it is the first test.
- **Speaker amplifiers** (without them the speakers stay silent or very quiet, though headphones work):
  - Cirrus CS35L41 and CS35L56 (many ASUS, Lenovo and HP laptops): `SND_HDA_SCODEC_CS35L41_I2C/SPI`,
    `SND_HDA_SCODEC_CS35L56`, firmware `cirrus/` for the listed laptops;
  - TI TAS2781 (Lenovo): `SND_HDA_SCODEC_TAS2781_I2C`, firmware `ti/tas2781/`.
- **The sound server:**
  - DSP sound cards need ALSA's UCM profiles: `alsa-ucm-conf` (1 MB) goes into the root file system;
  - PulseAudio 16.1 is checked to load them (module-alsa-card with UCM).
- **Settings › Sound** shows the microphone and its level, and what kind of sound card was found ("Intel sound
  processor (SOF)"), so a report says it.
- **Tests:**
  - in the host test, the chosen firmware is checked against the modules;
  - on the Vivobook: the speakers, the headphone jack, the built-in microphone (before and after), HDMI sound;
  - in QEMU: HDA as today (no change). Intel SOF can only be tested on hardware.

## 4. Thunderbolt and USB4

Basic USB-C works today: USB devices, and charging that the hardware does by itself. Docks, displays through
Thunderbolt or USB4, and devices that need PCIe over the cable do not.
- **Kernel:**
  - `USB4` (the thunderbolt driver), `HOTPLUG_PCI_PCIE`, `PCIEPORTBUS`;
  - `TYPEC`, `TYPEC_UCSI`, `UCSI_ACPI` (USB-C ports: which end powers which, and the modes);
  - `TYPEC_DP_ALTMODE` (displays over USB-C), DisplayPort tunnelling over USB4 in amdgpu and i915/xe;
  - the IOMMU for both makers (`INTEL_IOMMU`, `AMD_IOMMU`), turned on by default, so a device on the cable cannot
    read memory it was not given. Check first that kconfig.sh has them.
- **Network chips in docks:** `ATLANTIC` (Aquantia, in many Thunderbolt docks). Realtek USB and Intel I225 are
  already in.
- **Approving a device** (`/usr/lib/rudolf/thunderbolt`, rudolfd `/api/tb`):
  - When the firmware reports DMA protection (`iommu_dma_protection` = 1), devices are approved by themselves, as
    Windows and macOS do it.
  - Otherwise a new device asks: "Allow CalDigit TS4? Only allow docks and devices you trust." Only an
    administrator can answer, from the same kind of question box as Bluetooth pairing.
  - Approved devices are kept by their unique ID (`/persist/etc/thunderbolt-allowed`) and approved again when they
    are plugged in.
- **Settings › Devices** lists the Thunderbolt and USB4 devices, approved or not, with Forget.
- **Tests:**
  - host test of the script against a stand-in `/sys/bus/thunderbolt`: approval, refusal, devices kept and approved
    again when plugged back in, the automatic approval with DMA protection;
  - on hardware: a dock (display, network, USB), and a USB-C display on the Vivobook if its port supports
    DisplayPort.

## 5. Built-in cameras on newer Intel laptops (preview)

USB cameras (UVC), in nearly every AMD laptop and older Intel ones, already work in Oracle. Intel laptops since
Tiger Lake often have a MIPI camera behind Intel's IPU6, or IPU7 on Lunar Lake. These need a driver for the
processing unit, one for the camera sensor, and an image processor in software.
- **Kernel:**
  - `VIDEO_INTEL_IPU6`, plus IPU7 if 6.18 has it;
  - `INTEL_SKL_INT3472` (the camera's power and LED);
  - the common sensors: OV2740, OV01A10, OV02C10, OV08X40, OV13B10, HI556;
  - firmware `intel/ipu/`.
- **libcamera** with its software image processor (the "simple" pipeline), about 3 MB.
- **Getting the picture to Oracle:** Chromium reads cameras through V4L2, which libcamera cameras are not. Two ways,
  to be chosen after a prototype:
  - (a) **v4l2loopback**, built and signed with the kernel's modules. A small `rudolf-camera` turns the built-in
    camera into an ordinary V4L2 camera while a page uses it, started and stopped by rudolfd when Oracle asks for
    the camera (the permission prompt already exists);
  - (b) libcamera's V4L2 compatibility layer loaded into Oracle's camera process. Smaller, but Chromium's sandbox may
    not allow it.
- **Privacy:**
  - the camera light (INT3472) is on whenever the camera is;
  - Settings › Privacy shows which page is using the camera;
  - nothing runs while no page asks for the camera.
- **Off by default in 1.5.2:** Settings › Devices › Built-in camera › Try the new camera support. It is turned on by
  itself in a later update once it has worked on a few laptops.
- **Not included:** infrared face-recognition cameras (Windows Hello), and fingerprint readers (a later release).
- **Tests:**
  - host test of the start and stop by rudolfd, with a stand-in;
  - the Vivobook's USB camera in a video call (nothing changes);
  - an IPU6 laptop for the rest.

## 6. Steam and Proton

Steam is in the App Store's Games tab since 1.2.5, as a Flatpak from Flathub (unverified, so an administrator allows
it). It has never been tried with Windows games through Proton on Rudolf OS. 1.5.2 makes that a supported, one-click
path:
- **The kernel:**
  - `NTSYNC` (`/dev/ntsync`, used by recent Proton versions; readable by the user);
  - `vm.max_map_count` = 1048576 (some games need it);
  - the open-files limit for the session raised to 524288 (Proton's esync).
- **Controllers in Steam:**
  - `/dev/uinput` and the controllers' `/dev/hidraw*` usable by the signed-in user (mdev.conf). Then Steam Input
    works for Xbox, PlayStation and Switch controllers, by USB and by Bluetooth.
- **32-bit graphics:**
  - installing Steam also installs Flathub's 32-bit graphics runtime (`org.freedesktop.Platform.GL32.default`)
    that matches the Flatpak runtime;
  - Proton needs Vulkan. Steam's card says "Needs a graphics card with Vulkan" and warns on computers that draw with
    the processor or have no Vulkan driver (old Intel graphics before Broadwell, NVIDIA older than the cards NVK
    supports).
- **Game mode:** Steam's Big Picture and full-screen games already hold notifications (1.5.1). Steam is added to the
  apps that count as gaming, so the screen does not lock while a controller is used.
- **Where games are kept:**
  - in Steam's Flatpak folder on the data partition (inside the encrypted folder when "Encrypt my data" is on);
  - the App Store says how much space is free, and Steam's "add a library folder" works for a second drive
    (Flatpak permission for `/media`).
- **Anti-cheat:**
  - games whose anti-cheat supports Proton (Easy Anti-Cheat and BattlEye, when the developer turned it on) work;
  - the others (for example Valorant, Fortnite, Call of Duty) do not, and nothing tries to get around their
    anti-cheat;
  - Steam's card says this in one line, and points to cloud gaming for those.
- **Tests:**
  - On the Vivobook: install Steam, sign in (you), and run with Proton:
    - one free Windows-only game, for example a free-to-play title;
    - one game with Easy Anti-Cheat that supports Proton.
  - Check controller input by Bluetooth and USB, sound, game mode, and the frame rate against the same game on
    Windows if you have it.
  - Browser test: Steam's card and its warnings on a computer without Vulkan.

## Testing hardware other than the Vivobook

The Vivobook can test the Realtek Bluetooth (no change), AMD sound and microphone, the USB camera, USB-C displays (if
its port supports them) and Steam. Intel and MediaTek Bluetooth, Intel SOF sound, Thunderbolt docks and IPU6 cameras
need other laptops. Ways to test them:
1. **A USB Bluetooth adapter** with an Intel or MediaTek chip (inexpensive) tests those drivers on the Vivobook.
2. **Another laptop you have**, with a live session from the disc (nothing is installed). The new list "Not
   supported yet" and the system report then show what works.
3. **Shipping untested support as untested.** The drivers and firmware are the same as in other Linux systems, so
   the risk is low. The CHANGELOG says which parts were tested on hardware.

## Size

| Part | Download with the update (about) |
|---|---|
| Library | the desktop layer about 12 MB **smaller** |
| Bluetooth firmware | +8 MB (modules layer) |
| Sound: SOF, ACP, amplifiers, UCM | +15 MB (modules layer and root) |
| Thunderbolt and USB4 | +1 MB |
| Cameras (IPU6, libcamera) | +6 MB |
| Steam and Proton | nothing (Steam itself is the 1.5 GB Flatpak, downloaded only when installed) |

Computers download only the layers that changed. Most firmware is never loaded on a computer without that
hardware, but it is on the disk.

## Stages (one PR each, in this order)

1. **Devices without a driver** (section 0): it helps test the rest.
2. **Bluetooth** (section 2) and **sound** (section 3), with a beta on the Vivobook.
3. **Steam and Proton** (section 6), with a beta, and the Steam test on the Vivobook.
4. **Thunderbolt and USB4** (section 4).
5. **Cameras** (section 5), off by default.
6. **The Library** (section 1), once the repository and key are ready. It can come earlier, as soon as they are.
7. QEMU tests on the final beta:
   - `t150` (start-up, firewall, fallback);
   - new `t152`: the Library installs over the network and from the disc; a system update signed with the Library
     key is refused.
   Then the Vivobook, then promote.
