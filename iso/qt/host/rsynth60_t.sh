#!/bin/sh
# 1.6.0 (the owner's Rudolf sounds, 2026-11-19): the page's renderer (desktop/sndrs60.js rsRender) and the native desktop's
# (iso/sys/rsynth.h rs_render, in rudolf-native.c) make the same samples for every sound: each sound's events from the
# page's recipes, rendered by both, compared sample by sample.   sh rsynth60_t.sh   (node, gcc)
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; P=0; F=0
cat > $T/c.c <<X
#include <stdio.h>
#include "$R/iso/sys/rsynth.h"
/* events from standard input: "G g0 g1 g2 g3 g4", then one a line: "K v0 v1 ..."; the samples out as raw floats */
int main(void) { double G[5]; RsEv E[64]; int n = 0; char k; if (scanf(" G %lf %lf %lf %lf %lf", &G[0], &G[1], &G[2], &G[3], &G[4]) != 5) return 2;
  char line[1024]; if (!fgets(line, sizeof line, stdin)) return 2;
  while (n < 64 && fgets(line, sizeof line, stdin)) { char *p = line; if (sscanf(p, " %c", &k) != 1) continue; E[n].k = k; E[n].nv = 0; p = strchr(p, k) + 1;
    char *e; while (E[n].nv < RS_MAXV) { double v = strtod(p, &e); if (e == p) break; E[n].v[E[n].nv++] = v; p = e; } n++; }
  size_t N; float *f = rs_render(E, n, G, &N); if (!f) return 1; fwrite(f, sizeof(float), N, stdout); free(f); return 0; }
X
gcc -O2 -Wall -o $T/c $T/c.c -lm || { echo "FAIL rsynth.h does not build"; exit 1; }
cat > $T/j.js <<X
const r=require('$R/desktop/sndrs60.js'),fs=require('fs'),cp=require('child_process');let bad=0,n=0;
for(const k of Object.keys(r.RS_TUNE.sounds)){const R=r.rsEvents(k),js=r.rsRender(R);
 const inp='G '+R.G.join(' ')+'\n'+R.E.map(e=>e[0]+' '+e.slice(1).join(' ')).join('\n')+'\n';
 const out=cp.execFileSync('$T/c',{input:inp,maxBuffer:64<<20}),c=new Float32Array(out.buffer,out.byteOffset,out.length/4);
 let md=0,pk=0;for(let i=0;i<Math.min(js.length,c.length);i++){md=Math.max(md,Math.abs(js[i]-c[i]));pk=Math.max(pk,Math.abs(js[i]))}
 const ok=js.length==c.length&&md<1e-4&&pk>.01;n++;if(!ok)bad++;
 console.log((ok?'PASS ':'FAIL ')+k+': '+js.length+' samples (C '+c.length+'), the largest difference '+md.toExponential(2)+', peak '+pk.toFixed(3))}
console.log((n-bad)+'/'+n+' passed');process.exit(bad?1:0)
X
node $T/j.js
