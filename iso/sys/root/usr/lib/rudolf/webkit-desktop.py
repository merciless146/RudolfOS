#!/usr/bin/env python3
# Rudolf OS 1.5.7 (plan Part 4, step 2): the desktop page in WebKit (WebKitGTK 4.1, in Debian) instead of Chromium,
# when Settings › System health › Desktop engine says WebKit. The session copies this file into Debian
# (/usr/local/lib/rudolf) and starts it there as the user, in the desktop's memory group:
#   python3 webkit-desktop.py --rudolf-kiosk --size=WxH http://127.0.0.1:8080/
# One window without frame, class RudolfDesktop (rudolf-display keeps it full-screen, as Chromium's), the page's data
# in ~/.rudolf-webkit (the desktop loads rudolfd's mirror of its databases at start, as the shell does). Websites
# open in Chromium's own window (the page sees RUDOLFD.engine == "webkit"). A crashed page is loaded again.
# GPL-2.0-or-later.
import os, sys
import gi
gi.require_version('Gtk', '3.0'); gi.require_version('Gdk', '3.0'); gi.require_version('WebKit2', '4.1')
from gi.repository import Gtk, Gdk, GLib, WebKit2

args = sys.argv[1:]
url = next((a for a in args if a.startswith('http://127.0.0.1:')), 'http://127.0.0.1:8080/')
size = next((a[7:] for a in args if a.startswith('--size=')), '')
GLib.set_prgname('RudolfDesktop'); Gdk.set_program_class('RudolfDesktop')
Gtk.init(None)
home = os.path.expanduser('~/.rudolf-webkit')
mgr = WebKit2.WebsiteDataManager(base_data_directory=home + '/data', base_cache_directory=home + '/cache')
ctx = WebKit2.WebContext.new_with_website_data_manager(mgr)
ctx.set_cache_model(WebKit2.CacheModel.DOCUMENT_VIEWER)   # the least memory for caches: the page is local
view = WebKit2.WebView.new_with_context(ctx)
st = view.get_settings()
for name, val in (('enable-javascript', True), ('javascript-can-access-clipboard', True), ('enable-webaudio', True),
                  ('enable-media-stream', True), ('enable-webgl', True), ('enable-developer-extras', False),
                  ('media-playback-requires-user-gesture', False), ('enable-back-forward-navigation-gestures', False),
                  ('allow-file-access-from-file-urls', False), ('enable-write-console-messages-to-stdout', True)):
    try: st.set_property(name, val)
    except TypeError: pass
st.set_hardware_acceleration_policy(WebKit2.HardwareAccelerationPolicy.ON_DEMAND)

def allowed(u):   # (a website: http or https elsewhere)
    return not (u.startswith('http:') or u.startswith('https:')) or u.startswith('http://127.0.0.1:') or u.startswith('http://localhost:')
def on_perm(v, req):   # the camera, the microphone, notifications: the desktop asks the person itself
    req.allow(); return True
def on_decide(v, decision, kind):
    # the desktop never leaves itself: anything else goes to Chromium's window through the page (window.open)
    if kind == WebKit2.PolicyDecisionType.NAVIGATION_ACTION:
        try: frame = decision.get_frame_name()   # (the page's own frames load what they like)
        except Exception: frame = None
        u = decision.get_navigation_action().get_request().get_uri()
        if not frame and not allowed(u): decision.ignore(); return True
    return False
def on_crash(v, reason):
    print('webkit-desktop: the page stopped (%s), loading it again' % reason, flush=True)
    GLib.timeout_add_seconds(1, lambda: (v.load_uri(url), False)[1])
view.connect('permission-request', on_perm)
view.connect('decide-policy', on_decide)
view.connect('web-process-terminated', on_crash)
view.connect('create', lambda v, nav: None)   # new windows: the page sends websites to Chromium's window
win = Gtk.Window(); win.set_decorated(False); win.set_title('Rudolf OS'); win.add(view)
scr = Gdk.Screen.get_default()
w, h = (int(x) for x in size.split('x')) if 'x' in size else (scr.get_width(), scr.get_height())
win.set_default_size(w, h); win.move(0, 0); win.fullscreen()
win.connect('destroy', Gtk.main_quit)
view.load_uri(url); win.show_all()
Gtk.main()
