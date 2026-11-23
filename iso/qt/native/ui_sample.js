// 1.6.2 (docs/plan-1.6.2.md Part 39, step 1): the page's common controls, as librudolf-ui's sample window (iso/sys/ui/ui-sample.c)
// shows them natively: the same words, the same order. Put into a window of the desktop page by ui_t.js (Chromium), so the page's own
// styles draw them (its cascade, its dark theme, its accent colour, the Font settings); each control is named (data-rui) for its rect.
// (a module of ui_t.js)
const ROWS = [['chrome', '1234', '182.4 MB'], ['rudolfd', '412', '9.8 MB'], ['Xorg', '388', '61.0 MB']];
const HTML = `<div class='tmtabs' role='tablist' data-rui='tabs'><button data-t='a' role='tab' class='on'>Apps</button><button data-t='b' role='tab'>Processes</button><button data-t='c' role='tab'>Performance</button></div>
<div class='tmb' style='flex:1;overflow:hidden;padding:8px'>
<div class='rmrow' style='justify-content:flex-start;margin-top:0;margin-bottom:6px'><input data-rui='field' placeholder='Find a process' aria-label='Find a process' style='flex:0 1 220px'><button data-rui='btn'>End process</button><button data-rui='btnd' disabled>End now</button><button data-rui='pri' class='pri'>Report a problem…</button></div>
<div class='rmrow' style='justify-content:flex-start;margin-top:0;margin-bottom:6px'><input data-rui='fieldv' value='chrome' style='flex:0 1 220px'></div>
<label class='ck' data-rui='check'><input type='checkbox' checked> Reopen my apps when I sign in</label>
<h4 class='sh4' data-rui='h4'>Apps reopened at sign-in</h4>
<table class='tmt' data-rui='table'><thead><tr><th>Name</th><th style='text-align:right'>PID</th><th style='text-align:right'>Memory</th></tr></thead><tbody>${ROWS.map((r, i) => `<tr${i == 1 ? " class='on'" : ''}><td>${r[0]}</td><td style='text-align:right'>${r[1]}</td><td style='text-align:right'>${r[2]}</td></tr>`).join('')}</tbody></table>
<div class='msg' data-rui='msg'>3 window(s) · 212 processes · Rudolf OS on this computer</div>
<div data-rui='scroll' style='height:70px;overflow:auto;border:1px solid var(--bd);margin-top:8px'>${Array.from({length: 12}, (_, i) => `<div style='padding:2px 6px'>Line ${i + 1} of the list</div>`).join('')}</div>
</div>`;
// the menu: the page's cmenu, as Task Manager's process rows open it
const MENU = [['End process', true], ['End process tree', true], '-', ['End now', false]];
module.exports = {HTML, ROWS, MENU};
