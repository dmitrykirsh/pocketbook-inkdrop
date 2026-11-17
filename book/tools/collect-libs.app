#!/bin/sh
# One-time development helper: copies only runtime libraries exposed by firmware.
OUT=/mnt/ext1/inkdrop-libs
mkdir -p "$OUT"
for n in libinkview.so libc.so.6 libdl.so.2 libm.so.6 libpthread.so.0 librt.so.1 ld-linux.so.3; do
  for d in /ebrmain/lib /lib /usr/lib; do
    [ -e "$d/$n" ] && { cp -fL "$d/$n" "$OUT/"; break; }
  done
done
