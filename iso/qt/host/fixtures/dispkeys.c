/* 1.6.0 (plan Part 15, "Before stable" item 10): rudolf-display's keys, compiled from rudolf-display.c itself (main renamed),
   as torwinapi.c does with rudolfd.c. A keyboard's key presses go through rudolf-display's own handle_event (desk_key: the
   keys Chromium would act on handed to the desktop's and the Tor windows' Chromium as Ctrl+F20..F23, Alt+F20) to the X
   server through XTest, exactly as from /dev/input (here no evdev device is needed).
     dispkeys feed WINDOW COMBO...   WINDOW (an X window id, 0: as it is) given the keyboard first, then each combination
                                     pressed and let go: ctrl+w, ctrl+shift+n, alt+F4, w, ... (evdev's key names, as below)
     dispkeys find CLASS             the first window shown of that class (Chromium's --class), its id
     dispkeys unit                   its own windows (class RudolfDesktop, RudolfTor and a window inside one, RudolfOracle, a
                                     Linux app's), each given the keyboard in turn and each combination pressed: one line a
                                     key, "CLASS COMBO -> KEYSYM MODS" as the window received it
   GPL-2.0-or-later. */
#define main rudolf_display_main
#include "../../../sys/rudolf-display.c"
#undef main
#include <X11/keysym.h>
#include <X11/XKBlib.h>
static const struct { const char *n; int c; } NAMES[] = {
	{"ctrl", KEY_LEFTCTRL}, {"shift", KEY_LEFTSHIFT}, {"alt", KEY_LEFTALT}, {"w", KEY_W}, {"t", KEY_T}, {"n", KEY_N}, {"q", KEY_Q},
	{"a", KEY_A}, {"x", KEY_X}, {"F4", KEY_F4}, {"Tab", KEY_TAB}, {"Return", KEY_ENTER}, {NULL, 0} };
static int code_of(const char *n) { for (int i = 0; NAMES[i].n; i++) if (!strcmp(NAMES[i].n, n)) return NAMES[i].c; return -1; }
static struct dev KB = { .kind = 1 };
static void press(int c, int v) { struct input_event e = { .type = EV_KEY, .code = c, .value = v }; handle_event(&KB, &e); XSync(dpy, False); usleep(15000); }
static int combo(const char *s)
{
	char b[128]; snprintf(b, sizeof b, "%s", s); int k[8], n = 0;
	for (char *t = strtok(b, "+"); t && n < 8; t = strtok(NULL, "+")) { if ((k[n] = code_of(t)) < 0) { fprintf(stderr, "dispkeys: no key %s\n", t); return 1; } n++; }
	for (int i = 0; i < n; i++) press(k[i], 1);
	for (int i = n; i-- > 0;) press(k[i], 0);
	return 0;
}
static void focus(Window w) { if (w) { XSetInputFocus(dpy, w, RevertToParent, CurrentTime); XSync(dpy, False); } usleep(300000); /* (desk_focused looks again after 250 ms) */ }
static const char *COMBOS[] = { "ctrl+w", "ctrl+t", "ctrl+n", "ctrl+q", "ctrl+F4", "alt+F4", "ctrl+shift+n", "ctrl+shift+w", "w", "ctrl+a", "ctrl+alt+w", NULL };
static Window mkwin(const char *cls, Window parent)
{
	Window w = XCreateSimpleWindow(dpy, parent ? parent : DefaultRootWindow(dpy), 10, 10, 200, 100, 0, 0, 0);
	XClassHint h = { (char *)"test", (char *)cls }; if (!parent) XSetClassHint(dpy, w, &h);
	XSelectInput(dpy, w, KeyPressMask | KeyReleaseMask); XMapWindow(dpy, w); return w;
}
static void drain(const char *cls, const char *c, Window w)
{
	XSync(dpy, False); usleep(50000); XEvent e; int any = 0;
	while (XCheckWindowEvent(dpy, w, KeyPressMask | KeyReleaseMask, &e)) {
		if (e.type != KeyPress) continue;
		KeySym s = XkbKeycodeToKeysym(dpy, e.xkey.keycode, 0, 0); const char *nm = XKeysymToString(s); int ev = (int)e.xkey.keycode - 8;
		/* (F20..F23 by their code, as Chromium reads them: X's keymap may name them otherwise) */
		static const char *FN[] = { "F20", "F21", "F22", "F23" }; if (ev >= KEY_F20 && ev <= KEY_F23) nm = FN[ev - KEY_F20];
		if (s == XK_Control_L || s == XK_Shift_L || s == XK_Alt_L || s == XK_Meta_L) continue;
		printf("%s %s -> %s %s%s%s\n", cls, c, nm ? nm : "?", e.xkey.state & ControlMask ? "C" : "", e.xkey.state & ShiftMask ? "S" : "", e.xkey.state & Mod1Mask ? "A" : ""); any = 1;
	}
	if (!any) printf("%s %s -> none\n", cls, c);
	fflush(stdout);
}
int main(int argc, char **argv)
{
	if (argc < 2 || !(dpy = open_display())) return 2;
	xw = DisplayWidth(dpy, DefaultScreen(dpy)); xh = DisplayHeight(dpy, DefaultScreen(dpy));
	if (!strcmp(argv[1], "feed") && argc > 2) {
		focus(strtoul(argv[2], NULL, 0));
		for (int i = 3; i < argc; i++) if (combo(argv[i])) return 1;
		return 0;
	}
	if (!strcmp(argv[1], "find") && argc > 2) {
		Window r, p, *ch = NULL; unsigned n = 0; XQueryTree(dpy, DefaultRootWindow(dpy), &r, &p, &ch, &n);
		for (unsigned i = 0; i < n; i++) { XWindowAttributes wa; XClassHint h = {0}; int m = 0;
			if (!XGetWindowAttributes(dpy, ch[i], &wa) || wa.map_state != IsViewable || !XGetClassHint(dpy, ch[i], &h)) continue;
			m = h.res_class && !strcmp(h.res_class, argv[2]); if (h.res_name) XFree(h.res_name); if (h.res_class) XFree(h.res_class);
			if (m) { printf("0x%lx\n", ch[i]); return 0; } }
		return 1;
	}
	if (strcmp(argv[1], "unit")) return 2;
	const char *cls[] = { "RudolfDesktop", "RudolfTor", "RudolfTor/inside", "RudolfOracle", "LinuxApp" };
	Window W[5];
	for (int i = 0; i < 5; i++) W[i] = i == 2 ? mkwin(NULL, W[1]) : mkwin(cls[i], 0);
	XSync(dpy, False); usleep(200000);
	for (int i = 0; i < 5; i++) {
		focus(W[i]); XEvent e; while (XCheckWindowEvent(dpy, W[i], KeyPressMask | KeyReleaseMask, &e)) ;
		for (int k = 0; COMBOS[k]; k++) { combo(COMBOS[k]); drain(cls[i], COMBOS[k], W[i]); }
	}
	return 0;
}
