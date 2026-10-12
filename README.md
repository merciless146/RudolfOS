# Rudolf OS

A small desktop operating system for x86-64 PCs that starts from a CD or USB stick, built around one browser
engine. Version 1.1 is the first public release. Download the disc image from
[Releases](../../releases) (tag `disc-1.1`); `iso/README.TXT` explains how to start and install it.

| Folder | What is in it |
| --- | --- |
| `desktop/` | Source of the desktop (`build19.py` joins the modules into one page) |
| `iso/www/` | The built desktop that goes on the disc |
| `iso/` | Everything that makes the disc image: `build_iso.sh`, start-up, system programs (`sys/`), packages (`pkgs/`, `pkgtools/`) |
| `iso/systools/` | Release tools for automatic security updates (see `iso/systools/RELEASES.md`) |
| `.github/workflows/` | Builds signed security releases every 6 hours and publishes them on GitHub Pages |

Automatic updates: installed computers check `https://merciless146.github.io/RudolfOS/stable` once a day and
accept only releases signed by the key in `iso/trusted-keys.txt`.

Rudolf OS is free software under the GNU General Public License, version 2 or (at your option) any later
version. See `LICENSE`.
