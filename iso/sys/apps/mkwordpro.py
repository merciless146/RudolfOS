#!/usr/bin/env python3
"""rudolf-wordpro's generated part (1.6.3, docs/plan-1.6.3.md Part 1: the Office apps native), made at build time from
the web WordPro itself, so the native app's lists and menus are the page's and never drift from it:

    python3 mkwordpro.py OUT.h

(run by iso/sys/ui/build.sh when it builds "wordpro", beside mkui.py's header)

  - the paragraph styles and their names (base_v24.html's WP_STYLES), in the page's order;
  - the fonts (WP_FONTS), in the page's order;
  - the menu bar (MENUS): each menu's name, and each item's label and key (Ctrl+N, Ctrl+S...), with the separators
    where the page has them. Every item is given an id here (WPI_<MENU>_<LABEL>), so the app says which command it
    has by naming the id: a label the page renames follows the page, and an item the page drops (or the whole menu
    bar) stops the build instead of leaving a menu behind. The page's fourth field is its tick (the current zoom, the
    layout chosen): it is not read here yet, because the native menu bar has no field to show it, and the app's own
    state is where that tick will come from.

Values the page changes are read here, never copied by hand: a list or a menu that cannot be found, or an item whose
label is not a plain string in the page, is an error, never a quiet default.
GPL-2.0-or-later."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ISO = os.path.dirname(os.path.dirname(HERE)); TOP = os.path.dirname(ISO)
PAGE = os.path.join(TOP, 'desktop', 'base_v24.html')


def die(m):
    sys.stderr.write('mkwordpro.py: ' + m + '\n')
    sys.exit(1)


def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def close_of(s, i, open_c, close_c):
    """the index of the bracket that closes s[i] (a quote-aware scan: '...', "..." and `...` with its ${...})"""
    depth = 0
    q = None
    while i < len(s):
        c = s[i]
        if q:
            if c == '\\':
                i += 2; continue
            if q == '`' and c == '$' and i + 1 < len(s) and s[i + 1] == '{':
                i = close_of(s, i + 1, '{', '}') + 1; continue
            if c == q:
                q = None
        elif c in '\'"`':
            q = c
        elif c == open_c:
            depth += 1
        elif c == close_c:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    die('a "%s" is not closed in the page' % open_c)


def split_top(s):
    """the parts of s at its own level, split on the commas a person wrote (never inside a string or a bracket)"""
    out, start, depth, q, i = [], 0, 0, None, 0
    while i < len(s):
        c = s[i]
        if q:
            if c == '\\':
                i += 2; continue
            if q == '`' and c == '$' and i + 1 < len(s) and s[i + 1] == '{':
                i = close_of(s, i + 1, '{', '}') + 1; continue
            if c == q:
                q = None
        elif c in '\'"`':
            q = c
        elif c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
        elif c == ',' and depth == 0:
            out.append(s[start:i]); start = i + 1
        i += 1
    out.append(s[start:])
    return [p.strip() for p in out]


def lit(s, what):
    """a plain '...' string in the page (labels and keys are written that way; anything computed is refused)"""
    m = re.fullmatch(r"'([^'\\]*)'", s.strip())
    if not m:
        die('%s is not a plain string in the page (%s): the page\'s own text cannot be read here' % (what, s.strip()[:60]))
    return m.group(1)


def label_of_spread(expr, var):
    """the label a page's spread map builds: 'Zoom '+z+'%' with z the map's own variable"""
    i, out = 0, []
    while i < len(expr):
        c = expr[i]
        if c == '+':
            i += 1; continue
        if c in '\'"':
            j = expr.index(c, i + 1); out.append(expr[i + 1:j]); i = j + 1; continue
        m = re.match(r'[A-Za-z_$][\w$]*', expr[i:])
        if m and m.group(0) == var:
            out.append('\0'); i += len(m.group(0)); continue
        die('a menu item built by the page ("%s") is not a label this can read' % expr.strip()[:80])
    return ''.join(out)


def slug(s):
    t = re.sub(r'[^A-Za-z0-9]+', '_', s).strip('_').upper()
    return t or 'ITEM'


def menus(page):
    if page.count('const MENUS') != 1:
        die("base_v24.html has %d MENUS (the menu bar): one is read here, and the page must have exactly one"
            % page.count('const MENUS'))
    i = page.find('const MENUS=')
    b = page.index('{', i)
    e = close_of(page, b, '{', '}')
    body = page[b + 1:e]
    out, pos = [], 0
    while pos < len(body):
        m = re.compile(r"\s*,?\s*(\w+)\s*:\s*\(\s*\)\s*=>\s*\[").match(body, pos)
        if not m:
            if body[pos:].strip(' \n,;'):
                die('the page\'s MENUS has a part this cannot read at %r' % body[pos:pos + 60])
            break
        name = m.group(1)
        a = m.end() - 1
        z = close_of(body, a, '[', ']')
        items = []
        for el in split_top(body[a + 1:z]):
            el = el.strip()
            if not el:
                continue
            if el == "'-'":
                items.append((None, None, None)); continue
            if el.startswith('...'):
                sm = re.fullmatch(r"\.\.\.\[([0-9,\s]+)\]\.map\(\s*(\w+)\s*=>\s*\[\s*(.+?)\s*,\s*'[^']*'\s*,.*\)", el, re.S)
                if not sm:
                    die('%s has an item built by the page this cannot read: %s' % (name, el[:70]))
                nums = [int(x) for x in sm.group(1).replace(' ', '').split(',') if x]
                tmpl = label_of_spread(sm.group(3), sm.group(2))
                for n in nums:
                    items.append((tmpl.replace('\0', str(n)), '', None))
                continue
            if not el.startswith('['):
                die('%s has an item this cannot read (not a [label, key] pair): %s' % (name, el[:70]))
            inner = close_of(el, 0, '[', ']')
            parts = split_top(el[1:inner])
            if len(parts) < 2:
                die('%s has an item with only a label: %s' % (name, el[:70]))
            lab = lit(parts[0], '%s\'s item label' % name)
            if not lab.strip():
                die('%s has an item with no label in the page' % name)
            items.append((lab, lit(parts[1], '%s\'s item key' % name), None))
        if not any(lab for lab, _, _ in items):
            die('%s has no items in the page: a menu the page emptied is not read' % name)
        if name in [n for n, _ in out]:
            die("the page's MENUS names %s twice" % name)
        out.append((name, items))
        pos = z + 1
    if len(out) < 4:
        die("the page's MENUS has %d menus: not read" % len(out))
    return out


def main(out):
    if not os.path.isfile(PAGE):
        die('the page (%s) is not here' % PAGE)
    page = open(PAGE, encoding='utf-8').read()
    m = re.search(r"const WP_STYLES=(\[\[.*?\]\]);", page, re.S)
    if not m:
        die("base_v24.html's WP_STYLES (the paragraph styles) was not found")
    styles = re.findall(r"\['([^']*)','([^']*)'\]", m.group(1))
    if len(styles) < 5:
        die("WP_STYLES has %d styles: not read" % len(styles))
    m = re.search(r"const WP_FONTS=(\[.*?\]);", page, re.S)
    if not m:
        die("base_v24.html's WP_FONTS (the fonts) was not found")
    fonts = re.findall(r"'([^']+)'", m.group(1))
    if len(fonts) < 5:
        die("WP_FONTS has %d fonts: not read" % len(fonts))
    M = menus(page)

    o = ['/* (generated by iso/sys/apps/mkwordpro.py at build time from the web WordPro: never by hand) */']
    o.append('typedef struct { const char *k, *name; } WpStyle;')
    o.append('static const WpStyle WP_STYLES[] = {')
    for k, n in styles:
        o.append('\t{ %s, %s },' % (cstr(k), cstr(n)))
    o.append('};')
    o.append('#define NWPSTYLES %d' % len(styles))
    o.append('static const char *const WP_FONTS[] = {')
    for f in fonts:
        o.append('\t%s,' % cstr(f))
    o.append('};')
    o.append('#define NWPFONTS %d' % len(fonts))

    # ---- the menu bar: an id per item, so the app names the commands it has instead of copying the page's text
    ids = []
    seen = {}
    for name, items in M:
        for lab, key, _ in items:
            if lab is None:
                continue
            nid = 'WPI_%s_%s' % (slug(name), slug(lab))
            if nid in seen:
                die("two items of the page's %s menu would share the id %s" % (name, nid))
            seen[nid] = 1
            ids.append((nid, name, lab, key))
    o.append('/* (the menu bar\'s items: the page\'s labels and keys, each with the id the app dispatches by) */')
    o.append('typedef struct { const char *label, *key; int id; } WpItem;')
    o.append('typedef struct { const char *title; const WpItem *items; int n; } WpMenu;')
    o.append('enum { WPI_NONE = 0,')
    for k, (nid, _, _, _) in enumerate(ids):
        o.append('\t%s = %d%s' % (nid, k + 1, ',' if k + 1 < len(ids) else ''))
    o.append('};')
    for name, items in M:
        o.append('static const WpItem WP_MENU_%s[] = {' % slug(name))
        for lab, key, _ in items:
            if lab is None:
                o.append('\t{ NULL, NULL, 0 },')
            else:
                o.append('\t{ %s, %s, WPI_%s_%s },' % (cstr(lab), cstr(key), slug(name), slug(lab)))
        o.append('};')
    o.append('static const WpMenu WP_MENUS[] = {')
    for name, items in M:
        o.append('\t{ %s, WP_MENU_%s, %d },' % (cstr(name), slug(name), len(items)))
    o.append('};')
    o.append('#define WPMENUS_N ((int)G_N_ELEMENTS(WP_MENUS))')
    open(out, 'w', encoding='utf-8').write('\n'.join(o) + '\n')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        die('usage: mkwordpro.py OUT.h')
    main(sys.argv[1])
