#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 15, item 1): the app's own setting first. rudolf-apps asks an app on the list
# (iso/sys/root/usr/share/rudolf/titlebars) to use the system's title bar, once, only where the person has not chosen:
# Chromium's Preferences (browser.custom_chrome_frame false) and VS Code's settings.json (window.titleBarStyle native), for
# Flatpak's (~/.var/app/ID/config) and Debian's apps (~/.config); an existing choice kept, other settings kept, the files
# still valid JSON (VS Code's with its comments), links not followed, nothing for an app not on the list; and through
# "rudolf-apps launch" as Rudolf starts a Flatpak app. Needs gcc, zlib's headers and python3.   sh titlebars161_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d /tmp/titlebars161.XXXXXX)
trap 'rm -rf $T' EXIT
gcc -O2 -o $T/ra $SYS/rudolf-apps.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
export RA_TITLEBARS=$SYS/root/usr/share/rudolf/titlebars
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
H=$T/h; mkdir -p $H; tb() { $T/ra titlebar "$1" "${2:-$H}" 2>>$T/log; }
jget() { python3 -c "import json,sys; d=json.load(open(sys.argv[1])); print(json.dumps(eval(sys.argv[2])))" "$1" "$2" 2>&1; }
# VS Code's settings: JSON with comments and trailing commas (its own parser's), read here with those taken out
jcget() { python3 -c "
import json,re,sys
t=open(sys.argv[1]).read(); t=re.sub(r'//[^\n]*','',t); t=re.sub(r'/\*.*?\*/','',t,flags=re.S); t=re.sub(r',\s*([}\]])',r'\1',t)
d=json.loads(t); print(json.dumps(eval(sys.argv[2])))" "$1" "$2" 2>&1; }
CP=$H/.var/app/org.chromium.Chromium/config/chromium/Default/Preferences
tb org.chromium.Chromium
ok "1. Chromium (Flathub), never started: its Preferences made with the system's title bar" "$([ "$(jget $CP 'd')" = '{"browser": {"custom_chrome_frame": false}}' ] && echo 1)" "$(cat $CP)"
ok "2. ...once: marked for this account (~/.local/state/rudolf-titlebars)" "$([ -f $H/.local/state/rudolf-titlebars/org.chromium.Chromium ] && echo 1)"
rm $CP; tb org.chromium.Chromium
ok "3. ...not again (the person may have changed it since)" "$([ ! -e $CP ] && echo 1)"
H=$T/h2; mkdir -p $H; BP=$H/.var/app/com.brave.Browser/config/BraveSoftware/Brave-Browser/Default/Preferences; mkdir -p $(dirname $BP)
printf '%s' '{"profile":{"browser":{"x":1},"name":"Me"},"browser":{"window_placement":{"left":10},"has_seen_welcome_page":true},"extensions":{"a":"}{\"browser\":"}}' > $BP
tb com.brave.Browser
ok "4. Brave, started before: the key added to its top-level \"browser\", every other setting kept (valid JSON)" "$([ "$(jget $BP 'd["browser"]["custom_chrome_frame"]')" = false ] && [ "$(jget $BP '[d["browser"]["window_placement"],d["profile"],d["extensions"]]')" = '[{"left": 10}, {"browser": {"x": 1}, "name": "Me"}, {"a": "}{\"browser\":"}]' ] && echo 1)" "$(cat $BP)"
H=$T/h3; mkdir -p $H; EP=$H/.var/app/com.microsoft.Edge/config/microsoft-edge/Default/Preferences; mkdir -p $(dirname $EP)
printf '%s' '{"browser":{"custom_chrome_frame":true},"x":2}' > $EP; cp $EP $T/e0; tb com.microsoft.Edge
ok "5. Edge, its own title bar chosen by the person: left as it is" "$(cmp -s $EP $T/e0 && grep -q 'keeps its own' $T/log && echo 1)" "$(cat $EP)"
VP=$H/.var/app/com.vivaldi.Vivaldi/config/vivaldi/Default/Preferences; mkdir -p $(dirname $VP); printf '%s' '{ "profile": {"browser": {}} , "y": [1,{"browser":{}}] }' > $VP; tb com.vivaldi.Vivaldi
ok "6. Vivaldi, no top-level \"browser\" (only inside others): one added at the top, the others untouched" "$([ "$(jget $VP 'd')" = '{"browser": {"custom_chrome_frame": false}, "profile": {"browser": {}}, "y": [1, {"browser": {}}]}' ] && echo 1)" "$(cat $VP)"
GP=$H/.var/app/com.google.Chrome/config/google-chrome/Default/Preferences; mkdir -p $(dirname $GP); printf '%s' '{"browser":{}}' > $GP; tb com.google.Chrome
ok "7. Chrome with an empty \"browser\": the key alone in it (no stray comma)" "$([ "$(jget $GP 'd')" = '{"browser": {"custom_chrome_frame": false}}' ] && echo 1)" "$(cat $GP)"
VS=$H/.var/app/com.visualstudio.code/config/Code/User/settings.json; mkdir -p $(dirname $VS)
printf '%s\n' '// my settings {' '{' '  /* the font */ "editor.fontSize": 14, // big' '  "files.autoSave": "afterDelay",' '}' > $VS; tb com.visualstudio.code
ok "8. VS Code (Flathub): window.titleBarStyle native added; its comments and settings kept" "$([ "$(jcget $VS '[d["window.titleBarStyle"],d["editor.fontSize"],d["files.autoSave"]]')" = '["native", 14, "afterDelay"]' ] && grep -q '// my settings {' $VS && grep -q '/\* the font \*/' $VS && echo 1)" "$(cat $VS)"
CS=$H/.var/app/com.vscodium.codium/config/VSCodium/User/settings.json; mkdir -p $(dirname $CS); printf '{\n\t"window.titleBarStyle": "custom"\n}\n' > $CS; cp $CS $T/c0; tb com.vscodium.codium
ok "9. VSCodium with the app's own title bar chosen: kept" "$(cmp -s $CS $T/c0 && echo 1)" "$(cat $CS)"
H=$T/h4; mkdir -p $H; VS=$H/.var/app/com.visualstudio.code/config/Code/User/settings.json; mkdir -p $(dirname $VS); printf '{ }' > $VS; tb com.visualstudio.code
ok "10. VS Code with an empty settings file ({ }): the key alone (strict JSON)" "$([ "$(jget $VS 'd')" = '{"window.titleBarStyle": "native"}' ] && echo 1)" "$(cat $VS)"
tb deb:chromium
ok "11. Debian's Chromium (deb:chromium): in ~/.config/chromium" "$([ "$(jget $H/.config/chromium/Default/Preferences 'd["browser"]["custom_chrome_frame"]')" = false ] && echo 1)" "$(ls -R $H/.config 2>&1 | head -5)"
H=$T/h5; mkdir -p $H/.var/app/org.chromium.Chromium/config/chromium/Default; echo '{"secret":1}' > $T/other.json
ln -s $T/other.json $H/.var/app/org.chromium.Chromium/config/chromium/Default/Preferences; tb org.chromium.Chromium
ok "12. a link where the file should be: not followed (the file it points to unchanged)" "$([ "$(cat $T/other.json)" = '{"secret":1}' ] && echo 1)" "$(cat $T/other.json)"
tb org.gnome.TextEditor
ok "13. an app not on the list: nothing written, not marked" "$([ ! -e $H/.var/app/org.gnome.TextEditor ] && [ ! -e $H/.local/state/rudolf-titlebars/org.gnome.TextEditor ] && echo 1)"
ok "14. a bad ID or home is refused" "$($T/ra titlebar ../x $H 2>/dev/null; a=$?; $T/ra titlebar org.chromium.Chromium rel 2>/dev/null; b=$?; [ "$a $b" = "2 2" ] && echo 1)"
# ---- as Rudolf starts a Flatpak app (rudolfd's apps/run: rudolf-apps launch, as the account's user)
H=$T/h6; mkdir -p $H $T/ex/applications; printf '[Desktop Entry]\nName=Chromium\nExec=/bin/true %%U\n' > $T/ex/applications/org.chromium.Chromium.desktop
HOME=$H RA_EXPORTS=$T/ex PATH=/nonexistent:$PATH $T/ra launch org.chromium.Chromium 2>>$T/log
ok "15. rudolf-apps launch: asked before the app starts (then the launcher's command runs)" "$([ "$(jget $H/.var/app/org.chromium.Chromium/config/chromium/Default/Preferences 'd["browser"]["custom_chrome_frame"]')" = false ] && grep -q 'starting org.chromium.Chromium' $T/log && echo 1)" "$(tail -3 $T/log)"
ok "16. the list's entries are well formed (ID, how, a relative path)" "$(awk -F'\t' '!/^#/ && (NF!=3 || ($2!="chromium" && $2!="vscode" && $2!="cut") || ($2=="cut" && $3 !~ /^[0-9]+$/) || $3 ~ /^\// || $3 ~ /\.\./) {bad=1} END{print bad?0:1}' $RA_TITLEBARS)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
