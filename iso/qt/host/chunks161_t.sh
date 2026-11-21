#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 23 B, stage 1): the page's big payloads are files beside it (www/a/, named by their contents), read only by
# the app that uses them: the Quake engine (WebAssembly) and the Galaxy Map's universe. rudolf-native under Xvfb with the stand-in rudolfd
# (fixtures/native_rd.py) serving this tree's page:
#   1. the page every window loads has neither inline (no QWASM_B64, no GM_UNI), and both files are there
#   2. a window that does not use them (Notepad) never asks for them; it opens and works (typing), no error in its page
#   3. the Galaxy Map in a native window of its own: its universe read from its file, drawn (its systems counted), no error
#   4. the Quake engine's window: the engine read from its file and checked (qSupport: none missing), no error
#   5. a missing file says so (the Quake engine "could not be loaded"), it does not fail quietly
# Needs Xvfb, xdotool, node, python3, gcc and WebKitGTK's headers.   sh chunks161_t.sh [RUDOLF-NATIVE]
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/chunks161.XXXXXX); PORT=${CHUNKS_PORT:-8623}; DSP=${CHUNKS_DSP:-:98}
command -v Xvfb >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs Xvfb and xdotool"; exit 0; }
BIN=$1
if [ -z "$BIN" ]; then BIN=$T/rudolf-native
	gcc -O2 -Wall -Wno-misleading-indentation -Wno-deprecated-declarations -o $BIN $HERE/../../sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
	grep -q warning $T/cc && { cat $T/cc; echo "FAIL build warnings"; exit 1; }
fi
D=$T/d; mkdir -p $D; node $HERE/fixtures/deskls.js $D/ls.json $D/keys.json || { echo "FAIL node"; exit 1; }
PW=$(node -e "console.log(require('$D/keys.json').Ann.pw)")
printf '%s\n' '{"s":{"bs":"solid","b1":"#112233","tb":"bottom","accent":"#2563eb","h24":true},"pins":["notepad","calc","settings"],"notes":2,"wallpaper":false}' > $D/settings.json
python3 $HERE/fixtures/native_rd.py $PORT $D > $T/rd.log 2>&1 & RDP=$!
Xvfb $DSP -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
export DISPLAY=$DSP LIBGL_ALWAYS_SOFTWARE=1
mkfifo $T/ev
trap 'kill $NP $RDP $XV 2>/dev/null; pkill -f "$T/" 2>/dev/null; rm -rf $T' EXIT
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
w8 'xdotool getmouselocation >/dev/null 2>&1' 40
(cd $D && HOME=$D RUDOLF_NATIVE_STATE=$D/state RUDOLF_NATIVE_TEST_EVAL=$T/ev exec $BIN --size=1280x800 --draw=soft --base=http://127.0.0.1:$PORT > $T/native.log 2>&1) & NP=$!
exec 7<>$T/ev
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
st() { grep "^$1 " $D/state 2>/dev/null | head -1 | cut -d' ' -f2-; }
click() { xdotool mousemove $1 $2 click 1; }
hit() { grep "^hit $1 $2 " $D/state | head -1 | awk '{print $4+int($6/2), $5+int($7/2)}'; }
cmact() { grep "^cmitem [0-9]* [0-9]* 0 $1|" $D/state | head -1 | awk '{print $2, $3}'; }
flyb() { grep "^flybtn $1 " $D/state | head -1 | awk '{print $3, $4}'; }
# (a page's answer: KEY JS, read back from the FIFO's .out; the hidden page by its name)
ev() { : > $T/ev.out; printf '%s %s\n' "$1" "$2" >&7; w8 "grep -q '^$1 ' $T/ev.out 2>/dev/null" 40; grep "^$1 " $T/ev.out | tail -1 | cut -d' ' -f2-; : > $T/ev.out; }
pages() { grep '^page ' $D/state | cut -d' ' -f2 | sort | tr '\n' ' '; }
wp() { ps -eo pid=,ppid=,comm= | awk -v r=$NP '{p[$1]=$2; c[$1]=$3} END {a[r]=1; do {m=0; for (k in p) if (!(k in a) && (p[k] in a)) {a[k]=1; m=1}} while (m); n=0; for (k in a) if (c[k] ~ /^WebKitWebProces/) n++; print n}'; }
opened() { grep -c "the shell's hidden page is opened" $T/native.log; }
others() { grep -c "is opened\|a page of its own" $T/native.log | head -1; }
dolog() { ev do-bg "JSON.stringify(OA.doLog||[])"; }
cl() { grep "^client .* $1\$" $D/state | head -1; }
res() { ev "$1" "performance.getEntriesByType('resource').map(e=>e.name).filter(n=>/\\/a\\/[0-9a-f]+\\.(wasm|gz)$/.test(n)).map(n=>n.split('/').pop()).join(',')||'-'"; }
errs() { grep -c "ReferenceError\|TypeError\|SyntaxError" $T/native.log; }
P=$HERE/../../www/index.html; QF=$(grep -o "const QWASM_URL='a/[0-9a-f]*\.wasm'" $P | cut -d"'" -f2); GF=$(grep -o "const GM_UNI_URL='a/[0-9a-f]*\.gz'" $P | cut -d"'" -f2)
ok "1. the page has neither payload inline; both are files beside it" "$(! grep -q 'QWASM_B64\|GM_UNI=' $P && [ -s "$HERE/../../www/$QF" ] && [ -s "$HERE/../../www/$GF" ] && echo 1)" "$QF $GF"
w8 '[ "$(st signin)" = 1 ]' 160; LC_ALL=C.UTF-8 xdotool type --delay 40 "$PW"; xdotool key Return
w8 '[ "$(st user)" = Ann ]' 120; w8 '[ "$(st svc | cut -d" " -f1)" = ready ]' 240; E0=$(errs)
click $(hit pin notepad); w8 'grep -q "^client .* app=notepad " $D/state' 120; timeout 2 tail -f /dev/null; xdotool type --delay 30 'chunks'; timeout 1 tail -f /dev/null
ok "2. Notepad's window: opens and works (typed), neither file asked for by any page" "$([ "$(ev notepad "document.querySelector('.oamain textarea').value")" = chunks ] && [ "$(res notepad)" = - ] && [ "$(res do-bg)" = - ] && echo 1)" "$(ev notepad "document.querySelector('.oamain textarea')&&document.querySelector('.oamain textarea').value"); notepad: $(res notepad); hidden: $(res do-bg)"
ev do-bg "oaPost({op:'open',app:'do-galaxy'});1" >/dev/null; w8 '[ -n "$(cl "Galaxy Map")" ]' 160; timeout 6 tail -f /dev/null
GK=$(grep '^app .* Galaxy Map$' $D/state | head -1 | cut -d' ' -f2); G=$(ev "$GK" "(document.querySelector('.gm-load')?'loading':'drawn')+'|'+(typeof GM_CACHE=='object'&&GM_CACHE?GM_CACHE.sys.size:0)")
ok "3. the Galaxy Map in a window of its own: its universe read from its file and drawn" "$(echo "$G" | grep -qE '^drawn\|[1-9][0-9]+$' && [ "$(res "$GK")" = "$(basename $GF)" ] && echo 1)" "$GK: $G; $(res "$GK"); $(grep '^app ' $D/state | tr '\n' ' ')"
ev do-bg "oaPost({op:'open',app:'do-quake'});1" >/dev/null; w8 '[ -n "$(cl "Quake Engine")" ]' 160; timeout 3 tail -f /dev/null
QK=$(grep '^app .* Quake Engine$' $D/state | head -1 | cut -d' ' -f2); Q=$(ev "$QK" "JSON.stringify([qSupport(),qwasmBytes().length])")
ok "4. the Quake engine's window: the engine read from its file and checked, nothing missing" "$(echo "$Q" | grep -qE '^\[\"\",[1-9][0-9]{5}\]$' && res "$QK" | grep -q "$(basename $QF)" && echo 1)" "$QK: $Q"
M=$(ev "$QK" "(()=>{const o=XMLHttpRequest.prototype.open;XMLHttpRequest.prototype.open=function(m,u,...r){return o.call(this,m,String(u).replace(/[0-9a-f]{16}\\.wasm$/,'0000000000000000.wasm'),...r)};try{QSUP=null;QWASM=null;return qSupport()}finally{XMLHttpRequest.prototype.open=o;QSUP=null;QWASM=null}})()")
ok "5. a missing file: the engine 'could not be loaded', said, not a quiet failure" "$(echo "$M" | grep -q 'could not be loaded' && echo 1)" "$M"
ok "6. no ReferenceError, TypeError or SyntaxError in any page's log" "$([ "$(errs)" = "$E0" ] && echo 1)" "$(grep 'ReferenceError\|TypeError\|SyntaxError' $T/native.log | tail -3)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
