// 1.5.0: rudolf-remote end to end on this machine: an X screen (Xvfb :91) showing a test page in Chromium, rudolf-remote
// (built with its folders under WORK and the display :91), and the viewer in another browser over HTTPS + WebSocket:
// pairing code, sign-in against the PBKDF2 hashes, approval at the computer, the screen arrives, mouse and keyboard
// reach the page, End at the computer, unattended access, five wrong tries lock it.
// node remote50_t.js WORK RR-BINARY   (WORK: run/ etc/ www/ as rr was built with)
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process'), crypto = require('crypto');
const [WORK, RR] = process.argv.slice(2);
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const wait = ms => new Promise(r => setTimeout(r, ms));
const hash = pw => { const s = crypto.randomBytes(16); return 'p2$' + s.toString('base64') + '$' + crypto.pbkdf2Sync(pw, s, 210000, 32, 'sha256').toString('base64') };
(async () => {
  const run = path.join(WORK, 'run'), etc = path.join(WORK, 'etc');
  for (const f of fs.readdirSync(run)) fs.rmSync(path.join(run, f), {recursive: true, force: true});
  fs.rmSync(path.join(etc, 'remote-devices'), {force: true});
  fs.writeFileSync(path.join(etc, 'remote-accounts'), `boss\t${hash('b1')}\tadmin\t0\nkid\t${hash('k1')}\tuser\t1\n`);
  fs.writeFileSync(path.join(run, 'on'), '1\n'); fs.writeFileSync(path.join(run, 'code'), '123456\n');
  const X = cp.spawn('Xvfb', [':91', '-screen', '0', '800x600x24', '-nolisten', 'tcp', '-ac', '+extension', 'XTEST'], {stdio: 'ignore'}); await wait(1500);
  // the computer's screen: a page that records what reaches it
  const host = await chromium.launch({headless: false, env: {...process.env, DISPLAY: ':91'}, args: ['--window-position=0,0', '--window-size=800,600']});
  const hp = await host.newPage(); await hp.setViewportSize({width: 800, height: 600});
  await hp.setContent(`<body style="margin:0;background:rgb(0,160,80);height:100vh"><input id=t autofocus style="font-size:30px;margin:40px"><div id=c>0</div><script>addEventListener('mousedown',e=>{c.textContent=(+c.textContent+1)+' '+e.clientX+','+e.clientY})</script></body>`);
  await wait(1000);
  const srv = cp.spawn(RR, [], {stdio: ['ignore', 'ignore', 'pipe']}); let log = ''; srv.stderr.on('data', d => log += d); await wait(1500);
  ok('1. rudolf-remote starts, makes its certificate and listens on 8443', /serving https on port 8443/.test(log) && fs.existsSync(path.join(etc, 'remote/cert.pem')) && /^([0-9A-F]{2}:){31}[0-9A-F]{2}$/.test(fs.readFileSync(path.join(etc, 'remote/fingerprint'), 'utf8').trim()), log);
  const vb = await chromium.launch(); const vc = await vb.newContext({ignoreHTTPSErrors: true}); const v = await vc.newPage();
  const errs = []; v.on('pageerror', e => errs.push(e.message));
  await v.goto('https://127.0.0.1:8443/'); await wait(500);
  ok('2. the viewer asks for the pairing code in a new browser', await v.isVisible('#code'));
  await v.fill('#code', '000000'); await v.fill('#user', 'boss'); await v.fill('#pass', 'b1'); await v.click('#go'); await wait(800);
  ok('3. a wrong pairing code is refused', /pairing code is not right/.test(await v.textContent('#msg')));
  await v.fill('#code', '123456'); await v.fill('#pass', 'nope'); await v.click('#go'); await wait(2500);
  ok('4. the right code but a wrong password is refused', /name or password is not right/.test(await v.textContent('#msg')));
  await v.goto('https://127.0.0.1:8443/'); await wait(500);
  ok('5. ...the browser is remembered: no pairing code the next time', !(await v.isVisible('#code')));
  await v.fill('#user', 'boss'); await v.fill('#pass', 'b1'); await v.click('#go'); await wait(3000);
  const pend = fs.existsSync(path.join(run, 'pending')) ? fs.readFileSync(path.join(run, 'pending'), 'utf8') : '';
  ok('6. it waits for approval at the computer (pending: id, account, address)', /^[0-9a-f]{16}\tboss\t127\.0\.0\.1\t\d+/.test(pend) && /allow the connection/.test(await v.textContent('#waitMsg')), pend);
  fs.writeFileSync(path.join(run, 'answer-' + pend.split('\t')[0]), 'allow\n'); await wait(2500);
  ok('7. allowed: the screen is shown, at the computer\'s size', await v.isVisible('#c') && await v.evaluate(() => [c.width, c.height].join('x')) == '800x600');
  const px = await v.evaluate(() => Array.from(c.getContext('2d').getImageData(700, 500, 1, 1).data));
  ok('8. the picture arrives (the page\'s green)', Math.abs(px[0] - 0) < 30 && Math.abs(px[1] - 160) < 30 && Math.abs(px[2] - 80) < 30, px);
  const sess = fs.readFileSync(path.join(run, 'session'), 'utf8');
  ok('9. the session is recorded for the desktop\'s marker', /^\d+\tboss\t127\.0\.0\.1\t\d+\t0/.test(sess), sess);
  // click on the input (40px margin) and type
  const box = await v.evaluate(() => { const r = c.getBoundingClientRect(); return {x: r.left, y: r.top, w: r.width, h: r.height} });
  const at = (x, y) => [box.x + x * box.w / 800, box.y + y * box.h / 600];
  /* where the page is on the X screen (the browser's own bar is above it) */
  const off = await hp.evaluate(() => [screenX + (outerWidth - innerWidth) / 2, screenY + outerHeight - innerHeight]);
  await v.mouse.click(...at(400, 400)); await wait(600);
  const clicks = await hp.textContent('#c'), m = /^1 (\d+),(\d+)$/.exec(clicks);
  ok('10. a click reaches the computer at the right place', m && Math.abs(+m[1] - 400) <= 3 && Math.abs(+m[2] + off[1] - 400) <= 3, [clicks, off]);
  await hp.focus('#t');
  await v.keyboard.type('Hi 5!'); await wait(800);
  ok('11. typing reaches the computer (letters, Shift, space, digits)', await hp.inputValue('#t') == 'Hi 5!', await hp.inputValue('#t'));
  await hp.evaluate(() => document.body.style.background = 'rgb(200,40,40)'); await wait(1500);
  const px2 = await v.evaluate(() => Array.from(c.getContext('2d').getImageData(700, 500, 1, 1).data));
  ok('12. changes on the computer\'s screen arrive', px2[0] > 170 && px2[1] < 70, px2);
  fs.writeFileSync(path.join(run, 'end'), '1\n'); await wait(1500);
  ok('13. ended at the computer: the viewer says so', await v.isVisible('#bye') && /ended the session/.test(await v.textContent('#byeMsg')) && !fs.existsSync(path.join(run, 'session')));
  // unattended account
  await v.goto('https://127.0.0.1:8443/'); await wait(400); await v.fill('#user', 'kid'); await v.fill('#pass', 'k1'); await v.click('#go'); await wait(2500);
  ok('14. an account with unattended access connects without approval', await v.isVisible('#c') && !fs.existsSync(path.join(run, 'pending')) && /\tkid\t.*\t1\n$/.test(fs.readFileSync(path.join(run, 'session'), 'utf8')));
  // a second viewer while one is connected
  const v2 = await vc.newPage(); await v2.goto('https://127.0.0.1:8443/'); await wait(300); await v2.fill('#user', 'kid'); await v2.fill('#pass', 'k1'); await v2.click('#go'); await wait(2000);
  ok('15. one session at a time', /already connected/.test(await v2.textContent('#byeMsg')));
  await v.click('#end'); await wait(1000);
  // the WebSocket refuses another origin and no session
  const st = await v.evaluate(() => fetch('/status').then(r => r.json()));
  ok('16. /status: paired and signed in', st.paired && st.signedIn, st);
  const nb = await vb.newContext({ignoreHTTPSErrors: true}); const n = await nb.newPage(); await n.goto('https://127.0.0.1:8443/');
  const ws = await n.evaluate(() => new Promise(r => { const w = new WebSocket('wss://' + location.host + '/ws'); w.onopen = () => r('open'); w.onerror = () => r('refused') }));
  ok('17. no WebSocket without signing in', ws == 'refused');
  for (let i = 0; i < 5; i++) { await n.fill('#code', '123456'); await n.fill('#user', 'boss'); await n.fill('#pass', 'bad' + i); await n.click('#go'); await wait(1600) }
  await n.fill('#pass', 'b1'); await n.click('#go'); await wait(1200);
  ok('18. five wrong passwords lock signing in for a minute', /Too many wrong tries/.test(await n.textContent('#msg')), await n.textContent('#msg'));
  fs.rmSync(path.join(run, 'on'));
  const offp = await (await vb.newContext({ignoreHTTPSErrors: true})).newPage(); const r = await offp.goto('https://127.0.0.1:8443/');
  ok('19. turned off: it answers nothing but "off"', r.status() == 403);
  ok('20. no page errors in the viewer', !errs.length, errs);
  if (process.env.DBG) console.log(log.split('\n').filter(l => /^IN /.test(l)).slice(0, 12).join('\n'));
  console.log(`${R.filter(x => x).length}/${R.length} passed`);
  await vb.close(); await host.close(); srv.kill(); X.kill(); process.exit(R.every(x => x) ? 0 : 1);
})();
