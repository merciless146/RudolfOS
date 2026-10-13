// Layout at different screen sizes and scaling: reports anything off-screen.  node lay_t.js INDEX.HTML [WxH@scale ...]
const {chromium} = require('playwright'); const fs = require('fs');
const SIZES = process.argv.slice(3).length ? process.argv.slice(3) : ['1920x1080@100', '1920x1080@150', '1366x768@100', '1366x768@125', '1280x720@100', '1280x720@125', '1024x576@100', '1024x576@125', '1024x576@150', '1280x800@100', '1024x768@100', '1440x900@125'];
const html = fs.readFileSync(process.argv[2], 'utf8');
(async () => {
  const b = await chromium.launch(); let bad = 0;
  for (const sz of SIZES) {
    const [wh, sc] = sz.split('@'), [w, h] = wh.split('x').map(Number);
    const p = await b.newPage({viewport: {width: w, height: h}}); const errs = [];
    p.on('pageerror', e => errs.push(e.message));
    await p.addInitScript(sc => { window.RUDOLFD = {token: 't'}; try { localStorage.setItem('bd95set', JSON.stringify({scale: +sc})) } catch (e) {} }, sc);
    await p.route('**/*', r => {
      const u = r.request().url();
      if (u == 'http://rudolf.test/index.html') return r.fulfill({contentType: 'text/html', body: html});
      if (!u.startsWith('http://rudolf.test/api/')) return r.abort();
      const path = new URL(u).pathname.slice(5);
      const J = o => r.fulfill({contentType: 'application/json', body: JSON.stringify(Object.assign({ok: true}, o))});
      if (path == 'screen') return J({res: 'auto', fps: 60, actual: {w, h, hz: 60, kind: 'native', panel: w + 'x' + h}, display: 'soft', modes: null, pending: false, persistent: true});
      if (path == 'fs/list') return J({items: [{name: 'a.txt', dir: false, size: 5, mtime: 1}]});
      if (path == 'disks') return J({disks: []});
      if (path == 'devices') return J({usb: [], drives: [], inputs: []});
      return J({events: ''});
    });
    await p.goto('http://rudolf.test/index.html'); await p.waitForTimeout(2200);
    // scan: visible elements of these roots that leave the viewport
    const scan = (label, roots) => p.evaluate(([label, roots]) => {
      const out = [], vw = innerWidth, vh = innerHeight;
      for (const sel of roots) for (const root of document.querySelectorAll(sel)) {
        if (!root.getClientRects().length || getComputedStyle(root).visibility == 'hidden') continue;
        const rr = root.getBoundingClientRect();
        if (rr.right > vw + 1 || rr.bottom > vh + 1 || rr.left < -1 || rr.top < -1) out.push(`${label}: ${sel} box ${Math.round(rr.left)},${Math.round(rr.top)} ${Math.round(rr.width)}x${Math.round(rr.height)} (screen ${vw}x${vh})`);
        for (const e of root.querySelectorAll('button,input,select,a,[role=button]')) {
          if (!e.getClientRects().length) continue;
          const r = e.getBoundingClientRect(); if (!r.width || !r.height) continue;
          // inside a scrolling box that can reach it: fine
          let s = e.parentElement, reach = false;
          while (s && s !== root.parentElement) { const cs = getComputedStyle(s); if (/(auto|scroll)/.test(cs.overflowY + cs.overflowX) && s.scrollHeight > s.clientHeight + 1 || s.scrollWidth > s.clientWidth + 1 && /(auto|scroll)/.test(cs.overflowX)) { reach = true; break } s = s.parentElement }
          if (!reach && (r.right > vw + 1 || r.bottom > vh + 1 || r.left < -1 || r.top < -1)) { out.push(`${label}: ${sel} control "${(e.textContent || e.value || e.getAttribute('aria-label') || e.tagName).trim().slice(0, 30)}" at ${Math.round(r.left)},${Math.round(r.top)} off-screen`); if (out.length > 40) return out }
        }
      }
      return out;
    }, [label, roots]);
    const found = [];
    found.push(...await scan('setup', ['#oobe']));
    await p.evaluate(() => { ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }) });
    found.push(...await scan('desktop', ['#bar', '#tb']));
    await p.evaluate(() => { const s = document.querySelector('#start,#sbtn,.startb'); s && s.click() }); await p.waitForTimeout(400);
    found.push(...await scan('start menu', ['#sm', '#menu']));
    await p.keyboard.press('Escape'); await p.evaluate(() => document.body.click());
    const apps = [['Settings', "settings('disp')"], ['File Explorer', 'explorer()'], ['Oracle', 'browser()'], ['Notepad', 'notepad()'], ['Calculator', 'calc()'], ['Settings Mouse', "settings('mouse')"], ['Settings Oracle', "settings('web')"], ['Settings Devices', "settings('devs')"]];
    for (const [n, js] of apps) {
      await p.evaluate(js => { try { closeAll() } catch (e) {} ; eval(js) }, js); await p.waitForTimeout(700);
      found.push(...await scan(n, ['.win']));
      await p.evaluate(() => { const w = [...document.querySelectorAll('.win')].pop(); w && w.querySelector('.mx') && w.querySelector('.mx').click() }); await p.waitForTimeout(200);
      found.push(...(await scan(n + ' maximised', ['.win'])));
      const over = await p.evaluate(() => { const w = [...document.querySelectorAll('.win')].pop(), bar = document.getElementById('bar'); if (!w || !bar) return ''; const a = w.getBoundingClientRect(), t = bar.getBoundingClientRect(); return a.bottom > t.top + 1 && a.top < t.top ? `maximised window covers the taskbar (${Math.round(a.bottom)} > ${Math.round(t.top)})` : '' });
      if (over) found.push(n + ': ' + over);
    }
    await p.evaluate(() => { try { closeAll() } catch (e) {} ; const i = document.createElement('input'); i.type = 'file'; document.body.appendChild(i); i.click() }); await p.waitForTimeout(600);
    found.push(...await scan('file picker', ['.rfpick .rmbox']));
    await p.evaluate(() => { document.querySelectorAll('.rfpick').forEach(e => e.remove()); notify('A long notification title to test wrapping', 'Body text of the notification that is fairly long so that it wraps over a few lines.') }); await p.waitForTimeout(500);
    found.push(...await scan('notification', ['#ntoasts']));
    if (errs.length) found.push('page errors: ' + errs.slice(0, 3).join(' | '));
    console.log(`${sz}: ${found.length ? found.length + ' problems' : 'OK'}`); found.slice(0, 25).forEach(f => console.log('   ' + f)); bad += found.length;
    await p.screenshot({path: `/tmp/lay-${sz}.png`}); await p.close();
  }
  console.log('TOTAL', bad); await b.close();
})();
