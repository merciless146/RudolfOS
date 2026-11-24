# Rudolf OS 1.6.3: the signing keys protected, native Settings, the native Office apps, fingerprint readers, Oracle's memory, Oracle's encrypted websites in the page

1.6.3 follows 1.6.2. The owner, 2026-11-23 18:20 and 18:23 CST: "Push fingerprint scanners to 1.6.3" and, for the Office
apps' native versions, "Move them to 1.6.3 so we can expedite the rollout of 1.6.2 stable on the beta build that includes
native settings", then (18:27 CST) "Push settings to 1.6.3 as well." So 1.6.2 goes stable without the native Settings switched
on, and these parts follow it.
The owner's rule for the native desktop holds for every part: "Rudolf OS on the native desktop should appear and
function exactly as it does in Chromium mode." Chromium and Lite keep the web versions of every app unchanged.

## Parts

### Part 0. The signing keys: only main can use them (the first step of 1.6.3; the owner, 2026-11-24 08:55 CST)
The owner asked how secure Rudolf's keys are, then: "Make it the first step of 1.6.3."
**Today.** The release key (Ed25519, secret RPK_SIGNING_KEY), the Secure Boot key (RUDOLF_SB_KEY; also offline with the
owner) and the Library's key (rudolfos-library's RPK_LIBRARY_KEY; also offline) are strong and have never been in a
repository, a disc or a computer. But they are *repository* secrets: any workflow run on any branch of the repository gets
them, so anyone with write access (the owner, and the Claude sessions, forbidden by CLAUDE.md but not by GitHub) could
push a branch with a changed workflow, run it on that branch and read them. Whoever holds the owner's GitHub account
holds the keys. Nothing is in hardware; a copy works anywhere.
**The change.**
1. **A GitHub environment "release"** in each repository (RudolfOS and rudolfos-library), Settings > Environments >
   New environment, with **Deployment branches: Selected branches: main** (no other branch, no tags). Required reviewers
   are optional: they would also hold the 6-hourly security releases until the owner approves, so not by default; they
   may be set for promotions later as a second environment.
2. **The signing jobs run in it** (`environment: release`): rudolf-release.yml (release), extensions.yml (build),
   blitzen-model.yml (publish); in rudolfos-library publish.yml (build) and certify.yml (certify). Done by this part's
   PRs; harmless before the environment exists (GitHub then makes it with no rules and the repository secrets still
   apply), so they may be merged first.
3. **The secrets move into the environment, and the repository secrets are deleted.** A secret cannot be read back from
   GitHub. The Secure Boot key and the Library's key have offline copies: the owner pastes them as environment secrets
   of the same names. The release key exists only as the secret, so it is **replaced**, which also gives Rudolf its key
   replacement plan:
   a. **Done 2026-11-24:** the owner made a new Ed25519 key offline (on the Vivobook's Terminal: `openssl genpkey
      -algorithm ed25519`) and keeps it offline; its public half `IcNJiy4zFkKUU3g3jalcgTQlneiDXv3E0/bbJt19cVg=`. Claude
      never saw or handled it.
   b. A PR adds the new public half to iso/trusted-keys.txt **after** the old one, and lets recovery.efi carry both
      (mkuki.py: one to four official keys); a release still signed by the old key ships it, so every computer that
      updates trusts both, Recovery Mode included.
   c. **Only the first key in trusted-keys.txt signs** (keycheck.py refuses any other, so a release a not-yet-updated
      computer would refuse can't be published by mistake). So until the switch the new key must **not** be the
      environment's RPK_SIGNING_KEY: the old repository secret keeps signing.
   d. **The switch,** once the release trusting both has been the stable release for a while (the owner decides; a
      few weeks lets computers that update rarely catch up): one PR puts the new key first in trusted-keys.txt; at the
      same time the owner adds the new key as the "release" environment's RPK_SIGNING_KEY and deletes the repository
      secret. The next release is signed by the new key.
   e. Later, once every supported release trusts the new key, the old public half can leave trusted-keys.txt.
   Developer mode and custom builds are unaffected (their own keys).
4. **The GitHub account:** two-factor with a hardware security key; no long-lived tokens beyond what each tool needs.
**Checks:** a workflow run on a branch other than main that asks for the environment is refused by GitHub (tried once
with a harmless job that only says whether RPK_SIGNING_KEY is set, never its value); the release, an extension and the
Library's publish still sign on main; a computer on the new release trusts both keys.
**Later (not this part):** signing through a hardware key or a cloud key service, so the private key can't be copied at all.

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
- Also in main from 1.6.2 (switched off): Keyboard, Mouse and touchpad (#676).
#### Status (2026-11-24, scheduled task)
- **PR 4 (branch `native-settings-5`): Volume Controls, Multimedia Settings, Accessibility.** `settings_vol.c`: the system volume
  (rudolfd's volume: Mute, the slider, followed every 5 s while the page is open), "Use one style everywhere", All desktop sounds and
  the 18 categories (each one's switch, style, volume and ▶; a working copy until Apply, saved as `snd` in bd95set:ACCT; Revert; Reset
  to defaults keeping the master volume), App volume (bd95mix:ACCT), Output, the sound card (audio?card=), the microphones as
  settings25.js reads /proc/asound, the microphone (audio/mic?name=) and its test (PulseAudio's simple API: the bars and the level as
  the page's analyser draws them), Calls' echo cancelling (sound/echo). The ▶, a style chosen and a volume let go are played by the
  native shell (`sfx K V P` asked of rudolf-native: the page's own tones and Rudolf styles, which the shell already plays for the
  desktop's sounds). `settings_mm.c`: the plugins (Installed, Enabled; the lock bar for a standard user), the folders and players (asked
  of the shell: `open explorer C:\PROGRAMS\INSTALLERS`, `open gameboy`, `open flash`), Flash (Ruffle) and its Check Ruffle download (the
  runtime's address asked over HTTPS), Game Boy, ZIP archives; Apply as mmPanelS (https:// checked, bd95mm, "installed plugin" in the
  security log). `settings_acc.c`: Pointer and cursor (the page's cursors drawn from their paths), the link to Mouse and touchpad,
  Vision (Colour vision, Screen reader, Text-to-speech with each language's voice and ▶: rudolfd's say, Japanese in kana as jaKana
  reads it), Hearing (Caption style's colours with GTK's colour chooser), Voice control, Virtual keyboard, Keyboard and Pointer
  assistance; Apply, Revert, Reset to defaults (DEFS.acc) as the page. mksettings.py reads the sounds' categories and styles, the
  accessibility defaults, the cursors, the voices' samples and the multimedia defaults from the page at build time.
  Against the web one (`qt/native/settings_t.sh`, the stand-in now answering audio, audio/mics and sound/echo): **every view of the
  eight native pages under 2 %: 0.34 to 0.81 %** (light, dark, green and pink accents; Volume 0.49-0.71 %, Multimedia 0.46-0.76 %,
  Accessibility 0.45-0.81 %); Search in Settings the same rows (Volume 31, Multimedia 27, Accessibility 42); the checklist V1-V8,
  A1-A4, MM1-MM5 17/17; the whole test 173/173; memory at idle with System open 16.0 MB RssAnon (the web one 75.1 MB); 587 KB
  stripped. rudolf-settings now asks rudolfd for volume, audio, audio/mics, audio/mic, sound/echo and say (all of which the page asks
  already; winapi.py --update).
  **Known differences (PR 4):** before Apply the sounds' changes play only in Settings' ▶ (the page also changes the desktop's own
  sounds at once); Output device lists "System default" only (the page lists its own browser outputs, for its own sounds; the
  native shell plays the desktop's sounds on the system's default output); App volume lists "system" (the page lists what made a sound
  in its own page); Visual sound alerts' flash when it is turned on, the screen reader, live captions and voice control start
  when the settings are applied (the page starts them when ticked); Multimedia's plugin files (C:\PROGRAMS\INSTALLERS' launchers) are
  written by the desktop's page when it next starts (mmEnsure), and a removed plugin's launcher stays until then; Emu and Flash
  Player open as the Start menu opens them (asking to install or enable a plugin that is not ready); the firewall's check of a new
  Ruffle address is the page's alone.
- **PR 5 (in progress, branch `native-settings-6`, based on PR 4's `native-settings-5` for its helpers): Display Properties.** What the
  page is, read from it (the probe of 2026-11-24): about 2,600 px. Its first .r2 has Dynamic Backgrounds and Static Backgrounds
  (base_v24.html dispPanelS: the .dynb buttons in a 4-column grid, 8 px apart, 2 px border, radius 8, the accent's ring when chosen;
  .dth thumbnails 'center/cover' from DYN's tl/td, which the built page holds as a/*.jpg). Then Appearance (dm, its .dmi line at 12 px
  #4b5563, Latitude when automatic: dynResolve's sunrise and sunset), Northern lights (aurora58.js: aur, aurm; ACMODES Normal, Negative,
  Complementary), Spirit Falls statue (statue58.js: stm, lnm, and the animal's sentence from ST_NAMES and stAnimal()), Background
  style, Picture (wallpaper31.js: wpimg, wpfit, wpname; Choose from my files…, From this computer…), Color 1 / Color 2, then Lock and
  sign-in screen inside the grid (lock58.js: an h4 and rows shown with display:contents; the administrator's), Font, Font size,
  Window text color, Icon text color and Icon size. After it come .sp25's sections: Taskbar (settings31.js, tbbtn59.js,
  tbprog61.js), Title bars (titlebar59.js), Colours and theme (settings25.js, accent56.js, litepoint60.js), Night light, Screen and
  Graphics (native25.js: display, power; gpudrv61.js: gpu/driver/*), Screen saver (saver31.js), Tablet mode (tablet58.js) and
  Displays (mon61.js: screen, screen/arrange). Search in Settings has 66 rows for it. Its footer covers DKEYS as the page has them
  after every module: bs b1 b2 ff fs tx ic tb is dyn dm lat accent nl nlv nlsched darkui icons tbgroup rot scale icstyle icdyn wpimg
  wpfit wpname tbac tbacm icac icacm tbstyle tablet aur aurm stm lnm gmsleep gamemode twstyle twacm tbbstyle tbbac tbbacm lpac lpacm lpbr
  tbpcol (DEFS.disp gives each default; mksettings.py reads the lists the modules push). To be built in two halves: up to Icon size,
  then Taskbar to the end. It goes to the owner as one PR, once every view of the whole page holds under 2 %.
  The defaults and keys are not all plain literals in the page's code: ff becomes DejaVu Sans through fonts47.js's renaming,
  native25.js writes `DEFS.disp.icstyle??'rudolf'`, and scale comes from DEFS.acc. So they are read from the live page, as Search
  in Settings' index is: `node iso/qt/shell2/setsearch59_t.js --write` also writes desktop/settings-disp.json (DKEYS, DEFS.disp,
  ACMODES, ST_NAMES and the accent of each statue, DYN's names and thumbnails as the built page names them), and mksettings.py reads
  that file.
- **Left:** Display (PR 5, the memory with Display open); Network, Mobile network, Bluetooth, Oracle, Remote; Devices, Storage,
  Apps, Notifications; Privacy, Security, System health, Users; the switch.
- **The owner's checklist for PR 4 (Vivobook; `rudolf-settings vol`, `mm`, `acc` from a terminal on the native desktop until the
  switch):** 1. each page against Chromium mode's, light and dark. 2. Volume Controls: Mute and the system volume follow the keys;
  ▶ on each sound and a style chosen play the same sound as Chromium mode's (they play only once the switch makes Settings a native
  app the shell knows); Apply, then a window opened and closed uses the new sounds; the sound card; the microphone and Test (speak:
  the bars move); echo cancelling. 3. Multimedia: install Ruffle (Apply), the Start menu shows Flash Player after the desktop
  restarts; the folders open in File Explorer. 4. Accessibility: Cursor size and colour change the pointer after Apply; Speak a
  sample and each language's ▶ speak; Reduce motion, Sticky keys, Mouse keys take effect; Reset to defaults. 5. The memory in Task
  Manager.

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
- Part 0: first.
- Part 3: first, after 1.6.2 is stable.
- Part 1: after 1.6.2 is stable; WordPro first.
- Part 2: after 1.6.2 is stable.
- Part 4: after 1.6.2 is stable.
- Part 5: last, after 1.6.2 is stable.
