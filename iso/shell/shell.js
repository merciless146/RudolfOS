/* Rudolf OS shell (an Isolated Web App, installed by Chromium's policy and started by the session).
   The desktop page runs unchanged in a full-screen Controlled Frame; Oracle's tabs will be Controlled Frames too,
   placed over Oracle's windows, so websites get real, first-party browser pages. GPL-2.0-or-later.

   Messages (window.postMessage):
     shell -> desktop  {rudolfShell: 'hello', version}             after every load of the desktop, every second until
                                                                   the desktop answers {rudolfShell: 'ready'}
                       {rudolfShell: 'newwindow', url}              the desktop asked for a new browser window
     desktop -> shell  {rudolfShell: 'ping', id}  ->  {rudolfShell: 'pong', id}
                       {rudolfShell: 'lock', id}  ->  {rudolfShell: 'locked', id, ok}   pointer capture for games: the
                       pointer cannot be captured inside the desktop's frame, so the shell captures it on the frame
                       and sends {rudolfShell: 'mm', dx, dy}, {'mb', b, down}, {'mw', dy}; {'lockchange', locked}
                       {rudolfShell: 'unlock'}
   Only the desktop frame (http://127.0.0.1:8080) is listened to. */
'use strict';
const DESK_URL = 'http://127.0.0.1:8080/';
const DESK_ORIGIN = 'http://127.0.0.1:8080';
const VERSION = '1.2.0';
const desk = document.getElementById('desk');
const starting = document.getElementById('starting');

const toDesk = m => { try { desk.contentWindow.postMessage(m, DESK_ORIGIN) } catch (e) {} };

/* permissions the desktop may have; it asks the person itself where that matters (camera, screen sharing) */
const ALLOW = new Set(['media', 'fullscreen', 'pointerLock', 'download', 'geolocation']);
desk.addEventListener('permissionrequest', e => {
  try { ALLOW.has(e.permission) ? e.request.allow() : e.request.deny() } catch (x) {}
});
/* the desktop opens websites itself (Oracle, the browser window); anything else that asks for a window goes there */
desk.addEventListener('newwindow', e => {
  const url = e.targetUrl || '';
  try { e.window.discard() } catch (x) {}
  if (/^https?:/i.test(url)) toDesk({rudolfShell: 'newwindow', url});
});
let helloTimer = null, ready = false;
desk.addEventListener('loadstop', () => {
  starting.hidden = true; ready = false; desk.focus();
  clearInterval(helloTimer);
  const hello = () => { if (ready) clearInterval(helloTimer); else toDesk({rudolfShell: 'hello', version: VERSION}) };
  hello(); helloTimer = setInterval(hello, 1000);
});
/* rudolfd may not be listening yet right after start-up: try again */
desk.addEventListener('loadabort', e => {
  if (e.isTopLevel === false) return;
  starting.hidden = false; setTimeout(() => { desk.src = DESK_URL }, 2000);
});
addEventListener('focus', () => desk.focus());

/* pointer capture for games (Quake): movement, buttons and the wheel go to the desktop while captured */
let captured = false;
document.addEventListener('pointerlockchange', () => { captured = document.pointerLockElement === desk; toDesk({rudolfShell: 'lockchange', locked: captured}) });
document.addEventListener('mousemove', e => { if (captured) toDesk({rudolfShell: 'mm', dx: e.movementX, dy: e.movementY}) });
document.addEventListener('mousedown', e => { if (captured) { e.preventDefault(); toDesk({rudolfShell: 'mb', b: e.button, down: true}) } });
document.addEventListener('mouseup', e => { if (captured) toDesk({rudolfShell: 'mb', b: e.button, down: false}) });
document.addEventListener('wheel', e => { if (captured) toDesk({rudolfShell: 'mw', dy: e.deltaY}) }, {passive: true});

const handlers = {
  ready: () => { ready = true; clearInterval(helloTimer) },
  lock: async (m, reply) => { try { await desk.requestPointerLock(); reply({rudolfShell: 'locked', id: m.id, ok: true}) } catch (e) { reply({rudolfShell: 'locked', id: m.id, ok: false, error: String(e && e.message || e)}) } },
  unlock: () => { if (document.pointerLockElement) document.exitPointerLock() },
  ping: (m, reply) => reply({rudolfShell: 'pong', id: m.id}),
};
addEventListener('message', e => {
  if (e.origin !== DESK_ORIGIN || e.source !== desk.contentWindow) return;
  const m = e.data; if (!m || typeof m.rudolfShell !== 'string' || !handlers[m.rudolfShell]) return;
  handlers[m.rudolfShell](m, r => toDesk(r));
});

desk.src = DESK_URL;
