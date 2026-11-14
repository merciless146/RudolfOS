# the Rudolf logo as SVG paths (white parts, red nose), from an image on black
import cv2,numpy as np,json,sys
from PIL import Image
im=np.array(Image.open(sys.argv[1]).convert('RGB')).astype(np.float32);H,W=im.shape[:2];S=2
big=cv2.resize(im,(W*S,H*S),interpolation=cv2.INTER_CUBIC);r,g,b=big[...,0],big[...,1],big[...,2]
red=((r>140)&(g<100)&(b<100)).astype(np.uint8)*255
white=((r+g+b)/3>128).astype(np.uint8)*255;white[red>0]=0
def paths(mask,minA):
    cs,_=cv2.findContours(mask,cv2.RETR_CCOMP,cv2.CHAIN_APPROX_NONE);d=[]
    for c in cs:
        if cv2.contourArea(c)<minA*S*S: continue
        p=cv2.approxPolyDP(c,1.6*S,True)[:,0,:]/S
        d.append('M'+' L'.join(f'{x:.1f},{y:.1f}' for x,y in p)+' Z')
    return ' '.join(d)
json.dump({'w':W,'h':H,'white':paths(white,60),'red':paths(red,60)},open(sys.argv[2],'w'))
