#!/bin/sh
# (2026-11-10: the WebKit half ran on the Vivobook, results in docs/plan-1.5.7.md Part 4. Running the Chromium half beside
# the desktop while the computer was in use made the desktop stop responding for 65 s: run "webkit" only, or only
# while nobody uses the computer.)
# 1.5.7 (plan stage 10, Part 4 step 1): the WebKit trial. The desktop page, as a plain web page (the browser build:
# nothing reaches rudolfd or the person's accounts), is opened in WebKit (Cog, WPE WebKit, headless, in Debian) and in
# Chromium (headless, a profile of its own), one after the other, and goes through the same steps by itself (a small
# script added to a copy of the page): loaded, signed in to a throwaway account (in that page's own storage only),
# File Explorer, WordPro, Emu open, 20 s apart. Each step calls back (a request to the local server, which logs it);
# 12 s later the memory of that engine's processes is measured: PSS (shared pages divided among their users) and
# private memory (what this engine alone holds, in memory and in the compressed swap). Nothing on the screen; the
# person's desktop is only read. Stops if less than 250 MB are available.
# Run as root over the development link:   sh webkit_trial.sh [webkit|chromium|both]
W=/tmp/wtrial; PORT=8199; WHICH=${1:-both}
avail() { awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo; }
mem() {   # PIDS -> "pss_mb private_mb processes"
	pss=0; prv=0; n=0
	for p in $1; do a=0; b=0; grep -q '^State:.*Z' /proc/$p/status 2>/dev/null && continue; [ -r /proc/$p/smaps_rollup ] || continue
		eval "$(awk '/^Pss:/{a+=$2}/^SwapPss:/{a+=$2}/^Private_Clean:|^Private_Dirty:/{b+=$2}/^Swap:/{b+=$2}END{printf "a=%d b=%d",a,b}' /proc/$p/smaps_rollup 2>/dev/null)"
		[ "$a" -gt 0 ] && n=$((n+1)); pss=$((pss+a)); prv=$((prv+b)); done
	echo "$((pss/1024)) $((prv/1024)) $n"; }
[ "$(avail)" -lt 300 ] && { echo "STOP: only $(avail) MB available"; exit 1; }
rm -rf $W; mkdir -p $W/www; cp -a /usr/share/rudolf/www/. $W/www/
# the steps, in the page itself
cat > $W/steps.js <<'J'
(function(){const S=['loaded','signin','explorer','wordpro','emu'];let k=0,t0=Date.now();
 addEventListener('error',e=>{new Image().src='/beacon/jserror/'+encodeURIComponent(String(e.message).slice(0,200)+' @'+e.lineno)});
 const say=s=>{const st={wins:typeof wins!='undefined'?wins.length:-1,heap:performance.memory?Math.round(performance.memory.usedJSHeapSize/1048576):null,dom:document.getElementsByTagName('*').length};new Image().src='/beacon/'+s+'/'+encodeURIComponent(JSON.stringify(st))};
 const step=async()=>{const s=S[k++];if(!s)return;try{
   if(s=='signin'){try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS.length=0;ACCTS.push({n:'trial',h:await pwMake('trial1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'trial1')}
   else if(s!='loaded')launch(s)}catch(e){}say(s);setTimeout(step,20000)};
 new Image().src='/beacon/start/'+encodeURIComponent(navigator.userAgent);
 const wait=()=>{if(typeof launch=='function'&&typeof FSLOADED!='undefined'&&FSLOADED)setTimeout(step,3000);else if(Date.now()-t0>60000)new Image().src='/beacon/stuck/'+encodeURIComponent(JSON.stringify({launch:typeof launch,FSLOADED:typeof FSLOADED=='undefined'?'undef':FSLOADED,login:!!document.getElementById('login')}));else setTimeout(wait,500)};wait()})();
J
cp $W/steps.js $W/www/wtrial-steps.js
# (added at the very end: the page's own scripts hold "</body>" in strings, which a replacement would break)
echo '<script src="wtrial-steps.js"></script>' >> $W/www/index.html
mkdir -p $W/www/beacon; chown -R 1000:1000 $W
su -s /bin/sh rudolf -c "exec busybox httpd -f -vv -p 127.0.0.1:$PORT -h $W/www" 2>$W/http.log & HP=$!
stop_all() { pkill -f "user-data-dir=$W/chrome" 2>/dev/null; /usr/bin/rudolf-debian run -- sh -c 'pkill -x cog; pkill -x WPEWebProcess; pkill -x WPENetworkProce' 2>/dev/null; }
trap 'kill $HP 2>/dev/null; stop_all' EXIT
sleep 1
follow() {   # ENGINE PIDS-COMMAND
	n=0; for s in loaded signin explorer wordpro emu; do
		i=0; while ! grep -q "/beacon/$s/" $W/http.log; do sleep 1; i=$((i+1)); [ $i -gt 120 ] && { echo "$1 $s: no answer from the page in 2 minutes"; return; }; done
		sleep 12; a=$(avail); set -- "$1" "$2" $(mem "$(eval "$2")")
		st=$(grep -o "/beacon/$s/[^ ]*" $W/http.log | tail -1 | sed "s#.*/beacon/$s/##" | sed 's/%22/"/g; s/%3A/:/g; s/%2C/,/g; s/%7B/{/g; s/%7D/}/g')
		echo "$1 $s: PSS $3 MB, private $4 MB, $5 processes; page $st; available $a MB"
		[ "$a" -lt 250 ] && { echo "STOP: only $a MB available"; return; }
	done; }
if [ "$WHICH" != chromium ]; then echo "== WebKit (WPE WebKit 2.48, Cog 0.18, headless)"; : > $W/http.log
	/usr/bin/rudolf-debian run -- sh -c "exec cog --platform=headless http://127.0.0.1:$PORT/ >/tmp/cog-trial.log 2>&1" &
	follow webkit 'for p in $(ls /proc | grep "^[0-9]*$"); do case "$(cat /proc/$p/comm 2>/dev/null)" in cog|WPEWebProcess|WPENetworkPro*|WPEGPUProcess) echo $p;; esac; done'
	stop_all; sleep 5
fi
if [ "$WHICH" != webkit ]; then echo "== Chromium (the system's, headless, a profile of its own)"; : > $W/http.log
	[ "$(avail)" -lt 300 ] && { echo "STOP: only $(avail) MB available"; exit 1; }
	su -s /bin/sh rudolf -c "exec /opt/chromium/chrome --headless=new --user-data-dir=$W/chrome --window-size=1920,1080 --no-first-run http://127.0.0.1:$PORT/ >$W/chrome.log 2>&1" &
	follow chromium "pgrep -f 'user-data-dir=$W/chrome'"
	stop_all
fi
echo "== the desktop's own Chromium now: PSS, private, processes: $(mem "$(pgrep -f 'user-data-dir=/home/rudolf/.rudolf-browser')")"
