#!/bin/bash
# Prova con veri socket UDP (make reflector-smoke): il reflector e due nodi finti sul PC. I nodi usano una porta scelta dal sistema (--listen 0) e
# scrivono per primi: nessuna porta nota o aperta, come dietro un router. Il secondo nodo deve ricevere il testo e la voce del primo.
set -u
cd "$(dirname "$0")/.."
D=$(mktemp -d)
trap 'kill $(jobs -p) 2>/dev/null; rm -rf "$D"' EXIT
PORT=$((20000 + RANDOM % 20000))
cat > "$D/ref.conf" <<EOF
2 000102030405060708090a0b0c0d0e0f nodoA
3 101112131415161718191a1b1c1d1e1f nodoB
EOF
build/vlreflector --config "$D/ref.conf" --port $PORT --id 1000 --voice-mode 1 --duration 22 > "$D/ref.out" 2>&1 &
sleep 0.5
build/vlnode --id 3 --peer-id 1000 --key 101112131415161718191a1b1c1d1e1f --listen 0 --peer localhost:$PORT --duration 18 > "$D/b.out" 2>&1 &
sleep 4
build/vlnode --id 2 --peer-id 1000 --key 000102030405060708090a0b0c0d0e0f --listen 0 --peer 127.0.0.1:$PORT --send-text IU5HJU "Prova via reflector" --send-voice IU5HJU --frames 30 --duration 10 > "$D/a.out" 2>&1
wait
fail=0
check() { if grep -q "$2" "$1"; then echo "ok    $3"; else echo "FALLITO $3"; fail=1; fi; }
check "$D/ref.out" "client nodoA collegato" "il reflector vede il nodo A"
check "$D/ref.out" "client nodoB collegato" "il reflector vede il nodo B"
check "$D/b.out" 'testo "Prova via reflector"' "B riceve il testo di A"
check "$D/b.out" "modo 1 sessione .* flags C9 IU5HJU (ripetuto)" "B riceve l'inizio della voce di A, con il flag di ripetuto"
n=$(grep -c "^\[pacchetto\] modo 1 " "$D/b.out")
if [ "$n" = "5" ]; then echo "ok    B riceve i 5 pacchetti di voce"; else echo "FALLITO B riceve $n pacchetti di voce invece di 5"; fail=1; fi
if grep -q "pacchetto" "$D/a.out"; then echo "FALLITO A riceve indietro i suoi pacchetti"; fail=1; else echo "ok    A non riceve indietro i suoi pacchetti"; fi
check "$D/ref.out" "firme errate 0" "nessuna firma errata"
exit $fail
