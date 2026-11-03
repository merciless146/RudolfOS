# Rudolf OS 1.5.0: security

1.4.x made the system hard to tamper with (Secure Boot with Rudolf's own key, signed modules, dm-verity on the
read-only system layers, signed A/B updates with fallback). 1.5.0 closes the gaps that are left, without growing the
system by more than a few megabytes, and adds remote access.

## 1. Firewall (incoming connections)

- Kernel: `NF_TABLES` with `NF_TABLES_INET`, `NFT_CT`, `NFT_REJECT`, `NFT_LOG` built in (checked in `kconfig.sh`).
- `nft` (and its libraries, about 1 MB) in the root file system.
- `/usr/lib/rudolf/firewall` loads the rules at start-up, before the network is up:
  - incoming: allow loopback, replies to connections the computer made (`ct state established,related`), DHCP
    replies, ICMP needed for the network to work (ping replies, too big, unreachable); count and drop the rest.
  - outgoing: everything is allowed.
  - remote access opens its port (8443) only while it is on, and only from the local network (private addresses).
- Settings › Security: the firewall on/off (administrator), how many incoming connections it blocked since start-up.
- The desktop's URL rules for Oracle tabs were called "Firewall"; they become **Website blocking** (they did not
  change).

## 2. System health log

The **Report a problem** dialog and its system report already exist (docs/system-reports.md). 1.5.0 adds a record of
what happened over time, kept on the computer:

- `/persist/var/log/health.log`, one JSON line per event, at most 512 KB (the older half is dropped when full).
- Events: start-up (how long it took, which slot), slot fallback after an update, memory guard closing a page, crash
  of the desktop or the browser, Debian program crash, sign-in failures, disk nearly full, high temperature, firewall
  blocks (counted per hour, not one line each), remote-access sessions (who, from which address, how long).
- Settings › System › **System health**: the events, newest first, and a summary (start-up time, crashes this week).
- The system report includes the last 50 events (account names and addresses left out, like the rest of the report).
- Nothing is sent anywhere. The permanent machine ID (`/persist/etc/machine-id`, for D-Bus) is never in the log or
  the report.

## 3. Administrator access to account files

Today each home folder is encrypted with a key made from its own password: nobody else can open it, and an
administrator resetting a forgotten password leaves the user with an empty home folder.

- Each account gets a random **file key** (AES-256-GCM). The home folder is encrypted with it.
- The file key is stored wrapped:
  - with a key made from the account's password (PBKDF2-SHA256, 210 000 rounds, as now);
  - for administrators: each administrator has an ECDH P-256 key pair; the private key is wrapped with the
    administrator's password; any account's file key is wrapped for every administrator's public key, so this works
    when the administrator is not signed in.
- **Administrators can open this account's files**: a switch per account (Settings › Accounts), **on by default**.
  Turned off, the administrator copies of that account's key are deleted.
- File Explorer: an administrator sees `C:\USERS\<name>` of the other accounts and can open them.
- An administrator resetting a password re-wraps the file key: the files stay.
- Existing homes move over at the account's next sign-in.
- Honesty: the welcome file and Settings say that administrators can open the account's files (or that they cannot,
  when the switch is off).
- Not covered: the Linux side (Debian, Wine) has one Linux home for every account, as before.

## 4. Data encryption

The system layers stay as they are (signed, verified, unencrypted): start-up, updates and fallback do not change.
Data is encrypted with the kernel's own file encryption (fscrypt on ext4, `FS_ENCRYPTION`, AES-256-XTS for contents
and AES-256-CTS for names): no extra partition, no cryptsetup.

- `/persist/vault` is an encrypted directory. Debian (`/persist/debian`), Flatpak apps (`/persist/flatpak`) and files
  kept in Downloads move into it.
- One random **data key** for the computer. It is wrapped for every account with that account's password (as the
  file keys above), and with a **recovery key** shown once when encryption is turned on (to write down or save).
- Unlock: the first sign-in with a password after start-up unlocks the vault (rudolfd adds the key to the kernel);
  Guest does not unlock it. Linux and Flatpak programs wait until it is unlocked ("Sign in to an account to use
  Linux programs").
- Settings › Security › **Encrypt my data**: administrator, shows the recovery key, moves the data over as a job with
  progress (needs free space as large as the largest folder that moves), then deletes the unencrypted copies.
- New installs format RUDOLFDATA with the `encrypt` feature; existing ones get it with `tune2fs -O encrypt`.
- Not encrypted: the browser profile (the desktop's own data in it is already encrypted per account), settings,
  logs.
- A USB stick with Rudolf OS unlocks on any computer with the password (no TPM binding in 1.5.0).

## 5. Remote access

A remote desktop in any web browser, served by Rudolf OS itself.

- `rudolf-remote` (new, C, OpenSSL by dlopen as `rudolf-devlink`): HTTPS on port 8443 with the computer's own
  certificate (EC P-256, made on first use), a viewer page, and a WebSocket that sends the screen (changed tiles,
  deflate) and takes keyboard and mouse input (XTest, as `rudolf-display`).
- Off by default. An administrator turns it on in Settings › Remote access, which shows the address
  (`https://<ip>:8443`) and a **pairing code** (6 digits, new every time it is turned on).
- Signing in from the viewer: the pairing code (once per browser, remembered by a cookie), then a Rudolf OS account
  and its password (checked against the same PBKDF2 hashes as the sign-in screen). Five wrong tries lock it for a
  minute.
- **Approval**: someone at the computer must Allow the session; a marker stays on screen while someone is
  connected, with a button to end it.
- **Unattended access**: per account, set by an administrator. That account can connect without approval, so the
  computer can be reached when no one is there.
- The firewall opens 8443 only while remote access is on, and only to the local network. Access from outside (WireGuard)
  is for a later version.
- Every session goes into the health log.

## Size

nft and libraries ~1 MB, rudolf-remote ~60 KB, viewer ~20 KB, desktop modules ~40 KB. fscrypt needs only the kernel.

## Tests

- `iso/qt/shell2/`: Settings pages, admin access to homes (wrap/unwrap, reset keeps files, switch off), Website blocking.
- `iso/qt/sb/t150*`: in QEMU: firewall blocks an incoming connection and allows replies; health log after start-up;
  encryption: turn on, restart, locked until sign-in, Debian works after unlock, recovery key unlocks; remote access:
  viewer signs in, approval, screen arrives, input works, firewall closes 8443 when off.
