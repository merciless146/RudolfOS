# Rudolf OS changes

Each version's first line after "Summary:" is what Settings shows before an update is installed; the list is what
"What's new" and the notification after an update show. The newest version comes first.

## 1.5.2 (2026-11-04)
Summary: the Rudolf OS Library: Quake and Emu download when you first need them, and every update is smaller
- New: App Store › Rudolf OS Library, a catalog of apps made for Rudolf OS that not every computer needs. Each shows its size, description and licence, with Get, Open, Update and Remove. The Library has its own signing key: it can add apps, but it can never change Rudolf OS itself.
- Changed: Quake's free game data and Emu's Nintendo DS player moved into the Library. They download (21 MB and 0.4 MB) the first time you ask for them, so every desktop update is about 12 MB smaller. SNES games play without a download, and Quake data you already have on drive C: stays.
- Changed: a computer that already played DS games gets Emu's DS player back by itself after the update, once.
- New: Emu's "Add games" takes Game Boy, Game Boy Color and Game Boy Advance games (.gb, .gbc, .gba, or in a ZIP file). They go to Games › GB, show under Game Boy / GBA in Emu's library, and play in the Game Boy player.
- Fixed: a ZIP file whose game had a space in its name (most downloaded games) was skipped as "no SNES or DS game inside".

## 1.5.1 (2026-11-03)
Summary: Bluetooth (controllers, keyboards, mice, headphones), and Xbox Cloud Gaming and GeForce NOW in the Start menu
- New: Bluetooth (Settings › Bluetooth), on computers with a Realtek Bluetooth chip or a USB Bluetooth adapter with one (Intel and MediaTek chips follow in an update). Pair controllers (Xbox, PlayStation, Switch Pro), keyboards, mice and headphones: put the device in pairing mode, choose Search for devices, then Pair. No code to type. Paired devices reconnect by themselves; headphones become the sound output when they connect. Notifications say when a device connects, goes away, or its battery runs low.
- New: Xbox Cloud Gaming and GeForce NOW in the Start menu and the App Store's Games tab. They open in Oracle; controllers work in them.
- New: game mode. While a game or video is full screen, notifications wait in the notification centre instead of popping up, and one message afterwards says how many came.
- Fixed: playing with only a controller did not count as using the computer, so the screen could lock or the computer sleep in the middle of a game.
- New: controllers are listed in Settings › Devices.
- Fixed: Bluetooth pairings were not kept: a device showed as paired, but after a restart it had to be paired again, and a phone asking to compare codes never got an answer. A phone's code now appears on screen to confirm (Yes / No), a device that asks to pair is asked about, a keyboard that needs a code shows it, and Settings says so when a pairing did not finish.
- Fixed: phones and other computers could not find this computer over Bluetooth. Settings › Bluetooth has Let this computer be found (for a few minutes), and Search for devices makes it visible too.
- Fixed: Wi-Fi dropped every few minutes on laptops with a Realtek Wi-Fi chip (such as the RTL8821CE) when the router has both a 5 GHz and a 2.4 GHz radio. The chip's deep power saving is now off.

## 1.5.0 (2026-11-02)
Summary: security: a firewall, encrypted data, administrators can open account files, a health log, and remote access from any browser
- New: a firewall. Other computers can no longer connect to programs on this one (Linux and Windows programs included); everything this computer reaches out to works as before. Security Settings shows how many connections it blocked. The old "Firewall" setting for web pages is now called Website blocking; it works as before.
- New: Encrypt my data (Security Settings). Debian, Linux and Windows programs, Linux apps and files kept in Downloads move into a folder the system encrypts. Signing in with your password unlocks it; Guest does not. A recovery key, shown once, opens it if every password is forgotten.
- New: administrators can open the other accounts' files (in C:\USERS) and reset an account's password without its files being lost. It is on for every account and can be turned off per account (User Settings › Files and administrators). Each account's welcome file says who can open its files.
- New: System health (Settings) records what happened on this computer: start-ups and how long they took, crashes, pages closed to free memory, updates that fell back, failed sign-ins, firewall blocks and remote sessions. It stays on this computer; Report a problem includes its latest events after you read the report.
- New: Remote access (Settings). Use this computer from a phone, tablet or another computer on the same network, in any web browser, with nothing to install. A pairing code is asked once per browser, then an account and its password. Someone at the computer allows each connection, unless an administrator lets that account connect unattended. A red bar shows who is connected, with End.

## 1.4.9 (2026-11-02)
Summary: Windows programs in a click: install Wine from Settings, and installed programs appear in the Start menu
- New: Install Wine from Settings › Apps › Linux, or straight from a Windows program that cannot open yet. No Terminal commands are needed: it downloads about 1 GB and says when it is done.
- New: Windows programs installed with Wine (games and their launchers) appear in the Start menu by themselves, with their own icons.
- Fixed: programs that expect to start in their own folder (game launchers and updaters) are started there.
- Fixed: Windows programs whose paths have backslashes (all of them) started with a wrong path from Wine's own menu entries.

## 1.4.8 (2026-11-02)
Summary: Windows programs open with Wine, and Linux apps appear in the Start menu straight away
- New: with Wine installed in Debian (Settings › Apps › Linux), Windows programs (.exe, .msi, .bat) open with it: double-click the file, or use Open with. Large downloads kept on the disk open the same way. Without Wine, opening a Windows program says how to get it.
- Fixed: a Linux app installed outside the Linux Terminal only appeared in the Start menu after signing in again or opening the Terminal. The Start menu now checks for new Linux apps each time it opens.
- Fixed: Linux apps that only open files (such as Wine's program loader) were never offered in Open with.
- Changed: the L in Rudolf now stands for Linux: Rudolf OS is the Runtime Driver Open Linux Framework (it was Lua). The sign-in screen, About Rudolf OS, VER and the Prompt say so, and so does C:\README.TXT, also on computers set up by an older version.
- Fixed: when a page filled the memory faster than Rudolf OS could move it to the compressed swap, the desktop could stall for up to half a minute before the page was closed. The page is now closed as soon as the system reports it is stalling with little memory left.

## 1.4.7 (2026-11-01)
Summary: a page that uses up the memory no longer freezes the desktop: Rudolf OS closes that page and says why
- New: compressed swap in memory. Rudolf OS keeps memory it is not using right now compressed in memory, so a computer holds about half as much again before it runs out (a 3.4 GB laptop holds about 5 GB). Task Manager shows how much is stored this way.
- New: when memory runs out anyway, Rudolf OS closes the web page using the most memory, and only that page. Oracle shows "This page was closed because it used too much memory" in its tab, with a Reload button, and a notification names the page. Before, the whole desktop could stop responding for a minute and then start again with every window and tab closed.
- Changed: if memory runs out, the desktop and what draws it are the last things the system closes.
- Fixed: Display Properties' Font list offered fonts Rudolf OS does not have. Tahoma, Verdana, Trebuchet MS, Georgia, Comic Sans MS and Lucida Console all looked the same. Every choice is now a real font, named with the font it stands in for ("Gelasio (like Georgia)"), and each one looks different. Three of them (Gelasio, Fira Sans, Comic Neue) are new in Rudolf OS. A font chosen in an older version keeps the look it had.
- Fixed: Weather and its desktop widget showed Berlin until a city was searched for, and the widget never changed afterwards. They now show the city of your time zone (System Settings), or ask for your city when the time zone does not say. The widget follows the city chosen in Weather at once, and updates every 15 minutes; clicking it opens Weather.
- New: Weather shows °F and mph for places in the United States, with a °C/°F switch, and a city name with several matches (Springfield) lists them to choose from.
- Fixed: with "Use it for games and the desktop" (Display Properties › Graphics), playing a large game in Oracle (Friday Night Funkin' on Newgrounds) could freeze a laptop with little memory and bring back the sign-in screen.

## 1.4.6 (2026-11-01)
Summary: Notey in SpreadsheetPro and PresentPro as in WordPro, with help for each app; dark pages in Oracle, and its menu easier to read
- New: Notey in SpreadsheetPro and PresentPro is the same Notey as in WordPro. He moves in held poses, a few frames a second (not a constant smooth bob), with his expressions, tricks and reactions. He can be dragged anywhere, and falls asleep when nothing happens. Hiding him in one app hides him in all three.
- New: in SpreadsheetPro, Notey checks a spreadsheet and fixes what he finds:
  - errors, explained, or shown as a blank instead;
  - numbers stored as text;
  - a formula unlike the ones around it;
  - a total that misses the last rows;
  - columns too narrow for their numbers;
  - long tables whose headings scroll away.
  He also explains the formula in any cell in words, finds the function you describe ("how do I work out an average?"), offers a total under a column of numbers or a chart for a table, and reminds you to save.
- New: in PresentPro, Notey checks a presentation:
  - slides without a title;
  - slides with too much text (he can split them in two);
  - text too small or too faint to read;
  - stretched pictures;
  - empty slides;
  - too many fonts;
  - missing speaker notes.
  He offers a theme and transitions as the deck grows, opens the themes, and rehearses the talk.
- New: dark pages in Oracle (☰ › Dark pages). Oracle's bar, tabs and menu turn dark, and so do the pages. Pages that are dark already are left as they are; pictures and videos keep their colours.
- Fixed: Oracle's ☰ menu had light grey text on a white background when the desktop used the dark theme.

## 1.4.5 (2026-11-01)
Summary: two new apps with WordPro: SpreadsheetPro, in the place of Excel, and PresentPro, in the place of PowerPoint; Shut down and Restart finish within seconds
- New: SpreadsheetPro replaces the small Spreadsheet app. It has WordPro's menus and toolbar, 702 columns by 100,000 rows, several sheets, 167 functions (with a description as you type each one), number formats, fonts, colours, borders, merged cells, conditional formatting with colour scales and data bars, sorting, filters, data validation, find and replace, comments, named ranges, charts (column, bar, line, area, pie, doughnut and scatter) that follow the data, the fill handle for series, frozen panes, printing and PDF with page setup, and templates (a budget, an invoice, a timetable, a grade book, a loan calculator). It opens and saves Excel .xlsx files, CSV files, and the old Spreadsheet's files.
- New: PresentPro replaces the Presentations app. Slides with eight layouts, twelve themes and a master (footer, date, slide numbers, logo); text, shapes, pictures (cropped, with a border and brightness), tables, charts and icons that can be moved, resized, rotated, aligned, grouped and layered, with guides that snap; speaker notes; sections and the slide sorter; transitions and animations. Presenting works with the keyboard and with a presenter remote, has a presenter view (P) with the notes, the next slide and the time, a pen and a laser pointer, black and white screens, rehearsed timings and a show that loops. It opens and saves PowerPoint .pptx files, makes PDFs of the slides, handouts and notes pages, pictures of the slides, and has templates (a talk, a lesson, a report, a photo album, a pitch). The old Presentations files open as before.
- New: the three apps work together. A range copied in SpreadsheetPro pastes as a table in WordPro and PresentPro, and a chart copies as a picture or with its data. WordPro's Insert menu has Spreadsheet table and Chart, and File › Send to PresentPro makes slides from a document's headings; PresentPro's File › Send to WordPro makes a handout. While there are unsaved changes, SpreadsheetPro and PresentPro keep a copy every minute and offer it after a crash or a power cut. The Start menu has an Office group with all three.
- Fixed: Shut down and Restart could take half a minute, and stayed at "Please wait…" for good if a step of the desktop never finished. Every app is now asked to close at once and given 5 seconds at most, the desktop's own steps 3 seconds, and the data partition is made read-only before the power goes off. About › For developers shows how long the last shutdowns took. The power button set to Shut down now does so.
- Changed: the desktop's right-click menu says "Open settings…" instead of "Change background…".
- Removed: the Cyberdeck test in About › For developers.

## 1.4.4 (2026-11-01)
Summary: the volume and brightness keys work; the clock sets itself from the internet; the taskbar shows the battery, the real network and one time; Power and battery settings; eight screen savers; a tidier Settings
- New: the laptop's volume, mute, microphone and brightness keys work (on the ASUS Vivobook Go: F1 to F5), in the desktop, in games and in Linux apps alike, with an indicator showing the new level. The taskbar's volume is now the computer's own volume, which every app follows; a brightness icon sits next to it, and Settings › Volume Controls has System volume.
- New: System Settings › Power and battery: power mode (Power saver, Balanced, Performance), battery saver (on now, or by itself at 10, 20 or 30 %), turning the screen off and sleeping after a time without input, what closing the lid and pressing the power button do, and battery care (stop charging at 60 or 80 %), each for on battery and plugged in where it applies. The settings hold from the start, signed in or not.
- New: the battery on the taskbar, next to the workspace buttons, with its level, charging, and the time left or to full. A device's battery that reports -1 % is no longer listed.
- New: Display Properties › Screen saver: on or off, start after 1 to 60 minutes, eight styles (Starfield, Clock, Rudolf, Code rain, Mystify, Bubbles, Aurora, Fireworks) with a preview, the time over it, and the password when it ends. It is drawn by the processor, and it no longer starts while a game or a Linux app is being used.
- Fixed: the clock could go back to 2016 after a restart (when the computer's own clock lost its time), and then no secure connection worked: no update check, no development link. Rudolf OS now never starts earlier than its release date, and sets the clock from the internet by itself at every start, when the network comes up and every 12 hours, keeping the result in the computer's clock.
- Fixed: the calendar, the lock screen and the widgets showed another time than the taskbar (the browser's own time zone instead of the one set in System Settings). They all show the same time and date now.
- Fixed: the network icon said "Ethernet — connected" on a laptop on Wi-Fi. It shows the real connection now: Wi-Fi with the network's name and signal, or the cable with its speed.
- Fixed: the pointer speed in Mouse and touchpad was forgotten at every restart.
- Changed: Settings is tidier. Mouse and touchpad has every pointer setting (one pointer speed, also for games); the language and the internet clock are in System Settings › Date, time and language; web search and downloads are on the Oracle page; Accessibility is grouped as Vision, Hearing and Voice control; the Cyberdeck test and updating the desktop page from a file are in About › For developers. Quick settings keeps its switches; volume, brightness and the battery have their own taskbar icons.
- Removed: unused code and programs, and two Accessibility settings that did nothing (natural scrolling and horizontal scroll speed; natural scrolling is in Mouse and touchpad).
- Changed (1.4.4 update): the taskbar has one network icon. Its menu now also shows the Wi-Fi networks in range and joins one, as the separate Wi-Fi icon did; that icon is gone.
- Fixed (1.4.4 update): on ASUS laptops F1 to F5 needed Fn for the volume and brightness (the keyboard started with Fn lock on). They now work on their own, as in Windows; Fn with them gives F1 to F5, and Fn+Esc switches.
- New (1.4.4 update): a picture of your own as the desktop background. Display Properties › Picture: choose one from your files or from this computer, and how it fills the screen (fill, fit, stretch, centre or tile). It is kept with your settings, so it stays after a restart even if the file is moved or deleted.

## 1.4.3 (2026-10-30)
Summary: games and apps that draw with OpenGL through X11, such as Oolite, start again
- Fixed: apps and games that draw with OpenGL in an X11 window (Oolite among them, and many games made with SDL) stopped as soon as they started, because the display server offered no OpenGL to them ("GLX is not supported"). It now does, with the computer's own graphics driver.

## 1.4.2 (2026-10-30)
Summary: Google sign-in works in Oracle again; fixes 1.4.1 starting without its drivers (no Wi-Fi, slow screen) when an older Rudolf OS USB stick with its own files started the computer; the Rudolf OS boot entry works on more computers
- Fixed: on a computer started through an older Rudolf OS USB stick that keeps its own files, the 1.4.1 update could start with the stick's older system instead of its own. Its drivers were then refused (each release signs its own), so there was no Wi-Fi and the screen was drawn without the graphics card, slowly. The update now starts only with its own system, from whichever drive holds it.
- Fixed: Google refused to sign in from Oracle ("This browser or app may not be secure"), because Oracle's tabs said they were controlled by automation software. They no longer do.
- Fixed: on some computers (the ASUS Vivobook Go among them) the firmware skipped the "Rudolf OS" boot entry and started the drive through another entry, such as one made in its setup. Rudolf OS now takes the path of the entry that started it from the drive, so its own entry works on the next start.

## 1.4.1 (2026-10-28)
Summary: a computer where Rudolf OS is installed starts from its own drive, without a USB stick (after Get new boot files, or a new Install here)
- Fixed: some computers (the ASUS Vivobook Go among them) could not start Rudolf OS from the drive it was installed on, only from a USB stick. Install here copied the disc image's partition table, which suits a stick but which a computer's firmware takes for an invalid table on an internal drive. Install here and Get new boot files (Settings › Security › Secure Boot) now give an internal drive a table the firmware accepts, a disk ID of its own, and a "Rudolf OS" entry first in the computer's boot order. An installed drive keeps its files; USB sticks are made as before.
- Fixed: the development link ran every earlier command again when it was switched on with another data partition (for example on a USB stick). Commands that already have a result are not run again.

## 1.4.0 (2026-10-27)
Summary: the development link: let a developer you choose run tests on this computer through a private GitHub repository you own, while you watch and allow each command
- New: Settings › Security › Development link. While it is on, a developer you choose (for example a Claude session) can run tests on this computer: run commands with full control, take screenshots, send files from it and bring files to it, and restart it. Commands and results go through a private GitHub repository you own; only people and sessions you give that repository to can use it.
- It is off unless you switch it on, which asks for an administrator's password. It ends by itself after 1, 2, 4 or 8 hours, and stays on across a restart only if you tick that. A red banner across the top of the screen shows while it is on, with the time left and End now.
- Ask first (on unless you untick it): every command waits on the screen, with its full text, until you press Allow or Refuse.
- The GitHub token you paste is checked with GitHub (a private repository it can write to), kept where only the system can read it and never shown again; revoking it on GitHub ends all access. Files in home folders are not sent unless you allow it.
- Settings shows a log of every command, its result and whether you allowed it (30 days). The system report says whether the link is on.
- Use it on a test computer only, never on one with personal files.

## 1.3.1 (2026-10-27)
Summary: fixes 1.3.0 running an older Rudolf OS USB stick's system and files, instead of the computer's own, on computers started through such a stick
- Fixed: on a computer where Rudolf OS is installed but which starts through an older Rudolf OS USB stick that keeps its own files (for example because the computer's firmware does not start its internal drive), the 1.3.0 update ran the stick's older system and files. The desktop then said, for example, "Rudolf OS 1.1", and Wi-Fi and settings were the stick's. The update now always runs from the drive that holds it, with that drive's files, whatever drive the computer started from. Nothing on either drive was changed or erased.

## 1.3.0 (2026-10-26)
Summary: App Store › Games lists every game on Flathub, with search and genres; a USB stick uses its own files on a computer where Rudolf OS is installed; crashes are reported as crashes; the Linux password can be changed
- Fixed: a Rudolf OS USB stick started on a computer where Rudolf OS is installed could run from the computer's own drive instead, with its files (on a laptop with an NVMe drive it usually did, because that drive is ready first). Rudolf OS now starts from the kind of drive the computer's firmware started (a USB stick or an internal drive), and uses that drive's files when it has its own; otherwise it works as before.
- New: when a part of Rudolf OS crashes during a task (as the app list reader did in 1.2.8), the task says so, with Report a problem, instead of guessing ("Is this computer connected to the internet?"). The system report lists such crashes.
- New: Settings › Apps › Linux › Change the Linux password… sets a new password for Debian (the one sudo asks for). Before, it could only be chosen when Debian was set up.
- Changed: updates rebuild every Rudolf OS program, so fixes to the package tool, the assistant and printing reach installed computers too.
- New: App Store › Games lists every game in Flathub's app list under "All games on Flathub", with a search box, genres (action, arcade, board, emulators, puzzles, role-playing, simulation, sports, strategy and more) and "Verified only". Installed games come first.
- Fixed: Linux apps › Games left out games that list only their genre (for example "Action") and not "Game".
- Fixed: Command Prompt said "Rudolf OS [Version 1.1]" (when it opens and for VER), and Lua's sys.version() said "Rudolf OS 1.0", whatever the version. Both now give the version that is installed.

## 1.2.9 (2026-10-26)
Summary: the App Store's Linux app list downloads again; signing in to Roblox in Sober (and web pages inside other Linux apps) works
- Fixed: Sober could not sign in to Roblox: its sign-in window stayed blank with "No such interface org.freedesktop.portal.ProxyResolver", then "Something went wrong". Linux apps from the App Store ask Rudolf OS which proxy to use for each address; Rudolf OS now answers (its own proxy from Settings › Network, or none), and says whether the computer is online.
- Fixed: App Store › Linux apps › Download the app list stopped with "Segmentation fault" and "Is this computer connected to the internet?". Flathub's catalogue has empty entries (such as an empty paragraph), and the program that reads it crashed on them.
- Fixed: release updates now rebuild the desktop portal and the app catalogue reader for Linux apps too (before, only a full build did).

## 1.2.8 (2026-10-26)
Summary: the Linux terminal and App Store apps work on computers running an installed update; new boot files for an installed computer without erasing it (Secure Boot on computers installed from an older disc); webcams work, with a camera switch in Settings › Privacy; the hardware check tests the keyboard, touchpad, camera, battery and screen
- Fixed: the Linux terminal (Debian) could not be set up or started, with "the Debian sandbox did not start: mounting /proc failed (Operation not permitted)", on every computer running an installed update (since 1.2.3). The start-up options were mounted over /proc/cmdline there, and the kernel then refuses a sandbox its own /proc. Flatpak apps from the App Store were refused the same way. Settings › Apps › Linux › Check now tests this too ("A new /proc for sandboxes").
- New: Settings › Security › Secure Boot › Get new boot files: a computer installed from a disc older than 1.2.2 can start with Secure Boot on without being reinstalled. Download the boot image (rudolf-os-X.Y.Z-boot.iso and its .sha256) from the Rudolf OS releases; Rudolf OS checks it, keeps a copy, and writes it at the next start in front of your files, which stay as they are. The installed system then starts from the installed update.
- New: Install here › Replace the boot files only: the same for another drive Rudolf OS is installed on, from a USB stick, for example to repair one whose boot files were damaged.
- New: the disc releases have a boot image (rudolf-os-X.Y.Z-boot.iso, about 27 MB) next to the disc image.
- New: webcams work (USB Video Class, built-in laptop cameras included), in the Camera app, in websites (which still ask first) and in Linux apps.
- New: Settings › Privacy › Camera: one switch that turns the camera off for everything; its driver is not loaded until it is switched on again, also after a restart.
- New: the hardware check (Task Manager › Hardware) has five more steps: every key of the keyboard (as laid out in the chosen layout), the touchpad or mouse (moving, both buttons, scrolling), the camera, the battery's health (its capacity compared with new, and its charge cycles) and the screen (one colour at a time, to spot dead pixels).
- Changed: a factory reset of a drive that has boot files only keeps its installed system (everything else is erased as before).

## 1.2.7 (2026-10-26)
Summary: a disc image downloaded in parts can be made into a USB stick without joining it by hand; making a stick over an older Rudolf OS stick works again; a check for Linux apps and the terminal; more USB Wi-Fi adapters work; a power button on the login screen; Luau and Pilot.lua checks are ready straight away; Files copies whole folders from a USB drive
- New: Make a USB stick and Install here accept a Rudolf OS disc image downloaded from GitHub in parts (rudolf-os-X.Y.Z.iso.part00, part01, ...). The parts are joined in order, checked against the .sha256 downloaded with them, and then written; a missing, short or damaged part is named and nothing is written. The joined image stays in Downloads.
- Fixed: Make a USB stick refused to erase a stick that held an older Rudolf OS ("a file on it is still open"). (1.2.6)
- Fixed: Install here and Make a USB stick with a downloaded disc image stopped at once without a message, and wrote nothing. (1.2.4)
- Changed: downloads are in one place, C:\DOWNLOADS, which Files' "Downloads" now opens. Large files are kept on the disk and shown there.
- Changed: the disc files of a Rudolf OS release (the .iso, its parts and .sha256) downloaded from the Rudolf OS releases on GitHub are no longer blocked as unknown or program files. The same names from anywhere else still are.
- New: Settings › Security › Secure Boot, Make a USB stick and Install here say when this Rudolf OS started from boot files older than Secure Boot support (a disc before 1.2.2), and how to get newer ones: make a stick from a newer disc image.
- New: Settings › Apps › Linux › Check: what Linux apps and the terminal need from this computer (the data partition, temporary files, user namespaces, Flatpak's folders, the Debian sandbox), with what is wrong. It is also in the system report. When the Debian sandbox does not start, it now says which step failed and why.
- Fixed: Flatpak apps could not be installed ("open(O_TMPFILE): No such file or directory") when Flatpak's repository had lost its tmp folder; it is made again at start-up. /var/tmp is made at start-up too.
- New: more USB Wi-Fi adapters work: Atheros AR9271 and AR7010 (TP-Link TL-WN722N v1 and many others), MediaTek MT7601U, MT7610U and MT7612U (among them Alfa AWUS036ACM). Plug one in and it appears in Settings › Network.
- Changed: Blitzen and Code Editor get the Luau type checker ready in the background as soon as Luau or Pilot.lua is chosen, or a .LUAU file is opened, so the first check no longer waits for it to load (up to two minutes on a slower computer). Switching between Luau, Roblox and Pilot.lua scripts no longer loads it again.
- New: the login screen has a power button (bottom right) to shut down, restart or put the computer to sleep without signing in.
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
