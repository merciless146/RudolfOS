# Making your own Rudolf OS

Rudolf OS is free software (GPL-2.0-or-later): you may fork it, change it and build it yourself. This page explains
how, with your own keys, your own update address and your own Library. It also covers how a computer goes back to the
official Rudolf OS. The commands in the `sh ci` blocks are run in GitHub Actions on every change to this page or to
the tools it uses (`iso/qt/host/custombuild_t.sh`), so they stay correct.

## 1. Fork and build

1. Fork `merciless146/RudolfOS` on GitHub.
2. Turn on GitHub Pages for your fork (Settings › Pages › Deploy from a branch › `gh-pages`). Your update addresses
   are then `https://YOU.github.io/RudolfOS/beta` and `https://YOU.github.io/RudolfOS/stable`.
3. Add the two secrets from section 2 (Settings › Secrets and variables › Actions › New repository secret).
4. Run the workflow **Rudolf OS release** (Actions › Rudolf OS release › Run workflow). It builds a beta from your
   fork's `main` and publishes it to your `beta/` channel. With `promote` set to a beta's serial, it copies that beta
   to `stable/`. It also runs by itself every 6 hours (only when something changed).
5. The disc image comes from the workflow **Disc image (whole)**.

## 2. Keys

A Rudolf OS build needs two keys of its own. **Only their public halves ever go into the repository.**

**The release key** (Ed25519) signs the release lists that computers check before installing an update. Make it on
your own computer, outside the repository:

```sh ci
python3 iso/pkgtools/rpk-tool.py keygen "$KEYS"
```

This writes `rpk-signing.key` (private) and `rpk-signing.pub` (public) into `$KEYS`.
- Replace the line in `iso/trusted-keys.txt` with the public key (the `.pub` file's line), so your builds trust your
  releases and not the official ones.
- Put the **whole private key file** into the secret `RPK_SIGNING_KEY`. The workflow checks it matches
  `trusted-keys.txt`:

```sh ci
python3 iso/systools/keycheck.py "$KEYS/rpk-signing.key" iso/trusted-keys.txt
```

**The Secure Boot key** (RSA-2048 with a certificate) signs the start-up files, so computers with Secure Boot on
start your build once the key is enrolled:

```sh ci
(cd "$KEYS" && sh "$REPO/iso/systools/mksbkey.sh" >/dev/null)
```

- `rudolf-sb.crt` (public) replaces `iso/boot/sb/rudolf-sb.crt`.
- The whole `rudolf-sb.key` goes into the secret `RUDOLF_SB_KEY`.

**Keeping them safe:**
- Keep both private keys offline with a backup (a USB stick in a drawer, a password manager's file storage).
- Never commit them. `keys/`, `*.pem` and `*.key` are in `.gitignore`; check before every commit with `git status`.
- **If a key leaks:** make a new one and put its public half in the repository. Release once with **both** public
  keys in `trusted-keys.txt` (so computers accept the release that brings the new key), then remove the old one.

## 3. Your own Library

The Rudolf OS Library (`merciless146/rudolfos-library`) works the same way:
- Fork it and make its key with `rpk-tool.py keygen`. The private key goes in the secret `RPK_LIBRARY_KEY`.
- Packages are built and the index signed by its workflow:

```sh ci
python3 iso/pkgtools/rpk-tool.py build --key "$KEYS/rpk-signing.key" "$PKG" "$OUT"
```

- A computer uses another Library when an administrator adds it to `/persist/pkg/sources`, and its key in Developer
  mode (section 4).

## 4. Installing a custom build

- **On your own computer, from your disc:** start your disc image. With Secure Boot on, enroll your certificate first
  (MokManager, see `docs/secure-boot.md`), or turn Secure Boot off.
- **On an official Rudolf OS, by update:** Settings › Update and recovery › Developer mode › Turn on. Give your release
  key's public half (the 44 characters in `rpk-signing.pub`) and your update address (`https://YOU.github.io/RudolfOS/beta`).
  An administrator confirms with a typed sentence and their password. Rudolf OS then also installs releases signed by
  your key:

```sh ci
"$RU" developer on "$(cat "$KEYS/rpk-signing.pub")" https://YOU.github.io/RudolfOS/beta
"$RU" verify-manifest "$REL/channel/manifest.txt" "$REL/channel/manifest.sig"
```

What changes:
- Settings › About says **Custom build (signed by key …)** instead of Official Rudolf OS.
- The official recovery system stays official: Recovery mode in the boot menu always starts the official Rudolf OS.

## 5. Going back

- **Settings › Update and recovery › Return to official Rudolf OS:** Developer mode off. The official Rudolf OS starts
  at the next restart, keeping accounts, files and apps or erasing everything (your choice). Your build is removed
  once the official one has started.
- **Recovery mode** (the boot menu): the official Rudolf OS, with the same button.
- **A rescue stick:** Make a stick on another Rudolf OS computer, or the official boot image from GitHub Releases.

## 6. The name

A custom build must not call itself the official Rudolf OS:
- Give it a name of your own in `iso/VERSION`'s notes and the desktop's About page (for example "Rudolf OS, Alex's
  build").
- Use your own update addresses.
- The official recovery system, the official update addresses (`merciless146.github.io/RudolfOS`) and the official
  Library belong to the project.

## 7. Publishing games for the official Library

Until developer certificates exist (docs/plan-1.5.9.md 8.2), a game is published through a pull request to
`merciless146/rudolfos-library`, reviewed before it is listed. The Library's own workflow signs it.

- **The sandbox.** Library web apps run in a sandbox: no access to the desktop, its storage or the system. They ask
  only for what they need in their `PKGINFO` (`permissions = saves gamepad fullscreen`), and the person
  allows it when first opening them.
- **Their API.** The Library's game kit (`lib/gamekit/gamekit.js` in rudolfos-library, the `RG` global) talks to the
  desktop: `RG.load()` and `RG.save(value)` keep saves for the account, and the desktop pauses the game when its window
  is hidden. Full screen and game controllers are the browser's own.
- **The rules** are in docs/plan-games.md.
