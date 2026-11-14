// 1.6.0 (plan Part 12): the state same_t.js draws with both engines: the fixture's storage (fixtures/deskls.js) with Ann
// and Bob, Ann signed in last, her pinned apps, and each case's settings over the base ones.   (a module of same_t.js)
const fs = require('fs'), path = require('path');
const ACCT = 'uAnnA1b2';
const BASESET = {bs: 'solid', b1: '#1e3a5f', b2: '#000080', tb: 'bottom', accent: '#2563eb', h24: false, df: 'mdy', tz: 'UTC', icons: 'rudolf'};
let GEN = 1000, LS0 = null;
function base(D) {
  const ls = JSON.parse(fs.readFileSync(path.join(D, 'ls.base.json')));
  const u = JSON.parse(ls.data.bd95user); u.accts = u.accts.filter(a => a.n == 'Ann' || a.n == 'Bob'); u.accts.forEach(a => a.last = a.n == 'Ann' ? 3000 : 1000); ls.data.bd95user = JSON.stringify(u);
  ls.data['bd95pins:' + ACCT] = JSON.stringify(['explorer', 'notepad', 'calc', 'settings']);
  ls.data['bd95notif:' + ACCT] = JSON.stringify([]);
  return ls;
}
// write DIR/ls.json: the base state with SET's settings (and MORE: other keys of the storage)
function state(D, set, more) {
  if (!fs.existsSync(path.join(D, 'ls.base.json'))) fs.copyFileSync(path.join(D, 'ls.json'), path.join(D, 'ls.base.json'));
  if (!LS0) LS0 = base(D);
  const ls = JSON.parse(JSON.stringify(LS0)); let g = 0; try { g = +JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).gen || 0 } catch (e) {} ls.gen = Math.max(g, ++GEN) + 1; const s = Object.assign({}, BASESET, set || {});
  ls.data['bd95set:' + ACCT] = ls.data.bd95set = JSON.stringify(s);
  for (const k in (more || {})) ls.data[k] = JSON.stringify(more[k]);
  fs.writeFileSync(path.join(D, 'ls.json.t'), JSON.stringify(ls)); fs.renameSync(path.join(D, 'ls.json.t'), path.join(D, 'ls.json'));
}
module.exports = {state, ACCT, BASESET};
if (require.main === module) state(process.argv[2], JSON.parse(process.argv[3] || '{}'), JSON.parse(process.argv[4] || '{}'));
