#!/bin/bash
# Build Luau as WebAssembly into www/lib/luau.wasm, for Blitzen's Luau and Pilot.lua (Rudolf OS 1.2.5).
#   Luau (MIT):                 src/luau     github.com/luau-lang/luau, tag 0.741 (VM, compiler, type checker)
#   LLVM 18 runtimes (Apache-2.0 WITH LLVM-exception): src/llvm18  libcxxabi + libunwind, for C++ exceptions,
#                               which Luau's parser, compiler and checker use (wasi-sdk ships them without)
#   WASI libraries:             src/wasi     wasi-sysroot-22.0 + builtins (as mkemu.sh)
# The page runs it with WebAssembly exception handling (Chrome 95+, Firefox 100+, Safari 15.2+, Node 17+).
# GPL-2.0-or-later (this script).
set -e
I=$(cd "$(dirname "$0")/.." && pwd); S=${RUDOLF_SRC:-$I/src}; W=$S/wasi; OUT=$I/www/lib; B=$S/luau-build; E=$S/luau-eh
LUAU_TAG=0.741; LUAU_COMMIT=2d88f9b5facaa9e4ec91247b39ae8251bd49a244; LLVM_TAG=llvmorg-18.1.3
mkdir -p $B $E $OUT
[ -d $S/luau ] || git clone -q https://github.com/luau-lang/luau.git $S/luau
git -C $S/luau checkout -q $LUAU_COMMIT
if [ ! -d $S/llvm18/libcxxabi ]; then
	git clone -q --depth 1 --branch $LLVM_TAG --filter=blob:none --sparse https://github.com/llvm/llvm-project.git $S/llvm18
	git -C $S/llvm18 sparse-checkout set libcxxabi libunwind libcxx/include libcxx/src/include
fi
RD=$(clang -print-resource-dir); [ -f $RD/lib/wasi/libclang_rt.builtins-wasm32.a ] || { mkdir -p $RD/lib/wasi; cp $W/lib/wasi/libclang_rt.builtins-wasm32.a $RD/lib/wasi/; }
T="--target=wasm32-wasi --sysroot=$W/wasi-sysroot -O2 -DNDEBUG -fwasm-exceptions"

echo "== C++ exception runtime (libcxxabi, libunwind)"
L=$S/llvm18
F="$T -D__USING_WASM_EXCEPTIONS__ -D_LIBCXXABI_BUILDING_LIBRARY -D_LIBCPP_BUILDING_LIBRARY -D_LIBCXXABI_HAS_NO_THREADS -D_LIBCPP_HAS_NO_THREADS -DLIBCXXABI_USE_LLVM_UNWINDER -I$L/libcxxabi/include -I$L/libcxxabi/src -I$L/libcxx/src -I$L/libunwind/include -Wno-everything"
# LLVM 18 gives __cxa_init_primary_exception the usual destructor type, but in WebAssembly a destructor returns its
# argument (fixed in later LLVM): cast between the two
sed -e 's/exception_header->exceptionDestructor = dest;/exception_header->exceptionDestructor = (decltype(exception_header->exceptionDestructor))dest;/' \
	-e 's/__cxa_init_primary_exception(thrown_object, tinfo, dest);/__cxa_init_primary_exception(thrown_object, tinfo, (void (*)(void*))dest);/' \
	$L/libcxxabi/src/cxa_exception.cpp > $E/cxa_exception.cpp
rm -f $E/*.o $E/libehabi.a
clang $F -fdeclspec -std=c11 -c $L/libunwind/src/Unwind-wasm.c -o $E/unwind.o
clang++ $F -std=c++20 -c $E/cxa_exception.cpp -o $E/cxa_exception.o
for f in abort_message cxa_aux_runtime cxa_default_handlers cxa_exception_storage cxa_handlers cxa_personality cxa_vector \
	cxa_virtual cxa_guard fallback_malloc private_typeinfo stdlib_exception stdlib_new_delete stdlib_stdexcept stdlib_typeinfo cxa_demangle; do
	clang++ $F -std=c++20 -c $L/libcxxabi/src/$f.cpp -o $E/$f.o
done
ar rcs $E/libehabi.a $E/*.o

echo "== Luau $LUAU_TAG"
U=$S/luau
CF="$T -std=c++17 -Wno-everything -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_PROCESS_CLOCKS -include $I/sys/luau/nothreads.h"; for d in VM Compiler Ast Common Bytecode Config Analysis Require; do CF="$CF -I$U/$d/include"; done
OBJ=""; n=0
for f in $U/VM/src/*.cpp $U/Compiler/src/*.cpp $U/Ast/src/*.cpp $U/Common/src/*.cpp $U/Bytecode/src/*.cpp $U/Config/src/*.cpp $U/Analysis/src/*.cpp $I/sys/luau/rluau.cpp; do
	o=$B/$(basename $(dirname $(dirname $f)))_$(basename $f .cpp).o; OBJ="$OBJ $o"
	[ "$o" -nt "$f" ] && [ "$o" -nt "$0" ] && continue
	clang++ $CF -c $f -o $o &
	n=$((n+1)); [ $((n % $(nproc))) = 0 ] && wait
done
wait
clang++ $T -mexec-model=reactor -nostdlib++ -Wl,--strip-all -Wl,-z,stack-size=2097152 -Wl,--initial-memory=33554432 \
	-o $OUT/luau.wasm $OBJ -lc++ $E/libehabi.a -lwasi-emulated-process-clocks -lwasi-emulated-mman
{ echo "Luau (github.com/luau-lang/luau, $LUAU_TAG), built into lib/luau.wasm by iso/systools/mkluau.sh"; echo; cat $U/LICENSE.txt; echo; echo "===== Lua 5.1, which Luau is based on"; cat $U/lua_LICENSE.txt; } > $OUT/luau-LICENSE.txt
ls -la $OUT/luau.wasm
