#!/bin/sh
# 1.5.9 (plan 15.3): each account is its own Linux user. rudolfd's side, compiled from rudolfd.c (fixtures/usersapi.c),
# with accounts made as the page makes them (fixtures/deskls.js), on an ext4 drive as RUDOLFDATA is: a user per
# account (uid 1001 and up) made and removed with it, never a uid given twice; the move of existing accounts' folders
# (ownership only, and what cannot move named); one account's user cannot read another's files; the session at
# sign-in (its home, its runtime folder, the apps as its user) and every program of it stopped at sign-out.
# Needs root (a loop device, a private mount namespace), node and setpriv.   sh users59_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$US59_INNER" ] && exec env US59_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/users59.XXXXXX); chmod 755 $T
mount -t tmpfs none /run; mkdir -p /run/rudolf
# (1.6.0 Part 14: the keyring's stand-in, which says who started it, with which session, and stays until stopped)
printf '#!/bin/sh\necho "$(id -u) $HOME $XDG_CURRENT_DESKTOP ${DBUS_SESSION_BUS_ADDRESS:+bus} $*" >> %s/krlog\nexec sleep 300\n' $T > $T/kr; chmod 755 $T/kr; : > $T/krlog; chmod 666 $T/krlog
# (and rudolf-portal's: --keyring-list and --keyring-forget, saying who ran it)
printf '#!/bin/sh\nif [ "$1" = --keyring-list ]; then echo "{\\"ok\\":true,\\"keyring\\":true,\\"items\\":[{\\"id\\":\\"app:org.x\\",\\"label\\":\\"$(id -u) $HOME\\"}]}"; else echo "$(id -u) forget $2" >> %s/krlog; echo "{\\"ok\\":true}"; fi\n' $T > $T/rp; chmod 755 $T/rp
# 1.6.1 (docs/plan-1.6.1.md Part 11, accounts): US_VIA=helper: the users made, removed and moved, and the keyring, by the
# accounts helper (built with -DRR_TEST, its places in its configuration), as rudolfd does on the system: every check the same
RDIR=; [ "$US_VIA" = helper ] && { RDIR="-DRD_ROOT_DIR=\"$T/root\""; mkdir -p $T/root; echo "(through the accounts helper)"; }
gcc -O1 -w $RDIR -DAU_KEYRING="\"$T/kr\"" -DAU_KEYRING_DAEMON="\"$T/kr\"" -DAU_PORTAL="\"$T/rp\"" -o $T/t $HERE/fixtures/usersapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
truncate -s 64M $T/img; mkfs.ext4 -q -F $T/img; mkdir $T/m; mount -o loop $T/img $T/m || { echo "SKIP: no loop device"; exit 0; }
# (the users must be real for setpriv: this namespace's /etc is a copy, which rudolfd writes as it writes the system's)
mkdir $T/etc; cp -a /etc/. $T/etc/; mount --bind $T/etc /etc
R=$T/m/accounts; E=/etc; MAP=$T/m/etc/account-users; HM=$T/home; RU=$T/runuser; mkdir -p $R $HM $T/m/etc
[ -e $E/shadow ] || : > $E/shadow
grep -q '^audio:' $E/group || echo 'audio:x:29:' >> $E/group; grep -q '^video:' $E/group || echo 'video:x:44:' >> $E/group
grep -q '^input:' $E/group || echo 'input:x:101:' >> $E/group; grep -q '^rudolf:' $E/group || echo 'rudolf:x:1000:' >> $E/group
N=0; F=0; ok() { N=$((N+1)); case "$2" in *[!1]*|'') r=0;; *) r=1;; esac; if [ $r = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
printf '#!/bin/sh\necho "$*" >> %s/xlog\n' $T > $T/rdisp; chmod 755 $T/rdisp; export USERS_DISPLAY=$T/rdisp   # (rudolf-display's stand-in)
t() { $T/t $T/ls.json $R $E $MAP $HM $RU "$@"; }
if [ "$US_VIA" = helper ]; then
	printf 'RR_TEST_DIR=%s\nRR_TEST_HOMEROOT=%s\nRR_TEST_AUMAP=%s\nRR_TEST_ETC=%s\nRR_TEST_AUHOMES=%s\nRR_TEST_RUNUSER=%s\nRR_TEST_DISPLAY=%s\nRR_TEST_NOBIND=0\n' $T/root $R $MAP $E $HM $RU $USERS_DISPLAY > $T/rr.conf
	gcc -O1 -w -DRR_TEST -DRR_TEST_CONF="\"$T/rr.conf\"" -DAU_KEYRING="\"$T/kr\"" -DAU_KEYRING_DAEMON="\"$T/kr\"" -DAU_PORTAL="\"$T/rp\"" -o $T/root/accounts $HERE/../../sys/rudolf-accounts.c -lz || { echo "FAIL build the helper"; exit 1; }
	chmod 700 $T/root/accounts
fi
as() { u=$1; shift; setpriv --reuid=$u --regid=$u --init-groups "$@" 2>&1; }
H=/run/rudolf/health.log; [ -e /run/rudolf/persistent ] || H=/run/rudolf/health.log
# an existing computer: two accounts' folders on the disk, owned by the desktop's user as before 1.5.9
for a in uAnnA1b2 uBobC3d4; do mkdir -p $R/$a/Documents $R/$a/.config; echo "secret of $a" > $R/$a/Documents/diary.txt; echo cfg > $R/$a/.config/app.ini; chown -R 1000:1000 $R/$a; chmod 700 $R/$a; done
echo locked > $R/uBobC3d4/Documents/stuck.txt; chown 1000:1000 $R/uBobC3d4/Documents/stuck.txt; chattr +i $R/uBobC3d4/Documents/stuck.txt
o=$(t sync)
ok "1. a Linux user for each account (not the guest), uid 1001 and up, in the order of the accounts" "$(has() { grep -q "$1" $E/passwd && echo 1; }; has '^acct1001:x:1001:1001:Rudolf OS account uAnnA1b2:'$HM'/acct1001:/bin/sh')$(grep -c '^acct100[1-4]:' $E/passwd | grep -qx 4 && echo 1)" "$(grep acct $E/passwd)"
ok "...each with its own group, in audio, video, input and the devices' group" "$(grep -q '^acct1002:x:1002:$' $E/group && grep '^audio:' $E/group | grep -q 'acct1001,acct1002\|acct1002' && grep '^input:' $E/group | grep -q acct1003 && grep '^rudolf:' $E/group | grep -q acct1004 && echo 1)" "$(cat $E/group | tail -8)"
ok "...no password of its own (nobody signs in to it: the desktop's sign-in is the account's)" "$(grep -q '^acct1001:\*:' $E/shadow && echo 1)" "$(grep acct $E/shadow)"
o=$(t sync); ok "2. again: nothing changes (the same uids)" "$([ "$o" = 'sync 0' ] && [ "$(wc -l < $MAP)" = 4 ] && echo 1)" "$o $(cat $MAP)"
t start >/dev/null
ok "3. the move at the first start: each account's folder and files belong to its own user (ownership only), 0700" "$([ "$(stat -c %u:%a $R/uAnnA1b2)" = 1001:700 ] && [ "$(stat -c %u:%g $R/uAnnA1b2/Documents/diary.txt)" = 1001:1001 ] && [ "$(stat -c %u $R/uAnnA1b2/.config/app.ini)" = 1001 ] && [ "$(cat $R/uAnnA1b2/Documents/diary.txt)" = 'secret of uAnnA1b2' ] && echo 1)" "$(ls -laR $R/uAnnA1b2 | head -20)"
ok "...logged" "$(grep -q "uAnnA1b2's files belong to its own Linux user (acct1001) now: 5 moved" $H && echo 1)" "$(cat $H)"
ok "...a file that cannot be moved (here: immutable) is named in the health log and for System health, and its account is tried again next start" "$(grep -q 'files of the account uBobC3d4 could not all be given to its Linux user (acct1002)' $H && [ "$(cat /run/rudolf/account-move-failed)" = uBobC3d4 ] && grep -q '^uAnnA1b2 1001 moved$' $MAP && grep -q '^uBobC3d4 1002$' $MAP && echo 1)" "$(cat $MAP) $(tail -2 $H)"
chattr -i $R/uBobC3d4/Documents/stuck.txt; t start >/dev/null
ok "...the next start finishes it" "$(grep -q '^uBobC3d4 1002 moved$' $MAP && [ "$(stat -c %u $R/uBobC3d4/Documents/stuck.txt)" = 1002 ] && echo 1)" "$(cat $MAP)"
o=$(as 1002 cat $R/uAnnA1b2/Documents/diary.txt); o2=$(as 1002 ls $R/uAnnA1b2)
ok "4. one account's user cannot read another's files (Bob's programs, Ann's diary)" "$(echo "$o" | grep -q 'Permission denied' && echo "$o2" | grep -q 'Permission denied' && echo 1)" "$o $o2"
o=$(as 1001 cat $R/uAnnA1b2/Documents/diary.txt); ok "...its own, it can" "$([ "$o" = 'secret of uAnnA1b2' ] && echo 1)" "$o"
o=$(as 1000 cat $R/uAnnA1b2/Documents/diary.txt); ok "...nor can the desktop's user (rudolf) go into an account's folder directly (rudolfd serves the desktop)" "$(echo "$o" | grep -q 'Permission denied' && echo 1)" "$o"
o=$(t appuser); ok "5. nobody signed in: Linux apps run as the desktop's user, as before" "$([ "$o" = 'rudolf /run/rudolf/session.env' ] && echo 1)" "$o"
o=$(t session-start uAnnA1b2 appuser)
ok "6. signed in (files on the disk): the account's session, its apps as its own user" "$(echo "$o" | grep -q 'session-start 0' && echo "$o" | grep -q '^acct1001 /run/rudolf/acct-session.env$' && echo 1)" "$o"
ok "...its home is its folder (/home/acct1001), its runtime folder its own (0700)" "$([ "$(stat -c %u $HM/acct1001/Documents/diary.txt)" = 1001 ] && [ "$(stat -c %u:%a $RU/1001)" = 1001:700 ] && grep -q "^HOME=$HM/acct1001$" /run/rudolf/acct-session.env && grep -q "^XDG_RUNTIME_DIR=$RU/1001$" /run/rudolf/acct-session.env && grep -q '^PULSE_SERVER=unix:/run/rudolf/pulse/native$' /run/rudolf/acct-session.env && echo 1)" "$(cat /run/rudolf/acct-session.env)"
ok "...the display is given to its user (rudolf-display --xallow acct1001; X has authorization since 15.3's second step)" "$(grep -qx -- '--xallow acct1001' $T/xlog && echo 1)" "$(cat $T/xlog 2>&1)"
timeout 1 tail -f /dev/null
ok "6c. (1.6.0 Part 14) its keyring started as its user, in its session (its home, its bus), the desktop named GNOME's too for Electron apps" "$(grep -qx "1001 $HM/acct1001 Rudolf:GNOME bus start" $T/krlog && grep -qx 'XDG_CURRENT_DESKTOP=Rudolf:GNOME' /run/rudolf/acct-session.env && echo 1)" "$(cat $T/krlog 2>&1; cat /run/rudolf/acct-session.env)"
KRP=$(pgrep -u 1001 -f "sleep 300" | head -1)
o=$(t keyring-list); ok "6d. Saved passwords for apps (GET keyring/list): the account's, read as its user in its session" "$(echo "$o" | grep -q "^{\"ok\":true,\"signedIn\":true,\"keyring\":true,\"items\":\[{\"id\":\"app:org.x\",\"label\":\"1001 $HM/acct1001\"}\]}$" && echo 1)" "$o"
o=$(t keyring-forget app:org.x keyring-forget "/org/freedesktop/secrets/collection/login/3" keyring-forget "app:x';id;'" keyring-forget /etc/passwd)
ok "...POST keyring/forget: one removed, as the account's user; anything else refused" "$(grep -qx '1001 forget app:org.x' $T/krlog && grep -qx '1001 forget /org/freedesktop/secrets/collection/login/3' $T/krlog && [ "$(echo "$o" | grep -c '"ok":true')" = 2 ] && [ "$(echo "$o" | grep -c 'one of the list')" = 2 ] && [ "$(grep -c forget $T/krlog)" = 2 ] && echo 1)" "$o $(cat $T/krlog)"
o=$(t homehas $HM/acct1001/.wine/drive_c/Program\ Files /.wine/drive_c/ homehas /home/rudolf/.wine/drive_c/x /.wine/drive_c/)
ok "...Wine's prefix and Wine's icons are looked for in its home (Debian's home is the account's folder)" "$(echo "$o" | head -1 | grep -qx 'homehas 1' && echo "$o" | tail -1 | grep -qx 'homehas 0' && echo 1)" "$o"
setpriv --reuid=1001 --regid=1001 --init-groups sleep 300 & P1=$!; setpriv --reuid=1001 --regid=1001 --init-groups sh -c 'trap "" TERM; while :; do sleep 1; done' & P2=$!
setpriv --reuid=1002 --regid=1002 --init-groups sleep 300 & P3=$!; timeout 1 tail -f /dev/null
o=$(t rebind uAnnA1b2); timeout 0.5 tail -f /dev/null
ok "6b. rudolfd started again while the account is signed in: its programs go on, its session as it was" "$(echo "$o" | grep -q 'rebind 0' && kill -0 $P1 2>/dev/null && [ -e /run/rudolf/acct-session.env ] && [ "$(grep -c "$HM/acct1001" /proc/self/mountinfo)" = 1 ] && echo 1)" "$o $(grep acct1001 /proc/self/mountinfo)"
o=$(t session-stop uAnnA1b2 appuser); timeout 1 tail -f /dev/null
ok "7. signing out stops every program of the account's user (one that ignores TERM too)" "$(! kill -0 $P1 2>/dev/null && ! kill -0 $P2 2>/dev/null && echo 1)" "$(ps -o pid,user,cmd -p $P1,$P2)"
ok "...not another account's" "$(kill -0 $P3 2>/dev/null && echo 1)"; kill $P3 2>/dev/null
ok "...its keyring stopped with it (shut away until the next sign-in)" "$([ -n "$KRP" ] && ! kill -0 $KRP 2>/dev/null && echo 1)" "KRP=$KRP $(ps -u 1001 -o pid,cmd 2>&1)"
ok "...the display taken back from its user (--xdeny acct1001)" "$(grep -qx -- '--xdeny acct1001' $T/xlog && echo 1)" "$(cat $T/xlog)"
ok "...its home and runtime folder gone, and the apps back to the desktop's user" "$(! mountpoint -q $HM/acct1001 && [ ! -e $RU/1001 ] && [ ! -e /run/rudolf/acct-session.env ] && echo "$o" | grep -q '^rudolf /run/rudolf/session.env$' && echo 1)" "$o"
o=$(t keyring-list); ok "...nobody signed in: nothing to list" "$(echo "$o" | grep -q '"signedIn":false' && echo 1)" "$o"
o=$(t users); ok "8. GET accounts/users: each account's user, moved or not, who is signed in" "$(echo "$o" | grep -q '"id":"uAnnA1b2","uid":1001,"user":"acct1001","moved":true,"signedIn":false' && echo 1)" "$o"
node -e "const f='$T/ls.json',j=require(f),u=JSON.parse(j.data.bd95user);u.accts=u.accts.filter(a=>a.n!='Old');u.accts.push({n:'New',h:'p2\$x\$y',r:'user',id:'uNew00000'});j.data.bd95user=JSON.stringify(u);require('fs').writeFileSync(f,JSON.stringify(j))"
setpriv --reuid=1003 --regid=1003 --init-groups sleep 300 & P4=$!; timeout 0.5 tail -f /dev/null
o=$(t sync)
ok "9. an account removed: its user stops and is removed, and its uid is never given again (the new account gets 1005)" "$(! grep -q '^acct1003:' $E/passwd && ! kill -0 $P4 2>/dev/null && grep -q '^-uOld00000 1003$' $MAP && grep -q '^uNew00000 1005' $MAP && grep -q '^acct1005:' $E/passwd && ! grep '^audio:' $E/group | grep -q acct1003 && echo 1)" "$(cat $MAP) $(grep acct $E/passwd)"
ok "...said in the health log" "$(grep -q 'uOld00000 was removed, and its Linux user (acct1003) with it' $H && grep -q 'uNew00000 has its own Linux user (acct1005)' $H && echo 1)" "$(tail -3 $H)"
if [ "$US_VIA" = helper ]; then
	# (the checks above went through the helper: a stand-in for it says it was asked, and with what; with nothing to
	# change it is not asked at all)
	mv $T/root/accounts $T/accounts.real; printf '#!/bin/sh\necho "$1 $(cat | tr "\\n" " ")" >> %s/acalls\nexit 0\n' $T > $T/root/accounts; chmod 700 $T/root/accounts
	t sync >/dev/null; n0=$(cat $T/acalls 2>/dev/null | wc -l); cp $MAP $T/map.bak; sed -i '$d' $MAP; t sync >/dev/null; t start >/dev/null; cp $T/map.bak $MAP
	ok "10. (through the helper) asked only when an account came or went, with the page's accounts' ids; and at the start" "$([ "$n0" = 0 ] && grep -q '^sync uAnnA1b2 ' $T/acalls && grep -q '^start uAnnA1b2 ' $T/acalls && echo 1)" "$n0 $(cat $T/acalls 2>&1)"
	mv $T/accounts.real $T/root/accounts
	b=0; for x in '../x' 'guest' 'shared' 'a b' "$(printf 'u%.0s' $(seq 81))" ''; do printf '%s\n' "$x" | $T/root/accounts sync >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: $x)"; }; done
	printf 'uAnnA1b2' | $T/root/accounts sync >/dev/null 2>&1; [ $? = 2 ] || b=1
	$T/root/accounts keyring-forget '../../etc' </dev/null >/dev/null 2>&1; [ $? = 2 ] || b=1; $T/root/accounts frob </dev/null >/dev/null 2>&1; [ $? = 2 ] || b=1
	ok "11. (the helper) a list with an id that is not one (.., the guest, shared, a space, too long, empty, no last newline), a saved password's id that is not one, an unknown action: refused, nothing changed" "$([ $b = 0 ] && cmp -s $MAP $T/map.bak && echo 1)" "$b"
fi
umount /etc; umount $T/m; rm -rf $T
echo "$((N-F))/$N passed"; [ $F = 0 ]
