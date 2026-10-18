// tiny CDP client (Node's built-in WebSocket)
const base = 'http://127.0.0.1:9333';
async function list() { return (await fetch(base + '/json/list')).json() }
function conn(url) {
  return new Promise((res, rej) => {
    const ws = new WebSocket(url); let id = 0; const wait = new Map(); const ev = [];
    ws.onmessage = m => { const d = JSON.parse(m.data); if (d.id && wait.has(d.id)) { wait.get(d.id)(d); wait.delete(d.id) } else ev.push(d) };
    ws.onerror = e => rej(e);
    ws.onopen = () => res({send: (method, params = {}) => new Promise(r => { const i = ++id; wait.set(i, r); ws.send(JSON.stringify({id: i, method, params})) }), close: () => ws.close(), ev});
  });
}
async function evalIn(t, expr, gesture) {
  const c = await conn(t.webSocketDebuggerUrl);
  const r = await c.send('Runtime.evaluate', {expression: expr, awaitPromise: true, returnByValue: true, userGesture: !!gesture});
  c.close(); return r.result && r.result.result ? r.result.result.value : r;
}
async function browserWs() { return (await (await fetch(base + '/json/version')).json()).webSocketDebuggerUrl }
module.exports = {list, conn, evalIn, browserWs};
