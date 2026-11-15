// 2026-11-15 (docs/plan-1.6.0.md Part 14, third step): the keyring's prompts drawn by the desktop. gnome-keyring-daemon
// asks rudolf-portal (in gcr-prompter's place), whose question comes with the portal's (portal/poll, kind "keyring"):
// a new keyring's password (asked twice, a mismatch said), unlocking one (the daemon's warning after a wrong password,
// its "unlock whenever I sign in" choice), a confirmation, Cancel and Escape, who asked (the Flatpak app's name, else
// its program), every word drawn as text, and a question the daemon withdraws closing by itself.
//   node keyprompt60_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'); const PORT = 8645;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = {reqs: [], answers: []};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); let body = '';
  q.on('data', d => body += d); q.on('end', async () => {
    const J = (o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') { await new Promise(r => setTimeout(r, 800)); s.writeHead(200); return s.end('{"ok":true,"events":""}') }
    if (u == '/api/portal/poll') return J({ok: true, requests: M.reqs});
    if (u == '/api/portal/answer') { const id = new URLSearchParams(qs).get('id'); M.answers.push({id, a: JSON.parse(body || '{}')}); M.reqs = M.reqs.filter(x => x.id != id); return J({ok: true}) }
    if (u == '/api/apps/installed') return J({ok: true, apps: [{id: 'org.example.Mail', name: 'Example Mail'}]});
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const NEW = {app: '', proc: 'python3', kind: 'keyring', type: 'password', title: '', message: 'Choose password for new keyring', description: 'An application wants to create a new keyring called “<b>Work</b>”. Choose the password you want to use for it.', warning: '', choice: '', cont: 'Continue', cancel: 'Cancel', newpass: true, chosen: false};
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage({viewport: {width: 1280, height: 900}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof krPrompt == 'function' && typeof lxPortal == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];return 1})()");
  const box = () => q("(()=>{const o=document.querySelector('.krp60');if(!o)return null;const t=s=>(o.querySelector(s)||{}).textContent;return {title:t('.krt'),who:t('.kra'),msg:t('.krm'),desc:t('.krd'),warn:t('.rmmsg'),cancel:t('.en'),cont:t('.ey'),inputs:o.querySelectorAll('input:not([type=checkbox])').length,choice:t('.krc span'),checked:(o.querySelector('.kc')||{}).checked,html:o.innerHTML}})()");
  const type = async (sel, t) => { await p.click('.krp60 ' + sel); await p.keyboard.type(t) };
  const ask = async r => { M.reqs.push(r); await p.waitForSelector('.krp60', {timeout: 5000}).catch(() => {}); await wait(150) };
  // 1. a new keyring's password
  await ask({id: 'k1-1', req: NEW}); let x = await box();
  ok('1. an app creating a keyring: the desktop asks in Settings\' prompt style (🔑, who asked, the daemon\'s words)', x && /rmbox/.test(x.html) && x.title == 'New keyring password' && x.who == 'python3' && x.msg == 'Choose password for new keyring' && x.inputs == 2 && x.cont == 'Continue' && x.cancel == 'Cancel', x);
  ok('...the daemon\'s words drawn as text, never as HTML', x && /<b>Work<\/b>/.test(x.desc) && !/<b>Work/.test(x.html), x && x.desc);
  ok('...the password\'s field hides what is typed (no browser password store)', await q("(()=>{const i=document.querySelector('.krp60 .kp1');return i.type=='text'&&i.autocomplete=='off'})()"));
  await type('.kp1', 'work-pw-1'); await type('.kp2', 'work-pw-2'); await p.click('.krp60 .ey'); await wait(200); x = await box();
  ok('...two different passwords: said, not answered', x && /do not match/.test(x.warn) && !M.answers.length, [x && x.warn, M.answers]);
  ok('...what is typed is not in the page as text', !(await q("document.body.innerHTML.includes('work-pw-1')")));
  await type('.kp2', 'work-pw-1'); await p.keyboard.press('Enter'); await wait(400);
  ok('...the same twice (Enter): answered yes with the password, the prompt closed', M.answers.length == 1 && M.answers[0].id == 'k1-1' && M.answers[0].a.response == 0 && M.answers[0].a.password == 'work-pw-1' && !(await box()), M.answers);
  // 2. unlocking, after a wrong password, with the choice; a Flatpak app's name
  await q('lxInstalled(true).then(()=>1)');
  await ask({id: 'k1-2', req: {...NEW, app: 'org.example.Mail', proc: 'example-mail', newpass: false, title: 'Unlock Keyring', message: 'Authentication required', description: 'An application wants access to the keyring “Work”, but it is locked', warning: 'The unlock password was incorrect', choice: 'Automatically unlock this keyring whenever I’m logged in', chosen: true}});
  x = await box();
  ok('2. unlocking: one password, the daemon\'s warning, its choice (ticked as it says)', x && x.title == 'Unlock Keyring' && x.inputs == 1 && x.warn == 'The unlock password was incorrect' && /Automatically unlock/.test(x.choice) && x.checked === true, x);
  ok('...a Flatpak app named as the App Store names it', x && x.who == 'Example Mail', x && x.who);
  await type('.kp1', 'pässwörd 🔑'); await p.click('.krp60 .kc'); await p.click('.krp60 .ey'); await wait(400);
  ok('...answered with the password as typed (any characters) and the choice', M.answers.length == 2 && M.answers[1].a.password == 'pässwörd 🔑' && M.answers[1].a.chosen === false, M.answers[1]);
  // 3. a confirmation, Cancel, Escape
  await ask({id: 'k1-3', req: {...NEW, type: 'confirm', newpass: false, message: 'Store passwords unencrypted?', description: 'By choosing to use a blank password, your stored passwords will not be safely encrypted.'}});
  x = await box();
  ok('3. a confirmation: no password field', x && x.inputs == 0 && x.msg == 'Store passwords unencrypted?', x);
  await p.click('.krp60 .ey'); await wait(400);
  ok('...Continue: yes, no password sent', M.answers.length == 3 && M.answers[2].a.response == 0 && !('password' in M.answers[2].a), M.answers[2]);
  await ask({id: 'k1-4', req: {...NEW, newpass: false, message: 'Authentication required'}}); await p.click('.krp60 .en'); await wait(400);
  ok('...Cancel: no', M.answers.length == 4 && M.answers[3].a.response == 1 && !('password' in M.answers[3].a), M.answers[3]);
  await ask({id: 'k1-5', req: {...NEW, newpass: false, message: 'Authentication required'}}); await p.keyboard.press('Escape'); await wait(400);
  ok('...Escape: no', M.answers.length == 5 && M.answers[4].a.response == 1, M.answers[4]);
  // 4. withdrawn
  await ask({id: 'k1-6', req: {...NEW, proc: 'claude', newpass: false}}); x = await box();
  ok('4. a known program named by its name (claude: Claude)', x && x.who == 'Claude', x && x.who);
  M.reqs = []; await wait(1700);
  ok('...withdrawn by the daemon (the app went away): the prompt closes by itself, nothing answered', !(await box()) && M.answers.length == 5, M.answers.length);
  ok('5. other portal requests still work (a notification answered)', await (async () => { M.reqs.push({id: 'n1-1', req: {app: '', kind: 'notify', title: 'Hi', body: 'x'}}); await wait(1500); return M.answers.some(a => a.id == 'n1-1') })());
  ok('6. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
