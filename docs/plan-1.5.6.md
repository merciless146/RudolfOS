# Rudolf OS 1.5.6: your files on the disk

Item 8 of the file management recommendations (2026-11-08). 1.5.5 refines File Explorer
(docs/plan-1.5.5.md); 1.5.6 changes where your files are kept. On a computer with a RUDOLFDATA partition (installed,
or a USB stick with one), every account's home folder becomes real files on that partition. Drive C: becomes the
desktop's view of them. The version that runs in an ordinary web browser, and live sessions without RUDOLFDATA, keep
today's storage.

## How it works today (checked in the code)

- **Drive C:** is one tree of JavaScript objects (`fs`) in the desktop page:
  - folders are objects;
  - text files are strings;
  - other files are `data:` URLs (base64, about a third larger than the file).
- **Saving:** `fsSave()` (73 calls across the apps) schedules `fsFlush()` 300 ms later. It writes two kinds of record:
  - `fs:system`: C: without the homes, in IndexedDB;
  - each signed-in account's **whole home folder as one record**, `home:<id>`. `homeSave` serialises the home to
    one JSON string, encrypts it with the account's file key (AES-GCM, 1.5.0) and writes it.

  So one changed letter in Notepad re-encrypts and rewrites every file in that home. The bigger the home, the slower
  each save, and the closer to the browser's storage quota.
- **rudolfd mirrors those records** on the disk (`/home/rudolf/.rudolf-idb`), so the shell and the plain page share
  them.
- **Large downloads (over 1.5 MB, 1.2.1)** cannot fit in that record. They stay on the disk as files
  (`/home/rudolf/Downloads/.accounts/<id>/`, 1.5.5), and C: holds an entry pointing at each (`rudolfdisk:`).
  Viewers read them in pieces through `/dlf/` links. This is the only part of C: that is real files today.
- **Linux apps** see Documents, Pictures, Downloads and Games in `/home/rudolf/...`. `lxSync` (linux26.js) copies
  files back and forth between those folders and C:. Two copies, kept in step.
- **"Encrypt my data" (1.5.0)** encrypts the disk folders for Debian, Flatpak apps and Downloads with the kernel
  (fscrypt, `/persist/vault`). Each account's home is encrypted by the desktop instead. Two encryption systems.
- **Administrators can open accounts' files** (acct50.js): every account's file key is also wrapped for each
  administrator.
- **Backups** zip every record (still encrypted).

## What changes

- **One copy of every file, as a real file:**
  - opening and saving touch only that file;
  - no size limit beyond the disk;
  - no base64;
  - no 1.5 MB rule for downloads.
- **Linux apps, Steam, Wine and the desktop see the same folders.** No copying in either direction.
- **One encryption system:**
  - each account's folder is encrypted by the kernel (fscrypt) with the account's own file key;
  - the key is put into the kernel when the account signs in and taken out when it signs out;
  - administrators' access works as today, through their wrapped copy of the key.
- **Storage, search, duplicates and backup work on the disk directly.** rudolfd, `du`, `find`.

## Stage 1, as built

- **rudolfd `home/*`** in `iso/sys/rudolfd-home.inc`, included by rudolfd.c. The folder is
  `/persist/accounts/<id>`, not `/persist/home/<id>`: `/persist/home/rudolf` is already the Linux user's home.
  - Requests: `GET home`, `unlock` (with `session=1` for the binds), `lock`, `tree`, `list`, `stat`, `read` (ranges),
    `write` (base version, `new`, pieces with `part=1` and `final=1`, `mkdirs=1`), `mkdir`, `rename` (`replace=1`),
    `copy`, `delete`.
  - Every path is walked with `openat(O_NOFOLLOW)`. A key that another open account uses is never taken out.
- **The binds cover Documents, Pictures, Music and Videos only.** Downloads joins them in stage 4, when Chromium's
  `.incoming` moves out of `/home/rudolf/Downloads`; otherwise the bind would hide it.
- **Not yet:** `watch` (inotify) comes with stage 2, where the desktop uses it.
- **Test:** host `homeapi_t.sh`, 27/27, on a real ext4 drive with the encrypt feature (a loop device):
  - the folder is made with the key's policy, and another account's key is refused;
  - after `lock` the names and contents can't be read, also by root;
  - paths, links, conflicts, pieces, rename, copy and delete;
  - the binds.

## Stage 2, as built

- **New desktop module `home56.js`.** It decides which accounts keep their files on the disk: an account whose
  first sign-in is on 1.5.6, on a computer whose RUDOLFDATA supports encryption (`USER.disk`).
  - **Sign-in:** the folder is opened with a 64-byte key made from the account's file key (HKDF-SHA512), with the
    Linux binds (`session=1`).
  - **Loading:** files up to 1.5 MB are loaded, as text or `data:` URLs, the way C: keeps them. Larger files become
    `rudolfdisk:` references into the account's folder, which open through `home/link` like large downloads.
  - **Saving:** `homeSave` compares with the snapshot and writes only what changed: new folders, new or changed
    files (with their base version), copies of references, and deletions (top-most only).
  - **Large downloads** in the account's Downloads move in with `home/adopt`.
  - **Conflicts:** a file another program changed meanwhile is kept, and the desktop's version is saved beside it
    as "… (desktop copy)".
  - **Other programs' changes:** the tree is read again every 15 s instead of using inotify. A `watch` can replace
    the poll later.
  - **Sign-out:** pending writes finish, then the folder is locked.
- **rudolfd:**
  - `home/link`: `/dlf/` links that hold `home:<acct>:<path>` and are walked again at every request, so a link a
    Linux app swaps in can't redirect them;
  - `home/adopt`: from Downloads, the account's own folder or the top only, copied across file systems.
- **Not yet for these accounts:** backup, an administrator opening their files, and the Trash on the disk
  (stage 4). The CHANGELOG says so.
- **Tests:** browser `home56_t.js` 20/20; host `homeapi_t.sh` 30/30. acct50, files, Downloads, stick and desktop
  tests are unchanged.

## Stage 3, as built

- **`hdMove` in home56.js.** An account from before 1.5.6 (it has a home record, or a file key) moves 1.5 s after
  signing in, in the background, while it keeps working:
  1. the folder is opened without the binds;
  2. every folder is made and every file written (`replace` for what an earlier attempt left); large downloads move
     in with `home/adopt`, and references already in the folder are kept;
  3. everything is read back, comparing each file's SHA-256 (and the size of moved downloads) and each folder;
  4. only then: `USER.disk`, `USER.oldHome` (the date), the snapshot from what was written (so changes made during
     the move are written next), and the binds.
  - Progress shows every 200 files or 5 s.
  - A failure leaves everything as it was, says so, writes an event, and is tried again at the next sign-in.
- **The old record** `home:<id>` is no longer written after the switch, so it stays as it was at the move. It is
  deleted at the first sign-in after 7 days.
- **Tests:** browser `home56m_t.js` 12/12:
  - a failed read-back changes nothing; the move at the next sign-in;
  - text, pictures and empty folders; a 3 MB download moved, not copied;
  - read-back before binds; the old record kept, then deleted after 7 days;
  - one write after the move, nothing moved again.

  `home56_t.js` (20/20) now turns `supported` off while its pre-1.5.6 account signs in.

## Stage 4, as built

- **The Trash:**
  - something put in the Trash that holds large files (references) is renamed into the account's
    `.Trash/<stamp>/` on the disk instead of deleted, and the Trash's entries are pointed there
    (`hdTrashRepoint`);
  - Restore renames it back (`move`), not a copy;
  - `.Trash/<stamp>` folders nothing points at any more are deleted after the Trash changes (`hdTrashSweep`);
  - small files stay in the Trash's own record, as before.
- **Backup:** `backupBlob` first writes the signed-in account's home as the record backups always had (encrypted
  with the file key). Large files are not in it, as large downloads never were.
- **A restored backup:**
  - rudolfd's `home/unlock` now answers "made" when it made the folder;
  - a disk account whose folder was just made, but which has a record (a backup restored here, or the account on
    another computer), is filled from the record by the stage 3 move.
- **Administrators:** `acOpenHome` opens a disk account's folder with its file key (`home/unlock` without binds,
  key from `hdKeyHex(f, o.id)`). `acSaveOthers` writes its changes with `hdSyncFor`, and `acClose` writes and locks
  it.
- **`hdSync` is generalised** to `hdSyncFor(acct, home, snapshot)`.
- **Not done in stage 4,** each needing the Vivobook:
  - Downloads stays out of the Linux binds, because Chromium's `.incoming` would move with it;
    **done after stage 4:** the downloads store (`.incoming`, `.quarantine`, `.accounts`, Linux apps' old files) has its
    own bind, `/run/rudolf/dl`, made by rudolfd before the account's binds; Chromium's policy points there; Downloads
    joins the binds; `lxSync` copies nothing; the copies at the store's top are removed or moved in (`hdDlTop`);
  - the Encrypt my data page's wording;
  - `watch` (the 15 s re-read stays).
- **Tests:**
  - browser `home56r_t.js` 11/11: large files through the Trash and Restore, emptying, small files, the backup
    record, a restore on a new folder, an administrator opening, changing and locking;
  - host `homeapi_t.sh` 30/30 ("made", then "unlocked");
  - `home56_t`, `home56m_t`, acct50, files, Downloads and desktop tests unchanged.

## 1.5.5's remaining items

1.5.5 was promoted on 2026-11-08 with plan items 1 (Downloads for each account) and the fixes found on the Vivobook.
Its items 2–7 (sorting, favourite files, storage, the Trash, search, batch rename and duplicates) move here, after
stage 4, and are built on the disk directly. Storage, search and duplicates get simpler that way, and the Trash
becomes `.Trash/` from the start.

## Added on 2026-11-09: installing apps from files, and the camera

Asked for after the Vivobook tests: installation wizards for apps downloaded as files (built in 1.5.6, before 1.5.5's
remaining items), and the Vivobook's camera.

### What is there today (checked in the code and on the Vivobook)

- **.deb:** opening one says that Rudolf OS does not install packages from a file yet, and offers the App Store
  (#128). Debian 13 runs beside Rudolf OS (rudolf-debian, its own namespaces); Settings › Apps › Linux sets it up,
  updates it and installs Wine.
- **.exe and .msi:** they open with Wine when Wine is installed (1.4.8, 1.4.9). There is no wizard: the installer
  just runs, and what it installs shows up in the Start menu by itself.
- **.AppImage, .bin, .apk, .ipa:** nothing opens them.
- **Debian and your folders:** Debian's programs, Wine included, did not see the account's folders as 1.5.6 has them,
  and with "Encrypt my data" they could not open Downloads at all (fixed in #137, needed by every wizard below).
- **The camera:** the kernel finds the Vivobook's webcam (a USB webcam, UVC, 3277:0034), but its device was the
  system's only, so no app could open it (fixed in #136).

### The wizard (all file types)

Opening an installable file in File Explorer or from Oracle's Downloads opens **Install <name>**, one window with the
same steps for every type:

1. **What it is:** the app's name, version, publisher and size where the file says (a .deb's control fields, an
   .exe's version information), and where it will be installed.
2. **What it needs:** Linux (Debian) set up, Wine, or Android, each with the size of its download. The wizard sets
   them up itself, behind an administrator's password, with a progress bar (as Install Wine does today).
3. **Installing:** a progress bar from the job's own output, and the full log behind "Details".
4. **Done:** "Open <app>", "Put it on the desktop", and where it is in the Start menu. A failure says why, in plain
   words, from the last lines of the log.

Installing needs an administrator (or their password), as installing from the App Store does. Every installed app
can be removed again from Settings › Apps.

### Each file type

- **.deb (Debian packages):** `rudolf-debian deb FILE` copies the file into Debian and installs it with
  `apt-get install`, which fetches what it depends on. The app's launcher appears in the Start menu, with its own icon.
  Uninstall: `apt-get remove`, from Settings › Apps › Linux.
- **.exe and .msi (Windows):** the wizard runs the installer with Wine (`wine` or `wine msiexec /i`). It tells which
  programs appeared in the Start menu afterwards, and offers to put them on the desktop. A program that is not an
  installer, but the app itself, is offered "Keep it in Programs and add it to the Start menu" instead.
- **.AppImage:** kept in a Programs folder of the account (`~/Applications`), made runnable, and given a Start-menu
  launcher with the icon from inside it (`--appimage-extract` of the .desktop file and the icon only). It runs in
  Debian, which has the libraries AppImages expect; one that needs FUSE runs with `--appimage-extract-and-run`.
- **.bin (Linux installers):** these are programs that install something else (drivers, games, tools); what they do
  differs for each one. The wizard shows what the file is (`file`), warns that it runs with the account's rights in
  Debian, and runs it in a Terminal window, where its own questions can be answered. Not as administrator.
- **.apk (Android apps):** through Waydroid, which runs Android in a container.
  - the kernel needs Android's binder (`CONFIG_ANDROID_BINDER_IPC`, `CONFIG_ANDROID_BINDERFS`) and a few container
    options: a kernel change, checked in kconfig.sh like the others;
  - Waydroid itself comes from Debian; the Android system image (about 800 MB, LineageOS) is downloaded the first time,
    behind a confirmation;
  - Android apps show in a window of their own (Waydroid runs a small Wayland display inside X);
  - Google Play's apps that need Google services may not work; the wizard says so before the download.
- **.ipa (iPhone and iPad apps):** these cannot be installed. iPhone apps are built for Apple's own system and are
  encrypted for Apple's devices; nothing outside Apple's devices can run them. The wizard says so, and offers to look
  for the same app in the App Store (Linux) or as a web app.

### The camera

- **The device (#136):** cameras (`video*`, `media*`, `v4l-subdev*`) belong to the video group the desktop is in.
- **Checked on the Vivobook after the fix:** the Camera app (photos and video), Oracle's websites asking for the
  camera (the shell's permission prompt), Discord's video calls (Flatpak, which has the device), and Settings ›
  Privacy › Camera off and on.
- **Debian's programs (Wine included):** `/dev/video*` placed in Debian like `/dev/dri` and `/dev/snd`.
- **Cameras of other laptops:** Intel's MIPI cameras (IPU6, 1.5.4's kernel options) need libcamera rather than UVC;
  only checked on hardware that has one.
- **Hardware check (hwcheck30):** the camera step opens it for a moment and says which camera it found.

## 1. Where the files go

```
/persist/accounts/<account id>/          one per account, encrypted with its file key (fscrypt v2, AES-256-XTS)
    Desktop/ Documents/ Pictures/ Music/ Videos/ Downloads/
    .Trash/                          the account's Trash (files, and where each came from)
    .rudolf/                         the desktop's notes on files (comments, favourites, sort orders)
/persist/accounts/guest/                 tmpfs, emptied at sign-out (as the guest's home is today)
```

- C:\USERS\<name> is that folder. The other parts of C: (Programs, the system folders) stay in `fs:system`: they
  are small and the same for everyone.
- Names keep their case on the disk. The desktop's names are upper case today. The view shows a file as it is
  named, and compares names without case, so `Report.pdf` and `REPORT.PDF` are one file (a second one is
  `Report (2).pdf`).
- **While an account is signed in,** `/home/rudolf/Documents`, `Pictures`, `Downloads`, `Music`, `Videos` and
  `Games` are bind mounts of that account's folders. Every Linux app (Flatpak, Debian, Wine, Steam) sees that
  account's files. Nothing is mounted while nobody is signed in.

## 2. rudolfd: files in the homes

A new file API (`home/*`), only inside `/persist/accounts/<the signed-in account>` (and an administrator's opened
account):
- **`list`, `tree`, `stat`**: names, folder or file, size, modified time, a version number (mtime and size)
  for "changed elsewhere".
- **`read`** (with ranges, for viewers and the antivirus) and **`link`** (as `/dlf/`, for `<video>` and PDFs).
- **`write`:** whole file or appended chunks, then an atomic rename. It refuses a write whose base version is out of
  date (the file changed meanwhile), and the desktop asks which copy to keep.
- **`mkdir`, `rename`, `move`, `copy`** (a job when large), **`delete`** (into `.Trash/`), **`purge`**.
- **`watch`:** changes made by Linux apps (inotify), passed on as events, so File Explorer and open apps update.
- **Path checks:** absolute paths built by rudolfd from the account and a relative path. No `..`, and no links
  leading out.
- **Keys:** `POST home/unlock` (the account's file key, sent once over the local API after the desktop unwraps it)
  adds the fscrypt key and mounts the binds. `home/lock` removes the key and unmounts them, at sign-out and at
  switch-user.

## 3. The desktop: C: as a view

The apps keep using the in-memory tree (`node()`, `d[name]=value`, `fsSave()`), so they need no changes:
- **At sign-in**, the home's tree is read from the disk (`tree`): names, sizes, times. A file's content is read
  when it is first opened. Small text files (under 256 KB) are read at once in the background, so search and
  previews do not wait.
- **A file not read yet** is a reference, as downloads are today, generalised from `rudolfdisk:` to every file.
  Opening it reads it. Viewers that already read references in pieces (video, PDF, images) keep doing so.
- **Saving:**
  - `fsFlush()` no longer writes the home as one record. It compares the tree with the last version written
    (strings are compared by identity first, which costs nothing when unchanged) and writes only files that
    changed, made or went away, through `home/write`, `mkdir`, `rename` and `delete`;
  - a crash or power cut loses at most the last 300 ms, as today, and never a whole home;
  - renames and moves inside C: are found from File Explorer's own operations (cut, paste, rename, drag), which call
    rudolfd directly rather than going through the comparison.
- **Changes from outside** (a Linux app saved a file): `watch` events update the tree. If the file is open and
  edited in the desktop, the app is told and asks: keep mine, or reload.
- **The rest of C:** (`fs:system`), the browser-only version and live sessions without RUDOLFDATA keep the current
  storage, unchanged.

## 4. Moving the homes (once, at an account's first sign-in on 1.5.6)

1. The desktop decrypts the home record as today.
2. It writes every file to `/persist/accounts/<id>/`, which is already encrypted. Large downloads (the
   `.accounts/<id>/` files) are moved, not copied.
3. It reads the tree back and compares names, sizes and SHA-256s with the record.
4. Only then does it switch to the disk. It **keeps the old record** for the update fallback: if 1.5.6 does not
   start twice in a row, the computer goes back to 1.5.5, which still has the old record. Files changed after the
   move would then show their old version in 1.5.5. The notification says so, and the record is deleted once 1.5.6
   has been the working system for 7 days.

Other cases:
- A home that cannot be moved (a full disk, a damaged record) stays as it is, with a notification and a health-log
  line. Nothing is lost, and the move is tried again at the next sign-in.
- C:\SHARED DOWNLOADS (BEFORE 1.5.5) moves to `/persist/accounts/shared-downloads/`, readable by every account, if it
  is still there.
- Progress is shown, because a large home takes a while: "Moving your files to the disk… 1,234 of 5,678 files".

## 5. What changes with it

- **Downloads:**
  - Oracle's checked downloads move straight into the account's `Downloads/`;
  - the 1.5 MB split, the `rudolfdisk:` entries, the reconcile and the adoption (#115, #117) go away;
  - `.incoming` and `.quarantine` stay.
- **"Encrypt my data":**
  - Debian and Flatpak stay in `/persist/vault`;
  - Downloads leaves it, because it is now inside each account's own encrypted folder;
  - the setting says that homes are always encrypted, and that "Encrypt my data" covers Linux.
- **The Trash** is `.Trash/` in the account's folder: restoring is a rename, and its size comes from the disk.
  1.5.5's Trash items move with the home.
- **Storage, search and duplicates** (1.5.5 items 4, 6, 7) use rudolfd on the disk. Search can read every file.
- **Backup:**
  - "Back up everything" makes one archive of every account's folder, read through the keys of the accounts that
    are signed in. Accounts that are not signed in are copied with their fscrypt-encrypted names and contents, which
    restore only onto this computer's RUDOLFDATA. The page says so, and offers a backup per account, decrypted, for
    another computer;
  - restoring older backups (records) goes through the move in item 4.
- **Administrators opening an account's files** (acct50) adds that account's key while the administrator has the
  folder open, and removes it when they close it.
- **The guest:** a tmpfs home, as now.
- **Settings › Storage** shows homes as real sizes on the disk.

## 6. Risks, and how each is handled

- **Losing files in the move:**
  - it copies, verifies with SHA-256, and keeps the old record for 7 days;
  - QEMU tests move homes of 10 and 5,000 files, and one with a damaged record.
- **An app that writes a file the desktop has open:** versions on every write, and the app asks when they differ.
- **Speed:** opening a file is a local HTTP call to rudolfd (milliseconds). Listing a folder of 10,000 files is
  measured in the QEMU test and must stay under 1 s.
- **fscrypt on older drives:** the encrypt feature is added to RUDOLFDATA as "Encrypt my data" does today
  (`tune2fs -O encrypt`). A drive that cannot take it (not ext4) keeps today's storage, and says so.
- **Two people's files in Linux apps:** the bind mounts change at sign-in. An app left running from the previous
  account is stopped at switch-user, as Linux apps are today when the account changes.
- **Going back to 1.5.5:** covered for 7 days by the kept record (item 4). After that, a fallback to 1.5.5 would see
  an empty home. The update keeps 1.5.6 from falling back past 7 days, and says so.

## 7. Tests

- **Host:** rudolfd's `home/*` against a temporary ext4 image with the encrypt feature (root):
  - paths that try to leave the folder;
  - versions and conflicts;
  - unlock and lock (the files unreadable after lock);
  - bind mounts.
- **Browser** (stand-in rudolfd with a disk in memory):
  - the view;
  - only changed files written;
  - references read on open;
  - outside changes;
  - the move with verification and a damaged record.
- **QEMU `t156`:**
  - a 1.5.5 home with documents, pictures and a large download, updated to 1.5.6: files on the disk, encrypted
    after sign-out, readable after sign-in;
  - a Linux app (busybox in Debian) sees and edits a document, and the desktop shows the edit;
  - the fallback to 1.5.5 within the 7 days shows the old home;
  - a backup and its restore;
  - two accounts, an administrator opening the other's files.
- **The Vivobook:** your real home moved; Oracle, Steam, Emu, Notepad, WordPro, Paint and the Linux apps on the
  same files.

## Stages (one PR each)

1. rudolfd `home/*` with fscrypt per account and the bind mounts (host tests).
2. The desktop's view of C: on the disk, for new accounts only (browser tests).
3. The move for existing homes, with verification and the kept record (browser tests, QEMU t156).
4. Downloads, the Trash, Linux apps' folders, "Encrypt my data", backup and restore on the new storage.
5. 1.5.5's items 2–7 (docs/plan-1.5.5.md), on the disk: sorting and favourite files; storage and the Trash; search;
   batch rename and duplicates (one PR each).
5a. (added 2026-11-09) Installation wizards, one PR each: Debian sees the account's folders (#137) and the wizard
   with .deb; .exe and .msi; .AppImage and .bin; .apk (kernel binder, Waydroid); .ipa's explanation goes with the
   first. The camera: the device (#136), then Debian's /dev/video and the hardware check.
6. QEMU t150, t150f, t152 and t156 on the final beta; the Vivobook; then promote when you say; the disc image.

## Size

rudolfd (about 600 lines) and the desktop (a new storage layer of about 500 lines, and less in dl27.js and
linux26.js). No new packages: fscrypt's ioctls are already used by rudolfd's vault (1.5.0).
