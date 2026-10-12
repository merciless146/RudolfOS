/* ===== V25 misc: calculator Standard/Scientific modes, OS name beside Start, Command Prompt rename ===== */
const CALC_STD=['C','⌫','%','÷','7','8','9','×','4','5','6','−','1','2','3','+','±','0','.','='];
function calcModes(W,cg){if(!W||!cg)return;cg.querySelectorAll('button').forEach(b=>{b.dataset.k=b.textContent});
 const bar=document.createElement('div');bar.className='cmode';bar.setAttribute('role','tablist');bar.innerHTML=`<button data-m='std' role='tab'>Standard</button><button data-m='sci' role='tab'>Scientific</button>`;
 W.body.insertBefore(bar,W.body.firstChild);
 const set=(m,resize)=>{const std=m=='std';W.body.classList.toggle('calc-std',std);bar.querySelectorAll('button').forEach(b=>{const on=b.dataset.m==m;b.classList.toggle('on',on);b.setAttribute('aria-selected',on)});
  cg.querySelectorAll('button').forEach(b=>{const i=CALC_STD.indexOf(b.dataset.k);b.style.order=std&&i>=0?i:'';b.hidden=std&&i<0});
  try{svj('bd95calcmode',m)}catch(e){}
  if(resize&&!W.max){W.el.style.width=(std?290:340)+'px';W.el.style.height=(std?440:480)+'px'}};
 bar.querySelectorAll('button').forEach(b=>{b.onmousedown=e=>e.preventDefault();b.onclick=()=>{set(b.dataset.m,1);W.el.focus({preventScroll:true})}});
 set(ld('bd95calcmode','sci')=='std'?'std':'sci',1)}
/* OS name and version beside the Start button */
function osverDraw(){const st=document.getElementById('start');if(!st)return;let v=document.getElementById('osver');
 if(!v){v=document.createElement('button');v.id='osver';v.title='About Rudolf OS';v.onclick=()=>{try{settings('about')}catch(e){}};st.after(v)}v.textContent=`Rudolf OS ${RVER}`}
osverDraw();
/* the prompt used to be called “Rudolf Prompt”: keep its desktop position and pins */
(function(){try{Object.keys(localStorage).filter(k=>/^bd95cells/.test(k)).forEach(k=>{const c=JSON.parse(localStorage.getItem(k)||'null');const old='Rudolf '+'Prompt';if(c&&c[old]&&!c['Command Prompt']){c['Command Prompt']=c[old];delete c[old];localStorage.setItem(k,JSON.stringify(c))}})}catch(e){}})();
