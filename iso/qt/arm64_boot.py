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
for n, f in (('base', base), ('chromium', chrom), ('desktop', desk), ('modules', mods)): os.link(f, f'{d}/rudolf/{n}.sfs') if os.stat(f).st_dev == os.stat(d).st_dev else shutil.copy(f, f'{d}/rudolf/{n}.sfs')
iso = os.path.join(out, 'disc.iso')
subprocess.run(['xorriso', '-as', 'mkisofs', '-quiet', '-R', '-V', 'RUDOLF', '-o', iso, d], check=True)
shutil.rmtree(d)
N = 'a64'
for s in (f'/tmp/ser-{N}.sock', f'/tmp/qmp-{N}.sock'):
    if os.path.exists(s): os.unlink(s)
acc = 'kvm' if os.access('/dev/kvm', os.W_OK) else 'tcg'; say('accelerator', acc)
q = subprocess.Popen(['qemu-system-aarch64', '-M', 'virt', '-cpu', 'max' if acc == 'tcg' else 'host', '-accel', acc, '-m', '3G', '-smp', '4',
    '-kernel', img, '-append', 'console=ttyAMA0 rudolf.debug=1 panic=-1',
    '-drive', f'if=none,id=d0,format=raw,readonly=on,file={iso}', '-device', 'virtio-blk-pci,drive=d0',
    '-device', 'virtio-gpu-pci,xres=1280,yres=800', '-device', 'virtio-keyboard-pci', '-device', 'virtio-tablet-pci',
    '-nic', 'user,model=virtio-net-pci', '-display', 'none', '-no-reboot',
    '-serial', f'unix:/tmp/ser-{N}.sock,server=on,wait=off', '-qmp', f'unix:/tmp/qmp-{N}.sock,server=on,wait=off'],
    stdout=open(os.path.join(out, 'qemu.txt'), 'w'), stderr=subprocess.STDOUT)
time.sleep(2)
g = Guest(N); cons = open(os.path.join(out, 'console.txt'), 'w')
t0 = time.time(); ok = False; shell = False; last = ''
def sh(c, t=30):
    o = g.sh(c, t); return o
try:
    # the debug shell appears when rudolf-init starts (after the kernel and the initramfs found the disc)
    while time.time() - t0 < 900 and q.poll() is None:
        if not shell:
            cons.write(g.s.read(2)); cons.flush()
            # (the serial port takes input only once something reads it: a send can time out until then)
            try: g.s.s.send(b'\n'); o = g.sh('echo READY$((40+2))', 5)
            except OSError: o = ''
            cons.write(o); cons.flush()
            if 'READY42' in o: shell = True; say(f'{int(time.time()-t0)} s: the system started (debug shell on ttyAMA0)'); continue
            continue
        try: st = sh('printf "%s|%s|%s\\n" "$(cat /run/rudolf/desktop-engine 2>/dev/null)" "$([ -e /run/rudolf/heartbeat ] && echo up)" "$(pidof rudolf-webkit Xorg | wc -w)"', 20).strip().split('\n')[-1]
        except OSError: st = 'serial busy'
        if st != last: say(f'{int(time.time()-t0)} s: engine|heartbeat|processes = {st}'); last = st
        if st.startswith('webkit|up'): ok = True; break
        time.sleep(10)
    if q.poll() is not None: say('QEMU stopped:', q.returncode)
    if shell:
        time.sleep(5 if ok else 0)
        try: g.shot(os.path.join(out, 'screen.png')); say('screen: screen.png')
        except Exception as e: say('no screen picture:', e)
        for c in ('uname -m; cat /etc/rudolf/unverified', 'cat /run/rudolf/desktop-engine; ls /dev/dri', 'free -m', 'tail -40 /run/rudolf/session.log',
                  'grep -E "\\((EE|WW)\\)" /run/rudolf/Xorg.log | head -20', 'dmesg | grep -iE "error|fail|virtio" | tail -20', 'ps -o pid,rss,args | head -40'):
            say('$ ' + c)
            try: say(sh(c, 30))
            except OSError: say('(the serial port did not take the command)')
finally:
    try: cons.write(g.s.read(1))
    except OSError: pass
    cons.close(); q.kill()
    if not ok:   # (the artifacts may not be reachable: the console's end and QEMU's messages in the log too)
        say('--- the serial console, last lines:'); say('\n'.join(open(os.path.join(out, 'console.txt'), errors='replace').read().replace('\r', '').split('\n')[-60:]))
        say('--- QEMU:'); say(open(os.path.join(out, 'qemu.txt'), errors='replace').read()[-2000:])
say('PASS: the ARM64 system reached the Lite sign-in screen in QEMU' if ok else 'FAIL: the Lite desktop did not answer within 15 minutes')
sys.exit(0 if ok else 1)
