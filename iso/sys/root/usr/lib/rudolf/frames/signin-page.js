/* Rudolf OS web frames, in the page's own world: "Sign in with Google / Microsoft / Apple / Facebook..."
   buttons open a second window that only works when the site itself is in a real browser window. Inside an
   Oracle tab such a window is not opened; the desktop is told instead and moves the site. GPL-2.0-or-later. */
(() => {
	if (window.top === window) return;
	const a = location.ancestorOrigins;
	if (!a || !a.length || a[a.length - 1] !== 'http://127.0.0.1:8080') return;
	const IDP = /^https:\/\/(accounts\.google\.com|login\.microsoftonline\.com|login\.live\.com|appleid\.apple\.com|www\.facebook\.com\/(v[\d.]+\/)?dialog\/oauth|m\.facebook\.com\/(v[\d.]+\/)?dialog\/oauth|github\.com\/login|discord\.com\/(api\/)?oauth2|api\.twitter\.com\/oauth|twitter\.com\/i\/oauth2|x\.com\/i\/oauth2|www\.amazon\.com\/ap\/oa|login\.yahoo\.com|auth\.atlassian\.com|id\.twitch\.tv|www\.linkedin\.com\/oauth|slack\.com\/oauth)/i;
	const abs = u => { try { return new URL(String(u), location.href).href; } catch (e) { return ''; } };
	const say = u => document.dispatchEvent(new CustomEvent('rudolf-signin-popup', {detail: u}));
	const open0 = window.open;
	window.open = function (u, t, f) { const h = abs(u); if (h && IDP.test(h)) { say(h); return null; } return open0.apply(this, arguments); };
	/* links to a sign-in service that open a new window */
	document.addEventListener('click', e => {
		const l = e.target && e.target.closest && e.target.closest('a[href]');
		if (!l || l.target !== '_blank') return;
		const h = abs(l.getAttribute('href')); if (h && IDP.test(h)) { e.preventDefault(); say(h); }
	}, true);
})();
