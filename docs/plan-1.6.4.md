# Rudolf OS 1.6.4: the account's own folders, an extension from a file, firmware seen, installing beside Windows, the Archive Manager's rest, and File Explorer made native

1.6.4 follows 1.6.3. The owner, 2026-11-28: **"Add gaps 1, 2, 4, 6, 7, and 8 from the list above to the plan for 1.6.4.
Gap numbers 3, 5, and 9 will be addressed as part of the 2.0 WendigOS project."** The list is
`docs/gap-analysis-1.6.3.md`'s nine ranked gaps; this plan takes six of them, in its order, and the other three are
2.0's (`docs/plan-2.0.md`: the newest kernel and the "next" channel, Wayland, and Waydroid in the phone build).

**Where the six come from.** Three are new work (gap 1, gap 2, gap 4), two are parts already written for 1.6.3 that
nothing of was built (gap 6 is 1.6.3's Part 6, gap 7 is 1.6.3's Part 10, items 2 to 8), and one is a piece of 2.0's
wave 2 pulled forward because it is the desktop's most-used window (gap 8, File Explorer, step 1). This plan is the
whole of them: the parts below are their items, and 1.6.3's plan keeps what was written there with a line saying where
it went, as `plan-1.6.3.md` does for the parts it took from 1.6.2.

## Parts
A part's numbered bullet points are its **items**: a reference reads "Part 5, item 2" (AGENTS.md, Terms).

### Part 1. The account's own folders (gap 1; the first part of 1.6.4)
`dl/*`'s `acct=` was compared with the signed-in account in the Archive Manager's PRs (#754, #755, #757): the list, the
bytes, the ZIP, the extraction, the quarantine and the clear all belong to the account that signed in, with the guest's
and the shared store open to nobody's own. `home/*` was not, and there is no administrator's way to open another
account's folder on purpose.
- **1. `home/*`'s account is the session's.** `home/list`, `home/read`, `home/tree`, `home/write`, `home/delete`,
  `home/copy` and every other request that takes `acct=` compare it with the session (`/run/rudolf/home-session`), as
  `dl/*` does: the account signed in, or nothing is answered (`{ok:false}` 403, `"another account's folder"`). `home` —
  the list the sign-in screen's own pages and the session need before anyone has signed in — stays as it is, and so does
  every request while nobody is signed in, which is what keeps the sign-in housekeeping and the host checks working.
- **2. One administrator's way in, and only that.** An administrator opening an account's folder deliberately (the
  Storage page's "Open another account's files", the system report's collection) goes through the one-time answer
  `desk_admin_check` already gives the native apps: `home/*` also accepts `t=<the one-time answer>` in place of the
  session, and the answer names the account it was given for. A standard user has no such answer, and no answer is
  accepted for the vault: the folder is still locked until its account signs in.
- **3. The downloads store's account ids are the account's own.** `dlAcct()` lower-cases the id and `dl_acct_ok` takes
  lower case only, so two accounts whose ids differ in case would share one folder under
  `/home/rudolf/Downloads/.accounts/`. Accounts are made with lower-case ids already (`au_add`), and a request keeps
  refusing anything else; this item makes the store ask the account list when it makes a folder rather than trusting its
  own lower-casing, and says so in the security log when it finds a collision (an older computer's store).
- **4. `home/list` says what it is for.** It names every account and which folders are open, which any page of the
  desktop can read. It is the sign-in screen's and the session's list; it gains the caller's own session in its answer
  and Settings › Users stops asking it for anything but its own account. (Narrowing it to the sign-in window is the
  item after it, if the owner wants it: the desktop's own pages read it today.)
- **The tests:** `iso/qt/host/home164_t.sh` (new): `home/read?acct=`, `home/write?acct=`, `home/tree?acct=`,
  `home/delete?acct=` and `home/copy?acct=` for another account refused while one is signed in; the same allowed for the
  account signed in; `home/list` unchanged for the sign-in screen and for nobody signed in; the one-time answer from an
  administrator letting one account's folder through once and not twice; a standard user's answer refused; a store with
  two ids differing in case made once and named in the log. The existing `home*` cases of `rudolfd_t.sh` and of the
  desktop's own checks stay green (they run with nobody signed in).
- **Never weakened:** each account's files stay under its own fscrypt key, unreadable while it is signed out, root
  included; the guest's files stay in memory only; nothing here widens what a page can name without the session.

### Part 2. An extension from a file (gap 2)
The base is one signed, read-only image and a system part arrives as a signed system extension — which only this
project can publish, because the extension's signature is checked against the release key in `iso/trusted-keys.txt` and
the workflow that builds one runs in this repository. A printer's filter, a vendor's driver, a tool one computer needs
therefore waits for a release.
- **1. Install from a file…** Settings › Update and recovery › System extensions gains it: a file picker, the
  extension's name, version, what it covers (its files, its kernel modules, the services it starts) and its signature,
  then Install. The same row in the native Settings' Update and recovery page, and `rpk` learns `rpk ext add FILE`
  (`--yes` for scripts; the same prompt otherwise).
- **2. Everything a published extension gets, and nothing more.** Its signature is checked against the trusted release
  key only (never a key of its own, never a developer's key: that is the Library's chain, for apps); its `base=` and
  `kernel=` must be this base and this kernel; it may not replace or remove anything of Rudolf OS's (`/usr/bin/rudolf-*`,
  the base's own files, the kernel, the start-up files); it is **left out** rather than stopping the start-up if it
  fails to apply, and the computer starts without it and says so once; **Start once without extensions** stays a boot
  entry, so a person is never locked out by one; the security log records the install, the signature's key and the
  files it added, and removal takes them away again.
- **3. What the settings page says about the risk,** in plain words: an extension runs inside Rudolf OS's own read-only
  base, it is signed by this project's key, and a file from anywhere else is refused — with the check that a tampered
  copy (one byte changed) is refused before anything is written.
- **The tests:** `iso/qt/host/extfile164_t.sh` (new), with the checks' own signing key: a good extension installed from a
  file, applied at the next start, seen in its list, removed; one signed by another key refused; one for another base or
  kernel refused; one that replaces `/usr/bin/rudolf-settings` refused; one whose apply fails left out and the computer
  started; the boot entry without extensions; `rpk ext add` from a script; and the native page's rows against the web
  one. The screenshots under 2 % as every native page is measured.
- **The owner's checklist:** a real vendor extension on the Vivobook (an NVIDIA driver extension is the one this project
  already publishes), installed from a file, removed, and the computer's next start.

### Part 3. Device firmware, seen and not touched (gap 4)
Nothing here lists or updates firmware; a mainstream system does. The machines this is tested on — the Vivobook, the
Asus desktop — have firmware that occasionally needs it, and today it is a manual job on another computer.
- **1. `fwupd`, read-only, in the base layer.** `fwupd` with what it needs (libxmlb, libjcat, gusb, glib, libarchive,
  libcurl's own copy already there), built by `iso/systools/mkfwupd.sh` the way `mkfprint.sh` builds libfprint: dpkg/ldd
  discovery, named failures, its own budget, `ci_release.sh` failing over it. Two ways to run it, both narrow: the
  daemon as a root helper asked by rudolfd (as `fprint` is), or `fwupdtool` run per request with its cache in
  PERSIST — chosen by the size and by whether the daemon's D-Bus is worth it, with the answer written here when the
  first PR is made. Either way: **CAP_DAC_READ_SEARCH and no more**, dropped before it reads a device, Landlock with
  the confinement switch on, and nothing listens.
- **2. Settings › Devices shows them.** Each device `fwupd` knows: its name, its vendor, its current firmware version,
  and "Update available: VERSION" where there is one, with the update's vendor and its own notes' first lines. Nothing
  is downloaded or written; the row says plainly what updating needs ("Update it on another computer with the vendor's
  tool", or the vendor's own instructions) — and the flashing itself is not this part.
- **3. The system report says them too,** so a report from a computer in trouble carries what its firmware is.
- **4. Off unless asked.** The daemon (or the tool) is started only when the page is opened or the report is made;
  nothing polls the firmware at idle or at start-up.
- **The tests:** `iso/qt/host/fwupd164_t.sh` (new), with a stand-in `fwupdtool`'s own JSON (a laptop with three devices,
  one having an update, one not supported): the page's rows, the report's section, the request refused for a standard
  user when the daemon is the route, and the tool not started at idle (the process list before and after a session).
  The screenshots against the web Settings under 2 %, light and dark.

### Part 4. Installing beside Windows on the same drive (gap 6; 1.6.3's Part 6, moved here whole)
Nothing of it is built. Written in `docs/plan-1.6.3.md` under "Part 6", kept there as written; the substance, with its
own numbering here:
- **1. Room, made in Windows first.** The person shrinks Windows' partition in Windows' own Disk Management, which knows
  NTFS, BitLocker, the page file and hibernation; Rudolf OS uses only unallocated space and never resizes or writes to a
  Windows partition. The installer shows the disk's partitions by name, size and kind (EFI system, Microsoft reserved,
  Windows, recovery, free) and offers only a free area of at least 16 GB; Rudolf OS's guide and Settings' text say how,
  with the BitLocker recovery key and Windows' Fast Startup named.
- **2. What Rudolf OS writes, and nothing else.** One new partition in that free space, RUDOLFDATA, and Rudolf OS's
  start-up files in the disk's existing EFI system partition in their own folder only (`\EFI\rudolf\`), never touching
  `\EFI\Microsoft\` or `\EFI\Boot\`; if the EFI system partition is too small for them, it stops and says so.
- **3. Choosing at start-up.** A UEFI boot entry "Rudolf OS" (shim), put first; Rudolf's start-up menu gets a "Windows"
  entry that starts Windows Boot Manager from the same EFI system partition, so the person picks each time, with a short
  wait and Rudolf OS as the default (changeable in Settings › Update and recovery). A Windows update that puts Windows
  Boot Manager first again: Rudolf OS puts its entry back at its next start, and System health says so.
- **4. Secure Boot stays on for both,** Rudolf OS through Microsoft-signed shim as now, nothing enrolled on Windows'
  behalf.
- **The checks:** the installer's own host tests against a made-up GPT (partitions found, the free area chosen, the
  files written where they belong, a Windows partition never opened for writing); QEMU with Windows and Rudolf OS on one
  disk (both start, the menu, the entry put back); the owner's checklist on the Asus desktop, with Windows still
  starting afterwards.
- **Not in this part:** resizing or shrinking anything from Rudolf OS, a BIOS/MBR disk (whole-disk only), repairing an
  unbootable Windows, and Windows' own files' access from Rudolf OS (the partition is not mounted).

### Part 5. The Archive Manager, items 2 to 8 (gap 7; 1.6.3's Part 10, items 2 to 8, moved here)
Item 1 is the PRs of 2026-11-28 (#754, #757; #755 merged into it): one path for every archive on the disk, whatever its
size, with the antivirus by path and the guard that only ever touches the folder the extraction made. What is left is
what a person notices next; each item its own PR, as `docs/plan-1.6.3.md` writes them (kept there as written).
- **1. Choose where it goes, and merge when asked.** `Extract here`, `Extract to…` (a folder picker over the account's
  files and the drives) and "Extract into this folder" with the question when a name is already there (Replace, Keep
  both, Cancel for all); `dlzip.sh` takes the destination as a line of its request file.
- **2. Say what was changed.** A summary line after extraction ("12 names were changed"), names kept as they are
  otherwise (no upper-casing, no 60-character cut), cut only what the filesystem refuses.
- **3. One file, and the ones you ticked, from a big archive.** `dl/zip` gains entry contents (one entry, a range of
  bytes for a preview) and `dl/unzip` a list of names, so `Extract selected` and a preview work on the disk path as they
  do in memory.
- **4. Install from the archive.** When an archive holds an installer (`.RAPP`, `.DEB`, `.EXE`, `.MSI`, `.AppImage`,
  `.bin`, `.run`, an `install.sh`, a folder with a `.desktop` file) the window names them above the list and offers
  `Install…`, which hands the file to what already exists (`rappInstall`, `installWizard`, with the administrator prompt
  they use from Explorer); an app's own update is named as one.
- **5. Read more, and say what is not read.** TAR.GZ, and whatever Info-ZIP already does (a password field for an
  encrypted ZIP, through the request file so it never appears in a command line); `7z` through a Linux app if one is
  there, or a plain sentence for the formats it cannot read; the TAR reader learns GNU long names and pax and drops
  links rather than following them.
- **6. A damaged archive is named as damaged, before anything is written.** `unzip -t` in the list step, the count of
  files that fail their checksum in the window, and an extraction into a new folder that is all-or-nothing (its hidden
  folder removed when it stops).
- **7. The window's small things.** Sort and a filter box, Select all/Select none, the ticked files' count and size in
  the status line, `Extract selected` disabled until something is ticked, Enter extracting, `Add a folder…` and
  `Remove` inside the ZIP, and the archive's own packed and unpacked totals in its title.
- **The tests, item by item:** `iso/qt/host/dlzip164_t.sh` (new) for rudolfd's half (a destination from the request
  file, a name that is there with each answer, an entry's bytes and a range, a list of names to extract, a TAR.GZ with
  long names, an encrypted ZIP's password never in a command line, a ZIP with one byte flipped) and
  `iso/qt/shell2/dlzip164_t.js` (new) for the page's half (one folder from a second extraction, the changed names said,
  tick two of 5,000 and extract exactly those, the installers named, the filter and the disabled button). The
  antivirus, the sandbox and the administrator prompts stay exactly as they are and are used, never widened; item 1's
  guard (`dlz_in_dest`, the `acct=` check) is what every item here builds on.

### Part 6. File Explorer as a native program, step 1 (gap 8; taken from 2.0's wave 2)
On the native desktop File Explorer is a page of the whole desktop in a WebKit window: about 100 MB for a window that
should cost 15 to 40 MB — it is the desktop's most-used window, which is why this one step comes forward out of 2.0's
wave 2 (`docs/plan-2.0.md`, "The order"), and 2.0 keeps the rest of the wave.
- **1. The window and the folders.** `/usr/bin/rudolf-files` on librudolf-ui, as the native Settings and the Office
  apps are built: the page's window (its sizes and its two panes), the sidebar (Quick access, This PC, the drives and
  what is plugged in), the folder view (icons and details, the columns the page shows, sort by column, the tick and the
  status line), the address bar with its own places, back/forward/up, New folder and the context menu's own rows.
- **2. The same requests the page makes, and no new ones.** `home/*` and `file/*` through rudolfd exactly as
  `files25.js` asks them (so Part 1's session check is what the native app meets too), the drives from `disk/*`, the
  trash as the page has it. The checklist is the page's own, made from its code and its tests.
- **3. What it does not do yet, said as such:** copy, move, rename and delete in bulk (the page's own drag), the
  preview pane's viewers, search inside files, ZIP and extraction (Archive Manager's, and Part 5's when it lands),
  Wine's and Linux apps' own folders beyond what the page shows, and the drive tools. Each is drawn as the page draws a
  row it cannot choose, or left out of the native window's menu, and the page stays the way to them.
- **4. The switch.** Native opens `rudolf-files` for File Explorer (the Start menu, the taskbar, the desktop's `This
  PC`, `open explorer` asked of the shell, an app's "Show in folder") only once the whole step passes its checklist;
  Chromium and Lite keep the page.
- **The tests:** `iso/qt/native/files164_t.sh` (new) as `settings_t.sh` is: built with `RUI_WERROR=1`, checked
  hardened, used under Xvfb with a stand-in rudolfd (a home with folders, files, a drive, an empty folder, a file it
  cannot read): the window and its title, the sidebar's places, opening a folder and going back, the details' columns
  and a sort, the address bar, New folder, the context menu's rows, and a folder of 5,000 entries drawn and scrolled;
  every view compared with the web File Explorer **under 2 %** (light and dark, two accent colours); the memory at idle
  with a folder open against the page's (expected 15-40 MB against ~100 MB), recorded in the PR.
- **The owner's checklist:** File Explorer on the Vivobook against Chromium mode's — the folders he uses, a USB drive,
  a folder of photos, and Task Manager's memory for both.

## The order
The owner's list is the gap analysis' ranking, and this plan keeps that numbering: Part 1 is a security gap, Part 5
continues code that is in review today, Part 6 is the one part a person feels every day, and Parts 2, 3 and 4 are the
longest (Part 4 needs a Windows computer to test on).
1. **Part 1** — the account's own folders (a hole, and small).
2. **Part 5** — the Archive Manager's items 2 to 8 (its item 1 is in review).
3. **Part 6** — File Explorer, step 1.
4. **Part 2** — an extension from a file.
5. **Part 3** — firmware, seen and not touched.
6. **Part 4** — installing beside Windows.

## What 1.6.4 weighs
Measured in the repository: the images at release 2026112219 are base 297 MB, Chromium 131 MB, modules 97 MB and
desktop 7 MB (532 MB unpacked, **578 MB** packed with zstd, 1.6.2's figure), and idle memory on the Vivobook (3,416 MB)
is 854 MB (Native, 1.5.9) with the X11 baseline at 1,008 MB (1.6.1). What the six parts add is an estimate, from the
per-part figures the plans already record:

| Part | Disk | At idle |
|---|---|---|
| 1. The account's own folders | 0 | 0 |
| 2. An extension from a file | 0–2 MB (`rpk` and the settings' rows) | 0 |
| 3. Firmware, seen | +8–15 MB (`fwupd`, libxmlb, libjcat, gusb, libarchive) | 0 (nothing runs until the page is opened); +15–25 MB while it is |
| 4. Installing beside Windows | 0–2 MB (the installer's own code; `sfdisk` and `blkid` are in the base) | 0 |
| 5. The Archive Manager, items 2 to 8 | 0–2 MB (7z only if a Linux app is there; TAR.GZ uses what ships) | 0 |
| 6. File Explorer, native | +1–3 MB (one program; native Settings is 587 KB stripped) | 0 at idle; **about −80 MB** with a window open (15–40 MB instead of ~100 MB) |

So **1.6.4 lands the images at roughly 590–600 MB** (from 578), and an update still wants about 1 GB free while the new
image set downloads. Idle memory does not move: nothing in these six parts runs at idle, and the one saving is a window
that is open (Part 6). 2.0's under-700 MB target stays 2.0's, with its trimming work.

## Not in 1.6.4
- **Gaps 3, 5 and 9 are 2.0's** (the owner, 2026-11-28): the newest kernel and the "next" channel on `kryptonite`
  (`docs/plan-2.0.md`), Wayland with W0 (B)'s stick before W1, and Waydroid with the phone build — Waydroid alone is
  1.5–2.5 GB on the disk and 1–1.5 GB while Android runs, against the Vivobook's 3.3 GB of memory, which is why it is
  measured before it is offered and never part of a 1.6.x image.
- **1.6.3's own unfinished parts stay in 1.6.3:** the Office apps' remaining steps (Part 1), the native Settings' pages
  and its switch (Part 3), the native apps' administrator prompt with a finger (Part 2's left item), Oracle's encrypted
  websites in the page (Part 5, which waits on its proof), Oracle's extensions and password manager (Part 7, step 2
  waiting on the owner's decision), PipeWire's capture (Part 9), and Part 8's own list.
- **Anything of 2.0's waves beyond File Explorer's step 1:** the App Store, Packages, the archive window's own native
  version, and the rest of wave 2.
