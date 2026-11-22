/* 1.6.2 (docs/plan-1.6.2.md Part 25b): a stand-in Linux app's window for the taskbar progress tests: a plain X window with the
   class and instance given (WM_CLASS) and a title, then it waits; it ends when closed (WM_DELETE_WINDOW) or killed. Lines on
   standard input set its own progress as Firefox does on X11: "p N" the XApp hint _NET_WM_XAPP_PROGRESS (a CARDINAL), "d" it
   removed.   lxwin CLASS INSTANCE TITLE      gcc -o lxwin lxwin.c -lX11. GPL-2.0-or-later. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <X11/Xatom.h>
int main(int argc, char **argv)
{
	if (argc < 4) return 2;
	Display *d = XOpenDisplay(NULL); if (!d) return 2;
	Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), 40, 40, 480, 320, 0, 0, 0x30405a);
	XClassHint ch = {argv[2], argv[1]}; XSetClassHint(d, w, &ch); XStoreName(d, w, argv[3]);
	Atom del = XInternAtom(d, "WM_DELETE_WINDOW", False); XSetWMProtocols(d, w, &del, 1);
	XMapWindow(d, w); XFlush(d); printf("id 0x%lx\n", (unsigned long)w); fflush(stdout);
	Atom pr = XInternAtom(d, "_NET_WM_XAPP_PROGRESS", False); int in = 1;
	for (;;) {
		while (XPending(d)) { XEvent e; XNextEvent(d, &e); if (e.type == ClientMessage && (Atom)e.xclient.data.l[0] == del) return 0; }
		struct pollfd p[2] = {{ConnectionNumber(d), POLLIN, 0}, {0, POLLIN, 0}}; if (poll(p, in ? 2 : 1, 200) <= 0 || !(p[1].revents & (POLLIN | POLLHUP))) continue;
		char l[64]; ssize_t n = read(0, l, sizeof l - 1); if (n <= 0) { in = 0; continue; } l[n] = 0;
		if (l[0] == 'p') { unsigned long v = strtoul(l + 1, NULL, 10); XChangeProperty(d, w, pr, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&v, 1); }
		else if (l[0] == 'd') XDeleteProperty(d, w, pr);
		XFlush(d); printf("set\n"); fflush(stdout);
	}
}
