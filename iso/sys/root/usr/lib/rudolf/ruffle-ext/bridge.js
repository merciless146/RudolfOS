/* Runs only on the Rudolf desktop page. The desktop posts its extension settings here; this script stores
   them for the service worker and tells the desktop the extension is installed. Messages from web pages
   inside Oracle come from other windows and are ignored. */
const hello = () => chrome.storage.local.get('ruffle').then(({ruffle}) => window.postMessage({__rudolfExt: 'hello', id: chrome.runtime.id, name: chrome.runtime.getManifest().name, version: chrome.runtime.getManifest().version, ruffle: ruffle || null}, location.origin));
window.addEventListener('message', e => {
	if (e.source !== window || !e.data || typeof e.data.__rudolfExt !== 'string') return;
	const d = e.data;
	if (d.__rudolfExt === 'ping') hello();
	else if (d.__rudolfExt === 'cfg' && d.ruffle && typeof d.ruffle === 'object') {
		const r = {on: !!d.ruffle.on, urls: (Array.isArray(d.ruffle.urls) ? d.ruffle.urls : []).filter(u => typeof u === 'string' && /^(https?:)\/\//.test(u)).slice(0, 4), config: Object.assign({}, d.ruffle.config || {})};
		chrome.storage.local.set({ruffle: r}).then(hello);
	}
});
hello();
