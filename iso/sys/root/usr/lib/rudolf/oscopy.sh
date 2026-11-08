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
