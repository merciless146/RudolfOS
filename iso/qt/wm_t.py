import sys,time;sys.path.insert(0,'.')
from guest import Guest
g=Guest(sys.argv[1])
print(g.put('rd-new','/tmp/rd-new'))
print(g.sh("cp /tmp/rd-new /usr/bin/rudolf-display; pkill -x rudolf-display; sleep 4; pgrep -x rudolf-display && echo restarted; echo desktop > /run/rudolf/wm"))
print(g.sh("su -s /bin/sh rudolf -c 'DISPLAY=:0 /tmp/xwin > /tmp/xwin.log 2>&1 &'; sleep 4; tail -1 /tmp/xwin.log; cat /run/rudolf/wm"))
g.key('meta_l','esc');time.sleep(3)
print(g.sh("tail -1 /tmp/xwin.log; cat /run/rudolf/wm"))
print(g.sh("echo game > /run/rudolf/wm; sleep 3; tail -1 /tmp/xwin.log; pkill xwin; sleep 3; cat /run/rudolf/wm"))
