# Rudolf OS 1.6.0 in QEMU (docs/plan-1.6.0.md "Before stable" 15): the owner's Vivobook of 2026-11-16, step by step, on the
# real releases, with the firmware's entries watched before and after each step. The Vivobook's firmware (AMI) adds no
# entry for a drive Rudolf OS is installed on: it starts the drive only through BootOrder's entries, "Rudolf OS" (which
# rudolfd writes) and "Drive" (made in its setup on 2026-10-29: NVMe(...)/CDROM(1,0x52EC,0x1E000)/\EFI\BOOT\BOOTX64.EFI,
# docs/plan-1.4.1.md Stage C 4; rudolfd's entry took that path in 1.4.2). OVMF does add its own entry for every drive it
# finds; here a start through any entry but those two counts as the Vivobook's setup screen (BootCurrent says which
# entry started), and no entry of OVMF's starts the computer while the result is judged.
#   the drive: Install here's layout with 1.4.1's boot image in front (the Vivobook's, "Get new boot files" of
#   2026-10-29), an NVMe drive with the Vivobook's EUI-64; RUDOLFDATA with 1.5.9 stable in slot a, and 1.6.0 (the beta
#   the owner updated to) in slot b
#   1  1.5.9 stable starts (the Debug start, for the serial shell); "Drive" and a Windows Boot Manager entry made as on
#      the Vivobook, "Drive" to start next (BootNext), the Native desktop chosen, the update to 1.6.0 staged
#   2  1.6.0's first start, through "Drive": rudolfd's "Rudolf OS" takes Drive's path (CDROM(...)); espcheck finds
#      start-up files that are not all official (1.4.1's have no recovery.efi), downloads the release's boot image from
#      GitHub and stages it, and System health says nothing of it (rudolfd refused health events of kind "boot");
#      Settings' engine switch to Standard, then Restart (the owner's steps, through rudolfd's API as the native shell does)
#   3  the restart: the start-up writes the boot image over the area in front of RUDOLFDATA: the EFI image is now at
#      another place, so "Rudolf OS" and "Drive" name nothing; QEMU is stopped as the firmware starts (the Vivobook:
#      its setup screen, Boot Override empty)
#   4  a start from a USB stick (1.6.0's boot.iso, as written by Rufus): the installed system starts; the entries are
#      still the old ones (nothing puts them right); with FIX, this tree's rudolfd puts the entry right
#   5  a start without the stick: through "Rudolf OS" or not (the Vivobook's setup screen)
#   With FIX (a folder with this tree's rudolfd, espcheck, bootfiles.sh), also:
#   6  Settings' "Check this drive's start-up" (bootfiles.sh rescue): it says what it found and changed
#   7  the official start-up files put back (bootfiles.sh files): into the existing EFI partition, never the area; the
#      entry checked; the next start through "Rudolf OS"
#   python3 t160b_t.py REL-DIR OLD-REL-DIR OLD-BOOT-ISO   (REL-DIR: 1.6.0's files with boot.iso; OLD-REL-DIR: 1.5.9's;
#   OLD-BOOT-ISO: rudolf-os-1.4.1-boot.iso; SB_PARTS (OVMF_CODE_4M.secboot.fd, OVMF_VARS_4M.fd), QEMU, QEMU_ACCEL; root)
import os, sys, time, re, subprocess, shutil, struct
HERE = os.path.dirname(os.path.abspath(__file__)); QT = os.path.dirname(HERE); ISO = os.path.dirname(QT)
sys.path.insert(0, HERE); sys.path.insert(0, QT)
import sbvm
from guest import Guest
from ser import Ser
REL, OLD, OLDBOOT = (os.path.abspath(a) for a in sys.argv[1:4]); PARTS = os.environ.get('SB_PARTS', '/nonexistent'); FIX = os.environ.get('B160_FIX', '')
LOG = open('t160b.txt', 'w'); R = []; T0 = time.time()
def out(*a):
    s = ' '.join(str(x) for x in a); s = f'[{int(time.time() - T0):5d}s] ' + s; print(s, flush=True); LOG.write(s + '\n'); LOG.flush()
def ok(n, c, d=''):
    R.append(bool(c)); out(('PASS ' if c else 'FAIL ') + n + (' - ' + str(d)[-2500:] if d != '' and not c else ''))
def man(d): return open(f'{d}/manifest.txt').read()
def mv(d, k): return re.search(rf'^{k} (.*)$', man(d), re.M).group(1)
MEM = int(os.environ.get('MEM', '4096')); W = os.path.abspath('w160b'); shutil.rmtree(W, ignore_errors=True); os.makedirs(W)
G = '8be4df61-93ca-11d2-aa0d-00e098032b8c'; LG = '4a67b082-0a4c-41cf-b6c7-440b29bb8c4f'
EUI = '0026b738227247d5'
def run_(a, **k): return subprocess.run(a, check=True, **k)
# ---- the boot images' EFI images, as the firmware names them (EDK2's ElTorito.c: the catalogue's LBA and count x 4)
def cdrom(iso):
    with open(iso, 'rb') as f:
        f.seek(17 * 2048); bv = f.read(2048); f.seek(struct.unpack('<I', bv[0x47:0x4b])[0] * 2048); c = f.read(2048)
    for i in range(2, 64):
        e = c[32 * i:32 * i + 32]
        if e[0] == 0x88: return (1, struct.unpack('<I', e[8:12])[0] * 4, struct.unpack('<H', e[6:8])[0] * 4)
CD_OLD, CD_NEW = cdrom(OLDBOOT), cdrom(f'{REL}/boot.iso')
out('1.4.1\'s boot image: CDROM(%d,0x%X,0x%X); 1.6.0\'s: CDROM(%d,0x%X,0x%X)' % (CD_OLD + CD_NEW))
# ---- device paths and boot entries (UEFI 2.10, 10.3)
def node(t, s, b): return bytes([t, s]) + struct.pack('<H', len(b) + 4) + b
END = bytes([0x7f, 0xff, 4, 0])
def fnode(p): return node(4, 4, (p + '\0').encode('utf-16-le'))
def cdnode(e, s, n): return node(4, 2, struct.pack('<IQQ', e, s, n))
def loadopt(title, path, att=1): return struct.pack('<IH', att, len(path)) + (title + '\0').encode('utf-16-le') + path
def dp_text(p):
    o = []; i = 0
    while i + 4 <= len(p):
        t, s, L = p[i], p[i + 1], struct.unpack('<H', p[i + 2:i + 4])[0]; q = p[i + 4:i + L]
        if L < 4: break
        if t == 0x7f: break
        if t == 2 and s == 1: o.append('PciRoot(0x%X)' % struct.unpack('<I', q[4:8]))
        elif t == 1 and s == 1: o.append('Pci(0x%X,0x%X)' % (q[1], q[0]))
        elif t == 3 and s == 0x17: o.append('NVMe(0x%X,%s)' % (struct.unpack('<I', q[:4])[0], '-'.join('%02X' % x for x in q[4:12])))
        elif t == 3 and s == 5: o.append('USB(0x%X,0x%X)' % (q[0], q[1]))
        elif t == 3 and s == 0x12: o.append('Sata(0x%X)' % struct.unpack('<H', q[:2]))
        elif t == 4 and s == 1: o.append('HD(%d,%s,0x%X,0x%X)' % (struct.unpack('<I', q[:4])[0], 'MBR' if q[36] == 1 else 'GPT', *struct.unpack('<QQ', q[4:20])))
        elif t == 4 and s == 2: o.append('CDROM(0x%X,0x%X,0x%X)' % struct.unpack('<IQQ', q[:20]))
        elif t == 4 and s == 4: o.append(q.decode('utf-16-le').rstrip('\0'))
        elif t == 4 and s == 6: o.append('FvFile(...)')
        elif t == 4 and s == 7: o.append('Fv(...)')
        else: o.append(f'Path({t},{s})')
        i += L
    return '/'.join(o)
def entry(b):
    att, pl = struct.unpack('<IH', b[:6]); t = b[6:].decode('utf-16-le', 'replace').split('\0')[0]; i = 6 + 2 * (len(t) + 1)
    return {'title': t, 'active': bool(att & 1), 'path': b[i:i + pl], 'text': dp_text(b[i:i + pl])}
DUMP = (f"V=/sys/firmware/efi/efivars; for f in $V/Boot*-{G} $V/LoaderEntryDefault-{LG} $V/LoaderEntrySelected-{LG}; do [ -e $f ] && echo \"VAR ${{f##*/}} $(od -An -tx1 -v $f | tr -d ' \\n')\"; done; echo DUMPED")
def vars_of(o):
    v = {}
    for m in re.finditer(r'^VAR (\S+?)-[0-9a-f-]{36} ([0-9a-f]+)$', o, re.M): v[m.group(1)] = bytes.fromhex(m.group(2))[4:]
    return v
def show(v, what):
    lines = [f'--- the firmware\'s variables, {what}:']
    for k in sorted(v):
        b = v[k]
        if k in ('BootOrder', 'BootCurrent', 'BootNext'): lines.append(f'  {k} = ' + ','.join('%04X' % x for x in struct.unpack('<%dH' % (len(b) // 2), b)))
        elif re.match(r'^Boot[0-9A-F]{4}$', k): e = entry(b); lines.append(f'  {k} "{e["title"]}"{"" if e["active"] else " (inactive)"} {e["text"]}')
        else: lines.append(f'  {k} = ' + b.decode('utf-16-le', 'replace').rstrip('\0'))
    out('\n'.join(lines)); return v
def cur_entry(v):
    c = struct.unpack('<H', v['BootCurrent'])[0] if 'BootCurrent' in v else -1; k = 'Boot%04X' % c
    return c, (entry(v[k]) if k in v else {'title': '?', 'text': '?', 'path': b''})
def by_title(v, t): return [(k, entry(b)) for k, b in sorted(v.items()) if re.match(r'^Boot[0-9A-F]{4}$', k) and entry(b)['title'] == t]
def has_cd(e, cd): return cdnode(*cd) in e['path']

# ---- the drive (as t160o's make_drive): Install here's layout
def slot_files(T, s, rel):
    d = f'{T}/os/system/{s}'; os.makedirs(d)
    for n in ('vmlinuz', 'base.sfs', 'chromium.sfs', 'desktop.sfs', 'modules.sfs', 'manifest.txt', 'manifest.sig'): os.link(f'{rel}/{n}', f'{d}/{n}') if os.stat(rel).st_dev == os.stat(W).st_dev else shutil.copy(f'{rel}/{n}', f'{d}/{n}')
    open(f'{d}/RELEASE', 'w').write(f"serial {mv(rel, 'serial')}\nversion {mv(rel, 'version')}\nreleased {mv(rel, 'released')}\nchromium {mv(rel, 'chromium')}\nkernel {mv(rel, 'kernel')}\nnotes test\n")
    open(f'{d}/complete', 'w').write('1\n')
DRV = os.path.abspath('d160b.img'); DMB = 4000; START = 262144
def make_drive():
    T = f'{W}/tree'; os.makedirs(f'{T}/home/rudolf'); os.makedirs(f'{T}/etc/rudolf')
    slot_files(T, 'a', OLD); slot_files(T, 'b', REL)
    open(f'{T}/os/system/state', 'w').write('good=a\nprev=\nnext=\ntries=0\nfailed=\n')
    open(f'{T}/os/update.conf', 'w').write('auto=0\n')
    subprocess.run(['chown', '-R', '1000:1000', f'{T}/home/rudolf'], check=True)
    D = f'{W}/data'; run_(['truncate', '-s', f'{DMB}M', D]); run_(['mkfs.ext4', '-q', '-F', '-O', 'encrypt', '-L', 'RUDOLFDATA', '-m', '1', '-d', T, D]); shutil.rmtree(T)
    img = f'{W}/boot141.iso'; shutil.copy(OLDBOOT, img)
    # (the first start only: the Debug start, for the serial shell; LoaderEntryDefault keeps it after that)
    off = None
    with open(img, 'rb') as f:
        f.seek(17 * 2048); bv = f.read(2048); f.seek(struct.unpack('<I', bv[0x47:0x4b])[0] * 2048); c = f.read(2048)
        for i in range(2, 64):
            if c[32 * i] == 0x88: off = struct.unpack('<I', c[32 * i + 8:32 * i + 12])[0] * 2048; break
    lc = subprocess.run(['mtype', '-i', f'{img}@@{off}', '::/loader/loader.conf'], capture_output=True, text=True).stdout
    open(f'{W}/loader.conf', 'w').write(re.sub(r'^default .*$', 'default rudolf-debug.conf', lc, flags=re.M))
    run_(['mcopy', '-o', '-i', f'{img}@@{off}', f'{W}/loader.conf', '::/loader/loader.conf'])
    with open(DRV, 'wb') as f:
        f.truncate((START + DMB * 2048 + 2048) * 512)
        with open(img, 'rb') as i: shutil.copyfileobj(i, f)
        f.seek(440); f.write(bytes.fromhex('6a5ba9d4')); f.seek(446); f.write(bytes(16))
        f.seek(478); f.write(bytes([0, 0xfe, 0xff, 0xff, 0x83, 0xfe, 0xff, 0xff]) + struct.pack('<II', START, DMB * 2048))
        f.seek(510); f.write(b'\x55\xaa'); f.seek(START * 512)
        with open(D, 'rb') as d: shutil.copyfileobj(d, f)
    os.remove(D); os.remove(img)
def drive_cd():
    """the El Torito UEFI entry of the image now in front of the drive"""
    with open(DRV, 'rb') as f: head = f.read(64 << 20)
    p = f'{W}/head.iso'; open(p, 'wb').write(head); r = cdrom(p); os.remove(p); return r

class TSer(Ser):
    def __init__(s, path, logf): super().__init__(path); s.lf = open(logf, 'a'); s.seen = ''
    def read(s, t=1.0):
        o = super().read(t); s.lf.write(o); s.lf.flush(); s.seen = (s.seen + o)[-200000:]; return o
N = [0]; VARS = f'{W}/vars.fd'; shutil.copy(f'{PARTS}/OVMF_VARS_4M.fd', VARS)   # (no keys: Secure Boot off; the Debug start)
def vm_start(tag, stick=False, net=True):
    N[0] += 1; name = f't160b{N[0]}{tag}'
    # (use-intel-id: Linux ignores the IDs of QEMU's own NVMe controller (NVME_QUIRK_BOGUS_NID), so its EUI-64 would be
    #  missing in Linux while OVMF names the drive with it; with Intel's IDs both see the Vivobook's EUI-64)
    extra = ['-drive', f'file={DRV},format=raw,if=none,id=nv', '-device', 'nvme,id=nvc,serial=KINGSTON160B,use-intel-id=on',
             '-device', f'nvme-ns,bus=nvc,drive=nv,nsid=1,eui64=0x{EUI}']
    if net: extra += ['-netdev', 'user,id=n1', '-device', 'e1000,netdev=n1']
    extra += ['-drive', f'file={W}/rel.img,format=raw,if=none,id=rl,readonly=on', '-device', 'virtio-blk-pci,drive=rl']
    if FIX: extra += ['-drive', f'file={W}/fix.img,format=raw,if=none,id=fx', '-device', 'virtio-blk-pci,drive=fx']
    if stick: extra += ['-device', 'qemu-xhci,id=xh', '-drive', f'file={W}/stick.img,format=raw,if=none,id=u0', '-device', 'usb-storage,bus=xh.0,drive=u0,removable=on,bootindex=0']
    vm = sbvm.VM(name, PARTS, [], secure=True, vars_from=VARS, mem=MEM, extra=extra); vm.serlog = os.path.join(vm.dir, 'serial.txt')
    g = Guest.__new__(Guest); g.N = vm.name; g.s = TSer(vm.ser_path, vm.serlog); g.q = vm.q; g.s.read(0.3)
    return vm, g
def vm_stop(vm):
    vm.stop(); time.sleep(2); shutil.copy(vm.vars, VARS)   # (the firmware's variables for the next start)
def up(g, wait=900, old_id=None):
    end = time.time() + wait
    while time.time() < end:
        try:
            o = g.sh('cat /proc/sys/kernel/random/boot_id; echo ready', 8)
            if 'ready' in o:
                bid = o.strip().split('\n')[0].strip()
                if re.match(r'^[0-9a-f-]{36}$', bid) and bid != old_id: return bid
        except OSError: pass
        time.sleep(4)
    out('the serial line:', open(g.s.lf.name, errors='replace').read()[-3000:]); return None
def run(g, c, t=60, show_out=True):
    f = os.path.join(W, 'c.sh'); open(f, 'w').write(c + '\n'); g.put(f, '/tmp/c.sh', '644')
    o = g.sh('sh /tmp/c.sh 2>&1', t)
    if show_out: out('$', c[:200].replace('\n', ' ; ')); out(o.rstrip()[-3000:])
    return o
def dump(g, what): return show(vars_of(run(g, DUMP, 60, False)), what)
def put_var(g, name, guid, data):
    p = f'{W}/{name}.var'; open(p, 'wb').write(struct.pack('<I', 7) + data); g.put(p, f'/tmp/{name}.var', '644')
    return run(g, f"V=/sys/firmware/efi/efivars; f=$V/{name}-{guid}; [ -e $f ] && chattr -i $f 2>/dev/null; cat /tmp/{name}.var > $f; echo rc=$?", 30, False)
TOK = "T=$(busybox wget -q -O- http://127.0.0.1:8080/ | grep -o 'token:\"[0-9a-f]*' | cut -d'\"' -f2)"
def api(g, path, t=60): return run(g, f"{TOK}; busybox wget -q -O- --header \"X-Rudolf-Token: $T\" --post-data '' 'http://127.0.0.1:8080/api/{path}' 2>&1; echo", t)
HEALTH = "grep -h '\"k\":\"boot\"' /persist/var/log/health.log | tail -20; echo; grep -h 'memory group\\|system API' /persist/var/log/health.log | tail -5"
FIXSH = ("FX=$(blkid | grep 'LABEL=\"B160FIX\"' | cut -d: -f1 | head -1); mkdir -p /tmp/fx; mountpoint -q /tmp/fx || mount -o ro $FX /tmp/fx; X=/run/b160x; mkdir -p $X; "
         "mountpoint -q $X || mount -t tmpfs -o exec,mode=755 none $X; for n in rudolfd espcheck bootfiles.sh; do cp /tmp/fx/$n $X/$n; chmod 755 $X/$n; done; "
         "mountpoint -q /usr/bin/rudolfd || mount --bind $X/rudolfd /usr/bin/rudolfd; for n in espcheck bootfiles.sh; do mountpoint -q /usr/lib/rudolf/$n || mount --bind $X/$n /usr/lib/rudolf/$n; done; ls -la $X")

vm = None
try:
    make_drive()
    # the USB stick: 1.6.0's boot image as Rufus writes it (DD)
    shutil.copy(f'{REL}/boot.iso', f'{W}/stick.img'); run_(['truncate', '-s', '64M', f'{W}/stick.img'])
    # the release's boot image on a disk of its own: a stand-in for GitHub when QEMU's network cannot reach it
    os.makedirs(f'{W}/relfiles'); [shutil.copy(f'{REL}/{n}', f'{W}/relfiles/{n}') for n in ('boot.iso', 'boot.iso.sha256')]
    run_(['truncate', '-s', '96M', f'{W}/rel.img']); run_(['mkfs.ext4', '-q', '-F', '-L', 'B160REL', '-d', f'{W}/relfiles', f'{W}/rel.img'])
    if FIX:
        run_(['truncate', '-s', '32M', f'{W}/fix.img']); run_(['mkfs.ext4', '-q', '-F', '-L', 'B160FIX', '-d', FIX, f'{W}/fix.img'])
    # ======== 1: 1.5.9 stable
    vm, g = vm_start('159', net=False)
    b1 = up(g); ok('1. 1.5.9 stable starts from the drive (1.4.1\'s boot files, the Debug start)', b1)
    if not b1: raise SystemExit
    o = run(g, "echo SLOT=$(cat /etc/rudolf/slot); cat /usr/share/rudolf/VERSION; cat /etc/rudolf/persist; ls /sys/block; cat /sys/block/nvme0n1/eui")
    v = dump(g, 'after 1.5.9\'s first start')
    rud = by_title(v, 'Rudolf OS')
    ok('1a. 1.5.9\'s rudolfd made "Rudolf OS" for the drive (NVMe with the Vivobook\'s EUI-64), first in BootOrder', rud and 'NVMe(0x1,00-26-B7-38-22-72-47-D5)' in rud[0][1]['text']
       and struct.unpack('<H', v['BootOrder'][:2])[0] == int(rud[0][0][4:], 16), rud)
    if not rud: raise SystemExit
    pre = rud[0][1]['path'][:-4]
    drive = pre + cdnode(*CD_OLD) + fnode('\\EFI\\BOOT\\BOOTX64.EFI') + END
    put_var(g, 'Boot0040', G, loadopt('Drive', drive))
    put_var(g, 'Boot0041', G, loadopt('Windows Boot Manager', node(4, 1, struct.pack('<IQQ', 1, 2048, 204800) + bytes(16) + bytes([2, 2])) + fnode('\\EFI\\Microsoft\\Boot\\bootmgfw.efi') + END))
    bo = [int(rud[0][0][4:], 16), 0x40, 0x41] + [x for x in struct.unpack('<%dH' % (len(v['BootOrder']) // 2), v['BootOrder']) if x not in (int(rud[0][0][4:], 16), 0x40, 0x41)]
    put_var(g, 'BootOrder', G, struct.pack('<%dH' % len(bo), *bo)); put_var(g, 'BootNext', G, struct.pack('<H', 0x40))
    put_var(g, 'LoaderEntryDefault', LG, 'rudolf-debug.conf\0'.encode('utf-16-le'))
    run(g, "printf 'good=a\\nprev=\\nnext=b\\ntries=2\\nfailed=\\n' > /persist/os/system/state; mkdir -p /persist/etc; echo native > /persist/etc/rudolf-desktop-engine; sync; cat /persist/os/system/state")
    dump(g, 'the Vivobook\'s entries made ("Drive", Windows), Drive to start next; the update to 1.6.0 staged')
    run(g, "(/usr/lib/rudolf/shutdown reboot > /dev/null 2>&1 &); echo restarting", 30)
    # ======== 2: 1.6.0's first start, through Drive
    b2 = up(g, 900, b1); ok('2. the update to 1.6.0 starts (through "Drive", as on the Vivobook)', b2)
    if not b2: raise SystemExit
    run(g, "echo SLOT=$(cat /etc/rudolf/slot); cat /usr/share/rudolf/VERSION; cat /run/rudolf/desktop-engine; /usr/lib/rudolf/espcheck status")
    v = dump(g, '1.6.0\'s first start'); c, ce = cur_entry(v); rud = by_title(v, 'Rudolf OS')
    ok('2a. started through "Drive" (BootCurrent); rudolfd\'s "Rudolf OS" took its path: ' + ('CDROM(%d,0x%X,0x%X)' % CD_OLD), ce['title'] == 'Drive' and rud and has_cd(rud[0][1], CD_OLD), (c, ce['text'], rud))
    okp = False
    for i in range(40):
        o = run(g, "[ -s /persist/os/bootimg/pending ] && echo STAGED; cat /run/rudolf/espcheck.log 2>/dev/null | tail -3", 30, False)
        if 'STAGED' in o or 'could not' in o: okp = 'STAGED' in o; break
        if i == 18 and 'not all the official ones' in o: break   # (3 minutes: no download yet)
        time.sleep(10)
    if not okp:
        # QEMU's network did not reach GitHub: the same repair, given the release's boot image from a disk (what the
        # download would have brought), as the Vivobook's would have run once its Wi-Fi was up
        run(g, "ip -4 addr | grep inet; ip route; cat /etc/resolv.conf; pkill -f 'espcheck boot'; RL=$(blkid | grep 'LABEL=\"B160REL\"' | cut -d: -f1); mkdir -p /mnt/rl; mount -o ro $RL /mnt/rl; ls -la /mnt/rl; "
               "printf '#!/bin/sh\\ncp /mnt/rl/${1##*/} \"$2\"\\n' > /tmp/fetch.sh; chmod 755 /tmp/fetch.sh; rm -f /persist/os/esp-repair/tried; "
               "ESP_FETCH=/tmp/fetch.sh /usr/lib/rudolf/espcheck repair b >> /run/rudolf/espcheck.log 2>&1; echo rc=$?", 600)
        o = run(g, "[ -s /persist/os/bootimg/pending ] && echo STAGED", 30, False); okp = 'STAGED' in o
    o = run(g, "cat /run/rudolf/espcheck.log; cat /persist/os/bootimg/pending; echo; " + HEALTH, 60)
    ok('2b. espcheck: the start-up files are not all official (1.4.1\'s have no recovery.efi): the release\'s boot image downloaded and staged for the next start',
       okp and 'not all the official ones' in o and 'are written at the next start' in o, o)
    ok('2c. ...and System health has nothing of it (no event of kind "boot": rudolfd refused them) - as on the Vivobook', '"k":"boot"' not in o, o)
    # the owner's steps: Settings › Desktop engine › Standard, then Restart (as the native shell's power dialog does)
    o = api(g, 'desktop/engine?engine=chromium'); time.sleep(5)
    o += run(g, "cat /persist/etc/rudolf-desktop-engine 2>&1; " + HEALTH, 60)
    ok('2d. the engine switched to Standard (Settings), for the next start', 'saved' in o and 'No such file' in o, o)
    v2 = dump(g, 'before the restart')
    ok('2e. nothing so far wrote BootNext or removed an entry (the engine switch and the session touch no EFI variable)', 'BootNext' not in v2 and by_title(v2, 'Drive') and by_title(v2, 'Rudolf OS'), '')
    api(g, 'power?action=reboot&t=files=1', 30)
    # ======== 3: the restart: the start-up writes the staged boot image, then restarts into the firmware. QEMU is told to
    # stop at that second restart (the Vivobook's firmware found nothing to start there; OVMF would start its own entry)
    end = time.time() + 900; cdn = CD_OLD
    while time.time() < end and cdn == CD_OLD:
        time.sleep(0.5)
        try: cdn = drive_cd()
        except Exception: pass
    out('the drive\'s start-up area changed: CDROM(%d,0x%X,0x%X)' % cdn); vm.q.cmd('set-action', reboot='shutdown')
    try: vm.p.wait(300)
    except Exception: out('QEMU did not stop by itself')
    vm_stop(vm); vm = None; cdn = drive_cd()
    ok('3. at the restart the start-up wrote 1.6.0\'s boot image over the area in front of RUDOLFDATA: its EFI image is now at CDROM(%d,0x%X,0x%X), the entries name CDROM(%d,0x%X,0x%X)' % (cdn + CD_OLD),
       cdn == CD_NEW and cdn != CD_OLD, cdn)
    # ======== 4: a start from the USB stick
    vm, g = vm_start('stick', stick=True)
    b4 = up(g, 900); ok('4. started from the USB stick: the installed system starts', b4)
    if not b4: raise SystemExit
    run(g, "echo SLOT=$(cat /etc/rudolf/slot); cat /etc/rudolf/media; cat /etc/rudolf/persist; cat /persist/os/bootimg/result; grep -i 'boot files' /persist/var/log/boot-decisions.log | tail -3")
    v = dump(g, 'started from the USB stick'); c, ce = cur_entry(v); rud = by_title(v, 'Rudolf OS'); drv = by_title(v, 'Drive')
    ok('4a. the entries for the drive still name its start-up files\' old place, CDROM(%d,0x%X,0x%X): the start from the stick put nothing right' % CD_OLD,
       'USB' in ce['text'] and rud and has_cd(rud[0][1], CD_OLD) and drv and has_cd(drv[0][1], CD_OLD), (ce, rud, drv))
    if FIX:
        o = run(g, FIXSH + "; P=$(pidof rudolfd | tr ' ' '\\n' | sort -n | head -1); kill $P; sleep 2; (setsid /usr/bin/rudolfd >> /run/rudolf/rudolfd.log 2>&1 < /dev/null &); sleep 4; pidof rudolfd; " + HEALTH, 120)
        v = dump(g, 'this tree\'s rudolfd started (as at a start from the stick)'); rud = by_title(v, 'Rudolf OS'); pv = by_title(v, 'Rudolf OS (previous)')
        bo = struct.unpack('<%dH' % (len(v['BootOrder']) // 2), v['BootOrder'])
        ok('4b. FIX: this tree\'s rudolfd, started from the stick: "Rudolf OS" written again for CDROM(%d,0x%X,0x%X), read back, first; the old one kept second; said in System health' % CD_NEW,
           rud and has_cd(rud[0][1], CD_NEW) and pv and bo[0] == int(rud[0][0][4:], 16) and bo[1] == int(pv[0][0][4:], 16) and 'Rudolf OS (previous)' in o and '"k":"boot"' in o, (o, rud, pv, bo))
    run(g, "sync", 30); vm_stop(vm); vm = None
    # ======== 5: without the stick
    vm, g = vm_start('nostick')
    b5 = up(g, 900); v = dump(g, 'a start without the stick') if b5 else {}
    c, ce = cur_entry(v) if v else (-1, {'title': '?', 'text': ''})
    if FIX: ok('5. FIX: without the stick the computer starts through "Rudolf OS" (no entry of OVMF\'s needed)', b5 and ce['title'] == 'Rudolf OS' and has_cd(ce, CD_NEW), (c, ce))
    else: ok('5. without the stick: "Rudolf OS" and "Drive" do not start it (OVMF started its own entry, Boot%04X %s; the Vivobook has none: its setup screen)' % (c, ce['title']),
             b5 and ce['title'] not in ('Rudolf OS', 'Drive'), (c, ce))
    if FIX and b5:
        # ======== 6: Settings' check of the start-up (the rescue), and 7: the start-up files put back into the EFI partition
        run(g, FIXSH, 60)
        o = run(g, "/usr/lib/rudolf/bootfiles.sh rescue; echo rc=$?", 180)
        ok('6. FIX: the check of this drive\'s start-up (Settings): the EFI partition, its files and the entry checked, said; nothing needed changing', 'rc=0' in o and 'Nothing needed changing' in o and 'BOOTX64.EFI' in o, o)
        o = run(g, "mkdir -p /mnt/esp; mount -t vfat /dev/nvme0n1p2 /mnt/esp && f=/mnt/esp/EFI/rudolf/recovery.efi && s=$(stat -c %s $f) && printf 'X' | dd of=$f bs=1 seek=$((s / 2)) conv=notrunc 2>/dev/null; sync; umount /mnt/esp; "
                   "echo 3 > /proc/sys/vm/drop_caches; /usr/lib/rudolf/espcheck status; dd if=/dev/nvme0n1 bs=512 count=1 2>/dev/null | od -An -tx1 | tail -5 > /tmp/mbr.before; "
                   "mkdir -p /persist/os/esp-repair; "
                   "RL=$(blkid | grep 'LABEL=\"B160REL\"' | cut -d: -f1); mkdir -p /mnt/rl; mountpoint -q /mnt/rl || mount -o ro $RL /mnt/rl; cp /mnt/rl/boot.iso /persist/os/esp-repair/boot.iso; ls -la /persist/os/esp-repair; "
                   "/usr/lib/rudolf/bootfiles.sh files /persist/os/esp-repair/boot.iso; echo rc=$?; echo 3 > /proc/sys/vm/drop_caches; /usr/lib/rudolf/espcheck status; "
                   "dd if=/dev/nvme0n1 bs=512 count=1 2>/dev/null | od -An -tx1 | tail -5 > /tmp/mbr.after; cmp /tmp/mbr.before /tmp/mbr.after && echo MBR-SAME; " + HEALTH, 600)
        ok('7. FIX: recovery.efi changed, then the official start-up files put back into the existing EFI partition (each beside the old one, checked, renamed): both official; the partition table and disk signature unchanged',
           'rc=0' in o and o.count('start=official recovery=official') >= 1 and 'MBR-SAME' in o and drive_cd() == CD_NEW, o)
        run(g, "sync", 30); vm_stop(vm); vm = None
        vm, g = vm_start('afterfiles')
        b7 = up(g, 900); v = dump(g, 'the start after the files were put back') if b7 else {}
        c, ce = cur_entry(v) if v else (-1, {'title': '?', 'text': ''})
        o = run(g, "/usr/lib/rudolf/espcheck status; ls /mnt 2>&1 | head -2", 60) if b7 else ''
        ok('7b. FIX: the next start through "Rudolf OS", the start-up files official', b7 and ce['title'] == 'Rudolf OS' and 'start=official recovery=official' in o, (c, ce, o))
finally:
    try: vm and vm.stop()
    except Exception: pass
    for f in (DRV,):
        try: os.remove(f)
        except OSError: pass
    shutil.rmtree(W, ignore_errors=True)
out(f'{sum(R)}/{len(R)} passed'); sys.exit(0 if R and all(R) else 1)
