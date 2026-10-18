#!/bin/bash
# Run the 1.2 desktop inside the shell on this machine (no VM): Xvfb, the rudolfd stand-in, test websites
# (site.py on *.test:8099), Rudolf's Chromium started through rudolf-launch, as the session does.
#   host.sh start [fresh]   build the desktop page and the shell (test key), start everything
#   host.sh stop
# Needs: CHROME (Rudolf's Chromium), KEY (a test shell key: mkiwa.py key), WORK (a scratch folder).
set -e
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HERE/../../.." && pwd)
: "${WORK:?}" "${CHROME:?}" "${KEY:?}"
export DISPLAY=:77
PROF=$WORK/prof; WWW=$WORK/www; STATE=$WORK/state
stop() {
	ps aux | grep "[u]ser-data-dir=$PROF" | awk '{print $2}' | xargs -r kill 2>/dev/null || true
	[ -f $WORK/mock.pid ] && kill $(cat $WORK/mock.pid) 2>/dev/null || true
	[ -f $WORK/xvfb.pid ] && kill $(cat $WORK/xvfb.pid) 2>/dev/null || true
	[ -f $WORK/site.pid ] && kill $(cat $WORK/site.pid) 2>/dev/null || true
	[ -f $WORK/rudolfd.pid ] && kill $(cat $WORK/rudolfd.pid) 2>/dev/null || true
	rm -f $WORK/*.pid /etc/chromium/policies/managed/rudolf-shell.json
}
case "$1" in
stop) stop; exit 0;;
start) ;;
*) echo "usage: host.sh start [fresh] | stop"; exit 2;;
esac
stop; sleep 1
mkdir -p $WWW/iwa $STATE
[ "$2" = fresh ] && rm -rf $PROF $STATE && mkdir -p $STATE
(cd $REPO/desktop && python3 build19.py $WWW/index.html >/dev/null)
# a stand-in for Ruffle (the disc loads the real one from the internet): it swaps Flash content for a blue box
mkdir -p $WWW/lib/ruffle
echo "document.querySelectorAll('embed,object').forEach(e=>{const r=document.createElement('ruffle-embed');r.style.cssText='display:inline-block;width:300px;height:200px;background:rgb(0,0,255)';e.replaceWith(r)})" > $WWW/lib/ruffle/ruffle.js
ID=$(python3 $REPO/iso/systools/mkiwa.py id $KEY); echo $ID > $WWW/iwa/id
python3 $REPO/iso/systools/mkiwa.py build $REPO/iso/shell $WWW/iwa/shell.swbn $KEY >/dev/null
python3 $REPO/iso/systools/mkiwa.py update $REPO/iso/shell http://127.0.0.1:8080/iwa/shell.swbn $WWW/iwa/update.json >/dev/null
mkdir -p /etc/chromium/policies/managed
printf '{"IsolatedWebAppInstallForceList":[{"update_manifest_url":"http://127.0.0.1:8080/iwa/update.json","web_bundle_id":"%s"}]}\n' $ID > /etc/chromium/policies/managed/rudolf-shell.json
setsid Xvfb :77 -screen 0 1280x800x24 -nolisten tcp >$WORK/xvfb.log 2>&1 & echo $! > $WORK/xvfb.pid
setsid python3 $HERE/site.py 8099 >$WORK/site.log 2>&1 & echo $! > $WORK/site.pid
setsid python3 $HERE/rudolfd_mock.py $WWW $STATE $PROF >$WORK/mock.log 2>&1 & echo $! > $WORK/mock.pid
gcc -O2 -o $WORK/rudolf-launch $REPO/iso/sys/rudolf-launch.c
# a test copy of the real rudolfd for the system and process information: fixed token, port 8181, its own run folder
sed -e 's/for (int i = 0; i < 16; i++) sprintf(token + i \* 2, "%02x", rb\[i\]);/strcpy(token, "testtoken");/' -e 's/htons(8080)/htons(8181)/' \
	-e 's#define RUN      "/run/rudolf"#define RUN "/tmp/rdt-run"#' $REPO/iso/sys/rudolfd.c > $WORK/rudolfd_t.c
gcc -O2 -o $WORK/rudolfd_t $WORK/rudolfd_t.c -Wl,-Bstatic -lz -Wl,-Bdynamic 2>/dev/null
mkdir -p /run/user/1000
setsid $WORK/rudolfd_t >$WORK/rudolfd.log 2>&1 & echo $! > $WORK/rudolfd.pid
sleep 1
# the extensions, copied as the session does (Chromium writes into an extension's folder); with the shell the
# session loads only Ruffle's
EXT=$WORK/ext; rm -rf $EXT; mkdir -p $EXT; cp -a $REPO/iso/sys/root/usr/lib/rudolf/frames $REPO/iso/sys/root/usr/lib/rudolf/ruffle-ext $EXT/
# the installed shell is only replaced when its version changes: start from a fresh profile after editing it
setsid $WORK/rudolf-launch $ID $CHROME --no-sandbox --rudolf-kiosk --class=RudolfDesktop --kiosk --start-fullscreen --window-position=0,0 --window-size=1280,800 \
	--user-data-dir=$PROF --no-first-run --no-default-browser-check --noerrdialogs --disable-infobars --password-store=basic \
	--no-proxy-server --remote-debugging-port=9333 --load-extension=$EXT/ruffle-ext \
	--enable-features=SharedArrayBuffer,IsolatedWebApps --autoplay-policy=no-user-gesture-required \
	--use-fake-ui-for-media-stream --disable-gpu --host-resolver-rules="MAP *.test 127.0.0.1" ${EXTRA:-} >$WORK/chrome.log 2>&1 &
echo "started: shell $ID"
