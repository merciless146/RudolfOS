// 1.6.0 (docs/plan-1.6.0.md Part 18): Blitzen looks things up before it answers. The page with a stand-in rudolfd
// (ai/know/...) and a stand-in model: the prompt carries the pieces found and the notes; the answer's sources are links
// (a source file opens at its line, a web page in Oracle); "Search the web" sends only the question, and is off again
// after it; off, nothing goes to the web; "Look it up first" off: nothing looked up; "Remember that ..." keeps a note;
// what the account does is sent only while its learning is on; Settings › Apps › Blitzen: what it knows; What Blitzen
// knows: Forget asks first.   node blitzknow60_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const M = {search: [], web: [], show: [], activity: [], forget: [], learnSet: [], learn: false, asked: [],
  items: [{kind: 'source', src: 'iso/sys/rudolf-native.c', where: 'line 1661', score: 3, text: 'static void trash_empty(void) { /* the Trash is emptied on the desktop thread */ }'},
    {kind: 'log', src: 'the health log', where: 'line 40', score: 2, text: '05:09 ThreadPoolSingl stopped (trap)'}],
  pages: [{url: 'https://example.org/steam', title: 'Fixing Steam', text: 'Steam needs 32-bit libraries.'}, {url: 'https://example.org/two', title: 'Two', text: 'More.'}],
  list: [{item: 2, t: 1795000000, kind: 'launch', text: 'Opened Notepad'}, {item: 1, t: 1794900000, kind: 'error', text: 'Steam: could not start'}]};
const sse = t => 'data: ' + JSON.stringify({choices: [{delta: {content: t}}]}) + '\n\ndata: [DONE]\n\n';
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0), Q = new URLSearchParams(qs || ''); let body = '';
  q.on('data', c => body += c); q.on('end', () => {
    const J = (o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"usb",live:1,recovery:0};</script>')) }
    if (u == '/api/events/wait') { setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":""}') }, 800); return }
    if (u == '/api/ai/status') return J({ok: true, installed: true, persistent: true, cpu: 2, model: 'stand-in', running: true});
    if (u == '/api/ai/chat') { M.asked.push(JSON.parse(body)); s.writeHead(200, {'content-type': 'text/event-stream'}); return s.end(sse('The Trash is emptied on the desktop thread [1].')) }
    if (u == '/api/ai/know/search') { M.search.push(Q.get('q')); return J({ok: true, items: M.items}) }
    if (u == '/api/ai/know/web') { M.web.push({q: Q.get('q'), tor: Q.get('tor'), body}); return J({ok: true, items: M.pages}) }
    if (u == '/api/ai/know/show') { M.show.push([Q.get('src'), Q.get('line')]); return J({ok: true, text: '  1661  static void trash_empty(void)'}) }
    if (u == '/api/ai/know/learn') { if (Q.get('on')) { M.learn = Q.get('on') == '1'; M.learnSet.push(Q.get('on')) } return J({ok: true, learn: M.learn}) }
    if (u == '/api/ai/know/activity') { if (M.learn) M.activity.push(body); return J({ok: true, message: M.learn ? 'kept' : 'not kept'}) }
    if (u == '/api/ai/know/status') return J({ok: true, source: true, serial: '2026111812', source_bytes: 12e6, logs: true, logs_bytes: 3e5, built: 1795000000, account: true, learn: M.learn});
    if (u == '/api/ai/know/list') return J({ok: true, items: M.list});
    if (u == '/api/ai/know/forget') { M.forget.push(qs); return J({ok: true, message: 'forgotten'}) }
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8346, '127.0.0.1');
let p; const until = (f, t) => p.waitForFunction(f, null, {timeout: t || 15000}).catch(() => {});
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  p = await b.newPage({viewport: {width: 1280, height: 900}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8346/'); await p.waitForFunction(() => typeof blzKnowFor == 'function' && typeof blitzenApp == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];svj(blzkMemKey(),[]);window.webOpen=u=>{window.__web=u};wins.slice().forEach(w=>w.close());return 1})()");
  await q("(async()=>{window.__W=await blitzenApp();return 1})()"); await until("!!document.querySelector('.blzlook')");
  const bar = await q("(()=>{const b=document.querySelector('.blzbar');return {look:b.querySelector('.blzlook').checked,web:b.querySelector('.blzweb').checked,tor:b.querySelector('.blztor').hidden}})()");
  ok('1. the chat: "Look it up first" on, "Search the web" off (and no Tor choice until it is on)', bar.look && !bar.web && bar.tor, bar);
  const ask = async (t, n) => { await q(`(async()=>{__W.blz.ask(${JSON.stringify(t)});return 1})()`); await until(`document.querySelectorAll('.blza').length>=${n}&&!__W.blz.busy()`) };
  await ask('Why did my Trash freeze?', 1); await until("!!document.querySelector('.blzsrc')");
  const sys = M.asked[0] && M.asked[0].messages[0].content;
  ok('2. looked up with the question', M.search[0] == 'Why did my Trash freeze?', M.search);
  ok('...the pieces in the prompt, each with where it came from', /\[1\] iso\/sys\/rudolf-native\.c, line 1661 \(Rudolf OS's source\)\nstatic void trash_empty/.test(sys) && /\[2\] the health log, line 40\n05:09 ThreadPoolSingl/.test(sys) && /name each one you use as \[1\], \[2\]/.test(sys), sys);
  ok('...nothing went to the web', M.web.length == 0, M.web);
  const src = await q("[...document.querySelectorAll('.blzsrcb')].map(b=>b.textContent)");
  ok('3. the answer\'s sources under it', src.length == 2 && /^\[1\] 📄 iso\/sys\/rudolf-native\.c, line 1661$/.test(src[0]) && /^\[2\] 📋 the health log, line 40$/.test(src[1]), src);
  await q("document.querySelector('.blzsrcb').click()"); await until("!!document.querySelector('.blzview')&&/1661/.test(document.querySelector('.blzview').textContent)");
  ok('...a source opens the file at its line', M.show.length == 1 && M.show[0][0] == 'iso/sys/rudolf-native.c' && M.show[0][1] == '1661' && /trash_empty/.test(await q("document.querySelector('.blzview').textContent")), M.show);
  await q("document.querySelectorAll('.blzsrcb')[1].click()"); await until("document.querySelectorAll('.blzview').length>=2");
  ok('...a log opens its piece', /ThreadPoolSingl/.test(await q("[...document.querySelectorAll('.blzview')].at(-1).textContent")));
  await q("wins.filter(w=>w.body.querySelector('.blzview')).forEach(w=>w.close())");
  // "Remember that ..."
  await ask('Remember that I use the Native desktop', 2); await until("document.querySelectorAll('.blzsrc').length>=2");
  ok('4. "Remember that ...": a note kept for this account', JSON.stringify(await q("blzkNotes()")) == '["I use the Native desktop"]' && /Kept as a note: I use the Native desktop/.test(await q("[...document.querySelectorAll('.blzsrc')].at(-1).textContent")));
  ok('...and sent with the question', /Notes the person asked you to remember[^\n]*\n- I use the Native desktop/.test(M.asked[1].messages[0].content), M.asked[1].messages[0].content.slice(0, 400));
  // the web, asked for
  await q("(()=>{const w=document.querySelector('.blzweb');w.checked=true;w.onchange();return 1})()");
  ok('5. "Search the web" on: through Tor offered', await q("!document.querySelector('.blztor').hidden"));
  const n0 = M.search.length; await ask('steam will not start', 3); await until("document.querySelectorAll('.blzsrc').length>=3");
  ok('...only the question goes out', M.web.length == 1 && M.web[0].q == 'steam will not start' && !M.web[0].tor && M.web[0].body == '', M.web);
  const sys3 = M.asked[2].messages[0].content;
  ok('...the pages read, numbered [W1], [W2], in the prompt', /\[W1\] Fixing Steam\nSteam needs 32-bit libraries\./.test(sys3) && /\[W2\] Two/.test(sys3) && M.search.length == n0 + 1, sys3.slice(-500));
  const wsrc = await q("[...[...document.querySelectorAll('.blzsrc')].at(-1).querySelectorAll('.blzsrcb')].map(b=>b.textContent)");
  ok('...its sources: the pages as links', wsrc.includes('[W1] 🌐 Fixing Steam') && wsrc.includes('[W2] 🌐 Two'), wsrc);
  await q("[...[...document.querySelectorAll('.blzsrc')].at(-1).querySelectorAll('.blzsrcb')].find(b=>/W1/.test(b.textContent)).click()");
  ok('...a page opens in Oracle', await q("window.__web") == 'https://example.org/steam');
  ok('...and "Search the web" is off again after the question', !(await q("document.querySelector('.blzweb').checked")) && await q("document.querySelector('.blztor').hidden"));
  await ask('and what about proton?', 4);
  ok('6. the next question: nothing to the web', M.web.length == 1, M.web);
  // through Tor
  await q("(()=>{const w=document.querySelector('.blzweb');w.checked=true;w.onchange();document.querySelector('.blztorc').checked=true;return 1})()");
  await ask('tor question', 5); ok('7. through Tor when asked', M.web.length == 2 && M.web[1].tor == '1' && M.web[1].q == 'tor question', M.web);
  // "Look it up first" off
  await q("(()=>{const l=document.querySelector('.blzlook');l.checked=false;l.onchange();return 1})()"); const n1 = M.search.length;
  await ask('just chat', 6);
  ok('8. "Look it up first" off: nothing looked up (the notes still go)', M.search.length == n1 && !/computer's own records/.test(M.asked.at(-1).messages[0].content) && /I use the Native desktop/.test(M.asked.at(-1).messages[0].content));
  ok('...and remembered', await q("ld('bd95blzlook',true)") === false);
  await q("(()=>{const l=document.querySelector('.blzlook');l.checked=true;l.onchange();return 1})()");
  // what the account does: only while its learning is on
  await q("(()=>{launch('notepad');notify('Steam could not start','exit code 1',{kind:'error'});return 1})()"); await new Promise(r => setTimeout(r, 2500));
  ok('9. learning off: nothing kept of what the account does', M.activity.length == 0, M.activity);
  await q("(()=>{wins.slice().forEach(w=>w.close());settings('apps');return 1})()"); await until("!!document.querySelector('.bklearn')");
  ok('10. Settings › Apps: "Blitzen: what it knows", learning off at first', await q("(()=>{const l=document.querySelector('.bklearn');return !!l&&!l.checked&&/Blitzen: what it knows/.test(document.querySelector('.bkset').parentElement.textContent)})()"));
  await q("(()=>{const l=document.querySelector('.bklearn');l.checked=true;l.onchange();return 1})()"); await until("/learn from what you do from now on/.test(document.querySelector('.bkset .bkm').textContent)");
  ok('...switched on (for this account, through rudolfd)', M.learnSet.join() == '1' && M.learn);
  await q("(()=>{launch('notepad');notify('Steam could not start','exit code 1',{kind:'error'});notify('Copied','',{quiet:true});return 1})()"); await until("false", 3000);
  ok('11. on: an app opened and an error met are kept', M.activity.includes('launch\tOpened Notepad') && M.activity.includes('error\tSteam could not start: exit code 1'), M.activity);
  ok('...a plain notice is not', !M.activity.some(a => /Copied/.test(a)), M.activity);
  ok('...the note is in Memory', await q("[...document.querySelectorAll('.bkmem .blzsee1')].map(x=>x.textContent).join('|')") == '📝I use the Native desktop✕');
  await q("(()=>{document.querySelector('.bknew').value='I like short answers';document.querySelector('.bkadd').click();return 1})()"); await until("document.querySelectorAll('.bkmem .blzsee1').length==2");
  await q("document.querySelector('.bkmem button').click()"); await until("document.querySelectorAll('.bkmem .blzsee1').length==1");
  ok('12. Memory: a note added, one deleted', JSON.stringify(await q("blzkNotes()")) == '["I like short answers"]');
  await q("document.querySelector('.bksee').click()"); await until("!!document.querySelector('.blzseel')");
  const see = await q("(()=>{const b=document.querySelector('.blzsee');return {rows:[...b.querySelectorAll('.blzseel .blzsee1')].map(x=>x.children[1].textContent),src:b.querySelector('td+td').textContent}})()");
  ok('13. What Blitzen knows: the source, the logs, what was kept (newest first)', /release 2026111812/.test(see.src) && see.rows.join('|') == 'Opened Notepad|Steam: could not start', see);
  await q("document.querySelector('.bkfa').click()"); await until("!!document.querySelector('.rmodal')");
  ok('14. Forget everything asks first', /Forget everything Blitzen kept/.test(await q("document.querySelector('.rmodal').textContent")));
  await q("document.querySelector('.rmodal .no').click()"); ok('...Cancel: nothing forgotten', M.forget.length == 0);
  await q("document.querySelector('.bkfa').click()"); await until("!!document.querySelector('.rmodal')"); await q("document.querySelector('.rmodal .ok').click()"); await until("false", 800);
  await q("document.querySelector('.blzseel button').click()"); await until("false", 800);
  ok('...Forget: everything, and one item', M.forget.join('|') == 'all=1|item=2', M.forget);
  ok('15. no page errors', errs.length == 0, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL', e); process.exit(1) });
