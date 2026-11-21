/* 1.6.1 (docs/plan-1.6.1.md Part 19): a stand-in game for the game mode tests: an X window that asks to be full screen
 * (_NET_WM_STATE_FULLSCREEN, as SDL games do) and names its process (_NET_WM_PID), then waits; SIGUSR1 leaves full screen,
 * SIGUSR2 asks for it again, and a SIGKILL is the crash.   fsgame [W H]      gcc -o fsgame fsgame.c -lX11. GPL-2.0-or-later. */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static volatile sig_atomic_t want = -1;
static void on1(int s) { (void)s; want = 0; }
static void on2(int s) { (void)s; want = 1; }
static void full(Display *d, Window w, int on)
{
	XEvent e = {0}; e.xclient.type = ClientMessage; e.xclient.window = w; e.xclient.message_type = XInternAtom(d, "_NET_WM_STATE", False);
	e.xclient.format = 32; e.xclient.data.l[0] = on ? 1 : 0; e.xclient.data.l[1] = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False); e.xclient.data.l[3] = 1;
	XSendEvent(d, DefaultRootWindow(d), False, SubstructureRedirectMask | SubstructureNotifyMask, &e); XFlush(d);
}
int main(int argc, char **argv)
{
	Display *d = XOpenDisplay(NULL); if (!d) return 2;
	int W = argc > 2 ? atoi(argv[1]) : 640, H = argc > 2 ? atoi(argv[2]) : 480;
	Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, W, H, 0, 0, 0x203040);
	unsigned long pid = (unsigned long)getpid(); XChangeProperty(d, w, XInternAtom(d, "_NET_WM_PID", False), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
	XStoreName(d, w, "Stand-in game"); XMapWindow(d, w); XFlush(d); usleep(300000); full(d, w, 1);
	signal(SIGUSR1, on1); signal(SIGUSR2, on2);
	for (;;) { while (XPending(d)) { XEvent e; XNextEvent(d, &e); } if (want >= 0) { full(d, w, want); want = -1; } usleep(50000); }
}
