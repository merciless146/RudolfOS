/* ===== Rudolf OS 1.6.1: one ordered list of additions for each Settings page and for applySet/themeApply =====
   Many modules add a section to a Settings page (Display Properties has 15) or a step to applying the settings. Each
   used to wrap the function before it (const o=dispPanelS;dispPanelS=function(c,W){o(c,W);...}), so opening Display
   Properties went through 15 nested functions, and one section that failed stopped every section after it.
   NAME=__after(NAME,function(...){...},'module') adds a step that runs after NAME, with the same arguments: the steps
   of one function are kept in one list, in the order the modules load, which is the order the wrappers ran in. A step
   that fails is reported in the console and the next one still runs. A module that still wraps NAME itself (one that
   runs before the earlier one, or uses its result) is kept as it was: the steps added after it go in a new list around
   it, so everything runs in exactly the order it did. NAME.__hooks lists the steps (function, module). */
function __after(cur,fn,mod){
 if(typeof cur=='function'&&cur.__hooks){cur.__hooks.push([fn,mod]);return cur}
 const hooks=[[fn,mod]];
 const d=function(){const r=cur.apply(this,arguments);
  for(const [h,m] of hooks){try{h.apply(this,arguments)}catch(e){console.warn('['+m+'] '+(e&&e.message||e),e)}}
  return r};
 d.__hooks=hooks;d.__inner=cur;return d}
