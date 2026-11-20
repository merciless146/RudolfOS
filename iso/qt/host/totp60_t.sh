#!/bin/sh
# 1.6.0 (plan-1.6.1 Part 17, moved to 1.6.0): rudolfd's authenticator codes (rudolfd-desktop.inc dtotp, its own SHA-1)
# against RFC 6238's test values (SHA-1, 6 digits), and the page's (otp60.js otpCode) against the same, in node.
#   sh totp60_t.sh   (gcc, node)
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 ($3)"; fi; }
I=$R/iso/sys/rudolfd-desktop.inc
a=$(grep -n '^typedef struct { uint32_t h\[5\]; uint8_t b\[64\]; uint64_t n; } dsha1;' $I | cut -d: -f1); b=$(grep -n '^static int dsec_id_ok' $I | cut -d: -f1)
{ printf '#include <stdint.h>\n#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n'; sed -n "${a},$((b - 1))p" $I
  cat <<'Y'
int main(void) { const uint8_t k[] = "12345678901234567890"; long long ts[] = {59, 1111111109, 1111111111, 1234567890, 2000000000, 20000000000LL};
  for (int i = 0; i < 6; i++) printf("%06d\n", dtotp(k, 20, ts[i] / 30)); return 0; }
Y
} > $T/t.c
gcc -O2 -w -o $T/t $T/t.c && $T/t > $T/c.txt
# RFC 6238 Appendix B, SHA-1, the last six digits of 94287082, 07081804, 14050471, 89005924, 69279037, 65353130
printf '287082\n081804\n050471\n005924\n279037\n353130\n' > $T/want
ok "1. rudolfd's codes: RFC 6238's six test values" "$(cmp -s $T/c.txt $T/want && echo 1)" "$(tr '\n' ' ' < $T/c.txt)"
node -e "
global.crypto=require('crypto').webcrypto;const src=require('fs').readFileSync('$R/desktop/otp60.js','utf8');
const m=src.match(/async function otpCode[\s\S]*?\n return String[^\n]*\n/);eval(m[0].replace('async function otpCode','globalThis.otpCode=async function'));
(async()=>{const k=new TextEncoder().encode('12345678901234567890');const o=[];for(const t of [59,1111111109,1111111111,1234567890,2000000000,20000000000])o.push(await otpCode(k,Math.floor(t/30)));console.log(o.join('\n'))})()" > $T/p.txt
ok "2. the page's codes: the same" "$(cmp -s $T/p.txt $T/want && echo 1)" "$(tr '\n' ' ' < $T/p.txt)"
node -e "
const src=require('fs').readFileSync('$R/desktop/otp60.js','utf8');eval(src.match(/const OTP_B32=[^\n]*\n/)[0]+src.match(/function otpB32[^\n]*\n/)[0]);
console.log(otpB32(new TextEncoder().encode('12345678901234567890')))" > $T/b.txt
ok "3. the setup key (base32): GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ for RFC 6238's secret" "$(grep -qx GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ $T/b.txt && echo 1)" "$(cat $T/b.txt)"
echo "$P passed, $F failed"; [ $F = 0 ]
