/* Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 34, step 4): the network scanners Find scanners found (rudolf-scan find's
 * JSON) as sane-airscan's settings, for Linux apps (Simple Scan in the Debian sandbox): each eSCL or WSD scanner on the
 * network by its address ("NAME" = URL, escl|wsd), sane-airscan's own discovery off (it needs Avahi, which neither
 * Rudolf OS nor its Debian runs, and WS-Discovery's answers would not get through the firewall). USB scanners are not
 * in it: Linux apps do not get USB devices. Included by rudolfd (rudolfd-scan.inc, which writes the file) and
 * rudolf-scan (its "conf" command, for the tests). Names keep only letters, digits and " ._()-"; addresses only what
 * rudolf-scan's url_ok allows.
 * Copyright (C) 2026 the Rudolf OS authors. SPDX-License-Identifier: GPL-2.0-or-later */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
/* a JSON string's text from P (just after its opening quote) into OUT, escapes left out; P past its closing quote */
static void scanconf_str(const char **p, char *out, size_t on, int name)
{
	size_t k = 0; const char *s = *p;
	while (*s && *s != '"') {
		if (*s == '\\') { s++; if (*s == 'u') { for (int i = 0; i < 5 && *s; i++) s++; continue; } if (!*s) break; s++; continue; }
		unsigned char c = *s++;
		if (k + 1 < on && (isalnum(c) || (name ? strchr(" ._()-", c) != NULL : strchr(":/._-~%[]", c) != NULL))) out[k++] = c;
	}
	out[k] = 0; *p = *s ? s + 1 : s;
}
/* the settings into OUT (at most ON bytes); the number of scanners in it */
static int scanconf_make(const char *json, char *out, size_t on)
{
	int n = 0; size_t k = 0;
	k += snprintf(out + k, on - k, "# The network scanners Rudolf OS found (Scan, Find scanners): sane-airscan's settings for Linux apps.\n"
		"# Written by rudolfd; read only in the Debian sandbox.\n[options]\ndiscovery = disable\n[devices]\n");
	for (const char *p = json ? strstr(json, "{\"id\":\"") : NULL; p && k < on; p = strstr(p, "{\"id\":\"")) {
		p += 7; char id[400], nm[200] = "", via[16] = "";
		scanconf_str(&p, id, sizeof id, 0);
		const char *e = strchr(p, '}'); if (!e) break;
		const char *q = strstr(p, "\"name\":\""); if (q && q < e) { q += 8; scanconf_str(&q, nm, sizeof nm, 1); }
		q = strstr(p, "\"via\":\""); if (q && q < e) { q += 7; scanconf_str(&q, via, sizeof via, 1); }
		p = e;
		if (strcmp(via, "network")) continue;
		const char *kind = !strncmp(id, "escl:http", 9) ? "escl" : !strncmp(id, "wsd:http://", 11) ? "wsd" : NULL; if (!kind) continue;
		const char *url = strchr(id, ':') + 1;
		if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) continue;
		if (!*nm) snprintf(nm, sizeof nm, "Scanner %d", n + 1);
		int w = snprintf(out + k, on - k, "\"%s\" = %s, %s\n", nm, url, kind); if (w < 0 || (size_t)w >= on - k) break;
		k += w; n++;
	}
	return n;
}
