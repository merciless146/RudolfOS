# 1.6.1 (docs/plan-1.6.1.md Part 3, from 1.5.8: the dynamic backgrounds' moving parts in Lite): the real desktop page in
# rudolf-webkit on a virtual screen (wkpage.py), as iso/qt/shell2/aurora58_t.js and statue58_t.js check them in Chromium:
# Glacier Lake at night, the northern lights on (Lite's slower timer), in the sky and not on the mountains, moving, and still
# with Reduce motion; Spirit Falls by day, its 70 cherry petals blowing across; at night its 28 sky lanterns, kept to the sky
# by its mask, moving. And on the screen: two screenshots a moment apart differ where they move (the sky, the picture).
#   python3 dynbg61_ci.py ./rudolf-webkit [DRAW]   (DISPLAY set; DRAW: soft (default), mixed or gpu)
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wkpage import Page, JS_HEAD, shot, changed, Checks
BIN = sys.argv[1]; DRAW = sys.argv[2] if len(sys.argv) > 2 else 'soft'
TEST = '<script>' + JS_HEAD + r'''
(function(){
 const alpha=(cv,x0,y0,x1,y1)=>{const d=cv.getContext('2d').getImageData(x0,y0,x1-x0,y1-y0).data;let a=0;for(let i=3;i<d.length;i+=4)a+=d[i];return a/(d.length/4)};
 addEventListener('load',async()=>{try{await until(()=>typeof aurUpdate=='function'&&typeof stUpdate=='function'&&typeof DYN!='undefined'&&typeof applySet=='function',40000);
  try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];
  await post('up',{webkit:document.documentElement.classList.contains('webkit')});
  S.accent='#16a34a';S.aur=true;S.aurm='normal';S.rmotion=false;S.dyn='lake';S.dm='dark';applySet();
  await until(()=>AUR.on&&AUR.timer&&AUR.mask&&AUR.mask.complete,20000).catch(()=>0);await wait(1500);
  await post('aur',{on:AUR.on,timer:!!AUR.timer,first:document.body.firstChild&&document.body.firstChild.id,pe:getComputedStyle(document.getElementById('aurora')).pointerEvents,
   sky:alpha(AUR.cv,150,20,330,50),rock:alpha(AUR.cv,160,75,230,110),lake:alpha(AUR.cv,150,230,300,266)});
  {const f1=AUR.cv.toDataURL();await wait(1000);await post('aurmove',f1!=AUR.cv.toDataURL())}
  await post('shotlake1',true);await wait(2500);await post('shotlake2',true);await wait(2500);
  S.rmotion=true;applySet();await wait(400);{const g1=AUR.cv.toDataURL();await wait(800);await post('aurstill',{timer:!!AUR.timer,same:g1==AUR.cv.toDataURL(),on:AUR.on})}
  S.rmotion=false;S.dyn='oasis';S.dm='light';applySet();await until(()=>PET.timer&&STS.img&&STS.img.naturalWidth>0,20000).catch(()=>0);await wait(800);
  {const c1=PET.cv.toDataURL();await wait(800);await post('pet',{n:PET.ps.length,timer:!!PET.timer,moved:c1!=PET.cv.toDataURL(),statue:!!STS.el&&STS.el.style.display=='',aur:AUR.on})}
  S.dm='dark';applySet();await until(()=>PET.mode=='dark'&&PET.mask&&PET.mask.complete&&PET.mask.naturalWidth>0,20000).catch(()=>0);await wait(1200);
  {const l1=PET.lcv.toDataURL(),a1=alpha(PET.lcv,0,0,800,300);await wait(1000);
   await post('lan',{mode:PET.mode,n:PET.ls.length,mask:PET.mask&&PET.mask.naturalWidth,lit:a1,moved:l1!=PET.lcv.toDataURL(),timer:!!PET.timer})}
  await post('shotoasis1',true);await wait(2500);await post('shotoasis2',true);await wait(2500);
  await post('done',true)}catch(e){post('error',String(e&&e.stack||e))}})})();</script>'''
P = Page(TEST, 8607); RES = P.res; p = P.run(BIN, DRAW)
shots = {}; end = time.time() + 180
while time.time() < end and 'done' not in RES and 'error' not in RES and p.poll() is None:
    for k in ('shotlake1', 'shotlake2', 'shotoasis1', 'shotoasis2'):
        if k in RES and k not in shots: time.sleep(0.5); shots[k] = shot(f'dynbg61-{DRAW}-{k[4:]}.png')
    time.sleep(0.2)
ok = Checks(); T = f'({DRAW}) '
ok(T + '0. the desktop is up in WebKit (the page knows it is Lite)', (RES.get('up') or {}).get('webkit') is True, RES.get('error') or RES)
a = RES.get('aur') or {}
ok(T + '1. Glacier Lake at night: the northern lights on, moving (a timer), under everything, not in the way', a.get('on') and a.get('timer') and a.get('first') == 'aurora' and a.get('pe') == 'none', a)
ok(T + '2. ...in the sky, not on the mountains, and in the lake', (a.get('sky') or 0) > 8 and a.get('rock', 9) < 2 and (a.get('lake') or 0) > 4, a)
ok(T + '3. ...they move', RES.get('aurmove') is True, RES)
n = changed(shots.get('shotlake1', ''), shots.get('shotlake2', ''), (480, 40, 570, 90)); print('pixels changed in the sky:', n)
ok(T + '4. ...and on the screen: the sky changes from one moment to the next', n > 200, n)
s = RES.get('aurstill') or {}
ok(T + '5. Reduce motion: the lights stand still', s.get('on') and not s.get('timer') and s.get('same'), s)
pt = RES.get('pet') or {}
ok(T + '6. Spirit Falls by day: the statue, and 70 cherry petals blowing across (moving)', pt.get('statue') and pt.get('n') == 70 and pt.get('timer') and pt.get('moved') and not pt.get('aur'), pt)
ln = RES.get('lan') or {}
ok(T + '7. at night: 28 sky lanterns, kept to the sky by the mask, lit and moving', ln.get('mode') == 'dark' and ln.get('n') == 28 and ln.get('mask') == 400 and (ln.get('lit') or 0) > 0.5 and ln.get('moved') and ln.get('timer'), ln)
n2 = changed(shots.get('shotoasis1', ''), shots.get('shotoasis2', ''), (0, 0, 1280, 800)); print('pixels changed on the screen:', n2)
ok(T + '8. ...and on the screen: the petals and lanterns move', n2 > 200, n2)
ok(T + '9. no page errors', not P.errors and 'error' not in RES, P.errors or RES.get('error'))
P.stop(); sys.exit(ok.end())
