// Finto dispositivo seriale per provare la pagina di configurazione senza scheda.
// Si inietta nella pagina prima del suo codice (page.addInitScript) e sostituisce navigator.serial.
// Imita i comandi CFG del firmware (src/main.cpp, lib/voicecore/settings.*). Lo stato e' in window.__st:
// il test lo legge (cmds = comandi ricevuti) e lo modifica per simulare situazioni diverse.
(() => {
  const st = window.__st = {
    cfg: { callsign: 'N0CALL', freq: '433.900', power: 10, volume: 7, micfloor: -60 },
    active: { callsign: 'N0CALL', freq: '433.900', power: 10, volume: 7 },
    test: false,
    fw: 'abc1234',
    board: 't3',
    cmds: [],
    measure: { fondo: -59, min: -62, max: -57, frames: 75 },   // risposta di CFG MIC MEASURE
    measureSeq: null,                                          // se impostata: risposte successive, una per chiamata
    audio: [{ f: 500, base: -63.3, tone: -44.3 }, { f: 1000, base: -68.8, tone: -25.1 }, { f: 2000, base: -76.4, tone: -38.2 }],
    delay: 150,
  };
  const enc = new TextEncoder(), dec = new TextDecoder();
  let ctrl;
  const out = (l) => ctrl.enqueue(enc.encode(l + '\r\n'));
  const later = (f) => setTimeout(f, st.delay);
  const get = () => {
    const c = st.cfg, a = st.active;
    const reboot = (c.callsign !== a.callsign || c.freq !== a.freq || c.power !== a.power || c.volume !== a.volume) ? 1 : 0;
    return `CFG callsign=${c.callsign} freq=${c.freq} power=${c.power} volume=${c.volume} micfloor=${c.micfloor} board=${st.board} reboot=${reboot} test=${st.test ? 1 : 0}`;
  };
  const inRange = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;
  const handle = (line) => {
    st.cmds.push(line);
    const t = line.trim().split(/\s+/);
    if (t[0] !== 'CFG') return;
    const cmd = t[1];
    if (cmd === 'GET') out(get());
    else if (cmd === 'VERSION' && st.noVersion) out('CFG ERR comando sconosciuto');   // firmware vecchio
    else if (cmd === 'VERSION') out(`CFG VERSION fw=${st.fw} board=${st.board} proto=1`);
    else if (cmd === 'SET') {
      const key = t[2], v = t[3];
      if (st.reject === key) return out('CFG ERR ' + key.toLowerCase() + ' rifiutato dal firmware');
      if (key === 'CALLSIGN') {
        const cs = v.toUpperCase();
        if (!/^[A-Z0-9/]{1,8}$/.test(cs)) return out('CFG ERR callsign non valido');
        st.cfg.callsign = cs; out('CFG OK callsign=' + cs);
      } else if (key === 'FREQ') {
        const khz = Math.round(parseFloat(v) * 1000);
        if (!(khz >= 430000 && khz <= 440000)) return out('CFG ERR freq fuori dai limiti');
        st.cfg.freq = v; out('CFG OK freq=' + v);
      } else if (key === 'POWER') {
        if (!inRange(+v, 2, 17)) return out('CFG ERR power da 2 a 17');
        st.cfg.power = +v; out('CFG OK power=' + v);
      } else if (key === 'VOLUME') {
        if (!inRange(+v, 0, 10)) return out('CFG ERR volume da 0 a 10');
        st.cfg.volume = +v; out('CFG OK volume=' + v);
      } else if (key === 'MICFLOOR') {
        if (!inRange(+v, -80, -36)) return out('CFG ERR micfloor da -80 a -36');
        st.cfg.micfloor = +v; out('CFG OK micfloor=' + v);
      } else out('CFG ERR ' + key + ' parametro sconosciuto');
    } else if (cmd === 'REC' && (t[2] === 'ON' || t[2] === 'OFF')) {
      st.rec = t[2] === 'ON'; out('CFG OK rec=' + (st.rec ? 'on' : 'off'));
    } else if (cmd === 'RESET') {
      st.cfg = { callsign: 'N0CALL', freq: '433.900', power: 10, volume: 7, micfloor: -60 }; out('CFG OK reset');
    } else if (cmd === 'REBOOT') {
      out('CFG OK reboot'); st.active = { callsign: st.cfg.callsign, freq: st.cfg.freq, power: st.cfg.power, volume: st.cfg.volume };
      later(() => out(get()));
    } else if (cmd === 'TEST' && (t[2] === 'ON' || t[2] === 'OFF')) {
      st.test = t[2] === 'ON'; out('CFG OK test=' + (st.test ? 'on' : 'off'));
    } else if (cmd === 'TEST' && t[2] === 'AUDIO') {
      out('CFG OK audio prova in corso: ascolta i toni due volte, senza e con la radio (circa 10 secondi)');
      // Fase 1: toni senza radio; fase 2 (se st.rf e' un oggetto): stessi toni con la radio; st.rf stringa = salto.
      const lines = st.audio.map((a) => `CFG AUDIO f=${a.f} base=${a.base.toFixed(1)} tone=${a.tone.toFixed(1)}`);
      const lvl = (a, k) => Math.max(...st.audio.map((x) => x.tone)).toFixed(1);
      lines.push(`CFG AUDIO m1 rms=${lvl()} agc=${(Math.max(...st.audio.map((x) => x.tone)) - 12).toFixed(1)} dc=1.0`);
      if (typeof st.rf === 'string') lines.push(st.rf);
      else if (st.rf) {
        st.rf.audio.forEach((a) => lines.push(`CFG AUDIO rf f=${a.f} base=${a.base.toFixed(1)} tone=${a.tone.toFixed(1)}`));
        lines.push(`CFG AUDIO m2 rms=${st.rf.rms.toFixed(1)} agc=${st.rf.agc.toFixed(1)} dc=${st.rf.dc.toFixed(1)}`);
        lines.push('CFG AUDIO rf pkts=14');
      }
      lines.push('CFG AUDIO fine');
      lines.forEach((l, i) => setTimeout(() => out(l), st.delay * (i + 1)));
    } else if (cmd === 'MIC' && t[2] === 'MEASURE') {
      out('CFG OK mic misura in corso: stare in silenzio per 3 s');
      const m = st.measureSeq && st.measureSeq.length ? st.measureSeq.shift() : st.measure;
      later(() => out(`CFG MIC fondo=${m.fondo} min=${m.min} max=${m.max} frames=${m.frames || 75}`));
    } else out('CFG ERR comando sconosciuto');
  };
  let buf = '';
  const port = {
    open: async () => {},
    close: async () => {},
    readable: new ReadableStream({ start(c) { ctrl = c; setTimeout(() => out('[avvio] scheda LoRa32 T3 V1.6.1, firmware ' + st.fw), 50); } }),
    writable: new WritableStream({
      write(chunk) {
        buf += dec.decode(chunk);
        let i;
        while ((i = buf.indexOf('\n')) >= 0) { handle(buf.slice(0, i)); buf = buf.slice(i + 1); }
      },
    }),
  };
  Object.defineProperty(navigator, 'serial', { value: { requestPort: async () => port }, configurable: true });
})();
