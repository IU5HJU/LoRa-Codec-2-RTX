# T-Beam V1.2 senza display

Variante del firmware per la **LILYGO T-Beam V1.2** (alimentazione **AXP2101**, GPS **u-blox NEO-M8**) **senza il display OLED**. Si seleziona nella pagina di installazione: **"LILYGO T-Beam V1.2 (AXP2101, GPS NEO-M8) senza display"**. Ambiente PlatformIO: `tbeam_nodisplay` (`pio run -e tbeam_nodisplay`); artefatto: `firmware-tbeam-v12-senza-display`.

Funzionalmente è identica alla versione con display: stesso protocollo, stessa pagina di configurazione (`CFG ...` sulla seriale), stesso modo "registra e poi trasmetti". Cambia solo l'interfaccia.

## Cosa cambia

- **Display:** non viene mai toccato. Il bus I2C resta usato solo dall'AXP2101 (con il display l'I2C costa circa 25 ms ogni ridisegno: senza, quel tempo resta all'audio).
- **Stato sul LED di bordo** (blu, GPIO4, acceso a livello basso), più la seriale (115200) e i bip.
- **GPS:** il NEO-M8 resta **spento** (canale ALDO3 dell'AXP2101): il firmware non lo usa e così non consuma batteria. Se il modulo ha il connettore micro (u.FL) per l'antenna GPS, **non serve collegarlo** per far funzionare la radio.
- **Radio:** l'antenna da 433 MHz va collegata **prima** di alimentare la scheda e di trasmettere, come per ogni scheda (una trasmissione senza antenna può danneggiare lo stadio finale).

## Significato del LED

| Il LED | Significa |
|---|---|
| 3 lampi all'accensione | scheda pronta |
| lampeggio continuo veloce all'accensione | errore all'avvio (leggi il messaggio sulla seriale) |
| 1 lampo ogni 2 secondi | in ascolto, tutto a posto |
| 2 lampi ogni 2 secondi | in ascolto con nominativo **N0CALL**: solo ricezione, non trasmette. Imposta il nominativo dalla pagina |
| 3 lampi ogni 2 secondi | prova di collegamento attiva |
| acceso fisso | sta ricevendo una trasmissione |
| lampeggio veloce (5 volte al secondo) | sta trasmettendo |
| lampeggio lento (1 volta al secondo) | modo registra e trasmetti: sta registrando |
| spento per 1 secondo | modo registra e trasmetti: attesa prima della trasmissione |

Il PTT è il tasto utente **IO38** di bordo (come nella versione con display); tre pressioni veloci attivano o disattivano la prova di collegamento, come descritto in [prova-collegamento.md](prova-collegamento.md).

## Stato

Compila su GitHub Actions. La variante con display è provata sulla scheda per radio, AXP2101 e PTT; **questa variante senza display non è ancora stata provata sulla scheda**, né l'audio (microfono, amplificatore, altoparlante) sulla T-Beam. Cablaggio dell'audio: vedi [pinout.md](pinout.md).
