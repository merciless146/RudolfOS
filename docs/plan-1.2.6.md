# Rudolf OS 1.2.6 and 1.2.7 update plan

1.2.6 started with fixes for what came up after 1.2.5 went to stable (serial 2026102311): two bugs a user meets
in normal use, and a release-notes slip. They went first, in **stage A**, so they could reach stable on their own.

> **Stage A is released as 1.2.6**: PR #32 (with the tidy-ups in PR #34), stable serial 2026102517 (2026-10-25),
> disc `disc-1.2.6`, whose own checksum list passes with the boot loader included.

The features (items 5 to 9) follow in **stages B and C** and ship as **1.2.7** (`iso/VERSION` 1.2.7, `RVER` 1.2.7),
since 1.2.6 is out:
- **Stage B:** the kernel's new USB Wi-Fi drivers, and two changes to the desktop page (Luau, Files).
- **Stage C:** the hardware check's new steps, with the webcam driver they need, and joining a downloaded disc
  image. Both add to rudolfd.

| # | Item | Kind | Size | Stage |
|---|------|------|------|-------|
| 1 | "Check this disc" reports every disc as damaged | bug | small | A |
| 2 | Make a Rudolf OS USB stick refuses a stick that is already formatted | bug | small | A |
| 3 | CHANGELOG 1.2.5: date and component versions | bug | tiny | A |
| 4 | Tests that would have caught 1 and 2 | tests | small | A |
| 5 | More USB Wi-Fi adapters: Atheros AR9271, MediaTek MT7601U, MT7610U and MT7612U | feature | small | B |
| 6 | Faster first Luau type check | feature | small | B |
| 7 | Copy folders from a USB drive or memory card (Files) | feature | small | B |
| 8 | Hardware check: keyboard, touchpad or mouse, camera, battery, screen (and the webcam driver) | feature | medium | C |
| 9 | Make a stick or Install from the downloaded parts, without joining them by hand | feature | small | C |

Stage A changes the disc writer, the start-up script (initramfs) and one system script. That means it needs a
release build and the Secure Boot tests, not only page tests. Like every stage, it has one pull request, is built
to beta, and is tested in QEMU. Stage A was promoted to stable as 1.2.6 by itself.

---

## 1. "Check this disc" reports every disc as damaged

**What happens.** At start-up, the menu entry *Check this disc or USB drive for errors* (UEFI:
`rudolf-check.conf`, BIOS: item 3, both `rudolf.check=1`) runs `sha256sum -c rudolf/sha256` in the initramfs.
Every Rudolf OS disc and every stick written from one fails on `boot/rboot.bin`. The user sees "ERRORS FOUND - this
disc or USB drive is damaged. Make a new one.", which isn't true. Making a new stick doesn't help, because the new
one fails the same way.

**Why.** `systools/mkdisc.sh` writes `RUDOLF/SHA256` from the file tree. After that, `mkimage.py` writes the ISO and
patches the kernel's place on the disc (`RBTB` + LBA + size) into the copy of `BOOT/RBOOT.BIN` on the image. So the
file on the disc never matches its line in the list. This has been the case since 1.1. The published disc-1.2.4 and
disc-1.2.5 both show it: every other file matches, and `boot/rboot.bin` fails.

**Fix.**
- `mkimage.py iso`: after it patches the boot loader, it recomputes that file's SHA-256 and rewrites its line in
  `RUDOLF/SHA256`. The list file isn't in its own list, so the change is safe, and the list stays complete.
  An option, `--sums RUDOLF/SHA256`, names the list. mkdisc.sh passes it.
- initramfs `init`: discs already made (1.2.5 and older) must stop reporting damage too, once their computer has
  installed 1.2.6, because the updated kernel brings the new init. So the check compares `boot/rboot.bin`
  separately. The file is fine if the list's hash matches, or if the hash matches after the 8 bytes after `RBTB`
  are zeroed (the file as it was before patching). Every other file is checked as before.
- The check's report names the files that failed and says how many were checked, for example "1 312 files
  checked, all intact". At the moment it only prints the lines that aren't OK.

**Test.** `sb/t126a_t.py`:
- Start the test disc with `rudolf.check=1`. The debug entry gets a `rudolf-check-debug` twin, in test discs only.
- Expect "All files are intact" in `/etc/rudolf/check.txt`.
- Then flip one byte in a copy of the disc's `DESKTOP.SFS` and expect "ERRORS FOUND", naming `rudolf/desktop.sfs`.
- On the host, run a unit check of `mkimage.py`: the rewritten line matches the patched file on the image.

## 2. Make a Rudolf OS USB stick refuses a stick that is already formatted

**What happens (from the code; to be confirmed first in QEMU with a FAT stick).**
- When a USB stick is plugged in, `devplug` mounts its partitions under `/media/sdX1`, and so on. Sticks come
  formatted FAT32 or exFAT from the shop.
- *Make the stick…* goes from `stick30.js` to rudolfd `mkstick` to `mkstick.sh` to `install.sh`.
- `install.sh` stops at `grep -q "^$DEV" /proc/mounts`: "/dev/sdb is in use. Unmount its partitions first."
- The page doesn't unmount the stick first, and the page has no Unmount button for this.
- So on a real computer the feature only works with a blank or Rudolf OS stick. Rudolf OS doesn't mount its own
  sticks, because `devplug` skips iso9660 and RUDOLFDATA.
- The stage C test used a blank 4 GB image, so it never met a mounted stick.

**Fix.**
- `mkstick.sh`, before `install.sh`, does three things:
  - Marks the disk busy: `/run/rudolf/busy/<sdX>`, which `devplug` honours. While the file is there, `devplug`
    mounts nothing on that disk. Without this, the partitions that `--rereadpt` brings back, among them the
    image's EFI partition, would be mounted again.
  - Unmounts every partition of that disk that is mounted under `/media/`, and only there. Anything mounted
    elsewhere still counts as in use, as before.
  - Says which drive was unmounted.
  The busy mark is removed on exit, whether the script succeeds or fails, by a `trap`.
- If a program still has a file open on the stick, `umount` fails. The message then says to close what uses the
  drive (Files, a Linux app), not "Unmount its partitions".
- `stkConfirm` (the page) says that the drive's current files and name will be erased, and shows its label when
  it has one. The text already says "Everything on it is erased."
- *Install here* has the same check. Internal disks aren't auto-mounted, but an external USB disk is. The same
  unmount runs there, only for `/media/` mounts.

**Test.**
- `t125c_t.py` gets a second stick: 4 GB, FAT32, label `SHOPSTICK`, holding one file. Rudolf OS mounts it at start.
- *Make the stick* must succeed on it, and must leave nothing mounted from it during or after.
- The new stick then starts as in step 2 of that test.
- Separately: the same request while a shell holds a file open on the stick must give the new message, and the
  stick must stay unchanged.

## 3. CHANGELOG 1.2.5: date and component versions

- The 1.2.5 entry says `(2026-10-19)`. The release that went to stable was built on 2026-10-23. It becomes
  `(2026-10-23)`.
- The entry also doesn't mention that the release carries Chromium 156.0.8078.47 and Linux 6.18.58. A scheduled
  rebuild picked these up before the promote; the stable channel's `notes` line says only "Chromium …". Add one line:
  "Updated: Chromium 156 and Linux 6.18.58 (security fixes)."
- The stable notes line itself is fine. `devices26.js` already shows the CHANGELOG summary when `notes` only names
  a component.
- 1.2.6's own entry is written in stage A and grows with each stage.

## 4. Tests that would have caught 1 and 2

- Above: `t126a_t.py` covers the disc check, and the second stick goes into `t125c_t.py` (renamed `t126c_t.py`).
- Release steps: the regression suites (`sb_t.py`, `t124_t.py`, `v120_t.py`, `tm_t.js`, `dl_t.js`) run on the
  **final** beta, the one being promoted, and not only per stage. For 1.2.5 only the four stage tests were re-run on
  2026102311.
- A scheduled rebuild can replace the beta between the tests and the promote; that happened with 1.2.5
  (2026102102 → 2026102311). So the promote input is the tested serial, and the tests are re-run if the beta
  changed. This is a note in `RELEASES.md`, not a code change.

---

## 5. More USB Wi-Fi adapters (stage B)

> **As built** (branch `rudolf-1.2.7-b`). Differences from the plan below:
> - **Firmware.** `wififw.py` runs only in the full disc build, so a release build does not pick firmware up by
>   itself. A release build takes firmware from the pinned list in `firmware-extra.txt` (`fwextra.py`). The eight
>   files and three links are pinned there with their SHA-256: `htc_9271.fw`, `htc_7010.fw`,
>   `ath9k_htc/htc_*-1.4.0.fw`, `mediatek/mt7601u.bin`, `mediatek/mt7610u.bin`, `mediatek/mt7662.bin` and
>   `mt7662_rom_patch.bin`. They are added to the CI build's sparse linux-firmware checkout.
> - **Kernel options.** `MT76x0U` and `MT76x2U` have lower case in their names, and `scripts/config` upper-cases
>   names unless given `--keep-case`. So `kconfig.sh` has an `mk` helper for them; with `m` they would have been
>   left out without a word. No other option in the file is affected.
> - **mt76x2u firmware names.** The driver asks for `mt7662.bin` and `mt7662_rom_patch.bin`, not the `mt7662u…`
>   names in the table below.

**Now.** Rudolf OS already drives most current USB Wi-Fi adapters:
- Realtek RTL8188/8192 (`rtl8xxxu`), RTL8811AU/8812AU, RTL8821CU, RTL8822BU/CU, RTL8852BU, RTL8851BU (`rtw88`, `rtw89`);
- MediaTek MT7921AU and MT7925U;
- Ralink RT2800 (`rt2800usb`).

A plugged-in adapter appears in Settings › Network like a built-in card, and Devices lists it as a "Wi-Fi adapter".

**Missing** (`kconfig.sh` line 48), so these adapters show as a plain USB device and Settings says "No Wi-Fi adapter was found":

| Driver | Chip | Typical adapters | Firmware (linux-firmware) |
|--------|------|------------------|---------------------------|
| `ATH9K_HTC` | Atheros AR9271, AR7010 | TP-Link TL-WN722N v1, many "Linux-friendly" 2.4 GHz adapters | `htc_9271.fw`, `htc_7010.fw` |
| `MT7601U` | MediaTek MT7601U | low-cost 150 Mb/s nano adapters | `mt7601u.bin` |
| `MT76x0U` | MediaTek MT7610U | AC600 dual-band adapters | `mediatek/mt7610u.bin` |
| `MT76x2U` | MediaTek MT7612U | AC1200 adapters, among them Alfa AWUS036ACM | `mediatek/mt7662u.bin`, `mt7662u_rom_patch.bin` |

**Change.**
- Add the four options to the `m` line in `kconfig.sh`. Each one depends on USB and MAC80211 (already there) and pulls in its common parts by itself (`ATH9K_HW`, `ATH9K_COMMON`, `MT76x02_USB`, and so on).
- The firmware needs nothing extra. Each driver declares its files with `MODULE_FIRMWARE`, and `systools/wififw.py` copies exactly those from linux-firmware into the modules layer. Licences come along with linux-firmware's `WHENCE` entries, as for the others.
- Expected cost: roughly 0.5 MB of modules and 0.3 MB of firmware.
- `devplug` already names these as "Wi-Fi adapter" (by their network interface), and `wifi.sh` uses the first wireless interface. With a built-in card and a dongle both present, `wifi.sh` still takes the first one; picking the adapter is a separate, later item.
- CHANGELOG: "New: more USB Wi-Fi adapters work (Atheros AR9271, MediaTek MT7601U, MT7610U, MT7612U)."

**Test.** QEMU has no USB Wi-Fi device, so the test checks the built release instead (`t126b_t.py` on the test disc's serial shell):
- each new module is in the modules layer and loads (`modprobe`; without hardware it loads and stays idle);
- `modinfo` lists the USB IDs for one known adapter per driver (for example `usb:v0CF3p9271` for the AR9271);
- each declared firmware file is in `/lib/firmware`.

The existing `MAC80211_HWSIM` Wi-Fi test still passes. If you have one of these adapters, a short check on a real PC (join a network, run the hardware check's network step) completes it.

**Risk.** These drivers are mainline and long-standing. AR9271 and MT7612U are the usual recommendation for Linux, so the main risk is size, which is small.

## 6. Faster first Luau type check (stage B)

> **As built.**
> - The compiled `WebAssembly.Module` was already shared, so "compile once" needed no change.
> - The new parts:
>   - a checker per kind;
>   - a `warm` message;
>   - replies dispatched by id: the old code replaced the Worker's `onmessage` on each check, so two checks at once
>     could mix up their answers;
>   - an idle end after 10 minutes.
> - Tests:
>   - `luau_t.js` 29/29, with a warm Pilot.lua check in 7 ms against a 1.1 s load on the host;
>   - `blz_t.js` 14/14 (Blitzen's picker, and Code Editor with a Roblox `.LUAU` file).

**Now** (`desktop/luau30.js`). The type checker runs in one Web Worker (`LUAU.checker`), kept between checks.
- The first check of a kind loads `luau.wasm` (4.3 MB), then defines that kind's types: Roblox-lite and its
  `Enum` for `roblox`; those plus `pilot.d.luau` for `pilot`. The first check is allowed 120 s, later ones 20 s.
- Changing kind (a Pilot.lua check after a Roblox one, or back) loads and defines everything again, because the
  Worker holds one kind at a time (`checkerKind`).
- On a slow computer, the first Pilot.lua check in Blitzen or Code Editor takes up to two minutes, and the
  program says "(not type-checked: …)" if the time runs out.

**Change.**
- **One Worker per kind** (`LUAU.checkers[kind]`), each kept once loaded. That costs about 30 MB of memory per
  kind in use. Switching kinds no longer starts over.
- **Warm-up.** The checker for a kind is started in the background, after the page is idle (`requestIdleCallback`,
  at most one at a time), when:
  - Blitzen opens with Luau or Pilot.lua chosen, or a pack for them is opened;
  - Code Editor opens a `.LUAU` file (Luau, or Pilot.lua when the file uses `GetPart`).

  It defines the types with an empty program, so the first real check only checks.
- **Compile once.** `luauModule()` keeps the compiled `WebAssembly.Module` and gives it to each Worker, so
  `luau.wasm` is compiled once per session, not once per kind.
- **Waiting.** While a warm-up is still running, a check waits for it (with the 120 s limit counted from the
  warm-up's start) rather than starting a second load.

**Test.**
- `shell2/luau_t.js` gets three timings: a warm-up made at idle, then a Pilot.lua check (expected under 3 s on the
  host); a Roblox check after it; and a Pilot.lua check again, with no reload (the Worker's `define` count stays 1
  per kind).
- `luau_pw.js` (Code Editor): opening a `.LUAU` file starts the warm-up.

## 7. Copy folders from a USB drive or memory card (stage B)

> **As built.**
> - rudolfd already had `fs/tree` (every file below a folder, in one call), so no rudolfd change.
> - The copy goes to Downloads, or to C: when there is no Downloads folder; the notice says which.
> - Test: `files_t.js` 8/8.

**Now** (`desktop/files25.js`, `remoteImport`).
- A plugged-in drive (USB stick, memory card, external disk, under `/media`) opens in Files as a remote place.
- *Copy into Downloads* takes single files only: a folder gives "Copy folders one file at a time for now." Each
  file has a 256 MB limit.
- Rudolf OS does not mount phones (devplug lists them as "Phone or camera"), so this item is about drives. The old
  "from a phone or another computer" wording was wrong.

**Change.**
- A selected folder is walked through rudolfd's `fs/list` and copied with its subfolders into Downloads. The folder
  keeps its name, made unique as for files.
- Before copying, the walk counts the files and adds up their sizes. It stops early if the copy would not fit
  (`navigator.storage.estimate()`, leaving 10 % free) or holds more than 5 000 files, and says which.
- A progress line in the status bar shows "Copying 120 of 431 files (34 MB)…", with Cancel. A cancelled copy keeps
  what was copied so far and says so.
- File names follow the same rules as single files (upper case, `\ : * ? " < > |` replaced). Files over 256 MB are
  left out and listed at the end, as are files that cannot be read.
- One `fsSave()` at the end, not one per file.

**Test.** `shell2/files_t.js` (new) uses the stand-in rudolfd with a drive holding a tree three folders deep, 40
files, one file over 256 MB and one unreadable file. It checks:
- the copy, with the folder layout, names and content;
- the two files left out, and listed;
- Cancel halfway;
- the "does not fit" refusal, with a small storage estimate.

## 8. Hardware check: more steps, and the webcam driver (stage C)

**Now** (`desktop/hwcheck30.js`). Six steps: sound, microphone, network, USB, graphics, sleep. Two gaps:
- **No webcam driver.** The kernel has no UVC driver (`kconfig.sh` has no `MEDIA_SUPPORT` and no
  `USB_VIDEO_CLASS`), so no webcam works in Rudolf OS, built-in laptop cameras included. Devices lists one as
  "Camera" and nothing can use it.
- **No battery health.** rudolfd's battery reading (`/sys/class/power_supply`) gives only `capacity`, `status`
  and `online`.

**Change.**
- **Kernel.** Add `MEDIA_SUPPORT`, `MEDIA_USB_SUPPORT`, `MEDIA_CAMERA_SUPPORT`, `VIDEO_DEV` and `USB_VIDEO_CLASS`
  as modules (uvcvideo needs no firmware), so Chromium's camera access (`getUserMedia`) works. Webcams then work
  in web pages and Linux apps too: a CHANGELOG line of its own.
- **rudolfd.** `GET battery` also reports `energy_full` and `energy_full_design` (or the `charge_*` pair on
  batteries that report charge), `cycle_count` when there is one, and the maker and model.
- **Five new steps**, after graphics and before sleep. Each can be skipped, and each result goes into the system
  report as the existing ones do:
  1. **Keyboard.** A keyboard drawn on screen (the chosen layout from Settings › Keyboard). Each key lights when
     pressed; "Every key works" or "These did not answer: …" after the user says they are done. The function and
     Rudolf keys that rudolf-display takes for itself (Ctrl+W/T/N/Q) are shown as reserved, not as failures.
  2. **Touchpad or mouse.** A box to draw in: the pointer trail, the left and right buttons, and scrolling with the
     wheel or two fingers. Each of the four lights when seen.
  3. **Camera.** Lists the cameras (`enumerateDevices`) and shows the first one live, with a switch for the next.
     The result is "Camera works" if frames arrive, or "No camera was found". With no camera at all the step is
     skipped by itself. Nothing is recorded or saved.
  4. **Battery.** Only on computers with a battery. Shows "Health: 87 % of its design capacity
     (52.1 of 60.0 Wh), 312 charge cycles", with a note below 70 % ("holds noticeably less than new"). Shows
     "Charging" or "On battery" from `status`.
  5. **Screen.** Full screen, one colour at a time (black, white, red, green, blue), a tap or key for the next, to
     spot dead or stuck pixels. The user answers "Looks even" or "I saw spots".

**Test.**
- `shell2/hw_t.js` (page, stand-in rudolfd): the five steps' results with stand-in key, pointer and battery
  readings.
- `sb/t127c_t.py` (QEMU, extending `t125d_t.py`):
  - QEMU's USB webcam (`-device usb-video`) as a UVC camera, giving frames through the new driver;
  - keyboard and tablet input sent through QMP;
  - a battery from QEMU's `acpi` test battery, if the build has one, or else the page test only.

## 9. Make a stick or Install from the downloaded parts (stage C)

**Now.** disc-1.2.x comes as 18 parts (GitHub's file size limit). Before *Make a Rudolf OS USB stick* or *Install
from* can use it, the user has to join the parts with the commands in `JOIN-PARTS.TXT`, and Rudolf OS has no way
to do that from the desktop. rudolfd's `install/images` lists only whole `.iso` files in Downloads.

**Change.**
- `install/images` also lists **sets of parts**: `NAME.iso.part00` … `partNN` with no gaps, and `NAME.iso.sha256`
  next to them. Each set shows as "rudolf-os-1.2.7.iso (18 parts, not joined)".
- Choosing one runs a job, `install/join`:
  - check the free space (the image size plus 64 MB);
  - join the parts in order into `Downloads/NAME.iso.joining`;
  - compare its SHA-256 with the `.sha256` file;
  - rename it to `NAME.iso`.

  The job log shows progress, and a damaged or missing part is named ("part 07 does not match: download it
  again") using the same checks as `join.ps1`. The image then goes on, as today, to the Make a stick or Install
  from step that was chosen. That step still checks the image's signed release list itself.
- After a successful join, the dialog offers to delete the parts (548 MB).

**Test.**
- `shell2/stick_t.js`: the set is listed and offered; a missing part is reported.
- `sb/t127c_t.py` (QEMU): the real disc-1.2.6 parts (or a test disc split the same way) in Downloads are joined,
  checked, and a stick is made from the result, then started.
- A part with one byte changed is named, and nothing is written to the stick.

## Release steps (each stage)
1. Branch `rudolf-1.2.6-a`, `rudolf-1.2.7-b`, `rudolf-1.2.7-c`; implement, host tests, local release build from the
   published beta.
2. Test disc (debug, test keys) and the stage's QEMU tests.
3. Pull request; after merge, build to beta and check the published files.
4. Before promoting: run the regression suites and the stage tests on that exact beta serial. If a scheduled build
   replaced it, test again.
5. Promote to stable, then make the disc and check it: `disc-1.2.6` from `disc-1.2.5` (done), and after stage C
   `disc-1.2.7` from `disc-1.2.6`. The check of the disc's own list must pass with no skipped file.

## Risks
- **Memory (item 6):** a Worker per kind holds about 30 MB each. Only kinds actually used are started, and a
  checker idle for 10 minutes is ended.
- **Camera privacy (item 8):** the page already grants media access without asking (`--use-fake-ui-for-media-stream`
  in the session). With a working webcam that becomes real, so Settings › Privacy gets a "Camera" switch.
  Web pages that are not part of Rudolf OS ask first, through Chromium's own permission prompt for those sites
  (the session flag stays for the desktop page only, if Chromium allows it per origin; otherwise the switch is
  the control). To be settled in stage C before the driver is enabled.
- **Disk space (item 9):** joining needs the image's size again in free space. It is checked first, and the parts
  can be deleted after.
- **The init change runs on every start.** The check code only runs with `rudolf.check=1`, but a syntax error in
  `init` would stop every start. Mitigation: `sh -n` on the host, and every QEMU test starts the new init.
- **The `devplug` busy mark left behind** (a crash while writing) would stop that disk from auto-mounting until the
  next start. `/run` is cleared at start, and the trap removes the mark on any exit.
- **Unmounting a stick the user is using:** only `/media/` mounts of the chosen disk are touched, and only after
  the two confirmations the page already asks for.
