#!/bin/sh
# 1.5.4: firmware-sof.txt has Intel's sound processor firmware for every platform kconfig.sh builds, under the names
# the kernel asks for (sound/soc/sof/intel/pci-*.c), with topologies for each; fwextra.py copies it from a sof-bin
# checkout (SOF_BIN=..., optional) to those names, with the link Meteor and Arrow Lake look in.
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../..; L=$I/firmware-sof.txt
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
ok "every F line has a hash, a name and where it is in sof-bin" "$(awk '$1=="F" && (NF!=4 || $2 !~ /^[0-9a-f]{64}$/){b++} END{print b?0:1}' $L)"
for p in tgl tgl-h ehl adl adl-s adl-n rpl rpl-s; do
	ok "IPC3 firmware for $p" "$(grep -q "^F [0-9a-f]* intel/sof/sof-$p.ri v2.2.x/sof-v2.2/intel-signed/" $L && echo 1)"
done
for p in mtl arl arl-s lnl ptl wcl; do
	ok "IPC4 firmware for $p" "$(grep -q "^F [0-9a-f]* intel/sof-ipc4/$p/sof-$p.ri v2.15.x/sof-ipc4-v2.15/[a-z-]*/intel-signed/" $L && echo 1)"
done
for t in sof/sof-tplg/sof-hda-generic-2ch sof/sof-tplg/sof-tgl- sof/sof-tplg/sof-adl- sof/sof-tplg/sof-rpl- sof/sof-ipc4-tplg/sof-hda-generic-2ch sof/sof-ipc4-tplg/sof-mtl- sof/sof-ipc4-tplg/sof-arl- sof/sof-ipc4-tplg/sof-lnl- sof/sof-ipc4-tplg/sof-ptl-; do
	ok "topologies: ${t#sof/}" "$(grep -q " intel/${t#sof/}" $L && echo 1)"
done
ok "no community-signed firmware, no boards that are not built" "$(grep -E 'community/|sof-(nvl|imx|byt|bdw|apl|glk|cnl|icl|jsl)' $L | grep -q . && echo 0 || echo 1)"
ok "Meteor and Arrow Lake's topology folder is a link" "$(grep -qx 'L intel/sof-ace-tplg sof-ipc4-tplg' $L && echo 1)"
# the platforms kconfig.sh builds are the ones listed
for k in TIGERLAKE ELKHARTLAKE ALDERLAKE METEORLAKE LUNARLAKE PANTHERLAKE; do
	ok "kconfig.sh builds $k" "$(grep -q "SND_SOC_SOF_$k" $I/kconfig.sh && echo 1)"
done
if [ -n "$SOF_BIN" ]; then
	T=$(mktemp -d); trap 'rm -rf $T' EXIT
	O=$(python3 $I/systools/fwextra.py copy $L "$SOF_BIN" $T/fw 2>&1)
	ok "copied from sof-bin, every hash checked" "$([ $? = 0 ] && echo 1)" "$O"
	ok "firmware under the kernel's names" "$([ -s $T/fw/intel/sof/sof-rpl.ri ] && [ -s $T/fw/intel/sof-ipc4/mtl/sof-mtl.ri ] && echo 1)"
	ok "the link leads to the topologies" "$([ -s $T/fw/intel/sof-ace-tplg/sof-hda-generic-2ch.tplg ] && echo 1)"
	cp $L $T/bad; sed -i '2s/ \([0-9a-f]\)/ 0/' $T/bad
	O=$(python3 $I/systools/fwextra.py copy $T/bad "$SOF_BIN" $T/fw2 2>&1)
	ok "a changed file stops the build" "$([ $? != 0 ] && echo "$O" | grep -q 'is not the listed file' && echo 1)" "$O"
fi
echo "$((N-F))/$N passed"; [ $F = 0 ]
