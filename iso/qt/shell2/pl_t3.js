// pointer capture through the shell with real input (X test events): a click in the desktop asks the shell to
// capture the pointer; relative moves, buttons and the wheel must arrive in the desktop. Needs xin (XTest).
//   node pl_t3.js XIN
const {list, conn} = require('./cdp.js');
const {execFileSync} = require('child_process');
const X = (...a) => execFileSync(process.argv[2], a.map(String));
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const desk = (await list()).find(t => t.url.startsWith('http://127.0.0.1:8080'));
  const d = await conn(desk.webSocketDebuggerUrl);
  const ev = async e => (await d.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true})).result.result.value;
  console.log('shell seen by the desktop:', await ev('!!SHELL'));
  await ev(`['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__pl={moves:0,dx:0,dy:0,buttons:[],wheel:0,locked:[],result:null};document.addEventListener('mousedown',async()=>{if(__pl.result!==null)return;__pl.result='asked';__pl.result=await shellLock({move:(x,y)=>{__pl.moves++;__pl.dx+=x;__pl.dy+=y},button:(b,dn)=>__pl.buttons.push(b+(dn?'v':'^')),wheel:y=>__pl.wheel+=y,change:l=>__pl.locked.push(l)},true)},{capture:true});1`);
  const OFF = await ev('[screenX,screenY]'); await ev('wins.slice().forEach(w=>w.close());1'); X('move', 640 + OFF[0], 300 + OFF[1]); await wait(300); X('click', 1); await wait(1200);
  console.log('after the click:', await ev('JSON.stringify(__pl)'));
  for (let i = 0; i < 10; i++) { X('rel', 7, -3); await wait(30) }
  console.log('moves only:', await ev('JSON.stringify(__pl)'));
  X('click', 4); await wait(400); console.log('wheel:', await ev('JSON.stringify(__pl)'));
  X('click', 3); await wait(800);
  const r = JSON.parse(await ev('JSON.stringify(__pl)'));
  console.log('after moving:', JSON.stringify(r));
  await ev('shellUnlock();1'); await wait(500);
  const r2 = JSON.parse(await ev('JSON.stringify(__pl)'));
  const ok = r.result === true && r.locked[0] === true && r.moves >= 5 && r.dx > 0 && r.dy < 0 && r.buttons.includes('2v') && r2.locked.includes(false);
  console.log((ok ? 'PASS' : 'FAIL') + ' pointer capture through the shell', JSON.stringify(r2));
  process.exit(ok ? 0 : 1);
})();
