// 1.5.5 (dl27.js, Chromium's policy): what Oracle's Downloads page shows. Found on the Vivobook: downloads from an
// earlier session "Downloading…" for ever; a download whose partial file went away never ended; a saved file listed
// with another, stuck download's address; and a file Chromium held for confirmation reported as an insecure (http)
// download. A stand-in rudolfd.   node dlpage_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), POL = path.join(__dirname, '../../sys/root/etc/chromium/policies/managed/rudolf.json');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
let INC = [];
const api = (a, q) => {
  if (a == 'dl/incoming') return {ok: true, items: INC};
  if (a == 'dl/quarantine' || a == 'dl/home') return {ok: true, items: []};
  if (a == 'dl/space') return {ok: true, free: 50e9, total: 100e9};
  if (a == 'events/wait') return {ok: true, events: ''};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};localStorage.setItem("bd95dl",JSON.stringify([{n:"STEAM_LATEST.DEB",u:"https://repo.steampowered.com/steam/archive/precise/steam_latest.deb",t:1,sz:0,st:"wait",why:"",nat:1},{n:"OLD.ZIP",u:"https://x.test/old.zip",t:1,sz:9,st:"saved",why:""}]));</script>')) }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":""}') }, 20000);
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
    s.writeHead(404); s.end() }) }).listen(8091, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8091/'); await p.waitForFunction(() => typeof dlMatch == 'function' && typeof DLS != 'undefined', null, {timeout: 30000});
  const st = await q("JSON.stringify(DLS.map(r=>[r.n,r.st,r.why]))");
  ok('1. a download an earlier session left "Downloading…" is shown as stopped', /"STEAM_LATEST.DEB","failed","Stopped when the desktop or Rudolf OS restarted/.test(st) && /"OLD.ZIP","saved"/.test(st), st);
  await q("(()=>{['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__sys=1;clearTimeout(DLN.t);DLN.t=setTimeout(()=>{},1e9);return 1})()");
  // a file that arrives is matched only to its own download
  const m = await q(`(()=>{DLN.pend.length=0;const steam={n:'STEAM_LATEST.DEB',u:'https://repo.steampowered.com/x/steam_latest.deb',t:Date.now(),st:'wait',part:'Unconfirmed 1.crdownload'};DLN.pend.push(steam);
    const a=dlMatch({name:'RUDOLF OS RECOVERY KEY.TXT',size:175});a.st='check';const r1=a===steam;
    const disc={n:'DISCORD.DEB',u:'https://stable.dl2.discordapp.net/a/discord-0.0.1.deb',t:Date.now(),st:'wait'};DLN.pend.push(disc);
    const b=dlMatch({name:'discord-0.0.1.deb',size:9});const r2=b===disc;
    const old={n:'X.ZIP',u:'https://x.test/x.zip',t:Date.now()-3600e3+60e3,st:'wait'};DLN.pend.length=0;DLN.pend.push(old);const c=dlMatch({name:'other.bin',size:1});const r3=c===old;
    return JSON.stringify({keyToSteam:r1,discordByName:r2,oldTaken:r3,keyNew:a.u})})()`);
  const mm = JSON.parse(m);
  ok('2. a file nobody waited for (the recovery key) is not given a stuck download\'s address', !mm.keyToSteam && mm.keyNew === '', m);
  ok('3. a renamed file is still matched to the one recent download waiting for a file', mm.discordByName, m);
  ok('4. ...but not to a download from long ago', !mm.oldTaken, m);
  // a held file (Unconfirmed), and a partial file that goes away
  await q(`(()=>{DLN.pend.length=0;const r={n:'DISCORD.DEB',u:'https://stable.dl2.discordapp.net/x/Discord.deb',t:Date.now(),st:'wait'};DLS.push(r);DLN.pend.push(r);window.__R1=r;return 1})()`);
  INC = [{name: 'Unconfirmed 416795.crdownload', size: 2331000, mtime: 1, partial: true}];
  await q("(async()=>{clearTimeout(DLN.t);await dlPoll();clearTimeout(DLN.t);__R1.gt=Date.now()-130e3;await dlPoll();clearTimeout(DLN.t);return 1})()");
  const h = await q("JSON.stringify([__R1.st,__R1.why])");
  ok('5. a file the browser held back is reported as held, not as an insecure (http) download', /failed/.test(h) && /held this file back/.test(h) && !/insecure/.test(h), h);
  await q(`(()=>{DLN.pend.length=0;const r={n:'STEAM_LATEST.DEB',u:'https://repo.steampowered.com/x/steam_latest.deb',t:Date.now(),st:'wait'};DLS.push(r);DLN.pend.push(r);window.__R2=r;return 1})()`);
  INC = [{name: 'steam_latest.deb.crdownload', size: 1000, mtime: 1, partial: true}];
  await q("(async()=>{clearTimeout(DLN.t);await dlPoll();clearTimeout(DLN.t);return 1})()");
  INC = [];
  await q("(async()=>{for(let i=0;i<3;i++){clearTimeout(DLN.t);await dlPoll()}clearTimeout(DLN.t);return 1})()");
  const g = await q("JSON.stringify([__R2.st,__R2.why])");
  ok('6. a download whose partial file went away is stopped, not "Downloading…" for ever', /failed/.test(g) && /stopped before it finished/.test(g), g);
  // Chromium's policy: the file types it would hold back for confirmation are exempted
  const pol = JSON.parse(fs.readFileSync(POL, 'utf8')), ex = pol.ExemptDomainFileTypePairsFromFileTypeDownloadWarnings || [];
  ok('7. Chromium\'s policy exempts .deb, .exe, .appimage and the like from its confirmation, for every site', ['deb', 'exe', 'appimage', 'rpm', 'sh', 'msi'].every(e => ex.some(x => x.file_extension == e && x.domains.includes('*'))) && pol.DownloadRestrictions === 0, ex.length);
  ok('8. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
