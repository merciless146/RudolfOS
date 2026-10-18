# System reports

How a problem reported from Rudolf OS reaches this repository, and how to work with it.

## The path of a report

1. **On the computer.** The person chooses *Report a problem*: Task Manager (bottom right), or Start › Report a problem.
   They give it a title and say what happened; *Include the system report* is on by default.
2. **The system report** (`sysReport()` in `desktop/tm26.js`) describes the computer at that moment:
   - **Status:** Rudolf OS version and installed release, when the report was made, how the computer started (BIOS or
     UEFI; disc, USB drive or hard disk; live or installed; recovery), whether the desktop runs in the shell or in
     the plain page (and whether the shell failed), the display mode and size, uptime and load, CPU use, memory and
     swap, temperature, battery, free storage, network connected or not, the services, open windows and real tabs,
     and the number of kernel warnings.
   - **Computer:** machine and firmware, processor, graphics card and driver, sound cards, storage, network adapters
     (kind, state, speed), input devices.
   - **Kernel:** version, start-up options, drivers, memory in detail.
   - **Processes:** the ten using the most memory, the busiest, and the memory of each page (desktop, tabs).
   - **Latest:** kernel warnings and errors, the session log, the shell's notes, the desktop's errors.

   It leaves out what identifies the person: account names, file names, web addresses, network (IP) and hardware
   (MAC) addresses, e-mail addresses and Wi-Fi network names. The person can read it before sending (*See the
   report*), save it to `C:\DOCUMENTS` instead, or leave it out.
3. **GitHub.** *Continue on GitHub* opens a new issue in Oracle (real tabs keep the GitHub sign-in) with the form
   `.github/ISSUE_TEMPLATE/system-report.yml` filled in through the address: title, *What happened*, version,
   *Status at the time* and *Full system report*. GitHub refuses very long addresses, so when the full report does
   not fit, Rudolf OS puts it on the clipboard and the form says to paste it. The person reviews and submits; nothing
   is sent before that. A GitHub account is needed. Without internet, the dialog says to save the report and send it
   later.
4. **The database.** The workflow `.github/workflows/system-reports.yml` (script `.github/scripts/system-reports.js`)
   runs for every issue labelled `system-report` when it is opened, edited, closed or reopened, and files it in the
   **`system-reports` branch**:
   - `reports/<issue>.json`: the record: number, title, link, author, dates, state, version, the problem, every
     status line (`status`), and the full report text;
   - `index.json`: one line per report, newest first (state, date, version, how it started, desktop, display, memory,
     warnings, labels), for tools and scripts;
   - `README.md`: open reports by version and by area, and a table of all reports, to read on GitHub.

   It adds labels (`version: 1.2.0`, `area: sound`, `area: shell` when the shell failed, `needs: system report` when
   the status is missing) and, for a new issue, a comment with the key status lines.

## Working with reports

- **See them:** the issues labelled `system-report`, or the `system-reports` branch's README for the overview.
  Filter by `version: …` and `area: …` labels.
- **Address one:** reply on the issue, fix the area labels if the guess was wrong, link the fix (`Fixes #N` in the
  pull request), and close it when the fix is released. Closing updates its state in the database.
- **Missing status:** the comment asks the person to paste *Task Manager › Copy system report*; editing the issue
  files the new text.
- **Look across reports:** `index.json` has every report in one file, for example
  `curl -s https://raw.githubusercontent.com/merciless146/RudolfOS/system-reports/index.json | jq '.[] | select(.state=="open") | .desktop' | sort | uniq -c`.

## Safety

- The issue text is written by anyone. The workflow only reads it as data: it is parsed, trimmed and escaped (the
  README cannot gain links or images from it), the labels come from a fixed list, and nothing from it is run.
- The workflow has only `contents: write` and `issues: write`, writes only the `system-reports` branch, and takes its
  script from the default branch.
- Reports are public. The report leaves out personal details, but the person's own text and anything they paste
  are theirs to check; the form says so.

## Changing the report

Add a line to `key` in `sysReport()` for something every report should show (it lands in *Status at the time* and,
as `Key: value`, in the record's `status`), or a section below for details. Field ids in the form (`what`, `version`,
`status`, `report`) are what the desktop fills in; keep them when editing the form.
