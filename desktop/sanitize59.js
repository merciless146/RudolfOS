/* ===== Rudolf OS 1.5.9 (plan 15.2): HTML from a file, made safe before it is shown in the desktop's page
   safeHtml(html): the same HTML with nothing in it that can run. Parsed by the browser itself (DOMParser: an inert
   document, where nothing runs or loads), not by patterns, so the tricks that get past patterns (a slash for a space
   before onerror, entities, upper case, SVG and MathML, nested markup) do not work. Then:
   - elements that run or load things are removed with their contents: script, iframe, frame, frameset, object, embed,
     applet, base, link, meta, noscript, template, portal, SVG's animate, set and animateTransform (they can set an
     href to javascript:), SVG's foreignObject and MathML's annotation-xml (HTML inside them);
   - every attribute whose name begins with "on" is removed, and srcdoc, formaction, action, ping;
   - a link or source (href, src, xlink:href, poster, background, data, srcset, cite, longdesc) is kept only when it is
     relative, an anchor (#), or http, https, mailto, tel, blob, or data: for a picture (not SVG, which can carry script);
   - style attributes with expression(, javascript: or behavior: are removed;
   - the result is parsed and checked again (until nothing changes), so markup that would turn into something else
     when the browser reads it a second time cannot slip through.
   Used by WordPro's clean() (opening a document, pasting) and wherever a file's HTML is shown in the desktop's page. */
const SAFE_DROP = new Set(['SCRIPT','IFRAME','FRAME','FRAMESET','OBJECT','EMBED','APPLET','BASE','LINK','META','NOSCRIPT','TEMPLATE','PORTAL','ANIMATE','SET','ANIMATETRANSFORM','ANIMATEMOTION','FOREIGNOBJECT','ANNOTATION-XML','HANDLER','LISTENER']);
const SAFE_URLA = new Set(['href','src','xlink:href','poster','background','data','srcset','cite','longdesc','lowsrc','dynsrc','codebase','manifest','icon','formaction','action']);
function safeUrl(v) {
  const s = String(v || '').replace(/[\u0000-\u0020\u007f-\u009f\u00ad\u200b-\u200f\u2028\u2029\ufeff]/g, '').toLowerCase();
  if (!s || s[0] == '#' || s[0] == '/' || s[0] == '.' || s[0] == '?') return true;
  const m = /^([a-z][a-z0-9+.-]*):/.exec(s); if (!m) return true;   /* (no scheme: relative) */
  if (/^(https?|mailto|tel|blob)$/.test(m[1])) return true;
  return m[1] == 'data' && /^data:image\/(png|jpe?g|gif|webp|bmp|avif|x-icon)[;,]/.test(s);
}
function safeClean(root) {
  let changed = false;
  const all = [...root.querySelectorAll('*')];
  for (const el of all) {
    if (!el.isConnected && el !== root) continue;
    const tag = (el.localName || el.tagName || '').toUpperCase();
    if (SAFE_DROP.has(tag)) { el.remove(); changed = true; continue }
    for (const a of [...el.attributes]) {
      const n = a.name.toLowerCase(), v = a.value;
      if (n.startsWith('on') || n == 'srcdoc' || n == 'formaction' || n == 'action' || n == 'ping' || n == 'is') { el.removeAttribute(a.name); changed = true; continue }
      if (SAFE_URLA.has(n) || n.endsWith(':href')) {
        const ok = n == 'srcset' ? String(v).split(',').every(x => safeUrl(x.trim().split(/\s+/)[0])) : safeUrl(v);
        if (!ok) { el.removeAttribute(a.name); changed = true }
        continue }
      if (n == 'style' && /expression\s*\(|javascript\s*:|behavior\s*:|-moz-binding/i.test(v)) { el.removeAttribute(a.name); changed = true }
    }
  }
  return changed;
}
function safeHtml(html) {
  let h = String(html == null ? '' : html);
  for (let round = 0; round < 4; round++) {
    const d = new DOMParser().parseFromString('<!doctype html><body>' + h, 'text/html');
    const changed = safeClean(d.body), out = d.body.innerHTML;
    if (!changed && out == h) return out;
    h = out }
  return '';   /* (never settles: not shown) */
}
