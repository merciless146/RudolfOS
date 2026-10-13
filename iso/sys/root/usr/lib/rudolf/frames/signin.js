/* Rudolf OS web frames: sign-in pages in Oracle's tabs. GPL-2.0-or-later.
   Websites in Oracle's tabs are frames inside the desktop page, so their sign-in cookies count as third-party
   and are dropped: the sign-in seems to work, then starts again. This script tells the desktop when a page in an
   Oracle tab asks for a password, sends a sign-in form or opens a "Sign in with..." window, so Oracle can move the
   site to a real browser window, where sign-in works. It also reports the page's address as you click around.
   Only these facts and the page's address are sent, only to the desktop page; nothing typed is ever read. */
(() => {
	const DESK = 'http://127.0.0.1:8080';
	if (window.top === window) return;
	const a = location.ancestorOrigins;
	if (!a || !a.length || a[a.length - 1] !== DESK) return;          // not inside the Rudolf desktop
	const direct = a.length === 1;                                     // the tab's own page (not a frame inside it)
	const tell = (t, extra) => { try { window.top.postMessage(Object.assign({__rudolfSignin: t, href: location.href, direct}, extra || {}), DESK); } catch (e) {} };
	const hasPw = root => { for (const i of (root || document).querySelectorAll('input[type=password]')) if (!i.disabled && (i.offsetParent !== null || i.getClientRects().length)) return true; return false; };
	let saidPw = false, held = false;
	const look = () => { if (!saidPw && document.documentElement && hasPw(document)) { saidPw = true; tell('pw'); } };
	/* a form with a password field is about to be sent: hold it back (the sign-in would be lost in the frame anyway)
	   and move the site; the desktop answers "go on" when the person chose to stay in Oracle */
	addEventListener('submit', e => {
		const f = e.target; if (!f || !f.querySelector || !hasPw(f) || held === 'allow') return;
		e.preventDefault(); e.stopImmediatePropagation(); held = true; tell('submit');
	}, true);
	addEventListener('keydown', e => {
		if (e.key !== 'Enter' || held === 'allow' || !e.target || e.target.type !== 'password' || e.target.form) return;
		e.preventDefault(); e.stopImmediatePropagation(); held = true; tell('submit');            // sign-ins without a <form>
	}, true);
	document.addEventListener('rudolf-signin-popup', e => tell('popup', {to: String(e.detail || '').slice(0, 2000)}));
	addEventListener('message', e => { if (e.origin === DESK && e.data && e.data.__rudolfSignin === 'stay') held = 'allow'; });
	let last = '';
	const where = () => { if (direct && location.href !== last) { last = location.href; tell('url', {title: document.title || ''}); } };
	const start = () => {
		look(); where();
		new MutationObserver(() => { look(); }).observe(document.documentElement, {childList: true, subtree: true, attributes: true, attributeFilter: ['type']});
		setInterval(where, 1000);     // single-page sites change the address without loading a page
	};
	if (document.readyState === 'loading') addEventListener('DOMContentLoaded', start, {once: true}); else start();
})();
