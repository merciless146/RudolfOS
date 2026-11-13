/* Rudolf OS 1.5.8 (plan 7.5): the browser build as a web app on a phone or tablet. The desktop page and the files it
   needs are kept, so it opens without a connection; a newer copy replaces them when online. Not used on an installed
   Rudolf OS (the desktop page registers this only when no rudolfd answers). */
const C = 'rudolf-web-v1', CORE = ['./', 'index.html', 'favicon.png', 'rudolf.webmanifest'];
self.addEventListener('install', e => { e.waitUntil(caches.open(C).then(c => c.addAll(CORE)).then(() => self.skipWaiting())) });
self.addEventListener('activate', e => { e.waitUntil(caches.keys().then(ks => Promise.all(ks.filter(k => k != C).map(k => caches.delete(k)))).then(() => self.clients.claim())) });
/* the page and its files: the network first (the newest copy), the kept copy when offline; files from lib/ kept as
   they are fetched (Lua, the PDF viewer, Luau), the kept copy first as they do not change within a version */
self.addEventListener('fetch', e => {
  const u = new URL(e.request.url); if (e.request.method != 'GET' || u.origin != location.origin || u.pathname.includes('/api/')) return;
  const keep = r => { if (r && r.ok && r.type == 'basic') { const c = r.clone(); caches.open(C).then(k => k.put(e.request, c)) } return r };
  /* (1.5.9: and a/, the dynamic backgrounds' pictures and the fonts, named by their contents: they never change) */
  if (u.pathname.includes('/lib/') || u.pathname.includes('/a/')) { e.respondWith(caches.match(e.request).then(h => h || fetch(e.request).then(keep))); return }
  e.respondWith(fetch(e.request).then(keep).catch(() => caches.match(e.request).then(h => h || caches.match('index.html'))));
});
