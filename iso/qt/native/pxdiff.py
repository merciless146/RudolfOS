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
# PXDIFF_AA=1 (1.6.2 Part 39, ui_t.js: "text anti-aliasing aside"): a differing pixel that is an anti-aliased one in either picture (an
# edge of writing or of a curve: its colour between its darkest and brightest neighbours, with few neighbours of its own colour, the
# test pixelmatch uses) does not count
if os.environ.get('PXDIFF_AA') == '1' and m.getbbox():
    pa, pb, pm = ca.load(), cb.load(), m.load()
    Y = lambda c: .299 * c[0] + .587 * c[1] + .114 * c[2]
    def sib(P, x, y):
        c = P[x, y]; k = 0
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if (dx or dy) and 0 <= x + dx < w and 0 <= y + dy < h and P[x + dx, y + dy] == c: k += 1
        return k
    def aa(P, Q, x, y):
        y0 = Y(P[x, y]); zero = 0; lo = hi = 0; lp = hp = None
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if not (dx or dy) or not (0 <= x + dx < w and 0 <= y + dy < h): continue
                d = Y(P[x + dx, y + dy]) - y0
                if d == 0:
                    zero += 1
                    if zero > 2: return False
                elif d < lo: lo, lp = d, (x + dx, y + dy)
                elif d > hi: hi, hp = d, (x + dx, y + dy)
        if not lp or not hp: return False
        return (sib(P, *lp) >= 3 and sib(Q, *lp) >= 3) or (sib(P, *hp) >= 3 and sib(Q, *hp) >= 3)
    for y in range(h):
        for x in range(w):
            if pm[x, y] == 255 and (aa(pa, pb, x, y) or aa(pb, pa, x, y)): pm[x, y] = 0
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
