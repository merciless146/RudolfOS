# pixels of a screenshot: python3 px.py IMG x,y x,y ... -> JSON list of [r,g,b]
import sys, json
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
print(json.dumps([list(im.getpixel(tuple(int(float(v)) for v in a.split(',')))) for a in sys.argv[2:]]))
