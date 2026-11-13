// La pagina pubblica del reflector (servita da tools/vlreflector.cpp con --public-web): solo chi e' collegato e chi trasmette adesso, cosi' i
// colleghi vedono se il loro nodo e' davvero collegato. Niente token, niente chiavi, niente indirizzi IP, niente id dei nodi, niente comandi.
// Una pagina sola, senza librerie esterne; i dati arrivano da /status.json (stessa origine).
#pragma once

static const char kPublicPage[] = R"HTML(<!doctype html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Reflector VoLoRa</title>
<style>
  :root { --bg:#f6f7f9; --card:#ffffff; --fg:#1b1f24; --muted:#5b6672; --line:#d5dae0; --ok:#1a7f37; --bad:#b42318; --live:#b42318; }
  @media (prefers-color-scheme: dark) { :root { --bg:#0f1317; --card:#171c22; --fg:#e8ecf0; --muted:#9aa5b1; --line:#2b333c; --ok:#4cc26b; --bad:#ff7b72; --live:#ff7b72; } }
  * { box-sizing:border-box; }
  body { margin:0; background:var(--bg); color:var(--fg); font:16px/1.45 system-ui, sans-serif; }
  main { max-width:720px; margin:0 auto; padding:16px; }
  h1 { font-size:1.4rem; margin:.2em 0 .2em; }
  h2 { font-size:1.05rem; margin:0 0 .6em; }
  section { background:var(--card); border:1px solid var(--line); border-radius:10px; padding:14px; margin:0 0 14px; }
  .muted { color:var(--muted); font-size:.9rem; }
  table { width:100%; border-collapse:collapse; font-size:.95rem; }
  th, td { text-align:left; padding:6px 8px; border-bottom:1px solid var(--line); }
  th { color:var(--muted); font-weight:600; }
  .dot { display:inline-block; width:.7em; height:.7em; border-radius:50%; background:var(--ok); margin-right:.4em; }
  #now { min-height:3em; display:flex; align-items:center; }
  #now .big { font-size:1.6rem; font-weight:700; }
  #now.live .big { color:var(--live); }
</style>
</head>
<body>
<main>
  <h1>Reflector VoLoRa</h1>
  <p class="muted" id="info">Carico...</p>
  <section>
    <h2>In trasmissione adesso</h2>
    <div id="now"><span class="big muted">Silenzio</span></div>
  </section>
  <section>
    <h2>Stazioni collegate <span class="muted" id="count"></span></h2>
    <div id="clients"></div>
  </section>
  <section>
    <h2>Ultime trasmissioni</h2>
    <div id="recent"></div>
  </section>
  <p class="muted" id="err"></p>
</main>
<script>
(function () {
  var $ = function (id) { return document.getElementById(id); };
  function el(tag, text, cls) { var e = document.createElement(tag); if (text !== undefined) e.textContent = text; if (cls) e.className = cls; return e; }
  function ago(s) { return s < 60 ? s + " s fa" : s < 3600 ? Math.floor(s / 60) + " min fa" : Math.floor(s / 3600) + " h fa"; }
  function dur(s) { return s < 60 ? s + " s" : Math.floor(s / 60) + " min " + (s % 60) + " s"; }
  function table(head, rows) {
    var t = el("table"), h = el("tr");
    head.forEach(function (x) { h.appendChild(el("th", x)); });
    t.appendChild(h);
    rows.forEach(function (r) { var tr = el("tr"); r.forEach(function (c) { var td = el("td"); if (c instanceof Node) td.appendChild(c); else td.textContent = c; tr.appendChild(td); }); t.appendChild(tr); });
    return t;
  }
  function who(call, label) { return label ? call + " (" + label + ")" : call; }
  function show(d) {
    var r = d.reflector;
    $("info").textContent = (r.name ? r.name + " - " : "") + "modo " + (r.mode === "standard" ? "Standard (voce Codec2 3200)" : "Range (voce Codec2 1200)") + " - acceso da " + dur(r.uptimeS);
    var now = $("now");
    now.textContent = "";
    if (d.talking) { now.className = "live"; now.appendChild(el("span", d.talking.callsign + " - " + dur(d.talking.durationS), "big")); }
    else { now.className = ""; now.appendChild(el("span", "Silenzio", "big muted")); }
    $("count").textContent = "(" + d.clients.length + ")";
    var c = $("clients"); c.textContent = "";
    if (!d.clients.length) c.appendChild(el("p", "Nessuna stazione collegata.", "muted"));
    else c.appendChild(table(["Stazione", "Ultimo contatto"], d.clients.map(function (x) {
      var s = el("span"); s.appendChild(el("span", "", "dot")); s.appendChild(document.createTextNode(who(x.call, x.label)));
      return [s, x.seenS + " s fa"];
    })));
    var rc = $("recent"); rc.textContent = "";
    if (!d.recent.length) rc.appendChild(el("p", "Ancora nessuna trasmissione.", "muted"));
    else rc.appendChild(table(["Stazione", "Quando", "Durata"], d.recent.map(function (x) { return [x.callsign, ago(x.agoS), dur(x.durationS)]; })));
    $("err").textContent = "";
  }
  function poll() {
    var x = new XMLHttpRequest();
    x.open("GET", "status.json");
    x.timeout = 4000;
    x.onload = function () { try { show(JSON.parse(x.responseText)); } catch (e) { $("err").textContent = "Dati non leggibili."; } };
    x.onerror = x.ontimeout = function () { $("err").textContent = "Reflector non raggiungibile: riprovo."; };
    x.send();
  }
  poll();
  setInterval(poll, 3000);
})();
</script>
</body>
</html>
)HTML";
