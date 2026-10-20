// Blitzen's new languages (1.2.5): Lua, Luau and Pilot.lua in the desktop page, in Chromium. node blz_t.js [CHROME]
// The page is iso/www/index.html (build it first: python3 desktop/build19.py iso/www/index.html) with a stand-in
// rudolfd; the model is a stand-in too: it answers from a script, so the fix loop can be followed exactly.
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html', '.luau': 'text/plain', '.txt': 'text/plain'};
let replies = [], asked = [];
const sse = t => 'data: ' + JSON.stringify({choices: [{delta: {content: t}}]}) + '\n\ndata: [DONE]\n\n';
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]); let body = '';
  q.on('data', c => body += c); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"usb",live:1,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { const a = u.slice(5);
      if (a == 'ai/status') return s.writeHead(200, {'content-type': 'application/json'}), s.end(JSON.stringify({ok: true, installed: true, persistent: true, cpu: 2, model: 'stand-in', running: true}));
      if (a == 'ai/chat') { asked.push(JSON.parse(body)); s.writeHead(200, {'content-type': 'text/event-stream'}); return s.end(sse(replies.shift() || 'No more replies.')) }
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8098, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  p.on('pageerror', e => console.log('page error:', e.message));
  await p.goto('http://127.0.0.1:8098/'); await p.waitForFunction(() => typeof blitzenApp == 'function', null, {timeout: 30000});
  await p.evaluate(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); wins.slice().forEach(w => w.close()) });
  // pure parts first
  ok('the language list', await p.evaluate(() => BLZ_LANGS.map(x => x[1]).join()) == 'Auto,BlockLua,Lua,Luau,Pilot.lua,JavaScript,C++,Python');
  ok('fences: lua, luau, pilot (and its other names)', await p.evaluate(() => ['lua', 'luau', 'pilot', 'pilotlua', 'pilot.lua', 'roblox'].map(blzLang).join()) == 'lua,luau,pilot,pilot,pilot,luau');
  ok('a ```lua block that is Luau or Pilot.lua is run as that', await p.evaluate(() => [blzParts('```lua\nlocal n: number = 1\n```')[0].lang, blzParts('```lua\nlocal s = GetPart("Screen")\n```')[0].lang, blzParts('```lua\nprint(1)\n```')[0].lang, blzParts('```lua\nprint(1)\n```', 'pilot')[0].lang].join()) == 'luau,pilot,lua,pilot');
  ok('packs: lua, luau and pilot cards, each picked for its language', await p.evaluate(() => ['lua', 'luau', 'pilot'].every(k => BLZ_PACKS[k] && BLZ_PACKS[k].split(/\s+/).length < 800) && blzPickPack('', 'a waste of space microcontroller') == 'pilot' && blzPickPack('', 'my roblox game') == 'luau' && blzPickPack('lua', '') == 'lua'));
  // the chat, with the stand-in model: a Pilot.lua answer with a wrong part method, then the fix
  replies = ['Here it is:\n```pilot\nlocal screen = GetPart("Screen")\nif screen then screen:WriteText("hi") end\n```',
    'Fixed:\n```pilot\nlocal screen = GetPart("Screen")\nif screen then\n\tscreen:ClearElements()\n\tscreen:CreateElement("TextLabel", { Text = "hi" })\nend\n```'];
  await p.evaluate(() => { window.__W = blitzenApp(); return 1 }); await wait(1500);
  await p.evaluate(`(async()=>{const W=await __W;W.body.querySelector('.blzlang').value='pilot';W.blz.ask('Show hi on a screen');return 1})()`);
  const done = await p.waitForFunction(() => { const W = wins.find(w => w.el.querySelector('.blzw') || w.body.classList.contains('blzw')); return W && W.body.querySelectorAll('.blzbadge').length >= 2 && /checked/.test(W.body.querySelectorAll('.blzbadge')[1].textContent) }, null, {timeout: 30000}).then(() => true, () => false);
  const st = await p.evaluate(() => { const W = wins.find(w => w.body.classList.contains('blzw')); return {badges: [...W.body.querySelectorAll('.blzbadge')].map(x => x.textContent), langs: [...W.body.querySelectorAll('.blzcl')].map(x => x.textContent), outs: [...W.body.querySelectorAll('.blzres')].map(x => x.textContent)} });
  ok('the first answer fails its type check', done && /failed/.test(st.badges[0]) && /WriteText/.test(st.outs[0]), st);
  ok('...the type error goes back to the model with its line', asked.length == 2 && /line 2: .*WriteText/.test(asked[1].messages.at(-1).content), asked[1] && asked[1].messages.at(-1).content);
  ok('...with the Pilot.lua card', asked.every(a => /PILOT\.LUA \(Waste of Space/.test(a.messages[0].content)));
  ok('...and the fix checks and runs (simulated)', /checked/.test(st.badges[1]) && /simulated/.test(st.outs[1]) && /\[Screen 1\] CreateElement/.test(st.outs[1]) && st.langs.every(l => l == 'Pilot.lua'), st);
  // Luau, through the Run button, and a Roblox script (checked, not run)
  replies = ['```luau\nlocal n = 0\nfor i = 1, 10 do\n\tif i % 2 == 0 then continue end\n\tn += i\nend\nprint(`odd sum {n}`)\n```',
    '```luau\nlocal Players = game:GetService("Players")\nPlayers.PlayerAdded:Connect(function(p) print(p.Name) end)\n```'];
  asked = [];
  await p.evaluate(`(async()=>{const W=await __W;W.body.querySelector('.blzlang').value='luau';W.blz.ask('sum the odd numbers');return 1})()`);
  await p.waitForFunction(() => { const W = wins.find(w => w.body.classList.contains('blzw')); return W.body.querySelectorAll('.blzres').length >= 3 }, null, {timeout: 20000}).catch(() => 0);
  let o = await p.evaluate(() => [...wins.find(w => w.body.classList.contains('blzw')).body.querySelectorAll('.blzres')].map(x => x.textContent).at(-1));
  ok('a Luau answer checks and runs', o == 'odd sum 25' && /LUAU \(Roblox/.test(asked[0].messages[0].content), o);
  await p.evaluate(`(async()=>{const W=await __W;W.blz.ask('a roblox player join script');return 1})()`);
  await p.waitForFunction(() => { const W = wins.find(w => w.body.classList.contains('blzw')); return W.body.querySelectorAll('.blzres').length >= 4 }, null, {timeout: 20000}).catch(() => 0);
  const last = await p.evaluate(() => { const W = wins.find(w => w.body.classList.contains('blzw')); return [[...W.body.querySelectorAll('.blzbadge')].at(-1).textContent, [...W.body.querySelectorAll('.blzres')].at(-1).textContent] });
  ok('a Roblox script is checked, not run', /types checked/.test(last[0]) && /runs only in Roblox/.test(last[1]), last);
  // Lua (Fengari), as before
  replies = ['```lua\nlocal t = {}\nfor w in ("b a c"):gmatch("%a") do t[#t+1] = w end\ntable.sort(t)\nprint(table.concat(t, ","))\n```'];
  await p.evaluate(`(async()=>{const W=await __W;W.body.querySelector('.blzlang').value='lua';W.blz.ask('sort letters');return 1})()`);
  await p.waitForFunction(() => { const W = wins.find(w => w.body.classList.contains('blzw')); return W.body.querySelectorAll('.blzres').length >= 5 }, null, {timeout: 20000}).catch(() => 0);
  o = await p.evaluate(() => [...wins.find(w => w.body.classList.contains('blzw')).body.querySelectorAll('.blzres')].map(x => x.textContent).at(-1));
  ok('a Lua answer runs in Fengari, with the Lua card', o == 'a,b,c' && /LUA 5\.3 ON RUDOLF OS/.test(asked.at(-1).messages[0].content), o);
  // Code Editor: .LUAU files
  ok('Code Editor: .LUAU is Luau', await p.evaluate(() => codeLang('GAME.LUAU') == 'luau'));
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
