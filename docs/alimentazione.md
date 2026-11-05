# Alimentazione e batteria della T-Beam

Confronto tra T-Beam V1.0/1.1 (chip **AXP192**) e V1.2 (chip **AXP2101**), ricavato leggendo il
firmware LoRa APRS iGate di Ricardo Guzman (CA2RXU, `src/power_utils.cpp`, versione V4.0.4 del 14 ottobre 2026)
e il codice di riferimento LilyGO. Non sono prove su hardware: quello che segue è lettura del codice.

## Differenze nella gestione dell'alimentazione

| Aspetto | T-Beam V1.0/1.1 (AXP192) | T-Beam V1.2 (AXP2101) |
|---|---|---|
| Radio LoRa | LDO2 a 3,3 V | **ALDO2** a 3,3 V |
| GPS | LDO3 a 3,3 V | **ALDO3** a 3,3 V |
| Canali spenti all'avvio | DC2, LDO2, LDO3 (poi accende quelli che servono) | DC2-5, ALDO1, ALDO4, BLDO1-2, DLDO1-2 |
| Tensione principale | DC1 a 3,3 V acceso | DC1 a 3,3 V acceso (alimenta l'ESP32) |
| Canale protetto (mai da spegnere) | DCDC3 | DCDC1 |
| Pila a bottone (backup GPS/RTC) | non prevista | carica a 3,3 V attivata (`enableButtonBatteryCharge`) |
| Carica | terminazione sotto il 10 %, 4,2 V, 780 mA | **precarica 200 mA**, terminazione 25 mA, 4,2 V, 800 mA |
| Soglia di spegnimento del sistema (VOFF) | 2600 mV | 2600 mV (impostabile da 2600 a 3300 mV) |
| Misure | sensore di temperatura (TS) spento, rilevamento batteria acceso, misure VBUS/VBAT/VSYS | uguale |
| Interrupt | tutti disattivati | tutti disattivati |

Anche il **tasto PWR** funziona in modo diverso dal tasto utente: con il sistema spento si accende premendo PWR, con il sistema
acceso una pressione lunga (6 secondi di default) lo spegne.

## Cosa fa il nostro firmware sulla T-Beam V1.2

Segue la sequenza del codice LilyGO: DC1 protetto, canali non usati spenti, radio su ALDO2 a 3,3 V, GPS spento, carica 500 mA a 4,2 V, misure di
tensione attive. **Differenze rispetto a quello di Guzman**, tutte da valutare:

- non imposta la soglia di spegnimento VOFF (la lascia come la trova: ora la stampa all'avvio);
- non attiva esplicitamente il rilevamento batteria (`enableBattDetection`), anche se `isBatteryConnect()` risponde correttamente;
- non imposta DC1 a 3,3 V (si fida del valore già presente);
- non carica la pila a bottone;
- carica a 500 mA invece di 800 mA e non imposta la corrente di precarica.

## Perché la T-Beam V1.2 potrebbe non accendersi a batteria

Dal log del 4 novembre: **batteria presente, 1837 mV, 0 %, carica in corso.** Una cella Li-ion sana sta tra circa 3,0 e 4,2 V.
Cause, dalla più probabile:

1. **Cella troppo scarica.** Il PMU spegne il sistema quando la tensione scende sotto la soglia VOFF (impostabile fra 2,6 e 3,3 V). Con 1,84 V
   la scheda non può funzionare a batteria, e parte solo con l'USB. Un esperimento precedente che l'ha lasciata accesa a batteria (radio e GPS
   attivi, oppure un firmware sbagliato) può averla scaricata.
2. **PMU in stato di spegnimento.** Dopo uno spegnimento (pressione lunga di PWR, o soglia raggiunta) a batteria va premuto **PWR**; con l'USB parte da sola.
3. **Configurazione rimasta nei registri del PMU** da un firmware precedente (per esempio una soglia VOFF alta): il PMU conserva i registri finché ha
   alimentazione. Dal nuovo log `[pmu] soglia di spegnimento VOFF ...` si legge il valore attuale.
4. **Contatti o polarità della cella**, o una cella con protezione che scatta.

### Sicurezza

Una cella Li-ion a 1,8 V è **profondamente scarica**. Sotto circa 2,5 V non va ricaricata normalmente (rischio di danni e, in casi estremi, di
surriscaldamento): meglio **sostituirla** con una cella nuova, di qualità, preferibilmente con protezione. Non provare a "rianimarla".

### Controlli da fare

1. Estrai la cella e misura la tensione con un tester. Con meno di circa 2,5 V, sostituiscila.
2. Con una cella buona (almeno 3,3-3,7 V) premi **PWR** e verifica se la scheda parte a batteria.
3. Carica il firmware aggiornato e leggi in seriale le righe `[pmu]`: con l'USB collegata ogni 10 secondi stampa batteria, tensione, percentuale e stato di carica.

## Indicazione sul display

La tensione della batteria compare in basso a destra sull'OLED come `(Batt=4.12V)`, sotto 3,40 V in negativo; vedi [display.md](display.md).
Sulla T-Beam il valore viene dall'AXP2101, sulla T3 dall'ADC di GPIO35.

**T3 senza batteria (5 novembre):** con la scheda alimentata solo dall'USB e nessuna LiPo, l'ADC legge la tensione del caricatore: 4,24-4,42 V, che oscilla. Una LiPo non supera 4,2 V (4,35 V le celle ad alta tensione), quindi sopra 4,35 V il firmware mostra `(Batt=--)`. Sotto quel limite la lettura resta mostrata anche senza batteria (non esiste un modo affidabile per distinguere): per un valore vero serve la batteria collegata e il confronto col tester.
