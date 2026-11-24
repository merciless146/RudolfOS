# Rudolf OS 1.6.3: the native Office apps and fingerprint readers

1.6.3 follows 1.6.2. The owner, 2026-11-23 18:20 and 18:23 CST: "Push fingerprint scanners to 1.6.3" and, for the Office
apps' native versions, "Move them to 1.6.3 so we can expedite the rollout of 1.6.2 stable on the beta build that includes
native settings." 1.6.2 goes stable from the beta that carries native Settings (docs/plan-1.6.2.md Part 39); these two
parts start after it.
The owner's rule for the native desktop holds for every part: "Rudolf OS on the native desktop should appear and
function exactly as it does in Chromium mode." Chromium and Lite keep the web versions of every app unchanged.

## Parts

### Fixes
- **Oracle's status line stays one line** (the owner, 2026-11-23 19:04 CST, with photos: a long "Goes To:" address wrapped
  onto a second line and the page above it jumped; 19:08 CST: "Add it as the first 1.6.3 fix"): PR #677.

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
- **The Vivobook's reader:** (checked read-only on 2026-11-23 at 18:35 CST; filled in below).
- **The owner's checklist:** enrol a finger; the lock screen and an administrator prompt with it; five wrong fingers,
  then the password; after a restart the password is asked.

## Steps
- Part 1: after 1.6.2 is stable; WordPro first.
- Part 2: after 1.6.2 is stable.
