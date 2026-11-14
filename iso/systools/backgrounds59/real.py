# Moonlit Stag, refined: a more realistic night (d) and dawn (l) over a mountain lake, Rudolf (the white low-poly stag,
# red nose) on a boulder. Everything is procedural (numpy noise, polygons); nothing taken from other pictures.
# python3 real.py OUTDIR
import sys,subprocess,numpy as np,cv2
def blur(a,sg):return cv2.GaussianBlur(np.ascontiguousarray(a,np.float32),(0,0),sg)
def shift(a,m):return cv2.warpAffine(np.ascontiguousarray(a,np.float32),np.float32(m),(a.shape[1],a.shape[0]))
from PIL import Image,ImageDraw,ImageFilter
from stagdef2 import stag,HOOVES,NOSE
OUT=sys.argv[1];W,H=1920,1080;HZ=690;SS=2
def fbm1(n,oct,seed,base=0.004):
    r=np.random.default_rng(seed);x=np.arange(n,dtype=np.float64);v=np.zeros(n)
    for o in range(oct):
        f=base*2**o;k=int(n*f)+3;pts=r.standard_normal(k);xi=x*f;i=np.floor(xi).astype(int);t=xi-i;t=t*t*(3-2*t)
        v+=(pts[i]*(1-t)+pts[i+1]*t)/(1.9**o)
    return v
def fbm2(h,w,oct,seed,scale=1/180):
    r=np.random.default_rng(seed);v=np.zeros((h,w),np.float32)
    for o in range(oct):
        s=scale*2**o;gh=int(h*s)+3;gw=int(w*s)+3;g=r.standard_normal((gh,gw)).astype(np.float32)
        a=cv2.resize(g,(int(gw/s),int(gh/s)),interpolation=cv2.INTER_CUBIC)[:h,:w]
        v+=a/(2**o)
    return v/1.9
def hexc(h):h=h.lstrip('#');return np.array([int(h[i:i+2],16)/255 for i in (0,2,4)],np.float32)
def lerp(a,b,t):return a+(b-a)*t
def grad(stops,n):
    ys=np.linspace(0,1,n);out=np.zeros((n,3),np.float32)
    for k in range(3):out[:,k]=np.interp(ys,[s for s,_ in stops],[hexc(c)[k] for _,c in stops])
    return out
PAL={'d':dict(sky=[(0,'#03050e'),(.5,'#0b1430'),(.85,'#1d2b55'),(1,'#33427a')],moon=(560,205,58),moonc='#f2efe6',halo='#7f95d8',halo_s=.55,
        far='#2a3765',mid='#1b2548',near='#111a37',snow='#b9c6ee',fog='#5a6ca8',fogA=.22,tree=['#121b3a','#0b1229','#050912'],water='#0a1128',
        rock='#0c1124',rim='#9fb4ff',stars=True,glit='#e9eefc',stagL='#f3f6ff',stagS='#b9c6e8',grain=.028),
     'l':dict(sky=[(0,'#5b7fc9'),(.55,'#a8b3e0'),(.85,'#f0c4c2'),(1,'#ffd9ae')],moon=(560,330,50),moonc='#fff3d6',halo='#ffcf96',halo_s=.6,
        far='#7e88bb',mid='#636ca3',near='#4b538a',snow='#f6f1f4',fog='#f2e4ee',fogA=.35,tree=['#4a5186','#353b6c','#20244a'],water='#7c84b8',
        rock='#262a4f',rim='#ffd7a8',stars=False,glit='#fff1d6',stagL='#ffffff',stagS='#cfd4ee',grain=.02)}
def scene(m):
    c=PAL[m];rng=np.random.default_rng(3);img=np.zeros((H,W,3),np.float32)
    yy,xx=np.mgrid[0:H,0:W].astype(np.float32)
    # sky
    g=grad(c['sky'],HZ);img[:HZ]=g[:,None,:]
    mx,my,mr=c['moon'];d=np.sqrt((xx-mx)**2+(yy-my)**2)
    halo=np.exp(-d/ (260))*c['halo_s']+np.exp(-d/70)*0.35*c['halo_s'];img[:HZ]+=(halo[:HZ,:,None]*hexc(c['halo']))
    if c['stars']:
        mw=fbm2(H,W,5,9,1/300);band=np.exp(-((yy*0.9-xx*0.42+260)/170)**2);neb=np.clip(mw*0.6+0.4,0,1)*band*0.16
        img[:HZ]+=neb[:HZ,:,None]*hexc('#9aa9e0')
        n=1500;sx=rng.uniform(0,W,n);sy=rng.uniform(0,HZ-60,n);br=rng.power(9,n)**3*1.2+0.04
        br*=1+3*np.exp(-((sy*0.9-sx*0.42+260)/170)**2)*rng.uniform(0,1,n)
        st=np.zeros((H,W),np.float32);np.add.at(st,(sy.astype(int),sx.astype(int)),br)
        st=blur(st,0.7)*2.2
        fade=np.clip((HZ-80-yy)/260,0,1)
        img+=(st*fade)[...,None]*hexc('#eef2ff')
    # moon (or sun) disc with texture
    disc=np.clip(mr-d+1,0,1)
    if m=='d':
        tex=fbm2(H,W,5,21,1/40);limb=np.sqrt(np.clip(1-(d/mr)**2,0,1))
        mc=hexc(c['moonc'])*(0.78+0.22*limb)[...,None]*(0.9+0.12*np.clip(tex,-1,1))[...,None]
    else:
        mc=np.broadcast_to(hexc(c['moonc']),(H,W,3))
    img=img*(1-disc[...,None])+mc*disc[...,None]
    # mountains: three ridgelines, snow on the far one, shading from noise
    def ridge(y0,amp,oct,seed,base):return y0-amp*(fbm1(W,oct,seed,base)*0.5+0.5)
    tex=fbm2(H,W,6,31,1/90)
    hf=1-np.abs(fbm2(H,W,6,32,1/70));hf=blur(hf,1.2);gy_,gx_=np.gradient(hf)
    Ld=np.array([np.sign(mx-W/2)*0.7,-0.7]);relief=np.clip(0.5+(gx_*Ld[0]+gy_*Ld[1])*9,0,1)
    layers=[(ridge(560,230,7,1,0.0016),c['far'],0.0),(ridge(625,130,7,2,0.0026),c['mid'],0.35),(ridge(668,55,6,3,0.004),c['near'],0.7)]
    for k,(rl,col,depth) in enumerate(layers):
        mask=(yy>=rl[None,:]);mask[HZ+2:]=False
        aa=np.clip(yy-rl[None,:]+0.5,0,1)*(yy<=HZ+2)
        sm=cv2.GaussianBlur(rl.reshape(1,-1).astype(np.float32),(0,0),6).ravel();shade=np.clip(np.gradient(sm),-3,3)[None,:]
        near=np.exp(-np.clip(yy-rl[None,:],0,None)/90)   # the light on the slopes facing the moon, strongest near the crest
        lit=np.clip(-shade*np.sign(xx-mx)*0.5,0,1)*near*0.22
        col3=hexc(col)*((0.62+0.75*relief[...,None]) if m=='d' else (0.74+0.42*relief[...,None]))**(1-0.6*depth)*(0.94+0.1*tex[...,None])+lit[...,None]*hexc(c['rim'])*(1-depth)
        if k==0:
            snow=np.clip(1-(yy-rl[None,:])/(18+16*np.clip(tex,0,1)),0,1)*np.clip(1.4*(tex+0.2),0,1)*np.clip((440-rl[None,:])/60,0,1)
            col3=lerp(col3,hexc(c['snow'])*(0.55+0.45*np.clip(0.5-shade*np.sign(xx-mx)*0.3,0,1))[...,None],snow[...,None]*0.6)
        # fog: farther layers fade toward the horizon colour
        hz=g[-1];fogk=np.clip((yy-rl[None,:])/180,0,1)*(0.45 if k<2 else 0.25)
        col3=lerp(col3,hz,fogk[...,None])
        img=img*(1-aa[...,None])+col3*aa[...,None]
        # a band of mist in front of each layer
        mist=np.exp(-((yy-(np.median(rl)+60))/38)**2)*np.clip(0.5+0.5*fbm2(H,W,4,40+k,1/220),0,1)*c['fogA']
        img=lerp(img,np.broadcast_to(hexc(c['fog']),img.shape),(mist*(yy<HZ))[...,None])
    # forest along the shore: many spruces, nearer ones darker and larger
    def spruce(dr,x,base,h,col,r):
        pts=[];n=int(h/3.5)
        for i in range(n+1):
            t=i/n;y=base-h*t;w=h*0.17*(1-t)**1.05*(0.75+0.5*r.random())+1.2
            pts.append((x-w*(1.15 if i%2 else 0.7),y))
        right=[];
        for i in range(n,-1,-1):
            t=i/n;y=base-h*t;w=h*0.17*(1-t)**1.05*(0.75+0.5*r.random())+1.2
            right.append((x+w*(1.15 if i%2 else 0.7),y))
        dr.polygon([(px*SS,py*SS) for px,py in pts+[(x,base-h-h*0.04)]+right],fill=col)
    import random
    for li,(cnt,hmin,hmax,y0,col,fog) in enumerate([(420,22,48,HZ-6,c['tree'][0],0.45),(260,40,90,HZ+1,c['tree'][1],0.2),(60,140,300,H+20,c['tree'][2],0)]):
        lay=Image.new('L',(W*SS,H*SS),0);dr=ImageDraw.Draw(lay);r=random.Random(50+li)
        for _ in range(cnt):
            x=r.uniform(0,W)
            if li==2 and 330<x<1150:continue    # the foreground trees only at the sides
            if li==2 and 1150<x<1580 and r.random()<0.85:continue
            spruce(dr,x,y0+r.uniform(-3,4),r.uniform(hmin,hmax),255,r)
        a=np.asarray(lay.resize((W,H),Image.LANCZOS),np.float32)/255
        if li==2:a[:HZ]=a[:HZ]   # (near trees stand in front of everything)
        colr=lerp(hexc(col),g[-1],fog)*(0.9+0.1*tex[...,None])
        if li<2:a[HZ+2:]=0
        if li<2:img=img*(1-a[...,None])+colr*a[...,None]
        else:near_trees=(a,colr)
    above=img[:HZ].copy()
    # the lake: the scene above mirrored, rippled and darkened, with the moon's glitter path
    k=np.arange(H-HZ,dtype=np.float32);src=np.clip(HZ-1-k*1.0,0,HZ-1).astype(int)
    rip=fbm2(H-HZ,W,4,77,1/30)*(1+k[:,None]/90)*2.2
    cols=np.clip(xx[:H-HZ]+rip,0,W-1).astype(int)
    refl=above[src[:,None],cols]
    depth=np.clip(k/(H-HZ),0,1)[:,None,None]
    water=lerp(refl*0.62,hexc(c['water']),0.25+0.45*depth)
    hn=cv2.resize(np.random.default_rng(88).random(((H-HZ)//2,W//16)).astype(np.float32),(W,H-HZ),interpolation=cv2.INTER_LINEAR)   # horizontal glints
    gl=np.exp(-((xx[:H-HZ]-mx)/(22+k[:,None]*0.45))**2)*np.clip((hn-0.62)*4,0,1)*np.clip(1.1-depth[...,0]*0.7,0,1)
    water+=gl[...,None]*hexc(c['glit'])*(0.9 if m=='d' else 0.6)
    img[HZ:]=water
    img[HZ:HZ+2]=lerp(img[HZ:HZ+2],g[-1],0.4)
    # foreground trees
    a,colr=near_trees;img=img*(1-a[...,None])+colr*a[...,None]
    # the boulder, with texture and the moon's light on its left edges
    bl=Image.new('L',(W*SS,H*SS),0);db=ImageDraw.Draw(bl)
    SX,SY,SC=1202,535.6,2.2;gy=SY+152*SC+1.5;ly=SY+140*SC+1.5;lx0=SX+60*SC;lx1=SX+74*SC
    rock=[(1150,1080),(1165,990),(1198,930),(1250,885),(lx0-18,ly+6),(lx0,ly),(lx1,ly),(lx1+8,gy-3),(lx1+18,gy),(SX+139*SC,gy),(SX+148*SC,gy+8),(1548,925),(1578,995),(1600,1080)]
    db.polygon([(x*SS,y*SS) for x,y in rock],fill=255);ba=np.asarray(bl.resize((W,H),Image.LANCZOS),np.float32)/255
    rt=fbm2(H,W,6,99,1/25);rc=hexc(c['rock'])*(0.75+0.5*np.clip(rt*0.5+0.5,0,1))[...,None]
    edge=ba-shift(blur(ba,7),[[1,0,-6],[0,1,-6]])
    rim=np.clip(edge,0,1)*np.clip((1400-xx)/250,0,1)*(yy<1050)
    rc=rc+rim[...,None]*hexc(c['rim'])*1.1
    top=np.clip(1-(yy-860)/120,0,1)*0.16;rc=rc+top[...,None]*hexc(c['rim'])
    lip=np.clip(ba-shift(ba,[[1,0,0],[0,1,7]]),0,1)*np.clip((1560-xx)/400,0.35,1)   # the moonlit top of the rock
    rc=rc+blur(lip,1.2)[...,None]*hexc(c['rim'])*0.35
    img=img*(1-ba[...,None])+rc*ba[...,None]
    # Rudolf: the brand's white low-poly stag, moonlit, with his red nose
    sx,sy,sc=SX,SY,SC
    defs="<filter id='g' x='-200%' y='-200%' width='500%' height='500%'><feGaussianBlur stdDeviation='2.2'/></filter>"
    nose=f"<circle cx='{NOSE[0]}' cy='{NOSE[1]}' r='2.8' fill='#ff2a2a' filter='url(#g)'/><circle cx='{NOSE[0]}' cy='{NOSE[1]}' r='1.45' fill='#ff3b30'/>"
    svg=f"<svg xmlns='http://www.w3.org/2000/svg' width='{W*SS}' height='{H*SS}'><defs>{defs}</defs><g transform='scale({SS}) translate({sx},{sy}) scale({sc})'>{stag(c['stagL'],c['stagS'],'#9eabd2' if m=='d' else '#b3bbe0','#eef2ff')}{nose}</g></svg>"
    open(f'{OUT}/_s.svg','w').write(svg);subprocess.run(['rsvg-convert',f'{OUT}/_s.svg','-o',f'{OUT}/_s.png'],check=True)
    st=np.asarray(Image.open(f'{OUT}/_s.png').convert('RGBA').resize((W,H),Image.LANCZOS),np.float32)/255
    sa=st[...,3:4];sc3=st[...,:3]
    lightdir=np.clip(1.06-(xx-sx-80)/900,0.82,1.06)[...,None]   # brighter on the side toward the moon
    sc3=sc3*lightdir*(0.93 if m=='d' else 1)
    # a glow around him, and his shadow on the rock
    glow=blur(st[...,3],28)
    img+=glow[...,None]*hexc('#c8d6ff' if m=='d' else '#fff2de')*0.35
    shm=np.zeros((H,W),np.float32)
    for x0,x1,yh in HOOVES:
        cx=sx+(x0+x1)/2*sc;cy=sy+yh*sc+1;shm+=np.exp(-(((xx-cx)/((x1-x0)*sc*0.9))**2+((yy-cy)/3.2)**2))
    sh=np.clip(blur(shm,1.5)*0.85,0,0.85)*ba
    img=img*(1-sh[...,None])
    rimS=np.clip(st[...,3]-shift(st[...,3],[[1,0,5],[0,1,2]]),0,1)[...,None]
    sc3=sc3+rimS*hexc(c['rim'])*0.6
    img=img*(1-sa)+sc3*sa
    # fireflies
    r=np.random.default_rng(13);ff=np.zeros((H,W),np.float32)
    for _ in range(26):
        x,y=int(r.uniform(950,1750)),int(r.uniform(760,1040));ff[y,x]+=r.uniform(3,7)
    ffb=blur(ff,2.4)*3+blur(ff,9)*6
    img+=ffb[...,None]*hexc('#ffe39a' if m=='d' else '#fff3cf')*0.6
    # bloom, vignette, grain
    lum=img.mean(-1);bright=np.clip(lum-(0.62 if m=='d' else 0.9),0,1)
    bloom=blur(bright,18)
    img+=bloom[...,None]*np.array([0.85,0.9,1.0])*0.9
    vig=1-0.38*((xx/W-0.5)**2*1.4+(yy/H-0.45)**2*2.0);img*=vig[...,None]
    img+=np.random.default_rng(1).standard_normal((H,W,1)).astype(np.float32)*c['grain']
    Image.fromarray((np.clip(img,0,1)**(1/1.0)*255).astype(np.uint8)).save(f'{OUT}/stagr_{m}.png')
for m in sys.argv[2:] or 'dl':scene(m)
