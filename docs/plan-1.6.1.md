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

### 13. Files dropped from outside onto the native desktop (added on 2026-11-16; 1.6.0 Part 12's known differences)
On Standard a file dragged from a Linux app (the Debian file manager, a Flatpak) onto the desktop is saved in the
Desktop folder. rudolf-native did not speak X11's drag and drop (XDND), so such drops did nothing on Native.
**Built on 2026-11-17 (PR "files dropped from outside onto the native desktop"), for 1.6.1.**
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
