# Rudolf OS 1.6.2: what waits on the owner's own hardware

Moved from docs/plan-1.6.1.md on 2026-11-21 (the owner: "Move everything waiting on my own hardware to 1.6.2 and start
the scheduled tasks for everything else left to be done in 1.6.1"). The text of each part is as it stood in the 1.6.1
plan, numbers kept so its references still point the right way. What can be built and tested without the owner's
hardware stays in 1.6.1; what here can be prepared without it (stand-ins, QEMU, host tests) is prepared there or here,
but a part is done only with the owner's check on the hardware it names.
The owner's rule for the native desktop holds for every part: "Rudolf OS on the native desktop should appear and
function exactly as it does in Chromium mode."

## What it needs
- **The Vivobook (with the owner):** Part 14 (Discord and Steam on Native, then Native the default), Part 12 (kernel
  tuning: the owner's checklist), 21b (the checks after each beta), 19b (gaming measured, with the Asus desktop).
- **The MiFi 7730L with an active SIM:** Part 8, then Part 7 (T-Mobile-network prepaid SIM).
- **The first phone (an ARM64 phone; the PinePhone Pro through Tow-Boot) or an ARM64 computer with UEFI:** Part 4,
  Part 5 (eSIM and 5G hardware), Part 6's 1.6.1 items measured on it, 9b (the first ARM64 release).

## Parts
### 4. The first phone, stage 1: the hardware (from 1.6.0 "Not in 1.6.0", and docs/plan-1.5.8.md 8.3)
The phone's hardware made to work, on today's native desktop; its interface is 2.0's (stage 2).
- **One well-supported phone first:** a used PinePhone Pro (discontinued in August 2025, but its hardware is the best
  supported by mainline Linux and its modem, the Quectel EG25-G, has working VoLTE: Part 7), with the OnePlus 6/6T
  after it (its VoLTE does not work yet in postmarketOS). Bought and chosen by the owner.
- **Starting:** the ARM64 release from 1.6.0's cellular and ARM64 plan (stage A in QEMU, stage B's first ARM64
  computer), on the phone through its UEFI-capable boot firmware (Tow-Boot or U-Boot with UEFI on the PinePhone Pro),
  so it starts as Rudolf OS starts on a computer: the boot menu, Secure Boot where the firmware allows, recovery. The
  kernel: Rudolf's own build with the phone's drivers (the device tree and options postmarketOS and mainline Linux use
  for it, written into kconfig-arm64.sh), never a vendor kernel.
- **The modem:** mobile data, texts and picture messages (1.6.0's Messages), **voice calls** (a call each way, hold,
  two calls, DTMF tones for menus), and **emergency calls**, which must be checked on each device and carrier before
  they are offered (Part 7 says how). A minimal call screen in today's desktop (answer, end, mute, speaker, keypad),
  enough to prove calls; the phone's real dialler is stage 2's.
- **The call's sound:** the earpiece, the speaker and a headset, with the modem's audio routed (ALSA UCM for the
  phone's sound card), the microphone, no echo.
- **Power:** sleep with the modem waking the phone for a call or a text (and the alarm clock), the battery's level,
  charging and the charger's limit, the screen's brightness, the power key.
- **The rest of the hardware, at driver level:** the touch screen, Wi-Fi, Bluetooth, the cameras detected by
  libcamera, the sensors (accelerometer, proximity, light) and GPS readable, vibration. Their apps and behaviour
  (auto-rotation, the screen off at the ear, a Camera app) are stage 2's.
- **Today's desktop on a phone screen:** Native on X11, scaled for the screen, the on-screen keyboard (1.6.0's), touch
  mode: usable for testing, not meant to be a phone's interface.
- **Done when:** the phone starts Rudolf OS from its storage, makes and takes a call on a T-Mobile-network SIM, sends
  and receives texts, sleeps and wakes for a call, and its standby battery drain is measured (Part 6), all written in
  docs/testing.md.
- **On 1.6's kernel and X11 (2026-11-20):** stage 1 is built on 1.6.x (the 6.18 LTS, X11, ARM64); the phone's own build
  and interface move to 2.0's "next" channel (the newest kernel, Wayland). A phone with 8 GB for the full desktop and
  Wayland testing: the OnePlus 6T (8 GB), Snapdragon 845 as the OnePlus 6, started through U-Boot's UEFI from its boot
  partition; its VoLTE (needed for calls on T-Mobile) does not work yet, so the PinePhone Pro stays the phone for calls.
  The Fairphone 5 (8 GB) to watch as its mainline support matures. Phones running on Android's drivers (Furi Labs FLX1s+,
  Volla Phone Quintus) do not fit "never a vendor kernel".
- Nothing on a phone is flashed or unlocked without the owner's decision.

### 5. eSIM and 5G (from 1.6.0 "Not in 1.6.0")
- eSIM provisioning (downloading a carrier's eSIM profile);
- 5G standalone features beyond what the modem does by itself.

1.6.0 left these without a release; they are here so they are not lost. They need hardware with an eSIM and a 5G
modem.

### 6. Closing the gap with Android (added on 2026-11-15)
Asked for on 2026-11-15, after comparing 1.6.1 with Android: the items that decide whether someone could carry a
Rudolf phone every day. Each is measured on the first phone (Part 4) and compared with the same phone on Android
where it can be. **Split (owner, 2026-11-15):** 1.6.1 does what the hardware decides (marked *1.6.1* below: the
measurements, calls with the carrier, the modem's sleep); the parts people use every day are 2.0's phone build
(marked *2.0*), since they are built on Wayland and the native apps.
- *2.0, measured in 1.6.1.* **Android apps through Waydroid, made dependable:** set up from Settings, apps from F-Droid and an app store the
  owner chooses, their windows and notifications in Rudolf's own; a list of apps that work and that refuse (banking
  apps that check the device), kept in the docs. Measured on the Vivobook first (docs/plan-1.6.0.md: "Waydroid
  measured on the Vivobook with Lite before it is offered").
- *1.6.1 measured and the modem's sleep; the rest 2.0.* **Battery life:** the phone's idle and screen-on drain measured, then: the modem, Wi-Fi and the screen put to sleep
  when not needed, suspend with the modem waking it for a call or a text, apps in the background held back as
  Android's Doze does; targets set from the measurements, shown in Settings › Battery.
- *1.6.1.* **Calls with the carrier:** voice calls over 4G (VoLTE) where the modem and carrier allow, Wi-Fi calling if the
  carrier offers it, a call to a real phone and an emergency-call check on each device (Part 4), the call's sound
  through the earpiece, speaker and a headset.
- *2.0 (the cameras detected in 1.6.1).* **The camera:** photos and video with the phone's cameras (libcamera), in a Camera app; quality compared with
  Android's on the same phone.
- *2.0 (readable in 1.6.1).* **The phone's sensors and hardware:** auto-rotation, proximity (the screen off at the ear), brightness, GPS for maps
  and weather, the fingerprint reader where the phone has one, vibration.
- *2.0.* **Messaging apps people need:** what works through Waydroid or the web (Signal, WhatsApp Web), and what does not,
  said plainly in the docs.
- **Tests:** each measured and written in docs/testing.md; the battery and call results on the owner's phone and
  carrier.

### 7. T-Mobile and the prepaid services on its network (added on 2026-11-15)
Asked for on 2026-11-15: "Plan for T-Mobile and adjacent prepaid service coverage in 1.6.1." The US has no 3G left,
so calls need VoLTE, and carriers decide which phones get it. T-Mobile's network is the friendliest to Linux phones
(reports in the Pine64 wiki's carrier list and forum, 2020 to 2024); AT&T and Verizon are later.
- **The carriers covered:** T-Mobile (postpaid and prepaid), Mint Mobile, Tello, US Mobile on its T-Mobile network,
  Simple Mobile, Metro by T-Mobile, Ultra Mobile and Google Fi (data and calls; Fi's first activation needs an
  Android phone and its picture messages do not work: said in Settings). The list is checked again before work
  starts, since prepaid brands change networks.
- **The first phone: the PinePhone Pro** (Part 4). Its modem (Quectel EG25-G) has working VoLTE; the OnePlus 6's VoLTE
  does not work yet in postmarketOS (pmaports issue 1878), so it waits.
- **Set up by itself:** Settings › Mobile network recognises the carrier from the SIM (its MCC/MNC and its name) and
  fills in the APN exactly, letters' case included (US Mobile's "PWG" failed in lowercase on a PinePhone Pro; Simple
  Mobile's is "Simple"), with the picture messages' settings (MMSC, proxy). The APNs come from the operators'
  database 1.6.0 already ships, with Rudolf's own corrections for these carriers kept beside it.
- **VoLTE turned on:** the modem's IMS/VoLTE set up for T-Mobile (its carrier profile on the EG25-G), checked after
  every start and after a SIM change; Settings says whether calls go over 4G.
- **Wi-Fi calling** where T-Mobile offers it on the phone (VoWiFi through the modem's IMS), else said plainly.
- **Texts and picture messages:** SMS (1.6.0 Part 6) and MMS through the APN's MMSC, in the Messages app.
- **Activation help:** what each carrier needs (activate online, the phone's IMEI given to the carrier, support asked
  to turn VoLTE on for the line), written in the docs and linked from Settings when the SIM has no service.
- **5G:** not on the PinePhone Pro (an LTE modem). T-Mobile's 5G is tested later with a 5G modem card in a laptop
  (Part 5).
- **Tests:** a prepaid Mint or Tello SIM first (cheap, no contract), then T-Mobile itself: data, a call each way, a
  call to and from a landline, an emergency-call check as Part 4 says, texts and picture messages each way,
  Wi-Fi calling, roaming off, the APN filled in from a fresh SIM, and a call during a weak signal. Results in
  docs/testing.md, with the carrier and date. A host test of the APN table (each carrier's MCC/MNC and name gives the
  right APN, case kept).
- **Later:** AT&T's network (stricter about devices), then Verizon (approved phones only).

### 8. Mobile data and the SIM on real hardware (moved from 1.6.0 on 2026-11-16)
Decided by the owner on 2026-11-16: "For now, move the mobile/SIM testing to 1.6.1." 1.6.0 ships the cellular work
proven without hardware (its stage A: ModemManager, Settings › Mobile network, the connection and failover, data and
the monthly limit, the taskbar's signal, Messages, all with host, browser and QEMU tests against a stand-in modem);
what needs a real modem and SIM is checked here, before 1.6.1's phone work relies on it. From docs/plan-1.6.0.md's
stage B (items 1 to 3) and its release checks:
- **The MiFi 7730L** with an active SIM (the owner's choice; the HP desktop user's USB stick only if it is lent again):
  found in Settings › Network › Cellular modems (maker, model, SIM, operator), no "Invalid rule match" in
  /run/rudolf/modem.log, the self-test's modem line; the SIM's PIN (never the PUK on a SIM someone needs); the APN filled
  in (a prepaid T-Mobile-network SIM, Part 7); connected; Wi-Fi failover and back; IPv4 sites on an IPv6-only network
  (the CLAT); data counted against the operator's own count; the 80 % notice and data off at 100 %; an update offered
  on mobile data with its size; the bars and type against the device's own; the SIM taken out while running; unplugged
  and plugged in again; sleep and wake (after 1.6.0's sleep fix for the Vivobook, CONFIG_AMD_PMC).
- **Messages** to and from a real phone (Unicode, a long one, the delivery report), a sign-in code, the notice with
  the desktop locked, kept across a restart and in the encrypted folder.
- **Share this connection** with the MiFi's USB tethering as the uplink (1.5.9 Part 4, waiting since then for a data
  plan).
- **The FCC-unlock path** on a laptop with a WWAN card (a used ThinkPad with a Fibocom L850-GL or Quectel EM05-G).
- Each checked on Standard and Native (the owner's rule), results in docs/testing.md with the carrier and date.

### 12. Kernel tuning (added on 2026-11-16; proposed on 2026-11-12)
The kernel already has zram (zstd), MGLRU, PSI, schedutil and the AMD and Intel P-state drivers (iso/kconfig.sh).
What is left is measured, not guessed: each change kept only if the numbers say so.
- **Responsiveness:** PREEMPT_DYNAMIC with `preempt=full` against the current default; HZ 1000 against 250; measured
  as the desktop's input lag (the page's lag meter, rudolf-native's frame times) while a build or a game loads the CPU.
- **AMD power:** `amd_pstate=active` with an energy-performance preference (balance_performance on mains,
  balance_power on battery), set by the power settings; measured as battery drain at idle and with a video.
- **Start-up time:** what loads before "the desktop was ready" (built-in drivers that could be modules, the initramfs's
  size, waits in rudolf-init); the health log's timings (1.6.0, #425) are the measure. The Vivobook takes 27 to 37 s.
- **Memory:** vm.swappiness and zram's size with Oracle, a game and a call at once on 3.3 GB.
- **How:** QEMU first for what QEMU can show (start-up, memory), a test kernel built with each choice; the Vivobook last,
  by the owner (battery and real input lag), with a short checklist of numbers to note on each kernel.
- **One kernel for every computer, so a second machine (owner, 2026-11-16):** Rudolf OS ships one x86-64 kernel, so a
  change tuned on the Vivobook must not make another PC slower or stop it starting. Every change is also checked on the
  owner's **Asus desktop** (RuckXTrauma), running Rudolf OS live from a USB stick (never installed there, nothing on its
  disk changed): kept only if it helps on at least one machine and hurts on neither. Its specifications go here from
  its System report: CPU and cores ___, memory ___, graphics ___, storage ___. On it, power is measured as
  responsiveness and temperatures or fan noise (no battery). Start-up is also checked in QEMU on several kinds of
  virtual hardware (q35 and i440fx; AHCI, NVMe, virtio-blk, USB storage; BIOS and UEFI): anything a computer needs to
  reach its root file system stays built in.
- **First, before any tuning is merged: the kernel's required features checked at build time (owner, 2026-11-16).**
  Tuning must never remove a feature programs depend on. iso/kconfig.sh already stops the build when some groups are
  missing (the memory options, sound, games, Thunderbolt); it is widened to every feature Rudolf OS's programs and
  security rely on, each listed with who needs it, so a tuning change that drops one fails the build instead of
  reaching a computer:
  - **Flatpak and bubblewrap:** user, PID, network, mount and IPC namespaces; seccomp and its filters; overlayfs;
    FUSE (the document portal).
  - **The Debian container and Wine:** namespaces, cgroups v2 and its controllers (cpu, memory, io, pids), binfmt_misc,
    32-bit programs (IA32_EMULATION) for Wine and Steam.
  - **The desktop and its sandboxes:** cgroups for the desktop's memory group (rudolfd's peer check), inotify, epoll,
    eventfd, signalfd, timerfd, memfd, the X server's needs (DRM, KMS, DRI3's file passing), uinput, evdev.
  - **Security:** Secure Boot's kernel lockdown, module signature checking, YAMA, the keyring (KEYS), the encrypted
    home's dm-crypt and its ciphers, the firewall (nftables), audit if used.
  - **Storage and start-up:** every disk controller and file system needed to reach the system disk (AHCI, NVMe,
    virtio, USB storage, SquashFS, ext4, vfat for the EFI partition), the initramfs's needs.
  - **Power and devices:** suspend (s2idle), AMD_PMC, the P-state drivers, CPU frequency, thermal, the battery.
  The list is made by reading what each program needs (bubblewrap's and Flatpak's documented requirements, rudolfd's,
  the X server's), and checked against today's kernel so it starts green. Its test: a deliberately broken
  configuration (one option removed from each group) must fail the build with that option's name and who needs it.
  This check is its own PR, merged before any of Part 12's tuning PRs.

#### Results, 2026-11-17 (measured in QEMU with KVM; the Vivobook's part is the owner's checklist below)
How: `.github/workflows/kernel-tuning.yml` (on pushes to `ktune-*` branches; nothing published) builds test kernels from
this tree (`iso/systools/testkernel.sh`, the release's configuration with options changed) and test base layers (the
release's with this tree's sys/root and rudolfd), and starts them in QEMU/KVM (`iso/qt/sb/kt161_t.py`) on the 1.6.0 beta's
layers (r2026111620), as installed (slot a on NVMe, RUDOLFDATA), UEFI, 2 processors, 3.3 GB. **GitHub's runners differ
from each other by up to 20 %**, so every comparison below was made on one machine, the variants started in turn (the
first two rounds, which compared across machines, were thrown away for that). Medians of 4 starts or 3 runs.

**Responsiveness: no change kept.** The kernel already has PREEMPT_DYNAMIC (the voluntary model by default, `preempt=` at
start-up) and HZ 1000 (x86_64_defconfig's), so the plan's "HZ 1000 against the current" is "HZ 250 against the current".
Wake-up latency of an ordinary thread sleeping 1 ms (`kload wlat`), the desktop page's input-to-frame time (a key from
QEMU's keyboard to the next frame drawn after its keydown), frames over 33 ms, and two gzip runs' time, idle, with a busy
loop on each processor, and during a 1.5 GB copy:

| Busy loop on each processor | wake-up p99 | p99.9 | worst | page input-to-frame p99 | input queue p50 | frames > 33 ms (all loads) | gzip |
|---|---|---|---|---|---|---|---|
| voluntary (today) | 337 µs | 1.97 ms | 2.8 / 17.4 / 13.8 ms | 16.0 ms | 2.1 ms | 0 | 4.99 s |
| full | 327 µs | 1.91 ms | 3.2 / 3.3 / 3.3 ms | 15.1 ms (one run 20.3) | 2.1 ms | 4 (during the copy) | 5.04 s |
| lazy | 96 µs | 0.93 ms | 2.8 / 1.8 / 3.6 ms | 16.1 ms | 3.2 ms | 0 | 4.99 s |
| none (round 1, another machine) | 503-534 µs | 1.9-2.1 ms | 18-21 ms | 15.1-15.6 ms | 1.9-2.2 ms | 0 | |
| HZ 250, voluntary (beside HZ 1000 on one machine) | 119 µs (HZ 1000: 187) | 3.52 ms (HZ 1000: 1.84) | 7.8 / 7.8 / 8.6 ms (HZ 1000: 5.0 / 6.6 / 17.8) | 15.1 ms (15.0) | 0.9 ms (2.0) | 0 | 4.98 s (4.96) |

Idle and during the copy every model gave the same numbers (p99 122-158 µs, input-to-frame p99 14.5-15.7 ms).
- *full*: removes voluntary's occasional 14-17 ms stall under a CPU load, but showed 4 slow frames during the copy where
  voluntary showed none: mixed, so not kept. It can be tried on the Vivobook with the start-up option `preempt=full` (no
  new kernel needed): the checklist.
- *lazy*: the best wake-up tail by far, but the page's input waited longer in the browser (input queue 2.6-4.1 ms against
  2.1) while the processors were busy: what a person feels got worse, so not kept.
- *none*: the worst stalls (18-21 ms).
- *HZ 250*: a better p99 and a shorter input queue in the browser, but twice the p99.9 under load (3.5 ms against 1.8: a
  4 ms tick) and every worst case near 8 ms: mixed, not kept. (Trying it on the Vivobook needs a kernel built with it;
  left for later, only if the owner feels lag under load with HZ 1000.)
- Throughput (gzip beside the loops) did not change with the model.

**Start-up: kept: the kernel packed with zstd, the wired network cards' drivers as modules.** Where the time goes (the new
start-up steps, `/run/rudolf/startup-times`), Standard, uptime in s:

| Step | release (1.6.0) | this tree |
|---|---|---|
| before the kernel's clock (firmware, loading and unpacking the kernel; QEMU's TSC) | **1.56** | **0.94** |
| the kernel, to /init | 0.77 | 0.71 |
| the start-up image (finding the drive, checking the layers) | to 4.5 | to 4.45 |
| system extensions, devices (udev), data partition, memory, graphics driver | 4.5 → 5.9 | 4.5 → 5.8 |
| X server started | 6.6 | 6.6 |
| X server answers (`rudolf-display --xsize`: Xorg's own initialisation, 2.6 s) | (not timed) | 9.2 |
| session bus, sound, packages | → 10.0 | → 9.85 |
| desktop started → the desktop answered (its first heartbeat) | 10.2 → 18.5 | 10.0 → 18.3 |
| **from power-on (TSC + uptime)** | **20.0** | **19.2** |

| Same machine, 4 starts each | before the kernel | to /init | desktop answered | from power-on |
|---|---|---|---|---|
| release | 1.56 | 0.77 | 18.48 | 20.04 |
| zstd only | 0.94 | 0.77 | 18.24 | 19.18 |
| network drivers as modules only | 1.51 | 0.71 | 18.27 | 19.78 |
| both (this tree) | 0.94 | 0.71 | 18.28 | 19.22 |
| Native: release / this tree | 1.59 / 0.94 | 0.87 / 0.81 | 13.37 / 12.95 | 14.96 / 13.89 |

- **zstd** (KERNEL_ZSTD, INITRAMFS_COMPRESSION_ZSTD): 0.6 s less before the kernel starts, for a kernel 1 MB larger
  (13 MB; the EFI image's two kernels stay well under its 30 MiB warning). An installed computer unpacks two kernels
  (the boot files' and, after the handover, the system's), so about 1.2 s there.
- **Network cards as modules** (E1000, E1000E, IGB, IGC, R8169, ATL1C, ALX, TIGON3, 8139CP, 8139TOO, NE2K_PCI, PCNET32):
  0.06 s less in the kernel in QEMU (whose network card is an e1000: its probe took 0.35-0.4 s under initcall_debug), 1 MB
  smaller; the wired link still comes up (the e1000 module loaded by modalias, the link there). On a laptop without such a
  card it only makes the kernel smaller. WireGuard stays built in (its tunnels are made through netlink, which would need
  the module loaded on demand).
- **Native answers about 5 s sooner than Standard** (13 s against 18.5 s, two machines): Chromium's start and the desktop
  page are most of Standard's last 8 s. (Native is the default for new installations since 1.6.0.)
- **Xorg's own start takes 2.6 s** between its socket appearing and its first answer (QEMU's bochs driver); the
  Vivobook's steps will show amdgpu's.
- **The health log's own numbers were late:** "the desktop was ready N s" was written 4 s after the page came up (its
  timer), and the starts log's mark up to 10 s after (the session's loop: 26 s for a desktop that answered at 18.5).
  Both now use the first heartbeat's own time (rudolfd writes it into /run/rudolf/bootok; `readyAt` in /api/health).
- **Tried and not kept:** the session's 1-s wait for rudolfd's downloads store, made 0.1 s: rudolfd has made it by then
  on every start measured (no gain). The initramfs's 3 s "still looking for the disc" in these runs is QEMU's (a slot
  started with no disc attached); an installed computer passes the drive on at the handover.
- **Not done, an idea for the owner:** the handover itself (the boot files' kernel starting the system's kernel by kexec)
  costs a second kernel start and start-up image on every installed computer. Skipping it when both are the same kernel
  would touch Secure Boot's lockdown path, so it is left to the owner's decision; the Vivobook's numbers will show its
  cost (rudolf.t0, the first start-up's time, is now passed on and is the first line of the steps).
- **Starts on every kind of virtual hardware**, as 1.6.0's kernel does (`kt161_t.py hw`, the data partition mounted each
  time): q35 with UEFI on NVMe, AHCI, virtio-blk, virtio-scsi and USB storage; i440fx with BIOS on IDE, virtio-blk and
  NVMe: 8 of 8 for both kernels.
- **Reproducible:** two builds of this tree's kernel (same configuration, `testkernel.sh REPRO=1`) are identical except the
  module-signing certificate each build makes (.init.data) and the build id (.notes): every module (signature taken off)
  and every other section the same (`systools/kreprohash.py`).

**AMD power: switchable, kept at 1.6.0's until the owner's battery numbers.** Settings › System health › Power and
battery › Processor energy preference: *As in 1.6.0* (default), *Tuned* (Balanced on battery asks balance_power; game mode
asks performance), *The kernel's own* (amd_pstate passive). amd_pstate's active mode is already the kernel's default
(CONFIG_X86_AMD_PSTATE_DEFAULT_MODE=3, Mendocino has CPPC), so `amd_pstate=active` adds nothing. QEMU has no frequency
driver: the choice is greyed out there and on Intel, and the power settings work as before (host test power_t.sh 50/50,
the power phase in QEMU).

**Memory:** no change kept; zram's lz4 proposed for a Vivobook check. Stand-ins started together on the 1.6.0
beta (2026111707), 3.3 GB: tabs (500+500+500+300 MB, compressing about 3:1 as web pages' heaps do), a game (1.3 GB, half
compressible, a 60 fps loop touching a quarter of it each second) and a call (a 20 ms audio period), for 100 s; the
memory stalls (PSI "full"), the game's frames over 33 ms, the call's periods more than 10 ms late, and how long the memory
guard saw memory critical. (A first load of 2.1 GB left 400 MB free and measured nothing: no swap at all.) Medians, each
setting 2-3 times, on one machine per column:

| Setting | memory stalled (PSI full) | game frames > 33 ms | call periods > 10 ms late | guard: memory critical |
|---|---|---|---|---|
| swappiness 180, zram = memory, zstd (today) | 7.98 s | 111 | 218 | 23 s |
| swappiness 200 | 6.22 s | 88 | 128 | 17 s |
| swappiness 100 | 8.74 s | 77 | 131 | 12 s |
| swappiness 60 | **15.34 s** | 73 | 137 | **32 s** |
| zram 1.5 × memory | 7.66 s | 86 | 85 | 18 s |
| **lz4 instead of zstd** (beside zstd in turn, 3 pairs) | **5.62 s** (zstd 11.18) | **72** (110) | **85** (166) | **12 s** (28) |

No setting ended a process (no OOM kills, the memory guard ended nothing) and every one ran memory down to 0-6 MB free.
- **Swappiness:** 60 is clearly worse (twice the stalls, and the guard in critical twice as long), which confirms 1.5.9's
  move to a high value; 100, 180 and 200 differ by less than the runs among themselves (180's call figure ran 50 to 241):
  180 is kept.
- **zram's size:** 1.5 × memory changed nothing measurable (zram never filled at 1 ×): kept as it is.
- **lz4:** fewer late call periods in all three pairs, fewer slow game frames in two (the third even), fewer stalls in
  two, and the guard critical less than half as long: lz4 is faster to compress and read back. But it packs less (about 2:1 against zstd's 3:1), so
  zram holds less before memory really runs out, which these 100-second runs did not reach and on 3.3 GB matters (the
  Vivobook's game was ended by the guard in 1.5.9). **Not changed yet:** a later PR would build ZRAM_BACKEND_LZ4 and let
  the memory setting choose it, once a run as long as a real game session shows it does not end processes sooner.

**Required features:** the check (its own PR, first) found FUSE missing from the x86 kernel (Flatpak's document portal,
AppImages); it is added there. Not on the list, noted: `lsm=` names landlock but SECURITY_LANDLOCK is not built; the
encrypted folders use fscrypt, not dm-crypt (the list checks fscrypt and its ciphers).

- **The time before the kernel, on real computers (2026-11-20, the owner: "add a firmware/kernel/system breakdown ... if it
  will help to optimize the start-up time").** The steps above start at the kernel's clock; QEMU's "before the kernel"
  came from the test's own counter. Now rudolfd (rudolfd-boottime.inc), at its start, as root, adds one line to the
  steps: the time from power-on to the kernel's clock, from the processor's time-stamp counter (a constant, non-stop TSC,
  its rate from the kernel's log), split into the firmware's part and the boot loader's from ACPI's FPDT (the kernel's
  `ACPI_FPDT`, now on), with a handover's first stage (rudolf.t0) taken off. Last starts shows it first and ends with
  the whole from power-on; the health log's "ready" line adds "N s from power-on" (Standard and Native). Nothing is
  shown when neither source gives a believable number (0 to 300 s). Host test boottime61_t.sh (14/14); the check
  workflow boottime-check.yml. What it tells: whether the Vivobook's 27-37 s is mostly firmware (only the firmware's
  own settings can help: Fast Boot, fewer boot entries), the handover (the "skip the handover" idea below), Xorg and
  amdgpu, or the desktop.

#### The owner's checklist (the Vivobook; the Asus desktop where it says)
1. **Start-up** (each kernel: 1.6.0's, then this one): restart 3 times; after each, Settings › System health › Last starts
   › Where the time went: write down "the desktop answered" and the first line (t0, the boot files' part before the
   handover), and the time from the power button to the sign-in screen with a stopwatch. Expected: about 1 s less.
   The Asus desktop from the USB stick: the same, 3 starts each.
2. **Responsiveness** (no new kernel: a start-up option): start once normally and once with `preempt=full` (the boot menu's
   options), and on each: a game or a large copy (a 2 GB file to a USB stick) while typing in Oracle and moving windows for
   2 minutes; write down any stall you notice (when, how long) and the desktop's frame meter if shown. Keep *full* only if
   it feels better and nothing stutters more.
3. **Power** (Settings › System health › Power and battery › Processor energy preference): see the power PR's checklist
   (15 minutes idle and 15 minutes of a video on each of the three choices, battery % and watts).
4. **Memory:** (a kernel with lz4, if that later PR comes) a game, Oracle with 4 tabs and a Discord or Meet call together
   for 15 minutes on each compressor: write down whether the game stuttered, whether the call broke up, and the
   health log's memory lines (the guard's warnings and what it ended).

### 14. Native, the default for new installations (moved from 1.6.0 on 2026-11-17)
The owner, 2026-11-17: "Make Standard the default in 1.6.0 with the transition to Native default in 1.6.1."
- **First: Discord and Steam on Native on the Vivobook** (slow to load or not starting, beta 2026111713; #470's open
  item 3: its leads, the refused system-API request at 13:10:17, the new log lines "oa: KEY asked of rudolfd N ms after
  its page began"). A devlink read while each loads on Native; the cause proven and fixed; the owner's check.
- **Then the switch:** install.sh's `NATIVE_DEFAULT=1` (a new installation starts on Native, after the setup wizard on
  Standard, #467) and rudolfd's `NATIVE_OFFER 1` (Settings offers Native once to a computer updated on Standard); the
  engine page's "A new installation starts with it." back; nativedefault60_t.sh 0 and 0b changed to match.
- **The owner's checks:** a new stick (Make a USB stick) and Install here: the setup wizard, then Native; an updated
  computer on Standard: the offer once; Discord, Steam, the icons and Oracle's keys on Native.

### 9b. The first ARM64 release (from 1.6.1 Part 9)
- **ARM64 (1.6.0 Part 7 and its stage B item 4):** the first ARM64 release, after one ARM64 computer that starts with
  UEFI (or the first phone, Part 4: the PinePhone Pro through Tow-Boot), its own update lists (beta/arm64/,
  stable/arm64/), the update and recovery checks on it.


### 19b. Gaming measured (from 1.6.1 Part 19)
The NVIDIA driver choice and the game mode are built in 1.6.1; measuring them needs the owner's computers.
- **Measured:** the same games' frame rates and frame times on the open and NVIDIA drivers and with game mode on and
  off, on the owner's Asus desktop (live from a stick, as Part 12) and the Vivobook.


**The checklist is written (2026-11-21, branch game-checklist): below; the numbers are the owner's.** It follows the
  NVIDIA driver PR (#544, branch nvidia-driver) and the game mode PR (#546, branch game-mode): run it once both are in a beta.

#### 19b. The owner's checklist: games measured (the Asus desktop live from a stick, as Part 12, and the Vivobook)
What is measured: the same games' frame rates and frame times on the open driver (NVK) and on NVIDIA's driver, each with
game mode on and off, on the owner's **Asus desktop** (RuckXTrauma: its NVIDIA card must be GeForce 16 or newer for NVIDIA's
driver to be offered), Rudolf OS running live from a USB stick (nothing on its disk changed), and with game mode on and off
on the **Vivobook** (AMD graphics: no NVIDIA rows). Nothing here was measured by a session: no NVIDIA card or real game is
reachable from CI.

**Before (once).**
1. The stick: made with Settings › Update and recovery › Make a stick, with its RUDOLFDATA partition (system extensions and
   Steam's games are kept there; a stick without one cannot keep NVIDIA's driver). Start the Asus from it; Secure Boot as
   it is on that computer (with Secure Boot on, the first start asks to enroll the Rudolf OS key: that is the firmware's
   own list, the disk is not touched; or start with Secure Boot off for these tests).
2. NVIDIA's driver: after the owner ran **Actions › System extensions › Run workflow** (release = the release's serial,
   names = `nvidia-driver`), Settings › Update and recovery › System extensions › Install… › NVIDIA's graphics driver
   (an administrator's password), then restart. Settings › Display › Graphics driver shows the card's name.
3. The games, the same on both computers where they run (fill in): A ______ (a native Linux game in Steam),
   B ______ (a Windows game through Proton, DirectX 11/12), C ______ (a Vulkan game or a built-in benchmark), D ______
   (a browser game in Oracle, WebGL). Each with the same settings every time (resolution ____, quality ____, V-Sync **off**),
   the same 2-minute scene or the game's own benchmark.
4. Frame times: Steam (Flatpak) with MangoHud (`flatpak install flathub org.freedesktop.Platform.VulkanLayer.MangoHud`,
   the game's launch options `MANGOHUD=1 MANGOHUD_CONFIG=autostart_log=1,log_duration=120 %command%`): each run leaves a
   CSV in ~/.var/app/com.valvesoftware.Steam/ (its summary: average FPS, 1 % low, 0.1 % low). The browser game: Oracle's
   own frame meter (F12 › Rendering › Frame rendering stats) or the game's counter, average only.
5. Each line below: 3 runs, the median written; the computer on mains power; nothing else open (Oracle closed for A-C).

**Each run, also look at (write "ok" or what happened):**
- Game mode on: Settings › System health › Memory › Game mode says "On now" during the game (open it with the game
  in the background, then go back); on the Vivobook, System health › Power and battery shows the energy preference
  "performance" during the game and the usual one after; a notification sent during the game (a message, a reminder)
  is held, and "While you were playing" comes once the game leaves full screen.
- Game mode off (the same switch): none of that.
- After each game is closed: Task Manager › System (the desktop's processor time is back), no slow desktop.
- NVIDIA's driver: Task Manager › System › Graphics names NVIDIA's driver; once, start Recovery mode: it starts on the
  open driver (the firmware's screen), and the next normal start is back on NVIDIA's.
- Once, to see the switch back: with NVIDIA's driver chosen, remove the extension (System extensions › Remove) and
  restart: that start is on the open driver, and a notification says it went back by itself and why.

**The Asus desktop (RuckXTrauma), live from the stick.** CPU ______, memory ______, NVIDIA card ______ (its PCI ID from
Settings › Display › Graphics driver ______), screen ______ Hz.

| Game | Driver | Game mode | Average FPS | 1 % low FPS | 0.1 % low FPS | Frame time p99 (ms) | Notes (stutter, errors) |
|---|---|---|---|---|---|---|---|
| A | Open (NVK) | off | | | | | |
| A | Open (NVK) | on | | | | | |
| A | NVIDIA's | off | | | | | |
| A | NVIDIA's | on | | | | | |
| B | Open (NVK) | off | | | | | |
| B | Open (NVK) | on | | | | | |
| B | NVIDIA's | off | | | | | |
| B | NVIDIA's | on | | | | | |
| C | Open (NVK) | off | | | | | |
| C | Open (NVK) | on | | | | | |
| C | NVIDIA's | off | | | | | |
| C | NVIDIA's | on | | | | | |
| D | Open (NVK) | off | | | | | |
| D | Open (NVK) | on | | | | | |
| D | NVIDIA's | off | | | | | |
| D | NVIDIA's | on | | | | | |

Also on NVIDIA's driver, once: does DLSS (or its frame generation, GeForce 40 and newer) appear in B's or C's settings
(Proton gives games NVIDIA's DLSS files: the extension carries them), and does ray tracing switch on? ______

**The Vivobook (AMD graphics, amd_pstate).** The games that run on it (A, D, and B or C if they start):

| Game | Game mode | Average FPS | 1 % low FPS | 0.1 % low FPS | Frame time p99 (ms) | Energy preference during the game | Notes |
|---|---|---|---|---|---|---|---|
| A | off | | | | | | |
| A | on | | | | | | |
| D | off | | | | | | |
| D | on | | | | | | |
| B or C | off | | | | | | |
| B or C | on | | | | | | |

**What decides.** NVIDIA's driver stays an option (never the default) if it starts and runs the games on the Asus; it is
worth recommending in Settings only where it is clearly faster (more than 10 % average or 1 % low). Game mode stays on by
default if "on" is never slower than "off" on either computer (within the runs' own spread); if the desktop's
slowing behind a game ever makes the game itself stutter (a launcher or overlay in the desktop's group), say so: the
weight (20) is one line in /usr/lib/rudolf/gamemode.

### 21b. The checks with the owner (from 1.6.1 Part 21)
- **On the Vivobook, with the owner:** Oracle's websites encrypted (1.6.0 Part 22) on Native, Lite and Standard turned
  on; Recovery mode from a stick made from this release and from another build's; two-step sign-in on Standard and
  Native; calls; a real CAC only when the owner asks.
- **Blitzen's guards (1.6.1 Part 26), with the owner:** asked for the GitHub token and a key file in several ways, a throwaway
  test key pasted, a program listing ~/.ssh, on Standard and Native (docs/plan-1.6.1.md Part 26, the owner's checks).

### 27. Bring your files: personal files kept from the old system when installing (added on 2026-11-21)
The owner asked on 2026-11-21: "Would it be possible for someone downloading and installing Rudolf to be able to
save/transfer their personal files (documents, pictures, videos, music, etc.) from their old operating system?", then
"Go ahead" (for 1.6.2, after 1.6.1 is promoted). Unlike the rest of this plan it does not wait on the owner's hardware;
it is here so it does not delay 1.6.1's final beta.

**Today (1.6.0):** possible by hand. Started live from the stick, File Explorer › the old drive › Copy to another drive…
with "Only personal files" (drivecopy60.js, drivecopy.sh) copies them to a USB drive; Settings › Update and recovery ›
Install Rudolf OS on this computer erases the whole disk; the files are copied back by hand. The installer says the
disk is erased but does not offer to keep anything, so someone who does not know of the copy loses their files.

**What to build:**
1. **A "Bring your files" step in the installer**, before anything is erased, shown when the chosen disk holds an
   operating system or user folders: what was found (Windows: Users\NAME\Documents, Pictures, Music, Videos, Desktop,
   Downloads; macOS: /Users/NAME/...; Linux: /home/NAME/... and the XDG folders), per person on that computer, with the
   size of each folder. Choices: keep them (the default when any are found), choose which, or erase without keeping
   (said plainly, with the size that will be lost).
2. **Where they go while the disk is erased:** a USB drive or external disk the person plugs in (its free space checked
   against the size first; "Only personal files" as drivecopy60 defines it: no Windows, programs, AppData, caches or
   system files; several drives in turn as drivecopy does). Every file copied is checked (size and SHA-256, as xfer61's
   checked moves) before the installer may erase anything; a failed or cancelled copy stops the installation with
   nothing erased.
3. **After installing:** at the first sign-in (or when the person's account is made in the first-run setup) Rudolf
   offers to bring them in: the old Documents into Documents, Pictures into Pictures, and so on (the rest under
   "From the old computer"), into the account's own encrypted home (the fscrypt homes, the per-account Linux user's
   ownership), with names that clash kept side by side. The external copy is left as it is until the person deletes it.
4. **Disks that cannot be read as they are:** Windows with BitLocker (many Windows 11 laptops encrypt without the owner
   knowing): ask for the recovery key, say where it usually is (the person's Microsoft account, account.microsoft.com/
   devices/recoverykey, or a printout), unlock read-only (cryptsetup's BitLocker support, or dislocker if needed: check
   what the base has and the size it adds); macOS (APFS, usually encrypted): say that Rudolf cannot read it and how to
   copy the files on the Mac first; a Windows that was hibernated or "fast started" (NTFS left dirty): mount read-only,
   or say to shut Windows down fully. The old disk is only ever mounted read-only until the person agreed to erase it.
5. **Later, only if it is safe enough (decide after 1-4):** keeping the files on the same disk without an external drive
   (copying them into free space, then shrinking or replacing the old partition). Risky (a power cut mid-way); not
   without a checked backup path and the owner's decision.

**Security:** reading another system's disk runs through rudolfd's root helpers (drives, Part 11) with paths checked;
nothing from the old disk is run; the copied files go through the same checks as any file brought in (the antivirus
where Rudolf uses it); a BitLocker key is used only to unlock and never stored.

**Tests:** disk images made in CI (NTFS with a Windows-like Users tree, a dirty NTFS, ext4 with /home, a BitLocker image
if cryptsetup can make one; APFS: the message only), in QEMU with the real release (iso/qt/sb/) and a second virtual
disk as the "USB drive": the files found and sized, copied and checked, the install going ahead only after the copy,
a cancelled copy erasing nothing, the files in the right folders after the first sign-in, Standard and Native the same
(the owner's rule). **The owner's checks:** a real old Windows laptop or disk, if one is at hand, with a USB drive.

### 25b. Linux apps' progress on their taskbar buttons (from 1.6.1 Part 25, item 5)
Firefox, Chromium-based apps (Discord, VS Code, Steam's web parts) and others say their downloads' progress with the Unity
LauncherEntry D-Bus signal (com.canonical.Unity.LauncherEntry, Update: the app's application://NAME.desktop, "progress" 0-1 and
"progress-visible"). Not done in 1.6.1 Part 25: the apps run as each account's own Linux user on that account's own session bus
(/run/user/UID/bus, its folder 0700), and the shell, the desktop user, cannot listen there without opening the account's bus to it.
The way that weakens nothing:
- the account's own rudolf-portal (already that user's process on that bus) listens for the signal and leaves a progress note in its
  request folder as it leaves its questions (rudolfd's /run/rudolf/portal/req: each request its sender's, 0600);
- rudolfd passes on only a desktop file id (letters, digits and . - _) and a number, checking the note is from that account's portal
  (its peer check and owner), and the confinement lists (confinepure, winapi) are updated for the new kind;
- rudolf-native (and Standard's page, through linux26.js's lxtb buttons) maps the desktop file id to the account's window
  (_GTK_APPLICATION_ID, WM_CLASS, the lx: key) and calls the same fill (tbp_set; winProgress on Standard).
A host test with a D-Bus sender on a stand-in account bus; the owner's check: a Firefox download with Firefox minimized.

### 29. Rudolf's title bar on Standard's windows from outside the page (added on 2026-11-21)
The owner, 2026-11-21, on 1.6.1 (Standard, the Vivobook), with Oracle open in its own Chromium ("Keep my websites
encrypted with my account" on, #583): "The Oracle browser on Chrome OS does not use the operating system's title bars".
Asked which windows, the owner chose **every window from outside the page**: Standard's encrypted Oracle, its Tor
windows (Browse with Tor) and Linux apps, as Native already frames them all.

**Why it is so today.** Standard draws title bars only for its own windows inside the desktop page (and normal Oracle's,
whose tabs are in the page). Everything else is a separate X window that rudolf-display, Standard's minimal window
manager, fits above the taskbar with no frame of Rudolf's (1.6.1 Part 15: "Standard frames no Linux app's window"). With no
window manager it knows, Chromium draws its own frame (the shell's colour, "Rudolf OS", ⋮ – ▢ ✕), and Electron and GTK apps
draw theirs.

**The way (as Native, without changing what Standard's page windows look like):**
1. **A strip for the bar.** rudolf-display places a shown app window below a title-bar strip at the top (the page's own
   title-bar height, from the settings it already reads), above the taskbar. Full-screen windows and games (borderless,
   screen-sized, `_NET_WM_STATE_FULLSCREEN`) keep the whole screen, as now; dialogs stay centred above their window.
2. **The page draws the bar in that strip.** The desktop page is the full-screen window behind; while an app window is
   shown it draws Rudolf's title bar there with the same code and look as its own windows (style, accent, the account's
   title-bar settings): the window's icon and title (/run/rudolf/windows), minimize (back to the desktop: wm "desktop"),
   close (wmcmd "close"), and the ⋮ menu where Rudolf's windows have one. Maximize/restore is not offered: Standard shows
   one app window at a time, fitted. Clicks on the strip go to the page (it is the page's own pixels).
3. **The window's own bar off where the app allows.** rudolf-display announces itself as a window manager
   (`_NET_SUPPORTING_WM_CHECK`, as rudolf-native does), so Chromium leaves its own frame off (1.6.1 Part 15's finding);
   the encrypted Oracle's and the Tor windows' profiles get `browser.custom_chrome_frame: false` when the key is missing
   (rudolfd writes the Tor profile's Preferences already; orawin's in the account's folder, once). Part 15 item 1's list
   (`titlebars`: Chromium-based browsers, VS Code) then applies on Standard too. Apps that always draw their own (Steam,
   Discord, Claude) show both, as on Native before Part 15's later items: what Native does for them is followed here.
4. **Nothing weakened:** the page learns only what /run/rudolf/windows already gives the taskbar; no new rudolfd request
   unless a check finds one is needed (then confinepure/winapi updated, the token and peer check as for every request).

**Tests:** a host test with rudolf-display and a stand-in app (fixtures/xwmclient.c) on Xvfb: the strip, full screen and
games untouched, a dialog, `_NET_SUPPORTING_WM_CHECK`, close and minimize through wmcmd/wm; the page's bar in Chromium with
a stand-in rudolfd (title, icon, the title-bar styles); Standard and Native compared (same_t.sh) for an outside window.

**For the owner, on the Vivobook (Standard):** encrypted Oracle, a Tor window, a Flatpak app (one that asks for a frame,
one that draws its own), a game in full screen; minimize and close from Rudolf's bar; the title-bar styles and colours.

### 30. Two-player Library games on a home network: the LAN game relay (added on 2026-11-22)
Ember Reach's duels (rudolfos-library, Ember Reach 0.5.2) on a LAN as well as online; the owner: "Go with WebSocket and
LAN as the selectable options for networking for the player". A game page can't accept connections, so while a player
hosts, Rudolf OS runs a small relay on that computer, and the other computer finds it.
- **The relay:** `rudolf-duel serve` (iso/sys/rudolf-duel.c) speaks the Library relay's protocol (rudolfos-library
  docs/duel-protocol.md, tools/duel-relay/protocol.mjs), with one room, as `relay.mjs --lan CODE` has it.
  - The room has the code rudolfd chose, for that game and version. Another version is refused with a message.
  - The games' messages are passed to the other as sent, never with an address.
  - Limits (protocol.mjs's): 8 KB a message, 90 a second, 10 s to say hello, 60 s silent once paired; 4 connections.
    Text that isn't UTF-8 is refused.
- **Who and where:**
  - It runs as the signed-in account's own Linux user (the desktop's with nobody signed in), never root (it refuses
    to), with no new privileges and no extra groups.
  - It takes connections only from loopback and private addresses.
  - The firewall's new duel chain lets its port (47731) in from private addresses, never on a cellular interface. It
    is opened just before the relay starts and closed by its watcher the moment the relay ends.
- **When:**
  - Started only when the player hosts (the game kit's `lanHost` → desktop/duel62.js → rudolfd's `duel/host`).
  - Stops by itself when its duel ends (the hosting game leaves, or either player once paired), when no game hosts
    within a minute, or when nobody joins in 10 minutes. The game's `lanStop` (`duel/stop`) also stops it.
- **Found by multicast DNS:**
  - Announced as `NAME._emberduel._tcp.local` (the Library's protocol names it), with TXT game=, ver= and code=, on
    the interfaces that are up (not cellular). It answers only questions for itself, and says goodbye when it ends.
  - `duel/find` runs `rudolf-duel find` as the account's user and lists that game's duels on the network.
  - Port 5353 is shared with Find printers. The parsing is bounded and fuzzed.
- **Who may ask:**
  - rudolfd's `duel/` requests are authorized like the page's other system calls (the token and the peer check: only
    the desktop).
  - The desktop answers the game kit only for the project's own Library apps; an outside developer's app is refused.
  - Native's game windows are the same page, so it works the same on Standard, Lite and Native.
- **Tests:**
  - `iso/qt/host/duel162_t.sh`: the real rudolfd and relay in namespaces of the test's own, with the firewall's
    rules, a home network and a cellular one. It covers the token and peer check; the relay's user; the port open only
    while hosting; found by mDNS from the home network but not the cellular one; a duel and another version through
    it; stopping when the host leaves, on duel/stop and with nobody hosting; duel/find; and the fuzz test.
  - `iso/qt/shell2/duel62_t.js`: the page answering the game kit.
  - `iso/qt/shell2/duelgame62_t.sh`: the Library's own two-page Ember Reach test, its LAN relay swapped for
    rudolf-duel: the LAN duel through it.
- **The owner's checklist (two computers on the home network, both on a 1.6.2 beta with this, Ember Reach 0.5.2):**
  1. A: Ember Reach › Duels › LAN › Host on this network. A room code shows.
  2. B: Duels › LAN › Find games. A is listed by its computer name: Join. The match starts on both.
  3. A round: each ship moves smoothly on the other screen, hits count on both, a kill takes the round.
  4. Finish, then Rematch on both. Close B's game window mid-match: A says "The duel was cut short".
  5. On A after the match: Settings › Security shows the firewall on, and System health › Security names no new
     listener.
  6. With A on a phone's hotspot or a cellular modem only: Host on this network says it isn't on a home network.

## Steps
1. Part 14 first (Discord and Steam on Native on the Vivobook, then the owner's decision on the default).
2. Part 12's checklist and 19b's measurements with the owner, after the 1.6.1 beta.
3. Part 8 with the MiFi, then Part 7; then the phone: Part 4, Part 6's items measured, 9b, Part 5 when the hardware is there.
4. 21b after each beta.
5. 25b (Linux apps' progress on their taskbar buttons), then a Firefox download checked on the Vivobook.
6. Part 27 (Bring your files) after 1.6.1 is promoted: 1-4 in one or two PRs, 5 only by the owner's decision.
7. Part 29 (Rudolf's title bar on Standard's windows from outside the page): 1-3 in one PR, then checked on the Vivobook.
8. Part 30 (the LAN game relay): built with Ember Reach 0.5.2; then the owner's checklist on two computers.
