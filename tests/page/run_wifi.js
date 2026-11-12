// Prova della pagina interna della scheda (src/wifi_page.h) con una scheda finta che risponde a /cfg?c=<riga CFG>.
//   node tests/page/run_wifi.js
const http = require('http');
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const src = fs.readFileSync(path.resolve(__dirname, '..', '..', 'src', 'wifi_page.h'), 'utf8');
const m = /R"HTML\(([\s\S]*?)\)HTML"/.exec(src);
if (!m) { console.log('pagina non trovata in wifi_page.h'); process.exit(1); }
const PAGE = m[1];

const st = {
  cfg: { callsign: 'IU5HJU', profile: 'std', freq: '433.900', freqmsg: '433.900', freqqual: '433.900', power: 10, volume: 7, micfloor: -60, pos: 'off', qth: 'none', gps: 'off', role: 'off', freqout: '433.900' },
  monitor: 'off',
  cmds: [],
  board: 't3',
  texts: ['%C QRV su VoLoRa modo %M %F', 'Arrivo tra 10 minuti', 'Chiamami appena puoi', 'Tutto OK', '73 e buona giornata'],
  mc: { freq: '433.175', ssid: '99', window: '300', dests: ['22251', '222', '*', '', ''] },
  idint: '10',
  idmorse: 'on',
  sysop: { call: 'none', key: 'none', mode: 'off', keyHex: '' },
  drop: null,   // chiave che la scheda "dimentica" di salvare
};
const KEYS = ['callsign', 'freq', 'freqmsg', 'freqqual', 'power', 'volume', 'micfloor', 'profile', 'pos', 'qth', 'gps', 'role', 'freqout'];
function handle(line) {
  st.cmds.push(line);
  if (line === 'CFG GET MONITOR') return 'CFG MONITOR=' + st.monitor;
  const mo = /^CFG SET MONITOR (on|off)$/.exec(line);
  if (mo) { st.monitor = mo[1]; return 'CFG OK monitor=' + mo[1]; }
  if (line === 'CFG GET IDMORSE') return 'CFG IDMORSE=' + st.idmorse;
  const dm = /^CFG SET IDMORSE (on|off)$/.exec(line);
  if (dm) { st.idmorse = dm[1]; return 'CFG OK idmorse=' + dm[1]; }
  if (line === 'CFG GET IDINT') return 'CFG IDINT=' + st.idint;
  const im = /^CFG SET IDINT (\d+)$/.exec(line);
  if (im) { if (+im[1] < 1 || +im[1] > 10) return 'CFG ERR idint da 1 a 10'; st.idint = im[1]; return 'CFG OK idint=' + im[1]; }
  if (line === 'CFG GET SYSOP') return 'CFG SYSOP call=' + st.sysop.call + ' key=' + st.sysop.key + ' mode=' + st.sysop.mode;
  let sm = /^CFG SET SYSOP (.+)$/.exec(line);
  if (sm) { st.sysop.call = sm[1] === 'NONE' ? 'none' : sm[1]; return 'CFG OK sysop=' + st.sysop.call; }
  sm = /^CFG SET SYSOPMODE (on|off)$/.exec(line);
  if (sm) { st.sysop.mode = sm[1]; return 'CFG OK sysopmode=' + sm[1]; }
  sm = /^CFG SET SYSOPKEY (.+)$/.exec(line);
  if (sm) { if (sm[1] !== 'NONE' && !/^[0-9a-fA-F]{32}$/.test(sm[1])) return 'CFG ERR sysopkey servono 32 cifre'; st.sysop.key = sm[1] === 'NONE' ? 'none' : 'set'; st.sysop.keyHex = sm[1]; return 'CFG OK sysopkey=' + st.sysop.key; }
  const tm = /^CFG SET TEXT([1-5]) (.+)$/.exec(line);
  if (tm) { st.texts[+tm[1] - 1] = tm[2] === 'NONE' ? '' : tm[2]; return 'CFG OK text' + tm[1] + '=' + (tm[2] === 'NONE' ? '' : tm[2]); }
  const tg = /^CFG GET TEXT([1-5])$/.exec(line);
  if (tg) return 'CFG TEXT' + tg[1] + '=' + st.texts[+tg[1] - 1];
  if (line === 'CFG GET MC') return 'CFG MC freqmc=' + st.mc.freq + ' ssid=' + st.mc.ssid + ' window=' + st.mc.window;
  const gd = /^CFG GET MCDEST([1-5])$/.exec(line);
  if (gd) return 'CFG MCDEST' + gd[1] + '=' + st.mc.dests[+gd[1] - 1];
  const md = /^CFG SET MCDEST([1-5]) (.+)$/.exec(line);
  if (md) { st.mc.dests[+md[1] - 1] = md[2] === 'NONE' ? '' : md[2]; return 'CFG OK mcdest' + md[1] + '=' + (md[2] === 'NONE' ? '' : md[2]); }
  const mm = /^CFG SET (FREQMC|MCSSID|MCWINDOW) (.+)$/.exec(line);
  if (mm) {
    const k = mm[1].toLowerCase();
    if (k === 'freqmc') st.mc.freq = mm[2]; else if (k === 'mcssid') st.mc.ssid = mm[2]; else st.mc.window = mm[2];
    return 'CFG OK ' + k + '=' + mm[2];
  }
  const t = line.split(' ');
  if (t[1] === 'GET') return 'CFG ' + KEYS.map((k) => k + '=' + st.cfg[k]).join(' ') + ' board=' + st.board + ' reboot=0 test=0';
  if (t[1] === 'SET') {
    const k = t[2].toLowerCase();
    if (k === 'gps' && st.board !== 'tbeam') return 'CFG ERR gps questa scheda non ha il GPS';
    if (k === 'role') { st.cfg.role = t[3].toLowerCase(); return 'CFG OK role=' + st.cfg.role; }
    if (k === 'freqout' && !(+t[3] >= 430 && +t[3] <= 440)) return 'CFG ERR freqout 430-440';
    if (k === 'power' && (+t[3] < 2 || +t[3] > 17)) return 'CFG ERR power da 2 a 17';
    if (st.drop !== k) st.cfg[k] = k === 'callsign' ? t[3].toUpperCase() : t[3];
    return 'CFG OK ' + k + '=' + t[3];
  }
  if (t[1] === 'REBOOT') return 'CFG OK reboot';
  return 'CFG ERR comando sconosciuto';
}
const server = http.createServer((req, res) => {
  const u = new URL(req.url, 'http://x');
  if (u.pathname === '/') { res.setHeader('Content-Type', 'text/html; charset=utf-8'); return res.end(PAGE); }
  if (u.pathname === '/cfg') { res.setHeader('Content-Type', 'text/plain'); return res.end(handle(u.searchParams.get('c'))); }
  res.statusCode = 404; res.end('no');
});

let failures = 0, checks = 0;
function check(name, cond, extra) { checks++; if (!cond) { failures++; console.log('  FAIL ' + name + (extra !== undefined ? ' -> ' + extra : '')); } }

(async () => {
  await new Promise((r) => server.listen(0, r));
  const url = 'http://127.0.0.1:' + server.address().port + '/';
  const browser = await chromium.launch(process.env.PW_CHANNEL ? { channel: process.env.PW_CHANNEL } : {});
  const p = await browser.newPage();
  await p.goto(url);
  await p.waitForFunction(() => document.getElementById('callsign').value !== '');
  check('lettura iniziale', (await p.inputValue('#callsign')) === 'IU5HJU' && (await p.inputValue('#freqqual')) === '433.900');
  // La pagina legge prima i valori principali, poi MeshCom e i testi (decine di richieste): lo stato "letti" compare alla fine
  await p.waitForFunction(() => /letti/.test(document.getElementById('st').textContent), null, { timeout: 15000 });
  check('stato di lettura', /letti/.test(await p.textContent('#st')));

  check('testi letti', (await p.inputValue('#text1')) === '%C QRV su VoLoRa modo %M %F' && (await p.inputValue('#text5')) === '73 e buona giornata');
  await p.fill('#text2', 'Sono QRV su 433.775, chiamami'); await p.fill('#text4', '');
  check('valori MeshCom letti', (await p.inputValue('#mcfreq')) === '433.175' && (await p.inputValue('#mcssid')) === '99' && (await p.inputValue('#mcwindow')) === '300' && (await p.inputValue('#mcdest1')) === '22251' && (await p.inputValue('#mcdest3')) === '*');
  await p.fill('#mcdest4', 'iu5abc-9'); await p.fill('#mcssid', '12'); await p.fill('#mcwindow', '90');
  await p.fill('#callsign', 'iu5hju/p'); await p.selectOption('#profile', 'qua'); await p.fill('#freqqual', '434.250');
  await p.fill('#qth', '43.7696,11.2558'); await p.selectOption('#pos', 'on');
  await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('salvato e riletto', /tutto coincide/.test(await p.textContent('#st')), await p.textContent('#st'));
  check('comandi inviati', st.cmds.includes('CFG SET PROFILE qua') && st.cmds.includes('CFG SET FREQQUAL 434.250') && st.cmds.includes('CFG SET QTH 43.7696,11.2558'), st.cmds.join('|'));
  check('testi salvati (con spazi) e posto vuoto con NONE', st.cmds.includes('CFG SET TEXT2 Sono QRV su 433.775, chiamami') && st.cmds.includes('CFG SET TEXT4 NONE') && st.texts[1] === 'Sono QRV su 433.775, chiamami' && st.texts[3] === '', JSON.stringify(st.texts));
  check('MeshCom salvato (destinatario in maiuscolo, vuoto = NONE)', st.mc.dests[3] === 'IU5ABC-9' && st.mc.dests[4] === '' && st.mc.ssid === '12' && st.mc.window === '90' && st.cmds.includes('CFG SET MCDEST4 IU5ABC-9') && st.cmds.includes('CFG SET MCDEST5 NONE'), JSON.stringify(st.mc));
  check('rilettura dopo il salvataggio', st.cmds[st.cmds.length - 5] === 'CFG GET' && st.cmds[st.cmds.length - 4] === 'CFG GET SYSOP' && st.cmds[st.cmds.length - 3] === 'CFG GET IDINT' && st.cmds[st.cmds.length - 2] === 'CFG GET IDMORSE' && st.cmds[st.cmds.length - 1] === 'CFG GET MONITOR');
  check('nominativo maiuscolo', st.cfg.callsign === 'IU5HJU/P');

  // Modo Sysop: di partenza spento (testi visibili, elenco comandi nascosto), chiave non impostata
  check('sysop: lettura iniziale', (await p.inputValue('#sysopmode')) === 'off' && (await p.inputValue('#sysop')) === '' && /non impostata/.test(await p.textContent('#sysopkeyhint')));
  check('sysop spento: testi visibili, elenco nascosto', (await p.isVisible('#textsbox')) && (await p.isHidden('#sysopbox')));
  check('sysop: senza chiave nuova non si manda SYSOPKEY', !st.cmds.some((c) => c.startsWith('CFG SET SYSOPKEY')));
  await p.click('#genkey');
  const gk = await p.inputValue('#sysopkey');
  check('chiave generata: 32 cifre esadecimali, diversa a ogni volta', /^[0-9a-f]{32}$/.test(gk));
  await p.click('#genkey');
  check('seconda chiave diversa', (await p.inputValue('#sysopkey')) !== gk);
  const gk2 = await p.inputValue('#sysopkey');
  await p.selectOption('#sysopmode', 'on');
  check('sysop acceso: elenco comandi visibile e testi nascosti', (await p.isVisible('#sysopbox')) && (await p.isHidden('#textsbox')) && (await p.locator('#sysoplist li').count()) === 10);
  await p.fill('#sysop', 'iu5hju');
  await p.fill('#sysopkey', 'abc');
  await p.click('#save');
  await p.waitForFunction(() => /chiave|Salvato/.test(document.getElementById('st').textContent));
  check('chiave non valida bloccata', /32 cifre/.test(await p.textContent('#st')) && st.sysop.mode === 'off');
  await p.fill('#sysopkey', gk2);
  await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('sysop salvato (nominativo maiuscolo, modo, chiave)', st.sysop.call === 'IU5HJU' && st.sysop.mode === 'on' && st.sysop.key === 'set' && st.sysop.keyHex === gk2, JSON.stringify(st.sysop));
  check('dopo il salvataggio la chiave non si rilegge', (await p.inputValue('#sysopkey')) === '' && /impostata/.test(await p.textContent('#sysopkeyhint')) && (await p.inputValue('#sysopmode')) === 'on');
  const nk = st.cmds.filter((c) => c.startsWith('CFG SET SYSOPKEY')).length;
  await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('chiave lasciata vuota: non cambia', st.cmds.filter((c) => c.startsWith('CFG SET SYSOPKEY')).length === nk && st.sysop.keyHex === gk2);
  await p.selectOption('#sysopmode', 'off'); await p.fill('#sysop', '');
  await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('sysop spento e nominativo tolto', st.sysop.mode === 'off' && st.sysop.call === 'none' && (await p.isVisible('#textsbox')));

  // Ruolo: di base stazione normale, riquadri del ripetitore nascosti e nessun comando del ripetitore
  check('ruolo letto: stazione normale, riquadri nascosti', (await p.inputValue('#role')) === 'off' && (await p.isHidden('#rxbox')) && (await p.isHidden('#txbox')));
  check('stazione normale: nessun comando ROLE a parte quello del ruolo, nessun IDINT/MONITOR', !st.cmds.some((c) => /^CFG SET (IDINT|MONITOR|FREQOUT)/.test(c)) && st.cmds.includes('CFG SET ROLE off'));
  await p.selectOption('#role', 'rpttx');
  check('scheda TX: compaiono frequenza di uscita e identificativo, non il monitor', (await p.isVisible('#txbox')) && (await p.isHidden('#rxbox')) && (await p.inputValue('#idint')) === '10' && (await p.inputValue('#freqout')) === '433.900');
  await p.fill('#freqout', '999'); await p.click('#save');
  await p.waitForTimeout(400);
  check('frequenza di uscita fuori limite bloccata', st.cfg.role === 'off' && !st.cmds.some((c) => c.startsWith('CFG SET FREQOUT')));
  await p.fill('#freqout', '434.400'); await p.fill('#idint', '11'); await p.click('#save');
  await p.waitForTimeout(400);
  check('identificativo 11 minuti bloccato', st.idint === '10');
  await p.fill('#idint', '3'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('TX salvata: ruolo, frequenza di uscita e identificativo', st.cfg.role === 'rpttx' && st.cfg.freqout === '434.400' && st.idint === '3' && st.cmds.includes('CFG SET ROLE rpttx') && st.cmds.includes('CFG SET FREQOUT 434.400') && st.cmds.includes('CFG SET IDINT 3'), st.cmds.slice(-12).join('|'));
  check('identificativo in Morse: letto (acceso di base) e salvato', (await p.inputValue('#idmorse')) === 'on' && st.idmorse === 'on' && st.cmds.includes('CFG SET IDMORSE on'));
  await p.selectOption('#idmorse', 'off'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('Morse spento salvato e riletto', st.idmorse === 'off' && (await p.inputValue('#idmorse')) === 'off');
  await p.selectOption('#idmorse', 'on'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('rilettura: ruolo TX e riquadro visibile', (await p.inputValue('#role')) === 'rpttx' && (await p.isVisible('#txbox')) && (await p.inputValue('#idint')) === '3');
  await p.selectOption('#role', 'rptrx');
  check('scheda RX: compare il monitor, non la frequenza di uscita', (await p.isVisible('#rxbox')) && (await p.isHidden('#txbox')));
  await p.selectOption('#monitor', 'on'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('RX salvata: ruolo e monitor', st.cfg.role === 'rptrx' && st.monitor === 'on' && st.cmds.includes('CFG SET MONITOR on'));
  await p.selectOption('#role', 'off'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('di nuovo stazione normale', st.cfg.role === 'off' && (await p.isHidden('#rxbox')) && (await p.isHidden('#txbox')));

  check('GPS nascosto sulla T3', await p.isHidden('#gps'));
  check('nessun comando GPS sulla T3', !st.cmds.some((c) => c.startsWith('CFG SET GPS')));

  st.drop = 'volume';   // la scheda non salva il volume: la pagina deve accorgersene
  await p.fill('#volume', '3'); await p.click('#save');
  await p.waitForFunction(() => /diversi/.test(document.getElementById('st').textContent));
  check('impostazione non salvata segnalata', /volume/.test(await p.textContent('#st')), await p.textContent('#st'));
  st.drop = null;

  await p.fill('#power', '99'); await p.click('#save');
  await p.waitForTimeout(400);
  check('valore fuori limite bloccato dal modulo', !st.cmds.includes('CFG SET POWER 99'));
  await p.fill('#power', '10');

  // Sulla T-Beam il campo GPS compare e il comando parte; il ruolo del ripetitore e' spento (solo T3)
  st.board = 'tbeam';
  await p.reload();
  await p.waitForFunction(() => !document.getElementById('gpsbox').hidden);
  await p.selectOption('#gps', 'on');
  await p.click('#save');
  await p.waitForFunction(() => /Salvato|diversi|Rifiutato/.test(document.getElementById('st').textContent));
  check('T-Beam: ruolo del ripetitore non modificabile', (await p.isDisabled('#role')) && (await p.isVisible('#rolenote')));
  check('GPS sulla T-Beam', st.cmds.includes('CFG SET GPS on') && st.cfg.gps === 'on', st.cmds.slice(-3).join('|'));

  const n = st.cmds.length;
  await p.click('#reboot');
  await p.waitForFunction(() => /riavvio/.test(document.getElementById('st').textContent));
  check('riavvio richiesto', st.cmds.slice(n).includes('CFG REBOOT'));

  await browser.close();
  server.close();
  console.log('\n' + checks + ' controlli, ' + failures + ' falliti');
  process.exit(failures ? 1 : 0);
})();
