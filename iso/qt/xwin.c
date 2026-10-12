/* test client: a small red window with class "TestGame"; prints its geometry every second */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <unistd.h>
int main(void){Display*d=XOpenDisplay(NULL);if(!d)return 1;int s=DefaultScreen(d);
 Window w=XCreateSimpleWindow(d,RootWindow(d,s),50,50,320,200,0,0,0xd02020);
 XClassHint ch={"testgame","TestGame"};XSetClassHint(d,w,&ch);XStoreName(d,w,"Test game");XMapWindow(d,w);XFlush(d);
 for(int i=0;i<600;i++){sleep(1);XWindowAttributes a;XGetWindowAttributes(d,w,&a);Window f;int r;XGetInputFocus(d,&f,&r);
  printf("%d %d %dx%d focus=%d\n",a.x,a.y,a.width,a.height,f==w);fflush(stdout);}
 return 0;}
