#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 9): the print queue (rudolf-printq) with CUPS's own clients, as Linux apps print, and
# rudolfd's requests, against virtual IPP Everywhere printers (CUPS's ippeveprinter: one that takes PDF, one that takes
# only PWG and Apple raster; the repository's fakeprinter.py instead when ippeveprinter is not installed, said so).
#   - the queue started as root becomes its own user (with group lp), its socket 0666, its spool its own 0700, and it
#     listens on no network port;
#   - lp, lpstat and cancel (cups-client) through CUPS_SERVER=the socket: the printers, a PDF printed to the PDF printer
#     (the printer's file read back: its text), the same PDF to the raster printer (made into PWG raster here: the
#     printer's file decoded: a Letter page at 300 dpi with the text's ink on it), monochrome as asked;
#   - GTK's print dialog code (Gtk.PrintOperation, as GTK apps print) on both printers, when PyGObject is there;
#   - two accounts' users: each prints; the other's job is shown without its name and cannot be cancelled (cancel,
#     cancel -a, rudolfd's request with --as), its own can; root as the user it says, held to that user's jobs; a user
#     saying it is another (X-Rudolf-As) is still itself;
#   - rudolfd's submit (a desktop job with its options), the printer it used remembered; PostScript and other kinds
#     refused saying why; a printer that does not answer: the job fails with the reason, the next one goes on.
#   sudo sh printq161_t.sh       (root: it makes three users and removes them; RPQT_PRELOAD for ippeveprinter on a
#                                 host without IPv6)
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../..
[ "$(id -u)" = 0 ] || { echo "SKIP printq161_t.sh: needs root (it makes test users)"; exit 0; }
command -v lp >/dev/null && command -v lpstat >/dev/null || { echo "FAIL cups-client (lp, lpstat, cancel) is not installed"; exit 1; }
T=$(mktemp -d /tmp/printq161.XXXXXX); chmod 755 $T
PIDS=; cleanup() { for p in $PIDS; do kill $p 2>/dev/null; done; for u in rpqa rpqb rpqq; do userdel $u 2>/dev/null; done; groupdel rpqq 2>/dev/null; rm -rf $T; }
trap cleanup EXIT
N=0; F=0; ok() { N=$((N+1)); case "$2" in *[!1]*|'') r=0;; *) r=1;; esac; if [ $r = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
. $I/systools/hardflags.sh
gcc -O2 $HARD -o $T/rudolf-print $I/sys/rudolf-print.c -ldl 2>$T/cc && gcc -O2 $HARD -o $T/rudolf-printq $I/sys/rudolf-printq.c -lz 2>>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
chmod 755 $T/rudolf-print $T/rudolf-printq
# the users: the queue's (as "print" on Rudolf OS, with group lp) and two accounts'
for u in rpqq rpqa rpqb; do userdel $u 2>/dev/null; useradd -M -d /nonexistent -s /bin/false $u || { echo "FAIL useradd $u"; exit 1; }; done
usermod -a -G lp rpqq 2>/dev/null
# the printers
PY=python3; for p in /usr/bin/python3.12 /usr/bin/python3; do [ -x $p ] && $p -c 'import gi' 2>/dev/null && PY=$p && break; done
mkdir -p $T/pdfjobs $T/rasjobs
if command -v ippeveprinter >/dev/null; then KIND=ippeveprinter
	env ${RPQT_PRELOAD:+LD_PRELOAD=$RPQT_PRELOAD} ippeveprinter -p 8761 -n localhost -d $T/pdfjobs -k -f application/pdf,image/pwg-raster -F application/pdf -m "PDF Printer" "Test PDF" > $T/ipp1.log 2>&1 & PIDS="$PIDS $!"
	env ${RPQT_PRELOAD:+LD_PRELOAD=$RPQT_PRELOAD} ippeveprinter -p 8762 -n localhost -d $T/rasjobs -k -f image/pwg-raster,image/urf -F image/pwg-raster -m "Raster Printer" "Test Raster" > $T/ipp2.log 2>&1 & PIDS="$PIDS $!"
else KIND=fakeprinter.py; echo "NOTE ippeveprinter (cups-ipp-utils) is not installed: the repository's fakeprinter.py stands in"
	$PY $HERE/../fakeprinter.py $T/pdfjobs 8761 pdf > $T/ipp1.log 2>&1 & PIDS="$PIDS $!"
	$PY $HERE/../fakeprinter.py $T/rasjobs 8762 raster > $T/ipp2.log 2>&1 & PIDS="$PIDS $!"
fi
# (a printer that takes the connection and never answers: its job waits, the next one behind it)
$PY -c "import socket,time;s=socket.socket();s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);s.bind(('127.0.0.1',8763));s.listen(50);time.sleep(600)" & PIDS="$PIDS $!"
for i in $(seq 1 50); do $T/rudolf-print info ipp://localhost:8761/ipp/print 2>/dev/null | grep -q '"ok":true' && $T/rudolf-print info ipp://localhost:8762/ipp/print 2>/dev/null | grep -q '"ok":true' && break; timeout 0.2 tail -f /dev/null; done
if [ $KIND = ippeveprinter ] && ! $T/rudolf-print info ipp://localhost:8762/ipp/print 2>/dev/null | grep -q '"ok":true'; then
	KIND=fakeprinter.py; echo "NOTE ippeveprinter did not answer ($(tail -1 $T/ipp1.log)): the repository's fakeprinter.py stands in"
	$PY $HERE/../fakeprinter.py $T/pdfjobs 8761 pdf > $T/ipp1.log 2>&1 & PIDS="$PIDS $!"; $PY $HERE/../fakeprinter.py $T/rasjobs 8762 raster > $T/ipp2.log 2>&1 & PIDS="$PIDS $!"
	for i in $(seq 1 50); do $T/rudolf-print info ipp://localhost:8762/ipp/print 2>/dev/null | grep -q '"ok":true' && break; timeout 0.2 tail -f /dev/null; done
fi
ok "0. the virtual printers answer ($KIND)" "$($T/rudolf-print info ipp://localhost:8762/ipp/print | grep -q 'image/pwg-raster' && echo 1)" "$(cat $T/ipp1.log $T/ipp2.log | tail -5)"
# the queue: started as root, as rudolf-init starts it
printf 'ipp://localhost:8761/ipp/print\tOffice PDF printer\nipp://localhost:8762/ipp/print\tHall raster printer\nipp://127.0.0.1:8763/ipp/print\tSilent printer\n' > $T/known
export RPQ_SOCK=$T/cups/cups.sock RPQ_SPOOL=$T/spool RPQ_PRINT=$T/rudolf-print RPQ_KNOWN=$T/known RPQ_POPPLER=$(dirname "$(command -v pdftocairo || echo /nonexistent/x)")
RPQ_USER=rpqq $T/rudolf-printq serve > $T/q.log 2>&1 & QP=$!; PIDS="$PIDS $QP"
for i in $(seq 1 50); do [ -S $RPQ_SOCK ] && break; timeout 0.1 tail -f /dev/null; done; timeout 1 tail -f /dev/null
ok "1. the queue runs as its own user (not root), with group lp" "$([ "$(stat -c %U /proc/$QP)" = rpqq ] && echo 1)$(grep -q "^Groups:.* $(getent group lp | cut -d: -f3)\b" /proc/$QP/status && echo 1)$(grep -q 'NoNewPrivs:.1' /proc/$QP/status && echo 1)" "$(grep -E 'Uid|Gid|Groups|NoNew' /proc/$QP/status) $(cat $T/q.log)"
ok "...its socket 0666, its spool its own 0700" "$([ "$(stat -c %a $RPQ_SOCK)" = 666 ] && echo 1)$([ "$(stat -c '%U %a' $T/spool)" = 'rpqq 700' ] && echo 1)" "$(ls -la $T/cups $T)"
INO=$(for f in /proc/$QP/fd/*; do readlink $f; done | sed -n 's/^socket:\[\([0-9]*\)\]/\1/p')
NET=$(for i in $INO; do grep -h " $i " /proc/net/tcp /proc/net/tcp6 /proc/net/udp /proc/net/udp6 2>/dev/null; done)
ok "...and listens on no network port (its sockets: the one in the file system only)" "$([ -z "$NET" ] && echo 1)$([ -n "$INO" ] && echo 1)" "$NET"
export CUPS_SERVER=$RPQ_SOCK
as() { u=$1; shift; su -s /bin/sh $u -c "CUPS_SERVER=$RPQ_SOCK $*"; }
L=$(as rpqa lpstat -p -d 2>&1)
ok "2. lpstat: the printers, as CUPS's queues" "$(has "$L" 'printer Office_PDF_printer is idle')$(has "$L" 'printer Hall_raster_printer')$(has "$L" 'system default destination:')" "$L"
$PY - "$T" <<'EOF'
import sys
c=b'BT /F1 28 Tf 72 700 Td (Rudolf OS print queue test) Tj ET'
objs=[b'<< /Type /Catalog /Pages 2 0 R >>',b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>',b'<< /Length %d >>\nstream\n'%len(c)+c+b'\nendstream',b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>']
out=b'%PDF-1.4\n';offs=[]
for i,o in enumerate(objs): offs.append(len(out)); out+=b'%d 0 obj\n'%(i+1)+o+b'\nendobj\n'
x=len(out); out+=b'xref\n0 %d\n0000000000 65535 f \n'%(len(objs)+1)+b''.join(b'%010d 00000 n \n'%o for o in offs)+b'trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n'%(len(objs)+1,x)
open(sys.argv[1]+'/doc.pdf','wb').write(out)
open(sys.argv[1]+'/doc.ps','w').write('%!PS-Adobe-3.0\n/Helvetica findfont 20 scalefont setfont 72 700 moveto (PS) show showpage\n')
EOF
chmod 644 $T/doc.pdf $T/doc.ps
# wait until the queue says a job is no longer waiting or printing
jwait() { for i in $(seq 1 ${2:-100}); do s=$($T/rudolf-printq jobs | tr '{' '\n' | grep "\"id\":$1,"); case "$s" in *'"state":"waiting"'*|*'"state":"printing"'*|'') timeout 0.2 tail -f /dev/null;; *) echo "$s"; return;; esac; done; echo "$s"; }
o=$(as rpqa lp -d Office_PDF_printer -t "'Quarterly report'" $T/doc.pdf 2>&1); J1=$(echo "$o" | sed -n 's/.*-\([0-9]*\) .*/\1/p')
s=$(jwait $J1); f=$(ls $T/pdfjobs/*.pdf 2>/dev/null | head -1)
ok "3. lp (as an account's user): the PDF to the PDF printer, sent" "$(has "$s" '"state":"sent"')$(has "$s" '"user":"rpqa"')" "$o / $s / $(tail -5 $T/q.log)"
ok "...the printer's file is the document (its text)" "$([ -n "$f" ] && pdftotext "$f" - 2>/dev/null | grep -q 'Rudolf OS print queue test' && echo 1)" "$(ls -la $T/pdfjobs)"
o=$(as rpqa lp -d Hall_raster_printer -o print-color-mode=monochrome $T/doc.pdf 2>&1); J2=$(echo "$o" | sed -n 's/.*-\([0-9]*\) .*/\1/p'); s=$(jwait $J2 200)
RF=$(ls $T/rasjobs/* 2>/dev/null | head -1)
ok "4. the same PDF to a printer that takes no PDF: sent as raster made here (Poppler)" "$(has "$s" '"state":"sent"')$([ -n "$RF" ] && echo 1)" "$o / $s / $(tail -3 $T/q.log) $(ls -la $T/rasjobs)"
P=$($PY - "$RF" <<'EOF'
import sys, struct
b=open(sys.argv[1],'rb').read() if len(sys.argv)>1 and sys.argv[1] else b''
if b[:4]==b'RaS2':
    h=b[4:4+1796]; w,hh=struct.unpack('>II',h[372:380]); bpp=struct.unpack('>I',h[388:392])[0]; dpi=struct.unpack('>I',h[276:280])[0]; bpl=struct.unpack('>I',h[392:396])[0]
    i=4+1796; ink=0; y=0; px=bpp//8
    while y<hh:
        rep=b[i]; i+=1; x=0; line=bytearray()
        while x<w:
            c=b[i]; i+=1
            if c<128: line+=b[i:i+px]*(c+1); i+=px; x+=c+1
            else: n=257-c; line+=b[i:i+n*px]; i+=n*px; x+=n
        ink+=sum(1 for v in line if v<128)*(rep+1); y+=rep+1
    print('pwg',w,hh,bpp,dpi,ink)
elif b[:7]==b'UNIRAST': print('urf')
else: print('none', b[:8])
EOF
)
ok "...the printer's file decodes: PWG raster, a Letter page at 300 dpi, grey (monochrome), the text's ink" "$(echo "$P" | awk '{print ($1=="pwg" && $2==2550 && $3==3300 && $4==8 && $5==300 && $6>500 && $6<300000)?1:0}')" "$P"
if $PY -c 'import gi; gi.require_version("Gtk","3.0"); from gi.repository import Gtk' 2>/dev/null && command -v Xvfb >/dev/null; then
	Xvfb :87 -screen 0 800x600x24 >/dev/null 2>&1 & PIDS="$PIDS $!"; timeout 1.5 tail -f /dev/null
	cat > $T/gtk.py <<'EOF'
import gi, sys
gi.require_version('Gtk', '3.0'); from gi.repository import Gtk
op = Gtk.PrintOperation(); op.set_n_pages(1); op.set_job_name('GTK page')
def draw(o, ctx, n):
    c = ctx.get_cairo_context(); c.set_font_size(30); c.move_to(72, 120); c.show_text('Printed from GTK')
op.connect('draw-page', draw); s = Gtk.PrintSettings(); s.set_printer(sys.argv[1]); op.set_print_settings(s)
print(op.run(Gtk.PrintOperationAction.PRINT, None).value_nick)
EOF
	chmod 644 $T/gtk.py; n1=$(ls $T/pdfjobs | wc -l); n2=$(ls $T/rasjobs | wc -l)
	g1=$(su -s /bin/sh rpqb -c "DISPLAY=:87 CUPS_SERVER=$RPQ_SOCK HOME=$T timeout 60 $PY $T/gtk.py Office_PDF_printer" 2>&1 | tail -1)
	g2=$(su -s /bin/sh rpqb -c "DISPLAY=:87 CUPS_SERVER=$RPQ_SOCK HOME=$T timeout 60 $PY $T/gtk.py Hall_raster_printer" 2>&1 | tail -1)
	for i in $(seq 1 60); do [ $(ls $T/pdfjobs | wc -l) -gt $n1 ] && [ $(ls $T/rasjobs | wc -l) -gt $n2 ] && break; timeout 0.25 tail -f /dev/null; done
	gf=$(ls -t $T/pdfjobs/*.pdf | head -1)
	ok "5. GTK's printing (Gtk.PrintOperation, as GTK apps print), on both printers" "$(has "$g1" apply)$(has "$g2" apply)$([ $(ls $T/rasjobs | wc -l) -gt $n2 ] && echo 1)$(pdftotext "$gf" - 2>/dev/null | grep -q 'Printed from GTK' && echo 1)" "$g1 / $g2 / $(tail -4 $T/q.log)"
else echo "NOTE PyGObject with GTK 3 or Xvfb is not installed: GTK's printing not tried"; fi
# two accounts: a job of each held behind one that never finishes
o=$(as rpqa lp -d Silent_printer -t "'Blocker'" $T/doc.pdf 2>&1); JS=$(echo "$o" | sed -n 's/.*-\([0-9]*\) .*/\1/p')
o=$(as rpqa lp -d Silent_printer -t "'Private letter'" $T/doc.pdf 2>&1); JA=$(echo "$o" | sed -n 's/.*-\([0-9]*\) .*/\1/p')
o=$(as rpqb lp -d Silent_printer -t "'Bs page'" $T/doc.pdf 2>&1); JB=$(echo "$o" | sed -n 's/.*-\([0-9]*\) .*/\1/p')
timeout 1 tail -f /dev/null
Lb=$(as rpqb lpstat -o 2>&1); Lb2=$(as rpqb lpstat -l -o Silent_printer 2>&1)
ok "6. the other account's jobs are listed without their names (lpstat -o as rpqb)" "$(has "$Lb" "Silent_printer-$JA")$(has "$Lb" 'another account')$(has "$Lb" rpqb)$(echo "$Lb $Lb2" | grep -q 'Private letter' && echo 0 || echo 1)" "$Lb"
c=$(as rpqb cancel Silent_printer-$JA 2>&1; echo "status $?"); s=$($T/rudolf-printq jobs | tr '{' '\n' | grep "\"id\":$JA,")
ok "7. cancel of another account's job is refused" "$(has "$c" 'status 1')$(has "$c" "another account")$(has "$s" '"state":"waiting"')" "$c / $s"
c=$(as rpqb cancel -a Silent_printer 2>&1); s=$($T/rudolf-printq jobs | tr '{' '\n'); sa=$(echo "$s" | grep "\"id\":$JA,"); sb=$(echo "$s" | grep "\"id\":$JB,")
ok "...cancel -a cancels only one's own (B's canceled, A's still waiting)" "$(has "$sa" '"state":"waiting"')$(has "$sb" '"state":"canceled"')" "$c / $sa / $sb"
c=$($T/rudolf-printq cancel $JA --as rpqb; echo "status $?")
ok "...rudolfd's cancel for B (root, --as rpqb) is held to B's jobs: refused" "$(has "$c" 'status 1')$(has "$c" '"ok":false')$(has "$($T/rudolf-printq jobs | tr '{' '\n' | grep "\"id\":$JA,")" '"state":"waiting"')" "$c"
j=$(su -s /bin/sh rpqb -c "RPQ_SOCK=$RPQ_SOCK $T/rudolf-printq jobs --as rpqa" | tr '{' '\n' | grep "\"id\":$JA,")
ok "...a user that says it is another (X-Rudolf-As, not root) is still itself: A's job without its name, not B's to cancel" "$(has "$j" '"mine":false')$(has "$j" '"name":""')$(has "$j" '"cancel":false')" "$j"
jb=$($T/rudolf-printq jobs --as rpqb | tr '{' '\n' | grep "\"id\":$JA,"); ja=$($T/rudolf-printq jobs --as rpqa | tr '{' '\n' | grep "\"id\":$JA,")
ok "...rudolfd's queue for Settings: as A, A's job with its name and Cancel; as B, without" "$(has "$ja" '"name":"Private letter"')$(has "$ja" '"mine":true')$(has "$ja" '"cancel":true')$(has "$jb" '"name":""')$(has "$jb" '"cancel":false')" "$ja / $jb"
c=$(as rpqa cancel Silent_printer-$JA 2>&1; echo "status $?"); s=$($T/rudolf-printq jobs | tr '{' '\n' | grep "\"id\":$JA,")
ok "8. its own account cancels it" "$(has "$c" 'status 0')$(has "$s" '"state":"canceled"')" "$c / $s"
c=$($T/rudolf-printq cancel $JS --as rpqa); ok "...and rudolfd for A cancels A's job that is printing (stopped)" "$(has "$c" '"ok":true')$(has "$($T/rudolf-printq jobs | tr '{' '\n' | grep "\"id\":$JS,")" canceled)" "$c"
# rudolfd's submit: the desktop's job, as the signed-in account's user, with its options and title
printf 'Budget 2027\n' > $T/title.txt; printf 'Lobby printer\n' > $T/name.txt
s=$($T/rudolf-printq submit ipp://127.0.0.1:8761/ipp/print $T/doc.pdf application/pdf copies=2 sides=one-sided color=monochrome media=na_letter_8.5x11in title-file=$T/title.txt name-file=$T/name.txt --as rpqa)
JD=$(echo "$s" | sed -n 's/.*"job":\([0-9]*\).*/\1/p'); d=$(jwait $JD)
ok "9. rudolfd's submit (a desktop job): queued, sent, the account's, its title" "$(has "$s" '"queued":true')$(has "$d" '"state":"sent"')$(has "$d" '"user":"rpqa"')$(has "$d" '"name":"Budget 2027"')$(has "$d" '"printer":"Lobby printer"')" "$s / $d"
ok "...the printer it printed to by address remembered for the Linux apps" "$(grep -q '^ipp://127.0.0.1:8761/ipp/print	Lobby printer$' $T/known && echo 1)$(has "$(as rpqb lpstat -p 2>&1)" Lobby_printer)" "$(cat $T/known)"
s=$($T/rudolf-printq submit 'ipp://x;rm -rf /' $T/doc.pdf application/pdf --as rpqa); s2=$($T/rudolf-printq submit ipp://127.0.0.1:8761/ipp/print $T/doc.pdf text/html --as rpqa)
ok "...an address or format it does not take is refused" "$(has "$s" '"ok":false')$(has "$s2" 'unknown document format')" "$s / $s2"
o=$(as rpqa lp -d Office_PDF_printer $T/doc.ps 2>&1; echo "status $?")
ok "10. a PostScript document is refused, saying Rudolf OS's printers take PDF" "$(has "$o" 'take PDF')$(has "$o" 'status 1')" "$o"
o=$(as rpqa lp -d Office_PDF_printer /etc/hostname 2>&1; echo "status $?")
ok "...as is plain text" "$(has "$o" 'take PDF')" "$o"
# a printer that is not there: the job fails with why, the queue goes on
printf 'ipp://127.0.0.1:9/ipp/print\tGone printer\n' >> $T/known
s=$($T/rudolf-printq submit ipp://127.0.0.1:9/ipp/print $T/doc.pdf application/pdf --as rpqb); JG=$(echo "$s" | sed -n 's/.*"job":\([0-9]*\).*/\1/p'); g=$(jwait $JG)
ok "11. a printer that does not answer: the job fails, saying so" "$(has "$g" '"state":"failed"')$(has "$g" 'does not answer')" "$g"
o=$(as rpqb lp -d Office_PDF_printer $T/doc.pdf 2>&1); J3=$(echo "$o" | sed -n 's/.*-\([0-9]*\) .*/\1/p'); s=$(jwait $J3)
ok "...and the next job prints" "$(has "$s" '"state":"sent"')" "$s"
L=$(as rpqa lpstat -W completed -o 2>&1)
ok "12. finished jobs stay listed (lpstat -W completed)" "$(has "$L" "Office_PDF_printer-$J1")$(has "$L" "Hall_raster_printer-$J2")" "$L"
ok "...the spool keeps no document of a finished job" "$([ -z "$(ls $T/spool | grep -v '^body-')" ] && echo 1)" "$(ls -la $T/spool)"
kill $QP 2>/dev/null; for i in $(seq 1 20); do [ -S $RPQ_SOCK ] || break; timeout 0.1 tail -f /dev/null; done
ok "13. the queue stopped: its socket gone" "$([ -S $RPQ_SOCK ] && echo 0 || echo 1)" "$(tail -3 $T/q.log)"
# ipp-usb's ports (usbprint): kept to 60000-60099, and only root, the queue's user and the desktop's may connect to them
U=$I/sys/root/usr/lib/rudolf/usbprint; S=$T/sys/bus/usb; mkdir -p $S/devices/1-1/1-1:1.0 $S/drivers/usblp $T/urun $T/ust $T/ubin
for f in idVendor:03f0 idProduct:2c17 manufacturer:HP product:Printer; do echo ${f#*:} > $S/devices/1-1/${f%%:*}; done
echo 07 > $S/devices/1-1/1-1:1.0/bInterfaceClass; echo 04 > $S/devices/1-1/1-1:1.0/bInterfaceProtocol; ln -s 1-1/1-1:1.0 $S/devices/1-1:1.0
printf '[network]\n  http-min-port = 60000\n  http-max-port = 65535\n' > $T/ipp-usb.conf
printf '#!/bin/sh\nprintf "[device]\\n  http-port = 60000\\n" > %s/ust/03f0-2c17-X.state\nexec sleep 30\n' $T > $T/ubin/ipp-usb; chmod 755 $T/ubin/ipp-usb
RUDOLF_USBPRINT_SYS=$T/sys RUDOLF_RUN=$T/urun RUDOLF_IPPUSB=$T/ubin/ipp-usb RUDOLF_IPPUSB_STATE=$T/ust RUDOLF_IPPUSB_WAIT=30 RUDOLF_IPPUSB_CONF=$T/ipp-usb.conf RUDOLF_USBPRINT_NFT=$T/guard.nft sh $U scan
pkill -f "$T/ubin/ipp-usb" 2>/dev/null
ok "14. ipp-usb's ports kept to 60000-60099 when usbprint starts it" "$(grep -q 'http-max-port = 60099' $T/ipp-usb.conf && echo 1)$(grep -q 'dport 60000-60099 meta skuid { 0, 952, 1000 } accept' $T/guard.nft && echo 1)" "$(cat $T/ipp-usb.conf $T/guard.nft 2>&1)"
if command -v nft >/dev/null && command -v unshare >/dev/null && unshare -n true 2>/dev/null; then
	sed "s/{ 0, 952, 1000 }/{ 0, $(id -u rpqq) }/" $T/guard.nft > $T/guard2.nft; chmod 644 $T/guard2.nft
	printf 'import socket, sys\ntry:\n    s = socket.create_connection(("127.0.0.1", int(sys.argv[1])), 2); print("connected")\nexcept Exception as e: print("refused", type(e).__name__)\n' > $T/conn.py; chmod 644 $T/conn.py
	o=$(unshare -n sh -c "ip link set lo up 2>/dev/null || $PY -c 'import socket,fcntl,struct;s=socket.socket();fcntl.ioctl(s,0x8914,struct.pack(\"16sH14s\",b\"lo\",0x49,b\"\"))'; nft -f $T/guard2.nft || echo NFTFAIL; $PY -c \"import socket,time;s=socket.socket();s.bind(('127.0.0.1',60005));s.listen(9);time.sleep(8)\" & $PY -c \"import socket,time;s=socket.socket();s.bind(('127.0.0.1',8765));s.listen(9);time.sleep(8)\" & timeout 0.5 tail -f /dev/null
		echo root \$($PY $T/conn.py 60005); echo queue \$(su -s /bin/sh rpqq -c '$PY $T/conn.py 60005'); echo acct \$(su -s /bin/sh rpqa -c '$PY $T/conn.py 60005'); echo other \$(su -s /bin/sh rpqa -c '$PY $T/conn.py 8765'); kill %1 %2 2>/dev/null" 2>&1)
	ok "...(nftables, in a network namespace of its own) root and the queue's user connect to them, an account's user is refused; its other ports are not affected" "$(has "$o" 'root connected')$(has "$o" 'queue connected')$(has "$o" 'acct refused')$(has "$o" 'other connected')" "$o"
else echo "NOTE nft or network namespaces are not available: the ports' rules not tried"; fi
echo "printq161_t.sh: $((N-F))/$N passed"; [ $F = 0 ]
