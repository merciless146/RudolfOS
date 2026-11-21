# Rudolf OS 1.6.1: what was moved out of 1.6.0

Asked for on 2026-11-15: "What items have been pushed beyond 1.6.0? Can they all be added to a plan for 1.6.1?" This
plan gathers every item that docs/plan-1.6.0.md moved past 1.6.0, with where it came from, and (Parts 18 to 21, 2026-11-20)
what was found since and not yet in a plan. Nothing here is built yet.
The owner's rule for the native desktop holds for every part: "Rudolf OS on the native desktop should appear and
function exactly as it does in Chromium mode."

## Parts

### 1. Several monitors, on the three workspaces (from 1.6.0 Part 12, item 7)
Decided by the owner on 2026-11-15: "Rudolf OS has three workspaces that can be selected in the bottom right of the
taskbar. These workspaces will be the basis for Rudolf being able to support up to three monitors/displays, as well
as having three panels for the mobile build that can be selected by swiping left/right similar to how Android phones
work." Built after the 1.6.0 beta.
- **Up to three displays,** each showing one of the three workspaces, with its own wallpaper and taskbar.
- **A window moved to another workspace moves to that display;** the taskbar on each display lists that workspace's
  windows (as the taskbar lists the shown workspace's windows today).
- **One display: as today.** The three workspaces are switched from the taskbar, Super+Ctrl+arrows and the menus.
- **Standard and Native alike:** rudolf-display gives Chromium one output today, so Standard needs the change too
  (decided by the owner, so the rule holds); Native follows RandR's outputs.
- **Plugged in and out:** a display unplugged takes its workspace back onto the remaining ones, with its windows; Settings
  › Display lists the displays and which workspace each shows.
- **Tests:** Xvfb with several screens (or Xephyr / RandR's virtual outputs) for both engines, compared with same_t.sh;
  on the Vivobook with an HDMI monitor (owner).

### The phone, split in two (owner, 2026-11-15)
Decided on 2026-11-15, after asking whether the first real phone would be easier in 2.0 than in 1.6: "Make the sensible
split." 2.0 brings the two things a phone people carry needs most, Wayland (touch gestures, rotation, scaling, the
on-screen keyboard, power-saving drawing) and native apps (a fraction of the memory, so a day on the battery), but not
the hardest part: making Rudolf OS run on one particular phone. So the phone comes in two stages:
- **Stage 1, here in 1.6.1: the hardware.** Rudolf OS boots on one ARM64 phone and its hardware works: the modem's
  data, texts and voice calls, emergency calls checked, the call's sound, sleep with the modem waking it, charging and
  the battery. It runs today's native desktop (X11) as it is: proving the hardware, not the phone's interface (Parts 2,
  4, 6 and 7 below).
- **Stage 2, in 2.0: the phone build.** The phone's own interface on Wayland, the three swipe panels, and native apps
  laid out for touch, on the hardware stage 1 proved (docs/plan-2.0.md, "The phone build").
Nothing on a phone is flashed or unlocked without the owner's decision.

### 2. The mobile build's three panels (from the same decision): the groundwork only
The phone's home screen is the three workspaces as three panels, swiped left and right as on Android. In 1.6.1 only
the groundwork: the workspaces' code from Part 1 written so a panel is a workspace shown full screen, and a horizontal
swipe on a touch screen switching workspaces on both desktops (tested with a touch screen in QEMU and on the phone of
Part 4). The phone's home screen itself, with its status bar, notification shade and app grid, is 2.0's phone build
(stage 2).

### 3. Lite, refined as a compatibility option (from 1.6.0 Part 15)
Decided by the owner on 2026-11-15: Native replaces Lite as the desktop; Lite stays in Settings › System health ›
Desktop engine as a compatibility option, "to be worked on and refined later". Left from 1.5.8 and 1.5.9:
- the desktop's menus and windows behind Oracle's tab pages (the covered tab's still picture while a menu is open);
- stutter in Oracle's tabs;
- Google sign-in for GitHub (symptoms to be described first);
- GPU drawing for Lite measured;
- Oracle's WebKit tabs: YouTube, Google sign-in with FedCM, a download, full screen, memory with 5 tabs;
- the lock and sign-in screen settings and the shutting-down screen;
- the dynamic backgrounds' moving parts.

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

### 9. USB printers and the first ARM64 release (moved from 1.6.0 on 2026-11-16)
Decided by the owner on 2026-11-16: "USB printers and ARM64 can be delayed until 1.6.1."
**USB printers back in 1.6.0 (owner, 2026-11-17):** "Skip the test and just add it. Users can still print if the USB
printing has issues and we can refine it in 1.6.1 with the data that comes back if it does." The `usbprint` extension
is published for 1.6.0's beta by extensions.yml, untested on a real printer; 1.6.1 refines it from reports (the
self-test's usb-printer line, /run/rudolf/usbprint.log in a system report) and the HP desktop user's printer.
- **USB printers (1.6.0 Part 8, as written there):** IPP over USB (ipp-usb) and plain USB printers reading PDF or
  PostScript, as the first signed system extension (`usbprint`, Part 13 of 1.6.0); extensions.yml's first run to
  publish it for the release's base ABI; Settings › Devices; then the HP desktop user's printer. 1.6.0 prints to network
  printers (IPP Everywhere, AirPrint) and to PDF.
- **ARM64 (1.6.0 Part 7 and its stage B item 4):** the first ARM64 release, after one ARM64 computer that starts with
  UEFI (or the first phone, Part 4: the PinePhone Pro through Tow-Boot), its own update lists (beta/arm64/,
  stable/arm64/), the update and recovery checks on it.

### 10. Oracle: find, closed tabs, reader mode and blocking (added on 2026-11-16)
docs/plan-oracle.md, asked for by the owner on 2026-11-16. Suggested for 1.6.1: item 1 (find in page), item 2 (reopen a
closed tab, restore the session) and item 7 (reader mode); item 5 (ad and tracker blocking) here or in 2.0. Item 4
(website passwords) and item 6 (extensions from the Library) are suggested for 2.0 (docs/plan-2.0.md); item 6's
stage 1 could start here. The owner decides.

**Built (the owner, 2026-11-20): "Goes To:" in the status line.** "Mousing over a hyperlink or an executable
button/ad/clickable item ... should show the target URL at the bottom left of the screen where 'Done' shows. The displayed
phrase should be 'Goes To: (insert URL here)'." Standard and Native: iso/shell/shell.js HOVER_JS in every frame (links,
image-map areas, SVG links, a form's submit button; keyboard focus too; javascript: links not shown) sends tabev `hover`;
Lite: rudolf-webkit's `mouse-target-changed` (WebKit's own hit test); Oracle's own pages: ORACLE_HOVER_JS posts to the
window. The status line goes back to what it said, unless something new came meanwhile. Not possible to know before a
click: where a script-only button (no link, no form) will go. Test orahover61_t.js; workflow oracle-hover-check.yml.

**Built (the owner, 2026-11-20): the Downloads page's progress bar and Remove** (desktop/dlrm61.js). A file arriving: how
much so far and how fast, and with a size known the percentage and time left (Chromium's own downloads do not tell their
size: the bar then moves without a percentage); being checked: a bar. Remove beside Open: in Downloads → the Trash;
otherwise "File removed from list" with "⚡ Ask Blitzen" (the notification's own Blitzen button, kept in the centre; drawn
by rudolf-native too). Test dlpage61_t.js; workflow downloads-page-check.yml. Left: the size of Chromium's downloads
(rudolf-webkit's WebKitDownload knows it: Lite could send it).

**Built (the owner, 2026-11-20): large ZIPs in Downloads, listed and extracted** (desktop/dlzip61.js, iso/sys/rudolfd-dlzip.inc,
/usr/lib/rudolf/dlzip.sh). "Archive Manager would list what's inside a large ZIP without loading it into memory, and Extract
here would unpack it next to the ZIP, with a progress window, using the same unpacking Add games… already uses." rudolfd
`dl/zip` lists a ZIP kept on the disk (`unzip -l`, read as the desktop user; 20,000 entries at most), `dl/unzip` is the job:
the free space checked first, unpacked as the desktop user (su, the paths as arguments) into a hidden folder in the account's
folder, ".." and absolute entries refuse the ZIP, links removed, each file moved flat into the account's folder under a free
name; `dl/unzipped` is its whole result. The page makes a folder named after the ZIP beside it, its sub-folders as in the
ZIP, each file a reference (antivirus as dlAdopt), and dl27.js dlScope now leaves references in sub-folders of Downloads
where they are. While it runs, dl/home says "unzip" so no page adopts the files (Native's Explorer and Oracle are two pages).
Tests: iso/qt/host/dlzip161_t.sh (Info-ZIP and BusyBox), dlzip161api_t.sh (rudolfd), iso/qt/shell2/dlzip61_t.js (the page);
workflow dlzip-check.yml. Left: a ZIP in a home folder kept on the disk (1.5.6 accounts with their files on RUDOLFDATA,
whose large downloads home56.js moves into the home) is not listed yet: it keeps the old message; BusyBox's unzip drops ".."
and a leading "/" from names itself, so with it such entries land inside the folder rather than refusing the ZIP.

### 11. rudolfd's root work split into helpers (added on 2026-11-16; from docs/plan-1.5.9.md 15.5)

**Shipped in 1.6.0 (owner, 2026-11-17: "I do want to keep 446 and 447 in 1.6.0"):** rudolf-rootd with the drives' and
the updates' helpers (#446, #447). The other areas and rudolfd's drop to uid 1000 stay here for 1.6.1.
Asked for on 2026-11-16: "Add items 5, 8, 9, and 10 to the plan and schedule the tasks accordingly." rudolfd
(iso/sys/rudolfd.c and its .inc files, about 7,300 lines) runs as root for its whole life while it parses every HTTP
request of the desktop's page and API: a parsing bug there is a bug with root.
- **The aim:** every operation that needs root moves into a small helper program that rudolfd runs with checked
  arguments; rudolfd drops to the `rudolf` user (uid 1000) once it has started and opened what it must.
- **The areas** (one PR each, in this order, each keeping the API byte for byte): drives (mount, unmount, eject,
  format, the encrypted home's unlock), updates (rudolf-update's slots, roll back, tostable: much is already a separate
  program), accounts (the per-account Linux users of 1.6.0 Part 10: useradd, chown, the keyring), power (restart, shut
  down, sleep, brightness), network (Wi-Fi, the firewall, the hostname), packages (rpk, Flatpak's system parts), and
  whatever a sweep for root-only calls finds (`/sys` writes, `/dev` access, `mount`, `kill` of others' processes).
- **Each helper:** one job, its arguments validated again inside it (never trusting rudolfd), callable only by rudolfd
  (the `rudolf` user, checked by SO_PEERCRED on a root-owned socket, or a setuid helper owned root:rudolf mode 4750,
  whichever the area suits), logged, and small enough to read in one sitting.
- **Never:** a weaker check than today's (the token, the peer check of 1.5.9, administrator prompts), or a change
  the page or the native desktop can see.
- **Tests:** host tests for each helper's argument checks (bad paths, `..`, another account's files); the whole API
  before and after on the real release in QEMU (iso/qt/sb/: the existing t160 checks pass unchanged); a test that
  rudolfd's process is uid 1000 after start, and that the helpers refuse callers other than rudolfd.


#### 11.1 The sweep (2026-11-17): every place rudolfd needs root
From iso/sys/rudolfd.c and its .inc files on main after #441: each `run()`, `run_out()`, `system()`, `popen()`, `exec*`
and job (`start_job`), and the calls made directly (mount, ioctl, kill, chown, setuid, writes under /sys, /proc/sys,
/etc and the EFI variables). Reads of /proc, /sys and /etc that any user may make are left out (battery, network
counters, temperatures, the CPU and memory, /etc/rudolf/media). By area, in the order they move; the helper each will
move to is a program in /usr/lib/rudolf/root (11.2). "Program" means one of Rudolf OS's own scripts or programs that
rudolfd runs as root today (they keep running as root, from the helper).

| Area | Call site (function, API) | What it does as root | Helper |
|---|---|---|---|
| Drives | `api_mount` (POST mount, umount) | devplug mount (mount(8) of a USB drive or SD card under /media); umount and rmdir of /media/DEV | **drives** (moved) |
| Drives | eject (POST eject) | devplug eject: every partition unmounted, /sys/block/DEV/device/delete | **drives** (moved) |
| Drives | stick/info | mkstick.sh --info: the stick's partitions mounted read-only for a moment | **drives** (moved) |
| Drives | install, install bootonly=1, bootfiles, mkstick, keepfiles (jobs) | install.sh, bootfiles.sh write/stage, mkstick.sh, keepfiles.sh: whole disks partitioned and written, loop mounts, RUDOLFDATA made | **drives** (moved) |
| Drives | `home_unlock`, `home_lock`, `home_key_*`, `home_policy_id` (sign-in's home/unlock, home/lock, desktop.inc) | fscrypt: FS_IOC_ADD/REMOVE_ENCRYPTION_KEY, SET_ENCRYPTION_POLICY on /persist/accounts/ID, the folder made and given to the account's user | **drives** (moved, part 2: with Oracle's /persist/oracle/ID) |
| Drives | `home_bind`, `home_unbind`, `home_debian_rebind` | bind mounts of the account's folders over /home/rudolf/…; rudolf-debian rebind | **drives** (moved, part 2) |
| Drives | `dl_store_bind`, `dl_store_dirs`, `dl_acct_dir`, `dl_home_boot` | bind mount of the downloads' store; chown of its folders to 1000 | **drives** (moved, part 2: dl_store_bind, dl_store_dirs, dl_store_tidy, dl_home_boot's binds); dl_acct_dir next |
| Drives | the vault: `vault_add_key`, `vault_key_spec_remove`, `vault_feature` (POST vault/*), vault-move (job), the Flatpak bind at unlock | fscrypt on the encrypted folder (1.5.0), tune2fs -l, mount --bind of /var/lib/flatpak, vault-move | **drives** (moved, part 2; vault.json: the drop) |
| Drives | `addpart` (rudolfd --addpart, run by the scripts) | BLKPG ioctl on a disk | stays (a separate run by root scripts, not a request) |
| Drives | `api_disks`, `api_storage` (storage-measure) | blkid of every disk (needs read access to /dev), du of the whole system | drives (read-only actions) |
| Updates | sys/check, sys/download, sys/tostable, sys/return (jobs) | rudolf-update: slots on RUDOLFDATA, the manifest's signature, the ESP | **updates** (moved) |
| Updates | sys/rollback, sys/set, sys/beta, sys/developer, sys/stable, sys/status, speedtest, boot-ok (at the desktop's first heartbeat) | rudolf-update's state in /persist/os, its settings in /persist/etc | **updates** (moved) |
| Updates | the desktop's page update (POST update, POST rollback: /persist/os/new, current, previous) | files of root's on RUDOLFDATA | **updates** (moved) |
| Updates | bootmenu, secureboot (`efivar_write`, `unimmutable`, `boot_entry_ensure`), MokNew/MokAuth | EFI variables (/sys/firmware/efi/efivars: chattr -i, writes) | updates (the boot entries, next) |
| Accounts | `au_start`, `au_install`, `au_sync`, `au_migrate`, `au_mv_one` | /etc/passwd, group, shadow written (the per-account Linux users of 1.5.9 15.3 / 1.6.0 Part 10), lchown of the moved folders | **accounts** (moved) |
| Accounts | `au_session_start`, `au_session_stop`, `au_kill_user`, `au_xaccess` | /home/acctUID bind mount, /run/user/UID made and chowned, the session D-Bus and portal started as the account's user (setuid), its processes killed, X's localuser access | **drives** (moved with the sign-in's chain, part 2); au_kill_user for a removed account (au_sync): **accounts** (moved) |
| Accounts | `api_keyring` (keyring/*) | /usr/lib/rudolf/keyring as root for the account's key files | **accounts** (moved: keyring/list and keyring/forget's su) |
| Accounts | home/* (the files API: tree, list, read, write, rename, copy, delete, recovered) | reads and writes inside /persist/accounts/ID, which is the account's user's, 0700 (root's DAC override) | accounts: a file service run as the account's own user (not root), 11.2 |
| Accounts | desk_* (desktop.inc: the page's storage, dwp_ensure, dput) | chown of the desktop's folders to desk_uid | none needed after the drop (rudolfd is that user) |
| Power | power (POST power: poweroff, reboot), `sleep_now`, `api_sleep` | reboot(2), /usr/lib/rudolf/shutdown, pkill of Chromium, umount -a -r, /sys/power/state, /sys/class/rtc/rtc0/wakealarm | power |
| Power | power/set, `main`'s power apply, brightness (backlight writes through the power script) | /usr/lib/rudolf/power (CPU governor, backlight, USB autosuspend under /sys) | power |
| Power | memory/dropcache, memory settings (zram) | /proc/sys/vm/drop_caches, /usr/lib/rudolf/memory | power |
| Power | desktop/priority | /usr/lib/rudolf/desktop-priority (cgroup weights of the desktop) | power |
| Network | `api_wifi` (wifi/*: scan, connect (job), forget, radios, power) | wifi.sh (wpa_supplicant, wpa_cli, iw, rfkill), the root-only request file | **network** (moved) |
| Network | net/dhcp, net/static, dns, `api_net`'s wpa_cli status | ip link/addr/route, udhcpc, /etc/resolv.conf | **network** (moved) |
| Network | firewall (GET/POST), remote access's firewall holes, health_tick's firewall status | /usr/lib/rudolf/firewall (nft) | **network** (moved) |
| Network | hotspot, netplug, mobile/* (rudolfd-mobile.inc: modem, SMS, airplane mode) | hostapd, dnsmasq, ModemManager through /usr/lib/rudolf/hotspot, netplug, modem | **network** (moved: hotspot, netplug); modem next |
| Network | tor (`tor_start`, `tor_kill`), torwin (`torwin_start`) | Tor started as its own user (setuid in the child), its folder made and chowned; Chromium for Tor windows as the desktop's user with a cgroup write | network |
| Network | bt/* (bluetooth), tb/* (thunderbolt) | /usr/lib/rudolf/bluetooth, thunderbolt (sysfs authorize) | **devices** (moved) |
| Network | remote/* (`remote_start`, `remote_stop`) | rudolf-remote started (as root today), its certificate and settings in /persist/etc | network |
| Network | time sync (`time_tick`, POST time, time/sync) | settimeofday, ntpd, hwclock -w | **power** (moved, the clock) |
| Packages | pkg/* (rpk refresh, update, install, remove, install-file: jobs), pkg/exec (`api_pkg_exec`: setuid to the account's user) | rpk on the system's package folders; a Library program run as the account's user | packages |
| Packages | apps/* (`api_apps`: Flathub, install, update, remove, appextra), catalog | flatpak --system, /usr/lib/rudolf/appextra, RUN/apps chowned | packages |
| Packages | sysext/* (`api_sysext`) | rpk sysext (signed system extensions mounted over /usr) | packages |
| Packages | debian/* (`api_debian`, `spawn_debian`: setup, update, wine, deb, run --root), ai/* python and C++ | rudolf-debian (a container: namespaces, mounts) as root | packages (rudolf-debian is already a program: through the helper) |
| Packages | games/* (games-import), ai/* (`api_ai`: rudolf-ai, the model's folder), print/* (`api_print`: rudolf-print, usbprint) | folders chowned to 1000; rudolf-ai and rudolf-print as root | packages (most need no root: they run as the desktop's user after the drop) |
| Display | display (POST: /etc/rudolf-display), screen/* (screenctl), layout, pointer | /etc writes, xrandr through screenctl, setxkbmap through layout.sh | **screen** (moved: display, shell, screen/*, keyboard); pointer is RUN's (the drop) |
| Display | screenshot (`grab_x`, `grab_fb`), clipboard (`api_clipboard`: su rudolf) | /dev/fb0; su to the desktop's user | none after the drop (video group; rudolfd is that user) |
| Display | native/*, web/*, shell (POST shell: /etc/rudolf-shell), restart-browser, desktop/engine | the native desktop, Chromium windows started as the desktop's user (su/setuid), pkill of the kiosk browser, /etc and /persist/etc writes | display |
| Sound | audio (/etc/asound.conf), audio/mics, mic, volume, media, sound restart, echo | /usr/lib/rudolf/sound, hotkey (PulseAudio and ALSA as the desktop's user, through su) | **sound** (moved) |
| Other | processes, proc/end (`api_proc_end`, `proc_tree_end`) | kill(2) of other users' processes (the accounts' apps), /proc of every process | other: **procs** (moved) |
| Other | service (restart sound, Wi-Fi, display), selftest, starts, system (`api_system`: dmesg, modules), log, crash_list, health_tick's dmesg | the self-test, dmesg (kernel.dmesg_restrict), /proc/modules | other: **logs** (moved: dmesg read by a helper); the self-test and service next |
| Other | devlink/* (`dl_start`, `dl_stop`) | rudolf-devlink started with its own user and environment | other |
| Other | camera, portal, term (Debian's terminals) | /usr/lib/rudolf/camera, the portal's files chowned, rudolf-debian's terminals | other |
| Other | fs/* (`api_fs`: the desktop's file browser outside the accounts' folders) | chown of new files to 1000 under the shared folders, and reading any path it allows | none after the drop (rudolfd is 1000), accounts for the accounts' folders |
| Other | rudolfd's own files: RUN (/run/rudolf, root's 0755), the health log (root's 0600), /persist/etc/* | written by rudolfd as root | the drop: RUN/d of rudolf's, the health log rudolf's 0600 (helpers append as root) |

#### 11.2 The design (2026-11-17)
- **One broker, rudolf-rootd, which starts rudolfd** (iso/sys/rudolf-rootd.c). rudolf-init runs `rudolf-rootd
  /usr/bin/rudolfd`: it makes a socket pair, starts rudolfd with one end (`RUDOLF_ROOTD_FD`; rudolfd makes it
  close-on-exec at once) and keeps the other. It listens on nothing: no socket in the file system, no port. For a
  request rudolfd makes a socket pair of its own and passes one end over that first one; rudolf-rootd then checks who
  made it (SO_PEERCRED: root, or rudolf (1000) once rudolfd drops to it; and that process's /proc/PID/exe is the program
  it started), reads one message on it (`RRQ1`, the helper's name, its arguments, each ending in a zero byte, and three
  descriptors: the helper's standard input, output and error; iso/sys/rudolf-root.h) and runs /usr/lib/rudolf/root/NAME
  with exactly those arguments (execve, never a shell), with an empty environment (only PATH), its working folder /,
  umask 022, every other descriptor closed and every signal back to its default; it answers with four bytes, the exit
  status (254 refused, 253 not started). An unknown name, a malformed message, a request from another user or another
  program is refused and said in the health log (Security).
- **Why not the socket in the file system checked by SO_PEERCRED and /proc/PID/exe** (the first idea): once rudolfd
  runs as uid 1000, any program of that user can start its own copy of /usr/bin/rudolfd with LD_PRELOAD, or trace one
  (not set-user-ID, so the user's own), and that copy's /proc/PID/exe is rudolfd too: the check would let it in. The
  descriptor handed over at rudolfd's start cannot be had that way: rudolfd is not traceable by the user (it becomes
  non-dumpable when it drops; PR_SET_DUMPABLE 0 is set explicitly in the drop's step), and nothing it starts inherits
  the descriptor. The exe check is kept as a second line (a descriptor leaked by mistake still lets nothing else in).
- **Why not set-user-ID helpers (root:rudolf 4750)**: every process of the group could run them (the desktop's browser
  is uid 1000 too), with whatever environment, descriptors, limits and working folder it chooses (the classic
  set-user-ID mistakes), and rudolfd could not set no_new_privs (it stops set-user-ID). One broker gives every area the
  same check in one place, a clean start for each helper, and leaves no_new_privs possible. No area needs set-user-ID,
  so every area uses the broker. The cost: one more small root process, which parses only fixed messages from rudolfd.
- **Each helper** is a program of its own (root:root 0700 in a folder of root's, 0755; never set-user-ID), one area,
  small enough to read in one sitting. It checks every argument again itself, whatever rudolfd checked: device names
  letters and digits only and ones the kernel has (/sys/class/block, /sys/block for whole disks); paths made canonical
  (realpath: no "..", every link followed) and kept inside their allowed root (a link out of it is refused), files
  plain (not a link) and owned by the desktop's user or the signed-in account's user (another account's file, or a
  hard link to one, is refused); account names from the account list (/persist/etc/rudolf/account-users), never from
  rudolfd's word alone; keys and passwords on standard input, never as arguments (anyone can read a process's
  arguments); the scripts it hands work to are run with argument lists, never a command line for a shell. A refusal is
  said in the health log (Security), as is anything that erases or rewrites a disk; routine actions (mount, unmount)
  are not, to keep the log readable. rr_health writes the health log's format ({"t","k","m"}).
- **rudolfd's side** (iso/sys/rudolfd-root.inc): `root_call` (the helper's status, as run() gave a command's),
  `root_run`, `root_out` (its output, as run_out(), read while it runs), `root_job` (as start_job: job.name, job.log,
  job.status). Started without rudolf-rootd (an older rudolf-init, or the QEMU tests that start rudolfd again by hand)
  and running as root, rudolfd runs the helper itself in the same clean way; so every step works whether or not the
  drop is on, and the API is the same byte for byte either way (the host test compares them).
- **The drop** (the last step, behind a switch until every area has moved, default off): rudolfd starts as root (from
  rudolf-rootd), does what must be done before (opens 127.0.0.1:8080, which needs no root as a port above 1023; reads
  the start-up facts; makes RUN/d, its own folder, rudolf's 0700, for the files it writes; the health log made rudolf's
  0600: the helpers still append to it as root), then setgroups to rudolf's own groups (audio and video for the
  screenshot of /dev/fb0), setresgid and setresuid to 1000, checks it cannot go back (setuid(0) fails), and sets
  PR_SET_DUMPABLE 0. No capabilities are kept (none is needed: binding 8080 does not need one, reading /proc/net/tcp
  and the desktop's own processes' descriptors for the peer check of 15.1 is allowed to the same user). The switch: a
  build flag (RUDOLFD_DROP) and, for testing, `rudolf.rudolfd_drop=1` on the start-up options; the default turns on
  only when every area of 11.1 has moved and the QEMU checks pass with it.
- **no_new_privs:** rudolfd sets it after the drop if nothing it starts still needs set-user-ID. Today two would:
  `su` (the clipboard: unneeded once rudolfd is the desktop's user) and Chromium's chrome_sandbox (4755) for the
  windows rudolfd starts (websites on Lite, Tor windows); with no_new_privs Chromium cannot use it and needs its
  namespace sandbox, which needs unprivileged user namespaces. So: no_new_privs is set on rudolfd itself only if the
  QEMU checks show Chromium's windows still start with it; otherwise those windows are started through a helper and
  no_new_privs is set in every other child rudolfd starts (prctl in the child before exec). rudolf-rootd's helpers are
  unaffected either way (they are rudolf-rootd's children, not rudolfd's).
- **The accounts' files after the drop:** rudolfd as uid 1000 cannot read /persist/accounts/ID (the account's user's,
  0700), and giving it a capability to (CAP_DAC_OVERRIDE) would undo the point. So the accounts' area adds a file
  service run as the account's own user (not root): started by rudolf-rootd at the unlock with the account's folder,
  serving the home/* operations over a socket pair to rudolfd (the same openat(O_NOFOLLOW) walk as rudolfd-home.inc
  today, moved). rudolfd's API stays as it is.
- **Tests:** host tests for each helper's checks (iso/qt/host/roothelpers161_t.sh: bad names, "..", links out of the
  home folder, another account's file, a hard link to root's, an unknown helper, a request from another user, from
  another program holding the descriptor; rudolfd's API the same with and without rudolf-rootd); the build's owners and
  modes (iso/qt/host/rootbuild161_t.py, and checks in ci_release.sh on the base layer itself); in QEMU on the newest beta
  with this tree's programs put in place (iso/qt/sb/fix161.sh): iso/qt/sb/r161_t.py (each moved area, with a USB stick)
  and t160_t.py's checks unchanged with R161_FIX (.github/workflows/roothelpers-check.yml, run by .github/r161-run.txt).

#### 11.3 Status
- **Drives, part 1 (2026-11-17, branch root-helpers-drives):** rudolf-rootd, rudolf-root.h, rudolfd-root.inc and the
  drives helper (iso/sys/rudolf-drives.c, /usr/lib/rudolf/root/drives): mount, unmount, eject, a stick's information,
  Install (and its boot files only), Replace the boot files, Make a stick, Keep files. rudolf-init starts rudolfd
  through rudolf-rootd; mkrootfs.sh and ci_release.sh build and install them (root's 0700, checked); ci_release.sh runs
  the two host tests. Found and kept as it was (the API unchanged): unmount answers "safe to remove" even when umount
  failed (the old command's status was its last part's, rm -f); for the owner to decide, not changed here.
- **Updates (2026-11-17, branch root-helpers-update, after root-helpers-drives):** the updates helper
  (iso/sys/rudolf-updates.c, /usr/lib/rudolf/root/updates): every rudolf-update command rudolfd ran (check, download,
  rollback, stable-info, to-stable, status, speedtest, boot-ok, return-official keep|erase, set url|auto|channel,
  developer on|off), the arguments checked again as rudolfd checks them (the beta address only through set channel
  beta; a developer's key 44 characters of base64; addresses without spaces or quotes), and the desktop's page
  update (POST update, the page on standard input; POST rollback). Found and kept as they were: POST rollback always
  answers "returned to the previous version" (the old command's status was sync's); the page update is not signed
  (any holder of the token can replace the desktop's page until a restart) and Developer mode and Return to official
  are asked an administrator's password by the page only, not by rudolfd: for the owner (the accounts step can move the
  administrator's check into the helpers, the password on standard input).
- **Drives, part 2: the accounts' folders and sessions (2026-11-21, branch root-helpers-home):** the sign-in's chain as
  drives helper actions: `drives home-unlock ACCT 0|1 UID` (the key on standard input, 128 hex digits, never an
  argument; 1: with the session), `home-lock ACCT`, `home-bind ACCT UID` (rudolfd started again in a session),
  `home-store` (the downloads' store at start). The drives helper checks the account (letters, digits and "-", at most
  64; the guest and the shared folder 400 "bad account" as before; an account the account list says was removed
  refused) and UID (1000 or that account's own uid from the list: the owner of what the sign-in makes outside the new
  folder, which 1.6.0 took from the request: the account's user for the page's home/unlock, the desktop's for the
  native sign-in), then runs /usr/lib/rudolf/root/drives-home (root's 0700; not one of rudolf-rootd's helpers, so it
  refuses the name): rudolfd's own functions for this (home_unlock, home_lock, home_bind, dl_store_bind and what they
  call: the key and policy, Oracle's /persist/oracle/ID of 1.6.0 Part 22 under the same key, the binds, the downloads'
  store and its tidying, au_session_start and au_session_stop with the D-Bus, the portal, the keyring and the
  display's access) built from rudolfd.c as a program of their own (iso/sys/rudolf-drives-home.c, main renamed as the
  host tests do), so they do line for line what rudolfd did as root; nothing in it reads a request of the page. rudolfd
  asks it from home_unlock, home_lock, home_bind and dl_store_bind (rudolfd-roothome.inc, one line in each), so the
  page's home/unlock and home/lock, the native sign-in and sign-out and rudolfd's start go through it; without
  drives-home (rudolfd built for the host tests) and as root, rudolfd does it itself as before. Part 20 (rudolfd
  confined, branch rudolfd-confine): drives-home runs inside the drives helper's process (exec), so it gets the drives
  helper's capabilities and system-call list; what it needs (CAP_SYS_ADMIN for the mounts and the key, CAP_CHOWN,
  CAP_SETUID/SETGID for su, CAP_KILL for sign-out, CAP_DAC_OVERRIDE) is in that list already.
  Found and kept as they were: the folders made at sign-in outside the account's folder (the binds' mount points in
  /home/rudolf, the downloads moved into its Downloads) are the account's user's after the page's home/unlock and the
  desktop's user's after the native sign-in (home_uid differs between the two requests); an account that is not in
  the account list at all (no Linux user yet) is still opened, its folder the desktop's user's; for the owner.
  Changed (the one difference): an account the list says was removed is refused at the drives helper, where rudolfd
  alone made it a new folder. Tests: roothelpers161_t.sh H1-H7, 14b, 22, 23 (49 checks: the arguments, the key on
  standard input only, removed accounts, drives-home refused by name by rudolf-rootd, and the whole chain on an
  encrypted ext4 with the accounts' users in a copy of /etc: the same answers and the same state through the helper,
  with and without rudolf-rootd, as rudolfd doing it itself); homeapi_t.sh with HAT_VIA=helper (its 69 checks through
  the helper, and 2 more); in QEMU t160_t.py with R161_FIX (roothelpers-check.yml).
  The vault (1.5.0's encrypted folder) with it: `drives vault-unlock` (its key on standard input: vault_add_key, from
  POST vault/unlock and the native sign-in; with the Flatpak bind and the downloads' store again), `vault-feature`
  (tune2fs -l of the drive: GET vault, GET home), `vault-prepare` (tune2fs -O encrypt at setup, its output in
  RUN/vault.err as before; now said in the health log, Disk), `vault-move` (the job); vaultnative60_t.sh with
  VN_VIA=helper. Not moved yet: vault/setup's and vault/store's vault.json in /persist/etc (rudolfd's own files: the
  drop's step), dl_acct_dir's chown of the downloads' folders.
- **Accounts (2026-11-21, branch root-helpers-accounts, after root-helpers-home):** the accounts helper
  (iso/sys/rudolf-accounts.c, /usr/lib/rudolf/root/accounts, in rudolf-rootd's list): `accounts sync` (after each save
  of the page's storage: a Linux user made for a new account, the user of one that is gone stopped and removed, its
  uid never given again, its Oracle folder deleted), `accounts start` (rudolfd's start: that, every user into /etc, the
  move of existing accounts' folders), `accounts keyring-list`, `keyring-forget ID` (Settings › Privacy › Saved
  passwords for apps, as the signed-in account's user). Like drives-home, rudolfd's own functions built as a program
  of their own (au_sync's work split out as au_sync_ids, au_start's as au_start_users: the same lines). rudolfd reads
  the page's accounts from the page's storage, as before (the helper never parses it) and hands their ids on standard
  input, one a line; the helper checks each again (as au_sync did: letters, digits and "-", not the guest, not
  "shared"; at most 256, no line over 80) and takes the uids and who is removed from the account list. rudolfd asks it
  only when an account came or went (au_sync_needed reads the account list, which anyone may: the same test au_sync
  made), not at every save. The keyring's account is the one signed in (RUN/home-session and the list), never one the
  caller names; ID checked as rudolfd checks it. Part 20 (merged, #534): the accounts helper's line in
  rudolf-rootd-confine.h's HELPER_CAPS: CAP_CHOWN, CAP_DAC_OVERRIDE, CAP_DAC_READ_SEARCH, CAP_FOWNER, CAP_KILL,
  CAP_SETUID, CAP_SETGID, and the helpers' deny list; rudolf-rootd sets no_new_privs on a helper that keeps no
  CAP_SYS_ADMIN, which a seccomp filter needs without it (found by roothelpers161_t.sh with RH_CONFINE=on, now run by
  ci_release.sh and the check: the accounts helper did not start, 253). Should it ever not start, rudolfd does the work
  itself as before (said in its log), so nothing breaks. Found and kept as they were: which accounts exist
  is still rudolfd's word (the page's storage is the only list of them), so a rudolfd that is not what it should be
  could have an account's user removed and its Oracle folder deleted (its home folder stays, encrypted): for the
  owner, with the drop (a signed list from the page, or the removal asked with the account's key). Tests:
  users59_t.sh with US_VIA=helper (its 29 checks through the helper, and 2 more); roothelpers161_t.sh A1, 14c and its
  API check 22 (the users written at start, the keyring's answer, the same through the helper as without).
  Not moved: the administrator's check (left as it is: the page asks the password for Developer mode and Return to
  official; moving it into the helpers would change who asks, for the owner to decide); the home/* files API (the file
  service run as the account's user: needed only for the drop).
- **Power (2026-11-21, branch root-helpers-power, after root-helpers-accounts):** the power helper (iso/sys/rudolf-power.c,
  /usr/lib/rudolf/root/power, a small program of its own): restart and shut down (`power off|reboot [T]`: the shutdown
  script with the desktop's times, its output in shutdown.out, then as before 1.4.5: Chromium stopped, sync, umount -a
  -r, reboot(2)), `wakealarm N` (1-60), `sleep` (/sys/power/state; the kernel's refusal said back for sleep.json),
  `status`, `set KEY=VALUE...` (lower-case letters, digits, "_" and "=", at most 30), `apply` (at start),
  `brightness-step up|down` (hotkey), `backlight VALUE` (0 to max_brightness of the backlight rudolfd also finds: an
  intel or amdgpu one first), `dropcache`, `priority on|off`, `clock-set MS`, `clock-sync` (BusyBox ntpd, pool.ntp.org
  then time.google.com, the hardware clock, RUN/time-sync as before). The commands it runs are looked for in the
  system's folders only, with no environment but PATH. rudolfd keeps each old line beside the call for when the helper
  does not start (rudolfd-rootpower.inc, -2): restart and shut down always work. Part 20 (merged, #534): the power
  helper's line in HELPER_CAPS (CAP_SYS_BOOT, CAP_SYS_TIME, CAP_SYS_ADMIN for umount -a -r and /sys, CAP_DAC_OVERRIDE,
  CAP_KILL, CAP_WAKE_ALARM) and HELPER_ALLOW, the calls of the helpers' deny list it may still make (reboot,
  settimeofday, clock_settime, clock_adjtime, adjtimex); rudolf-rootd now sets no_new_privs on a helper that keeps no
  CAP_SYS_ADMIN (the accounts helper), which a seccomp filter needs without it (found by roothelpers161_t.sh with
  RH_CONFINE=on: the accounts helper did not start, 253); ci_release.sh and the check run that test with the
  confinement on too. memory/dropcache is narrow only with rudolf-rootd now (confinepure.py). Found and kept as they were:
  POST time takes any epoch rudolfd's atoll reads (garbage set the clock to 1970); through the helper only digits are
  taken, so a malformed epoch is now 500 "could not set the clock" (the page sends Date.now()); POST power/set with
  more than 30 settings, or a setting with an empty part between two "&", now answers 500 instead of the script's 400
  (the page sends a few). Tests: roothelpers161_t.sh P1-P5 (the arguments, stand-ins for every script and command, so
  a host test never restarts the computer or sets its clock) and check 24 (the API through the helper, with and without
  rudolf-rootd; rudolfd built there so that it could not restart or set the clock even if it did the work itself).
  In QEMU (roothelpers-check.yml, run 44874168648): r161_t.py 22/22 with its new 16-21 (the settings, the desktop's
  priority, the cache, the clock set and back, the helper never skipped, a restart through the API that comes back),
  t160_t.py 31/31, t150_t.py 18/18. Sleep, brightness and shut down: for the owner on the Vivobook.
- **Network (2026-11-21, branch root-helpers-network):** the network helper (iso/sys/rudolf-network.c,
  /usr/lib/rudolf/root/network, a small program of its own; rudolfd-rootnet.inc): every wifi.sh action rudolfd ran
  (`network wifi status|scan|disconnect|reconnect|radios|bands IF`, `country IF CC`, `adapter IF`, `radio IF BSSID
  on|off`, `band IF 2.4|5|6 on|off`, and `connect|forget IF` with the request on the helper's standard input, at most
  600 bytes, which the helper writes to the same root-only file wifi.sh reads and removes; the join is still a job),
  System health's wifiNow and Settings' service restart (reconnect), `wifi-link IF` (GET net's network name and signal:
  wpa_cli status and signal_poll on wpa_supplicant's sockets, root's), `dhcp IF` (ip link up, udhcpc with udhcpc.sh),
  `static IF ADDR GW DNS` (flush, add, up, the default route; DNS into /etc/resolv.conf, written even when ip failed,
  as before), `firewall on|off|status`, `firewall remote on|off` (remote access's port), the health log's hourly count,
  `hotspot status|stop|start` (its request on standard input, at most 200 bytes) and `metered-state`, `metered IF
  on|off|auto` (netplug; Blitzen's knowledge download asks it too). The helper checks every argument as rudolfd does
  (an adapter's name: letters, digits and - _ . :, at most 16; a country two capital letters or 00; a radio's address,
  a band, on or off), and for dhcp and static, which hand the name to ip and udhcpc, also that the kernel has it
  (/sys/class/net) and that it does not start with "-" (both answered 500 before as now). Its commands are looked for in
  the system's folders only, with no environment but PATH. GET net's own reads (ip route, ip addr show, /etc/resolv.conf,
  /sys/class/net) stay in rudolfd: any user may make them. rudolfd keeps each old line beside the call for when the
  helper does not start (-2), so the network always works. Part 20: the network helper's line in HELPER_CAPS
  (CAP_NET_ADMIN, CAP_NET_RAW, CAP_NET_BIND_SERVICE for the hotspot's dnsmasq, CAP_DAC_OVERRIDE and
  CAP_DAC_READ_SEARCH for wpa_supplicant's sockets and saved networks, CAP_FOWNER, CAP_CHOWN, CAP_KILL, CAP_SETUID and
  CAP_SETGID for dnsmasq's own user); no CAP_SYS_ADMIN, so it gets no_new_privs. The programs it starts and leaves
  running (wpa_supplicant, hostapd, dnsmasq, udhcpc's renewals) are now rudolf-rootd's descendants instead of
  rudolfd's, and with the confinement on they keep the network helper's capabilities and system-call list (enough for
  what they do; for the owner's check on the Vivobook: Wi-Fi joined, the hotspot shared). confinepure.py: no request
  became narrow (each keeps its old line for when the helper does not start). Found and kept as they were: POST
  net/static may still be asked for the loopback adapter (lo); the request files' names (RUN/wifi.req,
  RUN/hotspot.req) are the same as before, now written by the helper as root. Tests: roothelpers161_t.sh N1-N6 (the
  arguments, the requests on standard input only, stand-ins for wifi.sh, the firewall, the hotspot, netplug, ip, udhcpc
  and wpa_cli, so a host test never touches this computer's network) and check 25 (every network request through the
  helper, with and without rudolf-rootd, and as rudolfd doing it itself: the same answers and the same calls, byte for
  byte); in QEMU r161_t.py 22-26 (the firewall off and on with nft, a fixed address, DHCP with no server, Wi-Fi without
  an adapter, the helper never skipped). Not moved yet: the modem (mobile/*), Tor, remote access's own start
  (rudolf-remote), Bluetooth and Thunderbolt (the devices: next).
- **Devices (2026-11-21, branch root-helpers-devices, after root-helpers-network):** the devices helper
  (iso/sys/rudolf-devices.c, /usr/lib/rudolf/root/devices; rudolfd-rootdev.inc, which asks it the network's way):
  `devices bt status|on|off|visible`, `bt scan SECONDS` (3-60), `bt answer yes|no`, `bt pair|connect|disconnect|remove
  MAC` (capital hex digits, as rudolfd's bt_mac_ok), `devices tb json`, `tb allow|deny ID`, `tb forget UID` (as
  tb_arg_ok); the scripts' start-up actions (bluetooth start, stop, agent-run, boot; thunderbolt plug, boot) are refused:
  only rudolf-init and mdev run them, as root. GET bt, every POST bt/..., GET tb, POST tb/... and System health's
  Thunderbolt line go through it; each keeps its old command for when the helper does not start. Part 20: its line in
  HELPER_CAPS (CAP_NET_ADMIN, CAP_NET_RAW, CAP_NET_BIND_SERVICE for bluetoothd and its D-Bus when "on" starts them
  again, the DAC capabilities, CAP_FOWNER and CAP_CHOWN for the pairings in /persist/bluetooth, CAP_KILL, CAP_SETUID and
  CAP_SETGID for headphones set up as the desktop's user); no CAP_SYS_ADMIN, so no_new_privs. For the owner (with the
  confinement on only): whether the kernel asks more than the file's mode for a Thunderbolt device's "authorized" (the
  test computers have no Thunderbolt). Tests: roothelpers161_t.sh D1-D2 and check 26 (in check 25's comparison: the same
  answers and calls with and without rudolf-rootd and as rudolfd itself); r161_t.py 27-28 in QEMU (no adapter, no bus).
- **Other, part 1: Task Manager's End process and the kernel's messages (2026-11-21, branch root-helpers-procs, after
  root-helpers-devices):** two small helpers (rudolfd-rootother.inc). The procs helper (iso/sys/rudolf-procs.c,
  /usr/lib/rudolf/root/procs): `procs end term|kill` (one process) and `procs tree term|kill` (End process tree, in
  rudolfd's order: all stopped, then ended from the last, each let go again), the numbers on standard input (a tree
  can be longer than an argument list), one result a line (0, or the error's number: the API's 500 said with it as
  before). rudolfd still lists the processes and decides; the helper checks each again as Task Manager marks what must
  keep running (pid 1, the kernel's, rudolfd, rudolf-display, X, the session, rudolf-launch, the desktop's own browser,
  and rudolf-rootd and the helper itself) and refuses the whole list if one is there (Security). The logs helper
  (iso/sys/rudolf-logs.c): `logs dmesg`, `logs dmesg-raw` (-r), nothing else; rudolfd keeps every one of its pipelines
  (the crashes, a job's last crash line, Wi-Fi's kernel lines, System health's messages and the graphics' firmware,
  GET log) with the messages on their standard input instead of dmesg's own. Part 20: HELPER_CAPS has "procs"
  (CAP_KILL only) and "logs" (CAP_SYSLOG only), both with no_new_privs. Each keeps its old way for when its helper does
  not start. Tests: roothelpers161_t.sh O1-O3 (the test's own processes only, a stand-in dmesg) and check 27 (in check
  25's comparison: End process, a tree of five, rudolfd itself 403, a process gone 404, the log, the crashes, a job's
  last crash line; the same with and without rudolf-rootd and as rudolfd itself); proctree57_t.sh unchanged;
  r161_t.py 29-31 in QEMU.
- **Display and sound (2026-11-21, branch root-helpers-display, after root-helpers-procs):** two helpers
  (rudolfd-rootscreen.inc). The screen helper (iso/sys/rudolf-screen.c, /usr/lib/rudolf/root/screen; not "display":
  sys/rudolf-display.c is the window manager): `screen mode soft|games|full` and `screen shell on|off` (/etc's file, and
  /persist/etc's on an installed Rudolf OS, which the helper reads itself from /run/rudolf/persistent), `screen ctl
  status|keep|revert`, `ctl set RES FPS` (screenctl), `screen keyboard LAYOUT [VARIANT]` (setxkbmap on :0),
  `screen restart-display` (System health's restart of rudolf-display) and `screen restart-browser` (the desktop's own
  browser, either engine: the proxy, the sound card and POST restart-browser). The sound helper
  (iso/sys/rudolf-sound.c): `sound volume get|up|down|mute|set N|muted 0|1`, `sound playing`, `sound mic
  users|get|muted 0|1` (hotkey), `sound echo on|off|status`, `sound mics`, `sound mic-default NAME`, `sound default N`,
  `sound restart` (the sound script; echo, the microphone chosen and the restart still into RUN/sound.log), `sound card
  N` (/etc/asound.conf, and /persist/etc's on an installed one). Part 20: HELPER_CAPS "screen" (the DAC capabilities,
  CAP_FOWNER, CAP_KILL) and "sound" (CAP_SETUID and CAP_SETGID for su to the desktop's user, CAP_KILL, the DAC
  capabilities, CAP_FOWNER, CAP_CHOWN), both with no_new_privs. Not moved: GET display's and the shell's reads (any
  user's), pointer (RUN/pointer, rudolfd's own: the drop), desktop/engine (/persist/etc's files: the drop), the native
  desktop's and web windows' starts as the desktop's user (none needed after the drop: rudolfd is that user; Tor
  windows' cgroup write with Tor). Found and kept as it was: GET audio/mics never answers (a "use POST" for every GET
  further up api() comes first; Settings' microphone test falls back to the browser's list): for the owner. Tests:
  roothelpers161_t.sh S1-S2 and check 28 (in check 25's comparison: every display and sound request with and without
  rudolf-rootd and as rudolfd itself, the files written in /etc and /persist/etc too); r161_t.py 32-33 in QEMU.
- Next: the modem, Tor and remote access; packages; the rest of other (the self-test, devlink, the camera, the portal,
  the terminals); then the drop (with the accounts' file service of 11.2).

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

### 13. Files dropped from outside onto the native desktop (added on 2026-11-16; 1.6.0 Part 12's known differences)
On Standard a file dragged from a Linux app (the Debian file manager, a Flatpak) onto the desktop is saved in the
Desktop folder. rudolf-native did not speak X11's drag and drop (XDND), so such drops did nothing on Native.
**Built on 2026-11-17 (PR #444, "files dropped from outside onto the native desktop"). Merged early, and kept in 1.6.0 by
the owner's decision of 2026-11-16 ("Keep it in 1.6.0"): it ships with 1.6.0, not 1.6.1.**
- **What Standard does, checked in its page and in Chromium (xdrop161_std.js):** a file dropped anywhere on the desktop goes
  into the Desktop folder, also over a folder's icon or the Trash (their own drop handlers take only the page's own drags;
  the drop then reaches the desktop's: shell25.js), named as the page names files (upper case, `NAME - COPY.EXT` on a clash),
  kept as text or as a data: URL by its type, over 64 MB refused ("Too large"); dropped text (no file) is refused; the
  taskbar takes no drop and raises no window. Native now does exactly that. The plan's "plain text as a note" and "a hover
  raises the window" were not Standard's, so Native does neither (the rule); a raise on hover would be new on both engines,
  for the owner to decide.
- **Native:** the desktop is an X drop target (XDND 5, GTK's): copy while a drag offering `text/uri-list` is over it and an
  account is signed in (not locked, not the guest), refused for text; the `file://` paths go to a page of the shell's
  (`#app=do-fs&op=xdrop`, oneapp59.js `oaXdrop`), which does shell25.js's drop line by line (the same names, checks and
  `secIngress`), reading each file through rudolfd; XdndFinished (copy) once the page has kept them. Drops take their turn
  (one page at a time). The paths travel in parts of at most 12 000 characters.
- **rudolfd (`GET desktop/dropped`, rudolfd-drop.inc):** reads only the signed-in account's own folder (as /home/acctUID,
  /persist/accounts/ID or Debian's /home/rudolf) and the drives (/media, as the page's fs/read), by the account's own Linux
  user (a child process that becomes it), every step opened without following a link; Flatpak's document portal is mapped
  to the real file through the portal's `user.document-portal.host-path`, which must itself be one of those. Refusals come
  back to the page as words the page shows ("Blocked: … was not copied: your account may not open it."). No shell.
- **Not done:** Rudolf as a drag source (a desktop icon dragged into a Linux app), optional; the guest's drops (rudolfd has
  no Linux user for the guest); a folder dropped (skipped; on Standard Chromium's reader fails on it); a link inside the
  home is refused (Chromium follows it). Files dropped on an app's window are WebKit's own (as Chromium's on Standard).
  When rudolfd drops root (Part 11), desktop/dropped's change of user moves into a root helper.
- **Tests:** xdrop161_t.sh (host, 16 checks): a stand-in Linux app (fixtures/xdndsrc.c) drags onto the desktop, a folder's
  icon, the Trash, two files at once, a taskbar button for 1.5 s, text, a web address, another account's file, a link out,
  a folder; what Native's page keeps is compared with Standard's page given the same drops. drop161_t.sh (host, 20 checks):
  rudolfd's reading as the account's user (another account's file, a file the user may not read, links, `..`, folders,
  pipes, 64 MB, the portal's mapping, nobody signed in, a path too long to be whole). Not run on the release in QEMU (the
  beta's rudolfd has no desktop/dropped yet; no KVM here): the owner's checks on the Vivobook.

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

### 15. One title bar for apps that draw their own (added on 2026-11-18)
The owner, 2026-11-18, on Native with Steam, Discord and Claude open: "Steam, Discord, Claude, and other apps installed from
Debian, Flatpak, and some source sites seem to use their own windows inside of the OS-based windows. Is there a way to have
non-Rudolf apps like these use only Rudolf's own windows and title bars?" Of the three ways offered, the owner chose the
suggested mix ("We will go with your suggestion"):

**Why there are two.** Electron and Chromium apps (Discord, Claude, Steam's new interface, VS Code, Chromium-based
browsers) draw their own title bar and ask the window manager for none (`_MOTIF_WM_HINTS`, decorations 0). rudolf-native
frames every window the same, so it adds Rudolf's above theirs. Their bar is more than a title: Steam's menus (Steam, View,
Friends, Games, Help) and its account button, Discord's inbox and help buttons and the "Direct Messages" heading. Moving
the window by dragging their bar, and their own minimize, maximize and close buttons, need from the window manager what
rudolf-native does not do yet: `_NET_WM_MOVERESIZE` (it moves and resizes for the app), `_NET_WM_STATE_MAXIMIZED_VERT`
and `_HORZ`, `_NET_FRAME_EXTENTS`. Standard frames no Linux app's window (rudolf-display shows them without a title bar
of Rudolf's), so this part is Native's.

**1. The app's own setting first: Rudolf's title bar, nothing lost.** Where an app can use the system's title bar,
Rudolf asks it to, once, the first time it starts it, and only when the person has not chosen otherwise in the app:
- Chromium-based browsers (Flathub's Chromium, Brave, Edge, Vivaldi; Debian's Chromium): their "Use system title bar and
  borders" (`browser.custom_chrome_frame: false` in the profile's Preferences, when the key is not there yet);
- VS Code and VSCodium: `window.titleBarStyle: native` (when not set);
- a list in `iso/sys/root/usr/share/rudolf/titlebars` (app ID, how, where), so more can be added without a new
  program; each entry checked against the app's documentation before it goes in.
Those windows then ask for decorations, and get Rudolf's title bar as any other.

**2. Otherwise, the app's own bar alone (the default).** A window asking for no decorations gets no title bar of
Rudolf's: its frame keeps the edge (1 px, or 2 px with high contrast), the round corners and the resize edges; the
taskbar button, Alt+Tab, the overview, the workspaces, snapping and the LitePoint transitions (1.6.0 Part 17) all as
before. For its own bar to work, rudolf-native does what such apps ask:
- `_NET_WM_MOVERESIZE`: moving (dragging their bar), resizing (their edges), and the keyboard move and size, done as a
  drag of Rudolf's own title bar is (the same snapping, the same limits), until the button is let go;
- `_NET_WM_STATE_MAXIMIZED_VERT` and `_HORZ` (their maximize button and a double-click on their bar): Rudolf's maximize
  and restore (wm_maximise, with its transition); `_NET_WM_STATE_HIDDEN` and `WM_CHANGE_STATE` (minimize) as now;
- `_NET_FRAME_EXTENTS` set on every window (the edges' and the title bar's sizes), and all of these in
  `_NET_SUPPORTED`. GTK's own windows (the app windows of Rudolf's pages) keep moving themselves as now: they ask for
  decorations, so they are framed as before.
Games and Wine's borderless windows (the same hint, a window the size of the screen) stay as they are: full screen.

**3. Rudolf's title bar instead, by choice, per app.** The title bar's menu (right-click) on such a window has "Title
bar: the app's own / Rudolf's". With Rudolf's, the app's own bar is cut away as Oracle's Chromium bar is (the clip
window and its insets, 1.5.9 12.3c), at a height known for that app (the list above, `cut` entries: the bar's height in
the app's pixels at 100 %, scaled as the app is). Offered only for apps on the list, whose bar holds only what Rudolf's
has (the title and the three buttons); for one whose bar has more (Steam's menus, Discord's buttons), the menu says what
would be lost instead of offering it. Kept per account and app (`bd95titlebar:` in its settings, read by the shell).

**Tests.** `iso/qt/host/csd161_t.sh` (Xvfb and rudolf-native, a stand-in app made with `xwmclient.c`): a window with
decorations 0 has no title bar (its frame's height is the window's plus its edges), `_NET_FRAME_EXTENTS` says so; a
`_NET_WM_MOVERESIZE` move follows the pointer and ends on release, a resize from each edge; `_NET_WM_STATE_MAXIMIZED_*`
maximizes and restores (the transition runs); minimize from the app; a full-screen-sized one is full screen as before;
`native59_t.sh`'s framed windows unchanged. The settings step: the Preferences and settings files changed only when the
key is missing (an existing choice kept), once. Part 3: the cut window's insets, the menu's two entries and its notice.
**The owner's checks on the Vivobook (Native):** Discord, Steam and Claude with one title bar each; moving them by their
bar, their maximize, minimize and close; a Chromium-based browser from Flathub with Rudolf's title bar.

### 16. Each other's builds: the update address decides the updates and the sticks (added on 2026-11-19)
**Moved to 1.6.0 (the owner, 2026-11-19: "All four into 1.6.0"; one beta when all are built).** Built there; this text stays
the plan.
The owner, 2026-11-19: "The version made on the stick and the repository that updates are checked for should come from
whatever the set update address is written as by the computer's admin-level user. The system should be able to check that
the set update repository is set-up/working correctly and that it is able to match the user's key to it. The intent is for
people to be able to roll out their own updates but only to computers that are running the key specific to their own
version/build of Rudolf OS. USB sticks pull directly from whatever the set update address is, and still checks to ensure it
is set up properly before it attempts to make a copy of that repository's version of Rudolf OS. This is intended for people
to be able to download and use each-other's builds." Decided the same day: sticks offer both (the address first, "This
Rudolf OS" kept for use without internet); another key's build is allowed on a stick after a warning; Get beta versions
keeps the repository that is set.

**How it is today.** Check for updates uses the Update address box (`update.conf` `url`), else the build's own default
(`/etc/rudolf/update-url`). An update installs only when its release list is signed by a key the running system trusts
(the build's `rpk-keys`, plus Developer mode's `extra-keys`): that part of the intent is already true and stays as it is.
But Get beta versions overwrites the box with the build's own `…/beta` (and `…/stable` when turned off); nothing says
whether an address works before an update fails; and Make a USB stick never downloads: it copies the running system (or an
image already in Downloads), its boot files from the running release's own `boot.iso`.

**1. Check this address.** A button by the Update address box (and the same check when Save is pressed, and in
`rudolf-update check-url URL`, for rudolfd's `sys/urlcheck`). Each line passes, warns or fails, in plain words:
- the address answers, with `manifest.txt` and `manifest.sig` (https only, as today);
- it is a Rudolf OS release list this computer can read (its format, `datafmt`/`readsfmt`), not expired;
- **whose key signed it:** this computer's (the official key, or Developer mode's key) → "Updates from this address
  install here"; another key → "This is another build of Rudolf OS, signed with key `xxxx…` (its fingerprint): updates from
  it do not install on this computer. You can make a USB stick from it.";
- that other key is the build's own: the list's signature verifies with a key in that release's own base layer
  (`/etc/rudolf/rpk-keys`, read from `base.sfs` after its checksum matches the list), so a list that its own system would
  refuse is reported as broken, not as "another build";
- every file the list names, and the release's `boot.iso` beside its `vmlinuz`, can be downloaded (sizes checked by a
  request for the first byte, nothing big downloaded);
- its other channel: `…/beta` beside `…/stable` (a warning only when missing: the beta switch then has nowhere to go).
Saving an address signed with another key asks first: "Updates will stop on this computer until the address is changed
back." The status line under Update and recovery says which key the set address uses.

**2. Get beta versions keeps the repository.** On: the set address's `…/beta`; off: its `…/stable` (and the
"leaving the betas" rule, `tostable`, as today). An address that does not end in `/stable` or `/beta` is used as it is,
and the switch says so. Developer mode's own address is unchanged.

**3. Make a USB stick from the update address.** "Rudolf OS to put on it" lists first "The latest from the update address
(Rudolf OS *version*, *serial*)", then "This Rudolf OS" and the images in Downloads as today.
- Before anything is erased: the check of item 1 for that address (all must pass; a warning about the other channel is
  fine) and room on RUDOLFDATA for the download.
- Downloaded to `/persist/os/stickdl/SERIAL/`: the release list and its signature, `boot.iso`, every layer; each layer's
  SHA-256 must match the list (layers this computer already has with the same checksum are linked, not downloaded); the
  boot image's kernel must be the list's `vmlinuz`; the list's signature must verify with the release's own key (item 1).
- Another key's build: before the drive is erased, a warning names it ("another build of Rudolf OS, signed with key
  `xxxx…`, from ADDRESS"), says that it gets its updates from its own maker, not from this computer's, that with Secure
  Boot on it starts only where its maker's Secure Boot key is trusted, and that its Recovery mode is the official Rudolf OS
  (item 4); the administrator confirms.
- mkstick.sh writes `boot.iso` as the image (as with an image from Downloads) and puts the release in the stick's
  RUDOLFDATA system slot, with that release's own update address (the one it came from), so the stick updates from its
  maker. "Also copy my files and settings" stays available.
- The downloads are kept until the next stick is made from another release (one release only), so a second stick needs no
  second download.
- The stick check (`mkstick.sh --check`) and Storage's line about a plugged-in stick name the build's key when it is not
  this computer's.

**4. Recovery mode is always the official build's newest stable release.** The owner, 2026-11-19: "Recovery Mode boot is
always locked to the official build's most recent stable version. This is so that if someone encounters a catastrophic
failure of their normal boot that they have a reliable and guaranteed way to recover their machine." Today the start-up
side already holds: the boot menu's Recovery mode (`recovery.efi`, its own small initramfs with the official key alone)
starts only a system/r whose list the official key signed, whatever the update address or Developer mode. What is missing:
- **Kept newest.** rudolf-update fetches the official recovery system once (`recovery-fetch`) and never again. Each update
  check (whatever the address) also reads the official stable list (`OFFICIAL_URL`, the official key alone); when its
  serial is newer than system/r's, the new one is downloaded beside it (`r.new`, the layers system/r already has linked),
  checked, and swapped in at once, with `recovery.efi` from that release's `boot.iso`; the old one stays until the new one
  is complete. Not while in recovery mode, and never from the update address, a beta, or Developer mode's key.
- **On every stick.** Today a stick's rescue slot is a copy of the stick's own system (`os_rescue`). Instead, whatever the
  stick's system (this Rudolf OS, an image, the update address, another person's build): its system/r is the official newest
  stable (this computer's own when it is that, else downloaded and checked as above) and its `recovery.efi` is the official
  one. A stick is not made without it ("Connect to the internet: a stick always gets the official recovery system").
- **Another person's build cannot replace it.** Its boot files may bring their own `rudolf.efi`, but `recovery.efi` is
  always the official one (bootfiles.sh and install.sh keep the official file; the start-up file check of 1.6.0 Part 15
  reports any other); and its updates never write system/r.
- Settings › Update and recovery says which official release Recovery mode starts, and when it was last renewed.

**What does not change.** Updates never install across keys. "Return to official" keeps using the official address. "New
boot files" (Secure Boot) keeps its own image choice, with the official `recovery.efi` beside it.

**Built in 1.6.0 (2026-11-19), and where it differs from the text above.**
- `rudolf-update check-url URL` (JSON), through rudolf-updates and rudolfd `sys/urlcheck`; Settings › Update and recovery ›
  Update address: **Check this address**, and Save checks first, asks before an address whose updates would not install
  here, and sends an administrator's name and password (rudolfd now refuses the address without them: "written by the
  computer's admin-level user"). Files are checked with HEAD requests.
- **Whose key, for another build:** an Ed25519 signature does not name its key, so each channel now publishes its build's
  public key beside its list (`key.txt`, rudolf-release.yml); the check names that key when it signs the list, and says
  the address is set up wrong when no key there does. Reading the key out of the release's base layer is done when a
  stick is made (it needs the layer downloaded), as item 3 says.
- Get beta versions: `chan_url` (the set address's `/beta` or `/stable`; neither: kept, and said).
- `rudolf-update stick-fetch URL DIR`; Make a USB stick's "The latest from the update address" (`@address`: rudolfd,
  rudolf-drives, mkstick.sh): `/persist/os/stickdl`, checked with the release's own key (its base layer's `rpk-keys`,
  kept as `keys` for install.sh's check of its boot image), the official recovery system beside it, and the stick's
  update address the one it came from. Settings checks the address first and names another build before the erase
  question.
- Recovery mode: `recovery-fetch` replaces an older official recovery system with the newest official stable release
  (looked at, detached, at most once a day from each update check); `os_rescue` gives every stick the official recovery
  system (this computer's, brought up to date, or downloaded), and only without one (offline, none here) a rescue copy of
  its own system, with a warning, rather than no stick (the owner chose to keep sticks that need no internet).
  `status` says which release Recovery mode starts, shown in Settings.
- **The official `recovery.efi` on a stick made from another build** (done 2026-11-19, the owner: "Add items 1 and 2 to
  the next scheduled beta build"). Its boot image brings its maker's `recovery.efi`, which checks the recovery system
  with its maker's key. `os_recefi` (oscopy.sh, run by mkstick.sh once the stick has the official recovery system) puts
  the official `recovery.efi` of slot r's own release in its place, in the stick's EFI system partition: from the boot
  image beside that release (its address from r's signed list), its checksum and Rudolf OS's Secure Boot signature
  checked; kept in os/recefi for the next stick; the boot menu's Recovery mode entry added when the build has none. The
  build's own start is left. Never fatal: offline with none kept, a warning. With Secure Boot on, the official file
  starts where the computer trusts Rudolf OS's key (another build's own shim may not). Test: `iso/qt/host/recefi60_t.sh` (12).
- Tests: `iso/qt/host/ownbuilds60_t.sh` (19), `iso/qt/shell2/ownbuilds60_t.js` (7), in ownbuilds-check.yml.

**Tests (planned).** A host test with two local channels, each signed with its own test key (RUDOLF_TEST_KEYS), over a local https
server: the check's lines for the trusted channel, the other key's channel, a broken signature, a list whose own base layer
does not have its key, a missing layer, a missing `/beta`; Save and the beta switch keeping the repository; a stick made in
QEMU from each channel (the other key's after the warning), started, and its update address the channel it came from; on
each, Recovery mode starts the official channel's newest release; system/r renewed when the official channel advances,
and left alone when only the update address or a beta advances.

### 17. Two-step sign-in, done properly (added on 2026-11-19)
**Moved to 1.6.0 (the owner, 2026-11-19: "All four into 1.6.0"; one beta when all are built).** Built there; this text stays
the plan.
The owner, 2026-11-19, after "Can two-factor authenticated menu sign-in be enabled from the security settings?":
"Plan the proper version." Decided the same day: hardware security keys and authenticator-app codes (not phone
passkeys); an administrator's copy of an account's key stays, and the person may turn it off; an administrator may
require a second step for administrators or for every account.

**How it is today.** User Settings › Sign-in options › "Add a security key or passkey…" (settings25.js, WebAuthn): the
desktop page checks the key's signature after the password. It guards the sign-in screen only: the account's file key
(`fk`) is opened by the password alone (PBKDF2, `ks`), so a copy of the disk needs only the password; the check is the
page's, not the system's; Native and Lite refuse such an account (`desktop/accounts` `key`); there is one key and no
backup; no authenticator codes.

**1. Hardware security keys, read by the system (all three engines).**
- rudolfd talks to FIDO2 keys itself (USB and NFC readers; `libfido2` loaded with dlopen, as OpenSSL is, from the base
  layer; `/dev/hidraw*` for rudolfd only), so Standard, Native and Lite, the lock screen and remote access (rudolf-remote)
  use the same check. Not the browser's WebAuthn: WebKit and the native shell have none that can do item 2.
- Adding one (User Settings › Sign-in options › Security keys › Add): password first, then touch the key (and its PIN, if
  it has one: asked on screen, never kept). Each account may have up to 5, with names ("Blue YubiKey"); the screen asks
  for a second one as a spare ("If you lose your only key, only a recovery code or an administrator can let you in").
- Signing in: password, then "Touch your security key" (any of the account's). A key with a PIN asks for it (the key
  counts wrong PINs itself and locks after 8).

**2. The key unlocks the files, not only the screen.** For an account with a security key, its file key is no longer
wrapped by the password alone: each security key gets its own copy of `fk`, wrapped with
`HKDF-SHA-256(password key ‖ the key's hmac-secret output for this account's salt)` (FIDO2 `hmac-secret`: the key
computes it inside, from a secret that never leaves it). The password-only copy is deleted (and overwritten) when the
first key is added. So a copy of the disk, with the password, opens nothing without one of the keys. The same goes for
"Encrypt my data"'s data key (the account's copy of it) and an administrator's private key (`priv`), which opens other
accounts' copies.
- Keys that cannot do `hmac-secret` (very old U2F-only ones) are refused when added, with that reason.
- A **recovery code** (24 letters and digits in 6 groups, shown once, "write it down or print it") is one more copy of
  `fk`, wrapped with the code (PBKDF2, 600 000 rounds). It signs in without a key once, then asks to add a key or make a
  new code.
- **Administrators' copies** (`a.ak`, 1.5.0) stay, on by default: an administrator can still open the account's files and
  reset its password (the person's keys are then removed: they add them again). The person can turn them off (User
  Settings › Sign-in options: "Administrators can open my files", as now), after a warning: "If you lose your keys and
  your recovery code, nobody can open your files."

**3. Authenticator-app codes (TOTP).** Six-digit codes every 30 seconds from an app (Google or Microsoft Authenticator,
Aegis, 2FAS, …). Adding: a QR code and the text secret, then a code to confirm. Signing in: password, then the code
(one step either side of now accepted; a code used once is not taken again). Codes guard the sign-in only: the secret
is kept wrapped by the account's password key, so it cannot add anything to the encryption, and Settings says so ("A
security key also protects your files if the disk is copied; a code does not."). An account may have a security key,
codes, or both (either one is enough at sign-in).

**4. The system decides, not the page.** Every sign-in goes through rudolfd (`desktop/login`, as Native's does now): the
password, then the second step, then the file key opened and handed to the signed-in page (as Native's). The page no
longer opens `fk` itself for such an account. rudolfd counts failures: after 5 wrong passwords or codes for an account,
a wait (30 s, doubling to 15 min), recorded in System health; a security key needs no count (it is touched). The lock
screen asks again (password and the second step; administrators may set "the key alone unlocks the lock screen while
signed in").

**5. Requiring it.** Security Settings › Sign-in: "Two-step sign-in: Not required / Required for administrators /
Required for every account" (administrator, with its own password and second step to change it). An account that has
none is signed in once more, then asked to add one before the desktop opens (a security key or codes; "Later" is not
offered). The Guest account cannot have one and is turned off while "every account" is set.

**6. The old page-checked keys.** Accounts with one keep signing in on Standard as now; Settings shows "Set up again to
protect your files and use Native and Lite" until the key is added again under item 1 (then the old one is removed).

**What it protects, said in Settings.** A security key: someone who knows or guesses the password cannot sign in, and a
copy of the disk is unreadable without a key. Codes: someone who knows the password cannot sign in at the screen. Not:
someone with the computer while it is signed in and unlocked; that is the lock screen's job.

**Built in 1.6.0 (2026-11-19): items 3, 4 (in part) and 5; items 1, 2 and 6 later** (the owner: "Codes now, keys after":
security keys that unlock the files come in their own beta, tested first with a simulated key in QEMU and then the
owner's real key, because a mistake there would lock people out of their own files).
- `desktop/otp60.js`: User Settings › Sign-in options › Authenticator app (Set up… with the setup key, base32, and the
  `otpauth://` link; no QR code yet; a code and the password to confirm; Turn off, unless it is required); the code asked
  after the password at the sign-in screen (build19's sign-in patch, after the old page-checked security key) and at the
  lock screen; Security Settings › Two-step sign-in (Not required / administrators / every account; "every account"
  turns Guest off); an account that must and has none is asked to set it up after signing in, with Sign out and no
  "Later".
- rudolfd (`rudolfd-desktop.inc`): the native sign-in and unlock ask for the code (`{"need":"code"}`, HTTP 401) after a
  right password and before anything is opened; `desktop/second` for the page; per account (`second-ID`): the last step
  used (a code is not taken twice) and wrong codes counted (5: 30 s, then doubling to 15 minutes). Wrong *passwords* on
  Standard are still counted by the page's own sign-in screen as before; on Native rudolfd's as before.
- rudolf-native: the second step in the same field ("Code from your authenticator app"), the password kept for it.
- Tests: `iso/qt/host/totp60_t.sh` (RFC 6238's values, rudolfd's and the page's), `iso/qt/shell2/otp60_t.js` (12), in
  otp-check.yml.

**Tests (planned, for the keys).** A virtual FIDO2 key (a CTAP2 HID device made with uhid, in QEMU) with `hmac-secret` and a PIN: adding two
keys, signing in with either, the password-only copy gone (the disk image opened offline with the password fails, with
a key's output it opens), wrong PIN and no key, the recovery code once, an administrator opening and resetting, the
setting turned off; TOTP with the RFC 6238 test vectors, reuse refused, the waits; "required for every account" asking
at the next sign-in; Standard, Native and Lite each signing in the same accounts.

### 18. Security keys and the setup QR code: what Part 17 left (added on 2026-11-20)
The owner, 2026-11-20: "Add everything else to 1.6.1." Part 17 was moved to 1.6.0 with "Codes now, keys after": the
authenticator codes were built there (desktop/otp60.js, rudolfd's `desktop/second`); these were not.
- **Security keys that unlock the files** (Part 17 items 1, 2 and 6): a FIDO2 key's hmac-secret wraps the account's file
  key, so a copy of the disk needs the key as well as the password; two keys at least (a spare), rudolfd deciding
  (Part 17 item 4), Native and Lite accepting such accounts, the old page-only WebAuthn keys moved over.
- **The setup QR code:** User Settings › Sign-in options › Authenticator app shows the `otpauth://` link as a QR code
  beside the setup key (an encoder of Rudolf's own in desktop/, as the commercial's: byte mode, levels M and Q, versions
  1 to 10; checked by decoding it in the test), so a phone's app adds it with its camera.
- **Tests:** a simulated FIDO2 key in QEMU (as Part 17 planned) for the unlocking; the QR code decoded back to the link.
- **Status, the QR code: done (2026-11-20, branch otp-qr; qr-check.yml).** `desktop/qr61.js`, written from ISO/IEC
  18004: byte mode, levels M and Q, versions 1 to 10, the mask the four penalty rules choose; drawn as an SVG, black on
  white with the 4-module quiet zone in every theme (5 px a module for the setup link, version 6-M). The dialog shows
  it to the left of the setup key (level M); a link too long for version 10 shows the key and link only. Tests:
  `iso/qt/shell2/qr61_t.py` (66 codes, both levels, every version and mask, each version's largest text, decoded with
  OpenCV's `cv2.QRCodeDetector` and zbarimg, which must agree; the chosen mask the lowest penalty; the dialog in
  Chromium in the light, dark and high-contrast themes read back from a screenshot to the exact link, its quiet zone
  and 21:1 contrast measured) and `iso/qt/webkit/qr61_ci.py` (the same dialog in rudolf-webkit: Lite's whole page and
  Native's one-app Settings, light and dark, read from the X screen). OpenCV's classic detector misses a few codes at
  some sizes (finder patterns not found: zbarimg reads them, and so does OpenCV at another size or with its ArUco
  detector), so the test tries those before failing; it never accepts a different text. **For the owner:** scan it
  with the phone's authenticator app (Settings › User Settings › Sign-in options › Authenticator app › Set up…).
- **Status, the security keys: built (2026-11-20, branch fido-keys; fido-check.yml).** rudolfd reads the keys itself
  (`iso/sys/rudolfd-fido.inc`, libfido2 opened with dlopen; `ci_release.sh` adds libfido2 and libcbor to the base layer) and
  decides: `desktop/keys*` for the page (Standard and Lite: `desktop/fido61.js`), and `desktop/login`/`desktop/unlock` ask
  for the key for Native (`rudolf-native.c`: "need":"key", the password kept, the touch). **Changed from Part 17's item 2,
  so a new password needs no key:** the file key (and an administrator's private key) is sealed under HKDF(the
  password's secret ‖ the account's key secret); the password's secret is a box in the account sealed with the
  password's key (acRekey seals it again), and each key keeps its own copy of the key secret, sealed under HKDF(its
  hmac-secret output) (asked without a PIN, so a PIN set later changes nothing; the PIN is asked only when adding).
  Two keys before "Require a security key"; requiring deletes the password's copies (the page's fk and priv, rudolfd's
  copy of the key secret). The old page-only WebAuthn keys move over when the person adds keys here (the add dialog
  says the old one is then removed). An administrator's password reset removes the keys first. The recovery path:
  docs/security-keys.md (no recovery code yet). **Tests:** `iso/qt/host/fido161_t.sh` (47: rudolfd from rudolfd.c with
  its test I/O and the real libfido2 talking to `iso/qt/host/vfido.py`, a software CTAP2 key with hmac-secret and PIN
  protocol 1: a wrong key, another account's key, a replayed answer, a key not touched, one key only, a PIN, Native's
  sign-in and lock screen, a new password, the page's box lost, a damaged record, and a copy of the files opened
  offline: nothing with the password, the file key with a key's hmac-secret); `iso/qt/shell2/fido61_t.js` (18: the page
  with a stand-in rudolfd); `iso/qt/host/nativekey161_t.sh` (4: rudolf-native's sign-in and lock screen on Xvfb with a
  stand-in rudolfd: the key asked at once after the password, a wrong key, Enter asking again); `iso/qt/sb/fido161_t.py` (QEMU, the newest beta with this tree's page, rudolfd and libfido2:
  the keys as real HID devices in the guest through uhid, `iso/qt/vfido_uhid.c`, their reports over a serial port to
  vfido.py on the host; the real Settings and sign-in screen, the lock screen, rudolfd's native sign-in, and the drive
  image read on the host afterwards; 2026-11-21, after #522: everything passes in CI but item 3's read of rudolfd's copy of the
  page's storage, asked by wget, which rudolfd does not serve: the account is checked in the page now, and rudolfd's copy on the disk image, 9a). **Not covered yet:** Native's and Lite's own screens with a key in QEMU (Native's
  shows the request's message under the password field, not the page's dialog: a difference to fix in Native), a recovery
  code, the elevation prompts (an administrator's password only), "Encrypt my data"'s copy of its key (password only).

### 19. Gaming on stronger computers (added on 2026-11-20)
Asked on 2026-11-20 how Rudolf OS games on high-end hardware. Found then: AMD and Intel graphics use Mesa (close to
Windows through Steam and Proton); NVIDIA cards have only NVK (iso/systools/addnvk.sh), well behind NVIDIA's own driver
on recent GeForce cards (no DLSS frame generation, weaker ray tracing); no game mode; Standard's whole desktop stays
behind a full-screen game. (The newest-kernel track and Wayland, for HDR and variable refresh, are docs/plan-2.0.md's.)
- **NVIDIA's own driver, by choice:** Settings › Display › Graphics driver: "Open (NVK)" (the default) or "NVIDIA's
  driver", for GeForce 16 and newer (the open kernel modules NVIDIA publishes), downloaded and signed for Secure Boot as a
  package from the Library with Rudolf's key, built in CI for the release's own kernel; switched back by itself (and
  said) when it fails to load, the recovery system always on the open driver.
- **A game mode:** while a game is full screen, the CPU's energy preference raised (amd_pstate/intel_pstate), the
  desktop's own work paused (Standard's page throttled, the background jobs and Oracle's tabs held), notifications held;
  everything back as it was when the game leaves full screen. GameMode's requests from games honoured. gamescope comes
  with Wayland (2.0).
- **Measured:** the same games' frame rates and frame times on the open and NVIDIA drivers and with game mode on and
  off, on the owner's Asus desktop (live from a stick, as Part 12) and the Vivobook.

### 20. Security hardening (added on 2026-11-20)
From the comparison with other distributions on 2026-11-19: the weak points named then.
- **rudolfd confined:** an AppArmor profile (or, where AppArmor is not in the kernel, systemd-free equivalents: its
  capabilities limited, a seccomp filter) allowing only the paths and actions it uses; Part 11's helpers each with a
  narrower one; the page's API checked against a list of what each window may call.
- **An outside security review:** before 2.0, a review by people outside the project of rudolfd's API, the sign-in and
  encryption, the update and recovery chain and the development link; findings fixed, published with the release.
- **Status: Standard's website data, "off" (2026-11-21, branch tpm-webdata; webkey-check.yml; docs/website-data-key.md).**
  The release's kernel had no TPM driver: TCG_TPM, TCG_CRB (firmware TPMs: the Vivobook's) and TCG_TIS added, built in,
  with their lines in iso/kconfig-required.txt (Part 12's check); TCG_TPM2_HMAC off (only the kernel's own random
  numbers use it here, and its session costs time at each start). rudolf-webkey (iso/sys/rudolf-webkey.c, the TPM's
  commands written out, through /dev/tpmrm0, root only; no tpm2-tss) seals a 32-byte secret under the owner hierarchy's
  ECC storage key to **PCR 7** (Secure Boot's state, its keys, the certificate each start-up file was checked with) and
  **PCR 14** (shim's MOK list), not 0-6 (firmware and boot-loader updates, settings) nor 11-12 (nothing measured there):
  updates of Rudolf OS change neither. Chromium's password is HKDF of it, given by rudolf-webkey's KWallet stand-in on the
  desktop's session bus (`--password-store=kwallet6`: Chromium 157 asks KWallet's openAsync, then readPassword); before
  Chromium starts, every "v10" value of the profile (cookies, saved site passwords, payment cards) is encrypted again as
  "v11". The session's webkey_start (once per start) sets PWSTORE for every Chromium of the shared profile: Standard's
  desktop, Native's Oracle, rudolf-shellfix (found: Chromium drops what it cannot read, so all three must agree).
  Falls back to Chromium's built-in key, said in System health and Settings › Oracle › Website data (rudolfd's GET
  webkey): no TPM; the TPM refusing (an owner's password, locked out: nothing changed); Secure Boot's measurements changed
  or the TPM cleared (a new key at once: websites sign in again, said once). **Tests:** webkey161_t.sh 25/25 (swtpm;
  tpm2-tools' own policy session opens the same object to the same key; Rudolf's Chromium: v11 written and read, a v10
  profile moved over, nothing readable with the built-in key), webkey161_t.js 10/10, wk161_t.py in QEMU (swtpm as a CRB
  TPM, a kernel with the driver; the session's own webkey_start; a restart; Microsoft's keys enrolled in the firmware with
  Secure Boot still off, which changes PCR 7). **Not covered:** the computer itself taken (it starts Rudolf OS, which
  opens the key; the sign-in is then what protects them), signed-in use (anything running as the desktop's user can ask
  the key store), a discrete TPM's bus while it starts (no parameter encryption; firmware TPMs have no bus), Lite and the
  RudolfWeb windows' profile, and what Chromium does not encrypt itself (history, cache, site storage). **For the owner
  (Vivobook):** after a beta with this, Settings › Oracle › Website data says "encrypted with a key kept in this
  computer's security chip"; turn Secure Boot off and on again in the firmware: one start each says "a new key" in
  System health and websites ask to sign in again; a firmware update leaves the key as it was.
- **Standard's website data, "off":** a key sealed in the computer's TPM (bound to Secure Boot's measurements) in place
  of Chromium's built-in key, so a drive taken out cannot read the shared profile's cookies; written up with what it
  does not cover (the computer itself taken, signed-in use). From 1.6.0 Part 22's "could come later".

#### 20.1 Status: rudolfd confined (2026-11-21, branch rudolfd-confine; confine-check.yml)
- **What the release kernel has** (read from release 2026112014's own vmlinuz and modules layer): seccomp and its BPF
  filters (on for Flatpak), Yama, lockdown, the audit log; **no Landlock** ("landlock" was in the LSM list since 1.5.9,
  but SECURITY_LANDLOCK itself was never on, so the kernel had none), no AppArmor, no TPM driver. Added:
  SECURITY_LANDLOCK, with its line in iso/kconfig-required.txt (Part 12's check), x86-64 and ARM64. Why Landlock and not
  AppArmor: a program puts Landlock on itself, so the rules live beside the code Part 11 keeps changing, nothing is
  loaded at the start (AppArmor needs its profiles compiled and loaded by the start-up, a parser or a compiled policy in
  the image, and a new LSM with a profile per program to keep in step with every script), and it works the same for
  rudolfd as root today and as uid 1000 after Part 11's drop. About 30 KB of kernel.
- **The switch:** off by default (the build's RUDOLFD_CONFINE); /persist/etc/rudolf-confine (one word: off, log, on);
  rudolf.confine= at the start-up; RUDOLFD_CONFINE in the environment (host tests). Read by rudolfd and rudolf-rootd at
  their start. "log": the system calls are only logged, Landlock and the capabilities' cut left out, the windows' refusals
  answered and noted: for a beta's first run. Each call refused or logged reaches the health log once per start
  (Security: "the confinement refused system call N of PROGRAM"), read from the kernel's audit lines.
- **Why two layers:** everything seccomp, Landlock and the capabilities do is inherited by every program the process
  starts, and rudolfd still starts many (shell commands, the accounts' apps, Chromium's windows, the Debian container,
  Wine). A filter of only rudolfd's own calls on all of rudolfd would confine every app it starts. So:
  1. **All of rudolfd, from its start** (before it starts anything): a deny list of what nothing it starts needs (kexec,
     BPF, performance counters, file handles by number, I/O ports, process accounting, quotas, writing another
     process's memory, uselib, nfsservctl; the same in the 32-bit table for Wine's programs; x32 refused), and the
     capabilities no Rudolf OS program uses dropped from the bounding set (audit, MAC, process accounting, BPF,
     performance counters, checkpoint/restore). Modules, swap, mounts, the clock, ptrace, keyctl stay: the camera's
     driver, zram, the binds and the Debian container, time sync, debuggers in Debian need them.
  2. **Each request that starts nothing:** each connection is its own process, so once its path is read (before its
     body or JSON), a request that runs no program, mounts nothing and signals no other process is narrowly confined for
     the rest of its life: seccomp allowing only rudolfd's own calls (143: those traced in every host test that builds
     rudolfd or a fixture of it with strace -f, plus the C library's on paths the tests do not reach, each with why:
     iso/sys/rudolfd-confine-narrow.h; never exec, mount, kill, ptrace, setns, unshare, chroot, reboot, keyctl, io_uring,
     userfaultfd, 32-bit calls); Landlock: read anywhere (as root reads today), write, make and remove only in
     rudolfd's folders (/run/rudolf, /run/user/1000, /persist/etc, /persist/var/log, /persist/accounts, /home, /media,
     /tmp, /proc/sys/vm/drop_caches, /dev/null), run nothing, no TCP of its own, no signals or abstract sockets outside;
     capabilities cut to owners and modes, reading the accounts' folders, the peer check's /proc, the dropped file's
     child becoming the account's user; no_new_privs. Which requests: iso/systools/confinepure.py reads every function
     of rudolfd and its .inc files and what each reaches (iso/sys/rudolfd-confine-pure.h): today 28 requests and the
     page with all its files, and 9 more that reach root only through rudolf-rootd's helpers (while rudolf-rootd is
     there); the other 97 still start programs. As Part 11 moves an area, its requests join (the host test says which,
     so the list never lags the code). Five that start nothing are kept out by hand, as they write where the narrow
     folders do not reach: display (/etc), bootmenu (the firmware's variables), brightness (the backlight under
     /sys/devices), factory-reset (/persist itself), desktop/keys (the security keys' /dev/hidraw).
     Found in QEMU (r161, on): a job through rudolf-rootd (install, sticks, boot files) also runs a shell command
     (root_job), so the narrow confinement refused its exec: those are no longer narrow, and the reader follows
     root_job's body. Found on the way: the peer check reads the desktop's /proc/PID/fd and exe, which a Landlock domain may not (as
     ptrace): it is made before the narrow confinement and kept for the connection.
- **Part 11's helpers, each narrower** (rudolf-rootd.c, rudolf-rootd-confine.h): in the helper's own process just
  before it starts: its capabilities only (drives: mount, files' owners and modes, the firmware's boot entries' flag,
  loop devices, su; updates: mount, files' owners and modes) in every set, the bounding set included, and a deny list:
  rudolfd's, plus ptrace, modules, reboot, the clock, swap, and every 32-bit call. No Landlock: both helpers mount.
  A helper without its line is never run with "on".
- **The windows' lists:** the process holding the connection (found by the peer check, never from the request) is the
  native shell (rudolf-native) or the page (Standard's Chromium; WebKit's network process for the native desktop's app
  windows and Lite). iso/systools/winapi.py reads what each asks for (iso/sys/rudolfd-winapi.h: 352 paths for the page,
  every literal given to its API helpers and each string that is one of rudolfd's request names; 35 for the shell);
  anything else is refused (403 "this window does not use that part of the API") and said in the health log. The
  shell, for instance, is refused the files API; the page is refused desktop/login, desktop/accounts and the shell's
  other requests. Root is not checked.
- **Tests:** iso/qt/host/confine161_t.sh (21): the lists current; every call number 0-511 of each table (x86-64,
  32-bit, x32) through each filter by a BPF interpreter (fixtures/confineprobe.c); each layer for real with every
  allowed and refused folder and call; the real rudolfd and rudolf-rootd off, log and on (the filters on the right
  processes, the answers as before, a window's request refused or noted, a helper's capabilities). Every rudolfd host
  test passes the same with the switch off and on. .github/workflows/confine-check.yml: the same on GitHub's runner,
  a kernel built from this tree's kconfig.sh (with Landlock; not published), and in QEMU on the newest beta with this
  tree's programs (fix161.sh): t160's checks unchanged off, on (the release's kernel), on and log (the kernel built
  here), r161's on; the health log's confinement lines read from the test drive afterwards.
- **Not covered:** the requests that still start programs (most of the API until Part 11 moves them) are only in layer
  1; a narrow request may still ask rudolf-rootd's helpers for what they do (their own checks hold); reading is not
  narrowed (rudolfd as root reads the accounts' folders and every process's information); a missed system call in a
  narrow request on a path the tests did not reach fails that request with "on" (hence log first); Lite's WebKit
  windows and the native desktop's share WebKit's network process, so the page's list is the same for both.
- **For the owner (Vivobook):** a beta with this, then `echo log > /persist/etc/rudolf-confine` (from a root shell) and a
  day's use; System health › Security then lists each call "on" would refuse (none expected). Then `on` for a day:
  sign-in, files, Settings, Oracle, apps, the Debian container, a USB stick, an update check, sleep. Default on only
  after that.

### 21. The checks that cannot run, and the checks with the owner (added on 2026-11-20)
- **CI:** the Oracle file check, the Tor window keys check and "same (beta)" fail on GitHub's runners because the beta's
  Chromium (chromium.sfs) does not start there ("required file not found", exit 127). Each moved to Playwright's
  Chromium as the newer checks were, or given the libraries the beta's Chromium needs, so every check runs again.
  **Done (2026-11-20, branch ci-beta-chromium).** Nothing was missing but the loader: since 1.6.0 the beta's Chromium
  carries its own glibc and names it by absolute path (program interpreter `/opt/chromium/glibc/ld-linux-x86-64.so.2`,
  RPATH `/opt/chromium/glibc:$ORIGIN`, where Rudolf OS mounts chromium.sfs), so unpacked under `rel/c/` on a runner the
  kernel found no interpreter. `iso/qt/host/betachrome.sh` links the unpacked tree at `/opt/chromium`, checks the loader
  and `ldd` (the other libraries are the runner's, as base.sfs's are in Rudolf OS) and gives `/opt/chromium/chrome`. The
  three checks keep testing the beta's own Chromium 158 (not moved to Playwright's): the Oracle file check's page job,
  the Tor window keys' two beta steps and "same (beta)". The same fix in three more that start it the same way and
  would have stopped the same way: Blitzen knows, App links (host) and Dark pages video. Left for the owner: nothing.
- **A virtual smart card** in QEMU (vsmartcard's reader with a PIV emulator and a test CA: 1.6.0 Part 20's planned test).
  **Done (2026-11-20, branch smartcard-qemu; smartcard-qemu-check.yml).** vsmartcard has no PIV card of its own (vicc's
  types are ISO 7816, Cryptoflex, ePass, nPA), so `iso/qt/host/vpiv.py` is one: SP 800-73's PIV application (SELECT,
  GET DATA, VERIFY with 3 tries, GENERAL AUTHENTICATE with an ECC P-256 key in 9A), its keys from a test CA made in the
  test (never kept). On the host (`vpiv_t.py`) it sits in vsmartcard's `vpcd` reader: OpenSC, this tree's `smartcard
  status`/`nss`, NSS's `tstclnt` signing in to a test site with the PIN, a wrong PIN, the card taken out. In QEMU
  (`iso/qt/sb/sc161_t.py`, the newest beta, Standard) it sits in QEMU's own USB CCID reader (`usb-ccid` with
  `ccid-card-passthru`, QEMU's VSCard protocol), so the release's own devplug, pcscd, libccid, OpenSC, NSS, `smartcard`,
  rudolfd's `sc/*` and Oracle's Chromium are what is tested: the reader plugged in, Settings › Smart cards (holder,
  expiry, "Expires in N days"), Oracle on a test site that asks for a certificate (Chromium's picker, its PIN box, the
  site sees the holder), a Tor window with no card module, the lock when the card is taken out, the reader unplugged.
  **Found and fixed:** in 1.6.0 OpenSC's module never started (`C_Initialize`: CKR_GENERAL_ERROR), because pcsc-lite's
  client library `libpcsclite.so.1`, which OpenSC opens itself, was not in the base layer. So no reader or card was
  ever seen, and the module was never added to the desktop's database. `ci_release.sh` now copies it. The QEMU test
  puts it in place (fix=1) until a release has it. **Not covered:** a real reader and CAC (the owner's, below);
  Native and Lite; the Debian container's `pcsc_scan`; Chromium's retry after a wrong PIN (the host's half covers the
  wrong PIN through NSS); a website reached through the Tor window. The test checks that the Tor window's Chromium has
  its own HOME with no card module and never asks the card for anything, but its page stops at the first-start wizard,
  because the test skips the real wizard. A SOCKS stand-in for Tor is already in the test for when that is solved. The test tells OpenSC not to use a PIN pad, because QEMU's reader claims one it doesn't have.
  **For the owner, with a real reader:** on Standard, Chromium's PIN box opened without the keyboard (the test clicks
  into it first) and filled the whole screen. Check both on the Vivobook with a reader, then fix them in their own PR
  if real hardware shows the same.
- **On the Vivobook, with the owner:** Oracle's websites encrypted (1.6.0 Part 22) on Native, Lite and Standard turned
  on; Recovery mode from a stick made from this release and from another build's; two-step sign-in on Standard and
  Native; calls; a real CAC only when the owner asks.

### 22. File Explorer: dragging between windows, the selection box, the progress window (added on 2026-11-20)
The owner, 2026-11-20, with a photo of two File Explorer windows side by side (Downloads; the Games (Emu library) folder):
"Users should be able to move files by click-dragging items from one file explorer window to a different window showing a
different location within it. They should also be able to use the mouse to click-drag a selection box over items in the
file explorer to select multiple files at the same time to move via right-click or click-drag. A pop-up window should
appear that shows file transfer progress." On Standard, Native and Lite alike.
- **The selection box:** pressed on an empty spot of a list (details, icons, thumbnails; C: and a drive's pane) and dragged;
  what it touches is selected while dragging, Ctrl or Shift adds, the list scrolls at its edge, a plain click on an empty
  spot clears. The mouse only (touch and keys as they were). A drive's pane got Shift+click, a right-click menu (Open, Cut,
  Copy, Paste, Delete, Copy into Rudolf OS, Copy to another drive…, Properties), keys and dragging.
- **Dragging between windows and panes:** C: ↔ C:, C: ↔ a drive or the Games folder, a drive ↔ another; onto a list (the
  folder it shows) or a folder row; highlighted while over; the same folder: nothing. A drag moves, Ctrl+drag copies (the
  pointer says which); from a read-only folder it copies; a read-only folder takes nothing. Cut, Copy and Paste the same.
- **Moves that are checked** (desktop/xfer61.js, xfRun): across stores a copy, its sizes and SHA-256 compared (rudolfd's new
  fs/hash, and home/hash), then the originals removed only once everything arrived; cancelled or failed, the originals stay
  and what was copied is taken away again. On the same drive a rename (fs/move). Into C:: remoteImport's limits (5,000
  files, the storage left, files over 256 MB left out) and the antivirus and type rules (secIngress, as Upload). rudolfd
  (rudolfd.c): fs/copy, fs/move, fs/hash, fs/write in pieces (part=, final=1), all inside /media and the shared folders,
  nothing hidden, never over something, links not followed.
- **The progress window:** "Moving N items" / "Copying N items", the file, n of N, bytes, a bar, the time left, Cancel; at
  once across stores, after half a second within C:; closes itself after a success, stays with the details otherwise;
  one per transfer, stacked at the bottom right. remoteImport (Copy selected into Rudolf OS) uses it too.
- **Native:** each window is a page of its own: a drive's selection goes with the drag (text/x-rudolf-remote, as
  text/x-rudolf-paths), Cut and Copy are kept where every window reads them, and a page that changed a drive's folder
  tells the others. The progress window is drawn in the page doing the work, as Properties is.
- **The 99-byte "- COPY"** (the same photo): Copy and Paste of a large download kept on the disk copied its reference, not
  the file. Now rudolfd copies the file (dl/file to=copy, in the account's own folder only) and the copy is a file of its
  own; one in a home folder kept on the disk is copied by home56.js as before; any other is refused with a message. A
  move keeps the reference. Copies made before (two entries naming one file) are made files of their own when Downloads
  is looked at (dlScope, dlDupFix); one that cannot be is kept and said so, never deleted unseen (before, the second was
  dropped).
- **Found on the way:** C:'s Properties window stopped at its first line (its icon replaced the window's contents).
- **Status (2026-11-21, branch fe-drag):** done; tests: iso/qt/host/fsxfer161_t.sh (rudolfd, 15 checks),
  iso/qt/shell2/xdrag161_t.js (Chromium with a stand-in rudolfd, the native pages included, 56 checks),
  iso/qt/webkit/xdrag61_ci.py (WebKitGTK, real mouse input, on GitHub: xfer-check.yml). **For the owner, on the
  Vivobook:** drag a file from Downloads into the Games (Emu library) window and back; box-select several files and drag
  them; right-click Cut and Paste across windows; watch the progress window; Ctrl+drag copies.

### 23. The native desktop's memory (added on 2026-11-21)
The owner, 2026-11-20, from the Vivobook (Rudolf OS 1.6.0, Native, 3.3 GB, MemAvailable 1.48 GB): six WebKitWebProcess processes,
one per window (the desktop, the taskbar's pages, Task Manager, the shell's hidden pages do-svc, do-portal, do-start, ...), each
250-380 MB settled and 400-470 MB at its start, about 1 GB together. Every window loads the whole desktop page (iso/www/index.html,
about 4 MB, built from base_v24.html and the ~150 modules of build19.py's list) and then shows one app. #530 (WebKit's memory
pressure limit following MemTotal, the start-up pages one after another) is in; RAM compression (zram, rudolf-memguard.c) exists.
Three parts, one PR each, in this order:
- **A. Fewer hidden pages** (the owner: "Add item 2"). **Done (2026-11-21, PR "Native: one hidden page").** The service page
  (do-svc), the portal's page (do-portal) and the start-up page (do-start) are one page, #app=do-bg (oneapp59.js: oaSvcRun,
  oaPortalRun, oaStartRun with &start=1 for a signed-in account that is not Guest, oaSrchRun), opened at the sign-in as the
  start-up page was, or by bg_tick once no other page is starting (page_starting, up to 10 s), again when it ends or does not say
  both "svc ready" and "portal ready" within 90 s; svc's since= kept. The shell's single actions that had a page of the whole
  desktop loaded unseen for each (do-qs, do-nc, do-pin, do-mute, do-desk, do-fs's drop onto the Trash, rename and xdrop,
  do-short) and the start menu's search (do-srch) go to it (rudolf-native.c bg_route: oneapp59.js __oaDo with the same op and
  address; a page of its own only while it is not ready, or if it does not take the action). Unchanged: do-portalapp (a real
  window), do-saver, do-run, do-pwr, Open with and a file opened from the search (they show a window), modal pages, PT_SHOWN.
  Measured on the host (Xvfb, WebKitGTK 2.52.6, --draw=soft, the stand-in rudolfd, 30 s after signing in, no app window):
  170 MB PSS for the hidden pages before, 110 MB after; the peak while Night light was switched on and off 208 MB before (a
  page loaded for each switch), 111 MB after. On this host WebKitGTK already puts every page of rudolf-native in one web
  process (native59_t.sh test 21), so the Vivobook's count of six is checked there (the owner's checks). Tests:
  iso/qt/host/bgpage161_t.sh (one hidden page, the web processes counted, each action done there, its process killed, signing
  out and in), svc160_t.sh and nportal160_t.sh with the new name, native59_t.sh, native160_t.sh, xdrop161_t.sh.
- **B. Each window loads only what it needs** (the owner: "Yes"): the shared core plus the window's app, not every app.
  **Measured first (2026-11-21, the host: Xvfb, WebKitGTK 2.52.6, --draw=soft, the stand-in rudolfd; each window in a web process
  of its own, as on the Vivobook, PSS / RSS 25 s after it opened):** the hidden page 90 / 213 MB, Task Manager 87 / 214 MB
  (shown after 873 ms), Settings 109 / 241 MB (1139 ms), File Explorer 85 / 219 MB (882 ms), Notepad 80 / 213 MB (1123 ms),
  Oracle's page in WebKit 81 / 216 MB (1133 ms); 506 MB PSS for the six. What a window's process is made of (rudolf-webkit, one
  process): a plain page 34 / 139 MB (WebKit itself), the desktop page's HTML and styles without its code 39 MB, its code parsed
  but not run 51 MB, the page's own code before the modules run 67 MB, all of it 91 MB. The page's ~150 modules' code is not the
  big part: a page built without 45 of them (the emulators, Blitzen, the office and phone apps, Oracle's extras, Task Manager,
  about 600 KB) gave Notepad 78 MB instead of 80 and Settings 75 instead of 109 (noise of ±10 MB between runs); the JavaScript
  heap is 6-8 MB in every window (JSC's GC log; Chromium's: 4.4 MB). Most of a window is WebKit's own process (34 MB PSS) and
  what running the page builds (about 40 MB), so a window costs about 80 MB PSS however little of the page it loads.
  **Stage 1, done (2026-11-21, PR "Native: the page's big payloads as files"):** the page's last big inline payloads are files
  beside it (www/a/, named by their contents, read only by their app): the Quake engine (632 KB of WebAssembly as base64:
  aBytes, synchronous as its checks are) and the Galaxy Map's universe (261 KB: aFetch). The page is 3.39 MB instead of 4.26 MB
  (Standard's too: the same page). Every window 4-6 MB less (483 MB PSS for the six instead of 506). The files are in iso/www,
  so in desktop.sfs with its dm-verity tree (systools/verity.py), whose root hash is in the signed release list (make_release.py),
  as 1.5.9's a/ files are; a page installed by hand from Settings › Update (an .HTML file, not a release) reads them from the
  release's own www/a/ (rudolfd's fallback), so such a page whose payloads differ says "could not be loaded" for that app.
  Test: iso/qt/host/chunks161_t.sh (neither payload in the page; Notepad's window never reads them; the Galaxy Map and Quake in
  native windows read and use theirs; a missing file is said; no error).
  **Left (each its own PR, for the owner's decision with the numbers above):** stage 2, the modules each app needs found by
  exercising every app (a page per app built from the core and its modules, loaded by oneapp59.js; Standard keeps the whole page):
  measured potential 2-5 MB per window, because the modules mostly patch each other (win, termUI, appsPanel, startMenuApps...) and
  the core is most of the code; stage 3, what the page builds at its start in an app's window (the desktop's icons, the start
  menu, the widgets: about 16 MB before the modules and 24 MB with them at Lite's sign-in screen) left out where the window does
  not show it. The bigger saving on the Vivobook is the number of processes (one shared on this host; six there) and Part C.
- **C. Idle windows give their memory back** (the owner: "if it won't cause any issues with Oracle or other Rudolf/Debian/
  Flatpak/Wine apps, then add it"). **Done (2026-11-21, PR "Native: idle windows give their memory back").** rudolf-native.c
  rest_tick (every 2 s): a Rudolf app's window minimized, or covered by one other window (or a full-screen one), for the time
  set (Settings › System › Idle windows: on by default under 6 GB of memory, after 10 minutes; 5 to 60) asks its page
  (rest61.js __oaRest) for where it is; when the page says it can rest, its view is closed (its page goes, and its web process
  if it has one of its own: WebKit keeps no process without a page) and a view never loaded (no process) stands in it, the
  app's colour over it. Shown again (focused, its taskbar button, no longer covered, its workspace), its page comes back as every
  window's does (window.open from another window's page, into the same window: on_create's into), its address with &rest=
  (its state) instead of &file=; the window's size and place never change. The apps that can (REST_APPS, the opt-in list):
  Notepad (its tabs' files, the current one, the caret and the scroll; only with every tab saved) and File Explorer (its folder
  on C:, one tab, one pane; nothing cut or copied); every scrolled box of the window comes back too. Never: an app not on the
  list (Settings, the Terminal, the office apps, the games and emulators: they cannot say yet whether something would be lost),
  Oracle ("browser", its WebKit window too), a program rudolfd runs (Linux, Debian, Flatpak, Wine, the Library's: X windows of
  their own, their pages unseen, keys with ":"), the shell's hidden page and every do- page, the desktop and the taskbar (no
  page); and never while the window is full screen, always on top or in a dialog over the screen (the shell), WebKit says it is
  playing sound or using the camera, microphone or screen, or its page has unsaved changes, a dialog, a media element playing, an
  AudioContext running, a live camera or microphone track, a call (RTCPeerConnection), a request still going (a download, an
  upload), a transfer (xfer61.js xfRun), a job polled in the last 20 s (rudolfd's job: installs, the Debian apps, a stick, a
  drive's copy; an update being installed). Task Manager shows the window as "Resting (no memory used)"; closing it closes at
  once. Measured on the host: each window in a process of its own (as on the Vivobook) Notepad and File Explorer 90 + 90 MB PSS
  (about 210 MB RSS each), their processes ended when resting (260 MB PSS for the three processes before, 98 MB for the one left);
  with the processes shared (this host's) 258 MB before, 229 MB with two resting. Not on Lite (its windows are tabs of one
  window: kept as they are). Test: iso/qt/host/rest161_t.sh.

## Not moved here
- **Oracle's website data encrypted:** written here on 2026-11-19, moved to 1.6.0 the same day (the owner: "Put it in
  1.6.0 instead"): docs/plan-1.6.0.md Part 22.
- **elogind and polkit** (1.6.0 Part 11): left out by decision on 2026-11-15, since Rudolf OS's own inhibit portal and
  prompts cover what apps ask. Taken up again only if an app or a later part needs them.
- **Native as the default for new installations** (#384): still 1.6.0's, for the owner's decision after the week of
  betas.
- **The apps moved to native code:** docs/plan-2.0.md.
- **The phone's own interface** (stage 2: the home screen's panels, the dialler, the lock screen, the shade, the
  touch layouts, Waydroid integrated, the camera and sensor apps): docs/plan-2.0.md, "The phone build".

## Steps
1. Part 1, several monitors, after the 1.6.0 beta.
2. Part 3, Lite's leftovers, one PR each.
3. Part 8 (mobile data and the SIM on real hardware) with the MiFi, before the phone; Part 9 (USB printers; ARM64 with
   the phone or an ARM64 computer).
4. The phone's stage 1: Part 4 on the first phone (after 1.6.0's ARM64 release and stage B), Part 7 with a
   T-Mobile-network SIM, Part 6's 1.6.1 items measured on it, Part 2's groundwork; Part 5 when the hardware is there.
5. The phone's stage 2 (its interface, Wayland, the native apps for touch): docs/plan-2.0.md, "The phone build".
6. Part 15 (one title bar for apps that draw their own) with Part 14, before Native becomes the default: 2 first (the
   window manager's part), then 1 (the apps' own settings), then 3 (the per-app choice).
7. Part 16 (each other's builds): 1 and 2 first (the check and the beta switch), then 4's "kept newest", then 3 with 4's
   sticks (the stick from the address, always with the official recovery system).
8. Part 17 (two-step sign-in): 1 and 2 together (keys that unlock the files) with 4 (rudolfd decides), then 3 (codes),
   5 (requiring it) and 6 (the old keys).
9. Part 21's CI checks first (so every later part is checked), then Part 18 (security keys with the QR code), Part 20
   (rudolfd confined; the review booked before 2.0), Part 19 (the NVIDIA driver choice, then game mode, measured), and
   Part 21's checks with the owner after each beta.
10. Part 22 (File Explorer: dragging between windows, the selection box, the progress window): one PR, then the owner's
   checks on the Vivobook.
11. Part 23 (the native desktop's memory): A (one hidden page), then B (each window only what it needs, in stages), then C
   (idle windows give their memory back), one PR each, each checked on the Vivobook; B's stages 2 and 3 after the owner has
   seen B's measurements.
