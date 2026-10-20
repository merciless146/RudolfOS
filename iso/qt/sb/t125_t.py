# Rudolf OS 1.2.5 stage A in QEMU: Linux apps over Wi-Fi, the Games tab, Luau in the real page (UEFI, Secure Boot off).
#   python3 t125_t.py DISC.iso AUX.img
# DISC.iso: a test disc (mkdisc.sh, the test Secure Boot key, RUDOLF_UKI_TEST_CMDLINE=rudolf.debug=1: a serial shell).
# AUX.img: an ext4 disk labelled RUDOLFAUX with hostapd and its settings (qt/mkwifi.sh) and repo/, a stand-in for
# Flathub (qt/mkflat.sh's repo1). A blank 4 GB RUDOLFDATA disk makes the system keep files (apps need it).
# The computer's only network is Wi-Fi: wlan0 joins RudolfTest, an access point on a second simulated radio
# (mac80211_hwsim), which also serves the stand-in Flathub.
#   - apps/size: the app and runtime download sizes from Flatpak, and "wifi" for the connection; "none" before
#   - an install whose download fails is tried again by rudolfd and finishes when the server is back
#   - App Store › Games with its labels and the "drawing with the processor" note; Luau runs and checks in the page
import os, sys, time, subprocess, json
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
ISO = os.path.abspath(sys.argv[1]); AUX = os.path.abspath(sys.argv[2]); PARTS = os.environ['SB_PARTS']
LOG = open('t125.txt', 'w'); R = []


def guest(vm):
    from ser import Ser
    g = Guest.__new__(Guest); g.N = vm.name; g.s = Ser(vm.ser_path); g.q = vm.q; g.s.read(0.3)
    return g


def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[:300] if d != '' else ''))
def last(t): return t.strip().split('\n')[-1].strip() if t.strip() else ''

data = os.path.abspath('data125.img')
if os.path.exists(data): os.remove(data)
subprocess.run(['truncate', '-s', '4G', data], check=True)
subprocess.run(['mkfs.ext4', '-q', '-F', '-L', 'RUDOLFDATA', data], check=True)
vm = sbvm.VM('t125', PARTS, [ISO, data, AUX], secure=False, mem=3072)
g = guest(vm)
def sh(c, t=60): return g.sh(c, t)
def js(e, t=60):
    for _ in range(3):
        r = g.js(e, t)
        # ERR: no page to talk to; EXC ReferenceError: the page is (re)loading, e.g. while the desktop moves into its shell
        if not (isinstance(r, tuple) and (r[0] == 'ERR' or (r[0] == 'EXC' and 'ReferenceError' in str(r[1])))): return r
        time.sleep(15); wait_desktop()
    return r
def jsj(e, t=60):
    r = js(e, t)
    try: return json.loads(r) if isinstance(r, str) else {}
    except ValueError: return {'raw': r}
def wait_shell(t=900, delay=45):
    time.sleep(delay); end = time.time() + t
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): return True
        except OSError: pass
        time.sleep(4)
    return False
def wait_desktop(t=600):
    end = time.time() + t
    while time.time() < end:
        if isinstance(g.js('typeof RVER=="string"&&!!document.body&&RVER', 20), str): return True
        time.sleep(5)
    return False
def until(e, t=120):
    end = time.time() + t
    while time.time() < end:
        if g.js(e, 20) is True: return True
        time.sleep(3)
    return False
def job(t=600):
    end = time.time() + t; j = {}
    while time.time() < end:
        j = jsj("rd('job').then(j=>JSON.stringify(j))")
        if not j.get('running') and j.get('status', -1) >= 0: return j
        time.sleep(4)
    return j
def wait_sh(c, want, t=60):
    end = time.time() + t; o = ''
    while time.time() < end:
        o = sh(c, 20)
        if want in o: return o
        time.sleep(3)
    return o

try:
    ok('starts, keeping files on RUDOLFDATA', wait_shell() and last(sh('[ -e /run/rudolf/persistent ] && echo kept || echo live')) == 'kept', sh('grep persist /proc/mounts'))
    ok('the desktop is 1.2.5', wait_desktop() and js('RVER') == '1.2.5', js('RVER'))
    js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    ok('signed in', until('!!USER', 120))
    def ready():   # the desktop's own functions are there (after a reload the page has to be set up again)
        if not until("typeof rdPost=='function'&&typeof lxApi=='function'&&!!USER", 120): wait_desktop()
        js("USER.r='admin';window.elevate=async()=>true;1")
    ready()
    sh('ip link set eth0 down 2>/dev/null; ip route del default 2>/dev/null; true')
    s = jsj("lxApi('apps/size?id=org.rudolf.TestApp').then(j=>JSON.stringify(j),e=>JSON.stringify({err:e.message}))")
    ok('no network: apps/size says "none"', s.get('ok') and s.get('link') == 'none', s)
    # the access point, its DHCP and the stand-in Flathub on 10.42.0.1
    out(sh("modprobe mac80211_hwsim radios=2 2>&1; sleep 2; ls /sys/class/net", 30))
    A = last(sh("findfs LABEL=RUDOLFAUX"))
    out(sh(f"mkdir -p /mnt/aux; grep -q ' /mnt/aux ' /proc/mounts || mount {A} /mnt/aux; /mnt/aux/hostapd -B -P /run/hostapd.pid /mnt/aux/ap-mixed.conf >/run/hostapd.log 2>&1; sleep 2; ip addr add 10.42.0.1/24 dev wlan1; busybox udhcpd /mnt/aux/udhcpd.conf; busybox httpd -p 10.42.0.1:8099 -h /mnt/aux; tail -2 /run/hostapd.log", 40))
    ready()
    r = js("rdPost('wifi/connect?if=wlan0','RudolfTest\\ncorrect horse battery\\npsk').then(()=>'ok',e=>e.message)")
    j = job(120); joined = r == 'ok' and j.get('status') == 0 and '10.42.0.' in wait_sh('ip -4 -o addr show dev wlan0', '10.42.0.', 60)
    if not joined:   # what the Wi-Fi service saw, for the record
        out('connect:', r, j.get('status'), (j.get('log') or '')[-600:])
        out(sh('wpa_cli -p /run/rudolf/wpa -i wlan0 status; wpa_cli -p /run/rudolf/wpa -i wlan0 scan_results | head -5', 30))
        out(sh('ps -eo pid,args | grep -E "wpa_|udhcpc" | grep -v grep; tail -5 /run/hostapd.log', 30))
    ok('joins RudolfTest over Wi-Fi', joined, sh('ip -4 -o addr; ip route', 20))
    ok('...and the default route is the Wi-Fi', 'dev wlan0' in wait_sh('ip route show default', 'wlan0', 30), sh('ip route', 20))
    out(sh("flatpak remote-delete --system --force flathub 2>/dev/null; flatpak remote-add --system --no-gpg-verify flathub http://10.42.0.1:8099/repo && flatpak remotes --system", 60))
    r = js("lxApi('apps/refresh').then(()=>'ok',e=>e.message)"); j = job(300)
    ok('the app list downloads over Wi-Fi', r == 'ok' and j.get('status') == 0, (j.get('log') or '')[-300:])
    s = jsj("lxApi('apps/size?id=org.rudolf.TestApp').then(j=>JSON.stringify(j),e=>JSON.stringify({err:e.message}))", 120)
    raw = sh('flatpak remote-info --system flathub org.rudolf.TestApp 2>&1 | grep -E "Download|Installed|Runtime"', 60)
    ok('apps/size: Flatpak\'s sizes, over "wifi" (wlan0)', s.get('ok') and s.get('link') == 'wifi' and s.get('dev') == 'wlan0' and s.get('download', 0) > 0 and s.get('installed', 0) > 0, [s, raw])
    ok('...and the runtime it still needs', s.get('runtime', '').startswith('org.rudolf.TestPlatform/') and s.get('runtimeDownload', 0) > 0, s)
    note = js("JSON.stringify(appDownloadNote(" + json.dumps(s) + ",'RudolfTest',true,4e9))")
    ok('...which the App Store would not ask about (a small download), even metered', note == 'null', note)
    # the server goes away during the install: rudolfd tries again and finishes once it is back
    sh("kill $(pgrep -f 'httpd -p 10.42.0.1:8099') 2>/dev/null; true")
    r = js("lxApi('apps/install?id=org.rudolf.TestApp').then(()=>'ok',e=>e.message)")
    ok('install starts with the server gone', r == 'ok', r)
    time.sleep(6); sh('busybox httpd -p 10.42.0.1:8099 -h /mnt/aux')
    j = job(600)
    ok('...the download is tried again and the install finishes', j.get('status') == 0 and 'Trying again' in (j.get('log') or ''), (j.get('log') or '')[-400:])
    ok('...the app is installed', 'org.rudolf.TestApp' in sh('flatpak list --system --app --columns=application', 30))
    s = jsj("lxApi('apps/size?id=org.rudolf.TestApp').then(j=>JSON.stringify(j))", 120)
    ok('...and its runtime no longer counts as a download', s.get('runtimeDownload') == 0, s)
    # Games tab against the real rudolfd (QEMU draws with the processor)
    js("window.__S=appStore();__S.body.querySelector(\".tmtabs [data-t='games']\").click();1")
    ok('App Store › Games lists the games', until("__S.body.querySelectorAll('.gmc').length>=5", 60), js("__S.body.innerText.slice(0,400)"))
    ok('...and says this computer draws with the processor', until("!__S.body.querySelector('.gmnote').hidden", 30), js("__S.body.querySelector('.gmnote').innerText"))
    vm.shot('games')
    # Luau and Pilot.lua from rudolfd's own lib/
    r = jsj("luauRun('local n: number = 2\\nn += 3\\nprint(`n is {n}`)').then(r=>JSON.stringify(r))", 120)
    ok('Luau runs in the page (lib/luau.wasm from rudolfd)', r.get('code') == 0 and r.get('out') == 'n is 5\n', r)
    r = jsj("luauBlitzen('local s = GetPart(\"Screen\")\\nif s then s:Nonsense() end','pilot').then(r=>JSON.stringify(r))", 180)
    ok('...and the Pilot.lua checker finds a wrong part method', r.get('code') == 1 and 'Nonsense' in (r.get('out') or ''), r)
finally:
    vm.stop()
out('%d/%d passed' % (sum(R), len(R)))
