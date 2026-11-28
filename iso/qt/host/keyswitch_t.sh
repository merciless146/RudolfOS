#!/bin/sh
# 1.6.3 Part 0: the release key's replacement (iso/systools/keyswitch.py) -- where it stands, the list it reads, and the
# one change the switch makes. The gate (--check) names no key, so a rotation after the 2026-11-24 one still passes, and
# no case here asserts which side of the switch the tree is on: the switch's own pull request has to pass this test too.
# What it does assert is that the tool and the list agree. The real file is never touched.
#   sh keyswitch_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); ISO=$(cd "$HERE/../.." && pwd); K=$ISO/systools/keyswitch.py
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
K1=$(grep -v '^#' "$ISO/trusted-keys.txt" | grep . | sed -n 1p)   # the key that signs, whichever side of the switch
K2=$(grep -v '^#' "$ISO/trusted-keys.txt" | grep . | sed -n 2p)   # the other one, while both are trusted
gen() { python3 -c 'import base64,os;print(base64.b64encode(os.urandom(32)).decode())'; }

# ---- 1. the state the tree is in, and the gate
o=$("$K" --status "$ISO/trusted-keys.txt" 2>&1); r=$?
ok "1. the list as it stands: the first key signs, and the rotation is either ready or already switched" \
  "$([ $r = 0 ] && echo "$o" | grep -q "trusted, the first one signs: $K1" \
     && { echo "$o" | grep -q 'the 2026-11-24 rotation is ready' || echo "$o" | grep -q 'switched: the new key signs'; } \
     && echo 1)" "$o"
o=$("$K" --check "$ISO/trusted-keys.txt" 2>&1); r=$?
ok "2. --check accepts it, with mkuki.py's bounds (one to four keys, base64, no key twice)" \
  "$([ $r = 0 ] && echo "$o" | grep -q 'keyswitch: OK (0 found)' && echo 1)" "$o"
ok "3. deadcheck.py runs it before every pull request" \
  "$(grep -q 'keyswitch.py.*--check' "$ISO/systools/deadcheck.py" && echo 1)" ""
o=$("$K" --help 2>&1); r=$?
ok "4. --help is the one usage line" "$([ $r = 0 ] && [ "$(echo "$o" | wc -l)" = 1 ] && echo "$o" | grep -q '^usage: keyswitch.py' && echo 1)" "$o"

# ---- 5. what the gate refuses, case by case
bad() { printf '%s\n' "$2" > "$W/l"; o=$("$K" --check "$W/l" 2>&1); r=$?; ok "$1" "$([ $r = 1 ] && echo "$o" | grep -q "$3" && echo 1)" "$o"; }
bad "5. a key given twice refused" "$K1
$K2
$K1" 'is given twice'
bad "6. a line that is not a public key refused" "$K1
$K2
abc" 'is not one Ed25519 public key'
bad "7. five keys refused (recovery.efi carries one to four)" "$K1
$K2
$(gen)
$(gen)
$(gen)" 'one to four'
bad "8. a list with no key at all refused" "# only a comment" 'one to four'

# ---- 9. the gate names no key: a rotation after this one still passes, and --status says so
A=$(gen); B=$(gen)
printf '%s\n' "$A" "$B" > "$W/next"; o=$("$K" --check "$W/next" 2>&1); r=$?
ok "9. two keys of a later rotation pass the gate unchanged" "$([ $r = 0 ] && echo "$o" | grep -q 'OK' && echo 1)" "$o"
o=$("$K" --status "$W/next" 2>&1)
ok "10. ...and --status names them as another rotation and says what to change when a new one begins" \
  "$(echo "$o" | grep -q 'the 2026-11-24 pair is no longer in the list' && echo "$o" | grep -q 'change ROTATION' && echo 1)" "$o"
printf '%s\n' "$K1" "$K2" "$(gen)" "$(gen)" > "$W/l"; o=$("$K" --check "$W/l" 2>&1); r=$?
ok "11. four keys (mkuki.py's bound) accepted, a stranger among them named but not refused" \
  "$([ $r = 0 ] && echo "$o" | grep -q 'keyswitch: OK' && echo 1)" "$o"
printf '%s\n' "$(gen)" "$K2" "$K1" > "$W/lead"; o=$("$K" --status "$W/lead" 2>&1)
ok "11a. a later key leading while this pair is still trusted: --status does not claim the new key signs" \
  "$(echo "$o" | grep -q 'pair is in the list but does not sign' && ! echo "$o" | grep -q 'switched: the new key signs' && echo 1)" "$o"

# ---- 12. the switch itself, on two made-up keys so that no case depends on where this tree stands
A=$(gen); B=$(gen); printf '%s\n' "$A" "$B" > "$W/t2"; S0=$(cksum < "$W/t2")
o=$("$K" --switch --to "$B" "$W/t2" 2>&1); r=$?
ok "12. without --yes it refuses and writes nothing" \
  "$([ $r = 1 ] && echo "$o" | grep -q 'give --yes' && [ "$(cksum < "$W/t2")" = "$S0" ] && echo 1)" "$o"
o=$("$K" --switch --yes --to 2>&1); r=$?
ok "13a. --to with no key after it refused, rather than switching to the default" \
  "$([ $r = 1 ] && echo "$o" | grep -q -- '--to needs the public key to move first' && echo 1)" "$o"
o=$("$K" --switch --yes --to "$(gen)" "$W/t2" 2>&1); r=$?
ok "13. a key the list does not trust is refused, so a later rotation must say which key it means" \
  "$([ $r = 1 ] && echo "$o" | grep -q 'does not trust the key to move first' && [ "$(cksum < "$W/t2")" = "$S0" ] && echo 1)" "$o"
o=$("$K" --switch --yes --to "$B" "$W/t2" 2>&1); r=$?
ok "14. --yes puts the key it is given first and keeps the one that signed, and the list still checks" \
  "$([ $r = 0 ] && [ "$(grep -v '^#' "$W/t2" | grep . | sed -n 1p)" = "$B" ] \
     && [ "$(grep -v '^#' "$W/t2" | grep . | sed -n 2p)" = "$A" ] \
     && "$K" --check "$W/t2" >/dev/null 2>&1 && echo 1)" "$o"
S1=$(cksum < "$W/t2"); o=$("$K" --status "$W/t2" 2>&1)
ok "15. after it the key given is the one that signs" \
  "$(echo "$o" | grep -q "the first one signs: $B" && echo "$o" | grep -q 'another key\|the old key\|the new key' && echo 1)" "$o"
o=$("$K" --switch --yes --to "$B" "$W/t2" 2>&1); r=$?
ok "16. a second switch says it is already switched and changes nothing" \
  "$([ $r = 0 ] && echo "$o" | grep -q 'already switched' && [ "$(cksum < "$W/t2")" = "$S1" ] && echo 1)" "$o"

# ---- 17. the tool and the tree's own list agree, whichever side of the switch it is on
o=$("$K" --status "$ISO/trusted-keys.txt" 2>&1)
ok "17. the key --status calls signing is the list's own first line" \
  "$(echo "$o" | grep -q "the first one signs: $K1" && [ "$K1" != "$K2" ] && echo 1)" "$o"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
