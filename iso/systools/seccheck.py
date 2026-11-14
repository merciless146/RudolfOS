#!/usr/bin/env python3
"""Security fixes arrive soon (docs/plan-1.5.9.md 15.6), run once a day by .github/workflows/security-check.yml.

Compares the Chromium and the kernel of the latest release (the newest of the stable and beta channels' manifests on
the gh-pages branch) with the newest ones Ubuntu has published:
  - Chromium: Ubuntu's chromium snap, stable channel (amd64), the one source releases take it from (plan 11.1);
  - the kernel: Ubuntu's mainline kernel builds (kernel.ubuntu.com/mainline), which follow kernel.org's stable point
    releases; only the release's own series counts (6.18.x for 6.18.63): a point release is fixes, a new series is not
    a security update.
When either is newer, one GitHub issue "Security update available: …" says so for the owner, who decides whether to
build a beta. While one is open (label security-update, or the older chromium-update), it is updated, never a second
one. It never builds, releases, promotes or dispatches anything.

  python3 seccheck.py            (GITHUB_TOKEN, GITHUB_REPOSITORY; SECCHECK_GITHUB, SECCHECK_SNAP, SECCHECK_MAINLINE
                                  name stand-ins for tests; SECCHECK_DRY=1 prints what it would do)
"""
import base64, json, os, re, sys, time, urllib.error, urllib.request

GH = os.environ.get('SECCHECK_GITHUB', 'https://api.github.com')
SNAP = os.environ.get('SECCHECK_SNAP', 'https://api.snapcraft.io/v2/snaps/info/chromium?fields=version')
MAINLINE = os.environ.get('SECCHECK_MAINLINE', 'https://kernel.ubuntu.com/mainline/')
REPO = os.environ.get('GITHUB_REPOSITORY', 'merciless146/RudolfOS'); TOKEN = os.environ.get('GITHUB_TOKEN', '')
DRY = os.environ.get('SECCHECK_DRY') == '1'
LABEL = 'security-update'; OLD_LABELS = ('security-update', 'chromium-update')


def get(url, headers=None, tries=4, data=None, method=None):
    last = None
    for i in range(tries):
        try:
            q = urllib.request.Request(url, data=data, method=method, headers=headers or {})
            with urllib.request.urlopen(q, timeout=60) as r: return r.read()
        except urllib.error.HTTPError as e:
            if e.code < 500 and e.code != 429: raise
            last = e
        except (urllib.error.URLError, OSError) as e: last = e
        if i + 1 < tries: time.sleep(int(os.environ.get('SECCHECK_RETRY', '15')))
    raise last


def gh(path, body=None, method=None):
    h = {'Accept': 'application/vnd.github+json', 'User-Agent': 'rudolf-seccheck'}
    if TOKEN: h['Authorization'] = 'Bearer ' + TOKEN
    d = json.dumps(body).encode() if body is not None else None
    if d: h['Content-Type'] = 'application/json'
    return json.loads(get(GH + path, h, data=d, method=method) or b'null')


def vkey(v): return [int(x) for x in re.findall(r'\d+', v)]


def newer(a, b): return bool(a and b) and vkey(a) > vkey(b)


def manifest(channel):
    """the channel's manifest as {key: value}, or None"""
    try: j = gh(f'/repos/{REPO}/contents/{channel}/manifest.txt?ref=gh-pages')
    except urllib.error.HTTPError as e:
        if e.code == 404: return None
        raise
    m = {}
    for line in base64.b64decode(j.get('content', '')).decode('utf8', 'replace').split('\n'):
        k, _, v = line.partition(' ')
        if k in ('serial', 'version', 'released', 'chromium', 'kernel') and k not in m: m[k] = v.strip()
    return m if m.get('serial') else None


def latest_release():
    ms = [(c, m) for c in ('stable', 'beta') for m in [manifest(c)] if m]
    if not ms: return None, None
    return max(ms, key=lambda cm: int(cm[1]['serial']) if cm[1]['serial'].isdigit() else 0)


def snap_chromium():
    j = json.loads(get(SNAP, {'Snap-Device-Series': '16', 'User-Agent': 'rudolf-seccheck'}))
    for c in j.get('channel-map', []):
        ch = c.get('channel', {})
        if ch.get('name') == 'stable' and ch.get('architecture') == 'amd64': return str(c.get('version', '')).split('-')[0]
    return ''


def mainline_kernel(series):
    """the newest point release of SERIES ("6.18") that Ubuntu's mainline builds list, '' when none"""
    html = get(MAINLINE, {'User-Agent': 'rudolf-seccheck'}).decode('utf8', 'replace')
    vs = set(re.findall(r'href="v(%s(?:\.\d+)?)/"' % re.escape(series), html))
    vs = [v for v in vs if re.fullmatch(r'\d+\.\d+(\.\d+)?', v)]
    return max(vs, key=vkey) if vs else ''


def main():
    ch, rel = latest_release()
    if not rel: print('::warning::no release manifest could be read'); return 0
    rc = rel.get('chromium', ''); rk = re.sub(r'-rudolf$', '', rel.get('kernel', ''))
    print(f"latest release: {rel.get('version')} ({ch}, released {rel.get('released')}): Chromium {rc}, kernel {rk}")
    found, lines = [], []
    try: sc = snap_chromium()
    except Exception as e: sc = ''; print(f'::warning::Ubuntu\'s chromium snap could not be read: {e}')
    if newer(sc, rc): found.append(f'Chromium {sc}'); lines.append(f"- **Chromium {sc}** in Ubuntu's chromium snap (stable); the release has {rc}. Chromium's stable updates carry security fixes.")
    series = '.'.join(rk.split('.')[:2])
    try: uk = mainline_kernel(series) if series else ''
    except Exception as e: uk = ''; print(f'::warning::Ubuntu\'s mainline kernel list could not be read: {e}')
    if newer(uk, rk): found.append(f'kernel {uk}'); lines.append(f"- **Linux {uk}** in Ubuntu's mainline kernel builds (kernel.org's stable {series} series); the release has {rk}. Stable point releases are fixes, security fixes among them.")
    print(f'Ubuntu: Chromium {sc or "?"}, kernel {uk or "?"} ({series or "?"})')
    if not found: print('the latest release has the newest Chromium and kernel'); return 0
    title = 'Security update available: ' + ', '.join(found)
    body = '\n'.join([f"The latest release, **{rel.get('version')}** ({ch}, released {rel.get('released')}), has Chromium {rc} and Linux {rk}. Newer:", ''] + lines + ['',
        'A beta with them: run the release workflow (Actions › Rudolf OS release › Run workflow on main); it takes the newest snap and the kernel version it is given. Promotion stays your choice. This check builds, releases and promotes nothing.', '',
        f"_Checked {time.strftime('%Y-%m-%d %H:%M UTC', time.gmtime())} by the security check workflow (docs/plan-1.5.9.md 15.6). This issue is updated while it is open; close it once a release has them._"])
    open_ = []
    for lb in OLD_LABELS:
        open_ += [i for i in gh(f'/repos/{REPO}/issues?state=open&labels={lb}&per_page=20') if 'pull_request' not in i]
    open_ += [i for i in gh(f'/repos/{REPO}/issues?state=open&per_page=100') if 'pull_request' not in i and i.get('title', '').startswith('Security update available:')]
    seen = {}
    for i in open_: seen.setdefault(i['number'], i)
    if seen:
        n = min(seen); cur = seen[n]
        if cur.get('title') == title and (cur.get('body') or '').split('_Checked')[0] == body.split('_Checked')[0]: print(f'#{n} already says so'); return 0
        if DRY: print(f'DRY: would update #{n}: {title}'); return 0
        gh(f'/repos/{REPO}/issues/{n}', {'title': title, 'body': body}, 'PATCH'); print(f'updated #{n}: {title}')
    else:
        if DRY: print(f'DRY: would open: {title}'); return 0
        j = gh(f'/repos/{REPO}/issues', {'title': title, 'body': body, 'labels': [LABEL]}, 'POST'); print(f"opened #{j.get('number')}: {title}")
    print(f'::notice title=Security update::{title}')
    return 0


if __name__ == '__main__': sys.exit(main())
