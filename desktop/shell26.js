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
