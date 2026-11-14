#!/bin/sh
# 1.5.9 (plan Part 15.4): the development link is off unless it is being used. rudolfd's devlink requests, compiled from
# rudolfd.c (fixtures/devlinkapi.c) with a stand-in rudolf-devlink, accounts made as the page makes them
# (fixtures/deskls.js): only in Developer mode, only an administrator with the administrator's password (checked by
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
t() { RUDOLFD_DEVMODE_FILE=$T/extra-keys $T/t $T/ls.json "$@"; }
body() { printf '%s' "$1" > $T/b; }
H=/persist/var/log/health.log; C=/persist/devlink/config
killdl() { p=$(cat /run/rudolf/devlink/pid 2>/dev/null); [ -n "$p" ] && kill $p 2>/dev/null; rm -f /run/rudolf/devlink/pid; }   # (as a restart: /run is new)
trap 'killdl; umount /persist 2>/dev/null; rm -rf $T' EXIT
ON='POST devlink/on?repo=owner/devlink&hours=2'
body '{"n":"Ann","p":"pw-ann ü","token":"github_pat_TEST"}'
o=$(t "$ON <$T/b")
ok "1. not in Developer mode: refused, even for an administrator with the right password" "$(has "$o" '403 Forbidden')$(has "$o" 'Developer mode')" "$o"
o=$(t "GET devlink"); ok "...and the link says Developer mode is off (Settings shows why)" "$(has "$o" '"developer":false')$(has "$o" '"on":false')" "$o"
touch $T/extra-keys
body '{"n":"Bob","p":"bob-pass","token":"github_pat_TEST"}'; o=$(t "$ON <$T/b")
ok "2. in Developer mode, a standard account (right password): refused, only an administrator" "$(has "$o" '403')$(has "$o" 'only an administrator')" "$o"
body '{"n":"Ann","p":"wrong","token":"github_pat_TEST"}'; o=$(t "$ON <$T/b")
ok "3. an administrator with a wrong password: refused" "$(has "$o" '403')$(has "$o" 'Wrong password')" "$o"
ok "...said in the health log" "$(grep -c 'wrong administrator password for Ann (development link)' $H | grep -qx 1 && echo 1)" "$(cat $H)"
printf 'github_pat_TEST' > $T/b; o=$(t "$ON <$T/b")
ok "4. a token alone (the page's request before 1.5.9, no password): refused" "$(has "$o" '403')" "$o"
body '{"n":"Ann","p":"pw-ann ü","token":"github_pat_TEST"}'; o=$(t "$ON&ask=0 <$T/b")
ok "5. an administrator with the password, in Developer mode: on" "$(has "$o" '200 OK')" "$o"
o=$(t "GET devlink"); now=$(date +%s); since=$(echo "$o" | sed -n 's/.*"since":\([0-9]*\).*/\1/p')
ok "...System health's line: on, since when, by whom" "$(has "$o" '"on":true')$(has "$o" '"by":"Ann"')$([ -n "$since" ] && [ $((now - since)) -lt 30 ] && echo 1)" "$o"
ok "...Ask first was turned off for now" "$(has "$o" '"ask":false')" "$o"
ok "...said in the health log, without the token" "$(grep -q 'switched on by Ann for 2 h (owner/devlink; Ask first off)' $H && ! grep -q github_pat $H && echo 1)" "$(cat $H)"
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
killdl; rm -f $T/extra-keys; t boot; o=$(t "GET devlink")
ok "9. Developer mode turned off meanwhile: off at the restart, even when kept on" "$(has "$o" '"on":false')" "$o"
touch $T/extra-keys; body '{"n":"Ann","p":"pw-ann ü"}'; t "$ON <$T/b" >/dev/null
o=$(t "POST devlink/off"); o2=$(t "GET devlink")
ok "10. End now: off, said in the health log" "$(has "$o" '200')$(has "$o2" '"on":false')$(grep -q 'development link was switched off"' $H && echo 1)" "$o $o2"
grep -q '"k":"security"' $H; ok "...the link's events are under Security" "$([ $? = 0 ] && echo 1)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
