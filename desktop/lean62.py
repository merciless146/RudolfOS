# Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 38, step 2): small pages for the native desktop's app windows, made by build19.py.
# On the native desktop every app's window is a page of its own, and each loaded the whole desktop page: about 3.5 MB of script parsed,
# compiled and kept for one app (Task Manager's own web process: about 130 MB on the owner's Vivobook, 89-101 MB of a Chromium renderer
# on the host, against 31 MB for the same page without its script). A small page here is the whole page built (index.html, every edit
# build19.py makes to it already made) with only the top-level functions its app runs kept in it: the list below, measured with
# Chromium's coverage of a session of every tab and action (iso/qt/shell2/lean162m.js cover; written in the plan). Nothing is copied
# and nothing is left out for good:
#   - everything else of the page's script stays as it is (its settings, its state, the wrappers modules put over other functions, the
#     apps' registrations), so the page starts as the whole page does;
#   - every top-level function not on the list is a stub of the same name; the functions it stands for are one file beside the page
#     (www/a/, named by its contents), read and run the first time any of them is called (synchronously, as oapage61.py's modules are):
#     a function an action needs and the list missed is slower once, and noted (window.__leanRead: the test fails on any);
#   - a name on the list that the page has no top-level function of stops the build (a function renamed or removed in its module);
#   - the page says what it is as the apps' page does (<script>window.__OAPAGE=1;), so rudolfd serves it (/?oa=ID) as the page: with the
#     token, the storage and the page's policy, to the desktop only (rudolfd.c oa_page); nothing new is asked of rudolfd.
# The whole page sends its app's window there (the native desktop's handler only; Standard, Lite and a page opened by hand: never), and
# the hidden page tells the native shell the page's name, so it opens Task Manager there at once (rudolf-native.c tm_page).
import base64, json, re

# Task Manager (tm26.js, tmtop57.js, apps57.js, oneapp59.js's window list and the parts of the page they use; measured 2026-11-23)
TASKMGR = []   # (filled from the coverage: see LISTS below)

# ---- the page's script, read as JavaScript is (strings, templates with their ${...}, regular expressions and comments skipped whole)
KW_RE = set('return typeof instanceof in of new delete void throw case do else yield await'.split())
ID = re.compile(r'[A-Za-z_$\u0080-￿][\w$\u0080-￿]*')
NUM = re.compile(r'(?:0[xXoObB][0-9a-fA-F_]+n?|(?:\d[\d_]*\.?[\d_]*|\.\d[\d_]*)(?:[eE][+-]?\d+)?n?)')
PUNCT = ['>>>=', '...', '===', '!==', '**=', '<<=', '>>=', '>>>', '&&=', '||=', '??=', '=>', '==', '!=', '<=', '>=', '&&', '||', '??', '?.', '++', '--', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '**', '<<', '>>']
def scan(src):
    """tokens (depth, kind, text, start, end): kind id, num, str (strings and whole templates), re, open, close, op; comments skipped"""
    i, n, out, stack, prev = 0, len(src), [], [], None   # prev: the last token's (kind, text)
    tdepth = []   # template nesting: the stack length when each ${ opened
    def regex_ok():
        if prev is None: return True
        k, t = prev
        if k in ('num', 'str', 're'): return False
        if k == 'id': return t in KW_RE
        if k == 'close': return t == '}'
        return True
    def tmpl(j, start_tok):
        # from just after ` or }: to the closing ` (a str token) or a ${ (pushes and returns)
        while True:
            c = src[j]
            if c == '\\': j += 2; continue
            if c == '`': return j + 1, False
            if c == '$' and src[j + 1] == '{': return j + 2, True
            j += 1
    while i < n:
        c = src[i]
        if c in ' \t\r\n  ﻿': i += 1; continue
        if src.startswith('//', i): j = src.find('\n', i); i = n if j < 0 else j; continue
        if src.startswith('/*', i): j = src.find('*/', i + 2); assert j >= 0, 'an unclosed comment'; i = j + 2; continue
        d = len(stack)
        if c in '"\'':
            j = i + 1
            while src[j] != c:
                assert src[j] != '\n' or src[j - 1] == '\\', ('a string across lines', src[i:i + 60])
                j += 2 if src[j] == '\\' else 1
            out.append((d, 'str', '', i, j + 1)); prev = ('str', ''); i = j + 1; continue
        if c == '`' or (c == '}' and stack and stack[-1] == 'T'):
            if c == '}': stack.pop(); s0 = tdepth.pop()
            else: s0 = i
            j, sub = tmpl(i + 1, s0)
            if sub: stack.append('T'); tdepth.append(s0); i = j; prev = ('op', '${'); continue
            dd = len(stack); out.append((dd, 'str', '', s0, j)); prev = ('str', ''); i = j; continue
        if c == '/' and regex_ok():
            j, cls = i + 1, False
            while True:
                ch = src[j]
                if ch == '\\': j += 2; continue
                assert ch != '\n', ('a regular expression across lines', src[i:i + 60])
                if ch == '[': cls = True
                elif ch == ']': cls = False
                elif ch == '/' and not cls: break
                j += 1
            j += 1
            while j < n and (src[j].isalnum() or src[j] == '_'): j += 1
            out.append((d, 're', '', i, j)); prev = ('re', ''); i = j; continue
        m = ID.match(src, i)
        if m and not c.isdigit():
            out.append((d, 'id', m.group(0), i, m.end())); prev = ('id', m.group(0)); i = m.end(); continue
        if c.isdigit() or (c == '.' and i + 1 < n and src[i + 1].isdigit()):
            m = NUM.match(src, i); out.append((d, 'num', '', i, m.end())); prev = ('num', ''); i = m.end(); continue
        if c in '([{': out.append((d, 'open', c, i, i + 1)); stack.append(c); prev = ('open', c); i += 1; continue
        if c in ')]}':
            assert stack and stack[-1] == {')': '(', ']': '[', '}': '{'}[c], ('brackets do not match', src[max(0, i - 80):i + 20])
            stack.pop(); out.append((len(stack), 'close', c, i, i + 1)); prev = ('close', c); i += 1; continue
        for p in PUNCT:
            if src.startswith(p, i): break
        else: p = c
        out.append((d, 'op', p, i, i + len(p))); prev = ('op', p); i += len(p)
    assert not stack, 'brackets do not match at the end'
    return out

STMT_KW = set('const let var function class if for while do try switch return throw async import export'.split())
def items(src, T):
    """the script's top-level functions: [(kind, name, start, end, value start)]: kind "fn", a function declaration (to the end of its
    body); kind "val", a const, let or var of one name whose value is a function (an arrow or a function expression; to the end of the
    statement, its ";" included). Anything else is not one (and stays where it is)."""
    out, i, n = [], 0, len(T)
    def nl(a, b): return '\n' in src[a:b]
    while i < n:
        d, k, t, a, b = T[i]
        if d or k != 'id': i += 1; continue
        p = T[i - 1] if i else None
        start = p is None or (p[1] == 'op' and p[2] == ';') or (p[1] == 'close' and p[2] == '}') or \
            (p[1] in ('id', 'num', 'str', 're', 'close') and nl(p[4], a) and p[2] not in ('return', 'typeof', 'new', 'void', 'await', 'yield', 'in', 'of', 'case', 'throw', 'delete', 'instanceof', 'export', 'default', 'else', 'do'))
        if not start: i += 1; continue
        if t == 'function' or (t == 'async' and i + 1 < n and T[i + 1][2] == 'function' and not nl(b, T[i + 1][3])):
            j = i + (2 if t == 'async' else 1)
            if T[j][1] == 'op' and T[j][2] == '*': j += 1
            if T[j][1] != 'id': i += 1; continue
            name = T[j][2]
            while not (T[j][1] == 'open' and T[j][2] == '{' and T[j][0] == 0): j += 1
            j += 1
            while not (T[j][1] == 'close' and T[j][2] == '}' and T[j][0] == 0): j += 1
            out.append(('fn', name, a, T[j][4], a)); i = j + 1; continue
        if t in ('const', 'let', 'var') and i + 3 < n and T[i + 1][1] == 'id' and T[i + 2][2] == '=' and T[i + 1][0] == 0:
            name, v = T[i + 1][2], i + 3
            # the value: function..., async function..., NAME =>, (...) =>, async NAME =>, async (...) =>
            q = v + 1 if T[v][2] == 'async' and T[v][1] == 'id' else v
            fnv = T[q][2] == 'function' and T[q][1] == 'id'
            if not fnv and T[q][1] == 'id' and T[q + 1][2] == '=>': fnv = True
            if not fnv and T[q][1] == 'open' and T[q][2] == '(':
                r = q + 1
                while not (T[r][1] == 'close' and T[r][0] == 0): r += 1
                fnv = T[r + 1][2] == '=>'
            if not fnv: i += 1; continue
            # its end: the first ";" or "," back at depth 0, or (a block body) its "}" at the end of a line
            j, end = v, None
            while j < n:
                dj, kj, tj = T[j][0], T[j][1], T[j][2]
                if dj == 0 and kj == 'op' and tj == ',': break
                if dj == 0 and kj == 'op' and tj == ';': end = T[j][4]; break
                if dj == 0 and kj == 'close' and tj == '}' and (j + 1 >= n or nl(T[j][4], T[j + 1][3])) and (j + 1 >= n or T[j + 1][2] != ';'): end = T[j][4]; break
                if dj == 0 and j > v and nl(T[j - 1][4], T[j][3]) and (kj != 'id' or tj in STMT_KW): break   # (a statement ended without its ";": kept as it is)
                j += 1
            if end is not None: out.append(('val', name, a, end, T[v][3])); i = j + 1; continue
        i += 1
    return out

LOADER = r"""/* (1.6.2 Part 38, lean62.py: the functions of the page this window does not need: one file, read and run the first time any of them is
   called; their stubs then call them, and a wrapper another module put over a stub stays the one the page calls) */
const __LNF=%s,__LNU=%s,__LNS={},__LNR={};let __LND=0;for(const n of __LNF)__LNS[n]=globalThis[n];
function __lnm(n,t,a,nt){if(!__LND){__LND=1;(window.__leanRead=window.__leanRead||[]).push(n);console.warn('small page: the rest of the page read for '+n);
 const x=new XMLHttpRequest();x.open('GET',__LNU,false);x.send();if(x.status!=200){__LND=0;throw new Error('the rest of the page could not be read ('+__LNU+')')}
 const cur={};for(const k of __LNF)cur[k]=globalThis[k];const s=document.createElement('script');s.textContent=x.responseText;document.head.appendChild(s);s.remove();
 for(const k of __LNF){__LNR[k]=globalThis[k];if(cur[k]!==__LNS[k])globalThis[k]=cur[k]}}
 else if(!window.__leanRead.includes(n))window.__leanRead.push(n);
 const f=__LNR[n];if(typeof f!='function')throw new Error(n+' is not in the page');return nt?Reflect.construct(f,a,nt):f.apply(t,a)}
"""

def script(page):
    """where the page's own script is: (start, end)"""
    a = page.index('<script>\n' + "/* ===== Rudolf OS 1.6.0") + len('<script>\n'); z = page.index("__svcEnd();\n</script></body>")
    return a, z

def build(page, keep, out, what):
    """page: the whole page as built (oapage61's redirect in it or not: it does nothing in a page that says it is a small one); keep: the
    names of the top-level functions kept; out(data_url): a file in www/a/ and its name. Gives (the small page, its ID, the numbers)"""
    a, z = script(page); src = page[a:z]
    F = items(src, scan(src)); names = [f[1] for f in F]
    twice = {n for n in names if names.count(n) > 1}
    missing = [n for n in keep if n not in names]
    assert not missing, '%s: the list names functions the page does not have (renamed or removed?): %s' % (what, ' '.join(missing))
    K = set(keep) | twice
    rest, body, last, fns = [], [], 0, []
    stub = lambda n: 'function(){return __lnm(%s,this,arguments,new.target)}' % json.dumps(n)
    for kind, n, s0, s1, v0 in F:
        if n in K: continue
        body.append(src[last:s0])
        if kind == 'fn': rest.append(src[s0:s1]); fns.append(n); body.append('function %s(){return __lnm(%s,this,arguments,new.target)}' % (n, json.dumps(n)))
        else:
            v = src[v0:s1].rstrip(); v = v[:-1] if v.endswith(';') else v
            rest.append('__LNR[%s]=(%s);' % (json.dumps(n), v)); body.append(src[s0:v0] + stub(n) + ';')
        last = s1
    body.append(src[last:])
    u = out('data:text/javascript;base64,' + base64.b64encode('\n'.join(rest).encode()).decode())
    code = LOADER % (json.dumps(fns, separators=(',', ':')), json.dumps(u)) + ''.join(body)
    small = page[:a] + code + page[z:]
    small = small.replace("<script>/* (1.6.0, the owner's Vivobook report 1)", "<script>window.__OAPAGE=1;window.__LEAN=%s;/* (1.6.0, the owner's Vivobook report 1)" % json.dumps(what), 1)
    assert 'window.__OAPAGE=1;' in small
    h = out('data:text/html;base64,' + base64.b64encode(small.encode()).decode())
    hid = re.match(r'a/([0-9a-f]{16})\.html$', h).group(1)
    return small, hid, {'functions': len(F), 'kept': len(F) - len(rest), 'script': len(src), 'small': len(code), 'rest': sum(map(len, rest))}
