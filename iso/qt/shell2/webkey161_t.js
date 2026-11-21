// 1.6.1 (docs/plan-1.6.1.md Part 20): Settings › Oracle › Website data says what keeps the shared browser profile at this
// start (rudolfd's webkey: under a key in this computer's TPM, a new one and why, no TPM, the TPM not giving it), on Standard
// and on Native's Oracle; Lite (WebKit) and an older rudolfd as before.
//   node webkey161_t.js [CHROME]   (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8624, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8624/'); await p.waitForFunction(() => typeof oweRow == 'function' && typeof acctNew == 'function' && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});saveUser();USER=ACCTS[0];USER.id=USER.id||'uBoss';USER.disk=true;
    oweHasRD=()=>true;window.ENG='std';OWE_ENG=()=>ENG;window.WK=null;window.ASK=[];
    window.__rd0=rd;rd=async(p,o)=>{ASK.push(p);if(p.startsWith('desktop/owenc?'))return{ok:true,on:window.OWON===undefined?null:OWON,open:true,web:'/persist/oracle/x'};if(p=='webkey'){if(WK=='throw')throw new Error('unknown API');return Object.assign({ok:true},WK)}return __rd0(p,o)};return 1})()`);
  const panel = async () => { await q("(()=>{document.querySelectorAll('.owetest').forEach(x=>x.remove());const c=document.createElement('div');c.className='owetest';document.body.appendChild(c);oweRow(c);return 1})()"); await wait(300);
    return q("(()=>{const c=document.querySelector('.owetest .owec');return c?{on:c.checked,msg:document.querySelector('.owetest .owem').textContent}:null})()") };
  await q("WK={state:'tpm',why:'',t:1};1"); let pv = await panel();
  ok('1. Standard, off, the key in the TPM: said where it is kept, that the drive alone cannot read it, and what it does not cover', pv && !pv.on && /security chip \(TPM\)/.test(pv.msg) && /drive alone cannot read them/.test(pv.msg) && /Anyone who can sign in on this computer can still use them/.test(pv.msg) && !/new key/.test(pv.msg) && !/not encrypted/.test(pv.msg), pv);
  ok('...asked rudolfd (webkey), never for the key itself', await q("ASK.includes('webkey')"), await q('ASK'));
  await q("WK={state:'tpm',why:\"Secure Boot's measurements changed (PCR 7 or 14: Secure Boot turned off or on, its keys or the enrolled MOK keys changed)\",t:1};1"); pv = await panel();
  ok("2. ...a new key at this start: said why, and that websites asked to sign in again", pv && /A new key was made at this start \(Secure Boot's measurements changed\)/.test(pv.msg) && /sign in again/.test(pv.msg), pv);
  await q("WK={state:'tpm',why:'the first time',t:1};1"); pv = await panel();
  ok('3. ...the first key (nothing was lost): no word of a new one', pv && !/new key/.test(pv.msg), pv);
  await q("WK={state:'none',why:'no TPM 2.0',t:1};1"); pv = await panel();
  ok('4. no TPM: the built-in key only, said plainly that the drive could be read', pv && /no TPM security chip/.test(pv.msg) && /could read them/.test(pv.msg), pv);
  await q("WK={state:'failed',why:'the TPM is locked out (too many wrong passwords)',t:1};1"); pv = await panel();
  ok('5. the TPM would not give its key at this start: said, with why', pv && /did not give its key \(the TPM is locked out/.test(pv.msg) && /could read what is kept now/.test(pv.msg), pv);
  await q("WK={state:'tpm',why:'',t:1};ENG='native';OWON=false;1"); pv = await panel();
  ok("6. Native's Oracle (the same shared profile, in Rudolf's Chromium), off: the same", pv && /security chip \(TPM\)/.test(pv.msg) && /shared browser data/.test(pv.msg), pv);
  await q("ENG='lite';1"); pv = await panel();
  ok('7. Lite (WebKit, not this profile), off: as before (not encrypted)', pv && /not encrypted/.test(pv.msg) && !/TPM/.test(pv.msg), pv);
  await q("ENG='std';WK='throw';1"); pv = await panel();
  ok('8. an older rudolfd (no webkey): as before (not encrypted)', pv && /not encrypted/.test(pv.msg), pv);
  ok('no page errors', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
