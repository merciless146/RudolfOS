"""
1.6.3 (docs/plan-1.6.3.md Part 2's last step): the stand-in rudolfd for iso/qt/host/fpuiprompt63_t.c -- librudolf-ui's
administrator prompt with a finger. What rudolfd-fprint.inc and rudolfd-desktop.inc answer, as far as that prompt asks:
the token (GET /), the settings, the session (Ann, an administrator), the accounts, fp/status (a finger is taken now),
fp/verify (a finger not recognised, then the right one with its one-time answer "t"), fp/cancel, and desktop/admin, which
takes the finger's answer once and only in Ann's name -- and the password "abcd", the test's own.
  python3 fpui_rd.py PORT DIR [match|hold]      (DIR/calls: every request, one a line)
Made-up fingerprints only; nothing here is anyone's.
"""
import json, sys, time
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT, D = int(sys.argv[1]), sys.argv[2]
MODE = sys.argv[3] if len(sys.argv) > 3 else "match"
TICKET = "0123456789abcdef0123456789abcdef"
STATE = {"verify": 0, "took": 0}


class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, *a):
        pass

    def out(self, code, body, ctype="application/json"):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def rec(self, what):
        with open(D + "/calls", "a") as f:
            f.write(what + "\n")

    def do_GET(self):
        if self.path == "/":
            self.out(200, 'window.RUDOLFD={token:"tok",port:%d};' % PORT, "text/html")
        elif self.path.startswith("/api/desktop/settings"):
            self.out(200, json.dumps({"ok": True, "gen": 1, "s": {"accent": "#2563eb", "fs": 13, "scale": 100}}))
        elif self.path.startswith("/api/desktop/session"):
            self.out(200, json.dumps({"ok": True, "id": "ann01", "n": "Ann", "r": "admin", "pk": "00" * 32, "fk": "00" * 32}))
        elif self.path.startswith("/api/desktop/accounts"):
            self.out(200, json.dumps({"ok": True, "accounts": [{"n": "Ann", "r": "admin"}]}))
        elif self.path.startswith("/api/fp/status"):
            self.rec("status " + self.path.split("acct=")[-1])
            self.out(200, json.dumps({"ok": True, "ready": MODE != "noready", "why": "", "signed": True, "fails": 0,
                                      "fingers": ["right-index"], "disk": True, "open": True,
                                      "readers": [{"name": "Virtual device", "driver": "virtual_image"}], "unsupported": []}))
        else:
            self.out(404, '{"error":"?"}')

    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(n).decode() if n else ""
        if self.path == "/api/fp/verify":
            STATE["verify"] += 1
            self.rec("verify " + body)
            if MODE == "hold" or STATE["verify"] == 1:
                time.sleep(3.0 if MODE == "hold" else 1.2)     # (a person reading the line, then a finger not theirs)
                if MODE == "hold":
                    self.out(403, json.dumps({"ok": False, "error": "That finger was not recognised.", "left": 4}))
                else:
                    self.out(403, json.dumps({"ok": False, "error": "That finger was not recognised.", "left": 4}))
                return
            time.sleep(0.8)
            self.out(200, json.dumps({"ok": True, "acct": "ann01", "n": "Ann", "finger": "right-index", "t": TICKET}))
        elif self.path == "/api/fp/cancel":
            self.rec("cancel")
            self.out(200, '{"ok":true}')
        elif self.path == "/api/desktop/admin":
            j = json.loads(body or "{}")
            p = j.get("p", "")
            self.rec("admin %s %s" % (j.get("n", ""), p))
            if p == "fp:" + TICKET:
                if STATE["took"]:
                    self.out(403, json.dumps({"error": "Wrong password."}))
                else:
                    STATE["took"] = 1
                    self.out(200, '{"ok":true}')
            elif p == "abcd":
                self.out(200, '{"ok":true}')
            else:
                self.out(403, json.dumps({"error": "Wrong password."}))
        else:
            self.out(404, '{"error":"?"}')


HTTPServer(("127.0.0.1", PORT), H).serve_forever()
