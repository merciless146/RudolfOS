// 1.6.0 (the owner's Rudolf sounds, 2026-11-19): Settings › Volume Controls has the Rudolf style in every category, the nine new
// categories (each with Classic, Retro, Candy, Minimal and Rudolf), and "Use one style everywhere"; a Rudolf style sounds (rendered
// offline) as the owner tuned it; the new sounds play where they belong. node sounds60_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { let u = decodeURIComponent(q.url.split('?')[0]); if (u == '/') u = '/index.html'; const f = path.join(WWW, path.normalize(u));
  if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() } s.writeHead(200, {'content-type': u.endsWith('.html') ? 'text/html' : 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8191, '127.0.0.1');
const NEW = ['logout', 'update', 'warning', 'battery', 'usbin', 'usbout', 'menu', 'trash', 'shot'];
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu', '--autoplay-policy=no-user-gesture-required']});
  const p = await (await b.newContext()).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8191/'); await p.waitForFunction(() => typeof rsRender == 'function' && typeof SPRE == 'object' && typeof volPanelS == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  const cats = await q('SNDS.map(x=>x[0])');
  ok(`18 sound categories (${cats.length})`, cats.length == 18 && NEW.every(k => cats.includes(k)), cats);
  const sty = await q('Object.fromEntries(SNDS.map(([k])=>[k,SPRE[k].map(x=>x[0])]))');
  ok('the Rudolf style in every category', cats.every(k => sty[k].includes('Rudolf')), sty);
  ok('each new category: Classic, Retro, Candy, Minimal, Rudolf', NEW.every(k => sty[k].join() == 'Classic,Retro,Candy,Minimal,Rudolf'), NEW.map(k => sty[k].join()));
  const sch = await q(`Object.entries(RS_SCHEME).flatMap(([s,m])=>Object.entries(m).filter(([k,n])=>SPRE[k].findIndex(x=>x[0]==n)<0).map(([k,n])=>s+':'+k+':'+n))`);
  ok('"Use one style everywhere": every style it names is there', !sch.length, sch);
  const def = await q('SNDS.filter(([k])=>!S.snd[k]||S.snd[k].p).map(x=>x[0])');
  ok('the defaults as before: the first style everywhere', !def.length, def);
  // the Rudolf start-up chime, rendered offline at full volume
  const a = await q(`(async()=>{const i=SPRE.startup.findIndex(x=>x[0]=='Rudolf');S.snd.startup.p=i;S.snd.master.on=true;S.snd.master.vol=100;S.snd.startup.vol=100;
    const c=new OfflineAudioContext(1,44100*4,44100);AC=c;sfx('startup',true);const buf=await c.startRendering();AC=null;S.snd.startup.p=0;
    const d=buf.getChannelData(0);let pk=0,e=0,last=0;for(let j=0;j<d.length;j++){const v=Math.abs(d[j]);if(v>pk)pk=v;e+=v*v;if(v>.001)last=j}
    return{pk:+pk.toFixed(3),rms:+Math.sqrt(e/d.length).toFixed(4),len:+(last/44100).toFixed(2),want:+(rsRender(rsEvents('startup')).length/44100).toFixed(2)}})()`);
  ok(`the Rudolf start-up chime sounds (rms ${a.rms}, peak ${a.pk}, ${a.len} s of ${a.want} s)`, a.rms > .005 && a.pk < 1 && a.len > a.want * .6 && a.len <= a.want + .05, a);
  // every Rudolf sound renders, under full scale
  const all = await q(`SNDS.map(([k])=>{const f=rsRender(rsEvents(k));let pk=0;for(const v of f)pk=Math.max(pk,Math.abs(v));return[k,+(f.length/44100).toFixed(2),+pk.toFixed(3)]})`);
  ok('every Rudolf sound renders, between 0.2 and 4 s, peak under full scale', all.every(([k, l, pk]) => l >= .2 && l <= 4 && pk > .05 && pk <= .95), all);
  // Settings › Volume Controls
  await q("settings('vol')"); await p.waitForTimeout(500);
  const ui = await q(`({rows:document.querySelectorAll("select[data-k][data-t='p']").length,menu:[...document.querySelectorAll("select[data-k='menu'][data-t='p'] option")].map(o=>o.textContent),
    msg:[...document.querySelectorAll('.msg')].map(x=>x.textContent).find(t=>/sound categories/.test(t))||'',all:!!document.getElementById('rsall')})`);
  ok(`Volume Controls: a style for each of the 18 (${ui.rows}), the count said right`, ui.rows == 18 && /the 18 sound categories/.test(ui.msg), ui);
  ok('Volume Controls: Start menu offers ' + ui.menu.join(', '), ui.menu.join() == 'Classic,Retro,Candy,Minimal,Rudolf', ui.menu);
  const once = await q(`(()=>{window.__sfx=[];const s=document.getElementById('rsall');s.value='Rudolf';s.onchange({target:s});
    return{p:SNDS.every(([k])=>SPRE[k][S.snd[k].p][0]=='Rudolf'),sel:document.querySelector("select[data-k='error'][data-t='p']").value,heard:__sfx.map(x=>x[0])}})()`);
  ok('"Use one style everywhere" › Rudolf: every category Rudolf, the selects changed, the start-up chime heard', once.p && +once.sel == (await q(`SPRE.error.findIndex(x=>x[0]=='Rudolf')`)) && once.heard.includes('startup'), once);
  const retro = await q(`(()=>{const s=document.getElementById('rsall');s.value='Retro';s.onchange({target:s});return Object.fromEntries(SNDS.map(([k])=>[k,SPRE[k][S.snd[k].p][0]]))})()`);
  ok('"Use one style everywhere" › Retro: ' + Object.values(retro).slice(0, 4).join(', ') + '…', retro.startup == 'Retro boot' && retro.menu == 'Retro' && retro.trash == 'Retro', retro);
  await q(`(()=>{const s=document.getElementById('rsall');s.value='Classic';s.onchange({target:s});document.querySelectorAll('.win').forEach(w=>{try{w.querySelector('.x,.close')?.click()}catch(e){}})})()`);
  // where they play
  const hear = async (js, k) => { await q('window.__sfx=[]'); await q(js); await p.waitForTimeout(250); const h = await q('__sfx.map(x=>x[0])'); return {ok: h.includes(k), h} };
  let h = await hear(`notify('Screenshot saved','x',{snd:'shot'})`, 'shot'); ok('a notification with its own sound (the screenshot)', h.ok && !h.h.includes('notify'), h);
  h = await hear(`notify('Plain','x')`, 'notify'); ok('a plain notification: the notification sound', h.ok, h);
  h = await hear(`(()=>{const m=document.getElementById('menu');m.style.display='none';return new Promise(r=>setTimeout(()=>{m.style.display='block';setTimeout(()=>{m.style.display='none';r()},60)},60))})()`, 'menu');
  ok('the start menu opening: the Start menu sound', h.ok, h);
  h = await hear(`(()=>{const d=document.createElement('div');d.className='dlg';document.body.appendChild(d);setTimeout(()=>d.remove(),50)})()`, 'warning'); ok('a question before something that cannot be undone: the warning', h.ok, h);
  h = await hear(`(()=>{RS_BATLOW=false;PWR=Object.assign(window.PWR||{},{ac:false,battery:{capacity:8,status:'Discharging'}});batDraw()})()`, 'battery'); ok('the battery at 8 % on battery: Battery low', h.ok, h);
  h = await hear(`batDraw()`, 'battery'); ok('...once, not again at the next look', !h.ok, h);
  // the owner's Vivobook (2026-11-19): an account's settings saved before 1.6.0 have only the first ten categories; signing in put them
  // in place of the whole list, and Volume Controls was empty (S.snd[k] undefined)
  const e0 = errs.length;
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;const a={n:'old',id:'oldacct1',h:await pwMake('o1'),r:'admin'};ACCTS.push(a);saveUser();USER=a;const old=cln(S);old.snd={master:{on:true,vol:70}};
    ['startup','login','error','click','open','close','notify','game','shutdown'].forEach(k=>old.snd[k]={on:true,vol:60,p:0});
    localStorage.setItem('bd95set:'+uid(),JSON.stringify(old));USER=null;await userLogin(a,'o1');settings('vol');return 1})()`);
  await new Promise(r => setTimeout(r, 1500));
  const vp = await q("({rows:document.querySelectorAll('.vr2').length,n:SNDS.length,miss:SNDS.filter(([k])=>!S.snd[k]).map(x=>x[0])})");
  ok('an account saved before 1.6.0 (ten categories): every category filled in, and Volume Controls shows all of them', !vp.miss.length && vp.rows >= vp.n && errs.length == e0, {vp, errs: errs.slice(e0)});
  ok('...its own choices kept (startup at 60)', await q('S.snd.startup.vol') == 60, await q("({s:S.snd.startup,u:uid(),keys:Object.keys(localStorage).filter(k=>k.startsWith('bd95set'))})"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
