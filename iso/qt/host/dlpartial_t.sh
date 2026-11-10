#!/bin/sh
# 1.5.7 (plan stage 6): unfinished downloads (.crdownload) that have not grown for a day are removed from .incoming when
# it is listed, with a line in the health log; newer ones and finished files stay. dl_old_partials from rudolfd.c.
#   sh dlpartial_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); C=$HERE/../../sys/rudolfd.c
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 - $3"; fi; }
{ printf '#include <%s>\n' stdio.h stdlib.h string.h stdarg.h dirent.h time.h sys/stat.h unistd.h
  printf '%s\n' 'static void health_add(const char *k, const char *fmt, ...) { va_list ap; va_start(ap, fmt); printf("HEALTH %s ", k); vprintf(fmt, ap); puts(""); va_end(ap); }'
  sed -n '/^static int dl_old_partials(const char \*dir)/,/^}/p' "$C"
  printf '%s\n' 'int main(int c, char **v) { int n = dl_old_partials(v[1]); printf("removed %d", n); puts(""); return 0; }'; } > $W/t.c
gcc -w -o $W/t $W/t.c || { echo "FAIL dl_old_partials not found"; exit 1; }
I=$W/.incoming; mkdir -p $I
head -c 3145728 /dev/zero > "$I/Unconfirmed 123.crdownload"; touch -d '2 days ago' "$I/Unconfirmed 123.crdownload"
echo x > $I/fresh.zip.crdownload; echo x > $I/old.zip; touch -d '3 days ago' $I/old.zip; mkdir "$I/dir.crdownload"; touch -d '3 days ago' "$I/dir.crdownload"
o=$($W/t $I)
ok "1. a partial download older than a day is removed, with a health-log line (name and size)" "$([ ! -e "$I/Unconfirmed 123.crdownload" ] && echo "$o" | grep -q 'HEALTH disk an unfinished download left for over a day was removed: Unconfirmed 123.crdownload (3 MB)' && echo "$o" | grep -q 'removed 1' && echo 1)" "$o"
ok "2. a partial download still arriving, a finished file and a folder stay" "$([ -e $I/fresh.zip.crdownload ] && [ -e $I/old.zip ] && [ -d "$I/dir.crdownload" ] && echo 1)" "$(ls -la $I)"
ok "3. it runs when the incoming folder is listed" "$(grep -q 'if (!strcmp(what, "incoming")) dl_old_partials(dir);' "$C" && echo 1)" ""
echo "$P/$((P+F)) passed"; [ $F = 0 ]
