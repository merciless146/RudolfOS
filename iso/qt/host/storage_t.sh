#!/bin/sh
# 1.5.6 (1.5.5's plan item 4): storage-measure, what fills the disk, on a stand-in tree: each line counts its own
# folders once (an account's Downloads is the downloads store, measured on its own; Steam's games are taken out of
# the Linux apps), per account, and the answer is valid JSON.   sh storage_t.sh   (needs python3)
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/storage-measure; T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
mb() { mkdir -p "$(dirname "$1")"; dd if=/dev/urandom of="$1" bs=1048576 count=$2 2>/dev/null; }
P=$T/persist; H=$T/home; FP=$T/flatpak; DL=$T/dlstore
mb $P/accounts/ann1/Documents/a.bin 3; mb $P/accounts/ann1/Pictures/b.bin 2; mb $P/accounts/ann1/.Trash/old.bin 4
mb $P/accounts/bob2/Documents/c.bin 1
mb $DL/ann1/big.iso 5; mb $DL/top.deb 1
mb $P/accounts/ann1/Downloads/disc.iso 2   # an account's own Downloads (1.5.6: in its folder on the disk)
mb $FP/app/x.bin 2; mb $H/.var/app/com.discordapp.Discord/d.bin 1
mb $H/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/AmongUs/g.bin 6; mb $H/.var/app/com.valvesoftware.Steam/.local/share/Steam/ubuntu12_32/s.bin 1
mb $T/vault/debian/usr/x.bin 3; ln -s $T/vault/debian $P/debian
mb $H/Games/rom.bin 1; mb $P/ai/model.gguf 2; mb $P/os/a/sys.img 2
STORAGE_PERSIST=$P STORAGE_HOME=$H STORAGE_FLATPAK=$FP STORAGE_DL=$DL sh $S $T/out.json
J=$T/out.json
ok "it writes valid JSON" "$(python3 -c "import json;json.load(open('$J'))" 2>/dev/null && echo 1)" "$(cat $J)"
v() { python3 -c "import json,sys;d=json.load(open('$J'));x=d$1;print(round(x/1048576,1))"; }
near() { python3 -c "import sys;sys.exit(0 if abs($1-$2)<=0.1 else 1)"; }
near $(v "['items']['files']") 6 && ok "files: the accounts' folders, without Downloads and the Trash (3 + 2 + 1 MiB)" 1 || ok "files" 0 "$(v "['items']['files']")"
near $(v "['items']['trash']") 4 && ok "trash: the accounts' .Trash (4 MiB)" 1 || ok "trash" 0 "$(v "['items']['trash']")"
near $(v "['items']['downloads']") 8 && ok "downloads: the account's Downloads and the store (2 + 5 + 1 MiB)" 1 || ok "downloads" 0 "$(v "['items']['downloads']")"
near $(v "['items']['steam']") 6 && ok "steam: Steam's games (6 MiB)" 1 || ok "steam" 0 "$(v "['items']['steam']")"
near $(v "['items']['flatpak']") 4 && ok "flatpak: the apps and their data without Steam's games (2 + 1 + 1 MiB)" 1 || ok "flatpak" 0 "$(v "['items']['flatpak']")"
near $(v "['items']['debian']") 3 && ok "debian: through its link (3 MiB)" 1 || ok "debian" 0 "$(v "['items']['debian']")"
near $(v "['items']['games']") 1 && near $(v "['items']['ai']") 2 && near $(v "['items']['updates']") 2 && ok "Emu's games, Blitzen's model, the system's slots" 1 || ok "games/ai/updates" 0 "$(cat $J)"
ok "each account on its own (ann: 5 MiB of files, 4 in the Trash, 2 in Downloads; bob: 1)" "$(python3 -c "
import json;d=json.load(open('$J'));a={x['id']:x for x in d['accounts']};m=lambda b:round(b/1048576,1)
print(1 if m(a['ann1']['files'])==5 and m(a['ann1']['trash'])==4 and m(a['ann1']['downloads'])==2 and m(a['bob2']['files'])==1 else 0)")" "$(cat $J)"
ok "the disk's size and free space are there" "$(python3 -c "import json;d=json.load(open('$J'))['disk'];print(1 if d['total']>0 and d['free']>0 and d['used']>0 else 0)")"
echo "$((N-F))/$N passed"; [ $F = 0 ]
