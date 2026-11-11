#!/bin/sh
# 1.5.8: a sound chip that did not answer at start-up ("no codecs initialized"; the Vivobook's speakers after forced
# power-offs) is reset once (sound codec-check), and the self-test says so, or says to shut down fully.   sh codec58_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SO=$HERE/../../sys/root/usr/lib/rudolf/sound; ST=$HERE/../../sys/root/usr/lib/rudolf/selftest
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
A=0000:03:00.6; mkdir -p $W/pci/$A $W/drv $W/run $W/bin
printf '[    3.430471] snd_hda_intel %s: Codec #0 probe error; disabling it...\n[    3.430481] snd_hda_intel %s: no codecs initialized\n' $A $A > $W/dmesg
printf '#!/bin/sh\necho "$*" >> %s/health\n' $W > $W/bin/rudolfd; chmod 755 $W/bin/rudolfd
# a stand-in driver: binding makes the card (as a chip that answers this time would)
cat > $W/bin/watch.sh <<'X'
W=$1; A=$2; while [ ! -s $W/drv/bind ]; do sleep 0.1; done; mkdir -p $W/pci/$A/sound/card1
X
run() { RUDOLF_TEST_PATH=$W/bin RUDOLF_TEST_DMESG=$W/dmesg RUDOLF_TEST_PCI=$W/pci RUDOLF_TEST_DRV=$W/drv RUDOLF_TEST_RUN=$W/run RUDOLF_SOUND_RESET_WAIT=4 sh $SO codec-check; }
sh $W/bin/watch.sh $W $A & o=$(run)
ok "1. the chip is let go of and taken again by its driver" $([ "$(cat $W/drv/unbind)" = $A ] && [ "$(cat $W/drv/bind)" = $A ]; echo $?) "$o"
ok "2. it works now: said in System health" $(echo "$o" | grep -q 'reset, it works now' && grep -q 'hardware the sound chip did not answer at start-up; it was reset and works now' $W/health; echo $?) "$o"
: > $W/drv/bind; o=$(run); ok "3. once per start" $([ ! -s $W/drv/bind ] && [ -z "$o" ]; echo $?) "$o"
rm -rf $W/run/* $W/pci/$A/sound; : > $W/health; o=$(run)
ok "4. still silent: System health says to shut down fully" $(grep -q 'shut the computer down fully' $W/health; echo $?) "$o"
mkdir -p $W/sysx/bus/pci; ln -s $W/pci $W/sysx/bus/pci/devices; o=$(ST_SYS=$W/sysx ST_RUN=$W/r ST_PERSIST=$W/pp ST_HOME=$W/h ST_DEV=$W/d ST_PROC=$W/proc ST_DMESG=$W/dmesg sh $ST 2>/dev/null)
ok "5. self-test: a chip with no card is named, with what to do" $(echo "$o" | grep -q "FAIL sound-chip - the sound chip did not answer at start-up ($A): shut the computer down fully"; echo $?) "$(echo "$o" | grep sound)"
mkdir -p $W/pci/$A/sound/card1; o=$(ST_SYS=$W/sysx ST_RUN=$W/r ST_PERSIST=$W/pp ST_HOME=$W/h ST_DEV=$W/d ST_PROC=$W/proc ST_DMESG=$W/dmesg sh $ST 2>/dev/null)
ok "6. ...and one that was reset passes" $(echo "$o" | grep -q 'PASS sound-chip - it did not answer at start-up, and was reset'; echo $?)
echo '[ 1.0] snd_hda_intel 0000:03:00.1: bound' > $W/dmesg; o=$(ST_SYS=$W/sysx ST_RUN=$W/r ST_PERSIST=$W/pp ST_HOME=$W/h ST_DEV=$W/d ST_PROC=$W/proc ST_DMESG=$W/dmesg sh $ST 2>/dev/null)
ok "7. every chip answered: passes" $(echo "$o" | grep -q 'PASS sound-chip - every sound chip answered'; echo $?)
grep -q 'sound codec-check' $HERE/../../sys/root/sbin/rudolf-init; ok "8. rudolf-init runs it after start-up" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
