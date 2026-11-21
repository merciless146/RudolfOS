#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 9), in a QEMU guest (p161_t.py): this tree's printing put in place on the release, as the
# base layer will have it: rudolfd and the root helpers (fix161.sh), rudolf-print and rudolf-printq, the "print" user in
# group lp, the list of listeners, /var/run, the queue started as rudolf-init starts it, and the page (index.html).
#   sh fix161p.sh [reload]     (the disk labelled R161FIX, mounted at /tmp/fx by fix161.sh)
sh /tmp/fix161.sh || exit 1
X=/run/r161x; for b in rudolf-print rudolf-printq; do cp /tmp/fx/$b $X/$b; chmod 755 $X/$b; done
mountpoint -q /usr/bin/rudolf-print || mount --bind $X/rudolf-print /usr/bin/rudolf-print
cp $X/rudolf-printq /usr/bin/rudolf-printq; chmod 755 /usr/bin/rudolf-printq
grep -q '^print:' /etc/passwd || echo 'print:x:952:952:Printing (rudolf-printq):/nonexistent:/bin/false' >> /etc/passwd
grep -q '^print:' /etc/group || echo 'print:x:952:' >> /etc/group
grep -q '^lp:.*print' /etc/group || sed -i 's/^lp:x:7:$/lp:x:7:print/; s/^lp:x:7:\(..*\)$/lp:x:7:\1,print/' /etc/group
[ -f /tmp/fx/listeners ] && { mountpoint -q /usr/share/rudolf/listeners || mount --bind /tmp/fx/listeners /usr/share/rudolf/listeners; }
[ -e /var/run ] || ln -s ../run /var/run
pgrep -f 'rudolf-printq serve' >/dev/null || (setsid sh -c 'while :; do rudolf-printq serve >> /run/rudolf/printq.log 2>&1; sleep 5; done' </dev/null >/dev/null 2>&1 &)
for i in $(seq 1 30); do [ -S /run/cups/cups.sock ] && break; sleep 0.5; done
echo "queue $(stat -c '%a %U' /run/cups/cups.sock 2>&1) as $(ps -o user= -p $(pgrep -f 'rudolf-printq serve' | tail -1) 2>/dev/null)"; tail -2 /run/rudolf/printq.log
if [ -f /tmp/fx/index.html ]; then
	WWW=/usr/share/rudolf/www; [ -f /persist/os/current/index.html ] && WWW=/persist/os/current
	cp /tmp/fx/index.html /tmp/page.html && chmod 644 /tmp/page.html; mountpoint -q $WWW/index.html || mount --bind /tmp/page.html $WWW/index.html
	[ -d /tmp/fx/a ] && for f in /tmp/fx/a/*; do [ -e $WWW/a/${f##*/} ] || cp $f $WWW/a/ 2>/dev/null; done
fi
if [ "$1" = reload ]; then pkill -f '[-]-rudolf-kiosk'; echo reloaded; fi
exit 0
