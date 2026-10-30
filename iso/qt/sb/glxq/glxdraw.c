/* glxdraw: what an OpenGL game does through X11 (as Oolite does through SDL): a window, a GLX context, a frame
 * drawn and read back. Prints the renderer and "drew ok", or what failed. (1.4.3 test helper.) GPL-2.0-or-later. */
#include <stdio.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
int main(void)
{
	Display *d = XOpenDisplay(NULL); if (!d) { puts("no display"); return 2; }
	int att[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 24, None};
	XVisualInfo *vi = glXChooseVisual(d, DefaultScreen(d), att); if (!vi) { puts("no GLX visual"); return 1; }
	XSetWindowAttributes swa = {0}; swa.colormap = XCreateColormap(d, RootWindow(d, vi->screen), vi->visual, AllocNone);
	Window w = XCreateWindow(d, RootWindow(d, vi->screen), 0, 0, 64, 64, 0, vi->depth, InputOutput, vi->visual, CWColormap, &swa);
	XMapWindow(d, w); XSync(d, False);
	GLXContext c = glXCreateContext(d, vi, NULL, True); if (!c) { puts("no GLX context"); return 1; }
	if (!glXMakeCurrent(d, w, c)) { puts("could not make the context current"); return 1; }
	printf("renderer: %s | version: %s | direct: %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION), glXIsDirect(d, c) ? "yes" : "no");
	glClearColor(0.0f, 1.0f, 0.0f, 1.0f); glClear(GL_COLOR_BUFFER_BIT); glFinish();
	unsigned char px[4] = {0}; glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
	glXSwapBuffers(d, w);
	printf(px[1] > 200 && px[0] < 50 ? "drew ok\n" : "drew wrong (%u %u %u)\n", px[0], px[1], px[2]);
	glXMakeCurrent(d, None, NULL); glXDestroyContext(d, c); XCloseDisplay(d); return 0;
}
