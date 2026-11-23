/* ===== Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 38, step 2): Task Manager's small page =====
   build19.py makes a small page for Task Manager's window on the native desktop (lean62.py; a/ID.html, named by its contents) and names it
   in the whole page (window.__LEAN_TM, in its head). The shell's hidden page (#app=do-bg) tells the native shell its name as it starts, so
   the shell opens Task Manager there (/?oa=ID#app=taskmgr..., which rudolfd serves as it serves the apps' page). Nothing else: Standard,
   Lite and a page opened by hand have no native shell to tell. ===== */
if(OA_BG&&typeof window.__LEAN_TM=='string'&&/^[0-9a-f]{16}$/.test(window.__LEAN_TM)&&oaNative())oaPost({op:'tmpage',id:window.__LEAN_TM});
