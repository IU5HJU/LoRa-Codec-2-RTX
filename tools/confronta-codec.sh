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

# --- Scelta del microfono ---
# Con PulseAudio (Ubuntu MATE) si elencano le sorgenti di ingresso (senza i "monitor", che sono l'audio in uscita);
# altrimenti si elencano le schede audio di ALSA.
SRC=""      # sorgente PulseAudio scelta ("" = predefinita)
ALSADEV=""  # dispositivo ALSA scelto ("" = predefinito)
NOME="predefinito del sistema"
scegli_microfono() {
  local names=() descs=() n d
  if command -v pactl >/dev/null 2>&1 && pactl info >/dev/null 2>&1; then
    while IFS='|' read -r n d; do names+=("$n"); descs+=("$d"); done < <(
      pactl list sources | awk -F': ' '$1 ~ /Name$/ {name=$2} $1 ~ /Description$/ {if (name !~ /\.monitor$/) print name "|" $2}')
    if [ ${#names[@]} -eq 0 ]; then echo "Nessun microfono trovato da PulseAudio: si usa quello predefinito."; return; fi
    local def; def=$(pactl get-default-source 2>/dev/null || pactl info | sed -n 's/^Default Source: //p')
    echo
    echo "Microfoni trovati (scrivi il numero di quello che vuoi usare):"
    local i
    for i in "${!names[@]}"; do
      echo "  $((i+1))) ${descs[$i]}$([ "${names[$i]}" = "$def" ] && echo "   <-- predefinito del sistema")"
    done
    local c
    read -rp "Scelta [Invio = il predefinito]: " c
    if [ -n "$c" ] && [ "$c" -ge 1 ] 2>/dev/null && [ "$c" -le ${#names[@]} ]; then
      SRC="${names[$((c-1))]}"; NOME="${descs[$((c-1))]}"
    elif [ -n "$def" ]; then
      for i in "${!names[@]}"; do [ "${names[$i]}" = "$def" ] && NOME="${descs[$i]}"; done
    fi
  else
    echo
    echo "Schede audio di registrazione trovate (ALSA):"
    arecord -l 2>/dev/null | grep '^card' | sed 's/^/  /'
    local c dv
    read -rp "Numero della scheda e del dispositivo, per esempio 1,0 [Invio = il predefinito]: " c
    if [[ "$c" =~ ^[0-9]+,[0-9]+$ ]]; then ALSADEV="plughw:$c"; NOME="scheda $c"; fi
  fi
}
# Comando di registrazione (a 8 kHz, mono, 16 bit) dal microfono scelto, in RECCMD.
set_reccmd() {
  if [ -n "$SRC" ]; then RECCMD=(env PULSE_SOURCE="$SRC" arecord -q -D pulse -f S16_LE -r 8000 -c 1)
  elif [ -n "$ALSADEV" ]; then RECCMD=(arecord -q -D "$ALSADEV" -f S16_LE -r 8000 -c 1)
  else RECCMD=(arecord -q -f S16_LE -r 8000 -c 1); fi
}
rec() {   # rec <file> [opzioni]: registrazione in primo piano (per la prova di 3 secondi)
  local f=$1; shift
  set_reccmd
  "${RECCMD[@]}" "$@" "$f"
}
while true; do
  scegli_microfono
  echo
  echo "Uso il microfono: $NOME"
  echo "Prova rapida: dopo il conto alla rovescia di 3 secondi parla per 3 secondi, poi riascolti."
  read -rp "Premi Invio per la prova: "
  for n in 3 2 1; do echo "$n..."; sleep 1; done
  echo ">>> parla adesso (3 secondi) <<<"
  rec "$DIR/prova-microfono.wav" -d 3
  echo "Riascolto..."
  ffplay -nodisp -autoexit -loglevel quiet "$DIR/prova-microfono.wav"
  read -rp "Ti sei sentito bene? [S = si, procedo; n = scelgo un altro microfono] " ok
  case "$ok" in [nN]*) SRC=""; ALSADEV=""; NOME="predefinito del sistema"; continue ;; esac
  break
done

echo "=== Confronto dei modi del Codec2 ==="
echo
echo "Poi si registra la voce vera. Leggi una frase come questa, a voce normale, a circa 20 cm dal microfono:"
echo "  \"Qui IU5HJU, prova numero 4 5 7. Ripeto: 4 5 7. Passo.\""
echo "Meglio 10-20 secondi, anche piu' frasi. Poi aspetta un secondo e premi Invio per finire."
echo
read -rp "Premi Invio quando sei pronto: "
for n in 3 2 1; do echo "$n..."; sleep 1; done
echo ">>> REGISTRO: parla adesso <<<"
# arecord parte DIRETTAMENTE in background (non dentro una funzione): cosi' REC e' il suo PID e il segnale di fine lo
# raggiunge. Un comando in background ignora il Ctrl+C del guscio, ma arecord installa il proprio gestore di SIGINT.
set_reccmd
"${RECCMD[@]}" "$DIR/voce.wav" &
REC=$!
read -rp "(Premi Invio per FINIRE la registrazione) "
kill -INT "$REC" 2>/dev/null            # fine normale: arecord chiude e completa il file WAV
for _ in 1 2 3 4 5 6; do kill -0 "$REC" 2>/dev/null || break; sleep 0.5; done
if kill -0 "$REC" 2>/dev/null; then kill -TERM "$REC" 2>/dev/null; sleep 1; fi
if kill -0 "$REC" 2>/dev/null; then kill -KILL "$REC" 2>/dev/null; fi   # ultima risorsa: lo script non resta mai bloccato qui
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
