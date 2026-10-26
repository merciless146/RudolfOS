# Rudolf OS 1.3.0 plan

1.3.0 follows 1.2.9 (stable serial 2026102619). It collects what came up while the ASUS Vivobook Go (E1404FA) was
brought onto 1.2.8 and 1.2.9: one request (games), and the lessons of four bugs that only the real laptop showed.
Every item below comes from that work; nothing is speculative.

| # | Item | Kind | Size | Stage |
|---|------|------|------|-------|
| 1 | App Store › Games: every game on Flathub, with search and genres | feature | small | A (done: PR #43) |
| 2 | Command Prompt and Lua show the installed version (they said 1.1 and 1.0) | bug | tiny | A (done: PR #43) |
| 3 | A USB stick started on a computer with Rudolf OS installed uses the internal drive's RUDOLFDATA | bug | medium | B |
| 4 | A crash is reported as a crash, not as "Is this computer connected to the internet?" | bug | small | B |
| 5 | Release updates rebuild every system program (rpk, rudolf-ai and rudolf-print still are not) | build | small | B |
| 6 | Settings › Apps › Linux › Change the Linux password | feature | small | B |
| 7 | QEMU tests that start the way an updated computer starts (by kexec) for Linux apps and the terminal | tests | small | B |
| 8 | Confirm on the laptop: the webcam, new boot files, Secure Boot | check | — | C |

All stages ship together as **1.3.0**; stable stays on 1.2.9 until then (scheduled builds of main make 1.3.0 betas,
which are not promoted). Then `disc-1.3.0` from `disc-1.2.8`, with its boot image.

---

## Stage A (done: PR #43)

**1. Games.** App Store › Games gets "All games on Flathub": every game in the downloaded app list, a search box,
the genres Flathub uses (only those that have games), "Verified only", installed games first, a game's page going
back to Games. A game is an app with the category "Game" or any game genre (some list only "ActionGame"); Linux apps'
Games category uses the same test. Test: `shell2/gmall_t.js` 10/10.

**2. Version strings.** Command Prompt's banner and `VER`, and Lua's `sys.version()`, use `RVER`. Test:
`shell2/cmd_t.js` 3/3.

## Stage B

**3. The stick and the internal drive's RUDOLFDATA.**
*Now:* the start-up (`initramfs/init`) takes the first partition labelled RUDOLFDATA anywhere
(`findfs LABEL=RUDOLFDATA`). On a computer where Rudolf OS is installed, a Rudolf OS stick can therefore start with
the internal drive's files, and even hand over to the internal drive's installed update (its `os/system/state`).
Found while writing `t128b_t.py`, which has to plug the second drive in after the start for this reason. On the
laptop it matters for "start from the stick, then Install here or Replace the boot files only".
*Change:* RUDOLFDATA is taken from the disk the system started from (the media's own disk, partition 3) when there
is one. A disc (`/dev/sr*`) has none, so it keeps today's search, but then only removable drives qualify. A stick
without its own RUDOLFDATA runs live, as a stick written by Rufus does today, and Settings › Storage offers
"Keep files on this drive" (keepfiles.sh) as now. The choice is written to `/etc/rudolf/persist` as before.
*Tests:* QEMU: a stick and an internal drive, both with RUDOLFDATA and different files and slots: the stick starts
with its own files and its own system; the internal drive alone with its own; a disc with a RUDOLFDATA stick still
keeps files there; a disc with only an internal installed drive runs live.
*Risk:* this is the start-up of every computer. The rule for the usual case (one drive) does not change.

**4. Crashes reported as crashes.**
*Now:* when a program behind a job crashes, the page guesses from the job's end: the App Store said "Is this
computer connected to the internet?" when `rudolf-apps` crashed, and the cause was only in Event Viewer's kernel log.
*Change:* rudolfd notes a job that ends by a signal (exit 128+n) and reads the kernel's `segfault` lines for Rudolf
OS programs since start-up. The job's message says "<program> stopped unexpectedly (a bug in Rudolf OS)" with
Report a problem…; the App Store, Linux apps and the stick/install pages show that instead of their guesses; the
system report gets a "Crashes" section (program, time, library and offset; no file names or addresses).
*Tests:* host: rudolfd with a job that crashes (a test program); page: the App Store shows the crash text, not the
internet question; QEMU: `kill -SEGV` on a job, then the system report.

**5. Every system program in release updates.**
*Now:* `ci_release.sh` rebuilds only the programs it lists. `rudolf-debian` (until 1.2.7), `rudolf-portal` and
`rudolf-apps` (until 1.2.9) were missing, so their fixes would have reached no one by update. `rpk`, `rudolf-ai` and
`rudolf-print` still are.
*Change:* build those three too, and a check in `ci_release.sh` that stops the build when a `sys/*.c` is built by
neither it nor the start-up image (`initramfs.list`).
*Test:* a local release build; the check made to fail with a new empty `sys/x.c`.

**6. Change the Linux password.**
*Now:* Debian's password is set once, in Set up Debian…; forgetting it means removing Debian.
*Change:* Settings › Apps › Linux › "Change the Linux password…" (administrator; new password twice), done by
`rudolf-debian` with `chpasswd` inside the sandbox, as at setup.
*Test:* QEMU with a Debian root built for tests: the new password works with `su`, the old one does not.

**7. Tests that start like an updated computer.**
*Now:* most QEMU tests start the test disc's own kernel. Every real computer with an update installed starts its
update by kexec instead, which is what hid the `/proc/cmdline` mount (fixed in 1.2.8) from every test.
*Change:* `sbvm` gets a mode that installs the test release in slot a of the data disk, so the disc's kernel hands
over by kexec; `t127d_t.py` and a new Debian sandbox start test run in that mode.

## Stage C

**8. On the laptop** (needs the person and the hardware; not testable in QEMU):
- the hardware check's camera step with the built-in webcam (QEMU has no USB camera);
- Settings › Security › Secure Boot › Get new boot files with `rudolf-os-1.2.8-boot.iso`, then Prepare Secure Boot,
  MokManager, and Secure Boot on in the firmware;
- the result goes into this plan's as-built notes.

## Release steps
1. Stage B on branch `rudolf-1.3.0-b`: implement, host and page tests, local release build, test disc, QEMU tests.
2. Pull request; after merge, a beta; the stage tests and the regression suites on that serial.
3. Promote 1.3.0 to stable; `disc-1.3.0` from `disc-1.2.8`, checked (disc list, boot image, signatures).

## Risks
- **Item 3 changes which data partition a start uses.** Someone who started a stick on purpose to work with the
  internal drive's files gets the stick's own files (or a live session) instead. Settings › Storage shows which
  drive's files are in use, and the CHANGELOG says so.
- **Item 4 reads the kernel log**, which can hold other programs' lines: only Rudolf OS's own programs are reported.
