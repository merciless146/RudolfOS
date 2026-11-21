#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 9): rudolfd's printing through the print queue. The real rudolfd and rudolf-printq,
# built here, in a private mount namespace (its own /run, so the queue's socket is /run/cups/cups.sock as on Rudolf OS),
# against a virtual IPP Everywhere printer (ippeveprinter; fakeprinter.py without it):
#   POST print/job: queued for the desktop's user (nobody signed in), sent, its title and the printer's name kept;
#   print/jobs: the queue, the job sent, with Cancel only for one's own; another account's job without its name;
#   print/cancel: another account's job refused, a malformed number too; without the queue: print/jobs says so, print/job
#   sends the job itself as before.
# Needs root, curl, port 8080 free.   sudo sh printqapi161_t.sh   (RPQT_PRELOAD as for printq161_t.sh)
[ "$(id -u)" = 0 ] || { echo "SKIP printqapi161_t.sh: needs root"; exit 0; }
[ -z "$PQA_INNER" ] && exec env PQA_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../..; SYS=$I/sys; T=$(mktemp -d /tmp/printqapi161.XXXXXX); chmod 755 $T
mount --make-rprivate / 2>/dev/null
# (ippeveprinter announces itself through Avahi, which its library reaches over D-Bus: their sockets in /run kept)
KEEP=; for d in avahi-daemon dbus; do [ -d /run/$d ] && mkdir -p $T/keep/$d && mount --bind /run/$d $T/keep/$d && KEEP="$KEEP $d"; done
mount -t tmpfs none /run; for d in $KEEP; do mkdir -p /run/$d && mount --bind $T/keep/$d /run/$d && umount $T/keep/$d; done; mkdir -p /run/rudolf /usr/share/rudolf/www /home/rudolf /persist
mount --bind $I/www /usr/share/rudolf/www; mount -t tmpfs none /home/rudolf; mount -t tmpfs none /persist
N=0; F=0; ok() { N=$((N+1)); case "$2" in *[!1]*|'') r=0;; *) r=1;; esac; if [ $r = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
MADE=; PIDS=
trap 'for p in $PIDS; do kill $p 2>/dev/null; done; for u in $MADE; do userdel $u 2>/dev/null; done; umount /usr/share/rudolf/www /home/rudolf /persist 2>/dev/null; rm -rf $T' EXIT
# (the desktop's user, rudolf, as on Rudolf OS: the queue knows users by name; and another account's)
for u in rudolf rpqx; do id $u >/dev/null 2>&1 || { useradd -M -d /nonexistent -s /bin/false $u && MADE="$MADE $u"; }; done
gcc -O2 -w -o $T/rudolf-print $SYS/rudolf-print.c -ldl && gcc -O2 -w -o $T/rudolf-printq $SYS/rudolf-printq.c -lz || { echo "FAIL build"; exit 1; }
sed "s|\"/usr/bin/rudolf-print|\"$T/rudolf-print|g" $SYS/rudolfd.c > $T/rudolfd.c
gcc -O2 -w -I$SYS -DPRINTQ_BIN="\"$T/rudolf-printq\"" -o $T/rudolfd $T/rudolfd.c -lz || { echo "FAIL build rudolfd"; exit 1; }
mkdir -p $T/jobs
if command -v ippeveprinter >/dev/null; then env ${RPQT_PRELOAD:+LD_PRELOAD=$RPQT_PRELOAD} ippeveprinter -p 8771 -n localhost -d $T/jobs -k -f application/pdf -F application/pdf "API Printer" > $T/ipp.log 2>&1 & PIDS="$PIDS $!"
else python3 $HERE/../fakeprinter.py $T/jobs 8771 pdf > $T/ipp.log 2>&1 & PIDS="$PIDS $!"; fi
if ! w8 "$T/rudolf-print info ipp://localhost:8771/ipp/print | grep -q '\"ok\":true'" 40; then
	echo "NOTE ippeveprinter did not answer ($(tail -1 $T/ipp.log)): the repository's fakeprinter.py stands in"
	python3 $HERE/../fakeprinter.py $T/jobs 8771 pdf > $T/ipp.log 2>&1 & PIDS="$PIDS $!"; w8 "$T/rudolf-print info ipp://localhost:8771/ipp/print | grep -q '\"ok\":true'" 40
fi
export RPQ_PRINT=$T/rudolf-print RPQ_USER=-   # (its spool and printers' file where Rudolf OS has them: /run/rudolf-printq, in this /run)
qstart() { $T/rudolf-printq serve > $T/q.log 2>&1 & QP=$!; PIDS="$PIDS $QP"; w8 '[ -S /run/cups/cups.sock ]' 40; }
qstart
$T/rudolfd > $T/rd.log 2>&1 & PIDS="$PIDS $!"; w8 'curl -s http://127.0.0.1:8080/favicon.png -o /dev/null' 60
TOK=$(curl -s http://127.0.0.1:8080/ | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1)
ok "0. rudolfd and the queue run" "$([ ${#TOK} = 32 ] && echo 1)$([ -S /run/cups/cups.sock ] && echo 1)" "$TOK $(tail -3 $T/rd.log)"
api() { m=$1; p=$2; shift 2; curl -s -X $m -H "X-Rudolf-Token: $TOK" "$@" "http://127.0.0.1:8080/api/$p"; }
printf '%%PDF-1.4\n1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >> endobj\ntrailer << /Root 1 0 R >>\n%%%%EOF\n' > $T/doc.pdf
o=$(api POST 'print/job?uri=ipp%3A%2F%2Flocalhost%3A8771%2Fipp%2Fprint&format=application%2Fpdf&title=Holiday%20letter&name=Study%20printer&copies=1' --data-binary @$T/doc.pdf)
J=$(echo "$o" | sed -n 's/.*"job":\([0-9]*\).*/\1/p')
ok "1. POST print/job goes through the queue (queued, its job number)" "$(has "$o" '"queued":true')$([ -n "$J" ] && echo 1)" "$o"
w8 "api POST print/jobs | grep -q '\"id\":$J,\"state\":\"sent\"'" 60; j=$(api POST print/jobs)
ok "2. print/jobs: the job sent, the desktop's user's own, its title and the printer's name" "$(has "$j" "\"id\":$J,\"state\":\"sent\",\"mine\":true,\"name\":\"Holiday letter\",\"printer\":\"Study printer\"")$(has "$j" '"user":"rudolf"')$(has "$j" '"printers":[{"q":"Study_printer"')" "$j"
ok "...the printer has it" "$([ -n "$(ls $T/jobs)" ] && echo 1)" "$(ls -la $T/jobs; tail -3 $T/ipp.log)"
s=$(RPQ_SOCK=/run/cups/cups.sock $T/rudolf-printq submit ipp://127.0.0.1:9/ipp/print $T/doc.pdf application/pdf title-file=/etc/hostname --as rpqx); X=$(echo "$s" | sed -n 's/.*"job":\([0-9]*\).*/\1/p')
j=$(api POST print/jobs | tr '{' '\n' | grep "\"id\":$X,")
ok "3. another account's job: listed without its name, user or message, and no Cancel" "$(has "$j" '"mine":false,"name":""')$(has "$j" '"user":""')$(has "$j" '"cancel":false')" "$j"
c=$(api POST "print/cancel?id=$X"); c2=$(api GET "print/cancel?id=$X"); c3=$(api POST "print/cancel?id=1;reboot")
ok "4. print/cancel of another account's job is refused; GET and a malformed number too" "$(has "$c" '"ok":false')$(has "$c" "another account")$(has "$c2" 'use POST')$(has "$c3" 'which job')" "$c / $c2 / $c3"
kill $QP; w8 '[ ! -S /run/cups/cups.sock ]' 20
j=$(api POST print/jobs)
for i in $(seq 1 20); do o=$(api POST 'print/job?uri=ipp%3A%2F%2Flocalhost%3A8771%2Fipp%2Fprint&format=application%2Fpdf&title=Direct' --data-binary @$T/doc.pdf); case "$o" in *busy*) timeout 1 tail -f /dev/null;; *) break;; esac; done   # (the virtual printer still printing the last job)
ok "5. without the queue: print/jobs says so, print/job sends the job itself as before" "$(has "$j" '"queue":false')$(has "$o" '"ok":true')$(has "$o" 'queued' | tr 1 0 | tr 0 1)" "$j / $o"
echo "printqapi161_t.sh: $((N-F))/$N passed"; [ $F = 0 ]
