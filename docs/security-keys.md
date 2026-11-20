# Security keys that unlock your files (Rudolf OS 1.6.1)

docs/plan-1.6.1.md Part 18 (Part 17 items 1, 2, 4 and 6). What the keys protect, how they are kept, and what can and
cannot be done when keys are lost. The code: `iso/sys/rudolfd-fido.inc` (rudolfd), `desktop/fido61.js` (the page),
`iso/sys/rudolf-native.c` (Native's sign-in and lock screen).

## What a key does

- **Added (one or more, not required):** after your password, the sign-in screen and the lock screen ask you to touch
  one of your keys. Your files are still opened by your password alone (the password's own copy of your file key is
  still in your account), so a copy of your disk with your password still opens them.
- **Required (two keys at least: one is the spare):** the password's own copies are deleted. From then on your files
  (your home folder's encryption key, and an administrator's key that opens other accounts' copies) open only with
  **your password and one of your keys together**. A copy of the disk with the password opens nothing; a key without
  the password opens nothing.
- A code from an authenticator app guards the sign-in only (it is kept with your password): Settings says so.
- Not protected: someone who has the computer while you are signed in and it is unlocked (that is the lock screen's job),
  the encrypted folder of "Encrypt my data" (Debian, Linux and Windows programs, Downloads: it is shared by the computer's
  accounts and each account's copy of its key opens with that account's password), and old copies of the desktop's
  storage that its browser engine may keep on the disk until it compacts them.

## How it is kept

- rudolfd reads the keys itself (FIDO2 over USB, with libfido2), for Standard, Lite and Native alike: the desktop page
  cannot skip the key. Every touch is checked: the key signs a new random challenge (a recorded answer is refused), it
  must be touched, its counter must grow.
- Each key holds a secret that never leaves it. Asked with your account's salt (FIDO2's hmac-secret), it gives a value
  that, with nothing else, opens that key's copy of your account's **key secret**. Your file key is sealed with the key
  secret together with your **password's secret** (kept in your account, sealed with your password, so changing your
  password needs no key). AES-256-GCM, keys from HKDF-SHA-256.
- The keys' records are rudolfd's and root's only (`/persist/etc/fido/ID.json`); they hold no secret in the clear.
- A key's PIN is asked only when adding the key (it is used once and never kept); signing in needs a touch, not the PIN.
  This keeps the key's secret the same whether or not a PIN is set later.

## When keys are lost

| What you have | What happens |
| --- | --- |
| One of your keys and your password | Sign in as usual. Remove the lost key (Settings › User Settings › Sign-in options › Security keys) and add a new spare. |
| Your password, no key, keys **not required** | You cannot sign in (the key is asked for). An administrator resets your password (it removes your keys; your files stay when the administrators' copies are on). With no other administrator there is no way in from Rudolf OS yet, but nothing is lost: the password's copy of your file key is still in your account for a later recovery tool. |
| Your password, no key, keys **required**, "Administrators can open my files" **on** (the default) | An administrator resets your password (Settings › User Settings › All accounts › Reset password…): your keys are removed and your files open with the new password (the administrator's copy of your file key). Then add your keys again. |
| Your password, no key, keys **required**, administrators' copies **off** | **Nobody can open your files.** Not your password, not an administrator, not Rudolf OS's makers: that is what requiring a key with the administrators' copies off means. Settings warns before both are set. |
| An administrator who requires a key and lost their keys | Another administrator resets their password. If there is no other administrator, the administrator's own files follow the rows above, and the copies of other accounts' file keys that only this administrator had are lost with them (those accounts still open with their own passwords and keys). |
| Your keys, no password | An administrator resets your password: your keys are removed too (they are tied to the password's secret), and you add them again. |

There is no recovery code yet (Part 17 planned one: 24 letters and digits, one more copy of the file key); until it
comes, keep the spare somewhere safe, and leave "Administrators can open my files" on unless you mean the last row.

## For the owner's checks

The tests use simulated keys (iso/qt/host/vfido.py: on the host through libfido2, and in QEMU as real HID devices with
uhid). A real key (YubiKey 5, SoloKey, Nitrokey 3, Google Titan v2 or another FIDO2 key with hmac-secret) is checked on
the Vivobook: see the pull request's checklist.
