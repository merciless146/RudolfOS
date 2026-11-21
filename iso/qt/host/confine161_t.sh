#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 20): rudolfd confined (rudolfd-confine.inc, rudolf-confine.h) and rudolf-rootd's helpers
# each narrower. The lists read from the source are current (confinepure.py: the requests that start nothing; winapi.py:
# what each window asks for); every system call number through each filter (fixtures/confineprobe.c bpf); each layer put
# on for real and every allowed and refused folder and call tried (narrow, tree, helper); then the real rudolfd and
# rudolf-rootd with the switch off, log and on: the layers on the right processes, the page and the API answering as
# before, a window's request it never makes refused (on) or only noted (log), a helper's capabilities. In a private
# mount namespace (port 8080 must be free). Stops only what it started. Needs root, gcc, curl, python3.
#   sh confine161_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$CF_INNER" ] && exec env CF_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; ISO=$HERE/../..; T=$(mktemp -d /tmp/cf161.XXXXXX); chmod 755 $T
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 700)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt ${2:-80} ]; do eval "$1" && return 0; timeout 0.25 tail -f /dev/null; i=$((i+1)); done; return 1; }
RDP=
trap 'for p in $RDP; do kill $p 2>/dev/null; done; umount -l /usr/share/rudolf/www /home/rudolf /persist /run 2>/dev/null; rm -rf $T' EXIT

# ---- the lists read from the source
o=$(python3 $ISO/systools/confinepure.py 2>&1); ok "1. the requests that start nothing: rudolfd-confine-pure.h is current" "$(echo "$o" | grep -q '^confinepure: current' && echo 1)" "$o"
o=$(python3 $ISO/systools/winapi.py 2>&1); ok "2. what each window asks for: rudolfd-winapi.h is current" "$(echo "$o" | grep -q '^winapi: current' && echo 1)" "$o"
o=$(grep -c 'traced (' $SYS/rudolfd-confine-narrow.h); ok "3. the narrow system calls: most of them traced, each added one with why" "$([ "$o" -ge 60 ] && ! grep -E '^\s*SYS_' $SYS/rudolfd-confine-narrow.h | grep -vq 'traced (\|added: ' && echo 1)" "$o"

# ---- every call number through each filter, then each layer for real
gcc -O2 -I$SYS -o $T/probe $HERE/fixtures/confineprobe.c || { echo "FAIL build probe"; exit 1; }
o=$($T/probe bpf 2>&1); ok "4. every system call (0-511, x86-64, 32-bit, x32) through each filter as its list says; the narrow list never has exec, mount, kill, ptrace; layer 1 never refuses what rudolfd's programs need" "$(echo "$o" | tail -1 | grep -q '^PASS' && echo 1)" "$(echo "$o" | grep -v ' 0 wrong')"
R=$T/root; for d in /run/rudolf /run/user/1000 /persist/etc /persist/var/log /persist/accounts /home /media /tmp /etc /usr /persist/os /persist/debian /var /boot /proc/sys/vm /dev; do mkdir -p $R$d; done
: > $R/proc/sys/vm/drop_caches; : > $R/dev/null; echo x > $R/etc/existing
o=$($T/probe narrow $R 2>&1); ok "5. the narrow layer: writes only in rudolfd's folders (each tried), nowhere else; no program, mount, ptrace, kill, setns, unshare, chroot, reboot, module, kexec, bpf, keyctl, io_uring, clock, swap, 32-bit call or TCP of its own; its capabilities; what a request does still works" "$(echo "$o" | tail -1 | grep -q '^PASS' && echo 1)" "$(echo "$o" | grep FAIL)"
echo "$o" | grep -c '^PASS' > $T/n5; echo "   ($(cat $T/n5) checks; Landlock ABI $(python3 -c 'import ctypes;print(ctypes.CDLL(None).syscall(444,None,0,1))'))"
o=$($T/probe tree 2>&1); ok "6. layer 1 (rudolfd and all it starts): its deny list refused (64- and 32-bit), its capabilities gone from the bounding set; programs, mounts, swap and modules still as the kernel answers" "$(echo "$o" | tail -1 | grep -q '^PASS' && echo 1)" "$(echo "$o" | grep FAIL)"
o=$($T/probe helper drives 2>&1); o2=$($T/probe helper updates 2>&1)
ok "7. each helper: only its capabilities, ptrace, modules, reboot, the clock, swap, kexec and bpf refused, its scripts still run" "$(echo "$o" | tail -1 | grep -q '^PASS' && echo "$o2" | tail -1 | grep -q '^PASS' && echo 1)" "$(echo "$o $o2" | grep FAIL)"

# ---- the real rudolfd
mount -t tmpfs none /run; mkdir -p /run/rudolf /run/user/1000 /usr/share/rudolf/www /home/rudolf /persist
mount --bind $ISO/www /usr/share/rudolf/www; mount -t tmpfs none /home/rudolf; mount -t tmpfs none /persist; mkdir -p /persist/etc /persist/var/log /persist/accounts
CF="-O2 -w -DRR_TEST"
printf '#!/bin/sh\nexit 0\n' > $T/ru; chmod 755 $T/ru
gcc -O2 -w -DRU_BIN="\"$T/ru\"" -o $T/rudolfd $SYS/rudolfd.c -lz && gcc $CF -o $T/rudolf-rootd $SYS/rudolf-rootd.c || { echo "FAIL build rudolfd"; exit 1; }
mkdir -p $T/rh; chmod 755 $T/rh
printf '#!/bin/sh\ngrep -E "^(Cap|Seccomp|NoNewPrivs)" /proc/self/status > %s/helper.status\necho "{\\"ok\\":true,\\"slot\\":\\"a\\"}"\n' $T > $T/rh/updates; chmod 700 $T/rh/updates
mkdir -p $T/cg; : > $T/cg/cgroup.procs
start() { : > $T/rd.log; rm -f /persist/var/log/health.log /run/rudolf/health.log /run/rudolf/winapi-refused /persist/var/log/confine.log /run/rudolf/confine.log; env $1 RUDOLFD_DESKTOP_CG=$T/cg RUDOLFD_DESKTOP_UID=65534 RR_TEST_DIR=$T/rh $2 > $T/rd.log 2>&1 & RDP=$!
	w8 'curl -s http://127.0.0.1:8080/favicon.png -o /dev/null' 60; TOK=$(curl -s http://127.0.0.1:8080/ | sed -n 's/.*RUDOLFD={token:"\([0-9a-f]*\)".*/\1/p' | head -1); }
stop() { kill $RDP 2>/dev/null; wait $RDP 2>/dev/null; RDP=; w8 '! curl -s -o /dev/null http://127.0.0.1:8080/favicon.png' 40; }
health() { cat /persist/var/log/health.log /run/rudolf/health.log 2>/dev/null; }
api() { curl -s -o $T/body -w '%{http_code}' -H "X-Rudolf-Token: $TOK" "http://127.0.0.1:8080/api/$1"; }
rdpid() { echo $RDP; }
st() { grep "^$2:" /proc/$1/status | cut -f2; }
# a request held open (events/wait waits up to 25 s): its connection's process, a child of rudolfd
held() { rm -f /run/rudolf/events; curl -s -m 30 -H "X-Rudolf-Token: $TOK" "http://127.0.0.1:8080/api/events/wait" > /dev/null & HP=$!; M=$(rdpid)
	w8 "[ -n \"\$(pgrep -P $M rudolfd)\" ]" 40; C=$(pgrep -P $M rudolfd | head -1); }
# clients as nobody (the desktop's user here), listed in the desktop's group: python3 (the page) or a copy named rudolf-native
cp /usr/bin/python3 $T/rudolf-native; chmod 755 $T/rudolf-native
cat > $T/cl.py <<'EOF'
import os, sys, time, urllib.request, urllib.error
path, tok = sys.argv[1], sys.argv[2]
os.setgid(65534); os.setuid(65534)
open(sys.argv[3], 'w').write(str(os.getpid()))
while not os.path.exists(sys.argv[3] + '.go'): time.sleep(.05)
q = urllib.request.Request('http://127.0.0.1:8080' + path, headers={'X-Rudolf-Token': tok})
try: r = urllib.request.urlopen(q, timeout=10); print(r.status)
except urllib.error.HTTPError as e: print(e.code, e.read().decode('utf8', 'replace')[:300])
EOF
chmod 644 $T/cl.py; chmod 777 $T
win() { rm -f $T/w.pid $T/w.pid.go; env PYTHONHOME=/usr $1 $T/cl.py "$2" "$TOK" $T/w.pid > $T/w.out 2>&1 & W=$!; w8 "[ -s $T/w.pid ]" 40
	echo "$(cat $T/w.pid)" >> $T/cg/cgroup.procs; touch $T/w.pid.go; wait $W; cat $T/w.out; }

# off
start "RUDOLFD_CONFINE=off" "$T/rudolfd"; M=$(rdpid)
c1=$(api ping); held; s=$(st $C Seccomp); kill $HP 2>/dev/null
ok "8. off (the default): no filter on rudolfd or its requests, no word of it in the health log" "$([ "$(st $M Seccomp)" = 0 ] && [ "$s" = 0 ] && [ "$c1" = 200 ] && ! health | grep -q confinement && echo 1)" "main=$(st $M Seccomp) req=$s ping=$c1 $(health | tail -2)"
stop

# on
start "RUDOLFD_CONFINE=on" "$T/rudolfd"; M=$(rdpid)
ok "9. on: rudolfd has layer 1 (one filter), the dropped capabilities out of its bounding set, and says so in the health log" "$([ "$(st $M Seccomp)" = 2 ] && [ "$(st $M Seccomp_filters)" = 1 ] && [ $(( 0x$(st $M CapBnd) & (1 << 20) )) = 0 ] && [ $(( 0x$(st $M CapBnd) & (1 << 39) )) = 0 ] && [ $(( 0x$(st $M CapBnd) & (1 << 21) )) != 0 ] && health | grep -q "rudolfd's confinement is on" && echo 1)" "$(grep -E 'Seccomp|CapBnd' /proc/$M/status) $(health)"
held; s=$(st $C Seccomp_filters); nn=$(st $C NoNewPrivs); ce=$(st $C CapEff); cb=$(st $C CapBnd)
w8 '! kill -0 $HP 2>/dev/null' 140; wait $HP 2>/dev/null
ok "10. a request that starts nothing (events/wait): two filters (layer 1 and the narrow one), no_new_privs, its capabilities only" "$([ "$s" = 2 ] && [ "$nn" = 1 ] && [ $(( 0x$ce & ~0x$(printf %x $(( (1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<6)|(1<<7)|(1<<19)|(1<<24) )) ) )) = 0 ] && [ $(( 0x$cb & (1 << 21) )) = 0 ] && echo 1)" "filters=$s nnp=$nn eff=$ce bnd=$cb"
c=$(api ping); c2=$(api health); c3=$(api origin); c4=$(curl -s -o $T/p -w '%{http_code}' http://127.0.0.1:8080/); c5=$(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8080/favicon.png)
ok "11. ...and they answer as before: ping, health, origin, the page (with its token) and its files" "$([ "$c$c2$c3$c4$c5" = 200200200200200 ] && grep -q 'RUDOLFD={token:"' $T/p && echo 1)" "$c $c2 $c3 $c4 $c5"
c=$(api stats); c2=$(api memory/settings)
ok "12. a request that starts programs (stats runs a command) answers as before too (layer 1 only)" "$([ "$c" = 200 ] && [ "$c2" = 200 ] && echo 1)" "$c $c2 $(cat $T/body | head -c 200)"
c=$(curl -s -o $T/body -w '%{http_code}' -X POST -H "X-Rudolf-Token: $TOK" --data 'cf161 written from a narrow request' "http://127.0.0.1:8080/api/clientlog")
ok "13. a narrow request writing where it may (clientlog: rudolfd's run folder, a narrow request): written" "$([ "$c" = 200 ] && grep -q 'cf161 written from a narrow request' /run/rudolf/page.log && sed -n '/CONFINE_PURE/,/NULL/p' $SYS/rudolfd-confine-pure.h | grep -q '"clientlog",' && echo 1)" "$c $(cat $T/body) $(tail -2 /run/rudolf/page.log 2>/dev/null)"
# the windows
o=$(win python3 /api/stats); o2=$(win python3 /api/desktop/accounts); o3=$(win python3 /api/ping); o4=$(win python3 /)
ok "14. the page (a window's process in the desktop's group) gets what it asks for, narrow or not (stats; ping and the page itself, whose peer check is made before the narrow confinement), and is refused what only the native shell asks for (desktop/accounts): 403" "$([ "$(echo "$o" | head -1)" = 200 ] && [ "$(echo "$o3" | head -1)" = 200 ] && [ "$(echo "$o4" | head -1)" = 200 ] && echo "$o2" | grep -q '^403 .*does not use' && echo 1)" "$o / $o2 / $o3 / $o4"
ok "...said in the health log" "$(health | grep -q "the desktop's page asked for desktop/accounts, which it never asks for: refused" && echo 1)" "$(health | tail -3)"
ok "...and in the confinement's own log (never trimmed), with rudolfd's start" "$(grep -q 'the page asked for desktop/accounts: refused' /persist/var/log/confine.log /run/rudolf/confine.log 2>/dev/null && grep -q 'confinement on, Landlock ABI' /persist/var/log/confine.log /run/rudolf/confine.log 2>/dev/null && echo 1)" "$(cat /persist/var/log/confine.log /run/rudolf/confine.log 2>/dev/null | tail -3)"
o=$(win $T/rudolf-native /api/desktop/accounts); o2=$(win $T/rudolf-native /api/fs/list)
ok "15. the native shell (rudolf-native) gets its own (desktop/accounts) and is refused the rest (fs/list)" "$([ "$(echo "$o" | head -1)" = 200 ] && echo "$o2" | grep -q '^403' && echo 1)" "$o / $o2"
c=$(api desktop/accounts)
ok "16. root (rudolfd's helpers, the session's scripts) is never checked" "$([ "$c" = 200 ] && echo 1)" "$c"
stop

# on, through rudolf-rootd: a request reaching a helper from the narrow confinement, the helper's own
start "RUDOLFD_CONFINE=on" "$T/rudolf-rootd $T/rudolfd"
rm -f $T/helper.status; c=$(api sys/status)
hs=$(cat $T/helper.status 2>/dev/null); he=$(echo "$hs" | sed -n 's/^CapEff:\s*//p'); hb=$(echo "$hs" | sed -n 's/^CapBnd:\s*//p')
K=$(( (1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<21)|(1<<24) ))
ok "17. through rudolf-rootd: a narrow request (sys/status) reaches the updates helper, which has only its capabilities (bounding too) and its filter" "$([ "$c" = 200 ] && [ -n "$he" ] && [ $(( 0x$he & ~K )) = 0 ] && [ $(( 0x$hb & ~K )) = 0 ] && echo "$hs" | grep -q '^Seccomp:\s*2' && echo 1)" "$c $(cat $T/body) / $hs"
stop

# log
start "RUDOLFD_CONFINE=log" "$T/rudolfd"; M=$(rdpid)
o=$(win python3 /api/desktop/accounts); held; s=$(st $C Seccomp_filters); ce=$(st $C CapEff); kill $HP 2>/dev/null
ok "18. log: the same filters, only logging (the capabilities kept); a window's request it never makes is answered and noted" "$([ "$(st $M Seccomp_filters)" = 1 ] && [ "$s" = 2 ] && [ "$(echo "$o" | head -1)" = 200 ] && health | grep -q 'desktop/accounts, which it never asks for: answered (confinement: log)' && [ "$ce" = "$(st $M CapEff)" ] && echo 1)" "filters=$s eff=$ce $o $(health | tail -2)"
stop

# the switch's sources: the file, the start-up option (here /etc/rudolf/cmdline is not ours: the file is tried)
echo log > /persist/etc/rudolf-confine; start "" "$T/rudolfd"; M=$(rdpid)
ok "19. /persist/etc/rudolf-confine turns it on (log) with nothing else" "$([ "$(st $M Seccomp)" = 2 ] && health | grep -q "confinement is log" && echo 1)" "$(st $M Seccomp) $(health | tail -1)"
stop; echo off > /persist/etc/rudolf-confine; start "" "$T/rudolfd"; M=$(rdpid)
ok "...and off again" "$([ "$(st $M Seccomp)" = 0 ] && echo 1)" "$(st $M Seccomp)"
stop; rm -f /persist/etc/rudolf-confine

echo "$((N-F))/$N passed"; [ $F = 0 ]
