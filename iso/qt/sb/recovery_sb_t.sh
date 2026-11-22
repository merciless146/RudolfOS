#!/bin/sh
# 1.5.9 (docs/plan-1.5.9.md 8.4): the official recovery start-up file in QEMU, on OVMF with Secure Boot on (Microsoft's
# keys, as PCs ship, and a TEST Secure Boot certificate added to db: never the real key). The boot files are made by
# systools/mkefi.sh (shim, the menu, rudolf.efi and recovery.efi, signed with the test key), the menu set to Recovery
# mode; the kernel is a released one (KERNEL: its vmlinuz), with this repository's start-up script put over its own in
# recovery.efi only (RUDOLF_UKI_TEST_INITRD, tests only), and a TEST release key playing the official one
# (RUDOLF_TEST_KEYS). RUDOLFDATA holds system/r: the released kernel and small stand-in layers, its list signed by the
# test release key. Checks: Secure Boot on; a good system/r is checked and started (the kernel hands over to it: a
# second kernel starts); a system/r with a changed layer is refused with its reason; a recovery.efi changed after
# signing is refused by the firmware (no kernel starts).
#   SB_PARTS=DIR KERNEL=vmlinuz sh recovery_sb_t.sh       (DIR: the pinned Secure Boot parts, iso/boot/sb/parts.txt)
# Needs qemu-system-x86_64, OVMF (OVMF_CODE_4M.secboot.fd, OVMF_VARS_4M.ms.fd: SB_PARTS or /usr/share/OVMF), sbsign,
# virt-fw-vars (virt-firmware), mtools, mkfs.ext4, cpio, nasm, gcc, python3 with cryptography. Slow without KVM.
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../.." && pwd)
for c in qemu-system-x86_64 sbsign virt-fw-vars mcopy mkfs.ext4 cpio nasm gcc openssl; do command -v $c >/dev/null || { echo "SKIP: needs $c"; exit 0; }; done
[ -f "$KERNEL" ] && [ -d "$SB_PARTS" ] || { echo "SKIP: KERNEL (a released vmlinuz) and SB_PARTS are needed"; exit 0; }
OV=$SB_PARTS; [ -f $OV/OVMF_CODE_4M.secboot.fd ] || OV=/usr/share/OVMF
[ -f $OV/OVMF_CODE_4M.secboot.fd ] && [ -f $OV/OVMF_VARS_4M.ms.fd ] || { echo "SKIP: no OVMF with Secure Boot"; exit 0; }
KERNEL=$(realpath "$KERNEL"); W=$(mktemp -d /tmp/recsb.XXXXXX); QP=""
trap '[ -n "$QP" ] && kill $QP 2>/dev/null; [ -n "$RECSB_KEEP" ] || rm -rf "$W"' EXIT
P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $(echo "$3" | tail -c 1500)"; fi; }
# ---- TEST keys (deleted with W): a Secure Boot key and certificate, a release key
# (2.0, the kryptonite branch: RECSB_TEST_KEYDIR, a folder with a TEST sb.key and sb.crt the kernel under test was built
#  to trust (a test build's kernel: .github/workflows/kryptonite-kernel.yml makes them in its job); never the real key)
if [ -n "$RECSB_TEST_KEYDIR" ]; then cp "$RECSB_TEST_KEYDIR/sb.key" "$RECSB_TEST_KEYDIR/sb.crt" $W/ || { echo "FAIL test key"; exit 1; }
	openssl x509 -in $W/sb.crt -noout -subject | grep -q TEST || { echo "FAIL RECSB_TEST_KEYDIR: not a TEST certificate"; exit 1; }
else
(cd $W && openssl req -new -x509 -newkey rsa:2048 -nodes -days 30 -subj "/CN=Rudolf OS TEST Secure Boot/" -keyout sb.key -out sb.crt \
	-addext "keyUsage=critical,digitalSignature" -addext "extendedKeyUsage=codeSigning" >/dev/null 2>&1) || { echo "FAIL test key"; exit 1; }
fi
python3 - $W <<'PY' || { echo "FAIL release key"; exit 1; }
import sys, base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization
w = sys.argv[1]; k = Ed25519PrivateKey.generate()
open(w + '/rel.key', 'wb').write(k.private_bytes(serialization.Encoding.Raw, serialization.PrivateFormat.Raw, serialization.NoEncryption()))
open(w + '/rel.pub', 'w').write(base64.b64encode(k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)).decode() + '\n')
PY
# ---- the test's start-up script over the released kernel's (in recovery.efi only): this repository's init,
# rudolf-verity and rudolf-kexec
mkdir -p $W/ov/bin; cp $R/initramfs/init $W/ov/init; chmod 755 $W/ov/init
gcc -Os -static -o $W/ov/bin/rudolf-verity $R/sys/rudolf-verity.c && strip $W/ov/bin/rudolf-verity || { echo "FAIL rudolf-verity"; exit 1; }
gcc -Os -static -nostdlib -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -o $W/ov/bin/rudolf-kexec $R/sys/rudolf-kexec.c && strip $W/ov/bin/rudolf-kexec || { echo "FAIL rudolf-kexec"; exit 1; }
(cd $W/ov && find . | cpio -o -H newc --quiet -R 0:0 > $W/ov.cpio)
# ---- the boot files: mkefi.sh, as a disc's; then the menu set to Recovery mode at once
RUDOLF_TEST_KEYS=$W/rel.pub RUDOLF_UKI_TEST_INITRD=$W/ov.cpio RUDOLF_UKI_TEST_CMDLINE="console=ttyS0 ignore_loglevel earlyprintk=serial,ttyS0,115200${RECSB_APPEND:+ $RECSB_APPEND}" SB_KEY=$W/sb.key SB_CERT=$W/sb.crt \
	bash $R/systools/mkefi.sh $KERNEL 1.5.9 $SB_PARTS $W/EFIBOOT.IMG > $W/mkefi.log 2>&1 || { cat $W/mkefi.log; echo "FAIL mkefi.sh"; exit 1; }
mdir -i $W/EFIBOOT.IMG ::/EFI/rudolf > $W/esp.txt 2>&1
ok "0. mkefi.sh: recovery.efi beside rudolf.efi on the boot files, both signed; Recovery mode's entry names it" $(grep -qi 'recovery efi' $W/esp.txt && mcopy -n -i $W/EFIBOOT.IMG ::/EFI/rudolf/recovery.efi $W/recovery.efi && sbverify --cert $W/sb.crt $W/recovery.efi >/dev/null 2>&1 && mtype -i $W/EFIBOOT.IMG ::/loader/entries/rudolf-recovery.conf | grep -q '^efi /EFI/rudolf/recovery.efi'; echo $?) "$(cat $W/esp.txt)"
printf 'timeout 0\ndefault rudolf-recovery.conf\neditor no\nconsole-mode keep\n' > $W/loader.conf; mcopy -o -i $W/EFIBOOT.IMG $W/loader.conf ::/loader/loader.conf
mkiso() {
	rm -rf $W/t; mkdir -p $W/t/BOOT $W/t/EFI $W/t/RUDOLF; cp $KERNEL $W/t/BOOT/VMLINUZ; cp $1 $W/t/EFI/EFIBOOT.IMG
	nasm -f bin -o $W/t/BOOT/RBOOT.BIN $R/boot/rboot.asm; nasm -f bin -o $W/mbr.bin $R/boot/mbr.asm
	cp $R/initramfs/etc/rudolf-id $W/t/RUDOLF/ID; touch $W/t/RUDOLF/BOOTONLY
	python3 $R/mkimage.py iso $2 $W/t --mbr $W/mbr.bin --boot BOOT/RBOOT.BIN --kernel BOOT/VMLINUZ --efi EFI/EFIBOOT.IMG --label RUDOLF_TEST >/dev/null
}
mkiso $W/EFIBOOT.IMG $W/boot.iso || { echo "FAIL the boot image"; exit 1; }
# ---- RUDOLFDATA with system/r: the released kernel and stand-in layers, signed by the test release key
mkdata() {
	rm -rf $W/data; S=$W/data/os/system; mkdir -p $S/r; cp ${RECSB_RKERNEL:-$KERNEL} $S/r/vmlinuz
	for n in base chromium desktop modules; do head -c 1048576 /dev/urandom > $S/r/$n.sfs; done
	python3 - $S/r $W/rel.key <<'PY'
import sys, os, hashlib
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
d, kf = sys.argv[1:]; k = Ed25519PrivateKey.from_private_bytes(open(kf, 'rb').read())
t = 'rudolf-system 1\nserial 2026111599\nversion 1.5.9\n'
for n in ('vmlinuz', 'base', 'chromium', 'desktop', 'modules'):
    f = n if n == 'vmlinuz' else n + '.sfs'; b = open(os.path.join(d, f), 'rb').read()
    t += 'L\t%s\t%d\t%s\t%s\n' % (n, len(b), hashlib.sha256(b).hexdigest(), f)
open(d + '/manifest.txt', 'w').write(t); open(d + '/manifest.sig', 'wb').write(k.sign(t.encode()))
PY
	echo 1 > $S/r/complete; echo 'official recovery system (test)' > $S/r/official; printf 'good=r\nprev=\nnext=\ntries=0\nfailed=\n' > $S/state
	[ -n "$1" ] && eval "$1"
	rm -f $W/data.img; mkfs.ext4 -q -L RUDOLFDATA -d $W/data $W/data.img 64M >/dev/null 2>&1
}
# ---- OVMF: Secure Boot on, Microsoft's keys and the test certificate in db
VARS0=$W/vars0.fd
virt-fw-vars --input $OV/OVMF_VARS_4M.ms.fd --output $VARS0 --add-db 0f4b3c3a-0000-4000-8000-526f646f6c66 $W/sb.crt --secure-boot >/dev/null 2>&1 || { echo "FAIL virt-fw-vars"; exit 1; }
# boot ISO WAIT_FOR TIMEOUT: serial output in $W/serial.txt
boot() {
	cp $VARS0 $W/vars.fd; : > $W/serial.txt
	qemu-system-x86_64 -machine q35,smm=on -global driver=cfi.pflash01,property=secure,value=on -m ${RECSB_MEM:-1536} -smp 2 \
		-drive if=pflash,format=raw,unit=0,file=$OV/OVMF_CODE_4M.secboot.fd,readonly=on -drive if=pflash,format=raw,unit=1,file=$W/vars.fd \
		-drive file=$1,media=cdrom,if=none,id=cd -device ide-cd,drive=cd,bootindex=0 -drive file=$W/data.img,format=raw,if=virtio \
		-display none -serial file:$W/serial.txt -monitor none -no-reboot > $W/qemu.log 2>&1 & QP=$!
	i=0; while [ $i -lt $3 ]; do grep -qa "$2" $W/serial.txt && break; grep -qa "end Kernel panic" $W/serial.txt && break; kill -0 $QP 2>/dev/null || break; timeout 2 tail -f /dev/null; i=$((i + 2)); done
	timeout 3 tail -f /dev/null; kill $QP 2>/dev/null; wait $QP 2>/dev/null; QP=""
	tr -d '\r' < $W/serial.txt | sed 's/\x1b\[[0-9;?]*[a-zA-Z]//g' > $W/s.txt
	# (2.0, the kryptonite branch: RECSB_SERIAL, a folder that keeps each start's serial output for the check's log)
	[ -n "$RECSB_SERIAL" ] && { mkdir -p "$RECSB_SERIAL"; NB=$((${NB:-0} + 1)); cp $W/s.txt "$RECSB_SERIAL/boot$NB.txt"; }
}
T=${RECSB_TIMEOUT:-900}
# 1. a good system/r
# (system/r's own kernel then checks the stand-in layers with its own, real key: they fail there, as they must, and
# its start-up says so and stops: that is where this test ends)
mkdata ""; boot $W/boot.iso 'Press Enter to restart' $T
o=$(cat $W/s.txt); NK=$(grep -c 'Linux version' $W/s.txt)
ok "1. Secure Boot on (the kernel locked down): the menu's Recovery mode started recovery.efi (the recovery mark)" $(grep -q 'Secure boot enabled' $W/s.txt && grep -q 'Kernel is locked down' $W/s.txt && grep -q 'checking the official recovery system: vmlinuz' $W/s.txt; echo $?) "$o"
ok "...system/r checked (each file) and started: the kernel handed over to system/r's (signed, under lockdown)" $([ "$(grep -c 'checking the official recovery system: ' $W/s.txt)" = 5 ] && grep -q 'starting the official recovery system (release 2026111599, checked)' $W/s.txt && [ "$NK" -ge 2 ] && grep -q 'Command line:.* rudolf.slot=r rudolf.official=1 rudolf.sb=1 lockdown=integrity' $W/s.txt; echo $?) "$o"
ok "...and system/r's own start-up ran (no stale initramfs: the empty one given at the handover)" $(! grep -q 'Cannot find place for new RAMDISK' $W/s.txt && grep -q "Rudolf OS: the system layer '[a-z]*' did not pass the check" $W/s.txt; echo $?) "$o"
# (2.0, the kryptonite branch: RECSB_RKERNEL, another kernel for system/r than the boot files' (which kernel hands over to
#  which); RECSB_ONLY1=1 stops here, for comparing start-up options (RECSB_APPEND, more options on the
#  test kernel bundle's command line) and memory sizes (RECSB_MEM, MiB) on the handover alone)
[ -n "$RECSB_ONLY1" ] && { echo "$P/$((P+F)) passed"; [ $F = 0 ]; exit; }
# 2. a changed layer
mkdata 'printf X | dd of=$S/r/chromium.sfs bs=1 seek=5000 conv=notrunc 2>/dev/null'; boot $W/boot.iso 'Press Enter to restart' $T
o=$(cat $W/s.txt)
ok "2. a system/r with a changed layer: refused, saying which and why, nothing started" $(grep -q 'refused the recovery system on this computer (system/r): its file chromium was changed' $W/s.txt && [ "$(grep -c 'Linux version' $W/s.txt)" = 1 ]; echo $?) "$o"
# 3. a release list signed by another key (a developer's): refused
mkdata 'python3 -c "import sys;d=sys.argv[1];open(d,\"wb\").write(bytes(64))" $S/r/manifest.sig'; boot $W/boot.iso 'Press Enter to restart' $T
o=$(cat $W/s.txt)
ok "3. a system/r whose list is not signed by the (test) official key: refused" $(grep -q 'not signed by the official Rudolf OS key' $W/s.txt && [ "$(grep -c 'Linux version' $W/s.txt)" = 1 ]; echo $?) "$o"
# 4. recovery.efi changed after signing: the firmware refuses it
cp $W/EFIBOOT.IMG $W/bad.img; python3 - $W/recovery.efi <<'PY'
import sys; p = sys.argv[1]; b = bytearray(open(p, 'rb').read()); i = b.find(b'official recovery start-up file'); b[i] ^= 1; open(p, 'wb').write(b)
PY
mcopy -o -i $W/bad.img $W/recovery.efi ::/EFI/rudolf/recovery.efi; mkiso $W/bad.img $W/bad.iso
mkdata ""; boot $W/bad.iso 'Linux version' ${RECSB_BAD_TIMEOUT:-240}
o=$(cat $W/s.txt)
ok "4. a recovery.efi changed after signing: Secure Boot refuses it, no kernel starts" $(! grep -q 'Linux version' $W/s.txt && grep -q 'Error loading .EFI.rudolf.recovery.efi: Access denied' $W/s.txt; echo $?) "$o"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
