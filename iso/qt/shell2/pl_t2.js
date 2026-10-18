const {list, conn} = require('./cdp.js');
(async () => {
  const desk = (await list()).find(t => t.url.startsWith('http://127.0.0.1:8080'));
  const d = await conn(desk.webSocketDebuggerUrl);
  const ev = async e => (await d.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true})).result.result.value;
  await ev(`window.__pl={moves:0,buttons:0,locked:null,result:null};document.addEventListener('mousedown',async()=>{if(__pl.result!==null)return;__pl.result='asked';__pl.result=await shellLock({move:(x,y)=>{__pl.moves++},button:()=>__pl.buttons++,change:l=>__pl.locked=l})},{capture:true});1`);
  for (const type of ['mousePressed', 'mouseReleased']) await d.send('Input.dispatchMouseEvent', {type, x: 640, y: 300, button: 'left', clickCount: 1});
  await new Promise(r => setTimeout(r, 2500));
  console.log(await ev('JSON.stringify(__pl)'));
  await ev('shellUnlock();1'); process.exit(0);
})();
