# Rudolf OS 1.6.3: the signing keys protected, native Settings, the native Office apps, fingerprint readers, Oracle's memory, Oracle's encrypted websites in the page, installing beside Windows, Oracle's extensions

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
#### Status (2026-11-25, scheduled task)
- **Step 1 (branch `fingerprints-1`): the system's side.** One change to the design above, for the owner to see: **no
  fprintd.** fprintd is a D-Bus service that checks each caller with polkit; with only rudolfd asking, it would add a
  system bus name, polkit (which Rudolf OS does not have) and a daemon, and protect nothing more. Instead a root helper,
  `fprint` (iso/sys/rudolf-fprint.c, /usr/lib/rudolf/root/fprint, run by rudolf-rootd for rudolfd like the other root
  helpers), talks to libfprint itself for each request and ends: nothing listens, there is no service to ask (the
  listeners list says so). Its capabilities: CAP_DAC_READ_SEARCH only (to read the account's folder's key identifier),
  dropped with no_new_privs before the reader is opened; Landlock with the confinement switch on.
  - **The prints** are each account's own, in PERSIST/fprint/ACCOUNT (root's 0700), **encrypted by the kernel under the
    account's file key** (the same fscrypt key as its home folder, as Oracle's encrypted folder is). The password unwraps
    that key, so the prints can be read only while the account's files are open: after a restart or a sign-out nobody,
    root included, can read or use them, and a copy of the disk holds them encrypted. Removed with the account
    (au_sync_ids), with the reader's own copies for readers that keep prints in themselves (by their name,
    "rudolf:ACCOUNT").
  - **rudolfd** (iso/sys/rudolfd-fprint.inc; docs/api.md `fp/*`): `fp/status` (the reader, a reader libfprint does not
    support named from libfprint's own list, the account's fingers and whether a finger is taken now and why not),
    `fp/enroll` and `fp/remove` (the account's own password or an administrator's), `fp/verify` (`"for":"admin"`: an
    administrator's finger only), `fp/cancel`, `fp/password`. A finger is taken only for an account that signed in with
    its password since the start (the page's home/unlock?session=1, Native's desktop/login; signing out ends it), never
    after five fingers not recognised in a row until the password is used, never for Guest. An account whose files are
    not on the disk (older accounts) cannot have fingerprints: Settings will say so.
  - **The size: 972 KB** added to the base layer (fprint-check.yml's `size` job against beta 2026112422: libfprint with what
    it links that the base layer lacked, and its list of unsupported readers); ci_release.sh fails over its own budget,
    3 MB (systools/mkfprint.sh).
  - **Tests:** iso/qt/host/fprint163_t.sh (fprint-check.yml): libfprint's virtual reader with made-up fingerprints
    (fixtures/vfinger.py draws them from a number; never anyone's finger) on an encrypted ext4 drive: adding (passwords,
    the files not on the disk), matching, another account's finger refused, an administrator's prompt, five tries, the
    password giving the finger back, Guest, cancelling, the first sign-in after a restart, signing out, the prints
    unreadable once signed out, removing, an account removed, a release build ignoring the virtual reader.
- **Left:** step 2, Settings › Users (add and remove fingers, the administrator's prompt for another account's, a reader
  libfprint does not support named, no reader said); step 3, the lock screen (Chromium, Lite and Native the same) and the
  administrator prompts taking a finger (the password always still; the code or the key of two-step sign-in still asked
  after a finger; an administrator's finger in rudolfd-checked prompts by a one-time answer from `fp/verify`).

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
#### Status (2026-11-25, scheduled task): built (PR "Oracle's memory")
- **Measured** (`iso/qt/native/oramem63_t.js`, oracle-memory-check.yml on GitHub's runner): a real Chromium, Oracle's tabs as
  the shell makes them (Controlled Frames in an account's partition, the same in the desktop's Chromium and Oracle's own), the
  Pss of every process of that Chromium after the pages settle. Real websites (Wikipedia, GitHub, BBC News, the Guardian,
  MDN, YouTube), the Chromium snap's 158.0.8127.36 (the releases'): **1 tab** 114 MB more than none; **3 tabs** 838 MB more
  (BBC News' ads alone opened some 30 processes of their own); **6 tabs** 682 MB more. **Before:** every tab stays alive
  however long it has not been looked at. **After** (the background ones freed, the shown one alive): 3 tabs 192 MB less
  (**96 MB a background tab**), 6 tabs 395 MB less (**79 MB a background tab**). Playwright's Chromium 153: 67 and 85 MB a
  background tab; stand-in news pages: 42 MB. (The test page around the tabs is smaller than Oracle's, so the totals are
  the tabs' own; Oracle's own page adds its usual share.)
- **Kept, with that saving: background tabs freed after a while** (desktop/orasleep63.js): a tab that is not its window's
  current one and has not been for 30 minutes (Settings › Oracle › Memory: 15 minutes, 30 minutes, 1 hour, 2 hours, Never)
  is freed as game mode frees one: its page closed, its address, title and scroll kept (💤); choosing it loads it again
  and the shell scrolls it back (shell.js: the busy answer's `y`, `scrollto`). Never a tab playing sound or video, holding
  typed text, the shown one or a game's. **Under memory pressure** (the memory guard's warning, outside game mode, whose
  own freeing stays) the background tabs are freed at once, with a notification.
- **Not changed:** site isolation, Chromium's sandbox, the Tor and private windows' separation, the encrypted folders,
  the disk cache's size. No Chromium setting was found that frees a live tab's own caches on Linux without one of those,
  so what memory pressure frees is the background tabs.
- **The desktops:** Chromium and Native (the same shell and page): as above. Lite: its WebKit tabs are freed the same way
  and come back at their address, at the top (rudolf-webkit does not say how far a page was scrolled).
- **Tests:** orasleep63_t.js 19/19; Oracle's page tests as before (oracle58, private59, tor59, orahover61, orasession61,
  oraclosed61, orafind61, orablock61, orareader61, oraacct60, gamesleep59, oracledl, oraclefile60, oracleart60); the
  measuring test with each Chromium; the Oracle checks (find, closed tabs, reader, session, blocking, Goes To, downloads)
  on the branch.
- **The owner's checklist (Vivobook, Chromium and Native):** 1. open six sites in Oracle, leave five unlooked-at for 30
  minutes (or choose 15 minutes in Settings › Oracle › Memory): 💤 on them, and Task Manager's Oracle smaller; 2. choose a
  freed tab: it loads again where it was scrolled; 3. a tab playing music and one with a half-typed message stay awake;
  4. Never: nothing is freed.

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

### Part 6. Installing beside Windows on the same drive (the owner, 2026-11-24 11:35 and 12:30 CST)
The owner, installing on an ASUS desktop: "Is there a way to install it without formatting the hard drive or removing
Windows?", then "Yes, add it to 1.6.3."
**Today.** Settings > Update and recovery > Install here... writes Rudolf OS's whole disc image to the start of the
chosen disk and adds RUDOLFDATA after it (iso/sys/root/usr/lib/rudolf/install.sh): the whole disk is erased. Beside
Windows today means another disk (an internal second drive, an external SSD, or a stick with Keep my files).
**The change: "Install beside Windows..."** (UEFI computers with a GPT disk; BIOS/MBR stays whole-disk only):
1. **Room, made in Windows first.** The person shrinks Windows' partition in Windows' own Disk Management (Shrink
   Volume), which knows NTFS, BitLocker, the page file and hibernation; Rudolf OS uses only unallocated space and never
   resizes or writes to a Windows partition. The installer shows the disk's partitions by name, size and kind (EFI
   system, Microsoft reserved, Windows, recovery, free) and offers only a free area of at least 16 GB. Before that,
   Rudolf OS's own guide (and Settings' text) says how: back up, note the BitLocker recovery key (aka.ms/myrecoverykey),
   turn off Windows' Fast Startup, then Disk Management > Shrink Volume.
2. **What Rudolf OS writes, and nothing else:** one new partition in that free space, RUDOLFDATA (the installed
   system's slots, as an installed drive with boot files only already works: 1.5.3, 1.6.0 Part 16), and Rudolf OS's start-up
   files in the disk's existing EFI system partition, in their own folder only (\EFI\rudolf\: shim, Rudolf's signed
   start-up file, recovery.efi, the menu), never touching \EFI\Microsoft\ or \EFI\Boot\. If the EFI system
   partition is too small for them, it stops and says so (no resizing).
3. **Choosing at start-up:** a UEFI boot entry "Rudolf OS" (shim), put first; Rudolf's start-up menu gets a "Windows"
   entry that starts Windows Boot Manager from the same EFI system partition (systemd-boot finds it), so the person
   picks each time, with a short wait and Rudolf OS as the default (changeable in Settings > Update and recovery). The
   firmware's own boot menu (F8 on ASUS) still offers both. Windows updates that put Windows Boot Manager first again:
   Rudolf OS puts its entry back at its next start, and System health says so.
4. **Secure Boot** stays on for both: Rudolf OS through Microsoft-signed shim as now (its key enrolled once); nothing
   in the firmware's own key lists changes. BitLocker: changing the boot order can make Windows ask for its recovery
   key once; the installer says so before it starts, and the guide says where the key is.
5. **Updates and Recovery mode** work as on any installed Rudolf OS (both slots in RUDOLFDATA; recovery.efi in
   \EFI\rudolf\ starts the official stable).
6. **Removing it:** Settings > Update and recovery > "Remove Rudolf OS from this drive" (from a stick or from Windows'
   side: a guide) deletes RUDOLFDATA, \EFI\rudolf\ and the boot entry, and leaves the space unallocated for Windows to
   take back (Disk Management > Extend Volume).
**Never:** a Windows partition written, resized or formatted; anything in \EFI\Microsoft\; the firmware's Secure Boot
keys changed; an install that goes on when the free area, the EFI system partition or the disk's partition table is not
what was shown.
**Tests:** in QEMU with OVMF and Secure Boot on: a made-up GPT disk laid out as Windows lays one out (an EFI system
partition with a stand-in \EFI\Microsoft\Boot\bootmgfw.efi that only prints a mark, Microsoft reserved, an NTFS
partition with files, a recovery partition, free space; no Windows files or images): install beside; every Windows
partition's checksum unchanged and \EFI\Microsoft\ unchanged; Rudolf OS starts, updates, rolls back, Recovery mode;
the menu's Windows entry starts the stand-in; the boot order put back; too little free space, a full EFI system
partition, an MBR disk and a BIOS computer refused with plain words; removal leaves the disk as before (unallocated).
**The owner's checklist** (the ASUS desktop): back up and note the BitLocker key; shrink in Disk Management; install
beside; Rudolf OS and Windows each start from the menu and from F8; a Windows update, then Rudolf OS's next start; an
update of Rudolf OS; removal and Extend Volume back.

### Part 7. Oracle's extensions, and a password manager built in (the owner, 2026-11-24 12:42 and 12:45 CST)
The owner: "What would be the recommended means of adding extensions (password managers, specialized blockers, etc) to
Oracle?", then "Yes" to adding it with the built-in password manager first.
**Where extensions may run.** On the Chromium desktop, Oracle's tabs are Controlled Frames inside the desktop's page,
where Chromium runs no extensions, and that page holds rudolfd's token: **extensions never run in the desktop's own
Chromium.** They run only in Oracle's own Chromium with its own profile (as on Native, and with "Keep my websites
encrypted with my account"): turning extensions on makes Oracle open that way on the Chromium desktop too, as the
encrypted setting does (Part 5 does not change this: frames run no extensions). The Lite desktop (WebKit) has none, and
Settings says so.
**The owner's rule (2026-11-24 12:50 CST):** "Whenever Oracle is running on any Chromium desktop as part of the desktop's
own Chromium, instead of over top of it, extensions must not run at all and attempting to activate them should remind
the user to change the appropriate settings. This keeps potential unauthorized extension access from the desktop and the
rest of the PC." So:
- Oracle in the desktop's own Chromium (its tabs in the desktop page: the Chromium desktop with extensions off and the
  encrypted setting off, and Part 5's encrypted-in-the-page): **no extension runs**, enforced by Chromium's policy on the
  desktop's own profile (ExtensionInstallBlocklist "*", no allowlist, ExtensionSettings "*": installation_mode blocked;
  the desktop's profile never gets an extension, whatever Oracle's settings say), not only by Oracle's buttons.
- **Trying to use one there reminds the person, and does nothing else:** Oracle's extensions button, ☰ › Extensions, an
  extension's page on the Web Store ("Add to Chrome") and Settings › Oracle › Extensions all show the same notice:
  "Extensions only run when Oracle runs in its own window. Turn on Settings › Oracle › Extensions (Oracle then opens in
  its own window), or turn on 'Keep my websites encrypted with my account'." with a button that opens that page of
  Settings; nothing is installed or switched by the notice itself.
- **"Learn more"** in that reminder opens Oracle's page **about:safety#extensions-desktop** (the owner, 2026-11-24 12:55
  CST): "Oracle's safety and your data", a page of Oracle's user-safety and data-security policies in the default
  bookmarks (as "Safety"; added once to bookmarks kept from before), scrolled to the entry that explains why extensions
  are unsafe in the desktop's own Chromium and highlighted. Its entries: nothing sent about you; ads and trackers
  blocked; your websites account by account (the TPM, the encrypted setting); private windows; Tor windows (and their
  limits); updates and site isolation; extensions and the desktop; Guest; what Oracle cannot do for you. **Built and
  tested on the branch oracle-safety-page** (desktop/orasafe63.js, orasafe63_t.js 9/9, oracle-safety-check.yml;
  "This will be part of Oracle's 1.6.3 work": merged with this part, not before), its text kept true as each part lands.
- Oracle in its own Chromium, over the desktop (Native; the Chromium desktop with extensions or the encrypted setting
  on): extensions as below. A test checks the desktop's profile refuses every extension even if Oracle's own policy
  allows one.
**Step 1 (first): the built-in password manager.** Chromium's own (save, fill, generate, check for leaked passwords
only if the person turns that on) in Oracle's own-profile mode, its store the account's keyring (Rudolf OS already keeps
apps' sign-ins there: Settings › Privacy › Saved passwords for apps), so it is locked with the account; Oracle's ☰ ›
Passwords opens it; Guest and Tor windows never save. No extension needed for the most common need.
**Step 2: extensions, by Chromium's enterprise policies** (Rudolf OS already writes Chromium policy):
- ExtensionInstallAllowlist: by default a short vetted list, signed by the release key like the Library's lists (a forged
  list is refused): password managers (Bitwarden, 1Password, KeePassXC-Browser, Proton Pass), uBlock Origin Lite,
  Privacy Badger, Dark Reader. An administrator may allow any extension from the Chrome Web Store (the administrator's
  prompt). Extensions come from the Web Store, signed by Google, and update through Chromium's own updater.
- ExtensionSettings for every extension: runtime_blocked_hosts localhost, 127.0.0.1, [::1] and Rudolf OS's own pages,
  so no extension ever reaches rudolfd or the desktop; native messaging off unless allowed for one app (KeePassXC-Browser
  with KeePassXC from Flathub).
- Per account, in that account's Oracle profile (in the encrypted folder when that is on); Guest none.
- Tor windows: never (extensions leak identity and help fingerprinting). Private windows: off unless the person allows an
  extension there, as in Chrome.
- Settings › Oracle › Extensions: the list, add and remove, each one's permissions in plain words, its memory (each
  running extension ~20-60 MB), the administrator's switch; Oracle's toolbar: an extensions button for their menus.
**Never:** an extension in the desktop's Chromium (whatever Oracle's settings say), in an app window or a Tor window; one reaching 127.0.0.1 or the
desktop page; an extension installed without the vetted list or the administrator's switch.
**Tests:** with Oracle in the desktop's own Chromium, a stand-in extension cannot be installed or run (policy) and each
way of trying shows the reminder; a stand-in extension that tries 127.0.0.1, rudolfd's port and the desktop page: refused; a Tor window loads
none; a forged vetted list refused; the built-in password manager saves and fills in a test page and keeps nothing in
Guest; on runners, Bitwarden's and uBlock Origin Lite's real extensions install from the policy and work (a test
sign-in filled; a test ad blocked); memory with two extensions.
**The owner's checklist:** Oracle on the Vivobook: save and fill a password; Bitwarden (or the owner's manager) signed
in and filling; uBlock Origin Lite on a busy site; a Tor window without them.
#### Status (2026-11-25, scheduled task): step 1 waits on the owner's decision
- **Found: Oracle's own Chromium shows its websites in Controlled Frames too,** so Chromium's own password manager and
  extensions do not work there either. On Native (rudolf-native.c oracle_open) and with "Keep my websites encrypted with my
  account" on the Chromium desktop (rudolfd's orawin), Oracle's own Chromium runs the same shell (the Isolated Web App,
  iso/shell) as the desktop's: Oracle's toolbar and tabs are the desktop's page, and each website is a `<controlledframe>`.
  Chromium attaches its password manager and extensions' content scripts to its own tabs, not to Controlled Frames.
  Measured by `iso/qt/native/oraframes63_t.js` (oracle-frames-check.yml), the same profile, sign-in page and stand-in
  extension side by side: **a plain Chromium tab:** the password manager reads the form (its log names the site 37-44 times)
  and the extension runs; **a Controlled Frame:** the password manager never sees the form (0) and the extension never runs.
  The same with Playwright's Chromium 153 and the Chromium snap's 158.0.8127.36 (the releases' Chromium). The test fails
  with a plain message the day a Chromium release changes this.
- **So Part 7 as written cannot be built** (step 1, and step 2's extensions, which need their content scripts in the
  websites' pages). Nothing of step 1 was changed in Rudolf OS. The ways on, for the owner to choose:
  - **A. Oracle's own password manager in its tabs** (docs/plan-oracle.md Item 4's design): the shell's script in the
    tab's isolated world finds sign-in forms; "Save your password for example.com?"; filled only after a click and only
    for the tab's exact origin; "Use a strong password"; kept in the account's keyring (Settings › Privacy), locked with
    the account; Guest, private and Tor windows never save. Works the same on the Chromium desktop (either setting),
    Native and Lite (WebKit's user script), so the desktops match. Not Chromium's own: it needs its own security review.
  - **B. Chromium's own tabs in Oracle's own-profile mode:** Oracle's own Chromium shows websites in Chromium's own
    window (its tab strip and address bar instead of Oracle's), where Chromium's password manager and extensions work as
    in Chrome. Oracle there no longer looks or works like Oracle (the Goes To line, find bar, reader mode, the ad and
    tracker shield, the firewall's check, sleeping tabs, Ruffle and Oracle's menus are Oracle's page's), and Native would
    differ from the Chromium desktop with the setting off.
  - **C. A for passwords, and extensions only in a Chromium window opened from Oracle** ("Open in a Chromium window",
    as Lite's "Open in Chromium" is today, the account's own profile, never the desktop's): Bitwarden, uBlock Origin Lite
    and the vetted list there, under the owner's rule and the policies above; Oracle's own tabs keep none.
  The owner's rule (no extension in the desktop's own Chromium, by policy) can be built whichever is chosen. One thing for
  it: the desktop's Chromium loads Rudolf's own Ruffle extension (and, without the shell, the web frames extension) with
  `--load-extension`, which ExtensionInstallBlocklist "*" with no allowlist would also stop: step 2 checks this on the
  real Chromium and says what it means before the policy lands.
- **Tests:** oraframes63_t.js 8/8 with each Chromium (the runner); Lite: none (WebKit has neither).

## Steps
- Part 0: first.
- Part 3: first, after 1.6.2 is stable.
- Part 1: after 1.6.2 is stable; WordPro first.
- Part 2: after 1.6.2 is stable.
- Part 4: after 1.6.2 is stable.
- Part 5: last, after 1.6.2 is stable.
- Part 6: after Part 3 (native Settings), before the Office apps if the owner wants it sooner.
- Part 7: step 1 (the built-in password manager) early; step 2 after it.
