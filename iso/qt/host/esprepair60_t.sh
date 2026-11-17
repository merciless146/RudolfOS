#!/bin/bash
# 1.6.0 (docs/plan-1.6.0.md Part 15; 1.5.9 8.4, 8.5): the official recovery repaired. rudolf-update at each start
# (boot-ok): the official recovery system (slot r) checked (its list signed by the official key, its files' sizes, once
# a week their SHA-256), made again from an official system when it is not right, left for the next official one
# otherwise, a 1.5.8 stick's unmarked rescue system left alone; recovery-fetch downloads it from the official address
# (checked with the official key only, files already here linked), and Developer mode on asks for it when there is
# none. espcheck: the drive's start-up files (rudolf.efi, recovery.efi) checked as signed by Rudolf OS; the release's
# boot image downloaded, checked (its checksum, both files official) and staged; refused when not. Stand-ins for the
# images (tar instead of FAT and ISO 9660), mount, sbverify, the download and bootfiles.sh. Throwaway keys.   bash esprepair60_t.sh
set -u; cd "$(dirname "$0")"; I=$(cd ../.. && pwd); T=$(mktemp -d); P=0; F=0; trap 'kill $HP 2>/dev/null; rm -rf "$T"' EXIT
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $(echo "$3" | head -c 600)}"; fi; }
RU=$T/ru; gcc -O2 -w -o $RU $I/sys/rudolf-update.c -lcrypto -ldl || exit 1
python3 - "$T" <<'P'
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives import serialization as s
t=sys.argv[1]
for n in ('official','other'):
    k=Ed25519PrivateKey.generate()
    open(f'{t}/{n}.key','w').write(base64.b64encode(k.private_bytes(s.Encoding.Raw,s.PrivateFormat.Raw,s.NoEncryption())).decode())
    open(f'{t}/{n}.pub','w').write(base64.b64encode(k.public_key().public_bytes(s.Encoding.Raw,s.PublicFormat.Raw)).decode()+'\n')
P
cp $T/official.pub $T/keys; mkdir -p $T/media/rudolf $T/sys
rel() { local OUT=$1 SER=$2 K=$3; rm -rf $T/l $OUT; mkdir -p $T/l
	for f in base chromium desktop modules; do head -c 200000 /dev/urandom > $T/l/$f.sfs; done; head -c 100000 /dev/urandom > $T/l/vmlinuz
	python3 $I/systools/make_release.py --layers $T/l --kernel $T/l/vmlinuz --out $OUT --key $T/$K.key --serial $SER --version 1.6.0 ${FB:+--file-base $FB} >/dev/null 2>&1 || { echo "make_release failed"; exit 1; }; }
slot() { local S=$1 SER=$2 K=$3; rel $T/out $SER $K; rm -rf $T/sys/$S; mkdir -p $T/sys/$S
	cp $T/l/*.sfs $T/l/vmlinuz $T/out/channel/manifest.txt $T/out/channel/manifest.sig $T/sys/$S/
	printf 'serial %s\nversion 1.6.0\n' $SER > $T/sys/$S/RELEASE; echo 1 > $T/sys/$S/complete; }
run() { RU_NOAUTOFETCH=${NOAF-1} RU_NOESP=${NOESP-1} RU_SYSD=$T/sys RU_MEDIA=$T/media RU_CONF=$T/conf RU_KEYS=$T/keys RU_SLOT=$T/slot RU_CMDLINE=$T/cmd RU_MOUNTS=$T/mounts RU_SYSBLOCK=$T/sysblock RU_EXTRA=$T/extra $RU "$@" 2>&1; }
: > $T/cmd; : > $T/mounts
FB=https://github.example/releases/download/r2026111501 slot a 2026111501 official; printf 'good=a\nprev=\nnext=\ntries=0\n' > $T/sys/state; echo a > $T/slot
run boot-ok >/dev/null
ok "1. the official system started: its recovery system made (as in 1.5.9), and checked at the next start: nothing to do" "$([ -e $T/sys/r/complete ] && o=$(run boot-ok) && [ -z "$o" ] && echo 0)"
head -c 200000 /dev/urandom > $T/r.sfs; rm $T/sys/r/desktop.sfs; head -c 100 /dev/urandom > $T/sys/r/desktop.sfs
o=$(run boot-ok)
ok "2. a recovery system file changed (another size): said, made again from the official system" "$(echo "$o" | grep -q "recovery system's desktop is missing or changed" && echo "$o" | grep -q 'made again from this official system' && [ $(stat -c %s $T/sys/r/desktop.sfs) = 200000 ] && cmp -s $T/sys/r/desktop.sfs $T/sys/a/desktop.sfs && echo 0)" "$o"
printf '%s' "$(head -c 200000 /dev/urandom | base64 -w0 | head -c 200000)" > $T/x; rm $T/sys/r/base.sfs; cp $T/x $T/sys/r/base.sfs; echo 0 > $T/sys/r/checked
o=$(run boot-ok)
ok "3. the same size but other bytes: found by the weekly SHA-256 check, made again" "$(echo "$o" | grep -q "base does not match its signed release list" && cmp -s $T/sys/r/base.sfs $T/sys/a/base.sfs && echo 0)" "$o"
cp $T/other.key $T/x.key; python3 - $T <<'P'
import sys,base64
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
t=sys.argv[1]; k=Ed25519PrivateKey.from_private_bytes(base64.b64decode(open(f'{t}/other.key').read()))
m=open(f'{t}/sys/r/manifest.txt','rb').read(); open(f'{t}/sys/r/manifest.sig','wb').write(k.sign(m))
P
o=$(run boot-ok)
ok "4. its release list signed by another key: made again" "$(echo "$o" | grep -q 'made again from this official system' && cmp -s $T/sys/r/manifest.sig $T/sys/a/manifest.sig && echo 0)" "$o"
slot b 2026111502 other; printf 'good=b\nprev=a\nnext=\ntries=0\n' > $T/sys/state; echo b > $T/slot; rm $T/sys/r/vmlinuz
o=$(run boot-ok)
ok "5. a custom build running: a damaged recovery system is removed and said, not made from the custom build" "$(echo "$o" | grep -q 'next official release that starts makes it again' && [ ! -e $T/sys/r ] && echo 0)" "$o"
mkdir -p $T/sys/r; echo 1 > $T/sys/r/complete; o=$(run boot-ok)
ok "...a 1.5.8 stick's rescue system (no official mark) left alone" "$([ -e $T/sys/r/complete ] && ! echo "$o" | grep -q 'was not as it should be' && echo 0)" "$o"
rm -rf $T/sys/r
# recovery-fetch from a stand-in official address (http, on this machine only)
rel $T/srv 2026111503 official; mkdir -p $T/srv/stable; cp $T/srv/channel/manifest.* $T/srv/stable/; cp $T/srv/channel/manifest.* $T/srv/
( cd $T/srv && exec python3 -m http.server 18911 --bind 127.0.0.1 >/dev/null 2>&1 ) & HP=$!
for i in $(seq 50); do curl -s -o /dev/null http://127.0.0.1:18911/stable/manifest.txt && break; sleep 0.1; done
cp $T/srv/files/*/desktop.sfs $T/sys/b/desktop.sfs 2>/dev/null || true
o=$(RU_OFFICIAL_URL=http://127.0.0.1:18911/stable run recovery-fetch)
ok "6. recovery-fetch: the newest release at the official address, checked with the official key, kept as the official recovery system" "$([ -e $T/sys/r/complete ] && grep -q 'downloaded from the official address' $T/sys/r/official && grep -q 'serial 2026111503' $T/sys/r/RELEASE && echo "$o" | grep -q 'downloaded' && echo 0)" "$o"
ok "...a file already on this computer with the same SHA-256 is linked, not downloaded" "$(echo "$o" | grep -q 'desktop: already on this computer' && echo 0)" "$o"
o=$(RU_OFFICIAL_URL=http://127.0.0.1:18911/stable run recovery-fetch); ok "...asked again: already there" "$(echo "$o" | grep -q 'already on this computer' && echo 0)" "$o"
rm -rf $T/sys/r; rel $T/srv2 2026111504 other; cp $T/srv2/channel/manifest.* $T/srv/stable/
o=$(RU_OFFICIAL_URL=http://127.0.0.1:18911/stable run recovery-fetch)
ok "7. a release list there signed by another key: refused, nothing kept" "$(echo "$o" | grep -q 'not signed by the official Rudolf OS key' && [ ! -e $T/sys/r ] && [ ! -e $T/sys/r.new ] && echo 0)" "$o"
o=$(run developer on "$(cat $T/other.pub)" https://dev.example/rudolf)
ok "8. Developer mode on, with no official recovery system: it is downloaded (said)" "$(echo "$o" | grep -q 'no official recovery system on this computer yet: it is being downloaded' && echo 0)" "$o"
run developer off >/dev/null
# espcheck, with stand-ins
mkdir -p $T/bin; cat > $T/bin/mount <<'S'
#!/bin/sh
eval "dst=\${$#}"; src=; for a; do [ "$a" = "$dst" ] && break; src=$a; done
[ -f "$src" ] && tar -xf "$src" -C "$dst" 2>/dev/null
S
printf '#!/bin/sh\nfind "$1" -mindepth 1 -delete 2>/dev/null; exit 0\n' > $T/bin/umount
printf '#!/bin/sh\neval "f=\\${$#}"; grep -q SIGNED-BY-RUDOLF "$f"\n' > $T/bin/sbverify
chmod 755 $T/bin/*
efi() { rm -rf $T/e; mkdir -p $T/e/EFI/rudolf; printf "start %s" "$1" > $T/e/EFI/rudolf/rudolf.efi; printf "recovery %s" "$2" > $T/e/EFI/rudolf/recovery.efi; (cd $T/e && tar -cf "$3" EFI); }
efi SIGNED-BY-RUDOLF SIGNED-BY-RUDOLF $T/esp.img
E() { rm -f $T/dl/tried; ESP_AUTOREPAIR=1 ESP_TRIES=${ESP_TRIES:-1} ESP_WAIT=0 ESP_PATH=$T/bin ESP_IMG=$T/esp.img SB_CERT=/dev/null ESP_SYSD=$T/sys ESP_DL=$T/dl ESP_HEALTH="echo HEALTH" ESP_FETCH="$T/bin/fetch" ESP_STAGE="$T/bin/stage" sh $I/sys/root/usr/lib/rudolf/espcheck "$@" 2>&1; }
o=$(E status); ok "9. espcheck status: both start-up files signed by Rudolf OS" "$([ "$o" = "start=official recovery=official" ] && echo 0)" "$o"
efi custom SIGNED-BY-RUDOLF $T/esp.img; o=$(E status); r=$?
ok "...a custom build's rudolf.efi: said (and not 0)" "$([ "$o" = "start=other recovery=official" ] && [ $r != 0 ] && echo 0)" "$o"
# the release's boot image beside its files (slot a's manifest names vmlinuz's address)
mkdir -p $T/web; efi SIGNED-BY-RUDOLF SIGNED-BY-RUDOLF $T/bi-efi.img; rm -rf $T/bi; mkdir -p $T/bi/efi $T/bi/rudolf; cp $T/bi-efi.img $T/bi/efi/efiboot.img; (cd $T/bi && tar -cf $T/web/boot.iso efi rudolf); (cd $T/web && sha256sum boot.iso > boot.iso.sha256)
U=$(awk -F'\t' '$1=="L" && $2=="vmlinuz" {print $5}' $T/sys/a/manifest.txt)
cat > $T/bin/fetch <<S
#!/bin/sh
echo "\$1" >> $T/fetched; case "\$1" in */boot.iso) cp $T/web/boot.iso "\$2";; */boot.iso.sha256) cp $T/web/boot.iso.sha256 "\$2";; *) exit 3;; esac
S
printf '#!/bin/sh\necho "staged $1 sha=$RUDOLF_IMG_SHA" >> %s/staged\n' $T > $T/bin/stage; chmod 755 $T/bin/*
o=$(rm -f $T/dl/tried $T/fetched $T/staged; ESP_TRIES=1 ESP_WAIT=0 ESP_PATH=$T/bin ESP_IMG=$T/esp.img SB_CERT=/dev/null ESP_SYSD=$T/sys ESP_DL=$T/dl ESP_HEALTH="echo HEALTH" ESP_FETCH="$T/bin/fetch" ESP_STAGE="$T/bin/stage" sh $I/sys/root/usr/lib/rudolf/espcheck boot a 2>&1)
ok "10a. (2026-11-16) espcheck boot without ESP_AUTOREPAIR: a start-up file not official is only said in the health log; nothing downloaded, nothing staged" "$(echo "$o" | grep -q '^HEALTH boot the start-up files on this drive are not all the official ones (start=other recovery=official); they are not changed automatically' && [ ! -e $T/fetched ] && [ ! -e $T/staged ] && echo 0)" "$o"
o=$(E boot a)
ok "10. espcheck boot (an official system): a start-up file not official: said in the health log, the release's boot image fetched from beside its files" "$(echo "$o" | grep -q '^HEALTH boot the start-up files on this drive are not all the official ones (start=other recovery=official)' && grep -qx "${U%/vmlinuz}/boot.iso" $T/fetched && echo 0)" "$o $(cat $T/fetched 2>&1)"
ok "...checked (checksum, both files official) and put into the drive's EFI system partition (bootfiles.sh), said" "$(grep -q "staged $T/dl/boot.iso sha=$(cut -c1-64 $T/web/boot.iso.sha256)" $T/staged && echo "$o" | grep -q 'official start-up files of release 2026111501 were put back in the drive.s EFI system partition' && echo 0)" "$o $(cat $T/staged 2>&1)"
efi SIGNED-BY-RUDOLF SIGNED-BY-RUDOLF $T/esp.img; rm -f $T/staged; o=$(E boot a)
ok "...both official: nothing" "$([ -z "$o" ] && [ ! -e $T/staged ] && echo 0)" "$o"
efi SIGNED-BY-RUDOLF other $T/esp.img; efi SIGNED-BY-RUDOLF changed $T/bi-efi.img; cp $T/bi-efi.img $T/bi/efi/efiboot.img; (cd $T/bi && tar -cf $T/web/boot.iso efi rudolf); (cd $T/web && sha256sum boot.iso > boot.iso.sha256)
o=$(E boot a)
ok "11. a boot image whose recovery.efi is not signed by Rudolf OS: refused, nothing staged" "$(echo "$o" | grep -q 'holds files not signed by Rudolf OS (start=official recovery=other)' && [ ! -e $T/staged ] && [ ! -e $T/dl/boot.iso ] && echo 0)" "$o"
echo 0000 > $T/web/boot.iso.sha256; o=$(E repair a)
ok "...one that does not match its checksum: refused" "$(echo "$o" | grep -q 'does not match its checksum' && [ ! -e $T/staged ] && echo 0)" "$o"
E2() { ESP_AUTOREPAIR=1 ESP_TRIES=1 ESP_WAIT=0 ESP_PATH=$T/bin ESP_IMG=$T/esp.img SB_CERT=/dev/null ESP_SYSD=$T/sys ESP_DL=$T/dl ESP_HEALTH="echo HEALTH" ESP_FETCH="$T/bin/fetch" ESP_STAGE="$T/bin/stage" sh $I/sys/root/usr/lib/rudolf/espcheck "$@" 2>&1; }
efi SIGNED-BY-RUDOLF other $T/esp.img; o=$(E boot a >/dev/null; E2 boot a)
ok "...a refused image: at each start at most once a day" "$(echo "$o" | grep -q 'tried less than a day ago' && echo 0)" "$o"
mv $T/bin/fetch $T/bin/fetch.ok; printf '#!/bin/sh\nexit 3\n' > $T/bin/fetch; chmod 755 $T/bin/fetch; o=$(E boot a)
ok "...no network: said, and tried again at the next start (not counted as the day's try)" "$(o2=$(E2 boot a); echo "$o" | grep -q 'could not be downloaded (tried again at the next start)' && echo "$o2" | grep -q 'could not be downloaded' && echo 0)" "$o"
# (1.6.0, found in QEMU: the first heartbeat often comes before Wi-Fi is connected) the network a little later
(cd $T/web && sha256sum boot.iso > boot.iso.sha256); efi SIGNED-BY-RUDOLF SIGNED-BY-RUDOLF $T/bi-efi.img; cp $T/bi-efi.img $T/bi/efi/efiboot.img; (cd $T/bi && tar -cf $T/web/boot.iso efi rudolf); (cd $T/web && sha256sum boot.iso > boot.iso.sha256)
printf '#!/bin/sh\nn=$(cat %s/fails 2>/dev/null || echo 0); [ $n -lt 2 ] && { echo $((n+1)) > %s/fails; exit 3; }; exec %s/bin/fetch.ok "$@"\n' $T $T $T > $T/bin/fetch; chmod 755 $T/bin/fetch; rm -f $T/staged
o=$(ESP_TRIES=3 E boot a)
ok "...the network comes up a little later: waited for (attempts a while apart), then downloaded, checked and staged" "$(grep -q "staged $T/dl/boot.iso" $T/staged && [ "$(cat $T/fails)" = 2 ] && ! echo "$o" | grep -q 'could not be downloaded' && echo 0)" "$o"
o=$(E repair x); ok "...a slot that is not a, b or r: refused" "$(echo "$o" | grep -q usage && echo 0)" "$o"
grep -q 'esp_check(bs);' $I/sys/rudolf-update.c && grep -q '"repair", (char \*)T' $I/sys/rudolf-update.c
ok "12. rudolf-update runs it at each start of an official system (boot-ok), and Return to official stages the official files at once" "$([ $? = 0 ] && echo 0)"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
