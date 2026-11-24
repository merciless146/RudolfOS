/* ===== Rudolf OS 1.6.3: Oracle's safety and your data (about:safety) =====
   The owner, 2026-11-24: "Add a page outlining Oracle's user safety and data security policies in the default bookmarks.
   The notification for no extensions on desktop-based Chromium will have a "Learn more" link that the user can click
   which paths to this section. The page will scroll to the entry explaining why it is unsafe and highlight it for the
   user to read."
   about:safety is one of Oracle's own pages (as about:home), in the default bookmarks as "Safety" (added once to
   existing bookmarks). about:safety#ENTRY scrolls to that entry and highlights it: the extensions reminder (plan 1.6.3
   Part 7) links to about:safety#extensions-desktop. Every entry says only what Rudolf OS does today, or names the plan
   when it is a rule for something still coming. */
const ORA_SAFE=[
 ['nothing-sent','Nothing about you is sent anywhere',
  `<p>Oracle and Rudolf OS collect no usage data, crash reports or browsing history about you, and send none to anyone. What you browse stays on this computer.</p>`],
 ['blocking','Ads and trackers are blocked',
  `<p>Oracle blocks ads and trackers on every page by itself. The shield 🛡 in the address box says how many were blocked on the page, and its menu turns blocking off (or on) for a site; Settings › Privacy › Ads and trackers in Oracle lists them.</p>`],
 ['your-sites','Your websites are yours, account by account',
  `<p>Each Rudolf OS account has its own sign-ins, cookies, site data and history; no other account sees them. On a computer with a security chip (TPM 2.0) they are encrypted with a key kept in that chip, so a drive taken out of the computer cannot be read.</p>
   <p>Turn on <b>Settings › Oracle › Keep my websites encrypted with my account</b> and they are encrypted with <i>your account's</i> key instead, and locked when you sign out: then not even another administrator of this computer can read them. Oracle then opens in a window of its own.</p>`],
 ['private','Private windows keep nothing',
  `<p>A private tab or window (Ctrl+Shift+N) keeps no history; its cookies and site data are kept in memory only and forgotten when the last private window closes. Downloads and bookmarks you make there are kept.</p>`],
 ['tor','Tor windows hide where you are',
  `<p>Browse with Tor sends a window's tabs through the Tor network, so sites do not see your internet address; with a VPN on, Tor runs inside it. A Tor window is always private and never runs extensions.</p>
   <p>It hides your address, not who you are: sites can still recognise a browser by how it looks and behaves. For real anonymity use Tor Browser on a computer meant for it.</p>`],
 ['updates','Kept up to date, and kept apart',
  `<p>Oracle is built on Chromium. Rudolf OS checks for Chromium's security fixes every six hours and ships them as signed updates. Each site runs in its own sandboxed process, kept apart from other sites and from the rest of the computer.</p>`],
 ['extensions-desktop','Why extensions never run in Oracle as part of the desktop',
  `<p>On the Chromium desktop, Rudolf OS's desktop is itself a page in Chromium, and Oracle's tabs can be part of that page. The desktop's page holds the key that lets it ask Rudolf OS to do things for you: open and save your files, change settings, install apps and updates.</p>
   <p>An extension can read and change the pages it runs on. In the desktop's own Chromium it could reach that key, and with it your files and the rest of the computer, without asking. So while Oracle runs as part of the desktop, <b>no extension runs at all</b>: Rudolf OS's own rules for Chromium refuse every extension there, whatever any other setting says, and trying to add one only shows a reminder.</p>
   <p>Extensions run only when Oracle runs in a window of its own, a separate Chromium with your websites and nothing of the desktop: turn on <b>Settings › Oracle › Extensions</b>, or <b>Keep my websites encrypted with my account</b>. Even there, extensions never reach this computer's own services or the desktop's pages, never run in Tor windows, and run in private windows only if you allow them. (Extensions are planned for Rudolf OS 1.6.3; this rule holds from the start.)</p>`],
 ['guest','Guest leaves nothing behind',
  `<p>Signed in as Guest, everything Oracle keeps is held in memory only and gone when Guest signs out.</p>`],
 ['limits','What Oracle cannot do for you',
  `<p>Oracle cannot tell a real sign-in page from a convincing fake: check the address box before typing a password. A file you download and open runs as you and can read your own files, as on any computer; open only what you trust, and keep a backup of what matters.</p>`]];
function oraSafetyPage(u,doc,lk,esc){
 const m=String(u).match(/^about:safety(?:#([a-z-]+))?$/);if(!m)return null;
 const want=m[1]&&ORA_SAFE.some(e=>e[0]==m[1])?m[1]:'';
 const toc=ORA_SAFE.map(e=>`<li><a href='#${e[0]}' onclick='show("${e[0]}");return false'>${e[1]}</a></li>`).join('');
 const body=`<style>.os-e{border:1px solid #d1d5db;border-radius:6px;padding:4px 14px;margin:12px 0;transition:background .4s,box-shadow .4s}
  .os-e h3{margin:10px 0 4px;font-size:15px}.os-e p{line-height:1.5}
  .os-e.hl{background:#fff7cc;border-color:#d4a400;box-shadow:0 0 0 3px #f5d35b}
  @media (prefers-color-scheme:dark){.os-e.hl{background:#3a3000;border-color:#d4a400}}</style>
  <p>How Oracle keeps you safe and what happens to your data.</p>
  <ul id='os-toc'>${toc}</ul>`+ORA_SAFE.map(e=>`<div class='os-e' id='${e[0]}'><h3>${e[1]}</h3>${e[2]}</div>`).join('');
 const x=`function show(id){document.querySelectorAll('.os-e.hl').forEach(e=>e.classList.remove('hl'));const e=document.getElementById(id);if(!e)return;e.classList.add('hl');e.scrollIntoView({block:'center'})}${want?`addEventListener('load',()=>setTimeout(()=>show('${want}'),60))`:''}`;
 return doc("Oracle's safety and your data",body,x)}
