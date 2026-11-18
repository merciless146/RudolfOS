#!/bin/bash
# 1.6.0 (docs/plan-1.6.0.md Part 19 item 2): a fork's build points at the fork. /etc/rudolf/origin (iso/systools/origin.sh,
# iso/sys/origin.h, rudolfd's origin, desktop/origin60.js): the update channels, Blitzen's model, Debian's base, the devlink's
# put, the release downloads, Make a stick, system reports and About; nothing else names the official repository.
#   bash origin60_t.sh
set -u; cd "$(dirname "$0")"; I=$(cd ../.. && pwd); R=$(cd ../../.. && pwd); T=$(mktemp -d); P=0; F=0
trap 'rm -rf "$T"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
O=$I/systools/origin.sh

# ---- the file
o=$(env -u GITHUB_REPOSITORY -u GITHUB_REPOSITORY_ID sh $O)
ok "1. built outside GitHub: the official repository" "$([ "$o" = "$(printf 'repo=merciless146/RudolfOS\nrepo_id=1414456577\npages=https://merciless146.github.io/RudolfOS')" ] && echo 0)" "$o"
o=$(GITHUB_REPOSITORY=Alex/RudolfOS GITHUB_REPOSITORY_ID=123456 sh $O)
ok "2. a fork's workflow: the fork (its Pages address in lower case)" "$([ "$o" = "$(printf 'repo=Alex/RudolfOS\nrepo_id=123456\npages=https://alex.github.io/RudolfOS')" ] && echo 0)" "$o"
for bad in 'a/b/c' 'noslash' 'a/../b' 'a/b;rm' '/b'; do o=$(GITHUB_REPOSITORY="$bad" sh $O 2>&1); [ $? != 0 ] || { ok "3. $bad refused" 1 "$o"; continue; }; done; ok "3. a repository that is not OWNER/NAME is refused" 0
o=$(GITHUB_REPOSITORY=a/b GITHUB_REPOSITORY_ID=12x sh $O 2>&1); ok "...and an id that is not a number" "$([ $? != 0 ] && echo 0)" "$o"

# ---- origin.h
cat > $T/t.c <<X
#include "$I/sys/origin.h"
int main(int c, char **v) { (void)c; char r[300]; origin_release(r, sizeof r, "debian-13"); printf("%s|%s|%s|%s\n", origin_get("repo"), origin_get("repo_id"), origin_get("pages"), r); return 0; }
X
gcc -Wall -Werror -o $T/t $T/t.c || { echo "FAIL origin.h does not build"; exit 1; }
o=$(RUDOLF_ORIGIN_FILE=$T/none $T/t); ok "4. origin.h without the file: the official repository" "$([ "$o" = "merciless146/RudolfOS|1414456577|https://merciless146.github.io/RudolfOS|https://github.com/merciless146/RudolfOS/releases/download/debian-13" ] && echo 0)" "$o"
GITHUB_REPOSITORY=Alex/RudolfOS GITHUB_REPOSITORY_ID=123456 sh $O > $T/fork
o=$(RUDOLF_ORIGIN_FILE=$T/fork $T/t); ok "5. ...with a fork's file: the fork's, and its releases" "$([ "$o" = "Alex/RudolfOS|123456|https://alex.github.io/RudolfOS|https://github.com/Alex/RudolfOS/releases/download/debian-13" ] && echo 0)" "$o"
printf 'repo=evil/../x\nrepo_id=1x\npages=http://plain.example\n' > $T/bad
o=$(RUDOLF_ORIGIN_FILE=$T/bad $T/t); ok "6. ...values that are not addresses are not used" "$(echo "$o" | grep -q '^merciless146/RudolfOS|1414456577|https://merciless146.github.io/RudolfOS|' && echo 0)" "$o"
printf 'repo=Alex/RudolfOS\npages=https://alex.github.io/RudolfOS/\nrepo=a b/c\n' > $T/half
o=$(RUDOLF_ORIGIN_FILE=$T/half $T/t); ok "7. ...a missing key is the official one; a slash at the end taken off" "$(echo "$o" | grep -q '^Alex/RudolfOS|1414456577|https://alex.github.io/RudolfOS|' && echo 0)" "$o"

# ---- the programs that use it
RU=$T/ru; gcc -O2 -w -o $RU $I/sys/rudolf-update.c -lcrypto -ldl || exit 1
mkdir -p $T/media/rudolf $T/sys; printf 'serial 2026110100\nversion 1.6.0\n' > $T/media/rudolf/release; echo disc > $T/slot; echo "rudolf.boot=uefi" > $T/cmd
printf "/dev/nvme0n1 $T/media iso9660 ro 0 0\n" > $T/mounts; : > $T/keys; printf 'url=https://alex.github.io/RudolfOS/stable\n' > $T/conf
run() { RUDOLF_ORIGIN_FILE=$T/fork RU_NOAUTOFETCH=1 RU_NOESP=1 RU_SYSD=$T/sys RU_MEDIA=$T/media RU_CONF=$T/conf RU_KEYS=$T/keys RU_EXTRA=$T/extra RU_SLOT=$T/slot RU_CMDLINE=$T/cmd RU_MOUNTS=$T/mounts RU_SYSBLOCK=$T/sysblock "$@" 2>&1; }
o=$(run $RU set channel beta); ok "8. Get beta versions on a fork's build: the fork's beta channel" "$(grep -qx 'url=https://alex.github.io/RudolfOS/beta' $T/conf && echo 0)" "$o $(cat $T/conf)"
o=$(run $RU set channel stable); ok "...and back: the fork's stable channel" "$(grep -qx 'url=https://alex.github.io/RudolfOS/stable' $T/conf && echo 0)" "$o $(cat $T/conf)"
ok "9. the recovery system and Return to official stay official (OFFICIAL_URL)" "$(sed -n '/^static int cmd_recovery_fetch/,/^}/p' $I/sys/rudolf-update.c | grep -q 'OFFICIAL_URL' && ! sed -n '/^static int cmd_recovery_fetch/,/^}/p' $I/sys/rudolf-update.c | grep -q origin_get && echo 0)"
ok "10. Blitzen's model and Debian's base: this build's releases" "$(grep -q 'origin_release(d, sizeof d, "blitzen-1")' $I/sys/rudolf-ai.c && grep -q 'origin_release(u, n, "debian-13")' $I/sys/rudolf-debian.c && echo 0)"
cat > $T/p.c <<X
#define main devlink_main
#include "$I/sys/rudolf-devlink.c"
#undef main
int main(void) { const char *u[] = { "https://github.com/merciless146/RudolfOS/releases/download/r1/x", "https://github.com/Alex/RudolfOS/releases/download/r1/x",
  "https://github.com/Other/RudolfOS/releases/download/r1/x", "https://github.com/Alex/RudolfOS/releases/download/../../x", NULL };
  for (int i = 0; u[i]; i++) printf("%d", release_url_ok(u[i])); puts(""); return 0; }
X
gcc -w -o $T/p $T/p.c -lcrypto -ldl && o=$(RUDOLF_ORIGIN_FILE=$T/fork $T/p)
ok "11. the devlink's put --url: the official releases and this build's own, nobody else's" "$([ "${o:-}" = 1100 ] && echo 0)" "${o:-not built}"
ok "12. rudolfd and the updates helper refuse the fork's beta as a plain address too" "$(grep -q 'strstr(v, ob)' $I/sys/rudolfd.c && grep -q 'strstr(a\[1\], ob)' $I/sys/rudolf-updates.c && echo 0)"
ok "13. rudolfd's origin: repo, repo_id, pages, official, library" "$(sed -n '/if (!strcmp(p, "origin"))/,/return; }/p' $I/sys/rudolfd.c | grep -q '"official' && grep -q 'rpk-sources' <(sed -n '/if (!strcmp(p, "origin"))/,/reply_json/p' $I/sys/rudolfd.c) && echo 0)"
ok "14. written into every build (ci_release.sh and mkrootfs.sh), and part of base-src" "$(grep -q 'sh systools/origin.sh | sudo tee "$W/broot/etc/rudolf/origin"' $I/systools/ci_release.sh && grep -q 'origin.sh" > etc/rudolf/origin' $I/mkrootfs.sh && grep -q 'sys/origin.h systools/origin.sh' $I/systools/ci_release.sh && echo 0)"

# ---- nothing else names the official repository in a string (outside the recovery path and the official constants)
L=$(grep -rn 'merciless146' $I/sys/*.c $I/sys/*.inc $R/desktop/*.js 2>/dev/null | grep -v ':[0-9]*:\s*\(#\|//\|/\*\| \*\)' | grep "[\"'\`][^\"'\`]*merciless146" \
  | grep -v 'define OFFICIAL_URL\|define DEFAULT_URL\|merciless146.github.io/RudolfOS/beta\|DL_REPO_ID=\|SR_REPO = \|BF_RELEASES=\|LIB_URL=\|const ORIGIN=' )
ok "15. no other official address in the system programs and the page" "$([ -z "$L" ] && echo 0)" "$L"
ok "16. the page: origin60.js early, and its users" "$(grep -q "'core25.js', 'origin60.js'" $R/desktop/build19.py && grep -q 'ORIGIN.repo_id' $R/desktop/dl27.js && grep -q 'ORIGIN.repo' $R/desktop/tm26.js && grep -q 'ORIGIN.repo' $R/desktop/stick30.js && grep -q 'LIB_URL=j.library' $R/desktop/origin60.js && grep -q 'from github.com/' $R/desktop/devmode59.js && echo 0)"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
