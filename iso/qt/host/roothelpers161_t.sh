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
trap 'for p in $RDP $BKP; do kill $p 2>/dev/null; done; pkill -KILL -U 47103 2>/dev/null; pkill -KILL -U 47105 2>/dev/null; umount -l $T/media/sdb1 /usr/share/rudolf/www /usr/lib/rudolf /home/rudolf /run/rudolf/dl /home $T/oldhome /persist /etc /run 2>/dev/null; [ -n "$ULR_MADE" ] && rmdir /usr/lib/rudolf; rm -rf $T' EXIT

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
# (RH_CONFINE=on: rudolf-rootd puts plan Part 20's confinement on each helper, as /persist/etc/rudolf-confine would)
[ -n "$RH_CONFINE" ] && { echo "$RH_CONFINE" > $T/confine; echo "RR_TEST_CONFINE_FILE=$T/confine" >> $T/rr.conf; echo "(rudolf-rootd's confinement: $RH_CONFINE)"; }
CF="-O2 -Wall -Wno-format-truncation -DRR_TEST -DRR_TEST_CONF=\"$T/rr.conf\""
gcc $CF -o $T/root/drives $SYS/rudolf-drives.c && gcc $CF -o $T/root/updates $SYS/rudolf-updates.c && gcc $CF -o $T/rudolf-rootd $SYS/rudolf-rootd.c || { echo "FAIL build"; exit 1; }
gcc $CF -w -o $T/drives-home.real $SYS/rudolf-drives-home.c -lz && gcc $CF -w -o $T/root/accounts $SYS/rudolf-accounts.c -lz && gcc $CF -o $T/root/power $SYS/rudolf-power.c || { echo "FAIL build drives-home, accounts, power"; exit 1; }
chmod 700 $T/root/drives $T/root/updates $T/drives-home.real $T/root/accounts $T/root/power
# (1.6.1 power: the power settings, hotkey, the desktop's priority and the shutdown script; and the system's commands
# the power helper runs, found first in $T/bin: none of them does anything here but say how it was called)
for s in power hotkey desktop-priority shutdown; do printf '#!/bin/sh\necho "%s $*|path=$PATH|leak=$LEAKME" >> %s/calls\n[ "$1" = status ] && echo "{\\"ok\\":true,\\"s\\":1}"\n[ "$1" = set ] && echo "{\\"ok\\":true,\\"set\\":\\"$*\\"}"\n[ "$1" = brightness ] && echo 40\n[ "$1" = on ] && echo "the desktop first"\n[ "$1" = reboot ] && echo "shutdown steps"\nexit 0\n' $s $T > $T/lib/$s; chmod 755 $T/lib/$s; done
mkdir -p $T/bin; for c in pkill umount hwclock ntpd; do printf '#!/bin/sh\necho "%s $*" >> %s/calls\n[ "$1 $4" = "-n pool.ntp.org" ] && [ -e %s/ntp-down ] && exit 1\n[ "$1 $4" = "-n time.google.com" ] && [ -e %s/ntp-down2 ] && exit 1\nexit 0\n' $c $T $T $T > $T/bin/$c; chmod 755 $T/bin/$c; done
mkdir -p $T/sys/class/backlight/acpi_video0 $T/sys/class/backlight/intel_backlight $T/sys/class/rtc/rtc0 $T/sys/power $T/proc/sys/vm
echo 1000 > $T/sys/class/backlight/intel_backlight/max_brightness; echo 300 > $T/sys/class/backlight/intel_backlight/brightness; echo 15 > $T/sys/class/backlight/acpi_video0/max_brightness
: > $T/sys/class/rtc/rtc0/wakealarm; : > $T/sys/power/state; : > $T/proc/sys/vm/drop_caches
printf 'RR_TEST_PROC=%s/proc\nRR_TEST_BIN=%s/bin\n' $T $T >> $T/rr.conf
# (1.6.1 network: the network helper; stand-ins for wifi.sh, the firewall, the hotspot and netplug, and for ip, udhcpc
# and wpa_cli in $T/bin: none of them touches this computer's network, each only says how it was called)
gcc $CF -o $T/root/network $SYS/rudolf-network.c || { echo "FAIL build network"; exit 1; }; chmod 700 $T/root/network
gcc $CF -o $T/root/devices $SYS/rudolf-devices.c || { echo "FAIL build devices"; exit 1; }; chmod 700 $T/root/devices   # (1.6.1 devices: Bluetooth, Thunderbolt)
cat > $T/netstand <<'NS'
#!/bin/sh
T=@T@; n=${0##*/}; rq=; [ "$n $1" = "wifi.sh connect" ] || [ "$n $1" = "wifi.sh forget" ] && rq="|req=$(md5sum < "$3" | cut -c1-8) $(stat -c %a:%u "$3") ${3##*/}" && rm -f "$3"
[ "$n $1" = "hotspot start" ] && rq="|req=$(md5sum < "$2" | cut -c1-8) $(stat -c %a:%u "$2") ${2##*/}"
echo "$n $*$rq|path=$PATH|leak=$LEAKME" >> $T/calls
case "$n $1" in
"wifi.sh status") printf 'adapter=rtest0\nwpa_state=COMPLETED\nssid=Home Net\nnoise=1\nfreq=5180\n';;
"wifi.sh scan") printf 'aa:bb\t5180\t-50\t[WPA2]\tHome Net\n';; "wifi.sh radios") echo "radio aa:bb:cc:dd:ee:ff 5180 on";;
"wifi.sh bands") echo "2.4 on"; echo "5 on";; "wifi.sh disconnect") echo Disconnected;; "wifi.sh reconnect") echo Reconnecting;;
"wifi.sh radio") echo "Radio $3 turned $4";; "wifi.sh band") echo "This computer's $3 GHz radio turned $4";;
"wifi.sh adapter") [ "$2" = rtest0 ] && echo "Using $2" || { echo "No such adapter: $2"; exit 1; };; "wifi.sh country") echo "Country: $3";;
"wifi.sh connect") echo "Joining..."; echo "Connected";; "wifi.sh forget") echo "Forgotten";;
"firewall on"|"firewall off") [ -e $T/fw-fail ] && { echo "nft: no such table" >&2; exit 1; }; echo "turned $1";;
"firewall status") echo "on 7";; "firewall remote") ;;
"hotspot status") echo '{"on":false,"can":true}';; "hotspot stop") echo "The hotspot is off";; "hotspot start") echo "Sharing Phone Net (2.4 GHz)";;
"netplug state") echo '{"ok":true,"connected":true,"metered":false}';; "netplug set") [ "$2" = rtest0 ] || echo "no such adapter";;
"ip "*) [ -e $T/ip-fail ] && [ "$2" = add ] && { echo "RTNETLINK answers: File exists" >&2; exit 2; };;
"udhcpc "*) [ -e $T/nodhcp ] && exit 1;;
"bluetooth status") echo '{"ok":true,"adapter":"hci0","powered":true,"devices":[{"mac":"AA:BB:CC:DD:EE:01","name":"Pad","paired":true,"connected":false}]}';;
"bluetooth answer") [ -e $T/bt-noq ] && exit 1;; "bluetooth pair") echo "Pairing successful";; "bluetooth disconnect") echo "Successful disconnected";;
"bluetooth remove") echo "Device has been removed";; "bluetooth "*) ;;
"thunderbolt json") echo '{"ok":true,"present":true,"dma":false,"devices":[]}';; "thunderbolt allow") [ "$2" = 0-1 ] && echo approved || { echo "no such device"; exit 2; };;
"thunderbolt deny") echo "not approved";; "thunderbolt forget") echo forgotten;;
"wpa_cli "*) case "$5" in status) printf 'bssid=aa:bb:cc:dd:ee:ff\nssid=Home Net  \nwpa_state=COMPLETED\n';; signal_poll) printf 'RSSI=-55\nLINKSPEED=433\n';; esac;;
esac
exit 0
NS
sed -i "s|@T@|$T|" $T/netstand; chmod 755 $T/netstand
for s in wifi.sh firewall hotspot netplug udhcpc.sh bluetooth thunderbolt; do cp $T/netstand $T/lib/$s; done; for c in ip udhcpc wpa_cli; do cp $T/netstand $T/bin/$c; done
mkdir -p $T/sys/class/net/rtest0 $T/etc; printf 'RR_TEST_ETC=%s/etc\n' $T >> $T/rr.conf
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

# ---- 1.6.1 power: the power helper's checks
P=$T/root/power
o=$(env -i LEAKME=1 $P status </dev/null 2>&1); r=$?; $P set mode=quiet charge_limit=80 </dev/null >/dev/null 2>&1; r2=$?; $P apply </dev/null >/dev/null 2>&1; r3=$?; c=$(calls)
ok "P1. power: status, set KEY=VALUE..., apply: /usr/lib/rudolf/power with them as they are, nothing of the caller's environment" "$([ $r$r2$r3 = 000 ] && [ "$o" = '{"ok":true,"s":1}' ] && echo "$c" | grep -q '^power status|path=/usr/sbin:/usr/bin:/sbin:/bin|leak=$' && echo "$c" | grep -q '^power set mode=quiet charge_limit=80|' && echo "$c" | grep -q '^power apply|' && echo 1)" "$r$r2$r3 $o $c"
b=0; for a in "set" "set|a;b" "set|A=1" "set|a b" 'set|$(id)' "set|a=../x" "set|$(printf 'a%.0s' $(seq 101))" "status|x" "apply|now" "brightness-step|left" "brightness-step" "brightness-step|up|down" "backlight|1001" "backlight|-1" "backlight|1e3" "backlight|" "backlight" "dropcache|x" "priority|maybe" "priority" "wakealarm|0" "wakealarm|61" "wakealarm|x" "wakealarm" "sleep|now" "clock-set|-5" "clock-set|abc" "clock-set|1.5" "clock-set|1234567890123456" "clock-sync|pool.ntp.org" "reboot|a b" "off|a;b" "reboot|x|y" "frobnicate"; do
	IFS='|'; set -- $a; IFS=$OIFS; $P "$@" </dev/null >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: $a)"; }; done
$P set $(for i in $(seq 31); do printf 'a=%s ' $i; done) </dev/null >/dev/null 2>&1; [ $? = 2 ] || b=1; c=$(calls)
ok "P2. power: odd characters, too long, too many, a value over the screen's most, a time that is not one, extra arguments, unknown actions: refused, nothing run" "$([ $b = 0 ] && [ -z "$c" ] && [ "$(cat $T/sys/class/backlight/intel_backlight/brightness)" = 300 ] && echo 1)" "$b $c"
ok "...said in the health log (Security)" "$([ "$(logged 'power helper refused')" -ge 35 ] && echo 1)" "$(logged 'power helper refused')"
o=$($P brightness-step up </dev/null); $P backlight 500 </dev/null; r=$?; $P dropcache </dev/null; r2=$?; o2=$($P priority on </dev/null); $P wakealarm 5 </dev/null; r3=$?; $P sleep </dev/null; r4=$?; c=$(calls)
ok "P3. brightness (hotkey's step; the intel backlight before acpi_video0, set to the value), the cache, the desktop's priority, the wake alarm, sleep" "$([ "$o" = 40 ] && [ $r$r2$r3$r4 = 0000 ] && [ "$(cat $T/sys/class/backlight/intel_backlight/brightness)" = 500 ] && [ "$(cat $T/proc/sys/vm/drop_caches)" = 1 ] && [ "$o2" = 'the desktop first' ] && [ "$(cat $T/sys/class/rtc/rtc0/wakealarm)" = +5 ] && [ "$(cat $T/sys/power/state)" = mem ] && echo "$c" | grep -q '^hotkey brightness up|' && echo "$c" | grep -q '^desktop-priority on|' && echo 1)" "$o $r$r2$r3$r4 $o2 $c"
rm -f $T/sys/power/state; mkdir $T/sys/power/state; o=$($P sleep </dev/null); r=$?; rmdir $T/sys/power/state; : > $T/sys/power/state
ok "...sleep refused by the kernel: its reason said, status 1" "$([ $r = 1 ] && [ "$o" = 'Is a directory' ] && echo 1)" "$r $o"
$P clock-set 1700000000123 </dev/null; r=$?; c=$(calls)
ok "P4. the clock set (milliseconds), then the hardware clock" "$([ $r = 0 ] && [ "$(cat $T/run/clock)" = 1700000000.123000 ] && [ "$c" = 'hwclock -w -u' ] && echo 1)" "$r $(cat $T/run/clock) $c"
touch $T/ntp-down; $P clock-sync </dev/null; r=$?; c=$(calls); t1=$(cat $T/run/time-sync); touch $T/ntp-down2; $P clock-sync </dev/null; r2=$?; c2=$(calls); t2=$(cat $T/run/time-sync); rm -f $T/ntp-down $T/ntp-down2
ok "...from the internet: the second server when the first does not answer, then the hardware clock; none answering: fail, status 1" "$([ $r = 0 ] && [ $r2 = 1 ] && [ "$(echo "$c" | tr '\n' '|')" = 'ntpd -n -q -p pool.ntp.org|ntpd -n -q -p time.google.com|hwclock -w -u|' ] && echo "$t1" | grep -q '^ok [0-9]*$' && echo "$t2" | grep -q '^fail [0-9]*$' && [ "$(echo "$c2" | grep -c hwclock)" = 0 ] && echo 1)" "$r $r2 $c / $c2 $t1 $t2"
$P reboot t=12,x=3 </dev/null; r=$?; c=$(calls)
ok "P5. restart: the shutdown script with the desktop's times (its output in shutdown.out), then as before 1.4.5: Chromium stopped, every file system read-only, the restart" "$([ $r = 0 ] && [ "$(echo "$c" | tr '\n' '|')" = "shutdown reboot t=12,x=3|path=/usr/sbin:/usr/bin:/sbin:/bin|leak=|pkill -KILL -f /opt/chromium/chrome|umount -a -r|" ] && [ "$(cat $T/run/shutdown.out)" = 'shutdown steps' ] && [ "$(cat $T/run/power)" = reboot ] && [ "$(cat $T/run/rebooted)" = reboot ] && echo 1)" "$r $c"
$P off </dev/null; r=$?; c=$(calls)
ok "...shut down without the desktop's times: poweroff" "$([ $r = 0 ] && echo "$c" | grep -q '^shutdown poweroff |' && [ "$(cat $T/run/rebooted)" = poweroff ] && echo 1)" "$r $c"
rm -f $T/run/rebooted $T/run/power $T/run/shutdown.out $T/run/clock $T/run/time-sync

# ---- 1.6.1 network: the network helper's checks
NW=$T/root/network; PE='path=/usr/sbin:/usr/bin:/sbin:/bin|leak='
o=$(env -i LEAKME=1 $NW wifi status rtest0 </dev/null 2>&1); r=$?; $NW wifi scan "" </dev/null >/dev/null; r2=$?; $NW wifi country "" 00 </dev/null >/dev/null; $NW wifi country rtest0 DE </dev/null >/dev/null
$NW wifi radio "" aa:BB:cc:dd:ee:ff off </dev/null >/dev/null; $NW wifi band rtest0 2.4 on </dev/null >/dev/null; $NW wifi adapter rtest0 </dev/null >/dev/null; o2=$($NW wifi adapter wlan9 </dev/null); r3=$?; c=$(calls)
ok "N1. network: Wi-Fi's actions are wifi.sh's with their arguments as they are (an empty adapter kept: wifi.sh picks one), its output and status, nothing of the caller's environment" "$([ $r$r2$r3 = 001 ] && [ "$(echo "$o" | head -1)" = adapter=rtest0 ] && [ "$o2" = 'No such adapter: wlan9' ] && [ "$(echo "$c" | tr '\n' '#')" = "wifi.sh status rtest0|$PE#wifi.sh scan |$PE#wifi.sh country  00|$PE#wifi.sh country rtest0 DE|$PE#wifi.sh radio  aa:BB:cc:dd:ee:ff off|$PE#wifi.sh band rtest0 2.4 on|$PE#wifi.sh adapter rtest0|$PE#wifi.sh adapter wlan9|$PE#" ] && echo 1)" "$r$r2$r3 $o2 $c"
b=0; for a in "wifi" "wifi|status" "wifi|status|a;b" "wifi|status|a b" 'wifi|status|$(id)' "wifi|status|../x" "wifi|status|aaaaaaaaaaaaaaaaa" "wifi|status|rtest0|x" "wifi|frob|rtest0" "wifi|country|rtest0|de" "wifi|country|rtest0|DEU" "wifi|country|rtest0" "wifi|adapter|" "wifi|radio|rtest0|aa:bb:cc:dd:ee|off" "wifi|radio|rtest0|aa:bb:cc:dd:ee:fg|off" "wifi|radio|rtest0|aa:bb:cc:dd:ee:ff|maybe" "wifi|radio|rtest0|aa:bb:cc:dd:ee:ff" "wifi|band|rtest0|3|on" "wifi|band|rtest0|5" "wifi|connect|rtest0|x" \
	"wifi-link" "wifi-link|" "wifi-link|a/b" "dhcp|nosuch0" "dhcp|-rtest0" "dhcp|rtest0|x" "dhcp|" "dhcp|.." "static|rtest0|10.0.0.5/24|10.0.0.1" "static|rtest0||10.0.0.1|" "static|rtest0|10.0.0.5 24||" "static|rtest0|10.0.0.5/24|10.0.0.1/8|" "static|rtest0|10.0.0.5/24||1.1.1.1;x" "static|nosuch0|10.0.0.5/24||" \
	"firewall" "firewall|maybe" "firewall|on|now" "firewall|remote" "firewall|remote|maybe" "hotspot" "hotspot|frob" "hotspot|status|x" "metered-state|x" "metered|rtest0|maybe" "metered|a b|on" "metered|aaaaaaaaaaaaaaaa|on" "metered|rtest0" "frobnicate"; do
	IFS='|'; set -- $a; IFS=$OIFS; $NW "$@" </dev/null >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: $a)"; }; done
$NW static rtest0 "$(printf '1%.0s' $(seq 64))" "" "" </dev/null >/dev/null 2>&1; [ $? = 2 ] || b=1; c=$(calls)
ok "N2. network: an adapter's name that is not one (odd characters, too long, one the kernel does not have for ip and udhcpc, an option), a country, radio, band or address that is not one, extra or missing arguments, unknown actions: refused, nothing run" "$([ $b = 0 ] && [ -z "$c" ] && echo 1)" "$b $c"
ok "...said in the health log (Security)" "$([ "$(logged 'network helper refused')" -ge 45 ] && echo 1)" "$(logged 'network helper refused')"
o=$(printf 'Home Net\nsecret pw\npsk\n' | $NW wifi connect rtest0 2>&1); r=$?; c=$(calls); k=$(printf 'Home Net\nsecret pw\npsk\n' | md5sum | cut -c1-8)
printf 'Home Net\n' | $NW wifi forget "" >/dev/null 2>&1; r2=$?; c2=$(calls); k2=$(printf 'Home Net\n' | md5sum | cut -c1-8)
$NW wifi connect rtest0 </dev/null >/dev/null 2>&1; r3=$?; head -c 601 /dev/zero | tr '\0' x | $NW wifi connect rtest0 >/dev/null 2>&1; r4=$?; c3=$(calls)
ok "N3. Wi-Fi's join and forget: the request read from standard input only, into a file of root's (0600) that wifi.sh reads and removes; an empty one or one over 600 bytes refused" "$([ $r$r2$r3$r4 = 0022 ] && [ "$c" = "wifi.sh connect rtest0 $T/run/wifi.req|req=$k 600:0 wifi.req|$PE" ] && [ "$c2" = "wifi.sh forget  $T/run/wifi-forget.req|req=$k2 600:0 wifi-forget.req|$PE" ] && [ -z "$c3" ] && [ ! -e $T/run/wifi.req ] && echo "$o" | grep -q Connected && echo 1)" "$r$r2$r3$r4 $c / $c2 / $c3"
o=$($NW wifi-link rtest0 </dev/null); c=$(calls)
ok "N4. the connected network's name and signal: wpa_cli's status and signal_poll on wpa_supplicant's sockets, two lines" "$([ "$o" = "$(printf 'Home Net  \n-55')" ] && [ "$(echo "$c" | tr '\n' '#')" = "wpa_cli -p /run/rudolf/wpa -i rtest0 status|$PE#wpa_cli -p /run/rudolf/wpa -i rtest0 signal_poll|$PE#" ] && echo 1)" "$o $c"
$NW dhcp rtest0 </dev/null; r=$?; c=$(calls); touch $T/nodhcp; $NW dhcp rtest0 </dev/null; r2=$?; rm -f $T/nodhcp; calls >/dev/null
ok "N5. an address by DHCP: the adapter up, then udhcpc with the script, its status (no answer: 1)" "$([ $r$r2 = 01 ] && [ "$(echo "$c" | tr '\n' '#')" = "ip link set rtest0 up|$PE#udhcpc -i rtest0 -n -q -t 5 -O 43 -s $T/lib/udhcpc.sh|$PE#" ] && echo 1)" "$r$r2 $c"
$NW static rtest0 10.0.0.5/24 10.0.0.1 9.9.9.9 </dev/null; r=$?; c=$(calls); d=$(cat $T/etc/resolv.conf); rm -f $T/etc/resolv.conf
$NW static rtest0 10.0.0.6/24 "" "" </dev/null; r2=$?; c2=$(calls); touch $T/ip-fail; $NW static rtest0 10.0.0.7/24 10.0.0.1 8.8.8.8 </dev/null 2>/dev/null; r3=$?; c3=$(calls); rm -f $T/ip-fail
ok "...a fixed address: flush, add, up, the default route, the name server into resolv.conf; without a gateway or name server neither; an address refused by ip: 1, no route, the name server still written (as before)" "$([ $r$r2$r3 = 001 ] && [ "$(echo "$c" | tr '\n' '#')" = "ip addr flush dev rtest0|$PE#ip addr add 10.0.0.5/24 dev rtest0|$PE#ip link set rtest0 up|$PE#ip route replace default via 10.0.0.1|$PE#" ] && [ "$d" = 'nameserver 9.9.9.9' ] && [ "$(echo "$c2" | wc -l)" = 3 ] && [ "$(echo "$c3" | wc -l)" = 2 ] && [ "$(cat $T/etc/resolv.conf)" = 'nameserver 8.8.8.8' ] && echo 1)" "$r$r2$r3 $c / $c2 / $c3"
rm -f $T/etc/resolv.conf
o=$($NW firewall status </dev/null); $NW firewall remote on </dev/null; touch $T/fw-fail; o2=$($NW firewall on </dev/null 2>&1); r=$?; rm -f $T/fw-fail; o3=$($NW hotspot status </dev/null); o4=$(printf 'Phone Net\npassword1\n2.4\n' | $NW hotspot start 2>&1); o5=$($NW metered-state </dev/null); o6=$($NW metered wlan9 on </dev/null); c=$(calls)
ok "N6. the firewall (its count, remote access's port, a refusal's words and status), the hotspot (the request on standard input into a file of root's), metered connections: the scripts' own answers" "$([ "$o" = 'on 7' ] && [ $r = 1 ] && [ "$o2" = 'nft: no such table' ] && [ "$o3" = '{"on":false,"can":true}' ] && [ "$o4" = 'Sharing Phone Net (2.4 GHz)' ] && echo "$o5" | grep -q '"metered":false' && [ "$o6" = 'no such adapter' ] && echo "$c" | grep -q "^firewall remote on|$PE" && echo "$c" | grep -q "^hotspot start $T/run/hotspot.req|req=$(printf 'Phone Net\npassword1\n2.4\n' | md5sum | cut -c1-8) 600:0 hotspot.req|" && echo "$c" | grep -q "^netplug set wlan9 on|" && echo 1)" "$o $o2 $o3 $o4 $o5 $o6 $c"
rm -f $T/run/hotspot.req

# ---- 1.6.1 devices: the devices helper's checks (Bluetooth, Thunderbolt)
DV=$T/root/devices
o=$(env -i LEAKME=1 $DV bt status </dev/null); $DV bt on </dev/null; $DV bt scan 15 </dev/null; o2=$($DV bt pair AA:BB:CC:DD:EE:01 </dev/null); touch $T/bt-noq; $DV bt answer yes </dev/null; r=$?; rm -f $T/bt-noq
o3=$($DV tb json </dev/null); o4=$($DV tb allow 0-1 </dev/null); o5=$($DV tb allow 9-9 </dev/null); r2=$?; $DV tb forget 00ab-cd12 </dev/null >/dev/null; c=$(calls)
ok "D1. devices: Bluetooth's and Thunderbolt's actions are the scripts' with their arguments as they are, their answers and status, nothing of the caller's environment" "$([ $r$r2 = 12 ] && echo "$o" | grep -q '"mac":"AA:BB:CC:DD:EE:01"' && [ "$o2" = 'Pairing successful' ] && echo "$o3" | grep -q '"present":true' && [ "$o4" = approved ] && [ "$o5" = 'no such device' ] && [ "$(echo "$c" | tr '\n' '#')" = "bluetooth status|$PE#bluetooth on|$PE#bluetooth scan 15|$PE#bluetooth pair AA:BB:CC:DD:EE:01|$PE#bluetooth answer yes|$PE#thunderbolt json|$PE#thunderbolt allow 0-1|$PE#thunderbolt allow 9-9|$PE#thunderbolt forget 00ab-cd12|$PE#" ] && echo 1)" "$r$r2 $o2 $o4 $o5 $c"
b=0; for a in "bt" "bt|status|x" "bt|on|now" "bt|scan" "bt|scan|2" "bt|scan|61" "bt|scan|1e1" "bt|scan|-5" "bt|answer|maybe" "bt|answer" "bt|pair|aa:bb:cc:dd:ee:01" "bt|pair|AA:BB:CC:DD:EE" "bt|pair|AA:BB:CC:DD:EE:0G" "bt|connect|AA-BB-CC-DD-EE-01" 'bt|remove|$(id)' "bt|pair" "bt|pair|AA:BB:CC:DD:EE:01|x" "bt|frob" "bt|start" "bt|agent-run" \
	"tb" "tb|json|x" "tb|allow|01" "tb|allow|0/1" "tb|allow|../1" "tb|deny|0-1;x" "tb|forget|zz" "tb|forget|$(printf 'a%.0s' $(seq 65))" "tb|forget" "tb|plug" "tb|boot" "frob|status"; do
	IFS='|'; set -- $a; IFS=$OIFS; $DV "$@" </dev/null >/dev/null 2>&1; [ $? = 2 ] || { b=1; echo "  (allowed: $a)"; }; done; c=$(calls)
ok "D2. devices: a device's address or ID that is not one, a search that is not 3 to 60 seconds, an answer that is not yes or no, the scripts' own start-up actions, extra arguments, unknown actions: refused, nothing run" "$([ $b = 0 ] && [ -z "$c" ] && echo 1)" "$b $c"
ok "...said in the health log (Security)" "$([ "$(logged 'devices helper refused')" -ge 30 ] && echo 1)" "$(logged 'devices helper refused')"

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
# (1.6.1 power: rudolfd built so that, should it ever do the power work itself here, it can neither restart this
# computer nor set its clock; the commands it would run (pkill, umount, hwclock, ntpd) are the stand-ins in $T/bin)
printf '#define _GNU_SOURCE\n#include <sys/reboot.h>\n#include <sys/time.h>\n#include <stdio.h>\n#define reboot(x) (fprintf(stderr, "rudolfd: (test) no reboot\\n"), 0)\n#define settimeofday(a, b) (fprintf(stderr, "rudolfd: (test) no clock\\n"), 0)\n' > $T/noreboot.h
gcc -O2 -w -include $T/noreboot.h -DRD_ROOT_DIR="\"$T/root\"" -DRU_BIN="\"$T/ru\"" -o $T/rudolfd $SYS/rudolfd.c -lz || { echo "FAIL build rudolfd"; exit 1; }
mkdir -p $T/none; gcc -O2 -w -include $T/noreboot.h -DRD_ROOT_DIR="\"$T/none\"" -DRU_BIN="\"$T/ru\"" -o $T/rudolfd-inproc $SYS/rudolfd.c -lz || { echo "FAIL build rudolfd"; exit 1; }
# (rudolfd looks for the power settings, the desktop's priority and the shutdown script in /usr/lib/rudolf before it
# asks: stand-ins there, in this namespace; the folder made only if this computer has none, and removed again)
ULR_MADE=; [ -d /usr/lib/rudolf ] || { mkdir -p /usr/lib/rudolf && ULR_MADE=1; }
mkdir -p $T/ulr; for s in power desktop-priority shutdown wifi.sh firewall hotspot netplug udhcpc.sh bluetooth thunderbolt; do cp $T/lib/$s $T/ulr/$s; done; mount --bind $T/ulr /usr/lib/rudolf
cp $T/drives-home.real $T/root/drives-home; chmod 700 $T/root/drives-home
# (the stand-in home for the helper is the real /home/rudolf here, a tmpfs, as rudolfd's API checks the image's place)
sed -i "s|^RR_TEST_HOME=.*|RR_TEST_HOME=/home/rudolf|; s|^RR_TEST_PERSIST=.*|RR_TEST_PERSIST=/persist|; s|^RR_TEST_AUMAP=.*|RR_TEST_AUMAP=/persist/etc/rudolf/account-users|; s|^RR_TEST_ETC=.*|RR_TEST_ETC=/etc|" $T/rr.conf
# (1.6.1 network: the name server's file this namespace's own, a plain file, whatever this computer's /etc/resolv.conf is)
rm -f /etc/resolv.conf; echo 'nameserver 192.0.2.53' > /etc/resolv.conf
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
	# (1.6.1 power)
	api GET power/status; api POST 'power/set?mode=quiet&charge_limit=80'; api POST 'desktop/priority?on=1'; api POST memory/dropcache | sed 's/[0-9][0-9]*/N/g'
	api POST 'time?epoch=1700000000123'; cat $T/run/clock 2>&1; echo; api POST time/sync; cat $T/run/time-sync 2>&1 | sed 's/[0-9][0-9]*/N/'; api POST 'brightness/step?d=up'
	api POST 'power?action=reboot&t=a=1,b=2'; w8 '[ -e $T/run/rebooted ]' 40; cat $T/run/rebooted 2>&1; echo; cat $T/run/shutdown.out 2>&1; rm -f $T/run/rebooted $T/run/clock $T/run/time-sync $T/run/shutdown.out
}
# (1.6.1 network) every network request through the network helper, and as rudolfd did it itself; what the stand-ins
# were asked compared with the helper's own places (its $T/lib and $T/run) and PATH taken out
nprobe() {
	api GET 'wifi/status?if=rtest0'; api GET wifi/scan; api POST 'wifi/country?cc=DE&if=rtest0'; api POST 'wifi/country?cc=00'; api POST 'wifi/radio?bssid=aa:bb:cc:dd:ee:ff&on=off'
	api POST 'wifi/band?band=5&on=off'; api POST 'wifi/adapter?if=rtest0'; api POST 'wifi/adapter?if=wlan9'; api POST wifi/reconnect; api POST wifi/bands
	hpost 'wifi/connect?if=rtest0' "$(printf 'Home Net\nsecret pw\npsk')"; jobwait; ls /run/rudolf/wifi.req $T/run/wifi.req 2>&1 | grep -c 'No such'; hpost wifi/forget 'Home Net'
	api POST 'service?name=wifi'; api POST 'net/dhcp?if=rtest0'; touch $T/nodhcp; api POST 'net/dhcp?if=rtest0'; rm -f $T/nodhcp
	api POST 'net/static?if=rtest0&addr=10.0.0.5%2F24&gw=10.0.0.1&dns=9.9.9.9'; cat /etc/resolv.conf; touch $T/ip-fail; api POST 'net/static?if=rtest0&addr=10.0.0.6%2F24&dns=8.8.4.4'; rm -f $T/ip-fail; cat /etc/resolv.conf
	api GET firewall; api POST 'firewall?on=0'; touch $T/fw-fail; api POST 'firewall?on=1'; rm -f $T/fw-fail
	api GET net/hotspot; hpost net/hotspot "$(printf 'Phone Net\npassword1\n2.4')"; api POST 'net/hotspot?on=0'; rm -f $T/run/hotspot.req /run/rudolf/hotspot.req
	api GET net/metered; api POST 'net/metered?if=rtest0&v=on'; api POST 'net/metered?if=wlan9&v=off'
	# (1.6.1 devices: Bluetooth, Thunderbolt)
	api GET bt; api POST 'bt/power?on=0'; api POST bt/visible; api POST 'bt/scan?s=5'; api POST 'bt/answer?ok=1'; touch $T/bt-noq; api POST 'bt/answer?ok=0'; rm -f $T/bt-noq
	api POST 'bt/pair?mac=AA:BB:CC:DD:EE:01'; api POST 'bt/disconnect?mac=AA:BB:CC:DD:EE:01'; api POST 'bt/remove?mac=AA:BB:CC:DD:EE:01'; api POST 'bt/pair?mac=aa:bb:cc:dd:ee:01'
	api GET tb; api POST 'tb/allow?id=0-1'; api POST 'tb/deny?id=0-1'; api POST 'tb/forget?uid=00ab-cd12'; api POST 'tb/allow?id=9-9'
}
nnorm() { grep -v '^ntpd \|^hwclock \|^firewall status|' | sed "s|$T/run/|/run/rudolf/|g; s|$T/lib/|/usr/lib/rudolf/|g; s/|path=[^|]*//"; }
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
start() { : > $T/rd.log; PATH=$T/bin:$PATH $1 > $T/rd.log 2>&1 & RDP=$!; w8 'curl -s http://127.0.0.1:8080/favicon.png -o /dev/null' 60
	TOK=$(curl -s http://127.0.0.1:8080/ | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1); }
stop() { kill $RDP 2>/dev/null; wait $RDP 2>/dev/null; RDP=; w8 '! curl -s -o /dev/null http://127.0.0.1:8080/favicon.png' 40; }
# (the clock's sync at start runs when this computer has a network: when is not the same each time, so it is left out)
nosync() { grep -v '^ntpd \|^hwclock \|^firewall status|'; }
hreset; start $T/rudolfd; A=$(probe); CA=$(calls | nosync); HA=$(hprobe); calls >/dev/null; NA=$(nprobe); NCA=$(calls | nnorm); stop
grep -q 'run by rudolfd itself' $T/rd.log; d1=$?
hreset; start $T/rudolfd-inproc; HC=$(hprobe); calls >/dev/null; NC=$(nprobe); NCC=$(calls | nnorm); stop; timeout 1 tail -f /dev/null; calls >/dev/null   # (what it ran itself is not the helpers')
hreset; start "$T/rudolf-rootd $T/rudolfd"; B=$(probe); CB=$(calls | nosync); HB=$(hprobe); calls >/dev/null; NB=$(nprobe); NCB=$(calls | nnorm)
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
ok "24. power through the helper: the settings, the desktop's priority, the cache, the clock (set, and from the internet), brightness's step, restart with the desktop's times: rudolfd's answers as before, the same with and without rudolf-rootd" "$(echo "$A" | grep -q '^{"ok":true,"s":1}$' && echo "$A" | grep -q '"set":"set mode=quiet charge_limit=80"' && echo "$A" | grep -q '"text":"the desktop first' && echo "$A" | grep -q '"freed":N' && echo "$A" | grep -q '^1700000000.123000$' && echo "$A" | grep -q 'clock set from the internet' && echo "$A" | grep -q '^ok N$' && echo "$A" | grep -q '"brightness":40' && echo "$A" | grep -q '"message":"reboot"' && echo "$A" | grep -q '^reboot$' && echo "$CA" | grep -q '^shutdown reboot a=1,b=2|' && echo "$CA" | grep -q '^umount -a -r$' && ! grep -q '(test) no' $T/rd.log && echo 1)" "$(echo "$A" | tail -14) $(grep '(test)' $T/rd.log)"
ok "25. the network through the helper (Wi-Fi's actions, a join (a job) and a forget with the request on standard input, the service's reconnect, DHCP, a fixed address and the name server, the firewall, the hotspot, metered connections): the same answers, byte for byte, with and without rudolf-rootd, and as rudolfd doing it itself" "$([ -n "$NA" ] && [ "$NA" = "$NB" ] && [ "$NA" = "$NC" ] && echo 1)" "$(printf '%s\n---\n%s\n---\n%s' "$NA" "$NB" "$NC" | head -90)"
ok "...the scripts and commands asked the same things (the requests' files root's 0600, removed by wifi.sh as before)" "$([ -n "$NCA" ] && [ "$NCA" = "$NCB" ] && [ "$NCA" = "$NCC" ] && echo "$NCA" | grep -q '^wifi.sh connect rtest0 /run/rudolf/wifi.req|req=[0-9a-f]* 600:0 wifi.req|leak=$' && echo "$NCA" | grep -q '^udhcpc -i rtest0 -n -q -t 5 -O 43 -s /usr/lib/rudolf/udhcpc.sh|' && echo 1)" "$(printf '%s\n---\n%s\n---\n%s' "$NCA" "$NCB" "$NCC" | head -90)"
ok "...and they are 1.6.0's answers: the status's raw text, the radio and band turned off, the adapter refused, connecting then the job ended 0, forgotten, connected, no DHCP answer 500, applied, the address refused 500 with the name server written, the firewall's count, its refusal's words, sharing, the hotspot off, metered refused" "$(echo "$NA" | grep -q '"raw":"adapter=rtest0' && echo "$NA" | grep -q 'Radio aa:bb:cc:dd:ee:ff turned off' && echo "$NA" | grep -q "5 GHz radio turned off" && echo "$NA" | grep -q 'No such adapter: wlan9' && echo "$NA" | grep -q '"message":"connecting"' && echo "$NA" | grep -q '"message":"Forgotten"' && echo "$NA" | grep -q '"message":"connected"' && echo "$NA" | grep -q 'no DHCP answer' && echo "$NA" | grep -q '"message":"applied"' && echo "$NA" | grep -q 'could not apply the address' && echo "$NA" | grep -q '^nameserver 8.8.4.4$' && echo "$NA" | grep -q '"blocked":7' && echo "$NA" | grep -q 'nft: no such table' && echo "$NA" | grep -q 'Sharing Phone Net' && echo "$NA" | grep -q 'The hotspot is off' && echo "$NA" | grep -q 'no such adapter' && ! grep -q 'network helper did not start' $T/rd.log && echo 1)" "$NA"
ok "26. Bluetooth and Thunderbolt through the devices helper (in check 25's comparison): 1.6.0's answers: the status, answered, already answered 409, paired, disconnected, removed (the device still there: 409 as before), a lower-case address 400, the devices, approved, not approved, forgotten, no such device 500" "$(echo "$NA" | grep -q '"adapter":"hci0"' && echo "$NA" | grep -q '"message":"answered"' && echo "$NA" | grep -q 'already answered' && echo "$NA" | grep -q '"ok":true,"message":"Pairing successful"' && echo "$NA" | grep -q '"ok":true,"message":"Successful disconnected"' && echo "$NA" | grep -q '"ok":false,"message":"Device has been removed"' && echo "$NA" | grep -q 'bad device' && echo "$NA" | grep -q '"present":true' && echo "$NA" | grep -q '"message":"approved"' && echo "$NA" | grep -q '"message":"not approved"' && echo "$NA" | grep -q '"message":"forgotten"' && echo "$NA" | grep -q '"error":"no such device"' && echo "$NCA" | grep -q '^bluetooth scan 5|' && echo "$NCA" | grep -q '^thunderbolt forget 00ab-cd12|' && ! grep -q 'devices helper did not start' $T/rd.log && echo 1)" "$(echo "$NA" | tail -16)"
ok "21. rudolfd keeps rudolf-rootd's descriptor close-on-exec (nothing it starts has it)" "$(echo "$FDS" | python3 -c 'import sys,re; t=sys.stdin.read(); m=re.search(r"flags:\s*([0-7]+)", t); print(1 if m and int(m.group(1), 8) & 0o2000000 and "socket:" in t else 0)')" "$FDS"
echo "$((N-F))/$N passed"; [ $F = 0 ]
