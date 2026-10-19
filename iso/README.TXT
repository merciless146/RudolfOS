RUDOLF OS - READ ME
====================

Rudolf OS is a desktop operating system that starts straight from this disc
(CD, DVD or USB stick). It runs "live" without touching the computer's hard
disk, and can optionally be installed.


WHAT YOU NEED
-------------
* A 64-bit PC (x86-64: Intel or AMD), made roughly 2010 or later.
* 2 GB of memory (4 GB recommended).
* Graphics that support VESA (older PCs) or UEFI GOP (newer PCs). Almost
  every graphics card does.
* A CD/DVD drive, or a USB port and a USB stick of 512 MB or more.
* UEFI computers: Secure Boot can stay on (Rudolf OS 1.2.2 and later). The
  first time, the computer asks you to enroll the Rudolf OS key once: see
  SECURE BOOT below.


MAKING A BOOTABLE DISC OR STICK
-------------------------------
The file rudolf-os-VERSION.iso (for example rudolf-os-1.2.4.iso) is a
"hybrid" image: the same file works for discs and USB sticks, and for both
BIOS and UEFI computers. GitHub delivers it in parts: JOIN-PARTS.TXT (and
the script join.ps1 for Windows) joins them and checks the result.

CD or DVD: burn the .iso as a disc image ("Burn disc image" in Windows,
  Disk Utility / "Burn" on macOS, Brasero/K3b/xorriso on Linux). Do not copy
  the .iso file onto the disc as a normal file. The image fits on a 700 MB CD.

USB stick (all data on the stick is erased):
  Windows: use Rufus and choose "Write in DD Image mode" when asked (the
  default "ISO Image mode" does not work), or balenaEtcher.
  macOS / Linux: use balenaEtcher, or
      sudo dd if=rudolf-os-VERSION.iso of=/dev/sdX bs=4M conv=fsync
  (replace /dev/sdX with the stick, NOT your hard disk).
  Keep the .iso file on your computer, never on the stick you write it
  to: writing erases the whole stick, including the file being read.
  Afterwards Windows may say the stick "is not accessible" or offer to
  format it: that is normal (Windows cannot read Rudolf OS's partitions).
  Click Cancel, never Format.

Check the download first: the file rudolf-os-VERSION.iso.sha256 holds the
expected SHA-256 checksum (Windows PowerShell: Get-FileHash
rudolf-os-VERSION.iso -Algorithm SHA256; Command Prompt: certutil -hashfile
rudolf-os-VERSION.iso SHA256; macOS/Linux: sha256sum -c
rudolf-os-VERSION.iso.sha256). join.ps1 does this for you.


STARTING
--------
Put in the disc or stick, switch the computer on and open the boot menu
(usually F12, F11, F10, F8 or Esc - it is shown briefly on screen) and pick
the CD/DVD or USB device.

BIOS computers show the blue "Rudolf Boot" menu (5 second countdown):
  Start Rudolf OS        normal start
  Recovery mode          starts the built-in copy of the desktop and ignores
                         updates installed from the Settings app
  Check disc             verifies every file on the disc before starting
  Screen resolution      pick a different screen mode if the picture is wrong
  Boot from hard disk    leaves Rudolf OS and starts the installed system
  Press D for diagnostic messages on screen and the serial port.

UEFI computers show a short menu (4 seconds): Start Rudolf OS, Recovery
mode, Check this disc or USB drive, Start from memory, and Debug start
(only with Secure Boot off). With Secure Boot on, the very first start asks
you to enroll the Rudolf OS key once: see SECURE BOOT below.

The first start asks for a language, a user name and a password. On a live
system nothing is kept after shutdown: see KEEPING YOUR FILES below.


KEEPING YOUR FILES
------------------
A stick written from the image runs "live": your files, settings and apps
are lost when you shut down. To keep them on the same stick, sign in and
answer "Keep my files..." when Rudolf OS offers it (or use Settings >
Storage and backup > Keep files on this drive). Rudolf OS adds a RUDOLFDATA
partition in the stick's free space after the image (at least 1 GB is
needed); restart once, and everything you made so far moves onto it. Apps
from the App Store, such as Roblox (Sober), need this (or an installed
Rudolf OS).


INSTALLING
----------
Settings > Update and recovery > "Install here..." copies Rudolf OS to
another disk (an internal disk or a second USB stick) and adds a data
partition, so your files and settings are kept there. THE WHOLE TARGET DISK
IS ERASED. Your files on the drive you started from are not copied over.
Afterwards remove the stick or disc and choose the disk in the computer's
boot menu (for example F8 on ASUS computers, F12 on many others).

"Install from" picks what is installed: the Rudolf OS on the drive you
started from, or a newer disc image (rudolf-os-VERSION.iso) you downloaded
into C:\DOWNLOADS - only images signed by Rudolf OS are accepted. Use a
1.2.2 or newer image if the computer should start with Secure Boot on.

You cannot install onto the drive Rudolf OS is running from; to keep files
on that drive, see KEEPING YOUR FILES above.

Updates of the desktop can be installed from Settings > Update and recovery;
the previous version is kept and "Roll back" restores it. "Recovery mode" in
the boot menu always starts the version on the disc.


GOOD TO KNOW
------------
* Wired network (Ethernet) works automatically. Wi-Fi: Settings > Network
  lists the networks in range; choose one, type its password, and it is
  saved and joined by itself from then on (on an installed system). WPA2,
  WPA3 and open networks work; school or company networks that need a
  user name (802.1X/"Enterprise") and old WEP networks do not yet.
  Supported Wi-Fi chips: Intel (Wi-Fi 4 to Wi-Fi 6E, e.g. AX200/AX201/AX210/
  AX211), Realtek (RTL8852BE as in the Radxa X4, RTL8852AE/CE, RTL8822/8821,
  and many Realtek USB adapters), MediaTek MT7921/MT7922/MT7925 (PCIe and
  USB), Qualcomm Atheros ath9k/QCA6174 and Ralink USB adapters. If no
  network shows up, set your country under Settings > Network > Country.
* Sound: Intel HD Audio and USB audio, through PulseAudio. Settings >
  Sound chooses the output.
* Help (in the Start menu) explains every app, keyboard shortcut and setting.
* Oracle shows most websites inside the desktop. Sites with a sign-in
  (Gmail, Outlook, Google Docs, Yahoo Mail, PayPal...) open in a real browser
  window instead, because their sign-in does not work inside another page.
  The globe button in the taskbar switches between that window and the
  desktop; Super+Esc also returns to the desktop.
* Printing (WordPro: File > Print, Ctrl+P): network printers that support
  IPP Everywhere or AirPrint (most printers from about 2013 on) are found
  automatically and need no driver; "Add a printer by address" reaches one
  the search misses. USB-only printers and printers that need a driver are
  not supported. "Save as PDF" keeps the page layout.
* Not included: e-mail over IMAP, several monitors at once.
* Built-in apps share one browser engine; apps you add from the App Store
  run sandboxed.



LINUX APPS (FROM FLATHUB)
-------------------------
App Store > Linux apps lists the desktop apps on Flathub (LibreOffice, GIMP,
Inkscape, VLC, Firefox, VS Code and thousands more).
* Rudolf OS must be installed (or the USB stick must have a RUDOLFDATA
  partition), with a network cable. The first app also downloads the shared
  parts it needs (often a few hundred MB); the App Store shows the size first.
* Only apps verified by Flathub can be installed, unless an administrator
  allows others (App Store > Installed).
* Installed apps are in the Start menu. They open above the desktop with
  their own taskbar button; click elsewhere on the taskbar, or press
  Super+Esc, to return to the desktop. Super+Tab steps through open apps.
* Documents, Pictures and Downloads are shared with the apps: files saved
  there on drive C: appear in the apps, and the apps' files appear on drive
  C:. Files > Open with lists the apps that handle a file type.
* Apps' Open and Save dialogs are Rudolf OS dialogs; links open in Oracle.
* Linux apps update themselves once a day (App Store > Installed).

LINUX TERMINAL (DEBIAN 13)
--------------------------
Start > Terminal (Linux), or Ctrl+Alt+T, opens a Linux terminal. The first
time, an administrator sets up Debian 13: about 30 MB to download and 130 MB
on the data partition (so Rudolf OS must be installed), and a "Linux
password" that Debian asks for with sudo.
* Install programs:  sudo apt install NAME   (for example git, python3,
  build-essential, gimp). Graphical apps appear in the Start menu with the
  Linux apps and open in their own windows; Files' "Open with" offers them.
* Your Documents, Pictures and Downloads are in the home folder (~).
* Debian runs in a sandbox: its root user has no rights over Rudolf OS, and
  Rudolf OS updates do not touch it. Settings > Apps > Linux (Debian) shows
  its size, updates it (also automatically once a day) and removes it.
* There is no systemd inside, so background services (Docker, Snap, most
  servers) do not start by themselves; command-line tools and apps work.

EMU: GAME BOY, SUPER NINTENDO AND NINTENDO DS
--------------------------------------------
Emu (in the Start menu) plays Game Boy, GBA, SNES (.sfc, .smc) and DS
(.nds) games that you own. Rudolf OS includes no games and no Nintendo firmware.
* Game library: the games live in Games/SNES and Games/DS on the data
  partition (Files > Games). Emu > Add games copies them from Downloads,
  Documents or a USB stick and unpacks .zip files; Open a file plays a game
  from anywhere without copying it. Tabs: All, Recent, Favourites and one per
  console; right-click a game to favourite, show or delete it.
* Saves: in-game (battery) saves are kept in the game's Saves folder, and
  save states (9 slots per game, with a picture) in its States folder.
* Keys (change them under Controls): arrows = D-pad, X = A, Z = B, S = X,
  A = Y, Q = L, W = R, Enter = Start, right Shift = Select, Tab = fast
  forward. Gamepads work with buttons in Nintendo positions.
* DS: the lower screen is touchable with the touchscreen or mouse. Layouts:
  stacked, side by side, or one large screen; Swap exchanges the screens.
* Borderless: the button on the toolbar or Alt+Enter fills the screen and
  hides the taskbar; Esc shows a menu (save, load, controls, leave).
* An on-screen pad appears on touchscreens (Controls turns it on or off).

BLITZEN (THE ASSISTANT)
-----------------------
Blitzen (Start > Blitzen, or Ctrl+Alt+B) helps with BlockLua, JavaScript, C++
and Python and answers everyday questions. It runs entirely on this computer:
nothing you type is sent anywhere.
* First use: an administrator presses Download. The model is kept on the data
  partition, so Rudolf OS must be installed. It is signed with the same key as
  system updates; a damaged or unsigned download is refused.
* Code in an answer has Run, Insert (Code Editor), Copy and, for Lua, Blocks
  (opens it in BlockLua Builder). With "Check code by running it" on, Blitzen
  runs the code and, if it fails, fixes it from the error (at most twice).
  BlockLua and JavaScript run in the desktop; Python and C++ run in Linux
  (Debian) with no network and a 10-second limit (Blitzen offers to install
  python3 or g++ the first time).
* "Ask Blitzen" in Notepad, Code Editor and the Terminal sends what you are
  looking at; "Make blocks..." in BlockLua Builder turns a request into blocks.
* Chats are kept in Documents > Blitzen. Settings > Apps > Blitzen shows the
  model, memory and speed, unloads or removes it, or turns Blitzen off.
* Blitzen uses about 1 GB of memory while it answers, does not start while a
  game is running, and stops by itself after 10 minutes without questions.
* Lua now runs without the internet too (Fengari is part of the desktop).

ROBLOX (THROUGH SOBER)
----------------------
Rudolf OS can download and run Sober, an unofficial Roblox player for Linux
made by the Vinegar team (not by Roblox Corporation; use at your own risk).
* Rudolf OS must be installed on the computer (or the USB stick must have a
  RUDOLFDATA partition), with a network cable and about 2.5 GB free.
* App Store > Roblox (Sober) > Get. The download is about 1 GB from Flathub.
* Open "Roblox" from the Start menu. Super+Esc returns to the desktop.
* Games and Linux apps run on the graphics card (Intel, AMD or NVIDIA)
  through Xorg, with sound through the Rudolf OS sound server. NVIDIA
  GeForce GTX 16 / RTX 20 series and newer use NVIDIA's GSP firmware and
  Mesa's NVK driver (1.2.3); older NVIDIA cards have little or no
  acceleration with the open driver.
* Settings > Display > Graphics card chooses how the graphics card is used.
  If it cannot be used, Rudolf OS falls back to the software display, where
  games are slow. If the screen goes black, choose "Recovery mode".

AUTOMATIC SECURITY UPDATES
--------------------------
An installed Rudolf OS (or a USB stick with a RUDOLFDATA partition) checks
once a day for a newer signed release with Chromium and Linux security fixes.
* Only the changed parts are downloaded, into a spare system slot; the
  running system is not touched. "Restart to finish" switches to it.
* If the new version fails to start twice, Rudolf OS goes back to the
  previous one by itself. Settings > Update and recovery > Roll back system
  does the same by hand. "Recovery mode" always starts the disc's system.
* Settings > Update and recovery shows the versions and the update address,
  and turns automatic downloads on or off.
* This disc gets updates from https://merciless146.github.io/RudolfOS/stable
  and accepts only releases signed by the Rudolf OS release key; others are
  refused.

PACKAGES (RPK)
--------------
rpk is the Rudolf OS package manager: command-line tools, apps and web apps
as signed, read-only images, mounted at /pkg/<name>. Installing a package
never changes Rudolf OS itself; removing it deletes one file.
* Start menu > Packages, or "rpk" in the Command Prompt (rpk help).
* This disc carries its own packages (RUDOLF/PKG), so they install without
  a network. More sources: Packages > ... > Sources and keys.
* Command-line tools work on files on drive C:, for example
  jq .name < DATA.JSON > NAME.TXT   or   cat DATA.JSON | jq .
* Running live from the disc, packages are kept until shutdown only.
* Packages are signed; Rudolf OS refuses altered or unsigned ones.

FLASH IN THE ORACLE BROWSER (RUFFLE)
------------------------------------
Oracle plays Flash content with Ruffle, the Flash Player emulator the Flash
Player app uses. Turn it on or off in Oracle > Menu > Extensions.
* Websites: the built-in "Ruffle for Oracle" extension plays Flash games and
  animations on web pages, and tells old pages that check for Flash that it
  is installed.
* This computer: .SWF files and saved web pages with Flash open in Oracle.
* Ruffle is loaded from the address in Settings > Multimedia (unpkg or
  jsDelivr by default), so Flash needs an internet connection unless a copy
  of Ruffle's self-hosted package is built into the disc image (www/lib/ruffle).

RUDOLF CYBERDECK
----------------
Rudolf OS's handheld target is a Radxa X4 (8 GB, Intel N100) with a 10.1"
1280x800 HDMI touchscreen, powered over USB-C PD at 12 V/2.5 A (30 W).
Test the bare board with Radxa's PD30W adapter and any HDMI monitor first.
Settings > Display > Cyberdeck test makes a faster PC behave roughly like
the N100's processor (not its graphics), for a first look at speed.

SECURE BOOT
-----------
With Secure Boot on, a PC starts only software signed by a key it trusts.
Rudolf OS starts through "shim" (signed by Microsoft); shim trusts the
Rudolf OS key once you have enrolled it. Do this once per computer:
1. Start from the Rudolf OS disc or USB stick. The screen says
   "Verification failed" - press Enter. A blue screen appears:
   "Perform MOK management".
2. Choose "Enroll key from disk", then the Rudolf OS drive, then the folders
   EFI, rudolf, and the file rudolf.cer.
3. Choose "Continue", then "Yes", then "Reboot".
If the blue screen says "Unsupported Format" when you pick rudolf.cer (some
firmware, with discs older than 1.2.4), choose Continue boot or switch off;
turn Secure Boot off ("OS Type: Other OS" on ASUS), start Rudolf OS, use
Settings > Security Settings > Prepare Secure Boot (it shows a one-time
password), turn Secure Boot back on, and at the next start choose "Enroll
MOK", "Continue", "Yes", type the password and choose "Reboot". The blue
screen waits only 10 seconds for a key press at first.
Rudolf OS now starts with Secure Boot on: only signed software, the kernel
locked down, and every system file checked as it is read. Already running
Rudolf OS with Secure Boot off? Settings > Security Settings > Prepare
Secure Boot does steps 1-2 for you (with a one-time password); then turn
Secure Boot on in the firmware settings.

PROBLEMS?
---------
Black screen after the menu: choose "Recovery mode" (starts without the
  graphics card driver), or "Screen resolution" and pick 1024x768.
"Verification failed" or a blue MokManager screen on a UEFI PC: enroll the
  Rudolf OS key (SECURE BOOT below), or turn Secure Boot off.
Desktop does not appear: start with D pressed in the boot menu and note the
  messages; "Recovery mode" may help after a failed update.
Files on the disc damaged: choose "Check disc".


LICENCES AND SOURCE CODE
------------------------
Rudolf OS is free software under the GNU General Public License, version 2
or later (LICENSES\RUDOLF_OS.TXT). It contains Linux, BusyBox, Chromium,
X.Org, GTK, fonts and other free software, each under its own licence
(LICENSES folder). The SOURCE folder holds the Rudolf OS source code, the
BusyBox, Flatpak, OSTree and bubblewrap sources, the exact build
configuration, and a written offer for the remaining source code
(SOURCE\OFFER.TXT).

THERE IS NO WARRANTY, to the extent permitted by law.
