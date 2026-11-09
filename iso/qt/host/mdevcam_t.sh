#!/bin/sh
# 1.5.6: cameras belong to the video group (mdev.conf), so the desktop (Chromium, as rudolf in group video) can open
# them. busybox mdev takes the first rule whose regular expression matches the whole device name.
HERE=$(cd "$(dirname "$0")" && pwd); C=$HERE/../../sys/root/etc/mdev.conf
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $3"; F=$((F+1)); fi; }
first() {   # the first plain rule (no $ENV= / @ / - prefix tricks) that matches $1 as a whole: "pattern owner mode"
	grep -v '^#' "$C" | grep -v '^\$' | while read -r pat own mode rest; do
		p=${pat#-}; [ -z "$p" ] && continue
		if echo "$1" | grep -Eqx "$p"; then echo "$p $own $mode"; break; fi
	done
}
for d in video0 video1 video10 media0 v4l-subdev3; do
	r=$(first $d); ok "$d: group video (44), 660" "$([ "${r#* }" = "0:44 660" ] && echo 1)" "$r"
done
r=$(first videox); ok "not a camera name (videox): no camera rule" "$([ -z "$r" ] && echo 1)" "$r"
echo "$((N-F))/$N passed"; [ $F = 0 ]
