// 1.6.0 (the owner's Vivobook report of 2026-11-16: Google sign-in in Claude's Linux desktop app): a Linux app's own link
// scheme (claude://, discord://, teststand://) that an Oracle tab is sent to after signing in on a site, or that another
// Linux app asks the desktop to open, goes to the app that registered it (its launcher's MimeType x-scheme-handler/...),
// after "Open <app>?"; with the link as the launch's one argument (rudolfd's debian/launch or apps/run, the body);
// unknown schemes and the web's own stay refused; Settings › Apps › Links shows and changes which app opens each.
// The page is the built desktop (iso/www/index.html) with a stand-in for rudolfd; the tab's event is the shell's message.
// node links60_t.js [CHROME]  (build iso/www first; port 8602)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 600) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8602, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.__calls = []; window.__portal = []; window.__launched = 0;
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      const body = o && typeof o.body == 'string' ? o.body : '';
      if (/\/api\/(debian\/launch|apps\/run|apps\/show)/.test(u)) { __calls.push([u.replace(/^.*\/api\//, ''), body]); if (!/show/.test(u)) __launched++; return J({message: 'started'}) }
      if (u.includes('/api/apps/installed')) return J({apps: [{id: 'com.discordapp.Discord', name: 'Discord', version: '1', size: '1 MB', origin: 'flathub',
        launchers: [{desktop: 'com.discordapp.Discord', name: 'Discord', icon: 'com.discordapp.Discord', comment: 'Chat', mime: 'x-scheme-handler/discord;', wmclass: 'discord', cats: 'Network;'}]}]});
      if (u.includes('/api/debian/status')) return J({installed: true, running: true});
      if (u.includes('/api/debian/apps')) return J({apps: [
        {id: 'teststand', name: 'Test Stand', wmclass: 'teststand', mime: 'x-scheme-handler/teststand;', categories: 'Utility;', icon: '', menu: true},
        {id: 'claude-desktop', name: 'Claude', wmclass: 'Claude', mime: 'x-scheme-handler/claude;', categories: 'Office;', icon: '', menu: true},
        {id: 'otherstand', name: 'Other Stand', wmclass: 'otherstand', mime: 'text/plain;x-scheme-handler/teststand;', categories: 'Utility;', icon: '', menu: true}]});
      if (u.includes('/api/apps/windows')) return J({mode: 'desktop', windows: __launched ? [{win: '3a00007', class: 'teststand', instance: 'teststand', title: 'Test Stand'}, {win: '3c00009', class: 'discord', instance: 'discord', title: 'Discord'}] : []});
      if (u.includes('/api/portal/poll')) return J({requests: __portal.splice(0)});
      if (u.includes('/api/')) return J({apps: [], items: [], windows: []});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8602/'); await p.waitForFunction(() => typeof lkOpen == 'function' && FSLOADED && fs['C:'] && fs['C:'].USERS, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>{w.__force=1;w.close()});USER=USER||ACCTS[0];if(!HOME){HOME={DOWNLOADS:{}};fs['C:'].USERS[uname()]=HOME}return 1})()");
  await q("(async()=>{await lxInstalled(true);DEB.st=null;await debApps();lxRegister();return lxLaunchers().length})()");
  const v = await q("['teststand://done?code=X42','claude://claude.ai/login?token=abc','discord://-/channels/1','https://claude.ai/','http://x/','javascript:alert(1)','file:///etc/passwd','data:text/html,x','mailto:a@b','tel:123','teststand://a b','teststand://a\\nb','x://y','teststand://'+'a'.repeat(8100),'zzz://unknown'].map(lkLinkOk)");
  ok('1. links an app may be given: its own schemes; never the web\'s, the browser\'s, file:, javascript:, data:, mail or phone; no spaces or control characters, not too long',
     JSON.stringify(v) == JSON.stringify([true, true, true, false, false, false, false, false, false, false, false, false, false, false, true]), v);
  const sch = await q("JSON.stringify([...lkSchemes()].map(([s,l])=>[s,l.map(x=>x.name)]))");
  ok('2. the schemes Linux apps register (Debian\'s and Flathub\'s launchers), with their apps', sch == JSON.stringify([['discord', ['Discord']], ['claude', ['Claude']], ['teststand', ['Other Stand', 'Test Stand']]]) || (/"teststand",\["Other Stand","Test Stand"\]/.test(sch) && /"claude",\["Claude"\]/.test(sch) && /"discord",\["Discord"\]/.test(sch)), sch);
  // an Oracle window with a tab whose page (a sign-in) sends it to the app's scheme: the shell's message (tabs26.js)
  await q("(()=>{browser('about:blank');const W=wins.at(-1);window.__W=W;window.__ld=[];RT.set('tz',{h:{load:(on,err)=>__ld.push([on,err||''])},W,url:()=>'https://auth.example/consent',last:''});SHELL=window;return W.title||1})()");
  const tabev = (url) => q(`window.postMessage({rudolfShell:'tabev',tid:'tz',ev:'load',loading:false,error:'net::ERR_UNKNOWN_URL_SCHEME',url:${JSON.stringify(url)}},'*')`);
  await q("(()=>{const v=ld(LKK(),{});v.teststand='deb-teststand';svj(LKK(),v);return 1})()");
  await tabev('teststand://done?code=X42&state=s1'); await wait(800);
  let d = await q("(()=>{const o=__W.body.querySelector('.sdlg [role=alertdialog]');return o?{t:o.querySelector('h3').textContent,m:o.querySelector('.sdm').textContent,b:[...o.querySelectorAll('button')].map(x=>x.textContent),ld:__ld}:{none:1,ld:__ld}})()");
  ok('3. a tab sent to teststand:// asks "Open Test Stand?" over its Oracle window, saying which site wants it', d.t == 'Open Test Stand?' && /auth\.example wants to open Test Stand/.test(d.m) && JSON.stringify(d.b) == '["Cancel","Open Test Stand"]', d);
  ok('4. ...and the tab stays on its page without an error meanwhile', JSON.stringify(d.ld) == '[[false,""]]', d.ld);
  await q("__W.body.querySelector('.sdlg .lkc').click()"); await wait(600);
  let c = await q("__calls.slice()"); ok('5. Cancel: the app is not started', c.length == 0, c);
  await tabev('teststand://done?code=X42&state=s1'); await wait(800);
  await q("__W.body.querySelector('.sdlg .lko').click()"); await wait(2500);
  c = await q("__calls.slice()");
  ok('6. Open: Debian\'s launcher is started with the link as its one argument (the request\'s body, not in its address)', c.length >= 1 && c[0][0] == 'debian/launch?id=teststand&link=1' && c[0][1] == 'teststand://done?code=X42&state=s1', c);
  ok('7. ...and the app\'s window is brought to the front', c.some(x => x[0] == 'apps/show?win=3a00007'), c);
  // the Flatpak app (Discord's own links)
  await q("__calls.length=0"); await tabev('discord://-/channels/1/2'); await wait(800);
  await q("__W.body.querySelector('.sdlg .lko').click()"); await wait(1500);
  c = await q("__calls.slice()");
  ok('8. a Flathub app\'s scheme (discord://): the app is started through its launcher with the link', c.length >= 1 && c[0][0] == 'apps/run?id=com.discordapp.Discord&desktop=com.discordapp.Discord&link=1' && c[0][1] == 'discord://-/channels/1/2', c);
  // a scheme nobody registered: refused with Oracle's message, nothing asked
  await q("__calls.length=0;__ld.length=0"); await tabev('zzz://unknown/x'); await wait(1500);
  d = await q("({dlg:!!__W.body.querySelector('.sdlg [role=alertdialog]'),ld:__ld.slice(),calls:__calls.length})");
  ok('9. a scheme no app registered: nothing asked or started, the tab says it could not be loaded (as before)', !d.dlg && d.calls == 0 && d.ld.some(x => /ERR_UNKNOWN_URL_SCHEME/.test(x[1])), d);
  // Settings › Apps › Links
  await q("launch('settings')"); await wait(800);
  await q("(()=>{const W=wins.at(-1);const b=[...W.body.querySelectorAll('button,li,a,div')].find(e=>/^\\s*(📦\\s*)?Apps\\s*$/.test(e.textContent)&&e.onclick);if(b)b.click();return !!b})()"); await wait(500);
  let s = await q("(()=>{let c=document.querySelector('.lkp');if(!c){const x=document.createElement('div');appsPanel(x);document.body.appendChild(x);x.id='lkx';c=x.querySelector('.lkp')}return c?{rows:[...c.querySelectorAll('tr')].map(r=>[r.cells[0].textContent,[...r.querySelectorAll('option')].map(o=>(o.selected?'*':'')+o.textContent)])}:null})()");
  ok('10. Settings › Apps › Links lists each scheme with the apps that open it, the chosen one selected, and "Don\'t open them"',
     s && JSON.stringify(s.rows) == JSON.stringify([['claude: links', ['*Claude (Debian)', 'Don’t open them']], ['discord: links', ['*Discord', 'Don’t open them']], ['teststand: links', ['Other Stand (Debian)', '*Test Stand (Debian)', 'Don’t open them']]]), s);
  await q("(()=>{const sl=document.querySelector('.lkp select[data-scheme=teststand]');sl.value='deb-otherstand';sl.dispatchEvent(new Event('change'));return 1})()");
  await q("__calls.length=0"); await tabev('teststand://again'); await wait(800); await q("__W.body.querySelector('.sdlg .lko').click()"); await wait(1200);
  c = await q("__calls.slice()"); ok('11. another app chosen there: that one gets the links', c.length >= 1 && c[0][0] == 'debian/launch?id=otherstand&link=1' && c[0][1] == 'teststand://again', c);
  await q("(()=>{const sl=document.querySelector('.lkp select[data-scheme=teststand]');sl.value='none';sl.dispatchEvent(new Event('change'));return 1})()");
  await q("__calls.length=0;__ld.length=0"); await tabev('teststand://never'); await wait(1500);
  d = await q("({dlg:!!__W.body.querySelector('.sdlg [role=alertdialog]'),ld:__ld.slice(),calls:__calls.length})");
  ok('12. "Don\'t open them": refused as an unknown scheme is', !d.dlg && d.calls == 0 && d.ld.some(x => /ERR_UNKNOWN_URL_SCHEME/.test(x[1])), d);
  // a Linux app asks the desktop to open another app's link (the portal: rudolf-portal's OpenURI, from xdg-open in Debian)
  await q("__calls.length=0;__portal.push({id:'p1',req:{app:'',kind:'url',uri:'discord://-/invite/abc'}})"); await wait(2500);
  d = await q("(()=>{const o=document.querySelector('.rmodal [role=alertdialog]');return o?{t:o.querySelector('h3').textContent,m:o.querySelector('.sdm').textContent}:null})()");
  ok('13. a Linux app\'s request (the portal) for discord://: "Open Discord?" over the desktop', d && d.t == 'Open Discord?' && /Discord/.test(d.m), d);
  await q("document.querySelector('.rmodal .lko').click()"); await wait(1200);
  c = await q("__calls.slice()"); ok('14. ...Open: Discord is started with it', c.some(x => x[0] == 'apps/run?id=com.discordapp.Discord&desktop=com.discordapp.Discord&link=1' && x[1] == 'discord://-/invite/abc'), c);
  await q("__calls.length=0;window.__nt=[];const n0=notify;notify=function(t,b){__nt.push(t+': '+b);return n0.apply(this,arguments)};__portal.push({id:'p2',req:{app:'',kind:'url',uri:'zzz://nothing'}})"); await wait(2500);
  d = await q("({dlg:!!document.querySelector('.rmodal [role=alertdialog]'),calls:__calls.length,nt:__nt})");
  ok('15. ...one for a scheme no app opens: nothing started, the person is told', !d.dlg && d.calls == 0 && d.nt.some(x => /zzz: link/.test(x)), d);
  ok('16. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
