# Secure Boot

Rudolf OS 1.2.2 starts on UEFI computers with Secure Boot on. This page says how, and what to do when a part
of it has to change.

## The chain

| Step | File on the disc's UEFI image | Checked by | Signed with |
|---|---|---|---|
| 1 | `EFI/BOOT/BOOTX64.EFI`, shim (Ubuntu's `shim-signed`) | the firmware (Microsoft's UEFI CA, in every PC) | Microsoft |
| 2 | `EFI/BOOT/grubx64.efi`, systemd-boot: the boot menu | shim: the key enrolled in its key list (MOK) | the Rudolf OS key |
| 3 | `EFI/rudolf/rudolf.efi`, the kernel bundle: systemd's stub, the kernel with its initramfs, fixed options | shim, through systemd-boot | the Rudolf OS key |
| 3r | `EFI/rudolf/recovery.efi` (1.5.9), the official recovery start-up file, started by the menu's Recovery mode: the same kernel and a second initramfs in the bundle (the recovery mark and the official release key alone); it starts only `system/r` on RUDOLFDATA after checking its signed release list with that key and every file's size and SHA-256 (never Developer mode's extra key) | shim, through systemd-boot | the Rudolf OS key |
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

## Replacing the release key (1.6.3 Part 0)

Rudolf OS's releases are signed by one key, and computers trust the keys in `iso/trusted-keys.txt` (copied into the
system as `/etc/rudolf/rpk-keys`): a computer that has not updated trusts only the keys it shipped with, so a release it
cannot verify is refused. Replacing the key is therefore one change made in one order, never two.

**Where it stands.** Since 2026-11-24 the list holds two keys: the key of 1.4.9 to 1.6.3
(`UqTqXIW8…3mo=`) first, and the key the owner made offline beside it (`IcNJiy4z…cVg=`) second. Recovery mode's own
start-up file carries both (`mkuki.py`), so a computer on a release that ships this list trusts either. Only the first
key may sign: `keycheck.py` refuses a release signed by any other, because the computers that matter cannot verify it
yet.

**The switch, in this order:**

1. Wait until a stable release that ships both keys has been out for a while — the owner decides, and a few weeks is
   what lets computers that update rarely catch up.
2. In this repository: `python3 iso/systools/keyswitch.py --status` (where it stands), then
   `python3 iso/systools/keyswitch.py --switch --yes` (the new key first, the old one after it), then the pull request.
3. In GitHub, at the same time: Settings › Environments › `release` › `RPK_SIGNING_KEY` = **the new key**, and the
   repository secret `RPK_SIGNING_KEY` deleted. A secret can never be read back, so nothing can tell you afterwards
   which key it holds — the release's own `key.txt` and its signature can.
4. Merge the pull request. The next release is signed by the new key; `keycheck.py` refuses it only if the two steps
   above were done in the wrong order, which is the check that keeps a mistake from reaching a computer.
5. Later (Part 0 item 3e), once every supported stable release trusts the new key, the old one leaves the list.

**What proves each step.** `keyswitch-check` runs for every change of `iso/systools/**`, of the list, or of this
document: `keyswitch.py` and its 19 host cases, `deadcheck.py` (whose key gate these paths are for; no workflow ran it
before) with a negative control that breaks a copy of the list and expects deadcheck to refuse it, and on `main` a job
in the `release` environment that fails if `RPK_SIGNING_KEY` is missing. The environment's
Deployment branches are `main` only — read from GitHub on 2026-11-28. A later rotation changes `ROTATION` in
`keyswitch.py` and nothing else: the gate names no key, so a list that has moved past this pair still checks. `deadcheck.py` refuses a list the tools cannot read. On the day, the release's `manifest.sig` and
`beta/key.txt` show which key signed, and a computer on the previous release that updates proves the pair is trusted.

**Not done by a session:** the environment's secret, the repository secret's deletion, and the moment of the switch.
They are the owner's, and the sequence above is the whole of them.
