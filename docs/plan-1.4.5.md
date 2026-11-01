# Rudolf OS 1.4.5: SpreadsheetPro and PresentPro, the Cyberdeck test removed, "Open settings…", and shutting down and restarting quickly (plan)

What 1.4.5 adds and changes (asked for 2026-11-01, on 1.4.4):
- **Part 1:** the Cyberdeck test mode is removed.
- **Part 2:** the desktop's right-click item "Change background…" becomes "Open settings…".
- **Parts 3 and 4:** two companion apps to WordPro: **SpreadsheetPro**, in the place of Excel, and **PresentPro**, in the place of PowerPoint. They replace the small Spreadsheet and Presentations apps that Rudolf OS has had since 1.2.
- **Part 5:** Shut down and Restart are checked, measured and made to finish quickly.

The 1.4.4 updates waiting to be merged come first and are not part of this plan: #59 (one network icon) and #60 (a picture as the desktop background). The Fn-lock fix (#58) is already merged.

# Part 1: the Cyberdeck test mode is removed

Cyberdeck test mode is in About › For developers since 1.4.4, and was in Display Properties before. It made a fast PC behave roughly like the Rudolf Cyberdeck's Intel N100: 4 cores, no hyper-threading, about 2.4 GHz. Rudolf OS has no use for it anymore.

| # | What | Where |
|---|---|---|
| 1 | The desktop: the Cyberdeck test switch (`devCyberdeck`) and its place in About › For developers. The search keyword "cyberdeck" goes too. For developers keeps "Update the page from a file" | `desktop/native25.js`, `settings25.js`, `shell25.js` |
| 2 | rudolfd's `decktest` API | `iso/sys/rudolfd.c` |
| 3 | `/usr/lib/rudolf/decktest` | `iso/sys/root/usr/lib/rudolf/decktest` (deleted) |
| 4 | At start: `rudolf-init` no longer switches it on. A computer where it was left on has the flag file `/persist/etc/decktest`; it is deleted. Nothing else is needed, because the limits only ever lasted until the next start | `iso/sys/root/sbin/rudolf-init` |
| 5 | Old test scripts that use it: removed, or the lines taken out | `iso/qt/p3_t.py`, `iso/qt/ph23_t.py`, `iso/qt/shell2/settings144_t.js` |

**Tests:**
- `deadcheck` passes, with no route and no function left behind.
- `settings144_t`: About › For developers has no Cyberdeck test.
- Host test: `rudolf-init`'s start-up lines with a stand-in `/persist/etc/decktest` remove the file.

# Part 2: "Open settings…"

| # | What | Where |
|---|---|---|
| 6 | The desktop's right-click menu: "Change background…" is renamed **"Open settings…"**. It opens Settings where Settings normally starts, Display Properties. The new item is also added to the menu's translations | `desktop/shell25.js` (the menu in use), `desktop/base_v24.html` (the older menu, through `build19.py`) |

**Tests:** the page test right-clicks the desktop and finds "Open settings…" and no "Change background…". Clicking it opens Settings.

# Part 3: SpreadsheetPro

The current Spreadsheet (`apps25b.js`, since 1.2) is small:
- 26 columns by 100 rows, one sheet.
- 11 functions.
- Bold is the only formatting.
- CSV, and its own `.RSHEET` files.
- Formulas are run with `Function()`, which a file could misuse.

SpreadsheetPro replaces it, in the place of Excel, and looks and works like WordPro:
- a menu bar: File, Edit, View, Insert, Format, Data, Tools, Help;
- a toolbar;
- the same dialogs;
- Notey's help.

| # | What |
|---|---|
| 7 | **The grid:** 702 columns (A to ZZ) by 100,000 rows, drawn only where it is visible, so large sheets scroll smoothly. Several sheets, with tabs to add, rename, move, delete and colour them. Column widths and row heights, frozen panes, zoom from 50 to 200 % |
| 8 | **Selecting and editing:** the mouse and keyboard as in Excel: Shift for ranges, Ctrl for several ranges, Ctrl+arrows, Ctrl+Home/End, F2 to edit, Esc. The formula bar and the name box. Undo and redo; cut, copy and paste, also as values or formats only. The fill handle copies a cell or continues a series (1, 2, 3…; Monday…; January…; dates). Insert and delete rows, columns and cells, with the formulas that point to them adjusted |
| 9 | **Formulas:** a parser of their own, with no `Function()` or `eval`. Supported: A1, $A$1 and mixed references, ranges, other sheets (`Sheet2!A1`), whole columns (`A:A`), named ranges, and the operators `+ - * / ^ & = <> < > <= >= %`. Results are recalculated only where something changed, cycles are found, and errors are shown as Excel shows them (`#DIV/0!`, `#REF!`, `#NAME?`, `#VALUE!`, `#N/A`, `#NUM!`, `#CIRCULAR!`) |
| 10 | **About 100 functions**, in the groups below |
| 11 | **Formatting:** number formats (General, Number with decimals and thousands separator, Currency, Accounting, Percent, Scientific, Date, Time, Text, Custom codes such as `0.00` or `#,##0`). Font, size, bold, italic, underline, strikethrough, colour, fill, borders, horizontal and vertical alignment, wrap, merge and centre. The format painter, and cell styles such as Heading, Total, Good, Bad and Neutral |
| 12 | **Conditional formatting:** greater than, less than, between, equal to, text contains, top or bottom 10, duplicate values, colour scales and data bars |
| 13 | **Data:** sort by one or more columns, AutoFilter, Find and replace, data validation (a list, a number range, a date), remove duplicates, text to columns. Comments on cells |
| 14 | **Charts:** column, bar, line, area, pie, doughnut and scatter, from a selected range. Each chart is drawn on the sheet, can be moved and resized, and has a title, legend, axis titles and colours. It updates when the data changes. It can be copied into WordPro and PresentPro as a picture or as live data |
| 15 | **Files:** SpreadsheetPro's own `.RSHEET` (version 2: sheets, styles, charts); version 1 files from the old Spreadsheet open as before. Import and export of **CSV** and **Excel .xlsx** (values, formulas, number formats, fonts, fills, borders, merged cells, column widths, several sheets; through JSZip, which Rudolf OS already ships). Printing and PDF with page setup: orientation, fit to width, gridlines, repeated header rows, the print area. Templates: budget, invoice, timetable, grade book, loan calculator |
| 16 | **Taking over from the old app:** files, Start menu, Open with, recent files, the taskbar, parental controls and Task Manager open SpreadsheetPro where they opened Spreadsheet. The old app's id (`sheet`) stays a name for the new one, so desktop shortcuts and settings that name it keep working |

The functions in item 10:
- **Maths:** SUM, SUMPRODUCT, ROUND, ROUNDUP, ROUNDDOWN, INT, MOD, ABS, SQRT, POWER, PI, RAND, RANDBETWEEN, …
- **Statistics:** AVERAGE, MEDIAN, MODE, STDEV, VAR, MIN, MAX, LARGE, SMALL, RANK, COUNT, COUNTA, COUNTBLANK, COUNTIF(S), SUMIF(S), AVERAGEIF(S), …
- **Logic:** IF, IFS, IFERROR, AND, OR, NOT, XOR, SWITCH, CHOOSE.
- **Lookup:** VLOOKUP, HLOOKUP, XLOOKUP, INDEX, MATCH, OFFSET, ROW, COLUMN, INDIRECT.
- **Text:** LEFT, RIGHT, MID, LEN, FIND, SEARCH, SUBSTITUTE, REPLACE, UPPER, LOWER, PROPER, TRIM, TEXT, VALUE, CONCAT, TEXTJOIN, REPT.
- **Date and time:** TODAY, NOW, DATE, TIME, YEAR, MONTH, DAY, WEEKDAY, EDATE, EOMONTH, DATEDIF, NETWORKDAYS.
- **Finance:** PMT, FV, PV, NPV, IRR, RATE.

Every function has a short description, shown as you type its name.

# Part 4: PresentPro

The current Presentations app (`apps25b.js`, since 1.2) is small:
- Each slide has one title, one text and one picture.
- A colour background.
- Present, and export to PDF.
- Its own `.RDECK` files.

PresentPro replaces it, in the place of PowerPoint, with the same look as WordPro and SpreadsheetPro.

| # | What |
|---|---|
| 17 | **Slides:** the slide list on the left, with thumbnails; drag to reorder; duplicate, hide, sections. The slide sorter view. Layouts: Title, Title and content, Two content, Comparison, Section header, Title only, Blank, Picture with caption. Size 16:9 or 4:3 |
| 18 | **Themes:** at least 10, each with colours, fonts and a background. A master slide sets the logo, footer, date and slide numbers for every slide. Each slide's background can be a colour, a gradient or a picture |
| 19 | **Objects on a slide:** text boxes (rich text: font, size, bold, italic, underline, colour, highlight, alignment, bullets and numbering, line spacing, auto-fit), shapes, pictures, tables, charts and icons. Every object can be moved, resized, rotated, layered, grouped, aligned and spaced, with guides and snapping. Shapes include rectangle, rounded rectangle, ellipse, triangle, arrows, lines, connectors, star, callout and speech bubble, each with a fill, an outline and a shadow. Pictures come from your files and can be cropped, given a border, and have their brightness set. Charts come from data typed into PresentPro or from a SpreadsheetPro range |
| 20 | **Speaker notes** for each slide |
| 21 | **Transitions and animations:** transitions between slides (fade, push, wipe, cover, zoom, morph-like move) and animations for each object (appear, fade, fly in, zoom, wipe; on click or after the previous), with timings. All are drawn with the processor and stay smooth on modest laptops such as the Vivobook |
| 22 | **Presenting:** full screen from the start or from the current slide. The keyboard, mouse and a presenter remote's keys move through the slides (Page Up and Down, arrows, B or W for a black or white screen, a number then Enter to jump to a slide). A laser pointer and a pen. On a second screen, or in a window, the presenter view shows the current and next slide, the notes, the time and the elapsed time. Rehearse timings, and a slide show that runs by itself in a loop |
| 23 | **Files:** PresentPro's own `.RDECK` (version 2); version 1 files open as before. Export to **PowerPoint .pptx** (text, shapes, pictures, tables, notes, transitions where PowerPoint has the same) and import of .pptx (text, shapes, pictures, tables, notes). PDF of the slides, handouts with 1, 2, 3 or 6 slides a page, notes pages, and PNG pictures of the slides. Templates: talk, lesson, report, photo album, pitch |
| 24 | **Taking over from the old app:** the same as item 16, with the old id `deck` |

# The companion apps together

| # | What |
|---|---|
| 25 | One look across WordPro, SpreadsheetPro and PresentPro: the same menus, toolbar, dialogs, themes, keyboard shortcuts, Notey, recent files and autosave. Autosave makes a recovery copy every minute while there are changes, and offers it after a crash or power cut |
| 26 | Working between them. A SpreadsheetPro range pasted into WordPro or PresentPro becomes a table. A chart can be copied as a picture or as live data. WordPro's **Send to PresentPro** makes slides from a document's headings, and PresentPro's **Send to WordPro** makes a handout from the slides and notes. WordPro gets **Insert › Spreadsheet table** and **Insert › Chart** |
| 27 | In the Start menu: an **Office** group with WordPro, SpreadsheetPro and PresentPro. They are in the App Store's list of built-in apps, and Settings › Apps lists them as the default apps for their file types |

## How SpreadsheetPro and PresentPro are built ("create, then refine")

1. **Create:** each app is written as its own module (`desktop/spro45.js`, `desktop/ppro45.js`), with shared parts in `desktop/office45.js`: the menu bar, toolbar, dialogs, colour and font pickers, autosave, and the OOXML zip helpers. Each comes with a page test, and with the security rule that no file content ever reaches `eval`, `Function` or `innerHTML` unescaped.
2. **Refine, round 1 (correctness):**
   - Real files: test workbooks and decks made with the Python libraries openpyxl and python-pptx (formulas, formats, merged cells, charts, pictures, notes) are imported, saved and read back. Values and formulas match what openpyxl reads.
   - Formula results are compared with a reference table of more than 500 cases worked out with Excel's rules.
3. **Refine, round 2 (use):**
   - The common tasks are done start to finish in the page test, with screenshots checked at each step: a monthly budget with a chart; a grade book with conditional formatting and sorting; a 10-slide talk with a theme, pictures, a chart from SpreadsheetPro, notes, transitions, presenting with the presenter view, and PDF and .pptx export.
   - What is slow, unclear or missing is fixed.
4. **Refine, round 3 (speed and size):**
   - 100,000 rows with formulas should scroll at 60 frames a second and recalculate within 1 second on the Vivobook's processor.
   - A 50-slide deck with pictures should open within 2 seconds, and present at 60 frames a second.
   - The desktop page grows by no more than 600 KB for both apps (no new libraries beyond JSZip and what is already shipped).

**Tests:**
- New page tests: `spro_t.js` and `ppro_t.js` (with openpyxl and python-pptx fixtures), and `office_t.js` (working between the apps; autosave and recovery).
- The QEMU test `t145o` opens both apps in the real desktop and presents with the keyboard.
- On the Vivobook: make a short deck and a budget, present the deck, and export both. The .xlsx and .pptx files open in Excel and PowerPoint (if available to you).

# Part 5: shutting down and restarting quickly

**What happens now.**

The desktop, when Shut down or Restart is chosen:
- saves the open files (`fsFlush`, the home folder's encryption `homeChain`);
- saves the session and the local storage copy;
- plays the shutdown sound;
- calls rudolfd.

None of these steps has a time limit; if one stalls, the dialog stays at "Please wait…".

rudolfd's `power?action=poweroff|reboot` then:
- asks the desktop's Chromium to close, and waits up to **25 seconds** for it;
- kills it;
- finishes keeping the files of a live session, if that applies;
- runs `sync` and `umount -a -r`;
- powers off or restarts.

The browser window for sign-in sites (RudolfWeb), Linux apps, the sound server and Wi-Fi are not asked to close. `/persist` is often still in use when it is unmounted, so it is not made read-only first. It is left to the kernel's journal on the next start, which takes extra time.

| # | What | Where |
|---|---|---|
| 28 | **Measure first.** Each step of shutting down and restarting writes a timestamp to `/persist/var/log/shutdown.log`, flushed before the disk is unmounted. Settings › About › For developers shows the last one. Measured in QEMU and on the Vivobook: from pressing Shut down to the power going off, and from Restart to the firmware's logo | `desktop/core25.js`, `iso/sys/rudolfd.c` |
| 29 | **The desktop's steps get a time limit:** 3 seconds in all for saving, then shutting down goes on (what was saved is kept; the page's local storage copy was already sent every few seconds). The dialog shows what it is doing ("Saving your files…", "Closing apps…", "Turning off…") | `desktop/core25.js` |
| 30 | **rudolfd closes everything at once:** both Chromium instances, Linux apps and Flatpak (bubblewrap) processes, the sound server and Wi-Fi get TERM together. It waits up to **5 seconds**; Chromium needs about 1, the desktop's data having been saved first. The rest get KILL. Then `sync`, `/persist` remounted read-only, `umount -a -r`, and the power off or restart | `iso/sys/rudolfd.c` |
| 31 | **The same path** for the power button with "Shut down" (1.4.4 Power and battery), updates' "Restart now", and every other restart button | `iso/sys/root/usr/lib/rudolf/power`, desktop |
| 32 | **If the firmware's reboot hangs** after `reboot()`, which some laptops do: only if the measurements show it, the kernel gets `reboot=` (`acpi`, `pci` or `efi`, whichever works on the Vivobook) | `iso/kconfig.sh` (only if needed) |

**Targets:**
- Shut down: the screen dark within 5 seconds normally, and the power off within 10 at worst.
- Restart: the firmware's logo within 10 seconds.
- Nothing saved before pressing the button is lost.

**Tests:**
- New QEMU test `t145p`:
  - Shut down and Restart from the Power dialog, with an Oracle real tab and a Linux app open: QEMU exits, or the firmware starts again, within the targets.
  - A file saved in C:, and a setting changed, just before pressing are there after the start.
  - The start-up's file system check finds `/persist` clean (no journal recovery).
  - A desktop step made to stall (a test hook) does not stop the shutdown.
- On the Vivobook:
  - `shutdown.log` read through the development link after a restart.
  - Shut down timed by you.

# Release

- One version, 1.4.5.
- The order of work: parts 1 and 2 (small), part 5, then SpreadsheetPro and PresentPro (parts 3 and 4, with their three refinement rounds).
- Parts 1, 2 and 5 can go out first, as their own beta, if you would rather not wait for the two apps.
- Then: a beta, the QEMU tests (`t144k`, `t142`, `t145p`, `t145o`), promotion when you say so, and the checks on the Vivobook.
