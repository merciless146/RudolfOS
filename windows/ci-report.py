"""Turns Rust compiler errors in a build log into GitHub annotations (several errors per annotation), so a failed
build can be read from the run's summary page. Usage: python ci-report.py build.log"""
import re, sys
log = open(sys.argv[1], encoding='utf-8', errors='replace').read().splitlines()
blocks, cur = [], None
for l in log:
    if re.match(r'^(error|warning)(\[\w+\])?: ', l) and not l.startswith('warning: unused'):
        if cur: blocks.append(cur)
        cur = [l] if l.startswith('error') else None
    elif cur is not None:
        if l.strip() == '' and len(cur) > 3: blocks.append(cur); cur = None
        elif len(cur) < 14: cur.append(l)
if cur: blocks.append(cur)
blocks = [b for b in blocks if not b[0].startswith('error: could not compile')]
if not blocks:  # not a compile error: the end of the log
    blocks = [log[-40:]]
esc = lambda s: s.replace('%', '%25').replace('\r', '').replace('\n', '%0A')
for i in range(0, min(len(blocks), 60), 6):
    print('::error title=Build errors %d-%d of %d::%s' % (i + 1, min(i + 6, len(blocks)), len(blocks), esc('\n\n'.join('\n'.join(b) for b in blocks[i:i + 6]))[:60000]))
