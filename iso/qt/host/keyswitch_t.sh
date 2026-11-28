#!/bin/sh
# 1.6.3 Part 0: the release key's replacement (iso/systools/keyswitch.py) -- where it stands, the list it reads, and the
# one change the switch makes. The list in this tree is the state before the switch (the old key first, the new one
# after it), so the test also catches a switch made by accident, and it never touches the real file.
#   sh keyswitch_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); ISO=$(cd "$HERE/../.." && pwd); K=$ISO/systools/keyswitch.py
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
OLD=$(sed -n 's/^#.*//p' "$ISO/trusted-keys.txt" >/dev/null; grep -v '^#' "$ISO/trusted-keys.txt" | grep . | sed -n 1p)
NEW=$(grep -v '^#' "$ISO/trusted-keys.txt" | grep . | sed -n 2p)
gen() { python3 -c 'import base64,os;print(base64.b64encode(os.urandom(32)).decode())'; }

# ---- 1. the state the tree is in
o=$("$K" --status "$ISO/trusted-keys.txt" 2>&1); r=$?
ok "1. the list as it stands: the old key signs, both official keys trusted, and the switch is next" \
  "$([ $r = 0 ] && echo "$o" | grep -q "2 key(s) trusted, the first one signs: $OLD -- the old key" \
     && echo "$o" | grep -q 'both official keys are trusted: yes' \
     && echo "$o" | grep -q 'next, when 1 to 3 hold' && echo 1)" "$o"

# ---- 2. the list a check accepts, and the tool's own wiring
o=$("$K" --check "$ISO/trusted-keys.txt" 2>&1); r=$?
ok "2. --check accepts it, with mkuki.py's bounds (one to four keys, base64, no key twice)" \
  "$([ $r = 0 ] && echo "$o" | grep -q 'keyswitch: OK (0 found)' && echo 1)" "$o"
ok "3. deadcheck.py runs it before every pull request" \
  "$(grep -q 'keyswitch.py.*--check' "$ISO/systools/deadcheck.py" && echo 1)" ""

# ---- 4. what it refuses, case by case
bad() { printf '%s\n' "$2" > "$W/l"; o=$("$K" --check "$W/l" 2>&1); r=$?; ok "$1" "$([ $r = 1 ] && echo "$o" | grep -q "$3" && echo 1)" "$o"; }
bad "4. a key given twice refused" "$OLD
$NEW
$OLD" 'is given twice'
bad "5. a line that is not a public key refused" "$OLD
$NEW
abc" 'is not one Ed25519 public key'
bad "6. five keys refused (recovery.efi carries one to four)" "$OLD
$NEW
$(gen)
$(gen)
$(gen)" 'one to four'
bad "7. a list with no key at all refused" "# only a comment" 'one to four'
bad "8. neither official key refused: every computer would refuse its releases" "$(gen)
$(gen)" 'neither of the two official release keys'
o=$(printf '%s\n' "$OLD" "$NEW" "$(gen)" "$(gen)" > "$W/l"; "$K" --check "$W/l" 2>&1); r=$?
ok "9. four keys (the bound mkuki.py takes) accepted, a stranger among them named but not refused" \
  "$([ $r = 0 ] && echo "$o" | grep -q 'keyswitch: OK' && echo 1)" "$o"

# ---- 10. the switch itself
cp "$ISO/trusted-keys.txt" "$W/t"; S0=$(cksum < "$W/t")
o=$("$K" --switch "$W/t" 2>&1); r=$?
ok "10. without --yes it refuses and writes nothing" \
  "$([ $r = 1 ] && echo "$o" | grep -q 'give --yes' && [ "$(cksum < "$W/t")" = "$S0" ] && echo 1)" "$o"
o=$("$K" --switch --yes "$W/t" 2>&1); r=$?
ok "11. --yes puts the new key first and the old one after it, and the list still checks" \
  "$([ $r = 0 ] && [ "$(grep -v '^#' "$W/t" | grep . | sed -n 1p)" = "$NEW" ] \
     && [ "$(grep -v '^#' "$W/t" | grep . | sed -n 2p)" = "$OLD" ] \
     && "$K" --check "$W/t" >/dev/null 2>&1 && echo 1)" "$o"
S1=$(cksum < "$W/t")
o=$("$K" --status "$W/t" 2>&1)
ok "12. after it the new key signs, and the old one may leave the list later" \
  "$(echo "$o" | grep -q "the first one signs: $NEW -- the new key" && echo "$o" | grep -q 'switched -- the new key signs' \
     && echo "$o" | grep -q 'item 3e' && echo 1)" "$o"
o=$("$K" --switch --yes "$W/t" 2>&1); r=$?
ok "13. a second switch says it is already switched and changes nothing" \
  "$([ $r = 0 ] && echo "$o" | grep -q 'already switched' && [ "$(cksum < "$W/t")" = "$S1" ] && echo 1)" "$o"
printf '%s\n' "$NEW" > "$W/onlynew"; o=$("$K" --switch --yes "$W/onlynew" 2>&1); r=$?
ok "14. nothing to switch when the old key is gone" \
  "$([ $r = 1 ] && echo "$o" | grep -q 'nothing to switch' && [ "$(cat "$W/onlynew")" = "$NEW" ] && echo 1)" "$o"

# ---- 15. the tree is still before the switch
ok "15. this tree's own list is still the old key first, so a switch was not made by accident" \
  "$([ "$(grep -v '^#' "$ISO/trusted-keys.txt" | grep . | sed -n 1p)" = "$OLD" ] && echo 1)" \
  "$(grep -v '^#' "$ISO/trusted-keys.txt" | grep . | head -2)"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
