/* ===================== Blitzen: context packs =====================
   Short reference cards sent with a request, picked by language and by the app the question comes from, so the
   model doesn't have to remember Rudolf OS's own APIs. Each stays under about 800 words. */
const BLZ_SYSTEM=`You are Blitzen, the assistant built into Rudolf OS, running offline on this computer. You help with code in BlockLua (Lua), Luau (Roblox's Lua), Pilot.lua (Waste of Space microcontrollers), JavaScript, C++ and Python, and with everyday questions.
Rules: answer briefly and plainly. Put each program in one fenced code block with its language (\`\`\`lua, \`\`\`luau, \`\`\`pilot, \`\`\`js, \`\`\`cpp, \`\`\`python). Give one complete program or function at a time. If you are not sure, say so. Never invent Rudolf OS functions that are not in the reference below.`;
const BLZ_PACKS={
lua:`LUA 5.3 ON RUDOLF OS
Plain Lua 5.3 in any style (tables, metatables, closures, coroutines, string patterns, integer division //, bitwise operators). Write \`\`\`lua blocks. Programs run in Fengari (Lua in the page, offline) with a 10-second limit; print and io.write output is shown.
Standard Lua: string, table, math, utf8, coroutine, os.time/os.date/os.clock, io.write, io.read, io.open (files on drive C:).
sys library: sys.version() · sys.user() · sys.cwd() · sys.read(path) → text or nil · sys.write(path, text) → true/false · sys.append(path, text) · sys.exists(path) · sys.isdir(path) · sys.list(path) → table of names (folders end in \\) · sys.mkdir(path) · sys.remove(path) · sys.beep(name: "notify","error","click","game","open","close") · sys.open(app: "calc","notepad","paint","explorer","browser") · sys.input(prompt).
Paths use drive C: with backslashes, e.g. "C:\\\\PROGRAMS\\\\NOTES.TXT" (in a Lua string each \\ is written \\\\).
gl library (3D): gl.open(title,w,h) · gl.clear(r,g,b) · gl.color(r,g,b,a) · gl.camera(x,y,z,tx,ty,tz,fov) · gl.light(x,y,z) · gl.cube(x,y,z,size,rx,ry,rz) · gl.sphere(x,y,z,r) · gl.pyramid(...) · gl.plane(x,y,z,s) · gl.line(...) · gl.triangle(...) · gl.run(function(t, dt) ... end) draws every frame · gl.key(name) → pressed? · gl.mouse() · gl.stop() · gl.close().
quake library: quake.run(args) · quake.cmd("map e1m1") · quake.state() · quake.stop().
Example:
local words = {}
for w in ("the quick brown fox"):gmatch("%a+") do
  words[#words + 1] = w:upper()
end
table.sort(words)
print(table.concat(words, ", "))`,
blocklua:`BLOCKLUA REFERENCE (Rudolf OS)
BlockLua Builder makes Lua 5.3 programs from blocks; they run in Fengari (Lua in the browser). Each block is one line:
print(VALUE) · NAME = VALUE · local NAME = VALUE · NAME = io.read() (ask the user) · for _i=1,N do (repeat N times) · for V=FROM,TO do · while COND do · if COND then · elseif COND then · else · end · function NAME(PARAMS) · return VALUE · break · -- note · sys.beep("notify") · sys.open("calc") · sys.write(PATH, TEXT) · any other single line of Lua.
Write one statement per line. Prefer the shapes above; use "for V=FROM,TO do" instead of "for k,v in pairs(t)" when you can.
Standard Lua: string, table, math, utf8, coroutine, os.time/os.date/os.clock, io.write, io.read, io.open (files on drive C:).
sys library: sys.version() · sys.user() · sys.cwd() · sys.read(path) → text or nil · sys.write(path, text) → true/false · sys.append(path, text) · sys.exists(path) · sys.isdir(path) · sys.list(path) → table of names (folders end in \\) · sys.mkdir(path) · sys.remove(path) · sys.beep(name: "notify","error","click","game","open","close") · sys.open(app: "calc","notepad","paint","explorer","browser") · sys.input(prompt).
Paths use drive C: with backslashes, e.g. "C:\\\\PROGRAMS\\\\NOTES.TXT" (in a Lua string each \\ is written \\\\).
gl library (3D): gl.open(title,w,h) · gl.clear(r,g,b) · gl.color(r,g,b,a) · gl.camera(x,y,z,tx,ty,tz,fov) · gl.light(x,y,z) · gl.cube(x,y,z,size,rx,ry,rz) · gl.sphere(x,y,z,r) · gl.pyramid(...) · gl.plane(x,y,z,s) · gl.line(...) · gl.triangle(...) · gl.run(function(t, dt) ... end) draws every frame · gl.key(name) → pressed? · gl.mouse() · gl.stop() · gl.close().
quake library: quake.run(args) · quake.cmd("map e1m1") · quake.state() · quake.stop().
Example:
-- count down
for i=3,1,-1 do
  print(i)
end
sys.beep("game")
print("Go!")`,
js:`JAVASCRIPT ON RUDOLF OS
Plain JavaScript (ES2022) runs in a Web Worker here: console.log output is shown; there is no page (no document, no window) and no network.
For a page or an app, write HTML with a <script>. A Rudolf OS app (.RAPP) is JSON:
{ "id":"com.example.hello", "name":"Hello", "icon":"👋", "version":"1.0", "description":"…", "permissions":["notifications","files","clipboard"], "html":"<h1>Hello</h1><script>…<\/script>" }
The page runs in a sandboxed frame and talks to Rudolf OS through window.rudolf (all calls return promises):
rudolf.notify(title, body) (needs "notifications") · rudolf.setTitle(text) · rudolf.storage.get(key) / rudolf.storage.set(key, value) · rudolf.files.open() → {name, text, dataURL} (needs "files") · rudolf.files.save(name, text) (needs "files") · rudolf.clipboard.read() / rudolf.clipboard.write(text) (read needs "clipboard") · rudolf.user() → {name, language} · rudolf.theme() → {dark, accent}.
Ask only for the permissions the app needs. Make the package in Code Editor → "Package as app".
Example (app page):
<button id=b>Say hi</button>
<script>
document.getElementById('b').onclick = () => rudolf.notify('Hello', 'from my app');
<\/script>`,
cpp:`C++ ON RUDOLF OS
Programs are compiled in Linux (Debian 13) with: g++ -std=c++17 -O1 -Wall main.cpp, then run with no input, no network and a 10-second limit.
Write one complete file with int main(). Use only the standard library (iostream, string, vector, map, algorithm, fstream, chrono, random…). Read input with std::cin only when asked; print results with std::cout.
Files: the user's folders are ~/Documents, ~/Downloads and ~/Pictures (the same as Documents, Downloads and Pictures on drive C:).
To build in the Terminal: g++ -std=c++17 -O2 -Wall -o prog main.cpp && ./prog. Install the compiler with: sudo apt install g++.
Example:
#include <iostream>
#include <vector>
int main() {
    std::vector<int> v{3, 1, 2};
    int sum = 0;
    for (int x : v) sum += x;
    std::cout << "sum = " << sum << "\\n";
}`,
python:`PYTHON ON RUDOLF OS
Python 3 runs in Linux (Debian 13): python3 -I main.py, with no input, no network and a 10-second limit when Blitzen runs it.
Use the standard library (os, pathlib, csv, json, re, datetime, math, random, collections, itertools, shutil). Other packages come from Debian: sudo apt install python3-requests, python3-numpy, and so on (pip install needs a virtual environment: python3 -m venv ~/venv).
Files: the user's folders are ~/Documents, ~/Downloads and ~/Pictures, the same as Documents, Downloads and Pictures on drive C:. Use pathlib.Path.home() / "Documents".
Read input with input() only when asked. Print results with print().
Example:
from pathlib import Path
docs = Path.home() / "Documents"
for f in sorted(docs.glob("*.txt")):
    print(f.name, f.stat().st_size, "bytes")`,
luau:`LUAU (Roblox's Lua) ON RUDOLF OS
Luau is Lua 5.1 with more: optional types, compound assignment, continue, string interpolation and if-expressions. Write \`\`\`luau blocks. Blitzen checks the types with Luau's own checker, then runs the program (10-second limit, print() output shown).
Syntax: local n: number = 0 · n += 1 (also -= *= /= //= %= ..=) · local name: string? = nil · function add(a: number, b: number): number · type Point = { x: number, y: number } · local t: { string } = {} · continue inside loops · print(\`total: {n}\`) · local s = if n > 1 then "many" else "one" · local x = y :: number (a cast) · --!strict on the first line asks for strict checking.
Built in: print, typeof, type, tostring, tonumber, pairs, ipairs, select, error, pcall, assert, string, table (table.find, table.clone, table.clear, table.freeze), math (math.clamp, math.round, math.sign), os.time, os.clock, os.date, utf8, bit32, buffer, coroutine. for k, v in t do (no pairs needed). There is no io, no files and no network here.
Roblox game scripts (game:GetService, workspace, script.Parent, Instance.new) are checked but not run: they need Roblox. Vector3, CFrame, Color3, UDim2, Enum and task.wait are known to the checker.
Example:
--!strict
type Item = { name: string, price: number }
local items: { Item } = { { name = "apple", price = 2 }, { name = "pear", price = 3 } }
local total = 0
for _, item in items do
	total += item.price
end
print(\`{#items} items cost {total}\`)`,
pilot:`PILOT.LUA (Waste of Space microcontrollers)
Pilot.lua is the Luau a Microcontroller runs in the Roblox game Waste of Space. Write \`\`\`pilot blocks. Blitzen checks every part call against Pilot.lua's own definitions, then runs the program with stand-in parts that print what they are asked to do (no game here; events fire only if the game would fire them).
Parts: GetPart("Screen") → the first connected part of that class (or nil) · GetParts("Light") → all of them · GetPort(1) · TriggerPort(port) · GetPartFromPort(port, "Disk") · Beep(frequency) · JSONEncode(t) / JSONDecode(s) · task.wait(seconds) · Microcontroller.
Common parts: Screen: CreateElement("TextLabel", { Text = "hi", Size = UDim2.fromScale(1, 1), TextScaled = true }) · ClearElements() · GetDimensions(). Speaker: Chat(message) · PlaySound(id). Disk: Write(key, value) · Read(key) · ReadAll() · Clear(). Light: SetColor(Color3.fromRGB(255, 0, 0)) · Brightness. Sign: SignText. Switch: SwitchValue. Gyro: PointAt(position). Thruster: Propulsion. Modem: GetAsync(url) · PostAsync(url, data). Instrument: GetReading(type).
Events: part.Name:Connect(function(...) end), e.g. Microphone.Chatted:Connect(function(player, message) end), Keyboard.TextInputted:Connect(function(text, player) end), Port.Triggered, Switch.OnClick.
Check for nil when a part may be missing: local screen = GetPart("Screen") if not screen then error("no screen connected") end.
Example:
local screen = GetPart("Screen")
local mic = GetPart("Microphone")
assert(screen and mic, "connect a Screen and a Microphone")
screen:ClearElements()
local label = screen:CreateElement("TextLabel", { Text = "Say something", Size = UDim2.fromScale(1, 1), TextScaled = true })
mic.Chatted:Connect(function(player, message)
	label.Text = message
end)`,
help:`RUDOLF OS (for questions about the system itself)
Rudolf OS is a desktop that runs from a disc, a USB stick or an installed drive. Drive C: holds Documents, Pictures and Downloads; the Start menu lists apps; Settings has Network (Wi-Fi), Apps, Update and recovery.
Apps: Files, Notepad, Code Editor, BlockLua Builder, Rudolf Prompt (commands such as DIR, CD, LUA FILE.LUA, and ls, cat, grep), Terminal (Linux: Debian 13 with sudo apt install), Emu (Game Boy, GBA, SNES and DS games from Games/SNES and Games/DS), App Store (Linux apps), Oracle Browser.
Blitzen runs offline on this computer; nothing you type leaves it.`};
/* which pack: an explicit language wins; otherwise guess from the words */
function blzPickPack(lang,text){if(lang&&BLZ_PACKS[lang])return lang;const t=String(text||'').toLowerCase();
 if(/pilot\.?lua|waste of space|\bmicrocontroller|getpart(s|fromport)?\s*\(|triggerport/.test(t))return 'pilot';
 if(/\bluau\b|\broblox\b|game:getservice|\bworkspace\b|--!strict/.test(t))return 'luau';
 if(/blocklua|\bblocks?\b|\blua\b|sys\.\w+|\bgl\.\w+/.test(t))return 'blocklua';
 if(/\bc\+\+|\bcpp\b|g\+\+|#include|std::/.test(t))return 'cpp';
 if(/\bpython\b|\.py\b|\bpip\b|def \w+\(/.test(t))return 'python';
 if(/javascript|\bjs\b|\.rapp|\bhtml\b|rudolf\.\w+|console\.log/.test(t))return 'js';
 if(/rudolf os|settings|start menu|wi-?fi|drive c/.test(t))return 'help';
 return null}
