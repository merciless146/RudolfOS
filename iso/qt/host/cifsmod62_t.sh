#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 35, step 1): the kernel's SMB client as kconfig.sh builds it. The configuration: CIFS a module,
# without the insecure legacy dialects (SMB 1 and SMB 2.0), without upcalls, RDMA, the cache or the key dump, and no SMB server;
# the module: there, signed (the kernel's appended signature, SHA-256, by the build's own key), loaded by nothing at start (no
# device alias: only the file system's own names, which the kernel asks for when a share is mounted).
#   sh cifsmod62_t.sh CONFIG MODULES-DIR      (MODULES-DIR: lib/modules/KVER of modules_install)   GPL-2.0-or-later.
C=$1; M=$2; [ -r "$C" ] && [ -d "$M" ] || { echo "usage: cifsmod62_t.sh CONFIG MODULES-DIR"; exit 2; }
P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
has() { grep -q "^CONFIG_$1=$2$" "$C"; }
off() { ! grep -qE "^CONFIG_$1=(y|m)$" "$C"; }
ok "1. CIFS is a module" $(has CIFS m; echo $?) "$(grep CONFIG_CIFS= "$C")"
ok "2. no SMB 1 or SMB 2.0 (CIFS_ALLOW_INSECURE_LEGACY off), so no SMB 1 POSIX extensions either" $(off CIFS_ALLOW_INSECURE_LEGACY && off CIFS_POSIX; echo $?) "$(grep -E 'CIFS_(ALLOW|POSIX)' "$C")"
r=0; for o in CIFS_UPCALL CIFS_DFS_UPCALL CIFS_SWN_UPCALL CIFS_SMB_DIRECT CIFS_FSCACHE CIFS_DEBUG_DUMP_KEYS CIFS_DEBUG2; do off $o || { r=1; echo "  $o is on"; }; done
ok "3. no upcalls (Kerberos, DFS, witness), no RDMA, no cache, no session keys in the kernel's log" $r
ok "4. no SMB server (ksmbd): Rudolf OS serves no share" $(off SMB_SERVER; echo $?)
ok "5. every module signed and unsigned ones refused (MODULE_SIG_FORCE, MODULE_SIG_ALL, SHA-256)" $(has MODULE_SIG_FORCE y && has MODULE_SIG_ALL y && has MODULE_SIG_SHA256 y; echo $?)
K=$(find "$M" -name 'cifs.ko*' | head -1)
ok "6. the module is there ($(echo "$K" | sed "s|^$M/||"))" $([ -n "$K" ] && [ -s "$K" ]; echo $?)
T=$(tail -c 28 "$K" 2>/dev/null)
ok "7. it carries the kernel's appended signature" $([ "$T" = "~Module signature appended~" ] || tail -c 28 "$K" | grep -q 'Module signature appended'; echo $?) "$(tail -c 28 "$K" | od -c | head -2)"
if command -v modinfo >/dev/null; then
	S=$(modinfo -F sig_hashalgo "$K" 2>/dev/null); N=$(modinfo -F signer "$K" 2>/dev/null)
	ok "8. signed with SHA-256 by the build's key (\"$N\")" $([ "$S" = sha256 ] && [ -n "$N" ]; echo $?) "hash=$S signer=$N"
	A=$(modinfo -F alias "$K" 2>/dev/null | sort | tr '\n' ' ')
	ok "9. loaded by nothing at start: no device alias, only the file system's names ($A)" $(modinfo -F alias "$K" | grep -vqE '^(fs-)?(cifs|smb3)$' && echo 1 || echo 0) "$A"
	D=$(modinfo -F depends "$K"); ok "10. what it needs is there as modules or built in (depends: ${D:-none})" $(r=0; for d in $(echo "$D" | tr , ' '); do find "$M" -name "$d.ko*" | grep -q . || r=1; done; echo $r)
else echo "(no modinfo: 8 to 10 not checked)"; fi
echo "$P/$((P+F)) passed"; [ $F = 0 ]
