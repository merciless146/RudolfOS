# Rudolf OS 1.6.0 (plan-1.6.0.md, "Cellular and ARM64", stage A): the stand-in ModemManager for host tests, sourced:
#   . mmstub.sh; mmstub_up DIR [INIT-FILE] || exit 0    a private bus (its own dbus-daemon, in DIR) with ../mmstub.c on
#                                                       it; DBUS_SYSTEM_BUS_ADDRESS set to it, so mmcli and Rudolf's
#                                                       scripts talk to the stand-in, never to the computer's own bus
#   mmstub_cmd 'set 0 signal=40'                        a command to it (mmstub.c's list); waits until it is done
#   mmstub_log                                          what Rudolf asked of it (one call per line)
#   mmstub_down                                         stops what mmstub_up started (only those two processes)
# Needs dbus-daemon, gcc, libmm-glib's headers (libmm-glib-dev) and mmcli (modemmanager): "SKIP" when missing.
MMSTUB_SRC=$(cd "$(dirname "${MMSTUB_HERE:-$0}")/.." 2>/dev/null && pwd)/mmstub.c
mmstub_up() {
	MMS_D=$1; mkdir -p "$MMS_D"
	command -v dbus-daemon >/dev/null && command -v mmcli >/dev/null && pkg-config --exists mm-glib 2>/dev/null || { echo "SKIP: needs dbus-daemon, mmcli and libmm-glib-dev"; return 1; }
	[ -x "$MMS_D/mmstub" ] || gcc -O2 -w -o "$MMS_D/mmstub" "$MMSTUB_SRC" $(pkg-config --cflags --libs mm-glib gio-unix-2.0) || { echo "FAIL build mmstub"; return 1; }
	cat > "$MMS_D/bus.conf" <<C
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig><type>system</type><listen>unix:path=$MMS_D/bus</listen><auth>EXTERNAL</auth>
<policy context="default"><allow user="*"/><allow own="*"/><allow send_destination="*" eavesdrop="true"/><allow receive_sender="*"/><allow eavesdrop="true"/></policy></busconfig>
C
	dbus-daemon --config-file="$MMS_D/bus.conf" --fork --print-pid=3 3>"$MMS_D/bus.pid" >/dev/null 2>"$MMS_D/bus.err" || { echo "FAIL dbus-daemon: $(cat "$MMS_D/bus.err")"; return 1; }
	export DBUS_SYSTEM_BUS_ADDRESS=unix:path=$MMS_D/bus
	rm -f "$MMS_D/ctl"; mkfifo "$MMS_D/ctl"; : > "$MMS_D/calls"
	"$MMS_D/mmstub" --ctl "$MMS_D/ctl" --log "$MMS_D/calls" ${2:+--init "$2"} > "$MMS_D/out" 2>&1 & MMS_P=$!
	i=0; while [ $i -lt 80 ] && ! grep -q ready "$MMS_D/out" 2>/dev/null; do timeout 0.1 tail -f /dev/null; i=$((i+1)); done
	grep -q ready "$MMS_D/out" || { echo "FAIL mmstub did not start: $(cat "$MMS_D/out")"; mmstub_down; return 1; }
}
mmstub_cmd() { printf '%s\n' "$1" > "$MMS_D/ctl"; timeout 0.3 tail -f /dev/null; }
mmstub_log() { cat "$MMS_D/calls"; }
mmstub_down() { [ -n "$MMS_P" ] && kill $MMS_P 2>/dev/null; [ -s "$MMS_D/bus.pid" ] && kill "$(cat "$MMS_D/bus.pid")" 2>/dev/null; MMS_P=; rm -f "$MMS_D/bus.pid"; }
