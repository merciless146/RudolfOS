#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md "Before stable" 15): rudolfd's firmware entry for the installed drive, on the Vivobook's own
# entries of 2026-11-16, in a private mount namespace (a stand-in /sys, its EFI variables a plain folder, the drive a
# file holding the release's boot image with Install here's layout). The Vivobook: its "Rudolf OS" entry (learned by
# 1.4.2 from the entry made in the firmware's setup, "Drive") names the drive's start-up files by their place,
# NVMe(...)/CDROM(1,0x52EC,0x1E000)/\EFI\BOOT\BOOTX64.EFI: 1.4.1's boot image. Once 1.6.0's boot image was written over
# the area (the automatic repair, #433), its EFI image is at CDROM(1,0x5AC4,0x36000), so every entry for the drive named
# nothing, and the firmware opened its setup screen.
#   BOOTISO=boot.iso [RELEASED=rudolfd] sh bootentry60_t.sh     (root; RELEASED: a released rudolfd, run as it starts,
#   to show what it does with the same entries; BOOTISO: 1.6.0's boot image, r2026111620's or newer)
HERE=$(cd "$(dirname "$0")" && pwd); C=$HERE/../../sys/rudolfd.c
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root (a private mount namespace)"; exit 0; }
[ -s "$BOOTISO" ] || { echo "SKIP: BOOTISO (a release's boot.iso) is needed"; exit 0; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
gcc -O2 -w -o $W/rudolfd "$C" -Wl,-Bstatic -lz -Wl,-Bdynamic || { echo "FAIL rudolfd not built"; exit 1; }
# ---- the drive: the boot image, partition 1 cleared, RUDOLFDATA as partition 3 at 128 MiB, a disk signature
cp "$BOOTISO" $W/disk; truncate -s 160M $W/disk
python3 - $W/disk <<'X'
import sys,struct
f=open(sys.argv[1],'r+b'); f.seek(440); f.write(bytes.fromhex('6a5ba9d4')); f.seek(446); f.write(bytes(16))
f.seek(478); f.write(bytes([0,0xfe,0xff,0xff,0x83,0xfe,0xff,0xff])+struct.pack('<II',262144,40960))
X
# ---- the EFI variables: the Vivobook's (docs/plan-1.4.1.md Stage C 4: its prefix, its EUI, its "Drive"), as files
python3 - $W/vars $BOOTISO <<'X'
import sys,os,struct
d=sys.argv[1]; os.makedirs(d); G='8be4df61-93ca-11d2-aa0d-00e098032b8c'
def var(n,b): open(f'{d}/{n}-{G}','wb').write(struct.pack('<I',7)+b)
def node(t,s,b): return bytes([t,s])+struct.pack('<H',len(b)+4)+b
pre=node(2,1,struct.pack('<II',0x0a0341d0,0))+node(1,1,bytes([2,2]))+node(1,1,bytes([0,0]))+node(3,0x17,struct.pack('<I',1)+bytes.fromhex('0026b738227247d5'))
fil=lambda p: node(4,4,(p+'\0').encode('utf-16-le')); end=bytes([0x7f,0xff,4,0])
cd=lambda e,s,n: node(4,2,struct.pack('<IQQ',e,s,n))
def opt(t,p,a=1): return struct.pack('<IH',a,len(p))+(t+'\0').encode('utf-16-le')+p
old=pre+cd(1,0x52EC,0x1E000)+fil('\\EFI\\BOOT\\BOOTX64.EFI')+end
var('Boot0000',opt('Windows Boot Manager',node(4,1,struct.pack('<IQQ',1,2048,204800)+bytes(16)+bytes([2,2]))+fil('\\EFI\\Microsoft\\Boot\\bootmgfw.efi')+end))
var('Boot0001',opt('Rudolf OS',old)); var('Boot0003',opt('Drive',old))
var('Boot0004',opt('UEFI: USB DISK 3.0 PMAP',node(2,1,struct.pack('<II',0x0a0341d0,0))+node(1,1,bytes([3,8]))+node(1,1,bytes([3,0]))+node(3,5,bytes([2,0]))+end))
var('BootOrder',struct.pack('<HHH',1,3,0)); var('BootCurrent',struct.pack('<H',4))
open(f'{d}/../pre.hex','w').write(pre.hex())
X
cp -a $W/vars $W/vars0
# ---- the stand-in /sys and the rest, inside the namespace
mkdir -p $W/sys/devices/pci0000:00/firmware_node "$W/sys/devices/pci0000:00/0000:00:02.2/0000:01:00.0/nvme/nvme0/nvme0n1/queue" $W/sys/block $W/sys/firmware/efi/efivars
N="$W/sys/devices/pci0000:00/0000:00:02.2/0000:01:00.0/nvme/nvme0/nvme0n1"
echo 0 > $W/sys/devices/pci0000:00/firmware_node/uid; echo 1 > $N/nsid; echo "00 26 b7 38 22 72 47 d5" > $N/eui; echo 0 > $N/removable; echo 512 > $N/queue/logical_block_size
ln -s ../devices/pci0000:00/0000:00:02.2/0000:01:00.0/nvme/nvme0/nvme0n1 $W/sys/block/nvme0n1
cat > $W/in.sh <<'X'
# (inside: unshare -m -n) $1 the variables' folder, then the command
W=$WD; V=$1; shift
mount --bind $W/sys /sys; mount --bind $V /sys/firmware/efi/efivars
mkdir -p $W/run $W/persist/var/log $W/etcr; mount --bind $W/run /run; mkdir -p /run/rudolf; touch /run/rudolf/persistent
[ -d /persist ] || mkdir -p /persist; mount --bind $W/persist /persist
cp -a /etc $W/etc2 2>/dev/null; mount --bind $W/etc2 /etc; mkdir -p /etc/rudolf; echo /dev/nvme0n1p3 > /etc/rudolf/persist
touch /dev/nvme0n1 2>/dev/null || mount -t tmpfs none /dev; touch /dev/nvme0n1; mount --bind $W/disk /dev/nvme0n1
"$@"
X
inns() { WD=$W unshare -m -n sh $W/in.sh "$@"; }
dump() { python3 - "$1" <<'X'
import sys,os,struct
d=sys.argv[1]; G='8be4df61-93ca-11d2-aa0d-00e098032b8c'; out=[]
for n in sorted(os.listdir(d)):
    b=open(f'{d}/{n}','rb').read()[4:]; k=n.split('-')[0]
    if k in ('BootOrder','BootCurrent','BootNext'): out.append(k+'='+','.join('%04X'%x for x in struct.unpack('<%dH'%(len(b)//2),b)))
    elif k.startswith('Boot'):
        t=b[6:].decode('utf-16-le').split('\0')[0]; i=6+2*(len(t)+1); pl=struct.unpack('<H',b[4:6])[0]; p=b[i:i+pl]
        cd=p.find(bytes([4,2,24,0])); c=('CDROM(%d,0x%X,0x%X)'%struct.unpack('<IQQ',p[cd+4:cd+24])) if cd>=0 else ''
        out.append(f'{k} "{t}" {"active" if b[0]&1 else "inactive"} {c} {p.hex()}')
print('\n'.join(out))
X
}
setcur() { python3 -c "import struct,sys; open(sys.argv[1]+'/BootCurrent-8be4df61-93ca-11d2-aa0d-00e098032b8c','wb').write(struct.pack('<IH',6,int(sys.argv[2],16)))" "$1" "$2"; }
health() { grep '"k":"boot"' $W/persist/var/log/health.log 2>/dev/null; }
# the image's El Torito entry, as EDK2 names it
CDNEW=$(python3 - $BOOTISO <<'X'
import sys,struct
f=open(sys.argv[1],'rb'); f.seek(17*2048); bv=f.read(2048); f.seek(struct.unpack('<I',bv[0x47:0x4b])[0]*2048); c=f.read(2048)
for i in range(64):
    e=c[32*i:32*i+32]
    if e[0]==0x88 and i>1: print('CDROM(1,0x%X,0x%X)'%(struct.unpack('<I',e[8:12])[0]*4,struct.unpack('<H',e[6:8])[0]*4)); break
X
)
echo "the boot image's EFI image, as the firmware names it: $CDNEW (the Vivobook's entries: CDROM(1,0x52EC,0x1E000))"

# ---- 0. what the released rudolfd does with these entries, started from the USB stick (BootCurrent: the stick)
if [ -x "$RELEASED" ]; then
	cp -a $W/vars0 $W/vr
	inns $W/vr sh -c "(timeout 4 $RELEASED > $W/rel.log 2>&1 &); sleep 5" 2>/dev/null
	o=$(dump $W/vr); o0=$(dump $W/vars0)
	ok "0a. the released rudolfd, started from the USB stick: the \"Rudolf OS\" entry still names CDROM(1,0x52EC,0x1E000), which the drive no longer has (the computer still cannot start without the stick)" \
		"$([ "$o" = "$o0" ] && echo "$o" | grep -q '^Boot0001 "Rudolf OS" active CDROM(1,0x52EC,0x1E000)' && echo 1)" "$o $(cat $W/rel.log | head -5)"
	r=$(inns $W/vr sh -c "$RELEASED --health boot 'test'; echo rc=\$?" 2>&1)
	ok "0b. the released rudolfd refuses health events of kind \"boot\" (espcheck's): exit 2, nothing in System health" "$(echo "$r" | grep -q 'rc=2' && [ -z "$(health)" ] && echo 1)" "$r $(health)"
	rm -rf $W/persist/var/log/*
fi
# ---- 1. this rudolfd, the same start
cp -a $W/vars0 $W/v; rm -f $W/persist/var/log/health.log
o=$(inns $W/v $W/rudolfd --boot-entry start; echo rc=$?); d=$(dump $W/v)
ok "1. started from the USB stick: a new \"Rudolf OS\" entry (Boot0002) naming $CDNEW, written and read back, first; the old one kept second as \"Rudolf OS (previous)\"; \"Drive\" and Windows untouched" \
	"$(echo "$d" | grep -q "^Boot0002 \"Rudolf OS\" active $CDNEW" && echo "$d" | grep -q '^Boot0001 "Rudolf OS (previous)" active CDROM(1,0x52EC,0x1E000)' \
	&& echo "$d" | grep -q '^BootOrder=0002,0001,0003,0000$' && echo "$d" | grep -q '^Boot0003 "Drive" active CDROM(1,0x52EC,0x1E000)' && echo 1)" "$o $d"
h=$(health)
ok "1b. each change said in System health (kind boot): the new entry with its path, the previous one, the boot order" \
	"$(printf '%s\n' "$h" | grep -qF 'Boot0002 = \"Rudolf OS\" PciRoot(0x0)/Pci(0x2,0x2)/Pci(0x0,0x0)/NVMe(0x1,00-26-B7-38-22-72-47-D5)/'"$(echo "$CDNEW" | sed 's/CDROM(1,/CDROM(0x1,/')"'/\\EFI\\BOOT\\BOOTX64.EFI' \
	&& printf '%s\n' "$h" | grep -qF 'Boot0001 = \"Rudolf OS (previous)\"' && printf '%s\n' "$h" | grep -qF 'BootOrder = Boot0002,Boot0001,Boot0003,Boot0000' && [ "$(printf '%s\n' "$h" | wc -l)" = 3 ] && echo 1)" "$h"
cp -a $W/v $W/v1
# ---- 2. the next start through the new entry: the previous one goes
setcur $W/v 0002; : > $W/persist/var/log/health.log
o=$(inns $W/v $W/rudolfd --boot-entry start); d=$(dump $W/v)
ok "2. the next start through Boot0002: proven, the previous entry removed; the boot order 0002,0003,0000" \
	"$(! echo "$d" | grep -q '^Boot0001' && echo "$d" | grep -q '^BootOrder=0002,0003,0000$' && echo "$d" | grep -q "^Boot0002 \"Rudolf OS\" active $CDNEW" && health | grep -q 'Boot0001 removed' && echo 1)" "$d $(health)"
: > $W/persist/var/log/health.log; inns $W/v $W/rudolfd --boot-entry start >/dev/null; d2=$(dump $W/v)
ok "3. the start after: nothing written (nothing in System health)" "$([ "$d2" = "$d" ] && [ -z "$(health)" ] && echo 1)" "$(health)"
# ---- 4. a start through the previous entry instead: the new one did not work, it goes
cp -a $W/v1 $W/v4; setcur $W/v4 0001
o=$(inns $W/v4 $W/rudolfd --boot-entry start); d=$(dump $W/v4)
ok "4. a start through the previous entry instead (the new one did not start it): the new one removed, the previous one is \"Rudolf OS\" again, first" \
	"$(! echo "$d" | grep -q '^Boot0002' && echo "$d" | grep -q '^Boot0001 "Rudolf OS" active CDROM(1,0x52EC,0x1E000)' && echo "$d" | grep -q '^BootOrder=0001,0003,0000$' && echo 1)" "$d"
# ---- 5. check only: nothing changes; fix: the same as a start, with what it found
cp -a $W/vars0 $W/v5; o=$(inns $W/v5 $W/rudolfd --boot-entry check; echo rc=$?)
ok "5. check: says the entry names the old place and would be written again, changes nothing (exit 2)" \
	"$([ "$(dump $W/v5)" = "$(dump $W/vars0)" ] && echo "$o" | grep -q 'CDROM(0x1,0x52EC,0x1E000)' && echo "$o" | grep -q 'would be written again' && echo "$o" | grep -q 'rc=2' && echo 1)" "$o"
o=$(inns $W/v5 $W/rudolfd --boot-entry fix; echo rc=$?)
ok "5b. fix (Settings' check of the start-up): written, read back, said (exit 0)" "$(dump $W/v5 | grep -q "^Boot0002 \"Rudolf OS\" active $CDNEW" && echo "$o" | grep -q 'written and read back' && echo "$o" | grep -q 'rc=0' && echo 1)" "$o"
# ---- 6. a firmware that refuses the write: nothing else changes, the old entry stays first
cp -a $W/vars0 $W/v6; chmod 0555 $W/v6
o=$(inns $W/v6 sh -c "mount -o remount,bind,ro /sys/firmware/efi/efivars; $W/rudolfd --boot-entry fix; echo rc=\$?"); chmod 0755 $W/v6
ok "6. the firmware does not keep the new entry: nothing else changed (the old one stays first), said, exit 2" "$([ "$(dump $W/v6)" = "$(dump $W/vars0)" ] && echo "$o" | grep -q 'could not be written' && echo "$o" | grep -q 'rc=2' && echo 1)" "$o $(dump $W/v6)"
# ---- 7. a computer whose firmware made its own entry for the drive (OVMF): learned as before (1.4.2), no place in it
rm -rf $W/v7; mkdir $W/v7; PRE=$(cat $W/pre.hex)
python3 - $W/v7 $PRE <<'X'
import sys,struct
d=sys.argv[1]; G='8be4df61-93ca-11d2-aa0d-00e098032b8c'; p=bytes.fromhex(sys.argv[2])+bytes([0x7f,0xff,4,0])
open(f'{d}/Boot0005-{G}','wb').write(struct.pack('<IIH',7,1,len(p))+'UEFI NVMe Ctrl 1\0'.encode('utf-16-le')+p)
open(f'{d}/BootOrder-{G}','wb').write(struct.pack('<IH',7,5)); open(f'{d}/BootCurrent-{G}','wb').write(struct.pack('<IH',6,5))
X
inns $W/v7 $W/rudolfd --boot-entry start >/dev/null; d=$(dump $W/v7)
ok "7. a firmware's own entry for the drive started it: \"Rudolf OS\" takes its path (no CDROM), first" "$(echo "$d" | grep -q "^Boot0000 \"Rudolf OS\" active  ${PRE}7fff0400$" && echo "$d" | grep -q '^BootOrder=0000,0005$' && echo 1)" "$d"
# ---- 7b. a drive whose EUI-64 Linux leaves out (NVME_QUIRK_BOGUS_NID: no /sys/block/X/eui), the firmware naming it
#      with one: the firmware's own entry for the controller says it (the entry would name no drive otherwise)
mv $N/eui $W/eui.k; rm -rf $W/v7b; cp -a $W/v7 $W/v7b; for f in $W/v7b/Boot0000-*; do rm -f "$f"; done
python3 - $W/v7b <<'X'
import sys,struct
d=sys.argv[1]; G='8be4df61-93ca-11d2-aa0d-00e098032b8c'; open(f'{d}/BootOrder-{G}','wb').write(struct.pack('<IH',7,5))
X
inns $W/v7b $W/rudolfd --boot-entry start >/dev/null; d=$(dump $W/v7b); mv $W/eui.k $N/eui
ok "7b. no EUI-64 from Linux: taken from the firmware's own entry for the drive (NVMe(0x1,00-26-B7-...))" "$(echo "$d" | grep -q "^Boot0000 \"Rudolf OS\" active  ${PRE}7fff0400$" && echo 1)" "$d"
# ---- 8. System health takes kind "boot" now
r=$(inns $W/v7 sh -c "$W/rudolfd --health boot 'the start-up files test'; echo rc=\$?")
ok "8. rudolfd --health boot ...: kept in System health (espcheck's reports)" "$(echo "$r" | grep -q rc=0 && health | grep -q 'the start-up files test' && echo 1)" "$r"
ok "9. no EFI variable is deleted before it is written again (the write is in place, read back)" "$(! sed -n '/^static int efivar_set/,/^}/p' "$C" | grep -q unlink && sed -n '/^static int efivar_set/,/^}/p' "$C" | grep -q 'memcmp(rb, d, n)' && echo 1)" ""
echo "$P/$((P+F)) passed"; [ $F = 0 ]
