#!/bin/sh
# Bring your files (Rudolf OS 1.6.2, docs/plan-1.6.2.md Part 27): the personal files of the system on a disk Rudolf OS
# is about to be installed on, found, copied onto a USB drive and checked before anything is erased, and brought into
# the new account's folders after the first sign-in. GPL-2.0-or-later.
# Run by the drives helper (rudolf-drives.c: bring-*), as root, except "import", which the helper runs as the
# account's own Linux user. install.sh runs "guard".
#   bringfiles.sh scan DISK           the old disk's partitions opened read-only (never written), what is on them:
#                                     JSON on standard output and in STATE/DISK.json
#   bringfiles.sh unlock PART         a BitLocker partition opened read-only with the key on standard input (the
#                                     recovery key, or the password): never stored, never an argument
#   bringfiles.sh copy LIST           the chosen folders copied into a folder on a USB drive, each file checked
#                                     (size and SHA-256, read back from the drive); LIST: see copy_main
#   bringfiles.sh cancel              the copy running stops after its file (nothing is marked as kept)
#   bringfiles.sh decide DISK erase|partial   the person chose to erase without keeping (erase), or to erase although
#                                     some files could not be read (partial: only after a copy that said so)
#   bringfiles.sh guard /dev/DISK     install.sh, before it writes: 0 when the disk holds no personal files, or they
#                                     were copied and checked, or the person chose to erase them; else 12 (said why).
#                                     On 0 everything of the old disk this script opened is closed again.
#   bringfiles.sh close DISK          the old disk's partitions unmounted, BitLocker closed
#   bringfiles.sh found               the copies on the plugged-in drives (/media/*): JSON
#   bringfiles.sh import DIR PERSON HOME   (as the account's user) one person's files from a copy into the account's
#                                     folder: Documents into Documents and so on, the rest into Documents/From the old
#                                     computer; each file checked against the copy's SHA-256 list; names that clash
#                                     kept side by side
# Nothing from the old disk is ever run: it is mounted read-only, without programs, device files or set-user-ID, and
# links are never followed; the files copied are "only personal files" as drivecopy.sh defines them.
export PATH=${RUDOLF_TEST_PATH:+$RUDOLF_TEST_PATH:}/usr/sbin:/usr/bin:/sbin:/bin
ST=${RUDOLF_BRING_STATE:-/run/rudolf/bring}
SYS=${RUDOLF_BRING_SYS:-/sys}
MEDIA=${RUDOLF_BRING_MEDIA:-/media}
DEVD=${RUDOLF_BRING_DEV:-/dev}
TOPN='Rudolf OS - Bring your files'
NL='
'
TAB=$(printf '\t')
umask 022

# ---------------------------------------------------------------- the same lists as drivecopy.sh's "Only personal files"
# (folders at any depth, and kinds of file; iso/qt/host/bringfiles62_t.sh checks that they are the same)
PF='windows program?files program?files?(x86) programdata appdata $recycle.bin $windows.~bt $windows.~ws system?volume?information recovery perflogs msocache intel config.msi $sysreset onedrivetemp'
PX='exe dll sys msi msp cab cat mui inf drv ocx cpl scr com bat cmd lnk tmp temp log etl dmp pf manifest db-journal dat'
RX=$(for x in $PX; do printf '|%s' "$x"; done); RX="\\.(${RX#|})\$"
JUNK="(^|/|$TAB)(hiberfil|pagefile|swapfile|thumbs|desktop\\.ini|ntuser)[^/]*\$"
# a profile's own top-level entries that are not anyone's files (Windows' links and settings, Linux's caches)
SKIPTOP='^(appdata|application data|local settings|my documents|nethood|printhood|recent|sendto|start menu|templates|cookies|links|searches|contacts|3d objects|favorites|intelgraphicsprofiles|microsoftedgebackups|onedrive|desktop|documents|pictures|music|videos|downloads|snap|lost\+found|ntuser.*|.*\.(dat|ini|log|blf|regtrans-ms|pol))$'

# ---------------------------------------------------------------- small things
js() { printf '%s' "$1" | awk 'BEGIN{ORS=""} {gsub(/\\/,"\\\\"); gsub(/"/,"\\\""); gsub(/\t/," "); gsub(/[\001-\037\177]/,""); if (NR>1) print " "; print}'; }
jq_() { printf '"%s"' "$(js "$1")"; }
hb() { awk -v b="$1" 'BEGIN { if (b >= 1e9) printf "%.1f GB", b/1e9; else if (b >= 1e6) printf "%.0f MB", b/1e6; else printf "%.0f KB", b/1e3 }'; }
word_ok() { case "$1" in ""|*[!a-z0-9]*) return 1;; esac; [ ${#1} -le 32 ]; }
die() { echo "$2"; exit "$1"; }
state() { mkdir -p "$ST" "$ST/m" 2>/dev/null; chmod 755 "$ST" 2>/dev/null; }
disk_of() { case $1 in mmcblk*p[0-9]*|nvme*p[0-9]*|loop*p[0-9]*) echo "${1%p[0-9]*}";; sd*|vd*|hd*) echo "$1" | sed 's/[0-9]*$//';; *) echo "$1";; esac; }
parts_of() { for p in "$SYS/block/$1/$1"*; do [ -e "$p/partition" ] && echo "${p##*/}"; done; }
# the disk's identity: its size, serial and partition tables (a decision is for this disk as it is now)
ident() { { cat "$SYS/block/$1/size" "$SYS/block/$1/device/serial" "$SYS/block/$1/device/wwid" "$SYS/block/$1/wwid" 2>/dev/null; dd if="$DEVD/$1" bs=512 count=34 2>/dev/null | sha256sum; } | sha256sum | cut -c1-32; }
dev_of() { if [ -b "$DEVD/mapper/rbl-$1" ]; then echo "$DEVD/mapper/rbl-$1"; else echo "$DEVD/$1"; fi; }
mp_of() { echo "$ST/m/$1"; }
is_mounted() { awk -v m="$1" '$2==m {f=1} END {exit !f}' /proc/mounts; }
# bytes at an offset, in hex (BusyBox's tr has no character classes to complement)
hex_at() { dd if="$1" bs=1 skip="$2" count="$3" 2>/dev/null | od -An -tx1 -v | tr -d ' \n'; }

# what a partition is, from its first bytes: ntfs bitlocker apfs hfs ext vfat exfat luks swap or other
kind_of() {
	D=$1
	case "$(hex_at "$D" 3 8)" in 4e54465320202020) echo ntfs; return;; 2d4656452d46532d) echo bitlocker; return;; 4558464154202020) echo exfat; return;; esac
	# BitLocker To Go (a FAT boot sector with BitLocker's metadata identifier)
	[ "$(hex_at "$D" 160 16)" = 3bd66749292ed84a8399f6a339e3d001 ] && { echo bitlocker; return; }
	[ "$(hex_at "$D" 32 4)" = 4e585342 ] && { echo apfs; return; }   # (NXSB)
	case "$(hex_at "$D" 1024 2)" in 482b|4858) echo hfs; return;; esac   # (H+, HX)
	[ "$(hex_at "$D" 1080 2)" = 53ef ] && { echo ext; return; }
	[ "$(hex_at "$D" 0 4)" = 4c554b53 ] && { echo luks; return; }
	case "$(hex_at "$D" 4086 10)" in 53574150535041434532|535741502d5350414345) echo swap; return;; esac
	case "$(hex_at "$D" 82 5)" in 4641543332) echo vfat; return;; esac   # (FAT32)
	case "$(hex_at "$D" 54 3)" in 464154) echo vfat; return;; esac   # (FAT12, FAT16)
	echo other
}
# NTFS: the volume's "dirty" flag ($Volume, MFT record 3, its $VOLUME_INFORMATION): 1 dirty, 0 clean, "" unknown
ntfs_dirty() {
	D=$1
	set -- $(dd if="$D" bs=512 count=1 2>/dev/null | od -An -tu1 -v | awk '{for(i=1;i<=NF;i++) b[n++]=$i} END {
		bps=b[11]+256*b[12]; spc=b[13]; if (spc>128) { e=256-spc; spc=1; while (e-->0) spc*=2 }; cl=bps*spc
		m=0; for(i=7;i>=0;i--) m=m*256+b[48+i]; c=b[64]; if (c>127) { e=256-c; rs=1; while (e-->0) rs*=2 } else rs=c*cl
		if (bps<512 || cl<512 || rs<1024 || rs>65536) exit; printf "%.0f %d\n", m*cl+3*rs, rs}')
	[ -n "$2" ] || return 0
	dd if="$D" bs=512 skip=$(($1 / 512)) count=$(($2 / 512)) 2>/dev/null | od -An -tu1 -v | awk '{for(i=1;i<=NF;i++) b[n++]=$i} END {
		if (b[0]!=70 || b[1]!=73 || b[2]!=76 || b[3]!=69) exit
		uo=b[4]+256*b[5]; uc=b[6]+256*b[7]
		for (i=1;i<uc;i++) { p=i*512-2; b[p]=b[uo+2*i]; b[p+1]=b[uo+2*i+1] }
		a=b[20]+256*b[21]
		for (k=0;k<64 && a+8<n;k++) {
			t=b[a]+256*b[a+1]+65536*b[a+2]+16777216*b[a+3]; if (t==4294967295) exit
			l=b[a+4]+256*b[a+5]+65536*b[a+6]+16777216*b[a+7]; if (l<16) exit
			if (t==112) { co=b[a+20]+256*b[a+21]; f=b[a+co+10]+256*b[a+co+11]; print f%2; exit }
			a+=l }}'
}

# ---------------------------------------------------------------- the personal files below a folder
# plist DIR: "SIZE<TAB>RELATIVE PATH" lines (links, names with a tab or a new line, and what drivecopy.sh's "Only
# personal files" leaves out are not listed); what find could not open goes to $UNREAD
plist() {
	[ -d "$1" ] && [ ! -L "$1" ] || return 0
	PD_=$1; set --; for d in $PF; do set -- "$@" -o -iname "$d"; done; shift
	( cd "$PD_" && find . \( -type d \( "$@" \) -prune \) -o -type f ! -name "*$NL*" ! -name "*$TAB*" -exec stat -c "%s$TAB%n" {} + 2>>"${UNREAD:-/dev/null}" ) |
		sed "s#$TAB\./#$TAB#" | grep -viE "$RX" | grep -viE "$JUNK"
}
sum_list() { awk -F'\t' '{b+=$1; n++} END {printf "%d %.0f\n", n, b}'; }
# a profile's other entries (folders and files at its top), one a line: what is not anyone's own is left out
others() {
	[ -d "$1" ] || return 0
	ls -A "$1" 2>/dev/null | while IFS= read -r e; do
		case "$e" in .*|*"$TAB"*) continue;; esac
		[ -L "$1/$e" ] && continue
		printf '%s\n' "$e" | grep -qiE "$SKIPTOP" && continue
		printf '%s\n' "$e"
	done
}
# a folder of the profile, whatever its case on the old disk
ci() { [ -d "$1" ] || return 1; ls -A "$1" 2>/dev/null | while IFS= read -r e; do [ "$(printf '%s' "$e" | tr A-Z a-z)" = "$(printf '%s' "$2" | tr A-Z a-z)" ] && { [ -L "$1/$e" ] || printf '%s\n' "$1/$e"; } && break; done; }
xdg() {   # the XDG folder KEY of the Linux home H (user-dirs.dirs), or its English default
	v=$(sed -n "s/^XDG_$2_DIR=\"\\\$HOME\/\([^\"]*\)\"/\1/p" "$1/.config/user-dirs.dirs" 2>/dev/null | head -1)
	case "$v" in ""|*..*|/*) v=$3;; esac; printf '%s\n' "$1/$v"
}
# the sources of folder KEY of a person: "DIR<TAB>PREFIX" lines (PREFIX: a sub-folder of KEY they go into)
sources() {   # sources OS PROFILE KEY
	P=$2
	case "$1:$3" in
	windows:Desktop|windows:Documents|windows:Pictures) d=$(ci "$P" "$3"); [ -n "$d" ] && printf '%s\t\n' "$d"; o=$(ci "$P" OneDrive); [ -n "$o" ] && { d=$(ci "$o" "$3"); [ -n "$d" ] && printf '%s\tOneDrive\n' "$d"; } ;;
	windows:Music|windows:Videos|windows:Downloads) d=$(ci "$P" "$3"); [ -n "$d" ] && printf '%s\t\n' "$d" ;;
	windows:Other) others "$P" | while IFS= read -r e; do printf '%s\t%s\n' "$P/$e" "$e"; done
		o=$(ci "$P" OneDrive); [ -n "$o" ] && others "$o" | while IFS= read -r e; do printf '%s\tOneDrive/%s\n' "$o/$e" "$e"; done ;;
	winxp:Desktop) d=$(ci "$P" Desktop); [ -n "$d" ] && printf '%s\t\n' "$d" ;;
	winxp:Documents) d=$(ci "$P" "My Documents"); [ -n "$d" ] && printf '%s\t\n' "$d" ;;
	mac:*) ;;
	linux:Desktop) printf '%s\t\n' "$(xdg "$P" DESKTOP Desktop)" ;;
	linux:Documents) printf '%s\t\n' "$(xdg "$P" DOCUMENTS Documents)" ;;
	linux:Downloads) printf '%s\t\n' "$(xdg "$P" DOWNLOAD Downloads)" ;;
	linux:Music) printf '%s\t\n' "$(xdg "$P" MUSIC Music)" ;;
	linux:Pictures) printf '%s\t\n' "$(xdg "$P" PICTURES Pictures)" ;;
	linux:Videos) printf '%s\t\n' "$(xdg "$P" VIDEOS Videos)" ;;
	linux:Other) taken=$(for k in DESKTOP DOCUMENTS DOWNLOAD MUSIC PICTURES VIDEOS; do basename "$(xdg "$P" $k x)"; done)
		others "$P" | while IFS= read -r e; do printf '%s\n' "$taken" | grep -qxF "$e" && continue; printf '%s\t%s\n' "$P/$e" "$e"; done ;;
	esac
}
# a folder's files: every source's list, the prefix put in front ("SIZE<TAB>SOURCE FILE<TAB>PATH BELOW THE KEY")
key_files() {
	sources "$1" "$2" "$3" | while IFS="$TAB" read -r d pre; do
		if [ -f "$d" ] && [ ! -L "$d" ]; then
			printf '%s\n' "$pre" | grep -viE "$RX" | grep -qviE "$JUNK" || continue
			printf '%s\t%s\t%s\n' "$(stat -c %s "$d")" "$d" "$pre"; continue; fi
		plist "$d" | while IFS="$TAB" read -r z f; do printf '%s\t%s\t%s\n' "$z" "$d/$f" "${pre:+$pre/}$f"; done
	done
}
KEYS='Desktop Documents Pictures Music Videos Downloads Other'

# ---------------------------------------------------------------- scan
# the systems and people on a partition mounted at M: "OS<TAB>PERSON<TAB>PROFILE" lines
people() {
	M=$1; w=$(ci "$M" Windows); u=$(ci "$M" Users)
	if [ -n "$u" ] && { [ -n "$w" ] || [ -n "$(ci "$M" "Program Files")" ]; }; then
		ls -A "$u" 2>/dev/null | while IFS= read -r n; do
			[ -d "$u/$n" ] && [ ! -L "$u/$n" ] || continue
			case "$(printf '%s' "$n" | tr A-Z a-z)" in default|"default user"|"all users"|defaultuser[0-9]*|wdagutilityaccount|desktop.ini|.*) continue;; esac
			printf 'windows\t%s\t%s\n' "$n" "$u/$n"
		done; return
	fi
	x=$(ci "$M" "Documents and Settings")
	if [ -n "$x" ] && [ -n "$w" ]; then
		ls -A "$x" 2>/dev/null | while IFS= read -r n; do
			[ -d "$x/$n" ] && [ ! -L "$x/$n" ] || continue
			case "$(printf '%s' "$n" | tr A-Z a-z)" in "default user"|"all users"|localservice|networkservice|.*) continue;; esac
			printf 'winxp\t%s\t%s\n' "$n" "$x/$n"
		done; return
	fi
	if [ -d "$M/home" ] && [ ! -L "$M/home" ] && { [ -f "$M/etc/os-release" ] || [ -f "$M/usr/lib/os-release" ]; }; then H=$M/home
	elif [ ! -d "$M/etc" ] && ls -A "$M" 2>/dev/null | while IFS= read -r n; do [ -d "$M/$n/.config" ] && [ ! -L "$M/$n" ] && echo y; done | grep -q y; then H=$M   # (a separate /home partition)
	else return 0; fi
	ls -A "$H" 2>/dev/null | while IFS= read -r n; do
		[ -d "$H/$n" ] && [ ! -L "$H/$n" ] || continue
		case "$n" in lost+found|.*) continue;; esac
		printf 'linux\t%s\t%s\n' "$n" "$H/$n"
	done
}
os_name() {
	M=$1
	case "$2" in
	windows|winxp) [ "$2" = winxp ] && { echo "Windows XP"; return; }
		echo Windows ;;
	linux) n=$(sed -n 's/^PRETTY_NAME="\{0,1\}\([^"]*\)"\{0,1\}$/\1/p' "$M/etc/os-release" "$M/usr/lib/os-release" 2>/dev/null | head -1 | cut -c1-60)
		echo "${n:-Linux}" ;;
	esac
}
mount_ro() {   # mount_ro DEVICE KIND MOUNTPOINT
	mkdir -p "$3"; is_mounted "$3" && return 0
	case "$2" in
	ntfs) mount -t "${RUDOLF_BRING_NTFS:-ntfs3}" -o ro,nosuid,nodev,noexec "$1" "$3" 2>/dev/null ;;
	ext) mount -t ext4 -o ro,noload,nosuid,nodev,noexec "$1" "$3" 2>/dev/null ;;
	vfat|exfat) mount -t "$2" -o ro,nosuid,nodev,noexec "$1" "$3" 2>/dev/null ;;
	*) false ;;
	esac || { rmdir "$3" 2>/dev/null; return 1; }
}
scan_main() {
	D=$1; word_ok "$D" && [ -e "$SYS/block/$D" ] || die 2 "No such disk: $D"
	state; OUT=$ST/$D.json; PL=$ST/$D.people; : > "$PL.new"; chmod 600 "$PL.new"
	# the disk Rudolf OS runs from (or keeps its files on) is never looked at
	if grep -E "^$DEVD/$D(p?[0-9]+)? " /proc/mounts | awk '{print $2}' | grep -vqE "^($MEDIA/|$ST/m/)"; then die 4 "That is a disk Rudolf OS is using."; fi
	echo "Looking at what is on $D (read-only: nothing on it is changed)…"
	PJ=; LJ=; TOTAL=0; FOUND=0; N=0
	PARTS=$(parts_of "$D"); [ -n "$PARTS" ] || PARTS=$D
	for P in $PARTS; do
		DV=$(dev_of "$P"); K=$(kind_of "$DV"); M=$(mp_of "$P")
		case "$K" in
		bitlocker) FOUND=1; LJ="$LJ${LJ:+,}{\"part\":\"$P\",\"kind\":\"bitlocker\"}"; continue;;
		apfs|hfs|luks) FOUND=1; LJ="$LJ${LJ:+,}{\"part\":\"$P\",\"kind\":\"$K\"}"; continue;;
		ntfs|ext|vfat|exfat) ;;
		*) continue;;
		esac
		# (a partition File Explorer mounted under /media is made read-only, and read there)
		MM=$(awk -v d="$DV" -v m="$MEDIA/" '$1==d && index($2,m)==1 {print $2; exit}' /proc/mounts)
		if [ -n "$MM" ]; then mount -o remount,ro "$MM" 2>/dev/null; M=$MM
		elif ! mount_ro "$DV" "$K" "$M"; then
			[ "$K" = ntfs ] && { FOUND=1; LJ="$LJ${LJ:+,}{\"part\":\"$P\",\"kind\":\"ntfs-unreadable\"}"; }
			continue
		fi
		STATE=ok
		if [ "$K" = ntfs ]; then
			h=$(ci "$M" hiberfil.sys); [ -n "$h" ] && [ "$(head -c 4 "$h" 2>/dev/null | tr A-Z a-z)" = hibr ] && STATE=hibernated
			[ "$STATE" = ok ] && [ "$(ntfs_dirty "$DV")" = 1 ] && STATE=dirty
		fi
		people "$M" > "$ST/pp.$$"
		[ -s "$ST/pp.$$" ] || { rm -f "$ST/pp.$$"; continue; }
		FOUND=1; OS=$(head -1 "$ST/pp.$$" | cut -f1); PE=
		while IFS="$TAB" read -r os nm prof; do
			N=$((N + 1)); FJ=; PB=0
			for k in $KEYS; do
				set -- $(UNREAD=/dev/null key_files "$os" "$prof" "$k" | sum_list)
				[ "${1:-0}" -gt 0 ] || continue
				FJ="$FJ${FJ:+,}{\"k\":\"$k\",\"files\":$1,\"bytes\":$2}"; PB=$(awk -v a="$PB" -v b="$2" 'BEGIN{printf "%.0f", a+b}')
			done
			printf '%s\t%s\t%s\t%s\t%s\n' "$P" "p$N" "$os" "$nm" "$prof" >> "$PL.new"
			PE="$PE${PE:+,}{\"id\":\"p$N\",\"name\":$(jq_ "$nm"),\"bytes\":$PB,\"folders\":[$FJ]}"
			TOTAL=$(awk -v a="$TOTAL" -v b="$PB" 'BEGIN{printf "%.0f", a+b}')
			echo "  $nm: $(hb "$PB")"
		done < "$ST/pp.$$"
		rm -f "$ST/pp.$$"
		PJ="$PJ${PJ:+,}{\"part\":\"$P\",\"fs\":\"$K\",\"os\":\"$OS\",\"name\":$(jq_ "$(os_name "$M" "$OS")"),\"state\":\"$STATE\",\"people\":[$PE]}"
	done
	mv -f "$PL.new" "$PL"; ident "$D" > "$ST/$D.id"
	rm -f "$ST/$D.found" "$ST/$D.empty"
	if [ "$FOUND" = 1 ]; then : > "$ST/$D.found"; else : > "$ST/$D.empty"; fi
	J="{\"ok\":true,\"disk\":\"$D\",\"found\":$([ $FOUND = 1 ] && echo true || echo false),\"total\":$TOTAL,\"systems\":[$PJ],\"locked\":[$LJ],\"copied\":$([ -e "$ST/$D.copied" ] && echo true || echo false)}"
	printf '%s\n' "$J" > "$OUT.new"; chmod 644 "$OUT.new"; mv -f "$OUT.new" "$OUT"
	[ "$FOUND" = 1 ] && echo "Found: $(hb "$TOTAL") of personal files." || echo "No other system or personal folders on $D."
	echo "RESULT $J"
}

# ---------------------------------------------------------------- unlock (BitLocker)
unlock_main() {
	P=$1; word_ok "$P" && [ -e "$SYS/class/block/$P" ] || die 2 "No such partition: $P"
	[ "$(kind_of "$DEVD/$P")" = bitlocker ] || die 2 "$P is not locked with BitLocker."
	# the key: read here, in memory only; the recovery key's 48 digits with or without dashes and spaces
	IFS= read -r K || true; exec 0</dev/null
	K=$(printf '%s' "$K" | tr -d '\r')
	R=$(printf '%s' "$K" | tr -d ' -')
	case "$R" in *[!0-9]*) ;; *) [ ${#R} = 48 ] && K=$(printf '%s' "$R" | sed 's/\(......\)/\1-/g; s/-$//');; esac
	[ -n "$K" ] || die 2 "No key."
	if [ -n "$RUDOLF_BRING_TESTKEY" ]; then   # (host tests: no device mapper there; the key is checked, nothing opened)
		printf '%s' "$K" | cryptsetup open --type bitlk --readonly --test-passphrase --key-file=- "$DEVD/$P" >/dev/null 2>&1; R=$?
	else
		[ -b "$DEVD/mapper/rbl-$P" ] && { K=; echo "already open"; exit 0; }
		printf '%s' "$K" | cryptsetup open --type bitlk --readonly --key-file=- "$DEVD/$P" "rbl-$P" 2>"$ST/unlock.err"; R=$?
	fi
	K=; R2=$R
	case $R2 in 0) echo "opened (read-only)"; exit 0;; 2) echo "That key does not open it."; exit 3;;
	*) echo "BitLocker could not be opened here: $(head -c 200 "$ST/unlock.err" 2>/dev/null)"; exit 5;; esac
}

# ---------------------------------------------------------------- close, decide, guard
close_main() {
	D=$1; word_ok "$D" || die 2 "No such disk."
	for P in $(parts_of "$D") $D; do
		M=$(mp_of "$P"); is_mounted "$M" && umount "$M" 2>/dev/null; rmdir "$M" 2>/dev/null
		[ -b "$DEVD/mapper/rbl-$P" ] && cryptsetup close "rbl-$P" 2>/dev/null
	done
	return 0
}
decide_main() {
	D=$1; word_ok "$D" && [ -e "$ST/$D.id" ] || die 2 "Look at the disk first."
	[ "$(ident "$D")" = "$(cat "$ST/$D.id")" ] || die 2 "The disk changed: look at it again."
	case "$2" in
	erase) : > "$ST/$D.erase"; echo "The files on $D will be erased without keeping them.";;
	partial) [ -e "$ST/$D.partial" ] || die 2 "No copy said that files could not be read."; mv -f "$ST/$D.partial" "$ST/$D.copied"; echo "Kept: what could be read.";;
	*) die 2 "erase or partial";;
	esac
}
guard_main() {
	D=${1##*/}; word_ok "$D" || exit 0
	state
	ok() { [ -e "$ST/$D.id" ] && [ "$(ident "$D")" = "$(cat "$ST/$D.id")" ] && { [ -e "$ST/$D.copied" ] || [ -e "$ST/$D.erase" ] || [ -e "$ST/$D.empty" ]; }; }
	if ! ok; then rm -f "$ST/$D.copied" "$ST/$D.erase" "$ST/$D.empty"; ( scan_main "$D" ) >/dev/null 2>&1; fi
	if ok; then close_main "$D"; return 0; fi
	echo "Not installed: $D holds another system's personal files (or a disk Rudolf OS cannot read). Choose in the installer whether to keep them (copied onto a USB drive first) or to erase them. Nothing was erased."
	exit 12
}
cancel_main() { state; : > "$ST/stop"; echo "stopping"; }

# ---------------------------------------------------------------- copy
# LIST: line 1 "DISK continue=0|1", line 2 the folder on the USB drive to copy into (under /media/DEV/, made there:
# "Rudolf OS - Bring your files/NAME"), then "PERSON-ID<TAB>KEY" lines (the scan's people, KEY one of $KEYS).
# Ends: 0 all copied and checked, 3 copied and checked but some files could not be read (listed), 28 the drive is
# full (what is on it is checked: the next drive continues), 27 a file over 4 GB for a FAT32 drive, 9 a file read
# back differs, 130 cancelled, 2 bad input. Only 0 lets the installation go ahead (3 after the person agreed).
safe_name() { printf '%s' "$1" | tr '\\/:*?"<>|' '_________' | tr -d '\000-\037' | sed 's/[. ]*$//; s/^\.*//' | cut -c1-120; }
copy_main() {
	LIST=$1; [ -r "$LIST" ] || die 2 "Nothing to copy."; state; rm -f "$ST/stop"
	set -- $(sed -n 1p "$LIST"); D=$1; CONT=0; [ "$2" = continue=1 ] && CONT=1
	word_ok "$D" && [ -s "$ST/$D.people" ] || die 2 "Look at the disk first."
	[ "$(ident "$D")" = "$(cat "$ST/$D.id" 2>/dev/null)" ] || die 2 "The disk changed: look at it again."
	DEST=$(sed -n 2p "$LIST")
	# the destination: a drive's folder under /media (not on the old disk), no link anywhere on the way
	case "$DEST" in "$MEDIA"/*/"$TOPN"/*) ;; *) die 2 "The copy goes into $TOPN on a drive.";; esac
	case "$DEST" in *..*|*"$NL"*|*/./*) die 2 "Bad folder name.";; esac
	DV=${DEST#"$MEDIA"/}; DV=${DV%%/*}; word_ok "$DV" || die 2 "Bad drive."
	[ "$(disk_of "$DV")" != "$D" ] || die 2 "The copy cannot go onto the disk it is copied from."
	is_mounted "$MEDIA/$DV" || die 2 "The drive is not mounted."
	awk -v m="$MEDIA/$DV" '$2==m {print $4}' /proc/mounts | grep -q '^rw' || die 2 "The drive is read-only."
	X=$MEDIA/$DV; for c in "$TOPN" "${DEST##*/}"; do X=$X/$c; [ -L "$X" ] && die 2 "A link is in the way on the drive."; [ -e "$X" ] && [ ! -d "$X" ] && die 2 "A file is in the way on the drive."; done
	[ "$(dirname "$DEST")" = "$MEDIA/$DV/$TOPN" ] || die 2 "Bad folder name."
	mkdir -p "$DEST" || die 2 "The folder could not be made on the drive."
	[ "$(cd "$DEST" && pwd -P)" = "$DEST" ] || die 2 "A link is in the way on the drive."
	[ "$CONT" = 1 ] || [ ! -e "$DEST/bring.txt" ] || die 2 "That folder on the drive already holds a copy: choose another name."
	FS=$(awk -v m="$MEDIA/$DV" '$2==m {print $3}' /proc/mounts); FS=${RUDOLF_BRING_TESTFS:-$FS}   # (host tests: their kernel may have no FAT)
	SEL=$(sed -n '3,$p' "$LIST" | sort -u); [ -n "$SEL" ] || die 2 "Nothing chosen."
	W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
	SELK=$(printf '%s\n%s\n' "$SEL" "${DEST##*/}" | sha256sum | cut -c1-16)
	[ "$CONT" = 1 ] && [ "$(cat "$ST/$D.sel" 2>/dev/null)" = "$SELK" ] && [ -s "$ST/$D.done" ] || { CONT=0; : > "$ST/$D.done"; }
	rm -f "$ST/$D.copied" "$ST/$D.partial" "$ST/$D.erase"; echo "$SELK" > "$ST/$D.sel"; chmod 600 "$ST/$D.done" "$ST/$D.sel"
	echo "Counting what to copy…"
	: > "$W/todo"; : > "$W/people"; : > "$W/unread"
	printf '%s\n' "$SEL" | while IFS="$TAB" read -r id k; do
		case " $KEYS " in *" $k "*) ;; *) echo "Not a folder: $k" >&2; continue;; esac
		L=$(awk -F'\t' -v i="$id" '$2==i' "$ST/$D.people" | head -1); [ -n "$L" ] || { echo "Not a person: $id" >&2; continue; }
		P=$(printf '%s' "$L" | cut -f1); os=$(printf '%s' "$L" | cut -f3); nm=$(printf '%s' "$L" | cut -f4); prof=$(printf '%s' "$L" | cut -f5-)
		case "$prof" in "$ST/m/$P"/*) M=$ST/m/$P;; "$MEDIA/$P"/*) M=$MEDIA/$P;; *) M=;; esac
		[ -n "$M" ] && is_mounted "$M" || { echo "$nm's folder is not open: look at the disk again." >&2; continue; }
		pd=$(safe_name "$nm"); [ -n "$pd" ] || pd=$id
		grep -q "^$id$TAB" "$W/people" || printf '%s\t%s\t%s\n' "$id" "$pd" "$nm" >> "$W/people"
		UNREAD=$W/unread key_files "$os" "$prof" "$k" | while IFS="$TAB" read -r z s r; do printf '%s\t%s\t%s\n' "$z" "$s" "$pd/$k/$r"; done >> "$W/todo"
	done
	# names the drive cannot hold (FAT32 and exFAT: \ / : * ? " < > |) changed, and two names that are then the same
	# (any case) told apart
	if [ "$FS" = vfat ] || [ "$FS" = exfat ]; then
		awk -F'\t' 'BEGIN{OFS="\t"} { n=split($3,a,"/"); p=""; for(i=1;i<=n;i++){c=a[i]; gsub(/[\\:*?"<>|]/,"_",c); sub(/[. ]+$/,"",c); if(c=="")c="_"; p=p (i>1?"/":"") c}
			k=tolower(p); if (k in seen) { seen[k]++; x=p; sub(/\.[^.\/]*$/,"",x); e=substr(p,length(x)+1); p=x " (" seen[k] ")" e } else seen[k]=1; print $1,$2,p }' "$W/todo" > "$W/t2"; mv "$W/t2" "$W/todo"
	fi
	# continuing on the next drive: what the last ones already have and checked is left out
	[ "$CONT" = 1 ] && { awk -F'\t' 'NR==FNR {d[$0]=1; next} !($3 in d)' "$ST/$D.done" "$W/todo" > "$W/t2"; mv "$W/t2" "$W/todo"; }
	set -- $(sum_list < "$W/todo"); TN=${1:-0}; TB=${2:-0}
	[ "$TN" -gt 0 ] || [ -s "$W/unread" ] || die 2 "Nothing to copy: the folders chosen hold no personal files."
	FREE=$(df -Pk "$DEST" | awk 'NR==2 {printf "%.0f", $4 * 1024}')
	UNR=$(grep -c . "$W/unread"); [ "$UNR" -gt 0 ] || UNR=0
	echo "$TN files, $(hb "$TB"), to copy into $DEST ($(hb "${FREE:-0}") free there)."
	if [ "$FS" = vfat ]; then
		awk -F'\t' '$1 >= 4294967296 {print "  " $3}' "$W/todo" > "$W/big"
		[ -s "$W/big" ] && { echo "These files are over 4 GB and the drive is FAT32, which cannot hold them: use an exFAT or NTFS drive (or leave their folders out). Nothing was copied:"; cat "$W/big"; exit 27; }
	fi
	[ "$(awk -v a="${FREE:-0}" -v b="$TB" 'BEGIN{print (a<b)}')" = 1 ] && echo "Not everything fits on this drive: when it is full, the copy stops and another drive takes the rest."
	# the copy's own record on the drive (one per drive: what is on it, and the person each folder is)
	N=$(($(cat "$ST/$D.drives" 2>/dev/null || echo 0) + 1)); [ "$CONT" = 1 ] || N=1
	{ echo "rudolf-bring 1"; echo "made $(date -u +%Y-%m-%dT%H:%MZ)"; echo "system $(awk -F'\t' '{print $3; exit}' "$ST/$D.people")"; echo "drive $N"
	  while IFS="$TAB" read -r id pd nm; do printf 'person\t%s\t%s\n' "$pd" "$(printf '%s' "$nm" | tr -d '\t\r\n')"; done < "$W/people"; } > "$W/head"
	: > "$W/sums"; DN=0; DB=0; FULL=0; : > "$W/failed"
	while IFS="$TAB" read -r Z S R; do
		[ -e "$ST/stop" ] && { rm -f "$ST/stop"; echo "Cancelled: $DN of $TN files were copied. Nothing is erased: the disk is as it was."; exit 130; }
		T=$DEST/$R; mkdir -p "$(dirname "$T")" 2>/dev/null
		AV=$(df -Pk "$DEST" | awk 'NR==2 {printf "%.0f", $4 * 1024}')
		[ "$(awk -v a="${AV:-0}" -v b="$Z" 'BEGIN{print (a < b + 1048576)}')" = 1 ] && { FULL=1; break; }
		H=$(cat -- "$S" 2>>"$W/rerr" | tee "$T.part" | sha256sum | cut -c1-64)
		Z2=$(stat -c %s "$T.part" 2>/dev/null || echo -1)
		if [ "$Z2" != "$Z" ]; then
			rm -f "$T.part"
			AV=$(df -Pk "$DEST" | awk 'NR==2 {printf "%.0f", $4 * 1024}')
			[ "$(awk -v a="${AV:-0}" -v b="$Z" 'BEGIN{print (a < b + 1048576)}')" = 1 ] && { FULL=1; break; }
			printf '%s\n' "$R" >> "$W/failed"; DN=$((DN + 1)); continue
		fi
		mv -f "$T.part" "$T"; touch -r "$S" "$T" 2>/dev/null
		printf '%s  %s  %s\n' "$H" "$Z" "$R" >> "$W/sums"
		DN=$((DN + 1)); DB=$(awk -v a="$DB" -v b="$Z" 'BEGIN{printf "%.0f", a+b}')
		if [ $((DN % 25)) = 0 ] || [ "$Z" -ge 20000000 ]; then echo "PROGRESS $DN $TN $DB $TB"; echo "  $R"; fi
	done < "$W/todo"
	echo "PROGRESS $DN $TN $DB $TB"
	# read back from the drive itself (not from memory) and compared
	echo "Checking the copy (reading it back from the drive)…"
	sync; echo 3 > /proc/sys/vm/drop_caches 2>/dev/null
	sed 's/^\([0-9a-f]*\)  [0-9]*  /\1  /' "$W/sums" > "$W/check"
	BAD=$(cd "$DEST" && sha256sum -c "$W/check" 2>&1 | grep -c ': FAILED')
	if [ "${BAD:-0}" -gt 0 ]; then echo "$BAD files read back from the drive differ from the originals: the drive may be failing. Try another drive. Nothing is erased."; exit 9; fi
	# the record: added to what an earlier copy of the same selection put in this folder
	{ cat "$W/head"; echo "last $([ "$FULL" = 1 ] && echo 0 || echo 1)"; } > "$DEST/bring.txt"
	if [ "$CONT" = 1 ] && [ -f "$DEST/bring.sha256" ] && [ ! -L "$DEST/bring.sha256" ]; then cat "$W/sums" >> "$DEST/bring.sha256"; else cp "$W/sums" "$DEST/bring.sha256"; fi
	cat > "$DEST/README.TXT" <<EOF
Personal files from the old computer, copied by Rudolf OS's installer on $(date -u +%Y-%m-%d) and checked (SHA-256).
Each person's folders are in a folder of their own. bring.sha256 lists every file with its SHA-256 and size; bring.txt
is the copy's record. Rudolf OS brings them into your account after you sign in (or Settings › Update and recovery ›
Bring in files from the old computer). Delete this folder when you no longer need it.
EOF
	sync
	sed 's/^[0-9a-f]*  [0-9]*  //' "$W/sums" >> "$ST/$D.done"
	if [ "$FULL" = 1 ]; then
		echo "$N" > "$ST/$D.drives"
		echo "The drive is full: $DN of $TN files ($(hb "$DB") of $(hb "$TB")) are on it, checked."
		echo "Safely remove it, plug in another drive and continue: the next drive takes the rest. Nothing is erased until everything is copied."
		exit 28
	fi
	NF=$(grep -c . "$W/failed"); [ "$NF" -gt 0 ] || NF=0
	echo "Copied and checked: $DN files ($(hb "$DB")) into $DEST."
	rm -f "$ST/$D.drives"
	if [ $((NF + UNR)) -gt 0 ]; then
		echo "$((NF + UNR)) files or folders could not be read and are not in the copy: OneDrive files kept online only (their contents are in the Microsoft account, onedrive.live.com), or damaged ones:"
		{ head -30 "$W/failed"; head -20 "$W/unread" | sed 's/^find: //'; } | sed 's/^/  /'
		: > "$ST/$D.partial"; exit 3
	fi
	: > "$ST/$D.copied"; exit 0
}

# ---------------------------------------------------------------- after installing: the copies on the drives, and bringing one in
found_main() {
	J=; for b in "$MEDIA"/*/"$TOPN"/*/bring.txt; do
		[ -f "$b" ] && [ ! -L "$b" ] || continue
		d=${b%/bring.txt}; [ -L "$d" ] || [ -L "${d%/*}" ] && continue
		head -1 "$b" | grep -q '^rudolf-bring 1$' || continue
		PJ=; while IFS="$TAB" read -r t pd nm; do
			[ "$t" = person ] || continue
			case "$pd" in ""|*/*|.|..) continue;; esac
			set -- $(awk -v p="$pd/" 'index($0, "  ")==65 { r=$0; sub(/^[^ ]+  [0-9]+  /,"",r); if (index(r,p)==1) { n++; split($0,a,"  "); b+=a[2] } } END {printf "%d %.0f\n", n, b}' "$d/bring.sha256" 2>/dev/null)
			PJ="$PJ${PJ:+,}{\"dir\":$(jq_ "$pd"),\"name\":$(jq_ "$nm"),\"files\":${1:-0},\"bytes\":${2:-0}}"
		done < "$b"
		J="$J${J:+,}{\"dir\":$(jq_ "$d"),\"made\":$(jq_ "$(sed -n 's/^made //p' "$b" | head -1)"),\"system\":$(jq_ "$(sed -n 's/^system //p' "$b" | head -1)"),\"drive\":$(sed -n 's/^drive \([0-9]*\)$/\1/p' "$b" | head -1 | grep . || echo 1),\"last\":$(grep -q '^last 0$' "$b" && echo false || echo true),\"people\":[$PJ]}"
	done
	echo "{\"ok\":true,\"sets\":[$J]}"
}
# (as the account's own Linux user: what it writes is the account's, and a link in its folder leads nowhere it could
# not write itself)
import_main() {
	DIR=$1; PD=$2; HOME_=$3
	case "$DIR" in "$MEDIA"/*/"$TOPN"/*) ;; *) die 2 "Not a copy of the old computer's files.";; esac
	case "$PD" in ""|*/*|.|..) die 2 "Bad person.";; esac
	[ -f "$DIR/bring.sha256" ] && [ -d "$HOME_/Documents" ] || die 2 "Nothing to bring in (or the account's folder is locked)."
	grep -q "^person$TAB$PD$TAB" "$DIR/bring.txt" || die 2 "That person is not in this copy."
	W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
	awk -v p="$PD/" 'index($0,"  ")==65 { r=$0; sub(/^[^ ]+  [0-9]+  /,"",r); if (index(r,p)==1 && r !~ /(^|\/)\.\.?(\/|$)/) print }' "$DIR/bring.sha256" > "$W/l"
	set -- $(awk '{split($0,a,"  "); n++; b+=a[2]} END {printf "%d %.0f\n", n, b}' "$W/l"); TN=$1; TB=$2
	echo "$TN files, $(hb "$TB"), to bring into your folders."
	mkdir -p "$HOME_/.rudolf"; LOG=$HOME_/.rudolf/bring-imported.txt; : > "$LOG"; : > "$W/bad"
	DN=0; DB=0; SAME=0; [ -n "$ST" ] && rm -f "$ST/stop" 2>/dev/null
	while IFS= read -r L; do
		[ -e "$ST/stop" ] && { echo "Stopped: $DN of $TN files brought in."; break; }
		H=${L%%  *}; r=${L#*  }; Z=${r%%  *}; r=${r#*  }
		k=${r#"$PD"/}; key=${k%%/*}; rest=${k#*/}
		case "$key" in Desktop|Documents|Downloads|Music|Pictures|Videos) top=$key;; Other) top="Documents/From the old computer";; *) continue;; esac
		T=$HOME_/$top/$rest; S=$DIR/$r
		[ -f "$S" ] && [ ! -L "$S" ] || { printf '%s (not on the drive)\n' "$rest" >> "$W/bad"; DN=$((DN + 1)); continue; }
		mkdir -p "$(dirname "$T")" 2>/dev/null
		if [ -e "$T" ] || [ -L "$T" ]; then
			if [ -f "$T" ] && [ ! -L "$T" ] && [ "$(stat -c %s "$T")" = "$Z" ] && [ "$(sha256sum < "$T" | cut -c1-64)" = "$H" ]; then SAME=$((SAME + 1)); DN=$((DN + 1)); continue; fi
			b=${T%.*}; e=${T##*.}; [ "$b" = "$T" ] || [ "${e#*/}" != "$e" ] && { b=$T; e=; }
			i=1; while :; do T2="$b (from the old computer$([ $i -gt 1 ] && echo " $i"))${e:+.$e}"; [ -e "$T2" ] || [ -L "$T2" ] || break; i=$((i + 1)); done; T=$T2
		fi
		H2=$(cat -- "$S" | tee "$T.part" | sha256sum | cut -c1-64)
		if [ "$H2" != "$H" ] || [ "$(stat -c %s "$T.part" 2>/dev/null)" != "$Z" ]; then rm -f "$T.part"; printf '%s (changed on the drive: its SHA-256 differs)\n' "$rest" >> "$W/bad"; DN=$((DN + 1)); continue; fi
		mv -f "$T.part" "$T"; touch -r "$S" "$T" 2>/dev/null
		printf '%s\n' "${T#"$HOME_"/}" >> "$LOG"
		DN=$((DN + 1)); DB=$(awk -v a="$DB" -v b="$Z" 'BEGIN{printf "%.0f", a+b}')
		if [ $((DN % 25)) = 0 ] || [ "$Z" -ge 20000000 ]; then echo "PROGRESS $DN $TN $DB $TB"; echo "  $rest"; fi
	done < "$W/l"
	echo "PROGRESS $DN $TN $DB $TB"
	NB=$(grep -c . "$W/bad"); [ "$NB" -gt 0 ] || NB=0
	echo "Brought in: $(grep -c . "$LOG") files ($(hb "$DB"))$([ "$SAME" -gt 0 ] && echo ", $SAME already there")."
	[ "$NB" -gt 0 ] && { echo "$NB files were not brought in:"; head -30 "$W/bad" | sed 's/^/  /'; exit 3; }
	exit 0
}

case "$1" in
scan) scan_main "$2";;
unlock) unlock_main "$2";;
copy) copy_main "$2";;
cancel) cancel_main;;
decide) decide_main "$2" "$3";;
guard) guard_main "$2";;
close) close_main "$2";;
found) found_main;;
import) import_main "$2" "$3" "$4";;
*) echo "usage: bringfiles.sh scan DISK | unlock PART | copy LIST | cancel | decide DISK erase|partial | guard /dev/DISK | close DISK | found | import DIR PERSON HOME"; exit 2;;
esac
