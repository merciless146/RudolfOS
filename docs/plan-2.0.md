# Rudolf OS 2.0: the apps become native (added on 2026-11-13)

Asked for on 2026-11-13: the whole of Rudolf OS as built so far converted to the native desktop. The shell first
(docs/plan-1.6.0.md Part 12: everything the web desktop's shell has, Native the default), then every app rewritten as a
native program, one at a time, in this plan. 2.0 is released when the apps in waves 1 to 3 are native.

Why (1.5.9's measurements, docs/plan-1.5.9.md 12.3a): with nothing open Native uses 854 MB against Standard's 1,110 MB,
because no web page runs; each app opened today loads a WebKit page with the desktop's code (Settings: 195 MB). A native
app of the same kind is typically 15 to 40 MB.

## How a native app is built
- **C and GTK 3**, as rudolf-native: the system layer already has GTK 3, cairo, Pango and GStreamer (for WebKitGTK), so
  most apps need no new library. A library an app would add (a PDF renderer, a source-code view) is named in its wave
  with its size, and the base layer's budget is checked by the release build as for ModemManager.
- **One program per app** (`/usr/bin/rudolf-APP`), started by rudolf-native as any program, framed by its window
  manager. A small shared library, `librudolf-ui` (`iso/sys/ui/`), holds what every app needs and the page already has:
  the accent colour and its variants (acVariant, acInk: already ported to C in rudolf-native), light and dark, the
  fonts and sizes of Display Properties, the translations (the page's language files read as they are), dialogs (Open,
  Save, Confirm in Rudolf's look), the about box, and rudolfd's client (the token, HTTP, JSON: from rudolf-native).
- **Files:** a native app reads and writes the account's real home folder (opened at sign-in by rudolfd), the same
  files the web apps see, so a document opened in WordLite's web version and in its native one is one file. Paths are
  shown as the page shows them (C:\\Users\\NAME\\Documents).
- **Settings and an app's own state** go through rudolfd (`desktop/settings` and a new `apps/state?app=NAME`), never
  into the web page's storage directly, so the page (Standard and Lite, still there) and native apps stay in step
  while both exist.
- **The same behaviour:** each app's native version passes a checklist made from its web version (its menus, its
  shortcuts, its files, its settings, what its browser tests check), screenshots compared side by side under Xvfb.
- **Until an app is native, its web version opens** in a one-app WebKit window as in 1.6.0. When the native version
  passes, the start menu, the pins, file associations and Open with point to it; its web version stays in the page for
  Standard and Lite until 2.0 decides their future (below).

## The order
Small and much used first, so the way of building is proved on easy apps before the large ones.

**Wave 1: small apps, proving librudolf-ui**
Clock (alarms, timers, world clock), Calculator, Notepad, Task Manager (from rudolfd's process list, as now),
Image Viewer, Screenshot, Characters & Emoji, Font Viewer, Trash.

**Wave 2: the everyday tools**
File Explorer (its drag and drop with the shell's desktop and windows), Media Player (GStreamer), PDF Viewer (with
printing, as 1.5.9's: a PDF renderer is the wave's one new library, poppler-glib, about 5 MB, budget checked), Archive
Manager, Command Prompt and Terminal (VTE if it fits the budget, else a terminal of our own on a pty), Code Editor,
Event Viewer, Calendar, Contacts, Weather, Voice Recorder, Camera, Help.

**Wave 3: the large apps**
Settings (every page, Search in Settings with its index, Display Properties' previews drawn natively), WordLite,
WordPro, SpreadsheetPro, PresentPro, Paint, App Store and Packages, Developer Console, Blitzen, Phone screen.
Settings goes first in this wave: it is opened most, and its page costs the most memory today.

**Staying web pages (decided per app, not rewritten by default)**
- **Oracle Browser:** a browser is a web engine; it stays Chromium (Standard) or WebKit, framed by the native window
  manager, its tabs and menus already its own.
- **The games and creative tools built on the web:** Board Games, Card Games, Emu, Flash Player, Quake Engine,
  GPU Lab, WOS Galaxy Map, BlockLua Builder, and the screen savers: they draw with canvas and WebGL, a native rewrite
  gains little, and they open in one-app windows. Each is reconsidered after wave 3 with its memory measured.

## Each app's pull request
One app per pull request (a large app may take several, each one working), based on main, the owner merging: the
program, its host test under Xvfb (`iso/qt/native/APP_t.sh`: opens, its checklist, screenshots), the start menu and
associations switched once it passes, a CHANGELOG line, and its memory at idle with a file open measured against its
web version (the PR says both). The release build compiles every native app and fails if one is missing from the base
layer or over its budget.

## What 2.0 decides when wave 3 is done
- Whether Standard and Lite (the web desktop) stay in the system layer as fallbacks, or move to a recovery-only role,
  or go. Until then they stay, and Native gives up to Standard as in 1.5.9.
- Whether the web desktop is kept as the browser build (Rudolf OS in a browser tab) and the phone web app, or retired.
- Wayland (docs/plan-1.5.9.md 12.2: X11 for now): decided with the phone's shell.

## Measurements that close 2.0
On the Vivobook, at idle and with the five most used apps open: memory used, start to the desktop, power on battery;
against 1.5.9's table. Target: idle under 700 MB used, five apps open under 1 GB.
