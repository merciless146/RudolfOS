#!/bin/bash
# Bundle Fengari (Lua 5.3 in JavaScript) into www/lib/fengari-web.js, so Lua (Rudolf Prompt, BlockLua Builder, and
# Blitzen's code checks) runs without the internet. The page used to load it from a CDN only.
#   fengari          MIT    github.com/fengari-lua/fengari
#   fengari-interop  MIT    github.com/fengari-lua/fengari-interop
#   sprintf.js       BSD-3  github.com/alexei/sprintf.js
# Needs esbuild (npm). GPL-2.0-or-later (this script).
set -e
I=$(cd "$(dirname "$0")/.." && pwd); S=$I/src/fengari-web; N=$S/node_modules; mkdir -p $N
g() { [ -d $N/$1 ] || git clone -q --depth 1 ${3:+--branch $3} https://github.com/$2 $N/$1; }
g fengari fengari-lua/fengari; g fengari-interop fengari-lua/fengari-interop; g sprintf-js alexei/sprintf.js
# Node-only modules fengari asks for in code paths a browser never takes
for m in tmp readline-sync; do mkdir -p $N/$m; echo 'module.exports={};' > $N/$m/index.js; done
cp $I/systools/fengari-entry.js $S/entry.js
ESB=$(command -v esbuild || echo /opt/npm-tools/node_modules/esbuild/bin/esbuild)
$ESB $S/entry.js --bundle --minify --format=iife --platform=browser --define:process.env.FENGARICONF=undefined \
	--external:fs --external:child_process --external:path --external:os --external:crypto --outfile=$I/www/lib/fengari-web.js
{ echo "Fengari (Lua 5.3 in JavaScript), fengari-interop and sprintf.js, bundled into lib/fengari-web.js by iso/systools/mkfengari.sh"
  for p in fengari fengari-interop sprintf-js; do echo; echo "===== $p"; cat $N/$p/LICENSE*; done; } > $I/www/lib/fengari-LICENSE.txt
ls -la $I/www/lib/fengari-web.js
