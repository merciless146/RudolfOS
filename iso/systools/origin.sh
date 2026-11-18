#!/bin/sh
# /etc/rudolf/origin (1.6.0, docs/plan-1.6.0.md Part 19; read by iso/sys/origin.h and rudolfd's origin): where this build of
# Rudolf OS comes from. GITHUB_REPOSITORY and GITHUB_REPOSITORY_ID are GitHub Actions' own: a fork's workflow names the fork.
# Without them (a build on your own computer): the official repository, or ORIGIN_REPO / ORIGIN_REPO_ID if set.
R=${ORIGIN_REPO:-${GITHUB_REPOSITORY:-merciless146/RudolfOS}}
I=${ORIGIN_REPO_ID:-${GITHUB_REPOSITORY_ID:-}}
case "$R" in */*/*|/*|*/|*[!A-Za-z0-9._/-]*|*..*) echo "origin.sh: $R is not OWNER/NAME" >&2; exit 1;; */*) ;; *) echo "origin.sh: $R is not OWNER/NAME" >&2; exit 1;; esac
[ -z "$I" ] && [ "$R" = merciless146/RudolfOS ] && I=1414456577
case "$I" in *[!0-9]*) echo "origin.sh: the repository id must be a number" >&2; exit 1;; esac
O=$(echo "${R%%/*}" | tr 'A-Z' 'a-z')
echo "repo=$R"
[ -n "$I" ] && echo "repo_id=$I"
echo "pages=https://$O.github.io/${R#*/}"
