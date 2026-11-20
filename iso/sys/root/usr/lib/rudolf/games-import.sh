#!/bin/sh
# Emu's game library: copy Game Boy (and Color, Advance), SNES, DS, Nintendo 64, Dreamcast, (1.6.1) Genesis / Mega Drive
# and PlayStation games into /home/rudolf/Games/<GB|SNES|DS|N64|DC|MD|PS1>, unpacking ZIP files and
# (1.6.1) a Dreamcast disc as one file (.cdi, .chd) goes in DC/; one as a .gdi with its track files goes in a folder of
# its own, DC/<name>/, the .gdi and every track it names (from beside it);
# (1.6.1) a PlayStation disc as a .cue goes the same way in PS1/<name>/, with the files it names; a .chd is a Dreamcast
# disc when it says it is a GD-ROM, a PlayStation one otherwise; a .bin is a Genesis game when it says SEGA where its
# header is (a disc's track otherwise, which comes with its .cue); BIOS files (scph*.bin) go in PS1/BIOS;
# skipping games that are already there. Run by rudolfd as a background job.   GPL-2.0-or-later.
#   games-import.sh LIST-FILE      (one source path per line, from /media/... or the shared folders)
export PATH=${GAMES_TEST_PATH:+$GAMES_TEST_PATH:}/usr/sbin:/usr/bin:/sbin:/bin
# test hooks (iso/qt/host/gamesimport_t.sh): GAMES_DIR, GAMES_TMP, GAMES_TEST_SRC (one more allowed place), GAMES_TEST_PATH
G=${GAMES_DIR:-/home/rudolf/Games}; L=$1
for s in GB SNES DS MD PS1; do mkdir -p $G/$s/Saves $G/$s/States; done; mkdir -p $G/N64 $G/DC $G/PS1/BIOS
chown -R 1000:1000 $G 2>/dev/null
N=$(grep -c . "$L"); i=0; added=0; skipped=0; T=${GAMES_TMP:-/run/rudolf/games-unzip}
sys_of() {   # name, file
	case "$(echo "$1" | tr A-Z a-z)" in *.gb|*.gbc|*.gba) echo GB;; *.sfc|*.smc) echo SNES;; *.nds) echo DS;; *.z64|*.n64|*.v64) echo N64;; *.cdi) echo DC;;
	*.md|*.gen|*.smd) echo MD;; *.pbp) echo PS1;; scph*.bin|psxonpsp*.bin) echo PS1/BIOS;;
	*.bin) [ "$(dd if="$2" bs=1 skip=256 count=4 2>/dev/null)" = SEGA ] && echo MD;;
	*.chd) chd_kind "$2";; esac; }
# (1.6.1) a CHD's first metadata entry: CHGD for a Dreamcast GD-ROM; CHT2, CHTR, CHCD for a CD. The metadata's place is
# 8 bytes (big-endian) at 0x30 in version 5, at 0x24 in versions 3 and 4
chd_kind() {
	v=$(od -An -tx1 -j12 -N4 "$1" 2>/dev/null | tr -d ' \n'); case "$v" in 00000005) o=48;; 0000000[34]) o=36;; *) return;; esac
	m=$(od -An -tx1 -j$o -N8 "$1" 2>/dev/null | tr -d ' \n'); [ -n "$m" ] || return
	t=$(dd if="$1" bs=1 skip=$((0x$m)) count=4 2>/dev/null)
	case "$t" in CHGD) echo DC;; CHT2|CHTR|CHCD) echo PS1;; esac; }
# (1.6.1) the files a .cue names: FILE "name" BINARY (or WAVE), the name maybe without quotes
cue_tracks() { tr -d '\r' < "$1" | sed -n -e 's/^[[:space:]]*FILE[[:space:]]*"\([^"]*\)".*$/\1/p' -e 's/^[[:space:]]*FILE[[:space:]]*\([^" ][^ ]*\)[[:space:]][[:space:]]*[A-Za-z0-9]*[[:space:]]*$/\1/p'; }
# (1.6.1) the track files a .gdi names: its lines after the first are "N LBA TYPE SIZE FILE OFFSET", FILE maybe in quotes
gdi_tracks() { tail -n +2 "$1" | tr -d '\r' | sed -n 's/^ *[0-9][0-9]* *[0-9][0-9]* *[0-9][0-9]* *[0-9][0-9]* *"\{0,1\}\([^"]*[^" ]\)"\{0,1\} *[0-9-][0-9]* *$/\1/p'; }
place_gdi() {   # the .gdi (or .cue), its name: a folder with it and its tracks
	b=$(basename "$2" | sed 's/\.[^.]*$//'); src=$(dirname "$1")
	case "$(echo "$2" | tr A-Z a-z)" in *.cue) Y=PS1; T0=$(cue_tracks "$1");; *) Y=DC; T0=$(gdi_tracks "$1");; esac; D=$G/$Y/$b
	if [ -f "$D/$2" ] && [ "$(stat -c %s "$D/$2")" = "$(stat -c %s "$1")" ]; then echo "skip $2: already in the library"; skipped=$((skipped+1)); return; fi
	[ -n "$T0" ] || { echo "skip $2: no tracks named in it"; skipped=$((skipped+1)); return; }
	SZ=0; miss=""
	while IFS= read -r t; do case "$t" in */*) miss="$t"; break;; esac; [ -f "$src/$t" ] || { miss="$t"; break; }; SZ=$((SZ + $(stat -c %s "$src/$t"))); done <<EOF
$T0
EOF
	[ -z "$miss" ] || { echo "skip $2: its track $miss is not beside it"; skipped=$((skipped+1)); return; }
	[ "$(df -k $G | awk 'NR==2{print $4}')" -gt $(( SZ / 1024 + 102400 )) ] || { echo "skip $2: not enough free space"; skipped=$((skipped+1)); return; }
	rm -rf "$D.part"; mkdir -p "$D.part" && cp "$1" "$D.part/$2" || { rm -rf "$D.part"; echo "skip $2: copy failed"; skipped=$((skipped+1)); return; }
	echo "$T0" | while IFS= read -r t; do cp "$src/$t" "$D.part/$t" || exit 1; done || { rm -rf "$D.part"; echo "skip $2: copy failed"; skipped=$((skipped+1)); return; }
	rm -rf "$D"; mv "$D.part" "$D" && chown -R 1000:1000 "$D" && { echo "added $Y/$b/$2"; added=$((added+1)); }
}
place() {   # file, name
	case "$(echo "$2" | tr A-Z a-z)" in *.gdi|*.cue) place_gdi "$1" "$2"; return;; esac
	S=$(sys_of "$2" "$1"); [ -n "$S" ] || { case "$(echo "$2" | tr A-Z a-z)" in *.bin) echo "skip $2: a disc's track (add its .cue)";; *) echo "skip $2: not a Game Boy, SNES, DS, N64, Dreamcast, Genesis or PlayStation game";; esac; skipped=$((skipped+1)); return; }
	D=$G/$S/$2; SZ=$(stat -c %s "$1")
	if [ -f "$D" ] && [ "$(stat -c %s "$D")" = "$SZ" ]; then echo "skip $2: already in the library"; skipped=$((skipped+1)); return; fi
	SUM=$(sha256sum "$1" | cut -d' ' -f1)
	for e in $G/$S/*; do [ -f "$e" ] && [ "$(stat -c %s "$e")" = "$SZ" ] && [ "$(sha256sum "$e" | cut -d' ' -f1)" = "$SUM" ] && { echo "skip $2: same game as $(basename "$e")"; skipped=$((skipped+1)); return; }; done
	[ "$(df -k $G | awk 'NR==2{print $4}')" -gt $(( SZ / 1024 + 102400 )) ] || { echo "skip $2: not enough free space"; skipped=$((skipped+1)); return; }
	cp "$1" "$D.part" && mv "$D.part" "$D" && chown 1000:1000 "$D" && { echo "added $S/$2"; added=$((added+1)); } || { rm -f "$D.part"; echo "skip $2: copy failed"; skipped=$((skipped+1)); }
}
while IFS= read -r src || [ -n "$src" ]; do
	[ -n "$src" ] || continue; i=$((i+1)); n=$(basename "$src")
	case "$src" in /media/*|/home/rudolf/Downloads/*|/home/rudolf/Documents/*|"${GAMES_TEST_SRC:-/nonexistent}"/*) ;; *) echo "skip $n: not an allowed place"; continue;; esac
	case "$src" in */../*) continue;; esac
	[ -f "$src" ] || { echo "skip $n: not found"; continue; }
	echo "progress $i $N $n"
	case "$(echo "$n" | tr A-Z a-z)" in
	*.zip)
		rm -rf $T; mkdir -p $T
		# 1.5.2: the whole name (names with spaces were cut at the first space), and Game Boy games too
		inner=$(unzip -l "$src" 2>/dev/null | awk 'NR>3 && $1 ~ /^[0-9]+$/ && NF>=4 {sub(/^ *[0-9]+ +[^ ]+ +[^ ]+ +/, ""); print}' | grep -iE '\.(gb|gbc|gba|sfc|smc|nds|z64|n64|v64|cdi|chd|gdi|md|gen|smd|bin|cue|pbp)$')
		[ -n "$inner" ] || { rm -rf $T; echo "skip $n: no Game Boy, SNES, DS, N64, Dreamcast, Genesis or PlayStation game inside"; skipped=$((skipped+1)); continue; }
		# (1.6.1) a .gdi (1.6.1: or .cue) set: everything in the ZIP, for its tracks, which are then not games of their own
		set=""; if echo "$inner" | grep -qiE '\.(gdi|cue)$'; then set=1; unzip -o -j "$src" -d $T >/dev/null 2>&1
		else echo "$inner" | while IFS= read -r f; do unzip -o -j "$src" "$f" -d $T >/dev/null 2>&1; done; fi
		for f in $T/*; do [ -f "$f" ] || continue; case "$(echo "$f" | tr A-Z a-z)" in *.bin|*.raw|*.wav) [ -n "$set" ] && continue;; esac; place "$f" "$(basename "$f")"; done
		rm -rf $T ;;
	*) place "$src" "$n" ;;
	esac
done < "$L"
rm -f "$L"
echo "done $added added, $skipped skipped"
