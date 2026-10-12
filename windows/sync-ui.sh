#!/bin/bash
# Copies the files the Windows page shares with Rudolf OS into windows/ui (they are not kept twice in the repository).
# Run it before building the app or the Stage 1 zip: bash windows/sync-ui.sh
set -e
cd "$(dirname "$0")/.."
cp desktop/blitzpacks26.js desktop/blk2lua26.js iso/www/lib/fengari-web.js iso/www/lib/fengari-LICENSE.txt windows/ui/
echo "windows/ui is ready"
