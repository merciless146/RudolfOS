#!/bin/sh
# 1.6.1 (the owner, 2026-11-20): ZIP files kept on the disk, listed and extracted (/usr/lib/rudolf/dlzip.sh). Made-up ZIPs
# made here (nested folders, a ".." entry, an absolute path, a link, a file written through a link, a name clash, not
# enough space); with Info-ZIP's unzip and GNU tools, and with BusyBox's unzip and tools (as Rudolf OS has them), as gamesimport_t.sh. As root the
# unpacking is done as another user (nobody here, rudolf on Rudolf OS), which is checked.   sh dlzip161_t.sh
SH=${SH:-sh}; HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d /tmp/dlzip161.XXXXXX); trap 'rm -rf $T' EXIT; chmod 755 $T
cp $HERE/../../sys/root/usr/lib/rudolf/dlzip.sh $T/dlzip.sh; P=$T/dlzip.sh
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(printf '%s' "$3" | head -c 900)"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
hasq() { case "$1" in *"$2"*) return 0;; *) return 1;; esac; }
ROOT=0; [ "$(id -u)" = 0 ] && ROOT=1; ME=nobody
mkdir -p $T/bb $T/z; command -v busybox >/dev/null && for a in unzip find stat sort awk sed grep mktemp df ln rm chmod wc cat head tail cut tr id mkdir; do ln -s "$(command -v busybox)" $T/bb/$a; done
# the made-up ZIPs (python's zipfile, which writes names as given: "..", absolute paths, links)
python3 -I - $T/z <<'EOF'
import sys, zipfile, os
d = sys.argv[1]
def link(z, name, target):
    i = zipfile.ZipInfo(name); i.create_system = 3; i.external_attr = (0o120777 << 16); z.writestr(i, target)
with zipfile.ZipFile(d + '/game.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('Made Up Game/', ''); z.writestr('Made Up Game/readme.txt', 'Read me first\n')
    z.writestr('Made Up Game/data/level 1.dat', os.urandom(3000)); z.writestr('Made Up Game/data/deep/x.bin', os.urandom(5000))
    z.writestr('Made Up Game/empty/', ''); z.writestr('readme.txt', 'the other readme\n'); z.writestr('.hidden', 'dot file\n')
    z.writestr('notes (2).txt', 'n\n')
with zipfile.ZipFile(d + '/dotdot.zip', 'w') as z:
    z.writestr('ok.txt', 'fine'); z.writestr('../escape.txt', 'escaped')
with zipfile.ZipFile(d + '/abs.zip', 'w') as z:
    z.writestr('ok.txt', 'fine'); z.writestr(d + '/abs-escape.txt', 'escaped')
with zipfile.ZipFile(d + '/link.zip', 'w') as z:
    z.writestr('a.txt', 'a'); link(z, 'passwd-link', '/etc/passwd'); link(z, 'up', '..')
with zipfile.ZipFile(d + '/through.zip', 'w') as z:
    link(z, 'evil', d + '/outside'); z.writestr('evil/pwned.txt', 'through the link')
with zipfile.ZipFile(d + '/big.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('zeros.bin', b'\0' * (6 << 20))
with zipfile.ZipFile(d + '/many.zip', 'w') as z:
    for k in range(20005): z.writestr('f/%05d.txt' % k, '')
open(d + '/notzip.zip', 'w').write('this is not a zip archive\n')
EOF
mkdir -p $T/z/outside
for U in infozip busybox; do
	if [ $U = busybox ]; then [ -e $T/bb/unzip ] || { echo "SKIP busybox: not installed"; continue; }; export DLZIP_TEST_PATH=$T/bb; else unset DLZIP_TEST_PATH; fi
	A=$T/acct-$U; rm -rf $A; mkdir -p $A; [ $ROOT = 1 ] && chown $ME $A; chmod 700 $A
	export DLZIP_OUT=$T/out-$U DLZIP_STOP=$T/stop-$U DLZIP_USER=$ME DLZIP_MARGIN_KB=0
	run() { cp $T/z/$1 $A/$1; [ $ROOT = 1 ] && chown $ME $A/$1; printf '%s\n%s\n' "$A/$1" "$A" > $T/list; $SH $P extract $T/list 2>&1; }
	# listing
	o=$($SH $P list $T/z/game.zip 2>&1)
	ok "$U: list: names, sizes, folders (ending in /), nested" "$(echo "$o" | grep -qx 'entry 0 Made Up Game/' && echo "$o" | grep -qx 'entry 3000 Made Up Game/data/level 1.dat' && echo "$o" | grep -qx 'entry 5000 Made Up Game/data/deep/x.bin' && echo 1)" "$o"
	ok "$U: list: the total" "$(echo "$o" | tail -1 | grep -qx 'total 8 8042' && echo 1)" "$o"
	o=$($SH $P list $T/z/many.zip 2>&1)
	ok "$U: list: at most 20,000 entries, then more" "$([ "$(echo "$o" | grep -c '^entry ')" = 20000 ] && echo "$o" | grep -qx more && echo "$o" | tail -1 | grep -qx 'total 20005 0' && echo 1)" "$(echo "$o" | tail -3)"
	o=$($SH $P list $T/z/notzip.zip 2>&1); ok "$U: list: not a ZIP says so" "$(has "$o" 'error This file is not a ZIP archive')" "$o"
	# extract: nested folders, flat on the disk, names that clash, a dot file
	echo 'already here' > $A/readme.txt; [ $ROOT = 1 ] && chown $ME $A/readme.txt
	o=$(run game.zip); rc=$?
	ok "$U: extract: done, the count and bytes" "$([ $rc = 0 ] && echo "$o" | tail -1 | grep -qx 'done 6 8042' && echo 1)" "$o"
	ok "$U: progress lines (i of n, the name)" "$(echo "$o" | grep -q '^progress [0-9]* 6 Made Up Game/data/level 1.dat$' && echo 1)" "$o"
	ok "$U: folders as in the ZIP (the empty one too)" "$(echo "$o" | grep -qx 'dir Made Up Game/data/deep' && echo "$o" | grep -qx 'dir Made Up Game/empty' && echo 1)" "$o"
	ok "$U: each file flat in the account's folder, the same bytes" "$(echo "$o" | grep -q "^file Made Up Game/data/level 1.dat	level 1.dat	3000$" && [ -f "$A/level 1.dat" ] && [ "$(stat -c %s "$A/level 1.dat")" = 3000 ] && [ -f $A/x.bin ] && echo 1)" "$o"
	ok "$U: a name already there gets the next free one (name (2).ext), never overwritten" "$(echo "$o" | grep -q '^file readme.txt	readme (3).txt	17$\|^file readme.txt	readme (2).txt	17$' && [ "$(cat $A/readme.txt)" = 'already here' ] && [ -f "$A/readme (2).txt" ] && [ -f "$A/readme (3).txt" ] && echo 1)" "$(ls $A; echo "$o")"
	ok "$U: ...and one named like that already ('notes (2).txt' kept as it is)" "$(echo "$o" | grep -q '^file notes (2).txt	notes (2).txt	2$' && echo 1)" "$o"
	ok "$U: a dot file is not hidden on the disk (_.hidden)" "$(echo "$o" | grep -q '^file .hidden	_.hidden	9$' && [ -f $A/_.hidden ] && echo 1)" "$o"
	ok "$U: the hidden folder is gone, nothing else left" "$([ -z "$(ls -A $A | grep '^\.')" ] && echo 1)" "$(ls -A $A)"
	ok "$U: the lines kept for rudolfd (no progress lines)" "$(grep -q '^file Made Up Game/readme.txt	' $DLZIP_OUT && ! grep -q '^progress' $DLZIP_OUT && tail -1 $DLZIP_OUT | grep -qx 'done 6 8042' && echo 1)" "$(cat $DLZIP_OUT)"
	[ $ROOT = 1 ] && ok "$U: unpacked as the desktop user, not as root" "$([ "$(stat -c %U "$A/level 1.dat")" = $ME ] && [ "$(find $A -user root | wc -l)" = 0 ] && echo 1)" "$(ls -ln $A)"
	# refused
	# (BusyBox's unzip drops ".." and a leading "/" from the names itself, also in its list: those land inside the folder)
	o=$(run dotdot.zip); if [ $U = busybox ]; then ok "$U: an entry with .. lands inside (BusyBox drops the ..)" "$(echo "$o" | grep -q '^file escape.txt	escape.txt	7$' && echo 1)" "$o"
	else ok "$U: an entry with .. refuses the ZIP" "$(has "$o" 'error This ZIP has an entry that points outside it (../escape.txt)')" "$o"; ok "$U: ...nothing of it written" "$([ ! -e $A/ok.txt ] && echo 1)"; fi
	ok "$U: ...nothing written outside" "$([ ! -e $T/escape.txt ] && [ ! -e $T/z/escape.txt ] && echo 1)" "$(ls $T)"
	o=$(run abs.zip); if [ $U = busybox ]; then ok "$U: an absolute path lands inside (BusyBox drops the /)" "$(echo "$o" | grep -q '^file tmp/.*/abs-escape.txt	abs-escape.txt	7$' && echo 1)" "$o"
	else ok "$U: an absolute path refuses the ZIP" "$(has "$o" 'error This ZIP has an entry that points outside it (/')" "$o"; fi
	ok "$U: ...nothing written outside" "$([ ! -e $T/z/abs-escape.txt ] && echo 1)"
	o=$(run link.zip); ok "$U: links are removed, never followed; the files are extracted" "$(hasq "$o" 'removed 2 links' && echo "$o" | tail -1 | grep -qx 'done 1 1' && [ ! -e $A/passwd-link ] && [ ! -e $A/up ] && [ "$(find $A -type l | wc -l)" = 0 ] && echo 1)" "$o"
	o=$(run through.zip); ok "$U: a file meant to be written through a link lands inside, the link removed" "$([ ! -e $T/z/outside/pwned.txt ] && [ "$(find $A -type l | wc -l)" = 0 ] && echo 1)" "$o"
	o=$(run notzip.zip); ok "$U: not a ZIP: said so" "$(has "$o" 'error This file is not a ZIP archive')" "$o"
	o=$(DLZIP_MARGIN_KB=999999999999; run big.zip); ok "$U: not enough free space for the unpacked size: refused before unpacking" "$(hasq "$o" 'error Not enough free space: the files inside need 7 MB' && [ ! -e $A/zeros.bin ] && [ -z "$(ls -A $A | grep '^\.')" ] && echo 1)" "$o"
	# Cancel: the stop file
	: > $DLZIP_STOP; o=$(run many.zip); rm -f $DLZIP_STOP
	ok "$U: Cancel stops it, the hidden folder removed" "$(hasq "$o" 'stopped ' && [ -z "$(ls -A $A | grep '^\.')" ] && [ ! -d $A/f ] && echo 1)" "$(echo "$o" | tail -3)"
	o=$(printf '%s\n%s\n' "/etc/passwd" "$A" > $T/list; $SH $P extract $T/list 2>&1); ok "$U: a ZIP outside the account's folder: refused" "$(has "$o" "error The ZIP is not in the account's folder")" "$o"
	# 1.6.3 Part 10, item 1: beside (a ZIP at a path, unpacked into a new folder named after it next to it)
	B=$T/beside; rm -rf $B; mkdir -p $B; [ $ROOT = 1 ] && chown $ME $B
	runb() { cp $T/z/$1 $B/$1; [ $ROOT = 1 ] && chown $ME $B/$1; printf '%s\n%s\n%s\n' "$B/$1" "$B" beside > $T/list; $SH $P extract $T/list 2>&1; }
	o=$(runb game.zip)
	ok "$U: beside: a new folder named after the ZIP, its own folders and names inside" "$(echo "$o" | grep -qx "dest $B/game" && [ -f "$B/game/Made Up Game/readme.txt" ] && [ -f "$B/game/.hidden" ] && [ -d "$B/game/Made Up Game/empty" ] && [ ! -e "$B/Made Up Game" ] && [ -f $B/game.zip ] && echo 1)" "$o $(ls -A $B)"
	ok "$U: beside: dirs and files with their paths in that folder, done; nothing hidden left" "$(echo "$o" | grep -qx 'dir game/Made Up Game/data/deep' && echo "$o" | grep -q "^file Made Up Game/data/level 1.dat	game/Made Up Game/data/level 1.dat	3000$" && echo "$o" | tail -1 | grep -qx 'done 6 8042' && [ -z "$(ls -A $B | grep '^\.')" ] && echo 1)" "$o $(ls -A $B)"
	ok "$U: beside: a second time, the next free folder (game (2))" "$(runb game.zip >/dev/null; [ -d "$B/game (2)/Made Up Game/data" ] && [ -f "$B/game/Made Up Game/readme.txt" ] && echo 1)" "$(ls -A $B)"
	[ $ROOT = 1 ] && ok "$U: beside: unpacked as the desktop user, not as root" "$([ "$(stat -c %U "$B/game/readme.txt")" = $ME ] && [ -z "$(find $B/game -user root)" ] && echo 1)" "$(ls -ln $B/game)"
	[ $ROOT = 1 ] && { o=$(su -s /bin/sh $ME -c "sh $P extract $T/list" 2>&1); ok "$U: extract is root's (rudolfd's job) only" "$(has "$o" "error This is rudolfd's")" "$o"; }
done
# a real disk too small (as root: a 4 MB tmpfs in a mount namespace of its own) and the 100 MB kept free by default
if [ $ROOT = 1 ] && command -v unshare >/dev/null; then
	mkdir -p $T/small; o=$(unset DLZIP_MARGIN_KB; DLZIP_OUT=$T/out-s DLZIP_STOP=$T/stop-s DLZIP_USER=$ME unshare -m sh -c 'mount -t tmpfs -o size=4m none "$1" && mkdir "$1/a" && chown nobody "$1/a" && cp "$2" "$1/a/big.zip" && chown nobody "$1/a/big.zip" && printf "%s\n%s\n" "$1/a/big.zip" "$1/a" > "$3" && sh "$4" extract "$3"; ls -A "$1/a"' sh $T/small $T/z/big.zip $T/list $P 2>&1)
	ok "a small disk: not enough space, nothing unpacked" "$(hasq "$o" 'error Not enough free space' && [ "$(echo "$o" | tail -1)" = big.zip ] && echo 1)" "$o"
fi
ok "the script names no ZIP entry in a command line (no eval, su with arguments)" "$(! grep -n 'eval\|su .*-c "' $P >/dev/null && echo 1)" "$(grep -n 'eval\|su .*-c "' $P)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
