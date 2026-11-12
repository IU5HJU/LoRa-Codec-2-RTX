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
let versionsData = null;           // versions.json (menu delle versioni); null = assente, come nelle pagine di prima del menu
const server = http.createServer((req, res) => {
  const url = req.url.split('?')[0];
  if (url === '/version.json') {
    res.setHeader('Content-Type', 'application/json');
    return res.end(JSON.stringify({ version: publishedVersion, commit: 'abc1234def5678900000000000000000000000ff' }));
  }
  if (url === '/versions.json' && versionsData) {
    res.setHeader('Content-Type', 'application/json');
    return res.end(JSON.stringify(versionsData));
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
  // Passo del conto alla rovescia prima delle misure del silenzio: breve nei test, 1 s nella pagina vera.
  await page.addInitScript((ms) => { window.LCR_COUNTDOWN_STEP_MS = ms; }, opts.countdownStep ?? 30);
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
async function scenario(name, fn, opts = {}) {
  const before = failures;
  const page = await openPage(opts);
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
    check('firmware beta riconosciuto, con il protocollo', t.includes('BETA') && t.includes('protocollo 2') && t.includes('pubblicata su questo sito'), t);
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
    const order = ['CFG SET CALLSIGN IU5HJU', 'CFG SET FREQ 434.125', 'CFG SET FREQMSG 433.900', 'CFG SET FREQQUAL 433.900', 'CFG SET POWER 12', 'CFG SET VOLUME 4', 'CFG SET MICFLOOR -60', 'CFG SET PROFILE std', 'CFG REBOOT'].map(idx);
    check('ordine dei comandi', order.every((v, i) => v >= 0 && (i === 0 || v > order[i - 1])), c.join(' | '));
    check('messaggio di salvataggio', (await status(p)).includes('Salvato'));
    const st = await p.evaluate(() => window.__st.cfg);
    check('valori nella scheda', st.callsign === 'IU5HJU' && st.freq === '434.125' && st.power === 12 && st.volume === 4);
  });

  // ---- Menu delle versioni ----
  const DEV = { id: 'dev', version: 'main-09/11/2026', kind: 'dev', commit: 'abc1234def5678900000000000000000000000ff', date: null };
  const BETA = { id: 'v0.1.0-beta.1', version: 'v0.1.0-beta.1', kind: 'beta', commit: null, date: '2026-11-08' };
  const STABLE = { id: 'v1.0.0', version: 'v1.0.0', kind: 'stable', commit: null, date: '2027-01-15' };

  versionsData = { default: BETA.id, versions: [BETA, DEV] };
  await scenario('menu delle versioni: elenco, proposta e spiegazione', async (p) => {
    await until(p, () => !document.getElementById('fwverbox').hidden);
    const opts = await p.$$eval('#fwver option', (o) => o.map((x) => ({ v: x.value, t: x.textContent })));
    check('due voci: beta e sviluppo', opts.map((o) => o.v).join() === 'v0.1.0-beta.1,dev', JSON.stringify(opts));
    check('la beta e\' descritta come tale', opts[0].t.includes('BETA') && opts[0].t.includes('v0.1.0-beta.1'), opts[0].t);
    check('lo sviluppo e\' descritto come tale', opts[1].t.includes('Sviluppo') && opts[1].t.includes('main-09/11/2026'), opts[1].t);
    check('proposta: la versione rilasciata, non lo sviluppo', (await p.inputValue('#fwver')) === 'v0.1.0-beta.1');
    check('gruppi: rilasciate e sviluppo', (await p.$$eval('#fwver optgroup', (g) => g.map((x) => x.label).join('|'))) === 'Versioni rilasciate|Sviluppo');
    check('manifest della versione proposta', (await p.getAttribute('#installer', 'manifest')) === 'firmware/v0.1.0-beta.1/t3/manifest.json', await p.getAttribute('#installer', 'manifest'));
    const ver = await p.textContent('#ver');
    check('riga della versione con la data in formato europeo', ver.includes('v0.1.0-beta.1') && ver.includes('08/11/2026') && ver.includes('BETA'), ver);
    check('spiegazione della fase', (await p.textContent('#fwverhint')).includes('Beta'), await p.textContent('#fwverhint'));
    check('collegamento alle note di rilascio', !(await p.isHidden('#relnotes')) && (await p.getAttribute('#relnoteslink', 'href')) === 'https://github.com/IU5HJU/VoLoRa/releases/tag/v0.1.0-beta.1');
    check('dice di lasciare quella proposta', (await p.textContent('#verhelp')).includes('lascia quella proposta'));
  });
  await scenario('menu delle versioni: cambiare versione o scheda aggiorna il manifest', async (p) => {
    await until(p, () => !document.getElementById('fwverbox').hidden);
    await p.selectOption('#fwver', 'dev');
    check('manifest dello sviluppo', (await p.getAttribute('#installer', 'manifest')) === 'firmware/dev/t3/manifest.json');
    const ver = await p.textContent('#ver');
    check('riga dello sviluppo con il commit', ver.includes('main-09/11/2026 (abc1234)') && ver.includes('versione di sviluppo'), ver);
    check('nessuna nota di rilascio per lo sviluppo', await p.isHidden('#relnotes'));
    check('collegamento al sorgente del commit', (await p.getAttribute('#src', 'href')).endsWith('/tree/abc1234def5678900000000000000000000000ff'));
    await p.selectOption('#board', 'tbeam');
    check('cambiando scheda resta la versione', (await p.getAttribute('#installer', 'manifest')) === 'firmware/dev/tbeam/manifest.json');
    await p.selectOption('#fwver', 'v0.1.0-beta.1');
    check('di nuovo la beta con la scheda scelta', (await p.getAttribute('#installer', 'manifest')) === 'firmware/v0.1.0-beta.1/tbeam/manifest.json');
  });
  await scenario('menu delle versioni: il confronto con la scheda segue la versione scelta', async (p) => {
    await until(p, () => !document.getElementById('fwverbox').hidden);
    await p.evaluate(() => { window.__st.fw = 'v0.1.0-beta.1'; });
    await connect(p);
    await until(p, () => document.getElementById('fwinfo').textContent.includes('Firmware installato'));
    check('uguale alla versione scelta', (await p.textContent('#fwinfo')).includes('È la versione scelta nel passo 1'), await p.textContent('#fwinfo'));
    await p.selectOption('#fwver', 'dev');
    check('scelta un\'altra versione: diversa', (await p.textContent('#fwinfo')).includes('È diversa da quella scelta nel passo 1'), await p.textContent('#fwinfo'));
    await p.selectOption('#fwver', 'v0.1.0-beta.1');
    check('di nuovo uguale', (await p.textContent('#fwinfo')).includes('È la versione scelta nel passo 1'));
  });
  versionsData = { default: STABLE.id, versions: [DEV, BETA, STABLE] };   // ordine sparso: lo decide il programma che compila versions.json
  await scenario('menu delle versioni: con una stabile si propone quella', async (p) => {
    await until(p, () => !document.getElementById('fwverbox').hidden);
    check('proposta: la stabile', (await p.inputValue('#fwver')) === 'v1.0.0');
    const labels = await p.$$eval('#fwver option', (o) => o.map((x) => x.textContent));
    check('"(consigliata)" solo sulla stabile', labels.filter((l) => l.includes('(consigliata)')).length === 1 && labels.find((l) => l.includes('v1.0.0')).includes('(consigliata)'), JSON.stringify(labels));
    check('la stabile e\' descritta come tale', labels.find((l) => l.includes('v1.0.0')).includes('stabile'));
    check('spiegazione della stabile', (await p.textContent('#fwverhint')).includes('consigliata'));
  });
  versionsData = null;
  await scenario('menu delle versioni: senza versions.json resta il comportamento di prima', async (p) => {
    await sleep(300);
    check('menu nascosto', await p.isHidden('#fwverbox'));
    check('manifest storico', (await p.getAttribute('#installer', 'manifest')) === 'firmware/t3/manifest.json');
    await p.selectOption('#board', 'tbeam');
    check('manifest storico con la scheda', (await p.getAttribute('#installer', 'manifest')) === 'firmware/tbeam/manifest.json');
    check('la versione pubblicata si legge da version.json', (await p.textContent('#ver')).includes('test-1'));
  });

  await scenario('profilo di trasmissione: si legge, si cambia e si salva', async (p) => {
    check('il menu del profilo e\' spento prima di collegare', await p.isDisabled('#profile'));
    await connect(p);
    check('menu attivo con un firmware che ha i profili', !(await p.isDisabled('#profile')));
    check('profilo letto: Standard', (await p.inputValue('#profile')) === 'std');
    const hint = await p.textContent('#profilehint');
    check('spiega che profili diversi non si sentono', hint.includes('profili diversi non si sentono') && hint.includes('a ricezione conclusa'), hint);
    await p.fill('#callsign', 'IU5HJU');
    await p.selectOption('#profile', 'msg');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    check('CFG SET PROFILE msg inviato', (await cmds(p)).includes('CFG SET PROFILE msg'), (await cmds(p)).join(' | '));
    check('profilo salvato nella scheda', (await p.evaluate(() => window.__st.cfg.profile)) === 'msg');
  });

  await scenario('posizione: si legge, si imposta e si salva (T3, senza GPS)', async (p) => {
    check('campi spenti prima di collegare', (await p.isDisabled('#pos')) && (await p.isDisabled('#qthlat')) && (await p.isDisabled('#gps')));
    await connect(p);
    check('trasmissione della posizione: spenta di default', !(await p.isDisabled('#pos')) && (await p.inputValue('#pos')) === 'off');
    check('posizione fissa letta: vuota', (await p.inputValue('#qthlat')) === '' && (await p.inputValue('#qthlon')) === '' && !(await p.isDisabled('#qthlat')));
    check('GPS spento sulla T3 (la scheda non lo ha)', await p.isDisabled('#gps'));
    await p.fill('#callsign', 'IU5HJU');
    await p.selectOption('#pos', 'on');
    await p.fill('#qthlat', '43,7696');        // la virgola vale come punto
    await p.fill('#qthlon', '11.2558');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    const c = await cmds(p);
    check('CFG SET POS on inviato', c.includes('CFG SET POS on'), c.join(' | '));
    check('CFG SET QTH inviato con il punto decimale', c.includes('CFG SET QTH 43.7696,11.2558'), c.join(' | '));
    check('nessun CFG SET GPS sulla T3', !c.some((x) => x.startsWith('CFG SET GPS')));
    const st = await p.evaluate(() => window.__st.cfg);
    check('valori salvati nella scheda', st.pos === 'on' && st.qth === '43.76960,11.25580', JSON.stringify(st));
    check('dopo il riavvio i campi mostrano i valori riletti', (await p.inputValue('#qthlat')) === '43.76960' && (await p.inputValue('#pos')) === 'on', await p.inputValue('#qthlat'));
  });

  await scenario('posizione fissa: cancellarla e controlli sui valori', async (p) => {
    await p.evaluate(() => { window.__st.cfg.qth = '43.76960,11.25580'; window.__st.cfg.pos = 'on'; });
    await connect(p);
    check('valori letti', (await p.inputValue('#qthlat')) === '43.76960' && (await p.inputValue('#qthlon')) === '11.25580' && (await p.inputValue('#pos')) === 'on');
    await p.fill('#callsign', 'IU5HJU');
    // un solo campo: errore, nessun SET
    await p.fill('#qthlon', '');
    await p.click('#save');
    await until(p, () => document.getElementById('status').className === 'err');
    check('serve anche la longitudine', (await status(p)).includes('longitudine'), await status(p));
    for (const [la, lo, word] of [['91', '10', 'latitudine'], ['40', '181', 'longitudine'], ['abc', '10', 'non valida'], ['43.123456789', '10', 'non valida']]) {
      await p.fill('#qthlat', la); await p.fill('#qthlon', lo);
      await p.click('#save');
      await until(p, () => document.getElementById('status').className === 'err');
      check('errore per ' + la + ',' + lo, (await status(p)).toLowerCase().includes(word), await status(p));
    }
    check('nessun SET inviato con valori non validi', !(await cmds(p)).some((x) => x.startsWith('CFG SET')));
    // Cancella: entrambi vuoti -> QTH NONE
    await p.click('#qthclear');
    check('Cancella svuota i campi', (await p.inputValue('#qthlat')) === '' && (await p.inputValue('#qthlon')) === '');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    check('CFG SET QTH none inviato (il firmware accetta maiuscole e minuscole)', (await cmds(p)).includes('CFG SET QTH none'), (await cmds(p)).join(' | '));
    check('posizione fissa cancellata nella scheda', (await p.evaluate(() => window.__st.cfg.qth)) === 'none');
  });

  await scenario('GPS: attivo sulla T-Beam, spento sulle altre schede', async (p) => {
    await p.evaluate(() => { window.__st.board = 'tbeam'; });
    await connect(p);
    check('GPS attivo sulla T-Beam e spento di default', !(await p.isDisabled('#gps')) && (await p.inputValue('#gps')) === 'off');
    await p.fill('#callsign', 'IU5HJU');
    await p.selectOption('#gps', 'on');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    check('CFG SET GPS on inviato', (await cmds(p)).includes('CFG SET GPS on'), (await cmds(p)).join(' | '));
    check('GPS acceso nella scheda', (await p.evaluate(() => window.__st.cfg.gps)) === 'on');
    check('spiegazione del GPS visibile', (await p.textContent('#gpshint')).includes('fix recente'));
  });

  await scenario('firmware senza posizione ne\' GPS: campi spenti e nessun comando nuovo', async (p) => {
    await p.evaluate(() => { window.__st.noPos = true; window.__st.noGps = true; });
    await connect(p);
    check('campi della posizione spenti', (await p.isDisabled('#pos')) && (await p.isDisabled('#qthlat')) && (await p.isDisabled('#qthclear')) && (await p.isDisabled('#gps')));
    await p.fill('#callsign', 'IU5HJU');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    const c = await cmds(p);
    check('nessun comando POS, QTH o GPS', !c.some((x) => /^CFG SET (POS|QTH|GPS)/.test(x)), c.join(' | '));
    check('il resto si salva normalmente', c.includes('CFG SET CALLSIGN IU5HJU'));
  });

  await scenario('MeshCom: lettura, destinatari (gruppi), SSID, ascolto e frequenza', async (p) => {
    check('campi MeshCom spenti prima di collegare', (await p.isDisabled('#mcdest1')) && (await p.isDisabled('#mcssid')) && (await p.isDisabled('#mcfreq')) && (await p.isDisabled('#mcwindow')));
    await connect(p);
    await until(p, () => document.getElementById('mcdest1').value !== '');
    check('valori letti dalla scheda', (await p.inputValue('#mcfreq')) === '433.175' && (await p.inputValue('#mcssid')) === '99' && (await p.inputValue('#mcwindow')) === '300');
    check('destinatari letti (gruppi e tutti)', (await p.inputValue('#mcdest1')) === '22251' && (await p.inputValue('#mcdest2')) === '222' && (await p.inputValue('#mcdest3')) === '*' && (await p.inputValue('#mcdest4')) === '');
    check('campi attivi', !(await p.isDisabled('#mcdest1')) && !(await p.isDisabled('#mcdest5')) && !(await p.isDisabled('#mcwindow')));
    check('niente profilo MeshCom fisso nell\'elenco', (await p.locator('#profile option[value=mc]').count()) === 0);
    await p.fill('#callsign', 'IU5HJU');
    await p.fill('#mcdest1', '22253');
    await p.fill('#mcdest4', 'iu5abc-9');
    await p.fill('#mcssid', '12');
    await p.fill('#mcwindow', '120');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    const c = await cmds(p);
    check('CFG SET FREQMC, MCSSID e MCWINDOW', c.includes('CFG SET FREQMC 433.175') && c.includes('CFG SET MCSSID 12') && c.includes('CFG SET MCWINDOW 120'), c.join(' | '));
    check('CFG SET MCDESTn (maiuscolo, vuoto = NONE)', c.includes('CFG SET MCDEST1 22253') && c.includes('CFG SET MCDEST3 *') && c.includes('CFG SET MCDEST4 IU5ABC-9') && c.includes('CFG SET MCDEST5 NONE'), c.join(' | '));
    const m = await p.evaluate(() => window.__st.mc);
    check('valori salvati nella scheda', m.dests[0] === '22253' && m.dests[3] === 'IU5ABC-9' && m.ssid === '12' && m.window === '120', JSON.stringify(m));
  });

  await scenario('MeshCom: destinatario o ascolto non validi, nessun comando inviato', async (p) => {
    await connect(p);
    await until(p, () => document.getElementById('mcdest1').value !== '');
    await p.fill('#callsign', 'IU5HJU');
    await p.fill('#mcdest2', 'IU5 ABC');
    await p.click('#save');
    await until(p, () => document.getElementById('status').className === 'err');
    check('messaggio sul destinatario 2', (await status(p)).includes('Destinatario 2'), await status(p));
    check('nessun CFG SET inviato', !(await cmds(p)).some((x) => x.startsWith('CFG SET')));
    await p.fill('#mcdest2', '222');
    await p.fill('#mcwindow', '5');
    await p.click('#save');
    await until(p, () => document.getElementById('status').textContent.includes('ascolto'));
    check('ascolto fuori campo rifiutato', !(await cmds(p)).some((x) => x.startsWith('CFG SET')));
  });

  await scenario('firmware senza MeshCom: campi spenti e nessun comando nuovo', async (p) => {
    await p.evaluate(() => { window.__st.noMc = true; });
    await connect(p);
    await sleep(500);
    check('campi MeshCom spenti', (await p.isDisabled('#mcdest1')) && (await p.isDisabled('#mcfreq')));
    await p.fill('#callsign', 'IU5HJU');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    check('nessun comando MeshCom', !(await cmds(p)).some((x) => /FREQMC|MCDEST|MCSSID|MCWINDOW/.test(x)));
  });

  await scenario('testi predefiniti: lettura, modifica, svuotamento e rifiuto', async (p) => {
    check('campi dei testi spenti prima di collegare', await p.isDisabled('#text1'));
    await connect(p);
    await until(p, () => document.getElementById('text5').value !== '');
    check('testi letti dalla scheda', (await p.inputValue('#text1')) === '%C QRV su VoLoRa modo %M %F' && (await p.inputValue('#text5')) === '73 e buona giornata');
    check('campi attivi', !(await p.isDisabled('#text1')) && !(await p.isDisabled('#text5')));
    await p.fill('#callsign', 'IU5HJU');
    await p.fill('#text2', 'Sono QRV su 433.775, chiamami');
    await p.fill('#text3', '');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    const c = await cmds(p);
    check('CFG SET TEXT2 con il testo intero (anche con spazi)', c.includes('CFG SET TEXT2 Sono QRV su 433.775, chiamami'), c.join(' | '));
    check('posto vuoto: CFG SET TEXT3 NONE', c.includes('CFG SET TEXT3 NONE'));
    const t = await p.evaluate(() => window.__st.texts);
    check('testi salvati nella scheda', t[1] === 'Sono QRV su 433.775, chiamami' && t[2] === '' && t[0] === '%C QRV su VoLoRa modo %M %F', JSON.stringify(t));
  });

  await scenario('testi predefiniti: caratteri non ammessi o troppo lunghi, nessun comando inviato', async (p) => {
    await connect(p);
    await until(p, () => document.getElementById('text1').value !== '');
    await p.fill('#callsign', 'IU5HJU');
    await p.fill('#text1', 'Perché no');
    await p.click('#save');
    await until(p, () => document.getElementById('status').className === 'err');
    check('messaggio sui caratteri non ammessi', (await status(p)).includes('Il testo 1'), await status(p));
    check('nessun CFG SET inviato', !(await cmds(p)).some((x) => x.startsWith('CFG SET')));
    check('il limite di 150 caratteri e\' sul campo', (await p.getAttribute('#text1', 'maxlength')) === '150');
  });

  await scenario('modo Sysop: lettura, chiave generata, salvataggio e elenco comandi', async (p) => {
    check('campi sysop spenti prima di collegare', (await p.isDisabled('#sysopmode')) && (await p.isDisabled('#sysopkey')) && (await p.isDisabled('#genkey')));
    await connect(p);
    await until(p, () => !document.getElementById('sysopmode').disabled);
    check('sysop: spento, nominativo vuoto, chiave non impostata', (await p.inputValue('#sysopmode')) === 'off' && (await p.inputValue('#sysop')) === '' && /non impostata/.test(await p.textContent('#sysopkeyhint')));
    check('testi visibili e elenco nascosto', (await p.isVisible('#textsbox')) && (await p.isHidden('#sysopbox')));
    await p.click('#genkey');
    const gk = await p.inputValue('#sysopkey');
    check('chiave generata: 32 cifre esadecimali', /^[0-9a-f]{32}$/.test(gk));
    await p.selectOption('#sysopmode', 'on');
    check('acceso: elenco comandi visibile, testi nascosti', (await p.isVisible('#sysopbox')) && (await p.isHidden('#textsbox')) && (await p.locator('#sysoplist li').count()) === 10);
    await p.fill('#callsign', 'IU5HJU');
    await p.fill('#sysop', 'iu5hju');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    const c = await cmds(p);
    check('CFG SET SYSOP (maiuscolo), SYSOPMODE e SYSOPKEY', c.includes('CFG SET SYSOP IU5HJU') && c.includes('CFG SET SYSOPMODE on') && c.includes('CFG SET SYSOPKEY ' + gk), c.join(' | '));
    check('la scheda ha tutto', await p.evaluate(() => window.__st.sysop.call === 'IU5HJU' && window.__st.sysop.mode === 'on' && window.__st.sysop.key === 'set'));
  });

  await scenario('modo Sysop: chiave non valida bloccata, chiave vuota non cambia', async (p) => {
    await connect(p);
    await until(p, () => !document.getElementById('sysopmode').disabled);
    await p.fill('#callsign', 'IU5HJU');
    await p.fill('#sysopkey', 'abc');
    await p.click('#save');
    await sleep(500);
    check('chiave corta rifiutata dalla pagina', /32 cifre/.test(await p.textContent('#status')) && !(await cmds(p)).some((x) => x.startsWith('CFG SET SYSOP')));
    await p.fill('#sysopkey', '');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    check('senza chiave nuova non si manda SYSOPKEY', !(await cmds(p)).some((x) => x.startsWith('CFG SET SYSOPKEY')));
  });

  await scenario('ruolo del ripetitore: stazione, TX con frequenza di uscita, RX con monitor', async (p) => {
    check('campi spenti prima di collegare', (await p.isDisabled('#role')) && (await p.isDisabled('#freqout')) && (await p.isDisabled('#monitor')));
    await connect(p);
    await until(p, () => !document.getElementById('role').disabled);
    check('di base stazione normale, riquadri nascosti', (await p.inputValue('#role')) === 'off' && (await p.isHidden('#rxbox')) && (await p.isHidden('#txbox')));
    await p.fill('#callsign', 'IR5XXX');
    await p.selectOption('#role', 'rpttx');
    check('TX: compaiono frequenza di uscita e identificativo', (await p.isVisible('#txbox')) && (await p.isHidden('#rxbox')));
    await p.fill('#freqout', '999');
    await p.click('#save');
    await sleep(400);
    check('frequenza di uscita fuori limite bloccata', /430\.000 e 440\.000/.test(await p.textContent('#status')) && !(await cmds(p)).some((x) => x.startsWith('CFG SET ROLE')));
    await p.fill('#freqout', '434.400');
    await p.fill('#idint', '5');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    let c = await cmds(p);
    check('ROLE RPTTX, FREQOUT e IDINT', c.includes('CFG SET ROLE rpttx') && c.includes('CFG SET FREQOUT 434.400') && c.includes('CFG SET IDINT 5') && !c.some((x) => x.startsWith('CFG SET MONITOR')), c.join(' | '));
    check('la scheda ha il ruolo', await p.evaluate(() => window.__st.cfg.role === 'rpttx' && window.__st.cfg.freqout === '434.400'));
  });

  await scenario('ruolo del ripetitore: sulla T-Beam non si cambia (solo T3)', async (p) => {
    await p.evaluate(() => { window.__st.board = 'tbeam'; });
    await connect(p);
    await until(p, () => document.getElementById('rolenote') && !document.getElementById('rolenote').hidden);
    check('ruolo spento e avviso visibile', (await p.isDisabled('#role')) && (await p.isVisible('#rolenote')) && (await p.isDisabled('#freqout')));
    await p.fill('#callsign', 'IU5HJU');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    check('nessun comando ROLE sulla T-Beam', !(await cmds(p)).some((x) => /^CFG SET (ROLE|FREQOUT|MONITOR|IDINT)/.test(x)));
  });

  await scenario('ruolo NODE: avviso e salvataggio', async (p) => {
    await connect(p);
    await until(p, () => !document.getElementById('role').disabled);
    await p.fill('#callsign', 'IU5HJU');
    await p.selectOption('#role', 'node');
    check('avviso del nodo visibile, riquadri RX/TX nascosti', (await p.isVisible('#nodebox')) && (await p.isHidden('#rxbox')) && (await p.isHidden('#txbox')));
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    const c = await cmds(p);
    check('ROLE node, nessun FREQOUT/IDINT/MONITOR', c.includes('CFG SET ROLE node') && !c.some((x) => /^CFG SET (FREQOUT|IDINT|IDMORSE|MONITOR)/.test(x)), c.join(' | '));
  });

  await scenario('ruolo del ripetitore: scheda RX con il monitor', async (p) => {
    await connect(p);
    await until(p, () => !document.getElementById('role').disabled);
    await p.fill('#callsign', 'IR5XXX');
    await p.selectOption('#role', 'rptrx');
    check('RX: compare il monitor, non la frequenza di uscita', (await p.isVisible('#rxbox')) && (await p.isHidden('#txbox')));
    await p.selectOption('#monitor', 'on');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    const c = await cmds(p);
    check('ROLE RPTRX e MONITOR, nessun FREQOUT/IDINT', c.includes('CFG SET ROLE rptrx') && c.includes('CFG SET MONITOR on') && !c.some((x) => /^CFG SET (FREQOUT|IDINT)/.test(x)), c.join(' | '));
  });

  await scenario('firmware senza ruolo: campi spenti e nessun comando', async (p) => {
    await p.evaluate(() => { window.__st.noRole = true; });
    await connect(p);
    await sleep(600);
    check('campi del ruolo spenti', (await p.isDisabled('#role')) && (await p.isDisabled('#freqout')));
    await p.fill('#callsign', 'IU5HJU');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    check('nessun comando ROLE/FREQOUT/MONITOR', !(await cmds(p)).some((x) => /^CFG SET (ROLE|FREQOUT|MONITOR)/.test(x)));
  });

  await scenario('identificativo del ripetitore: lettura, salvataggio e limiti', async (p) => {
    check('campo spento prima di collegare', await p.isDisabled('#idint'));
    await p.evaluate(() => { window.__st.cfg.role = 'rpttx'; });
    await connect(p);
    await until(p, () => !document.getElementById('idint').disabled);
    check('letto dalla scheda (di base 10)', (await p.inputValue('#idint')) === '10');
    await p.fill('#callsign', 'IU5HJU');
    await p.fill('#idint', '11');
    await p.click('#save');
    await sleep(400);
    check('11 minuti rifiutati dalla pagina', /1 a 10/.test(await p.textContent('#status')) && !(await cmds(p)).some((x) => x.startsWith('CFG SET IDINT')));
    await p.fill('#idint', '4');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    check('CFG SET IDINT 4', (await cmds(p)).includes('CFG SET IDINT 4') && (await p.evaluate(() => window.__st.idint)) === '4');
    check('identificativo in Morse: acceso di base e comando mandato', (await p.inputValue('#idmorse')) === 'on' && (await cmds(p)).includes('CFG SET IDMORSE on'));
  });

  await scenario('firmware senza identificativo: campo spento e nessun comando', async (p) => {
    await p.evaluate(() => { window.__st.noIdint = true; });
    await connect(p);
    await sleep(600);
    check('campo spento', await p.isDisabled('#idint'));
    await p.fill('#callsign', 'IU5HJU');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    check('nessun comando IDINT', !(await cmds(p)).some((x) => /IDINT/.test(x) && x.startsWith('CFG SET')));
  });

  await scenario('firmware senza modo Sysop: campi spenti e nessun comando nuovo', async (p) => {
    await p.evaluate(() => { window.__st.noSysop = true; });
    await connect(p);
    await sleep(600);
    check('campi sysop spenti', (await p.isDisabled('#sysopmode')) && (await p.isDisabled('#sysop')) && (await p.isDisabled('#genkey')));
    await p.fill('#callsign', 'IU5HJU');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    check('nessun comando SYSOP', !(await cmds(p)).some((x) => /SYSOP/.test(x) && x.startsWith('CFG SET')));
  });

  await scenario('firmware senza testi predefiniti: campi spenti e nessun comando nuovo', async (p) => {
    await p.evaluate(() => { window.__st.noTexts = true; });
    await connect(p);
    await sleep(500);
    check('campi dei testi spenti', (await p.isDisabled('#text1')) && (await p.isDisabled('#text5')));
    await p.fill('#callsign', 'IU5HJU');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'), null, 15000);
    check('nessun comando TEXT', !(await cmds(p)).some((x) => /TEXT/.test(x) && x.startsWith('CFG SET')));
  });

  await scenario('posizione: la scheda non salva e la pagina lo dice', async (p) => {
    await connect(p);
    await p.fill('#callsign', 'IU5HJU');
    await p.selectOption('#pos', 'on');
    await p.evaluate(() => { window.__st.reject = 'POS'; });
    await p.click('#save');
    await until(p, () => document.getElementById('status').className === 'err');
    check('errore del firmware mostrato', (await status(p)).includes('CFG ERR pos'), await status(p));
    check('nessun riavvio dopo l\'errore', !(await cmds(p)).includes('CFG REBOOT'));
  });

  await scenario('frequenze dei profili: possono essere diverse o uguali, mai bloccanti', async (p) => {
    check('campo spento prima di collegare', await p.isDisabled('#freqmsg'));
    await connect(p);
    check('campo attivo e letto', !(await p.isDisabled('#freqmsg')) && (await p.inputValue('#freqmsg')) === '433.900');
    check('stesse frequenze: la nota (non bloccante) e\' visibile', !(await p.isHidden('#freqnote')));
    const note = await p.textContent('#freqnote');
    check('la nota dice che non e\' un canale condiviso', note.includes('non è un canale condiviso') && note.includes('canale occupato'), note);
    // diverse: la nota sparisce e si salva
    await p.fill('#callsign', 'IU5HJU'); await p.fill('#freqmsg', '434.200');
    check('frequenze diverse: nessuna nota', await p.isHidden('#freqnote'));
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    check('CFG SET FREQMSG inviato', (await cmds(p)).includes('CFG SET FREQMSG 434.200'), (await cmds(p)).join(' | '));
    check('frequenza del profilo Messaggio salvata, quella dello Standard intatta', (await p.evaluate(() => window.__st.cfg.freqmsg)) === '434.200' && (await p.evaluate(() => window.__st.cfg.freq)) === '433.900');
  });

  await scenario('frequenze dei profili: tutte uguali si salva senza avvisi di errore', async (p) => {
    await connect(p);
    await p.fill('#callsign', 'IU5HJU'); await p.fill('#freq', '434.000'); await p.fill('#freqmsg', '434.000');
    check('nota visibile ma non e\' un errore', !(await p.isHidden('#freqnote')));
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    check('salvate uguali', (await p.evaluate(() => window.__st.cfg.freq)) === '434.000' && (await p.evaluate(() => window.__st.cfg.freqmsg)) === '434.000');
    check('nessun errore', !(await status(p)).includes('Errore') && (await status(p)).includes('Salvato'), await status(p));
  });

  await scenario('frequenze dei profili: il pulsante copia quella dello Standard', async (p) => {
    await connect(p);
    await p.fill('#freq', '435.250');
    await p.fill('#freqmsg', '433.100');
    await p.click('#sameFreq');
    check('copiata', (await p.inputValue('#freqmsg')) === '435.250');
    check('la nota compare', !(await p.isHidden('#freqnote')));
  });

  await scenario('frequenze dei profili: fuori banda rifiutata prima di inviare', async (p) => {
    await connect(p);
    await p.fill('#callsign', 'IU5HJU'); await p.fill('#freqmsg', '445.000');
    await p.click('#save');
    await until(p, () => /Errore|deve essere/.test(document.getElementById('status').textContent));
    check('messaggio sulla frequenza del Messaggio', (await status(p)).includes('profilo Messaggio'), await status(p));
    check('nessun SET inviato', !(await cmds(p)).some((c) => c.startsWith('CFG SET')));
  });

  await scenario('frequenze dei profili: firmware con una sola frequenza', async (p) => {
    await p.evaluate(() => { window.__st.noFreqMsg = true; });
    await connect(p);
    check('campo e pulsante spenti', (await p.isDisabled('#freqmsg')) && (await p.isDisabled('#sameFreq')));
    await p.fill('#callsign', 'IU5HJU');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    check('nessun CFG SET FREQMSG con il firmware vecchio', !(await cmds(p)).some((c) => c.includes('FREQMSG')), (await cmds(p)).join(' | '));
  });

  await scenario('profilo di trasmissione: firmware senza profili', async (p) => {
    await p.evaluate(() => { window.__st.noProfile = true; });
    await connect(p);
    check('menu del profilo spento e nessun comando PROFILE', await p.isDisabled('#profile'));
    await p.fill('#callsign', 'IU5HJU');
    await p.click('#save');
    await until(p, () => window.__st.cmds.includes('CFG REBOOT'));
    check('nessun CFG SET PROFILE con il firmware vecchio', !(await cmds(p)).some((c) => c.includes('PROFILE')), (await cmds(p)).join(' | '));
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

  await scenario('installazione: spiegazione delle versioni', async (p) => {
    const t = await p.textContent('#verhelp');
    check('spiega stabile, beta e sviluppo', t.includes('Quale versione scelgo?') && t.includes('Stabile') && t.includes('Beta') && t.includes('Sviluppo') && t.includes('main-GG/MM/AAAA'), t);
    check('dice di lasciare quella proposta', t.includes('lascia quella proposta'), t);
    check('per chi usa la riga di comando: esptool', t.includes('web_factory.bin') && t.includes('write_flash 0x0'), t);
    check('rimanda alla pagina delle versioni', (await p.getAttribute('#verhelp a', 'href')) === 'https://github.com/IU5HJU/VoLoRa/releases');
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

  // Il clic sul pulsante non deve finire dentro la misura: tra il clic e CFG MIC MEASURE passano 3 passi del conto alla rovescia.
  await scenario('misura del fondo: conto alla rovescia prima della misura', async (p) => {
    await connect(p);
    const t0 = Date.now();
    await p.click('#measure');
    await new Promise((r) => setTimeout(r, 400));  // dopo 400 ms il conto (3 x 300 ms) non e' finito
    check('il comando non e\' ancora partito', !(await cmds(p)).includes('CFG MIC MEASURE'), (await cmds(p)).join(' | '));
    check('la pagina mostra il conto alla rovescia', /parte tra \d/.test(await status(p)), await status(p));
    await until(p, () => window.__st.cmds.includes('CFG MIC MEASURE'), null, 4000);
    check('il comando parte dopo almeno 3 passi', Date.now() - t0 >= 850, String(Date.now() - t0));
    await until(p, () => document.getElementById('status').textContent.includes('salvato'), null, 6000);
  }, { countdownStep: 300 });
  await scenario('prova del microfono: conto alla rovescia prima del silenzio', async (p) => {
    await connect(p);
    await p.click('#hwMic');
    await new Promise((r) => setTimeout(r, 400));
    check('il comando non e\' ancora partito', !(await cmds(p)).includes('CFG MIC MEASURE'), (await cmds(p)).join(' | '));
    check('mostra il conto alla rovescia', /parte tra \d/.test(await p.textContent('#hwMicOut')), await p.textContent('#hwMicOut'));
  }, { countdownStep: 300 });

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
