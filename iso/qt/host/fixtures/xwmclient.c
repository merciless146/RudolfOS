/* A stand-in X client for wmfocus_t.sh (1.5.2): the desktop (class RudolfDesktop), or an app window that behaves
 * like Wine's: WM_PROTOCOLS with WM_TAKE_FOCUS and WM_DELETE_WINDOW, input hint on. It logs what a window manager
 * sends it, one line each, to stdout: "id 0x..", "take_focus TIME", "focus_in", "geom X Y W H", "delete" (then it ends).
 *   xwmclient desktop | app | borderless W H (a Wine-style borderless window, then sized W x H) | resize ID W H |
 *             full ID 0|1|2 (full screen off/on/toggle) | focus ID | getfocus | wmstate ID (1 = NormalState)
 *             | stack (1.5.6: the viewable top-level windows, bottom to top) | inno (1.5.6: an app window, then a dialog
 *             of it, like Inno Setup's wizard: "id" then "dlg") | orphan (a dialog whose owner is never shown: "dlg")
 * 1.6.1 (docs/plan-1.6.1.md Part 15, csd161_t.sh): an app that draws its own title bar, as Electron's and GTK's do:
 *   csd W H [L R T B]   no decorations asked for (_MOTIF_WM_HINTS 0), GTK's margins L R T B (_GTK_FRAME_EXTENTS) if given;
 *                       its own bar (the top 30 px, drawn dark) asks _NET_WM_MOVERESIZE when pressed, as GTK does (the pointer
 *                       let go first), direction 8 (move) or the number in the file $XWM_DIR (0-7 the edges, 9 10 the keyboard's);
 *                       it logs "geom", "press X Y", "delete" as "app" does, and "state ATOMS" when _NET_WM_STATE changes
 *   max ID 0|1|2        _NET_WM_STATE_MAXIMIZED_VERT and _HORZ off, on, toggled (its maximize button)
 *   iconify ID          WM_CHANGE_STATE IconicState (its minimize button)   |   menu ID X Y   _GTK_SHOW_WINDOW_MENU
 *   props ID            "extents L R T B" (_NET_FRAME_EXTENTS) and "state ATOMS"   |   supported   _NET_SUPPORTED's names
 *   gtkext ID L R T B   GTK's margins changed   |   deco ID 0|1   decorations asked for or not   |   reqfe   _NET_REQUEST_FRAME_EXTENTS
 *                       for a new window asking for no decorations: "extents L R T B"
 * GPL-2.0-or-later. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void state_names(Display *d, Window w, char *o, size_t n)
{
	Atom st = XInternAtom(d, "_NET_WM_STATE", False), type; int fmt; unsigned long k = 0, after; unsigned char *p = NULL; o[0] = 0;
	XGetWindowProperty(d, w, st, 0, 32, False, XA_ATOM, &type, &fmt, &k, &after, &p);
	for (unsigned long i = 0; p && i < k; i++) { char *nm = XGetAtomName(d, ((Atom *)p)[i]); if (nm) { size_t l = strlen(o); snprintf(o + l, n - l, "%s%s", l ? "," : "", nm + (strncmp(nm, "_NET_WM_STATE_", 14) ? 0 : 14)); XFree(nm); } }
	if (p) XFree(p);
	if (!o[0]) snprintf(o, n, "-");
}
static void extents(Display *d, Window w)
{
	Atom fe = XInternAtom(d, "_NET_FRAME_EXTENTS", False), type; int fmt; unsigned long k = 0, after; unsigned char *p = NULL;
	XGetWindowProperty(d, w, fe, 0, 4, False, XA_CARDINAL, &type, &fmt, &k, &after, &p);
	if (p && k == 4) printf("extents %ld %ld %ld %ld\n", ((long *)p)[0], ((long *)p)[1], ((long *)p)[2], ((long *)p)[3]); else printf("extents -\n");
	if (p) XFree(p);
}
static void cmsg(Display *d, Window w, const char *type, long a, long b, long c, long e, long f)
{
	XEvent ev = {0}; ev.xclient.type = ClientMessage; ev.xclient.window = w; ev.xclient.format = 32; ev.xclient.message_type = XInternAtom(d, type, False);
	ev.xclient.data.l[0] = a; ev.xclient.data.l[1] = b; ev.xclient.data.l[2] = c; ev.xclient.data.l[3] = e; ev.xclient.data.l[4] = f;
	XSendEvent(d, DefaultRootWindow(d), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev); XSync(d, False);
}
static void motif(Display *d, Window w, int deco) { Atom m = XInternAtom(d, "_MOTIF_WM_HINTS", False); long h[5] = {2, 0, deco, 0, 0}; XChangeProperty(d, w, m, m, 32, PropModeReplace, (unsigned char *)h, 5); }
static void gtkext(Display *d, Window w, long l, long r, long t, long b) { Atom g = XInternAtom(d, "_GTK_FRAME_EXTENTS", False); long v[4] = {l, r, t, b}; XChangeProperty(d, w, g, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)v, 4); }
static int csd_app(Display *d, int argc, char **argv)
{
	Window root = DefaultRootWindow(d); int W = atoi(argv[2]), H = atoi(argv[3]);
	Window w = XCreateSimpleWindow(d, root, 0, 0, W, H, 0, 0, 0xe5e7eb);
	XClassHint ch = {"csdapp", "CsdApp"}; XSetClassHint(d, w, &ch); XStoreName(d, w, "Own title bar stand-in");
	Atom p[2] = {XInternAtom(d, "WM_DELETE_WINDOW", False), XInternAtom(d, "WM_TAKE_FOCUS", False)}; XSetWMProtocols(d, w, p, 2);
	XWMHints wh = {0}; wh.flags = InputHint; wh.input = True; XSetWMHints(d, w, &wh);
	motif(d, w, 0); if (argc > 7) gtkext(d, w, atol(argv[4]), atol(argv[5]), atol(argv[6]), atol(argv[7]));
	XSelectInput(d, w, FocusChangeMask | StructureNotifyMask | ExposureMask | ButtonPressMask | PropertyChangeMask);
	XMapWindow(d, w); XSync(d, False); printf("id 0x%lx\n", w);
	GC gc = XCreateGC(d, w, 0, NULL); Atom take = p[1], del = p[0], st = XInternAtom(d, "_NET_WM_STATE", False); int cw = W, chh = H;
	for (;;) {
		XEvent e; XNextEvent(d, &e);
		if (e.type == Expose && !e.xexpose.count) { XSetForeground(d, gc, 0x1f2937); XFillRectangle(d, w, gc, 0, 0, cw, 30); }
		else if (e.type == ClientMessage && (Atom)e.xclient.data.l[0] == take) printf("take_focus %ld\n", e.xclient.data.l[1]);
		else if (e.type == ClientMessage && (Atom)e.xclient.data.l[0] == del) { printf("delete\n"); return 0; }
		else if (e.type == ConfigureNotify) { cw = e.xconfigure.width; chh = e.xconfigure.height; printf("geom %d %d %d %d\n", e.xconfigure.x, e.xconfigure.y, cw, chh); }
		else if (e.type == PropertyNotify && e.xproperty.atom == st) { char o[300]; state_names(d, w, o, sizeof o); printf("state %s\n", o); }
		else if (e.type == ButtonPress && e.xbutton.button == 1) {
			printf("press %d %d\n", e.xbutton.x, e.xbutton.y);
			int dir = 8; const char *f = getenv("XWM_DIR"); FILE *fp = f ? fopen(f, "r") : NULL; if (fp) { if (fscanf(fp, "%d", &dir) != 1) dir = 8; fclose(fp); }
			if (e.xbutton.y < 30 || dir != 8) { XUngrabPointer(d, e.xbutton.time); cmsg(d, w, "_NET_WM_MOVERESIZE", e.xbutton.x_root, e.xbutton.y_root, dir, 1, 1); }
		}
		(void)chh;
	}
}
int main(int argc, char **argv)
{
	Display *d = XOpenDisplay(NULL); if (!d || argc < 2) return 2;
	Window root = DefaultRootWindow(d);
	setvbuf(stdout, NULL, _IOLBF, 0);
	if (!strcmp(argv[1], "csd") && argc > 3) return csd_app(d, argc, argv);
	if (!strcmp(argv[1], "max") && argc > 3) { cmsg(d, strtoul(argv[2], NULL, 0), "_NET_WM_STATE", atoi(argv[3]), XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_VERT", False), XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_HORZ", False), 1, 0); return 0; }
	if (!strcmp(argv[1], "iconify") && argc > 2) { XIconifyWindow(d, strtoul(argv[2], NULL, 0), DefaultScreen(d)); XSync(d, False); return 0; }
	if (!strcmp(argv[1], "menu") && argc > 4) { cmsg(d, strtoul(argv[2], NULL, 0), "_GTK_SHOW_WINDOW_MENU", 2, atol(argv[3]), atol(argv[4]), 0, 0); return 0; }
	if (!strcmp(argv[1], "mr") && argc > 5) { cmsg(d, strtoul(argv[2], NULL, 0), "_NET_WM_MOVERESIZE", atol(argv[3]), atol(argv[4]), atol(argv[5]), 1, 1); return 0; }
	if (!strcmp(argv[1], "props") && argc > 2) { Window w = strtoul(argv[2], NULL, 0); char o[300]; extents(d, w); state_names(d, w, o, sizeof o); printf("state %s\n", o); return 0; }
	if (!strcmp(argv[1], "gtkext") && argc > 6) { gtkext(d, strtoul(argv[2], NULL, 0), atol(argv[3]), atol(argv[4]), atol(argv[5]), atol(argv[6])); XSync(d, False); return 0; }
	if (!strcmp(argv[1], "deco") && argc > 3) { motif(d, strtoul(argv[2], NULL, 0), atoi(argv[3])); XSync(d, False); return 0; }
	if (!strcmp(argv[1], "supported")) {
		Atom s = XInternAtom(d, "_NET_SUPPORTED", False), type; int fmt; unsigned long k = 0, after; unsigned char *p = NULL;
		XGetWindowProperty(d, root, s, 0, 256, False, XA_ATOM, &type, &fmt, &k, &after, &p);
		for (unsigned long i = 0; p && i < k; i++) { char *nm = XGetAtomName(d, ((Atom *)p)[i]); if (nm) { printf("%s\n", nm); XFree(nm); } }
		return 0;
	}
	if (!strcmp(argv[1], "reqfe")) {
		Window w = XCreateSimpleWindow(d, root, 0, 0, 300, 200, 0, 0, 0); motif(d, w, 0); XSelectInput(d, w, PropertyChangeMask); XSync(d, False);
		XEvent ev = {0}; ev.xclient.type = ClientMessage; ev.xclient.window = w; ev.xclient.format = 32; ev.xclient.message_type = XInternAtom(d, "_NET_REQUEST_FRAME_EXTENTS", False);
		XSendEvent(d, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev); XSync(d, False);
		for (int i = 0; i < 40; i++) { if (XPending(d)) { XEvent e; XNextEvent(d, &e); if (e.type == PropertyNotify) break; } else usleep(50000); }
		extents(d, w); return 0;
	}
	if (!strcmp(argv[1], "full") && argc > 3) {
		XEvent e = {0}; e.xclient.type = ClientMessage; e.xclient.window = strtoul(argv[2], NULL, 0); e.xclient.format = 32;
		e.xclient.message_type = XInternAtom(d, "_NET_WM_STATE", False); e.xclient.data.l[0] = atoi(argv[3]);
		e.xclient.data.l[1] = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False); e.xclient.data.l[3] = 1;
		XSendEvent(d, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &e); XSync(d, False); return 0;
	}
	if (!strcmp(argv[1], "focus") && argc > 2) { XSetInputFocus(d, strtoul(argv[2], NULL, 0), RevertToPointerRoot, CurrentTime); XSync(d, False); return 0; }
	if (!strcmp(argv[1], "wmstate") && argc > 2) {
		Atom a = XInternAtom(d, "WM_STATE", False), type; int fmt; unsigned long n = 0, after; unsigned char *p = NULL;
		XGetWindowProperty(d, strtoul(argv[2], NULL, 0), a, 0, 2, False, a, &type, &fmt, &n, &after, &p);
		printf("%ld\n", p && n ? ((long *)p)[0] : -1L); return 0;
	}
	if (!strcmp(argv[1], "resize") && argc > 4) { XMoveResizeWindow(d, strtoul(argv[2], NULL, 0), 0, 0, atoi(argv[3]), atoi(argv[4])); XSync(d, False); return 0; }
	if (!strcmp(argv[1], "state") && argc > 2) {
		Atom st = XInternAtom(d, "_NET_WM_STATE", False), fs = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False), type; int fmt; unsigned long n = 0, after; unsigned char *p = NULL; int f = 0;
		XGetWindowProperty(d, strtoul(argv[2], NULL, 0), st, 0, 32, False, XA_ATOM, &type, &fmt, &n, &after, &p);
		for (unsigned long i = 0; p && i < n; i++) if (((Atom *)p)[i] == fs) f = 1;
		printf("%s\n", f ? "fullscreen" : "-"); return 0;
	}
	if (!strcmp(argv[1], "stack")) {
		Window r, p, *k = NULL; unsigned n = 0; XQueryTree(d, root, &r, &p, &k, &n);
		for (unsigned i = 0; i < n; i++) { XWindowAttributes wa; if (XGetWindowAttributes(d, k[i], &wa) && wa.map_state == IsViewable && !wa.override_redirect) printf("0x%lx\n", k[i]); }
		return 0;
	}
	if (!strcmp(argv[1], "inno") || !strcmp(argv[1], "orphan")) {
		int orphan = argv[1][0] == 'o';
		Window o = XCreateSimpleWindow(d, root, 0, 0, orphan ? 10 : 300, orphan ? 10 : 200, 0, 0, 0);
		XClassHint oc = {"setup.tmp", "setup.tmp"}; XSetClassHint(d, o, &oc); XStoreName(d, o, "Setup");
		if (!orphan) { XMapWindow(d, o); XSync(d, False); printf("id 0x%lx\n", o); usleep(400000); }
		Window w = XCreateSimpleWindow(d, root, 10, 10, 500, 380, 0, 0, 0);
		XSetClassHint(d, w, &oc); XStoreName(d, w, "Setup - Starport"); XSetTransientForHint(d, w, o);
		Atom pr[2] = {XInternAtom(d, "WM_DELETE_WINDOW", False), XInternAtom(d, "WM_TAKE_FOCUS", False)}; XSetWMProtocols(d, w, pr, 2);
		XWMHints h = {0}; h.flags = InputHint; h.input = True; XSetWMHints(d, w, &h);
		XMapWindow(d, w); XSync(d, False); printf("dlg 0x%lx\n", w);
		/* then the owner is raised by itself, as Wine does when the wizard's owner is activated */
		if (!orphan) { usleep(400000); XRaiseWindow(d, o); XSync(d, False); }
		for (;;) { XEvent e; XNextEvent(d, &e); }
	}
	if (!strcmp(argv[1], "getfocus")) { Window f; int r; XGetInputFocus(d, &f, &r); printf("0x%lx\n", f); return 0; }
	int desk = !strcmp(argv[1], "desktop"), bl = !strcmp(argv[1], "borderless") && argc > 3;
	Window w = XCreateSimpleWindow(d, root, 10, 10, 400, 300, 0, 0, 0);
	XClassHint ch = {desk ? "rudolfdesktop" : "launcher.exe", desk ? "RudolfDesktop" : "launcher.exe"}; XSetClassHint(d, w, &ch);
	XStoreName(d, w, desk ? "Rudolf OS" : "Starport stand-in");
	if (!desk) {
		Atom p[2] = {XInternAtom(d, "WM_DELETE_WINDOW", False), XInternAtom(d, "WM_TAKE_FOCUS", False)}; XSetWMProtocols(d, w, p, 2);
		XWMHints h = {0}; h.flags = InputHint; h.input = True; XSetWMHints(d, w, &h);
	}
	if (bl) { Atom m = XInternAtom(d, "_MOTIF_WM_HINTS", False); long h[5] = {2, 0, 0, 0, 0}; XChangeProperty(d, w, m, m, 32, PropModeReplace, (unsigned char *)h, 5); }
	XSelectInput(d, w, FocusChangeMask | StructureNotifyMask);
	XMapWindow(d, w); XSync(d, False);
	if (bl) { usleep(300000); XMoveResizeWindow(d, w, 0, 0, atoi(argv[2]), atoi(argv[3])); XSync(d, False); }
	printf("id 0x%lx\n", w);
	Atom take = XInternAtom(d, "WM_TAKE_FOCUS", False);
	for (;;) {
		XEvent e; XNextEvent(d, &e);
		if (e.type == ClientMessage && (Atom)e.xclient.data.l[0] == take) printf("take_focus %ld\n", e.xclient.data.l[1]);
		/* (1.5.9: a window manager's close button) */
		else if (e.type == ClientMessage && (Atom)e.xclient.data.l[0] == XInternAtom(d, "WM_DELETE_WINDOW", False)) { printf("delete\n"); return 0; }
		else if (e.type == FocusIn && e.xfocus.detail != NotifyPointer) printf("focus_in\n");
		else if (e.type == ConfigureNotify) printf("geom %d %d %d %d\n", e.xconfigure.x, e.xconfigure.y, e.xconfigure.width, e.xconfigure.height);
	}
}
