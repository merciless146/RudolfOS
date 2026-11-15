#!/bin/sh
# 1.6.0: the memory guard never ends the desktop's own page. On the Vivobook (2026-11-15) it ended a 765 MiB page
# process that was the desktop's: the file naming the desktop's process (/run/rudolf/desktop-renderer) was out of date,
# and the session then restarted the browser. Now the file counts only when it names a page process still running that
# started before it was written; else no page of the desktop's browser is ended. Stand-in Chromium processes (a
# program named chrome holding memory): a desktop browser (--rudolf-kiosk) with one page process, and a page process
# no browser holds.
#   sh memdesk60_t.sh   (needs gcc; node for the desktop's part)
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d)
gcc -O2 -o $T/mg $SYS/rudolf-memguard.c || { echo "FAIL build"; exit 1; }
cat > $T/stub.c <<'C'
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int c, char **v) { (void)c; char *e = getenv("MIB"); size_t n = (size_t)atoi(e ? e : "1") << 20; char *m = malloc(n); memset(m, 1, n);
	if (getenv("KID")) { char x[4096]; ssize_t l = readlink("/proc/self/exe", x, sizeof x - 1); if (l > 0 && fork() == 0) { x[l] = 0; setenv("MIB", getenv("KID"), 1); unsetenv("KID"); execl(x, "chrome", "--type=renderer", (char *)0); _exit(1); } }
	(void)v; sleep(600); return 0; }
C
gcc -O2 -o $T/chrome $T/stub.c || { echo "FAIL stub"; exit 1; }
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
trap 'kill $B $R2 $R1 2>/dev/null; rm -rf $T' EXIT
MIB=10 KID=100 $T/chrome --rudolf-kiosk --class=RudolfDesktop & B=$!
MIB=60 $T/chrome --type=renderer & R2=$!
sleep 1.5; R1=$(pgrep -P $B)
[ -n "$R1" ] || { echo "FAIL the stand-in desktop page did not start"; exit 1; }
run() { MG_TEST=1 MG_DESKFILE=$T/dr MG_MIN_MIB=40 MG_RUNUSER=$T/none MG_WINDOWS=$T/none $T/mg --once; }
O=$(run)
ok "no file: the desktop's own is not known, its browser's page (100 MiB) is kept and the other (60 MiB) would be ended" "$(echo "$O" | grep -q "^renderer $R1: .*not known: kept" && echo "$O" | grep -q "^would end: $R2 " && echo 1)" "$O"
echo $R1 > $T/dr; O=$(run)
ok "the file names the desktop's page: kept as its own, the other would be ended" "$(echo "$O" | grep -q "^renderer $R1: .*(the desktop's own: kept)" && echo "$O" | grep -q "^would end: $R2 " && echo 1)" "$O"
echo $R2 > $T/dr; O=$(run)
ok "the file names the other page: the desktop browser's page is then an ordinary one and the largest (would be ended)" "$(echo "$O" | grep -q "^would end: $R1 " && echo 1)" "$O"
sleep 600 & D=$!; kill $D; wait $D 2>/dev/null; echo $D > $T/dr; O=$(run)
ok "the file names a process that has gone (the desktop's page restarted): not known, the desktop's page kept" "$(echo "$O" | grep -q "^would end: $R2 " && echo "$O" | grep -q "^renderer $R1: .*not known" && echo 1)" "$O"
echo $R1 > $T/dr; touch -d '-1 hour' $T/dr; O=$(run)
ok "the file is older than the process it names (that number now belongs to another): not trusted" "$(echo "$O" | grep -q "^renderer $R1: .*not known: kept" && echo "$O" | grep -q "^would end: $R2 " && echo 1)" "$O"
rm -f $T/dr; kill $B; wait $B 2>/dev/null; sleep 0.5; O=$(run)
ok "no desktop browser (the native desktop): every page may be ended, the largest first" "$(echo "$O" | grep -q "^would end: $R1 " && echo 1)" "$O"
kill $R1 $R2 2>/dev/null
# the desktop: tries until it works, and again when the guard asks ("memory iam")
if command -v node >/dev/null; then
	r=$(node -e '
const fs=require("fs");const s=fs.readFileSync(process.argv[1],"utf8");
const a=s.indexOf("let memIamBusy"),b=s.indexOf("/* 1.5.8",a);let code=s.slice(a,b);
let posts=[],timers=[],hs=[],fail=4;global.RD=1;global.performance={now:(()=>{let t=0;return()=>t+=100})()};
global.rdPost=async u=>{posts.push(u);return u.includes("end")?{ok:fail--<=0}:{}};
global.setTimeout=(f,ms)=>{timers.push(ms);return timers.length};global.clearTimeout=()=>{};global.window={};
global.evOn=f=>hs.push(f);
eval(code+";global.memIam=memIam");
(async()=>{timers=[];for(let i=0;i<5;i++){const n=[0,1,2,3,4][i];await memIam(n)}
 const tries=timers.slice();posts=[];for(const h of hs)h("memory iam");await new Promise(r=>setImmediate(r));
 console.log(tries.join(",")+" "+posts.length)})()' "$HERE/../../../desktop/mem47.js")
	ok "the desktop tries again after 20 s, then each minute, then every 5 minutes, until it works" "$(echo "$r" | grep -q '^20000,60000,60000,300000 ' && echo 1)" "$r"
	ok "the guard's \"memory iam\" makes the desktop say again at once" "$(echo "$r" | grep -q ' 2$' && echo 1)" "$r"
else echo "SKIP the desktop's part (no node)"; fi
echo "$((N-F))/$N passed"; [ $F = 0 ]
