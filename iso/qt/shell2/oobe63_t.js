// 1.6.3 (the owner, 2026-11-26, from FreeBuff's user test): the setup wizard in the language chosen on its first page.
// Before, choosing Español left steps 2-5 in English: their words were not rows of I18N_ROWS, "Step n of m", Back, Next and
// Finish were built in English, the dialog's name was an attribute TRX does not watch, and a red error stayed after the
// mistake was fixed. Walks the wizard in Spanish, Japanese and back to English. The password minimum stays 4 (the owner).
// node oobe63_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../..', 'iso/www'), PORT = 8756;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(PORT, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1024, height: 768}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForSelector('#oobe', {timeout: 30000});
  const txt = () => p.evaluate(() => [...document.querySelectorAll('#oobe .oostep,#oobe h2,#oobe p,#oobe label,#oobe button,#oobe .err')].map(e => e.textContent.trim()).filter(Boolean).join(' | '));
  const nx = async () => { await p.click('#oobe .nx'); await p.waitForTimeout(300) };
  const en = await txt(); ok('English as before', /^Step 1 of 5 \| Welcome \| Let's set up Rudolf OS\. .* \| Next$/.test(en), en);
  await p.selectOption('#oobe .lg', 'es'); await p.waitForTimeout(300);
  const s1 = await txt(); ok('1: Spanish at once (heading, words, step, Next)', s1.startsWith('Paso 1 de 5 | Bienvenido | Vamos a configurar') && s1.endsWith('Siguiente'), s1);
  await nx(); const s2 = await txt();
  ok('2: every word Spanish', s2 == 'Paso 2 de 5 | Hazlo cómodo | Texto y botones más grandes | Lector de pantalla (lee la pantalla en voz alta) | Alto contraste | Puedes cambiar esto en cualquier momento en Configuración → Accesibilidad. | Atrás | Siguiente', s2);
  ok("the dialog's name", await p.getAttribute('#oobe', 'aria-label') == 'Configurar Rudolf OS');
  await nx(); await nx(); const s3 = await txt();
  ok('3: Spanish, and the missing name in Spanish', s3.includes('Crea tu cuenta') && s3.includes('Tu nombre') && s3.includes('Escribe un nombre (letras y números).'), s3);
  await p.click('#oobe .nm'); await p.keyboard.type('Ana'); await p.waitForTimeout(150);
  ok('the error goes once the name is typed', (await p.textContent('#oobe .err')) == '');
  await p.click('#oobe .p1'); await p.keyboard.type('ab'); await p.click('#oobe .p2'); await p.keyboard.type('ab'); await nx();
  const e4 = await p.textContent('#oobe .err'); ok('a short password: in Spanish, 4 characters still the minimum', e4 == 'Usa una contraseña de al menos 4 caracteres.', e4);
  await p.click('#oobe .p1'); await p.keyboard.type('cd'); await p.click('#oobe .p2'); await p.keyboard.type('cd'); await nx();
  const s4 = await txt(); ok('4: Spanish', s4.startsWith('Paso 4 de 5 | Privacidad | Rudolf OS guarda') && s4.includes('Estás ejecutando Rudolf OS en un navegador web.'), s4);
  await nx(); const s5 = await txt(); ok('5: Spanish, Finish', s5.startsWith('Paso 5 de 5 | Todo listo | Inicia sesión') && s5.includes('Pulsa F1') && s5.endsWith('Atrás | Finalizar'), s5);
  for (let i = 0; i < 4; i++) { await p.click('#oobe .bk'); await p.waitForTimeout(200) }
  await p.selectOption('#oobe .lg', 'ja'); await p.waitForTimeout(300); const ja = await txt();
  ok('Japanese: its own step order', ja.startsWith('ステップ 1 / 5 | ようこそ') && ja.endsWith('次へ'), ja);
  await p.selectOption('#oobe .lg', 'en'); await p.waitForTimeout(300); ok('back to English', (await txt()) == en, await txt());
  ok('no page errors', !errs.length, errs);
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); await b.close(); srv.close(); process.exit(R.every(Boolean) ? 0 : 1) })().catch(e => { console.log('FAIL ' + e.message); process.exit(1) });
