/* ===== Rudolf OS 1.6.1: a QR code encoder of Rudolf's own (docs/plan-1.6.1.md Part 18) =====
   Written from the QR code standard (ISO/IEC 18004): byte mode, error correction levels M and Q, versions 1 to 10, the
   mask chosen by the standard's four penalty rules. Used by User Settings › Sign-in options › Authenticator app to show
   the otpauth:// link, so a phone's app adds the account with its camera.
   qrMake(text,'M'|'Q'[,mask]) -> {size, mod: rows of 0/1, ver, ecl, mask}, or null when the text is too long for version 10
   (the mask is chosen unless one is given, for the tests).
   qrSvg(code) -> an <svg> element, dark modules on white with a 4-module quiet zone, in any theme.
   Tests: iso/qt/shell2/qr61_t.js (decoded back with OpenCV). ===== */
const QR_EC={M:{bits:0,ec:[10,16,26,18,24,16,18,22,22,26],nb:[1,1,1,2,2,4,4,4,5,5]},Q:{bits:3,ec:[13,22,18,26,18,24,18,22,20,24],nb:[1,1,2,2,4,4,6,6,8,8]}};
const QR_AL=[[],[6,18],[6,22],[6,26],[6,30],[6,34],[6,22,38],[6,24,42],[6,26,46],[6,28,50]];
/* GF(256) with the polynomial x^8+x^4+x^3+x^2+1 (0x11D) */
const QR_EXP=new Uint8Array(512),QR_LOG=new Uint8Array(256);
(function(){let x=1;for(let i=0;i<255;i++){QR_EXP[i]=x;QR_LOG[x]=i;x<<=1;if(x&256)x^=0x11D}for(let i=255;i<512;i++)QR_EXP[i]=QR_EXP[i-255]})();
const qrMul=(a,b)=>a&&b?QR_EXP[QR_LOG[a]+QR_LOG[b]]:0;
/* the Reed-Solomon error correction codewords of DATA, N of them (generator roots α^0..α^(N-1)) */
function qrRS(data,n){let g=[1];for(let i=0;i<n;i++){const h=new Array(g.length+1).fill(0);for(let j=0;j<g.length;j++){h[j]^=g[j];h[j+1]^=qrMul(g[j],QR_EXP[i])}g=h}
 const r=new Array(n).fill(0);for(const d of data){const f=d^r[0];r.shift();r.push(0);if(f)for(let j=0;j<n;j++)r[j]^=qrMul(g[j+1],f)}return r}
/* the modules a version has for data and error correction, in bits */
function qrRaw(v){let r=(16*v+128)*v+64;if(v>=2){const a=Math.floor(v/7)+2;r-=(25*a-10)*a-55;if(v>=7)r-=36}return r}
function qrBCH(d,g,glen){let r=d<<(glen-1);for(let i=31;i>=glen-1;i--)if(r>>>i&1)r^=g<<(i-glen+1);return r}
function qrMake(text,ecl,forceMask){ecl=QR_EC[ecl]?ecl:'M';const E=QR_EC[ecl],bytes=new TextEncoder().encode(String(text));
 let v=1,total,dataCw;for(;v<=10;v++){total=qrRaw(v)>>3;dataCw=total-E.ec[v-1]*E.nb[v-1];if(4+(v<10?8:16)+8*bytes.length<=dataCw*8)break}
 if(v>10)return null;
 /* the bit stream: mode 0100 (bytes), the count, the bytes, up to 4 zeros, to a whole byte, then 0xEC 0x11 … */
 const bits=[],put=(x,n)=>{for(let i=n-1;i>=0;i--)bits.push(x>>>i&1)};put(4,4);put(bytes.length,v<10?8:16);for(const b of bytes)put(b,8);
 put(0,Math.min(4,dataCw*8-bits.length));while(bits.length%8)bits.push(0);
 const dat=[];for(let i=0;i<bits.length;i+=8){let b=0;for(let j=0;j<8;j++)b=b<<1|bits[i+j];dat.push(b)}for(let p=0;dat.length<dataCw;p^=1)dat.push(p?0x11:0xEC);
 /* split into blocks (the short ones first), each with its own error correction, then interleaved */
 const nb=E.nb[v-1],ecn=E.ec[v-1],shortLen=Math.floor(dataCw/nb),nLong=dataCw%nb,blocks=[];let k=0;
 for(let i=0;i<nb;i++){const len=shortLen+(i>=nb-nLong?1:0),d=dat.slice(k,k+len);k+=len;blocks.push({d,e:qrRS(d,ecn)})}
 const cw=[];for(let i=0;i<=shortLen;i++)for(const b of blocks)if(i<b.d.length)cw.push(b.d[i]);for(let i=0;i<ecn;i++)for(const b of blocks)cw.push(b.e[i]);
 /* the function patterns */
 const size=17+4*v,M=[],F=[];for(let y=0;y<size;y++){M.push(new Array(size).fill(0));F.push(new Array(size).fill(false))}
 const set=(x,y,d)=>{M[y][x]=d?1:0;F[y][x]=true};
 for(let i=0;i<size;i++){set(6,i,i%2==0);set(i,6,i%2==0)}
 const finder=(cx,cy)=>{for(let dy=-4;dy<=4;dy++)for(let dx=-4;dx<=4;dx++){const x=cx+dx,y=cy+dy,d=Math.max(Math.abs(dx),Math.abs(dy));if(x>=0&&x<size&&y>=0&&y<size)set(x,y,d!=2&&d!=4)}};
 finder(3,3);finder(size-4,3);finder(3,size-4);
 const al=QR_AL[v-1];for(let i=0;i<al.length;i++)for(let j=0;j<al.length;j++){if((i==0&&j==0)||(i==0&&j==al.length-1)||(i==al.length-1&&j==0))continue;
  for(let dy=-2;dy<=2;dy++)for(let dx=-2;dx<=2;dx++)set(al[i]+dx,al[j]+dy,Math.max(Math.abs(dx),Math.abs(dy))!=1)}
 const format=mask=>{const d=E.bits<<3|mask,b=(d<<10|qrBCH(d,0x537,11))^0x5412,g=i=>b>>>i&1;
  for(let i=0;i<=5;i++)set(8,i,g(i));set(8,7,g(6));set(8,8,g(7));set(7,8,g(8));for(let i=9;i<15;i++)set(14-i,8,g(i));
  for(let i=0;i<8;i++)set(size-1-i,8,g(i));for(let i=8;i<15;i++)set(8,size-15+i,g(i));set(8,size-8,1)};
 format(0);
 if(v>=7){const b=v<<12|qrBCH(v,0x1F25,13);for(let i=0;i<18;i++){const a=size-11+i%3,c=Math.floor(i/3),d=b>>>i&1;set(a,c,d);set(c,a,d)}}
 /* the codewords, in the zigzag from the bottom right, two columns at a time (skipping the timing column) */
 let bi=0;for(let r=size-1;r>=1;r-=2){if(r==6)r=5;for(let vert=0;vert<size;vert++)for(let j=0;j<2;j++){const x=r-j,up=((r+1)&2)==0,y=up?size-1-vert:vert;
   if(!F[y][x]&&bi<cw.length*8){M[y][x]=cw[bi>>3]>>>(7-(bi&7))&1;bi++}}}
 const MASK=[(x,y)=>(x+y)%2==0,(x,y)=>y%2==0,(x,y)=>x%3==0,(x,y)=>(x+y)%3==0,(x,y)=>(Math.floor(x/3)+Math.floor(y/2))%2==0,
  (x,y)=>x*y%2+x*y%3==0,(x,y)=>(x*y%2+x*y%3)%2==0,(x,y)=>((x+y)%2+x*y%3)%2==0];
 const apply=m=>{for(let y=0;y<size;y++)for(let x=0;x<size;x++)if(!F[y][x]&&MASK[m](x,y))M[y][x]^=1};
 let best=0,bestP=Infinity;for(let m=0;m<8;m++){if(forceMask!=null&&m!=forceMask)continue;apply(m);format(m);const p=qrPenalty(M);if(p<bestP){bestP=p;best=m}apply(m)}
 apply(best);format(best);return{size,mod:M,ver:v,ecl,mask:best}}
/* the four penalty rules: runs of 5 or more, 2×2 blocks, the finder-like 1:1:3:1:1 with 4 light, the dark share */
function qrPenalty(M){const n=M.length;let p=0,dark=0;
 const line=get=>{let run=1;for(let i=1;i<n;i++){if(get(i)==get(i-1))run++;else{if(run>=5)p+=run-2;run=1}}if(run>=5)p+=run-2;
  for(let i=-4;i<n;i++){const s=k=>{const j=i+k;return j<0||j>=n?0:get(j)};const core=s(4)&&!s(5)&&s(6)&&s(7)&&s(8)&&!s(9)&&s(10);if(!core)continue;
   const lightBefore=!s(0)&&!s(1)&&!s(2)&&!s(3),lightAfter=!s(11)&&!s(12)&&!s(13)&&!s(14);if(lightBefore||lightAfter)p+=40}};
 for(let y=0;y<n;y++)line(x=>M[y][x]);for(let x=0;x<n;x++)line(y=>M[y][x]);
 for(let y=0;y<n-1;y++)for(let x=0;x<n-1;x++){const c=M[y][x];if(c==M[y][x+1]&&c==M[y+1][x]&&c==M[y+1][x+1])p+=3}
 for(const r of M)for(const c of r)dark+=c;p+=Math.floor(Math.abs(dark*20-n*n*10)/(n*n))*10;return p}
/* the code drawn: an <svg> of one path, dark on white with the quiet zone, so it reads in light and dark themes alike */
function qrSvg(code,px){const NS='http://www.w3.org/2000/svg',q=4,w=code.size+2*q,s=document.createElementNS(NS,'svg');px=px||Math.max(4,Math.floor(250/w))*w;
 s.setAttribute('viewBox',`0 0 ${w} ${w}`);s.setAttribute('width',px);s.setAttribute('height',px);s.setAttribute('shape-rendering','crispEdges');s.setAttribute('role','img');
 const bg=document.createElementNS(NS,'rect');bg.setAttribute('width',w);bg.setAttribute('height',w);bg.setAttribute('fill','#ffffff');s.appendChild(bg);
 let d='';code.mod.forEach((r,y)=>r.forEach((c,x)=>{if(c)d+=`M${x+q} ${y+q}h1v1h-1z`}));
 const p=document.createElementNS(NS,'path');p.setAttribute('d',d);p.setAttribute('fill','#000000');s.appendChild(p);s.style.display='block';s.style.background='#ffffff';return s}
if(typeof module=='object'&&module.exports)module.exports={qrMake,qrPenalty,qrRS};
