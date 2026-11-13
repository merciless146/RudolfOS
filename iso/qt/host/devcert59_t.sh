#!/bin/sh
# 1.5.9 (plan 8.2): packages from outside developers, through a chain of trust. The Library's key signs a developer's
# certificate (name, GitHub account, public key, name prefix, expiry); the developer signs their package with their
# own key and it carries the certificate. rpk accepts it only for the desktop, under the prefix, with a certificate
# from the Library that has not expired; it never replaces someone else's package. Revocations in the signed list
# refuse new installs and disable installed packages. Throwaway keys. Needs root (it mounts packages).
#   sh devcert59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../..; TOOL=$I/pkgtools/rpk-tool.py
T=$(mktemp -d); trap 'for m in $(grep " $T/" /proc/mounts | cut -d" " -f2); do umount -d $m 2>/dev/null; done; rm -rf $T' EXIT
gcc -O2 -o $T/rpk $I/sys/rpk.c -lcrypto || exit 1
for k in sys lib dev evil; do python3 $TOOL keygen $T/$k >/dev/null; done
export RPK_ROOT=$T/root RPK_MNT=$T/mnt RPK_DISC=$T/nodisc RPK_KEYS=$T/sys/rpk-signing.pub RPK_LIBRARY_KEYS=$T/lib/rpk-signing.pub RPK_SOURCES=$T/sources
export RPK_HEALTH=$T/health; printf '#!/bin/sh\necho "$2 $3" >> %s/hlog\n' $T > $T/health; chmod +x $T/health; : > $T/hlog
DEV=$(cut -d' ' -f1 $T/dev/rpk-signing.pub)
pk() { d=$T/p/$1-$2; rm -rf $d; mkdir -p $d/www; printf 'name = %s\nversion = %s\nkind = %s\ntitle = %s\nsummary = test\nlicense = MIT\n' $1 $2 $3 $1 > $d/PKGINFO; echo "$1 $2" > $d/www/hello.txt
	[ $3 = cli ] && { mkdir -p $d/bin; printf '#!/bin/sh\n' > $d/bin/$1; chmod +x $d/bin/$1; printf 'commands = %s\n' $1 >> $d/PKGINFO; }; echo $d; }
cert() { python3 $TOOL cert $T/$1.cert --key $T/$2/rpk-signing.key --name "Alex Doe" --github alexdoe --pubkey $DEV --prefix alex- --days $3 >/dev/null; }
mkdir -p $T/librepo; printf '%s\n' $T/librepo > $T/sources
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
cert good lib 365
python3 $TOOL build $(pk alex-game 1.0 webapp) $T/a.rpk --key $T/dev/rpk-signing.key --cert $T/good.cert >/dev/null
o=$($T/rpk install-file $T/a.rpk 2>&1); r=$?
ok "1. a developer's package with the Library's certificate installs, and is served" $([ $r = 0 ] && grep -q "alex-game 1.0" $T/mnt/alex-game/www/hello.txt && echo 1 || echo 0) "$r $o"
ok "2. ...recorded as the developer's (name, GitHub account, key)" $(grep -qx "signed = developer" $T/root/db/alex-game.info && grep -qx "github = alexdoe" $T/root/db/alex-game.info && grep -qx "certkey = $DEV" $T/root/db/alex-game.info && echo 1 || echo 0) "$(cat $T/root/db/alex-game.info)"
o=$(python3 $TOOL build $(pk bob-game 1.0 webapp) $T/b.rpk --key $T/dev/rpk-signing.key --cert $T/good.cert 2>&1)
ok "3. the build tool refuses a name outside the certificate's prefix" $(echo "$o" | grep -q "must start with the certificate's prefix" && echo 1 || echo 0) "$o"
# (made by hand, as someone bypassing the tool would)
python3 - $(pk bob-game 1.0 webapp) $T/b.rpk $T/dev/rpk-signing.key $T/good.cert <<'P'
import sys,subprocess,hashlib,base64,os
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
d,out,k,c=sys.argv[1:5];tmp=out+'.sfs';subprocess.run(['mksquashfs',d,tmp,'-noappend','-quiet','-no-progress','-all-root'],check=True)
b=open(tmp,'rb').read();key=Ed25519PrivateKey.from_private_bytes(base64.b64decode(open(k).read().strip()))
sig=key.sign(b'rpk1 '+hashlib.sha256(b).hexdigest().encode());ct=open(c,'rb').read();cs=open(c+'.sig','rb').read()
open(out,'wb').write(b+ct+cs+len(ct).to_bytes(4,'little')+b'RPKCERT1'+sig+b'RPKSIG02');os.unlink(tmp)
P
o=$($T/rpk install-file $T/b.rpk 2>&1); ok "4. ...and rpk refuses one made by hand" $(echo "$o" | grep -q "not one its developer certificate allows" && [ ! -e $T/root/store/bob-game.rpk ] && echo 1 || echo 0) "$o"
cert bad evil 365; python3 $TOOL build $(pk alex-two 1.0 webapp) $T/c.rpk --key $T/dev/rpk-signing.key --cert $T/bad.cert >/dev/null
o=$($T/rpk install-file $T/c.rpk 2>&1); ok "5. a certificate not signed by the Library: refused" $(echo "$o" | grep -q "not signed by the Rudolf OS Library" && echo 1 || echo 0) "$o"
cert old lib -1; python3 $TOOL build $(pk alex-two 1.0 webapp) $T/d.rpk --key $T/dev/rpk-signing.key --cert $T/old.cert >/dev/null
o=$($T/rpk install-file $T/d.rpk 2>&1); ok "6. an expired certificate: refused" $(echo "$o" | grep -q "has expired" && echo 1 || echo 0) "$o"
python3 $TOOL build $(pk alex-two 1.0 webapp) $T/e.rpk --key $T/evil/rpk-signing.key --cert $T/good.cert >/dev/null
o=$($T/rpk install-file $T/e.rpk 2>&1); ok "7. signed by another key than the certificate names: refused" $(echo "$o" | grep -q "not from the developer its certificate names" && echo 1 || echo 0) "$o"
python3 $TOOL build $(pk alex-cli 1.0 cli) $T/f.rpk --key $T/dev/rpk-signing.key --cert $T/good.cert >/dev/null
o=$($T/rpk install-file $T/f.rpk 2>&1); ok "8. a developer's package with a program: refused (files for the desktop only)" $(echo "$o" | grep -q "can only hold files for the desktop" && echo 1 || echo 0) "$o"
python3 $TOOL build $(pk alex-lib 1.0 addon) $T/librepo/alex-lib.rpk --key $T/lib/rpk-signing.key >/dev/null; python3 $TOOL index $T/librepo --key $T/lib/rpk-signing.key --serial 1 >/dev/null
$T/rpk install alex-lib >/dev/null 2>&1; python3 $TOOL build $(pk alex-lib 2.0 addon) $T/g.rpk --key $T/dev/rpk-signing.key --cert $T/good.cert >/dev/null
o=$($T/rpk install-file $T/g.rpk 2>&1); ok "9. it never replaces the Library's own package, even under its prefix" $(echo "$o" | grep -q "would replace a package from someone else" && grep -q "alex-lib 1.0" $T/mnt/alex-lib/www/hello.txt && echo 1 || echo 0) "$o"
python3 $TOOL build $(pk alex-game 1.1 webapp) $T/h.rpk --key $T/dev/rpk-signing.key --cert $T/good.cert >/dev/null
o=$($T/rpk install-file $T/h.rpk 2>&1); ok "10. the developer updates their own package" $(grep -q "alex-game 1.1" $T/mnt/alex-game/www/hello.txt && echo 1 || echo 0) "$o"
printf 'key %s\npkg alex-lib *\n' "$DEV" > $T/librepo/revoked.txt; python3 $TOOL index $T/librepo --key $T/lib/rpk-signing.key --serial 2 >/dev/null
o=$($T/rpk refresh 2>&1); s=$($T/rpk status)
ok "11. the Library revokes the developer's key and a package: both disabled at the next refresh (unmounted, the reason kept)" $(! grep -q " $T/mnt/alex-game " /proc/mounts && ! grep -q " $T/mnt/alex-lib " /proc/mounts && echo "$s" | grep -q '"disabled":"the Rudolf OS Library revoked its developer.s key"' && echo "$s" | grep -q '"disabled":"the Rudolf OS Library revoked this package"' && echo 1 || echo 0) "$o $s"
ok "12. ...and logged" $(grep -q "disabled alex-game" $T/hlog && echo 1 || echo 0) "$(cat $T/hlog)"
$T/rpk mount-all >/dev/null 2>&1; ok "13. a disabled package is not mounted again at start-up" $(! grep -q " $T/mnt/alex-game " /proc/mounts && echo 1 || echo 0)
o=$($T/rpk install-file $T/h.rpk 2>&1); ok "14. a revoked developer's package cannot be installed" $(echo "$o" | grep -q "key was revoked" && echo 1 || echo 0) "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
