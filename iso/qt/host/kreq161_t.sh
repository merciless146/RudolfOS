#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 12): the required kernel features' check (systools/kreq.sh, kconfig-required.txt). A
# configuration with all of them passes; with one option of each group taken out (or a module where it must be built in)
# it fails, naming the option and who needs it. With a kernel tree's .config as KCONFIG (kconfig.sh's), that one passes
# and the same options taken out of it fail.   sh kreq161_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); I=$HERE/../..; L=$I/kconfig-required.txt; K=$I/systools/kreq.sh
T=$(mktemp -d); trap 'rm -rf $T' EXIT
P=0; F=0; ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
if [ -n "$KCONFIG" ]; then cp "$KCONFIG" $T/good; else
	awk '$1 !~ /^#/ && NF >= 3 {print "CONFIG_" $1 "=" ($2 == "m" ? "m" : "y")}' $L > $T/good; echo 'CONFIG_SOMETHING_ELSE=y' >> $T/good; fi
o=$(sh $K $L $T/good); ok "1. every required feature there: passes ($(echo "$o" | tail -1))" $? "$o"
# one option of each group (the first after each group's heading), and one that is a module where it must be built in
for g in flatpak debian wine desktop security start-up power; do
	opt=$(awk -v g="$g:" '$1 !~ /^#/ && $3 == g {print $1; exit}' $L); who=$(awk -v o="$opt" '$1 == o {sub(/^[^ ]+ +[^ ]+ +/, ""); print; exit}' $L)
	sed "/^CONFIG_$opt=/d" $T/good > $T/bad; echo "# CONFIG_$opt is not set" >> $T/bad
	o=$(sh $K $L $T/bad); rc=$?
	ok "2. $g: without $opt the check fails, naming it and who needs it" $([ $rc = 1 ] && echo "$o" | grep -q "CONFIG_$opt is not set .* needed by $who" && echo "$o" | grep -q '1 of the'; echo $?) "$o"
done
sed 's/^CONFIG_SQUASHFS=y$/CONFIG_SQUASHFS=m/' $T/good > $T/bad; o=$(sh $K $L $T/bad)
ok "3. the system layers' file system as a module (it must be built in to start): fails" $([ $? = 1 ] && echo "$o" | grep -q 'CONFIG_SQUASHFS is m (wanted: y)'; echo $?) "$o"
sed 's/^CONFIG_FUSE_FS=y$/CONFIG_FUSE_FS=m/' $T/good > $T/bad; o=$(sh $K $L $T/bad)
ok "4. FUSE as a module (allowed: ym): passes" $?  "$o"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
