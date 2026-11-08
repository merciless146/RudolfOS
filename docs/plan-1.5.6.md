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

## 1. Where the files go

```
/persist/home/<account id>/          one per account, encrypted with its file key (fscrypt v2, AES-256-XTS)
    Desktop/ Documents/ Pictures/ Music/ Videos/ Downloads/
    .Trash/                          the account's Trash (files, and where each came from)
    .rudolf/                         the desktop's notes on files (comments, favourites, sort orders)
/persist/home/guest/                 tmpfs, emptied at sign-out (as the guest's home is today)
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

A new file API (`home/*`), only inside `/persist/home/<the signed-in account>` (and an administrator's opened
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
2. It writes every file to `/persist/home/<id>/`, which is already encrypted. Large downloads (the
   `.accounts/<id>/` files) are moved, not copied.
3. It reads the tree back and compares names, sizes and SHA-256s with the record.
4. Only then does it switch to the disk. It **keeps the old record** for the update fallback: if 1.5.6 does not
   start twice in a row, the computer goes back to 1.5.5, which still has the old record. Files changed after the
   move would then show their old version in 1.5.5. The notification says so, and the record is deleted once 1.5.6
   has been the working system for 7 days.

Other cases:
- A home that cannot be moved (a full disk, a damaged record) stays as it is, with a notification and a health-log
  line. Nothing is lost, and the move is tried again at the next sign-in.
- C:\SHARED DOWNLOADS (BEFORE 1.5.5) moves to `/persist/home/shared-downloads/`, readable by every account, if it
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
5. Storage, search and duplicates switched to the disk (1.5.5 items 4, 6, 7, if 1.5.5 shipped them on C:).
6. QEMU t150, t150f, t152 and t156 on the final beta; the Vivobook; then promote when you say; the disc image.

## Size

rudolfd (about 600 lines) and the desktop (a new storage layer of about 500 lines, and less in dl27.js and
linux26.js). No new packages: fscrypt's ioctls are already used by rudolfd's vault (1.5.0).
