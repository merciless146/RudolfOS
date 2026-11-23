#!/usr/bin/env python3
"""librudolf-ui's generated part (1.6.2, docs/plan-1.6.2.md Part 39 step 1), made at build time from what the desktop already has, so
a native app looks and reads as its web version and never drifts from it:   python3 mkui.py OUT.h

  - the look: the page's styles (iso/www/index.html, its <style> elements in the page's order: base_v24.html's with styles25.css
    where build19.py puts it), each value read from the rule the page itself uses for it (THEME below: the selector as the page
    writes it, the property; the last rule wins, !important first, as the cascade does for one selector), its var(--x) resolved
    with :root's values for the light theme and html.dark's over them for the dark one. Written as GTK 3 CSS (RUI_CSS_LIGHT,
    RUI_CSS_DARK: @AC@ the accent colour, @ACFG@ the writing on it, @FONT@ the Font setting's family, every "Npx" scaled at run
    time by the display scaling) and as numbers for the controls librudolf-ui draws itself (RUI_PAGE: the table, the right-click
    menu, the dialogs). A rule the page no longer has is an error here, never a quiet default;
  - the translations: the page's own language tables (base_v24.html's I18N_ROWS and the rows added after it, "English|es|pt|fr|
    de|ru|zh|ja|ko"), first row of an English text winning as in the page (RUI_TR);
  - the native desktop's own code that a native app needs too, taken from iso/sys/rudolf-native.c by its markers (as the host
    tests take its parts), not copied by hand: the JSON reader, the accent colour's rules (acVariant, acInk: accent56.js in C),
    light or dark (is_dark: the page's dynResolve) and the Font setting's family (fonts47.js uiFontStack: ui_family).
GPL-2.0-or-later."""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__)); ISO = os.path.dirname(os.path.dirname(HERE)); TOP = os.path.dirname(ISO)
PAGE = os.path.join(ISO, 'www', 'index.html'); BASE = os.path.join(TOP, 'desktop', 'base_v24.html'); NAT = os.path.join(ISO, 'sys', 'rudolf-native.c')
def die(m): sys.stderr.write('mkui.py: ' + m + '\n'); sys.exit(1)

# ---------------------------------------------------------------- the page's styles
html = open(PAGE, encoding='utf-8').read()
CSS = '\n'.join(m.group(1) for m in re.finditer(r'<style[^>]*>(.*?)</style>', html, re.S))
CSS = re.sub(r'/\*.*?\*/', '', CSS, flags=re.S)
RULES = []   # (selectors, {prop: (value, important)}) in the page's order; @media and the like left out (their rules are for other cases)
def walk(t):
    i = 0
    while True:
        b = t.find('{', i)
        if b < 0: return
        sel = t[i:b].strip()
        depth, j = 1, b + 1
        while depth and j < len(t):
            depth += {'{': 1, '}': -1}.get(t[j], 0); j += 1
        body = t[b + 1:j - 1]
        if not sel.startswith('@'):
            decl = {}
            for d in body.split(';'):
                if ':' not in d: continue
                k, v = d.split(':', 1); k = k.strip(); v = v.strip(); imp = v.endswith('!important')
                if imp: v = v[:-10].strip()
                decl[k] = (v, imp)
            RULES.append(([re.sub(r'\s+', ' ', s.strip()) for s in sel.split(',')], decl))
        i = j
walk(CSS)
def get(sel, prop, need=True):
    """the value the page gives PROP for exactly SEL: the last rule naming it, an !important one before the others"""
    found, imp = None, False
    for sels, decl in RULES:
        if sel in sels and prop in decl:
            v, i = decl[prop]
            if i or not imp: found, imp = v, i
    if found is None and need: die(f'the page has no "{prop}" for "{sel}" any more: librudolf-ui follows the page (THEME)')
    return found
def vars_of(dark):
    """:root's custom properties, then (dark) html.dark's over them: html.dark is the more specific, wherever the page has it"""
    V = {}
    for want in [':root'] + (['html.dark'] if dark else []):
        for sels, decl in RULES:
            if want in sels:
                for k, (v, _) in decl.items():
                    if k.startswith('--'): V[k] = v
    return V
def res(v, V):
    """var(--x[, fallback]) resolved; --ac and --acfg are the account's (run time)"""
    for _ in range(5):
        m = re.search(r'var\((--[\w-]+)\s*(?:,\s*([^()]*))?\)', v)
        if not m: break
        n = m.group(1); r = {'--ac': '@AC@', '--acfg': '@ACFG@'}.get(n) or V.get(n) or (m.group(2) or '').strip()
        if not r: die('var(%s) has no value in the page' % n)
        v = v[:m.start()] + r + v[m.end():]
    return v
def col(v):
    """a colour as GTK's CSS reads it (#rgba and #rrggbbaa as rgba())"""
    v = v.strip()
    m = re.fullmatch(r'#([0-9a-fA-F]{8})', v) or re.fullmatch(r'#([0-9a-fA-F]{4})', v)
    if m:
        h = m.group(1); h = ''.join(c * 2 for c in h) if len(h) == 4 else h
        return 'rgba(%d,%d,%d,%.3f)' % (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), int(h[6:8], 16) / 255)
    return v

# THEME: what librudolf-ui takes from the page, by the page's own selectors. name: (selector, property[, dark selector])
THEME = {
    'wbg': (':root', '--wbg'), 'bg2': (':root', '--bg2'), 'bd': (':root', '--bd'), 'mut': (':root', '--mut'), 'fg': (':root', '--fg'),
    'body_dark_bg': ('html.dark .win .body', 'background'), 'body_dark_fg': ('html.dark .win .body', 'color'),
    'btn_bg': ('button', 'background'), 'btn_bd': ('button', 'border'), 'btn_r': ('button', 'border-radius'), 'btn_pad': ('button', 'padding'),
    'btn_hov': ('button:hover', 'background'), 'btn_hovbd': ('button:hover', 'border-color'), 'btn_act': ('button:active', 'background'),
    'btn_dis': ('button:disabled', 'opacity'), 'btn_fg': ('button', 'color'),
    'btn_dark': ('html.dark .win .body button:not(.pri):not(.qst):not(.swb)', 'background'), 'btn_dark_fg': ('html.dark .win .body button:not(.pri):not(.qst):not(.swb)', 'color'),
    'btn_dark_bd': ('html.dark .win .body button:not(.pri):not(.qst):not(.swb)', 'border-color'),
    'pri_bg': ('button.pri', 'background'), 'pri_fg': ('button.pri', 'color'), 'pri_bd': ('button.pri', 'border-color'),
    'in_bd': ('input:not([type])', 'border'), 'in_r': ('input:not([type])', 'border-radius'), 'in_pad': ('input:not([type])', 'padding'),
    'in_bg': ('input:not([type])', 'background'), 'in_fg': ('input:not([type])', 'color'),
    'in_dark_bg': ('html.dark .win .body input:not([type=checkbox]):not([type=radio]):not([type=range]):not([type=color])', 'background'),
    'in_dark_fg': ('html.dark .win .body input:not([type=checkbox]):not([type=radio]):not([type=range]):not([type=color])', 'color'),
    'in_dark_bd': ('html.dark .win .body input:not([type=checkbox]):not([type=radio]):not([type=range]):not([type=color])', 'border-color'),
    'focus': (':focus-visible', 'outline'), 'focus_off': (':focus-visible', 'outline-offset'),
    'tabs_bg': ('.tmtabs', 'background'), 'tabs_bd': ('.tmtabs', 'border-bottom'), 'tabs_pad': ('.tmtabs', 'padding'), 'tabs_gap': ('.tmtabs', 'gap'),
    'tab_r': ('.tmtabs button', 'border-radius'), 'tab_pad': ('.tmtabs button', 'padding'), 'tab_on': ('.tmtabs button.on', 'background'), 'tab_onw': ('.tmtabs button.on', 'font-weight'),
    'h4_fs': ('.sh4', 'font-size'), 'h4_fg': ('.sh4', 'color'), 'h4_ls': ('.sh4', 'letter-spacing'), 'h4_m': ('.sh4', 'margin'), 'h4_dark': ('html.dark .sh4', 'color'),
    'msg_fs': ('.msg', 'font-size'), 'msg_mh': ('.msg', 'min-height'), 'msg_dark': ('html.dark .msg', 'color'),
    'th_bg': ('.tmt th', 'background'), 'th_bd': ('.tmt th', 'border-bottom'), 'th_pad': ('.tmt th', 'padding'), 'th_dark': ('html.dark .tmt th', 'background'),
    'td_pad': ('.tmt td', 'padding'), 'td_bd': ('.tmt td', 'border-bottom'), 'tr_on': ('html body .tmt tbody tr.on td', 'background'), 'tr_onfg': ('html body .tmt tbody tr.on td', 'color'),
    'tr_hov': (':root', '--selhov'), 'tmt_fs': ('.tmt', 'font-size'),
    'ctx_bg': ('.ctx', 'background'), 'ctx_bd': ('.ctx', 'border'), 'ctx_r': ('.ctx', 'border-radius'), 'ctx_pad': ('.ctx', 'padding'), 'ctx_minw': ('.ctx', 'min-width'), 'ctx_fs': ('.ctx', 'font-size'),
    'ctx_dark_bg': ('html.dark .ctx', 'background'), 'ctx_dark_fg': ('html.dark .ctx', 'color'), 'ctx_dark_bd': ('html.dark .ctx', 'border-color'),
    'cmi_pad': ('.cmenu .cmi', 'padding'), 'cmi_gap': ('.cmenu .cmi', 'gap'), 'cmi_r': ('.ctx div', 'border-radius'), 'cmi_i': ('.cmenu .cmi i', 'width'),
    'cmi_hi': ('.ctx.cmenu .cmi.hi', 'background'), 'cmi_dis': ('.ctx.cmenu .cmi.dis', 'opacity'),
    'cmh_font': ('.cmenu .cmh', 'font-weight'), 'cmh_fs': ('.cmenu .cmh', 'font-size'), 'cmh_fg': ('.cmenu .cmh', 'color'), 'cmh_pad': ('.cmenu .cmh', 'padding'),
    'kbd_font': ('.cmenu .cmi kbd', 'font'), 'kbd_op': ('.cmenu .cmi kbd', 'opacity'), 'kbd_ml': ('.cmenu .cmi kbd', 'margin-left'),
    'kbd_bg': ('kbd', 'background'), 'kbd_bd': ('kbd', 'border'), 'kbd_r': ('kbd', 'border-radius'), 'kbd_pad': ('kbd', 'padding'),
    'hr_bd': ('.ctx hr', 'border-top'), 'hr_m': ('.ctx hr', 'margin'),
    'dlg_dim': ('.dlg', 'background'), 'dlg_bg': ('.dlg>div', 'background'), 'dlg_r': ('.dlg>div', 'border-radius'), 'dlg_pad': ('.dlg>div', 'padding'),
    'dlg_dark_bg': ('html.dark .dlg>div', 'background'), 'dlg_dark_fg': ('html.dark .dlg>div', 'color'),
    'rmbox_bg': ('.rmbox', 'background'), 'rmbox_r': ('.rmbox', 'border-radius'), 'rmbox_pad': ('.rmbox', 'padding'),
}
def px(v): m = re.match(r'(-?[\d.]+)px', v.strip()); return float(m.group(1)) if m else float(v) if re.fullmatch(r'-?[\d.]+', v.strip()) else 0.0
def box(v):
    p = [px(x) for x in v.split()]; p = p + [p[0]] * (1 - len(p)) if len(p) < 1 else p
    if len(p) == 1: p = p * 4
    elif len(p) == 2: p = [p[0], p[1], p[0], p[1]]
    elif len(p) == 3: p = [p[0], p[1], p[2], p[1]]
    return p
def border(v, V):
    """'1px solid C' -> (1, C)"""
    w = re.search(r'([\d.]+)px', v); c = re.sub(r'([\d.]+px|solid|dashed)', '', v).strip()
    return (float(w.group(1)) if w else 1.0, col(res(c, V)))

def theme(dark):
    V = vars_of(dark); T = {}
    for k, (s, p) in THEME.items():
        v = get(s, p) if s != ':root' else V.get(p)
        if v is None: die(f'the page has no {p} for {"html.dark" if dark else ":root"}')
        T[k] = res(v, V)
    return T, V
L, VL = theme(False); D, VD = theme(True)

def css(T, dark):
    """GTK 3 CSS for the controls GTK draws (buttons, fields, the tabs, the writing), in the page's values"""
    bw, bc = border(T['btn_bd'], VD if dark else VL); iw, ic = border(T['in_bd'], VD if dark else VL); tw, tc = border(T['tabs_bd'], VD if dark else VL)
    fw, fc = border(T['focus'], VD if dark else VL)
    bg = col(T['wbg']); fg = col(T['body_dark_fg']) if dark else '@TX@'
    bb, bf, bbd = (col(T['btn_dark']), col(T['btn_dark_fg']), col(T['btn_dark_bd'])) if dark else (col(T['btn_bg']), col(T['btn_fg']), bc)
    hov, hbd, act = (bb, bbd, bb) if dark else (col(T['btn_hov']), col(T['btn_hovbd']), col(T['btn_act']))
    ib, ifg, ibd = (col(T['in_dark_bg']), col(T['in_dark_fg']), col(T['in_dark_bd'])) if dark else (col(T['in_bg']), col(T['in_fg']), ic)
    p = box(T['btn_pad']); q = box(T['in_pad']); tp = box(T['tabs_pad']); tbp = box(T['tab_pad']); tr = box(T['tab_r'])
    h4 = col(T['h4_dark'] if dark else T['h4_fg']); msg = col(T['msg_dark']) if dark else 'inherit'
    dbg = col(T['dlg_dark_bg'] if dark else T['dlg_bg']); dfg = col(T['dlg_dark_fg']) if dark else 'inherit'
    o = []
    o.append('* { all: unset; }')
    o.append(f'window, .rui-body {{ background-color: {bg}; color: {fg}; font-family: @FONT@; font-size: @FS@px; }}')
    o.append(f'.rui-body label, .rui-body entry, .rui-body button {{ font-family: @FONT@; font-size: @FS@px; }}')
    o.append(f'button.rui {{ background-color: {bb}; color: {bf}; border: {bw:g}px solid {bbd}; border-radius: {px(T["btn_r"]):g}px; padding: {p[0]:g}px {p[1]:g}px {p[2]:g}px {p[3]:g}px; }}')
    o.append(f'button.rui:hover {{ background-color: {hov}; border-color: {hbd}; }}')
    o.append(f'button.rui:active {{ background-color: {act}; }}')
    o.append(f'button.rui:disabled {{ opacity: {T["btn_dis"]}; }}')
    o.append(f'button.rui.pri, button.rui.pri:hover, button.rui.pri:active {{ background-color: @AC@; color: @PRIFG@; border-color: @AC@; }}')
    o.append(f'button.rui:focus {{ outline: {fw:g}px solid @AC@; outline-offset: {px(T["focus_off"]):g}px; -gtk-outline-radius: {px(T["btn_r"]) + px(T["focus_off"]):g}px; }}')
    o.append(f'entry.rui {{ background-color: {ib}; color: {ifg}; border: {iw:g}px solid {ibd}; border-radius: {px(T["in_r"]):g}px; padding: {q[0]:g}px {q[1]:g}px {q[2]:g}px {q[3]:g}px; caret-color: {ifg}; }}')
    o.append(f'entry.rui selection {{ background-color: @SELBG@; color: @SELFG@; }}')
    o.append(f'entry.rui:focus {{ outline: {fw:g}px solid @AC@; outline-offset: {px(T["focus_off"]):g}px; -gtk-outline-radius: {px(T["in_r"]) + px(T["focus_off"]):g}px; }}')
    o.append(f'.rui-tabs {{ background-color: {col(T["tabs_bg"])}; border-bottom: {tw:g}px solid {tc}; padding: {tp[0]:g}px {tp[1]:g}px {tp[2]:g}px {tp[3]:g}px; }}')
    o.append(f'.rui-tabs button {{ background-color: transparent; color: inherit; border-radius: {tr[0]:g}px {tr[1]:g}px {tr[2]:g}px {tr[3]:g}px; padding: {tbp[0]:g}px {tbp[1]:g}px {tbp[2]:g}px {tbp[3]:g}px; }}')
    o.append(f'.rui-tabs button.on {{ background-color: {bg}; font-weight: {T["tab_onw"]}; }}')
    o.append(f'.rui-h4 {{ font-size: {px(T["h4_fs"]):g}px; font-weight: bold; color: {h4}; letter-spacing: @LS@px; }}')
    o.append(f'.rui-msg {{ font-size: {px(T["msg_fs"]):g}px; color: {msg}; }}')
    o.append(f'.rui-frame {{ border: 1px solid {col(T["bd"])}; }}')
    o.append(f'.rui-dim {{ background-color: {col(T["dlg_dim"])}; }}')
    dp = box(T['dlg_pad'])
    o.append(f'.rui-dlg {{ background-color: {dbg}; color: {dfg}; border-radius: {px(T["dlg_r"]):g}px; padding: {dp[0]:g}px {dp[1]:g}px {dp[2]:g}px {dp[3]:g}px; }}')
    o.append(f'.rui-dlg .rui-t {{ font-size: 16px; font-weight: bold; }}')
    o.append(f'.rui-dlg .rui-m {{ color: {col(T["mut"])}; }}')
    o.append('tooltip, tooltip * { background-color: #111827; color: #f9fafb; border-radius: 4px; padding: 2px 4px; }')
    return '\n'.join(o) + '\n'

def cstr(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n"\n\t"') + '"'
out = []
out.append('/* (generated by iso/sys/ui/mkui.py at build time from the page\'s styles, its language tables and rudolf-native.c: never by hand) */')
out.append('static const char RUI_CSS_LIGHT[] =\n\t' + cstr(css(L, False)) + ';')
out.append('static const char RUI_CSS_DARK[] =\n\t' + cstr(css(D, True)) + ';')

# the numbers for what librudolf-ui draws itself
def rgba(v):
    v = col(v).strip()
    if v.startswith('@'): return None
    m = re.fullmatch(r'#([0-9a-fA-F]{6})', v) or re.fullmatch(r'#([0-9a-fA-F]{3})', v)
    if m:
        h = m.group(1); h = ''.join(c * 2 for c in h) if len(h) == 3 else h; return (int(h[0:2], 16) / 255, int(h[2:4], 16) / 255, int(h[4:6], 16) / 255, 1.0)
    m = re.fullmatch(r'rgba?\(([^)]*)\)', v)
    if m:
        a = [float(x) for x in re.split(r'[\s,/]+', m.group(1).strip())]; return (a[0] / 255, a[1] / 255, a[2] / 255, a[3] if len(a) > 3 else 1.0)
    m = re.fullmatch(r'color-mix\(in srgb,\s*(@AC@|#\w+)\s+(\d+)%,\s*transparent\)', v)
    if m: return ('AC', int(m.group(2)) / 100)
    if v == '#fff' or v == 'white': return (1, 1, 1, 1)
    die('a colour librudolf-ui cannot read: ' + v)
def crgba(v):
    c = rgba(v)
    if c is None: return '{0, 0, 0, -1}'   # (the accent colour, at run time)
    if c[0] == 'AC': return '{0, 0, 0, -%g}' % (1 + c[1])   # (the accent colour at that share: -1-share)
    return '{%.4f, %.4f, %.4f, %.4f}' % c
def page(T, dark):
    V = VD if dark else VL
    thw, thc = border(T['th_bd'], V); tdw, tdc = border(T['td_bd'], V); cw, cc = border(T['ctx_bd'], V); hw, hc = border(T['hr_bd'], V); kw, kc = border(T['kbd_bd'], V)
    f = {
        'wbg': crgba(T['wbg']), 'bg2': crgba(T['bg2']), 'bd': crgba(T['bd']), 'mut': crgba(T['mut']),
        'fg': crgba(T['body_dark_fg']) if dark else '{0, 0, 0, -2}',   # (light: the page's body colour, Display Properties' "tx")
        'th_bg': crgba(T['th_dark'] if dark else T['th_bg']), 'th_bd': crgba(thc), 'td_bd': crgba(tdc), 'tr_hov': crgba(T['tr_hov']),
        'ctx_bg': crgba(T['ctx_dark_bg'] if dark else T['ctx_bg']), 'ctx_fg': crgba(T['ctx_dark_fg']) if dark else '{0, 0, 0, -2}', 'ctx_bd': crgba(T['ctx_dark_bd'] if dark else cc),
        'cmh_fg': crgba(T['cmh_fg']), 'hr': crgba(T['ctx_dark_bd'] if dark else hc), 'kbd_bg': crgba(T['kbd_bg']), 'kbd_bd': crgba(T['ctx_dark_bd'] if dark else kc),
    }
    n = {
        'in_pad': box(T['in_pad']), 'btn_pad': box(T['btn_pad']), 'th_pad': box(T['th_pad']), 'td_pad': box(T['td_pad']), 'th_bw': [thw], 'td_bw': [tdw], 'tmt_fs': [px(T['tmt_fs'])],
        'ctx_pad': box(T['ctx_pad']), 'ctx_r': [px(T['ctx_r'])], 'ctx_minw': [px(T['ctx_minw'])], 'ctx_fs': [px(T['ctx_fs'])], 'ctx_bw': [cw],
        'cmi_pad': box(T['cmi_pad']), 'cmi_gap': [px(T['cmi_gap'])], 'cmi_r': [px(T['cmi_r'])], 'cmi_i': [px(T['cmi_i'])], 'cmi_dis': [float(T['cmi_dis'])],
        'cmh_fs': [px(T['cmh_fs'])], 'cmh_pad': box(T['cmh_pad']), 'kbd_op': [float(T['kbd_op'])], 'kbd_ml': [px(T['kbd_ml'])], 'kbd_fs': [px(re.search(r'[\d.]+px', T['kbd_font']).group(0))],
        'kbd_r': [px(T['kbd_r'])], 'kbd_pad': box(T['kbd_pad']), 'hr_m': box(T['hr_m']), 'hr_bw': [hw], 'msg_mh': [px(T['msg_mh'])], 'h4_m': box(T['h4_m']),
        'h4_ls': [float(re.match(r'[\d.]+', T['h4_ls']).group(0))], 'focus_w': [border(T['focus'], V)[0]], 'focus_off': [px(T['focus_off'])],
    }
    s = '\t{ ' + ', '.join(f'.{k} = {v}' for k, v in f.items()) + ',\n\t  ' + ', '.join(f'.{k} = {{{", ".join("%g" % x for x in v)}}}' for k, v in n.items()) + ' }'
    return s, f, n
pl, fl, nl = page(L, False); pd, _, _ = page(D, True)
out.append('typedef struct { double r, g, b, a; } RuiC;   /* a < 0: -1 the accent colour (-1-s: at the share s), -2 the writing colour */')
out.append('typedef struct { RuiC ' + ', '.join(fl.keys()) + '; double ' + ', '.join(f'{k}[{len(v)}]' for k, v in nl.items()) + '; } RuiPage;')
out.append('static const RuiPage RUI_PAGE[2] = {\n' + pl + ',\n' + pd + '\n};')

# ---------------------------------------------------------------- the translations
base = open(BASE, encoding='utf-8').read()
TR = {}; LANGS = ['es', 'pt', 'fr', 'de', 'ru', 'zh', 'ja', 'ko']; tables = 0
for m in re.finditer(r'=`([^`]*)`;', base):
    rows = [r for r in m.group(1).split('\n') if r.count('|') >= 8]
    if len(rows) < 5 or len(rows) < len(m.group(1).split('\n')) // 2: continue
    tables += 1
    for r in rows:
        c = r.split('|')
        if c[0] not in TR: TR[c[0]] = c[1:9]
if tables < 1 or 'Cancel' not in TR: die('the page\'s language tables (I18N_ROWS) were not found in base_v24.html')
out.append('static const char *const RUI_LANGS[] = { ' + ', '.join(cstr(x) for x in LANGS) + ', NULL };')
out.append('static const char *const RUI_TR[][9] = {')
for k in sorted(TR): out.append('\t{ ' + ', '.join(cstr(x) for x in [k] + TR[k]) + ' },')
out.append('};')

# ---------------------------------------------------------------- rudolf-native's own code, by its markers
nat = open(NAT, encoding='utf-8').read().split('\n')
def take(start, end_pred, what):
    s = next((i for i, l in enumerate(nat) if l.startswith(start)), None)
    if s is None: die(f'rudolf-native.c no longer has {what} where librudolf-ui takes it ("{start}")')
    e = next((i for i in range(s, len(nat)) if end_pred(nat[i], i - s)), None)
    if e is None: die(f'the end of {what} in rudolf-native.c was not found')
    return nat[s:e + 1]
blk = lambda l, n: n > 0 and l == '}'
parts = [
    take('typedef struct JV { int t;', lambda l, n: l.startswith('static JV *jat('), 'the JSON reader'),
    take('typedef struct { double r, g, b; } RGB;', lambda l, n: l.startswith('static RGB shade('), "the accent colour's rules"),
    take('static char UIFAM[96]', blk, "the Font setting's family"),
    take('static int is_dark(void)', blk, 'light or dark'),
]
out.append('/* ---- from iso/sys/rudolf-native.c (the native desktop\'s own: the same code in both) */')
out.append('#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored "-Wunused-function"')
for p in parts: out.append('\n'.join(p))
out.append('#pragma GCC diagnostic pop')
open(sys.argv[1], 'w', encoding='utf-8').write('\n'.join(out) + '\n')
