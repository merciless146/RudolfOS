import socket,time,sys
class Ser:
    def __init__(s,path):
        for i in range(100):
            try:
                s.s=socket.socket(socket.AF_UNIX);s.s.connect(path);break
            except Exception: time.sleep(0.2)
        s.s.settimeout(0.3);s.buf=b''
    def read(s,t=1.0):
        end=time.time()+t
        while time.time()<end:
            try:
                d=s.s.recv(65536)
                if d: s.buf+=d
            except socket.timeout: pass
        out=s.buf.decode('utf-8','replace');s.buf=b'';return out
    def run(s,cmd,t=3):
        s.s.send((cmd+'\n').encode());return s.read(t)
