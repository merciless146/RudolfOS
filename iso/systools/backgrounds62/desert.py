# Desert Camp (1.6.2): an open desert with sparse dry brush, a few tents across the plain and one in front, at sunrise
# (light: an orange sky with a few high clouds, the sun's mirage on the horizon) and at night (dark: stars and a nebula,
# the tents lit from inside, campfires). Everything is procedural (numpy noise, polygons); nothing is taken from other
# pictures. The moving parts (the mirage, the smoke, the flames, the twinkling and shooting stars) are drawn over this
# picture by desktop/desert62.js and rudolf-native.c; where they are is SCENE below (the same numbers there).
# python3 desert.py OUTDIR   ->  desert_l.png, desert_d.png (1600x1067)
import sys,json,numpy as np,cv2
from PIL import Image,ImageDraw
OUT=sys.argv[1];W,H=1600,1067;HZ=640;SS=2
# where the moving parts are, in the picture's pixels (desert62.js DS, rudolf-native.c DS_*)
SCENE={'hz':HZ,'sun':[1030,HZ,44],
       'fires':[[760,930,1.35],[312,714,.42],[1262,698,.36],[905,680,.26]],
       'tents':[[548,890,340,190,1,'dome'],[256,708,92,48,0,'ridge'],[370,706,78,50,1,'bell'],[314,688,58,28,0,'dome'],
                [1204,700,84,34,1,'tunnel'],[1320,698,64,38,0,'cabin'],[1262,684,44,27,1,'ridge'],[872,679,34,22,1,'bell'],[938,677,36,16,0,'tunnel']],
       # the campers' things, where people leave them: chairs facing each fire, a cooler (a lantern on it), the firewood,
       # packs and boots by the doors, water jugs, a folding table with a stove (kind, x, ground y, scale, facing)
       'props':[['wood',642,958,1.3,1],['chair',688,944,1.3,1],['chair',842,952,1.35,-1],['cooler',868,906,1.25,1],['lantern',868,880,1.25,1],
                ['pack',712,893,1.2,-1],['boots',590,897,1.05,1],['jug',392,896,1.15,1],['table',428,930,1.3,1],
                ['chair',282,722,.42,1],['chair',342,724,.42,-1],['cooler',214,712,.38,1],['pack',398,709,.36,1],['wood',300,728,.4,1],
                ['chair',1238,706,.36,1],['chair',1287,707,.36,-1],['table',1358,704,.34,1],['jug',1182,703,.32,1],['lantern',1358,690,.34,1],
                ['chair',889,685,.26,1],['cooler',924,686,.25,1]]}
def blur(a,sg):return cv2.GaussianBlur(np.ascontiguousarray(a,np.float32),(0,0),sg)
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
def grad(stops,n):
    ys=np.linspace(0,1,n);out=np.zeros((n,3),np.float32)
    for k in range(3):out[:,k]=np.interp(ys,[s for s,_ in stops],[hexc(c)[k] for _,c in stops])
    return out
def mask(draw_fn):
    im=Image.new('L',(W*SS,H*SS),0);d=ImageDraw.Draw(im);draw_fn(d,SS)
    return np.asarray(im.resize((W,H),Image.LANCZOS),np.float32)/255
def over(img,m,col,a=1.0):
    m=np.clip(m*a,0,1)[...,None];return img*(1-m)+np.asarray(col,np.float32)*m

PAL={'l':dict(sky=[(0,'#4f5d7c'),(.3,'#7d7894'),(.58,'#c98478'),(.8,'#e7762f'),(1,'#f07d24')],glow='#ff7a1e',
              far='#8a6e74',near='#a97d6a',sand=[(0,'#d7a070'),(.25,'#c98d5e'),(1,'#9c6544')],brush='#4b3a2c',brushL='#7a5a3d',
              tent=['#2f7d8c','#c4673a','#5d7a3a','#b8873a','#8c4a5e','#3f6b9e'],tentS=.55,cloud='#ffb07a',cloudS='#a77f93',grain=.022),
     'd':dict(sky=[(0,'#02030a'),(.45,'#070b1e'),(.8,'#121a36'),(1,'#22284a')],glow='#3a3f6a',
              far='#0c0f1f',near='#121527',sand=[(0,'#2a2735'),(.3,'#221f2c'),(1,'#141219')],brush='#08080d',brushL='#1b1a24',
              tent=['#1b3a46','#3b2420','#24301c','#3a2f1c','#33202a','#1c2a40'],tentS=.6,cloud='#6c7398',cloudS='#2d3150',grain=.03)}
def tent_poly(cx,by,w,h,st):
    # the tent's outline, by its style: a dome, a ridge (A-frame, seen three-quarters), a bell (a cone on a short wall,
    # its pole's tip above), a tunnel (low hoops, lower towards the back) or a cabin (walls and a pitched roof)
    if st=='dome':return [(cx-w/2+w*t,by-h*np.sin(np.pi*t)**0.8) for t in np.linspace(0,1,41)]
    if st=='ridge':return [(cx-w*.5,by),(cx-w*.12,by-h),(cx+w*.46,by-h*.86),(cx+w*.5,by-h*.04),(cx+w*.26,by)]
    if st=='bell':return [(cx-w*.5,by),(cx-w*.5,by-h*.26)]+[(cx+w*.5*np.cos(a)*(1-0.8*np.sin(a)),by-h*.26-h*.7*np.sin(a)) for a in np.linspace(np.pi,0,25)]+[(cx+w*.5,by-h*.26),(cx+w*.5,by)]
    if st=='tunnel':return [(cx-w/2+w*t,by-h*np.sin(np.pi*min(1,t*1.15))**0.55*(1-.42*t)) for t in np.linspace(0,1,41)]+[(cx+w/2,by)]
    return [(cx-w*.5,by),(cx-w*.5,by-h*.55),(cx-w*.3,by-h),(cx+w*.3,by-h),(cx+w*.5,by-h*.55),(cx+w*.5,by)]
def tent_seams(d,s,cx,by,w,h,st):
    lw=max(1,int(w*0.012*s));L=lambda pts,f=255,wd=lw:d.line([(x*s,y*s) for x,y in pts],fill=f,width=wd)
    if st=='dome':
        for k in (-1,1):L([(cx+k*w*0.48*(1-2*t)*0.55+k*w*0.02,by-h*np.sin(np.pi*t)**0.8*0.98) for t in np.linspace(0,1,24)])
        dw=w*0.2;L([(cx,by-h*0.72),(cx,by)],180,max(1,int(w*0.006*s)))
        d.arc([(cx-dw)*s,(by-h*0.75)*s,(cx+dw)*s,(by+h*0.7)*s],180,360,fill=200,width=lw)
    elif st=='ridge':
        L([(cx-w*.12,by-h),(cx+w*.26,by)]);L([(cx-w*.12,by-h),(cx+w*.46,by-h*.86)]);L([(cx-w*.12,by-h*.92),(cx-w*.12,by)],180)
        L([(cx-w*.12,by-h),(cx-w*.14,by-h*1.12)],255,lw+1)
    elif st=='bell':
        top=(cx,by-h*.96)
        for k in np.linspace(-.42,.42,5):L([top,(cx+w*k,by-h*.26)],170)
        L([(cx-w*.5,by-h*.26),(cx+w*.5,by-h*.26)]);L([top,(cx,by-h*1.14)],255,lw+1);L([(cx-w*.1,by),(cx,by-h*.5),(cx+w*.1,by)],200)
    elif st=='tunnel':
        for t in (.2,.47,.74):
            x=cx-w/2+w*t;hh=h*np.sin(np.pi*min(1,t*1.15))**0.55*(1-.42*t);L([(x-w*.04,by),(x,by-hh),(x+w*.04,by)])
        d.ellipse([(cx-w*.46)*s,(by-h*.62)*s,(cx-w*.3)*s,(by)*s],outline=200,width=lw)
    else:
        L([(cx-w*.5,by-h*.55),(cx+w*.5,by-h*.55)]);L([(cx-w*.18,by),(cx-w*.18,by-h*.48),(cx+w*.08,by-h*.48),(cx+w*.08,by)],200)
        d.rectangle([(cx+w*.2)*s,(by-h*.44)*s,(cx+w*.4)*s,(by-h*.26)*s],outline=200,width=lw)
def scene(m):
    c=PAL[m];rng=np.random.default_rng(7);img=np.zeros((H,W,3),np.float32)
    yy,xx=np.mgrid[0:H,0:W].astype(np.float32)
    sx,sy,sr=SCENE['sun']
    # --- sky
    img[:HZ]=grad(c['sky'],HZ)[:,None,:]
    d=np.sqrt((xx-sx)**2+((yy-sy)*1.6)**2)
    if m=='l':
        glow=np.exp(-d/460)*.5+np.exp(-d/150)*.3
        img[:HZ]+=glow[:HZ,:,None]*hexc(c['glow'])*np.array([1,.72,.45],np.float32)
    else:
        glow=np.exp(-np.abs(yy-HZ)/90)*.18;img[:HZ]+=glow[:HZ,:,None]*hexc(c['glow'])
        # the nebula: a band rising from the horizon right of the middle up to the top left, dust lanes through it
        u=(xx-1120)*0.93+(yy-HZ)*0.37;v=-(xx-1120)*0.37+(yy-HZ)*0.93
        band=np.exp(-(u/(190+0.14*np.abs(v)))**2)*np.clip(-v/900,0,1)**0.4*np.clip(1+v/1500,0,1)
        n1=fbm2(H,W,6,11,1/160);n2=fbm2(H,W,5,12,1/60)
        neb=band*np.clip(0.55+0.5*n1,0,1.4);dust=np.clip(0.5-0.9*n2,0,1)*band
        col=hexc('#b7a6d6')*(1-np.clip(n1,0,1))[...,None]+hexc('#e7b48f')*np.clip(n1,0,1)[...,None]
        img+=(neb*.62)[...,None]*col;img-=(dust*.22)[...,None]
        img+=(np.exp(-((u)/90)**2)*np.clip(-v/700,0,1)*np.clip(n1+.3,0,1)*.2)[...,None]*hexc('#d9a6c9')
        # stars: many faint, a few bright, denser in the band
        n=1500;stx=rng.uniform(0,W,n);sty=rng.uniform(0,HZ-8,n);br=rng.power(10,n)**3*1.3+0.05
        bu=(stx-1120)*0.93+(sty-HZ)*0.37;bv=-(stx-1120)*0.37+(sty-HZ)*0.93
        br*=1+2.0*np.exp(-(bu/220)**2)*np.clip(-bv/900,0,1)
        st=np.zeros((H,W),np.float32);np.add.at(st,(sty.astype(int),stx.astype(int)),br)
        k=5000;bx=rng.normal(0,170,k);by=-rng.uniform(0,1,k)**0.7*1100   # the band's own dust of faint stars
        px=1120+bx*0.93-by*0.37;py=HZ+bx*0.37+by*0.93;ok=(px>=0)&(px<W)&(py>=0)&(py<HZ-8)
        np.add.at(st,(py[ok].astype(int),px[ok].astype(int)),rng.uniform(0.03,0.22,ok.sum()))
        st=blur(st,0.75)*2.6;fade=np.clip((HZ-10-yy)/120,0.15,1)
        tint=np.stack([np.ones_like(st),np.ones_like(st)*0.97,np.ones_like(st)*1.04],-1)
        img+=(st*fade)[...,None]*tint*hexc('#f2f4ff')
    # --- very sparse high clouds: a few long, thin streaks (cirrus), lit from below at sunrise
    cl=np.zeros((H,W),np.float32)
    for (cx,cy,lw,lh,sd,a) in [(380,135,430,9,31,.8),(1290,235,330,7,32,.65),(860,330,200,5,33,.45)]:
        e=np.exp(-((xx-cx)/lw)**2-((yy-cy+0.06*(xx-cx))/lh)**2)
        st=cv2.resize(fbm2(H//8+4,W//2+4,4,sd,1/14),(W*2,H),interpolation=cv2.INTER_CUBIC)[:H,:W]   # streaks: noise stretched along
        cl+=e*np.clip(0.2+1.1*st,0,1)*a
    cl=np.clip(cl,0,1)
    if m=='l':
        lit=np.clip(1-yy/HZ,0,1);col=hexc(c['cloud'])[None,None,:]*(0.75+0.25*lit[...,None])
        img[:HZ]=img[:HZ]*(1-cl[:HZ,:,None]*.6)+(col*cl[...,None])[:HZ]*.75
    else:
        img[:HZ]=img[:HZ]*(1-cl[:HZ,:,None]*.55)+(hexc(c['cloud'])*cl[...,None])[:HZ]*.22
    # --- distant low mountains, two ridgelines, a gap where the sun rises
    far=HZ-6-34*(fbm1(W,5,41,0.003)*0.5+0.5);near=HZ+2-18*(fbm1(W,5,42,0.005)*0.5+0.5)
    gap=np.exp(-((np.arange(W)-sx)/150)**2);far=far+gap*30;near=near+gap*16
    for ridge,col,hz in [(far,c['far'],0.5),(near,c['near'],0.8)]:
        mm=(yy>=ridge[None,:])&(yy<=HZ+4);img=over(img,mm.astype(np.float32),hexc(col),1)
        if m=='l':
            haze=np.exp(-np.abs(xx-sx)/500)*0.35*hz;img=over(img,mm*haze,hexc(c['glow']),1)
    # --- the desert floor
    g=grad(c['sand'],H-HZ);img[HZ:]=g[:,None,:]
    tex=fbm2(H,W,5,51,1/90);rip=np.sin(xx*0.02+fbm2(H,W,3,52,1/300)*6+yy*0.11)*0.5+0.5
    persp=np.clip((yy-HZ)/(H-HZ),0,1)
    img[HZ:]*=(1+(tex*0.06+rip*0.03*persp)[HZ:,:,None])
    if m=='l':
        sh=np.exp(-np.abs(xx-sx)/380)*np.exp(-(yy-HZ)/90);img[HZ:]+=(sh[HZ:]*.22)[...,None]*hexc(c['glow'])
        img[HZ:]+=(np.exp(-(yy-HZ)/14)*.12)[HZ:,:,None]*hexc('#ff9a52')   # the heat haze along the horizon
    else:
        img[HZ:]+=(np.exp(-(yy-HZ)/60)*.06)[HZ:,:,None]*hexc('#6a6f9a')
    # --- sparse dry brush: clumps of twigs, smaller and fewer towards the horizon
    def brush(d,s):
        r=np.random.default_rng(61)
        for i in range(120):
            y=HZ+6+(H-HZ-10)*r.uniform(0,1)**1.5;sc=0.15+2.6*((y-HZ)/(H-HZ))**1.2
            x=r.uniform(0,W)
            if abs(x-sx)<80 and y<HZ+40:continue   # (none under the sun's mirage)
            if any(abs(x-t[0])<t[2]*0.6 and abs(y-t[1]+t[3]*0.3)<t[3]*0.9 for t in SCENE['tents']):continue
            n=int(8+r.integers(4,14));hgt=(10+r.uniform(0,20))*sc
            for k in range(n):
                a=np.pi*(0.15+0.7*r.uniform(0,1));l=hgt*(0.5+0.6*r.uniform(0,1))
                x2=x+np.cos(a)*l*(1 if k%2 else -1)*0.9;y2=y-np.sin(a)*l
                d.line([(x*s,y*s),(x2*s,y2*s)],fill=255,width=max(1,int(1.1*sc*s)))
                for j in range(2):
                    t=0.4+0.3*j;bx=x+(x2-x)*t;by=y+(y2-y)*t
                    d.line([(bx*s,by*s),((bx+(r.uniform(-1,1))*l*0.3)*s,(by-l*0.25)*s)],fill=255,width=max(1,int(0.8*sc*s)))
            d.ellipse([(x-hgt*0.5)*s,(y-hgt*0.12)*s,(x+hgt*0.5)*s,(y+hgt*0.05)*s],fill=160)
        for i in range(260):  # dry grass tufts
            y=HZ+4+(H-HZ-6)*r.uniform(0,1)**1.4;sc=0.1+1.1*((y-HZ)/(H-HZ));x=r.uniform(0,W)
            for k in range(5):
                dx=(k-2)*2.2*sc;d.line([((x+dx)*s,y*s),((x+dx*2.4+r.uniform(-2,2)*sc)*s,(y-(6+r.uniform(0,7))*sc)*s)],fill=200,width=max(1,int(0.8*s)))
    bm=mask(brush);img=over(img,bm,hexc(c['brush']),0.92)
    if m=='l':img=over(img,np.clip(bm-blur(bm,1.2),0,1)*np.exp(-np.abs(xx-sx)/700),hexc('#ffbf80'),0.6)
    # --- the tents: a dome, a door, poles; shadow on the sand; lit from inside at night
    # (farthest first: a tent whose base is higher up the picture stands behind the ones in front of it)
    for i,(cx,by,tw,th,lit,stl) in sorted(enumerate(SCENE['tents']),key=lambda e:e[1][1]):
        col=hexc(c['tent'][i%len(c['tent'])])
        sh=mask(lambda d,s:d.ellipse([(cx-tw*0.62)*s,(by-th*0.08)*s,(cx+tw*0.7+th*0.5)*s,(by+th*0.14)*s],fill=255))
        img=over(img,blur(sh,2+tw*0.02),np.zeros(3),0.45 if m=='l' else 0.35)
        poly=tent_poly(cx,by,tw,th,stl);tm=mask(lambda d,s:d.polygon([(x*s,y*s) for x,y in poly],fill=255))
        # the fabric: lighter towards the sun's side (light) or glowing from within (dark)
        if m=='l':
            shade=np.clip(0.6+0.55*(xx-cx)/tw*(1 if sx>cx else -1),0.35,1.15)
            fab=col[None,None,:]*shade[...,None]*1.25+hexc('#ffb36b')*0.12
        else:
            dd=np.sqrt(((xx-cx)/(tw*0.55))**2+((yy-(by-th*0.35))/(th*0.75))**2)
            inner=np.exp(-dd**2*1.6)*(1.0 if lit else 0.0)
            fab=col[None,None,:]*0.9+inner[...,None]*hexc('#ffd68a')*1.05+inner[...,None]**3*hexc('#fff3d6')*0.4
        img=img*(1-tm[...,None])+fab*tm[...,None]
        # seams and poles: two arcs crossing, and the door's zip
        def seams(d,s):tent_seams(d,s,cx,by,tw,th,stl)
        sm=mask(seams)*tm;img=over(img,sm,np.zeros(3) if m=='l' else hexc('#100a06'),0.35 if m=='l' else 0.45)
        if m=='d' and lit:  # the glow spilling out through the fabric onto the sand
            gl=blur(tm,tw*0.18)*1.2;img+=(np.clip(gl,0,1)*0.22)[...,None]*hexc('#ffc46e')
            sp=np.exp(-(((xx-cx)/(tw*0.9))**2+((yy-by-th*0.08)/(th*0.25))**2))*np.clip((yy-by+th*0.12)/(th*0.14),0,1);img+=(sp*0.16)[...,None]*hexc('#ffb860')
        # guy lines
        gm=mask(lambda d,s:[d.line([((cx+k*tw*0.38)*s,(by-th*0.62)*s),((cx+k*tw*0.78)*s,(by+th*0.02)*s)],fill=150,width=max(1,int(s*0.7))) for k in (-1,1)])
        img=over(img,gm,np.zeros(3) if m=='l' else hexc('#3a2c22'),0.5)
    # --- the campers' things: drawn on a layer of their own, the far ones smaller; at night dark, lit warm on their side
    #     towards the nearest fire
    lay=Image.new('RGBA',(W*SS,H*SS),(0,0,0,0));D=ImageDraw.Draw(lay)
    def col(hx,k=1.0):
        v=hexc(hx)*(1.0 if m=='l' else 0.32)*k;return tuple(int(min(1,x)*255) for x in v)+(255,)
    for kind,x,y,k,f in SCENE['props']:
        P=lambda pts:[((x+f*px*k)*SS,(y-py*k)*SS) for px,py in pts]
        R=lambda x0,y0,x1,y1,c:D.polygon(P([(x0,y0),(x1,y0),(x1,y1),(x0,y1)]),fill=c)
        lw=max(1,int(2.2*k*SS))
        if kind=='chair':   # a folding camp chair facing the fire: crossed legs, the seat, the back, the arms
            D.line(P([(-10,0),(8,22)]),fill=col('#2b2b2b'),width=lw);D.line(P([(8,0),(-10,22)]),fill=col('#2b2b2b'),width=lw)
            D.polygon(P([(-13,22),(11,20),(12,25),(-12,27)]),fill=col('#2f6fb0'))
            D.polygon(P([(-13,23),(-16,48),(-12,49),(-8,25)]),fill=col('#2a5f98'))
            D.line(P([(-15,36),(10,32)]),fill=col('#222222'),width=lw)
        elif kind=='cooler':
            R(-16,0,16,18,col('#c8372d'));R(-17,18,17,23,col('#f2efe8'));R(-6,23,6,25,col('#d9d4cc'))
        elif kind=='lantern':
            R(-4,0,4,3,col('#3a3a3a'));R(-3.4,3,3.4,11,col('#ffe39a' if m=='l' else '#ffe7a8',1.0 if m=='l' else 3.0));R(-4,11,4,13,col('#3a3a3a'))
            D.arc([(x-3*k)*SS,(y-17*k)*SS,(x+3*k)*SS,(y-11*k)*SS],180,360,fill=col('#3a3a3a'),width=max(1,int(k*SS)))
        elif kind=='pack':  # a backpack leaning on the tent, its sleeping mat strapped on top
            D.rounded_rectangle([(x-9*k)*SS,(y-26*k)*SS,(x+9*k)*SS,y*SS],radius=int(4*k*SS),fill=col('#6b7b3a'))
            R(-7,4,7,12,col('#55622d'));D.rounded_rectangle([(x-11*k)*SS,(y-31*k)*SS,(x+11*k)*SS,(y-25*k)*SS],radius=int(3*k*SS),fill=col('#d26a2a'))
        elif kind=='boots':
            for dx in (-6,5):D.polygon(P([(dx-3,0),(dx+6,0),(dx+6,3),(dx+1,4),(dx+1,11),(dx-3,11)]),fill=col('#5a3d26'))
        elif kind=='jug':
            R(-7,0,7,16,col('#3b7fc4',1.0));R(-2,16,2,19,col('#2b2b2b'));R(4,10,8,14,col('#2b6aa8'))
        elif kind=='wood':  # firewood stacked, cut ends showing
            for i,(dx,dy) in enumerate([(-10,0),(0,0),(10,0),(-5,7),(5,7),(0,14)]):
                D.ellipse([(x+(dx-5)*k)*SS,(y-(dy+7)*k)*SS,(x+(dx+5)*k)*SS,(y-dy*k)*SS],fill=col('#7a5536'));D.ellipse([(x+(dx-2.5)*k)*SS,(y-(dy+5)*k)*SS,(x+(dx+2.5)*k)*SS,(y-(dy+2)*k)*SS],fill=col('#c69a6a'))
        elif kind=='table':  # a folding table, a camp stove and a pot on it
            D.line(P([(-18,0),(-14,20)]),fill=col('#3a3a3a'),width=lw);D.line(P([(18,0),(14,20)]),fill=col('#3a3a3a'),width=lw)
            R(-22,20,22,24,col('#d8d2c4'));R(-14,24,4,31,col('#2f7a4a'));R(-10,31,-2,38,col('#9aa0a6'));R(7,24,15,33,col('#e0dcd4'))
    small=np.asarray(lay.resize((W,H),Image.LANCZOS),np.float32)/255
    a=small[...,3:4];rgb=small[...,:3]
    if m=='d':
        warm=np.zeros((H,W),np.float32)
        for (fx,fy,fs) in SCENE['fires']:warm+=np.exp(-(((xx-fx)/(150*fs))**2+((yy-fy)/(70*fs))**2))
        rgb=rgb+np.clip(warm,0,1)[...,None]*np.array([.55,.3,.12],np.float32)*a
    sh=blur(a[...,0],3)*0.35;img=img*(1-np.roll(sh,(3,6),(0,1))[...,None])
    img=img*(1-a)+rgb*a
    # --- the campfires' rings of stones and logs (the flames and smoke move: desert62.js)
    for (fx,fy,fs) in SCENE['fires']:
        rm=mask(lambda d,s:[d.ellipse([(fx+np.cos(a)*22*fs-4*fs)*s,(fy+np.sin(a)*7*fs-3*fs)*s,(fx+np.cos(a)*22*fs+4*fs)*s,(fy+np.sin(a)*7*fs+3*fs)*s],fill=255) for a in np.linspace(0,2*np.pi,11)[:-1]])
        img=over(img,rm,hexc('#5a5048') if m=='l' else hexc('#2a221e'),1)
        lg=mask(lambda d,s:[d.line([((fx-14*fs)*s,(fy+2*fs)*s),((fx+12*fs)*s,(fy-5*fs)*s)],fill=255,width=max(1,int(4*fs*s))),d.line([((fx+13*fs)*s,(fy+3*fs)*s),((fx-10*fs)*s,(fy-6*fs)*s)],fill=255,width=max(1,int(4*fs*s)))])
        img=over(img,lg,hexc('#3b2a1d') if m=='l' else hexc('#1d140e'),1)
        if m=='d':
            gl=np.exp(-(((xx-fx)/(90*fs))**2+((yy-fy)/(40*fs))**2));img+=(gl*0.35)[...,None]*hexc('#ff9a3c')
    # --- grain, then out
    img+=(rng.standard_normal((H,W)).astype(np.float32)*c['grain'])[...,None]
    img=np.clip(img,0,1)
    return (img*255+0.5).astype(np.uint8)
if __name__=='__main__':
    for m in 'ld':
        Image.fromarray(scene(m)).save(f'{OUT}/desert_{m}.png')
    json.dump(SCENE,open(f'{OUT}/desert-scene.json','w'))
    print('ok',json.dumps(SCENE))
