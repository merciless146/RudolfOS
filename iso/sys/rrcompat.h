/* The parts of <X11/extensions/Xrandr.h> rudolf-display uses, for building where libxrandr-dev is not
   installed (the library itself, libXrandr.so.2, is part of the base layer). Same layout as libXrandr 1.5;
   the release workflow installs libxrandr-dev and uses the real header. MIT licence, as libXrandr. */
#ifndef RRCOMPAT_H
#define RRCOMPAT_H
#if __has_include(<X11/extensions/Xrandr.h>)
#include <X11/extensions/Xrandr.h>
#else
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
typedef XID RROutput;
typedef XID RRCrtc;
typedef XID RRMode;
typedef unsigned long XRRModeFlags;
typedef unsigned short Rotation;
typedef unsigned short Connection;
typedef unsigned short SubpixelOrder;
#define RR_Rotate_0 1
#define RR_Connected 0
#define RR_Interlace 0x00000010
#define RR_DoubleScan 0x00000020
typedef struct _XRRModeInfo {
	RRMode id; unsigned int width, height; unsigned long dotClock;
	unsigned int hSyncStart, hSyncEnd, hTotal, hSkew, vSyncStart, vSyncEnd, vTotal;
	char *name; unsigned int nameLength; XRRModeFlags modeFlags;
} XRRModeInfo;
typedef struct _XRRScreenResources {
	Time timestamp, configTimestamp; int ncrtc; RRCrtc *crtcs; int noutput; RROutput *outputs; int nmode; XRRModeInfo *modes;
} XRRScreenResources;
typedef struct _XRROutputInfo {
	Time timestamp; RRCrtc crtc; char *name; int nameLen; unsigned long mm_width, mm_height; Connection connection;
	SubpixelOrder subpixel_order; int ncrtc; RRCrtc *crtcs; int nclone; RROutput *clones; int nmode, npreferred; RRMode *modes;
} XRROutputInfo;
typedef struct _XRRCrtcInfo {
	Time timestamp; int x, y; unsigned int width, height; RRMode mode; Rotation rotation; int noutput; RROutput *outputs;
	Rotation rotations; int npossible; RROutput *possible;
} XRRCrtcInfo;
Bool XRRQueryExtension(Display *, int *, int *);
Status XRRQueryVersion(Display *, int *, int *);
XRRScreenResources *XRRGetScreenResourcesCurrent(Display *, Window);
void XRRFreeScreenResources(XRRScreenResources *);
XRROutputInfo *XRRGetOutputInfo(Display *, XRRScreenResources *, RROutput);
void XRRFreeOutputInfo(XRROutputInfo *);
XRRCrtcInfo *XRRGetCrtcInfo(Display *, XRRScreenResources *, RRCrtc);
void XRRFreeCrtcInfo(XRRCrtcInfo *);
Status XRRSetCrtcConfig(Display *, XRRScreenResources *, RRCrtc, Time, int, int, RRMode, Rotation, RROutput *, int);
void XRRSetScreenSize(Display *, Window, int, int, int, int);
void XRRSetCrtcTransform(Display *, RRCrtc, XTransform *, const char *, XFixed *, int);
#endif
#endif
