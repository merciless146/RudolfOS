#!/bin/sh
# Emu's game library: copy SNES and DS games into /home/rudolf/Games/<SNES|DS>, unpacking ZIP files and
# skipping games that are already there. Run by rudolfd as a background job.   GPL-2.0-or-later.
#   games-import.sh LIST-FILE      (one source path per line, from /media/... or the shared folders)
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
G=/home/rudolf/Games; L=$1
for s in SNES DS; do mkdir -p $G/$s/Saves $G/$s/States; done
chown -R 1000:1000 $G 2>/dev/null
N=$(grep -c . "$L"); i=0; added=0; skipped=0; T=/run/rudolf/games-unzip
sys_of() { case "$(echo "$1" | tr A-Z a-z)" in *.sfc|*.smc) echo SNES;; *.nds) echo DS;; esac; }
place() {   # file, name
	S=$(sys_of "$2"); [ -n "$S" ] || { echo "skip $2: not a SNES or DS game"; skipped=$((skipped+1)); return; }
	D=$G/$S/$2; SZ=$(stat -c %s "$1")
	if [ -f "$D" ] && [ "$(stat -c %s "$D")" = "$SZ" ]; then echo "skip $2: already in the library"; skipped=$((skipped+1)); return; fi
	SUM=$(sha256sum "$1" | cut -d' ' -f1)
	for e in $G/$S/*; do [ -f "$e" ] && [ "$(stat -c %s "$e")" = "$SZ" ] && [ "$(sha256sum "$e" | cut -d' ' -f1)" = "$SUM" ] && { echo "skip $2: same game as $(basename "$e")"; skipped=$((skipped+1)); return; }; done
	[ "$(df -k $G | awk 'NR==2{print $4}')" -gt $(( SZ / 1024 + 102400 )) ] || { echo "skip $2: not enough free space"; skipped=$((skipped+1)); return; }
	cp "$1" "$D.part" && mv "$D.part" "$D" && chown 1000:1000 "$D" && { echo "added $S/$2"; added=$((added+1)); } || { rm -f "$D.part"; echo "skip $2: copy failed"; skipped=$((skipped+1)); }
}
while IFS= read -r src || [ -n "$src" ]; do
	[ -n "$src" ] || continue; i=$((i+1)); n=$(basename "$src")
	case "$src" in /media/*|/home/rudolf/Downloads/*|/home/rudolf/Documents/*) ;; *) echo "skip $n: not an allowed place"; continue;; esac
	case "$src" in */../*) continue;; esac
	[ -f "$src" ] || { echo "skip $n: not found"; continue; }
	echo "progress $i $N $n"
	case "$(echo "$n" | tr A-Z a-z)" in
	*.zip)
		rm -rf $T; mkdir -p $T
		inner=$(unzip -l "$src" 2>/dev/null | awk 'NR>3{print $4}' | grep -iE '\.(sfc|smc|nds)$')
		[ -n "$inner" ] || { echo "skip $n: no SNES or DS game inside"; skipped=$((skipped+1)); continue; }
		echo "$inner" | while IFS= read -r f; do unzip -o -j "$src" "$f" -d $T >/dev/null 2>&1; done
		for f in $T/*; do [ -f "$f" ] && place "$f" "$(basename "$f")"; done
		rm -rf $T ;;
	*) place "$src" "$n" ;;
	esac
done < "$L"
rm -f "$L"
echo "done $added added, $skipped skipped"
