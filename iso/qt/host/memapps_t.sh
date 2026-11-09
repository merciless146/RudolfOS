#!/bin/sh
# 1.5.6: when memory runs out and no web page can be ended, the memory guard quits the largest Flatpak app with no
# window open (found on the Vivobook: Discord in the tray, a game frozen, the desktop restarted). Stand-in apps: a
# "bwrap" process per sandbox holding memory, their folders in a stand-in /run/user, and a stand-in window list.
#   sh memapps_t.sh   (needs gcc and python3)
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; T=$(mktemp -d)
gcc -O2 -o $T/mg $SYS/rudolf-memguard.c || { echo "FAIL build"; exit 1; }
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
PIDS=""; trap 'for p in $PIDS; do pkill -P $p; kill $p; done 2>/dev/null; rm -rf $T' EXIT
# a sandbox: a parent holding MiB of memory, and a child of it holding more (its app's processes)
sandbox() {   # id MiB-parent MiB-child -> its folder in $T/run/1000/.flatpak/<n>
	python3 -c "import os,sys,time;a=bytearray(int(sys.argv[1])<<20)
if os.fork()==0:
  b=bytearray(int(sys.argv[2])<<20);time.sleep(600)
time.sleep(600)" $2 $3 & P=$!; PIDS="$PIDS $P"
	d=$T/run/1000/.flatpak/$P; mkdir -p $d; echo $P > $d/pid; printf '[Application]\nname=%s\nruntime=runtime/x\n' $1 > $d/info
}
sandbox com.discordapp.Discord 20 30                 # 50 MiB, in the tray (no window)
sandbox com.valvesoftware.Steam 10 20                # Steam: 30 MiB in its first sandbox...
sandbox com.valvesoftware.Steam 40 0                 # ...and 40 in a sandbox of its own (its interface, a game)
sandbox org.example.Tiny 5 0                         # small: left alone
sleep 1.5
run() { MG_RUNUSER=$T/run MG_WINDOWS=$T/windows MG_MIN_MIB=40 $T/mg --once; }
printf '' > $T/windows
O=$(run)
# (each stand-in process also holds about 10 MiB of Python, and a child shares its parent's memory: Discord about
# 20 + 50 + 2 x 10, Steam 10 + 30 + 40 + 3 x 10)
ok "Discord's memory counts all its processes (about 85 MiB)" "$(echo "$O" | grep -Eq '^app com.discordapp.Discord: ([7-9][0-9]) MiB, 1 sandbox' && echo 1)" "$O"
ok "Steam's two sandboxes count as one app (about 150 MiB)" "$(echo "$O" | grep -Eq '^app com.valvesoftware.Steam: (1[2-7][0-9]) MiB, 2 sandbox' && echo 1)" "$O"
ok "no window open at all: the largest app (Steam) would be quit" "$(echo "$O" | grep -qx 'would quit app: Steam ([0-9]* MiB)' && echo 1)" "$O"
printf '3a00001\t0\tsteam_app_945360\tsteam_app_945360\t1\tAmong Us\n' > $T/windows
O=$(run)
ok "a Steam game in front (steam_app_945360): Steam is left alone, Discord (in the tray) would be quit" "$(echo "$O" | grep -qx 'would quit app: Discord ([0-9]* MiB)' && echo 1)" "$O"
printf '3a00001\t0\tsteam_app_945360\tsteam_app_945360\t1\tAmong Us\n1000005\t0\tdiscord\tdiscord\t0\tFriends - Discord\n' > $T/windows
O=$(run)
ok "Discord's window open too: no app would be quit (Tiny is under the size)" "$(echo "$O" | grep -q 'would quit app' || echo 1)" "$O"
printf '3800001\t0\tSteam\tsteam\t0\tSteam\n1000005\t0\tDiscordCanary\tx\t0\tx\n' > $T/windows
O=$(run)
ok "a class that only starts with the name (DiscordCanary) is not Discord's window" "$(echo "$O" | grep -qx 'would quit app: Discord ([0-9]* MiB)' && echo 1)" "$O"
echo "$((N-F))/$N passed"; [ $F = 0 ]
