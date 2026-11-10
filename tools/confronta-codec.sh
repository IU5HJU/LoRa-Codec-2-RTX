#!/usr/bin/env bash
# Confronto dei modi del Codec2 con la tua voce: registra, confronta e fa ascoltare (vedi docs/codec2.md).
#
#   1. premi Invio: conto alla rovescia di 3 secondi, poi parla
#   2. premi Invio per finire la registrazione
#   3. si confronta con tutti i modi e si sceglie cosa ascoltare, quante volte si vuole
#
# Servono arecord (sudo apt install alsa-utils) e ffplay (sudo apt install ffmpeg). Le registrazioni restano in ~/confronto-codec/.
REPO=${REPO:-$HOME/LoRa-Codec-2-RTX}
OUT=${OUT:-$HOME/confronto-codec}
fine() { echo; read -rp "Premi Invio per chiudere"; exit "${1:-0}"; }

command -v arecord >/dev/null || { echo "Manca arecord: sudo apt install alsa-utils"; fine 1; }
command -v ffplay >/dev/null || { echo "Manca ffplay: sudo apt install ffmpeg"; fine 1; }
if [ ! -x "$REPO/build/codec_compare" ]; then
  echo "Compilo lo strumento di confronto..."
  make -C "$REPO" codec_compare >/dev/null 2>&1 || { echo "Compilazione fallita: nella cartella $REPO prova 'git pull origin main' e poi 'make codec_compare'"; fine 1; }
fi

DIR="$OUT/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$DIR"

echo "=== Confronto dei modi del Codec2 ==="
echo
echo "Leggi una frase come questa, a voce normale, a circa 20 cm dal microfono:"
echo "  \"Qui IU5HJU, prova numero 4 5 7. Ripeto: 4 5 7. Passo.\""
echo "Meglio 10-20 secondi, anche piu' frasi. Poi aspetta un secondo e premi Invio per finire."
echo
read -rp "Premi Invio quando sei pronto: "
for n in 3 2 1; do echo "$n..."; sleep 1; done
echo ">>> REGISTRO: parla adesso <<<"
arecord -q -f S16_LE -r 8000 -c 1 "$DIR/voce.wav" &
REC=$!
read -rp "(Premi Invio per FINIRE la registrazione) "
kill -INT "$REC" 2>/dev/null
wait "$REC" 2>/dev/null
echo "Registrazione finita."

SIZE=$(stat -c %s "$DIR/voce.wav" 2>/dev/null || echo 0)
if [ "$SIZE" -lt 16044 ]; then   # meno di un secondo a 8 kHz, 16 bit
  echo "La registrazione e' troppo breve (meno di 1 secondo) o il microfono non e' stato trovato."
  echo "Controlla il microfono nelle impostazioni audio e riprova."
  fine 1
fi
SECS=$(( (SIZE - 44) / 16000 ))
echo "Registrati circa $SECS secondi."
echo
echo "=== Confronto dei modi ==="
"$REPO/build/codec_compare" "$DIR/voce.wav" "$DIR/confronto" || fine 1
"$REPO/build/codec_compare" "$DIR/voce.wav" "$DIR/con-perdita" --modi 1200,700C,450 --perdita 5 >/dev/null || fine 1

play() { echo "  > $1"; ffplay -nodisp -autoexit -loglevel quiet "$1"; }

while true; do
  echo
  echo "Cosa vuoi ascoltare? (scrivi il numero)"
  PS3="Scelta: "
  select S in \
    "La tua voce originale" \
    "1200 (lo Standard di oggi)" "700C (Messaggio, piu' compatto)" "450 (Messaggio, il piu' compatto)" \
    "3200 (Qualita')" "2400 (Qualita', ripiego)" \
    "1200 con il 5% di frame persi" "700C con il 5% di frame persi" "450 con il 5% di frame persi" \
    "Tutti di seguito, in ordine" "Esci"; do
    case $REPLY in
      1) play "$DIR/confronto/originale.wav" ;;
      2) play "$DIR/confronto/1200.wav" ;;
      3) play "$DIR/confronto/700C.wav" ;;
      4) play "$DIR/confronto/450.wav" ;;
      5) play "$DIR/confronto/3200.wav" ;;
      6) play "$DIR/confronto/2400.wav" ;;
      7) play "$DIR/con-perdita/1200.wav" ;;
      8) play "$DIR/con-perdita/700C.wav" ;;
      9) play "$DIR/con-perdita/450.wav" ;;
      10) for f in confronto/originale confronto/1200 confronto/700C confronto/450 confronto/3200 confronto/2400; do play "$DIR/$f.wav"; sleep 1; done ;;
      11) echo "I file restano in $DIR"; fine 0 ;;
      *) echo "Scelta non valida" ;;
    esac
    break
  done
done
