/* ===== V26 file picker. On Rudolf OS every "choose a file" button (Upload, Open a file, From computer, Import...)
   opens this picker inside the desktop instead of Chromium's own file dialog. Chromium's dialog is a separate
   window, and the desktop runs without a window manager: on some PCs the dialog opened where it could not be seen
   or reached, while the desktop waited for it, so the screen stopped taking clicks and keys. This picker lists the
   real Downloads, Documents and Pictures folders and every drive (USB sticks, other disks), reads the chosen files
   through rudolfd and hands them to the page exactly as the dialog would. In a normal browser nothing changes. ===== */
if (RD) (function () {
  const isFileInput = el => el && el.tagName == 'INPUT' && String(el.type).toLowerCase() == 'file';
  const nativeClick = HTMLInputElement.prototype.click;
  HTMLInputElement.prototype.click = function () { if (isFileInput(this)) { rfPick(this); return } return nativeClick.call(this) };
  if (HTMLInputElement.prototype.showPicker) {
    const nativeShow = HTMLInputElement.prototype.showPicker;
    HTMLInputElement.prototype.showPicker = function () { if (isFileInput(this)) { rfPick(this); return } return nativeShow.call(this) };
  }
  // a visible file input clicked (or activated through its label, or Enter/Space): stop Chromium's dialog
  document.addEventListener('click', e => { if (isFileInput(e.target) && !e.target.disabled) { e.preventDefault(); e.stopPropagation(); rfPick(e.target) } }, true);
})();

const RF_MIME = {png: 'image/png', jpg: 'image/jpeg', jpeg: 'image/jpeg', gif: 'image/gif', webp: 'image/webp', bmp: 'image/bmp', svg: 'image/svg+xml', ico: 'image/x-icon',
  mp3: 'audio/mpeg', wav: 'audio/wav', ogg: 'audio/ogg', oga: 'audio/ogg', m4a: 'audio/mp4', aac: 'audio/aac', flac: 'audio/flac', opus: 'audio/opus',
  mp4: 'video/mp4', m4v: 'video/mp4', webm: 'video/webm', ogv: 'video/ogg', mov: 'video/quicktime', mkv: 'video/x-matroska',
  txt: 'text/plain', csv: 'text/csv', html: 'text/html', htm: 'text/html', css: 'text/css', js: 'text/javascript', json: 'application/json', xml: 'application/xml', md: 'text/markdown',
  pdf: 'application/pdf', zip: 'application/zip'};
const rfExt = n => (/\.([^.]+)$/.exec(n) || [, ''])[1].toLowerCase();
const rfMime = n => RF_MIME[rfExt(n)] || 'application/octet-stream';
const rfEsc = s => String(s).replace(/[&<>"']/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'}[c]));
const rfSize = b => b < 1024 ? b + ' B' : b < 1048576 ? (b / 1024).toFixed(0) + ' KB' : b < 1073741824 ? (b / 1048576).toFixed(1) + ' MB' : (b / 1073741824).toFixed(2) + ' GB';
const RF_MAX = 512 * 1048576;

/* does a file name match the input's accept="…" list (".png,image/*,audio/mpeg")? */
function rfAccepts(accept, name) {
  const list = String(accept || '').split(',').map(s => s.trim().toLowerCase()).filter(Boolean);
  if (!list.length) return true;
  const ext = '.' + rfExt(name), mime = rfMime(name);
  return list.some(a => a[0] == '.' ? a == ext : a.endsWith('/*') ? mime.startsWith(a.slice(0, -1)) : a == mime);
}

let RF_LAST = '/home/rudolf/Downloads';
function rfPick(input) {
  if (document.querySelector('.rfpick')) return;
  const multi = input.multiple, accept = input.accept || '';
  const o = document.createElement('div'); o.className = 'rmodal rfpick';
  o.innerHTML = `<div class='rmbox' role='dialog' aria-modal='true' aria-label='Choose ${multi ? 'files' : 'a file'}' style='width:min(680px,94vw);max-width:none;display:flex;flex-direction:column;gap:8px'>
    <h3 style='margin:0'>Choose ${multi ? 'files' : 'a file'}</h3>
    <div class='rfplaces' style='display:flex;flex-wrap:wrap;gap:4px'></div>
    <div style='display:flex;gap:6px;align-items:center'><button class='rfup' title='Up one folder' aria-label='Up one folder'>⬆</button><input class='rfpath' readonly aria-label='Folder' style='flex:1'></div>
    <div class='rflist' role='listbox' ${multi ? "aria-multiselectable='true'" : ''} tabindex='0' style='height:min(340px,50vh);overflow:auto;border:1px solid var(--bd,#ccc);background:var(--wbg,#fff)'></div>
    <label style='font-size:12px'><input type='checkbox' class='rfall'> Show all files${accept ? ' (not only ' + rfEsc(accept.replace(/,/g, ', ')) + ')' : ''}</label>
    <div class='rmmsg rfmsg' aria-live='polite'></div>
    <div class='rmrow'><button class='rfcancel'>Cancel</button><button class='rfok pri' disabled>Open</button></div></div>`;
  document.body.appendChild(o);
  const q = s => o.querySelector(s), L = q('.rflist');
  if (!accept) q('.rfall').parentElement.hidden = true;
  let cur = '', items = [], sel = new Set(), done = false;
  const finish = async ok => {
    if (done) return;
    if (!ok) { done = true; o.remove(); input.dispatchEvent(new Event('cancel', {bubbles: false})); return }
    const paths = [...sel]; if (!paths.length) return;
    const big = paths.map(p => items.find(i => cur + '/' + i.name == p)).filter(i => i && i.size > RF_MAX);
    if (big.length) { q('.rfmsg').textContent = big[0].name + ' is larger than ' + rfSize(RF_MAX) + '; copy it in Files instead.'; return }
    done = true; q('.rfok').disabled = true; q('.rfmsg').textContent = 'Reading…';
    try {
      const dt = new DataTransfer();
      for (const p of paths) {
        const r = await fetch('/api/fs/read?path=' + encodeURIComponent(p), {headers: {'X-Rudolf-Token': RD.token}});
        if (!r.ok) throw new Error(p.split('/').pop() + ': could not be read (HTTP ' + r.status + ')');
        const it = items.find(i => cur + '/' + i.name == p) || {};
        const name = p.split('/').pop();
        dt.items.add(new File([await r.blob()], name, {type: rfMime(name), lastModified: it.mtime || Date.now()}));
      }
      RF_LAST = cur; o.remove();
      input.files = dt.files;
      input.dispatchEvent(new Event('input', {bubbles: true}));
      input.dispatchEvent(new Event('change', {bubbles: true}));
    } catch (e) { done = false; q('.rfok').disabled = false; q('.rfmsg').textContent = e.message }
  };
  const draw = () => {
    q('.rfpath').value = cur; L.innerHTML = '';
    const all = q('.rfall').checked;
    const shown = items.filter(i => i.dir || all || rfAccepts(accept, i.name)).sort((a, b) => (b.dir - a.dir) || a.name.localeCompare(b.name));
    if (!shown.length) L.innerHTML = `<div class='msg' style='padding:8px'>${items.length ? 'No matching files here.' : 'This folder is empty.'}</div>`;
    shown.forEach(i => {
      const p = cur + '/' + i.name, r = document.createElement('div');
      r.className = 'pkr'; r.tabIndex = -1; r.setAttribute('role', 'option');
      r.style.cssText = 'display:flex;gap:8px;padding:3px 6px;cursor:default';
      r.innerHTML = `<span style='flex:1'>${i.dir ? '📁' : '📄'} ${rfEsc(i.name)}</span><small>${i.dir ? '' : rfSize(i.size)}</small>`;
      const on = sel.has(p); r.setAttribute('aria-selected', on); if (on) r.style.background = 'var(--sel,#cce4ff)';
      r.onclick = e => {
        if (i.dir) return;
        if (multi && (e.ctrlKey || e.metaKey || e.shiftKey)) sel.has(p) ? sel.delete(p) : sel.add(p);
        else { sel.clear(); sel.add(p) }
        q('.rfok').disabled = !sel.size; draw();
      };
      r.ondblclick = () => { if (i.dir) show(p); else { sel.clear(); sel.add(p); finish(true) } };
      L.appendChild(r);
    });
    q('.rfok').disabled = !sel.size;
  };
  const show = async p => {
    cur = p; sel.clear(); items = []; q('.rfmsg').textContent = ''; L.innerHTML = "<div class='msg' style='padding:8px'>Reading…</div>";
    try { items = (await rd('fs/list?path=' + encodeURIComponent(p))).items } catch (e) { q('.rfmsg').textContent = e.message }
    draw();
  };
  // places: the shared folders, then every drive (mounted when chosen)
  const places = [['⬇️ Downloads', '/home/rudolf/Downloads'], ['📄 Documents', '/home/rudolf/Documents'], ['🖼️ Pictures', '/home/rudolf/Pictures']];
  const addPlace = ([label, path, dev]) => {
    const b = document.createElement('button'); b.textContent = label;
    b.onclick = async () => {
      if (!path && dev) { try { await rdPost('mount?dev=' + dev); path = '/media/' + dev } catch (e) { q('.rfmsg').textContent = e.message; return } }
      show(path);
    };
    q('.rfplaces').appendChild(b);
  };
  places.forEach(addPlace);
  rd('disks').then(r => (r.disks || []).forEach(dk => {
    if (dk.partitions.some(x => x.label == 'RUDOLFDATA' || x.fs == 'iso9660')) return;   // Rudolf OS's own disk
    dk.partitions.filter(x => x.fs && x.fs != 'swap').forEach(pt => addPlace([(dk.removable ? '🔌 ' : '💽 ') + (pt.label || pt.name), pt.mount || '', pt.name]));
  })).catch(() => {});
  q('.rfup').onclick = () => { const top = /^\/home\/rudolf\/[^/]+$|^\/media\/[^/]+$/.test(cur); if (cur && !top) show(cur.replace(/\/[^/]+$/, '')) };
  q('.rfall').onchange = draw;
  q('.rfok').onclick = () => finish(true);
  q('.rfcancel').onclick = () => finish(false);
  o.addEventListener('keydown', e => { e.stopPropagation(); if (e.key == 'Escape') finish(false); if (e.key == 'Enter' && sel.size) finish(true) });
  show(RF_LAST);
  setTimeout(() => q('.rfcancel').focus(), 0);
}
