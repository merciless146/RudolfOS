#!/bin/sh
# rudolf-apps (1.2.9) on the host: the Flathub catalogue with empty elements (<p/>, <em/>, <value .../>), which
# crashed it ("Segmentation fault" after "Downloading the app catalogue..."). sh apps_t.sh   (needs gcc, zlib)
D=$(cd "$(dirname "$0")/../../sys" && pwd); T=$(mktemp -d); cd "$T" || exit 1
gcc -O2 -w -o ra "$D/rudolf-apps.c" -lz || exit 1
P=0; N=0; ok() { N=$((N + 1)); if [ "$1" = 0 ]; then P=$((P + 1)); echo "PASS $2"; else echo "FAIL $2 - $3"; fi; }
cat > a.xml <<'X'
<?xml version="1.0" encoding="UTF-8"?>
<components version="0.16" origin="flathub">
<component type="desktop-application"><id>org.example.One</id><name>One</name><name xml:lang="de">Eins</name><summary>First &amp; best</summary>
<description><p>Text with <em/> an empty emphasis and <code>code</code>.</p><p/><ul><li>Item</li><li/></ul></description>
<categories><category>Game</category><category/></categories><developer id="x"><name>Ex Ample</name></developer>
<url type="homepage">https://example.org/</url><releases><release version="1.0" timestamp="1700000000"/></releases>
<screenshots><screenshot type="default"><image type="source"/><image type="thumbnail" width="624">https://example.org/s.png</image></screenshot></screenshots>
<custom><value key="flathub::verification::verified"/><value key="other">x</value></custom></component>
<component type="desktop-application"><id>org.example.Two.desktop</id><name>Two</name><summary/>
<custom><value key="flathub::verification::verified">true</value></custom></component>
<component type="runtime"><id>org.example.Platform</id><name>Platform</name></component>
</components>
X
gzip -kf a.xml
o=$(RA_APPSTREAM=a.xml.gz ./ra catalog 2>&1); r=$?
[ $r = 0 ]; ok $? "catalog: no crash on empty elements" "rc $r: $o"
echo "$o" | grep -q '"id":"org.example.One","name":"One","summary":"First & best","cats":\["Game",""\],"verified":false,"dev":"Ex Ample"'; ok $? "...the app with its name, summary, categories, developer (an empty verified value is not verified)" "$o"
echo "$o" | grep -q '"id":"org.example.Two","name":"Two","summary":"","cats":\[\],"verified":true'; ok $? "...an id ending in .desktop, an empty summary, verified" "$o"
echo "$o" | grep -q Platform; [ $? = 1 ]; ok $? "...runtimes left out"
o=$(RA_APPSTREAM=a.xml.gz ./ra catalog cat.json 2>&1); [ "$o" = '{"ok":true,"apps":2}' ] && [ -s cat.json ]; ok $? "catalog OUT: written, two apps" "$o"
o=$(RA_APPSTREAM=a.xml.gz ./ra info org.example.One 2>&1); r=$?
[ $r = 0 ] && printf "%s" "$o" | grep -q '"description":"Text with  an empty emphasis and code.\\n• Item"' && echo "$o" | grep -q '"screenshots":\["https://example.org/s.png"\]' && echo "$o" | grep -q '"version":"1.0"'; ok $? "info: description, screenshot and release with empty elements" "rc $r: $o"
printf 'not gzip' > b.xml.gz; o=$(RA_APPSTREAM=b.xml.gz ./ra catalog 2>&1); r=$?; [ $r = 1 ] || [ $r = 0 ]; ok $? "a file that is not a catalogue: no crash" "rc $r"
cd /; rm -rf "$T"; echo "$P/$N passed"; [ $P = $N ]
