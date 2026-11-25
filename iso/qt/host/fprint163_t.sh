#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 2): fingerprint readers, rudolfd's side (rudolfd-fprint.inc, built from rudolfd.c with
# fixtures/fpapi.c) and the fprint root helper (rudolf-fprint.c, built with -DRR_TEST) with libfprint's virtual reader
# ("virtual_image": FP_VIRTUAL_IMAGE) fed made-up fingerprints (fixtures/vfinger.py: drawn from a number, never anyone's
# finger), on an ext4 drive with encryption as RUDOLFDATA has, in a private mount namespace. Accounts as the page makes
# them (fixtures/deskls.js: Ann, an administrator; Bob). Checked: the reader and a reader libfprint does not support
# named; adding fingers (the account's own password or an administrator's; never while its files are locked or not on
# the disk); the prints encrypted under the account's key (unreadable once it signs out); a finger matched; another
# account's finger refused; five fingers not recognised, then only the password; the first sign-in after a restart (and
# after signing out) refusing a finger; Guest none; cancelling; removing fingers; an account removed taking its prints
# with it; a release build of the helper ignoring the virtual reader.
# Needs root (a loop device), gcc, libfprint-2-dev, node, python3.   sh fprint163_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$FP163_INNER" ] && exec env FP163_INNER=1 unshare -m sh "$0" "$@"
pkg-config --exists libfprint-2 || { echo "FAIL libfprint-2-dev is missing (apt-get install libfprint-2-dev)"; exit 1; }
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/fprint163.XXXXXX); chmod 755 $T; PIDS=""
trap 'for p in $PIDS; do kill $p 2>/dev/null; done; umount $T/m 2>/dev/null; rm -rf "$T"' EXIT
mount -t tmpfs none /run; mkdir -p /run/rudolf
N=0; F=0
ok() { N=$((N+1)); if [ -n "$2" ] && [ -z "$(printf %s "$2" | tr -d 1)" ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
no() { case "$1" in *"$2"*) echo 0;; *) echo 1;; esac; }
truncate -s 96M $T/img; mkfs.ext4 -q -F -O encrypt $T/img; mkdir $T/m; mount -o loop $T/img $T/m || { echo "SKIP: no loop device"; exit 0; }
M=$T/m; mkdir -p $M/accounts $T/root $T/run $T/desk $T/proc $T/etc $T/usb/1-3 $T/usb/1-4 $T/usb/1-3:1.0
: > $T/etc/passwd; : > $T/etc/group; : > $T/etc/shadow; printf 'uAnnA1b2 59990\nuBobC3d4 59991\n' > $T/aumap
# a USB reader libfprint does not support (its own list's 04f3:036b) and a mouse, as the kernel's sysfs shows them
printf '04f3\n' > $T/usb/1-3/idVendor; printf '036b\n' > $T/usb/1-3/idProduct; printf 'ELAN\n' > $T/usb/1-3/manufacturer; printf 'Fingerprint Sensor\n' > $T/usb/1-3/product
printf '046d\n' > $T/usb/1-4/idVendor; printf 'c077\n' > $T/usb/1-4/idProduct; printf 'USB Optical Mouse\n' > $T/usb/1-4/product
printf '047d:00f2\n04f3:036b\n' > $T/unsup
cat > $T/conf <<EOF
RUDOLF_FP_ACCTS=$M/accounts
RUDOLF_FP_STORE=$M/fprint
RUDOLF_FP_RUN=$T/run
RUDOLF_FP_UNSUP=$T/unsup
RUDOLF_FP_USB=$T/usb
FP_VIRTUAL_IMAGE=$T/v.sock
RR_TEST_HEALTH=$T/health.log
EOF
FPL=$(pkg-config --cflags --libs libfprint-2)
gcc -O1 -w -DRR_TEST -DRR_TEST_CONF="\"$T/conf\"" -o $T/root/fprint $SYS/rudolf-fprint.c $FPL 2>$T/cc || { cat $T/cc; echo "FAIL build the helper"; exit 1; }
gcc -O1 -w -o $T/fprint-release $SYS/rudolf-fprint.c $FPL 2>$T/cc || { cat $T/cc; echo "FAIL build the helper (release)"; exit 1; }
gcc -O1 -w -DRD_ROOT_DIR="\"$T/root\"" -DAU_PROC="\"$T/proc\"" -o $T/api $HERE/fixtures/fpapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build rudolfd"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
j() { node -e "const k=require('$T/keys.json');console.log($1)"; }
j k.Ann.home > $T/annkey; j k.Bob.home > $T/bobkey; tr -d '\n' < $T/annkey > $T/annkey.b; tr -d '\n' < $T/bobkey > $T/bobkey.b
api() { $T/api $T/ls.json $T/desk /run/rudolf $M/accounts $T/fpstate $T/run $T/etc $T/aumap "$@" 2>>$T/api.err; }
body() { printf '%s' "$1" > $T/b; }
# a finger on the reader: the picture SEED sent TIMES times (the reader takes one per scan), in the background
finger() { python3 $HERE/fixtures/vfinger.py $T/v.sock $1 ${2:-1} ${3:-0.6} 2>/dev/null & PIDS="$PIDS $!"; }
B='"acct":"uBobC3d4"'
# ---- the reader
o=$(api "GET fp/status")
ok "1. the reader found (libfprint's virtual one here), and a USB reader libfprint does not support named plainly" "$(has "$o" '"helper":true')$(has "$o" '"driver":"virtual_image"')$(has "$o" '"unsupported":[{"usb":"04f3:036b","name":"ELAN Fingerprint Sensor"}]')$(no "$o" 'Mouse')" "$o"
o=$(api "GET fp/status?acct=uBobC3d4")
ok "2. Bob before signing in: his files not open, no fingers, a finger not taken" "$(has "$o" '"disk":false,"open":false,"fingers":[]')$(has "$o" '"ready":false')" "$o"
# ---- Bob signs in (the page's home/unlock with session=1, the file key unwrapped by his password)
o=$(api "POST home/unlock?acct=uBobC3d4&session=1 <$T/bobkey.b" "GET fp/status?acct=uBobC3d4")
ok "3. Bob signed in: his files open, signed in since the start" "$(has "$o" '200 OK|')$(has "$o" '"disk":true,"open":true')$(has "$o" '"signed":true')$(has "$o" '"why":"nofingers"')" "$o"
body "{$B,\"finger\":\"right-index\",\"n\":\"Bob\",\"p\":\"wrong\"}"; o=$(api "POST fp/enroll <$T/b")
ok "4. adding a finger with a wrong password: refused, the reader not asked" "$(has "$o" '403')$([ ! -e $M/fprint/uBobC3d4 ] && echo 1)" "$o"
finger 11 8; body "{$B,\"finger\":\"right-index\",\"n\":\"Bob\",\"p\":\"bob-pass\"}"; o=$(timeout 120 $T/api $T/ls.json $T/desk /run/rudolf $M/accounts $T/fpstate $T/run $T/etc $T/aumap "POST fp/enroll <$T/b" "GET fp/progress"); wait
ok "5. Bob adds his right index finger (the reader's 5 scans), with his own password" "$(has "$o" '"ok":true,"finger":"right-index"')$(has "$o" '"stage":5,"of":5')" "$o"
ok "...kept root's only (0600 in a 0700 folder), in a folder of its own" "$([ "$(stat -c '%u %a' $M/fprint/uBobC3d4/right-index)" = "0 600" ] && [ "$(stat -c '%u %a' $M/fprint/uBobC3d4)" = "0 700" ] && echo 1)" "$(ls -la $M/fprint $M/fprint/uBobC3d4)"
o=$(api "GET fp/status?acct=uBobC3d4")
ok "6. ...listed, and a finger is taken now" "$(has "$o" '"fingers":["right-index"]')$(has "$o" '"ready":true')" "$o"
body "{\"acct\":\"uAnnA1b2\",\"finger\":\"left-thumb\",\"n\":\"Ann\",\"p\":\"pw-ann ü\"}"; o=$(api "POST fp/enroll <$T/b")
ok "7. Ann's files are not on the disk: no finger can be added (409)" "$(has "$o" '409')$(has "$o" '"code":"disk"')" "$o"
body "{$B,\"finger\":\"left-index\",\"n\":\"Bob\",\"p\":\"bob-pass\"}"; finger 12 8; o=$(timeout 120 $T/api $T/ls.json $T/desk /run/rudolf $M/accounts $T/fpstate $T/run $T/etc $T/aumap "POST fp/enroll <$T/b"); wait
ok "8. a second finger (left index)" "$(has "$o" '"ok":true,"finger":"left-index"')" "$o"
# Ann signs in too, and adds hers; an administrator may add one to another account (Bob's ring finger)
o=$(api "POST home/unlock?acct=uAnnA1b2&session=1 <$T/annkey.b"); finger 21 8; body "{\"acct\":\"uAnnA1b2\",\"finger\":\"left-thumb\",\"n\":\"Ann\",\"p\":\"pw-ann ü\"}"
o=$(timeout 120 $T/api $T/ls.json $T/desk /run/rudolf $M/accounts $T/fpstate $T/run $T/etc $T/aumap "POST fp/enroll <$T/b"); wait
ok "9. Ann (her files on the disk now) adds her own finger" "$(has "$o" '"ok":true,"finger":"left-thumb"')" "$o"
body "{$B,\"finger\":\"right-ring\",\"n\":\"Ann\",\"p\":\"pw-ann ü\"}"; finger 13 8; o=$(timeout 120 $T/api $T/ls.json $T/desk /run/rudolf $M/accounts $T/fpstate $T/run $T/etc $T/aumap "POST fp/enroll <$T/b"); wait
body "{\"acct\":\"uAnnA1b2\",\"finger\":\"right-thumb\",\"n\":\"Bob\",\"p\":\"bob-pass\"}"; o2=$(api "POST fp/enroll <$T/b")
ok "10. an administrator adds one to another account; an account that is not one cannot" "$(has "$o" '"ok":true,"finger":"right-ring"')$(has "$o2" '403')$(has "$o2" 'Only the account itself or an administrator')" "$o $o2"
# ---- matching
v() { finger $1 1; body "{\"acct\":\"$2\",\"for\":\"${3:-unlock}\"}"; o=$(timeout 60 $T/api $T/ls.json $T/desk /run/rudolf $M/accounts $T/fpstate $T/run $T/etc $T/aumap "POST fp/verify <$T/b"); wait; echo "$o"; }
o=$(v 11 uBobC3d4); ok "11. Bob's right index finger unlocks Bob" "$(has "$o" '200 OK|{"ok":true,"acct":"uBobC3d4","n":"Bob","finger":"right-index"}')" "$o"
o=$(v 12 uBobC3d4); ok "...and his left index finger" "$(has "$o" '"finger":"left-index"')" "$o"
o=$(v 21 uBobC3d4); ok "12. Ann's finger is not Bob's: refused (403, four tries left)" "$(has "$o" '403')$(has "$o" '"left":4')" "$o"
o=$(v 11 uAnnA1b2); ok "...nor Bob's Ann's" "$(has "$o" '403')" "$o"
o=$(v 21 uAnnA1b2 admin); ok "13. an administrator's prompt: Ann's own finger answers it" "$(has "$o" '"ok":true,"acct":"uAnnA1b2","n":"Ann"')" "$o"
body "{$B,\"for\":\"admin\"}"; o=$(api "POST fp/verify <$T/b"); ok "...Bob, no administrator, cannot answer one (refused before the reader is asked)" "$(has "$o" '403')$(has "$o" 'administrator')" "$o"
# ---- five tries
o=$(v 30 uBobC3d4); o=$(v 31 uBobC3d4); o=$(v 32 uBobC3d4); ok "14. wrong fingers counted (one left after the fourth)" "$(has "$o" '"left":1')" "$o"
o=$(v 33 uBobC3d4); ok "...the fifth: the password is needed (423)" "$(has "$o" '423')$(has "$o" '"need":"password","why":"tries"')" "$o"
o=$(v 11 uBobC3d4); ok "...and then even his own finger is refused" "$(has "$o" '423')$(has "$o" '"why":"tries"')" "$o"
o=$(api "GET fp/status?acct=uBobC3d4"); ok "...which Settings shows" "$(has "$o" '"fails":5')$(has "$o" '"why":"tries"')" "$o"
body '{"n":"Bob","p":"nope"}'; o=$(api "POST fp/password <$T/b"); body '{"n":"Bob","p":"bob-pass"}'; o2=$(api "POST fp/password <$T/b")
o3=$(v 11 uBobC3d4)
ok "15. his password (checked here: a wrong one is not taken) gives the finger back" "$(has "$o" '403')$(has "$o2" '200 OK')$(has "$o3" '"ok":true,"acct":"uBobC3d4"')" "$o | $o2 | $o3"
grep -q 'a finger not recognised for the account Bob (5 of 5)' $T/health.log 2>/dev/null || grep -q 'a finger not recognised' /run/rudolf/health.log 2>/dev/null; hl=$?
ok "...the wrong fingers are in the health log" "$([ $hl = 0 ] && echo 1)" "$(cat /run/rudolf/health.log 2>/dev/null | tail -3)"
body '{"acct":"guest"}'; o=$(api "POST fp/verify <$T/b"); ok "16. Guest: no finger (423)" "$(has "$o" '423')$(has "$o" '"why":"guest"')" "$o"
# ---- cancelling: the lock screen's finger waiting, then the password typed instead
body "{$B}"; $T/api $T/ls.json $T/desk /run/rudolf $M/accounts $T/fpstate $T/run $T/etc $T/aumap "POST fp/verify <$T/b" > $T/cv 2>/dev/null & CP=$!; PIDS="$PIDS $CP"
timeout 2 tail -f /dev/null; o2=$(api "POST fp/cancel"); wait $CP; o=$(cat $T/cv)
ok "17. a waiting finger cancelled (409 cancelled), not counted" "$(has "$o" '409')$(has "$o" '"code":"cancelled"')$(has "$(api "GET fp/status?acct=uBobC3d4")" '"fails":0')" "$o $o2"
# ---- after a restart, and after signing out
mv $T/fpstate $T/fpstate.was
o=$(v 11 uBobC3d4); ok "18. the first sign-in after a restart: no finger, the password (423 restart), the reader not even asked" "$(has "$o" '423')$(has "$o" '"why":"restart"')" "$o"
mv $T/fpstate.was $T/fpstate
o=$(api "POST home/lock?acct=uBobC3d4")
ok "19. Bob signs out: his prints are unreadable, even to root (their names encrypted)" "$(has "$o" '200 OK')$(ls $M/fprint/uBobC3d4 | grep -qx right-index || echo 1)$(cat $M/fprint/uBobC3d4/* 2>/dev/null | grep -c . | grep -qx 0 && echo 1)" "$(ls $M/fprint/uBobC3d4)"
o=$(v 11 uBobC3d4); ok "...and a finger is refused (423 restart: signing out ended it)" "$(has "$o" '423')$(has "$o" '"why":"restart"')" "$o"
mkdir -p $T/fpstate; echo 1 > $T/fpstate/signed-uBobC3d4
o=$(v 11 uBobC3d4); ok "...even if rudolfd thought him signed in: his prints cannot be read without his password's key" "$(has "$o" '423')$(has "$o" '"code":"locked"')" "$o"
rm -f $T/fpstate/signed-uBobC3d4
o=$(api "POST home/unlock?acct=uBobC3d4 <$T/bobkey.b"); o2=$(v 11 uBobC3d4)
ok "20. his files opened without signing in (an administrator's copy of his key, session=0): still no finger" "$(has "$o" '200 OK')$(has "$o2" '423')$(has "$o2" '"why":"restart"')" "$o2"
o=$(api "POST home/unlock?acct=uBobC3d4&session=1 <$T/bobkey.b"); o2=$(v 11 uBobC3d4)
ok "...signed in again with his password: the finger again" "$(has "$o2" '"ok":true,"acct":"uBobC3d4"')" "$o2"
# Native's sign-in (desktop/login, rudolfd checking the password) counts as one too
rm -rf $T/fpstate; body '{"n":"Bob","p":"bob-pass"}'; o=$(api "POST desktop/login <$T/b" "GET fp/status?acct=uBobC3d4")
ok "21. Native's sign-in with the password: a finger taken from then on" "$(has "$o" '"signed":true')" "$o"
# ---- removing
body "{$B,\"finger\":\"right-index\",\"n\":\"Bob\",\"p\":\"bob-pass\"}"; o=$(api "POST fp/remove <$T/b" "GET fp/status?acct=uBobC3d4")
ok "22. Bob removes his right index finger" "$(has "$o" '"fingers":["left-index","right-ring"]')$([ ! -e $M/fprint/uBobC3d4/right-index ] && echo 1)" "$o"
o=$(v 11 uBobC3d4); ok "...which no longer unlocks him" "$(has "$o" '403')" "$o"
# ---- step 3: Native's lock screen and an administrator's finger where rudolfd checks the password
o=$(api "POST desktop/lock"); finger 12 1; body '{"f":true}'
o=$(timeout 60 $T/api $T/ls.json $T/desk /run/rudolf $M/accounts $T/fpstate $T/run $T/etc $T/aumap "POST desktop/unlock <$T/b" "GET desktop/session"); wait
ok "26. Native's lock screen: Bob's finger unlocks it (desktop/unlock with f)" "$(has "$o" '"message":"unlocked"')$(has "$(echo "$o" | tail -1)" '200 OK')" "$o"
o=$(api "POST desktop/lock"); finger 30 1
o=$(timeout 60 $T/api $T/ls.json $T/desk /run/rudolf $M/accounts $T/fpstate $T/run $T/etc $T/aumap "POST desktop/unlock <$T/b" "GET desktop/session"); wait
ok "...another finger: not recognised (403), the screen stays locked" "$(has "$o" '403')$(has "$(echo "$o" | tail -1)" '423')" "$o"
body '{"p":"bob-pass"}'; o=$(api "POST desktop/unlock <$T/b" "GET fp/status?acct=uBobC3d4")
ok "...his password unlocks it, and the count of fingers starts again" "$(has "$o" '"fails":0')" "$o"
# an authenticator app on Bob's account (the page's "otp" box: the secret sealed with his password's key)
node -e "const c=require('crypto'),f='$T/ls.json',l=require(f),u=JSON.parse(l.data.bd95user),k=require('$T/keys.json');const a=u.accts.find(x=>x.n=='Bob');
 const iv=c.randomBytes(12),ci=c.createCipheriv('aes-256-gcm',Buffer.from(k.Bob.pk,'hex'),iv),sec=Buffer.from('12345678901234567890');a.otp={iv:iv.toString('base64'),ct:Buffer.concat([ci.update(sec),ci.final(),ci.getAuthTag()]).toString('base64')};
 l.data.bd95user=JSON.stringify(u);require('fs').writeFileSync(f,JSON.stringify(l))"
totp() { node -e "const c=require('crypto');const t=Math.floor(Date.now()/30000)+($1);const b=Buffer.alloc(8);b.writeBigUInt64BE(BigInt(t));const h=c.createHmac('sha1','12345678901234567890').update(b).digest();const o=h[19]&15;console.log(String(((h.readUInt32BE(o)&0x7fffffff)%1000000)).padStart(6,'0'))"; }
o=$(api "POST desktop/lock"); finger 12 1; body '{"f":true}'
o=$(timeout 60 $T/api $T/ls.json $T/desk /run/rudolf $M/accounts $T/fpstate $T/run $T/etc $T/aumap "POST desktop/unlock <$T/b"); wait
FT=$(echo "$o" | sed -n 's/.*"ft":"\([0-9a-f]*\)".*/\1/p')
ok "27. two-step sign-in: after his finger, the code is still asked (401 need code, a one-time answer)" "$(has "$o" '401')$(has "$o" '"need":"code"')$([ ${#FT} = 32 ] && echo 1)" "$o"
body "{\"ft\":\"$FT\",\"c\":\"000000\"}"; o=$(api "POST desktop/unlock <$T/b" "GET desktop/session"); FT2=$(echo "$o" | head -1 | sed -n 's/.*"ft":"\([0-9a-f]*\)".*/\1/p')
ok "...a wrong code: refused, still locked, the finger not asked again (a new answer)" "$(has "$o" '403')$(has "$(echo "$o" | tail -1)" '423')$([ ${#FT2} = 32 ] && [ "$FT2" != "$FT" ] && echo 1)" "$o"
body "{\"ft\":\"$FT2\",\"c\":\"$(totp 0)\"}"; o=$(api "POST desktop/unlock <$T/b" "GET desktop/session")
ok "...the right code: unlocked" "$(has "$o" '"message":"unlocked"')$(has "$(echo "$o" | tail -1)" '200 OK')" "$o"
o=$(api "POST desktop/lock"); body "{\"ft\":\"$FT\",\"c\":\"$(totp 0)\"}"; o=$(api "POST desktop/unlock <$T/b")
ok "...an answer cannot be used again (nor an older one)" "$(has "$o" '403')$(has "$o" 'time ran out')" "$o"
body '{"p":"bob-pass"}'; o=$(api "POST desktop/unlock <$T/b")
ok "...and his password alone is not enough either (the code asked, as before)" "$(has "$o" '"need":"code"')" "$o"
body "{\"p\":\"bob-pass\",\"c\":\"$(totp 1)\"}"; o=$(api "POST desktop/unlock <$T/b")
# an administrator's finger in place of the password where rudolfd checks it (desktop/admin, desk_admin_check)
o=$(api "POST home/unlock?acct=uAnnA1b2&session=1 <$T/annkey.b"); o=$(v 21 uAnnA1b2 admin); T1=$(echo "$o" | sed -n 's/.*"t":"\([0-9a-f]*\)".*/\1/p')
ok "28. an administrator's finger gives a one-time answer for rudolfd's own checks" "$(has "$o" '"ok":true,"acct":"uAnnA1b2"')$([ ${#T1} = 32 ] && echo 1)" "$o"
body "{\"n\":\"Ann\",\"p\":\"fp:$T1\"}"; o=$(api "POST desktop/admin <$T/b" "POST desktop/admin <$T/b")
ok "...it allows once (desktop/admin), and never again" "$(has "$(echo "$o" | head -1)" '"message":"allowed"')$(has "$(echo "$o" | tail -1)" '403')" "$o"
o=$(v 21 uAnnA1b2 admin); T1=$(echo "$o" | sed -n 's/.*"t":"\([0-9a-f]*\)".*/\1/p'); body "{\"n\":\"Bob\",\"p\":\"fp:$T1\"}"; o=$(api "POST desktop/admin <$T/b")
ok "...not in another account's name" "$(has "$o" '403')" "$o"
# ---- an account removed: its prints with it
o=$(api "gone uBobC3d4")
ok "23. Ann's account removed: her prints removed with it (her folder locked or not)" "$(has "$o" 'gone 1')$([ ! -e $M/fprint/uAnnA1b2 ] && echo 1)$([ -d $M/fprint/uBobC3d4 ] && echo 1)" "$o $(ls -la $M/fprint)"
o=$(api "POST home/unlock?acct=uAnnA1b2&session=1 <$T/annkey.b" "GET fp/status?acct=uAnnA1b2")
ok "...none left for her (her files open again: no fingers)" "$(has "$o" '"fingers":[]')$(has "$o" '"why":"nofingers"')" "$o"
grep -q 'fingerprints of the removed account uAnnA1b2 were removed' /run/rudolf/health.log 2>/dev/null; hl=$?
ok "...said in the health log" "$([ $hl = 0 ] && echo 1)" "$(tail -3 /run/rudolf/health.log 2>/dev/null)"
# ---- the release build: the virtual reader is a test's only
o=$(env FP_VIRTUAL_IMAGE=$T/v.sock $T/fprint-release devices)
ok "24. a release build of the helper ignores the virtual reader (FP_VIRTUAL_IMAGE in its environment)" "$(has "$o" '"readers":[]')" "$o"
o=$(grep -c 'listen\|bind(' $SYS/rudolf-fprint.c)
ok "25. the helper listens on nothing (no listen or bind in it)" "$([ "$o" = 0 ] && echo 1)" "$o"
echo "$((N - F))/$N passed"
[ $F = 0 ]
