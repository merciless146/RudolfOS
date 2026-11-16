#!/bin/sh
# 1.6.0: OpenGL for Linux apps (the owner's report of 2026-11-16: Unvanquished's "Couldn't load a renderer.", SuperTuxKart
# not starting). 1.5.9's X server looked for Mesa's DRI drivers in "/usr/lib/$MA/dri" (xshim.sh wrote the folder in
# single quotes), so its GLX had no OpenGL for any program. Checks: xshim.sh's dri.pc names the build machine's folder
# (x86 and ARM64); xbuild.sh refuses a libglx.so with another folder (1.5.9's and 1.5.8's strings); the self-test's
# linux-opengl says when the X server offers no OpenGL (the Xorg.log of 1.6.0 beta 2026111611 in QEMU).   sh glx60_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); ISO=$HERE/../..; ST=$ISO/sys/root/usr/lib/rudolf/selftest
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; P=0; F=0
ok() { if [ "$2" = 0 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1${3:+ - $3}"; fi; }
# 1. xshim.sh's lines for dri.pc, run on a dri.pc as pc() writes it
grep -E '^(sed|grep) .*dri\.pc' $ISO/xshim.sh > $W/dripc.sh
for ma in x86_64-linux-gnu aarch64-linux-gnu; do
	mkdir -p $W/$ma/lib/pkgconfig; printf 'prefix=/usr\nincludedir=${prefix}/include\n\nName: dri\nVersion: 21.3.7\n' > $W/$ma/lib/pkgconfig/dri.pc
	o=$(cd $W && MA=$ma P=$W/$ma bash -e -c ". $W/dripc.sh" 2>&1); r=$?
	ok "1. xshim.sh: dri.pc names the DRI drivers' folder /usr/lib/$ma/dri" $([ $r = 0 ] && grep -qx "dridriverdir=/usr/lib/$ma/dri" $W/$ma/lib/pkgconfig/dri.pc; echo $?) "$o $(cat $W/$ma/lib/pkgconfig/dri.pc)"
done
# 2. xbuild.sh's check of the built libglx.so (its last lines), on 1.5.8's and 1.5.9's folder names
sed -n '/^GLXM=/,$p' $ISO/xbuild.sh > $W/glxchk.sh
mkdir -p $W/x/stage/usr/lib/xorg/modules/extensions; M=$W/x/stage/usr/lib/xorg/modules/extensions/libglx.so
printf '\177ELF\0\0../hw/xfree86/dri2\0/usr/lib/x86_64-linux-gnu/dri\0swrast\0' > $M
(XS=$W/x MA=x86_64-linux-gnu bash -c ". $W/glxchk.sh" >/dev/null 2>&1); ok "2. xbuild.sh accepts 1.5.8's libglx.so (/usr/lib/x86_64-linux-gnu/dri)" $?
printf '\177ELF\0\0../hw/xfree86/dri2\0/usr/lib/$MA/dri\0swrast\0' > $M
o=$(XS=$W/x MA=x86_64-linux-gnu bash -c ". $W/glxchk.sh" 2>&1); r=$?
ok "3. ...and refuses 1.5.9's (/usr/lib/\$MA/dri), saying so" $([ $r != 0 ] && echo "$o" | grep -q 'does not look for the DRI drivers'; echo $?) "$o"
# 3. the self-test
st() { ST_SYS=$W/s/sys ST_RUN=$W/s/run ST_PERSIST=$W/s/p ST_HOME=$W/s/h ST_DEV=$W/s/d ST_PROC=$W/s/proc sh $ST 2>/dev/null | grep ' linux-opengl '; }
mkdir -p $W/s/run; echo gpu > $W/s/run/display-mode
cat > $W/s/run/Xorg.log <<'X'
[    15.317] (II) Initializing extension GLX
[    15.317] (II) AIGLX: Screen 0 is not DRI2 capable
[    15.317] (EE) AIGLX error: dlopen of /usr/lib/$MA/dri/swrast_dri.so failed (/usr/lib/$MA/dri/swrast_dri.so: cannot open shared object file: No such file or directory)
[    15.317] (EE) AIGLX error: unable to load driver swrast
[    15.317] (EE) GLX: could not load software renderer
[    15.317] (II) GLX: no usable GL providers found for screen 0
X
o=$(st); ok "4. self-test: the X server offering no OpenGL fails, naming the driver it could not load" $(echo "$o" | grep -q '^FAIL linux-opengl - the X server offers no OpenGL .*AIGLX error: dlopen of /usr/lib/$MA/dri/swrast_dri.so failed'; echo $?) "$o"
cat > $W/s/run/Xorg.log <<'X'
[     4.101] (II) Initializing extension GLX
[     4.180] (II) AIGLX: Loaded and initialized radeonsi
[     4.180] (II) GLX: Initialized DRI2 GL provider for screen 0
X
o=$(st); ok "5. ...a GL provider passes" $(echo "$o" | grep -q '^PASS linux-opengl - the X server offers OpenGL (GLX) to Linux apps and games (DRI2)'; echo $?) "$o"
echo xvfb > $W/s/run/display-mode; o=$(st); ok "6. ...the software display: skipped" $(echo "$o" | grep -q '^SKIP linux-opengl'; echo $?) "$o"
echo "$P/$((P+F)) passed"; [ $F = 0 ]
