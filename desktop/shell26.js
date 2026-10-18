/* ===== V26 the Rudolf OS shell (1.2.0) =====
   On Rudolf OS 1.2 the desktop page runs inside the shell, an Isolated Web App (iso/shell) whose Controlled Frames
   give Oracle real browser tabs. The shell says hello after every load; SHELL is then its window, and
   shellAsk(op, data) sends it a request and resolves with its answer. Without the shell (older systems, a normal
   browser, or the session's fallback) SHELL stays null and everything works as before. ===== */
let SHELL = null, SHELL_VER = '', SHELL_N = 0, SHELL_ORIGIN = '';
/* the shell's address: isolated-app://<its ID>/ (the ID is published next to the shell's bundle) */
if (RD) fetch('/iwa/id').then(r => r.ok ? r.text() : '').then(t => { t = t.trim(); if (/^[a-z2-7]{56}$/.test(t)) SHELL_ORIGIN = 'isolated-app://' + t }).catch(() => {});
const SHELL_WAIT = new Map();
function shellSend(m) { if (SHELL) try { SHELL.postMessage(m, '*') } catch (e) {} }
function shellAsk(op, data) {
  if (!SHELL) return Promise.reject(new Error('no shell'));
  const id = ++SHELL_N;
  return new Promise((res, rej) => {
    const t = setTimeout(() => { SHELL_WAIT.delete(id); rej(new Error('the shell did not answer')) }, 5000);
    SHELL_WAIT.set(id, v => { clearTimeout(t); res(v) });
    shellSend(Object.assign({rudolfShell: op, id}, data || {}));
  });
}
addEventListener('message', e => {
  const m = e.data;
  if (!m || typeof m.rudolfShell != 'string' || e.source == null) return;
  if (!SHELL_ORIGIN || e.origin !== SHELL_ORIGIN) return;           // only Rudolf OS's own shell
  if (m.rudolfShell == 'hello') { SHELL = e.source; SHELL_VER = String(m.version || ''); document.documentElement.classList.add('inshell'); shellSend({rudolfShell: 'ready'}); return }
  if (m.id && SHELL_WAIT.has(m.id)) { SHELL_WAIT.get(m.id)(m); SHELL_WAIT.delete(m.id); return }
  if (m.rudolfShell == 'newwindow' && /^https?:/i.test(m.url || '')) { if (typeof webOpen == 'function') webOpen(m.url); return }
});

/* pointer capture for games inside the shell (the frame cannot capture the pointer itself): h = {move(dx,dy),
   button(b,down), wheel(dy), change(locked)}. Resolves true when the pointer is captured. */
let SHELL_PTR = null;
async function shellLock(h) { SHELL_PTR = h; try { const r = await shellAsk('lock'); return !!r.ok } catch (e) { return false } }
function shellUnlock() { shellSend({rudolfShell: 'unlock'}) }
addEventListener('message', e => {
  if (!SHELL || e.source !== SHELL || !SHELL_PTR) return;
  const m = e.data || {};
  if (m.rudolfShell == 'mm') SHELL_PTR.move && SHELL_PTR.move(m.dx | 0, m.dy | 0);
  else if (m.rudolfShell == 'mb') SHELL_PTR.button && SHELL_PTR.button(m.b | 0, !!m.down);
  else if (m.rudolfShell == 'mw') SHELL_PTR.wheel && SHELL_PTR.wheel(+m.dy || 0);
  else if (m.rudolfShell == 'lockchange') { SHELL_PTR.change && SHELL_PTR.change(!!m.locked); if (!m.locked) SHELL_PTR = null }
});

/* the clipboard inside the shell: reading goes through rudolfd (the X clipboard, shared with Linux apps);
   writing falls back to the copy command when the frame is not allowed to write */
function shellClipboard() {
  if (!navigator.clipboard || navigator.clipboard.__rudolf) return;
  const C = navigator.clipboard, w0 = C.writeText && C.writeText.bind(C);
  const copyCmd = t => { const a = document.createElement('textarea'); a.value = t; a.setAttribute('readonly', ''); a.style.cssText = 'position:fixed;left:-9999px;top:0'; document.body.appendChild(a); a.select(); let ok = false; try { ok = document.execCommand('copy') } catch (e) {} a.remove(); if (!ok) throw new DOMException('Copying is not allowed here', 'NotAllowedError') };
  try {
    Object.defineProperty(C, 'readText', {configurable: true, value: async () => (await rd('clipboard')).text || ''});
    Object.defineProperty(C, 'writeText', {configurable: true, value: async t => { try { if (w0) return await w0(t) } catch (e) {} copyCmd(String(t)) }});
    Object.defineProperty(C, '__rudolf', {value: true});
  } catch (e) {}
}

/* Rudolf OS 1.2's one-time move to the shell. On the plain desktop page, once the databases are in rudolfd's
   mirror and Chromium has installed the shell, the browser starts again with the shell: by itself right after
   start-up, otherwise when the person agrees. */
async function shellOffer() {
  if (!RD || SHELL) return;
  let st; try { st = await rd('shell') } catch (e) { return }
  if (!st.id || !st.on || st.failed || st.active || !st.ready) return;
  if (!st.installed) { if ((shellOffer.n = (shellOffer.n || 0) + 1) < 20) setTimeout(shellOffer, 10000); return }   // still installing
  const quiet = performance.now() < 120000 && !(typeof wins != 'undefined' && wins.length);
  const go = () => { try { if (typeof fsFlush == 'function') fsFlush() } catch (e) {} ; setTimeout(() => rdPost('shell/switch').catch(() => {}), 600) };
  if (quiet) {
    const o = document.createElement('div'); o.className = 'rmodal'; o.style.zIndex = 2147483000;
    o.innerHTML = "<div class='rmbox' role='status'><h3 style='margin-top:0'>Real browser tabs are ready</h3><p>Rudolf OS is starting its browser again so websites in Oracle open as real tabs. This takes a few seconds and happens once.</p></div>";
    document.body.appendChild(o); go();
  } else notify('Real browser tabs are ready', 'Oracle can open websites as real tabs, where signing in works. Click to restart the desktop now (your files are saved).', {action: go, toast: 15000});
}
if (RD) {
  /* inside the shell: the clipboard workaround; on the plain page: the move to the shell, after the databases are in the mirror */
  setTimeout(() => { if (SHELL) shellClipboard(); else IDB.ready.then(() => setTimeout(shellOffer, 1500)) }, 4000);
  addEventListener('message', e => { if (SHELL && e.source === SHELL && e.data && e.data.rudolfShell == 'hello') shellClipboard() });
}
