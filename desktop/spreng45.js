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
 function expr(min){let l=unary();for(;;){const o=peek();if(!o||o.k!='op'||!(o.v in BIN)||BIN[o.v]<min)break;next();const r=expr(o.v=='^'?BIN[o.v]:BIN[o.v]+1);l={t:'bin',o:o.v,l,r}}return l}
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
const sOpt=(a,i,d,ctx)=>a[i]?str(scalar(a[i](),ctx)):d;
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
function calc(book){const gen=++GEN;
 const byName=nm=>book.sheets.find(s=>s.name.toUpperCase()==String(nm).toUpperCase());
 const names=book.names||{};
 function value(s,r,c){const cell=s.cells.get(r*MAXC+c);if(!cell)return null;if(cell.f==null)return cell.v;
  if(cell._g===gen)return cell.val;if(cell._b===gen){cell._g=gen;cell.val=E.cyc;return E.cyc}cell._b=gen;
  let v;try{if(!cell.ast)cell.ast=parse(cell.f.slice(1));const cx={sheetByName:byName,names,sheet:s,r,c,value};v=scalar(ev(cell.ast,cx),cx);if(v&&v.arr)v=v.arr[0][0];if(v===undefined)v=null}catch(e){v=isE(e)?e:E.val}
  cell._b=0;if(cell._g===gen&&cell.val===E.cyc)v=E.cyc;cell._g=gen;cell.val=v;return v}
 /* row by row: references usually point up or left, so chains are short. The list of formula cells is kept on the
    sheet until a cell changes (the app sets sheet.fk=null when it changes cells) */
 for(const s of book.sheets){if(!s.fk||s.fkn!==s.cells.size){s.fk=[];for(const[k,cl]of s.cells)if(cl.f!=null)s.fk.push(k);s.fk.sort((a,b)=>a-b);s.fkn=s.cells.size}
  for(const k of s.fk)value(s,Math.floor(k/MAXC),k%MAXC)}
 return book}
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
return{parse,tokenize,calc,evalIn,fmtValue,fmtGeneral,colName,colNum,a1,parseA1,F,D,E,isE,MAXC,MAXR,toSerial,fromSerial,parseDate,copyFormula,shiftFormula,cmpVals}})();
