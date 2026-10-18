// wait (up to N seconds, default 90) until the shell and the desktop inside it are up; prints the targets
const {list} = require('./cdp.js');
(async () => {
  const end = Date.now() + 1000 * (+process.argv[2] || 90);
  let l = [];
  while (Date.now() < end) {
    try { l = await list() } catch (e) { l = [] }
    if (l.some(t => t.url.startsWith('isolated-app://')) && l.some(t => t.url.startsWith('http://127.0.0.1:8080'))) break;
    await new Promise(r => setTimeout(r, 1000));
  }
  console.log(l.map(t => t.type + ' ' + t.url).join('\n') || 'nothing');
})();
