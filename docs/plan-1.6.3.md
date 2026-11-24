# Rudolf OS 1.6.3: native Settings, the native Office apps, fingerprint readers, Oracle's memory, Oracle's encrypted websites in the page

1.6.3 follows 1.6.2. The owner, 2026-11-23 18:20 and 18:23 CST: "Push fingerprint scanners to 1.6.3" and, for the Office
apps' native versions, "Move them to 1.6.3 so we can expedite the rollout of 1.6.2 stable on the beta build that includes
native settings", then (18:27 CST) "Push settings to 1.6.3 as well." So 1.6.2 goes stable without the native Settings switched
on, and these parts follow it.
The owner's rule for the native desktop holds for every part: "Rudolf OS on the native desktop should appear and
function exactly as it does in Chromium mode." Chromium and Lite keep the web versions of every app unchanged.

## Parts

### Part 1. The Office apps as native programs (moved from 1.6.2 Part 39, item 4)
WordPro, SpreadsheetPro and PresentPro, about 130 MB each today as web windows (1.6.2 Part 38's readings), rewritten as
native programs on librudolf-ui as the native Task Manager (1.6.2 #660) and native Settings (1.6.2 Part 39) were: each
its own /usr/bin program (rudolf-wordpro, rudolf-sheets, rudolf-present), its checklist made from the web app's code and
tests, its windows screenshot-compared with the web version in Chromium mode (under 2 % different, light and dark, two
accent colours), its memory at idle against the web version, built in the release build within the base layer's budget.
- **The order:** WordPro first, then SpreadsheetPro, then PresentPro; one PR per app, or per part of an app when it is
  too large (WordPro: the editor and its toolbar, then styles, tables, pictures, then print and import/export).
- **The files:** documents open and save exactly as the web apps do (the same formats, the same rudolfd requests); a file
  saved by the native app opens unchanged in the web one and back.
- **The switch:** Native opens an app's native version only once that whole app passes (never half native, half web);
  until then the web app stays. Chromium and Lite keep the web apps.
- **Expected:** about 15-60 MB each instead of ~130 MB (as Task Manager went from ~130 to ~15 MB of its own), so the
  three open together free roughly 200-350 MB.
- **The owner's checklist:** each app on the Vivobook against Chromium mode's (every menu and toolbar button, a real
  document, spreadsheet and presentation of the owner's, printing), light and dark; the memory in Task Manager.

### Part 2. Fingerprint readers (from the owner's question of 2026-11-23)
A dedicated fingerprint sensor is needed (in the power button, a corner of the touchpad, a pad beside the keyboard, or
a USB reader): a touchscreen or touchpad senses at a few millimetres, far too coarse for a fingerprint's ridges.
- **What it does:** a fingerprint unlocks the lock screen and answers administrator prompts; the password always still
  works. Never the first sign-in after a restart: the account's file key (1.5.6, fscrypt) is unwrapped by the password,
  and a fingerprint cannot unwrap it without keeping the key where a fingerprint opens it (the TPM, as Windows Hello:
  a later part, if ever). Never instead of two-step sign-in's second step. Five failed tries: the password.
- **Settings › Users:** an account's fingerprints added and removed there, kept with that account, never shared.
- **How:** libfprint and fprintd (about 2-3 MB), run so that only rudolfd asks them (confinement and the listeners list
  as for every daemon). A reader libfprint does not support is named plainly in Settings › Users.
- **The Vivobook has no fingerprint reader** (checked read-only on 2026-11-23, devlink 000582): a Vivobook Go E1404FA; its
  USB devices are a hub, the Realtek Bluetooth radio (13d3:3556) and the webcam (3277:0034), its I2C devices the touchpad
  (ASUP1411, a HID touchpad) and the system's buses; nothing on SPI, no reader's ACPI id, nothing in the kernel's log. So
  this part is tested on the owner's hardware only with a USB reader from libfprint's supported list
  (https://fprint.freedesktop.org/supported-devices.html), and in host tests with fprintd's virtual device
  (libfprint's "virtual_image" driver) until then.
- **The owner's checklist** (with a supported USB reader): enrol a finger; the lock screen and an administrator prompt with it; five wrong fingers,
  then the password; after a restart the password is asked.

### Part 3. Native Settings, finished (moved from 1.6.2 Part 39, item 3)
`/usr/bin/rudolf-settings` (iso/sys/apps/settings*.c), one PR per group of pages, as begun in 1.6.2 and kept there switched off:
- **Done in 1.6.2 (in main, switched off):** the window, the sidebar, Search in Settings, System, About (#665); Update and
  recovery (#672); Bring in files (#674, if merged).
- **Left:** Display, Sound, Multimedia, Keyboard, Mouse, Accessibility; Network, Mobile, Bluetooth, Oracle, Remote; Devices,
  Storage, Apps (with Part 40's two rows: Apps › Linux "Bring them in…" and System health), Notifications; Privacy,
  Security, Health, Users (the administrator and two-step prompts exactly where the web ones are); then the switch:
  the native desktop opens rudolf-settings, at the page asked for, from every place that opens Settings.
- The tests, the memory against the web Settings (~170 MB) and the owner's checklist as 1.6.2 Part 39's PRs have them.
  Chromium and Lite keep the web Settings.

### Part 4. Oracle's Chromium, its memory (moved from 1.6.2 Part 38, item 5)
On Native, Oracle runs in Rudolf's own Chromium; each tab keeps its own process and memory, also when it has not been
looked at for an hour (estimated, before measuring: ~100-150 MB with one tab, ~450-700 MB with six). Measured with one,
three and six tabs; then only settings that lower it without weakening anything: background tabs freed after a while
(they reload when chosen), caches freed under memory pressure. Never site isolation, the sandbox, or the separation of
Oracle's Tor windows and encrypted folders. Kept only with a measured saving (estimated ~50-100 MB a background tab), with
Oracle's tests passing. The owner's checklist: Oracle on the Vivobook with several tabs, the memory in Task Manager, a
freed tab coming back as it was.

### Part 5. Oracle's encrypted websites back in the page on Chromium (the owner, 2026-11-23 20:08 and 20:10 CST)
The owner: "Is it possible to have the Oracle browser in Chromium running in the same Chromium page again?", then "Yes, add
it to the end of 1.6.3."
**Today.** On the Chromium desktop, Oracle's tabs are real tabs inside the desktop page (Controlled Frames, desktop/tabs26.js),
each account in its own partition of the desktop's one shared browser profile (encrypted by this computer's TPM key, 1.6.1
Part 20). With "Keep my websites encrypted with my account" on (1.6.0 Part 22, oraenc60.js; #583), Oracle runs instead in a
Chromium of its own whose profile is the account's encrypted folder (/persist/oracle/ID, the home folder's key, locked at
sign-out), as a separate window (1.6.2 Part 29 frames it). Turning the setting off brings the tabs back into the page.
**The way to have both** (to be proven first): while the account is signed in, rudolfd mounts the account's encrypted Oracle
folder over that account's partition's storage folder inside the desktop's Chromium profile (bind mount, as Documents are
mounted over /home/rudolf for Linux apps); at sign-out the partition's tabs are closed, Chromium is made to let go of the
partition's files, then the mount is taken away and the folder locked. So the page's real tabs use the account's own
encrypted storage, and Oracle stays in the page with the setting on.
**What has to be proven before it is built:**
- Where Chromium keeps a Controlled Frame partition's files (cookies, local storage, IndexedDB, cache, service workers) and
  that all of them are under one folder per partition, with nothing of it in the profile's shared files (the network
  service's shared cookie database, the HTTP cache, Safe Browsing, preferences, history, the password store, crash reports).
- That Chromium tolerates the folder appearing at sign-in and going away at sign-out while it runs (it opens a partition's
  stores lazily and may keep them open: closing them must be forced, or the partition must be one Chromium only opens after
  the mount), and that a crash or power loss never leaves the account's websites written in the clear profile.
- Saved site passwords: whether they live in the partition or in the profile's own password store; if the latter, they stay
  in the account's own place some other way.
**If any of these fails,** the setting keeps its current meaning (Oracle in its own window) and this part is written up as
not possible, with why.
**Never weakened:** each account's websites unreadable to others and to root while it is signed out; Guest in memory only;
the TPM-keyed shared profile for accounts with the setting off; the separation of Oracle's Tor windows and private windows.
**Tests:** host (a fake profile and partitions, the mount and unmount at sign-in and sign-out, a crash in between) and on
Chromium in QEMU (sign in, a site's cookie kept, sign out, the drive read from outside: nothing of the site in the clear);
the owner's checklist: Chromium mode on the Vivobook, the setting on, Oracle's tabs in the page, signed-in sites kept after
sign-out and back in.

## Steps
- Part 3: first, after 1.6.2 is stable.
- Part 1: after 1.6.2 is stable; WordPro first.
- Part 2: after 1.6.2 is stable.
- Part 4: after 1.6.2 is stable.
- Part 5: last, after 1.6.2 is stable.
