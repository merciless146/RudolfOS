#!/bin/sh
# 1.6.3 Part 0: the release key's replacement (iso/systools/keyswitch.py) -- where it stands, the list it reads, and the
# one change the switch makes. The gate (--check) names no key, so a rotation after the 2026-11-24 one still passes; the
# tree's own list is the state before this rotation's switch, which catches one made by accident. The real file is never
# touched.
#   sh keyswitch_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); ISO=$(cd "$HERE/../.." && pwd); K=$ISO/systools/keyswitch.py
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
OLD=$(grep -v '^#' "$ISO/trusted-keys.txt" | grep . | sed -n 1p)
NEW=$(grep -v '^#' "$ISO/trusted-keys.txt" | grep . | sed -n 2p)
gen() { python3 -c 'import base64,os;print(base64.b64encode(os.urandom(32)).decode())'; }

# ---- 1. the state the tree is in, and the gate
o=$("$K" --status "$ISO/trusted-keys.txt" 2>&1); r=$?
ok "1. the list as it stands: the old key signs, the rotation is ready, the day's actions named" \
  "$([ $r = 0 ] && echo "$o" | grep -q "2 key(s) trusted, the first one signs: $OLD" \
     && echo "$o" | grep -q 'the 2026-11-24 rotation is ready' && echo "$o" | grep -q -- '--switch --yes' && echo 1)" "$o"
o=$("$K" --check "$ISO/trusted-keys.txt" 2>&1); r=$?
ok "2. --check accepts it, with mkuki.py's bounds (one to four keys, base64, no key twice)" \
  "$([ $r = 0 ] && echo "$o" | grep -q 'keyswitch: OK (0 found)' && echo 1)" "$o"
ok "3. deadcheck.py runs it before every pull request" \
  "$(grep -q 'keyswitch.py.*--check' "$ISO/systools/deadcheck.py" && echo 1)" ""
o=$("$K" --help 2>&1); r=$?
ok "4. --help is the one usage line" "$([ $r = 0 ] && [ "$(echo "$o" | wc -l)" = 1 ] && echo "$o" | grep -q '^usage: keyswitch.py' && echo 1)" "$o"

# ---- 5. what the gate refuses, case by case
bad() { printf '%s\n' "$2" > "$W/l"; o=$("$K" --check "$W/l" 2>&1); r=$?; ok "$1" "$([ $r = 1 ] && echo "$o" | grep -q "$3" && echo 1)" "$o"; }
bad "5. a key given twice refused" "$OLD
$NEW
$OLD" 'is given twice'
bad "6. a line that is not a public key refused" "$OLD
$NEW
abc" 'is not one Ed25519 public key'
bad "7. five keys refused (recovery.efi carries one to four)" "$OLD
$NEW
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
printf '%s\n' "$OLD" "$NEW" "$(gen)" "$(gen)" > "$W/l"; o=$("$K" --check "$W/l" 2>&1); r=$?
ok "11. four keys (mkuki.py's bound) accepted, a stranger among them named but not refused" \
  "$([ $r = 0 ] && echo "$o" | grep -q 'keyswitch: OK' && echo 1)" "$o"

# ---- 12. the switch itself
cp "$ISO/trusted-keys.txt" "$W/t"; S0=$(cksum < "$W/t")
o=$("$K" --switch "$W/t" 2>&1); r=$?
ok "12. without --yes it refuses and writes nothing" \
  "$([ $r = 1 ] && echo "$o" | grep -q 'give --yes' && [ "$(cksum < "$W/t")" = "$S0" ] && echo 1)" "$o"
o=$("$K" --switch --yes --to "$(gen)" "$W/t" 2>&1); r=$?
ok "13. a key the list does not trust is refused, so a later rotation must say which key it means" \
  "$([ $r = 1 ] && echo "$o" | grep -q 'does not trust the key to move first' && [ "$(cksum < "$W/t")" = "$S0" ] && echo 1)" "$o"
o=$("$K" --switch --yes "$W/t" 2>&1); r=$?
ok "14. --yes puts the new key first and the old one after it, and the list still checks" \
  "$([ $r = 0 ] && [ "$(grep -v '^#' "$W/t" | grep . | sed -n 1p)" = "$NEW" ] \
     && [ "$(grep -v '^#' "$W/t" | grep . | sed -n 2p)" = "$OLD" ] \
     && "$K" --check "$W/t" >/dev/null 2>&1 && echo 1)" "$o"
S1=$(cksum < "$W/t"); o=$("$K" --status "$W/t" 2>&1)
ok "15. after it the new key signs, and the old one may leave the list later" \
  "$(echo "$o" | grep -q "the first one signs: $NEW" && echo "$o" | grep -q 'switched: the new key signs' \
     && echo "$o" | grep -q 'item 3e' && echo 1)" "$o"
o=$("$K" --switch --yes "$W/t" 2>&1); r=$?
ok "16. a second switch says it is already switched and changes nothing" \
  "$([ $r = 0 ] && echo "$o" | grep -q 'already switched' && [ "$(cksum < "$W/t")" = "$S1" ] && echo 1)" "$o"

# ---- 17. the tree is still before the switch
ok "17. this tree's own list is still the old key first, so a switch was not made by accident" \
  "$([ "$(grep -v '^#' "$ISO/trusted-keys.txt" | grep . | sed -n 1p)" = "$OLD" ] && [ "$OLD" != "$NEW" ] && echo 1)" \
  "$(grep -v '^#' "$ISO/trusted-keys.txt" | grep . | head -2)"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
