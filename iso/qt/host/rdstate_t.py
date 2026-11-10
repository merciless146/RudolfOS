#!/usr/bin/env python3
"""1.5.7 (plan stage 5, Part 2 item 3): rudolfd's state, reviewed. rudolfd answers each request in a process of its
own (forked), so a value one request sets in a variable is gone at the next (#131: Downloads' store, fixed by reading
it again from the mounts at each request). Every variable that outlives a function is listed here with why it is
safe; a new one fails this test until it is reviewed and added.   python3 rdstate_t.py"""
import os, re, sys
SYS = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../sys')
SRC = {f: open(os.path.join(SYS, f)).read() for f in ('rudolfd.c', 'rudolfd-home.inc')}
# file-scope variables: name -> (the functions that may set them, why that is safe)
GLOBAL = {
    'token': ({'main'}, 'made once at start, before the first request'),
    'boot_mode': ({'main'}, 'read once at start (how the computer started)'),
    'media_mode': ({'main'}, 'read once at start'),
    'live': ({'main'}, 'read once at start (rudolf-init made /run/rudolf/persistent before rudolfd)'),
    'recovery': ({'main'}, 'read once at start (the start-up options)'),
    'secureboot': ({'main'}, 'read once at start'),
    'DLHOME': ({'dl_store_paths'}, 'set again at every request by dl_store_state (#131)'),
    'INCOMING': ({'dl_store_paths'}, 'as DLHOME'), 'QUARANTINE': ({'dl_store_paths'}, 'as DLHOME'), 'DLACCTS': ({'dl_store_paths'}, 'as DLHOME'),
    'dl_store_bound': ({'dl_store_state', 'dl_store_bind'}, 'read from the mounts at every request (dl_store_state); dl_store_bind sets it for the rest of its own request'),
    'home_root': (set(), 'a constant, changed only by the host tests'),
    'home_uid': (set(), 'a constant, changed only by the host tests'),
    'home_nobind': (set(), 'a constant, changed only by the host tests'),
}
# variables kept inside a function (static): (function, name) -> why
LOCAL = {
    ('boot_secure', 'cached'): 'a fact of the start-up medium, the same in every process',
    ('qparam', 'vals'): 'scratch buffers within one request', ('qparam', 'vi'): 'scratch, within one request',
    ('img_sha_env', 'env'): 'scratch, within one request',
    ('time_tick', 'last_try'): 'the main loop only (checked below)', ('time_tick', 'last_ok'): 'the main loop only', ('time_tick', 'last_check'): 'the main loop only',
    ('time_tick', 'had_route'): 'the main loop only', ('time_tick', 'ok_since_up'): 'the main loop only',
    ('health_tick', 'last'): 'the main loop only', ('health_tick', 'last_fw'): 'the main loop only', ('health_tick', 'last_disk'): 'the main loop only',
    ('health_tick', 'last_heat'): 'the main loop only', ('health_tick', 'mg_off'): 'the main loop only', ('health_tick', 'fw_prev'): 'the main loop only', ('health_tick', 'ck'): 'the main loop only',
    ('dl_same_bytes', 'ba'): 'scratch buffers within one call', ('dl_same_bytes', 'bb'): 'scratch',
    ('api_fs', 'stack'): 'scratch, within one search', ('api_fs', 'depth'): 'scratch, within one search',
    ('api_apps', 'seq'): 'with the process id in the name, so unique in every process',
}
R = []
def ok(n, c, d=''): R.append(bool(c)); print(('PASS ' if c else 'FAIL ') + n + ('' if c else ' - ' + str(d)[:900]))
# functions: a header line at column 0 ending with ')' and a '{' line, up to the next '}' line at column 0
funcs = []
for f, s in SRC.items():
    lines = s.split('\n'); i = 0
    while i < len(lines):
        m = re.match(r'^(?:static\s+)?[\w\s\*]+?\b(\w+)\s*\([^;]*\)\s*$', lines[i])
        if m and i + 1 < len(lines) and lines[i + 1] == '{':
            j = i + 2
            while j < len(lines) and lines[j] != '}': j += 1
            funcs.append((m.group(1), '\n'.join(lines[i + 2:j]), f)); i = j
        i += 1
fnames = {n for n, _, _ in funcs}
ok('0. the functions are found (main, api_home, handle)', {'main', 'api_home', 'health_tick', 'time_tick', 'dl_store_state'} <= fnames, sorted(fnames)[:20])
# file-scope statics
glob = set()
for f, s in SRC.items():
    for line in s.split('\n'):
        m = re.match(r'^static\s+(?!const\b)(?:volatile\s+)?(?:unsigned\s+|signed\s+)?(?:int|long long|long|char|time_t|pid_t|size_t|double)\s+(.*?);', line)
        if m and '(' not in m.group(1).split('=')[0]:
            for part in re.split(r',\s*(?![^\[]*\])', m.group(1)):
                n = re.match(r'\**\s*(\w+)', part.strip())
                if n: glob.add(n.group(1))
ok('1. every variable outside a function is reviewed', glob <= set(GLOBAL), sorted(glob - set(GLOBAL)))
def setters(name):
    out = set()
    pat = re.compile(r'(?<![\w.>])' + re.escape(name) + r'\s*(\[[^\]]*\])?\s*(=(?!=)|\+\+|--|\+=|-=)|snprintf\(\s*' + re.escape(name) + r'\b|sprintf\(\s*' + re.escape(name) + r'\b|strcpy\(\s*' + re.escape(name) + r'\b|memcpy\(\s*' + re.escape(name) + r'\b|sprintf\(\s*' + re.escape(name) + r'\s*\+')
    for fn, body, _ in funcs:
        b = re.sub(r'\b(?:static\s+)?(?:const\s+)?(?:unsigned\s+)?(?:int|char|long|size_t|time_t|pid_t|struct\s+\w+)\s+\**' + re.escape(name) + r'\b', '', body)   # a local of the same name
        if pat.search(b): out.add(fn)
    return out
bad = {n: sorted(setters(n) - GLOBAL[n][0]) for n in GLOBAL if setters(n) - GLOBAL[n][0]}
ok('2. each is set only where it is safe (at start, or again at every request)', not bad, bad)
# statics inside functions
loc = set()
for fn, body, _ in funcs:
    for m in re.finditer(r'\bstatic\s+(?!const\b)(?:unsigned\s+)?(?:int|long long|long|char|time_t|unsigned|size_t|double)\s+([^;]*);', body):
        for part in re.split(r',\s*(?![^\[]*\])', m.group(1)):
            n = re.match(r'\**\s*(\w+)', part.strip())
            if n: loc.add((fn, n.group(1)))
unk = sorted(x for x in loc if x not in LOCAL)
ok('3. every variable kept inside a function is reviewed', not unk, unk)
calls = {fn for fn, body, _ in funcs if re.search(r'\b(time_tick|health_tick)\s*\(', body)}
ok("4. the main loop's own checks (time_tick, health_tick) run only in the main loop, never in a request", calls == {'main'}, calls)
if "-v" in sys.argv: print(sorted(loc))
print(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if all(R) else 1)
