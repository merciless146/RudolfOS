#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 13): rudolfd's desktop/dropped, which reads a file dropped from a Linux app onto the native
# desktop for the page (rudolfd-drop.inc), compiled from rudolfd.c (fixtures/dropapi.c): the signed-in account's own files under
# each name a Linux app gives them (its Linux user's home, its folder, Debian's /home/rudolf), a drive's, Flatpak's document
# portal mapped to the real file; refused: another account's files, a file the account's user may not read, links (also on the
# way), anything outside, folders and devices, over 64 MB, nobody signed in, a path too long to be whole. The file is never
# changed. Needs root (users by uid, a private mount namespace).   sh drop161_t.sh
[ "$(id -u)" = 0 ] || { echo "SKIP: needs root"; exit 0; }
[ -z "$DROP_INNER" ] && exec env DROP_INNER=1 unshare -m sh "$0" "$@"
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/drop161.XXXXXX); trap 'umount -l $T/home/acct1001 2>/dev/null; rm -rf $T' EXIT
chmod 755 $T; mount --make-rprivate / 2>/dev/null; mount -t tmpfs none /run; mkdir -p /run/rudolf
gcc -O1 -w -o $T/t $HERE/fixtures/dropapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
R=$T/accounts; A=$R/uAnnA1b2; B=$R/uBobC3d4; M=$T/media; RU=$T/runuser; HM=$T/home
mkdir -p $A/Desktop $A/Documents/sub $B/Documents $M/USB1 $RU/1001/doc/d1 $RU/1001/doc/d2 $RU/1001/doc/d3 $HM/acct1001 $HM/acct1002 $T/flat
printf 'uAnnA1b2 1001\nuBobC3d4 1002\n' > $T/map
echo "Ann's note" > $A/Documents/note.txt; echo 'on the desktop' > $A/Desktop/here.txt; echo 'deep' > $A/Documents/sub/deep.txt
echo "Bob's secret" > $B/Documents/secret.txt
echo 'not yours' > $A/Documents/theirs.txt; chown 1002:1002 $A/Documents/theirs.txt; chmod 600 $A/Documents/theirs.txt
ln -s $B/Documents/secret.txt $A/Documents/link.txt; ln -s $B/Documents $A/Documents/linkdir; ln -s /etc $A/etc
mkfifo $A/Documents/pipe; truncate -s 65M $A/Documents/big.bin; truncate -s 64M $A/Documents/max.bin
chown -R 1001:1001 $A; chown 1002:1002 $A/Documents/theirs.txt; chmod 700 $A; chown -R 1002:1002 $B; chmod 700 $B
echo photo > $M/USB1/photo.jpg; ln -s /etc/shadow $M/USB1/shadow
# the document portal: what it shows (its own copy here, to tell them apart) and the real file it names (its xattr, as xdg-document-portal's)
echo 'the portal copy' > $RU/1001/doc/d1/note.txt; echo x > $RU/1001/doc/d2/shadow; echo x > $RU/1001/doc/d3/plain.txt
python3 -c "import os,sys;os.setxattr(sys.argv[1],'user.document-portal.host-path',sys.argv[2].encode());os.setxattr(sys.argv[3],'user.document-portal.host-path',b'/etc/shadow')" \
	$RU/1001/doc/d1/note.txt $HM/acct1001/Documents/note.txt $RU/1001/doc/d2/shadow 2>$T/xa || { echo "SKIP: no user xattrs here ($(cat $T/xa))"; exit 0; }
chown -R 1001:1001 $RU/1001; chmod 700 $RU/1001
mount --bind $A $HM/acct1001   # (the account's folder as its Linux user's home, as au_session_start binds it)
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
t() { who=$1; shift; $T/t $R $T/map $HM $RU $M $T/flat $who "$@"; }
is() { case "$1" in "$2"*) echo 1;; *) echo 0;; esac; }
before=$(stat -c '%Y %s %u' $A/Documents/note.txt)
o=$(t uAnnA1b2 $HM/acct1001/Desktop/here.txt); ok "1. the account's own file, from its Linux user's home (a Flatpak app's path): the file" "$(is "$o" 'HTTP/1.1 200 OK|15|on the desktop')" "$o"
o=$(t uAnnA1b2 $A/Documents/note.txt); ok "2. ...from the account's folder itself" "$(is "$o" "HTTP/1.1 200 OK|11|Ann's note")" "$o"
o=$(t uAnnA1b2 /home/rudolf/Documents/sub/deep.txt); ok "3. ...from Debian's view of it (/home/rudolf, rudolf-debian's bind): the account's file" "$(is "$o" 'HTTP/1.1 200 OK|5|deep')" "$o"
o=$(t uAnnA1b2 '/home/acct1001//Documents/./note.txt'); ok "4. a path with . is refused, not tidied" "$(is "$o" 'HTTP/1.1 400')" "$o"
o=$(t uAnnA1b2 /home/acct1001/Documents/../../acct1002/x $HM/acct1001/../../etc/passwd); ok "5. ...and with .." "$([ "$(echo "$o" | grep -c '400 .*path')" = 2 ] && echo 1)" "$o"
o=$(t uAnnA1b2 $B/Documents/secret.txt /home/acct1002/Documents/secret.txt); ok "6. another account's file: refused (outside), never read" "$([ "$(echo "$o" | grep -c '^HTTP/1.1 403 .*"outside"')" = 2 ] && ! echo "$o" | grep -q secret && echo 1)" "$o"
o=$(t uAnnA1b2 $A/Documents/theirs.txt); ok "7. a file in the account's folder its Linux user may not read: refused" "$(echo "$o" | grep -q '403 .*"unreadable"' && ! echo "$o" | grep -q 'not yours' && echo 1)" "$o"
o=$(t uAnnA1b2 $A/Documents/link.txt $A/Documents/linkdir/secret.txt $A/etc/passwd); ok "8. a link out of the home (the file, a folder on the way): refused" "$([ "$(echo "$o" | grep -c '403 .*"link"')" = 3 ] && ! echo "$o" | grep -q "secret\|root:" && echo 1)" "$o"
o=$(t uAnnA1b2 /etc/passwd /run/rudolf/x /persist/debian/etc/passwd /home/rudolf/../acct1002); ok "9. anything else: refused" "$([ "$(echo "$o" | grep -c '403 .*"outside"')" = 3 ] && echo "$o" | tail -1 | grep -q '400' && echo 1)" "$o"
o=$(t uAnnA1b2 $A/Documents/sub $A/Documents/pipe $HM/acct1001); ok "10. a folder, a pipe (not waited on), the home itself: refused (folder)" "$([ "$(echo "$o" | grep -c '403 .*"folder"')" = 3 ] && echo 1)" "$o"
o=$(t uAnnA1b2 $A/Documents/big.bin $A/Documents/max.bin); ok "11. over 64 MB: refused (too-large, the page's message); 64 MB itself: read" "$(echo "$o" | head -1 | grep -q '^HTTP/1.1 413 .*too-large' && echo "$o" | tail -1 | grep -q '^HTTP/1.1 200 OK|67108864|' && echo 1)" "$(echo "$o" | cut -c1-120)"
o=$(t uAnnA1b2 $M/USB1/photo.jpg $M/USB1/shadow $M); ok "12. a drive's file: read; a link on the drive and the drives' folder itself: refused" "$(echo "$o" | sed -n 1p | grep -q '200 OK|6|photo' && echo "$o" | sed -n 2p | grep -q '"link"' && echo "$o" | sed -n 3p | grep -q '"outside"' && echo 1)" "$o"
o=$(t uAnnA1b2 $RU/1001/doc/d1/note.txt); ok "13. Flatpak's document portal: the real file it names, read (not the portal's copy)" "$(is "$o" "HTTP/1.1 200 OK|11|Ann's note")" "$o"
o=$(t uAnnA1b2 $T/flat/d1/note.txt); ok "14. ...also under the sandbox's own name for the portal (/run/flatpak/doc)" "$(is "$o" "HTTP/1.1 200 OK|11|Ann's note")" "$o"
o=$(t uAnnA1b2 $RU/1001/doc/d2/shadow $RU/1001/doc/d3/plain.txt $RU/1002/doc/d1/x); ok "15. ...a portal file naming a file outside, one that names none, another user's portal: refused" "$([ "$(echo "$o" | grep -c '403 .*"outside"')" = 3 ] && echo 1)" "$o"
o=$(t - $A/Documents/note.txt; t guest $A/Documents/note.txt); ok "16. nobody signed in, or the guest: refused" "$([ "$(echo "$o" | grep -c '409 .*nobody')" = 2 ] && echo 1)" "$o"
o=$(t uBobC3d4 $A/Documents/note.txt $B/Documents/secret.txt); ok "17. Bob signed in: Ann's file refused, his own read" "$(echo "$o" | sed -n 1p | grep -q '403 .*outside' && echo "$o" | sed -n 2p | grep -q "200 OK|13|Bob's secret" && echo 1)" "$o"
LONG=$HM/acct1001/$(printf 'ü%.0s' $(seq 400)); o=$(t uAnnA1b2 "$LONG"); ok "18. a path longer than the request's room (it would be cut short): refused" "$(echo "$o" | grep -q '400 .*path' && echo 1)" "$o"
ok "19. the files are never changed or moved (a copy is the page's)" "$([ "$(stat -c '%Y %s %u' $A/Documents/note.txt)" = "$before" ] && [ -e $RU/1001/doc/d1/note.txt ] && [ "$(cat $A/Documents/note.txt)" = "Ann's note" ] && echo 1)"
o=$(grep -c 'sh -c\|system(\|popen(\|run(' $HERE/../../sys/rudolfd-drop.inc); ok "20. no shell on the way (rudolfd-drop.inc runs no command)" "$([ "$o" = 0 ] && echo 1)" "$o"
echo "$((N-F))/$N passed"; [ $F = 0 ]
