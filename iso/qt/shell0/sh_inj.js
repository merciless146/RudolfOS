// scripts injected into a tab (how Ruffle would be loaded) and the request filter (how the firewall would work)
const {list, conn} = require('./cdp.js');
(async () => {
  const ts = await list(); const shell = ts.find(t => t.url.startsWith('isolated-app://'));
  const s = await conn(shell.webSocketDebuggerUrl);
  const ev = async e => (await s.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true})).result.result.value;
  console.log(await ev(`(async()=>{const o={};const f=document.createElement('controlledframe');f.className='tab';f.setAttribute('partition','persist:oracle');
    Object.assign(f.style,{left:'0',top:'0',width:'300px',height:'200px'});document.body.appendChild(f);
    try{await f.addContentScripts([{name:'ruffle',urlPatterns:['http://*/*','https://*/*'],js:{code:"document.documentElement.dataset.ruffle='loaded'"},runAt:'document-start',allFrames:true}]);o.addContentScripts='ok'}catch(e){o.addContentScripts='ERR '+e.message}
    try{const i=f.request.createWebRequestInterceptor({urlPatterns:['*://blocked.test/*'],blocking:true});i.addEventListener('beforerequest',e=>{o.intercepted=e.request.url;e.preventDefault()});o.interceptor='ok'}catch(e){o.interceptor='ERR '+e.message}
    const load=u=>new Promise(r=>{const done=ev=>{f.removeEventListener('loadstop',done);f.removeEventListener('loadabort',ab);r('stop')};const ab=e=>{f.removeEventListener('loadstop',done);f.removeEventListener('loadabort',ab);r('abort '+(e.reason||''))};f.addEventListener('loadstop',done);f.addEventListener('loadabort',ab);f.src=u});
    o.load1=await load('http://site.test:8082/xfo');
    try{const r=await f.executeScript({code:"document.documentElement.dataset.ruffle||'missing'"});o.injected=r[0]}catch(e){o.injected='ERR '+e.message}
    o.load2=await load('http://blocked.test:8082/xfo');
    try{o.canGoBack=await f.canGoBack();o.back=await f.back()}catch(e){o.nav='ERR '+e.message}
    try{await f.setZoom(1.5);o.zoom=await f.getZoom()}catch(e){o.zoom='ERR '+e.message}
    f.remove();return JSON.stringify(o)})()`));
  s.close();
})();
