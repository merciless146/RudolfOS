// 1.6.0 (plan Part 12, "The rule"): Native must look exactly as Standard. The same state (the page's storage, served by
// same_rd.py) drawn by Standard (the desktop page in Chromium) and by Native (rudolf-native under Xvfb), at the same
// screen size; each part cut out of both screenshots where Standard's page has it, and compared (pxdiff.py): a part
// fails over 2 % of its pixels differing by more than a small tolerance. Both pictures and the difference picture are
// kept in OUT for the PR.   node same_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY [ONLY-CASES,...]
// (Run by same_t.sh, which starts same_rd.py and Xvfb.)
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP, ONLY] = process.argv.slice(2);
const W = +(process.env.SAME_W || 1920), H = +(process.env.SAME_H || 1080), LIMIT = +(process.env.SAME_LIMIT || 2), TOL = +(process.env.SAME_TOL || 32);
const BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const wait = ms => new Promise(r => setTimeout(r, ms));
const sh = (c, o) => cp.execSync(c, Object.assign({env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})}, o || {})).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
fs.mkdirSync(OUT, {recursive: true});
// ---- the state (same_state.js)
const ST = require('./same_state.js'), ACCT = ST.ACCT; const state = (set, more) => ST.state(D, set, more);
const calls = () => { try { return fs.readFileSync(path.join(D, 'calls'), 'utf8') } catch (e) { return '' } };
// ---- the cases: the settings, what is done before the screenshot, and the parts compared
const STY = ['standard', 'wire', 'mono', 'candy', 'retro'];
const CASES = [];
CASES.push({n: 'signin', set: {}, signin: 1, parts: ['screen', 'signbox']});
CASES.push({n: 'desk', set: {}, parts: ['wallpaper', 'bar', 'start', 'pins', 'tray', 'clock', 'icons']});
for (const st of STY) if (st != 'standard') CASES.push({n: 'bar-' + st, set: {tbstyle: st}, parts: ['bar']});
for (const m of ['normal', 'negative', 'comp']) CASES.push({n: 'bar-accent-' + m, set: {tbac: true, tbacm: m}, parts: ['bar']});
for (const st of STY) CASES.push({n: 'btn-' + st, set: {tbbstyle: st}, parts: ['bar']});
CASES.push({n: 'btn-accent', set: {tbbac: true, tbbacm: 'normal'}, parts: ['bar']});
CASES.push({n: 'bar-top', set: {tb: 'top'}, parts: ['bar', 'icons']});
/* (item 3's last part) the taskbar at the left and the right: the bar, the icons beside it, a window's button and title bar, the
   start menu, the styles, the pop-ups placed beside it (flyPlace) or where the page puts them */
CASES.push({n: 'bar-left', set: {tb: 'left'}, parts: ['bar', 'icons']});
CASES.push({n: 'bar-right', set: {tb: 'right', h24: true}, parts: ['bar', 'icons']});
CASES.push({n: 'bar-left-win', set: {tb: 'left'}, act: 'notepad', parts: ['bar', 'title']});
CASES.push({n: 'bar-left-candy', set: {tb: 'left', tbstyle: 'candy'}, parts: ['bar']});
CASES.push({n: 'bar-right-retro', set: {tb: 'right', tbstyle: 'retro'}, parts: ['bar']});
CASES.push({n: 'menu-left', set: {tb: 'left'}, act: 'menu', parts: ['menu']});
CASES.push({n: 'menu-right', set: {tb: 'right'}, act: 'menu', parts: ['menu']});
CASES.push({n: 'fly-vol-left', set: {tb: 'left'}, fly: 'vol', parts: ['fly']});
CASES.push({n: 'fly-net-right', set: {tb: 'right'}, fly: 'net', parts: ['fly']});
CASES.push({n: 'fly-qs-left', set: {tb: 'left'}, fly: 'qs', parts: ['fly']});
CASES.push({n: 'clock-24', set: {h24: true, df: 'ymd'}, parts: ['clock']});
CASES.push({n: 'wall-gradient', set: {bs: 'gradient', b1: '#0f766e', b2: '#1e1b4b'}, parts: ['wallpaper']});
CASES.push({n: 'menu', set: {}, act: 'menu', parts: ['menu']});
/* (2026-11-16, the owner's Vivobook: Packages not found) the start menu's search as Standard's: the list hidden, what is found in groups, the
   menu as tall as that; light and dark */
CASES.push({n: 'menu-search', set: {dm: 'light'}, act: 'menu', type: 'pa', parts: ['menu']});
CASES.push({n: 'menu-search-dark', set: {dm: 'dark', darkui: true}, act: 'menu', type: 'pac', parts: ['menu']});
CASES.push({n: 'hover-pin', set: {}, act: 'hover', parts: ['bar', 'pins']});
CASES.push({n: 'hover-candy', set: {tbstyle: 'candy'}, act: 'hover', parts: ['bar']});
for (const st of STY) CASES.push({n: 'title-' + st, set: {twstyle: st}, act: 'notepad', parts: st == 'standard' ? ['title', 'bar'] : ['title']});
CASES.push({n: 'title-negative', set: {twacm: 'negative'}, act: 'notepad', parts: ['title']});
CASES.push({n: 'lock', set: {}, act: 'lock', parts: ['screen', 'signbox']});
/* (item 1's second part) files in the Desktop folder, the desktop's and a file's menus, Rename and Open with */
CASES.push({n: 'desk-files', set: {}, files: 1, parts: ['icons']});
CASES.push({n: 'desk-menu', set: {}, files: 1, act: 'deskmenu', parts: ['ctx']});
CASES.push({n: 'file-menu', set: {}, files: 1, act: 'filemenu', parts: ['ctx']});
CASES.push({n: 'file-rename', set: {}, files: 1, act: 'rename', parts: ['dlg']});
CASES.push({n: 'file-openwith', set: {}, files: 1, act: 'openwith', parts: ['dlg']});
/* (item 2) the widgets: each of the five, in its place; the weather's place and answer the same for both (Open-Meteo answered
   by the test: Standard's page through the route, Native's through RUDOLF_WX_BASE) */
const WG = [{t: 'clock', x: 1640, y: 30, txt: ''}, {t: 'cal', x: 1640, y: 200, txt: ''}, {t: 'note', x: 1370, y: 30, txt: 'Buy milk\nCall Bob at 5'}, {t: 'mon', x: 1370, y: 250, txt: ''}, {t: 'weather', x: 1100, y: 30, txt: ''}];
const WLOC = {lat: 52.52, lon: 13.41, name: 'Berlin, Land Berlin, Germany', cc: 'DE'};
const WX = require('./same_rd_wx.json');   /* (what same_rd.py answers Native with) */
CASES.push({n: 'widgets', set: {}, more: {['bd95widgets:' + ACCT]: WG, bd95wloc: WLOC}, wg: 5, parts: ['wg0', 'wg1', 'wg2', 'wg3', 'wg4']});
CASES.push({n: 'widgets-ask', set: {}, more: {['bd95widgets:' + ACCT]: [{t: 'weather', x: 1100, y: 30, txt: ''}, {t: 'note', x: 1370, y: 30, txt: ''}]}, wg: 2, parts: ['wg0', 'wg1']});
CASES.push({n: 'widgets-top', set: {tb: 'top', h24: true}, more: {['bd95widgets:' + ACCT]: WG.slice(0, 2), bd95wloc: WLOC}, wg: 2, parts: ['wg0', 'wg1']});
CASES.push({n: 'widgets-over', set: {bs: 'gradient', b1: '#0f766e', b2: '#1e1b4b'}, more: {['bd95widgets:' + ACCT]: [{t: 'clock', x: 30, y: 60, txt: ''}, {t: 'mon', x: 60, y: 200, txt: ''}]}, wg: 2, parts: ['wg0', 'wg1']});   /* (over the icons: what is behind, blurred) */
CASES.push({n: 'widget-picker', set: {}, act: 'wgpick', parts: ['ctx']});
/* (item 3) the taskbar's right-click menus: the empty bar, the start button, a pinned app (Oracle's with its own row), a window's
   button (read from the window's page), the sound, the network, the clock */
const PINS2 = {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'calc', 'settings', 'browser']};
for (const r of ['bar', 'start', 'pin:notepad', 'pin:browser', 'vol', 'net', 'clock']) CASES.push({n: 'tbm-' + r.replace(':', '-'), set: {}, more: PINS2, rc: r, parts: ['ctx']});
CASES.push({n: 'tbm-win', set: {}, more: PINS2, act: 'notepad', rc: 'win:notepad', parts: ['ctx']});
/* (item 3, second part) the tray's pop-ups: the calendar under the clock (with the Calendar's events; the next month; the dark
   theme; the taskbar at the top) and the volume's */
const CALEV = {['bd95cal:' + ACCT]: [{d: '2026-11-20', t: '10:00', title: 'Dentist'}, {d: '2026-11-14', t: '', title: 'Lunch with Bob'}, {d: '2026-10-02', t: '', title: 'Past'}]};
CASES.push({n: 'fly-cal', set: {}, more: CALEV, fly: 'clock', parts: ['fly']});
CASES.push({n: 'fly-cal-next', set: {}, more: CALEV, fly: 'clock', next: 1, parts: ['fly']});
CASES.push({n: 'fly-cal-dark', set: {dm: 'dark', darkui: true}, fly: 'clock', parts: ['fly']});
CASES.push({n: 'fly-cal-top', set: {tb: 'top', h24: true}, fly: 'clock', parts: ['fly']});
CASES.push({n: 'fly-vol', set: {}, fly: 'vol', parts: ['fly']});
/* (and quick settings: off, some switches on, the dark theme; the notifications' centre: two, a failure among them, and none) */
const NOTES = {['bd95notif:' + ACCT]: [{id: 1, t: 'Download finished', b: 'report.pdf is in Downloads', k: 'info', app: '', at: Date.parse('2026-11-14T09:40:00Z')},
  {id: 2, t: 'Update failed', b: 'Could not reach the update server', k: 'error', app: '', at: Date.parse('2026-11-14T08:40:00Z')}]};
CASES.push({n: 'fly-qs', set: {}, fly: 'qs', parts: ['fly']});
/* (1.6.0 Part 5) mobile data in use: the network icon's bars (signal 74 %: three), and the network pop-up's rows (mobile data in reserve behind Wi-Fi) */
const CELLNET = {ok: true, interfaces: [{name: 'wwan0', state: 'unknown', ip: '10.64.12.34/30', wireless: false, virtual: false, default: true, cellular: true, carrier: 'T-Mobile', tech: 'LTE', signal: 74}]};
CASES.push({n: 'tray-cell', set: {}, rd: {net: CELLNET}, parts: ['tray']});
CASES.push({n: 'fly-net-cell', set: {}, rd: {net: {ok: true, interfaces: [{name: 'wlan0', ip: '192.168.1.5/24', state: 'up', wireless: true, virtual: false, default: true, ssid: 'Home', rssi: -55}, Object.assign({}, CELLNET.interfaces[0], {default: false})]}}, fly: 'net', parts: ['fly']});
CASES.push({n: 'fly-qs-on', set: {dnd: true, sr: true}, fly: 'qs', parts: ['fly']});   /* (switches the shell does not show otherwise: Night light's tint, Touch mode's sizes are later items') */
CASES.push({n: 'fly-qs-dark', set: {dm: 'dark', darkui: true}, fly: 'qs', parts: ['fly']});
CASES.push({n: 'fly-nc', set: {}, more: NOTES, fly: 'notes', parts: ['fly']});
CASES.push({n: 'fly-nc-empty', set: {}, fly: 'notes', parts: ['fly']});
CASES.push({n: 'fly-net', set: {}, fly: 'net', parts: ['fly']});   /* (the network's pop-up: item 3's last part) */
/* (item 3) the notifications' pop-ups (a page's notifications: rudolfd's "memory closed" events, apps57.js; one, two, the dark theme, the
   taskbar at the top) and the call's 🎙️ (an app using the microphone, muted or not) */
CASES.push({n: 'toast', set: {}, act: 'notepad', ev: ['memory closed Paint 300'], parts: ['toast']});
CASES.push({n: 'toast-two', set: {}, act: 'notepad', ev: ['memory closed Paint 300', 'memory closed Calculator 2048'], parts: ['toast', 'bar']});
CASES.push({n: 'toast-dark', set: {dm: 'dark', darkui: true}, act: 'notepad', ev: ['memory closed Paint 300'], parts: ['toast']});
CASES.push({n: 'toast-top', set: {tb: 'top'}, act: 'notepad', ev: ['memory closed Paint 300'], parts: ['toast']});
CASES.push({n: 'call', set: {}, mic: ['Oracle'], parts: ['bar']});
CASES.push({n: 'call-muted', set: {}, mic: ['Oracle'], micmuted: 1, parts: ['bar']});
CASES.push({n: 'tbm-bar-win', set: {}, more: PINS2, act: 'notepad', rc: 'bar', parts: ['ctx']});
/* (item 4) the workspaces: workspace 2 shown with a window in workspace 1 (its button "has", the taskbar without it, "Workspace 2"
   said), and the dark theme */
CASES.push({n: 'ws2', set: {}, act: 'notepad', ws: 2, parts: ['bar', 'toast']});
CASES.push({n: 'ws2-dark', set: {dm: 'dark', darkui: true}, act: 'notepad', ws: 2, parts: ['bar', 'toast']});
CASES.push({n: 'ws2-left', set: {tb: 'left'}, act: 'notepad', ws: 2, parts: ['bar']});
/* (item 5) the window keys: Alt+Tab's switcher held open over two windows (Notepad, then Calculator: Notepad chosen), with the dark
   theme and the accent colour; Super+Tab's overview of the two */
CASES.push({n: 'altab', set: {}, more: PINS2, act: 'two', key: 'altab', parts: ['altab']});
CASES.push({n: 'altab-accent', set: {accent: '#db2777'}, more: PINS2, act: 'two', key: 'altab', parts: ['altab']});
CASES.push({n: 'overview', set: {}, more: PINS2, act: 'two', key: 'overview', parts: ['desk']});
/* (item 6) the screen saver, after an hour without input (rudolfd's idle, as the test sets it): the page's own, full screen over
   everything (Native: in a window of its own) */
CASES.push({n: 'saver', set: {}, more: {bd95sys: {sson: true, ssmin: 1, ssstyle: 'stars', lockss: false}}, saver: 1, parts: ['screen']});
/* (item 8) the Phone screen's and Blitzen's windows, opened from the taskbar (one-app windows on Native): the whole window, its frame
   and what it shows, each in the states rudolfd gives (Linux not set up, scrcpy missing, ready; Blitzen not downloaded, ready).
   The frame and title bar ('title') compared as every part; the whole window ('win') with its page, WebKit's against Chromium's,
   allowing a pixel's match one pixel away (the engines round line boxes' half pixels differently) */
const PINS8 = {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'phonescreen', 'blitzen']};
CASES.push({n: 'phone-nolinux', set: {}, more: PINS8, app: 'phonescreen', rd: {deb: {installed: false}}, parts: ['win', 'title', 'bar']});
CASES.push({n: 'phone-noscrcpy', set: {}, more: PINS8, app: 'phonescreen', rd: {deb: {installed: true, scrcpy: false}}, parts: ['win']});
CASES.push({n: 'phone-ready', set: {}, more: PINS8, app: 'phonescreen', rd: {deb: {installed: true, scrcpy: true}}, parts: ['win']});
CASES.push({n: 'phone-dark', set: {dm: 'dark', darkui: true}, more: PINS8, app: 'phonescreen', rd: {deb: {installed: true, scrcpy: true}}, parts: ['win']});
CASES.push({n: 'blitzen-setup', set: {}, more: PINS8, app: 'blitzen', rd: {ai: {cpu: 1, installed: false, persistent: true, size_hint: '0.7 GB'}}, parts: ['win', 'title', 'bar']});
CASES.push({n: 'blitzen-chat', set: {}, more: PINS8, app: 'blitzen', rd: {ai: {cpu: 1, installed: true, persistent: true}}, parts: ['win']});
CASES.push({n: 'blitzen-oldcpu', set: {twstyle: 'candy'}, more: PINS8, app: 'blitzen', rd: {ai: {cpu: 0}}, parts: ['win', 'title']});
/* (the test findings of 2026-11-15, docs/native-findings-2026-11-15.md) the page's dialogs over the whole desktop, dimmed, centred on the
   screen, as Standard's (N2: Notepad's Open; N4: Close anyway?, the camera's Allow access? before its window; N10: the Print dialog):
   the dialog and the whole screen under it. js: run in the app's page (Native: in its window, RUDOLF_NATIVE_TEST_EVAL); undo: after the
   pictures (Native), so its window can close */
const PINSF = {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'calc', 'settings', 'camera']};
const UNDLG = "document.querySelectorAll('.rmodal,.natdlg').forEach(x => x.remove())";
CASES.push({n: 'dlg-open', set: {}, more: PINSF, app: 'notepad', js: "fileDlg({mode: 'open'})", undo: UNDLG, parts: ['dlg', 'screen']});
CASES.push({n: 'dlg-ask', set: {}, more: PINSF, app: 'notepad', js: "docAsk('1 document has changes that are not saved. Close anyway?', 'Close', 'Keep open')", undo: UNDLG, parts: ['dlg', 'screen']});
CASES.push({n: 'dlg-print', set: {}, more: PINSF, app: 'notepad', js: "prDialog(null, '<p>Hello</p>', {title: 'Report'})", undo: UNDLG, parts: ['dlg', 'screen']});
CASES.push({n: 'dlg-camera', set: {}, more: PINSF, app: 'camera', nowin: 1, undo: "document.querySelector('.rmodal .n') && document.querySelector('.rmodal .n').click()", parts: ['dlg', 'screen']});
CASES.push({n: 'dlg-dark', set: {dm: 'dark', darkui: true}, more: PINSF, app: 'notepad', js: "fileDlg({mode: 'save', name: 'NOTES.TXT'})", undo: UNDLG, parts: ['dlg', 'screen']});
/* (N3) Task Manager's Apps with Notepad and Settings open: a row for each window (its app, title, status, workspace), as on Standard */
CASES.push({n: 'tm-apps', set: {}, more: {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'calc', 'settings', 'taskmgr']}, pre: ['notepad', 'settings'], app: 'taskmgr',
  read: "JSON.stringify([...document.querySelectorAll('.tmt tbody tr')].map(r => [...r.children].slice(0, 4).map(c => c.textContent)).concat([[...document.querySelectorAll('.tmst')].map(x => x.textContent.split(' · ')[0])]))", parts: []});
/* (N1) a video made full screen with the Media Player's ⛶ (paused at 2 s): the picture fills the screen on both (the test's own video,
   DIR/tm/t.webm, made by same_t.sh with ffmpeg; the case is skipped without it). Standard's page is given the shell's full screen
   (shell26.js fsStandIn, as the shell's "hello" does), as in the session. 'picture': the share of the screen in the video's colour bars,
   not every pixel (the engines may show a frame or two apart) */
const MPJS = "(() => { const w = [...document.querySelectorAll('.win')].filter(e => e.getClientRects().length).pop(); w.querySelector('.ou').click(); const d = w.querySelector('.dlg'); d.querySelector('input').value = location.origin + '/tm/t.webm'; [...d.querySelectorAll('button')].pop().click(); " +
  "const go = v => { v.muted = true; v.pause(); v.addEventListener('seeked', () => w.querySelector('.fs').click(), {once: true}); v.currentTime = 2 }; const v = w.querySelector('video'); if (v.readyState >= 2) go(v); else v.addEventListener('loadeddata', () => go(v), {once: true}) })()";
CASES.push({n: 'video-fs', set: {}, more: {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'media', 'settings']}, app: 'media', media: 't.webm', shellfs: 1, js: MPJS, jswait: 3000, undo: "document.fullscreenElement && document.exitFullscreen()", parts: ['picture']});
/* (2026-11-16, the known differences "the overview's and the toasts' pictures without a compositor") the overview over a playing video:
   its window's picture keeps moving, as Standard's windows do under the overview (the share of the screen changing in 2 s on both);
   a toast over a window moved under it after it came: what is round it (its shadow, its corners) is that window on both */
const MPLIVE = "(() => { const w = [...document.querySelectorAll('.win')].filter(e => e.getClientRects().length).pop(); w.querySelector('.ou').click(); const d = w.querySelector('.dlg'); d.querySelector('input').value = location.origin + '/tm/t.webm'; [...d.querySelectorAll('button')].pop().click(); " +
  "const v = w.querySelector('video'); v.muted = true; v.loop = true; v.play() })()";
CASES.push({n: 'overview-live', set: {}, more: {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'media', 'settings']}, app: 'media', media: 't.webm', js: MPLIVE, jswait: 2500, key: 'overview', moving: 2000, parts: ['moving']});
/* (what this session's cases read besides their pictures: wopen, video-frames; each engine's, for the case being run) */
const XTRA = {std: {}, nat: {}};
/* (N15; 1.6.0 Part 17) a window opening: the LitePoint transition, corner to corner (200 ms), on both: Standard's (litepoint60.js) and
   Native's (the same scene, litepoint.h), its picture part of the way through not yet the window's own */
CASES.push({n: 'wopen', set: {}, act: 'notepad', wopen: 1, parts: ['wopen']});
/* (N16, N29) a video played to its end in the Media Player: its frame callbacks called (requestVideoFrameCallback) and its count of frames
   at the end (getVideoPlaybackQuality) as Chromium's, within 10 */
const MPFRAMES = "(() => { const w = [...document.querySelectorAll('.win')].filter(e => e.getClientRects().length).pop(); w.querySelector('.ou').click(); const d = w.querySelector('.dlg'); d.querySelector('input').value = location.origin + '/tm/t.webm'; [...d.querySelectorAll('button')].pop().click(); " +
  "const v = w.querySelector('video'); v.muted = true; v.loop = false; window.__vf = {cb: 0, end: -1}; const f = () => { __vf.cb++; v.requestVideoFrameCallback(f) }; v.requestVideoFrameCallback(f); v.addEventListener('ended', () => { __vf.end = v.getVideoPlaybackQuality().totalVideoFrames }, {once: true}); v.play() })()";
CASES.push({n: 'video-frames', set: {}, more: {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'media', 'settings']}, app: 'media', media: 't.webm', js: MPFRAMES, jswait: 8500, readx: "JSON.stringify(window.__vf)", parts: ['frames']});
/* (N28) GPU Lab's Graphics info: hardware or software drawing, as each engine draws (Playwright's Chromium: SwiftShader; Native here:
   --draw=soft) */
CASES.push({n: 'gpulab', set: {}, more: {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'gpulab', 'settings']}, app: 'gpulab', jswait: 1500,
  read: "(([...document.querySelectorAll('.win td')].find(e => e.textContent == 'Acceleration') || {}).nextElementSibling || {}).textContent"});
CASES.push({n: 'toast-live', set: {}, act: 'notepad', ev: ['memory closed Paint 300'], drag: [W - 600, H - 330], parts: ['toast']});
/* (the test findings that look wrong: N5, N6, N7, N8, N9, N13) the apps' windows whole ('win': WebKit's page against Chromium's, a pixel's
   match one pixel away allowed): Settings' Volume Controls and Mouse and touchpad (sliders, lists), the Media Player (seek bar, volume,
   speed), WordLite (font and size lists, the colour box), the Clock's alarms (a time field), File Explorer (Folders on this device, the
   status line), and what Settings › About says (read from both pages; the storage's numbers and the graphics chip are each engine's own) */
const PINSL = {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'settings', 'media', 'wordlite', 'clock']};
CASES.push({n: 'ctl-volume', set: {}, more: PINSL, app: 'settings', js: "settings('vol')", parts: ['win']});
CASES.push({n: 'ctl-volume-dark', set: {dm: 'dark', darkui: true}, more: PINSL, app: 'settings', js: "settings('vol')", parts: ['win']});   /* (the lists' chevrons in dark mode: the beta in QEMU, 2026-11-16) */
CASES.push({n: 'ctl-mouse', set: {}, more: PINSL, app: 'settings', js: "settings('mouse')", parts: ['win']});
CASES.push({n: 'ctl-media', set: {}, more: PINSL, app: 'media', parts: ['win']});
CASES.push({n: 'ctl-wordlite', set: {}, more: PINSL, app: 'wordlite', parts: ['win']});
CASES.push({n: 'ctl-clock', set: {}, more: PINSL, app: 'clock', parts: ['win']});
CASES.push({n: 'explorer', set: {}, more: PINSL, app: 'explorer', parts: ['win'],
  read: "JSON.stringify([[...document.querySelectorAll('.win *')].some(e => !e.children.length && /^Folders on this device$/i.test(e.textContent.trim())), ([...document.querySelectorAll('.win *')].map(e => !e.children.length && /Storage: /.test(e.textContent) ? e.textContent : '').find(x => x) || '').replace(/[\\d.]+ (B|KB|MB|GB|TB)/g, 'N')])"});
CASES.push({n: 'about', set: {}, more: PINSL, app: 'settings', js: "settings('about')", jswait: 1500,
  read: "JSON.stringify([...document.querySelectorAll('.ab tr')].map(r => [r.cells[0].textContent, /^(Storage used|Graphics)$/.test(r.cells[0].textContent) ? (r.cells[0].textContent == 'Graphics' ? /^ANGLE \\(/.test(r.cells[1].textContent) : /^[\\d.]+ (B|KB|MB) of [\\d.]+ (MB|GB|TB)$/.test(r.cells[1].textContent)) : r.cells[1].textContent]))"});
/* (N11, N12) the dynamic backgrounds' moving parts: Glacier Lake at night (the northern lights: the share of the screen that changes in
   3 s, at least 0.25 % on both: Standard 1.7 % when found, Native 0), Spirit Falls (its statue where the page puts it, with Reduce motion
   so the petals stand still; and the petals moving, without it) */
CASES.push({n: 'dyn-lake', set: {dyn: 'lake', dm: 'dark', aur: true}, moving: 3000, parts: ['moving']});
CASES.push({n: 'dyn-oasis', set: {dyn: 'oasis', dm: 'light', rmotion: true}, parts: ['statue']});
CASES.push({n: 'dyn-oasis-dark', set: {dyn: 'oasis', dm: 'dark', accent: '#db2777'}, moving: 3000, parts: ['statue', 'moving']});
/* (N17) an iframe given the sandbox flags the page's browser gives its tabs: WebKit logged "'allow-presentation' is an invalid sandbox flag"
   for each (nolog: not in Native's log during the case) */
CASES.push({n: 'sandbox-flags', set: {}, app: 'notepad', js: "const f = document.createElement('iframe'); f.setAttribute('sandbox', 'allow-scripts allow-forms allow-popups allow-popups-to-escape-sandbox allow-same-origin allow-downloads allow-presentation'); document.body.appendChild(f); setTimeout(() => f.remove(), 500)",
  nolog: /invalid sandbox flag/, parts: []});
/* (N14) a window whose own icon is not its app's (Flash Player's "not installed" window): its title bar's picture, as the page draws it */
CASES.push({n: 'title-icon', set: {}, more: {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'flash']}, app: 'flash', parts: ['title']});
/* (the second run's findings) a right-click menu inside File Explorer, on a folder (N21: it goes past the window's edge with all its
   rows), the title bar in dark mode (N27), Help (its scrollbar and its search box: N22, N25) */
CASES.push({n: 'app-menu', set: {}, more: PINSL, app: 'explorer', appctx: 'DOCUMENTS', parts: ['ctx']});
CASES.push({n: 'title-dark', set: {dm: 'dark', darkui: true}, more: PINSL, app: 'explorer', parts: ['title']});
CASES.push({n: 'help', set: {}, more: {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'help']}, app: 'help', parts: ['win']});
/* (the owner's photo of 2026-11-15: an app's title bar on Native unlike Standard's) the title bars of Settings, File Explorer, Notepad,
   Task Manager and Oracle (its own window, as Native opens it without Rudolf's Chromium), light and dark, each title bar style and two
   accent colours; a window not in use beside one in use ('titlei'); the settings the page's title bar follows: the font size (the bar
   grows with it), the font, the display scaling, the touch mode, high contrast, the window text colour (Wireframe's writing); Always
   on top chosen from the title bar's own menu (its 📌); the pointer over each button */
const PINST = {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'settings', 'taskmgr', 'browser', 'calc']};
for (const ap of ['settings', 'explorer', 'notepad', 'taskmgr', 'browser']) for (const dm of ['light', 'dark']) for (const st of STY) for (const ac of ['#2563eb', '#db2777'])
  CASES.push({n: 'tb-' + ap + '-' + dm + '-' + st + '-' + ac.slice(1), set: Object.assign({twstyle: st, accent: ac}, dm == 'dark' ? {dm: 'dark', darkui: true} : {dm: 'light'}), more: PINST, app: ap, parts: ['title']});
for (const dm of ['light', 'dark']) for (const st of STY) CASES.push({n: 'tbi-' + dm + '-' + st, set: Object.assign({twstyle: st}, dm == 'dark' ? {dm: 'dark', darkui: true} : {dm: 'light'}), more: PINST, pre: ['notepad'], app: 'calc', parts: ['title', 'titlei']});
const TBSET = {fs11: {fs: 11}, fs16: {fs: 16}, fs20: {fs: 20}, verdana: {ff: 'Verdana', fs: 14}, scale125: {scale: 125}, touch: {touch: true}, hc: {cb: 'contrast'}, 'hc-dark': {cb: 'contrast', dm: 'dark', darkui: true},
  'tx-wire': {tx: '#7c2d12', twstyle: 'wire'}, 'fs18-retro': {fs: 18, twstyle: 'retro'}, 'touch-candy': {touch: true, twstyle: 'candy'}};
for (const k in TBSET) CASES.push({n: 'tbs-' + k, set: Object.assign({dm: 'light'}, TBSET[k]), more: PINST, app: 'notepad', parts: ['title']});
/* (the owner's photo) Settings › System Settings › Desktop engine: the engine in use and the one chosen, as rudolfd says for each (Standard's
   page told chromium, Native's native) */
const ENG = {chromium: {choice: 'chromium', now: 'chromium', available: true, nativeAvailable: true, failed: false, persistent: true, gaveUp: '', offer: false},
  native: {choice: 'native', now: 'native', available: true, nativeAvailable: true, failed: false, persistent: true, gaveUp: '', offer: false}};
const ENGREAD = "(() => { const s = document.querySelector('#eng57'), n = document.querySelector('.eng57 .engnow'); return s ? s.options[s.selectedIndex].text + ' | ' + (n ? n.textContent : '') : 'no panel' })()";
CASES.push({n: 'engine', set: {dm: 'light'}, more: PINST, app: 'settings', rd: {engine: ENG}, js: "settings('sys')", jswait: 3000, read: ENGREAD,
  expect: {std: 'Standard (Chromium) | In use now: Standard (Chromium).', nat: 'Native | In use now: Native.'}, parts: []});
CASES.push({n: 'tbs-ontop', set: {dm: 'light'}, more: PINST, app: 'notepad', tbmenu: 'Always on top', parts: ['title']});
CASES.push({n: 'tbs-max', set: {dm: 'light'}, more: PINST, app: 'notepad', tbmenu: 'Maximize', parts: ['title']});
CASES.push({n: 'tbs-max-top', set: {dm: 'dark', darkui: true, tb: 'top', twstyle: 'candy'}, more: PINST, app: 'notepad', tbmenu: 'Maximize', parts: ['title']});
for (const [i, st] of [[0, 'standard'], [1, 'standard'], [2, 'standard'], [2, 'retro'], [1, 'candy'], [2, 'wire']]) CASES.push({n: 'tbh-' + st + '-' + i, set: {dm: 'light', twstyle: st}, more: PINST, app: 'notepad', hoverbtn: i, parts: ['title']});
/* (1.6.1, docs/plan-1.6.1.md Part 25) a minimized window's work in progress on its taskbar button (tbprog61.js; rudolf-native.c tbp_*): at 40 %
   and busy (the creeping fill, 30 s on: about 89 %), in the button styles, the dark theme, at the left, the colours (not high contrast: Standard's is a filter over
   the page, Native's the screen's gamma (oneapp59.js oagamma), which Xvfb's screenshots do not show, the same without the fill) */
const PJS = v => "const W = typeof OA == 'object' && OA.main ? OA.main : wins.filter(o => o.app == 'notepad').pop(); winProgress(W, " + JSON.stringify(v) + ")";
CASES.push({n: 'prog-40', set: {dm: 'light'}, more: PINST, app: 'notepad', js: PJS(40), tbmenu: 'Minimize', after: 1000, parts: ['bar', 'tbw']});
CASES.push({n: 'prog-busy', set: {dm: 'light'}, more: PINST, app: 'notepad', js: PJS('busy'), tbmenu: 'Minimize', after: 30000, parts: ['bar', 'tbw']});
for (const st of ['wire', 'mono', 'candy', 'retro']) CASES.push({n: 'prog-40-' + st, set: {dm: 'light', tbbstyle: st}, more: PINST, app: 'notepad', js: PJS(40), tbmenu: 'Minimize', after: 1000, parts: ['tbw']});
CASES.push({n: 'prog-40-dark-blue', set: {dm: 'dark', darkui: true, tbpcol: 'blue'}, more: PINST, app: 'notepad', js: PJS(40), tbmenu: 'Minimize', after: 1000, parts: ['tbw']});
CASES.push({n: 'prog-40-accent', set: {dm: 'light', tbpcol: 'accent', accent: '#9333ea'}, more: PINST, app: 'notepad', js: PJS(40), tbmenu: 'Minimize', after: 1000, parts: ['tbw']});
CASES.push({n: 'prog-ended', set: {dm: 'light'}, more: PINST, app: 'notepad', js: PJS(40) + '; setTimeout(() => winProgress(W, null), 3000)', tbmenu: 'Minimize', after: 4000, parts: ['bar']});   /* (done: gone) */
CASES.push({n: 'prog-40-left-custom', set: {dm: 'light', tb: 'left', tbpcol: '#c026d3'}, more: PINST, app: 'notepad', js: PJS(40), tbmenu: 'Minimize', after: 1000, parts: ['bar', 'tbw']});
const only = ONLY ? ONLY.split(',') : null;
// ---- Standard: the page in Chromium
let BR;
const micSet = c => { fs.writeFileSync(path.join(D, 'mic'), JSON.stringify(c.mic || [])); if (c.mic) cp.execSync(`curl -s --noproxy '*' -X POST -H 'X-Rudolf-Token: tok160same' '${BASE}/api/mic?muted=${c.micmuted ? 1 : 0}'`) };
const rdSet = c => { for (const k of ['deb', 'ai', 'net', 'engine']) { const f = path.join(D, k); if (c.rd && c.rd[k]) fs.writeFileSync(f, JSON.stringify(c.rd[k])); else try { fs.unlinkSync(f) } catch (e) {} } };
async function stdCase(c) {
  fs.writeFileSync(path.join(D, 'std-on'), '');   /* (rudolfd's events Standard's while its case runs: same_rd.py, the native shell's events thread since #410) */
  XTRA.std = {}; XTRA.nat = {};
  state(c.set, c.more); micSet(c); rdSet(c);
  const ctx = await BR.newContext({viewport: {width: W, height: H}, deviceScaleFactor: 1, timezoneId: 'UTC'});
  await ctx.clock.install({time: new Date('2026-11-14T09:41:00Z')});
  /* (nothing from outside: the internet's check answers, as on a connected computer, the rest is refused) */
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => /generate_204/.test(r.request().url()) ? r.fulfill({status: 204, body: ''}) :
    /api\.open-meteo\.com\/v1\/forecast/.test(r.request().url()) ? r.fulfill({status: 200, contentType: 'application/json', body: JSON.stringify(WX)}) : r.abort());
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && document.getElementById('login') && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await p.clock.runFor(1500); await wait(500);
  const rects = {}; let pt = null, hov = null;
  const rectOf = async sel => p.evaluate(s => { const e = typeof s == 'string' ? document.querySelector(s) : null; if (!e) return null; const r = e.getBoundingClientRect(); return r.width ? [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(r.height)] : null }, sel);
  if (!c.signin) {
    await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
    await p.waitForFunction(() => typeof USER != 'undefined' && USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000});
    for (let i = 0; i < 12; i++) { await p.clock.runFor(500); await wait(150) }
    await p.mouse.move(W / 2, H / 3);
    if (c.act == 'menu') { await p.click('#start'); await p.clock.runFor(600); await wait(300); if (c.type) { await p.keyboard.type(c.type); await p.clock.runFor(300); await wait(300) } }
    if (c.act == 'hover') { const r = await rectOf('#pins .pin:nth-child(2)'); await p.mouse.move(r[0] + r[2] / 2, r[1] + r[3] / 2); await p.clock.runFor(400); await wait(200) }
    if (c.act == 'notepad' && c.wopen) { XTRA.std.wopen = await p.evaluate(() => { launch('notepad'); const w = [...document.querySelectorAll('.win')].pop(); return [/inset/.test(w.style.clipPath), 'litepoint', (typeof LP_MS == 'object' ? LP_MS.c2c : 0) + 'ms'].join(' ') }) }
    if (c.act == 'notepad') { if (!c.wopen) await p.evaluate(() => launch('notepad')); for (let i = 0; i < 4; i++) { await p.clock.runFor(400); await wait(150) } await p.mouse.move(W / 2, H - 200) }
    for (const k of c.pre || []) { await p.evaluate(k => { launch(k) }, k); for (let i = 0; i < 4; i++) { await p.clock.runFor(400); await wait(150) } }   /* (apps open before c.app) */
    if (c.app && c.nowin) { await p.mouse.click(W / 2, H - 200); await p.evaluate(k => { launch(k) }, c.app); await p.clock.runFor(100); await wait(800); await p.mouse.move(W / 2, H - 200) }   /* (an app asking before its window: a dialog open, see c.js) */
    else if (c.app) { await p.evaluate(k => { launch(k) }, c.app); for (let i = 0; i < 8; i++) { await p.clock.runFor(400); await wait(150) } await p.mouse.move(W / 2, H - 200); await wait(300) }
    if (c.appctx) { const r = await p.evaluate(n => { const w = [...document.querySelectorAll('.win')].filter(e => e.getClientRects().length).pop(); const e = [...w.querySelectorAll('tr,.fi,.row')].find(x => x.textContent.includes(n) && x.getClientRects().length); const b = e.getBoundingClientRect(); return [Math.round(b.left + 20), Math.round(b.top + b.height / 2)] }, c.appctx);
      await p.mouse.click(r[0], r[1], {button: 'right'}); await p.clock.runFor(300); await wait(300); await p.mouse.move(W / 2, H - 100); await wait(200) }
    if (c.shellfs) await p.evaluate(() => { if (typeof fsStandIn == 'function') fsStandIn(() => true) });
    if (c.js) { const r = await p.evaluate(() => { const w = [...document.querySelectorAll('.win')].filter(e => e.offsetParent).pop(); const q = w && w.querySelector('.tb .t'); if (!q) return null; const b = q.getBoundingClientRect(); return [b.left + 4, b.top + b.height / 2] });
      if (r) { await p.mouse.click(r[0], r[1]); await p.mouse.move(W / 2, H - 200) }   /* (the last input a click on the app's title bar, not the password's keys: Chromium rings a button a script focuses only after keys) */
      await p.evaluate(js => { try { (0, eval)(js) } catch (e) { console.warn(e) } }, c.js); await p.clock.runFor(100); await wait(c.jswait || 800) }   /* (not longer: the page's timers under the test's clock would run without end with a dialog open) */
    if (c.act == 'two') { for (const k of ['notepad', 'calc']) { await p.evaluate(k => launch(k), k); for (let i = 0; i < 4; i++) { await p.clock.runFor(400); await wait(150) } } await p.mouse.move(W / 2, H - 100) }
    /* (the title bars: a row of the window's menu from a right-click on its title bar; the pointer over one of its buttons) */
    const tbAt = (dx, dy) => p.evaluate(([dx, dy]) => { const w = [...document.querySelectorAll('.win.act')].find(e => e.offsetParent); const r = w.getBoundingClientRect(); return [Math.round(r.left + dx), Math.round(r.top + dy)] }, [dx, dy]);
    if (c.tbmenu) { const q = await tbAt(200, 17); await p.mouse.click(q[0], q[1], {button: 'right'}); await p.clock.runFor(300); await wait(300);
      const it = await p.evaluate(l => { const e = [...document.querySelectorAll('.ctx .cmi')].find(x => x.querySelector('span').textContent == l); if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + 40, r.top + r.height / 2] }, c.tbmenu);
      if (it) await p.mouse.click(it[0], it[1]); else await p.keyboard.press('Escape'); await p.clock.runFor(300); await wait(300); await p.mouse.move(W / 2, H - 200); await wait(200) }
    if (c.after) for (let i = 0; i < c.after / 500; i++) { await p.clock.runFor(500); await wait(40) }   /* (1.6.1 Part 25: the time the case's work has run) */
    if (c.hoverbtn !== undefined) { const q = await p.evaluate(i => { const w = [...document.querySelectorAll('.win.act')].find(e => e.offsetParent); const b = w.querySelectorAll('.tb button')[i].getBoundingClientRect(), r = w.getBoundingClientRect(); return [b.left + b.width / 2, b.top + b.height / 2, Math.round(b.left + b.width / 2 - r.left), Math.round(b.top + b.height / 2 - r.top)] }, c.hoverbtn); hov = q.slice(2); await p.mouse.move(q[0], q[1]); await p.clock.runFor(200); await wait(300) }
    if (c.key == 'altab') { await p.keyboard.down('Alt'); await p.keyboard.press('Tab'); await p.clock.runFor(200); await wait(200) }
    if (c.key == 'overview') { await p.keyboard.press('Meta+Tab'); for (let i = 0; i < 4; i++) { await p.clock.runFor(200); await wait(150) } }
    if (c.ws) { await p.click('#wsw button:nth-child(' + c.ws + ')'); for (let i = 0; i < 2; i++) { await p.clock.runFor(300); await wait(150) } await p.mouse.move(W / 2, H / 3); await wait(300) }
    if (c.act == 'lock') { await p.evaluate(() => typeof lockScreen == 'function' ? lockScreen() : showLogin()); for (let i = 0; i < 4; i++) { await p.clock.runFor(400); await wait(150) } }
    if (c.files) { await p.evaluate(() => { const d = deskFiles(); d['REPORT.TXT'] = 'hello'; d['PROJECTS'] = {}; fsSave(); EXPL.forEach(f => f()) }); for (let i = 0; i < 4; i++) { await p.clock.runFor(400); await wait(150) } }
    const icAt = n => p.evaluate(n => { const e = [...document.querySelectorAll('#desk > .ic')].find(x => (x.dataset.fn || x.querySelector('span').textContent) == n); const r = e.getBoundingClientRect(); return [Math.round(r.left + r.width / 2), Math.round(r.top + 20)] }, n);
    if (c.act == 'deskmenu') { pt = [1500, 500]; await p.mouse.click(pt[0], pt[1], {button: 'right'}); await p.clock.runFor(300); await wait(200) }
    if (c.act == 'filemenu') { pt = await icAt('REPORT.TXT'); await p.mouse.click(pt[0], pt[1], {button: 'right'}); await p.clock.runFor(300); await wait(200) }
    if (c.act == 'rename') { await p.evaluate(() => renameDlg(homePath('DESKTOP').concat('REPORT.TXT'))); await p.clock.runFor(300); await wait(200) }
    if (c.act == 'openwith') { await p.evaluate(() => openWith(homePath('DESKTOP').concat('REPORT.TXT'))); await p.clock.runFor(300); await wait(200) }
    if (c.rc) { const at = await p.evaluate(([r, H]) => { const [k, a] = r.split(':'); let e = null;
        if (k == 'bar') return [1000, H - 21];
        if (k == 'start') e = document.getElementById('start'); if (k == 'vol' || k == 'net') e = document.getElementById(k); if (k == 'clock') e = document.getElementById('tray');
        if (k == 'pin') e = [...document.querySelectorAll('#pins > *')].find(b => b.title == APPS[a].name);
        if (k == 'win') e = [...document.querySelectorAll('#tasks button')].find(b => { const W = winOf(b); return W && W.app == a });
        const q = e.getBoundingClientRect(); return [Math.round(q.left + q.width / 2), Math.round(q.top + q.height / 2)] }, [c.rc, H]);
      pt = at; await p.mouse.click(at[0], at[1], {button: 'right'}); await p.clock.runFor(300); await wait(200); await p.mouse.move(300, 300); await p.clock.runFor(200); await wait(100) }
    if (c.act == 'wgpick') { await p.mouse.click(1500, 500, {button: 'right'}); await p.clock.runFor(300); await wait(200);
      const r = await p.evaluate(() => { const e = [...document.querySelectorAll('.ctx .cmi')].find(x => x.textContent.includes('Add a widget')); const q = e.getBoundingClientRect(); return [q.left + 50, q.top + q.height / 2] });
      await p.mouse.click(r[0], r[1]); await p.clock.runFor(300); await wait(200) }
    if (/menu$|wgpick/.test(c.act || '')) await p.mouse.move(W / 2, H - 200);
    /* (idleCheck, which the page's 15 s timer runs, called at once: the page's clock is the test's, and the saver's frames, timers of
       no delay under it, are not run: its first picture) */
    if (c.saver) { fs.writeFileSync(path.join(D, 'idle'), '3600000'); await p.evaluate(() => idleCheck()); await wait(1500) }
    if (c.ev) { for (const e of c.ev) { const n0 = await p.evaluate(() => document.querySelectorAll('#ntoasts .ntoast').length); fs.writeFileSync(path.join(D, 'event'), e);
        await p.waitForFunction(n0 => document.querySelectorAll('#ntoasts .ntoast').length > n0, n0, {timeout: 20000}) } await wait(900) }
    if (c.drag) { const r = await rectOf('.win'); if (r) { const a = [r[0] + 100, r[1] + 15], b = [c.drag[0] + 100, c.drag[1] + 15]; await p.mouse.move(a[0], a[1]); await p.mouse.down();
        for (let i = 1; i <= 10; i++) { await p.mouse.move(a[0] + (b[0] - a[0]) * i / 10, a[1] + (b[1] - a[1]) * i / 10); await p.clock.runFor(16); await wait(30) } await p.mouse.up(); await p.mouse.move(W / 2, H / 3); await p.clock.runFor(100); await wait(400) } }
    if (c.fly) { await p.click({clock: '#tray', vol: '#vol', net: '#net', qs: '#qsb', notes: '#ncb'}[c.fly]); await p.clock.runFor(400); await wait(300);
      if (c.next) { await p.click('#calp .cn'); await p.clock.runFor(300); await wait(200) } await p.mouse.move(300, 300) }
  }
  rects.screen = [0, 0, W, H]; rects.wallpaper = [0, 0, W, H];
  rects.bar = await rectOf('#bar'); rects.start = await rectOf('#start'); rects.pins = await rectOf('#pins'); rects.tray = await rectOf('#tray'); rects.clock = await rectOf('#tray .clk, #clock, #tray');
  rects.menu = await rectOf('#menu'); rects.signbox = (await rectOf('#lockov .lkbox')) || (await rectOf('#login .lb'));
  rects.icons = await p.evaluate(() => { const l = [...document.querySelectorAll('#desk > .ic')].filter(e => e.offsetParent).map(e => e.getBoundingClientRect()); if (!l.length) return null;
    const x = Math.min(...l.map(r => r.left)), y = Math.min(...l.map(r => r.top)); return [Math.round(x), Math.round(y), Math.round(Math.max(...l.map(r => r.right)) - x), Math.round(Math.max(...l.map(r => r.bottom)) - y)] });
  rects.title = await p.evaluate(() => { const v = [...document.querySelectorAll('.win')].filter(e => e.offsetParent && e.getBoundingClientRect().width), w = v.find(e => e.classList.contains('act')) || v.pop();   /* (the window in use: Settings makes other .win of its own) */ const t = w && w.querySelector('.tb'); if (!t) return null; const r = w.getBoundingClientRect(), q = t.getBoundingClientRect(); return [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(q.bottom - r.top)] });
  rects.titlei = await p.evaluate(() => { const v = [...document.querySelectorAll('.win')].filter(e => e.offsetParent && e.getBoundingClientRect().width), w = v.find(e => !e.classList.contains('act')), a = v.find(e => e.classList.contains('act')); const t = w && w.querySelector('.tb'); if (!t) return null;
    const r = w.getBoundingClientRect(), q = t.getBoundingClientRect(), o = a && a.getBoundingClientRect(); return [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(Math.min(q.bottom, o && o.top > r.top ? o.top - 5 : 1e9) - r.top)] });   /* (another window's, not in use: above the one in use, which may cover its bar's foot) */
  { const r = await rectOf('.ctx'); rects.ctx = r ? [r[0] - 4, r[1] - 4, r[2] + 8, r[3] + 8] : null; rects.dlg = await rectOf('.rmodal .rmbox') }   /* (a menu with its focus ring) */
  for (let i = 0; i < (c.wg || 0); i++) rects['wg' + i] = await p.evaluate(i => { const e = document.querySelectorAll('#desk > .wgt')[i]; if (!e) return null; const r = e.getBoundingClientRect(); return [Math.round(r.left) - 20, Math.round(r.top) - 10, Math.round(r.width) + 40, Math.round(r.height) + 40] }, i);   /* (with its shadow) */
  { const r = await rectOf('#ntoasts'); rects.toast = r ? [r[0] - 30, r[1] - 16, r[2] + 42, r[3] + 52] : null }   /* (with their shadows) */
  { const r = await rectOf('#calp') || await rectOf('.svp') || await rectOf('#qsp') || await rectOf('#ncp') || await rectOf('.np'); rects.fly = r }
  rects.win = await p.evaluate(() => { const w = [...document.querySelectorAll('.win')].filter(e => e.offsetParent).pop(); if (!w) return null; const r = w.getBoundingClientRect(); return [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(r.height)] });
  rects.altab = await rectOf('#altab'); rects.desk = await rectOf('#desk'); rects.tbw = await rectOf('#tasks button.tbpon');   /* (1.6.1 Part 25: the button with the fill) */
  rects.desc = await p.evaluate(() => ({bar: document.getElementById('bar').className, body: document.body.className}));
  const read = c.read ? await p.evaluate(js => { try { return String((0, eval)(js)) } catch (e) { return 'ERROR ' + e.message } }, c.read) : null;
  if (c.readx) XTRA.std.readx = await p.evaluate(js => { try { return String((0, eval)(js)) } catch (e) { return 'ERROR ' + e.message } }, c.readx);
  rects.statue = await rectOf('#dynstatue img');
  const f = path.join(OUT, c.n + '.std.png'); await p.screenshot({path: f});
  if (c.moving) { for (let i = 0; i < c.moving / 250; i++) { await p.clock.runFor(250); await wait(60) } await p.screenshot({path: path.join(OUT, c.n + '.std2.png')}) } if (c.saver) fs.unlinkSync(path.join(D, 'idle'));
  if (c.key == 'altab') await p.keyboard.up('Alt');
  try { fs.unlinkSync(path.join(D, 'std-on')) } catch (e) {}
  await ctx.close(); return {f, rects, errs, pt, read, hov};
}
// ---- Native: rudolf-native under Xvfb, the same storage
let NP = null, NLOG = path.join(OUT, 'native.log');
function nlines0(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
function nst(k) { try { const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => x.startsWith(k + ' ')); return l ? l.slice(k.length + 1) : null } catch (e) { return null } }
function nhit(k) { try { const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] } catch (e) { return null } }
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
function natStart() {
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), RUDOLF_NATIVE_TEST_EVAL: TEVAL(), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC', RUDOLF_WX_BASE: BASE, RUDOLF_WX_GEO: BASE,
    LD_PRELOAD: process.env.SAME_FAKETIME || '', FAKETIME: '2026-11-14 09:41:00', FAKETIME_DONT_FAKE_MONOTONIC: '1', FAKETIME_NO_CACHE: '1'});
  if (!env.LD_PRELOAD) delete env.LD_PRELOAD;
  for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY', 'all_proxy', 'ALL_PROXY']) delete env[k];   /* (Open-Meteo is the test's own) */
  const lg = fs.openSync(NLOG, 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
}
const xdo = a => sh('xdotool ' + a);
/* (1.6.0 Part 12, the test findings) a script run in an app window's page on Native (rudolf-native's RUDOLF_NATIVE_TEST_EVAL: "KEY JS"
   lines on a FIFO, "KEY RESULT" lines back in FIFO.out), as Standard's is run in its page */
let TEV = null, TEVN = 0;
function TEVAL() { if (!TEV) { TEV = path.join(D, 'teval'); try { fs.unlinkSync(TEV) } catch (e) {} cp.execSync('mkfifo ' + TEV) } return TEV }
async function neval(k, js) {
  const o = TEV + '.out', got = () => { try { return fs.readFileSync(o, 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }, n0 = got().length;
  const fd = fs.openSync(TEV, fs.constants.O_RDWR | fs.constants.O_NONBLOCK); fs.writeSync(fd, k + ' ' + js.replace(/\n/g, ' ') + '\n'); fs.closeSync(fd);
  await until(() => got().length > n0, 8000); const r = got()[n0]; return r == null ? null : r.slice(k.length + 1);
}

async function natSettle(c) {
  const n0 = (calls().match(/GET desktop\/settings/g) || []).length;
  await until(() => (calls().match(/GET desktop\/settings/g) || []).length > n0 + 1, 15000); await wait(900);
}
/* the files of the desktop-files cases, made as anyone would: the desktop's menu, New text document and New folder, Rename */
let NATFILES = 0;
async function natFiles() {
  if (NATFILES) return; NATFILES = 1;
  const has = n => { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').some(x => x.startsWith('dicon ') && x.endsWith('|' + n)) } catch (e) { return false } };
  for (const [item, name] of [['New text document', 'REPORT.TXT'], ['New folder', 'PROJECTS']]) {
    xdo('mousemove 1500 500 click 3'); await until(() => nst('cmenu') != null, 5000);
    const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => x.startsWith('cmitem ') && x.endsWith('|' + item)); if (!l) continue; const a = l.split(' ');
    xdo('mousemove ' + a[1] + ' ' + a[2] + ' click 1'); await until(() => (nst('dialog') || '').startsWith('rename'), 30000); await wait(400);
    xdo('type --delay 30 ' + name); xdo('key Return'); await until(() => (nst('dialog') || '').startsWith('-'), 30000); await until(() => has(name), 15000); await wait(800);
  }
  xdo('mousemove ' + W / 2 + ' ' + Math.round(H / 3)); await wait(300);
}
async function natCase(c, s) {
  try { fs.unlinkSync(path.join(D, 'std-on')) } catch (e) {}
  state(c.set, c.more); micSet(c); rdSet(c);
  if (c.signin) {
    try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
    if (NP) { NP.kill(); NP = null; await wait(500) }
    natStart(); await until(() => nst('signin') == '1', 40000); await wait(1500);
  } else if (!NP || nst('user') == '-' || nst('user') == null) {
    if (!NP) { natStart(); await until(() => nst('signin') == '1', 40000) }
    await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
    await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(2500);
  } else await natSettle(c);
  // (what is open from before goes)
  xdo('mousemove ' + W / 2 + ' ' + Math.round(H / 3)); await wait(300);
  if (c.act == 'menu') { const h = nhit('start'); if (h) xdo('mousemove ' + h.join(' ') + ' click 1'); await until(() => nst('menu') == '1', 5000); xdo('mousemove ' + W / 2 + ' ' + Math.round(H / 3)); await wait(800)
    if (c.type) { xdo('type --delay 60 ' + c.type); await until(() => nlines0('srchcur').some(l => l.endsWith(' 1')) && nlines0('srch').length, 20000); await wait(1500) } }
  if (c.act == 'hover') { const h = nhit('pin notepad'); if (h) xdo('mousemove ' + h.map(Math.round).join(' ')); await wait(800) }
  let cl = null;
  if (c.act == 'notepad' && c.wopen) { const h = nhit('pin notepad'), st = () => nst('wopen') || '0'; if (h) xdo('mousemove ' + h.join(' ') + ' click 1');
    const T = Date.now(); while (st() == '0' && Date.now() - T < 30000) await wait(5); const t0 = Date.now(); sh('xwd -silent -root > ' + path.join(OUT, 'wopen.mid.xwd'));
    while (st() != '0' && Date.now() - t0 < 3000) await wait(5); const wo = XTRA.nat.wopen = {ms: Date.now() - t0}; await wait(400); sh('import -window root ' + path.join(OUT, 'wopen.end.png'));
    const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => /^client .* app=notepad/.test(x)); if (l) { const a = l.split(' '); wo.r = [+a[3], +a[4], +a[5] + 2, +a[6] + 2] }
    for (const k of ['mid']) sh('convert xwd:' + path.join(OUT, 'wopen.' + k + '.xwd') + ' ' + path.join(OUT, 'wopen.' + k + '.png') + ' && rm -f ' + path.join(OUT, 'wopen.' + k + '.xwd')); cl = wo.r ? [wo.r[0], wo.r[1], wo.r[2] - 2, wo.r[3] - 2] : null; await wait(2500) }
  else if (c.act == 'notepad') { const h = nhit('pin notepad'); if (h) xdo('mousemove ' + h.join(' ') + ' click 1');
    await until(() => /^client .* app=notepad/m.test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 30000); await wait(3000); xdo('mousemove ' + W / 2 + ' ' + (H - 200)); await wait(500);
    const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => /^client .* app=notepad/.test(x)); if (l) { const a = l.split(' '); cl = [+a[3], +a[4], +a[5], +a[6]] } }
  for (const k of c.pre || []) { const re = new RegExp('^client .* app=' + k + ' ', 'm'), h = nhit('pin ' + k); if (h) xdo('mousemove ' + h.map(Math.round).join(' ') + ' click 1'); await until(() => re.test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 30000); await wait(2500) }
  if (c.app) { const re = new RegExp('^client .* app=' + c.app + ' ', 'm'), h = nhit('pin ' + c.app); if (h) xdo('mousemove ' + h.map(Math.round).join(' ') + ' click 1');
    await until(() => re.test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 30000); await wait(3500); xdo('mousemove ' + W / 2 + ' ' + (H - 200)); await wait(500);
    const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => re.test(x)); if (l) { const a = l.split(' '); cl = [+a[3], +a[4], +a[5], +a[6]] } }
  if (c.appctx && cl) { const r = (await neval(c.app, "(() => { const w = OA.main.el; const e = [...w.querySelectorAll('tr,.fi,.row')].find(x => x.textContent.includes(" + JSON.stringify(c.appctx) + ") && x.getClientRects().length); const b = e.getBoundingClientRect(); return Math.round(b.left + 20) + ' ' + Math.round(b.top + b.height / 2) })()") || '0 0').split(' ').map(Number);
    xdo('mousemove ' + (cl[0] + 1 + r[0]) + ' ' + (cl[1] + 35 + r[1]) + ' click 3'); await until(() => nlines0('cmenu').length, 6000); xdo('mousemove ' + W / 2 + ' ' + (H - 100)); await wait(700) }
  if (c.js) { await neval(c.app, 'void((() => { try { ' + c.js + ' } catch (e) { console.warn(e) } })())'); await wait(Math.max(2500, c.jswait || 0)) }
  if (c.act == 'two') { for (const k of ['notepad', 'calc']) { const h = nhit('pin ' + k); if (h) xdo('mousemove ' + h.map(Math.round).join(' ') + ' click 1');
      await until(() => new RegExp('^client .* app=' + k + ' ', 'm').test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 30000); await wait(3000) } xdo('mousemove ' + W / 2 + ' ' + (H - 100)); await wait(500) }
  if (c.tbmenu && cl) { xdo('mousemove ' + (cl[0] + 200) + ' ' + (cl[1] + 17) + ' click 3'); await until(() => nlines0('cmitem').length, 8000); await wait(300);
    const l = nlines0('cmitem').find(x => x.endsWith('|' + c.tbmenu)); if (l) { const a = l.split(' '); xdo('mousemove ' + a[1] + ' ' + a[2] + ' click 1') } else xdo('key Escape'); await wait(1200); xdo('mousemove ' + W / 2 + ' ' + (H - 200)); await wait(500);
    const l2 = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => new RegExp('^client .* app=' + c.app + ' ').test(x)); if (l2) { const a = l2.split(' '); cl = [+a[3], +a[4], +a[5], +a[6]] } }   /* (its place after: Maximize) */
  if (c.after) await wait(c.after);   /* (1.6.1 Part 25) */
  if (c.hoverbtn !== undefined && cl && s.hov) { xdo('mousemove ' + (cl[0] + s.hov[0]) + ' ' + (cl[1] + s.hov[1])); await wait(700) }
  let cli = null; if (c.pre && c.pre.length) { const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => new RegExp('^client .* app=' + c.pre[0] + ' ').test(x)); if (l) { const a = l.split(' '); cli = [+a[3], +a[4]] } }
  if (c.key == 'altab') { xdo('keydown alt key Tab'); await until(() => (nst('altab') || '-') != '-', 5000); await wait(500) }
  if (c.key == 'overview') { xdo('key super+Tab'); await until(() => /^ovwin .* 1$/m.test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 5000); await wait(600) }
  if (c.saver) { fs.writeFileSync(path.join(D, 'idle'), '3600000'); await until(() => (nst('saver') || '').startsWith('1') && /^client .* full=1 .* app=do-saver /m.test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 40000); await wait(2500) }
  if (c.mic) { await until(() => (nst('mic') || '').startsWith(c.mic.length + ' ' + (c.micmuted ? 1 : 0)), 12000); await wait(600) }
  if (c.ws) { const h = nhit('ws ' + c.ws); if (h) xdo('mousemove ' + h.map(Math.round).join(' ') + ' click 1'); await until(() => nst('ws') == String(c.ws) && nlines0('toast').length > 0, 8000); xdo('mousemove ' + W / 2 + ' ' + Math.round(H / 3)); await wait(700) }
  if (c.ev) { for (const e of c.ev) { const n0 = nlines0('toast').length; fs.writeFileSync(path.join(D, 'event'), e); await until(() => nlines0('toast').length > n0, 20000) } xdo('mousemove ' + W / 2 + ' ' + Math.round(H / 3)); await wait(900) }
  if (c.drag && cl) { const a = [cl[0] + 100, cl[1] + 15], b = [c.drag[0] + 100, c.drag[1] + 15]; xdo('mousemove ' + a.join(' ') + ' mousedown 1');
    for (let i = 1; i <= 10; i++) { xdo('mousemove ' + Math.round(a[0] + (b[0] - a[0]) * i / 10) + ' ' + Math.round(a[1] + (b[1] - a[1]) * i / 10)); await wait(30) } xdo('mouseup 1'); xdo('mousemove ' + W / 2 + ' ' + Math.round(H / 3)); await wait(500) }
  if (c.act == 'lock') { xdo('key super+l'); await until(() => nst('locked') == '1', 8000); await wait(1200) }
  if (c.files) await natFiles();
  const nlines = k => { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } };
  const cmClick = lbl => { const l = nlines('cmitem').find(x => x.endsWith('|' + lbl)); if (l) { const a = l.split(' '); xdo('mousemove ' + a[1] + ' ' + a[2] + ' click 1') } return !!l };
  const fileAt = () => { const l = nlines('dicon').find(x => x.endsWith('|REPORT.TXT')); if (!l) return null; const a = l.split(' '); return [+a[3] + 42, +a[4] + 20] };
  if (c.rc) { const [k, a] = c.rc.split(':'); let q = k == 'bar' ? [1000, H - 21] : k == 'pin' ? nhit('pin ' + a) : k == 'win' ? null : nhit(k + ' -');
    if (k == 'win') { const cl = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => new RegExp('^client .* app=' + a + ' ').test(x)); if (cl) q = nhit('win ' + cl.split(' ')[1]) }
    if (q && s.pt && Math.abs(q[0] - s.pt[0]) < 6 && Math.abs(q[1] - s.pt[1]) < 6) q = s.pt;   /* (the same pixel as Standard's click, when the part is there) */
    if (q) xdo('mousemove ' + q.map(Math.round).join(' ') + ' click 3'); await until(() => nlines('cmenu').length, 8000); xdo('mousemove 300 300'); await wait(600) }
  if (c.act == 'wgpick') { xdo('mousemove 1500 500 click 3'); await until(() => nlines('cmenu').length, 5000); cmClick('Add a widget…'); await until(() => (nlines('cmenu')[0] || '').endsWith('Add a widget'), 5000); xdo('mousemove ' + W / 2 + ' ' + (H - 200)); await wait(500) }
  if (c.act == 'deskmenu') { xdo('mousemove 1500 500 click 3'); await until(() => nlines('cmenu').length, 5000); xdo('mousemove ' + W / 2 + ' ' + (H - 200)); await wait(500) }
  if (c.act == 'filemenu') { const q = s.pt || fileAt(); if (q) xdo('mousemove ' + q.join(' ') + ' click 3'); await until(() => nlines('cmenu').length, 5000); xdo('mousemove ' + W / 2 + ' ' + (H - 200)); await wait(500) }
  if (c.act == 'rename' || c.act == 'openwith') { const q = fileAt(); if (q) xdo('mousemove ' + q.join(' ') + ' click 3'); await until(() => nlines('cmenu').length, 5000);
    cmClick(c.act == 'rename' ? 'Rename' : 'Open with…'); await until(() => (nst('dialog') || '').startsWith(c.act == 'rename' ? 'rename' : 'openwith'), 20000); xdo('mousemove ' + W / 2 + ' ' + (H - 100)); await wait(700) }
  if (c.fly) { const q = nhit(c.fly + ' -'); if (q) xdo('mousemove ' + q.map(Math.round).join(' ') + ' click 1'); await until(() => (nst('fly') || '-') != '-', 8000); await wait(500);
    if (c.next) { const l = nlines('flybtn').find(x => x.startsWith('flybtn next ')); if (l) { const a = l.split(' '); xdo('mousemove ' + a[2] + ' ' + a[3] + ' click 1') } await wait(700) }
    xdo('mousemove 300 300'); await wait(500) }
  const read = c.read ? await neval(c.app, 'String((() => { try { return ' + c.read + ' } catch (e) { return "ERROR " + e.message } })())') : null;
  if (c.readx) XTRA.nat.readx = await neval(c.app, 'String((() => { try { return ' + c.readx + ' } catch (e) { return "ERROR " + e.message } })())');
  const f = path.join(OUT, c.n + '.nat.png'); sh('import -window root ' + f);
  if (c.moving) { await wait(c.moving); sh('import -window root ' + path.join(OUT, c.n + '.nat2.png')) }
  if (c.undo) { await neval(c.app, 'void((() => { try { ' + c.undo + ' } catch (e) {} })())'); await wait(1200) }
  if (c.saver) { fs.writeFileSync(path.join(D, 'idle'), '0'); await until(() => (nst('saver') || '').startsWith('0') && !/ app=do-saver /.test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 10000); fs.unlinkSync(path.join(D, 'idle')); await wait(800) }
  if (c.key == 'altab') { xdo('keyup alt'); await until(() => (nst('altab') || '-') == '-', 5000); await wait(400) }
  if (c.key == 'overview') { xdo('key Escape'); await until(() => nst('overview') == '0', 5000); await wait(400) }
  if (c.act == 'two') for (let i = 0; i < 2; i++) {   /* (the one in front first: its ✕ may be over the other's) */
    const L = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => /^client .* app=(notepad|calc) /.test(x)), l = L.find(x => / focus=1 /.test(x)) || L[0]; if (!l) break; const k = l.match(/ app=(\S+)/)[1];
    if (l) { const a = l.split(' '); xdo('mousemove ' + (+a[3] + +a[5] + 2 - 9 - 12) + ' ' + (+a[4] + 7 + 11) + ' click 1') }
    await until(() => !new RegExp('^client .* app=' + k + ' ', 'm').test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 10000); await wait(800) }
  if (c.fly) { xdo('mousemove 300 300 click 1'); await until(() => (nst('fly') || '-') == '-', 5000); await wait(300) }
  // (back as it was for the next case: the menu closed, the app window closed, unlocked)
  if (c.act == 'menu' || c.rc || c.appctx || /menu$|rename|openwith|wgpick/.test(c.act || '')) { xdo('key Escape'); await wait(400) }
  if (c.ws) { const h = nhit('ws 1'); if (h) xdo('mousemove ' + h.map(Math.round).join(' ') + ' click 1'); await until(() => nst('ws') == '1', 5000); await wait(1800) }   /* (back to workspace 1, its pop-up gone) */
  if (c.app) { const re = new RegExp('^client .* app=' + c.app + ' ', 'm'); const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => re.test(x));
    if (l) { const a = l.split(' '); xdo('mousemove ' + (+a[3] + +a[5] + 2 - 9 - 12) + ' ' + (+a[4] + 7 + 11) + ' click 1') }
    await until(() => !re.test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 10000); await wait(800) }
  for (const k of (c.pre || []).slice().reverse()) { const re = new RegExp('^client .* app=' + k + ' ', 'm'); const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => re.test(x));
    if (l) { const a = l.split(' '); xdo('mousemove ' + (+a[3] + +a[5] + 2 - 9 - 12) + ' ' + (+a[4] + 7 + 11) + ' click 1') }
    await until(() => !re.test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 10000); await wait(800) }
  if (c.act == 'notepad') {   /* (its ✕: 9 px from the frame's right edge, 7 px down, 24 by 22) */
    const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => /^client .* app=notepad/.test(x));
    if (l) { const a = l.split(' '); xdo('mousemove ' + (+a[3] + +a[5] + 2 - 9 - 12) + ' ' + (+a[4] + 7 + 11) + ' click 1') }
    await until(() => !/^client .* app=notepad/m.test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 10000); await wait(800) }
  if (c.act == 'lock') { xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return'); await until(() => nst('locked') == '0', 8000); await wait(800) }
  return {f, client: cl, clienti: cli, read};
}
// ---- the comparison
/* (the share of a screenshot in testsrc2's colour bars: the video's picture, not a black screen) */
const PICPY = "import sys\nfrom PIL import Image\nd=Image.open(sys.argv[1]).convert('RGB').resize((480,270)).tobytes()\nB=[(191,191,0),(0,191,191),(0,191,0),(191,0,191),(191,0,0),(0,0,191),(255,255,0),(0,255,255),(0,255,0),(255,0,255),(255,0,0),(0,0,255)]\n" +
  "n=sum(1 for i in range(0,len(d),3) if any(abs(d[i]-b[0])<48 and abs(d[i+1]-b[1])<48 and abs(d[i+2]-b[2])<48 for b in B))\nprint(round(100*n/(480*270)))";
function cmp(name, a, b, ra, rb, shift) {
  const o = path.join(OUT, name);
  try { const r = JSON.parse(cp.execFileSync('python3', [path.join(__dirname, 'pxdiff.py'), a, b, ra.join(','), (rb || ra).join(','), String(TOL), o, String(shift || 0)]).toString()); return r }
  catch (e) { return {pct: 100, err: String(e.message).slice(0, 300)} }
}
(async () => {
  /* (second run, N23) with Chromium's scrollbars, as Rudolf's Chromium shows them (Playwright hides them unless told) */
  BR = await chromium.launch({ignoreDefaultArgs: ['--hide-scrollbars']});
  const rows = [];
  for (const c of CASES) {
    if (only && !only.includes(c.n)) continue;
    if (c.media && !fs.existsSync(path.join(D, 'tm', c.media))) { console.log('SKIP ' + c.n + ': no test media (ffmpeg)'); continue }
    let s, n;
    try { s = await stdCase(c) } catch (e) { ok(c.n + ': Standard drew it', 0, e.message); continue }
    const log0 = (() => { try { return fs.statSync(NLOG).size } catch (e) { return 0 } })();
    try { n = await natCase(c, s) } catch (e) { ok(c.n + ': Native drew it', 0, e.message); continue }
    if (c.nolog) { let t = ''; try { t = fs.readFileSync(NLOG, 'utf8').slice(log0) } catch (e) {} const m = t.split('\n').filter(l => c.nolog.test(l)); ok(c.n + ': nothing in Native\'s log like ' + c.nolog, !m.length, m.slice(0, 3)) }
    if (c.read && c.expect) ok(c.n + ': each page reads what its engine should (' + c.expect.std + ' / ' + c.expect.nat + ')', s.read === c.expect.std && n.read === c.expect.nat, {std: s.read, nat: n.read});
    else if (c.read) ok(c.n + ': the page reads the same on both (' + c.read.slice(0, 60) + '…)', s.read === n.read, {std: s.read, nat: n.read});
    for (const part of c.parts || []) {
      if (part == 'moving') { const mv = e => { try { return JSON.parse(cp.execFileSync('python3', [path.join(__dirname, 'pxdiff.py'), path.join(OUT, c.n + '.' + e + '.png'), path.join(OUT, c.n + '.' + e + '2.png'), [0, 0, W, H].join(','), [0, 0, W, H].join(','), String(TOL), path.join(OUT, c.n + '.moving.' + e), '0']).toString()).pct } catch (x) { return -1 } };
        const a = mv('std'), b = mv('nat'); ok(c.n + ' moving: ' + a + ' % of Standard\'s screen and ' + b + ' % of Native\'s change in ' + c.moving / 1000 + ' s (at least 0.25 % on both)', a >= .25 && b >= .25, {a, b}); continue }
      if (part == 'picture') { const f = x => +cp.execFileSync('python3', ['-c', PICPY, x]).toString(), a = f(s.f), b = f(n.f);
        ok(c.n + ' picture: ' + a + ' % of Standard\'s screen and ' + b + ' % of Native\'s show the video (at least 60 %)', a >= 60 && b >= 60 && Math.abs(a - b) <= 10, {a, b}); continue }
      if (part == 'frames') { let a = {}, b = {}; try { a = JSON.parse(XTRA.std.readx) } catch (e) {} try { b = JSON.parse(XTRA.nat.readx) } catch (e) {}
        ok(c.n + ': frame callbacks ' + a.cb + ' on Standard and ' + b.cb + ' on Native (at least 60 each); frames counted at the end ' + a.end + ' and ' + b.end + ' (within 10)', a.cb >= 60 && b.cb >= 60 && a.end > 0 && Math.abs(a.end - b.end) <= 10, XTRA); continue }
      if (part == 'wopen') { const w = XTRA.nat.wopen || {}; const d = w.r ? cmp(c.n + '.wopen', path.join(OUT, 'wopen.mid.png'), path.join(OUT, 'wopen.end.png'), w.r, w.r, 0) : {pct: 0};
        ok(c.n + ': Standard animates its window (' + XTRA.std.wopen + '); Native\'s animation ran ' + w.ms + ' ms (100-600) with ' + d.pct + ' % of the window not yet itself at its start (at least 3 %)', /^true litepoint 200ms$/.test(XTRA.std.wopen || '') && w.ms >= 100 && w.ms <= 600 && d.pct >= 3, {std: XTRA.std.wopen, nat: w, d}); continue }
      let ra = s.rects[part], rb = null;
      if ((part == 'title' || part == 'win') && n.client && ra) rb = [n.client[0], n.client[1], ra[2], ra[3]];
      if (part == 'titlei' && n.clienti && ra) rb = [n.clienti[0], n.clienti[1], ra[2], ra[3]];   /* (each engine's own window; the frame's border and title bar above the app) */
      if (!ra) { ok(c.n + ' ' + part + ': Standard has it', 0, s.rects); continue }
      const r = cmp(c.n + '.' + part, s.f, n.f, ra, rb, part == 'win' ? 1 : 0);   /* (a whole window: its page drawn by WebKit, a pixel's match one pixel away allowed) */
      rows.push([c.n, part, r.pct]);
      ok(c.n + ' ' + part + ': ' + (r.pct != null ? r.pct.toFixed(2) : '?') + ' % of the pixels differ (at most ' + LIMIT + ' %)', r.pct <= LIMIT, r);
    }
  }
  fs.writeFileSync(path.join(OUT, 'summary.tsv'), rows.map(r => r.join('\t')).join('\n') + '\n');
  if (NP) NP.kill(); await BR.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.stack); if (NP) NP.kill(); process.exit(1) });
