#!/bin/sh
# 1.6.0 (the owner's Vivobook report 6: Terminal on Native had only Command Prompt tabs): the native desktop's sign-in
# (rudolfd's desktop/login, compiled from rudolfd.c: fixtures/deskapi.c) opens the encrypted folder (Debian, Linux and
# Windows programs, Linux apps, Downloads) as the page's own sign-in does on Standard (vault50.js vaultSignin): the
# account's copy of the data key in vault.json, made here exactly as the page makes it (Node's WebCrypto: PBKDF2-SHA256,
# 210 000 rounds, AES-256-GCM), unwrapped with the password and added to the kernel (ext4's encryption on a loop device);
# nothing without encryption set up or without the account's copy; a copy made with another password only noted in the
# health log. Needs root (a loop device, a private mount namespace) and node.   sh vaultnative60_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$VN60_INNER" ] && exec env VN60_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/vaultnative.XXXXXX)
trap 'umount $T/m 2>/dev/null; rm -rf $T' EXIT
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run; mkdir -p /run/rudolf
truncate -s 64M $T/img; mkfs.ext4 -q -F -O encrypt $T/img; mkdir $T/m; mount -o loop $T/img $T/m || { echo "SKIP: no loop device"; exit 0; }
P=$T/m; mkdir -p $P/etc $P/var/log $P/accounts; touch /run/rudolf/persistent
# 1.6.1 (docs/plan-1.6.1.md Part 11, drives part 2): VN_VIA=helper: the encrypted folder opened by the drives helper and
# drives-home (built with -DRR_TEST and this drive as PERSIST), as rudolfd does on the system: every check the same
RDIR=; if [ "$VN_VIA" = helper ]; then RDIR="-DRD_ROOT_DIR=\"$T/root\""; mkdir -p $T/root; echo "(through the drives helper and drives-home)"
	printf 'RR_TEST_DIR=%s\nRR_TEST_HOMEROOT=%s\nRR_TEST_NOBIND=1\nRR_TEST_AUON=0\nRR_TEST_AUMAP=%s/none\nRR_TEST_HEALTH=%s/rrhealth\n' $T/root $P/accounts $T $T > $T/rr.conf
	CF="-O2 -w -DRR_TEST -DRR_TEST_CONF=\"$T/rr.conf\""
	gcc $CF -o $T/root/drives $HERE/../../sys/rudolf-drives.c && gcc $CF -DPERSIST="\"$P\"" -o $T/root/drives-home $HERE/../../sys/rudolf-drives-home.c -lz || { echo "FAIL build the helpers"; exit 1; }
	chmod 700 $T/root/drives $T/root/drives-home; fi
gcc -O2 -w $RDIR -DPERSIST="\"$P\"" -o $T/t $HERE/fixtures/deskapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
D=$T/home; mkdir -p $D
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
t() { $T/t $P/accounts $T/ls.json $D /run/rudolf "$@"; }
login() { printf '{"n":"%s","p":"%s"}' "$1" "$2" > $T/b; t "POST login <$T/b"; }
# the page's vault.json (vault50.js vWrap): Ann's copy with her password, Bob's made with another one
cat > $T/v.js <<'E'
const {webcrypto: c} = require('crypto'), fs = require('fs'), s = c.subtle, b64e = u => Buffer.from(u).toString('base64');
const wrap = async (pw, dk) => { const salt = c.getRandomValues(new Uint8Array(16)), k = await s.deriveKey({name: 'PBKDF2', salt, iterations: 210000, hash: 'SHA-256'},
  await s.importKey('raw', new TextEncoder().encode(pw), 'PBKDF2', false, ['deriveKey']), {name: 'AES-GCM', length: 256}, false, ['encrypt']);
  const iv = c.getRandomValues(new Uint8Array(12)); return {salt: b64e(salt), iv: b64e(iv), ct: b64e(new Uint8Array(await s.encrypt({name: 'AES-GCM', iv}, k, dk)))} };
(async () => { const dk = c.getRandomValues(new Uint8Array(64));
  fs.writeFileSync(process.argv[2], JSON.stringify({v: 1, accts: {uAnnA1b2: await wrap('pw-ann ü', dk), uBobC3d4: await wrap('not bob', dk)}, rec: await wrap('RECOVERY', dk)})) })();
E
o=$(login Ann 'pw-ann ü')
ok "1. encryption not set up: the sign-in as before, nothing unlocked" "$(echo "$o" | grep -q '"ok":true' && [ ! -e /run/rudolf/vault-unlocked ] && echo 1)" "$o"
t "POST logout" >/dev/null
node $T/v.js $P/etc/vault.json || { echo "FAIL node"; exit 1; }
o=$(login Bob bob-pass)
ok "2. an account whose copy of the key was made with another password: signed in, the folder stays locked, noted in the health log" "$(echo "$o" | grep -q '"ok":true' && [ ! -e /run/rudolf/vault-unlocked ] && grep -q 'did not open (native sign-in)' $P/var/log/health.log && echo 1)" "$o $(cat $P/var/log/health.log 2>/dev/null)"
t "POST logout" >/dev/null
o=$(login Ann 'pw-ann ü')
ok "3. Ann's own copy: the native sign-in opens the encrypted folder (the key added to the kernel, the folder made with its policy)" "$(echo "$o" | grep -q '"ok":true' && [ -e /run/rudolf/vault-unlocked ] && [ -s $P/etc/vault.id ] && [ -d $P/vault ] && grep -q 'the encrypted folder was unlocked' $P/var/log/health.log && echo 1)" "$o $(cat $P/var/log/health.log 2>/dev/null)"
# the folder really is encrypted with that key: a file written now, unreadable once the key is removed
echo secret > $P/vault/f.txt 2>/dev/null; ok "4. ...it can be written to" "$(grep -q secret $P/vault/f.txt 2>/dev/null && echo 1)"
ID=$(cat $P/etc/vault.id); n0=$(grep -c . $P/var/log/health.log)
t "POST logout" >/dev/null; o=$(login Ann 'pw-ann ü')
ok "5. ...signing in again with it open does nothing more" "$(echo "$o" | grep -q '"ok":true' && [ "$(cat $P/etc/vault.id)" = "$ID" ] && [ "$(grep -c . $P/var/log/health.log)" = "$n0" ] && echo 1)" "$(tail -3 $P/var/log/health.log)"
if [ "$VN_VIA" = helper ]; then
	mv $T/root/drives $T/root/drives.real; printf '#!/bin/sh\necho "the stand-in helper"\nexit 200\n' > $T/root/drives; chmod 700 $T/root/drives
	rm -f /run/rudolf/vault-unlocked; t "POST logout" >/dev/null; o=$(login Ann 'pw-ann ü'); mv $T/root/drives.real $T/root/drives
	ok "6. (through the helper) a failing drives helper: the folder stays locked, the sign-in said so in the health log (so the checks above went through it)" "$([ ! -e /run/rudolf/vault-unlocked ] && tail -2 $P/var/log/health.log | grep -q 'the stand-in helper' && echo 1)" "$o $(tail -2 $P/var/log/health.log)"
fi
echo "$((N-F))/$N passed"; [ $F = 0 ]
