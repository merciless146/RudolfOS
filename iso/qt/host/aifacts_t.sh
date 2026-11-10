#!/bin/sh
# 1.5.7 (plan stage 2): rudolfd's ai/facts gives Blitzen the last lines of the computer's logs, read-only, with lines
# that carry a password, token or key left out and key-like strings hidden. The functions from rudolfd.c, on test
# logs.   sh aifacts_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); C=$HERE/../../sys/rudolfd.c
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
{ printf '#include <%s>\n' stdio.h stdlib.h string.h stdarg.h ctype.h fcntl.h unistd.h
  sed -n '/^typedef struct { char \*b; size_t n, cap; } str;/,/^static int run(/p' "$C" | sed '$d'
  sed -n '/^static int facts_secret_line/,/^static void api_ai_facts/p' "$C" | sed '$d'
  printf '%s\n' 'int main(int c,char**v){str s={0};sadd(&s,"{\"x\":0");facts_add(&s,"log",v[1],atoi(v[2]));sadd(&s,"}");puts(s.b);return 0;}'; } > $W/t.c
grep -q facts_tail $W/t.c && gcc -w -o $W/t $W/t.c || { echo "FAIL facts_tail not found or not built"; exit 1; }
cat > $W/a.log <<'X'
08:00 start
08:01 wifi: connected to Home (psk "hunter22")
08:02 rudolfd: signed in, X-Rudolf-Token: abc
08:03 hash 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08 ok
08:04 Password for root changed
08:05 a path /persist/home/rudolf/Documents/a_long_folder_name_without_digits_here/x
08:06 -----BEGIN PRIVATE KEY-----
08:07 MC4CAQAwBQYDK2VwBCIEIPq1abc1234567890abcdefghijklmn
08:08 the end
X
o=$($W/t $W/a.log 100)
J=$(echo "$o" | python3 -c 'import json,sys; print(json.load(sys.stdin)["log"])' 2>&1)
ok "1. the reply is JSON with the log" "$(echo "$J" | grep -q '08:00 start' && echo 1)" "$o"
ok "2. lines with a Wi-Fi password, a token, a password or a key are left out" "$([ "$(echo "$J" | grep -c 'was left out')" = 4 ] && ! echo "$J" | grep -qi 'hunter22\|abc$\|BEGIN' && echo 1)" "$J"
ok "3. a hash and a key-like string are hidden" "$(echo "$J" | grep -q '08:03 hash \[hidden\] ok' && echo "$J" | grep -q '08:07 \[hidden\]' && ! echo "$J" | grep -q '9f86d0' && echo 1)" "$J"
ok "4. long names without digits stay (paths are readable)" "$(echo "$J" | grep -q 'a_long_folder_name_without_digits_here' && echo 1)" "$J"
ok "5. only the last n lines" "$(J2=$($W/t $W/a.log 2 | python3 -c 'import json,sys; print(json.load(sys.stdin)["log"])'); [ "$(echo "$J2" | wc -l)" = 2 ] && echo "$J2" | tail -1 | grep -q 'the end' && echo 1)" ""
ok "6. a missing log is null" "$($W/t $W/none 10 | grep -q '"log":null' && echo 1)" ""
i=0; while [ $i -lt 5000 ]; do echo "line $i of a long log ......................................"; i=$((i+1)); done > $W/big.log
J=$($W/t $W/big.log 100 | python3 -c 'import json,sys; print(json.load(sys.stdin)["log"])')
ok "7. a large log: the last 100 whole lines, read from its end" "$([ "$(echo "$J" | wc -l)" = 100 ] && echo "$J" | head -1 | grep -q '^line 4900 ' && echo 1)" "$(echo "$J" | head -2)"
printf 'one\ntwo' > $W/nonl.log; J=$($W/t $W/nonl.log 1 | python3 -c 'import json,sys; print(json.load(sys.stdin)["log"])')
ok "8. a last line without a newline" "$([ "$J" = two ] && echo 1)" "$J"
ok "9. ai/facts answers without Blitzen installed, and only reads" "$(grep -q 'if (!strcmp(what, "facts")) { api_ai_facts(r); return; }' "$C" && sed -n '/^static void api_ai(/,+3p' "$C" | grep -q facts && ! sed -n '/^static char \*facts_tail/,/^static void api_ai(/p' "$C" | grep -q 'O_WRONLY\|unlink\|rename' && echo 1)" ""
echo "$P/$((P+F)) passed"; [ $F = 0 ]
