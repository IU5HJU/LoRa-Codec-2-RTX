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
  cfg: { callsign: 'IU5HJU', profile: 'std', freq: '433.900', freqmsg: '433.900', freqqual: '433.900', power: 10, volume: 7, micfloor: -60, pos: 'off', qth: 'none', gps: 'off' },
  cmds: [],
  drop: null,   // chiave che la scheda "dimentica" di salvare
};
const KEYS = ['callsign', 'freq', 'freqmsg', 'freqqual', 'power', 'volume', 'micfloor', 'profile', 'pos', 'qth', 'gps'];
function handle(line) {
  st.cmds.push(line);
  const t = line.split(' ');
  if (t[1] === 'GET') return 'CFG ' + KEYS.map((k) => k + '=' + st.cfg[k]).join(' ') + ' board=t3 reboot=0 test=0';
  if (t[1] === 'SET') {
    const k = t[2].toLowerCase();
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
  check('stato di lettura', /letti/.test(await p.textContent('#st')));

  await p.fill('#callsign', 'iu5hju/p'); await p.selectOption('#profile', 'qua'); await p.fill('#freqqual', '434.250');
  await p.fill('#qth', '43.7696,11.2558'); await p.selectOption('#pos', 'on');
  await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('salvato e riletto', /tutto coincide/.test(await p.textContent('#st')), await p.textContent('#st'));
  check('comandi inviati', st.cmds.includes('CFG SET PROFILE qua') && st.cmds.includes('CFG SET FREQQUAL 434.250') && st.cmds.includes('CFG SET QTH 43.7696,11.2558'), st.cmds.join('|'));
  check('rilettura dopo il salvataggio', st.cmds[st.cmds.length - 1] === 'CFG GET');
  check('nominativo maiuscolo', st.cfg.callsign === 'IU5HJU/P');

  st.drop = 'volume';   // la scheda non salva il volume: la pagina deve accorgersene
  await p.fill('#volume', '3'); await p.click('#save');
  await p.waitForFunction(() => /diversi/.test(document.getElementById('st').textContent));
  check('impostazione non salvata segnalata', /volume/.test(await p.textContent('#st')), await p.textContent('#st'));
  st.drop = null;

  await p.fill('#power', '99'); await p.click('#save');
  await p.waitForTimeout(400);
  check('valore fuori limite bloccato dal modulo', !st.cmds.includes('CFG SET POWER 99'));
  await p.fill('#power', '10');

  const n = st.cmds.length;
  await p.click('#reboot');
  await p.waitForFunction(() => /riavvio/.test(document.getElementById('st').textContent));
  check('riavvio richiesto', st.cmds.slice(n).includes('CFG REBOOT'));

  await browser.close();
  server.close();
  console.log('\n' + checks + ' controlli, ' + failures + ' falliti');
  process.exit(failures ? 1 : 0);
})();
