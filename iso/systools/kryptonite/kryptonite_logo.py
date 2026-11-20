# Rudolf Kryptonite's logo (2.0): Rudolf's logo in carbon-fibre off-black with a glowing green nose.
# The owner, 2026-11-20: "Rudolf's logo, but ... carbon-fibre off-black instead of white. The glowing red nose will be
# green instead." Drawn here from ../backgrounds59/rudolf-logo.json (the logo's own traced paths); nothing else is used.
# python3 kryptonite_logo.py ../backgrounds59/rudolf-logo.json OUTDIR [SIZE]
#   -> kryptonite-logo.png (transparent, SIZE x SIZE, default 1024) and kryptonite-logo-256.png
import json,sys,subprocess,os,tempfile,numpy as np
from PIL import Image,ImageFilter
L=json.load(open(sys.argv[1]));OUT=sys.argv[2];S=int(sys.argv[3]) if len(sys.argv)>3 else 1024
GREEN='#2bff6a'          # the nose; its glow a little deeper
GLOW='#14e04e'
rng=np.random.default_rng(11)

def render(svg,size):
    with tempfile.TemporaryDirectory() as t:
        p=os.path.join(t,'l.svg');o=os.path.join(t,'l.png');open(p,'w').write(svg)
        subprocess.run(['rsvg-convert','-w',str(size),'-h',str(size),p,'-o',o],check=True)
        return np.array(Image.open(o).convert('RGBA'),np.float32)/255

def mask(paths,size,pad,blur=0):
    LW=L['w'];inner=size-2*pad;s=inner/LW
    flt=f"<filter id='b' x='-100%' y='-100%' width='300%' height='300%'><feGaussianBlur stdDeviation='{blur/s:.1f}'/></filter>" if blur else ''
    body=''.join(f"<path d='{d}' fill='#fff' fill-rule='evenodd'{' filter=\"url(#b)\"' if blur else ''}/>" for d in paths)
    svg=f"<svg xmlns='http://www.w3.org/2000/svg' width='{size}' height='{size}'><defs>{flt}</defs><g transform='translate({pad},{pad}) scale({s:.5f})'>{body}</g></svg>"
    return render(svg,size)[...,3]

def weave(size,base,amp,cell):
    # 2x2 twill carbon weave, as backgrounds59/carbon.py draws it, sized to the logo
    y,x=np.mgrid[0:size,0:size].astype(np.float32)
    i=np.floor(x/cell);j=np.floor(y/cell);fu=x/cell-i;fv=y/cell-j
    horiz=((i-j)%4)<2
    across=np.where(horiz,fv,fu)
    bulge=np.sin(np.pi*across)**0.6
    edge=np.clip(np.minimum(across,1-across)*8,0,1)
    fib=rng.standard_normal((size,size)).astype(np.float32)
    fib=np.array(Image.fromarray(((fib*20+128).clip(0,255)).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.6)),np.float32)/255-.5
    tone=np.where(horiz,1.0,0.62)
    v=base+amp*tone*(0.35+0.65*bulge)*edge+amp*0.25*fib*edge
    # the resin's diagonal gloss
    d=(x/size)*0.8-(y/size)*0.55-0.05
    v=v+np.exp(-(d/0.25)**2)*0.07
    return np.stack([v*0.97,v*0.99,v*1.05],-1)

def make(size):
    pad=round(size*0.06)
    body=mask([L['white']],size,pad)
    nose=mask([L['red']],size,pad)
    glow=mask([L['red']],size,pad,blur=size*0.03)
    # a thin light rim (the resin's edge catching light) so the off-black reads on dark backgrounds too
    rim=np.clip(np.array(Image.fromarray((body*255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3)),np.float32)/255-body,0,1)
    carbon=weave(size,0.07,0.14,max(4,size//110))
    rgb=np.zeros((size,size,3),np.float32);a=np.zeros((size,size),np.float32)
    g=np.array([int(GLOW[k:k+2],16) for k in (1,3,5)],np.float32)/255
    n=np.array([int(GREEN[k:k+2],16) for k in (1,3,5)],np.float32)/255
    # premultiplied colour (rgb) and alpha (a), layered bottom to top
    ga=np.clip(glow*1.6,0,1)*0.8                        # the glow around the nose
    rgb=g*ga[...,None];a=ga.copy()
    rgb=rgb*(1-body[...,None])+carbon*body[...,None];a=a*(1-body)+body
    ra=rim*0.7;rgb=rgb*(1-ra[...,None])+np.array([.66,.68,.72])*ra[...,None];a=a*(1-ra)+ra
    gl=np.clip(glow*1.6,0,1)*0.45*body                  # the glow lighting the carbon around the nose
    rgb=rgb+g*gl[...,None]
    # the nose: green, brighter in its middle
    core=np.clip(mask([L['red']],size,pad,blur=size*0.006),0,1)
    nc=n*(0.85+0.15*core[...,None])+np.array([.75,1,.8])*0.25*core[...,None]**3
    rgb=rgb*(1-nose[...,None])+np.clip(nc,0,1)*nose[...,None];a=a*(1-nose)+nose
    # straight alpha for the PNG
    out=np.dstack([np.clip(rgb/np.maximum(a[...,None],1e-4),0,1),a])
    return Image.fromarray((np.clip(out,0,1)*255).astype(np.uint8),'RGBA')

big=make(S);big.save(os.path.join(OUT,'kryptonite-logo.png'))
big.resize((256,256),Image.LANCZOS).save(os.path.join(OUT,'kryptonite-logo-256.png'))
print('done')
