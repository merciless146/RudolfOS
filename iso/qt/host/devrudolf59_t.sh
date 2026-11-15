#!/bin/sh
# 2026-11-14 (found on the Vivobook): another Rudolf OS stick plugged in shows in the File Explorer: only the disk
# Rudolf OS runs from is hidden (a partition of it in use outside /media), not every disk with a RUDOLFDATA or an ISO
# 9660 partition; another stick's start-up parts (ISO 9660, RUDOLF EFI) are not shown and its RUDOLFDATA is read-only.
# devplug's own functions (system_disk, rudolf_part) with a stand-in blkid and mount table.   sh devrudolf59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); DP=$HERE/../../sys/root/usr/lib/rudolf/devplug
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
mkdir -p $W/bin; cat > $W/bin/blkid <<'X'
#!/bin/sh
case "$1" in
/dev/sda1) echo '/dev/sda1: LABEL="RUDOLF_BOOT_1_4_1" TYPE="iso9660"';; /dev/sda2) echo '/dev/sda2: LABEL="RUDOLF EFI" UUID="5255-4446" TYPE="vfat"';;
/dev/sda3) echo '/dev/sda3: LABEL="RUDOLFDATA" UUID="d0b6" TYPE="ext4"';; /dev/sdb1) echo '/dev/sdb1: LABEL="STICK" TYPE="vfat"';;
/dev/nvme0n1p3) echo '/dev/nvme0n1p3: LABEL="RUDOLFDATA" TYPE="ext4"';;
esac
X
chmod +x $W/bin/blkid
# the functions alone (running the whole script would mount drives)
sed -n '/^MOUNTS=/,/^mount_part()/p' $DP | sed '$d' > $W/f.sh
printf '/dev/nvme0n1p3 /persist ext4 rw 0 0\n/dev/loop0 /live/media iso9660 ro 0 0\n/dev/sdc1 /media/sdc1 vfat rw 0 0\n' > $W/mounts
r() { env PATH=$W/bin:$PATH RUDOLF_MOUNTS=$W/mounts RUDOLF_SYS=$W sh -c ". $W/f.sh; $*"; }
ok "1. the disk Rudolf OS runs from (its RUDOLFDATA in use) is its own" $(r system_disk nvme0n1; echo $?)
ok "2. another Rudolf OS stick (ISO 9660, EFI, RUDOLFDATA, none in use) is not" $(r system_disk sda; [ $? = 1 ]; echo $?)
ok "...nor a drive mounted under /media" $(r system_disk sdc; [ $? = 1 ]; echo $?)
ok "3. its start-up parts are not shown (ISO 9660, RUDOLF EFI)" $([ "$(r rudolf_part sda1)" = boot ] && [ "$(r rudolf_part sda2)" = boot ]; echo $?)
ok "...its RUDOLFDATA is (read-only)" $([ "$(r rudolf_part sda3)" = data ]; echo $?)
ok "...an ordinary stick is ordinary" $([ -z "$(r rudolf_part sdb1)" ]; echo $?)
grep -q 'mount_part $p ro' $DP && grep -q 'Rudolf OS data (another Rudolf OS, read-only)' $DP; ok "4. block_add mounts another stick's RUDOLFDATA read-only and names it so" $?
echo "$P/$((P+F)) passed"; [ $F = 0 ]
