#!/bin/sh
# librudolf-ui and a program that uses it, built as the release builds them (1.6.2, docs/plan-1.6.2.md Part 39):
#   sh build.sh WORKDIR OUTPUT SOURCE [GCC FLAGS...]
# WORKDIR gets mkui.py's header (the page's styles and language tables, rudolf-native's own code) and librudolf-ui's object;
# SOURCE is ui-sample (the test's sample window) or an app in iso/sys/apps (taskmgr: /usr/bin/rudolf-taskmgr): NAME.c with its parts,
# NAME_PART.c (settings: settings_about.c...), and, when there is one, NAME's own generated header made by mkNAME.py in WORKDIR
# (WORKDIR/NAME-gen.h: what the app reads from the page at build time, as mkui.py's). Extra flags (the
# release's hardening, systools/hardflags.sh) are given to every gcc. Fails on any warning with RUI_WERROR=1.
# A native program's FILE moves in 4 KB pages -- every load segment starts on one -- so a few dozen bytes of code can
# show as 4 or 8 KB in its size: the program's .text (readelf -S) is the measure of the code, not the file's.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); W=$1; OUT=$2; SRC=$3; shift 3
mkdir -p "$W"
python3 "$HERE/mkui.py" "$W/rui-gen.h"
case "$SRC" in ui-sample) S=$HERE/ui-sample.c;; *) S=$HERE/../apps/$SRC.c;; esac
[ -f "$S" ] || { echo "build.sh: no $S"; exit 1; }
case "$SRC" in ui-sample) ;; *) for p in "$HERE/../apps/${SRC}"_*.c; do [ -f "$p" ] && S="$S $p"; done
	G=$HERE/../apps/mk$SRC.py; [ -f "$G" ] && { python3 "$G" "$W/$SRC-gen.h" || exit 1; } ;; esac
WE=""; [ "${RUI_WERROR:-0}" = 1 ] && WE=-Werror
CF="-O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-misleading-indentation -Wno-deprecated-declarations $WE"
# 1.6.3 (docs/plan-1.6.3.md Part 3): Settings, all of its 22 pages in one program, is built for size. At -O2 it was 1126 KB,
# over a native app's 1 MB budget (systools/ci_release.sh, docs/plan-1.6.2.md Part 39); -Os with the sections nothing uses
# makes it 991 KB with every page, label and string kept. Measured on the same tree built as the release builds it (hardened,
# stripped, 2026-11-28): -O2 1126 KB, -O2 with the sections 1110, -Os 1003, -Os with the sections 991. Every caller of this
# script -- the release, the tests, native-apps-check.yml -- builds it the same way this way.
[ "$SRC" = settings ] && CF="$CF -Os -ffunction-sections -fdata-sections -Wl,--gc-sections"
gcc $CF "$@" -I"$W" -I"$HERE" -c -o "$W/rudolf-ui.o" "$HERE/rudolf-ui.c" $(pkg-config --cflags gtk+-3.0 fontconfig)
gcc $CF "$@" -I"$W" -I"$HERE" -o "$OUT" $S "$W/rudolf-ui.o" $(pkg-config --cflags --libs gtk+-3.0 fontconfig) -lX11 -lm
