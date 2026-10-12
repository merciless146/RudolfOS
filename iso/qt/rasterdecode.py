# Decode PWG raster (RaS2) or Apple raster (UNIRAST) into PNG pages, checking the stream on the way.
#   python3 rasterdecode.py FILE OUTPREFIX     prints one line per page: width height dpi bpp
import struct, sys
from PIL import Image
d = open(sys.argv[1], 'rb').read(); pre = sys.argv[2]
def unpack(d, i, w, h, bpp, urf):
    row = w * bpp; out = bytearray(row * h); y = 0
    while y < h:
        rep = d[i] + 1; i += 1; line = bytearray();
        while len(line) < row:
            c = d[i]; i += 1
            if urf and c == 0x80: line += b'\xff' * (row - len(line)); break
            if c < 128: px = d[i:i+bpp]; i += bpp; line += px * (c + 1)
            else: n = 257 - c; line += d[i:i+n*bpp]; i += n * bpp
        assert len(line) == row, f'line {y}: {len(line)} != {row}'
        for _ in range(rep):
            assert y < h, 'too many lines'
            out[y*row:(y+1)*row] = line; y += 1
    return out, i
if d[:4] == b'RaS2':
    i = 4; n = 0
    while i < len(d):
        h = d[i:i+1796]; i += 1796; g = lambda o: struct.unpack('>I', h[o:o+4])[0]
        w, ht, dpi, bpp = g(372), g(376), g(276), g(388) // 8
        assert h[:9] == b'PwgRaster' and g(392) == w * bpp and g(400) in (18, 19)
        px, i = unpack(d, i, w, ht, bpp, False); n += 1
        Image.frombytes('L' if bpp == 1 else 'RGB', (w, ht), bytes(px)).save(f'{pre}-{n}.png')
        print(w, ht, dpi, bpp, 'pages-total', g(452), 'size', h[1732:1796].rstrip(b'\0').decode(), 'duplex', g(272), 'feed', struct.unpack('>i', h[460:464])[0])
elif d[:8] == b'UNIRAST\0':
    pages = struct.unpack('>I', d[8:12])[0]; i = 12
    for n in range(1, pages + 1):
        h = d[i:i+32]; i += 32; bpp = h[0] // 8; w, ht, dpi = struct.unpack('>III', h[12:24])
        px, i = unpack(d, i, w, ht, bpp, True)
        Image.frombytes('L' if bpp == 1 else 'RGB', (w, ht), bytes(px)).save(f'{pre}-{n}.png')
        print(w, ht, dpi, bpp, 'colorspace', h[1], 'duplex', h[2])
    assert i == len(d), 'trailing bytes'
else: sys.exit('unknown format')
