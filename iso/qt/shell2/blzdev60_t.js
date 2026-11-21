// 1.6.0 (docs/plan-1.6.0.md Part 19 item 4): Blitzen as a developer's agent, the page's side, with a stand-in rudolfd
// (ai/dev/...) and model. Developer mode off: nothing of it. On: Settings › Apps › Blitzen › Developer keeps the names and
// the tokens (sent once, never shown); Runs (a failed job's log, Blitzen asked why; a workflow started only after Allow);
// Change (a file, Blitzen's new version as a diff; a new branch and a pull request only after Allow, with exactly that
// file); Test computer (a command only after Allow; its result and screenshot); a chat's shell block sent to the test
// computer after Allow.   node blzdev60_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const PNG = Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==', 'base64');
const M = {dev: false, settings: [], tokens: [], dispatch: [], push: [], pr: [], sent: [], waits: 0, asked: [],
  conf: {repo: '', base: 'main', devlink: '', github: false, devlinkToken: false}};
const FILE = 'function clock(){\n  return hh+":"+mm;\n}\n';
const sse = t => 'data: ' + JSON.stringify({choices: [{delta: {content: t}}]}) + '\n\ndata: [DONE]\n\n';
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0), Q = new URLSearchParams(qs || ''); let body = '';
  q.on('data', c => body += c); q.on('end', () => {
    const J = (o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"usb",live:1,recovery:0};</script>')) }
    if (u == '/api/events/wait') { setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":""}') }, 800); return }
    if (u == '/api/ai/status') return J({ok: true, installed: true, persistent: true, cpu: 2, model: 'stand-in', running: true});
    if (u == '/api/ai/chat') { const b = JSON.parse(body); M.asked.push(b); s.writeHead(200, {'content-type': 'text/event-stream'});
      if (/complete new version/.test(b.messages[0].content)) return s.end(sse('```js\nfunction clock(){\n  return hh+":"+mm+":"+ss;\n}\n```'));
      return s.end(sse('Look at the log:\n```sh\ntail -n 5 /persist/var/log/health.log\n```')) }
    if (u.startsWith('/api/ai/dev/')) { const w = u.slice(12);
      if (!M.dev) return J({ok: false, error: 'Blitzen\'s developer part is there only while Developer mode is on'}, 403);
      if (w == 'settings') { if (Q.has('repo')) { M.settings.push(qs); M.conf.repo = Q.get('repo'); M.conf.base = Q.get('base'); M.conf.devlink = Q.get('devlink') }
        return J({ok: true, developer: true, ...M.conf, keyring: true, official: false}) }
      if (w == 'token') { M.tokens.push([Q.get('key'), Q.get('clear'), body]); if (Q.get('key') == 'github') M.conf.github = Q.get('clear') != '1'; else M.conf.devlinkToken = Q.get('clear') != '1'; return J({ok: true, message: 'kept'}) }
      if (w == 'runs') return J({ok: true, data: {workflow_runs: [{id: 77, name: 'Rudolf OS release', head_branch: 'main', status: 'completed', conclusion: 'failure', html_url: 'https://github.com/dev/RudolfOS/actions/runs/77'}]}});
      if (w == 'workflows') return J({ok: true, data: {workflows: [{id: 9, name: 'Rudolf OS release', path: '.github/workflows/rudolf-release.yml'}]}});
      if (w == 'jobs') return J({ok: true, data: {jobs: [{id: 501, name: 'build', conclusion: 'failure'}]}});
      if (w == 'joblog') return J({ok: true, text: 'step 1\nerror: clock31.js: hh is not defined\n'});
      if (w == 'dispatch') { M.dispatch.push(qs); return J({ok: true, data: {ok: true}}) }
      if (w == 'file') return Q.get('path') == 'desktop/long.js' ? J({ok: true, text: 'x\n'.repeat(400)}) : J({ok: true, text: FILE});
      if (w == 'push') { M.push.push({qs, body}); return J({ok: true, data: {ok: true, branch: Q.get('branch')}}) }
      if (w == 'pr') { M.pr.push({qs, body}); return J({ok: true, data: {ok: true, number: 12, url: 'https://github.com/dev/RudolfOS/pull/12'}}) }
      if (w == 'dl-send') { M.sent.push(JSON.parse(body)); return J({ok: true, data: {ok: true, id: '00000' + M.sent.length}}) }
      if (w == 'dl-wait') { M.waits++; const c = M.sent[+Q.get('id') - 1];
        if (M.waits % 2) return J({ok: true, data: {ok: true, waiting: true}});
        return J({ok: true, data: {ok: true, result: c.kind == 'screenshot' ? {id: Q.get('id'), kind: 'screenshot', status: 0, files: ['screen.png'], approved: 'allowed on the computer'} : {id: Q.get('id'), kind: 'sh', status: 0, stdout: 'health ok\n', approved: 'allowed on the computer'}}}) }
      if (w == 'dl-file') return J({ok: true, base64: PNG.toString('base64')});
      return J({ok: false, error: 'no'}, 404) }
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8347, '127.0.0.1');
let p; const until = (f, t) => p.waitForFunction(f, null, {timeout: t || 15000}).catch(() => {}); const q = s => p.evaluate(s);
const allow = async yes => { await until("!!document.querySelector('.blzdallow')"); const t = await q("document.querySelector('.blzdallow').textContent"); await p.click(`.blzdallow ${yes ? '.ok' : '.no'}`); return t };
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  p = await b.newPage({viewport: {width: 1280, height: 900}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8347/'); await p.waitForFunction(() => typeof blzDevApp == 'function' && typeof blitzenApp == 'function', null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];window.webOpen=u=>{window.__web=u};wins.slice().forEach(w=>w.close());return 1})()");
  // Developer mode off
  await q("(async()=>{window.__W=await blitzenApp();return 1})()"); await until("!!document.querySelector('.blzlook')"); await until('false', 1200);
  ok('1. Developer mode off: no "Developer…" in the chat', await q("!document.querySelector('.bddev')"));
  await q("(()=>{settings('apps');return 1})()"); await until("!!document.querySelector('.bkset')"); await until('false', 1000);
  ok('...and nothing of it in Settings', await q("!document.querySelector('.bdset .bdr')"));
  await q("(async()=>{const W=await blzDevApp();window.__D=W;return 1})()");
  ok('...its window does not open', await q("!document.querySelector('.blzdw')"));
  // on
  M.dev = true; await q("wins.slice().forEach(w=>w.close())");
  await q("(()=>{settings('apps');return 1})()"); await until("!!document.querySelector('.bdset .bdr')");
  ok('2. Developer mode on: Settings › Apps › Blitzen › Developer', await q("/Blitzen › Developer/.test(document.querySelector('.bdset').textContent)"));
  await q(`(()=>{const b=document.querySelector('.bdset');b.querySelector('.bdr').value='dev/RudolfOS';b.querySelector('.bdl').value='dev/rudolfos-devlink';b.querySelector('.bdt').value='github_pat_SECRETgithub0123456789';b.querySelector('.bdlt').value='github_pat_SECRETdevlink0123456';b.querySelector('.bdsave').click();return 1})()`);
  await until("/Saved/.test(document.querySelector('.bdset .bdm').textContent)");
  ok('...the names saved', /repo=dev%2FRudolfOS&base=main&devlink=dev%2Frudolfos-devlink/.test(M.settings[0]), M.settings);
  ok('...each token sent once, as the body (never in an address)', JSON.stringify(M.tokens) == JSON.stringify([['github', null, 'github_pat_SECRETgithub0123456789'], ['devlink', null, 'github_pat_SECRETdevlink0123456']]) && !M.settings.join().includes('SECRET'), M.tokens);
  ok('...and never shown again', await q("!document.body.innerHTML.includes('SECRETgithub')&&!document.body.innerHTML.includes('SECRETdevlink')&&/kept \\(type a new one/.test(document.querySelector('.bdt').placeholder)"));
  // Runs
  await q("(async()=>{window.__D=await blzDevApp('runs');return 1})()"); await until("!!document.querySelector('.bdruns button')");
  ok('3. Runs: the newest, with their results', await q("/Rudolf OS release.*main.*failure/.test(document.querySelector('.bdruns tbody').textContent)"));
  await q("document.querySelector('.bdruns button').click()"); await until("/hh is not defined/.test(document.querySelector('.bdlog').textContent)");
  ok('...a failed job\'s log', true);
  await q("document.querySelector('.bdgo').click()"); let t = await allow(false);
  ok('4. Start a workflow: asked first, saying which and where', /Start a workflow[\s\S]*dev\/RudolfOS[\s\S]*Rudolf OS release[\s\S]*main/.test(t), t);
  await until('false', 300); ok('...Refuse: not started', M.dispatch.length == 0);
  await q("document.querySelector('.bdgo').click()"); await allow(true); await until("/Started/.test(document.querySelector('.bdm').textContent)");
  ok('...Allow: started (that workflow, that branch)', M.dispatch.length == 1 && /wf=rudolf-release.yml&ref=main/.test(M.dispatch[0]), M.dispatch);
  const nAsk = M.asked.length; await q("document.querySelector('.bdwhy').click()"); for (let i = 0; i < 100 && M.asked.length == nAsk; i++) await new Promise(r => setTimeout(r, 200));
  ok('5. Ask Blitzen why it failed: a question with the log', M.asked.length > nAsk && /Why did \W+Rudolf OS release\W+ fail/.test(JSON.stringify(M.asked.at(-1))) && /hh is not defined/.test(JSON.stringify(M.asked.at(-1))), JSON.stringify(M.asked.at(-1).messages.slice(1)));
  // Change
  await q("wins.filter(w=>w.body.classList.contains('blzdw')).forEach(w=>w.close())");
  await q("(async()=>{window.__D=await blzDevApp('change');return 1})()"); await until("!!document.querySelector('.bdpath')");
  await q("(()=>{document.querySelector('.bdpath').value='desktop/long.js';document.querySelector('.bdload').click();return 1})()"); await until("/400 lines/.test(document.querySelector('.blzdpane .bdm').textContent)");
  ok('6. a long file: only a part of it (lines asked for)', await q("document.querySelector('.bdmake').disabled"));
  await q("(()=>{document.querySelector('.bdpath').value='desktop/clock31.js';document.querySelector('.bdload').click();return 1})()"); await until("!document.querySelector('.bdmake').disabled");
  await q("(()=>{document.querySelector('.bdwhat').value='show seconds';document.querySelector('.bdmake').click();return 1})()"); await until("!document.querySelector('.bddiff').hidden", 20000);
  const diff = await q("document.querySelector('.bddiff').textContent");
  ok('7. Blitzen\'s change shown as a diff', /- {3}return hh\+":"\+mm;\n\+ {3}return hh\+":"\+mm\+":"\+ss;/.test(diff), diff);
  await q("document.querySelector('.bdpr').click()"); t = await allow(false);
  ok('8. Push: asked first: the repository, a new branch, the file and the diff', /dev\/RudolfOS[\s\S]*New branchblitzen\/show-seconds-[\s\S]*Frommain \(not changed\)[\s\S]*desktop\/clock31\.js[\s\S]*Blitzen: show seconds/.test(t) && /\+ {3}return hh/.test(t), t);
  await until('false', 300); ok('...Refuse: nothing pushed', M.push.length == 0 && M.pr.length == 0);
  await q("document.querySelector('.bdpr').click()"); await allow(true); await until("/Pull request/.test(document.querySelector('.blzdpane .bdm').textContent)");
  const pushed = M.push[0] && M.push[0].body.split('\t');
  ok('...Allow: that file, as shown, to a new blitzen/ branch', pushed && pushed[0] == 'desktop/clock31.js' && Buffer.from(pushed[1], 'base64').toString() == 'function clock(){\n  return hh+":"+mm+":"+ss;\n}\n' && /branch=blitzen%2Fshow-seconds-/.test(M.push[0].qs), M.push);
  ok('...and a pull request, its link opening in Oracle', M.pr.length == 1 && /title=Blitzen%3A%20show%20seconds/.test(M.pr[0].qs) && /allowed by the developer/.test(M.pr[0].body) && await q("(document.querySelector('.bdlink').click(),window.__web)") == 'https://github.com/dev/RudolfOS/pull/12', M.pr);
  // Test computer
  await q("wins.filter(w=>w.body.classList.contains('blzdw')).forEach(w=>w.close())");
  await q("(async()=>{window.__D=await blzDevApp('link');return 1})()"); await until("!!document.querySelector('.bdcmd')");
  await q("(()=>{document.querySelector('.bdcmd').value='uname -a';document.querySelector('.bdsend').click();return 1})()"); t = await allow(false);
  ok('9. A command to the test computer: asked first, the script word for word', /Send a command to the test computer[\s\S]*dev\/rudolfos-devlink[\s\S]*sh[\s\S]*uname -a/.test(t), t);
  await until('false', 300); ok('...Refuse: not sent', M.sent.length == 0);
  await q("document.querySelector('.bdsend').click()"); await allow(true); await until("/health ok/.test(document.querySelector('.bdout').textContent)", 30000);
  ok('...Allow: sent, waited for, its output shown', M.sent.length == 1 && M.sent[0].script == 'uname -a' && /000001: status 0/.test(await q("document.querySelector('.bdst').textContent")), M.sent);
  await q("(()=>{const k=document.querySelector('.bdk');k.value='screenshot';k.onchange();document.querySelector('.bdsend').click();return 1})()"); await allow(true);
  await until("!document.querySelector('.bdimg').hidden", 30000);
  ok('10. a screenshot of the test computer', M.sent[1].kind == 'screenshot' && /^data:image\/png;base64,/.test(await q("document.querySelector('.bdimg').src")));
  // the chat's shell block
  await q("wins.slice().forEach(w=>w.close())");
  await q("(async()=>{window.__W=await blitzenApp();return 1})()"); await until("!!document.querySelector('.bddev')");
  ok('11. the chat: "Developer…"', true);
  await q("(async()=>{__W.blz.ask('How do I read the health log?');return 1})()"); await until("!!document.querySelector('.bdtc')", 20000);
  await q("document.querySelector('.bdtc').click()"); t = await allow(true); await until("/health ok/.test([...document.querySelectorAll('.blzout')].map(x=>x.textContent).join())", 30000);
  ok('...a shell block in an answer: on the test computer, after Allow', /tail -n 5 \/persist\/var\/log\/health.log/.test(t) && M.sent.at(-1).script == 'tail -n 5 /persist/var/log/health.log', M.sent.at(-1));
  ok('12. no page errors', errs.length == 0, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL', e); process.exit(1) });
