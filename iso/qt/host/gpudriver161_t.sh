#!/bin/sh
# 1.6.1 (docs/plan-1.6.1.md Part 19): /usr/lib/rudolf/gpudriver on a stand-in /sys (PCI cards), stand-in modules and a
# stand-in insmod (one that refuses a module "signed by another key" as the kernel does): the offer (no NVIDIA card,
# an older card, GeForce 16 and newer, two graphics chips), the extension missing or left out, a module that fails to load
# and the switch back (the note, the health line, the choice reset, nouveau uncovered), the trial (a start whose desktop
# never answered), Xorg failing, the open driver keeping NVIDIA's files from loading, Recovery mode and safe graphics
# untouched, and set's refusal for a card that is not offered. Nothing is mounted (GPU_NOMOUNT), no root needed.
#   sh gpudriver161_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); G=$HERE/../../sys/root/usr/lib/rudolf/gpudriver
T=$(mktemp -d /tmp/gpudrv161.XXXXXX); trap 'rm -rf $T' EXIT
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
has() { case "$1" in *"$2"*) echo 1;; *) echo 0;; esac; }
hasnt() { case "$1" in *"$2"*) echo 0;; *) echo 1;; esac; }
mkdir -p $T/bin; printf '#!/bin/sh\necho "$*" >> %s/insmod.log\ngrep -q BADSIG "$1" && { echo "insmod: cannot insert '"'"'$1'"'"': Key was rejected by service" >&2; exit 1; }\nexit 0\n' $T > $T/bin/insmod
printf '#!/bin/sh\necho "$*" >> %s/rmmod.log\n' $T > $T/bin/rmmod; chmod +x $T/bin/*
export PATH=$T/bin:$PATH GPU_NOMOUNT=1 GPU_SYS=$T/sys GPU_RUN=$T/run GPU_PERSIST=$T/persist GPU_EXT=$T/ext GPU_CMDLINE=$T/cmdline \
	GPU_LIST=$T/list GPU_MODROOT=$T/mods GPU_MODDIR=$T/mods/extra/nvidia
printf '# test\n10de:1f82\tNVIDIA GeForce GTX 1650\n10de:2684\tNVIDIA GeForce RTX 4090\n' > $T/list
card() { rm -rf $T/sys; mkdir -p $T/sys/bus/pci/devices; for c in "$@"; do set -- $(echo $c | tr , ' '); d=$T/sys/bus/pci/devices/$1; mkdir -p $d
	echo 0x$2 > $d/class; echo 0x$3 > $d/vendor; echo 0x$4 > $d/device; echo $5 > $d/boot_vga; done; }
reset() { rm -rf $T/run $T/persist $T/ext $T/mods $T/*.log; mkdir -p $T/run $T/persist $T/ext $T/mods/kernel/drivers/gpu/drm/nouveau $T/mods/extra/nvidia
	echo quiet > $T/cmdline; echo nouveau > $T/mods/kernel/drivers/gpu/drm/nouveau/nouveau.ko
	for m in nvidia nvidia-modeset nvidia-drm nvidia-uvm; do echo "signed $m" > $T/mods/extra/nvidia/$m.ko; done
	printf 'start\twith extensions\nnvidia-driver\t595.112.07\tin-use\t\n' > $T/ext/state; }
st() { sh $G status; }

# 1-3 the offer
reset; card 0000:00:02.0,030000,8086,46a8,1
S=$(st); ok "1. no NVIDIA card: no card in the status, not offered" $( [ "$(has "$S" '"card":null')$(has "$S" '"offered":false')$(has "$S" 'no NVIDIA graphics card')" = 111 ] && echo 1) "$S"
card 0000:01:00.0,030000,10de,1b80,1; S=$(st)
ok "2. a GeForce 10 card (not on the list): shown, not offered, and why" $( [ "$(has "$S" '"card":"10de:1b80"')$(has "$S" '"offered":false')$(has "$S" 'GeForce 16 and newer')" = 111 ] && echo 1) "$S"
R=$(sh $G set nvidia); ok "3. set nvidia refused for it; the choice stays open" $( [ "$(has "$R" '"ok":false')$( [ ! -e $T/persist/gpu-driver ] && echo 1)" = 11 ] && echo 1) "$R"
card 0000:01:00.0,030000,10de,2684,1; S=$(st)
ok "4. a GeForce RTX 4090: offered, its name from the list" $( [ "$(has "$S" '"offered":true')$(has "$S" 'GeForce RTX 4090')$(has "$S" '"choice":"open"')" = 111 ] && echo 1) "$S"
card 0000:00:02.0,030000,8086,46a8,1 0000:01:00.0,030200,10de,2684,0; S=$(st)
ok "5. two graphics chips, the NVIDIA one not the boot card: not offered (PRIME)" $( [ "$(has "$S" '"offered":false')$(has "$S" 'two graphics chips')" = 11 ] && echo 1) "$S"

# 6-8 the open driver (the default): NVIDIA's files kept from loading, nouveau left alone
reset; card 0000:01:00.0,030000,10de,2684,1; sh $G boot >/dev/null
C=$(cat $T/run/gpu-covered 2>/dev/null)
ok "6. open driver: every NVIDIA module covered, nouveau not, nothing loaded" $( [ "$(has "$C" extra/nvidia/nvidia.ko)$(has "$C" nvidia-drm.ko)$(hasnt "$C" nouveau)$( [ ! -e $T/insmod.log ] && echo 1)" = 1111 ] && echo 1) "$C"
ok "7. open driver: no NVIDIA Xorg configuration, gpudriver xorg names the usual one" $( [ "$( [ ! -e $T/run/xorg-nvidia.conf ] && echo 1)$(has "$(sh $G xorg)" /usr/lib/rudolf/xorg.conf)" = 11 ] && echo 1)
# 8-12 NVIDIA's driver chosen and working
sh $G set nvidia >/dev/null; : > $T/run/gpu-covered; sh $G boot > $T/boot.out
C=$(cat $T/run/gpu-covered); I=$(cat $T/insmod.log 2>/dev/null)
ok "8. nvidia chosen: nouveau covered, NVIDIA's modules loaded in order, nvidia-drm with modeset=1" $( [ "$(has "$C" nouveau.ko)$(hasnt "$C" extra/nvidia)$(has "$(echo $I | tr -d '\n')" "nvidia.ko $T/mods/extra/nvidia/nvidia-modeset.ko $T/mods/extra/nvidia/nvidia-drm.ko modeset=1 fbdev=1")" = 111 ] && echo 1) "$C / $I"
ok "9. in use: gpudriver xorg names NVIDIA's configuration (Driver nvidia)" $( [ "$(has "$(sh $G xorg)" xorg-nvidia.conf)$(has "$(cat $T/run/xorg-nvidia.conf)" 'Driver "nvidia"')" = 11 ] && echo 1)
S=$(st); ok "10. status: inUse nvidia, on trial until the desktop answers" $( [ "$(has "$S" '"inUse":"nvidia"')$( [ -e $T/persist/gpu-driver.trying ] && echo 1)" = 11 ] && echo 1) "$S"
sh $G good; ok "11. the desktop answered (good): the trial is over" $( [ ! -e $T/persist/gpu-driver.trying ] && echo 1)
sh $G boot >/dev/null; ok "12. the next start: NVIDIA's driver again, no switch back" $( [ "$(has "$(st)" '"inUse":"nvidia"')$( [ ! -e $T/persist/gpu-driver.fallback ] && echo 1)" = 11 ] && echo 1) "$(st)"
# 13-17 a module that does not load (signed by another key): back to the open driver
reset; card 0000:01:00.0,030000,10de,2684,1; echo nvidia > $T/persist/gpu-driver; echo "BADSIG nvidia" > $T/mods/extra/nvidia/nvidia.ko
sh $G boot > $T/boot.out; S=$(st); C=$(cat $T/run/gpu-covered)
ok "13. a module refused by the kernel: the choice back to open, a note with the kernel's reason" $( [ "$(has "$S" '"choice":"open"')$(has "$S" '"inUse":"open"')$(has "$S" 'Key was rejected')" = 111 ] && echo 1) "$S"
ok "14. nouveau uncovered again, NVIDIA's files covered, no NVIDIA Xorg configuration" $( [ "$(has "$C" 'uncover nouveau')$(has "$C" extra/nvidia/nvidia.ko)$( [ ! -e $T/run/xorg-nvidia.conf ] && echo 1)" = 111 ] && echo 1) "$C"
ok "15. the switch back is in the health log" $(has "$(cat $T/run/gpu-health 2>/dev/null)" "back on the open driver") "$(cat $T/run/gpu-health 2>/dev/null)"
S=$(sh $G ack); ok "16. ack: the note is gone (the desktop showed its notification)" $(hasnt "$S" fellBack) "$S"
sh $G boot >/dev/null; ok "17. the start after: open driver, no new note" $( [ "$(has "$(st)" '"inUse":"open"')$( [ ! -e $T/persist/gpu-driver.fallback ] && echo 1)" = 11 ] && echo 1)
# 18-20 the extension missing, left out; a card no longer there
reset; card 0000:01:00.0,030000,10de,2684,1; echo nvidia > $T/persist/gpu-driver; printf 'start\twith extensions\n' > $T/ext/state
sh $G boot >/dev/null; ok "18. the extension not installed: back to open, said so" $(has "$(st)" "extension is not installed") "$(st)"
reset; card 0000:01:00.0,030000,10de,2684,1; echo nvidia > $T/persist/gpu-driver; printf 'nvidia-driver\t595.112.07\tleft-out\tbuilt for another base\n' > $T/ext/state
sh $G boot >/dev/null; ok "19. the extension left out (another base): back to open, with sysext's reason" $(has "$(st)" "left out (built for another base)") "$(st)"
reset; card 0000:00:02.0,030000,8086,46a8,1; echo nvidia > $T/persist/gpu-driver
sh $G boot >/dev/null; ok "20. the NVIDIA card taken out: back to open" $(has "$(st)" "no NVIDIA graphics card") "$(st)"
# 21 the trial: the last start with NVIDIA's driver never reached the desktop
reset; card 0000:01:00.0,030000,10de,2684,1; echo nvidia > $T/persist/gpu-driver; date > $T/persist/gpu-driver.trying
sh $G boot >/dev/null; ok "21. the desktop never answered last time: back to open, nothing loaded" $( [ "$(has "$(st)" "did not start with it")$( [ ! -e $T/insmod.log ] && echo 1)" = 11 ] && echo 1) "$(st)"
# 22 Xorg failing with NVIDIA's driver
reset; card 0000:01:00.0,030000,10de,2684,1; echo nvidia > $T/persist/gpu-driver; sh $G boot >/dev/null
sh $G failed "Xorg did not start with NVIDIA's driver"; ok "22. Xorg failed: open from the next start, said why; xorg names the usual configuration" $( [ "$(has "$(st)" '"choice":"open"')$(has "$(st)" 'Xorg did not start')$(has "$(sh $G xorg)" /usr/lib/rudolf/xorg.conf)" = 111 ] && echo 1) "$(st)"
# 23-24 Recovery mode and safe graphics: nothing done at all
for o in rudolf.recovery=1 nomodeset; do reset; card 0000:01:00.0,030000,10de,2684,1; echo nvidia > $T/persist/gpu-driver; echo "quiet $o" > $T/cmdline
	sh $G boot >/dev/null; ok "23. $o: nothing covered or loaded, the choice kept for a normal start" $( [ "$( [ ! -e $T/run/gpu-covered ] && echo 1)$( [ ! -e $T/insmod.log ] && echo 1)$(has "$(st)" '"choice":"nvidia"')$( [ ! -e $T/persist/gpu-driver.fallback ] && echo 1)" = 1111 ] && echo 1) "$(st)"; done
# 25 the QEMU test's stand-in card: only with rudolf.debug=1
reset; card 0000:00:02.0,030000,1234,1111,1; echo "quiet rudolf.gputest=10de:2684" > $T/cmdline
ok "25. rudolf.gputest without rudolf.debug=1: ignored" $(has "$(st)" '"card":null') "$(st)"
echo "quiet rudolf.debug=1 rudolf.gputest=10de:2684" > $T/cmdline
ok "26. rudolf.gputest with rudolf.debug=1: the stand-in card is offered" $(has "$(st)" '"offered":true') "$(st)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
