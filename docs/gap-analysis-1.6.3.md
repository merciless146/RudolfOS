# What Rudolf OS still lacks beside other operating systems (1.6.3)

**What this is.** A gap analysis of three things a person expects from a desktop operating system: the hardware it
works on, the software it can add (drivers, system parts, other systems' programs), and the files it can manage. It
ranks each gap by how much it blocks Rudolf OS from being as capable as a mainstream one, and names the first pull
request each gap would need — the smallest change that moves it, not the whole gap. Written from the repository's own
record: `CHANGELOG.md` (what shipped, 1.4.9 to 1.6.3) and the plans (`docs/plan-1.6.0.md` to `docs/plan-2.0.md`).

**How to read a rank.** 1 is a hole that can be used against a person today; 2 to 4 are the things that most often
make someone keep another operating system on a second machine; 5 and on are capability gaps with a known route.

| # | Gap | Why it matters | The first pull request it needs |
|---|---|---|---|
| 1 | One account can reach another account's files through the desktop's own API | A signed-in account is not separated from the others in `dl/*` and `home/*`; the vault (fscrypt) is the only wall, and Downloads is outside it | The guard in the Archive Manager's PRs (a path may only touch the folder the extraction made) and, after it, the session check on every `acct=` request |
| 2 | No way for anyone but this project to add a driver or a system part | A printer, GPU or device vendor cannot ship support here; it takes a project-built signed extension or a release | Signed system extensions installed from a file (Settings › Update and recovery › System extensions), with the same checks the published ones get |
| 3 | New hardware waits for a kernel release | 6.18 long-term is steady but lags new chipsets; the fix (7.x) is 2.0's and lives on `kryptonite` | The kernel-track switch: a "next" channel built from `main` with the newer kernel, X11 unchanged, 1.6.x untouched |
| 4 | Device firmware cannot be seen or updated | Most firmware is a manual, out-of-band job; a mainstream OS lists versions and updates them | `fwupd` read-only: the devices, their firmware versions and what can be updated, in Settings › Devices and the system report; no flashing |
| 5 | X11: no HDR, no variable refresh, no per-display fractional scaling, no touch/gestures for a phone | These are Wayland's, and every phone shell is Wayland; 2.0 is where it lands | W0 (B): the Wayland test stick, measured against X11 as W0 (A) already measured a nested session |
| 6 | No disk or partition management; no installation beside Windows | Resizing, choosing free space and a start-up menu for both is everyday work elsewhere | Part 6, step 1: the installer's free-space choice in space made in Windows, with its own check |
| 7 | The Archive Manager is only half rebuilt (items 2 to 8 of Part 10) | No destination choice or merging, silent renaming, no per-file extraction from a big ZIP, no install-from-archive, no TAR.GZ/7z, no damaged-ZIP warning | Part 10, item 2: `Extract to…` and "into this folder", with the replace/keep/cancel question and one folder for a second extraction |
| 8 | The big apps are still web pages on the native desktop | File Explorer and the Archive Manager cost about 100 MB a window instead of 15 to 40 MB | 2.0 wave 2: File Explorer as a native program, step 1 (the window, the folders, the drives) |
| 9 | No Android apps, and Windows only as far as Wine goes | A phone's apps do not run here; a program Wine cannot run is out of reach | Waydroid measured on the Vivobook with Lite, before it is offered (the plan's own gate) |

## 1. Another account's files through the desktop's API

The vault is good: each account's files are real files on the disk, encrypted with its own fscrypt key, and unreadable
while it is signed out. What is not good is what the API trusts:

- `GET dl/home?acct=`, `GET dl/read?acct=`, `GET dl/zip?acct=` and `POST dl/unzip?acct=` take an account name and only
  check its shape (`dl_acct_ok`), never that it is the account signed in. Downloads is kept unencrypted at
  `/home/rudolf/Downloads/.accounts/<id>` and owned by the desktop's user, so one account's page can list, read,
  extract from and write into another's (and Guest's) store.
- `GET home/*?acct=` takes an account name too, and only the vault stands in the way: any account whose folder is open
  is readable, whoever is signed in.
- The Archive Manager's new path mode (Part 10, item 1) added `dl/read?path=`, `POST dl/file?path=&to=quar|del` and
  `dl/zip?path=`/`dl/unzip?path=` for "a ZIP anywhere the account may read" — every real path under `/home`, `/tmp`,
  `/srv` and the mounted drives. `/home` holds each account's Linux home (`/home/<account>`), the downloads store above,
  and the shared quarantine; so the same requests read, quarantine (take) and delete another account's files, without
  an administrator's permission.

**The first PR:** the path guard, in the same PRs that added the path mode: `dl/read?path=` and `dl/file?path=` may
only touch a file inside the folder the extraction just made beside the ZIP (recorded when `dl/unzipped` says where it
is), a path with a hidden folder in it is refused, and a ZIP is only taken from the signed-in account's own places.
**Then** the session check on `acct=` in `dl/*` and `home/*`: the account signed in, or the guest and shared stores,
nothing else. **The test:** a host case for each refusal, and a page case that the antivirus still reads, quarantines
and deletes inside its own extraction.

## 2. A way to add a driver or a system part without a new release

This is the biggest structural difference from a mainstream OS, and it is deliberate: the base is one signed,
read-only image, updates are whole and reversible, and a system part arrives as a **signed system extension** (a
read-only layer over the unchanged base) rather than an `apt`-style package that changes it. It is why Rudolf OS is
one image on every computer, why an update cannot fail half-way.

The cost is that only this project can publish one: vendor drivers, a printer filter, a tool one computer needs, all
wait on the release workflow and its key. The Library's chain of trust (a certified developer, a key, revocation) does
this for apps, never for the system.

**The first PR:** a signed extension **from a file** — Settings › Update and recovery › System extensions › Install
from a file…, with everything the published ones get (the release key only, this base and kernel, nothing of Rudolf
OS's replaced, left out if it would keep the system from starting, Start once without extensions). It gives a vendor a
path that does not need this repository, without letting an extension change the base.

## 3. New hardware waits for a kernel release

1.6.x is on the 6.18 long-term kernel with a checked list of 75 features the system relies on, and the driver coverage
built up over 1.5.3 to 1.6.2 (Realtek/Intel/MediaTek Bluetooth, the RT2800 and AR9271 families of USB Wi-Fi, ELAN and
Synaptics touchpads, NVIDIA's driver as an extension) is real but bounded by that kernel. A chipset released after it
has no driver until the kernel moves, which is 2.0's newest-kernel track on `kryptonite`.

**The first PR:** the kernel-track switch, so a newer kernel can be built from `main` into a channel of its own
("next"), with 1.6.x's stable channel and recovery untouched. The kernel work itself already exists on `kryptonite`;
what is missing is the switch and the manifests that keep two editions apart.

## 4. Device firmware

Nothing here lists or updates firmware. Mainstream systems do (fwupd on Linux, OEM tools elsewhere), and the machines
this is tested on — the Vivobook, the Asus desktop — have firmware that occasionally needs it.

**The first PR:** read-only. `fwupd`'s device and version list in Settings › Devices, and in the system report, with
"what can be updated" named and nothing written. Flashing is a later PR, with the owner's hardware in its checklist.

## 5. Wayland, and what it brings

X11 has served the native desktop well, and 1.6.1's several monitors, 1.6.2's title bar and the whole native shell are
built on it. But HDR, variable refresh, per-display fractional scaling, tear-free drawing, and the touch and gesture
handling a phone needs are Wayland's. W0 (A) measured a nested Weston session on the Vivobook (2026-11-22) and every
app ran, Chromium and WebKitGTK and Wine included; the fair numbers — memory, battery, start-up — need a real session
from a stick.

**The first PR:** W0 (B), the test stick described in `docs/plan-2.0.md`: a WendigOS image with Cage and Xwayland, the
Chromium desktop on `--ozone-platform=wayland`, the measurements on both sessions, and the go or no-go for W1.

## 6. Disks, partitions, and installing beside Windows

Rudolf OS installs to a disk or a stick and can replace an installation, and 1.6.2 keeps an old computer's files
before an installation erases a disk. What is missing is everything a person does with a disk before that: choosing
free space made in Windows, shrinking and growing, a start-up menu for two systems. Part 6 of 1.6.3 is the plan and
nothing of it is built.

**The first PR:** Part 6, step 1 — the installer offers the free space Windows left (found, checked, never guessed),
writes Rudolf OS there and adds its start-up entry beside Windows', with the check that Windows still starts.

## 7. The Archive Manager, items 2 to 8

Item 1 (one path for every archive, on the disk) is the PRs of 2026-11-28. What is left is everything a person notices
next: no `Extract to…` and no merging (a second extraction makes "Game (2)"), names silently upper-cased and cut at 60
characters, no per-file extraction or preview from a 200 MB ZIP, no `Install…` for an archive holding a `.RAPP` or a
`setup.exe`, no TAR.GZ or 7z, no password for an encrypted ZIP, a damaged ZIP answered with one flat sentence after it
has started writing, and a window with no sort, filter or sizes.

**The first PR:** item 2 — the destination choice and the question when a name is there (Replace, Keep both, Cancel),
so a second extraction into one folder updates instead of duplicating.

## 8. The big apps are still web pages

The native desktop's File Explorer and Archive Manager are pages of the whole desktop in a WebKit window, about
100 MB each; a native program of the same kind is 15 to 40 MB. 2.0's wave 2 has both (and wave 3 has the App Store and
Packages). Until then the memory cost of the two most-used tools is the web page's.

**The first PR:** File Explorer as a native program, step 1: the window, the folders, the drives, and the checklist
made from the web version, as the native Settings and Office apps were built.

## 9. Other systems' programs

Linux apps run well (a Debian 13 sandbox, Flatpak from Flathub through the App Store, `.deb`/AppImage handling, a
keyring, portals, printing, cameras and controllers), and Windows programs run as far as Wine and Proton take them.
Android apps do not run at all: an `.apk` is inspected and the person is offered the phone, the web version, or the
App Store.

**The first PR:** Waydroid measured on the Vivobook with Lite, which the plan requires before it is offered anywhere
(`docs/plan-1.6.0.md`, Waydroid), and the list of what works and what refuses.

## Not gaps: choices

Worth saying, so they are not "fixed" by mistake: no conventional package manager (2); no vendor kernel, ever, on a
phone (2.0's phone build); no sharing of one account's files with another without that account's password (1); Chromium
and Lite kept beside the native desktop (2.0 decides their future); and the App Store's Linux apps sandboxed rather
than installed into the system (2).
