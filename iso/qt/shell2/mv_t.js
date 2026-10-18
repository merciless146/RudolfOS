// one step of the move test: node mv_t.js STEP   (the desktop is in a tab, or inside the shell: the webview)
const {list, conn, browserWs} = require('./cdp.js');
const sleep = ms => new Promise(r => setTimeout(r, ms));
async function desk() { for (let i = 0; i < 40; i++) { const t = (await list()).find(t => t.url.startsWith('http://127.0.0.1:8080')); if (t) return t; await sleep(500) } throw new Error('no desktop') }
async function ev(t, e) { const c = await conn(t.webSocketDebuggerUrl); const r = await c.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true, userGesture: true}); c.close(); return r.result && r.result.result ? r.result.result.value : JSON.stringify(r).slice(0, 300) }
const say = (n, ok, d) => console.log((ok ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? '  ' + String(d).slice(0, 300) : ''));
(async () => {
  const step = process.argv[2]; const t = await desk(); await sleep(6000);
  if (step == 'seed') {
    console.log(await ev(t, `(async()=>{await IDB.ready;await kvSet('mt-file',new Uint8Array([1,2,3,250]));await kvSet('mt-obj',{name:'note',when:new Date(5000),blob:new Blob(['abc'],{type:'text/plain'})});
      await rvPut('sav','mt-sav',new Uint8Array(1000).fill(7).buffer);localStorage.setItem('mt-ls','hello');await new Promise(r=>setTimeout(r,2500));return 'seeded'})()`));
  } else if (step == 'export') {
    await sleep(8000);
    console.log('classic page state:', await ev(t, `JSON.stringify({inShell:!!SHELL,mirrorOn:IDBM.on,gen:IDBM.gen,overlay:!!document.querySelector('.rmodal')&&document.querySelector('.rmodal').innerText.slice(0,40)})`));
  } else if (step == 'shell') {
    say('the desktop runs inside the shell', await ev(t, '!!SHELL') === true);
    const r = JSON.parse(await ev(t, `(async()=>{await IDB.ready;const f=await kvGet('mt-file'),o=await kvGet('mt-obj'),rom=await rvGet('sav','mt-sav');
      return JSON.stringify({file:f&&[...f],objName:o&&o.name,objDate:o&&+o.when,blobText:o&&o.blob?await o.blob.text():null,rom:rom&&rom.byteLength,romByte:rom&&new Uint8Array(rom)[999],ls:localStorage.getItem('mt-ls')})})()`));
    say('files on drive C: moved (bytes)', JSON.stringify(r.file) == '[1,2,3,250]', JSON.stringify(r));
    say('...including objects, dates and blobs', r.objName == 'note' && r.objDate == 5000 && r.blobText == 'abc');
    say('game saves (and ROMs, same path) moved', r.rom == 1000 && r.romByte == 7);
    say('settings (localStorage) moved', r.ls == 'hello');
    say('paste buttons read the clipboard through rudolfd', await ev(t, `navigator.clipboard.readText().catch(e=>'ERR '+e.message)`) == 'text from the X clipboard');
    say('copying works inside the shell', await ev(t, `navigator.clipboard.writeText('copied').then(()=>'ok',e=>'ERR '+e.message)`) == 'ok');
    console.log(await ev(t, `(async()=>{await kvSet('mt-new',{made:'in the shell'});await kvDel('mt-obj');await new Promise(r=>setTimeout(r,2500));return 'written in the shell'})()`));
  } else if (step == 'fallback') {
    await sleep(3000);
    const r = await ev(t, `(async()=>{await IDB.ready;const n=await kvGet('mt-new'),o=await kvGet('mt-obj');return JSON.stringify({inShell:!!SHELL,newer:n&&n.made,deleted:o===undefined})})()`);
    const j = JSON.parse(r); say('the plain page (fallback) gets what was saved in the shell', j.newer == 'in the shell' && !j.inShell, r); say('...and what was deleted there', j.deleted);
  }
  process.exit(0);
})().catch(e => { console.log('ERROR', e.message); process.exit(1) });
