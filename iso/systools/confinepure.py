#!/usr/bin/env python3
# 1.6.1 (docs/plan-1.6.1.md Part 20): which of rudolfd's requests start nothing, so that rudolfd may put their process
# (each connection is a process of its own) in the narrow confinement (rudolfd-confine.inc): no program started, no
# mount, no signal to another process. Read from the source, never guessed:
#   - every function of rudolfd.c and its .inc files, and what it calls;
#   - a function "starts" something when it calls system, popen, fork, the exec family, posix_spawn, run, run_out,
#     start_job (the shell), mount or umount, kill, or one that does (followed through every call);
#     root_call (rudolf-rootd's helpers: a message on a socket, the helper is rudolf-rootd's child, not this process's;
#     without rudolf-rootd it runs the helper itself) is kept apart: such a request is narrow only when rudolf-rootd is
#     there. root_run and root_out are read as any function (root_out forks a process that only calls root_call;
#     root_job runs a shell command too: found in QEMU, where the narrow confinement refused its exec);
#   - each request api() hands on (its `if (!strcmp(p, ...))` or `strncmp` line and the block it runs).
# Writes iso/sys/rudolfd-confine-pure.h with --update; without it, says what differs (the host test, confine161_t.sh,
# runs it, so a request that starts to run a program is never left in the narrow list, and one that no longer does
# is named). GPL-2.0-or-later.
import os, re, sys
SYS = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'sys')
OUT = os.path.join(SYS, 'rudolfd-confine-pure.h')
SPAWN = {'system', 'popen', 'fork', 'vfork', 'execv', 'execvp', 'execve', 'execl', 'execlp', 'execle', 'execvpe', 'posix_spawn',
         'posix_spawnp', 'run', 'run_out', 'start_job', 'mount', 'umount', 'umount2', 'kill', 'killpg', 'pkill', 'clone',
         'reboot', 'daemon', 'setuid', 'setgid', 'setresuid', 'setresgid', 'chroot', 'unshare', 'setns'}
ROOT = {'root_call'}
# (functions that fork without running a program and set a user: rudolfd-drop.inc's child opens the file as the
# account's user; it is allowed in the narrow confinement: no program, no mount, a child of this process only)
FORK_ONLY_OK = {'drop_open', 'root_out'}   # (their fork is read past: what the child does is still followed)
# requests that start nothing but write where the narrow confinement's folders do not reach, kept out of it by hand
# (rudolfd-confine.inc's CONFINE_WRITE has the folders; a narrow rule for one file elsewhere would have to name the
# file's folder, which would open the whole folder)
WIDE = {
    'display': '/etc/rudolf-display (the start-up reads /etc: never writable in the narrow confinement)',
    'bootmenu': "the firmware's variables (/sys/firmware/efi/efivars, and their immutable flag: CAP_LINUX_IMMUTABLE)",
    'bootmenu/*': "the firmware's variables (as bootmenu)",
    'desktop/keys*': "the security keys themselves (/dev/hidraw*, read, written and their ioctls, through libfido2)",
    'brightness': "the backlight's file under /sys/devices (the screen's own folder)",
    'factory-reset': '/persist/.factory-reset (the whole of /persist would have to be writable)',
}
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'defined', 'do', 'else', 'case'}

def strip(s):
    """comments blanked and the insides of strings and characters made x, every length kept (offsets stay the raw text's)"""
    o = list(s); i = 0; n = len(s)
    while i < n:
        c = s[i]
        if c == '/' and s.startswith('/*', i):
            j = s.find('*/', i + 2); j = n if j < 0 else j + 2
            for k in range(i, j):
                if o[k] != '\n': o[k] = ' '
            i = j
        elif c == '/' and s.startswith('//', i):
            j = s.find('\n', i); j = n if j < 0 else j
            for k in range(i, j): o[k] = ' '
            i = j
        elif c in '"\'':
            j = i + 1
            while j < n and s[j] != c and s[j] != '\n':
                if s[j] == '\\': o[j] = 'x'; j += 1
                o[j] = 'x'; j += 1
            i = j + 1
        else: i += 1
    return ''.join(o)

def functions(src, raw=None):
    out = {}; raw = raw or src
    for m in re.finditer(r'^(?:static\s+|inline\s+)*[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*\{', src, re.M):
        name = m.group(1)
        if name in KEYWORDS: continue
        i = m.end(); d = 1
        while d and i < len(src):
            c = src[i]; d += c == '{'; d -= c == '}'; i += 1
        out[name] = raw[m.end():i]
    return out

def calls(body): return set(re.findall(r'\b([A-Za-z_]\w*)\s*\(', body)) - KEYWORDS

def load():
    raw = ''
    for f in ['rudolfd.c'] + sorted(x for x in os.listdir(SYS) if x.startswith('rudolfd') and x.endswith('.inc')):
        raw += open(os.path.join(SYS, f)).read() + '\n'
    return raw

def classify():
    raw = load(); src = strip(raw); fns = functions(src)
    cg = {n: calls(b) for n, b in fns.items()}
    memo = {}
    def kind(n, stack=()):
        if n in memo: return memo[n]
        if n in SPAWN: return 2
        if n in ROOT: return 1
        if n not in cg or n in stack: return 0
        k = 0
        for c in cg[n] - ({'fork', 'setuid', 'setgid', 'setresuid', 'setresgid'} if n in FORK_ONLY_OK else set()):
            k = max(k, kind(c, stack + (n,)))
            if k == 2: break
        memo[n] = k; return k
    # api()'s requests, read from the raw text (the names are in strings)
    rawfns = functions(src, raw)
    api = rawfns['api']; sapi = strip(api)
    reqs = []
    lines = api.split('\n'); slines = sapi.split('\n'); i = 0
    while i < len(lines):
        ln = lines[i]
        m = re.match(r'\s*if \((.*)', ln)
        names = re.findall(r'!str(n?)cmp\(p, "([^"]*)"', ln) if m else []
        if not names: i += 1; continue
        # the block: this line, and on to its closing brace when it opens one
        sb = slines[i]; d = sb.count('{') - sb.count('}'); j = i
        while d > 0 and j + 1 < len(lines):
            j += 1; sb += '\n' + slines[j]; d += slines[j].count('{') - slines[j].count('}')
        k = 0
        for c in calls(sb) - {'strcmp', 'strncmp'}:
            k = max(k, kind(c))
        for n, nm in names:
            reqs.append((nm + ('*' if n else ''), k))
        i = j + 1
    # what every request goes through before api(): handle()'s own calls up to it
    pre = 0
    for c in calls(strip(rawfns['handle'].split('api(&r)')[0])):
        pre = max(pre, kind(c))
    # and the files and the page served without api() (/, /index.html, /dlf/, /pkg/, the page's own files)
    stat = 0
    for c in calls(strip(rawfns['handle'])) - {'api'}:
        stat = max(stat, kind(c))
    return reqs, (pre, stat), kind

def table(reqs):
    best = {}
    for nm, k in reqs:   # (a name met twice, as events/wait: the strictest)
        best[nm] = max(best.get(nm, 0), 2 if nm in WIDE else k)
    pure = sorted(n for n, k in best.items() if k == 0); root = sorted(n for n, k in best.items() if k == 1)
    return pure, root, best

def render(pure, root, best, stat):
    start = sorted(n for n, k in best.items() if k == 2)
    q = lambda xs: '\n'.join(f'\t"{x}",' for x in xs)
    return f'''/* (generated by iso/systools/confinepure.py --update: never by hand) 1.6.1 (docs/plan-1.6.1.md Part 20): rudolfd's
 * requests that start nothing (no program, no mount, no signal to another process), read from the source. A name
 * ending in * is a prefix (strncmp in api()). CONFINE_PURE: always narrow; CONFINE_ROOTD: narrow while rudolf-rootd is
 * there (they reach root only through its helpers); CONFINE_START: the rest (a request matched by one of these is
 * never narrow, whatever else it matches: api() may reach it before the narrow one, as fs/xcopy before fs/). */
static const char *const CONFINE_PURE[] = {{
{q(pure)}
	NULL
}};
static const char *const CONFINE_ROOTD[] = {{
{q(root)}
	NULL
}};
static const char *const CONFINE_START[] = {{
{q(start)}
	NULL
}};
#define CONFINE_STATIC {1 if stat == 0 else 0}   /* the page and its files (/, /index.html, /dlf/, /pkg/): narrow */
'''

if __name__ == '__main__':
    reqs, (pre, stat), kind = classify()
    if pre:
        print('confinepure: handle() itself starts something before api(): nothing can be narrow'); sys.exit(1)
    pure, root, best = table(reqs)
    new = render(pure, root, best, stat)
    if '--list' in sys.argv:
        for n, k in sorted(best.items()): print(('pure ', 'rootd', 'start')[k], n)
        sys.exit(0)
    old = open(OUT).read() if os.path.exists(OUT) else ''
    if '--update' in sys.argv:
        open(OUT, 'w').write(new); print(f'confinepure: {len(pure)} narrow, {len(root)} narrow with rudolf-rootd, {len(best) - len(pure) - len(root)} not'); sys.exit(0)
    if old != new:
        o = set(re.findall(r'"([^"]+)"', old)); n = set(re.findall(r'"([^"]+)"', new))
        print('confinepure: iso/sys/rudolfd-confine-pure.h is not current (run iso/systools/confinepure.py --update):')
        for x in sorted(o - n): print('  no longer narrow:', x)
        for x in sorted(n - o): print('  now narrow:', x)
        sys.exit(1)
    print(f'confinepure: current ({len(pure)} narrow, {len(root)} with rudolf-rootd)')

def writes_of(names):
    """(for review) the write-like calls in each listed request's own block and every function it reaches"""
    raw = load(); src = strip(raw); fns = functions(src, raw); sfns = {n: strip(b) for n, b in fns.items()}
    W = re.compile(r'\b(write_str|fopen|open|openat|mkdir|mkdirat|rename|renameat|unlink|unlinkat|rmdir|creat|truncate|chmod|chown|lchown|symlink|link|write_file|dput|save_file|efivar_write|utimes|utimensat)\s*\(')
    def reach(n, seen):
        if n in seen or n not in sfns: return
        seen.add(n)
        for c in calls(sfns[n]): reach(c, seen)
    return fns, sfns, W, reach
