/* rudolf-ai: Blitzen's model engine (llama.cpp's llama-server) on Rudolf OS. Run by rudolfd as root.
 * GPL-2.0-or-later.
 *
 *   rudolf-ai status        installed=, model=, file=, size=, licence=, loaded=, persistent=, mem=, need= (key=value)
 *   rudolf-ai download      fetch blitzen.txt + .sig, check the signature against /etc/rudolf/rpk-keys, download the
 *                           model it names (resuming a broken download), check its SHA-256, put it in place
 *   rudolf-ai load          start the engine (if needed) and wait until it answers; prints "ready"
 *   rudolf-ai unload        stop the engine
 *   rudolf-ai remove        stop it and delete the model
 *
 * The engine runs as the "ai" user (uid 950, no login, no home folder), listens only on 127.0.0.1:8099 and
 * answers only requests carrying a random key that is kept in /run/rudolf/ai/key (root only); rudolfd adds it.
 * A watchdog stops it after IDLE seconds without a request (rudolfd touches /run/rudolf/ai/last).
 * Settings in /persist/os/blitzen.conf (all optional): url=, min_mem_mb=, idle_s=, threads=, ctx=
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <openssl/evp.h>

#define AIDIR "/persist/ai"
#define MODELS AIDIR "/models"
#define DLD AIDIR "/download"
#define CACHE AIDIR "/cache"
#define RUND "/run/rudolf/ai"
#define CONF "/persist/os/blitzen.conf"
#define KEYF "/etc/rudolf/rpk-keys"
#define ENGINE "/usr/lib/rudolf/llama-server"
#define DEFAULT_URL "https://github.com/merciless146/RudolfOS/releases/download/blitzen-1"
#define AI_UID 950
#define PORT 8099

static void die(const char *f, ...) { va_list a; va_start(a, f); fputs("rudolf-ai: ", stderr); vfprintf(stderr, f, a); fputc('\n', stderr); va_end(a); exit(1); }
static void say(const char *f, ...) { va_list a; va_start(a, f); vprintf(f, a); putchar('\n'); fflush(stdout); va_end(a); }
static char *slurp(const char *p, size_t *len)
{
	FILE *f = fopen(p, "rb"); if (!f) return NULL;
	size_t cap = 4096, n = 0; char *b = malloc(cap + 1); size_t r;
	while ((r = fread(b + n, 1, cap - n, f)) > 0) { n += r; if (n == cap) { cap *= 2; b = realloc(b, cap + 1); } }
	fclose(f); b[n] = 0; if (len) *len = n; return b;
}
static int write_str(const char *p, const char *s, mode_t m) { int f = open(p, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, m); if (f < 0) return -1; ssize_t w = write(f, s, strlen(s)); close(f); return w == (ssize_t)strlen(s) ? 0 : -1; }
static void mkdirs(const char *d, mode_t m) { char t[512]; snprintf(t, sizeof t, "%s", d); for (char *p = t + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(t, m); *p = '/'; } mkdir(t, m); }
static const char *field(char *t, const char *k, char *out, size_t n)
{
	size_t kl = strlen(k); out[0] = 0;
	for (char *l = t; l && *l; ) { char *e = strchr(l, '\n'); if (!strncmp(l, k, kl) && l[kl] == ' ') { size_t m = e ? (size_t)(e - l - kl - 1) : strlen(l + kl + 1); if (m >= n) m = n - 1; memcpy(out, l + kl + 1, m); out[m] = 0; break; } l = e ? e + 1 : NULL; }
	return out;
}
/* a setting from blitzen.conf ("key=value" lines) */
static const char *conf(const char *k, char *out, size_t n, const char *def)
{
	snprintf(out, n, "%s", def); char *t = slurp(CONF, NULL); if (!t) return out;
	size_t kl = strlen(k);
	for (char *sv, *l = strtok_r(t, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) if (!strncmp(l, k, kl) && l[kl] == '=') { snprintf(out, n, "%s", l + kl + 1); break; }
	free(t); return out;
}
static long conf_num(const char *k, long def) { char b[40], d[40]; snprintf(d, sizeof d, "%ld", def); conf(k, b, sizeof b, d); long v = atol(b); return v > 0 ? v : def; }
static int persistent(void) { return !access("/persist", W_OK) && !system("mountpoint -q /persist"); }
static long long mem_avail(void)
{
	char *m = slurp("/proc/meminfo", NULL); if (!m) return 0; char *p = strstr(m, "MemAvailable:"); long long kb = p ? atoll(p + 13) : 0; free(m); return kb * 1024;
}

/* ------------------------------------------------------------------ download and trust (as rudolf-debian) */
typedef void CURL;
static struct { void *h; CURL *(*init)(void); int (*setopt)(CURL *, int, ...); int (*perform)(CURL *); int (*getinfo)(CURL *, int, ...); const char *(*strerror)(int); } C;
static int lastpct = -1; static long long resume_at, total_size;
static int prog(void *u, long long dt, long long dn, long long ut, long long un)
{
	(void)u; (void)ut; (void)un; long long t = total_size ? total_size : dt, d = dn + resume_at;
	if (t > 1000000) { int pc = (int)(d * 100 / t); if (pc != lastpct) { lastpct = pc; say("progress %d", pc); } }
	return 0;
}
static size_t wcb(char *p, size_t s, size_t n, void *f) { return fwrite(p, s, n, f); }
static int download(const char *url, const char *path, int showprog, int resume)
{
	if (!C.h) {
		if (!(C.h = dlopen("libcurl.so.4", RTLD_NOW))) { char cmd[1200]; snprintf(cmd, sizeof cmd, "wget -q -T 60 %s -O '%s' '%s'", resume ? "-c" : "", path, url); return system(cmd) ? -1 : 0; }
		int (*gi)(long) = dlsym(C.h, "curl_global_init"); C.init = dlsym(C.h, "curl_easy_init"); C.setopt = dlsym(C.h, "curl_easy_setopt");
		C.perform = dlsym(C.h, "curl_easy_perform"); C.getinfo = dlsym(C.h, "curl_easy_getinfo"); C.strerror = dlsym(C.h, "curl_easy_strerror");
		if (gi) gi(3);
	}
	struct stat st; resume_at = resume && !stat(path, &st) ? st.st_size : 0;
	FILE *f = fopen(path, resume_at ? "ab" : "wb"); if (!f) return -1;
	CURL *h = C.init();
	C.setopt(h, 10002, url); C.setopt(h, 20011, wcb); C.setopt(h, 10001, f); C.setopt(h, 52, 1L); C.setopt(h, 45, 1L);
	C.setopt(h, 78, 30L); C.setopt(h, 19, 1L); C.setopt(h, 20, 60L); C.setopt(h, 10018, "rudolf-ai/1");
	if (resume_at) C.setopt(h, 30116, (long long)resume_at);   /* CURLOPT_RESUME_FROM_LARGE */
	if (!access("/etc/ssl/certs/ca-certificates.crt", R_OK)) C.setopt(h, 10065, "/etc/ssl/certs/ca-certificates.crt");
	char *px = slurp("/run/rudolf/proxy", NULL); if (px) { for (char *e = px; *e; e++) if (*e == '\n') *e = 0; if (*px) C.setopt(h, 10004, px); }
	if (showprog) { lastpct = -1; C.setopt(h, 43, 0L); C.setopt(h, 20219, prog); }
	int r = C.perform(h); long code = 0; C.getinfo(h, 0x200002, &code);
	if (fclose(f)) r = -1;
	if (r) { say("download error: %s", C.strerror ? C.strerror(r) : "failed"); return -1; }
	if (code == 416 && resume_at) return 0;   /* already complete */
	if (code >= 400) { say("download error: HTTP %ld", code); unlink(path); return -1; }
	return 0;
}
static int verify_sig(const char *msg, size_t ml, const unsigned char *sig, size_t sl)
{
	if (sl != 64) return 0;
	char *t = slurp(KEYF, NULL); if (!t) return 0; int ok = 0;
	for (char *l = strtok(t, "\n"); l && !ok; l = strtok(NULL, "\n")) {
		while (isspace((unsigned char)*l)) l++; if (*l == '#') continue;
		char *e = l; while (*e && !isspace((unsigned char)*e)) e++; *e = 0;
		unsigned char raw[64]; if (strlen(l) != 44 || EVP_DecodeBlock(raw, (unsigned char *)l, 44) < 32) continue;
		EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, raw, 32); if (!k) continue;
		EVP_MD_CTX *c = EVP_MD_CTX_new();
		ok = EVP_DigestVerifyInit(c, NULL, NULL, NULL, k) == 1 && EVP_DigestVerify(c, sig, 64, (const unsigned char *)msg, ml) == 1;
		EVP_MD_CTX_free(c); EVP_PKEY_free(k);
	}
	free(t); return ok;
}
static int sha256_file(const char *path, char hex[65])
{
	int f = open(path, O_RDONLY | O_CLOEXEC); if (f < 0) return -1;
	EVP_MD_CTX *c = EVP_MD_CTX_new(); EVP_DigestInit_ex(c, EVP_sha256(), NULL);
	static unsigned char buf[1 << 20]; ssize_t n; while ((n = read(f, buf, sizeof buf)) > 0) EVP_DigestUpdate(c, buf, n);
	close(f); unsigned char md[32]; unsigned l; EVP_DigestFinal_ex(c, md, &l); EVP_MD_CTX_free(c);
	for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", md[i]); return 0;
}
/* the installed model: MODELS/model.txt is the verified list it came from */
static int model_file(char *path, size_t n, char **list)
{
	char *m = slurp(MODELS "/model.txt", NULL); if (!m) return 0; char f[160];
	field(m, "file", f, sizeof f); snprintf(path, n, MODELS "/%s", f);
	if (!*f || strchr(f, '/') || access(path, R_OK)) { free(m); return 0; }
	if (list) *list = m; else free(m);
	return 1;
}
static int cmd_download(void)
{
	if (getuid()) die("run as root");
	if (!persistent()) die("Blitzen needs an installed Rudolf OS (a data partition); the model cannot be kept on a live disc");
	char url[400], u2[600], hex[65], want[80], file[160], name[80], sz[40];
	conf("url", url, sizeof url, DEFAULT_URL);
	mkdirs(DLD, 0755);
	say("step Checking the download");
	snprintf(u2, sizeof u2, "%s/blitzen.txt", url); if (download(u2, DLD "/blitzen.txt", 0, 0)) die("could not download the model list from %s", url);
	snprintf(u2, sizeof u2, "%s/blitzen.txt.sig", url); if (download(u2, DLD "/blitzen.txt.sig", 0, 0)) die("could not download the signature");
	size_t ml, sl; char *m = slurp(DLD "/blitzen.txt", &ml), *s = slurp(DLD "/blitzen.txt.sig", &sl);
	if (!m || !s || !verify_sig(m, ml, (unsigned char *)s, sl)) die("refused: the model list is not signed by a trusted key");
	field(m, "file", file, sizeof file); field(m, "sha256", want, sizeof want); field(m, "model", name, sizeof name); field(m, "size", sz, sizeof sz);
	if (!*file || strchr(file, '/') || file[0] == '.' || strlen(want) != 64) die("the model list is not valid");
	total_size = atoll(sz);
	struct statvfs v; if (!statvfs("/persist", &v) && (long long)v.f_bavail * v.f_frsize < total_size + (200ll << 20)) die("not enough free space: %s needs %lld MB", name, (total_size >> 20) + 200);
	say("step Downloading %s (%lld MB)", name, total_size >> 20);
	char part[300]; snprintf(part, sizeof part, DLD "/%s.part", file);
	snprintf(u2, sizeof u2, "%s/%s", url, file);
	if (download(u2, part, 1, 1)) die("the download stopped; press Download again to continue where it left off");
	say("step Checking the model");
	if (sha256_file(part, hex) || strcmp(hex, want)) { unlink(part); die("refused: the download is damaged (checksum does not match)"); }
	char dst[300]; snprintf(dst, sizeof dst, MODELS "/%s", file);
	if (system("pkill -x llama-server 2>/dev/null; true")) {}
	mkdirs(MODELS, 0755); if (system("rm -f " MODELS "/*")) {}
	if (rename(part, dst)) die("could not put the model in place");
	chmod(dst, 0644); write_str(MODELS "/model.txt", m, 0644);
	unlink(DLD "/blitzen.txt"); unlink(DLD "/blitzen.txt.sig");
	say("done %s is ready", name); return 0;
}

/* ------------------------------------------------------------------ the engine */
static pid_t engine_pid(void)
{
	char *t = slurp(RUND "/pid", NULL); if (!t) return 0; pid_t p = atoi(t); free(t);
	if (p <= 0 || kill(p, 0)) return 0;
	char ex[64], lnk[256] = ""; snprintf(ex, sizeof ex, "/proc/%d/exe", p); ssize_t n = readlink(ex, lnk, sizeof lnk - 1); if (n > 0) lnk[n] = 0;
	return strstr(lnk, "llama-server") ? p : 0;
}
static int health(void)
{
	int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); if (s < 0) return 0;
	struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(PORT), .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
	struct timeval tv = {5, 0}; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	if (connect(s, (struct sockaddr *)&a, sizeof a)) { close(s); return 0; }
	const char *q = "GET /health HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n"; if (write(s, q, strlen(q)) < 0) {}
	char b[64] = ""; ssize_t n = read(s, b, sizeof b - 1); close(s); if (n > 0) b[n] = 0;
	return !strncmp(b, "HTTP/1.1 200", 12) || !strncmp(b, "HTTP/1.0 200", 12);
}
static void stop_engine(void)
{
	pid_t p = engine_pid(); if (p) { kill(p, SIGTERM); for (int i = 0; i < 30 && !kill(p, 0); i++) usleep(100000); kill(p, SIGKILL); }
	char *w = slurp(RUND "/watchdog", NULL); if (w) { pid_t q = atoi(w); if (q > 0 && q != getpid()) kill(q, SIGTERM); free(w); }
	unlink(RUND "/pid"); unlink(RUND "/key"); unlink(RUND "/watchdog");
}
static void quiet_fds(const char *log)
{
	int n = open("/dev/null", O_RDWR); dup2(n, 0);
	int l = log ? open(log, O_WRONLY | O_CREAT | O_TRUNC, 0640) : -1; dup2(l >= 0 ? l : n, 1); dup2(l >= 0 ? l : n, 2);
	for (int i = 3; i < 1024; i++) close(i);
}
/* the engine sees the system but none of the user's files: in a mount namespace of its own, the home folder,
 * USB drives, /tmp, /run/rudolf and the data partition (all but /persist/ai) are covered by empty folders */
static void hide_user_files(void)
{
	if (unshare(CLONE_NEWNS)) return;
	if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL)) return;
	mkdirs(RUND "/keep", 0700);
	int kept = !mount(AIDIR, RUND "/keep", NULL, MS_BIND | MS_REC, NULL);
	const char *hide[] = {"/home", "/media", "/mnt", "/root", "/tmp", "/live", "/run/user", "/persist", NULL};
	for (int i = 0; hide[i]; i++) if (!access(hide[i], F_OK)) mount("tmpfs", hide[i], "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, "size=64k,mode=755");
	if (kept) { mkdir(AIDIR, 0755); mount(RUND "/keep", AIDIR, NULL, MS_MOVE, NULL); }
	mount("tmpfs", "/run/rudolf", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, "size=64k,mode=755");
}
/* the engine is built for x86-64-v3 (AVX2, FMA, F16C, BMI2) */
static int cpu_ok(void)
{
	char *c = slurp("/proc/cpuinfo", NULL); if (!c) return 1;
	char *f = strstr(c, "\nflags"); int ok = 1;
	if (f) { char *e = strchr(f + 1, '\n'); if (e) *e = 0; const char *need[] = {" avx2", " fma", " f16c", " bmi2", NULL}; for (int i = 0; need[i]; i++) if (!strstr(f, need[i])) ok = 0; }
	free(c); return ok;
}
static void watchdog(pid_t eng)
{
	long idle = conf_num("idle_s", 600);
	for (;;) {
		sleep(10);
		if (kill(eng, 0)) break;
		struct stat st; time_t last = stat(RUND "/last", &st) ? 0 : st.st_mtime;
		if (time(NULL) - last > idle) { kill(eng, SIGTERM); sleep(3); kill(eng, SIGKILL); break; }
	}
	unlink(RUND "/pid"); unlink(RUND "/key"); unlink(RUND "/watchdog"); _exit(0);
}
static int cmd_load(void)
{
	if (getuid()) die("run as root");
	mkdirs(RUND, 0755); chmod(RUND, 0700);
	int lk = open(RUND "/lock", O_RDWR | O_CREAT | O_CLOEXEC, 0600); if (lk >= 0) flock(lk, LOCK_EX);
	write_str(RUND "/last", "", 0600);   /* a request counts as use */
	if (engine_pid() && health()) { say("ready"); return 0; }
	stop_engine();
	char model[400], *list = NULL;
	if (!model_file(model, sizeof model, &list)) die("Blitzen is not downloaded yet");
	free(list);
	if (access(ENGINE, X_OK)) die("this Rudolf OS has no model engine");
	if (!cpu_ok()) die("this computer's processor is too old for Blitzen (it needs AVX2, found in Intel and AMD processors since about 2015)");
	long need = conf_num("min_mem_mb", 1536);
	if (mem_avail() < (long long)need << 20) die("not enough free memory to start Blitzen (%lld MB free, %ld MB needed); close some apps or games and try again", mem_avail() >> 20, need);
	mkdirs(CACHE, 0755); if (chown(CACHE, AI_UID, AI_UID)) {}
	unsigned char rb[18]; char key[40]; int rf = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (rf < 0 || read(rf, rb, sizeof rb) != sizeof rb) die("no randomness"); close(rf);
	for (int i = 0; i < 18; i++) sprintf(key + 2 * i, "%02x", rb[i]);
	if (write_str(RUND "/key", key, 0600)) die("cannot write the key");
	char th[16], ctx[16], port[8]; snprintf(th, sizeof th, "%ld", conf_num("threads", 4)); snprintf(ctx, sizeof ctx, "%ld", conf_num("ctx", 8192)); snprintf(port, sizeof port, "%d", PORT);
	pid_t e = fork(); if (e < 0) die("fork");
	if (!e) {
		setsid(); quiet_fds(RUND "/engine.log");
		hide_user_files();
		if (setgroups(0, NULL) || setresgid(AI_UID, AI_UID, AI_UID) || setresuid(AI_UID, AI_UID, AI_UID)) _exit(126);
		prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
		if (chdir(CACHE)) if (chdir("/")) {}
		clearenv(); setenv("PATH", "/usr/bin:/bin", 1); setenv("HOME", CACHE, 1);
		setenv("LLAMA_API_KEY", key, 1);    /* not on the command line, where every user could read it */
		execl(ENGINE, "llama-server", "-m", model, "--host", "127.0.0.1", "--port", port, "-c", ctx, "-t", th,
		      "-np", "1", "-n", "1536", "--cache-type-k", "q8_0", "--slot-save-path", CACHE, "--no-webui", "--jinja", (char *)NULL);
		_exit(127);
	}
	memset(key, 0, sizeof key);
	char s[32]; snprintf(s, sizeof s, "%d\n", e); write_str(RUND "/pid", s, 0600);
	for (int i = 0; i < 360; i++) {
		if (health()) break;
		int st; if (waitpid(e, &st, WNOHANG) == e) { char *l = slurp(RUND "/engine.log", NULL); char *t = l ? l + (strlen(l) > 600 ? strlen(l) - 600 : 0) : ""; fprintf(stderr, "%s\n", t); free(l); unlink(RUND "/pid"); die("the model engine stopped while starting"); }
		usleep(500000);
	}
	if (!health()) { stop_engine(); die("the model engine did not start in time"); }
	pid_t w = fork();
	if (!w) { setsid(); quiet_fds(NULL); watchdog(e); }
	snprintf(s, sizeof s, "%d\n", w); write_str(RUND "/watchdog", s, 0600);
	say("ready"); return 0;
}
static int cmd_status(void)
{
	char model[400], *list = NULL, v[200];
	int inst = model_file(model, sizeof model, &list);
	say("installed=%d", inst);
	if (inst) {
		struct stat st; stat(model, &st);
		say("model=%s", field(list, "model", v, sizeof v)); say("licence=%s", field(list, "licence", v, sizeof v));
		say("size=%lld", (long long)st.st_size); free(list);
	}
	say("loaded=%d", engine_pid() && health() ? 1 : 0);
	say("persistent=%d", persistent());
	say("engine=%d", !access(ENGINE, X_OK));
	say("cpu=%d", cpu_ok());
	say("mem=%lld", mem_avail());
	say("need=%lld", (long long)conf_num("min_mem_mb", 1536) << 20);
	char *dl = NULL; struct stat ps; char part[300] = "";
	char *l2 = slurp(DLD "/blitzen.txt", NULL); if (l2) { field(l2, "file", v, sizeof v); snprintf(part, sizeof part, DLD "/%s.part", v); free(l2); }
	(void)dl; say("partial=%lld", *part && !stat(part, &ps) ? (long long)ps.st_size : 0);
	return 0;
}
int main(int argc, char **argv)
{
	if (argc < 2) die("usage: rudolf-ai status|download|load|unload|remove");
	const char *c = argv[1];
	if (!strcmp(c, "status")) return cmd_status();
	if (!strcmp(c, "download")) return cmd_download();
	if (!strcmp(c, "load")) return cmd_load();
	if (!strcmp(c, "unload")) { if (getuid()) die("run as root"); stop_engine(); say("stopped"); return 0; }
	if (!strcmp(c, "remove")) { if (getuid()) die("run as root"); stop_engine(); if (system("rm -rf " MODELS " " DLD " " CACHE)) {} say("removed"); return 0; }
	die("unknown command %s", c);
}
