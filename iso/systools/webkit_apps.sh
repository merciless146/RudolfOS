#!/bin/sh
# 1.5.7 (plan stage 11, Part 4 step 4): every built-in app opened in WebKit. The desktop page, as a plain web page (the
# browser build: nothing reaches rudolfd or the person's accounts), in Cog (WPE WebKit, headless, in Debian), signs in to
# a throwaway account in its own storage, then opens each app in the Start menu's list in turn (3 s each), closes its
# windows, and says whether a script error happened meanwhile. One line per app: "OK key name" or "ERROR key name:
# message". WebKit only (running Chromium beside the desktop while it is in use made it stop responding on 2026-11-10).
# Run as root over the development link:   sh webkit_apps.sh
W=/tmp/wapps; PORT=8198
avail() { awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo; }
[ "$(avail)" -lt 400 ] && { echo "STOP: only $(avail) MB available"; exit 1; }
rm -rf $W; mkdir -p $W/www; cp -a /usr/share/rudolf/www/. $W/www/
cat > $W/www/wapps.js <<'J'
(function(){let errs=[];const B=(p)=>{new Image().src='/b/'+p};
 addEventListener('error',e=>errs.push(String(e.message||e).slice(0,160)+(e.lineno?' @'+e.lineno:'')));
 addEventListener('unhandledrejection',e=>errs.push('promise: '+String(e.reason&&e.reason.message||e.reason).slice(0,160)));
 const enc=s=>encodeURIComponent(String(s).replace(/\//g,'∕')).slice(0,600);
 const sleep=ms=>new Promise(r=>setTimeout(r,ms));
 const run=async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
  ACCTS.length=0;ACCTS.push({n:'trial',h:await pwMake('trial1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'trial1');await sleep(2000);
  B('signedin/'+enc(errs.join(' | ')));errs=[];
  const keys=Object.keys(APPS).filter(k=>!/^(lx:|rapp:|pkg:)/.test(k)&&!APPS[k].rapp&&!['off','logoff','report'].includes(k));
  B('count/'+keys.length);
  for(const k of keys){const n0=wins.length;let thrown='';try{const r=launch(k);if(r&&r.then)await Promise.race([r,sleep(3000)])}catch(e){thrown=String(e.message||e)}
   await sleep(3000);const opened=wins.length>n0;
   const all=(thrown?['thrown: '+thrown]:[]).concat(errs);errs=[];
   B((all.length?'ERROR/':'OK/')+enc(k)+'/'+enc(APPS[k].name||k)+'/'+(opened?'win':'nowin')+'/'+enc(all.join(' | ')));
   wins.slice().forEach(w=>{try{w.__force=1;w.close()}catch(e){}});document.querySelectorAll('.rmodal').forEach(o=>o.remove());await sleep(500)}
  B('done/'+keys.length)};
 const wait=()=>{if(typeof launch=='function'&&typeof FSLOADED!='undefined'&&FSLOADED)setTimeout(run,2000);else setTimeout(wait,500)};
 B('start/'+enc(navigator.userAgent));wait()})();
J
echo '<script src="wapps.js"></script>' >> $W/www/index.html; mkdir -p $W/www/b; chown -R 1000:1000 $W
su -s /bin/sh rudolf -c "exec busybox httpd -f -vv -p 127.0.0.1:$PORT -h $W/www" 2>$W/http.log & HP=$!
trap 'kill $HP 2>/dev/null; /usr/bin/rudolf-debian run -- sh -c "pkill -x cog" 2>/dev/null' EXIT
sleep 1
/usr/bin/rudolf-debian run -- sh -c "exec cog --platform=headless http://127.0.0.1:$PORT/ >/tmp/cog-apps.log 2>&1" &
i=0; while ! grep -q 'url:/b/done/' $W/http.log; do sleep 2; i=$((i+2)); [ $i -gt 900 ] && { echo "STOP: not done in 15 minutes"; break; }
	[ "$(avail)" -lt 300 ] && { echo "STOP: only $(avail) MB available"; break; }; done
dec() { sed 's/%20/ /g; s/%3A/:/g; s/%2C/,/g; s/%27/'"'"'/g; s/%22/"/g; s/%7C/|/g; s/%40/@/g; s/%3D/=/g; s/%28/(/g; s/%29/)/g; s/%E2%88%95/\//g; s/%5B/[/g; s/%5D/]/g; s/%7B/{/g; s/%7D/}/g; s/%2B/+/g; s/%3B/;/g'; }
# (busybox httpd logs the address decoded: to the end of the line)
sed -n 's#^.*url:/b/##p' $W/http.log | dec | awk -F/ '$1=="OK"||$1=="ERROR"{printf "%s %s (%s, %s)%s\n",$1,$2,$3,$4,($5!=""?": "$5:"");next}{print}'
