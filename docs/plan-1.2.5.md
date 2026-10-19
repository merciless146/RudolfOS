# Rudolf OS 1.2.5 plan

The additions suggested after the first real-PC test of 1.2.3 (ASUS ROG Strix G15DK, Ryzen 7 5800X, RTX 3080).
1.2.4 fixed what that test found (keeping files on a stick, the Secure Boot key file name, the join and
write steps, graphics and network reporting, AMD microcode). 1.2.5 adds new features.

## 1. Make a Rudolf OS USB stick, from inside Rudolf OS
Settings › Storage and backup › **Make a Rudolf OS USB stick…**
- Source: the running Rudolf OS (the image on the drive it started from) or a downloaded `rudolf-os-*.iso`
  in Downloads. The same signature check as "Install from" (`rudolf-update verify-manifest`).
- Target: any removable drive of 2 GB or more that is not the running drive, chosen from a list with model and
  size; an administrator confirms by typing the drive's size.
- Writes the image, then adds RUDOLFDATA in the rest of the stick (keepfiles.sh's partition code, shared with
  install.sh), so the new stick keeps files from its first start. Option: copy this session's files onto it.
- Progress and a final read-back check of the image (sha256 against the release list's sizes and hashes).
- Removes the need for Windows, Rufus and the join step for anyone who already runs Rudolf OS.
- Test: QEMU with two USB sticks; start from the new one under Secure Boot (OVMF) and with BIOS.

## 2. Hardware check in Task Manager
Task Manager › Hardware › **Check this computer…**: a guided test of about a minute, one step per screen.
- Sound: a tone on each output (speakers, headphones, HDMI/DisplayPort); "Did you hear it?"
- Microphone: a 3-second level meter.
- Network: wired link speed (with the 1.2.4 cable hint), Wi-Fi scan, a download test to the update server.
- USB: "plug in a USB stick" and watch it appear.
- Graphics: a WebGL scene with frames per second; renderer (graphics card or processor) and glamor state.
- Sleep: optional suspend for 10 seconds and wake (laptops); checks screen, network and sound come back.
- Result: a pass/fail list that "Report a problem" attaches to the system report (no personal data).
- Test: host harness (tm_t.js) for the flow; QEMU for the sound and USB steps.

## 3. Start Windows from the Rudolf OS boot menu
- UEFI: systemd-boot lists "Windows Boot Manager" when it finds `\EFI\Microsoft\Boot\bootmgfw.efi` on any
  ESP (`auto-entries yes`, or a generated entry), after the Rudolf OS entries. Works with Secure Boot on
  (bootmgfw is signed by Microsoft; systemd-boot chain-loads it through the firmware).
- BIOS: "Boot from hard disk" already exists in Rudolf Boot.
- Settings › Update and recovery › Boot menu: default entry and timeout.
- Test: QEMU with a Windows-like ESP (a signed stand-in EFI program) under Secure Boot.

## 4. Linux apps over Wi-Fi
- The App Store allows large downloads (Sober, Steam) over Wi-Fi with a size warning and the free space
  shown, instead of requiring a network cable. Metered networks (marked in Settings › Network) still ask.
- Resume: Flatpak downloads that stop are resumed on the next try.
- Test: QEMU with the virtual Wi-Fi (mac80211_hwsim) used by the Wi-Fi tests.

## 5. Games page in the App Store
- A Games tab: Roblox (Sober), Steam (Flatpak), Minecraft launchers (Prism), RetroArch and the built-in
  emulators, each labelled "needs the graphics card" or "runs anywhere", with download size.
- Uses Task Manager's renderer check: on a computer drawing with the processor, a note explains which
  games will be slow and why.
- Test: host harness for the page; QEMU for one install.

## 6. Backup to a second USB stick
- Settings › Storage and backup › Back up everything: "To a USB stick" writes the backup (accounts' files
  still encrypted, settings, apps list) straight onto a plugged-in drive, as one file with a checksum.
- First start: "Restore from a backup" finds backups on plugged-in drives.
- Test: QEMU with a second virtual USB stick; back up, factory reset, restore.

## Order and size
1, 2 and 3 are the larger pieces (each about a day with tests); 4, 5 and 6 are smaller. Release 1.2.5 when
1–3 are done; 4–6 can follow as 1.2.6 if needed. No firmware changes: the update is small.
