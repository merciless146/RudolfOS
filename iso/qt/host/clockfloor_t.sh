#!/bin/sh
# 1.4.4 part 7: clockfloor with a stand-in clock. sh clockfloor_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); P=$HERE/../../sys/root/usr/lib/rudolf/clockfloor; SH=${SH:-sh}
T=$(mktemp -d); trap 'rm -rf $T' EXIT
cat > $T/date <<'E'
#!/bin/sh
# a stand-in clock: the time in $T/now (YYYY-MM-DD HH:MM:SS); -s sets it
S=$(dirname "$0")/now
case "$*" in *-s*) for a; do l=$a; done; echo "$l" > $S;; *'+%Y%m%d'*) cut -c1-10 $S | tr -d -;; *'+%F %T'*) cat $S;; esac
E
chmod +x $T/date; echo 2026-10-30 > $T/released
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
echo "2016-01-01 00:03:10" > $T/now; o=$(CLOCKFLOOR_DATE=$T/date $SH $P $T/released)
ok "a clock at 2016-01-01 is set to the release date" $([ "$(cat $T/now)" = "2026-10-30 00:00:00" ] && echo 1 || echo 0) "$(cat $T/now) / $o"
echo "2026-11-01 11:50:00" > $T/now; CLOCKFLOOR_DATE=$T/date $SH $P $T/released >/dev/null
ok "a clock after the release date is left alone" $([ "$(cat $T/now)" = "2026-11-01 11:50:00" ] && echo 1 || echo 0) "$(cat $T/now)"
echo "2016-01-01 00:00:00" > $T/now; CLOCKFLOOR_DATE=$T/date $SH $P $T/nofile >/dev/null
ok "no release date: left alone" $([ "$(cat $T/now)" = "2016-01-01 00:00:00" ] && echo 1 || echo 0)
echo "$((N-F))/$N passed"; [ $F = 0 ]
