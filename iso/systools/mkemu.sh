#!/bin/bash
# Build Emu's SNES, DS, Genesis and PlayStation cores as WebAssembly into src/emu-out (clang 18 + the WASI system libraries).
# 1.5.2: they are published in the Rudolf OS Library: copy them to rudolfos-library/apps/rudolf-emu/www (and
# emu-src.tar.xz to apps/rudolf-emu/source), raise its version, and merge there. The SNES core is also inlined in the
# desktop (EMU_SNES_WASM), so SNES games play without the package.
#   LakeSnes (MIT):          src/lakesnes      github.com/angelo-wf/LakeSnes
#   DeSmuME (GPL-2.0+):      src/desmume-wasm  github.com/44670/desmume-wasm (DeSmuME with a wasm port)
#   ClownMDEmu (AGPL-3.0+):  src/clownmdemu-libretro  github.com/Clownacy/clownmdemu-libretro (1.6.1, 004044c, with --recursive)
#   PCSX-ReARMed (GPL-2.0+): src/pcsx_rearmed          github.com/libretro/pcsx_rearmed (1.6.1, 7b0e8f8, with --recursive)
#   zlib:                    src/zlib          github.com/madler/zlib (v1.3.1)
#   WASI libraries:          src/wasi          github.com/WebAssembly/wasi-sdk (wasi-sysroot-22.0 + builtins)
# GPL-2.0-or-later (this script).
set -e
I=$(cd "$(dirname "$0")/.." && pwd); S=$I/src; W=$S/wasi; OUT=$S/emu-out; B=$S/emu-build; mkdir -p $OUT $B
RD=$(clang -print-resource-dir); [ -f $RD/lib/wasi/libclang_rt.builtins-wasm32.a ] || { mkdir -p $RD/lib/wasi; cp $W/lib/wasi/libclang_rt.builtins-wasm32.a $RD/lib/wasi/; }
T="--target=wasm32-wasi --sysroot=$W/wasi-sysroot"
LINK="-mexec-model=reactor -Wl,--export=malloc -Wl,--export=free -Wl,--export-memory -Wl,--strip-all"
echo "== SNES (LakeSnes)"
clang $T -O2 $LINK -Wl,--initial-memory=33554432 -Wl,--max-memory=268435456 -o $OUT/lakesnes.wasm $I/sys/emu/lakesnes_api.c $S/lakesnes/snes/*.c
echo "== DS (DeSmuME)"
D=$S/desmume-wasm/desmume/src
# the two places DeSmuME's core throws an exception end the program instead (no exceptions in this WebAssembly build, as in Emscripten's default)
sed -i 's/throw "Not tested: emufile memory fgets";/abort(); return NULL; \/\* rudolf \*\//' $D/emufile.h
sed -i 's/throw std::bad_alloc();/abort(); \/\* rudolf \*\//' $D/render3D.cpp
# the battery-save buffer (savFile) belongs to desmume_api.cpp and lives for the whole session: never free it on a reset or
# state load; and with an empty save, ensure() must not seek to -1 (that asks the memory file to grow to 4 GB)
sed -i -e 's/^\(\s*\)delete fpMC;/\1{ extern EMUFILE_MEMORY *savFile; if ((void *)fpMC != (void *)savFile) delete fpMC; }/' \
	-e 's/fpMC->fseek(addr-1, SEEK_SET);/fpMC->fseek(addr ? addr-1 : 0, SEEK_SET); \/\* rudolf \*\//' $D/mc.cpp
# the wasm port leaves some slot-1 devices out of the list; state loading looks them up by id
sed -i 's/if (slot1_List\[i\]->info()->id() == ID)/if (slot1_List[i] \&\& slot1_List[i]->info()->id() == ID)/' $D/slot1.cpp
python3 - $D/utils/vfat.cpp <<'PY'
import sys,re;p=sys.argv[1];s=open(p,newline='').read()
s2=re.sub(r'try\s*\{\s*(file = new EMUFILE_MEMORY\(dataSectors\*512\);)\s*\}\s*catch\(std::bad_alloc\)\s*\{[^}]*\}',r'\1 /* rudolf: no exceptions */',s)
if s2!=s: open(p,'w',newline='').write(s2)
PY
SRC=$(sed -n '/libdesmume_a_SOURCES = /,/^$/p' $D/Makefile.am | tr ' \t\\' '\n\n\n' | grep -E '\.(c|cpp)$' | grep -v -e '^Disassembler' -e 'commandline.cpp' -e 'utils/task.cpp')   # the wasm port's task.h has no threads
CF="$T -O2 -DHAVE_LIBZ -DHOST_LINUX -DNDEBUG -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_PROCESS_CLOCKS -I$I/sys/emu/stub -I$D -I$D/libretro-common/include -I$S/zlib -I$B -Wno-everything"
printf '#define VERSION "0.9.13"\n' > $B/config.h
OBJ=""; n=0
for f in $SRC driver.cpp libretro-common/encodings/encoding_utf.c $I/sys/emu/desmume_api.cpp $I/sys/emu/stub/sockstub.c; do
	p=$f; [ -f "$p" ] || p=$D/$f; [ -f "$p" ] || continue
	o=$B/$(echo $f | tr '/' '_' | sed 's/\.[a-z]*$//').o; OBJ="$OBJ $o"
	[ "$o" -nt "$p" ] && continue
	X=""; case $f in wifi.cpp) X="-include $I/sys/emu/stub/sockstub.h";; esac
	case $p in *.c) clang $CF -c $p -o $o & ;; *) clang++ $CF $X -std=gnu++14 -fno-exceptions -c $p -o $o & ;; esac
	n=$((n+1)); [ $((n % $(nproc))) = 0 ] && wait
done
wait
for f in adler32 compress crc32 deflate inflate inftrees inffast trees uncompr zutil; do o=$B/z_$f.o; [ -f $o ] || clang $T -O2 -c $S/zlib/$f.c -o $o; OBJ="$OBJ $o"; done
clang++ $T -O2 $LINK -Wl,--initial-memory=134217728 -Wl,--max-memory=1610612736 -Wl,-z,stack-size=1048576 -lwasi-emulated-signal -lwasi-emulated-mman -lwasi-emulated-process-clocks -o $OUT/desmume.wasm $OBJ
# 1.6.1: libretro cores, each linked with Emu's libretro frontend (sys/emu/lr_api.c). setjmp/longjmp are WebAssembly
# exceptions (-wasm-enable-sjlj); sjlj_shim.c gives the helper names clang 18 calls to the sysroot's newer ones
LR="-mllvm -wasm-enable-sjlj"
clang $T -O2 $LR -c $I/sys/emu/sjlj_shim.c -o $B/sjlj_shim.o
echo "== Genesis / Mega Drive (ClownMDEmu)"
C=$S/clownmdemu-libretro
clang $T -O2 $LR -I$C/libretro-common/include -c $I/sys/emu/lr_api.c -o $B/lr_api.o
clang $T -O2 $LR -Wno-everything -I$C/libretro-common/include -c $C/unity.c -o $B/clownmd_unity.o
clang $T -O2 $LINK -Wl,--initial-memory=33554432 -Wl,--max-memory=536870912 -Wl,-z,stack-size=1048576 -o $OUT/clownmd.wasm $B/clownmd_unity.o $B/lr_api.o $B/sjlj_shim.o -lsetjmp
echo "== PlayStation (PCSX-ReARMed: the interpreter, the Unai software renderer, CHD discs)"
P=$S/pcsx_rearmed; PF="$T -msimd128 -DNO_DYLIB -DNO_SOCKET -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_PROCESS_CLOCKS -Wno-everything $LR"
PM="-f Makefile.libretro platform=unix ARCH=wasm32 DYNAREC= NO_MMAP=1 USE_MINIZ=1 HAVE_CHD=1 BUILTIN_GPU=unai USE_RTHREADS=0 USE_ASYNC_CDROM=0 USE_ASYNC_GPU=0 USE_ASYNC_SPU=0 LIBPTHREAD= HAVE_PHYSICAL_CDROM=0"
# make builds the objects; its last step (a Linux .so) fails here and is replaced by the link below
make -C $P -k -j$(nproc) $PM CC="clang $PF" CXX="clang++ $PF -fno-exceptions" AR=llvm-ar >/dev/null 2>&1 || true
PO=$(make -C $P $PM CC=clang CXX=clang++ -n 2>/dev/null | grep -E -- '-o pcsx_rearmed_libretro' | head -1 | tr ' ' '\n' | grep '\.o$' | sed "s|^|$P/|")
[ -n "$PO" ] || { echo "PCSX-ReARMed: no objects"; exit 1; }
clang $T -O2 $LR -I$P/deps/libretro-common/include -c $I/sys/emu/lr_api.c -o $B/lr_api_ps.o
clang++ $T -O2 $LINK -Wl,--gc-sections -Wl,--initial-memory=67108864 -Wl,--max-memory=1073741824 -Wl,-z,stack-size=1048576 -o $OUT/pcsx.wasm $PO $B/lr_api_ps.o $B/sjlj_shim.o \
	-lsetjmp -lwasi-emulated-signal -lwasi-emulated-mman -lwasi-emulated-process-clocks
# the SNES core is also part of the desktop page, so SNES games play in the browser version too
printf "/* LakeSnes (MIT, github.com/angelo-wf/LakeSnes) as WebAssembly, built by iso/systools/mkemu.sh */\nconst EMU_SNES_WASM='%s';\n" "$(base64 -w0 $OUT/lakesnes.wasm)" > $I/../v10/emusnes26.js
# the corresponding source for the disc (build_iso.sh puts it in SOURCE/EMU.TXZ): both cores as built, with the changes above
tar -C $S -cJf $S/emu-src.tar.xz --owner=0 --group=0 --exclude=.git --exclude='desmume/src/frontend/windows' --exclude='desmume/src/frontend/cocoa' \
	--exclude='*.o' --exclude='*.so' lakesnes desmume-wasm zlib clownmdemu-libretro pcsx_rearmed
ls -la $OUT $S/emu-src.tar.xz
