// a real tab (controlled frame) over the desktop, placed and clipped by messages from the desktop
const {list, conn} = require('./cdp.js');
const sleep = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const ts = await list(); const shell = ts.find(t => t.url.startsWith('isolated-app://')), desk = ts.find(t => t.url.startsWith('http://127.0.0.1:8080'));
  const s = await conn(shell.webSocketDebuggerUrl), d = await conn(desk.webSocketDebuggerUrl);
  const evS = async e => (await s.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true})).result.result.value;
  const evD = async e => (await d.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true})).result.result.value;
  // shell side: create tabs on request, place them, clip them, answer
  await evS(`(()=>{window.T={};addEventListener('message',e=>{const m=e.data;if(!m||!m.tab)return;let f=T[m.tab];
    if(m.op=='open'){f=document.createElement('controlledframe');f.className='tab';f.setAttribute('partition','persist:oracle');document.body.appendChild(f);T[m.tab]=f;
      f.addEventListener('loadstop',()=>f.executeScript({code:"document.documentElement.style.background='rgb(255,0,0)';document.body.style.background='rgb(255,0,0)';1"}));f.src=m.url}
    if(m.op=='place'){const[x,y,w,h]=m.rect;Object.assign(f.style,{left:x+'px',top:y+'px',width:w+'px',height:h+'px'});
      f.style.clipPath=m.hole?"path(evenodd,'M0 0H"+w+"V"+h+"H0Z M"+m.hole[0]+" "+m.hole[1]+"h"+m.hole[2]+"v"+m.hole[3]+"h-"+m.hole[2]+"Z')":'none'}
    e.source.postMessage({ack:m.seq},'*')});return 1})()`);
  // desktop side: a channel to the shell (it learnt the shell's window from the hello message)
  await evD(`(()=>{window.SH=null;window.__acks={};addEventListener('message',e=>{if(e.data&&e.data.fromShell)SH=e.source;if(e.data&&e.data.ack)__acks[e.data.ack]=performance.now()});
    window.toShell=(m)=>{(SH||parent).postMessage(m,'*')};return 1})()`);
  await evS(`document.getElementById('desk').contentWindow.postMessage({fromShell:'hello'},'*')`);
  await sleep(300);
  await evD(`toShell({tab:2,op:'open',url:'http://site.test:8082/xfo',seq:1});toShell({tab:2,op:'place',rect:[200,150,500,300],hole:[300,100,150,120],seq:2});1`);
  await sleep(3000);
  const shot = await s.send('Page.captureScreenshot', {format: 'png'});
  require('fs').writeFileSync('/tmp/claude-0/-home-claude/dbf93716-fba6-5fcb-93f8-76b4ca76f210/scratchpad/cf/tab.png', Buffer.from(shot.result.data, 'base64'));
  // latency: 120 moves, each answered by the shell
  const lat = await evD(`(async()=>{const t=[];for(let i=0;i<120;i++){const seq=1000+i,t0=performance.now();toShell({tab:2,op:'place',rect:[200+i,150,500,300],seq});
    while(!__acks[seq])await new Promise(r=>setTimeout(r,0));t.push(__acks[seq]-t0)}t.sort((a,b)=>a-b);return {median:t[60].toFixed(2),p95:t[114].toFixed(2),max:t[119].toFixed(2)}})()`);
  console.log('round trip desktop -> shell -> desktop (ms):', JSON.stringify(lat));
  s.close(); d.close();
})();
