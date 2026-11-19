#!/bin/sh
# 1.6.0 (the owner's Rudolf sounds, 2026-11-19): the native desktop's sounds (rudolf-native.c, its sound section built on its own
# with its JSON reader): the 18 categories, the styles from the page's catalogue (sndp, as oneapp59.js sends it: the Rudolf
# style's {w:'rs', G, E}), the account's choice of style, and a Rudolf style played at the page's length (rsRender's), from the
# catalogue and from a page's sfx (snd_page). Nothing is heard: PulseAudio is not opened here.   sh sndnative60_t.sh   (node, gcc, glib)
HERE=$(cd "$(dirname "$0")" && pwd); R=$(cd "$HERE/../../.." && pwd); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; P=0; F=0
ok() { if [ "$2" = 1 ]; then P=$((P+1)); echo "PASS $1"; else F=$((F+1)); echo "FAIL $1 ($3)"; fi; }
C=$R/iso/sys/rudolf-native.c
a=$(grep -n '^typedef struct JV { int t;' "$C" | cut -d: -f1); b=$(grep -n '^static int jbool(JV \*o' "$C" | cut -d: -f1)
s=$(grep -n "the desktop's own sounds (1.6.0, the owner's report 2)" "$C" | cut -d: -f1); e=$(grep -n '^static double snd_out(int i, double v, const Tone \*T, int n, const SndRs \*r)$' "$C" | cut -d: -f1)
e=$(awk -v s="$e" 'NR>s && /^}/ {print NR; exit}' "$C")
[ -n "$a" ] && [ -n "$b" ] && [ -n "$s" ] && [ -n "$e" ] || { echo "FAIL the sound section is not where this test looks"; exit 1; }
{ printf '#include <glib.h>\n#include <dlfcn.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <math.h>\n#include "%s/iso/sys/rsynth.h"\n' "$R"
  sed -n "${a},${b}p;${s},${e}p" "$C"
  cat <<'Y'
static char *slurp(const char *p) { gchar *t = NULL; g_file_get_contents(p, &t, NULL, NULL); return t; }
int main(int c, char **v) {
  (void)c; char *cat = slurp(v[1]), *set = slurp(v[2]), *pj = slurp(v[3]); JV *cv = jparse(cat), *sv = jparse(set); if (!cv || !sv) return 2;
  snd_cat(cv); snd_set(sv); printf("n %d\n", SNDN);
  for (int i = 0; i < SNDN; i++) { int rs = 0; for (int p = 0; p < SNDPC[i]; p++) if (SNDR[i][p].e) rs = p; printf("cat %s %d %d %d\n", SNDK[i], SNDPC[i], rs, SND_P[i]); }
  for (int i = 0; i < SNDN; i++) printf("play %s %.4f\n", SNDK[i], snd_play(SNDK[i]));
  snd_page("menu", .5, pj); printf("page menu %d\n", SND_PLAYED[snd_i("menu")]);
  snd_page("menu", .5, "{\"t\":[[0,0,1,\"sine\",0,{\"w\":\"rs\",\"G\":[1,2],\"E\":[[\"Z\",0]]}]]}"); printf("bad %d\n", SND_PLAYED[snd_i("menu")]);
  jfree(cv); jfree(sv); g_usleep(200000); return 0; }
Y
} > $T/s.c
gcc -O2 -Wall -Wno-unused-function -o $T/s $T/s.c $(pkg-config --cflags --libs glib-2.0) -ldl -lm 2> $T/cc.txt || { cat $T/cc.txt; echo "FAIL the sound section does not build on its own"; exit 1; }
ok "0. the sound section builds on its own without a warning" "$( [ ! -s $T/cc.txt ] && echo 1)" "$(head -3 $T/cc.txt)"
cat > $T/m.js <<Y
const r=require('$R/desktop/sndrs60.js'),fs=require('fs');
const OLD=['startup','login','error','click','open','close','notify','game','shutdown'];
/* the catalogue as oneapp59.js sends it: each category's styles; the older ones here two plain tones and the Rudolf style */
const oaTone=x=>x[5]&&x[5].w=='rs'?[x[0],x[1],x[2],x[3],x[4],{w:'rs',G:x[5].G,E:x[5].E}]:x.slice(0,5);
const sndp={};for(const k of OLD)sndp[k]=[[[440,0,.1]],[[660,0,.1,'square']],r.rsPat(k)].map(p=>p.map(oaTone));
for(const [k] of r.RS_NEW){const st=r.RS_STY[k];sndp[k]=[st.Classic,st.Retro,st.Candy,st.Minimal,r.rsPat(k)].map(p=>p.map(oaTone))}
fs.writeFileSync('$T/cat.json',JSON.stringify({apps:[],sndp}));
/* the account: the Rudolf style everywhere but the click (Classic) and the game (Retro: square) */
const snd={master:{on:true,vol:80}};for(const k of OLD)snd[k]={on:true,vol:70,p:2};for(const [k] of r.RS_NEW)snd[k]={on:true,vol:60,p:4};snd.click.p=0;snd.game.p=1;snd.shot.on=false;
fs.writeFileSync('$T/set.json',JSON.stringify({snd}));
fs.writeFileSync('$T/pj.json',JSON.stringify({t:r.rsPat('menu').map(oaTone)}));
const L={};for(const k of [...OLD,...r.RS_NEW.map(x=>x[0])])L[k]=r.rsRender(r.rsEvents(k)).length/r.RS_SR;fs.writeFileSync('$T/len.json',JSON.stringify(L));
Y
node $T/m.js || { echo "FAIL the page's catalogue"; exit 1; }
$T/s $T/cat.json $T/set.json $T/pj.json > $T/o.txt 2> $T/err.txt; echo "rc $?" >> $T/o.txt
ok "1. 18 categories" "$(grep -qx 'n 18' $T/o.txt && echo 1)" "$(head -1 $T/o.txt)"
ok "2. each category's styles from the catalogue, the Rudolf style read as events (the older ones' 3rd, the new ones' 5th)" \
  "$(awk '/^cat /{ if (($2 ~ /^(startup|login|error|click|open|close|notify|game|shutdown)$/ && $3==3 && $4==2) || ($2 !~ /^(startup|login|error|click|open|close|notify|game|shutdown)$/ && $3==5 && $4==4)) n++ } END { print (n==18) }' $T/o.txt)" "$(grep '^cat' $T/o.txt | tr '\n' ' ')"
ok "3. the account's styles (snd.p)" "$(grep -q '^cat click 3 2 0$' $T/o.txt && grep -q '^cat game 3 2 1$' $T/o.txt && grep -q '^cat menu 5 4 4$' $T/o.txt && echo 1)" "$(grep '^cat' $T/o.txt | tr '\n' ' ')"
node -e "
const L=require('$T/len.json'),o=require('fs').readFileSync('$T/o.txt','utf8');let bad=[];
for(const m of o.matchAll(/^play (\w+) ([\d.]+)$/gm)){const k=m[1],x=+m[2];
 const want=k=='shot'?0:k=='click'||k=='game'?.13:L[k];if(Math.abs(x-want)>.002)bad.push(k+' '+x+' (wanted '+want.toFixed(4)+')')}
console.log(bad.length?bad.join(', '):'ok')" > $T/len.txt
ok "4. each played as chosen: the Rudolf style at the page's length (rsRender's), Classic and Retro as tones, the switched-off one not at all" "$(grep -qx ok $T/len.txt && echo 1)" "$(cat $T/len.txt)"
ok "5. a page's Rudolf sound (snd_page, as Oracle's window sends it) played; a malformed one as tones, not as events" "$(grep -qx 'page menu 2' $T/o.txt && grep -qx 'bad 3' $T/o.txt && echo 1)" "$(grep -E '^(page|bad)' $T/o.txt | tr '\n' ' ')"
ok "6. no crash" "$(grep -qx 'rc 0' $T/o.txt && echo 1)" "$(tail -1 $T/o.txt) $(head -2 $T/err.txt)"
echo "$P passed, $F failed"; [ $F = 0 ]
