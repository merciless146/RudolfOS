#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 20; docs/website-data-key.md): rudolf-webkey, the shared profile's website data under a
# key sealed in the TPM. Against swtpm (a TPM 2.0 in software, its TCP server) with tpm2-tools beside it: the key made,
# the same again, a new one when PCR 7 or 14 changes (Secure Boot's measurements) and not for the others, a damaged file,
# a cleared TPM, an owner's password (nothing changed then), no TPM; the sealed object opened by tpm2-tools' own policy
# session to the same secret (another implementation of the TPM's commands: the policy and the key's derivation), and
# never with a password; the key store on a session bus (only Chromium's entry, nothing written); and, with Rudolf's
# Chromium (CHROME, default /opt/chromium/chrome; skipped without it), cookies written as v11 under the key, readable
# again, a v10 profile moved over, and nothing readable with Chromium's built-in key.
# Needs root (the sealed file is root's), gcc, swtpm, tpm2-tools, dbus-daemon, python3. Ports 8621-8623.
#   sh webkey161_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
for t in swtpm tpm2_pcrextend tpm2_createprimary dbus-daemon dbus-send gcc python3; do command -v $t >/dev/null || { echo "SKIP: needs $t (swtpm, tpm2-tools, dbus)"; exit 0; }; done
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/wk161.XXXXXX); chmod 755 $T
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
STP=; BUS=; WAL=; SRV=
trap 'for p in $WAL $SRV $BUS $STP; do kill $p 2>/dev/null; done; rm -rf $T' EXIT
. $SYS/../systools/hardflags.sh
gcc -O2 $HARD -DWEBKEY_TEST -o $T/wk $SYS/rudolf-webkey.c $(pkg-config --cflags --libs gio-2.0) -lcrypto -ldl 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
gcc -O2 $HARD -o $T/wk-rel $SYS/rudolf-webkey.c $(pkg-config --cflags --libs gio-2.0) -lcrypto -ldl || { echo "FAIL build (as released)"; exit 1; }
tpm_up() { mkdir -p $T/tpm; swtpm socket --tpm2 --tpmstate dir=$T/tpm --server type=tcp,port=8621,bindaddr=127.0.0.1 --ctrl type=tcp,port=8622,bindaddr=127.0.0.1 --flags startup-clear > $T/swtpm.log 2>&1 & STP=$!; w8 "$T/wk status >/dev/null 2>&1" 40; }
tpm_down() { kill $STP 2>/dev/null; wait $STP 2>/dev/null; STP=; }
export RUDOLF_TPM_TCP=127.0.0.1:8621 RUDOLF_WEBKEY_DIR=$T/wk.d TPM2TOOLS_TCTI="swtpm:host=127.0.0.1,port=8621"
un() { $T/wk unseal 2>$T/err; echo "rc=$?"; }
key() { echo "$1" | head -1; }; rcof() { echo "$1" | tail -1; }
ext() { tpm2_pcrextend $1:sha256=$(printf '%064x' $2) >/dev/null 2>&1; }

# ---- no TPM
o=$(RUDOLF_TPM_TCP= RUDOLF_TPM_DEV=$T/none $T/wk status; echo "rc=$?"); o2=$(RUDOLF_TPM_TCP= RUDOLF_TPM_DEV=$T/none $T/wk unseal 2>&1; echo "rc=$?")
ok "1. no TPM: status says so, unseal answers 1 (Chromium's built-in key, as before) and writes nothing" "$(echo "$o" | grep -q '"tpm":false' && [ "$(rcof "$o2")" = rc=1 ] && [ ! -e $T/wk.d ] && echo 1)" "$o / $o2"
o=$(strings $T/wk-rel | grep -c 'RUDOLF_TPM_TCP\|RUDOLF_TPM_DEV\|RUDOLF_WEBKEY_DIR')
ok "...and the program as released has no test switches (only /dev/tpmrm0 and /persist/etc/rudolf-webkey)" "$([ "$o" = 0 ] && strings $T/wk-rel | grep -q '^/dev/tpmrm0$' && echo 1)" "$o"

# ---- the key
tpm_up
o=$($T/wk status); ok "2. status: a TPM 2.0, PCR 7 and 14 read, nothing sealed yet" "$(echo "$o" | grep -q '"tpm":true,"family":"2.0"' && echo "$o" | grep -q '"sealed":false' && echo "$o" | grep -q '"pcr7":"[0-9a-f]\{64\}","pcr14":"[0-9a-f]\{64\}"' && echo 1)" "$o"
o=$(un); K1=$(key "$o")
ok "3. the first time: a new key (3), 32 characters of base64, said why" "$([ "$(rcof "$o")" = rc=3 ] && echo "$K1" | grep -qE '^[A-Za-z0-9+/]{32}$' && grep -q 'the first time' $T/err && echo 1)" "$o $(cat $T/err)"
ok "...the sealed file root's alone (0600 in a 0700 folder), the key not in it" "$([ "$(stat -c %a $T/wk.d)" = 700 ] && [ "$(stat -c %a $T/wk.d/sealed)" = 600 ] && ! grep -q "$K1" $T/wk.d/sealed && echo 1)" "$(ls -la $T/wk.d)"
o=$(un); ok "4. again: the same key (0)" "$([ "$(rcof "$o")" = rc=0 ] && [ "$(key "$o")" = "$K1" ] && echo 1)" "$o"
# tpm2-tools: the same object, opened by its own policy session over PCR 7 and 14; never by a password
python3 - $T/wk.d/sealed $T <<'EOF'
import sys
d = open(sys.argv[1], 'rb').read(); i = 7
n = int.from_bytes(d[i:i+2], 'big'); open(sys.argv[2] + '/priv', 'wb').write(d[i:i+2+n]); i += 2 + n
n = int.from_bytes(d[i:i+2], 'big'); open(sys.argv[2] + '/pub', 'wb').write(d[i:i+2+n])
EOF
tt() { tpm2_flushcontext -t >/dev/null 2>&1; tpm2_flushcontext -s >/dev/null 2>&1
	tpm2_createprimary -Q -C o -G ecc256:aes128cfb -g sha256 -a 'fixedtpm|fixedparent|sensitivedataorigin|userwithauth|noda|restricted|decrypt' -c $T/prim.ctx && tpm2_load -Q -C $T/prim.ctx -u $T/pub -r $T/priv -c $T/obj.ctx; }
tt 2>$T/tt.err; tpm2_flushcontext -t >/dev/null 2>&1; o=$(tpm2_unseal -c $T/obj.ctx 2>&1 | head -c 300 | od -An -c | head -2)
ok "5. the sealed object: no password opens it (no userWithAuth: tpm2-tools' tpm2_unseal with a password session refused)" "$(tpm2_unseal -c $T/obj.ctx >/dev/null 2>&1 || echo 1)" "$o $(cat $T/tt.err)"
tpm2_flushcontext -t >/dev/null 2>&1; tt; tpm2_flushcontext -t >/dev/null 2>&1; tpm2_startauthsession -Q --policy-session -S $T/s.ctx && tpm2_policypcr -Q -S $T/s.ctx -l sha256:7,14 && tpm2_unseal -c $T/obj.ctx -p session:$T/s.ctx -o $T/secret 2>$T/tt.err
tpm2_flushcontext $T/s.ctx >/dev/null 2>&1; tpm2_flushcontext -t >/dev/null 2>&1
D=$(python3 -c "
import sys,hmac,hashlib,base64
s=open('$T/secret','rb').read(); prk=hmac.new(b'rudolf-os website data',s,hashlib.sha256).digest()
okm=b''; t=b''; i=1
while len(okm)<24: t=hmac.new(prk,t+b'chromium safe storage'+bytes([i]),hashlib.sha256).digest(); okm+=t; i+=1
print(len(s), base64.b64encode(okm[:24]).decode())" 2>&1)
ok "6. ...and tpm2-tools' own policy session over PCR 7 and 14 opens it to a 32-byte secret whose HKDF is the key (another implementation agrees)" "$([ "$D" = "32 $K1" ] && echo 1)" "$D vs $K1 $(cat $T/tt.err)"
rm -f $T/secret
# Secure Boot's measurements
ext 7 1; o=$(un); K2=$(key "$o")
ok "7. PCR 7 changed (Secure Boot turned off or on, its keys): a new key (3), said why; the old one set aside" "$([ "$(rcof "$o")" = rc=3 ] && [ "$K2" != "$K1" ] && grep -q "Secure Boot's measurements changed" $T/err && [ -f $T/wk.d/sealed.old ] && echo 1)" "$o $(cat $T/err)"
o=$(un); ok "...then that one stays (0)" "$([ "$(rcof "$o")" = rc=0 ] && [ "$(key "$o")" = "$K2" ] && echo 1)" "$o"
ext 14 2; o=$(un); K3=$(key "$o")
ok "8. PCR 14 changed (shim's MOK list: a key enrolled or removed): a new key" "$([ "$(rcof "$o")" = rc=3 ] && [ "$K3" != "$K2" ] && echo 1)" "$o $(cat $T/err)"
for p in 0 1 2 3 4 5 6 8 9 11 12; do ext $p 3; done; o=$(un)
ok "9. the other PCRs changed (firmware and boot-loader updates, settings, Rudolf OS's own start-up files): the same key" "$([ "$(rcof "$o")" = rc=0 ] && [ "$(key "$o")" = "$K3" ] && echo 1)" "$o $(cat $T/err)"
# damaged, cleared, an owner's password
cp $T/wk.d/sealed $T/good; printf 'XXXX' | dd of=$T/wk.d/sealed bs=1 conv=notrunc 2>/dev/null; o=$(un)
ok "10. a damaged sealed file: a new key, said" "$([ "$(rcof "$o")" = rc=3 ] && grep -q 'damaged' $T/err && echo 1)" "$o $(cat $T/err)"
K4=$(key "$o"); tpm2_clear -c p >/dev/null 2>&1; o=$(un)
ok "11. the TPM cleared (a new storage seed: the sealed object is not this TPM's any more): a new key, said" "$([ "$(rcof "$o")" = rc=3 ] && [ "$(key "$o")" != "$K4" ] && grep -q 'cleared' $T/err && echo 1)" "$o $(cat $T/err)"
cp $T/wk.d/sealed $T/before; tpm2_changeauth -c o ownerpw >/dev/null 2>&1; o=$(un)
ok "12. the TPM's owner has a password (another system set one): 2, said, and nothing changed (the sealed file as it was)" "$([ "$(rcof "$o")" = rc=2 ] && grep -q 'owner has a password\|storage key could not be made' $T/err && cmp -s $T/before $T/wk.d/sealed && echo 1)" "$o $(cat $T/err)"
tpm2_changeauth -c o -p ownerpw >/dev/null 2>&1; o=$(un); K5=$(key "$o")
ok "...and once it has none again, the key as before (0)" "$([ "$(rcof "$o")" = rc=0 ] && echo 1)" "$o $(cat $T/err)"
tpm_down

# ---- the key store on a session bus
dbus-daemon --session --nofork --address=unix:path=$T/bus --print-address=1 > $T/bus.addr 2>/dev/null & BUS=$!; w8 "[ -s $T/bus.addr ]" 40
export DBUS_SESSION_BUS_ADDRESS=unix:path=$T/bus
echo "$K5" | $T/wk wallet > $T/wal.out 2>$T/wal.err & WAL=$!; w8 "grep -q ready $T/wal.out" 40
ok "13. the key store: KWallet's two names on the session bus" "$(grep -q ready $T/wal.out && grep -q org.kde.kwalletd6 $T/wal.err && grep -q org.kde.kwalletd5 $T/wal.err && echo 1)" "$(cat $T/wal.out $T/wal.err)"
c() { m=$1; shift; dbus-send --session --print-reply --dest=org.kde.kwalletd6 /modules/kwalletd6 org.kde.KWallet.$m "$@" 2>&1 | sed -n 2p | sed 's/^ *//'; }
o1=$(c isEnabled); o2=$(c readPassword int32:1 string:"Chromium Keys" string:"Chromium Safe Storage" string:Chromium); o3=$(c readPassword int32:1 string:"Chromium Keys" string:"Other" string:x)
o4=$(c readPassword int32:1 string:"Passwords" string:"Chromium Safe Storage" string:x); o5=$(c writePassword int32:1 string:"Chromium Keys" string:"Chromium Safe Storage" string:"another" string:x); o6=$(c writePassword int32:1 string:"X" string:"y" string:"z" string:x)
ok "14. ...gives Chromium's one entry (the key), nothing for any other entry or folder, and takes no other password" "$([ "$o1" = 'boolean true' ] && [ "$o2" = "string \"$K5\"" ] && [ "$o3" = 'string ""' ] && [ "$o4" = 'string ""' ] && [ "$o5" = 'int32 -1' ] && [ "$o6" = 'int32 -1' ] && echo 1)" "$o1|$o2|$o3|$o4|$o5|$o6"
o=$(c removeEntry int32:1 string:a string:b string:c 2>&1; dbus-send --session --print-reply --dest=org.kde.kwalletd6 /modules/kwalletd6 org.kde.KWallet.entryList int32:1 string:a string:b 2>&1 | head -2)
ok "...and nothing else (unknown methods refused)" "$(echo "$o" | grep -q 'UnknownMethod' && echo 1)" "$o"
o=$(grep -c "$K5" /proc/$WAL/environ /proc/$WAL/cmdline 2>/dev/null | grep -v ':0' | head -1); dmp=$(grep '^' /proc/$WAL/status | sed -n 's/^\(TracerPid\):\s*//p')
ok "...the key on its standard input only (not in its arguments or environment)" "$([ -z "$o" ] && echo 1)" "$o"

# ---- Rudolf's Chromium
CH=${CHROME:-/opt/chromium/chrome}
if [ -x "$CH" ]; then
	cat > $T/srv.py <<'EOF'
import http.server, sys
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(s):
        s.send_response(200)
        if s.path.startswith('/set'): s.send_header('Set-Cookie', 'sid=' + s.path[5:] + '; Max-Age=86400; Path=/')
        s.send_header('Content-Type', 'text/html'); s.end_headers(); s.wfile.write(('<html>cookie=[%s]</html>' % s.headers.get('Cookie', '')).encode())
    def log_message(s, *a): pass
http.server.HTTPServer(('127.0.0.1', 8623), H).serve_forever()
EOF
	python3 $T/srv.py & SRV=$!; w8 "curl -s http://127.0.0.1:8623/ >/dev/null" 40
	cr() { timeout 90 "$CH" --headless=new --no-sandbox --user-data-dir=$T/prof --password-store=$1 --no-first-run --no-default-browser-check --dump-dom "http://127.0.0.1:8623/$2" 2>/dev/null | grep -o 'cookie=\[[^]]*\]'; }
	cr basic set=kept-from-before >/dev/null; o=$($T/wk count $T/prof)
	ok "15. Chromium's built-in key (basic): the cookie is v10, as on every computer" "$([ "$o" = 'v10 1 v11 0' ] && echo 1)" "$o"
	o=$(echo "$K5" | $T/wk migrate $T/prof); o2=$($T/wk count $T/prof)
	ok "16. migrate: every v10 value encrypted again as v11 with the key" "$([ "$o2" = 'v10 0 v11 1' ] && echo 1)" "$o / $o2"
	o=$(cr kwallet6 show)
	ok "17. Chromium with the key store (kwallet6) reads the moved cookie" "$([ "$o" = 'cookie=[sid=kept-from-before]' ] && echo 1)" "$o"
	cr kwallet6 set=new-under-the-key >/dev/null; o=$($T/wk count $T/prof)
	P=$(python3 -c "
import sqlite3,hashlib,subprocess
k=hashlib.pbkdf2_hmac('sha1',b'$K5',b'saltysalt',1,16)
h,v=sqlite3.connect('$T/prof/Default/Cookies').execute('select host_key,encrypted_value from cookies').fetchone()
open('$T/ct','wb').write(v[3:])
p=subprocess.run(['openssl','enc','-d','-aes-128-cbc','-K',k.hex(),'-iv','20'*16,'-in','$T/ct'],capture_output=True).stdout
print(v[:3].decode(), p[:32]==hashlib.sha256(h.encode()).digest(), p[32:].decode())")
	ok "18. ...writes new cookies as v11, which open with Chromium's key made from ours (PBKDF2 of it, AES-128-CBC) and with nothing else" "$([ "$o" = 'v10 0 v11 1' ] && [ "$P" = 'v11 True new-under-the-key' ] && echo 1)" "$o / $P"
	kill $WAL; wait $WAL 2>/dev/null; WAL=; o=$(cr basic show)
	ok "19. a drive taken out (Chromium's built-in key, no key store): the cookie cannot be read" "$([ "$o" = 'cookie=[]' ] && echo 1)" "$o"
else echo "SKIP 15-19: no Rudolf Chromium at $CH (CHROME=...)"; fi

echo "$((N-F))/$N passed"; [ $F = 0 ]
