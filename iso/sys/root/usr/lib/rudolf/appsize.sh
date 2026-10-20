#!/bin/sh
# How much installing a Flathub app downloads, and over which connection (Rudolf OS 1.2.5). GPL-2.0-or-later.
#   appsize.sh ID  ->  {"ok":true,"download":BYTES,"installed":BYTES,"runtime":"ID/arch/branch","runtimeDownload":BYTES,
#                       "link":"wifi|wired|none","dev":"wlan0"}
# runtimeDownload is 0 when the runtime is already installed. rudolfd's apps/size and native/size run this; the
# App Store asks before a large download over Wi-Fi or on a network marked metered.
ID=$1
case "$ID" in ''|*[!A-Za-z0-9._-]*) echo '{"ok":false,"error":"bad app"}'; exit 0;; esac
FLATHUB=https://dl.flathub.org/repo/flathub.flatpakrepo
# Flatpak prints sizes like "183.6 MB" (decimal units, a no-break space before the unit)
bytes() { awk -v s="$1" 'BEGIN{n=s+0; u=s; sub(/^[0-9.]+[^A-Za-z]*/, "", u); m=1
	if (u ~ /^kB/) m=1e3; else if (u ~ /^MB/) m=1e6; else if (u ~ /^GB/) m=1e9; else if (u ~ /^KiB/) m=1024; else if (u ~ /^MiB/) m=1048576; else if (u ~ /^GiB/) m=1073741824
	printf "%.0f", n*m}'; }
# Flatpak 1.14+ says "Download Size:" and "Installed Size:", older ones "Download:" and "Installed:"
field() { printf '%s\n' "$2" | sed -n "s/^ *$1\( [Ss]ize\)\{0,1\}: *//p" | head -n 1; }
# the connection the default route uses
DEV=$(awk '$2 == "00000000" { print $1; exit }' /proc/net/route 2>/dev/null)
if [ -z "$DEV" ]; then LINK=none; elif [ -d "/sys/class/net/$DEV/wireless" ]; then LINK=wifi; else LINK=wired; fi
D=0 I=0 RT= RD=0
if [ -x /usr/bin/flatpak ] && [ "$LINK" != none ]; then
	flatpak remotes --system --columns=name 2>/dev/null | grep -qx flathub || flatpak remote-add --system flathub "$FLATHUB" >/dev/null 2>&1
	if INFO=$(flatpak remote-info --system flathub "$ID" 2>/dev/null); then
		D=$(bytes "$(field Download "$INFO")"); I=$(bytes "$(field Installed "$INFO")"); RT=$(field Runtime "$INFO")
		if [ -n "$RT" ] && ! flatpak info --system "$RT" >/dev/null 2>&1; then
			RINFO=$(flatpak remote-info --system flathub "$RT" 2>/dev/null) && RD=$(bytes "$(field Download "$RINFO")")
		fi
	else
		printf '{"ok":false,"error":"Flathub did not answer","link":"%s","dev":"%s"}\n' "$LINK" "$DEV"; exit 0
	fi
fi
case "$RT" in *[!A-Za-z0-9._/-]*) RT=;; esac
printf '{"ok":true,"download":%s,"installed":%s,"runtime":"%s","runtimeDownload":%s,"link":"%s","dev":"%s"}\n' "${D:-0}" "${I:-0}" "$RT" "${RD:-0}" "$LINK" "$DEV"
