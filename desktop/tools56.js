/* ---------- 1.5.6: the Developer Console is a tool for building Rudolf OS ----------
   Out of the Start menu unless Settings › Apps › "Show developer tools in the Start menu" is on; always in Task
   Manager (a button) and in search. */
function devToolsShown(){return !!ld('bd95devtools',false)}
{const _ap=appsPanel;appsPanel=function(c,W){_ap(c,W);try{const d=document.createElement('div');d.className='devtools56';
  d.innerHTML=`<h4 class='sh4'>For developers</h4><label><input type='checkbox' class='dvt'> Show developer tools in the Start menu (Developer Console)</label><p class='msg'>The Developer Console shows the desktop's log, its windows and storage, and the app API. It is always in Task Manager and in search.</p>`;
  const cb=d.querySelector('.dvt');cb.checked=devToolsShown();cb.onchange=e=>{e.stopPropagation();svj('bd95devtools',cb.checked);try{startMenuApps()}catch(x){}};
  const foot=c.querySelector(':scope > .sfoot');if(foot)foot.before(d);else c.appendChild(d)}catch(e){console.warn(e)}}}
{const _tm=window.taskmgr;window.taskmgr=function(){const W=_tm.apply(this,arguments);try{const st=W&&(W.body.querySelector('.tmfoot > span:last-child')||W.body.querySelector('.stat'));if(st&&!W.body.querySelector('.tmdev')){const b=document.createElement('button');b.className='tmdev';b.textContent='🛠️ Developer Console';b.onclick=()=>launch('devcon');st.appendChild(b)}}catch(e){}return W};if(APPS.taskmgr)APPS.taskmgr.open=window.taskmgr}
