/* ===== 1.4.5 (spreng45): SpreadsheetPro's formula engine. No eval or Function: formulas are tokenized and parsed
   into a tree, then evaluated. Values: numbers, strings, booleans, errors ({e:'#DIV/0!'}), empty (null); ranges are
   {rg:{s,r0,c0,r1,c1}} until a function reads them. Dates are numbers, as in Excel (days since 1899-12-30).
   The engine knows nothing of the page: SpreadsheetPro gives it a book through SPE.calc(book). ===== */
const SPE=(()=>{
const ERR=e=>({e});const isE=v=>v&&typeof v=='object'&&'e'in v;
const E={div:ERR('#DIV/0!'),val:ERR('#VALUE!'),ref:ERR('#REF!'),name:ERR('#NAME?'),na:ERR('#N/A'),num:ERR('#NUM!'),cyc:ERR('#CIRCULAR!'),nul:ERR('#NULL!')};
const MAXC=702,MAXR=100000;
/* ---- A1 names ---- */
 function colName(c){let s='';c++;while(c>0){const m=(c-1)%26;s=String.fromCharCode(65+m)+s;c=Math.floor((c-1)/26)}return s}
 function colNum(s){let n=0;for(const ch of s.toUpperCase())n=n*26+(ch.charCodeAt(0)-64);return n-1}
const a1=(r,c)=>colName(c)+(r+1);
 function parseA1(s){const m=/^\$?([A-Za-z]{1,3})\$?(\d+)$/.exec(s);if(!m)return null;const c=colNum(m[1]),r=+m[2]-1;return c<MAXC&&r>=0&&r<MAXR?{r,c}:null}
/* ---- tokens ---- */
const OPS=['<>','<=','>=','+','-','*','/','^','&','=','<','>','%'];
 function tokenize(src){const t=[];let i=0;const n=src.length;
 while(i<n){const ch=src[i];
  if(/\s/.test(ch)){i++;continue}
  if(ch=='"'){let s='';i++;while(i<n){if(src[i]=='"'){if(src[i+1]=='"'){s+='"';i+=2;continue}break}s+=src[i++]}if(i>=n)throw E.val;i++;t.push({k:'str',v:s});continue}
  if(ch=='#'){const m=/^#(DIV\/0!|VALUE!|REF!|NAME\?|N\/A|NUM!|NULL!|CIRCULAR!)/i.exec(src.slice(i));if(m){t.push({k:'err',v:'#'+m[1].toUpperCase()});i+=m[0].length;continue}throw E.name}
  if(ch=="'"){let s='';i++;while(i<n&&!(src[i]=="'"&&src[i+1]!="'")){if(src[i]=="'"){s+="'";i+=2;continue}s+=src[i++]}i++;if(src[i]!='!')throw E.ref;i++;t.push({k:'sheet',v:s});continue}
  const num=/^(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?/.exec(src.slice(i));
  if(num&&!/^\d+:\d+/.test(src.slice(i))){t.push({k:'num',v:+num[0]});i+=num[0].length;continue}
  const w=/^[A-Za-z_\\$][A-Za-z0-9_.$]*/.exec(src.slice(i))||/^\d+:\d+/.exec(src.slice(i))&&{0:/^\$?\d+:\$?\d+/.exec(src.slice(i))[0]};
  if(w){let s=w[0];i+=s.length;
   if(src[i]=='!'){i++;t.push({k:'sheet',v:s});continue}
   /* A1:B2, A:A, 1:3 */
   const rest=src.slice(i);const m2=/^:(\$?[A-Za-z]{1,3}\$?\d+|\$?[A-Za-z]{1,3}|\$?\d+)/.exec(rest);
   if(m2&&(parseA1(s)||/^\$?[A-Za-z]{1,3}$/.test(s)||/^\$?\d+$/.test(s))){t.push({k:'rng',a:s,b:m2[1]});i+=m2[0].length;continue}
   if(/^\d+:\d+$/.test(s.replace(/\$/g,''))){const [x,y]=s.split(':');t.push({k:'rng',a:x,b:y});continue}
   if(parseA1(s)){t.push({k:'ref',v:s});continue}
   const u=s.toUpperCase();
   if(u=='TRUE'||u=='FALSE'){t.push({k:'bool',v:u=='TRUE'});continue}
   t.push({k:src[i]=='('?'fn':'name',v:u});continue}
  const op=OPS.find(o=>src.startsWith(o,i));if(op){t.push({k:'op',v:op});i+=op.length;continue}
  if(ch=='('||ch==')'||ch==','||ch==';'){t.push({k:ch==';'?',':ch});i++;continue}
  if(ch=='{'||ch=='}'){t.push({k:ch});i++;continue}
  throw E.name}
 return t}
/* ---- parser (precedence as in Excel: comparison < & < + - < * / < ^ < unary < %) ---- */
const BIN={'=':1,'<>':1,'<':1,'>':1,'<=':1,'>=':1,'&':2,'+':3,'-':3,'*':4,'/':4,'^':5};
 function parse(src){const t=tokenize(src);let p=0;const peek=()=>t[p],next=()=>t[p++];
 function expr(min){let l=unary();for(;;){const o=peek();if(!o||o.k!='op'||!(o.v in BIN)||BIN[o.v]<min)break;next();const r=expr(BIN[o.v]+1);l={t:'bin',o:o.v,l,r}}return l}
 function unary(){const o=peek();if(o&&o.k=='op'&&(o.v=='-'||o.v=='+')){next();return{t:'un',o:o.v,a:unary()}}return postfix(primary())}
 function postfix(a){while(peek()&&peek().k=='op'&&peek().v=='%'){next();a={t:'pct',a}}return a}
 function refNode(sheet,tk){if(tk.k=='ref')return{t:'ref',sheet,a:tk.v};if(tk.k=='rng')return{t:'rng',sheet,a:tk.a,b:tk.b};throw E.ref}
 function primary(){const k=next();if(!k)throw E.val;
  switch(k.k){case'num':return{t:'num',v:k.v};case'str':return{t:'str',v:k.v};case'bool':return{t:'bool',v:k.v};case'err':return{t:'err',v:k.v};
   case'sheet':{const r=next();if(!r)throw E.ref;return refNode(k.v,r)}
   case'ref':case'rng':return refNode(null,k);
   case'name':return{t:'name',v:k.v};
   case'fn':{next();const args=[];if(peek()&&peek().k==')'){next();return{t:'fn',f:k.v,args}}for(;;){if(peek()&&(peek().k==','||peek().k==')'))args.push({t:'blank'});else args.push(expr(1));const s=next();if(!s)throw E.val;if(s.k==')')break;if(s.k!=',')throw E.val}return{t:'fn',f:k.v,args}}
   case'(':{const e=expr(1);const c=next();if(!c||c.k!=')')throw E.val;return e}
   case'{':{const rows=[[]];for(;;){const v=next();if(!v)throw E.val;if(v.k=='}')break;if(v.k==',')continue;if(v.k=='op'&&v.v=='-'){const n=next();rows.at(-1).push(-n.v);continue}rows.at(-1).push(v.v)}return{t:'arr',v:rows}}
  }throw E.val}
 const e=expr(1);if(p<t.length)throw E.val;return e}
/* ---- references ---- */
 function rangeOf(n,ctx){const sh=n.sheet!=null?ctx.sheetByName(n.sheet):ctx.sheet;if(!sh)return null;
 if(n._g!==undefined)return n._g&&{s:sh,r0:n._g.r0,c0:n._g.c0,r1:n._g.r1,c1:n._g.c1};const g=rangeOf0(n,sh);n._g=g;return g}
 function rangeOf0(n,sh){
 if(n.t=='ref'){const a=parseA1(n.a);return a&&{s:sh,r0:a.r,c0:a.c,r1:a.r,c1:a.c}}
 const A=n.a.replace(/\$/g,''),B=n.b.replace(/\$/g,'');
 if(/^\d+$/.test(A)&&/^\d+$/.test(B))return{s:sh,r0:Math.min(+A,+B)-1,c0:0,r1:Math.max(+A,+B)-1,c1:MAXC-1};
 if(/^[A-Za-z]+$/.test(A)&&/^[A-Za-z]+$/.test(B))return{s:sh,r0:0,c0:Math.min(colNum(A),colNum(B)),r1:MAXR-1,c1:Math.max(colNum(A),colNum(B))};
 const a=parseA1(A),b=parseA1(B);if(!a||!b)return null;return{s:sh,r0:Math.min(a.r,b.r),c0:Math.min(a.c,b.c),r1:Math.max(a.r,b.r),c1:Math.max(a.c,b.c)}}
/* the cells of a range that hold something (whole columns are not walked cell by cell) */
 function rangeCells(rg,ctx,fn){const s=rg.s,big=(rg.r1-rg.r0+1)*(rg.c1-rg.c0+1)>200000;
 if(!big){for(let r=rg.r0;r<=rg.r1;r++)for(let c=rg.c0;c<=rg.c1;c++)fn(ctx.value(s,r,c),r,c);return}
 const ks=[...s.cells.keys()].map(k=>[Math.floor(k/MAXC),k%MAXC]).filter(([r,c])=>r>=rg.r0&&r<=rg.r1&&c>=rg.c0&&c<=rg.c1).sort((x,y)=>x[0]-y[0]||x[1]-y[1]);for(const[r,c]of ks)fn(ctx.value(s,r,c),r,c)}
 function rangeMatrix(rg,ctx){const s=rg.s;let r1=rg.r1,c1=rg.c1;if(r1-rg.r0>5000||c1-rg.c0>500){let mr=rg.r0,mc=rg.c0;for(const k of s.cells.keys()){const r=Math.floor(k/MAXC),c=k%MAXC;if(r>=rg.r0&&r<=rg.r1&&c>=rg.c0&&c<=rg.c1){mr=Math.max(mr,r);mc=Math.max(mc,c)}}r1=Math.min(r1,mr);c1=Math.min(c1,mc)}
 const m=[];for(let r=rg.r0;r<=r1;r++){const row=[];for(let c=rg.c0;c<=c1;c++)row.push(ctx.value(s,r,c));m.push(row)}return m}
/* ---- coercion ---- */
 function num(v){if(isE(v))throw v;if(v==null||v==='')return 0;if(typeof v=='number')return v;if(typeof v=='boolean')return v?1:0;const s=String(v).trim();if(s==='')return 0;const d=parseDate(s);if(d!=null)return d;const x=+s.replace(/,/g,'').replace(/%$/,'');if(!isNaN(x))return /%$/.test(s)?x/100:x;throw E.val}
 function str(v){if(isE(v))throw v;if(v==null)return'';if(typeof v=='boolean')return v?'TRUE':'FALSE';if(typeof v=='number')return fmtGeneral(v);return String(v)}
 function bool(v){if(isE(v))throw v;if(typeof v=='boolean')return v;if(typeof v=='number')return v!=0;if(v==null||v==='')return false;const u=String(v).toUpperCase();if(u=='TRUE')return true;if(u=='FALSE')return false;throw E.val}
 function fmtGeneral(v){if(!isFinite(v))return'#NUM!';if(Number.isInteger(v)&&Math.abs(v)<1e15)return String(v);const a=Math.abs(v);if(a>=1e11||a<1e-9&&a>0)return v.toExponential(5).replace(/\.?0+e/,'E').replace('e','E');return String(+v.toPrecision(11))}
/* dates: serial numbers from 1899-12-30, as Excel (1900 date system) */
const EPOCH=Date.UTC(1899,11,30);
const toSerial=(y,m,d)=>(Date.UTC(y,m-1,d)-EPOCH)/864e5;
 function fromSerial(n){const d=new Date(EPOCH+Math.floor(n)*864e5);return{y:d.getUTCFullYear(),m:d.getUTCMonth()+1,d:d.getUTCDate(),wd:d.getUTCDay()}}
 function parseDate(s){let m=/^(\d{4})-(\d{1,2})-(\d{1,2})$/.exec(s);if(m)return toSerial(+m[1],+m[2],+m[3]);m=/^(\d{1,2})\/(\d{1,2})\/(\d{4})$/.exec(s);if(m)return toSerial(+m[3],+m[1],+m[2]);return null}
/* ---- values of arguments ---- */
 function flat(args,ctx,opt){/* numbers of the arguments (for SUM and the like): in ranges only numbers count, typed values are converted */
 const out=[];for(const a of args){const v=a();if(v&&v.rg){rangeCells(v.rg,ctx,x=>{if(isE(x))throw x;if(typeof x=='number')out.push(x);else if(opt&&opt.a&&x!=null&&x!=='')out.push(typeof x=='boolean'?+x:0)})}else if(v&&v.arr){v.arr.flat().forEach(x=>{if(isE(x))throw x;if(typeof x=='number')out.push(x)})}else if(v==null&&opt&&opt.skipBlank){}else out.push(num(v))}return out}
 function vals(a,ctx){/* every value of one argument, a range walked cell by cell, empty cells included */
 const v=a();if(v&&v.rg)return rangeMatrix(v.rg,ctx).flat();if(v&&v.arr)return v.arr.flat();return[v]}
 function matrix(a,ctx){const v=a();if(v&&v.rg)return rangeMatrix(v.rg,ctx);if(v&&v.arr)return v.arr;return[[v]]}
 function scalar(v,ctx){if(v&&v.rg){const rg=v.rg;if(rg.r0==rg.r1&&rg.c0==rg.c1)return ctx.value(rg.s,rg.r0,rg.c0);
  /* implicit intersection with the formula's own row or column */
  if(rg.c0==rg.c1&&ctx.r>=rg.r0&&ctx.r<=rg.r1)return ctx.value(rg.s,ctx.r,rg.c0);if(rg.r0==rg.r1&&ctx.c>=rg.c0&&ctx.c<=rg.c1)return ctx.value(rg.s,rg.r0,ctx.c);return E.val}
 if(v&&v.arr)return v.arr[0][0];return v}
/* criteria as in COUNTIF: ">=10", "<>x", "a*", 5 */
 function crit(c){if(typeof c=='number'||typeof c=='boolean')return v=>v===c||(typeof v=='string'&&v.toUpperCase()==String(c).toUpperCase()&&typeof c!='boolean');
 const s=String(c??'');const m=/^(<=|>=|<>|=|<|>)?(.*)$/.exec(s);const op=m[1]||'=',rhs=m[2];const n=rhs!==''&&!isNaN(+rhs)?+rhs:null;
 const wild=/[*?]/.test(rhs)?new RegExp('^'+rhs.replace(/[.+^${}()|[\]\\]/g,'\\$&').replace(/~\*/g,'\u0001').replace(/~\?/g,'\u0002').replace(/\*/g,'.*').replace(/\?/g,'.').replace(/\u0001/g,'\\*').replace(/\u0002/g,'\\?')+'$','i'):null;
 return v=>{if(n!=null&&typeof v=='number'){return{'=':v==n,'<>':v!=n,'<':v<n,'>':v>n,'<=':v<=n,'>=':v>=n}[op]}
  if(n!=null&&op!='='&&op!='<>')return false;const a=v==null?'':String(v);
  if(op=='='){if(rhs==='')return v==null||v==='';return wild?wild.test(a):a.toUpperCase()==rhs.toUpperCase()}
  if(op=='<>'){if(rhs==='')return!(v==null||v==='');return wild?!wild.test(a):a.toUpperCase()!=rhs.toUpperCase()}
  if(typeof v!='string')return false;const cmp=a.toUpperCase().localeCompare(rhs.toUpperCase());return{'<':cmp<0,'>':cmp>0,'<=':cmp<=0,'>=':cmp>=0}[op]}}
 function cmpVals(a,b){/* Excel order: numbers < text < booleans; text without case */
 const rank=v=>typeof v=='number'?0:typeof v=='string'?1:typeof v=='boolean'?2:v==null?-1:3;const ra=rank(a),rb=rank(b);
 if(a==null&&b==null)return 0;if(a==null)return typeof b=='string'?cmpVals('',b):cmpVals(0,b);if(b==null)return typeof a=='string'?cmpVals(a,''):cmpVals(a,0);
 if(ra!=rb)return ra-rb;if(ra==0)return a-b;if(ra==1){const x=a.toUpperCase(),y=b.toUpperCase();return x<y?-1:x>y?1:0}return(a?1:0)-(b?1:0)}
/* ---- functions: each gets thunks (so IF and IFERROR evaluate only what they need) ---- */
const F={};const D={};   /* D: the one-line description shown while typing */
const def=(names,desc,f)=>names.split(',').forEach(n=>{F[n]=f;D[n]=desc});
const n1=(f)=>(a,ctx)=>{const v=scalar(a[0](),ctx);return f(num(v),a,ctx)};
const nOpt=(a,i,d,ctx)=>a[i]&&a[i].blank!==true?num(scalar(a[i](),ctx)):d;

const chk=x=>{if(typeof x=='number'&&!isFinite(x))throw E.num;return x};
/* maths */
def('SUM','SUM(number1, …): adds the numbers',(a,c)=>flat(a,c).reduce((s,x)=>s+x,0));
def('PRODUCT','PRODUCT(number1, …): multiplies the numbers',(a,c)=>flat(a,c).reduce((s,x)=>s*x,1));
def('SUMPRODUCT','SUMPRODUCT(array1, array2, …): the sum of the products of matching cells',(a,c)=>{const ms=a.map(x=>vals(x,c));const n=ms[0].length;if(ms.some(m=>m.length!=n))throw E.val;let s=0;for(let i=0;i<n;i++){let p=1;for(const m of ms){const v=m[i];if(isE(v))throw v;p*=typeof v=='number'?v:0}s+=p}return s});
def('SUMSQ','SUMSQ(number1, …): the sum of the squares',(a,c)=>flat(a,c).reduce((s,x)=>s+x*x,0));
const rnd=(x,d,m)=>{const f=Math.pow(10,d);const y=Math.abs(x)*f;const r=m=='up'?Math.ceil(y-1e-9):m=='down'?Math.floor(y+1e-9):Math.round(y+1e-9*Math.sign(y));return Math.sign(x)*r/f};
def('ROUND','ROUND(number, digits): rounds to that many digits',(a,c)=>rnd(num(scalar(a[0](),c)),nOpt(a,1,0,c)));
def('ROUNDUP','ROUNDUP(number, digits): rounds away from zero',(a,c)=>rnd(num(scalar(a[0](),c)),nOpt(a,1,0,c),'up'));
def('ROUNDDOWN','ROUNDDOWN(number, digits): rounds towards zero',(a,c)=>rnd(num(scalar(a[0](),c)),nOpt(a,1,0,c),'down'));
def('MROUND','MROUND(number, multiple): rounds to the nearest multiple',(a,c)=>{const x=num(scalar(a[0](),c)),m=num(scalar(a[1](),c));if(m==0)return 0;if(x*m<0)throw E.num;return Math.round(x/m)*m});
def('CEILING,CEILING.MATH','CEILING(number, significance): rounds up to a multiple',(a,c)=>{const x=num(scalar(a[0](),c)),m=nOpt(a,1,1,c);return m?Math.ceil(x/m-1e-12)*m:0});
def('FLOOR,FLOOR.MATH','FLOOR(number, significance): rounds down to a multiple',(a,c)=>{const x=num(scalar(a[0](),c)),m=nOpt(a,1,1,c);if(!m)throw E.div;return Math.floor(x/m+1e-12)*m});
def('INT','INT(number): rounds down to a whole number',n1(x=>Math.floor(x)));
def('TRUNC','TRUNC(number, digits): cuts off the decimals',(a,c)=>rnd(num(scalar(a[0](),c)),nOpt(a,1,0,c),'down'));
def('MOD','MOD(number, divisor): the remainder',(a,c)=>{const x=num(scalar(a[0](),c)),d=num(scalar(a[1](),c));if(d==0)throw E.div;return x-d*Math.floor(x/d)});
def('QUOTIENT','QUOTIENT(numerator, denominator): the whole part of a division',(a,c)=>{const d=num(scalar(a[1](),c));if(d==0)throw E.div;return Math.trunc(num(scalar(a[0](),c))/d)});
def('ABS','ABS(number): without its sign',n1(Math.abs));
def('SIGN','SIGN(number): 1, 0 or -1',n1(Math.sign));
def('SQRT','SQRT(number): the square root',n1(x=>{if(x<0)throw E.num;return Math.sqrt(x)}));
def('POWER','POWER(number, power)',(a,c)=>chk(Math.pow(num(scalar(a[0](),c)),num(scalar(a[1](),c)))));
def('EXP','EXP(number): e to that power',n1(x=>chk(Math.exp(x))));
def('LN','LN(number): the natural logarithm',n1(x=>{if(x<=0)throw E.num;return Math.log(x)}));
def('LOG','LOG(number, base): the logarithm (base 10 if not given)',(a,c)=>{const x=num(scalar(a[0](),c)),b=nOpt(a,1,10,c);if(x<=0||b<=0||b==1)throw E.num;return Math.log(x)/Math.log(b)});
def('LOG10','LOG10(number)',n1(x=>{if(x<=0)throw E.num;return Math.log10(x)}));
def('PI','PI(): 3.14159…',()=>Math.PI);
def('SIN','SIN(angle in radians)',n1(Math.sin));def('COS','COS(angle in radians)',n1(Math.cos));def('TAN','TAN(angle in radians)',n1(Math.tan));
def('ASIN','ASIN(number)',n1(x=>{if(x<-1||x>1)throw E.num;return Math.asin(x)}));def('ACOS','ACOS(number)',n1(x=>{if(x<-1||x>1)throw E.num;return Math.acos(x)}));def('ATAN','ATAN(number)',n1(Math.atan));
def('ATAN2','ATAN2(x, y)',(a,c)=>{const x=num(scalar(a[0](),c)),y=num(scalar(a[1](),c));if(!x&&!y)throw E.div;return Math.atan2(y,x)});
def('RADIANS','RADIANS(degrees)',n1(x=>x*Math.PI/180));def('DEGREES','DEGREES(radians)',n1(x=>x*180/Math.PI));
def('RAND','RAND(): a random number from 0 to 1 (new at each change)',()=>Math.random());
def('RANDBETWEEN','RANDBETWEEN(bottom, top): a random whole number',(a,c)=>{const lo=Math.ceil(num(scalar(a[0](),c))),hi=Math.floor(num(scalar(a[1](),c)));if(lo>hi)throw E.num;return lo+Math.floor(Math.random()*(hi-lo+1))});
def('FACT','FACT(number): 1 × 2 × … × number',n1(x=>{if(x<0)throw E.num;let f=1;for(let i=2;i<=Math.floor(x);i++)f*=i;return chk(f)}));
def('GCD','GCD(number1, …): the greatest common divisor',(a,c)=>flat(a,c).map(Math.trunc).reduce((x,y)=>{x=Math.abs(x);y=Math.abs(y);while(y){[x,y]=[y,x%y]}return x}));
def('LCM','LCM(number1, …): the least common multiple',(a,c)=>flat(a,c).map(Math.trunc).reduce((x,y)=>{let g=x,h=y;while(h){[g,h]=[h,g%h]}return g?Math.abs(x*y)/g:0}));
def('EVEN','EVEN(number): rounds up to an even number',n1(x=>{const y=Math.ceil(Math.abs(x)/2)*2;return Math.sign(x)*y||0}));
def('ODD','ODD(number): rounds up to an odd number',n1(x=>{let y=Math.ceil(Math.abs(x));if(y%2==0)y++;return x<0?-y:y}));
/* statistics */
def('AVERAGE','AVERAGE(number1, …): the mean',(a,c)=>{const n=flat(a,c);if(!n.length)throw E.div;return n.reduce((s,x)=>s+x,0)/n.length});
def('AVERAGEA','AVERAGEA(value1, …): the mean, text counted as 0',(a,c)=>{const n=flat(a,c,{a:1});if(!n.length)throw E.div;return n.reduce((s,x)=>s+x,0)/n.length});
def('MEDIAN','MEDIAN(number1, …): the middle value',(a,c)=>{const n=flat(a,c).sort((x,y)=>x-y);if(!n.length)throw E.num;const h=n.length>>1;return n.length%2?n[h]:(n[h-1]+n[h])/2});
def('MODE,MODE.SNGL','MODE(number1, …): the most common value',(a,c)=>{const n=flat(a,c),m=new Map();let best=null,bc=1;for(const x of n){const k=(m.get(x)||0)+1;m.set(x,k);if(k>bc){bc=k;best=x}}if(best==null)throw E.na;return best});
def('MIN','MIN(number1, …): the smallest',(a,c)=>{const n=flat(a,c);return n.length?Math.min(...n):0});
def('MAX','MAX(number1, …): the largest',(a,c)=>{const n=flat(a,c);return n.length?Math.max(...n):0});
def('MINA','MINA(value1, …)',(a,c)=>{const n=flat(a,c,{a:1});return n.length?Math.min(...n):0});def('MAXA','MAXA(value1, …)',(a,c)=>{const n=flat(a,c,{a:1});return n.length?Math.max(...n):0});
const variance=(n,s)=>{if(n.length<(s?2:1))throw E.div;const m=n.reduce((x,y)=>x+y,0)/n.length;return n.reduce((x,y)=>x+(y-m)*(y-m),0)/(n.length-(s?1:0))};
def('VAR,VAR.S','VAR(number1, …): the variance of a sample',(a,c)=>variance(flat(a,c),1));def('VARP,VAR.P','VARP(number1, …): the variance of a population',(a,c)=>variance(flat(a,c),0));
def('STDEV,STDEV.S','STDEV(number1, …): the standard deviation of a sample',(a,c)=>Math.sqrt(variance(flat(a,c),1)));def('STDEVP,STDEV.P','STDEVP(number1, …): of a population',(a,c)=>Math.sqrt(variance(flat(a,c),0)));
def('LARGE','LARGE(range, k): the k-th largest',(a,c)=>{const n=flat([a[0]],c).sort((x,y)=>y-x),k=Math.ceil(num(scalar(a[1](),c)));if(k<1||k>n.length)throw E.num;return n[k-1]});
def('SMALL','SMALL(range, k): the k-th smallest',(a,c)=>{const n=flat([a[0]],c).sort((x,y)=>x-y),k=Math.ceil(num(scalar(a[1](),c)));if(k<1||k>n.length)throw E.num;return n[k-1]});
def('RANK,RANK.EQ','RANK(number, range, order): its place in the list (largest first unless order is 1)',(a,c)=>{const x=num(scalar(a[0](),c)),n=flat([a[1]],c),asc=nOpt(a,2,0,c);if(!n.includes(x))throw E.na;return 1+n.filter(y=>asc?y<x:y>x).length});
def('PERCENTILE,PERCENTILE.INC','PERCENTILE(range, k): the value at that fraction (0-1)',(a,c)=>{const n=flat([a[0]],c).sort((x,y)=>x-y),k=num(scalar(a[1](),c));if(!n.length||k<0||k>1)throw E.num;const p=k*(n.length-1),i=Math.floor(p);return i+1<n.length?n[i]+(p-i)*(n[i+1]-n[i]):n[i]});
def('QUARTILE,QUARTILE.INC','QUARTILE(range, quart): 0 to 4',(a,c)=>{const q=Math.trunc(num(scalar(a[1](),c)));if(q<0||q>4)throw E.num;return F.PERCENTILE([a[0],()=>q/4],c)});
def('COUNT','COUNT(value1, …): how many numbers',(a,c)=>{let n=0;for(const x of a){const v=x();if(v&&(v.rg||v.arr))vals(()=>v,c).forEach(y=>{if(typeof y=='number')n++});else if(typeof v=='number'||(typeof v=='string'&&v.trim()!==''&&!isNaN(+v))||typeof v=='boolean')n++}return n});
def('COUNTA','COUNTA(value1, …): how many are not empty',(a,c)=>{let n=0;for(const x of a){const v=x();if(v&&(v.rg||v.arr))vals(()=>v,c).forEach(y=>{if(y!=null&&y!=='')n++});else if(v!=null)n++}return n});
def('COUNTBLANK','COUNTBLANK(range): how many are empty',(a,c)=>vals(a[0],c).filter(y=>y==null||y==='').length);
const pairs=(a,c,start)=>{const out=[];for(let i=start;i+1<a.length;i+=2){const rg=a[i]();if(!rg||!rg.rg)throw E.val;out.push([rangeMatrix(rg.rg,c).flat(),crit(scalar(a[i+1](),c))])}return out};
const condIdx=(ps)=>{const n=ps[0][0].length;if(ps.some(p=>p[0].length!=n))throw E.val;const out=[];for(let i=0;i<n;i++)if(ps.every(([v,f])=>!isE(v[i])&&f(v[i])))out.push(i);return out};
def('COUNTIF','COUNTIF(range, criteria): how many match, such as ">10" or "a*"',(a,c)=>condIdx(pairs(a,c,0)).length);
def('COUNTIFS','COUNTIFS(range1, criteria1, …)',(a,c)=>condIdx(pairs(a,c,0)).length);
const sumOver=(vs,idx)=>idx.reduce((s,i)=>s+(typeof vs[i]=='number'?vs[i]:0),0);
def('SUMIF','SUMIF(range, criteria, sum_range)',(a,c)=>{const ps=pairs(a.slice(0,2),c,0);const sr=a[2]?rangeMatrix(a[2]().rg,c).flat():ps[0][0];return sumOver(sr,condIdx(ps))});
def('SUMIFS','SUMIFS(sum_range, range1, criteria1, …)',(a,c)=>{const sr=rangeMatrix(a[0]().rg,c).flat();return sumOver(sr,condIdx(pairs(a,c,1)))});
def('AVERAGEIF','AVERAGEIF(range, criteria, average_range)',(a,c)=>{const ps=pairs(a.slice(0,2),c,0);const sr=a[2]?rangeMatrix(a[2]().rg,c).flat():ps[0][0];const idx=condIdx(ps).filter(i=>typeof sr[i]=='number');if(!idx.length)throw E.div;return sumOver(sr,idx)/idx.length});
def('AVERAGEIFS','AVERAGEIFS(average_range, range1, criteria1, …)',(a,c)=>{const sr=rangeMatrix(a[0]().rg,c).flat();const idx=condIdx(pairs(a,c,1)).filter(i=>typeof sr[i]=='number');if(!idx.length)throw E.div;return sumOver(sr,idx)/idx.length});
def('MAXIFS','MAXIFS(max_range, range1, criteria1, …)',(a,c)=>{const sr=rangeMatrix(a[0]().rg,c).flat();const v=condIdx(pairs(a,c,1)).map(i=>sr[i]).filter(x=>typeof x=='number');return v.length?Math.max(...v):0});
def('MINIFS','MINIFS(min_range, range1, criteria1, …)',(a,c)=>{const sr=rangeMatrix(a[0]().rg,c).flat();const v=condIdx(pairs(a,c,1)).map(i=>sr[i]).filter(x=>typeof x=='number');return v.length?Math.min(...v):0});
def('CORREL','CORREL(array1, array2): the correlation',(a,c)=>{const x=vals(a[0],c),y=vals(a[1],c);if(x.length!=y.length)throw E.na;const p=[];x.forEach((v,i)=>{if(typeof v=='number'&&typeof y[i]=='number')p.push([v,y[i]])});if(p.length<2)throw E.div;const mx=p.reduce((s,q)=>s+q[0],0)/p.length,my=p.reduce((s,q)=>s+q[1],0)/p.length;let sxy=0,sx=0,sy=0;p.forEach(([u,v])=>{sxy+=(u-mx)*(v-my);sx+=(u-mx)**2;sy+=(v-my)**2});if(!sx||!sy)throw E.div;return sxy/Math.sqrt(sx*sy)});
/* logic */
def('IF','IF(test, value if true, value if false)',(a,c)=>{const t=bool(scalar(a[0](),c));return t?(a[1]?a[1]():true):(a[2]?a[2]():false)});
def('IFS','IFS(test1, value1, test2, value2, …): the value of the first true test',(a,c)=>{for(let i=0;i+1<a.length;i+=2)if(bool(scalar(a[i](),c)))return a[i+1]();throw E.na});
def('IFERROR','IFERROR(value, value if error)',(a,c)=>{try{const v=scalar(a[0](),c);if(isE(v))return a[1]();return v}catch(e){if(isE(e))return a[1]();throw e}});
def('IFNA','IFNA(value, value if #N/A)',(a,c)=>{try{const v=scalar(a[0](),c);if(isE(v)&&v.e=='#N/A')return a[1]();return v}catch(e){if(isE(e)&&e.e=='#N/A')return a[1]();throw e}});
def('AND','AND(test1, …): TRUE if all are true',(a,c)=>a.every(x=>{const v=x();return v&&(v.rg||v.arr)?vals(()=>v,c).filter(y=>typeof y=='boolean'||typeof y=='number').every(y=>bool(y)):bool(v)}));
def('OR','OR(test1, …): TRUE if any is true',(a,c)=>a.some(x=>{const v=x();return v&&(v.rg||v.arr)?vals(()=>v,c).filter(y=>typeof y=='boolean'||typeof y=='number').some(y=>bool(y)):bool(v)}));
def('XOR','XOR(test1, …): TRUE if an odd number are true',(a,c)=>a.reduce((s,x)=>s^bool(scalar(x(),c)),false)==1);
def('NOT','NOT(test)',(a,c)=>!bool(scalar(a[0](),c)));
def('TRUE','TRUE()',()=>true);def('FALSE','FALSE()',()=>false);
def('SWITCH','SWITCH(value, match1, result1, …, default)',(a,c)=>{const v=scalar(a[0](),c);let i=1;for(;i+1<a.length;i+=2)if(cmpVals(v,scalar(a[i](),c))==0&&typeof v==typeof scalar(a[i](),c))return a[i+1]();if(i<a.length)return a[i]();throw E.na});
def('CHOOSE','CHOOSE(index, value1, value2, …)',(a,c)=>{const i=Math.trunc(num(scalar(a[0](),c)));if(i<1||i>=a.length)throw E.val;return a[i]()});
def('ISBLANK','ISBLANK(value)',(a,c)=>{const v=scalar(a[0](),c);return v==null});
def('ISNUMBER','ISNUMBER(value)',(a,c)=>typeof scalar(a[0](),c)=='number');def('ISTEXT','ISTEXT(value)',(a,c)=>typeof scalar(a[0](),c)=='string');
def('ISLOGICAL','ISLOGICAL(value)',(a,c)=>typeof scalar(a[0](),c)=='boolean');
def('ISERROR','ISERROR(value)',(a,c)=>{try{return isE(scalar(a[0](),c))}catch(e){return isE(e)}});def('ISERR','ISERR(value): an error other than #N/A',(a,c)=>{try{const v=scalar(a[0](),c);return isE(v)&&v.e!='#N/A'}catch(e){return isE(e)&&e.e!='#N/A'}});
def('ISNA','ISNA(value)',(a,c)=>{try{const v=scalar(a[0](),c);return isE(v)&&v.e=='#N/A'}catch(e){return isE(e)&&e.e=='#N/A'}});
def('ISEVEN','ISEVEN(number)',n1(x=>Math.trunc(x)%2==0));def('ISODD','ISODD(number)',n1(x=>Math.abs(Math.trunc(x))%2==1));
def('NA','NA(): #N/A',()=>{throw E.na});
/* lookup */
const lookIdx=(x,list,mode)=>{/* mode 0: exact (wildcards in text), 1: largest <= x (sorted), -1: smallest >= x (sorted descending) */
 if(mode==0){const f=typeof x=='string'&&/[*?]/.test(x)?crit(x):null;for(let i=0;i<list.length;i++){const v=list[i];if(f?f(v):(v!=null&&cmpVals(v,x)==0&&(typeof v==typeof x||typeof v!='boolean'&&typeof x!='boolean'&&(typeof v=='number')==(typeof x=='number'))))return i}return -1}
 if(mode==1){let lo=0,hi=list.length-1,ans=-1;while(lo<=hi){const m=(lo+hi)>>1;if(list[m]==null||cmpVals(list[m],x)<=0){ans=m;lo=m+1}else hi=m-1}return ans}
 let ans=-1;for(let i=0;i<list.length;i++){if(cmpVals(list[i],x)>=0)ans=i;else break}return ans};
def('VLOOKUP','VLOOKUP(value, table, column, exact): looks down the first column, gives the value in that column (FALSE: exact match)',(a,c)=>{const x=scalar(a[0](),c),m=matrix(a[1],c),k=Math.trunc(num(scalar(a[2](),c))),appr=a[3]?bool(scalar(a[3](),c)):true;if(k<1)throw E.val;if(m[0]&&k>m[0].length)throw E.ref;const i=lookIdx(x,m.map(r=>r[0]),appr?1:0);if(i<0)throw E.na;return m[i][k-1]??null});
def('HLOOKUP','HLOOKUP(value, table, row, exact): looks along the first row',(a,c)=>{const x=scalar(a[0](),c),m=matrix(a[1],c),k=Math.trunc(num(scalar(a[2](),c))),appr=a[3]?bool(scalar(a[3](),c)):true;if(k<1)throw E.val;if(k>m.length)throw E.ref;const i=lookIdx(x,m[0],appr?1:0);if(i<0)throw E.na;return m[k-1][i]??null});
def('MATCH','MATCH(value, range, type): its position (type 0: exact)',(a,c)=>{const x=scalar(a[0](),c),l=vals(a[1],c),t=a[2]?num(scalar(a[2](),c)):1;const i=lookIdx(x,l,t>0?1:t<0?-1:0);if(i<0)throw E.na;return i+1});
def('XLOOKUP','XLOOKUP(value, lookup_range, return_range, if_not_found, match_mode, search_mode)',(a,c)=>{const x=scalar(a[0](),c),l=vals(a[1],c),rv=matrix(a[2],c);const mm=a[4]?num(scalar(a[4](),c)):0,sm=a[5]?num(scalar(a[5](),c)):1;
 let idx=[...l.keys()];if(sm==-1)idx.reverse();let hit=-1;
 if(mm==2||mm==0){const f=mm==2?crit(x):null;for(const i of idx){const v=l[i];if(f?f(v):(v!=null&&cmpVals(v,x)==0)){hit=i;break}}}
 if(hit<0&&(mm==-1||mm==1)){let best=-1;for(const i of idx){const v=l[i];if(v==null)continue;const d=cmpVals(v,x);if(d==0){best=i;break}if(mm==-1&&d<0&&(best<0||cmpVals(v,l[best])>0))best=i;if(mm==1&&d>0&&(best<0||cmpVals(v,l[best])<0))best=i}hit=best}
 if(hit<0){if(a[3])return a[3]();throw E.na}
 const vert=rv.length==l.length;return vert?(rv[hit].length==1?rv[hit][0]:{arr:[rv[hit]]}):(rv.length==1?rv[0][hit]:{arr:rv.map(r=>[r[hit]])})});
def('INDEX','INDEX(range, row, column): the value at that place',(a,c)=>{const v=a[0]();const r=Math.trunc(nOpt(a,1,0,c)),k=Math.trunc(nOpt(a,2,0,c));
 if(v&&v.rg){const g=v.rg,h=g.r1-g.r0+1,w=g.c1-g.c0+1;let rr=r,kk=k;if(h==1&&!a[2]){kk=r;rr=1}if(rr<0||kk<0||rr>h||kk>w)throw E.ref;
  if(rr==0&&kk==0)return v;if(rr==0)return{rg:{s:g.s,r0:g.r0,r1:g.r1,c0:g.c0+kk-1,c1:g.c0+kk-1}};if(kk==0)return{rg:{s:g.s,r0:g.r0+rr-1,r1:g.r0+rr-1,c0:g.c0,c1:g.c1}};return{rg:{s:g.s,r0:g.r0+rr-1,r1:g.r0+rr-1,c0:g.c0+kk-1,c1:g.c0+kk-1}}}
 const m=matrix(()=>v,c);const rr=m.length==1&&!a[2]?1:r,kk=m.length==1&&!a[2]?r:k;const row=m[(rr||1)-1];if(!row)throw E.ref;const x=row[(kk||1)-1];if(x===undefined)throw E.ref;return x});
def('OFFSET','OFFSET(reference, rows, columns, height, width): a range moved by that much',(a,c)=>{const v=a[0]();if(!v||!v.rg)throw E.val;const g=v.rg,dr=Math.trunc(num(scalar(a[1](),c))),dc=Math.trunc(num(scalar(a[2](),c)));const h=a[3]?Math.trunc(num(scalar(a[3](),c))):g.r1-g.r0+1,w=a[4]?Math.trunc(num(scalar(a[4](),c))):g.c1-g.c0+1;const r0=g.r0+dr,c0=g.c0+dc;if(r0<0||c0<0||h<1||w<1||r0+h>MAXR||c0+w>MAXC)throw E.ref;return{rg:{s:g.s,r0,c0,r1:r0+h-1,c1:c0+w-1}}});
def('INDIRECT','INDIRECT(text): the cell or range named by the text',(a,c)=>{const s=str(scalar(a[0](),c));try{const n=parse(s);if(n.t!='ref'&&n.t!='rng')throw E.ref;const g=rangeOf(n,c);if(!g)throw E.ref;return{rg:g}}catch(e){throw E.ref}});
def('ROW','ROW(reference): its row number',(a,c)=>{if(!a.length)return c.r+1;const v=a[0]();if(!v||!v.rg)throw E.val;return v.rg.r0+1});
def('COLUMN','COLUMN(reference): its column number',(a,c)=>{if(!a.length)return c.c+1;const v=a[0]();if(!v||!v.rg)throw E.val;return v.rg.c0+1});
def('ROWS','ROWS(range): how many rows',(a,c)=>{const v=a[0]();if(v&&v.rg)return v.rg.r1-v.rg.r0+1;return matrix(()=>v,c).length});
def('COLUMNS','COLUMNS(range): how many columns',(a,c)=>{const v=a[0]();if(v&&v.rg)return v.rg.c1-v.rg.c0+1;return matrix(()=>v,c)[0].length});
def('ADDRESS','ADDRESS(row, column, abs): a cell address as text',(a,c)=>{const r=Math.trunc(num(scalar(a[0](),c))),k=Math.trunc(num(scalar(a[1](),c))),t=a[2]?num(scalar(a[2](),c)):1;if(r<1||k<1)throw E.val;const d1=t==1||t==3?'$':'',d2=t==1||t==2?'$':'';return d1+colName(k-1)+d2+r});
/* text */
const s1=f=>(a,c)=>f(str(scalar(a[0](),c)),a,c);
def('LEFT','LEFT(text, count): the first characters',s1((s,a,c)=>{const n=nOpt(a,1,1,c);if(n<0)throw E.val;return s.slice(0,n)}));
def('RIGHT','RIGHT(text, count): the last characters',s1((s,a,c)=>{const n=nOpt(a,1,1,c);if(n<0)throw E.val;return n?s.slice(-n):''}));
def('MID','MID(text, start, count)',s1((s,a,c)=>{const st=num(scalar(a[1](),c)),n=num(scalar(a[2](),c));if(st<1||n<0)throw E.val;return s.substr(st-1,n)}));
def('LEN','LEN(text): how many characters',s1(s=>s.length));
def('LOWER','LOWER(text)',s1(s=>s.toLowerCase()));def('UPPER','UPPER(text)',s1(s=>s.toUpperCase()));
def('PROPER','PROPER(text): Capitals For Each Word',s1(s=>s.toLowerCase().replace(/(^|[^a-z\u00c0-\u024f])([a-z\u00c0-\u024f])/g,(m,p,ch)=>p+ch.toUpperCase())));
def('TRIM','TRIM(text): without extra spaces',s1(s=>s.replace(/ +/g,' ').trim()));
def('CLEAN','CLEAN(text): without control characters',s1(s=>s.replace(/[\x00-\x1f]/g,'')));
def('FIND','FIND(find, within, start): its position (case matters)',(a,c)=>{const f=str(scalar(a[0](),c)),w=str(scalar(a[1](),c)),st=nOpt(a,2,1,c);const i=w.indexOf(f,st-1);if(i<0||st<1)throw E.val;return i+1});
def('SEARCH','SEARCH(find, within, start): its position (case does not matter; * and ? allowed)',(a,c)=>{const f=str(scalar(a[0](),c)),w=str(scalar(a[1](),c)),st=nOpt(a,2,1,c);const re=new RegExp(f.replace(/[.+^${}()|[\]\\]/g,'\\$&').replace(/\*/g,'.*').replace(/\?/g,'.'),'i');const m=re.exec(w.slice(st-1));if(!m||st<1)throw E.val;return st+m.index});
def('SUBSTITUTE','SUBSTITUTE(text, old, new, which)',(a,c)=>{const s=str(scalar(a[0](),c)),o=str(scalar(a[1](),c)),n=str(scalar(a[2](),c));if(!o)return s;if(a[3]){const k=num(scalar(a[3](),c));let i=-1;for(let j=0;j<k;j++){i=s.indexOf(o,i+1);if(i<0)return s}return s.slice(0,i)+n+s.slice(i+o.length)}return s.split(o).join(n)});
def('REPLACE','REPLACE(text, start, count, new)',(a,c)=>{const s=str(scalar(a[0](),c)),st=num(scalar(a[1](),c)),n=num(scalar(a[2](),c)),t=str(scalar(a[3](),c));return s.slice(0,st-1)+t+s.slice(st-1+n)});
def('CONCAT,CONCATENATE','CONCAT(text1, …): joined together',(a,c)=>a.map(x=>{const v=x();return v&&(v.rg||v.arr)?vals(()=>v,c).map(str).join(''):str(v)}).join(''));
def('TEXTJOIN','TEXTJOIN(delimiter, ignore_empty, text1, …)',(a,c)=>{const d=str(scalar(a[0](),c)),ig=bool(scalar(a[1](),c));const parts=[];a.slice(2).forEach(x=>{const v=x();(v&&(v.rg||v.arr)?vals(()=>v,c):[v]).forEach(y=>{const s=str(y);if(!(ig&&s===''))parts.push(s)})});return parts.join(d)});
def('REPT','REPT(text, times)',(a,c)=>{const n=num(scalar(a[1](),c));if(n<0||n>32767)throw E.val;return str(scalar(a[0](),c)).repeat(n)});
def('EXACT','EXACT(text1, text2): TRUE if exactly the same',(a,c)=>str(scalar(a[0](),c))===str(scalar(a[1](),c)));
def('VALUE','VALUE(text): the number in it',(a,c)=>num(scalar(a[0](),c)));
def('T','T(value): the text, or empty',(a,c)=>{const v=scalar(a[0](),c);return typeof v=='string'?v:''});
def('N','N(value): the number, or 0',(a,c)=>{const v=scalar(a[0](),c);return typeof v=='number'?v:typeof v=='boolean'?+v:0});
def('CHAR','CHAR(number): the character',n1(x=>String.fromCharCode(x)));def('CODE','CODE(text): the first character\'s number',s1(s=>{if(!s)throw E.val;return s.charCodeAt(0)}));
def('TEXT','TEXT(value, format): as text in a number format, such as "0.00" or "yyyy-mm-dd"',(a,c)=>{const v=scalar(a[0](),c),f=str(scalar(a[1](),c));return fmtValue(typeof v=='string'&&!isNaN(+v)?+v:v,f)});
def('FIXED','FIXED(number, decimals, no_commas)',(a,c)=>{const x=num(scalar(a[0](),c)),d=nOpt(a,1,2,c),nc=a[2]?bool(scalar(a[2](),c)):false;return fmtValue(x,(nc?'0':'#,##0')+(d>0?'.'+'0'.repeat(d):''))});
def('DOLLAR','DOLLAR(number, decimals)',(a,c)=>fmtValue(num(scalar(a[0](),c)),'$#,##0'+(nOpt(a,1,2,c)>0?'.'+'0'.repeat(nOpt(a,1,2,c)):'')));
/* date and time */
const nowSerial=()=>{const d=new Date();return(Date.UTC(d.getFullYear(),d.getMonth(),d.getDate(),d.getHours(),d.getMinutes(),d.getSeconds())-EPOCH)/864e5};
def('TODAY','TODAY(): today\'s date (changes every day)',()=>Math.floor(nowSerial()));
def('NOW','NOW(): the date and time now',()=>nowSerial());
def('DATE','DATE(year, month, day)',(a,c)=>{let y=Math.trunc(num(scalar(a[0](),c)));const m=Math.trunc(num(scalar(a[1](),c))),d=Math.trunc(num(scalar(a[2](),c)));if(y<1900)y+=1900;return(Date.UTC(y,m-1,d)-EPOCH)/864e5});
def('TIME','TIME(hour, minute, second)',(a,c)=>{const h=num(scalar(a[0](),c)),m=num(scalar(a[1](),c)),s=num(scalar(a[2](),c));const t=(h*3600+m*60+s)/86400;return t-Math.floor(t)});
def('YEAR','YEAR(date)',n1(x=>fromSerial(x).y));def('MONTH','MONTH(date)',n1(x=>fromSerial(x).m));def('DAY','DAY(date)',n1(x=>fromSerial(x).d));
def('HOUR','HOUR(time)',n1(x=>Math.floor((x-Math.floor(x))*24+1e-9)));def('MINUTE','MINUTE(time)',n1(x=>Math.floor((x-Math.floor(x))*1440+1e-7)%60));def('SECOND','SECOND(time)',n1(x=>Math.round((x-Math.floor(x))*86400)%60));
def('WEEKDAY','WEEKDAY(date, type): 1 (Sunday) to 7; type 2: 1 (Monday) to 7',(a,c)=>{const w=fromSerial(num(scalar(a[0](),c))).wd,t=nOpt(a,1,1,c);return t==2?(w+6)%7+1:t==3?(w+6)%7:w+1});
def('WEEKNUM','WEEKNUM(date): the week of the year',(a,c)=>{const x=num(scalar(a[0](),c)),d=fromSerial(x),j=toSerial(d.y,1,1),w0=fromSerial(j).wd;return Math.floor((x-j+w0)/7)+1});
def('EDATE','EDATE(date, months): the same day that many months later',(a,c)=>{const d=fromSerial(num(scalar(a[0](),c))),m=Math.trunc(num(scalar(a[1](),c)));const last=new Date(Date.UTC(d.y,d.m-1+m+1,0)).getUTCDate();return toSerial(d.y,d.m+m,Math.min(d.d,last))});
def('EOMONTH','EOMONTH(date, months): the last day of the month that many months later',(a,c)=>{const d=fromSerial(num(scalar(a[0](),c))),m=Math.trunc(num(scalar(a[1](),c)));return(Date.UTC(d.y,d.m+m,0)-EPOCH)/864e5});
def('DATEDIF','DATEDIF(start, end, unit): "Y", "M" or "D" between them',(a,c)=>{const s=num(scalar(a[0](),c)),e=num(scalar(a[1](),c)),u=str(scalar(a[2](),c)).toUpperCase();if(e<s)throw E.num;const A=fromSerial(s),B=fromSerial(e);
 let mo=(B.y-A.y)*12+B.m-A.m-(B.d<A.d?1:0);if(u=='Y')return Math.floor(mo/12);if(u=='M')return mo;if(u=='D')return Math.floor(e)-Math.floor(s);if(u=='YM')return mo%12;if(u=='MD'){return B.d>=A.d?B.d-A.d:B.d+new Date(Date.UTC(B.y,B.m-1,0)).getUTCDate()-A.d}throw E.num});
def('DAYS','DAYS(end, start)',(a,c)=>Math.floor(num(scalar(a[0](),c)))-Math.floor(num(scalar(a[1](),c))));
def('NETWORKDAYS','NETWORKDAYS(start, end, holidays): working days (Monday to Friday)',(a,c)=>{let s=Math.floor(num(scalar(a[0](),c))),e=Math.floor(num(scalar(a[1](),c)));const sg=e<s?-1:1;if(e<s)[s,e]=[e,s];const hol=new Set(a[2]?flat([a[2]],c).map(Math.floor):[]);let n=0;for(let x=s;x<=e;x++){const w=fromSerial(x).wd;if(w&&w<6&&!hol.has(x))n++}return n*sg});
def('WORKDAY','WORKDAY(start, days, holidays): the date that many working days later',(a,c)=>{let x=Math.floor(num(scalar(a[0](),c)));let n=Math.trunc(num(scalar(a[1](),c)));const hol=new Set(a[2]?flat([a[2]],c).map(Math.floor):[]);const st=n<0?-1:1;while(n){x+=st;const w=fromSerial(x).wd;if(w&&w<6&&!hol.has(x))n-=st}return x});
def('DATEVALUE','DATEVALUE(text): the date in it',(a,c)=>{const d=parseDate(str(scalar(a[0](),c)).trim());if(d==null)throw E.val;return d});
/* finance */
def('PMT','PMT(rate, periods, present_value, future_value, type): the payment each period',(a,c)=>{const r=num(scalar(a[0](),c)),n=num(scalar(a[1](),c)),pv=num(scalar(a[2](),c)),fv=nOpt(a,3,0,c),t=nOpt(a,4,0,c);if(!n)throw E.num;if(!r)return-(pv+fv)/n;const f=Math.pow(1+r,n);return-(r*(pv*f+fv))/((1+r*t)*(f-1))});
def('FV','FV(rate, periods, payment, present_value, type): the value at the end',(a,c)=>{const r=num(scalar(a[0](),c)),n=num(scalar(a[1](),c)),p=num(scalar(a[2](),c)),pv=nOpt(a,3,0,c),t=nOpt(a,4,0,c);if(!r)return-(pv+p*n);const f=Math.pow(1+r,n);return-(pv*f+p*(1+r*t)*(f-1)/r)});
def('PV','PV(rate, periods, payment, future_value, type): the value now',(a,c)=>{const r=num(scalar(a[0](),c)),n=num(scalar(a[1](),c)),p=num(scalar(a[2](),c)),fv=nOpt(a,3,0,c),t=nOpt(a,4,0,c);if(!r)return-(fv+p*n);const f=Math.pow(1+r,n);return-(fv+p*(1+r*t)*(f-1)/r)/f});
def('NPER','NPER(rate, payment, present_value, future_value, type): how many periods',(a,c)=>{const r=num(scalar(a[0](),c)),p=num(scalar(a[1](),c)),pv=num(scalar(a[2](),c)),fv=nOpt(a,3,0,c),t=nOpt(a,4,0,c);if(!r){if(!p)throw E.num;return-(pv+fv)/p}const x=p*(1+r*t)/r;const v=(x-fv)/(x+pv);if(v<=0)throw E.num;return Math.log(v)/Math.log(1+r)});
def('NPV','NPV(rate, value1, …): the net present value',(a,c)=>{const r=num(scalar(a[0](),c));return flat(a.slice(1),c).reduce((s,v,i)=>s+v/Math.pow(1+r,i+1),0)});
def('IRR','IRR(values, guess): the internal rate of return',(a,c)=>{const v=flat([a[0]],c);let r=a[1]?num(scalar(a[1](),c)):0.1;for(let k=0;k<100;k++){let f=0,d=0;v.forEach((x,i)=>{f+=x/Math.pow(1+r,i);d-=i*x/Math.pow(1+r,i+1)});if(Math.abs(f)<1e-10)return r;if(!d)break;const nr=r-f/d;if(Math.abs(nr-r)<1e-12)return nr;r=nr}throw E.num});
def('RATE','RATE(periods, payment, present_value, future_value, type, guess): the interest rate',(a,c)=>{const n=num(scalar(a[0](),c)),p=num(scalar(a[1](),c)),pv=num(scalar(a[2](),c)),fv=nOpt(a,3,0,c),t=nOpt(a,4,0,c);let r=nOpt(a,5,0.1,c);
 const g=r=>{if(Math.abs(r)<1e-12)return pv+p*n+fv;const f=Math.pow(1+r,n);return pv*f+p*(1+r*t)*(f-1)/r+fv};for(let k=0;k<100;k++){const y=g(r),h=1e-7,dy=(g(r+h)-y)/h;if(!dy)break;const nr=r-y/dy;if(Math.abs(nr-r)<1e-12)return nr;r=nr}throw E.num});
def('SLN','SLN(cost, salvage, life): straight-line depreciation',(a,c)=>{const l=num(scalar(a[2](),c));if(!l)throw E.div;return(num(scalar(a[0](),c))-num(scalar(a[1](),c)))/l});
/* information */
def('TYPE','TYPE(value): 1 number, 2 text, 4 logical, 16 error, 64 array',(a,c)=>{try{const v=a[0]();if(v&&(v.rg&&(v.rg.r0!=v.rg.r1||v.rg.c0!=v.rg.c1)||v.arr))return 64;const s=scalar(v,c);return isE(s)?16:typeof s=='number'||s==null?1:typeof s=='string'?2:4}catch(e){return 16}});
def('ERROR.TYPE','ERROR.TYPE(error): its number',(a,c)=>{let v;try{v=scalar(a[0](),c)}catch(e){v=e}if(!isE(v))throw E.na;return{'#NULL!':1,'#DIV/0!':2,'#VALUE!':3,'#REF!':4,'#NAME?':5,'#NUM!':6,'#N/A':7}[v.e]||8});
/* ---- evaluation ---- */
 function ev(n,ctx){switch(n.t){
 case'num':return n.v;case'str':return n.v;case'bool':return n.v;case'err':return ERR(n.v);case'blank':return null;
 case'arr':return{arr:n.v};
 case'ref':case'rng':{const g=rangeOf(n,ctx);if(!g)throw E.ref;return{rg:g}}
 case'name':{const nm=ctx.names&&ctx.names[n.v];if(!nm)throw E.name;return ev(nm.ast||(nm.ast=parse(nm.ref)),Object.assign({},ctx,{sheet:ctx.sheetByName(nm.sheet)||ctx.sheet}))}
 case'un':{const v=num(scalar(ev(n.a,ctx),ctx));return n.o=='-'?-v:v}
 case'pct':return num(scalar(ev(n.a,ctx),ctx))/100;
 case'fn':{const f=F[n.f];if(!f)throw E.name;const args=n.args.map(x=>{const th=()=>{const v=ev(x,ctx);if(isE(v))throw v;return v};th.blank=x.t=='blank';return th});return f(args,ctx)}
 case'bin':{let l=scalar(ev(n.l,ctx),ctx),r=scalar(ev(n.r,ctx),ctx);if(isE(l))throw l;if(isE(r))throw r;
  switch(n.o){case'+':return chk(num(l)+num(r));case'-':return chk(num(l)-num(r));case'*':return chk(num(l)*num(r));case'/':{const d=num(r);if(d==0)throw E.div;return chk(num(l)/d)}case'^':return chk(Math.pow(num(l),num(r)));case'&':return str(l)+str(r);
   default:{const d=cmpVals(l,r);return{'=':d==0,'<>':d!=0,'<':d<0,'>':d>0,'<=':d<=0,'>=':d>=0}[n.o]}}}}
 throw E.val}
/* ---- the book: every formula cell is worked out once per change, in order, with the values remembered ---- */
let GEN=0;
 function calc(book,only){const gen=++GEN;
 const byName=nm=>book.sheets.find(s=>s.name.toUpperCase()==String(nm).toUpperCase());
 const names=book.names||{};
 function value(s,r,c){const cell=s.cells.get(r*MAXC+c);if(!cell)return null;if(cell.f==null)return cell.v;
  if(cell._g===gen||only&&!only.has(cell))return cell.val;if(cell._b===gen){cell._g=gen;cell.val=E.cyc;return E.cyc}cell._b=gen;
  let v;try{if(!cell.ast)cell.ast=parse(cell.f.slice(1));const cx={sheetByName:byName,names,sheet:s,r,c,value};v=scalar(ev(cell.ast,cx),cx);if(v&&v.arr)v=v.arr[0][0];if(v===undefined)v=null}catch(e){v=isE(e)?e:E.val}
  cell._b=0;if(cell._g===gen&&cell.val===E.cyc)v=E.cyc;cell._g=gen;cell.val=v;return v}
 /* row by row: references usually point up or left, so chains are short. The list of formula cells is kept on the
    sheet until a cell changes (the app sets sheet.fk=null when it changes cells) */
 for(const s of book.sheets){if(!s.fk||s.fkn!==s.cells.size){s.fk=[];for(const[k,cl]of s.cells)if(cl.f!=null)s.fk.push(k);s.fk.sort((a,b)=>a-b);s.fkn=s.cells.size}
  if(!only)for(const k of s.fk)value(s,Math.floor(k/MAXC),k%MAXC)}
 if(only)for(const[,[s,r,c]]of only)value(s,r,c);
 return book}
/* what depends on what: the references in each formula, so a change recalculates only the cells it reaches.
   Formulas with OFFSET, INDIRECT, RAND, NOW, TODAY or names are worked out at every change */
const VOL=new Set(['OFFSET','INDIRECT','RAND','RANDBETWEEN','NOW','TODAY','INDEX']);
 function deps(book){const one=new Map(),rngs=[],vol=[];const byName=nm=>book.sheets.find(s=>s.name.toUpperCase()==String(nm).toUpperCase());
 for(const s of book.sheets){for(const[k,cl]of s.cells){if(cl.f==null)continue;try{if(!cl.ast)cl.ast=parse(cl.f.slice(1))}catch(e){continue}
  let v=false;const walk=n=>{if(!n||typeof n!='object')return;if(n.t=='fn'&&VOL.has(n.f))v=true;if(n.t=='name')v=true;
   if(n.t=='ref'||n.t=='rng'){const g=rangeOf(n,{sheetByName:byName,sheet:s});if(g){if(g.r0==g.r1&&g.c0==g.c1){const key=g.s.id+':'+(g.r0*MAXC+g.c0);let l=one.get(key);if(!l)one.set(key,l=[]);l.push([s,k,cl])}else rngs.push([g,s,k,cl])}}
   for(const x of n.args||[])walk(x);walk(n.l);walk(n.r);walk(n.a&&typeof n.a=='object'?n.a:null)};walk(cl.ast);if(v)vol.push([s,k,cl])}}
 return{one,rngs,vol}}
 function recalc(book,changed){/* changed: [[sheet,r,c],…]; null: everything */
 if(!changed||!book._dep){book._dep=deps(book);return calc(book)}
 const D=book._dep,dirty=new Map(),q=[];const push=(s,k,cl)=>{if(!dirty.has(cl)){const p=[s,Math.floor(k/MAXC),k%MAXC];dirty.set(cl,p);q.push(p)}};
 for(const[s,r,c]of changed){const cl=s.cells.get(r*MAXC+c);if(cl&&cl.f!=null)push(s,r*MAXC+c,cl);else q.push([s,r,c])}
 for(const[s,k,cl]of D.vol)push(s,k,cl);
 while(q.length){const[s,r,c]=q.pop();const l=D.one.get(s.id+':'+(r*MAXC+c));if(l)for(const[t,k,cl]of l)push(t,k,cl);
  for(const[g,t,k,cl]of D.rngs)if(g.s===s&&r>=g.r0&&r<=g.r1&&c>=g.c0&&c<=g.c1)push(t,k,cl)}
 return calc(book,dirty)}
 function evalIn(book,sheet,src){/* a formula worked out once, for the formula bar's preview and for tests */
 const s=book.sheets[sheet]||book.sheets[0],tmp={f:'='+src.replace(/^=/,'')},key=(MAXR-1)*MAXC+(MAXC-1),old=s.cells.get(key);s.cells.set(key,tmp);s.fk=null;calc(book);const v=tmp.val;if(old)s.cells.set(key,old);else s.cells.delete(key);s.fk=null;return v}
/* ---- number formats (a useful part of Excel's codes) ---- */
 function fmtValue(v,f){if(v==null)return'';if(isE(v))return v.e;if(typeof v=='boolean')return v?'TRUE':'FALSE';if(typeof v=='string'){if(f&&f.includes('@'))return f.replace(/"([^"]*)"/g,'$1').replace('@',v);return v}
 if(!f||f=='General')return fmtGeneral(v);
 const parts=f.split(';');let p=parts[0];if(v<0&&parts[1]!=null){p=parts[1];v=-v}else if(v==0&&parts[2]!=null)p=parts[2];else if(v<0&&!/[ymdhs]/i.test(p.replace(/"[^"]*"/g,'')))return'-'+fmtValue(-v,p);
 const lit=s=>s.replace(/"([^"]*)"/g,'$1').replace(/\\(.)/g,'$1').replace(/_.|\*./g,' ').replace(/\[[^\]]*\]/g,'');
 if(/[ymdhs]/i.test(p.replace(/"[^"]*"/g,''))&&!/^[#0,.%]+$/.test(p))return fmtDate(v,p);
 const pct=/%/.test(p);if(pct)v*=100;
 const m=/([#0,]+)(\.([0#]+))?|()\.([0#]+)/.exec(p.replace(/"[^"]*"/g,'').replace(/\[[^\]]*\]/g,''));
 if(m&&m[5]!=null){m[1]='';m[3]=m[5]}const sci=/E\+?0+/i.test(p);if(sci){const d=(m&&m[3]?m[3].length:0);return lit(p.replace(/[#0,.]+E\+?0+/i,v.toExponential(d).toUpperCase().replace(/E([+-])(\d)$/,'E$10$2')))}
 const dec=m&&m[3]?m[3].length:0,optDec=m&&m[3]?(m[3].match(/#/g)||[]).length:0,comma=/,/.test(m?m[1]:'');
 let s=Math.abs(v).toFixed(dec);if(optDec){s=s.replace(new RegExp('0{1,'+optDec+'}$'),'').replace(/\.$/,'')}
 let [ip,fp]=s.split('.');if(comma)ip=ip.replace(/\B(?=(\d{3})+(?!\d))/g,',');const minInt=m&&m[1]?(m[1].match(/0/g)||[]).length:1;if(ip=='0'&&minInt==0)ip='';
 const numS=ip+(fp!=null?'.'+fp:'');
 const body=p.replace(/"[^"]*"/g,x=>'\u0000'+x+'\u0000');
 return lit(body.replace(/[#0,]*\.[0#]+%?|[#0,]+%?|%/,numS+(pct?'%':'')).replace(/\u0000/g,'')).replace(/%%$/,'%')}
 function fmtDate(v,p){const d=new Date(EPOCH+Math.round(v*864e5));const Y=d.getUTCFullYear(),M=d.getUTCMonth()+1,Dd=d.getUTCDate(),h=d.getUTCHours(),mi=d.getUTCMinutes(),se=d.getUTCSeconds();
 const MN=['January','February','March','April','May','June','July','August','September','October','November','December'],DN=['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
 const ampm=/AM\/PM/i.test(p);const p2=x=>String(x).padStart(2,'0');let out='',i=0;const s=p;
 while(i<s.length){const r=s.slice(i);let m;
  if(r[0]=='"'){const j=s.indexOf('"',i+1);out+=s.slice(i+1,j);i=j+1;continue}
  if(m=/^yyyy/i.exec(r)){out+=Y;i+=4;continue}if(m=/^yy/i.exec(r)){out+=p2(Y%100);i+=2;continue}
  if(m=/^mmmm/i.exec(r)){out+=MN[M-1];i+=4;continue}if(m=/^mmm/i.exec(r)){out+=MN[M-1].slice(0,3);i+=3;continue}
  if(m=/^(mm?)/i.exec(r)){const minute=/h[^dmy]*$/i.test(s.slice(0,i))||/^mm?:?s/i.test(r.replace(/^m+/,'').replace(/^:/,'s')&&r)&&/[:]s/i.test(r);const isMin=/h\S*:$/i.test(s.slice(0,i))||/^mm?:ss?/i.test(r);out+=isMin?p2(mi):(m[1].length==2?p2(M):M);i+=m[1].length;continue}
  if(m=/^dddd/i.exec(r)){out+=DN[d.getUTCDay()];i+=4;continue}if(m=/^ddd/i.exec(r)){out+=DN[d.getUTCDay()].slice(0,3);i+=3;continue}if(m=/^(dd?)/i.exec(r)){out+=m[1].length==2?p2(Dd):Dd;i+=m[1].length;continue}
  if(m=/^(hh?)/i.exec(r)){let H=h;if(ampm)H=H%12||12;out+=m[1].length==2?p2(H):H;i+=m[1].length;continue}
  if(m=/^(ss?)/i.exec(r)){out+=m[1].length==2?p2(se):se;i+=m[1].length;continue}
  if(m=/^AM\/PM/i.exec(r)){out+=h<12?'AM':'PM';i+=5;continue}
  out+=r[0]=='\\'?r[1]:r[0];i+=r[0]=='\\'?2:1}
 return out}
/* references moved when rows or columns are inserted or deleted (and when cells are copied) */
 function shiftFormula(f,fn){/* fn(r,c,absR,absC) -> [r,c] or null (#REF!) */
 return f.replace(/(^|[^A-Za-z0-9_."'!])((?:'[^']+'|[A-Za-z_][A-Za-z0-9_]*)!)?(\$?)([A-Za-z]{1,3})(\$?)(\d+)(?![A-Za-z0-9_(])/g,(all,pre,sh,dc,col,dr,row)=>{
  const c=colNum(col),r=+row-1;if(c>=MAXC)return all;const x=fn(r,c,!!dr,!!dc,sh?sh.slice(0,-1).replace(/^'|'$/g,''):null);if(!x)return pre+(sh||'')+'#REF!';return pre+(sh||'')+dc+colName(x[1])+dr+(x[0]+1)})}
 function copyFormula(f,dr,dc){return shiftFormula(f,(r,c,ar,ac)=>{const nr=ar?r:r+dr,nc=ac?c:c+dc;return nr<0||nc<0||nr>=MAXR||nc>=MAXC?null:[nr,nc]})}
return{parse,tokenize,calc,recalc,deps,evalIn,fmtValue,fmtGeneral,colName,colNum,a1,parseA1,F,D,E,isE,MAXC,MAXR,toSerial,fromSerial,parseDate,copyFormula,shiftFormula,cmpVals}})();

/* ===== 1.4.5 (sprof45): SpreadsheetPro's book and its files, without the page: a new book, typed input, .RSHEET
   (version 2, and version 1 from the old Spreadsheet), CSV, Excel .xlsx (read and written through JSZip), templates.
   A book: {sheets:[sheet],names:{NAME:{ref,sheet}},active}. A sheet: {id,name,color,cells:Map(r*702+c → cell),
   cw:{c:px},rh:{r:px},hid:{r:1},hc:{c:1},freeze:{r,c},merges:[{r0,c0,r1,c1}],cf:[rule],dv:[rule],cm:{k:text},
   charts:[chart],filter,grid,print}. A cell: {v} or {f:'=…'} (its result in .val), and st, a style object that is
   never changed in place (a change makes a new one). ===== */
const SPM=SPE.MAXC,SPR=SPE.MAXR;
function spSheet(name,id){return{id,name,color:'',cells:new Map(),cw:{},rh:{},hid:{},hc:{},freeze:{r:0,c:0},merges:[],cf:[],dv:[],cm:{},charts:[],filter:null,grid:true,print:{}}}
function spBookNew(){return{sheets:[spSheet('Sheet1',1)],names:{},nid:2,active:0}}
const spK=(r,c)=>r*SPM+c,spRC=k=>[Math.floor(k/SPM),k%SPM];
const spVal=cell=>cell?(cell.f!=null?cell.val:cell.v):null;
const spText=cell=>cell?SPE.fmtValue(spVal(cell),cell.st&&cell.st.nf):'';
function spUsed(s){let r1=-1,c1=-1;for(const k of s.cells.keys()){const r=Math.floor(k/SPM),c=k%SPM;if(r>r1)r1=r;if(c>c1)c1=c}return{r1,c1}}
function spRangeStr(g){return SPE.a1(g.r0,g.c0)+(g.r0==g.r1&&g.c0==g.c1?'':':'+SPE.a1(g.r1,g.c1))}
function spParseRange(t){t=String(t||'').replace(/\$/g,'').trim().toUpperCase();let m=/^([A-Z]{1,3}\d+)(?::([A-Z]{1,3}\d+))?$/.exec(t);
 if(m){const a=SPE.parseA1(m[1]),b=SPE.parseA1(m[2]||m[1]);if(!a||!b)return null;return{r0:Math.min(a.r,b.r),c0:Math.min(a.c,b.c),r1:Math.max(a.r,b.r),c1:Math.max(a.c,b.c)}}
 if((m=/^([A-Z]{1,3}):([A-Z]{1,3})$/.exec(t))){const a=SPE.colNum(m[1]),b=SPE.colNum(m[2]);return{r0:0,r1:SPR-1,c0:Math.min(a,b),c1:Math.max(a,b)}}
 if((m=/^(\d+):(\d+)$/.exec(t)))return{r0:Math.min(+m[1],+m[2])-1,r1:Math.max(+m[1],+m[2])-1,c0:0,c1:SPM-1};return null}
/* formulas are kept with their names and references in capitals (text in quotes as typed) */
function spNormF(f){let out='',i=0;while(i<f.length){const ch=f[i];if(ch=='"'||ch=="'"){const j=f.indexOf(ch,i+1);const e=j<0?f.length:j+1;out+=f.slice(i,e);i=e;continue}out+=ch.toUpperCase();i++}return out}
/* what a typed text means: {v} (number, text, TRUE/FALSE) or {f}, and the number format it implies */
function spParse(t){if(t==null)return null;t=String(t);if(t==='')return null;
 if(t[0]=="'")return{v:t.slice(1)};
 if(t[0]=='='&&t.length>1)return{f:spNormF(t)};
 const s=t.trim();let m;
 if((m=/^([+-]?)\$\s?((\d{1,3}(,\d{3})+|\d+)(\.\d+)?)$/.exec(s)))return{v:(m[1]=='-'?-1:1)*+m[2].replace(/,/g,''),nf:'$#,##0'+(m[5]?'.00':'')};
 if((m=/^([+-]?(\d{1,3}(,\d{3})+|\d+)?(\.\d+)?)%$/.exec(s))&&m[1]&&/\d/.test(m[1]))return{v:+m[1].replace(/,/g,'')/100,nf:m[4]?'0.'+'0'.repeat(Math.min(4,m[4].length-1))+'%':'0%'};
 if(/^[+-]?(\d{1,3}(,\d{3})+|\d+)?(\.\d+)?([eE][+-]?\d+)?$/.test(s)&&/\d/.test(s)){const v=+s.replace(/,/g,'');return /,/.test(s)?{v,nf:/\./.test(s)?'#,##0.00':'#,##0'}:{v}}
 if((m=/^(\d{4})-(\d{1,2})-(\d{1,2})$/.exec(s))&&+m[2]>=1&&+m[2]<=12&&+m[3]>=1&&+m[3]<=31)return{v:SPE.toSerial(+m[1],+m[2],+m[3]),nf:'yyyy-mm-dd'};
 if((m=/^(\d{1,2})\/(\d{1,2})\/(\d{2}|\d{4})$/.exec(s))&&+m[1]>=1&&+m[1]<=12&&+m[2]>=1&&+m[2]<=31){let y=+m[3];if(y<100)y+=y<30?2000:1900;return{v:SPE.toSerial(y,+m[1],+m[2]),nf:'m/d/yyyy'}}
 if((m=/^(\d{1,2}):(\d{2})(?::(\d{2}))?\s*(am|pm)?$/i.exec(s))&&+m[2]<60){let h=+m[1];if(m[4]){if(h>12)return{v:t};h=h%12+(/pm/i.test(m[4])?12:0)}return{v:(h*3600+ +m[2]*60+ +(m[3]||0))/86400,nf:(m[4]?'h:mm':'h:mm')+(m[3]?':ss':'')+(m[4]?' AM/PM':'')}}
 const u=s.toUpperCase();if(u=='TRUE'||u=='FALSE')return{v:u=='TRUE'};
 return{v:t}}
/* the text shown in the formula bar and the cell editor for a cell */
function spInput(cell){if(!cell)return'';if(cell.f!=null)return cell.f;const v=cell.v,nf=cell.st&&cell.st.nf||'';if(v==null)return'';if(typeof v=='boolean')return v?'TRUE':'FALSE';
 if(typeof v=='number'){if(/[yd]/i.test(nf.replace(/"[^"]*"/g,''))){const t=v-Math.floor(v);return SPE.fmtValue(v,'yyyy-mm-dd')+(t>1e-9?' '+SPE.fmtValue(v,'h:mm:ss'):'')}
  if(/h|s/i.test(nf.replace(/"[^"]*"/g,''))&&v<1)return SPE.fmtValue(v,'h:mm:ss');if(/%/.test(nf))return+(v*100).toPrecision(15)+'%';return String(+v.toPrecision(15))}
 return(typeof v=='string'&&(spParse(v)||{}).v!==v)?"'"+v:String(v)}
/* ---- .RSHEET ---- */
function spToJSON(b){const st=[],sti=new Map();const si=o=>{if(!o)return undefined;const j=JSON.stringify(o);if(!sti.has(j)){sti.set(j,st.length);st.push(o)}return sti.get(j)};
 return JSON.stringify({v:2,app:'SpreadsheetPro',active:b.active||0,names:b.names||{},styles:st,sheets:b.sheets.map(s=>({name:s.name,color:s.color||'',
  cells:[...s.cells].sort((a,c)=>a[0]-c[0]).map(([k,c])=>{const a=[k,c.f!=null?null:c.v??null];const x=si(c.st);if(c.f!=null||x!=null)a.push(c.f??null);if(x!=null)a.push(x);return a}),
  cw:s.cw,rh:s.rh,hid:s.hid,hc:s.hc,freeze:s.freeze,merges:s.merges,cf:s.cf,dv:s.dv,cm:s.cm,charts:s.charts,filter:s.filter?{r0:s.filter.r0,c0:s.filter.c0,r1:s.filter.r1,c1:s.filter.c1,cr:s.filter.cr||{}}:null,grid:s.grid!==false,print:s.print||{}}))})}
function spFromJSON(text){const o=JSON.parse(text);if(!o||typeof o!='object')throw new Error('not a spreadsheet');
 const b=spBookNew();
 if(o.v==2&&Array.isArray(o.sheets)){b.sheets=[];const st=Array.isArray(o.styles)?o.styles:[];
  o.sheets.forEach((x,i)=>{const s=spSheet(String(x.name||'Sheet'+(i+1)).slice(0,31),i+1);s.color=typeof x.color=='string'&&/^#[0-9a-f]{3,8}$/i.test(x.color)?x.color:'';
   for(const a of x.cells||[]){if(!Array.isArray(a))continue;const k=+a[0];if(!(k>=0&&k<SPM*SPR))continue;const c={};if(a[2]!=null)c.f=String(a[2]);else{const v=a[1];c.v=typeof v=='number'||typeof v=='boolean'||typeof v=='string'?v:null}if(a[3]!=null&&st[a[3]])c.st=st[a[3]];s.cells.set(k,c)}
   const obj=v=>v&&typeof v=='object'&&!Array.isArray(v)?v:{};const arr=v=>Array.isArray(v)?v:[];
   s.cw=obj(x.cw);s.rh=obj(x.rh);s.hid=obj(x.hid);s.hc=obj(x.hc);s.freeze=x.freeze&&typeof x.freeze=='object'?{r:+x.freeze.r||0,c:+x.freeze.c||0}:{r:0,c:0};
   s.merges=arr(x.merges).filter(m=>m&&m.r1>=m.r0&&m.c1>=m.c0);s.cf=arr(x.cf);s.dv=arr(x.dv);s.cm=obj(x.cm);s.charts=arr(x.charts);s.filter=x.filter&&typeof x.filter=='object'?x.filter:null;s.grid=x.grid!==false;s.print=obj(x.print);
   b.sheets.push(s)});if(!b.sheets.length)b.sheets.push(spSheet('Sheet1',1));b.nid=b.sheets.length+1;b.names=o.names&&typeof o.names=='object'?o.names:{};b.active=Math.min(+o.active||0,b.sheets.length-1);return b}
 /* version 1, the old Spreadsheet: {v:1,cells:{A1:{v,b}}} */
 const s=b.sheets[0];for(const[a,c]of Object.entries(o.cells||{})){const p=SPE.parseA1(a);if(!p||!c)continue;const x=spParse(c.v==null?'':String(c.v));if(!x&&!c.b)continue;const cell=x?(x.f?{f:x.f}:{v:x.v}):{v:null};if(x&&x.nf||c.b)cell.st=Object.assign({},x&&x.nf?{nf:x.nf}:{},c.b?{b:1}:{});s.cells.set(spK(p.r,p.c),cell)}
 return b}
/* ---- CSV ---- */
function spToCSV(s,delim){delim=delim||',';const{r1,c1}=spUsed(s);let o='';for(let r=0;r<=r1;r++){const row=[];for(let c=0;c<=c1;c++){const t=spText(s.cells.get(spK(r,c)));row.push(/[",\n\r]/.test(t)||t.includes(delim)?'"'+t.replace(/"/g,'""')+'"':t)}o+=row.join(delim)+'\r\n'}return o}
function spCSVRows(text,delim){if(!delim){const l=text.split('\n')[0]||'';const n=d=>l.split(d).length;delim=n('\t')>n(',')&&n('\t')>n(';')?'\t':n(';')>n(',')?';':','}
 const rows=[];let row=[],f='',qd=false,i=0;text=text.replace(/^﻿/,'');for(;i<text.length;i++){const c=text[i];if(qd){if(c=='"'&&text[i+1]=='"'){f+='"';i++}else if(c=='"')qd=false;else f+=c}else if(c=='"')qd=true;else if(c==delim){row.push(f);f=''}else if(c=='\n'){row.push(f);rows.push(row);row=[];f=''}else if(c!='\r')f+=c}
 if(f!==''||row.length){row.push(f);rows.push(row)}return rows}
function spFromCSV(text,name,delim){const b=spBookNew();const s=b.sheets[0];if(name)s.name=String(name).replace(/\.[^.]*$/,'').slice(0,31)||'Sheet1';
 spCSVRows(text,delim).slice(0,SPR).forEach((r,ri)=>r.slice(0,SPM).forEach((v,ci)=>{const x=spParse(v);if(!x)return;const c=x.f?{f:x.f}:{v:x.v};if(x.nf)c.st={nf:x.nf};s.cells.set(spK(ri,ci),c)}));return b}
/* ---- Excel .xlsx ---- */
const SP_BUILTIN_NF={0:'General',1:'0',2:'0.00',3:'#,##0',4:'#,##0.00',5:'$#,##0;($#,##0)',6:'$#,##0;[Red]($#,##0)',7:'$#,##0.00;($#,##0.00)',8:'$#,##0.00;[Red]($#,##0.00)',9:'0%',10:'0.00%',11:'0.00E+00',12:'# ?/?',13:'# ??/??',14:'m/d/yyyy',15:'d-mmm-yy',16:'d-mmm',17:'mmm-yy',18:'h:mm AM/PM',19:'h:mm:ss AM/PM',20:'h:mm',21:'h:mm:ss',22:'m/d/yyyy h:mm',37:'#,##0;(#,##0)',38:'#,##0;[Red](#,##0)',39:'#,##0.00;(#,##0.00)',40:'#,##0.00;[Red](#,##0.00)',44:'_($* #,##0.00_);_($* (#,##0.00);_($* "-"??_);_(@_)',45:'mm:ss',46:'[h]:mm:ss',47:'mm:ss.0',48:'##0.0E+0',49:'@'};
const SP_THEME=['#ffffff','#000000','#e7e6e6','#44546a','#4472c4','#ed7d31','#a5a5a5','#ffc000','#5b9bd5','#70ad47'];
function spXColor(n){if(!n)return'';const rgb=n.getAttribute('rgb');let c='';if(rgb&&/^[0-9a-f]{6,8}$/i.test(rgb))c='#'+rgb.slice(-6).toLowerCase();else if(n.getAttribute('theme')!=null)c=SP_THEME[+n.getAttribute('theme')]||'';else if(n.getAttribute('indexed')!=null){const ix=+n.getAttribute('indexed');c=['#000000','#ffffff','#ff0000','#00ff00','#0000ff','#ffff00','#ff00ff','#00ffff'][ix%8]||'';if(ix==64||ix==65)c=''}
 const t=+(n.getAttribute('tint')||0);if(c&&t){const h=c.slice(1).match(/../g).map(x=>parseInt(x,16)).map(v=>Math.round(t<0?v*(1+t):v+(255-v)*t));c='#'+h.map(v=>v.toString(16).padStart(2,'0')).join('')}return c}
async function spFromXlsx(bytes){const z=await JSZip.loadAsync(bytes);const rd=async p=>{const f=z.file(p);return f?offXml(await f.async('string')):null};
 const wb=await rd('xl/workbook.xml');if(!wb)throw new Error('This is not an Excel workbook.');const rels=await rd('xl/_rels/workbook.xml.rels');const target={};offEls(rels,'Relationship').forEach(r=>{let t=r.getAttribute('Target')||'';t=t.startsWith('/')?t.slice(1):'xl/'+t.replace(/^\.\//,'');target[r.getAttribute('Id')]=t});
 const ss=[];const sx=await rd('xl/sharedStrings.xml');offEls(sx,'si').forEach(si=>ss.push(offKids(si,'t').length?offKids(si,'t').map(t=>t.textContent).join(''):offEls(si,'t').filter(t=>!t.closest||!offEls(si,'rPh').some(p=>p.contains(t))).map(t=>t.textContent).join('')));
 /* styles */
 const sty=await rd('xl/styles.xml');const nfs=Object.assign({},SP_BUILTIN_NF);offEls(sty,'numFmt').forEach(n=>{nfs[n.getAttribute('numFmtId')]=n.getAttribute('formatCode')});
 const fonts=offEls(offEl(sty,'fonts'),'font').map(f=>{const o={};const has=n=>{const e=offKids(f,n)[0];return e&&e.getAttribute('val')!=='0'&&e.getAttribute('val')!=='false'};if(has('b'))o.b=1;if(has('i'))o.i=1;if(offKids(f,'u')[0]&&offKids(f,'u')[0].getAttribute('val')!='none')o.u=1;if(has('strike'))o.s=1;
  const sz=offKids(f,'sz')[0];if(sz){const v=+sz.getAttribute('val');if(v&&v!=11)o.sz=v}const nm=offKids(f,'name')[0];if(nm&&nm.getAttribute('val')!='Calibri')o.font=nm.getAttribute('val');const cl=spXColor(offKids(f,'color')[0]);if(cl&&cl!='#000000')o.fc=cl;return o});
 const fills=offEls(offEl(sty,'fills'),'fill').map(f=>{const p=offEl(f,'patternFill');if(!p||!p.getAttribute('patternType')||p.getAttribute('patternType')=='none')return'';return spXColor(offEl(p,'fgColor'))||''});
 const borders=offEls(offEl(sty,'borders'),'border').map(b=>{const o={};[['left','l'],['right','r'],['top','t'],['bottom','b']].forEach(([n,k])=>{const e=offKids(b,n)[0];const st=e&&e.getAttribute('style');if(st&&st!='none')o[k]=/thick/.test(st)?'thick':/medium/.test(st)?'medium':st=='double'?'double':'thin'});return Object.keys(o).length?o:null});
 const xfs=offEls(offEl(sty,'cellXfs'),'xf').map(x=>{const o={};const nf=nfs[x.getAttribute('numFmtId')||'0'];if(nf&&nf!='General')o.nf=nf;Object.assign(o,fonts[+x.getAttribute('fontId')||0]||{});const fl=fills[+x.getAttribute('fillId')||0];if(fl)o.bg=fl;const bd=borders[+x.getAttribute('borderId')||0];if(bd)o.bd=bd;
  const al=offEl(x,'alignment');if(al){const h=al.getAttribute('horizontal'),v=al.getAttribute('vertical');if(h&&/left|center|right/.test(h))o.ha=h.replace('centerContinuous','center');if(v&&v!='bottom')o.va=v=='center'?'middle':v;if(al.getAttribute('wrapText')=='1'||al.getAttribute('wrapText')=='true')o.wrap=1}return Object.keys(o).length?o:null});
 const b=spBookNew();b.sheets=[];let id=1;
 for(const sh of offEls(offEl(wb,'sheets'),'sheet')){const rid=sh.getAttributeNS('http://schemas.openxmlformats.org/officeDocument/2006/relationships','id')||sh.getAttribute('r:id');const p=target[rid];const x=p&&await rd(p);const s=spSheet(String(sh.getAttribute('name')||'Sheet'+id).slice(0,31),id++);b.sheets.push(s);if(!x)continue;
  if(sh.getAttribute('state')=='hidden')s.hidden=1;
  const tc=offEl(x,'tabColor');if(tc)s.color=spXColor(tc);
  const pane=offEl(x,'pane');if(pane&&/frozen/.test(pane.getAttribute('state')||'')){s.freeze={r:+pane.getAttribute('ySplit')||0,c:+pane.getAttribute('xSplit')||0}}
  const sv=offEl(x,'sheetView');if(sv&&sv.getAttribute('showGridLines')=='0')s.grid=false;
  offEls(offEl(x,'cols'),'col').forEach(c=>{const a=+c.getAttribute('min')-1,z=Math.min(+c.getAttribute('max')-1,SPM-1),w=+c.getAttribute('width');for(let i=a;i<=z&&i-a<SPM;i++){if(w&&c.getAttribute('customWidth')!='0')s.cw[i]=Math.round(w*7+5);if(c.getAttribute('hidden')=='1')s.hc[i]=1}});
  const shared={};
  for(const row of offEls(offEl(x,'sheetData'),'row')){const r=+row.getAttribute('r')-1;if(r>=SPR)break;const ht=row.getAttribute('ht');if(ht&&row.getAttribute('customHeight')=='1')s.rh[r]=Math.round(+ht*4/3);if(row.getAttribute('hidden')=='1')s.hid[r]=1;
   let ci=0;for(const c of offKids(row,'c')){const ref=c.getAttribute('r');const p=ref?SPE.parseA1(ref):{r,c:ci};ci=(p?p.c:ci)+1;if(!p||p.c>=SPM)continue;
    const t=c.getAttribute('t')||'n',vEl=offKids(c,'v')[0],fEl=offKids(c,'f')[0],st=xfs[+c.getAttribute('s')||0];let cell=null;
    if(fEl){let f=fEl.textContent;const si=fEl.getAttribute('si');if(fEl.getAttribute('t')=='shared'){if(f)shared[si]={f,r:p.r,c:p.c};else if(shared[si])f=SPE.copyFormula('='+shared[si].f,p.r-shared[si].r,p.c-shared[si].c).slice(1)}if(f)cell={f:'='+f.replace(/_xlfn\.|_xlws\./g,'')}}
    if(!cell){let v=null;const tx=vEl?vEl.textContent:'';if(t=='s')v=ss[+tx]??'';else if(t=='inlineStr')v=offEls(c,'t').map(e=>e.textContent).join('');else if(t=='str')v=tx;else if(t=='b')v=tx=='1';else if(t=='e')v=tx;else if(vEl)v=+tx;if(v==null&&!st)continue;cell={v}}
    if(st)cell.st=st;s.cells.set(spK(p.r,p.c),cell)}}
  offEls(offEl(x,'mergeCells'),'mergeCell').forEach(m=>{const g=spParseRange(m.getAttribute('ref'));if(g)s.merges.push(g)});
  offEls(offEl(x,'dataValidations'),'dataValidation').forEach(d=>{const g=spParseRange((d.getAttribute('sqref')||'').split(' ')[0]);const f1=offKids(d,'formula1')[0];if(g&&d.getAttribute('type')=='list'&&f1){const l=f1.textContent;s.dv.push({...g,t:'list',list:/^".*"$/.test(l)?l.slice(1,-1):'='+l})}});
 }
 offEls(offEl(wb,'definedNames'),'definedName').forEach(d=>{const n=String(d.getAttribute('name')||'').toUpperCase();if(!n||n.startsWith('_XLNM'))return;const m=/^(?:'?(.+?)'?!)?(.+)$/.exec(d.textContent);if(m)b.names[n]={ref:m[2],sheet:m[1]||(b.sheets[0]&&b.sheets[0].name)}});
 if(!b.sheets.length)b.sheets.push(spSheet('Sheet1',1));b.nid=id;return b}
async function spToXlsx(b){const z=new JSZip();const X=offX;
 /* styles: each different style object once */
 const nfIds={},nfList=[],fonts=['<font><sz val="11"/><name val="Calibri"/></font>'],fills=['<fill><patternFill patternType="none"/></fill>','<fill><patternFill patternType="gray125"/></fill>'],borders=['<border><left/><right/><top/><bottom/><diagonal/></border>'],xfs=['<xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>'];
 const idx=(list,s)=>{let i=list.indexOf(s);if(i<0){i=list.length;list.push(s)}return i};const xfMap=new Map();const rgb=c=>'FF'+String(c||'#000000').replace('#','').toUpperCase().padEnd(6,'0').slice(0,6);
 const builtin=Object.fromEntries(Object.entries(SP_BUILTIN_NF).map(([k,v])=>[v,+k]));
 const xfOf=st=>{if(!st)return 0;const key=JSON.stringify(st);if(xfMap.has(key))return xfMap.get(key);let nf=0;if(st.nf&&st.nf!='General'){nf=builtin[st.nf]??nfIds[st.nf];if(nf==null){nf=164+nfList.length;nfIds[st.nf]=nf;nfList.push(st.nf)}}
  const fo=idx(fonts,`<font>${st.b?'<b/>':''}${st.i?'<i/>':''}${st.s?'<strike/>':''}${st.u?'<u/>':''}<sz val="${+st.sz||11}"/>${st.fc?`<color rgb="${rgb(st.fc)}"/>`:''}<name val="${X(st.font||'Calibri')}"/></font>`);
  const fi=st.bg?idx(fills,`<fill><patternFill patternType="solid"><fgColor rgb="${rgb(st.bg)}"/><bgColor indexed="64"/></patternFill></fill>`):0;
  const bd=st.bd?idx(borders,`<border>${['l','r','t','b'].map((k,i)=>{const n=['left','right','top','bottom'][i];return st.bd[k]?`<${n} style="${st.bd[k]}"><color rgb="FF000000"/></${n}>`:`<${n}/>`}).join('')}<diagonal/></border>`):0;
  const al=st.ha||st.va||st.wrap?`<alignment${st.ha?` horizontal="${st.ha}"`:''}${st.va?` vertical="${st.va=='middle'?'center':st.va}"`:''}${st.wrap?' wrapText="1"':''}/>`:'';
  const i=xfs.length;xfs.push(`<xf numFmtId="${nf}" fontId="${fo}" fillId="${fi}" borderId="${bd}" xfId="0"${nf?' applyNumberFormat="1"':''}${fo?' applyFont="1"':''}${fi?' applyFill="1"':''}${bd?' applyBorder="1"':''}${al?' applyAlignment="1">'+al+'</xf>':'/>'}`);xfMap.set(key,i);return i};
 const sheetsXml=b.sheets.map((s,si)=>{const{r1,c1}=spUsed(s);const rows=new Map();for(const[k,c]of s.cells){const r=Math.floor(k/SPM);if(!rows.has(r))rows.set(r,[]);rows.get(r).push([k%SPM,c])}
  let o=`<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">`;
  if(s.color)o+=`<sheetPr><tabColor rgb="${rgb(s.color)}"/></sheetPr>`;o+=`<dimension ref="A1${r1>=0?':'+SPE.a1(Math.max(0,r1),Math.max(0,c1)):''}"/>`;
  o+=`<sheetViews><sheetView workbookViewId="0"${si==(b.active||0)?' tabSelected="1"':''}${s.grid===false?' showGridLines="0"':''}>`;if(s.freeze&&(s.freeze.r||s.freeze.c))o+=`<pane${s.freeze.c?` xSplit="${s.freeze.c}"`:''}${s.freeze.r?` ySplit="${s.freeze.r}"`:''} topLeftCell="${SPE.a1(s.freeze.r,s.freeze.c)}" activePane="${s.freeze.r&&s.freeze.c?'bottomRight':s.freeze.r?'bottomLeft':'topRight'}" state="frozen"/>`;o+=`</sheetView></sheetViews><sheetFormatPr defaultRowHeight="15"/>`;
  const cws=Object.keys(s.cw).map(Number).concat(Object.keys(s.hc).map(Number)).filter((v,i,a)=>a.indexOf(v)==i).sort((a,c)=>a-c);if(cws.length)o+='<cols>'+cws.map(c=>`<col min="${c+1}" max="${c+1}" width="${(((s.cw[c]??80)-5)/7).toFixed(2)}" customWidth="1"${s.hc[c]?' hidden="1"':''}/>`).join('')+'</cols>';
  o+='<sheetData>';const rk=[...new Set([...rows.keys(),...Object.keys(s.rh).map(Number),...Object.keys(s.hid).map(Number)])].filter(r=>r>=0&&r<SPR).sort((a,c)=>a-c);
  for(const r of rk){const cs=(rows.get(r)||[]).sort((a,c)=>a[0]-c[0]);o+=`<row r="${r+1}"${s.rh[r]!=null?` ht="${(s.rh[r]*.75).toFixed(2)}" customHeight="1"`:''}${s.hid[r]?' hidden="1"':''}>`;
   for(const[c,cell]of cs){const a=SPE.a1(r,c),xs=xfOf(cell.st),sa=xs?` s="${xs}"`:'';const v=spVal(cell);
    if(cell.f!=null){const res=v==null?'':SPE.isE(v)?`<v>${X(v.e)}</v>`:`<v>${X(typeof v=='boolean'?(v?1:0):v)}</v>`;const t=SPE.isE(v)?' t="e"':typeof v=='string'?' t="str"':typeof v=='boolean'?' t="b"':'';o+=`<c r="${a}"${sa}${t}><f>${X(cell.f.slice(1))}</f>${res}</c>`}
    else if(v==null)o+=`<c r="${a}"${sa}/>`;else if(typeof v=='number')o+=`<c r="${a}"${sa}><v>${v}</v></c>`;else if(typeof v=='boolean')o+=`<c r="${a}"${sa} t="b"><v>${v?1:0}</v></c>`;else o+=`<c r="${a}"${sa} t="inlineStr"><is><t xml:space="preserve">${X(v)}</t></is></c>`}
   o+='</row>'}
  o+='</sheetData>';if(s.merges.length)o+=`<mergeCells count="${s.merges.length}">`+s.merges.map(m=>`<mergeCell ref="${spRangeStr(m)}"/>`).join('')+'</mergeCells>';
  const lists=s.dv.filter(d=>d.t=='list');if(lists.length)o+=`<dataValidations count="${lists.length}">`+lists.map(d=>`<dataValidation type="list" allowBlank="1" showErrorMessage="1" sqref="${spRangeStr(d)}"><formula1>${X(String(d.list).startsWith('=')?String(d.list).slice(1):'"'+d.list+'"')}</formula1></dataValidation>`).join('')+'</dataValidations>';
  return o+'<pageMargins left="0.7" right="0.7" top="0.75" bottom="0.75" header="0.3" footer="0.3"/>'+(s.print&&s.print.land?'<pageSetup orientation="landscape"/>':'')+'</worksheet>'});
 const nfXml=nfList.length?`<numFmts count="${nfList.length}">${nfList.map((f,i)=>`<numFmt numFmtId="${164+i}" formatCode="${X(f)}"/>`).join('')}</numFmts>`:'';
 z.file('xl/styles.xml',`<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">${nfXml}<fonts count="${fonts.length}">${fonts.join('')}</fonts><fills count="${fills.length}">${fills.join('')}</fills><borders count="${borders.length}">${borders.join('')}</borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs><cellXfs count="${xfs.length}">${xfs.join('')}</cellXfs><cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles></styleSheet>`);
 sheetsXml.forEach((x,i)=>z.file(`xl/worksheets/sheet${i+1}.xml`,x));
 const dn=Object.entries(b.names||{}).map(([n,v])=>`<definedName name="${X(n)}">${X((v.sheet?`'${String(v.sheet).replace(/'/g,"''")}'!`:'')+v.ref)}</definedName>`).join('');
 z.file('xl/workbook.xml',`<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><bookViews><workbookView activeTab="${b.active||0}"/></bookViews><sheets>${b.sheets.map((s,i)=>`<sheet name="${X(s.name)}" sheetId="${i+1}" r:id="rId${i+1}"/>`).join('')}</sheets>${dn?'<definedNames>'+dn+'</definedNames>':''}<calcPr calcId="191029" fullCalcOnLoad="1"/></workbook>`);
 z.file('xl/_rels/workbook.xml.rels',`<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">${b.sheets.map((s,i)=>`<Relationship Id="rId${i+1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet${i+1}.xml"/>`).join('')}<Relationship Id="rId${b.sheets.length+1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>`);
 z.file('_rels/.rels',`<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="docProps/core.xml"/><Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties" Target="docProps/app.xml"/></Relationships>`);
 z.file('docProps/app.xml',`<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties"><Application>SpreadsheetPro (Rudolf OS)</Application></Properties>`);
 z.file('docProps/core.xml',`<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dcterms="http://purl.org/dc/terms/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"><dc:creator>SpreadsheetPro</dc:creator><dcterms:created xsi:type="dcterms:W3CDTF">${new Date().toISOString().replace(/\.\d+Z/,'Z')}</dcterms:created></cp:coreProperties>`);
 z.file('[Content_Types].xml',`<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>${b.sheets.map((s,i)=>`<Override PartName="/xl/worksheets/sheet${i+1}.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>`).join('')}<Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/><Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/><Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/></Types>`);
 return z.generateAsync({type:'blob',compression:'DEFLATE',mimeType:'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet'})}
/* ---- templates ---- */
function spTemplate(name){const b=spBookNew(),s=b.sheets[0];const H={b:1,fc:'#ffffff',bg:'#1f4e79'},T={b:1,sz:18,fc:'#1f4e79'},TOT={b:1,bd:{t:'thin',b:'double'}},M='$#,##0.00';
 const put=(a,v,st)=>{const p=SPE.parseA1(a);const x=typeof v=='string'?spParse(v):{v};if(!x)return;const c=x.f?{f:x.f}:{v:x.v};const so=Object.assign({},x.nf?{nf:x.nf}:{},st||{});if(Object.keys(so).length)c.st=so;s.cells.set(spK(p.r,p.c),c)};
 const row=(r,vals,st)=>vals.forEach((v,i)=>{if(v!==''&&v!=null)put(SPE.colName(i)+r,v,st)});
 if(name=='budget'){s.name='Budget';put('A1','Monthly budget',T);row(3,['Category','Planned','Actual','Difference'],H);
  ['Rent','Groceries','Utilities','Transport','Phone and internet','Insurance','Eating out','Savings','Other'].forEach((c,i)=>{const r=i+4;put('A'+r,c);put('B'+r,[1200,450,180,160,90,120,150,300,100][i],{nf:M});put('C'+r,[1200,482,171,140,90,120,205,300,64][i],{nf:M});put('D'+r,'=B'+r+'-C'+r,{nf:M})});
  put('A13','Total',TOT);['B','C','D'].forEach(c=>put(c+'13',`=SUM(${c}4:${c}12)`,{...TOT,nf:M}));put('A15','Income');put('B15',3200,{nf:M});put('A16','Left over',{b:1});put('B16','=B15-C13',{b:1,nf:M});
  s.cw={0:150,1:100,2:100,3:100};s.cf.push({r0:3,c0:3,r1:11,c1:3,t:'lt',a:0,st:{fc:'#9c0006',bg:'#ffc7ce'}});s.charts.push({id:1,type:'column',rg:'A3:C12',x:480,y:66,w:440,h:280,title:'Planned and actual',legend:true});s.freeze={r:3,c:0}}
 else if(name=='invoice'){s.name='Invoice';put('A1','INVOICE',{b:1,sz:24,fc:'#1f4e79'});put('A3','Your company');put('A4','Street, town');put('D3','Invoice no.',{b:1});put('E3','1001');put('D4','Date',{b:1});put('E4','=TODAY()',{nf:'yyyy-mm-dd'});put('A6','Bill to',{b:1});put('A7','Customer name');
  row(9,['Description','','Quantity','Unit price','Amount'],H);for(let r=10;r<=17;r++){put('E'+r,`=IF(C${r}="","",C${r}*D${r})`,{nf:M});s.cells.set(spK(r-1,3),{v:null,st:{nf:M}})}put('A10','Design work');put('C10',10);put('D10',45,{nf:M});put('A11','Printing');put('C11',250);put('D11',0.4,{nf:M});
  put('D19','Subtotal',{b:1});put('E19','=SUM(E10:E17)',{nf:M});put('D20','Tax rate',{b:1});put('E20','8%');put('D21','Tax',{b:1});put('E21','=E19*E20',{nf:M});put('D22','Total',TOT);put('E22','=E19+E21',{...TOT,nf:M});
  s.merges.push({r0:8,c0:0,r1:8,c1:1});s.cw={0:180,1:60,2:80,3:100,4:110}}
 else if(name=='timetable'){s.name='Timetable';put('A1','Weekly timetable',T);row(3,['Time','Monday','Tuesday','Wednesday','Thursday','Friday'],H);
  for(let i=0;i<9;i++){put('A'+(4+i),SPE.fmtValue((8+i)/24,'h:mm'),{b:1});s.cells.get(spK(3+i,0)).st={b:1,nf:'h:mm'}}row(4,['','Maths','English','Maths','Science','Art']);row(5,['','Science','History','Music','Maths','English']);row(7,['','Lunch','Lunch','Lunch','Lunch','Lunch'],{fc:'#7f7f7f',i:1});
  s.cw={0:70,1:110,2:110,3:110,4:110,5:110};s.freeze={r:3,c:1}}
 else if(name=='grades'){s.name='Grade book';put('A1','Grade book',T);row(3,['Student','Test 1','Test 2','Project','Final','Average','Grade'],H);
  const st=[['Avery',88,92,95,90],['Blake',72,65,80,70],['Casey',95,98,92,97],['Devon',58,62,70,64],['Emery',81,77,85,80],['Finley',90,85,88,93]];
  st.forEach((x,i)=>{const r=4+i;row(r,x);put('F'+r,`=ROUND(AVERAGE(B${r}:E${r}),1)`);put('G'+r,`=IFS(F${r}>=90,"A",F${r}>=80,"B",F${r}>=70,"C",F${r}>=60,"D",TRUE,"F")`,{ha:'center'})});
  put('A11','Class average',{b:1});['B','C','D','E','F'].forEach(c=>put(c+'11',`=ROUND(AVERAGE(${c}4:${c}9),1)`,{b:1}));s.cf.push({r0:3,c0:5,r1:8,c1:5,t:'scale',cs:['#f8696b','#ffeb84','#63be7b']});s.cf.push({r0:3,c0:6,r1:8,c1:6,t:'eq',a:'F',st:{fc:'#9c0006',bg:'#ffc7ce'}});
  s.cw={0:120};s.filter={r0:2,c0:0,r1:8,c1:6,cr:{}};s.freeze={r:3,c:1}}
 else if(name=='loan'){s.name='Loan';put('A1','Loan calculator',T);put('A3','Loan amount');put('B3',250000,{nf:M});put('A4','Interest rate (yearly)');put('B4','6.5%');put('A5','Years');put('B5',30);put('A6','Monthly payment',{b:1});put('B6','=-PMT(B4/12,B5*12,B3)',{b:1,nf:M});put('A7','Total paid');put('B7','=B6*B5*12',{nf:M});put('A8','Total interest');put('B8','=B7-B3',{nf:M});
  row(10,['Month','Payment','Interest','Principal','Balance'],H);put('A11',1);put('B11','=$B$6',{nf:M});put('C11','=B3*$B$4/12',{nf:M});put('D11','=B11-C11',{nf:M});put('E11','=B3-D11',{nf:M});
  for(let r=12;r<=370;r++){put('A'+r,'=A'+(r-1)+'+1');put('B'+r,'=$B$6',{nf:M});put('C'+r,`=E${r-1}*$B$4/12`,{nf:M});put('D'+r,`=B${r}-C${r}`,{nf:M});put('E'+r,`=MAX(0,E${r-1}-D${r})`,{nf:M})}
  s.cw={0:160,1:110,2:100,3:100,4:120};s.freeze={r:10,c:0}}
 return b}

/* ===== 1.4.5 (spro45): SpreadsheetPro, in the place of Excel. It replaces the small Spreadsheet of 1.2 (its id `sheet`
   stays, so shortcuts, Open with and settings that name it keep working). The grid is drawn on a canvas, only where it
   is visible; the formula engine is spreng45.js, the files are sprof45.js, and the window's look is WordPro's
   (office45.js). ===== */
/* (its regApp line is in the page) */
const SP_FAQ=[
 ['formula sum add total calculate','Adding up numbers','Type = and a formula, such as <b>=SUM(B2:B10)</b>, or select the numbers and press Σ on the toolbar. Click cells while typing a formula to put in their addresses.'],
 ['chart graph plot','Making a chart','Select the data with its headings, then choose Insert › Chart…. Drag the chart to move it, its corner to resize it, and double-click it to change it.'],
 ['sort order alphabetical','Sorting','Click in the column and choose Data › Sort A to Z (or Z to A). Data › Sort… sorts by several columns.'],
 ['filter hide show only','Filtering','Choose Data › AutoFilter: each heading gets a ▾ button that shows only the rows you pick.'],
 ['format number currency percent decimal date money','Number formats','Use the format box on the toolbar, or Format › Cells… for decimals, currency symbols, dates and custom codes such as #,##0.00.'],
 ['colour color fill background highlight conditional','Colours and conditional formatting','The A and paint-bucket buttons colour text and cells. Format › Conditional formatting… colours cells by their values, with colour scales and data bars too.'],
 ['freeze lock header heading row','Freezing headings','View › Freeze panes keeps the rows above and the columns left of the selected cell in view while you scroll.'],
 ['excel xlsx open save export import','Excel files','File › Open… opens .xlsx files, and Save as… with a name ending .XLSX saves one. CSV files open and save too.'],
 ['print pdf page','Printing','File › Page setup… chooses the orientation, fitting to the page width and the rows repeated on each page. Then File › Print… or Save as PDF….'],
 ['fill series drag handle copy down','Filling a series','Drag the small square at the corner of the selection: numbers, dates, weekdays and months continue; formulas are copied with their references moved.'],
 ['vlookup lookup find match xlookup','Looking things up','=XLOOKUP(value, where, what) or =VLOOKUP(value, table, column, FALSE) finds a value in a table.'],
 ['insert delete row column','Rows and columns','Right-click a row number or column letter to insert or delete. Formulas that point to the cells are adjusted.'],
 ['undo mistake back','Undoing','Ctrl+Z undoes, Ctrl+Y redoes.'],
 ['merge centre center combine','Merging cells','Select the cells and press the ⇔ button (Merge and centre).'],
 ['sheet tab rename add','Sheets','The + at the bottom adds a sheet. Double-click a tab to rename it, right-click it for more.'],
 ['template budget invoice timetable grade loan','Templates','File › New from template… has a budget, an invoice, a timetable, a grade book and a loan calculator.']];
const SP_NF=[['General',''],['Number','#,##0.00'],['Currency','$#,##0.00'],['Accounting','_($* #,##0.00_);_($* (#,##0.00);_($* "-"??_);_(@_)'],['Percent','0.00%'],['Scientific','0.00E+00'],['Date','yyyy-mm-dd'],['Long date','dddd, mmmm d, yyyy'],['Time','h:mm:ss AM/PM'],['Text','@']];
const SP_STYLES={'Normal':null,'Title':{b:1,sz:18,fc:'#1f4e79'},'Heading 1':{b:1,sz:15,fc:'#1f4e79',bd:{b:'thick'}},'Heading 2':{b:1,sz:13,fc:'#1f4e79',bd:{b:'medium'}},'Total':{b:1,bd:{t:'thin',b:'double'}},'Good':{fc:'#006100',bg:'#c6efce'},'Bad':{fc:'#9c0006',bg:'#ffc7ce'},'Neutral':{fc:'#9c5700',bg:'#ffeb9c'},'Note':{bg:'#ffffcc',bd:{t:'thin',r:'thin',b:'thin',l:'thin'}},'Currency':{nf:'$#,##0.00'},'Percent':{nf:'0%'},'Header row':{b:1,fc:'#ffffff',bg:'#1f4e79'}};
const SP_SERIES=[['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'],['January','February','March','April','May','June','July','August','September','October','November','December']];
function sheetApp(path,seedBook){
 const W=appWin('sheet','SpreadsheetPro','📊',1000,660);
 const HW=46,HH=22,DW=80,DH=20;
 let book=seedBook||spBookNew(),sh=book.sheets[book.active||0],file=path||null,dirty=false,zoom=1,showF=false,showBar=true;
 let sel={rs:[{r0:0,c0:0,r1:0,c1:0}],r:0,c:0,ar:0,ac:0};
 let edit=null;          /* {src:'cell'|'bar',mode:'enter'|'edit',orig} while a cell is being edited */
 let tabStart=null,undoS=[],redoS=[],tx=null,clip=null,painter=null,chSel=null,vers=0;
 const X=offShell(W,{cls:'sp',refocus:()=>setTimeout(()=>{if(!edit)sc.focus({preventScroll:true})},0),menus:()=>MENUS(),
  tb:`<select class='sp-ff' title='Font'>${OFF_FONTS.map(f=>`<option>${f}</option>`).join('')}</select><select class='sp-fs' title='Font size'>${[8,9,10,11,12,14,16,18,20,24,28,36,48,72].map(n=>`<option>${n}</option>`).join('')}</select><i></i>
<button data-a='b' title='Bold (Ctrl+B)'><b>B</b></button><button data-a='i' title='Italic (Ctrl+I)'><i>I</i></button><button data-a='u' title='Underline (Ctrl+U)'><u>U</u></button><button data-a='s' title='Strikethrough'><s>S</s></button>
<button data-a='fc' title='Text colour'>A<b class='sp-sw' style='background:#c00000'></b></button><button data-a='bg' title='Fill colour'>🪣<b class='sp-sw2' style='background:#ffff00'></b></button><button data-a='bd' title='Borders'>▦</button><i></i>
<button data-a='al' title='Align left'>⬅</button><button data-a='ac' title='Centre'>↔</button><button data-a='ar' title='Align right'>➡</button><button data-a='wrap' title='Wrap text'>↵</button><button data-a='merge' title='Merge and centre'>⇔</button><i></i>
<select class='sp-nf' title='Number format'>${SP_NF.map(([n])=>`<option>${n}</option>`).join('')}<option value='*'>More…</option></select><button data-a='cur' title='Currency'>$</button><button data-a='pct' title='Percent'>%</button><button data-a='com' title='Thousands separator'>,</button><button data-a='dp' title='More decimals'>.0←</button><button data-a='dm' title='Fewer decimals'>.0→</button><i></i>
<button data-a='paint' title='Format painter'>🖌</button><button data-a='sum' title='AutoSum (Alt+=)'>Σ</button><button data-a='sa' title='Sort A to Z'>A↓</button><button data-a='sz' title='Sort Z to A'>Z↓</button><button data-a='flt' title='AutoFilter'>⏷</button><button data-a='chart' title='Insert chart'>📊</button><button data-a='undo' title='Undo (Ctrl+Z)'>↶</button><button data-a='redo' title='Redo (Ctrl+Y)'>↷</button>`,
  main:`<div class='sp-fb'><input class='sp-nb' title='Name box' aria-label='Name box'><span class='sp-fxl'>fx</span><input class='sp-fx' aria-label='Formula' spellcheck='false'></div>
<div class='sp-gw'><canvas class='sp-cv'></canvas><div class='sp-sc' tabindex='0' aria-label='Spreadsheet grid'><div class='sp-sp'></div></div><div class='sp-ol'></div><textarea class='sp-ed' spellcheck='false' style='display:none'></textarea><div class='sp-hint' style='display:none'></div><div class='sp-tip' style='display:none'></div></div>
<div class='sp-tabs'><button class='sp-add' title='New sheet'>＋</button><div class='sp-tl'></div></div>`,
  sb:`<span class='sp-st'>Ready</span><span style='flex:1'></span><span class='sp-agg'></span><span class='sp-sv'></span><input type='range' class='wp-zr sp-zr' min='50' max='200' step='10' value='100' title='Zoom'><span class='sp-zt'>100%</span>`});
 const{q,qa,root,dialog,info}=X;W.X=X;
 const gw=q('.sp-gw'),cv=q('.sp-cv'),sc=q('.sp-sc'),sp=q('.sp-sp'),ol=q('.sp-ol'),ed=q('.sp-ed'),fx=q('.sp-fx'),nb=q('.sp-nb'),hint=q('.sp-hint'),tip=q('.sp-tip');const g=cv.getContext('2d');
 let notey=null;Object.defineProperties(X,{dirty:{get:()=>dirty},path:{get:()=>file}});
 /* ================= geometry: column left edges and row top edges (zoom 1), rebuilt when sizes change ================= */
 function geom(s){if(s._cx&&s._gv===s._gk)return;const cx=new Float64Array(SPM+1),ry=new Float64Array(SPR+1);for(let c=0;c<SPM;c++)cx[c+1]=cx[c]+(s.hc[c]?0:(s.cw[c]??DW));
  /* rows without a height of their own grow to fit large text */const big={};for(const[k,cl]of s.cells){const z_=cl.st&&cl.st.sz;if(z_>12){const r=Math.floor(k/SPM);big[r]=Math.max(big[r]||0,Math.round(z_*4/3*1.3+3))}}
  const fh=s._fh||{};for(let r=0;r<SPR;r++)ry[r+1]=ry[r]+(s.hid[r]||fh[r]?0:(s.rh[r]??Math.max(DH,big[r]||0)));s._cx=cx;s._ry=ry;s._gv=s._gk}
 const regeom=(s=sh)=>{s._gk=(s._gk||0)+1};
 const bs=(a,v)=>{let lo=0,hi=a.length-2;while(lo<hi){const m=(lo+hi+1)>>1;if(a[m]<=v)lo=m;else hi=m-1}return lo};
 let VW=0,VH=0;   /* the grid's visible size (without scroll bars) */
 const FX=()=>sh._cx[sh.freeze.c]*zoom,FY=()=>sh._ry[sh.freeze.r]*zoom;
 const Xc=c=>c<sh.freeze.c?HW+sh._cx[c]*zoom:HW+FX()+(sh._cx[c]-sh._cx[sh.freeze.c])*zoom-sc.scrollLeft;
 const Yr=r=>r<sh.freeze.r?HH+sh._ry[r]*zoom:HH+FY()+(sh._ry[r]-sh._ry[sh.freeze.r])*zoom-sc.scrollTop;
 function colAtX(x){const f=sh.freeze.c;if(x<HW+FX()&&f)return Math.min(f-1,bs(sh._cx,(x-HW)/zoom));return bs(sh._cx,sh._cx[f]+(x-HW-FX()+sc.scrollLeft)/zoom)}
 function rowAtY(y){const f=sh.freeze.r;if(y<HH+FY()&&f)return Math.min(f-1,bs(sh._ry,(y-HH)/zoom));return bs(sh._ry,sh._ry[f]+(y-HH-FY()+sc.scrollTop)/zoom)}
 function sizeSpacer(){geom(sh);const f=sh.freeze;sp.style.width=Math.ceil(HW+FX()+(sh._cx[SPM]-sh._cx[f.c])*zoom)+'px';
  const used=Math.max(spUsed(sh).r1+200,sel.r+60,1000);sp.style.height=Math.ceil(HH+FY()+(sh._ry[Math.min(SPR,used)]-sh._ry[f.r])*zoom)+'px'}
 /* merged cells */
 const mergeAt=(r,c)=>sh.merges.find(m=>r>=m.r0&&r<=m.r1&&c>=m.c0&&c<=m.c1);
 /* ================= values, styles, conditional formatting ================= */
 const cellAt=(r,c,s=sh)=>s.cells.get(spK(r,c));
 const fontOf=st=>`${st&&st.i?'italic ':''}${st&&st.b?'bold ':''}${((st&&st.sz)||11)*4/3*zoom}px ${st&&st.font?`"${st.font}",`:''}Calibri,Carlito,Arial,sans-serif`;
 let cfCache=null;
 function cfInfo(){if(cfCache&&cfCache.v===vers&&cfCache.s===sh)return cfCache.m;const m=sh.cf.map(rule=>{const o={rule};const vals=[];eachIn(sh,rule,cell=>{const v=spVal(cell);if(typeof v=='number')vals.push(v)});
   if(rule.t=='top'||rule.t=='bottom'){const s=[...vals].sort((a,b)=>rule.t=='top'?b-a:a-b);o.th=s[Math.min(s.length,+rule.a||10)-1]}
   if(rule.t=='scale'||rule.t=='bar'){o.mn=Math.min(...vals);o.mx=Math.max(...vals)}
   if(rule.t=='dup'){const n=new Map();eachIn(sh,rule,cell=>{const v=spVal(cell);if(v!=null&&v!=='')n.set(String(v).toUpperCase(),(n.get(String(v).toUpperCase())||0)+1)});o.d=n}return o});cfCache={v:vers,s:sh,m};return m}
 const mix=(a,b,t)=>{const p=x=>[1,3,5].map(i=>parseInt(x.slice(i,i+2),16));const A=p(a),B=p(b);return'#'+A.map((v,i)=>Math.round(v+(B[i]-v)*t).toString(16).padStart(2,'0')).join('')};
 function cfFor(r,c,v){let out=null;for(const o of cfInfo()){const R=o.rule;if(r<R.r0||r>R.r1||c<R.c0||c>R.c1)continue;let hit=false;const n=typeof v=='number';const a=R.a!==''&&!isNaN(+R.a)?+R.a:R.a,b=R.b!==''&&!isNaN(+R.b)?+R.b:R.b;
   switch(R.t){case'gt':hit=n&&v>a;break;case'lt':hit=n&&v<a;break;case'between':hit=n&&v>=Math.min(a,b)&&v<=Math.max(a,b);break;case'eq':hit=v!=null&&String(v).toUpperCase()==String(a).toUpperCase();break;case'text':hit=v!=null&&String(v).toUpperCase().includes(String(a).toUpperCase());break;
    case'top':hit=n&&v>=o.th;break;case'bottom':hit=n&&v<=o.th;break;case'dup':hit=v!=null&&v!==''&&o.d.get(String(v).toUpperCase())>1;break;
    case'scale':if(n&&isFinite(o.mn)){const t=o.mx>o.mn?(v-o.mn)/(o.mx-o.mn):.5,cs=R.cs||['#f8696b','#ffeb84','#63be7b'];out=Object.assign(out||{},{bg:cs.length==3?(t<.5?mix(cs[0],cs[1],t*2):mix(cs[1],cs[2],(t-.5)*2)):mix(cs[0],cs[1],t)})}continue;
    case'bar':if(n&&isFinite(o.mn)){out=Object.assign(out||{},{bar:(v-Math.min(0,o.mn))/((Math.max(0,o.mx)-Math.min(0,o.mn))||1),barc:R.color||'#638ec6'})}continue}
   if(hit)out=Object.assign(out||{},R.st||{bg:'#ffc7ce',fc:'#9c0006'})}return out}
 /* every cell holding something in a range (whole columns are not walked cell by cell) */
 function eachIn(s,g,fn){const n=(g.r1-g.r0+1)*(g.c1-g.c0+1);if(n<=20000){for(let r=g.r0;r<=g.r1;r++)for(let c=g.c0;c<=g.c1;c++){const cl=s.cells.get(spK(r,c));if(cl)fn(cl,r,c)}return}
  for(const[k,cl]of s.cells){const r=Math.floor(k/SPM),c=k%SPM;if(r>=g.r0&&r<=g.r1&&c>=g.c0&&c<=g.c1)fn(cl,r,c)}}
 /* ================= drawing ================= */
 let raf=0;const redraw=()=>{if(!raf)raf=requestAnimationFrame(()=>{raf=0;draw()})};
 function resize(){const d=devicePixelRatio||1;VW=sc.clientWidth;VH=sc.clientHeight;cv.width=Math.max(1,VW*d);cv.height=Math.max(1,VH*d);cv.style.width=VW+'px';cv.style.height=VH+'px';sizeSpacer();draw()}
 const BW={thin:1,medium:2,thick:3,double:3};
 /* paint cells r0..r1 × c0..c1 with X(c),Y(r) as left/top edges: used for the screen (each pane) and for printing */
 function paint(G,s,r0,r1,c0,c1,X,Y,z,opt){const grid=opt.grid,items=[];const covered=new Set();const ms=s.merges.filter(m=>m.r1>=r0&&m.r0<=r1&&m.c1>=c0&&m.c0<=c1);
  for(const m of ms)for(let r=Math.max(m.r0,r0);r<=Math.min(m.r1,r1);r++)for(let c=Math.max(m.c0,c0);c<=Math.min(m.c1,c1);c++)if(r!=m.r0||c!=m.c0)covered.add(spK(r,c));
  /* backgrounds */
  for(let r=r0;r<=r1;r++){const y=Y(r),h=Y(r+1)-y;if(h<=0)continue;for(let c=c0;c<=c1;c++){const k=spK(r,c);if(covered.has(k))continue;const cl=s.cells.get(k);const m=ms.find(m=>m.r0==r&&m.c0==c);
    const x=X(c),w=(m?X(m.c1+1):X(c+1))-x,hh=(m?Y(m.r1+1):Y(r+1))-y;if(w<=0)continue;const st=cl&&cl.st,v=spVal(cl),cf=s.cf.length&&s===sh?cfFor(r,c,v):null;const bg=(cf&&cf.bg)||(st&&st.bg);
    if(bg){G.fillStyle=bg;G.fillRect(x,y,w,hh)}else if(m){G.fillStyle='#fff';G.fillRect(x,y,w,hh)}
    if(cl&&(v!=null&&v!==''||cf&&cf.bar)||m)items.push([r,c,x,y,w,hh,cl,st,v,cf,!!m])}}
  /* grid lines */
  if(grid){G.strokeStyle='#e2e3e3';G.lineWidth=1;G.beginPath();for(let c=c0;c<=c1+1;c++){const x=Math.round(X(c))+.5;G.moveTo(x,Y(r0));G.lineTo(x,Y(r1+1))}for(let r=r0;r<=r1+1;r++){const y=Math.round(Y(r))+.5;G.moveTo(X(c0),y);G.lineTo(X(c1+1),y)}G.stroke();
   for(const m of ms){const x=X(m.c0),y=Y(m.r0),w=X(m.c1+1)-x,h=Y(m.r1+1)-y;const cl=s.cells.get(spK(m.r0,m.c0));G.fillStyle=(cl&&cl.st&&cl.st.bg)||'#fff';G.fillRect(x+1,y+1,w-1,h-1)}}
  /* borders */
  for(let r=r0;r<=r1;r++)for(let c=c0;c<=c1;c++){const cl=s.cells.get(spK(r,c));const bd=cl&&cl.st&&cl.st.bd;if(!bd)continue;const m=ms.find(m=>m.r0==r&&m.c0==c);const x0=X(c),y0=Y(r),x1=m?X(m.c1+1):X(c+1),y1=m?Y(m.r1+1):Y(r+1);G.strokeStyle='#000';
   const ln=(k,a,b,cc,d)=>{const t=bd[k];if(!t)return;G.lineWidth=BW[t]||1;G.beginPath();if(t=='double'){G.lineWidth=1;G.moveTo(a[0]-d[0],a[1]-d[1]);G.lineTo(b[0]-d[0],b[1]-d[1]);G.moveTo(a[0]+d[0],a[1]+d[1]);G.lineTo(b[0]+d[0],b[1]+d[1])}else{G.moveTo(a[0],a[1]);G.lineTo(b[0],b[1])}G.stroke()};
   ln('t',[x0,y0+.5],[x1,y0+.5],0,[0,1]);ln('b',[x0,y1-.5],[x1,y1-.5],0,[0,1]);ln('l',[x0+.5,y0],[x0+.5,y1],0,[1,0]);ln('r',[x1-.5,y0],[x1-.5,y1],0,[1,0])}
  /* text */
  G.textBaseline='middle';
  for(const[r,c,x,y,w,h,cl,st,v,cf,merged]of items){if(cf&&cf.bar){G.fillStyle=cf.barc;G.globalAlpha=.75;G.fillRect(x+2,y+3,Math.max(0,(w-4)*Math.min(1,cf.bar)),h-6);G.globalAlpha=1}
   let t=showF&&cl&&cl.f!=null?cl.f:spText(cl);if(t==='')continue;if(s.cm[spK(r,c)]){}
   G.font=fontOf(Object.assign({},st,{b:(st&&st.b)||(cf&&cf.b)}));G.fillStyle=(cf&&cf.fc)||(st&&st.fc)||(SPE.isE(v)?'#c00000':'#000');
   const num=typeof v=='number'&&!showF;let ha=(st&&st.ha)||(num?'right':typeof v=='boolean'||SPE.isE(v)?'center':'left');const pad=3*z;
   if(num&&!showF){const tw=G.measureText(t).width;if(tw>w-2*pad&&!(st&&st.nf&&/@/.test(st.nf))){t=w>14*z?'#'.repeat(Math.max(1,Math.floor((w-2*pad)/G.measureText('#').width))):''}}
   const va=(st&&st.va)||'bottom',lh=((st&&st.sz)||11)*4/3*z*1.2;
   let lines=[t];if(st&&st.wrap){lines=[];for(const para of t.split('\n')){let cur='';for(const word of para.split(' ')){const tr=cur?cur+' '+word:word;if(G.measureText(tr).width>w-2*pad&&cur){lines.push(cur);cur=word}else cur=tr}lines.push(cur)}}
   else if(t.includes('\n'))lines=t.split('\n');
   /* text that does not fit runs on over empty cells to the right (or left) */
   let cx0=x,cx1=x+w;if(!(st&&st.wrap)&&!num&&!merged){const tw=G.measureText(t).width;if(tw>w-2*pad){if(ha=='left'){let cc=c+1;while(cx1-x<tw+2*pad&&cc<=c1+20&&cc<SPM){const n=s.cells.get(spK(r,cc));if(n&&spText(n)!==''||mergeIn(s,r,cc))break;cx1=X(cc+1);cc++}}else if(ha=='right'){let cc=c-1;while(x+w-cx0<tw+2*pad&&cc>=0&&cc>=c0-20){const n=s.cells.get(spK(r,cc));if(n&&spText(n)!=='')break;cx0=X(cc);cc--}}}}
   G.save();G.beginPath();G.rect(cx0,y,cx1-cx0,h);G.clip();const tot=lines.length*lh;let ty=va=='top'?y+lh/2+1:va=='middle'?y+h/2-tot/2+lh/2:y+h-tot+lh/2-2*z;
   for(const L of lines){const tx=ha=='center'?x+w/2:ha=='right'?x+w-pad:x+pad;G.textAlign=ha=='center'?'center':ha=='right'?'right':'left';G.fillText(L,tx,ty);
    if(st&&(st.u||st.s)){const lw=G.measureText(L).width,lx=ha=='center'?tx-lw/2:ha=='right'?tx-lw:tx;G.fillRect(lx,st.u?ty+lh*.35:ty,lw,Math.max(1,z))}ty+=lh}G.restore()}
  /* comments: a red corner; data validation lists: an arrow when the cell is selected */
  for(const k of Object.keys(s.cm)){const[r,c]=spRC(+k);if(r<r0||r>r1||c<c0||c>c1)continue;const x=X(c+1),y=Y(r);G.fillStyle='#d00';G.beginPath();G.moveTo(x-6*z,y);G.lineTo(x,y);G.lineTo(x,y+6*z);G.fill()}}
 const mergeIn=(s,r,c)=>s.merges.some(m=>r>=m.r0&&r<=m.r1&&c>=m.c0&&c<=m.c1);
 function draw(){if(!W.el.isConnected)return;geom(sh);const d=devicePixelRatio||1;g.setTransform(d,0,0,d,0,0);g.fillStyle='#fff';g.fillRect(0,0,VW,VH);
  const f=sh.freeze,fx_=FX(),fy_=FY();
  const cMain0=colAtX(HW+fx_+1),cMain1=Math.min(SPM-1,colAtX(VW-1)),rMain0=rowAtY(HH+fy_+1),rMain1=Math.min(SPR-1,rowAtY(VH-1));
  const panes=[[f.r,rMain1,f.c,cMain1,HW+fx_,HH+fy_],[f.r,rMain1,0,f.c-1,HW,HH+fy_],[0,f.r-1,f.c,cMain1,HW+fx_,HH],[0,f.r-1,0,f.c-1,HW,HH]];
  for(const[r0,r1,c0,c1,px,py]of panes){if(r1<r0||c1<c0)continue;const R0=r0==f.r?Math.max(r0,rMain0):r0,C0=c0==f.c?Math.max(c0,cMain0):c0;const pw=(c0==0&&f.c?HW+fx_:VW)-px,ph=(r0==0&&f.r?HH+fy_:VH)-py;if(pw<=0||ph<=0)continue;
   g.save();g.beginPath();g.rect(px,py,pw,ph);g.clip();paint(g,sh,R0,r1,C0,c1,Xc,Yr,zoom,{grid:sh.grid!==false});drawSel({r0,r1,c0,c1,px,py,pw,ph});g.restore()}
  if(f.c){g.fillStyle='#9ca3af';g.fillRect(HW+fx_-1,HH,1.5,VH)}if(f.r){g.fillStyle='#9ca3af';g.fillRect(HW,HH+fy_-1,VW,1.5)}
  drawHeaders(cMain0,cMain1,rMain0,rMain1);placeCharts();placeEditor()}
 function selBounds(){let r0=1e9,r1=-1,c0=1e9,c1=-1;for(const g_ of sel.rs){r0=Math.min(r0,g_.r0);r1=Math.max(r1,g_.r1);c0=Math.min(c0,g_.c0);c1=Math.max(c1,g_.c1)}return{r0,r1,c0,c1}}
 function rectOf(gr,P){/* a range's rectangle on screen within one pane */
  if(gr.r1<P.r0||gr.r0>P.r1||gr.c1<P.c0||gr.c0>P.c1)return null;const x0=gr.c0<P.c0?P.px-3:Xc(gr.c0),x1=gr.c1>P.c1?P.px+P.pw+3:Xc(gr.c1+1),y0=gr.r0<P.r0?P.py-3:Yr(gr.r0),y1=gr.r1>P.r1?P.py+P.ph+3:Yr(gr.r1+1);return[x0,y0,x1-x0,y1-y0]}
 function drawSel(P){const ac='#1a73e8';g.fillStyle='rgba(26,115,232,.12)';
  for(const gr of sel.rs){const R=rectOf(gr,P);if(!R)continue;g.fillRect(...R);g.strokeStyle=ac;g.lineWidth=sel.rs.length>1?1:2;g.strokeRect(R[0]+.5,R[1]+.5,R[2]-1,R[3]-1)}
  const m=mergeAt(sel.r,sel.c)||{r0:sel.r,r1:sel.r,c0:sel.c,c1:sel.c};const A=rectOf(m,P);if(A){g.fillStyle='#fff';const cl=cellAt(m.r0,m.c0);if(cl&&cl.st&&cl.st.bg)g.fillStyle=cl.st.bg;
   g.save();g.beginPath();g.rect(...A);g.clip();g.fillRect(...A);paint(g,sh,m.r0,m.r1,m.c0,m.c1,Xc,Yr,zoom,{grid:false});g.restore();g.strokeStyle=ac;g.lineWidth=2;g.strokeRect(A[0]+1,A[1]+1,A[2]-2,A[3]-2)}
  /* the fill handle */
  const last=sel.rs.at(-1);const H=rectOf({r0:last.r1,r1:last.r1,c0:last.c1,c1:last.c1},P);if(H&&!edit){g.fillStyle=ac;g.fillRect(H[0]+H[2]-4,H[1]+H[3]-4,7,7);g.strokeStyle='#fff';g.lineWidth=1;g.strokeRect(H[0]+H[2]-4.5,H[1]+H[3]-4.5,8,8)}
  if(fillDrag&&fillDrag.to){const R=rectOf(fillDrag.to,P);if(R){g.setLineDash([4,3]);g.strokeStyle='#555';g.strokeRect(R[0]+.5,R[1]+.5,R[2]-1,R[3]-1);g.setLineDash([])}}
  if(clip&&clip.sheet===sh&&!clip.done){const R=rectOf(clip.g,P);if(R){g.setLineDash([5,4]);g.lineDashOffset=-(Date.now()/60%9);g.strokeStyle='#1a73e8';g.lineWidth=2;g.strokeRect(R[0]+1,R[1]+1,R[2]-2,R[3]-2);g.setLineDash([])}}
  if(edit&&edit.pt){const R=rectOf(edit.pt.g,P);if(R){g.setLineDash([4,3]);g.strokeStyle='#e8710a';g.lineWidth=2;g.strokeRect(R[0]+1,R[1]+1,R[2]-2,R[3]-2);g.setLineDash([])}}
  /* list validation: a ▾ by the active cell */
  const dv=dvAt(sel.r,sel.c);if(dv&&dv.t=='list'&&A){g.fillStyle='#f3f4f6';g.fillRect(A[0]+A[2]+1,A[1],16,A[3]);g.strokeStyle='#9ca3af';g.strokeRect(A[0]+A[2]+1.5,A[1]+.5,15,A[3]-1);g.fillStyle='#333';g.font='10px sans-serif';g.textAlign='center';g.fillText('▼',A[0]+A[2]+9,A[1]+A[3]/2)}
  /* AutoFilter buttons */
  const F=sh.filter;if(F&&F.r0>=P.r0&&F.r0<=P.r1)for(let c=Math.max(F.c0,P.c0);c<=Math.min(F.c1,P.c1);c++){const x=Xc(c+1)-17,y=Yr(F.r0)+(Yr(F.r0+1)-Yr(F.r0)-15)/2;g.fillStyle=F.cr&&F.cr[c]?'#dbeafe':'#f3f4f6';g.fillRect(x,y,15,15);g.strokeStyle='#9ca3af';g.lineWidth=1;g.strokeRect(x+.5,y+.5,14,14);g.fillStyle='#333';g.font='9px sans-serif';g.textAlign='center';g.fillText(F.cr&&F.cr[c]?'⧩':'▼',x+7.5,y+8)}}
 function drawHeaders(c0,c1,r0,r1){const f=sh.freeze,b=selBounds(),full=g_=>g_.r0==0&&g_.r1==SPR-1,fullC=g_=>g_.c0==0&&g_.c1==SPM-1;
  g.font=`${Math.max(9,11*zoom)}px Segoe UI,Arial,sans-serif`;g.textBaseline='middle';g.textAlign='center';
  g.fillStyle='#f3f4f6';g.fillRect(0,0,VW,HH);g.fillRect(0,0,HW,VH);
  const cols=[];for(let c=0;c<f.c;c++)cols.push(c);for(let c=Math.max(f.c,c0);c<=c1;c++)cols.push(c);
  for(const c of cols){const x=Xc(c),w=Xc(c+1)-x;if(w<=0||x+w<HW||(c>=f.c&&x+w<HW+FX()))continue;const on=c>=b.c0&&c<=b.c1,all=sel.rs.some(g_=>full(g_)&&c>=g_.c0&&c<=g_.c1);
   if(on){g.fillStyle=all?'#1a73e8':'#d3e3fd';g.fillRect(Math.max(x,c>=f.c?HW+FX():HW),0,w,HH)}g.fillStyle=all?'#fff':on?'#0b57d0':'#444';g.fillText(SPE.colName(c),x+w/2,HH/2);g.fillStyle='#d1d5db';g.fillRect(x+w-1,0,1,HH)}
  const rows=[];for(let r=0;r<f.r;r++)rows.push(r);for(let r=Math.max(f.r,r0);r<=r1;r++)rows.push(r);
  g.save();g.beginPath();g.rect(0,HH,HW,VH);g.clip();
  for(const r of rows){const y=Yr(r),h=Yr(r+1)-y;if(h<=0||(r>=f.r&&y+h<HH+FY()))continue;const on=r>=b.r0&&r<=b.r1,all=sel.rs.some(g_=>fullC(g_)&&r>=g_.r0&&r<=g_.r1);
   if(on){g.fillStyle=all?'#1a73e8':'#d3e3fd';g.fillRect(0,Math.max(y,r>=f.r?HH+FY():HH),HW,h)}g.fillStyle=all?'#fff':on?'#0b57d0':'#444';g.fillText(String(r+1),HW/2,y+h/2);g.fillStyle='#d1d5db';g.fillRect(0,y+h-1,HW,1)}
  g.restore();g.fillStyle='#d1d5db';g.fillRect(HW-1,0,1,VH);g.fillRect(0,HH-1,VW,1);g.fillStyle='#e5e7eb';g.fillRect(0,0,HW-1,HH-1);g.fillStyle='#9ca3af';g.beginPath();g.moveTo(HW-5,HH-14);g.lineTo(HW-5,HH-5);g.lineTo(HW-14,HH-5);g.fill()}
 /* ================= selection ================= */
 const clampR=r=>Math.max(0,Math.min(SPR-1,r)),clampC=c=>Math.max(0,Math.min(SPM-1,c));
 const norm=(a,b,c,d)=>({r0:Math.min(a,c),r1:Math.max(a,c),c0:Math.min(b,d),c1:Math.max(b,d)});
 function grow(g_){/* a range made large enough to hold every merged cell it touches */let ch=true;while(ch){ch=false;for(const m of sh.merges){if(m.r1>=g_.r0&&m.r0<=g_.r1&&m.c1>=g_.c0&&m.c0<=g_.c1&&(m.r0<g_.r0||m.r1>g_.r1||m.c0<g_.c0||m.c1>g_.c1)){g_.r0=Math.min(g_.r0,m.r0);g_.r1=Math.max(g_.r1,m.r1);g_.c0=Math.min(g_.c0,m.c0);g_.c1=Math.max(g_.c1,m.c1);ch=true}}}return g_}
 function select(r,c,ext,add){r=clampR(r);c=clampC(c);if(ext){sel.rs[sel.rs.length-1]=grow(norm(sel.ar,sel.ac,r,c))}else{const m=mergeAt(r,c);if(m){r=m.r0;c=m.c0}const gg=m?{...m}:{r0:r,r1:r,c0:c,c1:c};if(add)sel.rs.push(gg);else sel.rs=[gg];sel.r=r;sel.c=c;sel.ar=r;sel.ac=c}
  afterSel()}
 function afterSel(){if(!edit){fx.value=spInput(cellAt(sel.r,sel.c))}nb.value=nameOfSel();status();updTb();redraw()}
 function nameOfSel(){const g_=sel.rs.at(-1);for(const[n,v]of Object.entries(book.names)){const r=spParseRange(v.ref);if(r&&(v.sheet||sh.name)==sh.name&&r.r0==g_.r0&&r.r1==g_.r1&&r.c0==g_.c0&&r.c1==g_.c1)return n}if(sel.rs.length==1&&g_.r0==g_.r1&&g_.c0==g_.c1)return SPE.a1(sel.r,sel.c);const m=mergeAt(sel.r,sel.c);if(m&&m.r0==g_.r0&&m.c0==g_.c0&&m.r1==g_.r1&&m.c1==g_.c1)return SPE.a1(sel.r,sel.c);return spRangeStr(g_)}
 function scrollInto(r,c){geom(sh);const f=sh.freeze;if(r>=f.r){const top=(sh._ry[r]-sh._ry[f.r])*zoom,bot=(sh._ry[r+1]-sh._ry[f.r])*zoom,vh=VH-HH-FY();if(top<sc.scrollTop)sc.scrollTop=top;else if(bot>sc.scrollTop+vh)sc.scrollTop=bot-vh}
  if(c>=f.c){const l=(sh._cx[c]-sh._cx[f.c])*zoom,rr=(sh._cx[c+1]-sh._cx[f.c])*zoom,vw=VW-HW-FX();if(l<sc.scrollLeft)sc.scrollLeft=l;else if(rr>sc.scrollLeft+vw)sc.scrollLeft=Math.min(l,rr-vw)}
  if(parseFloat(sp.style.height)<HH+FY()+(sh._ry[r+1]-sh._ry[f.r])*zoom+200)sizeSpacer()}
 /* Ctrl+arrow: to the edge of the data */
 function jump(r,c,dr,dc){const has=(r,c)=>{const cl=cellAt(r,c);return cl&&spText(cl)!==''};const ok=(r,c)=>r>=0&&r<SPR&&c>=0&&c<SPM;
  if(!ok(r+dr,c+dc))return[r,c];if(has(r,c)&&has(r+dr,c+dc)){while(ok(r+dr,c+dc)&&has(r+dr,c+dc)){r+=dr;c+=dc}return[r,c]}
  r+=dr;c+=dc;if(dr>0||dc>0){/* the next filled cell, using the stored cells rather than walking 100,000 rows */let best=null;for(const k of sh.cells.keys()){const rr=Math.floor(k/SPM),cc=k%SPM;if(dr&&cc==c&&rr>=r&&(best==null||rr<best)&&has(rr,cc))best=rr;if(dc&&rr==r&&cc>=c&&(best==null||cc<best)&&has(rr,cc))best=cc}
   if(best==null)return dr?[SPR-1,c]:[r,SPM-1];return dr?[best,c]:[r,best]}
  while(ok(r,c)&&!has(r,c)&&ok(r+dr,c+dc)){r+=dr;c+=dc}return[r,c]}
 /* ================= changes, undo and redo ================= */
 const clone=s=>({...s,cells:new Map([...s.cells].map(([k,c])=>[k,{...c,ast:undefined,_g:0}])),cw:{...s.cw},rh:{...s.rh},hid:{...s.hid},hc:{...s.hc},freeze:{...s.freeze},merges:s.merges.map(m=>({...m})),cf:JSON.parse(JSON.stringify(s.cf)),dv:JSON.parse(JSON.stringify(s.dv)),cm:{...s.cm},charts:JSON.parse(JSON.stringify(s.charts)),filter:s.filter?JSON.parse(JSON.stringify(s.filter)):null,_cx:null,fk:null});
 function begin(label){if(tx)commit();tx={label,cells:new Map(),snaps:new Map(),book:null,sel:JSON.parse(JSON.stringify(sel)),sheet:sh}}
 function setCell(s,r,c,cell){const k=spK(r,c);if(!tx.snaps.has(s)){const key=s.id+':'+k;if(!tx.cells.has(key))tx.cells.set(key,[s,k,s.cells.get(k)])}if(cell&&(cell.v!=null&&cell.v!==''||cell.f!=null||cell.st))s.cells.set(k,cell);else s.cells.delete(k);s.fk=null}
 function snap(s){if(!tx.snaps.has(s))tx.snaps.set(s,clone(s))}
 function snapBook(){if(!tx.book)tx.book={sheets:[...book.sheets],names:JSON.parse(JSON.stringify(book.names)),active:book.active}}
 function commit(opts){if(!tx)return;const t=tx;tx=null;if(!t.cells.size&&!t.snaps.size&&!t.book){redraw();return}undoS.push(t);if(undoS.length>200)undoS.shift();redoS=[];changed(t,opts)}
 function changed(t,opts){dirty=true;vers++;cfCache=null;
  const struct=t.snaps.size||t.book||[...t.cells.values()].some(([s,k,old])=>old&&old.f!=null||(s.cells.get(k)||{}).f!=null);
  if(struct||!book._dep){book._dep=null;for(const s of book.sheets)s.fk=null;SPE.recalc(book,null)}else SPE.recalc(book,[...t.cells.values()].map(([s,k])=>[s,...spRC(k)]));
  for(const s of t.snaps.keys())regeom(s);for(const[s,k,old]of t.cells.values()){const n=s.cells.get(k);if((old&&old.st&&old.st.sz)||(n&&n.st&&n.st.sz))regeom(s)}if(sh.filter)applyFilter(false);ttl();afterSel();sizeSpacer();renderTabs();if(!(opts&&opts.quiet))status();
  if(notey){const n=countCells();const d=(n-lastCount)*20;lastCount=n;lastChange={label:t.label,sheet:sh,r:sel.r,c:sel.c,cells:[...t.cells.values()].map(([s_,k])=>[s_,k])};notey.edited(opts&&opts.kind||(t.label=='Paste'?'insertFromPaste':''),d)}}
 let lastCount=0,lastChange=null;const countCells=()=>book.sheets.reduce((a,s_)=>a+s_.cells.size,0);
 function apply(t){/* undo t, giving what redoes it */const inv={label:t.label,cells:new Map(),snaps:new Map(),book:null,sel:JSON.parse(JSON.stringify(sel)),sheet:sh};
  if(t.book){inv.book={sheets:[...book.sheets],names:JSON.parse(JSON.stringify(book.names)),active:book.active};book.sheets=t.book.sheets;book.names=t.book.names;book.active=t.book.active}
  for(const[s,old]of t.snaps){inv.snaps.set(s,clone(s));Object.assign(s,old,{_cx:null,fk:null})}
  for(const[key,[s,k,old]]of t.cells){inv.cells.set(key,[s,k,s.cells.get(k)]);if(old)s.cells.set(k,old);else s.cells.delete(k);s.fk=null}
  if(!book.sheets.includes(t.sheet))t.sheet=book.sheets[0];sh=t.sheet;sel=t.sel;sh._sel=sel;return inv}
 function undo(){if(edit)cancelEdit();const t=undoS.pop();if(!t)return say('Nothing to undo');redoS.push(apply(t));changed(t,{kind:'historyUndo'});say('Undone: '+t.label)}
 function redo(){if(edit)cancelEdit();const t=redoS.pop();if(!t)return say('Nothing to redo');undoS.push(apply(t));changed(t);say('Redone: '+t.label)}
 /* style changes: fn(old style object) → new style object, for every cell of the selection */
 function styleSel(label,fn){begin(label);for(const gr of sel.rs){const big=(gr.r1-gr.r0+1)*(gr.c1-gr.c0+1)>200000;
   if(big){/* whole rows or columns: the cells that exist, plus the visible ones */const ks=new Set();eachIn(sh,gr,(cl,r,c)=>ks.add(spK(r,c)));for(let r=Math.max(gr.r0,rowAtY(HH+FY()+1));r<=Math.min(gr.r1,rowAtY(VH-1));r++)for(let c=Math.max(gr.c0,colAtX(HW+FX()+1));c<=Math.min(gr.c1,colAtX(VW-1));c++)ks.add(spK(r,c));
    for(const k of ks){const[r,c]=spRC(k);const cl=sh.cells.get(k);const st=fn(cl&&cl.st?{...cl.st}:{});setCell(sh,r,c,{...(cl||{v:null}),ast:undefined,st:clean(st)})}}
   else for(let r=gr.r0;r<=gr.r1;r++)for(let c=gr.c0;c<=gr.c1;c++){const cl=sh.cells.get(spK(r,c));const st=fn(cl&&cl.st?{...cl.st}:{},r,c);setCell(sh,r,c,{...(cl||{v:null}),ast:undefined,st:clean(st)})}}
  commit({quiet:1})}
 const clean=st=>{if(!st)return undefined;for(const k of Object.keys(st))if(st[k]==null||st[k]===''||st[k]===false||k=='bd'&&!Object.keys(st.bd).length)delete st[k];return Object.keys(st).length?st:undefined};
 const cur=()=>{const cl=cellAt(sel.r,sel.c);return(cl&&cl.st)||{}};
 /* ================= editing ================= */
 function startEdit(text,mode,src){if(edit)return;const cl=cellAt(sel.r,sel.c);edit={mode:mode||'edit',src:src||'cell',orig:spInput(cl),pt:null,home:sh,sheet:sh.name,r:sel.r,c:sel.c};const t=text==null?edit.orig:text;ed.value=t;fx.value=t;
  ed.style.display='block';placeEditor();if(edit.src=='cell'){ed.focus();ed.setSelectionRange(t.length,t.length)}status('Edit');hintUpd();redraw()}
 function placeEditor(){if(!edit){ed.style.display='none';return}const m=mergeAt(sel.r,sel.c)||{r0:sel.r,r1:sel.r,c0:sel.c,c1:sel.c};const x=Xc(m.c0),y=Yr(m.r0),w=Xc(m.c1+1)-x,h=Yr(m.r1+1)-y;const st=cur();
  ed.style.left=Math.max(HW,x)+'px';ed.style.top=Math.max(HH,y)+'px';ed.style.minWidth=Math.max(20,w)+'px';ed.style.height=Math.max(h,DH*zoom)+'px';ed.style.font=fontOf(st);ed.style.color=st.fc||'#000';ed.style.background=st.bg||'#fff';ed.style.textAlign=st.ha||'left';
  ed.style.width='auto';const need=Math.min(gw.clientWidth-x-4,Math.max(w,measure(ed.value,st)+24));ed.style.width=need+'px';ed.style.whiteSpace=st.wrap?'pre-wrap':'pre'}
 const mctx=document.createElement('canvas').getContext('2d');const measure=(t,st)=>{mctx.font=fontOf(st);return Math.max(...String(t).split('\n').map(l=>mctx.measureText(l).width))};
 function commitEdit(dr,dc){if(!edit)return true;const text=edit.src=='bar'?fx.value:ed.value;const home=edit.home;if(home!==sh){const i=book.sheets.indexOf(home);sh=home;book.active=i;sel=sh._sel||sel;renderTabs()}const r=edit.r,c=edit.c;sel.r=r;sel.c=c;
  if(text!==edit.orig){const x=spParse(text);
   if(x&&x.f){try{SPE.parse(x.f.slice(1))}catch(e){let fix=x.f;const open=(fix.match(/\(/g)||[]).length-(fix.match(/\)/g)||[]).length;if(open>0){fix+=')'.repeat(open);try{SPE.parse(fix.slice(1));x.f=fix}catch(e2){info('This formula has a problem','Check the brackets, commas and quotation marks.<br><code>'+offEsc(text)+'</code>');return false}}else{info('This formula has a problem','Check the brackets, commas and quotation marks.<br><code>'+offEsc(text)+'</code>');return false}}}
   const dv=dvAt(r,c);if(dv&&x&&!dvOk(dv,x)){info('Not allowed here',offEsc(dv.msg||dvMsg(dv)));return false}
   const old=cellAt(r,c);if(clip&&!clip.cut){clip=null}begin('Typing');const st=old&&old.st?{...old.st}:{};if(x&&x.nf&&(!st.nf||st.nf=='General'||(/[yd]/.test(x.nf)&&!/[yd]/.test(st.nf))))st.nf=x.nf;
   const cell=x?(x.f?{f:x.f}:{v:x.v}):{v:null};cell.st=clean(st);setCell(sh,r,c,cell);
   if(st.wrap){const w=Xc(c+1)-Xc(c);mctx.font=fontOf(st);const lines=Math.ceil(measure(text,st)/Math.max(10,w-6)+.01);if(sh.rh[r]==null||sh.rh[r]<lines*DH){snap(sh);sh.rh[r]=Math.min(400,Math.max(DH,lines*18))}}
   endEdit();commit()}else endEdit();
  if(dc&&!dr){if(tabStart==null)tabStart=c}if(dr&&!dc&&tabStart!=null&&sel.rs.length==1&&sel.rs[0].r0==sel.rs[0].r1&&sel.rs[0].c0==sel.rs[0].c1){const sc_=tabStart;tabStart=null;select(r+dr,sc_);scrollInto(sel.r,sel.c);return true}if(dr)tabStart=null;
  if(dr||dc){const g_=sel.rs[0];if(sel.rs.length==1&&(g_.r0!=g_.r1||g_.c0!=g_.c1)&&!(mergeAt(r,c)&&g_.r0==mergeAt(r,c).r0&&g_.r1==mergeAt(r,c).r1)){/* Enter inside a selection moves within it */let nr=r+dr,nc=c+dc;if(nr>g_.r1){nr=g_.r0;nc++}if(nc>g_.c1){nc=g_.c0;if(dc)nr++}if(nr>g_.r1)nr=g_.r0;if(nr<g_.r0)nr=g_.r1;if(nc<g_.c0)nc=g_.c1;sel.r=nr;sel.c=nc;afterSel()}else{const m=mergeAt(r,c);select((dr>0&&m?m.r1:r)+dr,(dc>0&&m?m.c1:c)+dc)}scrollInto(sel.r,sel.c)}
  return true}
 function endEdit(){edit=null;ed.style.display='none';hint.style.display='none';sc.focus({preventScroll:true});status();redraw()}
 function cancelEdit(){if(!edit)return;endEdit();fx.value=spInput(cellAt(sel.r,sel.c))}
 /* while typing a formula: the description of the function being typed, and names to choose from */
 let hintSel=0,hintList=[];
 function hintUpd(){const el=edit&&edit.src=='bar'?fx:ed,t=el.value.slice(0,el.selectionStart||el.value.length);hint.style.display='none';hintList=[];if(!edit||t[0]!='=')return;
  const m=/([A-Z][A-Z0-9.]*)$/i.exec(t);if(m&&!/[A-Z]{1,3}\d+$/i.test(m[1])){const p=m[1].toUpperCase();hintList=Object.keys(SPE.F).filter(n=>n.startsWith(p)).sort().slice(0,8);if(hintList.length){hintSel=Math.min(hintSel,hintList.length-1);
   hint.innerHTML=hintList.map((n,i)=>`<div class='sp-hi${i==hintSel?' on':''}' data-n='${n}'><b>${n}</b> <span>${offEsc((SPE.D[n]||'').split(':').slice(1).join(':'))}</span></div>`).join('')+`<div class='sp-hk'>Tab: put in the name</div>`;showHint();hint.querySelectorAll('.sp-hi').forEach(d=>d.onmousedown=e=>{e.preventDefault();takeHint(d.dataset.n)});return}}
  /* inside a function's brackets: its description */let depth=0,fn=null;for(let i=t.length-1;i>=0;i--){const ch=t[i];if(ch==')')depth++;else if(ch=='('){if(depth)depth--;else{const mm=/([A-Z][A-Z0-9.]*)$/i.exec(t.slice(0,i));if(mm){fn=mm[1].toUpperCase();break}}}}
  if(fn&&SPE.D[fn]){hint.innerHTML=`<div class='sp-hd'>${offEsc(SPE.D[fn])}</div>`;showHint()}}
 function showHint(){hint.style.display='block';const el=edit.src=='bar'?fx:ed;const r=el.getBoundingClientRect(),gr=gw.getBoundingClientRect();hint.style.left=Math.max(0,r.left-gr.left)+'px';hint.style.top=(edit.src=='bar'?2:r.bottom-gr.top+2)+'px'}
 function takeHint(n){const el=edit.src=='bar'?fx:ed;const pos=el.selectionStart,t=el.value;const m=/([A-Z][A-Z0-9.]*)$/i.exec(t.slice(0,pos));const a=pos-(m?m[1].length:0);el.value=t.slice(0,a)+n+'('+t.slice(pos);el.setSelectionRange(a+n.length+1,a+n.length+1);sync(el);hintUpd()}
 function sync(from){if(from===ed)fx.value=ed.value;else ed.value=fx.value;placeEditor()}
 /* clicking cells while typing a formula puts their addresses in */
 function refWanted(){const el=edit.src=='bar'?fx:ed;const t=el.value;if(t[0]!='=')return false;if(edit.pt&&el.selectionStart==edit.pt.b)return true;const before=t.slice(0,el.selectionStart).trimEnd();return /[=(,+\-*/^&<>:;]$/.test(before)}
 function putRef(g_,sheetName){const el=edit.src=='bar'?fx:ed;const ref=(sheetName&&sheetName!=edit.sheet?(/^[A-Za-z_][A-Za-z0-9_]*$/.test(sheetName)?sheetName:"'"+sheetName.replace(/'/g,"''")+"'")+'!':'')+spRangeStr(g_);
  let a=el.selectionStart,b=el.selectionEnd;if(edit.pt&&a==edit.pt.b){a=edit.pt.a}el.value=el.value.slice(0,a)+ref+el.value.slice(b);edit.pt={a,b:a+ref.length,g:g_};el.setSelectionRange(a+ref.length,a+ref.length);sync(el);redraw()}
 ed.addEventListener('input',()=>{edit&&(edit.pt=null);sync(ed);hintSel=0;hintUpd()});fx.addEventListener('input',()=>{if(!edit)startEdit(fx.value,'edit','bar');edit.pt=null;sync(fx);hintSel=0;hintUpd()});
 fx.addEventListener('focus',()=>{if(!edit)startEdit(null,'edit','bar');else edit.src='bar'});
 fx.addEventListener('keydown',e=>{e.stopPropagation();edKey(e,fx)});ed.addEventListener('keydown',e=>{e.stopPropagation();edKey(e,ed)});
 function edKey(e,el){if(hintList.length&&hint.style.display!='none'){if(e.key=='ArrowDown'||e.key=='ArrowUp'){e.preventDefault();hintSel=(hintSel+(e.key=='ArrowDown'?1:hintList.length-1))%hintList.length;hintUpd();return}if(e.key=='Tab'){e.preventDefault();takeHint(hintList[hintSel]);return}}
  const k=e.key;if(k=='Enter'&&!e.altKey){e.preventDefault();if(e.ctrlKey){fillSelWith(el.value);return}commitEdit(e.shiftKey?-1:1,0);return}
  if(k=='Enter'&&e.altKey){e.preventDefault();const p=el.selectionStart;el.value=el.value.slice(0,p)+'\n'+el.value.slice(el.selectionEnd);el.setSelectionRange(p+1,p+1);sync(el);return}
  if(k=='Tab'){e.preventDefault();commitEdit(0,e.shiftKey?-1:1);return}if(k=='Escape'){e.preventDefault();cancelEdit();return}
  if(k=='F2'){e.preventDefault();edit.mode=edit.mode=='enter'?'edit':'enter';status(edit.mode=='enter'?'Enter':'Edit');return}
  if(k=='F4'){e.preventDefault();cycleAbs(el);return}
  if(edit.mode=='enter'&&/^Arrow/.test(k)&&el===ed){const d={ArrowUp:[-1,0],ArrowDown:[1,0],ArrowLeft:[0,-1],ArrowRight:[0,1]}[k];
   if(refWanted()){e.preventDefault();const base=edit.pt?edit.pt.g:{r0:sel.r,c0:sel.c,r1:sel.r,c1:sel.c};let r=(edit.pt?base.r1:sel.r)+d[0],c=(edit.pt?base.c1:sel.c)+d[1];r=clampR(r);c=clampC(c);putRef(e.shiftKey&&edit.pt?norm(base.r0,base.c0,r,c):{r0:r,r1:r,c0:c,c1:c});return}
   e.preventDefault();commitEdit(d[0],d[1]);return}}
 function cycleAbs(el){const t=el.value,p=el.selectionStart;const re=/\$?[A-Z]{1,3}\$?\d+/gi;let m;while((m=re.exec(t))){if(p>=m.index&&p<=m.index+m[0].length){const s=m[0],a=/^\$/.test(s),b=/[A-Z]\$\d/i.test(s);const base=s.replace(/\$/g,''),cl=/[A-Z]+/i.exec(base)[0],rw=base.slice(cl.length);const nx=!a&&!b?'$'+cl+'$'+rw:a&&b?cl+'$'+rw:!a&&b?'$'+cl+rw:cl+rw;el.value=t.slice(0,m.index)+nx+t.slice(m.index+s.length);el.setSelectionRange(m.index+nx.length,m.index+nx.length);sync(el);return}}}
 function fillSelWith(text){/* Ctrl+Enter: the same entry in every selected cell, formulas adjusted */const x=spParse(text);endEdit();begin('Fill');for(const gr of sel.rs)for(let r=gr.r0;r<=Math.min(gr.r1,gr.r0+50000);r++)for(let c=gr.c0;c<=gr.c1;c++){const old=cellAt(r,c);const cell=!x?{v:null}:x.f?{f:SPE.copyFormula(x.f,r-sel.r,c-sel.c)}:{v:x.v};cell.st=old&&old.st;setCell(sh,r,c,cell)}commit()}
 /* ================= data validation ================= */
 const dvAt=(r,c)=>sh.dv.find(d=>r>=d.r0&&r<=d.r1&&c>=d.c0&&c<=d.c1);
 function dvList(d){const l=String(d.list||'');if(l[0]=='='){const g_=spParseRange(l.slice(1).replace(/^.*!/,''));if(!g_)return[];const out=[];for(let r=g_.r0;r<=Math.min(g_.r1,g_.r0+500);r++)for(let c=g_.c0;c<=g_.c1;c++){const t=spText(cellAt(r,c));if(t!=='')out.push(t)}return out}return l.split(',').map(s=>s.trim()).filter(Boolean)}
 function dvOk(d,x){if(x.f)return true;const v=x.v;if(v==null||v==='')return true;if(d.t=='list')return dvList(d).some(t=>t.toUpperCase()==String(typeof v=='number'?SPE.fmtValue(v,x.nf):v).toUpperCase());
  if(d.t=='num'||d.t=='whole'||d.t=='date'){if(typeof v!='number')return false;if(d.t=='whole'&&!Number.isInteger(v))return false;const mn=d.t=='date'?SPE.parseDate(String(d.min||''))??-Infinity:d.min===''||d.min==null?-Infinity:+d.min,mx=d.t=='date'?SPE.parseDate(String(d.max||''))??Infinity:d.max===''||d.max==null?Infinity:+d.max;return v>=mn&&v<=mx}
  if(d.t=='len')return String(v).length<=+d.max;return true}
 const dvMsg=d=>d.t=='list'?'Choose one of: '+dvList(d).join(', '):d.t=='len'?`At most ${d.max} characters.`:`Enter ${d.t=='whole'?'a whole number':d.t=='date'?'a date':'a number'}${d.min!==''&&d.min!=null?' from '+d.min:''}${d.max!==''&&d.max!=null?' to '+d.max:''}.`;
 function dvPick(){const d=dvAt(sel.r,sel.c);if(!d||d.t!='list')return;const items=dvList(d).map(t=>[t,'',()=>{begin('Choose');const x=spParse(t);const old=cellAt(sel.r,sel.c);setCell(sh,sel.r,sel.c,{...(x.f?{f:x.f}:{v:x.v}),st:old&&old.st});commit()}]);
  const m=mergeAt(sel.r,sel.c)||{r1:sel.r,c0:sel.c};const gr=gw.getBoundingClientRect();X.popMenu(items.length?items:[['(the list is empty)','',()=>{}]],gr.left+Xc(m.c0),gr.top+Yr(m.r1+1))}
 /* ================= keyboard on the grid ================= */
 sc.addEventListener('keydown',e=>{if(edit)return;const k=e.key,c=e.ctrlKey||e.metaKey,sft=e.shiftKey;const mv=(dr,dc)=>{e.preventDefault();if(c){const base=sft?[sel.rs.at(-1).r0==sel.ar?sel.rs.at(-1).r1:sel.rs.at(-1).r0,sel.rs.at(-1).c0==sel.ac?sel.rs.at(-1).c1:sel.rs.at(-1).c0]:[sel.r,sel.c];const[r,cc]=jump(base[0],base[1],dr,dc);select(r,cc,sft);scrollInto(r,cc);return}
   if(sft){const gr=sel.rs.at(-1);const er=gr.r0==sel.ar?gr.r1:gr.r0,ec=gr.c0==sel.ac?gr.c1:gr.c0;let r=er+dr,cc=ec+dc;while(r>0&&r<SPR-1&&dr&&(sh.hid[r]||(sh._fh||{})[r]))r+=dr;select(r,cc,true);scrollInto(clampR(r),clampC(cc));return}
   const m=mergeAt(sel.r,sel.c);let r=(dr>0&&m?m.r1:sel.r)+dr,cc=(dc>0&&m?m.c1:sel.c)+dc;while(r>0&&r<SPR-1&&dr&&(sh.hid[r]||(sh._fh||{})[r]))r+=dr;while(cc>0&&cc<SPM-1&&dc&&sh.hc[cc])cc+=dc;select(r,cc);scrollInto(sel.r,sel.c)};
  if(chSel&&(k=='Delete'||k=='Backspace')){e.preventDefault();delChart(chSel);return}
  if(k=='ArrowUp')return mv(-1,0);if(k=='ArrowDown')return mv(1,0);if(k=='ArrowLeft')return mv(0,-1);if(k=='ArrowRight')return mv(0,1);
  if(k=='Enter'){e.preventDefault();if(e.altKey&&dvAt(sel.r,sel.c))return dvPick();return mv(sft?-1:1,0)}if(k=='Tab'){e.preventDefault();return mv(0,sft?-1:1)}
  if(k=='PageDown'||k=='PageUp'){e.preventDefault();if(e.altKey){const n=Math.max(1,Math.floor((VW-HW)/(DW*zoom)));select(sel.r,sel.c+(k=='PageDown'?n:-n),sft)}else{const n=Math.max(1,Math.floor((VH-HH)/(DH*zoom))-1);sc.scrollTop+=(k=='PageDown'?1:-1)*n*DH*zoom;select(sel.r+(k=='PageDown'?n:-n),sel.c,sft)}scrollInto(sel.r,sel.c);return}
  if(k=='Home'){e.preventDefault();if(c){select(0,0,sft);scrollInto(0,0)}else{select(sel.r,0,sft);scrollInto(sel.r,0)}return}
  if(k=='End'&&c){e.preventDefault();const u=spUsed(sh);select(Math.max(0,u.r1),Math.max(0,u.c1),sft);scrollInto(Math.max(0,u.r1),Math.max(0,u.c1));return}
  if(k=='F2'){e.preventDefault();startEdit(null,'edit');return}if(k=='F9'){e.preventDefault();book._dep=null;SPE.recalc(book,null);vers++;redraw();say('Recalculated');return}
  if(k=='Delete'||k=='Backspace'){e.preventDefault();if(k=='Backspace'){startEdit('','enter');return}clearSel('c');return}
  if(k=='Escape'){if(clip){clip=null;redraw()}if(painter){painter=null;updTb()}return}
  if(e.key==' '&&c&&!sft){e.preventDefault();sel.rs=sel.rs.map(gr=>({...gr,r0:0,r1:SPR-1}));afterSel();return}if(e.key==' '&&sft&&!c){e.preventDefault();sel.rs=sel.rs.map(gr=>({...gr,c0:0,c1:SPM-1}));afterSel();return}
  if(c&&!e.altKey){const kk=k.toLowerCase();const map={a:selAll,c:()=>copySel(false),x:()=>copySel(true),z:undo,y:redo,b:()=>tgl('b'),i:()=>tgl('i'),u:()=>tgl('u'),'5':()=>tgl('s'),s:()=>doSave(),o:openDlg,n:()=>sheetApp(),p:printDlg,f:()=>findDlg(false),h:()=>findDlg(true),d:()=>fillDir('down'),r:()=>fillDir('right'),g:gotoDlg,';':()=>stamp('date'),':':()=>stamp('time'),'1':fmtDlg,'`':()=>{showF=!showF;redraw()},home:()=>{}};
   if(kk=='v'||(kk=='v'&&sft))return;/* paste comes as an event */
   if(sft&&(kk=='+'||kk=='=')){e.preventDefault();return insDlg()}if(kk=='-'){e.preventDefault();return delDlg()}
   if(map[kk]){e.preventDefault();map[kk]();return}}
  if(e.altKey&&(k=='='||k=='+')){e.preventDefault();autoSum();return}
  if(k.length==1&&!c&&!e.altKey){e.preventDefault();startEdit(k,'enter');hintUpd()}});
 /* copy and paste through the system clipboard: tab-separated text and an HTML table (WordPro and PresentPro make a table of it) */
 function rowsOf(gr){const out=[];for(let r=gr.r0;r<=Math.min(gr.r1,gr.r0+20000);r++){if(sh.hid[r]||(sh._fh||{})[r])continue;const row=[];for(let c=gr.c0;c<=Math.min(gr.c1,gr.c0+300);c++){row.push(cellAt(r,c))}out.push(row)}return out}
 function rangeHTML(gr){const rows=rowsOf(gr);return`<table style='border-collapse:collapse'>${rows.map(row=>`<tr>${row.map(cl=>{const st=(cl&&cl.st)||{},v=spVal(cl);const css=[`border:1px solid #bfbfbf`,`padding:2px 6px`,st.b?'font-weight:bold':'',st.i?'font-style:italic':'',st.fc?`color:${st.fc}`:'',st.bg?`background:${st.bg}`:'',`text-align:${st.ha||(typeof v=='number'?'right':'left')}`].filter(Boolean).join(';');return`<td style='${css}'>${offEsc(spText(cl))}</td>`}).join('')}</tr>`).join('')}</table>`}
 function copySel(cut){if(sel.rs.length>1)return info('Copy','Copying several separate ranges is not possible. Select one range.');const gr=sel.rs[0];clip={sheet:sh,g:{...gr},cut,cells:rowsOf(gr).map(r=>r.map(c=>c&&{...c,ast:undefined})),text:null};
  const tsv=clip.cells.map(r=>r.map(spText).join('\t')).join('\n');clip.text=tsv;OFFCLIP={kind:'range',rows:clip.cells.map(r=>r.map(spText)),html:rangeHTML(gr),from:'SpreadsheetPro'};
  const h=e=>{e.preventDefault();e.clipboardData.setData('text/plain',tsv);e.clipboardData.setData('text/html',OFFCLIP.html)};document.addEventListener('copy',h,{once:true});const ok=document.execCommand('copy');document.removeEventListener('copy',h);
  if(!ok)try{navigator.clipboard.writeText(tsv)}catch(e){}clipAnim();say(cut?'Cut: choose where to paste':'Copied')}
 let clipT=0;function clipAnim(){clearInterval(clipT);clipT=setInterval(()=>{if(!clip||clip.done||!W.el.isConnected){clearInterval(clipT);return}redraw()},120)}
 document.addEventListener('paste',onPaste);const oc0=W.onclose;W.onclose=()=>{if(oc0&&oc0()===false)return false;document.removeEventListener('paste',onPaste);clearInterval(clipT)};
 function onPaste(e){if(!W.el.isConnected||document.activeElement!==sc||edit)return;e.preventDefault();const t=e.clipboardData.getData('text/plain');const html=e.clipboardData.getData('text/html');pasteText(t,html,'all')}
 function pasteText(t,html,what){if(clip&&!clip.done&&t!=null&&t.replace(/\r/g,'').replace(/\n$/,'')==clip.text)return pasteClip(what);
  let rows=null;if(html&&/<table/i.test(html)){try{const d=new DOMParser().parseFromString(html,'text/html');const tb=d.querySelector('table');if(tb)rows=[...tb.rows].map(r=>[...r.cells].map(c=>c.innerText!=null?c.textContent.trim():''))}catch(e){}}
  if(!rows){if(t==null)return;rows=spCSVRows(t.replace(/\r?\n$/,''),'\t')}if(!rows.length)return;begin('Paste');rows.forEach((row,i)=>row.forEach((v,j)=>{const r=sel.r+i,c=sel.c+j;if(r>=SPR||c>=SPM)return;const x=spParse(v);const old=cellAt(r,c);const st=old&&old.st?{...old.st}:{};if(x&&x.nf&&!st.nf)st.nf=x.nf;setCell(sh,r,c,x?{...(x.f?{f:x.f}:{v:x.v}),st:clean(st)}:{v:null,st:old&&old.st})}));
  sel.rs=[{r0:sel.r,c0:sel.c,r1:Math.min(SPR-1,sel.r+rows.length-1),c1:Math.min(SPM-1,sel.c+Math.max(...rows.map(r=>r.length))-1)}];commit()}
 function pasteClip(what,transpose){if(!clip)return;const src=clip,h=src.cells.length,w=src.cells[0].length;begin(what=='values'?'Paste values':what=='formats'?'Paste formats':'Paste');
  const tg=sel.rs[0];const reps=!transpose&&tg.r1-tg.r0+1>=h&&tg.c1-tg.c0+1>=w&&((tg.r1-tg.r0+1)%h==0)&&((tg.c1-tg.c0+1)%w==0)?[(tg.r1-tg.r0+1)/h,(tg.c1-tg.c0+1)/w]:[1,1];
  if(src.cut){for(let i=0;i<h;i++)for(let j=0;j<w;j++)setCell(src.sheet,src.g.r0+i,src.g.c0+j,null)}
  for(let a=0;a<reps[0];a++)for(let b=0;b<reps[1];b++)for(let i=0;i<h;i++)for(let j=0;j<w;j++){const cl=src.cells[i][j];const r=tg.r0+a*h+(transpose?j:i),c=tg.c0+b*w+(transpose?i:j);if(r>=SPR||c>=SPM)continue;const old=cellAt(r,c);
   const dr=r-(src.g.r0+i),dc=c-(src.g.c0+j);let cell;
   if(what=='formats')cell={...(old||{v:null}),ast:undefined,st:cl&&cl.st};
   else if(what=='values')cell={v:cl?spVal(cl):null,st:old&&old.st};if(what=='values'&&SPE.isE(cell.v))cell.v=cell.v.e;
   if(!cell){cell=!cl?{v:null}:cl.f!=null?{f:src.cut?cl.f:SPE.copyFormula(cl.f,dr,dc)}:{v:cl.v};if(what=='formulas')cell.st=old&&old.st;else cell.st=cl&&cl.st}
   setCell(sh,r,c,cell)}
  if(src.cut){/* formulas elsewhere that pointed to the cut cells now point to where they went */const dr=tg.r0-src.g.r0,dc=tg.c0-src.g.c0;for(const s of book.sheets)for(const[k,cl]of[...s.cells])if(cl.f!=null){const nf=SPE.shiftFormula(cl.f,(r,c,ar,ac,shn)=>{const ts=shn?book.sheets.find(x=>x.name.toUpperCase()==shn.toUpperCase()):s;return ts===src.sheet&&r>=src.g.r0&&r<=src.g.r1&&c>=src.g.c0&&c<=src.g.c1?[r+dr,c+dc]:[r,c]});if(nf!=cl.f){const[r,c]=spRC(k);setCell(s,r,c,{...cl,f:nf,ast:undefined})}}clip.done=true;clip=null}
  sel.rs=[{r0:tg.r0,c0:tg.c0,r1:Math.min(SPR-1,tg.r0+(transpose?w:h)*reps[0]-1),c1:Math.min(SPM-1,tg.c0+(transpose?h:w)*reps[1]-1)}];commit()}
 async function pasteMenu(what,transpose){if(clip&&!clip.done)return pasteClip(what,transpose);try{const t=await navigator.clipboard.readText();pasteText(t,null,what)}catch(e){info('Paste','Use Ctrl+V to paste from another app.')}}
 /* ================= the mouse ================= */
 let drag=null,fillDrag=null,rz=null;
 const pos=e=>{const r=sc.getBoundingClientRect();return[e.clientX-r.left,e.clientY-r.top]};
 function edgeAt(x,y){/* a column or row border in the headings, for resizing */if(y<HH&&x>HW){for(let d=-4;d<=4;d++){const c=colAtX(x+d);if(Math.abs(Xc(c+1)-x)<=4)return{col:c};if(Math.abs(Xc(c)-x)<=4&&c>0)return{col:c-1}}}if(x<HW&&y>HH){const r=rowAtY(y);if(Math.abs(Yr(r+1)-y)<=3)return{row:r};if(Math.abs(Yr(r)-y)<=3&&r>0)return{row:r-1}}return null}
 function overFill(x,y){const gr=sel.rs.at(-1);const fx_=Xc(gr.c1+1),fy=Yr(gr.r1+1);return Math.abs(x-fx_)<=4&&Math.abs(y-fy)<=4}
 sc.addEventListener('mousemove',e=>{const[x,y]=pos(e);if(drag||fillDrag||rz)return;const ed_=edgeAt(x,y);sc.style.cursor=ed_?(ed_.col!=null?'col-resize':'row-resize'):overFill(x,y)?'crosshair':x>HW&&y>HH?'cell':'default';
  /* comments show when the mouse rests on the cell */clearTimeout(tipT);tip.style.display='none';if(x>HW&&y>HH){const r=rowAtY(y),c=colAtX(x),t=sh.cm[spK(r,c)];if(t)tipT=setTimeout(()=>{tip.textContent=t;tip.style.left=Xc(c+1)+4+'px';tip.style.top=Yr(r)+'px';tip.style.display='block'},350)}});
 let tipT=0;sc.addEventListener('mouseleave',()=>{clearTimeout(tipT);tip.style.display='none'});
 sc.addEventListener('mousedown',e=>{const[x,y]=pos(e);if(x>sc.clientWidth||y>sc.clientHeight)return;X.closeMenu();if(chSel){chSel=null;placeCharts()}
  if(e.button==2){if(x>HW&&y>HH&&!inSel(rowAtY(y),colAtX(x)))select(rowAtY(y),colAtX(x));return}
  const E=edgeAt(x,y);if(E){e.preventDefault();rz={...E,x,y,orig:E.col!=null?(sh.cw[E.col]??DW):(sh.rh[E.row]??DH)};return}
  if(edit&&edit.src=='cell'&&refWanted()&&x>HW&&y>HH){e.preventDefault();const r=rowAtY(y),c=colAtX(x);drag={pt:true,r,c};putRef({r0:r,r1:r,c0:c,c1:c},sh.name);return}
  if(edit&&edit.src=='bar'&&refWanted()&&x>HW&&y>HH){e.preventDefault();const r=rowAtY(y),c=colAtX(x);drag={pt:true,r,c};putRef({r0:r,r1:r,c0:c,c1:c},sh.name);return}
  if(edit&&!commitEdit(0,0))return;
  if(overFill(x,y)){e.preventDefault();fillDrag={from:{...sel.rs.at(-1)},to:null};return}
  /* the AutoFilter buttons and list arrows */
  const F=sh.filter;if(F&&y>HH&&x>HW){const r=rowAtY(y),c=colAtX(x);if(r==F.r0&&c>=F.c0&&c<=F.c1&&x>=Xc(c+1)-18){e.preventDefault();filterPop(c);return}}
  if(x>HW&&y>HH){const d=dvAt(sel.r,sel.c);if(d&&d.t=='list'){const m=mergeAt(sel.r,sel.c)||{r0:sel.r,r1:sel.r,c1:sel.c};if(x>Xc(m.c1+1)&&x<Xc(m.c1+1)+17&&y>Yr(m.r0)&&y<Yr(m.r1+1)){e.preventDefault();dvPick();return}}}
  e.preventDefault();sc.focus({preventScroll:true});
  if(x<HW&&y<HH){selAll();return}
  if(y<HH){const c=colAtX(x);if(e.shiftKey){sel.rs[sel.rs.length-1]={r0:0,r1:SPR-1,c0:Math.min(sel.ac,c),c1:Math.max(sel.ac,c)}}else{const gg={r0:0,r1:SPR-1,c0:c,c1:c};if(e.ctrlKey)sel.rs.push(gg);else sel.rs=[gg];sel.r=Math.max(0,rowAtY(HH+FY()+1));sel.c=c;sel.ar=0;sel.ac=c}drag={col:c};afterSel();return}
  if(x<HW){const r=rowAtY(y);if(e.shiftKey){sel.rs[sel.rs.length-1]={r0:Math.min(sel.ar,r),r1:Math.max(sel.ar,r),c0:0,c1:SPM-1}}else{const gg={r0:r,r1:r,c0:0,c1:SPM-1};if(e.ctrlKey)sel.rs.push(gg);else sel.rs=[gg];sel.r=r;sel.c=Math.max(0,colAtX(HW+FX()+1));sel.ar=r;sel.ac=0}drag={row:r};afterSel();return}
  tabStart=null;const r=rowAtY(y),c=colAtX(x);if(e.shiftKey)select(r,c,true);else select(r,c,false,e.ctrlKey);drag={cell:true};
  if(painter){applyPainter()}});
 const inSel=(r,c)=>sel.rs.some(gr=>r>=gr.r0&&r<=gr.r1&&c>=gr.c0&&c<=gr.c1);
 let autoT=0;
 document.addEventListener('mousemove',mm);document.addEventListener('mouseup',mu);const oc1=W.onclose;W.onclose=()=>{if(oc1&&oc1()===false)return false;document.removeEventListener('mousemove',mm);document.removeEventListener('mouseup',mu)};
 function mm(e){if(!drag&&!fillDrag&&!rz)return;const[x0,y0]=pos(e);const x=Math.max(HW+1,Math.min(VW-1,x0)),y=Math.max(HH+1,Math.min(VH-1,y0));
  /* scroll while dragging past the edge */clearInterval(autoT);if(x0>VW||y0>VH||x0<HW||y0<HH){autoT=setInterval(()=>{sc.scrollLeft+=x0>VW?20:x0<HW?-20:0;sc.scrollTop+=y0>VH?20:y0<HH?-20:0;mm(e)},60)}
  if(rz){if(rz.col!=null){sh.cw[rz.col]=Math.max(0,Math.round(rz.orig+(x0-rz.x)/zoom))}else{sh.rh[rz.row]=Math.max(0,Math.round(rz.orig+(y0-rz.y)/zoom))}regeom();sizeSpacer();redraw();return}
  const r=rowAtY(y),c=colAtX(x);
  if(fillDrag){const f=fillDrag.from;const dr=r>f.r1?r-f.r1:r<f.r0?r-f.r0:0,dc=c>f.c1?c-f.c1:c<f.c0?c-f.c0:0;if(!dr&&!dc){fillDrag.to=null}else if(Math.abs(dr)>=Math.abs(dc)){fillDrag.to=dr>0?{...f,r1:r}:{...f,r0:r};fillDrag.dir=dr>0?'down':'up'}else{fillDrag.to=dc>0?{...f,c1:c}:{...f,c0:c};fillDrag.dir=dc>0?'right':'left'}redraw();return}
  if(drag.pt){putRef(norm(drag.r,drag.c,r,c),sh.name);return}
  if(drag.col!=null){sel.rs[sel.rs.length-1]={r0:0,r1:SPR-1,c0:Math.min(drag.col,c),c1:Math.max(drag.col,c)};afterSel();return}
  if(drag.row!=null){sel.rs[sel.rs.length-1]={r0:Math.min(drag.row,r),r1:Math.max(drag.row,r),c0:0,c1:SPM-1};afterSel();return}
  sel.rs[sel.rs.length-1]=grow(norm(sel.ar,sel.ac,r,c));afterSel()}
 function mu(){clearInterval(autoT);if(rz){const z=rz;rz=null;const k=z.col!=null?'cw':'rh',i=z.col!=null?z.col:z.row,nv=sh[k][i];sh[k][i]=z.orig;begin('Resize');snap(sh);
   /* every selected column (or row) gets the same size */const tgt=sel.rs.some(gr=>z.col!=null?gr.r0==0&&gr.r1==SPR-1&&i>=gr.c0&&i<=gr.c1:gr.c0==0&&gr.c1==SPM-1&&i>=gr.r0&&i<=gr.r1)?sel.rs.flatMap(gr=>{const a=[];for(let j=z.col!=null?gr.c0:gr.r0;j<=(z.col!=null?gr.c1:gr.r1)&&a.length<1000;j++)a.push(j);return a}):[i];tgt.forEach(j=>{sh[k][j]=nv});commit();return}
  if(fillDrag){const f=fillDrag;fillDrag=null;if(f.to)doFill(f.from,f.to,f.dir);redraw();return}drag=null}
 sc.addEventListener('dblclick',e=>{const[x,y]=pos(e);const E=edgeAt(x,y);if(E){autofit(E.col!=null?'c':'r',E.col!=null?E.col:E.row);return}if(overFill(x,y)){autoFillDown();return}if(x>HW&&y>HH){if(sh.cm[spK(sel.r,sel.c)]&&e.altKey)return commentDlg();startEdit(null,'edit')}});
 sc.addEventListener('contextmenu',e=>{e.preventDefault();const[x,y]=pos(e);
  if(y<HH&&x>HW)return X.popMenu([['Cut','Ctrl+X',()=>copySel(true)],['Copy','Ctrl+C',()=>copySel(false)],['Paste','Ctrl+V',()=>pasteMenu('all')],'-',['Insert columns','',()=>insRC('c',1)],['Delete columns','',()=>insRC('c',-1)],['Clear contents','',()=>clearSel('c')],'-',['Column width…','',()=>sizeDlg('c')],['Autofit width','',()=>autofit('c')],['Hide','',()=>hideRC('c',true)],['Unhide','',()=>hideRC('c',false)]],e.clientX,e.clientY);
  if(x<HW&&y>HH)return X.popMenu([['Cut','Ctrl+X',()=>copySel(true)],['Copy','Ctrl+C',()=>copySel(false)],['Paste','Ctrl+V',()=>pasteMenu('all')],'-',['Insert rows','',()=>insRC('r',1)],['Delete rows','',()=>insRC('r',-1)],['Clear contents','',()=>clearSel('c')],'-',['Row height…','',()=>sizeDlg('r')],['Autofit height','',()=>autofit('r')],['Hide','',()=>hideRC('r',true)],['Unhide','',()=>hideRC('r',false)]],e.clientX,e.clientY);
  const k=spK(sel.r,sel.c);X.popMenu([['Cut','Ctrl+X',()=>copySel(true)],['Copy','Ctrl+C',()=>copySel(false)],['Paste','Ctrl+V',()=>pasteMenu('all')],['Paste values','',()=>pasteMenu('values')],['Paste formats','',()=>pasteMenu('formats')],'-',['Insert…','Ctrl+Shift++',insDlg],['Delete…','Ctrl+-',delDlg],['Clear contents','Delete',()=>clearSel('c')],'-',['Sort A to Z','',()=>quickSort(1)],['Sort Z to A','',()=>quickSort(-1)],['Filter by this value','',filterByCell],'-',[sh.cm[k]?'Edit comment…':'Insert comment…','',commentDlg],sh.cm[k]?['Delete comment','',()=>{begin('Comment');snap(sh);delete sh.cm[k];commit()}]:null,'-',['Format cells…','Ctrl+1',fmtDlg],['Define name…','',nameDlg],['Insert chart…','',chartDlg]],e.clientX,e.clientY)});
 sc.addEventListener('scroll',()=>{if(sc.scrollTop+sc.clientHeight>sp.offsetHeight-400)sizeSpacer();redraw();if(edit)placeEditor()});
 new ResizeObserver(()=>resize()).observe(gw);
 /* ================= operations ================= */
 function selAll(){sel.rs=[{r0:0,c0:0,r1:SPR-1,c1:SPM-1}];afterSel()}
 function tgl(k){const on=!cur()[k];styleSel(k=='b'?'Bold':k=='i'?'Italic':k=='u'?'Underline':'Strikethrough',st=>({...st,[k]:on?1:undefined}))}
 function setSt(label,o){styleSel(label,st=>({...st,...o}))}
 function clearSel(what){begin(what=='c'?'Clear contents':what=='f'?'Clear formats':'Clear');for(const gr of sel.rs){eachIn(sh,gr,(cl,r,c)=>{if(what=='c'){if(cl.st)setCell(sh,r,c,{v:null,st:cl.st});else setCell(sh,r,c,null)}else if(what=='f')setCell(sh,r,c,{...cl,ast:undefined,st:undefined});else setCell(sh,r,c,null)});
  if(what!='c'){snap(sh);sh.cf=sh.cf.filter(R=>!(R.r0>=gr.r0&&R.r1<=gr.r1&&R.c0>=gr.c0&&R.c1<=gr.c1));if(what=='a')for(const k of Object.keys(sh.cm)){const[r,c]=spRC(+k);if(r>=gr.r0&&r<=gr.r1&&c>=gr.c0&&c<=gr.c1)delete sh.cm[k]}}}commit()}
 function stamp(kind){const n=new Date();const v=kind=='date'?SPE.toSerial(n.getFullYear(),n.getMonth()+1,n.getDate()):(n.getHours()*3600+n.getMinutes()*60+n.getSeconds())/86400;begin('Insert '+kind);const old=cellAt(sel.r,sel.c);setCell(sh,sel.r,sel.c,{v,st:clean({...(old&&old.st),nf:kind=='date'?'yyyy-mm-dd':'h:mm AM/PM'})});commit()}
 function autoSum(){/* Σ: a SUM below (or right of) the selection, or of the numbers above the active cell */const gr=sel.rs[0];begin('AutoSum');
  if(gr.r0==gr.r1&&gr.c0==gr.c1){let r=sel.r-1;while(r>=0&&typeof spVal(cellAt(r,sel.c))!='number')r--;let r0=r;while(r0>0&&typeof spVal(cellAt(r0-1,sel.c))=='number')r0--;if(r<0){let c=sel.c-1;while(c>=0&&typeof spVal(cellAt(sel.r,c))!='number')c--;let c0=c;while(c0>0&&typeof spVal(cellAt(sel.r,c0-1))=='number')c0--;tx=null;if(c<0)return startEdit('=SUM()','edit');return startEdit(`=SUM(${spRangeStr({r0:sel.r,r1:sel.r,c0,c1:c})})`,'edit')}tx=null;return startEdit(`=SUM(${spRangeStr({r0,r1:r,c0:sel.c,c1:sel.c})})`,'edit')}
  const rowsMode=gr.r1>gr.r0||gr.c0==gr.c1;if(rowsMode){for(let c=gr.c0;c<=gr.c1;c++)setCell(sh,gr.r1+1,c,{f:`=SUM(${spRangeStr({r0:gr.r0,r1:gr.r1,c0:c,c1:c})})`,st:(cellAt(gr.r1,c)||{}).st})}else{for(let r=gr.r0;r<=gr.r1;r++)setCell(sh,r,gr.c1+1,{f:`=SUM(${spRangeStr({r0:r,r1:r,c0:gr.c0,c1:gr.c1})})`,st:(cellAt(r,gr.c1)||{}).st})}commit()}
 /* format painter: one click copies the active cell's format to the next selection */
 function applyPainter(){const p=painter;painter=null;updTb();setTimeout(()=>{styleSel('Format painter',()=>p?{...p}:{})},0)}
 /* ---- fill: continue a series, or copy ---- */
 function seriesOf(vals,n,back){/* vals: the source values in order; n new values after (or before, back) them */
  const nums=vals.every(v=>typeof v=='number');if(nums&&vals.length>=2){const k=vals.length,xs=vals.map((_,i)=>i),mx=(k-1)/2,my=vals.reduce((a,b)=>a+b,0)/k;let sxy=0,sxx=0;xs.forEach((x,i)=>{sxy+=(x-mx)*(vals[i]-my);sxx+=(x-mx)**2});const sl=sxy/sxx,b0=my-sl*mx;
   return Array.from({length:n},(_,i)=>{const x=back?-1-i:k+i;return+(b0+sl*x).toPrecision(14)})}
  const strs=vals.every(v=>typeof v=='string');if(strs){for(const L of SP_SERIES){const idx=vals.map(v=>{const u=v.toLowerCase();let i=L.findIndex(x=>x.toLowerCase()==u);if(i>=0)return[i,'f'];i=L.findIndex(x=>x.slice(0,3).toLowerCase()==u);return i>=0?[i,'s']:null});
    if(idx.every(Boolean)){const step=idx.length>1?((idx[1][0]-idx[0][0])+L.length)%L.length||1:1;const last=back?idx[0][0]:idx.at(-1)[0];const sh_=idx[0][1],cas=vals[0];return Array.from({length:n},(_,i)=>{let t=L[(((last+(back?-1:1)*step*(i+1))%L.length)+L.length)%L.length];if(sh_=='s')t=t.slice(0,3);return cas==cas.toUpperCase()?t.toUpperCase():cas==cas.toLowerCase()?t.toLowerCase():t})}}
   const m=vals.map(v=>/^(.*?)(\d+)(\D*)$/.exec(v));if(m.every(x=>x&&x[1]==m[0][1]&&x[3]==m[0][3])){const ns=m.map(x=>+x[2]);const step=ns.length>1?ns[1]-ns[0]:1;const last=back?ns[0]:ns.at(-1);const pad=m[0][2].length;return Array.from({length:n},(_,i)=>{const v=last+(back?-1:1)*step*(i+1);return m[0][1]+(v<0?'-':'')+String(Math.abs(v)).padStart(m[0][2][0]=='0'?pad:1,'0')+m[0][3]})}}
  return null}
 function doFill(from,to,dir){begin('Fill');const vert=dir=='down'||dir=='up',back=dir=='up'||dir=='left';const len=vert?from.r1-from.r0+1:from.c1-from.c0+1,n=vert?to.r1-to.r0+1-len:to.c1-to.c0+1-len;if(n<=0){tx=null;return}
  const lanes=vert?[from.c0,from.c1]:[from.r0,from.r1];
  for(let L=lanes[0];L<=lanes[1];L++){const src=[];for(let i=0;i<len;i++){const r=vert?from.r0+i:L,c=vert?L:from.c0+i;src.push([r,c,cellAt(r,c)])}
   const plain=src.filter(x=>x[2]&&x[2].f==null&&x[2].v!=null).map(x=>x[2].v);const allPlain=plain.length==len;
   let ser=null;if(allPlain){const v=plain;const date=v.length==1&&typeof v[0]=='number'&&src[0][2].st&&/[yd]/.test(src[0][2].st.nf||'');ser=date?Array.from({length:n},(_,i)=>v[0]+(back?-1:1)*(i+1)):v.length==1&&typeof v[0]=='number'?null:seriesOf(v,n,back)}
   for(let i=0;i<n;i++){const s_=src[back?(len-1-(i%len)):(i%len)],cl=s_[2];const r=vert?(back?from.r0-1-i:from.r1+1+i):L,c=vert?L:(back?from.c0-1-i:from.c1+1+i);if(r<0||c<0||r>=SPR||c>=SPM)continue;
    let cell;if(ser)cell={v:ser[i],st:cl&&cl.st};else if(!cl)cell=null;else if(cl.f!=null)cell={f:SPE.copyFormula(cl.f,r-s_[0],c-s_[1]),st:cl.st};else cell={v:cl.v,st:cl.st};setCell(sh,r,c,cell)}}
  sel.rs=[{...to}];commit()}
 function fillDir(d){const gr=sel.rs[0];if(d=='down'){if(gr.r0==gr.r1){if(gr.r0==0)return;doFill({...gr,r0:gr.r0-1,r1:gr.r0-1},{...gr,r0:gr.r0-1},'down')}else{begin('Fill down');for(let r=gr.r0+1;r<=gr.r1;r++)for(let c=gr.c0;c<=gr.c1;c++){const cl=cellAt(gr.r0,c);setCell(sh,r,c,!cl?null:cl.f!=null?{f:SPE.copyFormula(cl.f,r-gr.r0,0),st:cl.st}:{v:cl.v,st:cl.st})}commit()}}
  else{if(gr.c0==gr.c1){if(gr.c0==0)return;doFill({...gr,c0:gr.c0-1,c1:gr.c0-1},{...gr,c0:gr.c0-1},'right')}else{begin('Fill right');for(let c=gr.c0+1;c<=gr.c1;c++)for(let r=gr.r0;r<=gr.r1;r++){const cl=cellAt(r,gr.c0);setCell(sh,r,c,!cl?null:cl.f!=null?{f:SPE.copyFormula(cl.f,0,c-gr.c0),st:cl.st}:{v:cl.v,st:cl.st})}commit()}}}
 function autoFillDown(){/* double-clicking the fill handle: down as far as the column to the left has data */const gr=sel.rs.at(-1);const nc=gr.c0>0?gr.c0-1:gr.c1+1;let r=gr.r1;while(r+1<SPR&&cellAt(r+1,nc)&&spText(cellAt(r+1,nc))!=='')r++;if(r>gr.r1)doFill(gr,{...gr,r1:r},'down')}
 /* ---- rows, columns and cells: insert and delete, with formulas adjusted ---- */
 function shiftAll(s,axis,at,n,lo,hi){/* n>0: n new rows (or columns) at `at`; n<0: -n removed from `at`. lo..hi: the columns (rows) affected */
  const inBand=(r,c)=>axis=='r'?c>=lo&&c<=hi:r>=lo&&r<=hi;const MX=axis=='r'?SPR:SPM;
  const mv=(r,c)=>{const p=axis=='r'?r:c;if(!inBand(r,c))return[r,c];if(n>0){if(p>=at){const q=p+n;if(q>=MX)return null;return axis=='r'?[q,c]:[r,q]}return[r,c]}if(p>=at&&p<at-n)return null;if(p>=at-n)return axis=='r'?[p+n,c]:[r,p+n];return[r,c]};
  const nc=new Map();for(const[k,cl]of s.cells){const[r,c]=spRC(k);const p=mv(r,c);if(p)nc.set(spK(p[0],p[1]),cl)}s.cells=nc;
  const whole=lo==0&&hi==(axis=='r'?SPM-1:SPR-1);
  if(whole){const map=axis=='r'?['rh','hid']:['cw','hc'];for(const m of map){const o={};for(const[k,v]of Object.entries(s[m])){const p=+k;if(n>0)o[p>=at?p+n:p]=v;else if(p<at)o[p]=v;else if(p>=at-n)o[p+n]=v}s[m]=o}}
  const mvRange=R=>{const a=mv(R.r0,R.c0),b=mv(R.r1,R.c1);if(n<0&&(!a||!b)){/* a range that loses rows shrinks */const p0=axis=='r'?R.r0:R.c0,p1=axis=='r'?R.r1:R.c1;const np0=p0>=at-n?p0+n:p0>=at?at:p0,np1=p1>=at-n?p1+n:p1>=at?at-1:p1;if(np1<np0)return null;return axis=='r'?{...R,r0:np0,r1:np1}:{...R,c0:np0,c1:np1}}if(!a||!b)return null;
   /* inserting inside a range makes it larger */if(n>0){const p0=axis=='r'?R.r0:R.c0,p1=axis=='r'?R.r1:R.c1;if(at>p0&&at<=p1)return axis=='r'?{...R,r1:R.r1+n}:{...R,c1:R.c1+n}}return{...R,r0:a[0],c0:a[1],r1:b[0],c1:b[1]}};
  s.merges=s.merges.map(mvRange).filter(Boolean);s.cf=s.cf.map(mvRange).filter(Boolean);s.dv=s.dv.map(mvRange).filter(Boolean);if(s.filter)s.filter=mvRange(s.filter)||null;
  const cm={};for(const[k,t]of Object.entries(s.cm)){const[r,c]=spRC(+k);const p=mv(r,c);if(p)cm[spK(p[0],p[1])]=t}s.cm=cm;
  for(const ch of s.charts){const R=spParseRange(ch.rg);if(R){const nR=mvRange(R);if(nR)ch.rg=spRangeStr(nR)}}
  /* formulas everywhere that point into the sheet */
  const fixF=(f,home)=>SPE.shiftFormula(f,(r,c,ar,ac,shn)=>{const ts=shn?book.sheets.find(x=>x.name.toUpperCase()==shn.toUpperCase()):home;if(ts!==s)return[r,c];return mv(r,c)});
  for(const t of book.sheets){if(t!==s)snap(t);for(const[k,cl]of t.cells)if(cl.f!=null){const nf=fixF(cl.f,t);if(nf!=cl.f)t.cells.set(k,{...cl,f:nf,ast:undefined})}}
  for(const v of Object.values(book.names)){const home=book.sheets.find(x=>x.name==v.sheet)||book.sheets[0];v.ref=fixF('='+v.ref,home).slice(1)}
  s.fk=null}
 function insRC(axis,dir){const b=selBounds();const at=axis=='r'?b.r0:b.c0,cnt=axis=='r'?b.r1-b.r0+1:b.c1-b.c0+1;if(cnt>5000)return info('Too many','Select fewer than 5,000 rows or columns.');begin(dir>0?'Insert':'Delete');snapBook();snap(sh);shiftAll(sh,axis,at,dir>0?cnt:-cnt,0,axis=='r'?SPM-1:SPR-1);commit()}
 function insCells(down,dir){const b=selBounds();begin(dir>0?'Insert cells':'Delete cells');snapBook();snap(sh);if(down)shiftAll(sh,'r',b.r0,dir*(b.r1-b.r0+1),b.c0,b.c1);else shiftAll(sh,'c',b.c0,dir*(b.c1-b.c0+1),b.r0,b.r1);commit()}
 function insDlg(){const b=selBounds();if(b.r0==0&&b.r1==SPR-1)return insRC('c',1);if(b.c0==0&&b.c1==SPM-1)return insRC('r',1);dialog('Insert',`<label><input type='radio' name='i' value='r' checked> Shift cells right</label><br><label><input type='radio' name='i' value='d'> Shift cells down</label><br><label><input type='radio' name='i' value='R'> Entire row</label><br><label><input type='radio' name='i' value='C'> Entire column</label>`,[['Cancel'],['OK',d=>{const v=d.querySelector('input:checked').value;if(v=='R')insRC('r',1);else if(v=='C')insRC('c',1);else insCells(v=='d',1)},1]])}
 function delDlg(){const b=selBounds();if(b.r0==0&&b.r1==SPR-1)return insRC('c',-1);if(b.c0==0&&b.c1==SPM-1)return insRC('r',-1);dialog('Delete',`<label><input type='radio' name='i' value='l' checked> Shift cells left</label><br><label><input type='radio' name='i' value='u'> Shift cells up</label><br><label><input type='radio' name='i' value='R'> Entire row</label><br><label><input type='radio' name='i' value='C'> Entire column</label>`,[['Cancel'],['OK',d=>{const v=d.querySelector('input:checked').value;if(v=='R')insRC('r',-1);else if(v=='C')insRC('c',-1);else insCells(v=='u',-1)},1]])}
 function hideRC(axis,hide){const b=selBounds();begin(hide?'Hide':'Unhide');snap(sh);const m=axis=='r'?sh.hid:sh.hc;const a=axis=='r'?b.r0:b.c0,z=Math.min(axis=='r'?b.r1:b.c1,a+(hide?100000:100000));if(hide)for(let i=a;i<=z;i++)m[i]=1;else for(const k of Object.keys(m))if(+k>=a-1&&+k<=z+1)delete m[k];commit()}
 function autofit(axis,one){const b=selBounds();const list=one!=null?[one]:[];if(one==null){const a=axis=='c'?b.c0:b.r0,z=axis=='c'?b.c1:b.r1;for(let i=a;i<=Math.min(z,a+300);i++)list.push(i)}begin('Autofit');snap(sh);
  for(const i of list){if(axis=='c'){let w=0;for(const[k,cl]of sh.cells)if(k%SPM==i){const t=showF&&cl.f!=null?cl.f:spText(cl);if(t){mctx.font=fontOf(cl.st);w=Math.max(w,mctx.measureText(t).width/zoom)}}sh.cw[i]=w?Math.min(600,Math.ceil(w+10)):DW}
   else{let h=0;for(let c=0;c<SPM;c++){const cl=cellAt(i,c);if(!cl)continue;const sz=((cl.st&&cl.st.sz)||11)*4/3*1.25;let lines=1;if(cl.st&&cl.st.wrap){const w=(sh.cw[c]??DW)-6;mctx.font=fontOf({...cl.st});lines=Math.ceil(mctx.measureText(spText(cl)).width/zoom/Math.max(10,w)+.01)}h=Math.max(h,sz*lines+4)}if(h&&Math.round(h)!=DH)sh.rh[i]=Math.round(Math.max(h,14));else delete sh.rh[i]}}commit()}
 function sizeDlg(axis){const b=selBounds();const cur_=axis=='c'?(sh.cw[b.c0]??DW):(sh.rh[b.r0]??DH);dialog(axis=='c'?'Column width':'Row height',`<label>${axis=='c'?'Width':'Height'} in pixels <input class='v' type='number' min='0' max='1000' value='${cur_}' style='width:80px'></label>`,[['Cancel'],['OK',d=>{const v=Math.max(0,Math.min(1000,+d.querySelector('.v').value||0));begin('Size');snap(sh);const a=axis=='c'?b.c0:b.r0,z=Math.min(axis=='c'?b.c1:b.r1,a+5000);for(let i=a;i<=z;i++)(axis=='c'?sh.cw:sh.rh)[i]=v;commit()},1]])}
 /* ---- merging ---- */
 function mergeSel(center){const gr=sel.rs[0];const ex=sh.merges.filter(m=>m.r1>=gr.r0&&m.r0<=gr.r1&&m.c1>=gr.c0&&m.c0<=gr.c1);begin('Merge');snap(sh);
  if(ex.length&&ex.some(m=>m.r0==gr.r0&&m.c0==gr.c0&&m.r1==gr.r1&&m.c1==gr.c1)){sh.merges=sh.merges.filter(m=>!ex.includes(m));commit();return}
  if(gr.r0==gr.r1&&gr.c0==gr.c1){tx=null;return}let lost=0;eachIn(sh,gr,(cl,r,c)=>{if((r!=gr.r0||c!=gr.c0)&&spText(cl)!=='')lost++});
  const go=()=>{sh.merges=sh.merges.filter(m=>!ex.includes(m));sh.merges.push({...gr});eachIn(sh,gr,(cl,r,c)=>{if(r!=gr.r0||c!=gr.c0)sh.cells.delete(spK(r,c))});if(center){const cl=cellAt(gr.r0,gr.c0)||{v:null};sh.cells.set(spK(gr.r0,gr.c0),{...cl,ast:undefined,st:{...(cl.st||{}),ha:'center',va:'middle'}})}sel.r=gr.r0;sel.c=gr.c0;commit()};
  if(lost){const t=tx;tx=null;X.confirm('Merge cells','Merging keeps only the upper-left value. The other values are discarded.','Merge').then(y=>{if(y){tx=t;go()}})}else go()}
 /* ---- sorting and filtering ---- */
 function region(){/* the block of filled cells around the active cell (Excel's "current region") */const gr=sel.rs[0];if(gr.r0!=gr.r1||gr.c0!=gr.c1)return{...gr,r1:Math.min(gr.r1,Math.max(gr.r0,spUsed(sh).r1)),c1:Math.min(gr.c1,Math.max(gr.c0,spUsed(sh).c1))};
  let R={r0:sel.r,r1:sel.r,c0:sel.c,c1:sel.c};const has=(r,c)=>r>=0&&c>=0&&cellAt(r,c)&&spText(cellAt(r,c))!=='';let grew=true;while(grew){grew=false;
   for(const[dr,dc,k]of[[-1,0,'r0'],[1,0,'r1'],[0,-1,'c0'],[0,1,'c1']]){const line=k[0]=='r'?R[k]+dr:R[k]+dc;let any=false;if(k[0]=='r'){for(let c=R.c0-1;c<=R.c1+1;c++)if(has(line,c)){any=true;break}}else{for(let r=R.r0-1;r<=R.r1+1;r++)if(has(r,line)){any=true;break}}if(any&&line>=0){R[k]=line;grew=true}}}return R}
 const looksHeader=R=>{if(R.r1==R.r0)return false;let txt=0,num=0;for(let c=R.c0;c<=R.c1;c++){const a=spVal(cellAt(R.r0,c)),b=spVal(cellAt(R.r0+1,c));if(typeof a=='string')txt++;if(typeof b=='number')num++}return txt==R.c1-R.c0+1||(txt>0&&num>0)||(cellAt(R.r0,R.c0)||{}).st&&(cellAt(R.r0,R.c0).st.b)};
 function sortRange(R,keys,hdr){begin('Sort');snap(sh);const r0=R.r0+(hdr?1:0);const rows=[];for(let r=r0;r<=R.r1;r++){const row=[];for(let c=R.c0;c<=R.c1;c++)row.push(cellAt(r,c));rows.push({r,row})}
  rows.sort((a,b)=>{for(const[c,dir]of keys){const x=spVal(a.row[c-R.c0]),y=spVal(b.row[c-R.c0]);const ex=x==null||x==='',ey=y==null||y==='';if(ex&&ey)continue;if(ex)return 1;if(ey)return-1;const d=SPE.cmpVals(x,y);if(d)return d*dir}return a.r-b.r});
  rows.forEach((o,i)=>{const r=r0+i;o.row.forEach((cl,j)=>{const c=R.c0+j;sh.cells.delete(spK(r,c));if(cl)sh.cells.set(spK(r,c),cl.f!=null?{...cl,f:SPE.copyFormula(cl.f,r-o.r,0),ast:undefined}:cl)})});sh.fk=null;commit()}
 function quickSort(dir){const R=region();if(R.r1<=R.r0&&!spText(cellAt(R.r0,R.c0)))return;sortRange(R,[[Math.max(R.c0,Math.min(R.c1,sel.c)),dir]],looksHeader(R));say('Sorted '+spRangeStr(R))}
 function sortDlg(){const R=region();const hdr=looksHeader(R);const cols=[];for(let c=R.c0;c<=R.c1;c++)cols.push([c,hdr?spText(cellAt(R.r0,c))||'Column '+SPE.colName(c):'Column '+SPE.colName(c)]);const opt=cols.map(([c,n])=>`<option value='${c}'>${offEsc(n)}</option>`).join('');
  const row=(i,on)=>`<div style='margin:4px 0'>${i?'Then by':'Sort by'} <select class='k${i}'>${i?"<option value=''>(none)</option>":''}${opt}</select> <select class='d${i}'><option value='1'>A to Z, smallest first</option><option value='-1'>Z to A, largest first</option></select></div>`;
  dialog('Sort '+spRangeStr(R),row(0)+row(1)+row(2)+`<label><input type='checkbox' class='hd' ${hdr?'checked':''}> My data has headings</label>`,[['Cancel'],['Sort',d=>{const keys=[];for(let i=0;i<3;i++){const v=d.querySelector('.k'+i).value;if(v!=='')keys.push([+v,+d.querySelector('.d'+i).value])}sortRange(R,keys,d.querySelector('.hd').checked)},1]],d=>{d.querySelector('.k0').value=Math.max(R.c0,Math.min(R.c1,sel.c))})}
 function toggleFilter(){begin('AutoFilter');snap(sh);if(sh.filter){sh.filter=null;sh._fh={};regeom()}else{const R=region();sh.filter={...R,cr:{}}}commit()}
 function applyFilter(redo){const F=sh.filter;sh._fh={};if(F){for(let r=F.r0+1;r<=F.r1;r++){for(const[c,allowed]of Object.entries(F.cr||{})){if(!allowed)continue;const t=spText(cellAt(r,+c));if(!allowed.includes(t)){sh._fh[r]=1;break}}}}regeom();if(redo!==false){sizeSpacer();redraw()}}
 function filterPop(c){const F=sh.filter;const vals=new Set();for(let r=F.r0+1;r<=F.r1;r++)vals.add(spText(cellAt(r,c)));const list=[...vals].sort((a,b)=>SPE.cmpVals(isNaN(+a)||a===''?a:+a,isNaN(+b)||b===''?b:+b));const on=F.cr&&F.cr[c];
  dialog('Filter: '+(spText(cellAt(F.r0,c))||SPE.colName(c)),`<p><button class='sa'>Sort A to Z</button> <button class='sz'>Sort Z to A</button></p><input class='fs' placeholder='Search' style='width:100%'><div class='sp-fl'>${list.map((v,i)=>`<label><input type='checkbox' data-i='${i}' ${!on||on.includes(v)?'checked':''}> ${v===''?'(Blanks)':offEsc(v)}</label>`).join('')}</div><p><button class='all'>Select all</button> <button class='none'>Clear</button></p>`,
   [['Cancel'],['Clear filter',()=>{begin('Filter');snap(sh);delete sh.filter.cr[c];commit()}],['OK',d=>{const keep=[...d.querySelectorAll('.sp-fl input')].filter(i=>i.checked&&i.parentElement.style.display!='none').map(i=>list[+i.dataset.i]);begin('Filter');snap(sh);if(keep.length==list.length)delete sh.filter.cr[c];else sh.filter.cr[c]=keep;commit()},1]],d=>{
    d.querySelector('.sa').onclick=()=>{d.close();sortRange(F,[[c,1]],true)};d.querySelector('.sz').onclick=()=>{d.close();sortRange(F,[[c,-1]],true)};
    d.querySelector('.all').onclick=()=>d.querySelectorAll('.sp-fl input').forEach(i=>i.checked=true);d.querySelector('.none').onclick=()=>d.querySelectorAll('.sp-fl input').forEach(i=>i.checked=false);
    d.querySelector('.fs').oninput=e=>{const t=e.target.value.toLowerCase();d.querySelectorAll('.sp-fl label').forEach(l=>{l.style.display=l.textContent.toLowerCase().includes(t)?'':'none'})}})}
 function filterByCell(){const t=spText(cellAt(sel.r,sel.c));if(!sh.filter){const R=region();begin('AutoFilter');snap(sh);sh.filter={...R,cr:{}};commit()}const F=sh.filter;if(sel.c<F.c0||sel.c>F.c1)return;begin('Filter');snap(sh);F.cr[sel.c]=[t];commit()}
 function removeDups(){const R=region();const hdr=looksHeader(R);begin('Remove duplicates');snap(sh);const seen=new Set();const keep=[];for(let r=R.r0+(hdr?1:0);r<=R.r1;r++){const key=[];for(let c=R.c0;c<=R.c1;c++)key.push(spText(cellAt(r,c)).toUpperCase());const k=key.join('\u0001');if(seen.has(k))continue;seen.add(k);keep.push(r)}
  const rows=keep.map(r=>{const a=[];for(let c=R.c0;c<=R.c1;c++)a.push(cellAt(r,c));return[r,a]});const start=R.r0+(hdr?1:0);for(let r=start;r<=R.r1;r++)for(let c=R.c0;c<=R.c1;c++)sh.cells.delete(spK(r,c));rows.forEach(([r0,a],i)=>a.forEach((cl,j)=>{if(cl)sh.cells.set(spK(start+i,R.c0+j),cl.f!=null?{...cl,f:SPE.copyFormula(cl.f,start+i-r0,0),ast:undefined}:cl)}));sh.fk=null;const gone=R.r1-start+1-keep.length;commit();info('Remove duplicates',gone?`${gone} duplicate row${gone>1?'s were':' was'} removed; ${keep.length} remain.`:'No duplicates were found.')}
 function textToCols(){const gr=sel.rs[0];if(gr.c0!=gr.c1)return info('Text to columns','Select one column.');dialog('Text to columns',`Split at: <select class='dl'><option value=','>Comma</option><option value='\t'>Tab</option><option value=';'>Semicolon</option><option value=' '>Space</option><option value='other'>Other:</option></select> <input class='ot' size='2' maxlength='1'>`,[['Cancel'],['OK',d=>{let dl=d.querySelector('.dl').value;if(dl=='other')dl=d.querySelector('.ot').value||',';begin('Text to columns');const R=gr.r1-gr.r0>100000?{...gr,r1:spUsed(sh).r1}:gr;for(let r=R.r0;r<=R.r1;r++){const cl=cellAt(r,gr.c0);if(!cl||cl.f!=null)continue;const parts=String(spText(cl)).split(dl);if(parts.length<2)continue;parts.forEach((p,j)=>{const x=spParse(p.trim());setCell(sh,r,gr.c0+j,x?{...(x.f?{f:x.f}:{v:x.v}),st:x.nf?{nf:x.nf}:undefined}:null)})}commit()},1]])}
 /* ---- find and replace ---- */
 let findState={t:'',r:'',mc:false,whole:false,inF:true};
 function findDlg(rep){dialog(rep?'Find and replace':'Find',`<div style='display:grid;grid-template-columns:auto 1fr;gap:6px;align-items:center'><span>Find</span><input class='ft'>${rep?"<span>Replace with</span><input class='rt'>":''}</div><label><input type='checkbox' class='mc'> Match case</label> <label><input type='checkbox' class='wh'> Whole cell</label> <label><input type='checkbox' class='inf' checked> Look in formulas</label><div class='fres' style='margin-top:6px;color:#555;font-size:12px'></div>`,
  [['Close'],rep?['Replace all',d=>{read(d);replaceAll(d);return false}]:null,rep?['Replace',d=>{read(d);replaceOne(d);return false}]:null,['Find next',d=>{read(d);findNext(d);return false},1]].filter(Boolean),d=>{d.querySelector('.ft').value=findState.t;if(rep)d.querySelector('.rt').value=findState.r;d.querySelector('.mc').checked=findState.mc;d.querySelector('.wh').checked=findState.whole})}
 const read=d=>{findState={t:d.querySelector('.ft').value,r:(d.querySelector('.rt')||{}).value||'',mc:d.querySelector('.mc').checked,whole:d.querySelector('.wh').checked,inF:d.querySelector('.inf').checked}};
 function matchCell(cl){if(!cl||!findState.t)return false;const src=findState.inF&&cl.f!=null?cl.f:spText(cl);const a=findState.mc?src:src.toUpperCase(),b=findState.mc?findState.t:findState.t.toUpperCase();return findState.whole?a==b:a.includes(b)}
 function findNext(d){const order=[];for(const s of[sh,...book.sheets.filter(x=>x!==sh)])for(const k of[...s.cells.keys()].sort((a,b)=>a-b))if(matchCell(s.cells.get(k)))order.push([s,k]);if(!order.length){d.querySelector('.fres').textContent='Nothing was found.';return}
  const here=spK(sel.r,sel.c);const nx=order.find(([s,k])=>s===sh&&k>here)||order.find(([s])=>s!==sh)||order[0];if(nx[0]!==sh)showSheet(book.sheets.indexOf(nx[0]));const[r,c]=spRC(nx[1]);select(r,c);scrollInto(r,c);d.querySelector('.fres').textContent=`${order.length} found.`}
 function repIn(t){if(findState.whole)return findState.r;const re=new RegExp(findState.t.replace(/[.*+?^${}()|[\]\\]/g,'\\$&'),findState.mc?'g':'gi');return t.replace(re,()=>findState.r)}
 function repCell(s,r,c,cl){const src=findState.inF&&cl.f!=null?cl.f:cl.f!=null?null:spInput(cl);if(src==null)return;const x=spParse(repIn(src));setCell(s,r,c,x?{...(x.f?{f:x.f}:{v:x.v}),st:cl.st}:{v:null,st:cl.st})}
 function replaceOne(d){const cl=cellAt(sel.r,sel.c);if(matchCell(cl)){begin('Replace');repCell(sh,sel.r,sel.c,cl);commit()}findNext(d)}
 function replaceAll(d){let n=0;begin('Replace all');for(const s of book.sheets)for(const[k,cl]of[...s.cells])if(matchCell(cl)){const[r,c]=spRC(k);repCell(s,r,c,cl);n++}commit();d.querySelector('.fres').textContent=n?`${n} replaced.`:'Nothing was found.'}
 function gotoDlg(){X.ask('Go to','Cell, range or name (for example B12, A1:C9, Sheet2!A1)',nameOfSel()).then(t=>{if(t)goTo(t)})}
 function goTo(t){t=t.trim();let s=sh;const m=/^(?:'([^']+)'|([^!]+))!(.+)$/.exec(t);if(m){s=book.sheets.find(x=>x.name.toUpperCase()==(m[1]||m[2]).toUpperCase());t=m[3];if(!s)return say('There is no sheet called '+(m[1]||m[2]))}
  const nm=book.names[t.toUpperCase()];if(nm){s=book.sheets.find(x=>x.name==nm.sheet)||sh;t=nm.ref}const R=spParseRange(t);if(!R)return say('Not a cell address: '+t);if(s!==sh)showSheet(book.sheets.indexOf(s));sel.rs=[R];sel.r=R.r0;sel.c=R.c0;sel.ar=R.r0;sel.ac=R.c0;afterSel();scrollInto(R.r0,R.c0);sc.focus({preventScroll:true})}
 nb.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Enter'){e.preventDefault();const t=nb.value.trim();if(!t)return;if(/^[A-Za-z_][A-Za-z0-9_.]*$/.test(t)&&!SPE.parseA1(t)&&!spParseRange(t)&&!book.names[t.toUpperCase()]&&!/^[A-Z]{1,3}$/i.test(t)){begin('Define name');snapBook();book.names[t.toUpperCase()]={ref:spRangeStr(sel.rs[0]).replace(/([A-Z]+)(\d+)/g,'$$$1$$$2'),sheet:sh.name};commit();say('Named '+t.toUpperCase());return}goTo(t)}if(e.key=='Escape'){nb.value=nameOfSel();sc.focus()}});
 nb.addEventListener('focus',()=>nb.select());
 /* ================= sheets ================= */
 function renderTabs(){const tl=q('.sp-tl');tl.innerHTML='';book.sheets.forEach((s,i)=>{if(s.hidden)return;const b=document.createElement('button');b.className='sp-tab'+(s===sh?' on':'');b.textContent=s.name;if(s.color)b.style.borderBottom='3px solid '+s.color;b.draggable=true;
  b.onclick=()=>{if(edit&&edit.src&&refWanted()){showSheet(i,true);return}if(edit&&!commitEdit(0,0))return;showSheet(i)};b.ondblclick=()=>renameSheet(i);b.oncontextmenu=e=>{e.preventDefault();X.popMenu([['Insert sheet','',()=>addSheet(i+1)],['Duplicate','',()=>dupSheet(i)],['Rename…','',()=>renameSheet(i)],['Delete','',()=>delSheet(i)],'-',['Move left','',()=>moveSheet(i,-1),false,i==0],['Move right','',()=>moveSheet(i,1),false,i==book.sheets.length-1],['Tab colour…','',()=>X.color('Tab colour',s.color||'#4472c4',c=>{begin('Tab colour');snapBook();snap(s);s.color=c;commit()},'No colour')],['Hide','',()=>{if(book.sheets.filter(x=>!x.hidden).length<2)return;begin('Hide sheet');snap(s);s.hidden=1;commit();showSheet(book.sheets.findIndex(x=>!x.hidden))}],book.sheets.some(x=>x.hidden)?['Unhide…','',unhideSheet]:null],e.clientX,e.clientY-200)};
  b.ondragstart=e=>{e.dataTransfer.setData('text/x-sp-sheet',String(i))};b.ondragover=e=>e.preventDefault();b.ondrop=e=>{e.preventDefault();const from=+e.dataTransfer.getData('text/x-sp-sheet');if(isNaN(from)||from==i)return;begin('Move sheet');snapBook();const[m]=book.sheets.splice(from,1);book.sheets.splice(i,0,m);book.active=book.sheets.indexOf(sh);commit()};tl.appendChild(b)})}
 function showSheet(i,keepEdit){const s=book.sheets[i];if(!s)return;if(edit&&!keepEdit)cancelEdit();sh=s;book.active=i;if(!sh._sel)sh._sel={rs:[{r0:0,c0:0,r1:0,c1:0}],r:0,c:0,ar:0,ac:0};const old=sel;old&&book.sheets.forEach(x=>{});sel=sh._sel;geom(sh);if(sh.filter)applyFilter(false);sc.scrollTop=sh._st||0;sc.scrollLeft=sh._sl||0;sizeSpacer();renderTabs();afterSel();placeCharts()}
 sc.addEventListener('scroll',()=>{sh._st=sc.scrollTop;sh._sl=sc.scrollLeft});
 function uniqueName(base){let n=1,nm=base;while(book.sheets.some(s=>s.name.toUpperCase()==nm.toUpperCase()))nm=base.replace(/\d*$/,'')+(++n);return nm}
 function addSheet(at){begin('New sheet');snapBook();const s=spSheet(uniqueName('Sheet'+(book.sheets.length+1)),book.nid++);book.sheets.splice(at??book.sheets.length,0,s);commit();showSheet(book.sheets.indexOf(s))}
 function dupSheet(i){begin('Duplicate sheet');snapBook();const s=clone(book.sheets[i]);s.id=book.nid++;s.name=uniqueName(book.sheets[i].name.slice(0,26)+' (2)');s._sel=null;book.sheets.splice(i+1,0,s);commit();showSheet(i+1)}
 function renameSheet(i,given){const s=book.sheets[i];(given!=null?Promise.resolve(given):X.ask('Rename sheet','Name',s.name)).then(n=>{n=(n||'').trim().slice(0,31);if(!n||n==s.name)return;if(/[\\/?*[\]:]/.test(n))return info('Rename sheet','A sheet name cannot have \\ / ? * [ ] or :');if(book.sheets.some(x=>x!==s&&x.name.toUpperCase()==n.toUpperCase()))return info('Rename sheet','There is already a sheet with that name.');
  begin('Rename sheet');snapBook();const old=s.name;for(const t of book.sheets){snap(t);for(const[k,cl]of t.cells)if(cl.f!=null&&cl.f.toUpperCase().includes(old.toUpperCase()))t.cells.set(k,{...cl,f:renameRefs(cl.f,old,n),ast:undefined})}for(const v of Object.values(book.names))if(v.sheet==old)v.sheet=n;s.name=n;commit()})}
 function renameRefs(f,old,nw){const q_=x=>/^[A-Za-z_][A-Za-z0-9_]*$/.test(x)?x:"'"+x.replace(/'/g,"''")+"'";let out='',i=0;while(i<f.length){if(f[i]=='"'){const j=f.indexOf('"',i+1);out+=f.slice(i,j<0?f.length:j+1);i=j<0?f.length:j+1;continue}
  const m=/^('(?:[^']|'')+'|[A-Za-z_][A-Za-z0-9_.]*)!/.exec(f.slice(i));if(m&&(i==0||!/[A-Za-z0-9_]/.test(f[i-1]))){const nm=m[1][0]=="'"?m[1].slice(1,-1).replace(/''/g,"'"):m[1];out+=(nm.toUpperCase()==old.toUpperCase()?q_(nw):m[1])+'!';i+=m[0].length;continue}out+=f[i++]}return out}
 function delSheet(i){if(book.sheets.length<2)return info('Delete sheet','A workbook needs at least one sheet.');const s=book.sheets[i];X.confirm('Delete sheet',`Delete <b>${offEsc(s.name)}</b>? Formulas that point to it will show #REF!.`,'Delete').then(y=>{if(!y)return;begin('Delete sheet');snapBook();for(const t of book.sheets){if(t===s)continue;snap(t);for(const[k,cl]of t.cells)if(cl.f!=null)t.cells.set(k,{...cl,f:SPE.shiftFormula(cl.f,(r,c,a,b,shn)=>shn&&shn.toUpperCase()==s.name.toUpperCase()?null:[r,c]),ast:undefined})}book.sheets.splice(i,1);commit();showSheet(Math.max(0,i-1))})}
 function moveSheet(i,d){begin('Move sheet');snapBook();const[s]=book.sheets.splice(i,1);book.sheets.splice(i+d,0,s);book.active=book.sheets.indexOf(sh);commit()}
 function unhideSheet(){const hs=book.sheets.filter(s=>s.hidden);dialog('Unhide sheet',`<select class='u'>${hs.map((s,i)=>`<option value='${i}'>${offEsc(s.name)}</option>`).join('')}</select>`,[['Cancel'],['OK',d=>{const s=hs[+d.querySelector('.u').value];begin('Unhide sheet');snap(s);delete s.hidden;commit();showSheet(book.sheets.indexOf(s))},1]])}
 q('.sp-add').onclick=()=>addSheet();
 /* ================= dialogs: format cells, conditional formatting, validation, names, comments ================= */
 function fmtDlg(){const st=cur(),v=spVal(cellAt(sel.r,sel.c));const nf=st.nf||'';const cat=(SP_NF.find(x=>x[1]==nf)||[nf?'Custom':'General'])[0];const dec=(/\.(0+)/.exec(nf)||['',''])[1].length;
  dialog('Format cells',`<div class='sp-tabsel'><button data-t='n' class='on'>Number</button><button data-t='a'>Alignment</button><button data-t='f'>Font</button><button data-t='b'>Border</button><button data-t='p'>Fill</button></div>
<div class='sp-pg' data-p='n'><select class='cat' size='9' style='width:150px;float:left;margin-right:10px'>${[...SP_NF.map(x=>x[0]),'Custom'].map(n=>`<option ${n==cat?'selected':''}>${n}</option>`).join('')}</select><div><label>Decimals <input type='number' class='dec' min='0' max='10' value='${dec||(cat=='General'?2:dec)}' style='width:50px'></label><br><label><input type='checkbox' class='th' ${/,/.test(nf)?'checked':''}> Thousands separator</label><br><label>Symbol <select class='sym'>${['$','€','£','¥','₹',''].map(s=>`<option ${nf.includes(s)&&s?'selected':''}>${s}</option>`).join('')}</select></label><br><label>Code <input class='code' value='${offEsc(nf)}' style='width:200px'></label><div class='prev' style='margin-top:8px;padding:6px;background:#f3f4f6'></div></div></div>
<div class='sp-pg' data-p='a' style='display:none'><label>Horizontal <select class='ha'><option value=''>General</option><option value='left'>Left</option><option value='center'>Centre</option><option value='right'>Right</option></select></label><br><label>Vertical <select class='va'><option value='bottom'>Bottom</option><option value='middle'>Middle</option><option value='top'>Top</option></select></label><br><label><input type='checkbox' class='wr'> Wrap text</label><br><label><input type='checkbox' class='mg'> Merge cells</label></div>
<div class='sp-pg' data-p='f' style='display:none'><label>Font <select class='ff'>${OFF_FONTS.map(f=>`<option>${f}</option>`).join('')}</select></label> <label>Size <input type='number' class='fz' min='6' max='96' style='width:56px'></label><br><label><input type='checkbox' class='fb'> Bold</label> <label><input type='checkbox' class='fi'> Italic</label> <label><input type='checkbox' class='fu'> Underline</label> <label><input type='checkbox' class='fs'> Strikethrough</label><br><label>Colour <input type='color' class='fcl'></label></div>
<div class='sp-pg' data-p='b' style='display:none'>${['none','all','outside','bottom','top','thick outside','double bottom'].map(p=>`<button class='bp' data-b='${p}'>${p[0].toUpperCase()+p.slice(1)}</button> `).join('')}<p style='font-size:12px;color:#555'>Borders apply to the whole selection.</p></div>
<div class='sp-pg' data-p='p' style='display:none'><label><input type='checkbox' class='nofill'> No fill</label><br><label>Colour <input type='color' class='bgc'></label></div>`,
   [['Cancel'],['OK',d=>{const code=d.querySelector('.code').value;const ff=d.querySelector('.ff').value,fz=+d.querySelector('.fz').value,ha=d.querySelector('.ha').value,va=d.querySelector('.va').value;const o={nf:code||undefined,ha:ha||undefined,va:va=='bottom'?undefined:va,wrap:d.querySelector('.wr').checked?1:undefined,font:ff=='Calibri'?undefined:ff,sz:fz&&fz!=11?fz:undefined,b:d.querySelector('.fb').checked?1:undefined,i:d.querySelector('.fi').checked?1:undefined,u:d.querySelector('.fu').checked?1:undefined,s:d.querySelector('.fs').checked?1:undefined};
     const fc=d.querySelector('.fcl').value;o.fc=fc=='#000000'?undefined:fc;o.bg=d.querySelector('.nofill').checked?undefined:d.querySelector('.bgc').dataset.set?d.querySelector('.bgc').value:st.bg;
     styleSel('Format cells',s=>({...s,...o}));if(d.querySelector('.bdp'))borders(d.querySelector('.bdp').value);if(d.querySelector('.mg').checked!=!!mergeAt(sel.r,sel.c))mergeSel(false)},1]],d=>{
    d.querySelectorAll('.sp-tabsel button').forEach(b=>b.onclick=()=>{d.querySelectorAll('.sp-tabsel button').forEach(x=>x.classList.toggle('on',x===b));d.querySelectorAll('.sp-pg').forEach(p=>p.style.display=p.dataset.p==b.dataset.t?'':'none')});
    const upd=()=>{const c=d.querySelector('.cat').value,dc=+d.querySelector('.dec').value||0,th=d.querySelector('.th').checked,sym=d.querySelector('.sym').value;const dd=dc?'.'+'0'.repeat(dc):'';let code=d.querySelector('.code').value;
     if(c=='General')code='';else if(c=='Number')code=(th?'#,##0':'0')+dd;else if(c=='Currency')code=sym+'#,##0'+dd;else if(c=='Accounting')code=`_(${sym}* #,##0${dd}_);_(${sym}* (#,##0${dd});_(${sym}* "-"??_);_(@_)`;else if(c=='Percent')code='0'+dd+'%';else if(c=='Scientific')code='0'+dd+'E+00';else if(c!='Custom')code=(SP_NF.find(x=>x[0]==c)||[,''])[1];
     d.querySelector('.code').value=code;d.querySelector('.prev').textContent='Sample: '+SPE.fmtValue(v==null?1234.5678:v,code)};
    ['.cat','.dec','.th','.sym'].forEach(s=>d.querySelector(s).addEventListener('change',upd));d.querySelector('.code').addEventListener('input',()=>{d.querySelector('.cat').value='Custom';d.querySelector('.prev').textContent='Sample: '+SPE.fmtValue(v==null?1234.5678:v,d.querySelector('.code').value)});d.querySelector('.prev').textContent='Sample: '+SPE.fmtValue(v==null?1234.5678:v,nf);
    d.querySelector('.ha').value=st.ha||'';d.querySelector('.va').value=st.va||'bottom';d.querySelector('.wr').checked=!!st.wrap;d.querySelector('.mg').checked=!!mergeAt(sel.r,sel.c);d.querySelector('.ff').value=st.font||'Calibri';d.querySelector('.fz').value=st.sz||11;['b','i','u','s'].forEach(k=>d.querySelector('.f'+k).checked=!!st[k]);d.querySelector('.fcl').value=st.fc||'#000000';d.querySelector('.bgc').value=st.bg||'#ffffff';d.querySelector('.nofill').checked=!st.bg;
    d.querySelector('.bgc').oninput=e=>{e.target.dataset.set=1;d.querySelector('.nofill').checked=false};
    d.querySelectorAll('.bp').forEach(b=>b.onclick=()=>{let h=d.querySelector('.bdp');if(!h){h=document.createElement('input');h.type='hidden';h.className='bdp';d.appendChild(h)}h.value=b.dataset.b;d.querySelectorAll('.bp').forEach(x=>x.classList.toggle('on',x===b))})})}
 function borders(kind){const gr=sel.rs[0];begin('Borders');for(let r=gr.r0;r<=gr.r1&&r<=gr.r0+5000;r++)for(let c=gr.c0;c<=gr.c1&&c<=gr.c0+300;c++){const cl=cellAt(r,c);const st={...(cl&&cl.st||{})};let bd={...(st.bd||{})};
   const top=r==gr.r0,bot=r==gr.r1,lef=c==gr.c0,rig=c==gr.c1;
   if(kind=='none')bd={};else if(kind=='all')bd={t:'thin',b:'thin',l:'thin',r:'thin'};else if(kind=='outside'||kind=='thick outside'){const w=kind=='outside'?'thin':'thick';if(top)bd.t=w;if(bot)bd.b=w;if(lef)bd.l=w;if(rig)bd.r=w}else if(kind=='bottom'){if(bot)bd.b='thin'}else if(kind=='top'){if(top)bd.t='thin'}else if(kind=='double bottom'){if(bot)bd.b='double'}
   st.bd=Object.keys(bd).length?bd:undefined;setCell(sh,r,c,{...(cl||{v:null}),ast:undefined,st:clean(st)})}commit()}
 function cfDlg(){const gr=sel.rs[0];dialog('Conditional formatting: '+spRangeStr(gr),`<label>Colour cells when <select class='t'><option value='gt'>greater than</option><option value='lt'>less than</option><option value='between'>between</option><option value='eq'>equal to</option><option value='text'>text contains</option><option value='top'>in the top</option><option value='bottom'>in the bottom</option><option value='dup'>duplicate values</option><option value='scale'>colour scale (red, yellow, green)</option><option value='scale2'>colour scale (white to green)</option><option value='bar'>data bars</option></select></label>
<div class='ab' style='margin:6px 0'><input class='a' style='width:90px'> <span class='and'>and <input class='b' style='width:90px'></span></div><label class='stl'>Style <select class='st'><option value='0'>Light red fill, dark red text</option><option value='1'>Yellow fill, dark yellow text</option><option value='2'>Green fill, dark green text</option><option value='3'>Light red fill</option><option value='4'>Red text</option><option value='5'>Bold</option></select></label>
<h4 style='margin:10px 0 4px'>Rules on this sheet</h4><div class='rl' style='max-height:120px;overflow:auto;font-size:12px'></div>`,[['Close'],['Add rule',d=>{let t=d.querySelector('.t').value;const styles=[{bg:'#ffc7ce',fc:'#9c0006'},{bg:'#ffeb9c',fc:'#9c5700'},{bg:'#c6efce',fc:'#006100'},{bg:'#ffc7ce'},{fc:'#c00000'},{b:1}];
   const R={...gr,t:t=='scale2'?'scale':t,a:d.querySelector('.a').value,b:d.querySelector('.b').value,st:styles[+d.querySelector('.st').value]};if(t=='scale2')R.cs=['#ffffff','#63be7b'];if(R.r1-R.r0>100000)R.r1=Math.max(R.r0,spUsed(sh).r1);begin('Conditional formatting');snap(sh);sh.cf.push(R);commit();list(d);return false},1]],d=>{
    const upd=()=>{const t=d.querySelector('.t').value;d.querySelector('.ab').style.display=/dup|scale|bar/.test(t)?'none':'';d.querySelector('.and').style.display=t=='between'?'':'none';d.querySelector('.stl').style.display=/scale|bar/.test(t)?'none':'';if(/top|bottom/.test(t)&&!d.querySelector('.a').value)d.querySelector('.a').value='10'};d.querySelector('.t').onchange=upd;upd();list(d)})
  function list(d){const l=d.querySelector('.rl');l.innerHTML='';sh.cf.forEach((R,i)=>{const row=document.createElement('div');row.style.cssText='display:flex;justify-content:space-between;padding:2px 0';row.innerHTML=`<span></span><button>Delete</button>`;row.firstChild.textContent=`${spRangeStr(R)}: ${R.t} ${R.a??''}${R.t=='between'?' and '+R.b:''}`;row.querySelector('button').onclick=()=>{begin('Delete rule');snap(sh);sh.cf.splice(i,1);commit();list(d)};l.appendChild(row)});if(!sh.cf.length)l.textContent='None yet.'}}
 function dvDlg(){const gr=sel.rs[0];const ex=dvAt(sel.r,sel.c);dialog('Data validation: '+spRangeStr(gr),`<label>Allow <select class='t'><option value='any'>Any value</option><option value='list'>A list</option><option value='whole'>Whole numbers</option><option value='num'>Numbers</option><option value='date'>Dates</option><option value='len'>Text up to a length</option></select></label>
<div class='li' style='margin:6px 0'><label>Items, separated by commas, or a range such as =$A$1:$A$9<br><input class='ls' style='width:320px'></label></div><div class='mm' style='margin:6px 0'><label>From <input class='mn' style='width:100px'></label> <label>to <input class='mx' style='width:100px'></label></div><label>Message when not allowed<br><input class='ms' style='width:320px'></label>`,
  [['Cancel'],['OK',d=>{const t=d.querySelector('.t').value;begin('Data validation');snap(sh);sh.dv=sh.dv.filter(x=>!(x.r0>=gr.r0&&x.r1<=gr.r1&&x.c0>=gr.c0&&x.c1<=gr.c1));if(t!='any'){const R={...gr,t,list:d.querySelector('.ls').value,min:d.querySelector('.mn').value,max:d.querySelector('.mx').value,msg:d.querySelector('.ms').value};if(R.r1-R.r0>100000)R.r1=R.r0+9999;sh.dv.push(R)}commit()},1]],d=>{
   const upd=()=>{const t=d.querySelector('.t').value;d.querySelector('.li').style.display=t=='list'?'':'none';d.querySelector('.mm').style.display=/whole|num|date|len/.test(t)?'':'none';d.querySelector('.mn').disabled=t=='len'};d.querySelector('.t').onchange=upd;if(ex){d.querySelector('.t').value=ex.t;d.querySelector('.ls').value=ex.list||'';d.querySelector('.mn').value=ex.min||'';d.querySelector('.mx').value=ex.max||'';d.querySelector('.ms').value=ex.msg||''}upd()})}
 function nameDlg(){const rows=()=>Object.entries(book.names).map(([n,v])=>`<tr><td><b>${offEsc(n)}</b></td><td>${offEsc((v.sheet?v.sheet+'!':'')+v.ref)}</td><td><button data-n='${offEsc(n)}'>Delete</button></td></tr>`).join('')||'<tr><td colspan=3>No names yet.</td></tr>';
  dialog('Names',`<label>Name <input class='n' style='width:140px'></label> <label>Refers to <input class='r' style='width:140px' value='${spRangeStr(sel.rs[0]).replace(/([A-Z]+)(\d+)/g,'$$$1$$$2')}'></label><table class='sp-nt' style='margin-top:8px;width:100%;font-size:12px'>${rows()}</table>`,[['Close'],['Add',d=>{const n=d.querySelector('.n').value.trim().toUpperCase(),r=d.querySelector('.r').value.trim();if(!/^[A-Z_][A-Z0-9_.]*$/.test(n)||SPE.parseA1(n)||SPE.F[n])return info('Names','A name starts with a letter, has no spaces, and is not a cell address or a function.'),false;if(!spParseRange(r.replace(/^.*!/,'')))return info('Names','“Refers to” is not a range.'),false;begin('Define name');snapBook();book.names[n]={ref:r.replace(/^.*!/,''),sheet:/!/.test(r)?r.split('!')[0].replace(/^'|'$/g,''):sh.name};commit();d.querySelector('.sp-nt').innerHTML=rows();wire(d);return false},1]],wire);
  function wire(d){d.querySelectorAll('.sp-nt button').forEach(b=>b.onclick=()=>{begin('Delete name');snapBook();delete book.names[b.dataset.n];commit();d.querySelector('.sp-nt').innerHTML=rows();wire(d)})}}
 function commentDlg(){const k=spK(sel.r,sel.c);dialog('Comment on '+SPE.a1(sel.r,sel.c),`<textarea class='t' rows='5' style='width:320px'></textarea>`,[['Cancel'],sh.cm[k]?['Delete',()=>{begin('Comment');snap(sh);delete sh.cm[k];commit()}]:null,['OK',d=>{const t=d.querySelector('.t').value.trim();begin('Comment');snap(sh);if(t)sh.cm[k]=((typeof USER!='undefined'&&USER&&!sh.cm[k])?USER.n+': ':'')+t.replace(/^[^:]*: /,m=>sh.cm[k]?m:'');else delete sh.cm[k];commit()},1]].filter(Boolean),d=>{d.querySelector('.t').value=sh.cm[k]||''})}
 function styleDlg(){X.popMenu(Object.keys(SP_STYLES).map(n=>[n,'',()=>styleSel('Cell style',st=>SP_STYLES[n]?{...(st.nf&&!SP_STYLES[n].nf?{nf:st.nf}:{}),...SP_STYLES[n]}:(st.nf?{nf:st.nf}:{}))]),root.getBoundingClientRect().left+300,root.getBoundingClientRect().top+60)}
 function funcDlg(){const groups={'Maths':'SUM,SUMPRODUCT,PRODUCT,ROUND,ROUNDUP,ROUNDDOWN,INT,TRUNC,MOD,ABS,SQRT,POWER,EXP,LN,LOG,PI,RAND,RANDBETWEEN,CEILING,FLOOR,MROUND,GCD,LCM,FACT,SIGN',Statistics:'AVERAGE,AVERAGEIF,AVERAGEIFS,MEDIAN,MODE,MIN,MAX,MINIFS,MAXIFS,COUNT,COUNTA,COUNTBLANK,COUNTIF,COUNTIFS,SUMIF,SUMIFS,LARGE,SMALL,RANK,STDEV,STDEVP,VAR,VARP,PERCENTILE,QUARTILE,CORREL',Logic:'IF,IFS,IFERROR,IFNA,AND,OR,NOT,XOR,SWITCH,CHOOSE,ISBLANK,ISNUMBER,ISTEXT,ISERROR',Lookup:'XLOOKUP,VLOOKUP,HLOOKUP,INDEX,MATCH,OFFSET,INDIRECT,ROW,COLUMN,ROWS,COLUMNS,ADDRESS',Text:'LEFT,RIGHT,MID,LEN,FIND,SEARCH,SUBSTITUTE,REPLACE,UPPER,LOWER,PROPER,TRIM,TEXT,VALUE,CONCAT,TEXTJOIN,REPT,EXACT,CHAR,CODE',Date:'TODAY,NOW,DATE,TIME,YEAR,MONTH,DAY,HOUR,MINUTE,SECOND,WEEKDAY,WEEKNUM,EDATE,EOMONTH,DATEDIF,DAYS,NETWORKDAYS,WORKDAY,DATEVALUE',Finance:'PMT,FV,PV,NPER,NPV,IRR,RATE,SLN'};
  dialog('Insert function',`<input class='s' placeholder='Search, for example: average' style='width:100%'><select class='g'>${Object.keys(groups).map(g_=>`<option>${g_}</option>`).join('')}<option value='*'>All (${Object.keys(SPE.F).length})</option></select><select class='l' size='10' style='width:100%;margin-top:4px'></select><div class='dd' style='min-height:36px;font-size:12px;margin-top:4px'></div>`,[['Cancel'],['OK',d=>{const n=d.querySelector('.l').value;if(!n)return false;const t=edit?(edit.src=='bar'?fx:ed).value:'';if(edit){const el=edit.src=='bar'?fx:ed;const p=el.selectionStart;el.value=el.value.slice(0,p)+n+'('+el.value.slice(p);sync(el);el.focus();el.setSelectionRange(p+n.length+1,p+n.length+1)}else{startEdit('='+n+'(','edit')}hintUpd()},1]],d=>{
   const fill=()=>{const s=d.querySelector('.s').value.toUpperCase(),gv=d.querySelector('.g').value;const names=(s?Object.keys(SPE.F).filter(n=>n.includes(s)||(SPE.D[n]||'').toUpperCase().includes(s)):gv=='*'?Object.keys(SPE.F):groups[gv].split(',')).sort();d.querySelector('.l').innerHTML=names.map(n=>`<option>${n}</option>`).join('');d.querySelector('.l').selectedIndex=0;desc()};
   const desc=()=>{d.querySelector('.dd').textContent=SPE.D[d.querySelector('.l').value]||''};d.querySelector('.s').oninput=fill;d.querySelector('.g').onchange=fill;d.querySelector('.l').onchange=desc;d.querySelector('.l').ondblclick=()=>d.querySelector('[data-pri]').click();fill()})}
 /* ================= charts ================= */
 function chartData(ch,s){s=s||sh;const R=spParseRange(ch.rg);if(!R)return{cats:[],series:[]};const rows=[];for(let r=R.r0;r<=Math.min(R.r1,R.r0+2000);r++){if(s.hid[r]||(s._fh||{})[r])continue;const row=[];for(let c=R.c0;c<=Math.min(R.c1,R.c0+50);c++)row.push(spVal(s.cells.get(spK(r,c))));rows.push(row)}
  if(!rows.length)return{cats:[],series:[]};const hdr=rows[0].slice(1).some(v=>typeof v=='string')||rows.length>1&&rows[0].every(v=>typeof v!='number'&&v!=null)&&rows.slice(1).some(r=>r.some(v=>typeof v=='number'));
  const body=hdr?rows.slice(1):rows;const catCol=body.length&&body.every(r=>typeof r[0]!='number'||ch.type=='scatter')&&rows[0].length>1;
  const byCol=ch.by?ch.by=='c':body.length>=rows[0].length-(catCol?1:0);const cats=[],series=[];
  if(byCol){body.forEach((r,i)=>cats.push(catCol?(r[0]==null?'':typeof r[0]=='number'?SPE.fmtValue(r[0],(s.cells.get(spK(R.r0+(hdr?1:0)+i,R.c0))||{}).st?.nf):String(r[0])):String(i+1)));for(let j=catCol?1:0;j<rows[0].length;j++)series.push({name:hdr?String(rows[0][j]??''):'Series '+(series.length+1),vals:body.map(r=>typeof r[j]=='number'?r[j]:null)})}
  else{const start=catCol?1:0;for(let j=start;j<rows[0].length;j++)cats.push(hdr?String(rows[0][j]??''):String(j));body.forEach((r,i)=>series.push({name:catCol?String(r[0]??''):'Series '+(i+1),vals:r.slice(start).map(v=>typeof v=='number'?v:null)}))}
  if(ch.type=='scatter'&&catCol&&byCol)body.forEach((r,i)=>cats[i]=typeof r[0]=='number'?r[0]:i+1);
  (ch.colors||[]).forEach((c,i)=>{if(series[i]&&c)series[i].color=c});return{cats,series}}
 const chartSpec=(ch,s)=>({...chartData(ch,s),type:ch.type,title:ch.title,legend:ch.legend!==false,xt:ch.xt,yt:ch.yt,w:ch.w,h:ch.h,colors:/pie|doughnut/.test(ch.type)?ch.colors:null});
 function placeCharts(){const want=new Set(sh.charts.map(c=>c.id));[...ol.children].forEach(el=>{if(!want.has(+el.dataset.id)||el.dataset.sheet!=sh.id)el.remove()});
  const f=sh.freeze,ox=HW+FX(),oy=HH+FY();ol.style.left=ox+'px';ol.style.top=oy+'px';ol.style.width=Math.max(0,VW-ox)+'px';ol.style.height=Math.max(0,VH-oy)+'px';
  for(const ch of sh.charts){let el=ol.querySelector(`[data-id='${ch.id}'][data-sheet='${sh.id}']`);const key=JSON.stringify([ch,vers,zoom]);if(!el){el=document.createElement('div');el.className='sp-ch';el.dataset.id=ch.id;el.dataset.sheet=sh.id;el.innerHTML=`<div class='sp-cs'></div><i class='sp-crz'></i>`;ol.appendChild(el);chartEvents(el,ch.id)}
   el.style.left=((ch.x-sh._cx[f.c])*zoom-sc.scrollLeft)+'px';el.style.top=((ch.y-sh._ry[f.r])*zoom-sc.scrollTop)+'px';el.style.width=ch.w*zoom+'px';el.style.height=ch.h*zoom+'px';el.classList.toggle('on',chSel===ch.id);
   if(el.dataset.k!==key){el.dataset.k=key;el.querySelector('.sp-cs').innerHTML=offChartSVG(chartSpec(ch)).replace('<svg ',`<svg style='width:100%;height:100%' preserveAspectRatio='none' `)}}}
 function chartEvents(el,id){const get=()=>sh.charts.find(c=>c.id==id);
  el.addEventListener('mousedown',e=>{if(e.button!=0)return;e.preventDefault();e.stopPropagation();X.closeMenu();chSel=id;placeCharts();sc.focus({preventScroll:true});const ch=get();if(!ch)return;const rzh=e.target.classList.contains('sp-crz');const sx=e.clientX,sy=e.clientY,o={x:ch.x,y:ch.y,w:ch.w,h:ch.h};let moved=false;
   const mv=ev=>{const dx=(ev.clientX-sx)/zoom,dy=(ev.clientY-sy)/zoom;if(Math.abs(dx)+Math.abs(dy)>2)moved=true;if(rzh){ch.w=Math.max(120,o.w+dx);ch.h=Math.max(90,o.h+dy)}else{ch.x=Math.max(0,o.x+dx);ch.y=Math.max(0,o.y+dy)}placeCharts()};
   const up=()=>{document.removeEventListener('mousemove',mv);document.removeEventListener('mouseup',up);if(moved){const n={x:ch.x,y:ch.y,w:ch.w,h:ch.h};Object.assign(ch,o);begin(rzh?'Resize chart':'Move chart');snap(sh);Object.assign(sh.charts.find(c=>c.id==id),n);commit({quiet:1})}};
   document.addEventListener('mousemove',mv);document.addEventListener('mouseup',up)});
  el.addEventListener('dblclick',e=>{e.stopPropagation();chartDlg(get())});
  el.addEventListener('wheel',e=>{sc.scrollTop+=e.deltaY;sc.scrollLeft+=e.deltaX},{passive:true});
  el.addEventListener('contextmenu',e=>{e.preventDefault();e.stopPropagation();chSel=id;placeCharts();X.popMenu([['Change chart…','',()=>chartDlg(get())],['Copy as a picture','',()=>copyChart(get(),false)],['Copy with its data (for WordPro and PresentPro)','',()=>copyChart(get(),true)],['Save as a picture…','',()=>saveChartPng(get())],'-',['Bring to front','',()=>{begin('Chart order');snap(sh);const i=sh.charts.findIndex(c=>c.id==id);sh.charts.push(...sh.charts.splice(i,1));commit({quiet:1});ol.querySelectorAll('.sp-ch').forEach(x=>x.remove());placeCharts()}],['Delete','Delete',()=>delChart(id)]],e.clientX,e.clientY)})}
 function delChart(id){begin('Delete chart');snap(sh);sh.charts=sh.charts.filter(c=>c.id!=id);chSel=null;commit()}
 async function copyChart(ch,live){const spec=chartSpec(ch);const svg=offChartSVG(spec);const png=await offSvgPng(svg,ch.w,ch.h);OFFCLIP={kind:'chart',spec,png,live:live?{rg:ch.rg,sheet:sh.name}:null,from:'SpreadsheetPro'};
  try{const b=await (await fetch(png)).blob();await navigator.clipboard.write([new ClipboardItem({'image/png':b})])}catch(e){}say(live?'Chart copied with its data: paste it in WordPro or PresentPro':'Chart copied as a picture')}
 async function saveChartPng(ch){const png=await offSvgPng(offChartSVG(chartSpec(ch)),ch.w,ch.h);const p=await saveVfsDlg(((ch.title||'CHART').toUpperCase().replace(/[^A-Z0-9]+/g,'-')||'CHART')+'.PNG','PICTURES');if(p){vfsWrite(p,png);say('Saved '+p.join('\\'))}}
 function chartDlg(ch){const isNew=!ch;const R=isNew?(()=>{const g_=sel.rs[0];return g_.r0==g_.r1&&g_.c0==g_.c1?region():{...g_,r1:Math.min(g_.r1,Math.max(g_.r0,spUsed(sh).r1)),c1:Math.min(g_.c1,Math.max(g_.c0,spUsed(sh).c1))}})():null;
  const c=ch?JSON.parse(JSON.stringify(ch)):{id:Date.now()%1e9,type:'column',rg:spRangeStr(R),x:sh._cx[Math.min(SPM-1,R.c1+2)],y:sh._ry[R.r0]+4,w:440,h:280,title:'',legend:true,xt:'',yt:'',colors:[]};
  if(isNew&&!c.title){const d0=chartData(c);if(d0.series.length==1)c.title=d0.series[0].name}
  dialog(isNew?'Insert chart':'Change chart',`<div style='display:flex;gap:12px'><div><div class='sp-ctypes'>${[['column','▮▮ Column'],['bar','▬ Bar'],['line','📈 Line'],['area','◭ Area'],['pie','◔ Pie'],['doughnut','◎ Doughnut'],['scatter','⁘ Scatter']].map(([t,n])=>`<button data-t='${t}'>${n}</button>`).join('')}</div>
<label>Data <input class='rg' style='width:120px'></label><br><label>Series in <select class='by'><option value=''>(automatic)</option><option value='c'>Columns</option><option value='r'>Rows</option></select></label><br><label>Title <input class='ti' style='width:180px'></label><br><label><input type='checkbox' class='lg'> Legend</label><br><label>Axis titles <input class='xt' placeholder='across' style='width:80px'> <input class='yt' placeholder='up' style='width:80px'></label><div class='cols' style='margin-top:6px'></div></div><div class='pv' style='width:320px;height:210px;border:1px solid #ddd'></div></div>`,
   [['Cancel'],['OK',d=>{rd(d);if(!spParseRange(c.rg))return info('Chart','The data range is not valid.'),false;begin(isNew?'Insert chart':'Change chart');snap(sh);if(isNew)sh.charts.push(c);else Object.assign(sh.charts.find(x=>x.id==c.id),c);chSel=c.id;commit()},1]],d=>{
    const pv=()=>{rd(d);d.querySelector('.pv').innerHTML=offChartSVG({...chartSpec(c),w:320,h:210});d.querySelectorAll('.sp-ctypes button').forEach(b=>b.classList.toggle('on',b.dataset.t==c.type));
     const ss=chartData(c).series;const n=/pie|doughnut/.test(c.type)?chartData(c).cats.length:ss.length;d.querySelector('.cols').innerHTML=Array.from({length:Math.min(n,10)},(_,i)=>`<input type='color' data-i='${i}' value='${(c.colors||[])[i]||OFF_CHART_COLORS[i%10]}' title='Colour ${i+1}'>`).join('');d.querySelectorAll('.cols input').forEach(x=>x.oninput=()=>{c.colors=c.colors||[];c.colors[+x.dataset.i]=x.value;d.querySelector('.pv').innerHTML=offChartSVG({...chartSpec(c),w:320,h:210})})};
    d.querySelector('.rg').value=c.rg;d.querySelector('.ti').value=c.title||'';d.querySelector('.lg').checked=c.legend!==false;d.querySelector('.xt').value=c.xt||'';d.querySelector('.yt').value=c.yt||'';d.querySelector('.by').value=c.by||'';
    d.querySelectorAll('.sp-ctypes button').forEach(b=>b.onclick=()=>{c.type=b.dataset.t;pv()});d.querySelectorAll('input:not([type=color]),select').forEach(i=>i.addEventListener('input',pv));pv()});
  function rd(d){c.rg=d.querySelector('.rg').value.trim().toUpperCase().replace(/\$/g,'');c.title=d.querySelector('.ti').value;c.legend=d.querySelector('.lg').checked;c.xt=d.querySelector('.xt').value;c.yt=d.querySelector('.yt').value;c.by=d.querySelector('.by').value||undefined}}
 /* ================= files ================= */
 const ttl=()=>{W.setTitle((file?file.at(-1):'Book1')+(dirty?' *':'')+' - SpreadsheetPro');q('.sp-sv').textContent=dirty?'● Unsaved':'Saved'};
 async function loadPath(p){const v=node(p);if(v==null||isDir(v))return info('Open','The file could not be read.');const e=ext(p.at(-1));
  try{let b;if(e=='XLSX'||e=='XLSM'||/^data:application\/(vnd\.openxml|zip|octet)/.test(v))b=await spFromXlsx(offBytes(v));else if(e=='CSV'||e=='TSV'||e=='TXT')b=spFromCSV(typeof v=='string'&&v.startsWith('data:')?new TextDecoder().decode(offBytes(v)):v,p.at(-1),e=='TSV'?'\t':null);else b=spFromJSON(v);
   setBook(b);file=p;dirty=false;ttl();try{recentAdd(p)}catch(x){}say('Opened '+p.at(-1))}catch(err){console.warn(err);info('Open',`<b>${offEsc(p.at(-1))}</b> could not be opened: ${offEsc(err.message||err)}`)}}
 function setBook(b){book=b;book.sheets.forEach(s=>{s._sel=null;s._cx=null;s.fk=null});undoS=[];redoS=[];clip=null;chSel=null;book._dep=null;SPE.recalc(book,null);vers++;cfCache=null;ol.innerHTML='';showSheet(Math.min(book.active||0,book.sheets.length-1));renderTabs();ttl()}
 function confirmDiscard(next){if(!dirty)return next();dialog('SpreadsheetPro',`Save changes to <b>${offEsc(file?file.at(-1):'Book1')}</b>?`,[['Cancel'],["Don't save",()=>{dirty=false;next()}],['Save',()=>{doSave(false,next)},1]])}
 async function openDlg(){const p=await pickVfsFile('Open a spreadsheet',n=>/\.(RSHEET|XLSX|XLSM|CSV|TSV)$/i.test(n));if(!p)return;if(!dirty&&!file&&book.sheets.length==1&&!sh.cells.size)loadPath(p);else sheetApp(p)}
 async function doSave(as,then){let p=file;const e=p&&ext(p.at(-1));if(!p||as){p=await saveVfsDlg(file?file.at(-1).replace(/\.(CSV|TSV)$/i,'.RSHEET'):'BOOK1.RSHEET');if(!p)return}
  const E=ext(p.at(-1));if(!/^(RSHEET|XLSX|CSV|TSV)$/.test(E))p=[...p.slice(0,-1),p.at(-1)+'.RSHEET'];const E2=ext(p.at(-1));
  try{if(E2=='XLSX'){const blob=await spToXlsx(book);vfsWrite(p,await offDataUrl(blob))}else if(E2=='CSV'||E2=='TSV'){if(book.sheets.length>1&&!as&&e!==E2){}vfsWrite(p,spToCSV(sh,E2=='TSV'?'\t':','));if(book.sheets.length>1)say('Only the sheet “'+sh.name+'” was saved: CSV holds one sheet')}else vfsWrite(p,spToJSON(book))}
  catch(err){return info('Save','The file could not be saved: '+offEsc(err.message||err))}
  file=p;dirty=false;ttl();autos.clear();try{recentAdd(p)}catch(x){}notify('Saved',p.join('\\'),{quiet:true});then&&then()}
 async function importCSV(){const p=await pickVfsFile('Import a CSV file into a new sheet',n=>/\.(CSV|TSV|TXT)$/i.test(n));if(!p)return;const b=spFromCSV(String(node(p)),p.at(-1));const s=b.sheets[0];begin('Import');snapBook();s.id=book.nid++;s.name=uniqueName(s.name);book.sheets.push(s);commit();showSheet(book.sheets.length-1)}
 function exportCSV(){saveOut((file?file.at(-1).replace(/\.[^.]*$/,''):'Book1')+(book.sheets.length>1?'-'+sh.name:'')+'.csv',spToCSV(sh),'text/csv')}
 async function exportXlsx(){const blob=await spToXlsx(book);saveOut((file?file.at(-1).replace(/\.[^.]*$/,''):'Book1')+'.xlsx',blob,'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet')}
 function newFromTemplate(){dialog('New from template',`<div class='sp-tpl'>${[['budget','💰','Monthly budget'],['invoice','🧾','Invoice'],['timetable','🗓️','Timetable'],['grades','🎓','Grade book'],['loan','🏠','Loan calculator']].map(([k,i,n])=>`<button data-k='${k}'><span>${i}</span>${n}</button>`).join('')}</div>`,[['Cancel']],d=>{d.querySelectorAll('.sp-tpl button').forEach(b=>b.onclick=()=>{d.close();const tb=spTemplate(b.dataset.k);if(!dirty&&!file&&!sh.cells.size&&book.sheets.length==1){setBook(tb);dirty=true;ttl()}else{const w=sheetApp(null,tb);w&&w.sp&&w.sp.markDirty()}})})}
 /* ================= printing and PDF ================= */
 const PAGE={Letter:[816,1056],A4:[794,1123],Legal:[816,1344]};
 function pageSetup(){const P=sh.print||{};dialog('Page setup',`<label>Paper <select class='ps'>${Object.keys(PAGE).map(k=>`<option ${P.size==k?'selected':''}>${k}</option>`).join('')}</select></label> <label><input type='radio' name='o' value='p' ${!P.land?'checked':''}> Portrait</label> <label><input type='radio' name='o' value='l' ${P.land?'checked':''}> Landscape</label><br>
<label><input type='checkbox' class='fit' ${P.fit!==false?'checked':''}> Fit all columns on one page width</label><br><label><input type='checkbox' class='gr' ${P.grid?'checked':''}> Print grid lines</label><br><label>Rows to repeat at the top of each page <input class='rep' placeholder='e.g. 1:2' value='${offEsc(P.rep||'')}' style='width:70px'></label><br><label>Print area <input class='area' placeholder='all the data' value='${offEsc(P.area||'')}' style='width:110px'></label> <button class='usesel'>Use the selection</button>`,
  [['Cancel'],['OK',d=>{begin('Page setup');snap(sh);sh.print={size:d.querySelector('.ps').value,land:d.querySelector('input[name=o]:checked').value=='l',fit:d.querySelector('.fit').checked,grid:d.querySelector('.gr').checked,rep:d.querySelector('.rep').value.trim(),area:d.querySelector('.area').value.trim().toUpperCase()};commit({quiet:1})},1]],d=>{d.querySelector('.usesel').onclick=()=>{d.querySelector('.area').value=spRangeStr(sel.rs[0])}})}
 async function pages(){/* the sheet as page images (canvases), 2 pixels for every pixel at 96 dpi */const P=sh.print||{};let R=P.area&&spParseRange(P.area);const u=spUsed(sh);
  if(!R){if(u.r1<0&&!sh.charts.length)return[];R={r0:0,c0:0,r1:Math.max(0,u.r1),c1:Math.max(0,u.c1)};for(const ch of sh.charts){geom(sh);R.c1=Math.max(R.c1,bs(sh._cx,ch.x+ch.w));R.r1=Math.max(R.r1,bs(sh._ry,ch.y+ch.h))}}
  geom(sh);const[pw0,ph0]=PAGE[P.size||'Letter']||PAGE.Letter;const pw=P.land?ph0:pw0,ph=P.land?pw0:ph0,m=48,S=2;const rep=P.rep&&spParseRange(P.rep.includes(':')?P.rep:P.rep+':'+P.rep);
  const cw=c=>sh._cx[c+1]-sh._cx[c],rh=r=>sh._ry[r+1]-sh._ry[r];let totalW=0;for(let c=R.c0;c<=R.c1;c++)totalW+=cw(c);
  const k=P.fit!==false?Math.min(1,(pw-2*m)/Math.max(1,totalW)):1;
  const colPages=[];{let c=R.c0;while(c<=R.c1){let w=0,c1=c;while(c1<=R.c1&&(w+cw(c1))*k<=pw-2*m||c1==c){w+=cw(c1);c1++}colPages.push([c,c1-1]);c=c1}}
  const repRows=rep?Array.from({length:rep.r1-rep.r0+1},(_,i)=>rep.r0+i).filter(r=>r<R.r0||true):[];const repH=repRows.reduce((a,r)=>a+rh(r),0);
  const rowPages=[];{let r=R.r0;while(r<=R.r1){let h=rowPages.length&&repRows.length?repH:0,r1=r;while(r1<=R.r1&&((h+rh(r1))*k<=ph-2*m-20||r1==r)){h+=rh(r1);r1++}rowPages.push([r,r1-1]);r=r1}}
  const out=[];const chartImgs={};for(const ch of sh.charts){chartImgs[ch.id]=await new Promise(res=>{const im=new Image();im.onload=()=>res(im);im.onerror=()=>res(null);im.src='data:image/svg+xml;charset=utf-8,'+encodeURIComponent(offChartSVG(chartSpec(ch)))})}
  const savedZ=zoom;zoom=1;
  try{rowPages.forEach(([ra,rb],pi)=>colPages.forEach(([ca,cb])=>{const cnv=document.createElement('canvas');cnv.width=pw*S;cnv.height=ph*S;const G=cnv.getContext('2d');G.fillStyle='#fff';G.fillRect(0,0,cnv.width,cnv.height);G.scale(S*k,S*k);
   const ox=m/k,oy=m/k;let y0=oy;const rowsHere=[];if(pi>0&&repRows.length)repRows.forEach(r=>rowsHere.push(r));for(let r=ra;r<=rb;r++)rowsHere.push(r);
   const ys={};let yy=oy;for(const r of rowsHere){ys[r]=ys[r]??yy;yy+=rh(r)}
   const Xp=c=>ox+sh._cx[c]-sh._cx[ca];
   /* draw each run of consecutive rows (repeated rows, then the page's own) */let runs=[];let cur_=null;for(const r of rowsHere){if(cur_&&r==cur_[1]+1)cur_[1]=r;else{cur_=[r,r];runs.push(cur_)}}
   let yAt=oy;for(const[a,b]of runs){const base=yAt;const Yp=r=>base+sh._ry[r]-sh._ry[a];G.save();G.beginPath();G.rect(ox-1,base-1,Xp(cb+1)-ox+2,Yp(b+1)-base+2);G.clip();paint(G,sh,a,b,ca,cb,Xp,Yp,1,{grid:!!P.grid});
    for(const ch of sh.charts){const im=chartImgs[ch.id];if(!im)continue;const cy=Yp(a)+(ch.y-sh._ry[a]);G.drawImage(im,Xp(ca)+(ch.x-sh._cx[ca]),cy,ch.w,ch.h)}G.restore();yAt=Yp(b+1)}
   G.setTransform(S,0,0,S,0,0);G.fillStyle='#666';G.font='10px Arial';G.textAlign='center';G.fillText(`${sh.name} — page ${out.length+1}`,pw/2,ph-20);out.push(cnv)}))}finally{zoom=savedZ}
  return out}
 async function savePDF(){const ps=await pages();if(!ps.length)return info('Save as PDF','There is nothing on this sheet to print.');const P=sh.print||{};const[pw0,ph0]=PAGE[P.size||'Letter']||PAGE.Letter;const pw=P.land?ph0:pw0,ph=P.land?pw0:ph0;
  savePdf((file?file.at(-1).replace(/\.[^.]*$/,''):'BOOK1')+'.PDF',pdfFromCanvases(ps,pw*.75,ph*.75))}
 async function printDlg(){const ps=await pages();if(!ps.length)return info('Print','There is nothing on this sheet to print.');const P=sh.print||{};const html=`<style>@page{size:${P.size=='A4'?'A4':P.size=='Legal'?'legal':'letter'} ${P.land?'landscape':'portrait'};margin:0}body{margin:0}img{width:100%;display:block;page-break-after:always}</style>`+ps.map(c=>`<img src='${c.toDataURL('image/png')}'>`).join('');
  if(typeof wpPrint=='function'&&wpPrint(W,html,{size:P.size||'Letter',land:!!P.land,title:file?file.at(-1):'Book1'}))return;
  try{const fr=document.createElement('iframe');fr.style.cssText='position:fixed;right:0;bottom:0;width:0;height:0;border:0';fr.srcdoc=html;document.body.appendChild(fr);fr.onload=()=>{setTimeout(()=>{try{fr.contentWindow.print()}catch(e){}setTimeout(()=>fr.remove(),60000)},300)}}catch(e){info('Print','Printing is not available.')}}
 /* ================= toolbar, status, menus ================= */
 function status(m){if(m)q('.sp-st').textContent=m;else q('.sp-st').textContent=edit?(edit.mode=='enter'?'Enter':'Edit'):painter?'Format painter: click or select cells':clip&&!clip.done?(clip.cut?'Select where to move to, then press Enter or Ctrl+V':'Select where to paste, then press Ctrl+V'):'Ready';
  let n=0,cnt=0,sum=0,mn=Infinity,mx=-Infinity;for(const gr of sel.rs){if(gr.r0==gr.r1&&gr.c0==gr.c1&&sel.rs.length==1)break;eachIn(sh,gr,cl=>{const v=spVal(cl);if(v!=null&&v!==''){cnt++}if(typeof v=='number'){n++;sum+=v;mn=Math.min(mn,v);mx=Math.max(mx,v)}})}
  const nf=cur().nf;q('.sp-agg').textContent=n?`Average: ${SPE.fmtValue(sum/n,nf)}   Count: ${cnt}   Sum: ${SPE.fmtValue(sum,nf)}   Min: ${SPE.fmtValue(mn,nf)}   Max: ${SPE.fmtValue(mx,nf)}`:cnt>1?`Count: ${cnt}`:''}
 let sayT=0;function say(m){q('.sp-st').textContent=m;clearTimeout(sayT);sayT=setTimeout(()=>status(),3500)}
 function updTb(){const st=cur();qa('.wp-tb button[data-a]').forEach(b=>{const a=b.dataset.a;b.classList.toggle('on',a=='b'&&!!st.b||a=='i'&&!!st.i||a=='u'&&!!st.u||a=='s'&&!!st.s||a=='al'&&st.ha=='left'||a=='ac'&&st.ha=='center'||a=='ar'&&st.ha=='right'||a=='wrap'&&!!st.wrap||a=='merge'&&!!mergeAt(sel.r,sel.c)||a=='paint'&&!!painter||a=='flt'&&!!sh.filter)});
  const ff=q('.sp-ff');ff.value=st.font||'Calibri';q('.sp-fs').value=String(st.sz||11);const nf=st.nf||'';const o=SP_NF.find(x=>x[1]==nf);q('.sp-nf').value=o?o[0]:nf?'*':'General'}
 const decs=nf=>{const m=/\.([0#]+)/.exec(nf||'');return m?m[1].length:0};
 function chDec(d){const v=spVal(cellAt(sel.r,sel.c));styleSel('Decimals',st=>{let nf=st.nf||'';if(!nf||nf=='General'){const t=typeof v=='number'?String(v):'0';const k=(t.split('.')[1]||'').length;nf='0'+(k?'.'+'0'.repeat(k):'')}const k=Math.max(0,decs(nf)+d);const re=/(\d|#)(\.[0#]+)?(?=[^0#.]*($|;|%|E|_|\)|"))/g;
   nf=nf.split(';').map(part=>part.replace(/0(\.[0#]+)?(?![0#])/, '0'+(k?'.'+'0'.repeat(k):''))).join(';');return{...st,nf}})}
 const act={b:()=>tgl('b'),i:()=>tgl('i'),u:()=>tgl('u'),s:()=>tgl('s'),
  fc:()=>X.color('Text colour',cur().fc||'#c00000',c=>{q('.sp-sw').style.background=c||'#000';setSt('Text colour',{fc:c||undefined})},'Automatic'),
  bg:()=>X.color('Fill colour',cur().bg||'#ffff00',c=>{q('.sp-sw2').style.background=c||'transparent';setSt('Fill colour',{bg:c||undefined})},'No fill'),
  bd:()=>{const b=q('[data-a=bd]').getBoundingClientRect();X.popMenu([['All borders','',()=>borders('all')],['Outside borders','',()=>borders('outside')],['Thick outside borders','',()=>borders('thick outside')],['Bottom border','',()=>borders('bottom')],['Top border','',()=>borders('top')],['Double bottom border','',()=>borders('double bottom')],['No border','',()=>borders('none')]],b.left,b.bottom)},
  al:()=>setSt('Align left',{ha:cur().ha=='left'?undefined:'left'}),ac:()=>setSt('Centre',{ha:cur().ha=='center'?undefined:'center'}),ar:()=>setSt('Align right',{ha:cur().ha=='right'?undefined:'right'}),
  wrap:()=>setSt('Wrap text',{wrap:cur().wrap?undefined:1}),merge:()=>mergeSel(true),
  cur:()=>setSt('Currency',{nf:'$#,##0.00'}),pct:()=>setSt('Percent',{nf:'0%'}),com:()=>setSt('Comma',{nf:'#,##0.00'}),dp:()=>chDec(1),dm:()=>chDec(-1),
  paint:()=>{painter=painter?null:{...cur()};updTb();status()},sum:autoSum,sa:()=>quickSort(1),sz:()=>quickSort(-1),flt:toggleFilter,chart:()=>chartDlg(),undo,redo};
 qa('.wp-tb button[data-a]').forEach(b=>{b.onmousedown=e=>e.preventDefault();b.onclick=()=>{if(edit&&!commitEdit(0,0))return;act[b.dataset.a]();sc.focus({preventScroll:true})}});
 q('.sp-ff').onchange=e=>{setSt('Font',{font:e.target.value=='Calibri'?undefined:e.target.value});sc.focus()};q('.sp-fs').onchange=e=>{setSt('Font size',{sz:+e.target.value==11?undefined:+e.target.value});sc.focus()};
 q('.sp-nf').onchange=e=>{const v=e.target.value;if(v=='*'){fmtDlg();updTb();return}const o=SP_NF.find(x=>x[0]==v);setSt('Number format',{nf:o[1]||undefined});sc.focus()};
 q('.sp-zr').oninput=e=>setZoom(+e.target.value/100);
 function setZoom(z){const r=sel.r,c=sel.c;zoom=Math.max(.5,Math.min(2,z));q('.sp-zr').value=Math.round(zoom*100);q('.sp-zt').textContent=Math.round(zoom*100)+'%';sizeSpacer();scrollInto(r,c);ol.querySelectorAll('.sp-ch').forEach(x=>x.dataset.k='');redraw()}
 gw.addEventListener('wheel',e=>{if(e.ctrlKey){e.preventDefault();setZoom(zoom+(e.deltaY<0?.1:-.1))}},{passive:false});
 function freeze(r,c){begin('Freeze panes');snap(sh);sh.freeze={r,c};sc.scrollTop=0;sc.scrollLeft=0;commit({quiet:1})}
 const MENUS=()=>({
  File:[['New','Ctrl+N',()=>sheetApp()],['New from template…','',newFromTemplate],['Open…','Ctrl+O',openDlg],['Save','Ctrl+S',()=>doSave()],['Save as… (.RSHEET, .XLSX, .CSV)','',()=>doSave(true)],'-',['Import CSV into a new sheet…','',importCSV],['Export this sheet as CSV…','',exportCSV],['Export as Excel workbook (.xlsx)…','',exportXlsx],'-',['Page setup…','',pageSetup],['Print…','Ctrl+P',printDlg],['Save as PDF…','',savePDF],'-',['Close','',()=>W.close()]],
  Edit:[['Undo'+(undoS.length?' '+undoS.at(-1).label.toLowerCase():''),'Ctrl+Z',undo,false,!undoS.length],['Redo','Ctrl+Y',redo,false,!redoS.length],'-',['Cut','Ctrl+X',()=>copySel(true)],['Copy','Ctrl+C',()=>copySel(false)],['Paste','Ctrl+V',()=>pasteMenu('all')],['Paste values only','',()=>pasteMenu('values')],['Paste formats only','',()=>pasteMenu('formats')],['Paste formulas only','',()=>pasteMenu('formulas')],['Paste transposed','',()=>pasteMenu('all',true),false,!clip],'-',['Fill down','Ctrl+D',()=>fillDir('down')],['Fill right','Ctrl+R',()=>fillDir('right')],['Clear contents','Delete',()=>clearSel('c')],['Clear formats','',()=>clearSel('f')],['Clear all','',()=>clearSel('a')],['Delete cells…','Ctrl+-',delDlg],'-',['Select all','Ctrl+A',selAll],['Find…','Ctrl+F',()=>findDlg(false)],['Replace…','Ctrl+H',()=>findDlg(true)],['Go to…','Ctrl+G',gotoDlg]],
  View:[['Formula bar','',()=>{showBar=!showBar;q('.sp-fb').style.display=showBar?'':'none'},showBar],['Grid lines','',()=>{begin('Grid lines');snap(sh);sh.grid=sh.grid===false;commit({quiet:1})},sh.grid!==false],['Show formulas','Ctrl+`',()=>{showF=!showF;redraw()},showF],'-',['Freeze panes (above and left of the selected cell)','',()=>freeze(sel.r,sel.c)],['Freeze top row','',()=>freeze(1,0)],['Freeze first column','',()=>freeze(0,1)],['Unfreeze panes','',()=>freeze(0,0),false,!sh.freeze.r&&!sh.freeze.c],'-',...[50,75,100,125,150,200].map(z=>['Zoom '+z+'%','',()=>setZoom(z/100),Math.round(zoom*100)==z]),'-',['Show Notey, the assistant','',()=>notey&&notey.show(!notey.visible()),notey&&notey.visible()]],
  Insert:[['Cells…','Ctrl+Shift++',insDlg],['Rows','',()=>insRC('r',1)],['Columns','',()=>insRC('c',1)],['Sheet','',()=>addSheet(book.sheets.indexOf(sh)+1)],'-',['Chart…','',()=>chartDlg()],['Function…','',funcDlg],['Comment…','',commentDlg],['Name…','',nameDlg],'-',['Today\'s date','Ctrl+;',()=>stamp('date')],['The time now','Ctrl+Shift+:',()=>stamp('time')]],
  Format:[['Cells…','Ctrl+1',fmtDlg],['Cell styles…','',styleDlg],'-',['Bold','Ctrl+B',()=>tgl('b')],['Italic','Ctrl+I',()=>tgl('i')],['Underline','Ctrl+U',()=>tgl('u')],['Strikethrough','Ctrl+5',()=>tgl('s')],'-',...SP_NF.slice(0,8).map(([n,c])=>[n,'',()=>setSt('Number format',{nf:c||undefined}),(cur().nf||'')==c]),['More decimals','',()=>chDec(1)],['Fewer decimals','',()=>chDec(-1)],'-',['Row height…','',()=>sizeDlg('r')],['Column width…','',()=>sizeDlg('c')],['Autofit column width','',()=>autofit('c')],['Autofit row height','',()=>autofit('r')],['Hide rows','',()=>hideRC('r',true)],['Unhide rows','',()=>hideRC('r',false)],['Hide columns','',()=>hideRC('c',true)],['Unhide columns','',()=>hideRC('c',false)],'-',['Merge and centre','',()=>mergeSel(true),!!mergeAt(sel.r,sel.c)],['Wrap text','',act.wrap,!!cur().wrap],'-',['Conditional formatting…','',cfDlg],['Format painter','',act.paint,!!painter]],
  Data:[['Sort A to Z','',()=>quickSort(1)],['Sort Z to A','',()=>quickSort(-1)],['Sort…','',sortDlg],'-',['AutoFilter','',toggleFilter,!!sh.filter],['Clear all filters','',()=>{if(!sh.filter)return;begin('Filter');snap(sh);sh.filter.cr={};commit()},false,!sh.filter],'-',['Data validation…','',dvDlg],['Remove duplicates','',removeDups],['Text to columns…','',textToCols],'-',['Recalculate now','F9',()=>{book._dep=null;SPE.recalc(book,null);vers++;cfCache=null;redraw()}]],
  Tools:[['Insert function…','',funcDlg],['Names…','',nameDlg],['Check for errors','',errCheck],'-',['Statistics of this workbook…','',statsDlg]],
  Help:[['Ask Notey…','',()=>notey&&notey.ask()],['Functions…','',funcDlg],['Keyboard shortcuts','',()=>info('Keyboard shortcuts',`<table class='wp-stt'>${[['Arrows, Tab, Enter','Move'],['Shift + arrows','Select'],['Ctrl + arrows','To the edge of the data'],['Ctrl+Home / End','To A1 / the last cell'],['F2','Edit the cell'],['Esc','Cancel the edit'],['Alt+Enter','New line in a cell'],['Ctrl+Enter','The entry in every selected cell'],['F4','$ in a reference'],['Ctrl+C / X / V','Copy / cut / paste'],['Ctrl+Z / Y','Undo / redo'],['Ctrl+D / R','Fill down / right'],['Ctrl+B / I / U','Bold / italic / underline'],['Ctrl+1','Format cells'],['Ctrl+; / Ctrl+:','Date / time'],['Alt+=','AutoSum'],['Ctrl+F / H','Find / replace'],['Ctrl+G','Go to'],['Ctrl+Space / Shift+Space','Select the column / row'],['Ctrl+`','Show formulas'],['F9','Recalculate']].map(([a,b])=>`<tr><td>${a}</td><td>${b}</td></tr>`).join('')}</table>`)],['About SpreadsheetPro','',()=>info('SpreadsheetPro 1.0',`The spreadsheet of Rudolf OS ${offEsc(typeof RVER!='undefined'?RVER:'')}, a companion to WordPro and PresentPro. ${Object.keys(SPE.F).length} functions, charts, conditional formatting, and Excel files.`)]]});
 function errCheck(){const errs=[];for(const s of book.sheets)for(const[k,cl]of s.cells){const v=spVal(cl);if(SPE.isE(v))errs.push([s,k,v.e])}if(!errs.length)return info('Check for errors','No cell shows an error.');
  dialog('Check for errors',`<div style='max-height:260px;overflow:auto'>${errs.slice(0,200).map(([s,k,e],i)=>`<div><a href='#' data-i='${i}'>${offEsc(s.name)}!${SPE.a1(...spRC(k))}</a> ${e} <span style='color:#666'>${offEsc({'#DIV/0!':'a division by zero','#REF!':'a reference to a cell that was deleted','#NAME?':'an unknown function or name','#VALUE!':'text where a number is needed','#N/A':'a value that was not found','#NUM!':'a number out of range','#CIRCULAR!':'the formula depends on its own result'}[e]||'')}</span></div>`).join('')}</div>`,[['Close']],d=>d.querySelectorAll('a').forEach(a=>a.onclick=ev=>{ev.preventDefault();const[s,k]=errs[+a.dataset.i];showSheet(book.sheets.indexOf(s));const[r,c]=spRC(k);select(r,c);scrollInto(r,c)}))}
 function statsDlg(){let cells=0,f=0;for(const s of book.sheets){cells+=s.cells.size;for(const c of s.cells.values())if(c.f!=null)f++}info('Statistics',`Sheets: ${book.sheets.length}<br>Cells with something: ${cells.toLocaleString()}<br>Formulas: ${f.toLocaleString()}<br>Charts: ${book.sheets.reduce((a,s)=>a+s.charts.length,0)}<br>Names: ${Object.keys(book.names).length}`)}
 /* ================= start ================= */
 /* ================= Notey: what he knows about spreadsheets ================= */
 const ERRX={'#DIV/0!':'a number is divided by zero (or by an empty cell)','#REF!':'the formula points to cells that were deleted','#NAME?':'a function or name is misspelled, or text is missing its quotation marks','#VALUE!':'text is used where a number is needed','#N/A':'a lookup did not find the value','#NUM!':'a number is out of range (such as the square root of a negative number)','#CIRCULAR!':'the formula depends on its own result','#NULL!':'two ranges that do not meet'};
 const goCell=(s_,r,c)=>{if(s_!==sh)showSheet(book.sheets.indexOf(s_));select(r,c);scrollInto(r,c);sc.focus({preventScroll:true})};
 function explain(){const cl=cellAt(sel.r,sel.c),a=SPE.a1(sel.r,sel.c);if(!cl||(cl.f==null&&(cl.v==null||cl.v===''))){notey.say(`<b>${a}</b> is empty. Type a number, some text, or a formula starting with =.`,[['OK',notey.hide]]);return}
  if(cl.f==null){const v=cl.v,t=typeof v=='number'?(cl.st&&/[yd]/i.test(cl.st.nf||'')?'a date':'a number'):typeof v=='boolean'?'TRUE or FALSE':'text';notey.say(`<b>${a}</b> holds ${t}: <b>${offEsc(spText(cl))}</b>${typeof v=='string'&&/^-?[\d,]+(\.\d+)?$/.test(v.trim())?'<br>It looks like a number but is stored as text, so sums leave it out.':''}`,[['OK',notey.hide]]);return}
  let ast=null;try{ast=SPE.parse(cl.f.slice(1))}catch(e){}const fns=new Set(),refs=[];const walk=n=>{if(!n||typeof n!='object')return;if(n.t=='fn')fns.add(n.f);if(n.t=='ref'||n.t=='rng')refs.push(n);(n.args||[]).forEach(walk);walk(n.l);walk(n.r);if(n.a&&typeof n.a=='object')walk(n.a)};walk(ast);
  const parts=[...fns].map(f=>`<li><b>${f}</b>: ${offEsc((SPE.D[f]||'').split(':').slice(1).join(':').trim()||'a function')}</li>`);
  refs.slice(0,4).forEach(n=>{const g=spParseRange(n.t=='ref'?n.a:n.a+':'+n.b);const s_=n.sheet?book.sheets.find(x=>x.name.toUpperCase()==n.sheet.toUpperCase()):sh;if(!g||!s_)return;let cnt=0,num=0,sum=0;if((g.r1-g.r0+1)*(g.c1-g.c0+1)<=20000)for(let r=g.r0;r<=g.r1;r++)for(let c=g.c0;c<=g.c1;c++){const v=spVal(s_.cells.get(spK(r,c)));if(v!=null&&v!=='')cnt++;if(typeof v=='number'){num++;sum+=v}}
   parts.push(`<li><b>${offEsc((n.sheet?n.sheet+'!':'')+spRangeStr(g))}</b>: ${g.r0==g.r1&&g.c0==g.c1?(cnt?'holds '+offEsc(spText(s_.cells.get(spK(g.r0,g.c0)))):'is empty'):`${(g.r1-g.r0+1)*(g.c1-g.c0+1)} cells, ${num} with numbers${num?', adding up to '+SPE.fmtValue(sum):''}`}</li>`)});
  const v=spVal(cl);const res=SPE.isE(v)?`It shows <b>${v.e}</b> because ${ERRX[v.e]||'of an error'}.`:`The result is <b>${offEsc(spText(cl))}</b>.`;
  notey.say(`<b>${a}</b>: <code>${offEsc(cl.f)}</code><ul style='margin:4px 0;padding-left:18px'>${parts.join('')}</ul>${res}`,[['Insert a function…',()=>{notey.hide();funcDlg()}],['Thanks!',notey.hide]])}
 function spScan(){const F=[];const errs=[],txtNum=[],hashes=new Set(),emptyRefs=[];
  for(const s_ of book.sheets)for(const[k,cl]of s_.cells){const v=spVal(cl);const[r,c]=spRC(k);if(SPE.isE(v))errs.push([s_,r,c,v.e]);else if(cl.f==null&&typeof v=='string'&&/^-?(\d{1,3}(,\d{3})+|\d+)(\.\d+)?$/.test(v.trim())&&!(cl.st&&/@/.test(cl.st.nf||'')))txtNum.push([s_,r,c])}
  if(errs.length){const e=errs[0];const fixable=errs.filter(x=>book.sheets.includes(x[0])&&(x[0].cells.get(spK(x[1],x[2]))||{}).f!=null&&!/^=IFERROR\(/i.test(x[0].cells.get(spK(x[1],x[2])).f));
   F.push({k:'err',msg:`${errs.length==1?'A cell shows':errs.length+' cells show'} an error. <b>${offEsc(e[0].name)}!${SPE.a1(e[1],e[2])}</b> shows <b>${e[3]}</b> because ${ERRX[e[3]]||'of an error'}.`,fix:fixable.length&&/DIV|N\/A/.test(e[3])?'Show a blank instead':null,run:()=>{begin('IFERROR');let n=0;for(const[s_,r,c,ee]of fixable){if(!/DIV|N\/A/.test(ee))continue;const cl=s_.cells.get(spK(r,c));setCell(s_,r,c,{...cl,f:'=IFERROR('+cl.f.slice(1)+',"")',ast:undefined,val:undefined});n++}commit();return n},show:()=>goCell(e[0],e[1],e[2])})}
  if(txtNum.length){const e=txtNum[0];F.push({k:'textnum',msg:`${txtNum.length==1?'A number is':txtNum.length+' numbers are'} stored as text (such as <b>${SPE.a1(e[1],e[2])}</b>), so sums and charts leave ${txtNum.length==1?'it':'them'} out.`,fix:'Make them numbers',run:()=>{begin('Text to numbers');for(const[s_,r,c]of txtNum){const cl=s_.cells.get(spK(r,c));setCell(s_,r,c,{...cl,v:+String(cl.v).replace(/,/g,'')})}commit();return txtNum.length},show:()=>goCell(e[0],e[1],e[2])})}
  /* a formula unlike the ones above and below it in its column */
  for(const s_ of book.sheets){const rel=new Map();for(const[k,cl]of s_.cells)if(cl.f!=null){const[r,c]=spRC(k);rel.set(k,SPE.copyFormula(cl.f,50000-r,350-c))}
   let found=null;for(const[k,f]of rel){const[r,c]=spRC(k);const up=rel.get(spK(r-1,c)),dn=rel.get(spK(r+1,c));if(up&&dn&&up==dn&&f!=up&&!/#REF/.test(up)){found=[r,c,SPE.copyFormula(up,r-50000,c-350)];break}}
   if(found){const[r,c,nf]=found;F.push({k:'odd',msg:`The formula in <b>${offEsc(s_.name)}!${SPE.a1(r,c)}</b> is different from the ones above and below it. Is that on purpose?`,fix:'Make it match',run:()=>{begin('Fix formula');const cl=s_.cells.get(spK(r,c));setCell(s_,r,c,{...cl,f:nf,ast:undefined});commit();return 1},show:()=>goCell(s_,r,c)});break}}
  /* a SUM that stops before the numbers do */
  for(const s_ of book.sheets){let hit=null;for(const[k,cl]of s_.cells){if(cl.f==null)continue;const m=/^=SUM\(([A-Z]{1,3})(\d+):([A-Z]{1,3})(\d+)\)$/.exec(cl.f);if(!m||m[1]!=m[3])continue;const[r,c]=spRC(k);const col=SPE.colNum(m[1]),end=+m[4]-1;if(col!=c||end>=r-1)continue;let ok=true;for(let rr=end+1;rr<r;rr++)if(typeof spVal(s_.cells.get(spK(rr,c)))!='number'){ok=false;break}if(ok){hit=[r,c,m];break}}
   if(hit){const[r,c,m]=hit;F.push({k:'sumshort',msg:`The total in <b>${SPE.a1(r,c)}</b> adds up to row ${m[4]}, but there are numbers down to row ${r}. They are left out.`,fix:'Include them',run:()=>{begin('Fix total');const cl=s_.cells.get(spK(r,c));setCell(s_,r,c,{...cl,f:`=SUM(${m[1]}${m[2]}:${m[1]}${r})`,ast:undefined});commit();return 1},show:()=>goCell(s_,r,c)});break}}
  /* numbers too wide for their column (#####) */
  geom(sh);mctx.font=fontOf({});for(const[k,cl]of sh.cells){const v=spVal(cl);if(typeof v!='number')continue;const[r,c]=spRC(k);if(r>2000)continue;mctx.font=fontOf(cl.st);const w=(sh._cx[c+1]-sh._cx[c])*zoom;if(w>0&&mctx.measureText(spText(cl)).width>w-6*zoom)hashes.add(c)}
  if(hashes.size){const cs=[...hashes].slice(0,20);F.push({k:'hash',msg:`Some numbers are too wide for their column and show as <b>#####</b> (column${cs.length>1?'s':''} ${cs.map(SPE.colName).join(', ')}).`,fix:'Widen the columns',run:()=>{begin('Autofit');snap(sh);for(const c of cs){let w=0;for(const[k,cl]of sh.cells)if(k%SPM==c){mctx.font=fontOf(cl.st);w=Math.max(w,mctx.measureText(spText(cl)).width/zoom)}sh.cw[c]=Math.min(600,Math.ceil(w+12))}commit();return cs.length}})}
  /* a long table with headings that scroll away */
  const u=spUsed(sh);if(u.r1>=40&&!sh.freeze.r){const R=region();if(R.r1-R.r0>=30&&looksHeader(R)&&R.r0<10)F.push({k:'freeze',tip:true,msg:`This table is ${R.r1-R.r0} rows long. Freezing its headings keeps them in view as you scroll.`,fix:'Freeze the headings',run:()=>{freeze(R.r0+1,0);return 1}})}
  return F}
 function spAuto(ok){const L=lastChange;if(!L||L.sheet!==sh)return null;const r=sel.r,c=sel.c;
  /* the cell just typed shows an error */for(const[s_,k]of L.cells||[]){const cl=s_.cells.get(k);const v=spVal(cl);if(SPE.isE(v)&&ok('err1')){const[rr,cc]=spRC(k);return{k:'err1',msg:`<b>${SPE.a1(rr,cc)}</b> shows <b>${v.e}</b>: ${ERRX[v.e]||'there is an error in the formula'}.`,show:()=>{goCell(s_,rr,cc);explain()},fix:null}}}
  /* a column of numbers with no total under it */const col=[];for(let rr=Math.max(0,r-1);rr>=0;rr--){const v=spVal(cellAt(rr,c));if(typeof v=='number'&&!(cellAt(rr,c)||{}).f)col.push(rr);else break}
  if(col.length>=5&&!cellAt(r,c)&&ok('sum'))return{k:'sum',msg:`That’s ${col.length} numbers in a column. Want a total under them?`,fix:'Add a total',run:()=>{begin('AutoSum');setCell(sh,r,c,{f:`=SUM(${SPE.a1(col.at(-1),c)}:${SPE.a1(col[0],c)})`,st:(cellAt(col[0],c)||{}).st});commit();return 1}};
  /* a table with headings and numbers, and no chart yet */const R=region();if(!sh.charts.length&&R.r1-R.r0>=3&&R.c1>R.c0&&looksHeader(R)&&ok('chart')){let nums=0;for(let rr=R.r0+1;rr<=R.r1;rr++)for(let cc=R.c0;cc<=R.c1;cc++)if(typeof spVal(cellAt(rr,cc))=='number')nums++;
   if(nums>=4)return{k:'chart',msg:'This looks like a table of numbers with headings. A chart could show it at a glance.',fix:'Make a chart',run:()=>{sel.rs=[{...R}];chartDlg();return 0}}}
  return null}
 notey=W.notey=offNotey(W,X,{app:'SpreadsheetPro',check:'✍️ Check my spreadsheet',example:'how do I sort a column?',tryWords:'“chart”, “sort”, “total”, “freeze” or a function such as “average”',
  kb:SP_FAQ.map(([k,t,h])=>[k,`<b>${t}</b><br>${h}`,{chart:()=>chartDlg(),sort:sortDlg,filter:toggleFilter,format:fmtDlg,colour:cfDlg,freeze:()=>freeze(sel.r,sel.c),print:pageSetup,template:newFromTemplate,vlookup:funcDlg,formula:funcDlg,merge:()=>mergeSel(true),sheet:()=>addSheet()}[Object.keys({chart:1,sort:1,filter:1,format:1,colour:1,freeze:1,print:1,template:1,vlookup:1,formula:1,merge:1,sheet:1}).find(x=>k.split(' ')[0]==x)]||null]),
  find:w=>Object.keys(SPE.F).map(n=>[n,w.filter(x=>n.toLowerCase()==x||(SPE.D[n]||'').toLowerCase().includes(x)).length+(w.includes(n.toLowerCase())?3:0)]).filter(a=>a[1]>0).sort((a,b)=>b[1]-a[1]).slice(0,3).map(([n])=>[`<b>${n}</b>: ${offEsc((SPE.D[n]||'').split(':').slice(1).join(':').trim())}`,'Use it',()=>{if(edit)cancelEdit();startEdit('='+n+'(','edit');hintUpd()}]),
  tips:['Type <b>=</b> and click cells to build a formula; <b>F4</b> adds $ signs so the reference stays put when copied.','Double-click the small square at the corner of the selection to fill a formula down as far as the data goes.','<b>Ctrl+;</b> puts in today’s date, <b>Ctrl+Shift+:</b> the time.','Select numbers and look at the status bar: it shows their sum, average, count, smallest and largest.','<b>Alt+=</b> adds up the numbers above the cell.','<b>Ctrl+Arrow</b> jumps to the end of your data; add <b>Shift</b> to select on the way.','Right-click a sheet tab to give it a colour, or drag it to move it.','<b>Data › AutoFilter</b> puts a ▾ on each heading to show only the rows you want.','Copy a range here and paste it in WordPro or PresentPro: it becomes a table there.','Right-click a chart to copy it with its data for PresentPro.','<b>Format › Conditional formatting</b> can colour the highest values, or add data bars.','<b>Ctrl+`</b> shows every formula instead of its result.'],
  scan:spScan,auto:spAuto,empty:()=>!book.sheets.some(s_=>s_.cells.size),templates:newFromTemplate,save:()=>doSave(),nothing:'This spreadsheet is still empty. Type something in, or start from a template!',
  summary:()=>{let c=0,f=0;for(const s_ of book.sheets){c+=s_.cells.size;for(const x of s_.cells.values())if(x.f!=null)f++}return`<b>${c}</b> cells · <b>${f}</b> formulas · <b>${book.sheets.length}</b> sheet${book.sheets.length>1?'s':''}`},
  menu:[['🔍 Explain this cell',explain],['📊 Chart this table',()=>chartDlg()],['📄 Start from a template',newFromTemplate]],
  greet:['Hi! I’m <b>Notey</b>. Starting a new spreadsheet? I can set one up from a template, or explain any formula you write.','Hi! I’m <b>Notey</b>. I’ll keep an eye out for errors and things that could be easier. Click me any time.']});
 lastCount=countCells();
 const autos=offAutosave('SpreadsheetPro',W,()=>({dirty,name:file?file.at(-1):'Book1',data:()=>spToJSON(book)}));
 const oc2=W.onclose;W.onclose=()=>{if(dirty&&!W.__force){confirmDiscard(()=>{W.__force=1;W.close()});return false}return oc2?oc2():undefined};
 W.sp={get book(){return book},get sheet(){return sh},get sel(){return sel},select:(a,ext_)=>{const R=spParseRange(a);if(R){sel.rs=[R];sel.r=R.r0;sel.c=R.c0;sel.ar=R.r0;sel.ac=R.c0;afterSel()}},type:(a,t)=>{cancelEdit();goTo(a);startEdit(t,'enter');commitEdit(0,0)},
  value:(a,s)=>{const p=SPE.parseA1(a);const S=s?book.sheets.find(x=>x.name==s):sh;return spVal(S.cells.get(spK(p.r,p.c)))},text:(a)=>{const p=SPE.parseA1(a);return spText(cellAt(p.r,p.c))},cell:a=>{const p=SPE.parseA1(a);return cellAt(p.r,p.c)},
  undo,redo,copy:()=>copySel(false),cut:()=>copySel(true),paste:w=>pasteClip(w||'all'),fill:(a,b,d)=>doFill(spParseRange(a),spParseRange(b),d),insRC,sortRange:(a,keys,h)=>sortRange(spParseRange(a),keys,h),toggleFilter,filter:(c,vals)=>{begin('Filter');snap(sh);sh.filter.cr[c]=vals;commit()},
  chart:o=>{begin('Insert chart');snap(sh);const c={id:Date.now()%1e9,type:'column',x:300,y:20,w:400,h:260,legend:true,...o};sh.charts.push(c);commit();return c},chartSpec:c=>chartSpec(c),addSheet,showSheet,renameSheet:(i,n)=>renameSheet(i,n),setBook,doSave,loadPath,
  style:(o)=>setSt('Style',o),cf:R=>{begin('CF');snap(sh);sh.cf.push(R);commit()},dv:R=>{begin('DV');snap(sh);sh.dv.push(R);commit()},merge:()=>mergeSel(true),freeze,setZoom,pages,draw,markDirty:()=>{dirty=true;ttl()},
  get dirty(){return dirty},get file(){return file},get zoom(){return zoom},key:(k,o)=>sc.dispatchEvent(new KeyboardEvent('keydown',{key:k,bubbles:true,cancelable:true,...(o||{})})),
  get editing(){return!!edit},editor:()=>ed,bar:()=>fx,colAtX,rowAtY,Xc,Yr,get clip(){return clip},fmtDlg,cfDlg,dvDlg,sortDlg,findDlg,chartDlg,pageSetup,newFromTemplate,commentDlg,funcDlg,nameDlg,savePDF,printDlg,exportXlsx};
 ttl();setTimeout(()=>{resize();showSheet(book.active||0);sc.focus({preventScroll:true})},0);
 if(path)setTimeout(()=>loadPath(path),0);else if(!seedBook)setTimeout(()=>offRecover('SpreadsheetPro',X,(data,name)=>{try{setBook(spFromJSON(data));dirty=true;ttl();say('Recovered '+name)}catch(e){}}),400);
 if(seedBook){SPE.recalc(book,null)}
 return W}

LAZY_DONE.sheet=1;
