# Tests for Rudolf OS 1.2's shell (Oracle's real tabs)

Two ways to run them:

## On this machine, without a VM (fast)

`host.sh` runs the desktop inside the shell the way the session does: Xvfb (`:77`), a stand-in for rudolfd
(`rudolfd_mock.py`), test websites (`site.py`, every `*.test` name points here), and Rudolf's own Chromium started
through `rudolf-launch`. The shell is signed with a test key.

```sh
export WORK=/some/scratch CHROME=/path/to/opt/chromium/chrome KEY=$WORK/test.key DISPLAY=:77
python3 ../../systools/mkiwa.py key $KEY        # once
gcc -O2 -o $WORK/xin xin.c /usr/lib/x86_64-linux-gnu/libXtst.so.6 -lX11
./host.sh start fresh && node wait.js 90         # fresh: a new profile (the installed shell changes only with its version)
node tab_t.js $WORK/xin $WORK/shots              # Oracle's real tabs, end to end (42 checks)
node tm_t.js                                     # Task Manager and system reports (18 checks)
node pl_t3.js $WORK/xin                          # pointer capture for games, with real X input
node mv_t.js STEP                                # the move of the databases into the shell and back (phase 2)
node sr_t.js                                     # the system-reports workflow script, against a stand-in for GitHub
node dl_t.js                                     # 1.2.1 downloads of any size (35 checks; MEDIA=folder with clip.webm and big.pdf,
                                                 # PDFLIB=a disc's www/lib for PDF Viewer, both set before host.sh)
./host.sh stop
```

- `xin` sends real X input (XTest): clicks, relative moves, keys, raw key codes.
- `dev.js 'EXPR'` evaluates in the desktop page. The shell page itself cannot be evaluated over DevTools
  (Chromium does not attach to Isolated Web Apps); the desktop's `SHELL_LOG` holds the shell's notes.
- Keys: on Rudolf OS, `rudolf-display` turns Ctrl+W/T/N/Q into Ctrl+F20..F23 for the desktop's window. Here the
  input does not go through rudolf-display, so the tests send key codes 198..201 themselves.
- Ruffle: the disc loads Ruffle from the internet; `host.sh` puts a stand-in at `/lib/ruffle/ruffle.js` that swaps
  Flash content for a blue box.

## In QEMU, from a disc image

```sh
python3 v120_t.py NAME path/to/rudolf-os.iso     # diagnostic start; real keyboard, tablet and mouse from QEMU
```

It waits for the one-time move into the shell, then checks real tabs, cookies, downloads, Ctrl+T/W/Q through
rudolf-display, pointer capture with QEMU's relative mouse, fullscreen, Task Manager (processes, End process, System,
Hardware) and the system report (34 checks, with a large download kept on the disk).
