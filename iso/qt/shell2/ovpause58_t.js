// 1.5.8: the moving parts of the dynamic backgrounds (Spirit Falls' petals and lanterns, Glacier Lake's northern lights)
// run only when they can be seen: not behind a window covering most of the screen (a game in an Oracle tab ran choppy
// on the Vivobook), not over a picture of one's own, not in power saver; half the frames on battery. And Display
// Properties' dynamic backgrounds set a picture aside when one is chosen.   node ovpause58_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8265, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8265/'); await p.waitForFunction(() => typeof dynOvCheck == 'function' && typeof stUpdate == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});S.rmotion=false;S.wpimg='';S.dyn='oasis';S.dm='light';applySet();return 1})()`);
  await wait(1300);
  const moving = async () => { const a = await q('PET.cv.toDataURL()'); await wait(500); return a != await q('PET.cv.toDataURL()') };
  ok('1. Spirit Falls on its own: the petals move', await q('DYNOV.vis') && await moving());
  await q(`(()=>{const d=document.createElement('div');d.style.cssText='position:fixed;left:0;top:0;width:100vw;height:90vh';document.body.appendChild(d);window.__fw={el:d,ws:typeof WS!='undefined'?WS:1};wins.push(__fw);return 1})()`);
  await wait(1300);
  ok('2. a window covering most of the screen (a game in an Oracle tab): they stop', !(await q('DYNOV.vis')) && !(await moving()));
  await q(`(()=>{wins.splice(wins.indexOf(__fw),1);__fw.el.remove();return 1})()`); await wait(1300);
  ok('3. ...and move again when it closes', await q('DYNOV.vis') && await moving());
  await q(`(()=>{window.__bsaver=true;return 1})()`); await wait(1300);
  ok('4. power saver: they stop', !(await q('DYNOV.vis')) && !(await moving()));
  await q(`(()=>{window.__bsaver=false;PWR={battery:{status:'Discharging',capacity:50},now:{profile:'balanced'}};return 1})()`); await wait(1300);
  ok('5. on battery: they move, at half the frames', await q('DYNOV.vis') && await q('DYNOV.slow') && await moving());
  await q(`(()=>{PWR={battery:{status:'Discharging',capacity:50},now:{profile:'quiet',epp:'power'}};return 1})()`); await wait(1300);
  ok('6. the power saver profile (quiet) on battery: they stop', !(await q('DYNOV.vis')));
  await q(`(()=>{PWR=null;WPURL='data:image/png;base64,iVBORw0KGgo=';WPID='p1';S.wpimg='p1';WPREADY=true;applySet();dynOvCheck();return 1})()`); await wait(500);
  ok('7. a picture of one\'s own over a still-chosen Spirit Falls: no statue, no petals', await q("STS.el.style.display=='none'&&!PET.timer&&!dynShown()"));
  await q(`(()=>{S.dyn='lake';S.dm='dark';S.aur=true;applySet();return 1})()`); await wait(300);
  ok('8. ...nor northern lights over it with Glacier Lake still chosen', await q('!AUR.on'));
  await q(`(()=>{settings('disp');return 1})()`); await wait(800);
  await q(`(()=>{document.querySelector('.dynb[data-dyn=oasis]').click();return 1})()`); await wait(400);
  ok('9. choosing a dynamic background sets the picture aside, so it is what is shown', await q("S.dyn=='oasis'&&!S.wpimg&&dynShown()"));
  const bt = await q(`(()=>{BATAVG.v=0;const b=v=>({status:'Discharging',energy_now:20e6,energy_full:40e6,power_now:v});const a=batTime(b(5e6));BATAVG.t-=60000;const c=batTime(b(10e6));return [a,c]})()`);
  ok('10. the battery\'s time left follows the power averaged over minutes, not one reading', bt[0] == '4 h 0 min left' && /^3 h/.test(bt[1]), bt);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
