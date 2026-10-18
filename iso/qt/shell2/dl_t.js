// Downloads of any size (1.2.1), end to end in the shell (host.sh start fresh first; MEDIA=folder with clip.webm
// and big.pdf for site.py): node dl_t.js
// Small files go into C:\DOWNLOADS, larger ones stay on the disk with a reference; the size limit, the file-type
// rules, program approval (held in quarantine), the antivirus at the start and the end of a large file, opening a
// video and a PDF from the disk, rename / move / delete, the frames fallback, and how /dlf/ serves files.
const {list, conn} = require('./cdp.js');
const fs = require('fs');
const wait = ms => new Promise(r => setTimeout(r, ms));
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const DL = '/home/rudolf/Downloads';
const exists = p => { try { fs.statSync(p); return true } catch (e) { return false } };
(async () => {
  const desk = (await list()).find(t => t.url.startsWith('http://127.0.0.1:8080'));
  const d = await conn(desk.webSocketDebuggerUrl);
  const ev = async (e, g) => { const r = await d.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true, userGesture: !!g}); return r.result.exceptionDetails ? 'EXC ' + JSON.stringify(r.result.exceptionDetails).slice(0, 300) : r.result.result.value };
  const until = async (e, ms = 15000) => { const end = Date.now() + ms; while (Date.now() < end) { if (await ev(e) === true) return true; await wait(300) } return false };
  const inTab = async (match, e, g) => {
    const t = (await list()).find(x => x.type == 'webview' && match(x.url)); if (!t) return 'NO TAB';
    const c = await conn(t.webSocketDebuggerUrl);
    const r = await c.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true, userGesture: !!g}); c.close();
    return r.result.result ? r.result.result.value : r;
  };
  const tabUntil = async (match, e, ms = 15000) => { const end = Date.now() + ms; while (Date.now() < end) { try { if (await inTab(match, e) === true) return true } catch (x) {} await wait(300) } return false };
  const last = () => ev(`JSON.stringify(DLS.slice(-1)[0])`);
  const fetchIn = path => inTab(u => u.includes('site.test:8099/files'), `(()=>{const a=document.createElement('a');a.href=${JSON.stringify(path)};document.body.appendChild(a);a.click();return 1})()`, true);
  const settled = (n, ms = 30000) => until(`(()=>{const r=DLS.slice().reverse().find(x=>x.n==${JSON.stringify(n)});return !!r&&!['wait','check','approve'].includes(r.st)})()`, ms);
  const rec = n => ev(`JSON.stringify(DLS.slice().reverse().find(x=>x.n==${JSON.stringify(n)})||null)`).then(JSON.parse);
  const ref = n => ev(`JSON.stringify(dlRefInfo(fs['C:'].DOWNLOADS[${JSON.stringify(n)}]||'')||null)`).then(JSON.parse);

  ok('the desktop is in the shell with real tabs and the 1.2.1 downloads', await until('typeof rtOn=="function"&&rtOn()&&!!window.__dl27&&dlOn()', 20000));
  await until(`!!document.getElementById('oobe')||!!document.getElementById('login')&&getComputedStyle(document.getElementById('login')).display!='none'`, 8000);
  await ev(`try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});typeof closeAll=='function'&&closeAll();SECC.av.on=true;SECC.av.act='quarantine';NETC.dl.exe='ask';NETC.dl.max=4096;window.__elevAns=true;elevate=async r=>{window.__elevAsked=r;return window.__elevAns};1`);

  // a signed-in tab
  await ev(`browser('http://site.test:8099/login');window.__W=wins[wins.length-1];1`);
  await tabUntil(u => u.includes('/login'), `document.readyState=="complete"&&!!document.getElementById('u')`);
  await inTab(u => u.includes('/login'), `document.getElementById('u').value='alice';document.getElementById('p').value='pw';document.getElementById('go').click();1`, true);
  await tabUntil(u => u.includes('/me'), `document.readyState=="complete"`);
  await ev(`__W.body.querySelector('.ad').value='http://site.test:8099/files';__W.body.querySelector('.ad').dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}));1`);
  await tabUntil(u => u.includes('/files'), 'document.readyState=="complete"');

  // --- small: into C:\DOWNLOADS as before, with the cookie
  await inTab(u => u.includes('/files'), `document.getElementById('f').click();1`, true);
  ok('a small download from a signed-in tab is in C:\\DOWNLOADS', await until(`fs['C:'].DOWNLOADS&&fs['C:'].DOWNLOADS['NOTES.TXT']=='notes for alice'`, 15000), await last());
  ok('... and nothing of it is left on the disk', !exists(DL + '/notes.txt') && fs.readdirSync(DL + '/.incoming').length == 0, fs.readdirSync(DL + '/.incoming'));

  // --- 2 MB: kept on the disk
  await inTab(u => u.includes('/files'), `document.getElementById('b').click();1`, true);
  await settled('BIG.ZIP');
  let r = await ref('BIG.ZIP');
  ok('a 2 MB download is kept on the disk with a reference in C:\\DOWNLOADS', r && r.p == DL + '/big.zip' && r.s == 2097152 && exists(DL + '/big.zip'), [r, await rec('BIG.ZIP')]);
  ok('File Explorer sees its real size and that it is on the disk', await ev(`sizeOf(fs['C:'].DOWNLOADS['BIG.ZIP'])==2097152&&fType('BIG.ZIP',fs['C:'].DOWNLOADS['BIG.ZIP']).includes('on the disk')&&fIcon('BIG.ZIP',fs['C:'].DOWNLOADS['BIG.ZIP'])=='💽'`));

  // --- 50 MB and 1 GB
  await fetchIn('/gen/movie.mp4?mb=50');
  ok('a 50 MB download is kept on the disk', await settled('MOVIE.MP4', 60000) && (r = await ref('MOVIE.MP4')) && r.s == 50 * 1048576 && fs.statSync(DL + '/movie.mp4').size == 50 * 1048576, [r, await rec('MOVIE.MP4')]);
  ok('... the antivirus read its first and last 4 MB, and the list says so', r && r.av == 'part' && (await ev(`secPage('about:downloads',(t,b)=>b,(u,t)=>t,s=>s)`)).includes('first and last 4 MB'));
  const t0 = Date.now(); await fetchIn('/gen/huge.mkv?mb=1024');
  ok('a 1 GB download shows its progress while it arrives', await until(`(()=>{const r=DLS.slice(-1)[0];return r.st=='wait'&&r.sz>0})()`, 30000), await last());
  ok('a 1 GB download is kept on the disk', await settled('HUGE.MKV', 240000) && (r = await ref('HUGE.MKV')) && r.s == 1073741824, [r, await rec('HUGE.MKV'), Math.round((Date.now() - t0) / 1000) + ' s']);

  // --- the antivirus at the start and the end of a large file
  await fetchIn('/gen/bad1.webm?mb=20&kind=eicar-head'); await settled('BAD1.WEBM');
  r = await rec('BAD1.WEBM');
  ok('EICAR at the start of a 20 MB file: quarantined', r.st == 'quarantined' && /EICAR/.test(r.why) && exists(DL + '/.quarantine/bad1.webm') && !exists(DL + '/bad1.webm'), r);
  ok('... listed in the quarantine, as a file on the disk', await ev(`QUAR.some(q=>q.n=='BAD1.WEBM'&&isDlRef(q.data))`));
  await fetchIn('/gen/bad2.webm?mb=20&kind=eicar-tail'); await settled('BAD2.WEBM');
  r = await rec('BAD2.WEBM');
  ok('EICAR at the end of a 20 MB file: quarantined', r.st == 'quarantined' && /EICAR/.test(r.why), r);
  await ev(`SECC.av.act='block';1`);
  await fetchIn('/gen/fake.mp4?mb=3&kind=mz'); await settled('FAKE.MP4');
  r = await rec('FAKE.MP4');
  ok('a program disguised as a video (blocking mode): blocked and deleted', r.st == 'blocked' && /Executable/.test(r.why) && !exists(DL + '/fake.mp4') && !exists(DL + '/.quarantine/fake.mp4'), r);
  await ev(`SECC.av.act='quarantine';1`);

  // --- program files: held in quarantine until an administrator approves
  await ev(`window.__elevAns=new Promise(res=>window.__elevRes=res);1`);
  await fetchIn('/gen/setup.exe?mb=3');
  ok('a program file waits in the quarantine folder for approval', await until(`(DLS.slice(-1)[0]||{}).st=='approve'`, 20000) && exists(DL + '/.quarantine/setup.exe') && !exists(DL + '/setup.exe'), await last());
  ok('... the request names the file', await ev(`String(window.__elevAsked||'').includes('SETUP.EXE')`), await ev('window.__elevAsked'));
  await ev(`__elevRes(true);1`); await settled('SETUP.EXE');
  r = await rec('SETUP.EXE');
  ok('approved: kept on the disk, marked as a program file', r.st == 'saved' && r.exe && exists(DL + '/setup.exe') && !!(await ref('SETUP.EXE')), r);
  await ev(`window.__elevAns=false;1`);
  await fetchIn('/gen/tool.exe?mb=2'); await settled('TOOL.EXE');
  r = await rec('TOOL.EXE');
  ok('not approved: deleted', r.st == 'blocked' && !exists(DL + '/.quarantine/tool.exe') && !exists(DL + '/tool.exe'), r);
  await ev(`window.__elevAns=true;1`);

  // --- the size limit
  await ev(`NETC.dl.max=100;1`);
  await fetchIn('/gen/over.mov?mb=300'); await settled('OVER.MOV', 60000); await until(`!DLN.kill.size`, 20000); await wait(1500);
  r = await rec('OVER.MOV');
  ok('over the limit set in Settings: stopped while it arrives, nothing left behind', r && r.st == 'blocked' && /limit/.test(r.why) && !exists(DL + '/over.mov') && !fs.readdirSync(DL + '/.incoming').some(f => /over|crdownload/.test(f)) && (await ev(`DLS.filter(x=>x.n=='OVER.MOV').length`)) == 1, [r, fs.readdirSync(DL + '/.incoming')]);
  await ev(`NETC.dl.max=4096;1`);
  ok('the limit is a setting in Network ▸ Downloads', await ev(`netLoadCfg().dl.max===4096&&/Largest download/.test(document.documentElement.innerHTML+String(netPanelS))`));

  // --- open from the disk: a video and a PDF
  await fetchIn('/media/clip.webm'); await settled('CLIP.WEBM');
  ok('a 5 MB video is kept on the disk', !!(await ref('CLIP.WEBM')), await rec('CLIP.WEBM'));
  await ev(`openPath(['C:','DOWNLOADS','CLIP.WEBM']);1`);
  ok('it plays from the disk (read in pieces)', await until(`(()=>{const v=document.querySelector('.win video');return !!v&&v.readyState>=2&&v.currentTime>0.3&&v.src.includes('/dlf/')})()`, 20000), await ev(`(()=>{const v=document.querySelector('.win video');return v?[v.readyState,v.currentTime,v.src,v.error&&v.error.code]:'no video'})()`));
  ok('/dlf/ answers in pieces (Range)', await ev(`(async()=>{const v=document.querySelector('.win video');const r=await fetch(v.src,{headers:{Range:'bytes=0-99'}});return r.status==206&&(await r.arrayBuffer()).byteLength==100})()`));
  await ev(`typeof closeAll=='function'&&closeAll();1`);
  await fetchIn('/media/big.pdf'); await settled('BIG.PDF');
  await ev(`openPath(['C:','DOWNLOADS','BIG.PDF']);1`);
  ok('a 2 MB PDF opens from the disk in PDF Viewer', await until(`(()=>{const w=wins.find(w=>w.app=='pdf'||/PDF Viewer/.test(w.title||''));const c=document.querySelector('.pds canvas');return !!c&&c.width>100})()`, 20000), await ev(`(document.querySelector('.pds')||{}).parentNode&&document.querySelector('.pds').parentNode.querySelector('.stat').textContent`));
  await ev(`typeof closeAll=='function'&&closeAll();1`);

  // --- a downloaded page is never run on the desktop's origin
  await fetchIn('/gen/page.html?mb=2'); await settled('PAGE.HTML');
  ok('a downloaded page is served as plain text with a sandbox', await ev(`(async()=>{const u=await dlLink(fs['C:'].DOWNLOADS['PAGE.HTML']);const r=await fetch(u,{headers:{Range:'bytes=0-9'}});return /text\\/plain/.test(r.headers.get('content-type'))&&/sandbox/.test(r.headers.get('content-security-policy'))})()`));
  ok('/dlf/ refuses files outside Downloads', await ev(`rd('dl/link?path='+encodeURIComponent('/etc/passwd')).then(()=>false,()=>true)`) && await ev(`rd('dl/link?path='+encodeURIComponent('${DL}/.quarantine/bad1.webm')).then(()=>false,()=>true)`));

  // --- rename, move, copy, delete
  await ev(`const D=fs['C:'].DOWNLOADS;D['FILM.MP4']=D['MOVIE.MP4'];delete D['MOVIE.MP4'];fsSave();dlPoll();1`);
  ok('renamed in File Explorer: renamed on the disk', await until(`(dlRefInfo(fs['C:'].DOWNLOADS['FILM.MP4']||'')||{}).p=='${DL}/FILM.MP4'`, 10000) && exists(DL + '/FILM.MP4') && !exists(DL + '/movie.mp4'));
  await ev(`const C=fs['C:'];if(!C.DOCUMENTS)C.DOCUMENTS={};C.DOCUMENTS['BIG.ZIP']=C.DOWNLOADS['BIG.ZIP'];delete C.DOWNLOADS['BIG.ZIP'];C.DOWNLOADS['FILM (2).MP4']=C.DOWNLOADS['FILM.MP4'];fsSave();dlPoll();1`);
  ok('moved out of C:\\DOWNLOADS: put back; a copy of it: dropped', await until(`!('BIG.ZIP' in fs['C:'].DOCUMENTS)&&isDlRef(fs['C:'].DOWNLOADS['BIG.ZIP'])&&!('FILM (2).MP4' in fs['C:'].DOWNLOADS)`, 10000) && exists(DL + '/big.zip'));
  await ev(`trashPut([['C:','DOWNLOADS','HUGE.MKV']]);dlPoll();1`); await wait(3000);
  ok('deleted to the Trash: still on the disk', exists(DL + '/huge.mkv'));
  await ev(`TR.length=0;trSave();dlPoll();setTimeout(dlPoll,1500);1`);
  const end = Date.now() + 15000; while (Date.now() < end && exists(DL + '/huge.mkv')) await wait(500);
  ok('Trash emptied: deleted from the disk', !exists(DL + '/huge.mkv'));
  await ev(`(()=>{const i=QUAR.findIndex(q=>q.n=='BAD2.WEBM');QUAR.splice(i,1);saveQ();dlPoll();setTimeout(dlPoll,1500)})();1`);
  const e2 = Date.now() + 15000; while (Date.now() < e2 && exists(DL + '/.quarantine/bad2.webm')) await wait(500);
  ok('deleted from the quarantine: deleted from the disk', !exists(DL + '/.quarantine/bad2.webm'));

  // --- the frames fallback (the desktop itself downloads): handed to Chromium, same checks
  await ev(`dlStart('http://site.test:8099/gen/fb.zip?mb=3');1`);
  ok('a large download started by the desktop itself is handed to Chromium and kept on the disk', await settled('FB.ZIP', 30000) && !!(await ref('FB.ZIP')) && exists(DL + '/fb.zip'), await rec('FB.ZIP'));

  // --- Storage settings and backups
  ok('Storage settings list the files kept on the disk', await ev(`(()=>{const c=document.createElement('div');storPanel(c);return /Downloads kept on the disk/.test(c.innerHTML)&&/FILM.MP4/.test(c.innerHTML)})()`));
  ok('dl/space answers', await ev(`rd('dl/space').then(s=>s.total>0&&s.free>=0)`));
  ok('nothing is left behind in the incoming folder', fs.readdirSync(DL + '/.incoming').length == 0, fs.readdirSync(DL + '/.incoming'));
  ok('no errors on the desktop', await ev(`!(window.__errs||[]).length`), await ev(`JSON.stringify((window.__errs||[]).slice(-3))`));

  console.log(`\n${R.filter(Boolean).length}/${R.length} passed`);
  process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(2) });
