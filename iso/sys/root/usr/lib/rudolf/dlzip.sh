#!/bin/sh
# 1.6.1 (the owner, 2026-11-20): ZIP files kept on the disk (downloads larger than 1.5 MB, in the account's folder
# /home/rudolf/Downloads/.accounts/<id>/): what is inside them, and Extract here. Run by rudolfd (rudolfd-dlzip.inc).
#   dlzip.sh list ZIP        "entry SIZE NAME" per entry (NAME ends in / for a folder), at most 20000, then "more";
#                            then "total ENTRIES BYTES"; "error ..." when it is not a ZIP
#   dlzip.sh extract LIST    (rudolfd's job) LIST: line 1 the ZIP, line 2 the account's folder it is in. Checks the free
#                            space for the unpacked size first, unpacks it into a fresh hidden folder there (.unzip-XXXXXX, which
#                            dl/home does not list), removes what is not a file or a folder (links), then moves each file
#                            FLAT into the account's folder under a free name ("name (2).ext", as rudolfd's dl_unique), as
#                            every reference names a file there: "progress I N NAME" while unpacking, "dir PATH" for each
#                            folder, "file PATH<tab>NAME ON THE DISK<tab>SIZE" for each file; ends "done FILES BYTES",
#                            "stopped FILES BYTES" (Cancel) or "error MESSAGE". The same lines (but progress) go to OUT.
# The ZIP is never read or unpacked as root: as the desktop user (su), with its names given as arguments, never in a
# command line. Entries with ".." or an absolute path refuse the whole ZIP; links are removed, never followed.
# GPL-2.0-or-later.
# test hooks (iso/qt/host/dlzip161_t.sh): DLZIP_TEST_PATH, DLZIP_USER (who unpacks, default rudolf), DLZIP_MARGIN_KB (free
# space kept besides the files, default 100 MB), DLZIP_STOP, DLZIP_OUT
export PATH=${DLZIP_TEST_PATH:+$DLZIP_TEST_PATH:}/usr/sbin:/usr/bin:/sbin:/bin
MAX=20000; TAB=$(printf '\t'); NL='
'
# the entries' lines of unzip -l (Info-ZIP's and BusyBox's: "SIZE DATE TIME NAME"), whole names, as games-import.sh reads them
entries() { { unzip -l "$1" 2>/dev/null; echo "rc $?"; } | awk '
	$1 ~ /^[0-9]+$/ && $2 ~ /^[0-9][0-9-]*[0-9]$/ && $3 ~ /^[0-9][0-9]*:[0-9][0-9]$/ && NF >= 4 { s = $1; sub(/^ *[0-9]+ +[^ ]+ +[^ ]+ +/, ""); print "e " s " " $0; next }
	/^rc [0-9]+$/ { print }'; }
as_user() {   # verb args...: this script again as the desktop user (never as root)
	if [ "$(id -u)" = 0 ]; then su -s /bin/sh "${DLZIP_USER:-rudolf}" -c 'PATH=$1; export PATH; shift; exec /bin/sh "$0" "$@"' "$0" "$PATH" "$@" </dev/null
	else /bin/sh "$0" "$@" </dev/null; fi; }
fail() { echo "error $*"; exit 3; }
case "$1" in
list) [ "$(id -u)" = 0 ] && { as_user list "$2"; exit $?; }
	[ -f "$2" ] || fail "The file is not there any more."
	entries "$2" | awk -v max=$MAX '
		/^e / { n++; s = $2; b += s; if (n <= max) { sub(/^e [0-9]+ /, ""); print "entry " s " " $0 } else more = 1; next }
		/^rc / { rc = $2 }
		END { if (!n && rc) { print "error This file is not a ZIP archive, or it is damaged."; exit 3 } if (more) print "more"; printf "total %d %.0f\n", n, b }'
	exit $? ;;
extract)
	[ "$(id -u)" = 0 ] || fail "This is rudolfd's."
	L=$2; [ -f "$L" ] && [ ! -L "$L" ] || fail "No request."
	Z=$(sed -n 1p "$L"); D=$(sed -n 2p "$L"); rm -f "$L"
	OUT=${DLZIP_OUT:-/run/rudolf/dlzip.files}; STOP=${DLZIP_STOP:-/run/rudolf/dlzip.stop}
	rm -f "$OUT"; (umask 077; : > "$OUT")
	case "$Z" in "$D"/*) ;; *) echo "error The ZIP is not in the account's folder." | tee -a "$OUT"; exit 3;; esac
	case "${Z#"$D"/}" in */*|.*|"") echo "error The ZIP is not in the account's folder." | tee -a "$OUT"; exit 3;; esac
	{ [ -d "$D" ] && [ ! -L "$D" ] && [ -f "$Z" ] && [ ! -L "$Z" ]; } || { echo "error The ZIP is not there any more." | tee -a "$OUT"; exit 3; }
	# the desktop user's lines, each kept in OUT too (but progress, which only the window shows)
	{ as_user unpack "$Z" "$D" "$STOP"; echo "rc $?"; } | while IFS= read -r l; do
		case "$l" in "rc "*) exit "${l#rc }";; esac
		printf '%s\n' "$l"
		case "$l" in progress\ *) ;; *) printf '%s\n' "$l" >> "$OUT";; esac
	done
	exit $? ;;
unpack) ;;
*) echo "usage: dlzip.sh list ZIP | extract LIST"; exit 2;;
esac

# ---- as the desktop user: unpack ZIP into a hidden folder in DIR, then move each file flat into DIR
[ "$(id -u)" = 0 ] && fail "Not as root."
Z=$2; D=$3; STOP=$4; umask 077
[ -f "$Z" ] || fail "The ZIP is not there any more."
U=""; trap '[ -n "$U" ] && rm -rf "$U" "$U.paths" "$U.out" "$U.st"' EXIT; trap 'exit 5' INT TERM HUP
W=$(mktemp "$D/.unzip-list-XXXXXX") || fail "The account's folder cannot be written."
entries "$Z" > "$W"
rc=$(sed -n 's/^rc //p' "$W"); N=$(awk '/^e / && !/\/$/ {n++} END {print n+0}' "$W")
if [ "$N" = 0 ] && ! grep -q '^e ' "$W"; then rm -f "$W"; [ "$rc" = 0 ] && fail "The ZIP is empty." || fail "This file is not a ZIP archive, or it is damaged."; fi
# an entry with ".." or an absolute path (also as Windows writes them) refuses the whole ZIP: it was made to write elsewhere
bad=$(sed -n 's/^e [0-9]* //p' "$W" | grep -E -m1 '(^|[/\\])\.\.([/\\]|$)|^[/\\]|^[A-Za-z]:')
[ -z "$bad" ] || { rm -f "$W"; fail "This ZIP has an entry that points outside it ($bad), so it is not extracted."; }
BYTES=$(awk '/^e / {b += $2} END {printf "%.0f", b}' "$W"); BIG=$(awk '/^e / && $2 > m {m = $2} END {printf "%.0f", m}' "$W"); rm -f "$W"
# the free space for the unpacked size first (and room to spare: DLZIP_MARGIN_KB)
FREE=$(df -Pk "$D" 2>/dev/null | awk 'NR==2 {print $4}')
NEED=$(( BYTES / 1024 + 1 + ${DLZIP_MARGIN_KB:-102400} ))
[ -n "$FREE" ] && [ "$FREE" -ge "$NEED" ] 2>/dev/null || fail "Not enough free space: the files inside need $(( BYTES / 1048576 + 1 )) MB, and the disk has $(( ${FREE:-0} / 1024 )) MB free."
U=$(mktemp -d "$D/.unzip-XXXXXX") || fail "The account's folder cannot be written."
RU=$(cd "$U" && pwd -P)
# no file unzip writes may grow larger than the largest the ZIP says it holds (a ZIP that lies about its sizes stops there)
LIM=$(( BIG / 512 + 2 ))
echo "total $N $BYTES"
# unpack, a progress line for each file. A path unzip names outside the folder stops it (it never should: ".." and absolute
# paths were refused above, and both unzips make links last)
rm -f "$U.st"
{ cd "$U" && (ulimit -f $LIM 2>/dev/null; exec unzip -o "$Z" -d "$U") 2>&1; echo "rc $?"; } | { i=0; while IFS= read -r l; do
	case "$l" in
	*"inflating: "*|*"extracting: "*)
		f=${l#*ing: }; f=${f%"${f##*[! ]}"}; f=${f#"$RU"/}; f=${f#"$U"/}; f=${f#./}
		case "$f" in /*|../*|*/../*|..) echo "outside $f" > "$U.st"; exit 4;; esac
		printf '%s\n' "$f" >> "$U.paths"; i=$((i+1)); echo "progress $i $N $f"
		[ -e "$STOP" ] && { echo stopped > "$U.st"; exit 5; } ;;
	"rc "*) exit "${l#rc }";; esac
done; }; urc=$?
st=$(cat "$U.st" 2>/dev/null)
case "$st" in outside\ *) fail "unzip wrote ${st#outside } outside the folder: the ZIP is not extracted.";; stopped) echo "stopped 0 0"; exit 0;; esac
[ "$urc" -le 1 ] 2>/dev/null || fail "The ZIP could not be unpacked (unzip stopped with $urc): it may be damaged, protected by a password, or a file in it larger than it says."
# links (and anything else that is not a file or a folder) are removed, never followed; a file unzip wrote through a link that was
# there while it unpacked would be outside the folder: refused
chmod -R u+rwX "$U" 2>/dev/null
k=$(find "$U" ! -type f ! -type d | wc -l)
if [ "$k" != 0 ]; then
	find "$U" -type l > "$U.out"
	while IFS= read -r lk; do r=${lk#"$U"/}
		awk -v p="$r/" 'index($0, p) == 1 {f = 1} END {exit !f}' "$U.paths" 2>/dev/null && fail "A file in the ZIP was written through a link in it ($r): the ZIP is not extracted."
	done < "$U.out"
	find "$U" ! -type f ! -type d -exec rm -f {} \;
	echo "removed $k links or special files (only files and folders are extracted)"
fi
# names the lines cannot carry (a new line or a tab in them) are left out, said so
find "$U" -mindepth 1 \( -name "*$NL*" -o -name "*$TAB*" \) -prune -exec rm -rf {} \; -exec echo "skip a name with a new line or a tab in it" \; | sort -u
find "$U" -mindepth 1 -type d | sort | while IFS= read -r d; do echo "dir ${d#"$U"/}"; done
# each file moved FLAT into the account's folder, under a free name: a second name (ln), which never overwrites, so a name taken
# meanwhile goes on to the next one; the hidden folder, removed at the end, takes the first names with it
chmod -R go= "$U" 2>/dev/null
n=0; b=0
find "$U" -type f -exec stat -c '%s %n' {} + > "$U.out"
while IFS= read -r l; do
	[ -e "$STOP" ] && { echo "stopped $n $b"; exit 0; }
	s=${l%% *}; f=${l#* }; r=${f#"$U"/}; nm=${f##*/}; case "$nm" in .*) nm="_$nm";; esac
	[ ${#nm} -lt 200 ] || { e=${nm##*.}; [ "$e" != "$nm" ] && [ ${#e} -le 10 ] && nm="file.$e" || nm=file; }
	case "$nm" in ?*.*) stem=${nm%.*}; ext=.${nm##*.};; *) stem=$nm; ext="";; esac
	to=$nm; k=2
	until ln "$f" "$D/$to" 2>/dev/null; do [ $k -lt 10000 ] || { to=""; break; }; to="$stem ($k)$ext"; k=$((k+1)); done
	[ -n "$to" ] || { echo "skip $r: no free name"; continue; }
	n=$((n+1)); b=$((b+s))
	printf 'file %s\t%s\t%s\n' "$r" "$to" "$s"
done < "$U.out"
rm -rf "$U" "$U.paths" "$U.out" "$U.st"; U=""
echo "done $n $b"
