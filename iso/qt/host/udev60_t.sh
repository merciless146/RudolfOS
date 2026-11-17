#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md Part 9): udev instead of mdev, the parts that need no udev here: rudolf-init's start of it
# (its own lines, with stand-ins for udevd, udevadm and mdev): udev started, the devices announced and waited for;
# mdev when udevd does not start, when udev is not in the build, or with rudolf.udev=0, said in /run/rudolf/devices;
# udev-run giving the scripts mdev's environment; every mdev.conf line that ran a script has a udev rule running the
# same; the modem service with and without udev; the self-test's devices line. With udevadm (CI: the udev workflow),
# 90-rudolf.rules is checked by udevadm verify, and by udevadm test once it is in /usr/lib/udev/rules.d.   sh udev60_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys/root; T=$(mktemp -d /tmp/udev60.XXXXXX); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
RULES=$SYS/usr/lib/udev/rules.d/90-rudolf.rules
# rudolf-init's lines, from "UDEVD=" to the end of its fallback
awk '/^UDEVD=\/usr\/lib\/systemd\/systemd-udevd$/{f=1} f; f&&/^fi$/{exit}' $SYS/sbin/rudolf-init > $T/block
grep -q 'udevadm settle' $T/block || { echo "FAIL rudolf-init's lines not found"; exit 1; }
mkdir -p $T/bin $T/run/rudolf
for c in mdev udevadm pkill; do printf '#!/bin/sh\necho "%s $*" >> %s/calls\n[ "%s" = udevadm ] && [ "$1" = control ] && [ -e %s/noping ] && exit 1\nexit 0\n' $c $T $c $T > $T/bin/$c; chmod 755 $T/bin/$c; done
printf '#!/bin/sh\necho "udevd $*" >> %s/calls\n[ -e %s/udevd-fails ] && exit 1\nexit 0\n' $T $T > $T/udevd; chmod 755 $T/udevd
run() { rm -f $T/calls; echo "$1" > $T/cmdline; sed -e "s#/run/rudolf#$T/run/rudolf#g" -e "s#^UDEVD=.*#UDEVD=$UD#" $T/block > $T/b2
	( PATH=$T/bin:$PATH; RCMD=$T/cmdline; . $T/b2 ); cat $T/calls 2>/dev/null; }
UD=$T/udevd; o=$(run quiet)
ok "1. udevd started, the devices announced again (subsystems, then devices) and waited for (15 s at most); no mdev" "$(echo "$o" | grep -q '^udevd --daemon --resolve-names=early' && echo "$o" | grep -q '^udevadm trigger --type=subsystems --action=add' && echo "$o" | grep -q '^udevadm trigger --type=devices --action=add' && echo "$o" | grep -q '^udevadm settle --timeout=15' && ! echo "$o" | grep -q '^mdev' && [ "$(cat $T/run/rudolf/devices)" = udev ] && echo 1)" "$o"
touch $T/udevd-fails; o=$(run quiet); rm $T/udevd-fails
ok "2. udevd does not start: mdev as before (mdev -s, mdev -d), said" "$(echo "$o" | grep -q '^mdev -s' && echo "$o" | grep -q '^mdev -d' && ! echo "$o" | grep -q trigger && grep -q '^mdev (udevd did not start' $T/run/rudolf/devices && echo 1)" "$o $(cat $T/run/rudolf/devices)"
touch $T/noping; o=$(run quiet); rm $T/noping
ok "...nor answers: stopped, mdev" "$(echo "$o" | grep -q '^pkill -x systemd-udevd' && echo "$o" | grep -q '^mdev -s' && echo 1)" "$o"
UD=$T/none; o=$(run quiet); UD=$T/udevd
ok "...udev not in the build: mdev, said" "$(echo "$o" | grep -q '^mdev -s' && grep -q 'not part of this build' $T/run/rudolf/devices && echo 1)" "$o"
o=$(run "quiet rudolf.udev=0")
ok "...rudolf.udev=0 at start-up: mdev, said, udevd never started" "$(echo "$o" | grep -q '^mdev -s' && ! echo "$o" | grep -q '^udevd' && grep -q 'rudolf.udev=0' $T/run/rudolf/devices && echo 1)" "$o"
# udev-run
printf '#!/bin/sh\necho "MDEV=$MDEV ACTION=$ACTION SUBSYSTEM=$SUBSYSTEM ARGS=$* SID=$(ps -o sid= -p $$ | tr -d " ")" > %s/seen\n' $T > $T/script; chmod 755 $T/script
env -i PATH=$PATH DEVNAME=/dev/snd/controlC1 ACTION=add SUBSYSTEM=sound UDEV_RUN_LOG=$T/ur.log sh $SYS/usr/lib/rudolf/udev-run $T/script hotplug; timeout 1 tail -f /dev/null
ok "3. udev-run: mdev's MDEV (the name under /dev), udev's ACTION and SUBSYSTEM, the script's arguments, in a session of its own" "$(grep -q '^MDEV=snd/controlC1 ACTION=add SUBSYSTEM=sound ARGS=hotplug SID=' $T/seen && [ "$(sed 's/.*SID=//' $T/seen)" != "$(ps -o sid= -p $$ | tr -d ' ')" ] && echo 1)" "$(cat $T/seen 2>&1)"
env -i PATH=$PATH INTERFACE=usb0 ACTION=add SUBSYSTEM=net UDEV_RUN_LOG=$T/ur.log sh $SYS/usr/lib/rudolf/udev-run $T/script; timeout 1 tail -f /dev/null
ok "...a network adapter: MDEV is its interface (as mdev gave netplug)" "$(grep -q '^MDEV=usb0 ACTION=add SUBSYSTEM=net' $T/seen && echo 1)" "$(cat $T/seen)"
env -i PATH=$PATH DEVNAME=/dev/x sh $SYS/usr/lib/rudolf/udev-run /nonexistent; r=$?
ok "...a program that is not there: nothing, and udev is not told of a failure" "$([ $r = 0 ] && echo 1)"
# every script mdev ran, udev runs too
miss=; for s in $(grep -o '\*/usr/[^ ]*' $SYS/etc/mdev.conf | sed 's/^\*//' | sort -u); do grep -q "udev-run $s" $RULES || miss="$miss $s"; done
ok "4. every script /etc/mdev.conf runs has a udev rule running it" "$([ -z "$miss" ] && echo 1)" "missing:$miss"
bad=; for s in $(grep -o 'udev-run [^ "]*' $RULES | sed 's/^udev-run //' | sort -u); do [ -x $SYS$s ] || [ "$s" = /usr/bin/rudolf-debian ] || bad="$bad $s"; done
ok "...each of them there and runnable (rudolf-debian is built)" "$([ -z "$bad" ] && echo 1)" "$bad"
ok "...owners as numbers (no group name that might be missing), modes as mdev gave them" "$(! grep -E 'GROUP="[a-z]' $RULES >/dev/null && grep -q 'SUBSYSTEM=="input", GROUP="0", MODE="0640"' $RULES && grep -q 'KERNEL=="hidraw\[0-9\]\*", GROUP="1000", MODE="0660"' $RULES && grep -q 'SUBSYSTEM=="drm", KERNEL=="card\[0-9\]\*|renderD\[0-9\]\*", GROUP="44", MODE="0666"' $RULES && echo 1)"
# drivers for devices that come later (2026-11-17, the Vivobook Go on 2026111713: the AMD audio processor's driver adds
# its microphones' devices after start-up's pass, and udev's own "kmod load" cannot read busybox's module list): udev
# loads by modalias as mdev's line did, with the same modprobe, on add only
ML=$(grep 'ENV{MODALIAS}' $RULES)
ok "4b. drivers by modalias: a udev rule runs mdev's modprobe -b for every device added with a MODALIAS" "$(grep -q '^\$MODALIAS=.* @modprobe -b "\$MODALIAS"' $SYS/etc/mdev.conf && echo "$ML" | grep -q 'ACTION=="add", ENV{MODALIAS}=="?\*", RUN+="/sbin/modprobe -b \$env{MODALIAS}"' && echo 1)" "$ML"
# ...the line as udev runs it, with modprobe a stand-in fed the Vivobook's three devices (their modaliases as read there)
printf '#!/bin/sh\necho "modprobe $*" >> %s/calls\n' $T > $T/bin/modprobe; chmod 755 $T/bin/modprobe; rm -f $T/calls
cmdl=$(echo "$ML" | sed 's/.*RUN+="\([^"]*\)".*/\1/; s#^/sbin/modprobe#'$T'/bin/modprobe#')
for a in platform:acp_yc_mach platform:acp_yc_pdm_dma platform:dmic-codec; do MODALIAS=$a sh -c "$(echo "$cmdl" | sed 's/\$env{MODALIAS}/"$MODALIAS"/')"; done
ok "...the Vivobook's microphone devices (acp_yc_mach, acp_yc_pdm_dma, dmic-codec) each get modprobe -b" "$([ "$(cat $T/calls)" = "$(printf 'modprobe -b platform:acp_yc_mach\nmodprobe -b platform:acp_yc_pdm_dma\nmodprobe -b platform:dmic-codec')" ] && echo 1)" "$(cat $T/calls)"
ok ".../sbin/modprobe is busybox's in the base (it reads modules.dep.bb's aliases)" "$(grep -q 'modules.dep.bb\|busybox depmod' $HERE/../../mkrootfs.sh && echo 1)"
# the modem service
mkdir -p $T/mm; printf '#!/bin/sh\necho "MM $*" >> %s/mm/calls\nsleep 5\n' $T > $T/mm/ModemManager; printf '#!/bin/sh\nexit 0\n' > $T/mm/mmcli; chmod 755 $T/mm/*
grep -q 'with_udev()' $SYS/usr/lib/rudolf/modem && grep -q 'if with_udev; then NOU=; else NOU=--test-no-udev' $SYS/usr/lib/rudolf/modem
ok "5. the modem service: with udev, ModemManager finds its ports itself (no --test-no-udev, no reports); with mdev as in 1.5.9" "$([ $? = 0 ] && echo 1)"
# the self-test's line
mkdir -p $T/st/run $T/st/udev; echo udev > $T/st/run/devices
o=$(grep -A8 '^dv=\$(rd1 "\$RUN/devices")' $SYS/usr/lib/rudolf/selftest)
ok "6. the self-test: a devices line (udev running and its database filled; mdev the fallback fails; rudolf.udev=0 skipped)" "$(echo "$o" | grep -q 'pass devices "udev:' && echo "$o" | grep -q 'fail devices "the devices are set up by mdev' && echo "$o" | grep -q 'skip devices "mdev, as asked' && echo 1)" "$o"
# with udev (CI)
if command -v udevadm >/dev/null; then
	o=$(udevadm verify --no-style $RULES 2>&1); r=$?
	ok "7. (udevadm) udevadm verify: 90-rudolf.rules reads cleanly" "$([ $r = 0 ] && echo 1)" "$o"
	if [ -e /sys/class/net/lo ]; then o=$(udevadm test --action=add /sys/class/net/lo 2>&1)
		ok "...udevadm test on a network interface: netplug run through udev-run" "$(echo "$o" | grep -q 'udev-run /usr/lib/rudolf/netplug' && echo 1)" "$(echo "$o" | tail -20)"; fi
	ev=$(ls -d /sys/class/input/event* 2>/dev/null | head -1)
	if [ -n "$ev" ]; then o=$(udevadm test --action=add $ev 2>&1)
		ok "...on an input device: root's, 0640, joyperm run" "$(echo "$o" | grep -q 'udev-run /usr/lib/rudolf/joyperm' && echo 1)" "$(echo "$o" | tail -20)"; fi
	md=$(ls -d /sys/bus/platform/devices/* /sys/bus/pci/devices/* 2>/dev/null | while read -r d; do [ -s "$d/modalias" ] && { echo "$d"; break; }; done)
	if [ -n "$md" ]; then o=$(udevadm test --action=add "$md" 2>&1)
		ok "...on a device with a modalias: modprobe -b with it" "$(echo "$o" | grep -q "/sbin/modprobe -b $(cat $md/modalias)" && echo 1)" "$(echo "$o" | tail -20)"; fi
else echo "SKIP 7: no udevadm here (the udev workflow runs it)"; fi
echo "$((N-F))/$N passed"; [ $F = 0 ]
