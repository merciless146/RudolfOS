#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 12): the kernel features Rudolf OS relies on (iso/kconfig-required.txt) are all in this
# configuration. Run by kconfig.sh after olddefconfig; on a missing one it prints its name and who needs it and fails.
#   kreq.sh REQUIRED-LIST .config     GPL-2.0-or-later.
L=$1; C=$2; [ -r "$L" ] && [ -r "$C" ] || { echo "usage: kreq.sh REQUIRED-LIST .config"; exit 2; }
F=0; N=0
while read -r o want who; do
	case "$o" in ''|'#'*) continue;; esac
	N=$((N + 1)); v=$(sed -n "s/^CONFIG_$o=//p" "$C" | head -1)
	case "$want:$v" in y:y|m:m|ym:y|ym:m) ;; *) echo "kconfig.sh: CONFIG_$o is ${v:-not set} (wanted: $want) - needed by $who"; F=$((F + 1));; esac
done < "$L"
[ $F = 0 ] && { echo "kconfig.sh: the $N required kernel features are there"; exit 0; }
echo "kconfig.sh: $F of the $N required kernel features are missing (iso/kconfig-required.txt)"; exit 1
