// 1.6.0 (2026-11-19): File Explorer's Copy to another drive (drivecopy60.js) with a stand-in rudolfd: the drives it
// offers, the folder name, the request (fs/xcopy), the progress, a drive that fills up and the next one continuing,
// and the button in a drive's toolbar. node drivecopy60_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8144, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'usb', live: 0, recovery: 0};
    window.T = {calls: [], bodies: [], job: null, end: 0};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = (x, st) => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href);
      if (u.includes('/api/')) T.calls.push(q.pathname.slice(5) + q.search);
      if (q.pathname == '/api/disks') return J({disks: [
        {name: 'sda', removable: false, model: 'HP disk', partitions: [{name: 'sda1', fs: 'ntfs', label: 'System Reserved', bytes: 5e8, mount: ''}, {name: 'sda2', fs: 'ntfs', label: '', bytes: 4.6e11, mount: '/media/sda2'}]},
        {name: 'sdb', removable: true, model: 'Rudolf stick', partitions: [{name: 'sdb1', fs: 'iso9660', label: 'RUDOLF', bytes: 2e9, mount: '/live/media'}, {name: 'sdb3', fs: 'ext4', label: 'RUDOLFDATA', bytes: 3e10, mount: '/persist'}]},
        {name: 'sdc', removable: true, model: 'SanDisk', partitions: [{name: 'sdc1', fs: 'vfat', label: 'STICK2', bytes: 6.4e10, mount: ''}]}]});
      if (q.pathname == '/api/mount') return J({});
      if (q.pathname == '/api/umount') return J({});
      if (q.pathname == '/api/fs/xcopy') { T.bodies.push(o.body); T.job = {n: 0}; return J({message: 'started'}) }
      if (q.pathname == '/api/fs/list') return J({items: [{name: 'Users', dir: true, size: 0, mtime: 0}, {name: 'Windows', dir: true, size: 0, mtime: 0}]});
      if (q.pathname == '/api/job') { const j = T.job; if (!j) return J({name: '', running: false, status: 0, log: ''}); j.n++;
        const head = 'Counting what to copy…\n1200 files, 30.0 GB, to copy into /media/sdc1/x (64.0 GB free there).\n';
        if (j.n < 3) return J({name: 'drive-copy', running: true, status: -1, log: head + 'PROGRESS 300 1200 7500000000 30000000000\n  Users/Grandpa/Pictures/IMG_1.JPG\n'});
        return J({name: 'drive-copy', running: false, status: T.end, log: head + 'PROGRESS 1200 1200 30000000000 30000000000\n' + (T.end ? 'The drive is full: 700 of 1200 files\n' : 'Done: 1200 files (30.0 GB) copied into /media/sdc1/x.\n')}) }
      if (u.includes('/api/')) return J({items: []});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8144/'); await p.waitForFunction(() => typeof driveCopyDlg == 'function' && typeof explorer == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()})})()");
  const D = () => q("(()=>{const o=document.querySelector('.dcdlg');return o?{opts:[...o.querySelectorAll('.dcto option')].map(x=>x.textContent),name:o.querySelector('.dcn').value,msg:o.querySelector('.dcm').textContent,cont:!o.querySelector('.dccl').hidden,pt:o.querySelector('.dcpt').textContent,bar:o.querySelector('.dcbar').value,log:o.querySelector('.dcl').textContent,btn:[...o.querySelectorAll('button')].map(b=>b.textContent)}:null})()");
  await q("driveCopyDlg({dev:'sda2',root:'/media/sda2',path:'/media/sda2/Users',label:'sda2 (464.3 GB)'},[{name:'Grandpa',dir:true}])"); await wait(400);
  let d = await D();
  ok('the drives offered: the other stick first, not the drive copied from, not Rudolf OS\'s own stick', d && d.opts.length == 2 && d.opts.some(x => /STICK2/.test(x)) && d.opts.some(x => /System Reserved/.test(x)) && !d.opts.some(x => /RUDOLFDATA|sda2|RUDOLF \(/.test(x)) && /STICK2/.test(d.opts[0]), d && d.opts);
  ok('the folder named after what is copied, with the day', d && /^Grandpa \(copied \d{4}-\d\d-\d\d\)$/.test(d.name), d && d.name);
  ok('FAT32: said that files over 4 GB cannot go on it', d && /FAT32/.test(d.msg), d && d.msg);
  await q("document.querySelector('.dcdlg .dcgo').click()"); await wait(1500);
  d = await D(); const body = await q('T.bodies[0]'), calls = await q('T.calls');
  ok('the stick mounted, then fs/xcopy: personal, into its folder, the selection', calls.includes('mount?dev=sdc1') && /^personal=1 continue=0\n\/media\/sdc1\/Grandpa \(copied [\d-]+\)\n\/media\/sda2\/Users\/Grandpa\n$/.test(body), {body, calls: calls.slice(-4)});
  ok(`the progress: ${d && d.pt}`, d && /300 of 1,200 files · 7\.5 GB of 30\.0 GB/.test(d.pt) && d.bar == 250 && !/PROGRESS/.test(d.log), d);
  await wait(2500); d = await D();
  ok('done: said so, Safely remove offered', d && /^Done/.test(d.msg) && d.btn.some(x => /Safely remove STICK2/.test(x)) && d.btn.includes('Close'), d);
  await q("document.querySelector('.dcdlg .dcx').click()");
  // a stick that fills up, then the next one continuing
  await q("T.end=28;T.bodies=[];driveCopyDlg({dev:'sda2',root:'/media/sda2',path:'/media/sda2/Users',label:'sda2'},[{name:'Grandpa',dir:true}])"); await wait(400);
  await q("document.querySelector('.dcdlg .dcgo').click()"); await wait(4500); d = await D();
  ok('the drive full: plug in the next one and copy again', d && /drive is full/.test(d.msg) && /next one/.test(d.msg), d && d.msg);
  await q("document.querySelector('.dcdlg .dcx').click();T.end=0;T.bodies=[];driveCopyDlg({dev:'sda2',root:'/media/sda2',path:'/media/sda2/Users',label:'sda2'},[{name:'Grandpa',dir:true}])"); await wait(400);
  d = await D(); ok('the same selection again: "Continue from the last drive" offered, ticked', d && d.cont, d);
  await q("document.querySelector('.dcdlg .dcgo').click()"); await wait(600);
  ok('...and asked for (continue=1)', /^personal=1 continue=1\n/.test(await q('T.bodies[0]')), await q('T.bodies[0]'));
  await wait(3000); await q("document.querySelector('.dcdlg .dcx').click()");
  // the button in a drive's toolbar
  await q("explorer(homePath());const W=wins.at(-1);W.openRemote({dev:'sda2',root:'/media/sda2',path:'/media/sda2',label:'sda2 (464.3 GB)'})"); await wait(800);
  const has = await q("(()=>{const b=[...document.querySelectorAll('.xrb button')].find(x=>/Copy to another drive/.test(x.textContent));if(b)b.click();return !!b})()"); await wait(500);
  d = await D(); ok('a drive\'s toolbar: Copy to another drive…, the folder open when nothing is selected', has && d && /^sda2 \(copied/.test(d.name), {has, d});
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
