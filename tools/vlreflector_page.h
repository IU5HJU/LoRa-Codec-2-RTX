// La pagina di amministrazione del reflector (servita da tools/vlreflector.cpp con --web): chi e' collegato, chi trasmette adesso, le ultime
// trasmissioni e la registrazione dei nodi. Una pagina sola, senza librerie esterne; i dati arrivano da /api/status con la password di amministrazione.
#pragma once

static const char kAdminPage[] = R"HTML(<!doctype html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Reflector VoLoRa</title>
<style>
  :root { --bg:#f6f7f9; --card:#ffffff; --fg:#1b1f24; --muted:#5b6672; --line:#d5dae0; --accent:#0b62d6; --ok:#1a7f37; --bad:#b42318; --warn:#9a6700; --live:#b42318; }
  @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --bg:#0f1317; --card:#171c22; --fg:#e8ecf0; --muted:#9aa5b1; --line:#2b333c; --accent:#6aa7ff; --ok:#4cc26b; --bad:#ff7b72; --warn:#e3b341; --live:#ff7b72; } }
  * { box-sizing:border-box; }
  body { margin:0; background:var(--bg); color:var(--fg); font:16px/1.45 system-ui, sans-serif; }
  main { max-width:900px; margin:0 auto; padding:16px; }
  .tw { overflow-x:auto; }
  h1 { font-size:1.4rem; margin:.2em 0 .6em; }
  h2 { font-size:1.05rem; margin:0 0 .6em; }
  section { background:var(--card); border:1px solid var(--line); border-radius:10px; padding:14px; margin:0 0 14px; }
  .muted { color:var(--muted); font-size:.9rem; }
  table { width:100%; border-collapse:collapse; font-size:.93rem; }
  th, td { text-align:left; padding:6px 8px; border-bottom:1px solid var(--line); vertical-align:top; }
  th { color:var(--muted); font-weight:600; }
  .dot { display:inline-block; width:.7em; height:.7em; border-radius:50%; background:var(--bad); margin-right:.4em; }
  .dot.up { background:var(--ok); }
  #now { display:flex; align-items:center; gap:12px; min-height:3.2em; }
  #now .big { font-size:1.6rem; font-weight:700; }
  #now.live .big { color:var(--live); }
  input, select { font:inherit; padding:8px; border:1px solid var(--line); border-radius:6px; background:var(--bg); color:var(--fg); width:100%; }
  label { display:block; margin:.7em 0 .2em; font-weight:600; }
  button { font:inherit; padding:8px 14px; border-radius:6px; border:1px solid var(--accent); background:var(--accent); color:#fff; cursor:pointer; margin:.5em .4em 0 0; }
  button.secondary { background:transparent; color:var(--accent); }
  button.danger { background:transparent; color:var(--bad); border-color:var(--bad); padding:3px 9px; margin:0; }
  pre { background:var(--bg); border:1px solid var(--line); border-radius:6px; padding:10px; overflow:auto; white-space:pre-wrap; word-break:break-all; }
  .msg { margin:.6em 0; padding:8px 10px; border-radius:6px; border:1px solid var(--line); }
  .msg.err { color:var(--bad); border-color:var(--bad); }
  .msg.warn { color:var(--warn); border-color:var(--warn); }
  .msg.ok { color:var(--ok); border-color:var(--ok); }
  #login { display:none; }
  /* telefono: meno colonne, il resto resta nel riquadro */
  @media (max-width: 640px) {
    main { padding:10px; }
    section { padding:10px; }
    th, td { padding:6px 4px; }
    #clients td:nth-child(5), #clients td:nth-child(6), thead th:nth-child(5), thead th:nth-child(6) { display:none; }
    #recent td:nth-child(5) { display:none; }
  }
</style>
</head>
<body>
<main>
  <h1>Reflector VoLoRa <span class="muted" id="head"></span></h1>

  <section id="login">
    <h2>Accesso</h2>
    <p class="muted">Scrivi la password di amministrazione (la scegli sul server con <code>vlreflector --set-password</code>). Resta solo in questa scheda del browser.</p>
    <input id="token" type="password" autocomplete="off" placeholder="password">
    <button id="loginBtn" type="button">Entra</button>
    <div id="loginMsg" class="msg err" hidden></div>
  </section>

  <div id="app" hidden>
    <section>
      <h2>In trasmissione adesso</h2>
      <div id="now"><span class="big">Nessuno</span></div>
    </section>

    <section>
      <h2>Collegati</h2>
      <div class="tw"><table>
        <thead><tr><th>Nominativo</th><th>Id</th><th>Stato</th><th>Ultimo segnale</th><th>Pacchetti in/out</th><th title="L'indirizzo da cui il reflector riceve i pacchetti del nodo, dopo l'eventuale NAT del router: dietro un router e' quello del router, non quello interno del nodo">Indirizzo visto dal server</th><th></th></tr></thead>
        <tbody id="clients"></tbody>
      </table></div>
      <p class="muted" id="clientsEmpty" hidden>Nessun nodo in elenco.</p>
    </section>

    <section>
      <h2>Ultime trasmissioni</h2>
      <div class="tw"><table>
        <thead><tr><th>Quando</th><th>Nominativo</th><th>Da</th><th>Durata</th><th>Pacchetti</th></tr></thead>
        <tbody id="recent"></tbody>
      </table></div>
      <p class="muted" id="recentEmpty">Ancora nessuna.</p>
    </section>

    <section>
      <h2>Registra un nodo</h2>
      <p class="muted">Il collega genera la chiave dalla pagina di configurazione della sua scheda e te la manda (su un canale privato) con il suo nominativo e, se ne ha più d'uno, il nome del nodo. Scrivili qui: l'id lo assegna il reflector. Un nominativo può avere <strong>più nodi</strong> (casa, mare, hotspot): ognuno ha il suo id e la sua chiave, e una chiave non si può usare due volte.</p>
      <label for="call">Nominativo</label>
      <input id="call" maxlength="8" autocomplete="off" placeholder="IZ5IOF">
      <label for="label">Nome del nodo (facoltativo: casa, mare, hotspot…)</label>
      <input id="label" maxlength="16" autocomplete="off" placeholder="casa">
      <label for="target">Cosa fare</label>
      <select id="target"><option value="">Aggiungere un nodo nuovo</option></select>
      <label for="key">Chiave (32 cifre esadecimali) — vuota: la genera il reflector</label>
      <input id="key" maxlength="32" autocomplete="off" spellcheck="false" placeholder="vuota = ne genero una nuova">
      <div>
        <button id="checkBtn" type="button" class="secondary">Controlla il nominativo</button>
        <button id="regBtn" type="button">Registra</button>
      </div>
      <div id="regMsg" class="msg" hidden></div>
      <pre id="regOut" hidden></pre>
      <button id="copyBtn" type="button" class="secondary" hidden>Copia i dati da dare al collega</button>
    </section>
  </div>
</main>
<script>
(() => {
  const $ = (id) => document.getElementById(id);
  let token = '';
  try { token = sessionStorage.getItem('vlr-token') || ''; } catch (e) {}
  let info = {};
  const esc = (s) => String(s);
  function show(el, text, cls) { el.hidden = false; el.className = 'msg ' + (cls || ''); el.textContent = text; }
  async function api(path, opts) {
    opts = opts || {};
    opts.headers = Object.assign({ 'X-Token': token }, opts.headers || {});
    const r = await fetch(path, opts);
    if (r.status === 401) { $('app').hidden = true; $('login').style.display = 'block'; throw new Error('401'); }
    return r.json();
  }
  const fmtAgo = (ms) => ms < 0 ? 'mai' : ms < 60000 ? Math.round(ms / 1000) + ' s fa' : ms < 3600000 ? Math.round(ms / 60000) + ' min fa' : Math.round(ms / 3600000) + ' h fa';
  const fmtDur = (ms) => (ms / 1000).toFixed(1) + ' s';
  function cell(tr, text) { const td = document.createElement('td'); td.textContent = esc(text); tr.appendChild(td); return td; }
  async function refresh() {
    let s;
    try { s = await api('/api/status'); } catch (e) { return; }
    $('login').style.display = 'none'; $('app').hidden = false;
    info = s.reflector;
    $('head').textContent = 'id ' + s.reflector.id + ' — modo voce ' + s.reflector.voiceMode + ' — acceso da ' + Math.round(s.reflector.uptimeS / 60) + ' min';
    const now = $('now');
    if (s.talking) {
      now.className = 'live';
      now.innerHTML = '';
      const b = document.createElement('span'); b.className = 'big'; b.textContent = '● ' + s.talking.callsign; now.appendChild(b);
      const m = document.createElement('span'); m.className = 'muted'; m.textContent = 'da ' + s.talking.client + ', ' + fmtDur(s.talking.durationMs) + ', ' + s.talking.packets + ' pacchetti'; now.appendChild(m);
    } else { now.className = ''; now.innerHTML = '<span class="big">Nessuno</span>'; }
    const tb = $('clients'); tb.innerHTML = '';
    for (const c of s.clients) {
      const tr = document.createElement('tr');
      const n = cell(tr, c.name); n.style.fontWeight = '600';
      cell(tr, c.id);
      const st = document.createElement('td'); const d = document.createElement('span'); d.className = 'dot' + (c.up ? ' up' : ''); st.appendChild(d); st.appendChild(document.createTextNode(c.up ? 'collegato' : 'non collegato')); tr.appendChild(st);
      cell(tr, fmtAgo(c.seenMs)); cell(tr, c.in + ' / ' + c.out); cell(tr, c.endpoint || '-');
      const act = document.createElement('td'); const del = document.createElement('button'); del.className = 'danger'; del.textContent = 'Togli';
      del.addEventListener('click', async () => { if (!confirm('Togliere ' + c.name + ' (id ' + c.id + ')? Il nodo non potrà più collegarsi.')) return; await api('/api/remove', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'id=' + c.id }); refresh(); });
      act.appendChild(del); tr.appendChild(act);
      tb.appendChild(tr);
    }
    $('clientsEmpty').hidden = s.clients.length > 0;
    const rb = $('recent'); rb.innerHTML = '';
    for (const t of s.recent) { const tr = document.createElement('tr'); cell(tr, fmtAgo(t.agoMs)); cell(tr, t.callsign); cell(tr, t.client); cell(tr, fmtDur(t.durationMs)); cell(tr, t.packets); rb.appendChild(tr); }
    $('recentEmpty').hidden = s.recent.length > 0;
  }
  const normCall = () => $('call').value.trim().toUpperCase();
  async function check() {
    const c = normCall();
    if (!/^[A-Z0-9\/]{1,8}$/.test(c)) { show($('regMsg'), 'Nominativo non valido: lettere, cifre e "/", massimo 8 caratteri.', 'err'); return null; }
    const r = await api('/api/lookup?call=' + encodeURIComponent(c));
    const sel = $('target'), keep = sel.value;
    sel.innerHTML = '';
    const o0 = document.createElement('option'); o0.value = ''; o0.textContent = 'Aggiungere un nodo nuovo'; sel.appendChild(o0);
    for (const n of r.nodes) {
      const o = document.createElement('option'); o.value = String(n.id);
      o.textContent = 'Sostituire la chiave del nodo id ' + n.id + (n.label ? ' (' + n.label + ')' : '') + (n.up ? ' — collegato adesso' : '');
      sel.appendChild(o);
    }
    if ([...sel.options].some((o) => o.value === keep)) sel.value = keep;
    if (r.nodes.length) {
      const l = r.nodes.map((n) => 'id ' + n.id + (n.label ? ' ' + n.label : '') + (n.up ? ' (collegato)' : '')).join(', ');
      show($('regMsg'), c + ' ha già ' + r.nodes.length + (r.nodes.length === 1 ? ' nodo: ' : ' nodi: ') + l + '. Aggiungendone uno nuovo ha il prossimo id libero (' + r.nextId + ') e va chiamato in modo diverso; per cambiare la chiave di uno esistente scegli "Sostituire" qui sotto.', 'warn');
    } else {
      show($('regMsg'), c + ' non è ancora registrato: il nodo nuovo avrà il prossimo id libero (' + r.nextId + ').', 'ok');
    }
    return r;
  }
  $('checkBtn').addEventListener('click', check);
  $('call').addEventListener('change', () => { check().catch(() => {}); });
  $('regBtn').addEventListener('click', async () => {
    $('regOut').hidden = true; $('copyBtn').hidden = true;
    const c = normCall(), k = $('key').value.trim(), label = $('label').value.trim();
    const r0 = await check(); if (!r0) return;
    if (k !== '' && !/^[0-9a-fA-F]{32}$/.test(k)) { show($('regMsg'), 'La chiave deve avere 32 cifre esadecimali (oppure lasciala vuota).', 'err'); return; }
    const replace = $('target').value;
    if (replace && !confirm('Sostituire la chiave del nodo id ' + replace + '? Il nodo si stacca finché non usa la chiave nuova.')) return;
    const body = 'call=' + encodeURIComponent(c) + '&key=' + encodeURIComponent(k) + '&label=' + encodeURIComponent(label) + (replace ? '&replace=' + encodeURIComponent(replace) : '');
    const r = await api('/api/register', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body });
    if (!r.ok) { show($('regMsg'), r.error || 'Errore.', 'err'); return; }
    const who = r.call + (r.label ? ' ' + r.label : '');
    show($('regMsg'), (r.replaced ? 'Chiave sostituita per ' : 'Registrato ') + who + ' (id ' + r.id + '). Il nodo può collegarsi.', 'ok');
    const lines = ['Reflector: ' + (info.publicName || '(indirizzo del reflector):' + info.port), 'Id del reflector: ' + info.id, 'Id del nodo: ' + r.id, 'Nominativo: ' + r.call];
    if (r.label) lines.push('Nome del nodo: ' + r.label);
    lines.push(r.key ? 'Chiave: ' + r.key + '   (da dare al collega su un canale privato)' : 'Chiave: quella che mi hai mandato');
    $('regOut').hidden = false; $('regOut').textContent = lines.join('\n'); $('copyBtn').hidden = false;
    $('key').value = ''; $('target').value = '';
    refresh();
  });
  $('copyBtn').addEventListener('click', async () => { try { await navigator.clipboard.writeText($('regOut').textContent); show($('regMsg'), 'Copiato.', 'ok'); } catch (e) { show($('regMsg'), 'Copia non riuscita: seleziona il testo a mano.', 'err'); } });
  $('loginBtn').addEventListener('click', () => {
    token = $('token').value.trim();
    try { sessionStorage.setItem('vlr-token', token); } catch (e) {}
    api('/api/status').then(() => { $('loginMsg').hidden = true; refresh(); }).catch(() => { show($('loginMsg'), 'Password non valida.', 'err'); });
  });
  $('token').addEventListener('keydown', (e) => { if (e.key === 'Enter') $('loginBtn').click(); });
  refresh();
  setInterval(refresh, 2000);
})();
</script>
</body>
</html>
)HTML";
