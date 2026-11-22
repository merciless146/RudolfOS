#!/usr/bin/env python3
"""noremote.py: no script from outside Rudolf loads into the desktop's page or an app window (1.6.2, docs/plan-1.6.2.md Part 14).
The desktop's page holds rudolfd's token, so a script another site serves would have the whole system API. Run by deadcheck.py
(and so by ci_release.sh) on the sources (desktop/) and on the built pages (www/index.html, www/a/*.html and the lazy
groups www/a/*.js). Exits 1 and names what it found:
  - a <script src> with an address of another site (http:, https: or //host);
  - import(), importScripts() or new Worker() with such an address;
  - a script element given such an address (s.src='https://...');
  - a well-known script host (cdnjs, jsDelivr, unpkg, ...) named anywhere, but on the lines listed in ALLOW;
  - a built page without its Content-Security-Policy, or with one whose script-src lets another site's scripts in.
www/lib/ruffle-frame.html is not checked as a page: it is always opened in a sandbox (no origin: no token, no storage, no
access to the page that opened it) and loads only the addresses it is given (rufFrame, base_v24.html).
GPL-2.0-or-later."""
import glob, os, re, sys

ISO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOP = os.path.dirname(ISO)
HOSTS = r'(?:cdnjs\.cloudflare\.com|cdn\.jsdelivr\.net|unpkg\.com|ajax\.googleapis\.com|code\.jquery\.com|cdn\.skypack\.dev|esm\.sh|jspm\.dev|ga\.jspm\.io|cdn\.tailwindcss\.com|stackpath\.bootstrapcdn\.com|cdn\.rawgit\.com|raw\.githubusercontent\.com)'
REMOTE = r'''(?:https?:)?//(?!127\.0\.0\.1[:/]|localhost[:/])[a-z0-9.-]+\.[a-z]{2,}'''
RULES = [
    ('a <script src> of another site', re.compile(r'''<script\b[^>]*\bsrc\s*=\s*\\?["']?\s*''' + REMOTE, re.I)),
    ('import() of another site', re.compile(r'''\bimport\s*\(\s*["'`]''' + REMOTE, re.I)),
    ('importScripts() of another site', re.compile(r'''\bimportScripts\s*\([^)]*["'`]''' + REMOTE, re.I)),
    ('a worker from another site', re.compile(r'''\bnew\s+(?:Shared)?Worker\s*\(\s*["'`]''' + REMOTE, re.I)),
    ('a script element given an address of another site', re.compile(r'''\.src\s*=\s*["'`]''' + REMOTE + r'''[^"'`]*\.m?js\b''', re.I)),
    ('a script host named', re.compile(r'''\b''' + HOSTS + r'''\b''', re.I)),
]
# the only lines that may name a script host: Ruffle's addresses, which go to its sandboxed frame and Oracle's tab extension, never to a script element here
ALLOW = [re.compile(r"^const RUFURLS=\['https://unpkg\.com/@ruffle-rs/ruffle','https://cdn\.jsdelivr\.net/npm/@ruffle-rs/ruffle'\];$"),
         re.compile(r"Default: unpkg / jsDelivr \(@ruffle-rs/ruffle\)"), re.compile(r"RUNTIME=\$\{id=='ruffle'\?\(MMC\.set\.rufUrl\|\|'CDN \(unpkg / jsDelivr, @ruffle-rs/ruffle\)'\)")]
CSP = re.compile(r'''<meta http-equiv="Content-Security-Policy" content="([^"]*)">''')

def lines_of(text):
    # the built page has long lines: check each line, and inside it each statement-sized piece, so ALLOW can name one
    for n, ln in enumerate(text.split('\n'), 1):
        yield n, ln

def check(path, text, bad, page=False):
    rel = os.path.relpath(path, TOP)
    for n, ln in lines_of(text):
        for what, rx in RULES:
            for m in rx.finditer(ln):
                # an allowed piece: the statement around the match is one ALLOW names
                a = max(ln.rfind(';', 0, m.start()), ln.rfind('\n', 0, m.start())) + 1
                b = ln.find(';', m.end()); b = len(ln) if b < 0 else b + 1
                piece = ln[a:b].strip()
                if what == 'a script host named' and any(x.search(piece) or x.search(ln.strip()) for x in ALLOW):
                    continue
                bad.append(f'{rel}:{n}: {what}: …{ln[max(0, m.start() - 40):m.end() + 40]}…')
    if page:
        m = CSP.search(text[:20000])
        if not m:
            bad.append(f'{rel}: no Content-Security-Policy in the page')
        else:
            d = {x.strip().split(' ')[0]: x.strip().split(' ')[1:] for x in m.group(1).split(';') if x.strip()}
            ss = d.get('script-src') or d.get('default-src')
            if not ss:
                bad.append(f'{rel}: its Content-Security-Policy has no script-src')
            elif any(v in ('*', 'https:', 'http:', 'data:') or re.match(r'(https?:)?//|[a-z0-9-]+\.[a-z]', v) for v in ss):
                bad.append(f'{rel}: its Content-Security-Policy lets scripts of another site in: {" ".join(ss)}')

def main():
    bad = []
    src = sorted(glob.glob(os.path.join(TOP, 'desktop', '*.js'))) + [os.path.join(TOP, 'desktop', 'base_v24.html')]
    for f in src:
        check(f, open(f, encoding='utf-8', errors='ignore').read(), bad)
    www = os.path.join(ISO, 'www')
    pages = [os.path.join(www, 'index.html')] + sorted(glob.glob(os.path.join(www, 'a', '*.html')))
    for f in pages:
        if os.path.exists(f):
            check(f, open(f, encoding='utf-8', errors='ignore').read(), bad, page=True)
    for f in sorted(glob.glob(os.path.join(www, 'a', '*.js'))):
        check(f, open(f, encoding='utf-8', errors='ignore').read(), bad)
    for b in bad:
        print(b)
    print('noremote: ' + ('%d found' % len(bad) if bad else 'ok (%d sources, %d pages)' % (len(src), len(pages))))
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
