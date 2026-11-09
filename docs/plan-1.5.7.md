# Rudolf OS 1.5.7: Blitzen diagnostics, and what the 1.5.6 tests taught

Asked for on 2026-11-09, after the Vivobook tests of the 1.5.6 betas. 1.5.6 finishes first (its installation wizards,
the camera, the Terminal and 1.5.5's remaining File Explorer items); this plan starts once 1.5.6 is promoted.

Two parts:
1. **Blitzen diagnostics:** Blitzen reads the computer's own state and Rudolf OS's documents, and explains what went
   wrong, without changing anything.
2. **Recommended improvements:** what the 1.5.6 betas showed on real hardware: a deleted download, a wrong path, a
   setting lost between requests, a browser restarted after sleep. Each led to a fix; these make the next ones less
   likely, and quicker to find.

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

## Stages (one PR each)

1. Desktop events and errors on the disk, and rudolfd's log kept (item 2): Blitzen and the devlink need them first.
2. `ai/facts`, and the documents' index built with the release.
3. Blitzen diagnostics in the desktop: "Why?", System health, "Draft a report".
4. The recovery folder for background deletions (item 1).
5. rudolfd state review (item 3) and `rudolf-selftest` (item 4).
6. Downloads, the battery limit and error texts (items 6 to 8).
7. QEMU with Debian (item 5), then the Vivobook with `rudolf-selftest`; promote when you say.

## Size

Desktop: about 400 lines (Blitzen diagnostics, the recovery folder, events on the disk). rudolfd: about 300 (ai/facts,
logs, the recovery folder). The documents' index is built by the release workflow from `docs/`, the CHANGELOG and the
Help. No new packages, and the model stays the one Blitzen has.
