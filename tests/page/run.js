// Prova automatica della pagina di installazione e configurazione (site/index.html) con un dispositivo seriale finto.
//   node tests/page/run.js
// Serve il modulo "playwright" con Chromium (in CI: npm install --no-save playwright; npx playwright install chromium).
// Il componente di installazione (esp-web-tools, da CDN) viene bloccato: qui si prova solo la parte Web Serial.
const http = require('http');
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const ROOT = path.resolve(__dirname, '..', '..');
const SITE = path.join(ROOT, 'site');
const FAKE = path.join(__dirname, 'fake_serial.js');

let publishedVersion = 'test-1';   // modificabile dai test
const server = http.createServer((req, res) => {
  const url = req.url.split('?')[0];
  if (url === '/version.json') {
    res.setHeader('Content-Type', 'application/json');
    return res.end(JSON.stringify({ version: publishedVersion, commit: 'abc1234def5678900000000000000000000000ff' }));
  }
  const file = path.join(SITE, url === '/' ? 'index.html' : url);
  if (!file.startsWith(SITE) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) { res.statusCode = 404; return res.end('no'); }
  res.setHeader('Content-Type', file.endsWith('.html') ? 'text/html; charset=utf-8' : 'application/octet-stream');
  res.end(fs.readFileSync(file));
});

let failures = 0, checks = 0;
function check(name, cond, extra) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL ' + name + (extra !== undefined ? '  -> ' + extra : '')); }
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function until(page, fn, arg, ms = 8000) { await page.waitForFunction(fn, arg, { timeout: ms }); }

let browser, baseUrl;
async function openPage(opts = {}) {
  const page = await browser.newPage({ viewport: { width: 1000, height: 800 }, colorScheme: opts.colorScheme || 'light' });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.route(/esp-web-tools/, (r) => r.abort());     // niente rete: l'installer non si prova qui
  await page.addInitScript({ path: FAKE });
  await page.goto(baseUrl + '/');
  page.errors = errors;
  page.st = (fn, arg) => page.evaluate(fn, arg);
  return page;
}
const status = (page) => page.textContent('#status');
const cmds = (page) => page.evaluate(() => window.__st.cmds.slice());
async function connect(page) {
  await page.click('#connect');
  await until(page, () => document.getElementById('callsign').value !== '');
}
async function scenario(name, fn) {
  const before = failures;
  const page = await openPage();
  try {
    await fn(page);
    check('nessun errore JavaScript nella pagina', page.errors.length === 0, page.errors.join(' | '));
  } catch (e) {
    failures++; checks++;
    console.log('  ECCEZIONE: ' + e.message.split('\n')[0]);
  } finally {
    await page.close();
  }
  console.log((failures === before ? 'ok    ' : 'FALLITO ') + name);
}

(async () => {
  await new Promise((r) => server.listen(0, '127.0.0.1', r));
  baseUrl = 'http://127.0.0.1:' + server.address().port;
  // In CI si usa il Chrome gia' installato sul runner (PW_CHANNEL=chrome): niente download ne' pacchetti di sistema.
  browser = await chromium.launch(process.env.PW_CHANNEL ? { channel: process.env.PW_CHANNEL } : {});

  await scenario('lettura delle impostazioni e versione del firmware', async (p) => {
    check('versione pubblicata mostrata', (await p.textContent('#ver')).includes('test-1'));
    await connect(p);
    check('nominativo', (await p.inputValue('#callsign')) === 'N0CALL');
    check('frequenza', (await p.inputValue('#freq')) === '433.900');
    check('potenza', (await p.inputValue('#power')) === '10');
    check('volume', (await p.inputValue('#volume')) === '7');
    check('fondo del microfono', (await p.inputValue('#micfloor')) === '-60');
    await until(p, () => document.getElementById('fwinfo').textContent.includes('abc1234'));
    check('firmware riconosciuto come quello pubblicato', (await p.textContent('#fwinfo')).includes('pubblicata su questo sito'));
    check('stato di conferma', (await status(p)).includes('Impostazioni lette') && (await p.getAttribute('#status', 'class')) === 'ok');
    check('il messaggio di stato sta subito sotto il modulo', await p.evaluate(() => !!document.querySelector('#form + #status')));
  });

  await scenario('versione beta pubblicata e firmware beta installato', async (p) => {
    publishedVersion = 'v0.1.0-beta.1';
    await p.reload();
    await until(p, () => document.getElementById('ver').textContent.includes('v0.1.0-beta.1'));
    check('la pagina dice che la versione pubblicata e\' di prova', (await p.textContent('#ver')).includes('BETA'), await p.textContent('#ver'));
    await p.evaluate(() => { window.__st.fw = 'v0.1.0-beta.1'; });
    await connect(p);
    await until(p, () => document.getElementById('fwinfo').textContent.includes('beta'));
    const t = await p.textContent('#fwinfo');
    check('firmware beta riconosciuto, con il protocollo', t.includes('BETA') && t.includes('protocollo 1') && t.includes('pubblicata su questo sito'), t);
    publishedVersion = 'v0.1.0-rc.1';
    await p.reload();
    await until(p, () => document.getElementById('ver').textContent.includes('v0.1.0-rc.1'));
    check('release candidate', (await p.textContent('#ver')).includes('RC'));
    publishedVersion = 'v0.1.0';
    await p.reload();
    await until(p, () => document.getElementById('ver').textContent.includes('v0.1.0'));
    check('versione stabile senza etichetta di prova', !/BETA|RC|sviluppo/.test(await p.textContent('#ver')), await p.textContent('#ver'));
    publishedVersion = 'main-07/11/2026';
    await p.reload();
    await until(p, () => document.getElementById('ver').textContent.includes('main-07/11/2026'));
    check('versione di sviluppo', (await p.textContent('#ver')).includes('sviluppo'));
    publishedVersion = 'test-1';
  });

  await scenario('firmware vecchio senza CFG VERSION: la pagina funziona lo stesso', async (p) => {
    await p.evaluate(() => { window.__st.noVersion = true; });
    await connect(p);
    await until(p, () => document.getElementById('fwinfo').textContent.includes('non disponibile'), null, 6000);
    check('impostazioni lette comunque', (await p.inputValue('#callsign')) === 'N0CALL');
    check('pulsanti attivi', !(await p.isDisabled('#save')));
  });

  await scenario('valori non validi: nessun comando di scrittura inviato', async (p) => {
    await connect(p);
    const base = { callsign: 'IU5HJU', freq: '434.125', power: '12', volume: '4', micfloor: '-58' };
    const cases = [
      [{ freq: '868.0' }, 'frequenza'],
      [{ freq: '433,9' }, 'frequenza'],
      [{ power: '20' }, 'potenza'],
      [{ volume: '11' }, 'volume'],
      [{ micfloor: '-90' }, 'fondo'],
      [{ callsign: 'IU5-HJU' }, 'ominativo'],
    ];
    for (const [over, word] of cases) {
      const v = { ...base, ...over };
      await p.fill('#callsign', v.callsign); await p.fill('#freq', v.freq); await p.fill('#power', v.power);
      await p.fill('#volume', v.volume); await p.evaluate((x) => { document.getElementById('micfloor').value = x; }, v.micfloor);
      await p.click('#save');
      const s = await status(p);
      check('errore per ' + word, s.toLowerCase().includes(word.toLowerCase()), s);
      check('classe errore', (await p.getAttribute('#status', 'class')) === 'err');
    }
    check('nessun SET inviato', !(await cmds(p)).some((c) => c.startsWith('CFG SET')));
  });

  await scenario('salvataggio valido: comandi nell\'ordine e riavvio', async (p) => {
    await connect(p);
    await p.fill('#callsign', 'iu5hju'); await p.fill('#freq', '434.125'); await p.fill('#power', '12');
    await p.fill('#volume', '4');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    const c = await cmds(p);
    const idx = (s) => c.indexOf(s);
    check('callsign maiuscolo', idx('CFG SET CALLSIGN IU5HJU') >= 0, c.join(' | '));
    const order = ['CFG SET CALLSIGN IU5HJU', 'CFG SET FREQ 434.125', 'CFG SET POWER 12', 'CFG SET VOLUME 4', 'CFG SET MICFLOOR -60', 'CFG REBOOT'].map(idx);
    check('ordine dei comandi', order.every((v, i) => v >= 0 && (i === 0 || v > order[i - 1])), c.join(' | '));
    check('messaggio di salvataggio', (await status(p)).includes('Salvato'));
    const st = await p.evaluate(() => window.__st.cfg);
    check('valori nella scheda', st.callsign === 'IU5HJU' && st.freq === '434.125' && st.power === 12 && st.volume === 4);
  });

  await scenario('errore del firmware su un valore: la pagina lo mostra e non riavvia', async (p) => {
    await connect(p);
    await p.evaluate(() => { window.__st.reject = 'FREQ'; });
    await p.fill('#callsign', 'IU5HJU'); await p.fill('#freq', '434.125');
    await p.click('#save');
    await until(p, () => /Errore/.test(document.getElementById('status').textContent));
    check('messaggio con l\'errore della scheda', (await status(p)).includes('CFG ERR freq'), await status(p));
    check('classe errore', (await p.getAttribute('#status', 'class')) === 'err');
    check('nessun riavvio', !(await cmds(p)).includes('CFG REBOOT'));
  });

  await scenario('fondo del microfono: + e - e valore fuori campo', async (p) => {
    await connect(p);
    await p.click('#floorUp');
    await until(p, () => window.__st.cmds.includes('CFG SET MICFLOOR -59'));
    check('campo aggiornato', (await p.inputValue('#micfloor')) === '-59');
    await p.click('#floorDown'); await p.click('#floorDown');
    await until(p, () => window.__st.cmds.filter((c) => c === 'CFG SET MICFLOOR -60' || c === 'CFG SET MICFLOOR -61').length >= 2);
    check('due passi in giu', (await p.inputValue('#micfloor')) === '-61');
    await p.fill('#micfloor', '-90'); await p.press('#micfloor', 'Tab');
    await sleep(200);
    check('il campo torna all\'ultimo valore valido', (await p.inputValue('#micfloor')) === '-61', await p.inputValue('#micfloor'));
    check('messaggio con il valore che resta', (await status(p)).includes('resta -61'), await status(p));
    check('il valore errato non e\' stato inviato', !(await cmds(p)).includes('CFG SET MICFLOOR -90'));
  });

  const measureCases = [
    ['misura valida: salvata', { fondo: -59, min: -62, max: -57 }, (s, f) => s.includes('Fondo misurato: -59') && f === '-59'],
    ['misura instabile: non salvata', { fondo: -40, min: -62, max: -28 }, (s, f) => s.includes('MISURA NON SALVATA') && s.includes('cambiato troppo') && f === '-60'],
    ['microfono assente (-120 dBFS): non salvata', { fondo: -120, min: -120, max: -120 }, (s, f) => s.includes('MISURA NON SALVATA') && s.includes('nessun segnale') && f === '-60'],
    ['fondo troppo alto: non salvata', { fondo: -30, min: -31, max: -29 }, (s, f) => s.includes('troppo alto') && f === '-60'],
    ['fondo sotto il minimo: salvato -80 e detto chiaramente', { fondo: -90, min: -92, max: -88 }, (s, f) => s.includes('salvato -80') && f === '-80'],
  ];
  for (const [name, m, ok] of measureCases) {
    await scenario('Misura il fondo, ' + name, async (p) => {
      await connect(p);
      await p.evaluate((mm) => { window.__st.measure = { ...mm, frames: 75 }; }, m);
      await p.click('#measure');
      await until(p, () => /Fondo misurato|MISURA NON SALVATA|Errore/.test(document.getElementById('status').textContent), null, 10000);
      const s = await status(p), f = await p.inputValue('#micfloor');
      check(name, ok(s, f), s + ' | campo=' + f);
      check('i pulsanti tornano attivi', !(await p.isDisabled('#measure')));
    });
  }

  await scenario('installazione: scheda senza display', async (p) => {
    const opts = await p.$$eval('#board option', (o) => o.map((x) => x.value));
    check('tre schede nell\'elenco', opts.join() === 't3,tbeam,tbeam-nodisplay', opts.join());
    await p.selectOption('#board', 'tbeam-nodisplay');
    check('il manifest segue la scelta', (await p.getAttribute('#installer', 'manifest')) === 'firmware/tbeam-nodisplay/manifest.json');
  });

  await scenario('registra e trasmetti: attiva e disattiva', async (p) => {
    await connect(p);
    check('pulsanti attivi dopo la connessione', !(await p.isDisabled('#recOn')) && !(await p.isDisabled('#recOff')));
    await p.click('#recOn');
    await until(p, () => document.getElementById('recState').textContent.includes('ATTIVO'));
    check('CFG REC ON inviato', (await cmds(p)).includes('CFG REC ON'));
    check('spiega che parte dopo 1 secondo', (await status(p)).includes('dopo 1 secondo'));
    await p.click('#recOff');
    await until(p, () => document.getElementById('recState').textContent.includes('spento'));
    check('CFG REC OFF inviato', (await cmds(p)).includes('CFG REC OFF'));
  });

  await scenario('prova di collegamento: attiva e disattiva', async (p) => {
    await connect(p);
    await p.click('#testOn');
    await until(p, () => document.getElementById('testState').textContent.includes('ATTIVA'));
    check('CFG TEST ON inviato', (await cmds(p)).includes('CFG TEST ON'));
    await p.click('#testOff');
    await until(p, () => document.getElementById('testState').textContent.includes('spenta'));
    check('CFG TEST OFF inviato', (await cmds(p)).includes('CFG TEST OFF'));
    check('l\'indicazione dei tre clic e\' nella pagina', (await p.textContent('#config')).includes('tre pressioni veloci del PTT'));
  });

  await scenario('verifica audio: microfono che sente', async (p) => {
    await connect(p);
    await p.evaluate(() => { window.__st.measureSeq = [{ fondo: -61, min: -63, max: -58 }, { fondo: -24, min: -60, max: -22 }]; });
    await p.click('#hwMic');
    await until(p, () => document.getElementById('hwMicOut').textContent.includes('Il microfono sente'), null, 12000);
    check('differenza +37 dB', (await p.textContent('#hwMicOut')).includes('+37'));
  });

  await scenario('verifica audio: microfono assente', async (p) => {
    await connect(p);
    await p.evaluate(() => { window.__st.measure = { fondo: -120, min: -120, max: -120 }; });
    await p.click('#hwMic');
    await until(p, () => document.getElementById('hwMicOut').textContent.includes('nessun segnale'), null, 12000);
    check('messaggio di errore', (await p.getAttribute('#hwMicOut', 'class')).includes('err'));
  });

  await scenario('verifica audio: microfono con differenza piccola', async (p) => {
    await connect(p);
    await p.evaluate(() => { window.__st.measureSeq = [{ fondo: -61, min: -63, max: -58 }, { fondo: -52, min: -60, max: -45 }]; });
    await p.click('#hwMic');
    await until(p, () => document.getElementById('hwMicOut').textContent.includes('Differenza piccola'), null, 12000);
  });

  await scenario('verifica audio: toni sentiti', async (p) => {
    await connect(p);
    await p.click('#hwTones');
    await until(p, () => document.querySelectorAll('#hwTonesOut tr').length === 5, null, 12000);
    const t = await p.textContent('#hwTonesOut');
    check('tre toni sentiti', (t.match(/sentito/g) || []).length >= 3, t);
    check('giudizio positivo', t.includes('sembrano vivi'));
  });

  await scenario('verifica audio: toni non sentiti', async (p) => {
    await connect(p);
    await p.evaluate(() => { window.__st.audio = window.__st.audio.map((a) => ({ ...a, tone: a.base + 1 })); });
    await p.click('#hwTones');
    await until(p, () => document.getElementById('hwTonesOut').textContent.includes('non ha sentito'), null, 12000);
    check('indica le cause', (await p.textContent('#hwTonesOut')).includes('GPIO25'));
  });

  await scenario('verifica audio: risultato dubbio', async (p) => {
    await connect(p);
    await p.evaluate(() => { window.__st.audio = [{ f: 500, base: -60, tone: -52 }, { f: 1000, base: -60, tone: -52 }, { f: 2000, base: -60, tone: -52 }]; });
    await p.click('#hwTones');
    await until(p, () => document.getElementById('hwTonesOut').textContent.includes('dubbio'), null, 12000);
  });

  const base = [{ f: 500, base: -63.3, tone: -44.3 }, { f: 1000, base: -68.8, tone: -25.1 }, { f: 2000, base: -76.4, tone: -38.2 }];
  const rfObj = (dBase, dTone, rms, agc, dc) => ({ audio: base.map((a) => ({ f: a.f, base: a.base + dBase, tone: a.tone + dTone })), rms, agc, dc });
  const rfScenario = async (name, rf, expect, notExpect) => scenario(name, async (p) => {
    await connect(p);
    await p.evaluate((r) => { window.__st.rf = r; }, rf);
    await p.click('#hwTones');
    await until(p, () => !!document.querySelector('#hwTonesOut table'), null, 15000);
    const t = await p.textContent('#hwTonesOut');
    check(name + ': esito', t.includes(expect), t);
    if (notExpect) check(name + ': non confonde', !t.includes(notExpect), t);
  });
  await rfScenario('fase 2: nessun rientro', rfObj(0.5, -0.5, -25.0, -37.0, 1.2), 'nessun rientro di RF', 'rientra nel microfono');
  await rfScenario('fase 2: rientro forte', rfObj(30, 0, -5.0, -10.0, 40.0), 'la radiofrequenza rientra nel microfono', 'nessun rientro');
  await rfScenario('fase 2: rientro dubbio', rfObj(7, 0, -22.0, -30.0, 2.0), 'dubbio');
  await rfScenario('fase 2: toni coperti dalla radio', rfObj(0, -20, -25.0, -37.0, 1.0), 'si sentono peggio');
  await rfScenario('fase 2: senza nominativo', 'CFG AUDIO rf skip=nominativo', 'non eseguita');
  await scenario('fase 2: tabella con le due fasi', async (p) => {
    await connect(p);
    await p.evaluate((r) => { window.__st.rf = r; }, rfObj(0.5, -0.5, -25.0, -37.0, 1.2));
    await p.click('#hwTones');
    await until(p, () => document.querySelectorAll('#hwTonesOut tr').length === 5, null, 15000);
    const t = await p.textContent('#hwTonesOut');
    check('tabella con la fase radio', t.includes('Fase 2: con la radio in trasmissione') && t.includes('Pacchetti trasmessi: 14'), t);
  });

  await scenario('registro: salvataggio su file', async (p) => {
    await connect(p);
    const [dl] = await Promise.all([p.waitForEvent('download'), p.click('#logSave')]);
    check('nome del file con la data giorno-mese-anno', /^lora-codec2-seriale-\d{2}-\d{2}-\d{4}_\d{2}-\d{2}-\d{2}\.txt$/.test(dl.suggestedFilename()), dl.suggestedFilename());
    await p.click('#logClear');
    check('registro svuotato', (await p.textContent('#log')) === '');
  });

  // tema scuro: la pagina si apre e i testi di stato restano leggibili (contrasto minimo del testo di errore)
  {
    const p = await openPage({ colorScheme: 'dark' });
    await connect(p);
    await p.evaluate(() => { window.__st.measure = { fondo: -120, min: -120, max: -120, frames: 75 }; });
    await p.click('#measure');
    await until(p, () => document.getElementById('status').textContent.includes('NON SALVATA'), null, 10000);
    const contrast = await p.evaluate(() => {
      const el = document.getElementById('status'), cs = getComputedStyle(el);
      const lum = (c) => { const m = c.match(/[\d.]+/g).map(Number); const f = (v) => { v /= 255; return v <= 0.03928 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }; return 0.2126 * f(m[0]) + 0.7152 * f(m[1]) + 0.0722 * f(m[2]); };
      let bg = null; for (let n = el; n && !bg; n = n.parentElement) { const b = getComputedStyle(n).backgroundColor; if (b && !/rgba\(0, 0, 0, 0\)/.test(b) && !/rgba\([^)]*,\s*0\.\d+\)/.test(b)) bg = b; }
      const l1 = lum(cs.color), l2 = lum(bg || 'rgb(20,24,31)');
      return (Math.max(l1, l2) + 0.05) / (Math.min(l1, l2) + 0.05);
    });
    check('contrasto del messaggio di errore nel tema scuro (almeno 4,5)', contrast >= 4.5, contrast.toFixed(2));
    check('nessun errore JavaScript (tema scuro)', p.errors.length === 0);
    console.log((contrast >= 4.5 ? 'ok    ' : 'FALLITO ') + 'tema scuro');
    await p.close();
  }

  await browser.close();
  server.close();
  console.log(`\n${checks} controlli, ${failures} falliti`);
  process.exit(failures ? 1 : 0);
})().catch((e) => { console.error(e); process.exit(2); });
