#!/bin/sh
# 1.5.9 (plan Part 15.4): the development link is off unless it is being used. rudolfd's devlink requests, compiled from
# rudolfd.c (fixtures/devlinkapi.c) with a stand-in rudolf-devlink, accounts made as the page makes them
# (fixtures/deskls.js): only when allowed on this computer (its own switch, 2026-11-14), only an administrator with the administrator's password (checked by
# rudolfd), Ask first on again and the link off at every start unless "Keep on after restarting", what System health
# shows (since when, by whom, the last command). Run by devlink_t.sh. Needs root (a private mount namespace) and node.
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$DL59_INNER" ] && exec env DL59_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/devlink59.XXXXXX)
mount -t tmpfs none /run; mkdir -p /run/rudolf /persist; mount -t tmpfs none /persist; touch /run/rudolf/persistent
cat > $T/fakedl <<'X'
#!/bin/sh
case "$1" in
check) echo ok ;;
run) mkdir -p /run/rudolf/devlink; echo $$ > /run/rudolf/devlink/pid; exec tail -f /dev/null ;;
esac
X
chmod 755 $T/fakedl
gcc -O2 -w -DDLBIN="\"$T/fakedl\"" -o $T/t $HERE/fixtures/devlinkapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
N=0; F=0
ok() { N=$((N+1)); case "$2" in *[!1]*|'') r=0;; *) r=1;; esac; if [ $r = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
t() { RUDOLFD_DEVMODE_FILE=$T/extra-keys RUDOLFD_DEVLINK_ALLOW_FILE=$T/allowed $T/t $T/ls.json "$@"; }
body() { printf '%s' "$1" > $T/b; }
H=/persist/var/log/health.log; C=/persist/devlink/config
killdl() { p=$(cat /run/rudolf/devlink/pid 2>/dev/null); [ -n "$p" ] && kill $p 2>/dev/null; rm -f /run/rudolf/devlink/pid; }   # (as a restart: /run is new)
trap 'killdl; umount /persist 2>/dev/null; rm -rf $T' EXIT
ON='POST devlink/on?repo=owner/devlink&hours=2'
body '{"n":"Ann","p":"pw-ann ü","token":"github_pat_TEST"}'
o=$(t "$ON <$T/b")
ok "1. not allowed on this computer: refused, even for an administrator with the right password" "$(has "$o" '403 Forbidden')$(has "$o" 'not allowed on this computer')" "$o"
o=$(t "GET devlink"); ok "...and the link says it is not allowed (Settings shows why)" "$(has "$o" '"allowed":false')$(has "$o" '"on":false')" "$o"
touch $T/extra-keys; o=$(t "$ON <$T/b")
ok "...Developer mode (a developer's key) does not allow it: the official builds need no key" "$(has "$o" '403')$(has "$o" 'not allowed')" "$o"
rm -f $T/extra-keys
body '{"n":"Bob","p":"bob-pass"}'; o=$(t "POST devlink/allow?on=1 <$T/b")
ok "1b. allowing it: a standard account is refused" "$(has "$o" '403')$([ ! -e $T/allowed ] && echo 1)" "$o"
body '{"n":"Ann","p":"wrong"}'; o=$(t "POST devlink/allow?on=1 <$T/b")
ok "...an administrator with a wrong password is refused" "$(has "$o" '403')$([ ! -e $T/allowed ] && echo 1)" "$o"
: > $T/b; o=$(t "POST devlink/allow?on=1 <$T/b")
ok "...no password: refused" "$(has "$o" '403')$([ ! -e $T/allowed ] && echo 1)" "$o"
body '{"n":"Ann","p":"pw-ann ü"}'; o=$(t "POST devlink/allow?on=1 <$T/b"); o2=$(t "GET devlink")
ok "...an administrator with the password: allowed, root's only, said in the health log" "$(has "$o" '200')$(has "$o2" '"allowed":true')$([ "$(stat -c %a $T/allowed)" = 600 ] && echo 1)$(grep -q 'allowed on this computer by Ann' $H && echo 1)" "$o $o2"
body '{"n":"Ann","p":"pw-ann ü","token":"github_pat_TEST"}'
body '{"n":"Bob","p":"bob-pass","token":"github_pat_TEST"}'; o=$(t "$ON <$T/b")
ok "2. allowed, a standard account (right password): refused, only an administrator" "$(has "$o" '403')$(has "$o" 'only an administrator')" "$o"
body '{"n":"Ann","p":"wrong","token":"github_pat_TEST"}'; o=$(t "$ON <$T/b")
ok "3. an administrator with a wrong password: refused" "$(has "$o" '403')$(has "$o" 'Wrong password')" "$o"
ok "...said in the health log (the second time: once when allowing, once here)" "$(grep -c 'wrong administrator password for Ann (development link)' $H | grep -qx 2 && echo 1)" "$(cat $H)"
printf 'github_pat_TEST' > $T/b; o=$(t "$ON <$T/b")
ok "4. a token alone (the page's request before 1.5.9, no password): refused" "$(has "$o" '403')" "$o"
body '{"n":"Ann","p":"pw-ann ü","token":"github_pat_TEST"}'; o=$(t "$ON&ask=0 <$T/b")
ok "5. an administrator with the password, allowed: on" "$(has "$o" '200 OK')" "$o"
o=$(t "GET devlink"); now=$(date +%s); since=$(echo "$o" | sed -n 's/.*"since":\([0-9]*\).*/\1/p')
ok "...System health's line: on, since when, by whom" "$(has "$o" '"on":true')$(has "$o" '"by":"Ann"')$([ -n "$since" ] && [ $((now - since)) -lt 30 ] && echo 1)" "$o"
ok "...Ask first was turned off for now" "$(has "$o" '"ask":false')" "$o"
ok "...said in the health log, without the token" "$(grep -q 'switched on by Ann for 2 h (owner/devlink; Ask first off; read-only)' $H && ! grep -q github_pat $H && echo 1)" "$(cat $H)"
echo '{"t":"2026-11-14T10:00:00Z","id":"000007","kind":"sh","what":"uname -a","result":"status 0","approved":"allowed on the computer"}' >> /persist/devlink/log
o=$(t "GET devlink"); ok "6. its last command" "$(has "$o" '"lastCommand":{"t":"2026-11-14T10:00:00Z","id":"000007"')" "$o"
ok "...and the token is never sent back" "$(has "$o" 'github_pat' | grep -q 0 && echo 1)" "$o"
killdl; t boot
o=$(t "GET devlink")
ok "7. a restart (not kept on): the link is off, Ask first on again" "$(has "$o" '"on":false')$(grep -q '^on=0' $C && grep -q '^ask=1' $C && echo 1)" "$(cat $C) $o"
ok "...said in the health log" "$(grep -q 'switched off by the restart' $H && echo 1)" "$(cat $H)"
body '{"n":"Ann","p":"pw-ann ü","token":""}'; o=$(t "$ON&ask=0&restart=1 <$T/b")
ok "8. Keep on after restarting (the token kept from last time)" "$(has "$o" '200 OK')$(grep -q '^restart=1' $C && echo 1)" "$o"
killdl; t boot; timeout 2 tail -f /dev/null
o=$(t "GET devlink")
ok "...after the restart it is on again, but Ask first is on again" "$(has "$o" '"on":true')$(has "$o" '"ask":true')" "$o"
ok "...said in the health log" "$(grep -q 'stayed on after the restart (Keep on after restarting), with Ask first on' $H && echo 1)" "$(cat $H)"
killdl; rm -f $T/allowed; t boot; o=$(t "GET devlink")
ok "9. disallowed meanwhile: off at the restart, even when kept on" "$(has "$o" '"on":false')" "$o"
touch $T/allowed; body '{"n":"Ann","p":"pw-ann ü"}'; t "$ON <$T/b" >/dev/null
o=$(t "POST devlink/off"); o2=$(t "GET devlink")
ok "10. End now: off, said in the health log" "$(has "$o" '200')$(has "$o2" '"on":false')$(grep -q 'development link was switched off"' $H && echo 1)" "$o $o2"
grep -q '"k":"security"' $H; ok "...the link's events are under Security" "$([ $? = 0 ] && echo 1)"
# 1.5.9 (plan 15.7): read-only unless "Allow changes to this computer"
body '{"n":"Ann","p":"pw-ann ü"}'; t "$ON <$T/b" >/dev/null; o=$(t "GET devlink")
ok "11. switched on without ticking it: read-only" "$(has "$o" '"change":false')$(grep -q '^change=0' $C && echo 1)" "$(cat $C) $o"
ok "...said in the health log" "$(grep -q 'Ask first on; read-only)' $H && echo 1)" "$(cat $H)"
body '{"n":"Bob","p":"bob-pass"}'; o=$(t "POST devlink/change?on=1 <$T/b")
ok "12. allowing changes: a standard account is refused" "$(has "$o" '403')$(grep -q '^change=0' $C && echo 1)" "$o"
body '{"n":"Ann","p":"wrong"}'; o=$(t "POST devlink/change?on=1 <$T/b")
ok "...an administrator with a wrong password is refused" "$(has "$o" '403')$(grep -q '^change=0' $C && echo 1)" "$o"
body '{"n":"Ann","p":"pw-ann ü"}'; o=$(t "POST devlink/change?on=1 <$T/b"); o2=$(t "GET devlink")
ok "...the administrator's password: it can change this computer" "$(has "$o" '200')$(has "$o2" '"change":true')$(grep -q '^change=1' $C && echo 1)" "$o $o2"
ok "...said in the health log, with who" "$(grep -q 'allowed to change this computer by Ann' $H && echo 1)" "$(cat $H)"
: > $T/b; o=$(t "POST devlink/change?on=0 <$T/b"); o2=$(t "GET devlink")
ok "13. making it read-only again needs no password" "$(has "$o" '200')$(has "$o2" '"change":false')" "$o $o2"
body '{"n":"Ann","p":"pw-ann ü"}'; t "POST devlink/change?on=1 <$T/b" >/dev/null; rm -f $T/allowed
body '{"n":"Ann","p":"pw-ann ü"}'; t "POST devlink/change?on=0 <$T/b" >/dev/null; o=$(t "POST devlink/change?on=1 <$T/b")
ok "14. not allowed on this computer, changes cannot be allowed" "$(has "$o" '403')$(has "$o" 'not allowed')" "$o"
touch $T/allowed; t "POST devlink/change?on=1 <$T/b" >/dev/null
o=$(t "POST devlink/off"); ok "15. switched off: read-only again" "$(grep -q '^change=0' $C && echo 1)" "$(cat $C)"
body '{"n":"Ann","p":"pw-ann ü"}'; t "$ON&restart=1&change=1 <$T/b" >/dev/null
ok "16. switched on with it ticked: it can change this computer" "$(grep -q '^change=1' $C && echo 1)" "$(cat $C)"
killdl; t boot; timeout 2 tail -f /dev/null; o=$(t "GET devlink")
ok "17. a restart (kept on): still on, but read-only again" "$(has "$o" '"on":true')$(has "$o" '"change":false')$(grep -q '^change=0' $C && echo 1)" "$(cat $C) $o"
# 2026-11-14: unticking "Allow the development link" ends it at once and needs no password
: > $T/b; o=$(t "POST devlink/allow?on=0 <$T/b"); o2=$(t "GET devlink")
ok "18. disallowing it: no password, the link ends, said in the health log" "$(has "$o" '200')$(has "$o2" '"on":false')$(has "$o2" '"allowed":false')$([ ! -e $T/allowed ] && echo 1)$(grep -q 'disallowed on this computer (and switched off)' $H && echo 1)" "$o $o2"
body '{"n":"Ann","p":"pw-ann ü","token":""}'; o=$(t "$ON <$T/b")
ok "...and it cannot be switched on again until allowed" "$(has "$o" '403')" "$o"
killdl
echo "$((N-F))/$N passed"; [ $F = 0 ]
