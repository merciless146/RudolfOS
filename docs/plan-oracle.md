# Oracle: find, closed tabs, passwords, blocking and reader mode (asked for on 2026-11-16)

The owner compared Oracle with Chrome, Edge, Firefox, Safari and Brave on 2026-11-16 and asked for a plan for five of
the improvements suggested then. "Create a plan for recommended items 1, 2, 4, 5, and 7":

- Item 1: find in page (Ctrl+F).
- Item 2: reopen a closed tab (Ctrl+Shift+T) and restore the last session.
- Item 4: a password manager for websites, kept in the account's keyring.
- Item 5: ad and tracker blocking.
- Item 7: reader mode.

The parts below keep those numbers. The other items are not part of this plan: item 3 (no 12-tab limit), item 6
(extensions: the recommended source is at the end), item 8 (pinned and muted tabs, tab search, the padlock, site
permissions) and item 9 (export and import instead of sync).

## Where Oracle's pages live (what each part must cover)

Oracle shows a page in one of four ways. Every part below works in all four, on Standard and on Native, as the owner's
rule says: "Rudolf OS on the native desktop should appear and function exactly as it does in Chromium mode."

| Where | What it is | How Oracle reaches it |
|---|---|---|
| Real tabs (Standard and Native) | a `controlledframe` in the IWA shell (iso/shell/shell.js), Rudolf's Chromium | the shell's messages (tabs26.js ⇄ shell.js): `executeScript`, `addContentScripts`, `request.createWebRequestInterceptor` (already used by the firewall), and `find`/`stopFinding` |
| Lite's tabs | WebKitGTK pages made by rudolf-webkit (oracle58.js) | the same messages; rudolf-webkit uses `WebKitFindController`, `WebKitUserContentManager` scripts and `WebKitUserContentFilterStore` |
| The frames fallback | an iframe in the desktop page, when the shell is not running | only what an iframe allows: the page's own content cannot be reached, so these parts say so plainly or are left out |
| Oracle's own pages | about: pages, files on C:, .HTM files, all `srcdoc` (base_v24.html `page()`) | a small script added to the page, as ORACLE_TEL_JS is today |

Private and Tor windows (private59.js, tor59.js) keep nothing: no session, no closed-tab list after the window
closes, no passwords saved (saved ones can still be filled, after a click), and no reader-mode history.

## Item 1. Find in page

**What the person sees.** Ctrl+F, F3, or ☰ › Find in page… opens a bar under the bookmarks bar. It has a text box,
"3 of 17", up and down buttons, a "Match case" box and ✕. Enter goes to the next match, Shift+Enter to the previous
one, Escape closes the bar, and the matches are highlighted as Chromium highlights them. Text selected on the page
fills the box. The bar belongs to its tab: switching tabs hides it, and switching back shows it with the same text.

**How.**
- Real tabs: `controlledframe.find(text, {backward, matchCase})` and its `findupdate` event give the count and the
  current match; `stopFinding('clear')` when the bar closes. If Chromium 158's controlledframe lacks `find`, fall back
  to a script run with `executeScript` that walks the page's text (as below).
- Lite: rudolf-webkit's tab answers a new `find` message with `webkit_find_controller_search` and
  `count_matches` (WEBKIT_FIND_OPTIONS_WRAP_AROUND, CASE_INSENSITIVE unless "Match case").
- Oracle's own pages: a script added to every srcdoc page, which marks matches with `<mark>` through a TreeWalker over
  text nodes (never innerHTML) and scrolls to the current one.
- Frames fallback: the bar says "Find is not available for this page" (the page is another site's frame).
- **The keys.** A page may take Ctrl+F for itself (Google Docs, VS Code for the web), so Oracle takes it first, as
  Chrome does. On Standard, rudolf-display hands Oracle's own keys to the desktop as Ctrl+F20–F23 (desk_key, 1.5.9).
  Ctrl+F and F3 join them, for Oracle's windows only. On Native, rudolf-native's key handling for Oracle's window does
  the same, as #434 did for Ctrl+W, T, N and Q.

**Tests.**
- A shell2 browser test with a stand-in shell (as oracle58_t.js): the bar, the count, next and previous, Escape,
  Match case, and the bar following its tab.
- A test of the real shell in Chromium: a local test page with 17 matches gives "1 of 17" and moves to the next one.
- WebKit: the same with rudolf-webkit (oracle_native_t.sh's harness).
- The keys: Ctrl+F in a page that listens for it opens Oracle's bar on Standard and Native (dispkeys, tor_native_t.sh's
  way of sending keys).

**The owner's checks.** Ctrl+F on a long page (a Wikipedia article) on Standard, Native and Lite; F3; Escape; Ctrl+F
inside Google Docs (Oracle's bar opens, as in Chrome).

## Item 2. Reopen a closed tab, and restore the last session

**What the person sees.**
- Ctrl+Shift+T, or ☰ › Recently closed, reopens the last closed tab at its place, with its back and forward history.
  Pressed again, it reopens the one before. The list keeps the last 25 tabs, and a closed window counts as one entry
  that brings all its tabs back.
- ☰ › Recently closed lists them with their titles and sites.
- Settings › Oracle (or ☰ › Settings) › "When Oracle starts":
  - Open the start page (the default).
  - Continue where you left off: every Oracle window and its tabs as they were, each tab asleep (💤) until it is
    clicked, so starting is quick.
- After Oracle or the desktop ended unexpectedly (the memory guard, a crash, the power), the next Oracle window offers
  "Restore your tabs?" once, whatever the setting.

**How.**
- tabs26.js and browser() keep a per-window list of closed tabs: the URL, its title, the history entries (`t.h`, `t.i`)
  and the zoom. Real tabs give their own history through the shell's back and forward events (`t.rb`, `t.rf`).
- The session (windows, tabs, the active tab) is saved in the account's storage, with Oracle's history and bookmarks,
  when a tab opens, closes or navigates (at most once a second). A clean exit marks it closed.
- On start, "continue" opens each tab asleep with the sleeping tabs' existing path (`t.slept`, rtOpen when shown).
- Never for private and Tor windows: their tabs go to an in-memory list that disappears with the window.
- On Native, each Oracle window is its own one-app page (oneapp59.js). The session is the account's, written by
  whichever window changed last. The native desktop starting Oracle at sign-in reads it once, so two windows never
  restore the same session twice.

**Tests.**
- shell2: close three tabs, Ctrl+Shift+T three times (order, history, place); a closed window brings back its tabs.
- The session saved, the page reloaded, "continue" restoring windows and asleep tabs; "start page" not restoring.
- An unclean end offers "Restore your tabs?" once.
- A private window leaves nothing in storage.
- Native: two Oracle windows, the desktop restarted, both windows back once (host harness, same_oracle_t.sh).

**The owner's checks.**
- Close a tab and press Ctrl+Shift+T on Standard and Native.
- Turn on "Continue where you left off", restart the Vivobook, and the tabs are back.
- A private window's tabs are not offered.

## Item 4. A password manager for websites

**What the person sees.**
- After signing in to a site, Oracle asks "Save your password for example.com?" with Save, Never for this site and
  Not now.
- On a sign-in form of a site with saved passwords, a key 🔑 appears in the password box. Clicking it, or the user
  name box, shows the saved accounts. Choosing one fills both boxes, and nothing is filled before the person chooses.
- On a sign-up or new-password form, the key offers "Use a strong password" (20 characters, generated by Rudolf),
  saved once the form is sent.
- Settings › Privacy › Saved passwords for websites lists the sites, user names and dates. Show, copy, edit or delete
  each one, after the account's password. Import from a Chrome, Edge, Firefox or Bitwarden CSV export, and export to
  CSV, both after the account's password and with a warning that the file is not encrypted.
- Alongside it, the existing "Saved passwords for apps" (keyring60.js) stays as it is.

**Where they are kept.** In the signed-in account's keyring (1.6.0 Part 14, the Secret Service), in a collection of
Oracle's own ("Oracle website passwords"), unlocked when the account signs in. Nothing is kept in the page's storage,
localStorage or a file, and nothing leaves the computer: there is no sync in this plan. Removing the account removes
them.

**How.**
- rudolfd gets keyring endpoints for Oracle (as the account's own Linux user, as keyring/list does): `pw/list` gives
  sites and user names only; `pw/get` returns one password for an exact origin. `pw/set`, `pw/del`, `pw/import` and
  `pw/export`, with `get`, `export` and the Settings page's Show needing the account's password again, as elevation
  does today.
- **The page never sees the list.** A content script in the tab (addContentScripts for real tabs, a
  WebKitUserContentManager script for Lite, both in an isolated world) finds password forms and tells Oracle the
  form's origin when it is submitted. Oracle, not the page, asks rudolfd, and only for the tab's own top-level origin
  (scheme, host and port exactly: never a parent domain, never http for an https entry, never inside a cross-origin
  iframe).
- The fill goes back to that one form by the content script, after the person's click.
- **Never:** filling on http:// pages without a warning, filling without a click, or saving from private and Tor
  windows. The frames fallback (no access to the page) is never filled.
- The generator uses `crypto.getRandomValues` with a site-friendly alphabet. The CSV import reads the common column
  names (url, username, password, name).

**Tests.**
- shell2 with a stand-in shell and rudolfd: save, the key, fill after a click, never before; Never for this site.
- Origin rules: https://a.example's entry not offered on http://a.example, sub.a.example, a.example:8443 or in an
  iframe of another site; a private window offers but never saves.
- Host tests of rudolfd's endpoints with a test keyring (gnome-keyring in a private session): only names in `pw/list`;
  `pw/get` refused without the password and for another account's user.
- CSV import of Chrome's and Firefox's export formats (made up test data only).
- QEMU on the real release: save on a local test sign-in page, restart, fill.
- htmlsinks reviewed (the Settings list drawn as text).

**The owner's checks.**
- Sign in to a site (an account of the owner's choosing, by the owner), Save, sign out, sign in again with the key, on
  Standard and Native.
- Settings › Privacy › Saved passwords for websites: Show asks for the account's password.
- Import a CSV exported from Chrome (by the owner; the file deleted afterwards).

**Security review before merging:** the content script's isolation, the origin match, and the keyring endpoints'
access (only the desktop's page of that account, as Part 10's system API rule). This part should get a separate
review pass, as the update path's did.

## Item 5. Ad and tracker blocking

**What the person sees.**
- A shield 🛡 at the right of the address box with the number blocked on this page. Its menu shows "Blocking ads and
  trackers on example.com" with a switch to turn it off for this site, and "Report a broken site" (a note in the health
  log, nothing sent).
- Settings › Privacy › Ads and trackers: on (the default), off, and the sites it is off for.
- Private and Tor windows always block.

**What it blocks.** Network requests matching EasyList (ads) and EasyPrivacy (trackers), and the empty boxes left
behind (element hiding). The lists are compiled at build time into a compact matcher (domains, URL patterns with
their options: third-party, resource type, exceptions), and the page's hiding rules are applied as CSS. Licences:
EasyList and EasyPrivacy are dual-licensed GPLv3+ / CC BY-SA 3.0. They are credited in About Oracle, with the list
versions.

**How.**
- Real tabs: the shell's existing request interceptor (`request.createWebRequestInterceptor`, which the firewall
  already uses for every request) checks the matcher next to the firewall's rule. The hiding CSS is added by
  `addContentScripts` at document-start: generic rules plus that site's rules.
- Lite: the same lists converted at build time to WebKit's content-blocker JSON, compiled once with
  `WebKitUserContentFilterStore` and added to each tab's content manager. This is WebKit's own fast path, as Safari
  uses.
- Frames fallback: not possible (another site's frame). The shield says so.
- **Updates.** The lists change weekly. They come with Rudolf's updates, and between updates as a signed list
  published with the Library (signed by the Library's key; never an unsigned download). The matcher keeps working
  with the list it has when there is no network.
- **Speed.** The interceptor answers in microseconds per request (a hash of domains, then a small set of patterns).
  Measure page loads with and without on the Vivobook. If the interceptor is too slow, the fallback is shipping
  uBlock Origin Lite (GPLv3, Manifest V3, declarativeNetRequest) as a built-in extension loaded as Ruffle's is
  (`--load-extension`), with Oracle's shield driving it.

**Tests.**
- Host test of the compiler and matcher against EasyList's own syntax cases (exceptions, $third-party, $domain,
  separator ^).
- shell2 and real-shell tests with local test pages: an "ad" script and a tracker pixel from a second local host
  blocked, the page's own scripts not, the count right, off for this site restoring both.
- Lite: the same with rudolf-webkit.
- Page-load times measured with and without, in CI and on the Vivobook.
- A private window always blocking.

**The owner's checks.** A news site with ads on Standard, Native and Lite; the shield's count; turning it off for that
site; YouTube, a bank's site and Google sign-in still working (the usual places a blocker breaks things).

## Item 7. Reader mode

**What the person sees.** On a page with an article (a news story, a blog post, a recipe), a 📖 button appears in the
address box. Pressing it (or F9) shows the article alone: its title, author, date, text, pictures and links, in
Rudolf's reading style.
- **Style:** the size (A− A+), the width, the font (sans, serif, OpenDyslexic when the account uses the dyslexia
  font), and the colours (light, sepia, dark; dark by default when Rudolf's dark theme is on).
- **Read aloud:** Rudolf's own voice (tts25.js), with the sentence being read highlighted.
- **Leaving:** "Exit reader" (or F9 again) goes back to the page as it was. Reader mode is per tab, and the address
  stays the page's.

**How.**
- Mozilla's Readability.js (Apache-2.0, credited in About Oracle) is run in the tab by Oracle (`executeScript` for
  real tabs, rudolf-webkit's script for Lite) when the page has loaded. Its `isProbablyReaderable` shows the button,
  and `parse()` returns the article.
- The article's HTML goes through `safeHtml()` (sanitize59.js: parsed inert, nothing that runs, links made plain) and
  is shown in an Oracle page of its own (`about:reader?u=…`, srcdoc, sandboxed, no scripts but Oracle's controls).
  Pictures are loaded from the site as the page did; nothing else is.
- Oracle's own pages and files: reader mode on a .HTM file on C: as well. Frames fallback: not available.
- Private and Tor windows: available; nothing is kept.
- Printing from reader mode prints the article alone (print25.js), which is the most common reason people want it.

**Tests.**
- shell2: a local article page shows the button and a plain page does not; the article's title and text are right;
  scripts and event handlers in the article never run (an `onerror` and a `<script>` in the test page).
- The size, font and dark settings, read aloud starting, exit restoring the page.
- htmlsinks reviewed (the article is HTML from data: through safeHtml only).
- Lite: the same with rudolf-webkit.

**The owner's checks.** A long news article on Standard, Native and Lite: the button, the article, A+, dark, read
aloud, print, exit.

## Order and versions

Suggested; the owner decides.

| Part | Size | Suggested version | Why |
|---|---|---|---|
| 1. Find in page | small | 1.6.1 | everyone uses it; the keys path exists (#434) |
| 2. Closed tabs and the session | small–medium | 1.6.1 | uses sleeping tabs and the existing history |
| 7. Reader mode | medium | 1.6.1 | Readability and safeHtml exist; low risk |
| 5. Ad and tracker blocking | medium–large | 1.6.1 or 2.0 | the interceptor exists; the list updates need the Library's signing |
| 4. Passwords | large | 2.0 | security-sensitive: its own review |

Each part is its own PR from its own branch, title and body starting "❇️ Approval/merge only.", with a CHANGELOG line,
the page rebuilt when desktop/ changes, and the owner's checks listed. Oracle's comparison on Native
(same_oracle_t.sh, #435) gets each new piece of UI (the find bar, the shield, the reader button), so Native keeps
matching Standard.

## Item 6. Extensions (not planned yet): the recommended source

The owner asked what store or list Oracle should install extensions from. The recommendation:

- **Rudolf's own reviewed list, in the Library.** Each extension is an open-source Manifest V3 extension built from its
  published source by rudolfos-library's workflow, signed with the Library's key, and listed in the App Store under
  "Oracle extensions". Oracle loads only extensions signed by that key, as rpk does for packages. Each one has a
  stated licence, a pinned version, a review of its permissions, and updates through the Library.
- **First candidates:**
  - uBlock Origin Lite (GPLv3), for those who want it instead of item 5;
  - Bitwarden (GPLv3), for people who already keep their passwords there;
  - Dark Reader (MIT);
  - Privacy Badger (GPLv3);
  - SponsorBlock (GPLv3).
- **Later, perhaps:** the Chrome Web Store, behind Developer mode or an administrator's "Allow any extension"
  setting, with a plain warning. Rudolf's Chromium can install from it as other Chromium browsers do, but its
  extensions are reviewed by Google, not by Rudolf, and many are closed-source.
- **Not Firefox's add-ons site:** its extensions are written for Firefox and do not run in Chromium unchanged.
