#!/usr/bin/env python3
"""Firmware that goes in the modules layer, next to the kernel that asks for it (Rudolf OS 1.2.3). GPL-2.0-or-later.

  fwextra.py list LINUX_FIRMWARE > firmware-extra.txt     write the list from a linux-firmware checkout
  fwextra.py copy LIST LINUX_FIRMWARE DEST                 copy the listed files (checked) and make the links
  fwextra.py sof SOF_BIN > firmware-sof.txt                write the sound processor list from a sof-bin checkout

The list (iso/firmware-extra.txt) pins every file by SHA-256, so a release always carries the same bytes wherever
linux-firmware was fetched from; "L path target" lines are the links linux-firmware's WHENCE describes (its
install script makes them; a git checkout has none). What is listed:
  - NVIDIA GSP firmware (570.144) for GeForce 20 series and newer. Without it nouveau runs these cards with no
    acceleration at all (no 3D, so Xorg has no glamor and everything is drawn by the processor). The booter
    files already sit in the base layer; these are the large GSP images and their boot loaders.
  - Realtek Ethernet (r8169) firmware: small fixes the driver loads for most of its chips.
  - AMD processor microcode (1.2.4): built into the kernel too (kconfig.sh, UCODE_DIR) so it applies at start-up.
  - Bluetooth: Realtek (1.5.1); Intel, MediaTek MT79xx and Qualcomm USB chips (1.5.4).
  - Speaker amplifiers (1.5.4): Cirrus CS35L41, CS35L54 and CS35L56, TI TAS2563 and TAS2781. Intel's sound processor firmware (SOF) is
    not in linux-firmware: firmware-sof.txt, from thesofproject/sof-bin (below).
  - USB Wi-Fi adapters whose drivers came in 1.2.7 (ath9k_htc, mt7601u, mt76x0u, mt76x2u), and in 1.5.3 the
    Ralink and MediaTek RT2800 chips (rt2800usb, rt2800pci). The firmware of the
    older Wi-Fi drivers sits in the base layer (wififw.py, at the full build).
"""
import hashlib, os, re, shutil, sys

# Intel's sound processor firmware (SOF, 1.5.4), from thesofproject/sof-bin. Each laptop gets what the kernel picks
# by default (sound/soc/sof/intel/pci-*.c):
#   - 11th to 13th generation Core (Tiger Lake, Elkhart Lake, Alder Lake, Raptor Lake): IPC3, SOF's last IPC3
#     release (2.2: the firmware in intel/sof, its topologies, 2.2 to 2.2.13, in intel/sof-tplg);
#   - Core Ultra (Meteor Lake, Arrow Lake, Lunar Lake, Panther Lake, Wildcat Lake): IPC4, release 2.15 (firmware
#     in intel/sof-ipc4/<platform>, topologies 2.15.1 in intel/sof-ipc4-tplg; Meteor and Arrow Lake look in
#     intel/sof-ace-tplg, a link to it, as distributions make it).
# Only the Intel-signed firmware (what laptops sold with Windows accept), and only topologies for these platforms
# and the generic ones. Lines are "F sha256 path-in-firmware path-in-sof-bin".
SOF_IPC3 = ['tgl', 'tgl-h', 'ehl', 'adl', 'adl-s', 'adl-n', 'rpl', 'rpl-s']
SOF_IPC4 = ['mtl', 'arl', 'arl-s', 'lnl', 'ptl', 'wcl']
SOF_TPLG3 = r'sof-(tgl|ehl|adl|rpl|hda-generic).*\.tplg'
SOF_TPLG4_SKIP = r'sof-(imx8mp|nvl|tgl|adl|rpl)\b.*'     # boards not built, and 11th-13th generation (IPC3 above)

PATTERNS = [r'rtl_nic/.+', r'amd-ucode/microcode_amd(_fam[0-9a-f]+h)?\.bin',
            r'nvidia/[^/]+/gsp/(gsp|bootloader|gen_bootloader|scrubber|fmc)-570\.144\.bin',
            r'nvidia/[^/]+/gsp', r'nvidia/ad10[3-7]', r'nvidia/gb102', r'nvidia/gb20[3-7]',   # whole-folder links (ga104/gsp -> ../ga102/gsp, ad104 -> ad102, gb203 -> gb202)
            # USB Wi-Fi adapters (1.2.7): Atheros AR9271/AR7010 (ath9k_htc), MediaTek MT7601U, MT7610U, MT7612U (mt76x2u)
            r'htc_(9271|7010)\.fw', r'ath9k_htc/htc_(9271|7010)-1\.4\.0\.fw', r'(mediatek/)?mt7601u\.bin', r'mediatek/mt7610u\.bin',
            r'(mediatek/)?mt7662(_rom_patch)?\.bin',
            # USB and PCIe Wi-Fi (1.5.3): Ralink and MediaTek RT2800 chips (rt2800usb, rt2800pci)
            r'rt2870\.bin', r'rt2860\.bin', r'rt3290\.bin',
            # Bluetooth (1.5.1): Realtek (every chip: small)
            r'rtl_bt/.+',
            # Bluetooth (1.5.4): Intel (every ibt- file: AX200 to BE201, and the newest PCIe-attached chips), MediaTek
            # MT7921/MT7922/MT7925, Qualcomm chips on USB (btusb loads qca/rampatch_usb_* and qca/nvm_usb_* itself)
            r'intel/ibt-.+', r'mediatek/(mt79[0-9a-z]+/)?BT_RAM_CODE_MT79.+', r'qca/([^/]+/)?(rampatch|nvm)_usb_.+',
            # Speaker amplifiers (1.5.4): Cirrus CS35L41, CS35L54 and CS35L56 (their tuning per laptop), TI TAS2563 and TAS2781 (and the
            # top-level names the TAS2781 HDA driver asks for)
            r'cirrus/cs35l(41|54|56).*', r'ti/audio/tas2(563|781)/.+', r'TAS2XXX.+', r'TIAS2781.+']


def want(p):
    return any(re.fullmatch(x, p) for x in PATTERNS)


def whence(fw):
    files, links = [], []
    for line in open(os.path.join(fw, 'WHENCE'), errors='replace'):
        m = re.match(r'(File|RawFile):\s*(\S+)', line)
        if m and want(m.group(2)):
            files.append(m.group(2))
        m = re.match(r'Link:\s*(\S+)\s*->\s*(\S+)', line)
        if m and want(m.group(1)):
            links.append((m.group(1), m.group(2)))
    return files, links


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def cmd_list(fw):
    files, links = whence(fw)
    print('# firmware in the modules layer (iso/systools/fwextra.py); made by: fwextra.py list LINUX_FIRMWARE')
    for f in files:
        print('F', sha(os.path.join(fw, f)), f)
    for a, b in links:
        print('L', a, b)


def cmd_sof(sb):
    def rel(p):
        r = os.path.relpath(os.path.realpath(os.path.join(sb, p)), os.path.realpath(sb))
        if r.startswith('..') or not os.path.isfile(os.path.join(sb, r)):
            sys.exit('fwextra: sof-bin has no file %s' % p)
        return r
    out = []
    for plat in SOF_IPC3:
        out.append(('intel/sof/sof-%s.ri' % plat, rel('v2.2.x/sof-v2.2/intel-signed/sof-%s.ri' % plat)))
    def v(d):
        m = re.fullmatch(r'sof-tplg-v2\.2((\.\d+)*)', d)
        return tuple(int(x) for x in m.group(1).split('.')[1:]) if m else None
    tp = {}
    for d in sorted((d for d in os.listdir(os.path.join(sb, 'v2.2.x')) if v(d) is not None), key=v):   # later releases win
        for f in os.listdir(os.path.join(sb, 'v2.2.x', d)):
            if re.fullmatch(SOF_TPLG3, f):
                tp[f] = rel('v2.2.x/%s/%s' % (d, f))
    out += [('intel/sof-tplg/' + f, tp[f]) for f in sorted(tp)]
    for plat in SOF_IPC4:
        out.append(('intel/sof-ipc4/%s/sof-%s.ri' % (plat, plat), rel('v2.15.x/sof-ipc4-v2.15/%s/intel-signed/sof-%s.ri' % (plat, plat))))
    d = 'v2.15.x/sof-ipc4-tplg-v2.15.1'
    for f in sorted(os.listdir(os.path.join(sb, d))):
        if f.endswith('.tplg') and not re.fullmatch(SOF_TPLG4_SKIP, f):
            out.append(('intel/sof-ipc4-tplg/' + f, rel(d + '/' + f)))
    print('# Intel sound processor firmware (SOF) in the modules layer (iso/systools/fwextra.py); made by: fwextra.py sof SOF_BIN')
    for dst, src in out:
        print('F', sha(os.path.join(sb, src)), dst, src)
    print('L intel/sof-ace-tplg sof-ipc4-tplg')


def cmd_copy(lst, fw, dest):
    n = size = 0; bad = []
    for line in open(lst):
        t = line.split()
        if not t or t[0].startswith('#'):
            continue
        if t[0] == 'F':
            # 1.5.4: a fourth field is where the file is in the checkout (firmware-sof.txt), when not at the same path
            h, f = t[1], t[2]
            src = os.path.join(fw, t[3] if len(t) > 3 else f)
            if not os.path.isfile(src):
                sys.exit('fwextra: %s is missing from %s' % (f, fw))
            if sha(src) != h:
                # 1.5.1: every file that differs is reported with the hash it has now, then the build stops
                bad.append('fwextra: %s is not the listed file (listed %s, %s has %s; make the list again)' % (f, h, fw, sha(src)))
                continue
            d = os.path.join(dest, f); os.makedirs(os.path.dirname(d), exist_ok=True)
            shutil.copyfile(src, d); os.chmod(d, 0o644); n += 1; size += os.path.getsize(d)
        elif t[0] == 'L':
            d = os.path.join(dest, t[1]); os.makedirs(os.path.dirname(d), exist_ok=True)
            if os.path.lexists(d):
                os.remove(d)
            os.symlink(t[2], d)
    if bad:
        sys.exit('\n'.join(bad))
    print('fwextra: %d files, %.1f MB' % (n, size / 1048576))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'list':
        cmd_list(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == 'sof':
        cmd_sof(sys.argv[2])
    elif len(sys.argv) == 5 and sys.argv[1] == 'copy':
        cmd_copy(*sys.argv[2:])
    else:
        sys.exit(__doc__)
