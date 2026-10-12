/* ===================== Blitzen: Lua → BlockLua blocks, and the grammar that keeps answers block-shaped =====================
   blzLuaToBlocks(src) turns Lua into BlockLua Builder blocks ({t, v}); any line no block fits becomes a raw "lua" block,
   so every answer loads. BLZ_BLOCK_GRAMMAR (GBNF) limits a "make blocks" answer to lines that fit a block. */
function blzSplitTop(s,sep){const out=[];let d=0,q=null,cur='';for(let i=0;i<s.length;i++){const c=s[i];
  if(q){cur+=c;if(c=='\\'){cur+=s[++i]||'';continue}if(c==q)q=null;continue}
  if(c=='"'||c=="'"){q=c;cur+=c;continue}if('([{'.includes(c))d++;else if(')]}'.includes(c))d--;
  if(c==sep&&d==0){out.push(cur.trim());cur='';continue}cur+=c}out.push(cur.trim());return out}
/* brackets and strings still open at the end of s (so a statement continues on the next line) */
function blzOpen(s){let d=0,q=null,lc=false;for(let i=0;i<s.length;i++){const c=s[i];if(q){if(c=='\\'){i++;continue}if(c==q)q=null;continue}
  if(c=='-'&&s[i+1]=='-'){if(s.slice(i,i+4)=='--[['){const e=s.indexOf(']]',i+4);if(e<0)return 1;i=e+1;continue}break}
  if(s.slice(i,i+2)=='[['){const e=s.indexOf(']]',i+2);if(e<0)return 1;i=e+1;continue}
  if(c=='"'||c=="'"){q=c;continue}if('([{'.includes(c))d++;else if(')]}'.includes(c))d--}return d>0||q?1:0}
/* a call whose parentheses close at the end of the line: f(ARGS) -> ARGS */
function blzCallArgs(s,name){if(!s.startsWith(name+'('))return null;const inner=s.slice(name.length+1);let d=1,q=null;
  for(let i=0;i<inner.length;i++){const c=inner[i];if(q){if(c=='\\'){i++;continue}if(c==q)q=null;continue}if(c=='"'||c=="'"){q=c;continue}
   if(c=='(')d++;else if(c==')'){d--;if(d==0)return i==inner.length-1?inner.slice(0,i):null}}return null}
const BLZ_ID=/^[A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z_][A-Za-z0-9_]*)*$/;
function blzLine(l){
 let m;
 if(/^--\[\[/.test(l))return{t:'lua',v:[l]};
 if((m=/^--\s?(.*)$/.exec(l)))return{t:'rem',v:[m[1]]};
 if(l=='end')return{t:'end',v:[]};if(l=='else')return{t:'else',v:[]};if(l=='break')return{t:'brk',v:[]};
 if((m=/^elseif\s+(.+)\s+then$/.exec(l)))return{t:'elseif',v:[m[1]]};
 if((m=/^if\s+(.+)\s+then$/.exec(l))&&!/\bthen\b.*\bend$/.test(l))return{t:'if',v:[m[1]]};
 if((m=/^while\s+(.+)\s+do$/.exec(l)))return{t:'while',v:[m[1]]};
 if((m=/^for\s+_i\s*=\s*1\s*,\s*(.+?)\s+do$/.exec(l))&&!blzSplitTop(m[1],',')[1])return{t:'repeat',v:[m[1]]};
 if((m=/^for\s+([A-Za-z_]\w*)\s*=\s*(.+)\s+do$/.exec(l))){const p=blzSplitTop(m[2],',');if(p.length==2)return{t:'for',v:[m[1],p[0],p[1]]}}
 if((m=/^function\s+([A-Za-z_][\w.:]*)\s*\(([^)]*)\)$/.exec(l)))return{t:'func',v:[m[1],m[2].trim()]};
 if((m=/^return\s+(.+)$/.exec(l)))return{t:'ret',v:[m[1]]};
 let a;
 if((a=blzCallArgs(l,'print'))!==null)return{t:'print',v:[a]};
 if((a=blzCallArgs(l,'sys.beep'))!==null)return{t:'beep',v:[a]};
 if((a=blzCallArgs(l,'sys.open'))!==null)return{t:'openapp',v:[a]};
 if((a=blzCallArgs(l,'sys.write'))!==null){const p=blzSplitTop(a,',');if(p.length==2)return{t:'writef',v:p}}
 if((m=/^local\s+([A-Za-z_]\w*)\s*=\s*(.+)$/.exec(l)))return{t:'local',v:[m[1],m[2]]};
 if((m=/^([A-Za-z_][\w.]*)\s*=\s*io\.read\(\s*\)$/.exec(l)))return{t:'input',v:[m[1]]};
 if((m=/^([A-Za-z_][\w.]*)\s*=\s*(.+)$/.exec(l))&&BLZ_ID.test(m[1])&&!/^=/.test(m[2]))return{t:'set',v:[m[1],m[2]]};
 return{t:'lua',v:[l]}}
function blzLuaToBlocks(src){
 const lines=String(src).replace(/\r/g,'').split('\n');const out=[];let buf='';
 for(const raw of lines){const l=raw.trim();if(!l&&!buf)continue;
  buf=buf?buf+'\n'+raw:l;if(blzOpen(buf))continue;
  const b=buf;buf='';
  /* a statement over several lines (a table, a long call) stays one raw block */
  if(b.includes('\n')){out.push({t:'lua',v:[b]});continue}
  /* "a = 1; b = 2" on one line */
  const parts=blzSplitTop(b,';').filter(x=>x);if(parts.length>1){parts.forEach(p=>out.push(blzLine(p)));continue}
  out.push(blzLine(b))}
 if(buf)out.push({t:'lua',v:[buf]});
 return out}
/* how much of an answer fits real blocks (raw "lua" blocks don't count) */
function blzBlockFit(bl){if(!bl.length)return 0;return bl.filter(b=>b.t!='lua').length/bl.length}
/* GBNF for "make blocks" answers: one statement per line, each one a block shape (expressions are free text) */
const BLZ_BLOCK_GRAMMAR=String.raw`root ::= line+
line ::= ws stmt "\n"
ws ::= [ ]*
stmt ::= ( "print(" expr ")" | "local " name " = " expr | name " = io.read()" | name " = " expr
  | "for _i=1," expr " do" | "for " name "=" expr "," expr " do" | "while " expr " do" | "if " expr " then"
  | "elseif " expr " then" | "else" | "end" | "function " name "(" params ")" | "return " expr | "break"
  | "-- " text | "sys.beep(" expr ")" | "sys.open(" expr ")" | "sys.write(" expr ", " expr ")" )
name ::= [A-Za-z_] [A-Za-z0-9_.]*
params ::= ([A-Za-z_] [A-Za-z0-9_, ]*)?
expr ::= [^\n]+
text ::= [^\n]*`;
if(typeof module!='undefined')module.exports={blzLuaToBlocks,blzBlockFit,BLZ_BLOCK_GRAMMAR,blzSplitTop};
