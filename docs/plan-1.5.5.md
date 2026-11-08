# Rudolf OS 1.5.5: file management

1.5.4 is stable (1.5.4.261108, serial 2026110815, with disc-1.5.4). 1.5.5 has already started with fixes found on
the Vivobook:
- the Trash's tooltip (#114);
- adding and removing app shortcuts on the desktop, with Desktop icons… (#114);
- every file in the Downloads folder showing in File Explorer (#115).

This plan covers the rest of 1.5.5, File Explorer and storage, in the order of the stages at the end.

What is there today, checked in the code before writing this:
- **Your files on drive C:** live in the desktop's own storage (IndexedDB, mirrored by rudolfd). Each account's home
  folder (C:\USERS\<name>) is encrypted with that account's file key (1.5.0).
- **Shared with Linux apps:** Documents, Pictures, Downloads and Games are also folders on the disk
  (/home/rudolf/…), kept in step with C:. One Linux user holds them for every account.
- **Downloads** (since 1.2.7) are in **C:\DOWNLOADS, one folder for all accounts**, outside the encrypted homes.
  Files over 1.5 MB stay on the disk in /home/rudolf/Downloads, and C:\DOWNLOADS holds an entry pointing at each.
  With "Encrypt my data", that disk folder is inside the encrypted folder. It is still shared by every account.
- **Sorting:** the details view sorts by name, date, type or size when a column heading is clicked; folders come
  first. The icons and thumbnails views have no way to sort, and the order resets to name in every new window or
  tab.
- **Search:** the box searches the current folder, or "Everywhere" below it (file names, and the text inside text
  files). It stops at 300 results. Drives and the disk's Downloads are not searched.
- **Favourites:** folders only.
- **Settings › Storage and backup:** the desktop storage used, your home folder, Empty the Trash and the backup.
  Nothing about the disk itself: Linux apps, Steam games, Debian, Downloads on the disk.
- **The Trash:** each item records when it was deleted (`t`). It has no size shown and no automatic emptying.

## 1. Downloads for each account

**The problem:**
- Every account sees and can delete every other account's downloads. A child's account sees an administrator's.
- Small downloads (1.5 MB or less) are stored in C:\DOWNLOADS, outside the encrypted home folders.

**The change:**
- **Each account gets its own Downloads again:** C:\USERS\<name>\DOWNLOADS, encrypted with the account's file key
  like the rest of the home folder.
  - File Explorer's Downloads and Oracle's "Downloaded" notification point there.
  - The guest's Downloads is not kept after sign-out, like the guest's other files (its home is never saved).
- **On the disk, a folder per account:** /home/rudolf/Downloads/<account id>/ holds the large files. Entries point
  into it, and rudolfd's `dl/link` and `dl/read` stay inside Downloads as today. The `dl/home` adoption (#115)
  looks only in the signed-in account's folder.
- **Who gets a download:**
  - A download goes to the account that was signed in when it finished.
  - If nobody is signed in, it waits in .incoming and goes to whoever signs in next. Today it went into C: before
    C:'s files were loaded, which is how entries were lost (#115).
- **Linux apps:** files they save in ~/Downloads arrive in the top of /home/rudolf/Downloads. The signed-in account
  takes them in (as #115 does now). Apps keep one Downloads folder, because they run as the one Linux user.
- **Moving what is there now** (once, at the first start of 1.5.5):
  - C:\DOWNLOADS goes to the account that is signed in first, if there is only one account besides the guest.
  - With several accounts, nobody can know whose files they are. C:\DOWNLOADS stays where it is, renamed
    C:\SHARED DOWNLOADS (BEFORE 1.5.5) and readable by all, with a notification to administrators to move the files
    into an account or delete them. Nothing is deleted by the move.
  - Large files on the disk move with their entries. A file that cannot be moved, for example on a full disk,
    keeps its old entry and is reported once.
- **Encryption:** /home/rudolf/Downloads is already inside the encrypted folder when it is on. The per-account
  folders do not change that. Within the device, only the desktop decides which account sees which folder. That is
  the same protection as the shared Documents folder, and it is said plainly in Settings › Security.
- **Tests:**
  - browser test with two accounts and the guest: a download goes only to the account signed in; the guest's
    downloads are gone after sign-out; the move from C:\DOWNLOADS with one account and with two;
  - host test of rudolfd's per-account folders: no path outside Downloads, no `..`;
  - QEMU t150: the move into the encrypted folder still covers Downloads.

**As built** (this PR):
- the folder name on the disk is `dlAcct()`, the account id made safe (lower-case letters, digits, `-`);
- rudolfd takes `acct=` on `dl/home`, `dl/read` and `dl/file`, and has `from=root&to=acct` (the move) and `dl/clear`;
  `dl_path_ok` is checked by the host test `dlpath_t.sh` (26 checks);
- the shared folder's large files go to `.accounts/shared/` rather than staying at the top of Downloads, so that
  the top of Downloads is only the Linux apps' folder (kept in step with the Linux owner's Downloads by linux26's
  sync, as before). #115's adoption looks only in the account's own folder: at the top of Downloads it would have
  shown the sync's files twice;
- the references of each account are tracked in their own list (`bd95dlrefs:<id>`), and the shared folder's in
  `bd95dlrefs`, so one account's clean-up never deletes another account's files;
- Install from and Make a USB stick also list disc images in the account's own folder (`install/images?acct=`);
- browser test `dl155_t.js` (17 checks).

## 2. Sorting in every view, remembered

- Sorting in the icons and thumbnails views: a Sort by menu in the toolbar (Name, Date modified, Type, Size;
  ascending or descending). The details view keeps its clickable headings, and both stay in step.
- Remembered per folder, for each account (`bd95xsort:<account>`: path → key and direction), and the default for
  new folders in View › Sort folders by. Downloads opens newest first by default, the way people look for what they
  just downloaded.
- "Folders first" can be switched off (View › Folders first).
- Tests: browser test of each view, the remembered order after closing and reopening File Explorer, and Downloads
  newest first.

## 3. Favourites for files

- **Add to favourites** for files too, from the file's menu. Today it is offered for folders only.
- **The sidebar:** Favourites lists favourite files below the favourite folders, each with its file icon. A click
  opens the file in its app, and the menu has Open file location and Remove from favourites.
- **Moves, renames and deletions:**
  - A favourite follows its file when the file is renamed or moved inside File Explorer.
  - A favourite whose file is gone is shown greyed, with "Remove from favourites".
- **Tests:** browser test of adding, opening, renaming the file, deleting it, and two accounts keeping separate
  favourites.

## 4. Storage: what fills the disk

Settings › Storage and backup gets a section "On this computer's disk" (rudolfd `GET storage`, measured by a job
because counting a large Steam library takes a while). It shows:
- the disk: size, used, free (RUDOLFDATA);
- what uses it, largest first:
  - your files on C: (the desktop's storage);
  - Downloads kept on the disk (yours, and the shared folder from item 1 if any);
  - the Trash;
  - Linux apps (Flatpak), and Steam's games within them;
  - Linux (Debian);
  - Emu's games;
  - system updates (the spare system slot);
  - the rest.
- each line has its own action:
  - Open (in File Explorer);
  - Empty the Trash;
  - Manage Linux apps (App Store › Installed);
  - Steam's own Storage page (opens Steam);
  - Remove Linux (Debian), behind an administrator's password and its own confirmation.

The section is measured when the page is opened, with "Measure again". The existing "almost full" notification
(`dlSpaceCheck`) opens this page.

Tests:
- host test of the measuring script on a stand-in tree;
- browser test of the page with a stand-in answer (bars, order, actions);
- QEMU: the page on the release, compared with `du`.

## 5. The Trash: its size, and emptying old items

- **The Trash window and its desktop tooltip show the total size:** "3 items in the Trash (24.6 MB)", counting large
  downloads at their size on the disk.
- **Settings › Storage › Empty items in the Trash older than:** Never (the default, as today), 1 day, 14 days,
  30 days or 60 days. At each start and once a day, items deleted longer ago than that are deleted for good. Each
  account has its own setting and its own Trash. A notification says how many items went, at most once a day.
- **"Delete forever" and Empty Trash** also delete large downloads from the disk. That happens today, through the
  references' reconcile; the test checks it.
- **Tests:** browser test with a clock moved forward: items older than the setting go, newer ones stay; the size
  shown; another account's Trash is untouched.

## 6. Search everywhere

- The search box's "Everywhere" also covers:
  - plugged-in drives: by file name, through rudolfd, at most 3 folder levels deep and 2,000 files, so a large
    drive does not stall it;
  - your Downloads kept on the disk (file names; their text only for files the antivirus has read in full).
- Filters next to the box:
  - Type: any, documents, pictures, music, videos, archives, programs;
  - Modified: any time, today, this week, this month, this year.
- Results show where each file is (C:, Downloads, the drive's name), and stream in as they are found.
  - The limit becomes 1,000, with "Show more".
  - Searching can be stopped.
- Tests:
  - browser test with a stand-in drive: the filters, the limit, stopping;
  - host test of rudolfd's search on a drive: depth and count limits, nothing outside /media.

## 7. Rename several files, and find duplicates

- **Rename…** with several files selected opens a batch rename window:
  - a pattern ("Holiday ###" gives Holiday 001, 002…), find and replace in the names, and upper/lower case;
  - a preview of every old and new name before anything changes;
  - names that would collide are shown in red and nothing is done until they are fixed;
  - one Undo puts all the names back.
- **Find duplicates** (in a folder's menu, and in Settings › Storage):
  - files of the same size are compared by SHA-256: on C: in the desktop, on the disk through rudolfd;
  - the groups are listed with their locations and dates, and the newest of each group is suggested to keep;
  - nothing is deleted without choosing. "Move the others to the Trash" goes through the Trash, so it can be
    undone.
- **Tests:** browser tests of the batch rename (pattern, collisions, Undo) and of duplicates on C: (groups, the
  Trash); a host test of rudolfd's hashing on a stand-in folder.

## Size

Desktop code only, apart from rudolfd's additions (`storage`, search on drives, per-account Downloads, hashing for
duplicates). No new firmware or packages. The update is the desktop layer and the base layer (rudolfd).

## Stages (one PR each)

1. **Downloads for each account** (item 1), first, because it moves files. A beta, tried on the Vivobook with two
   accounts before the rest.
2. **Sorting and favourites** (items 2 and 3).
3. **Storage and the Trash** (items 4 and 5).
4. **Search everywhere** (item 6).
5. **Batch rename and duplicates** (item 7).
6. QEMU t150, t150f and t152 on the final beta. Then the Vivobook, then promote when you say, then the disc image (one
   file, since #113).
