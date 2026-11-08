#!/usr/bin/env bash
# Assembla il sito statico (pagina di installazione e configurazione) in una cartella.
#   tools/make_site.sh <cartella-artefatti> <cartella-uscita> <versione> <commit>
# <cartella-artefatti> contiene firmware-t3-v161/, firmware-tbeam-v12/ e firmware-tbeam-v12-senza-display/ (come li scarica
# actions/download-artifact), ognuna con web_factory.bin e ota_update.bin.
set -euo pipefail
in=${1:?cartella degli artefatti}
out=${2:?cartella di uscita}
version=${3:?versione}
commit=${4:?commit}
here=$(cd "$(dirname "$0")/.." && pwd)

rm -rf "$out"
mkdir -p "$out/firmware"
cp "$here"/site/index.html "$out/"
touch "$out/.nojekyll"

for pair in "t3:firmware-t3-v161" "tbeam:firmware-tbeam-v12" "tbeam-nodisplay:firmware-tbeam-v12-senza-display"; do
  board=${pair%%:*}
  art=${pair#*:}
  test -f "$in/$art/web_factory.bin" || { echo "manca $in/$art/web_factory.bin" >&2; exit 1; }
  mkdir -p "$out/firmware/$board"
  cp "$in/$art/web_factory.bin" "$in/$art/ota_update.bin" "$out/firmware/$board/"
  cat > "$out/firmware/$board/manifest.json" <<EOF
{
  "name": "LoRa-Codec-2-RTX ($board)",
  "version": "$version",
  "new_install_prompt_erase": false,
  "builds": [
    { "chipFamily": "ESP32", "parts": [ { "path": "web_factory.bin", "offset": 0 } ] }
  ]
}
EOF
done

cat > "$out/version.json" <<EOF
{ "version": "$version", "commit": "$commit" }
EOF
echo "sito in $out (versione $version, commit $commit)"
