# 1.6.0 (plan Part 12, "The rule"; "Before stable" item 9): same_oracle_t.sh's pictures compared. For each case both engines
# drew (OUT/std-CASE.png and OUT/nat-CASE.png, with the parts' places in OUT/ENGINE-CASE.json), each part is cut out of both
# and compared with pxdiff.py: it fails when its size differs by more than a pixel, or over LIMIT % of its pixels differ by
# more than TOL in a colour. The steps' outcomes (OUT/ENGINE-did.txt) must be the same line for line.
#   python3 same_oracle.py OUT [CASE,...]   (SAME_LIMIT, SAME_TOL as same_t.sh)
import json, os, subprocess, sys
OUT = sys.argv[1]; ONLY = set(filter(None, (sys.argv[2] if len(sys.argv) > 2 else '').split(',')))
LIMIT = float(os.environ.get('SAME_LIMIT', 2)); TOL = int(os.environ.get('SAME_TOL', 32))
HERE = os.path.dirname(os.path.abspath(__file__))
CASES = [
    ('home', ['title', 'tabs', 'toolbar', 'address', 'bookmarks', 'page', 'status', 'win'], 'the title bar, the tabs, the toolbar, the address box, the bookmarks bar, the Home page'),
    ('site', ['tabs', 'toolbar', 'page', 'win'], 'a local test site in a real tab'),
    ('menu', ['menu'], "Oracle's menu (☰)"),
    ('download', ['page', 'win'], 'a download, in about:downloads'),
    ('dark', ['title', 'tabs', 'toolbar', 'bookmarks', 'page', 'status', 'win'], 'dark mode'),
    ('light', ['title', 'tabs', 'toolbar', 'bookmarks', 'page', 'status', 'win'], 'light mode'),
    ('accent', ['title', 'tabs', 'toolbar', 'win'], 'another accent colour (green)'),
]
n = f = 0
def ok(name, good, why=''):
    global n, f
    n += 1; f += 0 if good else 1
    print(('PASS ' if good else 'FAIL ') + name + ('' if good else ' - ' + why[:900]))
for c, parts, what in CASES:
    if ONLY and c not in ONLY: continue
    a, b = os.path.join(OUT, 'std-%s' % c), os.path.join(OUT, 'nat-%s' % c)
    if not (os.path.exists(a + '.png') and os.path.exists(b + '.png')):
        ok('%s: %s' % (c, what), False, 'not drawn by both engines'); continue
    ja, jb = json.load(open(a + '.json')), json.load(open(b + '.json')); res = []
    for p in parts:
        ra, rb = ja.get(p), jb.get(p)
        if not ra or not rb: res.append((p, False, 'Standard %s, Native %s' % (ra, rb))); continue
        if abs(ra[2] - rb[2]) > 1 or abs(ra[3] - rb[3]) > 1: res.append((p, False, 'size: Standard %dx%d, Native %dx%d' % (ra[2], ra[3], rb[2], rb[3]))); continue
        o = subprocess.run([sys.executable, os.path.join(HERE, 'pxdiff.py'), a + '.png', b + '.png', ','.join(map(str, ra)), ','.join(map(str, rb)), str(TOL), os.path.join(OUT, 'cmp-%s-%s' % (c, p))], capture_output=True, text=True)
        try: d = json.loads(o.stdout)
        except ValueError: res.append((p, False, o.stderr[-300:])); continue
        res.append((p, d['pct'] <= LIMIT, '%s %% differ (cmp-%s-%s.*.png)' % (d['pct'], c, p)))
    ok('%s: %s (%s)' % (c, what, ', '.join(p for p, _, _ in res)), all(g for _, g, _ in res), '; '.join('%s: %s' % (p, w) for p, g, w in res if not g))
# the title bar's words, case by case (Native's frame says what Standard's window says)
ta, tb = os.path.join(OUT, 'std-titles.txt'), os.path.join(OUT, 'nat-titles.txt')
if os.path.exists(ta) and os.path.exists(tb):
    A = dict(l.split(' ', 1) for l in open(ta).read().splitlines() if ' ' in l); B = dict(l.split(' ', 1) for l in open(tb).read().splitlines() if ' ' in l)
    xn = open(os.path.join(OUT, 'nat-xnames.txt')).read() if os.path.exists(os.path.join(OUT, 'nat-xnames.txt')) else ''
    for c, _, _ in CASES:
        if (ONLY and c not in ONLY) or c not in A: continue
        ok('%s: the title "%s"' % (c, A[c]), A.get(c) == B.get(c), 'Native: "%s" %s' % (B.get(c), ' '.join(l for l in xn.splitlines() if l.startswith(c + ' '))))
da, db = os.path.join(OUT, 'std-did.txt'), os.path.join(OUT, 'nat-did.txt')
if (not ONLY or 'works' in ONLY) and os.path.exists(da) and os.path.exists(db):
    la, lb = open(da).read().splitlines(), open(db).read().splitlines()
    for i in range(max(len(la), len(lb))):
        x, y = la[i] if i < len(la) else '(nothing)', lb[i] if i < len(lb) else '(nothing)'
        ok('works: ' + x, x == y, 'Native: ' + y)
print('%d/%d passed' % (n - f, n)); sys.exit(1 if f else 0)
