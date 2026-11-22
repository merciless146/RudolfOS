#!/usr/bin/env python3
# Helpers for Emu's emulator test (nativerun_t.sh). GPL-2.0-or-later.
#   nrhelp.py pad FIFO     a virtual gamepad (uinput, as an Xbox 360 controller: SDL knows its layout); prints its
#                          /dev/input/eventN, then takes lines from FIFO: "down A", "up START", "quit"
#   nrhelp.py close WIN    asks window WIN (an X window id) to close, as a title bar's close button does (WM_DELETE_WINDOW)
#   nrhelp.py fullscreen WIN   prints "yes" if WIN asks for full screen (_NET_WM_STATE_FULLSCREEN), else "no"
import sys, os

def pad(fifo):
    from evdev import UInput, ecodes as e, AbsInfo
    keys = [e.BTN_A, e.BTN_B, e.BTN_X, e.BTN_Y, e.BTN_TL, e.BTN_TR, e.BTN_SELECT, e.BTN_START, e.BTN_MODE, e.BTN_THUMBL, e.BTN_THUMBR]
    st = AbsInfo(0, -32768, 32767, 16, 128, 0); tr = AbsInfo(0, 0, 255, 0, 0, 0); hat = AbsInfo(0, -1, 1, 0, 0, 0)
    cap = {e.EV_KEY: keys, e.EV_ABS: [(e.ABS_X, st), (e.ABS_Y, st), (e.ABS_RX, st), (e.ABS_RY, st), (e.ABS_Z, tr), (e.ABS_RZ, tr), (e.ABS_HAT0X, hat), (e.ABS_HAT0Y, hat)]}
    ui = UInput(cap, name='Microsoft X-Box 360 pad', vendor=0x045e, product=0x028e, version=0x110, bustype=e.BUS_USB)
    os.chmod(ui.device.path, 0o666)
    print(ui.device.path, flush=True)
    names = {'A': e.BTN_A, 'B': e.BTN_B, 'X': e.BTN_X, 'Y': e.BTN_Y, 'START': e.BTN_START, 'SELECT': e.BTN_SELECT}
    while True:
        with open(fifo) as f:
            for line in f:
                w = line.split()
                if not w: continue
                if w[0] == 'quit': ui.close(); return
                if w[0] in ('down', 'up') and len(w) > 1 and w[1] in names:
                    ui.write(e.EV_KEY, names[w[1]], 1 if w[0] == 'down' else 0); ui.syn()

def xwin(w):
    from Xlib import display, X, protocol
    d = display.Display(); return d, d.create_resource_object('window', int(w, 0)), X, protocol

def close(w):
    d, win, X, protocol = xwin(w)
    ev = protocol.event.ClientMessage(window=win, client_type=d.intern_atom('WM_PROTOCOLS'), data=(32, [d.intern_atom('WM_DELETE_WINDOW'), X.CurrentTime, 0, 0, 0]))
    win.send_event(ev); d.flush()

def fullscreen(w):
    d, win, X, protocol = xwin(w)
    p = win.get_full_property(d.intern_atom('_NET_WM_STATE'), X.AnyPropertyType)
    print('yes' if p and d.intern_atom('_NET_WM_STATE_FULLSCREEN') in p.value else 'no')

if __name__ == '__main__':
    {'pad': pad, 'close': close, 'fullscreen': fullscreen}[sys.argv[1]](sys.argv[2])
