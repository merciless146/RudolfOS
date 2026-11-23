#!/usr/bin/env python3
# Rudolf OS 1.6.2: every workflow file loads with no key given twice in one place.
# GitHub refuses a workflow with a key given twice and runs nothing at all. On 2026-11-22 two merges each added the
# release workflow's "Tools" step its own `run:` line, and the release workflow (the 6-hourly security releases and
# every beta) failed to start for a day without a job showing it. Run by deadcheck.py; exit 1 lists each duplicate.
import glob, os, sys
try:
    import yaml
except ImportError:
    print('wfkeys: skipped (PyYAML missing: pip install pyyaml)'); sys.exit(0)
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
class _L(yaml.SafeLoader): pass
def _map(loader, node, deep=False):
    seen = set()
    for k, _ in node.value:
        key = loader.construct_object(k, deep=deep)
        if key in seen:
            raise yaml.constructor.ConstructorError(None, None, f'the key {key!r} given twice', k.start_mark)
        seen.add(key)
    return yaml.SafeLoader.construct_mapping(loader, node, deep)
_L.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, _map)
bad = []
for f in sorted(glob.glob(os.path.join(ROOT, '.github', 'workflows', '*.y*ml'))):
    try:
        yaml.load(open(f), Loader=_L)
    except yaml.YAMLError as e:
        bad.append(f'{os.path.relpath(f, ROOT)}: ' + ' '.join(str(e).split()))
for b in bad:
    print('wfkeys:', b)
print(f'wfkeys: {"FAILED" if bad else "ok"} ({len(bad)} found)')
sys.exit(1 if bad else 0)
