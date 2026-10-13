/* Tests rudolf-display's input handling without X or hardware: simulated touchpad, mouse, touchscreen and
 * controller events go in, the XTest calls that come out are checked.
 *   gcc -O0 -o /tmp/input_t iso/qt/input_t.c -lX11 /usr/lib/x86_64-linux-gnu/libXfixes.so.3 && /tmp/input_t        (run from the repository root) */
#include <X11/Xlib.h>
#define XFlush(d) 0
#define main rd_display_main
#include "../sys/rudolf-display.c"
#undef main
static char out[4096]; static int movex, movey, absx = -1, absy = -1;
int XTestFakeKeyEvent(Display *d, unsigned k, Bool down, unsigned long t) { char b[32]; snprintf(b, sizeof b, "k%u%c ", k, down ? 'd' : 'u'); strcat(out, b); return 1; }
int XTestFakeButtonEvent(Display *d, unsigned b, Bool down, unsigned long t) { char s[32]; snprintf(s, sizeof s, "b%u%c ", b, down ? 'd' : 'u'); strcat(out, s); return 1; }
int XTestFakeMotionEvent(Display *d, int sc, int x, int y, unsigned long t) { absx = x; absy = y; return 1; }
int XTestFakeRelativeMotionEvent(Display *d, int x, int y, unsigned long t) { movex += x; movey += y; return 1; }
static void ev(struct dev *v, int type, int code, int val) { struct input_event e = {0}; e.type = type; e.code = code; e.value = val; handle_event(v, &e); }
static void syn(struct dev *v) { ev(v, EV_SYN, SYN_REPORT, 0); }
static void reset(void) { out[0] = 0; movex = movey = 0; absx = absy = -1; rem_x = rem_y = 0; }
static int fails;
static void check(const char *name, int ok) { printf("%s %s  [%s] move=%d,%d\n", ok ? "PASS" : "FAIL", name, out, movex, movey); if (!ok) fails++; }
static struct dev pad(void) { struct dev v; memset(&v, 0, sizeof v); v.touchpad = 1; v.buttonpad = 1; v.kind = 3; v.absx_max = 3000; v.absy_max = 2000; return v; }
static void touch(struct dev *v, int fingers, int x, int y) { ev(v, EV_ABS, ABS_X, x); ev(v, EV_ABS, ABS_Y, y); ev(v, EV_KEY, BTN_TOUCH, 1); ev(v, EV_KEY, fingers == 1 ? BTN_TOOL_FINGER : fingers == 2 ? BTN_TOOL_DOUBLETAP : BTN_TOOL_TRIPLETAP, 1); syn(v); }
static void lift(struct dev *v, int fingers) { ev(v, EV_KEY, BTN_TOUCH, 0); ev(v, EV_KEY, fingers == 1 ? BTN_TOOL_FINGER : fingers == 2 ? BTN_TOOL_DOUBLETAP : BTN_TOOL_TRIPLETAP, 0); syn(v); }
static void moveto(struct dev *v, int x, int y) { ev(v, EV_ABS, ABS_X, x); ev(v, EV_ABS, ABS_Y, y); syn(v); }
int main(void)
{
	xw = 1920; xh = 1080; speed = 1; accel = 1; last_key_ms = -100000;
	struct dev v = pad();
	reset(); touch(&v, 1, 1000, 1000); moveto(&v, 1003, 1002); lift(&v, 1); check("one-finger tap with a little wobble = left click", !strcmp(out, "b1d b1u "));
	reset(); touch(&v, 2, 1000, 1000); lift(&v, 2); check("two-finger tap = right click", !strcmp(out, "b3d b3u "));
	reset(); touch(&v, 2, 1000, 1000); ev(&v, EV_KEY, BTN_TOOL_DOUBLETAP, 0); ev(&v, EV_KEY, BTN_TOOL_FINGER, 1); syn(&v); lift(&v, 1);
	check("two-finger tap, fingers lifted one after the other = right click", !strcmp(out, "b3d b3u "));
	reset(); touch(&v, 3, 1000, 1000); lift(&v, 3); check("three-finger tap = middle click", !strcmp(out, "b2d b2u "));
	reset(); touch(&v, 1, 1000, 1000); for (int i = 1; i <= 50; i++) moveto(&v, 1000 + i * 2, 1000 + i); lift(&v, 1);
	check("slow movement (2 units per report) moves the pointer and does not click", out[0] == 0 && movex > 50 && movey > 25);
	reset(); touch(&v, 1, 1000, 1000); moveto(&v, 1600, 1300); lift(&v, 1); check("fast movement moves, no click", out[0] == 0 && movex > 300);
	reset(); touch(&v, 2, 1000, 1000); for (int i = 1; i <= 10; i++) moveto(&v, 1000, 1000 + i * 40); lift(&v, 2);
	check("two fingers down = scroll down, pointer still", strstr(out, "b5d b5u") && !strstr(out, "b4") && movex == 0 && movey == 0);
	reset(); touch(&v, 2, 1000, 1000); for (int i = 1; i <= 10; i++) moveto(&v, 1000 - i * 40, 1000); lift(&v, 2);
	check("two fingers sideways = horizontal scroll", strstr(out, "b6d b6u") && !strstr(out, "b5"));
	natural_scroll = 1; reset(); touch(&v, 2, 1000, 1000); for (int i = 1; i <= 10; i++) moveto(&v, 1000, 1000 + i * 40); lift(&v, 2); natural_scroll = 0;
	check("natural scrolling reverses it", strstr(out, "b4d b4u") && !strstr(out, "b5"));
	reset(); touch(&v, 1, 500, 500); ev(&v, EV_KEY, BTN_LEFT, 1); syn(&v); ev(&v, EV_KEY, BTN_LEFT, 0); syn(&v); lift(&v, 1);
	check("clickpad press = left click (no extra tap)", !strcmp(out, "b1d b1u "));
	reset(); touch(&v, 1, 2800, 1900); ev(&v, EV_KEY, BTN_LEFT, 1); syn(&v); ev(&v, EV_KEY, BTN_LEFT, 0); syn(&v); lift(&v, 1);
	check("clickpad press in the bottom-right corner = right click", !strcmp(out, "b3d b3u "));
	reset(); touch(&v, 2, 1000, 1000); ev(&v, EV_KEY, BTN_LEFT, 1); syn(&v); ev(&v, EV_KEY, BTN_LEFT, 0); syn(&v); lift(&v, 2);
	check("clickpad press with two fingers = right click", !strcmp(out, "b3d b3u "));
	reset(); touch(&v, 1, 1000, 1000); for (int i = 1; i <= 20; i++) moveto(&v, 1000 + i * 20, 1000); ev(&v, EV_KEY, BTN_LEFT, 1); syn(&v);
	for (int i = 1; i <= 10; i++) moveto(&v, 1400 + i * 20, 1000); ev(&v, EV_KEY, BTN_LEFT, 0); syn(&v); lift(&v, 1);
	check("press and drag keeps the button down while moving", !strcmp(out, "b1d b1u ") && movex > 400);
	tap_enabled = 0; reset(); touch(&v, 1, 1000, 1000); lift(&v, 1); tap_enabled = 1; check("tap to click switched off = no click", out[0] == 0);
	struct dev kb; memset(&kb, 0, sizeof kb); kb.kind = 1;
	reset(); ev(&kb, EV_KEY, KEY_A, 1); ev(&kb, EV_KEY, KEY_A, 0); touch(&v, 1, 1000, 1000); lift(&v, 1); moveto(&v, 1000, 1000);
	check("tap right after typing is ignored (palm)", !strcmp(out, "k38d k38u "));
	last_key_ms = -100000;
	reset(); ev(&kb, EV_KEY, KEY_LEFTSHIFT, 1); ev(&kb, EV_KEY, KEY_LEFTSHIFT, 0); touch(&v, 1, 1000, 1000); lift(&v, 1);
	check("Shift alone does not block a tap (Shift+click)", strstr(out, "b1d b1u"));
	struct dev m; memset(&m, 0, sizeof m); m.kind = 2;
	reset(); for (int i = 0; i < 10; i++) ev(&m, EV_REL, REL_X, 1); ev(&m, EV_REL, REL_WHEEL, -1); ev(&m, EV_KEY, BTN_RIGHT, 1); ev(&m, EV_KEY, BTN_RIGHT, 0);
	check("mouse: moves, wheel, right button", movex == 10 && !strcmp(out, "b5d b5u b3d b3u "));
	speed = 0.5; reset(); for (int i = 0; i < 10; i++) ev(&m, EV_REL, REL_X, 1); speed = 1; check("mouse at half speed keeps fractions (10 x 0.5 = 5)", movex == 5);
	struct dev ts; memset(&ts, 0, sizeof ts); ts.touchscreen = 1; ts.kind = 4; ts.absx_max = 4095; ts.absy_max = 4095;
	reset(); ev(&ts, EV_ABS, ABS_X, 2048); ev(&ts, EV_ABS, ABS_Y, 1024); ev(&ts, EV_KEY, BTN_TOUCH, 1); syn(&ts); ev(&ts, EV_KEY, BTN_TOUCH, 0); syn(&ts);
	check("touchscreen: touch = click where touched", !strcmp(out, "b1d b1u ") && absx == 960 && absy == 270);
	/* classification from capabilities */
	uint8_t evb[EV_MAX / 8 + 1], keyb[KEY_MAX / 8 + 1], relb[REL_MAX / 8 + 1], absb[ABS_MAX / 8 + 1], prop[INPUT_PROP_MAX / 8 + 1];
#define CLR() memset(evb, 0, sizeof evb), memset(keyb, 0, sizeof keyb), memset(relb, 0, sizeof relb), memset(absb, 0, sizeof absb), memset(prop, 0, sizeof prop)
#define SET(a, b) a[(b) / 8] |= 1 << ((b) % 8)
	struct dev c;
	CLR(); SET(evb, EV_KEY); SET(evb, EV_ABS); SET(keyb, BTN_SOUTH); SET(keyb, BTN_GAMEPAD); SET(absb, ABS_X); SET(absb, ABS_Y); memset(&c, 0, sizeof c);
	reset(); int k = classify(&c, evb, keyb, relb, absb, prop); check("game controller is left to games (not a touchscreen)", k == 0 && !c.touchscreen);
	CLR(); SET(evb, EV_KEY); SET(evb, EV_ABS); SET(keyb, BTN_TOUCH); SET(keyb, BTN_TOOL_FINGER); SET(keyb, BTN_LEFT); SET(absb, ABS_X); SET(absb, ABS_Y); SET(prop, INPUT_PROP_POINTER); SET(prop, INPUT_PROP_BUTTONPAD); memset(&c, 0, sizeof c);
	reset(); k = classify(&c, evb, keyb, relb, absb, prop); check("laptop clickpad is a touchpad", k == 3 && c.touchpad && c.buttonpad);
	CLR(); SET(evb, EV_KEY); SET(evb, EV_ABS); SET(keyb, BTN_TOUCH); SET(keyb, BTN_TOOL_FINGER); SET(absb, ABS_X); SET(absb, ABS_Y); SET(prop, INPUT_PROP_DIRECT); memset(&c, 0, sizeof c);
	reset(); k = classify(&c, evb, keyb, relb, absb, prop); check("touchscreen is a touchscreen", k == 4 && c.touchscreen);
	CLR(); SET(evb, EV_KEY); SET(evb, EV_ABS); SET(keyb, BTN_LEFT); SET(absb, ABS_X); SET(absb, ABS_Y); memset(&c, 0, sizeof c);
	reset(); k = classify(&c, evb, keyb, relb, absb, prop); check("absolute pointer (QEMU tablet) works like a touchscreen", k == 4);
	CLR(); SET(evb, EV_KEY); SET(evb, EV_REL); SET(keyb, BTN_LEFT); SET(relb, REL_X); SET(relb, REL_Y); memset(&c, 0, sizeof c);
	reset(); k = classify(&c, evb, keyb, relb, absb, prop); check("mouse is a mouse", k == 2);
	CLR(); SET(evb, EV_KEY); SET(keyb, KEY_A); SET(keyb, KEY_Z); memset(&c, 0, sizeof c);
	reset(); k = classify(&c, evb, keyb, relb, absb, prop); check("keyboard is a keyboard", k == 1);
	printf(fails ? "%d FAILED\n" : "all passed\n", fails);
	return fails != 0;
}
