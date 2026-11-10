#!/bin/sh
# 1.5.6 (plan stage 1): rudolfd's home/* requests on a real ext4 drive with the encrypt feature, as RUDOLFDATA has:
# an account's folder is made with its key's policy, opens with that key only, is unreadable after home/lock; paths
# never leave it (.., links, absolute paths); writes are atomic and refuse an out-of-date base version; large files go
# in pieces; rename, copy and delete; the binds for Linux apps. Compiled from rudolfd.c. Needs root (a loop device and
# a private mount namespace).   sh homeapi_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$HAT_INNER" ] && exec env HAT_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/homeapi.XXXXXX)
mount -t tmpfs none /run; mkdir -p /run/rudolf
gcc -O1 -w -o $T/t $HERE/fixtures/homeapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
truncate -s 96M $T/img; mkfs.ext4 -q -F -O encrypt $T/img; mkdir $T/m; mount -o loop $T/img $T/m || { echo "SKIP: no loop device"; exit 0; }
R=$T/m/accounts; N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
t() { $T/t $R "$@"; }
K1=$(head -c 64 /dev/urandom | od -An -tx1 | tr -d ' \n'); K2=$(head -c 64 /dev/urandom | od -An -tx1 | tr -d ' \n')
printf %s "$K1" > $T/k1; printf %s "$K2" > $T/k2; printf 'Dear diary' > $T/a; head -c 300000 /dev/urandom > $T/big; printf 'v2' > $T/b
o=$(t "POST home/unlock?acct=u-boss <$T/k1")
ok "1. the first unlock makes the account's folder (and says made)" "$(has "$o" '"message":"made"')" "$o"
ok "2. ...encrypted with the key's policy, with the usual folders" "$([ -d $R/u-boss/Documents ] && [ -d $R/u-boss/.Trash ] && [ "$(stat -c %u $R/u-boss/Documents)" = 1000 ] && echo 1)" "$(ls -la $R/u-boss)"
o=$(t "POST home/write?acct=u-boss&path=Documents/diary.txt&base=new <$T/a" "GET home/read?acct=u-boss&path=Documents/diary.txt")
ok "3. a file is written and read back" "$(has "$o" "Dear diary")" "$o"
V=$(echo "$o" | head -1 | sed 's/.*"v":"\([^"]*\)".*/\1/')
o=$(t "POST home/write?acct=u-boss&path=Documents/diary.txt&base=new <$T/b")
ok "4. base=new refuses to replace an existing file" "$(has "$o" "409")" "$o"
o=$(t "POST home/write?acct=u-boss&path=Documents/diary.txt&base=$V <$T/b" "POST home/write?acct=u-boss&path=Documents/diary.txt&base=$V <$T/a" "GET home/read?acct=u-boss&path=Documents/diary.txt")
ok "5. a write with the current version replaces it; one with an old version is refused (conflict)" "$([ "$(echo "$o" | sed -n 1p | grep -c '200 OK')" = 1 ] && [ "$(echo "$o" | sed -n 2p | grep -c '"conflict":true')" = 1 ] && echo "$o" | sed -n 3p | grep -q '|v2$' && echo 1)" "$o"
o=$(t "POST home/write?acct=u-boss&path=Pictures/big.bin&part=1 <$T/big" "POST home/write?acct=u-boss&path=Pictures/big.bin&part=1 <$T/big" "POST home/write?acct=u-boss&path=Pictures/big.bin&final=1")
ok "6. a large file in pieces arrives whole, with no piece file left" "$([ "$(stat -c %s $R/u-boss/Pictures/big.bin)" = 600000 ] && [ -z "$(ls -A $R/u-boss/Pictures | grep rudolf-part)" ] && echo 1)" "$o $(ls -la $R/u-boss/Pictures)"
o=$(t "GET home/tree?acct=u-boss")
ok "7. tree lists every folder and file with size and version" "$(echo "$o" | sed 's/^[^|]*|//' | python3 -c 'import json,sys; j=json.load(sys.stdin); p={i["p"]:i for i in j["items"]}; print(1 if p["Documents/diary.txt"]["s"]==2 and p["Pictures/big.bin"]["s"]==600000 and p["Documents"]["d"]==1 and "v" in p["Documents/diary.txt"] else 0)')" "$o"
for bad in "../u-x/a" "/etc/passwd" "Documents/../../x" "Documents//x" "./a" "a/./b"; do
	o=$(t "GET home/read?acct=u-boss&path=$bad"); ok "8. a bad path is refused: $bad" "$(has "$o" "400")" "$o"; done
ln -s /etc $R/u-boss/Documents/link; mkdir -p $R/u-boss/Music; ln -s ../../ $R/u-boss/Music/up
o=$(t "GET home/read?acct=u-boss&path=Documents/link/hostname" "GET home/list?acct=u-boss&path=Music/up" "POST home/write?acct=u-boss&path=Documents/link/x&mkdirs=1 <$T/a")
ok "9. links are never followed (reading, listing, writing through one)" "$([ "$(echo "$o" | grep -c ' 404 ')" = 3 ] && [ ! -e /etc/x ] && echo 1)" "$o"
o=$(t "GET home/tree?acct=u-boss&path=Documents"); ok "10. links are not listed" "$([ "$(has "$o" '"Documents/link"')" = 0 ] && echo 1)" "$o"
o=$(t "POST home/mkdir?acct=u-boss&path=Documents/Work/2026" "POST home/rename?acct=u-boss&from=Documents/diary.txt&to=Documents/Work/2026/diary.txt" "POST home/copy?acct=u-boss&from=Documents/Work&to=Desktop/Work%20copy" "POST home/copy?acct=u-boss&from=Documents/Work&to=Documents/Work/inside")
ok "11. mkdir, rename and copy (a folder, recursively); a folder is not copied into itself" "$([ -f $R/u-boss/Documents/Work/2026/diary.txt ] && [ "$(cat "$R/u-boss/Desktop/Work copy/2026/diary.txt")" = v2 ] && echo "$o" | sed -n 4p | grep -q ' 400 ' && echo 1)" "$o"
o=$(t "POST home/rename?acct=u-boss&from=Pictures/big.bin&to=Desktop/Work%20copy" "POST home/delete?acct=u-boss&path=Desktop/Work%20copy" "POST home/delete?acct=u-boss&path=")
ok "12. rename onto an existing name is refused without replace=1; delete removes a folder; the account's folder itself cannot be deleted" "$(echo "$o" | sed -n 1p | grep -q ' 409 ' && [ ! -e "$R/u-boss/Desktop/Work copy" ] && echo "$o" | sed -n 3p | grep -q ' 400 ' && [ -d $R/u-boss ] && echo 1)" "$o"
o=$(t "POST home/unlock?acct=u-kid <$T/k2" "POST home/write?acct=u-kid&path=Documents/kid.txt <$T/a" "GET home/read?acct=u-kid&path=../u-boss/Documents/Work/2026/diary.txt")
ok "13. a second account has its own folder; it cannot reach the first" "$([ -f $R/u-kid/Documents/kid.txt ] && echo "$o" | sed -n 3p | grep -q ' 400 ' && echo 1)" "$o"
o=$(t "POST home/unlock?acct=u-kid <$T/k1"); ok "14. another account's key does not open it" "$(has "$o" "403")" "$o"
o=$(t "GET home/read?acct=u-boss&path=Documents/Work/2026/diary.txt"); ok "15. ...and the right account stays open (its key was not taken out)" "$(has "$o" "|v2")" "$o"
o=$(t "POST home/lock?acct=u-boss" "GET home/read?acct=u-boss&path=Documents/Work/2026/diary.txt" "GET home")
ok "16. after lock, the folder is refused (423)" "$(echo "$o" | sed -n 2p | grep -q ' 423 ' && echo 1)" "$o"
ok "17. ...and its names and contents are unreadable on the disk, also to root" "$([ ! -e $R/u-boss/Documents ] && ! grep -rqa 'Dear diary\|v2' $R/u-boss 2>/dev/null && echo 1)" "$(ls $R/u-boss | head -3)"
ok "18. GET home lists the accounts, open or locked" "$(echo "$o" | sed -n 3p | grep -q '"id":"u-boss","unlocked":false' && echo "$o" | sed -n 3p | grep -q '"id":"u-kid","unlocked":true' && echo 1)" "$o"
o=$(t "POST home/unlock?acct=u-boss <$T/k1" "GET home/read?acct=u-boss&path=Documents/Work/2026/diary.txt")
ok "19. unlocked again with its key (not made again), the files are back" "$([ "$(has "$o" "|v2")" = 1 ] && echo "$o" | sed -n 1p | grep -q '"message":"unlocked"' && echo 1)" "$o"
o=$(t "POST home/unlock?acct=../x <$T/k1" "POST home/unlock?acct=guest <$T/k1" "POST home/unlock?acct=u-boss <$T/a")
ok "20. bad accounts and bad keys are refused" "$([ "$(echo "$o" | grep -c ' 400 ')" = 3 ] && echo 1)" "$o"
# the binds for Linux apps (a private /home here)
mount -t tmpfs none /home; mkdir -p /home/rudolf/Documents; echo old > /home/rudolf/Documents/old.txt
o=$(BIND=1 t "POST home/unlock?acct=u-boss&session=1 <$T/k1")
ok "21. session=1: Linux apps' Documents is the account's" "$([ -d /home/rudolf/Documents/Work/2026 ] && [ ! -e /home/rudolf/Documents/old.txt ] && grep -q u-boss /run/rudolf/home-session && echo 1)" "$o $(ls /home/rudolf/Documents)"
o=$(BIND=1 t "POST home/lock?acct=u-boss")
ok "22. lock: the binds go, and what was there before is back" "$([ -f /home/rudolf/Documents/old.txt ] && [ ! -e /run/rudolf/home-session ] && echo 1)" "$o"
# stage 2: a large download moved in (across file systems here, as across the vault's encryption: copied), and links
mkdir -p /home/rudolf/Downloads/.accounts/u-boss /home/rudolf/Downloads/.accounts/u-kid; head -c 2000000 /dev/urandom > /home/rudolf/Downloads/.accounts/u-boss/movie.mp4; echo x > /home/rudolf/Downloads/.accounts/u-kid/theirs.txt
o=$(t "POST home/unlock?acct=u-boss <$T/k1" "POST home/adopt?acct=u-boss&from=/home/rudolf/Downloads/.accounts/u-boss/movie.mp4&to=Downloads/MOVIE.MP4")
ok "23. adopt: a large download moves into the account's Downloads (copied across file systems, the original gone)" "$([ "$(stat -c %s $R/u-boss/Downloads/MOVIE.MP4 2>/dev/null)" = 2000000 ] && [ ! -e /home/rudolf/Downloads/.accounts/u-boss/movie.mp4 ] && echo 1)" "$o"
o=$(t "POST home/adopt?acct=u-boss&from=/home/rudolf/Downloads/.accounts/u-kid/theirs.txt&to=Documents/x" "POST home/adopt?acct=u-boss&from=/etc/hostname&to=Documents/x" "POST home/adopt?acct=u-boss&from=/home/rudolf/Downloads/.incoming/x&to=Documents/x")
ok "24. adopt refuses another account's download and anything outside Downloads" "$([ "$(echo "$o" | grep -c ' 400 ')" = 3 ] && [ -e /home/rudolf/Downloads/.accounts/u-kid/theirs.txt ] && echo 1)" "$o"
o=$(t "GET home/link?acct=u-boss&path=Downloads/MOVIE.MP4" "GET home/link?acct=u-boss&path=Downloads/NONE.MP4")
id=$(echo "$o" | sed -n 1p | sed 's/.*\/dlf\/\([0-9a-f]*\).*/\1/')
ok "25. link: a /dlf/ link holding the account and the path (walked again when used), not a resolved path" "$([ "$(cat /run/rudolf/dlf/$id 2>/dev/null)" = "home:u-boss:Downloads/MOVIE.MP4" ] && echo "$o" | sed -n 2p | grep -q ' 404 ' && echo 1)" "$o"
# 1.5.6: Downloads joins the binds. The downloads store (here a link into another folder, as the encrypted folder is)
# gets its own bind, /run/rudolf/dl, so Chromium's .incoming and the files Linux apps saved before stay reachable
DS=$T/m/vaultdl; mkdir -p $DS/.incoming $DS/.accounts/u-boss; echo partial > $DS/.incoming/x.crdownload; echo linux > $DS/LINUX.TXT
rm -rf /home/rudolf/Downloads; ln -s $DS /home/rudolf/Downloads
o=$(BOOT=1 BIND=1 t "POST home/unlock?acct=u-boss&session=1 <$T/k1")
ok "25b. session=1: Linux apps' Downloads is the account's own (MOVIE.MP4), the store is at /run/rudolf/dl" "$([ -f /home/rudolf/Downloads/MOVIE.MP4 ] && [ ! -e /home/rudolf/Downloads/LINUX.TXT ] && [ -f /run/rudolf/dl/.incoming/x.crdownload ] && [ -f /run/rudolf/dl/LINUX.TXT ] && [ -d /run/rudolf/dl/.quarantine ] && echo 1)" "$o $(ls -A /home/rudolf/Downloads) / $(ls -A /run/rudolf/dl)"
o=$(BIND=1 t "GET home")
ok "25b2. a later request (its own process in rudolfd) still knows Downloads is bound: it reads the mounts" "$(echo "$o" | grep -q '"downloadsBound":true' && echo 1)" "$o"
echo new > /run/rudolf/dl/.accounts/u-boss/new.bin   # (DS itself is covered by the account's bind now)
o=$(BIND=1 t "POST home/adopt?acct=u-boss&from=/home/rudolf/Downloads/.accounts/u-boss/new.bin&to=Downloads/NEW.BIN" "POST home/adopt?acct=u-boss&from=/home/rudolf/Downloads/LINUX.TXT&to=Downloads/LINUX.TXT")
ok "25c. ...the desktop's paths (/home/rudolf/Downloads/.accounts/..., and the top) still reach the store: adopt works" "$([ -f $R/u-boss/Downloads/NEW.BIN ] && [ -f $R/u-boss/Downloads/LINUX.TXT ] && [ ! -e /run/rudolf/dl/LINUX.TXT ] && echo 1)" "$o"
o=$(BOOT=1 BIND=1 t "POST home/unlock?acct=u-boss&session=1 <$T/k1")
ok "25d. rudolfd started again in the session: the binds are made again, not stacked" "$([ "$(grep -c ' /home/rudolf/Downloads \| '$DS' ' /proc/self/mounts)" -le 1 ] && [ -f /home/rudolf/Downloads/NEW.BIN ] && [ -f /run/rudolf/dl/.incoming/x.crdownload ] && echo 1)" "$(grep -E 'rudolf|vaultdl' /proc/self/mounts)"
o=$(BIND=1 t "POST home/lock?acct=u-boss")
ok "25e. lock: Downloads is the store again, its bind at /run/rudolf/dl stays" "$([ -f /home/rudolf/Downloads/.incoming/x.crdownload ] && [ ! -e /home/rudolf/Downloads/NEW.BIN ] && [ -f /run/rudolf/dl/.incoming/x.crdownload ] && echo 1)" "$o $(ls -A /home/rudolf/Downloads)"
# the Vivobook: at start-up Downloads is in the locked encrypted folder, so its name is not found yet
umount -l /run/rudolf/dl; rmdir /run/rudolf/dl; mv $DS $DS.locked
o=$(BOOT=1 BIND=1 t "GET home")
ok "25f. Downloads not found at the start (locked): /run/rudolf/dl is a link to Downloads, and Downloads is not bound" "$([ -L /run/rudolf/dl ] && [ "$(readlink /run/rudolf/dl)" = /home/rudolf/Downloads ] && echo "$o" | grep -q '"downloadsBound":false' && echo 1)" "$o $(ls -la /run/rudolf)"
mv $DS.locked $DS
BOOT=1 BIND=1 t "GET home" >/dev/null   # (vault/unlock calls dl_home_boot)
o=$(BIND=1 t "POST home/unlock?acct=u-boss&session=1 <$T/k1" "GET home")
ok "25g. once it is found (unlocked): the link becomes the bind, and Downloads joins the account's binds" "$([ -d /run/rudolf/dl ] && [ ! -L /run/rudolf/dl ] && [ -f /run/rudolf/dl/.incoming/x.crdownload ] && [ -f /home/rudolf/Downloads/NEW.BIN ] && echo "$o" | grep -q '"downloadsBound":true' && echo 1)" "$o $(ls -la /run/rudolf)"
# the top of the store (Linux apps' Downloads before): identical copies go; with one account folder the rest moves in
echo new > /run/rudolf/dl/NEW.BIN; echo other > /run/rudolf/dl/OTHER.TXT; echo part > /run/rudolf/dl/x.iso.crdownload
o=$(BIND=1 t "POST home/unlock?acct=u-boss&session=1 <$T/k1" 2>&1)
ok "25h. sign-in with Downloads bound: the identical copy at the top goes; OTHER.TXT stays (two account folders); the partial stays" "$([ ! -e /run/rudolf/dl/NEW.BIN ] && [ -f /run/rudolf/dl/OTHER.TXT ] && [ -f /run/rudolf/dl/x.iso.crdownload ] && [ -f $R/u-boss/Downloads/NEW.BIN ] && echo 1)" "$o $(ls -A /run/rudolf/dl)"
mv $R/u-kid $T/kid.aside; echo diff > /run/rudolf/dl/new.bin; mkdir -p $R/u-boss/Downloads/.accounts/u-boss
o=$(BIND=1 t "POST home/unlock?acct=u-boss&session=1 <$T/k1" 2>&1)
ok "25i. ...with one account folder: OTHER.TXT moves in, a different new.bin gets a free name (new (2).bin), an empty stray .accounts goes, and rudolfd says so" "$([ -f $R/u-boss/Downloads/OTHER.TXT ] && [ "$(cat "$R/u-boss/Downloads/new (2).bin")" = diff ] && [ "$(cat $R/u-boss/Downloads/NEW.BIN)" = new ] && [ ! -e /run/rudolf/dl/OTHER.TXT ] && [ -f /run/rudolf/dl/x.iso.crdownload ] && [ ! -e $R/u-boss/Downloads/.accounts ] && echo "$o" | grep -q '1 files moved in\|2 files moved in' && echo 1)" "$o $(ls -A $R/u-boss/Downloads) / $(ls -A /run/rudolf/dl)"
mv $T/kid.aside $R/u-kid
# the wizard gives a file in the account's folder; Linux sees it under /home/rudolf (home_linux_path)
o=$(BIND=1 t "LP $R/u-boss/Downloads/a b.deb" "LP $R/u-boss/Documents/x/y.deb" "LP $R/u-boss/Desktop/z.deb" "LP $R/u-kid/Downloads/k.deb" "LP $R/u-boss/Downloads/../../u-kid/Downloads/k.deb" "LP /etc/passwd")
ok "25j. a file in the signed-in account's Downloads or Documents -> /home/rudolf/...; Desktop, another account, .. and other paths are not" "$([ "$(echo "$o" | tr '\n' '|')" = "LP /home/rudolf/Downloads/a b.deb|LP /home/rudolf/Documents/x/y.deb|LP -|LP -|LP -|LP -|" ] && echo 1)" "$o"
# 1.5.6: home/hash (File Explorer's Find duplicates): a file's SHA-256, read in place; never a link, a folder or outside
cp $T/big $T/m/accounts/u-boss/Pictures/dup.bin 2>/dev/null || t "POST home/write?acct=u-boss&path=Pictures/dup.bin&base=new <$T/big" >/dev/null
o=$(t "GET home/hash?acct=u-boss&path=Pictures/dup.bin" "GET home/hash?acct=u-boss&path=Pictures" "GET home/hash?acct=u-boss&path=../u-kid/Documents/x")
ok "25k. home/hash gives the file's SHA-256; a folder and a path outside the account are refused" "$(echo "$o" | sed -n 1p | grep -q "\"sha256\":\"$(sha256sum $T/big | cut -c1-64)\"" && [ "$(echo "$o" | sed -n 2,3p | grep -c ' 40[04] ')" = 2 ] && echo 1)" "$o"
# 1.5.6: the desktop's account ids have capitals (u and 8 letters and digits): such a folder is made and opened (QEMU
# t156: uQppGToP7 got "bad account" and never moved to the disk)
o=$(t "POST home/unlock?acct=uQppGToP7 <$T/k2" "POST home/write?acct=uQppGToP7&path=Documents/x.txt&base=new <$T/a" "GET home/read?acct=uQppGToP7&path=Documents/x.txt" "POST home/lock?acct=uQppGToP7" "POST home/unlock?acct=u/../x <$T/k2")
ok "25l. an account id with capitals is a folder of its own (made, written, read); a path in its place is still refused" "$(echo "$o" | grep -q "\"made\"" && [ "$(echo "$o" | grep -c "Dear diary")" = 1 ] && echo "$o" | tail -1 | grep -q ' 400 ' && echo 1)" "$o"
# ...and its large download, kept in Downloads/.accounts/<the id in lower case> (dlAcct()), moves in (QEMU t156: "bad path"
# stopped the whole move)
mkdir -p /home/rudolf/Downloads/.accounts/uqppgtop7; head -c 300000 /dev/urandom > /home/rudolf/Downloads/.accounts/uqppgtop7/big.bin
o=$(t "POST home/unlock?acct=uQppGToP7 <$T/k2" "POST home/adopt?acct=uQppGToP7&from=/home/rudolf/Downloads/.accounts/uqppgtop7/big.bin&to=Downloads/BIG.BIN" "POST home/lock?acct=uQppGToP7")
ok "25m. an account id with capitals: its large download (in the lower-case folder) moves in" "$(echo "$o" | sed -n 2p | grep -q ' 200 ' && [ ! -e /home/rudolf/Downloads/.accounts/uqppgtop7/big.bin ] && echo 1)" "$o"
# 1.5.7 (plan stage 1): the desktop's Event Viewer events are appended to .rudolf/events.log in the account's folder
printf '2026-11-10T10:00:00Z sys one' > $T/e1; printf '2026-11-10T10:00:01Z fw two\n' > $T/e2
o=$(t "POST home/unlock?acct=u-boss <$T/k1" "POST home/append?acct=u-boss&path=.rudolf/events.log <$T/e1" "POST home/append?acct=u-boss&path=.rudolf/events.log <$T/e2")
ok "26a. append adds lines to .rudolf/events.log (a missing newline is added), owned by the account" "$([ "$(cat $R/u-boss/.rudolf/events.log)" = "$(printf '2026-11-10T10:00:00Z sys one\n2026-11-10T10:00:01Z fw two')" ] && [ "$(stat -c %u%a $R/u-boss/.rudolf/events.log)" = 1000600 ] && echo 1)" "$o $(cat $R/u-boss/.rudolf/events.log)"
o=$(t "POST home/append?acct=u-boss&path=Documents/diary.txt <$T/e1" "POST home/append?acct=u-boss&path=.rudolf/x.txt <$T/e1" "POST home/append?acct=u-boss&path=.rudolf/a/b.log <$T/e1" "POST home/append?acct=u-boss&path=.rudolf/.x.log <$T/e1" "POST home/append?acct=u-boss&path=.rudolf/../a.log <$T/e1")
ok "26b. append writes only .rudolf/*.log" "$([ "$(echo "$o" | grep -c ' 400 ')" = 5 ] && echo 1)" "$o"
ln -s /etc/hostname $R/u-boss/.rudolf/link.log
o=$(t "POST home/append?acct=u-boss&path=.rudolf/link.log <$T/e1"); ok "26c. ...never through a link" "$(has "$o" " 409 ")" "$o"
head -c 270000 /dev/zero | tr '\0' x > $R/u-boss/.rudolf/events.log
o=$(t "POST home/append?acct=u-boss&path=.rudolf/events.log <$T/e2")
ok "26d. past 256 KiB the log moves to events.log.1 and a new one starts" "$([ "$(stat -c %s $R/u-boss/.rudolf/events.log.1)" = 270000 ] && [ "$(cat $R/u-boss/.rudolf/events.log)" = '2026-11-10T10:00:01Z fw two' ] && echo 1)" "$o"
o=$(t "POST home/lock?acct=u-boss" "POST home/append?acct=u-boss&path=.rudolf/events.log <$T/e2")
ok "26e. a locked folder is not written (423)" "$(echo "$o" | sed -n 2p | grep -q ' 423 ' && echo 1)" "$o"
o=$(BIND=1 t "POST home/lock?acct=u-boss")
umount /run/rudolf/dl 2>/dev/null; umount -R /home; umount $T/m; rm -rf $T
echo "$((N-F))/$N passed"; [ $F = 0 ]
