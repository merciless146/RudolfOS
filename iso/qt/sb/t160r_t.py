# Rudolf OS 1.6.0 in QEMU (docs/plan-1.6.0.md Part 15, "Before stable" 7; docs/plan-1.5.9.md 8.4, 8.5, 8.7): the
# official recovery on the real release. A computer installed as Install here leaves it: the release's own boot image
# (boot.iso, signed with the Rudolf OS Secure Boot key) in front of RUDOLFDATA, the release in slot a; it starts from
# its own drive through the boot menu (shim, systemd-boot, rudolf.efi) with a network (QEMU's user network), so the
# repairs download from GitHub as on a computer. The serial shell: the menu's Debug start entry (Secure Boot off), made
# the default with the firmware variable LoaderEntryDefault, which outlives the boot files being written again.
#   Part 1 (Secure Boot off; the official boot files):
#     1-3   the official recovery system made at the first start; both start-up files official (sbverify with the
#           Rudolf OS certificate); the EFI image's size
#     4-6   recovery.efi changed (and system/r's desktop layer deleted): at the next start system/r is made again, the
#           release's boot image downloaded, checked and staged; the start after writes it and recovery.efi is the
#           official one again, and the computer starts through it
#     7-8   recovery.efi deleted: the same
#     9-12  Developer mode on (a TEST developer key), a custom build in slot b and a custom rudolf.efi (the official one
#           without its signature, as a custom build's own); Return to official (keep): the official system from
#           system/r, the official rudolf.efi written again, the custom slot cleared, Official in its status
#   Part 2 (a computer that never ran the official build: a custom build alone, started as t160 starts one):
#     13-15 Developer mode on: the official recovery system downloaded from the official address (the stable channel),
#           checked with the official key; Recovery mode then starts it
#   Part 3 (Secure Boot on: OVMF with Microsoft's keys and a TEST certificate in db, as recovery_sb_t.sh): the
#           release's boot image with its menu, rudolf.efi and recovery.efi made again from the release's kernel and
#           signed with the TEST Secure Boot key (rudolf.efi with rudolf.debug=1 for the serial shell; recovery.efi
#           carrying the official release key, iso/trusted-keys.txt), shim as released
#     16-20 Secure Boot on and locked down, every layer checked; Developer mode on: a custom build in slot b does not
#           start under Secure Boot (its layers are not signed by the official key), the computer goes back to the
#           official system by itself; Return to official from there; Recovery mode (recovery.efi) starts the official
#           system/r, checked, under Secure Boot
#   python3 t160r_t.py REL-DIR     (REL-DIR: the release's files, manifest.txt/.sig, boot.iso and boot.iso.sha256;
#   SB_PARTS: OVMF_CODE_4M.secboot.fd, OVMF_VARS_4M.fd, OVMF_VARS_4M.ms.fd; QEMU; STUB: linuxx64.efi.stub; root)
import os, sys, time, re, subprocess, shutil, struct, json, base64, hashlib
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL = os.path.abspath(sys.argv[1]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); ONLY = os.environ.get('PARTS_RUN', '123')
LOG = open('t160r.txt', 'w'); R = []
def out(*a):
    s = ' '.join(str(x) for x in a); print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[-2000:] if d != '' and not c else ''))
man = open(f'{REL}/manifest.txt').read(); mv = lambda k: re.search(rf'^{k} (.*)$', man, re.M).group(1)
MEM = int(os.environ.get('MEM', '3072')); W = os.path.abspath('w160r'); SER = int(mv('serial'))
shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
def run_(a, **k): return subprocess.run(a, check=True, **k)
GUID = '4a67b082-0a4c-41cf-b6c7-440b29bb8c4f'
# ---- the boot image: where its EFI image is (the El Torito catalogue's UEFI entry), and its size
BOOT = f'{REL}/boot.iso'
def efi_entry(iso):
    with open(iso, 'rb') as f:
        f.seek(17 * 2048); bv = f.read(2048); f.seek(struct.unpack('<I', bv[0x47:0x4b])[0] * 2048); c = f.read(2048)
    for i in range(32, 256, 32):
        if c[i] == 0x88 and c[i - 32] == 0x91: return struct.unpack('<I', c[i + 8:i + 12])[0] * 2048, struct.unpack('<H', c[i + 6:i + 8])[0]
    raise SystemExit('no UEFI entry in ' + iso)
EOFF, ESECS = efi_entry(BOOT)
out(f'the release\'s EFI image: {ESECS} sectors of 512 bytes, {ESECS * 512 / 1048576:.1f} MiB of the 32 MiB an El Torito entry can name (at byte {EOFF})')
def mt(img, *a, inp=None):
    return subprocess.run(list(a[:1]) + ['-i', f'{img}@@{EOFF}'] + list(a[1:]), capture_output=True, text=True, input=inp)
# ---- TEST keys: a developer's (the custom build) and, for part 3, a Secure Boot key; deleted with W
run_([sys.executable, f'{ISO}/pkgtools/rpk-tool.py', 'keygen', f'{W}/dev'], stdout=subprocess.DEVNULL)
DEVPUB = open(f'{W}/dev/rpk-signing.pub').read().strip()
os.makedirs(f'{W}/cl'); [os.link(f'{REL}/{n}.sfs', f'{W}/cl/{n}.sfs') for n in ('base', 'chromium', 'desktop', 'modules')]
run_([sys.executable, f'{ISO}/systools/make_release.py', '--layers', f'{W}/cl', '--kernel', f'{REL}/vmlinuz', '--out', f'{W}/cust', '--key', f'{W}/dev/rpk-signing.key',
      '--serial', str(SER + 1), '--version', mv('version') + '-dev', '--chromium', mv('chromium'), '--kernel-version', mv('kernel')], stdout=subprocess.DEVNULL)
CUST = f'{W}/cust/channel'
# the custom build's own rudolf.efi: the official one without its signature (it starts with Secure Boot off)
run_(['mcopy', '-n', '-i', f'{BOOT}@@{EOFF}', '::/EFI/rudolf/rudolf.efi', f'{W}/rudolf-off.efi'])
run_(['sbattach', '--remove', f'{W}/rudolf-off.efi']); shutil.move(f'{W}/rudolf-off.efi', f'{W}/rudolf-custom.efi')

# this tree's start-up script, rudolf-verity and rudolf-kexec, unpacked over the release kernel's own (as recovery_sb_t.sh)
os.makedirs(f'{W}/ov/bin'); shutil.copy(f'{ISO}/initramfs/init', f'{W}/ov/init'); os.chmod(f'{W}/ov/init', 0o755)
run_(['gcc', '-Os', '-static', '-o', f'{W}/ov/bin/rudolf-verity', f'{ISO}/sys/rudolf-verity.c'])
run_(['gcc', '-Os', '-static', '-nostdlib', '-fno-builtin', '-fno-stack-protector', '-fno-asynchronous-unwind-tables', '-o', f'{W}/ov/bin/rudolf-kexec', f'{ISO}/sys/rudolf-kexec.c'])
OVCPIO = f'{W}/ov.cpio'; subprocess.run(f'cd {W}/ov && find . | cpio -o -H newc --quiet -R 0:0 > {OVCPIO}', shell=True, check=True)
def slot_files(T, s, src_manifest, serial, ver):
    d = f'{T}/os/system/{s}'; os.makedirs(d)
    shutil.copy(f'{REL}/vmlinuz', f'{d}/vmlinuz')
    for n in ('base', 'chromium', 'desktop', 'modules'): shutil.copy(f'{REL}/{n}.sfs', f'{d}/{n}.sfs')
    for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{src_manifest}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {serial}\nversion {ver}\nreleased {mv('released')}\nchromium {mv('chromium')}\nkernel {mv('kernel')}\nnotes test\n")
    open(f'{d}/complete', 'w').write('1\n')
def make_drive(path, image, official=True, extra=None, dmb=3600):
    """Install here's layout: IMAGE (a boot image) at the start, RUDOLFDATA as partition 3 at 128 MiB"""
    start = 262144
    T = os.path.abspath(path + '.tree'); shutil.rmtree(T, ignore_errors=True); os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc/rudolf'); os.makedirs(f'{T}/t')
    if official: slot_files(T, 'a', REL, SER, mv('version'))
    else: slot_files(T, 'a', CUST, SER + 1, mv('version') + '-dev')
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    open(f'{T}/os/update.conf', 'w').write('auto=0\n')   # (no update of its own during the test)
    if extra: extra(T)
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = path + '.data'; subprocess.run(['truncate', '-s', f'{dmb}M', D], check=True)
    subprocess.run(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D], check=True); shutil.rmtree(T)
    with open(path, 'wb') as f:
        f.truncate((start + dmb * 2048 + 2048) * 512)
        if image:
            with open(image, 'rb') as i: shutil.copyfileobj(i, f)
        f.seek(446); f.write(bytes(16))   # (an internal drive: no partition 1 over the image, as Install here leaves it)
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', start, dmb * 2048))
        f.seek(510); f.write(b'\x55\xaa'); f.seek(start * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D)

class TSer(Ser):   # the serial line, also written to a file (the start-up's messages there, with console=ttyS0)
    def __init__(s, path, logf): super().__init__(path); s.lf = open(logf, 'a')
    def read(s, t=1.0):
        o = super().read(t); s.lf.write(o); s.lf.flush(); return o
N = [0]
def boot(tag, drive, vars_keep, secure_code=True, kernel_cmd=None, wait=1200, net=True, initrd=None):
    N[0] += 1; name = f't160r{N[0]}{tag}'
    extra = ['-drive', f'file={drive},format=raw,if=none,id=nv', '-device', 'nvme,drive=nv,serial=rudolf160r' + (',bootindex=0' if kernel_cmd is None else '')]
    if net: extra += ['-netdev', 'user,id=n1', '-device', 'e1000,netdev=n1']
    if kernel_cmd is not None: extra += ['-kernel', f'{REL}/vmlinuz', '-append', kernel_cmd] + (['-initrd', initrd] if initrd else [])
    vm = sbvm.VM(name, PARTS, [], secure=secure_code, vars_from=vars_keep, mem=MEM, extra=extra)
    vm.keep = vars_keep; vm.serlog = os.path.join(vm.dir, 'serial.txt')
    g = Guest.__new__(Guest); g.N = vm.name; g.s = TSer(vm.ser_path, vm.serlog); g.q = vm.q; g.s.read(0.3)
    time.sleep(20); end = time.time() + wait; up = False
    while time.time() < end:
        try:
            if 'ready' in g.sh('echo ready', 8): up = True; break
        except OSError: pass
        time.sleep(4)
    out(f'--- start {N[0]} ({tag}):', 'up' if up else 'NOT UP')
    return vm, g, up
def stop(vm, g):
    try: g.sh('sync', 60)
    except Exception: pass
    vm.stop(); time.sleep(3)
    if vm.keep: shutil.copy(vm.vars, vm.keep)   # (the firmware's variables kept for the next start)
def run(g, c, t=60):
    f = os.path.join(W, 'c160r.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c160r.sh', '644')
    o = g.sh('sh /tmp/c160r.sh 2>&1', t); out('$', c[:300].replace('\n', ' ; ')); out(o.rstrip()[-3500:]); return o
def wait_in(g, cond, secs, what):
    o = run(g, f"i=0; while [ $i -lt {secs // 10} ]; do {cond} && break; i=$((i+1)); sleep 10; done; {cond} && echo 'WAITED-OK {what}' || echo 'WAITED-NO {what}'", secs + 60)
    return 'WAITED-OK' in o
def efivar_file(name, value):
    p = f'{W}/{name}.var'; open(p, 'wb').write(struct.pack('<I', 7) + (value + '\0').encode('utf-16-le')); return p
def set_var(g, name, value):
    p = efivar_file(name, value); g.put(p, f'/tmp/{name}.var', '644')
    return run(g, f"V=/sys/firmware/efi/efivars; mountpoint -q $V || mount -t efivarfs efivarfs $V; f=$V/{name}-{GUID}; [ -e $f ] && chattr -i $f 2>/dev/null; "
                  f"cat /tmp/{name}.var > $f; echo rc=$?; od -An -c $f | head -3", 30)
STATUS = "echo SLOT=$(cat /etc/rudolf/slot); cat /etc/rudolf/cmdline; echo; /usr/lib/rudolf/espcheck status; rudolf-update status; echo; cat /persist/os/system/state; ls /persist/os/system"
ESPM = "mkdir -p /mnt/esp; mount -t vfat /dev/nvme0n1p2 /mnt/esp"

VMS = [None]
def part1():
        global vm
        img1 = f'{W}/boot1.iso'; shutil.copy(BOOT, img1)
        lc = mt(img1, 'mtype', '::/loader/loader.conf').stdout
        open(f'{W}/loader.conf', 'w').write(re.sub(r'^default .*$', 'default rudolf-debug.conf', lc, flags=re.M))
        run_(['mcopy', '-o', '-i', f'{img1}@@{EOFF}', f'{W}/loader.conf', '::/loader/loader.conf'])   # (the first start only)
        def ex1(T):
            shutil.copy(f'{W}/rudolf-custom.efi', f'{T}/t/rudolf-custom.efi'); os.makedirs(f'{T}/t/cust')
            for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{CUST}/{n}', f'{T}/t/cust/{n}')
        D1 = os.path.abspath('d160r1.img'); make_drive(D1, img1, extra=ex1); os.remove(img1)
        V1 = f'{W}/vars1.fd'; shutil.copy(f'{PARTS}/OVMF_VARS_4M.fd', V1)   # (no keys: Secure Boot off)
        vm, g, up = boot('first', D1, V1)
        ok('0. installed from the boot image: starts from its drive through the boot menu (rudolf.efi, Debug start)', up, 'not up')
        if not up: raise SystemExit
        o = set_var(g, 'LoaderEntryDefault', 'rudolf-debug.conf')
        o = run(g, STATUS, 120)
        ok('...through the menu (rudolf.boot=uefi), slot a, Secure Boot off', re.search(r'^SLOT=a$', o, re.M) and 'rudolf.boot=uefi' in o and 'rudolf.debug=1' in o, o)
        okr = wait_in(g, '[ -e /persist/os/system/r/complete ]', 600, 'system/r')
        o = run(g, STATUS + "; cat /persist/os/system/r/official; cat /run/rudolf/espcheck.log 2>&1; ip route; tail -3 /run/rudolf/update.log", 120)
        ok('1. the first start (the desktop answered): the official recovery system made from slot a', okr and 'official recovery system: release' in o, o)
        ok('2. both start-up files on the drive are the official ones (signed by Rudolf OS: sbverify with its certificate)', 'start=official recovery=official' in o, o)
        ok(f'3. the EFI image: {ESECS * 512 / 1048576:.1f} MiB of the 32 MiB an El Torito entry can name', ESECS < 0xFFFF and ESECS * 512 < (30 << 20), ESECS)
        o = run(g, f"{ESPM} && f=/mnt/esp/EFI/rudolf/recovery.efi && s=$(stat -c %s $f) && printf 'X' | dd of=$f bs=1 seek=$((s / 2)) conv=notrunc 2>/dev/null; sync; umount /mnt/esp; "
                   "echo 3 > /proc/sys/vm/drop_caches; rm /persist/os/system/r/desktop.sfs; sync; /usr/lib/rudolf/espcheck status", 120)
        ok('...recovery.efi changed (one byte, as anything writing to the drive could) is seen: recovery=other', 'recovery=other' in o, o)
        stop(vm, g)
        # ---- the next start: system/r made again; the boot image downloaded, checked, staged
        vm, g, up = boot('repair1', D1, V1)
        okp = up and wait_in(g, '[ -s /persist/os/bootimg/pending ]', 900, 'staged')
        o = run(g, STATUS + "; cat /run/rudolf/espcheck.log; cat /persist/os/bootimg/pending; ls -1 /persist/os/system/r | cat; grep -h '\"k\":\"boot\"' /persist/var/log/health.log | tail -4; tail -5 /run/rudolf/update.log", 120)
        ok('4. at the next start: system/r made again from the official system (its deleted layer back)', up and 'made again from this official system' in o and re.search(r'^desktop\.sfs$', o, re.M), o)
        ok('5. ...the release\'s boot image downloaded from GitHub, its checksum and both start-up files checked, staged for the next start (said in System health)', okp
           and f'serial={SER}' in o and 'are written at the next start' in o and 'not all the official ones' in o, o)
        stop(vm, g)
        vm, g, up = boot('written1', D1, V1)
        o = run(g, STATUS + "; cat /persist/os/bootimg/result 2>&1; ls /persist/os/bootimg 2>&1; grep -i 'boot files' /persist/var/log/boot-decisions.log | tail -3", 120) if up else 'not up'
        ok('6. the start after: the boot files written before anything else, recovery.efi the official one again, and the computer starts through the menu',
           up and 'start=official recovery=official' in o and 'rudolf.boot=uefi' in o, o)
        o = run(g, f"{ESPM} && rm /mnt/esp/EFI/rudolf/recovery.efi; sync; umount /mnt/esp; echo 3 > /proc/sys/vm/drop_caches; /usr/lib/rudolf/espcheck status", 60) if up else ''
        ok('7. recovery.efi deleted: seen (recovery=missing)', 'recovery=missing' in o, o)
        stop(vm, g)
        vm, g, up = boot('repair2', D1, V1)
        okp = up and wait_in(g, '[ -s /persist/os/bootimg/pending ]', 900, 'staged')
        o = run(g, "cat /run/rudolf/espcheck.log", 60) if up else ''
        stop(vm, g)
        vm, g, up = boot('written2', D1, V1)
        o += run(g, STATUS, 120) if up else 'not up'
        ok('8. ...downloaded and staged at the next start, written at the one after: recovery.efi back', okp and up and 'start=official recovery=official' in o, o)
        if not up: raise SystemExit
        # ---- Developer mode, a custom build and its own rudolf.efi; then Return to official
        o = run(g, f"rudolf-update developer on '{DEVPUB}' https://dev.example/rudolf; d=/persist/os/system/b; rm -rf $d; mkdir -p $d; "
                   "for n in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do ln /persist/os/system/a/$n $d/$n; done; cp /persist/t/cust/manifest.* $d/; "
                   f"printf 'serial {SER + 1}\\nversion {mv('version')}-dev\\n' > $d/RELEASE; echo 1 > $d/complete; "
                   "printf 'good=a\\nprev=\\nnext=b\\ntries=2\\nfailed=\\n' > /persist/os/system/state; "
                   f"{ESPM} && cp /persist/t/rudolf-custom.efi /mnt/esp/EFI/rudolf/rudolf.efi; sync; umount /mnt/esp; echo 3 > /proc/sys/vm/drop_caches; /usr/lib/rudolf/espcheck status", 120)
        ok('9. Developer mode on (a TEST key), a custom build in slot b and its own rudolf.efi (start=other)', 'Developer mode is on' in o and 'start=other recovery=official' in o, o)
        stop(vm, g)
        vm, g, up = boot('custom', D1, V1)
        o = run(g, STATUS + "; sleep 30; cat /run/rudolf/espcheck.log 2>&1", 120) if up else 'not up'
        ok('10. the custom build starts (slot b, its own rudolf.efi): Custom in its status; its start-up files left as they are (not an official system)',
           up and re.search(r'^SLOT=b$', o, re.M) and '"signer":"custom"' in o and 'start=other' in o and '"recoveryOfficial":true' in o, o)
        if not up: raise SystemExit
        o = run(g, "rudolf-update return-official keep; echo rc=$?; cat /persist/os/system/state", 300)
        okp = wait_in(g, '[ -s /persist/os/bootimg/pending ]', 900, 'staged')
        o += run(g, "cat /run/rudolf/espcheck.log; cat /persist/os/bootimg/pending", 60)
        ok('11. Return to official (keep): Developer mode off, the official system from system/r into slot a to start next, the official boot files staged',
           'Returning to the official Rudolf OS' in o and re.search(r'^next=a$', o, re.M) and okp, o)
        stop(vm, g)
        # (this start with this tree's start-up script over the release kernel's own, given as a second initramfs: the
        #  released one drops Return to official's mark when it tries the official system, fixed in initramfs/init)
        vm, g, up = boot('returned', D1, V1, kernel_cmd='quiet loglevel=3 consoleblank=0 rudolf.boot=uefi rudolf.debug=1', initrd=OVCPIO)
        o = run(g, "sleep 30; " + STATUS + "; cat /persist/etc/rudolf/extra-keys 2>&1; grep -h -e 'Cleared' -e 'Back to the official' /run/rudolf/update.log", 180) if up else 'not up'
        if up and '"developer":false' not in o: o += run(g, "sleep 60; rudolf-update status; ls /persist/os/system", 120)
        ok('12. ...the next starts: the official rudolf.efi written again, the official system (slot a), Official, Developer mode off, the custom build\'s slot cleared',
           up and re.search(r'^SLOT=a$', o, re.M) and 'start=official recovery=official' in o and '"signer":"official"' in o and '"developer":false' in o
           and '"b":{"complete":false' in o and 'No such file' in o and 'Cleared the custom build in slot b' in o, o)
        stop(vm, g); vm = None
        os.remove(D1)

def part2():
        global vm
        D2 = os.path.abspath('d160r2.img')
        def ex2(T):
            open(f'{T}/os/update.conf', 'w').write('auto=0\n')
        make_drive(D2, None, official=False, extra=ex2, dmb=3200)
        CMD = 'quiet loglevel=3 consoleblank=0 rudolf.debug=1'
        V2 = f'{W}/vars2.fd'; shutil.copy(f'{PARTS}/OVMF_VARS_4M.fd', V2)
        vm, g, up = boot('neverofficial', D2, V2, kernel_cmd=CMD)
        o = run(g, STATUS, 120) if up else 'not up'
        ok('13. a custom build alone (never the official one): no official recovery system', up and re.search(r'"signer":"(custom|unknown)"', o) and re.search(r'"recoveryOfficial":false', o), o)
        if up:
            o = run(g, f"rudolf-update developer on '{DEVPUB}' https://dev.example/rudolf", 60)
            okr = wait_in(g, '[ -e /persist/os/system/r/complete ]', 1500, 'system/r downloaded')
            o += run(g, "cat /run/rudolf/recovery-fetch.log; cat /persist/os/system/r/official /persist/os/system/r/RELEASE; head -3 /persist/os/system/r/manifest.txt; ls -l /persist/os/system/r; rudolf-update status", 120)
            ok('14. Developer mode turned on: the official recovery system downloaded from the official address, checked with the official key',
               'being downloaded from the official address' in o and okr and 'downloaded from the official address' in o and '"recoveryOfficial":true' in o, o)
            stop(vm, g)
            vm, g, up = boot('recovery', D2, V2, kernel_cmd=CMD + ' rudolf.recovery=1')
            o = run(g, "echo SLOT=$(cat /etc/rudolf/slot); cat /etc/rudolf/cmdline; head -3 /persist/os/system/r/RELEASE", 60) if up else 'not up'
            ok('15. ...Recovery mode starts it (slot r, the official release)', up and re.search(r'^SLOT=r$', o, re.M), o)
        stop(vm, g); vm = None
        os.remove(D2)

def part3():
        # (no network here: the official system checks its start-up files at each start and would put back the
        #  official ones, signed with the Rudolf OS key, which this firmware (TEST certificate only) would refuse.
        #  Putting them back is part 1's; here espcheck finds no network and leaves them, saying so)
        global vm
        STUB = os.environ.get('STUB', '/usr/lib/systemd/boot/efi/linuxx64.efi.stub')
        run_(['openssl', 'req', '-new', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '30', '-subj', '/CN=Rudolf OS TEST Secure Boot (t160r)/',
              '-keyout', f'{W}/sb.key', '-out', f'{W}/sb.crt', '-addext', 'keyUsage=critical,digitalSignature', '-addext', 'extendedKeyUsage=codeSigning'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        img3 = f'{W}/boot3.iso'; shutil.copy(BOOT, img3)
        env = dict(os.environ, RUDOLF_UKI_TEST_CMDLINE='rudolf.debug=1')
        run_([sys.executable, f'{ISO}/systools/mkuki.py', STUB, f'{REL}/vmlinuz', mv('version'), f'{W}/uki.efi'], env=env, stdout=subprocess.DEVNULL)
        run_([sys.executable, f'{ISO}/systools/mkuki.py', STUB, f'{REL}/vmlinuz', mv('version'), f'{W}/ruki.efi', '--recovery', f'{ISO}/trusted-keys.txt'],
             env=dict(os.environ, RUDOLF_UKI_TEST_CMDLINE='console=ttyS0 rudolf.debug=1'), stdout=subprocess.DEVNULL)
        run_(['mcopy', '-n', '-i', f'{img3}@@{EOFF}', '::/EFI/BOOT/grubx64.efi', f'{W}/sdboot.efi']); run_(['sbattach', '--remove', f'{W}/sdboot.efi'])
        for src, dst in (('uki.efi', 'EFI/rudolf/rudolf.efi'), ('ruki.efi', 'EFI/rudolf/recovery.efi'), ('sdboot.efi', 'EFI/BOOT/grubx64.efi')):
            run_(['sbsign', '--key', f'{W}/sb.key', '--cert', f'{W}/sb.crt', '--output', f'{W}/s-{src}', f'{W}/{src}'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            r = mt(img3, 'mcopy', '-o', f'{W}/s-{src}', f'::/{dst}'); out('put', dst, r.returncode, r.stderr[-300:])
        V3 = f'{W}/vars3.fd'
        run_(['virt-fw-vars', '--input', f'{PARTS}/OVMF_VARS_4M.ms.fd', '--output', V3, '--add-db', '0f4b3c3a-0000-4000-8000-526f646f6c66', f'{W}/sb.crt', '--secure-boot'], stdout=subprocess.DEVNULL)
        def ex3(T):
            os.makedirs(f'{T}/t/cust')
            for n in ('manifest.txt', 'manifest.sig'): shutil.copy(f'{CUST}/{n}', f'{T}/t/cust/{n}')
        D3 = os.path.abspath('d160r3.img'); make_drive(D3, img3, extra=ex3); os.remove(img3)
        vm, g, up = boot('sb', D3, V3, net=False)
        o = run(g, STATUS + "; cat /sys/kernel/security/lockdown; dmesg | grep -i -m3 'secure boot'; ls /dev/mapper; cat /run/rudolf/unverified 2>&1", 120) if up else 'not up'
        ok('16. Secure Boot on (the TEST certificate in db): the official release starts, locked down, its layers through dm-verity', up and '[integrity]' in o
           and re.search(r'(?i)secure boot enabled', o) and re.search(r'^SLOT=a$', o, re.M), o)
        if not up: raise SystemExit
        okr = wait_in(g, '[ -e /persist/os/system/r/complete ]', 600, 'system/r')
        o = run(g, f"rudolf-update developer on '{DEVPUB}' https://dev.example/rudolf; d=/persist/os/system/b; rm -rf $d; mkdir -p $d; "
                   "for n in vmlinuz base.sfs chromium.sfs desktop.sfs modules.sfs; do ln /persist/os/system/a/$n $d/$n; done; cp /persist/t/cust/manifest.* $d/; "
                   f"printf 'serial {SER + 1}\\nversion {mv('version')}-dev\\n' > $d/RELEASE; echo 1 > $d/complete; "
                   "printf 'good=a\\nprev=\\nnext=b\\ntries=2\\nfailed=\\n' > /persist/os/system/state; sync; cat /persist/os/system/state", 120)
        ok('17. Developer mode on under Secure Boot (a TEST key), the official recovery system there, a custom build in slot b to start next', okr and 'Developer mode is on' in o, o)
        stop(vm, g)
        vm, g, up = boot('sbcustom', D3, V3, net=False)
        o = run(g, STATUS + "; tail -25 /persist/var/log/boot-decisions.log", 120) if up else 'not up'
        ok('18. ...the custom build does not start under Secure Boot (its layers are not signed by the official key): back to the official system by itself, said',
           up and re.search(r'^SLOT=a$', o, re.M) and re.search(r'^failed=b$', o, re.M) and 'slot b given up' in o and '"signer":"official"' in o, o)
        if not up: raise SystemExit
        o = run(g, "RU_NOESP=1 rudolf-update return-official keep; echo rc=$?; cat /persist/os/system/state", 300)
        stop(vm, g)
        vm, g, up = boot('sbreturned', D3, V3, net=False)
        o2 = run(g, "sleep 30; " + STATUS, 180) if up else 'not up'
        ok('19. Return to official under Secure Boot: the official system from system/r starts checked (slot b), Developer mode off, Official',
           'Returning to the official Rudolf OS' in o and up and re.search(r'^SLOT=b$', o2, re.M) and '"signer":"official"' in o2 and '"developer":false' in o2, o + o2)
        if up:
            set_var(g, 'LoaderEntryOneShot', 'rudolf-recovery.conf')
            stop(vm, g)
            vm, g, up = boot('sbrecovery', D3, V3, net=False)
            time.sleep(5); s = open(vm.serlog, errors='replace').read()
            o = run(g, "echo SLOT=$(cat /etc/rudolf/slot); cat /etc/rudolf/cmdline; cat /sys/kernel/security/lockdown", 60) if up else 'not up'
            ok('20. Recovery mode under Secure Boot: recovery.efi checks the official system/r with the official key and starts it (locked down)',
               re.search(r'starting the official recovery system \(release \d+, checked\)', s) and (not up or re.search(r'^SLOT=r$', o, re.M)), o + s[-3000:])
        stop(vm, g); vm = None
        os.remove(D3)

vm = None
try:
    for k, f in (('1', part1), ('2', part2), ('3', part3)):
        if k not in ONLY: continue
        out(f'======== part {k}')
        try: f()
        except SystemExit: out(f'part {k} stopped early')
        try: vm and vm.stop()
        except Exception: pass
        vm = None
        for d in ('d160r1.img', 'd160r2.img', 'd160r3.img'):
            if os.path.exists(d): os.remove(d)
finally:
    try: vm and vm.stop()
    except Exception: pass
    shutil.rmtree(W, ignore_errors=True)
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
