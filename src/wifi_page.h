// Pagina di configurazione servita dalla scheda sul proprio Wi-Fi (192.168.4.1). Gli stessi comandi CFG della seriale
// passano da /cfg?c=<riga>: l'interprete e' lo stesso (lib/voicecore/settings.cpp). Testata con tests/page/run_wifi.js.
#pragma once

static const char kWifiPage[] = R"HTML(<!doctype html>
<html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>VoLoRa: configurazione</title>
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
<h2>MeshCom: avvisi e accordi</h2>
<p class="hint">Dal menu della scheda (volume lungo, voce MeshCom) la scheda passa per qualche minuto ai 433,175 MHz delle reti di testo, invia un testo a un destinatario (gruppo come 22251 o 222, nominativo-SSID come IU5ABC-9, * per tutti), ascolta le risposte e torna da sola al profilo voce. Un posto vuoto non si usa.</p>
<label for="mcfreq">Frequenza della rete (MHz)</label><input id="mcfreq" inputmode="decimal">
<label for="mcssid">SSID del tuo nominativo (0-99)</label><input id="mcssid" type="number" min="0" max="99">
<label for="mcwindow">Secondi di ascolto dopo l'invio (30-1800)</label><input id="mcwindow" type="number" min="30" max="1800">
<label for="mcdest1">Destinatario 1</label><input id="mcdest1" maxlength="15">
<label for="mcdest2">Destinatario 2</label><input id="mcdest2" maxlength="15">
<label for="mcdest3">Destinatario 3</label><input id="mcdest3" maxlength="15">
<label for="mcdest4">Destinatario 4</label><input id="mcdest4" maxlength="15">
<label for="mcdest5">Destinatario 5</label><input id="mcdest5" maxlength="15">
<h2>Ripetitore</h2>
<p class="hint">Due schede possono fare da ripetitore: una <strong>RX</strong> ascolta la frequenza di ingresso (quella del profilo, qui sopra) e una <strong>TX</strong> ritrasmette sulla frequenza di uscita. Con un ruolo da ripetitore la scheda non funziona più da stazione: dopo il riavvio mostra lo stato del ripetitore. <strong>Attenzione:</strong> una scheda in ruolo da ripetitore non ha il menu a bordo e questa pagina non si apre più; per tornare stazione serve il cavo USB (sito web delle impostazioni o terminale seriale, <code>CFG SET ROLE OFF</code>). Vale dal riavvio.</p>
<label for="role">Ruolo della scheda</label>
<select id="role"><option value="off">Stazione normale</option><option value="rptrx">Ripetitore: scheda RX (ascolta l'ingresso)</option><option value="rpttx">Ripetitore: scheda TX (trasmette l'uscita)</option><option value="node">Nodo Internet (T3 su Wi-Fi)</option></select>
<p class="hint" id="rolenote" hidden><strong>Il ripetitore usa solo la T3:</strong> due T-Beam sarebbero uno spreco (GPS, PMU e batteria che non servono). Su questa scheda il ruolo non si può cambiare.</p>
<div id="nodebox" hidden>
<p class="hint"><strong>Nodo Internet:</strong> la T3 porta i pacchetti LoRa a un'altra T3 via Wi-Fi (UDP firmato). Solo profilo Standard. Rete, indirizzo dell'altro nodo, id e chiave si impostano da seriale (<code>CFG SET WIFISSID</code>, <code>WIFIPASS</code>, <code>PEER</code>, <code>NODEID</code>, <code>PEERID</code>, <code>NODEKEY</code>: vedi la guida del firmware). Come per il ripetitore, in questo ruolo la scheda non ha il menu e questa pagina non si apre più.</p>
</div>
<div id="rxbox" hidden>
<label for="monitor">Altoparlante di monitor sulla RX</label><select id="monitor"><option value="off">Spento</option><option value="on">Acceso</option></select>
<p class="hint">Con un altoparlante collegato (per il sysop in postazione) la RX fa ascoltare la voce che riceve. La seriale fra le due schede non cambia; i pin dell'altoparlante sono nella guida del firmware.</p>
</div>
<div id="txbox" hidden>
<label for="freqout">Frequenza di uscita (MHz)</label><input id="freqout" inputmode="decimal">
<p class="hint">Da 430.000 a 440.000. Sulla scheda TX sono anche la frequenza dei messaggi.</p>
<label for="idint">Identificativo ogni (minuti, 1-10)</label><input id="idint" type="number" min="1" max="10">
<p class="hint">Ogni quanti minuti la TX trasmette l'identificativo del ripetitore (il nominativo qui sopra, in modo testo: le stazioni lo vedono passare senza suoni). Di base 10.</p>
<label for="idmorse">Identificativo anche in Morse (FM)</label><select id="idmorse"><option value="on">Sì</option><option value="off">No</option></select>
<p class="hint">Dopo il testo la TX manda il nominativo in Morse con un tono in FM stretta (15 parole al minuto, 700 Hz): chi non ha le schede LoRa lo sente con un ricevitore FM qualsiasi e capisce che la frequenza è occupata da un ripetitore. Dura circa 6 secondi.</p>
</div>
<h2>Modo Sysop</h2>
<p class="hint">Per chi gestisce un ripetitore. Con il <strong>modo Sysop</strong> acceso il menu della scheda (volume lungo) mostra <strong>Comandi Sysop</strong> al posto di Invia testo: scegli il comando (STATO, SPEGNI, RIACCENDI, RIAVVIA, IDENTIFICA, POTENZA) e la scheda lo trasmette firmato al ripetitore. Resta acceso finché non lo spegni. Il nominativo e la chiave vanno impostati uguali sul ripetitore (schede RX e TX) e sulla scheda del sysop; la chiave non si rilegge mai.</p>
<label for="sysopmode">Modo Sysop</label><select id="sysopmode"><option value="off">Spento</option><option value="on">Acceso</option></select>
<label for="sysop">Nominativo del sysop</label><input id="sysop" maxlength="8" placeholder="IU5HJU">
<label for="sysopkey">Chiave (32 cifre esadecimali)</label><input id="sysopkey" maxlength="32" autocomplete="off" spellcheck="false">
<p class="hint" id="sysopkeyhint">Chiave non impostata.</p>
<button type="button" class="sec" id="genkey">Genera una chiave nuova</button>
<div id="sysopbox" hidden><h2>Comandi Sysop</h2><p class="hint">L'elenco che compare sulla scheda al posto dei testi predefiniti.</p><ol id="sysoplist"><li>STATO</li><li>SPEGNI</li><li>SPEGNI 15 MIN</li><li>SPEGNI 60 MIN</li><li>RIACCENDI</li><li>RIAVVIA</li><li>IDENTIFICA</li><li>POTENZA 17 dBm</li><li>POTENZA 14 dBm</li><li>POTENZA 10 dBm</li></ol></div>
<div id="textsbox">
<h2>Testi predefiniti (SMS)</h2>
<p class="hint">Segnaposti: %C il tuo nominativo, %M il modo del profilo voce (S, M, Q), %F la sua frequenza. Fino a 5 testi da 150 caratteri al massimo (solo ASCII, senza accenti e senza le parentesi graffe), da inviare dal menu della scheda: <strong>Invia testo</strong>. Lascia vuoto un posto per non usarlo. Valgono subito.</p>
<label for="text1">Testo 1</label><input id="text1" maxlength="150">
<label for="text2">Testo 2</label><input id="text2" maxlength="150">
<label for="text3">Testo 3</label><input id="text3" maxlength="150">
<label for="text4">Testo 4</label><input id="text4" maxlength="150">
<label for="text5">Testo 5</label><input id="text5" maxlength="150">
</div>
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
function fill(m){if(m.board!==undefined){board=m.board;$('gpsbox').hidden=board!=='tbeam';$('role').disabled=board==='tbeam';$('rolenote').hidden=board!=='tbeam';}for(const k of ['callsign','profile','freq','freqmsg','freqqual','power','volume','micfloor','pos','qth','gps','role','freqout'])if(m[k]!==undefined&&$(k))$(k).value=m[k];}
function roleShow(){const r=$('role').value;$('rxbox').hidden=r!=='rptrx';$('txbox').hidden=r!=='rpttx';$('nodebox').hidden=r!=='node';}
$('role').addEventListener('change',roleShow);
async function loadRole(){try{const r=await cmd('CFG GET MONITOR');if(r.startsWith('CFG MONITOR='))$('monitor').value=r.slice(12)==='on'?'on':'off';}catch(e){}roleShow();}
function sysopShow(){const on=$('sysopmode').value==='on';$('sysopbox').hidden=!on;$('textsbox').hidden=on;}
$('sysopmode').addEventListener('change',sysopShow);
$('genkey').addEventListener('click',()=>{const b=new Uint8Array(16);crypto.getRandomValues(b);$('sysopkey').value=Array.from(b,x=>x.toString(16).padStart(2,'0')).join('');$('sysopkeyhint').textContent='Copia questa chiave adesso: dopo il salvataggio non si rilegge. Va impostata uguale sulle due schede del ripetitore e su questa.';});
async function loadIdint(){try{const r=await cmd('CFG GET IDINT');if(r.startsWith('CFG IDINT='))$('idint').value=r.slice(10);}catch(e){}try{const r=await cmd('CFG GET IDMORSE');if(r.startsWith('CFG IDMORSE='))$('idmorse').value=r.slice(12)==='off'?'off':'on';}catch(e){}}
async function loadSysop(){try{const r=await cmd('CFG GET SYSOP');if(r.startsWith('CFG SYSOP ')){const m=parse(r.slice(10));$('sysop').value=m.call&&m.call!=='none'?m.call:'';$('sysopmode').value=m.mode==='on'?'on':'off';$('sysopkey').value='';$('sysopkeyhint').textContent=m.key==='set'?'Chiave impostata (non si mostra). Lascia vuoto per non cambiarla.':'Chiave non impostata.';sysopShow();}}catch(e){}}
async function load(){try{const t=await cmd('CFG GET');if(!t.startsWith('CFG '))throw new Error(t);fill(parse(t));await loadSysop();await loadIdint();await loadRole();for(let i=1;i<=5;i++){const r=await cmd('CFG GET TEXT'+i);$('text'+i).value=r.startsWith('CFG TEXT'+i+'=')?r.slice(('CFG TEXT'+i+'=').length):'';}try{const r=await cmd('CFG GET MC');if(r.startsWith('CFG MC ')){const m=parse(r.slice(7));$('mcfreq').value=m.freqmc||'';$('mcssid').value=m.ssid||'';$('mcwindow').value=m.window||'';for(let i=1;i<=5;i++){const d=await cmd('CFG GET MCDEST'+i);$('mcdest'+i).value=d.startsWith('CFG MCDEST'+i+'=')?d.slice(('CFG MCDEST'+i+'=').length):'';}}}catch(e){}st('Valori letti dalla scheda.','ok');}catch(e){st('Lettura non riuscita: '+e.message,'err');}}
$('f').addEventListener('submit',async ev=>{
  ev.preventDefault();st('Salvataggio…','');
  const want={callsign:$('callsign').value.trim().toUpperCase(),profile:$('profile').value,freq:$('freq').value.trim(),freqmsg:$('freqmsg').value.trim(),freqqual:$('freqqual').value.trim(),power:$('power').value,volume:$('volume').value,micfloor:$('micfloor').value,pos:$('pos').value,qth:$('qth').value.trim()||'none',role:$('role').value};
  if($('role').disabled)delete want.role;   // la T-Beam non fa il ripetitore
  if(board==='tbeam')want.gps=$('gps').value;
  if(want.role==='rpttx'){const fo=$('freqout').value.trim();const k=Math.round(parseFloat(fo)*1000);if(!/^\d{3}(\.\d{1,3})?$/.test(fo)||!(k>=430000&&k<=440000)){st('La frequenza di uscita deve essere tra 430.000 e 440.000 MHz.','err');return;}want.freqout=fo;}   // la T3 non ha il GPS: il comando non si manda
  const keys={callsign:'CALLSIGN',freq:'FREQ',freqmsg:'FREQMSG',freqqual:'FREQQUAL',power:'POWER',volume:'VOLUME',micfloor:'MICFLOOR',profile:'PROFILE',pos:'POS',qth:'QTH',gps:'GPS',role:'ROLE',freqout:'FREQOUT'};  // il GPS e' in want solo sulla T-Beam
  const texts=[];for(let i=1;i<=5;i++){const v=$('text'+i).value.trim();if(v!==''&&!/^[\x20-\x7A\x7C\x7E]+$/.test(v)){st('Il testo '+i+' ha caratteri non ammessi: solo ASCII, senza accenti.','err');return;}texts.push(v);}
  try{
    for(const k of Object.keys(keys)){if(want[k]===undefined)continue;const r=await cmd('CFG SET '+keys[k]+' '+want[k]);if(!r.startsWith('CFG OK')){st('Rifiutato ('+keys[k]+'): '+r,'err');return;}}
    const mcds=[];for(let i=1;i<=5;i++){const d=$('mcdest'+i).value.trim().toUpperCase();if(d!==''&&!/^(\*|[A-Z0-9\/-]{1,15})$/.test(d)){st('Destinatario '+i+' non valido: gruppo, nominativo-SSID, * oppure vuoto.','err');return;}mcds.push(d);}
    const mcw=+$('mcwindow').value;if(!(mcw>=30&&mcw<=1800)){st("L'ascolto dopo l'invio deve durare da 30 a 1800 secondi.",'err');return;}
    for(const c of ['CFG SET FREQMC '+$('mcfreq').value.trim(),'CFG SET MCSSID '+$('mcssid').value.trim(),'CFG SET MCWINDOW '+mcw].concat(mcds.map((d,i)=>'CFG SET MCDEST'+(i+1)+' '+(d||'NONE')))){const r=await cmd(c);if(!r.startsWith('CFG OK')){st('Rifiutato ('+c+'): '+r,'err');return;}}
    if(want.role==='rpttx'){const idn=+$('idint').value;if(!(idn>=1&&idn<=10)){st('Il tempo dell\'identificativo deve essere da 1 a 10 minuti.','err');return;}
    for(const c of ['CFG SET IDINT '+idn,'CFG SET IDMORSE '+$('idmorse').value]){const r=await cmd(c);if(!r.startsWith('CFG OK')){st('Rifiutato ('+c+'): '+r,'err');return;}}}
    if(want.role==='rptrx'){const r=await cmd('CFG SET MONITOR '+$('monitor').value);if(!r.startsWith('CFG OK')){st('Rifiutato (MONITOR): '+r,'err');return;}}
    const sy=$('sysop').value.trim().toUpperCase();if(sy!==''&&!/^[A-Z0-9\/]{1,8}$/.test(sy)){st('Il nominativo del sysop non è valido.','err');return;}
    const sk=$('sysopkey').value.trim();if(sk!==''&&!/^[0-9a-fA-F]{32}$/.test(sk)){st('La chiave deve avere 32 cifre esadecimali.','err');return;}
    for(const c of ['CFG SET SYSOP '+(sy||'NONE'),'CFG SET SYSOPMODE '+$('sysopmode').value].concat(sk!==''?['CFG SET SYSOPKEY '+sk]:[])){const r=await cmd(c);if(!r.startsWith('CFG OK')){st('Rifiutato ('+(c.startsWith('CFG SET SYSOPKEY')?'SYSOPKEY':c)+'): '+r,'err');return;}}
    for(let i=0;i<5;i++){const r=await cmd('CFG SET TEXT'+(i+1)+' '+(texts[i]||'NONE'));if(!r.startsWith('CFG OK')){st('Rifiutato (testo '+(i+1)+'): '+r,'err');return;}}
    const back=parse(await cmd('CFG GET'));const bad=[];
    for(const k of Object.keys(want)){let a=String(back[k]),b=String(want[k]);if(k.startsWith('freq')){a=parseFloat(a);b=parseFloat(b);}else if(k==='qth'&&b!=='none'){const x=b.split(',').map(parseFloat),y=a.split(',').map(parseFloat);if(Math.abs(x[0]-y[0])<2e-5&&Math.abs(x[1]-y[1])<2e-5)continue;}else if(k==='callsign'||k==='profile'||k==='role'){a=a.toLowerCase();b=b.toLowerCase();}else if(!isNaN(a)&&!isNaN(b)){a=Number(a);b=Number(b);}
      if(a!==b)bad.push(k);}
    fill(back);await loadSysop();await loadIdint();await loadRole();
    if(bad.length)st('Salvato, ma la scheda riporta valori diversi: '+bad.join(', '),'err');
    else st('Salvato e riletto dalla scheda: tutto coincide. I cambi di profilo, frequenza, potenza e nominativo valgono dal riavvio.','ok');
  }catch(e){st('Errore di comunicazione: '+e.message,'err');}
});
$('reboot').addEventListener('click',async()=>{try{await cmd('CFG REBOOT');}catch(e){}st('Scheda in riavvio: puoi chiudere la pagina.','ok');});
load();
</script></body></html>
)HTML";
