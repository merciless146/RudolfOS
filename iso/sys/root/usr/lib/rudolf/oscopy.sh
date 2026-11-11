# Copying the installed Rudolf OS onto another drive (1.5.3), for Make a USB stick (mkstick.sh) and Install here
# (install.sh). Sourced. GPL-2.0-or-later.
# An installed Rudolf OS keeps only its boot files in front of RUDOLFDATA: its system is in RUDOLFDATA's os folder.
# A drive made from it gets those boot files (a boot image, boot files only), so it starts only if that system is
# copied too. Twice that was missed: a stick made on the Vivobook (2026-11-07, an empty os folder after a failed
# copy), then the HP's drive, installed from a working stick (Install here never copied the system at all): both
# stopped at "this drive has boot files only".

# 1 when the drive Rudolf OS runs from has a boot image (RUDOLF_MEDIA: tests)
os_running_bootonly() { [ -e "${RUDOLF_MEDIA:-/live/media}/rudolf/bootonly" ] && echo 1 || echo 0; }

# os_copy DEST SRCOS: the installed system (SRCOS: /persist/os) into DEST's os folder; 11 when it does not fit
os_copy() {
	_d=$1; _s=$2
	[ -d "$_s/system" ] || { echo "This Rudolf OS has no installed system to copy (its os folder is missing)."; return 11; }
	_need=$(du -sk "$_s" | cut -f1); _free=$(df -k "$_d" | awk 'NR==2 {print $4}')
	[ "$_need" -lt $((_free - 262144)) ] || { echo "Not enough room for Rudolf OS ($((_need / 1024)) MiB). Use a bigger drive."; return 11; }
	echo "Copying the installed system ($((_need / 1024)) MiB) ..."
	mkdir -p "$_d/os"
	cp -a "$_s/." "$_d/os/" || { echo "The installed system could not be copied: the drive may be failing or was unplugged."; return 13; }
	return 0
}

# os_check DEST BOOTONLY [SRCOS]: whether the RUDOLFDATA at DEST can start. With a boot image (BOOTONLY=1) its system
# slot in use must be complete, with all five layers, read back and (with SRCOS) the same as this computer's.
os_check() {
	_d=$1; _bo=$2; _s=$3
	[ "$_bo" = 1 ] || return 0
	_g=$(sed -n 's/^good=//p' "$_d/os/system/state" 2>/dev/null || true)   # (set -e: no state file is not an error here)
	case "$_g" in a|b) ;; *) echo "The new drive has no system: this Rudolf OS's installed system was not copied onto it."; return 11;; esac
	[ -e "$_d/os/system/$_g/complete" ] || { echo "The new drive's system (slot $_g) is incomplete."; return 12; }
	for _l in vmlinuz base.sfs chromium.sfs modules.sfs desktop.sfs; do
		[ -s "$_d/os/system/$_g/$_l" ] || { echo "The new drive's system is missing $_l."; return 12; }
	done
	[ -n "$_s" ] || return 0
	sync; echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true
	for _l in vmlinuz base.sfs chromium.sfs modules.sfs desktop.sfs; do
		cmp -s "$_s/system/$_g/$_l" "$_d/os/system/$_g/$_l" || { echo "The copy of $_l on the new drive does not match: the drive may be failing or was unplugged."; return 13; }
	done
	return 0
}

# 1.5.7: os_bootimg SRCOS MEDIA DIR: the boot files of the installed system in use, for a new stick. A stick made from
# an installed computer used to get the boot files on that computer's drive, which can be much older than its system
# (the Vivobook: 1.2.8's, whose Recovery mode could not start an installed system). When the drive's boot files are
# older than the system in use, or lack its Recovery mode, the system's own boot image (boot.iso, published with every
# release beside its kernel, in the signed release list's folder) is downloaded into DIR and checked: a boot image,
# the same disc ID, its kernel byte for byte the installed system's (whose release list is signed), and Recovery mode.
# Prints its path. 1: the drive's own boot files are current; 2: none could be had (offline, an older release), with
# why on stderr. RUDOLF_ETC, RUDOLF_FETCH and RUDOLF_ISODIR (an unpacked image instead of mounting one): for the tests.
os_bootimg() {
	_s=$1; _m=$2; _o=$3; _sl=$(cat "${RUDOLF_ETC:-/etc/rudolf}/slot" 2>/dev/null)
	case "$_sl" in a|b) ;; *) return 1;; esac
	_ser=$(sed -n 's/^serial //p' "$_s/system/$_sl/RELEASE" 2>/dev/null); _ms=$(sed -n 's/^serial //p' "$_m/rudolf/release" 2>/dev/null)
	if grep -qx recovery-installed "$_m/rudolf/features" 2>/dev/null && [ "${_ms:-0}" -ge "${_ser:-0}" ] 2>/dev/null; then return 1; fi
	_u=$(awk -F'\t' '$1=="L" && $2=="vmlinuz" {print $5}' "$_s/system/$_sl/manifest.txt" 2>/dev/null)
	case "$_u" in https://github.com/*/vmlinuz) _u=${_u%vmlinuz}boot.iso;; *) echo "this system's release list does not say where its boot files are" >&2; return 2;; esac
	mkdir -p "$_o"; _f=$_o/boot-$_ser.iso
	for _x in "$_o"/boot-*.iso; do [ "$_x" = "$_f" ] || rm -rf "$_x"; done   # (only the current one is kept)
	if [ ! -s "$_f" ]; then echo "Downloading the boot files of release $_ser ..." >&2
		${RUDOLF_FETCH:-rudolf-update fetch} "$_u" "$_f" >&2 || { rm -rf "$_f"; echo "they could not be downloaded (no internet, or the release has none)" >&2; return 2; }; fi
	_t=$_f; if [ -z "$RUDOLF_ISODIR" ]; then _t=/mnt/stickboot; mkdir -p $_t; umount $_t 2>/dev/null || true
		mount -t iso9660 -o ro,loop "$_f" $_t 2>/dev/null || { rm -rf "$_f"; echo "the downloaded boot files are not a disc image" >&2; return 2; }; fi
	_bad=""
	[ -e "$_t/rudolf/bootonly" ] || _bad="not a boot image"
	[ -z "$_bad" ] && [ "$(cat "$_t/rudolf/id" 2>/dev/null)" != "$(cat "$_m/rudolf/id" 2>/dev/null)" ] && _bad="another Rudolf OS disc ID"
	[ -z "$_bad" ] && ! cmp -s "$_t/boot/vmlinuz" "$_s/system/$_sl/vmlinuz" && _bad="its kernel is not this system's"
	[ -z "$_bad" ] && ! grep -qx recovery-installed "$_t/rudolf/features" 2>/dev/null && _bad="its start-up has no Recovery mode for installed systems"
	[ -z "$RUDOLF_ISODIR" ] && umount $_t 2>/dev/null
	[ -z "$_bad" ] || { rm -rf "$_f"; echo "the downloaded boot files were refused: $_bad" >&2; return 2; }
	echo "$_f"
}
