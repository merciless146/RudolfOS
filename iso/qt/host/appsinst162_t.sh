#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 14, the owner's Vivobook on 2026-11-22): "rudolf-apps installed" keeps its list
# (FLATPAK/rudolf-installed.json) and gives it again without running `flatpak list` while nothing it was made from changed:
# each installed app's app/ID/current and its active (installing, updating, uninstalling, a new branch) and the exported
# launchers' folders. A stand-in flatpak (PATH) lists 40 apps and takes RA_T_DELAY seconds (default 1.5: a slow computer
# while Steam starts); a fake Flatpak folder (RA_FLATPAK_DIR, RA_EXPORTS). Prints the times.   sh appsinst162_t.sh
D=$(cd "$(dirname "$0")/../../sys" && pwd); T=$(mktemp -d /tmp/appsinst162.XXXXXX); trap 'rm -rf $T' EXIT; cd "$T" || exit 1
gcc -O2 -w -o ra "$D/rudolf-apps.c" -lz || { echo "FAIL build"; exit 1; }
P=0; N=0; ok() { N=$((N + 1)); if [ "$1" = 0 ]; then P=$((P + 1)); echo "PASS $2"; else echo "FAIL $2 - $3"; fi; }
FP=$T/fp; EX=$FP/exports/share; mkdir -p $FP/app $EX/applications bin
DELAY=${RA_T_DELAY:-1.5}
# the stand-in flatpak: the apps in FP/app (each one's version from its active link), after DELAY seconds; counts its runs
cat > bin/flatpak <<X
#!/bin/sh
echo run >> $T/runs
python3 -c "import time; time.sleep($DELAY)"
for d in $FP/app/*; do [ -d "\$d" ] || continue; id=\${d##*/}; v=\$(readlink "\$d/current/active"); printf '%s\t%s\t%s\t1.0 MB\tflathub\n' "\$id" "Name \$id" "\$v"; done
X
chmod +x bin/flatpak; export PATH=$T/bin:$PATH RA_FLATPAK_DIR=$FP RA_EXPORTS=$EX
mkapp() { mkdir -p $FP/app/$1/x86_64/stable/$2; ln -sfn x86_64/stable $FP/app/$1/current; ln -sfn $2 $FP/app/$1/x86_64/stable/active
  printf '[Desktop Entry]\nName=%s\nIcon=%s\nExec=x\n' "$1" "$1" > $EX/applications/$1.desktop; }
mkapp com.valvesoftware.Steam v1; mkapp com.discordapp.Discord v1
i=0; while [ $i -lt 38 ]; do mkapp org.example.App$i v1; i=$((i + 1)); done
ms() { python3 -c "import time; print(int(time.time()*1000))"; }
runs() { [ -f runs ] && wc -l < runs || echo 0; }
t0=$(ms); o1=$(./ra installed); t1=$(ms); n1=$(runs)
echo "$o1" | python3 -c "import json,sys; d=json.load(sys.stdin); assert len(d['apps'])==40 and d['apps'][0]['launchers']" 2>/dev/null; ok $? "1. the first list: 40 apps with their launchers ($((t1 - t0)) ms, flatpak run once)" "$o1"
t0=$(ms); o2=$(./ra installed); t2=$(ms); n2=$(runs)
[ "$o2" = "$o1" ] && [ "$n2" = "$n1" ]; ok $? "2. asked again, nothing changed: the same list without running flatpak ($((t2 - t0)) ms)" "runs $n1 $n2"
[ $((t2 - t0)) -lt 300 ]; ok $? "3. ...at once (under 300 ms)" "$((t2 - t0))"
ln -sfn v2 $FP/app/com.valvesoftware.Steam/x86_64/stable/active; mkdir -p $FP/app/com.valvesoftware.Steam/x86_64/stable/v2
o3=$(./ra installed); echo "$o3" | grep -q '"id":"com.valvesoftware.Steam","name":"Name com.valvesoftware.Steam","version":"v2"'; ok $? "4. an app updated (its active link replaced): listed again, the new version" "$o3"
mkapp org.example.New v1; o4=$(./ra installed); echo "$o4" | grep -q '"id":"org.example.New"'; ok $? "5. an app installed: in the list" "$(echo "$o4" | head -c 300)"
rm -rf $FP/app/org.example.New $EX/applications/org.example.New.desktop; o5=$(./ra installed); echo "$o5" | grep -q '"id":"org.example.New"'; [ $? = 1 ]; ok $? "6. ...uninstalled: gone again"
printf '[Desktop Entry]\nName=Steam (Big Picture)\nIcon=x\nExec=x\n' > $EX/applications/com.valvesoftware.Steam.big.desktop
o6=$(./ra installed); echo "$o6" | grep -q 'Steam (Big Picture)'; ok $? "7. a launcher added by an app: listed" "$(echo "$o6" | head -c 300)"
chmod +x bin/flatpak; printf '#!/bin/sh\nexit 1\n' > bin/flatpak; rm -f $FP/rudolf-installed.json; mkapp org.example.Other v1
o7=$(./ra installed); [ ! -e $FP/rudolf-installed.json ]; ok $? "8. flatpak failing: its (empty) list not kept" "$o7"
printf 'x\n' > $FP/rudolf-installed.json; o8=$(./ra installed); echo "$o8" | grep -q '^{"ok":true'; ok $? "9. a damaged kept list: not given" "$o8"
echo "$P/$N passed"; [ $P = $N ]
