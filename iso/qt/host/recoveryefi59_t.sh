#!/bin/sh
# 1.5.9 (docs/plan-1.5.9.md 8.4): the official recovery start-up file (/EFI/rudolf/recovery.efi). Its initramfs (the
# kernel's own, with /etc/rudolf-recovery and the official key alone added by mkuki.py --recovery) starts only
# system/r, after checking its release list's signature with the official key and every file's size and SHA-256;
# Developer mode's extra key (/persist/etc/rudolf/extra-keys) is never used. The start-up script's own lines with the
# real rudolf-verity (as startfall58_t.sh), throwaway keys: one plays the official key, one a developer's.
# Also mkuki.py --recovery: the bundle's .initrd holds the mark and exactly one key (with a systemd stub: SB_PARTS or
# the system's). Needs root (a private mount namespace), gcc, python3 with cryptography.   sh recoveryefi59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); R=$HERE/../..; I=$R/initramfs/init
[ "$(id -u)" = 0 ] || { echo "run as root"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
mkdir -p $W/bin $W/etc
gcc -O2 -o $W/bin/rudolf-verity $R/sys/rudolf-verity.c 2>$W/cc || { cat $W/cc; echo "FAIL build rudolf-verity"; exit 1; }
python3 - "$W" <<'P' || { echo "FAIL keys"; exit 1; }
import sys, base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization
w = sys.argv[1]
for n in ('official', 'dev'):
    k = Ed25519PrivateKey.generate()
    open(f'{w}/{n}.key', 'wb').write(k.private_bytes(serialization.Encoding.Raw, serialization.PrivateFormat.Raw, serialization.NoEncryption()))
    pub = k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    open(f'{w}/{n}.pub', 'w').write(base64.b64encode(pub).decode() + '\n')
P
cp $W/official.pub $W/etc/rudolf-official-key
# the script's lines: the recovery mark, the choice of system, the layers (rudolf-verity open stubbed: which keys it was given)
{ echo 'say() { echo "$*"; }; note() { :; }; keeplog() { echo "LOG $*"; }; fail() { echo FAIL-STOP; exit 0; }; sync() { :; }; umount() { :; }; umount_media() { :; }; mount_media() { :; }; blkid() { echo "x UUID=\"u1\" y"; }; whole() { return 1; }; sleep() { :; }'
  echo 'reboot() { echo REBOOT; exit 0; }; kexec_stub() { echo "KEXEC $2"; exit 0; }'
  echo 'is_usb() { return 1; }; disk_of() { echo "${1%[0-9]}"; }; is_data() { return 1; }'
  echo 'has() { case " $CMD " in *" $1 "*) return 0;; esac; return 1; }'
  echo 'rv_open() { echo "VERITY $L keys=$3" >&2; [ "$3" = "$GOODKEYS" ] && echo /dev/dm-0; }; losetup() { [ "$1" = -f ] && echo /dev/loop9; return 0; }; mount() { return 0; }'
  sed -n '/^OFFREC=""; OKEY=/,/^if \[ -e \/etc\/rudolf-recovery \]/p' "$I" | sed "s#/etc/rudolf-#$W/etc/rudolf-#g"
  sed -n '/^SYSD=\/mnt\/persist/,/^slot_bad() {/p' "$I"; sed -n '/^slot_bad() {/,/^}/p' "$I" | tail -n +2
  sed -n '/^REC=""/,/^# the media check/p' "$I" | sed '$d' | sed 's/rudolf-kexec /kexec_stub /'
  sed -n '/^MAN=""; MSIG=""/,/^	LOWER=/p' "$I" | sed 's/rudolf-verity open /rv_open /; s#/etc/rudolf-keys#$W/etc/rudolf-keys#'; echo 'done; echo LAYERS-OK'
} > $W/part.sh
grep -q 'official_check' $W/part.sh && grep -q 'OFFREC=1' $W/part.sh && grep -q rv_open $W/part.sh || { echo "FAIL could not take the script's lines"; exit 1; }
unshare -m --propagation private sh -s "$W" <<'XEOF'
W=$1; P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $(echo "$3" | head -c 900)"; fi; }
export PATH=$W/bin:$PATH
mount -t tmpfs t /mnt; S=/mnt/persist/os/system
# a release in slot DIR, its list signed by KEY (official or dev)
mkrel() { mkdir -p $1; for f in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do head -c $((4096 + ${#f} * 1000)) /dev/urandom > $1/$f; done; echo 1 > $1/complete
	python3 - $1 $W/$2.key <<'P'
import sys, os, hashlib
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
d, kf = sys.argv[1:]; k = Ed25519PrivateKey.from_private_bytes(open(kf, 'rb').read())
t = 'rudolf-system 1\nserial 2026111500\nversion 1.5.9\n'
for n in ('vmlinuz', 'base', 'chromium', 'desktop', 'modules'):
    f = n if n == 'vmlinuz' else n + '.sfs'; b = open(os.path.join(d, f), 'rb').read()
    t += 'L\t%s\t%d\t%s\t%s\n' % (n, len(b), hashlib.sha256(b).hexdigest(), f)
open(d + '/manifest.txt', 'w').write(t); open(d + '/manifest.sig', 'wb').write(k.sign(t.encode()))
P
}
reset() { rm -rf /mnt/persist /mnt/media; mkdir -p $S /mnt/media/rudolf; mkrel $S/a official; mkrel $S/r ${1:-official}; echo 'official recovery system' > $S/r/official; printf 'good=a\nprev=\nnext=\ntries=0\nfailed=\n' > $S/state; }
# (a whole disc: its own layers and signed list on the start drive)
media() { mkrel /mnt/media/rudolf official; }
# (REC=1 as the menu's Recovery mode entry gives; OFFREC from the mark when MARK=1)
run() { (CMD="rudolf.boot=uefi $1"; SLOT=""; PERSIST=/dev/vda3; MEDIA=/dev/sr0; SB=1; GOODKEYS=$W/etc/rudolf-official-key; [ "$MARK" = 1 ] && touch $W/etc/rudolf-recovery || rm -f $W/etc/rudolf-recovery; . $W/part.sh) 2>&1; }
MARK=1
reset; o=$(run "")
ok "1. a good official system/r: checked (every file), then started (rudolf.slot=r, recovery's safe settings)" $(echo "$o" | grep -q 'KEXEC .*rudolf.recovery=1.*rudolf.slot=r rudolf.official=1' && [ "$(echo "$o" | grep -c 'checking the official recovery system: ')" = 5 ] && echo "$o" | grep -q 'starting the official recovery system (release 2026111500, checked)'; echo $?) "$o"
reset; printf 'X' | dd of=$S/r/base.sfs bs=1 seek=100 conv=notrunc 2>/dev/null; o=$(run "")
ok "2. a changed file (same size, one byte): refused, saying which file and why, and nothing else started" $(echo "$o" | grep -q "refused the recovery system on this computer (system/r): its file base was changed (its SHA-256 is not the one in its signed list)" && echo "$o" | grep -q FAIL-STOP && ! echo "$o" | grep -q KEXEC; echo $?) "$o"
reset; echo more >> $S/r/desktop.sfs; o=$(run "")
ok "3. a file of another size: refused (its size)" $(echo "$o" | grep -q "its file desktop is not the size its signed list says" && ! echo "$o" | grep -q KEXEC; echo $?) "$o"
reset; printf '\001' | dd of=$S/r/manifest.sig bs=1 seek=10 conv=notrunc 2>/dev/null; o=$(run "")
ok "4. a wrong signature: refused (not signed by the official key)" $(echo "$o" | grep -q "its release list is not signed by the official Rudolf OS key\.$" && ! echo "$o" | grep -q KEXEC; echo $?) "$o"
reset; sed -i 's/^version 1.5.9$/version 1.5.99/' $S/r/manifest.txt; o=$(run "")
ok "...a changed release list with its old signature: refused" $(echo "$o" | grep -q "not signed by the official Rudolf OS key" && ! echo "$o" | grep -q KEXEC; echo $?) "$o"
reset dev; o=$(run "")
ok "5. a system/r signed by a developer's key (marked official all the same): refused" $(echo "$o" | grep -q "not signed by the official Rudolf OS key" && ! echo "$o" | grep -q KEXEC; echo $?) "$o"
reset dev; mkdir -p /mnt/persist/etc/rudolf; cp $W/dev.pub /mnt/persist/etc/rudolf/extra-keys; o=$(run "")
ok "6. ...and with Developer mode's extra key (that developer's) present: still refused, saying the extra key is not used" $(echo "$o" | grep -q "not signed by the official Rudolf OS key (Developer mode's extra key is not used in Recovery mode)" && ! echo "$o" | grep -q KEXEC; echo $?) "$o"
reset; mkdir -p /mnt/persist/etc/rudolf; cp $W/dev.pub /mnt/persist/etc/rudolf/extra-keys; o=$(run "")
ok "7. a good official system/r with an extra key present: started (the extra key ignored, not a reason to refuse)" $(echo "$o" | grep -q 'KEXEC .*rudolf.slot=r rudolf.official=1'; echo $?) "$o"
reset; rm $S/r/complete; o=$(run "")
ok "8. an incomplete system/r: refused, not another system instead (slot a is not started)" $(echo "$o" | grep -q "system/r): it is incomplete" && ! echo "$o" | grep -q KEXEC; echo $?) "$o"
reset; rm $S/r/modules.sfs; o=$(run "")
ok "...a missing layer: refused" $(echo "$o" | grep -q "system/r): it is incomplete\|its file modules is missing" && ! echo "$o" | grep -q KEXEC; echo $?) "$o"
reset; rm -rf $S/r; media; o=$(run "")
ok "9. no system/r at all: the drive's own system, its layers checked with the official key alone" $(! echo "$o" | grep -q KEXEC && echo "$o" | grep -q "no recovery system on this computer's drive" && echo "$o" | grep -q LAYERS-OK && ! echo "$o" | grep "^VERITY" | grep -qv "keys=$W/etc/rudolf-official-key"; echo $?) "$o"
reset; rm -rf $S/r; media; o=$( (CMD="rudolf.boot=uefi"; SLOT=""; PERSIST=/dev/vda3; MEDIA=/dev/sr0; SB=0; GOODKEYS=nothing; touch $W/etc/rudolf-recovery; . $W/part.sh) 2>&1)
ok "...and a layer that does not pass that check stops it, even with Secure Boot off" $(echo "$o" | grep -q "did not pass the check with the official key" && echo "$o" | grep -q FAIL-STOP && ! echo "$o" | grep -q LAYERS-OK; echo $?) "$o"
MARK=0
reset dev; o=$(run "rudolf.recovery=1")
ok "10. without the mark (rudolf.efi's Recovery mode, older boot files): as before (slot r first if present, its layers checked by the second start-up)" $(echo "$o" | grep -q 'KEXEC .*rudolf.slot=r' && ! echo "$o" | grep -q 'rudolf.official=1' && ! echo "$o" | grep -q 'checking the official'; echo $?) "$o"
reset; o=$(run "")
ok "...and a normal start is unchanged (the good slot)" $(echo "$o" | grep -q 'KEXEC .*rudolf.slot=a' && ! echo "$o" | grep -q 'rudolf.recovery=1'; echo $?) "$o"
echo "$P $F" > $W/res
XEOF
set -- $(cat $W/res 2>/dev/null || echo 0 1); P=$1; F=$2
# ---- mkuki.py --recovery: the bundle (needs a systemd stub)
STUB=${SB_PARTS:+$SB_PARTS/linuxx64.efi.stub}; [ -f "$STUB" ] || STUB=/usr/lib/systemd/boot/efi/linuxx64.efi.stub
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
if [ -f "$STUB" ] && command -v objcopy >/dev/null && command -v cpio >/dev/null; then
	head -c 100000 /dev/urandom > $W/k
	python3 $R/systools/mkuki.py "$STUB" $W/k 1.5.9 $W/rec.efi --recovery $W/official.pub >/dev/null 2>$W/e && objcopy -O binary --only-section=.initrd $W/rec.efi $W/ird && objcopy -O binary --only-section=.cmdline $W/rec.efi $W/cl
	mkdir $W/x && (cd $W/x && cpio -id --quiet < $W/ird)
	ok "11. mkuki.py --recovery: the bundle's .initrd holds the mark, the official key alone and the empty initramfs for the handover; its options say recovery" $(grep -q 'official recovery start-up file' $W/x/etc/rudolf-recovery && cmp -s $W/x/etc/rudolf-official-key $W/official.pub && [ "$(ls $W/x/etc | wc -l)" = 3 ] && [ "$(head -c 6 $W/x/etc/rudolf-kexec-initrd)" = 070701 ] && grep -qa 'rudolf.boot=uefi rudolf.recovery=1' $W/cl; echo $?) "$(cat $W/e; ls -R $W/x)"
	python3 $R/systools/mkuki.py "$STUB" $W/k 1.5.9 $W/n.efi >/dev/null && ! objcopy -O binary --only-section=.initrd $W/n.efi $W/ird2 2>/dev/null || [ ! -s $W/ird2 ]
	ok "...the normal bundle (rudolf.efi) has none" $? ""
	cat $W/official.pub $W/official.pub $W/official.pub $W/official.pub $W/official.pub > $W/five
	python3 $R/systools/mkuki.py "$STUB" $W/k 1.5.9 $W/t.efi --recovery $W/five >/dev/null 2>$W/e2
	ok "...a key file with five keys is refused (one to four: 1.6.3 Part 0)" $([ $? != 0 ] && grep -q 'one to four public keys' $W/e2; echo $?) "$(cat $W/e2)"
	printf -- '-----BEGIN PRIVATE KEY-----\nMC4CAQAwBQYDK2VwBCIEIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n-----END PRIVATE KEY-----\n' > $W/priv
	python3 $R/systools/mkuki.py "$STUB" $W/k 1.5.9 $W/t.efi --recovery $W/priv >/dev/null 2>&1
	ok "...a private key is refused" $([ $? != 0 ]; echo $?) ""
	python3 $R/systools/mkuki.py "$STUB" $W/k 1.5.9 $W/o.efi --recovery $R/trusted-keys.txt >/dev/null 2>&1 && objcopy -O binary --only-section=.initrd $W/o.efi $W/ird3 && mkdir $W/y && (cd $W/y && cpio -id --quiet < $W/ird3)
	ok "...the repository's trusted-keys.txt: the official keys only (the release key and, 1.6.3 Part 0, its replacement), both in recovery.efi" $([ $? = 0 ] && [ "$(grep -v '^#' $R/trusted-keys.txt | grep -c .)" = 2 ] && grep -qx 'UqTqXIW8vtITjwoMix+MkFVZ0GAee9+kOS1WBIs/3mo=' $W/y/etc/rudolf-official-key && grep -qx 'IcNJiy4zFkKUU3g3jalcgTQlneiDXv3E0/bbJt19cVg=' $W/y/etc/rudolf-official-key; echo $?) ""
else echo "SKIP mkuki.py's checks: no systemd stub (linuxx64.efi.stub), objcopy or cpio"; fi
grep -q '^efi /EFI/rudolf/recovery.efi$' $R/boot/sb/loader/entries/rudolf-recovery.conf && grep -q 'EFI/rudolf/recovery.efi' $R/systools/mkefi.sh
ok "12. the boot menu's Recovery mode starts recovery.efi, which mkefi.sh makes and signs beside rudolf.efi" $? ""
echo "$P/$((P+F)) passed"; [ $F = 0 ]
