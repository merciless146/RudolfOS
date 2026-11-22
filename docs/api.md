# rudolfd's API for the parts that draw the desktop

rudolfd (iso/sys/rudolfd.c) serves the desktop page on http://127.0.0.1:8080/ and an API under `/api/`. Every API
request carries the `X-Rudolf-Token` header: a random token made at each start and written only into the page rudolfd
serves (`window.RUDOLFD.token`), so websites cannot use the API. Since 1.5.9 the page and the API answer only the
desktop's own processes (its memory group, /sys/fs/cgroup/desktop) and root: any other program on the computer gets
403, even with the token (iso/sys/rudolfd-peer.inc). Replies are JSON with `"ok": true`, or `"ok": false`
and an `"error"` to show, with an HTTP status (400 a bad request, 403 refused, 404 not there, 409 not possible now,
423 locked, 429 wait, 500 failed).

(1.6.1, docs/plan-1.6.1.md Part 20) With rudolfd's confinement on (/persist/etc/rudolf-confine; off by default), each
window gets only what its own code asks for: the page (Standard's, Lite's and the native desktop's app windows) the
paths in iso/sys/rudolfd-winapi.h's page list, the native shell (rudolf-native) its own; anything else is 403 "this
window does not use that part of the API" and a line in the health log. A request rudolfd could not confine as it
should is 503. A new request the page or the shell asks for needs `python3 iso/systools/winapi.py --update`.

Plan 1.5.9, 11.4: what new features need keeps its logic and state in rudolfd and the services, so that what draws the
desktop (the page today; the native desktop engine being prototyped, plan Part 12) only shows state and sends choices.
Each part's API is written down here with its first use. (Older endpoints are described where they are made, in
rudolfd.c and its .inc files.)

## The native desktop engine (1.5.9, plan Part 12; rudolfd-desktop.inc)
What rudolf-native needs that only the desktop page knew. Settings, pinned apps and accounts are read from the copy of
the page's storage rudolfd keeps (`POST lsync`, /home/rudolf/.rudolf-ls.json: `{"gen":N,"data":{key: string}}`);
rudolf-native never reads that copy itself, and never sees a password hash or a key except the session's.

| Request | What it does |
|---|---|
| `GET desktop/accounts` | The sign-in screen's accounts: `{"setup", "guest", "last", "accounts":[{"id","n","r","key","old","keys"}]}`. `r` admin or user; `key`: signs in with an old page-only security key (only Standard can, until it is set up again under Security keys); `keys` (1.6.1): how many security keys rudolfd reads for it (then `desktop/login` asks for one); `old`: a password hash from before 1.4 (sign in once with Standard); `last`: the account used last; `guest`: the guest may sign in; `setup`: Rudolf OS has been set up. |
| `POST desktop/login` | Body `{"n":NAME,"p":PASSWORD}` or `{"guest":true}`. Checks the password as the page does (`p2$SALT$HASH`: PBKDF2-HMAC-SHA-256, 210 000 rounds), makes the password's key (PBKDF2 with the account's salt `ks`), opens the file key (`fk`, AES-256-GCM), and for an account whose files are on the disk opens its encrypted folder with the home folder's key (HKDF-SHA-512 of the file key, salt "rudolf-os home folder", info the account's id) and the session's binds, as `home/unlock?session=1`. Replies `{"id","n","r"}`. A wrong password: 403 and the health log ("signin"); five in a row: 429 for 15 s. (1.6.1) An account with security keys (rudolfd-fido.inc): after a right password, 401 `{"need":"key"}` before anything is opened; again with `"k":true`, a touch of one of its keys (30 s), and the file key is the keys' copy (an account that requires a key has no other). No code is asked then. |
| `POST desktop/logout` | Locks the account's folder (as `home/lock`) and ends the session. |
| `POST desktop/lock` | The lock screen is up: `desktop/session` answers 423 until unlocked. |
| `POST desktop/unlock` | Body `{"p":PASSWORD}`: the session's account's password (the guest: none). (1.6.1) An account with security keys: then 401 `{"need":"key"}`, and with `"k":true` the touch, as `desktop/login`. |
| `GET desktop/keys?acct=ID` | (1.6.1, rudolfd-fido.inc; docs/security-keys.md) The account's security keys: `{"keys":[{"i","name","added","last"}],"req","lib","devs","bad"}`: `req` a key is required (the files need one), `lib` libfido2 is there, `devs` FIDO keys plugged in now, `bad` the record is damaged. No secrets. |
| `POST desktop/keys/signin` | (1.6.1) Body `{"n","p"}`: the password checked (wrong ones counted per account as the codes are: 5, then 30 s doubling), then 401 `{"need":"key"}`; again with `"k":true`: waits up to 30 s for a touch of one of the account's keys (its signature over a new challenge checked with the key's public key, its counter, user presence), then `{"pk","fk","pv"?,"req","fks"?}`: the password's key and the file key as hex (as `desktop/session`), an administrator's private key (PKCS#8, base64); `fks`: a new box the page keeps when its own was lost while keys were not required. A key that is not the account's: 403. |
| `POST desktop/keys/add` | (1.6.1) Body `{"n","p","name","pin"?,"k"?}`: a new key (up to 5), touched twice (its credential, then its hmac-secret); a key with a PIN: 401 `{"need":"pin"}` (the PIN is used once, never kept); while keys are required, first `{"need":"key"}` and a touch of one of the account's keys. The first key's answer has `fks`, the box the page keeps in the account. Keys without hmac-secret (U2F only): 409. |
| `POST desktop/keys/remove` | (1.6.1) Body `{"n","p","i"}`. Not below two while a key is required (409). |
| `POST desktop/keys/require` | (1.6.1) Body `{"n","p","on","k"}`: on needs two keys and a touch, and checks the whole way (the key, the page's box, the file key) before the password's copy of the key secret is deleted; the page then deletes its own copies (`fk`, `priv`). Off: a touch, then the page makes them again. |
| `POST desktop/keys/reset` | (1.6.1) Body `{"n","p","acct","k"?}`: an administrator removes another account's keys (before resetting its password with the administrator's copy of its file key); the administrator's own key is touched when it has keys. |
| `GET desktop/session` | For the one-app pages (desktop/oneapp59.js): `{"id","n","r","pk","fk"}`, the account and its two keys as hex (the password's key and the file key: what the page itself derives at sign-in), so `index.html#app=NAME` signs in without the password. 404 without a session, 423 while locked. Kept root-only in /run/rudolf/native-session. |
| `GET events/wait?shell=1` | (1.6.0) On the native desktop (/run/rudolf/wm-native) rudolfd's events (the power key, the keys' indicator `osd …`, `taskmgr`, the charger, drives, Wi-Fi, the memory guard) are the native shell's: only a request with `shell=1` takes them; any other gets none (`{"events":"","native":true}`, after 20 s for `events/wait`), and the page stops asking. The shell shows the power dialog, the indicator and Task Manager itself and gives the page of one app window (the one in front) the rest. |
| `GET desktop/settings` | What the shell draws, for the session's account (`bd95set:ID`), before sign-in the last settings (`bd95set`): `{"gen","acct","s":{…},"pins":[…],"notes":N,"wallpaper":bool,"wpacct"}`. `s` has, as the page keeps them: bs, b1, b2, dyn, dm, lat, wpimg, wpfit (background); tb, tbstyle, tbac, tbacm, tbbstyle, tbbac, tbbacm (taskbar); twstyle, twacm (title bars); accent, darkui, ck, h24, df, tz, doff, ff, fs, lang, scale, icons, icstyle. `wallpaper`: a picture of your own is there for the shell, `wpacct` the account to ask `desktop/wallpaper` for (before sign-in the last account, when the last settings show its picture). `pins`: the pinned apps (`bd95pins:ID`); `notes`: how many notifications the page keeps (`bd95notif:ID`); `gen` changes whenever the page's storage does. |
| `GET desktop/apps[?acct=ID]` | (1.6.0: with `acct`, that account's last catalogue, else the last one) `{"apps":[{"k","name","icon","kw","ic"}],"bg":{KEY:{"n","d","l"}},"hide":[…],"icons":{IC:"data:image/png;base64,…"},"desk":[{"n","k","kind","ic","icon"}],"cells":{NAME:[C,R]},"is","ver","acct"}`: the start menu's list (`ic`: the app's picture in `icons`, drawn by the page in its icon style, 48 px; `desk`: the desktop's icons the account sees, `cells` their places on the page's grid and `is` the icon size, 1.6.0), the drawn backgrounds' dark and light pictures (files beside the page, www/a/) and the account's hidden apps, as the page wrote them. |
| `POST desktop/apps` | The page, after each sign-in (any engine): that JSON, at most 1 MB. Kept in /home/rudolf/.rudolf-apps.json, and (1.6.0) as its account's (`acct`) in /home/rudolf/.rudolf-apps/ID.json. |
| `GET desktop/cells?acct=ID` | (1.6.0) Where the native shell put the desktop's icons: `{"NAME":[COLUMN,ROW]}`; `{}` until it moved one (the page's own places come in `desktop/apps` `cells`). |
| `POST desktop/cells?acct=ID` | The shell, when an icon is moved or arranged: that JSON, at most 64 KB. Kept in /home/rudolf/.rudolf-cells/ID.json. |
| `POST desktop/widgets?acct=ID` | (1.6.0) The native shell's desktop widgets: the page's list (`[{"t","x","y","txt"}]`), at most 256 KB, kept in /home/rudolf/.rudolf-widgets/ID.json with the page's own value (bd95widgets:ID) it replaced; `GET` gives that file. `desktop/settings` answers `widgets` (this list while the page's own is still the one it replaced, `wgown`: true; else the page's), `wloc` and `wunit` (the weather's place and unit, bd95wloc and bd95wunit). (1.6.0, item 3) and `cal`: the Calendar's events (bd95cal:ID, `[{"d","t","title"}]`) for the calendar under the clock, and `notif`: the notifications' list (bd95notif:ID) for their centre; `s` also has quick settings' switches nl, dnd, sr, mag, cb, vk, touch, (1.6.0) and Night light's strength and hours and the magnifier's zoom (nlv, nlsched, magz), which the native shell applies to the whole screen. |
| `GET desktop/wallpaper?acct=ID` | A picture of your own (JPEG), as the page gave it; when the page has not given it yet (no page has loaded it, as at the first start of Native) or gave another picture than the settings show, rudolfd makes it from the page's database mirror (idbm: the record `wp:PICTURE`); 404 when there is none. |
| `POST desktop/wallpaper?acct=ID[&img=PICTURE]` | The page, when it loads or changes a picture of your own (it lives in the page's IndexedDB): the JPEG it made of it, at most 16 MB; an empty body removes it. Kept in /home/rudolf/.rudolf-wallpaper/ID.jpg (and which picture it is in ID.img). |
| `GET desktop/dropped?path=PATH` | (1.6.1, plan-1.6.1 Part 13) A file dropped from a Linux app onto the native desktop, for the page that keeps it (oneapp59.js `oaXdrop`): the file itself (at most 64 MB), read by the signed-in account's own Linux user, every step of the path opened without following a link. Only the account's own folder (under /home/acctUID, /persist/accounts/ID or Debian's /home/rudolf) and the drives (/media); Flatpak's document portal (/run/user/UID/doc/…, /run/flatpak/doc/… in a sandbox) is mapped to the real file it names (its `user.document-portal.host-path`), which must be one of those. Else `{"ok":false,"error":WHY}`: `nobody` (409; also the guest), `path` (400: not absolute, `.` or `..`, or too long to be whole), `outside`, `link`, `folder`, `unreadable` (403), `gone` (404), `too-large` (413). rudolfd-drop.inc. |

`desktop/engine` and `desktop/priority` are older (1.5.7: Settings' desktop engine; Task Manager's priority).

## The website data's key (1.6.1, docs/plan-1.6.1.md Part 20; rudolfd-webkey.inc)
| Request | What it does |
|---|---|
| `GET webkey` | What keeps the shared browser profile's website data at this start (written by the session's webkey_start): `{"state","why","t"}`, state `tpm` (a key sealed in this computer's TPM; `why`: why it is a new one, if it is), `none` (no TPM 2.0: Chromium's built-in key), `failed` (the TPM would not give it at this start; `why`), `off` (not started: an older session, or Lite). Never the key. docs/website-data-key.md. |

## ZIP files kept on the disk (1.6.1; rudolfd-dlzip.inc, dlzip.sh)
A download larger than 1.5 MB stays in the account's folder (/home/rudolf/Downloads/.accounts/ACCT/NAME) and Downloads holds
a reference to it (dl27.js). For such a ZIP (dlzip61.js): `name` is its name on the disk (no `/`, not hidden, no control
characters), `acct` the account's folder (`dl_acct_ok`); the file must be a plain file there, its real path the account's
folder's (no link on the way).

| Request | What it does |
|---|---|
| `GET dl/zip?name=&acct=` | What is inside, read with `unzip -l` as the desktop user (never as root): `{"ok","items":[{"name","size","dir"}],"more","total","bytes"}`, at most 20,000 items (`more`: there are others; `total` and `bytes` count them all). Not a ZIP: 422. |
| `POST dl/unzip?name=&acct=` | Extract here: the job `dl-unzip` (`GET job`), dlzip.sh with the paths in a root-only file. It checks the free space for the unpacked size, unpacks as the desktop user into a hidden folder in the account's folder, refuses entries with `..` or an absolute path, removes links, then moves each file flat into the account's folder under a free name ("name (2).ext"). Its log: `total N BYTES`, `progress I N PATH`, `dir PATH`, `file PATH<tab>NAME<tab>SIZE`, then `done N BYTES`, `stopped N BYTES` or `error MESSAGE`. 409 while another job runs. |
| `POST dl/unzip?stop=1` | Cancel: the job stops before the next file (what was moved stays, and is in its result). |
| `GET dl/unzipped` | The last extraction's whole result (the job's log is only its end): `{"ok","running","acct","dirs":[PATH],"files":[{"path","disk","size"}],"end"}`. |
| `POST dl/unzipped` | The page has saved its references: the result is forgotten. |

While an extraction runs, or its result has not been taken (at most 15 minutes), `GET dl/home?acct=` for that account says
`"unzip":true`, and the page takes no file of that folder in as a download of its own (dl27.js `dlAdopt`, in every page).

## Game mode (1.6.1; rudolfd-game.inc, /usr/lib/rudolf/gamemode)
Root keeps game mode (docs/plan-1.6.1.md Part 19): the processor's energy preference raised (the power settings' `game=1`)
while any source is on, and the desktop's group slowed (`cpu.weight` 20) behind a Linux game; all put back when none is.

| Request | What it does |
|---|---|
| `POST game/on?src=page\|native&kind=page\|linux\|oracle[&pid=N]` | A full screen the desktop sees (Standard's page; the native desktop's window manager, with the window's `_NET_WM_PID`), said again every 2 s while it lasts: it counts 6 s at most, and not once its process (`pid`) is gone. The answer is `game/status`'s. |
| `POST game/off?src=page\|native` | That source's full screen is over. |
| `GET game/status` | `{"ok","enabled","on","throttled","sources":[{"src","kind","pid"}],"games":[{"pid","name"}],"cpu"}`: `games` asked through GameMode's D-Bus API (rudolf-portal's request files in /run/rudolf/game-req), `throttled` the desktop slowed. |
| `POST game/set?enabled=0\|1` | Settings › System health › Memory › Game mode (kept in /persist/etc/gamemode). |

## Duels on the local network (1.6.1; rudolfd-duel.inc, rudolf-duel)
For the Library's games (Ember Reach 0.5.2's LAN duels; rudolfos-library docs/duel-protocol.md). The desktop asks for these
on a game's behalf (duel61.js: the game kit's lanHost, lanFind and lanStop), after asking the person before hosting.

| Request | What it does |
|---|---|
| `POST duel/host?ver=VER` | The duel service (`rudolf-duel serve`) started for one match: as the signed-in account's own Linux user (never root), on the first free port of 47860-47869, with a new room code; the firewall opened for that port to the local network only (`firewall duel PORT`) while it runs, and closed again by rudolfd's keeper when it ends. Hosting again stops the last. `{"ok","url":"ws://127.0.0.1:PORT/","code","port"}` |
| `POST duel/stop` | The service stopped (its keeper closes the firewall); nothing hosted: nothing to do. |
| `GET duel/find` | The duels hosted on the local network (`rudolf-duel find`, as the account's user: multicast DNS, `_emberduel._tcp`): `{"ok","games":[{"name","addr","port","url","code","ver"}]}` |
| `GET duel` | `{"ok","hosting","port","code"}` |

## Graphics driver (1.6.1; rudolfd-gpu.inc, /usr/lib/rudolf/gpudriver)
Settings › Display › Graphics driver (docs/plan-1.6.1.md Part 19): the open driver (nouveau, NVK) or NVIDIA's, from the system
extension `nvidia-driver`.

| Request | What it does |
|---|---|
| `GET gpu/driver` | `{"ok","card":"10de:ID"\|null,"name","offered","why","extension":{"installed","version","state"},"choice":"open"\|"nvidia","inUse","version","fellBack":{"at","why"}}`: `fellBack` while the note of a switch back by itself has not been shown. |
| `POST gpu/driver/set?to=open\|nvidia` | The choice for the next start; the body `{"n":ADMIN,"p":PASSWORD}` (rudolfd checks it). `nvidia` is refused (`{"ok":false,"error"}`) for a card that is not offered. |
| `POST gpu/driver/ack` | The switch back was shown (the desktop's notification): its note is forgotten. |
| `GET gpu/driver/licence` | NVIDIA's licence (text), from the installed extension; 404 without it. |
