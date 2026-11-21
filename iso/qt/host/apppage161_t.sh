#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 23 B, stage 2): an app's window on the native desktop loads the apps' page (desktop/oapage61.py: the page
# without the modules no app of it uses, each a file of its own read the first time anything of it is used). rudolf-native under Xvfb with
# the stand-in rudolfd (fixtures/native_rd.py, which gives a/ID.html for /?oa=ID as rudolfd does):
#   1. the hidden page: the whole page; Notepad's window: the apps' page (lean too), nothing of what it leaves out read, it works
#   2. every app of the page in a native window of its own: the apps' page (or, for the apps listed in oapage61.py's OA_NOT, the whole
#      page), nothing left out read (a missing function fails here, loudly), no error
#   3. a function left out, used in Notepad's window: its module read then and the function's answer right; a name of one read: the same
#   4. a page installed by hand (its apps' page not among the files): every window the whole page, and it works
#   5. Part C: Notepad minimized rests and comes back in the apps' page
#   6. no ReferenceError, TypeError or SyntaxError in any page's log
# Needs Xvfb, xdotool, node, python3, gcc and WebKitGTK's headers.   sh apppage161_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/apppage161.XXXXXX); PORT=${APPP_PORT:-8627}; DSP=${APPP_DSP:-:95}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","h24":true,"idlerest":true},"pins":["notepad","explorer","settings","calc"],"notes":2,"wallpaper":false}' > $D/settings.json
mkdir -p $T/q/qt/host; cp -r $HERE/fixtures $T/q/qt/host/; cp -r $HERE/../../www $T/q/www; W=$T/q/www
python3 $T/q/qt/host/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
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
# (the window's first page sends it to the apps' page: asked only once the page there has its window)
openw() { ev do-bg "oaPost({op:'open',app:'$1'});1" >/dev/null; w8 "grep -q '^client .* app=$1 ' \$D/state" ${2:-120} && w8 "[ \"\$(ev $1 '(()=>{try{return String(!!(OA.main&&OA.main.el&&document.title))}catch(e){return String(false)}})()')\" = true ]" ${3:-80}; }
OAH=$(grep -o "a/[0-9a-f]\{16\}\.html" $W/index.html | head -1); OAN=$(sed -n "s/^OA_NOT = \[\(.*\)\]$/\1/p" $HERE/../../../desktop/oapage61.py | tr -d "' ")
lz() { ev $1 "JSON.stringify([location.search.includes('oa='),!!window.__OAPAGE,(window.__oaLazy||[]).join(' ')])"; }
w8 '[ "$(st signin)" = 1 ]' 160; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ]' 240; timeout 3 tail -f /dev/null; E0=$(errs)
B=$(lz do-bg)
openw notepad; timeout 3 tail -f /dev/null; xdotool type --delay 30 'apps'; timeout 1 tail -f /dev/null
L=$(ev notepad "JSON.stringify([location.search.includes('oa='),!!window.__OAPAGE,(window.__oaLazy||[]).length,__oaLean().on,document.querySelector('.oamain textarea').value])")
ok "1. the hidden page: the whole page; Notepad's window: the apps' page (lean too), nothing left out read, it works (typed)" "$([ -s $W/$OAH ] && [ "$B" = '[false,false,""]' ] && [ "$L" = '[true,true,0,true,"apps"]' ] && echo 1)" "$OAH; $B; $L"
LIST=$(ev do-bg "Object.keys(APPS).filter(k=>APPS[k]&&APPS[k].name&&typeof APPS[k].open=='function'&&!/[:]/.test(k)&&!oaExt(k)&&k!='notepad').join(' ')")
KNOWN=" camera recorder report frostbarricade nightflight logicpack blockdrop faultlines "
NO=; NW=; BAD=; NA=0; NP=0
for k in $LIST; do NA=$((NA+1)); e0=$(perr $k)
	if openw $k 120 100; then
		r=$(lz $k); case ",$OAN," in *",$k,"*) [ "$r" = '[false,false,""]' ] || BAD="$BAD $k(whole?$r)";; *) [ "$r" = '[true,true,""]' ] && NP=$((NP+1)) || BAD="$BAD $k($r)";; esac
		timeout 1 tail -f /dev/null; e1=$(perr $k); [ "$e1" = "$e0" ] || BAD="$BAD $k(errors)"
	else grep -q "^client .* app=$k " $D/state || case "$KNOWN" in *" $k "*) NW="$NW $k";; *) NO="$NO $k";; esac
		e1=$(perr $k); [ "$e1" = "$e0" ] || BAD="$BAD $k(errors)"; fi
	I=$(cid $k); [ -n "$I" ] && { ev $k "window.__oneAppClose();1" >/dev/null; w8 "[ -z \"\$(cid $k)\" ]" 40 || { ev $k "OA.main&&OA.main.el&&(OA.main.onclose=null);oaDone();1" >/dev/null; w8 "[ -z \"\$(cid $k)\" ]" 40; }; }
done
ok "2. every app ($NA; $NP in the apps' page) in a native window of its own: nothing left out read, no error (no window of its own, as on main:$NW)" "$([ -z "$BAD$NO" ] && [ $NP -gt 20 ] && echo 1)" "no window:$NO; $BAD; $(grep 'ReferenceError\|TypeError\|SyntaxError' $T/native.log | tail -3)"
R=$(ev notepad "(()=>{const a=mlEsc('<b>&');const v=Object.keys(__OAM).find(m=>m!='mail25.js'&&__OAM[m].v.length);const n=__OAM[v].v[0],x=(0,eval)(n);return JSON.stringify([a,(window.__oaLazy||[]).join(' '),v,x!==undefined,typeof hwcSummary])})()")
ok "3. a function left out (mail25.js mlEsc), used in Notepad's window: its module read then, its answer right; a name of another read: the same" "$(echo "$R" | grep -q '^\["&lt;b&gt;&amp;","mail25.js[^"]* [a-z0-9]*\.js[^"]*","[a-z0-9]*\.js",true,"function"\]$' && echo 1)" "$R"
ev notepad "(()=>{const ta=OA.main.body.querySelector('textarea'),t=OA.main.dtabs.tabs[OA.main.dtabs.i];ta.value=t.saved||'';ta.dispatchEvent(new Event('input'));return 1})()" >/dev/null
ev notepad "window.__oneAppClose();1" >/dev/null; w8 '[ -z "$(cid notepad)" ]' 40
mv $W/$OAH $W/$OAH.gone; openw explorer; openw calc
R=$(lz explorer)$(lz calc); R2=$(ev calc "(()=>{const b=[...OA.main.body.querySelectorAll('button')],p=t=>{const x=b.find(e=>e.textContent.trim()==t);x&&x.click()};p('2');p('+');p('3');p('=');return OA.main.body.innerText.includes('5')})()")
ok "4. a page installed by hand (no apps' page among its files): the windows take the whole page, and work (2+3=5)" "$([ "$R" = '[false,false,""][false,false,""]' ] && [ "$R2" = true ] && echo 1)" "$R $R2"
for k in explorer calc; do ev $k "window.__oneAppClose();1" >/dev/null; done; mv $W/$OAH.gone $W/$OAH; timeout 2 tail -f /dev/null
openw notepad; NI=$(cid notepad); ev notepad "(()=>{OA.main.dtabs.open(['C:','README.TXT']);return 1})()" >/dev/null; timeout 1 tail -f /dev/null
mini notepad; w8 '[ "$(rs notepad | cut -d" " -f1)" = 1 ]' 160; R1=$(rs notepad)
click $(grep "^hit win $NI " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'); w8 '[ "$(ev notepad "String((()=>{try{return !!OA.restored}catch(e){return false}})())")" = true ]' 80; timeout 1 tail -f /dev/null
R=$(ev notepad "JSON.stringify([location.search.includes('oa='),OA.main.dtabs.tabs.map(t=>t.p&&t.p.join('/')).filter(Boolean).includes('C:/README.TXT')])")
ok "5. Part C: Notepad minimized rests and comes back in the apps' page, with its file" "$(echo "$R1" | grep -q '^1' && [ "$R" = '[true,true]' ] && echo 1)" "$R1 / $R / $(ev notepad "String((document.querySelector('.rmodal,.natdlg')||{}).innerText||'').slice(0,200)")"
ok "6. no ReferenceError, TypeError or SyntaxError in any page's log" "$([ "$(errs)" = "$E0" ] && echo 1)" "$(grep 'ReferenceError\|TypeError\|SyntaxError' $T/native.log | tail -3)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
