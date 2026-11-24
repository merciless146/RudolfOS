#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 38, step 3c): the shell's hidden page (#app=do-bg) draws no pictures it never shows (lean61.js, its
# second part): its start menu's, desktop's and taskbar's rows have none, the catalogue it gives is the same, and at the next start the
# catalogue's pictures are taken from the last one rudolfd keeps instead of drawn again. rudolf-native under Xvfb with the stand-in
# rudolfd (fixtures/native_rd.py), signed in twice with the same account (the first start, then the next one, as after a restart):
#   1. the first start: the hidden page signed in and ready (svc, portal); no picture drawn in its start menu, desktop or taskbar (its
#      rows marked as the page pictures them, without the pictures), the catalogue given once, its pictures drawn (oaPng)
#   2. the catalogue: every start menu row as the page shows it with its pictures (iconsApply's): the row's picture key and emoji, its
#      words without the emoji; every picture it names is in it, a PNG; each drawn picture's mark (ich: WebKit's)
#   3. the next start: the catalogue's pictures all taken from the last one (none drawn), the same PNGs; the menu the same
#   4. the Emoji icon style: no pictures anywhere, the catalogue with none (as before)
# Needs Xvfb, xdotool, node, python3, gcc and WebKitGTK's headers.   sh bglean162_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/bglean162.XXXXXX); PORT=${BGLEAN_PORT:-8714}; DSP=${BGLEAN_DSP:-:95}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; [ -n "$BGLEAN_KEEP" ] && cp $T/cat*.json $BGLEAN_KEEP/ 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
ev() { : > $T/ev.out; printf '%s %s\n' "$1" "$2" >&7; w8 "grep -q '^$1 ' $T/ev.out 2>/dev/null" 40; grep "^$1 " $T/ev.out | tail -1 | cut -d' ' -f2-; : > $T/ev.out; }
# (the start menu's rows as the page shows them with its pictures, read from the hidden page's own rows: iconsApply's rule, an emoji
# first, then a space; what the catalogue's menu must say of each)
ROWS="JSON.stringify([...document.querySelectorAll('#menu ul > li')].filter(li=>getComputedStyle(li).display!='none').map(li=>{const t=li.textContent,m=/^(\\S+)\\s/.exec(t),k=m&&(EMO2IC[m[1]]||EMO2IC[m[1].replace(/\\uFE0F/g,'')]);return k?[li.dataset.v25||li.dataset.a||'',k,m[1],t.slice(m[0].length).trim()]:[li.dataset.v25||li.dataset.a||'','','',t.trim()]}))"
start() {   # start SETTINGS: rudolf-native signed in as Ann with the account's folder $D (kept between starts)
	python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
	rm -f $T/ev $D/state; mkfifo $T/ev
	(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$T/ev exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
	exec 7<>$T/ev
	w8 '[ "$(st signin)" = 1 ]' 160; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
	w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ] && [ "$(st portal | cut -d" " -f1)" = ready ]' 240
	w8 '[ "$(ev do-bg "String(!!(window.__oaBgLean&&__oaBgLean().given))")" = true ]' 120; timeout 4 tail -f /dev/null
}
stop() { kill $NP 2>/dev/null; wait $NP 2>/dev/null; pkill -f "$T/" 2>/dev/null; kill $RDP 2>/dev/null; wait $RDP 2>/dev/null; exec 7>&-; timeout 2 tail -f /dev/null; }
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json >/dev/null || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","h24":true},"pins":["notepad","calc","settings"],"notes":2,"wallpaper":false}' > $D/settings.json
# ---- 1. the first start
start
L1=$(ev do-bg "JSON.stringify(__oaBgLean())")
ok "1. the first start: the hidden page ready; no picture in its start menu, desktop or taskbar, none drawn; the catalogue given once, its pictures drawn" "$(echo "$L1" | python3 -c "import json,sys;j=json.loads(sys.stdin.read());print(int(j['on'] and j['given']==1 and j['kept']==0 and j['drawn']==0 and j['png']>=20 and j['seeded']==0))" 2>/dev/null)" "$L1 $(st svc) $(st portal)"
M=$(ev do-bg "String(__oaBgLean().marked)+'|'+document.querySelectorAll('#menu li').length")
ok "...its rows marked as the page pictures them, without the pictures ($M)" "$(echo "$M" | grep -q '^[1-9][0-9]*|[1-9]' && echo 1)" "$M"
R1=$(ev do-bg "$ROWS"); cp $D/apps.json $T/cat1.json 2>/dev/null
# ---- 2. the catalogue
C2=$(python3 - "$T/cat1.json" "$R1" <<'PY'
import json,sys,re
c=json.load(open(sys.argv[1])); rows=json.loads(sys.argv[2]); bad=[]
menu=[m for m in c['menu'] if m.get('t')=='li']
if len(menu)!=len(rows): bad.append('rows %d/%d'%(len(menu),len(rows)))
for m,r in zip(menu,rows):
    v,k,e,txt=r
    if (m.get('v') or m.get('a') or '')!=v: bad.append('order %s/%s'%(m.get('v') or m.get('a'),v)); continue
    if k and (m['ic']!=k or m['emo']!=e or m['txt']!=txt): bad.append('row %s: %r'%(v,[m['ic'],m['emo'],m['txt']]))
    if not k and m['txt']!=txt: bad.append('row %s words %r'%(v,m['txt']))
ics=set(m['ic'] for m in menu if m['ic'])|set(a.get('ic') for a in c['apps'] if a.get('ic'))|set(d.get('ic') for d in c['desk'] if d.get('ic'))
for k in ics:
    if not re.match(r'^data:image/png;base64,',c['icons'].get(k,'')): bad.append('picture '+k)
ich=c.get('ich') or {}
if not ich or any(not str(v).endswith('.w') for v in ich.values()): bad.append('ich %r'%list(ich.items())[:3])
print('ok' if not bad else '; '.join(bad[:8]), len(menu), len(ics), len(ich))
PY
)
ok "2. the catalogue: each start menu row as the page shows it with its pictures (its picture, emoji, words); every picture it names a PNG; the drawn ones marked (WebKit's)" "$(echo "$C2" | grep -q '^ok ' && echo 1)" "$C2"
stop
# ---- 3. the next start
start
L4=$(ev do-bg "JSON.stringify(__oaBgLean())"); R4=$(ev do-bg "$ROWS"); cp $D/apps.json $T/cat2.json 2>/dev/null
C4=$(python3 - "$T/cat1.json" "$T/cat2.json" <<'PY'
import json,sys
a=json.load(open(sys.argv[1])); b=json.load(open(sys.argv[2]))
d=[k for k in set(a)|set(b) if a.get(k)!=b.get(k)]
print('same' if not d else 'differ: '+','.join(sorted(d)))
PY
)
ok "3. the next start: the catalogue's pictures all taken from the last one, none drawn; no picture in the start menu, desktop or taskbar" "$(echo "$L4" | python3 -c "import json,sys;j=json.loads(sys.stdin.read());print(int(j['on'] and j['given']==1 and j['kept']==0 and j['drawn']==0 and j['png']==0 and j['seeded']>=20))" 2>/dev/null)" "$L4"
ok "...the catalogue the same as the first start's (its pictures, PNG for PNG; its menu)" "$([ "$C4" = same ] && [ "$R4" = "$R1" ] && echo 1)" "$C4"
ev do-bg "S.icons='emoji';persistS();1" >/dev/null; timeout 3 tail -f /dev/null; ev do-bg "window.__rudolfLsFlush&&__rudolfLsFlush();1" >/dev/null; timeout 2 tail -f /dev/null
stop
# ---- 4. the Emoji icon style
start
E5=$(ev do-bg "S.icons+'|'+document.querySelectorAll('img.ico').length+'|'+__oaBgLean().png")
[ "${E5%%|*}" = emoji ] || { ev do-bg "S.icons='emoji';iconsApply();oaCatalog().then(()=>oaPost({op:'cat'}));1" >/dev/null; timeout 4 tail -f /dev/null; E5=$(ev do-bg "S.icons+'|'+document.querySelectorAll('img.ico').length+'|'+__oaBgLean().png"); }
cp $D/apps.json $T/cat3.json 2>/dev/null
C5=$(python3 -c "import json,sys;c=json.load(open('$T/cat3.json'));print(len([m for m in c['menu'] if m.get('ic')]),len([k for k in c['icons'] if not k.startswith('im')]))" 2>/dev/null)
ok "4. the Emoji icon style: no pictures in the hidden page, none drawn; the catalogue with no drawn pictures" "$(echo "$E5" | grep -q '^emoji|0|0$' && [ "$C5" = "0 0" ] && echo 1)" "$E5 / $C5"
stop
echo "$((N-F))/$N passed"; [ $F = 0 ]
