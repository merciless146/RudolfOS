#!/usr/bin/env python3
# 1.5.9 (docs/plan-1.5.9.md Part 3, step 3): the ARM64 system started whole in QEMU (aarch64 virt, 3 GB, virtio
# graphics, keyboard, tablet, disk and network): the ARM64 kernel, and its four layers on a disc image, as a disc
# starts (no Secure Boot: the layers start unchecked, which Task Manager shows). Passes when the Lite desktop (WebKit:
# no Chromium on ARM64 yet) answers rudolfd (its heartbeat: the sign-in screen is up). Reads over the debug shell on
# the serial port (rudolf.debug=1, ttyAMA0) and takes a picture of the screen.
#   python3 arm64_boot.py IMAGE BASE.SFS CHROMIUM.SFS DESKTOP.SFS MODULES.SFS OUTDIR
import os, sys, subprocess, time, shutil
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from guest import Guest
img, base, chrom, desk, mods, out = sys.argv[1:7]
here = os.path.dirname(os.path.abspath(__file__)); os.makedirs(out, exist_ok=True)
log = open(os.path.join(out, 'boot.txt'), 'w')
def say(*a):
    t = ' '.join(str(x) for x in a); print(t, flush=True); log.write(t + '\n'); log.flush()
# the disc: rudolf/id (as the kernel's initramfs has it) and the four layers
d = os.path.join(out, 'disc'); shutil.rmtree(d, ignore_errors=True); os.makedirs(d + '/rudolf')
shutil.copy(os.path.join(here, '../initramfs/etc/rudolf-id'), d + '/rudolf/id')
# the probe, read from the disc inside the system (typing long commands into the emulated serial port loses characters,
# even slowly: 2026-11-13): it reports every 10 s until the desktop answers or 15 minutes pass, then what is needed
open(d + '/rudolf/p.sh', 'w').write(r'''
echo PROBE-START; [ -d /newroot ] && echo "PROBE: in the initramfs (the start-up stopped)" && tail -30 /tmp/initramfs.log
i=0; while [ $i -lt 90 ]; do
  S="$(cat /run/rudolf/desktop-engine 2>/dev/null)|$([ -e /run/rudolf/heartbeat ] && echo up)|$(pidof Xorg rudolf-webkit WebKitWebProcess | wc -w)"
  echo "STATUS $S"; case "$S" in webkit\|up*) break;; esac; i=$((i+1)); sleep 10; done
echo "== uname"; uname -m; echo "== unverified"; cat /etc/rudolf/unverified; echo "== engine"; cat /run/rudolf/engine-choice /run/rudolf/desktop-engine
echo "== dri"; ls /dev/dri /dev/fb* /sys/class/graphics; cat /proc/fb; grep -iE "drm|virtio|fb" /proc/modules; echo "== free"; free -m; echo "== session.log"; tail -50 /run/rudolf/session.log
echo "== Xorg"; grep -E "\((EE|WW)\)" /run/rudolf/Xorg.log | head -20; echo "== initramfs.log"; tail -20 /etc/rudolf/initramfs.log
echo "== dmesg"; dmesg | tail -40; echo "== ps"; ps -o pid,ppid,stat,args | grep -v "\[" | head -45
echo "== waiting"; for p in $(pidof rudolf-init sh); do echo "$p $(cat /proc/$p/wchan 2>/dev/null) $(tr '\0' ' ' < /proc/$p/cmdline | cut -c1-80)"; done | head -20
echo "== init-trace"; tail -40 /run/rudolf/init-trace.log; ls -l /usr/lib/rudolf/session
echo PROBE-DONE
''')
for n, f in (('base', base), ('chromium', chrom), ('desktop', desk), ('modules', mods)): os.link(f, f'{d}/rudolf/{n}.sfs') if os.stat(f).st_dev == os.stat(d).st_dev else shutil.copy(f, f'{d}/rudolf/{n}.sfs')
iso = os.path.join(out, 'disc.iso')
subprocess.run(['xorriso', '-as', 'mkisofs', '-quiet', '-R', '-V', 'RUDOLF', '-o', iso, d], check=True)
shutil.rmtree(d)
N = 'a64'
for s in (f'/tmp/ser-{N}.sock', f'/tmp/qmp-{N}.sock'):
    if os.path.exists(s): os.unlink(s)
acc = 'kvm' if os.access('/dev/kvm', os.W_OK) else 'tcg'; say('accelerator', acc)
q = subprocess.Popen(['qemu-system-aarch64', '-M', 'virt', '-cpu', 'max' if acc == 'tcg' else 'host', '-accel', acc, '-m', '3G', '-smp', '4',
    '-kernel', img, '-append', 'console=ttyAMA0 console=tty0 rudolf.debug=1 rudolf.lite=1 panic=-1',
    '-drive', f'if=none,id=d0,format=raw,readonly=on,file={iso}', '-device', 'virtio-blk-pci,drive=d0',
    '-device', 'virtio-gpu-pci,xres=1280,yres=800', '-device', 'virtio-keyboard-pci', '-device', 'virtio-tablet-pci',
    '-nic', 'user,model=virtio-net-pci', '-display', 'none', '-no-reboot',
    '-serial', f'unix:/tmp/ser-{N}.sock,server=on,wait=off', '-qmp', f'unix:/tmp/qmp-{N}.sock,server=on,wait=off'],
    stdout=open(os.path.join(out, 'qemu.txt'), 'w'), stderr=subprocess.STDOUT)
time.sleep(2)
g = Guest(N); cons = open(os.path.join(out, 'console.txt'), 'w')
# (console=tty0 last: /dev/console is the screen, so the debug shell is the only reader of ttyAMA0. With the console
#  there too, a second reader of the port took every other character typed: 2026-11-13)
def typ(c):   # one character at a time
    for ch in c.encode():
        try: g.s.s.send(bytes([ch]))
        except OSError: pass
        time.sleep(0.08)
t0 = time.time(); ok = False; started = False; buf = ''; seen = set()
try:
    while time.time() - t0 < 1080 and q.poll() is None:
        o = g.s.read(3).replace('\r', ''); cons.write(o); cons.flush(); buf += o
        if not started and time.time() - t0 > 8 and int(time.time() - t0) % 15 < 3:
            typ('\nsh /*/m*/rudolf/p.sh\n')
        if 'PROBE-START' in buf and not started: started = True; say(f'{int(time.time()-t0)} s: a shell in the started system runs the probe')
        for l in buf.split('\n')[:-1]:
            if (l.startswith('STATUS ') or l.startswith('PROBE:')) and l not in seen: seen.add(l); say(f'{int(time.time()-t0)} s: {l}')
        if 'STATUS webkit|up' in buf: ok = True
        if 'PROBE-DONE' in buf.split('PROBE-START')[-1] if 'PROBE-START' in buf else False: break
        buf = buf[-20000:]
    if q.poll() is not None: say('QEMU stopped:', q.returncode)
    try: g.shot(os.path.join(out, 'screen.png')); say('screen: screen.png')
    except Exception as e: say('no screen picture:', e)
    if 'PROBE-START' in buf: say('--- the probe:'); say(buf.split('PROBE-START')[-1][-8000:])
finally:
    cons.close(); q.kill()
    if not ok and 'PROBE-START' not in buf:
        say('--- the serial console, last lines:'); say('\n'.join(open(os.path.join(out, 'console.txt'), errors='replace').read().split('\n')[-60:]))
        say('--- QEMU:'); say(open(os.path.join(out, 'qemu.txt'), errors='replace').read()[-2000:])
say('PASS: the ARM64 system reached the Lite sign-in screen in QEMU' if ok else 'FAIL: the Lite desktop did not answer within 15 minutes')
sys.exit(0 if ok else 1)
