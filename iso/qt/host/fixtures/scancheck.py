#!/usr/bin/env python3
# 1.6.2 (docs/plan-1.6.2.md Part 34): checks a scanned page against the stand-in scanner's picture (fixtures/esclsrv.py):
# its size for the resolution and area, and its colours at the four quarters' middles and the black square's (where
# the area has them). PNG and JPEG through PIL; a PDF's page through poppler's pdftoppm at the page's resolution.
#   scancheck.py FILE [page=N] [mode=color|grey|bw] [dpi=D] [area=L,T,W,H] [pdfpage=K] [tol=T]
# Prints "ok ..." or "bad ..." (and exits 1).
import sys, subprocess, tempfile, os
from PIL import Image
W_IN, H_IN = 8.5, 11.69
a = dict(x.split('=', 1) for x in sys.argv[2:]); f = sys.argv[1]
page = int(a.get('page', 1)); mode = a.get('mode', 'color'); dpi = int(a.get('dpi', 75)); tol = int(a.get('tol', 0))
L, T, AW, AH = [float(v) / 25.4 for v in a.get('area', f'0,0,{W_IN * 25.4},{H_IN * 25.4}').split(',')]
AW = min(AW, W_IN - L); AH = min(AH, H_IN - T)
def pixel(fx, fy):
    if 1.0 <= fx < 1.25 and 1.0 <= fy < 1.25: return (0, 0, 0)
    l, t = fx < W_IN / 2, fy < H_IN / 2
    if t and l: return (220, 30, 30)
    if t: return (30, 180, 60)
    if l: return (40, 60, 200)
    g = max(0, 255 - 60 * page); return (g, g, g)
def want(fx, fy):
    p = pixel(fx, fy); g = (p[0] * 299 + p[1] * 587 + p[2] * 114 + 500) // 1000
    return p if mode == 'color' else (g,) if mode == 'grey' else ((255,) if g >= 128 else (0,))
if f.endswith('.pdf'):
    d = tempfile.mkdtemp(); k = a.get('pdfpage', '1')
    subprocess.run(['pdftoppm', '-r', str(dpi), '-f', k, '-l', k, '-png', f, d + '/p'], check=True)
    f = d + '/' + sorted(os.listdir(d))[0]; tol = max(tol, 3)
im = Image.open(f); im.load()
if mode == 'color': im = im.convert('RGB')
else: im = im.convert('L')
w, h = im.size; ew, eh = round(AW * dpi), round(AH * dpi); bad = []
if abs(w - ew) > 2 or abs(h - eh) > 2: bad.append(f'size {w}x{h}, not {ew}x{eh}')
pts = [(W_IN / 4, H_IN / 4), (W_IN * 3 / 4, H_IN / 4), (W_IN / 4, H_IN * 3 / 4), (W_IN * 3 / 4, H_IN * 3 / 4), (1.125, 1.125)]
seen = 0
for fx, fy in pts:
    if not (L + 2 / dpi <= fx < L + AW - 2 / dpi and T + 2 / dpi <= fy < T + AH - 2 / dpi): continue
    x, y = int((fx - L) * dpi), int((fy - T) * dpi)
    if not (0 <= x < w and 0 <= y < h): continue
    v = im.getpixel((x, y)); v = v if isinstance(v, tuple) else (v,); e = want(fx, fy); seen += 1
    if any(abs(p - q) > tol for p, q in zip(v, e)): bad.append(f'({fx:.2f},{fy:.2f}) in is {v}, not {e}')
if not seen: bad.append('no point of the picture in the area')
print(('bad ' if bad else 'ok ') + f'{os.path.basename(sys.argv[1])} {w}x{h} {mode} {seen} points' + ('; ' + '; '.join(bad) if bad else ''))
sys.exit(1 if bad else 0)
