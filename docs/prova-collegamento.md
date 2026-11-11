# Prova di collegamento

Serve a sapere con certezza se **radio e protocollo** funzionano, senza microfono, altoparlante né codec, e a separare
i dubbi: se la prova è perfetta e la voce è brutta, il problema è nell'audio, non nella radio.

## Perché non si usano toni

Codec2 1200 è un codificatore di voce: modella il parlato e non conserva un tono puro. Provato sul PC con la libreria del firmware
(sinusoidi a -12 dBFS): 200 Hz esce a 330 Hz e 17 dB più basso, 440 Hz a 600 Hz e 26 dB più basso, 1000 Hz a 33 dB più basso, 2000 Hz a 53 dB più basso.
Misurare frequenza e ampiezza dopo il decodificatore darebbe falsi allarmi sul codec, quindi la prova usa frame **noti** e li confronta nel dominio dei bit.

## Come funziona

1. Il trasmettitore, con la prova attiva, a ogni PTT invia **300 frame (12 s)** non di voce ma pseudocasuali, calcolati da (sessione, numero del frame).
   Usa gli stessi pacchetti, la stessa parità XOR e gli stessi tempi della voce (modo 2 nell'intestazione). Il microfono non serve.
2. Il ricevitore riconosce la prova dal modo del pacchetto, rigenera la sequenza attesa e confronta ogni frame. **Non riproduce nulla** (niente rumore nell'altoparlante).
   Alla fine mostra e stampa: frame arrivati identici, **danneggiati**, **persi** (non recuperabili), pacchetti **recuperati dalla parità**, RSSI e SNR medi.
3. Se anche sul ricevitore la prova è attiva, dopo circa 0,7 s questo **trasmette un pacchetto di risposta** (modo 3) con lo stesso risultato; il trasmettitore lo mostra.
   Così chi si sposta per misurare la portata vede il risultato sulla propria scheda. Senza la prova attiva sul ricevitore non c'è nessuna risposta, ma la misura si fa comunque.

## Come si usa

1. Su **entrambe** le schede attiva la prova in uno di questi tre modi: **tre pressioni veloci del PTT** (due bip acuti e avviso "PROVA ATTIVA"; altre tre la spengono con un bip grave), `CFG TEST ON` dalla seriale, oppure il pulsante "Attiva la prova" nella [pagina web](https://iu5hju.github.io/VoLoRa/). Sull'OLED a riposo compare "PROVA ON".
   La prova **non è salvata**: all'accensione è sempre spenta, così nessuna scheda resta in prova per errore.
2. Premi il PTT su una scheda (serve un nominativo vero: con N0CALL il PTT è bloccato). Dura 12 s, o meno se rilasci prima.
3. Sul ricevitore compare la schermata "PROVA 99%" per 15 s, con la riga sulla seriale:
   `PROVA ricevuta da IU5HJU: ok 296/300 (99%), danneggiati 1, persi 3, pacchetti recuperati 5, RSSI -92 dBm, SNR 6 dB`.
   Sul trasmettitore, se risponde, la stessa schermata con "Risposta" e la riga `PROVA risposta da IZ5ABC: ...`. Due bip acuti = nessuna perdita tra i frame arrivati, un bip grave = c'è stata qualche perdita.

## Tre clic sul PTT

Una pressione del PTT più breve di **0,3 s** è un "clic" e non trasmette: la trasmissione parte solo se il PTT resta premuto oltre 0,3 s (il microfono è comunque bufferizzato dalla pressione, quindi non si perde l'inizio della frase;
il nominativo parte con 0,3 s di ritardo). Tre clic, ciascuno entro 0,5 s dal precedente, attivano o disattivano la prova. Funziona anche con N0CALL (si riceve e si misura, ma non si risponde). Con una scheda che sta ricevendo i clic sono ignorati.
Una terza pressione subito dopo due clic è sempre il terzo clic, anche se tenuta premuta: per trasmettere dopo due clic basta aspettare mezzo secondo.

## Prova parziale

La percentuale è calcolata sui frame **arrivati**, non sui 300 previsti: una prova interrotta con il PTT dopo 7 frame, tutti perfetti, vale 100%. La scheda la segnala come **parziale**
(sull'OLED "Parz.7/300", in seriale `PARZIALE (prova interrotta o coda persa)`). Il ricevitore non può distinguere un PTT rilasciato presto dalla perdita degli ultimi pacchetti:
per misurare la portata tieni premuto il PTT fino in fondo (12 s) e cerca "ok 300/300".

## Come leggere l'esito

| Esito | Significato |
|---|---|
| 100%, danneggiati 0, persi 0 | radio, protocollo e parità funzionano nelle condizioni della prova |
| persi > 0, danneggiati 0 | pacchetti non arrivati (distanza, antenna, interferenze, ricevitore troppo lento). "Recuperati" dice quanto ha salvato la parità |
| danneggiati > 0 | frame arrivati ma diversi: non dovrebbe succedere con il CRC della radio; segnalalo come errore |
| ricevuta ma nessuna risposta | il ricevitore non ha la prova attiva, il canale era occupato, o la risposta non è tornata |

## Limiti

- **Non prova il codec né l'audio**: solo radio e protocollo. Autotest del codec e prova dell'altoparlante sono i livelli successivi, con l'hardware audio.
- Le due schede devono avere la **stessa versione** del firmware: una scheda più vecchia scarterebbe i pacchetti di prova (modo sconosciuto).
- La prova è una trasmissione: il nominativo viaggia in chiaro, ci si attiene a frequenza e potenza consentite.
- Misura il collegamento in **un verso** per volta (chi trasmette -> chi riceve). La risposta, che viaggia nel verso opposto, è un solo pacchetto breve: se arriva, il percorso inverso funziona; se non arriva, non si può dire quale dei due pacchetti sia mancato.
