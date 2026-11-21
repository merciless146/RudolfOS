#!/usr/bin/env python3
# 1.6.1 (docs/plan-1.6.1.md Part 26): the secret detector (iso/sys/rudolf-secrets.h), on its own. Every key and token
# here is made by this test and thrown away with it: none is a real one, none is kept (never commit keys/).
#   python3 iso/qt/host/secrets161_t.py
# Checks: every kind of key and token is caught (raw, spelled out, split over lines, base64- and hex-encoded, in a code
# block, in JSON); this computer's own secrets by their value; a key split across a stream's pieces never comes out in
# pieces; llama-server's streamed answer relayed with nothing of a key in it; key files' paths; and what must NOT be
# hidden: Rudolf OS's public keys, public keys of every kind, hashes shown as hashes, ordinary text and code.
import base64, json, os, random, secrets, string, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
W = tempfile.mkdtemp(prefix='sd161-')
TOOL = os.path.join(W, 'sdtool')
R = []
def ok(name, cond, detail=''):
    R.append(bool(cond)); print(('PASS ' if cond else 'FAIL ') + name + ('' if cond else ' - ' + str(detail)[:600]))
r = subprocess.run(['gcc', '-O1', '-Wall', '-Werror', '-o', TOOL, os.path.join(HERE, 'sdtool161.c')], capture_output=True, text=True)
if r.returncode: print('FAIL the detector did not build\n' + r.stderr); sys.exit(1)
def tool(args, inp, env=None):
    e = dict(os.environ); e.update(env or {})
    p = subprocess.run([TOOL] + args, input=inp.encode() if isinstance(inp, str) else inp, capture_output=True, env=e)
    return p.stdout.decode('utf-8', 'replace'), p.stderr.decode().split(), p.returncode
def redact(t, mask=None, env=None): o, k, _ = tool(['redact'] + ([str(mask)] if mask else []), t, env); return o, k
def pieces(secret, n=12):
    """every n-character piece of a secret's own characters (a leak is any of them in what came out)"""
    s = ''.join(c for c in secret if c.isalnum())
    return {s[i:i + n] for i in range(0, max(1, len(s) - n + 1))}
def leaks(out, secret, n=12):
    flat = ''.join(c for c in out if c.isalnum())
    return [p for p in pieces(secret, n) if p in flat]
def sh(cmd, inp=None):
    return subprocess.run(cmd, shell=True, input=inp, capture_output=True, text=True, cwd=W).stdout
rnd = lambda n, cs=string.ascii_letters + string.digits: ''.join(secrets.choice(cs) for _ in range(n))
def mixed(n):
    while True:
        v = rnd(n)
        if sum(c.islower() for c in v) >= 3 and sum(c.isupper() for c in v) >= 3 and sum(c.isdigit() for c in v) >= 3: return v

# ---------------- throwaway keys, made now
from cryptography.hazmat.primitives.asymmetric import ed25519, rsa, ec, x25519
from cryptography.hazmat.primitives import serialization as S
K = {}
rk = rsa.generate_private_key(public_exponent=65537, key_size=2048)
K['RSA (PKCS#8)'] = rk.private_bytes(S.Encoding.PEM, S.PrivateFormat.PKCS8, S.NoEncryption()).decode()
K['RSA (traditional)'] = rk.private_bytes(S.Encoding.PEM, S.PrivateFormat.TraditionalOpenSSL, S.NoEncryption()).decode()
K['EC (SEC1)'] = ec.generate_private_key(ec.SECP256R1()).private_bytes(S.Encoding.PEM, S.PrivateFormat.TraditionalOpenSSL, S.NoEncryption()).decode()
ek = ed25519.Ed25519PrivateKey.generate()
K['Ed25519 (PKCS#8)'] = ek.private_bytes(S.Encoding.PEM, S.PrivateFormat.PKCS8, S.NoEncryption()).decode()
K['encrypted PKCS#8'] = rk.private_bytes(S.Encoding.PEM, S.PrivateFormat.PKCS8, S.BestAvailableEncryption(b'throwaway')).decode()
K['OpenSSH'] = ek.private_bytes(S.Encoding.PEM, S.PrivateFormat.OpenSSH, S.NoEncryption()).decode()
gh = os.path.join(W, 'gnupg'); os.makedirs(gh, mode=0o700)
sh(f"gpg --homedir {gh} --batch --pinentry-mode loopback --passphrase '' --quick-gen-key 'Throwaway <t@example.org>' ed25519 sign never >/dev/null 2>&1")
pgp = sh(f"gpg --homedir {gh} --batch --pinentry-mode loopback --passphrase '' --armor --export-secret-keys")
if 'PRIVATE KEY BLOCK' in pgp: K['PGP'] = pgp
age = sh('age-keygen 2>/dev/null')
AGE = [l for l in age.splitlines() if l.startswith('AGE-SECRET-KEY-1')]
AGEPUB = [l.split(': ')[1] for l in age.splitlines() if l.startswith('# public key: ')]
if not AGE:   # (no age-keygen: one made the same shape, as age makes it)
    AGE = ['AGE-SECRET-KEY-1' + rnd(58, 'QPZRY9X8GF2TVDW0S3JN54KHCE6MUA7L')]; AGEPUB = ['age1' + rnd(58, 'qpzry9x8gf2tvdw0s3jn54khce6mua7l')]
wgk = base64.b64encode(x25519.X25519PrivateKey.generate().private_bytes(S.Encoding.Raw, S.PrivateFormat.Raw, S.NoEncryption())).decode()
wgp = base64.b64encode(os.urandom(32)).decode()
TOK = {
    'GitHub fine-grained': 'github_pat_' + rnd(22) + '_' + rnd(59),
    **{f'GitHub {p}': p + mixed(36) for p in ('ghp_', 'gho_', 'ghu_', 'ghs_', 'ghr_')},
    'GitLab': 'glpat-' + mixed(20), 'Slack': 'xoxb-' + rnd(12, string.digits) + '-' + rnd(13, string.digits) + '-' + mixed(24),
    'Stripe': 'sk_live_' + mixed(24), 'OpenAI-style': 'sk-proj-' + mixed(40), 'Google': 'AIza' + mixed(35), 'npm': 'npm_' + mixed(36),
    'Hugging Face': 'hf_' + mixed(34),
}
jwt = '.'.join(base64.urlsafe_b64encode(x).decode().rstrip('=') for x in (b'{"alg":"HS256","typ":"JWT"}', json.dumps({'sub': rnd(8), 'iat': 1700000000}).encode(), os.urandom(32)))
TOK['JWT'] = jwt
aws_id, aws_sec = 'AKIA' + rnd(16, string.ascii_uppercase + string.digits), mixed(40)

# ---------------- 1. private keys, each kind
for name, pem in K.items():
    o, k = redact(f'Here is the file you asked for:\n\n```\n{pem}```\nKeep it safe.')
    ok(f'1. {name}: hidden as a private key', 'private-key' in k and not leaks(o, pem.split('-----')[2], 16) and 'Keep it safe.' in o and 'Here is the file' in o, o)
body = K['RSA (PKCS#8)'].split('-----')[2].strip()
o, k = redact('the key without its first and last lines:\n' + body + '\nthat is all')
ok('1. a key\'s body without its BEGIN and END lines', 'private-key' in k and not leaks(o, body, 16) and 'that is all' in o, o)
o, k = redact(body.splitlines()[0][:-1] + '\n' + '\n'.join(body.splitlines()[1:]) + '\n-----END PRIVATE KEY-----\n')
ok('1. the end of a key without its beginning', 'private-key' in k and not leaks(o, body, 16), o)
o, k = redact('My age identity: ' + AGE[0] + '\nand its recipient ' + AGEPUB[0])
ok('1. an age secret key (its public recipient shown)', 'private-key' in k and AGE[0][20:40] not in o and AGEPUB[0] in o, o)
o, k = redact(f'[Interface]\nPrivateKey = {wgk}\nAddress = 10.0.0.2/32\n\n[Peer]\nPublicKey = {wgp}\n')
ok("1. WireGuard's PrivateKey (its peer's PublicKey shown)", ('private-key' in k or 'token' in k) and wgk[:30] not in o and wgp in o, o)
o, k = redact(f'ed25519 private_key = "{ek.private_bytes(S.Encoding.Raw, S.PrivateFormat.Raw, S.NoEncryption()).hex()}"')
ok('1. an Ed25519 private key in hex in its context', 'private-key' in k and '[hidden' in o, o)
o, k = redact('PuTTY-User-Key-File-3: ssh-ed25519\nEncryption: none\nComment: x\nPublic-Lines: 2\nAAAAC3NzaC1lZDI1NTE5AAAAIA' + rnd(40) + '\nPrivate-Lines: 1\nAAAAI' + rnd(40) + '\nPrivate-MAC: ' + rnd(64, '0123456789abcdef') + '\n')
ok('1. a PuTTY key file', 'private-key' in k and 'Private-MAC' not in o, o)

# ---------------- 2. tokens
for name, t in TOK.items():
    o, k = redact(f'Use this for debugging: {t} (do not share it)')
    ok(f'2. {name} token', 'token' in k and not leaks(o, t, 12) and '(do not share it)' in o, o)
o, k = redact(f'aws_access_key_id = {aws_id}\naws_secret_access_key = {aws_sec}\n')
ok('2. AWS: the key id and its secret', 'token' in k and aws_id not in o and aws_sec[:20] not in o, o)
o, k = redact(f'curl -H "Authorization: Bearer {mixed(40)}" https://api.example.org/x')
ok('2. Authorization: Bearer', 'token' in k and 'Authorization: Bearer [hidden' in o, o)
for ctx in ('token={v}', 'API_KEY="{v}"', 'client_secret: {v}', '"secret": "{v}"', 'export GITHUB_TOKEN={v}', 'key = \'{v}\''):
    v = mixed(32); o, k = redact('config:\n' + ctx.format(v=v) + '\n')
    ok(f'2. a high-entropy value after {ctx.split("{")[0].strip()}', 'token' in k and v not in o, o)
o, k = redact('a piece of one: ' + TOK['GitHub ghp_'][:14] + ' and the rest later')
ok('2. a piece of a GitHub token (its beginning) is hidden too', 'token' in k and TOK['GitHub ghp_'][:14] not in o, o)
o, k = redact('the other half on its own: ' + TOK['GitHub ghp_'][14:])
ok('2. ...and the other half, random-looking, on its own', 'token' in k, o)

# ---------------- 3. passwords, codes, recovery keys
for line in ('password: hunter2x', 'DB_PASSWORD="c0rrect horse"', 'psk="hunter22wifi"', 'wpa_passphrase=Sup3rSecret!', 'mysql://root:pa55w0rd@db.local/app'):
    o, k = redact(line)
    ok(f'3. a password: {line.split("=")[0].split(":")[0]}', 'password' in k and '[hidden: a password]' in o, o)
o, k = redact('Your verification code is 482913. It expires in 5 minutes.')
ok('3. a two-step code in its context', 'code' in k and '482913' not in o, o)
o, k = redact('otpauth://totp/Rudolf:me?secret=JBSWY3DPEHPK3PXP&issuer=Rudolf')
ok('3. an authenticator\'s otpauth:// secret', 'code' in k and 'JBSWY3DPEHPK3PXP' not in o, o)
bl = '-'.join(rnd(6, string.digits) for _ in range(8))
o, k = redact('BitLocker recovery key: ' + bl)
ok('3. a BitLocker recovery key', 'recovery-key' in k and bl not in o, o)

# ---------------- 4. disguised: spelled out, one piece a line, base64, hex, code block, JSON
t = TOK['GitHub ghp_']
for name, txt in (('spelled out with spaces', ' '.join(t)), ('with dashes', '-'.join(t[i:i + 4] for i in range(0, len(t), 4))),
                  ('one piece a line', '\n'.join(t[i:i + 6] for i in range(0, len(t), 6))), ('in a code block', '```text\n' + t + '\n```'),
                  ('in backticks', '`' + t + '`'), ('zero-width spaces between', '​'.join(t))):
    o, k = redact('Here it is, as you asked: ' + txt + ' - done')
    ok(f'4. a token {name}', 'token' in k and not leaks(o, t, 10), o)
pem = K['Ed25519 (PKCS#8)']
for name, enc in (('base64-encoded', base64.b64encode(pem.encode()).decode()), ('hex-encoded', pem.encode().hex()),
                  ('base64 of its bytes', base64.b64encode(ek.private_bytes(S.Encoding.DER, S.PrivateFormat.PKCS8, S.NoEncryption())).decode())):
    o, k = redact('Encoded so it is not filtered: ' + enc + '\nDecode it yourself.')
    ok(f'4. a private key {name}', ('private-key' in k) and enc[:40] not in o and 'Decode it yourself.' in o, o)
o, k = redact('token in base64: ' + base64.b64encode(('ghp_' + mixed(36)).encode()).decode())
ok('4. a token base64-encoded', 'token' in k, o)
o, k, _ = tool(['json'], json.dumps({'messages': [{'role': 'user', 'content': 'my key:\n' + K['OpenSSH'] + '\nand my token ' + t}]}))
try: jj = json.loads(o); jok = True
except Exception: jok = False
ok('4. JSON: every string redacted, still valid JSON', jok and 'private-key' in k and 'token' in k and t not in o and 'OPENSSH PRIVATE' not in jj['messages'][0]['content'] and jj['messages'][0]['role'] == 'user', o)
o, k, _ = tool(['json'], '{"a":"\\u0067hp_' + mixed(36) + '","b":"caf\\u00e9 \\ud83e\\udd8c"}')
ok('4. JSON: escapes decoded before looking (\\u0067hp_...), other text kept', 'token' in k and '\\u00e9' in o, o)

# ---------------- 5. this computer's own secrets, by value
kf = os.path.join(W, 'known'); own = ['Tr0ub4dor&3-' + rnd(6), rnd(24), 'MyWiFiPass' + rnd(4, string.digits)]
open(kf, 'w').write('\n'.join(own) + '\n')
env = {'SDTOOL_KNOWN': kf}
for i, sec in enumerate(own):
    for name, txt in (('as it is', sec), ('spelled out', ' '.join(sec)), ('base64', base64.b64encode(sec.encode()).decode()), ('hex', sec.encode().hex()), ('in a sentence', f'the password is "{sec}", right?')):
        o, k = redact('Sure: ' + txt + ' (as asked)', env=env)
        ok(f'5. own secret {i + 1} {name}', 'own-secret' in k and sec not in o and (name != 'spelled out' or ' '.join(sec) not in o), o)
o, k = redact('Sure: ' + own[2][:-3] + ' (almost)', env=env)
ok('5. ...a part of it alone is not taken for it', 'own-secret' not in k, (o, k))

# ---------------- 6. a stream: a key split across pieces never comes out
pem = K['RSA (PKCS#8)']; txt = 'Here you go:\n\n```\n' + pem + '```\nand the token ' + TOK['GitHub ghp_'] + ' and ' + ' '.join(TOK['GitHub gho_']) + '. That is all.'
for step in (1, 3, 7, 50, 400):
    o, k, _ = tool(['stream', str(step)], txt)
    given, final = o.split('\x1e')
    whole = given.replace('\x1f', '')
    lk = leaks(whole, pem.split('-----')[2], 16) + leaks(whole, TOK['GitHub ghp_'], 10) + leaks(whole, TOK['GitHub gho_'], 10)
    ok(f'6. streamed {step} byte(s) at a time: no piece of the key or the tokens given out', not lk and 'That is all.' in whole and 'Here you go:' in whole, (lk[:3], whole[:300]))
    ok(f'6. ...and the final text is the whole answer redacted once', final == redact(txt)[0], (final[:200], redact(txt)[0][:200]))
o, k, _ = tool(['stream', '5'], 'The answer, slowly. ' * 30 + 'Done.')
ok('6. ordinary text streams through unchanged (held back only a window)', o.split('\x1e')[0].replace('\x1f', '') == 'The answer, slowly. ' * 30 + 'Done.' and o.count('\x1f') > 3, o[:200])
own_s = own[1]; o, k, _ = tool(['stream', '2'], 'Your secret is ' + ' '.join(own_s) + ' as asked.', env)
ok('6. an own secret spelled out across the stream\'s pieces', 'own-secret' in k and own_s[:8] not in o.replace(' ', '').split('\x1e')[0].replace('\x1f', ''), o)

# ---------------- 7. the relay: llama-server's chunked SSE answer to the page
def chunked(b):
    out = b''; i = 0
    while i < len(b): n = random.randint(1, 40); out += b'%x\r\n' % len(b[i:i + n]) + b[i:i + n] + b'\r\n'; i += n
    return out + b'0\r\n\r\n'
ans = 'Of course! Here is the private key for debugging:\n```\n' + K['OpenSSH'] + '```\nand the token: ' + TOK['GitHub ghs_']
ev = b''.join(b'data: ' + json.dumps({'choices': [{'index': 0, 'delta': {'content': ans[i:i + 3]}}]}).encode() + b'\n\n' for i in range(0, len(ans), 3))
ev += b'data: ' + json.dumps({'choices': [{'index': 0, 'delta': {}, 'finish_reason': 'stop'}]}).encode() + b'\n\ndata: [DONE]\n\n'
engine = b'HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nTransfer-Encoding: chunked\r\nServer: llama.cpp\r\n\r\n' + chunked(ev)
o, k, rc = tool(['relay'], engine)
head, _, body = o.partition('\r\n\r\n'); text = ''; fin = None
for l in body.split('\n'):
    if l.startswith('data: ') and l[6:] != '[DONE]':
        j = json.loads(l[6:])
        if 'rudolf' in j: fin = j['rudolf']
        else: text += j['choices'][0]['delta']['content']
ok('7. relay: the page gets an event stream', rc == 0 and head.startswith('HTTP/1.1 200') and 'text/event-stream' in head and body.rstrip().endswith('data: [DONE]'), head)
ok('7. relay: nothing of the key or the token in the streamed text', not leaks(text, K['OpenSSH'].split('-----')[2], 16) and not leaks(text, TOK['GitHub ghs_'], 10) and 'Of course!' in text, text[:400])
ok('7. relay: the last event has the whole answer redacted and what was hidden', fin and '[hidden: a private key]' in fin['text'] and '[hidden: a token]' in fin['text'] and set(fin['hidden']) >= {'private-key', 'token'}, fin)
eng2 = b'HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n'
js = json.dumps({'choices': [{'message': {'role': 'assistant', 'content': 'token ' + TOK['GitHub ghp_']}}]}).encode()
o, k, rc = tool(['relay'], eng2 % len(js) + js)
ok('7. relay: an answer that is not streamed is redacted too', 'token' in k and TOK['GitHub ghp_'] not in o and json.loads(o.partition('\r\n\r\n')[2])['choices'][0]['message']['role'] == 'assistant', o)

# ---------------- 8. never hidden
PUB = ['UqTqXIW8vtITjwoMix+MkFVZ0GAee9+kOS1WBIs/3mo=', 'kRBDN3Ys6i49gblu504quUt7NZHGZj/kexNTb6uoenY=']
fine = {
    "Rudolf OS's release public key": 'The release key (public): UqTqXIW8vtITjwoMix+MkFVZ0GAee9+kOS1WBIs/3mo=',
    "the Library's public key": 'key = "kRBDN3Ys6i49gblu504quUt7NZHGZj/kexNTb6uoenY="',
    'both, bare, in a list': 'Trusted keys:\nUqTqXIW8vtITjwoMix+MkFVZ0GAee9+kOS1WBIs/3mo=\nkRBDN3Ys6i49gblu504quUt7NZHGZj/kexNTb6uoenY=\n',
    'an SSH public key': 'ssh-ed25519 ' + base64.b64encode(b'\x00\x00\x00\x0bssh-ed25519\x00\x00\x00 ' + os.urandom(32)).decode() + ' me@laptop',
    'a PEM public key': ek.public_key().public_bytes(S.Encoding.PEM, S.PublicFormat.SubjectPublicKeyInfo).decode(),
    'an RSA public key': rk.public_key().public_bytes(S.Encoding.PEM, S.PublicFormat.SubjectPublicKeyInfo).decode(),
    'a certificate-free age recipient': 'age -r ' + AGEPUB[0] + ' file',
    'a SHA-256 hash': 'sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08 rudolf.iso',
    'sha256sum output': '9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08  rudolf-os-1.6.1.iso',
    'a commit': 'commit 53ff5fe9a2b4c1d7e8f90a1b2c3d4e5f60718293 Merge pull request #574',
    'a UUID': 'UUID=3f2a9c1e-4b5d-4e6f-8a7b-9c0d1e2f3a4b /persist ext4',
    'an SRI hash': '<script src="x.js" integrity="sha384-oqVuAfXRKap7fdgcCY5uykM6+R9GqQ8K/uxy9rx7HNQlGYl1kPzQho1wx4JwY8wC">',
    'prose': 'Blitzen can explain how SSH keys work: the private key stays on your computer and the public key goes on the server. A PEM file starts with -----BEGIN PRIVATE KEY----- and never leaves it.\nTokens like ghp_ are for GitHub.',
    'code reading a password': 'password = input("Password: ")\npw = getpass.getpass()\ntoken = request.headers.get("Authorization")\nkey = crypto.randomBytes(32)\nif password == stored: ok()',
    'code with keys and tokens as words': 'for key, value in table.items():\n    tokens = tokenizer(text)\n    keyboard.press(key)\nsecret_count += 1',
    'an address with a long path': 'https://github.com/merciless146/RudolfOS/blob/main/iso/sys/rudolfd.c#L2400 and https://example.org/a/b/c?id=Ab3dE6gH9jK2mN5pQ8rS1tU4vW7',
    'an e-mail address (shown in the chat)': 'Write to support@example.com or the owner.',
    'CamelCase names': 'class ThisIsAVeryLongIdentifierName2 extends HTMLElementWithShadowRoot3 { getElementsByTagNameNS2() {} }',
    'numbers': 'Version 1.6.1, build 20261121, 482913 downloads, port 8600.',
    'a base64 picture': '<img src="data:image/png;base64,' + base64.b64encode(os.urandom(120)).decode() + '">',
    'a password placeholder': 'password: ******** and token=<your token> and PASSWORD=$DB_PASS',
}
for name, t in fine.items():
    o, k = redact(t)
    ok(f'8. not hidden: {name}', o == t and not k, (k, o))
o, k = redact('\n'.join(PUB), mask=127)
ok("8. Rudolf OS's public keys are not even personal details", o == '\n'.join(PUB) and not k, (k, o))
o, k = redact('Write to someone@gmail.com', mask=127)
ok('8. (an e-mail address is personal details for what is sent)', 'personal' in k, (k, o))

# ---------------- 9. key files' paths
for p in ('/home/u/.ssh/id_ed25519', '~/.ssh/config', 'keys/rudolf-signing.pem', 'rudolf-sb.key', '/home/u/.gnupg/private-keys-v1.d/x', 'cert.p12', 'me.pfx', 'vault.kdbx',
          '/home/u/.local/share/keyrings/login.keyring', '/persist/os/dev/u.github.token', '/persist/etc/wifi.conf', 'id_rsa', '.env', '/home/u/.config/rudolf-devlink/token',
          '/run/rudolf/ai/key', 'docs/../keys/x', 'KEYS/SIGNING.PEM', '/home/u/.aws/credentials', '/etc/shadow'):
    ok(f'9. a key file: {p}', tool(['keypath', p], '')[2] == 0)
for p in ('docs/plan-1.6.1.md', 'desktop/blitzen26.js', 'iso/sys/rudolf-secrets.h', 'id_rsa.pub', '/home/u/.ssh/id_ed25519.pub', 'keyboard.c', 'iso/systools/kryptonite/logo.svg', 'trusted-keys.txt', 'monkeys/zoo.txt'):
    ok(f'9. not a key file: {p}', tool(['keypath', p], '')[2] == 1)

print(f'{sum(R)} of {len(R)} passed' if all(R) else f'{len(R) - sum(R)} of {len(R)} FAILED')
subprocess.run(['rm', '-rf', W])
sys.exit(0 if all(R) else 1)
