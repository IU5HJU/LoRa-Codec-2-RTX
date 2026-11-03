# Roadmap e decisioni

Aggiornato il 3 novembre 2026. Ordine di lavoro concordato: prima la prima trasmissione,
poi TOT e profili, poi la pagina web di configurazione.

## 1. Prima trasmissione (in corso)

Procedura in [firmware.md](firmware.md). Stato: schede arrivate, compilazione e carico a carico di IU5HJU su Windows.
Regola ferma: **mai alimentare una scheda senza antenna**, nemmeno via USB per caricare il firmware.

## 2. TOT fino a 3 minuti e profili (dopo la prima trasmissione)

Decisioni prese:

- Il TOT diventa un'impostazione a gradini: **30 s, 60 s, 120 s, 180 s**. Per le prove resta 30 s.
- **Budget di tempo in aria per turno: 60 s.** Occupazione massima del canale: **70 %** del tempo di voce, parità compresa.
- Regola: TOT massimo = il gradino più grande che sta sotto `60 s / occupazione`, con tetto 180 s.
- Il livello è un accordo di rete: i parametri LoRa devono essere uguali tra le stazioni sulla stessa frequenza. Il modo Codec2 è nell'header e il ricevitore si adatta da solo.

Scala a gradini proposta (stime teoriche, da verificare sul campo):

| Livello | Codec2 | LoRa | Sensibilità vs SF7/BW250 | Occupazione | TOT max |
|---|---|---|---|---|---|
| 1 Massima copertura | 450 (**provvisorio**) | SF9/BW125 | +8,0 dB | 69 % | 60 s |
| 2 Efficienza | 700C | SF8/BW125 | +5,5 dB | 48 % | 120 s |
| 3 Standard | 1200 | SF8/BW250 | +2,5 dB | 33 % | 180 s |
| 4 Qualità | 2400 | SF7/BW250 | 0 dB | 33 % | 180 s |

**Livello 1 da decidere ad orecchio.** L'esperienza con FreeDV dice che già il 700C è molto "robotico" e il 450 lo sarà ancora
di più. Alternative da confrontare sul campo: 450 con parità, 700C a SF9/BW125 senza parità (68 %), 700C a SF8/BW125.
"700D" non è un modo di Codec2 ma un modem FreeDV che usa il vocoder 700C.

Modifiche al codice previste:

- sequenza dei pacchetti a 16 bit e finestra scorrevole in ricezione (al posto dei 128 slot fissi);
- frame di dimensione variabile in bit (700C: 28 bit, 450: 18 bit) con impacchettamento a bit;
- modi a 20 ms (2400, 3200) come due blocchi per frame audio;
- tabella dei profili nel core, guidata da dati, con test sul PC che verifica il limite del 70 % per ogni profilo;
- facoltativo: bip a 10 s dalla fine del TOT;
- il formato dei pacchetti cambia: le due schede devono avere la stessa versione.

## 3. Pagina web di configurazione (dopo il TOT)

- Entrata con pressione lunga (3 s) sul pulsante volume (GPIO15) a scheda ferma; in quella modalità la **trasmissione è disattivata**.
- La scheda crea un Wi-Fi proprio (`LoRaCodec2-XXXX`, password sull'OLED); pagina su `192.168.4.1`; salvataggio in memoria non volatile e riavvio.
- Impostazioni: nominativo, frequenza (limitata ai 70 cm), SF/BW/CR/potenza/sync, parità, **TOT**, livello/profilo, guadagno microfono, AGC, volume.
- Regole di validazione nel core (testabili sul PC); server web con le librerie già incluse nell'ESP32.

## Aperto

- Frequenza di lavoro: ancora il segnaposto 433,900 MHz in `src/user_config.h`, da scegliere col band plan IARU R1 / ARI.
- Licenza del progetto: la libreria `esp32_codec2` è GPL-3.0 (vedi [firmware.md](firmware.md)).
- Compilazione con PlatformIO e prova su scheda: mai eseguite.
