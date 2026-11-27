# Rudolf OS 1.6.3: the signing keys protected, native Settings, the native Office apps, fingerprint readers, Oracle's memory, Oracle's encrypted websites in the page, installing beside Windows, Oracle's passwords and extensions

1.6.3 follows 1.6.2. The owner, 2026-11-23 18:20 and 18:23 CST: "Push fingerprint scanners to 1.6.3" and, for the Office
apps' native versions, "Move them to 1.6.3 so we can expedite the rollout of 1.6.2 stable on the beta build that includes
native settings", then (18:27 CST) "Push settings to 1.6.3 as well." So 1.6.2 goes stable without the native Settings switched
on, and these parts follow it.
The owner's rule for the native desktop holds for every part: "Rudolf OS on the native desktop should appear and
function exactly as it does in Chromium mode." Chromium and Lite keep the web versions of every app unchanged.

## Parts
A part's numbered bullet points are its **items**: a reference reads "Part 3, item 4" (AGENTS.md, Terms).

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
#### Status (2026-11-27, scheduled task): WordPro begun, step 1 (the editor and its toolbar)
- **Branch `wordpro-1`:** `iso/sys/apps/wordpro.c` (`/usr/bin/rudolf-wordpro`, with librudolf-ui) has the page's window
  (980 x 646), the toolbar of `.wp-tb` -- the paragraph styles, the fonts, the size with its - and +, bold, italic,
  underline, strikethrough, sub- and superscript, the text colour and the highlight, the four alignments, bullets,
  numbering, the indent, the line spacing, clear formatting, undo, redo and Find -- the page's white sheet on its grey
  area with the 96 px margins and the 816 px width of Letter, and the status line's page, words, characters and zoom.
  Find and replace is the page's bar (next, previous, replace all, close). `iso/sys/apps/mkwordpro.py` reads WP_STYLES
  and WP_FONTS from `base_v24.html` at build time, so the toolbar's lists are the page's (9 styles, 12 fonts).
- **Its test:** `iso/qt/native/wordpro_t.sh` (`wordpro-check.yml`): built with `RUI_WERROR=1`, then used under Xvfb with a
  stand-in rudolfd -- the window, the lists, typing counted, Ctrl+B tagging the choice, Ctrl+F choosing what it finds, a
  second line counted the page's way (its newline is not a character). The screenshot comparison against the web WordPro
  is **not** in it yet: it comes with the step that makes every view of the window, so that no view can pass by being
  unfinished.
- **Branch `wordpro-3`: the page's menu bar** (`.wp-mb`): File, Edit, View, Insert, Format, Table, Tools and Help, each
  menu the page's own items in the page's order and with its shortcuts. What the editor can already do acts -- New,
  undo, redo, cut, copy, paste, select all, find and find-and-replace, the page's six zooms, the six toggles, the four
  case changes, clear formatting, bullets, numbering, the indent, block quote, the date and time insertions, document
  statistics, the keyboard shortcuts and About -- and what needs a later step is drawn unavailable, as the page draws an
  item that cannot be chosen. A toggle that is on, and the zoom in use, carry the page's check mark. The dropdowns are
  `rui_menu`, the desktop's menus in the page's look.
- **Branch `wordpro-4`: the bar's shortcuts, and step 2 begun** (tables and pictures): Ctrl+H opens the Find bar on its
  replace box and Shift+Ctrl+V pastes the clipboard's text alone, as the bar advertises; the document model is decided
  below; pictures and tables are places in the writing with their own data (a picture's bytes and size, a table's cells),
  the page's default 3 x 3 table, and a picture chosen through GTK's own file chooser until the page's dialog comes.
- **Branch `wordpro-5`: saving and opening the page's .DOC** (step 2's file half): the document is written as the page
  writes it -- `<!--wp:{meta}-->`, `<style data-wp>` with the page's stylesheet (read from `base_v24.html` at build time,
  so it cannot drift), the blocks as its HTML (paragraphs, the inline tags, the colour and font spans, `<table>` with
  `<th>`/`<td>`, `<img src="data:...">`) -- and opened by parsing that same subset back into the model, refusing nothing
  silently. `File > Open...`, `Save` and `Save As...` are live in the bar (GTK's own choosers until the page's dialogs
  come), with Ctrl+O and Ctrl+S; a document remembers its file. The check opens a document in the page's exact shape,
  saves it, and opens and saves that again: the same bytes, which is the round trip the model promises.
- **Not in this step, by the plan's own split:** the dialogs of those items (open, save, import, export, page setup,
  print, table, picture, link, special characters, header and footer), links, page setup, headers and footers, the table
  of contents, printing, and the rest of the page's HTML (lists, the paragraph classes' other blocks, a nested table,
  a picture's alt text) -- steps 2 and 3. The native desktop keeps opening the web WordPro until the
  whole app passes, and `wordpro` is **not** in `NATIVE_APPS[]` yet.

#### The document model (decided 2026-11-27, step 2 begins: tables and pictures)
The native document is the page's document, not a text buffer that happens to look like one.
- **What a document is:** a sequence of blocks. A block is a paragraph -- text with its inline tags (bold, italic,
  underline, strikethrough, sub- and superscript, the colours, the font and size, the paragraph styles and alignments) --
  or an object: a picture or a table. Text lives in the text view's buffer; an object is a place in that same buffer (a
  child anchor, which the page itself keeps as its placeholder character U+FFFC) whose data is held beside it, so the
  cursor, the counts and the undo still move through one writing in order.
- **A picture** is its image, its pixel size and its alt text. **A table** is a grid of cells, each cell an ordered list
  of blocks (the page's `<td>` holds paragraphs, lists and nested tables too); the first cut gives each cell one
  paragraph -- the page's default 3 x 3 with a header row and borders -- and its cells' words are part of the page's
  word count.
- **What it means for the page's .DOC** (exactly the file the web WordPro writes, its `serialize()`): saving writes that
  file -- `<!--wp:{meta}-->`, `<style data-wp>` carrying the page's own stylesheet, then the blocks as the page's HTML: a
  paragraph as `<p>`/`<h1>`-`<h4>` with the page's classes and its inline tags as `<b>`, `<i>`, `<u>`, `<s>`, `<sub>`,
  `<sup>`, `<span style="color:...">`, `<span style="font-family:...;font-size:...">`; a table as `<table>` with `<th>`
  and `<td>`; a picture as `<img src="data:image/png;base64,...">` with its width and height. Opening parses that same
  subset back into blocks, and refuses out loud a file it cannot read rather than dropping silently what it does not
  understand (the page's own rule). The test the save must pass: a document the web WordPro saved opens here unchanged,
  and one saved here opens there unchanged.
- **What it costs, and what the first cut leaves out:** an entry of the history is the text with its places and, at each
  place, the object itself -- a picture's bytes (one copy shared by the writing and every entry that has it, so an
  unchanged picture costs nothing per step) and a table's cells -- so undo and redo take an object away and put the object
  back, never a place with nothing behind it; a cell holds one paragraph, with no inline spans inside it yet; the page's
  insert dialogs (rows, columns, header row, borders; a picture's size and text) come with the dialogs step, so the menu
  inserts the page's default table and asks for a picture's file through GTK's own chooser.

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
- **PR 5 (branch `native-settings-6`): Display Properties.** What the
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
- **PR 5, done (2026-11-25, scheduled task): Display Properties** (`settings_disp.c`). Every section of the page:
  - the backgrounds' thumbnails (the page's own a/*.jpg; a WebP one through libwebp);
  - Appearance, Northern lights, the Spirit Falls statue, Background style;
  - Picture: From this computer… kept as wpUse keeps it, in IndexedDB rudolfos/kv "wp:ID" through rudolfd's idbm/put, with
    bd95wpids:ACCT; Remove; Position;
  - Color 1 / Color 2, Font, Font size, the text colours, Icon size;
  - Lock and sign-in screen: bd95sys.lk, at once for an administrator, and its picture bd95lkpic;
  - Taskbar, Title bars, Colours and theme (the accent colour, its swatches, where it is used), Night light;
  - Screen: scaling and rotation in the working copy; resolution and frame rate from rudolfd's screen, with their own confirmed Apply;
  - Graphics (display?mode=), Screen saver (bd95sys, at once), Tablet mode (kept at once), Displays.
  Its footer covers DKEYS as the live page has them. desktop/settings-disp.json is written by setsearch59_t.js --write with the
  keys, the defaults, the drop-downs and the thumbnails.
  Against the web one: every view under 2 % in light, dark, light-green and dark-pink, at the top, eight places on the way down and
  the end (0.54-1.87 %); Search the same 67 rows; the checklist D1-D7; memory with Display open 16.0 MB RssAnon (the web
  Settings 73.9 MB). The test's rudolfd now answers screen and display.
  **Known differences (PR 5):**
  - Choose from my files… (the page's own drive C:, in its storage) says so instead.
  - The lock screen's Preview (the page's overlay).
  - The icon style preview's eight icons (icSvg); the title bar styles' own looks in the preview (each drawn in the accent colour).
  - The screen saver's moving preview and Preview.
  - Displays' arrangement for two or three monitors (screen/arrange); one display is said as the page says it.
  - Changes before Apply show in Settings only.
- **PR 6, done (2026-11-25, scheduled task): Network Settings, Mobile network, Bluetooth, Oracle, Remote access** (`settings_net.c`,
  `settings_mob.c`, `settings_bt.c`, `settings_web.c`, `settings_remote.c`). Each page as the web one builds it, from the same rudolfd
  requests and the page's own storage, with the administrator's prompt (rui_elevate) or the administrator's name and password in the
  request (rui_elevate_cred) exactly where the page asks for them:
  - Bluetooth: bt, bt/power, bt/scan, bt/visible, bt/pair|connect|disconnect|remove, the pairing question (bt/ask, bt/answer) while
    the page is open;
  - Remote access: remote, remote?on=, remote/end, remote/accounts, bd95user's ru (the rest of the accounts kept), System health's note;
  - Mobile network: mobile/status, mobile/airplane, mobile/settings, mobile/signal (every 5 s), mobile/connect, mobile/pin and
    mobile/puk; MDM_STATE and MOB_MODE read from the page at build time;
  - Network Settings: wifi/* (the password form, joining a hidden network, the radios, this computer's bands, saved networks with
    Metered and Forget, the country), net, net/static, net/dhcp, proxy, the Web search and Downloads rows with the footer (bd95net),
    net/metered, mobile/fcc, net/hotspot, vpn/*; MTR_KIND and VPN_POL read from the page;
  - Oracle: shell, shell?on=, shell/switch, siAsk, webSites (siDomain), WEB_SITES and SI_BANKS read from the page, the Web search and
    Downloads rows (one working copy with Network Settings), desktop/owenc, oraFree.
  settings.c gained set_show_keep, s_radio, set_jw, p_r2_mt, set_https_get (Test search), c_add's full 2 (flex 1), and a footer sticky
  in its own box (p_footer_box, p_footer_here: Oracle's, as the page's position: sticky within its Web search part).
  Against the web one: every view under 2 % in light, dark, light-green and dark-pink; Search the same rows; the checklists B1-B7,
  R1-R3, M1-M7, N1-N10, O1-O6. The test's rudolfd answers these pages' requests (settings_rd.json, same_rd.py).
  **Known differences (PR 6):**
  - The pairing question of a Bluetooth device and the remote-access prompt and marker are the desktop's (bt51.js btAsk, remote50.js
    rmPoll): the native shell does not ask them yet; Settings asks the pairing question only while its Bluetooth page is open.
  - Why does the Wi-Fi drop? opens Blitzen without the question filled in (the shell cannot hand it on yet).
  - Test search asks the service from Settings itself, never through the relay (the relay is a browser's workaround).
  - Add a VPN… asks for its file with the file chooser, then its name (and an OpenVPN file's login) in small dialogs, instead of the
    page's one dialog with a field to paste the file's text into.
  - Oracle shows the page's layout without real tabs (rtOn() is the page's own shell connection); on a desktop whose page has real
    tabs the page shows the same settings under "Without real tabs".
- **PR 7, done (2026-11-26, scheduled task; branch native-settings-8): Notifications, Devices, Apps** (`settings_notif.c`,
  `settings_devs.c`, `settings_apps.c`). Each page as the web one builds it, from the same rudolfd requests and the page's own storage:
  - Notifications: dnd, nsnd, ntime kept at once (persistS), Send a test notification, Clear history (bd95notif:ACCT);
  - Devices: devices, devices/unsupported, tb (tb/allow with the administrator's prompt, tb/forget), print/usb, print/jobs
    (print/cancel), eject?dev=; Open asks the shell for a new "explorer-drive DEV" (rudolf-native.c native_open_page: DEV only
    [a-z0-9], at most 15), which opens File Explorer's page with &drive=DEV and that page does what devOpenDrive does
    (oneapp59.js); 📠 Open Scan; a keyboard's Settings;
  - Apps: Default apps (files25.js OPENERS tried as appsPanel tries them, worked out at build time by mksettings.py, with the Linux
    apps that open each kind: LX_MIME, LX_MIME_ALIAS; bd95defapps:ACCT), Links (bd95links:ACCT), Installed apps (bd95rapps,
    Uninstall with the administrator's prompt), Linux (debian/status, check, update, remove, wine, the job followed;
    bd95debauto), Blitzen (ai/status, unload, remove; bd95blzkeep, bd95blzoff with the administrator's prompt), For developers
    (bd95devtools), Blitzen: what it knows (ai/know/learn, notes, forget), Blitzen › Developer (ai/dev/settings, ai/dev/token:
    what is typed goes to rudolfd's keyring and is cleared from the field), Calls to phones (the shell's new "browser https://…",
    the address checked for spaces, quotes, < > and backslashes, opened in Oracle as webOpen does).
  - The page's own dialogs and windows that Apps opens (the Windows programs from before, Change the Linux password…, See what
    Blitzen knows…, Open Blitzen Developer) open in a page of one action (do-wineold, do-debpasswd, do-blzknows, do-blzdev:
    oneapp59.js's A, which the shell's "open" allows by name), as Task Manager's Check this computer… (do-hwcheck) does.
  Against the web one (light, dark, light-green, dark-pink; the test's rudolfd answering with realistic stand-in data:
  settings_rd.json, its `_ls` the page's storage): Notifications 0.32-0.34 %; Devices 0.6-0.94 %; Apps 0.55-1.57 % (top, seven
  places on the way down, the end). Search the same rows (5, 7, 19). Checklists T1-T3, Dv1-Dv5, Ap1-Ap10. The whole test
  (settings_t.sh): 432/432.
  **Known differences (PR 7):**
  - Calls to phones' "Ask each time" (shown only once a phone app was chosen to always open numbers) is a button, not a link.
  - Uninstall of an App Store app: its open windows are not closed by Settings (the page's rappRemove closes them).
- **PR 7b, done (2026-11-26, scheduled task; branch native-settings-9, on #703's): Storage and backup** (`settings_stor.c`). Most of
  the page is the page's own state, not rudolfd's: the browser storage's estimate, the account's files in C: (encrypted with the
  account's key in the page), the Trash, the Downloads kept on the disk, Back up everything and Restore (JSZip in the page). So:
  - **The page's figures:** storage56.js's new storSnap works them out where the page is, and storDo does what needs the page
    (Empty the Trash, the old items' rule, a download's Delete) and keeps the figures in the page's storage, `bd95stor:ACCT`
    (rudolfd's copy), which the native page reads. The native page asks the shell "stor OP [ARG]" when it opens and after each change;
    the shell gives it to its hidden page (do-bg's __oaDo, `do-stor`, unseen), or a hidden page of its own while that one is not there;
    the native page draws again when newer figures come (within 8 s).
  - **What it opens** (File Explorer at the home folder or Downloads, Find duplicates…, Back up everything…, Restore a backup…, the App
    Store's Linux apps, Steam, Emu, the Trash, a download kept on the disk, Back up here, Keep my files on this drive…): "storgo W
    [ARG]", a page of one action (`do-storgo`), as Apps' dialogs.
  - In both, the shell takes OP and W only from its lists; ARG (a download's place in Downloads, a partition) is escaped into the
    address, at most 150 bytes, no control characters; the page refuses a download's place with an empty part or "..".
  - **rudolfd's parts drawn here:** On this computer's disk (storage, storage/measure, every 1.5 s while measuring), RUDOLFDATA's free
    space (info), Back up to a USB stick's drives (disks), Make a Rudolf OS USB stick (sys/status, install/images, stick/info,
    sys/urlcheck, mkstick with the administrator's prompt and the size typed as stkConfirm, the job's lines as stkProgress's bar and
    time left, then what was checked and the notification), Keep files on this drive (keepfiles, power?action=reboot).
  Against the web one (light, dark, light-green, dark-pink; a USB stick, a measured disk and RUDOLFDATA in the test's rudolfd; the
  page's own figures as the page works them out): 0.40-1.46 %. Search the same 9 rows. Checklist S0-S7. The whole test
  (settings_t.sh): 469/469 (its U15 now puts its own USB drive in place of the stand-in's stick).
  **Known differences (PR 7b):**
  - The page's own figures are as new as the hidden page's last count: a change made in another window shows once the page is opened
    again (the web page counts each time it is drawn).
  - Back up here's steps ("Checking it…", "Writing …") are said as notifications by the page of one action, not under the button.
  - Restore a backup…'s file picker opens from a page of one action: the owner's checklist.
- **The owner's checklist for PR 7 (Vivobook; `rudolf-settings notif`, `devs`, `apps` from a terminal on the native desktop until
  the switch):** 1. each page against Chromium mode's, light and dark. 2. Devices: plug in a USB stick; Open shows it in File
  Explorer; ⏏ Eject; a printer's queue, Cancel. 3. Apps: a default app chosen opens that kind of file; Linux's Check and Update now;
  Bring them in… and Change the Linux password… open the page's own dialogs; See what Blitzen knows… and Open Blitzen Developer
  open their windows; Google Voice's Open in Oracle. 4. Notifications: Send a test notification shows it for the chosen time.
- **The owner's checklist for PR 7b (`rudolf-settings stor`):** 1. against Chromium mode's, light and dark. 2. Empty the Trash: the
  Trash's line and the home folder's table change within a few seconds. 3. Open File Explorer, Find duplicates…, Back up everything…
  (the Save dialog), Restore a backup… (the file picker opens). 4. Back up here with a stick plugged in: "Backup saved". 5. Make the
  stick… on a spare stick: the size asked, the bar, "It is safe to remove the stick now". 6. On a live stick: Keep my files on this drive….
- **PR 8, done (2026-11-27; branches native-settings-10 and native-settings-pr8): Privacy, Security Settings, System health,
  Users, and the switch.**
  - Done: Privacy (`settings_priv.c`): the camera's switch (camera?on=, the administrator's prompt), Saved passwords for apps
    (keyring/list, keyring/forget, each Remove asked first), App permissions (bd95perm:ACCT), Recent activity (bd95permlog:ACCT),
    Other data (Clear clipboard history: the hidden page's, "stor clipclear"), Ads and trackers in Oracle (S.oblock; the list's size
    read from desktop/orablock61.json at build time). Views 0.38-0.55 % light and dark; Search the same 8 rows; checklist P1-P6.
  - Security Settings (`settings_sec.c`), in progress: every section drawn (status cards, Firewall, Data encryption, Website blocking,
    Antivirus, System protection, Confirm, Quarantine, Activity log, Secure Boot, Developer mode, Development link, Smart cards,
    Two-step sign-in); Apply needs the administrator's own password, checked by rudolfd (desktop/admin) as the page checks it; the
    firewall, Secure Boot's request, two-step sign-in: the administrator's prompt; the development link's Allow and Switch on… and the
    DoD certificates' Remove: the administrator's name and password with the request (rui_elevate_cred); what the page does in its own
    dialogs (encrypting, the recovery key, Secure Boot's preparation, Developer mode, the link's log, installing the DoD's certificates)
    and Scan now, Quarantine all, Restore: "storgo W" and "stor OP" (sc60.js's install moved into scDodInstall for it). Views 0.42-1.75 %
    but one place (mid5, a 1 px rounding of the page's fractional layout: 2.1-2.3 %), Search the same 37 rows, checklist S1-S8.
  - System health (`settings_health.c`): the summary of the last 7 days, Power and battery (power/status, power/set at once,
    administrators only), the development link's and the accounts' Linux users' rows, Memory (memory/settings after the administrator's
    prompt), Game mode (S.gamemode with game/set, S.gmsleep), the events by kind with each one's "⚡ Why?"; Report a problem…, the
    self-test, Last starts and Blitzen's answers in a page of one action. Views 0.44-0.87 % light and dark; Search the same 33 rows;
    checklist H1-H5.
  - Users (`settings_usr.c`): the account's own row, Sign-in options, the family's controls (each app's kind, the hours,
    pcSave), two-step sign-in (otp60.js's rows), the security keys (devkey59/fido61: the keys, Add, Remove, Require), the
    fingerprints (fprint63: the account's own and, for an administrator, every other account's), Files and administrators
    (acct50.js's acSection, each reset asked for), the developer's publisher key. Views 0.38-0.55 %; Search the same 47 rows.
  - **The six actions that need a password** run the page's own dialogs (fido61.js's fidoAdd, fidoRemove, fidoRequire;
    fprint63.js's fpEnroll, fpRemove, fpOther) in a page of one action, `#app=do-storgo&w=fidoadd` and its five siblings
    (oneapp59.js's A.storgo), as Security's do: the page holds the file key, and those dialogs are the ones the page's tests
    cover. The app draws the page again once when that window closes (the next focus, not every focus).
  - **The switch:** `rudolf-settings` is in the shell's native apps (rudolf-native.c's NATIVE_APPS), so every place that opens
    Settings -- the start menu, the pins, the taskbar's right-click, Ctrl+I, quick settings, the battery's System health, the
    page's own open -- starts the native app instead of the page, at the page asked for: `tb_settings` passes `&page=NAME` and
    the shell gives it to the app as its argument (&page=usr is rudolf-settings' own). RUDOLF_NATIVE_APPS=0 keeps the page, for
    tests and as a fallback. Chromium and Lite keep the web Settings, unchanged.
  - **Known differences (PR 8):** an already-open Settings window is brought forward rather than changed to the page asked for,
    as the page's own window is: the app reads its page when it starts. Security's mid5 view is 2.1-2.3 % (the page's fractional
    layout rounded to whole pixels), the rest under 2 %.
- **Left for PR 8:** the owner's checklist on the Vivobook (each of the four pages against Chromium mode's, light and dark;
  Add a security key, a fingerprint, Require a security key), and Security's mid5 view if the owner wants it matched exactly.
- **A note from Part 8, item 5 (2026-11-25, scheduled task, PR "Make a stick in Backup"):** Make a Rudolf OS USB stick moved
  within the page's Storage and backup into its Backup part (after "Back up to a USB stick"), with what each choice puts on it in
  plain words, a stick too small for the chosen image refused before anything is written, a bar with the step and the time
  left, and at the end what was checked and that the stick can be removed; Update and recovery keeps a line linking to it
  (`.stkmoved`). Native Settings' Storage page (still to come) carries it there, in Backup, and Update and recovery the link.
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
#### Status (2026-11-25, scheduled task): option C planned
- This plan rewritten for option C (PR #691 "Oracle's tab tooltip shows its memory", with Part 4's tooltip).
#### Status (2026-11-25, scheduled task): step 1 built (PR "Oracle's own password manager")
- **Built:** the store (rudolf-portal `--webpw list|get|set|del`, the account's login keyring, attributes application=rudolf-oracle,
  origin, user; everything on standard input); rudolfd `oracle/pw/list|get|set|del` (rudolfd-webpw.inc: as the account's Linux user
  in its session, as Blitzen's tokens; the origin and the user name checked again; Guest or nobody signed in: 409; not narrow: it
  runs rudolf-portal, as confinepure.py says); the shell's script (shell.js PW_JS, PW_FILL, pwSent, pwFocus) and Lite's
  (rudolf-webkit.c PW_JS, PW_FILL_FN, pw_msg, tab_pw_op); the page (desktop/orapw63.js: Oracle's bar, ☰ › Passwords; tabs26.js
  passes the events on).
- **Where it differs from the design above, and why:** the 🔑 is in **Oracle's own bar** (under the bookmarks bar), not drawn in
  the page's password field: a key drawn in the page could be covered, imitated or clicked by the page's own script; Oracle's bar
  is outside the page. "Change" in ☰ › Passwords takes the new password in a plain prompt, after the account's password.
- **Security review: what a page can and cannot see.**
  - The script runs in an **isolated world** (Chromium: the Controlled Frame's executeScript world; Lite: WebKit's "rudolf" world),
    in the **top frame only**, on **https: pages only**, and never in a private window's or Guest's tab (partitions priv-…; the
    page also never offers or saves in private, Tor or Guest windows). The page's own scripts cannot see it or its pad, nor call it.
  - **It gives the page nothing:** it reads what the person typed when a sign-in is sent (the form's submit, Enter in the password
    field, or its sign-in button: keys and clicks must be the person's own, isTrusted; a form the page submits itself still counts,
    which only makes Oracle ask). It tells the shell the user name and password **under a one-time pad** of 1024 random bytes that
    only the isolated world and the shell hold: Chromium carries the message as a console message, which Native's Oracle logs
    (--enable-logging), so a log would hold only the padded bytes; the pad is used once and replaced. Lite's goes by WebKit's own
    message to rudolf-webkit's process (no console).
  - **The origin is the tab's, not the page's word:** the shell and rudolf-webkit take it from the tab's committed top-level
    address, https: only. Kept for that **exact origin** (scheme, host, port): never a parent domain, a subdomain, another port,
    http: or a look-alike host (tested against the real keyring). A host with letters of other alphabets (xn--) is said in the bar.
  - **Filled only after the person's click in Oracle's bar** (Fill, Use it), only into the top frame (never a frame of another
    site), and only while the tab is still at that origin: checked by the shell (or rudolf-webkit) and again by the fill script.
  - A page can: see the password once filled (it is that site's); make Oracle ask "Save?" by submitting its own form. It cannot:
    read saved passwords, learn which sites are saved, have Oracle fill without a click, or get another site's password.
- **Where they are stored and when they are locked:** in the signed-in account's login keyring (gnome-keyring-daemon, in the
  account's own folder, its user's only, 0600, encrypted as its home folder is), **unlocked by its sign-in and locked when it signs
  out** (its keyring stops with its session). Never in the page's storage, Oracle's profile, rudolfd's files or a command line
  (each request's lines go to rudolf-portal on its standard input; rudolfd wipes its copies). ☰ › Passwords lists sites and user
  names only; Show and Copy ask for the account's password first. "Never for this site" (not secret) is in the account's settings.
- **Tests:** webpw63_t.sh 14/14 (the real gnome-keyring-daemon: kept, read back, replaced, listed by name only, removed; the
  origin's rules; nothing in a command line; the keyring file the user's only); orapwjs63_t.js 25/25 (the shell's and Lite's scripts
  in a real Chromium page's isolated world, read from their sources); orapw63_t.js 26/26 (the page); rudolf-webkit, rudolfd and
  rudolf-portal built; Oracle's page tests as before (oracle-passwords-check.yml, keyring-check.yml). **Not tested:** rudolfd's
  oracle/pw requests end to end with a signed-in account (they hand over to rudolf-portal, tested); Lite and Native with a real
  site: the owner's checklist.
- **The owner's checklist (Vivobook):** 1. Chromium mode: sign in to a test site with a made-up password: "Save your password?",
  Save; back on its sign-in page: 🔑 Fill fills it after the click; 2. a private and a Tor window: never asked; 3. ☰ › Passwords:
  Show asks for the account's password; Remove; 4. sign out of Rudolf OS and back in: still there (Settings › Privacy › Saved
  passwords for apps names it); 5. Native and Lite: the same; 6. then a real site of the owner's choice.
- **Next:** step 2 ("Open in a Chromium window" with extensions and the reminder; oracle-safety-page merged in). Found while starting
  it (2026-11-25): Rudolf OS's Chromium policy is one file for the whole computer (/etc/chromium/policies/managed/rudolf.json), read by
  every Chromium it starts (the desktop's, Oracle's own, the Tor windows', the browser window for sign-in sites), so "no extension in
  the desktop's own Chromium, the vetted list in the Chromium window" needs each to read its own policy: the Chromium window started
  in a mount namespace of its own with its policy folder bound over (rudolfd starts it as root before it becomes the account's user),
  the others keeping the blocking file; and the desktop's own Ruffle (`--load-extension`) checked against "every extension blocked"
  on the releases' Chromium before the policy lands (a first probe with Playwright's Chromium for Testing did not start its
  debugging port with a managed policy file present: to be measured on the runner with the snap's Chromium).

#### Status (2026-11-25, scheduled task): step 2 waits on the owner's decision (the desktop's own extensions and the policy)
- **Measured** (`iso/qt/native/oraextpol63_t.sh`, oracle-extensions-check.yml; the same with Chromium for Testing 153 and the
  Chromium snap's 158.0.8127.36, the releases', on the runner; the snap reads /etc/chromium-browser/policies/managed): a stand-in extension loaded with `--load-extension`, as the desktop loads Ruffle for Oracle and the web frames extension,
  under each kind of managed policy. **Any policy that blocks extensions** (ExtensionInstallBlocklist "*", or ExtensionSettings "*"
  blocked) **refuses every `--load-extension` one, even with its own ID allowed** (by either policy): Chromium says "Failed to load
  extension … Loading of unpacked extensions is disabled by the administrator" in a dialog **that stops its start** (the page never
  loads). So the policy written above ("ExtensionInstallBlocklist '*', no allowlist" on the desktop's profile) would stop the
  desktop itself. A policy unrelated to extensions, or ExtensionInstallSources empty + BlockExternalExtensions + the Web Store's
  addresses blocked, leaves it running.
- **Also:** Chromium's managed policy is one folder for the whole computer (/etc/chromium/policies/managed), read by every Chromium
  Rudolf OS starts (the desktop's, Oracle's own, Tor windows', the browser window), so the Chromium window opened from Oracle needs
  its own policy folder (a mount namespace of its own, by rudolfd, before it becomes the account's user).
- **The ways on, for the owner to choose** (nothing of step 2 changed in Rudolf OS yet):
  - **1. The desktop's own extensions packaged and force-installed** (Ruffle and the web frames extension as signed CRX files,
    ExtensionInstallForcelist from a local update address served by rudolfd), and **every other extension blocked by policy**
    (ExtensionSettings "*": blocked) on the desktop's Chromium: the owner's rule enforced by Chromium itself. Needs **a new signing key
    for those CRX files** (their IDs come from it): made offline by the owner and kept as a "release" environment secret, as the
    others; Claude never handles it.
  - **2. No new key:** keep `--load-extension`, and on the desktop's Chromium block every way an extension could arrive
    (ExtensionInstallSources empty, BlockExternalExtensions, the Chrome Web Store's addresses blocked, no extensions page in the
    kiosk) plus a check at each start (rudolfd reads the desktop profile's extensions and removes any that is not Rudolf's own,
    with a note in the health log). Weaker than 1: enforced by Rudolf OS around Chromium, not by Chromium's own "blocked" rule.
  - **3.** Ruffle and the web frames extension out of the desktop's Chromium (Flash content in Oracle's tabs on the Chromium
    desktop lost), and the strict policy: not recommended.
  Recommended: **1** (Chromium itself refuses every other extension; the key is the owner's, offline, like the others).
- The Chromium window opened from Oracle (its own policy folder, the account's own profile, the vetted list) and the reminder
  with about:safety follow the choice; about:safety's text (oracle-safety-page) is made true of it then.

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
   - **Status (2026-11-25, scheduled task): fixed what was measured** (PR "Native's transitions drawn as Chromium's"). Recorded
     frame by frame (`iso/qt/native/trans163_t.js`: ffmpeg x11grab at 60 frames a second, Notepad opened from its pin,
     minimized and restored, Chromium mode and Native under Xvfb, the soft drawing). Found: **GDK sets no X shape on a window
     it did not make**, as the frames are (frame_shape's gdk_window_shape_combine_region did nothing; xwininfo -shape: "No
     window shape defined"), so no transition showed part of a window: an opening window appeared whole in one frame (72 % of
     its box changing at once) with the light drawn around it, and minimizing and restoring made it vanish and appear whole.
     Now X's SHAPE request itself (XShapeCombineRectangles from the libXext GTK has loaded, as libXcomposite is taken) during
     a transition; an app's window opening, and a window restored from its taskbar button, is given its first moment before
     it is shown; its shadow (drawn on the desktop) waits until it is whole, as the page's clip-path cuts its box-shadow. Each
     transition's frames are counted by the shell (state "lpstat", RUDOLF_LP_LOG=1 in the log). At rest the frame stays whole
     as it was: X's shape would round its corners but also cut the corners' resize grips (native59_t.sh's corner drag), so the
     rounded corners are a step of their own (the grips as windows of their own). A window opening is not there to click
     until its transition has shown it, as with the page's clip-path (native59_t.sh waits for it). **Before 2/5, after 5/5:**
     opening's biggest single-frame change 0.72 → 0.37-0.42 of the box (Chromium mode 0.32-0.66 under Xvfb), minimizing
     0.72 → 0.40-0.52, restoring 0.72 → 0.44-0.46; 13-15 frames each 240 ms transition, no gap over 50 ms; titlebeh_t.js
     16/16; native59_t.sh as main's (its 67 and 68 fail now and then on main too). **Not measured here:** the Vivobook's
     graphics (with acceleration), the start menu's and menus' transitions (their windows draw their own clip, unchanged),
     and the light's own drawing (the lines and points are small windows without see-through, there being no compositor):
     the owner's checklist.
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
   - **Status (2026-11-25, scheduled task): fixed** (PR "Desktop icons at once on Native"). Measured with
     `iso/qt/native/deskicons163_t.js` (nine changes, each timed from the action to the shell drawing it). With the test's
     files in the browser's storage everything showed in 0.05-0.5 s; the owner's minutes came from two things the owner has
     and the test did not:
     1. **An account whose files are on the disk** (home56.js; the test's stand-in rudolfd now keeps such homes:
        same_rd.py `home/*`, DIR/diskhome). A page told that another saved (oneapp59.js oaFsReload) took the account's files
        from the browser's storage, which holds only its copy from before the move to the disk: every window showed the
        old desktop files, and the desktop's icons it gave took the others' new ones away until a window looked at the disk
        again (15 s on, if shown). Now such a page reads the disk again; one told while still signing in reads it once
        signed in (deskquick63.js).
     2. **A busy computer**: the shell drew the icons only from the whole catalogue (every app's picture drawn again, the
        start menu, the statue, the sounds), which the Vivobook's hidden page gave 192 s late while Steam started
        (rudolf-native.c's note of 2026-11-22), and the hidden page's next action waited for it (its "done" came after the
        catalogue). Now a page gives the shell the desktop's icons at once (op "desk", desk_quick: their names, kinds,
        pictures' keys, places, size, and when they were read, "dt"), the catalogue follows with the same "dt" and is drawn
        over them only when as new; the hidden page's actions are done once the icons are given.
     **Test:** the account on the disk and each picture 500 ms late (about 20 s a catalogue): **before 3/12, changes 3.5 to
     37 s; after 12/12, 0.02 to 0.9 s**; as before (files in the browser's storage) 11/11 both; appsbeh_t.js 27/27.
     **Follow-up (PR "Desktop icons: done before the icons"):** with the icons given before the hidden page's "done", the
     shell's Rename dialog was still open for a moment after a file's new icon showed, and a click meant for the desktop or the
     taskbar then was taken by the dialog (native59_t.sh's 62, 65-68 failed now and then: a pin clicked opened nothing). The
     icons are now given just after "done". native59_t.sh 79/79 three times (main: 62, 67, 68 failing in each of three).
5. **Make a stick refined, moved to Backup.** "'Make a stick' needs further refinement and to be moved from
   update/recovery to backup settings." Settings › Backup gains "Make a stick" (Update and recovery keeps a link to it
   while people learn the new place; Search in Settings finds it there); refined: the stick's size and model shown, the
   full image or boot files only explained in plain words, a stick too small refused before anything is written, the
   progress and the time left, a check of what was written, the stick safe to remove at the end; the native Settings'
   pages carry it in its new place.
   - **Status (2026-11-25, scheduled task): done for the page's Settings** (PR "Make a stick in Backup"; Chromium mode and
     Lite, desktop/stick30.js). Found: on the page it was at the end of Storage and backup, after "Keep files on this drive",
     its words about the rescue stick and Recovery mode being why it read as Update and recovery's. Now in Backup's part,
     after "Back up to a USB stick"; Update and recovery has a line "Make a Rudolf OS USB stick (a rescue stick) is now in
     Storage and backup › Backup" that opens it there; Search in Settings finds it in Storage and backup
     (settings-index.json written again). Refined: each stick with its name, model and size (as before); what each choice
     puts on it in plain words (this Rudolf OS, the update address's, a whole disc image, boot files only); a stick too
     small for the chosen image refused before anything is asked or written (its button off and why said; mkstick.sh still
     checks this Rudolf OS's own size before writing); a bar with the step (from mkstick.sh's and install.sh's lines) and
     the time left (estimated from the time so far); at the end "Checked: what is on the stick matches. It is safe to
     remove the stick now." **Test:** `iso/qt/shell2/stick163_t.js` (stick163-check.yml, rudolfd's answers given by the
     test): **3/10 before, 10/10 after.** Native Settings' pages (Part 3) carry it when their Storage page is made: a
     note left in Part 3's status. The bar moves by steps: install.sh's copy (step 1) says nothing until it is done, so
     the time left is rough during it.
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
7. **Mute notifications** (the owner, 2026-11-26 05:25 CST): "Add a function to mute/unmute desktop notifications. The
   mute/unmute checkbox can be in the expanding menu for the notification button on the taskbar." Asked what it should do
   beside Do not disturb, the owner chose "Mute everything": a **Mute** checkbox in the notification centre's heading, left
   of Do not disturb. While on: no pop-up and no sound for any notification, an error's included; each is still kept in
   the centre and counted on the 🔔, which shows 🔕. Kept with the account's settings (`S.nmute`, bd95set) like Do not
   disturb. Native draws the same box and holds back its own pop-ups too (rudolf-native.c nc_check, toast_addf). Tested
   by qsbeh_t.js on both engines (on, off, kept; no pop-up while on). Native Settings' Notifications page (Part 3) may
   add the same switch when it is next changed.
8. **The setup wizard in the chosen language** (the owner, 2026-11-26, from FreeBuff's user test of a fresh install):
   choosing Español on the wizard's first page left steps 2-5 in English. Its 23 sentences are now rows of I18N_ROWS
   (FreeBuff's translations; the Portuguese made Brazilian like the rest, Settings' and Accessibility's names as the
   desktop's own); "Step n of m", Back/Next/Finish and the dialog's name come from T_; a red error goes once corrected.
   The password minimum stays 4 (the owner: "Keep 4 for now"). Test: oobe63_t.js. FreeBuff's other findings (the
   Terminal's HELP, the sign-in date's language, "Guest", Spanish desktop labels running together, windows opening in one
   place, Emu's first prompt) are not in it yet.
9. **The Command Prompt's HELP** (the owner, 2026-11-26, FreeBuff's text): HELP = a short list in groups with examples,
   HELP ALL = every command, HELP <name> = one explained (desktop/termhelp63.js); RPK's extra "packages: rpk help" line
   gone. Checked against what the commands do: EXPLORER opens the file manager (not "in this folder"); `CD \` did not go
   to the top (resolve() now treats a leading \ as C:\); EDIT of a missing file showed "cannot be opened" (it now makes
   the empty file where the account may write, then opens it). Test: termhelp63_t.js.
10. **FreeBuff's second round** (2026-11-26, the owner's screenshots), **done:** Paint's Save
   asked "relative to C:" with no folder picker or Cancel and saved to C:\; now the Save dialog Notepad has, in Pictures,
   with Cancel (paint-savedlg, paintsavedlg63_t.js). The ☰ menu's power button shut down at once with no menu, and closeAll()
   closed windows whatever they answered, so unsaved work was lost: now Shut down, Restart and Sign out close windows as
   their ✕ does and stop on Keep open; ⏻ Power gives the choice in the browser edition; the Shut down dialog has Sign out
   (power-menu, power63.js, power63_t.js). New text document's naming box said "Rename": now "Name your new file"
   / folder and Create, .TXT given when left out, the same on Native (newfile-name, newfile63_t.js, native59_t.sh 61-62);
   not opened by itself once named, as on Windows. **Not done yet:** Guest keeps nothing and
   says so only in Settings (a line at sign-in and a notice on the first save); double-clicking a picture opens Paint, not a viewer; a wrong user name
   once gave no message at sign-in (unconfirmed); a battery shown on a computer without one (browser edition).
**The owner's checklist:** each item on the Vivobook (Native and Chromium).

### Part 9. Native's camera and microphone: PipeWire capture (found on the Vivobook, 2026-11-27)

**Status (2026-11-27): begun, on `pw-beside-pulse`.** The owner chose PipeWire *beside* PulseAudio: PulseAudio stays the
sound server and keeps every card; WirePlumber's ALSA monitor is off, so nothing opens a card twice, and what runs is the
daemon, its session manager and the camera's monitors (no `pipewire-pulse`). In: `systools/mkpipewire.sh` (the daemon,
WirePlumber, libpipewire, the SPA plug-ins and both packages' share trees: dpkg/ldd discovery, named failures, a 12 MB
budget), `systools/pwconf/51-no-alsa.lua` for WirePlumber 0.4 and `51-no-alsa.conf` for 0.5, the start in
`usr/lib/rudolf/sound` (after PulseAudio's socket; `restart` stops both), the wiring in `mkrootfs.sh` and
`ci_release.sh` (with its `BSRC` list), the packages in `rudolf-release.yml`, `pipewire` moved into `addwebkit.sh`'s
`FATAL` list (a capture build without the plug-in lists a camera it cannot open), and
`.github/workflows/pipewire-check.yml`, which installs the real packages, packages the layer, starts it under the
layer's own configuration and fails on any audio node or ALSA device.
**The capture rebuild, and the two switches it meets.** `addwebkit.sh` now names `gstreamer1.0-pipewire` when it stops a
build (its comment still said the plug-in was a warning), the Lite engine check installs the daemon's packages and holds
the layer to its three capture plug-ins (pipewire, interleave, transcode, the first reaching libpipewire), and the
self-test's `capture` line fails when the pipewire plug-in or the daemon is missing instead of saying the camera waits.
Settings › Privacy › Camera's switch stops WirePlumber before unloading the camera's driver -- WirePlumber's v4l2 monitor
keeps the device open, so every "camera off" would have answered "the camera is in use" -- and starts it again when the
camera comes back, which `iso/qt/host/camwp_t.sh` checks in the PipeWire check.
**Where the daemon comes from.** Both build paths carry it: `mkrootfs.sh` runs `systools/mkpipewire.sh` for the layer it
makes -- so the ARM64 build, which builds through `mkrootfs.sh`, gets it too (its base layer's log reads
`pipewire: 9019 KB`) -- and `ci_release.sh` runs the same script against the release's base layer. `mklayers.sh` keeps
every file of it in `base.sfs`: it takes out only `opt/chromium`, `usr/share/rudolf/www` and `usr/lib/modules`. WendigOS
builds the same layers from `main` and has no build switch around this, so it gets them when this part merges; the
`kryptonite` branch (last at `ec2edfa9`, 2026-11-23) has neither `mkpipewire.sh` nor the call in its `mkrootfs.sh`, so it
must merge `main` before a layer built from it has the daemon -- until then the self-test's `capture` line fails there.
**The layer itself, not the tree.** `systools/layercheck.sh` unpacks nothing: it reads a packed layer's file list and wants
the daemon and WirePlumber (executable files), the camera's `spa-0.2/v4l2/libspa-v4l2.so`, the `51-no-alsa` drop-in and
no `pipewire-pulse`; it names whatever is missing and fails. It runs on the base layer the ARM64 build makes
(`arm64-check.yml`), on the one `ci_release.sh` packs for a release, and on the one the PipeWire check makes with
`mklayers.sh` from `mkpipewire.sh`'s own tree; its nine cases are a host test needing no squashfs-tools
(`iso/qt/host/layercheck163_t.sh`, its own job in the PipeWire check).

**The fault.** On the Native desktop Camera answers "No camera is available or access is blocked" with the privacy switch
on, the check's microphone reads −120 dB (all-zero samples) and Voice Recorder records nothing; on the same build and
machine Chromium does all three (checklist V1–V3; findings N33 and N35 in docs/native-findings-2026-11-26.md).

**What the machine says** (devlink `000652` and `000653`, read-only):
- `libwebkit2gtk-4.1.so.0` names `pipewiresrc` and `pipewiredeviceprovider` and nothing else: `v4l2src` and
  `v4l2deviceprovider` appear **zero** times. Its classes are `PipeWireCaptureDeviceManager` and "WebKit PipeWire Capture
  Device Manager", it has `OpenPipeWireRemote`, and it carries "PipeWire Device Provider is missing or too old. Please
  install PipeWire >= 0.3.64."
- The image has no PipeWire in any form: no `libpipewire-0.3` under `/usr/lib`, no `pipewire` or `wireplumber` program,
  and no `libgstpipewire.so` among the 39 GStreamer plug-ins. Its sound server is PulseAudio 16.1 (`iso/sndbuild.sh`).
- The hardware is fine: `/dev/video0` and `/dev/video1` are there, `uvcvideo` bound to the UVC camera.

**Therefore.** No camera plug-in can fix this: Ubuntu 24.04's WebKitGTK 2.52 captures only through PipeWire, so the image
needs all four of (1) `libpipewire-0.3` ≥ 0.3.64, (2) the PipeWire daemon, (3) WirePlumber as its session manager with a
monitor that publishes the v4l2 camera as a node, and (4) the `pipewire` GStreamer plug-in (`gstreamer1.0-pipewire`),
which `iso/systools/addwebkit.sh` names in its copy list since 2026-11-27.

**The work.**
1. Bring PipeWire and WirePlumber into the root file system as PulseAudio is brought (`iso/sndbuild.sh`, and the sound
   server section of `iso/mkrootfs.sh`): the programs, `libpipewire-0.3`, the SPA plug-ins (at least `v4l2` and
   `support`), `/usr/share/pipewire` and `/usr/share/wireplumber`, and WirePlumber's D-Bus policy files.
2. Start them with the session, not at boot, and decide with the owner whether PipeWire runs **beside** PulseAudio or
   **as** the sound server in its place. Beside is the smaller step and keeps `rudolf-sound`'s PulseAudio commands
   (`sound`, `hotkey mic`, the volume overlay) working.
3. If it runs beside PulseAudio, turn WirePlumber's ALSA monitor off in its configuration, leaving the v4l2 (and libcamera)
   monitors: otherwise it takes the sound cards PulseAudio is using.
   `addwebkit.sh` already refuses to build when `interleave` or `transcode` is missing (`addwebkit.sh` exits 1, and the
   release build stops): `pipewire` only warns there until this Part lands, since its plug-in has nothing to reach without
   the server. When this Part installs PipeWire, add `pipewire` to that script's `FATAL` list in the same change.
4. **The microphone and the Recorder are not this camera fault** (findings N35 point 3): its source is `pulsesrc`, and what stops its WebAudio
   path — the one the level meter reads — is a GStreamer element the image does not carry: `deinterleave`, from
   gst-plugins-good's `interleave` plug-in (`libgstinterleave.so`), so each attempt ends `not-linked`. That plug-in is now
   in `addwebkit.sh`'s list; moving the sound server from PulseAudio to PipeWire is a part of its own and has nothing to do
   with it. **The Recorder is a third fault and needs its own plug-in:** WebKitGTK 2.52's `MediaRecorderPrivateGStreamer`
   builds `uritranscodebin` (GStreamer Bad's `transcode` plug-in, `libgsttranscode.so`), which the image also lacks — so
   `rec.start()` throws `NotSupportedError` and the button stays dead. `transcode` is in `addwebkit.sh`'s list now, and its
   library pulls `libgsttranscoder-1.0.so.0` with it through the script's existing `ldd` pass. **This is V3's fault as well
   as V1's:** the Camera records its video through the same `MediaRecorder` (camera track plus microphone audio in one
   `MediaStream`, `apps25b.js`), so a working camera would still leave "Record video" dead until `libgsttranscode.so` is
   in the image — V3 needs both fixes, and needs them for different reasons.
5. **Test on the Vivobook** (checklist V2 and V3): the camera's preview at 640×480, a photo and a 5 s video saved and
   opened. Then with PipeWire stopped, to see what the app says. Two checks for two paths: the hardware check's microphone
   step reads through WebAudio, so it needs `libgstinterleave.so` and should move off −120 dB; V1 and the Camera's
   "Record video" (V3) are the Recorder, which needs `libgsttranscode.so` and should then save a clip. The self-test's
   `capture` line now asks for the camera's side too: with PipeWire in the image it fails when the pipewire plug-in or the
   daemon is missing (the camera cannot capture), names which, and passes only with all three. (Host test:
   `iso/qt/host/selftest_t.sh` 12a–12f, which holds the two microphone plug-ins in, then takes the camera's plug-in and
   the daemon away, each named.) If either stays dead, its own log line names the factory that is still missing.
6. **Cost:** two more daemons and their libraries, and their memory; read the native desktop's idle figure (checklist V42)
   before and after.

**Not this Part:** Oracle's WebRTC, which Native's WebKit does not have at all (docs/plan-1.5.9.md 7.1), and the camera in
Linux apps, which use the Debian container's own stack.

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
