#!/bin/sh
# 1.6.2 (docs/plan-1.6.2.md Part 34, step 1): driverless scanning with rudolf-scan and sane-airscan, with the firewall on.
# In network namespaces of this test's own (never the host's network or firewall), as mdns161_t.sh: this computer's
# with "wlan0" (a veth to a scanners' network) and "wwan0" (a veth marked as a cellular modem's); the firewall's real
# rules loaded with nft; stand-in scanners (fixtures/esclsrv.py: eSCL with a known picture made by its own code, mDNS
# and WS-Discovery responders); rudolf-scan as built from this tree, with the build machine's libsane and sane-airscan
# (the same Ubuntu 24.04 packages the base layer takes them from). Pages checked by their pixels (fixtures/scancheck.py),
# three pages into one PDF, the errors eSCL tells, a USB scanner through ipp-usb's address (a stand-in on loopback with
# usbprint's guard rules), scanning as an account's own user into its Documents (owner and mode), the fuzz test of what
# rudolf-scan parses (fuzz/scan_fuzz.c), and the sizes and memory.
# Needs unshare, ip, nft, nsenter, setpriv, python3 with PIL, gcc, zlib, libsane1, sane-airscan, poppler-utils. As root.
#   sh scan162_t.sh
HERE=$(cd "$(dirname "$0")" && pwd); SYS=$HERE/../../sys; FX=$HERE/fixtures
for c in unshare ip nft python3 gcc nsenter setpriv pdfinfo pdftoppm; do command -v $c >/dev/null || { echo "SKIP: needs $c"; exit 0; }; done
MA=$(gcc -print-multiarch 2>/dev/null || echo x86_64-linux-gnu)
[ -e /usr/lib/$MA/libsane.so.1 ] && [ -e /usr/lib/$MA/sane/libsane-airscan.so.1 ] || { echo "SKIP: needs libsane1 and sane-airscan (apt-get install libsane1 sane-airscan)"; exit 0; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3-pil"; exit 0; }
[ "$(id -u)" = 0 ] || { echo "SKIP: as root (network namespaces, the firewall's rules, another user)"; exit 0; }
if [ -z "$S162_INNER" ]; then exec env S162_INNER=1 unshare -n sh "$0" "$@"; fi
T=$(mktemp -d); chmod 755 $T; PIDS=""
UADD=; cleanup() { for p in $PIDS; do kill $p 2>/dev/null; done; [ -n "$UADD" ] && { userdel acct46001 2>/dev/null; groupdel acct46001 2>/dev/null; }; rm -rf "$T"; }   # (only what this test started)
trap cleanup EXIT INT TERM
N=0; F=0; ok() { N=$((N+1)); if [ "$2" = 1 ]; then echo "PASS $1"; else echo "FAIL $1 - $(echo "$3" | head -c 1500)"; F=$((F+1)); fi; }
w8() { i=0; while [ $i -lt 80 ]; do eval "$1" && return 0; timeout 0.1 tail -f /dev/null; i=$((i+1)); done; return 1; }
. $SYS/../systools/hardflags.sh 2>/dev/null

# ---- 1. the fuzz test (no network) and the program as the release builds it
if gcc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -Wno-misleading-indentation -o $T/scan_fuzz $HERE/fuzz/scan_fuzz.c -ldl -lm -lz 2>$T/fcc; then
	o=$($T/scan_fuzz ${SCAN_FUZZ_SECS:-20} 2>&1)
	ok "1. parse_mdns, the XML readers and png_decode: real inputs read; every cut and byte, ${SCAN_FUZZ_SECS:-20} s of mutations: no crash, no read past the end, no loop (ASan, UBSan)" "$(echo "$o" | grep -q '^found Office Scanner' && echo "$o" | grep -q '^xml ok' && echo "$o" | grep -q '^png ok' && echo "$o" | grep -q '^ok [0-9]' && echo 1)" "$o"
else ok "1. the fuzz test builds" 0 "$(cat $T/fcc)"; fi
gcc -O2 $HARD -o $T/rudolf-scan $SYS/rudolf-scan.c -ldl -lm -Wl,-Bstatic -lz -Wl,-Bdynamic 2>$T/cc && chmod 755 $T/rudolf-scan
ok "2. rudolf-scan builds with the hardening flags (hardcheck.py), needing only the C library, libm and libdl (libsane opened when it scans)" "$(python3 $SYS/../systools/hardcheck.py $T/rudolf-scan >/dev/null 2>&1 && [ -z "$(objdump -p $T/rudolf-scan | awk '/NEEDED/{print $2}' | grep -vE '^(libc|libm)\.so\.6$|^libdl\.so\.2$')" ] && echo 1)" "$(cat $T/cc; objdump -p $T/rudolf-scan | grep NEEDED)"
R=$T/rudolf-scan

# ---- the networks: B the scanners' (veth sc0), C a cellular network's (veth cell0)
ip link set lo up
unshare -n tail -f /dev/null & PB=$!; PIDS="$PIDS $PB"
unshare -n tail -f /dev/null & PC=$!; PIDS="$PIDS $PC"
w8 "[ \"\$(readlink /proc/$PB/ns/net)\" != \"\$(readlink /proc/self/ns/net)\" ] && [ \"\$(readlink /proc/$PC/ns/net)\" != \"\$(readlink /proc/self/ns/net)\" ]"
B() { nsenter -t $PB -n "$@"; }; C() { nsenter -t $PC -n "$@"; }
ip link add wlan0 type veth peer name sc0 && ip link set sc0 netns $PB && ip link add wwan0 type veth peer name cell0 && ip link set cell0 netns $PC || { echo "SKIP: veth pairs cannot be made here"; exit 0; }
ip addr add 192.168.77.1/24 dev wlan0; ip link set wlan0 up; ip addr add 10.64.0.1/24 dev wwan0; ip link set wwan0 up
B ip link set lo up; for a in 2 3 4 5; do B ip addr add 192.168.77.$a/24 dev sc0; done; B ip link set sc0 up
C ip link set lo up; C ip addr add 10.64.0.2/24 dev cell0; C ip link set cell0 up
ip route add 224.0.0.0/4 dev wlan0 2>/dev/null; B ip route add 224.0.0.0/4 dev sc0 2>/dev/null; C ip route add 224.0.0.0/4 dev cell0 2>/dev/null
w8 "ip -o link show wlan0 | grep -q LOWER_UP && ip -o link show wwan0 | grep -q LOWER_UP"

# ---- the firewall: its real rules (paths moved into this test's folder), wwan0 a cellular modem's
mkdir -p $T/p $T/run; sed -e "s#/persist/etc#$T/p#g" -e "s#/run/rudolf#$T/run#g" $SYS/root/usr/lib/rudolf/firewall > $T/fw; chmod +x $T/fw
$T/fw start && $T/fw cellular add wwan0
IN=$(nft list chain inet rudolf input); WS=$(nft list chain inet rudolf wsd); SET=$(nft list set inet rudolf wsdopen)
ok "3. the firewall: a wsd chain (not on a cellular interface; port 3702 from private addresses only while its port is in the set wsdopen), jumped to before the others and the drop; the set empty" "$(echo "$IN" | grep -B1 'jump wsd' | grep -q 'udp sport 547 udp dport 546 accept' && echo "$IN" | grep -A1 'jump wsd' | grep -q 'jump remote' && echo "$IN" | grep -A1 'jump mdns' | grep -q 'counter name "blocked" drop' && echo "$WS" | grep -A1 'iifname @cellular return' | grep 'udp dport @wsdopen accept' | grep -q 'ip saddr { 10.0.0.0/8, 169.254.0.0/16, 172.16.0.0/12, 192.168.0.0/16 }' && [ "$(echo "$WS" | grep -c accept)" = 1 ] && ! echo "$SET" | grep -q elements && echo 1)" "$IN $WS $SET"
$T/fw wsd open; S1=$(nft list set inet rudolf wsdopen); $T/fw wsd open; S2=$(nft list set inet rudolf wsdopen)
ok "4. firewall wsd open: port 3702 in the set for ten seconds (opened again: the ten seconds start again)" "$(echo "$S1" | grep -q 'elements = { 3702 timeout 10s expires' && echo "$S2" | grep -q 'elements = { 3702 timeout 10s expires [0-9]' && echo 1)" "$S1 $S2"
blocked() { $T/fw status | awk '{print $2}'; }

# ---- the stand-in scanners: Office Scanner (eSCL, mDNS and WSD) .2, Old WSD Scanner (WSD only) .3, one whose ProbeMatches
# relates to another message .4 and one whose device address is another computer's .5; a scanner on the cellular network
B python3 $FX/esclsrv.py 192.168.77.2 8641 $T/r1 --mdns "Office Scanner" --wsd normal & PIDS="$PIDS $!"; w8 "[ -s $T/r1 ]"
B python3 $FX/esclsrv.py 192.168.77.3 8642 $T/r2 --name "Old WSD Scanner" --wsd normal --no-escl & PIDS="$PIDS $!"
B python3 $FX/esclsrv.py 192.168.77.4 8643 $T/r3 --name "Forged Scanner" --wsd wrongid --no-escl & PIDS="$PIDS $!"
B python3 $FX/esclsrv.py 192.168.77.5 8644 $T/r4 --name "Elsewhere Scanner" --wsd otherhost --no-escl & PIDS="$PIDS $!"
C python3 $FX/esclsrv.py 10.64.0.2 8645 $T/r5 --mdns "Cell Scanner" --wsd normal & PIDS="$PIDS $!"
w8 "[ -s $T/r1 ] && [ -s $T/r2 ] && [ -s $T/r3 ] && [ -s $T/r4 ] && [ -s $T/r5 ]"
find() { env RS_USB=$T/none RS_CELLULAR=${1:-$T/run/firewall-cellular} setpriv --reuid=65534 --regid=65534 --clear-groups $R find; }
$T/fw wsd open; b0=$(blocked); o=$(find); b1=$(blocked)
ok "5. the firewall on, its WSD window open: find (as an unprivileged user) lists Office Scanner once, as eSCL at http://192.168.77.2:8641/eSCL with its model" "$(echo "$o" | grep -q '"id":"escl:http://192.168.77.2:8641/eSCL","name":"Office Scanner","model":"Office Scanner","via":"network","proto":"eSCL"' && [ "$(echo "$o" | grep -o '192.168.77.2:' | wc -l)" = 1 ] && echo 1)" "$o"
ok "6. ...and the WSD scanner by its scan service's address and its own name (its metadata asked of the address that answered)" "$(echo "$o" | grep -q '"id":"wsd:http://192.168.77.3:8642/wsd/scan","name":"Old WSD Scanner (WSD)","model":"Rudolf Old WSD Scanner","via":"network","proto":"WSD"' && echo 1)" "$o"
ok "7. ...not a ProbeMatches for another message, nor one whose device is at another address than the one that answered, nor the cellular network's scanner" "$(echo "$o" | grep -q '"ok":true' && ! echo "$o" | grep -q 'wsd:http://192.168.77.4' && ! echo "$o" | grep -q 'wsd:http://192.168.77.5' && ! echo "$o" | grep -q '192.0.2.99' && ! echo "$o" | grep -q 'Cell Scanner' && echo 1)" "$o"
ok "8. ...the firewall's blocked counter did not rise ($b0 -> $b1): mDNS's answers and the WSD answers came in" "$([ -n "$b0" ] && [ "$b0" = "$b1" ] && echo 1)" "$b0 $b1"
printf 'destroy element inet rudolf wsdopen { 3702 }\n' > $T/d.nft && nft -f $T/d.nft
b0=$(blocked); o=$(find); b1=$(blocked)
ok "9. the WSD window closed (as ten seconds after it opened): the WSD scanner's answer is dropped and counted ($b0 -> $b1); the eSCL one still found by mDNS" "$(echo "$o" | grep -q 'Office Scanner' && ! echo "$o" | grep -q 'wsd:' && [ "$b1" -gt "$b0" ] && echo 1)" "$o $b0 $b1"
$T/fw wsd open; find > $T/bg & FP=$!; w8 "grep -q ':14E9 ' /proc/net/udp"; U=$(cat /proc/net/udp); G=$(cat /proc/net/igmp); w8 "grep -q ':0E76 ' /proc/net/udp"; U2=$(cat /proc/net/udp); wait $FP
ok "10. while find runs: UDP 5353 (joined to 224.0.0.251 on wlan0, not on wwan0), then UDP 3702, as uid 65534; when it ends: both closed, the group left" "$(echo "$U" | grep ':14E9 ' | awk '{print $8}' | grep -qx 65534 && echo "$U2" | grep ':0E76 ' | awk '{print $8}' | grep -qx 65534 && echo "$G" | awk '/^[0-9]/{d=$2} /FB0000E0/{print d}' | grep -qx wlan0 && ! echo "$G" | awk '/^[0-9]/{d=$2} /FB0000E0/{print d}' | grep -qx wwan0 && ! grep -qE ':(14E9|0E76) ' /proc/net/udp && ! grep -q FB0000E0 /proc/net/igmp && grep -q 'Office Scanner' $T/bg && echo 1)" "$U $U2 $G"
$T/fw wsd open; b0=$(blocked); o=$(find /dev/null); b1=$(blocked)
ok "11. nothing accepted on a cellular-marked interface: find made to ask on wwan0 too still lists no cellular scanner, its answers counted as blocked ($b0 -> $b1)" "$(echo "$o" | grep -q 'Office Scanner' && ! echo "$o" | grep -q 'Cell Scanner' && [ "$b1" -gt "$b0" ] && echo 1)" "$o"

# ---- scanning (as an unprivileged user) and the pages' pixels
ID=escl:http://192.168.77.2:8641/eSCL; CK() { python3 $FX/scancheck.py "$@"; }
mkdir -p $T/s; chown 65534:65534 $T/s; as() { setpriv --reuid=65534 --regid=65534 --clear-groups "$@"; }
st() { B python3 -c "import urllib.request,sys;urllib.request.urlopen('http://192.168.77.2:8641/_test/set?'+sys.argv[1]).read()" "$1"; }
o=$(as $R caps $ID)
ok "12. caps: the glass and the feeder (215.9 x 296.9 mm), 75-300 dpi, colour, grey and black and white" "$(echo "$o" | grep -q '{"ok":true,"sources":\[{"source":"flatbed","width":215.9,"height":296.9,"resolutions":\[75,100,150,200,300\]},{"source":"adf"' && echo "$o" | grep -q '"modes":\["color","grey","bw"\]' && echo 1)" "$o"
o=$(as $R scan $ID $T/s preview=1); c=$(CK $T/s/preview.png dpi=75)
ok "13. a preview: the whole glass at 75 dpi in colour, the picture's colours where they belong" "$(echo "$o" | grep -q '"pages":\["preview.png"\],"width":637,"height":877,"dpi":75' && echo "$c" | grep -q '^ok' && echo 1)" "$o $c"
o=$(as $R scan $ID $T/s res=150); c=$(CK $T/s/page-1.png dpi=150)
ok "14. a page in colour at 150 dpi: 1275 x 1754 pixels, the pixels checked" "$(echo "$o" | grep -q '"pages":\["page-1.png"\]' && echo "$c" | grep -q '^ok' && echo 1)" "$o $c"
o=$(as $R scan $ID $T/s res=100 mode=grey first=2); c=$(CK $T/s/page-2.png dpi=100 mode=grey)
ok "15. grey at 100 dpi: the luminance of each colour" "$(echo "$o" | grep -q '"mode":"grey"' && echo "$c" | grep -q '^ok' && file -b $T/s/page-2.png | grep -q '8-bit grayscale' && echo 1)" "$o $c"
o=$(as $R scan $ID $T/s res=200 mode=bw first=3); c=$(CK $T/s/page-3.png dpi=200 mode=bw)
ok "16. black and white at 200 dpi (sane-airscan has no line-art mode: grey made black or white here): a 1-bit page, the right points black and white" "$(echo "$o" | grep -q '"mode":"bw"' && echo "$c" | grep -q '^ok' && file -b $T/s/page-3.png | grep -q '1-bit grayscale' && echo 1)" "$o $c"
o=$(as $R scan $ID $T/s res=300 area=20,15,120,80 first=4); c=$(CK $T/s/page-4.png dpi=300 area=20,15,120,80)
ok "17. an area (20, 15 mm; 120 x 80 mm) at 300 dpi: its size and what is in it (the black square, the red quarter)" "$(echo "$c" | grep -q '^ok .* 2 points' && echo 1)" "$o $c"
L=$(B python3 -c "import urllib.request;print(urllib.request.urlopen('http://192.168.77.2:8641/_test/log').read().decode())" | grep '^POST /eSCL/ScanJobs' | tail -1)
ok "18. ...asked of the scanner as eSCL says (the area in 1/300 inch: 1417 x 945 at 236, 177)" "$(echo "$L" | grep -q 'Platen RGB24 300 1417 945 236 177' && echo 1)" "$L"
st 'adf=loaded&pages=3'; o=$(as $R scan $ID $T/s source=adf res=150 first=10)
c1=$(CK $T/s/page-10.png dpi=150 page=1); c2=$(CK $T/s/page-11.png dpi=150 page=2); c3=$(CK $T/s/page-12.png dpi=150 page=3)
ok "19. the feeder: three pages, each its own (the bottom-right grey of pages 1, 2 and 3)" "$(echo "$o" | grep -q '"pages":\["page-10.png","page-11.png","page-12.png"\]' && printf '%s\n%s\n%s\n' "$c1" "$c2" "$c3" | grep -c '^ok' | grep -qx 3 && echo 1)" "$o $c1 $c2 $c3"
o=$(as $R save pdf $T/s/three.pdf $T/s/page-10.png $T/s/page-11.png $T/s/page-12.png); pi=$(pdfinfo $T/s/three.pdf 2>&1)
c=; for k in 1 2 3; do c="$c$(CK $T/s/three.pdf pdfpage=$k page=$k dpi=150 tol=10)
"; done
ok "20. three pages into one PDF: 3 pages of 612 x 841.44 points (1275 x 1753 pixels at 150 dpi), each page's picture checked (JPEG inside)" "$(echo "$o" | grep -q '"ok":true,"bytes":[0-9]*,"pages":3' && echo "$pi" | grep -q '^Pages: *3' && echo "$pi" | grep -q 'Page size: *612 x 841.44 pts' && [ "$(echo "$c" | grep -c '^ok')" = 3 ] && ! echo "$pi" | grep -qi error && echo 1)" "$o $pi $c"
o=$(as $R save pdf $T/s/mixed.pdf $T/s/page-1.png $T/s/page-2.png $T/s/page-3.png); im=$(pdfimages -list $T/s/mixed.pdf 2>&1)
ok "21. a PDF of a colour, a grey and a black and white page: JPEG colour, JPEG grey, 1-bit" "$(echo "$im" | awk 'NR>2{print $6, $8, $9}' | tr '\n' ';' | grep -qx 'rgb 8 jpeg;gray 8 jpeg;gray 1 image;' && echo 1)" "$o $im"
o1=$(as $R save png $T/s/one.png $T/s/page-1.png); o2=$(as $R save jpeg $T/s/one.jpg $T/s/page-1.png); c1=$(CK $T/s/one.png dpi=150); c2=$(CK $T/s/one.jpg dpi=150 tol=10)
o3=$(as $R save png $T/s/one.png $T/s/page-2.png)
ok "22. a page saved as PNG and as JPEG (150 dpi in the file), both checked; a file of that name already there is never written over" "$(echo "$c1" | grep -q '^ok' && echo "$c2" | grep -q '^ok' && file -b $T/s/one.jpg | grep -q 'density 150x150' && echo "$o3" | grep -q '"code":"exists"' && CK $T/s/one.png dpi=150 >/dev/null && echo 1)" "$o1 $o2 $c1 $c2 $o3"

# ---- what eSCL tells: said plainly
e() { as $R scan $ID $T/s "$@" | sed -n 's/.*"code":"\([a-z]*\)".*/\1/p'; }
st 'adf=jam'; j=$(e source=adf first=20); st 'adf=open'; op=$(e source=adf first=20); st 'adf=empty'; em=$(e source=adf first=20); st 'adf=loaded&fail=1'; io=$(e first=20)
st 'busy=1'; t0=$(date +%s); bz=$(e first=20); t1=$(date +%s); st 'busy=0'
o=$(as $R scan escl:http://192.168.77.9:8641/eSCL $T/s first=20)
ok "23. errors said plainly: a paper jam (jam), the cover or feeder open (open), the feeder empty (nodocs), the scanner cut off in the middle (io), busy (busy, after sane-airscan's retries: $((t1 - t0)) s), no scanner there (noscanner)" "$([ "$j $op $em $io $bz" = 'jam open nodocs io busy' ] && echo "$o" | grep -q '"code":"noscanner","error":"The scanner is not there any more' && [ ! -e $T/s/page-20.png ] && echo 1)" "jam=$j open=$op empty=$em io=$io busy=$bz $o"
o=$(as $R scan $ID $T/s mode=sepia); o2=$(as $R scan 'escl:http://192.168.77.2/;touch /tmp/x' $T/s); o3=$(as $R scan sane:net:192.168.77.2 $T/s); o4=$(as $R scan $ID /root)
ok "24. what it is asked is checked: an unknown setting, an address with other characters, a SANE backend not on the list (net), a folder it cannot write" "$(echo "$o" | grep -q '"code":"args"' && echo "$o2" | grep -q '"code":"args"' && echo "$o3" | grep -q '"code":"args"' && echo "$o4" | grep -q '"code":"write"' && echo 1)" "$o $o2 $o3 $o4"
ls $T/s/ > $T/ls; ok "25. nothing left behind in the folder but the pages and files made (no half-written files), nor SANE settings in /tmp" "$(! grep -q 'rscan-' $T/ls && ! ls /tmp | grep -q '^rscan-' && echo 1)" "$(cat $T/ls; ls /tmp | grep rscan)"

# ---- an account's own user: its Documents, the owner and mode
id acct46001 >/dev/null 2>&1 || { groupadd -g 46001 acct46001 2>/dev/null; useradd -u 46001 -g 46001 -M -d $T/h acct46001 2>/dev/null && UADD=1; }
mkdir -p $T/h/Documents $T/h/.cache/rudolf-scan/s1; chown -R 46001:46001 $T/h; chmod 700 $T/h
ac() { setpriv --reuid=46001 --regid=46001 --clear-groups env HOME=$T/h "$@"; }
o=$(ac $R scan $ID $T/h/.cache/rudolf-scan/s1 res=75) && o2=$(ac $R save pdf "$T/h/Documents/Scan 1.pdf" $T/h/.cache/rudolf-scan/s1/page-1.png)
ok "26. as an account's own user (46001): the page in its scans' folder, the PDF in its Documents, its own, 0644 (as File Explorer's files)" "$(echo "$o2" | grep -q '"ok":true' && [ "$(stat -c '%u %a' "$T/h/Documents/Scan 1.pdf")" = '46001 644' ] && [ "$(stat -c '%u %a' $T/h/.cache/rudolf-scan/s1/page-1.png)" = '46001 644' ] && echo 1)" "$o $o2 $(ls -la $T/h/Documents)"
o=$(as $R save pdf $T/h/Documents/x.pdf $T/h/.cache/rudolf-scan/s1/page-1.png)
ok "27. another user cannot read that account's pages nor write in its Documents" "$(echo "$o" | grep -q '"ok":false' && [ ! -e $T/h/Documents/x.pdf ] && echo 1)" "$o"

# ---- a USB scanner through ipp-usb (a stand-in eSCL on 127.0.0.1:60001 as ipp-usb serves one, its guard rules loaded)
python3 $FX/esclsrv.py 127.0.0.1 60001 $T/r6 & PIDS="$PIDS $!"; w8 "[ -s $T/r6 ]"
printf 'ipp\tCanon MF264dw II\tipp://127.0.0.1:60001/ipp/print\tMFG:Canon;MDL:MF264dw II;\t1-1:1.0\n' > $T/usb
sed -n "/^guard_rules()/,/^E\$/p" $SYS/root/usr/lib/rudolf/usbprint | sed '1,2d;$d' > $T/guard.nft
sed -i "s/{ 0, 952, 1000 }/{ 0, 65534 }/" $T/guard.nft; nft -f $T/guard.nft
o=$(as env RS_USB=$T/usb RS_ONLY=usb $R find)
ok "28. find lists the USB scanner that speaks eSCL through ipp-usb's address (as the desktop's user, which the guard lets through)" "$(echo "$o" | grep -q '"id":"escl:http://127.0.0.1:60001/eSCL","name":"USB: Canon MF264dw II","model":"Rudolf Test Scanner","via":"usb","proto":"eSCL"' && echo 1)" "$o"
o=$(ac $R scan escl:http://127.0.0.1:60001/eSCL $T/h/.cache/rudolf-scan/s1 res=75 first=5)
: > $T/allow.nft; env RUDOLF_USBPRINT_NFT=$T/allow.nft sh $SYS/root/usr/lib/rudolf/usbprint scanner allow 46001; nft -f $T/allow.nft
o2=$(ac $R scan escl:http://127.0.0.1:60001/eSCL $T/h/.cache/rudolf-scan/s1 res=75 first=5); c=$(CK $T/h/.cache/rudolf-scan/s1/page-5.png)
: > $T/allow.nft; env RUDOLF_USBPRINT_NFT=$T/allow.nft sh $SYS/root/usr/lib/rudolf/usbprint scanner done 46001; nft -f $T/allow.nft
o3=$(ac $R scan escl:http://127.0.0.1:60001/eSCL $T/h/.cache/rudolf-scan/s1 res=75 first=6)
bad=$(for u in 1000 0 abc 60000; do env RUDOLF_USBPRINT_NFT=$T/x.nft sh $SYS/root/usr/lib/rudolf/usbprint scanner allow $u >/dev/null 2>&1 && echo "took $u"; done)
ok "29. the account's own user scans the USB scanner only while usbprint lets it (scanner allow UID: an element of scanuids that times out in 15 minutes; scanner done UID): refused before and after; only accounts' users" "$(echo "$o" | grep -q '"code":"noscanner"' && echo "$o2" | grep -q '"ok":true' && echo "$c" | grep -q '^ok' && echo "$o3" | grep -q '"code":"noscanner"' && [ -z "$bad" ] && grep -q 'meta skuid @scanuids accept' $T/guard.nft && echo 1)" "$o | $o2 | $c | $o3 | $bad"

# ---- sizes and memory
st 'adf=loaded'; o=$(as $R scan $ID $T/s res=300 first=30); pk=$(echo "$o" | sed -n 's/.*"peakKB":\([0-9]*\).*/\1/p')
o2=$(as $R save pdf $T/s/big.pdf $T/s/page-30.png); pk2=$(echo "$o2" | sed -n 's/.*"peakKB":\([0-9]*\).*/\1/p')
SZ=$(( $(stat -L -c %s /usr/lib/$MA/libsane.so.1) + $(stat -L -c %s /usr/lib/$MA/sane/libsane-airscan.so.1) )); SR=$(stat -c %s $R)
echo "::notice title=Scanning (Part 34)::disk: rudolf-scan $((SR / 1024)) KB + libsane $(( $(stat -L -c %s /usr/lib/$MA/libsane.so.1) / 1024)) KB + sane-airscan $(( $(stat -L -c %s /usr/lib/$MA/sane/libsane-airscan.so.1) / 1024)) KB = $(( (SZ + SR) / 1024 )) KB; memory: nothing while idle (no process), ${pk} KB at most scanning A4 at 300 dpi in colour, ${pk2} KB making its PDF"
ok "30. sizes and memory: rudolf-scan, libsane and sane-airscan under 700 KB on the disk; scanning A4 at 300 dpi in colour under 80 MB; nothing running when idle" "$([ $((SZ + SR)) -lt 716800 ] && [ -n "$pk" ] && [ "$pk" -lt 81920 ] && [ -n "$pk2" ] && [ "$pk2" -lt 81920 ] && ! pgrep -x rudolf-scan >/dev/null && echo 1)" "$((SZ + SR)) bytes; $o; $o2"
echo "$((N-F))/$N passed"; [ $F = 0 ]
