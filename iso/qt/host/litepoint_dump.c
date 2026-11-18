/* litepoint_dump: the native desktop's LitePoint scenes (iso/sys/litepoint.h), for iso/qt/shell2/litepoint60_t.js to compare with
   the page's (desktop/litepoint60.js). Reads "KIND T FX FY FW FH SX SY SW SH BX BY DIR CORNER" lines; writes one JSON line each.
   gcc -O2 -o litepoint_dump litepoint_dump.c -lm.  GPL-2.0-or-later. */
#include <stdio.h>
#include "../../sys/litepoint.h"
int main(void)
{
	char k[8]; double t; LPGeo g;
	while (scanf("%7s %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %d %d", k, &t, &g.f.x, &g.f.y, &g.f.w, &g.f.h, &g.s.x, &g.s.y, &g.s.w, &g.s.h, &g.bx, &g.by, &g.dir, &g.corner) == 14) {
		int kind = -1; for (int i = 0; i < LP_KINDS; i++) if (!strcmp(k, LP_NAME[i])) kind = i; if (kind < 0) return 1;
		LPScene o; lp_scene(kind, t, &g, &o);
		printf("{\"ms\":%d,\"geo\":\"%s\",\"lit\":%.4f,\"clip\":", LP_MS[kind], o.geo_to ? "to" : "from", o.lit);
		if (o.clip) printf("{\"x\":%.4f,\"y\":%.4f,\"w\":%.4f,\"h\":%.4f}", o.cr.x, o.cr.y, o.cr.w, o.cr.h); else printf("null");
		printf(",\"L\":["); for (int i = 0; i < o.nl; i++) printf("%s[%.4f,%.4f,%.4f,%.4f,%.4f]", i ? "," : "", o.L[i][0], o.L[i][1], o.L[i][2], o.L[i][3], o.L[i][4]);
		printf("],\"P\":["); for (int i = 0; i < o.np; i++) printf("%s[%.4f,%.4f,%.4f,%.4f]", i ? "," : "", o.P[i][0], o.P[i][1], o.P[i][2], o.P[i][3]);
		printf("]}\n"); fflush(stdout);
	}
	return 0;
}
