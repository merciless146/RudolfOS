// duel62.js (1.6.2, docs/plan-1.6.2.md Part 30): a Library game's LAN calls (the game kit's lanHost, lanFind, lanStop)
// answered by the desktop through rudolfd's duel/ requests (with the token), for the project's own Library apps only.
// The desktop page with a stand-in rudolfd; a game's package frame (sandboxed, at /pkg/NAME/www/) that sends the game
// kit's messages as the game kit does (and, when a rudolfos-library checkout with the game kit 1.3.0 is next to this
// repository, or at RUDOLFOS_LIBRARY, through RG.lan itself).
//   node duel62_t.js [CHROME]   (build iso/www/index.html first; port 8851)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), KIT = path.join(process.env.RUDOLFOS_LIBRARY || path.join(ROOT, '../rudolfos-library'), 'lib/gamekit/gamekit.js');
const PORT = 8851;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const M = {calls: [], tokens: [], host: {ok: true, url: 'ws://192.168.1.20:47731/', port: 47731, code: 'ABC234', name: 'rudolf-pc'}};
const GAMES = [{name: 'Other PC', url: 'ws://192.168.1.30:47731/', code: 'XYZ789', ver: '0.5.2', game: 'ember-reach'}];
const api = (a, q) => {
	if (a.startsWith('duel/')) { M.calls.push(a + (q ? '?' + q : '')); if (a == 'duel/host') return M.host; if (a == 'duel/find') return {ok: true, games: GAMES}; if (a == 'duel/stop') return {ok: true} }
	if (a == 'net') return {ok: true, interfaces: [{name: 'wlan0', state: 'up', ip: '192.168.1.20', default: true}]};
	return {ok: true};
};
/* the game's page: the game kit's messages as RG sends them ({rudolfGame: 1, op, id, game}); the replies kept */
const GAME = `<!doctype html><meta charset=utf-8><body><script>window.R={};let id=0;
addEventListener('message',e=>{if(e.source!==parent||!e.data||e.data.rudolfGame!==1||e.data.op!=='reply')return;R[e.data.id]=e.data});
window.ask=(op,o)=>new Promise(res=>{const i=++id;parent.postMessage(Object.assign({rudolfGame:1,op,id:i,game:'ember-reach'},o||{}),'*');const t=setInterval(()=>{if(R[i]){clearInterval(t);res(R[i])}},20);setTimeout(()=>{clearInterval(t);res(R[i]||null)},4000)});
</script>${fs.existsSync(KIT) ? '<script src="gamekit.js"></script>' : ''}`;
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
	q.on('data', () => {}); q.on('end', () => {
		if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
			return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"tok62",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
		if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200, {'content-type': 'application/json'}); s.end('{"ok":true,"events":[]}') }, 20000);
			if (u.startsWith('/api/duel/')) M.tokens.push(q.method + ' ' + u.slice(5) + ' ' + (q.headers['x-rudolf-token'] || '-'));
			s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
		const m = u.match(/^\/pkg\/(rudolf-emberreach|dev-game)\/www\/(index\.html|gamekit\.js)$/);
		if (m) { if (m[2] == 'gamekit.js') { s.writeHead(200, {'content-type': 'text/javascript'}); return fs.createReadStream(KIT).pipe(s) }
			s.writeHead(200, {'content-type': 'text/html'}); return s.end(GAME) }
		s.writeHead(404); s.end() }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
	const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
	const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
	try {
		await p.goto('http://127.0.0.1:' + PORT + '/'); await p.waitForFunction(() => window.__games58 && window.__duel62 && typeof gameLanOp == 'function', null, {timeout: 30000});
		await p.evaluate(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0]; wins.slice().forEach(w => w.close()) });
		/* the project's own game, and an outside developer's (the frames as pkgbox59.js makes them) */
		await p.evaluate(port => { for (const [n, dev] of [['rudolf-emberreach', 0], ['dev-game', 1]]) { const f = document.createElement('iframe'); f.id = 'g-' + n; f.setAttribute('sandbox', 'allow-scripts allow-forms allow-modals allow-downloads allow-pointer-lock'); if (dev) f.dataset.dev = '1'; f.dataset.grants = dev ? 'saves' : 'saves gamepad fullscreen'; f.src = '/pkg/' + n + '/www/index.html'; f.style.cssText = 'position:fixed;left:0;top:0;width:200px;height:100px'; document.body.appendChild(f) } }, PORT);
		await wait(1500);
		const fr = n => p.frames().find(f => f.url().includes('/pkg/' + n + '/www/index.html'));
		const g = fr('rudolf-emberreach'), d = fr('dev-game');
		const h = await g.evaluate(() => ask('lanHost', {ver: '0.5.2'}));
		ok('1. lanHost from the project\'s game: the desktop asks rudolfd (POST duel/host, with the token) for this game (the game kit\'s id) and version, and gives the game the relay\'s address, code and name', h && h.ok && h.url == 'ws://192.168.1.20:47731/' && h.code == 'ABC234' && h.name == 'rudolf-pc' && M.tokens.includes('POST duel/host tok62') && M.calls.includes('duel/host?ver=0.5.2&game=ember-reach'), {h, M});
		const f = await g.evaluate(() => ask('lanFind'));
		ok('2. lanFind: duel/find for this game (GET, with the token): the games on the network', f && f.ok && f.games.length == 1 && f.games[0].code == 'XYZ789' && f.games[0].url == 'ws://192.168.1.30:47731/' && M.tokens.includes('GET duel/find tok62') && M.calls.includes('duel/find?game=ember-reach') && f.games[0].ver == '0.5.2', {f, M: M.calls});
		const st = await g.evaluate(() => ask('lanStop'));
		ok('3. lanStop: duel/stop', st && st.ok && M.calls.includes('duel/stop') && M.tokens.includes('POST duel/stop tok62'), st);
		const n0 = M.calls.length;
		const bad = await g.evaluate(() => ask('lanHost', {ver: '0.5.2&game=other'}));
		ok('4. a version that is not one is refused in the page; nothing asked of rudolfd', bad && !bad.ok && M.calls.length == n0, bad);
		const dv = await d.evaluate(() => Promise.all([ask('lanHost', {ver: '1.0'}), ask('lanFind'), ask('lanStop')]));
		ok('5. an outside developer\'s app is refused all three, and rudolfd is never asked', dv.every(x => x && !x.ok && /outside developers/.test(x.error)) && M.calls.length == n0, dv);
		M.host = {ok: false, error: 'This computer isn\'t on a home network (Wi-Fi or a cable): connect to the same network as the other player.'};
		const e = await g.evaluate(() => ask('lanHost', {ver: '0.5.2'}));
		ok('6. rudolfd\'s refusal reaches the game as it is (not on a home network)', e && !e.ok && /isn't on a home network/.test(e.error), e);
		const other = await g.evaluate(() => ask('lanSomething'));
		ok('7. other messages are as before (unknown)', other && !other.ok && other.error == 'unknown op', other);
		/* the game kit itself (rudolfos-library's, when it is next to this repository and has RG.lan) */
		const kit = await g.evaluate(() => !!(window.RG && RG.lan));
		if (kit) {
			M.host = {ok: true, url: 'ws://192.168.1.20:47731/', port: 47731, code: 'QRS456', name: 'rudolf-pc'};
			const r = await g.evaluate(async () => { await RG.init('ember-reach'); return {h: await RG.lan.host({ver: '0.5.2'}), f: await RG.lan.find(), s: await RG.lan.stop()} });
			ok('8. through the game kit\'s RG.lan (1.3.0): host (the address and code), find (the games, their versions) and stop', r.h && r.h.url == 'ws://192.168.1.20:47731/' && r.h.code == 'QRS456' && r.f && r.f.games[0].name == 'Other PC' && r.f.games[0].ver == '0.5.2' && r.s && !r.s.error, r);
		} else console.log('SKIP 8. the game kit with RG.lan (rudolfos-library 1.3.0) is not next to this repository');
		ok('9. no errors in the page', errs.length == 0, errs);
	} catch (e) { R.push(false); console.log('FAIL (threw) ' + (e.stack || e)) }
	finally { await b.close(); srv.close() }
	const n = R.filter(Boolean).length; console.log(n + '/' + R.length + ' passed'); process.exit(n == R.length ? 0 : 1);
})();
