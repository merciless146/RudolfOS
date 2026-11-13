#!/bin/sh
# 1.5.9 (plan 9.2b): the memory guard warns before memory is critical: under 12% available (whatever the swap holds:
# with zram as large as the memory it stays mostly free) it tells the desktop which page is the largest ("memory warn PID MB"), once, again only after
# memory was back above 20%; it changes nothing about when a page is ended.   sh memwarn59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); C=$HERE/../../sys/rudolf-memguard.c; W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
gcc -O2 -w -o $W/mg "$C" || { echo "FAIL build"; exit 1; }
t() { timeout 5 $W/mg --warn "$@"; }
ok "1. FNF on the Vivobook before the end: 340 MiB of 3415 left, 300 of 1707 MiB swap free: warn" $([ "$(t 3415 340 1707 300)" = 1 ]; echo $?)
ok "2. 15% left: not yet" $([ "$(t 3415 512 1707 300)" = 0 ]; echo $?)
ok "3. 10% left with the swap mostly free (zram on Automatic, FNF on 2026-11-12): warn" $([ "$(t 3415 340 3415 2387)" = 1 ]; echo $?)
ok "4. no swap at all, 10% left: warn" $([ "$(t 3415 340 0 0)" = 1 ]; echo $?)
grep -q '"memory warn %d %lld\\n", w->pid, w->rss >> 20' "$C"; ok "5. the desktop is told the largest page ('memory warn PID MB' in the events)" $?
grep -q 'if (warned && avail > total / 5) warned = 0;' "$C" && grep -q '!crit && !off && !warned && warn_due' "$C"; ok "6. once, and again only after memory was back above 20%; not while critical (the page is ended then)" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
