# 1.6.0 (plan Part 12, "The rule"): how far apart two drawings of one part are.   python3 pxdiff.py A.png B.png X,Y,W,H X2,Y2,W2,H2 TOL OUT
# The part is cut out of each picture (A at X,Y and B at X2,Y2, the smaller of the two sizes); a pixel differs when
# any of red, green or blue is more than TOL apart. Prints {"pct": percent of the part's pixels that differ, ...} and
# writes OUT.std.png, OUT.nat.png (the two cut-outs) and OUT.diff.png (the differing pixels in red over a faint copy).
# SHIFT (optional, 1.6.0 item 8): a pixel of A matched by one of B up to SHIFT pixels away does not differ (an app's page
# drawn by WebKit against Chromium: the engines round half pixels of line boxes differently).
import json, os, sys
from PIL import Image, ImageChops
a, b, ra, rb, tol, out = sys.argv[1], sys.argv[2], [int(v) for v in sys.argv[3].split(',')], [int(v) for v in sys.argv[4].split(',')], int(sys.argv[5]), sys.argv[6]
SH = int(sys.argv[7]) if len(sys.argv) > 7 else 0
A = Image.open(a).convert('RGB'); B = Image.open(b).convert('RGB')
w, h = min(ra[2], rb[2]), min(ra[3], rb[3])
def cut(I, r):
    x, y = max(0, r[0]), max(0, r[1]); return I.crop((x, y, x + w, y + h))
ca, cb = cut(A, ra), cut(B, rb)
def mask(x, y):
    d = ImageChops.difference(ca, y); r, g, bl = d.split()
    return ImageChops.lighter(ImageChops.lighter(r, g), bl).point(lambda v: 255 if v > tol else 0)
m = mask(ca, cb)
for dx in range(-SH, SH + 1):
    for dy in range(-SH, SH + 1):
        if dx or dy: m = ImageChops.darker(m, mask(ca, ImageChops.offset(cb, dx, dy)))
n = m.histogram()[255]; tot = w * h or 1
ca.save(out + '.std.png'); cb.save(out + '.nat.png')
faint = Image.blend(ca.convert('L').convert('RGB'), Image.new('RGB', ca.size, (255, 255, 255)), .7)
faint.paste(Image.new('RGB', ca.size, (255, 0, 0)), mask=m); faint.save(out + '.diff.png')
# PXDIFF_MAP=1 (monbeh_t.js): also where they differ, "box" (the box round the differing pixels) and "map" (a coarse map of them,
# a character for each cell of 16x8 pixels: '.' none, '1'-'9' a tenth of the cell's pixels or more, '#' all), for the test to print
# into its log: on GitHub's runner the pictures (the artifact) cannot always be fetched
R = {'pct': round(100.0 * n / tot, 3), 'px': n, 'w': w, 'h': h}
if os.environ.get('PXDIFF_MAP') == '1':
    R['box'] = list(m.getbbox() or []) or None; R['map'] = []
    for y0 in range(0, h, 8):
        r = ''
        for x0 in range(0, w, 16):
            c = m.crop((x0, y0, min(w, x0 + 16), min(h, y0 + 8))); k = c.histogram()[255]; t = c.size[0] * c.size[1] or 1
            r += '.' if not k else ('#' if k == t else str(min(9, max(1, 10 * k // t))))
        R['map'].append(r)
print(json.dumps(R))
