/* ===== Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 7): Oracle's reader mode =====
   On a page with an article (a news story, a blog post, a recipe) a 📖 button shows in the address box; it (or F9) shows the
   article alone in Rudolf's reading style: its title, author, date, text, pictures and links. A− A+, the width, the font (sans,
   serif, OpenDyslexic), the colours (light, sepia, dark: dark by default with Rudolf's dark theme), 🔊 Read aloud (Rudolf's own voice,
   tts25.js, the sentence being read highlighted), 🖨 Print (the article alone, print25.js) and Exit reader (or F9 again), which shows
   the page as it was. Per tab; the address stays the page's.
   - Whether a page has an article: the tab says (tabev readable, from a small check in the page when it has loaded: the shell's
     READABLE_JS, rudolf-webkit's the same); for Oracle's own pages (an .HTM file on C:) the desktop checks the file itself.
   - The article: the tab gives its page's HTML as it is now (tab op readhtml -> tabev readhtml {id, html, url, title}; a file's own
     HTML), which is parsed here into an inert document (DOMParser: nothing in it runs or loads) and read by Mozilla's Readability
     (libs/readability, Apache-2.0, loaded the first time: a file beside the page); its HTML goes through safeHtml (sanitize59.js),
     and forms, styles and media that play are left out. It is shown in the tab's own frame (srcdoc, sandboxed: scripts only, no
     origin of its own) with a Content-Security-Policy that lets only Oracle's own script (its nonce) run, pictures load (as the page
     loaded them) and nothing else. A link clicked opens in the tab, as Oracle's pages' links do.
   - Not for websites in plain frames (no shell): the page cannot be read there.
   browser() calls oraReader({...}) once per window. ===== */
const ORA_RDB_URL = '';   /* (desktop/build19.py: the file of libs/readability/Readability.js beside the page) */
let ORA_RDB_P = null;
function oraRdbLoad() {
  if (typeof Readability == 'function') return Promise.resolve();
  return ORA_RDB_P || (ORA_RDB_P = new Promise((res, rej) => { if (!ORA_RDB_URL) { ORA_RDB_P = null; return rej(new Error('Readability is not in this build')) }
    const s = document.createElement('script'); s.src = ORA_RDB_URL; s.async = false;
    s.onload = () => typeof Readability == 'function' ? res() : (ORA_RDB_P = null, rej(new Error('incomplete')));
    s.onerror = () => { ORA_RDB_P = null; s.remove(); rej(new Error('not found')) }; document.head.appendChild(s) }));
}
/* the article from a page's HTML (u: its address, for relative links and pictures): {title, byline, date, site, html, lang, dir} or null */
async function oraArticle(html, u) {
  await oraRdbLoad();
  const d = new DOMParser().parseFromString(String(html || ''), 'text/html');
  try { d.querySelectorAll('base').forEach(b => b.remove()); const b = d.createElement('base'); b.href = /^(https?|file):/i.test(u) ? u : 'about:blank'; (d.head || d.documentElement).prepend(b) } catch (e) {}
  let a = null; try { a = new Readability(d, {charThreshold: 300, keepClasses: false}).parse() } catch (e) { a = null }
  if (!a || !a.content || (a.length || 0) < 200) return null;
  /* made safe, then what a reader has no use for left out: forms and their fields, styles, media that would play */
  const box = new DOMParser().parseFromString('<!doctype html><body>' + safeHtml(a.content), 'text/html').body;
  box.querySelectorAll('form,input,button,select,textarea,style,audio,video,source,track,dialog,canvas').forEach(x => x.remove());
  box.querySelectorAll('[style],[class],[id],[width],[height],[srcset],[sizes],[loading],[target]').forEach(x => { ['style', 'class', 'id', 'width', 'height', 'srcset', 'sizes', 'loading', 'target'].forEach(n => x.removeAttribute(n)) });
  box.querySelectorAll('a[href]').forEach(x => { try { x.href = new URL(x.getAttribute('href'), u).href } catch (e) { x.removeAttribute('href') } });
  box.querySelectorAll('img[src]').forEach(x => { try { const v = new URL(x.getAttribute('src'), u).href; if (/^(https?:|data:image\/)/i.test(v)) x.src = v; else x.remove() } catch (e) { x.remove() } });
  return {title: String(a.title || '').slice(0, 300), byline: String(a.byline || '').slice(0, 200), date: String(a.publishedTime || '').slice(0, 40), site: String(a.siteName || '').slice(0, 100),
    html: safeHtml(box.innerHTML), lang: /^[a-z]{2,3}(-[A-Za-z0-9]{2,8})?$/.test(a.lang || '') ? a.lang : '', dir: a.dir == 'rtl' ? 'rtl' : 'ltr'};
}
/* whether a page's HTML holds an article (a file of Oracle's own): the same check as the tabs' (READABLE_JS) */
function oraReadable(doc) {
  try { const U = /-ad-|ai2html|banner|breadcrumbs|combx|comment|community|cover-wrap|disqus|extra|footer|gdpr|header|legends|menu|related|remark|replies|rss|shoutbox|sidebar|skyscraper|social|sponsor|supplemental|ad-break|agegate|pagination|pager|popup|yom-remote/i, C = /and|article|body|column|content|main|shadow/i;
    const L = new Set(doc.querySelectorAll('p,pre,article')); doc.querySelectorAll('div > br').forEach(b => L.add(b.parentNode)); let sc = 0;
    for (const n of L) { const m = (n.getAttribute('class') || '') + ' ' + (n.id || ''); if (U.test(m) && !C.test(m)) continue; if (n.matches && n.matches('li p')) continue;
      const l = (n.textContent || '').trim().length; if (l < 140) continue; sc += Math.sqrt(l - 140); if (sc > 20) return true } } catch (e) {}
  return false;
}
const oraRdEsc = s => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#39;');
/* the reader's page (srcdoc): the article, Oracle's own controls; only its own script (the nonce) may run */
function oraReaderPage(a, u, p, font) {
  const n = Array.from(crypto.getRandomValues(new Uint8Array(12)), x => x.toString(16).padStart(2, '0')).join('');
  let host = ''; try { host = new URL(u).host } catch (e) {}
  const meta = [a.byline, a.site || host, a.date && !isNaN(Date.parse(a.date)) ? new Date(a.date).toLocaleDateString() : ''].filter(Boolean).map(oraRdEsc).join(' · ');
  const css = `*{box-sizing:border-box}html{-webkit-text-size-adjust:100%}body{margin:0;background:var(--bg);color:var(--fg);font:var(--sz)/1.65 var(--ff)}
:root{--bg:#fdfdfc;--fg:#1f2328;--mu:#59636e;--ln:#0b62d6;--bar:#f3f4f6;--bd:#d1d5db;--hl:#fde68a;--sz:19px;--w:680px;--ff:Georgia,"Gelasio","Times New Roman",serif}
:root[data-th=sepia]{--bg:#f4ecd8;--fg:#433422;--mu:#7a6650;--ln:#8a4b08;--bar:#eadfc4;--bd:#cdbf9f;--hl:#f2c66d}
:root[data-th=dark]{--bg:#16181d;--fg:#d7dae0;--mu:#9aa1ab;--ln:#7cb7ff;--bar:#22252c;--bd:#3a3f48;--hl:#5b4a12}
:root[data-ff=sans]{--ff:"Fira Sans","Segoe UI",Tahoma,Arial,sans-serif}:root[data-ff=dys]{--ff:"OpenDyslexic",Arial,sans-serif}
#bar{position:sticky;top:0;z-index:5;display:flex;flex-wrap:wrap;gap:4px;align-items:center;padding:6px 10px;background:var(--bar);border-bottom:1px solid var(--bd);font:13px "Segoe UI",Tahoma,Arial,sans-serif}
#bar button,#bar select{font:inherit;padding:3px 9px;border:1px solid var(--bd);border-radius:6px;background:var(--bg);color:var(--fg);cursor:pointer}#bar .sp{flex:1}#bar button[aria-pressed=true]{background:var(--hl)}
main{max-width:var(--w);margin:0 auto;padding:24px 20px 80px}h1#t{font-size:1.75em;line-height:1.25;margin:.2em 0 .3em}#m{color:var(--mu);font-size:.8em;margin-bottom:1.6em}
main a{color:var(--ln)}main img{max-width:100%;height:auto;display:block;margin:1em auto}figure{margin:1.2em 0}figcaption{color:var(--mu);font-size:.8em}
pre,code{font-family:"DejaVu Sans Mono",monospace;font-size:.85em}pre{overflow:auto;padding:10px;background:var(--bar)}blockquote{margin:1em 0;padding-left:1em;border-left:3px solid var(--bd);color:var(--mu)}
table{border-collapse:collapse;max-width:100%}td,th{border:1px solid var(--bd);padding:4px 8px}::highlight(rd-say){background-color:var(--hl);color:inherit}
@media print{#bar{display:none}body{background:#fff;color:#000}}`;
  const ff = font ? `@font-face{font-family:'OpenDyslexic';src:url(${font}) format('woff2')}` : '';
  const js = `(function(){var P=${JSON.stringify(p).replace(/</g, '\\u003c')},R=document.documentElement,$=function(i){return document.getElementById(i)},say=function(m){parent.postMessage(m,'*')};
function ap(){R.dataset.th=P.th;R.dataset.ff=P.ff;R.style.setProperty('--sz',P.sz+'px');R.style.setProperty('--w',{n:'560px',m:'680px',w:'880px'}[P.w]||'680px');$('th').value=P.th;$('ff').value=P.ff;$('w').value=P.w}
function set(k,v){P[k]=v;ap();say({rdr:'prefs',p:P})}ap();
$('sm').onclick=function(){set('sz',Math.max(14,P.sz-2))};$('lg').onclick=function(){set('sz',Math.min(32,P.sz+2))};
$('th').onchange=function(){set('th',this.value)};$('ff').onchange=function(){set('ff',this.value)};$('w').onchange=function(){set('w',this.value)};
$('x').onclick=function(){say({rdr:'exit'})};$('pr').onclick=function(){say({rdr:'print'})};
var S=[],B=[].slice.call(document.querySelectorAll('main h1,main h2,main h3,main h4,main h5,main h6,main p,main li,main blockquote,main figcaption,main pre'));
B=B.filter(function(b){return !B.some(function(o){return o!==b&&o.contains(b)})});
B.forEach(function(b,bi){var t=b.textContent||'',re=/[^.!?\\u3002]+[.!?\\u3002]*\\s*/g,m;while((m=re.exec(t))){if(!m[0].trim())continue;S.push([bi,m.index,m.index+m[0].length,m[0].trim()])}});
function rng(s){var b=B[s[0]],w=document.createTreeWalker(b,NodeFilter.SHOW_TEXT),n,o=0,r=document.createRange(),st=0;while((n=w.nextNode())){var l=n.data.length;if(!st&&o+l>=s[1]){r.setStart(n,s[1]-o);st=1}if(st&&o+l>=s[2]){r.setEnd(n,s[2]-o);return r}o+=l}return null}
$('rd').onclick=function(){say(this.getAttribute('aria-pressed')=='true'?{rdr:'stop'}:{rdr:'read',s:S.map(function(x){return x[3]})})};
addEventListener('message',function(e){var d=e.data;if(e.source!==parent||!d)return;
 if(typeof d.rdhl=='number'){try{if(CSS.highlights){CSS.highlights.delete('rd-say');var s=S[d.rdhl],r=s&&rng(s);if(r){CSS.highlights.set('rd-say',new Highlight(r));var q=r.getBoundingClientRect();if(q.top<50||q.bottom>innerHeight)B[s[0]].scrollIntoView({block:'center'})}}}catch(x){}}
 if(typeof d.rdreading=='boolean'){$('rd').setAttribute('aria-pressed',String(d.rdreading));$('rd').textContent=d.rdreading?'⏹ Stop reading':'🔊 Read aloud'}});
document.addEventListener('click',function(e){var a=e.target.closest&&e.target.closest('main a[href]');if(!a)return;e.preventDefault();var h=a.href;if(/^https?:/i.test(h))say({go:h})});
addEventListener('keydown',function(e){if(e.key=='F9'){e.preventDefault();say({rdr:'exit'})}else if((e.ctrlKey||e.metaKey)&&!e.shiftKey&&!e.altKey&&e.code=='KeyF'){e.preventDefault();say({rfkey:'cf'})}})})()`;
  return `<!doctype html><html lang="${oraRdEsc(a.lang || 'en')}" dir="${a.dir}"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">`
    + `<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'nonce-${n}'; style-src 'unsafe-inline'; img-src https: http: data:; font-src data:; base-uri 'none'; form-action 'none'">`
    + `<title>${oraRdEsc(a.title || 'Reader')}</title><style>${ff}${css}</style><body><div id="bar" role="toolbar" aria-label="Reader"><button id="sm" title="Smaller text">A−</button><button id="lg" title="Larger text">A+</button>`
    + `<select id="w" title="Width"><option value="n">Narrow</option><option value="m">Medium</option><option value="w">Wide</option></select>`
    + `<select id="ff" title="Font"><option value="serif">Serif</option><option value="sans">Sans</option>${font ? '<option value="dys">OpenDyslexic</option>' : ''}</select>`
    + `<select id="th" title="Colours"><option value="light">Light</option><option value="sepia">Sepia</option><option value="dark">Dark</option></select>`
    + `<button id="rd" aria-pressed="false" title="Read the article aloud">🔊 Read aloud</button><button id="pr" title="Print the article">🖨 Print</button><span class="sp"></span><button id="x" title="Back to the page (F9)">✕ Exit reader</button></div>`
    + `<main><h1 id="t">${oraRdEsc(a.title || '')}</h1>${meta ? `<div id="m">${meta}</div>` : ''}<article>${a.html}</article></main><script nonce="${n}">${js}</scr` + `ipt></body></html>`;
}
const ORA_RD_TABS = new WeakMap();
let ORA_RD_FONT = null;
async function oraRdFont() {   /* (the desktop's own OpenDyslexic, given to the reader's page inside it: the page has no origin to fetch with) */
  if (ORA_RD_FONT !== null) return ORA_RD_FONT;
  try { const css = [...document.querySelectorAll('style')].map(s => s.textContent).join('\n'), m = /font-family:'OpenDyslexic';src:url\(([^)]+)\) format\('woff2'\);font-weight:400/.exec(css);
    if (!m) return ORA_RD_FONT = ''; const u = m[1].replace(/^['"]|['"]$/g, ''); if (/^data:/.test(u)) return ORA_RD_FONT = u;
    const b = await (await fetch(u)).blob(); ORA_RD_FONT = await new Promise(r => { const f = new FileReader(); f.onload = () => r(String(f.result).replace(/^data:[^;]*;/, 'data:font/woff2;')); f.onerror = () => r(''); f.readAsDataURL(b) }) } catch (e) { ORA_RD_FONT = '' }
  return ORA_RD_FONT;
}
function oraReader(c) {
  const W = c.W, T = c.T, q = s => W.body.querySelector(s), ad = q('.ad');
  const btn = document.createElement('button'); btn.className = 'ord'; btn.textContent = '📖'; btn.title = 'Reader mode (F9)'; btn.setAttribute('aria-label', 'Reader mode'); btn.setAttribute('aria-pressed', 'false'); btn.hidden = true;
  if (ad) ad.after(btn);
  let ID = 0, reading = null;
  const prefs = () => { const p = Object.assign({sz: 19, w: 'm', ff: 'serif', th: ''}, typeof S == 'object' && S.oreader || {});
    if (!p.th) p.th = typeof S == 'object' && S.dm == 'dark' ? 'dark' : 'light'; if (typeof S == 'object' && S.acc && S.acc.dysfont && !(S.oreader && S.oreader.ff)) p.ff = 'dys'; return p };
  const own = t => !t.real && t.fr && t.fr.hasAttribute('srcdoc') && !t.fr.hasAttribute('src');
  const urlOf = t => t.h && t.h[t.i] || '';
  /* the button: shown when the page has an article, or the tab is in reader mode */
  const sync = () => { (c.tabs() || []).forEach(x => ORA_RD_TABS.set(x, api)); const t = T(); if (!t) return;
    if (t.rdr && t.rdr.u !== urlOf(t)) drop(t);
    if (t.rdOkU !== urlOf(t)) { t.rdOk = false; t.rdOkU = urlOf(t); if (own(t) && /^file:\/\/\/.*\.html?$/i.test(urlOf(t)) && t.src) { try { t.rdOk = oraReadable(new DOMParser().parseFromString(t.src, 'text/html')) } catch (e) {} } }
    btn.hidden = !(t.rdOk || t.rdr) || (!t.real && !own(t) && !t.rdr); btn.setAttribute('aria-pressed', t.rdr ? 'true' : 'false'); btn.classList.toggle('on', !!t.rdr);
    if (t.rdr) { t.fr.style.display = 'block'; if (c.say && t.rdr.say) { c.say(t.rdr.say); t.rdr.say = '' } } };
  /* leave reader mode: the page as it was (a real tab shown again; Oracle's own page loaded again) */
  const drop = (t, reload) => { if (!t || !t.rdr) return; stopReading(); t.rdr = null; t.fr.removeAttribute('srcdoc'); if (t.real) { t.fr.style.display = 'none' } else if (reload !== false) setTimeout(() => c.reload(t), 0) };
  const enter = async () => { const t = T(); if (!t) return; if (t.rdr) { drop(t); c.sync(); return }
    const u = urlOf(t); let html = '';
    if (t.real && t.rt) { const id = ++ID; t.rdWait = id; c.say('Reading the article…'); rtCmd(t.rt, 'readhtml', {id}); return }
    if (own(t) && t.src) { html = t.src } else { c.say('Reader mode is not available for this page'); return }
    show(t, html, u) };
  const show = async (t, html, u) => { let a = null;
    try { a = await oraArticle(html, u) } catch (e) { c.say('Reader mode could not start (' + (e.message || e) + ')'); return }
    if (!a) { c.say('No article found on this page'); return }
    if (urlOf(t) !== u && !(t.real && t.ru === u)) return;   /* (the tab went elsewhere meanwhile) */
    const p = prefs(), font = (p.ff == 'dys' || (typeof S == 'object' && S.acc && S.acc.dysfont)) ? await oraRdFont() : '';
    if (p.ff == 'dys' && !font) p.ff = 'sans';
    t.rdr = {u, title: a.title, a}; const fr = t.fr; fr.removeAttribute('src'); fr.setAttribute('sandbox', 'allow-scripts'); fr.setAttribute('allow', ''); fr.srcdoc = oraReaderPage(a, u, p, font);
    t.rdr.say = 'Reader mode (F9 or ✕ Exit reader for the page)'; c.sync() };
  /* the tab's answers: whether its page has an article; its HTML */
  const readable = (t, on) => { t.rdOk = !!on; t.rdOkU = urlOf(t); if (t === T()) sync() };
  const readhtml = (t, o) => { if (!o || o.id !== t.rdWait) return; t.rdWait = 0; if (typeof o.html != 'string' || !o.html) { c.say('Reader mode could not read this page'); return } show(t, o.html, t.ru || urlOf(t)) };
  /* read aloud: sentence by sentence in Rudolf's voice, the one being read highlighted in the page */
  const stopReading = () => { if (!reading) return; const r = reading; reading = null; try { ttsStop() } catch (e) {} try { r.fr.contentWindow.postMessage({rdhl: -1, rdreading: false}, '*') } catch (e) {} };
  const readAloud = (t, L) => { stopReading(); if (typeof ttsSpeak != 'function') { c.say('Read aloud is not available'); return }
    const r = reading = {fr: t.fr, k: 0}; const lang = t.rdr && t.rdr.a && t.rdr.a.lang ? t.rdr.a.lang.slice(0, 2) : undefined;
    const post = m => { try { r.fr.contentWindow.postMessage(m, '*') } catch (e) {} };
    const next = () => { if (reading !== r) return; if (r.k >= L.length) { stopReading(); return } post({rdhl: r.k, rdreading: true});
      const k = r.k++; if (!ttsSpeak(String(L[k]).slice(0, 2000), {lang, onend: () => setTimeout(next, 0)})) { if (k == 0) { c.say('Read aloud: no voice on this computer'); stopReading() } else setTimeout(next, 0) } };
    next() };
  const onm = e => { if (!W.el.isConnected) { removeEventListener('message', onm); return } const d = e.data; if (!d || typeof d.rdr != 'string') return;
    const t = (c.tabs() || []).find(x => x.fr && x.fr.contentWindow === e.source); if (!t || !t.rdr) return;
    if (d.rdr == 'exit') { drop(t); c.sync() }
    else if (d.rdr == 'prefs' && d.p && typeof S == 'object') { const p = d.p; S.oreader = {sz: Math.max(14, Math.min(32, +p.sz || 19)), w: /^[nmw]$/.test(p.w) ? p.w : 'm', ff: /^(serif|sans|dys)$/.test(p.ff) ? p.ff : 'serif', th: /^(light|sepia|dark)$/.test(p.th) ? p.th : 'light'}; try { persistS() } catch (x) {} }
    else if (d.rdr == 'read' && Array.isArray(d.s)) readAloud(t, d.s.slice(0, 3000));
    else if (d.rdr == 'stop') stopReading();
    else if (d.rdr == 'print') { const a = t.rdr.a, h = '<h1>' + oraRdEsc(a.title || '') + '</h1>' + (a.byline ? '<p><i>' + oraRdEsc(a.byline) + '</i></p>' : '') + a.html;
      if (!(typeof wpPrint == 'function' && wpPrint(W, h, {title: a.title || 'Article', size: 'Letter'}))) c.say('Printing needs Rudolf OS\'s print service') } };
  addEventListener('message', onm);
  btn.onclick = () => enter();
  W.el.addEventListener('keydown', e => { if (e.key == 'F9' && !e.ctrlKey && !e.altKey && !e.metaKey) { e.preventDefault(); e.stopPropagation(); enter() } });
  /* About Oracle credits Readability */
  const page = (u, r) => { if (u == 'about:oracle' && r && typeof r.html == 'string') r.html = r.html.replace('</div><script>', "<p style='color:#6b7280;font-size:12px'>Reader mode uses Readability.js by Mozilla (Apache License 2.0).</p></div><script>"); return r };
  const api = {sync, readable, readhtml, enter, drop, page, key: a => { if (a == 'f9') { enter(); return true } return false }};
  return api;
}
/* ☰ › Reader mode */
{ const o = window.__oracleMenu; window.__oracleMenu = x => { const a = x && x.tab && ORA_RD_TABS.get(x.tab), r = o ? o(x) : [];
  return a && (x.tab.rdOk || x.tab.rdr) ? r.concat([[x.tab.rdr ? 'Exit reader mode (F9)' : 'Reader mode (F9)', () => a.enter()]]) : r } }
(function () { const s = document.createElement('style'); s.textContent = '.ob .ord{padding:2px 7px}.ob .ord[hidden]{display:none}.ob .ord.on{background:var(--ac,#2563eb);color:#fff}';
  (document.head || document.documentElement).appendChild(s) })();
