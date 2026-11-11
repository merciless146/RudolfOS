#!/bin/sh
# 1.5.8: the memory guard's "thrashing" rule. Two kernel stall reports within 2.5 s with under 8% left end the largest
# page only when the stall is real (10-second stall average 5% or more), the swap is nearly full, or under 4% is left.
# On the Vivobook a game loading a song (Friday Night Funkin', 1.6 GB) left 205 MiB of 3415 for a moment, with 2.6 GB
# of swap free and a stall average of 0%, and was ended twice.   sh memthrash58_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
gcc -O2 -w -o $W/mg $HERE/../../sys/rudolf-memguard.c || { echo "FAIL build"; exit 1; }
t() { timeout 5 $W/mg --thrash "$@"; }
ok "1. the Vivobook's game: 205 MiB left, swap mostly free, stall 0%: not thrashing (the game is not ended)" $([ "$(t 3415 205 3415 2607 0 1.0)" = 0 ]; echo $?)
ok "2. the same with the stall average at 6%: thrashing" $([ "$(t 3415 205 3415 2607 6 1.0)" = 1 ]; echo $?)
ok "3. under 4% left (120 MiB): thrashing, swap or not" $([ "$(t 3415 120 3415 2607 0 1.0)" = 1 ]; echo $?)
ok "4. the swap nearly full: thrashing" $([ "$(t 3415 205 3415 200 0 1.0)" = 1 ]; echo $?)
ok "5. reports 3 s apart: not" $([ "$(t 3415 205 3415 2607 9 3.0)" = 0 ]; echo $?)
ok "6. 10% left: not (the 8% rule as before)" $([ "$(t 3415 340 3415 100 50 1.0)" = 0 ]; echo $?)
ok "7. no swap at all, 6% left: thrashing (as before)" $([ "$(t 3415 205 0 0 0 1.0)" = 1 ]; echo $?)
echo "$P/$((P+F)) passed"; [ $F = 0 ]
