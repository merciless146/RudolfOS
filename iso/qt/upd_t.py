# System updates end to end on an installed (virtual) disk: download into slot A, restart into it, keep it;
# a damaged update in slot B is tried twice and dropped; roll back to the disc's system.
#   python3 upd_t.py NAME PHASE      (QEMU already running with the debug shell, see run_upd.sh)
import sys,time,os,json;sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import guest
class _NoQMP:
    def __init__(s,*a):pass
    def shot(s,*a):pass
    def key(s,*a):pass
guest.QMP=_NoQMP  # the boot-menu thread in drive.py holds the only QMP connection
from guest import Guest
from PIL import Image
N=sys.argv[1];g=Guest(N);LOG=open(f'{N}_upd.txt','a')
def out(*a):
    s=' '.join(str(x) for x in a);print(s,flush=True);LOG.write(s+'\n');LOG.flush()
def ok(n,c,d=''):out(('PASS ' if c else 'FAIL ')+n+(' - '+str(d)[:300] if d else ''))
def w(c,t=420):
    e=time.time()+t
    while time.time()<e:
        try:
            if g.js(c,20) is True: return True
        except Exception: pass
        time.sleep(4)
    return False
def menu_unused(t=600):
    """after a restart: wait for the blue boot menu and choose the diagnostic start (keeps the serial shell)"""
    e=time.time()+t;n=0
    while time.time()<e:
        time.sleep(1.5)
        try:
            g.q.shot(os.path.abspath(f'{N}_bm.png'));im=Image.open(f'{N}_bm.png').convert('RGB')
            if im.size==(720,400) and im.getpixel((10,200))[2]>150 and im.getpixel((10,200))[0]<20:
                g.q.key('d');time.sleep(0.7);g.q.key('ret');n+=1;out('  boot menu: diagnostic start');return True
        except Exception: pass
    return False
def job(path,t=600):
    r=g.js(f"(async()=>{{await rdPost('{path}');for(let i=0;i<{t//2};i++){{await new Promise(r=>setTimeout(r,2000));const j=await rd('job');if(!j.running)return j.status+'|'+j.log}}return 'timeout'}})().catch(e=>'ERR '+e.message)",t+30)
    return str(r)
def wait_sh(cmd,want,t=1200):
    e=time.time()+t
    while time.time()<e:
        try:
            o=g.sh(cmd,15)
            if want in o: return o
        except Exception: pass
        time.sleep(8)
    return ''
def state(): return g.sh('cat /persist/os/system/state; echo slot=$(cat /etc/rudolf/slot)',20)
def serve():
    return g.sh('mkdir -p /mnt/rel; grep -q " /mnt/rel " /proc/mounts || mount /dev/vdb /mnt/rel; pgrep -f "httpd -p 127.0.0.1:8098" >/dev/null || busybox httpd -p 127.0.0.1:8098 -h /mnt/rel/srv; wget -q -O - http://127.0.0.1:8098/channel/manifest.txt | sed -n 2p',40)
def reboot():
    g.js("rdPost('power?action=reboot').catch(()=>0)",20);time.sleep(5)
ph=sys.argv[2]
if ph=='A':
    ok('desktop up',w("typeof rd=='function'&&!!RD"))
    out(g.sh('cat /etc/rudolf/slot; cat /live/media/rudolf/release; ls /persist/os; ip -4 addr | grep inet',30))
    out('release server:',serve())
    r=g.js("rdPost('sys/set?k=url&v='+encodeURIComponent('http://127.0.0.1:8098/channel')).then(r=>r.message).catch(e=>'ERR '+e.message)");ok('set update address',r=='saved',r)
    r=job('sys/check');ok('check finds the update','Update available' in r,r)
    r=job('sys/download');ok('download into slot A',r.startswith('0|') and 'Ready' in r,r)
    out(state())
    s=g.js("rd('sys/status').then(s=>JSON.stringify({next:s.state.next,slotA:s.slots.a}))");ok('status shows the waiting update','"next":"a"' in str(s),s)
    reboot()
elif ph=='B':
    ok('restarted into the update',w("typeof rd=='function'&&!!RD") and g.sh('cat /etc/rudolf/slot',20).strip().endswith('a'),g.sh('cat /etc/rudolf/slot; cat /proc/cmdline',20))
    ok('new desktop running',g.js("window.__RELTEST")=='r1',g.js("String(window.__RELTEST)"))
    time.sleep(15);st=state();ok('update kept after the desktop came up','good=a' in st and 'next=\n' in st+'\n',st)
    out(g.sh('cat /run/rudolf/update.log 2>/dev/null; ls -la /persist/os/system/a',20))
    out(g.sh('mkdir -p /mnt/rel; mount /dev/vdb /mnt/rel; cp /mnt/rel/next/* /mnt/rel/srv/channel/; sync',30));out('release server:',serve())
    r=job('sys/download');ok('second update into slot B',r.startswith('0|') and 'Ready' in r,r)
    out(g.sh("dd if=/dev/urandom of=/persist/os/system/b/desktop.sfs bs=1k count=64 seek=4 conv=notrunc 2>&1 | tail -1; sync; cat /persist/os/system/state",30))
    reboot()
elif ph=='C':
    st=wait_sh('cat /persist/os/system/state; echo slot=$(cat /etc/rudolf/slot)','failed=b');w("typeof rd=='function'&&!!RD",600)
    ok('back on slot A after the damaged update','slot=a' in st,st)
    ok('damaged update dropped','failed=b' in st and 'good=a' in st,st)
    s=g.js("(async()=>{settings('upd');await new Promise(r=>setTimeout(r,3000));return (document.querySelector('.sysu')||{}).innerText})()",40)
    ok('Settings explains the fallback','did not start' in str(s),s);
    r=g.js("rdPost('sys/rollback').then(r=>r.message).catch(e=>'ERR '+e.message)");ok('roll back',str(r).startswith('The previous system'),r)
    out(state());reboot()
elif ph=='D':
    st=wait_sh('echo slot=$(cat /etc/rudolf/slot)','slot=disc');w("typeof rd=='function'&&!!RD",600)
    ok('rolled back to the disc system','slot=disc' in st,st)
    ok('old desktop again',g.js("String(window.__RELTEST)")=='undefined',g.js("String(window.__RELTEST)"))
    out(state())
elif ph=='K':
    # the published 1.1 disc: only the owner's key is trusted, the update address is built in
    KEY='UqTqXIW8vtITjwoMix+MkFVZ0GAee9+kOS1WBIs/3mo=';URL='https://merciless146.github.io/RudolfOS/stable'
    ok('desktop up',w("typeof rd=='function'&&!!RD"))
    k=g.sh("grep -v '^#' /etc/rudolf/rpk-keys",20).strip();ok('trusts only the owner key',k==KEY,k)
    s=g.js("rd('sys/status').then(s=>s.url+'|'+s.auto+'|'+s.persistent+'|'+(s.current||{}).version)");ok('update address built in',str(s).startswith(URL+'|'),s)
    out('release server:',serve())
    g.js("rdPost('sys/set?k=url&v='+encodeURIComponent('http://127.0.0.1:8098/channel')).then(r=>r.message)")
    r=job('sys/check');ok('release signed by another key refused','not signed by a trusted key' in r,r)
    g.js("rdPost('sys/set?k=url&v='+encodeURIComponent('"+URL+"')).then(r=>r.message)")
    s=g.js("rd('sys/status').then(s=>s.url)");ok('address set back',s==URL,s)
    r=job('sys/check');out('check against the real address (no network in the test):',r)
    r=g.sh("rpk install jq 2>&1 | tail -1; echo '{\"a\":42}' | rpk run jq .a 2>&1",90);ok('disc package installs and runs','Installed jq' in r and '42' in r,r)
    r=g.sh("rpk install-file /live/media/rudolf/pkg/sudoku.rpk 2>&1 | tail -1",60);ok('package signed by another key refused as a file','not from a trusted key' in r,r)
elif ph=='T':
    # text-to-speech voices for all nine languages
    ok('desktop up',w("typeof rd=='function'&&!!RD"))
    r=g.js("""(async()=>{const o=[];for(const [l,v] of Object.entries(TTS_ESPEAK)){const t0=performance.now();const r=await fetch('/api/say?lang='+v+'&text='+encodeURIComponent(TTS_SAMPLE[l]),{method:'POST',headers:{'X-Rudolf-Token':RD.token}});const b=r.ok?await r.blob():null;o.push(l+':'+r.status+':'+(b?b.size:0)+':'+Math.round(performance.now()-t0)+'ms')}return o.join(' ')})()""",120)
    out('voices:',r);ok('all nine languages speak',str(r).count(':200:')==9 and all(int(x.split(':')[2])>20000 for x in str(r).split()),r)
    r=g.js("jaKana('設定を変更しました。音声合成').catch(e=>'ERR '+e.message)",60);ok('Japanese kanji read as kana','せっていをへんこうしました。おんせいごうせい'==r,r)
    r=g.js("JSON.stringify(LANGS.map(l=>[l[0],(ttsEngine(l[0])||{}).kind]))");ok('every language has an engine','null' not in str(r) and str(r).count('"')>=36,r)
    r=g.js("ttsGuess('Привет, как дела?','en')+ttsGuess('こんにちは','en')+ttsGuess('你好','en')+ttsGuess('안녕','en')+ttsGuess('Hello','ru')");ok('language picked from the text','rujazhkoen'==r,r)
    r=g.js("(async()=>{settings('acc');await new Promise(r=>setTimeout(r,2500));const b=document.querySelector('.ttsbox');return b?b.querySelectorAll('tr').length+'|'+b.innerText.replace(/\\s+/g,' ').slice(0,300):'no box'})()",40);ok('Settings lists a voice per language',str(r).startswith('9|'),r)
    g.q.shot(os.path.abspath(f'{N}_tts.png')) if hasattr(g.q,'shot') else None
    r=g.js("new Promise(res=>{ttsSpeak('Проверка связи',{lang:'ru',onend:ok=>res('end '+ok)});setTimeout(()=>res('timeout'),20000)})",40);ok('spoken audio plays to the end',r=='end true',r)
elif ph=='P':
    # WordPro printing on the real system: PDF by the disc's Chromium, preview, Save as PDF, printer search
    ok('desktop up',w("typeof rd=='function'&&!!RD"))
    g.js("""(()=>{document.querySelectorAll('.rmodal').forEach(e=>e.remove());wordpro();setTimeout(()=>{const e=document.querySelector('.wp-ed,[contenteditable=true]');let h='<h1>Print test</h1><p>Some <b>bold</b> text.</p>';for(let i=0;i<60;i++)h+='<p>Line '+(i+1)+' of the print test document.</p>';e.innerHTML=h;e.dispatchEvent(new Event('input',{bubbles:true}));wins.find(x=>x.printDoc).printDoc()},2500);return 1})()""",30)
    t0=time.time();okp=w("document.querySelectorAll('.prdlg canvas.prpage').length>=2",300)
    ok('PDF made and previewed',okp,f'{time.time()-t0:.0f}s '+str(g.js("(document.querySelector('.prdlg .prprev')||{}).innerText")))
    t0=time.time();okf=w("!document.querySelector('.prdlg .pfind').disabled",60);ok('printer search finishes',okf,f'{time.time()-t0:.0f}s')
    ok('Save as PDF selected by default',g.js("document.querySelector('.prdlg .pdst').value")=='pdf')
    g.q.shot(os.path.abspath(f'{N}_print.png'))
    g.js("document.querySelector('.prdlg .pgo').click()");time.sleep(1.5)
    r=g.js("[...document.querySelectorAll('.rmodal .rmbox')].map(b=>b.innerText).join(' | ')");ok('Save as PDF dialog offers the formatted PDF','UNTITLED.PDF' in str(r) and 'Save in Documents' in str(r),r)
    r=g.js("prJSON('print/find').then(j=>JSON.stringify(j)).catch(e=>'ERR '+e.message)",30);ok('printer search answers',str(r).startswith('{"ok":true'),r)
    r=g.js("fetch('/api/print/info?uri='+encodeURIComponent('ipp://127.0.0.1:9/ipp/print'),{method:'POST',headers:{'X-Rudolf-Token':RD.token}}).then(r=>r.text())",40);ok('unreachable printer explained','does not answer' in str(r),r)
    r=g.js("fetch('/api/print/info?uri='+encodeURIComponent('ipp://x;reboot/'),{method:'POST',headers:{'X-Rudolf-Token':RD.token}}).then(r=>r.status)",20);ok('bad printer address refused',r==400,r)
    out(g.sh('ls /tmp | grep rprint; which rudolf-print',20))
elif ph=='M':
    ok('desktop up',w("typeof rd=='function'&&!!RD"))
    href=g.js("location.href")
    g.js("""(()=>{document.querySelectorAll('.rmodal').forEach(e=>e.remove());emailCompose({subject:'REPORT.DOC',body:mlText('<!--wp:{}--><h1>Sales</h1><p>Up <b>5%</b></p>'),files:[['REPORT.DOC','<p>x</p>']]});return 1})()""")
    time.sleep(1);ok('compose window opens',g.js("!!document.querySelector('.mlbox')"))
    ok('message is plain text',g.js("document.querySelector('.mlbox .mbo').value")=='Sales\nUp 5%',g.js("document.querySelector('.mlbox .mbo').value"))
    g.js("document.querySelector('.mlbox .mweb').click()");time.sleep(4)
    ok('desktop stays (no blank page)',g.js("location.href")==href and g.js("typeof wins")=='object',g.js("location.href"))
    ok('Gmail opened in Oracle',g.js("[...document.querySelectorAll('iframe')].some(f=>f.src.includes('mail.google.com'))"))
    out(g.sh('ls -la /home/rudolf/Downloads',20))
elif ph=='W':
    import base64
    ok('desktop up',w("typeof rd=='function'&&!!RD"))
    def shot(name):
        b64=g.js("fetch('/api/screenshot',{headers:{'X-Rudolf-Token':RD.token}}).then(r=>r.blob()).then(b=>new Promise(r=>{const f=new FileReader();f.onload=()=>r(f.result.split(',')[1]);f.readAsDataURL(b)}))",60)
        open(f'{N}_{name}.png','wb').write(base64.b64decode(b64))
    # the test machine has no network, so the desktop's firewall check would stop the address; and the first-start
    # screen has no taskbar yet, so the strip is set by hand
    g.js("fwCheck=()=>null;rdPost('web/strut?side=bottom&size=46')");time.sleep(2);out('strut:',g.sh('cat /run/rudolf/strut',20))
    out('open:',g.js("webOpen('http://127.0.0.1:8080/lib/ja-yomi-COPYING.txt').then(()=>'ok').catch(e=>'ERR '+e.message)",30))
    st=wait_sh("pgrep -f 'class=RudolfWeb' >/dev/null && echo RUN; cat /run/rudolf/wm",'RUN',240);ok('browser window started','RUN' in st,st)
    time.sleep(40);ok('window shown',g.sh('cat /run/rudolf/wm',20).strip()=='web');shot('web')
    r=g.js("webToggle().then(()=>WEB_SHOWN)",30);time.sleep(4);ok('back to the desktop',g.sh('cat /run/rudolf/wm',20).strip()=='desktop',r);shot('desk')
    g.js("webToggle()");time.sleep(4);ok('shown again',g.sh('cat /run/rudolf/wm',20).strip()=='web')
    g.js("rdPost('web/close')");time.sleep(6);ok('closed',g.sh("pgrep -f 'class=RudolfWeb' >/dev/null && echo RUN || echo NO",20).strip()=='NO')
    out(g.sh('tail -5 /run/rudolf/web.log',20))
elif ph=='L':
    # Linux apps end to end, with a local Flatpak repository on the second disk standing in for Flathub
    import base64
    def shot(name):
        b64=g.js("fetch('/api/screenshot',{headers:{'X-Rudolf-Token':RD.token}}).then(r=>r.blob()).then(b=>new Promise(r=>{const f=new FileReader();f.onload=()=>r(f.result.split(',')[1]);f.readAsDataURL(b)}))",60)
        open(f'{N}_{name}.png','wb').write(base64.b64decode(b64))
    def jobwait(t=900): return g.js(f"(async()=>{{for(let i=0;i<{t//3};i++){{await new Promise(r=>setTimeout(r,3000));const j=await rd('job');if(!j.running)return j.status+'|'+(j.log||'').slice(-400)}}return 'timeout'}})()",t+30)
    ok('desktop up',w("typeof rd=='function'&&!!RD"))
    out(g.sh("mkdir -p /mnt/flat; grep -q ' /mnt/flat ' /proc/mounts || mount /dev/vdb /mnt/flat; ln -sfn repo1 /mnt/flat/repo; pgrep -f 'httpd -p 127.0.0.1:8099' >/dev/null || busybox httpd -p 127.0.0.1:8099 -h /mnt/flat; flatpak remote-add --system --no-gpg-verify flathub http://127.0.0.1:8099/repo 2>&1; flatpak remotes --system",60))
    g.js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    ok('signed in',w("!!USER",120),g.js("String(USER&&USER.n)"))
    r=g.js("lxApi('apps/refresh').then(()=>'ok').catch(e=>'ERR '+e.message)",30);out('refresh:',r);r=jobwait(600);ok('catalogue downloaded',r.startswith('0|'),r)
    r=g.js("lxApi('apps/catalog').then(j=>JSON.stringify(j.apps)).catch(e=>'ERR '+e.message)",60);ok('catalogue lists the test app','org.rudolf.TestApp' in str(r) and '"verified":true' in str(r),r)
    g.js("(()=>{appStore();setTimeout(()=>{const b=[...document.querySelectorAll('.tmtabs button')].find(b=>b.dataset.t=='linux');b&&b.click()},1500);return 1})()")
    ok('App Store shows it',w("[...document.querySelectorAll('.lxrow b')].some(b=>b.textContent=='Rudolf Test App')",120))
    g.js("[...document.querySelectorAll('.lxrow')].find(d=>d.textContent.includes('Rudolf Test App')).click()")
    ok('app page with details',w("(document.querySelector('.lxfacts')||{}).textContent&&document.querySelector('.lxfacts').textContent.includes('Version')",120),g.js("(document.querySelector('.lxfacts')||{}).textContent"))
    out('page:',str(g.js("document.querySelector('.lxfacts').textContent"))[:300]);shot('apppage')
    r=g.js("lxApi('apps/install?id=org.rudolf.TestApp').then(()=>'ok').catch(e=>'ERR '+e.message)",30);ok('install accepted',r=='ok',r);r=jobwait(900);ok('installed',r.startswith('0|'),r)
    r=g.js("lxInstalled(true).then(l=>JSON.stringify(l))",60);ok('installed list with launcher','RudolfTest' in str(r) and 'text/plain' in str(r),r)
    ok('Start menu entry',w("startMenuApps(),[...document.querySelectorAll('#menu li.lxv')].some(l=>l.textContent.includes('Rudolf Test App'))",30))
    r=g.js("(async()=>{const d=node(homePath('DOCUMENTS'));d['NOTE.TXT']='hello from Rudolf';fsSave();await lxSync();return 'ok'})()",120);out('sync:',r)
    ok('file reached the shared folder','hello from Rudolf' in g.sh('cat /home/rudolf/Documents/NOTE.TXT',20),g.sh('ls -la /home/rudolf/Documents',20))
    ok('Open with lists the app',g.js("openersFor([...homePath('DOCUMENTS'),'NOTE.TXT']).some(o=>String(o[0]).startsWith('lx:'))"))
    g.js("lxOpenFile(lxLaunchers()[0],[...homePath('DOCUMENTS'),'NOTE.TXT'])")
    st=wait_sh("cat /run/rudolf/windows; cat /run/rudolf/wm",'RudolfTest',180);ok('app window shown',('RudolfTest' in st) and 'win ' in st,st)
    time.sleep(4);ok('taskbar button',w("[...document.querySelectorAll('.lxtb')].some(b=>b.classList.contains('on'))",30),g.js("[...document.querySelectorAll('.lxtb')].map(b=>b.textContent+'/'+b.className).join(';')"))
    shot('appshown')
    out(g.sh('cat /run/rudolf/apps/org.rudolf.TestApp.log',20))
    r=g.js("lxSync().then(()=>String(node([...homePath('DOCUMENTS'),'NOTE.TXT'])))",120);ok('app edit synced back to drive C:','edited by the test app' in str(r),r)
    g.js("document.querySelector('#bar').dispatchEvent(new PointerEvent('pointerdown',{bubbles:true}))");time.sleep(2);ok('taskbar click returns to the desktop',g.sh('cat /run/rudolf/wm',20).strip()=='desktop')
    g.js("document.querySelector('.lxtb').click()");time.sleep(2);ok('taskbar button shows it again',g.sh('cat /run/rudolf/wm',20).startswith('win '))
    win=g.js("document.querySelector('.lxtb').dataset.win");g.js(f"lxApi('apps/close?win={win}')");time.sleep(4)
    ok('closed politely','asked to close' in g.sh('cat /run/rudolf/apps/org.rudolf.TestApp.log',20) and g.sh('cat /run/rudolf/wm',20).strip()=='desktop',g.sh('cat /run/rudolf/wm; tail -2 /run/rudolf/apps/org.rudolf.TestApp.log',20))
    # portal: a save dialog and a notification from inside the sandbox
    g.sh("(su -s /bin/sh rudolf -c 'set -a; . /run/rudolf/session.env; set +a; flatpak run org.rudolf.TestApp --notify --portal-save SAVED.TXT' >/run/rudolf/apps/portaltest.log 2>&1 &)",20)
    ok('file dialog appears on the desktop',w("!!document.querySelector('.lxfd')",180),g.sh('cat /run/rudolf/apps/portaltest.log; ls /run/rudolf/portal/req; cat /run/rudolf/portal.log',20))
    shot('filedialog')
    g.js("(()=>{const n=document.querySelector('.lxfd .nm');n.value='SAVED.TXT';document.querySelector('.lxfd .ok').click();return 1})()")
    st=wait_sh('cat /home/rudolf/Documents/SAVED.TXT','saved by the test app',90);ok('app saved where the dialog said',('saved by the test app' in st),g.sh('cat /run/rudolf/apps/portaltest.log',20))
    ok('notification shown',w("document.body.innerText.includes('Hello from a Linux app')",60))
    g.sh("pkill -f 'testapp --notify' ; true",10);time.sleep(3)
    # update to version 2, then uninstall
    g.sh("ln -sfn repo2 /mnt/flat/repo",10)
    r=g.js("lxApi('apps/update').then(()=>'ok').catch(e=>'ERR '+e.message)",30);r=jobwait(600);ok('update ran',r.startswith('0|'),r)
    out('version:',g.sh("flatpak list --system --app --columns=application,version",20))
    ok('version 2 installed','2.0' in g.sh("flatpak list --system --app --columns=application,version",20))
    r=g.js("lxApi('apps/uninstall?id=org.rudolf.TestApp').then(()=>'ok').catch(e=>'ERR '+e.message)",30);r=jobwait(300);ok('uninstalled',r.startswith('0|'),r)
    ok('runtime removed with it',g.sh("flatpak list --system --columns=application",20).strip()=='',g.sh("flatpak list --system --columns=application",20))
elif ph=='F':
    # Wi-Fi end to end on simulated radios (mac80211_hwsim): wlan1 is the access point (hostapd + udhcpd from the
    # second disk), wlan0 is the computer's Wi-Fi, driven through Settings > Network like a person would.
    import base64
    def shot(name):
        b64=g.js("fetch('/api/screenshot',{headers:{'X-Rudolf-Token':RD.token}}).then(r=>r.blob()).then(b=>new Promise(r=>{const f=new FileReader();f.onload=()=>r(f.result.split(',')[1]);f.readAsDataURL(b)}))",60)
        open(f'{N}_{name}.png','wb').write(base64.b64decode(b64))
    W0="wpa_cli -p /run/rudolf/wpa -i wlan0"
    ok('desktop up',w("typeof rd=='function'&&!!RD"))
    out(g.sh("ls -la /usr/sbin/wpa_supplicant /usr/sbin/wpa_cli; ls /usr/lib/firmware | head -3; ls /usr/lib/firmware/rtw89 | head -3; ls -la /usr/lib/firmware/regulatory.db*",20))
    out(g.sh("modprobe mac80211_hwsim radios=2 2>&1; sleep 2; ls /sys/class/net; dmesg | grep -i -E 'regulatory|cfg80211' | tail -4",30))
    ok('two simulated radios',all(x in g.sh('ls /sys/class/net',20) for x in ('wlan0','wlan1')))
    out(g.sh("mkdir -p /mnt/wifi; grep -q ' /mnt/wifi ' /proc/mounts || mount /dev/vdb /mnt/wifi; /mnt/wifi/hostapd -B -P /run/hostapd.pid /mnt/wifi/ap-mixed.conf >/run/hostapd.log 2>&1; sleep 2; ip addr add 10.42.0.1/24 dev wlan1; busybox udhcpd /mnt/wifi/udhcpd.conf; tail -3 /run/hostapd.log; iw dev 2>/dev/null; cat /sys/class/net/wlan1/operstate",40))
    g.js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    ok('signed in',w("!!USER",120))
    g.js("settings('net')")
    ok('Wi-Fi panel shown',w("!!document.querySelector('.wfcur')",60),g.js("(document.querySelector('.wf')||{}).innerText"))
    ok('test network listed',w("[...document.querySelectorAll('.wfnm')].some(n=>n.textContent=='RudolfTest')",90),g.js("(document.querySelector('.wfl')||{}).innerText")+' | '+g.sh(W0+" scan_results",20))
    out('row:',g.js("[...document.querySelectorAll('.wfrow')].map(r=>r.innerText.replace(/\\n/g,' ')).join(' / ')"))
    shot('list')
    def try_pw(pw):
        g.js("(()=>{const r=[...document.querySelectorAll('.wfrow')].find(r=>r.querySelector('.wfnm')&&r.querySelector('.wfnm').textContent=='RudolfTest');r.querySelector('button').click();return 1})()");time.sleep(1)
        g.js(f"(()=>{{const f=document.querySelector('.wfpw');f.querySelector('.wfp').value={json.dumps(pw)};f.querySelector('.wfgo').click();return 1}})()")
    # a password that is too short is caught on the page
    g.js("(()=>{const r=[...document.querySelectorAll('.wfrow')].find(r=>r.querySelector('.wfnm')&&r.querySelector('.wfnm').textContent=='RudolfTest');r.querySelector('button').click();return 1})()");time.sleep(1)
    g.js("(()=>{const f=document.querySelector('.wfpw');f.querySelector('.wfp').value='short';f.querySelector('.wfgo').click();return 1})()")
    ok('short password refused on the page','8 to 63' in str(g.js("document.querySelector('.wfpw .wfe').textContent")))
    g.js("document.querySelector('.wfpw .wfx').click()")
    try_pw('wrong password 123')
    ok('wrong password reported',w("document.body.innerText.includes('password is wrong')||document.body.innerText.includes('Could not join')",90),g.js("String(window.__notifCentre&&JSON.stringify(__notifCentre.list?__notifCentre.list():''))")[:300]+' '+g.sh("cat /run/rudolf/job.log",20))
    out('after wrong:',g.sh(W0+" list_networks; cat /persist/etc/wifi.conf | grep -c network=",20))
    ok('nothing saved after a wrong password','RudolfTest' not in g.sh(W0+" list_networks",20))
    time.sleep(3)
    try_pw('correct horse battery')
    ok('connected',w("document.querySelector('.wfcur.on')&&document.querySelector('.wfcur').innerText.includes('RudolfTest')",120),g.sh("cat /run/rudolf/job.log; "+W0+" status",20))
    st=wait_sh("ip -4 -o addr show dev wlan0",'10.42.0.',60);ok('address from DHCP','10.42.0.' in st,st)
    ok('ping the access point','0% packet loss' in g.sh("ping -c 3 -W 2 10.42.0.1",30))
    out(g.sh(W0+" status | grep -E 'key_mgmt|wpa_state|freq'; ip route",20))
    ok('saved, root only',('RudolfTest' in g.sh("grep ssid /persist/etc/wifi.conf; xxd -p /persist/etc/wifi.conf | head -0",20) or '526' in g.sh("grep ssid /persist/etc/wifi.conf",20)) and g.sh("stat -c %a /persist/etc/wifi.conf",20).strip()=='600',g.sh("stat -c %a /persist/etc/wifi.conf; grep -v psk /persist/etc/wifi.conf",20))
    ok('taskbar shows the Wi-Fi signal',w("!!document.getElementById('wftray')&&document.getElementById('wftray').classList.contains('on')",60))
    time.sleep(6);ok('saved list on the page',w("document.querySelector('.wfsv').innerText.includes('RudolfTest')",30));shot('connected')
    # disconnect and reconnect from the page
    g.js("document.querySelector('.wfdis').click()");ok('disconnected',bool(wait_sh(W0+" status",'wpa_state=DISCONNECTED',40)))
    st=wait_sh("ip -4 -o addr show dev wlan0 | grep -c inet || true",'0',30);ok('address dropped','0' in st,st)
    ok('page shows not connected',w("!document.querySelector('.wfcur.on')",30))
    g.js("document.querySelector('.wfrc').click()");ok('reconnected',bool(wait_sh(W0+" status",'wpa_state=COMPLETED',60)))
    ok('address again','10.42.0.' in wait_sh("ip -4 -o addr show dev wlan0",'10.42.0.',60))
    # as after a restart: the Wi-Fi service starts and joins the saved network by itself
    g.sh("kill $(cat /run/rudolf/wpa-ev-wlan0.pid) $(cat /run/rudolf/wpa-wlan0.pid) $(cat /run/rudolf/udhcpc-wlan0.pid) 2>/dev/null; sleep 2; ip addr flush dev wlan0; rm -f /run/rudolf/wpa-*.pid",30)
    ok('service stopped','wpa_supplicant' not in g.sh("ps -eo args | awk '$1==\"wpa_supplicant\"'",20))
    g.sh("(/usr/lib/rudolf/wifi.sh boot >/dev/null 2>&1 &)",20)
    st=wait_sh("ip -4 -o addr show dev wlan0",'10.42.0.',90);ok('joins the saved network at start-up','10.42.0.' in st,st)
    # the router changes to WPA3 only: the saved network still works (SAE)
    g.sh("kill $(cat /run/hostapd.pid); sleep 1; /mnt/wifi/hostapd -B -P /run/hostapd.pid /mnt/wifi/ap-wpa3.conf >/run/hostapd.log 2>&1; sleep 1; ip addr add 10.42.0.1/24 dev wlan1 2>/dev/null; true",30)
    st=wait_sh(W0+" status",'key_mgmt=SAE',120);ok('WPA3 (SAE) works','wpa_state=COMPLETED' in st,st+g.sh('tail -5 /run/hostapd.log',20))
    ok('address on WPA3','10.42.0.' in wait_sh("ip -4 -o addr show dev wlan0",'10.42.0.',60))
    # country
    r=g.js("rdPost('wifi/country?cc=DE').then(r=>r.message).catch(e=>'ERR '+e.message)",30);ok('country set',r=='Country set to DE',r)
    ok('country saved','country=DE' in g.sh("grep country /persist/etc/wifi.conf",20))
    # forget
    time.sleep(6);g.js("(()=>{const b=document.querySelector('.wfsv button');b.click();setTimeout(()=>b.click(),500);return 1})()")
    ok('forgotten',bool(wait_sh("echo N=$(grep -c ssid= /persist/etc/wifi.conf)",'N=0',40)),g.sh("cat /persist/etc/wifi.conf | grep -v psk",20))
    ok('disconnected after forgetting',bool(wait_sh(W0+" status",'wpa_state=',20)) and 'COMPLETED' not in g.sh(W0+" status",20))
    ok('page: no saved networks',w("document.querySelector('.wfsv').innerText.includes('No saved networks')",30))
    shot('final')
    out(g.sh("tail -20 /run/hostapd.log",20))
elif ph=='G':
    # Debian 13 end to end: setup from a signed base on the second disk, the Terminal, sudo + APT from a local
    # repository standing in for Debian's mirrors, a graphical app in the Start menu and taskbar, Open with,
    # the sandbox's walls, a restart, then removal.
    import base64
    def shot(name):
        b64=g.js("fetch('/api/screenshot',{headers:{'X-Rudolf-Token':RD.token}}).then(r=>r.blob()).then(b=>new Promise(r=>{const f=new FileReader();f.onload=()=>r(f.result.split(',')[1]);f.readAsDataURL(b)}))",60)
        open(f'{N}_{name}.png','wb').write(base64.b64decode(b64))
    def signin():
        g.js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
        ok('signed in',w("!!USER",120));g.js("USER.r='admin';1")   # the terminal is for accounts other than Guest
    def tsend(t,keep=False): g.js(f"(()=>{{const W=wins.find(w=>w.term);const s=W.term.cur().s;if(!{str(keep).lower()})s.t.clear();s.send({json.dumps(t)});return 1}})()",20)
    def tscreen(): return str(g.js("(()=>{const W=wins.find(w=>w.term);if(!W)return '';const t=W.term.cur().s.t;let o=[];for(let i=0;i<t.buffer.active.length;i++){const l=t.buffer.active.getLine(i).translateToString(true);if(l.trim())o.push(l)}return o.join('\\n')})()",20))
    def twait(want,t=120):
        e=time.time()+t
        while time.time()<e:
            s=tscreen()
            if want in s: return s
            time.sleep(3)
        return tscreen()
    ok('desktop up',w("typeof rd=='function'&&!!RD"))
    out(g.sh("cat /home/rudolf/.ssh 2>/dev/null; mkdir -p /mnt/deb; grep -q ' /mnt/deb ' /proc/mounts || mount /dev/vdb /mnt/deb; pgrep -f 'httpd -p 127.0.0.1:8097' >/dev/null || busybox httpd -p 127.0.0.1:8097 -h /mnt/deb; grep -q \"$(cat /mnt/deb/base/testkey.pub)\" /etc/rudolf/rpk-keys || cat /mnt/deb/base/testkey.pub >> /etc/rudolf/rpk-keys; ls /mnt/deb/base; ls -la /usr/bin/rudolf-debian",40))
    signin()
    r=g.js("lxApi('debian/status').then(j=>JSON.stringify(j)).catch(e=>'ERR '+e.message)",30);ok('status: not set up','"installed":0' in str(r),r)
    r=g.js("lxApi('debian/settings?url='+encodeURIComponent('http://127.0.0.1:8097/base')).then(j=>j.message).catch(e=>'ERR '+e.message)",30);ok('test address saved',r=='saved',r)
    g.js("terminalApp();1")
    ok('setup page',w("!!document.querySelector('.dsetup .dsgo')",60),g.js("(document.querySelector('.dsetup')||{}).innerText"))
    shot('setup')
    g.js("(()=>{const f=document.querySelector('.dsform');f.querySelector('.dp1').value='abc';f.querySelector('.dp2').value='abc';f.querySelector('.dsgo').click();return 1})()")
    ok('short password refused',w("document.querySelector('.dserr').textContent.includes('4 characters')",20))
    g.js("(()=>{const f=document.querySelector('.dsform');f.querySelector('.dp1').value='secret12';f.querySelector('.dp2').value='secret12';f.querySelector('.dsgo').click();return 1})()")
    ok('setup shows progress',w("!document.querySelector('.dsprog').hidden",30))
    ok('setup finished, terminal open',w("!!wins.find(w=>w.term)&&!!wins.find(w=>w.term).term.cur()",420),g.js("(document.querySelector('.dsetup')||{innerText:''}).innerText")+' | '+g.sh('cat /run/rudolf/job.log | grep -v progress | tail -5',20))
    s=twait('rudolf@rudolf',60);ok('shell prompt',('rudolf@rudolf' in s),s[-300:])
    out(g.sh("/usr/bin/rudolf-debian status --size; ls -ln /persist/debian | head -4",30))
    r=g.js("(async()=>{const d=node(homePath('DOCUMENTS'));d['NOTE.TXT']='hello from drive C';fsSave();await lxSync();await new Promise(r=>setTimeout(r,800));await lxSync();return 'ok'})()",120)
    wait_sh('cat /home/rudolf/Documents/NOTE.TXT','hello from drive C',60)
    tsend('id; cat /etc/debian_version; cat ~/Documents/NOTE.TXT; echo\r');s=twait('hello from drive C',30)
    ok('user rudolf, Debian 13, shared Documents','uid=1000(rudolf)' in s and '13.' in s and 'hello from drive C' in s,s[-400:])
    tsend('sudo true\r');twait('Password:',60);time.sleep(1);tsend('wrongpass\r',True);s=twait('Authentication failure',40);ok('sudo: wrong password refused','Authentication failure' in s,s[-200:])
    tsend('sudo sh -c "mv /etc/apt/sources.list.d/debian.sources /root/ ; echo deb [trusted=yes] http://127.0.0.1:8097/apt ./ > /etc/apt/sources.list.d/test.list && apt-get update && apt-get install -y rudolf-hello rudolf-xtest && echo APT-\"\"DONE"\r')
    twait('Password:',60);time.sleep(1);tsend('secret12\r',True)
    s=twait('APT-DONE',900);ok('sudo apt install',('Setting up rudolf-xtest' in s) and ('APT-DONE' in s),s[-500:])
    tsend('rudolf-hello\r');s=twait('Hello from Debian',30);ok('installed program runs','Hello from Debian on Rudolf OS' in s)
    tsend('ls /persist /mnt/deb 2>&1 | head -2; sudo sh -c "touch /sys/kernel/x"\r');twait('Password:',60);time.sleep(1);tsend('secret12\r',True);s=twait('Permission denied',30)
    ok('sandbox: no Rudolf OS data, no system writes',('No such file' in s) and ('Permission denied' in s),s[-300:])
    out(g.sh("stat -c '%u %n' /persist/debian/usr/bin/rudolf-hello /persist/debian/etc/shadow",20))
    ok('Debian root is not Rudolf root',g.sh("stat -c %u /persist/debian/usr/bin/rudolf-hello",20).strip()=='200000')
    shot('terminal')
    # graphical app: Start menu, taskbar, window, sound socket, Open with
    r=g.js("debApps().then(a=>JSON.stringify(a)).catch(e=>'ERR '+e.message)",30);ok('Debian app listed','Rudolf X Test' in str(r),r)
    ok('Start menu entry',w("startMenuApps(),[...document.querySelectorAll('#menu li.lxv')].some(l=>l.textContent.includes('Rudolf X Test'))",30))
    g.js("lxRun(lxLaunchers().find(x=>x.deb=='rudolf-xtest'));1")
    st=wait_sh("cat /run/rudolf/windows; cat /run/rudolf/wm",'RudolfXTest',120);ok('app window shown',('RudolfXTest' in st) and 'win ' in st,st+g.sh('cat /run/rudolf/debian/rudolf-xtest.log',20))
    lg=g.sh('cat /run/rudolf/debian/rudolf-xtest.log',20);ok('app reached display and sound','window shown' in lg and 'sound socket: ok' in lg,lg)
    time.sleep(3);ok('taskbar button with the app name',w("[...document.querySelectorAll('.lxtb')].some(b=>b.textContent.includes('Rudolf X Test'))",30),g.js("[...document.querySelectorAll('.lxtb')].map(b=>b.textContent).join(';')"))
    shot('xtest')
    win=g.js("document.querySelector('.lxtb').dataset.win");g.js(f"lxApi('apps/close?win={win}')")
    st=wait_sh('cat /run/rudolf/debian/rudolf-xtest.log','closed',60);ok('app closed','closed' in st,st+g.sh('cat /run/rudolf/windows',20))
    ok('Open with lists it',g.js("openersFor([...homePath('DOCUMENTS'),'NOTE.TXT']).some(o=>String(o[0])=='lx:deb-rudolf-xtest')"))
    r=g.js("(async()=>{const t0=Date.now();await lxOpenFile(lxLaunchers().find(x=>x.deb=='rudolf-xtest'),[...homePath('DOCUMENTS'),'NOTE.TXT']);return 'done in '+(Date.now()-t0)+' ms'})().catch(e=>'ERR '+e.message)",120);out('open with:',r)
    st=wait_sh('cat /run/rudolf/debian/rudolf-xtest.log','file: hello from drive C',90);ok('opened a file from drive C:','file: hello from drive C' in st,g.sh('cat /run/rudolf/debian/rudolf-xtest.log; tail -3 /run/rudolf/rudolfd.log',20))
    win=g.js("(document.querySelector('.lxtb')||{dataset:{}}).dataset.win");g.js(f"lxApi('apps/close?win={win}')");time.sleep(3)
    # restart: Debian and what was installed are kept
    g.js("wins.filter(w=>w.term).forEach(w=>w.close());1");time.sleep(2)
    reboot();time.sleep(30)   # the boot menu is answered by the driver (drive11.py)
    ok('desktop up again',w("typeof rd=='function'&&!!RD",600))
    signin()
    g.js("terminalApp();1");ok('terminal opens after restart',w("!!wins.find(w=>w.term)&&!!wins.find(w=>w.term).term.cur()",120))
    twait('rudolf@rudolf',60);tsend('rudolf-hello\r');s=twait('Hello from Debian',30);ok('installed program kept','Hello from Debian on Rudolf OS' in s,s[-300:])
    ok('app still in the Start menu',w("startMenuApps(),[...document.querySelectorAll('#menu li.lxv')].some(l=>l.textContent.includes('Rudolf X Test'))",60))
    g.js("wins.filter(w=>w.term).forEach(w=>w.close());1");time.sleep(2)
    # removal keeps the user's files (a file made directly in the real folder: Guest's drive C: is new after a restart)
    g.sh("echo kept > /home/rudolf/Documents/KEEP.TXT; chown 1000:1000 /home/rudolf/Documents/KEEP.TXT",20)
    r=g.js("(async()=>{await lxApi('debian/remove');const j=await lxJobWait();return j.status+'|'+j.log})().catch(e=>'ERR '+e.message)",180);ok('removed',str(r).startswith('0|'),r)
    ok('space freed',g.sh("ls /persist/debian 2>&1 | head -1",20).find('No such file')>=0)
    ok('Documents kept','kept' in g.sh('cat /home/rudolf/Documents/KEEP.TXT',20))
    r=g.js("lxApi('debian/status').then(j=>j.installed).catch(e=>'ERR '+e.message)",30);ok('status: not set up again',str(r)=='0',r)
elif ph=='E':
    # Emu on the real system: the game library (Add games from Downloads, with a .zip), a DS and a SNES game,
    # buttons, a save state in the library's States folder, borderless mode over the taskbar.
    import base64
    def shot(name):
        b64=g.js("fetch('/api/screenshot',{headers:{'X-Rudolf-Token':RD.token}}).then(r=>r.blob()).then(b=>new Promise(r=>{const f=new FileReader();f.onload=()=>r(f.result.split(',')[1]);f.readAsDataURL(b)}))",60)
        open(f'{N}_{name}.png','wb').write(base64.b64decode(b64))
    P="[...EMU.players].at(-1)"
    def lower(): return g.js(f"(()=>{{const W={P};const cv=W.emuApi.canvas,[x,y,w,h]=W.emuApi.lowerRect();const d=cv.getContext('2d').getImageData(x+Math.floor(w*10/256),y+Math.floor(h*10/192),1,1).data;return [d[0],d[1],d[2]]}})()",20)
    ok('desktop up',w("typeof rd=='function'&&!!RD"))
    out(g.sh("mkdir -p /mnt/g; grep -q ' /mnt/g ' /proc/mounts || mount -o ro /dev/vdb /mnt/g; mkdir -p /home/rudolf/Downloads; cp /mnt/g/*.sfc /mnt/g/*.nds /mnt/g/*.zip /home/rudolf/Downloads/; chown -R 1000:1000 /home/rudolf/Downloads; rm -rf /home/rudolf/Games; ls -la /home/rudolf/Downloads",40))
    g.js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
    ok('signed in',w("!!USER",120));g.js("USER.r='admin';1")
    r=g.js("lxApi('games/info').then(j=>JSON.stringify(j)).catch(e=>'ERR '+e.message)",30);ok('library info (installed disk)','"persistent":1' in str(r).replace(' ','') or '"persistent":true' in str(r).replace(' ',''),r)
    g.js("APPS.gameboy.open();1");ok('Emu opens the library',w("!!document.querySelector('.emulib .elgrid')",60))
    g.js("document.querySelector('.eladd').click();1");ok('Add games lists Downloads',w("document.querySelectorAll('.ealist input').length==4",60),g.js("(document.querySelector('.emuadd')||{}).innerText"))
    g.js("document.querySelector('.eaall').click();document.querySelector('.eago').click();1")
    ok('import finished',w("!!document.querySelector('.eastep')&&/Finished: 4 added/.test(document.querySelector('.eastep').textContent)",120),g.js("(document.querySelector('.eares')||{}).innerText"))
    out(g.sh("find /home/rudolf/Games -type f | sort; stat -c '%U %n' /home/rudolf/Games/DS/rudolftest.nds",20))
    ok('games in Games/SNES and Games/DS, owned by rudolf','rudolf /home' in g.sh("stat -c '%U %n' /home/rudolf/Games/DS/rudolftest.nds",20) and 'CPUADC.sfc' in g.sh('ls /home/rudolf/Games/SNES',20))
    ok('library titles',w("document.querySelector('.elgrid').innerText.includes('RUDOLF TEST')&&document.querySelector('.elgrid').innerText.includes('MONSTER FARM JUMP')",60),g.js("document.querySelector('.elgrid').innerText.replace(/\\n/g,' | ')"))
    g.js("wins.filter(w=>w.el.querySelector('.emuadd')).forEach(w=>w.close());1")
    shot('lib')
    g.js("document.querySelector(\".eltabs [data-t='ds']\").click();document.querySelector('.emulib .eltile').click();1")
    ok('DS game starts',w("[...EMU.players].some(w=>w.emuApi&&w.emuApi.frames()>30)",120))
    time.sleep(2);c=lower();ok('DS picture (dark blue lower screen)',c[2]>60 and c[0]<30,c)
    f0=g.js(f"{P}.emuApi.frames()");time.sleep(5);f1=g.js(f"{P}.emuApi.frames()");out('DS frames per second in QEMU (no KVM):',(f1-f0)/5)
    g.js(f"{P}.emuApi.hold('A',true);1");time.sleep(1.5);c=lower();ok('A button (red)',c[0]>200 and c[2]<40,c);g.js(f"{P}.emuApi.hold('A',false);1")
    shot('ds')
    g.js(f"{P}.emuApi.saveState();1");st=wait_sh('ls /home/rudolf/Games/DS/States','rudolftest.st1png',60);ok('state saved in Games/DS/States','rudolftest.st1' in st,st)
    g.js(f"{P}.emuApi.loadState();1");ok('state loads',w(f"{P}.body.querySelector('.emsg').textContent=='State 1 loaded'",40),g.js(f"{P}.body.querySelector('.emsg').textContent"))
    g.js(f"{P}.body.querySelector('.ebl').click();1");time.sleep(1)
    ok('borderless covers the taskbar',g.js(f"{P}.el.classList.contains('emubl')&&getComputedStyle(document.querySelector('#bar')).display=='none'"))
    shot('borderless')
    g.js(f"{P}.emuApi.setBl(false);{P}.close();1");time.sleep(1)
    g.js("document.querySelector(\".eltabs [data-t='snes']\").click();[...document.querySelectorAll('.emulib .eltile')].find(t=>t.innerText.includes('MONSTER')).click();1")
    ok('SNES game starts',w("[...EMU.players].some(w=>w.emuApi&&w.emuApi.frames()>60)",120))
    time.sleep(3);nz=g.js(f"(()=>{{const d={P}.emuApi.canvas.getContext('2d').getImageData(0,0,512,448).data;let n=0;for(let i=0;i<d.length;i+=4)if(d[i]|d[i+1]|d[i+2])n++;return n}})()",20);ok('SNES picture drawn',nz>1000,nz)
    f0=g.js(f"{P}.emuApi.frames()");time.sleep(5);f1=g.js(f"{P}.emuApi.frames()");out('SNES frames per second in QEMU (no KVM):',(f1-f0)/5)
    shot('snes')
    g.js(f"{P}.close();1")
    ok('Files shows the library',g.js("typeof emuOpenReal=='function'"))
elif ph=='Z':
    # Blitzen on the real system: signed download (a damaged one refused), the engine as the "ai" user behind its
    # key, a chat streamed through rudolfd, Python and C++ runs in Debian with no network and a time limit,
    # memory and idle rules, a restart, removal. The model is a tiny random-weights test model (qt/mkai.sh).
    import base64
    def shot(name):
        b64=g.js("fetch('/api/screenshot',{headers:{'X-Rudolf-Token':RD.token}}).then(r=>r.blob()).then(b=>new Promise(r=>{const f=new FileReader();f.onload=()=>r(f.result.split(',')[1]);f.readAsDataURL(b)}))",60)
        open(f'{N}_{name}.png','wb').write(base64.b64decode(b64))
    def signin():
        g.js("(()=>{const o=document.getElementById('oobe');if(o)o.remove();svj('bd95setup',1);const b=[...document.querySelectorAll('button')].find(b=>b.textContent.trim()=='Guest');if(b)b.click();return 1})()")
        ok('signed in',w("!!USER",120));g.js("USER.r='admin';1")
    def api(path,body=None,t=120):
        b='' if body is None else ','+json.dumps(body)
        return g.js(f"lxApi({json.dumps(path)}{b}).then(j=>JSON.stringify(j)).catch(e=>'ERR '+e.message)",t)
    def job(t=600): return str(g.js(f"lxJobWait().then(j=>j.status+'|'+j.log).catch(e=>'ERR '+e.message)",t))
    ok('desktop up',w("typeof rd=='function'&&!!RD"))
    out(g.sh("mkdir -p /mnt/ai; grep -q ' /mnt/ai ' /proc/mounts || mount /dev/vdb /mnt/ai; pgrep -f 'httpd -p 127.0.0.1:8097' >/dev/null || busybox httpd -p 127.0.0.1:8097 -h /mnt/ai; grep -q \"$(cat /mnt/ai/base/testkey.pub)\" /etc/rudolf/rpk-keys || cat /mnt/ai/base/testkey.pub >> /etc/rudolf/rpk-keys; printf 'min_mem_mb=300\\nidle_s=90\\nthreads=2\\n' > /persist/os/blitzen.conf; ls -la /usr/lib/rudolf/llama-server /usr/bin/rudolf-ai; grep '^ai:' /etc/passwd",40))
    signin()
    capmax="(()=>{if(!window.__cap){window.__cap=1;const _s=blzStream;blzStream=(m,o,...r)=>_s(m,{...o,max:16},...r)}return 1})()"   # the random test model never ends an answer by itself; emulated AVX2 is slow
    g.js(capmax)
    r=api('ai/status');ok('status: engine on the disc, model not downloaded','"installed":0' in str(r) and '"engine":1' in str(r),r)
    api('ai/settings?url='+'http://127.0.0.1:8097/blitzen-bad');api('ai/download');r=job()
    ok('a damaged model is refused',r.startswith('1|') and 'damaged' in r,r[-300:])
    r=api('ai/settings?url='+'http://127.0.0.1:8097/blitzen');ok('test address saved','saved' in str(r),r)
    out(g.sh('cat /persist/os/blitzen.conf',10))
    g.js("blitzenApp();1");ok('setup page',w("!!document.querySelector('.blzsetup .dsgo')",60))
    shot('setup')
    g.js("document.querySelector('.blzsetup .dsgo').click();1")
    ok('download, signature and checksum pass; chat opens',w("!!document.querySelector('.blzw .blzq')",300),g.sh('cat /run/rudolf/job.log | tail -5',20))
    out(g.sh('ls -la /persist/ai/models; cat /persist/ai/models/model.txt',20))
    t0=time.time();g.js("[...BLZ.wins].at(-1).blz.ask('Hello Blitzen');1")
    ok('engine starts and an answer streams in',w("!![...document.querySelectorAll('.blzw .blza')].at(-1)&&![...BLZ.wins].at(-1).blz.busy()&&[...document.querySelectorAll('.blzw .blza')].at(-1).innerText.length>20",240) and g.js("!document.querySelector('.blzw .blzerr')"),g.js("(document.querySelector('.blzw .blzst')||{}).textContent")+' '+g.sh('tail -5 /run/rudolf/ai/engine.log',20))
    out('first answer (engine start included):',round(time.time()-t0,1),'s;',g.js("document.querySelector('.blzw .blzst').textContent"))
    shot('chat')
    ps=g.sh("ps | grep [l]lama-server",20);ok('engine runs as the ai user',' ai ' in ps,ps)
    pid=g.sh("pgrep -x llama-server | head -1",10).strip()
    key=g.sh("cat /run/rudolf/ai/key",10).strip()
    ok('the key is not on the engine command line',len(key)==36 and key not in g.sh(f"tr '\\0' ' ' < /proc/{pid}/cmdline",10))
    r=g.sh("wget -q -O - --header 'Content-Type: application/json' --post-data '{\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}' http://127.0.0.1:8099/v1/chat/completions 2>&1 | head -c 200; echo; wget -q -O /dev/null http://127.0.0.1:8099/v1/models 2>&1 | head -2",30)
    ok('the engine refuses requests without the key','401' in r or 'Unauthorized' in r,r)
    r=g.sh(f"echo home=$(ls -A /proc/{pid}/root/home | wc -l) persist=$(ls -A /proc/{pid}/root/persist | tr '\\n' ' ') media=$(ls -A /proc/{pid}/root/media | wc -l) rr=$(ls -A /proc/{pid}/root/run/rudolf | wc -l)",20);out("the engine's view:",r)
    ok('the engine sees no user files (home, USB, data partition)','home=0' in r and 'persist=ai ' in r and 'media=0' in r and 'rr=0' in r,r)
    # Debian, with stand-ins for python3 and g++ from the test disk's APT repository
    api('debian/settings?url='+'http://127.0.0.1:8097/base');api('debian/setup','secret12');r=job(900);ok('Debian set up',r.startswith('0|'),r[-300:])
    r=api('ai/status');ok('status: Debian yes, Python no','"debian":true' in str(r) and '"python":false' in str(r),r)
    r=g.js("blzRunServer('python','print(1)').then(j=>JSON.stringify(j))",60);ok('Python not installed is reported','"missing":"python"' in str(r),r)
    out(g.sh("/usr/bin/rudolf-debian run --root -- sh -c 'mv /etc/apt/sources.list.d/debian.sources /root/ 2>/dev/null; echo deb [trusted=yes] http://127.0.0.1:8097/apt ./ > /etc/apt/sources.list.d/test.list'",60))
    api('ai/install?what=python');r=job(600);ok('Python installs',r.startswith('0|'),r[-300:])
    api('ai/install?what=cpp');r=job(600);ok('g++ installs',r.startswith('0|'),r[-300:])
    r=g.js("blzRunServer('python','print(1)').then(j=>JSON.stringify(j))",90);out('python run:',r)
    ok('a Python program runs in Debian','stand-in python: print(1)' in str(r) and '"code":0' in str(r),r)
    ok('with no network (only lo)','network: lo ' in str(r) and 'eth' not in str(r),r)
    r=g.js("blzRunServer('python','raise SystemExit(1)').then(j=>JSON.stringify(j))",90);ok('a failing program reports its error','"code":1' in str(r) and 'RuntimeError' in str(r),r)
    t1=time.time();r=g.js("blzRunServer('python','while True: pass').then(j=>JSON.stringify(j))",120);el=time.time()-t1
    ok('a program that never ends is stopped','"timeout":true' in str(r),r);out('stopped after',round(el,1),'s')
    r=g.js("blzRunServer('cpp','int main(){}').then(j=>JSON.stringify(j))",120);ok('C++ compiles and runs','sum = 6' in str(r) and '"code":0' in str(r),r)
    ok('run folders are cleaned up',g.sh('ls /home/rudolf/.cache/blitzen | wc -l',10).strip()=='0')
    # memory rule
    g.sh("sed -i 's/^min_mem_mb=.*/min_mem_mb=100000/' /persist/os/blitzen.conf; /usr/bin/rudolf-ai unload",20)
    r=api('ai/load');ok('refuses to start without enough free memory','not enough free memory' in str(r),r)
    g.sh("sed -i 's/^min_mem_mb=.*/min_mem_mb=300/' /persist/os/blitzen.conf",10)
    r=api('ai/load',None,240);ok('starts again',"ready" in str(r),r)
    # idle: 90 s in this test (10 minutes normally)
    st=wait_sh("pgrep -x llama-server >/dev/null && echo running || echo stopped",'stopped',240);ok('unloads by itself when idle','stopped' in st,st)
    # restart: the model is kept
    g.js("wins.filter(w=>BLZ.wins.has(w)).forEach(w=>w.close());1");reboot();time.sleep(30)
    ok('desktop up again',w("typeof rd=='function'&&!!RD",600));signin();g.js(capmax)
    r=api('ai/status');ok('model kept after a restart','"installed":1' in str(r),r)
    g.js("blitzenApp().then(W=>W&&W.blz&&W.blz.ask('Hi again'));1")
    ok('chat works after the restart',w("!!document.querySelector('.blzw .blza')&&![...BLZ.wins].at(-1).blz.busy()&&document.querySelector('.blzw .blza').innerText.length>20",300) and g.js("!document.querySelector('.blzw .blzerr')"))
    r=api('ai/remove',None,60);ok('removed','removed' in str(r),r)
    ok('model files gone',g.sh('ls /persist/ai/models 2>&1 | head -1',10).find('No such')>=0)
    r=api('ai/status');ok('status: not downloaded again','"installed":0' in str(r),r)
