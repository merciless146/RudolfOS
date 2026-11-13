#!/bin/sh
# Rudolf OS kernel configuration for ARM64 (1.5.9, docs/plan-1.5.9.md Part 3), on top of the arm64 defconfig. What
# Rudolf OS itself needs (the start-up, the checked system layers, updates by kexec, memory, lockdown, Waydroid's
# binder) as in kconfig.sh, the x86-only parts left out (Intel and AMD platform drivers, microcode, laptop hotkeys,
# x86 sound and cameras), and the virtual machine the QEMU checks use (virtio graphics, input, disk and network).
# Real ARM64 hardware (phones, Snapdragon and Rockchip laptops) comes later, device by device.
#   kconfig-arm64.sh LINUX_DIR "INITRAMFS_SOURCE"     (SB_CERT: the Secure Boot certificate, as for kconfig.sh)
set -e
cd "$1"
make ARCH=arm64 defconfig >/dev/null
C=scripts/config
y(){ for o in "$@"; do $C --enable $o; done; }
n(){ for o in "$@"; do $C --disable $o; done; }
m(){ for o in "$@"; do $C --module $o; done; }
$C --set-str LOCALVERSION "-rudolf"
$C --set-str DEFAULT_HOSTNAME "rudolf"
y MODULES MODULE_COMPRESS_NONE USER_NS
# updates: the disc's kernel hands over to an updated kernel in a slot (rudolf-kexec, ARM64 system calls)
y KEXEC KEXEC_FILE
# boot: UEFI (an EFI application, compressed: EFI_ZBOOT), the built-in initramfs; the boot loader's command line is
# extended with Rudolf OS's own
y EFI EFI_STUB EFI_ZBOOT EFI_PARTITION MSDOS_PARTITION BLK_DEV_INITRD RD_XZ RD_GZIP
$C --set-str CMDLINE "quiet loglevel=3 consoleblank=0 vt.global_cursor_default=0"
$C --set-str INITRAMFS_SOURCE "$2"
y INITRAMFS_COMPRESSION_XZ
# (1.5.9: the screen's /dev/fb0 comes from the graphics driver's fbdev client, which since Linux 6.13 needs the client
#  selection: without it the QEMU check's virtio graphics had /dev/dri but no fb0, and the session stopped)
y SYSFB_SIMPLEFB DRM DRM_SIMPLEDRM DRM_CLIENT_SELECTION DRM_CLIENT_SETUP DRM_FBDEV_EMULATION DRM_CLIENT_DEFAULT_FBDEV FB FRAMEBUFFER_CONSOLE FB_EFI DRM_PANEL_ORIENTATION_QUIRKS
n LOGO
y CPU_FREQ_DEFAULT_GOV_SCHEDUTIL
# the virtual machine of the QEMU checks: virtio graphics (with 3D), input (a touchscreen), disk, network
y VIRTIO VIRTIO_PCI VIRTIO_MMIO VIRTIO_BLK VIRTIO_NET VIRTIO_INPUT VIRTIO_CONSOLE HW_RANDOM_VIRTIO
m DRM_VIRTIO_GPU
# file systems the start-up and the system use
y SQUASHFS SQUASHFS_XZ SQUASHFS_ZLIB SQUASHFS_FILE_DIRECT ISO9660_FS JOLIET ZISOFS OVERLAY_FS EXT4_FS VFAT_FS MSDOS_FS EXFAT_FS TMPFS DEVTMPFS DEVTMPFS_MOUNT NLS_CODEPAGE_437 NLS_ISO8859_1 FUSE_FS
y BLK_DEV_LOOP BLK_DEV_NVME BLK_DEV_SD USB_STORAGE USB_UAS MMC MMC_BLOCK
y USB USB_XHCI_HCD USB_XHCI_PCI USB_XHCI_PLATFORM HID HID_GENERIC USB_HID HID_MULTITOUCH INPUT_EVDEV INPUT_UINPUT HIDRAW UHID
# signed modules and kernels, lockdown, checked system layers (as kconfig.sh)
y MODULE_SIG MODULE_SIG_FORCE MODULE_SIG_ALL MODULE_SIG_SHA256
$C --set-str MODULE_SIG_HASH sha256
$C --set-str MODULE_SIG_KEY "certs/signing_key.pem"
y SYSTEM_TRUSTED_KEYRING SECONDARY_TRUSTED_KEYRING
$C --set-str SYSTEM_TRUSTED_KEYS "${SB_CERT:?SB_CERT: the Secure Boot certificate (PEM)}"
y KEXEC_SIG KEXEC_IMAGE_VERIFY_SIG SIGNED_PE_FILE_VERIFICATION
y SECURITY SECURITYFS SECURITY_LOCKDOWN_LSM SECURITY_LOCKDOWN_LSM_EARLY LOCK_DOWN_KERNEL_FORCE_NONE
$C --set-str LSM "lockdown,landlock,yama"
y MD BLK_DEV_DM DM_VERITY CRYPTO_SHA256 EFIVAR_FS
# memory (as kconfig.sh 1.4.7): zram with zstd, memory pressure for the memory guard, memory groups
y SWAP ZSMALLOC ZRAM ZRAM_BACKEND_ZSTD CRYPTO_ZSTD PSI LRU_GEN LRU_GEN_ENABLED CGROUPS MEMCG
n PSI_DEFAULT_DISABLED ZRAM_DEF_COMP_LZORLE ZRAM_DEF_COMP_LZO
y ZRAM_DEF_COMP_ZSTD
# the firewall, Bluetooth's basics, encrypted home folders
y NETFILTER NETFILTER_ADVANCED NF_CONNTRACK NF_TABLES NF_TABLES_INET NF_TABLES_IPV4 NF_TABLES_IPV6 NFT_CT NFT_LIMIT NFT_LOG NFT_REJECT NFT_REJECT_INET
y FS_ENCRYPTION CRYPTO_AES CRYPTO_XTS CRYPTO_CTS CRYPTO_CBC CRYPTO_ECB CRYPTO_SHA512 CRYPTO_HKDF
y BT BT_BREDR BT_LE BT_HIDP
m BT_HCIBTUSB
# sound: USB sound and virtio sound (the QEMU checks)
y SOUND SND
m SND_USB_AUDIO SND_VIRTIO
# mobile data (as kconfig.sh 1.5.8): USB modems and phones sharing their connection
m USB_NET_RNDIS_HOST USB_NET_CDC_NCM USB_NET_CDC_MBIM USB_NET_QMI_WWAN USB_SERIAL_OPTION
y USB_USBNET USB_SERIAL WWAN
# Android apps (Waydroid, Part 5)
y ANDROID_BINDER_IPC ANDROID_BINDERFS
$C --set-str ANDROID_BINDER_DEVICES ""
n DEBUG_INFO DEBUG_INFO_DWARF5 DEBUG_INFO_DWARF4 DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT DEBUG_INFO_BTF KALLSYMS_ALL FTRACE
$C --enable DEBUG_INFO_NONE
make ARCH=arm64 olddefconfig >/dev/null
# what Rudolf OS cannot start without is there
for o in DRM_FBDEV_EMULATION KEXEC_FILE EFI_STUB SQUASHFS OVERLAY_FS BLK_DEV_LOOP DM_VERITY EXT4_FS SECURITY_LOCKDOWN_LSM MODULE_SIG_FORCE \
	SWAP ZSMALLOC ZRAM ZRAM_DEF_COMP_ZSTD PSI LRU_GEN CGROUPS MEMCG VIRTIO_PCI VIRTIO_BLK VIRTIO_INPUT ANDROID_BINDER_IPC ANDROID_BINDERFS NF_TABLES; do
	grep -q "^CONFIG_$o=y$" .config || { echo "kconfig-arm64.sh: $o is not on after olddefconfig"; exit 1; }
done
for o in DRM_VIRTIO_GPU SND_USB_AUDIO USB_NET_CDC_NCM USB_NET_QMI_WWAN; do grep -q "^CONFIG_$o=m$" .config || { echo "kconfig-arm64.sh: $o is not a module after olddefconfig"; exit 1; }; done
grep -q "^CONFIG_PSI_DEFAULT_DISABLED=y" .config && { echo "kconfig-arm64.sh: PSI is off by default"; exit 1; }
true
