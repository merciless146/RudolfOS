#!/bin/sh
# 1.5.9 (plan 11.1): Chromium comes from the snap only, sturdier: the download retried, then snapd's own download;
# no fallback to another kind of browser (Chrome for Testing reached 1.5.8 that way); never a lower version than the
# release's; a browser that does not say "Chromium" is not taken.   sh chromsrc59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); F=$HERE/../../systools/chromium_fetch.sh; C=$HERE/../../systools/ci_release.sh
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
P=0; N=0; ok() { N=$((N+1)); if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else echo "FAIL $1"; fi; }
# a fake snap: usr/lib/chromium-browser/chrome
mkdir -p "$W/s/usr/lib/chromium-browser"; printf '#!/bin/sh\necho Chromium 158.0.8127.20 snap\n' > "$W/s/usr/lib/chromium-browser/chrome"; chmod +x "$W/s/usr/lib/chromium-browser/chrome"
mksquashfs "$W/s" "$W/fake.snap" -quiet -no-progress -noappend >/dev/null
mkdir "$W/bin"
cat > "$W/bin/curl" <<E
#!/bin/sh
case "\$*" in *snaps/info*) echo '{"channel-map":[{"channel":{"name":"stable","architecture":"amd64"},"version":"158.0.8127.20","download":{"url":"https://x/c.snap"}}]}'; exit 0;; esac
[ -e "$W/curl-fails" ] && exit 52
o=""; while [ \$# -gt 0 ]; do [ "\$1" = -o ] && o=\$2; shift; done; cp "$W/fake.snap" "\$o"
E
cat > "$W/bin/snap" <<E
#!/bin/sh
echo "\$*" > "$W/snap-called"; [ -e "$W/snap-fails" ] && exit 1; cp "$W/fake.snap" c.snap
E
chmod +x "$W/bin/curl" "$W/bin/snap"
PATH="$W/bin:$PATH" bash "$F" snap "$W/o1" >/dev/null 2>&1; r=$?
[ $r = 0 ] && [ -x "$W/o1/chrome" ] && [ "$(cat "$W/o1.version")" = 158.0.8127.20 ] && [ ! -e "$W/snap-called" ]; ok "1. the snap, downloaded: its Chromium and version" $?
touch "$W/curl-fails"
PATH="$W/bin:$PATH" bash "$F" snap "$W/o2" >/dev/null 2>"$W/e2"; r=$?
[ $r = 0 ] && [ -x "$W/o2/chrome" ] && grep -q "stable --basename=c" "$W/snap-called"; ok "2. the download fails: snapd's own download of the same snap" $?
touch "$W/snap-fails"
PATH="$W/bin:$PATH" bash "$F" snap "$W/o3" >/dev/null 2>"$W/e3"; r=$?
[ $r != 0 ] && grep -q "snapd could not download" "$W/e3"; ok "3. both fail: no Chromium, said so" $?
# ci_release.sh: no fallback, a warning, the version and branding rules
! grep -q 'try_chromium playwright' "$C" && grep -q '::warning title=Chromium::Chromium $CH kept' "$C"; ok "4. no fallback to Playwright's browser; the previous Chromium kept, with a warning" $?
V=$(sed -n '/never a lower version/,+2p' "$C" | grep -o '\[ "$(printf.*= "$NV" \]')
lower() { NV=$1 CH=$2 sh -c "$V" && echo newer || echo older; }
[ "$(lower 158.0.8127.20 158.0.8127.18)" = newer ] && [ "$(lower 157.0.8103.38 158.0.8127.18)" = older ] && [ "$(lower 158.0.8127.18 158.0.8127.18)" = newer ] && [ "$(lower 159.0.1.2 158.0.8127.18)" = newer ]; ok "5. the version rule: 157 is older than the release's 158 (kept), 158.x newer or equal is taken" $?
B=$(sed -n '/case "\$BR" in/p' "$C")
brand() { BR=$1; set -- snap; eval "f() { $B
return 0; }"; f >/dev/null 2>&1 && echo taken || echo refused; }
[ "$(brand 'Chromium 158.0.8127.20 snap')" = taken ] && [ "$(brand 'Google Chrome for Testing 158.0.8127.18')" = refused ] && [ "$(brand '')" = refused ]; ok "6. the browser must say Chromium: Chrome for Testing is refused" $?
grep -q 'Local test builds only, never a release' "$F"; ok "7. Playwright's browser is for local test builds only (said where it is chosen)" $?
echo "$P/$N passed"; [ $P = $N ]
