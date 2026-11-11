#!/bin/sh
# 1.5.7 (plan stage 7): rudolf-memguard --apps lists the apps that run for Task Manager: Flatpak apps (all sandboxes as
# one), Windows programs in Wine and Linux programs in Debian (under a stand-in keeper), each with its memory and
# whether a window of it is open; --end asks one to quit and ends what is left 5 s later, its own processes only; an id
# that is not one of the listed apps is refused.   sh memapps57_t.sh   (needs gcc and python3)
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d); TREE=$HERE/fixtures/proctree.py
gcc -O2 -w -o $T/mg $SYS/rudolf-memguard.c || { echo "FAIL build"; exit 1; }
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
PIDS=""; trap 'for p in $PIDS; do kill -9 $p; done 2>/dev/null; pkill -9 -f "$TREE" 2>/dev/null; sleep 0.3; rm -rf $T' EXIT
alive() { [ -e /proc/$1/stat ] && ! grep -q ') Z' /proc/$1/stat; }   # (an ended child of a stand-in is a zombie until reaped)
PY=$(command -v python3); PY=$(readlink -f $PY)
tree() { PT_PY=$PY $PY $TREE "$1" >/dev/null 2>&1 & P=$!; PIDS="$PIDS $P"; }
# Flatpak: Discord (in the tray)
python3 -c "import time;a=bytearray(30<<20);time.sleep(600)" & D=$!; PIDS="$PIDS $D"
mkdir -p $T/run/1000/.flatpak/1; echo $D > $T/run/1000/.flatpak/1/pid; printf '[Application]\nname=com.discordapp.Discord\n' > $T/run/1000/.flatpak/1/info
# Debian: the keeper, Wine's own processes, a Windows game that starts a helper program, and a Linux program under a shell
tree '{"n":"debian-init","c":[
 {"n":"wineserver","m":5},
 {"n":"services.exe","a":"C:\\windows\\system32\\services.exe","c":[{"n":"winedevice.exe","a":"C:\\windows\\system32\\winedevice.exe"}]},
 {"n":"StarportGE.exe","a":"C:\\users\\rudolf\\StarportGE\\StarportGE.exe","m":40,"c":[{"n":"crashhandler.ex","a":"C:\\users\\rudolf\\StarportGE\\crashhandler.exe","m":10}]},
 {"n":"AVeryLongGameNa","a":"Z:\\games\\AVeryLongGameName.exe","m":5,"stubborn":1},
 {"n":"sh","c":[{"n":"gimp","m":20,"c":[{"n":"script-fu","m":5}]}]}]}'
K=$P; sleep 2
printf '0x1\t0\tDiscord\tdiscord\t0\tDiscord\n0x2\t0\tstarportge.exe\tstarportge.exe\t1\tStarport GE\n' > $T/windows
run() { MG_TEST=1 MG_RUNUSER=$T/run MG_WINDOWS=$T/windows MG_KEEPER=$K $T/mg "$@"; }
O=$(run --apps)
J() { echo "$O" | python3 -c "import json,sys;j=json.load(sys.stdin);a={x['name']:x for x in j['apps']};print($1)"; }
ok "1. the list is JSON with every app: Discord (Flatpak), StarportGE.exe and AVeryLongGameName.exe (Wine), gimp (Debian)" "$(J "sorted(a)==['AVeryLongGameName.exe','Discord','StarportGE.exe','gimp'] and 1 or 0")" "$O"
ok "2. ...each kind, and Wine's own processes (wineserver, services.exe) are not apps" "$(J "1 if a['Discord']['kind']=='flatpak' and a['StarportGE.exe']['kind']=='wine' and a['gimp']['kind']=='debian' and a['Discord']['id']=='flatpak:com.discordapp.Discord' else 0")" "$O"
ok "3. a program counts with what it started (the game's crash handler; gimp's script-fu)" "$(J "1 if a['StarportGE.exe']['rss']>=55<<20 and a['gimp']['rss']>=30<<20 else 0")" "$O"
ok "4. windows: Discord's and the game's are open, gimp has none" "$(J "1 if a['Discord']['window'] and a['StarportGE.exe']['window'] and not a['gimp']['window'] else 0")" "$O"
ok "4b. 1.5.7: each app lists its windows by id (a window's Force quit finds its app by them)" "$(J "1 if a['Discord']['wins']==['0x1'] and a['StarportGE.exe']['wins']==['0x2'] and a['gimp']['wins']==[] else 0")" "$O"
GID=$(J "a['gimp']['id']"); SID=$(J "a['StarportGE.exe']['id']"); LID=$(J "a['AVeryLongGameName.exe']['id']")
ok "5. an id that is not a listed app is refused (the keeper, wineserver, a pid passed as a game)" "$(run --end debian:$K | grep -q '"ok":false' && run --end wine:${GID#debian:} | grep -q '"ok":false' && run --end flatpak:org.none | grep -q '"ok":false' && kill -0 $K && echo 1)" ""
o=$(run --end $GID); sleep 0.3
ok "6. End gimp: gimp and script-fu end, nothing else" "$(echo "$o" | grep -q '"ok":true,"processes":2' && ! alive ${GID#debian:} && run --apps | grep -q StarportGE && kill -0 $K && echo 1)" "$o"
t0=$(date +%s); o=$(run --end $SID --force); t1=$(date +%s); sleep 0.3
ok "6b. 1.5.7: Force quit (--force): the game and its crash handler end at once, without being asked to quit" "$(echo "$o" | grep -q '"forced":true' && [ $((t1-t0)) -le 1 ] && ! alive ${SID#wine:} && run --apps | grep -q AVeryLong && kill -0 $K && echo 1)" "$o"
t0=$(date +%s); o=$(run --end $LID); t1=$(date +%s)
ok "7. a program that does not quit when asked is ended 5 s later" "$(echo "$o" | grep -q '"forced":true' && [ $((t1-t0)) -ge 4 ] && ! alive ${LID#wine:} && echo 1)" "$o"
o=$(run --end flatpak:com.discordapp.Discord); sleep 0.3
ok "8. End Discord (Flatpak): its sandbox ends" "$(echo "$o" | grep -q '"ok":true' && ! alive $D && echo 1)" "$o"
printf '' > $T/windows; python3 -c "import time;a=bytearray(60<<20);time.sleep(600)" & D2=$!; PIDS="$PIDS $D2"; echo $D2 > $T/run/1000/.flatpak/1/pid; sleep 1
o=$(MG_RUNUSER=$T/run MG_WINDOWS=$T/windows MG_MIN_MIB=40 MG_TEST=1 MG_MEMCONF=$T/none $T/mg --once)
ok "8b. with no setting, an app with no window open would be quit when memory runs out" "$(echo "$o" | grep -q '^would quit app: Discord' && echo 1)" "$o"
printf 'zram=auto\nclose_apps=0\n' > $T/memconf
o=$(MG_RUNUSER=$T/run MG_WINDOWS=$T/windows MG_MIN_MIB=40 MG_TEST=1 MG_MEMCONF=$T/memconf $T/mg --once)
ok "8c. with \"Close apps with no window open\" off (close_apps=0), none is" "$(echo "$o" | grep -q '^would quit app: none (closing apps is off)' && echo 1)" "$o"
ok "9. the memory guard says to the desktop when it quits an app (memory closed NAME MB in /run/rudolf/events)" "$(grep -q 'memory closed %s %lld' $SYS/rudolf-memguard.c && echo 1)" ""
echo "$((N-F))/$N passed"; [ $F = 0 ]
