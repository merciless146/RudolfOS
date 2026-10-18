// Phase 0 test shell: the desktop in a full-screen controlled frame; permissions and messages are logged in window.L
window.L = {perm: [], msg: [], newwin: [], ev: []};
const desk = document.getElementById('desk');
desk.addEventListener('permissionrequest', e => { L.perm.push(e.permission); try { e.request.allow() } catch (x) { L.perm.push('allow failed: ' + x.message) } });
desk.addEventListener('newwindow', e => { L.newwin.push(e.targetUrl) });
desk.addEventListener('loadstop', () => { L.ev.push('loadstop'); try { desk.contentWindow.postMessage({fromShell: 'hello'}, '*'); L.ev.push('posted') } catch (x) { L.ev.push('post failed: ' + x.message) } });
addEventListener('message', e => { L.msg.push(JSON.stringify(e.data).slice(0, 200)) });
desk.src = 'http://127.0.0.1:8080/';
