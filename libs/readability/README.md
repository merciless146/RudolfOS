# Readability.js (Mozilla), for Oracle's reader mode

`Readability.js` is Mozilla's Readability, version 0.6.0, unchanged: the `@mozilla/readability` package on npm
(https://www.npmjs.com/package/@mozilla/readability, source https://github.com/mozilla/readability). Licensed under the
Apache License, Version 2.0 (`LICENSE.md`, as it comes in the package; copyright (c) 2010 Arc90 Inc).

Rudolf OS uses it in the desktop page for Oracle's reader mode (`desktop/orareader61.js`, docs/plan-1.6.1.md Part 10):
`desktop/build19.py` puts it beside the page as a file of its own (`iso/www/a/`, named by its contents), loaded the first time
reader mode is used. It runs on an inert copy of the website's page (DOMParser: nothing in it runs or loads), and what it
returns is made safe (`safeHtml`, desktop/sanitize59.js) before it is shown. It is credited in About Oracle.
