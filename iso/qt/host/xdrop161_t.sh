#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 13): files dropped from outside Rudolf OS onto the native desktop. rudolf-native on a virtual
# screen (Xvfb) with the stand-in rudolfd (fixtures/native_rd.py), whose desktop/dropped is rudolfd's own (fixtures/dropapi.c,
# rudolfd-drop.inc: the files read by the account's Linux user, uid 1001 here); a stand-in Linux app (fixtures/xdndsrc.c) drags
# files as a file manager does (XDND 5): onto the empty desktop, a folder's icon, the Trash, a taskbar button, as text; another
# account's file, a link out of the home, a folder, a web address. What Native keeps is compared with what Standard's page keeps
# for the same drops (xdrop161_std.js, the page in Playwright's Chromium, served by same_rd.py). Needs root (the account's user,
# a private mount namespace), Xvfb, xdotool, node with Playwright, gcc and WebKitGTK's headers.   sh xdrop161_t.sh [RUDOLF-NATIVE]
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$XD_INNER" ] && exec env XD_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/xdrop161.XXXXXX); chmod 755 $T; PORT=${XDROP_PORT:-8612}; SPORT=${XDROP_SPORT:-8613}; DSP=${XDROP_DSP:-:87}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run; mkdir -p /run/rudolf   # (rudolfd's session file: this namespace's own)
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
fi
gcc -O1 -w -o $T/dropapi $HERE/fixtures/dropapi.c -lz 2>$T/cc && gcc -O2 -Wall -o $T/xdndsrc $HERE/fixtures/xdndsrc.c -lX11 2>>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
D=$T/d; mkdir -p $D $D/idbm; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","b2":"#000080","tb":"bottom","accent":"#2563eb","h24":true,"df":"ymd"},"pins":["notepad","calc","settings"],"notes":2,"wallpaper":false}' > $D/settings.json
# the accounts' folders: Ann's (uid 1001, her Linux user's home bound at /home/acct1001) and Bob's (1002)
R=$T/accounts; A=$R/uAnnA1b2; B=$R/uBobC3d4; F=$T/files
mkdir -p $A/Documents/sub $A/Pictures $B/Documents $T/home/acct1001 $T/media $T/runuser $T/flat $F
echo 'hello from a Linux app XD1' > $F/hello.txt; echo 'second file XD2' > $F/two.txt
python3 -c "
import struct,zlib,sys
raw=b'\0'+b'\xff\0\0'*2
ch=lambda t,d: struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
open(sys.argv[1],'wb').write(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',2,1,8,2,0,0,0))+ch(b'IDAT',zlib.compress(raw))+ch(b'IEND',b''))" $F/dot.png
cp $F/hello.txt $A/Documents/; cp $F/dot.png $A/Pictures/; cp $F/two.txt $A/Documents/; echo "Bob's secret XD9" > $B/Documents/secret.txt
ln -s $B/Documents/secret.txt $A/Documents/away.txt
chown -R 1001:1001 $A; chmod 700 $A; chown -R 1002:1002 $B; chmod 700 $B; mount --bind $A $T/home/acct1001
printf 'uAnnA1b2 1001\nuBobC3d4 1002\n' > $T/map
printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' $T/dropapi $R $T/map $T/home $T/runuser $T/media $T/flat uAnnA1b2 > $D/drop-fix
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
trap 'kill $NP $RDP $XV $SRP 2>/dev/null; pkill -f "$T/" 2>/dev/null; umount -l $T/home/acct1001 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
mkfifo $T/ev; exec 7<>$T/ev
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$T/ev exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
N=0; FL=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; FL=$((FL+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
click() { xdotool mousemove $1 $2 click 1; }
di() { grep "^dicon .*|$1$" $D/state | head -1; }
dc() { di "$1" | awk '{print $4+42, $5+24}'; }
cmi() { grep "^cmitem .*|$1$" $D/state | head -1 | awk '{print $2, $3}'; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
xn() { st xdrop | cut -d' ' -f1; }
# the Desktop folder as the page keeps it (read in an app window's page, as xdrop161_std.js reads Standard's): each name, a folder's
# names, a file kept as text (its words) or as a data: URL (its type)
LISTJS="JSON.stringify(Object.fromEntries(Object.keys(deskFiles()).sort().map(n=>{const v=deskFiles()[n];return[n,isDir(v)?{dir:Object.keys(v)}:typeof v=='string'?(v.startsWith('data:')?'data:'+v.slice(5,v.indexOf(',')):'text:'+v):typeof v]})))"
drag() { x=$1; y=$2; shift 2; n0=$(xn); o=$($T/xdndsrc $x $y "$@" 2>&1); case "$o" in *"status 1"*) w8 "[ \"\$(xn)\" != \"$n0\" ]" 160;; esac; timeout 1.5 tail -f /dev/null; echo "$o"; }
U=file://$T/home/acct1001
# ---- signed in, a folder on the desktop (New folder, as a person makes it)
w8 '[ "$(st signin)" = 1 ]' 120; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ] && [ -n "$(di Trash)" ]' 160; timeout 1 tail -f /dev/null
xdotool mousemove 900 500 click 3; w8 '[ -n "$(cmi "New folder")" ]' 20; click $(cmi "New folder"); w8 '[ "$(st dialog | cut -d" " -f1)" = rename ]' 80
xdotool key Return; w8 '[ -n "$(di "NEW FOLDER")" ]' 80
w8 '[ "$(st dialog | cut -d" " -f1)" = - ]' 40
ok "0. signed in, the Trash and a new folder on the desktop" "$([ -n "$(di "NEW FOLDER")" ] && [ -n "$(di Trash)" ] && echo 1)" "$(grep '^dicon\|^user\|^dialog' $D/state; tail -5 $T/native.log)"
# ---- 1. the empty desktop: the file copied into the Desktop folder
o=$(drag 900 450 "$U/Documents/hello.txt"); w8 '[ -n "$(di HELLO.TXT)" ]' 60
ok "1. a file dropped on the empty desktop: the source told copy (XdndStatus), the data asked, finished (XdndFinished, copy)" "$(echo "$o" | grep -q '^status 1 XdndActionCopy' && echo "$o" | grep -q '^data text/uri-list' && echo "$o" | grep -q '^finished 1 XdndActionCopy' && echo 1)" "$o $(st xdrop)"
ok "...the file in the Desktop folder (its icon), once" "$(di HELLO.TXT | grep -q '|file|HELLO.TXT$' && [ "$(grep -c '^dicon .*|HELLO' $D/state)" = 1 ] && echo 1)" "$(grep '^dicon' $D/state)"
ok "...a copy: the Linux app's file is still there, unchanged" "$(cmp -s $A/Documents/hello.txt $F/hello.txt && echo 1)"
# ---- 2. a folder's icon: as on Standard, into the Desktop folder (the folder's own drop takes only the page's drags); the name as the page makes it
o=$(drag $(dc "NEW FOLDER") "$U/Documents/hello.txt"); w8 '[ -n "$(di "HELLO - COPY.TXT")" ]' 60
ok "2. onto a folder's icon: into the Desktop folder, the name clashing as the page names it (HELLO - COPY.TXT)" "$(echo "$o" | grep -q '^finished 1' && [ -n "$(di "HELLO - COPY.TXT")" ] && echo 1)" "$o $(grep '^dicon' $D/state)"
# ---- 3. the Trash: as on Standard, into the Desktop folder; Debian's path for the account's folder (/home/rudolf)
o=$(drag $(dc Trash) "file:///home/rudolf/Pictures/dot.png"); w8 '[ -n "$(di DOT.PNG)" ]' 60
ok "3. onto the Trash, a picture from Debian's file manager (/home/rudolf): into the Desktop folder" "$(echo "$o" | grep -q '^finished 1' && [ -n "$(di DOT.PNG)" ] && echo 1)" "$o $(grep '^dicon' $D/state)"
# ---- 4. two files in one drag (a Flatpak app's path: its user's home)
o=$(drag 1000 300 "$U/Documents/two.txt" "file://$A/Pictures/dot.png"); w8 '[ -n "$(di TWO.TXT)" ] && [ -n "$(di "DOT - COPY.PNG")" ]' 60
ok "4. two files in one drag: both, the second's name clashing" "$([ -n "$(di TWO.TXT)" ] && [ -n "$(di "DOT - COPY.PNG")" ] && echo 1)" "$o $(grep '^dicon' $D/state)"
# ---- refused
o=$(drag 900 450 "file://$B/Documents/secret.txt"); w8 'grep -q "^toast .*|Blocked$" $D/state' 20; TS=$(grep '^toast' $D/state)
ok "5. another account's file: not copied, not read (the drop finishes), the page's notice (Blocked)" "$([ -z "$(di SECRET.TXT)" ] && echo "$o" | grep -q '^finished' && echo "$TS" | grep -q '|Blocked$' && echo 1)" "$o $TS"
o=$(drag 900 450 "$U/Documents/away.txt"); timeout 2 tail -f /dev/null
ok "6. a link out of the home (to Bob's file): not copied" "$([ -z "$(di AWAY.TXT)" ] && echo 1)" "$o $(grep '^dicon' $D/state)"
o=$(drag 900 450 "$U/Documents/sub"); timeout 1 tail -f /dev/null
ok "7. a folder: not copied" "$([ -z "$(di SUB)" ] && echo 1)" "$o $(grep '^dicon' $D/state)"
o=$(drag 900 450 "https://example.com/page.html"); timeout 1 tail -f /dev/null
ok "8. a web address (no file): the drop refused when given (XdndFinished, not accepted)" "$(echo "$o" | grep -q '^finished 0' && [ "$(st xdrop | cut -d' ' -f2)" = nofiles ] && echo 1)" "$o $(st xdrop)"
o=$(drag 700 300 -text "just some words"); timeout 1 tail -f /dev/null
ok "9. dropped text (no file): refused while it is over the desktop, as Standard's page refuses it (XdndStatus: none)" "$(echo "$o" | grep -q '^status 0' && ! echo "$o" | grep -q '^finished' && echo 1)" "$o"
# ---- the taskbar: as on Standard, it takes no drop (and raises no window)
click $(hit pin notepad); w8 'grep -q "^client .* app=notepad " $D/state' 80; timeout 2 tail -f /dev/null
NID=$(grep '^client .* app=notepad' $D/state | head -1 | cut -d' ' -f2); P=$(grep "^hit win " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}')
click $P; w8 "grep \"^client $NID \" $D/state | grep -q 'min=1'" 40   # (minimised: its taskbar button, as on Standard)
o=$($T/xdndsrc $P -hover 1500 "$U/Documents/hello.txt" 2>&1); timeout 0.5 tail -f /dev/null
ok "10. a file held over a taskbar button for 1.5 s: no drop there, the window stays minimised (Standard's taskbar takes none)" "$(echo "$o" | grep -q '^target none\|^status 0' && grep "^client $NID " $D/state | grep -q 'min=1' && echo 1)" "$o / $P / $(grep "^client $NID \|^hit win" $D/state)"
# ---- what Native's page keeps, read in Notepad's page
echo "notepad $LISTJS" > $T/ev; w8 '[ -s $T/ev.out ]' 40; NL=$(sed -n 's/^notepad //p' $T/ev.out | head -1)
ok "11. Native: the files' words kept as text, the pictures as data: URLs, the folder empty, nothing of Bob's" "$(printf '%s' "$NL" | node -e "const s=JSON.parse(require('fs').readFileSync(0));console.log(s['HELLO.TXT']=='text:hello from a Linux app XD1\\n'&&s['DOT.PNG']=='data:image/png;base64'&&s['TWO.TXT']=='text:second file XD2\\n'&&s['NEW FOLDER'].dir.length==0&&!JSON.stringify(s).includes('XD9')?1:0)")" "$NL"
# ---- Standard's page with the same drops (xdrop161_std.js), compared
SD=$T/sd; mkdir -p $SD; cp $D/ls.json $D/keys.json $SD/
python3 $HERE/../native/same_rd.py $SPORT $SD > $T/srd.log 2>&1 & SRP=$!
(cd $HERE/../native && timeout 400 node xdrop161_std.js $SPORT "$PW" $F $T/std.json) > $T/std.log 2>&1
SL=$(node -e "console.log(JSON.stringify(require('$T/std.json').afterTwo))" 2>/dev/null)
ok "12. Standard's page, the same drops (the empty desktop, a folder's icon, the Trash, two files): the same Desktop folder, name by name and file by file" "$([ -n "$SL" ] && [ "$SL" = "$NL" ] && echo 1)" "Standard: $SL / Native: $NL / $(cat $T/std.log)"
j() { node -e "const s=require('$T/std.json');console.log($1)" 2>/dev/null; }
ok "...dropped text refused, and the taskbar takes no drop and raises no window, as on Native" "$([ "$(j 's.text.took')" = false ] && [ "$(j 's.bar.took')" = false ] && [ "$(j 's.barWindowShown')" = false ] && echo 1)" "$(cat $T/std.json)"
echo "$((N-FL))/$N passed"; [ $FL = 0 ]
