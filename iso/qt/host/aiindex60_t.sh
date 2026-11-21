#!/bin/sh
# 1.6.0 (docs/plan-1.6.0.md Part 18): what Blitzen looks up before it answers. rudolf-know on test logs, a test
# release (its signed list with the source archive's S row, signed by a key made here), test activity and a stand-in
# web search on this computer; rudolfd's ai/know routes read for what they promise.   sh aiindex60_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys
W=$(mktemp -d); trap 'kill $WP 2>/dev/null; rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
gcc -O2 -Wall -o $W/rk $S/rudolf-know.c -lcrypto -ldl -lm || { echo "FAIL rudolf-know did not build"; exit 1; }
export RUDOLF_KNOW_DIR=$W/kd RUDOLF_KNOW_LOGS=$W/logs; mkdir -p $W/logs
K=$W/rk
js() { python3 -c "import json,sys; d=json.load(sys.stdin); $1"; }

# ---- the logs: read, with what must not be kept left out
cat > $W/logs/health.log <<'X'
08:00 start
08:01 wm_minimise failed for window 0x400001 (the window had gone)
08:02 wifi: connected to Home (psk "hunter22")
08:03 rudolfd: X-Rudolf-Token: abc
08:04 hash 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08 ok
08:05 -----BEGIN PRIVATE KEY-----
X
printf '09:00 Oracle: a page could not be saved (portal request timed out)\n' > $W/logs/session.log
o=$($K build); ok "1. build reads the logs" "$(echo "$o" | grep -q '^logs: 2$' && echo 1)" "$o"
ok "2. a Wi-Fi password, a token and a key are left out; a hash is hidden" "$(! grep -q 'hunter22\|abc$\|BEGIN\|9f86d0' $W/kd/logs.chunks && [ "$(grep -c 'was left out' $W/kd/logs.chunks)" = 3 ] && grep -q 'hash \[hidden\] ok' $W/kd/logs.chunks && echo 1)" "$(cat $W/kd/logs.chunks)"
o=$($K search "why did minimising the window fail")
ok "3. a question finds its log line (wm_minimise failed: by the parts of a name and their endings)" "$(printf "%s\n" "$o" | js 'print(d[0]["src"], d[0]["kind"], d[0]["where"]); assert "wm_minimise failed" in d[0]["text"]' | grep -q '^health.log log line 1$' && echo 1)" "$o"
o=$($K search "the the and of"); ok "4. a question of only small words finds nothing" "$([ "$o" = "[]" ] && echo 1)" "$o"
o=$($K search "saving a page in oracle"); ok "5. the most fitting piece first" "$(printf "%s\n" "$o" | js 'print(d[0]["src"])' | grep -q session.log && echo 1)" "$o"

# ---- an account's activity (1.6.1 Part 27: in the account's own folder, given by rudolfd while it is open: --home)
mkdir -p $W/hA $W/hB $W/hC
printf 'launch\tOpened Oracle\n' | $K activity --home $W/hA; printf 'settings\tChanged the wallpaper to Aurora\n' | $K activity --home $W/hA
printf 'error\tSteam: could not start (exit code 1)\n' | $K activity --home $W/hA
o=$($K list --home $W/hA); ok "6. kept, and listed newest first" "$(printf "%s\n" "$o" | js 'print(len(d), d[0]["kind"], d[0]["item"], d[2]["text"])' | grep -q '^3 error 3 Opened Oracle$' && echo 1)" "$o"
o=$(printf 'settings\tWi-Fi password set to hunter22\n' | $K activity --home $W/hA); ok "7. a line with a password is not kept" "$(echo "$o" | grep -q 'not kept' && ! grep -q hunter22 $W/hA/.rudolf/blitzen/activity.log && echo 1)" "$o"
o=$($K search --home $W/hA "why won't steam start"); ok "8. the account's own activity is searched" "$(printf "%s\n" "$o" | js 'print(d[0]["kind"], d[0]["text"])' | grep -q '^activity Steam' && echo 1)" "$o"
o=$($K search --home $W/hB "why won't steam start"); ok "9. not another account's" "$(! echo "$o" | grep -q Steam && echo 1)" "$o"
o=$($K search "why won't steam start"); ok "10. nor without an account" "$(! echo "$o" | grep -q Steam && echo 1)" "$o"
o=$(printf 'x\ty\n' | $K activity --home hA 2>&1); o2=$(printf 'x\ty\n' | $K activity 2>&1); ok "11. a folder that is not one (relative, none) is refused" "$(echo "$o" | grep -q 'not an account' && echo "$o2" | grep -q "which account's folder" && echo 1)" "$o $o2"
$K forget --home $W/hA --item 2 >/dev/null; o=$($K list --home $W/hA); ok "12. forget one thing" "$(printf "%s\n" "$o" | js 'print(len(d), " ".join(x["kind"] for x in d))' | grep -q '^2 error launch$' && echo 1)" "$o"
$K forget --home $W/hA --day "$(date +%Y-%m-%d)" >/dev/null; o=$($K list --home $W/hA); ok "13. forget a day" "$([ "$o" = "[]" ] && echo 1)" "$o"
printf 'launch\tOpened Notey\n' | $K activity --home $W/hA; $K forget --home $W/hA --all >/dev/null; ok "14. forget everything" "$([ ! -e $W/hA/.rudolf/blitzen/activity.log ] && echo 1)" ""
i=0; while [ $i -lt 3000 ]; do echo "$((1700000000 + i))	launch	Opened an app with a long name number $i ............................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................"; i=$((i+1)); done > $W/hC/.rudolf/blitzen/activity.log
printf 'launch\tOpened the newest\n' | $K activity --home $W/hC; sz=$(wc -c < $W/hC/.rudolf/blitzen/activity.log)
ok "15. at most 2 MB: the oldest dropped, the newest kept" "$([ $sz -lt 2000000 ] && tail -1 $W/hC/.rudolf/blitzen/activity.log | grep -q 'Opened the newest' && ! head -1 $W/hC/.rudolf/blitzen/activity.log | grep -q 'number 0 ' && echo 1)" "$sz"
ok "16. kept for the account alone (0600 file, 0700 folder)" "$([ "$(stat -c %a $W/hC/.rudolf/blitzen/activity.log)" = 600 ] && [ "$(stat -c %a $W/hC/.rudolf/blitzen)" = 700 ] && [ "$(stat -c %a $W/kd)" = 700 ] && [ ! -e $W/kd/acct ] && echo 1)" "$(stat -c %a $W/hC/.rudolf/blitzen/activity.log $W/hC/.rudolf/blitzen $W/kd)"

# ---- the source: the release's signed list, its S row, the archive checked and taken apart
mkdir -p $W/src/rudolf-os-7/iso/sys $W/src/rudolf-os-7/docs $W/src/rudolf-os-7/iso/www $W/sys/a
printf '/* the taskbar */\nstatic void wm_minimise(Client *c)\n{\n\tif (!c->mapped) { log("wm_minimise failed: not mapped"); return; }\n}\n' > $W/src/rudolf-os-7/iso/sys/rudolf-native.c
printf '# Plan\n\nBlitzen looks things up in the source.\n' > $W/src/rudolf-os-7/docs/plan.md
printf '<html>built page: wm_minimise</html>\n' > $W/src/rudolf-os-7/iso/www/index.html
printf 'binary\000stuff wm_minimise' > $W/src/rudolf-os-7/iso/sys/blob.txt
tar -C $W/src -cJf $W/source.tar.xz rudolf-os-7
SHA=$(sha256sum $W/source.tar.xz | cut -c1-64); SZ=$(wc -c < $W/source.tar.xz)
python3 - "$W" "$SHA" "$SZ" <<'X' || { echo "FAIL the test key (python3-cryptography)"; exit 1; }
import sys, base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization as s
w, sha, sz = sys.argv[1:]; k = Ed25519PrivateKey.generate(); o = Ed25519PrivateKey.generate()
pub = lambda x: base64.b64encode(x.public_key().public_bytes(s.Encoding.Raw, s.PublicFormat.Raw)).decode()
open(w + '/keys', 'w').write('# test\n' + pub(k) + '\n')
m = f'rudolf-system 1\nserial 7\nversion 1.6.0.261118\nL\tbase\t1\t{"0"*64}\t../files/7/base.sfs\nS\tsource\t{sz}\t{sha}\thttps://example.invalid/r7/rudolf-os-7-source.tar.xz\n'.encode()
open(w + '/sys/a/manifest.txt', 'wb').write(m); open(w + '/sys/a/manifest.sig', 'wb').write(k.sign(m))
open(w + '/other.sig', 'wb').write(o.sign(m))
X
export RUDOLF_KNOW_KEYS=$W/keys RUDOLF_KNOW_SYSD=$W/sys RUDOLF_KNOW_SLOT=a RUDOLF_KNOW_SOURCE_FILE=$W/source.tar.xz
o=$($K build --source 2>&1); ok "17. the source of the running release is taken apart" "$(echo "$o" | grep -q '^source 7: 2 files$' && echo 1)" "$o"
ok "18. by file and line, without the archive's top folder; not the built page, not a binary file" "$(grep -q "$(printf '\036')iso/sys/rudolf-native.c	line 1" $W/kd/source.chunks && grep -q 'docs/plan.md' $W/kd/source.chunks && ! grep -q 'built page\|blob.txt\|rudolf-os-7/' $W/kd/source.chunks && [ "$(cat $W/kd/source.serial)" = 7 ] && [ ! -e $W/kd/source.tar.xz ] && [ ! -e $W/kd/source.x ] && echo 1)" "$(head -c 400 $W/kd/source.chunks)"
o=$($K build --source 2>&1); ok "19. once per release" "$(echo "$o" | grep -q 'source 7 already kept' && echo 1)" "$o"
o=$($K search "minimise not mapped"); ok "20. a question finds the source and the log together" "$(printf "%s\n" "$o" | js 'print(sorted(x["kind"] for x in d))' | grep -q "'log'.*'source'" && echo 1)" "$o"
o=$($K show iso/sys/rudolf-native.c 4); ok "21. show: the file around a line, numbered" "$(echo "$o" | grep -q '^     4  	if (!c->mapped)' && echo 1)" "$o"
o=$($K show iso/sys/nothing.c 4 2>&1); ok "22. show: a file it does not keep" "$(echo "$o" | grep -q 'not among' && echo 1)" "$o"
rm -f $W/kd/source.serial; cp $W/other.sig $W/sys/a/manifest.sig; o=$($K build --source 2>&1)
ok "23. a list signed by another key: no source fetched" "$(echo "$o" | grep -q 'not signed by Rudolf OS' && [ ! -e $W/kd/source.serial ] && echo 1)" "$o"
printf 'not the archive' > $W/bad.tar.xz
# (a newer release's list, signed by a new test key)
python3 - "$W" "$SHA" "$SZ" <<'X'
import sys, base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization as s
w, sha, sz = sys.argv[1:]; k = Ed25519PrivateKey.generate()
open(w + '/keys', 'w').write(base64.b64encode(k.public_key().public_bytes(s.Encoding.Raw, s.PublicFormat.Raw)).decode() + '\n')
m = f'rudolf-system 1\nserial 8\nS\tsource\t{sz}\t{sha}\thttps://example.invalid/r8/rudolf-os-8-source.tar.xz\n'.encode()
open(w + '/sys/a/manifest.txt', 'wb').write(m); open(w + '/sys/a/manifest.sig', 'wb').write(k.sign(m))
m2 = b'rudolf-system 1\nserial 9\n'; open(w + '/m9.txt', 'wb').write(m2); open(w + '/m9.sig', 'wb').write(k.sign(m2))
X
o=$(RUDOLF_KNOW_SOURCE_FILE=$W/bad.tar.xz $K build --source 2>&1)
ok "24. an archive that does not match its list is not used" "$(echo "$o" | grep -q 'does not match' && grep -q 'wm_minimise' $W/kd/source.chunks && [ ! -e $W/kd/source.tar.xz ] && echo 1)" "$o"
o=$($K build --source 2>&1); ok "25. a newer release: its source again" "$(echo "$o" | grep -q '^source 8: 2 files$' && echo 1)" "$o"
cp $W/m9.txt $W/sys/a/manifest.txt; cp $W/m9.sig $W/sys/a/manifest.sig; o=$($K build --source 2>&1)
ok "26. a release without a source archive says so" "$(echo "$o" | grep -q 'has no source archive' && echo 1)" "$o"
o=$(RUDOLF_KNOW_SLOT= $K build --source 2>&1); ok "27. started from the disc: no source, the logs still" "$(echo "$o" | grep -q '^logs: 2' && echo "$o" | grep -q 'not started from an installed' && echo 1)" "$o"
o=$($K status); ok "28. status" "$(printf "%s\n" "$o" | js 'print(d["source"], d["serial"], d["logs"])' | grep -q '^True 8 True$' && echo 1)" "$o"

# ---- the web: a stand-in search page and its results, on this computer
mkdir -p $W/web/html $W/web/p
cat > $W/web/html/index.html <<'X'
<html><body>
<a class="result__a" href="https://duckduckgo.com/y.js?ad=1">an advert</a>
<a rel="nofollow" class="result__a" href="//duckduckgo.com/l/?uddg=http%3A%2F%2F127.0.0.1%3APORT%2Fp%2Fone.html&amp;rut=x">One</a>
<a rel="nofollow" class="result__a" href="//duckduckgo.com/l/?uddg=http%3A%2F%2F127.0.0.1%3APORT%2Fp%2Ftwo.html&amp;rut=y">Two</a>
<a rel="nofollow" class="result__a" href="//duckduckgo.com/l/?uddg=http%3A%2F%2F127.0.0.1%3APORT%2Fp%2Fone.html&amp;rut=z">One again</a>
<a rel="nofollow" class="result__a" href="//duckduckgo.com/l/?uddg=http%3A%2F%2F127.0.0.1%3APORT%2Fp%2Fgone.html">Gone</a>
</body></html>
X
printf '<html><head><title>Fixing Steam &amp; Proton</title><script>var secret="do not show"</script><style>p{}</style></head><body><nav>Menu Home About</nav><h1>Steam</h1><p>Steam needs 32-bit libraries.</p><p>Run it again.</p><footer>Copyright</footer></body></html>' > $W/web/p/one.html
w=0; T=""; while [ $w -lt 1500 ]; do T="$T word$w"; w=$((w+1)); done; printf '<html><title>Long</title><body><p>%s</p></body></html>' "$T" > $W/web/p/two.html
PORT=$(python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')
sed -i "s/PORT/$PORT/g" $W/web/html/index.html
(cd $W/web && exec python3 -m http.server $PORT --bind 127.0.0.1 >/dev/null 2>&1) & WP=$!
i=0; while ! curl -s -o /dev/null http://127.0.0.1:$PORT/ && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
o=$(RUDOLF_KNOW_SEARCH="http://127.0.0.1:$PORT/html/index.html?q=" RUDOLF_KNOW_PROXY= $K web "steam won't start" 2>&1)
ok "29. the results' pages as text (adverts, repeats and pages that do not answer left out)" "$(printf "%s\n" "$o" | js 'print(len(d), d[0]["title"], "|", d[0]["url"].rsplit("/",1)[1])' | grep -q '^2 Fixing Steam & Proton | one.html$' && echo 1)" "$o"
ok "30. without scripts, styles, menus and footers" "$(printf "%s\n" "$o" | js 'print(d[0]["text"])' | grep -q 'Steam needs 32-bit libraries' && ! echo "$o" | grep -q 'do not show\|Menu Home\|Copyright' && echo 1)" "$o"
ok "31. at most about 1,200 words of a page" "$(printf "%s\n" "$o" | js 'print(len(d[1]["text"].split()))' | grep -q '^1200$' && echo 1)" "$(printf "%s\n" "$o" | js 'print(len(d[1]["text"].split()))')"
o=$(RUDOLF_KNOW_SEARCH="http://127.0.0.1:1/html/?q=" RUDOLF_KNOW_PROXY= $K web --tor "x" 2>&1); ok "32. a search that does not answer says so (and asks about Tor)" "$(echo "$o" | grep -q 'did not answer (is a Tor window open' && echo 1)" "$o"
ok "33. the web is read as nobody" "$(grep -q 'setgid(65534) || setuid(65534)' $S/rudolf-know.c && sed -n '/^static int web(/,/^}/p' $S/rudolf-know.c | grep -n 'setuid(65534)\|fetch(u' | head -1 | grep -q setuid && echo 1)" ""

# ---- rudolfd's routes
I=$S/rudolfd-know.inc
ok "34. the account is the one signed in, never one the page names" "$(grep -q 'slurp(HOME_SESSION' $I && ! grep -q 'qparam(r->query, "acct")' $I && echo 1)" ""
ok "35. activity is kept only while that account's learning is on (off at first)" "$(grep -q 'if (!know_learning(acct)) { reply_ok(r->fd, "not kept: learning is off")' $I && grep -q 'know_learning(acct) ? a1 : a2' $I && echo 1)" ""
ok "36. what was asked goes as one argument, after --, with no shell" "$(grep -q 'execv(argv\[0\]' $I && ! sed -n '/^static void api_ai_know/,/^}/p' $I | grep -q 'run_out\|system(' && grep -q '"--", q, NULL' $I && echo 1)" ""
ok "37. the source is fetched only idle, on mains or a half-full battery, not metered, at the idle priority" "$(grep -q '< 600000) return' $I && grep -q 'cap >= 50' $I && grep -q 'metered) return' $I && grep -q 'nice(19)' $I && grep -q '3 << 13' $I && echo 1)" ""
ok "38. built and kept with the system (ci_release.sh, mkrootfs.sh, arm64-check, base-src)" "$(grep -q 'rudolf-know" sys/rudolf-know.c' $S/../systools/ci_release.sh && grep -q 'sys/rudolfd-know.inc' $S/../systools/ci_release.sh && grep -q 'usr/bin/rudolf-know' $S/../mkrootfs.sh && grep -q 'rudolf-know.c' $S/../../.github/workflows/arm64-check.yml && echo 1)" ""
ok "39. every system release carries its source, in its signed list" "$(grep -q -- '--source "$W/source.tar.xz"' $S/../systools/ci_release.sh && grep -q "rows.append('\\\\t'.join(\['S', 'source'" $S/../systools/make_release.py && echo 1)" ""
echo "$P/$((P+F)) passed"; [ $F = 0 ]
