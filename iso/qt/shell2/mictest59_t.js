// 2026-11-14: Settings › Volume Controls › Microphones: the list of microphones (rudolfd audio/mics; choosing one
// posts audio/mic) and the Test button's equalizer, with Chromium's fake microphone (a tone): bars move, the level
// shows, it says it was heard; Stop ends it; closing Settings ends it; choosing another restarts it on that one.
//   node mictest59_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = {posts: [], def: 'alsa_input.card1.analog-stereo'};
const MICS = () => ({ok: true, mics: [{name: 'alsa_input.card2.stereo-fallback', desc: 'acp6x Stereo', default: M.def == 'alsa_input.card2.stereo-fallback'}, {name: 'alsa_input.card1.analog-stereo', desc: 'HD-Audio Generic Analog Stereo', default: M.def == 'alsa_input.card1.analog-stereo'}]});
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') { await new Promise(r => setTimeout(r, 800)); s.writeHead(200); return s.end('{"ok":true,"events":""}') }
    if (u == '/api/audio/mics') { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(MICS())) }
    if (u == '/api/audio/mic' && q.method == 'POST') { const n = new URLSearchParams(qs).get('name'); M.posts.push(n); M.def = n; s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true,"message":"microphone chosen"}') }
    if (u == '/api/audio') { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify({ok: true, cards: ' 1 [Generic_1      ]: HDA-Intel - HD-Audio Generic\n 2 [acp6x          ]: acp6x - acp6x\n', pcm: '01-00: ALC : ALC : playback 1 : capture 1\n02-00: DMIC : DMIC : capture 1\n'})) }
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8338, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--use-fake-device-for-media-stream', '--use-fake-ui-for-media-stream', '--autoplay-policy=no-user-gesture-required']});
  const ctx = await b.newContext({viewport: {width: 1280, height: 900}}); await ctx.grantPermissions(['microphone'], {origin: 'http://127.0.0.1:8338'});
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8338/'); await p.waitForFunction(() => typeof micTestBox == 'function' && typeof settings == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];USER.r='admin';settings('vol');return 1})()"); await wait(1500);
  const opts = await q("[...document.querySelectorAll('.mtsel option')].map(o=>[o.value,o.textContent,o.selected])");
  ok('1. Microphones: a list of the sound server\'s microphones, the default chosen', opts.length == 2 && opts[1][2] && /acp6x/.test(opts[0][1]), opts);
  ok('...with a Test button, not pressed', (await q("(()=>{const b=document.querySelector('.mtgo');return b&&b.textContent=='Test'&&b.getAttribute('aria-pressed')=='false'})()")));
  await q("document.querySelector('.mtgo').click()"); await wait(2500);
  const s1 = await q("(()=>{const box=document.querySelector('.mtbox'),cv=box.querySelector('.mteq'),g=cv.getContext('2d'),d=g.getImageData(0,0,cv.width,cv.height).data;let lit=0;for(let i=0;i<d.length;i+=4)if(d[i+3]>0)lit++;return {lit,level:+box.querySelector('.mtlv').dataset.v,heard:box.dataset.heard,peak:MT.peak,btn:box.querySelector('.mtgo').textContent,pressed:box.querySelector('.mtgo').getAttribute('aria-pressed'),st:box.querySelector('.mtst').textContent}})()");
  ok('2. Test: the equalizer shows what the microphone hears (bars drawn, the level meter reached)', s1.lit > 200 && s1.peak > 0 && s1.heard == "1", s1);
  ok('...the button reads Stop test, pressed, and says what it listens to', s1.btn == 'Stop test' && s1.pressed == 'true' && /^Listening to /.test(s1.st), s1);
  await q("(()=>{const s=document.querySelector('.mtsel');s.value='alsa_input.card2.stereo-fallback';s.dispatchEvent(new Event('change'));return 1})()"); await wait(1500);
  ok('3. another microphone chosen: made the default for every app (audio/mic), the test goes on with it', M.posts.at(-1) == 'alsa_input.card2.stereo-fallback' && (await q("!!MT.stream&&document.querySelector('.mtgo').textContent=='Stop test'")), [M.posts, await q("document.querySelector('.mtst').textContent")]);
  await q("document.querySelector('.mtgo').click()"); await wait(300);
  ok('4. Stop test: the microphone is let go, the button back to Test', (await q("!MT.stream&&document.querySelector('.mtgo').textContent=='Test'&&/heard/.test(document.querySelector('.mtst').textContent)")), await q("document.querySelector('.mtst').textContent"));
  await q("document.querySelector('.mtgo').click()"); await wait(800);
  await q("wins.slice().forEach(w=>w.close())"); await wait(500);
  ok('5. closing Settings ends the test (the microphone let go)', await q("!MT.stream"));
  ok('6. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
