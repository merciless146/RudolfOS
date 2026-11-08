#!/bin/sh
# 1.5.4: appextra gives Steam the drives (/media, for library folders) and the 32-bit graphics drivers that match its
# runtime (Proton), says when they are missing, retries a download that stops, and does nothing for other apps.
# A stand-in flatpak.
HERE=$(cd "$(dirname "$0")" && pwd); X=$HERE/../../sys/root/usr/lib/rudolf/appextra
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
B=$T/bin; mkdir -p $B; export ST=$T/st; mkdir -p $ST; export RUDOLF_TEST_PATH=$B RUDOLF_RETRY_WAIT=0
cat > $B/flatpak <<'Y'
#!/bin/sh
echo "flatpak $*" >> $ST/log
case "$1" in
info) shift; [ "$1" = --system ] && shift; m=0; [ "$1" = -m ] && { m=1; shift; }
	[ -e "$ST/has-$(echo "$1" | tr / _)" ] || exit 1
	[ $m = 1 ] && printf '[Application]\nname=com.valvesoftware.Steam\nruntime=org.freedesktop.Platform/x86_64/24.08\nsdk=org.freedesktop.Sdk/x86_64/24.08\n'; exit 0;;
install) n=$(cat $ST/fails 2>/dev/null || echo 0); if [ "$n" -gt 0 ]; then echo $((n-1)) > $ST/fails; exit 1; fi
	for a; do :; done; touch "$ST/has-$(echo "$a" | tr / _)"; exit 0;;
override) exit 0;;
esac
Y
chmod +x $B/flatpak
L() { cat $ST/log 2>/dev/null; }
sh $X org.example.Other; ok "another app: nothing to do" "$([ $? = 0 ] && [ -z "$(L)" ] && echo 1)" "$(L)"
sh $X --all; ok "--all without Steam: nothing to do" "$([ $? = 0 ] && [ "$(has "$(L)" install)" = 0 ] && echo 1)" "$(L)"
touch $ST/has-com.valvesoftware.Steam; : > $ST/log
sh $X com.valvesoftware.Steam --check; ok "Steam without the 32-bit drivers: --check says so" "$([ $? != 0 ] && echo 1)"
sh $X com.valvesoftware.Steam --quick; ok "--quick: the drives, no download" "$([ "$(has "$(L)" "override --system --filesystem=/media com.valvesoftware.Steam")" = 1 ] && [ "$(has "$(L)" install)" = 0 ] && echo 1)" "$(L)"
: > $ST/log; echo 2 > $ST/fails; O=$(sh $X com.valvesoftware.Steam); R=$?
ok "the drivers matching Steam's runtime (24.08)" "$(has "$(L)" "install --system -y --noninteractive --or-update flathub org.freedesktop.Platform.GL32.default//24.08")" "$(L)"
ok "a download that stops is tried again" "$([ $R = 0 ] && [ "$(grep -c ' install ' $ST/log)" = 3 ] && [ "$(has "$O" "Trying again")" = 1 ] && echo 1)" "$R $O"
sh $X com.valvesoftware.Steam --check; ok "then --check is content" "$([ $? = 0 ] && echo 1)"
rm $ST/has-org.freedesktop.Platform.GL32.default__24.08; echo 5 > $ST/fails
sh $X --all >/dev/null; ok "a download that keeps failing: an error" "$([ $? != 0 ] && echo 1)"
ok "rudolfd: after an install, after Update all, and when Steam is opened" "$(c=$HERE/../../sys/rudolfd.c; grep -q 'appextra %s ||' $c && grep -q 'appextra --all; echo Done' $c && grep -q 'appextra com.valvesoftware.Steam --check' $c && echo 1)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
