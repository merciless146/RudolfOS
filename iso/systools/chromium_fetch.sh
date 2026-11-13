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
	# 1.5.9 (plan Part 3): a snap built on a newer Ubuntu than Rudolf OS's (the arm64 snap moved to core26 first and
	# needs GLIBC_2.43; Rudolf OS's layer has Ubuntu 24.04's 2.39) brings its own C library: the one of the snap's base
	# (core26, from the same store), in OUTDIR/glibc; chrome and its crash handler load it (patchelf: their program
	# loader and an RPATH /opt/chromium/glibc). The other libraries in the layer work with the newer C library.
	NEED=$(strings -a "$OUT/chrome" "$OUT"/*.so 2>/dev/null | sed -n 's/^GLIBC_2\.\([0-9]*\)$/\1/p' | sort -n | tail -1)
	HAVE=$(getconf GNU_LIBC_VERSION | sed -n 's/^glibc 2\.\([0-9]*\).*/\1/p')
	if [ "${NEED:-0}" -gt "${HAVE:-99}" ]; then
		echo "the snap needs glibc 2.$NEED, this system has 2.$HAVE: bundling its base's C library" >&2
		unsquashfs -q -d "$W/m" "$W/c.snap" meta/snap.yaml >/dev/null
		BASE=$(sed -n 's/^base: *//p' "$W/m/meta/snap.yaml"); [ -n "$BASE" ] || { echo "the snap names no base" >&2; exit 1; }
		JB=$(curl -fsSL --retry 4 --retry-delay 10 --retry-all-errors --connect-timeout 30 -H 'Snap-Device-Series: 16' "https://api.snapcraft.io/v2/snaps/info/$BASE?fields=download,version")
		UB=$(echo "$JB" | jq -r '."channel-map"[] | select(.channel.name=="stable" and .channel.architecture==$a) | .download.url' --arg a "$ARCH" | head -1)
		[ -n "$UB" ] && [ "$UB" != null ] || { echo "no stable $ARCH $BASE snap" >&2; exit 1; }
		curl -fsSL --retry 6 --retry-delay 20 --retry-all-errors --connect-timeout 30 --max-time 900 -o "$W/b.snap" "$UB"
		case "$ARCH" in arm64) M=aarch64-linux-gnu;; amd64) M=x86_64-linux-gnu;; *) echo "no C library bundling for $ARCH" >&2; exit 1;; esac
		unsquashfs -q -d "$W/b" "$W/b.snap" "usr/lib/$M" >/dev/null
		mkdir -p "$OUT/glibc"
		for l in libc.so.6 libm.so.6 libmvec.so.1 libpthread.so.0 libdl.so.2 librt.so.1 libresolv.so.2 libanl.so.1 libutil.so.1; do
			[ -e "$W/b/usr/lib/$M/$l" ] && cp -L "$W/b/usr/lib/$M/$l" "$OUT/glibc/"; done
		LDN=$(cd "$W/b/usr/lib/$M" && ls ld-linux*.so.* | head -1); [ -n "$LDN" ] && cp -L "$W/b/usr/lib/$M/$LDN" "$OUT/glibc/"
		[ -e "$OUT/glibc/libc.so.6" ] && [ -n "$LDN" ] || { echo "$BASE has no C library at usr/lib/$M" >&2; exit 1; }
		for x in chrome chrome_crashpad_handler; do [ -e "$OUT/$x" ] || continue
			patchelf --set-interpreter "/opt/chromium/glibc/$LDN" --force-rpath --set-rpath '/opt/chromium/glibc:$ORIGIN' "$OUT/$x"; done
		echo "$BASE $(echo "$JB" | jq -r '."channel-map"[] | select(.channel.name=="stable" and .channel.architecture==$a) | .version' --arg a "$ARCH" | head -1): glibc $(strings -a "$OUT/glibc/libc.so.6" | sed -n 's/^GNU C Library .* version \([0-9.]*\).*/\1/p' | head -1)" > "$OUT.glibc"
		cat "$OUT.glibc" >&2
	fi
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
