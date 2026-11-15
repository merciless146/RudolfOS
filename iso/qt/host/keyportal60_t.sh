#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md Part 14): rudolf-portal's Secret portal and Settings' "Saved passwords for apps" list, on a
# private session bus. An app in a sandbox (a root with its /.flatpak-info, as Flatpak makes) is given a secret of its
# own (libsecret encrypts the app's keyring file with it), the same each time, another app another; a program outside a
# sandbox is refused; the secrets are kept in the account's folder, its user's only; --keyring-list names them (never
# a secret) and --keyring-forget removes one. (The keyring's own items, with the real daemon: keyring60_t.sh.)
# Needs root (a chroot for the sandbox), gcc, GLib, dbus-run-session.   sh keyportal60_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (the sandbox's root)"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); D=$HERE/../../sys; T=$(mktemp -d /tmp/kp60.XXXXXX); chmod 755 $T
CF=$(pkg-config --cflags --libs gio-2.0 gio-unix-2.0)
gcc -O2 -w -o $T/rudolf-portal $D/rudolf-portal.c $CF && gcc -O2 -w -o $T/client $HERE/fixtures/secretclient.c $CF || { echo "FAIL build"; exit 1; }
cat > $T/t.sh <<'EOS'
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
mkdir -p $T/home /run/rudolf/portal; HOME=$T/home $T/rudolf-portal 2>$T/portal.log & PID=$!
i=0; until gdbus introspect --session -d org.freedesktop.portal.Desktop -o /org/freedesktop/portal/desktop >/dev/null 2>&1 || [ $i -ge 50 ]; do i=$((i+1)); timeout 0.1 tail -f /dev/null; done
# a sandbox's root: what the client needs, and the app's /.flatpak-info
sandbox() { R=$T/sb-$1; [ -d $R ] || { mkdir -p $R; for d in usr lib lib64 bin sbin etc tmp; do [ -e /$d ] && { if [ -L /$d ]; then ln -s $(readlink /$d) $R/$d; else mkdir -p $R/$d; mount --bind /$d $R/$d; fi; }; done
	mkdir -p $R/c; cp $T/client $R/c/; printf '[Application]\nname=%s\n' "$1" > $R/.flatpak-info; }; unshare -m chroot $R /c/client; }
o1=$(sandbox org.example.Chat); o2=$(sandbox org.example.Chat); o3=$(sandbox org.example.Mail)
s1=${o1#RESPONSE 0 }; s3=${o3#RESPONSE 0 }
ok "1. an app in a sandbox: its secret (64 bytes, the Request answered 0)" "$(echo "$o1" | grep -qE '^RESPONSE 0 [0-9a-f]{128}$' && echo 1)" "$o1 $(cat $T/portal.log)"
ok "...the same each time (its keyring file stays readable)" "$([ "$o1" = "$o2" ] && echo 1)" "$o1 / $o2"
ok "...another app another" "$(echo "$o3" | grep -qE '^RESPONSE 0 [0-9a-f]{128}$' && [ "$s1" != "$s3" ] && echo 1)" "$o3"
o=$($T/client)
ok "2. a program outside a sandbox is refused (it uses the Secret Service)" "$(echo "$o" | grep -q 'ERROR.*only an app in a sandbox' && echo 1)" "$o"
SD=$T/home/.local/share/rudolf/app-secrets
ok "3. kept in the account's folder, its user's only (0700, 0600)" "$([ "$(stat -c %a $SD)" = 700 ] && [ "$(stat -c %a $SD/org.example.Chat)" = 600 ] && [ "$(stat -c %s $SD/org.example.Chat)" = 64 ] && echo 1)" "$(ls -la $SD 2>&1)"
o=$(HOME=$T/home $T/rudolf-portal --keyring-list)
ok "4. --keyring-list: no Secret Service here (keyring false), the two apps named, no secret in it" "$(echo "$o" | grep -q '^{"ok":true,"keyring":false,"items":\[' && echo "$o" | grep -q '{"id":"app:org.example.Chat","label":"org.example.Chat","app":"org.example.Chat","kind":"flatpak"}' && echo "$o" | grep -q 'app:org.example.Mail' && ! echo "$o" | grep -q "$s1" && echo 1)" "$o"
o=$(HOME=$T/home $T/rudolf-portal --keyring-forget app:org.example.Chat); o2=$(HOME=$T/home $T/rudolf-portal --keyring-list)
ok "5. --keyring-forget app:ID: that app's secret removed, the other kept" "$(echo "$o" | grep -q '"ok":true' && [ ! -e $SD/org.example.Chat ] && ! echo "$o2" | grep -q 'app:org.example.Chat' && echo "$o2" | grep -q 'app:org.example.Mail' && echo 1)" "$o $o2"
o=$(sandbox org.example.Chat); ok "...asked again, it gets a new one (its old keyring file can no longer be read)" "$(echo "$o" | grep -qE '^RESPONSE 0 [0-9a-f]{128}$' && [ "$o" != "$o1" ] && echo 1)" "$o"
bad=0; for b in 'app:../x' 'app:' 'app:.hidden' '/org/freedesktop/secrets/collection/login' '/org/freedesktop/secrets/collection/login/1/x' '/etc/passwd' '/org/freedesktop/secrets/collection/../x/1'; do HOME=$T/home $T/rudolf-portal --keyring-forget "$b" >/dev/null 2>&1; [ $? = 2 ] || bad=$((bad+1)); done
ok "6. anything else to forget is refused (an app's id, a collection, a path)" "$([ $bad = 0 ] && echo 1)" "bad=$bad"
r=$(gdbus call --session -d org.freedesktop.portal.Desktop -o /org/freedesktop/portal/desktop -m org.freedesktop.DBus.Properties.Get org.freedesktop.portal.Secret version 2>&1)
ok "7. the Secret portal's version: 1" "$([ "$r" = "(<uint32 1>,)" ] && echo 1)" "$r"
kill $PID; echo "$((N-F))/$N passed"; [ $F = 0 ]
EOS
unshare -m sh -c "mount --make-rprivate / 2>/dev/null; T=$T dbus-run-session -- sh $T/t.sh" > $T/out 2>&1; r=$?; grep -v dbus-daemon $T/out; rm -rf $T; exit $r
