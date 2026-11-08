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
ok "1. the first unlock makes the account's folder" "$(has "$o" "200 OK")" "$o"
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
ok "19. unlocked again with its key, the files are back" "$(has "$o" "|v2")" "$o"
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
umount /home; umount $T/m; rm -rf $T
echo "$((N-F))/$N passed"; [ $F = 0 ]
