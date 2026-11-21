# The website data's key in the TPM (Rudolf OS 1.6.1)

docs/plan-1.6.1.md Part 20, from 1.6.0 Part 22's "could come later". For Standard (and Native's Oracle), whose websites
live in the computer's one shared browser profile (/home/rudolf/.rudolf-browser) when "Keep my websites encrypted with
my account" is off.

## What it changes
Chromium encrypts the cookies that keep you signed in to websites, the passwords it saves for sites and payment cards
with a key of its own. Until 1.6.1 Rudolf OS ran it with `--password-store=basic`, where that key is built into
Chromium and the same on every computer: anyone with the drive (taken out, or the computer started from another stick)
could read those sign-ins as if they were plain.

Now, on a computer with a TPM 2.0 (the security chip; AMD's and Intel's processors have one in their firmware, which
the owner's Vivobook uses):
- at each start the session asks `rudolf-webkey unseal` (as root: only root may open /dev/tpmrm0) for the key's
  password. The first time it makes a 32-byte secret from the kernel's random numbers and has the TPM seal it: the
  sealed secret (/persist/etc/rudolf-webkey/sealed, root's 0600) can be opened only by this TPM, and only while PCR 7
  and PCR 14 hold what they held when it was sealed. Chromium's password is HKDF-SHA-256 of the secret;
- `rudolf-webkey wallet` gives it to Chromium as its key store (KWallet's D-Bus interface, on the desktop's session
  bus, `--password-store=kwallet6`): one entry, Chromium's own, read from memory; nothing written anywhere. Chromium then
  encrypts with a key made from it ("v11" values instead of "v10");
- before Chromium starts, `rudolf-webkey migrate` encrypts again everything still under the built-in key (cookies,
  saved site passwords, payment cards, sign-in tokens), so nothing old is left readable;
- every Chromium of that profile uses the same store: Standard's desktop, Native's Oracle, rudolf-shellfix (Chromium
  drops what it cannot read, so a Chromium with the other store would sign websites out).

Settings › Oracle › Website data says which way this start keeps them; System health says when a new key was made and
why, when there is no TPM and when the TPM would not give the key.

## Which measurements, and why
- **PCR 7**: Secure Boot's own state, measured by the firmware: on or off, its keys (PK, KEK, db, dbx), and the
  certificate each start-up file was checked with (Rudolf OS's, through shim).
- **PCR 14**: shim's MOK list (the keys enrolled with MokManager, Rudolf OS's among them).

So a start with Secure Boot turned off (or on), with other keys in the firmware, with another key enrolled in MOK, or from
something signed by another key gives other values and the TPM refuses the secret. Updates of Rudolf OS change neither:
every release is checked with the same certificate. Not PCR 0 to 6 (they change with every firmware or boot-loader
update and with the firmware's settings, which would sign websites out for nothing), not 11 or 12 (Rudolf OS's
start-up file measures nothing there).

## When it cannot be used
- **No TPM 2.0** (or it is turned off in the firmware's settings): Chromium's built-in key, as before.
- **Secure Boot's measurements changed** (Secure Boot turned off or on, its keys changed, a MOK key enrolled or
  removed, Microsoft's revocation list (dbx) updated by the firmware or another system), **or the TPM was cleared**: the
  sealed secret cannot be opened. A new one is made at once, and what Chromium kept under the old key cannot be read
  any more: websites ask you to sign in again, once. The old sealed file is kept beside the new one (sealed.old) for a
  look, not used.
- **The TPM would not** (its owner has a password set by another system, it is locked out, it failed): nothing is
  changed, and Chromium's built-in key is used for that start; what it cannot read then is dropped by Chromium.

## What it does not cover
- **The computer itself taken**, Secure Boot as it is: it starts Rudolf OS, which opens the key. The websites are then
  as safe as the computer's sign-in: someone who can sign in to any account (Standard's shared profile is everyone's),
  or who breaks into a running Rudolf OS, can use them. Per account and locked at sign-out is the other choice: "Keep my
  websites encrypted with my account".
- **Signed-in use**: anything running as the desktop's own user (uid 1000: the desktop's Chromium, its pages) can ask
  the key store, and Chromium itself holds the key while it runs. An account's own apps run as their own Linux users
  and cannot reach the desktop's session bus.
- **A discrete TPM chip's wires**: the secret travels to the processor unencrypted (no parameter encryption), so
  someone with tools on the motherboard's TPM bus while it starts could read it. Firmware TPMs (the Vivobook's) have no
  such bus.
- **Lite** (WebKit, no shared Chromium profile) and the RudolfWeb windows' own profile keep Chromium's built-in key.
- **Everything else in the profile** that Chromium does not encrypt itself (history, the cache, site storage) is as
  before: on RUDOLFDATA, not encrypted. Turn on "Keep my websites encrypted with my account" for those.

## Tests
iso/qt/host/webkey161_t.sh (against swtpm, with tpm2-tools as a second implementation and Rudolf's own Chromium),
iso/qt/shell2/webkey161_t.js (Settings' words), iso/qt/sb/wk161_t.py (QEMU with swtpm as a CRB TPM, on a release, with a
kernel that has the TPM's driver): .github/workflows/webkey-check.yml. The owner's check on the Vivobook: the key after
Secure Boot is turned off and on again (a new key, said once, websites sign in again), and after a firmware update
(the same key).
