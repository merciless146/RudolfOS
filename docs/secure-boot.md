# Secure Boot

Rudolf OS 1.2.2 starts on UEFI computers with Secure Boot on. This page says how, and what to do when a part
of it has to change.

## The chain

| Step | File on the disc's UEFI image | Checked by | Signed with |
|---|---|---|---|
| 1 | `EFI/BOOT/BOOTX64.EFI`, shim (Ubuntu's `shim-signed`) | the firmware (Microsoft's UEFI CA, in every PC) | Microsoft |
| 2 | `EFI/BOOT/grubx64.efi`, systemd-boot: the boot menu | shim: the key enrolled in its key list (MOK) | the Rudolf OS key |
| 3 | `EFI/rudolf/rudolf.efi`, the kernel bundle: systemd's stub, the kernel with its initramfs, fixed options | shim, through systemd-boot | the Rudolf OS key |
| 4 | the system layers (`RUDOLF/*.SFS`) | dm-verity, against the root hashes in the signed release list `RUDOLF/MANIFEST.TXT` | the release key (Ed25519) |
| 5 | an updated kernel in a slot on RUDOLFDATA (kexec) | the running kernel: the Rudolf OS certificate is built in as trusted | the Rudolf OS key |

- MokManager (`EFI/BOOT/mmx64.efi`) is where a person enrolls the Rudolf OS key the first time
  (`EFI/rudolf/rudolf.cer`), or confirms the request made by Settings > Security Settings > Prepare Secure
  Boot (rudolfd: `POST /api/secureboot/prepare`, MokNew and MokAuth with a one-time password).
- With Secure Boot on, the initramfs locks the kernel down (integrity) before anything else runs: unsigned
  modules (all modules are signed when the kernel is built, with a key made for that build), unsigned kernels
  (kexec) and writing to kernel memory are refused. A layer that fails its check, or a missing signed list,
  stops the start-up with a restart instead of a shell. rudolfd ignores the desktop page on RUDOLFDATA
  (`os/current`), which nothing checks.
- The boot menu's options are not taken from the menu (the stub ignores them under Secure Boot); the
  initramfs reads which entry was chosen from the firmware variable `LoaderEntrySelected` and adds the
  matching option (`rudolf.recovery=1`, `rudolf.check=1`, `rudolf.toram=1`; `rudolf.debug=1` only with Secure
  Boot off).
- With Secure Boot off everything works as before, and the layers are still checked: a damaged one is caught,
  but an unchecked one may start (Task Manager > System says so).
- BIOS computers start as before (Rudolf Boot). 32-bit UEFI computers are not supported with Secure Boot on.

## Keys

- **Rudolf OS Secure Boot key** (RSA-2048): made by `iso/systools/mksbkey.sh` on the maintainer's computer.
  The private key is only the repository secret `RUDOLF_SB_KEY`; the certificate is
  `iso/boot/sb/rudolf-sb.crt`. The release build checks that they match (`systools/sbkeycheck.py`).
  If the key is ever replaced, every computer must enroll the new certificate (MokManager) before it starts
  with Secure Boot on.
- **Release key** (Ed25519, `RPK_SIGNING_KEY`): signs the release lists, as before; the initramfs trusts the
  keys in `iso/trusted-keys.txt`.

## Keeping shim current

The parts a disc ships are pinned in `iso/boot/sb/parts.txt` and kept in the draft release `sb-parts` by the
workflow *Secure Boot parts*. Microsoft revokes old shims from time to time (SBAT and the firmware's
revocation list); a disc with a revoked shim no longer starts with Secure Boot on. Then: run *Secure Boot
parts*, check the new versions in `sb-parts.txt` of that release, put the new hashes in `parts.txt`, and make a
new disc. Rudolf OS's own SBAT line (`rudolf,1`, in `systools/mkuki.py`) is raised if a Rudolf OS kernel
bundle must be withdrawn.
