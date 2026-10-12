/* Blitzen for Windows: the chat page. GPL-2.0-or-later.
   Served by llama-server (--path, Stage 1) it talks to the same address; inside the Blitzen app (Stage 2, Tauri)
   it asks the app for the engine's port and key, and runs Python and C++ through the app's sandbox.
   Shared with Rudolf OS: the context packs (blitzpacks26.js) and the Lua-to-blocks converter (blk2lua26.js). */
'use strict';
const TAURI = window.__TAURI__ || null;
const invoke = TAURI ? TAURI.core.invoke : null;
const $ = s => document.querySelector(s);
const esc = s => String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const store = {get(k, d) { try { const v = localStorage.getItem('blz:' + k); return v == null ? d : JSON.parse(v) } catch (e) { return d } },
  set(k, v) { try { localStorage.setItem('blz:' + k, JSON.stringify(v)) } catch (e) {} }};

/* ---------- Windows versions of the packs that describe the system ---------- */
const SYSTEM_WIN = BLZ_SYSTEM.replace('the assistant built into Rudolf OS, running offline on this computer', 'an offline assistant running on this Windows PC').replace(/Never invent Rudolf OS functions/, 'Never invent library functions');
BLZ_PACKS.python = `PYTHON ON WINDOWS
Programs run with Python 3 (python -I main.py), with no input, no network and a 10-second limit when Blitzen checks them.
Use the standard library (os, pathlib, csv, json, re, datetime, math, random, collections, itertools, shutil). Windows paths: use pathlib, e.g. Path.home() / "Documents". Other packages: pip install NAME in a terminal (Blitzen's checks use only the standard library).
Read input with input() only when asked. Print results with print().
Example:
from pathlib import Path
for f in sorted((Path.home() / "Documents").glob("*.txt")):
    print(f.name, f.stat().st_size, "bytes")`;
BLZ_PACKS.cpp = `C++ ON WINDOWS
Programs are compiled with a C++17 compiler (MSVC cl /std:c++17 /EHsc, or clang++/g++ -std=c++17) and run with no input, no network and a 10-second limit when Blitzen checks them.
Write one complete file with int main(). Use only the standard library. Read input with std::cin only when asked; print with std::cout.
Example:
#include <iostream>
#include <vector>
int main() {
    std::vector<int> v{3, 1, 2};
    int sum = 0;
    for (int x : v) sum += x;
    std::cout << "sum = " << sum << "\\n";
}`;
BLZ_PACKS.js = `JAVASCRIPT ON WINDOWS
Plain JavaScript (ES2022) is checked in a Web Worker: console.log output is shown; there is no page (no document, no window), no network and a 10-second limit.
For Node.js scripts (run with: node file.js) use require('fs'), require('path') and process.argv; they are not checked here because the Worker has no file access, so say so.
For a web page, write one complete HTML file with its <script> inline; it is saved as .html and opened in the browser.
Example:
const names = ['Mia', 'alex', 'Zoe'];
names.sort((a, b) => a.localeCompare(b, undefined, {sensitivity: 'base'}));
console.log(names.join(', '));`;
BLZ_PACKS.help = `BLITZEN ON WINDOWS
Blitzen runs on this PC with a local model (llama.cpp) and needs no internet after setup; nothing typed is sent anywhere. It checks code by running it: JavaScript and BlockLua in the page, Python and C++ in the Blitzen app's sandbox.`;
BLZ_PACKS.blocklua = BLZ_PACKS.blocklua + `
On Windows, Blitzen runs BlockLua with plain Lua 5.3: print, io.write and the standard libraries work; sys, gl and quake calls only work on Rudolf OS (here they print what they would do).`;

/* ---------- the engine ---------- */
let ENG = null;   // {base, key}
async function engine() {
  if (!TAURI) return {base: '', key: null};
  ENG = await invoke('engine_ensure');   // starts it if needed: {port, key}
  return {base: 'http://127.0.0.1:' + ENG.port, key: ENG.key};
}
async function stream(messages, opt, onText, signal) {
  const e = await engine();
  const body = {messages, stream: true, temperature: opt.temperature ?? 0.5, top_p: 0.9, max_tokens: opt.max || 2048, cache_prompt: true};
  if (opt.grammar) body.grammar = opt.grammar;
  const headers = {'Content-Type': 'application/json'};
  if (e.key) headers.Authorization = 'Bearer ' + e.key;
  const r = await fetch(e.base + '/v1/chat/completions', {method: 'POST', headers, body: JSON.stringify(body), signal});
  if (!r.ok) { let m = 'Blitzen did not answer (HTTP ' + r.status + ')'; try { const j = await r.json(); m = (j.error && j.error.message) || m } catch (x) {} throw new Error(m) }
  const rd = r.body.getReader(), dec = new TextDecoder(); let buf = '', text = '', n = 0, first = 0; const t0 = performance.now();
  for (;;) {
    const {done, value} = await rd.read(); if (done) break; buf += dec.decode(value, {stream: true}); let i;
    while ((i = buf.indexOf('\n')) >= 0) {
      const l = buf.slice(0, i).trim(); buf = buf.slice(i + 1); if (!l.startsWith('data:')) continue;
      const d = l.slice(5).trim(); if (d == '[DONE]') continue; let j; try { j = JSON.parse(d) } catch (x) { continue }
      if (j.error) throw new Error(j.error.message || 'Blitzen stopped');
      const c = j.choices && j.choices[0] && j.choices[0].delta && j.choices[0].delta.content;
      if (c) { if (!first) first = performance.now(); text += c; n++; onText(text) }
    }
  }
  const end = performance.now();
  return {text, tokens: n, ms: end - t0, first: first ? first - t0 : end - t0, tps: n > 1 && first ? (n - 1) / ((end - first) / 1000) : 0};
}

/* ---------- answers ---------- */
const LNAME = {lua: 'BlockLua / Lua', js: 'JavaScript', cpp: 'C++', python: 'Python', html: 'HTML', text: 'Text'};
function langOf(l) { l = (l || '').toLowerCase(); return /^(lua|blocklua)$/.test(l) ? 'lua' : /^(js|javascript|mjs|node)$/.test(l) ? 'js' : /^(cpp|c\+\+|cc|cxx|hpp|c)$/.test(l) ? 'cpp' : /^(py|python|python3)$/.test(l) ? 'python' : /^html?$/.test(l) ? 'html' : l || 'text' }
function parts(text) {
  const out = [], re = /```([\w+#.-]*)[^\n]*\n([\s\S]*?)(```|$)/g; let last = 0, m;
  while ((m = re.exec(text))) { if (m.index > last) out.push({t: 'text', s: text.slice(last, m.index)}); out.push({t: 'code', lang: langOf(m[1]), s: m[2].replace(/\n$/, ''), done: m[3] == '```'}); last = re.lastIndex; if (!m[0].length) break }
  if (last < text.length) out.push({t: 'text', s: text.slice(last)}); return out;
}
const md = s => esc(s).replace(/`([^`\n]+)`/g, '<code>$1</code>').replace(/\*\*([^*\n]+)\*\*/g, '<b>$1</b>').replace(/^#{1,4} (.*)$/gm, '<b>$1</b>').replace(/^[-*] /gm, '• ').replace(/\n/g, '<br>');
function needsInput(lang, code) { return lang == 'lua' ? /io\.read|sys\.input/.test(code) : lang == 'python' ? /\binput\s*\(|sys\.stdin/.test(code) : lang == 'cpp' ? /std::cin|\bcin\s*>>|getline\s*\(\s*(std::)?cin|scanf/.test(code) : lang == 'js' ? /\bprompt\s*\(/.test(code) : false }

/* ---------- running code ---------- */
async function runLua(code) {
  if (!window.fengari) return {ok: false, error: 'The Lua runtime is missing.'};
  const {lua, lauxlib} = fengari, L = fengari.L, t0 = Date.now();
  fengari.load(`__blz_out = {}
local function put(s) __blz_out[#__blz_out + 1] = s end
print = function(...) local t = {} for i = 1, select('#', ...) do t[i] = tostring((select(i, ...))) end put(table.concat(t, '\\t') .. '\\n') end
io = io or {}
io.write = function(...) for i = 1, select('#', ...) do put(tostring((select(i, ...)))) end end
io.read = function() return nil end
sys = setmetatable({}, {__index = function(_, k) return function(...) put('[sys.' .. k .. ' works on Rudolf OS]\\n') end end})
gl, quake = sys, sys`)();
  let err = null;
  try { lua.lua_sethook(L, L2 => { if (Date.now() - t0 > 10000) lauxlib.luaL_error(L2, fengari.to_luastring('time limit: the program ran for more than 10 seconds')) }, lua.LUA_MASKCOUNT, 100000) } catch (e) {}
  try { fengari.load(String(code), '=main.lua')() } catch (e) { err = String(e && e.message || e) } finally { try { lua.lua_sethook(L, null, 0, 0) } catch (e) {} }
  let out = ''; try { out = fengari.load('return table.concat(__blz_out)')() || '' } catch (e) {}
  return {ok: true, code: err ? 1 : 0, timeout: /time limit/.test(err || ''), out: out + (err ? err + '\n' : '')};
}
function runJS(code) {
  return new Promise(res => {
    const src = `const __o=[];const __s=v=>typeof v=='string'?v:(()=>{try{return JSON.stringify(v)}catch(e){return String(v)}})();
console.log=console.info=console.warn=console.error=(...a)=>__o.push(a.map(__s).join(' '));self.fetch=undefined;self.XMLHttpRequest=undefined;self.WebSocket=undefined;self.importScripts=undefined;
onmessage=async e=>{let err=null;try{await (0,eval)('(async()=>{'+e.data+'\\n})()')}catch(x){err=(x&&x.name?x.name+': ':'')+(x&&x.message||x)}postMessage({out:__o.join('\\n'),err})}`;
    const w = new Worker(URL.createObjectURL(new Blob([src], {type: 'text/javascript'})));
    const tm = setTimeout(() => { w.terminate(); res({ok: true, code: 124, timeout: true, out: 'time limit: the program ran for more than 10 seconds\n'}) }, 10000);
    w.onmessage = e => { clearTimeout(tm); w.terminate(); res({ok: true, code: e.data.err ? 1 : 0, out: (e.data.out ? e.data.out + '\n' : '') + (e.data.err ? e.data.err + '\n' : '')}) };
    w.onerror = e => { clearTimeout(tm); w.terminate(); res({ok: true, code: 1, out: (e.message || 'error') + '\n'}) };
    w.postMessage(code);
  });
}
async function run(lang, code) {
  if (lang == 'lua') return runLua(code);
  if (lang == 'js') return runJS(code);
  if (lang == 'python' || lang == 'cpp') {
    if (!TAURI) return {ok: false, error: (lang == 'python' ? 'Python' : 'C++') + ' answers are checked in the Blitzen app (Stage 2); this page checks JavaScript and BlockLua.'};
    try { return await invoke('run_code', {lang, code}) } catch (e) { return {ok: false, error: String(e)} }
  }
  return {ok: false, error: 'Blitzen does not run ' + (LNAME[lang] || lang) + ' here.'};
}

/* ---------- chats (Documents\Blitzen in the app, this browser's storage otherwise) ---------- */
const chat = {msgs: [], name: null};
async function saveChat() {
  if (!chat.msgs.length) return;
  if (!chat.name) { const t = new Date(), p = n => String(n).padStart(2, '0'); chat.name = `Chat ${t.getFullYear()}-${p(t.getMonth() + 1)}-${p(t.getDate())} ${p(t.getHours())}${p(t.getMinutes())}${p(t.getSeconds())}` }
  const text = 'Blitzen chat\n' + chat.msgs.map(m => `### ${m.role == 'user' ? 'You' : 'Blitzen'}\n${m.content}`).join('\n') + '\n';
  if (TAURI) { try { await invoke('chat_save', {name: chat.name, text}) } catch (e) {} return }
  const all = store.get('chats', {}); all[chat.name] = text; const names = Object.keys(all).sort(); while (names.length > 50) delete all[names.shift()]; store.set('chats', all);
}
async function listChats() { if (TAURI) { try { return await invoke('chat_list') } catch (e) { return [] } } return Object.keys(store.get('chats', {})).sort().reverse() }
async function loadChat(name) {
  const t = TAURI ? await invoke('chat_load', {name}).catch(() => '') : store.get('chats', {})[name] || '';
  const msgs = []; t.split(/^### (You|Blitzen)\n/m).slice(1).forEach((x, i, a) => { if (i % 2 == 0) msgs.push({role: x == 'You' ? 'user' : 'assistant', content: (a[i + 1] || '').replace(/\n$/, '')}) });
  return msgs;
}

/* ---------- the page ---------- */
const log = $('#log'), ta = $('#q'), st = $('#st'), langSel = $('#lang'), check = $('#check');
let busy = false, abort = null; const RETRIES = 2;
check.checked = store.get('check', true); check.onchange = () => store.set('check', check.checked);
const scroll = () => { log.scrollTop = log.scrollHeight };
function bubble(role, label) { const d = document.createElement('div'); d.className = 'm ' + (role == 'user' ? 'u' : 'a'); d.innerHTML = `<div class='who'>${esc(label || (role == 'user' ? 'You' : '⚡ Blitzen'))}</div><div class='body'></div>`; log.appendChild(d); scroll(); return d }
function hello() {
  log.innerHTML = `<div class='hello'><b>⚡ Hi, I'm Blitzen.</b><br>I run on this PC and help with BlockLua, JavaScript, C++ and Python, or anything else you want to talk about.
  <div class='sugg'>${['Write a Python script that lists the largest files in Downloads', 'Explain what a C++ vector is with a short example', 'Write JavaScript that sorts a list of names', 'Make BlockLua that counts down from 10'].map(s => `<button>${esc(s)}</button>`).join('')}</div></div>`;
  log.querySelectorAll('.sugg button').forEach(b => b.onclick = () => { ta.value = b.textContent; ta.focus() });
}
function badge(c, kind, t) { const b = c.querySelector('.badge'); b.className = 'badge ' + kind; b.textContent = t }
function showOut(c, res) {
  const o = c.querySelector('.out'); o.hidden = false; o.innerHTML = '';
  if (!res.ok) {
    o.innerHTML = `<div>${esc(res.error || 'Could not run it.')}</div>`;
    if (TAURI && (res.missing == 'python' || res.missing == 'cpp')) { const b = document.createElement('button'); b.textContent = res.missing == 'python' ? 'Set up Python (about 25 MB)' : 'Set up the C++ compiler (about 150 MB)'; b.onclick = async () => { b.disabled = true; b.textContent = 'Setting up…'; try { await invoke('runner_setup', {lang: res.missing}); b.textContent = 'Ready: press ▶ Run again' } catch (e) { b.textContent = String(e) } }; o.appendChild(b) }
    return;
  }
  const pre = document.createElement('pre'); if (res.code) pre.className = 'bad'; pre.textContent = (res.out || '').trim() || (res.code ? '(no output)' : '(finished with no output)'); o.appendChild(pre);
}
async function runBlock(c, auto) {
  const lang = c.dataset.lang, code = c.code;
  if (!['lua', 'js', 'python', 'cpp'].includes(lang)) { badge(c, 'nr', 'not run'); return null }
  if (auto && needsInput(lang, code)) { badge(c, 'nr', 'not run: it asks for input'); return null }
  if (auto && lang == 'js' && /\brequire\s*\(|\bprocess\.(argv|env|exit|stdin)|^\s*import\s.+\sfrom\s/m.test(code)) { badge(c, 'nr', 'not run: Node.js script'); return null }
  if (auto && !TAURI && (lang == 'python' || lang == 'cpp')) { badge(c, 'nr', 'not run: needs the app'); return null }
  badge(c, 'run', 'running…'); const res = await run(lang, code); showOut(c, res);
  if (!res.ok) { badge(c, 'nr', 'not run'); return null }
  badge(c, res.code ? 'bad' : 'ok', res.code ? (res.timeout ? 'stopped: too slow' : 'failed') : '✓ checked'); return res;
}
function draw(el, text, o = {}) {
  const body = el.querySelector('.body'); body.innerHTML = '';
  (o.raw ? [{t: 'code', lang: 'lua', s: text, done: !o.streaming}] : parts(text)).forEach(p => {
    if (p.t == 'text') { const d = document.createElement('div'); d.className = 'txt'; d.innerHTML = md(p.s); body.appendChild(d); return }
    const c = document.createElement('div'); c.className = 'code'; c.dataset.lang = p.lang; c.code = p.s;
    c.innerHTML = `<div class='ch'><b>${esc(LNAME[p.lang] || p.lang)}</b><span class='badge'></span><span class='grow'></span>${p.done ? `<button class='r'>▶ Run</button><button class='cp'>Copy</button><button class='sv'>Save…</button>` : ''}</div><pre class='src'></pre><div class='out' hidden></div>`;
    c.querySelector('pre').textContent = p.s; body.appendChild(c);
    if (p.done) {
      c.querySelector('.r').onclick = () => runBlock(c, false);
      c.querySelector('.cp').onclick = () => navigator.clipboard.writeText(p.s).then(() => { st.textContent = 'Copied.' }).catch(() => {});
      c.querySelector('.sv').onclick = () => saveFile(p.lang, p.s);
    }
  });
  scroll();
}
async function saveFile(lang, code) {
  const ext = {lua: 'lua', js: 'js', python: 'py', cpp: 'cpp', html: 'html'}[lang] || 'txt';
  if (TAURI) { try { const p = await invoke('save_file', {ext, text: code}); if (p) st.textContent = 'Saved to ' + p } catch (e) { st.textContent = String(e) } return }
  const a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([code], {type: 'text/plain'})); a.download = 'blitzen.' + ext; a.click();
}
function setBusy(b) { busy = b; $('#stop').hidden = !b; $('#go').disabled = b }
async function answer(label, o = {}) {
  const last = chat.msgs.filter(m => m.role == 'user').at(-1);
  const pack = o.pack || blzPickPack(langSel.value || null, (last && last.content) || '');
  const sys = SYSTEM_WIN + (pack ? '\n\n' + BLZ_PACKS[pack] : '') + (o.blocks ? '\n\nAnswer with Lua only, one block-shaped statement per line, no explanation and no code fence.' : '');
  const el = bubble('assistant', label); el.querySelector('.body').textContent = '…';
  abort = new AbortController(); setBusy(true); st.textContent = 'Thinking…'; let res;
  try { res = await stream([{role: 'system', content: sys}, ...chat.msgs.slice(-12)], {grammar: o.blocks ? BLZ_BLOCK_GRAMMAR : null, temperature: pack && pack != 'help' ? 0.4 : 0.7}, t => draw(el, t, {raw: o.blocks, streaming: true}), abort.signal) }
  catch (e) {
    setBusy(false);
    if (e.name == 'AbortError') { st.textContent = 'Stopped.'; const t = el.querySelector('.body').textContent; if (t && t != '…') chat.msgs.push({role: 'assistant', content: t}); else el.remove(); return null }
    el.querySelector('.body').innerHTML = `<div class='err'>${esc(e.message)}</div>`; st.textContent = ''; return null;
  }
  setBusy(false); const text = o.blocks ? '```lua\n' + res.text.trim() + '\n```' : res.text;
  chat.msgs.push({role: 'assistant', content: text}); draw(el, text); saveChat();
  st.textContent = res.tokens ? `${res.tokens} tokens in ${(res.ms / 1000).toFixed(1)} s · ${res.tps ? res.tps.toFixed(1) : '–'} tokens per second · first word after ${(res.first / 1000).toFixed(1)} s` : '';
  store.set('speed', {tps: res.tps, at: Date.now()}); return el;
}
async function checkAnswer(el, tries) {
  if (!check.checked || !el) return;
  const c = [...el.querySelectorAll('.code')].find(x => ['lua', 'js', 'python', 'cpp'].includes(x.dataset.lang)); if (!c) return;
  const res = await runBlock(c, true); if (!res || !res.code) return;
  if (tries >= RETRIES) { st.textContent = 'The code still fails after ' + RETRIES + ' fixes; you can edit it or ask again.'; return }
  const err = (res.out || '').trim().split('\n').slice(-25).join('\n');
  chat.msgs.push({role: 'user', content: `That program ${res.timeout ? 'ran too long and was stopped' : 'failed'}. Output:\n\`\`\`\n${err}\n\`\`\`\nFix it and send the whole corrected program.`});
  const fx = await answer(`⚡ Blitzen · fix ${tries + 1} of ${RETRIES}`, {pack: {lua: 'blocklua', js: 'js', python: 'python', cpp: 'cpp'}[c.dataset.lang]});
  await checkAnswer(fx, tries + 1);
}
async function ask(text, o = {}) {
  text = String(text || '').trim(); if (!text || busy) return;
  if (o.lang !== undefined) langSel.value = o.lang;
  if (!chat.msgs.length) log.innerHTML = '';
  chat.msgs.push({role: 'user', content: text + (o.context ? '\n\n' + o.context : '')});
  const u = bubble('user'); u.querySelector('.body').innerHTML = md(text) + (o.context ? `<details class='ctx'><summary>with ${esc(o.contextName || 'a file')}</summary><pre></pre></details>` : '');
  if (o.context) u.querySelector('.ctx pre').textContent = o.context;
  ta.value = ''; const el = await answer(null, {blocks: o.blocks}); if (el && !o.blocks) await checkAnswer(el, 0);
}
$('#go').onclick = () => ask(ta.value);
$('#stop').onclick = () => abort && abort.abort();
ta.addEventListener('keydown', e => { if (e.key == 'Enter' && !e.shiftKey) { e.preventDefault(); ask(ta.value) } });
$('#newBtn').onclick = () => { if (busy) return; chat.msgs = []; chat.name = null; hello(); st.textContent = '' };
$('#histBtn').onclick = async () => {
  const h = $('#hist'); if (!h.hidden) { h.hidden = true; return }
  const names = await listChats();
  h.innerHTML = names.length ? names.slice(0, 40).map(n => `<button data-n='${esc(n)}'>${esc(n.replace(/^Chat /, ''))}</button>`).join('') : 'No saved chats yet.';
  h.hidden = false;
  h.querySelectorAll('button').forEach(b => b.onclick = async () => { chat.msgs = await loadChat(b.dataset.n); chat.name = b.dataset.n; log.innerHTML = ''; chat.msgs.forEach(m => { const el = bubble(m.role); if (m.role == 'user') el.querySelector('.body').innerHTML = md(m.content); else draw(el, m.content) }); h.hidden = true });
};

/* ---------- Settings (the app only): hardware, model, speed ---------- */
async function drawSettings() {
  const s = $('#settings'); let i;
  try { i = await invoke('status') } catch (e) { s.innerHTML = `<div class='err'>${esc(String(e))}</div>`; return }
  const gb = b => (b / 1073741824).toFixed(1) + ' GB';
  s.innerHTML = `<table>
   <tr><td>Processor</td><td>${esc(i.cpu)} (${i.cores} cores${i.avx2 ? ', AVX2' : ''})</td></tr>
   <tr><td>Memory</td><td>${gb(i.ram)}</td></tr>
   <tr><td>Graphics</td><td>${i.gpus.map(g => esc(g.name) + ' · ' + gb(g.vram)).join('<br>') || 'none found'}</td></tr>
   <tr><td>Engine</td><td>${esc(i.engine)}${i.engine_ready ? '' : ' (downloads on first use)'}</td></tr>
   <tr><td>Model</td><td>${i.model ? esc(i.model) + ' · ' + gb(i.model_size) : 'not downloaded yet'}</td></tr>
   <tr><td>Running</td><td>${i.loaded ? 'yes' : 'no (starts when you ask)'}</td></tr>
   <tr><td>Speed</td><td>${i.bench_tps ? i.bench_tps.toFixed(0) + ' tokens per second (measured)' : '–'}</td></tr></table>
   <div class='bar2' hidden><i></i></div><div class='smsg'></div>
   <button class='dl'>${i.model ? 'Check for a new model' : 'Download the model'}</button> <button class='bm'>Measure speed</button> <button class='ul'>Unload now</button> <button class='hw'>Re-check hardware</button>`;
  const msg = s.querySelector('.smsg'), bar = s.querySelector('.bar2');
  s.querySelector('.dl').onclick = async () => { bar.hidden = false; msg.textContent = 'Downloading…'; try { await invoke('model_download'); msg.textContent = 'Model ready.' } catch (e) { msg.textContent = String(e) } drawSettings() };
  s.querySelector('.bm').onclick = async () => { msg.textContent = 'Measuring (about 20 seconds)…'; try { const t = await invoke('benchmark'); msg.textContent = t.toFixed(0) + ' tokens per second' } catch (e) { msg.textContent = String(e) } };
  s.querySelector('.ul').onclick = async () => { await invoke('engine_unload').catch(() => {}); drawSettings() };
  s.querySelector('.hw').onclick = async () => { await invoke('hardware_recheck').catch(() => {}); drawSettings() };
}
if (TAURI) {
  $('#setBtn').hidden = false;
  $('#setBtn').onclick = () => { const s = $('#settings'); s.hidden = !s.hidden; if (!s.hidden) drawSettings() };
  TAURI.event.listen('download://progress', e => { const b = document.querySelector('#settings .bar2 i'); if (b) b.style.width = e.payload.pct + '%'; const m = document.querySelector('#settings .smsg'); if (m) m.textContent = e.payload.text });
  const askFile = f => { if (!f) return; if (busy) { st.textContent = 'Blitzen is busy; ask about ' + f.name + ' again when it finishes.'; return }
    if (chat.msgs.length) { chat.msgs = []; chat.name = null }
    ask('Explain this file and point out any mistakes.', {context: '```' + (f.lang || '') + '\n' + f.text + '\n```', contextName: f.name, lang: f.pack || ''}) };
  TAURI.event.listen('ask-file', e => askFile(e.payload));
  invoke('take_pending_ask').then(askFile).catch(() => {});
  TAURI.event.listen('engine://notice', e => { st.textContent = e.payload });
}
window.blitzen = {ask, chat, busy: () => busy, run, stop: () => abort && abort.abort()};
hello(); ta.focus();
