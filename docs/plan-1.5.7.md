# Rudolf OS 1.5.7: Blitzen diagnostics, and what the 1.5.6 tests taught

Asked for on 2026-11-09, after the Vivobook tests of the 1.5.6 betas. 1.5.6 finishes first (its installation wizards,
the camera, the Terminal and 1.5.5's remaining File Explorer items); this plan starts once 1.5.6 is promoted.

Three parts:
1. **Blitzen diagnostics:** Blitzen reads the computer's own state and Rudolf OS's documents, and explains what went
   wrong, without changing anything.
2. **Recommended improvements:** what the 1.5.6 betas showed on real hardware: a deleted download, a wrong path, a
   setting lost between requests, a browser restarted after sleep. Each led to a fix; these make the next ones less
   likely, and quicker to find.
3. **Apps, games and start-up** (added on 2026-11-09, after Steam and Among Us on the Vivobook): stopping apps,
   saying what the memory guard did, the network at start-up, and what 1.5.6 leaves for later. (Three more found that
   day went into 1.5.6 itself: prompts in front of full-screen games, installer windows hidden behind the desktop,
   and Chromium's own shortcuts that could close the desktop.)

## What is there today (checked in the code and on the Vivobook)

- **Blitzen** (1.3.x, blitzen26.js) is the offline assistant: a small model run by llama.cpp (rudolf-ai, started on
  demand by rudolfd), with a context of 8,192 tokens by default. It answers about code (BlockLua, Lua, Luau, Pilot.lua,
  JavaScript, C++, Python) with **context packs** (blitzpacks26.js): short reference cards, each under about 800 words,
  sent with a question. "Ask Blitzen" is in Notepad, the Code Editor, BlockLua Builder and the Terminal.
- **The Vivobook** (the test computer): Ryzen 3 7320U, 3.4 GB of memory, about 1.2 GB used with the desktop open. A
  model that fits beside the desktop has about 1 to 3 billion parameters.
- **What the computer records about itself:**
  - the health log (`/persist/var/log/health.log`, System health): start-ups, crashes, the browser restarted, sign-in
    failures, the encrypted folder;
  - rudolfd's log (`/run/rudolf/rudolfd.log`, lost at restart), session.log, the job log of the last task;
  - the system report (tm26.js `sysReport`, "Report a problem"): version, hardware, drivers, storage, network;
  - the hardware check (hwcheck30): keyboard, touchpad, camera, battery, screen;
  - the desktop's own events (`logEvt`, Event Viewer), which stay in the desktop's storage.
- **What could not be seen during the 1.5.6 tests:**
  - the desktop's errors were to go to `/run/rudolf/page.log` (`/api/clientlog`), but no page.log was ever written on
    the Vivobook, also when the 1.5.6 Downloads clean-up should have reported;
  - the desktop's events (Event Viewer) are not on the disk, so neither the devlink nor a system report sees them;
  - rudolfd's log is gone after a restart.

## Part 1: Blitzen diagnostics

### What it does

- **"Why?"** next to what failed:
  - a notification of an error ("Your files could not be opened", an install that failed);
  - a line of the health log or the Event Viewer;
  - the installation wizard's failure, the Terminal's last command.
- **"What's wrong with this computer?"** in System health: Blitzen reads the latest problems and says, in plain words:
  - what happened and when;
  - the likely cause, with the log line it comes from;
  - what to do: a setting to change, something to install, or "report it" when it is a fault in Rudolf OS.
- **"Draft a report":** the problem, the steps, the system report and the relevant log lines in one text, for the
  person to read and send (Report a problem, or to the developers). Nothing is sent by Blitzen.

### What it reads (read-only)

- **Rudolf OS's documents,** as reference material found by search, not training:
  - the CHANGELOG (what changed in which version, and the known fixes);
  - the plans in `docs/` (how each part works: the disk, Downloads, the wizards, encryption, updates);
  - the Help pages and the Settings search entries.

  Kept with the release as a small index (`/usr/share/rudolf/blitzen-docs`), split into sections of about 300 words,
  and searched by words (BM25) before each question. The best few sections go with the question: the model's context
  is small, so only what matters is sent.
- **This computer's state,** through a new rudolfd endpoint `ai/facts` that only reads:
  - the health log (last 100 lines), rudolfd's log, session.log (filtered: no passwords, tokens or keys), the last
    job's log;
  - the system report;
  - the desktop's events (sent by the desktop with the question);
  - the hardware check's last result.

  The person sees what is sent to the model ("Blitzen will read: …") and can remove lines first. It all stays on this
  computer, as Blitzen already does.

### What it does not do

- **No changes:** Blitzen does not run commands, change settings, install, delete or move files. It suggests; the
  person does it (with a button to open the right page when there is one).
- **No GitHub, no devlink, no keys:** the repositories, the development link and every key stay outside Blitzen.
  Anthropic's terms do not allow training a general assistant on Claude's outputs without written permission, and
  this plan does not: the documents are searched, not trained on.
- **No promises it cannot keep:** answers say how sure they are, and quote the log line they rest on. When nothing
  in the logs explains a problem, it says so and offers the report.

### Size and speed on the Vivobook

- The index of documents: about 2 MB, searched in the desktop in under 100 ms.
- An answer of a few sentences from a 1 to 3B model on 4 cores: about 10 to 30 seconds. The answer streams as it comes.
- When memory is short (a game, a large page), Blitzen says it will answer when there is room, as today.

### Tests

- **Host:** `ai/facts` filters secrets (a test log with a token, a password line and a key) and reads only the files
  it lists.
- **Browser** (a stand-in model that echoes what it received): the right sections and log lines are sent for a
  known failure; nothing is sent without the person seeing it; no button runs a command.
- **On the Vivobook:** three real problems from the 1.5.6 betas (a download not found, a package not readable,
  sleep restarting the browser), asked as the person would ask. The answer must name the right cause or say it does
  not know.

## Part 2: recommended improvements

From the 1.5.6 betas on the Vivobook, in order of how much each would have helped.

1. **Nothing deleted by a background task without a way back.** Two downloads were deleted by Linux apps' two-way
   copying (fixed in #133). Deletions that no person asked for (sync, clean-ups, reconciles) go to a recovery folder
   for 7 days instead (`.Recovered` in the account's folder, shown in the Trash with "Restore"), and the health log
   says what went there and why.
2. **The desktop's events and errors on the disk.** Find why page.log was never written on the Vivobook (the shell
   frame, the token, or the page), fix it, and write the desktop's Event Viewer to the account's folder too, so the
   devlink, the system report and Blitzen see them. rudolfd's log kept for the last 3 start-ups
   (`/persist/var/log/rudolfd.log.1…3`).
3. **State read from the system, not kept in memory.** rudolfd handles each request in its own process; a value set in
   one request was lost in the next (#131). A host test fixture now works the same way. Review rudolfd's other
   module-level state the same way: each value a request uses must come from the disk or the kernel.
4. **Hardware tests as scripts.** The checks run by hand over the devlink (the binds, the downloads folder, Debian's
   view, the camera, the battery limit) become one script, `rudolf-selftest`, run on request from Settings or the
   devlink. It only reads, and prints PASS/FAIL lines. Each beta is checked on the Vivobook with it.
5. **QEMU only where it must be,** as agreed on 2026-11-09: installing, "Encrypt my data", the move to the disk, and
   updates and going back. A QEMU test that sets up Debian (from a cached image) for the wizards and Debian's view of
   the folders, since that cannot be tested without Debian.
6. **Downloads:** remove partial downloads (`.crdownload`) older than a day in `.incoming` (one from 2026-11-04 is
   still there); a download held back by Chromium is offered again.
7. **The battery limit, checked:** read the limit back after setting it (as 1.5.6 does) and test the four limits on the
   Vivobook; some ASUS laptops keep only 60, 80 or 100 %. If 70 or 90 % is not kept, say so in Power and battery.
8. **Clear errors:** messages say what to do, not what failed inside ("not a package in your folders" was a wrong
   path). A pass over every error text shown by rudolfd and the desktop, with the Help page each points to.
9. **Test speed:** the browser tests run in parallel (each has its own port since 1.5.6), and a test of a fix fails
   before the fix (as lxdesk_t and mdevcam_t do).

## Part 3: apps, games and start-up

Found on 2026-11-09 with Steam (Flatpak) and Among Us (Proton) on the Vivobook.

1. **Ending an app from Task Manager.** Steam stuck half-closed (its window gone, its process left) could only be
   stopped over the devlink, and opening it again did nothing. Task Manager lists every app that runs: Flatpak apps
   (all their sandboxes as one, from `/run/user/N/.flatpak`, as the memory guard counts them since 1.5.6), Debian's
   Linux apps and Wine programs, also those with no window open (Discord in the tray). "End" asks the app to quit, then
   ends it after 5 s; a rudolfd endpoint `apps/end` does it as root, for that app's processes only.
2. **Saying what the memory guard did.** Since 1.5.6 it closes an app with no window open when memory runs out, and
   says so only in System health. A notification says it at once ("Discord was closed to free memory"), with a link
   to System health. And before a full-screen game starts while little memory is free and background apps are open:
   "Discord is running in the background and uses 600 MB. Close it for the game?"
3. **The network at start-up.** The Weather widget failed once at start-up, before the Wi-Fi had an address, and
   waited 15 minutes (fixed in 1.5.6 for the widget). The same for everything the desktop fetches at start-up: the
   update check, the App Store's lists, Oracle's home page and Blitzen's packs wait for the network (rudolfd's
   network state, and the `online` event) or try again soon, instead of failing once and waiting for the next turn.
4. **`rudolf-selftest` checks Linux apps and games** (with Part 2's item 4): `/dev/fd`, `/dev/stdin`, `/dev/stdout`
   and `/dev/stderr` (missing before 1.5.6: Steam's interface did not start), the compressed memory's size, the
   camera (`/dev/video*` and its group), controllers (`/dev/input/js*`, `event*`), the Flatpak and Debian state, and the
   windows on screen. The windows come from the display server itself (a short client, no new package: the computer has
   no xwininfo); 1.5.6's checks used such a script over the devlink.
5. **Memory in System health** (asked for on 2026-11-09). A **Memory** section in Settings › System health, beside
   Power and battery:
   - **Compressed memory:** Automatic (as large as the memory, at most 8 GiB, as since 1.5.6; recommended), Smaller
     (half of it, as before 1.5.6) or Off. No free size: too small runs out sooner, too large spends the computer's
     time compressing. A change applies at the next restart (shrinking a full one would stall the computer); the
     `rudolf.nozram=1` start-up option stays, and wins. Kept in `/persist/etc/rudolf-memory`, read by
     `/usr/lib/rudolf/memory`, set through a rudolfd endpoint `memory/settings` (administrators).
   - **Now:** memory in use, and how much is compressed and how full it is (as Task Manager shows).
   - **What the memory guard closed** in the last 7 days, from the health log (on 2026-11-09 a Newgrounds Flash game's
     page at 1,882 MB; Discord, when it is closed as a background app), each with its time and size.
   - **"Close apps with no window open when memory runs out"**, on by default (the memory guard's rule since 1.5.6).
6. **What 1.5.6 leaves for later:**
   - the installation wizards for **.AppImage** and **.bin** (Linux programs that run without installing) and **.apk**
     (Android apps, with Waydroid; it needs the kernel's binder);
   - the **camera in Debian's Linux apps** (`/dev/video*` into Debian's namespace, as the account's folders are);
   - **controllers for Linux apps** (Debian and Flatpak games: `/dev/input` and the uinput rules).

(Item 7, one Start button for the logo and the version, moved to the 1.5.6 plan on 2026-11-10: built in 1.5.6.)

## Stages (one PR each)

1. Desktop events and errors on the disk, and rudolfd's log kept (item 2): Blitzen and the devlink need them first.
2. `ai/facts`, and the documents' index built with the release.
3. Blitzen diagnostics in the desktop: "Why?", System health, "Draft a report".
4. The recovery folder for background deletions (item 1).
5. rudolfd state review (item 3) and `rudolf-selftest` (item 4).
6. Downloads, the battery limit and error texts (items 6 to 8).
7. Task Manager's "End" for apps, the memory guard's notification and game warning, and the Memory section in System
   health (Part 3, items 1, 2 and 5).
8. The network at start-up (Part 3, item 3), and `rudolf-selftest`'s Linux and game checks (item 4).
9. The .AppImage and .bin wizards; the camera and controllers in Debian; then .apk with Waydroid (Part 3, item 6).
10. QEMU with Debian (Part 2, item 5), then the Vivobook with `rudolf-selftest`; promote when you say.

## Size

Desktop: about 400 lines (Blitzen diagnostics, the recovery folder, events on the disk), and about 300 more for Part 3
(Task Manager's apps, the notifications, the network at start-up, the new wizards). rudolfd: about 300 (ai/facts,
logs, the recovery folder), and about 200 for Part 3 (apps/end, the selftest's checks, Debian's devices). Waydroid
is the one new package, for .apk only. The documents' index is built by the release workflow from `docs/`, the CHANGELOG and the
Help. The model stays the one Blitzen has.
