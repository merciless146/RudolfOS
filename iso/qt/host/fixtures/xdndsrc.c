/* 1.6.1 (docs/plan-1.6.1.md Part 13): a stand-in X client that drags files as a Linux app's file manager does (XDND, version 5),
   for the native harness (xdrop161_t.sh), as xwmclient.c stands in for an app's window.
   xdndsrc X Y [-hover MS] [-text] [-nodrop] URI...: finds the window under X Y that takes drops (XdndAware, the top one, as
   GTK's and Qt's sources look), offers text/uri-list (with -text: text/plain only, the URIs as words), says XdndEnter and
   XdndPosition (every 100 ms for MS with -hover), waits for XdndStatus, then drops (XdndDrop) and gives the data, and waits for
   XdndFinished. Prints one line each: "target WINDOW NAME" (or "target none"), "status ACCEPT ACTION", "data TYPE",
   "finished ACCEPT ACTION" (or "finished timeout"). Build: gcc -o xdndsrc xdndsrc.c -lX11 */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <poll.h>
static Display *D; static Window SRC;
static Atom A(const char *n) { return XInternAtom(D, n, False); }
static long ms(void) { struct timeval t; gettimeofday(&t, NULL); return t.tv_sec * 1000L + t.tv_usec / 1000; }
static int aware(Window w)
{
	Atom t; int f; unsigned long n, a; unsigned char *p = NULL;
	if (XGetWindowProperty(D, w, A("XdndAware"), 0, 1, False, AnyPropertyType, &t, &f, &n, &a, &p) != Success || !p) return 0;
	XFree(p); return n > 0;
}
/* the top window at X Y (the root's coordinates) that takes drops: the topmost child holding the point, then its own children */
static Window find(Window w, int x, int y, int ox, int oy)
{
	Window r, pa, *k = NULL; unsigned n = 0;
	if (!XQueryTree(D, w, &r, &pa, &k, &n)) return 0;
	for (int i = (int)n - 1; i >= 0; i--) { XWindowAttributes a; if (k[i] == SRC || !XGetWindowAttributes(D, k[i], &a) || a.map_state != IsViewable || a.class == InputOnly) continue;
		int cx = ox + a.x + a.border_width, cy = oy + a.y + a.border_width;
		if (x < cx || y < cy || x >= cx + a.width || y >= cy + a.height) continue;
		Window got = aware(k[i]) ? k[i] : find(k[i], x, y, cx, cy); XFree(k); return got; }
	if (k) XFree(k); return 0;
}
static void send(Window to, const char *type, long l0, long l1, long l2, long l3, long l4)
{
	XEvent e; memset(&e, 0, sizeof e); e.xclient.type = ClientMessage; e.xclient.display = D; e.xclient.window = to; e.xclient.message_type = A(type); e.xclient.format = 32;
	e.xclient.data.l[0] = l0; e.xclient.data.l[1] = l1; e.xclient.data.l[2] = l2; e.xclient.data.l[3] = l3; e.xclient.data.l[4] = l4;
	XSendEvent(D, to, False, NoEventMask, &e); XFlush(D);
}
/* the next event within MS (0: none) */
static int next(XEvent *e, long until)
{
	for (;;) { if (XPending(D)) { XNextEvent(D, e); return 1; }
		long left = until - ms(); if (left <= 0) return 0; struct pollfd p = { ConnectionNumber(D), POLLIN, 0 }; poll(&p, 1, left > 100 ? 100 : (int)left); }
}
int main(int argc, char **argv)
{
	if (argc < 4) { fprintf(stderr, "xdndsrc X Y [-hover MS] [-text] [-nodrop] URI...\n"); return 2; }
	int x = atoi(argv[1]), y = atoi(argv[2]), hover = 0, text = 0, nodrop = 0, i = 3;
	for (; i < argc && argv[i][0] == '-'; i++) { if (!strcmp(argv[i], "-hover") && i + 1 < argc) hover = atoi(argv[++i]); else if (!strcmp(argv[i], "-text")) text = 1; else if (!strcmp(argv[i], "-nodrop")) nodrop = 1; }
	D = XOpenDisplay(NULL); if (!D) { printf("no display\n"); return 1; }
	/* the data: the URIs as text/uri-list (CRLF after each), or as plain text */
	size_t cap = 16; for (int j = i; j < argc; j++) cap += strlen(argv[j]) + 2; char *data = calloc(1, cap);
	for (int j = i; j < argc; j++) { strcat(data, argv[j]); strcat(data, text ? (j + 1 < argc ? " " : "") : "\r\n"); }
	Atom ty = A(text ? "text/plain" : "text/uri-list");
	SRC = XCreateSimpleWindow(D, DefaultRootWindow(D), -10, -10, 1, 1, 0, 0, 0); XSelectInput(D, SRC, PropertyChangeMask);
	XSetSelectionOwner(D, A("XdndSelection"), SRC, CurrentTime);
	Window t = find(DefaultRootWindow(D), x, y, 0, 0);
	if (!t) { printf("target none\n"); return 0; }
	{ char *nm = NULL; Window w = t; XFetchName(D, w, &nm); printf("target 0x%lx %s\n", t, nm ? nm : "-"); if (nm) XFree(nm); }
	send(t, "XdndEnter", SRC, 5L << 24, ty, 0, 0);
	int acc = -1; long act = 0; long end = ms() + (hover > 0 ? hover : 0);
	do { send(t, "XdndPosition", SRC, 0, ((long)x << 16) | y, CurrentTime, A("XdndActionCopy"));
		XEvent e; long until = ms() + 2000;
		while (next(&e, until)) if (e.type == ClientMessage && e.xclient.message_type == A("XdndStatus")) { acc = e.xclient.data.l[1] & 1; act = e.xclient.data.l[4]; break; }
		if (hover > 0) { long w8 = ms() + 100; XEvent e2; while (next(&e2, w8)) {} }
	} while (ms() < end);
	{ char *an = act ? XGetAtomName(D, act) : NULL; printf("status %d %s\n", acc, an ? an : "none"); if (an) XFree(an); }
	if (acc != 1 || nodrop) { send(t, "XdndLeave", SRC, 0, 0, 0, 0); return 0; }
	send(t, "XdndDrop", SRC, 0, CurrentTime, 0, 0);
	long until = ms() + 150000; XEvent e;
	while (next(&e, until)) {
		if (e.type == SelectionRequest) { XSelectionRequestEvent *r = &e.xselectionrequest; XEvent n; memset(&n, 0, sizeof n);
			n.xselection.type = SelectionNotify; n.xselection.requestor = r->requestor; n.xselection.selection = r->selection; n.xselection.target = r->target; n.xselection.time = r->time; n.xselection.property = None;
			if (r->target == ty) { XChangeProperty(D, r->requestor, r->property, ty, 8, PropModeReplace, (unsigned char *)data, strlen(data)); n.xselection.property = r->property;
				char *tn = XGetAtomName(D, ty); printf("data %s\n", tn); XFree(tn); fflush(stdout); }
			else if (r->target == A("TARGETS")) { Atom l[2] = { A("TARGETS"), ty }; XChangeProperty(D, r->requestor, r->property, XA_ATOM, 32, PropModeReplace, (unsigned char *)l, 2); n.xselection.property = r->property; }
			XSendEvent(D, r->requestor, False, NoEventMask, &n); XFlush(D); }
		else if (e.type == ClientMessage && e.xclient.message_type == A("XdndFinished")) {
			long fa = e.xclient.data.l[2]; char *an = fa ? XGetAtomName(D, fa) : NULL; printf("finished %ld %s\n", e.xclient.data.l[1] & 1, an ? an : "none"); if (an) XFree(an); return 0; }
	}
	printf("finished timeout\n"); return 0;
}
