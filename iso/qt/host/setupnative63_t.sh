#!/bin/sh
# 1.6.3 (the owner, 2026-11-28): the session's rule for when the Native desktop may start, from the session's own
# native_setup_done(). "Set up" is "bd95setup" alone (the setup wizard's last step): the desktop page keeps its placeholder
# account ("admin", a password nobody chose) as "bd95user" at its very first start, before the wizard runs, so a rule that
# counted "bd95user" started Native at the next start over a computer that had never been set up: the person was at Native's
# sign-in screen with the placeholder account alone and could not get in. Needs nothing but sh.   sh setupnative63_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/session
[ -f "$S" ] || { echo "FAIL the session is not there ($S)"; exit 1; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
sed -n '/^native_setup_done() {/p' "$S" > $W/f.sh
grep -q '^native_setup_done() {' $W/f.sh || { echo "FAIL native_setup_done is not in the session"; exit 1; }
. $W/f.sh
P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
t() { native_setup_done && echo yes || echo no; }
RUDOLF_LS=$W/ls.json
rm -f $RUDOLF_LS
ok "1. nothing yet (a new stick, a new installation): Standard, whose setup wizard makes the first account" $([ "$(t)" = no ]; echo $?)
printf '{"data":{"bd95user":"{\\"accts\\":[{\\"n\\":\\"admin\\",\\"h\\":\\"71c54994464eac2567702987533301a6714931f94d9d3526ba9ccaa7561ea18c\\",\\"r\\":\\"admin\\"}]}"}}\n' > $RUDOLF_LS
ok "2. only the page's placeholder account (written at its first start, before the wizard): still Standard, not Native's sign-in screen" $([ "$(t)" = no ]; echo $?) "$(cat $RUDOLF_LS)"
printf '{"data":{"other":"1"}}\n' > $RUDOLF_LS
ok "3. the desktop's data without setup or accounts: still Standard" $([ "$(t)" = no ]; echo $?)
printf '{"data":{"bd95setup":"1","bd95user":"{}"}}\n' > $RUDOLF_LS
ok "4. set up (the wizard's last step): Native at the next start, the choice install.sh made kept" $([ "$(t)" = yes ]; echo $?)
printf '{"data":{"bd95setup":"1","bd95user":"{\\"accts\\":[{\\"n\\":\\"Ann\\",\\"h\\":\\"p2$x$y\\",\\"r\\":\\"admin\\",\\"id\\":\\"uAnn\\"}]}"}}\n' > $RUDOLF_LS
ok "5. ...with the account the person made: Native" $([ "$(t)" = yes ]; echo $?)
# the page's own rule is the same one (a1y25.js firstRun: "bd95setup" alone), or a computer set up with the wizard would be
# sent back to it at every start
ok "6. the wizard's own rule is \"bd95setup\" alone, which the session now reads the same way" $(grep -q "function firstRun(){if(ld('bd95setup',0)" $HERE/../../../desktop/a11y25.js; echo $?)
echo "$P/$((P+F)) passed"; [ $F = 0 ]
