#!/bin/bash
# Build Blitzen's model engine: llama.cpp's llama-server, static, CPU only, for x86-64 with AVX2 (x86-64-v3:
# the Intel N100 and anything from about 2015 on). llama.cpp: MIT, github.com/ggml-org/llama.cpp, pinned tag below.
#   mkllama.sh            -> src/llama-build/bin/llama-server and src/llama-src.tar.xz (source for the disc)
# GPL-2.0-or-later (this script).
set -e
I=$(cd "$(dirname "$0")/.." && pwd); S=$I/src; TAG=b11559; B=$S/llama-build
[ -d $S/llama.cpp ] || git clone -q --depth 1 --branch $TAG https://github.com/ggml-org/llama.cpp $S/llama.cpp
cmake -S $S/llama.cpp -B $B -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
	-DGGML_NATIVE=OFF -DGGML_AVX=ON -DGGML_AVX2=ON -DGGML_FMA=ON -DGGML_F16C=ON -DGGML_BMI2=ON -DGGML_AVX512=OFF \
	-DGGML_OPENMP=OFF -DGGML_CPU_ALL_VARIANTS=OFF -DGGML_BACKEND_DL=OFF \
	-DLLAMA_OPENSSL=OFF -DLLAMA_CURL=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_UI=OFF \
	-DLLAMA_USE_PREBUILT_UI=OFF -DLLAMA_BUILD_APP=OFF -DLLAMA_BUILD_SERVER=ON -DLLAMA_BUILD_TOOLS=ON \
	-DCMAKE_C_FLAGS="-march=x86-64-v3 -mtune=alderlake" -DCMAKE_CXX_FLAGS="-march=x86-64-v3 -mtune=alderlake" \
	-DCMAKE_EXE_LINKER_FLAGS="-static -Wl,--gc-sections" >/dev/null
cmake --build $B --target llama-server -j$(nproc) >/dev/null
strip $B/bin/llama-server
tar -C $S -cJf $S/llama-src.tar.xz --owner=0 --group=0 --exclude=.git --exclude='llama.cpp/models/*.gguf*' \
	--exclude=CLAUDE.md --exclude=AGENTS.md llama.cpp
ls -la $B/bin/llama-server $S/llama-src.tar.xz
