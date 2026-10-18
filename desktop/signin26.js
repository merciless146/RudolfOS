/* ===== V26 Oracle sign-in sites (Rudolf OS 1.1.2) =====
   Oracle shows websites as frames inside the desktop page. Chromium treats a frame's cookies as third-party, so a
   site's sign-in cookie is dropped and signing in starts over; Google and Microsoft refuse outright. Sites that
   need a sign-in therefore go to the real browser window (web25.js), where cookies work:
   - before a page loads: the built-in sign-in sites, banks and payment sites, and the sites added in Settings
   - while a page is open: the "web frames" extension (/usr/lib/rudolf/frames, signin.js) reports a password
     field (Oracle asks, or moves the site when "Ask first" is off), a sign-in form being sent or a
     "Sign in with..." window (Oracle moves the site at once). It never reads what is typed.
   The sites on these lists keep their frame protections (the extension removes them for other sites). ===== */
const SI_BANKS = ['chase.com', 'wellsfargo.com', 'citi.com', 'capitalone.com', 'americanexpress.com', 'discover.com', 'schwab.com', 'fidelity.com', 'vanguard.com',
  'ally.com', 'sofi.com', 'robinhood.com', 'coinbase.com', 'paypal.com', 'venmo.com', 'cash.app', 'stripe.com', 'wise.com', 'revolut.com', 'santander.com',
  'barclays.co.uk', 'hsbc.com', 'natwest.com', 'monzo.com', 'n26.com', 'klarna.com', 'irs.gov', 'ssa.gov', 'login.gov', 'id.me', 'healthcare.gov'];
const SI_SLD = /^(co|com|net|org|gov|ac|edu|ne|or|go)$/;
/* the site a host name belongs to: mail.example.co.uk → example.co.uk */
function siDomain(h) {
  const p = String(h || '').toLowerCase().replace(/^www\./, '').split('.').filter(Boolean);
  if (p.length < 3) return p.join('.');
  return p.slice(SI_SLD.test(p[p.length - 2]) && p[p.length - 1].length == 2 ? -3 : -2).join('.');
}
const siBank = h => { const d = siDomain(h), first = d.split('.')[0]; return SI_BANKS.includes(d) || /bank$|^bankof|creditunion|fcu$/.test(first) };
const siUser = () => (S.webSites || []).filter(d => typeof d == 'string');
const siAsk = () => S.siAsk !== false;
/* every site that signs in in the real browser window */
const siAll = () => [...new Set(WEB_SITES.concat(siUser(), SI_BANKS))];
function siWants(u) {
  if (!RD) return false;
  const h = webHost(u); if (!h) return false;
  return webWants(u) || siBank(h);
}
/* tell the web frames extension which sites keep their frame protections */
function siPush() { if (typeof rtOn == 'function' && rtOn()) return; try { window.postMessage({__rudolfFrames: 'sites', sites: siAll()}, location.origin) } catch (e) {} }
function siAdd(h) {
  const d = siDomain(h); if (!d || !/^[a-z0-9.-]+\.[a-z0-9-]+$/.test(d)) return '';
  if (!siUser().includes(d) && !WEB_SITES.some(w => d == w || d.endsWith('.' + w))) { S.webSites = siUser().concat(d); persistS(); siPush() }
  return d;
}
function siRemove(d) { S.webSites = siUser().filter(x => x != d); persistS(); siPush() }

/* a message from the extension inside an Oracle tab. o: {cur(t), note(t,u), setUrl(t,u,title), bar(t,el), T()} */
function siMessage(d, t, src, origin, o) {
  if (!RD || !d || typeof d.__rudolfSignin != 'string') return;
  let href = '';
  try { const x = new URL(String(d.href)); if (/^https?:$/.test(x.protocol) && (!d.direct || x.origin == origin)) href = x.href } catch (e) {}
  if (!href) return;
  const site = d.direct ? href : o.cur(t);
  if (!/^https?:/i.test(site)) return;
  const host = webHost(site);
  if (d.__rudolfSignin == 'url') {
    if (!d.direct) return;
    o.setUrl(t, href, String(d.title || '').slice(0, 120));
    if (siWants(href)) siMove(t, href, o);
    return;
  }
  t.siStay = t.siStay || new Set();
  if (t.siStay.has(host)) { if (d.__rudolfSignin == 'submit') try { src.postMessage({__rudolfSignin: 'stay'}, '*') } catch (e) {} return }
  if (d.__rudolfSignin == 'submit' || d.__rudolfSignin == 'popup' || !siAsk() || siWants(site)) { siMove(t, site, o); return }
  if (d.__rudolfSignin == 'pw' && !t.siBar) siBar(t, site, host, src, o);
}
function siMove(t, u, o) {
  if (t.siBar) { t.siBar.remove(); t.siBar = null }
  if (window.__sys && typeof fwCheck == 'function') { const g = fwCheck(u); if (g) { notify('Blocked by the firewall', g.why, {kind: 'error'}); return } }
  webOpen(u); o.note(t, u);
}
/* the bar above a tab whose page asks for a password */
function siBar(t, site, host, src, o) {
  const b = document.createElement('div'); b.className = 'sibar'; b.setAttribute('role', 'alert');
  b.style.cssText = 'position:absolute;left:0;right:0;top:0;z-index:3;display:flex;flex-wrap:wrap;gap:6px;align-items:center;padding:7px 10px;background:#fef3c7;color:#1f2937;border-bottom:1px solid #f59e0b;font:13px system-ui,sans-serif';
  const e = s => String(s).replace(/[&<>"']/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'})[c]);
  b.innerHTML = `<span style='flex:1;min-width:200px'>🔐 <b>${e(host)}</b> needs a real browser window to sign in; inside Oracle the sign-in would not be kept.</span>
    <button class='si-open pri'>Open in browser window</button><button class='si-always'>Always for this site</button><button class='si-no' title='Stay in Oracle'>Not now</button>`;
  b.querySelector('.si-open').onclick = () => siMove(t, site, o);
  b.querySelector('.si-always').onclick = () => { const d = siAdd(host); siMove(t, site, o); if (d) notify('Oracle', d + ' will always open in the browser window.', {toast: 4000}) };
  b.querySelector('.si-no').onclick = () => { t.siStay = t.siStay || new Set(); t.siStay.add(host); try { src.postMessage({__rudolfSignin: 'stay'}, '*') } catch (x) {} b.remove(); t.siBar = null };
  t.siBar = b; o.bar(t, b);
}

/* the shell's state (rudolfd /api/shell), shown in Settings */
const SI_SHELL = {};
function siShellText() {
  const st = SI_SHELL;
  if (!RD) return 'Needs Rudolf OS started from its disc, USB drive or hard disk.';
  if (!st.id) return 'Checking…';
  if (st.on === false) return 'Off: websites in Oracle are shown the 1.1 way, and sites that need a sign-in open in the browser window.';
  if (st.active) return 'On: Oracle opens websites as real tabs.';
  if (st.failed) return 'The real tabs did not start, so the desktop runs without them until Rudolf OS starts again.';
  if (!st.installed) return 'Being installed in the background…';
  if (!st.ready) return 'Getting your files ready for the move…';
  return 'Ready: starts with the desktop next time.';
}

/* Settings › Oracle sign-in */
function signinPanel(c) {
  const e = s => String(s).replace(/[&<>"']/g, x => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'})[x]);
  const draw = () => {
    const mine = siUser();
    c.innerHTML = `<h3>Oracle sign-in</h3>
<h4 class='sh4'>Real browser tabs</h4><div class='r2'><label>Oracle's tabs</label><div>${ckH('sishell', 'Open websites as real browser tabs (signing in works inside Oracle)', SI_SHELL.on !== false)}<div class='msg sishm'>${siShellText()}</div><div class='rmrow sishr' style='justify-content:flex-start' hidden><button class='sishgo'>Restart the desktop now</button></div></div></div>
${typeof rtOn == 'function' && rtOn() ? "<p class='msg'>Oracle's tabs are real browser tabs now: signing in works inside Oracle, for every site. The settings below are used only when real tabs are off or cannot start.</p><details><summary>Without real tabs</summary>" : ''}<p class='msg'>Websites in Oracle are shown inside the desktop, where they cannot keep you signed in. Sites that need a sign-in open in a real browser window instead (🌐 in the taskbar switches between it and the desktop).</p>
<div class='r2'><label>When a page asks for a password</label><div>${ckH('siask', 'Ask first (otherwise the site moves to the browser window by itself)', siAsk())}<div class='msg'>Sending a sign-in, or a “Sign in with Google / Microsoft / Apple…” button, always moves the site.</div></div></div>
<h4 class='sh4'>Your sign-in sites</h4><div class='sil'>${mine.length ? mine.map(d => `<div style='display:flex;gap:8px;align-items:center;padding:3px 0'><span style='flex:1'>${e(d)}</span><button data-rm='${e(d)}' aria-label='Remove ${e(d)}'>Remove</button></div>`).join('') : "<div class='msg'>None yet. Choose “Always for this site” when Oracle asks, or add one here.</div>"}</div>
<div class='rmrow' style='justify-content:flex-start'><input class='sin' placeholder='example.com' aria-label='Site to add' style='flex:0 1 240px'><button class='sia'>Add</button><span class='msg sim'></span></div>
<details style='margin-top:10px'><summary>Built-in sign-in sites, banks and payment sites (${WEB_SITES.length + SI_BANKS.length})</summary><div class='msg' style='margin-top:6px;line-height:1.6'>${WEB_SITES.concat(SI_BANKS).map(e).join(' · ')}<br>Also any site whose name ends in “bank” or is a credit union.</div></details>${typeof rtOn == 'function' && rtOn() ? '</details>' : ''}`;
    c.querySelector('#siask').onchange = ev => { S.siAsk = ev.target.checked; persistS() };
    c.querySelector('#sishell').onchange = async ev => {
      try { await rdPost('shell?on=' + (ev.target.checked ? 1 : 0)); SI_SHELL.on = ev.target.checked; c.querySelector('.sishm').textContent = 'Saved: this takes effect when the desktop starts again.'; c.querySelector('.sishr').hidden = false }
      catch (e) { c.querySelector('.sishm').textContent = e.message }
    };
    c.querySelector('.sishgo').onclick = () => { try { if (typeof fsFlush == 'function') fsFlush() } catch (e) {} setTimeout(() => rdPost('shell/switch').catch(() => {}), 500) };
    if (RD) rd('shell').then(st => { Object.assign(SI_SHELL, st); const m = c.querySelector('.sishm'); if (m && !m.textContent.startsWith('Saved')) m.textContent = siShellText(); const b = c.querySelector('#sishell'); if (b) b.checked = st.on }).catch(() => {});
    c.querySelectorAll('[data-rm]').forEach(b => b.onclick = () => { siRemove(b.dataset.rm); draw() });
    const add = () => { let v = c.querySelector('.sin').value.trim(); try { if (/^https?:/i.test(v)) v = new URL(v).hostname } catch (x) {} const d = siAdd(v.toLowerCase()); if (d) draw(); else c.querySelector('.sim').textContent = 'That is not a web address such as example.com.' };
    c.querySelector('.sia').onclick = add; c.querySelector('.sin').onkeydown = ev => { if (ev.key == 'Enter') add() };
  };
  draw();
}
if (RD) setTimeout(siPush, 2500);
