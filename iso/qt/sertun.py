# sertun's host side (test scaffolding, see sertun.c): reads frames from the guest's serial port (a QEMU unix socket)
# and relays each connection to HOST:PORT here. python3 sertun.py SERIAL-SOCKET HOST PORT
import os, select, socket, struct, sys, time
V = os.environ.get('SERTUN_LOG')
def log(*a):
    if V: open(V, 'a').write(' '.join(str(x) for x in a) + '\n')

SOCK, HOST, PORT = sys.argv[1], sys.argv[2], int(sys.argv[3])


def main():
    while True:
        try:
            t = socket.socket(socket.AF_UNIX); t.connect(SOCK); log('connected to', SOCK)
        except OSError:
            time.sleep(1); continue
        buf = b''; up = None
        def send(kind, data=b''): t.sendall(b'STN' + kind + struct.pack('>I', len(data)) + data)
        try:
            while True:
                fds = [t] + ([up] if up else [])
                r, _, _ = select.select(fds, [], [], 1)
                if up in r:
                    d = up.recv(65536)
                    if d: send(b'D', d)
                    else: up.close(); up = None; send(b'C')
                if t in r:
                    d = t.recv(65536); log('from guest', len(d))
                    if not d: raise OSError('the guest went away')
                    buf += d
                    while True:
                        i = buf.find(b'STN')     # skip what else the port carries (the firmware's screen)
                        if i < 0: buf = buf[-2:]; break
                        buf = buf[i:]
                        if len(buf) < 8: break
                        kind = buf[3:4]; n = struct.unpack('>I', buf[4:8])[0]
                        if kind not in (b'O', b'D', b'C') or n > 65536: buf = buf[3:]; continue
                        if len(buf) < 8 + n: break
                        data, buf = buf[8:8 + n], buf[8 + n:]
                        if kind == b'O':
                            if up: up.close()
                            try: up = socket.create_connection((HOST, PORT))
                            except OSError: up = None; send(b'C')
                        elif kind == b'D' and up: up.sendall(data)
                        elif kind == b'C':
                            if up: up.close(); up = None; send(b'C')
        except OSError as e:
            log('error', e)
            try: t.close()
            except OSError: pass
            if up: up.close()
            time.sleep(1)


main()
