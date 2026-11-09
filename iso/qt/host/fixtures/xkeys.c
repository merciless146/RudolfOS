/* Sends one key combination through XTest (kioskkeys_t.py, 1.5.6): xkeys ctrl+shift+w. GPL-2.0-or-later. */
#include <X11/Xlib.h>
extern int XTestFakeKeyEvent(Display *, unsigned int, Bool, unsigned long);
#include <X11/keysym.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int c, char **v) {
	Display *d = XOpenDisplay(NULL); if (!d || c < 2) return 2;
	char b[128]; snprintf(b, sizeof b, "%s", v[1]); KeyCode ks[8]; int n = 0;
	for (char *t = strtok(b, "+"); t && n < 8; t = strtok(NULL, "+")) {
		KeySym s = !strcmp(t, "ctrl") ? XK_Control_L : !strcmp(t, "shift") ? XK_Shift_L : !strcmp(t, "alt") ? XK_Alt_L : XStringToKeysym(t);
		if (s == NoSymbol) { fprintf(stderr, "no key %s\n", t); return 1; }
		ks[n++] = XKeysymToKeycode(d, s);
	}
	for (int i = 0; i < n; i++) { XTestFakeKeyEvent(d, ks[i], True, 0); XSync(d, False); usleep(20000); }
	for (int i = n; i-- > 0;) { XTestFakeKeyEvent(d, ks[i], False, 0); XSync(d, False); usleep(20000); }
	return 0;
}
