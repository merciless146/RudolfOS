/* 1.6.1 (docs/plan-1.6.1.md Part 28): what Standard (the desktop page in Chromium) makes of File Explorer's items dragged onto the desktop
   and pasted there, for xdesk161_t.sh to compare with Native's. The page is served by same_rd.py as rudolfd serves it to Standard; Ann
   signs in; the same steps as Native's, with the mouse at the same places of the screen: an Explorer window (Downloads) over the left of
   the screen, BOX and the Trash where Native's test put them (13,6 and 14,7); A.TXT dragged to the cell 12,1, B.TXT with Ctrl to the
   same cell, C.TXT onto BOX, D.TXT onto the Trash; Copy NOTE.TXT, the desktop's Paste at 12,4; Cut E.TXT, Ctrl+V on the desktop; Copy
   NOTE.TXT, BOX's Paste into folder; a desktop file's Copy, Paste in File Explorer's Documents.
   node explorer-desk161_std.js PORT PASSWORD   (prints the folders as xdesk161_t.sh's LS reads them, then the icons' cells) */
const {chromium} = require('playwright');
const [PORT, PW] = process.argv.slice(2); const BASE = 'http://127.0.0.1:' + PORT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const LS = "JSON.stringify({desk:Object.fromEntries(Object.keys(deskFiles()).sort().map(n=>[n,isDir(deskFiles()[n])?Object.keys(deskFiles()[n]).sort():deskFiles()[n]])),dl:Object.keys(node(downloadsPath())).sort(),docs:Object.keys(node(homePath('DOCUMENTS'))).sort(),tr:TR.map(t=>t.n)})";
(async () => {
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: 1280, height: 800}});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message)); const q = s => p.evaluate(s);
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && document.getElementById('login') && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1500); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => typeof USER != 'undefined' && USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000});
  await wait(4000);
  /* (the display scaling Native's test has: 100 %, the desktop's cells 84 x 86 pixels on the screen) */
  await q("(()=>{S.scale=100;applySet();return 1})()"); await wait(800);
  await q(`(()=>{wins.slice().forEach(w=>w.close());const D=node(downloadsPath());Object.keys(D).forEach(k=>delete D[k]);['A','B','C','D','E','NOTE'].forEach(n=>D[n+'.TXT']='file '+n);
    const K=deskFiles();Object.keys(K).forEach(k=>delete K[k]);K.BOX={};fsSave();EXPL.forEach(f=>f());cells['file:BOX']=[13,6];cells['Trash']=[14,7];layIcons();
    const A=explorer(downloadsPath());A.el.style.cssText='left:40px;top:20px;width:900px;height:526px';window.WA=A;return 1})()`); await wait(3000);
  /* (only that window: what the sign-in opened meanwhile closed, as Native's test has no other window) */
  await q("(()=>{wins.filter(w=>w!==WA).forEach(w=>w.close());WA.focus();WA.el.style.cssText+=';left:40px;top:20px;width:900px;height:526px';return 1})()"); await wait(500);
  const cxy = (c, r) => q(`(()=>{const d=DESK.getBoundingClientRect();return {x:d.left+8+${c}*84+42,y:d.top+8+${r}*86+43}})()`);
  const row = n => q(`(()=>{const e=WA.body.querySelector('[data-k="'+downloadsPath().join('/')+'/${n}"]');if(!e)return null;const r=e.getBoundingClientRect();return {x:r.left+Math.min(40,r.width/2),y:r.top+r.height/2}})()`);
  const icon = n => q(`(()=>{const e=[...DESK.querySelectorAll(':scope > .ic')].find(x=>(x.dataset.fn||x.querySelector('span').textContent)==${JSON.stringify(n)});if(!e)return null;const r=e.getBoundingClientRect();return {x:r.left+42,y:r.top+30}})()`);
  const done = () => p.waitForFunction(() => !wins.some(w => w.xfer && !w.xfer.end), null, {timeout: 30000}).then(() => wait(600)).catch(() => 0);
  const drag = async (a, t, ctrl) => { await p.mouse.move(a.x, a.y); await p.mouse.down(); await p.mouse.move(a.x + 20, a.y + 5, {steps: 3}); await p.mouse.move(t.x, t.y, {steps: 10}); await wait(60);
    if (ctrl) await p.keyboard.down('Control'); await p.mouse.move(t.x + 1, t.y + 1, {steps: 2}); await p.mouse.up(); if (ctrl) await p.keyboard.up('Control'); await wait(300); await done() };
  const menu = async (pt, label) => { await p.mouse.click(pt.x, pt.y, {button: 'right'}); await wait(200);
    return q(`(()=>{const it=[...document.querySelectorAll('.ctx .cmi')].find(x=>x.querySelector('span').textContent==${JSON.stringify(label)});const ok=!!it&&!it.classList.contains('dis');if(ok)it.click();document.querySelectorAll('.ctx').forEach(x=>x.remove());return ok})()`) };
  const tool = async (n, cls) => { const r = await row(n); await p.mouse.click(r.x, r.y); await q(`WA.body.querySelector('${cls}').click()`); await wait(200) };
  await drag(await row('A.TXT'), await cxy(12, 1));
  await drag(await row('B.TXT'), await cxy(12, 1), true);
  await drag(await row('C.TXT'), await icon('BOX'));
  await drag(await row('D.TXT'), await icon('Trash'));
  await tool('NOTE.TXT', '.cp'); await menu(await cxy(12, 4), 'Paste'); await done();
  await tool('E.TXT', '.cu'); const e0 = await cxy(14, 0); await p.mouse.click(e0.x, e0.y); await p.keyboard.press('Control+v'); await done();
  await tool('NOTE.TXT', '.cp'); const bx = await icon('BOX'); await p.mouse.click(bx.x, bx.y); await menu(bx, 'Paste into folder'); await done();
  const ai = await icon('A.TXT'); await p.mouse.click(ai.x, ai.y); await menu(ai, 'Copy');
  await q("(()=>{WA.openPath(homePath('DOCUMENTS'));return 1})()"); await wait(400); await q("WA.body.querySelector('.ps').click()"); await done();
  console.log(await q(LS));
  console.log(await q(`['A.TXT','B.TXT','NOTE.TXT'].map(n=>n+'='+(cells['file:'+n]||[]).join(',')+' ').join('')`));
  if (errs.length) console.error('page errors: ' + errs.join(' | '));
  await br.close();
})().catch(e => { console.error(e); process.exit(1) });
