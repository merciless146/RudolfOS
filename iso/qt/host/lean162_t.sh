#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 38, step 2): Task Manager's small page on the native desktop (desktop/lean62.py), in WebKit. rudolf-native
# under Xvfb with the stand-in rudolfd (fixtures/native_rd.py: /?oa=ID as rudolfd serves it; Task Manager's answers from fixtures/tm62.json),
# twice: as built (the hidden page names the small page, the shell opens Task Manager there) and with RUDOLF_NATIVE_TM_FULL (the whole page,
# as before):
#   1. the hidden page named the small page (op tmpage) and Task Manager's window opened it (/?oa=ID, served as the page), in a web
#      process of its own; no sign-in screen
#   2. every tab drawn, the small page never read the rest of the page, no error in the log
#   3. its own web process's memory (private, and PSS), 20 s after it opened: the small page's against the whole page's (both written)
# Needs Xvfb, xdotool, node, python3, gcc and WebKitGTK's headers.   sh lean162_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/lean162.XXXXXX); PORT=${LEAN_NPORT:-8674}; DSP=${LEAN_DSP:-:96}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
W=$HERE/../../www; TMID=$(grep -o "__LEAN_TM='[0-9a-f]\{16\}'" $W/index.html | head -1 | cut -d"'" -f2)
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
# (the web processes rudolf-native started: its children)
webs() { python3 -c "
import os,sys
top=int(sys.argv[1])
def anc(p):
    while p>1:
        try: p=int(open('/proc/%d/stat'%p).read().rsplit(')',1)[1].split()[1])
        except Exception: return False
        if p==top: return True
    return False
for d in os.listdir('/proc'):
    if d.isdigit():
        try: c=open('/proc/'+d+'/cmdline','rb').read()
        except Exception: continue
        if b'WebKitWebProcess' in c.split(b'\\0')[0] and anc(int(d)): print(d)
" $NP; }
mem() { awk '/^Private_(Clean|Dirty):/{p+=$2}/^Pss:/{s+=$2}END{printf "%.1f %.1f", p/1024, s/1024}' /proc/$1/smaps_rollup; }
run() {   # run MODE (small or full): sets R_OPEN R_TABS R_MEM
	D=$T/d.$1; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
	PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
	printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","h24":true},"pins":["notepad","calc"],"notes":2,"wallpaper":false}' > $D/settings.json
	cp $HERE/fixtures/tm62.json $D/tm.json; : > $D/stats-on
	python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.$1.log 2>&1 & RDP=$!
	rm -f $T/ev $T/ev.out; mkfifo $T/ev
	FULL=; [ $1 = full ] && FULL=1
	(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$T/ev ${FULL:+RUDOLF_NATIVE_TM_FULL=1} exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.$1.log 2>&1) & NP=$!
	exec 7<>$T/ev
	st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
	ev() { : > $T/ev.out; printf '%s %s\n' "$1" "$2" >&7; w8 "grep -q '^$1 ' $T/ev.out 2>/dev/null" ${3:-40}; grep "^$1 " $T/ev.out | tail -1 | cut -d' ' -f2-; : > $T/ev.out; }
	w8 '[ "$(st signin)" = 1 ]' 160; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
	w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ]' 240; timeout 4 tail -f /dev/null
	B=$(webs | sort)
	ev do-bg "oaPost({op:'open',app:'taskmgr'});1" >/dev/null
	w8 "grep -q '^client .* app=taskmgr ' \$D/state" 120
	w8 "[ \"\$(ev taskmgr '(()=>{try{return String(!!(window.__tmW&&__tmW.body.querySelector(\".tmb\").textContent.length>20))}catch(e){return String(false)}})()')\" = true ]" 120
	TP=$(for p in $(webs); do echo "$B" | grep -qx "$p" || echo $p; done | head -1)
	R_OPEN=$(ev taskmgr "JSON.stringify([location.search,window.__LEAN||'',(()=>{const l=document.getElementById('login');return !!(l&&l.offsetParent)})()])")
	for t in apps procs perf system hw start; do ev taskmgr "__tmW.body.querySelector(\".tmtabs button[data-t='$t']\").click();1" >/dev/null; timeout 1.5 tail -f /dev/null; done
	R_TABS=$(ev taskmgr "JSON.stringify([(window.__leanRead||[]).join(' '),__tmW.body.querySelector('.tmb').textContent.length>20])")
	timeout 12 tail -f /dev/null
	R_MEM=$([ -n "$TP" ] && mem $TP)
	R_ERR=$(grep "ReferenceError\|TypeError\|SyntaxError" $T/native.$1.log | grep -c "?oa=\|#app=taskmgr")
	R_CALLS=$(grep -c "^PAGE oa" $D/calls)
	kill $NP 2>/dev/null; wait $NP 2>/dev/null; pkill -f "$T/" 2>/dev/null; kill $RDP 2>/dev/null; wait $RDP 2>/dev/null; exec 7>&-; timeout 2 tail -f /dev/null
}
run small; S_OPEN=$R_OPEN; S_TABS=$R_TABS; S_MEM=$R_MEM; S_ERR=$R_ERR; S_CALLS=$R_CALLS
run full; F_OPEN=$R_OPEN; F_TABS=$R_TABS; F_MEM=$R_MEM
ok "1. the hidden page named the small page and Task Manager opened it (/?oa=$TMID, served as the page), in its own web process; no sign-in screen" "$([ -n "$TMID" ] && [ "$S_OPEN" = "[\"?oa=$TMID\",\"taskmgr\",false]" ] && [ "$S_CALLS" -ge 1 ] && [ -n "$S_MEM" ] && echo 1)" "$TMID $S_OPEN calls $S_CALLS mem '$S_MEM'"
ok "2. every tab drawn, the rest of the page never read, no error; the whole page (RUDOLF_NATIVE_TM_FULL) the whole page" "$([ "$S_TABS" = '["",true]' ] && [ "$S_ERR" = 0 ] && [ "$F_OPEN" = '["","",false]' ] && [ "$F_TABS" = '["",true]' ] && echo 1)" "$S_TABS err $S_ERR; full $F_OPEN $F_TABS"
echo "memory of Task Manager's own web process (private, PSS MB): the whole page $F_MEM; the small page $S_MEM"
ok "3. the small page's web process uses less memory than the whole page's ($F_MEM -> $S_MEM)" "$([ -n "$S_MEM" ] && [ -n "$F_MEM" ] && [ "$(python3 -c "print(int(${S_MEM%% *} < ${F_MEM%% *}))")" = 1 ] && echo 1)" "$F_MEM / $S_MEM"
echo "$((N-F))/$N passed"; [ $F = 0 ]
