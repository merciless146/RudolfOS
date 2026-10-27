# Rudolf OS 1.4.0 plan: the development link

1.4.0 follows 1.3.1 (stable serial 2026102706). Its main item is the **development link**: a switch in Settings that
lets a Claude session (or any developer the owner chooses) run tests on a real computer running Rudolf OS, without
someone at the keyboard relaying commands and photos. The ASUS Vivobook Go (E1404FA) is the first computer it is for.

Why: the last four bugs (the Debian sandbox after kexec, the App Store crash, Sober's sign-in, 1.3.0 running an old
stick's system) only showed on the real laptop, and each took several rounds of photos to pin down. QEMU cannot show
the laptop's firmware, its NVMe drive, its webcam, its Wi-Fi chip or its boot entries. The next fix, making the
laptop start from its own drive (item 6), depends on exactly those.

| # | Item | Kind | Size | Stage |
|---|------|------|------|-------|
| 1 | `rudolf-devlink`: the program that fetches commands, runs them and sends the results | feature | medium | A |
| 2 | rudolfd: the switch, the token, the log, the banner state, staying on across a restart | feature | medium | A |
| 3 | Settings › Security › Development link, the banner, the approval prompt, the log viewer | feature | medium | A |
| 4 | Tests: host test with a stand-in for GitHub; QEMU test end to end | tests | medium | B |
| 5 | On the laptop: set up the repository and token, turn the link on, a first session | check | — | C |
| 6 | Use it: why the laptop's firmware does not start its internal drive, and the fix | bug | ? | D (1.4.x) |

Stages A and B ship as **1.4.0** (beta, checked, promoted as usual). Stage C needs the owner at the laptop once.
Stage D is the first real use; its fix ships as 1.4.1 or later.

---

## How it works

A Claude session runs in Anthropic's cloud. It cannot open a connection to the laptop, and from the cloud it can
reach only a few sites, GitHub among them. The laptop can reach GitHub too. So both sides use a **private GitHub
repository owned by the owner** (for example `merciless146/rudolfos-devlink`) as a mailbox:

1. The session writes a command to the repository: `cmd/000123.json` on the branch `link`.
2. The laptop asks GitHub every 5 seconds whether `cmd/` changed (a conditional request with ETag, which does not
   count against GitHub's rate limit when nothing changed), fetches new commands, and runs them in order.
3. The laptop writes the result to `res/000123.json` (and any files, such as a screenshot, under `res/000123/`).
4. The session reads the result.

Commands (`cmd/NNNNNN.json`):

| kind | what it does | limits |
|------|--------------|--------|
| `sh` | runs a shell script as root, with a time limit; stdout, stderr and the exit status come back | 10 min default, 60 min at most; 1 MB of output, the rest cut and said so |
| `screenshot` | a PNG of the real screen (rudolfd's `screenshot` route, as Task Manager uses) | one per command |
| `get` | sends a file from the laptop | 25 MB at most; nothing under `/home` unless the owner allowed it when switching the link on |
| `put` | brings a file to the laptop (a test kernel, a script) from the repository or a GitHub release of RudolfOS | 512 MB at most; written under `/persist/devlink/files` only |
| `restart` | restarts the computer; the link comes back by itself if it was set to stay on across a restart | — |
| `end` | turns the link off | — |

Results (`res/NNNNNN.json`): `{id, kind, started, ended, status, stdout, stderr, files[], approved_by}`.

## Who has access, and the limits

Anyone who can write to the repository can run any command as root on the laptop while the link is on. Testing boot
files, drives and the firmware needs root, so the limits are about **when** and **who**, not what:

- **Who:** the owner, and whoever the owner gives write access to the repository: a Claude session the owner attaches
  it to, a collaborator the owner adds. GitHub stores the commands and results, as it stores the code.
- **The token:** a GitHub fine-grained personal access token that can read and write the contents of that one
  repository and nothing else. The owner makes it on github.com and types it into Settings on the laptop. It is kept
  root-only on RUDOLFDATA (`/persist/devlink/token`, mode 0600), never shown again, never in the log or the system
  report. Revoking it on GitHub ends all access, even if the laptop was lost. Claude never types or handles it.
- **Off by default.** Switching it on asks for an administrator's password. It turns itself off after the time
  chosen when switching it on (1, 2, 4 or 8 hours) and does not stay on across a restart unless that box was ticked
  (needed for boot tests; it then still ends at the chosen time).
- **Always visible.** While it is on, a red banner across the top of the screen says "Development link on: commands
  from GitHub can control this computer", with the time left and **End now**. It is drawn above every window,
  including fullscreen games and the login screen. The boot menu and start-up messages say it too after a restart.
- **Ask first (on by default).** Each command waits on screen with its text until the owner presses **Allow** or
  **Refuse**; refused commands come back as refused. The owner can untick it when switching the link on, for
  unattended runs.
- **A log** on the laptop of every command, its result and who allowed it (Settings › Security › Development link ›
  Log), kept for 30 days.
- **Test computers only.** Settings says so when it is switched on: a computer with personal files should not use it.

Known limit, said plainly: rudolfd trusts the desktop's administrator prompt, as it does for ending a process or
installing Rudolf OS; it cannot check the password itself, because accounts live in the desktop. Any program already
running as the desktop user could switch the link on, but only with a valid token, which only the owner has, and the
banner would show it.

## Stage A: implementation

**1. `iso/sys/rudolf-devlink.c`** (root, started and stopped by rudolfd; built in `ci_release.sh` like the other
system programs, so the every-program check covers it):
- GitHub's REST API with libcurl (as `rudolf-update` already loads it): `GET /repos/{repo}/contents/cmd?ref=link`
  with `If-None-Match`, then `GET` each new file; `PUT /repos/{repo}/contents/res/{id}.json` for results (files over
  1 MB through the Git data API: blobs, a tree, a commit, then the branch).
- Runs commands one at a time, in id order, each in its own process group with the time limit. The last finished id
  is kept in `/persist/devlink/state`, so after a restart nothing runs twice and nothing is skipped.
- Asks rudolfd for approval (rudolfd shows the prompt) and waits; a command not answered in 10 minutes is refused.
- Backs off on network errors (5 s up to 2 min) and says so in the banner ("waiting for the network").
- A test setting (`RUDOLF_DEVLINK_API`, test discs only, like `FILE_BASE`) points it at a stand-in server.

**2. rudolfd** (`/devlink` routes):
- `GET devlink`: on/off, repository, ends at, stays across a restart, ask first, last command, network state.
- `POST devlink/on` (repo, token, hours, restart, ask): checks the token with GitHub (`GET /repos/{repo}`, must be
  private and writable), stores it, starts `rudolf-devlink`.
- `POST devlink/off`: stops it, keeps the token (so turning it on again is one step); `POST devlink/forget` deletes
  the token.
- `GET devlink/pending`, `POST devlink/answer` for the approval prompt; `GET devlink/log`.
- At start-up (`rudolf-init`): if the link was on, set to stay across a restart and not past its end time, start it
  again; otherwise mark it off.
- The system report gets "Development link: off" or "on until HH:MM (repository)", never the token.

**3. The desktop** (`desktop/devlink40.js`): Settings › Security › Development link (repository, token, how long,
stay across a restart, ask first, the warning, Switch on/End now/Forget the token, the log); the banner (from the
display layer, so no window can cover it); the approval prompt (the command's full text, Allow/Refuse).

## Stage B: tests

- **Host test** `iso/qt/host/devlink_t.sh`: `rudolf-devlink` against a small stand-in for GitHub's contents API
  (Python): commands in order, one at a time; time limit; output cut at 1 MB; results written back; ETag polling;
  restart in the middle of a command (no double run); network errors and back-off; a token never in any output.
- **QEMU test** `iso/qt/sb/t140_t.py` (test disc, the stand-in server on the host): switching on asks for the
  administrator password; a wrong or read-only token is refused; the banner shows over a fullscreen window; ask
  first (allow, refuse, no answer); `sh`, `screenshot`, `get`, `put`, `restart` (comes back on when ticked, stays
  off when not); ends at its time; End now; the log; the system report; the token never in the report, the log or
  the page; the link off after an update unless it was on.
- The usual regression set (t125, t128b, t130, t131, sb_t, v120).

## Stage C: on the laptop (the owner, once)

1. Update the laptop to 1.4.0.
2. On github.com: make a private repository (`rudolfos-devlink`, empty), then a fine-grained personal access token:
   only that repository, permission "Contents: Read and write", expiry 30 days.
3. On the laptop: Settings › Security › Development link: the repository, the token, 2 hours, stay across a restart,
   ask first. Switch on.
4. In the Claude session: attach the repository. The session sends `uname -a`, a screenshot and the firmware's boot
   entries; the owner allows each. The results go into this plan's as-built notes.

## Stage D: the first use (1.4.x)

The laptop's firmware lists only "Ubuntu" and "Windows Boot Manager" and does not start Rudolf OS from its own NVMe
drive, so the laptop starts through an old USB stick, and Secure Boot cannot work. With the link: read the boot
entries (`Boot####`, `BootOrder`) and the drive's partition table and EFI partition. Then find out why the
firmware skips the drive (likely the hybrid ISO layout: an MBR whose first partition starts at sector 0). Then fix
Install here and Get new boot files: a proper boot entry for the drive, written by Rudolf OS as "Restart into
Windows" already writes `BootNext`. Test it there, with restarts, without the stick.

## Release steps
1. Stage A and B on the branch `rudolf-1.4.0`: implement, host and QEMU tests, local release build, test disc.
2. Pull request with the results; after the merge, a beta, checked; the owner promotes 1.4.0.
3. Stage C with the owner; stage D on a new branch, released as 1.4.1.
