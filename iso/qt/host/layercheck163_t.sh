#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 9): systools/layercheck.sh, asking a base layer's file list -- each thing it wants, each
# one taken away, the sound server's rule (no pipewire-pulse), and an image that cannot be read. Runs anywhere: a
# stand-in unsquashfs prints the listing, so no squashfs-tools and no 250 MB layer are needed.
#   sh layercheck163_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../systools/layercheck.sh
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; NP=0; NF=0
ok() { if [ "$2" = 1 ]; then NP=$((NP+1)); echo "PASS $1"; else NF=$((NF+1)); echo "FAIL $1 - $3"; fi; }
mkdir -p "$W/bin"; : > "$W/base.sfs"; : > "$W/bad.sfs"; : > "$W/list"
cat > "$W/bin/unsquashfs" <<'EOS'
#!/bin/sh
# (the stand-in: a real layer's listing, or a refusal for the image the test calls unreadable)
case "$2" in *bad.sfs) echo "Not a valid squashfs image" >&2; exit 1;; esac
cat "$RUDOLF_TEST_LIST"
EOS
chmod 755 "$W/bin/unsquashfs"
export PATH=$W/bin:$PATH RUDOLF_TEST_LIST=$W/list
L() {  # a full layer: the daemon, the session manager, the camera's plug-in and the drop-in
	cat > "$W/list" <<'EOS'
drwxr-xr-x root/root         0 2026-11-27 12:00 squashfs-root/
-rwxr-xr-x root/root     65536 2026-11-27 12:00 squashfs-root/usr/bin/pipewire
-rwxr-xr-x root/root     32768 2026-11-27 12:00 squashfs-root/usr/bin/wireplumber
-rw-r--r-- root/root     16384 2026-11-27 12:00 squashfs-root/usr/lib/x86_64-linux-gnu/spa-0.2/v4l2/libspa-v4l2.so
-rw-r--r-- root/root       120 2026-11-27 12:00 squashfs-root/etc/wireplumber/main.lua.d/51-no-alsa.lua
EOS
}
run() { OUT=$(sh "$S" "$1" 2>&1); RC=$?; }
L; run "$W/base.sfs"
ok "1. the layer with all four: passes" "$([ $RC = 0 ] && echo "$OUT" | grep -q '^PASS base.sfs carries PipeWire' && echo 1)" "$RC $OUT"
grep -v 'usr/bin/pipewire$' "$W/list" > "$W/l2"; mv "$W/l2" "$W/list"; run "$W/base.sfs"
ok "2. no daemon: a failure that names it" "$([ $RC = 1 ] && echo "$OUT" | grep -q 'pipewire (the daemon)' && echo 1)" "$RC $OUT"
L; grep -v 'usr/bin/wireplumber$' "$W/list" > "$W/l2"; mv "$W/l2" "$W/list"; run "$W/base.sfs"
ok "3. no WirePlumber: named" "$([ $RC = 1 ] && echo "$OUT" | grep -q 'wireplumber (the session manager)' && echo 1)" "$RC $OUT"
L; grep -v 'v4l2/libspa-v4l2\.so$' "$W/list" > "$W/l2"; mv "$W/l2" "$W/list"; run "$W/base.sfs"
ok "4. no camera plug-in: named" "$([ $RC = 1 ] && echo "$OUT" | grep -q 'v4l2 SPA plug-in' && echo 1)" "$RC $OUT"
L; sed 's/^-rwxr-xr-x root\/root     65536/-rw-r--r-- root\/root     65536/' "$W/list" > "$W/l2"; mv "$W/l2" "$W/list"; run "$W/base.sfs"
ok "5. a daemon that is not executable: named" "$([ $RC = 1 ] && echo "$OUT" | grep -q 'pipewire (the daemon)' && echo 1)" "$RC $OUT"
L; grep -v '51-no-alsa' "$W/list" > "$W/l2"; mv "$W/l2" "$W/list"; run "$W/base.sfs"
ok "6. no 51-no-alsa drop-in: named" "$([ $RC = 1 ] && echo "$OUT" | grep -q '51-no-alsa drop-in' && echo 1)" "$RC $OUT"
L; echo '-rwxr-xr-x root/root      8192 2026-11-27 12:00 squashfs-root/usr/bin/pipewire-pulse' >> "$W/list"; run "$W/base.sfs"
ok "7. pipewire-pulse in it: refused, PulseAudio stays the sound server" "$([ $RC = 1 ] && echo "$OUT" | grep -q 'pipewire-pulse (PulseAudio must stay' && echo 1)" "$RC $OUT"
run "$W/bad.sfs"
ok "8. an image that is not a squashfs: it says so, exit 2" "$([ $RC = 2 ] && echo "$OUT" | grep -q 'is not a squashfs image' && echo 1)" "$RC $OUT"
OUT=$(sh "$S" 2>&1); RC=$?
ok "9. no argument: the usage, exit 2" "$([ $RC = 2 ] && echo "$OUT" | grep -q 'usage: layercheck.sh' && echo 1)" "$RC $OUT"
echo "$NP passed, $NF failed"; [ $NF = 0 ]
