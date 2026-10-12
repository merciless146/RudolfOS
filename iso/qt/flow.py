# Full install flow: python3 flow.py PHASE NAME   (phases: live-install, disk-first, disk-second)
import sys,time,os,subprocess;sys.path.insert(0,'.')
from guest import Guest
ph,N=sys.argv[1],sys.argv[2]
g=Guest(N)
def w(c,t=60):
    e=time.time()+t
    while time.time()<e:
        if g.js(c) is True: return True
        time.sleep(2)
    return False
def oobe(name,pw):
    assert w("!!document.querySelector('#oobe .oobox')",240),'no oobe'
    g.click_sel('#oobe .nx');time.sleep(1.5);g.click_sel('#oobe .nx');time.sleep(1.5)
    g.click_sel('#oobe .nm');g.type(name);g.click_sel('#oobe .p1');g.type(pw);g.click_sel('#oobe .p2');g.type(pw);time.sleep(0.5)
    for i in range(3): g.click_sel('#oobe .nx');time.sleep(1.5)
    assert w("!document.getElementById('oobe')",20),'oobe stuck'
def login(pw):
    assert w("typeof LG=='object'&&LG.style.display!='none'",240),'no login'
    g.click_sel('#lp');time.sleep(0.5);g.type(pw);g.key('ret')
    return w("!!USER&&LG.style.display=='none'",60)
if ph=='live-install':
    oobe('Alice','secret1');print('login',login('secret1'))
    g.js("settings('upd')");assert w("!!document.querySelector('.dsk .acr button')",60)
    g.click_sel('.dsk .acr button');time.sleep(3);g.type('secret1');g.key('ret');time.sleep(3)
    r=g.js('(()=>{const b=[...document.querySelectorAll("button")].find(b=>/Erase and install/.test(b.textContent));const r=b.getBoundingClientRect();return [r.x+r.width/2,r.y+r.height/2]})()');g.q.at(*r)
    ok=w("[...document.querySelectorAll('h3,h2,b,div')].some(e=>e.textContent.trim()=='Rudolf OS is installed')",600)
    print('installed',ok);g.shot(N+'_installed.png')
    g.js("rdPost('power?action=poweroff')")
elif ph=='disk-first':
    print(g.js('RUDOLFD'));print(g.sh('cat /etc/rudolf/persist; mount | grep persist',20))
    oobe('Bob','hunter22');print('login',login('hunter22'))
    print(g.js("(async()=>{vfsWrite(['C:','USERS',uname(),'DOCUMENTS','PERSIST.TXT'],'hello from first boot');await fsFlush();return true})()"))
    print(g.js("JSON.stringify(Object.keys(node(['C:','USERS',uname(),'DOCUMENTS'])||{}))"))
    g.shot(N+'_first.png');time.sleep(3);g.js("A.off()");time.sleep(2)
    r=g.js('(()=>{const b=[...document.querySelectorAll(".rmodal button")].find(b=>/Shut down/.test(b.textContent));const r=b.getBoundingClientRect();return [r.x+r.width/2,r.y+r.height/2]})()');g.q.at(*r)
elif ph=='disk-second':
    w("typeof LG=='object'",240);time.sleep(4);print('no oobe',g.js("!document.getElementById('oobe')"))
    print(g.js("document.getElementById('lu').value"));print('login',login('hunter22'))
    time.sleep(3);print(g.js("JSON.stringify(Object.keys(node(['C:','USERS',uname(),'DOCUMENTS'])||{}))"))
    g.shot(N+'_second.png')
