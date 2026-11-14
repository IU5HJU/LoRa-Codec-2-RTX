#!/usr/bin/env bash
# Assembla il sito statico (pagina di installazione e configurazione) in una cartella.
#   tools/make_site.sh <cartella-artefatti> <cartella-uscita> <versione> <commit> [<cartella-release>]
# <cartella-artefatti> contiene firmware-t3-v161/, firmware-tbeam-v12/ e firmware-tbeam-v12-sx1262/ (come li scarica actions/download-artifact),
# ognuna con web_factory.bin e ota_update.bin: e' il build corrente.
# <cartella-release> (facoltativa) contiene una sottocartella per ogni release gia' pubblicata (nome = tag, per esempio
# v0.1.0-beta.1) con i suoi *-web_factory.bin, piu' index.json di `gh release list --json tagName,publishedAt`:
# servono per il menu delle versioni della pagina.
#
# Struttura prodotta:
#   firmware/<id>/<scheda>/   web_factory.bin e manifest.json di ogni versione; <id> e' "dev" per lo sviluppo (main),
#                             oppure il tag (build corrente di un tag e release scaricate)
#   firmware/<scheda>/        manifest del build corrente (per le pagine in cache di prima del menu delle versioni)
#   versions.json             elenco per il menu (tools/site_versions.py); version.json: il build corrente
set -euo pipefail
in=${1:?cartella degli artefatti}
out=${2:?cartella di uscita}
version=${3:?versione}
commit=${4:?commit}
releases=${5:-}
here=$(cd "$(dirname "$0")/.." && pwd)

case "$version" in main-*) cur_id=dev ;; *) cur_id=$version ;; esac

rm -rf "$out"
mkdir -p "$out/firmware"
cp "$here"/site/index.html "$out/"
touch "$out/.nojekyll"

# web_factory.bin e' un'immagine unica da scrivere all'indirizzo 0, con i vuoti fra i pezzi riempiti di 0xFF: comprese le impostazioni salvate sulla
# scheda (partizione nvs, 0x9000-0xDFFF), che una scrittura intera cancellerebbe a ogni aggiornamento. Il flasher della pagina scrive invece solo i
# quattro pezzi, tagliati dall'immagine: bootloader (0x1000), tabella delle partizioni (0x8000), avvio (0xE000) e applicazione (0x10000).
# split_image <immagine> <cartella>: scrive part_boot.bin, part_table.bin, part_otadata.bin e part_app.bin.
split_image() {
  python3 - "$1" "$2" <<'PY'
import sys
src, out = sys.argv[1], sys.argv[2]
d = open(src, "rb").read()
if len(d) <= 0x10000:
    sys.exit("immagine troppo corta per il flasher: %s (%d byte)" % (src, len(d)))
for name, a, b in (("part_boot.bin", 0x1000, 0x8000), ("part_table.bin", 0x8000, 0x9000), ("part_otadata.bin", 0xE000, 0x10000), ("part_app.bin", 0x10000, len(d))):
    open(out + "/" + name, "wb").write(d[a:b])
PY
}

# manifest.json di una versione: <cartella> <prefisso-dei-percorsi> <versione> <scheda>
# Quattro pezzi (non l'immagine intera): la partizione delle impostazioni, in mezzo, non viene toccata.
manifest() {
  local dir=$1 pre=$2 ver=$3 board=$4
  cat > "$dir/manifest.json" <<EOF
{
  "name": "VoLoRa ($board)",
  "version": "$ver",
  "new_install_prompt_erase": true,
  "builds": [
    { "chipFamily": "ESP32", "parts": [
      { "path": "${pre}part_boot.bin", "offset": 4096 },
      { "path": "${pre}part_table.bin", "offset": 32768 },
      { "path": "${pre}part_otadata.bin", "offset": 57344 },
      { "path": "${pre}part_app.bin", "offset": 65536 }
    ] }
  ]
}
EOF
}

for pair in "t3:firmware-t3-v161" "tbeam:firmware-tbeam-v12" "tbeam1262:firmware-tbeam-v12-sx1262"; do
  board=${pair%%:*}
  art=${pair#*:}
  test -f "$in/$art/web_factory.bin" || { echo "manca $in/$art/web_factory.bin" >&2; exit 1; }

  # Build corrente
  mkdir -p "$out/firmware/$cur_id/$board" "$out/firmware/$board"
  cp "$in/$art/web_factory.bin" "$in/$art/ota_update.bin" "$out/firmware/$cur_id/$board/"
  split_image "$in/$art/web_factory.bin" "$out/firmware/$cur_id/$board"
  manifest "$out/firmware/$cur_id/$board" "" "$version" "$board"
  # Percorso storico firmware/<scheda>/manifest.json: stessi pezzi, senza una seconda copia
  manifest "$out/firmware/$board" "../$cur_id/$board/" "$version" "$board"

  # Release gia' pubblicate (una cartella per tag)
  if [ -n "$releases" ] && [ -d "$releases" ]; then
    for d in "$releases"/*/; do
      [ -d "$d" ] || continue
      tag=$(basename "$d")
      [ "$tag" = "$cur_id" ] && continue
      f="$d/$art-web_factory.bin"
      [ -f "$f" ] || continue
      mkdir -p "$out/firmware/$tag/$board"
      cp "$f" "$out/firmware/$tag/$board/web_factory.bin"
      split_image "$f" "$out/firmware/$tag/$board"
      manifest "$out/firmware/$tag/$board" "" "$tag" "$board"
    done
  fi
done

python3 "$here/tools/site_versions.py" --current-id "$cur_id" --current-version "$version" --current-commit "$commit" \
  ${releases:+--releases-dir "$releases"} ${releases:+--index "$releases/index.json"} --out "$out/versions.json"

# Per ogni versione le schede che hanno davvero il firmware (le release uscite prima di una scheda nuova non lo hanno): la pagina non
# propone la versione per una scheda che non c'e'
python3 - "$out" <<'PY'
import json, os, sys
out = sys.argv[1]
p = os.path.join(out, "versions.json")
v = json.load(open(p))
for e in v["versions"]:
    d = os.path.join(out, "firmware", e["id"])
    e["boards"] = sorted(b for b in os.listdir(d) if os.path.isfile(os.path.join(d, b, "manifest.json"))) if os.path.isdir(d) else []
json.dump(v, open(p, "w"), indent=2, ensure_ascii=False)
PY

cat > "$out/version.json" <<EOF
{ "version": "$version", "commit": "$commit" }
EOF
echo "sito in $out (versione $version, commit $commit, $(python3 -c "import json;print(len(json.load(open('$out/versions.json'))['versions']))") versioni nel menu)"
