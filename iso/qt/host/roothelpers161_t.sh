#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 11): rudolfd's root work in helpers. rudolf-rootd (who may ask it, what it runs) and
# the drives helper's checks (built with -DRR_TEST: a stand-in tree for /sys, /home/rudolf, /media and the scripts,
# which here only say how they were called), then rudolfd's API for drives with the helper run by rudolfd itself and
# through rudolf-rootd: the same answers. In a private mount namespace (port 8080 must be free). Stops only what it
# started. Needs root, gcc, curl, python3.   sh roothelpers161_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$RH_INNER" ] && exec env RH_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/rh161.XXXXXX); chmod 755 $T
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
RDP=; BKP=
# (uid 47103 and 47105: the accounts' users of the API's checks below, made in this namespace's copy of /etc; only
# their programs, which this test started, are stopped)
trap 'for p in $RDP $BKP; do kill $p 2>/dev/null; done; pkill -KILL -U 47103 2>/dev/null; pkill -KILL -U 47105 2>/dev/null; umount -l $T/media/sdb1 /usr/share/rudolf/www /home/rudolf /run/rudolf/dl /home $T/oldhome /persist /etc /run 2>/dev/null; rm -rf $T' EXIT

# ---- the stand-in tree
mkdir -p $T/sys/class/block/sdb $T/sys/class/block/sdb1 $T/sys/block/sdb $T/lib $T/home/rudolf/Downloads $T/home/rudolf/Documents $T/media $T/run/drives $T/root $T/etc
for s in devplug install.sh bootfiles.sh mkstick.sh keepfiles.sh vault-move; do
	printf '#!/bin/sh\necho "%s $*|sha=$RUDOLF_IMG_SHA|path=$PATH|leak=$LEAKME|uid=$(id -u)" >> %s/calls\n[ "$1" = --info ] || echo "ran %s $*"\n[ "$1" = --info ] && echo "{\\"rudolf\\":true,\\"info\\":{\\"disk\\":\\"$2\\"}}"\n[ "$1" = eject ] && [ "$2" = sdb ] && { echo "a file on the drive is still open" >&2; exit 1; }\nexit 0\n' "$s" $T "$s" > $T/lib/$s
	chmod 755 $T/lib/$s
done
printf '#!/bin/sh\necho "ru $*|path=$PATH|leak=$LEAKME" >> %s/calls\ncase "$1" in status) echo "{\\"ok\\":true,\\"slot\\":\\"a\\"}";; stable-info) echo "{\\"stable\\":\\"1.6.0\\"}";; speedtest) echo "bytes 1000"; echo "ms 20";; rollback) echo "rolled back to slot b";; developer) echo "Developer mode is $2";; set) [ "$3" = bad ] && echo "no such value";; esac\nexit 0\n' $T > $T/ru; chmod 755 $T/ru
mkdir -p $T/persist/os/current; echo old > $T/persist/os/current/index.html
echo iso > $T/home/rudolf/Downloads/good.iso; chown 1000:1000 $T/home/rudolf/Downloads/good.iso
ln -s /etc/passwd $T/home/rudolf/Downloads/link.iso; ln -s /etc $T/home/rudolf/Downloads/out
echo other > $T/home/rudolf/Documents/other.iso; chown 1005:1005 $T/home/rudolf/Documents/other.iso
echo mine > $T/home/rudolf/Documents/mine.iso; chown 1003:1003 $T/home/rudolf/Documents/mine.iso
echo secret > $T/etc/secret; ln $T/etc/secret $T/home/rudolf/Downloads/hard.iso
printf 'uABCdef12 1003 moved\nuOTHER999 1005\n' > $T/aumap; echo uABCdef12 > $T/run/home-session
cat > $T/rr.conf <<EOF
RR_TEST_SYS=$T/sys
RR_TEST_LIB=$T/lib
RR_TEST_HOME=$T/home/rudolf
RR_TEST_MEDIA=$T/media
RR_TEST_RUN=$T/run
RR_TEST_AUMAP=$T/aumap
RR_TEST_HEALTH=$T/health.log
RR_TEST_DIR=$T/root
RR_TEST_UPDATE=$T/ru
RR_TEST_PERSIST=$T/persist
EOF
CF="-O2 -Wall -Wno-format-truncation -DRR_TEST -DRR_TEST_CONF=\"$T/rr.conf\""
gcc $CF -o $T/root/drives $SYS/rudolf-drives.c && gcc $CF -o $T/root/updates $SYS/rudolf-updates.c && gcc $CF -o $T/rudolf-rootd $SYS/rudolf-rootd.c || { echo "FAIL build"; exit 1; }
gcc $CF -w -o $T/drives-home.real $SYS/rudolf-drives-home.c -lz && gcc $CF -w -o $T/root/accounts $SYS/rudolf-accounts.c -lz || { echo "FAIL build drives-home, accounts"; exit 1; }
chmod 700 $T/root/drives $T/root/updates $T/drives-home.real $T/root/accounts
# (first a stand-in for drives-home, which says how the drives helper called it: its arguments, what came on its
# standard input, its environment)
printf '#!/bin/sh\nk=$(cat | md5sum | cut -c1-8)\necho "dh $*|in=$k|path=$PATH|leak=$LEAKME|uid=$(id -u)" >> %s/calls\necho made\nexit 0\n' $T > $T/root/drives-home; chmod 700 $T/root/drives-home
D=$T/root/drives
calls() { cat $T/calls 2>/dev/null; : > $T/calls; }
logged() { c=$(grep -c "$1" $T/health.log 2>/dev/null); echo ${c:-0}; }

# ---- the drives helper's checks (run as rudolf-rootd runs it: no environment)
o=$(env -i $D mount sdb1 2>&1); c=$(calls)
ok "1. mount of a partition the kernel has: devplug mount NAME, with nothing of the caller's environment" "$(echo "$c" | grep -q '^devplug mount sdb1|sha=|path=/usr/sbin:/usr/bin:/sbin:/bin|leak=|' && echo 1)" "$o $c"
o=$(env -i LEAKME=1 $D mount sdb1 2>&1); c=$(calls)
ok "...the scripts get no variable of the helper's caller" "$(echo "$c" | grep -q 'leak=|' && echo 1)" "$c"
b=0; for d in ../sdb1 'sdb1;reboot' 'sdb1 x' SDB1 'sdb1/../../x' '' "$(printf 'sdb1\nx')" .. sdb-1 sdb_1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa; do $D mount "$d" >/dev/null 2>&1; [ $? = 2 ] || b=1; done; c=$(calls)
ok "2. a name that is not a device's (.., /, ;, spaces, upper case, too long, empty) is refused, nothing run" "$([ $b = 0 ] && [ -z "$c" ] && echo 1)" "$b $c"
ok "...and said in the health log (Security)" "$([ "$(logged 'drives helper refused a device name')" -ge 10 ] && echo 1)" "$(tail -2 $T/health.log)"
h=$(logged refused); $D mount sdz1 >/dev/null 2>&1; r=$?; c=$(calls)
ok "3. a well-formed name the kernel does not have (a drive just pulled out): refused, not said as an attack" "$([ $r = 2 ] && [ -z "$c" ] && [ "$(logged refused)" = $h ] && echo 1)" "$r $c"
$D mount sdb1 extra >/dev/null 2>&1; r=$?; $D frobnicate sdb1 >/dev/null 2>&1; r2=$?
ok "...extra arguments and unknown actions are refused" "$([ $r = 2 ] && [ $r2 = 2 ] && [ -z "$(calls)" ] && echo 1)" "$r $r2"
SHA=ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789
$D install sdb $T/home/rudolf/Downloads/good.iso --sha=$SHA >/dev/null 2>&1; r=$?; c=$(calls)
ok "4. install: the whole disk and an image in the home folder, its checksum handed on in lower case" "$([ $r = 0 ] && echo "$c" | grep -q "^install.sh /dev/sdb $T/home/rudolf/Downloads/good.iso|sha=$(echo $SHA | tr A-F a-f)|" && echo 1)" "$r $c"
ok "...said in the health log (it erases a disk)" "$([ "$(logged 'Install: /dev/sdb is being written')" = 1 ] && echo 1)" "$(tail -1 $T/health.log)"
$D install sdb1 >/dev/null 2>&1; r=$?; $D install sdc >/dev/null 2>&1; r2=$?
ok "5. install onto a partition, or onto a disk the kernel does not have: refused" "$([ $r = 2 ] && [ $r2 = 2 ] && [ -z "$(calls)" ] && echo 1)" "$r $r2"
b=0; for i in "$T/home/rudolf/Downloads/../../../etc/passwd" /etc/passwd "$T/home/rudolf/Downloads/link.iso" "$T/home/rudolf/Downloads/out/passwd" "$T/home/rudolf/Downloads/.hidden.iso" "$T/home/rudolf/Downloads/a b.iso" "$T/home/rudolf/Downloads/x;y.iso" "$T/home/rudolfX/a.iso" "$T/home/rudolf/Documents/other.iso" "$T/home/rudolf/Downloads/hard.iso" "$T/home/rudolf/Downloads"; do
	$D install sdb "$i" >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: $i)"; }; done; c=$(calls)
ok "6. images refused: \"..\", outside the home folder, a link (to a file or a folder outside), hidden, odd characters, another account's file, a hard link to root's, a folder" "$([ $b = 0 ] && [ -z "$c" ] && echo 1)" "$b $c"
$D install sdb $T/home/rudolf/Documents/mine.iso >/dev/null 2>&1; r=$?; $D install sdb $T/home/rudolf/Downloads/parts.iso >/dev/null 2>&1; r2=$?; c=$(calls)
ok "7. allowed: the signed-in account's own file, and an image not there yet (still in parts: joinparts.sh joins them)" "$([ $r = 0 ] && [ $r2 = 0 ] && [ "$(echo "$c" | grep -c '^install.sh /dev/sdb ')" = 2 ] && echo 1)" "$r $r2 $c"
echo uOTHER999 > $T/run/home-session; $D install sdb $T/home/rudolf/Documents/mine.iso >/dev/null 2>&1; r=$?; $D install sdb $T/home/rudolf/Documents/other.iso >/dev/null 2>&1; r2=$?; echo uABCdef12 > $T/run/home-session
ok "...whose file it is follows the account signed in (another account's is refused, the one signed in allowed)" "$([ $r = 2 ] && [ $r2 = 0 ] && echo 1)" "$r $r2 $(calls)"
$D install sdb $T/home/rudolf/Downloads/good.iso --sha=1234 >/dev/null 2>&1; r=$?; $D install sdb --sha=$SHA --sha=$SHA >/dev/null 2>&1; r2=$?
ok "8. a checksum that is not 64 hex digits, or two of them: refused" "$([ $r = 2 ] && [ $r2 = 2 ] && [ -z "$(calls)" ] && echo 1)" "$r $r2"
$D mkstick sdb "" 1 >/dev/null 2>&1; r=$?; $D mkstick sdb "" 2 >/dev/null 2>&1; r2=$?; $D bootfiles-write sdb "" >/dev/null 2>&1; r3=$?; $D bootfiles-stage $T/home/rudolf/Downloads/good.iso >/dev/null 2>&1; r4=$?; $D keepfiles >/dev/null 2>&1; r5=$?; $D keepfiles now >/dev/null 2>&1; r6=$?; c=$(calls)
ok "9. Make a stick (files 0 or 1 only), Replace the boot files, the next start's boot files, Keep files: the scripts as rudolfd ran them" "$([ $r = 0 ] && [ $r2 = 2 ] && [ $r3 = 0 ] && [ $r4 = 0 ] && [ $r5 = 0 ] && [ $r6 = 2 ] && echo "$c" | grep -q '^mkstick.sh /dev/sdb  1|' && echo "$c" | grep -q '^bootfiles.sh write /dev/sdb |' && echo "$c" | grep -q "^bootfiles.sh stage $T/home/rudolf/Downloads/good.iso|" && echo "$c" | grep -q '^keepfiles.sh make|' && echo 1)" "$r $r2 $r3 $r4 $r5 $r6 $c"
o=$($D info sdb 2>&1); r=$?; $D info sdb1 >/dev/null 2>&1; r2=$?; c=$(calls)
ok "10. a stick's information: mkstick.sh --info /dev/DISK for a whole disk only" "$([ $r = 0 ] && [ $r2 = 2 ] && echo "$o" | grep -q '"disk":"/dev/sdb"' && echo 1)" "$r $r2 $o"
mkdir -p $T/media/sdb1; mount -t tmpfs none $T/media/sdb1; echo '{}' > $T/run/drives/sdb1
$D umount sdb1 >/dev/null 2>&1; r=$?
ok "11. unmount: /media/NAME unmounted, its folder and its record gone" "$([ $r = 0 ] && ! mountpoint -q $T/media/sdb1 2>/dev/null && [ ! -e $T/media/sdb1 ] && [ ! -e $T/run/drives/sdb1 ] && echo 1)" "$r $(ls $T/media)"
mkdir -p $T/media/sdb1; ln -s / $T/media/sdb1/x 2>/dev/null; $D umount sdb1 >/dev/null 2>&1; r=$?
ok "...nothing mounted there: as before (its status 0, the shell's rm -f's), nothing else touched" "$([ $r = 0 ] && [ -d $T/media/sdb1 ] && echo 1)" "$r"
rm -rf $T/media/sdb1

OIFS=$IFS
# ---- 1.6.1 drives part 2: the accounts' folders and sessions (drives home-*): the account checked, the key on standard
# input only, then drives-home (rudolfd's own functions) with exactly those arguments
printf 'uABCdef12 1003 moved\nuOTHER999 1005\n-uGONE1234 1007\n' > $T/aumap
K=$(head -c 64 /dev/urandom | od -An -tx1 | tr -d ' \n'); KS=$(printf %s "$K" | md5sum | cut -c1-8)
o=$(printf %s "$K" | env -i LEAKME=1 $D home-unlock uABCdef12 1 1003 2>&1); r=$?; c=$(calls)
ok "H1. home-unlock: drives-home unlock ACCT 1 UID, the key on its standard input, nothing of the caller's environment" "$([ $r = 0 ] && [ "$o" = made ] && [ "$c" = "dh unlock uABCdef12 1 1003|in=$KS|path=/usr/sbin:/usr/bin:/sbin:/bin|leak=|uid=0" ] && echo 1)" "$r $o $c"
printf %s "$K" | $D home-unlock uABCdef12 0 1000 >/dev/null 2>&1; r=$?; $D home-lock uOTHER999 >/dev/null 2>&1; r2=$?; $D home-bind uABCdef12 1003 >/dev/null 2>&1; r3=$?; $D home-store >/dev/null 2>&1; r4=$?; c=$(calls)
ok "H2. ...without the session and with the desktop's user as the owner; home-lock, home-bind (rudolfd's start), home-store" "$([ $r$r2$r3$r4 = 0000 ] && echo "$c" | grep -q '^dh unlock uABCdef12 0 1000|' && echo "$c" | grep -q '^dh lock uOTHER999|' && echo "$c" | grep -q '^dh bind uABCdef12 1003|' && echo "$c" | grep -q '^dh store|' && echo 1)" "$r$r2$r3$r4 $c"
b=0; for x in .. ../uABCdef12 uABC/def a.b 'uABC def' '' 'u;id' "$(printf 'uA\nb')" uABCdef12x_ "$(printf 'u%.0s' $(seq 65))" '*' '$(id)'; do $D home-lock "$x" >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: $x)"; }; printf %s "$K" | $D home-unlock "$x" 1 1000 >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: $x)"; }; done; c=$(calls)
ok "H3. an account name that is not one (.., /, spaces, a newline, odd characters, too long, empty): refused, drives-home not run" "$([ $b = 0 ] && [ -z "$c" ] && echo 1)" "$b $c"
ok "...said in the health log (Security)" "$([ "$(logged 'drives helper refused an account name that is not one')" -ge 24 ] && echo 1)" "$(logged 'drives helper refused an account name')"
h=$(logged refused); o=$(printf %s "$K" | $D home-unlock guest 1 1000); r=$?; o2=$($D home-lock shared); r2=$?
ok "H4. the guest and the shared folder: 400 bad account, as rudolfd answered, not said as an attack" "$([ $r = 100 ] && [ $r2 = 100 ] && [ "$o" = 'bad account' ] && [ "$(logged refused)" = $h ] && [ -z "$(calls)" ] && echo 1)" "$r $r2 $o"
printf %s "$K" | $D home-unlock uGONE1234 1 1000 >/dev/null 2>&1; r=$?; $D home-lock uGONE1234 >/dev/null 2>&1; r2=$?
ok "H5. an account the account list says was removed: refused, said" "$([ $r = 2 ] && [ $r2 = 2 ] && [ -z "$(calls)" ] && [ "$(logged 'refused a removed account')" = 2 ] && echo 1)" "$r $r2"
b=0; for a in "home-unlock|uABCdef12|1" "home-unlock|uABCdef12|1|1000|$K" "home-unlock|uABCdef12|2|1000" "home-unlock|uABCdef12|1|1005" "home-unlock|uABCdef12|1|0" "home-unlock|uABCdef12|1|1003x" "home-unlock|uABCdef12|1|" "home-unlock|uOTHER999|1|1003" "home-bind|uABCdef12" "home-bind|uABCdef12|1005" "home-bind|uGONE1234|1007" "home-lock" "home-lock|uABCdef12|x" "home-store|now" "home-lock|uABCdef12|--sha=$SHA" "home-frob|uABCdef12"; do
	IFS='|'; set -- $a; IFS=$OIFS; printf %s "$K" | $D "$@" >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: $a)"; }; done; c=$(calls)
ok "H6. the wrong arguments: the key as an argument, a session that is not 0 or 1, an owner that is another account's or root's, a removed account's, a checksum, an unknown action: refused, nothing run" "$([ $b = 0 ] && [ -z "$c" ] && echo 1)" "$b $c"
mv $T/root/drives-home $T/dh.standin; o=$($D home-lock uABCdef12 2>/dev/null); r=$?; mv $T/dh.standin $T/root/drives-home
ok "H7. drives-home missing: 500 (status 200), said" "$([ $r = 200 ] && echo "$o" | grep -q 'could not be opened' && echo 1)" "$r $o"
o=$(printf %s "$K" | $D vault-unlock 2>&1); r=$?; $D vault-feature </dev/null >/dev/null 2>&1; r2=$?; $D vault-prepare </dev/null >/dev/null 2>&1; r3=$?; $D vault-move </dev/null >/dev/null 2>&1; r4=$?; c=$(calls)
ok "H8. the vault (1.5.0's encrypted folder): vault-unlock with its key on standard input, vault-feature, vault-prepare to drives-home; vault-move the script" "$([ $r$r2$r3$r4 = 0000 ] && echo "$c" | grep -q "^dh vault-unlock|in=$KS|path=/usr/sbin:/usr/bin:/sbin:/bin|leak=|uid=0" && echo "$c" | grep -q '^dh vault-feature|' && echo "$c" | grep -q '^dh vault-prepare|' && echo "$c" | grep -q '^vault-move |' && echo 1)" "$r$r2$r3$r4 $c"
b=0; for a in "vault-unlock|$K" "vault-feature|/dev/sda" "vault-prepare|/dev/sda" "vault-move|now" "vault-unlock|--sha=$SHA"; do IFS='|'; set -- $a; IFS=$OIFS; $D "$@" </dev/null >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: $a)"; }; done
ok "...any argument for them (a key, a device, a checksum): refused, nothing run" "$([ $b = 0 ] && [ -z "$(calls)" ] && echo 1)" "$b"
printf 'uABCdef12 1003 moved\nuOTHER999 1005\n' > $T/aumap

# ---- 1.6.1 accounts: the accounts helper's checks (refusals only here: what it does is checked below with the API, in
# this namespace's copy of /etc, and by users59_t.sh with US_VIA=helper)
A=$T/root/accounts; b=0
for x in '../x' 'guest' 'shared' 'a b' 'u;id' "$(printf 'u%.0s' $(seq 81))" ''; do for op in sync start; do printf '%s\n' "$x" | $A $op >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: $op $x)"; }; done; done
printf 'uABCdef12' | $A sync >/dev/null 2>&1; [ $? = 2 ] || b=1; $A sync extra </dev/null >/dev/null 2>&1; [ $? = 2 ] || b=1
for x in '../../etc' "app:x'; id; '" '/etc/passwd' ''; do $A keyring-forget "$x" </dev/null >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: keyring-forget $x)"; }; done
$A keyring-list now </dev/null >/dev/null 2>&1; [ $? = 2 ] || b=1; $A frob </dev/null >/dev/null 2>&1; [ $? = 2 ] || b=1
ok "A1. accounts: a list with an id that is not one (.., the guest, shared, a space, odd characters, too long, empty, no last newline), extra arguments, a saved password's id that is not one, an unknown action: refused" "$([ $b = 0 ] && echo 1)" "$b"
ok "...said in the health log (Security)" "$([ "$(logged 'accounts helper refused')" -ge 20 ] && echo 1)" "$(logged 'accounts helper refused')"

# ---- the updates helper's checks
U=$T/root/updates
for c in check download rollback stable-info to-stable status speedtest boot-ok; do env -i $U $c >/dev/null 2>&1 || echo "rc $c"; done > $T/u.rc; c=$(calls)
ok "12a. updates: rudolf-update's own commands, each as it is, with nothing of the caller's environment" "$([ ! -s $T/u.rc ] && [ "$(echo "$c" | grep -c '^ru [a-z-]*|path=/usr/sbin:/usr/bin:/sbin:/bin|leak=$')" = 8 ] && echo 1)" "$(cat $T/u.rc) $c"
b=0; OIFS=$IFS; for a in "check|now" "status|--x" "return-official" "return-official|maybe" "set" "set|url" "set|url|a;b" "set|url|a b" 'set|url|$(id)' "set|auto|'x'" "set|channel|nightly" "set|other|x" "set|url|https://merciless146.github.io/RudolfOS/beta/" "developer|on" "developer|on|short|https://x" "developer|on|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=|" "developer|on|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=|https://x/a?b" "developer|maybe" "page-install|now" "frobnicate"; do
	IFS='|'; set -- $a; IFS=$OIFS; $U "$@" >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: $a)"; }; done; c=$(calls)
ok "12b. updates: arguments where none belong, an unknown setting or channel, odd characters, the beta address as an update address, a key that is not 44 base64 characters: refused, nothing run" "$([ $b = 0 ] && [ -z "$c" ] && echo 1)" "$b $c"
ok "...said in the health log" "$([ "$(logged 'updates helper refused')" -ge 20 ] && echo 1)" "$(logged 'updates helper refused')"
$U return-official erase >/dev/null; $U set channel beta >/dev/null; $U set url https://example.org/x/ >/dev/null; $U developer on AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA= https://dev.example/r >/dev/null; $U developer off >/dev/null; c=$(calls)
ok "12c. updates: Return to official, the channel, an address, Developer mode on and off: handed on as they are, logged" "$(echo "$c" | grep -q '^ru return-official erase|' && echo "$c" | grep -q '^ru set channel beta|' && echo "$c" | grep -q '^ru set url https://example.org/x/|' && echo "$c" | grep -q '^ru developer on AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA= https://dev.example/r|' && echo "$c" | grep -q '^ru developer off|' && grep -q 'Developer mode was turned on' $T/health.log && echo 1)" "$c"
head -c 50000 /dev/zero | tr '\0' x | $U page-install >/dev/null 2>&1; r=$?; { printf '<html>'; head -c 150000 /dev/zero | tr '\0' x; } | $U page-install >/dev/null 2>&1; r2=$?
ok "12d. the desktop's page: one that is not a page (or too small) refused; one that is: the old page kept as the previous one" "$([ $r = 2 ] && [ $r2 = 0 ] && [ "$(cat $T/persist/os/previous/index.html)" = old ] && [ "$(head -c 6 $T/persist/os/current/index.html)" = '<html>' ] && [ ! -e $T/persist/os/new ] && echo 1)" "$r $r2 $(ls -R $T/persist/os)"
$U page-rollback; r=$?; $U page-rollback; r2=$?
ok "...rolled back: the previous page is the current one again; with none, the current one goes (status 0 both times, as before)" "$([ $r = 0 ] && [ $r2 = 0 ] && [ ! -e $T/persist/os/current ] && echo 1)" "$r $r2 $(ls -R $T/persist/os)"

# ---- rudolf-rootd: only the program it started may ask, only known helpers, never a shell
cat > $T/cl.c <<'EOF'
#define _GNU_SOURCE
#include "rudolf-root.h"
#include <sys/wait.h>
/* a stand-in for rudolfd: cl ACTION... ; prints the status rudolf-rootd answered */
static int ask(int master, const char *msg, size_t n, int nfd)
{
	int sp[2]; socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sp);
	rr_send(master, "c", 1, &sp[1], 1); close(sp[1]);
	int fds[3] = { 0, 1, 2 }; rr_send(sp[0], msg, n, fds, nfd);
	int32_t st = -1; if (recv(sp[0], &st, 4, 0) != 4) st = -1; close(sp[0]); return st;
}
static size_t req(char *b, const char *const *a) { size_t n = 5; memcpy(b, "RRQ1", 5); for (; *a; a++) { strcpy(b + n, *a); n += strlen(*a) + 1; } return n; }
int main(int argc, char **argv)
{
	const char *v = getenv("RUDOLF_ROOTD_FD"); int m = v ? atoi(v) : -1; char b[70000]; size_t n;
	if (m < 3) { puts("nofd"); return 3; }
	if (!strcmp(argv[1], "ok")) { const char *a[] = { "drives", "mount", "sdb1", NULL }; n = req(b, a); printf("%d\n", ask(m, b, n, 3)); }
	else if (!strcmp(argv[1], "shell")) { const char *a[] = { "sh", "-c", "id > /tmp/rh-pwned", NULL }; n = req(b, a); printf("%d\n", ask(m, b, n, 3)); }
	else if (!strcmp(argv[1], "acc")) { const char *a[] = { "accounts", "sync", NULL }; n = req(b, a); printf("%d\n", ask(m, b, n, 3)); }
	else if (!strcmp(argv[1], "dh")) { const char *a[] = { "drives-home", "lock", "uABCdef12", NULL }; n = req(b, a); printf("%d\n", ask(m, b, n, 3)); }
	else if (!strcmp(argv[1], "dots")) { const char *a[] = { "../drives", "mount", "sdb1", NULL }; n = req(b, a); printf("%d\n", ask(m, b, n, 3)); }
	else if (!strcmp(argv[1], "magic")) { const char *a[] = { "drives", "mount", "sdb1", NULL }; n = req(b, a); b[0] = 'X'; printf("%d\n", ask(m, b, n, 3)); }
	else if (!strcmp(argv[1], "fds")) { const char *a[] = { "drives", "mount", "sdb1", NULL }; n = req(b, a); printf("%d\n", ask(m, b, n, 2)); }
	else if (!strcmp(argv[1], "long")) { memcpy(b, "RRQ1", 5); memset(b + 5, 'a', 5000); b[5005] = 0; printf("%d\n", ask(m, b, 5006, 3)); }
	else if (!strcmp(argv[1], "uid")) {
		/* the request's socket pair made by a process of another user (nobody), over the right descriptor */
		pid_t p = fork(); if (!p) { if (setgid(65534) || setuid(65534)) _exit(9); const char *a[] = { "drives", "mount", "sdb1", NULL }; n = req(b, a); printf("%d\n", ask(m, b, n, 3)); fflush(stdout); _exit(0); }
		waitpid(p, NULL, 0);
	} else if (!strcmp(argv[1], "other")) {
		/* ...or by another program that was given the descriptor (here: one that is not close-on-exec) */
		fcntl(m, F_SETFD, 0); execl(argv[2], argv[2], "ok", (char *)NULL); return 9;
	} else if (!strcmp(argv[1], "env")) { setenv("LEAKME", "1", 1); const char *a[] = { "drives", "mount", "sdb1", NULL }; n = req(b, a); printf("%d\n", ask(m, b, n, 3)); }
	else if (!strcmp(argv[1], "exit")) return 7;
	return 0;
}
EOF
gcc -O2 -I$SYS -o $T/cl $T/cl.c && gcc -O2 -I$SYS -o $T/cl2 $T/cl.c || { echo "FAIL build cl"; exit 1; }
RD() { $T/rudolf-rootd $T/cl "$@" 2>&1; }
o=$(RD ok); c=$(calls)
ok "12. a request from the program rudolf-rootd started: the helper runs (0), with its arguments" "$([ "$(echo "$o" | tail -1)" = 0 ] && echo "$c" | grep -q '^devplug mount sdb1|.*uid=0' && echo 1)" "$o $c"
o=$(RD env); c=$(calls)
ok "...with nothing of the caller's environment" "$(echo "$c" | grep -q 'leak=|' && [ "$(echo "$o" | tail -1)" = 0 ] && echo 1)" "$o $c"
rm -f /tmp/rh-pwned; h=$(logged 'rootd refused'); o=$(RD shell)
ok "13. a helper that is not one of the list (sh -c ...): refused (254), nothing run, said in the health log" "$([ "$(echo "$o" | tail -1)" = 254 ] && [ ! -e /tmp/rh-pwned ] && [ "$(logged 'rootd refused')" -gt $h ] && echo 1)" "$o"
o1=$(RD dots); o2=$(RD magic); o3=$(RD fds); o4=$(RD long)
ok "14. a name with \"..\", a malformed request, one without its three descriptors, an argument over 4096 bytes: refused" "$([ "$(echo "$o1" | tail -1)" = 254 ] && [ "$(echo "$o2" | tail -1)" = 254 ] && [ "$(echo "$o3" | tail -1)" = 254 ] && [ "$(echo "$o4" | tail -1)" = 254 ] && [ -z "$(calls)" ] && echo 1)" "$o1 $o2 $o3 $o4"
o=$(printf 'a b\n' | RD acc)
ok "14c. rudolf-rootd runs the accounts helper for rudolfd (here a list it refuses: 2)" "$([ "$(echo "$o" | tail -1)" = 2 ] && echo 1)" "$o"
o=$(RD dh)
ok "14b. drives-home asked for by its own name (it is the drives helper's only): refused (254), nothing run" "$([ "$(echo "$o" | tail -1)" = 254 ] && [ -z "$(calls)" ] && echo 1)" "$o"
o=$(RD uid)
ok "15. a request whose socket pair another user (uid 65534) made: refused, nothing run" "$([ "$(echo "$o" | tail -1)" = 254 ] && [ -z "$(calls)" ] && grep -q 'request from uid 65534' $T/health.log && echo 1)" "$o"
o=$(RD other $T/cl2)
ok "16. a request from another program, even holding rudolfd's descriptor: refused (its /proc/PID/exe is not the program started)" "$([ "$(echo "$o" | tail -1)" = 254 ] && [ -z "$(calls)" ] && grep -q "request from $T/cl2" $T/health.log && echo 1)" "$o $(tail -1 $T/health.log)"
$T/rudolf-rootd $T/cl exit >/dev/null 2>&1; r=$?
ok "17. rudolf-rootd ends with its program's exit status" "$([ $r = 7 ] && echo 1)" "$r"
o=$($T/cl ok 2>&1); s=$(nm -D $T/rudolf-rootd 2>/dev/null | grep -cwE 'bind|listen|accept|accept4|connect')
ok "18. nothing to connect to: without the descriptor a program has no way in (rudolf-rootd never binds, listens or accepts)" "$(echo "$o" | grep -q nofd && [ "$s" = 0 ] && nm -D $T/rudolf-rootd | grep -qw socketpair && echo 1)" "$o $s"

# ---- rudolfd's API for drives: the helper run by rudolfd itself (root, no rudolf-rootd), and through rudolf-rootd
# (1.6.1 drives part 2: /persist an ext4 drive with the encrypt feature, as RUDOLFDATA is, for the accounts' encrypted
# folders; /home and /etc this namespace's own, for the accounts' Linux users and their homes)
truncate -s 96M $T/pimg; mkfs.ext4 -q -F -O encrypt $T/pimg; mkdir -p $T/etc.copy; cp -a /etc/. $T/etc.copy/; mount --bind $T/etc.copy /etc
# (this tree may be under /home itself, as on GitHub's runner: kept in view at its place)
TOP=$(cd $HERE/../../.. && pwd -P); mkdir -p $T/oldhome; mount --bind /home $T/oldhome
mount -t tmpfs none /run; mkdir -p /run/rudolf /usr/share/rudolf/www /persist; mount -t tmpfs none /home; mkdir -p /home/rudolf
case "$TOP" in /home/*) mkdir -p "$TOP"; mount --bind "$T/oldhome/${TOP#/home/}" "$TOP";; esac
mount --bind $HERE/../../www /usr/share/rudolf/www; mount -t tmpfs none /home/rudolf; mount -o loop $T/pimg /persist || { echo "FAIL no loop device"; exit 1; }
gcc -O2 -w -DRD_ROOT_DIR="\"$T/root\"" -DRU_BIN="\"$T/ru\"" -o $T/rudolfd $SYS/rudolfd.c -lz || { echo "FAIL build rudolfd"; exit 1; }
mkdir -p $T/none; gcc -O2 -w -DRD_ROOT_DIR="\"$T/none\"" -DRU_BIN="\"$T/ru\"" -o $T/rudolfd-inproc $SYS/rudolfd.c -lz || { echo "FAIL build rudolfd"; exit 1; }
cp $T/drives-home.real $T/root/drives-home; chmod 700 $T/root/drives-home
# (the stand-in home for the helper is the real /home/rudolf here, a tmpfs, as rudolfd's API checks the image's place)
sed -i "s|^RR_TEST_HOME=.*|RR_TEST_HOME=/home/rudolf|; s|^RR_TEST_PERSIST=.*|RR_TEST_PERSIST=/persist|; s|^RR_TEST_AUMAP=.*|RR_TEST_AUMAP=/persist/etc/rudolf/account-users|" $T/rr.conf
printf 'RR_TEST_HOMEROOT=/persist/accounts\nRR_TEST_NOBIND=0\nRR_TEST_AUON=1\nRR_TEST_AUHOMES=/home\nRR_TEST_RUNUSER=/run/user\nRR_TEST_DISPLAY=%s/nodisplay\n' $T >> $T/rr.conf
mkdir -p /persist/etc/rudolf; printf 'uABCdef12 47103 moved\nuOTHER999 47105 moved\n-uGONE1234 47107\n' > /persist/etc/rudolf/account-users
: > /run/rudolf/persistent; mkdir -p /home/rudolf/Downloads /home/rudolf/Documents; echo iso > /home/rudolf/Downloads/r.iso; echo before > /home/rudolf/Documents/before.txt; chown -R 1000:1000 /home/rudolf
api() { m=$1; p=$2; curl -s -X $m -H "X-Rudolf-Token: $TOK" "http://127.0.0.1:8080/api/$p"; echo; }
jobwait() { w8 'curl -s -H "X-Rudolf-Token: $TOK" http://127.0.0.1:8080/api/job | grep -q "\"running\":false"' 40; api GET job | sed 's/"log":"[^"]*"/"log":LOG/'; }
probe() {
	api POST heartbeat >/dev/null; w8 '[ -e /run/rudolf/bootok ]' 20; timeout 1 tail -f /dev/null; rm -f /run/rudolf/bootok /run/rudolf/update.log
	api POST 'mount?dev=sdb1'; api POST 'mount?dev=..%2Fsdb1'; api POST 'umount?dev=sdb1'; api POST 'eject?dev=sdb1'; api POST 'eject?dev=sdb'
	api GET 'stick/info?disk=sdb'; api GET 'stick/info?disk=sdq'
	api POST 'install?disk=sdb&confirm=sdb&image=%2Fhome%2Frudolf%2FDownloads%2Fr.iso&sha='$SHA; jobwait
	api POST 'install?disk=sdb&confirm=sdb&bootonly=1'; jobwait
	api POST 'mkstick?disk=sdb&confirm=sdb&files=1'; jobwait
	api POST 'keepfiles'; jobwait
	api GET sys/status; api GET sys/stable; api POST sys/check; jobwait; api POST sys/rollback; api POST 'sys/set?k=url&v=https%3A%2F%2Fexample.org%2Fr'
	api POST 'sys/set?k=auto&v=bad'; api POST 'sys/developer?on=1&key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA%3D&url=https%3A%2F%2Fdev.example'; api POST 'sys/developer?on=0'
	api POST 'sys/return?erase=0'; jobwait; api GET speedtest; api POST sys/beta?on=0
	{ printf '<html>'; head -c 120000 /dev/zero | tr '\0' x; } > $T/page.html; curl -s -X POST -H "X-Rudolf-Token: $TOK" --data-binary @$T/page.html http://127.0.0.1:8080/api/update; echo
	api POST rollback; ls /persist/os 2>&1 | tr '\n' ' '; echo
}
# the accounts' folders: sign-in with the session, the state it leaves (owners, the binds, the session's files), a
# second account, another's key, a link planted in place of a folder, sign-out; each run from a fresh start
K1=$(head -c 64 /dev/urandom | od -An -tx1 | tr -d ' \n'); K2=$(head -c 64 /dev/urandom | od -An -tx1 | tr -d ' \n')
hpost() { printf %s "$2" | curl -s -X POST -H "X-Rudolf-Token: $TOK" --data-binary @- "http://127.0.0.1:8080/api/$1"; echo; }
hstate() {
	stat -c '%n %u:%g %a' /persist/accounts/uABCdef12 /persist/accounts/uABCdef12/Documents /persist/oracle/uABCdef12 /persist/accounts/uOTHER999 /home/acct47103 /run/user/47103 2>&1 | sed 's/^stat: //'
	awk '$5 ~ "^/home/rudolf|^/run/rudolf/dl|^/home/acct" {print $5, $4}' /proc/self/mountinfo | sort
	cat /run/rudolf/home-session 2>&1; sed 's/^DBUS_SESSION_BUS_ADDRESS=.*/DBUS (a bus)/' /run/rudolf/acct-session.env 2>&1; ls /home/rudolf/Documents; ls -A /run/rudolf/home-keys 2>&1
}
hprobe() {
	hpost 'home/unlock?acct=uABCdef12&session=1' "$K1"; hstate; api GET keyring/list; grep -h '^acct' /etc/passwd /etc/group /etc/shadow | sort
	api GET home; hpost 'home/unlock?acct=uOTHER999' "$K2"; hpost 'home/unlock?acct=uOTHER999' "$K1"; hpost 'home/unlock?acct=uLINKed01' "$K1"
	hpost 'home/unlock?acct=guest' "$K1"; hpost 'home/unlock?acct=uABCdef12&session=1' "$K1"
	hpost 'home/lock?acct=uABCdef12'; hstate; hpost 'home/lock?acct=uOTHER999'; hpost 'home/lock?acct=uNOTHERE1'; api GET home
	api GET vault; hpost vault/unlock "$K1"; hpost vault/move
	ls -ld /persist/accounts/uLINKed01 | cut -c1; ls /etc/home-keys /etc/Documents 2>&1 | grep -c "No such"
}
hreset() { rm -rf /persist/accounts /persist/oracle /run/rudolf/home-session /run/rudolf/home-keys /run/rudolf/acct-session.env; mkdir -p /persist/accounts; ln -s /etc /persist/accounts/uLINKed01; }
start() { : > $T/rd.log; $1 > $T/rd.log 2>&1 & RDP=$!; w8 'curl -s http://127.0.0.1:8080/favicon.png -o /dev/null' 60
	TOK=$(curl -s http://127.0.0.1:8080/ | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1); }
stop() { kill $RDP 2>/dev/null; wait $RDP 2>/dev/null; RDP=; w8 '! curl -s -o /dev/null http://127.0.0.1:8080/favicon.png' 40; }
hreset; start $T/rudolfd; A=$(probe); CA=$(calls); HA=$(hprobe); stop
grep -q 'run by rudolfd itself' $T/rd.log; d1=$?
hreset; start $T/rudolfd-inproc; HC=$(hprobe); stop
hreset; start "$T/rudolf-rootd $T/rudolfd"; B=$(probe); CB=$(calls); HB=$(hprobe)
RPID=$(pgrep -f "^$T/rudolfd" | head -1); RFD=$(tr '\0' '\n' < /proc/$RPID/environ | sed -n 's/^RUDOLF_ROOTD_FD=//p'); FDS="fd $RFD: $(cat /proc/$RPID/fdinfo/$RFD 2>&1 | grep flags) $(readlink /proc/$RPID/fd/$RFD)"
grep -q 'through rudolf-rootd' $T/rd.log; d2=$?; stop
ok "19. rudolfd says how it runs the helpers (by itself as root; through rudolf-rootd)" "$([ $d1 = 0 ] && [ $d2 = 0 ] && echo 1)" "$(cat $T/rd.log | head -5)"
ok "20. the API for drives answers the same, byte for byte, either way (mount, unmount, eject, a stick's information, install, Replace the boot files, Make a stick, Keep files, a refusal)" "$([ -n "$A" ] && [ "$A" = "$B" ] && echo 1)" "$(printf '%s\n---\n%s' "$A" "$B" | head -40)"
ok "...and the helper ran the same scripts the same way" "$([ -n "$CA" ] && [ "$CA" = "$CB" ] && echo "$CA" | grep -q "^install.sh /dev/sdb /home/rudolf/Downloads/r.iso|sha=$(echo $SHA | tr A-F a-f)|" && echo "$CA" | grep -q '^bootfiles.sh write /dev/sdb |' && echo "$CA" | grep -q '^mkstick.sh /dev/sdb  1|' && echo 1)" "$(printf '%s\n---\n%s' "$CA" "$CB")"
ok "...the answers as before 1.6.1: mounted, a bad device refused by rudolfd, safe to remove, eject's own error, the stick's information, the jobs ended 0" "$(echo "$A" | sed -n 1p | grep -q '"mounted"' && echo "$A" | sed -n 2p | grep -q 'bad device' && echo "$A" | sed -n 3p | grep -q 'safe to remove' && echo "$A" | sed -n 5p | grep -q 'still open' && echo "$A" | sed -n 6p | grep -q '"disk":"/dev/sdb"' && echo "$A" | sed -n 8p | grep -q '"rudolf":false' && [ "$(echo "$A" | grep -c '"status":0')" = 6 ] && echo 1)" "$A"
ok "20b. ...and for updates: status, a check (a job), roll back, the settings, Developer mode, Return to official, the speed test, beta off, the desktop's page and its roll back" "$(echo "$A" | grep -q '"slot":"a"' && echo "$A" | grep -q 'rolled back to slot b' && echo "$A" | grep -q 'no such value' && echo "$A" | grep -q 'Developer mode is on' && echo "$A" | grep -q 'update installed; restart to use it' && echo "$A" | grep -q 'returned to the previous version' && echo "$CA" | grep -q '^ru check|' && echo "$CA" | grep -q '^ru return-official keep|' && echo "$CA" | grep -q '^ru set channel stable|' && echo "$CA" | grep -q '^ru boot-ok|' && echo 1)" "$A $CA"
ok "22. the accounts' folders (sign-in with the session, the binds, a second account, another's key, a planted link, the guest, sign-out): the same answers and the same state through the helper (rudolfd as root, and through rudolf-rootd) as rudolfd doing it itself" "$([ -n "$HA" ] && [ "$HA" = "$HB" ] && [ "$HA" = "$HC" ] && echo 1)" "$(printf '%s\n---\n%s\n---\n%s' "$HA" "$HB" "$HC" | head -80)"
ok "...and they are 1.6.0's: made, the folder the account's user's (0700), its Oracle folder the desktop's, the binds, the session's home, unlocked, another's key 403, the link 500 with /etc untouched, the guest 400, locked: the binds gone, the folder before them back" "$(echo "$HA" | sed -n 1p | grep -q '"message":"made"' && echo "$HA" | grep -q '^/persist/accounts/uABCdef12 47103:47103 700' && echo "$HA" | grep -q '^/persist/oracle/uABCdef12 1000:1000 700' && echo "$HA" | grep -q '^/home/rudolf/Documents /accounts/uABCdef12/Documents' && echo "$HA" | grep -q '^/home/acct47103 /accounts/uABCdef12' && echo "$HA" | grep -q '^HOME=/home/acct47103' && echo "$HA" | grep -q 'that key does not open' && echo "$HA" | grep -q 'bad account' && echo "$HA" | grep -q '"message":"locked"' && echo "$HA" | grep -q '^before.txt' && echo "$HA" | tail -2 | head -1 | grep -qx l && [ "$(echo "$HA" | tail -1)" = 2 ] && echo 1)" "$HA"
ok "23. a removed account (in the account list as removed): refused through the helper (the one change: rudolfd alone made its folder)" "$(hreset; start $T/rudolfd; o=$(hpost 'home/unlock?acct=uGONE1234' "$K1"); stop; echo "$o" | grep -q 'bad account' && [ ! -e /persist/accounts/uGONE1234 ] && echo 1)" ""
ok "21. rudolfd keeps rudolf-rootd's descriptor close-on-exec (nothing it starts has it)" "$(echo "$FDS" | python3 -c 'import sys,re; t=sys.stdin.read(); m=re.search(r"flags:\s*([0-7]+)", t); print(1 if m and int(m.group(1), 8) & 0o2000000 and "socket:" in t else 0)')" "$FDS"
echo "$((N-F))/$N passed"; [ $F = 0 ]
