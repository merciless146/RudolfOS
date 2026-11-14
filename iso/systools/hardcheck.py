#!/usr/bin/env python3
"""The release check of the hardening flags (docs/plan-1.5.9.md 15.5) on built programs, read with readelf:
  a position-independent program (ELF type DYN), full RELRO (PT_GNU_RELRO and BIND_NOW), the stack protector
  (__stack_chk_fail), FORTIFY_SOURCE (the _chk functions), and control-flow protection in the program's property note
  (x86-64: IBT and SHSTK; ARM64: BTI and PAC).
  python3 hardcheck.py PROGRAM...       (a static program is found as such: static-pie, symbols from its own table)
Exit status 1 when one lacks any of them, naming what."""
import re, subprocess, sys


def readelf(args, f):
    return subprocess.run(['readelf', '-W'] + args + [f], capture_output=True, text=True).stdout


def check(f):
    h = readelf(['-h'], f); ph = readelf(['-l'], f); dyn = readelf(['-d'], f); notes = readelf(['-n'], f)
    if 'ELF' not in h: return ['not an ELF program']
    static = 'INTERP' not in ph
    syms = readelf(['--dyn-syms'] if not static else ['-s'], f)
    if static and not re.search(r'\bFUNC\b', syms): syms += readelf(['-s'], f)
    miss = []
    if not re.search(r'Type:\s+DYN', h): miss.append('PIE (it is not position-independent)')
    if 'GNU_RELRO' not in ph: miss.append('RELRO')
    if not (re.search(r'\(FLAGS\)\s.*BIND_NOW', dyn) or re.search(r'\(FLAGS_1\)\s.*\bNOW\b', dyn)): miss.append('BIND_NOW (-z now)')
    if '__stack_chk_fail' not in syms: miss.append('the stack protector (-fstack-protector-strong)')
    if not re.search(r'__\w+_chk\b', syms.replace('__stack_chk_fail', '')): miss.append('FORTIFY_SOURCE')
    m = re.search(r'Machine:\s+(.*)', h); mach = m.group(1) if m else ''
    if 'X86-64' in mach or 'Advanced Micro Devices' in mach:
        if not re.search(r'x86 feature: .*IBT', notes) or not re.search(r'x86 feature: .*SHSTK', notes): miss.append('CET (-fcf-protection: IBT and SHSTK)')
    elif 'AArch64' in mach:
        if not re.search(r'AArch64 feature: .*BTI', notes) or not re.search(r'AArch64 feature: .*PAC', notes): miss.append('BTI and PAC (-mbranch-protection=standard)')
    return miss


def main(argv):
    bad = 0
    for f in argv:
        miss = check(f)
        if miss: bad += 1; print(f'{f}: built without {", ".join(miss)}')
        else: print(f'{f}: hardened (PIE, full RELRO, stack protector, FORTIFY_SOURCE, control-flow protection)')
    return 1 if bad else 0


if __name__ == '__main__': sys.exit(main(sys.argv[1:]))
