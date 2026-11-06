#!/bin/sh
# 1.5.2: Emu's "Add games" (/usr/lib/rudolf/games-import.sh) takes Game Boy, Color and Advance games too, and reads
# whole names from ZIP files (a ZIP with spaces in the game's name was skipped as "no SNES or DS game inside").
# Runs with Info-ZIP's unzip and with BusyBox's. Made-up files only, no games. sh gamesimport_t.sh
SH=${SH:-sh}; HERE=$(cd "$(dirname "$0")" && pwd); P=$HERE/../../sys/root/usr/lib/rudolf/games-import.sh
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
S=$T/src; mkdir -p $S/z $T/bb
head -c 4096 /dev/urandom > "$S/z/MADE UP - SPECIAL EDITION.gbc"; head -c 2048 /dev/urandom > $S/a.gba; head -c 1024 /dev/urandom > $S/b.SFC
head -c 512 /dev/urandom > $S/notes.txt; head -c 3000 /dev/urandom > $S/c.gb
(cd $S/z && zip -q "$S/MADE UP_ YELLOW - SPECIAL EDITION.ZIP" "MADE UP - SPECIAL EDITION.gbc" && zip -q $S/text.zip ../notes.txt 2>/dev/null || zip -q -j $S/text.zip $S/notes.txt)
ln -s "$(command -v busybox)" $T/bb/unzip
for U in infozip busybox; do
	export GAMES_DIR=$T/g-$U GAMES_TMP=$T/tmp-$U GAMES_TEST_SRC=$S; [ $U = busybox ] && export GAMES_TEST_PATH=$T/bb || unset GAMES_TEST_PATH
	printf '%s\n' "$S/MADE UP_ YELLOW - SPECIAL EDITION.ZIP" $S/a.gba $S/b.SFC $S/c.gb $S/notes.txt $S/text.zip /etc/passwd > $T/list
	o=$($SH $P $T/list 2>&1)
	ok "$U: the Game Boy Color game inside a ZIP with spaces in its names is added" $([ -f "$GAMES_DIR/GB/MADE UP - SPECIAL EDITION.gbc" ] && echo 1 || echo 0) "$o"
	ok "$U: ...as the same file" $(cmp -s "$GAMES_DIR/GB/MADE UP - SPECIAL EDITION.gbc" "$S/z/MADE UP - SPECIAL EDITION.gbc" && echo 1 || echo 0)
	ok "$U: a Game Boy Advance game goes to GB" $([ -f $GAMES_DIR/GB/a.gba ] && echo 1 || echo 0) "$o"
	ok "$U: a Game Boy game goes to GB" $([ -f $GAMES_DIR/GB/c.gb ] && echo 1 || echo 0) "$o"
	ok "$U: a SNES game still goes to SNES" $([ -f $GAMES_DIR/SNES/b.SFC ] && echo 1 || echo 0) "$o"
	ok "$U: a text file is skipped" $(has "$o" "skip notes.txt: not a Game Boy, SNES or DS game") "$o"
	ok "$U: a ZIP without a game is skipped" $(has "$o" "skip text.zip: no Game Boy, SNES or DS game inside") "$o"
	ok "$U: a file outside the allowed places is refused" $(has "$o" "skip passwd: not an allowed place") "$o"
	ok "$U: the count" $(has "$o" "done 4 added, 2 skipped") "$o"
	ok "$U: the unpacking folder is cleaned up" $([ ! -e $GAMES_TMP ] && echo 1 || echo 0)
	printf '%s\n' "$S/MADE UP_ YELLOW - SPECIAL EDITION.ZIP" > $T/list; o=$($SH $P $T/list 2>&1)
	ok "$U: adding it again skips it" $(has "$o" "already in the library") "$o"
done
echo "$((N-F))/$N passed"; [ $F = 0 ]
