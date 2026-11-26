# Native vs Standard: test findings (2026-11-26)

A hardware session on the owner's Vivobook, following `docs/native-vivobook-checklist.md` through the development link.
**V58 passed**: the machine starts from its own drive with no stick attached. **Every other item is deferred**, not
passed and not failed — the link had been dead for two days, and the desktop turned out to be on Standard, where the N
items cannot be checked. Nothing below claims that something works on the machine unless the machine said so itself.

## Header

| What | Value |
|---|---|
| Build | **Rudolf OS 1.6.2.261124**, serial **2026112422**, **Official Rudolf OS** (from the result's `system` field) |
| Kernel | `6.18.65-rudolf` (`#1 SMP PREEMPT_DYNAMIC Tue Nov 24 23:06:26 UTC 2026`) |
| Machine | ASUSTeK **Vivobook Go E1404FA_E1404FA** (board E1404FA) — the test laptop |
| Started | From **its own drive**, with **no stick attached**: `/live/media` is `/dev/loop0` backed by **`/dev/nvme0n1`**, no `/dev/sd*` exists, `rudolf.slot=b`, `rudolf.sb=1`, `rudolf.mediadisk=/dev/nvme0n1`, `lockdown=integrity`. That first boot was about 22:49Z (uptime 38 min at 23:27:51Z) |
| Persistent disk | `/dev/nvme0n1p3`, 116.4 GB, 41.2 GB used (36 %), mounted at `/persist` and `/home/rudolf` |
| Read-only | **On**, in the link's own words: the banner reads "can look at this computer (read-only)" and the Allow dialog "It runs read-only". **But** `sh` ran as `uid=0(root)`, where `DEVLINK.md` says it runs as `nobody` — recorded as a question in "Two things the banner and the command disagree on" below |
| Ask first | **On**: `000584`-`000590` all read `approved: allowed on the computer` (it would read `not asked (Ask first was off)` otherwise). `000591` was **not answered** within 150 s |
| Home folders | **Not established** — nothing in this session read a home folder |
| The link | `merciless146/rudolfos-devlink`, branch `link`, `iso/systools/devlink.py` |
| Today's `main` | `745fcf18`, 2026-11-26 19:43Z, "Merge pull request #712 from merciless146/newfile-name"; this branch is `ros-vivobook-findings` |

**The version matters.** The checklist was written for **1.6.0** (`docs/native-vivobook-checklist.md`). The machine runs
**1.6.2.261124**, and `main` is **1.6.3 in progress**. So the checklist is two versions behind the machine and three
behind `main`, and a few of its "Done in #410" notes have been overtaken by later work. Before the next hardware
session, the checklist needs re-reading against the build that machine actually runs, not against the one it was
written on.

## Summary

- **The link was dead for two days.** The last result before this session was `000583`, **2026-11-24T00:40:51Z**. This
  session's command `000584` was written at **2026-11-26T23:19:07Z** and sat unanswered for 8½ minutes, then ran at
  **23:27:51Z**. `DEVLINK.md` ends a link after 4 hours without a command, so the link had ended by inactivity long
  before this session started; it came back when the computer did (the machine's own start was about 22:49Z today).
- **N1–N32: every fix named in `docs/native-findings-2026-11-15.md` is in today's `main`,** and the harness cases that
  check them are still in `iso/qt/native/same_t.js`. That is a fact about the repository, not about the machine: not one
  of them was re-tested on the Vivobook.
- **V58 passed.** The machine restarted on the session's `restart` at **23:34:07Z** and was up **54 s later** on the same build, still with **no stick attached**: it starts from its own drive, no firmware setup screen. Evidence: `docs/native-findings-2026-11-26/V58-native.png`.
- **V59 was not needed** (the setup screen did not appear), and **the other 70 items are deferred**. No window was opened and no key was pressed by the session: it has no `xdotool`, `wmctrl`, `ydotool`, `import` or `ffmpeg` on that build, and `DISPLAY` is empty in its `sh`.
- **Nothing was written to the computer.** The single command that ran was a read (`uname`, `/proc/cmdline`, `/run/rudolf`,
  `df`); it changed nothing, and it was not a `put`, a `restart` or a firmware operation.

## How it was tested

- **One command, `000584`**, `kind: sh`, `timeout: 120`. Its script, as written in the mailbox:

  ```sh
  # Read-only: record the build, serial, hardware and the link's settings for this session's findings header. Reads only; writes nothing.
  uname -a; cat /etc/os-release; … /proc/version; uptime; cat /proc/cmdline; id
  ls -la /run/rudolf; df -h
  ```

- **Its result**: `status 0`, `statusText null`, `error null`, `approved allowed on the computer`,
  `started 2026-11-26T23:27:51Z`, `ended 2026-11-26T23:27:51Z` (same second: the whole script is reads).
- **What the host could not do**: this session's own machine is the Vivobook's Debian host, not Rudolf OS. It has no
  WebKitGTK, Xvfb, Playwright, Chromium or `rudolf-native` build, so none of the host harnesses
  (`same_t.sh`, `native59_t.sh`, `videolog60_t.sh`, `oneapp59_t.js`) were run here, and running them would not be
  evidence about the Vivobook anyway (`docs/testing.md`, rule 3). The machine itself is the subject; it was reachable
  only through the link.

### The link's record

| Command | Kind | Written | Ran | Status | Approved | Note |
|---|---|---|---|---|---|---|
| `000583` (previous session) | `sh` | 2026-11-24 | result **2026-11-24T00:40:51Z** | — | — | The last thing the computer ran before this session |
| `000584` (this session) | `sh` | **2026-11-26T23:19:07Z** | **2026-11-26T23:27:51Z** | `0` | `allowed on the computer` | Unanswered for 8 min 44 s; the script ran in the same second it started |

The 47½-hour gap between the two is not a fault of the link's protocol: it is the 4-hour inactivity rule, after which
the computer stops polling, and it polls again only once the link is turned on again.

This session's commands after the header read:

| Command | Kind | Written | Result | Status | Approved |
|---|---|---|---|---|---|
| `000585` | `sh` | 2026-11-26T23:33:27Z | same second | `0` | `allowed on the computer` |
| `000586` | `sh` | 2026-11-26T23:33:36Z | same second | `0` | `allowed on the computer` |
| `000587` | `restart` | 2026-11-26T23:34:00Z | 2026-11-26T23:34:07Z | `0` | `allowed on the computer` |
| `000588` | `sh` | 2026-11-26T23:35Z | same second | `0` | `allowed on the computer` |
| `000589` | `screenshot` | 2026-11-26T23:35Z | same second | `0` | `allowed on the computer` |
| `000590` | `sh` | 2026-11-26T23:36Z | same second | `0` | `allowed on the computer` |
| `000591` | `screenshot` | 2026-11-26T23:38Z | **none** | — | — (no answer: no result within 150 s) |

`000587`'s result carries `system: Rudolf OS 1.6.2.261124 (2026112422), Official Rudolf OS`, written seven seconds
before the machine went down. It came back at 23:35:15Z with `uptime` 54 s.

### Two things the banner and the command disagree on

Not findings with a checklist number, and not acted on: both are for the owner to decide.

1. **`read-only` and `uid=0`.** The red banner and the Allow dialog both say the link is read-only, and `DEVLINK.md`
   says `sh` then runs as `nobody`; the command ran as `uid=0(root)`. Either the read-only mode no longer drops to
   `nobody`, or the banner is wrong about what it guarantees. Nothing in this session wrote anything, so the machine is
   unchanged either way — but the promise and the behaviour should agree.
2. **What the banner says the link may do** is "look at this computer (read-only)" while the settings page was reported
   as *Allow changes to this computer* ticked, which is why `restart` was accepted at all (a read-only link refuses it).
   So the banner's wording and the setting conflict; the setting is what the protocol enforces.

## N1–N32 checked against today's `main`

Each row asks: is the fix that `docs/native-findings-2026-11-15.md` names actually an ancestor of today's `main`
(`745fcf18`), and is the harness case that checks it still there? "In 1.6.2.261124?" asks whether that fix predates the
machine's build (2026-11-24 22:xx) — if it does not, the machine cannot show it however carefully it is tested.
**No row was confirmed, refuted or re-measured on the machine today.**

| # | Finding | On today's `main` | Evidence | In 1.6.2.261124? | Confirmed on the Vivobook? |
|---|---|---|---|---|---|
| N1 | Full-screen video black | fixed | `d363693` 2026-11-15; case `video-fs` | yes | no — deferred |
| N2 | File dialog cut off inside the window | fixed | `d363693`; case `dlg-open` | yes | no — deferred |
| N3 | Task Manager lists only itself | fixed | `d363693`; case `tm-apps` | yes | no — deferred |
| N4 | The page's dialogs in a window of their own | fixed | `d363693`; cases `dlg-ask`, `dlg-camera`, `dlg-print`, `dlg-dark` | yes | no — deferred |
| N5 | Sliders drawn by GTK | fixed | `6212d1f` 2026-11-15; cases `ctl-volume`, `ctl-mouse`, `ctl-media` | yes | no — deferred |
| N6 | Drop-down lists without chevrons | fixed, and the dark-mode chevron too | `6212d1f`; `a6da7aa8` 2026-11-16 (dark list); cases `ctl-wordlite`, `ctl-volume-dark` | yes | no — deferred |
| N7 | File Explorer: no "Folders on this device", 0 B used | fixed | `6212d1f`; case `explorer` | yes | no — deferred |
| N8 | About: "Apple GPU" | fixed | `068aa95` 2026-11-15; case `about` | yes | no — deferred |
| N9 | 24-hour time field in Clock | fixed | `6212d1f`; case `ctl-clock` | yes | no — deferred |
| N10 | Print dialog inside the app's window | fixed | `d363693`; case `dlg-print` | yes | no — deferred |
| N11 | Glacier Lake without northern lights | fixed | `36507ee` 2026-11-15; case `dyn-lake` | yes | no — deferred |
| N12 | Spirit Falls without its statue | fixed | `36507ee`; cases `dyn-oasis`, `dyn-oasis-dark` | yes | no — deferred |
| N13 | Colour box drawn by GTK | fixed | `6212d1f`; case `ctl-wordlite` | yes | no — deferred |
| N14 | Flash Player's own title-bar icon | fixed | `cb33ba5` 2026-11-15; case `title-icon` | yes | no — deferred |
| N15 | No window opening animation | fixed | `6254bc3f` 2026-11-17 (PR #412); case `wopen` | yes | no — deferred |
| N16 | Video statistics stop near the end | not reproduced | case `video-frames` present | — | no — a Vivobook check, deferred |
| N17 | Sandbox flag error (WebKit stand-in) | fixed | `cb33ba5`; case `sandbox-flags` | yes | no — deferred |
| N18 | Log video problems' noise | fixed in part (underflow lines kept by the owner's decision) | `d363693` | yes | no — deferred |
| N19 | Two WebKit web processes / memory | the test fixed; **memory with real use is a Vivobook check** | `cb33ba5`; `native59_t.sh` | yes | no — deferred |
| N20 | Log video problems emptied by each new window | fixed | `b83e625` 2026-11-16; `videolog60_t.sh` | yes | no — deferred |
| N21 | Right-click menus cut off inside apps | fixed | `b83e625`; case `app-menu` | yes | no — deferred |
| N22 | No scrollbars in the apps | fixed | `b83e625` | yes | no — deferred |
| N23 | The harness hid scrollbars | fixed | `b83e625` | yes | no — deferred |
| N24 | Display Properties' tiles 4-5 px taller | fixed | `b83e625`, with `510c0a3` | yes | no — deferred |
| N25 | Text boxes the page does not style | fixed | `b83e625` | yes | no — deferred |
| N26 | Check boxes | fixed | `d363693` | yes | no — deferred |
| N27 | Title bars in dark mode | fixed | `b83e625` (PR #409 finishes the frames); case `title-dark` | yes | no — deferred |
| N28 | GPU Lab says "Hardware" | fixed | `068aa95`; case `gpulab` | yes | no — deferred |
| N29 | Video frame callbacks never called | fixed | `b83e625`; case `video-frames` | yes | no — deferred |
| N30 | Start menu's search box and scrollbar | fixed | `510c0a3` 2026-11-16; case `menu` | yes | no — deferred |
| N31 | Oracle's focus on opening, Ctrl+T | **a Vivobook check** (real Oracle is Chromium) | — | — | no — deferred |
| N32 | Desktop sounds from app windows | fixed | CHANGELOG 1.6.0 ("on the Native desktop sound from the app windows … the desktop's own sounds play there too") | yes | no — deferred |

Also still open on `main`, and untouched by this session:

- **Night light tints only the app windows** (plan 1.6.3 / Part 12 item 9 series) — see checklist V40.
- **Touch mode's title bars** (Part 12 item 10) — see checklist V39.
- **Native's transitions** were changed again in `314e319b` (2026-11-25 07:00, "1.6.3 Part 8, item 2: Native's transitions
  drawn as Chromium's"), which is **after** the machine's build: the machine will not show it, so V47 and V66 on
  1.6.2.261124 are not a test of today's `main`.

## The checklist: V58 passed, every other item deferred

Every number in `docs/native-vivobook-checklist.md` (V1–V63 and V66–V74; there is no V64 or V65) is deferred. "Needs"
is what the machine alone can show; none of it was done.

| # | Area | Needs | Status |
|---|---|---|---|
| V1 | Sound Recorder | record 10 s, play back; 🎙️ in the taskbar | deferred |
| V2 | Camera | first open, the "Allow access?" prompt, frame rate | deferred |
| V3 | Camera | a photo and a 5 s video saved and opened | deferred |
| V4 | Calls | the call screen, 🎙️, mute, hang up | deferred |
| V5 | Microphone denied | the blocked message and how to allow it again | deferred |
| V6 | Media Player | video with sound, volume keys, the overlay | deferred |
| V7 | Media Player | another window opened while a video plays: gaps or clicks | deferred |
| V8 | Desktop sounds | Media Player › URL… › Add (N32) heard on Native | deferred |
| V9 | Bluetooth headphones | sound through them, and back when they are turned off | deferred |
| V10 | Sleep | Sleep, wait, wake: the desktop with its windows | deferred |
| V11 | Lid | close for 1 min, open: as V10 | deferred |
| V12 | Sleep with a video | paused or playing, sound back | deferred |
| V13 | Battery | unplug: the icon and percentage | deferred |
| V14 | Battery | the 20 % and 10 % warnings, once each | deferred |
| V15 | Power profiles | each profile applies at once; the taskbar battery's tip | deferred |
| V16 | Battery button | Settings' System health page | deferred |
| V17 | Battery saver | window opening animations off | deferred |
| V18 | Bluetooth | turn on, find, pair; the pairing code prompt | deferred |
| V19 | Bluetooth | unpair, turn off: list empty, switch off | deferred |
| V20 | Printing | WordPro › Print… to a real printer | **deferred — needs a printer** |
| V21 | Printing | Print to PDF from Notepad, opened from Documents | deferred |
| V22 | Second monitor | plug in HDMI: what each screen shows | **deferred — needs a second screen** |
| V23 | Second monitor | move a window towards it | **deferred — needs a second screen** |
| V24 | Touchpad | two-finger scroll: speed and direction | deferred |
| V25 | Touchpad | pinch to zoom in Image Viewer and Oracle | deferred |
| V26 | Touchpad | edge swipes | deferred |
| V27 | Debian container | a Linux app's window, title bar, taskbar button, Alt+Tab | deferred |
| V28 | Debian container | that app's File › Open dialog | deferred |
| V29 | Debian container | a Linux app with sound | deferred |
| V30 | Flatpak | open, use, close | deferred |
| V31 | Library games | download a game, launch, F11, sound | deferred |
| V32 | Gamepad | a game with a gamepad | deferred |
| V33 | Update | beta box ticked, Check for updates, Download, Restart | **deferred — needs the owner's agreement: updating changes the machine** |
| V34 | Update | beta unticked: "Up to date" or the release | **deferred — needs the owner's agreement** |
| V35 | Accounts | a second account, sign out, sign in as it | deferred |
| V36 | Accounts | Switch user from the start menu | deferred |
| V37 | Accounts | Lock (Super+L) with each account | deferred |
| V38 | Touch | on-screen keyboard into Notepad, Oracle and a Linux app | deferred |
| V39 | Touch | title bars at least 40 px, buttons 40×40 | deferred |
| V40 | Night light | the whole screen tinted, 8 PM-7 AM when scheduled | deferred |
| V41 | Long use | 30 minutes of mixed use, no restarts, no repeated notices | deferred |
| V42 | Memory | Task Manager at idle and with one app (N19) | deferred |
| V43 | Oracle | focus on opening, Ctrl+T (N31) | deferred |
| V44 | Video | frames counted to the end (N16) | deferred |
| V45 | Super+Tab | a video keeps moving in the overview | deferred |
| V46 | Notifications | a video moves round a notification | deferred |
| V47 | Windows | an app fades in and grows in 0.18 s | deferred |
| V48 | Files | drag to Notepad and Trash; copy between Notepad, Oracle, a Linux app | deferred |
| V49 | Screen saver | the saver, then the lock | deferred |
| V50 | Accessibility | magnifier, screen reader, high contrast | deferred |
| V51 | Alarms | an alarm with every window closed | deferred |
| V52 | Calendar | a reminder with every window closed | deferred |
| V53 | Calls and messages | 🎙️ and a notice with no window open (needs a phone or the SIM) | **deferred — needs a phone or SIM** |
| V54 | Alarms | an alarm with two windows open: once, not per window | deferred |
| V55 | Timers | a timer after Clock's window is closed | deferred |
| V56 | Task Manager | only it open: no extra row | deferred |
| V57 | Files | a drag from a Linux app's file manager onto the desktop, a folder, Trash, Notepad | deferred |
| V58 | Start-up entries | a restart without the stick | **passed** — the drive started on its own; see "V58" below |
| V59 | Start-up entries | Boot › Add New Boot Option if the setup screen appears | **not needed** — no setup screen appeared |
| V60 | Start-up entries | Secure Boot › Get new boot files… just opened, or a system report's start-up decisions | **deferred — needs the owner** (the entries below are the read-only half of it) |
| V61 | Start-up entries | the boot-entry fix's System health lines | **deferred — needs the owner** |
| V62 | Start-up entries | "Check this drive's start-up…" with the password | **deferred — needs the owner** |
| V63 | Start-up entries | "Put back the official start-up files…" on power | **deferred — needs the owner** |
| V66 | Windows | five apps from the start menu, a pin and a desktop icon | deferred |
| V67 | Windows | an app opened right after signing in | deferred |
| V68 | Menus | right-click menus replacing each other cleanly | deferred |
| V69 | Menus | the tray's pop-ups replacing each other | deferred |
| V70 | Icons | sign in: the icons with their pictures at once | deferred |
| V71 | Icons | New folder and New text document from the desktop menu | deferred |
| V72 | Icons | save to the Desktop, delete from it: the desktop at once | deferred |
| V73 | Backgrounds | each background at once, then Revert; again, then Discard | deferred |
| V74 | Display Properties | accent colour, dark mode, title-bar style, the screen saver's Preview | deferred |

Only V58 is marked: it `passes`. No other item is marked `matches`, `minor`, `looks wrong` or `breaks`, because this
session has no evidence for any of them.

## V58: the restart without the stick

**Passed.** What was done, in order:

1. `000585`, read-only, before anything changed: `BootOrder` = **`0006, 0002, 0001, 0000`**; entries `Boot0000 Windows
   Boot Manager`, `Boot0001 Drive`, `Boot0002 Ubuntu`, `Boot0006 Rudolf OS`; drive `KINGSTON OM8PGP4128P-AB`
   (`nvme0n1p2`, `nvme0n1p3`); **no `efibootmgr` binary** on the build, so the entries were read from the raw variables.
2. `000586`, read-only: no `/dev/sd*`, so **no stick was attached**; `/live/media` is `/dev/loop0` over **`/dev/nvme0n1`**.
   The two drive entries disagree with each other: **`Rudolf OS` (0006) names LBA `0x5AC4`, size `0x36000`** — the current
   file location — while **`Drive` (0001) still names `0x52EC`, `0x1E000`**, the 1.4.1 location the checklist describes.
   `Rudolf OS` is first in `BootOrder`, which is why this start worked.
3. `000587`, `{"kind":"restart"}`: `status 0`, `approved allowed on the computer`, result written 23:34:07Z.
4. The desktop came back: `000588` at 23:35:15Z showed `uptime` 54 s, the same kernel and build, `rudolf.slot=b`, and
   `000589`'s screenshot (`V58-native.png`) shows the desktop, its taskbar (`Rudolf OS 1.6.2`, 65 % battery, 17:35
   local) and the red link banner — over an Allow dialog for the screenshot itself, which is part of the picture.

What this does **not** show: that a *cold* start works (this was a warm restart from a running system), that the
firmware's setup screen is never reached (one restart is one sample), or that `Drive` (0001) is harmless — it is stale
and, being third in the order, untested. V60–V63 still need the owner.

### N1–N3 were not started, and why

`000590` found `/run/rudolf/desktop-engine` = **`chromium`**: the desktop was on **Standard**, and N1–N3 are Native
checks. Switching the engine is Settings › System health › Desktop engine and a restart, which is the owner's to do, so
the three were left for the next step. Also missing for them: no `xdotool`, `wmctrl`, `ydotool`, `import` or `ffmpeg` on
that build, `DISPLAY` empty in `sh`, and `~/Videos` empty — so N1 needs a video the owner adds (Media Player › URL… ›
Add), and each step is the owner's hands with the session taking the pictures.

## What this session did not do

- No window was opened, no key was pressed, no item performed by the session itself. V58's restart was the one change,
  and it was the item.
- N1, N2, N3, and every other item except V58: not run (see the table above).
- No `put`, no firmware operation, no update.
- **No fix was made and no code was changed** — this document only. So there is no `deadcheck` run and no page rebuilt
  (`AGENTS.md`'s "Before every PR" applies to code and to the desktop page, not to a findings document).
- Not checked: anything audible, anything on a second screen, printing on paper, a phone, a gamepad, a printer, an
  optical drive (the machine has none), and the 30-minute mixed-use pass.

## Where to start next time

1. **N1, N2, N3 next, on Native.** Switch Settings › System health › Desktop engine to Native and restart, then: N1 the
   Media Player's ⛶ on a video, N2 Notepad › Open, N3 Task Manager with Notepad and Settings open. The fixes are all in
   `main` before 1.6.2.261124, so anything still reproducing here is a `breaks` item that outranks the rest.
2. **V60–V63** with the owner: the entries above are the read-only half of V60, and V61–V63 need the Secure Boot and
   start-up pages. Never the firmware's Key Management, F9, or anything that clears keys. V59 stays available as the
   repair for a start that lands in the setup screen.
3. Re-read the checklist against **1.6.2.261124** first: it was written for 1.6.0, and `314e319b` (Native's
   transitions, 2026-11-25) is newer than the installed build, so V47 and V66 must not be judged against `main`.
4. Take the screenshot **before** leaving each item, and name it `V##-native.png`.
