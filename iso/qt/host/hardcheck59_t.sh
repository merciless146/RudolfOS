#!/bin/sh
# 1.5.9 (plan 15.5): the C programs in sys/ are built hardened (systools/hardflags.sh) by the release
# (ci_release.sh), the full build (mkrootfs.sh) and the ARM64 workflow, and the release's check
# (systools/hardcheck.py) finds the flags on the built program and fails one built without them.
# Needs gcc, readelf, zlib's static library.   sh hardcheck59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../..; R=$I/..; T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 800)"; F=$((F+1)); fi; }
. $I/systools/hardflags.sh
ok "1. the flags: stack protector, FORTIFY_SOURCE=3, PIE, full RELRO, control-flow protection" "$(for f in -fstack-protector-strong -D_FORTIFY_SOURCE=3 -fPIE -pie -Wl,-z,relro,-z,now; do echo "$HARD" | grep -q -- "$f" || exit; done; case "$(gcc -dumpmachine)" in x86_64*) echo "$HARD" | grep -q -- -fcf-protection && echo 1;; *) echo 1;; esac)" "$HARD"
bad=$(for f in $I/systools/ci_release.sh $I/mkrootfs.sh $R/.github/workflows/arm64-check.yml; do grep -nE '^\s*(\. systools/hardflags.sh; )?gcc ' $f | grep 'sys/\|SYS/' | grep -v 'rudolf-kexec' | grep -v '\$HARD' | sed "s|^|$(basename $f):|"; done)
ok "2. every C program the release, the full build and the ARM64 workflow build gets them (rudolf-kexec, without a C library, aside)" "$([ -z "$bad" ] && echo 1)" "$bad"
ok "3. ...and the release and the ARM64 workflow check the built programs" "$(grep -q 'hardcheck.py "$B/rudolf-display" "$B/rudolfd"' $I/systools/ci_release.sh && grep -q 'hardcheck.py "$B/rudolf-webkit" "$B/rudolf-native"' $I/systools/ci_release.sh && grep -q 'hardcheck.py' $R/.github/workflows/arm64-check.yml && echo 1)"
gcc -O2 $HARD -o $T/rudolfd $I/sys/rudolfd.c -Wl,-Bstatic -lz -Wl,-Bdynamic 2>$T/cc || { cat $T/cc | tail -5; }
gcc -O2 $HARD -o $T/rudolf-devlink $I/sys/rudolf-devlink.c -lcrypto -ldl 2>/dev/null
gcc -O2 $HARD_STATIC -o $T/cdp-eval $I/sys/cdp-eval.c 2>/dev/null
o=$(python3 $I/systools/hardcheck.py $T/rudolfd $T/rudolf-devlink $T/cdp-eval); rc=$?
ok "4. rudolfd, rudolf-devlink and the static cdp-eval built so pass the check" "$([ $rc = 0 ] && [ $(echo "$o" | grep -c ': hardened') = 3 ] && echo 1)" "$o"
gcc -O0 -fno-stack-protector -U_FORTIFY_SOURCE -fcf-protection=none -no-pie -Wl,-z,lazy -o $T/plain $I/sys/rudolf-launch.c 2>/dev/null
o=$(python3 $I/systools/hardcheck.py $T/plain); rc=$?
ok "5. one built without them fails the check, naming what it lacks" "$([ $rc = 1 ] && echo "$o" | grep -q 'PIE' && echo "$o" | grep -q 'BIND_NOW' && echo "$o" | grep -q 'stack protector' && echo "$o" | grep -q 'FORTIFY' && echo 1)" "$o"
gcc -O2 $HARD -Wl,-z,lazy -o $T/lazy $I/sys/rudolf-launch.c 2>/dev/null
o=$(python3 $I/systools/hardcheck.py $T/lazy); ok "6. ...even when only BIND_NOW is missing (partial RELRO)" "$(echo "$o" | grep -q 'built without BIND_NOW' && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
