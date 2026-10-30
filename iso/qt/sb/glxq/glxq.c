/* glxq: does the X server offer GLX, and which version? (1.4.3 test helper; needs only libX11, like an app before
 * it loads OpenGL). Prints "GLX 1.4" or "no GLX". GPL-2.0-or-later. */
#include <stdio.h>
#include <X11/Xlibint.h>
#include <GL/glxproto.h>
int main(void)
{
	Display *dpy = XOpenDisplay(NULL), *d = dpy; if (!d) { puts("no display"); return 2; }
	int op, ev, er; if (!XQueryExtension(d, "GLX", &op, &ev, &er)) { puts("no GLX"); return 1; }
	xGLXQueryVersionReq *q; xGLXQueryVersionReply rep;
	LockDisplay(d);
	q = (xGLXQueryVersionReq *)_XGetRequest(d, X_GLXQueryVersion, sz_xGLXQueryVersionReq);
	q->reqType = op; q->glxCode = X_GLXQueryVersion; q->majorVersion = 1; q->minorVersion = 4;
	int ok = _XReply(d, (xReply *)&rep, 0, False);
	UnlockDisplay(d); SyncHandle();
	if (!ok) { puts("GLX (no version reply)"); return 1; }
	printf("GLX %u.%u\n", (unsigned)rep.majorVersion, (unsigned)rep.minorVersion);
	XCloseDisplay(d); return 0;
}
