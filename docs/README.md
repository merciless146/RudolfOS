# Documentation for Rudolf OS

What is written down, and where. The disc's own files are for someone building or starting Rudolf OS
(`iso/BUILD.TXT`, `iso/INSTALL.TXT`, `iso/README.TXT`); everything here is for someone working on it.

## Start here

| File | What it is |
| --- | --- |
| `custom-builds.md` | Making your own Rudolf OS — your own keys, update address and Library — and going back to the official one. |
| `DEVLINK.md` | The development link: a developer (a person or an agent) running commands on a Rudolf OS computer through a private GitHub repository, and what is refused there. |
| `testing.md` | What has to be tested before a change reaches a computer, and where each kind of test lives. |
| `api.md` | rudolfd's API for everything that draws the desktop: each request, its reply and its errors. |

## How the system works

- `secure-boot.md` — starting on UEFI computers with Secure Boot on, and what to do when a part of it has to change.
- `security-keys.md` — the keys that unlock an account's files (1.6.1).
- `website-data-key.md` — the key that protects Oracle's websites (1.6.1).
- `system-reports.md` — how a report made in Rudolf OS reaches this repository.
- `gap-analysis-1.6.3.md` — what Rudolf OS still lacks beside other operating systems (hardware, system software and
  files), ranked, each gap with the first pull request it needs.
- `native-vivobook-checklist.md` — Native against Standard, checked on the owner's computer (1.6.0).
- `native-findings-2026-11-15.md` and `native-titlebars-2026-11-16/` — what the native desktop was measured and compared against.

## Plans

`plan-1.2.5.md` to `plan-2.0.md` are the plans for 1.2.5 to 2.0: one file per version, each written before the work
(what was wrong, what would be changed, what would be tested) and marked afterwards with "As built. …" where it was
built differently. They are kept as written, so a plan can be older than the code it describes; `CHANGELOG.md` is the
list of what shipped.

Bigger subjects have a file of their own: `plan-library.md` (the Library), `plan-games.md` (games), `plan-oracle.md`
(Oracle), `plan-memdebug.md` (where the memory goes).

## Working in the repository

`AGENTS.md` in the top folder has the rules for any session changing Rudolf OS: your own branch and a pull request, the
owner merges, never `main`; no key or token in a file or a command; `python3 iso/systools/deadcheck.py` before every
pull request.

## Building the page while you work

    cd desktop && python3 build19.py
    cd ../iso/www && python3 -m http.server 8600

The first line writes `iso/www/index.html`, the files it fetches from `iso/www/a/` and `iso/www/blitzen-docs.json`.
Nothing else needs copying. The second serves the page at http://localhost:8600/ — serve it rather than opening the
file directly, because the page reads some of its parts from `a/` beside it. Browser tests use their own ports, 8600
and up.
