# Rudolf OS 1.6.1: what was moved out of 1.6.0

Asked for on 2026-11-15: "What items have been pushed beyond 1.6.0? Can they all be added to a plan for 1.6.1?" This
plan gathers every item that docs/plan-1.6.0.md moved past 1.6.0, with where it came from. Nothing here is built yet.
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

### 11. rudolfd's root work split into helpers (added on 2026-11-16; from docs/plan-1.5.9.md 15.5)
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
| Drives | `home_unlock`, `home_lock`, `home_key_*`, `home_policy_id` (sign-in's home/unlock, home/lock, desktop.inc) | fscrypt: FS_IOC_ADD/REMOVE_ENCRYPTION_KEY, SET_ENCRYPTION_POLICY on /persist/accounts/ID, the folder made and given to the account's user | drives (next) |
| Drives | `home_bind`, `home_unbind`, `home_debian_rebind` | bind mounts of the account's folders over /home/rudolf/…; rudolf-debian rebind | drives (next) |
| Drives | `dl_store_bind`, `dl_store_dirs`, `dl_acct_dir`, `dl_home_boot` | bind mount of the downloads' store; chown of its folders to 1000 | drives (next) |
| Drives | the vault: `vault_add_key`, `vault_key_spec_remove`, `vault_feature` (POST vault/*), vault-move (job), the Flatpak bind at unlock | fscrypt on the encrypted folder (1.5.0), tune2fs -l, mount --bind of /var/lib/flatpak, vault-move | drives (next) |
| Drives | `addpart` (rudolfd --addpart, run by the scripts) | BLKPG ioctl on a disk | stays (a separate run by root scripts, not a request) |
| Drives | `api_disks`, `api_storage` (storage-measure) | blkid of every disk (needs read access to /dev), du of the whole system | drives (read-only actions) |
| Updates | sys/check, sys/download, sys/tostable, sys/return (jobs) | rudolf-update: slots on RUDOLFDATA, the manifest's signature, the ESP | **updates** (moved) |
| Updates | sys/rollback, sys/set, sys/beta, sys/developer, sys/stable, sys/status, speedtest, boot-ok (at the desktop's first heartbeat) | rudolf-update's state in /persist/os, its settings in /persist/etc | **updates** (moved) |
| Updates | the desktop's page update (POST update, POST rollback: /persist/os/new, current, previous) | files of root's on RUDOLFDATA | **updates** (moved) |
| Updates | bootmenu, secureboot (`efivar_write`, `unimmutable`, `boot_entry_ensure`), MokNew/MokAuth | EFI variables (/sys/firmware/efi/efivars: chattr -i, writes) | updates (the boot entries, next) |
| Accounts | `au_start`, `au_install`, `au_sync`, `au_migrate`, `au_mv_one` | /etc/passwd, group, shadow written (the per-account Linux users of 1.5.9 15.3 / 1.6.0 Part 10), lchown of the moved folders | accounts |
| Accounts | `au_session_start`, `au_session_stop`, `au_kill_user`, `au_xaccess` | /home/acctUID bind mount, /run/user/UID made and chowned, the session D-Bus and portal started as the account's user (setuid), its processes killed, X's localuser access | accounts |
| Accounts | `api_keyring` (keyring/*) | /usr/lib/rudolf/keyring as root for the account's key files | accounts |
| Accounts | home/* (the files API: tree, list, read, write, rename, copy, delete, recovered) | reads and writes inside /persist/accounts/ID, which is the account's user's, 0700 (root's DAC override) | accounts: a file service run as the account's own user (not root), 11.2 |
| Accounts | desk_* (desktop.inc: the page's storage, dwp_ensure, dput) | chown of the desktop's folders to desk_uid | none needed after the drop (rudolfd is that user) |
| Power | power (POST power: poweroff, reboot), `sleep_now`, `api_sleep` | reboot(2), /usr/lib/rudolf/shutdown, pkill of Chromium, umount -a -r, /sys/power/state, /sys/class/rtc/rtc0/wakealarm | power |
| Power | power/set, `main`'s power apply, brightness (backlight writes through the power script) | /usr/lib/rudolf/power (CPU governor, backlight, USB autosuspend under /sys) | power |
| Power | memory/dropcache, memory settings (zram) | /proc/sys/vm/drop_caches, /usr/lib/rudolf/memory | power |
| Power | desktop/priority | /usr/lib/rudolf/desktop-priority (cgroup weights of the desktop) | power |
| Network | `api_wifi` (wifi/*: scan, connect (job), forget, radios, power) | wifi.sh (wpa_supplicant, wpa_cli, iw, rfkill), the root-only request file | network |
| Network | net/dhcp, net/static, dns, `api_net`'s wpa_cli status | ip link/addr/route, udhcpc, /etc/resolv.conf | network |
| Network | firewall (GET/POST), remote access's firewall holes, health_tick's firewall status | /usr/lib/rudolf/firewall (nft) | network |
| Network | hotspot, netplug, mobile/* (rudolfd-mobile.inc: modem, SMS, airplane mode) | hostapd, dnsmasq, ModemManager through /usr/lib/rudolf/hotspot, netplug, modem | network |
| Network | tor (`tor_start`, `tor_kill`), torwin (`torwin_start`) | Tor started as its own user (setuid in the child), its folder made and chowned; Chromium for Tor windows as the desktop's user with a cgroup write | network |
| Network | bt/* (bluetooth), tb/* (thunderbolt) | /usr/lib/rudolf/bluetooth, thunderbolt (sysfs authorize) | network (devices) |
| Network | remote/* (`remote_start`, `remote_stop`) | rudolf-remote started (as root today), its certificate and settings in /persist/etc | network |
| Network | time sync (`time_tick`, POST time, time/sync) | settimeofday, ntpd, hwclock -w | power (the clock) |
| Packages | pkg/* (rpk refresh, update, install, remove, install-file: jobs), pkg/exec (`api_pkg_exec`: setuid to the account's user) | rpk on the system's package folders; a Library program run as the account's user | packages |
| Packages | apps/* (`api_apps`: Flathub, install, update, remove, appextra), catalog | flatpak --system, /usr/lib/rudolf/appextra, RUN/apps chowned | packages |
| Packages | sysext/* (`api_sysext`) | rpk sysext (signed system extensions mounted over /usr) | packages |
| Packages | debian/* (`api_debian`, `spawn_debian`: setup, update, wine, deb, run --root), ai/* python and C++ | rudolf-debian (a container: namespaces, mounts) as root | packages (rudolf-debian is already a program: through the helper) |
| Packages | games/* (games-import), ai/* (`api_ai`: rudolf-ai, the model's folder), print/* (`api_print`: rudolf-print, usbprint) | folders chowned to 1000; rudolf-ai and rudolf-print as root | packages (most need no root: they run as the desktop's user after the drop) |
| Display | display (POST: /etc/rudolf-display), screen/* (screenctl), layout, pointer | /etc writes, xrandr through screenctl, setxkbmap through layout.sh | display |
| Display | screenshot (`grab_x`, `grab_fb`), clipboard (`api_clipboard`: su rudolf) | /dev/fb0; su to the desktop's user | none after the drop (video group; rudolfd is that user) |
| Display | native/*, web/*, shell (POST shell: /etc/rudolf-shell), restart-browser, desktop/engine | the native desktop, Chromium windows started as the desktop's user (su/setuid), pkill of the kiosk browser, /etc and /persist/etc writes | display |
| Sound | audio (/etc/asound.conf), audio/mics, mic, volume, media, sound restart, echo | /usr/lib/rudolf/sound, hotkey (PulseAudio and ALSA as the desktop's user, through su) | display (devices) |
| Other | processes, proc/end (`api_proc_end`, `proc_tree_end`) | kill(2) of other users' processes (the accounts' apps), /proc of every process | other: **procs** |
| Other | service (restart sound, Wi-Fi, display), selftest, starts, system (`api_system`: dmesg, modules), log, crash_list, health_tick's dmesg | the self-test, dmesg (kernel.dmesg_restrict), /proc/modules | other: **logs** (dmesg read by a helper, or dmesg_restrict=0 chosen in Part 12) |
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
- Next: drives, part 2 with the accounts' sessions (the encrypted folders' unlock and lock, the binds,
  the downloads' store, the vault: at sign-in home_unlock, home_bind, dl_store_bind and au_session_start are one chain,
  so they move as one helper action, "drives home-unlock ACCT [session]", the account checked against the account list
  and its key on standard input, never an argument; sign-out the same with home-lock); accounts; power; network;
  packages; display; other; then the drop.

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

## Not moved here
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
