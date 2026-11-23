# Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 38, step 2): Task Manager's small page for the native desktop, made by build19.py.
# On the native desktop every app's window is a page of its own, and each loaded the whole desktop page. Measured on the host (Chromium,
# lean162m.js; the plan has the numbers): what a window costs is not the script's size so much as what the whole page does as it starts
# (every module's setup, the sign-in's work for the whole desktop), and Task Manager has a web process of its own (#598), so all of it is
# its own memory. The small page (a/ID.html, named by its contents) is the whole page as built (index.html, every edit build19.py makes
# to it already made), read statement by statement (scan, statements: JavaScript's own reading of strings, templates, regular
# expressions and the end of a statement), with only what the list below keeps:
#   - mods: the modules kept whole (each from its __svcMod marker);
#   - fns: the functions of the page's own code (base_v24.html) kept (measured with Chromium's coverage of a session of every tab and
#     action: lean162m.js cover; the plan has the list's source); every other statement of the page's own code is kept;
#   - every variable any kept statement reads is kept too, wherever it is (and what it reads), and every app's registration (regApp)
#     and shortcut, so the page knows every app by its name and key as the whole page does;
#   - noop: what the whole page does for the whole desktop at a window's sign-in, done here by nobody: the start menu's catalogue (the
#     shell's hidden page gives it: a small page must never give one, it does not have every module), the desktop's icons, the Library's,
#     Linux's and Oracle's lists;
#   - the rest of the page (every other statement, in order) is one file beside it, read and run the first time any of its functions is
#     called: each such function is a stub of the same name in the small page (window.__leanRead says which were called: the test
#     fails on any); so nothing is lost, only slower once;
#   - a name the list has that the page does not stops the build (a module or a function renamed or removed).
# The page says what it is as the apps' page does (<script>window.__OAPAGE=1;), so rudolfd serves it (/?oa=ID) as the page, with the
# token, the storage and the page's policy, to the desktop only (rudolfd.c oa_page): nothing new is asked of rudolfd. The whole page
# names it (__LEAN_TM): the native shell's hidden page tells the shell (op tmpage), which opens Task Manager there (rudolf-native.c
# tm_page); a Task Manager window that opens the whole page all the same (before the hidden page said it) goes there at once. Standard,
# Lite and a page opened by hand never do.
import base64, json, re

# Task Manager (tm26.js, tmtop57.js, apps57.js, oneapp59.js and what they use; measured 2026-11-23: iso/qt/shell2/lean162_t.js compares it)
TASKMGR = {
    'mods': ['hooks61.js', 'core25.js', 'apps25a.js', 'tm26.js', 'tmtop57.js', 'apps57.js', 'oneapp59.js', 'lean61.js', 'a11y25.js', 'titlebar59.js',
             'sb28.js', 'sysupd25.js', 'power31.js', 'accent56.js', 'fonts47.js', 'wallpaper31.js', 'xframe62.js', 'clock31.js', 'shell25.js',
             'shell26.js', 'osd31.js', 'acct50.js', 'native25.js', 'litepoint60.js', 'tools56.js', 'hwcheck30.js', 'linux26.js', 'vault50.js', 'tabs26.js', 'sec50.js', 'devlink40.js', 'term26.js', 'blitzdiag57.js', 'sound58.js', 'sndrs60.js',
             'settings31.js'],
    'fns': ['$', 'RUF_FRAME', 'T_', '__svcEnd', '__svcMod', 'a11yApply', 'a11ySound', 'applyCB', 'applySet', 'bgCSS', 'clk', 'clk2', 'cln', 'cmenuClose',
            'curApply', 'dateFmt', 'dblMs', 'deskIcons', 'dynBg', 'dynCity', 'dynFix', 'dynLoc', 'dynResolve', 'ensureWelcome', 'fmtB',
            'frameOwnerRefresh', 'freeCell', 'gridInfo', 'initIcons2', 'isAdmin', 'isDir', 'isEditable', 'layIcons', 'ld', 'loadUser', 'logEvt',
            'maskInput', 'mmClone', 'mmEnsure', 'mmInstScript', 'mmLoadCfg', 'mmMenu', 'mmReady', 'mmRecog', 'mouseApply', 'netClone', 'netConn',
            'netLoadCfg', 'netPanelRefresh', 'netProbe', 'netUp', 'netUpd', 'node', 'nowH', 'off', 'qEnsure', 'saveUser', 'sndPat', 'sunCalc',
            'svcIs', 'svj', 'sysDateStr', 'sysNow', 'trBadge', 'userUI', 'vkSync', 'volLevel', 'volUpd', 'win', 'wireIcon', 'ymd'],
    'noop': ['pkgStatus', 'pkgRegister', 'deskApply', 'hdOn', 'rtOn', 'icTarget', 'dkPlaceFresh', 'oaCatalog',
             'oaCatSigSave', 'oaCatPush', 'stUpdate'],
}

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

BLOCK={'function','class','if','for','while','try','switch','do'}
CONT={'else','catch','finally'}
def statements(src,T):
    """top-level statements: (start_tok, end_tok) inclusive"""
    out=[];i=0;n=len(T)
    while i<n:
        s=i;d,k,t,a,b=T[i]
        lead=t if k=='id' else (t if k=='open' else '')
        if k=='id' and t=='async' and i+1<n and T[i+1][2]=='function': lead='function'
        j=i;head=-1
        if lead in('if','for','while') and i+1<n and T[i+1][2]=='(':
            q=i+2
            while not(T[q][1]=='close' and T[q][0]==0): q+=1
            head=q
        while j<n:
            dj,kj,tj,aj,bj=T[j]
            if dj==0 and kj=='op' and tj==';': break
            if dj==0 and kj=='close' and tj=='}' and (lead in BLOCK or lead=='{'):
                # the block statement ends here unless continued
                nx=T[j+1] if j+1<n else None
                if nx is None: break
                if nx[1]=='id' and nx[2] in CONT: j+=1;continue
                if lead=='do' : j+=1;continue
                # function/class declarations end at their body; if/for/... at a closing brace followed by newline or a statement start
                if lead in ('function','class') and T[s][2] in('function','class','async'): break
                if '\n' in src[bj:nx[3]] or (nx[1]=='id'): break
            if j>s and dj==0:
                p=T[j-1]
                if j-1!=head and '\n' in src[p[4]:aj] and p[0]==0 and (p[1] in('id','num','str','re') or (p[1]=='close')) and kj=='id' and tj not in('in','instanceof','of') and not(p[1]=='id' and p[2] in KW_RE):
                    if not(p[1]=='close' and p[2]=='}' and lead not in BLOCK and lead!='{' and False):
                        j-=1;break
            j+=1
        out.append((s,min(j,n-1)));i=j+1
    return out

def local_names(T, s, e):
    """names a statement declares inside itself (its functions' parameters, its own const/let/var/function/class, catch's): not
    references to the page's top-level names"""
    L = set()
    for j in range(s, e + 1):
        d, k, t, x, y = T[j]
        if d == 0: continue
        if k == 'id' and t in ('const', 'let', 'var', 'function', 'class') and j + 1 <= e and T[j + 1][1] == 'id': L.add(T[j + 1][2])
        if k == 'op' and t == '=>':
            if T[j - 1][1] == 'id': L.add(T[j - 1][2])
            elif T[j - 1][2] == ')':
                q, dd = j - 2, T[j - 1][0]
                while q > s and not (T[q][1] == 'open' and T[q][0] == dd):
                    if T[q][1] == 'id' and T[q][0] == dd + 1: L.add(T[q][2])
                    q -= 1
        if k == 'id' and t == 'function':
            q = j + 1
            if T[q][1] == 'id': q += 1
            if T[q][2] == '(':
                dd = T[q][0]; q += 1
                while not (T[q][1] == 'close' and T[q][0] == dd):
                    if T[q][1] == 'id' and T[q][0] == dd + 1: L.add(T[q][2])
                    q += 1
        if k == 'id' and t == 'catch' and T[j + 1][2] == '(' and T[j + 2][1] == 'id': L.add(T[j + 2][2])
    return L

def refs(T, s, e):
    """the names a statement reads that may be the page's top-level ones (not a property's name, an object's key or its own)"""
    loc, r = local_names(T, s, e), set()
    for j in range(s, e + 1):
        d, k, t, x, y = T[j]
        if k != 'id' or t in loc or t in KW_ALL: continue
        if j > 0 and T[j - 1][1] == 'op' and T[j - 1][2] in ('.', '?.'): continue
        if j + 1 < len(T) and T[j + 1][2] == ':' and j > 0 and T[j - 1][2] in ('{', ','): continue
        r.add(t)
    return r
KW_ALL = set('break case catch class const continue debugger default delete do else export extends false finally for function if import in instanceof let new null of return static super switch this throw true try typeof var void while with yield async await undefined'.split())

def analyse(src, T):
    """the page's top-level statements: dicts of kind (fn: a function declaration; val: a const/let/var of one name whose value is a
    function; var: other declarations; other), names, where (start, end, value start), module and what it reads"""
    S = statements(src, T); marks = []
    for m in re.finditer(r'__svcMod\((["\'])([^"\']+)\1\);', src): marks.append((m.start(), m.group(2)))
    import bisect
    pos = [m[0] for m in marks]
    vals = {it[2]: it for it in items(src, T)}   # (by start: the items() reading of fn and val)
    out = []
    for s, e in S:
        a, b = T[s][3], T[e][4]; k = bisect.bisect(pos, a) - 1; mod = marks[k][1] if k >= 0 else 'base'
        d0 = T[s]; st = {'s': s, 'e': e, 'a': a, 'b': b, 'mod': mod, 'names': [], 'kind': 'other', 'v': a}
        it = vals.get(a)
        if it and it[3] == b or (it and it[3] == b + 0): st['kind'] = it[0]; st['names'] = [it[1]]; st['v'] = it[4]
        elif it and it[0] == 'fn': st['kind'] = 'fn'; st['names'] = [it[1]]
        elif d0[1] == 'id' and d0[2] in ('const', 'let', 'var', 'class'):
            names, j, want = [], s + 1, True
            if d0[2] == 'class': names = [T[s + 1][2]] if T[s + 1][1] == 'id' else []
            else:
                while j <= e:
                    if T[j][0] == 0 and want and T[j][1] == 'id': names.append(T[j][2]); want = False
                    elif T[j][0] == 0 and want and T[j][1] == 'open': names = None; break   # (destructuring: kept as "other")
                    elif T[j][0] == 0 and T[j][1] == 'op' and T[j][2] == ',': want = True
                    j += 1
            if names: st['kind'] = 'var'; st['names'] = names
        out.append(st)
    return out

LOADER = r"""/* (1.6.2 Part 38, lean62.py) the rest of the page: what this window does not need, one file (__LNU) read and run the first time any of
   its functions is called (window.__leanRead says which, and when); their stubs then call them, and a wrapper the page put over a stub
   stays the one the page calls */
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

SETDEF = re.compile(r'if\((S\.\w+===undefined|!S\.\w+)\)S\.\w+=')

def build(page, spec, out, what):
    """page: the whole page as built; spec: the small page's list (below: mods, the modules kept whole; fns, the functions of the page's
    own code (base) kept; out, the statements of the page's own code left out, each by the text it starts with); out(data_url): a file in
    www/a/ and its name. Gives (the small page, its ID, the numbers)"""
    a, z = script(page); src = page[a:z]; T = scan(src); A = analyse(src, T)
    have = {m for st in A for m in [st['mod']]}
    miss = [m for m in spec['mods'] if m not in have]
    assert not miss, '%s: the list names modules the page does not have: %s' % (what, ' '.join(miss))
    fnames = {}
    for i, st in enumerate(A):
        if st['kind'] in ('fn', 'val'): fnames.setdefault(st['names'][0], []).append(i)
    miss = [n for n in spec['fns'] if n not in fnames]
    assert not miss, '%s: the list names functions the page does not have (renamed or removed?): %s' % (what, ' '.join(miss))
    outs = {}
    for p in spec.get('out', []):
        w = [i for i, st in enumerate(A) if st['mod'] == 'base' and src.startswith(p, st['a'])]
        assert len(w) == 1, '%s: the statement left out that starts with %r is in the page %d times' % (what, p, len(w))
        outs[w[0]] = p
    # (a function declared twice: the last is the page's; the others never run)
    dead = {i for n, L in fnames.items() for i in L[:-1] if A[i]['kind'] == 'fn'}
    keepf = set(spec['fns']); noop = set(spec.get('noop', []))
    miss = [n for n in noop if n not in fnames]
    assert not miss, '%s: the list names functions the page does not have (renamed or removed?): %s' % (what, ' '.join(miss))
    inp = set()
    for i, st in enumerate(A):
        if i in dead: continue
        if st['mod'] in spec['mods']: inp.add(i)
        elif src.startswith('regApp(', st['a']) or src.startswith('shortcut(', st['a']): inp.add(i)   # (every app's registration and every shortcut's: the page lists them as the whole page does)
        elif st['kind'] == 'other' and (SETDEF.match(src, st['a']) or (refs(T, st['s'], st['e']) & {'DEFS', 'DKEYS', 'SKEYS', 'AKEYS'})): inp.add(i)   # (every setting's default: the page saves the settings as the whole page does)
        elif st['mod'] == 'base':
            if st['kind'] == 'other' and i not in outs: inp.add(i)
            elif st['kind'] in ('fn', 'val') and st['names'][0] in keepf: inp.add(i)
    # what the page's statements read: their variables in the page too (and what those read), wherever they are; functions stay out
    # (stubs) unless listed
    decl = {}
    for i, st in enumerate(A):
        if st['kind'] == 'var':
            for n in st['names']: decl.setdefault(n, []).append(i)
    R = {}
    todo = list(inp); seen = set()
    while todo:
        i = todo.pop()
        if i in seen: continue
        seen.add(i); st = A[i]
        if i not in R: R[i] = refs(T, st['s'], st['e'])
        for n in R[i]:
            for j in decl.get(n, []):
                if j not in inp: inp.add(j); todo.append(j)
    read = set().union(*[R[i] for i in inp]) if inp else set()
    rest, body, fns, stubv = [], [], [], set()
    stub = lambda n: 'function(){return __lnm(%s,this,arguments,new.target)}' % json.dumps(n)
    for i, st in enumerate(A):
        t = src[st['a']:st['b']]
        if i in dead: continue
        n = st['names'][0] if st['names'] else None
        if i in inp and not (n in noop and st['kind'] in ('fn', 'val')): body.append(t); continue
        if n in noop and st['kind'] in ('fn', 'val'):   # (the whole desktop's work, done by the shell's hidden page: nothing here)
            rest.append(t if st['kind'] == 'fn' else '')
            asy = src.startswith('async', st['v'] if st['kind'] == 'val' else st['a'])   # (an async one: a promise of nothing, as it would give)
            f = 'function(){%s}' % ('return Promise.resolve()' if asy else '')
            body.append(('function %s(){%s}' % (n, 'return Promise.resolve()' if asy else '')) if st['kind'] == 'fn' else src[st['a']:st['v']] + f + ';')
        elif st['kind'] == 'fn': rest.append(t); fns.append(n); body.append('function %s(){return __lnm(%s,this,arguments,new.target)}' % (n, json.dumps(n)))
        elif st['kind'] == 'val' and n in read:
            v = src[st['v']:st['b']].rstrip(); v = v[:-1] if v.endswith(';') else v
            rest.append('__LNR[%s]=(%s);' % (json.dumps(n), v)); body.append(src[st['a']:st['v']] + stub(n) + ';')
        else: rest.append(t)
    u = out('data:text/javascript;base64,' + base64.b64encode('\n'.join(rest).encode()).decode())
    code = LOADER % (json.dumps(fns, separators=(',', ':')), json.dumps(u)) + '\n'.join(body) + '\n'
    small = page[:a] + code + page[z:]
    small = small.replace("<script>/* (1.6.0, the owner's Vivobook report 1)", "<script>window.__OAPAGE=1;window.__LEAN=%s;/* (1.6.0, the owner's Vivobook report 1)" % json.dumps(what), 1)
    assert 'window.__OAPAGE=1;' in small
    h = out('data:text/html;base64,' + base64.b64encode(small.encode()).decode())
    hid = re.match(r'a/([0-9a-f]{16})\.html$', h).group(1)
    return small, hid, {'statements': len(A), 'kept': len(inp), 'script': len(src), 'small': len(code), 'rest': sum(map(len, rest)), 'stubs': len(fns)}

def redirect(page, hid):
    """the whole page with the small page's name in it (__LEAN_TM, which the hidden page tells the native shell: lean62.js), and Task
    Manager's window sent there if it opened the whole page all the same (the native shell's handler only; not &full=1, not a small page)"""
    go = ("<script>/* (1.6.2, docs/plan-1.6.2.md Part 38) Task Manager's small page: lean62.py */window.__LEAN_TM='%s';"
          "(()=>{try{if(window.__OAPAGE||/[?&]oa=/.test(location.search))return;const q=new URLSearchParams(String(location.hash||'').replace(/^#/,''));"
          "if(q.get('app')!='taskmgr'||q.get('full')=='1'||!(window.webkit&&window.webkit.messageHandlers&&window.webkit.messageHandlers.rudolfNative))return;"
          "const x=new XMLHttpRequest();x.open('HEAD','a/%s.html',false);x.send();if(x.status!=200)return;window.stop();location.replace('/?oa=%s'+location.hash)}catch(e){}})()</script>\n"
          % (hid, hid, hid))
    a = page.index('<head><meta charset="utf-8">\n') + len('<head><meta charset="utf-8">\n')
    o = page.find("the native desktop's apps' page for an app's window: oapage61.py */", a)   # (after the apps' page's: the first a/ID.html the page names stays that one)
    if o >= 0: a = page.index('</script>\n', o) + len('</script>\n')
    return page[:a] + go + page[a:]
