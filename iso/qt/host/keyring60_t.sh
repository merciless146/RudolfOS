#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md Part 14): the keyring for Linux apps. /usr/lib/rudolf/keyring as rudolfd starts it at
# sign-in (as the account's user, with its session): the account's login keyring made once in its own folder, with no
# password of its own (the folder is encrypted with the account's key and only its user's), never one already there
# changed; gnome-keyring-daemon's secrets part only. With gnome-keyring-daemon and secret-tool (CI: apt-get install
# gnome-keyring libsecret-tools dbus): a secret kept by an app (secret-tool, libsecret, as the Claude app and Electron
# apps keep their sign-ins) is found again after the daemon starts again (a restart) without any prompt, and another
# user cannot read the keyring.   sh keyring60_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); KR=$HERE/../../sys/root/usr/lib/rudolf/keyring; T=$(mktemp -d /tmp/kr60.XXXXXX)
trap 'kill $(cat $T/pids 2>/dev/null) 2>/dev/null; rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
printf '#!/bin/sh\necho "$*" > %s/args\n' $T > $T/daemon; chmod 755 $T/daemon
mkdir -p $T/home
o=$(env -i HOME=$T/home DBUS_SESSION_BUS_ADDRESS=unix:path=$T/none KEYRING_DAEMON=$T/daemon sh $KR start 2>&1)
K=$T/home/.local/share/keyrings
ok "1. the account's login keyring made in its own folder, its user's only (0700, 0600), the default" "$([ "$(stat -c %a $K)" = 700 ] && [ "$(stat -c %a $K/login.keyring)" = 600 ] && head -1 $K/login.keyring | grep -qx '\[keyring\]' && grep -qx 'lock-on-idle=false' $K/login.keyring && [ "$(cat $K/default)" = login ] && echo 1)" "$o $(ls -la $K 2>&1)"
ok "...gnome-keyring-daemon's secrets part only (no SSH or GPG agent), in the foreground (stopped with the account's programs)" "$([ "$(cat $T/args)" = '--foreground --components=secrets' ] && echo 1)" "$(cat $T/args 2>&1)"
printf 'an encrypted keyring of the person own\n' > $K/login.keyring; echo other > $K/default
env -i HOME=$T/home DBUS_SESSION_BUS_ADDRESS=unix:path=$T/none KEYRING_DAEMON=$T/daemon sh $KR start >/dev/null 2>&1
ok "2. a keyring already there is never changed (nor which is the default)" "$(grep -q 'of the person own' $K/login.keyring && [ "$(cat $K/default)" = other ] && echo 1)"
rm -f $T/args; o=$(env -i HOME=$T/home KEYRING_DAEMON=$T/daemon sh $KR start 2>&1); r1=$?; o2=$(env -i HOME=/ DBUS_SESSION_BUS_ADDRESS=x KEYRING_DAEMON=$T/daemon sh $KR start 2>&1); r2=$?
ok "3. no session (no bus, or no home): it does not start" "$([ $r1 = 1 ] && [ $r2 = 1 ] && [ ! -e $T/args ] && echo 1)" "$r1 $r2 $o $o2"
grep -q 'AU_KEYRING " start' $HERE/../../sys/rudolfd-users.inc && grep -q 'XDG_CURRENT_DESKTOP=%s' $HERE/../../sys/rudolfd-users.inc
ok "4. rudolfd starts it at sign-in as the account's user (qt/host/users59_t.sh 6c and 7 check who, its session and its stop)" "$([ $? = 0 ] && echo 1)"
grep -q "^unix	/run/user/\*/keyring/\*	gnome-keyring-daemon	" $HERE/../../sys/root/usr/share/rudolf/listeners
ok "5. its socket is in the list of listeners (the self-test)" "$([ $? = 0 ] && echo 1)"
# the real one
if command -v gnome-keyring-daemon >/dev/null && command -v secret-tool >/dev/null && command -v dbus-daemon >/dev/null; then
	H=$T/real; mkdir -p $H; RT=$T/rt; mkdir -m 700 $RT
	dbus-daemon --session --fork --nopidfile --print-pid --address=unix:path=$RT/bus > $T/dpid; cat $T/dpid >> $T/pids
	S="env -i PATH=$PATH HOME=$H XDG_RUNTIME_DIR=$RT DBUS_SESSION_BUS_ADDRESS=unix:path=$RT/bus"
	$S sh $KR start > $T/kr.log 2>&1 & echo $! > $T/krpid; cat $T/krpid >> $T/pids
	i=0; until $S secret-tool search --all x x >/dev/null 2>&1 || [ $i -ge 50 ]; do i=$((i+1)); timeout 0.2 tail -f /dev/null; done
	echo -n "token-123" | $S timeout 10 secret-tool store --label="Claude sign-in" app claude account ann 2>$T/st.err; r=$?
	ok "6. (gnome-keyring-daemon) an app keeps a secret through the Secret Service (libsecret), with no prompt" "$([ $r = 0 ] && [ "$($S timeout 10 secret-tool lookup app claude account ann)" = token-123 ] && echo 1)" "$r $(cat $T/st.err $T/kr.log)"
	kill $(cat $T/krpid); timeout 1 tail -f /dev/null
	$S sh $KR start >> $T/kr.log 2>&1 & echo $! >> $T/pids
	i=0; until $S secret-tool search --all x x >/dev/null 2>&1 || [ $i -ge 50 ]; do i=$((i+1)); timeout 0.2 tail -f /dev/null; done
	ok "7. started again (as after a restart): the secret is still there, with no prompt" "$([ "$($S timeout 10 secret-tool lookup app claude account ann)" = token-123 ] && echo 1)" "$(cat $T/kr.log)"
	ok "...kept in the account's folder, its user's only" "$([ "$(stat -c %a $H/.local/share/keyrings)" = 700 ] && [ "$(stat -c %a $H/.local/share/keyrings/login.keyring)" = 600 ] && grep -q 'Claude sign-in' $H/.local/share/keyrings/login.keyring && echo 1)" "$(ls -la $H/.local/share/keyrings)"
	if [ "$(id -u)" = 0 ] && command -v setpriv >/dev/null; then chmod 755 $T; o=$(setpriv --reuid=65534 --regid=65534 --clear-groups cat $H/.local/share/keyrings/login.keyring 2>&1); r=$?
		ok "8. another user cannot read it" "$([ $r != 0 ] && echo 1)" "$o"; fi
else echo "SKIP 6-8: gnome-keyring-daemon, secret-tool or dbus-daemon is not here (CI installs them)"; fi
echo "$((N-F))/$N passed"; [ $F = 0 ]
