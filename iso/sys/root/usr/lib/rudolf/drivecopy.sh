#!/bin/sh
# Copy to another drive (1.6.0, 2026-11-19: a grandfather's old HP's files onto a USB stick, from a Rudolf OS stick).
# GPL-2.0-or-later. Run by rudolfd as a job (fs/xcopy); its log is what File Explorer shows.
#   drivecopy.sh LIST
# LIST: line 1 "personal=0|1 continue=0|1", line 2 the folder to copy into (created), then one source (file or folder)
# a line. Everything under /media (rudolfd checked). Folders keep their structure; links are not followed (Windows'
# junctions, "Documents and Settings", "Application Data", would loop); nothing on the source is changed.
#   personal=1: the person's own files only: Windows, programs, AppData, the recycle bin, system files and programs
#               (.exe, .dll, .sys, ...) are left out.
#   continue=1: the files an earlier copy of the same sources already put on another drive (which filled up) are
#               left out, so a second stick takes the rest.
# A file already at the destination with the same size is kept (a copy that was cut off goes on where it stopped).
# Progress lines: "PROGRESS files-done files-total bytes-done bytes-total". Ends: 0 done, 28 the drive is full,
# 2 bad input.
export PATH=${RUDOLF_TEST_PATH:+$RUDOLF_TEST_PATH:}/usr/sbin:/usr/bin:/sbin:/bin
LIST=$1; [ -r "$LIST" ] || { echo "Nothing to copy."; exit 2; }
OPT=$(sed -n 1p "$LIST"); DEST=$(sed -n 2p "$LIST")
PERS=0; CONT=0; case "$OPT" in *personal=1*) PERS=1;; esac; case "$OPT" in *continue=1*) CONT=1;; esac
[ -n "$DEST" ] || { echo "No destination."; exit 2; }
STATE=${RUDOLF_DRIVECOPY_STATE:-/persist/var/lib/rudolf/drivecopy}; mkdir -p "$STATE" 2>/dev/null || { STATE=/run/rudolf/drivecopy; mkdir -p "$STATE"; }
KEY=$(sed -n '3,$p' "$LIST" | md5sum | cut -c1-16); DONE=$STATE/$KEY.done; W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
[ "$CONT" = 1 ] && [ -s "$DONE" ] || { CONT=0; : > "$DONE"; }
mkdir -p "$DEST" || { echo "The folder $DEST could not be made on that drive."; exit 2; }
FS=$(awk -v d="$DEST" 'index(d"/", $2"/")==1 && length($2)>=l {l=length($2); f=$3} END {print f}' /proc/mounts)
# what is left out with "personal": folders (any depth) and kinds of file
PF='windows program?files program?files?(x86) programdata appdata $recycle.bin $windows.~bt $windows.~ws system?volume?information recovery perflogs msocache intel config.msi $sysreset onedrivetemp'
PX='exe dll sys msi msp cab cat mui inf drv ocx cpl scr com bat cmd lnk tmp temp log etl dmp pf manifest db-journal dat'
echo "Counting what to copy…"
: > "$W/list"
sed -n '3,$p' "$LIST" | while IFS= read -r S; do
	[ -n "$S" ] || continue
	P=$(dirname "$S"); N=$(basename "$S")
	if [ -L "$S" ]; then continue; fi
	if [ -f "$S" ]; then printf '%s\t%s\n' "$P" "$N" >> "$W/list"; continue; fi
	[ -d "$S" ] || { echo "Not found: $S"; continue; }
	set --
	if [ "$PERS" = 1 ]; then for d in $PF; do set -- "$@" -o -iname "$d"; done; shift; set -- \( -type d \( "$@" \) -prune \) -o; fi
	( cd "$P" && find "./$N" "$@" -type f -print 2>>"$W/unread" ) | sed 's#^\./##' | while IFS= read -r F; do printf '%s\t%s\n' "$P" "$F"; done >> "$W/list"
done
# what find could not open (a OneDrive file kept online only, a damaged entry): said, not copied
UNR=$(grep -c . "$W/unread" 2>/dev/null || echo 0)
if [ "$PERS" = 1 ]; then
	RX=$(for x in $PX; do printf '|%s' "$x"; done); RX="\\.(${RX#|})\$"
	grep -viE "$RX" "$W/list" | grep -viE '(^|/)(hiberfil|pagefile|swapfile|thumbs|desktop\.ini|ntuser)[^/]*$' > "$W/l2"; mv "$W/l2" "$W/list"
fi
if [ "$CONT" = 1 ]; then awk -F'\t' 'NR==FNR {d[$0]=1; next} !(substr($0, index($0, "\t")+1) in d)' "$DONE" "$W/list" > "$W/l2"; mv "$W/l2" "$W/list"; fi
# sizes, and the list to copy (source dir \t relative path \t size)
: > "$W/todo"; TB=0; TN=0
while IFS="$(printf '\t')" read -r P F; do
	Z=$(stat -c %s "$P/$F" 2>/dev/null) || continue
	printf '%s\t%s\t%s\n' "$P" "$F" "$Z" >> "$W/todo"; TB=$((TB + Z)); TN=$((TN + 1))
done < "$W/list"
hb() { awk -v b="$1" 'BEGIN { if (b >= 1e9) printf "%.1f GB", b/1e9; else if (b >= 1e6) printf "%.0f MB", b/1e6; else printf "%.0f KB", b/1e3 }'; }
FREE=$(df -Pk "$DEST" | awk 'NR==2 {print $4 * 1024}')
echo "$TN files, $(hb $TB), to copy into $DEST ($(hb ${FREE:-0}) free there)."
[ "$CONT" = 1 ] && echo "(Continuing: the files the last drive already has are left out.)"
[ "$TN" -gt 0 ] || { echo "Done: nothing to copy."; exit 0; }
[ "${FREE:-0}" -lt "$TB" ] && echo "Not everything fits: when the drive is full the copy stops, and you can carry on with another drive."
DN=0; DB=0; NF=0; FULL=0; : > "$W/failed"; : > "$W/big"
while IFS="$(printf '\t')" read -r P F Z; do
	T="$DEST/$F"; D=$(dirname "$T")
	if [ "$FS" = vfat ] && [ "$Z" -ge 4294967296 ]; then echo "$F" >> "$W/big"; DN=$((DN + 1)); continue; fi
	if [ -f "$T" ] && [ "$(stat -c %s "$T" 2>/dev/null)" = "$Z" ]; then :
	else
		mkdir -p "$D" 2>/dev/null
		if ! cp "$P/$F" "$T.part" 2>"$W/err"; then
			rm -f "$T.part"
			AV=$(df -Pk "$DEST" | awk 'NR==2 {print $4 * 1024}')
			if grep -qi "no space" "$W/err" || [ "${AV:-0}" -lt "$Z" ]; then FULL=1; break; fi
			echo "$F: $(head -c 200 "$W/err")" >> "$W/failed"; NF=$((NF + 1)); DN=$((DN + 1)); continue
		fi
		mv -f "$T.part" "$T"; touch -r "$P/$F" "$T" 2>/dev/null
	fi
	echo "$F" >> "$DONE"; DN=$((DN + 1)); DB=$((DB + Z))
	if [ $((DN % 25)) = 0 ] || [ "$Z" -ge 20000000 ]; then echo "PROGRESS $DN $TN $DB $TB"; echo "  $F"; fi
done < "$W/todo"
echo "PROGRESS $DN $TN $DB $TB"
sync
if [ -s "$W/big" ]; then echo "Left out, over 4 GB (the drive is FAT32, which cannot hold them; an exFAT or NTFS drive can):"; sed 's/^/  /' "$W/big"; fi
if [ -s "$W/failed" ]; then echo "$NF files could not be read (left out):"; head -50 "$W/failed" | sed 's/^/  /'; fi
if [ "${UNR:-0}" -gt 0 ]; then echo "$UNR entries could not be opened on the drive and were left out: OneDrive files kept online only (their contents are in the Microsoft account, onedrive.live.com), or damaged ones:"; head -20 "$W/unread" | sed 's/^find: /  /'; fi
if [ "$FULL" = 1 ]; then
	echo "The drive is full: $DN of $TN files ($(hb $DB) of $(hb $TB)) are on it."
	echo "Safely remove it, plug in another drive, and choose Copy to another drive again with the same selection and \"Continue from the last drive\": it copies the rest."
	exit 28
fi
echo "Done: $DN files ($(hb $DB)) copied into $DEST."
rm -f "$DONE"; exit 0
