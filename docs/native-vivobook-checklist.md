# Native vs Standard: checks for the owner on the Vivobook (1.6.0)

The owner's rule: "Rudolf OS on the native desktop should appear and function exactly as it does in Chromium mode." These
are the checks that need the Vivobook's own hardware (or a long real session) and could not be made on a host or in QEMU
(2026-11-16, docs/native-findings-2026-11-15.md, "Status on 2026-11-16"). Each line: what to do on Native, what Standard
shows for the same steps, and what to report. Do each on Standard first if unsure (Settings › System health › Desktop
engine, then restart), and report any difference with a photo of both.

## Sound, camera, microphone and calls

| # | Do (on Native) | Standard shows | Report |
|---|---|---|---|
| V1 | Sound Recorder: record 10 s, stop, play it back | The 🎙️ in the taskbar while recording; the recording plays at the same level | Whether 🎙️ shows, the recording's length, whether it plays and is heard |
| V2 | Camera: open it the first time | "Allow access?" centred over the dimmed desktop; after Allow, the picture | Where the prompt is, whether the picture moves smoothly (frames per second in Settings › System health › Hardware check) |
| V3 | Camera: take a photo and a 5 s video | The photo and the video in Pictures and Videos | Whether both are saved and open |
| V4 | Start a call (the in-desktop call screen) and hang up | The call screen, the microphone indicator, mute | The call screen's place and size, 🎙️ and mute |
| V5 | Deny the microphone, then open Sound Recorder again | The "blocked" message and how to allow it | The same message, in the same place |
| V6 | Media Player: play a video with sound, volume keys up and down | The volume overlay at the top; the sound changes in steps | The overlay, the steps, any delay |
| V7 | With the video playing, open another app's window | Sound goes on, no click | Gaps or clicks |
| V8 | Desktop sounds (Settings › Sound › Desktop sounds on): Media Player › URL… › Add (N32) | A short sound | Whether it is heard on Native |
| V9 | Bluetooth headphones: play sound through them | Sound in the headphones; the volume overlay | Whether it switches and back when they are turned off |

## Sleep, lid, battery and power

| # | Do (on Native) | Standard shows | Report |
|---|---|---|---|
| V10 | Start › Power › Sleep, wait 1 min, press a key | The lock screen, then the desktop with every window as it was | Whether the desktop comes back without restarting (windows still open), how long |
| V11 | Close the lid for 1 min, open it | As V10 | As V10 |
| V12 | Sleep with a video playing, wake | Paused or playing as before, sound back | Sound after waking |
| V13 | Unplug the charger | The battery icon changes, the battery's percentage | The icon and percentage, the same as Standard |
| V14 | Run on battery to 20 % and 10 % (or wait for the warnings) | The low-battery notifications | Whether each comes once, where |
| V15 | Settings › Power and battery: each power profile | The profile changes; the taskbar battery's tip says it | Whether the change applies at once |
| V16 | The battery's taskbar button | Settings' System health | The same page |
| V17 | Battery saver on | Animations off (window opening, toasts) | Native's window opening animation off too |

## Bluetooth, printing, screens and touchpad

| # | Do (on Native) | Standard shows | Report |
|---|---|---|---|
| V18 | Settings › Bluetooth: turn on, find a phone or headphones, pair | The list, the pairing code prompt, Paired | Each step's look and the prompt's place |
| V19 | Unpair, turn Bluetooth off | The list empty, the switch off | The same |
| V20 | WordPro › File › Print… to a real printer, 1 page | The print dialog centred over the dimmed desktop; the page printed | The dialog, whether the page prints |
| V21 | Print to PDF from Notepad | The PDF in Documents | Whether it is there and opens |
| V22 | Plug in a second monitor (HDMI) | One desktop on the laptop's screen; what the second screen shows (mirrored or nothing) | What each screen shows on Native; the taskbar's place |
| V23 | With the second monitor: move a window towards it | What Standard does | The same or not |
| V24 | Touchpad: two-finger scroll in Settings, File Explorer, a long web page in Oracle | Smooth scrolling, the scrollbar moving | Speed and direction the same |
| V25 | Touchpad: pinch in Image Viewer and in Oracle | Zoom | The same |
| V26 | Touchpad: edge swipes (if Standard has them) | — | The same |

## Linux and Flatpak apps, games

| # | Do (on Native) | Standard shows | Report |
|---|---|---|---|
| V27 | Open a Linux app from the Debian container (e.g. a text editor) | Its window with Rudolf's title bar, a taskbar button, Alt+Tab | Title bar, button, Alt+Tab, closing |
| V28 | In that app: File › Open | Its file dialog | Whether the dialog opens and shows your files |
| V29 | A Linux app with sound | Sound | Sound on Native |
| V30 | A Flatpak app: open, use, close | As V27 | As V27 |
| V31 | App Store: download a Library game, launch it | The game's window; F11 full screen | Download, launch, full screen, sound |
| V32 | A game with a gamepad (if Standard supports it) | The gamepad works | The same |

## Updates, accounts, touch and long use

| # | Do (on Native) | Standard shows | Report |
|---|---|---|---|
| V33 | Settings › Update and recovery: Check for updates (beta box ticked) | The newer beta, Download, Restart | Each step; after restarting, Native again |
| V34 | Untick the beta box, check again | "Up to date" or the release | The same message |
| V35 | Add a second account, sign out, sign in as it | The new account's own desktop and files | Its own settings and files; the Linux user is its own (Part 10) |
| V36 | Switch user from the start menu | The sign-in screen with both accounts | The same; each account's windows kept apart |
| V37 | Lock (Super+L) with each account | The lock screen with that account | The same |
| V38 | Touch mode on: type into Notepad, Oracle and a Linux app with the on-screen keyboard | The keyboard over the bottom; typing reaches the app | Whether it reaches each, the keyboard's place |
| V39 | Touch mode's title bars | Taller title bars | The same: title bars at least 40 px tall, their buttons 40 by 40 (done in #410: rudolf-native's touch mode sizes) |
| V40 | Night light on | The whole screen tinted | The same: the whole screen tinted, Linux apps and the taskbar too, and only from 8 PM to 7 AM when scheduled (done in #410: the shell sets the screen's gamma) |
| V41 | Use Native for 30 minutes of mixed use (Oracle, videos, Settings, File Explorer, notifications) | No restarts, no repeated start-up notifications | Any restart, blank widgets, repeated notifications (tasks 20:10-20:25) |
| V42 | Memory at idle and with one app (Task Manager) | About 900 MB or less at idle | The numbers on both engines (N19) |
| V43 | Oracle (Chromium on Native): focus on opening, Ctrl+T (N31) | The search box focused; Ctrl+T opens a tab | The same |
| V44 | A video's statistics (any site showing frames, or the hardware check's camera frame rate) | Frames counted to the end (N16) | The numbers on both |
| V45 | Super+Tab with a video playing | The video keeps moving in the overview | The same (fixed 2026-11-16) |
| V46 | A notification while a video plays under its corner | The video moves round the notification | The same (fixed 2026-11-16) |
| V47 | Open any app | The window fades in and grows in 0.18 s | The same; smooth with real drawing (fixed 2026-11-16) |
| V48 | Drag a file from File Explorer to Notepad and to Trash; copy text between Notepad, Oracle and a Linux app | Dropped and pasted | The same |
| V49 | Screen saver: wait for it, move the mouse | The saver, then the lock if set | The same |
| V50 | Accessibility: magnifier, screen reader, high contrast | Each works over the whole screen | Native's (plan item 10) |
| V57 | Drag a file from a Linux app's file manager (Debian's, or a Flatpak's) onto the desktop, onto a desktop folder, onto the Trash, and onto Notepad's taskbar button and into Notepad (#444, kept in 1.6.0 on 2026-11-16) | Copied to the Desktop folder / the folder; the Trash; Notepad opens it | The same, the icon on the desktop at once |

## Background work: alarms, reminders and notices with no window or several open (Part 12, item 12)

Fixed on 2026-11-17: on Native each app window ran the desktop's background work itself, so with no window open an alarm
did not ring and with several open it rang once per window. One unseen page of the desktop's now runs it once.

| # | Do (on Native) | Standard shows | Report |
|---|---|---|---|
| V51 | Clock › Alarms: an alarm two minutes ahead; close every window; wait | The alarm rings once (its notification and sound) | Whether it rings with no window open; how many notifications and how long the sound |
| V52 | Calendar: an event three minutes ahead, Remind me "At the start"; close every window | The reminder once; its click opens Calendar | Whether it shows, how many times, what its click opens |
| V53 | With no Rudolf window open: a call in a Linux app (Zoom's app), or a text message or call to the computer's SIM | The 🎙️ in the taskbar; a message's notice once | Whether 🎙️ and the notice show with no window open, and how many times |
| V54 | Open two windows (Notepad and Settings), then an alarm a minute ahead | It rings once | How many notifications and sounds (it was one per window) |
| V55 | Clock › Timers: start a 1 minute timer, close the Clock's window | The timer rings once | Whether it rings after the window is closed; open Clock again: the timer says Done |
| V56 | Task Manager with only it open | No extra window or app listed | Whether anything named Rudolf OS or do-svc shows on the taskbar or in Task Manager's Apps |

## The start-up after 2026-11-16's restart ("Before stable" 15; docs/plan-1.6.0.md)

The Vivobook opened its firmware's setup screen at the restart after the engine switch. The cause: 1.6.0's automatic
repair wrote a new boot image over the drive's start-up area at that restart, and the firmware's two entries for the
drive ("Rudolf OS" and "Drive") still name where 1.4.1's start-up files were, CDROM(1,0x52EC,0x1E000); they are now at
CDROM(1,0x5AC4,0x36000). Your files and the installed system are not touched (the stick starts them). Do not use the
firmware's Key Management, F9 or anything that clears keys: none of it is needed.

| # | Do | Expected | Report |
|---|---|---|---|
| V58 | First, a restart without the stick (nothing to lose) | Most likely the setup screen again: 2026111620 started from the stick does not correct the entry (found in QEMU) | Whether it starts or opens the setup screen |
| V59 | If it opens the setup screen: Boot › Add New Boot Option, as on 2026-10-29: a name ("Drive 2"), the KINGSTON drive, the file `EFI\BOOT\BOOTX64.EFI`; Save & Exit | Rudolf OS starts from the drive (without the stick); at that start rudolfd takes the new entry's path for "Rudolf OS", so the next starts work too | Whether it starts; Boot Override's list now |
| V60 | Started (from the drive or the stick): Settings › Security › Secure Boot › Get new boot files… (just open it, then Cancel), or a system report's "start-up decisions" | "Last time: written 2026-11-16T…: Rudolf OS 1.6.0.261116 (serial 2026111620)", or "boot files written" on 2026-11-16 | A photo: this confirms the cause on the Vivobook |
| V61 | After the beta with the fix (bootentry-160): start once (from the drive, or the stick if the drive does not start yet) | System health shows "Start-up files and firmware" lines: the "Rudolf OS" entry written for CDROM(0x1,0x5AC4,0x36000), the old one kept as "Rudolf OS (previous)"; the next start without the stick works, and the previous one is removed | Photos of those lines |
| V62 | Settings › Update and recovery › Start-up of this drive › Check this drive's start-up… (your password) | "All is right: the computer can start from this drive", the files signed, the entry first in the boot order | A photo of the report |
| V63 | Same place › Put back the official start-up files… (on power) | "already the image's: nothing to change" (the drive has 1.6.0's files now); on a drive with 1.4's files it says there is no room and changes nothing | What it says; then restart without the stick |
