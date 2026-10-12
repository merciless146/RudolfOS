/* ===== V25 Notey: limited-animation sticky note with a wide range of expressions =====
 * Drawn as simple flat shapes and animated in held poses at 8 frames per second (most poses
 * held for two or more frames), like hand-drawn limited animation. Notey can fold into a paper
 * airplane (after saving, or as a trick) and into an origami crane (word-count milestones). */
var NOTEY_FPS=8;
const NY={paper:'#ffe66d',shade:'#f2bd00',edge:'#c79a00',ink:'#5b4300',crease:'#d4a900'};
function nyEye(cx,cy,k,lk,side){const I=NY.ink,lx=lk[0],ly=lk[1];
 const open=(rx,ry,pr)=>`<ellipse cx='${cx}' cy='${cy}' rx='${rx}' ry='${ry}' fill='#fff' stroke='#444' stroke-width='2.5'/><circle cx='${cx+lx}' cy='${cy+2+ly}' r='${pr}' fill='#1f2937'/><circle cx='${cx+lx+2}' cy='${cy-1+ly}' r='1.7' fill='#fff'/>`;
 const lid=pts=>`<polygon points='${pts}' fill='${NY.paper}'/><path d='M${pts.split(' ').slice(2).reverse().join(' L')}' stroke='${I}' stroke-width='2.6' fill='none' stroke-linecap='round'/>`;
 const arc=d=>`<path d='${d}' stroke='${I}' stroke-width='3.4' fill='none' stroke-linecap='round'/>`;
 if(k=='wink')k=side=='L'?'happy':'open';
 switch(k){
  case 'wide':return open(13,15,3.6);
  case 'squint':return `<ellipse cx='${cx}' cy='${cy+2}' rx='11' ry='6' fill='#fff' stroke='#444' stroke-width='2.5'/><circle cx='${cx+lx}' cy='${cy+2}' r='4' fill='#1f2937'/>`;
  case 'half':return open(11,13,5.5)+lid(`${cx-13},${cy-15} ${cx+13},${cy-15} ${cx+13},${cy+1} ${cx-13},${cy+1}`);
  case 'sad':return open(11,13,5.5)+(side=='L'?lid(`${cx-13},${cy-15} ${cx+13},${cy-15} ${cx+13},${cy-12} ${cx-13},${cy-3}`):lid(`${cx-13},${cy-15} ${cx+13},${cy-15} ${cx+13},${cy-3} ${cx-13},${cy-12}`));
  case 'angry':return open(11,13,5)+(side=='L'?lid(`${cx-13},${cy-15} ${cx+13},${cy-15} ${cx+13},${cy-2} ${cx-13},${cy-10}`):lid(`${cx-13},${cy-15} ${cx+13},${cy-15} ${cx+13},${cy-10} ${cx-13},${cy-2}`));
  case 'happy':return arc(`M${cx-9} ${cy+3} Q${cx} ${cy-9} ${cx+9} ${cy+3}`);
  case 'closed':return arc(`M${cx-9} ${cy} Q${cx} ${cy+7} ${cx+9} ${cy}`);
  case 'line':return arc(`M${cx-10} ${cy+1} H${cx+10}`);
  case 'heart':return `<path d='M${cx} ${cy+10} C${cx-15} ${cy} ${cx-10} ${cy-13} ${cx} ${cy-4} C${cx+10} ${cy-13} ${cx+15} ${cy} ${cx} ${cy+10} Z' fill='#ef4444' stroke='#991b1b' stroke-width='2'/>`;
  case 'star':{let p=[];for(let i=0;i<10;i++){const r=i%2?5:12,a=-Math.PI/2+i*Math.PI/5;p.push((cx+r*Math.cos(a)).toFixed(1)+','+(cy+r*Math.sin(a)).toFixed(1))}return `<polygon points='${p.join(' ')}' fill='#facc15' stroke='#a16207' stroke-width='2' stroke-linejoin='round'/>`}
  case 'spiral':case 'spiral2':return `<g transform='rotate(${k=='spiral'?0:180} ${cx} ${cy})'>${arc(`M${cx} ${cy} a2 2 0 1 1 3 0 a5 5 0 1 1 -8 0 a8 8 0 1 1 13 1`)}</g>`;
  case 'dot':return `<circle cx='${cx}' cy='${cy}' r='3.4' fill='#1f2937'/>`;
  default:return open(11,13,5.5)}}
function nyBrows(k){const I=NY.ink;const B={normal:['M31 38 Q43 31 55 38','M71 38 Q83 31 95 38'],raised:['M31 31 Q43 23 55 31','M71 31 Q83 23 95 31'],angry:['M31 31 L55 40','M71 40 L95 31'],
  worried:['M31 40 L55 31','M71 31 L95 40'],oneup:['M31 38 Q43 33 55 38','M71 29 Q83 20 95 29'],flat:['M31 36 H55','M71 36 H95'],low:['M31 41 Q43 37 55 41','M71 41 Q83 37 95 41']}[k];
 return B?`<g stroke='${I}' stroke-width='3' stroke-linecap='round' fill='none'><path d='${B[0]}'/><path d='${B[1]}'/></g>`:''}
function nyMouth(k){const I=NY.ink,D='#7c2d12',s=d=>`<path d='${d}' fill='none' stroke='${I}' stroke-width='3.2' stroke-linecap='round' stroke-linejoin='round'/>`;
 switch(k){case 'small':return s('M54 82 Q64 89 74 82');case 'grin':return `<path d='M42 78 Q64 100 86 78 Z' fill='${D}' stroke='${I}' stroke-width='2.5' stroke-linejoin='round'/><path d='M54 90 Q64 84 74 90 Q64 95 54 90 Z' fill='#f87171'/>`;
  case 'laugh':return `<path d='M38 74 Q64 112 90 74 Z' fill='${D}' stroke='${I}' stroke-width='2.5' stroke-linejoin='round'/><path d='M50 94 Q64 84 78 94 Q64 102 50 94 Z' fill='#f87171'/>`;
  case 'o':return `<ellipse cx='64' cy='84' rx='5' ry='6' fill='${D}' stroke='${I}' stroke-width='2.4'/>`;case 'O':return `<ellipse cx='64' cy='86' rx='9' ry='11' fill='${D}' stroke='${I}' stroke-width='2.5'/>`;
  case 'yawn':return `<ellipse cx='64' cy='86' rx='8' ry='13' fill='${D}' stroke='${I}' stroke-width='2.5'/>`;case 'flat':return s('M50 84 H78');
  case 'frown':return s('M46 90 Q64 76 82 90');case 'wobble':return s('M46 86 q4.5 -5 9 0 t9 0 t9 0 t9 0');case 'smirk':return s('M50 84 Q68 90 80 77');
  case 'tongue':return s('M46 80 Q64 92 82 80')+`<path d='M64 86 q0 10 7 10 q7 0 7 -10 Z' fill='#f87171' stroke='${I}' stroke-width='2'/>`;
  case 'cat':return s('M48 82 q8 9 16 0 q8 9 16 0');case 'grit':return `<rect x='46' y='78' width='36' height='12' rx='4' fill='#fff' stroke='${I}' stroke-width='2.5'/><path d='M55 78 V90 M64 78 V90 M73 78 V90' stroke='${I}' stroke-width='1.6'/>`;
  default:return s('M45 80 Q64 94 83 80')}}
function nyExtras(list,t){const I=NY.ink;let o='';const ph=Math.floor(t/2)%2;
 const drop=(x,y,c)=>`<path d='M${x} ${y} q-5 8 0 11 q5 -3 0 -11 Z' fill='${c||'#7dd3fc'}' stroke='#0369a1' stroke-width='1.6'/>`;
 const heart=(x,y,s)=>`<path transform='translate(${x} ${y}) scale(${s})' d='M0 6 C-9 0 -6 -8 0 -3 C6 -8 9 0 0 6 Z' fill='#f43f5e'/>`;
 const spark=(x,y,s)=>`<path transform='translate(${x} ${y}) scale(${s})' d='M0 -8 L2 -2 L8 0 L2 2 L0 8 L-2 2 L-8 0 L-2 -2 Z' fill='#facc15' stroke='#a16207' stroke-width='1'/>`;
 const txt=(x,y,sz,c)=>`<text x='${x}' y='${y}' font-size='${sz}' font-weight='700' font-family='Georgia,serif' fill='${I}'>${c}</text>`;
 (list||[]).forEach(e=>{switch(e){
  case 'sweat':o+=drop(110,24+ph*3);break;case 'tear':o+=drop(36,70+ph*5);break;
  case 'hearts':o+=heart(110,18-ph*4,1.2)+heart(122,40-ph*3,.8);break;case 'sparkles':o+=spark(6,18,ph?1:.7)+spark(122,28,ph?.7:1)+spark(118,6,.6);break;
  case 'question':o+=txt(108,22-ph*2,24,'?');break;case 'exclaim':o+=txt(112,24,26,'!');break;
  case 'zzz':{const k=Math.floor(t/4)%3;o+=txt(98,30-k*3,14,'z')+(k>0?txt(108,18-k*2,11,'z'):'')+(k>1?txt(116,8,9,'z'):'');break}
  case 'dots':o+=[0,1,2].map(i=>i<=Math.floor(t/3)%3?`<circle cx='${98+i*9}' cy='12' r='3' fill='${I}'/>`:'').join('');break;
  case 'vein':o+=`<g stroke='#dc2626' stroke-width='3' fill='none' stroke-linecap='round' transform='translate(106 18) scale(${ph?1.1:.9})'><path d='M-7 -3 q4 0 4 -4 M3 -7 q0 4 4 4 M7 3 q-4 0 -4 4 M-3 7 q0 -4 -4 -4'/></g>`;break;
  case 'notes':o+=txt(104,28-ph*6,18,'♪')+txt(116,44-(1-ph)*6,15,'♫');break;
  case 'bulb':o+=`<g transform='translate(63 -10)'><circle r='9' fill='#fde047' stroke='${I}' stroke-width='2'/><rect x='-4' y='8' width='8' height='6' fill='#a8a29e' stroke='${I}' stroke-width='1.5'/>${ph?`<path d='M-15 -6 l-5 -3 M15 -6 l5 -3 M0 -13 v-6' stroke='#eab308' stroke-width='2.5' stroke-linecap='round'/>`:''}</g>`;break;
  case 'steam':o+=`<g fill='#e5e7eb' stroke='#9ca3af' stroke-width='1.5'><circle cx='${8-ph*3}' cy='${18-ph*4}' r='${5+ph}'/><circle cx='${120+ph*3}' cy='${18-ph*4}' r='${5+ph}'/></g>`;break;
  case 'stars':o+=[0,1,2].map(i=>{const a=(t*0.9+i*2.1);return spark(63+38*Math.cos(a),8+8*Math.sin(a),.6)}).join('');break;
  case 'phew':o+=`<g fill='#f3f4f6' stroke='#9ca3af' stroke-width='1.5'><circle cx='${94+ph*4}' cy='90' r='5'/><circle cx='${103+ph*6}' cy='87' r='3.5'/></g>`;break;
  case 'shine':o+=spark(26,22,ph?.8:.5);break}});return o}
function nyArms(k){const I=NY.ink,A={rest:['M14 70 Q2 78 6 94','M112 70 Q124 78 120 94'],wave1:['M14 70 Q2 78 6 94','M112 70 Q128 62 124 46'],wave2:['M14 70 Q2 78 6 94','M112 70 Q132 66 138 52'],
  up:['M14 66 Q2 54 6 38','M112 66 Q124 54 120 38'],hips:['M14 70 L2 80 L14 88','M112 70 L124 80 L112 88'],chin:['M14 70 Q2 78 6 94','M112 74 Q108 100 86 94'],shrug:['M14 72 Q0 70 2 58','M112 72 Q126 70 124 58'],
  cheeks:['M14 72 Q22 92 30 80','M112 72 Q104 92 96 80'],hug:['M14 72 Q34 102 58 96','M112 72 Q92 102 70 96'],pointup:['M14 70 Q2 78 6 94','M112 68 Q122 52 118 34'],flap1:['M14 66 Q0 60 -4 50','M112 66 Q126 60 130 50'],flap2:['M14 74 Q0 80 -4 90','M112 74 Q126 80 130 90']}[k||'rest']||[];
 return `<g stroke='${I}' stroke-width='3.2' stroke-linecap='round' stroke-linejoin='round' fill='none'>${A.map(d=>`<path d='${d}'/>`).join('')}</g>`}
function nyBody(k){const P=NY.paper,S=NY.shade,E=NY.edge,C=NY.crease,st=`stroke='${E}' stroke-width='3' stroke-linejoin='round'`;
 switch(k){
  case 'fA':return {paper:`<path d='M14 40 L63 14 L112 40 V120 H14 Z' fill='${P}' ${st}/><path d='M63 14 L14 40 L63 64 Z M63 14 L112 40 L63 64 Z' fill='${S}' ${st}/>`,face:'translate(63 80) scale(.62) translate(-63 -66)',limbs:1};
  case 'fB':return {paper:`<path d='M63 10 L96 52 L84 122 H42 L30 52 Z' fill='${P}' ${st}/><path d='M63 10 L30 52 L63 70 Z M63 10 L96 52 L63 70 Z' fill='${S}' ${st}/><path d='M63 70 V122' stroke='${C}' stroke-width='2'/>`,face:'translate(63 94) scale(.42) translate(-63 -66)'};
  case 'fC':return {paper:`<path d='M63 8 L82 58 L72 124 H54 L44 58 Z' fill='${S}' ${st}/><path d='M63 8 V124' stroke='${E}' stroke-width='2'/>`,face:'translate(63 92) scale(.3) translate(-63 -66)'};
  case 'plane':return {paper:`<path d='M10 54 L124 70 L34 73 Z' fill='${P}' ${st}/><path d='M34 73 L124 70 L44 90 Z' fill='${S}' ${st}/><circle cx='106' cy='68' r='2.6' fill='#1f2937'/><path d='M98 76 q4 3 8 0' stroke='${NY.ink}' stroke-width='2' fill='none' stroke-linecap='round'/>`,flat:1};
  case 'sq':return {paper:`<rect x='40' y='60' width='46' height='46' rx='2' fill='${S}' ${st}/><path d='M40 60 L86 106' stroke='${E}' stroke-width='2'/>`,face:'translate(63 84) scale(.36) translate(-63 -66)'};
  case 'diamond':return {paper:`<path d='M63 10 L116 66 L63 122 L10 66 Z' fill='${P}' ${st}/><path d='M63 10 V122 M10 66 H116' stroke='${C}' stroke-width='2'/>`,face:'translate(63 70) scale(.55) translate(-63 -66)'};
  case 'kite':return {paper:`<path d='M63 6 L92 64 L63 130 L34 64 Z' fill='${P}' ${st}/><path d='M63 6 L34 64 L63 76 Z M63 6 L92 64 L63 76 Z' fill='${S}' ${st}/>`,face:'translate(63 96) scale(.38) translate(-63 -66)'};
  case 'crane':case 'craneD':{const up=k=='crane';return {paper:`<path d='${up?'M63 76 L26 20 L54 86 Z':'M63 82 L14 106 L54 90 Z'}' fill='${P}' ${st}/><path d='M76 86 L112 42 L117 45 L84 90 Z' fill='${P}' ${st}/><path d='M50 88 L22 40 L29 37 L58 86 Z' fill='${P}' ${st}/><path d='M22 40 L11 50 L29 37 Z' fill='${S}' ${st}/>
    <path d='M44 88 L63 72 L86 88 L63 102 Z' fill='${S}' ${st}/><path d='${up?'M63 76 L100 22 L74 86 Z':'M63 82 L112 106 L72 90 Z'}' fill='${P}' ${st}/><circle cx='24' cy='42' r='1.8' fill='#1f2937'/>`,flat:1}}
  default:return {paper:`<path d='M14 14 H112 V94 L88 120 H14 Z' fill='${P}' ${st}/><rect x='15.5' y='15.5' width='95' height='13' fill='#ffd43b' opacity='.75'/><path d='M112 94 L88 120 V101 Q88 94 95 94 Z' fill='${S}' ${st}/><path d='M26 108 H70 M26 98 H58' stroke='${C}' stroke-width='2' stroke-linecap='round' opacity='.5'/>`,face:'',limbs:1,legs:1}}}
function nyRender(p,t){const b=nyBody(p.body),tf=p.tf||{},dx=tf.dx||0,dy=tf.dy||0,r=tf.r||0,sx=tf.sx||1,sy=tf.sy||1,lk=p.look||[0,0];
 let face='';if(b.face!=null){face=nyBrows(p.brows||'normal')+nyEye(43,54,p.eyes,lk,'L')+nyEye(83,54,p.eyes,lk,'R')+nyMouth(p.mouth);
  if(p.cheeks)face+=`<g fill='#fb7185' opacity='${p.cheeks>1?.75:.5}'><ellipse cx='30' cy='76' rx='7' ry='4.5'/><ellipse cx='98' cy='76' rx='7' ry='4.5'/></g>`+(p.cheeks>1?`<path d='M24 74 l3 -4 M30 75 l3 -4 M92 74 l3 -4 M98 75 l3 -4' stroke='#e11d48' stroke-width='1.6' stroke-linecap='round'/>`:'');
  if(b.face)face=`<g transform='${b.face}'>${face}</g>`}
 const legs=b.legs?`<g stroke='${NY.ink}' stroke-width='3.2' stroke-linecap='round' fill='none'><path d='${p.legs=='tuck'?'M34 119 l-4 8 M60 119 l4 8':'M34 119 V130 M60 119 V130'}'/></g>`:'';
 const body=`<g transform='translate(${dx} ${dy}) rotate(${r} 63 122) translate(63 122) scale(${sx} ${sy}) translate(-63 -122)'>${legs}${b.paper}${face}${b.limbs?nyArms(p.arms):''}</g>`;
 const shadow=b.flat&&p.airborne?'':`<ellipse cx='${63+dx}' cy='133' rx='${Math.max(14,34+dy*.6)}' ry='4.5' fill='#0003'/>`;
 return shadow+(p.flip?`<g transform='translate(126 0) scale(-1 1)'>${body}</g>`:body)+`<g>${nyExtras(p.extras,t)}</g>`}
/* expressions: base poses held while nothing else happens */
const NY_BASE={neutral:{},focused:{eyes:'squint',look:[-3,2],brows:'low',mouth:'small'},asleep:{eyes:'closed',mouth:'small',brows:'flat',extras:['zzz'],tf:{r:-4}},
 thinking:{eyes:'open',look:[3,-4],brows:'oneup',mouth:'smirk',arms:'chin',extras:['dots']},whee:{eyes:'happy',mouth:'O',arms:'up',extras:['sparkles'],tf:{r:8}}};
/* clips: [ticks, pose] frames played at NOTEY_FPS; loop repeats the frames; then = next clip or base */
const NY_CLIPS={
 wave:{f:[[2,{arms:'wave1',eyes:'happy',mouth:'grin'}],[2,{arms:'wave2',eyes:'happy',mouth:'grin'}]],loop:3},
 joy:{f:[[2,{eyes:'happy',mouth:'laugh',arms:'up',legs:'tuck',tf:{dy:-12}}],[2,{eyes:'happy',mouth:'grin',arms:'up',tf:{sy:.94}}]],loop:3,end:[[8,{eyes:'happy',mouth:'grin',cheeks:1,arms:'hips'}]]},
 love:{f:[[3,{eyes:'heart',mouth:'smile',cheeks:1,arms:'hug',extras:['hearts']}],[3,{eyes:'heart',mouth:'cat',cheeks:1,arms:'hug',extras:['hearts'],tf:{sy:1.04}}]],loop:3},
 excited:{f:[[2,{eyes:'star',mouth:'laugh',arms:'up',legs:'tuck',tf:{dy:-8},extras:['sparkles']}],[2,{eyes:'star',mouth:'grin',arms:'flap2',extras:['sparkles']}]],loop:3},
 proud:{f:[[2,{eyes:'closed',mouth:'smile',arms:'hips',tf:{sy:1.05}}],[14,{eyes:'closed',mouth:'smirk',arms:'hips',tf:{r:-4,sy:1.03},extras:['shine'],cheeks:1}]]},
 surprised:{f:[[1,{eyes:'wide',brows:'raised',mouth:'O',arms:'up',tf:{dy:-8,sy:1.08},extras:['exclaim']}],[10,{eyes:'wide',brows:'raised',mouth:'o',extras:['exclaim']}]]},
 shocked:{f:[[1,{eyes:'wide',brows:'raised',mouth:'O',arms:'up',tf:{sy:1.12,dy:-6}}],[8,{eyes:'wide',brows:'worried',mouth:'grit',arms:'cheeks',extras:['sweat']}]],then:'sad'},
 sad:{f:[[10,{eyes:'sad',brows:'worried',mouth:'frown',extras:['tear'],tf:{sy:.96}}],[10,{eyes:'sad',brows:'worried',mouth:'wobble',extras:['tear'],tf:{sy:.96}}]]},
 worried:{f:[[3,{eyes:'sad',brows:'worried',mouth:'wobble',arms:'cheeks',extras:['sweat'],tf:{dx:-1}}],[3,{eyes:'sad',brows:'worried',mouth:'wobble',arms:'cheeks',extras:['sweat'],tf:{dx:1}}]],loop:3},
 curious:{f:[[14,{eyes:'open',look:[3,-1],brows:'oneup',mouth:'small',tf:{r:7},extras:['question']}]]},
 idea:{f:[[3,{eyes:'open',look:[0,-4],brows:'raised',mouth:'o',arms:'chin'}],[12,{eyes:'wide',brows:'raised',mouth:'grin',arms:'pointup',extras:['bulb']}]]},
 confused:{f:[[4,{eyes:'open',look:[-2,-2],brows:'oneup',mouth:'wobble',arms:'shrug',extras:['question'],tf:{r:-6}}],[4,{eyes:'open',look:[2,-2],brows:'oneup',mouth:'wobble',arms:'shrug',extras:['question'],tf:{r:6}}]],loop:2},
 embarrassed:{f:[[16,{eyes:'happy',brows:'worried',mouth:'wobble',cheeks:2,arms:'cheeks',extras:['sweat'],tf:{sy:.97}}]]},
 grumpy:{f:[[2,{eyes:'angry',brows:'angry',mouth:'grit',arms:'hips',extras:['vein'],tf:{sy:1.06}}],[12,{eyes:'angry',brows:'angry',mouth:'frown',arms:'hips',extras:['vein'],tf:{r:-3}}],[6,{eyes:'closed',brows:'angry',mouth:'flat',arms:'hips',extras:['steam']}]]},
 dizzy:{f:[[2,{eyes:'spiral',mouth:'wobble',extras:['stars'],tf:{r:-7}}],[2,{eyes:'spiral2',mouth:'wobble',extras:['stars'],tf:{r:7}}]],loop:4},
 dizzyShort:{f:[[2,{eyes:'spiral',mouth:'wobble',extras:['stars'],tf:{r:-5}}],[2,{eyes:'spiral2',mouth:'o',extras:['stars'],tf:{r:5}}]],loop:2},
 cheeky:{f:[[14,{eyes:'wink',brows:'oneup',mouth:'tongue',tf:{r:-5}}]]},
 relieved:{f:[[14,{eyes:'closed',brows:'worried',mouth:'smile',extras:['phew','sweat'],tf:{sy:.95}}]]},
 impressed:{f:[[2,{eyes:'star',brows:'raised',mouth:'O',tf:{sy:1.06}}],[12,{eyes:'star',brows:'raised',mouth:'grin',extras:['sparkles']}]]},
 hum:{f:[[4,{eyes:'closed',mouth:'o',extras:['notes'],tf:{r:-4}}],[4,{eyes:'closed',mouth:'small',extras:['notes'],tf:{r:4}}]],loop:3},
 yawn:{f:[[3,{eyes:'half',mouth:'o'}],[8,{eyes:'closed',mouth:'yawn',arms:'up',tf:{sy:1.06}}],[4,{eyes:'half',mouth:'small'}]]},
 wake:{f:[[2,{eyes:'line'}],[2,{eyes:'half',mouth:'small'}],[2,{eyes:'wide',brows:'raised',mouth:'o',extras:['exclaim'],tf:{dy:-6}}],[5,{eyes:'open',mouth:'smile'}]]},
 lookAround:{f:[[5,{look:[-4,0],mouth:'small'}],[5,{look:[4,0],mouth:'small'}],[3,{look:[0,0]}]]},
 stretch:{f:[[4,{arms:'up',eyes:'closed',mouth:'o',tf:{sy:1.08,dy:-3}}],[4,{arms:'up',eyes:'closed',mouth:'small',tf:{sy:1.02}}],[2,{}]]},
 peek:{f:[[10,{look:[-4,4],eyes:'squint',mouth:'small',tf:{r:-5}}],[3,{eyes:'happy',mouth:'smile'}]]},
 nod:{f:[[2,{eyes:'happy',mouth:'smile',tf:{dy:2,sy:.96}}],[2,{eyes:'happy',mouth:'smile'}]],loop:2},
 appear:{lock:1,f:[[2,{body:'sq'}],[2,{body:'diamond'}],[2,{body:'fA',eyes:'wide',mouth:'o'}],[3,{eyes:'wide',brows:'raised',mouth:'o',tf:{sy:1.06}}]],then:'wave'},
 crane:{lock:1,f:[[2,{eyes:'wide',brows:'raised',mouth:'o'}],[2,{body:'diamond'}],[2,{body:'kite'}],[3,{body:'crane'}],
   [2,{body:'craneD',tf:{dy:-6},extras:['sparkles']}],[2,{body:'crane',tf:{dy:-12},extras:['sparkles']}],[2,{body:'craneD',tf:{dy:-8},extras:['sparkles']}],[2,{body:'crane',tf:{dy:-14},extras:['sparkles']}],
   [2,{body:'craneD',tf:{dy:-6},extras:['sparkles']}],[2,{body:'crane',tf:{dy:-10},extras:['sparkles']}],[3,{body:'craneD'}],[6,{body:'crane',extras:['sparkles']}],
   [2,{body:'kite'}],[2,{body:'diamond'}],[2,{eyes:'wide',mouth:'o'}]],then:'joy'}};
const NY_FACES=['joy','love','excited','proud','surprised','curious','idea','confused','embarrassed','grumpy','dizzy','cheeky','relieved','impressed','sad','worried'];
function notey2(W){const X=W.X,root=X.root,box=root&&root.querySelector('.wn');if(!box||box.__n2)return;box.__n2=1;
 const m=box.querySelector('.wn-m'),bub=box.querySelector('.wn-bub');m.classList.remove('pop');
 m.innerHTML=`<svg viewBox='0 0 130 140' width='100%' height='100%' class='wn-svg wn2' aria-hidden='true'></svg>`;const sv=m.firstChild;
 const st={base:'neutral',clip:null,fi:0,left:0,loops:0,t:0,ptr:null,blink:30,idle:90,last:'',clicks:[],drag:0,pd:null,wasDirty:false,path:X.path,len:(X.ed.textContent||'').length,shockAt:0};
 const rm=()=>document.documentElement.classList.contains('rmotion');const pick=a=>a[Math.floor(Math.random()*a.length)];
 const visible=()=>box.style.display!='none'&&W.el.isConnected&&!W.el.classList.contains('min');
 function play(name,opt){opt=opt||{};if(st.clip&&st.clip.lock&&!opt.force)return;
  if(name=='airplane')return flyPlane();if(name=='crane'&&rm())name='proud';
  const c=NY_CLIPS[name];if(!c){if(NY_BASE[name])st.base=name;return}
  st.clip={name,frames:c.loop?Array.from({length:c.loop},()=>c.f).flat().concat(c.end||[]):c.f.concat(c.end||[]),then:c.then,lock:c.lock};st.fi=-1;next()}
 function next(){const c=st.clip;st.fi++;if(st.fi>=c.frames.length){st.clip=null;if(c.restore)c.restore();if(c.then)play(c.then,{force:1});return}
  const f=c.frames[st.fi];st.left=f[0];if(f[2])try{f[2]()}catch(e){}}
 /* paper airplane: fold, fly a loop around the window in held steps, land and unfold */
 function flyPlane(){if(rm()||!visible())return play('proud');
  const rr=root.getBoundingClientRect(),br=box.getBoundingClientRect(),x0=br.left-rr.left,y0=br.top-rr.top,maxX=Math.max(0,root.clientWidth-box.offsetWidth),maxY=Math.max(0,root.clientHeight-box.offsetHeight);
  const saved={left:box.style.left,top:box.style.top,right:box.style.right,bottom:box.style.bottom},bv=bub.style.visibility;
  const pos=(x,y)=>()=>{box.style.right='auto';box.style.bottom='auto';box.style.left=Math.round(Math.max(0,Math.min(maxX,x)))+'px';box.style.top=Math.round(Math.max(0,Math.min(maxY,y)))+'px'};
  const fr=[[2,{eyes:'wide',brows:'raised',mouth:'o'},()=>{bub.style.visibility='hidden'}],[2,{body:'fA',eyes:'happy',mouth:'grin'}],[2,{body:'fB',eyes:'happy',mouth:'grin'}],[2,{body:'fC'}],[3,{body:'plane'}]];
  const pts=[[x0,y0],[x0-maxX*.25,y0-80],[maxX*.05,Math.max(8,y0-150)],[maxX*.5,8],[maxX*.95,Math.max(8,y0*.45)],[x0,y0]];
  for(let i=1;i<pts.length;i++){const [ax,ay]=pts[i-1],[bx,by]=pts[i],n=Math.max(3,Math.round(Math.hypot(bx-ax,by-ay)/55));
   for(let j=1;j<=n;j++){const x=ax+(bx-ax)*j/n,y=ay+(by-ay)*j/n;fr.push([1,{body:'plane',airborne:1,flip:bx<ax,tf:{r:by<ay-4?-10:by>ay+4?9:0,dy:j%2?-2:0}},pos(x,y)])}}
  fr.push([3,{body:'plane',flip:true}],[2,{body:'fC'}],[2,{body:'fB',eyes:'happy',mouth:'grin'}],[2,{body:'fA',eyes:'happy',mouth:'grin'}],[2,{eyes:'happy',mouth:'grin',arms:'up'}]);
  st.clip={name:'airplane',frames:fr,lock:1,then:'proud',restore:()=>{Object.assign(box.style,saved);bub.style.visibility=bv}};st.fi=-1;next()}
 /* moods set by the original Notey (classes on the box) */
 let lastCls='';const mo=new MutationObserver(()=>{const c=['alert','happy','think','sleep','wave'].find(k=>box.classList.contains(k))||'';if(c==lastCls)return;lastCls=c;const tx=bub.textContent||'';
  if(c=='happy')play(/words|milestone|roll|progress/i.test(tx)?'crane':/fixed|All set|Done/i.test(tx)?'proud':/template/i.test(tx)?'excited':pick(['joy','love','excited','proud','nod']));
  else if(c=='alert')play(/save/i.test(tx)?'worried':/letter/i.test(tx)?'curious':/Stuck|blank/i.test(tx)?'idea':/spell|repeat|twice/i.test(tx)?'surprised':pick(['surprised','idea','curious']));
  else if(c=='wave'){if(st.base=='asleep'){st.base='neutral';play('wake')}else play('wave')}
  else if(c=='think')st.base='thinking';
  else if(c=='sleep'){play('yawn');st.base='asleep'}
  else{if(st.base=='asleep'){st.base='neutral';play('wake')}else if(st.base=='thinking')st.base='neutral'}});
 mo.observe(box,{attributes:true,attributeFilter:['class']});
 /* tricks: right-click Notey, or the button added to Notey's main menu */
 function tricks(){W.notey.say('Want to see a trick?',[['✈️ Paper airplane',()=>{W.notey.hide();play('airplane')},1],['🕊️ Origami crane',()=>{W.notey.hide();play('crane')}],['🎭 Pull a face',()=>{W.notey.hide();play(pick(NY_FACES))}],['🎵 Hum a tune',()=>{W.notey.hide();play('hum')}],['Close',()=>W.notey.hide()]])}
 const bo=new MutationObserver(()=>{if(/What can I help you with/.test(bub.textContent)&&!bub.querySelector('.wn-trick')){const bb=bub.querySelector('.wn-b');if(!bb)return;const b=document.createElement('button');b.className='wn-trick';b.textContent='🎭 Show me a trick';b.onclick=tricks;bb.appendChild(b)}});
 bo.observe(bub,{childList:true,subtree:true});
 m.addEventListener('contextmenu',e=>{e.preventDefault();e.stopPropagation();tricks()});
 /* poking and dragging */
 m.addEventListener('pointerdown',e=>{st.pd={x:e.clientX,y:e.clientY}});
 m.addEventListener('pointermove',e=>{if(st.pd&&!st.drag&&Math.abs(e.clientX-st.pd.x)+Math.abs(e.clientY-st.pd.y)>6&&!(st.clip&&st.clip.lock)){st.drag=1;st.clip=null;st.prevBase=st.base=='whee'?'neutral':st.base;st.base='whee'}});
 const up=()=>{st.pd=null;if(st.drag){st.drag=0;st.base=st.prevBase||'neutral';play('dizzyShort')}};m.addEventListener('pointerup',up);m.addEventListener('pointercancel',up);
 m.addEventListener('click',()=>{if(st.drag)return;const now=Date.now();st.clicks=st.clicks.filter(t=>now-t<2500);st.clicks.push(now);if(st.clicks.length>=6){st.clicks=[];play('grumpy')}else if(st.clicks.length==4)play('dizzy');else if(st.clicks.length==1&&!st.clip&&st.base=='neutral')play('nod')});
 m.addEventListener('pointerenter',()=>{if(!st.clip&&st.base=='neutral'&&Math.random()<.35)play('curious')});
 const onPtr=e=>{st.ptr=[e.clientX,e.clientY]};document.addEventListener('pointermove',onPtr);
 /* reacting to the writing */
 let typeT=0;const onInput=e=>{const L=(X.ed.textContent||'').length,d=L-st.len;st.len=L;st.idle=Math.max(st.idle,60);
  if(d<-250&&e.inputType!='historyUndo'){st.shockAt=Date.now();play('shocked',{force:0});return}
  if(e.inputType=='historyUndo'&&Date.now()-st.shockAt<20000&&d>0){st.shockAt=0;play('relieved');return}
  if(e.inputType=='insertFromPaste'&&d>400){play('impressed');return}
  if(!st.clip&&(st.base=='neutral'||st.base=='focused')){st.base='focused';clearTimeout(typeT);typeT=setTimeout(()=>{if(st.base=='focused')st.base='neutral'},2500)}};
 X.ed.addEventListener('input',onInput);
 function tick(){if(!W.el.isConnected){clearInterval(timer);mo.disconnect();bo.disconnect();document.removeEventListener('pointermove',onPtr);return}
  st.t++;if(!visible())return;
  if(st.t%8==0){const d=X.dirty,p=X.path;if(st.wasDirty&&!d&&p&&(p===st.path||!st.path))play('airplane');st.wasDirty=d;st.path=p}
  let pose;
  if(st.clip){const f=st.clip.frames[st.fi];pose=Object.assign({},f[1]);if(--st.left<=0)next()}
  else{pose=Object.assign({},NY_BASE[st.base]||{});
   if(!rm()&&st.base!='whee'){const ph=Math.floor(st.t/6)%2;pose.tf=Object.assign({},pose.tf,{dy:((pose.tf&&pose.tf.dy)||0)-(ph?1.5:0)})}
   if(--st.blink<=0){if(!pose.eyes||pose.eyes=='open'||pose.eyes=='squint')pose.eyes='line';if(st.blink<=-1)st.blink=20+Math.floor(Math.random()*28)}
   if(st.base=='neutral'&&!st.drag&&--st.idle<=0){st.idle=70+Math.floor(Math.random()*80);const h=new Date().getHours();play(pick(['lookAround','lookAround','stretch','peek','hum','nod'].concat(h>=22||h<6?['yawn','yawn']:[])))}}
  if(!pose.look&&st.ptr&&(!pose.eyes||pose.eyes=='open'||pose.eyes=='wide')){const r=m.getBoundingClientRect(),dx=st.ptr[0]-(r.left+r.width/2),dy=st.ptr[1]-(r.top+r.height*.4),q=v=>Math.abs(v)<40?0:v<0?-3:3;pose.look=[q(dx),q(dy)]}
  if(rm()&&pose.tf)pose.tf={};
  const html=nyRender(pose,st.t);if(html!==st.last){st.last=html;sv.innerHTML=html}}
 const timer=setInterval(tick,Math.round(1000/NOTEY_FPS));
 if(box.style.display!='none'&&!rm())play('appear');tick();
 W.notey.play=play;W.notey.tricks=tricks}
(function(){if(typeof noteyInit!=='function')return;const orig=noteyInit;noteyInit=function(W){orig(W);try{notey2(W)}catch(e){console.warn('Notey animation',e)}}})();
