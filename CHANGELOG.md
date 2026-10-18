# Rudolf OS changes

Each version's first line after "Summary:" is what Settings shows before an update is installed; the list is what
"What's new" and the notification after an update show. The newest version comes first.

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
