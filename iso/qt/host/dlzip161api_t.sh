#!/bin/sh
# 1.6.1: rudolfd's dl/zip (what is inside a ZIP kept on the disk), dl/unzip (Extract here, a job; Cancel), dl/unzipped (the
# whole result) and dl/home's "unzip" while one runs, compiled from rudolfd.c (fixtures/dlzipapi.c) with dlzip.sh; refused: other
# places, names with "/", "..", control characters, a link, an account's folder that is a link, a folder, not a ZIP, GET for
# unzip. Made-up ZIPs. Needs root (a private /run/rudolf, the desktop user, uid 1000, to unpack as).   sh dlzip161api_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$DLZ_INNER" ] && exec env DLZ_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/dlzip161api.XXXXXX); trap 'rm -rf $T' EXIT; chmod 755 $T
mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run; mkdir -p /run/rudolf
U1000=$(getent passwd 1000 | cut -d: -f1)
if [ -z "$U1000" ]; then [ -n "$CI" ] && useradd -u 1000 -M -s /usr/sbin/nologin rudolfzip && U1000=rudolfzip || { echo "SKIP: no user with uid 1000 (the desktop user's)"; exit 0; }; fi
export DLZIP_USER=$U1000 DLZIP_MARGIN_KB=0
gcc -O1 -w -o $T/t $HERE/fixtures/dlzipapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
cp $HERE/../../sys/root/usr/lib/rudolf/dlzip.sh $T/dlzip.sh
S=$T/dl; A=$S/.accounts/u-ann; mkdir -p $A $S/.accounts/u-bob /tmp/dlz-elsewhere-$$; chown -R 1000:1000 $S; chmod 700 $S/.accounts $A
python3 -I - $A <<'EOF'
import sys, zipfile, os
d = sys.argv[1]
with zipfile.ZipFile(d + '/game.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('Made Up Game/', ''); z.writestr('Made Up Game/readme.txt', 'Read me\n'); z.writestr('Made Up Game/data/level.dat', os.urandom(4000))
    z.writestr('Made Up Game/"quoted" name.txt', 'q\n')
with zipfile.ZipFile(d + '/many.zip', 'w') as z:
    for k in range(20003): z.writestr('f/%05d.txt' % k, 'x')
open(d + '/notzip.zip', 'w').write('not a zip\n')
EOF
cp $A/game.zip /tmp/dlz-elsewhere-$$/game.zip; ln -s /tmp/dlz-elsewhere-$$/game.zip $A/link.zip; mkdir $A/folder.zip; ln -s /tmp/dlz-elsewhere-$$ $S/.accounts/u-evil
chown -R 1000:1000 $A; trap 'rm -rf $T /tmp/dlz-elsewhere-$$' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
t() { $T/t $S $T/dlzip.sh "$@"; }
o=$(t 'GET zip?name=game.zip&acct=u-ann')
ok "1. dl/zip: what is inside (names, sizes, folders, nested), the totals" "$(echo "$o" | grep -q '^HTTP/1.1 200 OK|{"ok":true,"items":\[{"name":"Made Up Game/","size":0,"dir":true},' && echo "$o" | grep -q '{"name":"Made Up Game/data/level.dat","size":4000,"dir":false}' && echo "$o" | grep -q '"name":"Made Up Game/\\"quoted\\" name.txt"' && echo "$o" | grep -q '"more":false,"total":4,"bytes":4010}' && echo 1)" "$o"
o=$(DLZIPAPI_RAW=$T/raw t 'GET zip?name=many.zip&acct=u-ann'); c=$(grep -o '"name":' $T/raw | wc -l)
ok "2. ...at most 20,000 entries, then more" "$([ "$c" = 20000 ] && grep -q '"more":true,"total":20003,"bytes":20003}$' $T/raw && echo 1)" "$c $(tail -c 100 $T/raw)"
o=$(t 'GET zip?name=../game.zip&acct=u-ann' 'GET zip?name=.hidden&acct=u-ann' 'GET zip?name=game.zip&acct=U-Ann' 'GET zip?name=game.zip&acct=' 'GET zip?name=a%0Ab.zip&acct=u-ann' 'GET zip?name=game.zip&acct=../u-ann')
ok "3. names with / or .., hidden ones, control characters, a bad account or none: refused" "$([ "$(echo "$o" | grep -c '^HTTP/1.1 400')" = 6 ] && echo 1)" "$o"
o=$(t 'GET zip?name=link.zip&acct=u-ann' 'GET zip?name=game.zip&acct=u-evil')
ok "4. a link to a ZIP elsewhere, an account's folder that is a link: refused" "$([ "$(echo "$o" | grep -c '^HTTP/1.1 403')" = 2 ] && echo 1)" "$o"
o=$(t 'GET zip?name=folder.zip&acct=u-ann' 'GET zip?name=gone.zip&acct=u-ann' 'GET zip?name=notzip.zip&acct=u-ann')
ok "5. a folder, a file not there: not found; not a ZIP: said so" "$(echo "$o" | sed -n 1p | grep -q '^HTTP/1.1 404' && echo "$o" | sed -n 2p | grep -q '^HTTP/1.1 404' && echo "$o" | sed -n 3p | grep -q '^HTTP/1.1 422 .*not a ZIP archive' && echo 1)" "$o"
o=$(t 'GET unzip?name=game.zip&acct=u-ann' 'POST unzip?name=link.zip&acct=u-ann' 'POST unzip?stop=1')
ok "6. dl/unzip: GET refused, a link refused, Cancel with nothing running refused" "$(echo "$o" | sed -n 1p | grep -q '^HTTP/1.1 400' && echo "$o" | sed -n 2p | grep -q '^HTTP/1.1 403' && echo "$o" | sed -n 3p | grep -q '^HTTP/1.1 409' && echo 1)" "$o"
o=$(t 'POST unzip?name=game.zip&acct=u-ann' 'GET home?acct=u-ann' 'GET home?acct=u-bob' wait 'GET unzipped' 'GET home?acct=u-ann')
ok "7. Extract here: a job started; dl/home says unzip for that account only" "$(echo "$o" | sed -n 1p | grep -q '^HTTP/1.1 200' && echo "$o" | sed -n 2p | grep -q '"unzip":true}$' && ! echo "$o" | sed -n 3p | grep -q unzip && echo 1)" "$o"
ok "8. dl/unzipped: the folders and every file (its path inside, its name on the disk, its size), how it ended" "$(echo "$o" | sed -n 5p | grep -q '"dirs":\["Made Up Game","Made Up Game/data"\]' && echo "$o" | sed -n 5p | grep -q '{"path":"Made Up Game/data/level.dat","disk":"level.dat","size":4000}' && echo "$o" | sed -n 5p | grep -q '"end":"done 3 4010"}' && echo 1)" "$(echo "$o" | sed -n 5p)"
ok "9. ...the files flat in the account's folder, the desktop user's, the hidden folder gone" "$([ -f $A/level.dat ] && [ -f $A/readme.txt ] && [ "$(stat -c %u $A/level.dat)" = 1000 ] && [ -z "$(ls -A $A | grep '^\.')" ] && echo 1)" "$(ls -lA $A)"
ok "10. ...until the page has taken the result, dl/home still says unzip" "$(echo "$o" | sed -n 6p | grep -q '"unzip":true}$' && echo 1)" "$(echo "$o" | sed -n 6p)"
o=$(t 'POST unzipped' 'GET home?acct=u-ann' 'GET unzipped')
ok "11. taken: forgotten (dl/home lists them as before)" "$(echo "$o" | sed -n 2p | grep -q '"name":"level.dat"' && ! echo "$o" | sed -n 2p | grep -q unzip && echo "$o" | sed -n 3p | grep -q '"files":\[\],"end":""}' && echo 1)" "$o"
o=$(t 'POST unzip?name=many.zip&acct=u-ann' 'POST unzip?name=game.zip&acct=u-ann' 'POST unzip?stop=1' wait 'GET unzipped')
ok "12. a second one while one runs: busy; Cancel stops it" "$(echo "$o" | sed -n 2p | grep -q '^HTTP/1.1 409' && echo "$o" | sed -n 3p | grep -q '^HTTP/1.1 200' && echo "$o" | sed -n 5p | grep -q '"end":"stopped ' && [ -z "$(ls -A $A | grep '^\.')" ] && echo 1)" "$o"
ok "13. the request file is root's and gone; the ZIP is never changed" "$([ ! -e /run/rudolf/dlzip.list ] && cmp -s $A/game.zip /tmp/dlz-elsewhere-$$/game.zip && echo 1)"
ok "14. an account folder that is a link: its target not given to the desktop user (lchown)" "$([ "$(stat -c %u /tmp/dlz-elsewhere-$$)" = 0 ] && echo 1)"
ok "15. no command line made from a name (rudolfd-dlzip.inc: exec arguments, a fixed command for the job)" "$(! grep -n 'run(\|system(\|popen(' $HERE/../../sys/rudolfd-dlzip.inc >/dev/null && echo 1)" "$(grep -n 'run(\|system(\|popen(' $HERE/../../sys/rudolfd-dlzip.inc)"
# (1.6.3 Part 10, item 1: a ZIP at a path the account may read, not only the account's Downloads folder)
t 'POST unzipped' >/dev/null
O=$T/beside; rm -rf $O; mkdir -p $O; cp $A/game.zip $O/game.zip; chown -R 1000:1000 $O; ln -s /etc/hostname $O/etc.zip
o=$(t "GET zip?path=$O/game.zip")
ok "16. dl/zip: a ZIP at a path outside Downloads is listed as the same answer" "$(echo "$o" | grep -q '^HTTP/1.1 200 OK|{"ok":true,"items":\[{"name":"Made Up Game/","size":0,"dir":true},' && echo "$o" | grep -q '"more":false,"total":4,"bytes":4010}$' && echo 1)" "$o"
o=$(t 'GET zip?path=game.zip' "GET zip?path=$T/gone-$$.zip" "GET zip?path=$O" 'GET zip?path=/etc/hostname' "GET zip?path=$O/etc.zip")
ok "17. ...a path that is not absolute, one not there, a folder, one outside the account's places, a link to one: refused" "$(echo "$o" | sed -n 1p | grep -q '^HTTP/1.1 400' && echo "$o" | sed -n 2p | grep -q '^HTTP/1.1 404' && echo "$o" | sed -n 3p | grep -q '^HTTP/1.1 404' && echo "$o" | sed -n 4p | grep -q '^HTTP/1.1 403' && echo "$o" | sed -n 5p | grep -q '^HTTP/1.1 403' && echo 1)" "$o"
o=$(t "POST unzip?path=$O/game.zip" wait 'GET unzipped' 'GET home?acct=u-ann')
ok "18. dl/unzip: a ZIP at a path unpacks into a new folder beside it, named after it, as the desktop user" "$(echo "$o" | sed -n 1p | grep -q '^HTTP/1.1 200' && echo "$o" | sed -n 3p | grep -q "\"dest\":\"$O/game\"" && echo "$o" | sed -n 3p | grep -q '"end":"done 3 4010"}' && [ -f "$O/game/Made Up Game/data/level.dat" ] && [ -d "$O/game/Made Up Game/data" ] && [ -f "$O/game.zip" ] && [ "$(stat -c %u "$O/game/Made Up Game/data/level.dat")" = 1000 ] && echo 1)" "$o $(ls -ln $O)"
ok "19. ...each folder and file with its path in that folder" "$(echo "$o" | sed -n 3p | grep -q '"dirs":\["Made Up Game","Made Up Game/data"\]' && echo "$o" | sed -n 3p | grep -q '{"path":"Made Up Game/data/level.dat","disk":"Made Up Game/data/level.dat","size":4000}' && echo 1)" "$(echo "$o" | sed -n 3p)"
ok "20. ...no account is marked as unzipping, and the Downloads folder is not touched" "$(! echo "$o" | sed -n 4p | grep -q unzip && [ ! -e "$A/game" ] && echo 1)" "$(echo "$o" | sed -n 4p)"
o=$(t "POST unzip?path=$O/game.zip" wait 'GET unzipped')
ok "21. ...a second time: the next free folder beside it (game (2))" "$([ -d "$O/game (2)" ] && echo "$o" | sed -n 3p | grep -q "\"dest\":\"$O/game (2)\"" && [ -f "$O/game (2)/Made Up Game/readme.txt" ] && echo 1)" "$o $(ls $O)"
o=$(t "GET unzip?path=$O/game.zip" 'POST unzip?path=relative.zip' 'POST unzip?path=/')
ok "22. dl/unzip: GET refused; a relative path and the root refused" "$(echo "$o" | sed -n 1p | grep -q '^HTTP/1.1 400' && echo "$o" | sed -n 2p | grep -q '^HTTP/1.1 400' && echo "$o" | sed -n 3p | grep -q '^HTTP/1.1 403' && echo 1)" "$o"
# (1.6.3 Part 10, item 1, security: an account's Downloads is the signed-in account's own -- never only a name of the right
# shape, which any window could say -- and a path never leads into one of the hidden folders the accounts' files live in)
cp $A/game.zip $S/.accounts/u-bob/game.zip; chown 1000:1000 $S/.accounts/u-bob/game.zip
echo u-ann > /run/rudolf/home-session
o=$(t 'GET home?acct=u-bob' 'GET home?acct=u-ann' 'GET zip?name=game.zip&acct=u-bob' 'GET read?name=game.zip&acct=u-bob&from=home')
ok "23. signed in as u-ann: another account's Downloads (its list, its ZIP, its bytes) refused; u-ann's own listed" "$(echo "$o" | sed -n 1p | grep -q '^HTTP/1.1 403' && echo "$o" | sed -n 2p | grep -q '^HTTP/1.1 200' && echo "$o" | sed -n 2p | grep -q 'game.zip' && echo "$o" | sed -n 3p | grep -q '^HTTP/1.1 403' && echo "$o" | sed -n 4p | grep -q '^HTTP/1.1 403' && echo 1)" "$o"
o=$(t "GET zip?path=$S/.accounts/u-bob/game.zip")
ok "24. ...and never by path: the hidden store is not a place a path may name" "$(echo "$o" | grep -q '^HTTP/1.1 403' && [ -f "$S/.accounts/u-bob/game.zip" ] && echo 1)" "$o"
o=$(t 'GET home?acct=guest' 'GET home?acct=shared')
ok "25. ...while the guest's store and the shared one stay reachable: they are nobody's own" "$(echo "$o" | sed -n 1p | grep -q '^HTTP/1.1 200' && echo "$o" | sed -n 2p | grep -q '^HTTP/1.1 200' && echo 1)" "$o"
rm -f /run/rudolf/home-session
o=$(t 'GET home?acct=u-bob')
ok "26. nobody signed in (the sign-in screen): as before, so its housekeeping and the checks still run" "$(echo "$o" | grep -q '^HTTP/1.1 200' && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
