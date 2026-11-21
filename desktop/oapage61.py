# Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 23 B, stage 2): the native desktop's apps' page, made by build19.py.
# An app's window of the native desktop loads the page without the modules no app of it uses (OA_OUT below: they are other apps' or the
# shell's); each of them is a file of its own beside the page (www/a/, named by its contents), read and run the first time anything of
# it is used: its functions stand in the page as stubs that do so, its other names as getters that do so, its regApp lines stay. The
# whole page (index.html) is still every other page's: Standard's, Lite's, the shell's do- pages', and a window's when the apps' page is
# not there (a page installed by hand: its file names are its own, so the release's files are not taken for it).
import json, re

# the modules left out (each checked by top(): declarations and regApp lines only, none of whose names the page reads as it starts;
# iso/qt/host/apppage161_t.sh opens every app with the apps' page and fails on any of them read)
OA_OUT = ['wp25.js', 'mail25.js', 'wifi26.js', 'emusnes26.js', 'dlrm61.js', 'luau30.js', 'drivecopy60.js', 'hwcheck30.js', 'notey45.js',
          'weather47.js', 'setsearch59.js']
# the apps whose windows take the apps' page (not those that are, or use, what it leaves out)
OA_NOT = ['gameboy', 'wordpro', 'sheet', 'deck', 'settings', 'store', 'blitzen', 'blocklua', 'weather', 'taskmgr', 'browser']

KW_RE = set('return typeof instanceof in of new delete void throw case do else yield await'.split())
def toks(src):
    """the module's tokens that matter at its top level: (depth, kind, text, start, end); strings, templates, comments and regular
    expressions skipped whole"""
    i, n, d, out, prev = 0, len(src), 0, [], ''
    ident = re.compile(r'[A-Za-z_$][\w$]*')
    while i < n:
        c = src[i]
        if c in ' \t\r\n': i += 1; continue
        if src.startswith('//', i): j = src.find('\n', i); i = n if j < 0 else j; continue
        if src.startswith('/*', i): j = src.find('*/', i + 2); assert j >= 0; i = j + 2; continue
        if c in '"\'':
            j = i + 1
            while src[j] != c: j += 2 if src[j] == '\\' else 1
            out.append((d, 'str', '', i, j + 1)); i = j + 1; prev = 'str'; continue
        if c == '`':
            j, stack = i + 1, 0
            while True:
                if src[j] == '\\': j += 2; continue
                if src[j] == '`' and stack == 0: break
                if src.startswith('${', j):
                    # a substitution: skipped by bracket depth, strings inside it are rare in these modules and checked below
                    k, b = j + 2, 1
                    while b:
                        if src[k] == '{': b += 1
                        elif src[k] == '}': b -= 1
                        elif src[k] in '"\'`':
                            q = src[k]; k += 1
                            while src[k] != q: k += 2 if src[k] == '\\' else 1
                        k += 1
                    j = k; continue
                j += 1
            out.append((d, 'str', '', i, j + 1)); i = j + 1; prev = 'str'; continue
        if c == '/' and (prev in ('', 'op') or prev in KW_RE):
            j, cls = i + 1, False
            while True:
                if src[j] == '\\': j += 2; continue
                if src[j] == '[': cls = True
                elif src[j] == ']': cls = False
                elif src[j] == '/' and not cls: break
                assert src[j] != '\n', ('a regular expression across lines', src[i:i + 40])
                j += 1
            j += 1
            while j < n and (src[j].isalnum()): j += 1
            out.append((d, 're', '', i, j)); i = j; prev = 're'; continue
        m = ident.match(src, i)
        if m:
            out.append((d, 'id', m.group(0), i, m.end())); i = m.end(); prev = m.group(0) if m.group(0) in KW_RE else 'id'; continue
        if c.isdigit(): j = i
        if c.isdigit() or (c == '.' and i + 1 < n and src[i + 1].isdigit()):
            m = re.compile(r'[0-9a-fA-FxXoObBeE_.n]+').match(src, i); out.append((d, 'num', '', i, m.end())); i = m.end(); prev = 'num'; continue
        if c in '([{': out.append((d, 'open', c, i, i + 1)); d += 1; i += 1; prev = 'op'; continue
        if c in ')]}': d -= 1; out.append((d, 'close', c, i, i + 1)); i += 1; prev = 'close' if c != '}' else 'op'; continue
        out.append((d, 'op', c, i, i + 1)); i += 1; prev = 'op' if c not in ')]' else 'close'
    assert d == 0, 'brackets do not match'
    return out

def top(src, name):
    """the module's top-level statements: its functions' names, its other names, its regApp lines (start, end); anything else stops the
    build (the module cannot be left out)"""
    T = toks(src); fns, names, regs, i = [], [], [], 0
    while i < len(T):
        d, k, t, a, b = T[i]
        assert d == 0, (name, src[a:a + 60])
        if k == 'id' and t in ('function', 'async'):
            j = i + 1 if t == 'function' else i + 2
            assert T[j][1] == 'id', (name, src[a:a + 60]); fns.append(T[j][2])
            # to the end of its body: the first "}" back at depth 0 after its "{"
            while not (T[j][1] == 'open' and T[j][2] == '{' and T[j][0] == 0): j += 1
            j += 1
            while not (T[j][1] == 'close' and T[j][0] == 0): j += 1
            i = j + 1; continue
        if k == 'id' and t in ('const', 'let', 'var'):
            j = i + 1; want = True
            while j < len(T):
                dj, kj, tj = T[j][0], T[j][1], T[j][2]
                if dj == 0 and want:
                    assert kj == 'id', (name, src[T[j][3]:T[j][3] + 60]); names.append(tj); want = False
                elif dj == 0 and kj == 'op' and tj == ',': want = True
                elif dj == 0 and kj == 'op' and tj == ';': break
                j += 1
            i = j + 1; continue
        if k == 'id' and t == 'regApp':
            j = i + 1
            while not (T[j][0] == 0 and T[j][1] == 'op' and T[j][2] == ';'): j += 1
            regs.append((a, T[j][4])); i = j + 1; continue
        if k == 'op' and t == ';': i += 1; continue
        raise AssertionError('%s: a statement that is not a declaration: %s' % (name, src[a:a + 80]))
    return fns, names, regs

LOADER = r"""/* (1.6.1, the native desktop's apps' page: a module left out of it, read and run the first time anything of it is used; its functions'
   stubs then call its own, and a wrapper another module put over a stub stays the one the page calls) */
const __OAM=%s,__OAS={},__OAR={},__OAD={};for(const m in __OAM)for(const n of __OAM[m].f)__OAS[n]=globalThis[n];
function __oam(m,n,t,a,nt){if(!__OAD[m]){__OAD[m]=1;(window.__oaLazy=window.__oaLazy||[]).push(m+(n?' ('+n+')':''));console.warn('apps page: '+m+' read'+(n?' for '+n:''));
 const x=new XMLHttpRequest();x.open('GET',__OAM[m].u,false);x.send();if(x.status!=200){__OAD[m]=0;throw new Error(m+' could not be read ('+__OAM[m].u+')')}
 const cur={};for(const k of __OAM[m].f)cur[k]=globalThis[k];for(const k of __OAM[m].v)delete globalThis[k];
 const s=document.createElement('script');s.textContent=x.responseText;document.head.appendChild(s);s.remove();
 for(const k of __OAM[m].f){__OAR[k]=globalThis[k];if(cur[k]!==__OAS[k])globalThis[k]=cur[k]}}
 if(!n)return;const f=__OAR[n];if(typeof f!='function')throw new Error(n+' is not in '+m);return nt?Reflect.construct(f,a,nt):f.apply(t,a)}
"""

def build(s, L, out):
    """s: the page as built; L(name): a module's source; out(data_url): a file in www/a/, its name. Gives (the page with the loader that
    sends an app's window to the apps' page, the apps' page)"""
    import base64
    page, M = s, {}
    for m in OA_OUT:
        a0 = '__svcMod(%s);\n' % json.dumps(m); i = page.index(a0) + len(a0); j = page.index('__svcMod(', i); code = page[i:j]
        fns, names, regs = top(code, m)
        lazy, last = '', 0
        for a, b in regs: lazy += code[last:a]; last = b
        lazy += code[last:]
        u = out('data:text/javascript;base64,' + base64.b64encode(lazy.encode()).decode())
        M[m] = {'u': u, 'f': fns, 'v': names}
        st = ''.join('function %s(){return __oam(%s,%s,this,arguments,new.target)}\n' % (f, json.dumps(m), json.dumps(f)) for f in fns)
        st += ''.join("Object.defineProperty(globalThis,%s,{configurable:true,get(){__oam(%s);return (0,eval)(%s)}});\n" % (json.dumps(v), json.dumps(m), json.dumps(v)) for v in names)
        st += ''.join(code[a:b] + '\n' for a, b in regs)
        page = page[:i] + st + page[j:]
    # (the loader first in the page's code, after svc60.js's part)
    k = page.index('\nconst $=s=>document.querySelector(s);')
    page = page[:k] + '\n' + LOADER % json.dumps(M, separators=(',', ':')) + page[k:]
    page = page.replace('<script>/* (1.6.0, the owner\'s Vivobook report 1)', '<script>window.__OAPAGE=1;/* (1.6.0, the owner\'s Vivobook report 1)', 1)
    assert 'window.__OAPAGE=1;' in page
    h = out('data:text/html;base64,' + base64.b64encode(page.encode()).decode())
    hid = re.match(r'a/([0-9a-f]{16})\.html$', h).group(1)
    # the whole page: an app's window of the native desktop (WebKit's handler there), of an app not in OA_NOT, goes to the apps' page if
    # it is there (rudolfd gives it, as the page, for /?oa=ID); none: the whole page, as before
    go = ("<script>/* (1.6.1, docs/plan-1.6.1.md Part 23 B, stage 2) the native desktop's apps' page for an app's window: oapage61.py */"
          "(()=>{try{if(window.__OAPAGE||/[?&]oa=/.test(location.search))return;const q=new URLSearchParams(String(location.hash||'').replace(/^#/,'')),k=q.get('app')||'';"
          "if(!k||/^do-|:/.test(k)||%s.includes(k)||q.get('full')=='1'||!(window.webkit&&window.webkit.messageHandlers&&window.webkit.messageHandlers.rudolfNative))return;"
          "const x=new XMLHttpRequest();x.open('HEAD','a/%s.html',false);x.send();if(x.status!=200)return;window.stop();location.replace('/?oa=%s'+location.hash)}catch(e){}})()</script>\n"
          % (json.dumps(OA_NOT), hid, hid))
    a = s.index('<head><meta charset="utf-8">\n') + len('<head><meta charset="utf-8">\n')
    return s[:a] + go + s[a:], hid
