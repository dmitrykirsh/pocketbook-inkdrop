#!/bin/sh
# InkDrop — local Wi-Fi transfer to /mnt/ext1/Downloads.
D=/mnt/ext1/system/inkdrop
T=/var/tmp
export LD_LIBRARY_PATH=/ebrmain/lib:/lib:/usr/lib
mkdir -p "$D"
if ! cmp -s "$D/inkdrop" "$T/inkdrop" 2>/dev/null; then
  P=$(cat "$T/inkdrop.pid" 2>/dev/null)
  if [ -n "$P" ] && [ -r "/proc/$P/cmdline" ] && tr '\000' ' ' < "/proc/$P/cmdline" | grep -q '/var/tmp/inkdrop'; then kill "$P" 2>/dev/null; sleep 2; fi
  cp -f "$D/inkdrop" "$T/inkdrop"; chmod +x "$T/inkdrop"
fi
cp -f "$D/inkdrop-ui" "$T/inkdrop-ui"; chmod +x "$T/inkdrop-ui"
P=$(cat "$T/inkdrop.pid" 2>/dev/null)
if [ -z "$P" ] || ! kill -0 "$P" 2>/dev/null; then "$T/inkdrop" >>"$D/server.log" 2>&1 & sleep 2; fi
exec "$T/inkdrop-ui" >>"$D/ui.log" 2>&1
