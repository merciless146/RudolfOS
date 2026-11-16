#!/bin/sh
# The stand-in Flatpak for the portal's tests (iso/qt/sb/p160_t.py): org.rudolf.PortalStand, the static iso/qt/portalstand.c
# ("portalstand app": does what its command file says, then waits, as a window-less app left running) in a runtime of its own
# with nothing in it (org.rudolf.StandPlatform: the program needs no library), its launcher "PortalStand", and the session bus
# (as Sober and most apps have it). No Roblox, no account, nothing downloaded. Writes OUT/repo (a local repository, no
# signature: installed with --no-gpg-verify) and OUT/portalstand. Needs gcc (static), flatpak (build-export) and
# desktop-file-validate.   sh mkpstand.sh OUT
set -e
I=$(cd "$(dirname "$0")" && pwd); OUT=$(realpath -m "$1"); W=$(mktemp -d /tmp/mkpstand.XXXXXX)
trap 'rm -rf "$W"' EXIT
mkdir -p "$OUT"
gcc -O2 -static -o "$OUT/portalstand" "$I/portalstand.c"
RT=$W/rt; mkdir -p $RT/files/bin $RT/files/etc
printf '[Runtime]\nname=org.rudolf.StandPlatform\nruntime=org.rudolf.StandPlatform/x86_64/1\nsdk=org.rudolf.StandPlatform/x86_64/1\n' > $RT/metadata
A=$W/app; mkdir -p $A/files/bin $A/files/share/applications
cp "$OUT/portalstand" $A/files/bin/portalstand
printf '[Application]\nname=org.rudolf.PortalStand\nruntime=org.rudolf.StandPlatform/x86_64/1\nsdk=org.rudolf.StandPlatform/x86_64/1\ncommand=portalstand\n\n[Context]\nsockets=session-bus;\n' > $A/metadata
printf '[Desktop Entry]\nType=Application\nName=PortalStand\nComment=Asks the desktop for a keyring, a file and a link (a test)\nExec=portalstand app\nCategories=Utility;\n' > $A/files/share/applications/org.rudolf.PortalStand.desktop
F="env HOME=$W FLATPAK_USER_DIR=$W/u"
$F flatpak build-export --runtime --files=files $W/repo $RT 1 >/dev/null
$F flatpak build-finish $A >/dev/null
$F flatpak build-export $W/repo $A stable >/dev/null
$F flatpak build-update-repo $W/repo >/dev/null
rm -rf "$OUT/repo"; cp -a $W/repo "$OUT/repo"
du -sh "$OUT/repo"
