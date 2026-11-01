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
