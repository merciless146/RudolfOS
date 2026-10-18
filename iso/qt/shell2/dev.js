// evaluate in the desktop page: node dev.js 'EXPR'
const {list, conn} = require('./cdp.js');
(async () => {
  const desk = (await list()).find(t => t.url.startsWith('http://127.0.0.1:8080'));
  const d = await conn(desk.webSocketDebuggerUrl);
  const r = await d.send('Runtime.evaluate', {expression: process.argv[2], awaitPromise: true, returnByValue: true});
  console.log(JSON.stringify(r.result.exceptionDetails || r.result.result.value)); process.exit(0);
})();
