/*
 * rudolfd - Rudolf OS system service.
 *
 * Serves the Rudolf OS web desktop on http://127.0.0.1:8080/ and gives it a
 * small JSON API for the things a web page cannot do by itself: power off,
 * restart, suspend, battery and brightness, network interfaces and DHCP,
 * disks and USB drives (browse, read, write), screenshots of the real
 * screen, keyboard layout, pointer speed, sound card choice, clock, OS
 * updates with rollback, factory reset and installing to an internal disk.
 *
 * Every /api/ request must carry the X-Rudolf-Token header. The token is
 * random per boot and is only written into the page this server delivers,
 * so other web sites opened inside Rudolf OS cannot drive the API.
 *
 * Copyright (C) 2026 Rudolf OS contributors. GPL-2.0-or-later.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <grp.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/blkpg.h>
#include <zlib.h>

#define WWW      "/usr/share/rudolf/www"
#define PERSIST  "/persist"
#define RUN      "/run/rudolf"
#define MAXBODY  (512u << 20)

static char token[33];
static char boot_mode[16] = "bios", media_mode[16] = "cd";
static int live = 1, recovery = 0, secureboot = 0;   /* secureboot: the start-up found Secure Boot on (1.2.2) */

/* ------------------------------------------------------------- helpers */

static char *slurp(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	size_t cap = 4096, n = 0;
	char *b = malloc(cap + 1);
	size_t r;
	while ((r = fread(b + n, 1, cap - n, f)) > 0) { n += r; if (n == cap) { cap *= 2; b = realloc(b, cap + 1); } }
	fclose(f);
	b[n] = 0;
	if (len) *len = n;
	return b;
}
static int boot_secure(void)
{
	static int cached = -2; if (cached != -2) return cached;
	size_t n = 0; char *img = NULL;
	const char *paths[] = {"/live/media/efi/efiboot.img", "/live/media/EFI/EFIBOOT.IMG", NULL};
	for (int i = 0; paths[i] && !img; i++) img = slurp(paths[i], &n);
	if (!img) return cached = -1;
	cached = memmem(img, n, "RUDOLF  CER", 11) && memmem(img, n, "GRUBX64 EFI", 11) ? 1 : 0;
	free(img); return cached;
}

static void trim(char *s) { size_t n = strlen(s); while (n && (s[n - 1] == '\n' || s[n - 1] == ' ' || s[n - 1] == '\r')) s[--n] = 0; }

static long long read_ll(const char *path, long long def)
{
	char *s = slurp(path, NULL);
	if (!s) return def;
	long long v = atoll(s);
	free(s);
	return v;
}

static int write_str(const char *path, const char *s)
{
	int f = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (f < 0) return -1;
	ssize_t r = write(f, s, strlen(s));
	close(f);
	return r < 0 ? -1 : 0;
}

/* growable string for JSON replies */
typedef struct { char *b; size_t n, cap; } str;
static void sadd(str *s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void sadd(str *s, const char *fmt, ...)
{
	va_list ap;
	for (;;) {
		if (!s->b) { s->cap = 1024; s->b = malloc(s->cap); s->n = 0; }
		va_start(ap, fmt);
		int r = vsnprintf(s->b + s->n, s->cap - s->n, fmt, ap);
		va_end(ap);
		if ((size_t)r < s->cap - s->n) { s->n += r; return; }
		s->cap = (s->cap + r) * 2;
		s->b = realloc(s->b, s->cap);
	}
}
static void sjson(str *s, const char *v)
{
	sadd(s, "\"");
	for (const unsigned char *p = (const unsigned char *)(v ? v : ""); *p; p++) {
		if (*p == '"' || *p == '\\') sadd(s, "\\%c", *p);
		else if (*p < 0x20) sadd(s, "\\u%04x", *p);
		else sadd(s, "%c", *p);
	}
	sadd(s, "\"");
}

static int run(const char *cmd) { int r = system(cmd); return WIFEXITED(r) ? WEXITSTATUS(r) : -1; }

static char *run_out(const char *cmd)
{
	FILE *p = popen(cmd, "r");
	if (!p) return strdup("");
	str s = {0};
	char buf[4096];
	size_t r;
	sadd(&s, "%s", "");
	while ((r = fread(buf, 1, sizeof buf - 1, p)) > 0) { buf[r] = 0; sadd(&s, "%s", buf); }
	pclose(p);
	return s.b;
}

/* only plain names: letters, digits and - _ . : */
static int safe_word(const char *s, size_t max)
{
	if (!s || !*s || strlen(s) > max) return 0;
	for (; *s; s++) if (!isalnum((unsigned char)*s) && !strchr("-_.:", *s)) return 0;
	return 1;
}

/* paths for the disk browser must stay inside /media */
/* the folders Linux apps share with drive C: (kept in step by the desktop) */
static int shared_path(const char *p)
{
	static const char *const R[] = {"/home/rudolf/Documents", "/home/rudolf/Pictures", "/home/rudolf/Downloads", "/home/rudolf/Games", NULL};
	for (int i = 0; R[i]; i++) { size_t l = strlen(R[i]); if (!strncmp(p, R[i], l) && (!p[l] || p[l] == '/')) return 1; }
	return 0;
}
static int safe_media_path(const char *p)
{
	if (!p || (strncmp(p, "/media/", 7) && !shared_path(p)) || strstr(p, "/../") || strstr(p, "/./") || strlen(p) > 1024) return 0;
	size_t n = strlen(p);
	if (n >= 3 && !strcmp(p + n - 3, "/..")) return 0;
	return 1;
}

static void urldecode(char *s)
{
	char *o = s;
	for (; *s; s++) {
		if (*s == '+') *o++ = ' ';
		else if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
			char h[3] = { s[1], s[2], 0 };
			*o++ = (char)strtol(h, NULL, 16);
			s += 2;
		} else *o++ = *s;
	}
	*o = 0;
}

static char *qparam(const char *query, const char *name)
{
	/* a few buffers in turn, so values asked for one after another stay valid together */
	static char vals[8][2048]; static int vi; char *val = vals[vi++ & 7];
	size_t nl = strlen(name);
	const char *p = query;
	while (p && *p) {
		if (!strncmp(p, name, nl) && p[nl] == '=') {
			const char *e = strchr(p + nl + 1, '&');
			size_t l = e ? (size_t)(e - p - nl - 1) : strlen(p + nl + 1);
			if (l >= sizeof vals[0]) l = sizeof vals[0] - 1;
			memcpy(val, p + nl + 1, l);
			val[l] = 0;
			urldecode(val);
			return val;
		}
		p = strchr(p, '&');
		if (p) p++;
	}
	return NULL;
}

/* ------------------------------------------------------------- HTTP */
struct req {
	int fd;
	char method[8], path[2048], query[2048];
	char tok[64], range[64];
	char *body; size_t blen;
};

static void send_all(int fd, const void *b, size_t n)
{
	const char *p = b;
	while (n) {
		ssize_t w = write(fd, p, n);
		if (w < 0) { if (errno == EINTR) continue; if (errno == EAGAIN) { struct pollfd pf = { fd, POLLOUT, 0 }; poll(&pf, 1, 5000); continue; } return; }
		p += w; n -= w;
	}
}

static void reply(int fd, int code, const char *ctype, const void *body, size_t n, const char *extra)
{
	char h[512];
	const char *msg = code == 200 ? "OK" : code == 206 ? "Partial Content" : code == 400 ? "Bad Request" : code == 403 ? "Forbidden" : code == 404 ? "Not Found" : code == 413 ? "Too Large" : "Error";
	int l = snprintf(h, sizeof h, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n%s\r\n", code, msg, ctype, n, extra ? extra : "");
	send_all(fd, h, l);
	if (n) send_all(fd, body, n);
}
static void reply_json(int fd, str *s) { reply(fd, 200, "application/json", s->b, s->n, NULL); free(s->b); }
static void reply_err(int fd, int code, const char *m)
{
	str s = {0};
	sadd(&s, "{\"ok\":false,\"error\":");
	sjson(&s, m);
	sadd(&s, "}");
	reply(fd, code, "application/json", s.b, s.n, NULL);
	free(s.b);
}
static void reply_ok(int fd, const char *m)
{
	str s = {0};
	sadd(&s, "{\"ok\":true,\"message\":");
	sjson(&s, m ? m : "");
	sadd(&s, "}");
	reply_json(fd, &s);
}

static const char *mime(const char *p)
{
	const char *e = strrchr(p, '.');
	if (!e) return "application/octet-stream";
	e++;
	static const char *m[][2] = { {"html","text/html; charset=utf-8"},{"htm","text/html; charset=utf-8"},{"js","text/javascript"},{"mjs","text/javascript"},{"css","text/css"},{"json","application/json"},{"txt","text/plain; charset=utf-8"},{"png","image/png"},{"jpg","image/jpeg"},{"jpeg","image/jpeg"},{"gif","image/gif"},{"svg","image/svg+xml"},{"webp","image/webp"},{"wasm","application/wasm"},{"pdf","application/pdf"},{"mp3","audio/mpeg"},{"ogg","audio/ogg"},{"wav","audio/wav"},{"mp4","video/mp4"},{"webm","video/webm"},{"mkv","video/x-matroska"},{"mov","video/quicktime"},{"m4v","video/mp4"},{"ogv","video/ogg"},{"m4a","audio/mp4"},{"aac","audio/aac"},{"flac","audio/flac"},{"opus","audio/ogg"},{"bmp","image/bmp"},{"avif","image/avif"},{"woff2","font/woff2"},{"woff","font/woff"},{"ttf","font/ttf"},{"ico","image/x-icon"},{"md","text/markdown; charset=utf-8"},{"swbn","application/webbundle"},{"webmanifest","application/manifest+json"} };
	for (size_t i = 0; i < sizeof m / sizeof m[0]; i++) if (!strcasecmp(e, m[i][0])) return m[i][1];
	return "application/octet-stream";
}

/* the page Rudolf OS boots from: an installed update unless recovering */
static void www_root(char *out, size_t n)
{
	struct stat st;
	/* with Secure Boot on, only the checked system's own page: the data partition is not checked (1.2.2) */
	if (!recovery && !secureboot && !access(PERSIST "/os/current/index.html", R_OK) && !stat(PERSIST "/os/current/index.html", &st) && st.st_size > 100000 && access(PERSIST "/os/disabled", F_OK))
		snprintf(out, n, PERSIST "/os/current");
	else snprintf(out, n, WWW);
}

/* dl = a downloaded file (/dlf/): it is never run as a page of this origin (the desktop's), so pages and scripts
   come as plain text and everything carries a sandbox */
static void serve_file_as(struct req *r, const char *path, int dl)
{
	int f = open(path, O_RDONLY | O_CLOEXEC);
	struct stat st;
	if (f < 0 || fstat(f, &st) || S_ISDIR(st.st_mode)) { if (f >= 0) close(f); reply_err(r->fd, 404, "not found"); return; }
	off_t start = 0, end = st.st_size - 1;
	int partial = 0;
	if (r->range[0] && !strncmp(r->range, "bytes=", 6)) {
		long long a = -1, b = -1;
		if (sscanf(r->range + 6, "%lld-%lld", &a, &b) >= 1 && a >= 0 && a < st.st_size) {
			start = a; if (b >= a && b < st.st_size) end = b;
			partial = 1;
		}
	}
	char h[600];
	/* shared libraries (Ruffle, PDF.js) are also loaded by Oracle's sandboxed pages, which have no origin */
	const char *cors = dl ? "Content-Security-Policy: sandbox\r\nX-Content-Type-Options: nosniff\r\n" : strstr(path, "/lib/") || !strncmp(path, "/pkg/", 5) ? "Access-Control-Allow-Origin: *\r\n" : "";
	const char *mt = mime(path);
	if (dl && (!strncmp(mt, "text/html", 9) || !strcmp(mt, "text/javascript") || (strstr(mt, "xml") && strcmp(mt, "image/svg+xml")))) mt = "text/plain; charset=utf-8";
	int l = snprintf(h, sizeof h, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %lld\r\nAccept-Ranges: bytes\r\n%s%s%sCache-Control: no-cache\r\nConnection: close\r\n\r\n",
		partial ? 206 : 200, partial ? "Partial Content" : "OK", mt, (long long)(end - start + 1),
		partial ? "Content-Range: bytes " : "", "", cors);
	if (partial) {
		l -= 0;
		l = snprintf(h, sizeof h, "HTTP/1.1 206 Partial Content\r\nContent-Type: %s\r\nContent-Length: %lld\r\nAccept-Ranges: bytes\r\nContent-Range: bytes %lld-%lld/%lld\r\n%sConnection: close\r\n\r\n",
			mt, (long long)(end - start + 1), (long long)start, (long long)end, (long long)st.st_size, cors);
	}
	send_all(r->fd, h, l);
	if (!strcmp(r->method, "HEAD")) { close(f); return; }
	lseek(f, start, SEEK_SET);
	char buf[65536];
	off_t left = end - start + 1;
	while (left > 0) {
		ssize_t n = read(f, buf, left > (off_t)sizeof buf ? sizeof buf : (size_t)left);
		if (n <= 0) break;
		send_all(r->fd, buf, n);
		left -= n;
	}
	close(f);
}
static void serve_file(struct req *r, const char *path) { serve_file_as(r, path, 0); }

/* Chromium keeps localStorage in memory and writes it to disk late; after a power cut or a
 * hurried shutdown recent settings and accounts could be lost. The desktop therefore also
 * sends a copy to rudolfd (POST /api/lsync), which writes it atomically to LSFILE; the copy
 * is handed back inside the page and restored when it is newer than what Chromium kept. */
#define LSFILE "/home/rudolf/.rudolf-ls.json"
static const char LSJS[] =
	"(function(){var G='__rudolf_ls_gen',S=localStorage,P=Storage.prototype,si=P.setItem,ri=P.removeItem,cl=P.clear,t=0;"
	"try{var e=document.getElementById('rudolf-ls'),L=e&&JSON.parse(e.textContent);"
	"if(L&&L.data&&L.gen>(+S.getItem(G)||0)){var ks=[];for(var i=0;i<S.length;i++)ks.push(S.key(i));"
	"ks.forEach(function(k){if(k!=G&&!(k in L.data))ri.call(S,k)});for(var k in L.data)si.call(S,k,L.data[k]);si.call(S,G,String(L.gen))}}catch(x){}"
	"function push(){t=0;try{var d={};for(var i=0;i<S.length;i++){var k=S.key(i);if(k!=G)d[k]=S.getItem(k)}"
	"var g=(+S.getItem(G)||0)+1;si.call(S,G,String(g));"
	"return fetch('/api/lsync',{method:'POST',headers:{'X-Rudolf-Token':RUDOLFD.token},body:JSON.stringify({gen:g,data:d})}).catch(function(){})}catch(x){return Promise.resolve()}}"
	"function sch(o){if(o===S&&!t)t=setTimeout(push,1500)}"
	"P.setItem=function(k,v){si.call(this,k,v);if(k!=G)sch(this)};P.removeItem=function(k){ri.call(this,k);sch(this)};P.clear=function(){cl.call(this);sch(this)};"
	"window.__rudolfLsFlush=function(){if(t)clearTimeout(t);return push()};"
	"addEventListener('pagehide',function(){if(t){clearTimeout(t);push()}})})();";

static void serve_index(struct req *r, const char *root)
{
	char p[512];
	snprintf(p, sizeof p, "%s/index.html", root);
	size_t n;
	char *html = slurp(p, &n);
	if (!html) { reply_err(r->fd, 404, "Rudolf OS page missing"); return; }
	size_t lsn = 0;
	char *ls = slurp(LSFILE, &lsn);
	if (ls && (lsn < 2 || ls[0] != '{')) { free(ls); ls = NULL; lsn = 0; }
	size_t cap = 4096 + sizeof LSJS + lsn * 6;
	char *inj = malloc(cap), *q = inj;
	/* the error reporter sends uncaught page errors to /api/clientlog so they can be read from the console */
	q += snprintf(q, cap, "<script type=\"application/json\" id=\"rudolf-ls\">");
	for (size_t i = 0; ls && i < lsn; i++) {   /* '<' escaped so the data can never end the script element */
		if (ls[i] == '<') { memcpy(q, "\\u003c", 6); q += 6; } else *q++ = ls[i];
	}
	q += sprintf(q, "</script><script>window.RUDOLFD={token:\"%s\",base:\"\",boot:\"%s\",media:\"%s\",live:%d,recovery:%d};"
		"(function(){var n=0;function s(m){if(++n>200)return;try{fetch('/api/clientlog',{method:'POST',headers:{'X-Rudolf-Token':RUDOLFD.token},body:new Date().toISOString()+' '+String(m).slice(0,3000)})}catch(e){}}"
		"addEventListener('error',function(e){s('error: '+e.message+' @'+(e.filename||'').split('/').pop()+':'+e.lineno+':'+e.colno)});"
		"addEventListener('unhandledrejection',function(e){var r=e.reason;s('promise: '+(r&&r.stack||r))});window.__rdlog=s})();%s</script>",
		token, boot_mode, media_mode, live, recovery, LSJS);
	size_t il = q - inj;
	free(ls);
	char *h = strstr(html, "<head>");
	size_t at = h ? (size_t)(h - html) + 6 : 0;
	char *out = malloc(n + il + 1);
	memcpy(out, html, at); memcpy(out + at, inj, il); memcpy(out + at + il, html + at, n - at);
	reply(r->fd, 200, "text/html; charset=utf-8", out, n + il, NULL);
	free(out); free(html); free(inj);
}

/* ------------------------------------------------------------- system info */
static void cpu_times(unsigned long long *busy, unsigned long long *total)
{
	FILE *f = fopen("/proc/stat", "r");
	unsigned long long u = 0, ni = 0, s = 0, id = 0, io = 0, irq = 0, sirq = 0, st = 0;
	if (f) { if (fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &u, &ni, &s, &id, &io, &irq, &sirq, &st) < 4) {} fclose(f); }
	*busy = u + ni + s + irq + sirq + st;
	*total = *busy + id + io;
}

static void battery(str *s)
{
	DIR *d = opendir("/sys/class/power_supply");
	struct dirent *e;
	int first = 1;
	sadd(s, "[");
	while (d && (e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		char p[256], t[64] = "";
		snprintf(p, sizeof p, "/sys/class/power_supply/%s/type", e->d_name);
		char *ty = slurp(p, NULL);
		if (!ty) continue;
		trim(ty);
		snprintf(t, sizeof t, "%s", ty);
		free(ty);
		snprintf(p, sizeof p, "/sys/class/power_supply/%s/capacity", e->d_name);
		long long cap = read_ll(p, -1);
		snprintf(p, sizeof p, "/sys/class/power_supply/%s/status", e->d_name);
		char *stt = slurp(p, NULL);
		if (stt) trim(stt);
		snprintf(p, sizeof p, "/sys/class/power_supply/%s/online", e->d_name);
		long long online = read_ll(p, -1);
		sadd(s, "%s{\"name\":", first ? "" : ",");
		sjson(s, e->d_name);
		sadd(s, ",\"type\":");
		sjson(s, t);
		sadd(s, ",\"capacity\":%lld,\"online\":%lld,\"status\":", cap, online);
		sjson(s, stt ? stt : "");
		/* health (1.2.8, Settings > Hardware check): energy in uWh, or charge in uAh on batteries that report charge */
		{ static const char *k[] = {"energy_full", "energy_full_design", "charge_full", "charge_full_design", "cycle_count", "energy_now", "power_now", "voltage_min_design"};
		  for (unsigned i = 0; i < sizeof k / sizeof *k; i++) { snprintf(p, sizeof p, "/sys/class/power_supply/%s/%s", e->d_name, k[i]); long long v = read_ll(p, -1); if (v >= 0) sadd(s, ",\"%s\":%lld", k[i], v); }
		  static const char *t2[] = {"manufacturer", "model_name", "technology"};
		  for (unsigned i = 0; i < sizeof t2 / sizeof *t2; i++) { snprintf(p, sizeof p, "/sys/class/power_supply/%s/%s", e->d_name, t2[i]); char *v = slurp(p, NULL); if (v) { trim(v); sadd(s, ",\"%s\":", t2[i]); sjson(s, v); free(v); } } }
		sadd(s, "}");
		free(stt);
		first = 0;
	}
	if (d) closedir(d);
	sadd(s, "]");
}

static int backlight_dir(char *out, size_t n)
{
	DIR *d = opendir("/sys/class/backlight");
	struct dirent *e;
	int ok = 0;
	while (d && (e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		snprintf(out, n, "/sys/class/backlight/%s", e->d_name);
		ok = 1;
		if (strstr(e->d_name, "intel") || strstr(e->d_name, "amdgpu")) break;
	}
	if (d) closedir(d);
	return ok;
}

static void api_info(struct req *r)
{
	str s = {0};
	struct sysinfo si; sysinfo(&si);
	char *model = run_out("grep -m1 'model name' /proc/cpuinfo | cut -d: -f2-");
	trim(model);
	char *kern = run_out("uname -r"); trim(kern);
	char *prod = slurp("/sys/class/dmi/id/product_name", NULL); if (prod) trim(prod);
	char *vend = slurp("/sys/class/dmi/id/sys_vendor", NULL); if (vend) trim(vend);
	char *ver = slurp("/usr/share/rudolf/VERSION", NULL); if (ver) trim(ver);
	struct statvfs sv;
	long long ptotal = -1, pfree = -1;
	if (!statvfs(PERSIST, &sv) && !live) { ptotal = (long long)sv.f_blocks * sv.f_frsize; pfree = (long long)sv.f_bavail * sv.f_frsize; }
	char bl[256];
	sadd(&s, "{\"ok\":true,\"version\":"); sjson(&s, ver ? ver : "25");
	sadd(&s, ",\"kernel\":"); sjson(&s, kern);
	sadd(&s, ",\"cpu\":"); sjson(&s, *model == ' ' ? model + 1 : model);
	sadd(&s, ",\"threads\":%ld,\"memTotal\":%llu,\"memFree\":%llu,\"uptime\":%ld", sysconf(_SC_NPROCESSORS_ONLN), (unsigned long long)si.totalram * si.mem_unit, (unsigned long long)si.freeram * si.mem_unit, si.uptime);
	sadd(&s, ",\"vendor\":"); sjson(&s, vend ? vend : "");
	sadd(&s, ",\"product\":"); sjson(&s, prod ? prod : "");
	sadd(&s, ",\"boot\":"); sjson(&s, boot_mode);
	sadd(&s, ",\"media\":"); sjson(&s, media_mode);
	/* the device Rudolf OS started from (1.2.5: Make a USB stick leaves its whole drive out of the list) */
	{ char *md = slurp("/etc/rudolf/media", NULL); if (md) { md[strcspn(md, "\n")] = 0; } sadd(&s, ",\"mediaDev\":"); sjson(&s, md ? md : ""); free(md); }
	/* 1.2.7: whether the drive Rudolf OS started from has boot files that start with Secure Boot (a disc from 1.2.2 on:
	   shim, the signed boot menu and EFI/rudolf/rudolf.cer in its EFI image). Updates never replace these, so a
	   computer installed from an older disc and updated still has the old, unsigned ones. 1 yes, 0 no, -1 unknown. */
	sadd(&s, ",\"bootSecure\":%d", boot_secure());
	/* 1.2.8: new boot files staged for the next start (bootfiles.sh), and what the start-up did with the last ones */
	{ char *res = slurp(PERSIST "/os/bootimg/result", NULL); if (res) trim(res);
	  sadd(&s, ",\"bootFiles\":{\"pending\":%s,\"result\":", access(PERSIST "/os/bootimg/pending", F_OK) ? "false" : "true"); sjson(&s, res ? res : ""); sadd(&s, "}"); free(res); }
	sadd(&s, ",\"live\":%s,\"recovery\":%s,\"persistTotal\":%lld,\"persistFree\":%lld,\"backlight\":%s,\"updateInstalled\":%s}", live ? "true" : "false", recovery ? "true" : "false", ptotal, pfree,
		backlight_dir(bl, sizeof bl) ? "true" : "false", access(PERSIST "/os/current/index.html", R_OK) ? "false" : "true");
	free(model); free(kern); free(prod); free(vend); free(ver);
	reply_json(r->fd, &s);
}

static void api_stats(struct req *r)
{
	unsigned long long b, t, pb = 0, pt = 0;
	cpu_times(&b, &t);
	FILE *pf = fopen(RUN "/cpu.prev", "r");
	if (pf) { if (fscanf(pf, "%llu %llu", &pb, &pt) != 2) pb = pt = 0; fclose(pf); }
	double cpu = (t > pt && pt) ? 100.0 * (b - pb) / (t - pt) : 0;
	char cb[64]; snprintf(cb, sizeof cb, "%llu %llu\n", b, t); write_str(RUN "/cpu.prev", cb);
	struct sysinfo si; sysinfo(&si);
	str s = {0};
	sadd(&s, "{\"ok\":true,\"cpu\":%.1f,\"load\":%.2f,\"memTotal\":%llu,\"memFree\":%llu,\"memAvailable\":", cpu, si.loads[0] / 65536.0, (unsigned long long)si.totalram * si.mem_unit, (unsigned long long)si.freeram * si.mem_unit);
	char *ma = run_out("awk '/MemAvailable/{print $2*1024}' /proc/meminfo"); trim(ma);
	sadd(&s, "%s", *ma ? ma : "0");
	free(ma);
	sadd(&s, ",\"battery\":"); battery(&s);
	char bl[256];
	if (backlight_dir(bl, sizeof bl)) {
		char p[300];
		snprintf(p, sizeof p, "%s/brightness", bl); long long cur = read_ll(p, 0);
		snprintf(p, sizeof p, "%s/max_brightness", bl); long long mx = read_ll(p, 1);
		sadd(&s, ",\"brightness\":%lld", mx > 0 ? cur * 100 / mx : -1);
	} else sadd(&s, ",\"brightness\":null");
	char *temp = run_out("cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | sort -n | tail -1"); trim(temp);
	sadd(&s, ",\"tempC\":%s", *temp ? temp : "null");
	if (*temp) { s.n -= strlen(temp); sadd(&s, "%.1f", atof(temp) / 1000.0); }
	free(temp);
	char *procs = run_out("ps -o pid,rss,comm 2>/dev/null | tail -n +2 | sort -k2 -n -r | head -12");
	sadd(&s, ",\"processes\":"); sjson(&s, procs); free(procs);
	sadd(&s, "}");
	reply_json(r->fd, &s);
}

/* ------------------------------------------------------------- network */
static void api_net(struct req *r)
{
	str s = {0};
	sadd(&s, "{\"ok\":true,\"interfaces\":[");
	DIR *d = opendir("/sys/class/net");
	struct dirent *e;
	int first = 1;
	while (d && (e = readdir(d))) {
		if (e->d_name[0] == '.' || !strcmp(e->d_name, "lo")) continue;
		char p[256];
		snprintf(p, sizeof p, "/sys/class/net/%s/operstate", e->d_name);
		char *st = slurp(p, NULL); if (st) trim(st);
		snprintf(p, sizeof p, "/sys/class/net/%s/address", e->d_name);
		char *mac = slurp(p, NULL); if (mac) trim(mac);
		snprintf(p, sizeof p, "/sys/class/net/%s/wireless", e->d_name);
		int wifi = !access(p, F_OK);
		snprintf(p, sizeof p, "/sys/class/net/%s/statistics/rx_bytes", e->d_name);
		long long rx = read_ll(p, 0);
		snprintf(p, sizeof p, "/sys/class/net/%s/statistics/tx_bytes", e->d_name);
		long long tx = read_ll(p, 0);
		char cmd[256];
		snprintf(cmd, sizeof cmd, "ip -4 -o addr show dev %s 2>/dev/null | awk '{print $4}' | head -1", e->d_name);
		char *ip = run_out(cmd); trim(ip);
		sadd(&s, "%s{\"name\":", first ? "" : ","); sjson(&s, e->d_name);
		sadd(&s, ",\"state\":"); sjson(&s, st ? st : "");
		sadd(&s, ",\"mac\":"); sjson(&s, mac ? mac : "");
		sadd(&s, ",\"ip\":"); sjson(&s, ip);
		sadd(&s, ",\"wireless\":%s,\"rx\":%lld,\"tx\":%lld}", wifi ? "true" : "false", rx, tx);
		free(st); free(mac); free(ip);
		first = 0;
	}
	if (d) closedir(d);
	char *gw = run_out("ip route 2>/dev/null | awk '/default/{print $3; exit}'"); trim(gw);
	char *dns = run_out("awk '/nameserver/{print $2}' /etc/resolv.conf 2>/dev/null | head -2 | tr '\\n' ' '"); trim(dns);
	sadd(&s, "],\"gateway\":"); sjson(&s, gw);
	sadd(&s, ",\"dns\":"); sjson(&s, dns);
	sadd(&s, ",\"wifiTools\":%s}", access("/usr/sbin/wpa_supplicant", X_OK) ? "false" : "true");
	free(gw); free(dns);
	reply_json(r->fd, &s);
}

/* ------------------------------------------------------------- disks */
static void api_disks(struct req *r)
{
	char *out = run_out("for d in /sys/block/*; do n=${d##*/}; case $n in loop*|ram*|zram*) continue;; esac; "
		"s=$(cat $d/size); rm=$(cat $d/removable); m=$(cat $d/device/model 2>/dev/null | tr -s ' '); "
		/* USB drives and SD cards count as removable (many USB sticks say they are not) */
		"case $(readlink -f $d/device) in */usb*) rm=1;; */mmc*) [ \"$(cat $d/device/type 2>/dev/null)\" = SD ] && rm=1;; esac; "
		"echo \"D|$n|$s|$rm|$m\"; np=1; for p in $d/$n*; do [ -e \"$p/partition\" ] && np=0; done; "
		/* a disk without partitions whose whole disk holds a file system (many USB sticks and SD cards) is listed as its own partition */
		"for p in $d/$n* $d; do if [ \"$p\" = \"$d\" ]; then [ $np = 1 ] && blkid /dev/$n >/dev/null 2>&1 || continue; else [ -e \"$p/partition\" ] || continue; fi; pn=${p##*/}; "
		"ps=$(cat $p/size); mp=$(awk -v d=/dev/$pn '$1==d{print $2; exit}' /proc/mounts); "
		"fs=$(blkid /dev/$pn 2>/dev/null | sed -n 's/.*TYPE=\"\\([^\"]*\\)\".*/\\1/p'); "
		"lb=$(blkid /dev/$pn 2>/dev/null | sed -n 's/.* LABEL=\"\\([^\"]*\\)\".*/\\1/p'); echo \"P|$pn|$ps|$fs|$lb|$mp\"; done; done");
	str s = {0};
	sadd(&s, "{\"ok\":true,\"disks\":[");
	int fd = 1, fp = 1;
	char *save, *line = strtok_r(out, "\n", &save);
	while (line) {
		/* split on "|" keeping empty fields (a partition without a label still has its mount point after it) */
		char *f[6] = {0};
		int k = 0;
		char *t = line;
		while (t && k < 6) { f[k++] = t; t = strchr(t, '|'); if (t) *t++ = 0; }
		if (f[0] && !strcmp(f[0], "D") && k >= 4) {
			if (!fd) sadd(&s, "]}");
			sadd(&s, "%s{\"name\":", fd ? "" : ","); sjson(&s, f[1]);
			sadd(&s, ",\"bytes\":%lld,\"removable\":%s,\"model\":", atoll(f[2]) * 512, f[3][0] == '1' ? "true" : "false");
			sjson(&s, k > 4 ? f[4] : "");
			sadd(&s, ",\"partitions\":[");
			fd = 0; fp = 1;
		} else if (f[0] && !strcmp(f[0], "P") && k >= 3 && !fd) {
			sadd(&s, "%s{\"name\":", fp ? "" : ","); sjson(&s, f[1]);
			sadd(&s, ",\"bytes\":%lld,\"fs\":", atoll(f[2]) * 512); sjson(&s, k > 3 ? f[3] : "");
			sadd(&s, ",\"label\":"); sjson(&s, k > 4 ? f[4] : "");
			sadd(&s, ",\"mount\":"); sjson(&s, k > 5 ? f[5] : "");
			sadd(&s, "}");
			fp = 0;
		}
		line = strtok_r(NULL, "\n", &save);
	}
	if (!fd) sadd(&s, "]}");
	sadd(&s, "]}");
	free(out);
	reply_json(r->fd, &s);
}

static void api_mount(struct req *r, int mount)
{
	char *dev = qparam(r->query, "dev");
	if (!safe_word(dev, 32)) { reply_err(r->fd, 400, "bad device"); return; }
	char cmd[512];
	/* mounted without device files, set-user-ID or programs run from the drive (devplug: also SD cards, NTFS) */
	if (mount) snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/devplug mount %s", dev);
	else snprintf(cmd, sizeof cmd, "sync; umount /media/%s && rmdir /media/%s; rm -f /run/rudolf/drives/%s", dev, dev, dev);
	if (run(cmd)) reply_err(r->fd, 500, mount ? "could not mount this partition" : "could not unmount (files in use?)");
	else reply_ok(r->fd, mount ? "mounted" : "safe to remove");
}

static void api_fs(struct req *r, const char *op)
{
	char *p = qparam(r->query, "path");
	char path[1100];
	if (!p || !safe_media_path(p)) { reply_err(r->fd, 403, "only /media/... and the shared folders are reachable"); return; }
	snprintf(path, sizeof path, "%s", p);
	if (!strcmp(op, "list")) {
		DIR *d = opendir(path);
		if (!d) { reply_err(r->fd, 404, "no such folder"); return; }
		str s = {0};
		sadd(&s, "{\"ok\":true,\"items\":[");
		struct dirent *e;
		int first = 1;
		while ((e = readdir(d))) {
			if (e->d_name[0] == '.') continue;   /* hidden: also Downloads/.incoming and .quarantine */
			char fp[2200];
			struct stat st;
			snprintf(fp, sizeof fp, "%s/%s", path, e->d_name);
			if (stat(fp, &st)) continue;
			sadd(&s, "%s{\"name\":", first ? "" : ","); sjson(&s, e->d_name);
			sadd(&s, ",\"dir\":%s,\"size\":%lld,\"mtime\":%lld}", S_ISDIR(st.st_mode) ? "true" : "false", (long long)st.st_size, (long long)st.st_mtime * 1000);
			first = 0;
		}
		closedir(d);
		sadd(&s, "]}");
		reply_json(r->fd, &s);
	} else if (!strcmp(op, "read")) serve_file(r, path);
	else if (!strcmp(op, "write")) {
		char tmp[1200];
		snprintf(tmp, sizeof tmp, "%s.rudolf-tmp", path);
		int f = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (f < 0) { reply_err(r->fd, 500, "cannot write here (read-only drive?)"); return; }
		ssize_t w = r->blen ? write(f, r->body, r->blen) : 0;
		fsync(f); close(f);
		if (w != (ssize_t)r->blen || rename(tmp, path)) { unlink(tmp); reply_err(r->fd, 500, "write failed (drive full?)"); return; }
		if (shared_path(path)) { if (chown(path, 1000, 1000)) {} }
		const char *mt = qparam(r->query, "mtime"); if (mt && atoll(mt) > 0) { struct timespec ts[2] = {{atoll(mt) / 1000, (atoll(mt) % 1000) * 1000000}, {atoll(mt) / 1000, (atoll(mt) % 1000) * 1000000}}; utimensat(AT_FDCWD, path, ts, 0); }
		{ struct stat st2; str s2 = {0}; if (!stat(path, &st2)) { sadd(&s2, "{\"ok\":true,\"message\":\"saved\",\"mtime\":%lld,\"size\":%lld}", (long long)st2.st_mtim.tv_sec * 1000 + st2.st_mtim.tv_nsec / 1000000, (long long)st2.st_size); reply_json(r->fd, &s2); } else reply_ok(r->fd, "saved"); }
	} else if (!strcmp(op, "mkdir")) { if (mkdir(path, 0755) && errno != EEXIST) reply_err(r->fd, 500, strerror(errno)); else { if (shared_path(path)) { if (chown(path, 1000, 1000)) {} } reply_ok(r->fd, "created"); } }
	else if (!strcmp(op, "tree")) {
		/* every file and folder below path (for keeping drive C: in step): relative path, size, mtime in ms */
		str s = {0}; sadd(&s, "{\"ok\":true,\"items\":["); int n = 0;
		char stack[64][1100]; int sp = 0; snprintf(stack[sp++], sizeof stack[0], "%s", path);
		size_t base = strlen(path);
		while (sp && n < 20000) {
			char dir[1100]; snprintf(dir, sizeof dir, "%s", stack[--sp]);
			DIR *d = opendir(dir); if (!d) continue; struct dirent *e;
			while ((e = readdir(d)) && n < 20000) {
				if (e->d_name[0] == '.' || strstr(e->d_name, ".rudolf-tmp")) continue;
				char fp[2200]; struct stat st; snprintf(fp, sizeof fp, "%s/%s", dir, e->d_name);
				if (lstat(fp, &st) || S_ISLNK(st.st_mode)) continue;
				int isd = S_ISDIR(st.st_mode);
				sadd(&s, "%s{\"p\":", n ? "," : ""); sjson(&s, fp + base + 1);
				sadd(&s, ",\"dir\":%s,\"size\":%lld,\"mtime\":%lld}", isd ? "true" : "false", (long long)st.st_size, (long long)st.st_mtim.tv_sec * 1000 + st.st_mtim.tv_nsec / 1000000);
				n++; if (isd && sp < 64 && strlen(fp) < 1090) snprintf(stack[sp++], sizeof stack[0], "%s", fp);
			}
			closedir(d);
		}
		sadd(&s, "]}"); reply_json(r->fd, &s);
	}
	else if (!strcmp(op, "delete")) {
		struct stat st;
		if (stat(path, &st)) { reply_err(r->fd, 404, "not found"); return; }
		int rc = S_ISDIR(st.st_mode) ? rmdir(path) : unlink(path);
		if (rc) reply_err(r->fd, 500, strerror(errno)); else reply_ok(r->fd, "deleted");
	} else reply_err(r->fd, 404, "unknown file operation");
}

/* ------------------------------------------------------------- screenshot */
static void put32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
/* RGB rows with a PNG filter byte in front of each. From the X server when Xorg draws the screen
 * (graphics-card display path), otherwise straight from the framebuffer. */
static uint8_t *grab_x(int *pw, int *ph, size_t *pn)
{
	FILE *p = popen("DISPLAY=:0 rudolf-display --shot 2>/dev/null", "r");
	if (!p) return NULL;
	int w = 0, h = 0, mx = 0;
	if (fscanf(p, "P6 %d %d %d", &w, &h, &mx) != 3 || w <= 0 || h <= 0 || w > 16384 || h > 16384) { pclose(p); return NULL; }
	fgetc(p);
	size_t n = (size_t)(w * 3 + 1) * h;
	uint8_t *raw = malloc(n);
	for (int y = 0; y < h; y++) { uint8_t *o = raw + (size_t)y * (w * 3 + 1); *o = 0; if (fread(o + 1, 3, w, p) != (size_t)w) { free(raw); pclose(p); return NULL; } }
	pclose(p);
	*pw = w; *ph = h; *pn = n; return raw;
}
static uint8_t *grab_fb(int *pw, int *ph, size_t *pn, const char **err)
{
	int f = open("/dev/fb0", O_RDONLY);
	struct fb_var_screeninfo v; struct fb_fix_screeninfo fx;
	if (f < 0 || ioctl(f, FBIOGET_VSCREENINFO, &v) || ioctl(f, FBIOGET_FSCREENINFO, &fx) || v.bits_per_pixel != 32) { if (f >= 0) close(f); *err = "no 32-bit framebuffer"; return NULL; }
	size_t len = (size_t)fx.line_length * v.yres;
	uint8_t *fb = mmap(NULL, len, PROT_READ, MAP_SHARED, f, 0);
	if (fb == MAP_FAILED) { close(f); *err = "mmap failed"; return NULL; }
	int w = v.xres, h = v.yres, ro = v.red.offset;
	size_t rawn = (size_t)(w * 3 + 1) * h;
	uint8_t *raw = malloc(rawn);
	for (int y = 0; y < h; y++) {
		uint8_t *o = raw + (size_t)y * (w * 3 + 1);
		uint32_t *s = (uint32_t *)(fb + (size_t)y * fx.line_length);
		*o++ = 0;
		for (int x = 0; x < w; x++) {
			uint32_t c = s[x];
			if (ro == 0) c = (c & 0xFF00) | (c >> 16 & 0xFF) | (c & 0xFF) << 16;
			*o++ = c >> 16; *o++ = c >> 8; *o++ = c;
		}
	}
	munmap(fb, len); close(f);
	*pw = w; *ph = h; *pn = rawn; return raw;
}
static void api_screenshot(struct req *r)
{
	int w = 0, h = 0; size_t rawn = 0; const char *err = "screenshot failed";
	char *mode = slurp(RUN "/display-mode", NULL);
	uint8_t *raw = mode && !strncmp(mode, "gpu", 3) ? grab_x(&w, &h, &rawn) : NULL;
	free(mode);
	if (!raw) raw = grab_fb(&w, &h, &rawn, &err);
	if (!raw) { reply_err(r->fd, 500, err); return; }
	uLongf zn = compressBound(rawn);
	uint8_t *z = malloc(zn);
	compress2(z, &zn, raw, rawn, 6);
	free(raw);
	size_t pn = 8 + 25 + 12 + zn + 12;
	uint8_t *png = malloc(pn), *p = png;
	memcpy(p, "\x89PNG\r\n\x1a\n", 8); p += 8;
	put32(p, 13); memcpy(p + 4, "IHDR", 4); put32(p + 8, w); put32(p + 12, h); p[16] = 8; p[17] = 2; p[18] = 0; p[19] = 0; p[20] = 0;
	put32(p + 21, crc32(0, p + 4, 17)); p += 25;
	put32(p, zn); memcpy(p + 4, "IDAT", 4); memcpy(p + 8, z, zn); put32(p + 8 + zn, crc32(0, p + 4, zn + 4)); p += 12 + zn;
	put32(p, 0); memcpy(p + 4, "IEND", 4); put32(p + 8, crc32(0, p + 4, 4)); p += 12;
	reply(r->fd, 200, "image/png", png, p - png, NULL);
	free(z); free(png);
}

/* ------------------------------------------------------------- background jobs (install, wifi)
 * Connections are served by forked processes, so job state lives in files. */
static int job_running(void) { return !access(RUN "/job.name", F_OK) && access(RUN "/job.status", F_OK); }
/* 1.2.7: sha=<64 hex> (from the image's .sha256, which the page reads) for an image still in parts: passed to
   joinparts.sh through the environment. Anything else is ignored. */
static const char *img_sha_env(struct req *r)
{
	static char env[100]; env[0] = 0; char *h = qparam(r->query, "sha");
	if (h && strlen(h) == 64) { int ok = 1; for (const char *c = h; *c; c++) if (!isxdigit((unsigned char)*c)) ok = 0; if (ok) { snprintf(env, sizeof env, "RUDOLF_IMG_SHA=%s ", h); for (char *c = env; *c; c++) *c = tolower((unsigned char)*c); memcpy(env, "RUDOLF_IMG_SHA", 14); } }
	return env;
}
static void start_job(const char *name, const char *cmd)
{
	unlink(RUN "/job.status"); unlink(RUN "/job.log");
	write_str(RUN "/job.name", name);
	/* 1.3.0: the kernel's last crash line now, so a crash during this job can be told apart from older ones */
	run("dmesg 2>/dev/null | grep -E ': segfault at |: general protection|traps: ' | tail -1 > " RUN "/job.kmark");
	FILE *f = fopen(RUN "/job.sh", "w");
	if (!f) return;
	fprintf(f, "%s > " RUN "/job.log 2>&1\necho $? > " RUN "/job.status\n", cmd);
	fclose(f);
	run("setsid sh " RUN "/job.sh </dev/null >/dev/null 2>&1 &");
}
static void reap(void) { while (waitpid(-1, NULL, WNOHANG) > 0) {} }
/* 1.3.0: crashes of Rudolf OS's own programs, from the kernel's log ("rudolf-apps[9979]: segfault at 0 ip ... in
 * libc.so.6[18badd,...]"): program, seconds after start-up, and where (library + offset). after: only lines that come
 * after this one (the last crash line when a job started). Other programs' crashes are not reported. */
static int crash_list(str *s, const char *after, int max)
{
	char *l = run_out("dmesg 2>/dev/null | grep -E ': segfault at |: general protection|traps: ' | tail -200");
	int n = 0, past = !after || !*after;
	for (char *line = l ? strtok(l, "\n") : NULL; line; line = strtok(NULL, "\n")) {
		if (!past) { if (!strcmp(line, after)) past = 1; continue; }
		double t = 0; char prog[64] = ""; const char *b = strchr(line, ']'); if (line[0] == '[') sscanf(line + 1, "%lf", &t);
		if (!b) continue; b++; while (*b == ' ') b++;
		if (!strncmp(b, "traps: ", 7)) b += 7;
		size_t k = strcspn(b, "["); if (k == 0 || k >= sizeof prog) continue; snprintf(prog, sizeof prog, "%.*s", (int)k, b);
		if (strncmp(prog, "rudolf", 6) && strcmp(prog, "rpk") && strcmp(prog, "cdp-eval")) continue;
		char where[160] = ""; const char *in = strstr(b, " in "); if (in) { in += 4; size_t w = strcspn(in, "[ "); const char *o = in[w] == '[' ? in + w + 1 : NULL;
			snprintf(where, sizeof where, "%.*s%s%.*s", (int)w, in, o ? "+" : "", o ? (int)strcspn(o, ",]") : 0, o ? o : ""); }
		const char *kind = strstr(b, "segfault") ? "segmentation fault" : strstr(b, "general protection") ? "general protection fault" : "trap";
		if (n < max) { sadd(s, "%s{\"program\":", n ? "," : ""); sjson(s, prog); sadd(s, ",\"at\":%.0f,\"kind\":", t); sjson(s, kind); sadd(s, ",\"where\":"); sjson(s, where); sadd(s, "}"); }
		n++;
	}
	free(l); return n;
}
static void api_job(struct req *r)
{
	str s = {0};
	char *log = slurp(RUN "/job.log", NULL), *name = slurp(RUN "/job.name", NULL);
	long long st = read_ll(RUN "/job.status", -1);
	sadd(&s, "{\"ok\":true,\"name\":"); sjson(&s, name ? name : "");
	sadd(&s, ",\"running\":%s,\"status\":%lld,\"log\":", job_running() ? "true" : "false", st);
	if (log && strlen(log) > 8000) sjson(&s, log + strlen(log) - 8000); else sjson(&s, log ? log : "");
	/* 1.3.0: a job ended by a signal (shell status 128+n), or a crash of one of Rudolf OS's programs during it: the
	   page says so instead of guessing (the App Store asked "Is this computer connected to the internet?") */
	if (st > 128 || (log && (strstr(log, "Segmentation fault") || strstr(log, "Aborted") || strstr(log, "Illegal instruction")))) {
		char *mk = slurp(RUN "/job.kmark", NULL); if (mk) trim(mk);
		sadd(&s, ",\"crashed\":true,\"crashes\":["); crash_list(&s, mk ? mk : "", 3); sadd(&s, "]"); free(mk);
	}
	sadd(&s, "}");
	free(log); free(name);
	reply_json(r->fd, &s);
}

/* ------------------------------------------------------------- Wi-Fi (wifi.sh drives wpa_supplicant)
 * Names and passwords go to wifi.sh through a root-only file, never through a shell command line. */
static void api_wifi(struct req *r, const char *what)
{
	char *i = qparam(r->query, "if");
	if (i && *i && !safe_word(i, 16)) { reply_err(r->fd, 400, "bad interface"); return; }
	const char *ifn = i && *i ? i : "\"\"";
	char cmd[300];
	if (!strcmp(what, "status") || !strcmp(what, "scan") || !strcmp(what, "disconnect") || !strcmp(what, "reconnect")) {
		snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/wifi.sh %s %s 2>&1", what, ifn);
		char *o = run_out(cmd); str s = {0}; sadd(&s, "{\"ok\":true,\"raw\":"); sjson(&s, o ? o : ""); sadd(&s, "}"); free(o); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "country")) {
		char *c = qparam(r->query, "cc");
		if (!c || strlen(c) != 2 || !isupper((unsigned char)c[0]) || !isupper((unsigned char)c[1])) { if (!c || strcmp(c, "00")) { reply_err(r->fd, 400, "bad country"); return; } }
		snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/wifi.sh country %s %s 2>&1", ifn, c);
		char *o = run_out(cmd); trim(o); reply_ok(r->fd, o); free(o); return;
	}
	if (!strcmp(what, "connect") || !strcmp(what, "forget")) {
		if (!r->body || !r->blen || r->blen > 600) { reply_err(r->fd, 400, "bad request"); return; }
		const char *f = !strcmp(what, "connect") ? RUN "/wifi.req" : RUN "/wifi-forget.req";
		unlink(f);
		int fd = open(f, O_WRONLY | O_CREAT | O_EXCL, 0600);
		if (fd < 0 || write(fd, r->body, r->blen) != (ssize_t)r->blen) { if (fd >= 0) close(fd); reply_err(r->fd, 500, "could not write the request"); return; }
		close(fd);
		snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/wifi.sh %s %s %s", what, ifn, f);
		if (!strcmp(what, "connect")) { if (job_running()) { reply_err(r->fd, 409, "busy, try again in a moment"); unlink(f); return; } start_job("wifi", cmd); reply_ok(r->fd, "connecting"); return; }
		strncat(cmd, " 2>&1", sizeof cmd - strlen(cmd) - 1);
		char *o = run_out(cmd); trim(o); reply_ok(r->fd, o); free(o); return;
	}
	reply_err(r->fd, 404, "unknown Wi-Fi action");
}

/* ------------------------------------------------------------- Linux apps from Flathub
 * Any verified Flathub app (or any Flathub app, when an admin allows unverified ones) can be installed
 * system-wide into /var/lib/flatpak on RUDOLFDATA. rudolf-apps reads the catalogue Flatpak downloads and
 * the launchers installed apps export; rudolf-display manages their windows (/run/rudolf/windows). */
#ifndef FLATHUB_REPO
#define FLATHUB_REPO "https://dl.flathub.org/repo/flathub.flatpakrepo"
#endif
#define CATALOG "/var/lib/flatpak/rudolf-catalog.json"
#define APPSTREAM_GZ "/var/lib/flatpak/appstream/flathub/x86_64/active/appstream.xml.gz"
#define APPSCONF PERSIST "/os/apps.conf"
static int app_id_ok(const char *id)
{
	if (!id || strlen(id) < 3 || strlen(id) > 200 || !strchr(id, '.') || id[0] == '.' || id[0] == '-') return 0;
	for (const char *c = id; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("._-", *c)) return 0;
	return 1;
}
static int apps_setting(const char *key)
{
	char *t = slurp(APPSCONF, NULL); int v = 0; char k[40]; snprintf(k, sizeof k, "%s=1", key);
	if (t) { v = strstr(t, k) != NULL; free(t); } return v;
}
/* verified on Flathub, according to the catalogue */
static int app_verified(const char *id)
{
	char *c = slurp(CATALOG, NULL); if (!c) return 0;
	char pat[260]; snprintf(pat, sizeof pat, "{\"id\":\"%s\",", id);
	char *a = strstr(c, pat), *e = a ? strchr(a, '}') : NULL; int v = 0;
	if (a && e) { *e = 0; v = strstr(a, "\"verified\":true") != NULL; }
	free(c); return v;
}
static void catalog_fresh(void)
{
	struct stat a, c;
	if (stat(APPSTREAM_GZ, &a)) return;
	if (!stat(CATALOG, &c) && c.st_mtime >= a.st_mtime) return;
	run("/usr/bin/rudolf-apps catalog " CATALOG " >/dev/null 2>&1");
}
/* 1.2.5: a Flatpak download that stops (Wi-Fi drops, the router restarts) is tried again up to three times;
 * Flatpak keeps the objects it already fetched, so each try continues where the last one stopped */
#define FP_INSTALL "n=0; until flatpak install --system -y --noninteractive flathub %s; do n=$((n+1)); [ $n -lt 3 ] || exit 1; " \
	"echo \"The download stopped. Trying again in 10 seconds; what was already downloaded is kept...\"; sleep 10; done; echo Done."
static void reply_cmd_json(struct req *r, const char *cmd, const char *fail)
{
	char *o = run_out(cmd);
	if (!o || o[0] != '{') { free(o); reply_err(r->fd, 500, fail); return; }
	reply(r->fd, 200, "application/json", o, strlen(o), NULL); free(o);
}
static void api_apps(struct req *r, const char *what)
{
	char cmd[2400];
	int fp = !access("/usr/bin/flatpak", X_OK), persist = !access(RUN "/persistent", F_OK);
	if (!strcmp(what, "status")) {
		struct statvfs v; long long freeb = statvfs("/var/lib/flatpak", &v) ? 0 : (long long)v.f_bavail * v.f_frsize;
		struct stat st; long long age = stat(APPSTREAM_GZ, &st) ? -1 : (long long)(time(NULL) - st.st_mtime);
		str s = {0}; sadd(&s, "{\"ok\":true,\"flatpak\":%s,\"persistent\":%s,\"free\":%lld,\"catalogAge\":%lld,\"auto\":%s,\"unverified\":%s}",
			fp ? "true" : "false", persist ? "true" : "false", freeb, age, apps_setting("auto") || access(APPSCONF, F_OK) ? "true" : "false", apps_setting("unverified") ? "true" : "false");
		reply_json(r->fd, &s); return;
	}
	if (!fp) { reply_err(r->fd, 404, "this Rudolf OS cannot run Linux apps"); return; }
	if (!strcmp(what, "catalog")) {
		catalog_fresh();
		if (access(CATALOG, R_OK)) { reply_err(r->fd, 404, "the app catalogue has not been downloaded yet"); return; }
		serve_file(r, CATALOG); return;
	}
	if (!strcmp(what, "installed")) { reply_cmd_json(r, "/usr/bin/rudolf-apps installed 2>/dev/null", "could not list the installed apps"); return; }
	if (!strcmp(what, "windows")) {
		char *w = slurp(RUN "/windows", NULL); str s = {0}; sadd(&s, "{\"ok\":true,\"windows\":["); int n = 0;
		char *m = slurp(RUN "/wm", NULL); unsigned long shown = 0; if (m && !strncmp(m, "win ", 4)) shown = strtoul(m + 4, NULL, 16);
		for (char *sv, *l = w ? strtok_r(w, "\n", &sv) : NULL; l; l = strtok_r(NULL, "\n", &sv)) {
			char *f[5] = {0}; int k = 0; char *sp2; for (char *t = strtok_r(l, "\t", &sp2); t && k < 5; t = strtok_r(NULL, "\t", &sp2)) f[k++] = t;
			if (k < 4) continue;
			sadd(&s, "%s{\"win\":", n++ ? "," : ""); sjson(&s, f[0]); sadd(&s, ",\"web\":%s,\"shown\":%s,\"class\":", !strcmp(f[1], "1") ? "true" : "false", strtoul(f[0], NULL, 16) == shown ? "true" : "false");
			sjson(&s, f[2]); sadd(&s, ",\"instance\":"); sjson(&s, f[3]); sadd(&s, ",\"title\":"); sjson(&s, k > 4 ? f[4] : ""); sadd(&s, "}");
		}
		sadd(&s, "],\"mode\":"); if (m) m[strcspn(m, "\n")] = 0; sjson(&s, m ? m : "desktop"); sadd(&s, "}");
		free(w); free(m); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "show") || !strcmp(what, "close")) {
		const char *wq = qparam(r->query, "win"); char *e; unsigned long x = wq ? strtoul(wq, &e, 16) : 0;
		if (!x || *e) { reply_err(r->fd, 400, "bad window"); return; }
		char b[64]; snprintf(b, sizeof b, "%s %lx\n", !strcmp(what, "show") ? "win" : "close", x);
		write_str(!strcmp(what, "show") ? RUN "/wm" : RUN "/wmcmd", b); reply_ok(r->fd, "ok"); return;
	}
	if (!strcmp(what, "desktop")) { write_str(RUN "/wm", "desktop"); reply_ok(r->fd, "ok"); return; }
	if (!strcmp(what, "settings")) {
		if (!persist) { reply_err(r->fd, 409, "settings are kept only on an installed Rudolf OS"); return; }
		const char *a = qparam(r->query, "auto"); int au = a ? !strcmp(a, "1") : apps_setting("auto") || access(APPSCONF, F_OK);
		const char *u = qparam(r->query, "unverified"); int un = u ? !strcmp(u, "1") : apps_setting("unverified");
		char b[64]; snprintf(b, sizeof b, "auto=%d\nunverified=%d\n", au, un); write_str(APPSCONF, b); reply_ok(r->fd, "saved"); return;
	}
	const char *idq = qparam(r->query, "id"); char id[220] = ""; if (idq) snprintf(id, sizeof id, "%s", idq);
	if (!strcmp(what, "icon")) {
		/* an installed app's launcher icon, else the catalogue's cached icon */
		const char *ic = qparam(r->query, "icon"); char icon[220]; snprintf(icon, sizeof icon, "%s", ic && app_id_ok(ic) ? ic : id);
		if (!app_id_ok(icon) && !app_id_ok(id)) { reply_err(r->fd, 400, "bad app"); return; }
		static const char *const SZ[] = {"128x128", "256x256", "scalable", "96x96", "64x64", "512x512", "48x48", NULL};
		char f[600];
		for (int i = 0; SZ[i]; i++) for (int k = 0; k < 2; k++) {
			snprintf(f, sizeof f, "/var/lib/flatpak/exports/share/icons/hicolor/%s/apps/%s.%s", SZ[i], icon, k ? "svg" : "png");
			if (!access(f, R_OK)) { serve_file(r, f); return; }
		}
		if (app_id_ok(id)) for (int i = 0; i < 2; i++) {
			snprintf(f, sizeof f, "/var/lib/flatpak/appstream/flathub/x86_64/active/icons/%s/%s.png", i ? "64x64" : "128x128", id);
			if (!access(f, R_OK)) { serve_file(r, f); return; }
		}
		reply_err(r->fd, 404, "no icon"); return;
	}
	if (!strcmp(what, "refresh")) {
		if (!persist) { reply_err(r->fd, 409, "Linux apps need Rudolf OS installed (or a RUDOLFDATA partition)"); return; }
		if (job_running()) { reply_err(r->fd, 409, "another task is still running"); return; }
		start_job("catalog", "sh -c 'set -e; echo \"Adding the Flathub app source...\"; flatpak remotes --system --columns=name | grep -qx flathub || flatpak remote-add --system flathub " FLATHUB_REPO "; "
			"echo \"Downloading the app catalogue...\"; flatpak update --system --appstream flathub; /usr/bin/rudolf-apps catalog " CATALOG "'");
		reply_ok(r->fd, "started"); return;
	}
	if (!strcmp(what, "update")) {
		if (job_running()) { reply_err(r->fd, 409, "another task is still running"); return; }
		start_job("apps-update", "sh -c 'flatpak update --system -y --noninteractive && flatpak uninstall --system -y --noninteractive --unused; echo Done.'");
		reply_ok(r->fd, "started"); return;
	}
	if (!app_id_ok(id)) { reply_err(r->fd, 400, "bad app"); return; }
	if (!strcmp(what, "info")) { snprintf(cmd, sizeof cmd, "/usr/bin/rudolf-apps info %s --remote 2>/dev/null", id); reply_cmd_json(r, cmd, "no details for this app"); return; }
	if (!strcmp(what, "size")) { snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/appsize.sh %s 2>/dev/null", id); reply_cmd_json(r, cmd, "the download size is not known"); return; }
	if (!strcmp(what, "install") || !strcmp(what, "uninstall")) {
		if (!persist) { reply_err(r->fd, 409, "Linux apps need Rudolf OS installed (or a RUDOLFDATA partition)"); return; }
		if (job_running()) { reply_err(r->fd, 409, "another task is still running"); return; }
		if (!strcmp(what, "install")) {
			if (!app_verified(id) && !apps_setting("unverified") && strcmp(id, "org.vinegarhq.Sober")) { reply_err(r->fd, 403, "this app is not verified on Flathub; an administrator can allow unverified apps in App Store > Installed"); return; }
			snprintf(cmd, sizeof cmd, "sh -c 'set -e; flatpak remotes --system --columns=name | grep -qx flathub || flatpak remote-add --system flathub " FLATHUB_REPO "; "
				"echo \"Downloading %s and anything it needs...\"; " FP_INSTALL "'", id, id);
		} else snprintf(cmd, sizeof cmd, "sh -c 'set -e; flatpak uninstall --system -y --noninteractive %s; flatpak uninstall --system -y --noninteractive --unused; echo Done.'", id);
		start_job(what, cmd); reply_ok(r->fd, "started"); return;
	}
	if (!strcmp(what, "run")) {
		/* started as the desktop user through its launcher (the app's own command and options) */
		const char *dq = qparam(r->query, "desktop"); char desk[220]; snprintf(desk, sizeof desk, "%s", dq && app_id_ok(dq) ? dq : id);
		run("mkdir -p " RUN "/apps; chown rudolf:rudolf " RUN "/apps; chmod 755 " RUN "/apps");
		char argf[300] = "";
		const char *fq = qparam(r->query, "file");
		if (fq && *fq) {
			if (!safe_media_path(fq)) { reply_err(r->fd, 403, "only files in Documents, Pictures, Downloads or on drives can be opened"); return; }
			static unsigned seq; snprintf(argf, sizeof argf, RUN "/apps/arg-%d-%u", (int)getpid(), ++seq);
			write_str(argf, fq); if (chown(argf, 1000, 1000)) {}
		}
		snprintf(cmd, sizeof cmd, "setsid su -s /bin/sh rudolf -c 'set -a; . " RUN "/session.env; set +a; exec /usr/bin/rudolf-apps launch %s %s' </dev/null >>" RUN "/apps/%s.log 2>&1 &",
			desk, argf, id);
		run(cmd); reply_ok(r->fd, "started"); return;
	}
	if (!strcmp(what, "log")) { char f[300]; snprintf(f, sizeof f, RUN "/apps/%s.log", id); char *l = slurp(f, NULL); str s = {0}; sadd(&s, "{\"ok\":true,\"log\":"); sjson(&s, l ? (strlen(l) > 8000 ? l + strlen(l) - 8000 : l) : ""); sadd(&s, "}"); free(l); reply_json(r->fd, &s); return; }
	reply_err(r->fd, 404, "unknown app action");
}

/* ------------------------------------------------------------- the desktop portal's questions
 * rudolf-portal (running as the desktop user) leaves requests in /run/rudolf/portal/req/<n>.json:
 * file dialogs, links to open, notifications. The desktop collects them here and answers into
 * /run/rudolf/portal/resp/<n>.json. While a dialog is open the desktop is shown; afterwards the
 * window that asked comes back. */
#define PORTALD RUN "/portal"
static void api_portal(struct req *r, const char *what)
{
	if (!strcmp(what, "poll")) {
		DIR *d = opendir(PORTALD "/req"); str s = {0}; sadd(&s, "{\"ok\":true,\"requests\":["); int n = 0; struct dirent *e;
		while (d && (e = readdir(d)) && n < 20) {
			size_t l = strlen(e->d_name); if (l < 6 || strcmp(e->d_name + l - 5, ".json")) continue;
			char f[400]; snprintf(f, sizeof f, PORTALD "/req/%s", e->d_name);
			char *t = slurp(f, NULL); if (!t || t[0] != '{') { free(t); continue; }
			sadd(&s, "%s{\"id\":", n++ ? "," : ""); char id[64]; snprintf(id, sizeof id, "%.*s", (int)(l - 5), e->d_name); sjson(&s, id); sadd(&s, ",\"req\":%s}", t); free(t);
		}
		if (d) closedir(d);
		sadd(&s, "]}"); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "answer")) {
		const char *iq = qparam(r->query, "id"); char id[64];
		if (!iq || !safe_word(iq, 40)) { reply_err(r->fd, 400, "bad request id"); return; }
		snprintf(id, sizeof id, "%s", iq);
		if (r->blen < 2 || r->blen > 65536 || r->body[0] != '{') { reply_err(r->fd, 400, "bad answer"); return; }
		char f[300], t[320], q[300]; snprintf(f, sizeof f, PORTALD "/resp/%s.json", id); snprintf(t, sizeof t, "%s.tmp", f); snprintf(q, sizeof q, PORTALD "/req/%s.json", id);
		int fd = open(t, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
		if (fd < 0 || write(fd, r->body, r->blen) != (ssize_t)r->blen) { if (fd >= 0) close(fd); reply_err(r->fd, 500, "could not answer"); return; }
		close(fd); if (chown(t, 1000, 1000)) {} rename(t, f); unlink(q);
		/* back to the window that asked */
		const char *back = qparam(r->query, "back");
		if (back && *back) { char *e; unsigned long x = strtoul(back, &e, 16); if (x && !*e) { char b[40]; snprintf(b, sizeof b, "win %lx\n", x); write_str(RUN "/wm", b); } }
		reply_ok(r->fd, "ok"); return;
	}
	reply_err(r->fd, 404, "unknown portal action");
}

/* ------------------------------------------------------------- the desktop's databases, mirrored on disk
 * The desktop keeps its files (drive C:), game ROMs and saves in IndexedDB. Rudolf OS 1.2 runs the desktop inside
 * the shell, whose storage is separate from the plain desktop page's, so every write is also sent here and kept
 * as one file per record; a desktop whose copy is older than the mirror (the shell's first start, or the plain
 * page after the shell failed) loads it. Like the localStorage copy (LSFILE), it also survives power cuts.
 *   GET  idbm/meta          {"gen":N,"count":M,"ready":bool}     ready: the plain page has written everything
 *   GET  idbm/list          {"gen":N,"items":["<16 hex>",...]}
 *   GET  idbm/get?f=HEX     one record (JSON written by the desktop: {"db","store","key","v"})
 *   POST idbm/put?f=HEX     store a record (body)             -> {"gen":N}
 *   POST idbm/del?f=HEX     forget a record                   -> {"gen":N}
 *   POST idbm/clear         forget everything (factory reset)  -> {"gen":N}
 *   POST idbm/ready         the full copy is complete: the shell may be used */
#define IDBM "/home/rudolf/.rudolf-idb"
#define SHELL_READY "/home/rudolf/.rudolf-shell-ready"
static int idbm_name(const char *f) { if (!f || strlen(f) != 16) return 0; for (int i = 0; i < 16; i++) if (!isxdigit((unsigned char)f[i]) || isupper((unsigned char)f[i])) return 0; return 1; }
static long idbm_gen(void) { char *g = slurp(IDBM "/gen", NULL); long v = g ? atol(g) : 0; free(g); return v; }
static long idbm_bump(void) { long v = idbm_gen() + 1; char b[32]; snprintf(b, sizeof b, "%ld\n", v); write_str(IDBM "/gen.tmp", b); rename(IDBM "/gen.tmp", IDBM "/gen"); return v; }
static void api_idbm(struct req *r, const char *what, int post)
{
	mkdir(IDBM, 0700);
	const char *f = qparam(r->query, "f");
	char path[256]; str s = {0};
	if (!strcmp(what, "meta") || !strcmp(what, "list")) {
		DIR *d = opendir(IDBM); int n = 0, first = 1;
		sadd(&s, "{\"ok\":true,\"gen\":%ld,\"ready\":%s", idbm_gen(), access(SHELL_READY, F_OK) ? "false" : "true");
		if (!strcmp(what, "list")) sadd(&s, ",\"items\":[");
		struct dirent *e;
		while (d && (e = readdir(d))) {
			size_t l = strlen(e->d_name);
			if (l != 21 || strcmp(e->d_name + 16, ".json")) continue;
			n++;
			if (!strcmp(what, "list")) { sadd(&s, "%s\"%.16s\"", first ? "" : ",", e->d_name); first = 0; }
		}
		if (d) closedir(d);
		if (!strcmp(what, "list")) sadd(&s, "]");
		sadd(&s, ",\"count\":%d}", n); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "get")) {
		if (!idbm_name(f)) { reply_err(r->fd, 400, "bad record"); return; }
		snprintf(path, sizeof path, IDBM "/%s.json", f); size_t n; char *b = slurp(path, &n);
		if (!b) { reply_err(r->fd, 404, "no such record"); return; }
		reply(r->fd, 200, "application/json", b, n, NULL); free(b); return;
	}
	if (!post) { reply_err(r->fd, 400, "use POST"); return; }
	if (!strcmp(what, "put")) {
		if (!idbm_name(f) || !r->body || r->blen < 2 || r->body[0] != '{') { reply_err(r->fd, 400, "bad record"); return; }
		char tmp[256]; snprintf(tmp, sizeof tmp, IDBM "/.%s.tmp", f); snprintf(path, sizeof path, IDBM "/%s.json", f);
		int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
		if (fd < 0 || write(fd, r->body, r->blen) != (ssize_t)r->blen || fsync(fd)) { if (fd >= 0) close(fd); unlink(tmp); reply_err(r->fd, 507, "the disk is full"); return; }
		close(fd); rename(tmp, path);
	} else if (!strcmp(what, "del")) {
		if (!idbm_name(f)) { reply_err(r->fd, 400, "bad record"); return; }
		snprintf(path, sizeof path, IDBM "/%s.json", f); unlink(path);
	} else if (!strcmp(what, "clear")) {
		run("find " IDBM " -maxdepth 1 -name '*.json' -delete");
		unlink(SHELL_READY);
	} else if (!strcmp(what, "ready")) {
		write_str(SHELL_READY, "1\n"); reply_ok(r->fd, "ready"); return;
	} else { reply_err(r->fd, 404, "unknown mirror action"); return; }
	sadd(&s, "{\"ok\":true,\"gen\":%ld}", idbm_bump()); reply_json(r->fd, &s);
}

/* ------------------------------------------------------------- the shell (Rudolf OS 1.2)
 *   GET  shell              {"id","installed","ready","active","failed","on"}
 *   POST shell?on=0|1       Settings: use the shell (real browser tabs in Oracle) or not, from the next start
 *   POST shell/switch       start the browser again now (with the shell, when it can be used) */
/* the camera (1.2.8): Settings > Privacy > Camera. Off unloads the webcam driver (uvcvideo) and keeps it from loading
 * again (/usr/lib/rudolf/camera: its file covered, kept on RUDOLFDATA), so nothing on this computer can use the camera,
 * websites and Linux apps included, until it is switched on. GET camera: {on, devices (video devices now), loaded} */
static void api_camera(struct req *r, int post)
{
	if (post) {
		const char *on = qparam(r->query, "on"); if (!on || (strcmp(on, "0") && strcmp(on, "1"))) { reply_err(r->fd, 400, "on=0 or on=1"); return; }
		char cmd[64]; snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/camera %s 2>&1", strcmp(on, "0") ? "on" : "off");
		FILE *p = popen(cmd, "r"); char out[200] = ""; if (p) { if (!fgets(out, sizeof out, p)) out[0] = 0; int st = pclose(p); trim(out);
			if (WIFEXITED(st) && !WEXITSTATUS(st)) { reply_ok(r->fd, out); return; } }
		reply_err(r->fd, 409, *out ? out : "the camera could not be switched"); return;
	}
	int n = 0; DIR *d = opendir("/sys/class/video4linux"); struct dirent *e;
	while (d && (e = readdir(d))) if (!strncmp(e->d_name, "video", 5)) n++;
	if (d) closedir(d);
	char *m = slurp("/proc/modules", NULL); int loaded = m && (!strncmp(m, "uvcvideo ", 9) || strstr(m, "\nuvcvideo ")); free(m);
	str s = {0}; sadd(&s, "{\"ok\":true,\"on\":%s,\"devices\":%d,\"loaded\":%s}", access(RUN "/camera-off", F_OK) ? "true" : "false", n, loaded ? "true" : "false");
	reply_json(r->fd, &s);
}
static void api_shell(struct req *r, const char *what, int post)
{
	char *id = slurp(WWW "/iwa/id", NULL); if (id) id[strcspn(id, "\n")] = 0;
	if (!*what && !post) {
		char cmd[256]; int inst = 0;
		if (id && strlen(id) == 56 && strspn(id, "abcdefghijklmnopqrstuvwxyz234567") == 56) {
			snprintf(cmd, sizeof cmd, "grep -lqs %s /home/rudolf/.rudolf-browser/Default/iwa/*/main.swbn", id); inst = run(cmd) == 0;
		}
		char *url = slurp(RUN "/desktop-url", NULL), *off = slurp("/etc/rudolf-shell", NULL);
		str s = {0}; sadd(&s, "{\"ok\":true,\"id\":"); sjson(&s, id ? id : "");
		sadd(&s, ",\"installed\":%s,\"ready\":%s,\"active\":%s,\"failed\":%s,\"on\":%s}", inst ? "true" : "false",
			access(SHELL_READY, F_OK) ? "false" : "true", url && !strncmp(url, "isolated-app://", 15) ? "true" : "false",
			access(RUN "/shell-failed", F_OK) ? "false" : "true", off && !strncmp(off, "off", 3) ? "false" : "true");
		free(url); free(off); free(id); reply_json(r->fd, &s); return;
	}
	free(id);
	if (!post) { reply_err(r->fd, 400, "use POST"); return; }
	if (!*what) {
		const char *on = qparam(r->query, "on"); if (!on) { reply_err(r->fd, 400, "on?"); return; }
		const char *v = !strcmp(on, "0") ? "off\n" : "on\n";
		write_str("/etc/rudolf-shell", v);
		if (!live) { mkdir(PERSIST "/etc", 0755); write_str(PERSIST "/etc/rudolf-shell", v); }
		if (!strcmp(on, "1")) unlink(RUN "/shell-failed");
		reply_ok(r->fd, "saved"); return;
	}
	if (!strcmp(what, "/switch")) { write_str(RUN "/shell-switch", "1\n"); reply_ok(r->fd, "the browser starts again"); return; }
	reply_err(r->fd, 404, "unknown shell action");
}

/* the text on the clipboard (the X CLIPBOARD selection, shared with Linux apps): the desktop inside the shell
   cannot read the clipboard itself */
static void api_clipboard(struct req *r)
{
	char *o = run_out("su -s /bin/sh rudolf -c 'DISPLAY=:0 timeout 3 rudolf-display --clip-get' 2>/dev/null");
	str s = {0}; sadd(&s, "{\"ok\":true,\"text\":"); sjson(&s, o ? o : ""); sadd(&s, "}"); free(o); reply_json(r->fd, &s);
}

/* ------------------------------------------------------------- browser window for sign-in sites
 * Oracle shows websites inside the desktop page. Sites that refuse that (Gmail, Outlook, banks: their
 * sign-in needs first-party cookies) open in a second Chromium as a real browser window, with its own
 * profile in the user's home. rudolf-display shows it above the desktop and leaves the taskbar visible;
 * the taskbar button or Super+Esc returns to the desktop. */
#define WEBPROFILE "/home/rudolf/.config/rudolf-web"
static int web_running(void) { return run("pgrep -f 'chrom[e].*--class=RudolfWeb' >/dev/null 2>&1") == 0; }
static int web_url_ok(const char *u)
{
	if (!u || strlen(u) > 2000 || (strncmp(u, "https://", 8) && strncmp(u, "http://", 7))) return 0;
	for (const unsigned char *c = (const unsigned char *)u; *c; c++) if (*c <= 0x20 || *c >= 0x7f || strchr("'\"\\`$", *c)) return 0;
	return 1;
}
static void api_web(struct req *r, const char *what)
{
	char cmd[3200];
	if (!strcmp(what, "status")) {
		char *m = slurp(RUN "/wm", NULL); int run_ = web_running(), shown = run_ && m && !strncmp(m, "web", 3);
		/* rudolf-display shows a new window as "win ID": shown too when that window is the browser window
		   (/run/rudolf/windows: "ID<tab>is-web<tab>..." per line) */
		if (run_ && m && !strncmp(m, "win ", 4)) {
			char id[32]; snprintf(id, sizeof id, "%.20s", m + 4); id[strcspn(id, " \n")] = 0;
			char *w = slurp(RUN "/windows", NULL);
			for (char *l = w; l && *l && !shown; ) { char *nl = strchr(l, '\n'); size_t n = strlen(id); if (!strncmp(l, id, n) && l[n] == '\t' && l[n + 1] == '1') shown = 1; l = nl ? nl + 1 : NULL; }
			free(w);
		}
		str s = {0}; sadd(&s, "{\"ok\":true,\"running\":%s,\"shown\":%s}", run_ ? "true" : "false", shown ? "true" : "false");
		free(m); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "open")) {
		const char *u = qparam(r->query, "url"); if (!web_url_ok(u)) { reply_err(r->fd, 400, "that address cannot be opened"); return; }
		char *gl = slurp(RUN "/glflags", NULL), *px = slurp(RUN "/proxy", NULL);
		if (gl) gl[strcspn(gl, "\n'")] = 0; if (px) px[strcspn(px, "\n' ")] = 0;
		/* starting it again while it runs just opens the address as a new tab in the running window */
		snprintf(cmd, sizeof cmd, "mkdir -p " WEBPROFILE "; chown rudolf:rudolf /home/rudolf/.config " WEBPROFILE " 2>/dev/null; "
			"setsid su -s /bin/sh rudolf -c 'set -a; . " RUN "/session.env; set +a; FONTCONFIG_FILE=/etc/fonts/fonts.conf GOOGLE_API_KEY=no GOOGLE_DEFAULT_CLIENT_ID=no GOOGLE_DEFAULT_CLIENT_SECRET=no; export FONTCONFIG_FILE GOOGLE_API_KEY GOOGLE_DEFAULT_CLIENT_ID GOOGLE_DEFAULT_CLIENT_SECRET; "
			"exec /opt/chromium/chrome --class=RudolfWeb --user-data-dir=" WEBPROFILE " --no-first-run --no-default-browser-check "
			"--password-store=basic --disable-session-crashed-bubble --disable-features=Translate,MediaRouter,Vulkan --disable-vulkan-surface "
			/* Ruffle plays Flash here too; the web frames extension is not loaded, so sites keep their protections */
			"--load-extension=" RUN "/ruffle-ext %s %s%s \"%s\"' "
			"</dev/null >>" RUN "/web.log 2>&1 &", gl ? gl : "", px && *px ? "--proxy-server=" : "", px && *px ? px : "", u);
		free(gl); free(px);
		write_str(RUN "/wm", "web"); run(cmd);
		reply_ok(r->fd, "opened"); return;
	}
	if (!strcmp(what, "show")) { if (!web_running()) { reply_err(r->fd, 404, "the browser window is closed"); return; } write_str(RUN "/wm", "web"); reply_ok(r->fd, "ok"); return; }
	if (!strcmp(what, "hide")) { write_str(RUN "/wm", "desktop"); reply_ok(r->fd, "ok"); return; }
	if (!strcmp(what, "close")) { run("pkill -f 'chrom[e].*--class=RudolfWeb'; for i in 1 2 3 4 5 6 7 8; do pgrep -f 'chrom[e].*--class=RudolfWeb' >/dev/null || break; sleep 1; done; pkill -9 -f 'chrom[e].*--class=RudolfWeb'; pkill -9 -f 'chrom[e].*rudolf-web'"); write_str(RUN "/wm", "desktop"); reply_ok(r->fd, "closed"); return; }
	if (!strcmp(what, "strut")) {
		const char *sd = qparam(r->query, "side"); char side[8]; snprintf(side, sizeof side, "%s", sd && !strcmp(sd, "top") ? "top" : "bottom");
		const char *sz = qparam(r->query, "size"); int n = sz ? atoi(sz) : 0; if (n < 0 || n > 200) n = 0;
		char b[32]; snprintf(b, sizeof b, "%s %d\n", side, n); write_str(RUN "/strut", b);
		char *m = slurp(RUN "/wm", NULL); write_str(RUN "/wm", m && *m ? m : "desktop\n"); free(m);   /* the window manager re-reads both */
		reply_ok(r->fd, "ok"); return;
	}
	reply_err(r->fd, 404, "unknown browser window action");
}

/* ------------------------------------------------------------- printing
 * PDF: the page WordPro builds is printed by a headless Chromium (the one on the disc), running as the
 * desktop user. Printers: rudolf-print talks IPP Everywhere / AirPrint to network printers (no CUPS). */
static int print_uri_ok(const char *u)
{
	if (!u || strlen(u) > 300 || (strncmp(u, "ipp://", 6) && strncmp(u, "ipps://", 7))) return 0;
	for (; *u; u++) if (!isalnum((unsigned char)*u) && !strchr(":/._-~%[]", *u)) return 0;
	return 1;
}
static int print_tmp(char *d)
{
	strcpy(d, "/tmp/rprint-XXXXXX"); if (!mkdtemp(d)) return -1;
	char cmd[200]; snprintf(cmd, sizeof cmd, "chown rudolf:rudolf %s 2>/dev/null; chmod 700 %s", d, d); run(cmd); return 0;
}
static void api_print(struct req *r, const char *what)
{
	char cmd[2000], d[32], f[96];
	if (!strcmp(what, "pdf")) {
		if (r->blen < 16 || r->blen > (64u << 20)) { reply_err(r->fd, 400, "no document"); return; }
		if (print_tmp(d)) { reply_err(r->fd, 500, "no room for the print job"); return; }
		snprintf(f, sizeof f, "%s/doc.html", d);
		int fd = open(f, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
		if (fd < 0 || write(fd, r->body, r->blen) != (ssize_t)r->blen) { if (fd >= 0) close(fd); reply_err(r->fd, 500, "could not store the document"); return; }
		close(fd);
		snprintf(cmd, sizeof cmd, "chown rudolf:rudolf %s; su -s /bin/sh rudolf -c 'FONTCONFIG_FILE=/etc/fonts/fonts.conf timeout 120 /opt/chromium/chrome"
			" --headless=new --disable-gpu --no-first-run --no-default-browser-check --disable-extensions --disable-sync --mute-audio"
			" --no-pdf-header-footer --run-all-compositor-stages-before-draw --virtual-time-budget=4000 --user-data-dir=%s/profile"
			" --print-to-pdf=%s/out.pdf file://%s/doc.html' >%s/log.txt 2>&1", f, d, d, d, d);
		run(cmd);
		size_t n = 0; snprintf(f, sizeof f, "%s/out.pdf", d); char *pdf = slurp(f, &n);
		if (!pdf || n < 64 || memcmp(pdf, "%PDF", 4)) reply_err(r->fd, 500, "the PDF could not be made");
		else reply(r->fd, 200, "application/pdf", pdf, n, NULL);
		free(pdf); snprintf(cmd, sizeof cmd, "rm -rf %s", d); run(cmd); return;
	}
	if (access("/usr/bin/rudolf-print", X_OK)) { reply_err(r->fd, 404, "this Rudolf OS cannot print"); return; }
	if (!strcmp(what, "find")) {
		char *o = run_out("/usr/bin/rudolf-print find 2>/dev/null");
		if (!o || o[0] != '{') { free(o); reply_err(r->fd, 500, "printer search failed"); return; }
		reply(r->fd, 200, "application/json", o, strlen(o), NULL); free(o); return;
	}
	char uri[320]; { const char *u = qparam(r->query, "uri"); if (!print_uri_ok(u)) { reply_err(r->fd, 400, "a printer address looks like ipp://192.168.1.20/ipp/print"); return; } snprintf(uri, sizeof uri, "%s", u); }
	if (!strcmp(what, "info")) {
		snprintf(cmd, sizeof cmd, "/usr/bin/rudolf-print info '%s' 2>/dev/null", uri);
		char *o = run_out(cmd); if (!o || o[0] != '{') { free(o); reply_err(r->fd, 500, "the printer could not be asked"); return; }
		reply(r->fd, 200, "application/json", o, strlen(o), NULL); free(o); return;
	}
	if (!strcmp(what, "job")) {
		static const char *const FMT[] = {"application/pdf", "image/pwg-raster", "image/urf", NULL};
		char fmt[40] = "", opt[400] = ""; const char *q = qparam(r->query, "format");
		for (int i = 0; FMT[i]; i++) if (q && !strcmp(q, FMT[i])) snprintf(fmt, sizeof fmt, "%s", FMT[i]);
		if (!*fmt) { reply_err(r->fd, 400, "unknown document format"); return; }
		static const char *const K[] = {"copies", "sides", "color", "media", "pages", NULL};
		for (int i = 0; K[i]; i++) { q = qparam(r->query, K[i]); if (q && *q && safe_word(q, 60)) { size_t l = strlen(opt); snprintf(opt + l, sizeof opt - l, " %s=%s", K[i], q); } }
		if (r->blen < 16) { reply_err(r->fd, 400, "no document"); return; }
		if (print_tmp(d)) { reply_err(r->fd, 500, "no room for the print job"); return; }
		snprintf(f, sizeof f, "%s/title.txt", d); q = qparam(r->query, "title");
		{ char t[160]; snprintf(t, sizeof t, "%s", q && *q ? q : "Document"); for (char *c = t; *c; c++) if ((unsigned char)*c < 32) *c = ' '; write_str(f, t); }
		snprintf(f, sizeof f, "%s/doc", d);
		int fd = open(f, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
		if (fd < 0 || write(fd, r->body, r->blen) != (ssize_t)r->blen) { if (fd >= 0) close(fd); reply_err(r->fd, 500, "could not store the print job (is the memory full?)"); return; }
		close(fd);
		snprintf(cmd, sizeof cmd, "/usr/bin/rudolf-print send '%s' %s/doc %s%s title-file=%s/title.txt 2>/dev/null", uri, d, fmt, opt, d);
		char *o = run_out(cmd); snprintf(cmd, sizeof cmd, "rm -rf %s", d); run(cmd);
		if (!o || o[0] != '{') { free(o); reply_err(r->fd, 500, "sending to the printer failed"); return; }
		reply(r->fd, 200, "application/json", o, strlen(o), NULL); free(o); return;
	}
	reply_err(r->fd, 404, "unknown print action");
}

/* ------------------------------------------------------------- native apps (Flatpak)
 * Only apps on this list can be installed or started through rudolfd. They come from Flathub
 * and are installed system-wide into /var/lib/flatpak (the RUDOLFDATA partition), never shipped
 * on the disc. Sober is a closed-source runtime for Roblox by the Vinegar project. */
static const char *const NATIVE_APPS[] = { "org.vinegarhq.Sober", NULL };
#ifndef FLATHUB_REPO
#define FLATHUB_REPO "https://dl.flathub.org/repo/flathub.flatpakrepo"
#endif
static int native_ok(const char *id)
{
	if (!id) return 0;
	for (int i = 0; NATIVE_APPS[i]; i++) if (!strcmp(id, NATIVE_APPS[i])) return 1;
	return 0;
}
/* The app is started with setsid; that process becomes bubblewrap (the sandbox monitor) and lives
 * until the app exits, so its pid (recorded at start) tells whether the app is running. */
static int native_pid(void)
{
	char *t = slurp(RUN "/native.pid", NULL);
	int pid = t ? atoi(t) : 0; free(t);
	if (pid <= 1 || kill(pid, 0)) return 0;
	char path[64], cl[256] = {0}; snprintf(path, sizeof path, "/proc/%d/cmdline", pid);
	int f = open(path, O_RDONLY); if (f < 0) return 0;
	ssize_t n = read(f, cl, sizeof cl - 1); close(f);
	for (ssize_t i = 0; i < n; i++) if (!cl[i]) cl[i] = ' ';
	return strstr(cl, "bwrap") || strstr(cl, "flatpak") || strstr(cl, "su ") ? pid : 0;
}
static void api_native_status(struct req *r)
{
	char *id = qparam(r->query, "id");
	str s = {0};
	int fp = !access("/usr/bin/flatpak", X_OK), persist = !access(RUN "/persistent", F_OK);
	sadd(&s, "{\"ok\":true,\"flatpak\":%s,\"persistent\":%s,\"gpu\":", fp ? "true" : "false", persist ? "true" : "false");
	char *g = slurp(RUN "/gpu", NULL); if (g) { char *nl = strchr(g, '\n'); if (nl) *nl = 0; } sjson(&s, g ? g : ""); free(g);
	struct statvfs v; long long freeb = statvfs("/var/lib/flatpak", &v) ? 0 : (long long)v.f_bavail * v.f_frsize;
	sadd(&s, ",\"free\":%lld", freeb);
	if (fp && native_ok(id)) {
		char cmd[256]; snprintf(cmd, sizeof cmd, "flatpak info --system %s 2>/dev/null | sed -n 's/^ *Version: //p;s/^ *Installed: //p'", id);
		char *o = run_out(cmd);
		sadd(&s, ",\"installed\":%s,\"info\":", o && *o ? "true" : "false"); sjson(&s, o ? o : ""); free(o);
		sadd(&s, ",\"running\":%s", native_pid() ? "true" : "false");
	}
	sadd(&s, "}");
	reply_json(r->fd, &s);
}
static void api_native(struct req *r, const char *what)
{
	char *id = qparam(r->query, "id");
	if (!native_ok(id)) { reply_err(r->fd, 400, "this app is not available"); return; }
	if (access("/usr/bin/flatpak", X_OK)) { reply_err(r->fd, 404, "this Rudolf OS has no Flatpak support"); return; }
	char cmd[1024];
	if (!strcmp(what, "install") || !strcmp(what, "update") || !strcmp(what, "uninstall")) {
		if (access(RUN "/persistent", F_OK)) { reply_err(r->fd, 409, "install Rudolf OS (or add a RUDOLFDATA partition) first: apps need somewhere to be kept"); return; }
		if (job_running()) { reply_err(r->fd, 409, "another task is still running"); return; }
		if (!strcmp(what, "install"))
			snprintf(cmd, sizeof cmd, "sh -c 'set -e; echo \"Adding the Flathub app source...\"; flatpak remotes --system --columns=name | grep -qx flathub || flatpak remote-add --system flathub " FLATHUB_REPO "; "
				"echo \"Downloading %s and its runtime (this can take a while)...\"; " FP_INSTALL "'", id, id);
		else if (!strcmp(what, "update"))
			snprintf(cmd, sizeof cmd, "sh -c 'set -e; flatpak update --system -y --noninteractive; echo Done.'");
		else
			snprintf(cmd, sizeof cmd, "sh -c 'set -e; flatpak uninstall --system -y --noninteractive %s; flatpak uninstall --system -y --noninteractive --unused; echo Done.'", id);
		start_job(what, cmd);
		reply_ok(r->fd, "started");
		return;
	}
	if (!strcmp(what, "run")) {
		/* started as the desktop user with the session's display and buses; rudolf-display shows it full screen */
		snprintf(cmd, sizeof cmd, "setsid su -s /bin/sh rudolf -c 'set -a; . " RUN "/session.env; set +a; exec flatpak run %s' "
			"</dev/null >>" RUN "/native.log 2>&1 & echo $! > " RUN "/native.pid", id);
		if (native_pid()) { write_str(RUN "/wm", "game"); reply_ok(r->fd, "already running"); return; }
		{ char chk[160]; snprintf(chk, sizeof chk, "flatpak info --system %s >/dev/null 2>&1", id);
		  if (run(chk)) { reply_err(r->fd, 404, "this app is not installed"); return; } }
		run(cmd);
		write_str(RUN "/wm", "game");
		reply_ok(r->fd, "started");
		return;
	}
	if (!strcmp(what, "size")) { snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/appsize.sh %s 2>/dev/null", id); reply_cmd_json(r, cmd, "the download size is not known"); return; }
	if (!strcmp(what, "show")) { write_str(RUN "/wm", "game"); reply_ok(r->fd, "ok"); return; }
	if (!strcmp(what, "stop")) { int pid = native_pid(); if (pid) { kill(-pid, SIGTERM); kill(pid, SIGTERM); } write_str(RUN "/wm", "desktop"); reply_ok(r->fd, pid ? "stopped" : "not running"); return; }
	if (!strcmp(what, "log")) { char *l = slurp(RUN "/native.log", NULL); str s = {0}; sadd(&s, "{\"ok\":true,\"log\":"); sjson(&s, l ? (strlen(l) > 8000 ? l + strlen(l) - 8000 : l) : ""); sadd(&s, "}"); free(l); reply_json(r->fd, &s); return; }
	reply_err(r->fd, 404, "unknown action");
}


/* ------------------------------------------------------------- packages (rpk)
 * rpk does the work (see rpk.c); rudolfd starts it as a job for long tasks, runs programs from
 * packages as the desktop user, and serves web-app packages from /pkg/<name>/www/. */
static int pkg_names_ok(const char *s)
{
	if (!s || !*s || strlen(s) > 400) return 0;
	for (; *s; s++) if (!islower((unsigned char)*s) && !isdigit((unsigned char)*s) && !strchr("-_,", *s)) return 0;
	return 1;
}
static void b64(str *s, const unsigned char *d, size_t n)
{
	static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char q[5] = {0};
	for (size_t i = 0; i < n; i += 3) {
		unsigned v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
		q[0] = T[v >> 18 & 63]; q[1] = T[v >> 12 & 63]; q[2] = i + 1 < n ? T[v >> 6 & 63] : '='; q[3] = i + 2 < n ? T[v & 63] : '=';
		sadd(s, "%s", q);
	}
}
/* ------------------------------------------------------------- Emu's game library (/home/rudolf/Games)
 * games/info: folders made, space used and free; games/import: copy games in (games-import.sh, a job). */
static void api_games(struct req *r, const char *what)
{
	if (!strcmp(what, "info")) {
		run("mkdir -p /home/rudolf/Games/SNES/Saves /home/rudolf/Games/SNES/States /home/rudolf/Games/DS/Saves /home/rudolf/Games/DS/States && chown -R 1000:1000 /home/rudolf/Games");
		char *u = run_out("du -sk /home/rudolf/Games 2>/dev/null | cut -f1"); trim(u);
		struct statvfs v; long long fr = statvfs("/home/rudolf/Games", &v) ? 0 : (long long)v.f_bavail * v.f_frsize;
		str s = {0}; sadd(&s, "{\"ok\":true,\"used\":%lld,\"free\":%lld,\"persistent\":%s}", atoll(u) * 1024, fr, access(RUN "/persistent", F_OK) ? "false" : "true");
		free(u); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "import")) {
		if (job_running()) { reply_err(r->fd, 409, "another task is still running"); return; }
		if (!r->body || !r->blen || r->blen > 65536) { reply_err(r->fd, 400, "choose some games"); return; }
		/* every line must be a path the file endpoints may read */
		char *b = malloc(r->blen + 1); memcpy(b, r->body, r->blen); b[r->blen] = 0;
		for (char *sv, *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) if (*l && !safe_media_path(l)) { free(b); reply_err(r->fd, 403, "games can be added from USB drives and the shared folders only"); return; }
		free(b);
		unlink(RUN "/games.list");
		int f = open(RUN "/games.list", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
		if (f < 0 || write(f, r->body, r->blen) != (ssize_t)r->blen) { if (f >= 0) close(f); reply_err(r->fd, 500, "could not write the request"); return; }
		close(f); start_job("games-import", "/usr/lib/rudolf/games-import.sh " RUN "/games.list");
		reply_ok(r->fd, "started"); return;
	}
	reply_err(r->fd, 404, "unknown games action");
}
/* ------------------------------------------------------------- Debian 13 (rudolf-debian) and its terminal
 * The terminal is a long-poll: term/read waits up to 20 s for output; term/write sends keys. */
#define DEBIAN_BIN "/usr/bin/rudolf-debian"
static int term_id_ok(const char *id) { if (!id || !*id || strlen(id) > 32) return 0; for (const char *c = id; *c; c++) if (!isalnum((unsigned char)*c)) return 0; return 1; }
static int term_conn(const char *id)
{
	int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0); if (s < 0) return -1;
	struct sockaddr_un a = {.sun_family = AF_UNIX}; snprintf(a.sun_path, sizeof a.sun_path, "/run/rudolf/term/%s.sock", id);
	if (connect(s, (struct sockaddr *)&a, sizeof a)) { close(s); return -1; }
	struct timeval tv = {30, 0}; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	return s;
}
static size_t read_all(int s, unsigned char **out)
{
	size_t cap = 65536, n = 0; unsigned char *b = malloc(cap); ssize_t r;
	while ((r = read(s, b + n, cap - n)) > 0) { n += r; if (n == cap) { cap *= 2; b = realloc(b, cap); } }
	*out = b; return n;
}
static void spawn_debian(char *const argv[])
{
	pid_t p = fork();
	if (!p) { int n = open("/dev/null", O_RDWR); dup2(n, 0); dup2(n, 1); dup2(n, 2); execv(DEBIAN_BIN, argv); _exit(127); }
	if (p > 0) waitpid(p, NULL, 0);
}
static void api_term(struct req *r, const char *what)
{
	if (access(DEBIAN_BIN, X_OK)) { reply_err(r->fd, 404, "this Rudolf OS has no Linux terminal"); return; }
	if (!strcmp(what, "open")) {
		static const char A[] = "abcdefghijklmnopqrstuvwxyz0123456789"; char id[16]; unsigned char rb[12];
		int f = open("/dev/urandom", O_RDONLY | O_CLOEXEC); if (f < 0 || read(f, rb, sizeof rb) != sizeof rb) { if (f >= 0) close(f); reply_err(r->fd, 500, "no randomness"); return; } close(f);
		for (int i = 0; i < 12; i++) id[i] = A[rb[i] % 36]; id[12] = 0;
		char cmd[200]; snprintf(cmd, sizeof cmd, DEBIAN_BIN " pty %s 2>&1", id);
		char *o = run_out(cmd); trim(o);
		if (strcmp(o, id)) { reply_err(r->fd, 500, *o ? o : "the terminal did not start"); free(o); return; }
		free(o); str s = {0}; sadd(&s, "{\"ok\":true,\"id\":\"%s\"}", id); reply_json(r->fd, &s); return;
	}
	char id[40]; const char *iq = qparam(r->query, "id");
	if (!term_id_ok(iq)) { reply_err(r->fd, 400, "bad terminal"); return; }
	snprintf(id, sizeof id, "%s", iq);
	int s = term_conn(id);
	if (s < 0) { str j = {0}; sadd(&j, "{\"ok\":true,\"gone\":true}"); reply_json(r->fd, &j); return; }
	char line[96];
	if (!strcmp(what, "read")) {
		/* qparam returns one shared buffer: take each value before asking for the next */
		const char *o = qparam(r->query, "off"); long long off = o ? atoll(o) : 0;
		const char *w = qparam(r->query, "wait"); int wt = w ? atoi(w) : 20;
		snprintf(line, sizeof line, "R %lld %d\n", off, wt); send_all(s, line, strlen(line));
		unsigned char *b; size_t n = read_all(s, &b); close(s);
		unsigned char *nl = memchr(b, '\n', n); str j = {0};
		if (!nl) sadd(&j, "{\"ok\":true,\"off\":%lld,\"data\":\"\"}", off);
		else if (b[0] == 'X') sadd(&j, "{\"ok\":true,\"exit\":%d}", atoi((char *)b + 2));
		else { *nl = 0; sadd(&j, "{\"ok\":true,\"off\":%lld,\"data\":\"", atoll((char *)b)); b64(&j, nl + 1, n - (nl + 1 - b)); sadd(&j, "\"}"); }
		free(b); reply_json(r->fd, &j); return;
	}
	if (!strcmp(what, "write")) {
		if (r->blen > 65536) { close(s); reply_err(r->fd, 413, "too much input"); return; }
		snprintf(line, sizeof line, "W %zu\n", r->blen); send_all(s, line, strlen(line)); if (r->blen) send_all(s, r->body, r->blen);
	} else if (!strcmp(what, "resize")) {
		const char *rw = qparam(r->query, "rows"); int rows = rw ? atoi(rw) : 0;
		const char *cl = qparam(r->query, "cols"); int cols = cl ? atoi(cl) : 0;
		snprintf(line, sizeof line, "S %d %d\n", rows, cols); send_all(s, line, strlen(line));
	} else if (!strcmp(what, "close")) send_all(s, "K\n", 2);
	else { close(s); reply_err(r->fd, 404, "unknown terminal action"); return; }
	char ok[8]; if (read(s, ok, sizeof ok) < 0) {} close(s); reply_ok(r->fd, "ok");
}
static void api_debian(struct req *r, const char *what)
{
	if (access(DEBIAN_BIN, X_OK)) { reply_err(r->fd, 404, "this Rudolf OS cannot run Debian"); return; }
	if (!strcmp(what, "status")) {
		const char *sz = qparam(r->query, "size");
		char *o = run_out(sz && !strcmp(sz, "1") ? DEBIAN_BIN " status --size 2>/dev/null" : DEBIAN_BIN " status 2>/dev/null");
		str s = {0}; sadd(&s, "{\"ok\":true");
		for (char *sv, *l = strtok_r(o, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) {
			char *e = strchr(l, '='); if (!e) continue; *e = 0;
			int num = e[1] && strspn(e + 1, "0123456789") == strlen(e + 1);
			sadd(&s, ",\"%s\":", l); if (num) sadd(&s, "%s", e + 1); else sjson(&s, e + 1);
		}
		sadd(&s, ",\"job\":"); char *jn = slurp(RUN "/job.name", NULL); if (jn) trim(jn);
		sjson(&s, job_running() && jn ? jn : ""); free(jn); sadd(&s, "}"); free(o); reply_json(r->fd, &s); return;
	}
	/* 1.2.7: what Linux apps and the terminal need (rudolf-fscheck), as JSON strings */
	if (!strcmp(what, "check")) {
		char *o = run_out("/usr/bin/rudolf-fscheck 2>&1");
		str s = {0}; sadd(&s, "{\"ok\":true");
		for (char *sv, *l = strtok_r(o, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) {
			char *e = strchr(l, '='); if (!e) continue; *e = 0;
			sadd(&s, ",\"%s\":", l); sjson(&s, e + 1);
		}
		sadd(&s, "}"); free(o); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "setup") || !strcmp(what, "passwd") || !strcmp(what, "update") || !strcmp(what, "remove")) {
		if (job_running()) { reply_err(r->fd, 409, "another task is still running"); return; }
		if (!strcmp(what, "setup") || !strcmp(what, "passwd")) {
			if (!r->body || r->blen < 4 || r->blen > 200) { reply_err(r->fd, 400, "choose a password of at least 4 characters"); return; }
			unlink(RUN "/debian.pw");
			int f = open(RUN "/debian.pw", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
			if (f < 0 || write(f, r->body, r->blen) != (ssize_t)r->blen) { if (f >= 0) close(f); reply_err(r->fd, 500, "could not write the request"); return; }
			close(f);
			/* 1.3.0: passwd sets a new Linux password (the same file, the same checks) */
			if (!strcmp(what, "passwd")) start_job("debian-passwd", DEBIAN_BIN " passwd " RUN "/debian.pw");
			else start_job("debian-setup", DEBIAN_BIN " setup " RUN "/debian.pw");
		} else if (!strcmp(what, "update")) start_job("debian-update", DEBIAN_BIN " update");
		else start_job("debian-remove", DEBIAN_BIN " remove");
		reply_ok(r->fd, "started"); return;
	}
	if (!strcmp(what, "apps")) { reply_cmd_json(r, DEBIAN_BIN " apps 2>/dev/null", "could not list the Debian apps"); return; }
	if (!strcmp(what, "launch")) {
		char idc[220] = "", filec[1100] = ""; const char *q1 = qparam(r->query, "id"); if (q1) snprintf(idc, sizeof idc, "%s", q1);
		const char *q2 = qparam(r->query, "file"); if (q2) snprintf(filec, sizeof filec, "%s", q2);
		const char *id = idc, *file = filec;
		if (!*id || strlen(id) > 200 || strspn(id, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-+") != strlen(id)) { reply_err(r->fd, 400, "bad app"); return; }
		if (file && *file && (strncmp(file, "/home/rudolf/", 13) || strstr(file, "/../"))) { reply_err(r->fd, 400, "bad file"); return; }
		char a0[] = "rudolf-debian", a1[] = "launch"; char idb[220], fb[1100]; snprintf(idb, sizeof idb, "%s", id); snprintf(fb, sizeof fb, "%s", file ? file : "");
		char *argv[] = {a0, a1, idb, *fb ? fb : NULL, NULL};
		spawn_debian(argv); reply_ok(r->fd, "started"); return;
	}
	if (!strcmp(what, "icon")) {
		const char *pth = qparam(r->query, "path");
		if (!pth || strncmp(pth, "/persist/debian/usr/share/", 26) || strstr(pth, "..")) { reply_err(r->fd, 400, "bad icon"); return; }
		serve_file(r, pth); return;
	}
	if (!strcmp(what, "settings")) {
		const char *u = qparam(r->query, "url");
		if (!u || (*u && strncmp(u, "https://", 8) && strncmp(u, "http://127.0.0.1", 16))) { reply_err(r->fd, 400, "bad address"); return; }
		for (const char *q = u; *q; q++) if (!isalnum((unsigned char)*q) && !strchr("/:._-~%+", *q)) { reply_err(r->fd, 400, "bad address"); return; }
		char b[600]; snprintf(b, sizeof b, "url=%s\n", u);
		if (!*u) unlink(PERSIST "/os/debian.conf"); else if (write_str(PERSIST "/os/debian.conf", b)) { reply_err(r->fd, 409, "settings are kept only on an installed Rudolf OS"); return; }
		reply_ok(r->fd, "saved"); return;
	}
	reply_err(r->fd, 404, "unknown Debian action");
}
/* ------------------------------------------------------------- Blitzen (rudolf-ai + llama-server)
 * ai/status, ai/download (job), ai/load, ai/unload, ai/remove, ai/settings?url=; ai/chat: the request body
 * (an OpenAI-style chat request) goes to the engine with its key, and the engine's streamed answer comes back as
 * it is written; ai/run?lang=python|cpp: the body is a program, run in the Debian sandbox with no network and a
 * time limit; ai/install?what=python|cpp: install python3 or g++ into Debian (a job). */
#define AI_BIN "/usr/bin/rudolf-ai"
static void ai_touch(void) { int f = open(RUN "/ai/last", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600); if (f >= 0) close(f); }
static int ai_connect(void)
{
	int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); if (s < 0) return -1;
	struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(8099), .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
	if (connect(s, (struct sockaddr *)&a, sizeof a)) { close(s); return -1; }
	struct timeval tv = {120, 0}; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	return s;
}
static void api_ai(struct req *r, const char *what)
{
	if (access(AI_BIN, X_OK)) { reply_err(r->fd, 404, "this Rudolf OS has no Blitzen"); return; }
	if (!strcmp(what, "status")) {
		char *o = run_out(AI_BIN " status 2>/dev/null");
		str s = {0}; sadd(&s, "{\"ok\":true");
		for (char *sv, *l = strtok_r(o, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) {
			char *e = strchr(l, '='); if (!e) continue; *e = 0;
			int num = e[1] && strspn(e + 1, "0123456789") == strlen(e + 1);
			sadd(&s, ",\"%s\":", l); if (num) sadd(&s, "%s", e + 1); else sjson(&s, e + 1);
		}
		sadd(&s, ",\"debian\":%s,\"python\":%s,\"cpp\":%s", access("/persist/debian/.rudolf-ready", F_OK) ? "false" : "true",
		     access("/persist/debian/usr/bin/python3", F_OK) ? "false" : "true", access("/persist/debian/usr/bin/g++", F_OK) ? "false" : "true");
		sadd(&s, ",\"job\":"); char *jn = slurp(RUN "/job.name", NULL); if (jn) trim(jn);
		sjson(&s, job_running() && jn ? jn : ""); free(jn); sadd(&s, "}"); free(o); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "download") || !strcmp(what, "install")) {
		if (job_running()) { reply_err(r->fd, 409, "another task is still running"); return; }
		if (!strcmp(what, "download")) { start_job("blitzen-download", AI_BIN " download"); reply_ok(r->fd, "started"); return; }
		const char *w = qparam(r->query, "what");
		if (access("/persist/debian/.rudolf-ready", F_OK)) { reply_err(r->fd, 409, "set up Linux (Debian) first: Start > Terminal (Linux)"); return; }
		if (w && !strcmp(w, "python")) start_job("blitzen-python", "/usr/bin/rudolf-debian run --root -- sh -c 'apt-get update && apt-get install -y python3'");
		else if (w && !strcmp(w, "cpp")) start_job("blitzen-cpp", "/usr/bin/rudolf-debian run --root -- sh -c 'apt-get update && apt-get install -y g++'");
		else { reply_err(r->fd, 400, "install what?"); return; }
		reply_ok(r->fd, "started"); return;
	}
	if (!strcmp(what, "load") || !strcmp(what, "unload") || !strcmp(what, "remove")) {
		char cmd[100]; snprintf(cmd, sizeof cmd, AI_BIN " %s 2>&1", what);
		char *o = run_out(cmd); trim(o);
		if (!strcmp(o, "ready") || !strcmp(o, "stopped") || !strcmp(o, "removed")) reply_ok(r->fd, o);
		else { char *m = strstr(o, "rudolf-ai: "); reply_err(r->fd, 409, m ? m + 11 : (*o ? o : "Blitzen did not start")); }
		free(o); return;
	}
	if (!strcmp(what, "settings")) {
		const char *u = qparam(r->query, "url");
		if (!u || (*u && strncmp(u, "https://", 8) && strncmp(u, "http://127.0.0.1", 16))) { reply_err(r->fd, 400, "bad address"); return; }
		for (const char *q = u; *q; q++) if (!isalnum((unsigned char)*q) && !strchr("/:._-~%+", *q)) { reply_err(r->fd, 400, "bad address"); return; }
		char *old = slurp(PERSIST "/os/blitzen.conf", NULL); str c = {0};
		if (old) for (char *sv, *l = strtok_r(old, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) if (strncmp(l, "url=", 4)) sadd(&c, "%s\n", l);
		if (*u) sadd(&c, "url=%s\n", u);
		free(old);
		if (write_str(PERSIST "/os/blitzen.conf", c.b ? c.b : "")) { free(c.b); reply_err(r->fd, 409, "settings are kept only on an installed Rudolf OS"); return; }
		free(c.b); reply_ok(r->fd, "saved"); return;
	}
	if (!strcmp(what, "chat")) {
		if (!r->body || !r->blen || r->blen > 262144) { reply_err(r->fd, 400, "nothing to ask"); return; }
		char *o = run_out(AI_BIN " load 2>&1"); trim(o);
		if (strcmp(o, "ready")) { char *m = strstr(o, "rudolf-ai: "); reply_err(r->fd, 409, m ? m + 11 : (*o ? o : "Blitzen did not start")); free(o); return; }
		free(o);
		char *key = slurp(RUN "/ai/key", NULL); if (key) trim(key);
		int s = key ? ai_connect() : -1;
		if (s < 0) { free(key); reply_err(r->fd, 502, "Blitzen is not answering"); return; }
		ai_touch();
		char h[400]; snprintf(h, sizeof h, "POST /v1/chat/completions HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: Bearer %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", key, r->blen);
		memset(key, 0, strlen(key)); free(key);
		send_all(s, h, strlen(h)); send_all(s, r->body, r->blen);
		/* the engine's whole reply (status line, headers, chunked stream) goes to the page as it arrives;
		 * the page's connection ends with it */
		char buf[16384]; ssize_t n; time_t t0 = time(NULL);
		while ((n = read(s, buf, sizeof buf)) > 0) { send_all(r->fd, buf, n); if (time(NULL) - t0 > 30) { ai_touch(); t0 = time(NULL); } }
		close(s); ai_touch(); return;
	}
	if (!strcmp(what, "run")) {
		const char *lq = qparam(r->query, "lang"); char lang[16]; snprintf(lang, sizeof lang, "%s", lq ? lq : "");
		int py = !strcmp(lang, "python"), cpp = !strcmp(lang, "cpp");
		if (!py && !cpp) { reply_err(r->fd, 400, "Blitzen runs Python and C++ here (BlockLua and JavaScript run in the page)"); return; }
		if (!r->body || r->blen > 200000) { reply_err(r->fd, 400, "no program"); return; }
		if (access("/persist/debian/.rudolf-ready", F_OK)) { str j = {0}; sadd(&j, "{\"ok\":false,\"missing\":\"debian\",\"error\":\"Running %s needs Linux (Debian): Start > Terminal (Linux) sets it up\"}", py ? "Python" : "C++"); reply_json(r->fd, &j); return; }
		if (access(py ? "/persist/debian/usr/bin/python3" : "/persist/debian/usr/bin/g++", F_OK)) { str j = {0}; sadd(&j, "{\"ok\":false,\"missing\":\"%s\",\"error\":\"%s is not installed in Debian yet\"}", lang, py ? "Python" : "The C++ compiler (g++)"); reply_json(r->fd, &j); return; }
		/* a fresh folder in the home folder (shared with Debian), owned by the user */
		unsigned char rb[8]; char id[20]; int rf = open("/dev/urandom", O_RDONLY | O_CLOEXEC); if (rf < 0 || read(rf, rb, 8) != 8) { if (rf >= 0) close(rf); reply_err(r->fd, 500, "no randomness"); return; } close(rf);
		for (int i = 0; i < 8; i++) sprintf(id + 2 * i, "%02x", rb[i]);
		char dir[200], file[260]; mkdir("/home/rudolf/.cache", 0755); if (chown("/home/rudolf/.cache", 1000, 1000)) {}
		mkdir("/home/rudolf/.cache/blitzen", 0755); if (chown("/home/rudolf/.cache/blitzen", 1000, 1000)) {}
		snprintf(dir, sizeof dir, "/home/rudolf/.cache/blitzen/%s", id);
		if (mkdir(dir, 0755) || chown(dir, 1000, 1000)) { reply_err(r->fd, 500, "could not make a folder to run in"); return; }
		snprintf(file, sizeof file, "%s/%s", dir, py ? "main.py" : "main.cpp");
		int f = open(file, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
		if (f < 0 || (r->blen && write(f, r->body, r->blen) != (ssize_t)r->blen)) { if (f >= 0) close(f); reply_err(r->fd, 500, "could not write the program"); return; }
		close(f); if (chown(file, 1000, 1000)) {}
		char cmd[800];
		/* the exit status comes back through a file: BusyBox sh has no PIPESTATUS */
		char st[260]; snprintf(st, sizeof st, "%s/.status", dir);
		if (py) snprintf(cmd, sizeof cmd, "(/usr/bin/rudolf-debian run --nonet --cwd %s -- timeout -k 2 10 python3 -I main.py </dev/null 2>&1; echo $? > %s) | head -c 65536", dir, st);
		else snprintf(cmd, sizeof cmd, "(/usr/bin/rudolf-debian run --nonet --cwd %s -- sh -c 'timeout -k 2 30 g++ -std=c++17 -O1 -Wall -o prog main.cpp && timeout -k 2 10 ./prog' </dev/null 2>&1; echo $? > %s) | head -c 65536", dir, st);
		char *out = run_out(cmd); long long code = read_ll(st, -1);
		str j = {0}; sadd(&j, "{\"ok\":true,\"code\":%lld,\"timeout\":%s,\"out\":", code, code == 124 || code == 137 ? "true" : "false"); sjson(&j, out ? out : ""); sadd(&j, "}");
		free(out); char rm[260]; snprintf(rm, sizeof rm, "rm -rf %s", dir); run(rm);
		reply_json(r->fd, &j); return;
	}
	reply_err(r->fd, 404, "unknown Blitzen action");
}
/* runs "rpk run NAME ARGS..." as the desktop user with the request body as its input */
static void api_pkg_exec(struct req *r)
{
	char nm[48], args[2048];
	char *q = qparam(r->query, "name"); if (!q || !pkg_names_ok(q) || strchr(q, ',') || strlen(q) > 40) { reply_err(r->fd, 400, "bad package name"); return; }
	snprintf(nm, sizeof nm, "%s", q);
	q = qparam(r->query, "args"); snprintf(args, sizeof args, "%s", q ? q : "");
	char *argv[66]; int n = 0; argv[n++] = "/usr/bin/rpk"; argv[n++] = "run"; argv[n++] = nm;
	if (*args) for (char *t = args, *e; t && n < 64; t = e) { e = strchr(t, '\x1f'); if (e) *e++ = 0; argv[n++] = t; }
	argv[n] = NULL;
	int in[2], out[2], er[2];
	if (pipe(in) || pipe(out) || pipe(er)) { reply_err(r->fd, 500, "cannot start"); return; }
	signal(SIGCHLD, SIG_DFL);
	pid_t pid = fork();
	if (pid < 0) { reply_err(r->fd, 500, "cannot start"); return; }
	if (!pid) {
		setsid(); dup2(in[0], 0); dup2(out[1], 1); dup2(er[1], 2);
		for (int fd = 3; fd < 256; fd++) close(fd);
		if (chdir("/home/rudolf")) { if (chdir("/tmp")) {} }
		if (setgroups(0, NULL) || setgid(1000) || setuid(1000)) _exit(126);
		clearenv(); setenv("HOME", "/home/rudolf", 1); setenv("USER", "rudolf", 1); setenv("LANG", "C.UTF-8", 1); setenv("PATH", "/usr/bin:/bin", 1);
		execv(argv[0], argv); _exit(127);
	}
	close(in[0]); close(out[1]); close(er[1]);
	fcntl(in[1], F_SETFL, O_NONBLOCK); fcntl(out[0], F_SETFL, O_NONBLOCK); fcntl(er[0], F_SETFL, O_NONBLOCK);
	size_t sent = 0, on = 0, en = 0, ocap = 1 << 16, ecap = 1 << 14; unsigned char *ob = malloc(ocap), *eb = malloc(ecap);
	int fin = r->blen ? in[1] : (close(in[1]), -1), fo = out[0], fe = er[0], killed = 0;
	time_t until = time(NULL) + 30;
	while (fo >= 0 || fe >= 0) {
		struct pollfd pf[3]; int k = 0;
		if (fin >= 0) { pf[k].fd = fin; pf[k].events = POLLOUT; k++; }
		if (fo >= 0) { pf[k].fd = fo; pf[k].events = POLLIN; k++; }
		if (fe >= 0) { pf[k].fd = fe; pf[k].events = POLLIN; k++; }
		if (time(NULL) > until) { kill(-pid, SIGKILL); killed = 1; break; }
		if (poll(pf, k, 1000) < 0 && errno != EINTR) break;
		for (int i = 0; i < k; i++) {
			if (!pf[i].revents) continue;
			if (pf[i].fd == fin) { ssize_t w = write(fin, r->body + sent, r->blen - sent); if (w > 0) sent += w; if (w < 0 && errno != EAGAIN) sent = r->blen; if (sent >= r->blen) { close(fin); fin = -1; } }
			else if (pf[i].fd == fo) {
				if (on + 65536 > ocap && ocap < (8u << 20)) { ocap *= 2; ob = realloc(ob, ocap); }
				ssize_t g = on + 65536 <= ocap ? read(fo, ob + on, 65536) : -2;
				if (g > 0) on += g; else if (g == -2) { kill(-pid, SIGKILL); killed = 2; close(fo); fo = -1; } else if (g == 0 || errno != EAGAIN) { close(fo); fo = -1; }
			} else {
				char tmp[4096]; ssize_t g = read(fe, tmp, sizeof tmp);
				if (g > 0) { size_t c = en + g < ecap - 1 ? (size_t)g : ecap - 1 - en; memcpy(eb + en, tmp, c); en += c; } else if (g == 0 || errno != EAGAIN) { close(fe); fe = -1; }
			}
		}
	}
	if (fin >= 0) close(fin); if (fo >= 0) close(fo); if (fe >= 0) close(fe);
	int st = 0; waitpid(pid, &st, 0);
	str s = {0}; eb[en] = 0;
	sadd(&s, "{\"ok\":true,\"code\":%d,\"stopped\":", WIFEXITED(st) ? WEXITSTATUS(st) : -1);
	sjson(&s, killed == 1 ? "it ran for more than 30 seconds" : killed == 2 ? "its output passed 8 MB" : "");
	sadd(&s, ",\"err\":"); sjson(&s, (char *)eb); sadd(&s, ",\"out64\":\""); b64(&s, ob, on); sadd(&s, "\"}");
	free(ob); free(eb);
	reply_json(r->fd, &s);
}
static void api_pkg(struct req *r, const char *what)
{
	if (access("/usr/bin/rpk", X_OK)) { reply_err(r->fd, 404, "this Rudolf OS has no package manager"); return; }
	char cmd[1200], *q;
	if (!strcmp(what, "exec")) { api_pkg_exec(r); return; }
	if (!strcmp(what, "run")) {
		q = qparam(r->query, "name"); if (!q || !pkg_names_ok(q) || strchr(q, ',')) { reply_err(r->fd, 400, "bad package name"); return; }
		if (native_pid()) { write_str(RUN "/wm", "game"); reply_err(r->fd, 409, "another full-screen app is running"); return; }
		snprintf(cmd, sizeof cmd, "setsid su -s /bin/sh rudolf -c 'set -a; . " RUN "/session.env; set +a; exec /usr/bin/rpk run %s' "
			"</dev/null >>" RUN "/native.log 2>&1 & echo $! > " RUN "/native.pid", q);
		run(cmd); write_str(RUN "/wm", "game"); reply_ok(r->fd, "started"); return;
	}
	if (!strcmp(what, "source") || !strcmp(what, "key")) {
		const char *op = qparam(r->query, "op"); char o[16]; snprintf(o, sizeof o, "%s", op ? op : "");
		q = qparam(r->query, "v");
		if (!q || !*q || strlen(q) > 400) { reply_err(r->fd, 400, "missing address"); return; }
		for (const char *c = q; *c; c++) if (!isalnum((unsigned char)*c) && !strchr(!strcmp(what, "key") ? "+/=" : "/:._-~%+", *c)) { reply_err(r->fd, 400, "that has characters rpk does not accept"); return; }
		if (!strcmp(what, "source") && strcmp(o, "add") && strcmp(o, "remove")) { reply_err(r->fd, 400, "op is add or remove"); return; }
		snprintf(cmd, sizeof cmd, "/usr/bin/rpk %s %s '%s' 2>&1", what, !strcmp(what, "key") ? "add" : o, q);
		char *out = run_out(cmd); reply_ok(r->fd, out); free(out); return;
	}
	if (job_running()) { reply_err(r->fd, 409, "another task is still running"); return; }
	if (!strcmp(what, "refresh") || !strcmp(what, "update")) { start_job("packages", !strcmp(what, "refresh") ? "/usr/bin/rpk refresh" : "/usr/bin/rpk update"); reply_ok(r->fd, "started"); return; }
	if (!strcmp(what, "install") || !strcmp(what, "remove")) {
		q = qparam(r->query, "name"); if (!q || !pkg_names_ok(q)) { reply_err(r->fd, 400, "bad package name"); return; }
		char names[420]; snprintf(names, sizeof names, "%s", q); for (char *c = names; *c; c++) if (*c == ',') *c = ' ';
		snprintf(cmd, sizeof cmd, "/usr/bin/rpk %s %s", what, names); start_job("packages", cmd); reply_ok(r->fd, "started"); return;
	}
	if (!strcmp(what, "installfile")) {
		if (r->blen < 4096) { reply_err(r->fd, 400, "that is not a package file"); return; }
		const char *dir = access(RUN "/persistent", F_OK) ? RUN "/pkg/tmp" : PERSIST "/pkg/tmp";
		snprintf(cmd, sizeof cmd, "mkdir -p %s", dir); run(cmd);
		char path[300]; snprintf(path, sizeof path, "%s/upload.rpk", dir);
		int f = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
		if (f < 0 || write(f, r->body, r->blen) != (ssize_t)r->blen) { if (f >= 0) close(f); reply_err(r->fd, 500, "could not save the file (is the disk full?)"); return; }
		close(f);
		snprintf(cmd, sizeof cmd, "/usr/bin/rpk install-file %s; s=$?; rm -f %s; exit $s", path, path); start_job("packages", cmd); reply_ok(r->fd, "started"); return;
	}
	reply_err(r->fd, 404, "unknown package action");
}

/* ------------------------------------------------------------- Task Manager (1.2) */
/* the kernel, the hardware and the services, for Task Manager's System and Hardware tabs and system reports.
   Lists that are many lines (drivers, kernel messages, devices) are plain text, one item a line. Nothing that
   identifies the person or their network (no addresses, network names or serial numbers). */
static long long meminfo_kb(const char *key)
{
	FILE *f = fopen("/proc/meminfo", "r"); char line[256]; long long v = -1; size_t n = strlen(key);
	while (f && fgets(line, sizeof line, f)) if (!strncmp(line, key, n) && line[n] == ':') { v = atoll(line + n + 1) * 1024; break; }
	if (f) fclose(f);
	return v;
}
static void jtext(str *s, const char *key, char *v) { sadd(s, ",\"%s\":", key); if (v) trim(v); sjson(s, v ? v : ""); free(v); }
static int running(const char *pat) { char c[256]; snprintf(c, sizeof c, "pgrep -f '%s' >/dev/null 2>&1", pat); return !run(c); }
static void api_system(struct req *r)
{
	str s = {0};
	struct utsname u; uname(&u);
	struct sysinfo si; sysinfo(&si);
	sadd(&s, "{\"ok\":true,\"time\":%lld,\"kernel\":{\"release\":", (long long)time(NULL)); sjson(&s, u.release);
	sadd(&s, ",\"version\":"); sjson(&s, u.version);
	sadd(&s, ",\"machine\":"); sjson(&s, u.machine);
	sadd(&s, ",\"uptime\":%ld,\"load\":[%.2f,%.2f,%.2f],\"procs\":%d", si.uptime, si.loads[0] / 65536.0, si.loads[1] / 65536.0, si.loads[2] / 65536.0, si.procs);
	jtext(&s, "cmdline", (access("/etc/rudolf/cmdline", F_OK) ? slurp("/proc/cmdline", NULL) : slurp("/etc/rudolf/cmdline", NULL)));
	sadd(&s, ",\"boot\":"); sjson(&s, boot_mode); sadd(&s, ",\"media\":"); sjson(&s, media_mode);
	sadd(&s, ",\"live\":%s,\"recovery\":%s", live ? "true" : "false", recovery ? "true" : "false");
	jtext(&s, "modules", run_out("awk '{print $1}' /proc/modules 2>/dev/null | sort"));
	/* the kernel's own warnings and errors (levels 0-4), newest last */
	jtext(&s, "messages", run_out("dmesg -r 2>/dev/null | grep '^<[0-4]>' | sed 's/^<[0-9]>//' | tail -60"));
	sadd(&s, "},\"memory\":{\"total\":%lld,\"available\":%lld,\"cached\":%lld,\"buffers\":%lld,\"swapTotal\":%lld,\"swapFree\":%lld",
		meminfo_kb("MemTotal"), meminfo_kb("MemAvailable"), meminfo_kb("Cached"), meminfo_kb("Buffers"), meminfo_kb("SwapTotal"), meminfo_kb("SwapFree"));
	/* compressed swap in memory (zram): what was stored, what it takes */
	{ unsigned long long o = 0, c = 0, m = 0; FILE *f = fopen("/sys/block/zram0/mm_stat", "r"); if (f) { if (fscanf(f, "%llu %llu %llu", &o, &c, &m) != 3) o = c = m = 0; fclose(f); }
	  sadd(&s, ",\"zramStored\":%llu,\"zramUsed\":%llu}", o, m); }
	sadd(&s, ",\"cpu\":{\"model\":");
	{ char *m = run_out("grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ *//'"); trim(m); sjson(&s, m); free(m); }
	{ char *f = run_out("awk -F: '/cpu MHz/{s+=$2;n++}END{if(n)printf \"%.0f\",s/n}' /proc/cpuinfo"); trim(f); sadd(&s, ",\"threads\":%ld,\"mhz\":%s", sysconf(_SC_NPROCESSORS_ONLN), *f ? f : "null"); free(f); }
	{ char *t = run_out("cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | sort -n | tail -1"); trim(t); if (*t) sadd(&s, ",\"tempC\":%.1f}", atof(t) / 1000.0); else sadd(&s, ",\"tempC\":null}"); free(t); }
	sadd(&s, ",\"battery\":"); battery(&s);
	jtext(&s, "machine", run_out("echo \"$(cat /sys/class/dmi/id/sys_vendor 2>/dev/null) $(cat /sys/class/dmi/id/product_name 2>/dev/null)\""));
	jtext(&s, "firmware", run_out("cat /sys/class/dmi/id/bios_vendor /sys/class/dmi/id/bios_version 2>/dev/null | tr '\\n' ' '"));
	/* graphics: card, PCI vendor:device, driver; then how the screen is driven (graphics card or software display) */
	jtext(&s, "gpu", run_out("for d in /sys/class/drm/card[0-9]; do [ -e $d/device/vendor ] || continue; dr=$(readlink $d/device/driver 2>/dev/null); echo \"${d##*/} $(cat $d/device/vendor):$(cat $d/device/device) ${dr##*/}\"; done"));
	/* what draws (1.2.4): the renderer glamor uses on the graphics card, and NVIDIA's GSP firmware (nouveau) */
	jtext(&s, "renderer", run_out("sed -n 's/.*glamor X acceleration enabled on \\(.*\\)$/\\1/p' " RUN "/Xorg.log 2>/dev/null | head -1"));
	jtext(&s, "gsp", run_out("grep -q '^nouveau ' /proc/modules && { dmesg 2>/dev/null | grep -i 'nouveau' | grep -iE 'gsp|firmware' | sed 's/^\\[[^]]*\\] *//' | tail -2; } || true"));
	jtext(&s, "display", run_out("echo \"$(cat " RUN "/display-mode 2>/dev/null) | $(cat " RUN "/screen-actual 2>/dev/null) | $(cat " RUN "/display-reason 2>/dev/null)\""));
	jtext(&s, "audio", slurp("/proc/asound/cards", NULL));
	jtext(&s, "input", run_out("sed -n 's/^N: Name=\"\\(.*\\)\"$/\\1/p' /proc/bus/input/devices 2>/dev/null"));
	/* network adapters: name, state, speed (Mb/s), bytes received, bytes sent, kind, driver (1.2.4) */
	jtext(&s, "net", run_out("for d in /sys/class/net/*; do n=${d##*/}; [ $n = lo ] && continue; dr=$(readlink $d/device/driver 2>/dev/null); echo \"$n $(cat $d/operstate 2>/dev/null) $(cat $d/speed 2>/dev/null || echo -) $(cat $d/statistics/rx_bytes) $(cat $d/statistics/tx_bytes) $([ -d $d/wireless ] && echo wifi || echo wired) ${dr##*/}\"; done"));
	/* storage: device, size, used, where (bytes) */
	jtext(&s, "storage", run_out("df -kP 2>/dev/null | awk 'NR>1 && $1 ~ /^\\/dev\\//{print $1, $2*1024, $3*1024, $6}'"));
	{
		char *url = slurp(RUN "/desktop-url", NULL);
		int shell = url && strstr(url, "isolated-app://") != NULL; free(url);
		sadd(&s, ",\"services\":{\"rudolfd\":true,\"display\":%s,\"sound\":%s,\"wifi\":%s,\"shell\":%s,\"shellFailed\":%s,\"webWindow\":%s,\"linuxApps\":%s,\"x\":%s}",
			running("[r]udolf-display") ? "true" : "false", running("[p]ulseaudio") ? "true" : "false", running("[w]pa_supplicant") ? "true" : "false",
			shell && running("[r]udolf-launch") ? "true" : "false", access(RUN "/shell-failed", F_OK) ? "false" : "true",
			running("[c]lass=RudolfWeb") ? "true" : "false", running("[r]udolf-debian") ? "true" : "false", running("X[v]fb|X[o]rg") ? "true" : "false");
	}
	jtext(&s, "session", run_out("tail -25 " RUN "/session.log 2>/dev/null"));
	sadd(&s, "}");
	reply_json(r->fd, &s);
}

/* every process: pid, parent, owner, name, state, memory (RSS), CPU share since the last look, what it is (tag),
   whether it may be ended (protected), and its command line (shortened). Tags: kernel, desktop (the desktop's
   Chromium and rudolf-launch), web (Chromium pages: the desktop and Oracle's tabs), gpu, network, audio, chromium
   (Chromium's other helpers), webwindow (the browser window for sign-in sites), app (the person's programs), system. */
#define MAXP 4096
struct pinfo { int pid, ppid, uid; char name[32], state; long long rss; unsigned long long ticks; char cmd[400]; const char *tag; int prot; };
static int read_proc(int pid, struct pinfo *p)
{
	char path[64], buf[1024];
	snprintf(path, sizeof path, "/proc/%d/stat", pid);
	int f = open(path, O_RDONLY); if (f < 0) return 0;
	ssize_t n = read(f, buf, sizeof buf - 1); close(f); if (n <= 0) return 0; buf[n] = 0;
	char *a = strchr(buf, '('), *b = strrchr(buf, ')'); if (!a || !b) return 0;
	memset(p, 0, sizeof *p); p->pid = pid;
	size_t nl = b - a - 1; if (nl >= sizeof p->name) nl = sizeof p->name - 1; memcpy(p->name, a + 1, nl);
	unsigned long long ut = 0, st = 0; int ppid = 0; char state = '?';
	if (sscanf(b + 2, "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu", &state, &ppid, &ut, &st) < 4) return 0;
	p->state = state; p->ppid = ppid; p->ticks = ut + st;
	snprintf(path, sizeof path, "/proc/%d/statm", pid);
	FILE *m = fopen(path, "r"); long long sz = 0, res = 0; if (m) { if (fscanf(m, "%lld %lld", &sz, &res) != 2) res = 0; fclose(m); }
	p->rss = res * sysconf(_SC_PAGESIZE);
	snprintf(path, sizeof path, "/proc/%d/status", pid);
	FILE *u = fopen(path, "r"); p->uid = -1; if (u) { char l[256]; while (fgets(l, sizeof l, u)) if (!strncmp(l, "Uid:", 4)) { p->uid = atoi(l + 4); break; } fclose(u); }
	snprintf(path, sizeof path, "/proc/%d/cmdline", pid);
	f = open(path, O_RDONLY);
	if (f >= 0) { n = read(f, p->cmd, sizeof p->cmd - 1); close(f); if (n > 0) { for (ssize_t i = 0; i < n; i++) if (!p->cmd[i]) p->cmd[i] = ' '; p->cmd[n] = 0; trim(p->cmd); } }
	return 1;
}
static int nprocs(struct pinfo *ps)
{
	DIR *d = opendir("/proc"); struct dirent *e; int n = 0;
	while (d && (e = readdir(d)) && n < MAXP) { int pid = atoi(e->d_name); if (pid > 0 && read_proc(pid, &ps[n])) n++; }
	if (d) closedir(d);
	return n;
}
static struct pinfo *pfind(struct pinfo *ps, int n, int pid) { for (int i = 0; i < n; i++) if (ps[i].pid == pid) return &ps[i]; return NULL; }
static void tag_procs(struct pinfo *ps, int n)
{
	int me = getppid();   /* rudolfd (each request is a child of it) */
	for (int i = 0; i < n; i++) {
		struct pinfo *p = &ps[i]; const char *c = p->cmd;
		int chrome = strstr(c, "/chrome") != NULL || !strcmp(p->name, "chrome");
		if (!*c || p->pid == 2 || p->ppid == 2) { p->tag = "kernel"; p->prot = 1; continue; }
		if (p->pid == 1 || p->pid == me || p->pid == getpid() || !strcmp(p->name, "rudolfd") || !strcmp(p->name, "rudolf-display") || !strcmp(p->name, "Xorg") || !strcmp(p->name, "Xvfb") || strstr(c, "/usr/lib/rudolf/session")) { p->tag = "system"; p->prot = 1; continue; }
		if (!strcmp(p->name, "rudolf-launch")) { p->tag = "desktop"; p->prot = 1; continue; }
		if (chrome && strstr(c, "--class=RudolfWeb")) { p->tag = "webwindow"; continue; }
		if (chrome && strstr(c, "--rudolf-kiosk") && !strstr(c, "--type=")) { p->tag = "desktop"; p->prot = 1; continue; }
		if (chrome) {
			if (strstr(c, "--type=renderer")) p->tag = strstr(c, "--extension-process") ? "chromium" : "web";
			else if (strstr(c, "--type=gpu-process")) p->tag = "gpu";
			else if (strstr(c, "NetworkService")) p->tag = "network";
			else if (strstr(c, "AudioService")) p->tag = "audio";
			else p->tag = "chromium";
			continue;
		}
		p->tag = p->uid == 1000 ? "app" : "system";
	}
	/* Chromium's helpers belong to the browser window for sign-in sites when that is their ancestor */
	for (int i = 0; i < n; i++) {
		struct pinfo *p = &ps[i];
		if (strcmp(p->tag, "web") && strcmp(p->tag, "gpu") && strcmp(p->tag, "network") && strcmp(p->tag, "audio") && strcmp(p->tag, "chromium")) continue;
		struct pinfo *q = p;
		for (int k = 0; k < 8 && q; k++) { q = pfind(ps, n, q->ppid); if (q && q->tag && !strcmp(q->tag, "webwindow") ) { p->tag = "webwindow"; break; } }
	}
}
static void api_processes(struct req *r)
{
	struct pinfo *ps = calloc(MAXP, sizeof *ps); if (!ps) { reply_err(r->fd, 500, "out of memory"); return; }
	int n = nprocs(ps); tag_procs(ps, n);
	unsigned long long busy, total; cpu_times(&busy, &total);
	/* the CPU share since the previous look (kept in a file: each request runs in its own process) */
	unsigned long long ptotal = 0; char *prev = slurp(RUN "/procs.prev", NULL);
	if (prev) ptotal = strtoull(prev, NULL, 10);
	str nx = {0}; sadd(&nx, "%llu\n", total);
	str s = {0}; sadd(&s, "{\"ok\":true,\"threads\":%ld,\"items\":[", sysconf(_SC_NPROCESSORS_ONLN));
	for (int i = 0; i < n; i++) {
		struct pinfo *p = &ps[i];
		double cpu = -1;
		if (prev && total > ptotal) {
			char key[32]; snprintf(key, sizeof key, "\n%d ", p->pid);
			char *at = strstr(prev, key);
			if (at) { unsigned long long t0 = strtoull(at + strlen(key), NULL, 10); if (p->ticks >= t0) cpu = 100.0 * (p->ticks - t0) / (total - ptotal); }
		}
		sadd(&nx, "%d %llu\n", p->pid, p->ticks);
		const char *user = p->uid == 0 ? "root" : p->uid == 1000 ? "rudolf" : "";
		sadd(&s, "%s{\"pid\":%d,\"ppid\":%d,\"user\":", i ? "," : "", p->pid, p->ppid);
		if (*user) sjson(&s, user); else sadd(&s, "\"%d\"", p->uid);
		sadd(&s, ",\"name\":"); sjson(&s, p->name);
		sadd(&s, ",\"state\":\"%c\",\"rss\":%lld,\"cpu\":", p->state, p->rss);
		if (cpu >= 0) sadd(&s, "%.1f", cpu); else sadd(&s, "null");
		sadd(&s, ",\"tag\":\"%s\",\"prot\":%s,\"cmd\":", p->tag, p->prot ? "true" : "false");
		/* Chromium's command lines are long and the same: its kind is enough */
		char cmd[200]; const char *t = strstr(p->cmd, "--type=");
		if (strstr(p->cmd, "/chrome")) { snprintf(cmd, sizeof cmd, "chrome %.*s", t ? (int)strcspn(t, " ") : 0, t ? t : ""); }
		else snprintf(cmd, sizeof cmd, "%s", p->cmd);
		sjson(&s, cmd); sadd(&s, "}");
	}
	sadd(&s, "]");
	/* the memory of each page (the desktop and Oracle's tabs), measured by rudolf-launch while Task Manager looks */
	write_str("/run/user/1000/rudolf-tabmem.want", "1");
	char *pg = slurp("/run/user/1000/rudolf-tabmem.json", NULL);
	if (pg && pg[0] == '{') { trim(pg); sadd(&s, ",\"pages\":%s", pg); }
	free(pg);
	sadd(&s, "}");
	write_str(RUN "/procs.prev", nx.b ? nx.b : ""); free(nx.b); free(prev); free(ps);
	reply_json(r->fd, &s);
}
/* ending a process (Task Manager). The desktop asks for an administrator's password first; the processes that keep
   Rudolf OS running are refused here whatever the caller says. */
static void api_proc_end(struct req *r)
{
	char *a = qparam(r->query, "pid"); int pid = a ? atoi(a) : 0;
	char *f = qparam(r->query, "force"); int force = f && *f == '1';
	if (pid <= 1) { reply_err(r->fd, 400, "which process?"); return; }
	struct pinfo *ps = calloc(MAXP, sizeof *ps); if (!ps) { reply_err(r->fd, 500, "out of memory"); return; }
	int n = nprocs(ps); tag_procs(ps, n);
	struct pinfo *p = pfind(ps, n, pid);
	if (!p) { free(ps); reply_err(r->fd, 404, "that process has already ended"); return; }
	if (p->prot) { free(ps); reply_err(r->fd, 403, "Rudolf OS needs this process to keep running, so it cannot be ended"); return; }
	char name[32]; snprintf(name, sizeof name, "%s", p->name); free(ps);
	if (kill(pid, force ? SIGKILL : SIGTERM)) { reply_err(r->fd, 500, strerror(errno)); return; }
	str s = {0}; sadd(&s, "{\"ok\":true,\"message\":"); sjson(&s, name); sadd(&s, "}"); reply_json(r->fd, &s);
}
/* restarting a service: sound (PulseAudio), wifi (reconnect), display (rudolf-display; the session starts it again) */
static void api_service(struct req *r)
{
	char *n = qparam(r->query, "name");
	if (!n) { reply_err(r->fd, 400, "which service?"); return; }
	int rc = -1;
	if (!strcmp(n, "sound")) rc = run("/usr/lib/rudolf/sound restart >>" RUN "/sound.log 2>&1");
	else if (!strcmp(n, "wifi")) rc = run("/usr/lib/rudolf/wifi.sh reconnect >/dev/null 2>&1");
	else if (!strcmp(n, "display")) rc = run("pkill -x rudolf-display >/dev/null 2>&1; true");
	else { reply_err(r->fd, 404, "unknown service"); return; }
	if (rc) reply_err(r->fd, 500, "it did not start again"); else reply_ok(r->fd, n);
}

/* ------------------------------------------------------------- downloads (1.2.1) */
/* Chromium saves every website download in INCOMING (its download folder, set by policy); when one is finished
   the desktop checks it (file type, antivirus, size) and asks here to keep it in Downloads, hold it in QUARANTINE,
   or delete it. Files kept in Downloads are opened through a link (/dlf/<id>) that needs no token, so a video
   or a PDF can be read in pieces by the page. */
#define DLHOME "/home/rudolf/Downloads"
#define INCOMING DLHOME "/.incoming"
#define QUARANTINE DLHOME "/.quarantine"
static int dl_name_ok(const char *n) { return n && *n && *n != '.' && !strchr(n, '/') && strlen(n) < 240; }
/* a free name in dir: "name", "name (2).ext", ... */
static void dl_unique(const char *dir, const char *name, char *out, size_t n)
{
	snprintf(out, n, "%s/%s", dir, name);
	if (access(out, F_OK)) return;
	const char *dot = strrchr(name, '.'); size_t stem = dot && dot != name ? (size_t)(dot - name) : strlen(name);
	for (int i = 2; i < 10000; i++) { snprintf(out, n, "%s/%.*s (%d)%s", dir, (int)stem, name, i, dot && dot != name ? dot : ""); if (access(out, F_OK)) return; }
}
static void api_dl(struct req *r, const char *what, int post)
{
	if (!strcmp(what, "incoming") || !strcmp(what, "quarantine")) {
		const char *dir = !strcmp(what, "incoming") ? INCOMING : QUARANTINE;
		DIR *d = opendir(dir); str s = {0}; sadd(&s, "{\"ok\":true,\"items\":["); int first = 1;
		struct dirent *e;
		while (d && (e = readdir(d))) {
			if (e->d_name[0] == '.') continue;
			char fp[600]; struct stat st; snprintf(fp, sizeof fp, "%s/%s", dir, e->d_name);
			if (lstat(fp, &st) || !S_ISREG(st.st_mode)) continue;
			size_t l = strlen(e->d_name); int part = l > 11 && !strcmp(e->d_name + l - 11, ".crdownload");
			sadd(&s, "%s{\"name\":", first ? "" : ","); sjson(&s, e->d_name);
			sadd(&s, ",\"size\":%lld,\"mtime\":%lld,\"partial\":%s}", (long long)st.st_size, (long long)st.st_mtim.tv_sec * 1000 + st.st_mtim.tv_nsec / 1000000, part ? "true" : "false");
			first = 0;
		}
		if (d) closedir(d);
		sadd(&s, "]}"); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "space")) {
		struct statvfs v; long long fr = -1, tot = -1;
		if (!statvfs("/home/rudolf", &v)) { fr = (long long)v.f_bavail * v.f_frsize; tot = (long long)v.f_blocks * v.f_frsize; }
		str s = {0}; sadd(&s, "{\"ok\":true,\"free\":%lld,\"total\":%lld,\"live\":%s}", fr, tot, live ? "true" : "false"); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "link")) {
		/* a link to a file in Downloads that the page can use in <video>, <img> or the PDF viewer */
		char *p = qparam(r->query, "path");
		if (!p || strncmp(p, DLHOME "/", sizeof DLHOME) || strstr(p, "/.") || strstr(p, "..")) { reply_err(r->fd, 403, "only files in Downloads"); return; }
		struct stat st; if (stat(p, &st) || !S_ISREG(st.st_mode)) { reply_err(r->fd, 404, "the file is not there any more"); return; }
		unsigned char rb[16]; char id[33]; int rf = open("/dev/urandom", O_RDONLY);
		if (rf < 0 || read(rf, rb, 16) != 16) { if (rf >= 0) close(rf); reply_err(r->fd, 500, "no randomness"); return; }
		close(rf); for (int i = 0; i < 16; i++) sprintf(id + i * 2, "%02x", rb[i]);
		mkdir(RUN "/dlf", 0700);
		char lf[128]; snprintf(lf, sizeof lf, RUN "/dlf/%s", id); write_str(lf, p);
		str s = {0}; sadd(&s, "{\"ok\":true,\"url\":\"/dlf/%s\"}", id); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "read")) {
		/* bytes of a downloaded file for the antivirus: from=incoming|quarantine|home, off (negative = from the end), len <= 8 MB */
		char *name = qparam(r->query, "name"), *from = qparam(r->query, "from"), *os = qparam(r->query, "off"), *ls = qparam(r->query, "len");
		if (!dl_name_ok(name)) { reply_err(r->fd, 400, "which file?"); return; }
		const char *dir = from && !strcmp(from, "quarantine") ? QUARANTINE : from && !strcmp(from, "home") ? DLHOME : INCOMING;
		char fp[600]; snprintf(fp, sizeof fp, "%s/%s", dir, name);
		int f = open(fp, O_RDONLY | O_NOFOLLOW); struct stat st;
		if (f < 0 || fstat(f, &st) || !S_ISREG(st.st_mode)) { if (f >= 0) close(f); reply_err(r->fd, 404, "the file is not there any more"); return; }
		long long off = os ? atoll(os) : 0, len = ls ? atoll(ls) : 8 << 20;
		if (len < 0 || len > 8 << 20) len = 8 << 20;
		if (off < 0) off = st.st_size + off < 0 ? 0 : st.st_size + off;
		if (off > st.st_size) off = st.st_size;
		if (len > st.st_size - off) len = st.st_size - off;
		char *b = malloc(len + 1); ssize_t got = 0;
		if (b && lseek(f, off, SEEK_SET) == off) while (got < len) { ssize_t k = read(f, b + got, len - got); if (k <= 0) break; got += k; }
		close(f);
		if (!b) { reply_err(r->fd, 500, "no memory"); return; }
		reply(r->fd, 200, "application/octet-stream", b, got, NULL); free(b); return;
	}
	if (!post) { reply_err(r->fd, 400, "use POST"); return; }
	if (!strcmp(what, "file")) {
		/* to=keep (incoming -> Downloads), quar (incoming -> quarantine), release (quarantine -> Downloads),
		   del (from=incoming|quarantine|home), rename (from=home, as=new name), quar from home; as = another name for keep/release */
		char *name = qparam(r->query, "name"), *to = qparam(r->query, "to"), *from = qparam(r->query, "from"), *as = qparam(r->query, "as");
		if (!dl_name_ok(name) || !to) { reply_err(r->fd, 400, "which file?"); return; }
		const char *src_dir = !strcmp(to, "release") || (from && !strcmp(from, "quarantine")) ? QUARANTINE : from && !strcmp(from, "home") ? DLHOME : INCOMING;
		if (!strcmp(src_dir, DLHOME) && strcmp(to, "del") && strcmp(to, "rename") && strcmp(to, "quar")) { reply_err(r->fd, 400, "to?"); return; }
		char src[600]; snprintf(src, sizeof src, "%s/%s", src_dir, name);
		struct stat st; if (lstat(src, &st) || !S_ISREG(st.st_mode)) { reply_err(r->fd, 404, "the file is not there any more"); return; }
		char dst[700] = "";
		if (!strcmp(to, "del")) { if (unlink(src)) { reply_err(r->fd, 500, strerror(errno)); return; } reply_ok(r->fd, "deleted"); return; }
		if (!strcmp(to, "rename")) {
			if (strcmp(src_dir, DLHOME) || !dl_name_ok(as)) { reply_err(r->fd, 400, "which name?"); return; }
			snprintf(dst, sizeof dst, "%s/%s", DLHOME, as);
			if (!access(dst, F_OK)) { reply_err(r->fd, 409, "a file with that name is already there"); return; }
		}
		else if (!strcmp(to, "keep") || !strcmp(to, "release")) dl_unique(DLHOME, dl_name_ok(as) ? as : name, dst, sizeof dst);
		else if (!strcmp(to, "quar")) { mkdir(QUARANTINE, 0700); if (chown(QUARANTINE, 1000, 1000)) {} dl_unique(QUARANTINE, name, dst, sizeof dst); }
		else { reply_err(r->fd, 400, "to?"); return; }
		if (rename(src, dst)) { reply_err(r->fd, 500, strerror(errno)); return; }
		str s = {0}; sadd(&s, "{\"ok\":true,\"path\":"); sjson(&s, dst); sadd(&s, ",\"name\":"); sjson(&s, strrchr(dst, '/') + 1); sadd(&s, ",\"size\":%lld}", (long long)st.st_size);
		reply_json(r->fd, &s); return;
	}
	reply_err(r->fd, 404, "unknown download action");
}

/* ------------------------------------------------------------- Secure Boot (1.2.2) */
/* The state of Secure Boot, and "Prepare Secure Boot": a request to shim's MokManager to enroll the Rudolf OS
   certificate at the next start (what mokutil --import does, with the simple password hash): MokNew holds the
   certificate as an EFI signature list, MokAuth the SHA-256 of that list followed by the one-time password in
   UCS-2. MokManager asks for the password before it enrolls anything. */
#define EFIV "/sys/firmware/efi/efivars"
#define SHIMG "605dab50-e046-4300-abb6-3dd810dd8b23"
#define SBCERT "/usr/share/rudolf/rudolf-sb.crt"
static unsigned char *sb_cert(size_t *n)
{
	char *p = slurp(SBCERT, NULL); if (!p) return NULL;
	char *b = strstr(p, "-----BEGIN CERTIFICATE-----"), *e = b ? strstr(b, "-----END CERTIFICATE-----") : NULL;
	if (!b || !e) { free(p); return NULL; }
	b += 27; *e = 0;
	static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	unsigned char *o = malloc(strlen(b)); size_t k = 0; unsigned v = 0; int bits = 0;
	for (; *b; b++) { const char *q = strchr(T, *b); if (!q || !*b) continue; v = (v << 6) | (q - T); bits += 6; if (bits >= 8) { bits -= 8; o[k++] = v >> bits; } }
	free(p); *n = k; return o;
}
static unsigned char *efivar(const char *name, size_t *n)
{
	char f[200]; snprintf(f, sizeof f, EFIV "/%s", name); size_t l = 0; char *d = slurp(f, &l);
	if (!d || l < 4) { free(d); return NULL; }
	memmove(d, d + 4, l - 4); *n = l - 4; return (unsigned char *)d;
}
static int sb_enrolled(void)
{
	size_t cn, ln; unsigned char *c = sb_cert(&cn), *l = efivar("MokListRT-" SHIMG, &ln); int r = 0;
	if (c && l && cn > 100) r = memmem(l, ln, c, cn) != NULL;
	free(c); free(l); return r;
}
static void unimmutable(const char *path)
{
	int f = open(path, O_RDONLY); if (f < 0) return;
	int fl = 0; if (!ioctl(f, FS_IOC_GETFLAGS, &fl)) { fl &= ~FS_IMMUTABLE_FL; ioctl(f, FS_IOC_SETFLAGS, &fl); }
	close(f);
}
static int efivar_write(const char *name, const unsigned char *d, size_t n)
{
	char f[200]; snprintf(f, sizeof f, EFIV "/%s", name);
	if (!access(f, F_OK)) { unimmutable(f); unlink(f); }
	unsigned char *b = malloc(n + 4); if (!b) return -1;
	uint32_t at = 7; memcpy(b, &at, 4); memcpy(b + 4, d, n);   /* non-volatile, boot and run time */
	int o = open(f, O_WRONLY | O_CREAT | O_TRUNC, 0644); int ok = o >= 0 && write(o, b, n + 4) == (ssize_t)(n + 4);
	if (o >= 0) close(o);
	free(b); return ok ? 0 : -1;
}
/* ------------------------------------------------------------- boot menu (1.2.5)
 * The UEFI boot menu (systemd-boot) keeps its choices in EFI variables, so nothing on the disc or stick changes:
 * LoaderEntryDefault (which entry starts when nobody chooses) and LoaderConfigTimeout (how long the menu waits).
 * systemd-boot lists its entries in LoaderEntries and the one that started in LoaderEntrySelected.
 * Windows usually sits on another drive, where systemd-boot cannot start it from its own menu; the firmware can:
 * "Restart into Windows" sets BootNext to the firmware's Windows Boot Manager entry (Boot####), once. */
#define LOADERG "4a67b082-0a4c-41cf-b6c7-440b29bb8c4f"
#define GLOBALG "8be4df61-93ca-11d2-aa0d-00e098032b8c"
/* UTF-16LE (ASCII only) to a C string; stops at a NUL */
static void u16s(const unsigned char *d, size_t n, char *o, size_t on)
{
	size_t k = 0;
	for (size_t i = 0; i + 1 < n && k + 1 < on; i += 2) { if (!d[i] && !d[i + 1]) break; o[k++] = d[i + 1] ? '?' : d[i]; }
	o[k] = 0;
}
static size_t su16(const char *s, unsigned char *o)
{
	size_t k = 0; for (; *s; s++) { o[k++] = (unsigned char)*s; o[k++] = 0; } o[k++] = 0; o[k++] = 0; return k;
}
static int boot_entry_ok(const char *e)
{
	if (!e || !*e || strlen(e) > 80) return 0;
	for (const char *c = e; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("-_.", *c)) return 0;
	return 1;
}
/* a firmware boot entry: its title, and whether it starts Windows */
static int boot_option(unsigned n, char *title, size_t tl)
{
	char v[80]; snprintf(v, sizeof v, "Boot%04X-" GLOBALG, n); size_t l = 0; unsigned char *d = efivar(v, &l);
	if (!d || l < 8) { free(d); return -1; }
	u16s(d + 6, l - 6, title, tl);
	char low[600]; size_t k = 0;   /* the device path, as lower-case ASCII, to look for the Windows loader */
	for (size_t i = 6; i + 1 < l && k + 1 < sizeof low; i += 2) if (!d[i + 1] && d[i]) low[k++] = tolower(d[i]);
	low[k] = 0; free(d);
	return strstr(low, "bootmgfw.efi") || strstr(title, "Windows Boot Manager") ? 1 : 0;
}
/* 1.4.1: the installed drive's own firmware boot entry. Firmware does not add one for a drive Rudolf OS is installed
 * on (the ASUS Vivobook Go listed only its old Windows and Ubuntu entries and the USB stick), so at every start on a
 * UEFI computer: when the drive holding RUDOLFDATA has the installed layout (initramfs/init, layout.sh: partition 1
 * empty, 2 the EFI system partition, 3 RUDOLFDATA, a disk signature of its own), a "Rudolf OS" entry naming the
 * drive (disk_devpath; firmware then starts \EFI\BOOT\BOOTX64.EFI, the shim, so it works with Secure Boot too), or for a
 * controller not known there HD(2,MBR,signature,start,size)/\EFI\BOOT\BOOTX64.EFI, is made or corrected, and put first
 * in BootOrder. Other entries named "Rudolf OS ..." (left over, or for the same drive under an older
 * signature) are removed; nothing else is touched. Nothing is written when all is already so. */
/* The drive itself as UEFI names it, the way firmware's own entries for a drive do ("UEFI QEMU NVMe Ctrl ..."):
 * PciRoot(uid)/Pci(dev,fn)/.../NVMe(nsid,EUI-64) or .../Sata(port,0xFFFF,0), with no file. Firmware starts such an
 * entry by looking for \EFI\BOOT\BOOTX64.EFI on the drive, through whichever partitions it found: EDK2's partition
 * driver takes the image's El Torito catalogue before the MBR, so it never makes the HD(2,MBR,...) node a partition
 * path would name (OVMF skipped that entry). RP is the drive's /sys/devices path. Returns the length, 0 if unknown. */
static size_t disk_devpath(const char *rp, const char *dev, unsigned char *o, size_t on)
{
	const char *q = strstr(rp, "/pci"); if (!q || on < 128) return 0;
	unsigned seg = 0, bus = 0; if (sscanf(q, "/pci%x:%x", &seg, &bus) != 2) return 0;
	char f[300], *h; snprintf(f, sizeof f, "/sys/devices/pci%04x:%02x/firmware_node/uid", seg, bus);
	uint32_t uid = (uint32_t)read_ll(f, 0), eisa = 0x0a0341d0;   /* PciRoot(): Acpi(PNP0A03,uid), as EDK2 names it */
	/* firmware that names its PCI root otherwise (PNP0A08) shows it in its own entries: take theirs */
	for (unsigned n = 0; n < 256; n++) {
		char v[80]; snprintf(v, sizeof v, "Boot%04X-" GLOBALG, n); size_t l = 0; unsigned char *d = efivar(v, &l);
		if (!d) continue;
		size_t i = 6; while (i + 1 < l && (d[i] || d[i + 1])) i += 2; i += 2;
		uint32_t e2, u2;
		if (i + 12 <= l && d[i] == 2 && d[i + 1] == 1 && d[i + 2] == 12 && !d[i + 3]) { memcpy(&e2, d + i + 4, 4); memcpy(&u2, d + i + 8, 4); if (u2 == uid && (e2 & 0xffff) == 0x41d0) eisa = e2; }
		free(d);
	}
	size_t k = 0;
	o[k++] = 2; o[k++] = 1; o[k++] = 12; o[k++] = 0; memcpy(o + k, &eisa, 4); memcpy(o + k + 4, &uid, 4); k += 8;
	/* each PCI device on the way: /0000:00:02.2/0000:01:00.0/ */
	const char *s = strchr(q + 1, '/'); unsigned d0, b0, dv, fn; int n;
	while (s && sscanf(s, "/%x:%x:%x.%x%n", &d0, &b0, &dv, &fn, &n) == 4 && s[n] == '/') {
		if (k + 6 > on - 40) return 0;
		o[k++] = 1; o[k++] = 1; o[k++] = 6; o[k++] = 0; o[k++] = (unsigned char)fn; o[k++] = (unsigned char)dv; s += n;
	}
	if (k == 12) return 0;
	const char *nm = dev + 5;
	if (!strncmp(nm, "nvme", 4)) {
		snprintf(f, sizeof f, "/sys/block/%s/nsid", nm); uint32_t ns = (uint32_t)read_ll(f, 0); if (!ns) return 0;
		unsigned char eui[8] = {0}; snprintf(f, sizeof f, "/sys/block/%s/eui", nm);
		if ((h = slurp(f, NULL))) { unsigned b[8]; if (sscanf(h, "%x %x %x %x %x %x %x %x", b, b + 1, b + 2, b + 3, b + 4, b + 5, b + 6, b + 7) == 8) for (int i = 0; i < 8; i++) eui[i] = (unsigned char)b[i]; free(h); }
		o[k++] = 3; o[k++] = 0x17; o[k++] = 16; o[k++] = 0; memcpy(o + k, &ns, 4); memcpy(o + k + 4, eui, 8); k += 12;
	} else if (strstr(rp, "/ata")) {
		const char *a = strstr(rp, "/ata"); unsigned an; if (sscanf(a, "/ata%u/", &an) != 1) return 0;
		snprintf(f, sizeof f, "/sys/class/ata_port/ata%u/port_no", an); long long pn = read_ll(f, 0); if (pn < 1) return 0;
		uint16_t port = (uint16_t)(pn - 1), pm = 0xffff, lun = 0;
		o[k++] = 3; o[k++] = 0x12; o[k++] = 10; o[k++] = 0; memcpy(o + k, &port, 2); memcpy(o + k + 2, &pm, 2); memcpy(o + k + 4, &lun, 2); k += 6;
	} else return 0;
	o[k++] = 0x7f; o[k++] = 0xff; o[k++] = 4; o[k++] = 0;
	return k;
}
static void boot_entry_ensure(void)
{
	if (access("/sys/firmware/efi/efivars", F_OK) || access(RUN "/persistent", F_OK)) return;
	char *p = slurp("/etc/rudolf/persist", NULL); if (!p) return; trim(p);
	char disk[64] = ""; size_t pl = strlen(p);
	if (!strncmp(p, "/dev/nvme", 9) || !strncmp(p, "/dev/mmcblk", 11)) { if (pl > 2 && !strcmp(p + pl - 2, "p3")) snprintf(disk, sizeof disk, "%.*s", (int)(pl - 2), p); }
	else if (pl > 1 && p[pl - 1] == '3') snprintf(disk, sizeof disk, "%.*s", (int)(pl - 1), p);
	free(p); if (!*disk) return;
	char sp[128], rp[4096]; snprintf(sp, sizeof sp, "/sys/block/%s", disk + 5);
	if (!realpath(sp, rp) || strstr(rp, "/usb") || strstr(rp, "/mmc")) return;
	snprintf(sp, sizeof sp, "/sys/block/%s/removable", disk + 5); if (read_ll(sp, 0) == 1) return;
	unsigned char m[512]; int f = open(disk, O_RDONLY | O_CLOEXEC); if (f < 0) return;
	ssize_t got = pread(f, m, 512, 0); close(f); if (got != 512 || m[510] != 0x55 || m[511] != 0xaa) return;
	static const unsigned char zero[16];
	if (memcmp(m + 446, zero, 16) || m[446 + 16 + 4] != 0xef || m[446 + 32 + 4] != 0x83 || !memcmp(m + 440, zero, 4)) return;
	uint32_t st, sz; memcpy(&st, m + 446 + 16 + 8, 4); memcpy(&sz, m + 446 + 16 + 12, 4);
	/* the load option: attributes (active), the device path's length, the title, the device path */
	unsigned char opt[600]; size_t k = 0; uint32_t att = 1; memcpy(opt, &att, 4); k = 6;
	k += su16("Rudolf OS", opt + k);
	unsigned char *dp = opt + k;
	unsigned char hd[42] = {4, 1, 42, 0, 2, 0, 0, 0}; uint64_t st64 = st, sz64 = sz;
	memcpy(hd + 8, &st64, 8); memcpy(hd + 16, &sz64, 8); memcpy(hd + 24, m + 440, 4); hd[40] = 1; hd[41] = 1;
	unsigned char fp[64]; size_t fl = su16("\\EFI\\BOOT\\BOOTX64.EFI", fp);
	size_t dl = disk_devpath(rp, disk, dp, sizeof opt - k), whole = dl > 0;
	if (!dl) {   /* a controller not named above: the partition and the file */
		memcpy(dp, hd, 42); dl = 42;
		dp[dl] = 4; dp[dl + 1] = 4; dp[dl + 2] = (unsigned char)(4 + fl); dp[dl + 3] = 0; memcpy(dp + dl + 4, fp, fl); dl += 4 + fl;
		dp[dl] = 0x7f; dp[dl + 1] = 0xff; dp[dl + 2] = 4; dp[dl + 3] = 0; dl += 4;
	}
	/* 1.4.2: firmware may not start the drive's path as written. The Vivobook's (AMI) skipped it at every start and
	   changed it, and started the entry made by hand in its setup, which names the drive the same way and goes on
	   to the image's El Torito catalogue (CDROM(...)) and \EFI\BOOT\BOOTX64.EFI. So when this system was started by
	   another entry (BootCurrent) whose path begins with the drive's own, that is the path that works: ours takes it. */
	size_t pre = whole ? dl - 4 : 0, ll = 0; unsigned char learned[400]; int cur = -1;
	{ size_t l = 0; unsigned char *c = efivar("BootCurrent-" GLOBALG, &l); if (c && l >= 2) cur = c[0] | c[1] << 8; free(c); }
	if (pre && cur >= 0) {
		char v[80]; snprintf(v, sizeof v, "Boot%04X-" GLOBALG, cur); size_t l = 0; unsigned char *d = efivar(v, &l);
		size_t i = 6; while (d && i + 1 < l && (d[i] || d[i + 1])) i += 2;
		if (d && i + 1 < l) {
			char t[100]; u16s(d + 6, l - 6, t, sizeof t); uint16_t pl; memcpy(&pl, d + 4, 2); i += 2;
			if (strncmp(t, "Rudolf OS", 9) && i + pl <= l && pl > pre && pl <= sizeof learned && k + pl <= sizeof opt && !memcmp(d + i, dp, pre)) { memcpy(learned, d + i, pl); ll = pl; }
		}
		free(d);
	}
	if (ll) { memcpy(dp, learned, ll); dl = ll; }
	uint16_t dl16 = (uint16_t)dl; memcpy(opt + 4, &dl16, 2); k += dl;
	/* ours, and the other "Rudolf OS ..." entries */
	int mine = -1, same = 0, rm[32], nrm = 0, used[256] = {0};
	for (unsigned n = 0; n < 256; n++) {
		char v[80], t[100]; snprintf(v, sizeof v, "Boot%04X-" GLOBALG, n); size_t l = 0; unsigned char *d = efivar(v, &l);
		if (!d) continue;
		used[n] = 1; t[0] = 0; if (l > 6) u16s(d + 6, l - 6, t, sizeof t);
		if (!strncmp(t, "Rudolf OS", 9)) {
			/* ours when it still names this drive's partition 2 and the start file: firmware may store the path in
			   its own, longer form (the whole device path in front of HD(...)) */
			if (mine < 0 && !strcmp(t, "Rudolf OS")) {
				mine = n; same = (l == k && !memcmp(d, opt, k)) || (!whole && l >= 6 && (d[0] & 1) && memmem(d, l, hd, 42) && memmem(d, l, fp, fl));
				/* 1.4.2: one that names this drive is kept as it is unless another entry started this system (then
				   ours takes that one's path, above): it may be the path learned at an earlier start */
				size_t i = 6; while (i + 1 < l && (d[i] || d[i + 1])) i += 2; i += 2;
				if (!same && whole && !ll && (d[0] & 1) && i + pre <= l && !memcmp(d + i, dp, pre)) same = 1;
			}
			else if (nrm < 32) rm[nrm++] = n;
		}
		free(d);
	}
	if (mine < 0) { for (unsigned n = 0; n < 256; n++) if (!used[n]) { mine = n; break; } if (mine < 0) return; }
	char v[80]; snprintf(v, sizeof v, "Boot%04X-" GLOBALG, mine);
	if (!same) { int had = used[mine]; if (efivar_write(v, opt, k)) { fprintf(stderr, "rudolfd: the boot entry %s could not be written\n", v); return; } fprintf(stderr, "rudolfd: boot entry Boot%04X \"Rudolf OS\" for %s (%s%s)\n", mine, disk, had ? "it named another path" : "new", ll ? ", the path of the entry this system started from" : ""); }
	for (int i = 0; i < nrm; i++) { char f2[200]; snprintf(f2, sizeof f2, EFIV "/Boot%04X-" GLOBALG, rm[i]); unimmutable(f2); unlink(f2); fprintf(stderr, "rudolfd: removed the old boot entry Boot%04X\n", rm[i]); }
	/* BootOrder: ours first, the removed ones out, the rest as they were */
	size_t ol = 0; unsigned char *o = efivar("BootOrder-" GLOBALG, &ol);
	uint16_t nw[300]; size_t nn = 0; nw[nn++] = (uint16_t)mine;
	for (size_t i = 0; o && i + 1 < ol && nn < 300; i += 2) {
		uint16_t e; memcpy(&e, o + i, 2); int skip = e == mine;
		for (int j = 0; j < nrm; j++) if (e == rm[j]) skip = 1;
		if (!skip) nw[nn++] = e;
	}
	if (!o || ol != nn * 2 || memcmp(o, nw, ol)) { if (!efivar_write("BootOrder-" GLOBALG, (unsigned char *)nw, nn * 2)) fprintf(stderr, "rudolfd: Boot%04X first in the boot order\n", mine); }
	free(o);
}
static void api_bootmenu(struct req *r, const char *what, int post)
{
	int efi = !access("/sys/firmware/efi", F_OK);
	if (!strcmp(what, "") && !post) {
		str s = {0}; sadd(&s, "{\"ok\":true,\"uefi\":%s", efi ? "true" : "false");
		if (efi) {
			size_t l = 0; unsigned char *d = efivar("LoaderEntries-" LOADERG, &l); char e[100];
			sadd(&s, ",\"loader\":%s,\"entries\":[", d ? "true" : "false");
			for (size_t i = 0, first = 1; d && i + 1 < l;) {
				u16s(d + i, l - i, e, sizeof e); size_t el = strlen(e);
				if (el) { if (!first) sadd(&s, ","); sjson(&s, e); first = 0; }
				i += (el + 1) * 2;
			}
			free(d); sadd(&s, "]");
			const char *one[][2] = { {"selected", "LoaderEntrySelected-" LOADERG}, {"default", "LoaderEntryDefault-" LOADERG}, {"timeout", "LoaderConfigTimeout-" LOADERG} };
			for (int i = 0; i < 3; i++) { d = efivar(one[i][1], &l); e[0] = 0; if (d) u16s(d, l, e, sizeof e); free(d); sadd(&s, ",\"%s\":", one[i][0]); sjson(&s, e); }
			sadd(&s, ",\"windows\":[");
			int first = 1; DIR *dd = opendir(EFIV); struct dirent *de;
			while (dd && (de = readdir(dd))) {
				unsigned n; char t[200];
				if (strlen(de->d_name) != 4 + 4 + 1 + 36 || sscanf(de->d_name, "Boot%4X-", &n) != 1 || strcmp(de->d_name + 9, GLOBALG)) continue;
				if (boot_option(n, t, sizeof t) != 1) continue;
				sadd(&s, "%s{\"n\":\"%04X\",\"title\":", first ? "" : ",", n); sjson(&s, t); sadd(&s, "}"); first = 0;
			}
			if (dd) closedir(dd);
			sadd(&s, "]");
		}
		sadd(&s, "}"); reply_json(r->fd, &s); return;
	}
	if (!post) { reply_err(r->fd, 400, "use POST"); return; }
	if (!efi) { reply_err(r->fd, 409, "this computer started in BIOS mode"); return; }
	if (!strcmp(what, "set")) {
		/* default=ID (an entry systemd-boot listed) or empty for the disc's own; timeout=0..10 or empty */
		char *def = qparam(r->query, "default"), *to = qparam(r->query, "timeout");
		unsigned char b[200];
		if (def) {
			if (!*def) { char f[200]; snprintf(f, sizeof f, EFIV "/LoaderEntryDefault-" LOADERG); unimmutable(f); unlink(f); }
			else {
				size_t l = 0; unsigned char *d = efivar("LoaderEntries-" LOADERG, &l); int found = 0; char e[100];
				for (size_t i = 0; d && i + 1 < l;) { u16s(d + i, l - i, e, sizeof e); if (!strcmp(e, def)) found = 1; i += (strlen(e) + 1) * 2; if (!*e) break; }
				free(d);
				if (!boot_entry_ok(def) || !found) { reply_err(r->fd, 400, "that is not an entry of the boot menu"); return; }
				if (efivar_write("LoaderEntryDefault-" LOADERG, b, su16(def, b))) { reply_err(r->fd, 500, "the firmware did not store the choice"); return; }
			}
		}
		if (to) {
			if (!*to) { char f[200]; snprintf(f, sizeof f, EFIV "/LoaderConfigTimeout-" LOADERG); unimmutable(f); unlink(f); }
			else {
				char *end; long t = strtol(to, &end, 10);
				if (*end || t < 0 || t > 10) { reply_err(r->fd, 400, "the wait is 0 to 10 seconds"); return; }
				char ts[8]; snprintf(ts, sizeof ts, "%ld", t);
				if (efivar_write("LoaderConfigTimeout-" LOADERG, b, su16(ts, b))) { reply_err(r->fd, 500, "the firmware did not store the wait"); return; }
			}
		}
		reply_ok(r->fd, "saved"); return;
	}
	if (!strcmp(what, "windows")) {
		/* the next start only: the firmware starts this Windows Boot Manager entry, then BootNext is gone */
		char *n = qparam(r->query, "n"), t[200]; unsigned v;
		if (!n || strlen(n) != 4 || sscanf(n, "%4X", &v) != 1 || boot_option(v, t, sizeof t) != 1) { reply_err(r->fd, 400, "that is not a Windows entry"); return; }
		unsigned char b[2] = { v & 255, v >> 8 };
		if (efivar_write("BootNext-" GLOBALG, b, 2)) { reply_err(r->fd, 500, "the firmware did not store it"); return; }
		reply_ok(r->fd, "Windows starts at the next restart"); return;
	}
	reply_err(r->fd, 404, "unknown boot menu action");
}
static void api_secureboot(struct req *r, const char *what, int post)
{
	int efi = !access("/sys/firmware/efi", F_OK);
	char f[200]; snprintf(f, sizeof f, EFIV "/MokNew-" SHIMG);
	if (!strcmp(what, "status")) {
		str s = {0}; char *lk = slurp("/sys/kernel/security/lockdown", NULL), *uv = slurp("/etc/rudolf/unverified", NULL), *en = slurp("/etc/rudolf/boot-entry", NULL);
		size_t sl = 0; unsigned char *fw = efi ? efivar("SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c", &sl) : NULL;
		sadd(&s, "{\"ok\":true,\"efi\":%s,\"on\":%s,\"firmware\":%s,\"enrolled\":%s,\"pending\":%s,\"certificate\":%s", efi ? "true" : "false", secureboot ? "true" : "false",
			fw && sl ? (fw[0] ? "true" : "false") : "null", efi && sb_enrolled() ? "true" : "false", efi && !access(f, F_OK) ? "true" : "false", access(SBCERT, R_OK) ? "false" : "true");
		jtext(&s, "lockdown", lk); jtext(&s, "unverified", uv); jtext(&s, "entry", en);
		free(fw); sadd(&s, "}"); reply_json(r->fd, &s); return;
	}
	if (!post) { reply_err(r->fd, 400, "use POST"); return; }
	if (!efi) { reply_err(r->fd, 400, "this computer started without UEFI; Secure Boot is a UEFI feature"); return; }
	if (!strcmp(what, "cancel")) {
		char a[200]; snprintf(a, sizeof a, EFIV "/MokAuth-" SHIMG);
		if (!access(f, F_OK)) { unimmutable(f); unlink(f); }
		if (!access(a, F_OK)) { unimmutable(a); unlink(a); }
		reply_ok(r->fd, "cancelled"); return;
	}
	if (!strcmp(what, "prepare")) {
		if (sb_enrolled()) { reply_err(r->fd, 409, "the Rudolf OS key is already enrolled"); return; }
		size_t cn; unsigned char *c = sb_cert(&cn); if (!c || cn < 100) { free(c); reply_err(r->fd, 500, "the Rudolf OS certificate is missing"); return; }
		/* EFI_SIGNATURE_LIST: type X.509, list size, header size 0, signature size; one entry: owner (shim) + DER */
		static const unsigned char X509[16] = {0xa1, 0x59, 0xc0, 0xa5, 0xe4, 0x94, 0xa7, 0x4a, 0x87, 0xb5, 0xab, 0x15, 0x5c, 0x2b, 0xf0, 0x72};
		static const unsigned char OWNER[16] = {0x50, 0xab, 0x5d, 0x60, 0x46, 0xe0, 0x00, 0x43, 0xab, 0xb6, 0x3d, 0xd8, 0x10, 0xdd, 0x8b, 0x23};
		size_t ln = 28 + 16 + cn; unsigned char *l = calloc(1, ln);
		uint32_t ls = ln, hs = 0, ss = 16 + cn;
		memcpy(l, X509, 16); memcpy(l + 16, &ls, 4); memcpy(l + 20, &hs, 4); memcpy(l + 24, &ss, 4); memcpy(l + 28, OWNER, 16); memcpy(l + 44, c, cn); free(c);
		/* a one-time password of 8 digits (MokManager asks for it) */
		unsigned char rb[8]; int rf = open("/dev/urandom", O_RDONLY); if (rf < 0 || read(rf, rb, 8) != 8) { if (rf >= 0) close(rf); free(l); reply_err(r->fd, 500, "no randomness"); return; } close(rf);
		char pw[9]; for (int i = 0; i < 8; i++) pw[i] = '0' + rb[i] % 10; pw[8] = 0;
		/* MokAuth = SHA-256(list || password in UCS-2), with the system's sha256sum */
		char tf[64]; snprintf(tf, sizeof tf, RUN "/mokauth.%d", getpid());
		int t = open(tf, O_WRONLY | O_CREAT | O_TRUNC, 0600); int ok = t >= 0 && write(t, l, ln) == (ssize_t)ln;
		for (int i = 0; ok && i < 8; i++) { unsigned char u[2] = {(unsigned char)pw[i], 0}; ok = write(t, u, 2) == 2; }
		if (t >= 0) close(t);
		char cmd[120]; snprintf(cmd, sizeof cmd, "sha256sum %s", tf); char *h = ok ? run_out(cmd) : NULL; unlink(tf);
		unsigned char auth[32]; for (int i = 0; ok && i < 32; i++) { unsigned v; if (!h || sscanf(h + 2 * i, "%2x", &v) != 1) ok = 0; else auth[i] = v; }
		free(h);
		if (!ok || efivar_write("MokNew-" SHIMG, l, ln) || efivar_write("MokAuth-" SHIMG, auth, 32)) { free(l); reply_err(r->fd, 500, "the request could not be stored in the firmware"); return; }
		free(l);
		str s = {0}; sadd(&s, "{\"ok\":true,\"password\":\"%s\"}", pw); reply_json(r->fd, &s); return;
	}
	reply_err(r->fd, 404, "unknown Secure Boot action");
}

/* ------------------------------------------------------------- dispatcher */
static int restart_browser(void) { return run("pkill -f 'chrom[e].*--rudolf-kiosk' >/dev/null 2>&1"); }

/* ------------------------------------------------------------- the development link (1.4.0)
 * rudolf-devlink runs commands from a private GitHub repository the owner chose (see rudolf-devlink.c and
 * docs/plan-1.4.0.md). Here: switching it on (the desktop asks for an administrator's password first; the token is
 * checked with GitHub, then kept root-only and never sent back), off, forgetting the token, the Allow prompt, the
 * log, and starting it again after a restart when the owner ticked that and its time is not up. */
#define DLBIN "/usr/bin/rudolf-devlink"
#define DLRUN RUN "/devlink"
static void dl_dir(char *o, size_t n) { snprintf(o, n, "%s", access(RUN "/persistent", F_OK) ? RUN "/devlink-store" : PERSIST "/devlink"); }
static char *dl_cfg(const char *key)
{
	char d[128], p[160]; dl_dir(d, sizeof d); snprintf(p, sizeof p, "%s/config", d);
	char *c = slurp(p, NULL); if (!c) return NULL;
	char k[64]; snprintf(k, sizeof k, "%s=", key); char *v = NULL;
	for (char *l = strtok(c, "\n"); l; l = strtok(NULL, "\n")) if (!strncmp(l, k, strlen(k))) { v = strdup(l + strlen(k)); break; }
	free(c); return v;
}
static int dl_cfg_on(const char *key) { char *v = dl_cfg(key); int r = v && *v == '1'; free(v); return r; }
static void dl_cfg_write(const char *repo, long long until, int restart, int ask, int home, int on)
{
	char d[128], p[160], t[160]; dl_dir(d, sizeof d); mkdir(d, 0700); snprintf(p, sizeof p, "%s/config", d); snprintf(t, sizeof t, "%s/config.new", d);
	str s = {0}; sadd(&s, "on=%d\nrepo=%s\nuntil=%lld\nrestart=%d\nask=%d\nhome=%d\n", on, repo, until, restart, ask, home);
	int f = open(t, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600); if (f >= 0) { if (write(f, s.b, s.n) < 0) {} fsync(f); close(f); rename(t, p); }
	free(s.b);
}
static void dl_set_off(void)
{
	char *repo = dl_cfg("repo"), *u = dl_cfg("until");
	dl_cfg_write(repo ? repo : "", u ? atoll(u) : 0, dl_cfg_on("restart"), dl_cfg_on("ask"), dl_cfg_on("home"), 0);
	free(repo); free(u);
}
static int dl_pid(void) { long long p = read_ll(DLRUN "/pid", 0); return p > 1 && !kill((pid_t)p, 0) ? (int)p : 0; }
static int dl_repo_ok(const char *r)
{
	if (!r || !*r || strlen(r) > 100 || strstr(r, "..")) return 0;
	const char *sl = strchr(r, '/'); if (!sl || sl == r || !sl[1] || strchr(sl + 1, '/')) return 0;
	for (const char *c = r; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("-_./", *c)) return 0;
	return 1;
}
static void dl_start(void)
{
	mkdir(DLRUN, 0755); unlink(DLRUN "/stop"); unlink(DLRUN "/status.json");
	pid_t p = fork();
	if (p == 0) {
		setsid();
		if (fork() == 0) {
			int n = open("/dev/null", O_RDWR), l = open(DLRUN "/daemon.log", O_WRONLY | O_CREAT | O_APPEND, 0600);
			dup2(n, 0); dup2(l >= 0 ? l : n, 1); dup2(l >= 0 ? l : n, 2);
			char te[64]; snprintf(te, sizeof te, "RUDOLF_TOKEN=%s", token);
			char *env[] = { te, "PATH=/usr/sbin:/usr/bin:/sbin:/bin", "HOME=/root", NULL };
			char *argv[] = { DLBIN, "run", NULL };
			execve(DLBIN, argv, env); _exit(127);
		}
		_exit(0);
	}
	if (p > 0) waitpid(p, NULL, 0);
	for (int i = 0; i < 20 && !dl_pid(); i++) usleep(100000);
}
static void dl_stop(void)
{
	mkdir(DLRUN, 0755); write_str(DLRUN "/stop", "1");
	int p = dl_pid(); if (p) { kill(p, SIGTERM); for (int i = 0; i < 50 && !kill(p, 0); i++) usleep(100000); }
	unlink(DLRUN "/pending.json");
}
/* at start-up: on again only when the owner ticked "stay on across a restart" and its time is not up */
static void dl_boot(void)
{
	if (!dl_cfg_on("on")) return;
	char *u = dl_cfg("until"); long long until = u ? atoll(u) : 0; free(u);
	if (dl_cfg_on("restart") && !access(RUN "/persistent", F_OK) && until > (long long)time(NULL) && !access(DLBIN, X_OK)) { fprintf(stderr, "rudolfd: the development link stays on until %lld\n", until); dl_start(); }
	else dl_set_off();
}
static int id6(const char *s) { if (!s || strlen(s) != 6) return 0; for (int i = 0; i < 6; i++) if (!isdigit((unsigned char)s[i])) return 0; return 1; }
static void api_devlink(struct req *r, const char *what, int post)
{
	char d[128]; dl_dir(d, sizeof d);
	if (!*what && !post) {
		char *repo = dl_cfg("repo"), *u = dl_cfg("until"), *st = slurp(DLRUN "/status.json", NULL), *pend = slurp(DLRUN "/pending.json", NULL);
		char tp[160]; snprintf(tp, sizeof tp, "%s/token", d); int pid = dl_pid(); long long until = u ? atoll(u) : 0;
		str s = {0}; sadd(&s, "{\"ok\":true,\"available\":%s,\"on\":%s,\"repo\":", access(DLBIN, X_OK) ? "false" : "true", pid ? "true" : "false"); sjson(&s, repo ? repo : "");
		sadd(&s, ",\"until\":%lld,\"secondsLeft\":%lld,\"restart\":%s,\"ask\":%s,\"home\":%s,\"hasToken\":%s,\"persistent\":%s", until, pid && until > time(NULL) ? until - (long long)time(NULL) : 0,
		     dl_cfg_on("restart") ? "true" : "false", dl_cfg_on("ask") || !repo ? "true" : "false", dl_cfg_on("home") ? "true" : "false", access(tp, F_OK) ? "false" : "true", access(RUN "/persistent", F_OK) ? "false" : "true");
		if (st) { trim(st); sadd(&s, ",\"status\":%s", st[0] == '{' ? st : "null"); }
		if (pend && pid) { trim(pend); sadd(&s, ",\"pending\":%s", pend[0] == '{' ? pend : "null"); }
		sadd(&s, "}"); free(repo); free(u); free(st); free(pend); reply_json(r->fd, &s); return;
	}
	if (!strcmp(what, "on") && post) {
		if (access(DLBIN, X_OK)) { reply_err(r->fd, 501, "this Rudolf OS has no development link"); return; }
		char *repo = qparam(r->query, "repo"), *h = qparam(r->query, "hours");
		int hours = h ? atoi(h) : 0;
		if (!dl_repo_ok(repo)) { reply_err(r->fd, 400, "write the repository as owner/name (letters, digits, - _ .)"); return; }
		if (hours != 1 && hours != 2 && hours != 4 && hours != 8) { reply_err(r->fd, 400, "for 1, 2, 4 or 8 hours"); return; }
		char *rs = qparam(r->query, "restart"), *as = qparam(r->query, "ask"), *hs = qparam(r->query, "home");
		int restart = rs && *rs == '1', askf = !as || *as != '0', home = hs && *hs == '1';
		if (restart && access(RUN "/persistent", F_OK)) { reply_err(r->fd, 400, "staying on across a restart needs the files kept on this drive (RUDOLFDATA)"); return; }
		mkdir(d, 0700); chmod(d, 0700);
		char tp[160], tn[170]; snprintf(tp, sizeof tp, "%s/token", d); snprintf(tn, sizeof tn, "%s/token.new", d);
		/* a new token in the body, or the one kept when switching on again */
		size_t bl = r->body ? r->blen : 0; while (bl && isspace((unsigned char)r->body[bl - 1])) bl--;
		const char *bt = r->body ? r->body : ""; while (bl && isspace((unsigned char)*bt)) { bt++; bl--; }
		if (bl) {
			if (bl > 255) { reply_err(r->fd, 400, "that is not a GitHub token"); return; }
			int f = open(tn, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600); if (f < 0 || write(f, bt, bl) != (ssize_t)bl) { if (f >= 0) close(f); reply_err(r->fd, 500, "the token could not be stored"); return; }
			fsync(f); close(f);
		} else if (access(tp, F_OK)) { reply_err(r->fd, 400, "paste the token from GitHub"); return; }
		else { char *o = slurp(tp, NULL); write_str(tn, ""); chmod(tn, 0600); write_str(tn, o ? o : ""); free(o); }
		char cmdl[400]; snprintf(cmdl, sizeof cmdl, DLBIN " check '%s' '%s' 2>&1", repo, tn);
		char *o = run_out(cmdl); if (o) trim(o);
		if (!o || strcmp(o, "ok")) { unlink(tn); reply_err(r->fd, 400, o && *o ? o : "the token could not be checked"); free(o); return; }
		free(o);
		dl_stop(); rename(tn, tp); chmod(tp, 0600);
		dl_cfg_write(repo, (long long)time(NULL) + hours * 3600LL, restart, askf, home, 1);
		dl_start();
		if (!dl_pid()) { reply_err(r->fd, 500, "the development link did not start"); return; }
		reply_ok(r->fd, "on"); return;
	}
	if (!strcmp(what, "off") && post) { dl_stop(); dl_set_off(); reply_ok(r->fd, "off"); return; }
	if (!strcmp(what, "forget") && post) { char tp[160]; dl_stop(); dl_set_off(); snprintf(tp, sizeof tp, "%s/token", d); unlink(tp); reply_ok(r->fd, "the token is deleted"); return; }
	if (!strcmp(what, "answer") && post) {
		char *id = qparam(r->query, "id"), *a = qparam(r->query, "a");
		char *pend = slurp(DLRUN "/pending.json", NULL); char want[32]; snprintf(want, sizeof want, "\"id\":\"%s\"", id ? id : "");
		if (!id6(id) || !a || (strcmp(a, "allow") && strcmp(a, "refuse")) || !pend || !strstr(pend, want)) { free(pend); reply_err(r->fd, 409, "that command is no longer waiting"); return; }
		free(pend); char ap[160]; snprintf(ap, sizeof ap, DLRUN "/answer.%s", id); write_str(ap, a); reply_ok(r->fd, a); return;
	}
	if (!strcmp(what, "log") && !post) {
		char lp[160]; snprintf(lp, sizeof lp, "%s/log", d); char *l = slurp(lp, NULL); const char *t = l ? l : "";
		/* the last 200 lines */
		int n = 0; const char *q = t + strlen(t); while (q > t && n <= 200) { q--; if (*q == '\n') n++; } if (n > 200) q++;
		str s = {0}; sadd(&s, "{\"ok\":true,\"log\":"); sjson(&s, q); sadd(&s, "}"); free(l); reply_json(r->fd, &s); return;
	}
	reply_err(r->fd, 404, "unknown request");
}

static void api(struct req *r)
{
	const char *p = r->path + 5;   /* after /api/ */
	int post = !strcmp(r->method, "POST");
	if (!strcmp(p, "ping")) { reply_ok(r->fd, "rudolfd"); return; }
	if (!strcmp(p, "info")) { api_info(r); return; }
	if (!strcmp(p, "stats")) { api_stats(r); return; }
	if (!strcmp(p, "system")) { api_system(r); return; }
	if (!strcmp(p, "processes")) { api_processes(r); return; }
	if (!strcmp(p, "net")) { api_net(r); return; }
	if (!strcmp(p, "disks")) { api_disks(r); return; }
	/* the hardware check's download test (1.2.5): rudolf-update speedtest prints "bytes N" and "ms N" */
	if (!strcmp(p, "speedtest")) {
		char *o = run_out("/usr/bin/rudolf-update speedtest 2>&1"); long long by = 0, ms = 0; char *q;
		if (o && (q = strstr(o, "bytes "))) by = atoll(q + 6);
		if (o && (q = strstr(o, "ms "))) ms = atoll(q + 3);
		if (by <= 0) { reply_err(r->fd, 502, o && *o ? strtok(o, "\n") : "the update server could not be reached"); free(o); return; }
		str s = {0}; sadd(&s, "{\"ok\":true,\"bytes\":%lld,\"ms\":%lld}", by, ms); free(o); reply_json(r->fd, &s); return;
	}
	if (!strcmp(p, "install/images")) {
		/* disc images downloaded to the Downloads folder, for Install from a newer image (1.2.4) */
		str s = {0}; sadd(&s, "{\"ok\":true,\"images\":[");
		DIR *dd = opendir("/home/rudolf/Downloads"); struct dirent *e; int n = 0;
		while (dd && (e = readdir(dd))) {
			size_t l = strlen(e->d_name);
			if (l < 5 || strcasecmp(e->d_name + l - 4, ".iso") || e->d_name[0] == '.') continue;
			char fp[600]; snprintf(fp, sizeof fp, "/home/rudolf/Downloads/%s", e->d_name); struct stat st;
			if (stat(fp, &st) || !S_ISREG(st.st_mode)) continue;
			sadd(&s, "%s{\"path\":", n++ ? "," : ""); sjson(&s, fp); sadd(&s, ",\"name\":"); sjson(&s, e->d_name); sadd(&s, ",\"bytes\":%lld,\"boot\":%s}", (long long)st.st_size, l > 9 && !strcasecmp(e->d_name + l - 9, "-boot.iso") ? "true" : "false");
		}
		if (dd) closedir(dd);
		/* 1.2.7: an image downloaded in parts (NAME.iso.part00, part01, ... with no gaps), not joined yet; install.sh
		   joins it (joinparts.sh) when it is chosen. path is the image it becomes. */
		dd = opendir("/home/rudolf/Downloads");
		char seen[64][200]; int ns = 0;
		while (dd && (e = readdir(dd))) {
			size_t l = strlen(e->d_name);
			if (l < 12 || l >= 200 || e->d_name[0] == '.' || strncasecmp(e->d_name + l - 7, ".part", 5) || !isdigit((unsigned char)e->d_name[l - 2]) || !isdigit((unsigned char)e->d_name[l - 1])) continue;
			char base[200]; snprintf(base, sizeof base, "%.*s", (int)(l - 7), e->d_name);
			size_t bl = strlen(base); if (bl < 5 || strcasecmp(base + bl - 4, ".iso")) continue;
			int dup = 0; for (int i = 0; i < ns; i++) if (!strcmp(seen[i], base)) dup = 1;
			if (dup || ns >= 64) continue;
			snprintf(seen[ns++], 200, "%s", base);
			char fp[600]; struct stat st; snprintf(fp, sizeof fp, "/home/rudolf/Downloads/%s", base);
			if (!stat(fp, &st)) continue;   /* already joined: listed above */
			/* count part00, part01, ... until one is missing (either case for "part") */
			int np = 0; long long tot = 0;
			for (;; np++) {
				char pp[640]; struct stat ps; int found = 0;
				for (int c = 0; c < 2 && !found; c++) { snprintf(pp, sizeof pp, "/home/rudolf/Downloads/%s.%s%02d", base, c ? "PART" : "part", np); if (!stat(pp, &ps) && S_ISREG(ps.st_mode)) found = 1; }
				if (!found) break;
				tot += ps.st_size;
			}
			if (!np) continue;
			sadd(&s, "%s{\"path\":", n++ ? "," : ""); sjson(&s, fp); sadd(&s, ",\"name\":"); sjson(&s, base);
			sadd(&s, ",\"bytes\":%lld,\"parts\":%d,\"boot\":%s}", tot, np, bl > 9 && !strcasecmp(base + bl - 9, "-boot.iso") ? "true" : "false");
		}
		if (dd) closedir(dd);
		sadd(&s, "]}"); reply_json(r->fd, &s); return;
	}
	if (!strcmp(p, "keepfiles") && !post) {
		/* {"possible":bool,"disk","freeBytes","reason","pending":bool} (keepfiles.sh check) */
		char *o = run_out("/usr/lib/rudolf/keepfiles.sh check 2>/dev/null");
		char *nl = o ? strchr(o, '\n') : NULL; if (nl) *nl = 0;
		str s = {0}; int ok = o && !strncmp(o, "ok ", 3);
		sadd(&s, "{\"ok\":true,\"possible\":%s,\"pending\":%s", ok ? "true" : "false", access(RUN "/keepfiles-pending", F_OK) ? "false" : "true");
		if (ok) { char d[64] = ""; long long mb = 0; sscanf(o + 3, "%63s %lld", d, &mb); sadd(&s, ",\"disk\":"); sjson(&s, d); sadd(&s, ",\"freeBytes\":%lld", mb * 1048576); }
		else { sadd(&s, ",\"reason\":"); sjson(&s, o && !strncmp(o, "no ", 3) ? o + 3 : "unknown"); }
		sadd(&s, "}"); free(o); reply_json(r->fd, &s); return;
	}
	if (!strcmp(p, "devices")) { char *o = run_out("/usr/lib/rudolf/devplug list 2>/dev/null"); if (!o || o[0] != '{') { reply_err(r->fd, 500, "could not list the devices"); free(o); return; } reply(r->fd, 200, "application/json", o, strlen(o), NULL); free(o); return; }
	if (!strcmp(p, "job")) { api_job(r); return; }
	if (!strncmp(p, "devlink", 7) && (!p[7] || p[7] == '/')) { api_devlink(r, p[7] ? p + 8 : "", post); return; }
	if (!strcmp(p, "crashes")) { str s = {0}; sadd(&s, "{\"ok\":true,\"crashes\":["); crash_list(&s, "", 20); sadd(&s, "]}"); reply_json(r->fd, &s); return; }
	if (!strcmp(p, "screenshot")) { api_screenshot(r); return; }
	if (!strcmp(p, "heartbeat")) {
		write_str(RUN "/heartbeat", "1");
		/* the first heartbeat after a start: the desktop works, so an update being tried is kept */
		if (access(RUN "/bootok", F_OK)) { write_str(RUN "/bootok", "1"); if (!access("/usr/bin/rudolf-update", X_OK)) run("/usr/bin/rudolf-update boot-ok >>" RUN "/update.log 2>&1"); }
		reply_ok(r->fd, "ok"); return;
	}
	/* 1.4.4: events/wait waits (up to 25 s) until there are events, so the desktop hears of a key press (the volume
	   and brightness indicator), the charger or Wi-Fi at once instead of at its next check; each connection has its
	   own process, so waiting here holds nothing up */
	if (!strcmp(p, "events/wait")) {
		struct stat st;
		for (int i = 0; i < 250; i++) { if (!stat(RUN "/events", &st) && st.st_size > 0) break; usleep(100000); }
		p = "events";
	}
	if (!strcmp(p, "events")) {
		char *ev = slurp(RUN "/events", NULL);
		unlink(RUN "/events");
		str s = {0}; sadd(&s, "{\"ok\":true,\"events\":"); sjson(&s, ev ? ev : ""); sadd(&s, "}");
		free(ev); reply_json(r->fd, &s); return;
	}
	if (!strcmp(p, "log")) { char *l = run_out("dmesg | tail -200"); str s = {0}; sadd(&s, "{\"ok\":true,\"log\":"); sjson(&s, l); sadd(&s, "}"); free(l); reply_json(r->fd, &s); return; }
	if (!strcmp(p, "native/status")) { api_native_status(r); return; }
	if (!strcmp(p, "sys/status")) { char *o = access("/usr/bin/rudolf-update", X_OK) ? NULL : run_out("/usr/bin/rudolf-update status 2>/dev/null"); if (!o || o[0] != '{') { reply_err(r->fd, 404, "this Rudolf OS has no system updates"); free(o); return; } reply(r->fd, 200, "application/json", o, strlen(o), NULL); free(o); return; }
	if (!strcmp(p, "pkg/status")) { if (access("/usr/bin/rpk", X_OK)) { reply_err(r->fd, 404, "this Rudolf OS has no package manager"); return; } char *o = run_out("/usr/bin/rpk status 2>/dev/null"); if (!o || o[0] != '{') { reply_err(r->fd, 500, "rpk failed"); free(o); return; } reply(r->fd, 200, "application/json", o, strlen(o), NULL); free(o); return; }
	if (!strcmp(p, "clientlog")) { char *c = slurp(RUN "/page.log", NULL); str s = {0}; sadd(&s, "{\"ok\":true,\"log\":"); sjson(&s, c ? c : ""); sadd(&s, "}"); free(c); reply_json(r->fd, &s); return; }
	if (!strcmp(p, "check")) { char *c = slurp(RUN "/check.txt", NULL); str s = {0}; sadd(&s, "{\"ok\":true,\"result\":"); sjson(&s, c ? c : ""); sadd(&s, "}"); free(c); reply_json(r->fd, &s); return; }
	if (!strcmp(p, "downloads")) {
		char *l = run_out("ls -l --full-time /home/rudolf/Downloads 2>/dev/null | tail -n +2");
		str s = {0}; sadd(&s, "{\"ok\":true,\"list\":"); sjson(&s, l); sadd(&s, "}"); free(l); reply_json(r->fd, &s); return;
	}
	if (!strncmp(p, "download/", 9)) {
		char *n = r->path + 14;
		urldecode(n);
		if (strchr(n, '/') || n[0] == '.') { reply_err(r->fd, 400, "bad name"); return; }
		char fp[600]; snprintf(fp, sizeof fp, "/home/rudolf/Downloads/%s", n); serve_file(r, fp); return;
	}
	if (!strncmp(p, "fs/", 3)) {
		if (strcmp(p + 3, "list") && strcmp(p + 3, "read") && !post) { reply_err(r->fd, 400, "use POST"); return; }
		api_fs(r, p + 3); return;
	}
	if (!strcmp(p, "decktest")) {
		/* Cyberdeck test mode: approximate the Intel N100's CPU on a faster PC (see /usr/lib/rudolf/decktest) */
		char *on = qparam(r->query, "on");
		if (on) {
			if (!post) { reply_err(r->fd, 400, "use POST"); return; }
			int en = !strcmp(on, "1");
			run(en ? "/usr/lib/rudolf/decktest on" : "/usr/lib/rudolf/decktest off");
			if (!live) { if (en) { mkdir(PERSIST "/etc", 0755); write_str(PERSIST "/etc/decktest", "1"); } else unlink(PERSIST "/etc/decktest"); }
			reply_ok(r->fd, en ? "Cyberdeck test mode is on" : "Cyberdeck test mode is off"); return;
		}
		char *o = run_out("/usr/lib/rudolf/decktest status");
		str s = {0}; sadd(&s, "{\"ok\":true,\"deck\":%s}", o && *o == '{' ? o : "null"); free(o);
		reply_json(r->fd, &s); return;
	}
	if (!strcmp(p, "display")) {
		/* graphics path: "soft" (software display), "games" (Xorg on the graphics card; the desktop
		   still renders in software) or "full" (Xorg, and the desktop also uses the graphics card).
		   Takes effect when Rudolf OS restarts. */
		char *m = qparam(r->query, "mode");
		if (m) {
			if (!post) { reply_err(r->fd, 400, "use POST"); return; }
			if (strcmp(m, "soft") && strcmp(m, "games") && strcmp(m, "full")) { reply_err(r->fd, 400, "bad mode"); return; }
			char b[16]; snprintf(b, sizeof b, "%s\n", m);
			write_str("/etc/rudolf-display", b);
			if (!live) { mkdir(PERSIST "/etc", 0755); write_str(PERSIST "/etc/rudolf-display", b); }
			reply_ok(r->fd, live ? "saved until Rudolf OS restarts (not installed, so it is not kept)" : "saved; restart Rudolf OS to use it"); return;
		}
		str s = {0};
		char *want = slurp("/etc/rudolf-display", NULL), *cur = slurp(RUN "/display-mode", NULL), *why = slurp(RUN "/display-reason", NULL), *gpu = slurp(RUN "/gpu", NULL);
		char *strip[] = { want, cur, gpu };
		for (int i = 0; i < 3; i++) if (strip[i]) { char *nl = strchr(strip[i], '\n'); if (nl) *nl = 0; }
		sadd(&s, "{\"ok\":true,\"setting\":"); sjson(&s, want && *want ? want : "games");
		sadd(&s, ",\"active\":"); sjson(&s, cur ? cur : "soft");
		sadd(&s, ",\"reason\":"); sjson(&s, why ? why : "");
		sadd(&s, ",\"gpu\":"); sjson(&s, gpu ? gpu : "");
		sadd(&s, ",\"xorg\":%s}", access("/usr/bin/Xorg", X_OK) ? "false" : "true");
		free(want); free(cur); free(why); free(gpu);
		reply_json(r->fd, &s); return;
	}
	if (!strncmp(p, "screen", 6)) {
		/* Settings > Display: resolution and frame rate (/usr/lib/rudolf/screenctl) */
		const char *a = p + 6; char cmd[200];
		if (!*a && !post) { char *o = run_out("/usr/lib/rudolf/screenctl status 2>/dev/null"); if (!o || o[0] != '{') { free(o); reply_err(r->fd, 500, "the screen settings could not be read"); return; } reply(r->fd, 200, "application/json", o, strlen(o), NULL); free(o); return; }
		if (!post) { reply_err(r->fd, 400, "use POST"); return; }
		if (!*a) {
			const char *res = qparam(r->query, "res"), *fps = qparam(r->query, "fps");
			if (!safe_word(res, 12) || !safe_word(fps, 4)) { reply_err(r->fd, 400, "resolution and frame rate?"); return; }
			snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/screenctl set %s %s", res, fps);
			if (run(cmd)) { reply_err(r->fd, 400, "that resolution or frame rate is not offered"); return; }
			reply_ok(r->fd, "the display starts again with the new setting"); return;
		}
		if (!strcmp(a, "/keep")) { run("/usr/lib/rudolf/screenctl keep"); reply_ok(r->fd, "kept"); return; }
		if (!strcmp(a, "/revert")) { run("/usr/lib/rudolf/screenctl revert"); reply_ok(r->fd, "the previous setting comes back"); return; }
		reply_err(r->fd, 404, "unknown screen action"); return;
	}
	if (!strncmp(p, "idbm/", 5)) { api_idbm(r, p + 5, post); return; }
	if (!strncmp(p, "dl/", 3)) { api_dl(r, p + 3, post); return; }
	if (!strncmp(p, "secureboot/", 11)) { api_secureboot(r, p + 11, post); return; }
	if (!strcmp(p, "bootmenu")) { api_bootmenu(r, "", post); return; }
	if (!strncmp(p, "bootmenu/", 9)) { api_bootmenu(r, p + 9, post); return; }
	if (!strcmp(p, "shell") || !strncmp(p, "shell/", 6)) { api_shell(r, p + 5, post); return; }
	if (!strcmp(p, "camera")) { api_camera(r, post); return; }
	if (!strcmp(p, "clipboard")) { api_clipboard(r); return; }
	if (!strcmp(p, "web/status")) { api_web(r, "status"); return; }   /* the taskbar asks whether the browser window is open */
	if (!strcmp(p, "wifi/status")) { api_wifi(r, "status"); return; }
	if (!post) { reply_err(r->fd, 400, "use POST"); return; }
	/* ---- actions ---- */
	if (!strcmp(p, "proc/end")) { api_proc_end(r); return; }
	if (!strcmp(p, "service/restart")) { api_service(r); return; }
	if (!strcmp(p, "power")) {
		char *a = qparam(r->query, "action");
		if (!a) { reply_err(r->fd, 400, "action?"); return; }
		if (!strcmp(a, "suspend")) {
			/* wake=N (1-60, 1.2.5's hardware check): the real-time clock wakes the computer after N seconds */
			char *w = qparam(r->query, "wake"); int ws = w ? atoi(w) : 0;
			if (ws > 0 && ws <= 60) { char b[24]; write_str("/sys/class/rtc/rtc0/wakealarm", "0"); snprintf(b, sizeof b, "+%d", ws); if (write_str("/sys/class/rtc/rtc0/wakealarm", b)) { reply_err(r->fd, 409, "this computer cannot wake itself (no wake alarm)"); return; } }
			reply_ok(r->fd, "suspending"); sync(); write_str("/sys/power/state", "mem"); return;
		}
		if (strcmp(a, "poweroff") && strcmp(a, "reboot")) { reply_err(r->fd, 400, "unknown action"); return; }
		reply_ok(r->fd, a);
		write_str(RUN "/power", a);
		/* let Chromium shut down cleanly so it writes local storage to disk (up to 25 s) */
		run("pkill -TERM -f 'chrom[e].*--rudolf-kiosk'");
		for (int i = 0; i < 50 && !run("pgrep -f 'chrom[e].*--rudolf-kiosk' >/dev/null 2>&1"); i++) usleep(500000);
		run("pkill -KILL -f '/opt/chromium/chrome' >/dev/null 2>&1");
		/* a RUDOLFDATA partition made during this live session (1.2.4): the session's files move onto it now */
		if (!access(RUN "/keepfiles-pending", F_OK)) run("/usr/lib/rudolf/keepfiles.sh finish >" RUN "/keepfiles.log 2>&1");
		sync();
		run("umount -a -r 2>/dev/null");
		reboot(!strcmp(a, "reboot") ? RB_AUTOBOOT : RB_POWER_OFF);
		return;
	}
	if (!strncmp(p, "native/", 7)) { api_native(r, p + 7); return; }
	if (!strncmp(p, "pkg/", 4)) { api_pkg(r, p + 4); return; }
	if (!strncmp(p, "print/", 6)) { api_print(r, p + 6); return; }
	if (!strncmp(p, "apps/", 5)) { api_apps(r, p + 5); return; }
	if (!strncmp(p, "debian/", 7)) { api_debian(r, p + 7); return; }
	if (!strncmp(p, "ai/", 3)) { api_ai(r, p + 3); return; }
	if (!strncmp(p, "games/", 6)) { api_games(r, p + 6); return; }
	if (!strncmp(p, "term/", 5)) { api_term(r, p + 5); return; }
	if (!strncmp(p, "portal/", 7)) { api_portal(r, p + 7); return; }
	if (!strncmp(p, "web/", 4)) { api_web(r, p + 4); return; }
	if (!strncmp(p, "sys/", 4)) {
		const char *w = p + 4; char cmd[700];
		if (access("/usr/bin/rudolf-update", X_OK)) { reply_err(r->fd, 404, "this Rudolf OS has no system updates"); return; }
		if (!strcmp(w, "check") || !strcmp(w, "download")) {
			if (job_running()) { reply_err(r->fd, 409, "another task is still running"); return; }
			snprintf(cmd, sizeof cmd, "/usr/bin/rudolf-update %s", w); start_job("system-update", cmd); reply_ok(r->fd, "started"); return;
		}
		if (!strcmp(w, "rollback")) { char *o = run_out("/usr/bin/rudolf-update rollback 2>&1"); reply_ok(r->fd, o); free(o); return; }
		if (!strcmp(w, "set")) {
			const char *k = qparam(r->query, "k"); char kk[8]; snprintf(kk, sizeof kk, "%s", k ? k : "");
			const char *v = qparam(r->query, "v"); if (!v) v = "";
			if (strcmp(kk, "url") && strcmp(kk, "auto")) { reply_err(r->fd, 400, "k is url or auto"); return; }
			for (const char *c = v; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("/:._-~%+", *c)) { reply_err(r->fd, 400, "bad character"); return; }
			snprintf(cmd, sizeof cmd, "/usr/bin/rudolf-update set %s '%s' 2>&1", kk, v);
			char *o = run_out(cmd); if (o && *o) { reply_err(r->fd, 400, o); free(o); return; } free(o); reply_ok(r->fd, "saved"); return;
		}
		reply_err(r->fd, 404, "unknown update action"); return;
	}
	if (!strcmp(p, "lsync")) {
		if (r->blen < 2 || r->blen > (16u << 20) || r->body[0] != '{' || r->body[r->blen - 1] != '}') { reply_err(r->fd, 400, "bad data"); return; }
		int f = open(LSFILE ".tmp", O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (f < 0 || write(f, r->body, r->blen) != (ssize_t)r->blen || fsync(f)) { if (f >= 0) close(f); unlink(LSFILE ".tmp"); reply_err(r->fd, 500, "could not save"); return; }
		close(f);
		if (rename(LSFILE ".tmp", LSFILE)) { reply_err(r->fd, 500, "could not save"); return; }
		reply_ok(r->fd, "saved"); return;
	}
	if (!strcmp(p, "clientlog")) {   /* desktop errors, kept for the Event Viewer and diagnostics */
		struct stat st;
		if (!stat(RUN "/page.log", &st) && st.st_size > 262144) rename(RUN "/page.log", RUN "/page.log.1");
		int f = open(RUN "/page.log", O_WRONLY | O_CREAT | O_APPEND, 0600);
		if (f >= 0) { size_t n = r->blen > 4096 ? 4096 : r->blen; if (n && write(f, r->body, n) < 0) {} if (write(f, "\n", 1) < 0) {} close(f); }
		reply_ok(r->fd, "ok"); return;
	}
	/* 1.4.4: the system volume (the sound server's default output, through /usr/lib/rudolf/hotkey, as the keys do):
	   GET volume; POST volume?set=N (0-100), ?muted=0|1 or ?step=up|down|mute */
	if (!strcmp(p, "volume")) {
		char cmd[96]; const char *v;
		if (!post) snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/hotkey volume get 2>/dev/null");
		else if ((v = qparam(r->query, "set")) && *v && strspn(v, "0123456789") == strlen(v) && strlen(v) <= 3) snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/hotkey volume set %s 2>/dev/null", v);
		else if ((v = qparam(r->query, "muted")) && (!strcmp(v, "0") || !strcmp(v, "1"))) snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/hotkey volume muted %s 2>/dev/null", v);
		else if ((v = qparam(r->query, "step")) && (!strcmp(v, "up") || !strcmp(v, "down") || !strcmp(v, "mute"))) snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/hotkey volume %s >/dev/null 2>&1; /usr/lib/rudolf/hotkey volume get 2>/dev/null", v);
		else { reply_err(r->fd, 400, "set=0-100, muted=0|1 or step=up|down|mute"); return; }
		char *o = run_out(cmd); trim(o); int lv = -1; char mu[8] = "";
		if (sscanf(o, "%d %7s", &lv, mu) < 1) { free(o); reply_err(r->fd, 503, "the sound server is not running"); return; }
		str js = {0}; sadd(&js, "{\"ok\":true,\"volume\":%d,\"muted\":%s}", lv, strcmp(mu, "muted") ? "false" : "true");
		free(o); reply_json(r->fd, &js); return;
	}
	/* 1.4.4: brightness/step?d=up|down (the mouse wheel over the taskbar's brightness icon), as the keys */
	if (!strcmp(p, "brightness/step")) {
		const char *d = qparam(r->query, "d");
		if (!post || !d || (strcmp(d, "up") && strcmp(d, "down"))) { reply_err(r->fd, 400, "POST with d=up or down"); return; }
		char cmd[96]; snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/hotkey brightness %s 2>/dev/null", d);
		char *o = run_out(cmd); trim(o); int lv = atoi(o);
		if (!strcmp(o, "none") || !*o) { free(o); reply_err(r->fd, 404, "this screen has no adjustable backlight"); return; }
		str js = {0}; sadd(&js, "{\"ok\":true,\"brightness\":%d}", lv); free(o); reply_json(r->fd, &js); return;
	}
	/* 1.4.4: idle: milliseconds since the last input of any kind, from any device (rudolf-display), so the screen saver
	   and the screen-off time count input in games and Linux apps too */
	if (!strcmp(p, "idle")) {
		long long last = read_ll(RUN "/input-ms", -1); struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
		long long now = t.tv_sec * 1000LL + t.tv_nsec / 1000000;
		str js = {0}; if (last < 0) sadd(&js, "{\"ok\":true,\"idle\":null}"); else sadd(&js, "{\"ok\":true,\"idle\":%lld}", now > last ? now - last : 0);
		reply_json(r->fd, &js); return;
	}
	if (!strcmp(p, "brightness")) {
		char bl[256], f[300];
		char *v = qparam(r->query, "value");
		if (!backlight_dir(bl, sizeof bl)) { reply_err(r->fd, 404, "this screen has no adjustable backlight"); return; }
		if (!v) {   /* 1.4.4: GET brightness: the level now, 0-100 (the taskbar's brightness icon) */
			snprintf(f, sizeof f, "%s/max_brightness", bl); long long mx = read_ll(f, 1);
			snprintf(f, sizeof f, "%s/brightness", bl); long long cur = read_ll(f, 0);
			str js = {0}; sadd(&js, "{\"ok\":true,\"brightness\":%lld}", mx > 0 ? (cur * 100 + mx / 2) / mx : 0); reply_json(r->fd, &js); return;
		}
		snprintf(f, sizeof f, "%s/max_brightness", bl);
		long long mx = read_ll(f, 100), val = atoll(v) * mx / 100;
		if (val < mx / 20) val = mx / 20;
		char b[32]; snprintf(b, sizeof b, "%lld", val);
		snprintf(f, sizeof f, "%s/brightness", bl);
		if (write_str(f, b)) reply_err(r->fd, 500, "could not set brightness"); else reply_ok(r->fd, "ok");
		return;
	}
	if (!strcmp(p, "net/dhcp")) {
		char *i = qparam(r->query, "if");
		if (!safe_word(i, 16)) { reply_err(r->fd, 400, "bad interface"); return; }
		char cmd[256]; snprintf(cmd, sizeof cmd, "ip link set %s up; udhcpc -i %s -n -q -t 5 -s /usr/lib/rudolf/udhcpc.sh >/dev/null 2>&1", i, i);
		if (run(cmd)) reply_err(r->fd, 500, "no DHCP answer on this network"); else reply_ok(r->fd, "connected");
		return;
	}
	if (!strcmp(p, "net/static")) {
		char i[20] = "", a[64] = "", g[64] = "", d[64] = "";
		char *x;
		if ((x = qparam(r->query, "if"))) snprintf(i, sizeof i, "%s", x);
		if ((x = qparam(r->query, "addr"))) snprintf(a, sizeof a, "%s", x);
		if ((x = qparam(r->query, "gw"))) snprintf(g, sizeof g, "%s", x);
		if ((x = qparam(r->query, "dns"))) snprintf(d, sizeof d, "%s", x);
		if (!safe_word(i, 16) || !*a || strspn(a, "0123456789./") != strlen(a) || strspn(g, "0123456789.") != strlen(g) || strspn(d, "0123456789.") != strlen(d)) { reply_err(r->fd, 400, "bad address"); return; }
		char cmd[512];
		snprintf(cmd, sizeof cmd, "ip addr flush dev %s && ip addr add %s dev %s && ip link set %s up%s%s%s", i, a, i, i, *g ? " && ip route replace default via " : "", g, "");
		int rc = run(cmd);
		if (*d) { char rs[96]; snprintf(rs, sizeof rs, "nameserver %s\n", d); write_str("/etc/resolv.conf", rs); }
		if (rc) reply_err(r->fd, 500, "could not apply the address"); else reply_ok(r->fd, "applied");
		return;
	}
	if (!strncmp(p, "wifi/", 5)) { api_wifi(r, p + 5); return; }
	if (!strcmp(p, "keyboard")) {
		char *l = qparam(r->query, "layout"), *v = qparam(r->query, "variant");
		if (!safe_word(l, 16) || (v && *v && !safe_word(v, 24))) { reply_err(r->fd, 400, "bad layout"); return; }
		char cmd[200]; snprintf(cmd, sizeof cmd, "DISPLAY=:0 setxkbmap -layout %s%s%s", l, v && *v ? " -variant " : "", v && *v ? v : "");
		if (run(cmd)) reply_err(r->fd, 500, "unknown layout"); else reply_ok(r->fd, "layout set");
		return;
	}
	if (!strcmp(p, "pointer")) {
		char *sp = qparam(r->query, "speed"); double s = sp ? atof(sp) : 1;
		char *ac = qparam(r->query, "accel"); double a = ac ? atof(ac) : 1;
		/* touchpad: tap to click, natural (reversed) scrolling, ignore the touchpad while typing */
		char *tp = qparam(r->query, "tap"), *na = qparam(r->query, "natural"), *ty = qparam(r->query, "typing");
		int tap = tp ? atoi(tp) != 0 : 1, nat = na ? atoi(na) != 0 : 0, typ = ty ? atoi(ty) != 0 : 1;
		if (s < 0.2) s = 0.2; if (s > 5) s = 5; if (a < 1) a = 1; if (a > 3) a = 3;
		char b[64]; snprintf(b, sizeof b, "%.3f %.3f %d %d %d\n", s, a, tap, nat, typ); write_str(RUN "/pointer", b);
		reply_ok(r->fd, "ok"); return;
	}
	if (!strcmp(p, "audio")) {
		char *c = qparam(r->query, "card");
		if (c) {
			if (strspn(c, "0123456789") != strlen(c) || !*c) { reply_err(r->fd, 400, "bad card"); return; }
			char b[128]; snprintf(b, sizeof b, "defaults.pcm.card %s\ndefaults.ctl.card %s\n", c, c);
			write_str("/etc/asound.conf", b);
			mkdir(PERSIST "/etc", 0755);
			if (!live) write_str(PERSIST "/etc/asound.conf", b);
			/* with the sound server running, switch its default output; otherwise the browser
			   must restart to pick up the new ALSA default */
			char cmd[96]; snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/sound default %s >/dev/null 2>&1", c);
			if (!access("/run/user/1000/pulse/native", F_OK) && !run(cmd)) { reply_ok(r->fd, "sound card selected"); return; }
			restart_browser();
			reply_ok(r->fd, "sound card selected; Rudolf OS restarts its browser"); return;
		}
		char *cards = slurp("/proc/asound/cards", NULL);
		str s = {0}; sadd(&s, "{\"ok\":true,\"cards\":"); sjson(&s, cards ? cards : ""); sadd(&s, "}"); free(cards); reply_json(r->fd, &s); return;
	}
	if (!strcmp(p, "proxy")) {
		char *sv = qparam(r->query, "server");
		if (sv && *sv && (strlen(sv) > 200 || strpbrk(sv, " ;'\"`$\\|&<>"))) { reply_err(r->fd, 400, "bad proxy"); return; }
		write_str(RUN "/proxy", sv ? sv : "");
		if (!live) write_str(PERSIST "/proxy", sv ? sv : "");
		restart_browser(); reply_ok(r->fd, "proxy saved; the browser restarts"); return;
	}
	if (!strcmp(p, "time")) {
		char *e = qparam(r->query, "epoch");
		if (!e) { reply_err(r->fd, 400, "epoch?"); return; }
		struct timeval tv = { atoll(e) / 1000, (atoll(e) % 1000) * 1000 };
		if (settimeofday(&tv, NULL)) { reply_err(r->fd, 500, "could not set the clock"); return; }
		run("hwclock -w -u >/dev/null 2>&1"); reply_ok(r->fd, "clock set"); return;
	}
	if (!strcmp(p, "update")) {
		if (live) { reply_err(r->fd, 400, "updates need an installed system or a data partition"); return; }
		if (r->blen < 100000 || !memmem(r->body, r->blen > 4096 ? 4096 : r->blen, "<html", 5)) { reply_err(r->fd, 400, "this is not a Rudolf OS page"); return; }
		run("mkdir -p " PERSIST "/os/new && rm -rf " PERSIST "/os/previous && [ -d " PERSIST "/os/current ] && mv " PERSIST "/os/current " PERSIST "/os/previous; true");
		int f = open(PERSIST "/os/new/index.html", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (f < 0 || write(f, r->body, r->blen) != (ssize_t)r->blen) { if (f >= 0) close(f); reply_err(r->fd, 500, "could not save the update"); return; }
		fsync(f); close(f);
		run("mv " PERSIST "/os/new " PERSIST "/os/current; rm -f " PERSIST "/os/disabled; sync");
		reply_ok(r->fd, "update installed; restart to use it"); return;
	}
	if (!strcmp(p, "rollback")) {
		int rc = run("[ -d " PERSIST "/os/previous ] && rm -rf " PERSIST "/os/current && mv " PERSIST "/os/previous " PERSIST "/os/current || { rm -rf " PERSIST "/os/current; true; }; sync");
		reply_ok(r->fd, rc ? "rolled back" : "returned to the previous version"); return;
	}
	if (!strcmp(p, "say")) {
		char *t = qparam(r->query, "text"), *l = qparam(r->query, "lang");
		char lang[16] = "en";
		if (l && safe_word(l, 12)) snprintf(lang, sizeof lang, "%s", l);
		if (!t || access("/usr/bin/espeak-ng", X_OK)) { reply_err(r->fd, 404, "no speech engine"); return; }
		write_str(RUN "/say.txt", t);
		char cmd[200]; snprintf(cmd, sizeof cmd, "espeak-ng -v %s -s 170 -f " RUN "/say.txt --stdout 2>/dev/null", lang);
		FILE *pp = popen(cmd, "r");
		if (!pp) { reply_err(r->fd, 500, "speech failed"); return; }
		str w = {0}; char buf[8192]; size_t n;
		while ((n = fread(buf, 1, sizeof buf, pp)) > 0) { if (!w.b) { w.cap = 65536; w.b = malloc(w.cap); w.n = 0; } if (w.n + n > w.cap) { w.cap = (w.n + n) * 2; w.b = realloc(w.b, w.cap); } memcpy(w.b + w.n, buf, n); w.n += n; }
		pclose(pp);
		reply(r->fd, 200, "audio/wav", w.b, w.n, NULL); free(w.b); return;
	}
	if (!strcmp(p, "time/sync")) {
		if (run("ntpd -n -q -p pool.ntp.org >/dev/null 2>&1 || ntpd -n -q -p time.google.com >/dev/null 2>&1")) { reply_err(r->fd, 500, "no time server answered (offline?)"); return; }
		run("hwclock -w -u >/dev/null 2>&1"); reply_ok(r->fd, "clock set from the internet"); return;
	}
	if (!strcmp(p, "restart-browser")) { reply_ok(r->fd, "restarting"); restart_browser(); return; }
	if (!strcmp(p, "factory-reset")) {
		if (live) { reply_err(r->fd, 400, "nothing is saved in live mode"); return; }
		write_str(PERSIST "/.factory-reset", "1"); sync();
		reply_ok(r->fd, "all saved data will be erased at the next start"); return;
	}
	if (!strcmp(p, "install")) {
		char *d = qparam(r->query, "disk"), *c = qparam(r->query, "confirm");
		if (!safe_word(d, 16) || !c || strcmp(c, d)) { reply_err(r->fd, 400, "confirm by sending the disk name twice"); return; }
		if (job_running()) { reply_err(r->fd, 409, "another job is running"); return; }
		/* image (1.2.4): a downloaded disc image in the home folder to install instead of the running drive's */
		char *img = qparam(r->query, "image");
		if (img && *img) {
			int okp = !strncmp(img, "/home/rudolf/", 13) && !strstr(img, "..") && strlen(img) < 200;
			for (const char *q = img; okp && *q; q++) if (!isalnum((unsigned char)*q) && !strchr("/._-", *q)) okp = 0;
			if (!okp) { reply_err(r->fd, 400, "the image must be a file in the home folder (letters, digits, . _ - only)"); return; }
		}
		/* 1.2.8: bootonly=1 replaces only the boot files in front of the disk's RUDOLFDATA, keeping it (bootfiles.sh) */
		char *bo = qparam(r->query, "bootonly");
		if (bo && !strcmp(bo, "1")) {
			char cmd[600]; snprintf(cmd, sizeof cmd, "%s/usr/lib/rudolf/bootfiles.sh write /dev/%s '%s'", img_sha_env(r), d, img && *img ? img : "");
			start_job("bootfiles", cmd); reply_ok(r->fd, "writing the boot files"); return;
		}
		char cmd[600]; snprintf(cmd, sizeof cmd, "%s/usr/lib/rudolf/install.sh /dev/%s%s%s", img_sha_env(r), d, img && *img ? " " : "", img && *img ? img : "");
		start_job("install", cmd); reply_ok(r->fd, "installing"); return;
	}
	/* new boot files for the drive Rudolf OS runs from (1.2.8): checked and kept on RUDOLFDATA, written by the start-up
	   at the next start (bootfiles.sh stage); image: a downloaded disc image or boot image in the home folder */
	if (!strcmp(p, "bootfiles")) {
		if (live) { reply_err(r->fd, 400, "Rudolf OS is not keeping files on this drive"); return; }
		if (job_running()) { reply_err(r->fd, 409, "another job is running"); return; }
		char *img = qparam(r->query, "image");
		int okp = img && !strncmp(img, "/home/rudolf/", 13) && !strstr(img, "..") && strlen(img) < 200;
		for (const char *q = img; okp && *q; q++) if (!isalnum((unsigned char)*q) && !strchr("/._-", *q)) okp = 0;
		if (!okp) { reply_err(r->fd, 400, "the image must be a file in the home folder (letters, digits, . _ - only)"); return; }
		char cmd[600]; snprintf(cmd, sizeof cmd, "%s/usr/lib/rudolf/bootfiles.sh stage '%s'", img_sha_env(r), img);
		start_job("bootfiles", cmd); reply_ok(r->fd, "checking the image"); return;
	}
	/* make a Rudolf OS USB stick (1.2.5, mkstick.sh): disk and confirm (the same name twice), image (empty for this
	   Rudolf OS, or a downloaded image in the home folder), files=1 to copy this session's files and settings too */
	if (!strcmp(p, "mkstick")) {
		char *d = qparam(r->query, "disk"), *c = qparam(r->query, "confirm"), *img = qparam(r->query, "image"), *fl = qparam(r->query, "files");
		if (!safe_word(d, 16) || !c || strcmp(c, d)) { reply_err(r->fd, 400, "confirm by sending the drive name twice"); return; }
		if (job_running()) { reply_err(r->fd, 409, "another job is running"); return; }
		if (img && *img) {
			int okp = !strncmp(img, "/home/rudolf/", 13) && !strstr(img, "..") && strlen(img) < 200;
			for (const char *q = img; okp && *q; q++) if (!isalnum((unsigned char)*q) && !strchr("/._-", *q)) okp = 0;
			if (!okp) { reply_err(r->fd, 400, "the image must be a file in the home folder (letters, digits, . _ - only)"); return; }
		}
		char cmd[700]; snprintf(cmd, sizeof cmd, "%s/usr/lib/rudolf/mkstick.sh /dev/%s '%s' %d", img_sha_env(r), d, img ? img : "", fl && !strcmp(fl, "1"));
		start_job("mkstick", cmd); reply_ok(r->fd, "making the stick"); return;
	}
	/* keep files on the drive Rudolf OS runs from (1.2.4): add RUDOLFDATA after the image (keepfiles.sh) */
	if (!strcmp(p, "keepfiles")) {
		if (job_running()) { reply_err(r->fd, 409, "another job is running"); return; }
		start_job("keepfiles", "/usr/lib/rudolf/keepfiles.sh make"); reply_ok(r->fd, "adding the RUDOLFDATA partition"); return;
	}
	if (!strcmp(p, "mount")) { api_mount(r, 1); return; }
	if (!strcmp(p, "umount")) { api_mount(r, 0); return; }
	/* plugged-in devices (Settings > Devices) and "Eject" for a USB drive or SD card */
	if (!strcmp(p, "eject")) {
		if (!post) { reply_err(r->fd, 400, "use POST"); return; }
		char *dev = qparam(r->query, "dev");
		if (!safe_word(dev, 32) || strchr(dev, '.') || strchr(dev, ':')) { reply_err(r->fd, 400, "bad device"); return; }
		char cmd[160]; snprintf(cmd, sizeof cmd, "/usr/lib/rudolf/devplug eject %s 2>" RUN "/eject.err", dev);
		if (run(cmd)) { char *e = slurp(RUN "/eject.err", NULL); if (e) trim(e); reply_err(r->fd, 409, e && *e ? e : "could not eject it"); free(e); }
		else reply_ok(r->fd, "safe to remove");
		return;
	}
	reply_err(r->fd, 404, "unknown API");
}

static void handle(int fd)
{
	struct req r;
	memset(&r, 0, sizeof r);
	r.fd = fd;
	char hdr[16384];
	size_t n = 0;
	char *end = NULL;
	struct pollfd pf = { fd, POLLIN, 0 };
	while (n < sizeof hdr - 1) {
		if (poll(&pf, 1, 10000) <= 0) return;
		ssize_t k = read(fd, hdr + n, sizeof hdr - 1 - n);
		if (k <= 0) return;
		n += k; hdr[n] = 0;
		if ((end = strstr(hdr, "\r\n\r\n"))) break;
	}
	if (!end) return;
	*end = 0;
	char *body0 = end + 4;
	size_t have = n - (body0 - hdr);
	char url[4096] = "";
	if (sscanf(hdr, "%7s %4095s", r.method, url) != 2) return;
	char *q = strchr(url, '?');
	if (q) { *q = 0; snprintf(r.query, sizeof r.query, "%s", q + 1); }
	snprintf(r.path, sizeof r.path, "%s", url);
	size_t clen = 0;
	for (char *l = strstr(hdr, "\r\n"); l; l = strstr(l + 2, "\r\n")) {
		char *h = l + 2;
		if (!strncasecmp(h, "Content-Length:", 15)) clen = strtoul(h + 15, NULL, 10);
		else if (!strncasecmp(h, "X-Rudolf-Token:", 15)) { char *v = h + 15; while (*v == ' ') v++; snprintf(r.tok, sizeof r.tok, "%.*s", (int)strcspn(v, "\r"), v); }
		else if (!strncasecmp(h, "Range:", 6)) { char *v = h + 6; while (*v == ' ') v++; snprintf(r.range, sizeof r.range, "%.*s", (int)strcspn(v, "\r"), v); }
	}
	if (clen > MAXBODY) { reply_err(fd, 413, "too large"); return; }
	if (clen) {
		r.body = malloc(clen + 1);
		memcpy(r.body, body0, have > clen ? clen : have);
		r.blen = have > clen ? clen : have;
		while (r.blen < clen) {
			if (poll(&pf, 1, 30000) <= 0) break;
			ssize_t k = read(fd, r.body + r.blen, clen - r.blen);
			if (k <= 0) break;
			r.blen += k;
		}
		r.body[r.blen] = 0;
	}
	if (strstr(r.path, "..")) { reply_err(fd, 400, "bad path"); goto out; }
	if (!strncmp(r.path, "/api/", 5)) {
		if (strcmp(r.tok, token)) { reply_err(fd, 403, "missing token"); goto out; }
		api(&r);
		goto out;
	}
	if (!strncmp(r.path, "/dlf/", 5)) {
		const char *id = r.path + 5; char lf[128];
		if (strlen(id) != 32 || strspn(id, "0123456789abcdef") != 32) { reply_err(fd, 404, "not found"); goto out; }
		snprintf(lf, sizeof lf, RUN "/dlf/%s", id);
		char *p = slurp(lf, NULL); if (!p) { reply_err(fd, 404, "not found"); goto out; }
		trim(p); serve_file_as(&r, p, 1); free(p); goto out;
	}
	char root[256];
	www_root(root, sizeof root);
	if (!strcmp(r.path, "/") || !strcmp(r.path, "/index.html")) { serve_index(&r, root); goto out; }
	char fp[2400];
	if (!strncmp(r.path, "/pkg/", 5)) {
		/* web-app packages: only files under www/ (or web/ for add-ons) of a mounted package */
		snprintf(fp, sizeof fp, "%s", r.path); urldecode(fp);
		const char *sl = strchr(fp + 5, '/');
		if (strstr(fp, "..") || !sl || (strncmp(sl, "/www/", 5) && strncmp(sl, "/web/", 5))) { reply_err(fd, 404, "not found"); goto out; }
		serve_file(&r, fp); goto out;
	}
	snprintf(fp, sizeof fp, "%s%s", root, r.path);
	urldecode(fp);
	if (access(fp, R_OK)) snprintf(fp, sizeof fp, WWW "%s", r.path), urldecode(fp);
	if (strstr(fp, "..")) { reply_err(fd, 400, "bad path"); goto out; }
	serve_file(&r, fp);
out:
	free(r.body);
}

/* rudolfd --addpart DISK N START SECTORS (1.2.4, keepfiles.sh): tell the kernel about one new partition of a disk
   whose other partitions are in use (the running system), which re-reading the whole table cannot do */
static int addpart(int argc, char **argv)
{
	if (argc != 6) { fprintf(stderr, "usage: rudolfd --addpart DISK N START SECTORS\n"); return 2; }
	int fd = open(argv[2], O_RDONLY | O_CLOEXEC);
	if (fd < 0) { perror(argv[2]); return 1; }
	struct blkpg_partition bp = {0};
	bp.pno = atoi(argv[3]); bp.start = atoll(argv[4]) * 512; bp.length = atoll(argv[5]) * 512;
	struct blkpg_ioctl_arg a = {.op = BLKPG_ADD_PARTITION, .datalen = sizeof bp, .data = &bp};
	int rc = ioctl(fd, BLKPG, &a);
	if (rc) perror("BLKPG_ADD_PARTITION");
	close(fd);
	return rc ? 1 : 0;
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "--addpart")) return addpart(argc, argv);
	signal(SIGPIPE, SIG_IGN);
	mkdir(RUN, 0755);
	int rf = open("/dev/urandom", O_RDONLY);
	unsigned char rb[16];
	if (rf < 0 || read(rf, rb, 16) != 16) { for (int i = 0; i < 16; i++) rb[i] = rand(); }
	if (rf >= 0) close(rf);
	for (int i = 0; i < 16; i++) sprintf(token + i * 2, "%02x", rb[i]);
	char *cl = (access("/etc/rudolf/cmdline", F_OK) ? slurp("/proc/cmdline", NULL) : slurp("/etc/rudolf/cmdline", NULL));
	if (cl) {
		if (strstr(cl, "rudolf.recovery=1")) recovery = 1;
		if (strstr(cl, "rudolf.media=disk")) snprintf(media_mode, sizeof media_mode, "disk");
		free(cl);
	}
	/* what the system started from, by the device the initramfs found it on (1.2.3): a disc image written to a
	   USB stick is not a disc; a UEFI start says nothing about it on the start-up options */
	{ char *md = slurp("/etc/rudolf/media", NULL);
	  if (md && !strncmp(md, "/dev/", 5)) {
		char *nl = strchr(md, '\n'); if (nl) *nl = 0;
		char sp[300], rp[4096]; snprintf(sp, sizeof sp, "/sys/class/block/%s", md + 5);
		if (!strncmp(md + 5, "sr", 2)) snprintf(media_mode, sizeof media_mode, "cd");
		else if (realpath(sp, rp)) snprintf(media_mode, sizeof media_mode, "%s", strstr(rp, "/usb") ? "usb" : strstr(rp, "/mmc") ? "sd" : "disk");
	  }
	  free(md); }
	if (!access("/sys/firmware/efi", F_OK)) snprintf(boot_mode, sizeof boot_mode, "uefi");
	{ char *sb = slurp("/etc/rudolf/secureboot", NULL); secureboot = sb && sb[0] == '1'; free(sb); }
	live = access(RUN "/persistent", F_OK) != 0;
	dl_boot();
	boot_entry_ensure();
	int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0), one = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(8080), .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
	if (bind(s, (struct sockaddr *)&a, sizeof a) || listen(s, 64)) { perror("rudolfd: bind 127.0.0.1:8080"); return 1; }
	fprintf(stderr, "rudolfd: serving Rudolf OS on http://127.0.0.1:8080/ (%s, %s)\n", live ? "live" : "persistent", boot_mode);
	for (;;) {
		struct pollfd pf = { s, POLLIN, 0 };
		reap();
		if (poll(&pf, 1, 1000) <= 0) continue;
		int c = accept4(s, NULL, NULL, SOCK_CLOEXEC);
		if (c < 0) continue;
		setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
		/* each connection in its own process so a slow download never blocks the desktop */
		pid_t p = fork();
		if (p == 0) { close(s); handle(c); shutdown(c, SHUT_WR); close(c); _exit(0); }
		close(c);
		/* state that must survive in the parent */
	}
}
