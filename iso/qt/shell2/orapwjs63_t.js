// 1.6.3 (docs/plan-1.6.3.md Part 7, step 1): Oracle's password scripts in a real Chromium page, each in an isolated world as the shell
// and rudolf-webkit run them: the shell's (iso/shell/shell.js PW_JS, PW_FILL, pwSent) and Lite's (iso/sys/rudolf-webkit.c PW_JS,
// PW_FILL_FN), read from their sources. Test sites served by the test itself (https://shop.example with a frame of
// https://ads.example, http://plain.example). Made-up passwords only.
//   the page's own scripts cannot see the script or its pad; a password field with the keyboard is told (a new password said);
//   a sign-in sent (its button, Enter) is told once, under the pad (the console message holds neither the password nor the user
//   name); a show-password button, typing, and events made by the page's script are not; never on http:, never in a frame (only
//   the top frame); the fill goes only into a page still at its origin; the shell undoes the pad and tells the desktop only for
//   the tab's own https: origin, never for a private window's tab; Lite's the same, by WebKit's own message.
//   node orapwjs63_t.js [CHROME]
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path');
const REPO = path.join(__dirname, '../../..');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const wait = ms => new Promise(r => setTimeout(r, ms));
// ---- the scripts, from their sources
const SH = fs.readFileSync(path.join(REPO, 'iso/shell/shell.js'), 'utf8');
const NONCE = 'rudolfTEST123';
const PW_JS = eval(SH.match(/const PW_JS = (`[\s\S]*?`);\n/)[1]);
const PW_FILL = eval('(' + SH.match(/const PW_FILL = (o => \{[\s\S]*?return 'ok' \});\n/)[1] + ')');
const WK = fs.readFileSync(path.join(REPO, 'iso/sys/rudolf-webkit.c'), 'utf8');
const cs = name => { const m = WK.match(new RegExp('static const char \\*' + name + ' =\\n((?:\\t"(?:[^"\\\\]|\\\\.)*"\\n?)+);')); return m[1].split('\n').map(l => JSON.parse(l.trim())).join('') };
const LITE_JS = cs('PW_JS'), LITE_FILL = cs('PW_FILL_FN');
// the shell's own side: pwSent and pwFocus with a stand-in tab and toTab
const shellSide = new Function('toTab', SH.match(/const pwPrivate = [\s\S]*?\nfunction pwFocus[^\n]*\n/)[0] + '; return {pwSent, pwFocus, pwPad};');
const LOGIN = `<!doctype html><meta charset=utf-8><title>Shop sign-in</title><form id=f action="/done" onsubmit="event.preventDefault();window.__submits=(window.__submits||0)+1">
<input id=u name=user autocomplete=username><input id=p type=password name=pw autocomplete=current-password><button type=button id=eye>👁</button><button id=go>Sign in</button></form>
<iframe src="https://ads.example/frame" width=300 height=120></iframe>`;
const SIGNUP = `<!doctype html><meta charset=utf-8><title>Sign up</title><form><input id=u type=email><input id=n1 type=password autocomplete=new-password><input id=n2 type=password autocomplete=new-password><button>Create</button></form>`;
const FRAME = `<!doctype html><meta charset=utf-8><form><input id=fu><input id=fp type=password></form>`;
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const ctx = await b.newContext({ignoreHTTPSErrors: true});
  await ctx.route('**/*', r => { const u = r.request().url();
    const body = /^https:\/\/shop\.example\/login/.test(u) ? LOGIN : /^https:\/\/shop\.example\/signup/.test(u) ? SIGNUP : /^https:\/\/ads\.example\/frame/.test(u) ? FRAME : /^http:\/\/plain\.example/.test(u) ? LOGIN.replace(/<iframe[^>]*><\/iframe>/, '') : null;
    return body ? r.fulfill({status: 200, contentType: 'text/html', body}) : r.fulfill({status: 404, body: ''}) });
  async function world(p) {
    const c = await ctx.newCDPSession(p); await c.send('Runtime.enable'); await c.send('Page.enable');
    const msgs = []; c.on('Runtime.consoleAPICalled', e => msgs.push((e.args || []).map(a => a.value).join(' ')));
    const {frameTree} = await c.send('Page.getFrameTree');
    const mk = async fid => (await c.send('Page.createIsolatedWorld', {frameId: fid, worldName: 'rudolf'})).executionContextId;
    const top = await mk(frameTree.frame.id), sub = frameTree.childFrames && frameTree.childFrames[0] ? await mk(frameTree.childFrames[0].frame.id) : null;
    const run = async (ctxId, expr) => { const r = await c.send('Runtime.evaluate', {expression: expr, contextId: ctxId, returnByValue: true, awaitPromise: true}); return r.result.value };
    return {msgs, top, sub, run};
  }
  const said = (msgs, pre) => msgs.filter(m => m.startsWith(pre)).map(m => { try { return JSON.parse(m.slice(pre.length)) } catch (e) { return null } });
  // ---- the shell's script
  {
    const p = await ctx.newPage(); await p.goto('https://shop.example/login'); await wait(300);
    const W = await world(p); const pad = Array.from({length: 1024}, (_, i) => (i * 73 + 41) & 255);
    await W.run(W.top, 'window.__rudolfPwPad=new Uint8Array(' + JSON.stringify(pad) + ');' + PW_JS);
    ok('1. the page\'s own scripts cannot see the script or its pad (an isolated world)', await p.evaluate(() => window.__rudolfPw === undefined && window.__rudolfPwPad === undefined) && await W.run(W.top, '!!window.__rudolfPw&&!!window.__rudolfPwPad'));
    await p.click('#p'); await wait(200);
    ok('2. a password field with the keyboard is told ({pwf:1})', said(W.msgs, NONCE).some(o => o && o.pwf === 1), W.msgs);
    await p.click('#u'); await p.keyboard.type('ann@example.com'); await p.click('#p'); await p.keyboard.type('Made-up-Pass-1!');
    await p.click('#eye'); await wait(200);
    ok('3. typing and a show-password button are not a sign-in: nothing sent', !said(W.msgs, NONCE).some(o => o && o.pws));
    await p.evaluate(() => { const e = document.getElementById('p'); e.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter', bubbles: true})); document.getElementById('eye').dispatchEvent(new MouseEvent('click', {bubbles: true})) }); await wait(200);
    ok('4. keys and clicks made by the page\'s own script are not either (a form the page submits itself still is: it only makes Oracle ask)', !said(W.msgs, NONCE).some(o => o && o.pws));
    await p.click('#go'); await wait(300);
    const S1 = said(W.msgs, NONCE).filter(o => o && o.pws);
    ok('5. the sign-in sent (its button) is told once, under the pad: the console message holds neither the password nor the user name', S1.length == 1 && !W.msgs.some(m => /Made-up|ann@example/.test(m)), W.msgs);
    const hex = S1[0] && S1[0].pws, dec = hex ? Buffer.from(hex.match(/../g).map((h, i) => parseInt(h, 16) ^ pad[i])).toString() : '';
    ok('...which the pad undoes: ["ann@example.com","Made-up-Pass-1!"]', dec == '["ann@example.com","Made-up-Pass-1!"]', dec);
    ok('...and the pad is used once: gone from the page\'s world after', await W.run(W.top, 'window.__rudolfPwPad===null'));
    await p.click('#go'); await p.focus('#p'); await p.keyboard.press('Enter'); await wait(300);
    ok('6. sent again without a new pad (or the same again): nothing more', said(W.msgs, NONCE).filter(o => o && o.pws).length == 1);
    // the shell's side: the pad undone, the origin the tab's own
    const told = []; const side = shellSide((tid, ev, o) => told.push([tid, ev, o]));
    const tab = (url, part) => ({tid: 'o1', url, part: part || 'a1', pad: Uint8Array.from(pad), f: {executeScript: async () => []}});
    side.pwSent(tab('https://shop.example/login?x=1'), hex);
    ok('7. the shell undoes the pad and tells the desktop the tab\'s own origin (from its address, not from the page)', told.length == 1 && told[0][1] == 'pwsave' && told[0][2].origin == 'https://shop.example' && told[0][2].user == 'ann@example.com' && told[0][2].pw == 'Made-up-Pass-1!', told);
    told.length = 0; side.pwSent(tab('http://shop.example/login'), hex); side.pwSent(tab('https://shop.example/login', 'priv-a1-x'), hex); side.pwSent(Object.assign(tab('https://shop.example/'), {pad: null}), hex); side.pwSent(tab('https://shop.example/'), hex.slice(2)); side.pwSent(tab('https://shop.example/'), 'zz' + hex.slice(2));
    ok('...never for an http: tab, a private window\'s tab, without its pad, or a message changed', told.length == 0, told);
    side.pwFocus(tab('https://shop.example/a'), 2); side.pwFocus(tab('http://shop.example/a'), 1); side.pwFocus(tab('https://shop.example/a', 'priv-x'), 1);
    ok('...a password field told for the tab\'s https: origin only (a new password said)', told.length == 1 && told[0][1] == 'pwfocus' && told[0][2].origin == 'https://shop.example' && told[0][2].newpw === true, told);
    // never in a frame
    if (W.sub) { await W.run(W.sub, PW_JS); }
    ok('8. never in a frame (the frame of ads.example): the script does nothing there', W.sub && await W.run(W.sub, 'window.__rudolfPw===undefined'));
    // the fill
    await p.evaluate(() => { document.getElementById('u').value = ''; document.getElementById('p').value = '' });
    const r0 = await W.run(W.top, '(' + PW_FILL.toString() + ')(' + JSON.stringify({o: 'https://evil.example', u: 'ann', p: 'Made-up-X'}) + ')');
    ok('9. the fill refuses a page at another origin (nothing filled)', r0 == 'origin' && await p.evaluate(() => document.getElementById('p').value == ''), r0);
    const r1 = await W.run(W.top, '(' + PW_FILL.toString() + ')(' + JSON.stringify({o: 'https://shop.example', u: 'ann@example.com', p: 'Made-up-Pass-2'}) + ')');
    ok('10. ...and fills the page at its origin: the password and the user name', r1 == 'ok' && await p.evaluate(() => document.getElementById('p').value == 'Made-up-Pass-2' && document.getElementById('u').value == 'ann@example.com'), r1);
    const fr = p.frames().find(f => /ads\.example/.test(f.url()));
    ok('...never the frame of another site', fr && await fr.evaluate(() => document.getElementById('fp').value == ''));
    // a new password
    await p.goto('https://shop.example/signup'); await wait(300); const W2 = await world(p);
    await W2.run(W2.top, 'window.__rudolfPwPad=new Uint8Array(' + JSON.stringify(pad) + ');' + PW_JS); await p.click('#n1'); await wait(200);
    ok('11. a new password\'s field is told as one ({pwf:2})', said(W2.msgs, NONCE).some(o => o && o.pwf === 2), W2.msgs);
    const r2 = await W2.run(W2.top, '(' + PW_FILL.toString() + ')(' + JSON.stringify({o: 'https://shop.example', u: '', p: 'Strong-Made-Up-20ch!', g: true}) + ')');
    ok('...a strong password goes into both new-password fields', r2 == 'ok' && await p.evaluate(() => document.getElementById('n1').value == 'Strong-Made-Up-20ch!' && document.getElementById('n2').value == 'Strong-Made-Up-20ch!'), r2);
    // http
    await p.goto('http://plain.example/login'); await wait(300); const W3 = await world(p);
    await W3.run(W3.top, 'window.__rudolfPwPad=new Uint8Array(4);' + PW_JS);
    ok('12. never on an http: page', await W3.run(W3.top, 'window.__rudolfPw===undefined'));
    await p.close();
  }
  // ---- Lite's script (rudolf-webkit's, WebKit's own message stood in for by the isolated world's console)
  {
    const p = await ctx.newPage(); await p.goto('https://shop.example/login'); await wait(300); const W = await world(p);
    await W.run(W.top, "window.webkit={messageHandlers:{rudolfTab:{postMessage:o=>console.debug('LITE'+JSON.stringify(o))}}};" + LITE_JS);
    ok('13. Lite: the page\'s own scripts cannot see it', await p.evaluate(() => window.__rudolfPw === undefined));
    await p.click('#p'); await p.keyboard.type('Made-up-Pass-3'); await p.click('#u'); await p.keyboard.type('bob'); await p.click('#eye'); await wait(200);
    ok('14. Lite: a password field told; typing and the show-password button are not a sign-in', said(W.msgs, 'LITE').some(o => o && o.pwf === 1) && !said(W.msgs, 'LITE').some(o => o && o.pws));
    await p.click('#go'); await wait(300);
    const S = said(W.msgs, 'LITE').filter(o => o && o.pws);
    ok('15. Lite: the sign-in sent, once, to rudolf-webkit (WebKit\'s message to its own process): ["bob","Made-up-Pass-3"]', S.length == 1 && JSON.stringify(S[0].pws) == '["bob","Made-up-Pass-3"]', S);
    const r = await W.run(W.top, '(' + LITE_FILL + ')(' + JSON.stringify({o: 'https://evil.example', u: '', p: 'X'}) + ')'), r2 = await W.run(W.top, '(' + LITE_FILL + ')(' + JSON.stringify({o: 'https://shop.example', u: '', p: 'Made-up-Pass-4'}) + ')');
    ok('16. Lite: the fill refuses another origin and fills its own', r == 'origin' && r2 == 'ok' && await p.evaluate(() => document.getElementById('p').value == 'Made-up-Pass-4'), [r, r2]);
    if (W.sub) await W.run(W.sub, "window.webkit={messageHandlers:{rudolfTab:{postMessage:()=>{}}}};" + LITE_JS);
    ok('17. Lite: never in a frame', W.sub && await W.run(W.sub, 'window.__rudolfPw===undefined'));
    await p.close();
  }
  ok('18. rudolf-webkit adds the script only to tabs that are not a private window\'s or Guest\'s, in its "rudolf" world, top frame', /if \(strncmp\(part, "priv-", 5\)\) webkit_user_content_manager_add_script\(ucm, webkit_user_script_new_for_world\(PW_JS, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, "rudolf"/.test(WK));
  ok('19. the shell gives the script only to https: pages of tabs not a private window\'s, with a new pad each page', /function pwPage\(t, url\) \{ if \(pwPrivate\(t\) \|\| !\/\^https:\/i\.test\(url \|\| ''\)\) \{ t\.pad = null; return \} t\.f\.executeScript\(\{code: pwPad\(t\) \+ PW_JS\}\)/.test(SH) && /pwPage\(t, e\.url\)/.test(SH));
  await b.close();
  const fl = R.filter(x => !x).length; console.log(fl ? `FAIL ${fl} of ${R.length}` : `PASS ${R.length}/${R.length}`); process.exit(fl ? 1 : 0);
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
