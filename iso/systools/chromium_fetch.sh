#!/bin/bash
# Fetch the newest stable Chromium for the next Rudolf OS release.   chromium_fetch.sh SOURCE OUTDIR
#   snap        Ubuntu's chromium snap, stable channel (security fixes within days of Chrome stable;
#               built for Ubuntu, like the rest of Rudolf OS). Default.
#   playwright  Playwright's Chromium build, as on the first Rudolf OS discs (updated with Playwright releases)
# Writes OUTDIR in the layout mkrootfs.sh expects (chrome, chrome_crashpad_handler, *.pak, locales/ ...)
# and OUTDIR.version. Network access is needed. GPL-2.0-or-later.
set -e
SRC=${1:-snap}; OUT=$(realpath -m "$2"); rm -rf "$OUT"; mkdir -p "$OUT"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
case "$SRC" in
snap)
	J=$(curl -fsSL -H 'Snap-Device-Series: 16' 'https://api.snapcraft.io/v2/snaps/info/chromium?fields=download,version')
	URL=$(echo "$J" | jq -r '."channel-map"[] | select(.channel.name=="stable" and .channel.architecture=="amd64") | .download.url' | head -1)
	VER=$(echo "$J" | jq -r '."channel-map"[] | select(.channel.name=="stable" and .channel.architecture=="amd64") | .version' | head -1)
	[ -n "$URL" ] && [ "$URL" != null ] || { echo "no stable amd64 chromium snap" >&2; exit 1; }
	curl -fsSL -o "$W/c.snap" "$URL"
	unsquashfs -q -d "$W/s" "$W/c.snap" 'usr/lib/chromium-browser' >/dev/null
	cp -a "$W/s/usr/lib/chromium-browser/." "$OUT/"
	[ -x "$OUT/chrome" ] || { echo "the snap has no chrome binary" >&2; exit 1; }
	# the setuid sandbox helper is not in the snap; Rudolf OS runs Chromium with the namespace sandbox
	echo "${VER%%-*}" > "$OUT.version";;
playwright)
	npx -y playwright@latest install chromium >/dev/null
	D=$(ls -d ~/.cache/ms-playwright/chromium-*/chrome-linux | sort -V | tail -1)
	cp -a "$D/." "$OUT/"
	"$OUT/chrome" --version | awk '{print $2}' > "$OUT.version";;
*) echo "unknown source $SRC" >&2; exit 2;;
esac
echo "chromium $(cat "$OUT.version") from $SRC"
