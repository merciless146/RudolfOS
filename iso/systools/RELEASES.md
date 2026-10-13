# Rudolf OS automatic security releases

Installed Rudolf OS computers check a signed release list once a day, download only the changed parts of the
system into a spare slot on RUDOLFDATA and switch to it at the next restart. A release that fails to start
twice is dropped and the previous system starts instead.

## How a release is made

`ci/rudolf-release.yml` (GitHub Actions) runs every 6 hours:

1. Reads the latest release from the `beta` channel and downloads its five files.
2. `systools/chromium_fetch.sh` fetches the newest stable Chromium (Ubuntu's chromium snap by default,
   Playwright's build as the fallback). A new Chromium must render a page inside the real base layer, or the
   previous Chromium is kept.
3. Builds Linux 6.18.y when kernel.org has a newer point release (kernel + modules layers).
4. Rebuilds the desktop layer from `iso/www` (byte-identical, so skipped, when nothing changed).
5. `systools/make_release.py` writes a delta control file (`.rzs`) per file and the signed `manifest.txt`.
6. Publishes the files as GitHub release `r<serial>` and the manifest to `gh-pages/beta/`.

Promote a tested beta release: Actions > Rudolf OS release > Run workflow, promote = its serial. That copies
it to `gh-pages/stable/`, which is where computers look.

A new version (iso/VERSION and a new section at the top of CHANGELOG.md): Run workflow with disc = the version
(for example 1.1.1). After building the release it also publishes the disc image `disc-<version>`
(systools/mkdisc.sh): the kernel and system layers of the newest beta release on top of the earlier disc
(disc_from), whose packages, licences and sources are kept. The release notes come from CHANGELOG.md.

## One-time setup

1. Put this source tree in a GitHub repository with the folder `iso/` at the top level, and copy
   `iso/ci/rudolf-release.yml` to `.github/workflows/rudolf-release.yml`. Never commit a private key
   (`keys/`, `*.pem`, `*.key` are listed in `.gitignore`).
2. Signing key, on your own computer:
   ```
   openssl genpkey -algorithm ed25519 -out rudolf-signing.pem          # private: keep it offline and backed up
   openssl pkey -in rudolf-signing.pem -pubout -outform DER | tail -c 32 | base64   # public key
   openssl pkey -in rudolf-signing.pem -outform DER | tail -c 32 | base64           # value for the secret
   ```
   Put the public key in `iso/trusted-keys.txt` (computers trust only the keys listed there; the build copies
   it to `/etc/rudolf/rpk-keys`). Add the secret value (or the whole PEM file; the workflows convert it and
   check it against `iso/trusted-keys.txt`) as the repository secret `RPK_SIGNING_KEY`
   (Settings > Secrets and variables > Actions).
3. Settings > Pages: deploy from the `gh-pages` branch, folder `/ (root)`. The stable address is then
   `https://OWNER.github.io/REPO/stable`.
4. Build the disc image with that address and a serial:
   `UPDATE_URL=https://OWNER.github.io/REPO/stable SERIAL=$(date -u +%Y%m%d%H) ./build_iso.sh`
5. Create a GitHub release with the tag `disc-1.1` and attach the disc image (`rudolf-os-1.1.iso`, or its
   `.partNN` pieces, plus `rudolf-os-1.1.iso.sha256`).
6. Actions > Rudolf OS release > Run workflow (promote empty). The first run finds the channels empty, takes
   the system out of the disc image, signs it as release `r<serial>` and publishes it to both `beta` and
   `stable`. From then on every run builds only what changed.

Release lists are valid for 14 days. When nothing new has been released, each run re-signs a list that would
expire within 5 days (same serial, same files), so computers keep seeing "up to date".

Packages on the disc (RUDOLF/PKG) are accepted because they come with the system on the same read-only disc:
each one must match the SHA-256 in the disc's package list. Packages and package lists from any other source
must be signed by a trusted key: sign them with `python3 iso/pkgtools/rpk-tool.py` using the same key.

## Trying a release by hand

`systools/make_release.py` with `--out` pointing at any folder served over HTTP(S) with range requests
(`qt/rangesrv.py` for local tests), then set that folder's `channel/` as the update address.
