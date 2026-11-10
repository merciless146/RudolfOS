#!/bin/sh
# 1.5.6: cameras belong to the video group (1.5.7: group 1000) (mdev.conf), so the desktop (Chromium, as rudolf in group video) can open
# them. busybox mdev takes the first rule whose regular expression matches the whole device name.
HERE=$(cd "$(dirname "$0")" && pwd); C=$HERE/../../sys/root/etc/mdev.conf
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
first() {   # the first plain rule (no $ENV= / @ / - prefix tricks) that matches $1 as a whole: "pattern owner mode"
	grep -v '^#' "$C" | grep -v '^\$' | while read -r pat own mode rest; do
		p=${pat#-}; [ -z "$p" ] && continue
		if echo "$1" | grep -Eqx "$p"; then echo "$p $own $mode"; break; fi
	done
}
# 1.5.7: group 1000 (the desktop user's own, which Debian's rudolf is too), and placed in Debian when plugged in
for d in video0 video1 video10 media0 v4l-subdev3; do
	r=$(first $d); ok "$d: group 1000, 660" "$([ "${r#* }" = "0:1000 660" ] && echo 1)" "$r"
done
for d in video0 hidraw2 uinput; do
	p=$(first $d | cut -d' ' -f1); ok "$d: placed in Debian when it comes or goes (rudolf-debian devices)" "$(grep -v '^#' "$C" | grep -F "$p " | grep -q '\*/usr/bin/rudolf-debian devices$' && echo 1)" "$p"
done
r=$(first videox); ok "not a camera name (videox): no camera rule" "$([ -z "$r" ] && echo 1)" "$r"
echo "$((N-F))/$N passed"; [ $F = 0 ]
