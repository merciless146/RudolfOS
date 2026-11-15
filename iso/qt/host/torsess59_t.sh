#!/bin/sh
# 1.5.9 (docs/plan-1.5.9.md 16.2): a Tor window's sign-in on Standard. Standard has no native session (desktop/session), so the
# Tor window's page showed the sign-in screen; the desktop page now gives rudolfd its account and file key before torwin/open
# (POST torwin/session), and the Tor window's page takes it (GET torwin/session) while that window's Chromium runs. rudolfd's
# own code (fixtures/torwinapi.c, compiled from rudolfd.c), in a mount namespace with /run of its own. Needs root and gcc.
#   sh torsess59_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a mount namespace)"; exit 0; }
[ -z "$TS59_INNER" ] && exec env TS59_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/torsess.XXXXXX)
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run && mkdir -p /run/rudolf || { echo "FAIL /run"; exit 1; }
trap 'kill $SL 2>/dev/null; rm -rf $T' EXIT
gcc -O2 -w -DTORWIN_USER='"root"' -DTORWIN_LAUNCH='"/bin/true"' -o $T/tw $HERE/fixtures/torwinapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
FK=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
o=$($T/tw 'POST torwin/session {"id":"uAbc12","n":"Ann","r":"admin","fk":"'$FK'"}')
ok "1. the desktop page's account and file key kept" "$(echo "$o" | grep -q '^HTTP/1.1 200' && [ "$(stat -c '%a %u' /run/rudolf/torwin-session)" = "600 0" ] && echo 1)" "$o $(stat -c '%a %u' /run/rudolf/torwin-session 2>&1)"
o=$($T/tw 'GET torwin/session')
ok "2. no Tor window's Chromium running: not given out (404)" "$(echo "$o" | grep -q '^HTTP/1.1 404' && echo 1)" "$o"
sleep 60 & SL=$!; echo $SL > /run/rudolf/torwin.pid
o=$($T/tw 'GET torwin/session')
ok "3. while it runs: the account for its page ({id,n,r,fk}, no password key)" "$(echo "$o" | grep -q '^HTTP/1.1 200' && echo "$o" | grep -q '"id":"uAbc12","n":"Ann","r":"admin","fk":"'$FK'"}' && ! echo "$o" | grep -q '"pk"' && echo 1)" "$o"
o=$($T/tw 'POST torwin/session {"id":"guest","n":"Guest","r":"guest"}'; $T/tw 'GET torwin/session')
ok "4. a guest: no file key" "$(echo "$o" | tail -1 | grep -q '"id":"guest","n":"Guest","r":"guest","fk":""}' && echo 1)" "$o"
bad=0; for b in '{"id":"../x","n":"A","r":"admin","fk":"'$FK'"}' '{"id":"u1","n":"A","r":"root","fk":"'$FK'"}' '{"id":"u1","n":"A","r":"admin","fk":"12zz"}' '{"id":"u1","n":"A","r":"admin"}' '{"id":"u1","n":"A","r":"guest"}' '{"id":"u1","n":"A\nB","r":"user","fk":"'$FK'"}'; do
  $T/tw "POST torwin/session $b" | grep -q '^HTTP/1.1 400' || bad=$((bad+1)); done
o=$($T/tw 'GET torwin/session')
ok "5. a bad account (its id, role, file key, a guest's id, a line break in the name) refused, the one kept untouched" "$([ $bad = 0 ] && echo "$o" | grep -q '"id":"guest"' && echo 1)" "bad=$bad $o"
o=$($T/tw 'POST torwin/session {}'; $T/tw 'GET torwin/session')
ok "6. {} forgets it" "$(echo "$o" | tail -1 | grep -q '^HTTP/1.1 404' && [ ! -e /run/rudolf/torwin-session ] && echo 1)" "$o"
grep -q 'unlink(TORWIN_SESS); _exit(0);' $HERE/../../sys/rudolfd.c
ok "7. removed when the Tor window's Chromium goes (the watcher)" "$([ $? = 0 ] && echo 1)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
