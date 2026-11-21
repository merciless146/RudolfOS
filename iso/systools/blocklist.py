#!/usr/bin/env python3
# Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 5): Oracle's ad and tracker blocking list, compiled from
# filter lists in Adblock Plus syntax (Rudolf's own iso/systools/blocklists/rudolf.txt; EasyList and EasyPrivacy use the same
# syntax) into the compact form Oracle checks on this computer (desktop/orablock61.js, the shell, rudolf-webkit):
#   {"v": VERSION, "src": [the lists], "b": [domains blocked], "tp": [domains blocked from other sites only], "ex": [domains never blocked]}
# Only whole-domain rules are taken: ||domain^ (and ||domain^$third-party), @@||domain^ (and with $third-party). Rules with other
# options (resource types, $domain=, $document, ...), path and pattern rules, and element hiding (##) are left out for now: a domain
# rule is answered with a set lookup for each request, and leaving out a rule can only block less, never more.
#   python3 blocklist.py LIST.txt [LIST.txt ...] > desktop/orablock61.json
import json, re, sys

DOM = re.compile(r'^[a-z0-9]([a-z0-9-]*[a-z0-9])?(\.[a-z0-9]([a-z0-9-]*[a-z0-9])?)+$')

def compile_lists(paths):
    b, tp, ex, src = set(), set(), set(), []
    for p in paths:
        txt = open(p, encoding='utf-8').read()
        m = re.search(r'^! *Version: *(\S+)', txt, re.M); src.append(p.split('/')[-1] + (' ' + m.group(1) if m else ''))
        for line in txt.splitlines():
            line = line.strip()
            if not line or line[0] in '![' or '##' in line or '#@#' in line or '#?#' in line or '#$#' in line: continue
            exc = line.startswith('@@')
            if exc: line = line[2:]
            if not line.startswith('||'): continue
            rule, _, opts = line[2:].partition('$')
            if not rule.endswith('^'): continue
            d = rule[:-1].lower()
            if not DOM.match(d): continue
            o = [x.strip() for x in opts.split(',') if x.strip()] if opts else []
            if o not in ([], ['third-party'], ['3p']): continue
            third = bool(o)
            if exc: ex.add(d)
            elif third: tp.add(d)
            else: b.add(d)
    tp -= b
    return {'v': 1, 'src': src, 'b': sorted(b), 'tp': sorted(tp), 'ex': sorted(ex)}

if __name__ == '__main__':
    if len(sys.argv) < 2: sys.exit(__doc__ or 'python3 blocklist.py LIST.txt [...]')
    sys.stdout.write(json.dumps(compile_lists(sys.argv[1:]), separators=(',', ':')) + '\n')
