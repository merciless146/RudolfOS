/* ===== Text-to-speech in every Rudolf OS language =====
   One way to speak for the whole desktop (alerts, screen reader, WordPro "Read aloud", Settings sample):
   1. a voice of this computer's browser engine that matches the language (picked explicitly, because the
      engine otherwise reads e.g. Russian with an English voice or stays silent);
   2. on Rudolf OS, the built-in eSpeak NG voices (rudolfd /api/say), which cover all nine languages.
   Japanese for eSpeak NG is first turned into kana with the KAKASI reading dictionary (lib/ja-yomi.txt),
   because eSpeak NG only reads kana. The text's own script picks the language when it clearly differs from
   the desktop language (a Russian paragraph is read in Russian on an English desktop). */
const TTS_ESPEAK={en:'en-us',es:'es',pt:'pt-br',fr:'fr-fr',de:'de',ru:'ru',zh:'cmn',ja:'ja',ko:'ko'};
DEFS.acc.ttsVoice={};if(!AKEYS.includes('ttsVoice'))AKEYS.push('ttsVoice');if(!S.ttsVoice)S.ttsVoice={};if(typeof SC=='object'&&SC&&!SC.ttsVoice)SC.ttsVoice={};
const ttsEsc=s=>String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/'/g,'&#39;');
let TTS_ES_OK=null,TTS_AUDIO=null,TTS_GEN=0;
function ttsGuess(t,ui){ui=ui||S.lang||'en';const s=String(t).slice(0,400);
 const n=re=>(s.match(re)||[]).length,cy=n(/[Ѐ-ӿ]/g),hg=n(/[가-힯ᄀ-ᇿ]/g),kn=n(/[぀-ヿ]/g),hz=n(/[㐀-鿿]/g),la=n(/[A-Za-zÀ-ÿ]/g);
 const top=Math.max(cy,hg,kn+hz,la);if(!top)return ui;
 if(top==cy)return'ru';if(top==hg)return'ko';
 if(top==kn+hz)return kn?'ja':(ui=='ja'?'ja':'zh');
 return['en','es','pt','fr','de'].includes(ui)?ui:'en'}
function ttsLocale(l){return(LANGS.find(x=>x[0]==l)||[])[2]||l}
function ttsBrowserVoices(l){if(!window.speechSynthesis)return[];const loc=ttsLocale(l).toLowerCase();
 const vs=speechSynthesis.getVoices().filter(v=>v.lang&&v.lang.replace('_','-').toLowerCase().split('-')[0]==l);
 return vs.sort((a,b)=>(b.lang.replace('_','-').toLowerCase()==loc)-(a.lang.replace('_','-').toLowerCase()==loc)||b.localService-a.localService||b.default-a.default)}
function ttsBrowserVoice(l){const pick=S.ttsVoice&&S.ttsVoice[l];const vs=ttsBrowserVoices(l);if(pick=='espeak'&&RD)return null;return vs.find(v=>v.voiceURI==pick)||vs[0]||null}
/* where speech in language l comes from: {kind:'browser',voice} | {kind:'espeak'} | null */
function ttsEngine(l){const v=ttsBrowserVoice(l);if(v)return{kind:'browser',voice:v};if(RD&&TTS_ES_OK!==false)return{kind:'espeak'};return null}
function ttsStop(){TTS_GEN++;try{window.speechSynthesis&&speechSynthesis.cancel()}catch(e){}try{TTS_AUDIO&&TTS_AUDIO.pause()}catch(e){}TTS_AUDIO=null}
/* ---------- Japanese: kanji -> hiragana for eSpeak NG ---------- */
let JAY=null;
const JA_CL={a:'あぁっわゎ',i:'いぃっゐ',u:'うぅっ',e:'えぇっゑ',o:'おぉっ',k:'かゕきくけゖこっ',g:'がぎぐげごっ',s:'さしすせそっ',z:'ざじずぜぞっ',j:'ざじずぜぞっ',t:'たちつてとっ',d:'だぢづでどっ',c:'ちっ',n:'なにぬねのん',h:'はひふへほっ',b:'ばびぶべぼっ',f:'ふっ',p:'ぱぴぷぺぽっ',m:'まみむめも',y:'やゃゆゅよょ',r:'らりるれろ',l:'らりるれろ',w:'わゐゑゎをっ',v:'ゔ'};
const JA_TAILS={};for(const[k,v]of Object.entries(JA_CL))for(const c of v)(JA_TAILS[c]=JA_TAILS[c]||[]).push(k);
async function jaDict(){if(JAY)return JAY;const t=await(await fetch('/lib/ja-yomi.txt')).text();const m=new Map();let max=1;
 for(const l of t.split('\n')){const p=l.split('\t');if(!p[1])continue;const key=p[2]?p[0]+'\u0001'+p[2]:p[0];if(!m.has(key))m.set(key,p[1]);if(p[0].length>max)max=p[0].length}
 return JAY={m,max:Math.min(max,16)}}
const JA_HAN=/[㐀-鿿豈-﫿々〆]/;
function jaKanaWith(D,s){let o='',i=0;
 while(i<s.length){if(!JA_HAN.test(s[i])){o+=s[i++];continue}
  let best='',bl=0;
  for(let L=Math.min(D.max,s.length-i);L>=1&&bl<=L;L--){const w=s.substr(i,L),nx=s[i+L];
   if(nx&&JA_TAILS[nx]&&L+1>bl)for(const tl of JA_TAILS[nx]){const y=D.m.get(w+'\u0001'+tl);if(y!=null){best=y+nx;bl=L+1;break}}
   if(L>bl){const y=D.m.get(w);if(y!=null){best=y;bl=L}}}
  if(bl){o+=best;i+=bl}else i++}   // a kanji with no known reading is skipped rather than spelled out
 return o}
async function jaKana(s){if(!JA_HAN.test(s))return s;try{return jaKanaWith(await jaDict(),s)}catch(e){return s}}
/* ---------- speaking ---------- */
async function ttsEspeak(t,l,gen,onend){if(l=='ja')t=await jaKana(t);if(gen!=TTS_GEN)return;
 const r=await fetch('/api/say?lang='+encodeURIComponent(TTS_ESPEAK[l]||'en-us')+'&text='+encodeURIComponent(t.slice(0,1500)),{method:'POST',headers:{'X-Rudolf-Token':RD.token}}).catch(()=>null);
 if(!r||!r.ok){if(r&&r.status==404)TTS_ES_OK=false;onend&&onend(false);return}TTS_ES_OK=true;const b=await r.blob();if(gen!=TTS_GEN)return;
 const a=new Audio(URL.createObjectURL(b));TTS_AUDIO=a;a.playbackRate=1;a.onended=()=>{URL.revokeObjectURL(a.src);if(gen==TTS_GEN&&onend)onend(true)};a.onerror=()=>onend&&onend(false);a.play().catch(()=>onend&&onend(false))}
/* ttsSpeak(text,{lang,rate,queue,onend}): lang defaults to the text's script / desktop language */
function ttsSpeak(t,o={}){t=String(t||'').trim();if(!t)return false;const l=o.lang||ttsGuess(t),e=ttsEngine(l);if(!e){o.onend&&o.onend(false);return false}
 if(!o.queue)ttsStop();const gen=TTS_GEN;
 if(e.kind=='browser'){try{const u=new SpeechSynthesisUtterance(t);u.voice=e.voice;u.lang=e.voice.lang;u.rate=o.rate||1.05;u.onend=()=>{if(gen==TTS_GEN&&o.onend)o.onend(true)};u.onerror=()=>o.onend&&o.onend(false);speechSynthesis.speak(u);return true}catch(er){}}
 if(RD){ttsEspeak(t,l,gen,o.onend);return true}return false}
/* the older entry points all go through ttsSpeak now */
speak=function(t,force){if(!(S.tts||force)||!t)return;ttsSpeak(t)};
srSpeak=function(t){ttsSpeak(t)};
if(window.speechSynthesis){speechSynthesis.getVoices();speechSynthesis.addEventListener('voiceschanged',()=>{srVoiceOK=speechSynthesis.getVoices().length>0})}
/* ---------- Settings > Accessibility: which voice speaks each language ---------- */
function ttsVoiceLabel(l){const e=ttsEngine(l);if(!e)return'none on this computer';if(e.kind=='espeak')return'eSpeak NG (built into Rudolf OS)';return e.voice.name+(e.voice.localService?'':' (online)')}
function ttsSettingsBox(host){const box=document.createElement('div');box.className='ttsbox';host.appendChild(box);
 const draw=()=>{const cur=S.lang||'en';
  box.innerHTML=`<div class='msg' style='margin-top:6px'>Voice for each language:</div><table class='ttst'>${LANGS.map(([c,n])=>{const vs=ttsBrowserVoices(c),pick=(S.ttsVoice||{})[c]||'';
   const opts=vs.length+(RD?1:0)>1?`<select data-l='${c}'>${vs.map(v=>`<option value='${ttsEsc(v.voiceURI)}'${pick==v.voiceURI?' selected':''}>${ttsEsc(v.name)}${v.localService?'':' (online)'}</option>`).join('')}${RD?`<option value='espeak'${pick=='espeak'||!vs.length?' selected':''}>eSpeak NG (built in)</option>`:''}</select>`:`<span>${ttsEsc(ttsVoiceLabel(c))}</span>`;
   return`<tr${c==cur?" class='cur'":''}><td>${ttsEsc(n)}</td><td>${opts}</td><td><button data-t='${c}' title='Speak a sample in this language'${ttsEngine(c)?'':' disabled'}>▶</button></td></tr>`}).join('')}</table>`;
  box.querySelectorAll('select').forEach(s=>s.onchange=()=>{S.ttsVoice=Object.assign({},S.ttsVoice,{[s.dataset.l]:s.value});if(typeof SC=='object'&&SC)SC.ttsVoice=Object.assign({},S.ttsVoice);persistS();ttsSpeak(TTS_SAMPLE[s.dataset.l],{lang:s.dataset.l})});
  box.querySelectorAll('button[data-t]').forEach(b=>b.onclick=()=>ttsSpeak(TTS_SAMPLE[b.dataset.t],{lang:b.dataset.t}))};
 draw();if(window.speechSynthesis)speechSynthesis.addEventListener('voiceschanged',()=>box.isConnected&&draw())}
const TTS_SAMPLE={en:'Hello! This is the text-to-speech voice.',es:'¡Hola! Esta es la voz de texto a voz.',pt:'Olá! Esta é a voz de leitura em voz alta.',fr:'Bonjour ! Voici la voix de synthèse vocale.',de:'Hallo! Das ist die Sprachausgabe.',ru:'Здравствуйте! Это голос синтеза речи.',zh:'你好！这是文字转语音的声音。',ja:'こんにちは。これは音声読み上げの声です。',ko:'안녕하세요! 텍스트 음성 변환 음성입니다.'};
new MutationObserver(()=>{const b=document.getElementById('atp');if(b&&!b.dataset.tts){b.dataset.tts=1;b.onclick=()=>ttsSpeak(TTS_SAMPLE[S.lang||'en']||T_('Hello! This is the text-to-speech voice.'),{lang:S.lang||'en'});ttsSettingsBox(b.parentElement)}}).observe(document.body,{childList:true,subtree:true});
