#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md Part 8): USB printers. /usr/lib/rudolf/usbprint on a stand-in sysfs: an IPP over USB printer
# (interfaces 7/1/2 and 7/1/4: usblp let go of, ipp-usb's stand-in started and its address recorded), a plain PostScript
# one, a PCL-only one ("needs a driver"), a PDF one, and a USB stick (not a printer); without the extension; unplugged.
# rudolf-print (built from sys/rudolf-print.c): find listing them as "USB: name", usb, info, a job to the plain kinds
# written to stand-in /dev/usb/lpN (a PDF as it is, copies, PostScript through a stand-in pdftops with the pages), the
# others refused saying why; rudolfd's check of a USB printer's address; the udev rules and mdev's lines, the kernel's
# usblp, the self-test's line.   sh usbprint60_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../..; U=$I/sys/root/usr/lib/rudolf/usbprint
T=$(mktemp -d /tmp/usbprint60.XXXXXX); trap 'p=$(cat $T/ipp.pid 2>/dev/null); [ -n "$p" ] && kill $p 2>/dev/null; [ -n "$FP" ] && kill $FP 2>/dev/null; rm -rf $T' EXIT
# the printer behind ipp-usb: a pretend IPP Everywhere printer (qt/fakeprinter.py), with an mDNS responder for the network's
python3 $HERE/../fakeprinter.py $T/jobs 8752 pdf 8753 >/dev/null 2>&1 & FP=$!
gcc -O2 -o $T/rp $I/sys/rudolf-print.c -ldl 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
N=0; F=0; ok() { N=$((N+1)); case "$2" in *[!1]*|'') r=0;; *) r=1;; esac; if [ $r = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
S=$T/sys/bus/usb; mkdir -p $S/drivers/usblp $T/run $T/state $T/devusb
# (as sysfs: an interface is under its device, and linked beside it in bus/usb/devices)
dev() { d=$S/devices/$1; mkdir -p $d; echo $2 > $d/idVendor; echo $3 > $d/idProduct; [ -n "$4" ] && echo "$4" > $d/manufacturer; [ -n "$5" ] && echo "$5" > $d/product; return 0; }
ifc() { i=$S/devices/${1%%:*}/$1; mkdir -p $i; ln -s ${1%%:*}/$1 $S/devices/$1; echo $2 > $i/bInterfaceClass; echo $3 > $i/bInterfaceProtocol; [ -n "$4" ] && printf '%s' "$4" > $i/ieee1284_id; [ -n "$5" ] && mkdir -p $i/usbmisc/$5; return 0; }
dev 1-1 03f0 2c17 HP "OfficeJet Pro 9010"; ifc 1-1:1.0 07 02 "MFG:HP;MDL:OfficeJet Pro 9010;CMD:PCL3GUI,PJL,URF,PWGRaster;" lp9; ifc 1-1:1.1 07 04; ifc 1-1:1.2 07 04
ln -s ../../../drivers/usblp $S/devices/1-1/1-1:1.0/driver
dev 1-2 04f9 0068; ifc 1-2:1.0 07 02 "MFG:Brother;CMD:PJL,PCL,PCLXL,POSTSCRIPT;MDL:HL-L5100DN;CLS:PRINTER;" lp0
dev 1-3 03f0 2b17; ifc 1-3:1.0 07 02 "MANUFACTURER:Hewlett-Packard;MODEL:HP LaserJet 1020;COMMAND SET:ZJS, PCL;" lp1
dev 1-5 1234 5678 Example "PDF Printer"; ifc 1-5:1.0 07 01 "MFG:Example;MDL:PDF Printer;CMD:PWG, PDF ;" lp2
dev 1-4 0781 5581 SanDisk "Cruzer"; ifc 1-4:1.0 08 50
# ipp-usb's stand-in: says how it was started, then gives the printer its port (as ipp-usb's state file does)
mkdir -p $T/bin; printf '#!/bin/sh\necho "$*" >> %s/ipp.args\necho $$ > %s/ipp.pid\nsleep 0.3\nprintf "[device]\\n  http-port = 8752\\n" > %s/state/03f0-2c17-CN12345-HP.state\nwhile :; do sleep 1; done\n' $T $T $T > $T/bin/ipp-usb; chmod 755 $T/bin/ipp-usb
export RUDOLF_USBPRINT_SYS=$T/sys RUDOLF_RUN=$T/run RUDOLF_IPPUSB=$T/bin/ipp-usb RUDOLF_IPPUSB_STATE=$T/state RUDOLF_IPPUSB_WAIT=40
sh $U scan; L=$(cat $T/run/usb-printers 2>&1)
ok "1. an IPP over USB printer: ipp-usb started (its udev mode), its local address recorded" "$(echo "$L" | grep -qx 'ipp	HP OfficeJet Pro 9010	ipp://127.0.0.1:8752/ipp/print	MFG:HP;MDL:OfficeJet Pro 9010;CMD:PCL3GUI,PJL,URF,PWGRaster;	1-1' && echo 1)$(grep -qx udev $T/ipp.args && echo 1)" "$L $(cat $T/ipp.args 2>&1)"
ok "...usblp let go of its interfaces (ipp-usb needs them)" "$(grep -qx '1-1:1.0' $S/drivers/usblp/unbind && echo 1)" "$(cat $S/drivers/usblp/unbind 2>&1)"
ok "2. a plain printer that reads PostScript: its /dev/usb/lpN, named from its device ID" "$(echo "$L" | grep -q '^postscript	Brother HL-L5100DN	/dev/usb/lp0	MFG:Brother;' && echo 1)" "$L"
ok "3. a PCL-only one (the long names of the device ID): needs a driver" "$(echo "$L" | grep -q '^driver	Hewlett-Packard HP LaserJet 1020	/dev/usb/lp1	' && echo 1)" "$L"
ok "...a PDF one (spaces in its command set): pdf" "$(echo "$L" | grep -q '^pdf	Example PDF Printer	/dev/usb/lp2	' && echo 1)" "$L"
ok "...a USB stick is not a printer" "$(echo "$L" | grep -q 1-4 && echo 0 || echo 1)$([ $(echo "$L" | wc -l) = 4 ] && echo 1)" "$L"
ok "...each one plugged in said in the log" "$(grep -c 'plugged in:' $T/run/usbprint.log | grep -qx 4 && echo 1)" "$(cat $T/run/usbprint.log)"
sh $U scan; ok "...looked at again: nothing new said, ipp-usb not started twice" "$(grep -c 'plugged in:' $T/run/usbprint.log | grep -qx 4 && echo 1)$([ $(wc -l < $T/ipp.args) = 1 ] && echo 1)" "$(cat $T/ipp.args)"
# rudolf-print
printf '%%PDF-1.4 test document\n' > $T/doc.pdf
export RP_USB=$T/run/usb-printers RP_USBDEV=$T/devusb RP_FILTERS=$T/filters RP_MDNS=127.0.0.1:8753
o=$($T/rp find); ok "4. rudolf-print find: the USB printers with the network's, as \"USB: name\"" "$(echo "$o" | python3 -c 'import json,sys;j=json.load(sys.stdin);print(1 if len(j["printers"])==5 and not j["printers"][0]["name"].startswith("USB") else 0)')$(has "$o" '{"name":"USB: HP OfficeJet Pro 9010","uri":"ipp://127.0.0.1:8752/ipp/print","model":"OfficeJet Pro 9010","location":"USB","usb":"ipp","kind":"IPP over USB"}')$(has "$o" '"name":"USB: Brother HL-L5100DN","uri":"usb://lp0"')$(has "$o" '"uri":"usb://lp1","model":"HP LaserJet 1020","location":"USB","usb":"driver","kind":"needs a driver"')" "$o"
o=$($T/rp info ipp://127.0.0.1:8752/ipp/print); o2=$($T/rp send ipp://127.0.0.1:8752/ipp/print $T/doc.pdf application/pdf)
ok "...the IPP over USB printer (through ipp-usb's address) is asked and printed to as a network printer" "$(has "$o" '"ok":true')$(has "$o" 'application/pdf')$(has "$o2" '"ok":true')$([ -s $T/jobs/job-1.pdf ] && echo 1)" "$o $o2"
o=$($T/rp usb); ok "...rudolf-print usb (Settings › Devices): each one's kind and whether it can print now" "$(has "$o" '"usb":"ipp","kind":"IPP over USB","ready":true')$(has "$o" '"usb":"postscript","kind":"PostScript","ready":false')$(has "$o" '"usb":"pdf","kind":"PDF","ready":true')$(has "$o" '"usb":"driver","kind":"needs a driver","ready":false')" "$o"
o=$($T/rp info usb://lp0); ok "5. a PostScript printer without the extension: info says to install it" "$(has "$o" '"ok":false')$(has "$o" 'reads PostScript: install the USB printers extension')" "$o"
mkdir -p $T/filters; printf '#!/bin/sh\n{ echo "%%!PS-Adobe-3.0"; echo "pdftops $*"; } > "$(eval echo \\${$#})"\n' > $T/filters/pdftops; chmod 755 $T/filters/pdftops
o=$($T/rp info usb://lp0); ok "...with it: a PDF is taken (made into PostScript), pages can be picked" "$(has "$o" '"ok":true')$(has "$o" '"formats":["application/pdf"]')$(has "$o" '"pageRanges":[true]')" "$o"
printf '%%PDF-1.4 test document\n' > $T/doc.pdf; : > $T/devusb/lp0; : > $T/devusb/lp1; : > $T/devusb/lp2
o=$($T/rp send usb://lp0 $T/doc.pdf application/pdf pages=2-3); ok "6. a job to it: PostScript from pdftops (the pages asked for) written to its /dev/usb/lp0" "$(has "$o" '"ok":true')$(head -1 $T/devusb/lp0 | grep -q '^%!PS-Adobe' && echo 1)$(grep -q "pdftops -q -f 2 -l 3 $T/doc.pdf" $T/devusb/lp0 && echo 1)" "$o $(cat $T/devusb/lp0)"
o=$($T/rp send usb://lp2 $T/doc.pdf application/pdf copies=2); ok "7. a PDF printer: the PDF as it is, once per copy" "$(has "$o" '"ok":true')$([ "$(cat $T/devusb/lp2)" = "$(cat $T/doc.pdf $T/doc.pdf)" ] && echo 1)" "$o"
: > $T/devusb/lp2; o=$($T/rp send usb://lp2 $T/doc.pdf application/pdf pages=1-1); ok "...pages asked for without the extension: all of them, and said" "$(has "$o" 'all the pages (picking pages needs the USB printers extension)')$([ "$(cat $T/devusb/lp2)" = "$(cat $T/doc.pdf)" ] && echo 1)" "$o"
o=$($T/rp send usb://lp1 $T/doc.pdf application/pdf); o2=$($T/rp info usb://lp1)
ok "8. one that needs a driver: no job, said; info has no format the dialog can send (it says: needs a driver)" "$(has "$o" '"ok":false')$(has "$o" 'needs a driver')$([ ! -s $T/devusb/lp1 ] && echo 1)$(has "$o2" '"formats":[]')" "$o $o2"
o=$($T/rp send usb://lp2 $T/doc.pdf image/pwg-raster); ok "...a raster job to a plain printer is refused" "$(has "$o" 'cannot take this kind of document')" "$o"
rm -f $T/devusb/lp2; o=$($T/rp send usb://lp2 $T/doc.pdf application/pdf); ok "...the printer gone from /dev: said" "$(has "$o" 'does not answer')" "$o"
o=$($T/rp send usb://lp7 $T/doc.pdf application/pdf); ok "...one not plugged in: said" "$(has "$o" 'not plugged in')" "$o"
# without the extension, then unplugged
p=$(cat $T/ipp.pid); kill $p; rm -f $T/ipp.pid $T/state/*; RUDOLF_IPPUSB=$T/none sh $U scan; L=$(cat $T/run/usb-printers)
ok "9. without the USB printers extension (no ipp-usb): the IPP over USB printer listed, with no address" "$(echo "$L" | grep -q '^ipp	HP OfficeJet Pro 9010		MFG:HP' && echo 1)" "$L"
o=$($T/rp find); o2=$($T/rp info usb://ipp/1-1); ok "...the print dialog lists it, and info says to install the extension" "$(has "$o" '"uri":"usb://ipp/1-1"')$(has "$o" '"kind":"IPP over USB (needs the USB printers extension)"')$(has "$o2" 'prints over USB with IPP: install the USB printers extension')" "$o $o2"
sh $U scan; rm -rf $S/devices/1-1 $S/devices/1-1:1.*; sleep 0.5; sh $U scan; L=$(cat $T/run/usb-printers); sleep 0.3
ok "10. unplugged: forgotten, said; ipp-usb not running with no IPP over USB printer left" "$(echo "$L" | grep -q OfficeJet && echo 0 || echo 1)$(grep -q 'unplugged: HP OfficeJet Pro 9010' $T/run/usbprint.log && echo 1)$(pgrep -f "$T/bin/ipp-usb" >/dev/null && echo 0 || echo 1)" "$L $(cat $T/run/usbprint.log)"
# rudolfd's check of the address
gcc -O1 -w -o $T/uri $HERE/fixtures/printuri.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build of printuri"; exit 1; }
o=$($T/uri usb://lp0 usb://ipp/1-1.2 "usb://../x" "usb://lp0'x" usb://a:b ipp://192.168.1.2/ipp/print http://x/)
ok "11. rudolfd: a USB printer's address is accepted (usb://lpN, usb://KIND/SYSNAME); .., quotes and ports are not" "$([ "$(echo "$o" | cut -c1 | tr -d '\n')" = 1100010 ] && echo 1)" "$o"
# the devices' rules, the kernel, the self-test
RU=$I/sys/root/usr/lib/udev/rules.d/90-rudolf.rules
ok "12. udev runs usbprint for a printer's interface (also when it goes) and for usblp's devices; mdev too (the fallback)" "$(grep -q 'ENV{INTERFACE}=="7/1/\*", ACTION=="add|remove|bind", RUN+="/usr/lib/rudolf/udev-run /usr/lib/rudolf/usbprint hotplug"' $RU && grep -q 'SUBSYSTEM=="usbmisc", KERNEL=="lp\[0-9\]\*".*usbprint hotplug' $RU && grep -q '^-$INTERFACE=7/1/\[0-9\]+ 0:0 660 \*/usr/lib/rudolf/usbprint hotplug' $I/sys/root/etc/mdev.conf && grep -q '^-usb/lp\[0-9\]+ .*usbprint hotplug' $I/sys/root/etc/mdev.conf && [ -x $U ] && echo 1)"
ok "...before mdev's catch-all line (a module's modalias), so both run" "$(awk '/usbprint hotplug/{u=NR} /^\$MODALIAS=/{m=NR} END{print (u && m && u < m) ? 1 : 0}' $I/sys/root/etc/mdev.conf)"
ok "...the kernel's usblp is a module, checked (x86 and ARM64)" "$(grep -q '^m USB_PRINTER$' $I/kconfig.sh && grep -q 'for o in $WWANM USB_PRINTER[ A-Z_$]*; do' $I/kconfig.sh && grep -q '^m USB_PRINTER$' $I/kconfig-arm64.sh && grep -q 'USB_PRINTER[ A-Z]*; do grep -q' $I/kconfig-arm64.sh && echo 1)"
printf 'ipp\tHP X\t\t\t1-1\npdf\tExample PDF Printer\t/dev/usb/lp2\t\t1-5\n' > $T/run/usb-printers
ST=$I/sys/root/usr/lib/rudolf/selftest; printf '#!/bin/sh\nexit 0\n' > $T/bin/rpok; chmod 755 $T/bin/rpok
o=$(ST_RUN=$T/run ST_PERSIST=$T/nop ST_SYS=$T/nop ST_PROC=$T/nop ST_PRINT=$T/bin/rpok sh $ST 2>/dev/null | grep ' usb-printer ')
ok "13. the self-test: each USB printer and its kind; one IPP over USB with no address fails (the extension)" "$(has "$o" 'FAIL usb-printer - an IPP over USB printer has no address')" "$o"
printf 'ipp\tHP X\tipp://127.0.0.1:60000/ipp/print\t\t1-1\npdf\tExample PDF Printer\t/dev/usb/lp2\t\t1-5\n' > $T/run/usb-printers
o=$(ST_RUN=$T/run ST_PERSIST=$T/nop ST_SYS=$T/nop ST_PROC=$T/nop ST_PRINT=$T/bin/rpok sh $ST 2>/dev/null | grep ' usb-printer ')
ok "...ipp-usb answering: passes, naming them" "$([ "$o" = "PASS usb-printer - HP X (ipp), Example PDF Printer (pdf)" ] && echo 1)" "$o"
o=$(ST_RUN=$T/run ST_PERSIST=$T/nop ST_SYS=$T/nop ST_PROC=$T/nop ST_PRINT=/bin/false sh $ST 2>/dev/null | grep ' usb-printer ')
ok "...ipp-usb not answering: fails" "$(has "$o" 'FAIL usb-printer - ipp-usb does not answer at ipp://127.0.0.1:60000/ipp/print')" "$o"
rm $T/run/usb-printers; o=$(ST_RUN=$T/run ST_PERSIST=$T/nop ST_SYS=$T/nop ST_PROC=$T/nop sh $ST 2>/dev/null | grep ' usb-printer ')
ok "...none plugged in: skipped" "$(has "$o" 'SKIP usb-printer - no USB printer plugged in')" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
