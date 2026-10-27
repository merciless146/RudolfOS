# sertun's host side (test scaffolding, see sertun.c): reads frames from the guest's serial port (a QEMU unix socket)
# and relays each connection to HOST:PORT here. python3 sertun.py SERIAL-SOCKET HOST PORT
import os, select, socket, struct, sys, time

SOCK, HOST, PORT = sys.argv[1], sys.argv[2], int(sys.argv[3])


def main():
    while True:
        try:
            t = socket.socket(socket.AF_UNIX); t.connect(SOCK)
        except OSError:
            time.sleep(1); continue
        buf = b''; up = None
        def send(kind, data=b''): t.sendall(kind + struct.pack('>I', len(data)) + data)
        try:
            while True:
                fds = [t] + ([up] if up else [])
                r, _, _ = select.select(fds, [], [], 1)
                if up in r:
                    d = up.recv(65536)
                    if d: send(b'D', d)
                    else: up.close(); up = None; send(b'C')
                if t in r:
                    d = t.recv(65536)
                    if not d: raise OSError('the guest went away')
                    buf += d
                    while len(buf) >= 5:
                        n = struct.unpack('>I', buf[1:5])[0]
                        if len(buf) < 5 + n: break
                        kind, data, buf = buf[:1], buf[5:5 + n], buf[5 + n:]
                        if kind == b'O':
                            if up: up.close()
                            try: up = socket.create_connection((HOST, PORT))
                            except OSError: up = None; send(b'C')
                        elif kind == b'D' and up: up.sendall(data)
                        elif kind == b'C':
                            if up: up.close(); up = None; send(b'C')
        except OSError:
            try: t.close()
            except OSError: pass
            if up: up.close()
            time.sleep(1)


main()
