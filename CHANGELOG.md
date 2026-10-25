# Rudolf OS changes

Each version's first line after "Summary:" is what Settings shows before an update is installed; the list is what
"What's new" and the notification after an update show. The newest version comes first.

## 1.2.7 (2026-10-26)
Summary: more USB Wi-Fi adapters work; Luau and Pilot.lua checks are ready straight away; Files copies whole folders from a USB drive
- New: more USB Wi-Fi adapters work: Atheros AR9271 and AR7010 (TP-Link TL-WN722N v1 and many others), MediaTek MT7601U, MT7610U and MT7612U (among them Alfa AWUS036ACM). Plug one in and it appears in Settings › Network.
- Changed: Blitzen and Code Editor get the Luau type checker ready in the background as soon as Luau or Pilot.lua is chosen, or a .LUAU file is opened, so the first check no longer waits for it to load (up to two minutes on a slower computer). Switching between Luau, Roblox and Pilot.lua scripts no longer loads it again.
- New: Files › a USB drive or memory card › Copy into Rudolf OS copies whole folders, with everything in them, into Downloads. It shows how far it has got and can be cancelled; files over 256 MB and files that cannot be read are left out and listed; a folder that does not fit is refused before anything is copied.

## 1.2.6 (2026-10-25)
Summary: Check this disc no longer reports a good disc as damaged; Make a Rudolf OS USB stick works with a stick that is already formatted
- Fixed: "Check this disc or USB drive for errors" (in the start menu) said every disc and stick was damaged, because of one file (boot/rboot.bin) that is changed while the disc image is written. A 1.2.6 disc or stick passes. The result now says how many files were checked and names the damaged ones (Settings › Update and recovery shows the last result).
- Fixed: on a computer with an update installed, the check ran twice (once before the update started, once after) and the first result was lost. It now runs once. A disc or stick made before 1.2.6 still runs its own check first and shows the false error there; the second check, from this update, gives the right answer and is the one Settings shows.
- Fixed: Make a Rudolf OS USB stick and Install here refused a USB stick that Rudolf OS had opened by itself when it was plugged in (any stick formatted in the shop), with "is in use. Unmount its partitions first". Rudolf OS now closes the drive itself, and nothing on it is opened again while it is written. If a file on it is still open, the message says to close File Explorer or the app using it.
- Changed: the erase question before making a stick names what is on the drive now (its name and format).

## 1.2.5 (2026-10-23)
Summary: make a Rudolf OS USB stick from Rudolf OS; check this computer's hardware; apps over Wi-Fi; a Games page; Restart into Windows; Blitzen writes Luau and Pilot.lua
- New: Blitzen has three more languages. Lua is plain Lua 5.3, without the BlockLua rules. Luau is Roblox's Lua (types, continue, +=, string interpolation): Blitzen checks the types with Luau's own checker, then runs the program on this computer. Roblox game scripts are checked but not run, since they need Roblox. Pilot.lua is for Waste of Space microcontrollers: every part call is checked against Pilot.lua's definitions, then the program runs with stand-in parts that say what they were asked to do.
- New: when Luau code has a type error, Blitzen sees the error and its line and fixes the program, as it already did for programs that fail when run.
- New: Code Editor opens .LUAU files and runs them (Luau and Pilot.lua), and "Ask Blitzen" picks Luau or Pilot.lua for them.
- New: Linux apps and Roblox (Sober) install over Wi-Fi, not only over a network cable. Before a download of more than 500 MB over Wi-Fi the App Store says how much it is and how much space is free, and asks.
- New: Settings › Network › Saved networks: mark a network Metered (a phone hotspot, a capped plan) and the App Store asks before any download over 50 MB on it.
- New: an app download that stops (the Wi-Fi drops, the router restarts) is tried again by itself, and Get continues where it stopped instead of starting over.
- New: App Store › Games: Roblox, Steam, Minecraft (Prism Launcher), RetroArch and the games built into Rudolf OS, each with its download size and whether it needs the graphics card. When Rudolf OS draws with the processor, the page says which games will be slow and why.
- New: Settings › Storage and backup › Make a Rudolf OS USB stick: puts this Rudolf OS (with its installed updates), or a downloaded image, on a USB stick or memory card of 2 GB or more. The copy is read back and checked, the stick keeps files from its first start, and it can take your files and settings with it. No Windows, Rufus or joining of parts is needed any more.
- New: Back up to a USB stick (Settings › Storage and backup): the backup is written straight to a plugged-in drive, with a checksum, and read back. On a new Rudolf OS, the first-start screen has Restore from a backup on a USB stick, which finds it there and refuses a damaged one.
- Changed: Install here reads the copy back and compares it before it finishes.
- New: Task Manager › Hardware › Check this computer…: a guided check of about a minute: sound (a tone: did you hear it?), microphone (a level meter), network (wired speed with the cable hint, Wi-Fi networks in range, a download from the update server), USB (plug in a stick), graphics (what draws the screen, frames per second) and, if you like, sleep for 10 seconds. The results go into the system report, so a problem report shows them (nothing personal is in it).
- Fixed: joining a Wi-Fi network for the first time after starting could fail with "out of range, or the password is wrong" on slower computers, because Rudolf OS gave up after 20 seconds while it was still scanning. It now waits up to a minute.
- New: Restart into Windows (in the Power dialog and in Settings › Update and recovery) on a computer that also has Windows: the computer starts Windows Boot Manager once, then comes back to the Rudolf OS menu as before. It works with Secure Boot on.
- New: Settings › Update and recovery › Boot menu: choose what the UEFI boot menu starts when nobody chooses (for example Start from memory) and how long it waits. The choice is kept in the computer's firmware, so the disc or stick is not changed.
- Updated: Chromium 156 and Linux 6.18.58 (security fixes).

## 1.2.4 (2026-10-19)
Summary: a USB stick written from the image can keep your files; Secure Boot key enrolment works on more computers; clearer graphics and network status
- New: a USB stick written with Rufus or Etcher can keep your files, settings and apps. After you sign in, Rudolf OS offers to use the stick's free space (Settings › Storage and backup › Keep files on this drive); restart once and everything you made so far moves onto it. App Store apps such as Roblox (Sober) need this.
- Fixed: on some computers (ASUS among them) the blue Secure Boot screen refused rudolf.cer with "Unsupported Format". The boot files now carry their lowercase names. On older discs, Settings › Security Settings › Prepare Secure Boot always works.
- New: "Install here…" can install a newer disc image you downloaded (Install from), so an older stick can install a Secure Boot ready system. Only images signed by Rudolf OS are accepted.
- New: Task Manager › System and the system report say what draws the screen (the graphics card and its name, or the processor), what web pages are drawn with, and the NVIDIA GSP firmware state.
- New: a wired network running slower than its chip (100 Mb/s on gigabit Ethernet) is pointed out, with a hint about the cable or router port.
- New: AMD processors get AMD's latest microcode at every start (security fixes such as SRSO), without a BIOS update.
- New: the disc download has join.ps1 for Windows: it joins the parts, checks them and names a missing or damaged part. JOIN-PARTS.TXT has PowerShell commands, and warns never to keep the image on the stick being written.
- Changed: Settings › Update and recovery says when an update has been kept after a successful start.
- Changed: the README covers the UEFI boot menu, keeping files, installing from a downloaded image and what to do when Windows says the stick is not accessible.

## 1.2.3 (2026-10-19)
Summary: graphics acceleration for NVIDIA GeForce 20, 30 and 40 series cards, and fixes found on a real computer
- New: NVIDIA GeForce RTX 20, 30 and 40 series (and GTX 16) cards draw with the graphics card instead of the processor. Rudolf OS now carries NVIDIA's GSP firmware and Mesa's NVK driver, which these cards need for any acceleration with the open driver. The first start after the update can take a few seconds longer while the card starts its firmware. If the card cannot use it, Rudolf OS draws with the processor as before.
- New: the firmware for Realtek Ethernet chips (the r8169 driver) is included.
- Fixed: Task Manager › System › Installed release and the system report now show the release that is running (version, serial, channel, slot).
- Fixed: Task Manager says "USB drive" or "SD card" when Rudolf OS started from one; it said "disc" for a disc image written to a USB stick.
- Fixed: saving a window as PDF no longer turns dashes, quotes and table columns into "?".
- Fixed: the start-up options in Task Manager and the system report no longer list the same options twice.
- Note: the update is larger than usual (about 75 MB) because of the NVIDIA firmware.

## 1.2.2 (2026-10-18)
Summary: Rudolf OS starts with Secure Boot on: enroll its key once, and the computer starts only signed, checked software
- New: Secure Boot. UEFI computers no longer need Secure Boot turned off. The first time, a blue screen asks to enroll the Rudolf OS key (Enroll key from disk › EFI › rudolf › rudolf.cer); after that Rudolf OS starts as before. Already running Rudolf OS? Settings › Security Settings › Prepare Secure Boot does it with a one-time password, then turn Secure Boot on in the firmware settings.
- New: UEFI computers have a boot menu: Start, Recovery mode, Check this disc or USB drive, Start from memory, and Debug start (only with Secure Boot off).
- Security: every system file is checked as it is read (dm-verity) against the signed release list, with Secure Boot on or off; a damaged or changed layer is caught.
- Security: with Secure Boot on, the kernel is locked down: only signed drivers and signed updated kernels start, and nothing can write to kernel memory. An update whose files fail the check is dropped and the previous system starts.
- New: Task Manager › System and the system report show whether Secure Boot is on, the lockdown and whether the system layers were checked.
- Note: to use Secure Boot, write the 1.2.2 disc image to the USB stick or disc; an older stick keeps working with Secure Boot off. Computers that started in BIOS (legacy) mode are not affected.

## 1.2.1 (2026-10-18)
Summary: download files of any size from websites, up to 4 GB by default; large files stay on the disk and open from C:\DOWNLOADS
- New: downloads are no longer limited to 1.5 MB. Files up to 1.5 MB go into C:\DOWNLOADS as before; larger ones are kept on the disk (in the Downloads folder that Linux apps see) and C:\DOWNLOADS shows them with 💽 and their real size.
- New: videos, music, pictures and PDFs kept on the disk open in Rudolf OS and are read in pieces, so a large video starts at once. Text files up to 4 MB open read-only. Other kinds of large files can be opened in Linux apps.
- New: Settings > Network > Downloads > Largest download (100 MB to 16 GB, 4 GB at first). about:downloads shows the progress of a download and what was checked.
- New: Settings > Storage lists the downloads kept on the disk and how much space is free. Rudolf OS warns when the disk is over 90% full; running live, downloads are kept in memory until you shut down.
- New: PDF, ZIP and other archives, more video and audio formats, fonts and subtitles are recognized file types.
- Security: every download still goes through the file-type rules and the antivirus. For files over 8 MB the antivirus checks the first and last 4 MB, and about:downloads says so. Program files wait in a quarantine folder until an administrator approves them. Files kept on the disk are never run as pages of the desktop.
- Changed: renaming or deleting a large download in C:\DOWNLOADS does the same on the disk (deleting it keeps it in the Trash until the Trash is emptied). Large downloads cannot be copied elsewhere in C: (files up to 1.5 MB can). Backups contain a note of them, not the files.
- Changed: Chromium's own Safe Browsing check of downloads is off; it needed a connection to Google and held downloads in place. Rudolf OS's antivirus checks them.

## 1.2.0 (2026-10-18)
Summary: Oracle's tabs are real browser tabs, so signing in to websites works inside Oracle; a new Task Manager shows the kernel, the services and the hardware; report a problem with the computer's status in one step
- New: Oracle opens websites as real browser tabs. Signing in works inside Oracle for every site (Google, Microsoft, banks…), pop-up sign-ins keep working, and each Rudolf OS account has its own cookies. The browser window for sign-in sites is no longer needed.
- New: Rudolf OS moves to its new desktop host by itself once, shortly after starting (a few seconds; your files, ROMs, game saves and settings move along). If it cannot start, the desktop runs as before. Settings > Oracle sign-in > Real browser tabs turns it off.
- New: downloads from websites go to C:\DOWNLOADS through the firewall, file-type and antivirus checks, also files that need you to be signed in.
- New: a website or a video can go fullscreen; Esc brings it back. Games can capture the mouse pointer.
- Fixed: Ctrl+W, Ctrl+Q, Ctrl+T and Ctrl+N could close or quit the desktop or open a browser window over it. Ctrl+W now closes an Oracle tab, Ctrl+T opens one, and Ctrl+Q does nothing harmful. Linux apps get these keys unchanged.
- New: Task Manager: Processes (every process grouped by what it is, with CPU and memory, and End process), Performance (CPU, memory and network), System (Rudolf OS, the desktop's host, the services with Restart, the kernel, its drivers and warnings), Hardware (processor, graphics, display, storage, network adapters, sound, input devices, battery). Apps lists Oracle's real tabs with the memory each page uses.
- New: Report a problem (Task Manager, or Start): opens a ready-filled report on the Rudolf OS issue tracker on GitHub, with the state of the computer at that moment. It leaves out your name, files, web and network addresses and Wi-Fi names; you review it before sending, or save it to send later.
- Security: ending a system process or restarting a service asks for an administrator's password; the processes Rudolf OS needs cannot be ended. Websites in Oracle keep their own protection against being shown inside other pages.
- Fixed: with the newer Chromium that security updates bring, Rudolf OS's browser settings were not applied, so the system's own file dialog (which froze some PCs) could open again, and real tabs could not start. Both work again.
- Known limits: a few websites forbid the script that makes fullscreen work and stay in their window; downloads are limited to 1.5 MB as before.

## 1.1.2 (2026-10-13)
Summary: signing in to websites works (Oracle moves sign-in pages to a real browser window); choose the screen resolution and frame rate; fixes for small screens and display scaling
- Fixed: signing in to websites in Oracle did not last: the site accepted the password, then asked again. Oracle shows websites inside the desktop, where Chromium drops their sign-in cookies. Sites that need a sign-in now move to a real browser window (🌐 in the taskbar switches between it and the desktop).
- New: Oracle notices when a page asks for a password and offers to open it in the browser window, or does it by itself when you send a sign-in or press "Sign in with Google / Microsoft / Apple…". Nothing you type is read.
- New: Settings > Oracle sign-in: your sign-in sites, and whether Oracle asks first. Google, Microsoft, Yahoo, banks and payment sites always open in the browser window.
- Security: sign-in sites, banks and payment sites keep their own protection against being shown inside other pages.
- New: Flash content plays in the browser window too (Ruffle).
- New: Settings > Display > Resolution: Automatic, 1920×1080, 1366×768, 1280×720 or 1024×576. On a screen of another shape the picture keeps its shape, with black bars, and is scaled smoothly; touchscreens still touch the right spot.
- New: Settings > Display > Frame rate: 120, 90, 60 or 30 fps. 90 and 120 need a monitor that offers them and the graphics-card display; 30 saves power.
- New: after changing the resolution or frame rate, "Keep this display setting?" — the previous setting comes back by itself after 15 seconds, or after 90 seconds if the screen shows nothing.
- Fixed: with display scaling above 100%, the Start menu ran off the top of the screen, and windows, the Calculator and the file picker could be larger than small screens such as 1024×576.

## 1.1.1 (2026-10-13)
Summary: choosing files no longer freezes some PCs; laptop touchpads work; plugged-in devices are recognised and USB drives open in File Explorer
- Fixed: choosing a file (Files > Upload, Open a file, From computer, Import) froze the whole screen on some PCs, so the computer had to be switched off. Rudolf OS now uses its own file picker, which lists Downloads, Documents, Pictures and every drive.
- Fixed: the touchpads of most laptops did not work, because drivers they need were missing (Intel LPSS, the Intel and AMD GPIO controllers, ELAN, Synaptics and Elantech).
- New: touchpad gestures: tap to click (two fingers for a right click, three for a middle click), two-finger scrolling up, down and sideways, and a right click in a clickpad's bottom-right corner. Taps are ignored while you type.
- New: Settings > Mouse and touchpad: pointer speed, acceleration, tap to click, natural scrolling and ignoring the touchpad while typing.
- New: plugged-in devices are recognised. A notification names each USB device (keyboard, mouse, game controller, camera, audio device, printer, Wi-Fi adapter and more) and says when Rudolf OS has no driver for it.
- New: USB drives and SD cards open by themselves in File Explorer's Drives list; the notification opens them. ⏏ Eject makes a drive safe to unplug.
- New: Settings > Devices lists the connected drives, USB devices, keyboards, mice and touchpads.
- New: after an update, a notification tells you what changed, and "What's new" shows every version's changes.
- Fixed: game controllers no longer move the mouse pointer.
- Security: drives are mounted without device files, set-user-ID or programs run from them, and websites cannot open the system's file dialog.

## 1.1 (2026-10-11)
Summary: the first public release
- Boots on real computers: Rudolf Boot (BIOS) and UEFI, from DVD or USB; install to disk with saved data; recovery mode, updates with rollback, factory reset.
- Task Manager, App Store with sandboxed apps, Code Editor, Image and PDF viewers, Screenshot and screen recording, Calendar, Clock, Spreadsheet, Presentations, Contacts, Weather, Characters & Emoji, Event Viewer, Camera, Voice Recorder, Font Viewer, Developer Console.
- Alt+Tab, workspaces, overview, Start search, notification centre, quick settings, calendar flyout, pinned and grouped taskbar buttons, desktop files and widgets, resize from any edge.
- New File Explorer with tabs, split view, preview, thumbnails, search inside files, undo, ZIP, Open with and drives.
- Encrypted home folders, stronger passwords, lock screen, administrator prompts, guest account and parental controls, backups.
- Screen reader, magnifier, dark and high-contrast themes, scaling, keyboard and mouse assistance, input methods, Arabic and Hebrew.
- Emu (Super Nintendo and Nintendo DS), Blitzen (offline coding assistant), Linux apps from Flathub and a Debian 13 terminal.
