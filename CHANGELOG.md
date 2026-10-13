# Rudolf OS changes

Each version's first line after "Summary:" is what Settings shows before an update is installed; the list is what
"What's new" and the notification after an update show. The newest version comes first.

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
