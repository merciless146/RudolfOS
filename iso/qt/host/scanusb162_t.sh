#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 34, step 3): older USB scanners through a small set of SANE's own drivers.
#   1. systools/mkscan.sh on a folder of its own: libsane, sane-airscan and the eight drivers, each driver's settings for
#      USB only (no network or SCSI lines; pixma's networking off), the list of drivers, SANE's USB scanners' ids; the size
#   2. rudolf-scan through SANE's own frames, against scanimage (SANE's own front end) on SANE's test driver, pixel for
#      pixel: colour, a three-pass scanner's three frames, 16 bits a sample, grey, line art, the grid; its feeder's pages
#   3. find: the USB scanners SANE's drivers know, from a stand-in sysfs (a CanoScan LiDE 220, an Epson Perfection), not a
#      mouse, and not the one ipp-usb already serves as eSCL (the same device); the drivers never look on the network
#      (no internet socket opened: strace)
#   4. scanusb: a stand-in sysfs and device nodes: only the scanners' nodes given to the account's user (its own, 0600)
#      and given back as they were; only users 1000-59999; a node made a link is not followed
#   5. the devices helper's scanusb (as rudolfd asks): its checks
# Needs gcc, zlib, libsane1 (with SANE's test driver), sane-utils (scanimage), sane-airscan, python3 with PIL; as root for
# 4 (device nodes). sh scanusb162_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; ST=$HERE/../../systools
MA=$(gcc -print-multiarch 2>/dev/null || echo x86_64-linux-gnu)
command -v scanimage >/dev/null && [ -e /usr/lib/$MA/sane/libsane-test.so.1 ] || { echo "SKIP: needs sane-utils and libsane1 (scanimage, SANE's test driver)"; exit 0; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3-pil"; exit 0; }
T=$(mktemp -d); chmod 755 $T; trap 'rm -rf "$T"' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 1500)"; F=$((F+1)); fi; }
. $ST/hardflags.sh 2>/dev/null
gcc -O2 $HARD -o $T/rudolf-scan $SYS/rudolf-scan.c -ldl -lm -Wl,-Bstatic -lz -Wl,-Bdynamic 2>$T/cc || { echo "FAIL: rudolf-scan does not build: $(cat $T/cc)"; exit 1; }
R=$T/rudolf-scan

# ---- 1. the base layer's files
mkdir -p $T/root; o=$(sh $ST/mkscan.sh $T/root / 2>&1); D=$T/root/usr/share/rudolf
nets=$(grep -lE '^[[:space:]]*(net|tcp|scsi)([[:space:]]|$)' $D/sane.d/*.conf 2>/dev/null)
ok "1. mkscan.sh: libsane, sane-airscan and the eight drivers; each driver's settings for USB only (no net, tcp or scsi lines; pixma's networking off); the drivers' list; SANE's USB scanners' ids (over 1000); under 3 MB ($o)" "$(echo "$o" | grep -q '8 drivers (pixma genesys epson2 epsonds plustek canon_lide70 xerox_mfp fujitsu)' && [ -f $T/root/usr/lib/$MA/libsane.so.1 ] && [ $(ls $T/root/usr/lib/$MA/sane/ | wc -l) = 9 ] && [ -z "$nets" ] && grep -qx 'networking=no' $D/sane.d/pixma.conf && [ "$(wc -l < $D/scan-backends)" = 8 ] && [ "$(wc -l < $D/scan-usb-ids)" -gt 1000 ] && grep -qx '04A9:190F' $D/scan-usb-ids && [ "$(echo "$o" | sed -n 's/^\([0-9]*\) KB.*/\1/p')" -lt 3072 ] && echo 1)" "$o $nets"
echo "::notice title=Scanning, step 3 (Part 34)::$o"

# ---- 2. SANE's own frames, against scanimage on SANE's test driver
mkdir -p $T/conf $T/sd; printf 'test\n' > $T/be; printf 'number_of_devices 1\n' > $T/conf/test.conf; printf 'test\n' > $T/sd/dll.conf; cp $T/conf/test.conf $T/sd/
E="env RS_BACKENDS=$T/be RS_SANECONF=$T/conf"
cmp_case() {   # NAME MODE RS_OPTS SCANIMAGE-ARGS...
	nm=$1; md=$2; op=$3; shift 3; mkdir -p $T/$nm
	o=$($E RS_OPTS="$op" $R scan sane:test:0 $T/$nm res=75 mode=$md)
	SANE_CONFIG_DIR=$T/sd scanimage -d test:0 --resolution 75 -l 0 -t 0 -x 200 -y 200 --format=pnm "$@" > $T/$nm/ref.pnm 2>$T/$nm/err
	python3 - "$T/$nm/page-1.png" "$T/$nm/ref.pnm" "$md" <<'PY'
import sys
from PIL import Image
a = Image.open(sys.argv[1]); b = open(sys.argv[2], 'rb').read()
# the PNM: P4 (1 bit, 1 black), P5/P6 with maxval 255 or 65535
import re
toks = []; i = 0
while len(toks) < (3 if b[:2] == b'P4' else 4):
    m = re.compile(rb'\s*(#[^\n]*\n\s*)*(\S+)').match(b, i); toks.append(m.group(2)); i = m.end()
kind = toks[0]; w, h = int(toks[1]), int(toks[2])
if kind == b'P4':
    data = b[len(b) - ((w + 7) // 8) * h:]; ref = Image.frombytes('1', (w, h), bytes(255 - x for x in data))
else:
    mx = int(toks[3]); data = b[len(b) - w * h * (3 if kind == b'P6' else 1) * (2 if mx > 255 else 1):]
    if mx > 255: data = data[0::2]   # (16 bits: the high byte, as rudolf-scan keeps)
    ref = Image.frombytes('RGB' if kind == b'P6' else 'L', (w, h), data)
mode = sys.argv[3]
A = a.convert('RGB' if mode == 'color' else 'L'); B = ref.convert('RGB' if mode == 'color' else 'L')
if A.size != B.size: print('bad size', A.size, B.size); sys.exit(1)
diff = sum(1 for p, q in zip(A.tobytes(), B.tobytes()) if p != q)
print(('ok' if diff == 0 else 'bad') + f' {A.size} {mode} {diff} pixels differ'); sys.exit(0 if diff == 0 else 1)
PY
}
c1=$(cmp_case color color 'test-picture=Color pattern' --mode Color --test-picture 'Color pattern')
c2=$(cmp_case three color 'test-picture=Color pattern;three-pass=1' --mode Color --test-picture 'Color pattern' --three-pass=yes)
c3=$(cmp_case deep color 'test-picture=Color pattern;depth=16' --mode Color --test-picture 'Color pattern' --depth 16)
c4=$(cmp_case grey grey 'test-picture=Grid' --mode Gray --test-picture Grid)
c5=$(cmp_case art grey 'test-picture=Grid;depth=1' --mode Gray --depth 1 --test-picture Grid)
ok "2. through SANE's own frames, pixel for pixel as scanimage gives them (SANE's test driver): colour, three-pass (three frames), 16 bits, grey, line art (its own 1-bit mode)" "$(printf '%s\n' "$c1" "$c2" "$c3" "$c4" "$c5" | grep -c '^ok' | grep -qx 5 && echo 1)" "$c1 | $c2 | $c3 | $c4 | $c5"
mkdir -p $T/adf; o=$($E $R scan sane:test:0 $T/adf source=adf res=75 first=5)
ok "...its feeder: every page in it (the test driver's ten), numbered on from first=" "$(echo "$o" | grep -q '"pages":\["page-5.png",.*"page-14.png"\]' && [ $(ls $T/adf | wc -l) = 10 ] && echo 1)" "$o"
mkdir -p $T/cx; $E RS_OPTS='test-picture=Color pattern;read-delay=1;read-delay-duration=200000' $R scan sane:test:0 $T/cx res=600 > $T/cx.out & P=$!; timeout 1.5 tail -f /dev/null; kill -TERM $P 2>/dev/null; wait $P
ok "...stopped while it reads (as rudolfd's scan/cancel): said cancelled, no page left" "$(grep -q '"code":"cancelled"' $T/cx.out && [ -z "$(ls $T/cx)" ] && echo 1)" "$(cat $T/cx.out; ls $T/cx)"

# ---- 3. find: SANE's USB scanners from a stand-in sysfs
U=$T/sys/bus/usb/devices; mkusb() { mkdir -p $U/$1; printf '%s\n' "$2" > $U/$1/idVendor; printf '%s\n' "$3" > $U/$1/idProduct; echo $4 > $U/$1/busnum; echo $5 > $U/$1/devnum; printf '%s\n' "$6" > $U/$1/manufacturer; printf '%s\n' "$7" > $U/$1/product; }
mkusb 1-1 04a9 190f 1 5 Canon 'CanoScan LiDE 220'; mkusb 1-2 04b8 0104 1 6 EPSON 'Perfection 1200U'; mkusb 1-3 046d c077 1 7 Logitech 'USB Optical Mouse'; mkusb 2-1 04a9 176d 2 3 Canon 'MG2500 series'
mkdir -p $U/1-1:1.0; printf 'ipp\tCanon MG2500 series\tipp://127.0.0.1:1/ipp/print\tMFG:Canon;\t2-1:1.0\n' > $T/usbp
o=$(env RS_SYS=$T/sys RS_IDS=$D/scan-usb-ids RS_USB=/dev/null RS_ONLY=usb $R find)
ok "3. find: the USB scanners SANE's drivers know (CanoScan LiDE 220, the Epson), by their ids and their own names; not a mouse" "$(echo "$o" | grep -q '"id":"sane:usb:04A9:190F","name":"USB: Canon CanoScan LiDE 220","model":"Canon CanoScan LiDE 220","via":"usb","proto":"driver"' && echo "$o" | grep -q '"id":"sane:usb:04B8:0104","name":"USB: EPSON Perfection 1200U"' && ! echo "$o" | grep -qi 'mouse\|046D' && echo 1)" "$o"
# (2-1 is in ipp-usb's list: had its eSCL answered, it would be the eSCL entry; here its port does not answer, so the driver's is listed)
o=$(env RS_SYS=$T/sys RS_IDS=$D/scan-usb-ids RS_USB=$T/usbp RS_ONLY=usb $R find)
ok "...a printer-scanner ipp-usb serves but whose eSCL does not answer: still offered by its driver" "$(echo "$o" | grep -q 'sane:usb:04A9:176D' && echo 1)" "$o"
o=$(env RS_SYS=$T/sys RS_IDS=$D/scan-usb-ids RS_BACKENDS=$D/scan-backends RS_SANECONF=$D/sane.d $R caps sane:usb:04A9:190F); o2=$(env RS_SYS=$T/sys RS_IDS=$D/scan-usb-ids $R caps sane:usb:0000:0001)
ok "...a stand-in scanner no driver can open: said plainly (no driver answers, connect it to the Wi-Fi if it can); one not plugged in: said" "$(echo "$o" | grep -q '"code":"noscanner","error":"No driver of this Rudolf OS answers for this scanner (04A9:190F)' && echo "$o2" | grep -q 'not plugged in any more' && echo 1)" "$o | $o2"
if command -v strace >/dev/null; then
	env RS_SYS=$T/sys RS_IDS=$D/scan-usb-ids RS_BACKENDS=$D/scan-backends RS_SANECONF=$D/sane.d strace -f -qq -e trace=socket -o $T/st $R caps sane:usb:04A9:190F >/dev/null 2>&1
	ok "...the eight drivers loaded with their settings open no internet socket (strace): they look only at USB" "$(grep -q 'socket(' $T/st && ! grep -E 'socket\(AF_INET6?,' $T/st | grep -q . && echo 1; [ -s $T/st ] || echo 1)" "$(grep socket $T/st | head)"
else ok "...(strace is not here: the drivers' network not checked)" 1; fi

# ---- 4. scanusb: the device nodes given and given back
if [ "$(id -u)" = 0 ]; then
	SU=$SYS/root/usr/lib/rudolf/scanusb; DV=$T/dev; mkdir -p $DV/001 $DV/002 $T/run
	for n in 001/005 001/006 001/007 002/003; do mknod $DV/$n c 189 $((${n#*/} + 0)); chown 0:0 $DV/$n; chmod 664 $DV/$n; done; chown 0:1000 $DV/001/006; chmod 660 $DV/001/006
	SE="env RUDOLF_SCANUSB_SYS=$T/sys RUDOLF_SCANUSB_DEV=$DV RUDOLF_SCANUSB_IDS=$D/scan-usb-ids RUDOLF_RUN=$T/run"
	l=$($SE sh $SU list | sort); g=$($SE sh $SU allow 46001); st=$(stat -c '%n %u:%g %a' $DV/001/005 $DV/001/006 $DV/001/007 $DV/002/003)
	ok "4. scanusb allow 46001: the scanners' nodes (the LiDE 220, the Epson, the MG2500) given to it, its own and 0600; the mouse's left alone" "$(echo "$l" | tr '\n' ';' | grep -qx '04A9:176D 2 3;04A9:190F 1 5;04B8:0104 1 6;' && echo "$st" | grep -q "001/005 46001:46001 600" && echo "$st" | grep -q "001/006 46001:46001 600" && echo "$st" | grep -q "002/003 46001:46001 600" && echo "$st" | grep -q "001/007 0:0 664" && [ "$(echo "$g" | wc -l)" = 3 ] && echo 1)" "$l | $g | $st"
	$SE sh $SU done 46001; st=$(stat -c '%n %u:%g %a' $DV/001/005 $DV/001/006 $DV/002/003)
	ok "...scanusb done 46001: each given back as it was (the Epson's 0:1000 660 too), nothing kept" "$(echo "$st" | grep -q "001/005 0:0 664" && echo "$st" | grep -q "001/006 0:1000 660" && echo "$st" | grep -q "002/003 0:0 664" && [ -z "$(ls $T/run/scanusb)" ] && echo 1)" "$st $(ls $T/run/scanusb)"
	bad=$(for u in 0 999 60000 abc 1000x ''; do $SE sh $SU allow "$u" >/dev/null 2>&1 && echo "took '$u'"; done); st=$(stat -c '%u' $DV/001/005)
	ok "...only users 1000-59999 (not root, a system user, a number past them, letters)" "$([ -z "$bad" ] && [ "$st" = 0 ] && echo 1)" "$bad"
	rm -f $DV/001/005; ln -s /etc/hostname $DV/001/005; $SE sh $SU allow 46001 >/dev/null; s2=$(stat -c '%u' /etc/hostname); $SE sh $SU done 46001
	ok "...a node made a link is not followed (the file it points at stays root's)" "$([ "$s2" = 0 ] && echo 1)" "$s2"
	# ---- 5. the devices helper, as rudolfd asks it
	gcc -O2 -DRR_TEST -o $T/devices $SYS/rudolf-devices.c 2>$T/dcc && mkdir -p $T/lib && cat > $T/lib/scanusb <<'E'
#!/bin/sh
echo "scanusb $*"
E
	chmod 755 $T/lib/scanusb
	o=$(RR_TEST_LIB=$T/lib $T/devices scanusb allow 46001 2>&1); o2=$(RR_TEST_LIB=$T/lib $T/devices scanusb done 1000 2>&1); o3=$(for a in "allow 0" "allow 999" "allow 60000" "allow 4600x" "give 46001" "allow"; do RR_TEST_LIB=$T/lib $T/devices scanusb $a 2>&1; done)
	ok "5. the devices helper: scanusb allow|done UID run as asked (1000-59999), anything else refused" "$([ "$o" = 'scanusb allow 46001' ] && [ "$o2" = 'scanusb done 1000' ] && [ "$(echo "$o3" | grep -c '^refused')" = 6 ] && ! echo "$o3" | grep -q '^scanusb' && echo 1)" "$o | $o2 | $o3 | $(cat $T/dcc 2>/dev/null)"
else echo "SKIP 4-5: as root (device nodes)"; fi
echo "$((N-F))/$N passed"; [ $F = 0 ]
