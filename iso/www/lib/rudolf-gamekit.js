/* rudolf-gamekit 1 (Rudolf OS 1.5.9, docs/plan-1.5.9.md 8.2): what a Rudolf OS Library web app may ask of the desktop.
   Load it in the app's page:  <script src="http://127.0.0.1:8080/lib/rudolf-gamekit.js"></script>
   Then (each returns a promise; it fails with the reason when the app's PKGINFO did not ask for it or the person did
   not allow it):
     await RudolfGamekit.hello()              {name, title, granted: ['saves', ...], signedIn}
     await RudolfGamekit.save('slot1', text)  keep a save (text, 1 MB at most; JSON.stringify an object)
     await RudolfGamekit.load('slot1')        the save, or null
     await RudolfGamekit.saves()              the save names
     await RudolfGamekit.remove('slot1')
     await RudolfGamekit.notify('Level 2 unlocked')
   Full screen and game controllers are the browser's own (element.requestFullscreen(), navigator.getGamepads()),
   allowed when the PKGINFO asks for "fullscreen" and "gamepad". Outside Rudolf OS every call fails. MIT licence. */
(function () {
	var n = 0, wait = {};
	window.addEventListener('message', function (e) {
		var m = e.data; if (e.source !== window.parent || !m || m.rudolf !== 'v1' || !wait[m.id]) return;
		var w = wait[m.id]; delete wait[m.id]; m.ok ? w[0](m.result) : w[1](new Error(m.error));
	});
	function call(c, a) {
		return new Promise(function (ok, no) {
			if (window.parent === window) return no(new Error('not inside Rudolf OS'));
			var id = 'g' + (++n); wait[id] = [ok, no];
			setTimeout(function () { if (wait[id]) { delete wait[id]; no(new Error('Rudolf OS did not answer')); } }, 10000);
			window.parent.postMessage({ rudolf: 'v1', id: id, call: c, args: a || {} }, '*');
		});
	}
	window.RudolfGamekit = {
		hello: function () { return call('hello'); },
		save: function (k, v) { return call('save.set', { key: k, value: v }); },
		load: function (k) { return call('save.get', { key: k }); },
		saves: function () { return call('save.list'); },
		remove: function (k) { return call('save.delete', { key: k }); },
		notify: function (t) { return call('notify', { text: t }); }
	};
})();
