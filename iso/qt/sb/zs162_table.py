#!/usr/bin/env python3
"""1.6.2 (docs/plan-1.6.2.md Part 31, item 2): the measurements' tables (markdown) and the choice they make.
  python3 zs162_table.py DIR > zs162-table.md
DIR holds every job's zs162-DISK-ENGINE.json (zs162_t.py) and each variant's info.json (zs162_layers.sh, in a folder named
after it). The choice (the plan's rule): of the zstd variants at most about 20 % larger than xz (-b 1M, today's) overall,
the one whose start-ups are fastest overall (the mean, over every disk and engine, of its median time to "the desktop was
ready" against xz's); then every disk and engine where it is slower than xz is listed: if there is one, the images are not
switched and the numbers go to the owner."""
import json, os, sys, glob
D = sys.argv[1]; BASE = 'xz-1m'
med = lambda x: sorted(x)[len(x) // 2] if x else None
info = {}
for f in glob.glob(f'{D}/**/info.json', recursive=True):
    j = json.load(open(f)); info[j['spec']] = j
runs = []
for f in glob.glob(f'{D}/**/zs162-*.json', recursive=True):
    runs += [json.loads(l) for l in open(f) if l.strip()]
runs = [r for r in runs if 'error' not in r]
order = lambda v: (['xz', 'zstd', 'lz4'].index(v.split('-')[0].rstrip('0123456789')) if v.split('-')[0].rstrip('0123456789') in ('xz', 'zstd', 'lz4') else 9, v.split('-')[0], v.split('-')[-1] != '1m')
VARS = sorted({r['variant'] for r in runs} | set(info), key=order)
DISKS = [d for d in ('nvme', 'emmc', 'usb') if any(r['disk'] == d for r in runs)]
ENGS = [e for e in ('chromium', 'native', 'lite') if any(r['engine'] == e for r in runs)]
CELLS = [(d, e) for d in DISKS for e in ENGS]
def m(v, d, e, k):
    return med([r[k] for r in runs if r['variant'] == v and r['disk'] == d and r['engine'] == e and isinstance(r.get(k), (int, float))])
def n(v, d, e): return sum(1 for r in runs if r['variant'] == v and r['disk'] == d and r['engine'] == e and isinstance(r.get('ready_s'), (int, float)))
mb = lambda b: f'{b / 1048576:.0f}' if b else '-'
P = print
P('### Sizes (MB; the squashfs image, without its hash tree), made twice from the same root (SOURCE_DATE_EPOCH)\n')
P('| variant | base | chromium | desktop | modules | total | against xz -b 1M | the two builds the same | xz -b 1M: the release\'s own bytes | made in (s) |')
P('|---|---|---|---|---|---|---|---|---|---|')
tot = {v: sum(x['bytes'] for x in info[v]['layers'].values()) for v in info}
for v in VARS:
    if v not in info: continue
    L = info[v]['layers']
    rep = all(x['reproducible'] for x in L.values()); same = [x['same_as_release'] for x in L.values() if x['same_as_release'] is not None]
    P(f"| {v} | {mb(L['base']['bytes'])} | {mb(L['chromium']['bytes'])} | {mb(L['desktop']['bytes'])} | {mb(L['modules']['bytes'])} | **{mb(tot[v])}** | "
      f"{(tot[v] / tot[BASE] - 1) * 100:+.1f} % | {'yes' if rep else '**no**'} | {('yes' if all(same) else 'no') if same else ''} | {sum(x['make_s'] for x in L.values()):.0f} |" if BASE in tot else f"| {v} | | | | | {mb(tot[v])} | | {'yes' if rep else 'no'} | | |")
def table(title, k, unit, fmt='{:.1f}'):
    P(f'\n### {title}\n'); P('| variant | ' + ' | '.join(f'{d} {e}' for d, e in CELLS) + ' |'); P('|---|' + '---|' * len(CELLS))
    for v in VARS:
        row = []
        for d, e in CELLS:
            x, b = m(v, d, e, k), m(BASE, d, e, k)
            row.append('-' if x is None else fmt.format(x) + ('' if v == BASE or b is None else f' ({x - b:+.1f})'))
        P(f'| {v} | ' + ' | '.join(row) + ' |')
    P(f'\n({unit}; the median of each disk and engine\'s starts, and in brackets the difference from {BASE})')
table('Start-up: the desktop was ready (the health log), s', 'ready_s', 's')
table("Chromium's first start, cold (s)", 'chromium_s', 's')
table("Oracle's first start, cold (Rudolf's Chromium with Oracle's own flags; s)", 'oracle_s', 's')
table('Memory at idle (MiB)', 'mem_idle_mib', 'MiB', '{:.0f}')
P('\n### Decompression: every file of the four layers read once, cold (the first counted start of each)\n')
P('| variant | ' + ' | '.join(f'{d}: read s / CPU s' for d in DISKS) + ' |'); P('|---|' + '---|' * len(DISKS))
for v in VARS:
    row = []
    for d in DISKS:
        dx = [r['decomp'] for r in runs if r['variant'] == v and r['disk'] == d and r.get('decomp')]
        rd = med([sum(x['read_s'] for x in z.values()) for z in dx]); cp = med([sum(x['sys_cpu_s'] for x in z.values()) for z in dx])
        row.append('-' if rd is None else f'{rd:.1f} / {cp:.1f}')
    P(f'| {v} | ' + ' | '.join(row) + ' |')
P('\n(the median over the engines\' jobs; CPU: the processors\' system, irq and softirq time while reading, both processors)')
starts = sorted({n(v, d, e) for v in VARS for d, e in CELLS})
P(f'\nStarts counted per variant, disk and engine: {", ".join(map(str, starts))}.')
# ---- the choice
if BASE not in tot or not CELLS: P('\n(no choice: xz -b 1M or the start-ups are missing)'); sys.exit(0)
def score(v):
    q = [m(v, d, e, 'ready_s') / m(BASE, d, e, 'ready_s') for d, e in CELLS if m(v, d, e, 'ready_s') and m(BASE, d, e, 'ready_s')]
    return sum(q) / len(q) if len(q) == len(CELLS) else None
cand = [v for v in VARS if v.startswith('zstd') and v in tot and tot[v] <= tot[BASE] * 1.2 and score(v)]
P('\n### The choice\n')
for v in sorted(cand, key=score): P(f'- {v}: start-ups {(score(v) - 1) * 100:+.1f} % against {BASE} on average, {(tot[v] / tot[BASE] - 1) * 100:+.1f} % in size')
if not cand: P('No zstd variant within 20 % of xz\'s size has every start measured.'); sys.exit(0)
best = min(cand, key=score)
slow = [(d, e, m(best, d, e, 'ready_s'), m(BASE, d, e, 'ready_s')) for d, e in CELLS if m(best, d, e, 'ready_s') > m(BASE, d, e, 'ready_s')]
oslow = [(d, e, m(best, d, e, 'oracle_s'), m(BASE, d, e, 'oracle_s')) for d, e in CELLS if (m(best, d, e, 'oracle_s') or 0) > (m(BASE, d, e, 'oracle_s') or 1e9)]
P(f'\n**Chosen: {best}**, {(score(best) - 1) * 100:+.1f} % start-up time against {BASE} on average, {(tot[best] / tot[BASE] - 1) * 100:+.1f} % in size.')
if slow: P('\n**Slower than xz at start-up on:** ' + '; '.join(f'{d} {e} ({a:.1f} s against {b:.1f} s)' for d, e, a, b in slow) + ' — by the owner\'s rule, not switched: for the owner to decide.')
else: P('\nNot slower than xz at start-up on any disk or engine.')
if oslow: P('\nOracle\'s first start slower than xz on: ' + '; '.join(f'{d} {e} ({a:.1f} s against {b:.1f} s)' for d, e, a, b in oslow) + '.')
