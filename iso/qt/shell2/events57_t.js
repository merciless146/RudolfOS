// 1.5.7 (plan stage 1): the Event Viewer's events go to the account's folder (.rudolf/events.log, rudolfd's home/append),
// and wait while the folder is locked. home56.js's block run with stand-ins for logEvt, hdOn and rd.   node events57_t.js
const fs = require('fs'), path = require('path'), vm = require('vm');
const src = fs.readFileSync(path.join(__dirname, '../../../desktop/home56.js'), 'utf8');
const blk = src.slice(src.indexOf('/* 1.5.7: the Event Viewer'));
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (!c && d !== undefined ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const S = {open: false, calls: [], fail: 0, kept: []};
  const ctx = {setTimeout: (f, ms) => setTimeout(f, Math.min(ms, 30)), Date, String, encodeURIComponent,
    hdOn: () => S.open, hdQ: () => 'acct=u-boss',
    rd: async (p, o) => { S.calls.push({p, b: o && o.body}); if (S.fail) throw new Error('HTTP 500'); return {ok: true} }};
  ctx.logEvt = function (k, m) { if (m) S.kept.push(k + ':' + m) };
  vm.createContext(ctx); vm.runInContext('var logEvt=this.logEvt;' + blk + ';this.logEvt=logEvt;this.EVQ=EVQ', ctx);
  ok('0. the block is in home56.js', blk.length > 100 && /home\/append/.test(blk));
  ctx.logEvt('sys', 'Signed in as Boss'); ctx.logEvt('fw', 'Blocked\n example.com'); ctx.logEvt('sys', '');
  ok('1. the Event Viewer still keeps each event', S.kept.join('|') == 'sys:Signed in as Boss|fw:Blocked\n example.com', S.kept);
  await wait(120);
  ok('2. nothing is sent while the folder is locked; the events wait', S.calls.length == 0 && ctx.EVQ.length == 2, S.calls);
  S.open = true; await wait(120);
  const c = S.calls[0] || {};
  ok('3. once open, they go in one request to .rudolf/events.log', S.calls.length == 1 && c.p == 'home/append?acct=u-boss&path=.rudolf%2Fevents.log', S.calls);
  ok('4. one line each: time, kind, the message on one line', /^\d{4}-\d\d-\d\dT[\d:.]+Z sys Signed in as Boss\n\S+ fw Blocked example\.com\n$/.test(c.b || ''), c.b);
  ok('5. the queue is empty after', ctx.EVQ.length == 0);
  S.fail = 1; ctx.logEvt('av', 'Scan finished'); await wait(120);
  ok('6. a failed write keeps the event for later', ctx.EVQ.length == 1, ctx.EVQ);
  S.fail = 0; await wait(150);
  ok('7. ...and it is written on the next try', ctx.EVQ.length == 0 && / av Scan finished\n$/.test(S.calls[S.calls.length - 1].b), S.calls);
  S.open = false; for (let i = 0; i < 250; i++) ctx.logEvt('sys', 'e' + i);
  ok('8. at most 200 wait (the newest)', ctx.EVQ.length == 200 && / e249$/.test(ctx.EVQ[199]) && / e50$/.test(ctx.EVQ[0]));
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
