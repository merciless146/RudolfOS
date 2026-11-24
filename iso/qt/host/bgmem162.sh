#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 38, step 3c): the shell's hidden page's memory in WebKit, as the owner's Vivobook runs it.
# rudolf-native under Xvfb with the stand-in rudolfd (fixtures/native_rd.py) serving this tree's page; signed in as Ann, nothing opened:
# the hidden page (#app=do-bg) is then the only page, in this rudolf-native's one web process. Read 30 s after it said "svc ready" and
# "portal ready": the web process's RssAnon (/proc/PID/status, the readings the owner's Vivobook gave after its restart), its private
# memory and PSS (smaps_rollup). Each run starts twice with the same account (the first start, then the next one, as after a restart:
# rudolfd keeps what the page gave, the catalogue among it). One line of JSON per start:
#   sh bgmem162.sh [RUDOLF-NATIVE] [RUNS]
#   BGMEM_SET='{"s":{...}}'   the account's settings (default: a solid colour, as the host tests); BGMEM_EVAL=JS also asked of the page;
#   BGMEM_KEEP=PREFIX          the catalogue each start gave, kept as PREFIX.RUN.START.json
# Ports: BGMEM_PORT (default 8712), display BGMEM_DSP (:92). Needs Xvfb, xdotool, node, python3, gcc and WebKitGTK's headers.
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/bgmem162.XXXXXX); PORT=${BGMEM_PORT:-8712}; DSP=${BGMEM_DSP:-:92}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1; RUNS=${2:-1}
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
fi
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
wps() { ps -eo pid=,ppid=,comm= | awk -v r=$NP '{p[$1]=$2; c[$1]=$3} END {a[r]=1; do {m=0; for (k in p) if (!(k in a) && (p[k] in a)) {a[k]=1; m=1}} while (m); for (k in a) if (c[k] ~ /^WebKitWebProces/) print k}'; }
mem() { a=$(awk '/^RssAnon:/{print $2}' /proc/$1/status); awk -v a=$a '/^Private_(Clean|Dirty):/{p+=$2}/^Pss:/{s+=$2}END{printf "{\"rssanon\":%.1f,\"priv\":%.1f,\"pss\":%.1f}", a/1024, p/1024, s/1024}' /proc/$1/smaps_rollup; }
RN=0
while [ $RN -lt $RUNS ]; do RN=$((RN+1))
	D=$T/d$RN; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json >/dev/null || { echo "FAIL node"; exit 1; }
	PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
	printf '%s\n' "${BGMEM_SET:-{\"s\":{\"bs\":\"solid\",\"b1\":\"#112233\",\"tb\":\"bottom\",\"accent\":\"#2563eb\",\"h24\":true},\"pins\":[\"notepad\",\"calc\",\"settings\"],\"notes\":2,\"wallpaper\":false\}}" > $D/settings.json
	for START in first next; do
	python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
	rm -f $T/ev; mkfifo $T/ev; rm -f $D/state
	(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$T/ev exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
	exec 7<>$T/ev
	st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
	ev() { : > $T/ev.out; printf '%s %s\n' "$1" "$2" >&7; w8 "grep -q '^$1 ' $T/ev.out 2>/dev/null" 40; grep "^$1 " $T/ev.out | tail -1 | cut -d' ' -f2-; : > $T/ev.out; }
	w8 '[ "$(st signin)" = 1 ]' 160; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
	w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ] && [ "$(st portal | cut -d" " -f1)" = ready ]' 240
	timeout 30 tail -f /dev/null
	P=$(wps | head -1); N=$(wps | wc -l)
	X=$(ev do-bg "JSON.stringify({lean:window.__oaBgLean?__oaBgLean():null,nodes:document.getElementsByTagName('*').length${BGMEM_EVAL:+,x:($BGMEM_EVAL)}})")
	echo "{\"run\":$RN,\"start\":\"$START\",\"webprocs\":$N,\"mem\":$([ -n "$P" ] && mem $P || echo null),\"svc\":\"$(st svc)\",\"portal\":\"$(st portal | cut -d' ' -f1)\",\"page\":${X:-null}}"
	[ -n "$BGMEM_KEEP" ] && cp $D/apps.json $BGMEM_KEEP.$RN.$START.json 2>/dev/null
	kill $NP 2>/dev/null; wait $NP 2>/dev/null; pkill -f "$T/" 2>/dev/null; kill $RDP 2>/dev/null; wait $RDP 2>/dev/null; exec 7>&-; timeout 2 tail -f /dev/null
	done
done
