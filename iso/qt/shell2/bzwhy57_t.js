// 1.5.7 (plan stage 3): Blitzen diagnostics. "⚡ Why?" on error notifications, System health's events and the Event
// Viewer's errors; System health's "What's wrong with this computer?"; the person sees and trims what Blitzen reads;
// the answer runs nothing and offers "Draft a report" (Report a problem, filled in).   node bzwhy57_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm', '.json': 'application/json'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8165, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await (await b.newContext()).newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    const now = Math.floor(Date.now() / 1000);
    const H = [{t: now - 3600, k: 'start', m: 'start-up'}, {t: now - 600, k: 'browser', m: 'the browser was restarted: no heartbeat for 60 s'}, {t: now - 300, k: 'memory', m: 'closed a page of 1882 MB to free memory'}];
    window.HL = H; window.CHAT = [];
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href);
      if (q.pathname == '/api/ai/facts') return J({version: '1.5.7', uptime: 4000, health: H.map(e => JSON.stringify(e)).join('\n') + '\n',
        rudolfd: 'rudolfd: listening\ndownloads: open /home/rudolf/Downloads/setup.deb: not found (the download is not there)\nrudolfd: wifi up\n', page: 'TypeError: x is undefined at dlOpen\n', session: 'watchdog: ok\n'});
      if (q.pathname == '/api/health') return J({events: H.slice().reverse(), now});
      if (q.pathname == '/api/ai/status') return J({model: 'test', state: 'ready'});
      if (u.includes('/api/')) return J({items: [], apps: [], windows: [], events: []});
      return f0(u, o) } });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8165/'); await p.waitForFunction(() => typeof bzWhy == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');
    window.blzAskFrom=(app,question,context,o)=>{CHAT.push({app,question,context,o});return 1};return 1})()`);
  await wait(1500);
  ok('0. Blitzen has a diagnostics pack that forbids commands', await q("!!BLZ_PACKS.diag&&/Never tell the person to run commands/.test(BLZ_PACKS.diag)"));
  // 1. notifications
  await q("(()=>{document.querySelectorAll('.ntoast').forEach(x=>x.remove());notify('Download not found','setup.deb could not be opened');notify('Wallpaper changed','Your new wallpaper is set');return 1})()");
  const tw = await q("[...document.querySelectorAll('.ntoast')].map(t=>[t.querySelector('b').textContent,!!t.querySelector('.bzwhy')])");
  ok('1. an error notification has "⚡ Why?"; an ordinary one does not', tw.length == 2 && tw[0][1] === true && tw[1][1] === false, tw);
  await q("(()=>{const t=[...document.querySelectorAll('.ntoast')].find(t=>/Download not found/.test(t.textContent));t.querySelector('.bzwhy').click();return 1})()");
  await p.waitForSelector('.bzrev', {timeout: 8000});
  const parts = await q("[...document.querySelectorAll('.bzrev .bzparts > div')].map(d=>({l:d.querySelector('b').textContent,t:d.querySelector('pre').textContent}))");
  ok('2. Blitzen first shows what it will read: what failed, the matching log lines and documents', parts.some(x => x.l == 'What failed') && parts.some(x => /^Log: rudolfd/.test(x.l) && /setup\.deb: not found/.test(x.t)) && parts.some(x => /^Rudolf OS: /.test(x.l)), parts.map(x => x.l));
  ok('3. ...and the toast was closed', await q("![...document.querySelectorAll('.ntoast')].some(t=>/Download not found/.test(t.textContent))"));
  const docl = parts.find(x => /^Rudolf OS: /.test(x.l)).l;
  await q(`(()=>{const d=[...document.querySelectorAll('.bzrev .bzparts > div')].find(d=>d.querySelector('b').textContent==${JSON.stringify(docl)});d.querySelector('input').click();document.querySelector('.bzrev .ok').click();return 1})()`);
  await wait(300);
  const c = await q("CHAT[0]&&{q:CHAT[0].question,ctx:CHAT[0].context,pack:CHAT[0].o.pack,norun:CHAT[0].o.norun}");
  ok('4. asked with what was kept, not what was unticked', c && /setup\.deb: not found/.test(c.ctx) && !c.ctx.includes(docl) && /^Why did this happen: Download not found/.test(c.q), c);
  ok('5. the diagnostics pack, and no code is run from the answer', c && c.pack == 'diag' && c.norun === true, c);
  // 6. the answer's buttons; Draft a report
  await q(`(()=>{const el=document.createElement('div');el.id='ans';el.innerHTML="<div class='blzbody'></div>";document.body.appendChild(el);CHAT[0].o.onAnswer(el,'The download was deleted before it was opened: \`setup.deb: not found\`. Likely.\\n\`\`\`sh\\nrm x\\n\`\`\`');return 1})()`);
  ok('6. the answer gets "Draft a report" and "System health"', await q("[...document.querySelectorAll('#ans .bzact button')].map(b=>b.textContent).join('|')") == '🐞 Draft a report|⚙️ System health');
  await q("(()=>{document.querySelector('#ans .bzact button').click();return 1})()"); await wait(300);
  const rep = await q("(()=>{const o=document.querySelector('.rmodal .srt')?.closest('.rmodal');return o?{t:o.querySelector('.srt').value,w:o.querySelector('.srw').value,l:o.querySelector('.srl')?.checked,lt:o.querySelector('.srl')?.parentElement.textContent}:null})()");
  ok('7. Draft a report: Report a problem filled in (title, what happened, Blitzen\'s explanation without code), with the log lines', rep && /^Download not found/.test(rep.t) && /Blitzen's explanation/.test(rep.w) && /setup\.deb: not found/.test(rep.w) && !/rm x/.test(rep.w) && rep.l === true && /log line/.test(rep.lt), rep);
  ok('8. ...nothing is sent: the dialog waits for the person', await q("CHAT.length") == 1);
  await q("(()=>{document.querySelectorAll('.rmodal').forEach(o=>o.remove());return 1})()");
  // 9. System health
  await q("(()=>{settings('health');return 1})()"); await wait(1500);
  const hb = await q("(()=>{const w=document.querySelector('.hlw');const rows=[...document.querySelectorAll('.hll .acr')].map(r=>[r.textContent.replace('⚡ Why?',''),!!r.querySelector('.bzwhy')]);return {w:w&&!w.hidden,rows}})()");
  ok('9. System health has "What\'s wrong with this computer?", and "Why?" on a crash or a closed page but not a start-up', hb.w && hb.rows.some(r => /browser was restarted/.test(r[0]) && r[1]) && hb.rows.some(r => /1882 MB/.test(r[0]) && r[1]) && hb.rows.some(r => /start-up/.test(r[0]) && !r[1]), hb);
  await q("(()=>{document.querySelector('.hlw').click();return 1})()"); await p.waitForSelector('.bzrev', {timeout: 8000});
  const p2 = await q("[...document.querySelectorAll('.bzrev .bzparts > div')].map(d=>d.querySelector('b').textContent+': '+d.querySelector('pre').textContent).join('\\n')");
  ok('10. ...which reads the problems of the whole computer (crashes, errors)', /browser was restarted/.test(p2) && /1882 MB/.test(p2) && /TypeError/.test(p2), p2);
  await q("(()=>{document.querySelector('.bzrev .no').click();return 1})()"); await wait(200);
  ok('11. Cancel asks nothing', await q("CHAT.length") == 1 && !(await q("!!document.querySelector('.bzrev')")));
  // 12. Event Viewer
  await q("(()=>{SYSLOG.push({t:Date.now(),k:'error',m:'Uncaught TypeError: cannot read the file'});launch('events');return 1})()"); await wait(800);
  const ev = await q("[...document.querySelectorAll('.el tbody tr')].filter(r=>/cannot read the file/.test(r.textContent)).map(r=>!!r.querySelector('.bzwhy'))");
  ok('12. the Event Viewer\'s error has "Why?"', ev.length && ev[0], ev);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
