#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 35): network shares on the host, against Samba in network namespaces on a bridge of their own
# (192.168.62.0/24: "nas" Samba 4 with SMB 3, wsdd and multicast DNS; "old" a server that speaks only SMB 1).
#   - rudolf-shares: find (WS-Discovery's and multicast DNS's answers), probe (SMB 3; SMB 1 found as such), list (a user's shares
#     with NTLMv2 and signing; a wrong password; a guest), resolve (NAME.local by multicast DNS);
#   - the drives helper's share-mount, share-umount and share-umount-all (rudolf-drives.c, built with -DRR_TEST on a stand-in
#     tree), through the host's own kernel SMB client: refused arguments (SMB 1 and 2.0, another user than the signed-in
#     account's, names that are not names, options hidden in a name), a share mounted for the account's uid only (its folder
#     0700, files 0600 and its own, nosuid, nodev, noexec, encrypted), files copied each way with the right owners (the
#     account's uid here, the share's user on the server), another user kept out, a guest share, a wrong password and a
#     missing share said as such, SMB 1 refused, the password (with commas, as an option would have them) never in
#     /proc/mounts, the health log or what the helper says.
# Needs root, samba, wsdd, python3-zeroconf and the kernel's cifs module (without it the mount checks are skipped, said).
#   sh shares62_t.sh      GPL-2.0-or-later.
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/sh62.XXXXXX); chmod 755 $T
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
PIDS=""; U1=ann62t; U2=bob62t
cleanup() {
	for p in $PIDS; do kill $p 2>/dev/null; done
	for m in $(awk '$2 ~ /^\/tmp\/sh62/ {print $2}' /proc/mounts | sort -r); do umount -l "$m" 2>/dev/null; done
	ip netns del nas62 2>/dev/null; ip netns del old62 2>/dev/null; ip netns del cli62 2>/dev/null; ip link del br62 2>/dev/null
	userdel $U1 2>/dev/null; userdel $U2 2>/dev/null; rm -rf $T
}
trap cleanup EXIT
[ -n "$SH62_KEEP" ] && trap - EXIT
command -v smbd >/dev/null && command -v wsdd >/dev/null && python3 -c 'import zeroconf' 2>/dev/null || { echo "FAIL: needs samba, wsdd and python3-zeroconf"; exit 1; }
. $SYS/../systools/hardflags.sh
gcc -O2 $HARD -o $T/rudolf-shares $SYS/rudolf-shares.c -lcrypto || { echo "FAIL build rudolf-shares"; exit 1; }
mkdir -p $T/run $T/root $T/sh/docs $T/sh/pub $T/sh/old
printf 'uANN000001 47110\nuBOB000002 47111\n' > $T/aumap; echo uANN000001 > $T/run/home-session
printf 'RR_TEST_RUN=%s/run\nRR_TEST_AUMAP=%s/aumap\nRR_TEST_HEALTH=%s/health.log\nRR_TEST_DIR=%s/root\nRR_TEST_SHARES=%s/run/shares\n' $T $T $T $T $T > $T/rr.conf
gcc -O2 -Wall -Wno-format-truncation -DRR_TEST -DRR_TEST_CONF="\"$T/rr.conf\"" -o $T/drives $SYS/rudolf-drives.c || { echo "FAIL build drives"; exit 1; }
# the network: a bridge, the two servers' namespaces
for l in br62 vnas62 vold62 vcli62; do ip link del $l 2>/dev/null; done
ip link add br62 type bridge && ip addr add 192.168.62.1/24 dev br62 && ip link set br62 up
for n in nas62:2 old62:3 cli62:10; do ns=${n%:*}; a=${n#*:}; ip netns add $ns; ip link add v$ns type veth peer name e$ns; ip link set v$ns master br62 up
	ip link set e$ns netns $ns; ip -n $ns addr add 192.168.62.$a/24 dev e$ns; ip -n $ns link set e$ns up; ip -n $ns link set lo up; ip -n $ns route add 224.0.0.0/4 dev e$ns; done
# the servers' users and passwords, made for this run (a comma and an equals sign in Ann's, as a mount option would have them)
useradd -M -s /bin/false $U1; useradd -M -s /bin/false $U2
P1="$(openssl rand -hex 6),vers=1.0=x"; P2=$(openssl rand -hex 8)
chown $U1 $T/sh/docs; chmod 755 $T/sh/docs; chmod 777 $T/sh/pub $T/sh/old
echo "from the NAS" > $T/sh/docs/onserver.txt; chown $U1 $T/sh/docs/onserver.txt
conf() {   # NAME ADDR MIN MAX
	mkdir -p $T/$1/priv $T/$1/lock $T/$1/state $T/$1/cache $T/$1/pid $T/$1/ncalrpc $T/$1/log
	cat > $T/$1/smb.conf <<EOF
[global]
 workgroup = HOME62
 netbios name = $(echo $1 | tr a-z A-Z)
 server role = standalone server
 map to guest = Bad User
 interfaces = $2
 bind interfaces only = yes
 lock directory = $T/$1/lock
 state directory = $T/$1/state
 cache directory = $T/$1/cache
 private dir = $T/$1/priv
 pid directory = $T/$1/pid
 ncalrpc dir = $T/$1/ncalrpc
 log file = $T/$1/log/%m.log
 disable netbios = yes
 server min protocol = $3
 server max protocol = $4
 ntlm auth = ntlmv2-only
$( [ $3 = NT1 ] && echo " lanman auth = no" )
[docs]
 path = $T/sh/docs
 valid users = $U1
 read only = no
 comment = Ann's documents
[pub]
 path = $T/sh/pub
 guest ok = yes
 read only = no
 force user = nobody
 comment = For everyone
[old]
 path = $T/sh/old
 guest ok = yes
EOF
}
conf nas 192.168.62.2 SMB2_10 SMB3; conf old 192.168.62.3 NT1 NT1
mkdir -p /run/samba
(printf '%s\n%s\n' "$P1" "$P1" | smbpasswd -c $T/nas/smb.conf -s -a $U1 && printf '%s\n%s\n' "$P2" "$P2" | smbpasswd -c $T/nas/smb.conf -s -a $U2) >/dev/null 2>&1
setsid ip netns exec nas62 smbd -s $T/nas/smb.conf -F --no-process-group > $T/nas.log 2>&1 & PIDS="$PIDS $!"
setsid ip netns exec old62 smbd -s $T/old/smb.conf -F --no-process-group > $T/old.log 2>&1 & PIDS="$PIDS $!"
ip netns exec nas62 wsdd -i enas62 -n WIN62 -w HOME62 > $T/wsdd.log 2>&1 & PIDS="$PIDS $!"
ip netns exec nas62 python3 $HERE/fixtures/smbmdns.py Nas62 192.168.62.2 > $T/mdns.log 2>&1 & PIDS="$PIDS $!"
for i in $(seq 1 30); do grep -q announced $T/mdns.log && timeout 1 bash -c '</dev/tcp/192.168.62.2/445' 2>/dev/null && timeout 1 bash -c '</dev/tcp/192.168.62.3/445' 2>/dev/null && break; timeout 1 tail -f /dev/null; done
RS=$T/rudolf-shares
# ---- rudolf-shares
o=$(RS_CELLULAR=/nonexistent $RS find); o2=$(RS_MDNS=127.0.0.1:9 $RS find)
ok "1. find: the NAS by multicast DNS (Nas62, nas62.local) and, asked by WS-Discovery alone, the Windows-like computer by its own name (WIN62, workgroup HOME62)" "$(echo "$o" | grep -q '"name":"Nas62","ip":"192.168.62.2","host":"nas62.local","how":"mdns"' && echo "$o2" | grep -q '"name":"WIN62","ip":"192.168.62.2","host":"WIN62","how":"wsd","group":"HOME62"' && echo 1)" "$o | $o2"
# with Rudolf OS's firewall on (a computer of its own on the same network: cli62, the rules of iso/sys/root/usr/lib/rudolf/firewall)
FW=$SYS/root/usr/lib/rudolf/firewall; mkdir -p /run/rudolf
if command -v nft >/dev/null && grep -q 'chain wsd' $FW; then
	ip netns exec cli62 sh $FW start; o=$(ip netns exec cli62 nft list chain inet rudolf wsd 2>&1); c0=$(ip netns exec cli62 nft list counter inet rudolf blocked | grep -o 'packets [0-9]*')
	ip netns exec cli62 sh $FW wsd open; o2=$(ip netns exec cli62 env RS_CELLULAR=/nonexistent $RS find)   # (as rudolfd does: the window for WS-Discovery's answers, ten seconds; Part 34's)
	o3=$(ip netns exec cli62 sh -c 'cat /proc/net/udp /proc/net/tcp' | awk 'NR>1 && $4 != "01" {split($2,a,":"); print a[2]}' | sort -u | tr '\n' ' ')
	ip netns exec nas62 python3 -c "import socket;s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);[s.sendto(b'x',('192.168.62.10',p)) for p in (3702,5353,445)];t=socket.socket();t.settimeout(1)
try: t.connect(('192.168.62.10',445))
except Exception as e: print(type(e).__name__)"; c1=$(ip netns exec cli62 nft list counter inet rudolf blocked | grep -o 'packets [0-9]*')
	ok "1b. with the firewall on: the wsd chain (3702 while \"firewall wsd open\"'s ten seconds last, private addresses, not cellular); found both ways (multicast DNS's and WS-Discovery's answers let in); nothing listening afterwards, and what the NAS sends to it dropped" "$(echo "$o" | grep -q 'iifname @cellular return' && echo "$o" | grep -q 'udp dport @wsdopen accept' && echo "$o2" | grep -q '"how":"mdns"' && [ -z "$(echo $o3)" ] && [ "$c0" != "$c1" ] && echo 1)" "$o | $o2 | listening: $o3 | $c0 -> $c1"
	ip netns exec cli62 sh $FW wsd open; o2=$(ip netns exec cli62 env RS_CELLULAR=/nonexistent RS_MDNS=127.0.0.1:9 $RS find)
	timeout 11 tail -f /dev/null; o3=$(ip netns exec cli62 env RS_CELLULAR=/nonexistent RS_MDNS=127.0.0.1:9 $RS find)
	ok "1d. ...and once the ten seconds are over, WS-Discovery's answers are dropped again (found nothing without \"wsd open\")" "$(echo "$o3" | grep -q '"servers":\[\]' && echo 1)" "$o3"
	ok "1c. ...and the Windows-like computer by WS-Discovery alone, through the firewall" "$(echo "$o2" | grep -q '"name":"WIN62".*"how":"wsd"' && echo 1)" "$o2"
else echo "NOTE: no nft here, or a firewall without the wsd chain (step 3): the firewall checks are skipped"; fi
o=$($RS resolve nas62); o2=$($RS resolve 192.168.62.3); o3=$($RS resolve 'a;b' 2>&1; echo "rc=$?")
ok "2. resolve: nas62 by multicast DNS (nas62.local), an address as it is; a name that is not one refused" "$([ "$o" = 192.168.62.2 ] && [ "$o2" = 192.168.62.3 ] && echo "$o3" | grep -q 'rc=2' && echo 1)" "$o $o2 $o3"
o=$($RS probe 192.168.62.2); o2=$($RS probe 192.168.62.3); o3=$($RS probe 192.168.62.9)
ok "3. probe: SMB 3.1.1 with encryption; the old server found to speak only SMB 1; nobody there: no answer" "$(echo "$o" | grep -q '"dialect":"3.1.1","encryption":true' && echo "$o2" | grep -q '"why":"smb1"' && echo "$o3" | grep -q '"why":"noanswer"' && echo 1)" "$o | $o2 | $o3"
o=$(printf '%s\n%s\n\n' $U1 "$P1" | $RS list nas 192.168.62.2); o2=$(printf '%s\nwrong\n\n' $U1 | $RS list nas 192.168.62.2); o3=$(printf '\n\n\n' | $RS list nas 192.168.62.2); o4=$(printf '%s\n%s\n\n' $U1 "$P1" | $RS list nas 192.168.62.2 2.1)
ok "4. list: Ann's view (docs, pub, old; not IPC\$), signed in with NTLMv2 over SMB 3.1.1; a wrong password said as such; a guest's list; over SMB 2.1 when asked" "$(echo "$o" | grep -q '"dialect":"3.1.1","shares":\[{"name":"docs","remark":"Ann.s documents"},{"name":"pub"' && ! echo "$o" | grep -q 'IPC' && echo "$o2" | grep -q '"why":"password"' && echo "$o3" | grep -q '"name":"pub"' && echo "$o4" | grep -q '"dialect":"2.1"' && echo 1)" "$o | $o2 | $o3 | $o4"
o=$(printf '\n\n\n' | $RS list old 192.168.62.3)
ok "5. list on the SMB 1 server: refused as SMB 1 (never spoken)" "$(echo "$o" | grep -q '"why":"smb1"' && echo 1)" "$o"
# ---- the drives helper
ID=0123456789abcdef; G=fedcba9876543210
DR() { $T/drives "$@" > $T/out 2> $T/err; echo $?; }
DRI() { i=$1; shift; printf '%b' "$i" | $T/drives "$@" > $T/out 2> $T/err; echo $?; }
r1=$(DRI "$U1\n$P1\n\n" share-mount 47110 $ID 192.168.62.2 nas docs 1.0 0); r2=$(DRI "x\ny\n\n" share-mount 47110 $ID 192.168.62.2 nas docs 2.0 0)
r3=$(DRI "x\ny\n\n" share-mount 47111 $ID 192.168.62.2 nas docs 3 0); r4=$(DRI "x\ny\n\n" share-mount 47110 $ID 192.168.62.2 nas 'docs,uid=0' 3 0)
r5=$(DRI "x\ny\n\n" share-mount 47110 $ID 192.168.62.2 'nas share' docs 3 0); r6=$(DRI "x\ny\n\n" share-mount 47110 ABC 192.168.62.2 nas docs 3 0)
r7=$(DRI "x\ny\n\n" share-mount 47110 $ID 127.0.0.1 nas docs 3 0); r8=$(DRI "a,uid=0\ny\n\n" share-mount 47110 $ID 192.168.62.2 nas docs 3 0)
r9=$(DRI "x\ny\n\n" share-mount 0 $ID 192.168.62.2 nas docs 3 0)
ok "6. refused (2, said in the health log): SMB 1.0 and 2.0, another user than the signed-in account's (or root), a share's name with an option in it, a server's name with a space, an id that is not one, this computer itself, a user name with an option in it" "$([ "$r1$r2$r3$r4$r5$r6$r7$r8$r9" = 222222222 ] && grep -q 'an SMB version other than 3 or 2.1' $T/health.log && grep -q 'another user than the signed-in account' $T/health.log && ! grep -q cifs /proc/mounts && echo 1)" "$r1 $r2 $r3 $r4 $r5 $r6 $r7 $r8 $r9 $(cat $T/health.log)"
if ! grep -qw cifs /proc/filesystems && ! modprobe cifs 2>/dev/null; then echo "NOTE: this kernel has no cifs module: the mount checks are skipped"; echo "$((N-F))/$N passed (mounts skipped)"; [ $F = 0 ]; exit; fi
r=$(DRI "$U1\n$P1\n\n" share-mount 47110 $ID 192.168.62.2 nas docs 3 0); M=$T/run/shares/47110/$ID
o=$(grep " $M " /proc/mounts); st=$(stat -c '%U:%u %a' $T/run/shares/47110 $M); cat $T/out $T/err
ok "7. Ann's share opened (SMB 3, encrypted: \"mounted encrypted\") at shares/47110/ID: cifs, nosuid, nodev, noexec, uid and gid 47110, seal; the folders 47110's own, 0700" "$([ $r = 0 ] && grep -q '^mounted encrypted$' $T/out && echo "$o" | grep -q '^//nas/docs .* cifs rw,nosuid,nodev,noexec' && echo "$o" | grep -q 'uid=47110,forceuid,gid=47110,forcegid' && echo "$o" | grep -q ',seal' && echo "$st" | tr '\n' ' ' | grep -q '47110 700 .*47110 700' && echo 1)" "$r $(cat $T/out $T/err) | $o | $st"
o=$(cat /proc/mounts /proc/self/mountinfo $T/health.log $T/out $T/err | grep -cF "$P1")
ok "8. the password (with its commas) in none of /proc/mounts, mountinfo, the health log or what the helper said" "$([ "$o" = 0 ] && echo 1)" "$o"
o=$(cat $M/onserver.txt; stat -c '%u %a' $M/onserver.txt); echo "from Rudolf" > $M/fromrudolf.txt; sync; o2=$(stat -c '%U %s' $T/sh/docs/fromrudolf.txt; stat -c '%u %a' $M/fromrudolf.txt)
ok "9. a file each way: the server's read here as 47110's (0600); one written here is on the server as Ann's (its Samba user), here 47110's" "$(echo "$o" | grep -q '^from the NAS' && echo "$o" | grep -q '^47110 600$' && echo "$o2" | grep -q "^$U1 12$" && echo "$o2" | grep -q '^47110 600$' && echo 1)" "$o | $o2"
o=$(setpriv --reuid 47111 --regid 47111 --clear-groups ls $T/run/shares/47110 2>&1; echo "rc=$?"); o2=$(setpriv --reuid 47111 --regid 47111 --clear-groups cat $M/onserver.txt 2>&1; echo "rc=$?")
ok "10. another account's Linux user (47111) can neither list Ann's shares nor read her share's files" "$(echo "$o" | grep -q 'Permission denied' && echo "$o2" | grep -q 'Permission denied' && echo 1)" "$o | $o2"
r=$(DRI "$U1\n$P1\n\n" share-mount 47110 $ID 192.168.62.2 nas docs 3 0)
ok "11. opened again: \"open already\", nothing mounted twice" "$([ $r = 0 ] && grep -q 'open already' $T/out && [ $(grep -c " $M " /proc/mounts) = 1 ] && echo 1)" "$r $(cat $T/out)"
r=$(DRI "$U1\nwrong\n\n" share-mount 47110 1111111111111111 192.168.62.2 nas docs 3 0); o=$(cat $T/out)
r2=$(DRI "$U1\n$P1\n\n" share-mount 47110 2222222222222222 192.168.62.2 nas nothere 3 0); o2=$(cat $T/out)
r3=$(DRI "\n\n\n" share-mount 47110 3333333333333333 192.168.62.3 old old 3 1); o3=$(cat $T/out)
ok "12. a wrong password: 3 (\"password\"); a share the server does not have: 4; the SMB 1 server not mounted (5 or 6: the kernel has no SMB 1; rudolfd then asks rudolf-shares probe, which says SMB 1, check 3); no folder left behind" "$([ $r = 3 ] && [ "$o" = password ] && [ $r2 = 4 ] && [ "$o2" = noshare ] && { [ $r3 = 6 ] || [ $r3 = 5 ]; } && ! grep -q '192.168.62.3' /proc/mounts && [ ! -e $T/run/shares/47110/1111111111111111 ] && [ ! -e $T/run/shares/47110/3333333333333333 ] && echo 1)" "$r $o | $r2 $o2 | $r3 $o3 $(cat $T/err)"
r=$(DRI "\n\n\n" share-mount 47110 $G 192.168.62.2 nas pub 3 1); echo guest > $T/run/shares/47110/$G/g.txt 2>/dev/null; o=$(ls -l $T/sh/pub/g.txt 2>&1)
ok "13. the guest share opened as a guest (no name, no password) and written to" "$([ $r = 0 ] && grep -q mounted $T/out && echo "$o" | grep -q 'g.txt' && echo 1)" "$r $(cat $T/out $T/err) | $o"
echo uBOB000002 > $T/run/home-session; r=$(DR share-umount 47110 $ID); echo uANN000001 > $T/run/home-session
ok "14. another account signed in now: Ann's share cannot be closed (or opened) for her uid" "$([ $r = 2 ] && grep -q " $M " /proc/mounts && echo 1)" "$r"
r=$(DR share-umount 47110 $ID)
ok "15. Disconnect (share-umount): unmounted, its folder gone" "$([ $r = 0 ] && ! grep -q " $M " /proc/mounts && [ ! -e $M ] && echo 1)" "$r $(cat $T/out $T/err)"
r=$(DRI "$U1\n$P1\n\n" share-mount 47110 $ID 192.168.62.2 nas docs 2.1 0); o=$(grep " $M " /proc/mounts)
ok "16. SMB 2.1 when asked: mounted with vers=2.1 (not encrypted: SMB 2.1 has none)" "$([ $r = 0 ] && grep -q '^mounted$' $T/out && echo "$o" | grep -q 'vers=2.1' && echo 1)" "$r $(cat $T/out $T/err) | $o"
r=$(DR share-umount-all 47110); o=$(grep -c " $T/run/shares/" /proc/mounts)
ok "17. signed out (share-umount-all): every one of Ann's shares closed" "$([ $r = 0 ] && [ "$o" = 0 ] && grep -q '^2 closed$' $T/out && echo 1)" "$r $(cat $T/out) $o"
o=$(grep -cF "$P1" $T/health.log $T/out $T/err $T/nas/log/* 2>/dev/null | awk -F: '{s+=$NF} END {print s+0}')
ok "18. the password nowhere afterwards (the health log, the helper's answers, Samba's own logs)" "$([ "$o" = 0 ] && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
