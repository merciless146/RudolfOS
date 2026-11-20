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
	# (1.6.0: not the update address's release kept for sticks, stickdl, nor a boot image kept for them, bootimg)
	for _e in "$_s"/* "$_s"/.[!.]*; do [ -e "$_e" ] || continue; case "${_e##*/}" in stickdl|bootimg) continue;; esac
		cp -a "$_e" "$_d/os/" || { echo "The installed system could not be copied: the drive may be failing or was unplugged."; return 13; }; done
	return 0
}

# 1.5.8: os_rescue DEST: a rescue system on the new drive: slot r, a fixed copy of the system it starts (the good
# slot), which updates never change (they only ever write slots a and b). The start-up falls back to it when no
# installed system can start, and Recovery mode can choose it, so a stick made by Make a stick can always start: on
# 2026-11-10 a stick's own update, made with a layer it did not have, left it with nothing that could start. Hard
# links: it costs no room (an update replaces a slot's files, it never writes into them).
os_rescue() {
	_d=$1; _g=$(sed -n 's/^good=//p' "$_d/os/system/state" 2>/dev/null || true)
	case "$_g" in a|b) ;; *) return 0;; esac
	# 1.6.0 (plan-1.6.1 Part 16, moved to 1.6.0: Recovery mode is always the official newest stable release): the official
	# recovery system copied with the system is kept, brought up to the newest official stable release when online
	# (rudolf-update recovery-fetch, linking what it already has); only without one is a rescue copy of the system made
	if [ -e "$_d/os/system/r/official" ] || { [ -e "${RUDOLF_SYSD:-/persist/os/system}/r/official" ] && [ ! -e "$_d/os/system/r" ]; }; then
		RU_SYSD="$_d/os/system" RU_RECOVERY_SEED="${RUDOLF_SYSD:-/persist/os/system}/r" RU_NOAUTOFETCH=1 ${RUDOLF_UPDATE:-rudolf-update} recovery-fetch >/dev/null 2>&1 || true
		[ -e "$_d/os/system/r/official" ] && { echo "The stick's Recovery mode is the official Rudolf OS ($(sed -n 's/^version //p' "$_d/os/system/r/RELEASE" 2>/dev/null))."; return 0; }
	fi
	RU_SYSD="$_d/os/system" RU_RECOVERY_SEED="${RUDOLF_SYSD:-/persist/os/system}/r" RU_NOAUTOFETCH=1 ${RUDOLF_UPDATE:-rudolf-update} recovery-fetch >/dev/null 2>&1 && [ -e "$_d/os/system/r/official" ] && {
		echo "The stick's Recovery mode is the official Rudolf OS ($(sed -n 's/^version //p' "$_d/os/system/r/RELEASE" 2>/dev/null)), downloaded."; return 0; }
	echo "Warning: the official recovery system could not be had (no internet?): the stick keeps a rescue copy of its own system instead."
	rm -rf "$_d/os/system/r.new" "$_d/os/system/r"
	cp -al "$_d/os/system/$_g" "$_d/os/system/r.new" 2>/dev/null || { rm -rf "$_d/os/system/r.new"; cp -a "$_d/os/system/$_g" "$_d/os/system/r.new"; } || { echo "The rescue system could not be kept on the new drive."; return 13; }
	_v=$(sed -n 's/^version //p' "$_d/os/system/r.new/RELEASE" 2>/dev/null)
	printf 'rescue copy of slot %s, made %s; never changed by updates\n' "$_g" "$(date '+%Y-%m-%d %H:%M')" > "$_d/os/system/r.new/rescue"
	mv "$_d/os/system/r.new" "$_d/os/system/r"
	echo "Kept a rescue system on it (Rudolf OS ${_v:-?}): it starts if an update on the stick ever cannot."
}

# 1.6.0 (the owner, 2026-11-19: "Recovery Mode boot is always locked to the official build's most recent stable
# version"): os_recefi DISK DEST: the new drive's Recovery mode start-up file (EFI/rudolf/recovery.efi in DISK's EFI
# system partition) made the official one. A stick made from another build (its update address, or a computer running
# it) has that build's boot files, and its recovery.efi would not start the official recovery system DEST has (slot r,
# os_rescue). The official file of slot r's own release (its kernel and r's modules belong together): from the boot
# image beside that release (boot.iso; its address from r's signed release list), its checksum checked, the file's
# Secure Boot signature checked as Rudolf OS's; kept in RUDOLF_RECEFI_CACHE for the next stick. Only recovery.efi (and
# the boot menu's Recovery mode entry when the build has none) is changed; the build's own start is left as it is.
# Never fatal: the stick works without it, and says so.
# Tests: RUDOLF_TEST_ESP (a folder as the EFI system partition), RUDOLF_TEST_OPENISO (CMD ISO DIR: the image's EFI
# files into DIR), SBVERIFY, SB_CERT, RUDOLF_UPDATE, RUDOLF_RECEFI_CACHE.
os_recefi() {
	_k=$1; _d=$2; _r=$_d/os/system/r; _sbv=${SBVERIFY:-sbverify}; _crt=${SB_CERT:-/usr/share/rudolf/rudolf-sb.crt}
	[ -e "$_r/official" ] && [ -s "$_r/manifest.txt" ] || return 0
	_sig() { $_sbv --cert "$_crt" "$1" >/dev/null 2>&1; }
	_w=$(mktemp -d); _e=$_w/esp
	if [ -n "$RUDOLF_TEST_ESP" ]; then _e=$RUDOLF_TEST_ESP
	else case "$_k" in *[0-9]) _p=${_k}p2;; *) _p=${_k}2;; esac; mkdir -p "$_e"
		mount -t vfat -o rw,flush "$_p" "$_e" 2>/dev/null || { rm -rf "$_w"; echo "Warning: the stick's EFI system partition could not be opened: its Recovery mode start-up file is the build's own."; return 0; }; fi
	_fin() { sync; [ -n "$RUDOLF_TEST_ESP" ] || umount "$_e" 2>/dev/null; rm -rf "$_w"; }
	_rf=$_e/EFI/rudolf/recovery.efi; _en=$_e/loader/entries/rudolf-recovery.conf
	if [ -s "$_rf" ] && _sig "$_rf" && grep -q '^efi /EFI/rudolf/recovery.efi' "$_en" 2>/dev/null; then _fin; return 0; fi
	_ser=$(sed -n 's/^serial //p' "$_r/manifest.txt"); _u=$(awk -F'\t' '$1=="L" && $2=="vmlinuz" {print $5}' "$_r/manifest.txt")
	echo "This stick's Recovery mode start-up file is not the official one: the official recovery system's (release $_ser) is put in its place ..."
	case "$_u" in https://*/vmlinuz) ;; http://127.0.0.1*/vmlinuz) [ -n "$RUDOLF_TEST_ESP" ] || _u=;; *) _u=;; esac
	[ -n "$_u" ] || { _fin; echo "Warning: the official release $_ser names no download address: the stick's Recovery mode start-up file is the build's own."; return 0; }
	_c=${RUDOLF_RECEFI_CACHE:-}; [ -n "$_c" ] || { mountpoint -q /persist 2>/dev/null && _c=/persist/os/recefi; }
	_n=$_w/new; mkdir -p "$_n"
	if [ -n "$_c" ] && [ -s "$_c/$_ser/recovery.efi" ] && _sig "$_c/$_ser/recovery.efi"; then cp "$_c/$_ser/recovery.efi" "$_c/$_ser/rudolf-recovery.conf" "$_n/" 2>/dev/null
	else
		_b=${_u%/vmlinuz}/boot.iso; _up=${RUDOLF_UPDATE:-rudolf-update}
		if ! { $_up fetch "$_b" "$_w/boot.iso" >/dev/null 2>&1 && $_up fetch "$_b.sha256" "$_w/boot.sha256" >/dev/null 2>&1; }; then
			_fin; echo "Warning: the official boot image could not be downloaded (no internet?): the stick's Recovery mode start-up file is the build's own. Make the stick again when online."; return 0; fi
		[ "$(sha256sum "$_w/boot.iso" | cut -c1-64)" = "$(cut -c1-64 "$_w/boot.sha256")" ] || { _fin; echo "Warning: the official boot image did not match its checksum: nothing was changed on the stick's start-up files."; return 0; }
		_x=$_w/x; mkdir -p "$_x"
		if [ -n "$RUDOLF_TEST_OPENISO" ]; then $RUDOLF_TEST_OPENISO "$_w/boot.iso" "$_x"
		else mkdir -p "$_w/i" && mount -t iso9660 -o ro,loop "$_w/boot.iso" "$_w/i" 2>/dev/null && mount -t vfat -o ro,loop "$_w/i/efi/efiboot.img" "$_x" 2>/dev/null; fi
		cp "$_x/EFI/rudolf/recovery.efi" "$_n/" 2>/dev/null; cp "$_x/loader/entries/rudolf-recovery.conf" "$_n/" 2>/dev/null
		[ -n "$RUDOLF_TEST_OPENISO" ] || { umount "$_x" 2>/dev/null; umount "$_w/i" 2>/dev/null; }
		[ -s "$_n/recovery.efi" ] && _sig "$_n/recovery.efi" || { _fin; echo "Warning: the official boot image's Recovery mode file is not signed by Rudolf OS: nothing was changed on the stick's start-up files."; return 0; }
		[ -n "$_c" ] && mkdir -p "$_c/$_ser" && cp "$_n"/* "$_c/$_ser/" 2>/dev/null && for _o in "$_c"/*; do [ "$_o" = "$_c/$_ser" ] || rm -rf "$_o"; done
	fi
	# room: the build's own recovery.efi is taken out first (it is not needed to start the build)
	_s=$(stat -c %s "$_n/recovery.efi"); _o=0; [ -f "$_rf" ] && _o=$(stat -c %s "$_rf")
	_fr=$(($(df -k "$_e" | awk 'NR==2 {print $4}') * 1024))
	[ $((_fr + _o)) -gt $((_s + 65536)) ] || { _fin; echo "Warning: the stick's EFI system partition has no room for the official Recovery mode file ($((_s / 1048576)) MiB): it keeps the build's own."; return 0; }
	mkdir -p "$_e/EFI/rudolf"; [ -f "$_rf" ] && mv -f "$_rf" "$_w/old.efi"
	cp "$_n/recovery.efi" "$_rf.new" && sync && cmp -s "$_n/recovery.efi" "$_rf.new" && _sig "$_rf.new" && mv -f "$_rf.new" "$_rf" || {
		rm -f "$_rf.new"; [ -f "$_w/old.efi" ] && cp "$_w/old.efi" "$_rf"; _fin; echo "Warning: the official Recovery mode file could not be written to the stick: it keeps the build's own. Make the stick again, or use another stick."; return 0; }
	if ! grep -q '^efi /EFI/rudolf/recovery.efi' "$_en" 2>/dev/null && [ -s "$_n/rudolf-recovery.conf" ]; then mkdir -p "$_e/loader/entries"; cp "$_n/rudolf-recovery.conf" "$_en"; fi
	_fin; echo "The stick's Recovery mode starts the official Rudolf OS (release $_ser): its start-up file is the official one, signed by Rudolf OS."
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
		[ ! -d "$_d/os/system/r" ] || [ -s "$_d/os/system/r/$_l" ] || { echo "The new drive's rescue system is missing $_l."; return 12; }
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
	case "$_sl" in a|b|r) ;; *) return 1;; esac
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

# 1.5.8: os_info IMAGEDIR DATADIR: what a Rudolf OS drive holds, as JSON (Settings > Storage lists plugged-in sticks):
# its image (a whole disc image, or boot files only), the systems in its RUDOLFDATA, and whether it can start
os_info() {
	_i=$1; _d=$2; _bo=false; [ -e "$_i/rudolf/bootonly" ] && _bo=true
	_iv=$(sed -n 's/^version //p' "$_i/rudolf/release" 2>/dev/null); _is=$(sed -n 's/^serial //p' "$_i/rudolf/release" 2>/dev/null)
	_g=$(sed -n 's/^good=//p' "$_d/os/system/state" 2>/dev/null || true)
	_sv() { sed -n "s/^$2 //p" "$_d/os/system/$1/RELEASE" 2>/dev/null; }
	_ok() { [ -e "$_d/os/system/$1/complete" ] && for _l in vmlinuz base.sfs chromium.sfs modules.sfs desktop.sfs; do [ -s "$_d/os/system/$1/$_l" ] || return 1; done; }
	_sys=""; case "$_g" in a|b) _ok "$_g" && _sys=$_g;; esac
	_res=""; _ok r && _res=r
	_can=false; { [ $_bo = false ] || [ -n "$_sys" ] || [ -n "$_res" ]; } && _can=true
	printf '{"bootonly":%s,"image":{"version":"%s","serial":"%s"},"system":%s,"rescue":%s,"canStart":%s}\n' "$_bo" "$_iv" "$_is" \
		"$([ -n "$_sys" ] && printf '{"slot":"%s","version":"%s","serial":"%s"}' "$_sys" "$(_sv $_sys version)" "$(_sv $_sys serial)" || echo null)" \
		"$([ -n "$_res" ] && printf '{"version":"%s","serial":"%s"}' "$(_sv r version)" "$(_sv r serial)" || echo null)" "$_can"
}
