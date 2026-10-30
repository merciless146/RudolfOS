# Rudolf OS 1.4.4: cleanup (plan)

A pass over everything Rudolf OS builds and ships, looking for code and files that nothing uses (2026-10-30, on
1.4.2). The system programs are clean: no unused functions in `iso/sys/*.c` (gcc `-Wunused-function`, and every
non-static function is called), and every rudolfd API route has a caller (`lsync` is called by the script rudolfd
injects into the page). What is left is small. 1.4.4 removes it and adds a build check so it does not come back.
Nothing a person uses changes.

## Items

| # | What | Where | Why it can go |
|---|---|---|---|
| 1 | `sysuMB` | `desktop/sysupd25.js` | defined, never called |
| 2 | `tmE` | `desktop/tm26.js` | defined, never called (Task Manager fills its rows with `textContent`) |
| 3 | `frames/_metadata/generated_indexed_rulesets/_ruleset1` | `iso/sys/root/usr/lib/rudolf/frames/` | a cache Chromium writes when it loads the web frames extension, committed by accident with 1.1; the session copies the extension to `/run/rudolf/frames` at every start and Chromium writes its own there. `.gitignore` gets `_metadata/` |
| 4 | `/usr/bin/ostree` | base layer | the ostree command line; Flatpak uses the ostree library (`libostree-1.so.1`), not this program, and nothing in Rudolf OS runs it |
| 5 | `/usr/bin/dbus-send`, `/usr/bin/dbus-run-session` | base layer | nothing in Rudolf OS runs them (the session starts `dbus-daemon` itself; Flatpak talks to D-Bus through its library) |
| 6 | `/usr/bin/pacat` and its links `paplay`, `parecord` | base layer | PulseAudio's command-line player and recorder; Rudolf OS uses the sound server through `pactl` and the library, and apps through the library, never these |

Kept, and why (checked the same way, but used):
- `gpg`, `gpgv`, `gpg-agent`, `gpgconf`: Flatpak runs them to check the signatures of app repositories.
- `fusermount3`: Flatpak's document portal mounts with it.
- The nine desktop functions that look unused in `desktop/*.js` (`homeRekey`, `kvDel`, `pwFind`, `calcModes`,
  `wpPrint`, `passkeyCheck`, `lxCheckLines`, `wpExtend`, `wpWmCss`): `build19.py` patches calls to them into the base
  page (`base_v24.html`), so they are only called from there.

## How

**Desktop (items 1-2).** Delete the two definitions; rebuild `iso/www/index.html`.

**The cache (item 3).** `git rm -r iso/sys/root/usr/lib/rudolf/frames/_metadata`; `iso/.gitignore` gets
`sys/root/usr/lib/rudolf/frames/_metadata/`.

**The base layer (items 3-6).** The release build copies `sys/root` over the previous base layer, so files that
leave the repository, and programs the full build used to copy, stay in the base layer until removed explicitly.
`ci_release.sh` gets a list of paths to remove from the base layer (`systools/base-remove.txt`):
```
usr/lib/rudolf/frames/_metadata
usr/bin/ostree
usr/bin/dbus-send
usr/bin/dbus-run-session
usr/bin/pacat
usr/bin/paplay
usr/bin/parecord
```
Each is removed only if present; the list is part of `BSRC`, so a change to it rebuilds the base layer.
`mkrootfs.sh` (full builds) stops copying the same programs: `dbus-send` and `dbus-run-session` from its D-Bus list,
`pacat` and its links from the sound server's, and `ostree` from Flatpak's.

**A check that keeps it so** (`systools/deadcheck.py`, run by `ci_release.sh` before anything is built, and as a host
test):
- every top-level function and arrow function in `desktop/*.js` is named somewhere else in `desktop/` (its own
  module, another one, or `build19.py`'s patches);
- every C program in `sys/` compiles with `-Wunused-function -Werror=unused-function`;
- every file in `sys/root` is tracked by git (no build output);
- every program in `systools/base-remove.txt` is absent from `mkrootfs.sh`'s copy lists.
It fails the release build with the name of what is unused, like the duplicate-function check `build19.py` has
had since 1.4.0.

## Tests

| Test | What it shows |
|---|---|
| host: `deadcheck.py` | passes on 1.4.4; fails on 1.4.3 (names `sysuMB`, `tmE` and `_ruleset1`) |
| host: devlink / layout / bootfiles | unchanged (41/41, 7/7, 12/12) |
| `dl140_t.js` (Settings page) | unchanged |
| new `t144_t.py` | on the 1.4.4 test disc: the six programs and the cache are gone from the system; the web frames extension loads (Chromium writes its own `_metadata` under `/run/rudolf/frames`); `flatpak --version`, `flatpak remote-ls` from a local test repository with a signed tiny app, `flatpak install` and `flatpak run` of it; the sound server starts and `pactl info` answers; the D-Bus session starts |
| t125 (Wi-Fi, apps, Luau) | unchanged: Flatpak apps install and run |
| t141, t142, t143, t140, t131, t130, t128b, sb_t, v120 | unchanged |
| The Vivobook (after release) | Task Manager › System: the services as before; an installed Flatpak app (Oolite, after 1.4.3) still starts; sound plays; Settings › Update and recovery works |

## Not removed now (fallback paths kept on purpose)

Older paths that only run in rare situations. Retiring them changes what happens there, so they stay until the
conditions below hold; each is noted in the code where it starts.
- **The separate browser window for sign-in sites and Oracle's site frames** (`desktop/web25.js`,
  `desktop/signin26.js`, the web frames extension): since 1.2, Oracle uses real tabs; these run only when the desktop
  starts without its shell (the session's fallback). Retire when the fallback desktop is retired.
- **Start-up code for boot files from before 1.3** (`initramfs/init`: the first-found data partition, the
  slot-complete scan, 1.4.2's build check): it is what lets old boot files and old sticks hand over to newer systems.
  Retire when no supported install can still have boot files older than 1.3 (Get new boot files replaces them).
- **The software display (Xvfb)**: the display when there is no usable graphics driver; not a leftover.

## Release

Branch `rudolf-1.4.4`, after 1.4.3 is out. Version 1.4.4; the changes list says "Removed unused programs and code;
nothing you use changes." A beta, the checks above, promote; no disc needed.
