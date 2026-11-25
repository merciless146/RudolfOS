#!/usr/bin/env python3
# 1.6.3 Part 8, item 2 (trans163_t.js): a recording of the screen (ffmpeg x11grab, 60 frames a second, grey, raw) read frame by
# frame inside a box (the window's place): which frames change, how much of the box each changes, and the longest run of frames
# that do not change while it moves.   python3 trans163_an.py FILE W H X Y BW BH  ->  JSON
import json, sys
import numpy as np
f, W, H, X, Y, BW, BH = sys.argv[1], *map(int, sys.argv[2:8])
a = np.memmap(f, dtype=np.uint8, mode='r'); n = a.size // (W * H); a = a[:n * W * H].reshape(n, H, W)
x0, y0, x1, y1 = max(0, X - 40), max(0, Y - 40), min(W, X + BW + 40), min(H, Y + BH + 50)
area = max(1, (x1 - x0) * (y1 - y0)); prev = None; ch = []
for i in range(n):
    b = a[i, y0:y1, x0:x1].astype(np.int16)
    if prev is not None: ch.append(float((np.abs(b - prev) > 10).sum()) / area)
    prev = b
moving = [i for i, c in enumerate(ch) if c > 0.0005]
if not moving: print(json.dumps({'frames': n, 'moving': 0})); sys.exit()
i0, i1 = moving[0], moving[-1]; seq = ch[i0:i1 + 1]
still, run = 0, 0
for c in seq:
    run = run + 1 if c <= 0.0005 else 0; still = max(still, run)
print(json.dumps({'frames': n, 'first': i0, 'last': i1, 'ms': round((i1 - i0 + 1) * 1000 / 60), 'changed': sum(1 for c in seq if c > 0.0005),
                  'still_run': still, 'biggest': round(max(seq), 3), 'big_frames': sum(1 for c in seq if c > 0.05), 'seq': [round(c, 3) for c in seq]}))
