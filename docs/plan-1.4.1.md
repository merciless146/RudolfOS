# Rudolf OS 1.4.1 plan: start from the internal drive

1.4.1 follows 1.4.0 (stable serial 2026102713). Its one goal: a computer where Rudolf OS is installed starts from its
own drive, without a USB stick. The ASUS Vivobook Go (E1404FA) cannot today, so it starts through an old 1.1 stick,
and Secure Boot cannot be used (the stick's 1.1 start files are not signed).

## What the development link showed (2026-10-27, commands 000001-000006)

- The firmware's boot entries: Boot0000 "Windows Boot Manager" and Boot0002 "Ubuntu" (GPT partitions that no longer
  exist), Boot0003 "UEFI:  USB" (the stick), and none for the internal drive. The firmware did not add one by itself.
- The stick starts as a **CD**: Boot0003's device path ends in a CD-ROM node (El Torito entry 1). The firmware has
  never used the image's partition table, on the stick either.
- The internal drive's table is the disc image's hybrid MBR, as Install here copies it: partition 1 (type 0x17, the
  ISO 9660 file system) **starts at sector 0** and **contains** partition 2 (type 0xEF, the EFI system partition,
  sectors 0x4C58-0xAC58); partition 3 is RUDOLFDATA. By the UEFI rules for MBR disks (partitions must not overlap),
  such a table is invalid, and the firmware treats the disk as having no partitions.
- A boot entry Rudolf OS wrote (Boot0004: `HD(2,MBR,0x6ACC513D,0x4C58,0x6000)/\EFI\BOOT\BOOTX64.EFI`), used once
  with BootNext and the stick unplugged: the firmware kept the entry but went to its setup screen. Consistent with
  the above: it cannot find partition 2.
- The internal drive and the stick have the same MBR disk signature (0x6ACC513D, from the image), so a boot entry
  naming the drive by signature could also match the stick.
- The 1.3.1 fix works on the laptop: started through the stick (`bootvia=usb`), it runs 1.4.0 from the internal
  drive (`nvme0n1p3`, slot b).

| # | Item | Kind | Stage |
|---|------|------|-------|
| 1 | Confirm on the laptop which partition table its firmware starts from (before any code) | check | A |
| 2 | Installed drives get that table: the EFI system partition and RUDOLFDATA only, no partition over the ISO | bug | B |
| 3 | The start-up finds the system on such a drive (the ISO through a read-only loop over the whole disk) | bug | B |
| 4 | Installed drives get a disk signature of their own | bug | B |
| 5 | Install here and Get new boot files add a "Rudolf OS" boot entry for the drive, first in the boot order | feature | B |
| 6 | Tests in QEMU; the laptop converted with Get new boot files, then started without the stick | tests | B, C |
| 7 | Then Secure Boot on the laptop: Prepare Secure Boot, MokManager, Secure Boot on | check | C |

---

## Stage A: confirm the cause on the laptop first (through the development link)

Nothing is built until the laptop has shown which table its firmware accepts. Each step is a link command the owner
allows; each is undone afterwards.

1. **Valid MBR (layout A).** Back up the first sector of `nvme0n1` to RUDOLFDATA. Clear partition entry 1 (bytes
   446-461) and give the disk its own signature (bytes 440-443); entries 2 (EFI) and 3 (RUDOLFDATA) stay. Point
   Boot0004 at the new signature, set BootNext, unplug the stick, restart.
   - The firmware starts the drive: layout A works. The drive's own start files are still 1.1's, and that start-up
     finds no partition 1, mounts the whole disk and then cannot mount RUDOLFDATA. So it runs live: a 1.1 desktop
     without your files. That is expected, and proof enough. Plug the stick back in and restart; the stick starts as
     before (its start-up takes RUDOLFDATA, and 1.3.1's update kernel keeps the stick's image when the drive has no
     partition 1).
   - The firmware goes to its setup screen again: layout A is not enough; go to step 2.
   - Afterwards, restore the backed-up sector (unless the drive is to stay that way until 1.4.1, see below).
2. **GPT (layout B)**, only if A failed: a protective MBR and a GPT with the EFI system partition and RUDOLFDATA at
   the same sectors (back up the first and last 34 sectors first). Same test, same restore.

The rest of the plan uses the layout that works, called "the installed layout" below.

## Stage B: implementation

**2. The installed layout.** Written where an internal drive gets Rudolf OS's start files:
- `install.sh` (Install here, and Replace the boot files only), after writing the image and RUDOLFDATA's entry;
- the start-up's "new boot files" writer (`initramfs/init`), after writing the boot image in front of RUDOLFDATA;
  this is how an installed drive gets it without being reinstalled (Get new boot files);
- not `mkstick.sh`: sticks start as a CD, and they keep the image's hybrid table, so they work on every firmware.

**3. The start-up** (`initramfs/init`): a drive without partition 1 but with RUDOLFDATA as partition 3 holds the
system in its first sectors (the ISO 9660 file system starts at sector 0). Mounting the whole disk would claim it
and lock out RUDOLFDATA, so the ISO is mounted read-only through a loop device over the whole disk (a loop device
does not claim the disk). One `mount_media` function for every place that mounts media: `find_media`, the slot
kernel's `find_media_given`, the kexec failure path. `MEDIA` stays the disk (`/dev/nvme0n1`), so `disk_of`, `part3`,
`/etc/rudolf/media` and `rudolf.mediadev=` keep meaning "the start drive".
- **Older start-ups.** An update kernel from before 1.4.1 (a rollback to the other slot) would mount a whole disk named
  in `rudolf.mediadev=` directly and lock out RUDOLFDATA. So 1.4.1 passes `rudolf.mediadev=` only for a partition;
  for a whole disk, the older kernel finds no media, and takes RUDOLFDATA by `rudolf.persist=UUID` (since 1.3.0).
  The test covers a rollback to a 1.4.0 slot.
- Everything that reads `/etc/rudolf/media` is checked for a whole disk: `rudolfd` (where it started from),
  `bootfiles.sh`, `install.sh`, `mkstick.sh` (Make a stick from "this Rudolf OS"), `keepfiles.sh`.

**4. Disk signature.** A random signature (bytes 440-443) when the installed layout is written, so boot entries name
this drive and not a stick made from the same image.

**5. Boot entry.** After the installed layout is written: a "Rudolf OS" entry (`HD(2,...)/\EFI\BOOT\BOOTX64.EFI`,
shim, so it also works with Secure Boot), first in BootOrder; an older Rudolf OS entry for the same drive is
replaced, other entries are left alone. rudolfd already writes EFI variables (Restart into Windows, Prepare Secure
Boot); this uses the same code. Without EFI (BIOS starts) nothing is written.

## Tests

- QEMU's firmware (OVMF) starts the hybrid table anyway, so it cannot show the laptop's problem. The QEMU tests
  check that the installed layout is written correctly and starts: a new `t141` (Install here, and Get new boot
  files on a drive installed from 1.2.0 and on one installed from 1.4.0: the table, the signature, the boot entry
  and BootOrder in the firmware's variables, a start from the drive by that entry, files kept, an update and a
  rollback to the older slot); and the usual set (t125, t128b, t130, t131, t140, sb_t, v120).
- The real check is stage C.

## Stage C: on the laptop (the owner, with the development link)

1. Update to 1.4.1 through the stick (as now).
2. Download `rudolf-os-1.4.1-boot.iso` and its `.sha256`; Settings › Security › Secure Boot › Get new boot files.
   The next start writes them, the installed layout and the boot entry, then restarts.
3. Unplug the stick before that restart. The laptop should start Rudolf OS 1.4.1 from its own drive, with your
   files. If it does not: plug the stick back in, start through it, and the link shows why.
4. Then Prepare Secure Boot, confirm the key in MokManager, and switch Secure Boot on in the firmware.
5. The results go into this plan's as-built notes; the leftover Boot0004 entry from stage A is removed.

## Release steps
1. Stage A with the owner (the link), before code.
2. Stage B on the branch `rudolf-1.4.1`: implement, QEMU tests, local test release; a pull request with the results.
3. After the merge: a beta, checked; the owner promotes 1.4.1; disc-1.4.1 from disc-1.3.1 (the boot image is what
   the laptop needs).
4. Stage C.
