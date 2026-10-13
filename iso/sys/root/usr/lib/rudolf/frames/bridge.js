/* Rudolf OS web frames, on the desktop page only: the desktop sends the sites that sign in in the real browser
   window; the service worker stops removing those sites' frame protections. Messages from web pages inside Oracle
   come from other windows and are ignored. */
window.addEventListener('message', e => {
	if (e.source !== window || !e.data || e.data.__rudolfFrames !== 'sites' || !Array.isArray(e.data.sites)) return;
	chrome.runtime.sendMessage({t: 'sites', sites: e.data.sites.filter(s => typeof s === 'string' && /^[a-z0-9.-]{3,253}$/.test(s)).slice(0, 500)}).catch(() => {});
});
