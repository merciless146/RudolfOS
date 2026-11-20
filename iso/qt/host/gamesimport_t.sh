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
# (1.6.1) a Nintendo 64 game, a Dreamcast disc in one file, and one as a .gdi with its tracks (one with a space in its
# name), loose and in a ZIP; and a .gdi whose track is missing
head -c 4096 /dev/urandom > $S/n.z64; head -c 5000 /dev/urandom > $S/d.cdi; mkdir -p "$S/dc" "$S/dz"
printf '3\r\n1 0 4 2352 "track01.bin" 0\r\n2 756 0 2352 track02.raw 0\r\n3 45000 4 2352 "track 03.bin" 0\r\n' > "$S/dc/Made Up Disc.gdi"
head -c 1000 /dev/urandom > $S/dc/track01.bin; head -c 2000 /dev/urandom > $S/dc/track02.raw; head -c 3000 /dev/urandom > "$S/dc/track 03.bin"
printf '1\n1 0 4 2352 zt.bin 0\n' > $S/dz/Zipped.gdi; head -c 1500 /dev/urandom > $S/dz/zt.bin; (cd $S/dz && zip -q $S/zipped-dc.zip Zipped.gdi zt.bin)
printf '1\n1 0 4 2352 gone.bin 0\n' > $S/broken.gdi
(cd $S/z && zip -q "$S/MADE UP_ YELLOW - SPECIAL EDITION.ZIP" "MADE UP - SPECIAL EDITION.gbc" && zip -q $S/text.zip ../notes.txt 2>/dev/null || zip -q -j $S/text.zip $S/notes.txt)
# (1.6.1) Genesis games (.md, .gen, a .bin with SEGA in its header), a disc's .bin alone, PlayStation discs as a .cue with
# its tracks (loose and in a ZIP), CHD discs of both kinds (version 5: the metadata's place at 0x30), a .pbp, a BIOS file
mkdir -p $S/ps $S/pz; head -c 3000 /dev/urandom > $S/g.md; head -c 3000 /dev/urandom > $S/h.GEN
{ head -c 256 /dev/urandom; printf 'SEGA MEGA DRIVE '; head -c 2000 /dev/urandom; } > $S/sega.bin; { head -c 256 /dev/zero; printf 'NOPE'; head -c 2000 /dev/urandom; } > $S/track.bin
printf 'FILE "Made Up PS (Track 1).bin" BINARY\r\n  TRACK 01 MODE2/2352\r\n    INDEX 01 00:00:00\r\nFILE t2.bin BINARY\r\n  TRACK 02 AUDIO\r\n    INDEX 01 00:00:00\r\n' > "$S/ps/Made Up PS.cue"
head -c 4000 /dev/urandom > "$S/ps/Made Up PS (Track 1).bin"; head -c 1000 /dev/urandom > $S/ps/t2.bin
printf 'FILE "pz.bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n' > "$S/pz/Zipped PS.cue"
head -c 2500 /dev/urandom > $S/pz/pz.bin; (cd $S/pz && zip -q $S/zipped-ps.zip "Zipped PS.cue" pz.bin)
chd() { { printf 'MComprHD\000\000\000\174\000\000\000\005'; head -c 32 /dev/zero; printf '\000\000\000\000\000\000\000\200'; head -c 72 /dev/zero; printf "$2"; head -c 3000 /dev/urandom; } > "$1"; }
chd $S/gd.chd CHGD; chd $S/cd.chd CHT2; head -c 3000 /dev/urandom > $S/e.pbp; head -c 4096 /dev/urandom > $S/SCPH5501.BIN
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
	ok "$U: a text file is skipped" $(has "$o" "skip notes.txt: not a Game Boy, SNES, DS, N64, Dreamcast, Genesis or PlayStation game") "$o"
	ok "$U: a ZIP without a game is skipped" $(has "$o" "skip text.zip: no Game Boy, SNES, DS, N64, Dreamcast, Genesis or PlayStation game inside") "$o"
	ok "$U: a file outside the allowed places is refused" $(has "$o" "skip passwd: not an allowed place") "$o"
	ok "$U: the count" $(has "$o" "done 4 added, 2 skipped") "$o"
	ok "$U: the unpacking folder is cleaned up" $([ ! -e $GAMES_TMP ] && echo 1 || echo 0)
	printf '%s\n' "$S/MADE UP_ YELLOW - SPECIAL EDITION.ZIP" > $T/list; o=$($SH $P $T/list 2>&1)
	ok "$U: adding it again skips it" $(has "$o" "already in the library") "$o"
	printf '%s\n' $S/n.z64 $S/d.cdi "$S/dc/Made Up Disc.gdi" $S/zipped-dc.zip $S/broken.gdi > $T/list; o=$($SH $P $T/list 2>&1)
	ok "$U: (1.6.1) a Nintendo 64 game goes to N64" $([ -f $GAMES_DIR/N64/n.z64 ] && echo 1 || echo 0) "$o"
	ok "$U: a Dreamcast disc in one file goes to DC" $([ -f $GAMES_DIR/DC/d.cdi ] && echo 1 || echo 0) "$o"
	ok "$U: a .gdi goes in a folder of its own with all its tracks, the same files" $(cmp -s "$GAMES_DIR/DC/Made Up Disc/track 03.bin" "$S/dc/track 03.bin" && cmp -s "$GAMES_DIR/DC/Made Up Disc/track02.raw" $S/dc/track02.raw && [ -f "$GAMES_DIR/DC/Made Up Disc/Made Up Disc.gdi" ] && echo 1 || echo 0) "$o"
	ok "$U: ...and one in a ZIP too" $(cmp -s $GAMES_DIR/DC/Zipped/zt.bin $S/dz/zt.bin && [ -f $GAMES_DIR/DC/Zipped/Zipped.gdi ] && echo 1 || echo 0) "$o"
	ok "$U: a .gdi whose track is missing is skipped, saying which" $(has "$o" "skip broken.gdi: its track gone.bin is not beside it") "$o"
	ok "$U: the count" $(has "$o" "done 4 added, 1 skipped") "$o"
	printf '%s\n' $S/g.md $S/h.GEN $S/sega.bin $S/track.bin "$S/ps/Made Up PS.cue" $S/zipped-ps.zip $S/gd.chd $S/cd.chd $S/e.pbp $S/SCPH5501.BIN > $T/list; o=$($SH $P $T/list 2>&1)
	ok "$U: (1.6.1) Genesis games go to MD" $([ -f $GAMES_DIR/MD/g.md ] && [ -f $GAMES_DIR/MD/h.GEN ] && echo 1 || echo 0) "$o"
	ok "$U: a .bin saying SEGA in its header is a Genesis game" $(cmp -s $GAMES_DIR/MD/sega.bin $S/sega.bin && echo 1 || echo 0) "$o"
	ok "$U: another .bin is skipped as a disc's track" $(has "$o" "skip track.bin: a disc's track (add its .cue)") "$o"
	ok "$U: a .cue goes in a folder of its own in PS1 with the files it names" $(cmp -s "$GAMES_DIR/PS1/Made Up PS/Made Up PS (Track 1).bin" "$S/ps/Made Up PS (Track 1).bin" && cmp -s "$GAMES_DIR/PS1/Made Up PS/t2.bin" $S/ps/t2.bin && [ -f "$GAMES_DIR/PS1/Made Up PS/Made Up PS.cue" ] && echo 1 || echo 0) "$o"
	ok "$U: ...and one in a ZIP, its track not added as a game of its own" $(cmp -s "$GAMES_DIR/PS1/Zipped PS/pz.bin" $S/pz/pz.bin && [ ! -e $GAMES_DIR/MD/pz.bin ] && echo 1 || echo 0) "$o"
	ok "$U: a GD-ROM CHD goes to DC, a CD one to PS1" $([ -f $GAMES_DIR/DC/gd.chd ] && [ -f $GAMES_DIR/PS1/cd.chd ] && [ ! -e $GAMES_DIR/PS1/gd.chd ] && echo 1 || echo 0) "$o"
	ok "$U: a .pbp goes to PS1, a BIOS file to PS1/BIOS" $([ -f $GAMES_DIR/PS1/e.pbp ] && [ -f $GAMES_DIR/PS1/BIOS/SCPH5501.BIN ] && echo 1 || echo 0) "$o"
	ok "$U: the count" $(has "$o" "done 9 added, 1 skipped") "$o"
	ok "$U: saves and states folders for Genesis and PlayStation" $([ -d $GAMES_DIR/MD/Saves ] && [ -d $GAMES_DIR/PS1/States ] && echo 1 || echo 0)
done
echo "$((N-F))/$N passed"; [ $F = 0 ]
