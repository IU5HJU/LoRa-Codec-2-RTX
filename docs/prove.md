# Registro delle prove

Qui si annotano le prove fatte sulle schede, da chiunque partecipi, in modo che i risultati restino
nel repository. Una prova per sezione, in ordine cronologico, la più recente in fondo.
Per un problema da risolvere apri invece una segnalazione (Issues) e collega la prova.

> **Prima di ogni prova:** antenna a 433 MHz collegata a entrambe le schede **prima** del cavo USB,
> microSD tolta, PTT non premuto all'accensione. Trasmetti solo con il tuo nominativo, su una frequenza
> e con una potenza consentite. Procedura di caricamento in [firmware.md](firmware.md).

## Quanto annotare per ogni prova

| Voce | Esempio |
|---|---|
| Data e luogo | 2026-11-03, casa |
| Chi | IU5HJU |
| Versione del firmware | commit `0466536`, artefatto di Actions, ramo `prova-iu5hju` |
| Schede | 2 × LoRa32 T3 V1.6.1 |
| Collegato | solo radio / microfono / altoparlante / tutto |
| Frequenza e parametri | 433,900 MHz, SF8, BW250, CR 4/5, 10 dBm, parità on |
| TOT | 30 s |
| Antenne | stilo originale, SMA |
| Distanza e ambiente | 5 m, stessa stanza |
| Risultato | cosa si è visto e sentito |
| Problemi | messaggi dell'OLED, righe della seriale |

## Scaletta consigliata

Ogni livello presuppone che il precedente funzioni.

1. **Avvio.** L'OLED mostra "AVVIO" e poi "ASCOLTO" con nominativo e frequenza. Con N0CALL mostra "SOLO RX".
2. **Solo radio.** Premi il PTT su una scheda (anche senza microfono): l'altra passa a "RX" con il nominativo, l'**RSSI** (dBm) e l'**SNR** (dB) dell'ultimo pacchetto ricevuto.
3. **Microfono.** Parla con il PTT premuto: sull'altra scheda deve arrivare audio (anche con l'altoparlante non ancora collegato non si rompe nulla; si guardano le statistiche "persi" e "recup." a fine ricezione).
4. **Altoparlante.** Controlla volume, bip di fine trasmissione e di fine ricezione, assenza di fruscio a riposo.
5. **Voce a turni.** Turno di qualche secondo, attesa di 5-10 s, risposta. Qualità e comprensibilità.
6. **Portata.** Allontana le schede a passi, annotando distanza, RSSI, SNR e comprensibilità. Con SF8 il pacchetto si demodula fino a circa -10 dB di SNR (valore teorico): più ci si avvicina a quel limite, più i frame persi aumentano.

## Aiuto per la diagnosi

- Seriale della scheda: `pio device monitor` a 115200 baud, oppure un qualunque terminale seriale.
  A fine di ogni ricezione stampa una riga pronta per il registro, per esempio
  `RX fine: IU5HJU RSSI -92 dBm SNR 7.5 dB, frame persi 0, pacchetti recuperati 1`.
- **Riavvio continuo con `A stack overflow in task loopTask has been detected` sulla seriale:** lo stack del task principale è troppo piccolo. Corretto portandolo a 32 kB (3 novembre 2026); se ricompare, indica il punto in cui si ferma con le righe `[avvio]`.
- **"ERRORE" sull'OLED:** il nome indica il blocco (Codec2, audio I2S, radio); il codice è nella seriale.
- **Si sente solo silenzio:** prova `LCR_MIC_SLOT 1` in `src/user_config.h`.
- **Volume dimezzato o basso:** verifica il pin `SD` dell'amplificatore e l'alimentazione (5 V o 3,3 V).
- **"CANALE OCCUPATO" a canale libero:** il rilevamento del canale ha rilevato attività; annota quando e dove.
- **"STAI RICEVENDO":** la scheda sta ancora riproducendo una trasmissione; è normale per qualche secondo.

## Registro

### Modello da copiare

```
### AAAA-MM-GG, luogo, chi
- Firmware: (commit o artefatto)
- Schede e collegamenti:
- Parametri radio (frequenza, SF, BW, CR, potenza, parità, TOT):
- Antenne:
- Distanza e ambiente:
- Livello raggiunto della scaletta (1-6):
- Risultato (cosa si vede e si sente, RSSI, SNR, "persi" e "recup."):
- Problemi (OLED, seriale):
- Da fare / idee:
```

### Prove effettuate

*(nessuna per ora: la prima è in programma)*
