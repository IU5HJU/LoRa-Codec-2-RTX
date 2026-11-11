// Pagina di configurazione servita dalla scheda sul proprio Wi-Fi (192.168.4.1). Gli stessi comandi CFG della seriale
// passano da /cfg?c=<riga>: l'interprete e' lo stesso (lib/voicecore/settings.cpp). Testata con tests/page/run_wifi.js.
#pragma once

static const char kWifiPage[] = R"HTML(<!doctype html>
<html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>LoRa-Codec-2-RTX: configurazione</title>
<style>
body{font-family:system-ui,sans-serif;max-width:34rem;margin:0 auto;padding:1rem;background:#fff;color:#14181f}
label{display:block;margin-top:.8rem;font-weight:600}
input,select,button{font:inherit;padding:.5rem;width:100%;box-sizing:border-box;margin-top:.2rem}
button{background:#0b5ed7;color:#fff;border:0;border-radius:.4rem;margin-top:1rem}
button.sec{background:#e5e7eb;color:#14181f}
.hint{color:#555;font-size:.85rem;margin:.2rem 0 0}
#st{margin-top:1rem;padding:.6rem;border-radius:.4rem;display:none}
#st.ok{display:block;background:#d9f2e0;color:#14532d}
#st.err{display:block;background:#fde2e2;color:#7f1d1d}
@media(prefers-color-scheme:dark){body{background:#12151a;color:#e8eaed}.hint{color:#aab}button.sec{background:#2b3038;color:#e8eaed}input,select{background:#1d2128;color:#e8eaed;border:1px solid #444}}
</style></head><body>
<h1>Configurazione</h1>
<p class="hint">Scheda collegata al suo Wi-Fi. Finché questa pagina è aperta <strong>la trasmissione è disattivata</strong>. Per uscire riavvia la scheda (pulsante in fondo o pressione lunga del volume).</p>
<form id="f" autocomplete="off">
<label for="callsign">Nominativo</label><input id="callsign" maxlength="8" required>
<label for="profile">Profilo</label>
<select id="profile"><option value="std">Standard</option><option value="msg">Messaggio</option><option value="qua">Qualità</option></select>
<label for="freq">Frequenza Standard (MHz)</label><input id="freq" inputmode="decimal" required>
<label for="freqmsg">Frequenza Messaggio (MHz)</label><input id="freqmsg" inputmode="decimal" required>
<label for="freqqual">Frequenza Qualità (MHz)</label><input id="freqqual" inputmode="decimal" required>
<p class="hint">Da 430.000 a 440.000. Le frequenze possono essere uguali.</p>
<label for="power">Potenza (dBm)</label><input id="power" type="number" min="2" max="17" required>
<label for="volume">Volume (0-10)</label><input id="volume" type="number" min="0" max="10" required>
<label for="micfloor">Fondo del microfono (dBFS)</label><input id="micfloor" type="number" min="-80" max="-36" required>
<label for="pos">Trasmetti la posizione</label><select id="pos"><option value="off">No</option><option value="on">Sì</option></select>
<label for="qth">Posizione fissa (latitudine,longitudine, oppure none)</label><input id="qth" placeholder="43.7696,11.2558">
<div id="gpsbox" hidden><label for="gps">GPS (solo T-Beam)</label><select id="gps"><option value="off">Spento</option><option value="on">Acceso</option></select></div>
<h2>Testi predefiniti (SMS)</h2>
<p class="hint">Fino a 5 testi da 150 caratteri al massimo (solo ASCII, senza accenti e senza le parentesi graffe), da inviare dal menu della scheda: <strong>Invia testo</strong>. Lascia vuoto un posto per non usarlo. Valgono subito.</p>
<label for="text1">Testo 1</label><input id="text1" maxlength="150">
<label for="text2">Testo 2</label><input id="text2" maxlength="150">
<label for="text3">Testo 3</label><input id="text3" maxlength="150">
<label for="text4">Testo 4</label><input id="text4" maxlength="150">
<label for="text5">Testo 5</label><input id="text5" maxlength="150">
<button type="submit" id="save">Salva</button>
<button type="button" class="sec" id="reboot">Riavvia la scheda</button>
</form>
<div id="st"></div>
<script>
const $=id=>document.getElementById(id);
function st(t,c){const e=$('st');e.textContent=t;e.className=c||'';}
async function cmd(l){const r=await fetch('/cfg?c='+encodeURIComponent(l));return (await r.text()).trim();}
function parse(t){const m={};for(const kv of t.replace(/^CFG /,'').split(' ')){const i=kv.indexOf('=');if(i>0)m[kv.slice(0,i)]=kv.slice(i+1);}return m;}
let board='';
function fill(m){if(m.board!==undefined){board=m.board;$('gpsbox').hidden=board!=='tbeam';}for(const k of ['callsign','profile','freq','freqmsg','freqqual','power','volume','micfloor','pos','qth','gps'])if(m[k]!==undefined&&$(k))$(k).value=m[k];}
async function load(){try{const t=await cmd('CFG GET');if(!t.startsWith('CFG '))throw new Error(t);fill(parse(t));for(let i=1;i<=5;i++){const r=await cmd('CFG GET TEXT'+i);$('text'+i).value=r.startsWith('CFG TEXT'+i+'=')?r.slice(('CFG TEXT'+i+'=').length):'';}st('Valori letti dalla scheda.','ok');}catch(e){st('Lettura non riuscita: '+e.message,'err');}}
$('f').addEventListener('submit',async ev=>{
  ev.preventDefault();st('Salvataggio…','');
  const want={callsign:$('callsign').value.trim().toUpperCase(),profile:$('profile').value,freq:$('freq').value.trim(),freqmsg:$('freqmsg').value.trim(),freqqual:$('freqqual').value.trim(),power:$('power').value,volume:$('volume').value,micfloor:$('micfloor').value,pos:$('pos').value,qth:$('qth').value.trim()||'none'};
  if(board==='tbeam')want.gps=$('gps').value;   // la T3 non ha il GPS: il comando non si manda
  const keys={callsign:'CALLSIGN',freq:'FREQ',freqmsg:'FREQMSG',freqqual:'FREQQUAL',power:'POWER',volume:'VOLUME',micfloor:'MICFLOOR',profile:'PROFILE',pos:'POS',qth:'QTH',gps:'GPS'};  // il GPS e' in want solo sulla T-Beam
  const texts=[];for(let i=1;i<=5;i++){const v=$('text'+i).value.trim();if(v!==''&&!/^[\x20-\x7A\x7C\x7E]+$/.test(v)){st('Il testo '+i+' ha caratteri non ammessi: solo ASCII, senza accenti.','err');return;}texts.push(v);}
  try{
    for(const k of Object.keys(keys)){if(want[k]===undefined)continue;const r=await cmd('CFG SET '+keys[k]+' '+want[k]);if(!r.startsWith('CFG OK')){st('Rifiutato ('+keys[k]+'): '+r,'err');return;}}
    for(let i=0;i<5;i++){const r=await cmd('CFG SET TEXT'+(i+1)+' '+(texts[i]||'NONE'));if(!r.startsWith('CFG OK')){st('Rifiutato (testo '+(i+1)+'): '+r,'err');return;}}
    const back=parse(await cmd('CFG GET'));const bad=[];
    for(const k of Object.keys(want)){let a=String(back[k]),b=String(want[k]);if(k.startsWith('freq')){a=parseFloat(a);b=parseFloat(b);}else if(k==='qth'&&b!=='none'){const x=b.split(',').map(parseFloat),y=a.split(',').map(parseFloat);if(Math.abs(x[0]-y[0])<2e-5&&Math.abs(x[1]-y[1])<2e-5)continue;}else if(k==='callsign'||k==='profile'){a=a.toLowerCase();b=b.toLowerCase();}else if(!isNaN(a)&&!isNaN(b)){a=Number(a);b=Number(b);}
      if(a!==b)bad.push(k);}
    fill(back);
    if(bad.length)st('Salvato, ma la scheda riporta valori diversi: '+bad.join(', '),'err');
    else st('Salvato e riletto dalla scheda: tutto coincide. I cambi di profilo, frequenza, potenza e nominativo valgono dal riavvio.','ok');
  }catch(e){st('Errore di comunicazione: '+e.message,'err');}
});
$('reboot').addEventListener('click',async()=>{try{await cmd('CFG REBOOT');}catch(e){}st('Scheda in riavvio: puoi chiudere la pagina.','ok');});
load();
</script></body></html>
)HTML";
