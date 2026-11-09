#!/bin/sh
# 1.5.6: rudolf-debian deb's script (taken from the C source), with stand-in apt-get and dpkg-deb: the package is
# installed; a second pass adds what it recommends (Steam's package adds Valve's source, so its recommendations can be
# found only after it is installed), and that pass failing does not fail the installation; a failed install stops.
HERE=$(cd "$(dirname "$0")" && pwd); SRC=$HERE/../../sys/rudolf-debian.c
T=$(mktemp -d); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
python3 - "$SRC" > $T/script.sh <<'PY'
import re,sys
s=open(sys.argv[1]).read();i=s.index('char *a[] = {"/bin/sh", "-c", "export DEBIAN_FRONTEND');j=s.index('"sh", (char *)f, NULL};',i)
print(''.join(re.findall(r'"((?:[^"\\]|\\.)*)"',s[i:j])[2:]).encode().decode('unicode_escape'))
PY
sed -i "s#/tmp/rudolf-install.deb#$T/t.deb#g" $T/script.sh
mkdir $T/bin; cat > $T/bin/apt-get <<'E'
#!/bin/sh
echo "apt-get $*" >> $LOG; case "$*" in *"install -y /"*) exit ${FAIL1:-0};; *install-recommends*) exit ${FAIL2:-0};; esac; exit 0
E
printf '#!/bin/sh\necho steam-launcher\n' > $T/bin/dpkg-deb; chmod +x $T/bin/*; echo x > $T/x.deb
run() { export LOG=$T/log FAIL1=$1 FAIL2=$2; : > $LOG; PATH=$T/bin:$PATH sh $T/script.sh $T/x.deb > $T/out 2>&1; echo $?; }
r=$(run 0 0); ok "installed, then its recommendations in a second pass (after apt-get update), exit 0" "$([ $r = 0 ] && grep -q 'install -y --install-recommends steam-launcher' $T/log && [ "$(grep -c '^apt-get update' $T/log)" = 2 ] && echo 1)" "$r $(cat $T/log)"
r=$(run 0 100); ok "recommendations not found: a note, and the installation still succeeds" "$([ $r = 0 ] && grep -q 'note: some recommended' $T/out && echo 1)" "$r $(cat $T/out)"
r=$(run 100 0); ok "a failed install fails, with no second pass" "$([ $r = 100 ] && ! grep -q install-recommends $T/log && echo 1)" "$r $(cat $T/log)"
ok "the copied file is removed" "$([ ! -e $T/t.deb ] && echo 1)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
