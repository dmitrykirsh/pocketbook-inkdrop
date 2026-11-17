#!/bin/bash
set -e
# Run inside the known PocketBook cross-build container after placing the toolchain and firmware libs in book/.
cd "$(dirname "$0")"
mkdir -p out
export PATH=/tc/bin:$PATH
CC=arm-buildroot-linux-gnueabihf-gcc
COMMON="-O2 -static-libgcc -I src -I /tc/include -L libs -l:libinkview.so -l:libc.so.6 -l:libpthread.so.0 -l:libdl.so.2 -l:libm.so.6 -Wl,--allow-shlib-undefined -Wl,--dynamic-linker=/lib/ld-linux.so.3"
$CC -Wall -o out/inkdrop src/inkdrop_server.c $COMMON
$CC -Wall -Wno-unused-result -o out/inkdrop-ui src/inkdrop_ui.c src/qr.c $COMMON
