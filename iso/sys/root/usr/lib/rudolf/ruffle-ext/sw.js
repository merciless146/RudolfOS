/* Ruffle for Oracle (Rudolf OS). GPL-2.0-or-later.
   The desktop sends its settings through bridge.js: on/off, where to load Ruffle from (the same sources as
   the Flash Player app, so one copy of Ruffle serves both) and the Ruffle options. */
const DEF = {on: true, urls: ['https://unpkg.com/@ruffle-rs/ruffle', 'https://cdn.jsdelivr.net/npm/@ruffle-rs/ruffle'], config: {autoplay: 'auto'}};
async function cfg() { const {ruffle} = await chrome.storage.local.get('ruffle'); return Object.assign({}, DEF, ruffle || {}); }
/* Old pages check navigator.plugins for Flash before they add it; this entry makes them add it, then Ruffle plays it */
async function sync() {
	const c = await cfg();
	const reg = await chrome.scripting.getRegisteredContentScripts({ids: ['flash-plugin']});
	if (c.on && !reg.length) await chrome.scripting.registerContentScripts([{id: 'flash-plugin', matches: ['<all_urls>'], excludeMatches: ['http://127.0.0.1/*'], js: ['plugin.js'], runAt: 'document_start', allFrames: true, world: 'MAIN', persistAcrossSessions: true}]);
	else if (!c.on && reg.length) await chrome.scripting.unregisterContentScripts({ids: ['flash-plugin']});
}
chrome.runtime.onInstalled.addListener(() => sync().catch(() => {}));
chrome.runtime.onStartup.addListener(() => sync().catch(() => {}));
chrome.storage.onChanged.addListener(ch => { if (ch.ruffle) sync().catch(() => {}); });
/* runs in the page's own world, so Ruffle can replace the page's <embed> and <object> elements */
function loadRuffle(urls, config) {
	if (window.__rudolfRuffle) return; window.__rudolfRuffle = 1;
	window.RufflePlayer = window.RufflePlayer || {};
	window.RufflePlayer.config = Object.assign({unmuteOverlay: 'visible', warnOnUnsupportedContent: false}, config || {}, window.RufflePlayer.config || {});
	let i = 0;
	const next = () => { if (i >= urls.length) return; const s = document.createElement('script'); s.src = urls[i++]; s.onerror = () => { s.remove(); next(); }; (document.head || document.documentElement).appendChild(s); };
	next();
}
chrome.runtime.onMessage.addListener((m, s) => {
	if (!m || m.t !== 'inject' || !s.tab) return;
	cfg().then(c => { if (c.on && c.urls.length) return chrome.scripting.executeScript({target: {tabId: s.tab.id, frameIds: [s.frameId]}, world: 'MAIN', func: loadRuffle, args: [c.urls, c.config]}); }).catch(() => {});
});
