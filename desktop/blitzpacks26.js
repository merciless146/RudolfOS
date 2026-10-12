/* ===================== Blitzen: context packs =====================
   Short reference cards sent with a request, picked by language and by the app the question comes from, so the
   model doesn't have to remember Rudolf OS's own APIs. Each stays under about 800 words. */
const BLZ_SYSTEM=`You are Blitzen, the assistant built into Rudolf OS, running offline on this computer. You help with code in BlockLua (Lua), JavaScript, C++ and Python, and with everyday questions.
Rules: answer briefly and plainly. Put each program in one fenced code block with its language (\`\`\`lua, \`\`\`js, \`\`\`cpp, \`\`\`python). Give one complete program or function at a time. If you are not sure, say so. Never invent Rudolf OS functions that are not in the reference below.`;
const BLZ_PACKS={
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
help:`RUDOLF OS (for questions about the system itself)
Rudolf OS is a desktop that runs from a disc, a USB stick or an installed drive. Drive C: holds Documents, Pictures and Downloads; the Start menu lists apps; Settings has Network (Wi-Fi), Apps, Update and recovery.
Apps: Files, Notepad, Code Editor, BlockLua Builder, Rudolf Prompt (commands such as DIR, CD, LUA FILE.LUA, and ls, cat, grep), Terminal (Linux: Debian 13 with sudo apt install), Emu (Game Boy, GBA, SNES and DS games from Games/SNES and Games/DS), App Store (Linux apps), Oracle Browser.
Blitzen runs offline on this computer; nothing you type leaves it.`};
/* which pack: an explicit language wins; otherwise guess from the words */
function blzPickPack(lang,text){if(lang&&BLZ_PACKS[lang])return lang;const t=String(text||'').toLowerCase();
 if(/blocklua|\bblocks?\b|\blua\b|sys\.\w+|\bgl\.\w+/.test(t))return 'blocklua';
 if(/\bc\+\+|\bcpp\b|g\+\+|#include|std::/.test(t))return 'cpp';
 if(/\bpython\b|\.py\b|\bpip\b|def \w+\(/.test(t))return 'python';
 if(/javascript|\bjs\b|\.rapp|\bhtml\b|rudolf\.\w+|console\.log/.test(t))return 'js';
 if(/rudolf os|settings|start menu|wi-?fi|drive c/.test(t))return 'help';
 return null}
