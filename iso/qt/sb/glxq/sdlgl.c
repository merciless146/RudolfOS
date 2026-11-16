/* sdlgl: what a game such as Unvanquished (the Daemon engine) or SuperTuxKart does at start: SDL2's video, a window
 * with OpenGL, a context (the newest core profile first, then compatibility, as Daemon tries them), a frame drawn and
 * read back. Prints the renderer and "drew ok", or SDL's error. (1.6.0 test helper; SDL2 is loaded at run time, so the
 * same binary runs in a Flatpak runtime, which has SDL2, and outside.) GPL-2.0-or-later. */
#include <stdio.h>
#include <dlfcn.h>
#include <SDL2/SDL.h>
#include <GL/gl.h>
#define F(n) static __typeof__(n) *p_##n
F(SDL_Init); F(SDL_GetError); F(SDL_CreateWindow); F(SDL_GL_SetAttribute); F(SDL_GL_CreateContext); F(SDL_GL_SwapWindow);
F(SDL_GL_GetProcAddress); F(SDL_GetCurrentVideoDriver); F(SDL_Quit);
int main(void)
{
	void *h = dlopen("libSDL2-2.0.so.0", RTLD_NOW); if (!h) { printf("no SDL2: %s\n", dlerror()); return 3; }
#define L(n) if (!(p_##n = dlsym(h, #n))) { printf("SDL2 without " #n "\n"); return 3; }
	L(SDL_Init); L(SDL_GetError); L(SDL_CreateWindow); L(SDL_GL_SetAttribute); L(SDL_GL_CreateContext); L(SDL_GL_SwapWindow);
	L(SDL_GL_GetProcAddress); L(SDL_GetCurrentVideoDriver); L(SDL_Quit);
	if (p_SDL_Init(SDL_INIT_VIDEO)) { printf("SDL video failed: %s\n", p_SDL_GetError()); return 1; }
	SDL_Window *w = p_SDL_CreateWindow("sdlgl", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if (!w) { printf("no OpenGL window: %s\n", p_SDL_GetError()); return 1; }
	static const int V[][3] = {{3, 3, SDL_GL_CONTEXT_PROFILE_CORE}, {3, 2, SDL_GL_CONTEXT_PROFILE_CORE}, {2, 1, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY}};
	SDL_GLContext c = NULL;
	for (unsigned i = 0; i < 3 && !c; i++) {
		p_SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, V[i][0]); p_SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, V[i][1]);
		p_SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, V[i][2]);
		if (!(c = p_SDL_GL_CreateContext(w))) printf("OpenGL %d.%d: %s\n", V[i][0], V[i][1], p_SDL_GetError());
	}
	if (!c) { puts("no OpenGL context (as \"Couldn't load a renderer.\")"); return 1; }
	const GLubyte *(*gs)(GLenum) = p_SDL_GL_GetProcAddress("glGetString");
	void (*cc)(GLfloat, GLfloat, GLfloat, GLfloat) = p_SDL_GL_GetProcAddress("glClearColor");
	void (*cl)(GLbitfield) = p_SDL_GL_GetProcAddress("glClear"); void (*fi)(void) = p_SDL_GL_GetProcAddress("glFinish");
	void (*rp)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *) = p_SDL_GL_GetProcAddress("glReadPixels");
	printf("sdl %s renderer: %s | version: %s\n", p_SDL_GetCurrentVideoDriver(), gs(GL_RENDERER), gs(GL_VERSION));
	cc(0, 1, 0, 1); cl(GL_COLOR_BUFFER_BIT); fi();
	unsigned char px[4] = {0}; rp(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px); p_SDL_GL_SwapWindow(w);
	printf(px[1] > 200 && px[0] < 50 ? "drew ok\n" : "drew wrong (%u %u %u)\n", px[0], px[1], px[2]);
	p_SDL_Quit(); return 0;
}
