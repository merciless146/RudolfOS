#!/bin/sh
# Rudolf OS 1.6.0 (docs/plan-1.6.0.md Part 13): the base layer's ABI, for signed system extensions: a SHA-256 over the
# base's shared libraries (each one's path and contents, in a fixed order), so an extension built against one base is
# used only on it (PKGINFO "base = ABI"; rpk refuses one for another base, and /usr/lib/rudolf/sysext leaves it out).
# Written into the base as usr/share/rudolf/base-abi by ci_release.sh; extensions.yml reads it from the release.
#   sh baseabi.sh ROOT        prints the ABI (64 hex characters)
# GPL-2.0-or-later.
R=${1:?usage: baseabi.sh ROOT}
cd "$R" || exit 1
find lib lib64 usr/lib usr/lib64 -type f \( -name '*.so' -o -name '*.so.*' \) 2>/dev/null | LC_ALL=C sort | xargs -r sha256sum | sha256sum | cut -c1-64
