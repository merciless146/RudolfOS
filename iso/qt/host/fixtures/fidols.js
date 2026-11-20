// 1.6.1 (docs/plan-1.6.1.md Part 18): an lsync copy with accounts made as the page makes them (acct50.js acctNew: the
// password's key, the file key sealed with it, an administrator's key pair with its private key sealed too), and what
// each account's keys must be, for fido161_t.sh.   node fidols.js OUT.json KEYS.json
const {webcrypto: c} = require('crypto'), fs = require('fs'); const s = c.subtle;
const b64e = u => Buffer.from(u instanceof ArrayBuffer ? new Uint8Array(u) : u).toString('base64'), b64d = x => new Uint8Array(Buffer.from(x, 'base64'));
const hex = u => Buffer.from(u instanceof ArrayBuffer ? new Uint8Array(u) : u).toString('hex');
const bits = async (pw, salt, n) => s.deriveBits({name: 'PBKDF2', salt: b64d(salt), iterations: 210000, hash: 'SHA-256'}, await s.importKey('raw', new TextEncoder().encode(pw), 'PBKDF2', false, ['deriveBits']), n);
const pwMake = async pw => { const sl = b64e(c.getRandomValues(new Uint8Array(16))); return 'p2$' + sl + '$' + b64e(await bits(pw, sl, 256)) };
const enc = async (key, bytes) => { const iv = c.getRandomValues(new Uint8Array(12)); return {iv: b64e(iv), ct: b64e(await s.encrypt({name: 'AES-GCM', iv}, key, bytes))} };
(async () => {
  const K = {}, accts = [];
  for (const [n, pw, r, id] of [['Ann', 'pw-ann ü', 'admin', 'uAnnA1b2'], ['Cat', 'cat-pass', 'user', 'uCatE5f6'], ['Dan', 'dan-pass', 'user', 'uDanG7h8']]) {
    const ks = b64e(c.getRandomValues(new Uint8Array(16))), pkb = await bits(pw, ks, 256), pk = await s.importKey('raw', pkb, {name: 'AES-GCM'}, false, ['encrypt', 'decrypt']);
    const raw = c.getRandomValues(new Uint8Array(32)); const a = {n, h: await pwMake(pw), r, id, ks, fk: await enc(pk, raw), last: accts.length};
    K[n] = {id, pw, pk: hex(pkb), fk: hex(raw)};
    if (r == 'admin') { const kp = await s.generateKey({name: 'ECDH', namedCurve: 'P-256'}, true, ['deriveBits']); a.pub = await s.exportKey('jwk', kp.publicKey);
      const pv = new Uint8Array(await s.exportKey('pkcs8', kp.privateKey)); a.priv = await enc(pk, pv); K[n].pv = b64e(pv) }
    accts.push(a);
  }
  fs.writeFileSync(process.argv[2], JSON.stringify({gen: 1, data: {bd95user: JSON.stringify({accts}), bd95setup: '1', bd95sys: JSON.stringify({guest: true})}}));
  fs.writeFileSync(process.argv[3], JSON.stringify(K));
})();
