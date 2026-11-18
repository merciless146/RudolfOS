/* 1.6.0 (docs/plan-1.6.0.md Part 19 item 2): where this build of Rudolf OS comes from (rudolfd's origin, from
   /etc/rudolf/origin: iso/sys/origin.h). The official build names the official repository; a fork's build names the fork,
   so its release downloads, Make a stick's boot files, system reports and Library are the fork's. Until rudolfd answers
   (and without it), the official ones. */
const ORIGIN={repo:'merciless146/RudolfOS',repo_id:'1414456577',pages:'https://merciless146.github.io/RudolfOS',library:'',official:true};
if(RD)fetch('/api/origin',{headers:{'X-Rudolf-Token':RD.token}}).then(r=>r.ok?r.json():null).then(j=>{if(!j||!j.ok)return;
 if(/^[A-Za-z0-9._-]+\/[A-Za-z0-9._-]+$/.test(j.repo||''))ORIGIN.repo=j.repo;if(/^\d+$/.test(j.repo_id||''))ORIGIN.repo_id=j.repo_id;
 if(/^https:\/\//.test(j.pages||''))ORIGIN.pages=j.pages;ORIGIN.official=j.official!==false;
 if(/^https:\/\//.test(j.library||'')){ORIGIN.library=j.library;if(typeof LIB_URL!='undefined')LIB_URL=j.library}}).catch(()=>{});
