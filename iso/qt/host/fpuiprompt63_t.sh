#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 2's last step): the native apps' administrator prompt takes a finger -- librudolf-ui
# (iso/sys/ui/rudolf-ui.c, fp_*), as the page's prompt does (core25.js elevate's __elevOpened, fprint63.js's fpLoop). On a
# virtual screen (Xvfb) with the stand-in rudolfd (fixtures/fpui_rd.py) answering fp/verify as rudolfd-fprint.inc does
# (a finger not recognised with the tries left, then the right one with its one-time answer "t", taken once):
#   - the prompt offers the reader ("Or Ann puts a finger on the reader.");
#   - a finger not recognised is said with the tries left, and the reader asked again;
#   - a match answers the prompt: "fp:" and its one-time answer where the password goes, in the account signed in's name,
#     and rudolfd's own check (desktop/admin) takes it once and only for that name;
#   - the password still answers the prompt while a finger waits, and the reader is given up (fp/cancel) when the dialog
#     goes.
#   sh fpuiprompt63_t.sh        (Xvfb, gcc, GTK 3's headers, python3, curl)
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d /tmp/fpui63.XXXXXX)
PORT=${FPUI_PORT:-8733}; DSP=${FPUI_DSP:-:86}; TICKET=0123456789abcdef0123456789abcdef
command -v Xvfb >/dev/null || { echo "SKIP: needs Xvfb"; exit 0; }
W=$T/w; D=$T/d; mkdir -p "$W" "$D"
python3 "$R/iso/sys/ui/mkui.py" "$W/rui-gen.h" || { echo "FAIL mkui.py"; exit 1; }
gcc -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare -Wno-misleading-indentation -Wno-deprecated-declarations \
	-o "$T/fpui" "$HERE/fpuiprompt63_t.c" "$R/iso/sys/ui/rudolf-ui.c" -I"$W" -I"$R/iso/sys/ui" \
	$(pkg-config --cflags --libs gtk+-3.0 fontconfig) -lX11 -lm 2>"$T/cc" || { cat "$T/cc"; echo "FAIL build"; exit 1; }
[ -s "$T/cc" ] && { echo "FAIL the prompt's test builds with warnings:"; cat "$T/cc"; exit 1; }
Xvfb "$DSP" -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XV=$!
RDP=""
trap 'kill $RDP $XV 2>/dev/null; rm -rf "$T"' EXIT
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi }
up() { i=0; while [ $i -lt 80 ]; do curl -s -o /dev/null --max-time 1 "http://127.0.0.1:$PORT/api/fp/status?acct=ann01" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
run() { ( cd "$T" && DISPLAY=$DSP RUI_BASE=http://127.0.0.1:$PORT FPUI_TICKET=$TICKET exec timeout 40 "$T/fpui" "$1" ) >"$T/$1.out" 2>"$T/$1.err"; echo $?; }

# (1) a finger answers the prompt
: > "$D/calls"
python3 "$HERE/fixtures/fpui_rd.py" "$PORT" "$D" match >"$T/rd1.log" 2>&1 & RDP=$!
up || { echo "FAIL the stand-in rudolfd did not start"; exit 1; }
: > "$D/calls"
r=$(run finger); cat "$T/finger.out"
ok "1. the prompt offers the reader, says a finger was not recognised with the tries left, and the right one answers it" \
	"$([ "$r" = 0 ] && ! grep -q '^FAIL' "$T/finger.out" && echo 1)" "$(head -c 700 "$T/finger.out" "$T/finger.err" 2>/dev/null)"
ok "2. ...asking the reader again after the finger that was not recognised" "$([ $(grep -c '^verify ' "$D/calls") -ge 2 ] && echo 1)" "$(cat "$D/calls")"
ok "3. ...the one-time answer in Ann's name, where a password goes" "$(grep -q "^admin Ann fp:$TICKET$" "$D/calls" && echo 1)" "$(cat "$D/calls")"
kill $RDP 2>/dev/null; wait $RDP 2>/dev/null; RDP=""
sleep 1

# (2) the password still answers it, and the reader is given up when the dialog goes
: > "$D/calls"
python3 "$HERE/fixtures/fpui_rd.py" "$PORT" "$D" hold >"$T/rd2.log" 2>&1 & RDP=$!
up || { echo "FAIL the stand-in rudolfd did not start (2)"; exit 1; }
: > "$D/calls"
r=$(run password); cat "$T/password.out"
ok "4. the password still answers the prompt while a finger waits" "$([ "$r" = 0 ] && ! grep -q '^FAIL' "$T/password.out" && echo 1)" "$(head -c 700 "$T/password.out")"
ok "5. ...and the reader is given up when the dialog goes (fp/cancel)" "$(grep -q '^cancel$' "$D/calls" && echo 1)" "$(cat "$D/calls")"
echo "$((N-F))/$N passed"
[ "$F" = 0 ]
