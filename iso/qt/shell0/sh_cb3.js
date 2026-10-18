const {list, conn} = require('./cdp.js');
(async () => {
  const ts = await list(); const desk = ts.find(t => t.url.startsWith('http://127.0.0.1:8080'));
  const d = await conn(desk.webSocketDebuggerUrl);
  const ev = async (e, g) => (await d.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true, userGesture: !!g})).result.result.value;
  console.log('frame focused:', await ev('document.hasFocus()'));
  console.log('copy command:', await ev(`(()=>{const t=document.createElement('textarea');t.value='rudolf-copy';document.body.appendChild(t);t.select();let ok;try{ok=document.execCommand('copy')}catch(e){ok='ERR '+e.message}t.remove();return ok})()`, true));
  // keyboard paste into a text field: the copied text should arrive
  await ev(`(()=>{const t=document.createElement('textarea');t.id='pt';t.style.cssText='position:fixed;left:10px;top:10px;z-index:2147483647';document.body.appendChild(t);t.focus();return 1})()`);
  await d.send('Input.dispatchKeyEvent', {type: 'keyDown', modifiers: 2, key: 'v', code: 'KeyV', windowsVirtualKeyCode: 86, commands: ['paste']});
  await d.send('Input.dispatchKeyEvent', {type: 'keyUp', modifiers: 2, key: 'v', code: 'KeyV', windowsVirtualKeyCode: 86});
  await new Promise(r => setTimeout(r, 500));
  console.log('Ctrl+V pasted:', await ev(`document.getElementById('pt').value`));
  console.log('async read after copy:', await ev(`navigator.clipboard.readText().catch(e=>'ERR '+e.message)`, true));
  d.close();
})();
