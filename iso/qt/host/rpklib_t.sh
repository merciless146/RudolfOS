#!/bin/sh
# 1.5.2: rpk and the Rudolf OS Library's key. Built from sys/rpk.c and run against folder sources signed with throwaway
# keys (a "system" key and a "Library" key), with its own root and mount folder. Needs root (it mounts the packages).
#   sh rpklib_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../..; TOOL=$I/pkgtools/rpk-tool.py
T=$(mktemp -d); trap 'for m in $(grep " $T/" /proc/mounts | cut -d" " -f2); do umount -d $m 2>/dev/null; done; rm -rf $T' EXIT
gcc -O2 -o $T/rpk $I/sys/rpk.c -lcrypto || exit 1
python3 $TOOL keygen $T/sys >/dev/null; python3 $TOOL keygen $T/lib >/dev/null
export RPK_ROOT=$T/root RPK_MNT=$T/mnt RPK_DISC=$T/nodisc RPK_KEYS=$T/sys/rpk-signing.pub RPK_LIBRARY_KEYS=$T/lib/rpk-signing.pub RPK_SOURCES=$T/sources
# 1.5.4: a stand-in for "rudolfd --health KIND MESSAGE" (the health log)
export RPK_HEALTH=$T/health; printf '#!/bin/sh\necho "$2 $3" >> %s/hlog\n' $T > $T/health; chmod +x $T/health; : > $T/hlog
mk() {   # NAME VERSION KIND KEY OUT
	d=$T/p/$1-$2-$3; mkdir -p $d/www; printf 'name = %s\nversion = %s\nkind = %s\ntitle = %s\nsummary = test\nlicense = MIT\n%s' $1 $2 $3 $1 "$6" > $d/PKGINFO
	echo "$1 $2" > $d/www/hello.txt; [ $3 = cli ] && { mkdir -p $d/bin; printf '#!/bin/sh\necho hi\n' > $d/bin/$1; chmod +x $d/bin/$1; printf 'commands = %s\n' $1 >> $d/PKGINFO; }
	python3 $TOOL build $d $5 --key $T/$4/rpk-signing.key >/dev/null; }
mkdir -p $T/sysrepo $T/librepo
mk sysapp 1.0 addon sys $T/sysrepo/sysapp.rpk; python3 $TOOL index $T/sysrepo --key $T/sys/rpk-signing.key --serial 1 >/dev/null
mk rudolf-quake 1.0.0 addon lib $T/librepo/rudolf-quake.rpk
mk evilcli 1.0 cli lib $T/librepo/evilcli.rpk
mk sysapp 9.0 addon lib $T/librepo/sysapp.rpk
python3 $TOOL index $T/librepo --key $T/lib/rpk-signing.key --serial 5 >/dev/null
printf '%s\n' $T/librepo > $T/sources
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
s=$($T/rpk status)
ok "1. the system's sources (the Library) are read, and its list is marked as the Library's" $(has "$s" '"loc":"'$T/librepo'","type":"folder","ok":true,"library":true') "$s"
ok "2. its packages are marked as the Library's" $(has "$s" '"source":"'$T/librepo'","library":true') "$s"
o=$($T/rpk install rudolf-quake 2>&1); r=$?
ok "3. a Library package for the desktop installs and is served from its mount" $([ $r = 0 ] && grep -q "rudolf-quake 1.0.0" $T/mnt/rudolf-quake/www/hello.txt && echo 1 || echo 0) "$r $o"
ok "4. ...recorded as signed by the Library" $(grep -qx "signed = library" $T/root/db/rudolf-quake.info && echo 1 || echo 0) "$(cat $T/root/db/rudolf-quake.info)"
o=$($T/rpk install evilcli 2>&1); r=$?
ok "5. a Library package with a program is refused" $([ $r != 0 ] && [ "$(has "$o" "can only hold files for the desktop")" = 1 ] && [ ! -e $T/root/store/evilcli.rpk ] && echo 1 || echo 0) "$r $o"
# a system package installed first; the Library may not replace it
printf '%s\n%s\n' $T/sysrepo $T/librepo > $T/sources
$T/rpk install-file $T/sysrepo/sysapp.rpk >/dev/null 2>&1
o=$($T/rpk install-file $T/librepo/sysapp.rpk 2>&1); r=$?
ok "6. a Library package cannot replace a system package of the same name, even with a higher version" $([ $r != 0 ] && [ "$(has "$o" "would replace a package that came with Rudolf OS")" = 1 ] && grep -q "sysapp 1.0" $T/mnt/sysapp/www/hello.txt && echo 1 || echo 0) "$r $o"
# a Library list must be signed: a list signed by an unknown key is refused
python3 $TOOL keygen $T/evil >/dev/null; mkdir -p $T/evilrepo; cp $T/librepo/rudolf-quake.rpk $T/evilrepo/; python3 $TOOL index $T/evilrepo --key $T/evil/rpk-signing.key --serial 9 >/dev/null
printf '%s\n' $T/evilrepo > $T/sources; s=$($T/rpk status)
ok "7. a list signed by an unknown key is refused" $(has "$s" 'not signed by a trusted key') "$s"
# a Library update replaces the Library's own package
printf '%s\n' $T/librepo > $T/sources; mk rudolf-quake 1.1.0 addon lib $T/librepo/rudolf-quake.rpk; python3 $TOOL index $T/librepo --key $T/lib/rpk-signing.key --serial 6 >/dev/null
o=$($T/rpk install rudolf-quake 2>&1); r=$?
ok "8. a newer version from the Library replaces the Library's own package" $([ $r = 0 ] && grep -q "rudolf-quake 1.1.0" $T/mnt/rudolf-quake/www/hello.txt && echo 1 || echo 0) "$r $o"
# a PKGINFO that claims "signed" itself is refused
mk liar 1.0 addon lib $T/liar.rpk "signed = system
"
o=$($T/rpk install-file $T/liar.rpk 2>&1); r=$?
ok "9. a package that writes its own \"signed\" line is refused" $([ $r != 0 ] && echo 1 || echo 0) "$r $o"
# 1.5.4: the health log has a line for each Library install, update, removal and refusal
H=$(cat $T/hlog)
ok "11. health log: the Library install" $(has "$H" "library installed rudolf-quake 1.0.0, signed by the Rudolf OS Library") "$H"
ok "12. health log: the Library update" $(has "$H" "library updated rudolf-quake from 1.0.0 to 1.1.0, signed by the Rudolf OS Library") "$H"
ok "13. health log: the refused program and the refused replacement" $([ "$(has "$H" "library refused evilcli 1.0, signed by the Rudolf OS Library: a Rudolf OS Library package can only hold files for the desktop")" = 1 ] && [ "$(has "$H" "library refused sysapp 9.0")" = 1 ] && echo 1 || echo 0) "$H"
ok "14. health log: nothing for the system's own package" $(has "$H" "sysapp 1.0" | tr 1 x | tr 0 1 | tr x 0) "$H"
$T/rpk remove rudolf-quake >/dev/null 2>&1; H=$(cat $T/hlog)
ok "15. health log: the Library removal" $(has "$H" "library removed rudolf-quake 1.1.0, from the Rudolf OS Library") "$H"
# rudolf-update never reads library-keys
ok "10. rudolf-update reads only rpk-keys (never library-keys)" $(grep -q "library-keys" $I/sys/rudolf-update.c && echo 0 || echo 1)
echo "$((N-F))/$N passed"; [ $F = 0 ]
