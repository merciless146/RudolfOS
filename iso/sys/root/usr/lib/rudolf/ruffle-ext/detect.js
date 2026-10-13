/* Inside Oracle's tabs and in the browser window for sign-in sites: when a page has Flash content, ask for
   Ruffle to be loaded into it. (In the desktop's own Chromium the only top-level page is the desktop itself.) */
(() => {
	const inOracle = window.top !== window;
	const SEL = 'embed[src*=".swf" i],embed[type*="shockwave" i],embed[type*="futuresplash" i],object[data*=".swf" i],object[type*="shockwave" i],object[classid*="d27cdb6e" i],param[name="movie" i][value*=".swf" i]';
	let done = false, mo = null;
	chrome.storage.local.get('ruffle').then(({ruffle}) => {
		if (ruffle && ruffle.on === false) return;
		const go = () => {
			if (done) return; done = true; if (mo) mo.disconnect();
			chrome.runtime.sendMessage({t: 'inject'});
			if (inOracle) try { window.top.postMessage({__rudolfExt: 'ruffle-active', href: location.href}, 'http://127.0.0.1:8080'); } catch (e) {}
		};
		if (document.querySelector(SEL)) return go();
		mo = new MutationObserver(() => { if (document.querySelector(SEL)) go(); });
		mo.observe(document.documentElement, {childList: true, subtree: true, attributes: true, attributeFilter: ['src', 'data', 'type', 'value']});
		setTimeout(() => mo && mo.disconnect(), 60000);
	});
})();
