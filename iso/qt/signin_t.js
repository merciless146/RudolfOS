// Oracle sign-in routing, with the real "web frames" extension in Chromium.  node si_t.js INDEX.HTML FRAMES-EXT-DIR
const {chromium} = require('playwright'); const fs = require('fs'), os = require('os'), path = require('path');
(async () => {
  const ext = fs.mkdtempSync(path.join(os.tmpdir(), 'fx-')); fs.cpSync(process.argv[3], ext, {recursive: true});
  const ctx = await chromium.launchPersistentContext(fs.mkdtempSync(path.join(os.tmpdir(), 'pf-')), {headless: true, channel: 'chromium', viewport: {width: 1280, height: 800},
    args: ['--headless=new', `--disable-extensions-except=${ext}`, `--load-extension=${ext}`, '--host-resolver-rules=MAP *.test 127.0.0.1', '--no-proxy-server']});
  const errs = [], posts = [];
  await ctx.route('http://127.0.0.1:8080/**', r => {
    const u = new URL(r.request().url());
    if (!u.pathname.startsWith('/api/')) return r.continue();   // the page itself comes from a real server on 127.0.0.1:8080, as on Rudolf OS
    const p = u.pathname.slice(5); if (r.request().method() == 'POST') posts.push(p + u.search);
    const J = o => r.fulfill({contentType: 'application/json', body: JSON.stringify(Object.assign({ok: true}, o))});
    if (p == 'web/status') return J({running: false, shown: false});
    return J({events: ''});
  });
  const pg = ctx.pages()[0] || await ctx.newPage(); pg.on('pageerror', e => errs.push(e.message));
  await pg.addInitScript(() => { window.RUDOLFD = {token: 't'} });
  await pg.goto('http://127.0.0.1:8080/'); await pg.waitForTimeout(2500);
  await pg.evaluate(() => { ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }) });
  const say = (n, ok, d) => console.log((ok ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? '  ' + JSON.stringify(d).slice(0, 300) : ''));
  const W = (f, t = 6000) => pg.waitForFunction(f, null, {timeout: t}).then(() => true, () => false);
  const opened = () => posts.filter(x => x.startsWith('web/open')).map(x => decodeURIComponent(x.split('url=')[1] || ''));
  const site = () => pg.frames().find(f => /\.test:8082/.test(f.url()));
  // 1) a site without a sign-in stays in Oracle; clicking around updates the address bar
  await pg.evaluate(() => browser('http://shop.test:8082/')); await pg.waitForTimeout(2000);
  say('a normal site loads in Oracle', !!site() && await site().$('#login') != null);
  await site().click('#login'); await pg.waitForTimeout(1800);
  say('the address bar follows the site', await pg.evaluate(() => document.querySelector('.win .ad').value) == 'http://shop.test:8082/login', await pg.evaluate(() => document.querySelector('.win .ad').value));
  // 2) a password field: Oracle asks
  say('Oracle asks when a page wants a password', await W(() => !!document.querySelector('.sibar')), await pg.evaluate(() => (document.querySelector('.sibar') || {}).innerText));
  // 3) Not now: the sign-in form is sent inside Oracle
  await pg.click('.sibar .si-no'); await pg.waitForTimeout(300);
  await site().fill('#pw', 'secret'); await site().click('#go'); await pg.waitForTimeout(1500);
  say('"Not now" lets the sign-in go ahead in Oracle', !!pg.frames().find(f => /\/session$/.test(f.url())) && !opened().length, opened());
  // 4) sending a sign-in moves the site, without sending the form
  await pg.evaluate(() => browser('http://shop2.test:8082/login')); await pg.waitForTimeout(2000);
  const f2 = pg.frames().find(f => /shop2\.test:8082\/login/.test(f.url()));
  await f2.fill('#pw', 'secret'); await f2.press('#pw', 'Enter'); await pg.waitForTimeout(1500);
  say('sending a sign-in moves the site to the browser window', opened().includes('http://shop2.test:8082/login'), opened());
  say('the form was not sent inside Oracle', !pg.frames().some(f => /shop2\.test:8082\/session/.test(f.url())));
  say('the tab explains where the site went', await pg.evaluate(() => [...document.querySelectorAll('.win iframe')].some(f => /opened in a browser window/.test(f.srcdoc || ''))));
  // 5) "Sign in with Google" moves the site, no pop-up
  const pages0 = ctx.pages().length;
  await pg.evaluate(() => browser('http://news.test:8082/')); await pg.waitForTimeout(2000);
  await pg.frames().find(f => /news\.test/.test(f.url())).click('#google'); await pg.waitForTimeout(1500);
  say('"Sign in with Google" moves the site', opened().includes('http://news.test:8082/') && ctx.pages().length == pages0, {o: opened(), pages: ctx.pages().length});
  // 6) password field added later by a single-page app, with "Always for this site"
  await pg.evaluate(() => browser('http://app.mail.test:8082/spa')); 
  say('a password field added later is noticed', await W(() => [...document.querySelectorAll('.sibar')].some(b => b.style.display != 'none')));
  await pg.evaluate(() => [...document.querySelectorAll('.sibar')].find(b => b.style.display != 'none').querySelector('.si-always').click()); await pg.waitForTimeout(500);
  say('"Always for this site" remembers the site', await pg.evaluate(() => S.webSites.includes('mail.test')), await pg.evaluate(() => S.webSites));
  const n = opened().length; await pg.evaluate(() => browser('http://www.mail.test:8082/')); await pg.waitForTimeout(1200);
  say('a remembered site goes straight to the browser window', opened().length == n + 1 && opened().includes('http://www.mail.test:8082/'), opened());
  // 7) built-in and bank sites never load in a frame
  await pg.evaluate(() => { browser('https://accounts.google.com/'); browser('http://mybank.test:8082/') }); await pg.waitForTimeout(1500);
  say('built-in and bank sites open in the browser window', opened().includes('https://accounts.google.com/') && opened().includes('http://mybank.test:8082/'), opened());
  say('...and are never framed', !pg.frames().some(f => /accounts\.google|mybank/.test(f.url())));
  // 8) frame protections: removed for ordinary sites, kept for sign-in sites
  await pg.evaluate(() => { for (const h of ['other.test', 'mybank.test', 'www.mail.test']) { const f = document.createElement('iframe'); f.src = `http://${h}:8082/xfo`; f.id = 'x-' + h.split('.')[0]; document.body.appendChild(f) } }); await pg.waitForTimeout(2000);
  const shown = async h => { const f = pg.frames().find(x => x.url().includes(h)); try { return f ? await f.$('#ok') != null : false } catch (e) { return false } };
  say('ordinary sites can still be shown in Oracle', await shown('other.test'));
  say('bank sites keep their frame protections', !(await shown('mybank.test')));
  say('sites added in Settings keep their frame protections', !(await shown('www.mail.test')));
  // 9) Settings page
  await pg.evaluate(() => settings('web')); await pg.waitForTimeout(800);
  say('Settings > Oracle sign-in lists the added site', await pg.evaluate(() => /mail\.test/.test(document.body.innerText) && /Ask first/.test(document.body.innerText)));
  say('no page errors', !errs.length, errs);
  await ctx.close();
})();
