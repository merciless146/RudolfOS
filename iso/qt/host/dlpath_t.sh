#!/bin/sh
# 1.5.5: rudolfd's per-account Downloads folders: which account ids and which paths are accepted (dl/link, and the
# folder names dl/file, dl/read and dl/home build). Compiled from rudolfd.c.   sh dlpath_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d); trap 'rm -rf $T' EXIT
gcc -O1 -w -o $T/t $HERE/fixtures/dlpath.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
N=0; F=0
chk() { want=$1; shift; got=$($T/t "$1" | awk '{print $NF}'); N=$((N+1)); if [ "$got" = "$want" ]; then echo "PASS $want $1"; else echo "FAIL $1: got $got, want $want"; F=$((F+1)); fi; }
D=/home/rudolf/Downloads
chk 1 acct:u-boss; chk 1 acct:guest; chk 1 acct:shared; chk 1 acct:3f9a0c1e-77b2
chk 0 acct:; chk 0 acct:U-Boss; chk 0 acct:../x; chk 0 acct:a/b; chk 0 "acct:a b"; chk 0 acct:.hidden
chk 0 acct:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
chk 1 "$D/movie.mp4"; chk 1 "$D/.accounts/u-boss/movie.mp4"; chk 1 "$D/.accounts/shared/old.iso"
chk 0 "$D/.incoming/x.bin"; chk 0 "$D/.quarantine/x.bin"; chk 0 "$D/.accounts/u-boss/.hidden"
chk 0 "$D/.accounts/u-boss/sub/x"; chk 0 "$D/.accounts/../../etc/passwd"; chk 0 "$D/.accounts/U-X/a"; chk 0 "$D/.accounts//a"
chk 0 "$D/../Documents/a"; chk 0 "/etc/passwd"; chk 0 "$D"; chk 0 "$D/"; chk 0 "/home/rudolf/DownloadsX/a"
echo "$((N-F))/$N passed"; [ $F = 0 ]
