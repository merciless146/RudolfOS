#!/bin/sh
# Emu's game library: copy Game Boy (and Color, Advance), SNES, DS, Nintendo 64 and Dreamcast games into
# /home/rudolf/Games/<GB|SNES|DS|N64|DC>, unpacking ZIP files and
# (1.6.1) a Dreamcast disc as one file (.cdi, .chd) goes in DC/; one as a .gdi with its track files goes in a folder of
# its own, DC/<name>/, the .gdi and every track it names (from beside it);
# skipping games that are already there. Run by rudolfd as a background job.   GPL-2.0-or-later.
#   games-import.sh LIST-FILE      (one source path per line, from /media/... or the shared folders)
export PATH=${GAMES_TEST_PATH:+$GAMES_TEST_PATH:}/usr/sbin:/usr/bin:/sbin:/bin
# test hooks (iso/qt/host/gamesimport_t.sh): GAMES_DIR, GAMES_TMP, GAMES_TEST_SRC (one more allowed place), GAMES_TEST_PATH
G=${GAMES_DIR:-/home/rudolf/Games}; L=$1
for s in GB SNES DS; do mkdir -p $G/$s/Saves $G/$s/States; done; mkdir -p $G/N64 $G/DC
chown -R 1000:1000 $G 2>/dev/null
N=$(grep -c . "$L"); i=0; added=0; skipped=0; T=${GAMES_TMP:-/run/rudolf/games-unzip}
sys_of() { case "$(echo "$1" | tr A-Z a-z)" in *.gb|*.gbc|*.gba) echo GB;; *.sfc|*.smc) echo SNES;; *.nds) echo DS;; *.z64|*.n64|*.v64) echo N64;; *.cdi|*.chd) echo DC;; esac; }
# (1.6.1) the track files a .gdi names: its lines after the first are "N LBA TYPE SIZE FILE OFFSET", FILE maybe in quotes
gdi_tracks() { tail -n +2 "$1" | tr -d '\r' | sed -n 's/^ *[0-9][0-9]* *[0-9][0-9]* *[0-9][0-9]* *[0-9][0-9]* *"\{0,1\}\([^"]*[^" ]\)"\{0,1\} *[0-9-][0-9]* *$/\1/p'; }
place_gdi() {   # the .gdi, its name: a folder with it and its tracks
	b=$(basename "$2" | sed 's/\.[^.]*$//'); D=$G/DC/$b; src=$(dirname "$1")
	if [ -f "$D/$2" ] && [ "$(stat -c %s "$D/$2")" = "$(stat -c %s "$1")" ]; then echo "skip $2: already in the library"; skipped=$((skipped+1)); return; fi
	T0=$(gdi_tracks "$1"); [ -n "$T0" ] || { echo "skip $2: no tracks named in it"; skipped=$((skipped+1)); return; }
	SZ=0; miss=""
	while IFS= read -r t; do case "$t" in */*) miss="$t"; break;; esac; [ -f "$src/$t" ] || { miss="$t"; break; }; SZ=$((SZ + $(stat -c %s "$src/$t"))); done <<EOF
$T0
EOF
	[ -z "$miss" ] || { echo "skip $2: its track $miss is not beside it"; skipped=$((skipped+1)); return; }
	[ "$(df -k $G | awk 'NR==2{print $4}')" -gt $(( SZ / 1024 + 102400 )) ] || { echo "skip $2: not enough free space"; skipped=$((skipped+1)); return; }
	rm -rf "$D.part"; mkdir -p "$D.part" && cp "$1" "$D.part/$2" || { rm -rf "$D.part"; echo "skip $2: copy failed"; skipped=$((skipped+1)); return; }
	echo "$T0" | while IFS= read -r t; do cp "$src/$t" "$D.part/$t" || exit 1; done || { rm -rf "$D.part"; echo "skip $2: copy failed"; skipped=$((skipped+1)); return; }
	rm -rf "$D"; mv "$D.part" "$D" && chown -R 1000:1000 "$D" && { echo "added DC/$b/$2"; added=$((added+1)); }
}
place() {   # file, name
	case "$(echo "$2" | tr A-Z a-z)" in *.gdi) place_gdi "$1" "$2"; return;; esac
	S=$(sys_of "$2"); [ -n "$S" ] || { echo "skip $2: not a Game Boy, SNES, DS, N64 or Dreamcast game"; skipped=$((skipped+1)); return; }
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
		inner=$(unzip -l "$src" 2>/dev/null | awk 'NR>3 && $1 ~ /^[0-9]+$/ && NF>=4 {sub(/^ *[0-9]+ +[^ ]+ +[^ ]+ +/, ""); print}' | grep -iE '\.(gb|gbc|gba|sfc|smc|nds|z64|n64|v64|cdi|chd|gdi)$')
		[ -n "$inner" ] || { rm -rf $T; echo "skip $n: no Game Boy, SNES, DS, N64 or Dreamcast game inside"; skipped=$((skipped+1)); continue; }
		# (1.6.1) a .gdi set: everything in the ZIP, for its tracks
		if echo "$inner" | grep -qi '\.gdi$'; then unzip -o -j "$src" -d $T >/dev/null 2>&1
		else echo "$inner" | while IFS= read -r f; do unzip -o -j "$src" "$f" -d $T >/dev/null 2>&1; done; fi
		for f in $T/*; do [ -f "$f" ] || continue; case "$(echo "$f" | tr A-Z a-z)" in *.bin|*.raw) continue;; esac; place "$f" "$(basename "$f")"; done
		rm -rf $T ;;
	*) place "$src" "$n" ;;
	esac
done < "$L"
rm -f "$L"
echo "done $added added, $skipped skipped"
