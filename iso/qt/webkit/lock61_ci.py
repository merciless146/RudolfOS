# 1.6.1 (docs/plan-1.6.1.md Part 3, from 1.5.8: Lite's lock and sign-in screen settings and the shutting-down screen): the
# real desktop page (iso/www) in rudolf-webkit on a virtual screen, with a stand-in rudolfd, the same steps as
# iso/qt/shell2/lock58_t.js takes in Chromium: Display Properties › Lock and sign-in screen (a colour, no darkening, the
# small clock at the bottom left, no account name) kept for the computer and shown on the lock screen; a picture of one's
# own, blurred, no clock; the desktop's background remembered for the sign-in page with its clock; Shut down: the full
# screen "Shutting down…" at once over everything, the power asked for after the tone. And what is on the screen (the
# lock screen's colour, the shutting-down screen), read from screenshots.   python3 lock61_ci.py ./rudolf-webkit
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wkpage import Page, JS_HEAD, shot, px, Checks
BIN = sys.argv[1]; M = {'power': None}
TEST = '<script>' + JS_HEAD + r'''
(function(){
 addEventListener('load',async()=>{try{await until(()=>typeof ssRun=='function'&&typeof launch=='function'&&typeof lockScreen=='function',40000);
  try{svj('bd95setup',1)}catch(e){}const o=document.getElementById('oobe');o&&o.remove();const l=document.getElementById('login');if(l)l.style.display='none';
  wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;await post('up',!!USER);
  launch('settings');await wait(800);
  {const W=wins.find(w=>w.app=='settings');const b=[...W.body.querySelectorAll('*')].find(e=>e.children.length==0&&/^Display Properties$/.test(e.textContent.trim()));b&&b.click()}await wait(800);
  await post('section',!!document.querySelector('.lk58'));
  {const d=document.querySelector('.lk58'),b=document.querySelector('#b1').parentElement,f=document.querySelector('#ff');
   await post('grid',d.previousElementSibling===b&&!!(d.compareDocumentPosition(f)&Node.DOCUMENT_POSITION_FOLLOWING)&&getComputedStyle(d).display=='contents')}
  {const d=document.querySelector('.lk58');const s=(id,v)=>{const e=d.querySelector(id);e.value=v;e.dispatchEvent(new Event('change'))};s('#lkbg','color');
   const c=d.querySelector('#lkcol');c.value='#aa0000';c.dispatchEvent(new Event('change'));const r=d.querySelector('#lkdim');r.value=0;r.dispatchEvent(new Event('input'));
   s('#lkck','small');s('#lkpos','bottom');const u=d.querySelector('#lkuser');u.checked=false;u.dispatchEvent(new Event('change'))}
  await post('kept',SYSC.lk||null);
  wins.slice().forEach(w=>w.close());lockScreen();await wait(600);
  {const o=document.getElementById('lockov');await post('lock',{bg:[...o.querySelectorAll('.lkbgl')].map(x=>getComputedStyle(x).backgroundColor),ck:getComputedStyle(o.querySelector('.lkclock')).fontSize,
   av:getComputedStyle(o.querySelector('.lkav')).display,jc:o.style.justifyContent,ai:o.style.alignItems})}
  await post('shotlock',true);await wait(2500);
  document.getElementById('lockov').remove();LOCKED=false;
  {const c=document.createElement('canvas');c.width=40;c.height=20;const x=c.getContext('2d');x.fillStyle='#00aa00';x.fillRect(0,0,40,20);svj('bd95lkpic',c.toDataURL('image/jpeg',.9));
   SYSC.lk=Object.assign({},SYSC.lk,{bg:'picture',blur:6,clock:'off',user:true});sysSave()}
  lockScreen();await wait(600);
  {const o=document.getElementById('lockov');const l=[...o.querySelectorAll('.lkbgl')];await post('pic',{img:l.map(x=>getComputedStyle(x).backgroundImage).join(' ').slice(0,60),
   blur:l.map(x=>getComputedStyle(x).filter).join(' '),ck:getComputedStyle(o.querySelector('.lkclock')).display,av:getComputedStyle(o.querySelector('.lkav')).display})}
  await post('shotpic',true);await wait(2500);
  document.getElementById('lockov').remove();LOCKED=false;
  SYSC.lk=Object.assign({},SYSC.lk,{bg:'desktop',blur:0,clock:'large'});sysSave();document.body.style.backgroundImage='linear-gradient(rgb(1, 2, 3), rgb(4, 5, 6))';
  lockScreen();await wait(600);
  await post('desk',[...document.getElementById('lockov').querySelectorAll('.lkbgl')].map(x=>getComputedStyle(x).backgroundImage).join(' ').slice(0,200));
  document.getElementById('lockov').remove();LOCKED=false;
  showLogin();await wait(1500);
  {const L=document.getElementById('login');await post('login',{bg:[...L.querySelectorAll('.lkbgl')].map(x=>getComputedStyle(x).backgroundImage).join(' ').slice(0,200),ck:(L.querySelector('.lkck .lkclock')||{}).textContent||''})}
  USER=USER||ACCTS[0];document.getElementById('login').style.display='none';S.snd.master.on=true;S.snd.shutdown={on:true,vol:100};
  await post('t0',Date.now());shutdownDlg();await wait(300);document.querySelector('.rmodal button[data-a=poweroff]').click();await wait(400);
  {const o=document.getElementById('shutov');await post('shut',o?{t:o.querySelector('.shutt').textContent,z:+getComputedStyle(o).zIndex,img:!!o.querySelector('img')}:null)}
  await post('len',sfxLen('shutdown'));await post('shotshut',true);await wait(4000);await post('done',true)}catch(e){post('error',String(e&&e.stack||e))}})})();</script>'''
def api(a, q):
    if a == 'power' and 'action=' in (q or ''):
        if not M['power']: M['power'] = {'at': time.time() * 1000, 'q': q}
        return {'ok': True}
P = Page(TEST, 8606, api); RES = P.res; p = P.run(BIN)
shots = {}; end = time.time() + 150
while time.time() < end and 'done' not in RES and 'error' not in RES and p.poll() is None:
    for k in ('shotlock', 'shotpic', 'shotshut'):
        if k in RES and k not in shots: time.sleep(0.8); shots[k] = shot('lock61-' + k[4:] + '.png')
    time.sleep(0.3)
end = time.time() + 10
while time.time() < end and not M['power']: time.sleep(0.2)
ok = Checks()
ok('0. the desktop is up in WebKit, signed in', RES.get('up') is True, RES.get('error') or RES)
ok('1. Display Properties has a "Lock and sign-in screen" section', RES.get('section') is True, RES)
ok('1b. ...with the background settings: after the colours, before the fonts, its rows in the same grid', RES.get('grid') is True, RES)
k = RES.get('kept') or {}
ok('2. its choices are kept for the computer: colour, no darkening, small clock, bottom left, no account name', k.get('bg') == 'color' and k.get('color') == '#aa0000' and k.get('dim') == 0 and k.get('clock') == 'small' and k.get('pos') == 'bottom' and k.get('user') is False, k)
lk = RES.get('lock') or {}
ok('3. the lock screen: the colour behind it', 'rgb(170, 0, 0)' in (lk.get('bg') or []), lk)
ok('...the small clock, no picture or name, at the bottom left', lk.get('ck') == '36px' and lk.get('av') == 'none' and lk.get('jc') == 'flex-end' and lk.get('ai') == 'flex-start', lk)
c = px(shots.get('shotlock', ''), 900, 200)
ok('...and on the screen: the lock screen in that colour', bool(c) and c[0] > 140 and c[1] < 40 and c[2] < 40, c)
pk = RES.get('pic') or {}
ok('4. a picture of your own, blurred; no clock; the name back', 'data:image/jpeg' in (pk.get('img') or '') and 'blur(6px)' in (pk.get('blur') or '') and pk.get('ck') == 'none' and pk.get('av') != 'none', pk)
c = px(shots.get('shotpic', ''), 900, 200)
ok('...and on the screen: the picture (green) behind the lock screen', bool(c) and c[1] > 100 and c[1] > c[0] + 40 and c[1] > c[2] + 40, c)
ok("5. the desktop's background behind the lock screen", 'rgb(1, 2, 3)' in str(RES.get('desk', '')), RES.get('desk'))
lg = RES.get('login') or {}
ok('6. the sign-in page: the remembered desktop background, and a clock', 'rgb(1, 2, 3)' in (lg.get('bg') or '') and any(ch.isdigit() for ch in (lg.get('ck') or '')), lg)
sc = RES.get('shut') or {}
ok('7. Shut down: a full-screen "Shutting down…" at once, over everything', sc.get('t') == 'Shutting down…' and (sc.get('z') or 0) > 1000007 and sc.get('img'), sc)
c = px(shots.get('shotshut', ''), 20, 780)
ok('...and on the screen: its dark background fills the screen', bool(c) and max(c) < 40, c)
ln = RES.get('len') or 2.4; t0 = RES.get('t0') or 0
ok('8. ...the power is asked for only after the tone has played', bool(M['power']) and M['power']['at'] - t0 >= min(3000, ln * 1000) and 'action=poweroff' in M['power']['q'], [M['power'], t0, ln])
ok('9. no page errors', not P.errors and 'error' not in RES, P.errors or RES.get('error'))
P.stop(); sys.exit(ok.end())
