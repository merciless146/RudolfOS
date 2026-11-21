#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 18): security keys that unlock the files, rudolfd's side (rudolfd-fido.inc), compiled
# from rudolfd.c with its test I/O (fixtures/fidoapi.c): the real libfido2 talks CTAPHID to software FIDO2 keys
# (vfido.py: CTAP2.0, ES256, hmac-secret, PIN protocol 1), each on a Unix socket; which are "plugged in" is
# RUDOLFD_FIDO_SOCKS. Accounts are made as the page makes them (fixtures/fidols.js). Checked: adding keys (the password,
# a PIN when the key has one, a key added twice refused), two keys before they can be required, the sign-in giving the
# page's own keys only after a touch (no key, the wrong key, another account's key, a replayed answer, a key not touched),
# the password's copy removed and then a copy of the disk opening nothing with the password alone (offline, from the
# files: with a key's hmac-secret it opens), Native's sign-in and lock screen asking for the key, the counts of wrong
# passwords, removing keys, an administrator removing another account's keys.
#   sh fido161_t.sh   (gcc, libfido2-dev, node, python3 with cryptography)
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/fido161.XXXXXX); PIDS=""
trap 'for p in $PIDS; do kill $p 2>/dev/null; done; rm -rf "$T"' EXIT
N=0; F=0
ok() { N=$((N+1)); if [ -n "$2" ] && [ -z "$(printf %s "$2" | tr -d 1)" ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
no() { [ "$1" = 0 ] && echo 1 || echo 0; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
gcc -O1 -w -o $T/api $HERE/fixtures/fidoapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
node $HERE/fixtures/fidols.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
mkdir -p $T/run $T/home $T/fido
# the keys: A1, A2 (Ann's), X (nobody's), P (with a PIN), C1 (Cat's)
for k in A1 A2 X P C1; do
  extra=""; [ $k = P ] && extra="--pin 2468"
  python3 $HERE/vfido.py $T/$k --listen unix:$T/$k.sock --ctl $T/$k.ctl --log $T/$k.log $extra >$T/$k.out 2>&1 & PIDS="$PIDS $!"
done
for i in $(seq 50); do [ -S $T/C1.ctl ] && [ -S $T/A1.sock ] && break; timeout 0.2 tail -f /dev/null; done
ctl() { python3 -c "import socket,sys;s=socket.socket(socket.AF_UNIX);s.connect(sys.argv[1]);s.sendall((sys.argv[2]+'\n').encode());print(s.recv(4096).decode().strip())" $T/$1.ctl "$2"; }
api() { keys="$1"; shift; env RUDOLFD_FIDO_DIR=$T/fido RUDOLFD_FIDO_SOCKS="$keys" $T/api $T/ls.json $T/home $T/run "$@"; }
body() { printf '%s' "$1" > $T/b; }
# what the page does with a reply's "fks" (fido61.js): the box kept in the account (a.fks); or another change to an account
acct() { node -e "const f='$T/ls.json',l=require(f),u=JSON.parse(l.data.bd95user);const a=u.accts.find(x=>x.n=='$1');$2;l.data.bd95user=JSON.stringify(u);require('fs').writeFileSync(f,JSON.stringify(l))"; }
keepfks() { b=$(echo "$2" | sed 's/^[^|]*|//' | node -e "const j=JSON.parse(require('fs').readFileSync(0));console.log(j.fks?JSON.stringify(j.fks):'')"); [ -n "$b" ] && acct "$1" "a.fks=$b"; }
j() { node -e "const k=require('$T/keys.json');console.log($1)"; }
A='"n":"Ann","p":"pw-ann ü"'
# ---- adding
o=$(api "" "GET keys?acct=uAnnA1b2"); ok "1. no keys yet, libfido2 loaded" "$(has "$o" '"lib":true,"devs":0,"keys":[]')" "$o"
body '{"n":"Ann","p":"wrong","name":"Blue"}'; o=$(api $T/A1.sock "POST keys/add <$T/b"); ok "2. adding with a wrong password: refused, nothing made" "$(has "$o" '403')$(no $(ctl A1 stats | grep -c '"0x01"'))" "$o"
body "{$A,\"name\":\"Blue key\"}"; o=$(api $T/A1.sock "POST keys/add <$T/b")
ok "3. the first key added (touched twice: its credential, its hmac-secret); the page given its box (fks) to keep" "$(has "$o" '"ok":true,"n":1,"req":false,"fks":{"iv"')" "$o"; keepfks Ann "$o"
ok "4. ...its record root-only, with no file key or private key in it unsealed" "$([ "$(stat -c %a $T/fido/uAnnA1b2.json)" = 600 ] && ! grep -q "$(j k.Ann.fk)" $T/fido/uAnnA1b2.json && ! grep -q "$(j "Buffer.from(k.Ann.pv,'base64').toString('hex')")" $T/fido/uAnnA1b2.json && echo 1)"
body "{$A,\"on\":true}"; o=$(api $T/A1.sock "POST keys/require <$T/b"); ok "5. requiring a key with one key only: refused (a spare first)" "$(has "$o" '409')$(has "$o" 'second security key')" "$o"
body "{$A,\"name\":\"Blue key\"}"; o=$(api $T/A1.sock "POST keys/add <$T/b"); ok "6. the same key again: refused (\"one of this account's already\")" "$(has "$o" '409')" "$o"
body "{$A,\"name\":\"Spare\"}"; o=$(api "$T/A1.sock,$T/A2.sock" "POST keys/add <$T/b"); ok "7. the spare added with both plugged in (the first one passed over)" "$(has "$o" '"n":2')" "$o"
body "{$A,\"name\":\"Pinned\"}"; o=$(api $T/P.sock "POST keys/add <$T/b"); ok "8. a key with a PIN, no PIN given: asked for (need pin)" "$(has "$o" '"need":"pin"')" "$o"
body "{$A,\"name\":\"Pinned\",\"pin\":\"1111\"}"; o=$(api $T/P.sock "POST keys/add <$T/b"); ok "9. ...a wrong PIN: refused, the key counts it" "$(has "$o" 'Wrong PIN')$(ctl P stats | grep -c '"retries": 7')" "$o"
body "{$A,\"name\":\"Pinned\",\"pin\":\"2468\"}"; o=$(api $T/P.sock "POST keys/add <$T/b"); ok "10. ...the right PIN: added (3 keys)" "$(has "$o" '"n":3')" "$o"
# ---- signing in (the page's: Standard and Lite)
SI() { body "{$A,\"k\":true$1}"; api "$2" "POST keys/signin <$T/b"; }
n2() { ctl $1 stats | python3 -c "import json,sys;print(json.load(sys.stdin)['cmds'].get('0x02',0))"; }
c0=$(n2 A1); body "{$A}"; o=$(api $T/A1.sock "POST keys/signin <$T/b"); ok "11a. sign-in, the password right: \"need\":\"key\" (401) before the key is asked (the screen says to touch it), nothing given" "$(has "$o" '401')$(has "$o" '"need":"key"')$([ "$(n2 A1)" = "$c0" ] && echo 1)" "$o"
o=$(SI "" ""); ok "11. sign-in, no key plugged in: \"need\":\"key\" (401), nothing given" "$(has "$o" '401')$(has "$o" '"need":"key"')$(no $(echo "$o" | grep -c '"fk"'))" "$o"
want_fk=$(j k.Ann.fk); want_pk=$(j k.Ann.pk); want_pv=$(j k.Ann.pv)
for k in A1 A2 P; do o=$(SI "" $T/$k.sock)
  ok "12-$k. ...key $k touched: the page's own keys (password key, file key, administrator's key)" "$(has "$o" "\"pk\":\"$want_pk\",\"fk\":\"$want_fk\",\"pv\":\"$want_pv\"")" "$o"; done
o=$(SI "" $T/X.sock); ok "13. a key that is nobody's: refused (not one of this account's), no keys given" "$(has "$o" '403')$(has "$o" 'not one of this account')$(no $(echo "$o" | grep -c '"fk"'))" "$o"
body '{"n":"Cat","p":"cat-pass","name":"Cat key"}'; o=$(api $T/C1.sock "POST keys/add <$T/b"); keepfks Cat "$o"
o=$(SI "" $T/C1.sock); ok "14. another account's key (Cat's): refused for Ann" "$(has "$o" '403')$(has "$o" 'not one of this account')" "$o"
ctl A1 "replay on" >/dev/null; o=$(SI "" $T/A1.sock); ctl A1 "replay off" >/dev/null
ok "15. a replayed answer (the key's last assertion sent again, unchanged): refused (its signature is not over this challenge)" "$(has "$o" '403')$(has "$o" 'signature')" "$o"
ctl A2 "touch never" >/dev/null; o=$(SI "" $T/A2.sock); ctl A2 "touch auto" >/dev/null; ok "16. a key not touched: \"not touched in time\" (408)" "$(has "$o" '408')" "$o"
body '{"n":"Ann","p":"nope"}'; o=$(api $T/A1.sock "POST keys/signin <$T/b"); ok "17. a wrong password: refused before any key is asked" "$(has "$o" '403')$(has "$o" 'incorrect')" "$o"
body '{"n":"Ann","p":"nope2"}'; api "" "POST keys/signin <$T/b" >/dev/null
o=$(SI "" $T/A1.sock); ok "17b. five refusals for the account (a wrong key, another's key, a replayed answer, two wrong passwords): a wait now, even with the right password and key" "$(has "$o" 429)" "$o"
rm -f $T/run/second-uAnnA1b2   # (the test goes on without waiting)
# ---- requiring it, the page's copies removed
body "{$A,\"on\":true}"; o=$(api $T/A2.sock "POST keys/require <$T/b"); ok "18a. requiring: the password, then \"need\":\"key\" (the screen says to touch it)" "$(has "$o" '"need":"key"')" "$o"
body "{$A,\"on\":true,\"k\":true}"; o=$(api $T/A2.sock "POST keys/require <$T/b"); ok "18. required (three keys, one touched)" "$(has "$o" '"ok":true,"req":true')" "$o"
# what the page does next (fido61.js): the password's copies deleted (fk, priv)
acct Ann "delete a.fk;delete a.priv;a.fido={n:3,req:true}"
# a copy of the disk: the page's storage (ls.json) and the keys' records, read offline with the password
cat > $T/off.py <<'PY'
import json, sys, base64, hashlib, hmac, subprocess
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
T, pw, vf = sys.argv[1], sys.argv[2], sys.argv[3]
ls = json.load(open(T + '/ls.json')); a = [x for x in json.loads(ls['data']['bd95user'])['accts'] if x['n'] == 'Ann'][0]
pk = hashlib.pbkdf2_hmac('sha256', pw.encode(), base64.b64decode(a['ks']), 210000, 32)
rec = json.load(open(T + '/fido/uAnnA1b2.json'))
def hk(ikm, salt):
    prk = hmac.new(salt, ikm, 'sha256').digest(); return hmac.new(prk, b'uAnnA1b2\x01', 'sha256').digest()
def opens(key, b):
    try: return AESGCM(key).decrypt(b[:12], b[12:], None)
    except Exception: return None
pb = lambda box: opens(pk, base64.b64decode(box['iv']) + base64.b64decode(box['ct'])) if box else None
pws = pb(a.get('fks')); fkb = base64.b64decode(rec['fk'])
out = {'pwcopy': 'fk' in a or 'priv' in a, 'kp': bool(rec.get('kp')), 'pws': pws is not None}
# everything the password alone opens, tried as the key secret too
out['pw_only'] = [x for x in [opens(hk((pws or bytes(32)) + (pb({'iv': '', 'ct': ''}) or bytes(32)), b'rudolf-os file key v1'), fkb), opens(pk, fkb)] if x]
r = []
for st in sys.argv[4:]:
    for k in rec['keys']:
        h = subprocess.run([sys.executable, vf, st, '--hmac', k['cid'], rec['salt']], capture_output=True, text=True).stdout.strip()
        if h == 'NONE': continue
        ksec = opens(hk(bytes.fromhex(h), b'rudolf-os security key v1'), base64.b64decode(k['ks']))
        fk = ksec and pws and opens(hk(pws + ksec, b'rudolf-os file key v1'), fkb)
        r.append(fk.hex() if fk else None)
out['with_key'] = r
print(json.dumps(out))
PY
o=$(python3 $T/off.py $T 'pw-ann ü' $HERE/vfido.py $T/A1 $T/A2)
ok "19. a copy of the disk with the password: the password's copy is gone and no key's copy opens without the key" "$(echo "$o" | node -e "const o=JSON.parse(require('fs').readFileSync(0));console.log(!o.pwcopy&&!o.kp&&o.pws&&o.pw_only.length==0?1:0)")" "$o"
ok "20. ...with a key's hmac-secret it opens, to the account's file key" "$(echo "$o" | node -e "const o=JSON.parse(require('fs').readFileSync(0));console.log(o.with_key.length==2&&o.with_key.every(x=>x=='$want_fk')?1:0)")" "$o"
o=$(SI "" $T/A1.sock); ok "21. required: the sign-in still gives the file key with a key (the password's copy gone)" "$(has "$o" "\"req\":true,\"pk\":\"$want_pk\",\"fk\":\"$want_fk\"")" "$o"
# ---- Native (desktop/login, desktop/unlock)
body "{$A}"; o=$(api $T/A1.sock "POST login <$T/b"); ok "22. Native's sign-in: right password, then \"need\":\"key\" (no session yet)" "$(has "$o" '"need":"key"')$(has "$(api "" 'GET session')" 404)" "$o"
body "{$A,\"k\":true}"; o=$(api $T/X.sock "POST login <$T/b"); ok "23. ...the wrong key: refused" "$(has "$o" '403')" "$o"
body "{$A,\"k\":true}"; o=$(api $T/A2.sock "POST login <$T/b" "GET session")
ok "24. ...a key touched: signed in, the session's file key the key's copy (the password's is gone)" "$(has "$o" '"id":"uAnnA1b2"')$(has "$o" "\"fk\":\"$want_fk\"")" "$o"
api "" "POST lock" >/dev/null
body '{"p":"pw-ann ü"}'; o=$(api "" "POST unlock <$T/b"); ok "25. Native's lock screen: the password, then the key asked for" "$(has "$o" '"need":"key"')" "$o"
body '{"p":"pw-ann ü","k":true}'; o=$(api $T/X.sock "POST unlock <$T/b"); ok "26. ...the wrong key: still locked" "$(has "$o" '403')$(has "$(api '' 'GET session')" 423)" "$o"
body '{"p":"pw-ann ü","k":true}'; o=$(api $T/A1.sock "POST unlock <$T/b" "GET session"); ok "27. ...the right one: unlocked" "$(has "$o" 'unlocked')$(has "$o" '"pk"')" "$o"
api "" "POST logout" >/dev/null
o=$(api "" "GET accounts"); ok "28. the accounts' list says which have keys (Native shows the key step), never more" "$(has "$o" '"n":"Ann","r":"admin","key":false,"old":false,"keys":3')$(no $(echo "$o" | grep -c 'cid\|salt'))" "$o"
body '{"n":"Dan","p":"dan-pass"}'; o=$(api "" "POST login <$T/b"); ok "29. an account without keys signs in as before (no key asked)" "$(has "$o" '"id":"uDanG7h8"')" "$o"
api "" "POST logout" >/dev/null
# ---- removing, counting, the administrator
body "{$A,\"i\":2}"; o=$(api "" "POST keys/remove <$T/b"); ok "30. removing one of three while required: done (two left)" "$(has "$o" '"n":2,"req":true')" "$o"
body "{$A,\"i\":0}"; o=$(api "" "POST keys/remove <$T/b"); ok "31. ...below two while required: refused" "$(has "$o" '409')" "$o"
o=$(SI "" $T/P.sock); ok "32. the removed key no longer signs in" "$(has "$o" '403')" "$o"
body '{"n":"Cat","p":"x1"}'; for i in 1 2 3 4 5; do api $T/C1.sock "POST keys/signin <$T/b" >/dev/null; done
body '{"n":"Cat","p":"cat-pass"}'; o=$(api $T/C1.sock "POST keys/signin <$T/b"); ok "33. five wrong passwords for an account: a wait, even with the right password and key" "$(has "$o" '429')" "$o"
body "{$A,\"acct\":\"uCatE5f6\"}"; o=$(api "" "POST keys/reset <$T/b"); body "{$A,\"acct\":\"uCatE5f6\",\"k\":true}"; ok "34. an administrator with keys removing another account's keys: the administrator's key asked first" "$(has "$o" '"need":"key"')$([ -f $T/fido/uCatE5f6.json ] && echo 1)" "$o"
o=$(api $T/A1.sock "POST keys/reset <$T/b"); ok "35. ...touched: Cat's keys removed" "$(has "$o" 'removed')$([ ! -f $T/fido/uCatE5f6.json ] && echo 1)" "$o"
body '{"n":"Dan","p":"dan-pass","acct":"uCatE5f6"}'; o=$(api "" "POST keys/reset <$T/b"); ok "36. ...not an administrator: refused" "$(has "$o" '403')" "$o"
body "{$A,\"on\":false,\"k\":true}"; o=$(api $T/A1.sock "POST keys/require <$T/b"); ok "37. no longer required (a touch): the page then makes the password's copy again" "$(has "$o" '"req":false')" "$o"
# a new password (the page's acRekey with fido61.js: a new salt, its boxes sealed again with the new password's key, fks too)
node -e "
const {webcrypto:c}=require('crypto'),s=c.subtle,fs=require('fs'),f='$T/ls.json',l=require(f),u=JSON.parse(l.data.bd95user),a=u.accts.find(x=>x.n=='Ann');
const b64e=x=>Buffer.from(x).toString('base64'),b64d=x=>new Uint8Array(Buffer.from(x,'base64'));
const pk=async(pw,ks)=>s.importKey('raw',await s.deriveBits({name:'PBKDF2',salt:b64d(ks),iterations:210000,hash:'SHA-256'},await s.importKey('raw',new TextEncoder().encode(pw),'PBKDF2',false,['deriveBits']),256),{name:'AES-GCM'},false,['encrypt','decrypt']);
(async()=>{const o=await pk('pw-ann ü',a.ks),pws=new Uint8Array(await s.decrypt({name:'AES-GCM',iv:b64d(a.fks.iv)},o,b64d(a.fks.ct)));
 const ks=b64e(c.getRandomValues(new Uint8Array(16))),n=await pk('new pass 2',ks),iv=c.getRandomValues(new Uint8Array(12));
 const sl=b64e(c.getRandomValues(new Uint8Array(16)));const h=await s.deriveBits({name:'PBKDF2',salt:b64d(sl),iterations:210000,hash:'SHA-256'},await s.importKey('raw',new TextEncoder().encode('new pass 2'),'PBKDF2',false,['deriveBits']),256);
 a.h='p2\$'+sl+'\$'+b64e(new Uint8Array(h));a.ks=ks;a.fks={iv:b64e(iv),ct:b64e(new Uint8Array(await s.encrypt({name:'AES-GCM',iv},n,pws)))};
 l.data.bd95user=JSON.stringify(u);fs.writeFileSync(f,JSON.stringify(l))})()"
rm -f $T/run/second-uAnnA1b2
body '{"n":"Ann","p":"new pass 2","k":true}'; o=$(api $T/A2.sock "POST keys/signin <$T/b"); ok "38a. a new password (the page's box sealed again; no key touched for it): the keys still open the files" "$(has "$o" "\"fk\":\"$want_fk\"")" "$o"
body '{"n":"Ann","p":"pw-ann ü","k":true}'; o=$(api $T/A2.sock "POST keys/signin <$T/b"); ok "38b. ...the old password no longer" "$(has "$o" 403)" "$o"
acct Cat "delete a.fks"; body '{"n":"Cat","p":"cat-pass","k":true}'; rm -f $T/run/second-uCatE5f6
api $T/C1.sock "POST keys/add <$T/b" >/dev/null 2>&1
o=$(api $T/C1.sock "POST keys/signin <$T/b"); ok "38c. keys not required and the page's box lost: the password's copy opens, a new box given to keep" "$(has "$o" "\"fk\":\"$(j k.Cat.fk)\"")$(has "$o" '"fks":')" "$o"
acct Ann "delete a.fks"; body '{"n":"Ann","p":"new pass 2","k":true}'; o=$(api $T/A2.sock "POST keys/signin <$T/b")
ok "38d. keys required and the page's box lost: nothing opens (no fall back to a password's copy)" "$(has "$o" 409)$(no $(echo "$o" | grep -c '"fk"'))" "$o"
printf '{broken' > $T/fido/uDanG7h8.json; body '{"n":"Dan","p":"dan-pass"}'; o=$(api "" "POST login <$T/b")
ok "39. a damaged record: the sign-in stops (it never falls back to the password's copy)" "$(has "$o" '500')$(has "$o" 'damaged')" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
