#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 27 item 5): rudolfd's side of Developer mode's test-computer commands (ai/dev/dl-send, dl-wait;
# compiled from rudolfd.c, fixtures/knowapi.c, with a stand-in rudolf-dev that notes what it is asked to send): a command carrying a
# password, a token or a key is refused (never given to rudolf-dev); one command waits at a time (429 while the last one is waited
# for; free once its result is read, or 2 minutes after nobody asked about it); an ordinary command goes as before.
#   sh blzdev161_t.sh (as root)
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (rudolf-dev runs as nobody)"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/blzdev161.XXXXXX); trap 'rm -rf $T' EXIT
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
mkdir -p $T/dev; printf 'repo=dev/RudolfOS\nbase=main\ndevlink=dev/rudolfos-devlink\n' > $T/dev/rudolf.conf
printf 'github_pat_TESTONLY%s\n' "$(head -c 12 /dev/urandom | od -An -tx1 | tr -d ' \n')" > $T/dev/rudolf.devlink.token; chmod 600 $T/dev/rudolf.devlink.token
cat > $T/rudolf-dev <<X
#!/bin/sh
# (the stand-in: what it was asked for, noted; the token line read and dropped)
read tok; case "\$1" in dl-send) read cmd; echo "\$cmd" >> $T/sent; n=\$(wc -l < $T/sent); printf '{"ok":true,"id":"%06d"}\n' \$n;;
dl-wait) if [ -e $T/done-\$3 ]; then printf '{"ok":true,"result":{"id":"%s","status":0}}\n' "\$3"; else echo '{"ok":true,"waiting":true}'; fi;; *) echo '{}';; esac
X
chmod 755 $T/rudolf-dev; touch $T/devmode; chmod 755 $T; : > $T/sent; chmod 666 $T/sent   # (rudolf-dev runs as nobody)
gcc -O1 -w -DDEV_BIN="\"$T/rudolf-dev\"" -DDEV_DIR="\"$T/dev\"" -DDEV_WAITF="\"$T/waiting\"" -DPERSIST="\"$T/p\"" -o $T/t $HERE/fixtures/knowapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
mkdir -p $T/p/os; export RUDOLFD_DEVMODE_FILE=$T/devmode
t() { $T/t $T/accounts "$@"; }
printf '{"kind":"sh","script":"curl -H \\"Authorization: Bearer ghp_%s\\" https://api.github.com/user"}' "$(printf 'A%.0s' $(seq 1 36))" > $T/c1
printf '{"kind":"sh","script":"echo password=hunter2222 > /tmp/x"}' > $T/c2
printf '{"kind":"sh","script":"echo -----BEGIN OPENSSH PRIVATE KEY-----"}' > $T/c3
printf '{"kind":"sh","script":"echo dGhpc0lzQVNlY3JldEtleTEyMzQ1Njc4OTBhYmNkZWZn"}' > $T/c4
o=$(t "POST ai/dev/dl-send <$T/c1" "POST ai/dev/dl-send <$T/c2" "POST ai/dev/dl-send <$T/c3" "POST ai/dev/dl-send <$T/c4")
ok "1. a command carrying a token, a password, a private key or a key-like run is refused, and never given to rudolf-dev" "$([ "$(echo "$o" | grep -c '^HTTP/1.1 400.*\|not sent: the command carries')" -ge 4 ] && [ ! -s $T/sent ] && echo 1)" "$o"
printf '{"kind":"sh","script":"tail -n 50 /persist/var/log/health.log; sha256sum /etc/os-release"}' > $T/ok1; printf '{"kind":"sh","script":"uname -a"}' > $T/ok2
o=$(t "POST ai/dev/dl-send <$T/ok1"); ok "2. an ordinary command goes (its id back)" "$([ "$(has "$o" '"id":"000001"')" = 1 ] && grep -q 'health.log' $T/sent && echo 1)" "$o"
o=$(t "POST ai/dev/dl-send <$T/ok2"); ok "3. one at a time: another while the first is waited for is refused (429), not sent" "$([ "$(has "$o" '429')" = 1 ] && [ "$(has "$o" '000001 is still waiting')" = 1 ] && [ "$(wc -l < $T/sent)" = 1 ] && echo 1)" "$o"
o=$(t "POST ai/dev/dl-wait?id=000001&secs=1" "POST ai/dev/dl-send <$T/ok2"); ok "4. ...still waiting: still refused" "$([ "$(has "$o" '"waiting":true')" = 1 ] && [ "$(has "$o" '429')" = 1 ] && echo 1)" "$o"
touch $T/done-000001; o=$(t "POST ai/dev/dl-wait?id=000001&secs=1" "POST ai/dev/dl-send <$T/ok2"); ok "5. its result read: the next one goes" "$([ "$(has "$o" '"result"')" = 1 ] && [ "$(has "$o" '"id":"000002"')" = 1 ] && echo 1)" "$o"
touch -d '3 minutes ago' $T/waiting; o=$(t "POST ai/dev/dl-send <$T/ok2"); ok "6. nobody asked about the last one for 2 minutes: the next one goes" "$([ "$(has "$o" '"id":"000003"')" = 1 ] && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
