/* 1.6.0 (docs/plan-1.6.0.md Part 12, item 12): svc160_t.sh's script, given to every page before its own (native_rd.py DIR/inject.js).
   It stands in for what a person does in the pages and says what they see, through the stand-in rudolfd (each request is in DIR/calls):
   - test/plan (DIR/plan.json): {id, seed:{KEY:VALUE}} put into the page's storage once per id, by the desktop's service page (as the
     Clock's or the Calendar's window saves alarms and events); {clock:{secs,label}}: in the Clock's window, a timer started on its
     Timers tab, then the window closed with its ✕ (test/timerset first); {close:ID}: the service page closes itself once per ID
   - test/list?k=APP: the page's held and running timers (svc60.js __svcList), 4 s after it loads
   - test/evgot?k=APP: rudolfd's event "svctest" heard by this page */
(function () {
	var TOK = 'tok59native', ST = window.setTimeout, SI = window.setInterval;   /* (the browser's own: svc60.js wraps them after this) */
	function api(p, body) { try { var x = new XMLHttpRequest(); x.open(body != null ? 'POST' : 'GET', '/api/' + p, false); x.setRequestHeader('X-Rudolf-Token', TOK); x.send(body != null ? body : null); return x.responseText } catch (e) { return '' } }
	var k = ''; try { k = new URLSearchParams(String(location.hash).slice(1)).get('app') || '' } catch (e) {}
	var plan = function () { try { return JSON.parse(api('test/plan') || '{}') } catch (e) { return {} } };
	var closed = {};
	if (k == 'do-svc' || k == 'do-bg') SI.call(window, function () { var P = plan();
		if (P.seed && P.id && localStorage.getItem('svct-seed') != P.id) { for (var n in P.seed) localStorage.setItem(n, P.seed[n]); localStorage.setItem('svct-seed', P.id) }
		if (P.close && !closed[P.close] && localStorage.getItem('svct-close') != P.close) { closed[P.close] = 1; localStorage.setItem('svct-close', P.close); try { oaPost({op: 'close'}) } catch (e) {} }
	}, 1000);
	addEventListener('load', function () {
		ST.call(window, function () { try { api('test/list?k=' + encodeURIComponent(k), JSON.stringify(__svcList())) } catch (e) {} }, 4000);
		var hook = function () { if (typeof evOn != 'function' || typeof USER == 'undefined' || !USER) return ST.call(window, hook, 500); evOn(function (t) { if (/^svctest/m.test(String(t))) api('test/evgot?k=' + encodeURIComponent(k)) }) }; hook();
		if (k == 'clock') { var P = plan(); if (!P.clock) return; var n = 0;
			var go = function () { var tab = document.querySelector('.tmtabs [data-t=timer]'); if (!tab) { if (++n < 120) ST.call(window, go, 250); return }
				tab.click(); ST.call(window, function () { var q = function (s) { return document.querySelector('.clb ' + s) };
					if (!q('.ta')) return; q('.tm').value = 0; q('.ts').value = P.clock.secs; q('.tl').value = P.clock.label; q('.ta').click();
					ST.call(window, function () { api('test/timerset?n=' + TIMERS.length); window.__oneAppClose() }, 1500) }, 400) };
			go() }
	});
})();
