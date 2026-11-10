"""1.5.7 (PT_PY: the python to start each child with, as its argv[0] is not a path): a stand-in process tree for the memory guard's host test. Each node is a process: its name as the kernel
gives it (comm), its command line's first word (argv[0]), MiB of memory it holds, and its children. Each child is
started with execv, so its argv[0] is the one given.   python3 proctree.py 'JSON'   (the root's pid is printed)"""
import ctypes, json, os, sys, time
def run(node):
    ctypes.CDLL(None).prctl(15, node['n'].encode()[:15], 0, 0, 0)   # PR_SET_NAME
    keep = bytearray(int(node.get('m', 0)) << 20)
    for i in range(0, len(keep), 4096): keep[i] = 1
    for c in node.get('c', []):
        if os.fork() == 0:
            os.execv(os.environ['PT_PY'], [c.get('a', c['n']), os.path.abspath(__file__), json.dumps(c)])
    signal = __import__('signal')
    if node.get('stubborn'): signal.signal(signal.SIGTERM, signal.SIG_IGN)
    while True: time.sleep(600)
spec = json.loads(sys.argv[-1])
run(spec)
