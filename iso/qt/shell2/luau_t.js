// Luau and Pilot.lua in the page (1.2.5): desktop/luau30.js with lib/luau.wasm, in Node: node luau_t.js
// The Worker is played by a same-thread stand-in (so only the WebAssembly limit stops a long loop here).
const fs = require('fs'), path = require('path'), vm = require('vm');
const ROOT = path.join(__dirname, '../../..'), LIB = path.join(ROOT, 'iso/www/lib');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const ctx = {console, TextEncoder, TextDecoder, WebAssembly, Proxy, BigInt, performance, setTimeout, clearTimeout, Promise, Date, URL};
ctx.fetch = async u => { const f = path.join(LIB, u.replace(/^\/?lib\//, '')); if (!fs.existsSync(f)) return {ok: false, status: 404};
  const b = fs.readFileSync(f); return {ok: true, arrayBuffer: async () => b.buffer.slice(b.byteOffset, b.byteOffset + b.length), text: async () => b.toString()} };
ctx.Blob = class { constructor(p) { this.s = p.join('') } }; ctx.URL = {createObjectURL: b => b};
ctx.Worker = class { constructor(b) { const w = this; this.dead = false;
    const g = {postMessage: m => setTimeout(() => !w.dead && w.onmessage && w.onmessage({data: m}), 0), WebAssembly, TextEncoder, TextDecoder, Proxy, BigInt, performance, Date, Error, console, Uint8Array, DataView, String, Math};
    vm.createContext(g); vm.runInContext(b.s, g); this.g = g }
  postMessage(m) { setTimeout(() => this.g.onmessage({data: m}), 0) } terminate() { this.dead = true } };
ctx.window = ctx; vm.createContext(ctx);
vm.runInContext(fs.readFileSync(path.join(ROOT, 'desktop/luau30.js'), 'utf8') + '\n;this.LUAU=LUAU;', ctx);
const ev = s => vm.runInContext(s, ctx);
(async () => {
  let r = await ev(`luauRun('print("hello from Luau", 1+2)')`);
  ok('runs a Luau program', r.ok && r.code == 0 && r.out == 'hello from Luau\t3\n', r);
  r = await ev(`luauRun('local x: number = 5\\nx += 2\\nlocal s = \`x is {x}\`\\nfor i = 1, 3 do if i == 2 then continue end print(i) end\\nprint(s, if x > 6 then "big" else "small")')`);
  ok('Luau syntax: types, +=, continue, interpolation, if-expressions', r.code == 0 && r.out == '1\n3\nx is 7\tbig\n', r);
  r = await ev(`luauRun('local t = {} \\n print(t.a.b)')`);
  ok('a runtime error is reported with its line', r.code == 1 && /program:2: attempt to index nil/.test(r.out), r.out);
  r = await ev(`luauRun('print(("x"):rep(3)) error("stop here")')`);
  ok('error() stops the program with the message', r.code == 1 && r.out.startsWith('xxx\n') && /stop here/.test(r.out), r.out);
  r = await ev(`luauRun('local x = = 1')`);
  ok('a syntax error is reported', r.code == 1 && /program:1:/.test(r.out), r.out);
  let t0 = Date.now(); r = await ev(`luauRun('while true do end',{limit:1500})`);
  ok('an endless loop is stopped at the time limit', r.code == 124 && r.timeout && /time limit/.test(r.out) && Date.now() - t0 < 4000, [r, Date.now() - t0]);
  r = await ev(`luauRun('print(os.time() > 0, type(io), type(loadstring))')`);
  ok('the sandbox: no io, no loadstring', r.code == 0 && r.out == 'true\tnil\tnil\n', r.out);
  r = await ev(`luauRun('x = 1 print(x) string.upper = nil')`);
  ok('...the program\'s globals work but the libraries are read-only', r.code == 1 && r.out.startsWith('1\n') && /readonly/i.test(r.out), r.out);
  // the checker
  r = await ev(`luauCheck('local x: number = "five"\\nprint(x)')`);
  ok('the checker finds a type error, with its line', r.ok && r.problems.length == 1 && r.problems[0].line == 1 && /string.*number|number.*string/i.test(r.problems[0].msg), r);
  r = await ev(`luauCheck('local function add(a: number, b: number): number return a + b end\\nprint(add(1, 2))')`);
  ok('...and passes a correct program', r.ok && r.problems.length == 0, r);
  r = await ev(`luauCheck('--!strict\\nlocal function f(a) return a.x end\\nprint(f(1))')`);
  ok('...strict mode from --!strict', r.ok && r.problems.length >= 1, r);
  r = await ev(`luauCheck('print(undefinedThing.y)')`);
  ok('...an unknown global', r.ok && r.problems.some(p => /undefinedThing/.test(p.msg)), r);
  // Pilot.lua
  const P1 = `local screen = GetPart("Screen")
local label = screen:CreateElement("TextLabel", { Text = "Hello", TextScaled = true })
local disk = GetPart("Disk")
disk:Write("count", 3)
print("count", disk:Read("count"))
local light = GetPart("Light")
light:SetColor(Color3.fromRGB(255, 0, 0))
local v = Vector3.new(1, 2, 3) + Vector3.new(1, 1, 1)
print(v, v.Magnitude > 4)
task.wait(1)
print(Enum.Material.Plastic)`;
  r = await ev(`luauCheck(${JSON.stringify(P1)},{kind:'pilot'})`);
  ok('Pilot.lua definitions load (Roblox-lite, Enum, Pilot)', r.ok && !r.defError, r.defError || r.error);
  ok('...and a Pilot.lua program checks clean', r.ok && r.problems.length == 0, r.out);
  r = await ev(`luauCheck('local s = GetPart("Screen")\\nif s then s:CreateElement(5) end',{kind:'pilot'})`);
  ok('...a wrong Pilot.lua call is a type error', r.ok && r.problems.length >= 1 && r.problems[0].line == 2, r.out);
  r = await ev(`luauCheck('local s = GetPart("Screen")\\ns:Nonsense()',{kind:'pilot'})`);
  ok('...and a part method that does not exist', r.ok && r.problems.some(p => /Nonsense/.test(p.msg)), r.out);
  r = await ev(`luauRun(${JSON.stringify(P1)},{pilot:true})`);
  ok('a Pilot.lua program runs with stand-in parts', r.code == 0 && /\[Screen 1\] CreateElement\("TextLabel"/.test(r.out) && /count\t3/.test(r.out) && /\[Light 1\] SetColor\(Color3\(1, 0, 0\)\)/.test(r.out) && /Vector3\(2, 3, 4\)\ttrue/.test(r.out) && /Enum\.Material\.Plastic/.test(r.out), r.out);
  r = await ev(`luauRun('local mic = GetPart("Microphone")\\nmic.Chatted:Connect(function(p, m) print(p, "said", m) end)\\nmic.Chatted:Fire("me", "hi")',{pilot:true})`);
  ok('...events connect and fire', r.code == 0 && /me\tsaid\thi/.test(r.out), r.out);
  // Blitzen's two new reference cards: their examples check clean and run
  const packs = new Function(fs.readFileSync(path.join(ROOT, 'desktop/blitzpacks26.js'), 'utf8') + ';return BLZ_PACKS')();
  const ex = k => packs[k].split('\nExample:\n')[1];
  r = await ev(`luauBlitzen(${JSON.stringify(ex('luau'))},'luau')`);
  ok('the Luau card\'s example checks and runs', r.code == 0 && r.out == '2 items cost 5\n', r);
  r = await ev(`luauBlitzen(${JSON.stringify(ex('pilot'))},'pilot')`);
  ok('the Pilot.lua card\'s example checks and runs', r.code == 0 && /\[Screen 1\] ClearElements\(\)/.test(r.out), r);
  r = await ev(`luauBlitzen('local s = GetPart("Screen")\\nif s then s:Nonsense() end','pilot')`);
  ok('a type error is a failure Blitzen can fix (with the line)', r.code == 1 && r.types && /line 2: .*Nonsense/.test(r.out), r.out);
  r = await ev(`luauBlitzen('local Players = game:GetService("Players")\\nlocal n: number = #Players:GetPlayers()\\nprint(n)','luau')`);
  ok('a Roblox game script is checked, not run', r.code == 0 && r.checkedOnly, r);
  r = await ev(`luauBlitzen('local v: Vector3 = Vector3.new(1, 2, 3)\\nlocal x: string = v.X\\nprint(workspace)','luau')`);
  ok('...and its type errors are found', r.code == 1 && /line 2/.test(r.out), r.out);
  // dialects
  ok('dialects: Pilot.lua, Roblox, Luau, Lua', ev(`[luauDialect('local s = GetPart("Screen")'),luauDialect('local p = game:GetService("Players")'),luauDialect('local x: number = 1'),luauDialect('local n = 0\\nn += 1'),luauDialect('print("hi")\\nlocal t = {}')].join()`) == 'pilot,roblox,luau,luau,lua', ev(`[luauDialect('local s = GetPart("Screen")'),luauDialect('local p = game:GetService("Players")'),luauDialect('local x: number = 1'),luauDialect('local n = 0\\nn += 1'),luauDialect('print("hi")\\nlocal t = {}')].join()`));
  // 1.2.7: a checker per kind, loaded and defined once, warmed ahead of time
  let D = JSON.parse(ev('JSON.stringify(LUAU.defines)'));
  ok('1.2.7: switching between Luau, Pilot.lua and Roblox checks loads each kind once', D.luau == 1 && D.pilot == 1 && D.roblox == 1, D);
  ev(`for (const k of Object.keys(LUAU.checkers)) luauDropChecker(k, LUAU.checkers[k]); LUAU.defines = {}`);
  ev(`luauWarm('pilot')`); let t1 = Date.now();
  while (!ev(`!!(LUAU.checkers.pilot && LUAU.checkers.pilot.done)`) && Date.now() - t1 < 120000) await new Promise(r => setTimeout(r, 100));
  const warmMs = Date.now() - t1; t1 = Date.now();
  r = await ev(`luauCheck('local s = GetPart("Screen")\\nprint(s)', {kind: 'pilot'})`);
  const checkMs = Date.now() - t1;
  ok('...a warm-up loads the Pilot.lua checker ahead of time, and the first check then only checks', r.ok && checkMs < 3000 && warmMs > checkMs, {warmMs, checkMs, r});
  r = await ev(`Promise.all([luauCheck('local a: number = "x"', {kind: 'pilot'}), luauCheck('local b: string = "y"', {kind: 'pilot'})])`);
  ok('...two checks at once each get their own answer', r[0].ok && r[0].problems.length == 1 && r[1].ok && r[1].problems.length == 0, r);
  r = await ev(`luauCheck('local n: number = 1', {kind: 'luau'})`);
  D = JSON.parse(ev('JSON.stringify(LUAU.defines)'));
  ok('...and a Luau check meanwhile does not drop the Pilot.lua checker', r.ok && D.pilot == 1 && ev(`!!LUAU.checkers.pilot`), D);
  ev(`LUAU.idleMs = 50; luauIdle('luau', LUAU.checkers.luau)`); await new Promise(r => setTimeout(r, 300));
  ok('...a checker left idle is ended', ev(`!LUAU.checkers.luau && !!LUAU.checkers.pilot`));
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
