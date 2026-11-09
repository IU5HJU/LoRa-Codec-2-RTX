#!/usr/bin/env bash
# Ricevitore SDR in tempo reale per LoRa-Codec-2-RTX: la chiavetta riceve, i pacchetti vengono ricostruiti come fa la scheda
# ricevente e la voce esce dall'altoparlante mentre la scheda trasmette (ritardo di circa 1-2 s).
#
#   chiavetta -> lora_rx.py -> file -> tail -F -> lcr_live -> c2dec -> aplay
#
# Si ferma con Ctrl+C. La cattura resta in ~/catture-sdr/ e alla fine se ne stampa il riepilogo.
# Servono: l'ambiente conda "lora" (docs/sdr.md), "make lcr_live" (lo fa da solo), c2dec (sudo apt install codec2) e aplay
# (sudo apt install alsa-utils).
REPO=${REPO:-$HOME/LoRa-Codec-2-RTX}
OUT=${OUT:-$HOME/catture-sdr}
mkdir -p "$OUT"
fine() { read -rp "Premi Invio per chiudere"; exit "${1:-0}"; }

for c in ~/miniconda3 ~/anaconda3 ~/miniforge3 ~/mambaforge /opt/conda; do
  if [ -f "$c/etc/profile.d/conda.sh" ]; then source "$c/etc/profile.d/conda.sh"; break; fi
done
command -v conda >/dev/null || { echo "conda non trovato"; fine 1; }
conda activate lora || { echo "Ambiente conda 'lora' non trovato (vedi docs/sdr.md)"; fine 1; }
command -v c2dec >/dev/null || { echo "Manca c2dec: sudo apt install codec2"; fine 1; }
command -v aplay >/dev/null || { echo "Manca aplay: sudo apt install alsa-utils"; fine 1; }
[ -x "$REPO/build/lcr_live" ] || make -C "$REPO" lcr_live >/dev/null || { echo "Compilazione di lcr_live fallita"; fine 1; }
[ -x "$REPO/build/lcr_decode" ] || make -C "$REPO" lcr_decode >/dev/null

echo "=== Ricevitore SDR in tempo reale ==="
echo "Antenna collegata e scheda a piu' di un metro dalla chiavetta."
echo
echo "Che modo ascolti?"
select M in "Standard (SF8, banda 250 kHz): voce in diretta" "Messaggio (SF12, banda 125 kHz): si sente a messaggio completo" "Esci"; do
  case $REPLY in
    1) SF=8;  BW=250000; OPT=""; break ;;
    2) SF=12; BW=125000; OPT="--messaggio"; break ;;
    3) exit 0 ;;
    *) echo "Scelta non valida" ;;
  esac
done
read -rp "Frequenza in MHz [433.9]: " F; F=${F:-433.9}
read -rp "Guadagno RF in dB [30] (abbassalo se la scheda e' vicina): " G; G=${G:-30}
FREQ=$(awk -v f="$F" 'BEGIN{printf "%d", f*1000000}')
FILE="$OUT/cattura-$(date +%Y%m%d-%H%M%S).txt"
LOG="${FILE%.txt}.log"
: > "$FILE"

# Su Ubuntu 20.04 la libudev di conda non vede la chiavetta: si usano quelle del sistema (vedi docs/sdr.md)
LDP=""
for l in /lib/x86_64-linux-gnu/libudev.so.1 /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 /usr/lib/x86_64-linux-gnu/librtlsdr.so.0; do
  [ -f "$l" ] && LDP="$LDP $l"
done

echo
echo "In ascolto su $F MHz, SF$SF, banda $((BW/1000)) kHz. Ctrl+C per fermare."
echo "Cattura: $FILE"
trap : INT   # Ctrl+C ferma il ricevitore ma non questo script
LD_PRELOAD="$LDP" python3 "$REPO/tools/sdr/lora_rx.py" --freq "$FREQ" --sf "$SF" --bw "$BW" --gain "$G" --quiet --out "$FILE" >"$LOG" 2>&1 &
RX=$!
sleep 4   # la chiavetta ha bisogno di qualche istante per partire
if ! kill -0 "$RX" 2>/dev/null; then
  echo "Il ricevitore non e' partito. Ultime righe del registro:"; tail -8 "$LOG"; fine 1
fi
echo "Pronto: fai trasmettere la scheda."
tail -n0 -F "$FILE" 2>/dev/null | "$REPO/build/lcr_live" $OPT | c2dec 1200 - - | aplay -q -t raw -f S16_LE -r 8000 -c 1
kill "$RX" 2>/dev/null
wait "$RX" 2>/dev/null
trap - INT

echo
if [ -s "$FILE" ]; then
  echo "=== Riepilogo della cattura ==="
  mkdir -p "${FILE%.txt}-sessioni"
  "$REPO/build/lcr_decode" "$FILE" --out "${FILE%.txt}-sessioni" | grep -v "^  "
else
  echo "Nessun pacchetto ricevuto."
fi
echo
fine
