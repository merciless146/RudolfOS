#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 27): the drives helper's Bring your files actions (rudolf-drives.c bring-*), built with
# -DRR_TEST on a stand-in tree: what reaches bringfiles.sh (a stand-in that says how it was called and as whom), what is
# refused and said in the health log; bring-import run as the account's own Linux user (never root), only for the account
# signed in, only for a copy on a drive without a link on the way; the BitLocker key on standard input only.
# Needs root and gcc.   sh bringhelper62_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/bh62.XXXXXX); chmod 755 $T; trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
mkdir -p $T/sys/block/nvme0n1 $T/sys/class/block/nvme0n1p3 $T/lib $T/run "$T/media/sdc1/Rudolf OS - Bring your files/Old computer" $T/media/sdc1/other $T/accounts/uABCdef12/Documents $T/root
ln -s "$T/media/sdc1/Rudolf OS - Bring your files/Old computer" "$T/media/sdc1/Rudolf OS - Bring your files/lnk"
printf '#!/bin/sh\nk=; [ "$1" = unlock ] && read -r k\necho "bf $*|uid=$(id -u)|gid=$(id -g)|groups=$(id -G)|key=$k|path=$PATH|leak=$LEAKME" >> %s/calls\necho "said $1"\n' $T > $T/lib/bringfiles.sh; chmod 755 $T/lib/bringfiles.sh
printf 'uABCdef12 47110\nuOTHER999 47111\n-uGONE0000 47112\n' > $T/aumap; echo uABCdef12 > $T/run/home-session
cat > $T/rr.conf <<EOF
RR_TEST_SYS=$T/sys
RR_TEST_LIB=$T/lib
RR_TEST_MEDIA=$T/media
RR_TEST_RUN=$T/run
RR_TEST_AUMAP=$T/aumap
RR_TEST_HEALTH=$T/health.log
RR_TEST_DIR=$T/root
RR_TEST_HOMEROOT=$T/accounts
EOF
gcc -O2 -Wall -Wno-format-truncation -DRR_TEST -DRR_TEST_CONF="\"$T/rr.conf\"" -o $T/drives $SYS/rudolf-drives.c || { echo "FAIL build"; exit 1; }
DR() { : > $T/calls; chmod 666 $T/calls; env LEAKME=x $T/drives "$@" > $T/out 2>&1; echo $?; }
r=$(DR bring-scan nvme0n1); ok "1. bring-scan of a disk: bringfiles.sh scan DISK, as root, nothing of the caller's environment" "$([ $r = 0 ] && grep -q '^bf scan nvme0n1|uid=0|.*|leak=$' $T/calls && echo 1)" "$r $(cat $T/calls $T/out)"
r=$(DR bring-scan nvme0n1p3); r2=$(DR bring-scan 'sd;a'); r3=$(DR bring-scan sdz)
ok "2. ...not a partition, not a name that is not one, not a disk the kernel does not have (said in the health log)" "$([ $r = 2 ] && [ $r2 = 2 ] && [ $r3 = 2 ] && grep -q 'a disk name that is not one (sd;a)' $T/health.log && echo 1)" "$r $r2 $r3"
r=$(DR bring-decide nvme0n1 erase); r2=$(DR bring-decide nvme0n1 everything)
ok "3. bring-decide: erase or partial only; the choice to erase said in the health log" "$([ $r = 0 ] && [ $r2 = 2 ] && grep -q "nvme0n1's personal files are to be erased (erase)" $T/health.log && echo 1)" "$r $r2"
: > $T/calls; echo '235818-357951-253979' | $T/drives bring-unlock nvme0n1p3 > $T/out 2>&1; r=$?
ok "4. bring-unlock: the key reaches the script on its standard input (not an argument)" "$([ $r = 0 ] && grep -q '^bf unlock nvme0n1p3|uid=0|.*|key=235818-357951-253979|' $T/calls && ! grep -q 'unlock nvme0n1p3 235818' $T/calls && grep -q 'BitLocker on nvme0n1p3 is being opened read-only (the key is not kept)' $T/health.log && ! grep -q 235818 $T/health.log && echo 1)" "$r $(cat $T/calls)"
r=$(DR bring-unlock nvme0n1p9); r2=$(DR bring-unlock nvme0n1p3 extra)
ok "5. ...not a partition the kernel does not have, not with more arguments" "$([ $r = 2 ] && [ $r2 = 2 ] && echo 1)" "$r $r2"
r=$(DR bring-copy); r2=$(DR bring-copy /etc/passwd); r3=$(DR bring-found); r4=$(DR bring-cancel)
ok "6. bring-copy: only the list rudolfd writes (its run folder), no list of the caller's; bring-found, bring-cancel" "$([ $r = 0 ] && [ $r2 = 2 ] && [ $r3 = 0 ] && [ $r4 = 0 ] && echo 1)" "$r $r2 $r3 $r4"
DST="$T/media/sdc1/Rudolf OS - Bring your files/Old computer"
r=$(DR bring-import uABCdef12 "$DST" Ann); c=$(cat $T/calls)
ok "7. bring-import: as the account's own Linux user (47110, no other group), into its folder" "$([ $r = 0 ] && echo "$c" | grep -q "^bf import $DST Ann $T/accounts/uABCdef12|uid=47110|gid=47110|groups=47110|" && echo 1)" "$r $c $(cat $T/out)"
r1=$(DR bring-import uOTHER999 "$DST" Ann); r2=$(DR bring-import uGONE0000 "$DST" Ann); r3=$(DR bring-import guest "$DST" Ann)
ok "8. ...only for the account signed in (not another, not a removed one, not the guest)" "$([ $r1 = 2 ] && [ $r2 = 2 ] && [ $r3 = 2 ] && grep -q 'an account that is not signed in (uOTHER999)' $T/health.log && echo 1)" "$r1 $r2 $r3"
r1=$(DR bring-import uABCdef12 "$T/media/sdc1/other" Ann); r2=$(DR bring-import uABCdef12 "$T/media/sdc1/Rudolf OS - Bring your files/lnk" Ann)
r3=$(DR bring-import uABCdef12 "$DST/../../other" Ann); r4=$(DR bring-import uABCdef12 /etc Ann)
ok "9. ...only a copy in Bring your files' folder on a drive: not another folder, not through a link, not with .., not outside the drives" "$([ $r1 = 2 ] && [ $r2 = 2 ] && [ $r3 = 2 ] && [ $r4 = 2 ] && echo 1)" "$r1 $r2 $r3 $r4"
r1=$(DR bring-import uABCdef12 "$DST" ..); r2=$(DR bring-import uABCdef12 "$DST" "a/b"); r3=$(DR bring-import uABCdef12 "$DST" "$(printf 'a\tb')")
ok "10. ...only a person's folder name (no .., no slash, no control character)" "$([ $r1 = 2 ] && [ $r2 = 2 ] && [ $r3 = 2 ] && echo 1)" "$r1 $r2 $r3"
r=$(DR bring-nothing); r2=$(DR bring-scan nvme0n1 --sha=$(printf '%064d' 0))
ok "11. an unknown Bring your files action, and a checksum, refused" "$([ $r = 2 ] && [ $r2 = 2 ] && echo 1)" "$r $r2"
echo "$((N-F))/$N passed"; [ $F = 0 ]
