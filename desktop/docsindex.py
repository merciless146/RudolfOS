"""1.5.7 (plan stage 2): Rudolf OS's documents as Blitzen's reference material, searched (not trained on).
docs/*.md and the CHANGELOG, split at their headings into sections of a few paragraphs (each CHANGELOG entry its own), each with where it comes from
and its headings. Written beside the desktop (blitzen-docs.json); the desktop searches it by words (BM25, blitzdiag57.js)
and adds the Help pages it has. Lines naming the release secrets are left out.   python3 docsindex.py OUT.json"""
import json, os, re, sys
here = os.path.dirname(os.path.abspath(__file__)); root = os.path.join(here, '..')
SECRET = re.compile(r'RPK_SIGNING_KEY|RPK_LIBRARY_KEY|RUDOLF_SB_KEY|\.pem\b|devlink|private key', re.I)
def clean(t):
    t = re.sub(r'`([^`]*)`', r'\1', t); t = re.sub(r'\*\*([^*]*)\*\*', r'\1', t); t = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', t)
    return re.sub(r'[ \t]+', ' ', t).strip()
def sections(name, text, group=120, most=200):
    """paragraphs and list items (with their indented lines) are the units; units under one heading are put together
    up to about `group` words (0: each unit alone, as the CHANGELOG's entries are); a unit over `most` words is split"""
    out, heads, units = [], [], []
    def flush():
        cur = []
        def put():
            if cur and sum(len(u) for u in cur) >= 6:
                out.append({'s': name, 'h': ' › '.join(heads), 't': ' '.join(w for u in cur for w in u)})
            cur.clear()
        for u in units:
            for i in range(0, len(u), most):
                part = u[i:i + most]
                if cur and sum(len(x) for x in cur) + len(part) > group: put()
                cur.append(part)
                if not group: put()
        put(); units.clear()
    unit = None
    for line in text.splitlines():
        m = re.match(r'^(#{1,4})\s+(.*)', line)
        if m:
            if unit: units.append(unit); unit = None
            flush(); lv = len(m.group(1)); del heads[lv - 1:]; heads.append(clean(m.group(2)))
            continue
        if SECRET.search(line): continue
        if not line.strip():
            if unit: units.append(unit); unit = None
            continue
        if re.match(r'^\s{0,1}([-*]|\d+\.)\s', line) or unit is None:
            if unit: units.append(unit)
            unit = clean(re.sub(r'^\s*[-*]\s+', '', line)).split()
        else: unit += clean(line).split()
    if unit: units.append(unit)
    flush(); return out
def build():
    docs = []
    for f in sorted(os.listdir(os.path.join(root, 'docs'))):
        if f.endswith('.md'): docs += sections('docs/' + f, open(os.path.join(root, 'docs', f), encoding='utf-8').read())
    docs += sections('CHANGELOG.md', open(os.path.join(root, 'CHANGELOG.md'), encoding='utf-8').read(), group=0)
    ver = open(os.path.join(root, 'iso', 'VERSION')).read().strip()
    for d in docs: d['h'] = d['h'].strip(' ›')
    return {'v': 1, 'version': ver, 'docs': docs}
if __name__ == '__main__':
    j = build(); open(sys.argv[1], 'w').write(json.dumps(j, ensure_ascii=False, separators=(',', ':')))
    print(len(j['docs']), 'sections')
