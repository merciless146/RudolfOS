#!/bin/bash
# Download the Secure Boot parts from the draft release "sb-parts" (workflow "Secure Boot parts") into DIR and
# check the shipped ones against iso/boot/sb/parts.txt (Rudolf OS 1.2.2). Needs gh with a token that can read
# draft releases (the workflow's own).   sb_parts_get.sh DIR
set -e
D=$1; [ -n "$D" ] || { echo "usage: $0 DIR"; exit 1; }
HERE=$(cd "$(dirname "$0")/.." && pwd); R=${GITHUB_REPOSITORY:-merciless146/RudolfOS}
mkdir -p "$D"
RID=$(gh api "repos/$R/releases?per_page=100" --jq '.[] | select(.name == "Secure Boot parts") | .id' | head -1)
[ -n "$RID" ] || { echo "::error title=Secure Boot parts::the draft release sb-parts is missing; run the workflow Secure Boot parts"; exit 1; }
gh api "repos/$R/releases/$RID/assets?per_page=100" --jq '.[] | "\(.id) \(.name)"' | while read -r id n; do
	case $n in *.efi|*.stub|licences.tar.gz|sb-parts.txt) gh api -H 'Accept: application/octet-stream' "repos/$R/releases/assets/$id" > "$D/$n";; esac
done
(cd "$D" && tar -xzf licences.tar.gz && grep -v '^#' "$HERE/boot/sb/parts.txt" | sha256sum -c --quiet) || { echo "::error title=Secure Boot parts::the files in sb-parts are not the pinned ones (iso/boot/sb/parts.txt)"; exit 1; }
echo "Secure Boot parts OK"; cat "$D/sb-parts.txt" | head -4
