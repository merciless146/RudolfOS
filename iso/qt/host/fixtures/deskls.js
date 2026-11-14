// 1.5.9 (plan Part 12): an lsync copy (the page's localStorage, as rudolfd keeps it) with accounts made exactly as the
// page makes them (core25.js pwMake, acct50.js acctNew: Node's WebCrypto is the browser's), and what each account's
// keys must be, for deskapi59_t.sh.   node deskls.js OUT.json KEYS.json
const {webcrypto: c} = require('crypto'), fs = require('fs'); const s = c.subtle;
const b64e = u => Buffer.from(u instanceof ArrayBuffer ? new Uint8Array(u) : u).toString('base64'), b64d = x => new Uint8Array(Buffer.from(x, 'base64'));
const hex = u => Buffer.from(u instanceof ArrayBuffer ? new Uint8Array(u) : u).toString('hex');
const IT = 210000;
const bits = async (pw, salt, n) => s.deriveBits({name: 'PBKDF2', salt: b64d(salt), iterations: IT, hash: 'SHA-256'}, await s.importKey('raw', new TextEncoder().encode(pw), 'PBKDF2', false, ['deriveBits']), n);
const pwMake = async pw => { const sl = b64e(c.getRandomValues(new Uint8Array(16))); return 'p2$' + sl + '$' + b64e(await bits(pw, sl, 256)) };
const enc = async (key, bytes) => { const iv = c.getRandomValues(new Uint8Array(12)); return {iv: b64e(iv), ct: b64e(await s.encrypt({name: 'AES-GCM', iv}, key, bytes))} };
const hdKey = async (raw, id) => hex(await s.deriveBits({name: 'HKDF', hash: 'SHA-512', salt: new TextEncoder().encode('rudolf-os home folder'), info: new TextEncoder().encode(id)}, await s.importKey('raw', raw, 'HKDF', false, ['deriveBits']), 512));
(async () => {
  const K = {}, accts = [];
  for (const [n, pw, r, disk, id] of [['Ann', 'pw-ann ü', 'admin', 0, 'uAnnA1b2'], ['Bob', 'bob-pass', 'user', 1, 'uBobC3d4']]) {
    const ks = b64e(c.getRandomValues(new Uint8Array(16))), pk = await s.importKey('raw', await bits(pw, ks, 256), {name: 'AES-GCM'}, false, ['encrypt', 'decrypt']);
    const raw = c.getRandomValues(new Uint8Array(32));
    const a = {n, h: await pwMake(pw), r, id, ks, fk: await enc(pk, raw), last: r == 'admin' ? 1000 : 2000}; if (disk) a.disk = 1;
    accts.push(a); K[n] = {id, pw, pk: hex(await bits(pw, ks, 256)), fk: hex(raw), home: await hdKey(raw, id)};
  }
  accts.push({n: 'Old', h: 'a'.repeat(64), r: 'user', id: 'uOld00000'});
  accts.push({n: 'Keyed', h: await pwMake('k'), r: 'user', id: 'uKey00000', ks: 'AAAAAAAAAAAAAAAAAAAAAA==', pk: {jwk: {}, alg: -7}});
  const set = {bs: 'solid', b1: '#112233', b2: '#000080', tb: 'top', accent: '#16a34a', tbstyle: 'candy', tbac: true, tbacm: 'comp', twstyle: 'retro', twacm: 'negative', h24: true, dyn: 'alpine', dm: 'dark', ignored: 'x'};
  const data = {bd95user: JSON.stringify({accts}), bd95setup: '1', bd95sys: JSON.stringify({guest: true, ssmin: 10}),
    'bd95set:uAnnA1b2': JSON.stringify(set), bd95set: JSON.stringify({bs: 'solid', b1: '#445566', tb: 'bottom'}),
    'bd95pins:uAnnA1b2': JSON.stringify(['notepad', 'calc']), 'bd95notif:uAnnA1b2': JSON.stringify([{t: 'a'}, {t: 'b'}, {t: 'c'}]), other: '"\\u2028 \\ud83d\\ude00"'};
  fs.writeFileSync(process.argv[2], JSON.stringify({gen: 42, data})); fs.writeFileSync(process.argv[3], JSON.stringify(K));
  // test vectors for the crypto alone
  const v = {sha256: hex(await s.digest('SHA-256', new TextEncoder().encode('abc'))), sha512: hex(await s.digest('SHA-512', new TextEncoder().encode('abc'))),
    pbkdf2: hex(await bits('password', b64e(new TextEncoder().encode('saltsalt')), 256)), hkdf: await hdKey(new Uint8Array(32).fill(7), 'uX'),
    gcmkey: hex(new Uint8Array(32).fill(9))};
  const gk = await s.importKey('raw', new Uint8Array(32).fill(9), {name: 'AES-GCM'}, false, ['encrypt']), box = await enc(gk, new TextEncoder().encode('forty-two bytes of plain text, and a few!'));
  v.gcmiv = box.iv; v.gcmct = box.ct; v.gcmpt = hex(new TextEncoder().encode('forty-two bytes of plain text, and a few!'));
  fs.writeFileSync(process.argv[3].replace(/\.json$/, '.vec.json'), JSON.stringify(v));
})();
