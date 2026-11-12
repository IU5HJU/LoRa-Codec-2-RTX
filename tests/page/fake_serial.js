// Finto dispositivo seriale per provare la pagina di configurazione senza scheda.
// Si inietta nella pagina prima del suo codice (page.addInitScript) e sostituisce navigator.serial.
// Imita i comandi CFG del firmware (src/main.cpp, lib/voicecore/settings.*). Lo stato e' in window.__st:
// il test lo legge (cmds = comandi ricevuti) e lo modifica per simulare situazioni diverse.
(() => {
  const st = window.__st = {
    cfg: { callsign: 'N0CALL', freq: '433.900', freqmsg: '433.900', freqrange: '433.900', power: 10, volume: 7, micfloor: -60, profile: 'std', pos: 'off', qth: 'none', gps: 'off' },
    active: { callsign: 'N0CALL', freq: '433.900', freqmsg: '433.900', power: 10, volume: 7, profile: 'std' },
    noFreqMsg: false,     // firmware con una sola frequenza (piu' vecchio)
    noPos: false,         // firmware senza posizione (piu' vecchio): CFG GET senza pos=/qth= e CFG SET POS/QTH sconosciuti
    noGps: false,
    noMc: false,          // firmware senza profilo MeshCom (piu' vecchio): CFG GET MC e CFG SET FREQMC/MCDEST/MCSSID sconosciuti
    mc: { freq: '433.175', ssid: '99', window: '300', dests: ['22251', '222', '*', '', ''] },
    noRole: false,        // firmware senza ruolo del ripetitore (piu' vecchio): niente role= nel CFG GET
    monitor: 'off',
    idmorse: 'on',
    noIdmorse: false,
    legacyFw: false,      // firmware precedente al 12/11/2026: nel CFG GET c'e' freqqual= e non freqrange=
    noShift: false,       // firmware senza shift (piu' vecchio): CFG GET SHIFT e CFG SET SHIFT sconosciuti
    shift: 0,             // shift in trasmissione, kHz: 0, +10000 o -10000
    noIdint: false,       // firmware senza identificativo periodico (piu' vecchio)
    idint: '10',
    noSysop: false,       // firmware senza modo Sysop (piu' vecchio): CFG GET SYSOP e CFG SET SYSOP* sconosciuti
    sysop: { call: 'none', key: 'none', mode: 'off', keyHex: '' },
    noTexts: false,       // firmware senza testi predefiniti (piu' vecchio): CFG GET TEXTn e CFG SET TEXTn sconosciuti
    texts: ['%C QRV su VoLoRa modo %M %F', 'Arrivo tra 10 minuti', 'Chiamami appena puoi', 'Tutto OK', '73 e buona giornata'],
    // (riga seguente: firmware senza GPS)
    noProfile: false,     // firmware senza profili (piu' vecchio): CFG GET senza profile= e CFG SET PROFILE sconosciuto
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
    const reboot = (c.callsign !== a.callsign || c.freq !== a.freq || c.power !== a.power || c.volume !== a.volume || c.profile !== a.profile || c.freqmsg !== a.freqmsg || c.freqrange !== a.freqrange) ? 1 : 0;
    return `CFG callsign=${c.callsign} freq=${c.freq}${st.noFreqMsg ? '' : ' freqmsg=' + c.freqmsg + (st.legacyFw ? ' freqqual=' : ' freqrange=') + c.freqrange} power=${c.power} volume=${c.volume} micfloor=${c.micfloor}${st.noProfile ? '' : ' profile=' + c.profile} board=${st.board} reboot=${reboot} test=${st.test ? 1 : 0}${st.noPos ? '' : ' pos=' + c.pos + ' qth=' + c.qth}${st.noGps ? '' : ' gps=' + c.gps}${st.noRole ? '' : ' role=' + (c.role || 'off') + ' freqout=' + (c.freqout || c.freq)}`;
  };
  const inRange = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;
  const handle = (line) => {
    st.cmds.push(line);
    const t = line.trim().split(/\s+/);
    if (t[0] !== 'CFG') return;
    const cmd = t[1];
    // CFG SET TEXTn <testo> (il testo e' il resto della riga) e CFG GET TEXTn, come il firmware
    if (!st.noRole) {
      if (/^CFG\s+GET\s+MONITOR$/i.test(line.trim())) return out('CFG MONITOR=' + st.monitor);
      const rm = /^CFG\s+SET\s+(ROLE|MONITOR|FREQOUT)\s+(\S+)$/i.exec(line.trim());
      if (rm) {
        const k = rm[1].toUpperCase(), v = rm[2];
        if (st.reject === k) return out('CFG ERR ' + k.toLowerCase() + ' rifiutato dal firmware');
        if (k === 'ROLE') { if (!/^(off|rptrx|rpttx|node)$/i.test(v)) return out('CFG ERR role usa OFF, RPTRX, RPTTX o NODE'); st.cfg.role = v.toLowerCase(); return out('CFG OK role=' + st.cfg.role); }
        if (k === 'MONITOR') { st.monitor = v.toLowerCase(); return out('CFG OK monitor=' + st.monitor); }
        const khz = Math.round(parseFloat(v) * 1000);
        if (!(khz >= 430000 && khz <= 440000)) return out('CFG ERR freqout fuori dai limiti');
        st.cfg.freqout = v; return out('CFG OK freqout=' + v);
      }
    }
    if (!st.noIdmorse) {
      if (/^CFG\s+GET\s+IDMORSE$/i.test(line.trim())) return out('CFG IDMORSE=' + st.idmorse);
      const dm = /^CFG\s+SET\s+IDMORSE\s+(\S+)$/i.exec(line.trim());
      if (dm) { if (!/^(on|off)$/i.test(dm[1])) return out('CFG ERR idmorse usa ON oppure OFF'); st.idmorse = dm[1].toLowerCase(); return out('CFG OK idmorse=' + st.idmorse); }
    }
    if (!st.noShift) {
      if (/^CFG\s+GET\s+SHIFT$/i.test(line.trim())) return out('CFG SHIFT=' + (st.shift > 0 ? '+' : '') + st.shift);
      const sm = /^CFG\s+SET\s+SHIFT\s+([+-]?\d+)$/i.exec(line.trim());
      if (sm) {
        const v = parseInt(sm[1], 10);
        if (v !== 0 && v !== 10000 && v !== -10000) return out('CFG ERR shift in kHz: 0, +10000 oppure -10000');
        const f = Math.round(parseFloat(st.cfg.freq) * 1000);
        if (v !== 0 && (f + v < 430000 || f + v > 440000)) return out('CFG ERR shift la frequenza di trasmissione uscirebbe da 430-440 MHz');
        st.shift = v; return out('CFG OK shift=' + (v > 0 ? '+' : '') + v);
      }
    }
    if (!st.noIdint) {
      if (/^CFG\s+GET\s+IDINT$/i.test(line.trim())) return out('CFG IDINT=' + st.idint);
      const im = /^CFG\s+SET\s+IDINT\s+(\S+)$/i.exec(line.trim());
      if (im) { if (!inRange(+im[1], 1, 10)) return out('CFG ERR idint in minuti, da 1 a 10'); st.idint = String(+im[1]); return out('CFG OK idint=' + st.idint); }
    }
    if (!st.noSysop) {
      if (/^CFG\s+GET\s+SYSOP$/i.test(line.trim())) return out(`CFG SYSOP call=${st.sysop.call} key=${st.sysop.key} mode=${st.sysop.mode}`);
      const sm = /^CFG\s+SET\s+(SYSOPMODE|SYSOPKEY|SYSOP)\s+(\S+)$/i.exec(line.trim());
      if (sm) {
        const k = sm[1].toUpperCase(), v = sm[2];
        if (st.reject === k) return out('CFG ERR ' + k.toLowerCase() + ' rifiutato dal firmware');
        if (k === 'SYSOP') { st.sysop.call = v.toUpperCase() === 'NONE' ? 'none' : v.toUpperCase(); return out('CFG OK sysop=' + st.sysop.call); }
        if (k === 'SYSOPMODE') { st.sysop.mode = v.toLowerCase(); return out('CFG OK sysopmode=' + st.sysop.mode); }
        if (v.toUpperCase() !== 'NONE' && !/^[0-9a-fA-F]{32}$/.test(v)) return out('CFG ERR sysopkey servono 32 cifre esadecimali');
        st.sysop.key = v.toUpperCase() === 'NONE' ? 'none' : 'set'; st.sysop.keyHex = v; return out('CFG OK sysopkey=' + st.sysop.key);
      }
    }
    const tm = /^CFG\s+SET\s+TEXT([1-5])\s+(.*?)\s*$/i.exec(line.trim());
    if (tm && !st.noTexts) {
      const n = +tm[1];
      if (tm[2].toUpperCase() === 'NONE') { st.texts[n - 1] = ''; return out('CFG OK text' + n + '='); }
      if (!/^[\x20-\x7A\x7C\x7E]{1,150}$/.test(tm[2])) return out('CFG ERR text' + n + ' testo ASCII da 1 a 150 caratteri (NONE per svuotare)');
      st.texts[n - 1] = tm[2]; return out('CFG OK text' + n + '=' + tm[2]);
    }
    if (/^CFG\s+GET\s+MC$/i.test(line.trim()) && !st.noMc) return out(`CFG MC freqmc=${st.mc.freq} ssid=${st.mc.ssid} window=${st.mc.window}`);
    const gd = /^CFG\s+GET\s+MCDEST([1-5])$/i.exec(line.trim());
    if (gd && !st.noMc) return out('CFG MCDEST' + gd[1] + '=' + st.mc.dests[+gd[1] - 1]);
    const tg = /^CFG\s+GET\s+TEXT([1-5])$/i.exec(line.trim());
    if (tg && !st.noTexts) return out('CFG TEXT' + tg[1] + '=' + st.texts[+tg[1] - 1]);
    if (cmd === 'GET') out(get());
    else if (cmd === 'VERSION' && st.noVersion) out('CFG ERR comando sconosciuto');   // firmware vecchio
    else if (cmd === 'VERSION') out(`CFG VERSION fw=${st.fw} board=${st.board} proto=2`);
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
      } else if (key === 'FREQMC' && !st.noMc) {
        const khz = Math.round(parseFloat(v) * 1000);
        if (!(khz >= 430000 && khz <= 440000)) return out('CFG ERR freqmc fuori dai limiti');
        st.mc.freq = v; out('CFG OK freqmc=' + v);
      } else if (/^MCDEST[1-5]$/.test(key) && !st.noMc) {
        const n = +key[6];
        const d = v.toUpperCase();
        if (d === 'NONE') { st.mc.dests[n - 1] = ''; return out('CFG OK mcdest' + n + '='); }
        if (!/^(\*|[A-Z0-9\/-]{1,15})$/.test(d)) return out('CFG ERR mcdest non valido');
        st.mc.dests[n - 1] = d; out('CFG OK mcdest' + n + '=' + d);
      } else if (key === 'MCWINDOW' && !st.noMc) {
        if (!inRange(+v, 30, 1800)) return out('CFG ERR mcwindow da 30 a 1800');
        st.mc.window = String(+v); out('CFG OK mcwindow=' + v);
      } else if (key === 'MCSSID' && !st.noMc) {
        if (!inRange(+v, 0, 99)) return out('CFG ERR mcssid da 0 a 99');
        st.mc.ssid = String(+v); out('CFG OK mcssid=' + v);
      } else if (key === 'FREQRANGE' && !st.noFreqMsg) {
        const khz = Math.round(parseFloat(v) * 1000);
        if (!(khz >= 430000 && khz <= 440000)) return out('CFG ERR freqrange fuori dai limiti');
        st.cfg.freqrange = v; out('CFG OK freqrange=' + v);
      } else if (key === 'FREQMSG' && !st.noFreqMsg) {
        const khz = Math.round(parseFloat(v) * 1000);
        if (!(khz >= 430000 && khz <= 440000)) return out('CFG ERR freqmsg fuori dai limiti');
        st.cfg.freqmsg = v; out('CFG OK freqmsg=' + v);
      } else if (key === 'POWER') {
        if (!inRange(+v, 2, 17)) return out('CFG ERR power da 2 a 17');
        st.cfg.power = +v; out('CFG OK power=' + v);
      } else if (key === 'VOLUME') {
        if (!inRange(+v, 0, 10)) return out('CFG ERR volume da 0 a 10');
        st.cfg.volume = +v; out('CFG OK volume=' + v);
      } else if (key === 'PROFILE' && !st.noProfile) {
        const pv = v.toLowerCase();
        if (pv !== 'std' && pv !== 'msg' && pv !== 'rng') return out('CFG ERR profile usa STD (Standard), RNG (Range) oppure MSG (Messaggio)');
        st.cfg.profile = pv; out('CFG OK profile=' + pv);
      } else if (key === 'POS' && !st.noPos) {
        const pv = v.toLowerCase();
        if (pv !== 'on' && pv !== 'off') return out('CFG ERR pos usa ON oppure OFF');
        st.cfg.pos = pv; out('CFG OK pos=' + pv);
      } else if (key === 'QTH' && !st.noPos) {
        if (v.toUpperCase() === 'NONE') { st.cfg.qth = 'none'; return out('CFG OK qth=none'); }
        const m = /^(-?\d{1,3}(?:\.\d{1,7})?),(-?\d{1,3}(?:\.\d{1,7})?)$/.exec(v);
        if (!m || Math.abs(+m[1]) > 90 || Math.abs(+m[2]) > 180) return out('CFG ERR qth latitudine da -90 a 90, longitudine da -180 a 180, senza spazi');
        st.cfg.qth = (+m[1]).toFixed(5) + ',' + (+m[2]).toFixed(5);   // la scheda stampa 5 decimali
        out('CFG OK qth=' + st.cfg.qth);
      } else if (key === 'GPS' && !st.noGps) {
        if (st.board !== 'tbeam') return out('CFG ERR gps questa scheda non ha il GPS');
        const pv = v.toLowerCase();
        if (pv !== 'on' && pv !== 'off') return out('CFG ERR gps usa ON oppure OFF');
        st.cfg.gps = pv; out('CFG OK gps=' + pv);
      } else if (key === 'MICFLOOR') {
        if (!inRange(+v, -80, -36)) return out('CFG ERR micfloor da -80 a -36');
        st.cfg.micfloor = +v; out('CFG OK micfloor=' + v);
      } else out('CFG ERR ' + key + ' parametro sconosciuto');
    } else if (cmd === 'RESET') {
      st.cfg = { callsign: 'N0CALL', freq: '433.900', power: 10, volume: 7, micfloor: -60 }; out('CFG OK reset');
    } else if (cmd === 'REBOOT') {
      out('CFG OK reboot'); st.active = { callsign: st.cfg.callsign, freq: st.cfg.freq, power: st.cfg.power, volume: st.cfg.volume, profile: st.cfg.profile, freqmsg: st.cfg.freqmsg, freqrange: st.cfg.freqrange };
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
