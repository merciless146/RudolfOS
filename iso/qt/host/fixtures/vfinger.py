#!/usr/bin/env python3
# 1.6.3 (docs/plan-1.6.3.md Part 2): made-up fingerprints for libfprint's virtual reader ("virtual_image": the
# FP_VIRTUAL_IMAGE socket), never anyone's real finger. Each SEED draws a different pattern: ridges about 9 pixels
# apart round a whorl, broken by phase dislocations at random places (each one a ridge ending or a fork, the minutiae
# NBIS finds), over the whole picture (an outline would give every seed the same ridge endings along it). The same seed always gives the same picture.
#   vfinger.py SOCKET SEED [TIMES] [GAP_S]   send the picture TIMES (1) times, GAP_S (0.6) seconds apart
#   vfinger.py --pgm SEED FILE               write it as a PGM (to look at)
# The virtual reader's wire format (libfprint's virtual-image.c): two native ints, width and height, then the grey
# bytes row by row (0 dark ridge, 255 paper).
import math, os, random, socket, struct, sys, time
W, H = 256, 340
def picture(seed):
    rnd = random.Random(int(seed) * 7919 + 13)
    # the pattern's core and its kind (a whorl, a loop or an arch: how much the ridges curl round it), its ridge spacing
    cx, cy = rnd.uniform(0.25, 0.75) * W, rnd.uniform(0.25, 0.7) * H
    curl, sq, th = rnd.uniform(0.2, 1.0), rnd.uniform(0.8, 1.8), rnd.uniform(0, math.pi)
    k = 2 * math.pi / rnd.uniform(8.0, 10.5)
    dis = [(rnd.uniform(0, W), rnd.uniform(0, H), rnd.choice((-1, 1))) for _ in range(rnd.randint(28, 40))]
    wa, wf, wp = rnd.uniform(3, 10), rnd.uniform(0.01, 0.035), rnd.uniform(0, 6.3)
    c, s = math.cos(th), math.sin(th)
    px = bytearray(W * H)
    for y in range(H):
        for x in range(W):
            xx = x + wa * math.sin(wf * y + wp); yy = y + wa * math.cos(wf * x + wp)
            u, v = (xx - cx) * c + (yy - cy) * s, (-(xx - cx) * s + (yy - cy) * c) / sq
            ph = k * (curl * math.hypot(u, v) + (1 - curl) * v)
            for (dx, dy, sg) in dis: ph += sg * math.atan2(yy - dy, xx - dx)
            px[y * W + x] = max(0, min(255, int(128 + 120 * math.cos(ph))))
    return bytes(px)
def main():
    if sys.argv[1] == '--pgm':
        open(sys.argv[3], 'wb').write(b'P5\n%d %d\n255\n' % (W, H) + picture(sys.argv[2])); return
    sock, seed = sys.argv[1], sys.argv[2]
    times = int(sys.argv[3]) if len(sys.argv) > 3 else 1; gap = float(sys.argv[4]) if len(sys.argv) > 4 else 0.6
    img = picture(seed)
    for i in range(times):
        s = socket.socket(socket.AF_UNIX)
        for _ in range(100):
            try: s.connect(sock); break
            except OSError: time.sleep(0.1)
        s.sendall(struct.pack('=ii', W, H) + img)
        time.sleep(gap); s.close()
main()
