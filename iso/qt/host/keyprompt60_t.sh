#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md Part 14, third step): the keyring's prompts. gnome-keyring-daemon asks gcr's prompter
# (org.gnome.keyring.SystemPrompter) when an app creates a keyring or unlocks one; rudolf-portal answers in its place and
# the desktop draws the question. Here: gcr's secret exchange checked against another implementation of it
# (fixtures/sxpeer.py), then, with the real gnome-keyring-daemon on a private session bus that cannot start gcr's own
# prompter (no service files), an app (fixtures/krapp.py, python3-secretstorage) creating a keyring with a password,
# unlocking it (a wrong password first, then the right one), a cancelled unlock, a keyring with no password (the
# daemon's confirmation), and an app that goes away while asked (the question withdrawn); the desktop stood in for by
# fixtures/krdesk.py, which answers as rudolfd does. As root (CI: sudo), in /run/rudolf/portal. Kills only what it
# started (never by user: on GitHub's runner uid 1001 is the runner itself).   sh keyprompt60_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; KR=$SYS/root/usr/lib/rudolf/keyring; T=$(mktemp -d /tmp/kp60.XXXXXX)
trap 'kill $(cat $T/pids 2>/dev/null) 2>/dev/null; rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)$(why)"; F=$((F+1)); fi; }
# 2026-11-17 ("Before stable" 12): every wait is for the answer itself, up to a generous bound (as hwcnative160_t.sh's
# w8), never a fixed time. The occasional failure of 5, 6 or 7 (about 1 run in 25, here and on GitHub's runner, with or
# without a busy CPU) was not timing: gnome-keyring-daemon could not read the password rudolf-portal sent it, when their
# shared value began with a zero byte (1 in 256 exchanges; 1's sxpeer.py 12 sends one every time), and said so in its log
# ("Cannot import an invalid key"); a failing step now says it (why).
w8() { i=0; while [ $i -lt ${2:-120} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
why() { [ -f "$T/kr.log" ] && grep -q "Cannot import an invalid key" $T/kr.log && echo " / gnome-keyring-daemon: \"$(grep -m1 "Cannot import an invalid key" $T/kr.log | sed 's/.*WARNING \*\*: [0-9:.]*: //')\": it could not read the password sent (the secret exchange's shared value with a leading zero byte: sxpeer.py 12)"; }
PY=python3; [ -x /usr/bin/python3 ] && PY=/usr/bin/python3
gcc -O2 -w -o $T/rp $SYS/rudolf-portal.c $(pkg-config --cflags --libs gio-2.0 gio-unix-2.0) -lcrypto 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
# gcr's secret exchange
if $PY -c 'import cryptography' 2>/dev/null; then
	o=$($PY $HERE/fixtures/sxpeer.py $T/rp 2>&1); echo "$o" | sed 's/^/  sx: /'
	ok "1. gcr's secret exchange [sx-aes-1] as gcr and gnome-keyring-daemon do it (DH 1536, HKDF-SHA256, AES-128-CBC): $(echo "$o" | tail -1)" "$(echo "$o" | tail -1 | grep -q '^\([0-9]*\)/\1 passed$' && echo 1)" "$(echo "$o" | grep -v '^PASS')"
else echo "SKIP 1: no python3-cryptography"; fi
grep -q '"rudolf-portal".*-lcrypto\|rudolf-portal.c.*-lcrypto' $HERE/../../mkrootfs.sh && grep -q 'rudolf-portal.c.*-lcrypto' $HERE/../../systools/ci_release.sh
ok "2. built with OpenSSL's libcrypto, which the base layer already has (no new library)" "$([ $? = 0 ] && echo 1)"
grep -q 'O_CLOEXEC, 0600); .*ou = st.st_uid' $SYS/rudolfd.c
ok "3. rudolfd writes the desktop's answer (a password) for the prompt's user only (0600)" "$([ $? = 0 ] && echo 1)"
if ! command -v gnome-keyring-daemon >/dev/null || ! command -v dbus-daemon >/dev/null || ! $PY -c 'import secretstorage' 2>/dev/null || [ "$(id -u)" != 0 ]; then
	echo "SKIP 4-9: needs root, gnome-keyring-daemon, dbus-daemon and python3-secretstorage (CI installs them)"; echo "$((N-F))/$N passed"; [ $F = 0 ]; exit; fi
H=$T/home; RT=$T/rt; mkdir -p $H /run/rudolf/portal/req /run/rudolf/portal/resp; mkdir -m 700 $RT
# a session bus with no service files: nothing (gcr-prompter) is started for a name nobody has
cat > $T/bus.conf <<EOC
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig><type>session</type><keep_umask/><listen>unix:path=$RT/bus</listen><auth>EXTERNAL</auth>
<policy context="default"><allow send_destination="*" eavesdrop="true"/><allow eavesdrop="true"/><allow own="*"/></policy></busconfig>
EOC
dbus-daemon --config-file=$T/bus.conf --fork --nopidfile --print-pid > $T/dpid; cat $T/dpid >> $T/pids
S="env -i PATH=$PATH HOME=$H XDG_RUNTIME_DIR=$RT DBUS_SESSION_BUS_ADDRESS=unix:path=$RT/bus"
$S sh $KR start > $T/kr.log 2>&1 & echo $! >> $T/pids
w8 "$S $PY -c 'import secretstorage; secretstorage.get_default_collection(secretstorage.dbus_init())' >/dev/null 2>&1" 240
app() { $S timeout 30 $PY $HERE/fixtures/krapp.py "$@" 2>&1; }
o=$(app create Nobody)
ok "4. no prompter on the bus: creating a keyring fails (gcr's own is never started)" "$(echo "$o" | grep -q '^error\|^dismissed' && ! pgrep -f gcr-prompter >/dev/null && echo 1)" "$o"
$S $T/rp > $T/rp.log 2>&1 & echo $! >> $T/pids
w8 "$S gdbus call --session -d org.freedesktop.DBus -o /org/freedesktop/DBus -m org.freedesktop.DBus.NameHasOwner org.gnome.keyring.SystemPrompter 2>/dev/null | grep -q true" 240
: > $T/ans; : > $T/log; $PY $HERE/fixtures/krdesk.py $T/ans $T/log & echo $! >> $T/pids
q() { sed -n "${1}p" $T/log; }
printf 'password:work-pw-1\n' > $T/ans; o=$(app create Work)
ok "5. an app creates a keyring: the desktop is asked for its new password (who asked, the daemon's words), the keyring made with it" "$([ "$o" = created ] && q 1 | grep -q '"kind": "keyring", "type": "password"' && q 1 | grep -q '"newpass": true' && q 1 | grep -q '"proc": "python3"' && q 1 | grep -q '"message": "Choose password for new keyring"' && head -c 12 $H/.local/share/keyrings/Work.keyring | grep -q GnomeKeyring && ! grep -q work-pw-1 $H/.local/share/keyrings/Work.keyring && echo 1)" "$o / $(cat $T/log $T/rp.log)"
o=$(app store Work; app lock Work); printf 'password:wrong\npassword:work-pw-1\n' > $T/ans; o2=$(app unlock Work); o3=$(app read Work)
ok "6. unlocking it: a wrong password, asked again with the daemon's warning, then the right one: unlocked, its secret read" "$([ "$o2" = unlocked ] && [ "$o3" = "['work-token-7']" ] && q 2 | grep -q '"newpass": false' && q 3 | grep -q '"warning": "[^"]' && echo 1)" "$o / $o2 / $o3 / $(sed -n '2,3p' $T/log)"
app lock Work >/dev/null; printf 'cancel\n' > $T/ans; o=$(app unlock Work); o2=$(app read Work)
ok "7. Cancel: the app is told it was dismissed, the keyring stays locked" "$([ "$o" = dismissed ] && [ "$o2" = locked ] && echo 1)" "$o / $o2"
n=$(wc -l < $T/log); printf 'password:\nyes\n' > $T/ans; o=$(app create Open)
ok "8. a keyring with no password: the daemon's confirmation (kept unencrypted?) asked as a confirm, Continue: made" "$([ "$o" = created ] && sed -n "$((n+2))p" $T/log | grep -q '"type": "confirm"' && echo 1)" "$o / $(tail -2 $T/log)"
printf 'wait\n' > $T/ans; $S timeout 30 $PY $HERE/fixtures/krapp.py create Gone > /dev/null 2>&1 & AP=$!; echo $AP >> $T/pids
w8 'ls /run/rudolf/portal/req/k*.json >/dev/null 2>&1' 240; had=$(ls /run/rudolf/portal/req/k*.json 2>/dev/null)
kill $AP; w8 '! ls /run/rudolf/portal/req/k*.json >/dev/null 2>&1' 240
printf 'password:after\n' > $T/ans; o=$(app create After)
ok "9. the app goes away while asked: the question is withdrawn (the desktop closes it), and the next prompt still works" "$([ -n "$had" ] && ! ls /run/rudolf/portal/req/k*.json >/dev/null 2>&1 && [ "$o" = created ] && echo 1)" "$had / $o / $(cat $T/rp.log)"
rm -f /run/rudolf/portal/req/k*.json /run/rudolf/portal/resp/k*.json
echo "$((N-F))/$N passed"; [ $F = 0 ]
