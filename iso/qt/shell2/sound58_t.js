// 1.5.8: orchestral sound cues. Settings › Sound lists the new versions under the old names, in the old places, and the
// old ones last as "… (legacy)"; each new one is longer than its old one, sounds (rendered offline), stays under full
// scale, and has a bass, a tenor and an alto voice under the melody. node sound58_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { let u = decodeURIComponent(q.url.split('?')[0]); if (u == '/') u = '/index.html'; const f = path.join(WWW, path.normalize(u));
  if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() } s.writeHead(200, {'content-type': u.endsWith('.html') ? 'text/html' : 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8190, '127.0.0.1');
const CUES = [['startup', 'Classic chime'], ['startup', 'Soft bells'], ['login', 'Classic'], ['login', 'Bright ding'], ['notify', 'Classic ding'], ['notify', 'Gentle bell'], ['shutdown', 'Classic chime'], ['shutdown', 'Slow fade']];
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu', '--autoplay-policy=no-user-gesture-required']});
  const p = await (await b.newContext()).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8190/'); await p.waitForFunction(() => typeof orchVoice == 'function' && typeof SPRE == 'object', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  for (const [k, n] of CUES) {
    const r = await q(`(()=>{const L=SPRE.${k},i=L.findIndex(x=>x[0]==${JSON.stringify(n)}),j=L.findIndex(x=>x[0]==${JSON.stringify(n + ' (legacy)')});if(i<0||j<0)return{i,j};
      const len=pat=>Math.max(...pat.map(x=>x[1]+x[2])),nw=L[i][1],old=L[j][1];
      const low=nw.filter(x=>x[5]&&x[5].w=='str').map(x=>x[0]).sort((a,b)=>a-b);
      return{i,j,last:j==L.length-1||L.slice(j).every(x=>/legacy/.test(x[0])),nl:len(nw),ol:len(old),voices:new Set(low.map(f=>Math.round(f))).size,bass:low[0]}})()`);
    ok(`${k} › ${n}: new version in its old place, "${n} (legacy)" at the end`, r.i >= 0 && r.j > r.i && r.last, r);
    ok(`...longer (${r.nl && r.nl.toFixed(2)} s, was ${r.ol && r.ol.toFixed(2)} s), not much longer (at most 2.5 s)`, r.nl > r.ol && r.nl <= 2.5, r);
    ok(`...bass, tenor and alto under the melody (string voices: ${r.voices}, the lowest ${r.bass && r.bass.toFixed(0)} Hz)`, r.voices >= 3 && r.bass < 200, r);
    // rendered offline: it sounds, and stays under full scale
    const a = await q(`(async()=>{const L=SPRE.${k},i=L.findIndex(x=>x[0]==${JSON.stringify(n)});S.snd.${k}.p=i;S.snd.${k}.on=true;S.snd.master.on=true;S.snd.master.vol=100;S.snd.${k}.vol=100;
      const c=new OfflineAudioContext(2,44100*3,44100);AC=c;ORCH_HALL=null;sfx('${k}',true);const buf=await c.startRendering();AC=null;
      let pk=0,e=0;for(let ch=0;ch<2;ch++){const d=buf.getChannelData(ch);for(let j=0;j<d.length;j++){const v=Math.abs(d[j]);if(v>pk)pk=v;e+=v*v}}
      const tail=buf.getChannelData(0).slice(44100*1.0,44100*1.1).reduce((s,v)=>s+Math.abs(v),0);return{pk:+pk.toFixed(3),rms:+Math.sqrt(e/(buf.length*2)).toFixed(4),tail:+tail.toFixed(2)}})()`);
    ok(`...it sounds (rms ${a.rms}), peak ${a.pk} under full scale`, a.rms > .005 && a.pk < 1, a);
  }
  const legacyPlays = await q(`(async()=>{const L=SPRE.notify,j=L.findIndex(x=>x[0]=='Classic ding (legacy)');S.snd.notify.p=j;const c=new OfflineAudioContext(1,44100,44100);AC=c;sfx('notify',true);const b=await c.startRendering();AC=null;return Math.max(...b.getChannelData(0).map(Math.abs))})()`);
  ok('a legacy one plays as before (Classic ding (legacy))', legacyPlays > .05 && legacyPlays < 1, legacyPlays);
  await q("settings('vol')"); await p.waitForTimeout(400);
  const opts = await q("[...document.querySelectorAll(\"select[data-k='startup'] option\")].map(o=>o.textContent)");
  ok('Settings › Sound lists them: ' + opts.join(', '), opts[0] == 'Classic chime' && opts.includes('Classic chime (legacy)') && opts.includes('Soft bells (legacy)'), opts);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
