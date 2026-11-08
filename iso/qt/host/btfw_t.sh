#!/bin/sh
# 1.5.4: the Bluetooth firmware in iso/firmware-extra.txt covers the files the drivers ask for, by the names they build
# at run time (btintel: intel/ibt-<cnvi>-<cnvr>.sfi and .ddc; btmtk: mediatek/BT_RAM_CODE_MT79xx_1_x_hdr.bin; btusb:
# qca/rampatch_usb_<ver>.bin and qca/nvm_usb_<ver>.bin), for the chips the 1.5.4 plan names. A file is listed when it
# has an "F" line (pinned by SHA-256) or an "L" line (a link to a pinned file). And kconfig.sh asks for UART Bluetooth.
HERE=$(cd "$(dirname "$0")" && pwd); L=$HERE/../../firmware-extra.txt; K=$HERE/../../kconfig.sh
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
listed() { awk -v f="$1" '($1=="F" && $3==f) || ($1=="L" && $2==f) {x=1} END {exit !x}' $L; }
# a link must point at a file that is itself listed (as linux-firmware's WHENCE makes them: relative to its folder)
resolves() { t=$(awk -v f="$1" '$1=="L" && $2==f {print $3}' $L); [ -z "$t" ] && return 0; d=$(dirname "$1"); T=$(realpath -m "/$d/$t" | cut -c2-); listed "$T" || awk -v p="$T/" '($1=="F" && index($3,p)==1) || ($1=="L" && index($2,p)==1) {x=1} END {exit !x}' $L; }   # (or a folder link: files listed in it)
for c in "AX200:intel/ibt-20-1-3" "AX201:intel/ibt-19-0-4" "AX203:intel/ibt-0040-1020" "AX210:intel/ibt-0041-0041" \
         "AX211:intel/ibt-0040-0041" "AX411:intel/ibt-0040-4150" "BE200:intel/ibt-0291-0291" "BE201:intel/ibt-0190-0291" \
         "9260:intel/ibt-18-16-1" "9560:intel/ibt-17-16-1" "8265:intel/ibt-12-16" "8260:intel/ibt-11-5"; do
	n=${c%%:*}; f=${c#*:}
	ok "Intel $n: $f.sfi and .ddc" $(listed $f.sfi && listed $f.ddc && resolves $f.sfi && resolves $f.ddc && echo 1 || echo 0)
done
for f in mediatek/BT_RAM_CODE_MT7961_1_2_hdr.bin mediatek/BT_RAM_CODE_MT7922_1_1_hdr.bin mediatek/mt7925/BT_RAM_CODE_MT7925_1_1_hdr.bin; do
	ok "MediaTek: $f" $(listed $f && echo 1 || echo 0)
done
for f in qca/rampatch_usb_00130201.bin qca/nvm_usb_00130201.bin qca/rampatch_usb_00190200.bin; do
	ok "Qualcomm on USB: $f" $(listed $f && echo 1 || echo 0)
done
ok "Realtek (1.5.1) still there: rtl_bt/rtl8821c_fw.bin" $(listed rtl_bt/rtl8821c_fw.bin && echo 1 || echo 0)
ok "every link in the list points at a listed file" $(awk '$1=="L"{print $2}' $L | while read f; do resolves $f || echo "$f"; done | grep -q . && echo 0 || echo 1) "$(awk '$1=="L"{print $2}' $L | while read f; do resolves $f || echo "$f"; done | head -3)"
ok "no file is pinned twice" $([ -z "$(awk '$1=="F"{print $3}' $L | sort | uniq -d)" ] && echo 1 || echo 0) "$(awk '$1=="F"{print $3}' $L | sort | uniq -d | head -3)"
ok "kconfig.sh asks for UART Bluetooth (hci_uart, serdev) and checks it" $(grep -q '^m BT_HCIUART$' $K && grep -q '^y SERIAL_DEV_BUS SERIAL_DEV_CTRL_TTYPORT \$BTUART$' $K && grep -q 'the Bluetooth option \$o is not on' $K && echo 1 || echo 0)
echo "$((N-F))/$N passed"; [ $F = 0 ]
