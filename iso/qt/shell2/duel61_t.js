// duel61.js (1.6.1): duels on the local network for the Library's games, the desktop's side. A game's sandboxed frame
// (a Library web app) sends the game kit's lanHost, lanFind and lanStop (gamekit 1.3.0's RG.lan); the desktop asks the
// person before hosting, then asks rudolfd (duel/host, duel/find, duel/stop: with its token, as every request), and
// stops the service when the hosting game's window closes. An outside developer's app is refused. The desktop page
// with a stand-in rudolfd; the games are small pages of this test's own sending the kit's messages.
//   node duel61_t.js [CHROME]   (build iso/www/index.html first; port 8851)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), PORT = 8851;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const PK = (name, title, signed) => ({name, version: '0.5.2', kind: 'webapp', title, summary: 'A test game', icon: '🎮', license: 'GPL-2.0-or-later', signed, mounted: true, update: null, entry: 'www/index.html'});
const M = {inst: {'rudolf-emberreach': PK('rudolf-emberreach', 'Ember Reach', 'library'), 'dev-duelgame': PK('dev-duelgame', 'Dev Duel', 'developer')}, calls: []};
/* the game: posts a game kit message and keeps the reply (window.ask(op, extra) -> the reply) */
const GAME = `<!doctype html><meta charset=utf-8><body>game<script>
let n=0;window.ask=(op,o)=>new Promise(r=>{const id=++n;const h=e=>{const m=e.data;if(m&&m.rudolfGame===1&&m.op=='reply'&&m.id===id){removeEventListener('message',h);r(m)}};addEventListener('message',h);parent.postMessage(Object.assign({rudolfGame:1,op,id,game:'ember-reach'},o||{}),'*')});
</script>`;
const api = (a, q, tok, method) => {
	M.calls.push(method + ' ' + a + (q ? '?' + q : '') + (tok == 't' ? '' : ' (no token)'));
	if (a == 'pkg/status') return {ok: true, persistent: true, sources: [], installed: Object.values(M.inst), available: []};
	if (a == 'duel/host') return {ok: true, url: 'ws://127.0.0.1:47860/', code: 'HANDUE', port: 47860};
	if (a == 'duel/find') return {ok: true, games: [{name: 'Den PC', addr: '192.168.1.20', port: 47861, url: 'ws://192.168.1.20:47861/', code: 'ABC234', ver: '0.5.2'}]};
	if (a == 'net') return {ok: true, interfaces: [{name: 'eth0', state: 'up', ip: '10.0.2.15', default: true}]};
	if (a == 'apps/status') return {ok: true, flatpak: true, persistent: true};
	if (a == 'apps/installed') return {ok: true, apps: []};
	return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
	q.on('data', () => {}); q.on('end', () => {
		if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
			return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
		if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200, {'content-type': 'application/json'}); s.end('{"ok":true,"events":[]}') }, 20000);
			s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.headers['x-rudolf-token'], q.method))) }
		const m = u.match(/^\/pkg\/(rudolf-emberreach|dev-duelgame)\/www\/index\.html$/);
		if (m) { s.writeHead(200, {'content-type': 'text/html', 'content-security-policy': 'sandbox allow-scripts allow-forms allow-modals allow-downloads allow-pointer-lock'}); return s.end(GAME) }
		s.writeHead(404); s.end() }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
	const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
	const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
	try {
		await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof pkgOpen == 'function' && window.__games58 && window.__duel61 && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
		await p.evaluate(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0]; wins.slice().forEach(w => w.close()); localStorage.setItem('bd95lib52', JSON.stringify({done: 1})) });
		const frame = async name => { for (let i = 0; i < 60; i++) { const f = p.frames().find(f => f.url().includes('/pkg/' + name + '/www/index.html')); if (f && await f.evaluate(() => typeof ask == 'function').catch(() => false)) return f; await wait(100) } return null };
		const dlg = () => p.evaluate(() => { const d = document.querySelector('.natdlg'); return d ? d.innerText : null });
		const press = re => p.evaluate(re => { const d = document.querySelector('.natdlg'); const x = d && [...d.querySelectorAll('button')].find(x => new RegExp(re).test(x.textContent)); if (x) x.click(); return !!x }, re);
		await p.evaluate(() => pkgOpen('rudolf-emberreach')); const g = await frame('rudolf-emberreach');
		ok('1. a Library game opens in its sandboxed frame and talks to the desktop', !!g && (await g.evaluate(() => ask('hello'))).ok);
		/* hosting: the person is asked first; No: nothing asked of rudolfd */
		M.calls.length = 0;
		let pr = g.evaluate(() => ask('lanHost', {ver: '0.5.2'})); await p.waitForSelector('.natdlg', {timeout: 5000}).catch(() => {});
		const q1 = await dlg(); await press('Not now'); const r1 = await pr;
		ok('2. lanHost: the desktop asks the person first ("Host a duel on this network?", naming the game and that only the local network can join); Not now: refused, rudolfd not asked', /Host a duel on this network\?/.test(q1 || '') && /Ember Reach wants to host a duel/.test(q1) && /local network only, never to the internet/.test(q1) && r1.ok === false && /chose not to host/.test(r1.error) && !M.calls.some(c => /duel/.test(c)), {q1, r1, calls: M.calls});
		pr = g.evaluate(() => ask('lanHost', {ver: '0.5.2'})); await p.waitForSelector('.natdlg', {timeout: 5000}).catch(() => {}); await press('^Host$'); const r2 = await pr;
		ok('3. Host: rudolfd asked (POST duel/host with the game\'s version, with its token), and the game gets the address to join and the code', r2.ok && r2.url == 'ws://127.0.0.1:47860/' && r2.code == 'HANDUE' && M.calls.includes('POST duel/host?ver=0.5.2'), {r2, calls: M.calls});
		const r3 = await g.evaluate(() => ask('lanFind'));
		ok('4. lanFind: rudolfd\'s search (GET duel/find), the duels found as the game kit gives them (name, address, code, version)', r3.ok && r3.games.length == 1 && r3.games[0].name == 'Den PC' && r3.games[0].url == 'ws://192.168.1.20:47861/' && r3.games[0].code == 'ABC234' && r3.games[0].ver == '0.5.2' && !('addr' in r3.games[0]) && M.calls.includes('GET duel/find'), {r3, calls: M.calls});
		M.calls.length = 0; const r4 = await g.evaluate(() => ask('lanStop'));
		ok('5. lanStop: the duel service stopped (POST duel/stop)', r4.ok && M.calls.includes('POST duel/stop'), {r4, calls: M.calls});
		M.calls.length = 0; const r5 = await g.evaluate(() => ask('lanStop'));
		ok('...a second time, with nothing hosted: nothing asked', r5.ok && !M.calls.some(c => /duel/.test(c)), M.calls);
		const r6 = await g.evaluate(() => ask('lanHost', {ver: '0.5.2;rm'})), r7 = await g.evaluate(() => ask('frob'));
		ok('6. a version that is not one is refused without asking; an op the desktop doesn\'t know is still "unknown op"', r6.ok === false && /bad version/.test(r6.error) && !(await dlg()) && r7.ok === false && r7.error == 'unknown op', {r6, r7});
		/* the hosting game's window closed: the service stopped */
		pr = g.evaluate(() => ask('lanHost', {ver: '0.5.2'})); await p.waitForSelector('.natdlg', {timeout: 5000}).catch(() => {}); await press('^Host$'); await pr;
		M.calls.length = 0; await p.evaluate(() => wins.slice().forEach(w => w.close())); await wait(2600);
		ok('7. the hosting game\'s window closed: the desktop stops its duel service', M.calls.includes('POST duel/stop'), M.calls);
		/* an outside developer's app: refused, nothing asked */
		await p.evaluate(() => pkgOpen('dev-duelgame')); const d = await frame('dev-duelgame'); M.calls.length = 0;
		const r8 = d ? await d.evaluate(() => ask('lanHost', {ver: '0.5.2'})) : null, r9 = d ? await d.evaluate(() => ask('lanFind')) : null;
		ok('8. an outside developer\'s app (signed through a developer certificate) is refused the network\'s duels: no question, nothing asked of rudolfd', d && r8.ok === false && /Library's own games/.test(r8.error) && r9.ok === false && !(await dlg()) && !M.calls.some(c => /duel/.test(c)), {r8, r9, calls: M.calls});
		ok('9. no errors', errs.length == 0, errs);
	} catch (e) { R.push(false); console.log('FAIL (threw) ' + (e.stack || e)) }
	finally { await b.close(); srv.close() }
	console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
