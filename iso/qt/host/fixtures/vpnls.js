// 1.6.2 (docs/plan-1.6.2.md Part 33): an lsync copy (the page's localStorage, as rudolfd keeps it) with the accounts
// the VPN test signs in as, made exactly as the page makes them (core25.js pwMake): Ann (an administrator), Bob (a
// standard account) and Kim (a standard account with parental controls: a.pc).   node vpnls.js OUT.json
const {webcrypto: c} = require('crypto'), fs = require('fs'); const s = c.subtle;
const b64e = u => Buffer.from(u instanceof ArrayBuffer ? new Uint8Array(u) : u).toString('base64'), b64d = x => new Uint8Array(Buffer.from(x, 'base64'));
const bits = async (pw, salt, n) => s.deriveBits({name: 'PBKDF2', salt: b64d(salt), iterations: 210000, hash: 'SHA-256'}, await s.importKey('raw', new TextEncoder().encode(pw), 'PBKDF2', false, ['deriveBits']), n);
const pwMake = async pw => { const sl = b64e(c.getRandomValues(new Uint8Array(16))); return 'p2$' + sl + '$' + b64e(await bits(pw, sl, 256)) };
(async () => {
  const accts = [];
  for (const [n, pw, r, id, pc] of [['Ann', 'pw-ann', 'admin', 'uAnnA1b2'], ['Bob', 'pw-bob', 'user', 'uBobC3d4'], ['Kim', 'pw-kim', 'user', 'uKimE5f6', {mins: 120, block: []}]]) {
    const a = {n, h: await pwMake(pw), r, id}; if (pc) a.pc = pc; accts.push(a);
  }
  fs.writeFileSync(process.argv[2], JSON.stringify({gen: 7, data: {bd95user: JSON.stringify({accts}), bd95setup: '1'}}));
})();
