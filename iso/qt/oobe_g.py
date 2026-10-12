import sys,time;sys.path.insert(0,'.')
from guest import Guest
g=Guest(sys.argv[1])
def w(c,t=30):
    e=time.time()+t
    while time.time()<e:
        if g.js(c) is True: return True
        time.sleep(1)
    return False
print('oobe',w("!!document.querySelector('#oobe .oobox')"))
g.click_sel('#oobe .nx');time.sleep(2);print('step2',g.js("document.querySelector('#oobe h2').textContent"))
g.click_sel('#oobe .nx');time.sleep(2);print('step3',g.js("document.querySelector('#oobe h2').textContent"))
g.click_sel('#oobe .nm');time.sleep(0.5);g.type('Alice');g.click_sel('#oobe .p1');g.type('secret1');g.click_sel('#oobe .p2');g.type('secret1');time.sleep(1)
print('name',g.js("document.querySelector('#oobe .nm').value"));g.shot('o_3.png')
g.click_sel('#oobe .nx');time.sleep(2);print('step4',g.js("document.querySelector('#oobe h2')&&document.querySelector('#oobe h2').textContent"))
g.click_sel('#oobe .nx');time.sleep(2);g.click_sel('#oobe .nx');time.sleep(3)
print('closed',g.js("!document.getElementById('oobe')"),g.js("document.getElementById('lu').value"))
g.click_sel('#lp');time.sleep(0.5);g.type('secret1');g.key('ret')
print('login',w("!!USER&&LG.style.display=='none'",60));time.sleep(5);g.shot('o_4.png')
print(g.sh('cat /run/rudolf/page.log 2>/dev/null | tail -5'))
