# 1.6.0 (Part 14): a stand-in for the desktop (and rudolfd) answering the keyring's prompts that rudolf-portal asks in
# /run/rudolf/portal/req: each question kept in LOG (one JSON per line), answered with the next line of ANSWERS
# (password:TEXT, yes, cancel, wait: leave it), written as rudolfd writes it (0600, then the question removed).
#   python3 krdesk.py ANSWERS LOG
import json, os, sys, time
R = "/run/rudolf/portal"; ans, log = sys.argv[1], sys.argv[2]; seen = set()
while True:
    for f in sorted(os.listdir(R + "/req")):
        if not (f.startswith("k") and f.endswith(".json")) or f in seen: continue
        seen.add(f)
        try: q = json.load(open(R + "/req/" + f))
        except Exception: seen.discard(f); continue
        with open(log, "a") as l: l.write(json.dumps(q) + "\n")
        lines = open(ans).read().split("\n"); a = lines[0] if lines else ""; open(ans, "w").write("\n".join(lines[1:]))
        if a == "wait" or not a: continue
        r = {"response": 0, "password": a[9:]} if a.startswith("password:") else {"response": 0} if a == "yes" else {"response": 1}
        t = R + "/resp/" + f + ".tmp"; fd = os.open(t, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        os.write(fd, json.dumps(r).encode()); os.close(fd); os.rename(t, R + "/resp/" + f)
        try: os.unlink(R + "/req/" + f)
        except OSError: pass
    time.sleep(0.1)
