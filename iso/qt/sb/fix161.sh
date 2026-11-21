#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 11), in a QEMU guest (r161_t.py, t160_t.py with R161_FIX): this tree's rudolfd,
# rudolf-rootd and root helpers (the disk labelled R161FIX: rudolfd, rudolf-rootd, root/NAME) put in place of the
# release's, and rudolfd started again through rudolf-rootd, as the base layer's rudolf-init will start it.
#   sh fix161.sh [reload]   reload: the desktop's browser started again too (rudolfd's token is new)
FX=; for d in /dev/nvme*n1 /dev/sd?; do [ -b "$d" ] && blkid $d 2>/dev/null | grep -q R161FIX && FX=$d; done; echo fix=$FX
[ -n "$FX" ] || { echo "no R161FIX disk"; exit 1; }
mkdir -p /tmp/fx; mountpoint -q /tmp/fx || mount -o ro $FX /tmp/fx
X=/run/r161x; mkdir -p $X; mountpoint -q $X || mount -t tmpfs -o exec,mode=755 none $X
cp /tmp/fx/rudolfd /tmp/fx/rudolf-rootd $X/; chmod 755 $X/rudolfd $X/rudolf-rootd
mountpoint -q /usr/bin/rudolfd || mount --bind $X/rudolfd /usr/bin/rudolfd
cp $X/rudolf-rootd /usr/bin/rudolf-rootd; chmod 755 /usr/bin/rudolf-rootd
mkdir -p /usr/lib/rudolf/root; chmod 755 /usr/lib/rudolf/root
for h in /tmp/fx/root/*; do n=${h##*/}; cp $h /usr/lib/rudolf/root/$n; chown 0:0 /usr/lib/rudolf/root/$n; chmod 700 /usr/lib/rudolf/root/$n; done
ls -la /usr/lib/rudolf/root
# 1.6.1 (plan Part 20): the confinement's switch for this run (the disk's file "confine": off, log or on), read by
# rudolf-rootd and rudolfd at their start below
[ -f /tmp/fx/confine ] && { mkdir -p /persist/etc; cp /tmp/fx/confine /persist/etc/rudolf-confine; echo "confine=$(cat /persist/etc/rudolf-confine)"; }
# (the running one ended: rudolf-rootd if there is one, which passes it on to its rudolfd; the first rudolfd, the main one)
R=$(pidof rudolf-rootd); [ -n "$R" ] && kill $R
P=$(pidof rudolfd | tr ' ' '\n' | sort -n | head -1); [ -n "$P" ] && kill $P 2>/dev/null
for i in $(seq 1 20); do busybox wget -q -O /dev/null http://127.0.0.1:8080/favicon.png 2>/dev/null || break; sleep 0.5; done
(setsid /usr/bin/rudolf-rootd /usr/bin/rudolfd >> /run/rudolf/rudolfd.log 2>&1 < /dev/null &)
for i in $(seq 1 60); do busybox wget -q -O /dev/null http://127.0.0.1:8080/favicon.png 2>/dev/null && break; sleep 1; done
echo rudolfd $(pidof rudolfd) rootd $(pidof rudolf-rootd); grep 'root helpers' /run/rudolf/rudolfd.log | tail -1
[ "$1" = reload ] && { pkill -f '[-]-rudolf-kiosk'; pkill -f 'rudolf-native --rudolf-kiosk'; pkill -f 'rudolf-webkit.*--rudolf-kiosk'; echo reloaded; }
# (1.6.1 Part 20: what the confinement refused or logged since this drive was made, last in this script's output, which
# the QEMU tests keep the end of)
[ -f /persist/var/log/confine.log ] && echo "confine.log: $(grep -c ' confinement ' /persist/var/log/confine.log) starts, $(grep -cE 'Z (refused|logged) ' /persist/var/log/confine.log) calls, $(grep -c ' asked for ' /persist/var/log/confine.log) requests; $(grep -E 'Z (refused|logged) | asked for ' /persist/var/log/confine.log | sed 's/ (.*//' | sort -u | tail -4 | cut -c22- | tr '\n' ';')"
exit 0
