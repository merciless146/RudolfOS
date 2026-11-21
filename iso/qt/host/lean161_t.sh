#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 23 B, stage 3): an app's window on the native desktop builds only what it shows (lean61.js): no pictures for
# the hidden start menu, desktop and taskbar, no pinned buttons, no dynamic background's moving parts, no catalogue at sign-in when the
# hidden page's is the same; the shell's own pages (#app=do-NAME) build all of it as before. rudolf-native under Xvfb with the stand-in
# rudolfd (fixtures/native_rd.py), Spirit Falls (the statue) as the background:
#   1. the hidden page: not lean, its catalogue given (every menu row's picture, the statue), the start menu's pictures made
#   2. Notepad's window: lean; none of the hidden parts' pictures, no pinned buttons, no statue or petals; no catalogue given (the hidden
#      page's is the same); it works (typed); a window of its page's own (a dialog) has its title bar's picture
#   3. every app of the page (those with a window of the page's) opened in a native window of its own, one after another: its window
#      there (its title), lean, no ReferenceError, TypeError or SyntaxError in its page; closed
#   4. the main actions: Calculator (2+3=5), File Explorer (a folder opened), Settings (a page; the icons' style changed there: the hidden
#      page gives the catalogue again, without the pictures), an app hidden from the start menu in Settings (that window gives it)
#   5. Part C: Notepad minimized rests and comes back, lean, with its file
#   6. no ReferenceError, TypeError or SyntaxError in any page's log
# Needs Xvfb, xdotool, node, python3, gcc and WebKitGTK's headers.   sh lean161_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/lean161.XXXXXX); PORT=${LEAN_PORT:-8625}; DSP=${LEAN_DSP:-:97}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
# (Ann's background: Spirit Falls, no picture of her own)
python3 -c "import json;p='$D/ls.json';o=json.load(open(p));k='bd95set:uAnnA1b2';s=json.loads(o['data'][k]);s.update(dyn='oasis',wpimg='');o['data'][k]=json.dumps(s);json.dump(o,open(p,'w'))"
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","h24":true,"idlerest":true},"pins":["notepad","explorer","settings","calc"],"notes":2,"wallpaper":false}' > $D/settings.json
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
mkfifo $T/ev
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$T/ev RUDOLF_NATIVE_REST_SECS=4 exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
exec 7<>$T/ev
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
click() { xdotool mousemove $1 $2 click 1; }
# (a page's answer: KEY JS, read back from the FIFO's .out; the hidden page by its name)
ev() { : > $T/ev.out; printf '%s %s\n' "$1" "$2" >&7; w8 "grep -q '^$1 ' $T/ev.out 2>/dev/null" ${3:-40}; grep "^$1 " $T/ev.out | tail -1 | cut -d' ' -f2-; : > $T/ev.out; }
cl() { grep "^client $1 " $D/state | head -1; }; cv() { cl $1 | awk -v f=$2 '{print $f}'; }
cid() { grep "^client .* app=$1 " $D/state | head -1 | cut -d' ' -f2; }
rs() { grep "^rest $1 " $D/state | head -1 | cut -d' ' -f3-; }
mini() { I=$(cid $1); click $(( $(cv $I 4) + $(cv $I 6) + 2 - 75 )) $(( $(cv $I 5) + 18 )); w8 "cl $I | grep -q ' min=1 '" 20; }
# (a TypeError counted unless it is a promise rejected because the stand-in rudolfd has no answer for that request: Task Manager's processes,
# Packages' list, as on main)
errs() { grep "ReferenceError\|TypeError\|SyntaxError" $T/native.log | grep -vc "Unhandled Promise Rejection: TypeError: undefined is not an object"; }
perr() { grep "#app=$1&.*\(ReferenceError\|TypeError\|SyntaxError\)" $T/native.log | grep -vc "Unhandled Promise Rejection: TypeError: undefined is not an object"; }
cats() { grep -c "POST desktop/apps" $D/calls 2>/dev/null; }
catj() { python3 -c "import json,sys;c=json.load(open('$D/apps.json'));print($1)" 2>/dev/null; }
# (the hidden parts' pictures: the start menu's rows, the desktop's icons, the taskbar's buttons)
HID="[...document.querySelectorAll('#menu img.ico,#desk>.ic img.ico,#pins img,#tasks img.ico')].length+'|'+document.querySelectorAll('#pins .pin').length"
openw() { ev do-bg "oaPost({op:'open',app:'$1'});1" >/dev/null; w8 "grep -q '^client .* app=$1 ' \$D/state" ${2:-120} && w8 "[ \"\$(ev $1 'String(!!(OA.main&&OA.main.el&&document.title))')\" = true ]" ${3:-80}; }
w8 '[ "$(st signin)" = 1 ]' 160; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ]' 240; w8 '[ "$(cats)" -ge 1 ]' 80; timeout 3 tail -f /dev/null
B=$(ev do-bg "JSON.stringify([__oaLean().on,[...document.querySelectorAll('#menu img.ico')].length])")
ok "1. the hidden page: not lean, the start menu's pictures made, its catalogue given (the menu's pictures, the statue)" "$([ "$(echo "$B" | cut -c1-7)" = '[false,' ] && ! echo "$B" | grep -q ',0]' && [ "$(catj "sum(1 for r in c['menu'] if r.get('ic'))>10 and bool(c.get('fx',{}).get('statue'))")" = True ] && echo 1)" "$B; $(catj "[sum(1 for r in c['menu'] if r.get('ic')),list(c.get('fx',{}))]")"
C0=$(cats); E0=$(errs)
openw notepad; timeout 4 tail -f /dev/null; xdotool type --delay 30 'lean'; timeout 1 tail -f /dev/null
L=$(ev notepad "JSON.stringify([__oaLean(),$HID,!!(STS.el||PET.cv||AUR.cv),document.querySelector('.oamain textarea').value])")
ok "2. Notepad's window: lean, none of the hidden parts' pictures or pinned buttons, no statue or petals; it works (typed)" "$(echo "$L" | grep -q '^\[{"on":true,"full":0,"given":0,"skipped":[1-9][0-9]*},"0|0",false,"lean"\]$' && echo 1)" "$L"
timeout 3 tail -f /dev/null
ok "2b. ...no catalogue given by it (the hidden page's is the same)" "$([ "$(cats)" = "$C0" ] && echo 1)" "$(cats) / $C0"
W=$(ev notepad "(()=>{const W=win('Find','📁',260,120);const r=!!W.el.querySelector('.tb img.ico');setTimeout(()=>{window.__lt=!!W.el.querySelector('.tb img.ico');W.close()},300);return r})()"); timeout 1 tail -f /dev/null
ok "2c. ...a window of its page's own (a dialog): its title bar's picture drawn" "$([ "$(ev notepad 'String(window.__lt)')" = true ] && echo 1)" "$W $(ev notepad 'String(window.__lt)')"
# ---- 3. every app
LIST=$(ev do-bg "Object.keys(APPS).filter(k=>APPS[k]&&APPS[k].name&&typeof APPS[k].open=='function'&&!/[:]/.test(k)&&!oaExt(k)&&!['notepad','browser'].includes(k)).join(' ')")
# (no window of the page's own here, as on main: the camera and the sound recorder need a device, Report a problem is a dialog over the
# screen, these games ask to download their data first)
KNOWN=" camera recorder report frostbarricade nightflight logicpack blockdrop faultlines "
NO=; NW=; BAD=; NA=0
for k in $LIST; do NA=$((NA+1)); e0=$(perr $k)
	if openw $k 120 100; then
		r=$(ev $k "String(__oaLean().on)"); [ "$r" = true ] || BAD="$BAD $k(lean=$r)"
		timeout 1 tail -f /dev/null; e1=$(perr $k); [ "$e1" = "$e0" ] || BAD="$BAD $k(errors)"
	else grep -q "^client .* app=$k " $D/state || case "$KNOWN" in *" $k "*) NW="$NW $k";; *) NO="$NO $k";; esac
		e1=$(perr $k); [ "$e1" = "$e0" ] || BAD="$BAD $k(errors)"; fi
	I=$(cid $k); [ -n "$I" ] && { ev $k "window.__oneAppClose();1" >/dev/null; w8 "[ -z \"\$(cid $k)\" ]" 40 || { ev $k "OA.main&&OA.main.el&&(OA.main.onclose=null);oaDone();1" >/dev/null; w8 "[ -z \"\$(cid $k)\" ]" 40; }; }
done
ok "3. every app ($NA) in a native window of its own: its window there, lean, no error in its page; closed (no window of its own, as on main:$NW)" "$([ -z "$BAD$NO" ] && [ $NA -gt 30 ] && echo 1)" "no window:$NO; $BAD; $(grep 'ReferenceError\|TypeError\|SyntaxError' $T/native.log | tail -3)"
# ---- 4. main actions
openw calc; R=$(ev calc "(()=>{const b=[...OA.main.body.querySelectorAll('button')],p=t=>{const x=b.find(e=>e.textContent.trim()==t);x&&x.click()};p('2');p('+');p('3');p('=');return OA.main.body.innerText.includes('5')})()")
ok "4a. Calculator: 2+3=5" "$([ "$R" = true ] && echo 1)" "$R $(ev calc 'OA.main.body.innerText.slice(0,200)')"
openw explorer; ev explorer "(()=>{const ad=OA.main.body.querySelector('.ad');ad.value='C:\\\\USERS';ad.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}));return 1})()" >/dev/null; timeout 1 tail -f /dev/null
ok "4b. File Explorer: a folder opened (C:\\USERS)" "$([ "$(ev explorer "OA.main.body.querySelector('.ad').value")" = 'C:\USERS' ] && echo 1)" "$(ev explorer "OA.main.body.querySelector('.ad').value")"
openw settings; C1=$(cats)
ev settings "(()=>{S.icons='emoji';if(SC)SC.icons='emoji';persistS();applySet();return 1})()" >/dev/null
w8 '[ "$(cats)" -gt "$C1" ] && [ "$(catj "len(c.get(\"icons\",{}))")" = 0 ]' 80
ok "4c. Settings: the icons' style changed there: the hidden page gives the catalogue again (no pictures now), Settings' window gives none" "$([ "$(catj "len(c.get('icons',{}))")" = 0 ] && [ "$(ev settings 'String(__oaLean().given)')" = 0 ] && [ "$(ev do-bg "document.querySelectorAll('#menu img.ico').length")" = 0 ] && echo 1)" "$(cats)/$C1 icons $(catj "len(c.get('icons',{}))") $(ev settings 'JSON.stringify(__oaLean())')"
ev settings "(()=>{S.icons='rudolf';if(SC)SC.icons='rudolf';persistS();applySet();return 1})()" >/dev/null; w8 '[ "$(catj "len(c.get(\"icons\",{}))>5")" = True ]' 80
C2=$(cats); ev settings "(()=>{const h=ld(HIDEK(),[]);h.push('calc');svj(HIDEK(),h);startMenuApps();oaCatPush();return 1})()" >/dev/null
w8 '[ "$(catj "\"calc\" in c[\"hide\"]")" = True ]' 80
ok "4d. an app hidden from the start menu in Settings: that window gives the catalogue (with every picture made first)" "$([ "$(catj "'calc' in c['hide'] and len(c.get('icons',{}))>5")" = True ] && [ "$(ev settings 'String(__oaLean().given)')" -ge 1 ] && echo 1)" "$(catj "c['hide']") $(ev settings 'JSON.stringify(__oaLean())')"
ev settings "(()=>{svj(HIDEK(),ld(HIDEK(),[]).filter(k=>k!='calc'));startMenuApps();oaCatPush();return 1})()" >/dev/null
for k in calc explorer settings; do ev $k "window.__oneAppClose();1" >/dev/null; done; timeout 2 tail -f /dev/null
# ---- 5. Part C
NI=$(cid notepad); ev notepad "(()=>{const ta=OA.main.body.querySelector('textarea');const t=OA.main.dtabs.tabs[OA.main.dtabs.i];ta.value='';ta.dispatchEvent(new Event('input'));OA.main.dtabs.open(['C:','README.TXT']);return 1})()" >/dev/null; timeout 1 tail -f /dev/null
mini notepad; w8 '[ "$(rs notepad | cut -d" " -f1)" = 1 ]' 160; R1=$(rs notepad)
click $(grep "^hit win $NI " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'); w8 '[ "$(ev notepad "String((()=>{try{return !!OA.restored}catch(e){return false}})())")" = true ]' 80; timeout 1 tail -f /dev/null
R=$(ev notepad "JSON.stringify([__oaLean().on,OA.main.dtabs.tabs.map(t=>t.p&&t.p.join('/')).filter(Boolean),$HID])")
ok "5. Part C: Notepad minimized rests and comes back, lean, with its file" "$(echo "$R1" | grep -q '^1' && echo "$R" | grep -q '^\[true,\[.*"C:/README.TXT".*\],"0|0"\]$' && echo 1)" "$R1 / $R"
ok "6. no ReferenceError, TypeError or SyntaxError in any page's log" "$([ "$(errs)" = "$E0" ] && echo 1)" "$(grep 'ReferenceError\|TypeError\|SyntaxError' $T/native.log | tail -3)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
