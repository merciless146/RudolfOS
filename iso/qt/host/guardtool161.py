#!/usr/bin/env python3
# 1.6.1 (docs/plan-1.6.1.md Part 26): builds guardtool, rudolfd's own Blitzen guards (rudolfd-guard.inc, rudolf-secrets.h)
# and its ai/dev handler (rudolfd-dev.inc's api_ai_dev, dev_reply, dev_token) taken from the source as they are, with
# stand-ins only for what is not theirs (the health log, the signed-in account, starting rudolf-dev without dropping to
# nobody). Never installed. Used by blzguard161_t.py (the red-team suite) and iso/qt/shell2/blzguard161_t.js (the page's).
#   python3 guardtool161.py OUT RUN PERSIST   builds OUT, with rudolfd's folders RUN (/run/rudolf) and PERSIST (/persist) there
#   OUT 'PATH?QUERY' < BODY           what rudolfd answers to that request: ai/guard, ai/authorize, ai/dev/..., ai/run
#                                     (its guard and its output's redaction, as rudolfd's ai/run has them)
#   OUT chat-in < REQUEST             ai/chat's request as the engine gets it (stderr: the kinds hidden)
#   OUT chat-out [KINDS] < ANSWER     the engine's whole HTTP answer as the page gets it
# Environment: GT_LOG (the health log's lines), GT_ACCT (the account), DEV_BIN
# (rudolf-dev, or a stand-in), RUDOLFD_DEVMODE_FILE (Developer mode on when it exists), GT_RUN_OUT (what ai/run's
# program printed). GPL-2.0-or-later.
import os, re, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__)); SYS = os.path.join(HERE, '..', '..', 'sys')
D = open(os.path.join(SYS, 'rudolfd.c')).read(); DEV = open(os.path.join(SYS, 'rudolfd-dev.inc')).read()

def fn(src, name):
    """a top-level function (static ... name(...) to its closing brace at the start of a line), or a one-line one"""
    lines = src.split('\n')
    for i, l in enumerate(lines):
        if l.startswith('static') and re.search(r'[ *]' + re.escape(name) + r'\(', l) and not l.rstrip().endswith(';'):
            if l.rstrip().endswith('}') and l.count('{') == l.count('}'): return l
            out = [l]
            for m in lines[i + 1:]:
                out.append(m)
                if m == '}': return '\n'.join(out)
    raise SystemExit(f'guardtool161: {name} not found')

def build(out, run, persist):
    a = D.index('typedef struct { char *b; size_t n, cap; } str;'); b = D.index('static int run(const char *cmd)')
    c = ['#define _GNU_SOURCE', *[f'#include <{h}>' for h in ('stdio.h', 'stdlib.h', 'string.h', 'stdarg.h', 'ctype.h', 'fcntl.h', 'unistd.h', 'errno.h', 'poll.h', 'signal.h', 'time.h', 'strings.h', 'sys/stat.h', 'sys/wait.h', 'sys/types.h')],
         f'#define RUN "{run}"', f'#define PERSIST "{persist}"', '#define DEV_DIR PERSIST "/os/dev"', '#define OFFICIAL_REPO "merciless146/RudolfOS"', '#define DL_DEVKEYS "/nonexistent"',
         '#define AU_PORTAL "/nonexistent/rudolf-portal"']
    c.append('static char token[33] = "0123456789abcdef0123456789abcdef";')
    c.append(D[a:b])
    for n in ('slurp', 'trim', 'write_str', 'urldecode', 'qparam'): c.append(fn(D, n))
    s = D.index('struct req {'); c.append(D[s:D.index('};', s) + 2])
    for n in ('send_all', 'reply', 'reply_json', 'reply_err', 'reply_ok', 'dl_devmode'): c.append(fn(D, n))
    c.append('static void ai_touch(void) {}')
    c.append('static void dl_dir(char *o, size_t n) { snprintf(o, n, "%s", PERSIST "/devlink"); }')
    c.append('static void health_add(const char *kind, const char *fmt, ...) { const char *f = getenv("GT_LOG"); FILE *o = f ? fopen(f, "a") : NULL; if (!o) return; va_list a; va_start(a, fmt); fprintf(o, "%s: ", kind); vfprintf(o, fmt, a); fputc(10, o); va_end(a); fclose(o); }')
    c.append('static int know_acct(char *a, size_t n) { const char *e = getenv("GT_ACCT"); snprintf(a, n, "%s", e ? e : ""); return e && *e; }')
    c.append('static void au_app_user(char *user, size_t un, char *env, size_t en) { snprintf(user, un, "rudolf"); if (en) *env = 0; }')
    # rudolf-dev started as rudolfd starts it, but never dropping to nobody (the test is not root on GitHub's runner)
    dr = fn(DEV, 'dev_run').replace('if (drop && (setgroups(0, NULL) || setgid(65534) || setuid(65534))) _exit(126);', '(void)drop;')
    c.append('#include <grp.h>')
    c.append('#define DEV_BIN (getenv("DEV_BIN"))')
    c.append('static size_t dev_len;')
    c.append(dr)
    c.append('#include "%s"' % os.path.join(SYS, 'rudolfd-guard.inc'))
    for n in ('dev_name_ok', 'dev_conf', 'dev_keyring', 'dev_token', 'dev_reply', 'api_ai_dev'): c.append(fn(DEV, n))
    c.append(r'''
static char *gt_in(size_t *n) { size_t cap = 1 << 16, k = 0; char *b = malloc(cap + 1); ssize_t g; while ((g = read(0, b + k, cap - k)) > 0) { k += g; if (k == cap) { cap *= 2; b = realloc(b, cap + 1); } } b[k] = 0; *n = k; return b; }
int main(int argc, char **argv)
{
	if (argc < 2) return 2; size_t n; char *body = gt_in(&n);
	if (!strcmp(argv[1], "chat-in")) { int k = 0; size_t on; char *x = guard_chat_in(body, n, &k, &on); if (!x) return 3; fwrite(x, 1, on, stdout); for (int b = 1; b <= SD_PERSONAL; b <<= 1) if (k & b) fprintf(stderr, "%s\n", sd_name(b)); return 0; }
	if (!strcmp(argv[1], "chat-out")) {
		int sv[2]; if (pipe(sv)) return 1; pid_t p = fork(); if (!p) { close(sv[0]); for (size_t i = 0; i < n; i += 5) { if (write(sv[1], body + i, i + 5 <= n ? 5 : n - i) < 0) {} } _exit(0); }
		close(sv[1]); int ik = 0; if (argc > 2) for (int b = 1; b <= SD_PERSONAL; b <<= 1) if (strstr(argv[2], sd_name(b))) ik |= b;
		guard_chat_out(sv[0], 1, ik); waitpid(p, NULL, 0); return 0; }
	struct req r = { .fd = 1 }; snprintf(r.method, sizeof r.method, "POST"); const char *q = strchr(argv[1], '?');
	snprintf(r.path, sizeof r.path, "%.*s", q ? (int)(q - argv[1]) : (int)strlen(argv[1]), argv[1]); snprintf(r.query, sizeof r.query, "%s", q ? q + 1 : "");
	r.body = n ? body : NULL; r.blen = n;
	if (!strcmp(r.path, "ai/guard")) api_ai_guard(&r);
	else if (!strcmp(r.path, "ai/authorize")) api_ai_authorize(&r);
	else if (!strncmp(r.path, "ai/dev/", 7)) api_ai_dev(&r, r.path + 7);
	else if (!strcmp(r.path, "ai/run")) {   /* (rudolfd's ai/run: its guard, then what the program printed, redacted) */
		if (guard_action(&r, "ai/run", "a program Blitzen runs", r.body, r.blen, "a program run on this computer")) return 0;
		char *out = getenv("GT_RUN_OUT") ? slurp(getenv("GT_RUN_OUT"), NULL) : NULL; int k = 0; size_t on; char *x = guard_text(out ? out : "", out ? strlen(out) : 0, &k, &on);
		guard_note("what a program Blitzen ran printed", k, 0); str j = {0}; sadd(&j, "{\"ok\":true,\"code\":0,\"out\":"); sjson(&j, x); sadd(&j, "}"); reply_json(1, &j); }
	else { reply_err(1, 404, "no such request in guardtool"); }
	return 0;
}
''')
    src = out + '.c'; open(src, 'w').write('\n'.join(c) + '\n')
    r = subprocess.run(['gcc', '-O1', '-w', '-o', out, src], capture_output=True, text=True)
    if r.returncode: sys.stderr.write(r.stderr[:4000]); raise SystemExit('guardtool161: did not build')

if __name__ == '__main__':
    build(*sys.argv[1:4])
