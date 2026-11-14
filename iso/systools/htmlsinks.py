#!/usr/bin/env python3
"""1.5.9 (plan 15.2): every place the desktop page turns data into HTML, counted per file and checked against the list
of those already reviewed (htmlsinks.json), so a new one cannot come in unseen. Run before each pull request, as
deadcheck.py:
    python3 iso/systools/htmlsinks.py            check: fails when a file has more than its reviewed number
    python3 iso/systools/htmlsinks.py --list F   each one in file F (desktop/NAME.js), to review
    python3 iso/systools/htmlsinks.py --update   after reviewing: write the numbers as they are now
A sink is innerHTML, outerHTML, insertAdjacentHTML, srcdoc, document.write or execCommand('insertHTML') given anything
but a plain string: a template with ${...} or a string joined with +. A reviewed one either escapes what goes in
(esc, esc2, lxEsc, textContent afterwards), passes it through safeHtml (desktop/sanitize59.js), or shows it in a
frame with sandbox and no allow-same-origin.
Fewer than the reviewed number is fine (the check says so; --update keeps the list tight)."""
import json, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
DESK = os.path.join(ROOT, 'desktop'); LIST = os.path.join(HERE, 'htmlsinks.json')
SINK = re.compile(r"(\.innerHTML\s*\+?=(?!=)|\.outerHTML\s*=(?!=)|insertAdjacentHTML\s*\(|\.srcdoc\s*=(?!=)|document\.write(?:ln)?\s*\(|execCommand\(\s*['\"]insertHTML['\"])")
def sinks(text):
    out = []
    for m in SINK.finditer(text):
        tail = text[m.end():m.end() + 400]
        # the value: up to the end of the statement at the same depth (roughly: the first ; or newline outside brackets)
        depth, val, q = 0, '', None
        for ch in tail:
            if q:
                val += ch
                if ch == q: q = None
                continue
            if ch in '`\'"': q = ch; val += ch; continue
            if ch in '([{': depth += 1
            elif ch in ')]}':
                if depth == 0: break
                depth -= 1
            elif ch in ';\n' and depth == 0: break
            val += ch
        v = val.strip()
        plain = re.fullmatch(r"""(['"])[^'"\\]*\1|``|`[^`$]*`""", v) is not None or v in ("''", '""', '')
        if not plain: out.append((text.count('\n', 0, m.start()) + 1, (m.group(0) + v)[:160]))
    return out
def files():
    for n in sorted(os.listdir(DESK)):
        if n.endswith('.js') or n == 'base_v24.html': yield n
def counts(): return {n: len(sinks(open(os.path.join(DESK, n), encoding='utf8').read())) for n in files()}
if __name__ == '__main__':
    a = sys.argv[1:]
    if a[:1] == ['--list']:
        n = os.path.basename(a[1]); [print(f'{n}:{l}: {t}') for l, t in sinks(open(os.path.join(DESK, n), encoding='utf8').read())]; sys.exit(0)
    now = {k: v for k, v in counts().items() if v}
    if a[:1] == ['--update']:
        json.dump(now, open(LIST, 'w'), indent=0, sort_keys=True); print(f'htmlsinks: {sum(now.values())} reviewed in {len(now)} files written'); sys.exit(0)
    seen = json.load(open(LIST)) if os.path.exists(LIST) else {}
    bad = [(k, v, seen.get(k, 0)) for k, v in now.items() if v > seen.get(k, 0)]
    less = [k for k, v in seen.items() if now.get(k, 0) < v]
    for k, v, w in bad: print(f'htmlsinks: desktop/{k} has {v} places that make HTML from data, {w} reviewed: review the new ones (--list desktop/{k}), then --update')
    if less: print('htmlsinks: fewer than reviewed in ' + ', '.join(less) + ' (fine; --update to keep the list tight)')
    if not bad: print(f'htmlsinks: {sum(now.values())} places, all reviewed')
    sys.exit(1 if bad else 0)
