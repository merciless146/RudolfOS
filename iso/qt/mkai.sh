#!/bin/bash
# Test disk for the Blitzen phase (upd_t.py Z):
#   blitzen/       a model list signed with the workspace test key + a tiny random-weights model (qt/mktinymodel.py)
#   blitzen-bad/   the same list, but the model file does not match its SHA-256 (must be refused)
#   base/          the signed Debian base (as qt/mkdebrepo.sh) and the test key
#   apt/           stand-ins for python3 and g++ (shell scripts), so the run route can be tested without Debian's
#                  mirrors: "python3" prints the program's first line and the network interfaces it can see,
#                  "g++" writes a ./prog that prints "sum = 6"
#   mkai.sh OUT.img
set -e
I=$(cd "$(dirname "$0")/.." && pwd); D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
mkdir -p $D/blitzen $D/blitzen-bad $D/base $D/apt
[ -f /tmp/blitzen-test.gguf ] || python3 $I/qt/mktinymodel.py $I/src/llama.cpp /tmp/blitzen-test.gguf
bash $I/systools/mkblitzen-list.sh /tmp/blitzen-test.gguf $I/keys/rpk-signing.key "Blitzen test (random weights)" MIT $D/blitzen >/dev/null
cp $D/blitzen/blitzen.txt $D/blitzen/blitzen.txt.sig $D/blitzen-bad/; head -c 1000000 /tmp/blitzen-test.gguf > $D/blitzen-bad/blitzen-test.gguf
[ -f /tmp/debout/debian.txt ] || bash $I/systools/mkdebian.sh /tmp/debout $I/keys/rpk-signing.key
cp /tmp/debout/debian.txt /tmp/debout/debian.txt.sig /tmp/debout/debian-13-rudolf.tar.xz $D/base/
cp $I/keys/rpk-signing.pub $D/base/testkey.pub
P=$D/pkg; mkdir -p $P/py/DEBIAN $P/py/usr/bin $P/cc/DEBIAN $P/cc/usr/bin
cat > $P/py/usr/bin/python3 <<'S'
#!/bin/sh
# test stand-in for python3: "python3 -I main.py"
f=$2; echo "stand-in python: $(head -n 1 "$f")"; echo "network: $(tail -n +3 /proc/net/dev | cut -d: -f1 | tr -d ' ' | tr '\n' ' ')"
grep -q raise "$f" && { echo "Traceback (most recent call last):"; echo "RuntimeError: test failure"; exit 1; }
grep -q 'while True' "$f" && while :; do :; done
exit 0
S
cat > $P/cc/usr/bin/g++ <<'S'
#!/bin/sh
# test stand-in for g++: writes ./prog (or the -o file) that prints "sum = 6"
o=a.out; while [ $# -gt 0 ]; do [ "$1" = -o ] && { o=$2; shift; }; shift; done
printf '#!/bin/sh\necho "sum = 6"\n' > "$o"; chmod 755 "$o"
S
chmod 755 $P/py/usr/bin/python3 $P/cc/usr/bin/g++
printf 'Package: python3\nVersion: 99.0-test\nArchitecture: all\nMaintainer: Rudolf OS <test@rudolf.invalid>\nDescription: test stand-in for python3\n' > $P/py/DEBIAN/control
printf 'Package: g++\nVersion: 99.0-test\nArchitecture: all\nMaintainer: Rudolf OS <test@rudolf.invalid>\nDescription: test stand-in for g++\n' > $P/cc/DEBIAN/control
dpkg-deb -Zxz --root-owner-group -b $P/py $D/apt/python3_99.0-test_all.deb >/dev/null
dpkg-deb -Zxz --root-owner-group -b $P/cc $D/apt/g++_99.0-test_all.deb >/dev/null
(cd $D/apt && dpkg-scanpackages -m . /dev/null 2>/dev/null > Packages && gzip -k Packages)
rm -rf $P
rm -f "$1"; truncate -s 112M "$1"; mkfs.ext4 -q -F -d $D "$1"; ls -la "$1"
