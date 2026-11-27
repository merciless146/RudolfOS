#!/bin/sh
# 1.6.3 (docs/plan-1.6.3.md Part 9): Settings › Privacy › Camera's switch against the session's WirePlumber. Its v4l2
# monitor keeps the camera's device open, so the driver cannot be unloaded while it runs: off must stop it first, and on
# must start it again; a session that runs no WirePlumber is left as it was, and a camera a program really holds is still
# reported as in use. Runs unprivileged: mount, umount, rmmod, modprobe, pgrep, pkill and su are stand-ins on PATH.
#   sh camwp_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); S=$HERE/../../sys/root/usr/lib/rudolf/camera
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; NP=0; NF=0
ok() { if [ "$2" = 1 ]; then NP=$((NP+1)); echo "PASS $1"; else NF=$((NF+1)); echo "FAIL $1 - $3"; fi; }
mkdir -p "$W/bin" "$W/run" "$W/persist/etc"; : > "$W/mounts"; : > "$W/ko"; : > "$W/run/wp"; : > "$W/pw"
echo 'uvcvideo 16384 0' > "$W/modules"
export RUDOLF_TEST_PATH=$W/bin RUDOLF_TEST_RUN=$W/run RUDOLF_TEST_PERSIST=$W/persist RUDOLF_TEST_KO=$W/ko \
	RUDOLF_TEST_MODULES=$W/modules RUDOLF_TEST_MOUNTS=$W/mounts RUDOLF_TEST_PWSOCK=$W/pw
# (each stand-in writes the order things happened in, which is what the switch's own order is checked by)
cat > "$W/bin/pgrep" <<'EOS'
#!/bin/sh
[ "$1" = -x ] && [ "$2" = wireplumber ] && [ -e "$RUDOLF_TEST_RUN/wp" ]
EOS
cat > "$W/bin/pkill" <<'EOS'
#!/bin/sh
echo "pkill $*" >> "$RUDOLF_TEST_RUN/order"; rm -f "$RUDOLF_TEST_RUN/wp"
EOS
cat > "$W/bin/rmmod" <<'EOS'
#!/bin/sh
echo "rmmod $*" >> "$RUDOLF_TEST_RUN/order"
[ -e "$RUDOLF_TEST_RUN/busy" ] && exit 1
grep -v '^uvcvideo ' "$RUDOLF_TEST_MODULES" > "$RUDOLF_TEST_MODULES.n"; mv "$RUDOLF_TEST_MODULES.n" "$RUDOLF_TEST_MODULES"
EOS
cat > "$W/bin/modprobe" <<'EOS'
#!/bin/sh
echo "modprobe $*" >> "$RUDOLF_TEST_RUN/order"
EOS
cat > "$W/bin/mount" <<'EOS'
#!/bin/sh
echo "mount $*" >> "$RUDOLF_TEST_RUN/order"; echo "/dev/null $2 none bind 0 0" >> "$RUDOLF_TEST_MOUNTS"
EOS
cat > "$W/bin/umount" <<'EOS'
#!/bin/sh
echo "umount $*" >> "$RUDOLF_TEST_RUN/order"; : > "$RUDOLF_TEST_MOUNTS"
EOS
cat > "$W/bin/su" <<'EOS'
#!/bin/sh
echo "su $*" >> "$RUDOLF_TEST_RUN/order"; echo 1 > "$RUDOLF_TEST_RUN/wp"
EOS
printf '#!/bin/sh\nexit 0\n' > "$W/bin/wireplumber"   # (what the session starts again)
chmod 755 "$W/bin"/*
order() { tr '\n' ',' < "$W/run/order"; }
mk_loaded() { echo 'uvcvideo 16384 0' > "$W/modules"; : > "$W/mounts"; : > "$W/run/wp"; rm -f "$W/run/camera-off" "$W/run/camera-wp-stopped" "$W/run/busy"; : > "$W/run/order"; }

mk_loaded
OUT=$(sh "$S" off 2>&1)
ok "1. the camera off with WirePlumber running: WirePlumber is stopped before the driver is unloaded" \
	"$(awk '/^pkill/{p=NR} /^rmmod/{r=NR} END{exit !(p && r && p < r)}' "$W/run/order" && echo 1)" "$(order)"
ok "2. ...and the camera is off, with the mark that says it was this switch that stopped it" \
	"$([ -f "$W/run/camera-wp-stopped" ] && [ -f "$W/run/camera-off" ] && [ ! -e "$W/run/wp" ] && echo "$OUT" | grep -q '^the camera is off$' && echo 1)" "$OUT"

OUT=$(sh "$S" on 2>&1)
ok "3. the camera on: the driver loads and WirePlumber is started again from the mark, as the desktop user" \
	"$([ -e "$W/run/wp" ] && [ ! -f "$W/run/camera-wp-stopped" ] && [ ! -f "$W/run/camera-off" ] && grep -q '^su -s /bin/sh rudolf' "$W/run/order" && echo "$OUT" | grep -q '^the camera is on$' && echo 1)" "$(order)"

mk_loaded; rm -f "$W/run/wp"
OUT=$(sh "$S" off 2>&1)
ok "4. a session that runs no WirePlumber: the driver is unloaded and nothing is started on the way back" \
	"$(grep -qx 'rmmod uvcvideo' "$W/run/order" && ! grep -q '^pkill' "$W/run/order" && [ ! -f "$W/run/camera-wp-stopped" ] && echo 1)" "$(order)"
OUT=$(sh "$S" on 2>&1)
ok "5. ...and switching it on starts nothing" "$(! grep -q '^su ' "$W/run/order" && [ ! -e "$W/run/wp" ] && echo 1)" "$(order)"

mk_loaded; : > "$W/run/busy"
OUT=$(sh "$S" off 2>&1); RC=$?
ok "6. a program really holds the camera: it is still in use, and WirePlumber is given back" \
	"$([ $RC = 1 ] && echo "$OUT" | grep -q 'the camera is in use' && [ ! -f "$W/run/camera-wp-stopped" ] && [ -e "$W/run/wp" ] && echo 1)" "$RC $OUT $(order)"

mk_loaded; : > "$W/persist/etc/camera-off"
OUT=$(sh "$S" start 2>&1)
ok "7. a restart with the camera off: its driver is covered again, and nothing else runs" \
	"$(grep -q '^mount --bind /dev/null' "$W/run/order" && ! grep -q '^rmmod' "$W/run/order" && echo 1)" "$(order)"

echo "$NP passed, $NF failed"; [ $NF = 0 ]
