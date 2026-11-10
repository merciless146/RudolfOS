#!/bin/sh
# 1.5.7 (plan stage 9): rudolf-debian's runfile/keepfile and devices: the device names placed in Debian (cameras,
# controllers), the files it runs (the person's own only), and the launcher's Exec quoting (read back by its own
# launcher rules). Also the wizard's endpoints in rudolfd.   sh debfiles_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
gcc -w -o $W/t $HERE/fixtures/debfiles.c -ldl -lcrypto 2>$W/cc || { cat $W/cc; echo "FAIL build"; exit 1; }
o=$($W/t dev video0 video12 media1 v4l-subdev3 hidraw4 uinput videox vide0 video0a snd sda input event3)
ok "1. cameras, hidraw and uinput go into Debian; nothing else" "$([ "$(echo "$o" | grep -c '=1$')" = 6 ] && echo "$o" | grep -q '^video0=1' && echo "$o" | grep -q '^uinput=1' && echo "$o" | grep -q '^videox=0' && echo "$o" | grep -q '^video0a=0' && echo "$o" | grep -q '^sda=0' && echo 1)" "$o"
ok "2. a file in the person's folders may be run; others not" "$([ "$($W/t file /home/rudolf/Downloads/App.AppImage)" = 1 ] && [ "$($W/t file /etc/passwd)" = 0 ] && [ "$($W/t file /home/rudolf/Downloads/../../etc/x)" = 0 ] && [ "$($W/t file "$(printf '/home/rudolf/a\nb')")" = 0 ] && echo 1)" ""
q=$($W/t quote '/home/rudolf/Downloads/My "Game" $HOME `x` a\b.AppImage')
ok "3. Exec quoting: the path in quotes, with \" \\ \$ and \` escaped" "$([ "$q" = '"/home/rudolf/Downloads/My \"Game\" \$HOME \`x\` a\\b.AppImage"' ] && echo 1)" "$q"
C=$HERE/../../sys/rudolfd.c
ok "4. rudolfd: runfile and keepfile take only .AppImage, .bin or .run among the person's files" "$(grep -q 'if (!strcmp(what, "runfile") || !strcmp(what, "keepfile"))' $C && grep -q '".AppImage"' $C && grep -q '".run"' $C && echo 1)" ""
echo "$P/$((P+F)) passed"; [ $F = 0 ]
