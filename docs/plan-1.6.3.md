# Rudolf OS 1.6.3: the signing keys protected, native Settings, the native Office apps, fingerprint readers, Oracle's memory, Oracle's encrypted websites in the page, installing beside Windows, Oracle's passwords and extensions

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
- **Step 2 (branch `fingerprints-2`, after step 1): Settings › Users** (desktop/fprint63.js): User Settings › Sign-in
  options › Fingerprints: the account's own fingers (added with its password: a finger chosen, the reader's scans shown
  as they come from `fp/progress`, Cancel; removed with its password); an administrator's "Other accounts' fingerprints"
  (each account's Fingerprints…, the administrator's prompt; adding only while that account's files are open, its owner's
  finger on the reader). The row says plainly: no reader (a touchpad or touchscreen cannot read one), a reader libfprint
  does not support (its name and USB id), the files not on the disk, five tries. Guest none. Test:
  iso/qt/shell2/fprint63_t.js (15/15, a stand-in rudolfd). The native Settings' Users page is Part 3's: a note there.
- **Step 3 (branch `fingerprints-3`, after step 2): the lock screen and administrator prompts.**
  - **The page (Chromium and Lite, the same page):** the lock screen says "Or put your finger on the reader" while rudolfd
    says a finger is taken now, and asks the reader again after a finger not recognised (the tries left said); a match
    unlocks as a right password does. The password always still works and tells rudolfd (`fp/password`, the count starts
    again). After a finger, two-step sign-in's code is still asked (the authenticator's secret opened with the password's
    key the session holds, as after the password); an account with security keys is not offered a finger (its key's step
    needs the password). The administrator's prompt offers the signed-in administrator's finger; where rudolfd checks the
    answer itself (`{cred:1}`), the finger's one-time answer (`fp/verify`'s `"t"`, 60 s, once) goes in place of the
    password (`"fp:"+t`, taken by desk_admin_check and desktop/admin). Small hooks in core25.js (`__lockOpened`,
    `__elevOpened`, `__lockPw`); the rest in fprint63.js.
  - **Native (rudolf-native.c, fp_*):** the same line on its lock screen; desktop/unlock `{"f":true}` waits (20 s, again
    and again) for the session account's finger; an authenticator app: 401 `{"need":"code","ft"}` and the code sent with
    that one-time answer (`{"ft","c"}`: a wrong code asks again without the finger, a used answer is refused); five
    tries: rudolfd's reason shown, the reader left alone; the password unlocking gives the reader up (`fp/cancel`).
  - **Tests:** fprint163_t.sh 50/50 (adds Native's unlock with a finger, the code after it, one-time answers, an
    administrator's finger for desktop/admin once and only in its own name); fprint63_t.js 25/25 (the lock screen and
    the prompt); nativefp163_t.sh 11/11 (Xvfb, a stand-in rudolfd).
- **Left:** the native apps' own administrator prompt (librudolf-ui, iso/sys/ui/rudolf-ui.c, used by the native apps that
  are still switched off with the native Settings, Part 3): a finger there too, by the same one-time answer, when the
  native Settings is switched on; the owner's checklist with a real reader.

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
- **Note from Part 2 (fingerprint readers, 2026-11-25):** the web User Settings › Sign-in options has a Fingerprints row
  (desktop/fprint63.js, branch `fingerprints-2`): the account's own fingers and, for an administrator, other accounts'
  Fingerprints…, all through rudolfd's `fp/*` (docs/api.md). The native Users page needs the same row when it is built
  (Part 3's own work: Part 2 does not touch iso/sys/apps/settings*.c).
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

#### Each tab's memory in its tooltip (the owner, 2026-11-24 22:14 CST)
The owner: "Can we have Oracle show the memory usage of a tab in the tooltip when the mouse is held over that tab?" So **each
tab's tooltip** (its title and address) **gains the memory it uses**, the same on Native, Chromium and Lite (the owner's rule),
honest about what can be known:
- **A tab alone in its process:** that process's memory, as Task Manager counts it (the proportional set size, Pss: what the
  process alone holds plus its share of what it shares), with the frames of other sites the page holds (their own processes:
  site isolation) said apart: "Memory: 180 MB (the page 120 MB; 3 frames from other sites 60 MB)".
- **Tabs sharing a process** (Chromium may put pages of one site in one process): "Shares its memory with 2 other tabs: 180 MB
  together".
- **A freed tab** (💤, above): "Memory: none until you choose it".
- **Oracle's own pages** (about:…, files on the drive): part of Oracle's page, said so.
- **Fresh when the tooltip opens:** measured when the mouse has rested on the tab, again every few seconds while it shows,
  never while no tooltip is shown.
**How it is known.** Chromium does not say which process shows which Controlled Frame, so rudolf-launch (which already
measures each page's JavaScript memory for Task Manager while it looks, over Chromium's private pipe) asks the shell's window
for a trace of its frames (DevTools' "TracingStartedInBrowser": every frame of the window and its tabs, each with its process),
reads each of those processes' Pss and RssAnon (/proc, the same user), and writes them beside the pages it already writes, the
address of each frame as a hash, not in the clear. In Oracle's own Chromium (Native, and the encrypted setting on the Chromium
desktop; also the Tor windows) its own rudolf-launch does the same for each of its windows into a file of its own, which
rudolfd's existing processes reply passes on with the rest (no new request). Lite: WebKit does not say which of its page
processes shows which tab, so Lite's tooltip says so and gives what can be known: the page processes of Lite together.
**Tests:** the tooltip's text for each case (alone, with frames of other sites, shared, freed, Oracle's own page, not yet
measured, the browser version, Lite) with a stand-in rudolfd; rudolf-launch with a real Chromium, a shell of its own and tabs on
stand-in sites (one alone, two of one site, one with a frame of another site), in both modes: each tab's process found and its
Pss read, the frames' addresses hashed; Lite's tooltip with WebKit on the runner. **The owner's checklist:** the Vivobook,
Chromium and Native: hold the mouse over a tab: its memory, and about what Task Manager's Processes shows for it; a freed tab
says none; two tabs of one site.

#### Status (2026-11-25, scheduled task): the tooltip built (PR "Oracle's tab tooltip shows its memory")
- **The page** (desktop/oratip63.js; base_v24.html's tab bar calls it for each tab): Oracle's own tooltip (the browser's, from the
  tab's title, is not shown while the mouse is on the tab; the title stays otherwise), after the mouse rests 0.6 s: the title, the
  address and the memory line; asked of rudolfd (its processes reply, Task Manager's) at once, every second until a measurement
  taken after the tooltip opened is there (up to 8 s, else "not known"), then every 3 s; nothing asked once it hides. The page is
  cut around it as for a menu.
- **rudolf-launch** (both modes): the trace of each shell window's frames (found: the shell window's own trace lists its tabs'
  frames and their processes in ~40-70 ms with only that category; asked of a tab's own target it lists only that tab; of the
  browser, none), each process's Pss and RssAnon, the addresses hashed. **rudolfd** (rudolfd.c procs_oracle_frames, small): the
  processes reply adds "oframes", the files of Oracle's own Chromium's rudolf-launch processes (only a live rudolf-launch's, only
  one JSON object); the request stays narrow (no signal sent: liveness from /proc). No new request: confinepure.py and winapi.py
  current.
- **Tests:** oratip63_t.js 24/24 (every case of the line, asked only while shown, the page cut around it; Oracle's own Chromium's
  file; Lite's own way; the browser version); oratabmem63_t.js 20/20 here (Playwright's Chromium 153) and on the runner with
  Playwright's Chromium 153 and the Chromium snap's 158.0.8127.36 (the releases', **sandboxed**, as on Rudolf OS: its renderers'
  Pss readable by the same user; a 150 MB page's process 175 MB Pss, a 80 MB frame of another site its own process, 106 MB);
  confine161_t.sh 26/26 (15c: oframes, narrow); Oracle's page tests as before (oracle-memory-check.yml).
  **Lite:** WebKit does not say which page process shows which tab (WebKitGTK has no way to ask), so its tooltip says so and gives
  its page processes together (resident memory, from rudolfd's list); tested with the stand-in, not yet with WebKit itself (the
  owner's checklist).
- **The owner's checklist (Vivobook):** 1. Chromium mode: hold the mouse over a tab: its memory, close to Task Manager ›
  Processes' number for it; a news site's tab says its frames from other sites; 2. two tabs of one site; 3. a freed tab (💤)
  says none; 4. Native: the same; 5. Lite: the tooltip says what WebKit lets it know.

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

### Part 7. Oracle's extensions, and a password manager built in (the owner, 2026-11-24 12:42 and 12:45 CST; option C, 22:14 CST)
The owner: "What would be the recommended means of adding extensions (password managers, specialized blockers, etc) to
Oracle?", then "Yes" to adding it with the built-in password manager first. Measured on 2026-11-25 (the status below):
Chromium's own password manager and extensions never reach Oracle's tabs, which are Controlled Frames in every mode, so the
owner chose (2026-11-24 22:14 CST) **"Option C": A for passwords** (Oracle's own password manager in its tabs) **and
extensions only in a Chromium window opened from Oracle.**
**The owner's rule (2026-11-24 12:50 CST), which stands:** "Whenever Oracle is running on any Chromium desktop as part of the
desktop's own Chromium, instead of over top of it, extensions must not run at all and attempting to activate them should
remind the user to change the appropriate settings. This keeps potential unauthorized extension access from the desktop and
the rest of the PC."
**Step 1: Oracle's own password manager** (docs/plan-oracle.md Item 4's design), the same on the Chromium desktop (either
setting), Native and Lite:
- **The script:** the shell's script in the tab's **isolated world** (Chromium: the Controlled Frame's content script in a world
  of its own; Lite: WebKit's user script in the "rudolf" world), which the page's own scripts cannot see or call. It finds
  sign-in forms (a password field, the name or e-mail field before it) and tells Oracle only the form's origin and that a
  password was typed; the password itself goes to Oracle only when the person says "Save".
- **"Save your password for example.com?"** after a sign-in form is sent (Save, Never for this site, Not now), kept for that
  **exact origin** (scheme, host and port: never a look-alike host, another subdomain or a cross-origin frame).
- **Filled only after a click:** the key in the password field, or the account chosen from it; never by itself, never in a
  frame of another origin, never on an http: page (local addresses included), never in a hidden field.
- **"Use a strong password"** in a new-password field: made from the system's random source, shown before it is used, saved
  with the sign-up.
- **Kept in the account's keyring** (Rudolf OS already keeps apps' sign-ins there: Settings › Privacy › Saved passwords for
  apps), so it is locked with the account and opened only by its sign-in; never in the page's storage, the session's files,
  Oracle's profile or a sync.
- **Guest, private and Tor windows never save** (nor offer to fill: they keep nothing).
- **Oracle's ☰ › Passwords:** to see (after the account's password), change and remove them, and the sites marked "Never".
- **Its security review is in its PR:** what a page can and cannot see, where the passwords are stored and when they are locked.
**Step 2: extensions in a Chromium window opened from Oracle** ("Open in a Chromium window", as Lite's "Open in Chromium"):
- From Oracle's ☰ and a tab's menu: the tab's address opens in a window of Rudolf's Chromium with **the account's own
  Chromium profile** (its own folder, in the encrypted folder when "Keep my websites encrypted with my account" is on), never
  the desktop's profile; Chromium's own window, tab strip and address bar, where Chromium's password manager and extensions
  work as in Chrome. Oracle's own tabs keep no extensions.
- **Extensions there by Chromium's enterprise policies** (Rudolf OS already writes Chromium policy):
  - ExtensionInstallAllowlist: by default a short vetted list, signed by the release key like the Library's lists (a forged
    list is refused): password managers (Bitwarden, 1Password, KeePassXC-Browser, Proton Pass), uBlock Origin Lite, Privacy
    Badger, Dark Reader. An administrator may allow any extension from the Chrome Web Store (the administrator's prompt).
    Extensions come from the Web Store, signed by Google, and update through Chromium's own updater.
  - ExtensionSettings for every extension: runtime_blocked_hosts localhost, 127.0.0.1, [::1] and Rudolf OS's own pages, so
    no extension ever reaches rudolfd or the desktop; native messaging off unless allowed for one app (KeePassXC-Browser with
    KeePassXC from Flathub).
  - Per account; Guest none. **No Tor** (a Tor window never opens a Chromium window; extensions leak identity and help
    fingerprinting). Private: a private Oracle window opens Chromium's incognito window, where an extension runs only if the
    person allowed it there, as in Chrome.
  - Settings › Oracle › Extensions: the list, add and remove, each one's permissions in plain words, its memory (each running
    extension ~20-60 MB), the administrator's switch.
- **The desktop's own Chromium runs no extension at all,** whatever Oracle's settings say: Chromium's policy on the desktop's
  profile (ExtensionInstallBlocklist "*", ExtensionSettings "*": installation_mode blocked). Rudolf's own Ruffle extension and
  the web frames extension are loaded with `--load-extension`, which that policy would also stop: this step checks on the real
  Chromium what the policy does to them and keeps them by the narrowest means that keeps every other extension out (their IDs
  only, as "allowed" with their own settings, or loaded as component extensions), said in its PR.
- **Every way of trying in the desktop's own Chromium shows the reminder,** and does nothing else: Oracle's extensions button,
  ☰ › Extensions, an extension's page on the Web Store ("Add to Chrome") and Settings › Oracle › Extensions: "Extensions only
  run in a Chromium window opened from Oracle (☰ › Open in a Chromium window), never in Oracle's tabs or the desktop's own
  Chromium." with "Open in a Chromium window" and **"Learn more"**, which opens **about:safety#extensions-desktop** (the owner,
  2026-11-24 12:55 CST): "Oracle's safety and your data", a page of Oracle's user-safety and data-security policies in the
  default bookmarks (as "Safety"; added once to bookmarks kept from before), scrolled to the entry that explains why extensions
  are unsafe in the desktop's own Chromium and highlighted. Its entries: nothing sent about you; ads and trackers blocked; your
  websites account by account (the TPM, the encrypted setting); private windows; Tor windows (and their limits); updates and
  site isolation; passwords (step 1); extensions and the desktop; Guest; what Oracle cannot do for you. **Built and tested on
  the branch oracle-safety-page** (desktop/orasafe63.js, orasafe63_t.js, oracle-safety-check.yml; "This will be part of
  Oracle's 1.6.3 work"): merged into this step, its text brought up to date with what was built (every entry true of the build).
- The Lite desktop: Lite's "Open in Chromium" is this window (the account's profile, the same policies).
**Never:** an extension in the desktop's Chromium, in Oracle's tabs, an app window or a Tor window; one reaching 127.0.0.1 or
the desktop page; an extension installed without the vetted list or the administrator's switch; a password filled without a
click, into another origin, on http: or in Guest, private and Tor windows.
**Tests:** step 1: a test sign-in page (made-up passwords): saved after "Save" only, filled after a click only, never into a
cross-origin frame, an http: page or a look-alike host, nothing in Guest, private and Tor windows, the page's own script cannot
read what the isolated world holds, locked with the account (the keyring); on Chromium, Native and Lite. Step 2: with Oracle in
the desktop's own Chromium, a stand-in extension cannot be installed or run (policy) and each way of trying shows the reminder;
in the Chromium window, a stand-in extension that tries 127.0.0.1, rudolfd's port and the desktop page: refused; a Tor window
offers no Chromium window; a forged vetted list refused; on runners, Bitwarden's and uBlock Origin Lite's real extensions
install from the policy and work (a test sign-in filled; a test ad blocked); memory with two extensions; about:safety's entries.
**The owner's checklist:** Oracle on the Vivobook: save and fill a password (a made-up one on a test site, then a real site);
☰ › Passwords; a Chromium window from Oracle with Bitwarden (or the owner's manager) signed in and filling; uBlock Origin Lite
on a busy site; a Tor window without them; the reminder and about:safety in the desktop's Oracle.
#### Status (2026-11-25, scheduled task): Chromium's own password manager and extensions do not reach Controlled Frames (#684)
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
- **The ways on, as set out for the owner:** A. Oracle's own password manager in its tabs; B. Chromium's own tabs in
  Oracle's own-profile mode (Oracle there no longer Oracle; Native unlike the Chromium desktop); C. A for passwords, and
  extensions only in a Chromium window opened from Oracle. **The owner chose C (2026-11-24 22:14 CST)**: steps 1 and 2 above.
- **Tests:** oraframes63_t.js 8/8 with each Chromium (the runner); Lite: none (WebKit has neither).
#### Status (2026-11-25, scheduled task): option C planned; step 1 next
- This plan rewritten for option C (PR "Oracle's tab tooltip shows its memory", with Part 4's tooltip). Step 1 (the password
  manager) and step 2 (the Chromium window, with about:safety from oracle-safety-page) follow, one PR each.

### Part 8. Fixes from the owner's use (the owner, 2026-11-24 22:51 CST)
"Some items to add to 1.6.3:" each fixed with a test that fails before the fix, the same on Native, Chromium and Lite
unless the item is Native's own (then Native is made to match Chromium, never the other way: the owner's rule).
1. **Paint doesn't ask to save.** "Paint app does not prompt user to save after unsaved changes are made to images."
   Closing Paint's window, opening another picture or New with unsaved changes asks "Save changes to NAME?" (Save, Don't
   save, Cancel), as Notepad and WordPro do; signing out or shutting down with Paint changed lists it like the other
   apps' unsaved work.
   - **Status (2026-11-25, scheduled task): asked at close and New** (PR "Paint asks to save"). Paint now knows when its
     picture changed (a stroke, a shape, a fill, Undo, Redo, Resize) since it was opened, saved or started: its window's ✕
     (and the taskbar's Close, the native frame's ✕) and New ask "Save changes to NAME?" (Save: the name asked when it has
     none, then the file written and the window closed; Don't save; Cancel or Esc); New starts an untitled picture (it kept
     the old file's name, so Save wrote the blank picture over it). Opening another picture opens a window of its own, as
     before, so nothing is lost there. **Test:** `iso/qt/native/paintsave163_t.js` (paintsave163-check.yml): Chromium mode
     11, Native 5; **before 2/8 (the rest could not run: the window had closed), after 16/16.** **Not done, a step of its
     own:** signing out or shutting down lists no app's unsaved work today, on either engine (Notepad's and WordPro's
     neither: Chromium mode saves the session, Native closes the windows at once, rudolf-native `power("logout")`); Paint
     now says what it has unsaved (`W.unsaved()`) for that list when it is built.
2. **Native's transitions are rough.** "Transitions on Native are rough/blocky." Native's window animations (open, close,
   minimize, restore, maximize, the Start menu, menus, pop-ups, LitePoint) drawn smoothly as Chromium's: measured frame
   by frame against Chromium mode (frames dropped, steps visible, the animation's length), on the Vivobook's integrated
   graphics as well as without acceleration; the soft-drawing mode kept for weak machines.
3. **Xbox Cloud Gaming and GeForce NOW don't respond.** "Xbox gaming and GeForce apps do not respond when clicked."
   (desktop/game51.js: each opens in Oracle.) Clicking either in the Start menu, the Games tab, a pin or the desktop
   opens it, on every desktop engine (Lite: in Chromium if WebKit can't play them, said plainly); a test for each place.
   - **Status (2026-11-25, scheduled task): fixed** (PR "Xbox Cloud Gaming and GeForce NOW open from the Start menu"). The
     cause: in Chromium mode the Start menu's rows call the menu's own table of actions (base_v24.html `A`), which had
     neither game (nor the Library's games of games58.js), so a click there threw "A[a] is not a function" and did
     nothing; the pins, the desktop icons, the Games tab and the right-click Open went through `launch` and worked. A row
     without an action of its own now opens through `launch`, as its pin does. With Lite, WebKitGTK has no WebRTC, which
     both streams need: they open in Chromium's window (rudolfd's web/open, as the call sites, oracle58.js), with a
     notification saying so. **Test:** `iso/qt/native/cloudgames163_t.js` (cloudgames163-check.yml): Chromium mode 10
     places (the Start menu, its right-click Open, the Games tab, a pin, a desktop icon, for each game) and the Library
     games' rows, Lite 2, Native 6 (the Start menu, a pin, a desktop icon: Oracle's window at the game's address);
     **15/19 before, 24/24 after**. Native opened both from every place already in the test (Oracle's WebKit window);
     on the Vivobook Native's Oracle is Rudolf's Chromium: the owner's checklist.
4. **Desktop icons are slow on Native.** "Desktop icon removal/changes still take several minutes on Native." A desktop
   icon removed, renamed, added or changed shows within a second on Native, as on Chromium; find what Native waits for
   (a poll, a watch, the hidden page's sync) and measure before and after.
5. **Make a stick refined, moved to Backup.** "'Make a stick' needs further refinement and to be moved from
   update/recovery to backup settings." Settings › Backup gains "Make a stick" (Update and recovery keeps a link to it
   while people learn the new place; Search in Settings finds it there); refined: the stick's size and model shown, the
   full image or boot files only explained in plain words, a stick too small refused before anything is written, the
   progress and the time left, a check of what was written, the stick safe to remove at the end; the native Settings'
   pages carry it in its new place.
6. **Native's desktop menu stays open.** "Desktop right-click menu does not disappear if clicked outside of while open on
   Native. Will only close after an option is selected." On Native, a right-click menu (the desktop's, the taskbar's,
   a window's) closes when clicking anywhere outside it, pressing Esc, or switching windows, as on Chromium.
   - **Status (2026-11-25, scheduled task): fixed** (PR "Native's menus close on a click outside"). The cause: the menu's
     grab (cm_open) leaves the desktop's own windows their presses (so a right-click elsewhere opens the next menu at once),
     and the desktop, the taskbar and the apps' pages are GTK's windows, their presses and keys coming by XInput 2: none of
     them reached the menu, which closed only on a press in another program's window. Now any press outside the menu (the
     desktop, the taskbar, a title bar, inside a window, another monitor) closes it first and then goes on, as the page's
     pointerdown; Esc and Tab close it wherever the keyboard's focus is; another window chosen (Alt+Tab, a window coming up)
     closes it, as the page's blur. **Test:** `iso/qt/native/cmclose163_t.js` (menus163-check.yml): 12 steps on Chromium
     mode's page and Native, **3/12 before, 12/12 after**; tbbeh_t.js and keybeh_t.js unchanged (tbbeh's step 9 fails
     before and after: Command Prompt's key, `terminal` and `prompt`, a test of its own). Found: on Chromium mode's page
     (Playwright, one window) the menu stays open over the page's own Alt+Tab; Native closes it, the owner's words.
**The owner's checklist:** each item on the Vivobook (Native and Chromium).

## Steps
- Part 0: first.
- Part 3: first, after 1.6.2 is stable.
- Part 1: after 1.6.2 is stable; WordPro first.
- Part 2: after 1.6.2 is stable.
- Part 4: after 1.6.2 is stable.
- Part 5: last, after 1.6.2 is stable.
- Part 6: after Part 3 (native Settings), before the Office apps if the owner wants it sooner.
- Part 7 (option C): step 1 (Oracle's own password manager) early; step 2 (extensions in a Chromium window opened from
  Oracle, with about:safety) after it.
- Part 8: alongside the others; each item its own PR.
