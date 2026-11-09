#!/usr/bin/env bash
# Assembla il sito statico (pagina di installazione e configurazione) in una cartella.
#   tools/make_site.sh <cartella-artefatti> <cartella-uscita> <versione> <commit> [<cartella-release>]
# <cartella-artefatti> contiene firmware-t3-v161/ e firmware-tbeam-v12/ (come li scarica actions/download-artifact),
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

# manifest.json di una versione: <cartella> <percorso-del-binario> <versione> <scheda>
manifest() {
  local dir=$1 path=$2 ver=$3 board=$4
  cat > "$dir/manifest.json" <<EOF
{
  "name": "LoRa-Codec-2-RTX ($board)",
  "version": "$ver",
  "new_install_prompt_erase": false,
  "builds": [
    { "chipFamily": "ESP32", "parts": [ { "path": "$path", "offset": 0 } ] }
  ]
}
EOF
}

for pair in "t3:firmware-t3-v161" "tbeam:firmware-tbeam-v12"; do
  board=${pair%%:*}
  art=${pair#*:}
  test -f "$in/$art/web_factory.bin" || { echo "manca $in/$art/web_factory.bin" >&2; exit 1; }

  # Build corrente
  mkdir -p "$out/firmware/$cur_id/$board" "$out/firmware/$board"
  cp "$in/$art/web_factory.bin" "$in/$art/ota_update.bin" "$out/firmware/$cur_id/$board/"
  manifest "$out/firmware/$cur_id/$board" "web_factory.bin" "$version" "$board"
  # Percorso storico firmware/<scheda>/manifest.json: stesso binario, senza una seconda copia
  manifest "$out/firmware/$board" "../$cur_id/$board/web_factory.bin" "$version" "$board"

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
      manifest "$out/firmware/$tag/$board" "web_factory.bin" "$tag" "$board"
    done
  fi
done

python3 "$here/tools/site_versions.py" --current-id "$cur_id" --current-version "$version" --current-commit "$commit" \
  ${releases:+--releases-dir "$releases"} ${releases:+--index "$releases/index.json"} --out "$out/versions.json"

cat > "$out/version.json" <<EOF
{ "version": "$version", "commit": "$commit" }
EOF
echo "sito in $out (versione $version, commit $commit, $(python3 -c "import json;print(len(json.load(open('$out/versions.json'))['versions']))") versioni nel menu)"
