// 1.4.7: Display Properties' fonts are real fonts (each looks different), and Weather and its widget show the right
// place (the time zone's city, the city chosen, °F in the US). node wx47_t.js [CHROME]  (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), SHOT = process.env.SHOTS || path.join(os.tmpdir(), 'wx47-shots'); fs.mkdirSync(SHOT, {recursive: true});
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8096, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const GEO = {
  Chicago: [{name: 'Chicago', latitude: 41.85, longitude: -87.65, country: 'United States', country_code: 'US', admin1: 'Illinois', timezone: 'America/Chicago'}],
  Paris: [{name: 'Paris', latitude: 33.66, longitude: -95.55, country: 'United States', country_code: 'US', admin1: 'Texas', timezone: 'America/Chicago'}, {name: 'Paris', latitude: 48.85, longitude: 2.35, country: 'France', country_code: 'FR', admin1: 'Île-de-France', timezone: 'Europe/Paris'}],
  Springfield: [{name: 'Springfield', latitude: 39.8, longitude: -89.64, country: 'United States', country_code: 'US', admin1: 'Illinois'}, {name: 'Springfield', latitude: 37.2, longitude: -93.29, country: 'United States', country_code: 'US', admin1: 'Missouri'}],
  Oslo: [{name: 'Oslo', latitude: 59.91, longitude: 10.75, country: 'Norway', country_code: 'NO', admin1: 'Oslo'}]};
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1366, height: 768}});
  const calls = [];
  await ctx.route('https://geocoding-api.open-meteo.com/**', r => { const n = new URL(r.request().url()).searchParams.get('name'); calls.push('geo:' + n); r.fulfill({contentType: 'application/json', body: JSON.stringify({results: GEO[n] || []})}) });
  await ctx.route('https://api.open-meteo.com/**', r => { const u = new URL(r.request().url()); calls.push('wx:' + u.searchParams.get('latitude') + ',' + u.searchParams.get('longitude') + ',' + u.searchParams.get('wind_speed_unit'));
    r.fulfill({contentType: 'application/json', body: JSON.stringify({current: {temperature_2m: 20, weather_code: 0, wind_speed_10m: 10, relative_humidity_2m: 50}, daily: {time: ['2026-11-02'], weather_code: [0], temperature_2m_max: [22], temperature_2m_min: [10]}})}) });
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8096/'); await p.waitForFunction(() => typeof uiFontStack == 'function' && typeof weatherLoc == 'function' && typeof settings == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>{w.__force=1;w.close()});USER=USER||ACCTS[0];USER.r='admin';return 1})()");
  // ---------- fonts ----------
  await q("S.ff='Tahoma';settings('disp');1"); await wait(1500);
  const opts = await q("[...document.querySelectorAll('#ff option')].map(o=>[o.value,o.textContent,o.selected])");
  ok('1. Display Properties offers 10 fonts, all fonts Rudolf OS has, named as they are', opts.length == 10 && !opts.some(o => /^(Tahoma|Verdana|Georgia|Trebuchet MS|Comic Sans MS|Lucida Console)$/.test(o[0])), opts);
  ok('2. a font saved by an older version (Tahoma) shows as its stand-in, DejaVu Sans (what it always looked like)', (opts.find(o => o[2]) || [])[0] == 'DejaVu Sans', opts.find(o => o[2]));
  const w = await q(`(async()=>{await document.fonts.ready;const out={};for(const [n] of uiFonts()){const s=document.createElement('span');s.style.cssText='position:absolute;white-space:nowrap;font-size:40px;font-family:'+uiFontStack(n);s.textContent='The quick brown fox jumps 0123 Wg';document.body.appendChild(s);await document.fonts.load('40px '+JSON.stringify(n));out[n]=Math.round(s.getBoundingClientRect().width*10)/10;s.remove()}return out})()`);
  console.log('   widths:', JSON.stringify(w));
  const vals = Object.values(w); ok('3. every choice looks different (10 different text widths)', new Set(vals).size == vals.length, w);
  ok('4. the built-in fonts are loaded (Gelasio, Fira Sans, Comic Neue)', await q("Promise.all(['Gelasio','Fira Sans','Comic Neue'].map(f=>document.fonts.load('16px \"'+f+'\"').then(l=>l.length>0))).then(a=>a.every(Boolean))"));
  await p.selectOption('#ff', 'Comic Neue'); await wait(300); await p.click('text=Apply'); await wait(500); await p.click('text=Confirm'); await wait(800);
  const ff = await q("[S.ff,document.getElementById('desk').style.fontFamily||document.body.style.fontFamily]");
  ok('5. choosing Comic Neue and Apply changes the desktop\'s font', ff[0] == 'Comic Neue' && /Comic Neue/.test(ff[1]), ff);
  await p.screenshot({path: path.join(SHOT, 'fonts.png')});
  await p.selectOption('#ff', 'DejaVu Sans'); await wait(300); await p.click('text=Apply'); await wait(500); await p.click('text=Confirm'); await wait(500);
  await q("wins.slice().forEach(w=>{w.__force=1;w.close()});1");
  // ---------- weather ----------
  await q("localStorage.removeItem('bd95wloc');localStorage.removeItem('bd95wunit');S.tz='America/Chicago';1");
  let h = await q("weatherNow()");
  ok('6. no city chosen: the widget shows the time zone\'s city (America/Chicago -> Chicago), not Berlin', /Chicago/.test(h) && !/Berlin/.test(h), h);
  ok('7. ...in °F and mph in the United States', /68°F/.test(h) && /mph/.test(h) && calls.some(c => c == 'wx:41.85,-87.65,mph'), [h, calls]);
  await q("S.tz='Europe/Paris';1"); h = await q("weatherNow()");
  ok('8. a new time zone finds its own city: Europe/Paris -> Paris, France (not Paris, Texas), in °C', /Paris/.test(h) && /20°C/.test(h) && calls.some(c => c == 'wx:48.85,2.35,kmh'), [h, calls.slice(-3)]);
  await q("S.tz='UTC';localStorage.removeItem('bd95wloc');1"); h = await q("weatherNow()");
  ok('9. a time zone that names no place (UTC) asks for a city', /Where are you/.test(h), h);
  // the widget on the desktop, then a city chosen in Weather
  await q("S.tz='America/Chicago';localStorage.removeItem('bd95wloc');WG=[{t:'weather',x:900,y:80}];wgDraw();1"); await wait(1500);
  ok('10. the desktop widget shows Chicago', await q("(()=>{const e=document.querySelector('[data-wx]');return !!e&&/Chicago/.test(e.textContent)})()"), await q("(document.querySelector('[data-wx]')||{}).textContent"));
  await q("document.querySelector('[data-wx]').click();1"); await wait(1500);
  ok('11. clicking the widget opens Weather, which says the place comes from the time zone', await q("(()=>{const W=wins.at(-1);return /Weather/.test(W.el.textContent)&&/from your time zone/.test(W.el.textContent)&&/Chicago/.test(W.el.querySelector('h2').textContent)})()"));
  await q("(()=>{const W=wins.at(-1);W.el.querySelector('.c').value='Springfield';W.el.querySelector('.go').click();return 1})()"); await wait(800);
  const ch = await q("[...wins.at(-1).el.querySelectorAll('.ch button')].map(b=>b.textContent)");
  ok('12. a city name with several matches lists them to choose from', ch.length == 2 && /Missouri/.test(ch[1]), ch);
  await q("wins.at(-1).el.querySelectorAll('.ch button')[1].click();1"); await wait(1200);
  ok('13. the chosen city is shown in Weather', await q("/Springfield, Missouri/.test(wins.at(-1).el.querySelector('h2').textContent)"));
  ok('14. ...and the desktop widget follows at once', await q("/Springfield/.test(document.querySelector('[data-wx]').textContent)"), await q("document.querySelector('[data-wx]').textContent"));
  await q("S.tz='Europe/Oslo';1"); h = await q("weatherNow()");
  ok('15. a city chosen by the person is kept when the time zone changes', /Springfield/.test(h), h);
  await q("wins.at(-1).el.querySelector('.un').click();1"); await wait(1200);
  ok('16. the °C/°F switch changes Weather and the widget', await q("/°C/.test(wins.at(-1).el.textContent)&&/°C/.test(document.querySelector('[data-wx]').textContent)"));
  await p.screenshot({path: path.join(SHOT, 'weather.png')});
  ok('17. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})();
