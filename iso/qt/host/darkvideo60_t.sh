#!/bin/sh
# 1.6.0: Oracle's dark pages over a playing video: no layer of the player draws the video as a negative (YouTube's
# progress bar on the Vivobook, 2026-11-15). The shell's own script, in Playwright's Chromium, with the display flags
# the session gives the browser (Settings › Display: software and games, then full on the graphics card as near as a
# machine without one comes: SwiftShader with GPU compositing). The video is made here with ffmpeg.
#   sh darkvideo60_t.sh      (needs node with playwright, ffmpeg; uses port 8601)
#   CHROME=.../opt/chromium/chrome: a release's Chromium (its chromium.sfs unpacked); SHELLJS: another shell.js
HERE=$(cd "$(dirname "$0")" && pwd); W=$(mktemp -d); F=0
trap 'rm -rf "$W"' EXIT
command -v ffmpeg >/dev/null || { echo "SKIP (no ffmpeg)"; exit 0; }
node -e "require('playwright')" 2>/dev/null || { echo "SKIP (no playwright)"; exit 0; }
ffmpeg -loglevel error -y -f lavfi -i "testsrc2=s=640x360:r=25:d=6" -c:v libvpx-vp9 -b:v 300k -pix_fmt yuv420p $W/v.webm || { echo "FAIL ffmpeg"; exit 1; }
python3 -c "import zlib,struct
def c(t,d): return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
open('$W/t.png','wb').write(b'\x89PNG\r\n\x1a\n'+c(b'IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))+c(b'IDAT',zlib.compress(b'\0\xc8\xc8\xc8'))+c(b'IEND',b''))"
SESS=$HERE/../../sys/root/usr/lib/rudolf/session
SOFT=$(sed -n 's/^\tGLFLAGS="\(.*disable-gpu-compositing\)"$/\1/p' $SESS)
FULL=$(sed -n 's/.*SETTING" = full ] && GLFLAGS="\(.*\)"$/\1/p' $SESS | sed 's/--use-angle=gl-egl/--use-angle=swiftshader/')
[ -n "$SOFT" ] && [ -n "$FULL" ] || { echo "FAIL the session's display flags were not found"; exit 1; }
# the two copies of the style (the shell's for real tabs, the desktop's for Oracle's own pages) are the same
a=$(sed -n "s/^const DARK_CSS = '\(.*\)';$/\1/p" ${SHELLJS:-$HERE/../../shell/shell.js}); b=$(sed -n "s/^const WEB_DARK_CSS='\(.*\)';$/\1/p" $HERE/../../../desktop/web46.js)
if [ -n "$a" ] && [ "$a" = "$b" ]; then echo "PASS the shell's and the desktop's dark-pages styles are the same"; else echo "FAIL the shell's and the desktop's dark-pages styles differ"; F=1; fi
for m in soft full; do
	eval "fl=\$$(echo $m | tr a-z A-Z)"; echo "== display $m: $fl"
	timeout 300 node $HERE/darkvideo60.js $W 8601 $fl || F=1
done
[ $F = 0 ] && echo "all passed"; [ $F = 0 ]
