// Task Manager (1.2) and system reports, inside the shell (host.sh start fresh first): node tm_t.js
// rudolfd's system and process information comes from a test copy of the real rudolfd (host.sh).
const {list, conn} = require('./cdp.js');
const {spawn, execSync} = require('child_process');
const wait = ms => new Promise(r => setTimeout(r, ms));
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
(async () => {
  const desk = (await list()).find(t => t.url.startsWith('http://127.0.0.1:8080'));
  const d = await conn(desk.webSocketDebuggerUrl);
  const ev = async e => { const r = await d.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true}); return r.result.exceptionDetails ? 'EXC ' + JSON.stringify(r.result.exceptionDetails).slice(0, 300) : r.result.result.value };
  const until = async (e, ms = 15000) => { const end = Date.now() + ms; while (Date.now() < end) { if (await ev(e) === true) return true; await wait(300) } return false };
  await until('typeof rtOn=="function"&&rtOn()', 20000);
  await until(`!!document.getElementById('oobe')`, 8000);
  await ev(`try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());1`);
  await ev(`browser('http://site.test:8099/');1`); await wait(2500);
  await ev(`launch('taskmgr');window.__T=wins[wins.length-1];1`);
  const tm = s => `__T.body.querySelector(${JSON.stringify(s)})`;
  const tabTo = t => ev(`${tm(`.tmtabs [data-t='${t}']`)}.click();1`);
  ok('Task Manager opens (the 1.2 one)', await ev(`__T.tb.title=='Task Manager'&&!!${tm(`.tmtabs [data-t='hw']`)}`));
  // --- Apps: the real tab under its Oracle window, with the memory its page uses
  ok('Apps lists the real tab with its page memory', await until(`[...__T.body.querySelectorAll('tr')].some(r=>r.textContent.includes('Real tab')&&/[KMG]B/.test(r.children[4].textContent))`, 15000), await ev(`[...__T.body.querySelectorAll('tr')].map(r=>r.textContent).join(' | ').slice(0,400)`));
  ok('...and the desktop page\'s memory', await ev(`__T.body.textContent.includes('The desktop page uses')`));
  // --- Processes
  const sl = spawn('sleep', ['600'], {detached: true}); const spid = sl.pid;
  await tabTo('procs');
  ok('Processes are grouped by what they are', await until(`['Desktop','Web pages','Kernel'].every(k=>__T.body.textContent.includes(k))`, 8000), await ev(`[...__T.body.querySelectorAll('tbody tr:first-child b')].map(b=>b.textContent).join(', ')`));
  ok('...with CPU and memory', await until(`[...__T.body.querySelectorAll('tr[data-pid]')].length>20&&[...__T.body.querySelectorAll('tr[data-pid]')].some(r=>/%$/.test(r.children[3].textContent))`, 8000));
  ok('the processes Rudolf OS needs are marked and cannot be ended', await ev(`(()=>{const r=[...__T.body.querySelectorAll('tr[data-pid]')].find(r=>r.children[0].textContent.startsWith('🔒'));if(!r)return false;r.click();return ${tm('.tme')}.disabled})()`));
  await ev(`window.__elev=elevate;elevate=async r=>{window.__asked=r;return true};1`);   // the administrator approves
  await ev(`(()=>{const f=${tm('.tmf')};f.value='${spid}';f.dispatchEvent(new Event('input'));return 1})()`);
  ok('finding a process', await until(`[...__T.body.querySelectorAll('tr[data-pid]')].length==1`, 5000), await ev(`__T.body.querySelectorAll('tr[data-pid]').length`));
  await ev(`__T.body.querySelector('tr[data-pid]').click();${tm('.tme')}.click();1`);
  await wait(1500);
  let alive = true; try { process.kill(spid, 0) } catch (e) { alive = false }
  ok('End process asks an administrator and ends it', !alive && /End sleep/.test(await ev('window.__asked||""')), await ev('window.__asked||""'));
  const rd = execSync('pgrep -x rudolfd_t').toString().trim().split('\n')[0];
  ok('rudolfd refuses to end a process Rudolf OS needs', /cannot be ended/.test(await ev(`rdPost('proc/end?pid=${rd}').then(()=>'ended',e=>e.message)`)));
  await ev(`elevate=__elev;1`);
  // --- Performance
  await tabTo('perf'); await wait(2500);
  ok('Performance shows CPU, memory and network graphs', await ev(`__T.body.querySelectorAll('.tmb canvas').length==3&&__T.body.textContent.includes('Network')`));
  // --- System and Hardware
  await tabTo('system');
  ok('System shows Rudolf OS, the desktop\'s host, services and the kernel', await until(`['Started from','the shell (Oracle has real browser tabs)','Services','Kernel','Linux','Kernel warnings'].every(k=>__T.body.textContent.includes(k))`, 10000), await ev(`__T.body.textContent.slice(0,300)`));
  ok('System shows the installed release (1.2.3)', await until(`__T.body.textContent.includes('1.2.2.261019 · serial 2026101902 · stable channel · slot a (trial start)')`, 5000), await ev(`(__T.body.textContent.match(/Installed release.{0,90}/)||[''])[0]`));
  ok('System says what draws the screen and web pages (1.2.4)', await until(`['Drawn by','Pages drawn with'].every(k=>__T.body.textContent.includes(k))`, 5000), await ev(`(__T.body.textContent.match(/Drawn by.{0,80}/)||[''])[0]+' | '+(__T.body.textContent.match(/Pages drawn with.{0,60}/)||[''])[0]`));
  ok('PDF text keeps dashes, quotes and table columns (1.2.3)', await ev(`pdfEsc('Version\\t1.2.2 \\u2014 \\u201cok\\u201d \\u2192 x \\u00b7 1920\\u00d71080')==='Version   1.2.2 \\x97 \\x93ok\\x94 -> x \\xb7 1920\\xd71080'`), await ev(`JSON.stringify(pdfEsc('Version\\t1.2.2 \\u2014 \\u201cok\\u201d'))`));
  await tabTo('hw');
  ok('Hardware shows the processor, storage and network adapters', await until(`['Processor','Xeon','Storage','Network adapters'].every(k=>__T.body.textContent.includes(k))`, 10000), await ev(`__T.body.textContent.slice(0,300)`));
  // --- the system report
  await ev(`SHELL_LOG.push('test: talked to 10.1.2.3 at aa:bb:cc:dd:ee:ff for alice@example.com');1`);
  const rep = await ev(`sysReport('Sound stops after sleep').then(r=>JSON.stringify(r))`);
  const r = JSON.parse(rep);
  console.log(r.text.split('\n').slice(0, 24).join('\n'));
  ok('the report has the computer\'s status at that moment', ['Problem: Sound stops after sleep', '== Status', 'Rudolf OS: ', 'release 1.2.2.261019 · serial 2026101902', 'Drawn by: ', 'Pages drawn with: ', 'Started from: ', 'Desktop: shell', 'Display: ', 'Memory: ', 'CPU: ', 'Services: ', '== Computer', '== Kernel', '== Processes'].every(k => r.text.includes(k)), r.summary);
  ok('...without network or e-mail addresses', !/10\.1\.2\.3|aa:bb:cc|alice@example/.test(r.text) && r.text.includes('x.x.x.x'));
  const url = await ev(`sysReportUrl(${rep},'Sound stops',false)`);
  const U = new URL(url);
  ok('the GitHub form address carries the template, title, status and problem', U.origin + U.pathname == 'https://github.com/merciless146/RudolfOS/issues/new' && U.searchParams.get('template') == 'system-report.yml' && U.searchParams.get('title') == '[Report] Sound stops' && U.searchParams.get('status').includes('Rudolf OS:') && U.searchParams.get('what') == 'Sound stops after sleep', url.slice(0, 200));
  ok('...and the full report when it fits', new URL(await ev(`sysReportUrl(${rep},'x',true)`)).searchParams.get('report') == r.text);
  // the dialog: saving to a file; without internet it says so instead of opening GitHub
  await ev(`sysReportDialog();1`);
  await ev(`document.querySelector('.srt').value='Test report';document.querySelector('.srw').value='testing';document.querySelector('.srs').click();1`);
  ok('"Save to a file" puts the report in C:\\DOCUMENTS', await until(`Object.keys(fs['C:'].DOCUMENTS||{}).some(n=>/^REPORT-\\d+\\.TXT$/.test(n)&&fs['C:'].DOCUMENTS[n].includes('== Status'))`, 8000));
  await ev(`document.querySelector('.srg').click();1`); await wait(2500);
  const online = await ev(`typeof netUp=='function'&&netUp()`);
  if (online) ok('Continue opens the GitHub form in Oracle', await ev(`[...RT.values()].some(t=>t.url().startsWith('https://github.com/merciless146/RudolfOS/issues/new'))`));
  else ok('without internet, the dialog says to save the report and send it later', await ev(`(document.querySelector('.srm')||{}).textContent.includes('no internet connection')`));
  await ev(`document.querySelectorAll('.rmodal').forEach(o=>o.remove());wins.slice().forEach(w=>w.close());1`);
  console.log(`RESULT ${R.filter(Boolean).length}/${R.length}`);
  process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(2) });
