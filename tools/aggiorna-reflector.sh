#!/bin/bash
# Aggiorna il reflector sul server con un solo comando (da lanciare come utente normale, nella cartella del repository o da qualunque posto):
#   ~/VoLoRa/tools/aggiorna-reflector.sh
# Scarica le novita', ricompila, ferma il servizio, copia il programma nuovo e lo riavvia.
set -e
cd "$(dirname "$0")/.."
git pull
make vlreflector
sudo systemctl stop vlreflector
sudo cp build/vlreflector /opt/vlreflector/
sudo systemctl start vlreflector
sleep 1
systemctl status vlreflector --no-pager | head -6
journalctl -u vlreflector -n 4 --no-pager
