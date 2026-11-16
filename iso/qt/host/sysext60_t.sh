#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md Part 13): signed system extensions. rpk (built from sys/rpk.c) with throw-away keys made here
# (a "system" key standing in for the release key, a Library key, and a key added with "rpk key add"), and
# /usr/lib/rudolf/sysext laying them over a stand-in system root in a private mount namespace: the signature (only the
# system key), the base ABI and the kernel, what an extension may hold, the overlay with two extensions in name order,
# read-only, a clash with the base or with another extension refused, the start script and its time limit, the trial
# (left out after a start whose desktop never came up, Try again), Start once without extensions, Recovery mode, a file
# changed after it was installed, a revocation, removal leaving nothing behind, kernel modules and depmod; rudolfd's
# sysext/ requests (fixtures/sysextapi.c, accounts made as the page makes them): only an administrator's password.
# Needs root (loop devices, mount namespaces), mksquashfs and python3's cryptography.   sh sysext60_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
command -v mksquashfs >/dev/null || { echo "SKIP: needs mksquashfs (squashfs-tools)"; exit 0; }
[ -z "$SX60_INNER" ] && exec env SX60_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../..; TOOL=$I/pkgtools/rpk-tool.py; SX=$I/sys/root/usr/lib/rudolf/sysext
T=$(mktemp -d /tmp/sysext60.XXXXXX); trap 'for m in $(grep " $T/" /proc/mounts | cut -d" " -f2 | sort -r); do umount -l $m 2>/dev/null; done; rm -rf $T' EXIT
gcc -O2 -o $T/rpk $I/sys/rpk.c -lcrypto 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
for k in sys lib added; do python3 $TOOL keygen $T/$k >/dev/null; done
KV=6.18.9-test; ABI=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
# the stand-in system: its base files, the base ABI, a kernel modules folder
TOP=$T/top; mkdir -p $TOP/usr/bin $TOP/usr/share/rudolf $TOP/usr/lib/rudolf $TOP/lib/firmware $TOP/lib/modules/$KV/kernel
echo base > $TOP/usr/bin/base-tool; echo $ABI > $TOP/usr/share/rudolf/base-abi; echo fw > $TOP/lib/firmware/base.bin
export RPK_ROOT=$T/root RPK_MNT=$T/mnt RPK_DISC=$T/nodisc RPK_KEYS=$T/sys/rpk-signing.pub RPK_LIBRARY_KEYS=$T/lib/rpk-signing.pub RPK_SOURCES=$T/sources
export RPK_BASE_ABI=$TOP/usr/share/rudolf/base-abi RPK_KERNEL=$KV RPK_HEALTH=$T/health
export SYSEXT_TOP=$TOP SYSEXT_RUN=$T/run/ext SYSEXT_RPK=$T/rpk SYSEXT_KVER=$KV SYSEXT_HEALTH="$T/health extension" SYSEXT_CMDLINE=$T/cmdline SYSEXT_TEST_PATH=$T/bin
printf '#!/bin/sh\n[ "$1" = --health ] && shift\necho "$*" >> %s/hlog\n' $T > $T/health; chmod +x $T/health; : > $T/hlog
mkdir -p $T/bin; printf '#!/bin/sh\necho "depmod $*" >> %s/depmod.log\n' $T > $T/bin/depmod; chmod +x $T/bin/depmod
echo quiet > $T/cmdline; : > $T/sources
N=0; F=0; ok() { N=$((N+1)); case "$2" in *[!1]*|'') r=0;; *) r=1;; esac; if [ $r = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
# mk NAME VERSION KEY "PKGINFO lines" FILE=CONTENT...   an extension laid out and signed (the base ABI unless given)
mk() { n=$1; v=$2; k=$3; extra=$4; shift 4; d=$T/p/$n-$v; rm -rf $d; mkdir -p $d
	printf 'name = %s\nversion = %s\nkind = sysext\ntitle = %s\nsummary = test\nlicense = MIT\n' $n $v $n > $d/PKGINFO
	case "$extra" in *base*) ;; *) echo "base = $ABI" >> $d/PKGINFO;; esac; [ -n "$extra" ] && printf '%s\n' "$extra" >> $d/PKGINFO
	for fc in "$@"; do f=${fc%%=*}; mkdir -p $d/$(dirname $f); printf '%s\n' "${fc#*=}" > $d/$f; case $f in *.start) chmod 755 $d/$f;; esac; done
	python3 $TOOL build $d $T/$n-$v.rpk --key $T/$k/rpk-signing.key >/dev/null || echo "mk $n failed"; }
asm() { rm -rf $T/run; mkdir -p $T/run; unshare -m sh -c "sh $SX assemble; $1" 2>&1; }
state() { cat $T/run/ext/state 2>/dev/null; }

mk aaa 1.0 sys "" usr/bin/aaa-tool=aaa usr/lib/rudolf/ext.d/aaa.start='echo started > '$T'/aaa.mark; echo "aaa says hello"'
o=$($T/rpk install-file --system $T/aaa-1.0.rpk 2>&1); r=$?
o0=$($T/rpk install-file $T/aaa-1.0.rpk 2>&1); r0=$?
ok "0. the App Store's way (no --system: no administrator's password checked) cannot install a system extension" "$([ $r0 != 0 ] && echo 1)$(has "$o0" 'installed by an administrator')$([ ! -e $T/root/db/aaa.info ] && echo 1)" "$r0 $o0"
ok "1. an extension signed by the system key installs, is not mounted with the apps, and is in use after a restart" "$([ $r = 0 ] && echo 1)$(has "$o" 'in use after the next restart')$([ ! -d $T/mnt/aaa ] && echo 1)$(grep -qx 'signed = system' $T/root/db/aaa.info && echo 1)" "$r $o"
ok "...said in the health log as an extension" "$(grep -q '^extension installed the system extension aaa 1.0' $T/hlog && echo 1)" "$(cat $T/hlog)"
s=$($T/rpk status); ok "...the App Store's list does not show it (it is not an app)" "$(has "$s" '"installed":[]')" "$s"
s=$($T/rpk sysext-status); ok "...Settings' list does: not in use until the restart" "$(has "$s" '"name":"aaa","version":"1.0"')$(has "$s" '"state":"next"')$(has "$s" "\"base\":\"$ABI\"")" "$s"
mk lext 1.0 lib "" usr/bin/lext=x; o=$($T/rpk install-file --system $T/lext-1.0.rpk 2>&1); r=$?
ok "2. one signed by a Library key is refused" "$([ $r != 0 ] && echo 1)$(has "$o" "must be signed by Rudolf OS's release key")$([ ! -e $T/root/db/lext.info ] && echo 1)" "$r $o"
ok "...said in the health log" "$(grep -q 'refused the system extension lext 1.0' $T/hlog && echo 1)" "$(cat $T/hlog)"
$T/rpk key add "$(cat $T/added/rpk-signing.pub)" >/dev/null; mk kext 1.0 added "" usr/bin/kext=x; o=$($T/rpk install-file --system $T/kext-1.0.rpk 2>&1); r=$?
ok "...and one signed by a key added with rpk key add (not the release key)" "$([ $r != 0 ] && echo 1)$(has "$o" "release key")" "$r $o"
mk other 1.0 sys "base = ffff" usr/bin/other=x; o=$($T/rpk install-file --system $T/other-1.0.rpk 2>&1); r=$?
ok "3. one built for another base is refused" "$([ $r != 0 ] && echo 1)$(has "$o" 'built for another Rudolf OS base')" "$r $o"
mk kold 1.0 sys "kernel = 6.1.0-old" lib/modules/6.1.0-old/extra/x.ko=ko; o=$($T/rpk install-file --system $T/kold-1.0.rpk 2>&1); r=$?
ok "...and one built for another kernel" "$([ $r != 0 ] && echo 1)$(has "$o" 'built for Linux 6.1.0-old')" "$r $o"
mk etcx 1.0 sys "" etc/passwd=x; o=$($T/rpk install-file --system $T/etcx-1.0.rpk 2>&1); r=$?
ok "4. one with files outside usr/, lib/firmware and lib/modules is refused" "$([ $r != 0 ] && echo 1)$(has "$o" 'outside usr/')" "$r $o"
mk scr 1.0 sys "" usr/lib/rudolf/ext.d/other.start=x; o=$($T/rpk install-file --system $T/scr-1.0.rpk 2>&1); r=$?
ok "...and one whose start script is not its own NAME.start" "$([ $r != 0 ] && echo 1)$(has "$o" 'only start script')" "$r $o"
mk mod 1.0 sys "" lib/modules/$KV/extra/x.ko=ko; o=$($T/rpk install-file --system $T/mod-1.0.rpk 2>&1); r=$?
ok "...and kernel modules without the kernel line" "$([ $r != 0 ] && echo 1)$(has "$o" 'kernel modules are not for the kernel')" "$r $o"
# the overlay: aaa and bbb, in name order
mk bbb 2.1 sys "kernel = $KV" usr/bin/bbb-tool=bbb lib/firmware/bbb.bin=fwb lib/modules/$KV/extra/bbb.ko=ko; $T/rpk install-file --system $T/bbb-2.1.rpk >/dev/null 2>&1
o=$(asm "cat $TOP/usr/bin/aaa-tool $TOP/usr/bin/bbb-tool $TOP/usr/bin/base-tool $TOP/lib/firmware/bbb.bin $TOP/lib/firmware/base.bin $TOP/lib/modules/$KV/extra/bbb.ko; touch $TOP/usr/bin/new 2>/dev/null && echo WRITABLE; grep -c ' $TOP/usr ' /proc/mounts")
ok "5. at start: both extensions laid over the base (their files and the base's, in /usr, /lib/firmware, /lib/modules)" "$(has "$o" "aaa
bbb
base
fwb
fw
ko")" "$o"
ok "...read-only, and the base is never written" "$(has "$o" WRITABLE | tr 01 10)$([ ! -e $TOP/usr/bin/aaa-tool ] && [ ! -e $TOP/usr/bin/new ] && echo 1)" "$o"
ok "...in name order, both in use" "$([ "$(state | cut -f1,3 | tr '\t\n' ' ')" = "start aaa in-use bbb in-use " ] && echo 1)" "$(state)"
ok "...depmod lists the extensions' kernel modules (in a scratch layer in /run)" "$(grep -q "^depmod -a -b $TOP $KV" $T/depmod.log && echo 1)" "$(cat $T/depmod.log 2>&1)"
ok "6. the start script ran as root, its output in the extension's log" "$(grep -qx started $T/aaa.mark && grep -q 'aaa says hello' $T/run/ext/aaa.log && echo 1)" "$(cat $T/run/ext/aaa.log 2>&1)"
ok "7. the trial: the extensions used for the first time are written down" "$(grep -qx 'aaa 1.0' $T/root/sysext/trying && grep -qx 'bbb 2.1' $T/root/sysext/trying && echo 1)" "$(cat $T/root/sysext/trying 2>&1)"
s=$(RPK_SYSEXT_STATE=$T/run/ext/state $T/rpk sysext-status); ok "...Settings shows them in use" "$(has "$s" '"name":"aaa","version":"1.0"')$(has "$s" '"state":"in-use"')$(has "$s" '"started":"with extensions"')" "$s"
o=$(asm "ls $TOP/usr/bin")
ok "...the desktop never answered: the next start leaves them out, says why, and the health log too" "$(has "$o" aaa-tool | tr 01 10)$(state | grep -q '^aaa	1.0	left-out	the desktop did not come up' && echo 1)$(grep -q 'left out the system extension aaa 1.0: the desktop did not come up' $T/hlog && echo 1)" "$o $(state)"
sh $SX retry aaa >/dev/null; sh $SX retry bbb >/dev/null; asm true >/dev/null; sh $SX good
ok "...Try again; the desktop answered: kept (no longer on trial)" "$([ ! -e $T/root/sysext/trying ] && grep -qx 'aaa 1.0' $T/root/sysext/good && grep -qx 'bbb 2.1' $T/root/sysext/good && echo 1)" "$(state; cat $T/root/sysext/good 2>&1)"
o=$(asm "ls $TOP/usr/bin"); ok "...and used at the next start with no new trial" "$(has "$o" aaa-tool)$([ ! -e $T/root/sysext/trying ] && echo 1)" "$o"
# clashes
mk ccc 1.0 sys "" usr/bin/base-tool=evil; $T/rpk install-file --system $T/ccc-1.0.rpk >/dev/null 2>&1
mk ddd 1.0 sys "" usr/bin/aaa-tool=evil2; $T/rpk install-file --system $T/ddd-1.0.rpk >/dev/null 2>&1
o=$(asm "cat $TOP/usr/bin/base-tool $TOP/usr/bin/aaa-tool")
ok "8. one with a file the base has is left out (an extension adds, never replaces), said and in the health log" "$(has "$o" "base
aaa")$(state | grep -q '^ccc	1.0	left-out	it would replace /usr/bin/base-tool, which the system already has' && echo 1)$(grep -q 'left out the system extension ccc 1.0: it would replace /usr/bin/base-tool' $T/hlog && echo 1)" "$o $(state)"
ok "...and one with a file an earlier extension has" "$(state | grep -q '^ddd	1.0	left-out	it would replace /usr/bin/aaa-tool' && echo 1)" "$(state)"
$T/rpk remove --system ccc >/dev/null; $T/rpk remove --system ddd >/dev/null; sh $SX good >/dev/null
# the start script's time limit
mk eee 1.0 sys "" usr/bin/eee=e usr/lib/rudolf/ext.d/eee.start='sleep 60'; $T/rpk install-file --system $T/eee-1.0.rpk >/dev/null 2>&1
t0=$(date +%s); SYSEXT_TIMEOUT=2 asm true >/dev/null; t1=$(date +%s)
ok "9. a start script that hangs is stopped after its time limit, said in the health log" "$([ $((t1 - t0)) -lt 15 ] && echo 1)$(grep -q "the system extension eee's start script did not finish in 2 s" $T/hlog && echo 1)" "$((t1 - t0)) s $(cat $T/hlog)"
$T/rpk remove --system eee >/dev/null
# no extensions: Recovery mode, rudolf.noext=1, Start once without extensions
echo "quiet rudolf.recovery=1" > $T/cmdline; o=$(asm "ls $TOP/usr/bin"); echo quiet > $T/cmdline
ok "10. Recovery mode starts without extensions, and says so" "$(has "$o" aaa-tool | tr 01 10)$(state | grep -q '^start	Recovery mode starts without system extensions' && echo 1)$(state | grep -q '^aaa	1.0	left-out	Recovery mode' && echo 1)" "$o $(state)"
echo "quiet rudolf.noext=1" > $T/cmdline; o=$(asm "ls $TOP/usr/bin"); echo quiet > $T/cmdline
ok "...so does rudolf.noext=1" "$(has "$o" aaa-tool | tr 01 10)$(state | grep -q 'rudolf.noext=1' && echo 1)" "$o"
sh $SX skip-once >/dev/null; s=$($T/rpk sysext-status); o=$(asm "ls $TOP/usr/bin"); o2=$(asm "ls $TOP/usr/bin")
ok "...Start once without extensions: the next start only" "$(has "$s" '"skipOnce":true')$(has "$o" aaa-tool | tr 01 10)$(has "$o2" aaa-tool)" "$s / $o / $o2"
sh $SX good; ok "...and a start without them does not change what was kept" "$(grep -qx 'aaa 1.0' $T/root/sysext/good && echo 1)" "$(cat $T/root/sysext/good)"
# a file changed after it was installed
cp $T/root/store/bbb.rpk $T/bbb.keep; printf 'x' | dd of=$T/root/store/bbb.rpk bs=1 seek=200 conv=notrunc 2>/dev/null
o=$(asm "ls $TOP/usr/bin"); ok "11. an extension whose file was changed after it was installed is left out" "$(has "$o" bbb-tool | tr 01 10)$(state | grep -q '^bbb	2.1	left-out	its file is no longer the signed one' && echo 1)" "$o $(state)"
cp $T/bbb.keep $T/root/store/bbb.rpk
echo "$ABI" | sed 's/0/1/' > $TOP/usr/share/rudolf/base-abi; o=$(asm "ls $TOP/usr/bin"); echo $ABI > $TOP/usr/share/rudolf/base-abi
ok "...and every extension after an update to another base (until its build for that base is installed)" "$(has "$o" aaa-tool | tr 01 10)$(state | grep -q '^aaa	1.0	left-out	it was built for another Rudolf OS base' && echo 1)" "$o $(state)"
# a revocation in the signed list
mkdir -p $T/repo; cp $T/aaa-1.0.rpk $T/repo/; echo 'pkg aaa *' > $T/repo/revoked.txt; python3 $TOOL index $T/repo --key $T/sys/rpk-signing.key --serial 3 >/dev/null
echo $T/repo > $T/sources; $T/rpk refresh >/dev/null 2>&1; o=$(asm "ls $TOP/usr/bin")
ok "12. a revoked extension is left out at the next start" "$(has "$o" aaa-tool | tr 01 10)$(state | grep -q '^aaa	1.0	left-out	the Rudolf OS Library revoked this package' && echo 1)" "$o $(state)"
rm $T/repo/revoked.txt; python3 $TOOL index $T/repo --key $T/sys/rpk-signing.key --serial 4 >/dev/null
s=$($T/rpk sysext-status); ok "13. the extensions' list (a source signed by the system key) is offered in Settings, not in the App Store" "$(has "$s" '"available":[{"name":"aaa","version":"1.0"')$(has "$($T/rpk status)" '"available":[]')" "$s"
# removal
o=$($T/rpk remove aaa 2>&1); ok "...nor remove one" "$(has "$o" 'an administrator removes it')$([ -e $T/root/db/aaa.info ] && echo 1)" "$o"
$T/rpk remove --system aaa >/dev/null; $T/rpk remove --system bbb >/dev/null; o=$(asm "ls $TOP/usr/bin $T/run/ext")
ok "14. removed: nothing of them is left at the next start" "$([ -z "$(ls $T/root/store $T/root/db 2>/dev/null)" ] && echo 1)$(has "$o" aaa | tr 01 10)$(state | grep -qx 'start	none installed' && echo 1)" "$o $(ls $T/root/store $T/root/db)"
ok "...said in the health log" "$(grep -q 'removed the system extension aaa 1.0' $T/hlog && echo 1)" "$(cat $T/hlog)"
ok "15. rudolf-init lays them over before udev, from the data partition; the session keeps them when the desktop answers" "$(awk '/sysext assemble/{a=NR} /^UDEVD=/{u=NR} END{print (a && u && a < u) ? 1 : 0}' $I/sys/root/sbin/rudolf-init)$(grep -q 'RPK_ROOT=/persist/pkg /usr/lib/rudolf/sysext assemble' $I/sys/root/sbin/rudolf-init && echo 1)$(grep -q 'sysext_ok;' $I/sys/root/usr/lib/rudolf/session && echo 1)"
# the self-test's line
ST=$I/sys/root/usr/lib/rudolf/selftest; mkdir -p $T/st/ext
printf 'start\twith extensions\naaa\t1.0\tin-use\t\nbbb\t2.1\tin-use\t\n' > $T/st/ext/state; o=$(ST_RUN=$T/st ST_PERSIST=$T/nop ST_SYS=$T/nop ST_PROC=$T/nop sh $ST 2>/dev/null | grep ' extensions ')
ok "15b. the self-test: the extensions in use" "$([ "$o" = "PASS extensions - in use: aaa 1.0, bbb 2.1" ] && echo 1)" "$o"
printf 'ccc\t1.0\tleft-out\tit would replace /usr/bin/base-tool, which the system already has\n' >> $T/st/ext/state; o=$(ST_RUN=$T/st ST_PERSIST=$T/nop ST_SYS=$T/nop ST_PROC=$T/nop sh $ST 2>/dev/null | grep ' extensions ')
ok "...fails on one left out (a clash, a mismatch), saying why" "$(has "$o" "FAIL extensions - left out: ccc 1.0 (it would replace /usr/bin/base-tool")" "$o"
printf 'start\tRecovery mode starts without system extensions\n' > $T/st/ext/state; o=$(ST_RUN=$T/st ST_PERSIST=$T/nop ST_SYS=$T/nop ST_PROC=$T/nop sh $ST 2>/dev/null | grep ' extensions ')
ok "...skipped when the start used none, saying why" "$(has "$o" "SKIP extensions - Recovery mode")" "$o"
# rudolfd: Settings' requests (sysext/status, install, remove, retry, skip-once), an administrator's password checked
mount -t tmpfs none /run; mkdir -p /run/rudolf /persist; mount -t tmpfs none /persist; touch /run/rudolf/persistent
printf '#!/bin/sh\necho "rpk $*" >> %s/api.log\n[ "$1" = sysext-status ] && echo "{\\"ok\\":true,\\"installed\\":[]}"\nexit 0\n' $T > $T/fakerpk
printf '#!/bin/sh\necho "sysext $*" >> %s/api.log\necho "said $1"\n' $T > $T/fakesx; chmod 755 $T/fakerpk $T/fakesx
gcc -O1 -w -DSX_RPK="\"$T/fakerpk\"" -DSX_SH="\"$T/fakesx\"" -o $T/api $HERE/fixtures/sysextapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build of the API"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json >/dev/null || { echo "FAIL node"; exit 1; }
api() { $T/api $T/ls.json "$@"; }; body() { printf '%s' "$1" > $T/b; }; : > $T/api.log
o=$(api "GET sysext/status"); ok "16. rudolfd: the extensions' state for Settings (rpk sysext-status)" "$(has "$o" '200 OK')$(has "$o" '"installed":[]')" "$o"
body '{"n":"Bob","p":"bob-pass"}'; o=$(api "POST sysext/install?name=usbprint <$T/b")
ok "...installing needs an administrator: a standard account is refused" "$(has "$o" '403')$(has "$o" 'only an administrator')$(grep -q install $T/api.log && echo 0 || echo 1)" "$o"
body '{"n":"Ann","p":"wrong"}'; o=$(api "POST sysext/install?name=usbprint <$T/b"); o2=$(api "POST sysext/skip-once")
ok "...a wrong password, or none, too" "$(has "$o" '403')$(has "$o" 'Wrong password')$(has "$o2" '403')$(grep -q 'install\|skip' $T/api.log && echo 0 || echo 1)" "$o $o2"
body '{"n":"Ann","p":"pw-ann \303\274"}'; printf '{"n":"Ann","p":"pw-ann \303\274"}' > $T/b
o=$(api "POST sysext/install?name=usbprint <$T/b"); for i in 1 2 3 4 5 6 7 8 9 10; do [ -e /run/rudolf/job.status ] && break; sleep 0.3; done
ok "...an administrator with the password: rpk install --system, as a job, in the health log" "$(has "$o" '200')$(grep -qx 'rpk install --system usbprint' $T/api.log && echo 1)$(grep -q 'installation of the system extension usbprint asked by Ann' /persist/var/log/health.log && echo 1)" "$o $(cat $T/api.log)"
rm -f /run/rudolf/job.name; o=$(api "POST sysext/remove?name=usbprint <$T/b" "POST sysext/retry?name=usbprint <$T/b" "POST sysext/skip-once <$T/b"); sleep 1
ok "...remove, Try again and Start once without extensions" "$(grep -qx 'rpk remove --system usbprint' $T/api.log && grep -qx 'sysext retry usbprint' $T/api.log && grep -qx 'sysext skip-once' $T/api.log && echo 1)" "$o $(cat $T/api.log)"
o=$(api "POST sysext/install?name=a%3Bb <$T/b" "POST sysext/install?name=../x <$T/b" "POST sysext/what <$T/b")
ok "...a bad name or action is refused" "$(has "$o" 'bad extension name')$(has "$o" 'unknown extension action')$(grep -q 'a;b\|\.\./x\|what' $T/api.log && echo 0 || echo 1)" "$o $(cat $T/api.log)"
# (1.6.0, found in QEMU, qt/sb/t160x_t.py) the start-up marks the data partition as in use before the assembly, so its
# health-log lines (left out, a start script stopped) go to the data partition's health log (System health), not /run's
RI=$I/sys/root/sbin/rudolf-init; lt=$(grep -n '^mountpoint -q /persist && touch /run/rudolf/persistent' $RI | head -1 | cut -d: -f1); la=$(grep -n 'sysext assemble' $RI | grep -v '^#' | grep -v ':#' | head -1 | cut -d: -f1)
ok "16. rudolf-init: the data partition is marked in use before the extensions are laid over (their health-log lines kept)" "$([ -n "$lt" ] && [ -n "$la" ] && [ "$lt" -lt "$la" ] && echo 1)" "touch at ${lt:-none}, assemble at ${la:-none}"
echo "$((N-F))/$N passed"; [ $F = 0 ]
