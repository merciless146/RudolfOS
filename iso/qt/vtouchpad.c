/* Rudolf OS test: a virtual laptop touchpad (uinput) that performs gestures, for the QEMU tests.
 *   vtouchpad GESTURE...   gestures: move, tap, tap2, tap3, scroll, click, rclick, wait
 * Build: gcc -static -O2 -o vtouchpad vtouchpad.c. GPL-2.0-or-later. */
#include <fcntl.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fd;
static void ev(int t, int c, int v) {
	struct input_event e; memset(&e, 0, sizeof e);
	e.type = t; e.code = c; e.value = v;
	if (write(fd, &e, sizeof e) != sizeof e) perror("write");
}
static void syn(void) { ev(EV_SYN, SYN_REPORT, 0); usleep(12000); }
static void slot(int s, int id, int x, int y) {
	ev(EV_ABS, ABS_MT_SLOT, s); ev(EV_ABS, ABS_MT_TRACKING_ID, id);
	if (id >= 0) { ev(EV_ABS, ABS_MT_POSITION_X, x); ev(EV_ABS, ABS_MT_POSITION_Y, y); }
}
static void tool(int n) {
	ev(EV_KEY, BTN_TOUCH, n > 0);
	ev(EV_KEY, BTN_TOOL_FINGER, n == 1);
	ev(EV_KEY, BTN_TOOL_DOUBLETAP, n == 2);
	ev(EV_KEY, BTN_TOOL_TRIPLETAP, n == 3);
}
/* n fingers down at (x,y), spaced 300 apart */
static void down(int n, int x, int y) {
	for (int i = 0; i < n; i++) slot(i, 100 + i, x + i * 300, y);
	ev(EV_ABS, ABS_X, x); ev(EV_ABS, ABS_Y, y); tool(n); syn();
}
static void up(int n) { for (int i = 0; i < n; i++) slot(i, -1, 0, 0); tool(0); syn(); }
static void moveto(int n, int x, int y) {
	for (int i = 0; i < n; i++) { ev(EV_ABS, ABS_MT_SLOT, i); ev(EV_ABS, ABS_MT_POSITION_X, x + i * 300); ev(EV_ABS, ABS_MT_POSITION_Y, y); }
	ev(EV_ABS, ABS_X, x); ev(EV_ABS, ABS_Y, y); syn();
}

int main(int argc, char **argv) {
	fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (fd < 0) { perror("/dev/uinput"); return 1; }
	ioctl(fd, UI_SET_EVBIT, EV_KEY); ioctl(fd, UI_SET_EVBIT, EV_ABS); ioctl(fd, UI_SET_EVBIT, EV_SYN);
	int keys[] = {BTN_LEFT, BTN_TOUCH, BTN_TOOL_FINGER, BTN_TOOL_DOUBLETAP, BTN_TOOL_TRIPLETAP};
	for (unsigned i = 0; i < sizeof keys / sizeof *keys; i++) ioctl(fd, UI_SET_KEYBIT, keys[i]);
	ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_POINTER); ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_BUTTONPAD);
	struct uinput_user_dev u; memset(&u, 0, sizeof u);
	strcpy(u.name, "Rudolf Test Touchpad"); u.id.bustype = BUS_I2C; u.id.vendor = 0x04f3; u.id.product = 0x3195;
	int ax[] = {ABS_X, ABS_Y, ABS_MT_SLOT, ABS_MT_TRACKING_ID, ABS_MT_POSITION_X, ABS_MT_POSITION_Y};
	int mx[] = {3000, 2000, 4, 65535, 3000, 2000};
	for (int i = 0; i < 6; i++) { ioctl(fd, UI_SET_ABSBIT, ax[i]); u.absmax[ax[i]] = mx[i]; }
	if (write(fd, &u, sizeof u) != sizeof u || ioctl(fd, UI_DEV_CREATE) < 0) { perror("create"); return 1; }
	sleep(2); /* rudolf-display rescans /dev/input */
	for (int a = 1; a < argc; a++) {
		const char *g = argv[a];
		if (!strcmp(g, "move")) { down(1, 1000, 1000); for (int i = 1; i <= 20; i++) moveto(1, 1000 + i * 30, 1000 + i * 15); up(1); }
		else if (!strcmp(g, "tap")) { down(1, 1500, 1000); usleep(60000); up(1); }
		else if (!strcmp(g, "tap2")) { down(2, 1200, 1000); usleep(60000); up(2); }
		else if (!strcmp(g, "tap3")) { down(3, 900, 1000); usleep(60000); up(3); }
		else if (!strcmp(g, "scroll")) { down(2, 1200, 600); for (int i = 1; i <= 20; i++) moveto(2, 1200, 600 + i * 40); up(2); }
		else if (!strcmp(g, "click")) { down(1, 1000, 800); ev(EV_KEY, BTN_LEFT, 1); syn(); ev(EV_KEY, BTN_LEFT, 0); syn(); up(1); }
		else if (!strcmp(g, "rclick")) { down(1, 2700, 1900); ev(EV_KEY, BTN_LEFT, 1); syn(); ev(EV_KEY, BTN_LEFT, 0); syn(); up(1); }
		else if (!strcmp(g, "wait")) sleep(1);
		usleep(400000);
		printf("%s ok\n", g); fflush(stdout);
	}
	sleep(1);
	ioctl(fd, UI_DEV_DESTROY);
	return 0;
}
