# Registra e poi trasmetti (prova di diagnosi)

Normalmente il PTT trasmette **in tempo reale**: mentre parli la scheda codifica la voce e la manda in radio a pacchetti. Microfono, Codec2 e radio lavorano quindi **insieme** sullo stesso processore. Se la voce arriva disturbata o a scatti, la domanda è: colpa dell'elaborazione contemporanea, della radiofrequenza che rientra, o di altro?

Il modo **registra e poi trasmetti** separa le due cose:

1. Premi il PTT e parla (fino a 30 secondi). La scheda **registra e codifica** il messaggio in memoria, **senza trasmettere**. Display: `REGISTRA`.
2. Rilasci il PTT. Display: `ATTESA` per **1 secondo**; il microfono è già spento.
3. La scheda **trasmette il messaggio** in radio, senza altro da fare. Display: `INVIO`. Poi torna in ascolto.

Il protocollo è identico: l'altra scheda riceve come sempre, con gli stessi pacchetti e la stessa parità.

## Come si attiva

Solo dalla **seriale** (115200, per esempio dal registro della pagina di configurazione o da un terminale): `CFG REC ON` e `CFG REC OFF`. I pulsanti nella pagina web sono stati tolti per semplificarla (restano nella cronologia di git). Non è salvato: dopo ogni riavvio il modo è spento. Con la prova di collegamento attiva ha la precedenza la prova.

Serve il tuo nominativo (non N0CALL) e l'antenna collegata, come ogni trasmissione.

## Cosa leggere nel registro seriale

```
[rec] registrazione...
[rec] registrati 87 frame (3.5 s), frame persi dal microfono 0; si trasmette tra 1 s
[rec] trasmissione del messaggio
[rec] trasmissione finita
```

Sul ricevitore restano le solite righe `[rx]`, `[rx-audio]`, `[rx-tempi]` e `[rx-codec]`.

## Come interpretare il risultato

Fai la stessa frase **due volte**, con il modo spento e con il modo acceso, e confronta l'ascolto sull'altra scheda:

- **Con registra e trasmetti la voce è chiara, in tempo reale no:** la causa è l'elaborazione contemporanea (microfono, codifica e radio sullo stesso processore). La cura è software: distribuire il lavoro tra i due core o anticipare la codifica.
- **Disturbata in entrambi i casi:** non è la contemporaneità in trasmissione. Restano la radiofrequenza che rientra nell'amplificatore o nell'altoparlante del **ricevitore**, il Codec2 stesso o la riproduzione.
- **Chiara in entrambi i casi:** il problema era altrove (per esempio il microfono toccato o soffiato durante la prova).

Il modo è pensato per le prove; se serve può restare come funzione normale, con l'unico costo di 1 secondo più il tempo del messaggio di ritardo.
