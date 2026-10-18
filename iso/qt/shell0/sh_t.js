// Phase 0: the 1.1.2 desktop inside the test shell
const {list, conn, evalIn, browserWs} = require('./cdp.js');
const sleep = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await conn(await browserWs());
  await b.send('Target.createTarget', {url: process.argv[2]}); await sleep(9000);
  const ts = await list(); console.log(ts.map(t => t.type + ' ' + t.url.slice(0, 70)).join('\n'));
  const shell = ts.find(t => t.url.startsWith('isolated-app://')), desk = ts.find(t => t.url.startsWith('http://127.0.0.1:8080'));
  if (!desk) { console.log('desktop guest not found'); process.exit(1) }
  const R = await evalIn(desk, `(async()=>{const o={};
    o.desktop = typeof screenUi=='function' && !!RD;
    o.size = [innerWidth, innerHeight];
    try{localStorage.setItem('cftest','1');o.localStorage=localStorage.getItem('cftest')=='1'}catch(e){o.localStorage='ERR '+e.message}
    o.indexedDB = await new Promise(r=>{try{const q=indexedDB.open('cft',1);q.onupgradeneeded=()=>q.result.createObjectStore('kv');q.onsuccess=()=>{const tx=q.result.transaction('kv','readwrite');tx.objectStore('kv').put(new Uint8Array([1,2,3]),'k');tx.oncomplete=()=>r(true);tx.onerror=()=>r('ERR tx')};q.onerror=()=>r('ERR open')}catch(e){r('ERR '+e.message)}});
    o.sharedArrayBuffer = typeof SharedArrayBuffer;
    o.wasm = typeof WebAssembly;
    o.shellSaid = window.__shellSaid || null; o.replyErr = window.__replyErr || null;
    try{const s=await navigator.mediaDevices.getUserMedia({audio:true,video:true});o.camera=s.getTracks().map(t=>t.kind).join('+');s.getTracks().forEach(t=>t.stop())}catch(e){o.camera='ERR '+e.name}
    try{await document.documentElement.requestFullscreen();o.fullscreen=!!document.fullscreenElement;await document.exitFullscreen()}catch(e){o.fullscreen='ERR '+e.message}
    try{const a=new AudioContext();o.audio=a.state;a.close()}catch(e){o.audio='ERR '+e.message}
    try{await navigator.clipboard.writeText('rudolf');o.clipboard=await navigator.clipboard.readText()}catch(e){o.clipboard='ERR '+e.name}
    try{o.notifications=Notification.permission}catch(e){o.notifications='ERR'}
    try{const w=window.open('https://example.test/x','_blank');o.windowOpen=w===null?'null':'window'}catch(e){o.windowOpen='ERR '+e.message}
    o.crossOriginIsolated = crossOriginIsolated;
    return JSON.stringify(o)})()`, true);
  console.log('DESKTOP:', R);
  // pointer lock needs a click in the page; try with a gesture
  console.log('POINTERLOCK:', await evalIn(desk, `(async()=>{try{await document.body.requestPointerLock();await new Promise(r=>setTimeout(r,300));const ok=document.pointerLockElement===document.body;document.exitPointerLock();return ok}catch(e){return 'ERR '+e.message}})()`, true));
  console.log('SHELL:', await evalIn(shell, 'JSON.stringify(window.L)'));
  b.close();
})();
