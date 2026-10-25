# Rudolf OS 1.2.6 update plan

1.2.6 starts with fixes for what came up after 1.2.5 went to stable (serial 2026102311): two bugs a user meets
in normal use, and a release-notes slip. These go first, in **stage A**, so they can reach stable on their own
before any new feature. New features follow in later stages once they are chosen (see "Features: to choose").
Version: `iso/VERSION` 1.2.6, `RVER` 1.2.6.

| # | Item | Kind | Size | Stage |
|---|------|------|------|-------|
| 1 | "Check this disc" reports every disc as damaged | bug | small | A |
| 2 | Make a Rudolf OS USB stick refuses a stick that is already formatted | bug | small | A |
| 3 | CHANGELOG 1.2.5: date and component versions | bug | tiny | A |
| 4 | Tests that would have caught 1 and 2 | tests | small | A |
| 5 | More USB Wi-Fi adapters: Atheros AR9271, MediaTek MT7601U, MT7610U and MT7612U | feature | small | B |
| – | Further features (to choose) | feature | – | B and on |

Stage A changes the disc writer, the start-up script (initramfs) and one system script. That means it needs a
release build and the Secure Boot tests, not only page tests. Like every stage, it has one pull request, is built
to beta, and is tested in QEMU. Stage A can be promoted to stable as 1.2.6 by itself if the features take longer.

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

## Features: to choose

These came up while building 1.2.5. They are candidates, not commitments: pick the ones for 1.2.6, and add others.

| Candidate | What | Size |
|-----------|------|------|
| Faster first Luau check | Keep the type checker's definitions loaded between checks, and warm it in the background when Blitzen opens. At the moment the first Pilot.lua or Roblox check can take up to two minutes on a slow computer. | small |
| Copy folders from a phone or another computer | Files › Import from another device only takes single files ("Copy folders one file at a time for now"). | small |
| Hardware check: more steps | Keyboard (press every key), touchpad or mouse, camera, battery health (design vs full charge), screen dead-pixel colours. | medium |
| Make a stick from a downloaded image, without joining parts | Pick the 18 `.part` files from Downloads and write them in order, checking the `.sha256`. | small |

## Release steps (each stage)
1. Branch `rudolf-1.2.6-<stage>`, implement, host tests, local release build from the published beta.
2. Test disc (debug, test keys) and the stage's QEMU tests.
3. Pull request; after merge, build to beta and check the published files.
4. Before promoting: run the regression suites and the stage tests on that exact beta serial. If a scheduled build
   replaced it, test again.
5. Promote to stable; disc `disc-1.2.6` from `disc-1.2.5`, then check it. The check of the disc's own list must now
   pass with no skipped file.

## Risks
- **The init change runs on every start.** The check code only runs with `rudolf.check=1`, but a syntax error in
  `init` would stop every start. Mitigation: `sh -n` on the host, and every QEMU test starts the new init.
- **The `devplug` busy mark left behind** (a crash while writing) would stop that disk from auto-mounting until the
  next start. `/run` is cleared at start, and the trap removes the mark on any exit.
- **Unmounting a stick the user is using:** only `/media/` mounts of the chosen disk are touched, and only after
  the two confirmations the page already asks for.
