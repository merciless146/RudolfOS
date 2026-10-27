#!/bin/sh
# rudolf-devlink (1.4.0) on the host against a stand-in for GitHub: sh devlink_t.sh   (needs gcc, libcrypto, libcurl)
exec python3 "$(dirname "$0")/devlink_t.py"
