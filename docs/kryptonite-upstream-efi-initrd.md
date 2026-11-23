# Draft upstream report: a kexec'd kernel writes into the previous kernel's INITRD config table (not sent)

WendigOS (2.0), the kryptonite branch. A draft for the owner: **nothing here has been sent anywhere.** If the
owner decides to report it, it goes to the EFI maintainers (Ard Biesheuvel; linux-efi@vger.kernel.org, Cc
kexec@lists.infradead.org and the reporter of the original problem), as a reply to the thread of the commit below or as a
new patch. The patch Rudolf's 7.3 build carries is `iso/systools/kpatches/efi-initrd-table-kexec.patch`.

---

**Subject:** [REGRESSION] efi: kexec'd kernel corrupts itself writing to stale LINUX_EFI_INITRD table (6.19+)

Hello,

since 6e62d1c6f2c7 ("efi: Wipe INITRD config table from memory after consumption", merged for v6.19-rc6), a kernel
started by kexec_file_load() on x86-64 can panic at once while unpacking its own built-in initramfs:

    Kernel panic - not syncing: ZSTD-compressed data is corrupt

(or "XZ-compressed data is corrupt" with an xz initramfs). Reproduced with 6.19.0, 6.19.14, 7.0.14, 7.1.13, 7.2.15 and
7.3.5; 6.18.64 is fine.

Setup: OVMF with Secure Boot (QEMU q35, 1.5 GB of RAM, KVM), systemd-boot starting a UKI (systemd-stub) that has an
`.initrd` section. The kernel in it (CONFIG_KEXEC_FILE, lockdown=integrity, built-in initramfs) loads a second signed
bzImage with kexec_file_load() (with an initrd of its own, an empty cpio) and reboots into it. The second kernel panics as above.

- 6.18 → 7.3 fails; 7.3 → 6.18 passes: only the *receiving* kernel matters.
- 3 GB of RAM instead of 1.5 GB passes (where things land in memory); nokaslr on the second kernel changes nothing.
- A bisect between v6.19-rc1 and v6.19 (first-parent, then into the merge) ends at the efi-fixes-for-v6.19-2 merge
  (bc08b658803) and its commit 6e62d1c6f2c7; v6.19-rc6-era mainline with that one line removed passes.

Cause, as far as we can tell: the UKI's stub makes the first kernel's EFI stub install the LINUX_EFI_INITRD_MEDIA config
table, allocated as EFI_LOADER_DATA. The first kernel treats that memory as free RAM after ExitBootServices. The config
table array is handed to the kexec'd kernel (SETUP_EFI → efi_reuse_config), so in efi_config_parse_tables() the second
kernel finds the old table's address and, since 6e62d1c6f2c7, writes 16 zero bytes there. That memory may by then hold
anything; in our case part of the second kernel's own image (its .init.ramfs), hence the corrupt initramfs. Reading the
table was already unsafe for the same reason (the stale-initrd case the commit was meant to fix: the reader trusts a
pointer into memory the previous kernel freed); 6e62d1c6f2c7 only helps when the previous kernel was a 6.19+ one, and
even then the second kernel still writes.

What we carry (against 7.3.5): skip the INITRD table altogether when the kernel was started by kexec (x86: efi_setup
set, i.e. the loader passed SETUP_EFI); a kexec loader passes any initrd in boot_params:

```diff
+static bool __init efi_booted_by_kexec(void)
+{
+#ifdef CONFIG_X86
+	return efi_setup != 0;
+#else
+	return false;
+#endif
+}
...
 	if (IS_ENABLED(CONFIG_BLK_DEV_INITRD) &&
-	    initrd != EFI_INVALID_TABLE_ADDR && phys_initrd_size == 0) {
+	    initrd != EFI_INVALID_TABLE_ADDR && phys_initrd_size == 0 &&
+	    !efi_booted_by_kexec()) {
```

Other ways it could be fixed upstream, which you may prefer: have the first kernel memblock-reserve the 16-byte table
(as LINUX_EFI_MEMRESERVE does) so it stays valid across kexec and the wipe is safe; or have the stub allocate it from a
type the kernel keeps (EFI_ACPI_RECLAIM_MEMORY); or drop the table's entry from the config table array passed to the
kexec'd kernel. arm64 and other EFI architectures pass the config tables across kexec too and may need the same.

The serial logs, the configuration and the bisect's runs can be shared on request.

---

*(For the owner: before sending, check whether a fix is already on linux-efi or in a newer stable release; the 7.4-rc4
tree of 2026-11-22 still has the write. Who sends it, and under which name, is the owner's decision.)*
