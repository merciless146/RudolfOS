#!/bin/sh
# rudolf-devlink (1.4.0) on the host against a stand-in for GitHub, then (1.5.9, plan 15.4) rudolfd's side of it:
# sh devlink_t.sh   (needs gcc, libcrypto, libcurl; the second part needs root and node, and is skipped without them)
H=$(dirname "$0"); python3 "$H/devlink_t.py"; a=$?
sh "$H/devlink59_t.sh"; b=$?
[ $a = 0 ] && [ $b = 0 ]
