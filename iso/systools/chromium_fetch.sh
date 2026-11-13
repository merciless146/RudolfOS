#!/bin/bash
# Fetch the newest stable Chromium for the next Rudolf OS release.   chromium_fetch.sh SOURCE OUTDIR
#   snap        Ubuntu's chromium snap, stable channel (security fixes within days of Chrome stable;
#               built for Ubuntu, like the rest of Rudolf OS). Default.
#   playwright  Playwright's browser: since Playwright 1.5x Google's "Chrome for Testing", a Chrome build for tests,
#               not open-source Chromium. Local test builds only, never a release (1.5.9, docs/plan-1.5.9.md 11.1).
# Writes OUTDIR in the layout mkrootfs.sh expects (chrome, chrome_crashpad_handler, *.pak, locales/ ...)
# and OUTDIR.version. Network access is needed. GPL-2.0-or-later.
set -e
SRC=${1:-snap}; OUT=$(realpath -m "$2"); rm -rf "$OUT"; mkdir -p "$OUT"
# (1.5.9, plan Part 3: the build machine's architecture: amd64, or arm64 on GitHub's ARM64 runner; CHROMIUM_ARCH overrides)
ARCH=${CHROMIUM_ARCH:-$(dpkg --print-architecture 2>/dev/null || echo amd64)}
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
case "$SRC" in
snap)
	# (1.5.8: retried; on 2026-11-12 the store's TLS handshake timed out once and the release fell back to Playwright)
	J=$(curl -fsSL --retry 4 --retry-delay 10 --retry-all-errors --connect-timeout 30 -H 'Snap-Device-Series: 16' 'https://api.snapcraft.io/v2/snaps/info/chromium?fields=download,version')
	URL=$(echo "$J" | jq -r '."channel-map"[] | select(.channel.name=="stable" and .channel.architecture==$a) | .download.url' --arg a "$ARCH" | head -1)
	VER=$(echo "$J" | jq -r '."channel-map"[] | select(.channel.name=="stable" and .channel.architecture==$a) | .version' --arg a "$ARCH" | head -1)
	[ -n "$URL" ] && [ "$URL" != null ] || { echo "no stable $ARCH chromium snap" >&2; exit 1; }
	# (1.5.9: on 2026-11-12 the download itself failed four builds running (curl 52, an empty reply): longer retries,
	# then snapd's own download as a second way to the same snap)
	curl -fsSL --retry 6 --retry-delay 20 --retry-all-errors --connect-timeout 30 --max-time 900 -o "$W/c.snap" "$URL" || {
		echo "the snap's download failed; trying snapd" >&2; rm -f "$W/c.snap"
		( cd "$W" && UBUNTU_STORE_ARCH=$ARCH timeout 900 snap download chromium --channel=stable --basename=c >/dev/null ) || { echo "snapd could not download the snap either" >&2; exit 1; }
		[ -s "$W/c.snap" ] || { echo "snapd gave no snap" >&2; exit 1; }; }
	unsquashfs -q -d "$W/s" "$W/c.snap" 'usr/lib/chromium-browser' >/dev/null
	cp -a "$W/s/usr/lib/chromium-browser/." "$OUT/"
	[ -x "$OUT/chrome" ] || { echo "the snap has no chrome binary" >&2; exit 1; }
	# the setuid sandbox helper is not in the snap; Rudolf OS runs Chromium with the namespace sandbox
	echo "${VER%%-*}" > "$OUT.version";;
playwright)
	npx -y playwright@latest install chromium >/dev/null
	# (1.5.8: newer Playwright unpacks to chrome-linux64. Found nothing, the empty name made this copy the runner's
	# whole root file system, "/.", until it failed: now both names, and nothing is copied unless a chrome is there)
	D=$(ls -d ~/.cache/ms-playwright/chromium-*/chrome-linux ~/.cache/ms-playwright/chromium-*/chrome-linux64 2>/dev/null | sort -V | tail -1)
	[ -n "$D" ] && [ -x "$D/chrome" ] || { echo "Playwright's Chromium was not found in ~/.cache/ms-playwright" >&2; exit 1; }
	cp -a "$D/." "$OUT/"
	# (newer Playwright brings "Google Chrome for Testing 1xx.x.x.x": the version is the dotted number, not the 2nd word)
	"$OUT/chrome" --version | grep -oE '[0-9]+(\.[0-9]+){3}' | head -1 > "$OUT.version"
	[ -s "$OUT.version" ] || { echo "Playwright's Chromium gave no version" >&2; exit 1; };;
*) echo "unknown source $SRC" >&2; exit 2;;
esac
echo "chromium $(cat "$OUT.version") from $SRC"
