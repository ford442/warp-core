#!/bin/sh

em++ --bind -std=c++17 \
    src/*.cxx src/App/*.cxx -Iinclude/ \
    -O3  -g \
    -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=14 \
    -s -s INITIAL_MEMORY=1400mb -s ALLOW_MEMORY_GROWTH=0 -s MALLOC="emmalloc" -s USES_DYNAMIC_ALLOC=0 -s SUPPORT_LONGJMP=0 -s FILESYSTEM=0 -flto \
    -s WASM=1 \
    -Wcast-align -Wover-aligned -s WARN_UNALIGNED=1 \
    -o main.js

