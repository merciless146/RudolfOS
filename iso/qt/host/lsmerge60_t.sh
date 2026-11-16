#!/bin/sh
# 1.6.0 (the owner's reports 3 and 5 on the native desktop): rudolfd's copy of the pages' storage (lsync, compiled from rudolfd.c:
# fixtures/deskapi.c). On Native each app window and Oracle's page in Chromium send only the keys they changed (part); rudolfd
# puts them into its copy: the other keys kept as they were (another engine's page cannot put its older values back), removed
# keys gone, the copy's number its own (larger than before and than the page's); one value read back (GET lsync?key=), as
# Oracle's page takes a setting the WebKit windows saved; pages sending at once lose nothing (a lock).   sh lsmerge60_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d /tmp/lsmerge.XXXXXX); trap 'rm -rf $T' EXIT
gcc -O2 -w -o $T/t $HERE/fixtures/deskapi.c -lz 2>$T/cc || { cat $T/cc; echo "FAIL build"; exit 1; }
N=0; F=0
ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 600)"; F=$((F+1)); fi; }
mkdir -p $T/acc $T/home $T/run; L=$T/ls.json
t() { $T/t $T/acc $L $T/home $T/run "$@"; }
j() { node -e "const l=JSON.parse(require('fs').readFileSync('$L'));console.log($1)"; }
printf '%s' '{"gen":40,"data":{"bd95set:u1":"{\"accent\":\"#16a34a\",\"seenRel\":\"r9\"}","bd95notif:u1":"[{\"t\":\"a\"}]","other":"\"\\u2028 x\""}}' > $L
printf '%s' '{"gen":7,"part":1,"set":{"bd95set:u1":"{\"accent\":\"#e11d48\",\"seenRel\":\"r9\"}","new":"1"},"del":["other"]}' > $T/b
o=$(t "POST lsync <$T/b")
ok "1. a page's part: its keys put in, the others kept as they were, a removed key gone" "$([ "$(j "[JSON.parse(l.data['bd95set:u1']).accent,l.data['bd95notif:u1'],l.data.new,'other' in l.data].join('|')")" = '#e11d48|[{"t":"a"}]|1|false' ] && echo 1)" "$o $(cat $L)"
ok "2. ...the copy's number rudolfd's (41: its own plus one, not the page's 7), given back" "$(echo "$o" | grep -q '"gen":41' && [ "$(j l.gen)" = 41 ] && echo 1)" "$o"
printf '%s' '{"gen":90,"part":1,"set":{"x":"2"},"del":[]}' > $T/b; o=$(t "POST lsync <$T/b")
ok "3. ...a page that counted further: its number (90)" "$(echo "$o" | grep -q '"gen":90' && echo 1)" "$o"
o=$(t "GET lsync?key=bd95set%3Au1" "GET lsync?key=none")
ok "4. one value read back, exactly as kept; a key not there: null" "$(echo "$o" | sed -n 1p | grep -q '"v":"{\\"accent\\":\\"#e11d48\\",\\"seenRel\\":\\"r9\\"}"' && echo "$o" | sed -n 2p | grep -q '"v":null' && echo 1)" "$o"
printf '%s' '{"gen":1,"part":1,"set":{"bad":5}}' > $T/b; o=$(t "POST lsync <$T/b")
ok "5. a value that is not a string is not taken (the storage's values are strings)" "$([ "$(j "'bad' in l.data")" = false ] && echo 1)" "$o"
printf '%s' '{"gen":1,"part":1,"set":"x"}' > $T/b; o=$(t "POST lsync <$T/b")
ok "6. a part that is not an object is refused, the copy unchanged" "$(echo "$o" | grep -q '^HTTP/1.[01] 400' && [ "$(j l.gen)" = 91 ] && echo 1)" "$o"
# twenty pages at once, each its own key: none lost
for i in $(seq 1 20); do printf '{"gen":1,"part":1,"set":{"k%s":"v%s"}}' $i $i > $T/b$i; done
for i in $(seq 1 20); do t "POST lsync <$T/b$i" >/dev/null & done; wait
ok "7. twenty pages sending at once: every key is there" "$([ "$(j "Array.from({length:20},(_,i)=>l.data['k'+(i+1)]).filter(Boolean).length")" = 20 ] && echo 1)" "$(head -c 400 $L)"
echo "$((N-F))/$N passed"; [ $F = 0 ]
