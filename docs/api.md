# rudolfd's API for the parts that draw the desktop

rudolfd (iso/sys/rudolfd.c) serves the desktop page on http://127.0.0.1:8080/ and an API under `/api/`. Every API
request carries the `X-Rudolf-Token` header: a random token made at each start and written only into the page rudolfd
serves (`window.RUDOLFD.token`), so websites cannot use the API. Since 1.5.9 the page and the API answer only the
desktop's own processes (its memory group, /sys/fs/cgroup/desktop) and root: any other program on the computer gets
403, even with the token (iso/sys/rudolfd-peer.inc). Replies are JSON with `"ok": true`, or `"ok": false`
and an `"error"` to show, with an HTTP status (400 a bad request, 403 refused, 404 not there, 409 not possible now,
423 locked, 429 wait, 500 failed).

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
| `GET desktop/accounts` | The sign-in screen's accounts: `{"setup", "guest", "last", "accounts":[{"id","n","r","key","old"}]}`. `r` admin or user; `key`: signs in with a security key (only Standard can); `old`: a password hash from before 1.4 (sign in once with Standard); `last`: the account used last; `guest`: the guest may sign in; `setup`: Rudolf OS has been set up. |
| `POST desktop/login` | Body `{"n":NAME,"p":PASSWORD}` or `{"guest":true}`. Checks the password as the page does (`p2$SALT$HASH`: PBKDF2-HMAC-SHA-256, 210 000 rounds), makes the password's key (PBKDF2 with the account's salt `ks`), opens the file key (`fk`, AES-256-GCM), and for an account whose files are on the disk opens its encrypted folder with the home folder's key (HKDF-SHA-512 of the file key, salt "rudolf-os home folder", info the account's id) and the session's binds, as `home/unlock?session=1`. Replies `{"id","n","r"}`. A wrong password: 403 and the health log ("signin"); five in a row: 429 for 15 s. |
| `POST desktop/logout` | Locks the account's folder (as `home/lock`) and ends the session. |
| `POST desktop/lock` | The lock screen is up: `desktop/session` answers 423 until unlocked. |
| `POST desktop/unlock` | Body `{"p":PASSWORD}`: the session's account's password (the guest: none). |
| `GET desktop/session` | For the one-app pages (desktop/oneapp59.js): `{"id","n","r","pk","fk"}`, the account and its two keys as hex (the password's key and the file key: what the page itself derives at sign-in), so `index.html#app=NAME` signs in without the password. 404 without a session, 423 while locked. Kept root-only in /run/rudolf/native-session. |
| `GET desktop/settings` | What the shell draws, for the session's account (`bd95set:ID`), before sign-in the last settings (`bd95set`): `{"gen","acct","s":{…},"pins":[…],"notes":N,"wallpaper":bool,"wpacct"}`. `s` has, as the page keeps them: bs, b1, b2, dyn, dm, lat, wpimg, wpfit (background); tb, tbstyle, tbac, tbacm, tbbstyle, tbbac, tbbacm (taskbar); twstyle, twacm (title bars); accent, darkui, ck, h24, df, tz, doff, ff, fs, lang, scale, icons, icstyle. `wallpaper`: a picture of your own is there for the shell, `wpacct` the account to ask `desktop/wallpaper` for (before sign-in the last account, when the last settings show its picture). `pins`: the pinned apps (`bd95pins:ID`); `notes`: how many notifications the page keeps (`bd95notif:ID`); `gen` changes whenever the page's storage does. |
| `GET desktop/apps` | `{"apps":[{"k","name","icon","kw","ic"}],"bg":{KEY:{"n","d","l"}},"hide":[…],"icons":{IC:"data:image/png;base64,…"},"desk":[{"n","k","kind","ic","icon"}],"cells":{NAME:[C,R]},"is","ver","acct"}`: the start menu's list (`ic`: the app's picture in `icons`, drawn by the page in its icon style, 48 px; `desk`: the desktop's icons the account sees, `cells` their places on the page's grid and `is` the icon size, 1.6.0), the drawn backgrounds' dark and light pictures (files beside the page, www/a/) and the account's hidden apps, as the page wrote them. |
| `POST desktop/apps` | The page, after each sign-in (any engine): that JSON, at most 1 MB. Kept in /home/rudolf/.rudolf-apps.json. |
| `GET desktop/cells?acct=ID` | (1.6.0) Where the native shell put the desktop's icons: `{"NAME":[COLUMN,ROW]}`; `{}` until it moved one (the page's own places come in `desktop/apps` `cells`). |
| `POST desktop/cells?acct=ID` | The shell, when an icon is moved or arranged: that JSON, at most 64 KB. Kept in /home/rudolf/.rudolf-cells/ID.json. |
| `POST desktop/widgets?acct=ID` | (1.6.0) The native shell's desktop widgets: the page's list (`[{"t","x","y","txt"}]`), at most 256 KB, kept in /home/rudolf/.rudolf-widgets/ID.json with the page's own value (bd95widgets:ID) it replaced; `GET` gives that file. `desktop/settings` answers `widgets` (this list while the page's own is still the one it replaced, `wgown`: true; else the page's), `wloc` and `wunit` (the weather's place and unit, bd95wloc and bd95wunit). |
| `GET desktop/wallpaper?acct=ID` | A picture of your own (JPEG), as the page gave it; when the page has not given it yet (no page has loaded it, as at the first start of Native) or gave another picture than the settings show, rudolfd makes it from the page's database mirror (idbm: the record `wp:PICTURE`); 404 when there is none. |
| `POST desktop/wallpaper?acct=ID[&img=PICTURE]` | The page, when it loads or changes a picture of your own (it lives in the page's IndexedDB): the JPEG it made of it, at most 16 MB; an empty body removes it. Kept in /home/rudolf/.rudolf-wallpaper/ID.jpg (and which picture it is in ID.img). |

`desktop/engine` and `desktop/priority` are older (1.5.7: Settings' desktop engine; Task Manager's priority).
