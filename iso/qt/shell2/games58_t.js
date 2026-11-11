// games58.js: games from the Rudolf OS Library on the desktop. Fault Lines in the Start menu and the App Store's Games
// tab; opened without its package it offers the download; the game kit's messages (saves kept per account in the
// desktop's database, pause when the window is minimised or the computer locks). The desktop page with a stand-in
// rudolfd, and the game itself from a rudolfos-library checkout next to this repository (with lib/gamekit).
//   node games58_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), LIBR = path.join(ROOT, '../rudolfos-library');
const GW = path.join(LIBR, 'apps/rudolf-faultlines/www'), KIT = path.join(LIBR, 'lib/gamekit/gamekit.js');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
if (!fs.existsSync(GW) || !fs.existsSync(KIT)) { console.log('SKIP (no rudolfos-library checkout with Fault Lines next to this repository)'); process.exit(0) }
const LIB = 'https://merciless146.github.io/rudolfos-library';
const AV = {'rudolf-faultlines': {name: 'rudolf-faultlines', version: '1.0.0', kind: 'webapp', size: 16456, title: 'Fault Lines', summary: 'A logic puzzle', icon: '⛏️', license: 'GPL-2.0-or-later', source: LIB, library: true}};
const M = {inst: {}, calls: [], job: {running: false, status: 0, log: ''}};
const status = () => ({ok: true, persistent: true, sources: [{loc: LIB, type: 'web', ok: true, library: true, count: 1}],
	installed: Object.values(M.inst), available: Object.values(AV).map(a => ({...a, installed: M.inst[a.name] ? M.inst[a.name].version : null}))});
const api = (a, q) => {
	M.calls.push(a + (q ? '?' + q : ''));
	const n = new URLSearchParams(q || '').get('name');
	if (a == 'pkg/status') return status();
	if (a == 'pkg/install') { const x = AV[n]; M.inst[n] = {name: n, version: x.version, kind: x.kind, title: x.title, summary: x.summary, icon: x.icon, license: x.license, signed: 'library', mounted: true, update: null}; M.job = {running: false, status: 0, log: 'Installed ' + n}; return {ok: true} }
	if (a == 'job') return {ok: true, ...M.job};
	if (a == 'net') return {ok: true, interfaces: [{name: 'eth0', state: 'up', ip: '10.0.2.15', default: true}]};
	if (a == 'apps/status') return {ok: true, flatpak: true, persistent: true};
	if (a == 'apps/installed') return {ok: true, apps: []};
	if (a == 'system') return {ok: true, renderer: 'AMD'};
	return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
	q.on('data', () => {}); q.on('end', () => {
		if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
			return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
		if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200, {'content-type': 'application/json'}); s.end('{"ok":true,"events":[]}') }, 20000);
			s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
		/* the package as tools/publish.sh lays it out (the kit copied into www/) */
		const m = u.match(/^\/pkg\/rudolf-faultlines\/www\/([a-z.]+)$/);
		if (m && M.inst['rudolf-faultlines']) { const f = m[1] == 'gamekit.js' ? KIT : path.join(GW, m[1]);
			if (fs.existsSync(f)) { s.writeHead(200, {'content-type': {html: 'text/html', js: 'text/javascript', css: 'text/css'}[f.split('.').pop()]}); return fs.createReadStream(f).pipe(s) } }
		s.writeHead(404); s.end() }) }).listen(8195, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
	const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
	const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
	await p.route(LIB + '/catalog.json', r => r.fulfill({contentType: 'application/json', body: '{"catalog":1,"apps":[]}'}));
	await p.goto('http://127.0.0.1:8195/'); await p.waitForFunction(() => typeof appStore == 'function' && window.__games58 && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
	await p.evaluate(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0]; wins.slice().forEach(w => w.close()); localStorage.setItem('bd95lib52', JSON.stringify({done: 1})) });
	const dlgBtn = async re => { await p.waitForSelector('.natdlg', {timeout: 5000}).catch(() => {}); return p.evaluate(re => { const d = document.querySelector('.natdlg'); if (!d) return null; const t = d.innerText; [...d.querySelectorAll('button')].find(x => new RegExp(re).test(x.textContent)).click(); return t }, re) };

	const menu = await p.evaluate(() => [...document.querySelectorAll('#menu ul li[data-a=faultlines]')].map(l => l.textContent));
	ok('1. Fault Lines is in the Start menu', menu.length == 1 && /Fault Lines/.test(menu[0]), menu);
	const nf = await p.evaluate(() => ({menu: [...document.querySelectorAll('#menu ul li[data-a=nightflight]')].map(l => l.textContent), lib: LIB_APPS['rudolf-nightflight'] && LIB_APPS['rudolf-nightflight'].app}));
	ok('1b. ...and Night Flight, downloaded from the Library when first opened', nf.menu.length == 1 && /Night Flight/.test(nf.menu[0]) && nf.lib == 'nightflight', nf);
	const fbm = await p.evaluate(() => [...document.querySelectorAll('#menu ul li[data-a=frostbarricade]')].map(l => l.textContent));
	ok('1c. ...and Frost Barricade', fbm.length == 1 && /Frost Barricade/.test(fbm[0]), fbm);
	await p.evaluate(() => { const W = appStore(); W.showGames ? W.showGames() : [...W.body.querySelectorAll('.tmtabs button')].find(x => x.textContent == 'Games').click() }); await wait(1500);
	const cards = await p.evaluate(() => [...document.querySelectorAll('.gmbi .gmc')].map(c => c.innerText.replace(/\s+/g, ' '))), card = cards.find(t => /Fault Lines/.test(t)) || '';
	ok('2. ...and in the App Store\'s Games tab, with Play (Night Flight too)', /Fault Lines.*logic puzzle.*Play/.test(card) && cards.some(t => /Night Flight.*bullet-hell.*Play/.test(t)), cards.filter(t => /Fault|Night/.test(t)));
	await p.evaluate(() => wins.slice().forEach(w => w.close()));

	/* opened without its package: the download is offered, then the game opens in a sandboxed frame */
	const opening = p.evaluate(() => launch('faultlines'));
	const q = await dlgBtn('^Download$'); await opening; await wait(2500);
	const fr = () => p.frames().find(f => f.url().includes('/pkg/rudolf-faultlines/www/index.html'));
	ok('3. without its package: it offers the download (with its size), and installs it', /Fault Lines needs a download/.test(q || '') && /16 KB/.test(q) && M.calls.includes('pkg/install?name=rudolf-faultlines'), q);
	const sb = await p.evaluate(() => { const f = [...document.querySelectorAll('iframe')].find(f => /rudolf-faultlines/.test(f.src)); return f && f.getAttribute('sandbox') });
	ok('4. the game opens in a window, sandboxed (no same-origin)', fr() && sb && !/allow-same-origin/.test(sb), sb);
	for (let i = 0; i < 40 && !(await fr().evaluate(() => !!window.__fl).catch(() => false)); i++) await wait(100);
	const g = fr();
	ok('5. the game kit greets the desktop and is answered', await g.evaluate(() => RG.inDesktop));
	/* saves: kept in the desktop's database, per package, game and account */
	await g.evaluate(() => RG.save({v: 1, best: {small: 42.5}, daily: {}, played: 3, won: 1}));
	await wait(300);
	const kv = await p.evaluate(() => kvGet('game:rudolf-faultlines:fault-lines:' + uid())); const key = await p.evaluate(() => uid());
	ok('6. a save is kept in the desktop\'s database (mirrored by rudolfd) for this account', kv && kv.best && kv.best.small == 42.5 && key != 'nobody', {kv, key});
	const big = await g.evaluate(() => RG.save({x: 'a'.repeat(300000)}).then(() => 'took', e => e.message));
	ok('7. a save over 256 KB is refused', /too large/.test(big), big);
	/* closed and opened again: the save comes back */
	await p.evaluate(() => wins.slice().forEach(w => w.close())); await p.evaluate(() => launch('faultlines')); await wait(2500);
	for (let i = 0; i < 40 && !(fr() && await fr().evaluate(() => !!window.__fl).catch(() => false)); i++) await wait(100);
	const back = await fr().evaluate(() => __fl.save().best.small);
	ok('8. opened again (no second download): the best time is back', back == 42.5 && M.calls.filter(c => c == 'pkg/install?name=rudolf-faultlines').length == 1, back);
	/* pausing: minimised, then locked */
	const g2 = fr();
	await g2.evaluate(() => { __fl.newGame('medium') }); const cc = await g2.evaluate(() => __fl.cellCenter(8, 8));
	const fb = await (await g2.frameElement()).boundingBox(); await p.mouse.click(fb.x + cc.x, fb.y + cc.y); await wait(300);
	await p.evaluate(() => { const W = wins.find(w => w.body.querySelector('iframe')); W.el.querySelector('.mn').click() }); await wait(1600);
	const pz = await g2.evaluate(() => ({p: RG.paused, why: RG.pauseReasons}));
	ok('9. the window minimised: the game is paused by the desktop', pz.p && pz.why.includes('desktop'), pz);
	await p.evaluate(() => { const W = wins.find(w => w.body.querySelector('iframe')); W.focus() }); await wait(1600);
	ok('10. ...restored: the desktop resumes it', await g2.evaluate(() => !RG.pauseReasons.includes('desktop')));
	await p.evaluate(() => Object.defineProperty(window, 'LOCKED', {get: () => true, configurable: true})); await wait(1600);
	ok('11. the computer locked: paused', await g2.evaluate(() => RG.paused && RG.pauseReasons.includes('desktop')));
	await p.evaluate(() => Object.defineProperty(window, 'LOCKED', {get: () => false, configurable: true})); await wait(1600);
	/* the Start menu does not list it twice once installed */
	await p.evaluate(() => pkgStatus(true)); await wait(300);
	const twice = await p.evaluate(() => ({apps: Object.keys(APPS).filter(k => /faultlines/.test(k)), menu: [...document.querySelectorAll('#menu ul li')].filter(l => /Fault Lines/.test(l.textContent)).length}));
	ok('12. installed, it is listed once (no second Packages entry)', twice.apps.join() == 'faultlines' && twice.menu == 1, twice);
	/* a frame that is not a package's gets no answer */
	const stray = await p.evaluate(() => new Promise(r => { const f = document.createElement('iframe'); f.setAttribute('sandbox', 'allow-scripts');
		f.srcdoc = '<script>let got=0;addEventListener("message",e=>{if(e.data&&e.data.op=="reply")got=1});parent.postMessage({rudolfGame:1,op:"load",id:1,game:"fault-lines"},"*");setTimeout(()=>parent.postMessage({strayGot:got},"*"),700)<\/script>';
		addEventListener('message', e => { if (e.data && 'strayGot' in e.data) { f.remove(); r(e.data.strayGot) } }); document.body.appendChild(f) }));
	ok('13. a frame that is not a Library package gets no saves', stray === 0, stray);
	ok('14. no page errors', errs.length == 0, errs);
	await b.close(); srv.close();
	console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL', e.stack); process.exit(1) });
