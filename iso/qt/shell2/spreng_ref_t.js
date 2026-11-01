// 1.4.5: SpreadsheetPro's formula engine against more than 500 results worked out with Excel's rules (spreng_ref.py).
// node spreng_ref_t.js   (python3 spreng_ref.py makes spreng_ref.json)
const fs = require('fs'), path = require('path'), vm = require('vm');
const ctx = {}; vm.createContext(ctx); vm.runInContext(fs.readFileSync(path.join(__dirname, '../../../desktop/spreng45.js'), 'utf8') + ';this.SPE=SPE', ctx); const SPE = ctx.SPE;
const cases = JSON.parse(fs.readFileSync(path.join(__dirname, 'spreng_ref.json'), 'utf8')); let pass = 0; const bad = [];
for (const [f, want, cells] of cases) {
  const s = {name: 'Sheet1', id: 1, cells: new Map()}; for (const [a, v] of Object.entries(cells)) { const p = SPE.parseA1(a); s.cells.set(p.r * SPE.MAXC + p.c, {v}) }
  const got = SPE.evalIn({sheets: [s], names: {}}, 0, f);
  const same = typeof want == 'number' && typeof got == 'number' ? Math.abs(got - want) <= 1e-9 * Math.max(1, Math.abs(want)) : got === want;
  if (same) pass++; else bad.push([f, want, got]) }
bad.slice(0, 30).forEach(b => console.log('FAIL', JSON.stringify(b)));
console.log(`${pass}/${cases.length} passed`); process.exit(bad.length ? 1 : 0);
