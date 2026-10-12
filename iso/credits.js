// Extract Chromium's third-party licence page (about_credits.html) from resources.pak as plain text.
// Usage: node credits.js resources.pak out.txt   (GPL-2.0-or-later)
const fs=require('fs'),zlib=require('zlib');
const d=fs.readFileSync(process.argv[2]);
if(d.readUInt32LE(0)!==5)throw new Error('pak version');
const cnt=d.readUInt16LE(8);let best=null;
for(let i=0;i<cnt;i++){const a=d.readUInt32LE(12+i*6+2),b=d.readUInt32LE(12+(i+1)*6+2);let x=d.subarray(a,b);
 try{if(x[0]==0x1f&&x[1]==0x8b)x=zlib.gunzipSync(x);else if(x[0]==0x1e&&x[1]==0x9b)x=zlib.brotliDecompressSync(x.subarray(8));}catch(e){continue}
 if(x.length>100000&&x.includes('class="product"')&&(!best||x.length>best.length))best=x}
if(!best)throw new Error('credits not found');
const t=best.toString('utf8'),un=s=>s.replace(/&lt;/g,'<').replace(/&gt;/g,'>').replace(/&quot;/g,'"').replace(/&#39;/g,"'").replace(/&amp;/g,'&');
const out=[];const re=/<span class="title">([\s\S]*?)<\/span>[\s\S]*?<pre>([\s\S]*?)<\/pre>/g;let m;
while((m=re.exec(t)))out.push('='.repeat(70)+'\n'+un(m[1].trim())+'\n'+'='.repeat(70)+'\n'+un(m[2]));
fs.writeFileSync(process.argv[3],'Chromium and its third-party components\n\n'+out.join('\n\n'));console.log(out.length);
