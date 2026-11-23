#!/usr/bin/env python3
# 1.6.1 (docs/plan-1.6.1.md Part 20): what each window may ask rudolfd's API for, read from the code that asks.
#   - the page (desktop/*.js, desktop/*.html): Standard's whole desktop, Lite's, and the native desktop's app windows,
#     which are the same page one app at a time (index.html#app=NAME): every literal path given to rd, rdPost, fidoReq,
#     prJSON, lxApi, blzdApi or written as '/api/...'; a path that goes on with a variable (rd('pkg/'+op), `home/${op}`)
#     is a prefix ("pkg/*"); calls whose path is a variable only are listed by hand below (PAGE_DYNAMIC), each with
#     where it is;
#   - the native shell (iso/sys/rudolf-native.c, the program itself, not its pages): every "/api/..." in its source, a
#     printf-style %s or %d where it goes on making it a prefix;
#   - (1.6.2, docs/plan-1.6.2.md Part 39) the native apps (iso/sys/apps/NAME.c, /usr/bin/rudolf-NAME, each with librudolf-ui,
#     iso/sys/ui): every "/api/..." in their sources and the library's, read the same way; the programs' names listed too
#     (WINAPI_APPS_EXE), so rudolfd knows one by its program as it knows the native shell.
# rudolfd (rudolfd-confine.inc) finds out which it is from the process that holds the connection (the peer check of
# 1.5.9 already finds it: rudolfd-peer.inc), never from anything the request says, and refuses (or, in "log" mode,
# only notes) a request that its window never makes.
# Writes iso/sys/rudolfd-winapi.h with --update; without it, says what differs (confine161_t.sh runs it).
# GPL-2.0-or-later.
import os, re, sys, glob
TOP = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
OUT = os.path.join(TOP, 'iso', 'sys', 'rudolfd-winapi.h')
WRAP = {'rd': '', 'rdPost': '', 'fidoReq': '', 'fidoTouchReq': '', 'prJSON': '', 'lxApi': '', 'blzdApi': 'ai/dev/'}
# the page's calls whose path is held in a variable: (where, what it can be)
PAGE_DYNAMIC = {
    'events': 'desktop/core25.js (rd(wait?...))', 'events/wait': 'desktop/core25.js (rd(wait?...))',
    'apps/desktop': 'desktop/linux26.js (lxApi(cur&&cur.shown?...))', 'apps/show': 'desktop/linux26.js (lxApi(cur&&cur.shown?...))',
    'web/hide': 'desktop/web25.js (rdPost(WEB_SHOWN?...))', 'web/show': 'desktop/web25.js (rdPost(WEB_SHOWN?...))',
}
# and, so that a call through a helper of its own is not missed (sysReport's get('system'), fidoTouchReq(path, ...)):
# every other string in the page's code that is one of api()'s own names (confinepure.py reads them), or starts with
# one of its prefixes, counts as asked for. (More is only a wider list; a missed one would refuse the page.)

def norm(p, more):
    """a path as the API sees it: up to the query; a prefix (ending in *) when the code goes on with a variable"""
    q = p.split('?')[0]
    for cut in ('${', '%s', '%d', '%u', '%ld'):
        if cut in q: q = q[:q.index(cut)]; more = True
    q = q.strip()
    if not q: return None
    return q + '*' if more and '?' not in p else q

def page_paths():
    found = {}
    files = sorted(glob.glob(os.path.join(TOP, 'desktop', '*.js')) + glob.glob(os.path.join(TOP, 'desktop', '*.html')))
    lit = r"""(['"`])((?:\\.|(?!\1).)*)\1(\s*\+)?"""
    for f in files:
        s = open(f, errors='replace').read(); rel = os.path.relpath(f, TOP)
        for name, pre in WRAP.items():
            for m in re.finditer(r'(?<![\w.$])' + name + r'\(\s*' + lit, s):
                p = norm(pre + m.group(2), bool(m.group(3)))
                if p: found.setdefault(p, rel)
        for m in re.finditer(r"""(['"`])/api/((?:\\.|(?!\1).)*)\1(\s*\+)?""", s):
            p = norm(m.group(2), bool(m.group(3)))
            if p: found.setdefault(p, rel)
    for p, where in PAGE_DYNAMIC.items(): found.setdefault(p, where)
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import confinepure
    reqs = confinepure.classify()[0]; names = {n for n, _ in reqs}
    exact = {n for n in names if not n.endswith('*')}; pre = [n[:-1] for n in names if n.endswith('*')]
    for f in files:
        s = open(f, errors='replace').read(); rel = os.path.relpath(f, TOP)
        for m in re.finditer(r"""(['"`])([a-z][a-z0-9_/.-]{1,60})(\?[^'"`]*)?\1""", s):
            v = m.group(2)
            if v in exact: found.setdefault(v, rel + ' (a string)')
            elif any(v.startswith(x) and len(v) > len(x) for x in pre): found.setdefault(v, rel + ' (a string)')
    return found

def shell_paths():
    s = open(os.path.join(TOP, 'iso', 'sys', 'rudolf-native.c')).read(); found = {}
    for m in re.finditer(r'"/api/([^"]*)"', s):
        p = norm(m.group(1), False)
        if p: found.setdefault(p, 'iso/sys/rudolf-native.c')
    return found

def apps_paths():
    found = {}; files = sorted(glob.glob(os.path.join(TOP, 'iso', 'sys', 'ui', '*.c')) + glob.glob(os.path.join(TOP, 'iso', 'sys', 'apps', '*.c')))
    for f in files:
        if f.endswith('ui-sample.c'): continue   # (the test's sample window: not shipped)
        s = open(f).read()
        for m in re.finditer(r'"/api/([^"]*)"', s):
            p = norm(m.group(1), False)
            if p: found.setdefault(p, os.path.relpath(f, TOP))
    return found

def apps_exe():
    return sorted('rudolf-' + os.path.basename(f)[:-2] for f in glob.glob(os.path.join(TOP, 'iso', 'sys', 'apps', '*.c')) if '_' not in os.path.basename(f))   # (NAME_PART.c: a part of NAME)

def render(page, shell, apps=None, exe=None):
    apps = apps if apps is not None else apps_paths(); exe = exe if exe is not None else apps_exe()
    q = lambda d: '\n'.join(f'\t"{p}",' for p in sorted(d))
    return f'''/* (generated by iso/systools/winapi.py --update: never by hand) 1.6.1 (docs/plan-1.6.1.md Part 20): the API paths
 * each window asks for, read from its code. A name ending in * is a prefix. */
static const char *const WINAPI_PAGE[] = {{   /* the desktop's page: Standard, Lite, the native desktop's app windows */
{q(page)}
	NULL
}};
static const char *const WINAPI_SHELL[] = {{   /* the native shell, rudolf-native itself */
{q(shell)}
	NULL
}};
static const char *const WINAPI_APPS[] = {{   /* (1.6.2 Part 39) the native apps (iso/sys/apps) with librudolf-ui (iso/sys/ui) */
{q(apps)}
	NULL
}};
static const char *const WINAPI_APPS_EXE[] = {{   /* their programs (/usr/bin/rudolf-NAME) */
{q({e: 1 for e in exe})}
	NULL
}};
'''

if __name__ == '__main__':
    page, shell = page_paths(), shell_paths()
    if '--list' in sys.argv:
        for p, w in sorted(page.items()): print('page ', p, w)
        for p, w in sorted(shell.items()): print('shell', p, w)
        sys.exit(0)
    new = render(page, shell); old = open(OUT).read() if os.path.exists(OUT) else ''
    if '--update' in sys.argv:
        open(OUT, 'w').write(new); print(f'winapi: {len(page)} for the page, {len(shell)} for the native shell'); sys.exit(0)
    if old != new:
        print('winapi: iso/sys/rudolfd-winapi.h is not current (run iso/systools/winapi.py --update)'); sys.exit(1)
    print(f'winapi: current ({len(page)} for the page, {len(shell)} for the native shell)')
