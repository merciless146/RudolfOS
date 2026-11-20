#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 21): a release's own Chromium (its chromium.sfs) made to start on a CI runner or another
# Linux host. Since 1.6.0 Rudolf's Chromium carries its own glibc (opt/chromium/glibc) and names it by absolute path: its
# program interpreter is /opt/chromium/glibc/ld-linux-x86-64.so.2 and its RPATH /opt/chromium/glibc:$ORIGIN, as it is mounted
# in Rudolf OS. Unpacked anywhere else, the kernel cannot find that loader ("cannot execute: required file not found",
# exit 127: GitHub's runners, 2026-11-20). So the unpacked tree is linked at /opt/chromium (root), the loader and the
# other libraries are checked (ldd: the rest come from the host, as from base.sfs in Rudolf OS), and the path to start
# is printed: /opt/chromium/chrome.
#   sh betachrome.sh CHROMIUM.SFS DIR     (unpacks opt/chromium into DIR/c; refuses an /opt/chromium that is not its link)
set -e
SFS=$1; D=$(realpath -m "$2"); [ -f "$SFS" ] && [ -n "$2" ] || { echo "usage: betachrome.sh CHROMIUM.SFS DIR" >&2; exit 2; }
SU=; [ "$(id -u)" = 0 ] || SU=sudo
[ -x "$D/c/opt/chromium/chrome" ] || { $SU unsquashfs -q -n -d "$D/c" "$SFS" opt/chromium >/dev/null; $SU chown -R "$(id -u):$(id -g)" "$D/c"; }
INTERP=$(readelf -l "$D/c/opt/chromium/chrome" | sed -n 's/.*program interpreter: \(.*\)]/\1/p')
echo "the beta's chrome: interpreter $INTERP; $(readelf -d "$D/c/opt/chromium/chrome" | sed -n 's/.*Library rpath: \[\(.*\)\]/rpath \1/p')" >&2
if [ -L /opt/chromium ]; then [ "$(readlink /opt/chromium)" = "$D/c/opt/chromium" ] || $SU ln -sfn "$D/c/opt/chromium" /opt/chromium
elif [ -e /opt/chromium ]; then echo "betachrome: /opt/chromium is already there and not this script's link; not touched" >&2; exit 1
else $SU ln -s "$D/c/opt/chromium" /opt/chromium; fi
[ -e "$INTERP" ] || { echo "betachrome: the loader $INTERP is still missing" >&2; exit 1; }
MISS=$(ldd /opt/chromium/chrome 2>&1 | grep 'not found' || true)
[ -z "$MISS" ] || { echo "betachrome: libraries missing on this host:" >&2; echo "$MISS" >&2; exit 1; }
/opt/chromium/chrome --version >&2
echo /opt/chromium/chrome
