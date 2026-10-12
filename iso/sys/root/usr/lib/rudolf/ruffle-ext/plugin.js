/* Report a Flash plugin to pages that check for one (SWFObject and similar) so they add their Flash
   content; detect.js then loads Ruffle, which plays it. Runs in the page's world at document start. */
(() => {
	try {
		if (window.top === window || (navigator.plugins && navigator.plugins.namedItem && navigator.plugins.namedItem('Shockwave Flash'))) return;
		const mime = {type: 'application/x-shockwave-flash', suffixes: 'swf', description: 'Shockwave Flash'};
		const spl = {type: 'application/futuresplash', suffixes: 'spl', description: 'FutureSplash Player'};
		const plug = {name: 'Shockwave Flash', description: 'Shockwave Flash 32.0 r0', filename: 'ruffle.js', version: '32.0.0.0', length: 2, 0: mime, 1: spl,
			item: i => i == 0 ? mime : i == 1 ? spl : null, namedItem: n => n === mime.type ? mime : n === spl.type ? spl : null};
		mime.enabledPlugin = spl.enabledPlugin = plug;
		const wrap = (orig, extra) => new Proxy(orig, {get(t, k) {
			const n = t.length;
			if (k === 'length') return n + extra.length;
			if (typeof k === 'string' && /^\d+$/.test(k) && +k >= n) return extra[+k - n];
			if (typeof k === 'string') { const hit = extra.find(x => x.name === k || x.type === k); if (hit) return hit; }
			if (k === 'item') return i => i >= n ? extra[i - n] || null : t.item(i);
			if (k === 'namedItem') return s => extra.find(x => x.name === s || x.type === s) || t.namedItem(s);
			if (k === Symbol.iterator) return function* () { for (let i = 0; i < n; i++) yield t[i]; yield* extra; };
			const v = Reflect.get(t, k, t); return typeof v === 'function' ? v.bind(t) : v;
		}});
		const P = wrap(navigator.plugins, [plug]), M = wrap(navigator.mimeTypes, [mime, spl]);
		Object.defineProperty(navigator, 'plugins', {get: () => P, configurable: true});
		Object.defineProperty(navigator, 'mimeTypes', {get: () => M, configurable: true});
	} catch (e) {}
})();
