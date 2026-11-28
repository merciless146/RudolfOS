#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 40, the owner's Vivobook on 2026-11-23: Starport GE did not start, its folder being in the Wine
# prefix every account shared before 1.5.9): rudolfd's offer to bring the Windows programs from before into the account
# (rudolfd-wineold.inc), compiled from rudolfd.c (fixtures/wineoldapi.c), on an ext4 drive as RUDOLFDATA is, with made-up
# prefixes (no real program: a few files with Wine's layout). Checked: what is offered and to whom (one account, several:
# an administrator decides), room first, the copy owned by the account and the old prefix untouched, the account's own prefix
# kept (renamed, or the copy beside it), Wine's launchers and icons without overwriting the account's own, a copy cut off
# (the account keeps its prefix, the next one starts again), a running Windows program, links on either side never followed,
# and rudolf-debian's message of a program that ended at once (fixtures/debreport.c).
# Needs root (a loop device, a private mount namespace) and setpriv. Stops only the processes it started.   sh wineold162_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$WO62_INNER" ] && exec env WO62_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/wineold162.XXXXXX); chmod 755 $T
mount -t tmpfs none /run; mkdir -p /run/rudolf
gcc -O1 -w -o $T/t $HERE/fixtures/wineoldapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
gcc -O1 -w -DLR_DIR="\"$T/req\"" -o $T/dr $HERE/fixtures/debreport.c -lcrypto 2>$T/cc || { cat $T/cc; echo "FAIL build debreport"; exit 1; }
node $HERE/fixtures/deskls.js $T/ls.json $T/keys.json || { echo "FAIL node"; exit 1; }
truncate -s 512M $T/img; mkfs.ext4 -q -F $T/img; mkdir $T/m; mount -o loop $T/img $T/m || { echo "SKIP: no loop device"; exit 0; }
N=0; F=0; ok() { N=$((N+1)); case "$2" in *[!1]*|'') r=0;; *) r=1;; esac; if [ $r = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 900)"; F=$((F+1)); fi; }
has() { echo "$1" | grep -q -- "$2" && echo 1; }
OH=$T/m/home/rudolf; OLD=$OH/.wine; MAP=$T/m/etc/account-users; ST=$T/m/etc/wine-old; HM=$T/m/homes; H=/run/rudolf/health.log
mkdir -p $T/m/etc $T/m/accounts $HM
t() { $T/t $T/ls.json $T/m/accounts $MAP $OH $ST "$@"; }
as() { u=$1; shift; setpriv --reuid=$u --regid=1000 --clear-groups "$@" 2>&1; }
signin() { echo "$1" > /run/rudolf/home-session; printf 'DISPLAY=:0\nHOME=%s\nUSER=acct%s\nLOGNAME=acct%s\nXDG_RUNTIME_DIR=/run/user/%s\n' $HM/acct$2 $2 $2 $2 > /run/rudolf/acct-session.env; }
sum() { (cd "$1" && find . -printf '%p %y %s %m %u:%g %l\n' | sort; find . -type f -print0 | sort -z | xargs -0 -r md5sum) | md5sum | cut -c1-32; }
# ---- the desktop user's prefix from before 1.5.9 (uid 1000): Wine's layout, two made-up programs, Wine's own folders
mkdir -p "$OLD/drive_c/users/rudolf/StarportGE/data" "$OLD/drive_c/users/rudolf/Desktop" "$OLD/drive_c/Program Files/Common Files" "$OLD/drive_c/Program Files/Notepad2" "$OLD/drive_c/windows/system32" $OLD/dosdevices
head -c 3000000 /dev/urandom > "$OLD/drive_c/users/rudolf/StarportGE/Launcher.exe"; for i in $(seq 1 120); do echo "file $i" > "$OLD/drive_c/users/rudolf/StarportGE/data/f$i.dat"; done
echo 'made-up' > "$OLD/drive_c/Program Files/Notepad2/notepad2.exe"; echo dll > $OLD/drive_c/windows/system32/kernel32.dll
printf 'WINE REGISTRY Version 2\n[Software\\\\StarportGE]\n' > $OLD/system.reg; echo user > $OLD/user.reg
ln -s ../drive_c $OLD/dosdevices/c:; ln -s / $OLD/dosdevices/z:; ln -s /etc/shadow "$OLD/drive_c/users/rudolf/StarportGE/evil"
echo setuid > "$OLD/drive_c/users/rudolf/StarportGE/suid.exe"; chmod 4755 "$OLD/drive_c/users/rudolf/StarportGE/suid.exe"
# (Wine's launchers, menus, folders and icons; one of another app's, which stays)
L=$OH/.local/share/applications; mkdir -p "$L/wine/Programs/Starport GE" $OH/.config/menus/applications-merged $OH/.local/share/desktop-directories $OH/.local/share/icons/hicolor/256x256/apps $OH/.local/share/icons/hicolor/48x48/apps
printf '[Desktop Entry]\nName=Starport GE\nExec=env WINEPREFIX="/home/rudolf/.wine" wine C:\\\\\\\\users\\\\\\\\rudolf\\\\\\\\StarportGE\\\\\\\\Launcher.exe\nType=Application\nIcon=4C5D_Launcher.0\nPath=/home/rudolf/.wine/drive_c/users/rudolf/StarportGE\n' > "$L/wine/Programs/Starport GE/Starport GE.desktop"
printf '[Desktop Entry]\nName=Notepad2\nExec=env WINEPREFIX="/home/rudolf/.wine" wine notepad2.exe\nType=Application\nIcon=1A2B_notepad2.0\n' > "$L/wine/Programs/Notepad2.desktop"
printf '[Desktop Entry]\nName=Other\nExec=other\nIcon=other\n' > $L/org.other.desktop
echo '<Menu/>' > "$OH/.config/menus/applications-merged/wine-Programs-Starport GE.menu"; echo '[Desktop Entry]' > $OH/.local/share/desktop-directories/wine-Programs.directory
echo PNG256 > $OH/.local/share/icons/hicolor/256x256/apps/4C5D_Launcher.0.png; echo PNG48 > $OH/.local/share/icons/hicolor/48x48/apps/4C5D_Launcher.0.png; echo other > $OH/.local/share/icons/hicolor/48x48/apps/other.png
chown -R 1000:1000 $OH; chown -h root:root "$OLD/drive_c/users/rudolf/StarportGE/evil"
echo 'root only' > "$OLD/drive_c/users/rudolf/StarportGE/rootonly.dat"; chmod 600 "$OLD/drive_c/users/rudolf/StarportGE/rootonly.dat"   # (not the desktop user's to read)
OLDSUM=$(sum $OLD); OHSUM=$(sum $OH)
# ---- Ann's account (1001), with the fresh prefix Wine made her (Wine's own folders only), as on the Vivobook
A=$HM/acct1001; mkdir -p "$A/.wine/drive_c/users/rudolf/Desktop" "$A/.wine/drive_c/Program Files/Common Files" $A/.wine/drive_c/windows "$A/.local/share/applications/wine/Programs"
echo fresh > $A/.wine/system.reg; printf '[Desktop Entry]\nName=Notepad2 (mine)\nExec=mine\n' > "$A/.local/share/applications/wine/Programs/Notepad2.desktop"
chown -R 1001:1000 $A; chown 1001:1001 $A; chmod 700 $A; ASUM=$(sum $A/.wine)
echo 'uAnnA1b2 1001 moved' > $MAP
o=$(t get); ok "1. nobody signed in (Debian as the desktop's user, whose prefix it is): nothing offered" "$(has "$o" '"on":false')$(has "$o" '"offer":false')" "$o"
signin uAnnA1b2 1001
o=$(t get)
ok "2. Ann signed in, the only account: offered, with the programs' names (Wine's own folders and Desktop are not programs)" "$(has "$o" '"on":true')$(has "$o" '"pending":true')$(has "$o" '"several":false')$(has "$o" '"programs":\["Notepad2","StarportGE"\]\|"programs":\["StarportGE","Notepad2"\]')$(has "$o" '"count":2')" "$o"
ok "...her prefix has no programs of its own: the copy takes its place (replace), room enough, its size the whole old prefix" "$(has "$o" '"mode":"replace"')$(has "$o" '"own":false')$(has "$o" '"room":true')$(echo "$o" | grep -q '"bytes":30[0-9][0-9][0-9][0-9][0-9]' && echo 1)" "$o"
ok "...Wine's launchers she lacks are counted (Starport GE's; her own Notepad2 entry counts as there)" "$(has "$o" '"launchers":1')" "$o"
o=$(t get /home/rudolf/.wine/drive_c/users/rudolf/StarportGE); o2=$(t get /home/rudolf/.wine/drive_c/users/rudolf/Nothing); o3=$(t get /home/rudolf/.wine/../.wine/drive_c/users/rudolf/StarportGE); o4=$(t get /home/rudolf/.wine/drive_c/users/rudolf/Desktop)
ok "3. a program's message names its folder: in the old prefix only (inOld); one in neither, a path with .., one in both: not" "$(has "$o" '"inOld":true')$(has "$o2" '"inOld":false')$(has "$o3" '"inOld":false')$(has "$o4" '"inOld":false')" "$o2 $o3 $o4"
# ---- too little room: her folder on a small drive
mkdir -p $T/small; truncate -s 40M $T/simg; mkfs.ext4 -q -F $T/simg; mount --bind $A $T/small
mount -o loop $T/simg $A; chown 1001:1001 $A; chmod 700 $A
o=$(t get); o2=$(t bring)
ok "4. too little room (the copy and 256 MB to spare): said, and nothing started" "$(has "$o" '"room":false')$(has "$o2" '507')$(has "$o2" 'not enough room')$([ ! -e /run/rudolf/wine-bring.json ] && echo 1)$([ -z "$(ls -A $A | grep -v lost)" ] && echo 1)" "$o $o2 $(ls -A $A)"
umount $A; umount $T/small
# ---- links planted in her folder by her own programs: removed as links, never followed
as 1001 ln -s /etc $A/.wine.bringing; as 1001 ln -s /run $A/.wine.brought; ETCSUM=$(ls -la /etc | md5sum); MINE0=$(find /etc /run -maxdepth 2 -user 1001 2>/dev/null | sort)   # (the runner's own user is 1001: what was its before)
# ---- a cut-off copy: stopped in the middle (killed), as a power cut would
WO_SLOW=30000 t bring >$T/b1; timeout 2 tail -f /dev/null
JP=$(pgrep -f "^$T/t .* bring$" | head -1); [ -n "$JP" ] && kill -9 $JP
for i in 1 2 3 4 5 6 7 8 9 10; do pgrep -f "^$T/t .* bring$" >/dev/null || break; timeout 1 tail -f /dev/null; done
o=$(t get)
ok "5. a copy cut off: started (200), stopped in the middle; her own prefix as it was, the half copy only in .wine.bringing" "$(has "$(cat $T/b1)" '200 OK')$([ -n "$JP" ] && echo 1)$([ "$(sum $A/.wine)" = "$ASUM" ] && echo 1)$([ -d $A/.wine.bringing ] && [ ! -L $A/.wine.bringing ] && echo 1)$(has "$o" '"cut":true')$(has "$o" '"alive":false')" "$(cat $T/b1) $o $(ls -la $A)"
ok "...the links she had there were removed as links: /etc and /run untouched, nothing of hers in them" "$([ "$(ls -la /etc | md5sum)" = "$ETCSUM" ] && echo 1)$([ "$(find /etc /run -maxdepth 2 -user 1001 2>/dev/null | sort)" = "$MINE0" ] && echo 1)$([ ! -e $A/.wine.brought ] && echo 1)" "$(find /etc /run -maxdepth 2 -user 1001 2>&1 | head -3)"
# ---- a Windows program of hers running (its wineserver): the copy is made and kept, but does not take its place yet
cp /bin/sleep $T/wineserver; setpriv --reuid=1001 --regid=1000 --clear-groups $T/wineserver 120 & WS=$!; timeout 1 tail -f /dev/null
o=$(t bring); ok "6. while a Windows program of hers runs: refused first (close it)" "$(has "$o" '409')$(has "$o" 'close your Windows programs')" "$o"
kill $WS; wait $WS 2>/dev/null
o=$(t bring); w=$(t wait)
ok "7. then: copied; the copy is her .wine, her own fresh prefix kept beside it as .wine.own-DATE, the half copy gone" "$(has "$o" '200 OK')$(has "$o" '"mode":"replace"')$(has "$w" '"phase":"done"')$(cmp -s "$OLD/drive_c/users/rudolf/StarportGE/Launcher.exe" "$A/.wine/drive_c/users/rudolf/StarportGE/Launcher.exe" && echo 1)$(ls -d $A/.wine.own-* >/dev/null 2>&1 && [ "$(sum $(ls -d $A/.wine.own-* | head -1))" = "$ASUM" ] && echo 1)$([ ! -e $A/.wine.bringing ] && [ ! -e $A/.wine.brought ] && echo 1)" "$o $w $(ls -la $A)"
ok "...everything in the copy is hers (uid 1001, group 1000 as Wine's own files in Debian), nothing set-user-ID" "$([ -z "$(find $A/.wine ! -user 1001)" ] && [ -z "$(find $A/.wine ! -group 1000)" ] && [ -z "$(find $A/.wine -perm -4000)" ] && echo 1)$([ "$(stat -c %a "$A/.wine/drive_c/users/rudolf/StarportGE/suid.exe")" = 755 ] && echo 1)" "$(find $A/.wine ! -user 1001 | head -3) $(stat -c %a "$A/.wine/drive_c/users/rudolf/StarportGE/suid.exe")"
ok "...links copied as links (c:, z:, and one to /etc/shadow: its target never read); a file the desktop's user cannot read is left out" "$([ "$(readlink $A/.wine/dosdevices/c:)" = ../drive_c ] && [ "$(readlink $A/.wine/dosdevices/z:)" = / ] && [ "$(readlink "$A/.wine/drive_c/users/rudolf/StarportGE/evil")" = /etc/shadow ] && echo 1)$([ ! -e "$A/.wine/drive_c/users/rudolf/StarportGE/rootonly.dat" ] && echo 1)$(has "$w" '"skipped":1')" "$w"
ok "...the old prefix and the desktop user's home are exactly as they were" "$([ "$(sum $OLD)" = "$OLDSUM" ] && [ "$(sum $OH)" = "$OHSUM" ] && echo 1)"
ok "...Wine's launcher, menu, folder and both icons came; her own Notepad2 entry kept; another app's launcher and icon did not" "$([ -f "$A/.local/share/applications/wine/Programs/Starport GE/Starport GE.desktop" ] && [ -f "$A/.config/menus/applications-merged/wine-Programs-Starport GE.menu" ] && [ -f $A/.local/share/desktop-directories/wine-Programs.directory ] && echo 1)$([ "$(cat $A/.local/share/icons/hicolor/256x256/apps/4C5D_Launcher.0.png)" = PNG256 ] && [ -f $A/.local/share/icons/hicolor/48x48/apps/4C5D_Launcher.0.png ] && echo 1)$(grep -q 'mine' "$A/.local/share/applications/wine/Programs/Notepad2.desktop" && echo 1)$([ ! -e $A/.local/share/applications/org.other.desktop ] && [ ! -e $A/.local/share/icons/hicolor/48x48/apps/other.png ] && echo 1)$([ -z "$(find $A/.local $A/.config ! -user 1001)" ] && echo 1)" "$(find $A/.local $A/.config | head -20)"
o=$(t get)
ok "...nothing is offered any more; said in the health log" "$(has "$o" '"offer":false')$(has "$o" '"pending":false')$(grep -q 'were brought into the account uAnnA1b2 (replace' $H && echo 1)$(grep -q '^uAnnA1b2 brought replace' $ST && echo 1)" "$o $(cat $H)"
# ---- the launchers only (the prefix copied by hand, as on the Vivobook, without Wine's launchers)
rm -rf "$A/.local/share/applications/wine/Programs/Starport GE"; o=$(t get)
ok "8. the programs in the account, Wine's launchers not: offered as the launchers only (no copy, no room needed)" "$(has "$o" '"mode":"launchers"')$(has "$o" '"count":0')$(has "$o" '"launchers":1')$(has "$o" '"pending":true')" "$o"
ASUM2=$(sum $A/.wine); o=$(t bring); w=$(t wait)
ok "...brought: the launcher back, the prefix not touched" "$(has "$w" '"phase":"done"')$([ -f "$A/.local/share/applications/wine/Programs/Starport GE/Starport GE.desktop" ] && echo 1)$([ "$(sum $A/.wine)" = "$ASUM2" ] && echo 1)" "$o $w"
# ---- declined
rm -rf "$A/.local/share/applications/wine/Programs/Starport GE"; o=$(t decline); o2=$(t get)
ok "9. Don't ask again: not pending (no notice, not in System health), still offered (Settings › Apps › Linux)" "$(has "$o" '200')$(has "$o2" '"declined":true')$(has "$o2" '"pending":false')$(has "$o2" '"offer":true')" "$o2"
# ---- several accounts: Bob (1002), whose prefix has a program of his own
echo 'uBobC3d4 1002 moved' >> $MAP; B=$HM/acct1002; mkdir -p "$B/.wine/drive_c/Program Files/BobsTool" $B/.wine/drive_c/windows; echo bob > "$B/.wine/drive_c/Program Files/BobsTool/tool.exe"
chown -R 1002:1000 $B; chown 1002:1002 $B; chmod 700 $B; BSUM=$(sum $B/.wine); signin uBobC3d4 1002
o=$(t get)
ok "10. two accounts: an administrator decides; the programs' names are not given to Bob, only how many" "$(has "$o" '"several":true')$(has "$o" '"accounts":2')$(has "$o" '"programs":\[\]')$(has "$o" '"count":2')" "$o"
ok "...his prefix has a program of its own: the copy goes beside it" "$(has "$o" '"own":true')$(has "$o" '"mode":"beside"')" "$o"
o1=$(t bring); o2=$(t bring Bob bob-pass); o3=$(t bring Ann wrong-one)
ok "...without an administrator's password, with his own (not an administrator), with a wrong one: refused, nothing started" "$(has "$o1" '403')$(has "$o2" '403')$(has "$o2" 'only an administrator')$(has "$o3" '403')$(has "$o3" 'Wrong password')$([ ! -e $B/.wine-shared ] && [ ! -e $B/.wine.bringing ] && echo 1)" "$o1 $o2 $o3"
o=$(t bring Ann 'pw-ann ü'); w=$(t wait)
ok "...with Ann's (an administrator): brought beside his own (.wine-shared), his .wine untouched, said who allowed it" "$(has "$o" '200 OK')$(has "$w" '"phase":"done"')$(has "$w" '"mode":"beside"')$([ "$(sum $B/.wine)" = "$BSUM" ] && echo 1)$(cmp -s "$OLD/drive_c/users/rudolf/StarportGE/Launcher.exe" "$B/.wine-shared/drive_c/users/rudolf/StarportGE/Launcher.exe" && echo 1)$(grep -q 'uBobC3d4 (beside, allowed by the administrator Ann)' $H && echo 1)" "$o $w"
ok "...the launchers brought with it use it (WINEPREFIX .wine-shared)" "$(grep -q 'WINEPREFIX="/home/rudolf/.wine-shared" wine' "$B/.local/share/applications/wine/Programs/Starport GE/Starport GE.desktop" && echo 1)$(grep -q 'WINEPREFIX="/home/rudolf/.wine"' "$B/.local/share/applications/wine/Programs/Starport GE/Starport GE.desktop" || echo 1)" "$(cat "$B/.local/share/applications/wine/Programs/Starport GE/Starport GE.desktop" 2>&1)"
ok "...one account's copy is not readable by the other's user" "$(as 1001 ls $B 2>&1 | grep -q 'Permission denied' && echo 1)$(as 1002 ls $A 2>&1 | grep -q 'Permission denied' && echo 1)" ""
ok "...the old prefix still exactly as it was" "$([ "$(sum $OLD)" = "$OLDSUM" ] && echo 1)"
# ---- the portal: a program's message (rudolf-debian's lxfail request) is taken away when answered, with no answer left behind
mkdir -p /run/rudolf/portal/req /run/rudolf/portal/resp; echo '{"kind":"lxfail"}' > /run/rudolf/portal/req/lxfail-1-2.json
o=$(t answer lxfail-1-2); ok "11. a program's message, answered: taken away, no answer file left for nobody" "$(has "$o" '200')$([ ! -e /run/rudolf/portal/req/lxfail-1-2.json ] && [ ! -e /run/rudolf/portal/resp/lxfail-1-2.json ] && echo 1)" "$o"
rm -f /run/rudolf/acct-session.env; o=$(t get); ok "12. signed out: nothing offered (on: false)" "$(has "$o" '"on":false')" "$o"
# ---- rudolf-debian: a program that ended at once (fixtures/debreport.c: launch_report as cmd_launch calls it)
mkdir -p $T/req; printf "fixme:x\n/usr/local/bin/starport-ge: 1: cd: can't cd to /home/rudolf/.wine/drive_c/users/rudolf/StarportGE\n0024:fixme:ntdll:y\n" > $T/sp.log
$T/dr starport-ge 'Starport GE' '/usr/local/bin/starport-ge' '' 2 0.3 $T/sp.log; f=$(ls $T/req/*.json 2>/dev/null | head -1)
j=$(node -e "const j=JSON.parse(require('fs').readFileSync(process.argv[1],'utf8'));console.log(JSON.stringify(j))" "$f" 2>&1)
ok "13. rudolf-debian: Starport GE's launcher ending at once (exit 2): a portal request lxfail-..., valid JSON: its name, how it ended, its log's last line (Wine's fixme: left out), a Windows program, its folder" "$(echo "$f" | grep -q '/lxfail-[0-9]*-[0-9]*\.json$' && echo 1)$(has "$j" '"kind":"lxfail"')$(has "$j" '"name":"Starport GE"')$(has "$j" '"status":2')$(has "$j" "\"lines\":\[\"/usr/local/bin/starport-ge: 1: cd: can't cd to /home/rudolf/.wine/drive_c/users/rudolf/StarportGE\"\]")$(has "$j" '"wine":true')$(has "$j" '"dir":"/home/rudolf/.wine/drive_c/users/rudolf/StarportGE"')" "$f $j"
rm -f $T/req/*; printf '0024:fixme:ntdll:wine_init\n00b0:err:d3d:wined3d_init No available renderer\n00b8:err:ntoskrnl:ZwLoadDriver failed to create driver L"winebth": c00000e5\n0034:fixme:service:scmdatabase_autostart_services Auto-start service L"winebth" failed to start: 1359\n0010:err:wineusb:add_device no device\n0070:err:d3d:swapchain_create Failed to create swap chain\n' > $T/d3d.log
$T/dr sf 'Starport GE' 'wine' '' 3 0.4 $T/d3d.log; f=$(ls $T/req/*.json | head -1)
j=$(node -e "const j=JSON.parse(require('fs').readFileSync(process.argv[1],'utf8'));console.log(j.lines.join('|'))" "$f" 2>&1)
ok "...a Windows game that ended at once: the first real error first, then the last line (Wine's driver-service setup and fixme: left out)" "$([ "$j" = '00b0:err:d3d:wined3d_init No available renderer|0070:err:d3d:swapchain_create Failed to create swap chain' ] && echo 1)" "$j"
rm -f $T/req/*; printf '00b8:err:ntoskrnl:ZwLoadDriver failed to create driver L"winebth": c00000e5\n0034:fixme:service:scmdatabase_autostart_services Auto-start service L"winebth" failed to start: 1359\n' > $T/noise.log
$T/dr nz 'Dead Frontier' 'wine' '' 3 0.2 $T/noise.log; f=$(ls $T/req/*.json | head -1)
j=$(node -e "const j=JSON.parse(require('fs').readFileSync(process.argv[1],'utf8'));console.log(j.lines.join('|'))" "$f" 2>&1)
ok "...a log of nothing but that setup: one line saying so, neither the setup nor \"its log is empty\"" "$(echo "$j" | grep -q 'harms nothing' && echo 1)$(echo "$j" | grep -q 'winebth' || echo 1)$(echo "$j" | grep -q '|' || echo 1)" "$j"
rm -f $T/req/*; printf 'line "one"\tq\\x\n' > $T/q.log
$T/dr x 'A "quoted" app' 'prog' '' 0 0.5 $T/q.log; n0=$(ls $T/req | wc -l); $T/dr x 'Slow' 'prog' '' 1 30 $T/q.log; n1=$(ls $T/req | wc -l)
$T/dr x 'A "quoted" app' 'prog' '' 137 0.2 $T/q.log; f=$(ls $T/req/*.json | head -1); j=$(node -e "const j=JSON.parse(require('fs').readFileSync(process.argv[1],'utf8'));console.log(j.name+'|'+j.lines[0]+'|'+j.status+'|'+j.wine)" "$f" 2>&1)
ok "...one that ended well (0) or after a while (30 s): nothing; one stopped by a signal at once: said, quotes and tabs kept as JSON" "$([ $n0 = 0 ] && [ $n1 = 0 ] && echo 1)$([ "$j" = "$(printf 'A "quoted" app|line "one"\tq\\x|137|false')" ] && echo 1)" "$n0 $n1 $j"
ok "...cmd_launch times the program and reports it after it ends" "$(grep -q 'launch_report(id, &k, WIFEXITED(st)' $HERE/../../sys/rudolf-debian.c && echo 1)"
umount $T/m; rm -rf $T; echo "$((N-F))/$N passed"; [ $F = 0 ]
