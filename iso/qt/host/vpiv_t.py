#!/usr/bin/env python3
# 1.6.1 (docs/plan-1.6.1.md Part 21; docs/plan-1.6.0.md Part 20's planned test), the host's half: a virtual PIV card
# (vpiv.py) in vsmartcard's virtual reader (vpcd, pcscd's driver from vsmartcard-vpcd), read by OpenSC's PKCS#11 module as
# Rudolf OS reads a card, through this tree's /usr/lib/rudolf/smartcard (status, nss) and NSS (the certificate database
# Oracle's Chromium uses): the card's certificate without its PIN, TLS client authentication with the card's key after
# the PIN (NSS's tstclnt to a local server that asks for a certificate), a wrong PIN signing nothing, the card taken out.
# The test CA, the card's key and the server's are made here, in a folder of its own, and removed (never kept).
# Runs as root in a private mount and network namespace (its own pcscd socket folder, its own loopback for vpcd's port
# and the server): nothing of the host's pcscd is touched.
#   sudo python3 vpiv_t.py   (needs pcscd, vsmartcard-vpcd, opensc, libnss3-tools, python3-cryptography)
import os, sys, subprocess, json, time, ssl, socket, threading, tempfile, shutil, base64
HERE = os.path.dirname(os.path.abspath(__file__)); ISO = os.path.dirname(os.path.dirname(HERE))
if os.environ.get('VPIV_INNER') != '1':
    if os.geteuid() != 0: print('SKIP: needs root (a private namespace)'); sys.exit(0)
    os.execvp('unshare', ['unshare', '-m', '-n', '--propagation', 'private', 'env', 'VPIV_INNER=1', sys.executable, os.path.abspath(__file__)] + sys.argv[1:])
sys.path.insert(0, HERE)
import vpiv
R = []
def ok(n, c, d=''):
    R.append(bool(c)); print(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:600] if d != '' and not c else ''), flush=True)
def sh(c, t=60, env=None):
    p = subprocess.run(c, shell=True, capture_output=True, text=True, timeout=t, env=env); return p.returncode, p.stdout + p.stderr
MOD = next((m for m in ('/usr/lib/x86_64-linux-gnu/opensc-pkcs11.so', '/usr/lib/x86_64-linux-gnu/pkcs11/opensc-pkcs11.so', '/usr/lib/opensc-pkcs11.so') if os.path.exists(m)), '')
VPCD = next((m for m in ('/usr/lib/pcsc/drivers/serial/libifdvpcd.so', '/usr/lib/x86_64-linux-gnu/pcsc/drivers/serial/libifdvpcd.so') if os.path.exists(m)), '')
miss = [n for n, v in (('opensc-pkcs11.so', MOD), ('vpcd (vsmartcard-vpcd)', VPCD), ('pcscd', shutil.which('pcscd')), ('tstclnt', shutil.which('tstclnt')),
                       ('certutil', shutil.which('certutil')), ('modutil', shutil.which('modutil')), ('pkcs11-tool', shutil.which('pkcs11-tool'))) if not v]
if miss: print('SKIP: missing ' + ', '.join(miss)); sys.exit(0)
W = tempfile.mkdtemp(prefix='vpiv'); os.chmod(W, 0o700); K = f'{W}/k'
sh('ip link set lo up')
subprocess.run(['mount', '-t', 'tmpfs', '-o', 'mode=755', 'tmpfs', '/run/pcscd'] if os.path.isdir('/run/pcscd') else ['true'], check=True)
if not os.path.isdir('/run/pcscd'): os.makedirs('/run/pcscd'); subprocess.run(['mount', '-t', 'tmpfs', 'tmpfs', '/run/pcscd'], check=True)
os.makedirs(f'{W}/rc'); open(f'{W}/rc/vpcd', 'w').write(f'FRIENDLYNAME "Virtual PCD"\nDEVICENAME   /dev/null:0x8C7B\nLIBPATH      {VPCD}\nCHANNELID    0x8C7B\n')
pc = subprocess.Popen(['pcscd', '-f', '-a', '--disable-polkit', '-c', f'{W}/rc'], stdout=open(f'{W}/pcscd.log', 'w'), stderr=subprocess.STDOUT)
P = vpiv.pki(K); card = vpiv.PIVCard(K); CN = P['card']
v = vpiv.VPCD(card).start(30)
procs = [pc]
try:
    for _ in range(40):
        if CN in sh(f'pkcs11-tool --module {MOD} -L')[1]: break
        time.sleep(0.5)
    # 1. the reader, the card and its certificate, no PIN
    rc, o = sh(f'pkcs11-tool --module {MOD} -L')
    ok('1. vsmartcard\'s reader (vpcd) in pcscd, the card in it: OpenSC\'s PIV driver, its token named after the card\'s holder',
       'Virtual PCD' in o and CN in o and 'piv_II' in o, o)
    rc, o = sh(f'pkcs11-tool --module {MOD} --list-objects --type cert')
    ok('...the PIV Authentication certificate (9A) read without the PIN', 'Certificate for PIV Authentication' in o and CN in o and card.counts['verify_ok'] == 0, o)
    # 2. this tree's smartcard script, as rudolfd's sc/status runs it
    SC = f'{ISO}/sys/root/usr/lib/rudolf/smartcard'; H = f'{W}/home'; os.makedirs(H)
    env = dict(os.environ, RUDOLF_PKCS11=MOD, RUDOLF_SC_USER='root', RUDOLF_SC_HOME=H)
    rc, o = sh(f'sh {SC} status', env=env)
    try: st = json.loads(o)
    except Exception: st = {}
    rds = st.get('readers', []); rd0 = next((r for r in rds if r.get('card')), {})
    ders = [base64.b64decode(c['der']) for c in rd0.get('certs', [])]
    ok('2. smartcard status (Settings › Smart cards\' source): pcscd, the module, the reader with a card, its label and maker',
       st.get('pcscd') is True and st.get('module') is True and rd0.get('label') == CN and rd0.get('maker') == 'piv_II', o)
    ok('...the card\'s certificate as it is on the card (DER), without the PIN', open(f'{K}/card.der', 'rb').read() in ders and card.counts['verify_ok'] == 0, [len(d) for d in ders])
    rc, o = sh(f'sh {SC} nss', env=env); rc2, o2 = sh(f"modutil -dbdir sql:{H}/.pki/nssdb -list")
    ok('3. smartcard nss: the card\'s module ("Smart cards") in the certificate database (Oracle\'s Chromium\'s)', rc == 0 and 'Smart cards' in o2 and MOD in o2, o + o2)
    rc, o = sh(f"sh {SC} nss", env=env); rc3, o3 = sh(f"modutil -dbdir sql:{H}/.pki/nssdb -list")
    ok('...a second time: still once', rc == 0 and o3.count('Smart cards') == 1, o3)
    rc, o = sh(f"certutil -d sql:{H}/.pki/nssdb -L -h all")
    ok('...NSS lists the card\'s certificate there (as Chromium\'s picker will)', f'{CN}:Certificate for PIV Authentication' in o, o)
    # 4. TLS client authentication: a server that asks for a certificate from the test CA; NSS's tstclnt with the database
    srv = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); srv.load_cert_chain(f'{K}/server.pem', f'{K}/server.key')
    srv.verify_mode = ssl.CERT_REQUIRED; srv.load_verify_locations(f'{K}/ca.pem')
    seen = []; ls = socket.socket(); ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); ls.bind(('127.0.0.1', 8643)); ls.listen(5)
    def serve():
        while True:
            try: c, _ = ls.accept()
            except OSError: return
            try:
                t = srv.wrap_socket(c, server_side=True); pc_ = t.getpeercert()
                seen.append(dict(x[0] for x in pc_.get('subject', ())).get('commonName', '?'))
                t.recv(1000); t.sendall(b'HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\ncard ok ' + seen[-1].encode()); t.close()
            except Exception as e: seen.append('refused: ' + str(e)[:120]); c.close()
    threading.Thread(target=serve, daemon=True).start()
    sh(f"certutil -d sql:{H}/.pki/nssdb -A -n 'Rudolf test CA' -t 'C,,' -i {K}/ca.der")
    nick = f'{CN}:Certificate for PIV Authentication'
    def tls(pin):
        open(f'{W}/req', 'w').write('GET / HTTP/1.0\r\n\r\n')
        return sh(f"tstclnt -h 127.0.0.1 -p 8643 -d sql:{H}/.pki/nssdb -n '{nick}' -w {pin} -V tls1.2:tls1.3 < {W}/req", 60)
    n0 = card.counts['sign']; rc, o = tls('000000')
    ok('4. a wrong PIN: the card refuses it (2 tries left) and signs nothing; the site gets no certificate',
       card.counts['verify_bad'] == 1 and card.tries == 2 and card.counts['sign'] == n0 and CN not in seen, (card.log[-6:], seen, o[-300:]))
    rc, o = tls('123456')
    ok('...the right PIN: TLS client authentication with the card\'s key, the site sees the card\'s holder',
       f'card ok {CN}' in o and seen and seen[-1] == CN and card.counts['sign'] > n0 and card.tries == 3, (card.log[-6:], seen, o[-400:]))
    # 5. the card taken out
    v.stop()
    for _ in range(30):
        rc, o = sh(f'sh {SC} status', env=env)
        try: st = json.loads(o)
        except Exception: st = {}
        if st.get('readers') is not None and not any(r.get('card') for r in st['readers']): break
        time.sleep(0.5)
    ok('5. the card taken out: smartcard status shows the reader with no card', st.get('readers') and not any(r.get('card') for r in st['readers']), o)
    rc, o = tls('123456')
    ok('...and the site gets no certificate from it', f'card ok' not in o, o[-300:])
    ls.close()
finally:
    try: v.stop()
    except Exception: pass
    for p in procs:
        p.terminate()
        try: p.wait(10)
        except Exception: p.kill()
    shutil.rmtree(W, ignore_errors=True)
print(f'{sum(R)}/{len(R)} passed', flush=True); sys.exit(0 if all(R) else 1)
