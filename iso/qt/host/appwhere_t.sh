#!/bin/sh
# 1.5.6: appwhere, where an app is kept and how much it takes on the disk (an app's Properties): a Flathub app, a
# Windows program in Wine (its starter script's folder and .exe), a Debian package's program (dpkg's Installed-Size).
# Stand-in folders.   sh appwhere_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); A=$HERE/../../sys/root/usr/lib/rudolf/appwhere
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
mkdir -p $T/fp/com.x.App/current/active/files $T/deb/usr/share/applications $T/deb/usr/local/bin $T/deb/usr/bin $T/deb/var/lib/dpkg/info "$T/home/.wine/drive_c/users/rudolf/Starport GE"
head -c 300000 /dev/urandom > $T/fp/com.x.App/current/active/files/big; head -c 100000 /dev/urandom > "$T/home/.wine/drive_c/users/rudolf/Starport GE/Launcher.exe"
printf '[Desktop Entry]\nName=Starport GE\nExec=starport-ge\n' > $T/deb/usr/share/applications/starport-ge.desktop
printf '#!/bin/sh\ncd "$HOME/.wine/drive_c/users/rudolf/Starport GE" && exec wine Launcher.exe "$@"\n' > $T/deb/usr/local/bin/starport-ge
printf '[Desktop Entry]\nName=Python\nExec=python3.13 %%f\n' > $T/deb/usr/share/applications/python3.13.desktop; echo x > $T/deb/usr/bin/python3.13
printf '/.\n/usr/bin/python3.13\n' > $T/deb/var/lib/dpkg/info/python3.13-minimal:amd64.list
printf 'Package: libc6\nInstalled-Size: 99\n\nPackage: python3.13-minimal\nStatus: install ok installed\nInstalled-Size: 5000\n\n' > $T/deb/var/lib/dpkg/status
export APPWHERE_FLATPAK=$T/fp APPWHERE_DEBIAN=$T/deb APPWHERE_HOME=$T/home
o=$(sh $A flatpak com.x.App); ok "1. a Flathub app: its files' folder, the size of all its versions" "$([ "$(has "$o" "\"path\":\"$T/fp/com.x.App/current/active/files\"")" = 1 ] && [ "$(echo "$o" | sed 's/.*"bytes":\([0-9]*\).*/\1/')" -ge 300000 ] && echo 1)" "$o"
o=$(sh $A debian starport-ge); ok "2. a Windows program in Wine: the folder its starter goes to, and its .exe" "$([ "$(has "$o" "Starport GE\",\"bytes\"")" = 1 ] && [ "$(has "$o" '"kind":"exe","program":"Launcher.exe"')" = 1 ] && echo 1)" "$o"
o=$(sh $A debian python3.13); ok "3. a Debian package's program: its package and dpkg's size" "$([ "$(has "$o" '"bytes":5120000')" = 1 ] && [ "$(has "$o" '"package":"python3.13-minimal.deb"')" = 1 ] && echo 1)" "$o"
o=$(sh $A debian nothere); ok "4. no such program: says so" "$(has "$o" '"ok":false')" "$o"
o=$(sh $A flatpak com.none); ok "5. an app not installed: says so" "$(has "$o" '"ok":false')" "$o"
c=$HERE/../../sys/rudolfd.c; grep -q '"/var/lib/flatpak/app/", "/home/rudolf/.wine/drive_c/"' $c && grep -q 'ro && app_view_path(p)' $c
ok "6. rudolfd lets File Explorer look into the apps' folders, read only (list, read, search)" "$([ $? = 0 ] && echo 1)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
