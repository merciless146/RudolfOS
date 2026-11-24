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

## Steps
- Part 0: first.
- Part 3: first, after 1.6.2 is stable.
- Part 1: after 1.6.2 is stable; WordPro first.
- Part 2: after 1.6.2 is stable.
- Part 4: after 1.6.2 is stable.
- Part 5: last, after 1.6.2 is stable.
- Part 6: after Part 3 (native Settings), before the Office apps if the owner wants it sooner.
- Part 7: step 1 (the built-in password manager) early; step 2 after it.
