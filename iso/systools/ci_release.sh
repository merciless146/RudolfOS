#!/bin/bash
# Build the next Rudolf OS system release from the previous one (run by the GitHub Actions workflow on
# ubuntu-24.04, the same system the disc images are built on). Only what changed is rebuilt:
#   kernel + modules  when a newer 6.18.y is out, or kconfig.sh / the initramfs changed
#   chromium          when a newer stable Chromium is out
#   desktop           when iso/www changed in the repo
#   base              when the system programs (rudolf-display, rudolfd, rudolf-update) or sys/root changed:
#                     the previous base layer with those rebuilt and the files of sys/root copied over it
#                     (everything else in it comes from build_iso.sh's full builds)
# Every new Chromium must start inside the real base layer before it is released; otherwise the previous
# Chromium layer is kept and the release notes say so. GPL-2.0-or-later.
#
# Environment: PREV (folder with the previous release: manifest.txt and its five files), OUT, KEYFILE,
# SERIAL, FILE_BASE (where devices download the files), CHROMIUM_FROM (snap|playwright), KERNEL_VERSION.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd); cd "$HERE"
# 1.4.4: nothing unused gets into a release (unused desktop functions, unused C functions, build output in sys/root)
python3 systools/deadcheck.py
# 1.5.9 (plan 15.5): before each release, rudolfd's request parsing and JSON fuzzed for a bounded time (FUZZ_SECONDS,
# 120 by default), and nothing listening that the list of listeners does not name
FUZZ_SECONDS=${FUZZ_SECONDS:-120} sh qt/host/fuzz59_t.sh || { echo "the fuzz test of rudolfd found a crash (see above)"; exit 1; }
sh qt/host/listeners59_t.sh || { echo "something listens that sys/root/usr/share/rudolf/listeners does not name"; exit 1; }
# 1.6.1 (docs/plan-1.6.1.md Part 11): rudolf-rootd and the root helpers: who may ask, their checks, the API through them
# the same; and that this build installs them root's, 0700
python3 qt/host/rootbuild161_t.py || { echo "the root helpers are not built and installed as they must be (see above)"; exit 1; }
sudo sh qt/host/roothelpers161_t.sh || { echo "the root helpers' test failed (see above)"; exit 1; }
sudo env RH_CONFINE=on sh qt/host/roothelpers161_t.sh || { echo "the root helpers' test with their confinement on failed (see above)"; exit 1; }
sudo env HAT_VIA=helper sh qt/host/homeapi_t.sh || { echo "the accounts' folders through the drives helper failed (see above)"; exit 1; }
sudo env VN_VIA=helper sh qt/host/vaultnative60_t.sh || { echo "the encrypted folder through the drives helper failed (see above)"; exit 1; }
W=$(realpath -m work); rm -rf "$W"; mkdir -p "$W/new"
export SOURCE_DATE_EPOCH=1760000000
pv() { sed -n "s/^$1 //p" "$PREV/manifest.txt"; }
# (1.5.9, plan Part 3: the build machine's library directory, x86_64-linux-gnu or aarch64-linux-gnu)
MA=$(gcc -print-multiarch); [ -n "$MA" ] || MA=x86_64-linux-gnu
# 1.5.9 (plan 15.5): every program built hardened (systools/hardflags.sh: HARD, HARD_STATIC), checked on the built
# programs (systools/hardcheck.py)
. systools/hardflags.sh
MK="mksquashfs"; SQ="-comp xz -Xbcj x86 -b 1M -noappend -all-root -quiet -no-progress"
NOTES=(); CH=$(pv chromium); KV=$(pv kernel)
for f in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do cp "$PREV/$f" "$W/new/$f"; done

# ---- Chromium
try_chromium() {
	systools/chromium_fetch.sh "$1" "$W/chrome" || return 1
	NV=$(cat "$W/chrome.version"); [ "$NV" = "$CH" ] && { echo "Chromium $NV unchanged"; return 0; }
	# never a lower version: Chromium does not open a profile written by a newer one (1.5.8 shipped Chrome for
	# Testing 158 from the fallback; the snap's Chromium replaces it only once it is 158 or newer)
	[ "$(printf '%s\n%s\n' "$NV" "$CH" | sort -V | tail -1)" = "$NV" ] || { echo "Chromium $NV from $1 is older than the release's $CH: kept $CH"; NOTES+=("Chromium $CH kept (the $1 has $NV, older)"); return 0; }
	rm -rf "$W/cs"; mkdir -p "$W/cs/opt/chromium"; cp -a "$W/chrome/." "$W/cs/opt/chromium/"
	( cd "$W/cs/opt/chromium/locales" && for f in *.pak; do case $f in en-US.pak|de.pak|es.pak|fr.pak|pt-BR.pak|ru.pak|zh-CN.pak|ja.pak|ko.pak|ar.pak|he.pak) ;; *) rm -f "$f";; esac; done )
	rm -f "$W/cs/opt/chromium/chrome-wrapper" "$W/cs/opt/chromium/xdg-mime" "$W/cs/opt/chromium/xdg-settings" "$W/cs/opt/chromium"/locales/*.info
	strip "$W/cs/opt/chromium/chrome" "$W/cs/opt/chromium/chrome_crashpad_handler" 2>/dev/null || true
	[ -e "$W/cs/opt/chromium/chrome_sandbox" ] && chmod 4755 "$W/cs/opt/chromium/chrome_sandbox"
	# start test: the new Chromium inside the real base layer must render a page
	sudo rm -rf "$W/root"; sudo unsquashfs -q -d "$W/root" "$W/new/base.sfs" >/dev/null
	sudo cp -a "$W/cs/opt/chromium/." "$W/root/opt/chromium/"
	sudo mount --bind /dev "$W/root/dev"; sudo mount -t proc proc "$W/root/proc"
	OK=$(sudo chroot "$W/root" /opt/chromium/chrome --headless=new --no-sandbox --disable-gpu --user-data-dir=/tmp/ct --dump-dom 'data:text/html,<p>rudolf-ok</p>' 2>/dev/null | grep -c rudolf-ok || true)
	# open-source Chromium, not a Google Chrome build ("Chromium 158.0.…", not "Google Chrome for Testing 158.0.…")
	BR=$(sudo chroot "$W/root" /opt/chromium/chrome --version 2>/dev/null | head -1)
	sudo umount "$W/root/proc" "$W/root/dev"; sudo rm -rf "$W/root"
	[ "$OK" -ge 1 ] || { echo "Chromium $NV from $1 did not start inside Rudolf OS"; return 1; }
	case "$BR" in Chromium\ *) ;; *) [ "$1" = playwright ] || { echo "the browser from $1 says \"$BR\", not Chromium"; return 1; };; esac
	$MK "$W/cs" "$W/new/chromium.sfs" $SQ; CH=$NV; NOTES+=("Chromium $NV"); return 0
}
# CHROMIUM_FROM=none skips Chromium (local test builds)
# (1.5.9: no fallback to another kind of Chromium: when the snap cannot be had, the previous release's chromium.sfs
# stays, said in the notes and as a warning. Before, the fallback brought Chrome for Testing into 1.5.8 unasked.)
[ "${CHROMIUM_FROM:-snap}" = none ] || try_chromium "${CHROMIUM_FROM:-snap}" || { NOTES+=("Chromium $CH kept: the new one could not be fetched or failed the start test"); echo "::warning title=Chromium::Chromium $CH kept: the ${CHROMIUM_FROM:-snap} could not be fetched or failed the start test"; }

# ---- kernel and modules
# what the kernel is built from besides its version: when it changes, the kernel is rebuilt
# Secure Boot (1.2.2): the certificate built into the kernel as trusted (kexec), and the key that signs the kernel
# (SB_KEY: the secret RUDOLF_SB_KEY; local test builds pass a test key and certificate)
SB_CERT=$(realpath "${SB_CERT:-boot/sb/rudolf-sb.crt}"); export SB_CERT
[ -n "$SB_KEY" ] && [ -f "$SB_KEY" ] || { echo "SB_KEY: the Secure Boot signing key is needed"; exit 1; }
python3 systools/sbkeycheck.py "$SB_KEY" "$SB_CERT"
SBSIGN=${SBSIGN:-sbsign}
KSRC=$(cat kconfig.sh firmware-extra.txt firmware-sof.txt systools/fwextra.py initramfs.list initramfs/init initramfs/etc/rudolf-id sys/rudolf-kexec.c sys/rudolf-verity.c trusted-keys.txt "$SB_CERT" ${RUDOLF_TEST_KEYS:+"$RUDOLF_TEST_KEYS"} | sha256sum | cut -c1-16)
PKSRC=$(unsquashfs -cat "$PREV/modules.sfs" "usr/lib/modules/$KV/rudolf-ksrc" 2>/dev/null || true)
[ -n "$KERNEL_VERSION" ] || KERNEL_VERSION=${KV%-rudolf}
if [ "$KERNEL_VERSION-rudolf" != "$KV" ] || [ "$KSRC" != "$PKSRC" ]; then
	# LINUX_SRC: an unpacked kernel tree of that version to build in instead of downloading one (local test builds)
	# 1.5.7: kernel.org's git server refused the clone in two builds in a row (exit 128, 2026-11-10): it is tried twice,
	# then the release tarball from cdn.kernel.org, checked against the SHA-256 in kernel.org's sha256sums.asc beside it
	if [ -n "$LINUX_SRC" ]; then ln -s "$(realpath "$LINUX_SRC")" "$W/linux"
	else ok=""
		for i in 1 2; do rm -rf "$W/linux"; git clone -q --depth 1 -b "v$KERNEL_VERSION" https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git "$W/linux" && { ok=1; break; }; echo "kernel: git.kernel.org did not answer, trying again"; sleep 20; done
		if [ -z "$ok" ]; then
			KM=${KERNEL_VERSION%%.*}; T=linux-$KERNEL_VERSION.tar.xz; echo "kernel: from cdn.kernel.org ($T)"
			curl -fsSL --retry 3 -o "$W/$T" "https://cdn.kernel.org/pub/linux/kernel/v$KM.x/$T"
			curl -fsSL --retry 3 -o "$W/sha256sums.asc" "https://cdn.kernel.org/pub/linux/kernel/v$KM.x/sha256sums.asc"
			want=$(awk -v f="$T" '$2==f{print $1}' "$W/sha256sums.asc"); got=$(sha256sum "$W/$T" | cut -c1-64)
			[ -n "$want" ] && [ "$want" = "$got" ] || { echo "kernel: $T does not match kernel.org's checksum"; exit 1; }
			mkdir -p "$W/linux" && tar -xJf "$W/$T" -C "$W/linux" --strip-components=1 && rm -f "$W/$T"
		fi
	fi
	# 1.4.7: busybox's own server often fails mid-clone ("possible repository corruption on the remote side"): try it
	# three times, then GitHub's mirror; either way the code must be release 1_36_1's commit
	BB_COMMIT=1a64f6a20aaf6ea4dbba68bbfa8cc1ab7e5c57c4
	if [ ! -x initramfs/bin/busybox ]; then
		for u in https://git.busybox.net/busybox https://git.busybox.net/busybox https://git.busybox.net/busybox https://github.com/mirror/busybox; do
			rm -rf "$W/bb"; if git clone -q --depth 1 -b 1_36_1 "$u" "$W/bb" && [ "$(git -C "$W/bb" rev-parse HEAD)" = "$BB_COMMIT" ]; then break; fi
			echo "busybox: $u did not give release 1_36_1, trying again"; sleep 10
		done
		if [ "$(git -C "$W/bb" rev-parse HEAD 2>/dev/null)" != "$BB_COMMIT" ]; then echo "busybox 1_36_1 could not be fetched"; exit 1; fi
		cp systools/busybox.config "$W/bb/.config"; make -C "$W/bb" -s -j$(nproc) >/dev/null; mkdir -p initramfs/bin; cp "$W/bb/busybox" initramfs/bin/
	fi
	gcc -Os -static -nostdlib -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -o initramfs/bin/rudolf-kexec sys/rudolf-kexec.c && strip initramfs/bin/rudolf-kexec
	gcc -Os $HARD_STATIC -o initramfs/bin/rudolf-verity sys/rudolf-verity.c && strip initramfs/bin/rudolf-verity
	cp trusted-keys.txt initramfs/etc/rudolf-keys
	# local test builds only (never set in the workflow): also trust the test release key, so that a test
	# release's layers pass the start-up check
	[ -n "$RUDOLF_TEST_KEYS" ] && cat "$RUDOLF_TEST_KEYS" >> initramfs/etc/rudolf-keys
	# firmware the kernel and its drivers need that the base layer lacks (1.2.3: NVIDIA GSP, Realtek Ethernet;
	# 1.2.4: AMD microcode, built into the kernel), pinned by firmware-extra.txt. LINUX_FIRMWARE: a linux-firmware
	# checkout to take it from (local test builds).
	FW=$LINUX_FIRMWARE
	if [ -z "$FW" ]; then
		FW=$W/linux-firmware
		for u in https://gitlab.com/kernel-firmware/linux-firmware.git https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git; do
			rm -rf "$FW"; git clone -q --filter=blob:none --no-checkout "$u" "$FW" || continue
			# 1.5.3: exactly the files firmware-extra.txt pins (its "F" lines), so a file added there is always fetched
			# (rt2870.bin was not: the hand-written list here had fallen behind)
			# 1.6.1: at the linux-firmware commit the list was made from (its "R" line), not the newest: linux-firmware's
			# own updates (2026-11-12, -18, -19: Qualcomm and Intel Bluetooth) stopped the release build three times
			awk '$1=="F"{print "/"$3}' firmware-extra.txt | { echo /WHENCE; cat; } | git -C "$FW" sparse-checkout set --no-cone --stdin && git -C "$FW" checkout -q "$(awk '$1=="R"{print $2}' firmware-extra.txt)" && break
		done
	fi
	rm -rf "$W/fw"; python3 systools/fwextra.py copy firmware-extra.txt "$FW" "$W/fw"
	# 1.5.4: Intel's sound processor firmware (SOF) and its topologies, which linux-firmware does not have: from
	# thesofproject/sof-bin at this commit, exactly the files firmware-sof.txt pins. SOF_BIN: a checkout (local test builds)
	SB=$SOF_BIN
	if [ -z "$SB" ]; then
		SB=$W/sof-bin; SOF_COMMIT=fcd5ae85ead7ecd67ff199335b61fac825843ca0
		rm -rf "$SB"; git clone -q --filter=blob:none --no-checkout https://github.com/thesofproject/sof-bin.git "$SB"
		awk '$1=="F"{print "/"$4}' firmware-sof.txt | git -C "$SB" sparse-checkout set --no-cone --stdin
		git -C "$SB" checkout -q "$SOF_COMMIT"
	fi
	python3 systools/fwextra.py copy firmware-sof.txt "$SB" "$W/fw"
	UCODE_DIR="$W/fw" ./kconfig.sh "$W/linux" "$HERE/initramfs $HERE/initramfs.list"
	make -C "$W/linux" -s -j$(nproc) bzImage modules
	make -C "$W/linux" -s modules_install INSTALL_MOD_PATH="$W/mods" DEPMOD=true >/dev/null
	# the module index in busybox's format (modules.dep.bb), which the system's busybox modprobe reads, as on the disc
	KR=$(ls "$W/mods/lib/modules"); cp initramfs/bin/busybox "$W/mods/depmod"; sudo chroot "$W/mods" /depmod "$KR"; rm -f "$W/mods/depmod"
	[ -s "$W/mods/lib/modules/$KR/modules.dep.bb" ] || { echo "no module index"; exit 1; }
	echo "$KSRC" > "$W/mods/lib/modules/$KR/rudolf-ksrc"
	mkdir -p "$W/ms/usr/lib"; cp -a "$W/mods/lib/modules" "$W/ms/usr/lib/"
	mkdir -p "$W/ms/usr/lib/firmware" && cp -a "$W/fw/." "$W/ms/usr/lib/firmware/"
	$MK "$W/ms" "$W/new/modules.sfs" $SQ
	# signed with the Rudolf OS Secure Boot key: the disc's boot menu starts it (inside the kernel bundle), and a
	# Rudolf OS kernel under Secure Boot hands over (kexec) only to a kernel with this signature
	"$SBSIGN" --key "$SB_KEY" --cert "$SB_CERT" --output "$W/new/vmlinuz" "$W/linux/arch/x86/boot/bzImage"
	[ "$KR" = "$KV" ] && NOTES+=("Linux $KERNEL_VERSION with new drivers") || NOTES+=("Linux $KERNEL_VERSION"); KV=$KR
fi

# ---- base: the system programs and sys/root
# 1.3.0: every program in sys/ is built here or into the start-up image, or a fix to it would reach no one by update
# (rudolf-debian until 1.2.7, rudolf-portal and rudolf-apps until 1.2.9 were left out)
# (this script, read from here: the start-up cd above makes a relative $0 point nowhere)
SELF=systools/ci_release.sh; [ -r "$SELF" ] || { echo "cannot read $SELF from $(pwd)"; exit 1; }
for c in sys/*.c; do grep -q "gcc[^#]*$c\b" "$SELF" || { echo "$c is not built by $SELF: add it (base programs) so updates carry it"; exit 1; }; done

# the Secure Boot certificate, for Settings > Security > Prepare Secure Boot (rudolfd enrolls it through MokManager)
mkdir -p sys/root/usr/share/rudolf && cp "$SB_CERT" sys/root/usr/share/rudolf/rudolf-sb.crt
# 1.4.3: the X server is rebuilt when its build changes (xshim.sh, xbuild.sh, the pinned X source commit); before, it
# came only from full local builds, so GLX (off since 1.1) could not be turned on by an update
XREV=f6f1ab56f7350b63cb52dd944e53ceb3db80a5c2
XSRC=$( (cat xshim.sh xbuild.sh systools/xfetch.sh; echo "$XREV") | sha256sum | cut -c1-16)
# (1.5.9: and this script itself: a program it adds to the base layer, such as iw, rebuilds the layer)
BSRC=$( (cat sys/rudolf-display.c sys/rrcompat.h sys/rudolf-launch.c sys/rudolf-memguard.c sys/rudolf-shellfix.c sys/rudolf-remote.c sys/rudolfd.c sys/rudolfd-home.inc sys/rudolfd-bring.inc sys/rudolfd-desktop.inc sys/rudolfd-peer.inc sys/rudolfd-users.inc sys/rudolfd-know.inc sys/rudolfd-dev.inc sys/rudolfd-mobile.inc sys/rudolfd-smartcard.inc sys/rudolfd-drop.inc sys/rudolfd-dlzip.inc sys/rudolfd-root.inc sys/rudolfd-roothome.inc sys/rudolfd-rootpower.inc sys/rudolfd-rootnet.inc sys/rudolfd-rootdev.inc sys/rudolfd-rootother.inc sys/rudolfd-rootscreen.inc sys/rudolfd-boottime.inc sys/rudolfd-game.inc sys/rudolfd-lxprog.inc sys/rudolfd-gpu.inc sys/rudolfd-duel.inc sys/rudolfd-vpn.inc sys/rudolf-root.h sys/rudolf-rootd.c sys/rudolf-drives.c sys/rudolf-drives-home.c sys/rudolf-accounts.c sys/rudolf-power.c sys/rudolf-network.c sys/rudolf-devices.c sys/rudolf-procs.c sys/rudolf-logs.c sys/rudolf-screen.c sys/rudolf-sound.c sys/rudolf-vpn.c sys/rudolf-updates.c sys/rudolf-update.c sys/cdp-eval.c sys/rudolf-debian.c sys/rudolf-fscheck.c sys/rudolf-portal.c sys/rudolf-open.c sys/rudolf-apps.c sys/rudolf-steamquit.h sys/rpk.c sys/rudolf-ai.c sys/origin.h systools/origin.sh sys/rudolf-know.c sys/rudolf-dev.c sys/rudolf-print.c sys/rudolf-duel.c sys/rudolf-printq.c sys/rudolf-devlink.c sys/rudolf-webkit.c sys/rudolf-native.c sys/litepoint.h sys/rsynth.h trusted-keys.txt systools/addnvk.sh systools/addwebkit.sh VERSION xshim.sh xbuild.sh systools/xfetch.sh systools/base-remove.txt systools/ci_release.sh systools/hardflags.sh systools/baseabi.sh; echo "$XREV"; echo "${GITHUB_REPOSITORY:-}"; cd sys/root && find . -type f | sort | xargs sha256sum) | sha256sum | cut -c1-16)
PBSRC=$(unsquashfs -cat "$PREV/base.sfs" usr/share/rudolf/base-src 2>/dev/null || true)
BSRC=$( (echo "$BSRC"; cat sys/rudolf-webkey.c sys/rudolfd-webkey.inc) | sha256sum | cut -c1-16)   # (1.6.1 Part 20: the website data's key)
BSRC=$( (echo "$BSRC"; cat sys/rudolfd-confine.inc sys/rudolf-confine.h sys/rudolfd-confine-tables.h sys/rudolfd-confine-narrow.h sys/rudolfd-confine-pure.h sys/rudolfd-winapi.h sys/rudolf-rootd-confine.h) | sha256sum | cut -c1-16)   # (1.6.1 Part 20: the confinement)
BSRC=$( (echo "$BSRC"; cat sys/rudolf-secrets.h sys/rudolfd-guard.inc) | sha256sum | cut -c1-16)   # (1.6.1 Part 26: Blitzen's guards, the secret detector)
if [ "$BSRC" != "$PBSRC" ]; then
	sudo rm -rf "$W/broot"; sudo unsquashfs -q -d "$W/broot" "$PREV/base.sfs" >/dev/null
	B=$W/bbin; mkdir -p "$B"
	# the same commands as mkrootfs.sh, on the same Ubuntu 24.04 the base layer's libraries come from
	gcc -O2 $HARD -o "$B/rudolf-display" sys/rudolf-display.c -lX11 /usr/lib/$MA/libXtst.so.6 /usr/lib/$MA/libXfixes.so.3 /usr/lib/$MA/libXrandr.so.2 -lm
	gcc -O2 $HARD -o "$B/rudolfd" sys/rudolfd.c -Wl,-Bstatic -lz -Wl,-Bdynamic
	gcc -O2 $HARD -o "$B/rudolf-update" sys/rudolf-update.c -lcrypto -ldl
	# 1.6.1 (docs/plan-1.6.1.md Part 11): rudolf-rootd and the root helpers (installed below: root's only, 0700)
	RHELP="drives updates accounts power network devices procs logs screen sound vpn"
	RINNER="drives-home"   # (run by a helper only, not by rudolf-rootd: the drives helper's drives-home, drives part 2)
	gcc -O2 $HARD -o "$B/rudolf-rootd" sys/rudolf-rootd.c
	# (each named: the check below wants every sys/*.c in a gcc line of its own)
	gcc -O2 $HARD -o "$B/root-drives" sys/rudolf-drives.c
	gcc -O2 $HARD -o "$B/root-updates" sys/rudolf-updates.c
	gcc -O2 $HARD -o "$B/root-drives-home" sys/rudolf-drives-home.c -Wl,-Bstatic -lz -Wl,-Bdynamic
	gcc -O2 $HARD -o "$B/root-accounts" sys/rudolf-accounts.c -Wl,-Bstatic -lz -Wl,-Bdynamic   # (1.6.1 Part 11: the accounts' Linux users)
	gcc -O2 $HARD -o "$B/root-power" sys/rudolf-power.c   # (1.6.1 Part 11: power and the clock)
	gcc -O2 $HARD -o "$B/root-network" sys/rudolf-network.c   # (1.6.1 Part 11: the network)
	gcc -O2 $HARD -o "$B/root-devices" sys/rudolf-devices.c   # (1.6.1 Part 11: Bluetooth and Thunderbolt)
	gcc -O2 $HARD -o "$B/root-procs" sys/rudolf-procs.c   # (1.6.1 Part 11: Task Manager's End process)
	gcc -O2 $HARD -o "$B/root-logs" sys/rudolf-logs.c   # (1.6.1 Part 11: the kernel's messages)
	gcc -O2 $HARD -o "$B/root-screen" sys/rudolf-screen.c   # (1.6.1 Part 11: the display's settings)
	gcc -O2 $HARD -o "$B/root-sound" sys/rudolf-sound.c   # (1.6.1 Part 11: sound)
	gcc -O2 $HARD -o "$B/root-vpn" sys/rudolf-vpn.c   # (1.6.2 Part 33: the VPN)
	gcc -O2 $HARD -o "$B/rudolf-launch" sys/rudolf-launch.c
	gcc -O2 $HARD -o "$B/rudolf-memguard" sys/rudolf-memguard.c   # 1.4.7: the memory guard
	gcc -O2 $HARD -o "$B/rudolf-shellfix" sys/rudolf-shellfix.c   # 1.5.8: the shell put into the profile when it changed
	gcc -O2 $HARD -o "$B/rudolf-remote" sys/rudolf-remote.c -lssl -lcrypto -lX11 -lXext /usr/lib/$MA/libXtst.so.6 -lz   # 1.5.0: remote access
	gcc -O2 $HARD_STATIC -o "$B/cdp-eval" sys/cdp-eval.c
	# 1.2.7: the Debian sandbox and the Linux self-check, rebuilt here too (before, only a full build made rudolf-debian)
	gcc -O2 $HARD -o "$B/rudolf-debian" sys/rudolf-debian.c -lcrypto -ldl
	gcc -O2 $HARD -o "$B/rudolf-fscheck" sys/rudolf-fscheck.c
	# 1.2.9: the desktop portal and the app catalogue reader for Linux apps (never rebuilt here before either)
	gcc -O2 $HARD -o "$B/rudolf-portal" sys/rudolf-portal.c $(pkg-config --cflags --libs gio-2.0 gio-unix-2.0) -lcrypto
	# 1.6.1 (docs/plan-1.6.1.md Part 20): the website data's key in the TPM, and Chromium's key store
	gcc -O2 $HARD -o "$B/rudolf-webkey" sys/rudolf-webkey.c $(pkg-config --cflags --libs gio-2.0) -lcrypto -ldl
	gcc -O2 $HARD -o "$B/rudolf-apps" sys/rudolf-apps.c -lz
	# 1.6.0: xdg-open and the browser commands inside Debian (static: Debian's C library is its own), in usr/lib/rudolf
	gcc -O2 $HARD_STATIC -o "$B/rudolf-open" sys/rudolf-open.c
	# 1.3.0: the package tool, the assistant and printing too (the same commands as mkrootfs.sh)
	gcc -O2 $HARD -o "$B/rpk" sys/rpk.c -lcrypto
	gcc -O2 $HARD -o "$B/rudolf-ai" sys/rudolf-ai.c -lcrypto -ldl
	gcc -O2 $HARD -o "$B/rudolf-know" sys/rudolf-know.c -lcrypto -ldl -lm
	gcc -O2 $HARD -o "$B/rudolf-dev" sys/rudolf-dev.c -lcrypto -ldl
	gcc -O2 $HARD -o "$B/rudolf-print" sys/rudolf-print.c -ldl
	# 1.6.2 (docs/plan-1.6.2.md Part 30): the LAN game relay
	gcc -O2 $HARD -o "$B/rudolf-duel" sys/rudolf-duel.c
	# 1.6.1 (docs/plan-1.6.1.md Part 9): the print queue (zlib linked in: the base layer's programs do not share it)
	gcc -O2 $HARD -o "$B/rudolf-printq" sys/rudolf-printq.c -Wl,-Bstatic -lz -Wl,-Bdynamic
	# 1.4.0: the development link
	gcc -O2 $HARD -o "$B/rudolf-devlink" sys/rudolf-devlink.c -lcrypto -ldl
	python3 systools/hardcheck.py "$B/rudolf-display" "$B/rudolfd" "$B/rudolf-update" "$B/rudolf-launch" "$B/rudolf-memguard" "$B/rudolf-shellfix" "$B/rudolf-remote" "$B/cdp-eval" "$B/rudolf-debian" "$B/rudolf-fscheck" "$B/rudolf-portal" "$B/rudolf-webkey" "$B/rudolf-open" "$B/rudolf-apps" "$B/rpk" "$B/rudolf-ai" "$B/rudolf-know" "$B/rudolf-dev" "$B/rudolf-print" "$B/rudolf-duel" "$B/rudolf-printq" "$B/rudolf-devlink" "$B/rudolf-rootd" $(for h in $RHELP $RINNER; do echo "$B/root-$h"; done) || { echo "a system program was built without the hardening flags (plan 15.5)"; exit 1; }
	for b in rudolf-display rudolfd rudolf-update rudolf-launch rudolf-memguard rudolf-shellfix rudolf-remote cdp-eval rudolf-debian rudolf-fscheck rudolf-portal rudolf-webkey rudolf-apps rpk rudolf-ai rudolf-know rudolf-dev rudolf-print rudolf-duel rudolf-printq rudolf-devlink rudolf-rootd; do
		for l in $(ldd "$B/$b" | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || { echo "$b needs $l, which the base layer does not have"; exit 1; }; done
		sudo install -m 0755 "$B/$b" "$W/broot/usr/bin/$b"
	done
	# (1.6.1: rudolf-webkey opens SQLite itself when it moves a profile's values over, so ldd does not see it)
	[ -e "$W/broot/usr/lib/$MA/libsqlite3.so.0" ] || { echo "rudolf-webkey opens libsqlite3.so.0, which the base layer does not have"; exit 1; }
	sudo ln -sf rudolf-dev "$W/broot/usr/bin/devlink"   # (1.6.0, plan Part 19: one Rudolf OS computer drives another's development link)
	sudo mkdir -p "$W/broot/usr/lib/rudolf"; sudo install -m 0755 "$B/rudolf-open" "$W/broot/usr/lib/rudolf/rudolf-open"
	# (1.6.1: the root helpers, root:root 0700 in a folder of root's; checked, as the base layer keeps owners and modes:
	# mksquashfs -all-root makes every file root's, and keeps the modes)
	sudo install -d -o root -g root -m 0755 "$W/broot/usr/lib/rudolf/root"
	for h in $RHELP $RINNER; do
		for l in $(ldd "$B/root-$h" | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || { echo "root-$h needs $l, which the base layer does not have"; exit 1; }; done
		sudo install -o root -g root -m 0700 "$B/root-$h" "$W/broot/usr/lib/rudolf/root/$h"
		[ "$(sudo stat -c '%u:%g %a' "$W/broot/usr/lib/rudolf/root/$h")" = "0:0 700" ] || { echo "the root helper $h is not root's 0700 in the base layer"; exit 1; }
	done
	[ "$(sudo stat -c '%u:%g %a' "$W/broot/usr/bin/rudolf-rootd")" = "0:0 755" ] && [ ! -u "$W/broot/usr/bin/rudolf-rootd" ] || { echo "rudolf-rootd is not root's 0755 (and never set-user-ID) in the base layer"; exit 1; }
	# the X server (1.4.3: with GLX), when its build changed
	if [ "$(cat "$W/broot/usr/lib/xorg/rudolf-xsrc" 2>/dev/null)" != "$XSRC" ]; then
		systools/xfetch.sh "$XREV"
		bash xbuild.sh > "$W/xbuild.log" 2>&1 || { tail -40 "$W/xbuild.log"; echo "the X server did not build"; exit 1; }
		XS=src/xorg/stage
		# GLX is linked with glvnd's libGL.so.1 and libGLX.so.0 (libGLdispatch.so.0 is there for glamor already)
		for l in libGL.so.1 libGLX.so.0; do sudo cp -L "/usr/lib/$MA/$l" "$W/broot/usr/lib/$MA/$l"; done
		[ -x "$XS/usr/bin/Xorg" ] && [ -f "$XS/usr/lib/xorg/modules/extensions/libglx.so" ] || { echo "the X server build has no Xorg or GLX module"; exit 1; }
		for f in "$XS/usr/bin/Xorg" $(find "$XS/usr/lib/xorg/modules" -name '*.so'); do
			for l in $(LD_LIBRARY_PATH="$XS/usr/lib/$MA" ldd "$f" | awk '/=>/{print $3}'); do case "$l" in *libxcvt*) continue;; /*) ;; *) echo "$f: a library it needs is missing on the build machine"; exit 1;; esac; [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || { echo "$f needs $l, which the base layer does not have"; exit 1; }; done
		done
		sudo install -m 0755 "$XS/usr/bin/Xorg" "$W/broot/usr/bin/Xorg"
		sudo rm -rf "$W/broot/usr/lib/xorg/modules"; sudo cp -a "$XS/usr/lib/xorg/modules" "$W/broot/usr/lib/xorg/"
		sudo cp -a "$XS/usr/lib/$MA/"libxcvt.so* "$W/broot/usr/lib/$MA/"
		echo "$XSRC" | sudo tee "$W/broot/usr/lib/xorg/rudolf-xsrc" >/dev/null
		NOTES+=("X server with GLX")
	fi
	# 1.5.0: the firewall (nft) and file encryption on RUDOLFDATA (tune2fs -O encrypt), with the libraries they need
	# 1.5.1: Bluetooth: BlueZ (bluetoothd, bluetoothctl) and bluez-alsa (headphones: the daemon and its ALSA plug-in),
	# from this Ubuntu 24.04, with the libraries the base layer lacks
	BTF=$(for p in bluez bluez-alsa-utils libasound2-plugin-bluez; do dpkg -L $p; done 2>/dev/null | grep -E '/(bluetoothd|bluetoothctl|bluealsa|bluealsad)$|alsa-lib/libasound_module_(pcm|ctl)_bluealsa\.so$|alsa/alsa\.conf\.d/[^/]*bluealsa[^/]*\.conf$')
	echo "$BTF" | grep -q '/bluetoothd$' && echo "$BTF" | grep -q '/bluetoothctl$' && echo "$BTF" | grep -qE '/bluealsad?$' || { echo "BlueZ or bluez-alsa is missing on the build machine (apt-get install bluez bluez-alsa-utils libasound2-plugin-bluez): $BTF"; exit 1; }
	for x in $BTF; do [ -f "$x" ] || continue; sudo mkdir -p "$W/broot$(dirname "$x")"; sudo cp -L "$x" "$W/broot$x"
		case "$x" in *.conf) continue;; esac
		for l in $(ldd "$x" | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || sudo cp -L "$l" "$W/broot/usr/lib/$MA/"; done
	done
	# 1.6.0 (plan Part 20): smart cards and the DoD Common Access Card: pcscd (the PC/SC service, started by
	# /usr/lib/rudolf/smartcard when a reader is plugged in), the CCID readers' driver (libccid's bundle), OpenSC's PKCS#11
	# module and pkcs11-tool (Settings' card details, no PIN), and NSS's certutil and modutil (the desktop's certificate
	# database: the card's module for Oracle, the DoD's certificates when an administrator asks). 1.6.1 (plan Part 21, found by
	# the virtual card in QEMU, qt/sb/sc161_t.py): pcsc-lite's client library too, libpcsclite.so.1, which OpenSC opens itself
	# (dlopen, so ldd below does not see it): without it OpenSC's module failed to start (CKR_GENERAL_ERROR), so no reader
	# or card was ever seen and the module was never added to the desktop's database
	SCF=$(for p in pcscd libccid libpcsclite1 opensc opensc-pkcs11 libnss3-tools; do dpkg -L $p; done 2>/dev/null | grep -E '/(pcscd|pkcs11-tool|certutil|modutil)$|/opensc-pkcs11\.so$|/libopensc\.so[.0-9]*$|/libpcsclite\.so\.1$|/pcsc/drivers/|/opensc\.conf$')
	echo "$SCF" | grep -q '/pcscd$' && echo "$SCF" | grep -q '/opensc-pkcs11\.so$' && echo "$SCF" | grep -q '/modutil$' && echo "$SCF" | grep -q 'ifd-ccid' && echo "$SCF" | grep -q '/libpcsclite\.so\.1$' || { echo "pcscd, libccid, libpcsclite1, opensc or libnss3-tools is missing on the build machine (apt-get install pcscd libccid libpcsclite1 opensc opensc-pkcs11 libnss3-tools)"; exit 1; }
	SCB=0
	for x in $SCF; do [ -f "$x" ] || continue; sudo mkdir -p "$W/broot$(dirname "$x")"; sudo cp -L "$x" "$W/broot$x"; SCB=$((SCB + $(stat -c %s "$x")))
		case "$x" in *.conf|*.plist|*Info.plist) continue;; esac
		for l in $(ldd "$x" 2>/dev/null | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || { sudo cp -L "$l" "$W/broot/usr/lib/$MA/"; SCB=$((SCB + $(stat -L -c %s "$l"))); }; done
	done
	SCP=$(echo "$SCF" | grep '/opensc-pkcs11\.so$' | head -1); echo "$SCP" | sudo tee "$W/broot/etc/rudolf-pkcs11" >/dev/null   # (where the card's module is: /usr/lib/rudolf/smartcard)
	echo "  Smart cards (pcscd, CCID, OpenSC, NSS tools): $((SCB / 1024)) KB"; echo "::notice title=Smart cards::$((SCB / 1024)) KB added to the base layer"
	# 1.6.1 (plan-1.6.1 Part 18): security keys that unlock the files: libfido2 (rudolfd opens it with dlopen, so ldd does not
	# see it) and what it links that the base layer does not have yet (libcbor at least)
	FXL=$(dpkg -L libfido2-1 2>/dev/null | grep -E '/libfido2\.so\.1$' | head -1)
	[ -n "$FXL" ] || { echo "libfido2-1 is missing on the build machine (apt-get install libfido2-1)"; exit 1; }
	FXB=0; for x in $FXL $(ldd "$FXL" | awk '/=>/{print $3}'); do [ -e "$W/broot$x" ] || [ -e "$W/broot/usr$x" ] || [ -e "$W/broot/usr/lib/$MA/$(basename "$x")" ] || { sudo cp -L "$x" "$W/broot/usr/lib/$MA/"; FXB=$((FXB + $(stat -L -c %s "$x"))); }; done
	echo "  Security keys (libfido2): $((FXB / 1024)) KB"; echo "::notice title=Security keys::$((FXB / 1024)) KB added to the base layer"
	# 1.5.9 (plan Part 1): ModemManager (read only in 1.5.9: /usr/lib/rudolf/modem) with libqmi, libmbim and libqrtr,
	# its plug-ins and the operators' database (mobile-broadband-provider-info, for 1.6.0's APNs). Its udev rules too:
	# with no udev, ModemManager applies them itself (/usr/lib/rudolf/modem runs it with --test-no-udev).
	# 1.6.0 (plan Part 1): QMI, MBIM, QRTR and AT (the plug-ins, libqrtr-glib for Qualcomm's QRTR modules, qmi-proxy and
	# mbim-proxy); its FCC unlock scripts (off until an administrator turns one on: /usr/lib/rudolf/modem fcc) and what
	# they run (qmicli, mbimcli from libqmi-utils and libmbim-utils; bash for MediaTek's); the sizes printed, no budget
	# (the owner dropped the size budgets for 1.6.0)
	MMF=$(for p in modemmanager mobile-broadband-provider-info libqmi-proxy libmbim-proxy libqmi-utils libmbim-utils; do dpkg -L $p; done 2>/dev/null | grep -E '/(ModemManager|mmcli|qmi-proxy|mbim-proxy|qmicli|mbimcli)$|/ModemManager/[^/]*\.so$|/ModemManager/fcc-unlock\.available\.d/|/serviceproviders\.xml$|/dbus-1/system\.d/org\.freedesktop\.ModemManager1\.conf$|/udev/rules\.d/[0-9]+-mm-[^/]*\.rules$')
	echo "$MMF" | grep -q '/ModemManager$' && echo "$MMF" | grep -q '/mmcli$' || { echo "ModemManager is missing on the build machine (apt-get install modemmanager mobile-broadband-provider-info)"; exit 1; }
	echo "$MMF" | grep -q '/qmicli$' && echo "$MMF" | grep -q '/mbimcli$' || { echo "qmicli or mbimcli is missing on the build machine (apt-get install libqmi-utils libmbim-utils): the FCC unlock scripts need them"; exit 1; }
	echo "$MMF" | grep -q '/fcc-unlock\.available\.d/' || { echo "ModemManager's FCC unlock scripts are not in the modemmanager package"; exit 1; }
	echo "$MMF" | grep -q -- '-mm-candidate\.rules$' || { echo "ModemManager's udev rules (80-mm-candidate.rules) are not in the modemmanager package: without them it sees no modem on Rudolf OS"; exit 1; }
	MMB=0
	for x in $MMF; do [ -f "$x" ] || continue; sudo mkdir -p "$W/broot$(dirname "$x")"; sudo cp -L "$x" "$W/broot$x"; MMB=$((MMB + $(stat -c %s "$x")))
		case "$x" in *.xml|*.conf|*.rules) continue;; esac
		for l in $(ldd "$x" | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || { sudo cp -L "$l" "$W/broot/usr/lib/$MA/"; MMB=$((MMB + $(stat -L -c %s "$l"))); }; done
	done
	# (what it is made of, largest first: each file of the packages and each library it brought)
	MML=$(for x in $MMF; do [ -f "$x" ] && echo "$(stat -c %s "$x") $x"; done; for x in $MMF; do case "$x" in *.xml|*.conf|*.rules) continue;; esac; [ -f "$x" ] && ldd "$x" 2>/dev/null | awk '/=>/{print $3}'; done | sort -u | while read -r l; do [ -n "$l" ] && [ -e "$l" ] && ! echo "$MMF" | grep -qx "$l" && echo "$(stat -L -c %s "$l") $l (library)"; done)
	echo "$MML" | sort -rn | head -15 | awk '{printf "  %6d KB %s %s\n", $1/1024, $2, $3}'
	# (bash, for the FCC unlock script of MediaTek's modules (14c3): the base layer's sh is BusyBox's ash)
	if [ ! -e "$W/broot/bin/bash" ] && [ ! -e "$W/broot/usr/bin/bash" ]; then sudo install -m 0755 /bin/bash "$W/broot/usr/bin/bash"; [ -L "$W/broot/bin" ] || sudo ln -sf /usr/bin/bash "$W/broot/bin/bash"; MMB=$((MMB + $(stat -L -c %s /bin/bash)))
		for l in $(ldd /bin/bash | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || { sudo cp -L "$l" "$W/broot/usr/lib/$MA/"; MMB=$((MMB + $(stat -L -c %s "$l"))); }; done; fi
	echo "  ModemManager and its parts: $((MMB / 1024)) KB in all ($(echo "$MMF" | grep -c 'fcc-unlock\.available\.d/') FCC unlock scripts, $(echo "$MMF" | grep -c '/ModemManager/[^/]*\.so$') plug-ins)"
	echo "::notice title=ModemManager::$((MMB / 1024)) KB added to the base layer (QMI, MBIM, QRTR and AT; no budget in 1.6.0)"
	# 1.6.0 (plan Part 3): tayga, the CLAT for IPv6-only cellular networks (464XLAT: /usr/lib/rudolf/modem starts it)
	[ -x /usr/sbin/tayga ] || { echo "tayga is missing on the build machine (apt-get install tayga)"; exit 1; }
	sudo install -m 0755 /usr/sbin/tayga "$W/broot/usr/sbin/tayga"; echo "  tayga (the CLAT): $(($(stat -c %s /usr/sbin/tayga) / 1024)) KB"
	for l in $(ldd /usr/sbin/tayga | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || sudo cp -L "$l" "$W/broot/usr/lib/$MA/"; done
	# (the hotspot's tools have their own budget, 6 MB: with ModemManager's, the two came to 13 MB in 2026111314's build)
	HSB=0
	# 1.5.9 (plan Part 4): Share this connection: hostapd and hostapd_cli (the access point) and dnsmasq (addresses)
	for x in /usr/sbin/hostapd /usr/sbin/hostapd_cli /usr/sbin/dnsmasq; do
		[ -x "$x" ] || { echo "$x is missing on the build machine (apt-get install hostapd dnsmasq-base)"; exit 1; }
		sudo install -m 0755 "$x" "$W/broot/usr/sbin/${x##*/}"; HSB=$((HSB + $(stat -c %s "$x")))
		for l in $(ldd "$x" | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || { sudo cp -L "$l" "$W/broot/usr/lib/$MA/"; HSB=$((HSB + $(stat -L -c %s "$l"))); }; done
	done
	[ $HSB -lt 6291456 ] || { echo "the hotspot tools add $((HSB / 1048576)) MB to the base layer: over their 6 MB budget (docs/plan-1.5.9.md Part 4)"; exit 1; }
	echo "::notice title=The hotspot::$((HSB / 1024)) KB added to the base layer"
	# 1.5.9 (plan 16.1): Tor, for Oracle's Tor windows (rudolfd starts it only while one is open), with what it needs that the
	# base layer lacks (libevent's), in a budget of its own, 6 MB; and its user (951), which a base layer made before has not
	TRB=0
	for x in /usr/bin/tor; do
		[ -x "$x" ] || { echo "$x is missing on the build machine (apt-get install tor)"; exit 1; }
		sudo install -m 0755 "$x" "$W/broot/usr/bin/tor"; TRB=$((TRB + $(stat -c %s "$x")))
		for l in $(ldd "$x" | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || { sudo cp -L "$l" "$W/broot/usr/lib/$MA/"; TRB=$((TRB + $(stat -L -c %s "$l"))); echo "  Tor needs $l ($(($(stat -L -c %s "$l") / 1024)) KB)"; }; done
	done
	[ $TRB -lt 6291456 ] || { echo "Tor adds $((TRB / 1024)) KB to the base layer: over its 6 MB budget (docs/plan-1.5.9.md 16.1)"; exit 1; }
	echo "::notice title=Tor::$((TRB / 1024)) KB added to the base layer"
	grep -q '^tor:' "$W/broot/etc/passwd" || echo "tor:x:951:951:Tor (Oracle's Tor windows):/run/rudolf/tor:/bin/false" | sudo tee -a "$W/broot/etc/passwd" >/dev/null
	grep -q '^tor:' "$W/broot/etc/group" || echo "tor:x:951:" | sudo tee -a "$W/broot/etc/group" >/dev/null
	# 1.6.1 (docs/plan-1.6.1.md Part 9): the print queue's user (952), in group lp for the USB printers' /dev/usb/lpN
	grep -q '^print:' "$W/broot/etc/passwd" || echo "print:x:952:952:Printing (rudolf-printq):/nonexistent:/bin/false" | sudo tee -a "$W/broot/etc/passwd" >/dev/null
	grep -q '^print:' "$W/broot/etc/group" || echo "print:x:952:" | sudo tee -a "$W/broot/etc/group" >/dev/null
	# 1.6.0 (plan Part 9): udev instead of mdev (rudolf-init starts it; mdev stays as the fallback): systemd-udevd on its
	# own and udevadm, from this Ubuntu 24.04's udev package (no systemd as init), with its helpers, its rules (not the
	# ones for systemd's own seats and consoles, nor the renaming of network interfaces: Rudolf OS's scripts know eth0,
	# wlan0 and the adapters by their kernel names) and the hardware database built from its hwdb.d; the groups its
	# rules name; the libraries the base layer lacks, at the paths they are found at (libsystemd-shared's own folder)
	UDF=$(dpkg -L udev 2>/dev/null | grep -E '^/usr/(bin/udevadm|lib/systemd/systemd-udevd|lib/udev/[^/]+|lib/udev/rules\.d/[^/]+\.rules|lib/udev/hwdb\.d/[^/]+\.hwdb)$')
	echo "$UDF" | grep -q '^/usr/bin/udevadm$' && echo "$UDF" | grep -q '/systemd-udevd$' || { echo "udev is missing on the build machine (apt-get install udev)"; exit 1; }
	UDB=0
	for x in $UDF; do [ -e "$x" ] && [ ! -d "$x" ] || continue
		case "${x##*/}" in 71-seat.rules|73-seat-late.rules|80-net-setup-link.rules|90-vconsole.rules|99-systemd.rules) continue;; esac
		sudo mkdir -p "$W/broot$(dirname "$x")"; sudo cp -a "$x" "$W/broot$x"; [ -L "$x" ] || UDB=$((UDB + $(stat -c %s "$x")))
		case "$x" in *.rules|*.hwdb) continue;; esac
		[ -L "$x" ] && continue; file -b "$x" 2>/dev/null | grep -q ELF || continue
		for l in $(ldd "$x" | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || { sudo mkdir -p "$W/broot$(dirname "$l")"; sudo cp -L "$l" "$W/broot$l"; UDB=$((UDB + $(stat -L -c %s "$l"))); echo "  udev needs $l ($(($(stat -L -c %s "$l") / 1024)) KB)"; }; done
	done
	if command -v systemd-hwdb >/dev/null; then sudo systemd-hwdb --root="$W/broot" update --usr; else sudo udevadm hwdb --update --root="$W/broot" --usr; fi
	[ -s "$W/broot/usr/lib/udev/hwdb.bin" ] || { echo "udev's hardware database (hwdb.bin) was not made"; exit 1; }
	sudo rm -rf "$W/broot/usr/lib/udev/hwdb.d"   # (built into hwdb.bin)
	for g in disk:6 lp:7 kmem:15 dialout:20 cdrom:24 tape:26 render:990 kvm:991 sgx:992; do grep -q "^${g%%:*}:" "$W/broot/etc/group" || echo "${g%%:*}:x:${g#*:}:" | sudo tee -a "$W/broot/etc/group" >/dev/null; done
	grep -q '^lp:.*print' "$W/broot/etc/group" || sudo sed -i 's/^lp:x:7:$/lp:x:7:print/; s/^lp:x:7:\(..*\)$/lp:x:7:\1,print/' "$W/broot/etc/group"   # (1.6.1 Part 9: the print queue's user in lp)
	echo "::notice title=udev::$((UDB / 1024)) KB added to the base layer, its hardware database $(($(stat -c %s "$W/broot/usr/lib/udev/hwdb.bin") / 1024)) KB (read as needed)"
	# 1.6.0 (plan Part 15; 1.5.9 8.4): sbverify, for /usr/lib/rudolf/espcheck: the start-up files on the drive checked as
	# signed by Rudolf OS's Secure Boot key (its certificate is /usr/share/rudolf/rudolf-sb.crt)
	x=/usr/bin/sbverify; [ -x "$x" ] || { echo "$x is missing on the build machine (apt-get install sbsigntool)"; exit 1; }
	sudo install -m 0755 "$x" "$W/broot/usr/bin/sbverify"
	for l in $(ldd "$x" | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || sudo cp -L "$l" "$W/broot/usr/lib/$MA/"; done
	# 1.5.9: and iw: the Wi-Fi chip's bands (Settings › Network › Wi-Fi › This computer's Wi-Fi radios, wifi.sh radios
	# and band); it was never in the base layer, so on the Vivobook (2026111308) those found no bands at all
	# 1.6.2 (docs/plan-1.6.2.md Part 27, Bring your files): cryptsetup, an old Windows disk's BitLocker opened read-only
	# with its recovery key (cryptsetup's BITLK; dm-crypt in the kernel: kconfig.sh)
	for x in /usr/sbin/nft /usr/sbin/tune2fs /usr/sbin/iw /usr/sbin/cryptsetup; do
		[ -x "$x" ] || { echo "$x is missing on the build machine (apt-get install nftables e2fsprogs iw cryptsetup-bin)"; exit 1; }
		sudo install -m 0755 "$x" "$W/broot/usr/sbin/${x##*/}"
		for l in $(ldd "$x" | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || sudo cp -L "$l" "$W/broot/usr/lib/$MA/"; done
	done
	# 1.5.4: ALSA's use case files (alsa-ucm-conf), how PulseAudio sets up the speakers, headphones and microphones of
	# sound processors (Intel SOF, AMD ACP) and USB sound cards (/usr/lib/rudolf/sound)
	[ -f /usr/share/alsa/ucm2/ucm.conf ] || { echo "alsa-ucm-conf is missing on the build machine (apt-get install alsa-ucm-conf)"; exit 1; }
	sudo rm -rf "$W/broot/usr/share/alsa/ucm2"; sudo mkdir -p "$W/broot/usr/share/alsa"; sudo cp -a /usr/share/alsa/ucm2 "$W/broot/usr/share/alsa/"
	sudo cp -a sys/root/. "$W/broot/"
	# 1.4.4: what no longer belongs in the base layer (systools/base-remove.txt)
	grep -v '^#' systools/base-remove.txt | while read -r x; do [ -n "$x" ] || continue; case "$x" in /*|*..*) echo "base-remove.txt: bad path $x"; exit 1;; esac
		if [ -e "$W/broot/$x" ] || [ -L "$W/broot/$x" ]; then sudo rm -rf "$W/broot/$x"; echo "removed from the base layer: $x"; fi; done
	# NVK and the Vulkan loader: OpenGL on GeForce 20 series and newer (Zink on NVK) (1.2.3)
	systools/addnvk.sh "$W/broot"
	# 1.5.7 (plan Part 4): the Lite desktop engine, WebKitGTK, with what it needs (systools/addwebkit.sh)
	gcc -O2 $HARD -Wno-deprecated-declarations -o "$B/rudolf-webkit" sys/rudolf-webkit.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) || { echo "rudolf-webkit did not build (apt-get install libwebkit2gtk-4.1-dev)"; exit 1; }
	# 1.5.9 (plan Part 12): the native desktop engine's prototype, with WebKit
	gcc -O2 $HARD -Wno-deprecated-declarations -o "$B/rudolf-native" sys/rudolf-native.c $(pkg-config --cflags --libs webkit2gtk-4.1 gtk+-3.0) -lX11 -lm || { echo "rudolf-native did not build"; exit 1; }
	python3 systools/hardcheck.py "$B/rudolf-webkit" "$B/rudolf-native" || { echo "rudolf-webkit or rudolf-native was built without the hardening flags (plan 15.5)"; exit 1; }
	systools/addwebkit.sh "$W/broot" "$B/rudolf-webkit" "$B/rudolf-native"
	# 1.6.0 (plan Part 14): the keyring for Linux apps, gnome-keyring-daemon's secrets part (/usr/lib/rudolf/keyring starts
	# it at sign-in, as the account's user), with what it needs that the base layer lacks, in a budget of its own, 3 MB.
	# After WebKit, whose libraries it
	# shares (GLib, GIO, libgcrypt, p11-kit), so only what is the keyring's own is counted
	KRB=0
	for x in /usr/bin/gnome-keyring-daemon; do
		[ -x "$x" ] || { echo "$x is missing on the build machine (apt-get install gnome-keyring)"; exit 1; }
		sudo install -m 0755 "$x" "$W/broot/usr/bin/gnome-keyring-daemon"; KRB=$((KRB + $(stat -c %s "$x")))
		for l in $(ldd "$x" | awk '/=>/{print $3}'); do [ -e "$W/broot$l" ] || [ -e "$W/broot/usr$l" ] || { sudo cp -L "$l" "$W/broot/usr/lib/$MA/"; KRB=$((KRB + $(stat -L -c %s "$l"))); echo "  the keyring needs $l ($(($(stat -L -c %s "$l") / 1024)) KB)"; }; done
	done
	[ $KRB -lt 3145728 ] || { echo "the keyring adds $((KRB / 1024)) KB to the base layer: over its 3 MB budget (docs/plan-1.6.0.md Part 14)"; exit 1; }
	echo "::notice title=The keyring::$((KRB / 1024)) KB added to the base layer"
	sudo mkdir -p "$W/broot/etc/rudolf"; sudo cp trusted-keys.txt "$W/broot/etc/rudolf/rpk-keys"
	# 1.6.0 (plan Part 19): where this build comes from (sys/origin.h): a fork's build names the fork
	sh systools/origin.sh | sudo tee "$W/broot/etc/rudolf/origin" >/dev/null
	sudo cp VERSION "$W/broot/usr/share/rudolf/VERSION"
	# 1.4.4: the build date; rudolf-init never lets the clock read earlier than this (a reset hardware clock)
	date -u +%F | sudo tee "$W/broot/usr/share/rudolf/released" >/dev/null
	echo "$BSRC" | sudo tee "$W/broot/usr/share/rudolf/base-src" >/dev/null
	# 1.6.0 (plan Part 13): the base's ABI (its libraries' hash) for signed system extensions, and where the extensions
	# built for this base are listed (extensions.yml publishes them in the release ext-ABI16, with a signed index)
	ABI=$(sudo sh systools/baseabi.sh "$W/broot"); echo "$ABI" | sudo tee "$W/broot/usr/share/rudolf/base-abi" >/dev/null
	echo "https://github.com/${GITHUB_REPOSITORY:-merciless146/rudolfos}/releases/download/ext-$(echo "$ABI" | cut -c1-16)" | sudo tee "$W/broot/usr/share/rudolf/sysext-source" >/dev/null
	echo "::notice title=Base ABI::$ABI (system extensions for this base: release ext-$(echo "$ABI" | cut -c1-16))"
	sudo $MK "$W/broot" "$W/new/base.sfs" $SQ; sudo chown "$(id -u)" "$W/new/base.sfs"; sudo rm -rf "$W/broot"
	# (1.6.1: and in the layer itself, as the computer will see them)
	for h in $RHELP $RINNER; do unsquashfs -lls "$W/new/base.sfs" "usr/lib/rudolf/root/$h" 2>/dev/null | grep -qE '^-rwx------ root/root .* squashfs-root/usr/lib/rudolf/root/'"$h"'$' || { echo "the root helper $h is not root's 0700 in base.sfs"; unsquashfs -lls "$W/new/base.sfs" "usr/lib/rudolf/root/$h"; exit 1; }; done
	NOTES+=("system programs")
fi

# ---- desktop (byte-identical when iso/www and iso/shell did not change)
mkdir -p "$W/ds/usr/share/rudolf"; cp -a www "$W/ds/usr/share/rudolf/www"
# the shell (an Isolated Web App that gives Oracle real tabs), signed with a key derived from the release key;
# Chromium installs it from these files (policy written by the session). Ed25519 signatures are deterministic,
# so an unchanged shell gives the same bytes.
# 1.5.8: Chromium updates an installed Isolated Web App only to a higher version, and the shell's manifest said 1.5.1
# since 1.5.1: no change to the shell after that reached a computer (the Vivobook still ran the shell of 2026-11-01;
# YouTube's full-screen fix, #206, never arrived). The version is now set here: the previous release's when the
# shell's files did not change (the layer stays byte-identical), else VERSION.SERIAL, higher than any before it.
if [ -d shell ]; then
	I="$W/ds/usr/share/rudolf/www/iwa"; mkdir -p "$I"
	SH=$(cd shell && find . -type f ! -path ./.well-known/manifest.webmanifest | LC_ALL=C sort | xargs sha256sum | sha256sum | cut -c1-32)
	PSH=$(unsquashfs -cat "$PREV/desktop.sfs" usr/share/rudolf/www/iwa/shell.sha 2>/dev/null || true)
	PSV=$(unsquashfs -cat "$PREV/desktop.sfs" usr/share/rudolf/www/iwa/update.json 2>/dev/null | python3 -c 'import json,sys
try: print(json.load(sys.stdin)["versions"][0]["version"])
except Exception: print("")' || true)
	if [ -n "$PSV" ] && [ "$SH" = "$PSH" ]; then SV=$PSV; else SV="$(cat VERSION).${SERIAL:-$(date -u +%Y%m%d%H)}"; fi
	rm -rf "$W/shell"; cp -a shell "$W/shell"
	python3 - "$W/shell/.well-known/manifest.webmanifest" "$SV" <<'PY'
import json, sys
p, v = sys.argv[1], sys.argv[2]; m = json.load(open(p)); m['version'] = v
open(p, 'w').write(json.dumps(m, indent=1, ensure_ascii=False) + '\n')
PY
	echo "$SH" > "$I/shell.sha"
	python3 systools/mkiwa.py derive "$KEYFILE" "$W/iwa.key" > "$I/id"
	python3 systools/mkiwa.py build "$W/shell" "$I/shell.swbn" "$W/iwa.key" >/dev/null
	python3 systools/mkiwa.py update "$W/shell" http://127.0.0.1:8080/iwa/shell.swbn "$I/update.json" >/dev/null
	rm -f "$W/iwa.key"; [ "$SV" = "$PSV" ] || NOTES+=("Oracle's tabs (shell $SV)")
fi
$MK "$W/ds" "$W/desk.sfs" $SQ; python3 systools/verity.py add "$W/desk.sfs" desktop >/dev/null
if ! cmp -s "$W/desk.sfs" "$W/new/desktop.sfs"; then cp "$W/desk.sfs" "$W/new/desktop.sfs"; NOTES+=("desktop update"); fi

# every layer carries its dm-verity hash tree (1.2.2); the same data always gives the same tree
for n in base chromium desktop modules; do python3 systools/verity.py add "$W/new/$n.sfs" $n >/dev/null; done
CHANGED=0; for f in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do cmp -s "$PREV/$f" "$W/new/$f" || CHANGED=1; done
if [ $CHANGED = 0 ]; then echo "nothing changed: no release"; echo "released=0" >> "${GITHUB_OUTPUT:-/dev/null}"; exit 0; fi
NOTE=$(printf '%s, ' "${NOTES[@]}"); NOTE=${NOTE%, }; NOTE=${NOTE:-security fixes}
# the first release of a new version says what is new in it (CHANGELOG.md, the "Summary:" line)
# (1.2.7: also every release while stable is still an older version, so a second beta of the same version, which
# is the one promoted, does not fall back to "system programs, desktop update"; STABLE_MANIFEST: stable's list)
SV=$(sed -n 's/^version //p' "${STABLE_MANIFEST:-/nonexistent}" 2>/dev/null | cut -d. -f1-3)
if { [ "$(pv version | cut -d. -f1-3)" != "$(cat VERSION)" ] && [ "$(pv version)" != "$(cat VERSION)" ]; } || { [ -n "$SV" ] && [ "$SV" != "$(cat VERSION)" ]; }; then
	SUM=$(awk -v v="$(cat VERSION)" '$1=="##"&&$2==v{f=1;next} /^## /{f=0} f&&/^Summary:/{sub(/^Summary: */,"");print;exit}' ../CHANGELOG.md 2>/dev/null)
	[ -n "$SUM" ] && NOTE="$(cat VERSION): $SUM"
fi
NOTE=$(printf '%s' "$NOTE" | tr -d '\n\r' | cut -c1-290)
# 1.6.0 (plan Part 18): this release's source and documents, which Blitzen looks things up in (rudolf-know build --source)
git -C .. archive --format=tar --prefix="rudolf-os-$SERIAL/" HEAD desktop iso libs docs LICENSE README.md CHANGELOG.md | xz -9 -T0 > "$W/source.tar.xz"
python3 systools/make_release.py --layers "$W/new" --kernel "$W/new/vmlinuz" --out "$OUT" --key "$KEYFILE" --serial "$SERIAL" \
	--version "$(cat VERSION).$(date -u +%y%m%d)" --chromium "$CH" --kernel-version "$KV" --notes "$NOTE" --file-base "$FILE_BASE" ${URGENT:+--urgent} --source "$W/source.tar.xz"
echo "released=1" >> "${GITHUB_OUTPUT:-/dev/null}"; echo "notes=$NOTE" >> "${GITHUB_OUTPUT:-/dev/null}"
