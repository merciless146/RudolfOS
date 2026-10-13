/* Rudolf OS web frames. GPL-2.0-or-later.
   rules.json removes the headers that stop websites from being shown in Oracle's tabs. Sites that sign in in the
   real browser window keep those protections: the built-in ones in rules.json, the ones added in Settings here. */
chrome.runtime.onMessage.addListener(m => {
	if (!m || m.t !== 'sites' || !Array.isArray(m.sites)) return;
	chrome.declarativeNetRequest.getDynamicRules().then(old => chrome.declarativeNetRequest.updateDynamicRules({
		removeRuleIds: old.map(r => r.id),
		addRules: m.sites.length ? [{id: 1, priority: 3, action: {type: 'allow'}, condition: {requestDomains: m.sites, resourceTypes: ['sub_frame']}}] : []
	})).catch(() => {});
});
