import sys,time;sys.path.insert(0,'.')
from guest import Guest
g=Guest(sys.argv[1])
def w(c,t=30):
    e=time.time()+t
    while time.time()<e:
        if g.js(c) is True: return True
        time.sleep(1)
    return False
g.js("settings('upd')")
print('disk row',w("!!document.querySelector('.dsk .acr button')",60))
print(g.js("[...document.querySelectorAll('.dsk .acr')].map(r=>r.textContent)"),g.js("document.querySelector('.ui')&&document.querySelector('.ui').textContent"))
g.shot('inst_1.png')
g.click_sel('.dsk .acr button');time.sleep(3);g.shot('inst_2.png')
print('elevate',g.js("[...document.querySelectorAll('.rmodal,.sdlg,[role=dialog]')].map(d=>d.className+':'+d.textContent.slice(0,80))"))
