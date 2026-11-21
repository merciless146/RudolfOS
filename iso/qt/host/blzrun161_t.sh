#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 26): a program Blitzen runs (rudolfd's ai/run) sees nothing of the person's but its own
# folder, and has no network. rudolf-debian's run_only (its own code, built alone) in a mount namespace of the test's own,
# with a stand-in home: the keyring, ~/.ssh, ~/.gnupg, another account's folder, the Documents, the display's socket and
# the session's folder are not there; its folder is, writable. And rudolfd's ai/run asks for --nonet and --only.
# Needs root (or sudo -n) for the namespaces; nothing outside them is touched.   sh blzrun161_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys
W=$(mktemp -d /var/tmp/blzrun161.XXXXXX); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
cat > $W/h.c <<X
#define RUDOLF_DEBIAN_NO_MAIN
#include "$S/rudolf-debian.c"
int main(int c, char **v) { if (c < 3) return 2; if (run_only(v[1])) { perror("run_only"); return 1; } if (setgid(65534) || setuid(65534)) return 3; execvp(v[2], v + 2); return 127; }
X
gcc -O1 -w -o $W/h $W/h.c -lcrypto -ldl || { echo "FAIL the harness did not build"; exit 1; }
SU=""; [ "$(id -u)" = 0 ] || SU="sudo -n"
$SU true 2>/dev/null || { echo "SKIP: needs root or sudo for the namespaces"; echo "0/0 passed"; exit 0; }
K="-----BEGIN OPENSSH PRIVATE KEY-----"   # (a key's first line only: no key is made or kept here)
o=$($SU unshare -m sh -c "
  mount --make-rprivate / && mount -t tmpfs t /home && mount -t tmpfs t /tmp && mkdir -p /run/user && mount -t tmpfs t /run/user
  mkdir -p /home/ann/.ssh /home/ann/.gnupg /home/ann/.local/share/keyrings /home/ann/Documents /home/bob /home/ann/.cache/blitzen/r1 /tmp/.X11-unix /run/user/1000
  echo '$K' > /home/ann/.ssh/id_ed25519; echo keyring > /home/ann/.local/share/keyrings/login.keyring; echo gpg > /home/ann/.gnupg/pubring.kbx
  echo letter > /home/ann/Documents/letter.txt; echo bob > /home/bob/notes; echo x > /tmp/.X11-unix/X0; echo bus > /run/user/1000/bus
  echo 'print(1)' > /home/ann/.cache/blitzen/r1/main.py; chmod -R 777 /home/ann/.cache
  $W/h /home/ann/.cache/blitzen/r1 sh -c '
    cd /home/ann/.cache/blitzen/r1 && echo cwd=ok
    for f in /home/ann/.ssh/id_ed25519 /home/ann/.local/share/keyrings/login.keyring /home/ann/.gnupg/pubring.kbx /home/ann/Documents/letter.txt /home/bob/notes /tmp/.X11-unix/X0 /run/user/1000/bus; do
      if cat \$f >/dev/null 2>&1; then echo SEEN \$f; else echo gone \$f; fi; done
    ls /home; ls /home/ann; cat main.py; echo out > made.txt && echo write=ok; id -u' 2>&1
  echo after:; cat /home/ann/.ssh/id_ed25519 | head -c 20; echo; ls /home/ann/.cache/blitzen/r1" 2>&1)
ok "1. its own folder is there: it starts in it, reads its program, writes its output" "$(echo "$o" | grep -q '^cwd=ok' && echo "$o" | grep -qx 'print(1)' && echo "$o" | grep -q '^write=ok' && echo 1)" "$o"
for f in .ssh/id_ed25519 .local/share/keyrings/login.keyring .gnupg/pubring.kbx Documents/letter.txt; do
	ok "2. not there: ~/$f" "$(echo "$o" | grep -q "^gone /home/ann/$f" && echo 1)" "$o"; done
ok "2. not there: another account's folder, the display's socket, the session's bus" "$(echo "$o" | grep -q '^gone /home/bob/notes' && echo "$o" | grep -q '^gone /tmp/.X11-unix/X0' && echo "$o" | grep -q '^gone /run/user/1000/bus' && ! echo "$o" | grep -q '^SEEN' && echo 1)" "$o"
ok "3. the folders above its own hold nothing else (only the way to it)" "$(echo "$o" | sed -n '/^gone \/run/,/^print/p' | grep -v '^gone' | tr '\n' ' ' | grep -q '^ann ' && echo 1)" "$o"
ok "4. outside its namespace nothing changed (the key is still where it was, its output too)" "$(echo "$o" | sed -n '/^after:/,$p' | grep -q 'BEGIN OPENSSH' && echo "$o" | sed -n '/^after:/,$p' | grep -q made.txt && echo 1)" "$o"
o=$($SU unshare -m sh -c "mount --make-rprivate / && $W/h ../x true" 2>&1); ok "5. a folder given as a relative path or with .. is refused" "$(echo "$o" | grep -q run_only && echo 1)" "$o"
grep -q 'rudolf-debian run --nonet --only %s --cwd %s -- timeout -k 2 10 python3 -I main.py' $S/rudolfd.c && grep -q "rudolf-debian run --nonet --only %s --cwd %s -- sh -c 'timeout -k 2 30 g++" $S/rudolfd.c
ok "6. rudolfd's ai/run runs Python and C++ with no network (--nonet) and its folder only (--only)" "$([ $? = 0 ] && echo 1)"
grep -q 'if (guard_action(r, "ai/run", "a program Blitzen runs"' $S/rudolfd.c && grep -q 'guard_text(out ? out : "", out ? strlen(out) : 0, &k, &on); guard_note("what a program Blitzen ran printed"' $S/rudolfd.c
ok "7. ...a program with a secret in it is not run, and what one prints is redacted (blzguard161_t.py runs both)" "$([ $? = 0 ] && echo 1)"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
