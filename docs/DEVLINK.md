# The development link

The development link lets a developer run things on a Rudolf OS computer from anywhere. The developer can be a person, Claude, another agent, or an agent of your own. Everything goes through a **private GitHub repository** that the computer's owner chooses, used as a mailbox:
- the developer writes a command;
- the computer runs it and writes back the result.

This page is the whole protocol, so any agent can use it. The reference client, `iso/systools/devlink.py`, uses only Python's standard library, so it runs anywhere Python 3.8 or later does. On Rudolf OS the same command line is the `devlink` command (`iso/sys/rudolf-dev.c`).

The program on the computer is `iso/sys/rudolf-devlink.c`. Its first plan is docs/plan-1.4.0.md; the limits came in 1.5.9 (plan 15.4 and 15.7), and the client and this page in 1.6.0 (plan Part 19). It works the same on an official Rudolf OS and on your own build (docs/custom-builds.md).

## 1. Setting it up

**The repository** (once):
1. Make a new **private** repository on GitHub, for example `you/rudolfos-devlink`. The computer refuses a public one: anyone could read what runs on it.
2. Give it a branch named `link`. One file in it is enough, for example a README:
   ```sh
   git switch -c link && git commit --allow-empty -m link && git push -u origin link
   ```
3. Make a **fine-grained personal access token** (GitHub › Settings › Developer settings › Fine-grained tokens):
   - Repository access: *Only select repositories* › this repository alone.
   - Permissions › Contents: **Read and write**. Nothing else.
   - An expiry date: a month is plenty.

   The computer and the developer can share one token, or each have its own.

**The computer** (Settings › Security › Development link). Only an administrator can do this, and the computer checks their password.
1. Tick **Allow the development link on this computer**.
2. Give the repository (`you/rudolfos-devlink`) and the token. The computer checks that the repository is private and that the token can write to it. The token is kept for root alone, and is never shown again, logged or sent.
3. Choose:
   - **how long**: 1 to 8 hours;
   - **Ask first**: on by default, and on again after every restart;
   - **Allow changes to this computer**: off by default, and off again at every start;
   - **Let it read files in home folders**: off by default;
   - **Keep on after restarting**: off by default.
4. **Turn on.** A red banner over everything shows the time left and **End now** for as long as the link is on.

**The developer**: the repository's name, and a token that can write to it.
- Keep the token in `DEVLINK_TOKEN`, or in a file (`~/.config/rudolf-devlink/token`, mode 600).
- Never put it in a command, a commit or a chat.

## 2. The mailbox

Everything is on the branch `link`:

| Path | Written by | What |
|---|---|---|
| `cmd/NNNNNN.json` | the developer | a command |
| `res/NNNNNN.json` | the computer | its result |
| `res/NNNNNN/NAME` | the computer | a file sent with a result (a screenshot, a file asked for with `get`) |
| `files/NAME` | the developer | a file for `put` to bring to the computer (any path works; `devlink.py` uses this one) |

**Ids** are six digits, `000001` to `999999`.
- The next id is **one more than the highest in `cmd/` or `res/`**. Count both: a result can outlive its command.
- Write the command as a **new** file. If GitHub refuses it because another writer took that id (`409` or `422`), look again and take the next one.
- Never rewrite or delete a command that is already there: the computer may already have run it.

**Order.** The computer looks for new commands about every 5 seconds. It runs them **one at a time, in id order**.
- A command that already has a result in `res/` is never run again, even after a restart or on another computer.
- A command that was running when the computer restarted is not run again. Its result says `interrupted`.

## 3. Commands

A command is a JSON object with a `kind` and that kind's fields:

| `kind` | Fields | What it does | When the link is read-only |
|---|---|---|---|
| `sh` | `script` (text); `timeout` (seconds, 1-3600, 600 by default) | runs the script with `/bin/sh` | runs as `nobody`, which can read everything but change nothing (every mount read-only, its own `/tmp`, no system API) |
| `screenshot` | (none) | sends `res/NNNNNN/screen.png`, the whole screen | the same |
| `get` | `path` (a full path) | sends the file as `res/NNNNNN/NAME` (at most 25 MB) | the same |
| `put` | `name` (letters, digits, `-` `_` `.` `+`); `src` (a file on the branch `link`) **or** `url` (a file of a release of the official Rudolf OS, `https://github.com/merciless146/RudolfOS/releases/download/...`, or of the repository the computer's own build comes from) | brings the file to `/persist/devlink/files/NAME` (at most 512 MB) | refused |
| `restart` | (none) | restarts the computer; the link comes back only with **Keep on after restarting** | refused |
| `end` | (none) | ends the link (the owner must turn it on again) | the same |

Examples:

```json
{"kind": "sh", "script": "uname -a; tail -n 50 /persist/var/log/health.log", "timeout": 120}
{"kind": "get", "path": "/run/rudolf/session.log"}
{"kind": "put", "name": "test-kernel", "src": "files/test-kernel"}
```

`get` never sends:
- the link's own token;
- files in home folders, unless the owner allowed it.

`sh` output is cut at 1 MB for stdout and 1 MB for stderr.

## 4. Results

`res/NNNNNN.json`:

| Field | What |
|---|---|
| `id`, `kind` | the command's |
| `started`, `ended` | times (UTC, ISO 8601) |
| `status` | the script's exit status. 0 means done. **-1 means the command did not run, or failed before running** (see `error`). 128+N means it was ended by signal N |
| `statusText` | for example `timeout after 600 s`, `ended by signal 9` |
| `stdout`, `stderr` | text |
| `files` | the names sent in `res/NNNNNN/` |
| `error` | why it did not run: refused on the computer, not allowed within 10 minutes, read-only, a bad path, too large… |
| `approved` | `allowed on the computer`, `refused`, `no answer`, `no desktop to ask` or `not asked (Ask first was off)` |
| `system` | the computer's Rudolf OS: version, serial, and whether it is official or a custom build |

## 5. Ask first, read-only, and when it ends

- **Ask first** (on by default): each command waits on the computer's screen, saying what it would do, for **Allow** or **Refuse**.
  - Ten minutes without an answer means not run.
  - If the desktop is not running, nobody can allow anything, so nothing runs. The result says so.
- **Read-only** (unless the owner ticked *Allow changes to this computer*): see the table in section 3.
- The link ends by itself:
  - when its hours are up;
  - after **4 hours without a command**;
  - when GitHub refuses the token;
  - with `end`;
  - with **End now** on the computer.

  The computer says why in Settings › Security › Development link and in System health.

## 6. Good practice for agents

- **One command, then its result**, then the next. Commands queue up, but a person answering Ask first sees them one at a time.
- **Say what a script is for** in a comment on its first line. The person who allows it reads it.
- **Read before you change.** Most diagnosis needs nothing more than read-only.
- **Ask the owner before you restart**, and before running anything that could lose their work. The owner may be using the computer.
- **No secrets in commands.** Everything in the repository is kept in its history.
- **When the owner is not there:** with Ask first on, nothing runs, and that is intended.

## 7. The reference client

```sh
export DEVLINK_REPO=you/rudolfos-devlink DEVLINK_TOKEN=...      # or --token-file
python3 devlink.py run sh 'uname -a'                     # prints stdout and stderr; exits with the command's status
python3 devlink.py run sh - < diagnose.sh --timeout 300
python3 devlink.py run screenshot --save .
python3 devlink.py run get /persist/var/log/health.log --save .
python3 devlink.py run put test.sfs --file ./test.sfs    # uploaded to files/test.sfs, then brought over (95 MB at most this way)
python3 devlink.py send sh 'long job'                    # prints the id; later:
python3 devlink.py wait 000042 --wait 3600
python3 devlink.py last                                  # the newest command and result
```

`run` and `wait` exit with:
- the command's status;
- **124** when no result came in time;
- **2** when the command could not be sent (wrong token, no branch `link`, bad arguments).

As a module:

```python
from devlink import DevLink
d = DevLink('you/rudolfos-devlink', token)
r = d.run('sh', script='cat /proc/meminfo')
print(r['status'], r['stdout'])
```

The client retries when GitHub fails for a moment, never prints the token, and takes the next id when another writer got there first. Tests: `iso/qt/host/devlinkclient60_t.py` runs it against a stand-in for GitHub and the real `rudolf-devlink`.

## 8. Your own builds

A fork of Rudolf OS (docs/custom-builds.md, section 7) has the same link, with your own devlink repository; `put --url` takes your build's own releases too.

Blitzen is a developer's agent through this protocol too, once Developer mode is on (Settings › Apps › Blitzen › Developer; Blitzen Developer's *Test computer*). It sends a command only after you Allow that one command, shown word for word, and the test computer's own Ask first still applies. Its tokens are kept in the account's keyring.
