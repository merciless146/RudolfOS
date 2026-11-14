# Carbon Fibre: a 2x2 twill carbon weave with a resin sheen, Rudolf's logo in brushed metal with a red nose.
# python3 carbon.py logo.json OUTDIR   -> carbon_d.png (dark), carbon_l.png (light)
import json,sys,subprocess,io,numpy as np
from PIL import Image,ImageFilter
L=json.load(open(sys.argv[1]));OUT=sys.argv[2]
W,H=1920,1080;rng=np.random.default_rng(7)
def weave(base,amp,cell=12):
    y,x=np.mgrid[0:H,0:W].astype(np.float32)
    # rotate the weave 45 degrees for the classic diagonal look
    u=x;v=y
    i=np.floor(u/cell);j=np.floor(v/cell);fu=u/cell-i;fv=v/cell-j
    horiz=((i-j)%4)<2   # 2x2 twill: the diagonal steps
    across=np.where(horiz,fv,fu)          # position across the tow (0..1)
    along=np.where(horiz,u,v)             # position along it
    bulge=np.sin(np.pi*across)**0.6        # the round tow catching light in the middle
    edge=np.clip(np.minimum(across,1-across)*8,0,1)  # dark gaps between tows
    fib=rng.standard_normal((H,W)).astype(np.float32)
    fib=np.array(Image.fromarray(((fib*20+128).clip(0,255)).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.6)),np.float32)/255-.5
    tone=np.where(horiz,1.0,0.62)          # the two directions reflect differently
    val=base+amp*tone*(0.35+0.65*bulge)*edge+amp*0.25*fib*edge
    return val
def sheen(img,strength):
    y,x=np.mgrid[0:H,0:W].astype(np.float32)
    d=((x/W)*0.8-(y/H)*0.55-0.12)          # a broad diagonal gloss across the resin
    g=np.exp(-(d/0.22)**2)*strength
    vig=1-0.55*(((x/W-.5)**2*1.2+(y/H-.5)**2*1.6))
    return img*vig[...,None]+g[...,None]
def logo_layer(fill_defs,fill,stroke,nose,glow,height):
    LW=L['w'];s=height/LW;ox=(W-LW*s)/2;oy=(H-LW*s)/2
    svg=f"""<svg xmlns='http://www.w3.org/2000/svg' width='{W}' height='{H}'><defs>{fill_defs}
<filter id='gl' x='-100%' y='-100%' width='300%' height='300%'><feGaussianBlur stdDeviation='{glow/s:.1f}'/></filter>
<filter id='sh' x='-20%' y='-20%' width='140%' height='140%'><feGaussianBlur stdDeviation='{7/s:.1f}'/></filter></defs>
<g transform='translate({ox:.1f},{oy:.1f}) scale({s:.4f})'>
<path d='{L['white']}' fill='#000' fill-opacity='.7' fill-rule='evenodd' transform='translate({3/s:.1f},{6/s:.1f})' filter='url(#sh)'/>
<path d='{L['white']}' fill='{fill}' fill-rule='evenodd' stroke='{stroke}' stroke-width='{1.2/s:.2f}'/>
<path d='{L['red']}' fill='{nose}' filter='url(#gl)'/>
<path d='{L['red']}' fill='{nose}'/></g></svg>"""
    open(OUT+'/_l.svg','w').write(svg);subprocess.run(['rsvg-convert',OUT+'/_l.svg','-o',OUT+'/_l.png'],check=True)
    return np.array(Image.open(OUT+'/_l.png').convert('RGBA'),np.float32)/255
def brushed(dark):
    # brushed metal: fine horizontal streaks as a pattern image the logo is filled with
    n=rng.standard_normal((H,W)).astype(np.float32)
    n=np.array(Image.fromarray(((n*30+128).clip(0,255)).astype(np.uint8)).filter(ImageFilter.BoxBlur(0)).resize((W,H)),np.float32)
    k=np.array(Image.fromarray(n.astype(np.uint8)).filter(ImageFilter.GaussianBlur((12,0)) if False else ImageFilter.BoxBlur(1)),np.float32)
    streak=np.array(Image.fromarray(n.astype(np.uint8)).resize((W//24,H)).resize((W,H),Image.BILINEAR),np.float32)/255-.5
    return streak
def make(mode):
    if mode=='d':
        v=weave(0.035,0.16);rgb=np.stack([v*0.96,v*0.98,v*1.06],-1);rgb=sheen(rgb,0.10)
        defs="<linearGradient id='m' x1='0' y1='0' x2='0' y2='1'><stop offset='0' stop-color='#f4f6f8'/><stop offset='.45' stop-color='#b9c0c8'/><stop offset='.55' stop-color='#8d949c'/><stop offset='1' stop-color='#d7dce1'/></linearGradient>"
        lg=logo_layer(defs,'url(#m)','#ffffff40','#ef1b24',14,560)
    else:
        v=weave(0.42,0.17);rgb=np.stack([v*0.98,v*1.0,v*1.04],-1);rgb=sheen(rgb,0.16)
        defs="<linearGradient id='m' x1='0' y1='0' x2='0' y2='1'><stop offset='0' stop-color='#3a3f46'/><stop offset='.45' stop-color='#1d2126'/><stop offset='.55' stop-color='#111418'/><stop offset='1' stop-color='#2b3036'/></linearGradient>"
        lg=logo_layer(defs,'url(#m)','#ffffff55','#e0161f',10,560)
    st=brushed(mode)[...,None]*0.10
    a=lg[...,3:4];logo=np.clip(lg[...,:3]+st*(lg[...,0:1]>0.2),0,1)
    out=rgb*(1-a)+logo*a
    Image.fromarray((np.clip(out,0,1)*255).astype(np.uint8)).save(f"{OUT}/carbon_{mode}.png")
for m in ('d','l'):make(m)
print('done')
