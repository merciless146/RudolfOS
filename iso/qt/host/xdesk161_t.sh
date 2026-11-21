#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 28; the owner, 2026-11-21): File Explorer's items dragged onto the native desktop and pasted there.
# rudolf-native on a virtual screen (Xvfb) with the stand-in rudolfd (fixtures/native_rd.py); an Explorer window (a page of its own,
# WebKit) and the desktop the shell draws, with real mouse input (xdotool): a file dragged from the window onto the empty desktop (moved,
# its icon in the cell under the pointer), Ctrl+drag (copied), onto a desktop folder's icon (into it), onto the Trash; the desktop's
# menu: Paste greyed until something is cut or copied, then Paste (the icon at the menu's cell), Ctrl+V after Cut (moved, the clipboard
# empty again), Paste into folder; Copy on a desktop file, Paste in File Explorer. Then Standard's page (explorer-desk161_std.js, in
# Playwright's Chromium) given the same steps: the same Desktop folder and the same places.
# Needs Xvfb, xdotool, node (Playwright for the comparison), python3, gcc and WebKitGTK's headers.   sh xdesk161_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/xdesk161.XXXXXX); PORT=${XDESK_PORT:-8642}; SPORT=${XDESK_SPORT:-8643}; DSP=${XDESK_DSP:-:89}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","h24":true},"pins":["notepad","explorer","settings"],"notes":2,"wallpaper":false}' > $D/settings.json
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
mkfifo $T/ev
trap '[ -n "$XDESK_KEEP" ] && cp $T/native.log $D/state $XDESK_KEEP/ 2>/dev/null; kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$T/ev exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
exec 7<>$T/ev
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
click() { xdotool mousemove $1 $2 click 1; }
ev() { : > $T/ev.out; printf '%s %s\n' "$1" "$2" >&7; w8 "grep -q '^$1 ' $T/ev.out 2>/dev/null" ${3:-40}; grep "^$1 " $T/ev.out | tail -1 | cut -d' ' -f2-; : > $T/ev.out; }
di() { grep "^dicon .*|$1$" $D/state | head -1; }
dcell() { di "$1" | awk '{print $2","$3}'; }
dc() { di "$1" | awk '{print $4+42, $5+30}'; }
cmi() { grep "^cmitem .*|$1$" $D/state | head -1; }
cmixy() { cmi "$1" | awk '{print $2, $3}'; }
cmidis() { cmi "$1" | awk '{print $4}'; }
cl() { grep "^client .* app=$1 " $D/state | head -1; }
fcn() { st deskxfer | cut -d' ' -f1; }
openw() { ev do-bg "oaPost({op:'open',app:'$1'});1" >/dev/null; w8 "cl $1 | grep -q ." 120 && w8 "[ \"\$(ev $1 '(()=>{try{return String(!!(OA.main&&OA.main.el&&document.title))}catch(e){return String(false)}})()')\" = true ]" 80; }
# the Desktop folder and Downloads as the pages keep them (read in the Explorer window's page)
LS="JSON.stringify({desk:Object.fromEntries(Object.keys(deskFiles()).sort().map(n=>[n,isDir(deskFiles()[n])?Object.keys(deskFiles()[n]).sort():deskFiles()[n]])),dl:Object.keys(node(downloadsPath())).sort(),docs:Object.keys(node(homePath('DOCUMENTS'))).sort(),tr:TR.map(t=>t.n)})"
# a row of the Explorer window on the screen: its page's rect moved by the window's place
row() { R=$(ev explorer "(()=>{const e=OA.main.body.querySelector('[data-k=\"'+downloadsPath().join('/')+'/$1\"]');if(!e)return '';const r=e.getBoundingClientRect();return Math.round(r.left+Math.min(40,r.width/2))+' '+Math.round(r.top+r.height/2)})()"); [ -n "$R" ] && echo $(( OX + ${R% *} )) $(( OY + ${R#* } )); }
# (wait for what the pages keep to say so)
lsw() { w8 "ev explorer \"\$LS\" | grep -q '$1'" ${2:-40}; L=$(ev explorer "$LS"); }
# a drag with the mouse, as a person makes it (Ctrl held from the middle: copy)
drag() { xdotool mousemove $1 $2; timeout 0.3 tail -f /dev/null; xdotool mousedown 1; timeout 0.15 tail -f /dev/null; x0=$1; y0=$2; x1=$3; y1=$4
	for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14; do xdotool mousemove $(( x0 + (x1 - x0) * i / 14 )) $(( y0 + (y1 - y0) * i / 14 )); timeout 0.05 tail -f /dev/null; done
	[ "$5" = ctrl ] && { xdotool keydown ctrl; timeout 0.15 tail -f /dev/null; }
	xdotool mousemove $((x1 + 1)) $((y1 + 1)); timeout 0.3 tail -f /dev/null; xdotool mouseup 1; timeout 0.2 tail -f /dev/null; [ "$5" = ctrl ] && xdotool keyup ctrl; }
dn() { st deskxfer | cut -d' ' -f2; }
# ---- signed in; an Explorer window at the screen's right, showing Downloads
w8 '[ "$(st signin)" = 1 ]' 160; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ] && [ -n "$(di Trash)" ]' 200; timeout 2 tail -f /dev/null
# (the desktop folder and the Trash where the window leaves them uncovered: the shell's own places, as if moved there)
printf '%s' '{"file:BOX":[13,6],"Trash":[14,7]}' > $D/cells.json
openw explorer
ev explorer "(()=>{const D=node(downloadsPath());Object.keys(D).forEach(k=>delete D[k]);['A','B','C','D','E','NOTE'].forEach(n=>D[n+'.TXT']='file '+n);const K=deskFiles();Object.keys(K).forEach(k=>delete K[k]);K.BOX={};fsSave();EXPL.forEach(f=>f());OA.main.openPath(downloadsPath());return 1})()" >/dev/null
w8 '[ -n "$(di BOX)" ]' 80
ok "0. an Explorer window (Downloads) and a desktop folder (BOX) on the native desktop" "$([ -n "$(row A.TXT)" ] && [ -n "$(di BOX)" ] && echo 1)" "$(cl explorer) $(grep '^dicon' $D/state)"
# a free cell's middle on the screen (Z = 1, the taskbar at the bottom: the cell C,R is at 8 + C*84, 8 + R*86)
cxy() { echo $(( 8 + $1 * 84 + 42 )) $(( 8 + $2 * 86 + 43 )); }
# ---- 1. dragged onto the empty desktop: moved, its icon in the cell under the pointer
# (where the page is on the screen: the pointer moved into the window, the page says where it saw it)
OX=0; OY=0; ev explorer "(()=>{addEventListener('mousemove',e=>{window.__mxy=[e.screenX-e.clientX,e.screenY-e.clientY,e.clientX,e.clientY]},true);return 1})()" >/dev/null
set -- $(cl explorer); X0=$(( $4 + 302 )); Y0=$(( $5 + 203 )); xdotool mousemove $(( X0 - 2 )) $(( Y0 - 3 )); timeout 0.3 tail -f /dev/null; xdotool mousemove $X0 $Y0; timeout 0.5 tail -f /dev/null
M=$(ev explorer "(window.__mxy||[]).join(' ')"); set -- $M; [ -n "$4" ] && { OX=$(( X0 - $3 )); OY=$(( Y0 - $4 )); }; echo "(the page at $OX,$OY on the screen: $M)" 
xdotool mousemove $(row E.TXT) click 1; timeout 0.5 tail -f /dev/null   # (the window has the pointer and the focus first)
d0=$(dn); drag $(row A.TXT) $(cxy 12 1); w8 '[ -n "$(di A.TXT)" ]' 60; lsw '"A.TXT":"file A"'
ok "1. a file dragged from the Explorer window onto the empty desktop: the shell took the drop (desk 12,1), moved into the Desktop folder" "$([ "$(dn)" -gt "$d0" ] && st deskxfer | grep -q 'desk 12,1' && echo "$L" | grep -q '"A.TXT":"file A"' && ! echo "$L" | grep -q '"dl":\[[^]]*"A.TXT"' && echo 1)" "$(st deskxfer) $L $(tail -3 $T/native.log)"
ok "...its icon in that cell (12,1)" "$([ "$(dcell A.TXT)" = 12,1 ] && echo 1)" "$(di A.TXT)"
# ---- 2. Ctrl+drag: copied; onto the folder's icon; onto the Trash
drag $(row B.TXT) $(cxy 12 1) ctrl; w8 '[ -n "$(di B.TXT)" ]' 60; lsw '"B.TXT":"file B"'
ok "2. Ctrl+drag: copied (Downloads keeps it), its icon next to A.TXT" "$(echo "$L" | grep -q '"B.TXT":"file B"' && echo "$L" | grep -q '"dl":\[[^]]*"B.TXT"' && dcell B.TXT | grep -q '^1[123],[012]$' && echo 1)" "$L $(di B.TXT)"
drag $(row C.TXT) $(dc BOX); lsw '"BOX":\["C.TXT"\]'
ok "3. dropped on the desktop folder's icon: into it" "$(echo "$L" | grep -q '"BOX":\["C.TXT"\]' && st deskxfer | grep -q folder && echo 1)" "$(st deskxfer) $L"
drag $(row D.TXT) $(dc Trash); lsw '"tr":\[[^]]*"D.TXT"'
ok "4. dropped on the Trash: in the Trash" "$(echo "$L" | grep -q '"tr":\[[^]]*"D.TXT"' && ! echo "$L" | grep -q '"dl":\[[^]]*"D.TXT"' && echo 1)" "$(st deskxfer) $L"
# ---- 5. the desktop's menu: Paste greyed, then Copy in File Explorer, Paste
xdotool mousemove $(cxy 12 4) click 3; w8 '[ -n "$(cmi Paste)" ]' 20; P0=$(cmidis Paste); xdotool key Escape; timeout 0.5 tail -f /dev/null
ok "5. the desktop's menu with nothing cut or copied: Paste there, greyed" "$([ "$P0" = 1 ] && echo 1)" "$(grep '^cmitem' $D/state)"
ev explorer "(()=>{const r=OA.main.body.querySelector('[data-k=\"'+downloadsPath().join('/')+'/NOTE.TXT\"]');r.click();OA.main.body.querySelector('.cp').click();return 1})()" >/dev/null
w8 '[ "$(fcn)" = 1 ]' 20
xdotool mousemove $(cxy 12 4) click 3; w8 '[ -n "$(cmi Paste)" ]' 20; P1=$(cmidis Paste)
ok "6. Copy in File Explorer: the shell told (1 item), the desktop's Paste no longer greyed" "$([ "$(fcn)" = 1 ] && [ "$P1" = 0 ] && echo 1)" "$(st deskxfer) $(cmi Paste)"
click $(cmixy Paste); w8 '[ -n "$(di NOTE.TXT)" ]' 80; lsw '"NOTE.TXT":"file NOTE"'
ok "7. Paste: copied into the Desktop folder (Downloads keeps it), its icon at the menu's cell (12,4)" "$(echo "$L" | grep -q '"NOTE.TXT":"file NOTE"' && echo "$L" | grep -q '"dl":\[[^]]*"NOTE.TXT"' && [ "$(dcell NOTE.TXT)" = 12,4 ] && echo 1)" "$L $(di NOTE.TXT)"
# ---- 8. Cut in File Explorer, Ctrl+V on the desktop
ev explorer "(()=>{const r=OA.main.body.querySelector('[data-k=\"'+downloadsPath().join('/')+'/E.TXT\"]');r.click();OA.main.body.querySelector('.cu').click();return 1})()" >/dev/null
w8 '[ "$(fcn)" = 1 ]' 20; for t in 1 2 3; do click $(cxy 14 0); timeout 0.6 tail -f /dev/null; xdotool key ctrl+v; w8 '[ -n "$(di E.TXT)" ]' 24 && break; done   # (the desktop takes the keys once clicked: as bgpage161_t.sh's Delete)
 w8 '[ "$(fcn)" = 0 ]' 40; lsw '"E.TXT":"file E"'
ok "8. Cut in File Explorer, Ctrl+V on the desktop: moved, the clipboard empty again (Paste greyed)" "$(echo "$L" | grep -q '"E.TXT":"file E"' && ! echo "$L" | grep -q '"dl":\[[^]]*"E.TXT"' && [ "$(fcn)" = 0 ] && echo 1)" "$(st deskxfer) $L"
# ---- 9. Paste into folder
ev explorer "(()=>{const r=OA.main.body.querySelector('[data-k=\"'+downloadsPath().join('/')+'/NOTE.TXT\"]');r.click();OA.main.body.querySelector('.cp').click();return 1})()" >/dev/null
w8 '[ "$(fcn)" = 1 ]' 20; xdotool mousemove $(dc BOX) click 1 click 3; w8 '[ -n "$(cmi "Paste into folder")" ]' 20; PF=$(cmidis "Paste into folder")
click $(cmixy "Paste into folder"); lsw '"BOX":\["C.TXT","NOTE.TXT"\]'
ok "9. a desktop folder's menu: Paste into folder, pasted into it" "$([ "$PF" = 0 ] && echo "$L" | grep -q '"BOX":\["C.TXT","NOTE.TXT"\]' && echo 1)" "$PF $L"
xdotool mousemove $(dc A.TXT) click 1 click 3; w8 '[ -n "$(cmi Cut)" ]' 20; FM=$(grep '^cmitem' $D/state | sed 's/.*|//' | tr '\n' ',')
ok "10. a file's menu: Cut and Copy, no Paste into folder (as Standard's)" "$(echo "$FM" | grep -q '^Open,Open with…,Cut,Copy,Rename,Move to Trash,Properties,$' && echo 1)" "$FM"
# ---- 11. Copy on a desktop file, Paste in File Explorer
click $(cmixy Copy); w8 '[ "$(fcn)" = 1 ]' 40
ev explorer "(()=>{OA.main.openPath(homePath('DOCUMENTS'));setTimeout(()=>OA.main.body.querySelector('.ps').click(),400);return 1})()" >/dev/null; lsw '"docs":\[[^]]*"A.TXT"'
ok "11. Copy on a desktop file (the shell's menu), Paste in File Explorer (Documents): copied there, the desktop keeps it" "$(echo "$L" | grep -q '"docs":\[[^]]*"A.TXT"' && [ -n "$(di A.TXT)" ] && echo 1)" "$L"
ok "12. no error in the pages" "$(! grep -q 'ReferenceError\|TypeError: .* is not a function\|SyntaxError' $T/native.log && echo 1)" "$(grep 'ReferenceError\|TypeError\|SyntaxError' $T/native.log | tail -3)"
NL=$(ev explorer "$LS"); NC=$(for n in A.TXT B.TXT NOTE.TXT; do printf '%s=%s ' $n $(dcell $n); done)
# ---- Standard's page, the same steps (the same cells, the same menu items), compared
if node -e "require('playwright')" 2>/dev/null; then
	SD=$T/sd; mkdir -p $SD; cp $D/ls.json $D/keys.json $SD/
	python3 $HERE/../native/same_rd.py $SPORT $SD > $T/srd.log 2>&1 & SRP=$!
	(cd $HERE/../native && timeout 300 node explorer-desk161_std.js $SPORT "$PW" > $T/std.json 2> $T/std.log); kill $SRP 2>/dev/null
	SL=$(sed -n 1p $T/std.json); SC=$(sed -n 2p $T/std.json)
	ok "13. Standard's page given the same steps: the same Desktop folder, Downloads, Documents and Trash" "$([ -n "$SL" ] && [ "$SL" = "$NL" ] && echo 1)" "Standard: $SL / Native: $NL / $(tail -5 $T/std.log)"
	ok "...and the same places for the icons dropped and pasted at a cell ($NC; E.TXT's, pasted with Ctrl+V, is the first free one, which depends on the stand-in's icons)" "$([ -n "$SC" ] && [ "$SC" = "$NC" ] && echo 1)" "Standard: $SC / Native: $NC"
else echo "(Standard's comparison skipped: no Playwright)"; fi
echo "$((N-F))/$N passed"; [ $F = 0 ]
