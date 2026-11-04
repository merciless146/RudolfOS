# The Rudolf OS Library: a catalog of apps made for Rudolf OS (planned for 1.5.2)

Apps made for Rudolf OS that not everyone needs move out of the system into an online catalog, from which the
App Store downloads them, like Flathub for Linux apps. Apps made for Rudolf OS later go there too.

## What moves, what stays

| | Size (uncompressed) | 1.5.2 |
|---|---|---|
| Quake (LibreQuake data and the engine's source) | 27 MB | **Library** |
| Emu (Nintendo DS and SNES emulators: DeSmuME, LakeSnes) | 2.6 MB | **Library** |
| The desktop and its built-in apps (WOS Galaxy Map among them) | 4.4 MB | stays |
| PDF viewer (pdf.js) | 3 MB | stays |
| Japanese input dictionary | 5.2 MB | stays |
| Luau | 4.1 MB | stays |

The desktop layer gets about 30 MB smaller uncompressed (roughly half; about 12 MB less to download with every
desktop update). Sober is not part of Rudolf OS: it is a Flatpak from Flathub, installed from the Games tab, and stays
there.

Only the large data and engines move. The few kilobytes of each app's own desktop code stay, so Quake and Emu stay in
the Start menu and the Games tab; opened without their package, they offer to download it.

## How it works: rpk, which Rudolf OS already has

`rpk` (iso/sys/rpk.c) is Rudolf OS's package manager since 1.3.0:
- a package is a read-only squashfs image with an Ed25519 signature; installed packages are mounted at /pkg/<name>,
  so installing never changes the system and removing one deletes one file;
- kinds: `webapp` (www/ served by rudolfd at /pkg/<name>/, opened in a desktop window), `addon` (files the desktop
  uses), `app`, `cli`;
- a repository is static files: index.txt (one package per line) and index.sig, plus the packages;
- sources: the disc's own repository and lines in /persist/pkg/sources.

The Library is an rpk repository on the internet, added to every computer's sources. Packages:
- `rudolf-quake` (addon): the files now in www/rudolfq, served at /pkg/rudolf-quake/www/;
- `rudolf-emu` (addon): the emulators now in www/lib/emu.

The desktop looks for them at /pkg/<name>/ instead of in its own folder.

## Where it lives: a GitHub repository, yes, a separate one

**A GitHub repository is the right home**, and a separate one, `merciless146/rudolfos-library`, is better than a
folder in RudolfOS.

Why GitHub:
- **Hosting at no cost, and no server to run.** The packages go in GitHub Releases (files up to 2 GB each), and the
  signed index in GitHub Pages. Rudolf OS's own updates already work this way, so nothing new needs to be learned or
  paid for.
- **The host does not need to be trusted.** rpk checks every package's signature against keys built into Rudolf OS,
  so a database server or anything more would add nothing.
- **A build workflow (Actions)** packs, signs and publishes each app, and keeps the history of every version.

Why separate from RudolfOS:
- **Its own schedule.** A new version of an app is published by itself, without a Rudolf OS release, and every
  computer gets it through the App Store's updates.
- **Its own key.** See below: if the Library's key were ever stolen, it could not sign a system update.
- **Room for others.** People who make apps for Rudolf OS can later be given access to the Library repository
  without any access to the operating system or its keys.
- **A clean RudolfOS repository**, whose gh-pages branch holds only the system's update channels.

Layout of rudolfos-library:
```
apps/<name>/PKGINFO           name, version, kind, title, description, icon, licence, category
apps/<name>/www/...           the app's files (or a fetch script for large data, pinned by SHA-256)
.github/workflows/publish.yml  build each changed app, sign it, upload it to a release, update the signed index
```
Published: `https://merciless146.github.io/rudolfos-library/index.txt`, `index.sig` and `catalog.json` (titles,
icons, descriptions, sizes, for the App Store), with the packages next to them (`<name>.rpk`). (As built: rpk takes
each package's file from the same place as the list, so the packages are on Pages too, not in Releases.)

## Keys: the Library gets its own

Today /etc/rudolf/rpk-keys is trusted by both rpk (packages) and rudolf-update (the system's updates). A key added
there for the Library could also sign a system update. So:
- a new Ed25519 key pair for the Library, made by you with the existing tool (as the release key was);
- its private key only in the rudolfos-library repository's secret `RPK_LIBRARY_KEY`, and never in a file;
- its public key in a new file, /etc/rudolf/library-keys, which **rpk** trusts and **rudolf-update does not**.

The release key keeps signing system updates only.

## The App Store and the desktop

- A **Rudolf OS Library** tab in the App Store: each app with its icon, size, description and licence, and
  Get / Open / Remove / Update.
- Opening Quake or Emu without the package: "Quake is in the Rudolf OS Library (13 MB). Download it?", and it opens
  when the download finishes.
- **Nobody loses an app:** on the first start of 1.5.2, a computer whose Games folder or saves show it used Quake or
  Emu gets that package installed by itself (in the background, when online), with a notification.
- **Offline:** the disc image's own repository carries both packages, so an install from the disc or a live session
  can add them without internet. The download of the system shrinks, and the disc stays the same size.
- **Updates:** the App Store's Updates also checks the Library; the health log records installs and updates.

## Steps

1. You make the rudolfos-library repository and the Library key pair, and add `RPK_LIBRARY_KEY` to its secrets. I
   never handle the private key.
2. rudolfos-library: the layout, the publish workflow, the two packages; first publish.
3. Rudolf OS 1.5.2:
   - /etc/rudolf/library-keys, rpk trusting it, and the Library added to sources;
   - Quake and Emu reading their files from /pkg/;
   - the Library tab;
   - "download it?" when an app is missing;
   - reinstalling for computers that used them;
   - the disc repository carrying both packages;
   - www/rudolfq and www/lib/emu removed from the desktop layer.
4. Tests:
   - in the browser: the Library tab, download prompts, reinstall on update;
   - in QEMU: install from the Library over the network and from the disc, Quake and Emu running from /pkg/, removal;
   - a system update signed with the Library key is refused.
