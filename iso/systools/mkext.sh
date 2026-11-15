#!/bin/sh
# Rudolf OS 1.6.0 (docs/plan-1.6.0.md Part 13): builds one signed system extension from its recipe (iso/ext/NAME/recipe.json)
# for one base: its files laid out (rpk-tool.py sysext-recipe) from the recipe's Debian packages (apt-get download,
# Ubuntu 24.04 as the base layer's libraries), anything the base already has left out (an extension may only add: one
# that replaced a file would be left out at every start), every library its programs need found in the base or in the
# extension, its size printed, then signed (rpk-tool.py build) with the key given. Used by .github/workflows/extensions.yml;
# the key is the release key there (RPK_SIGNING_KEY), and only ever a throw-away key anywhere else.
#   sh mkext.sh NAME BASEROOT KEYFILE OUTDIR [DEBS]     (KVER=... for an extension with kernel modules)
# GPL-2.0-or-later.
set -e
I=$(cd "$(dirname "$0")/.." && pwd); N=$1; BR=$2; KEY=$3; OUT=$4; DEBS=${5:-$4/debs}
[ -n "$N" ] && [ -d "$BR" ] && [ -f "$KEY" ] && [ -n "$OUT" ] || { echo "usage: mkext.sh NAME BASEROOT KEYFILE OUTDIR [DEBS]"; exit 2; }
R=$I/ext/$N/recipe.json; [ -f "$R" ] || { echo "mkext: no recipe iso/ext/$N/recipe.json"; exit 1; }
ABI=$(head -1 "$BR/usr/share/rudolf/base-abi" 2>/dev/null); [ -n "$ABI" ] || { echo "mkext: the base has no usr/share/rudolf/base-abi (a release before 1.6.0's extensions)"; exit 1; }
MA=$(gcc -dumpmachine 2>/dev/null || echo x86_64-linux-gnu)
mkdir -p "$OUT" "$DEBS"; W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
for d in $(python3 -c 'import json,sys;print(" ".join(json.load(open(sys.argv[1])).get("debs",[])))' "$R"); do
	ls "$DEBS/${d}"_*.deb >/dev/null 2>&1 || (cd "$DEBS" && apt-get download -q "$d" >/dev/null) || { echo "mkext: $d could not be downloaded"; exit 1; }
done
python3 "$I/pkgtools/rpk-tool.py" sysext-recipe "$R" "$W/pkg" --base-abi "$ABI" ${KVER:+--kernel "$KVER"} --debs "$DEBS"
# what the base has already is left out
(cd "$W/pkg" && find usr lib ! -type d 2>/dev/null) | while read -r f; do
	if [ -e "$BR/$f" ] || [ -L "$BR/$f" ]; then echo "  the base has /$f already: left out of the extension"; rm -f "$W/pkg/$f"; fi; done
# every library its programs need, in the base or in the extension
miss=; for f in $(find "$W/pkg/usr" "$W/pkg/lib" -type f 2>/dev/null); do
	[ "$(head -c 4 "$f" | od -An -c | tr -d ' ')" = "177ELF" ] || continue
	for l in $(objdump -p "$f" 2>/dev/null | awk '/NEEDED/{print $2}'); do
		found=; for d in lib usr/lib lib/$MA usr/lib/$MA lib64; do { [ -e "$BR/$d/$l" ] || [ -e "$W/pkg/$d/$l" ]; } && found=1; done
		[ -n "$found" ] || miss="$miss ${f#$W/pkg/}:$l"; done; done
[ -z "$miss" ] || { echo "mkext: libraries neither in the base nor in the extension:$miss (add their packages to the recipe)"; exit 1; }
KB=$(du -sk "$W/pkg" | cut -f1)
python3 "$I/pkgtools/rpk-tool.py" build "$W/pkg" "$OUT/$N.rpk" --key "$KEY"
echo "::notice title=System extension $N::$KB KB of files, $(($(stat -c %s "$OUT/$N.rpk") / 1024)) KB to download (base $(echo "$ABI" | cut -c1-16))"
