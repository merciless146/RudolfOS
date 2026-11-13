#!/bin/bash
# 1.5.9 (plan 8.6): docs/custom-builds.md's commands (its ```sh ci blocks) run as written, in order, in a throw-away
# copy of the repository's tools: keys made, the release key checked, a package built with it, Developer mode turned
# on with it and a release signed by it accepted. Between blocks this script does what the page tells the reader to do
# by hand (the public key into trusted-keys.txt). Nothing is kept.   bash custombuild_t.sh
set -u; cd "$(dirname "$0")"; I=$(cd ../.. && pwd); R=$(cd "$I/.." && pwd); W=$(mktemp -d); P=0; F=0; trap 'rm -rf "$W"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
D=$R/docs/custom-builds.md
python3 - "$D" "$W/blocks" <<'P'
import sys,re,os
t=open(sys.argv[1]).read();os.makedirs(sys.argv[2])
for i,b in enumerate(re.findall(r'```sh ci\n(.*?)```',t,re.S)):open(f'{sys.argv[2]}/{i}.sh','w').write(b)
P
N=$(ls $W/blocks | wc -l); ok "0. the page has its five command blocks" "$([ "$N" = 5 ] && echo 0)" "$N"
mkdir -p $W/repo/iso $W/keys $W/pkg/www $W/out; cp -a $I/pkgtools $I/systools $I/trusted-keys.txt $W/repo/iso/
gcc -O2 -w -o $W/ru $I/sys/rudolf-update.c -lcrypto -ldl || exit 1
mkdir -p $W/sys; : > $W/cmd; : > $W/mounts; echo disc > $W/slot
cat > $W/rux <<E
#!/bin/sh
RU_SYSD=$W/sys RU_MEDIA=$W/media RU_CONF=$W/conf RU_KEYS=$W/repo/iso/trusted-keys.txt RU_EXTRA=$W/extra-keys RU_SLOT=$W/slot RU_CMDLINE=$W/cmd RU_MOUNTS=$W/mounts RU_SYSBLOCK=$W/sb exec $W/ru "\$@"
E
chmod +x $W/rux
printf 'name = hello-game\nversion = 1.0\nkind = webapp\ntitle = Hello Game\nsummary = A test\nlicense = MIT\npermissions = saves\n' > $W/pkg/PKGINFO; echo '<!doctype html>hi' > $W/pkg/www/index.html
export REPO=$W/repo KEYS=$W/keys PKG=$W/pkg OUT=$W/out/hello-game.rpk RU=$W/rux REL=$W/rel
blk() { (cd $W/repo && bash -e $W/blocks/$1.sh) 2>&1; }
o=$(blk 0); ok "1. the release key is made (private and public halves)" "$([ -s $W/keys/rpk-signing.key ] && [ -s $W/keys/rpk-signing.pub ] && echo 0)" "$o"
o=$(blk 1); ok "2. before its public half is in trusted-keys.txt, the check refuses it" "$([ -n "$o" ] && ! grep -q "$(cat $W/keys/rpk-signing.pub)" $W/repo/iso/trusted-keys.txt && echo "$o" | grep -qi 'not\|error' && echo 0)" "$o"
cut -d' ' -f1 $W/keys/rpk-signing.pub > $W/repo/iso/trusted-keys.txt   # (the page: replace the line with your public key)
o=$(blk 1); ok "3. ...after, the workflow's check accepts it" "$(echo "$o" | grep -q 'public key' && ! echo "$o" | grep -qi error && echo 0)" "$o"
o=$(blk 2); ok "4. the Secure Boot key and certificate are made" "$([ -s $W/keys/rudolf-sb.key ] && [ -s $W/keys/rudolf-sb.crt ] && openssl x509 -in $W/keys/rudolf-sb.crt -noout >/dev/null 2>&1 && echo 0)" "$o"
o=$(blk 3); ok "5. a Library package is built and signed with the key" "$([ -s $OUT ] && echo "$o" | grep -q 'hello-game 1.0' && echo 0)" "$o"
mkdir -p $W/l; for f in base chromium desktop modules; do head -c 50000 /dev/urandom > $W/l/$f.sfs; done; head -c 20000 /dev/urandom > $W/l/vmlinuz
python3 $I/systools/make_release.py --layers $W/l --kernel $W/l/vmlinuz --out $REL --key $W/keys/rpk-signing.key --serial 2026120100 --version 1.5.9-alex >/dev/null 2>&1
cp $I/trusted-keys.txt $W/repo/iso/trusted-keys.txt   # (an official computer: it trusts the official key only)
o=$(blk 4); ok "6. on an official computer, Developer mode with the key, and then the custom release is accepted" "$(echo "$o" | grep -q 'Developer mode is on' && echo "$o" | grep -q 'serial 2026120100' && echo 0)" "$o"
for k in rpk-signing.key rudolf-sb.key; do (cd $R && git check-ignore -q "keys/$k" && git check-ignore -q "x/$k") || { ok "7. private keys are ignored by git" 1 "$k"; break; }; done; [ $F = 0 ] && ok "7. private keys (keys/, *.key, *.pem) are ignored by git" 0
grep -q 'never go into the repository\|Only their public halves ever go into the repository' $D; ok "8. the page says only public halves go into the repository" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
