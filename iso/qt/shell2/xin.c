/* xin: real X input for tests (XTest). Steps, in order:
   move X Y | rel DX DY | down B | up B | click B | kdown KEYSYM | kup KEYSYM | key KEYSYM (Control_L, w, Return...) | kc KEYCODE (press and release) | sleep MS */
#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
extern int XTestFakeMotionEvent(Display*,int,int,int,unsigned long);
extern int XTestFakeRelativeMotionEvent(Display*,int,int,unsigned long);
extern int XTestFakeButtonEvent(Display*,unsigned,Bool,unsigned long);
extern int XTestFakeKeyEvent(Display*,unsigned,Bool,unsigned long);
#include <X11/keysym.h>
static unsigned kc(Display*d,const char*n){return XKeysymToKeycode(d,XStringToKeysym(n));}
int main(int c,char**v){Display*d=XOpenDisplay(0);if(!d)return 1;
 for(int i=1;i+1<c;){char*o=v[i];
  if(!strcmp(o,"move")){XTestFakeMotionEvent(d,-1,atoi(v[i+1]),atoi(v[i+2]),0);i+=3;}
  else if(!strcmp(o,"rel")){XTestFakeRelativeMotionEvent(d,atoi(v[i+1]),atoi(v[i+2]),0);i+=3;}
  else if(!strcmp(o,"down")){XTestFakeButtonEvent(d,atoi(v[i+1]),True,0);i+=2;}
  else if(!strcmp(o,"up")){XTestFakeButtonEvent(d,atoi(v[i+1]),False,0);i+=2;}
  else if(!strcmp(o,"click")){XTestFakeButtonEvent(d,atoi(v[i+1]),True,0);XFlush(d);usleep(60000);XTestFakeButtonEvent(d,atoi(v[i+1]),False,0);i+=2;}
  else if(!strcmp(o,"kdown")){XTestFakeKeyEvent(d,kc(d,v[i+1]),True,0);i+=2;}
  else if(!strcmp(o,"kup")){XTestFakeKeyEvent(d,kc(d,v[i+1]),False,0);i+=2;}
  else if(!strcmp(o,"key")){XTestFakeKeyEvent(d,kc(d,v[i+1]),True,0);XFlush(d);usleep(30000);XTestFakeKeyEvent(d,kc(d,v[i+1]),False,0);i+=2;}
  else if(!strcmp(o,"kc")){XTestFakeKeyEvent(d,atoi(v[i+1]),True,0);XFlush(d);usleep(30000);XTestFakeKeyEvent(d,atoi(v[i+1]),False,0);i+=2;}
  else if(!strcmp(o,"sleep")){XFlush(d);usleep(atoi(v[i+1])*1000);i+=2;}
  else break; XFlush(d);}
 XSync(d,0);return 0;}
