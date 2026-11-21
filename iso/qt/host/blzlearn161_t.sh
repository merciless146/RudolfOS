#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 27 item 4): what Blitzen learns of a person is theirs. rudolfd's ai/know/... (compiled from rudolfd.c,
# fixtures/knowapi.c) with rudolf-know built here, on a real ext4 drive with the encrypt feature, as RUDOLFDATA has (homeapi_t.sh):
# "Learn from what I do" (off at first), the activity and the notes are kept only in the account's own encrypted folder
# (.rudolf/blitzen: its Linux user's, 0700; files 0600), only while it is open; nothing in /persist/os/know (the system's); 1.6.0's
# copy there is moved in and removed; never given to another account; unreadable once the folder is locked; turned off and
# forgotten; an account removed leaves nothing outside its folder; a note with a password is not kept. And the model engine's
# arguments (rudolf-ai engine-args): no --slot-save-path, nothing of a conversation saved. Needs root (a loop device and a private
# mount namespace).   sh blzlearn161_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$BLT_INNER" ] && exec env BLT_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys; T=$(mktemp -d /tmp/blzlearn.XXXXXX)
mount -t tmpfs none /run; mkdir -p /run/rudolf
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
# ---- the engine's arguments
gcc -O2 -w -o $T/rai $S/rudolf-ai.c -lcrypto -ldl || { echo "FAIL rudolf-ai did not build"; exit 1; }
A=$($T/rai engine-args | tr '\n' ' ')
ok "1. the model engine: no --slot-save-path (no conversation's state saved to disk), in its memory only" "$(! echo "$A" | grep -q 'slot' && echo "$A" | grep -q '^llama-server -m MODEL --host 127.0.0.1 ' && ! grep -q '"--slot-save-path"\|/slots' $S/rudolf-ai.c $S/rudolfd.c && echo 1)" "$A"
ok "2. ...rudolfd asks the engine only for answers (/v1/chat/completions), nothing that saves; the engine's folder emptied at its start" "$(grep -c 'POST /v1/' $S/rudolfd.c | grep -q '^1$' && grep -q 'cache_clear();' $S/rudolf-ai.c && echo 1)" "$(grep -n 'POST /v1/' $S/rudolfd.c)"
# ---- rudolfd and rudolf-know
gcc -O2 -w -o $T/rk $S/rudolf-know.c -lcrypto -ldl -lm || { echo "FAIL rudolf-know did not build"; exit 1; }
KD=$T/kd; export RUDOLF_KNOW_DIR=$KD RUDOLF_KNOW_LOGS=$T/logs; mkdir -p $T/logs $KD; chmod 700 $KD
gcc -O1 -w -DKNOW_BIN="\"$T/rk\"" -DKNOW_DIR="\"$KD\"" -o $T/t $HERE/fixtures/knowapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
truncate -s 96M $T/img; mkfs.ext4 -q -F -O encrypt $T/img; mkdir $T/m; mount -o loop $T/img $T/m || { echo "SKIP: no loop device"; exit 0; }
R=$T/m/accounts; t() { $T/t $R "$@"; }
K1=$(head -c 64 /dev/urandom | od -An -tx1 | tr -d ' \n'); K2=$(head -c 64 /dev/urandom | od -An -tx1 | tr -d ' \n'); printf %s "$K1" > $T/k1; printf %s "$K2" > $T/k2
t "POST home/unlock?acct=uAnnA1b2 <$T/k1" "POST home/unlock?acct=uBobC3d4 <$T/k2" >/dev/null
# 1.6.0's copy of Ann's activity, outside her folder (as 1.6.0 kept it)
mkdir -p $KD/acct; printf '1700000000\tlaunch\tOpened Steam (from 1.6.0)\n' > $KD/acct/uAnnA1b2.log; printf '1\n' > $KD/acct/uAnnA1b2.on
o=$(t "SESSION uAnnA1b2" "GET ai/know/learn")
AL=$R/uAnnA1b2/.rudolf/blitzen
ok "3. 1.6.0's copy outside the folder is moved into Ann's folder (her setting with it) and removed" "$([ "$(has "$o" '"learn":true')" = 1 ] && grep -q 'from 1.6.0' $AL/activity.log && [ -e $AL/learn ] && [ ! -e $KD/acct/uAnnA1b2.log ] && [ ! -e $KD/acct/uAnnA1b2.on ] && echo 1)" "$o"
printf 'launch\tOpened Notepad\n' > $T/a1; printf 'error\tSteam: could not start (exit code 1)\n' > $T/a2; printf 'settings\tChanged the Wi-Fi password to hunter22\n' > $T/a3
o=$(t "SESSION uAnnA1b2" "POST ai/know/activity <$T/a1" "POST ai/know/activity <$T/a2" "POST ai/know/activity <$T/a3" "GET ai/know/list")
ok "4. what Ann does is kept in her own folder (a line with a password is not)" "$([ "$(has "$o" 'Opened Notepad')" = 1 ] && [ "$(has "$o" 'Steam: could not start')" = 1 ] && ! grep -q hunter22 $AL/activity.log && [ "$(has "$o" hunter22)" = 0 ] && echo 1)" "$o"
U=$(stat -c %u $R/uAnnA1b2)
ok "5. ...readable only by her Linux user (the folder's owner) and rudolfd: .rudolf/blitzen 0700, the files 0600, hers" "$([ "$(stat -c '%a %u' $AL)" = "700 $U" ] && [ "$(stat -c '%a %u' $AL/activity.log)" = "600 $U" ] && [ "$(stat -c '%a %u' $AL/learn)" = "600 $U" ] && echo 1)" "$(stat -c '%n %a %u' $AL $AL/* 2>&1)"
ok "6. ...nothing of it in /persist/os/know (the system's: the source and the logs)" "$([ -z "$(ls -A $KD/acct 2>/dev/null)" ] && ! grep -rq 'Notepad\|Steam' $KD 2>/dev/null && echo 1)" "$(ls -la $KD $KD/acct 2>&1)"
printf 'I use the Native desktop\nmy password is hunter22\nI like short answers\n' > $T/n1
o=$(t "SESSION uAnnA1b2" "POST ai/know/notes?set=1 <$T/n1" "POST ai/know/notes")
ok "7. Ann's notes: in her folder (0600, hers); one with a password not kept; a read (POST, as the desktop asks) changes nothing" "$([ "$(has "$o" '["I use the Native desktop","I like short answers"]')" = 1 ] && [ "$(has "$o" 'was not kept')" = 1 ] && [ "$(stat -c '%a %u' $AL/notes.txt)" = "600 $U" ] && ! grep -q hunter22 $AL/notes.txt && echo 1)" "$o"
o=$(t "SESSION uAnnA1b2" "GET ai/know/search?q=why+won%27t+steam+start")
ok "8. Blitzen's look-up for Ann finds her activity" "$(has "$o" '"kind":"activity"')" "$o"
o=$(t "SESSION uBobC3d4" "GET ai/know/search?q=why+won%27t+steam+start" "GET ai/know/list" "POST ai/know/notes" "GET ai/know/learn" "POST ai/know/activity <$T/a1")
ok "9. never given to another account: Bob signed in finds nothing of Ann's (her activity, her notes), and learns nothing (off at first)" "$([ "$(has "$o" 'Steam')" = 0 ] && [ "$(has "$o" 'Native desktop')" = 0 ] && [ "$(has "$o" '"learn":false')" = 1 ] && [ "$(has "$o" 'not kept: learning is off')" = 1 ] && [ ! -e $R/uBobC3d4/.rudolf/blitzen/activity.log ] && echo 1)" "$o"
o=$(t "SESSION -" "GET ai/know/list" "POST ai/know/notes" "GET ai/know/search?q=steam")
ok "10. nobody signed in: nothing of anyone's" "$([ "$(has "$o" 'Steam')" = 0 ] && [ "$(has "$o" 'Native desktop')" = 0 ] && [ "$(has "$o" '409')" = 1 ] && echo 1)" "$o"
t "POST home/lock?acct=uAnnA1b2" >/dev/null
o=$(t "SESSION uAnnA1b2" "GET ai/know/learn" "POST ai/know/activity <$T/a1" "GET ai/know/list" "POST ai/know/notes" "POST ai/know/learn?on=1")
ok "11. Ann's folder locked (signed out): unreadable, also to root; learning off; nothing kept anywhere else meanwhile" "$([ "$(has "$o" '"learn":false')" = 1 ] && [ "$(has "$o" 'Opened Notepad')" = 0 ] && ! ls $AL >/dev/null 2>&1 && [ -z "$(ls -A $KD/acct 2>/dev/null)" ] && echo 1)" "$o; $(ls $R/uAnnA1b2 2>&1 | head -3)"
t "POST home/unlock?acct=uAnnA1b2 <$T/k1" >/dev/null
o=$(t "SESSION uAnnA1b2" "POST ai/know/learn?on=0" "POST ai/know/activity <$T/a1" "POST ai/know/forget?all=1" "GET ai/know/list")
ok "12. turned off: nothing more kept; Forget everything: the activity gone from her folder" "$([ "$(has "$o" '"learn":false')" = 1 ] && [ "$(has "$o" 'not kept: learning is off')" = 1 ] && [ ! -e $AL/learn ] && [ ! -e $AL/activity.log ] && echo 1)" "$o"
o=$(t "SESSION uAnnA1b2" "POST ai/know/learn?on=1" "POST ai/know/activity <$T/a1" "POST ai/know/activity <$T/a2" "GET ai/know/list" "POST ai/know/forget?item=1" "GET ai/know/list")
ok "13. on again (only by Ann's own choice): kept; one thing forgotten" "$([ "$(has "$o" '"learn":true')" = 1 ] && [ "$(t "SESSION uAnnA1b2" "GET ai/know/list" | grep -c 'Opened Notepad')" = 0 ] && [ "$(has "$(t "SESSION uAnnA1b2" "GET ai/know/list")" 'Steam')" = 1 ] && echo 1)" "$o"
mkdir -p $KD/acct; printf '1700000000\tlaunch\tleft over\n' > $KD/acct/uOldX0000.log; printf '1\n' > $KD/acct/uOldX0000.on
o=$(t "GONE uOldX0000"); ok "14. an account removed: nothing of it left outside its folder" "$([ ! -e $KD/acct/uOldX0000.log ] && [ ! -e $KD/acct/uOldX0000.on ] && grep -q 'know_acct_gone(e\[i\].id)' $S/rudolfd-users.inc && echo 1)" "$o"
o=$(t "SESSION uAnnA1b2" "POST ai/know/learn?on=1"); ok "15. off at first, and only Ann's own request turns it on (the account is rudolfd's record, never one the page names)" "$(! grep -q 'qparam(r->query, "acct")' $S/rudolfd-know.inc && grep -q 'slurp(HOME_SESSION' $S/rudolfd-know.inc && echo 1)" ""
umount $T/m 2>/dev/null; rm -rf $T
echo "$((N-F))/$N passed"; [ $F = 0 ]
