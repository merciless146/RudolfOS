#!/bin/sh
# 1.5.9 (plan 15.5): rudolfd's HTTP request parsing and JSON handling fuzzed on the host, for a bounded time
# (FUZZ_SECONDS, 60 by default), from the corpus kept in fuzz/corpus. With clang's libFuzzer: libFuzzer with
# AddressSanitizer; else (gcc) fuzz/rudolfd_fuzz.c's own mutation fuzzer, coverage-guided, with AddressSanitizer and
# UBSan. Any crash fails the test and its input is kept (FUZZ_KEEP=DIR, else shown). Then each crash found earlier and
# fixed, kept in the corpus, is run once more (fuzz/corpus/fixed-*).   sh fuzz59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/fuzz59.XXXXXX); S=${FUZZ_SECONDS:-60}
trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | tail -c 1500)"; F=$((F+1)); fi; }
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g -O1 -w"
cp -r $HERE/fuzz/corpus $T/corpus
if echo 'int LLVMFuzzerTestOneInput(const char*d,unsigned long n){return 0;}' > $T/p.c && clang -fsanitize=fuzzer $T/p.c -o $T/p 2>/dev/null; then
	clang $SAN -fsanitize=fuzzer -o $T/fz $HERE/fuzz/rudolfd_fuzz.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build (libFuzzer)"; exit 1; }
	echo "libFuzzer with AddressSanitizer, $S s"
	(cd $T && ./fz -max_total_time=$S -timeout=10 -max_len=65536 -artifact_prefix=$T/crash- corpus > $T/log 2>&1); rc=$?
else
	gcc $SAN -DRF_DRIVER -fsanitize-coverage=trace-pc -o $T/fz $HERE/fuzz/rudolfd_fuzz.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build (gcc)"; exit 1; }
	echo "rudolfd_fuzz.c's own mutation fuzzer (no libFuzzer here) with AddressSanitizer and UBSan, $S s"
	ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 $T/fz $T/corpus $S $T/crashes ${FUZZ_SEED:-} > $T/log 2>&1; rc=$?
fi
tail -2 $T/log
CR=$(ls $T/crash-* $T/crashes/crash-* 2>/dev/null)
if [ -n "$CR" ] && [ -n "$FUZZ_KEEP" ]; then mkdir -p "$FUZZ_KEEP"; cp $CR "$FUZZ_KEEP/"; fi
ok "1. ${S} s of fuzzing the request's head, JSON, the page's storage and sjson: no crash, no undefined behaviour" "$([ $rc = 0 ] && [ -z "$CR" ] && echo 1)" "$(grep -m3 'ERROR\|runtime error\|SUMMARY\|round trip\|rudolfd_fuzz:' $T/log) $(for c in $CR; do od -c $c | head -5; done)"
ok "...it ran the whole corpus and more" "$(grep -q 'runs\|stat::number_of_executed_units\|Done [0-9]* runs' $T/log && echo 1)" "$(tail -5 $T/log)"
# what the fuzz test made firmer in the request's head (req_head): a length that is not a length is refused
cat > $T/rh.c <<'X'
#define main rudolfd_main
#include "SYS/rudolfd.c"
#undef main
int main(int c, char **v) { for (int i = 1; i < c; i++) { char h[4096]; snprintf(h, sizeof h, "%s", v[i]); for (char *p = h; (p = strstr(p, "\\r\\n")); ) { p[0] = '\r'; p[1] = '\n'; memmove(p + 2, p + 4, strlen(p + 4) + 1); }
  struct req r; memset(&r, 0, sizeof r); size_t n = 0; int k = req_head(h, &r, &n); printf("%d %zu %s|%s|%s\n", k, n, r.path, r.query, r.tok); } return 0; }
X
sed -i "s|SYS/|$HERE/../../sys/|" $T/rh.c; gcc -O1 -w -fsanitize=address -o $T/rh $T/rh.c -lz 2>$T/cc2 || cat $T/cc2
o=$($T/rh 'GET /api/x?a=1 HTTP/1.1\r\nX-Rudolf-Token: abc\r\nContent-Length: 12' 'POST / HTTP/1.1\r\nContent-Length: -1' 'POST / HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 6' 'POST / HTTP/1.1\r\nContent-Length: 5x' 'POST / HTTP/1.1\r\nContent-Length: 99999999999999999999999' 'GARBAGE')
ok "3. the request's head: path, query, token and length read; a negative, second, unreadable or overflowing length, and a line that is no request, refused" "$(echo "$o" | sed -n 1p | grep -qx '0 12 /api/x|a=1|abc' && [ "$(echo "$o" | sed -n '2,6p' | cut -c1-2 | tr -d '\n')" = "-1-1-1-1-1" ] && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
