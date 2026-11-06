/* A stand-in X client for wmfocus_t.sh (1.5.2): the desktop (class RudolfDesktop), or an app window that behaves
 * like Wine's: WM_PROTOCOLS with WM_TAKE_FOCUS and WM_DELETE_WINDOW, input hint on. It logs what a window manager
 * sends it, one line each, to stdout: "id 0x..", "take_focus TIME", "focus_in", "geom X Y W H".
 *   xwmclient desktop | app | borderless W H (a Wine-style borderless window, then sized W x H) | resize ID W H |
 *             full ID 0|1|2 (full screen off/on/toggle) | focus ID | getfocus | wmstate ID (1 = NormalState)
 * GPL-2.0-or-later. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char **argv)
{
	Display *d = XOpenDisplay(NULL); if (!d || argc < 2) return 2;
	Window root = DefaultRootWindow(d);
	setvbuf(stdout, NULL, _IOLBF, 0);
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
		else if (e.type == FocusIn && e.xfocus.detail != NotifyPointer) printf("focus_in\n");
		else if (e.type == ConfigureNotify) printf("geom %d %d %d %d\n", e.xconfigure.x, e.xconfigure.y, e.xconfigure.width, e.xconfigure.height);
	}
}
