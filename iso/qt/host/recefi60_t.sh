#!/bin/bash
# 1.6.0 (the owner, 2026-11-19: "Recovery Mode boot is always locked to the official build's most recent stable
# version"): oscopy.sh's os_recefi, as Make a stick runs it: a stick made from another build gets the official
# Recovery mode start-up file (recovery.efi) of its official recovery system's release, from the boot image beside
# that release, checked; the build's own start is left. A local server, a folder for the stick's EFI system partition,
# a stand-in sbverify (official = the file holds OFFICIAL) and a stand-in for opening the image (a tar).
#   bash recefi60_t.sh   (python3)
set -u; cd "$(dirname "$0")"; I=$(cd ../.. && pwd); W=$(mktemp -d); P=0; F=0; SP=
trap '[ -n "$SP" ] && kill $SP 2>/dev/null; rm -rf "$W"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
mkdir -p $W/bin $W/srv/files/2026120100 $W/img/EFI/rudolf $W/img/loader/entries
printf '#!/bin/sh\nf=; for a; do f=$a; done; grep -q OFFICIAL "$f"\n' > $W/bin/sbverify
printf '#!/bin/sh\ntar -xf "$1" -C "$2"\n' > $W/bin/openiso
printf '#!/bin/sh\n[ "$1" = fetch ] || exit 2\nexec python3 -c "import sys,urllib.request;open(sys.argv[2],\\"wb\\").write(urllib.request.urlopen(sys.argv[1]).read())" "$2" "$3"\n' > $W/bin/ru
chmod +x $W/bin/*
{ echo OFFICIAL recovery kernel 2026120100; head -c 300000 /dev/urandom | base64; } > $W/img/EFI/rudolf/recovery.efi
printf 'title Recovery mode\nefi /EFI/rudolf/recovery.efi\n' > $W/img/loader/entries/rudolf-recovery.conf
(cd $W/img && tar -cf $W/srv/files/2026120100/boot.iso .); (cd $W/srv/files/2026120100 && sha256sum boot.iso | cut -c1-64 > boot.iso.sha256)
PORT=$((20000 + RANDOM % 20000)); (cd $W/srv && exec python3 -m http.server $PORT --bind 127.0.0.1 >/dev/null 2>&1) & SP=$!; sleep 1
U=http://127.0.0.1:$PORT
. $I/sys/root/usr/lib/rudolf/oscopy.sh
export SBVERIFY=$W/bin/sbverify SB_CERT=/dev/null RUDOLF_TEST_OPENISO=$W/bin/openiso RUDOLF_UPDATE=$W/bin/ru RUDOLF_RECEFI_CACHE=$W/cache
stick() {   # a stick from another build: its own boot files, the official recovery system (slot r)
	rm -rf $W/st $W/esp; mkdir -p $W/st/os/system/r $W/esp/EFI/rudolf $W/esp/EFI/BOOT $W/esp/loader/entries
	echo "other build's own recovery" > $W/esp/EFI/rudolf/recovery.efi; echo "other build's start" > $W/esp/EFI/rudolf/rudolf.efi
	echo "other build's shim" > $W/esp/EFI/BOOT/BOOTX64.EFI; printf 'title Recovery mode\nefi /EFI/rudolf/recovery.efi\n' > $W/esp/loader/entries/rudolf-recovery.conf
	: > $W/st/os/system/r/official; printf 'rudolf-system 1\nserial 2026120100\nL\tvmlinuz\t1\tx\t%s/files/2026120100/vmlinuz\n' "$U" > $W/st/os/system/r/manifest.txt; }
run() { o=$(RUDOLF_TEST_ESP=$W/esp os_recefi /dev/sdz $W/st 2>&1); r=$?; }
stick; run
ok "1. another build's stick: its recovery.efi replaced by the official one of the recovery system's release (2026120100)" \
	$([ $r = 0 ] && cmp -s $W/esp/EFI/rudolf/recovery.efi $W/img/EFI/rudolf/recovery.efi && echo "$o" | grep -q 'starts the official Rudolf OS (release 2026120100)' && echo 0 || echo 1) "$o"
ok "2. ...the build's own start (rudolf.efi, its shim) left as it was; no file left half-written" \
	$(grep -q "other build's start" $W/esp/EFI/rudolf/rudolf.efi && grep -q "other build's shim" $W/esp/EFI/BOOT/BOOTX64.EFI && [ ! -e $W/esp/EFI/rudolf/recovery.efi.new ] && echo 0 || echo 1)
ok "3. ...kept for the next stick (by release)" $(cmp -s $W/cache/2026120100/recovery.efi $W/img/EFI/rudolf/recovery.efi && echo 0 || echo 1)
run; ok "4. already official: nothing done, nothing said" $([ $r = 0 ] && [ -z "$o" ] && echo 0 || echo 1) "$o"
kill $SP; SP=; stick; run
ok "5. offline, from what was kept: still the official one" $(cmp -s $W/esp/EFI/rudolf/recovery.efi $W/img/EFI/rudolf/recovery.efi && echo 0 || echo 1) "$o"
rm -rf $W/cache; stick; run
ok "6. offline with nothing kept: the build's own kept, a warning, never an error" \
	$([ $r = 0 ] && grep -q "other build's own recovery" $W/esp/EFI/rudolf/recovery.efi && echo "$o" | grep -q 'could not be downloaded' && echo 0 || echo 1) "$o"
(cd $W/srv && exec python3 -m http.server $PORT --bind 127.0.0.1 >/dev/null 2>&1) & SP=$!; sleep 1
echo 0000000000000000000000000000000000000000000000000000000000000000 > $W/srv/files/2026120100/boot.iso.sha256
stick; run
ok "7. a boot image that does not match its checksum: refused, the stick's start-up files unchanged" \
	$(grep -q "other build's own recovery" $W/esp/EFI/rudolf/recovery.efi && echo "$o" | grep -q 'did not match its checksum' && echo 0 || echo 1) "$o"
sed -i 's/OFFICIAL/UNSIGNED/' $W/img/EFI/rudolf/recovery.efi; (cd $W/img && tar -cf $W/srv/files/2026120100/boot.iso .); (cd $W/srv/files/2026120100 && sha256sum boot.iso | cut -c1-64 > boot.iso.sha256)
stick; run
ok "8. a Recovery mode file in the image not signed by Rudolf OS: refused, unchanged" \
	$(grep -q "other build's own recovery" $W/esp/EFI/rudolf/recovery.efi && echo "$o" | grep -q 'not signed by Rudolf OS' && [ ! -d $W/cache/2026120100 ] && echo 0 || echo 1) "$o"
sed -i 's/UNSIGNED/OFFICIAL/' $W/img/EFI/rudolf/recovery.efi; (cd $W/img && tar -cf $W/srv/files/2026120100/boot.iso .); (cd $W/srv/files/2026120100 && sha256sum boot.iso | cut -c1-64 > boot.iso.sha256)
stick; rm $W/esp/loader/entries/rudolf-recovery.conf; run
ok "9. a build whose boot menu has no Recovery mode entry: the official entry added" \
	$(grep -q '^efi /EFI/rudolf/recovery.efi' $W/esp/loader/entries/rudolf-recovery.conf && cmp -s $W/esp/EFI/rudolf/recovery.efi $W/img/EFI/rudolf/recovery.efi && echo 0 || echo 1) "$o"
stick; rm $W/st/os/system/r/official; echo rescue > $W/st/os/system/r/rescue; run
ok "10. a stick whose recovery system is not the official one (a rescue copy, offline): left as it is" $([ -z "$o" ] && grep -q "other build's own recovery" $W/esp/EFI/rudolf/recovery.efi && echo 0 || echo 1) "$o"
stick; sed -i "s#$U#http://example.com#" $W/st/os/system/r/manifest.txt; rm -rf $W/cache; run
ok "11. a release list naming no https address: nothing downloaded, unchanged" $(grep -q "other build's own recovery" $W/esp/EFI/rudolf/recovery.efi && echo "$o" | grep -q 'names no download address' && echo 0 || echo 1) "$o"
grep -q 'os_recefi "$DEV" /mnt/newstick || true' $I/sys/root/usr/lib/rudolf/mkstick.sh
ok "12. Make a stick runs it after the recovery system is on the stick, before the stick is checked" $?
echo; echo "$P/$((P + F)) passed"; [ $F = 0 ]
