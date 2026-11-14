#!/bin/sh
# 1.5.9 (plan Part 12, step 2): rudolfd's desktop/* requests for the native desktop engine, compiled from rudolfd.c: its
# cryptography against Node's WebCrypto (the browser's), accounts made exactly as the page makes them (fixtures/
# deskls.js), the sign-in (a wrong password refused and noted, the right one giving the page's own keys, five failures
# waiting, an account on the disk opening its encrypted folder with the page's key), the session for one-app pages,
# lock and unlock, logout locking the folder, the settings, pins and notifications' count of the signed-in account,
# and the files the page gives the shell. Needs root (a loop device with ext4's encryption, a private mount namespace)
# and node.   sh deskapi59_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$DAT_INNER" ] && exec env DAT_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/deskapi.XXXXXX)
mount -t tmpfs none /run; mkdir -p /run/rudolf
gcc -O2 -w -o $T/t $HERE/fixtures/deskapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
truncate -s 64M $T/img; mkfs.ext4 -q -F -O encrypt $T/img; mkdir $T/m; mount -o loop $T/img $T/m || { echo "SKIP: no loop device"; exit 0; }
R=$T/m/accounts; D=$T/home; RUNX=$T/run; mkdir -p $R $D $RUNX
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
t() { $T/t $R $T/ls.json $D $RUNX "$@"; }
j() { node -e "const k=require('$T/$1');console.log($2)"; }
body() { printf '%s' "$1" > $T/b; }
# the cryptography
V=$T/keys.vec.json
ok "1. SHA-256 and SHA-512 as WebCrypto's" "$([ "$($T/t crypto sha256 abc)" = "$(j keys.vec.json k.sha256)" ] && [ "$($T/t crypto sha512 abc)" = "$(j keys.vec.json k.sha512)" ] && echo 1)"
ok "2. PBKDF2-SHA-256 (210 000 rounds) as WebCrypto's" "$([ "$($T/t crypto pbkdf2 password c2FsdHNhbHQ= 210000)" = "$(j keys.vec.json k.pbkdf2)" ] && echo 1)" "$($T/t crypto pbkdf2 password c2FsdHNhbHQ= 210000)"
ok "3. HKDF-SHA-512 as WebCrypto's (the home folder's key)" "$([ "$($T/t crypto hkdf $(printf '07%.0s' $(seq 32)) 'rudolf-os home folder' uX)" = "$(j keys.vec.json k.hkdf)" ] && echo 1)"
G=$($T/t crypto gcm $(j keys.vec.json k.gcmkey) $(j keys.vec.json k.gcmiv) $(j keys.vec.json k.gcmct))
ok "4. AES-256-GCM opens WebCrypto's box (41 bytes: a partial block)" "$([ "$G" = "$(j keys.vec.json k.gcmpt)" ] && echo 1)" "$G"
BAD=$(j keys.vec.json "k.gcmct.replace(/^./,c=>c=='A'?'B':'A')")
ok "5. ...and refuses it when changed (the tag)" "$([ "$($T/t crypto gcm $(j keys.vec.json k.gcmkey) $(j keys.vec.json k.gcmiv) $BAD)" = FAIL ] && echo 1)"
# accounts
o=$(t "GET accounts")
ok "6. accounts: names, roles and ids, the guest allowed, the last account" "$(echo "$o" | sed 's/^[^|]*|//' | node -e "const j=JSON.parse(require('fs').readFileSync(0));const a=j.accounts;console.log(a.length==4&&a[0].n=='Ann'&&a[0].r=='admin'&&a[1].id=='uBobC3d4'&&a[2].old&&a[3].key&&j.guest&&j.last=='uBobC3d4'&&j.setup?1:0)")" "$o"
ok "7. ...never a hash, salt or key" "$(echo "$o" | grep -q 'p2\$\|"h"\|"ks"\|"fk"\|aaaaaaaa' && echo 0 || echo 1)" "$o"
# sign-in
body '{"n":"Ann","p":"wrong"}'; o=$(t "POST login <$T/b")
ok "8. a wrong password is refused" "$(has "$o" '403')" "$o"
ok "9. ...and there is no session" "$(has "$(t 'GET session')" 404)"
body '{"n":"Ann","p":"pw-ann ü"}'; o=$(t "POST login <$T/b")
ok "10. the right password (not ASCII) signs in" "$(has "$o" '"id":"uAnnA1b2"')" "$o"
s=$(t "GET session" | sed 's/^[^|]*|//')
ok "11. the session gives the page's own keys (the password's key and the file key)" "$([ "$(echo "$s" | node -e "const s=JSON.parse(require('fs').readFileSync(0)),k=require('$T/keys.json').Ann;console.log(s.pk==k.pk&&s.fk==k.fk&&s.n=='Ann'&&s.r=='admin'?1:0)")" = 1 ] && echo 1)" "$s"
ok "12. ...kept root-only" "$([ "$(stat -c %a $RUNX/native-session)" = 600 ] && echo 1)"
body '{"n":"ann","p":"pw-ann ü"}'; ok "13. the name in any case, as the page" "$(has "$(t "POST login <$T/b")" '"n":"Ann"')"
o=$(t "GET settings" | sed 's/^[^|]*|//')
ok "14. settings: the account's own (bd95set:ID), only the shell's keys, the pins and the notifications' count" "$(echo "$o" | node -e "const j=JSON.parse(require('fs').readFileSync(0));const s=j.s;console.log(s.b1=='#112233'&&s.tb=='top'&&s.tbstyle=='candy'&&s.tbac===true&&s.twacm=='negative'&&s.h24===true&&s.dyn=='alpine'&&!('ignored' in s)&&j.pins.join()=='notepad,calc'&&j.notes==3&&j.gen==42&&j.acct=='uAnnA1b2'?1:0)")" "$o"
# lock and unlock
o=$(t "POST lock" "GET session")
ok "15. locked: the session is not given to new app windows" "$(echo "$o" | sed -n 2p | grep -c ' 423 ')" "$o"
body '{"p":"nope"}'; o=$(t "POST unlock <$T/b"); ok "16. unlock with a wrong password: refused" "$(has "$o" 403)" "$o"
body '{"p":"pw-ann ü"}'; o=$(t "POST unlock <$T/b" "GET session")
ok "17. ...with the right one: unlocked" "$(echo "$o" | sed -n 2p | grep -c '"pk"')" "$o"
o=$(t "POST logout" "GET session" "GET settings")
ok "18. logout ends the session; settings are the last ones again (bd95set)" "$(echo "$o" | sed -n 2p | grep -q ' 404 ' && echo "$o" | sed -n 3p | grep -q '"b1":"#445566"' && echo 1)" "$o"
# five failures
body '{"n":"Ann","p":"x"}'; o=$(t "POST login <$T/b" "POST login <$T/b" "POST login <$T/b" "POST login <$T/b" "POST login <$T/b" "POST login <$T/b")
ok "19. five wrong passwords: then a wait, as the page's sign-in screen" "$(echo "$o" | sed -n 6p | grep -c '429.*wait')" "$o"
body '{"n":"Ann","p":"pw-ann ü"}'; ok "20. ...even with the right password, until it is over" "$(has "$(t "POST login <$T/b")" 429)"
rm -f $RUNX/native-login-fails
# refused accounts
body '{"n":"Old","p":"x"}'; ok "21. a password from before 1.4: use Standard once" "$(has "$(t "POST login <$T/b")" 'older Rudolf OS')"
body '{"n":"Keyed","p":"k"}'; ok "22. an account with a security key: use Standard" "$(has "$(t "POST login <$T/b")" 'security key')"
body '{"n":"Nobody","p":"x"}'; ok "23. an account that does not exist: the same message as a wrong password" "$(has "$(t "POST login <$T/b")" 'user name or password is incorrect')"
body '{"guest":true}'; o=$(t "POST login <$T/b" "GET session"); ok "24. the guest signs in without a password or keys" "$(echo "$o" | sed -n 2p | grep -c '"r":"guest","pk":"","fk":""')" "$o"
t "POST logout" >/dev/null
# an account whose files are on the disk
body '{"n":"Bob","p":"bob-pass"}'; o=$(t "POST login <$T/b")
ok "25. an account on the disk: its encrypted folder is made and opened" "$([ "$(has "$o" '"id":"uBobC3d4"')" = 1 ] && [ -d $R/uBobC3d4/Documents ] && echo 1)" "$o"
echo hello > $R/uBobC3d4/Documents/a.txt
KHEX=$(j keys.json k.Bob.home); printf %s "$KHEX" > $T/k
o=$($T/t $R $T/ls.json $D $RUNX "POST logout"); n=$(ls $R/uBobC3d4/Documents 2>/dev/null)
ok "26. ...with the page's key: locked at logout, then opened by the key home56.js makes" "$([ "$n" != a.txt ] && echo 1)" "$n"
cat > $T/hk.c <<'EOF'
#define main rudolfd_main
#include "HERE/../../sys/rudolfd.c"
#undef main
int main(int c, char **v) { snprintf(home_root, sizeof home_root, "%s", v[1]); home_nobind = 1; unsigned char k[64]; for (int i = 0; i < 64; i++) k[i] = hexval(v[2][2 * i]) << 4 | hexval(v[2][2 * i + 1]); char e[200] = ""; int m = 0; int r = home_unlock("uBobC3d4", k, 0, e, sizeof e, &m); printf("%d %s\n", r, e); return r; }
EOF
sed -i "s|HERE|$HERE|" $T/hk.c; gcc -O1 -w -o $T/hk $T/hk.c -lz && $T/hk $R $KHEX >/dev/null
ok "27. ...the files are there again (the key is the page's)" "$([ "$(cat $R/uBobC3d4/Documents/a.txt 2>/dev/null)" = hello ] && echo 1)"
# the page's files for the shell
printf '{"apps":[{"k":"notepad","name":"Notepad","icon":"N"}],"bg":{"alpine":{"d":"a/x.webp","l":"a/y.webp"}}}' > $T/apps
o=$(t "POST apps <$T/apps" "GET apps"); ok "28. the start menu's list: written by the page, read by the shell" "$(echo "$o" | sed -n 2p | grep -c '"k":"notepad"')" "$o"
printf '{"apps":' > $T/bad; ok "29. ...only whole JSON" "$(has "$(t "POST apps <$T/bad")" 400)"
printf '\377\330\377\340fakejpeg' > $T/jpg; o=$(t "POST wallpaper?acct=uAnnA1b2 <$T/jpg" "GET wallpaper?acct=uAnnA1b2")
ok "30. a picture of your own: kept for the shell" "$(echo "$o" | sed -n 2p | grep -c fakejpeg)" "$o"
ok "31. ...only a JPEG, for a real account id" "$([ "$(has "$(t "POST wallpaper?acct=uAnnA1b2 <$T/apps")" 400)$(has "$(t "GET wallpaper?acct=../x")" 400)" = 11 ] && echo 1)"
ok "32. other desktop/ requests are left to rudolfd (engine, priority)" "$(has "$(t "GET engine")" 'not a desktop request')"
# 1.5.9 step 7: the picture of your own made from the page's database mirror (idbm), as the page names its records
rm -rf $D/.rudolf-wallpaper; mkdir -p $D/.rudolf-idb
node -e "
const n=(db,st,key)=>{let h=0xcbf29ce484222325n;const b=new TextEncoder().encode(db+'\0'+st+'\0'+JSON.stringify(key));for(const c of b){h^=BigInt(c);h=(h*0x100000001b3n)&0xffffffffffffffffn}return h.toString(16).padStart(16,'0')};
const fs=require('fs'),jpg=Buffer.from([0xff,0xd8,0xff,0xe0,...Buffer.from('mirrored picture')]),j2=Buffer.from([0xff,0xd8,0xff,0xe0,...Buffer.from('second picture')]);
fs.writeFileSync('$D/.rudolf-idb/'+n('rudolfos','kv','wp:mp1abc')+'.json',JSON.stringify({db:'rudolfos',store:'kv',key:'wp:mp1abc',v:'data:image/jpeg;base64,'+jpg.toString('base64')}));
fs.writeFileSync('$T/second.jpg',j2)"
body '{"n":"Ann","p":"pw-ann ü"}'; t "POST login <$T/b" >/dev/null
o=$(t "GET settings" | sed 's/^[^|]*|//')
ok "33. a picture of your own that no page has given yet: made from the page's database mirror (the first start of Native)" "$(echo "$o" | grep -q '"wallpaper":true,"wpacct":"uAnnA1b2"' && grep -q 'mirrored picture' $D/.rudolf-wallpaper/uAnnA1b2.jpg && [ "$(cat $D/.rudolf-wallpaper/uAnnA1b2.img)" = mp1abc ] && echo 1)" "$o"
o=$(t "GET wallpaper?acct=uAnnA1b2"); ok "34. ...and given to the shell; the display scale is in the settings" "$([ "$(has "$o" 'mirrored picture')" = 1 ] && t "GET settings" | grep -q '"scale":125' && echo 1)" "$o"
o=$(t "POST wallpaper?acct=uAnnA1b2&img=other1 <$T/second.jpg" "GET wallpaper?acct=uAnnA1b2")
ok "35. a copy of another picture (the page changed it, the settings not yet written) is replaced by the settings' picture" "$(echo "$o" | sed -n 2p | grep -c 'mirrored picture')" "$o"
o=$(t "POST logout" "GET settings" | sed -n 2p); ok "36. before sign-in: the last account's picture only if the last settings (bd95set) show it" "$(has "$o" '"wallpaper":false,"wpacct":""')" "$o"
umount $T/m; rm -rf $T
echo "$((N-F))/$N passed"; [ $F = 0 ]
