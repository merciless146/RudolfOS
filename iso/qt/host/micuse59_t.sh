#!/bin/sh
# 1.5.9 (plan 7.2): hotkey mic users / mic get / mic muted, with a stand-in pactl (the sound server's answers).
#   sh micuse59_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); H=$HERE/../../sys/root/usr/lib/rudolf/hotkey; T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
cat > $T/pactl <<'E'
#!/bin/sh
case "$*" in
"list source-outputs") printf 'Source Output #12\n\tDriver: protocol-native.c\n\tCorked: no\n\tProperties:\n\t\tapplication.name = "Chromium"\n\t\tmedia.name = "Playback"\n\nSource Output #13\n\tCorked: yes\n\tProperties:\n\t\tapplication.name = "Old Recorder"\n\nSource Output #14\n\tCorked: no\n\tProperties:\n\t\tapplication.name = "ZOOM VoiceEngine"\n' ;;
"get-source-mute @DEFAULT_SOURCE@") cat $T/m 2>/dev/null || echo "Mute: no" ;;
set-source-mute*) [ "$3" = 1 ] && echo "Mute: yes" > $T/m || echo "Mute: no" > $T/m ;;
esac
E
sed -i "s#\$T/m#$T/m#g" $T/pactl; chmod +x $T/pactl
export HOTKEY_PACTL=$T/pactl HOTKEY_EVENTS=$T/ev
o=$(sh $H mic users); ok "1. the apps recording now (not a paused one), one per line" $([ "$o" = "$(printf 'Chromium\nZOOM VoiceEngine')" ] && echo 1 || echo 0) "$o"
o=$(sh $H mic get); ok "2. not muted" $([ "$o" = on ] && echo 1 || echo 0) "$o"
o=$(sh $H mic muted 1); ok "3. muted, and the desktop's indicator told" $([ "$o" = muted ] && [ "$(sh $H mic get)" = muted ] && grep -q "osd mic muted" $T/ev && echo 1 || echo 0) "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
