# Studio: il reflector VoLoRa (preso da quello YSF)

13/11/2026. Scopo: un **reflector** a cui si collegano **nodi e ripetitori** VoLoRa e che porta i pacchetti radio da uno a tutti gli altri, **senza aprire porte sul router** di chi si collega. Parte dall'esame del reflector YSF (System Fusion) di G4KLX. Stato: **fatto sul PC** (passi 1-4 sotto); come si usa è in [reflector.md](reflector.md).

## Come funziona il reflector YSF (letto dal sorgente)

Fonte: `YSFReflector.cpp` e `Network.cpp` di G4KLX, nella copia [wdatkinson/YSFClients](https://github.com/wdatkinson/YSFClients/tree/master/YSFReflector) (il repository originale è [g4klx/YSFClients](https://github.com/g4klx/YSFClients)). I numeri sotto sono quelli del codice, non una mia stima.

- **UDP, un solo socket sul server.** I ripetitori e i gateway (i *client*) **iniziano sempre loro**: mandano un datagramma al server e il server risponde all'**indirizzo e porta da cui è arrivato**. Per questo i client **non aprono nessuna porta** sul router: il NAT crea da solo la corrispondenza quando il client scrive per primo.
- **Registrazione = il poll.** Un datagramma `YSFP` + nominativo (10 caratteri): se il server non conosce quella coppia (indirizzo, porta) la aggiunge all'elenco, avvia un timer da **60 s** e risponde con `YSFPREFLECTOR ` (14 byte). Il client si identifica quindi **solo con l'indirizzo e la porta di origine**: se il NAT cambia la porta, per il server è un altro client.
- **Poll in due sensi ogni 5 s.** Il server manda `YSFPREFLECTOR ` a tutti i client ogni **5 s** (tiene aperto il NAT e dice che c'è); chi non si fa sentire per 60 s viene tolto. Il client risponde con i suoi poll.
- **Dati.** `YSFD` è un datagramma da **155 byte** (all'inizio 4 byte di tipo, poi 10 caratteri del ripetitore che ha trasmesso, 10 del mittente, 10 del destinatario; il bit 0 del byte 34 segna **fine trasmissione**). Il server lo **inoltra identico a tutti i client tranne quello da cui arriva**.
- **Scollegamento.** `YSFU`: il server toglie subito il client. **Stato.** `YSFS`: risposta di 42 byte con un numero, nome, descrizione e conteggio dei client collegati.
- **Chi parla.** Il reflector G4KLX tiene un solo "watchdog di rete" da **1,5 s** che serve per i messaggi di log: **inoltra comunque tutto**. Chi parla per primo è deciso da implementazioni successive (per esempio [pYSFReflector](https://github.com/dg9vh/pYSFReflector): "first-come-first-served", la seconda trasmissione contemporanea è silenziata; elenchi di nominativi e gateway bloccati).
- **Nessuna autenticazione.** Chiunque conosca l'indirizzo manda un poll e entra; un datagramma da un indirizzo qualsiasi è accettato.

## Cosa prendiamo, cosa cambiamo

| YSF | VoLoRa |
|---|---|
| Il client scrive per primo, il server risponde da dove è arrivato: **nessuna porta aperta** | **Uguale.** Il collegamento VoLoRa-IP ([studio-nodo-internet.md](studio-nodo-internet.md)) è già così: il nodo manda `HELLO` al server e il server risponde all'indirizzo di origine. Il nodo ascolta sulla propria porta (5400) ma gli arriva solo quello che è partito da lui |
| Un client è (indirizzo, porta) | Un client è il suo **id** (4 byte, nell'intestazione del datagramma) con la sua **chiave**; l'indirizzo e la porta si **aggiornano** a ogni datagramma firmato valido (cambia il NAT, cambia l'indirizzo, resta lo stesso client) |
| Nessuna autenticazione | **Firma HMAC-SHA256 per client** (chiave diversa per ogni nodo o ripetitore) e sfida dopo un riavvio, come fra due nodi. Un client si può togliere cambiando la sua riga nel file |
| Poll del server ogni 5 s a tutti | Il server manda un `KEEPALIVE` ogni 20 s ai client **collegati** (come i nodi): tiene aperto il NAT, che di solito scade dopo 30-120 s. Un client senza datagrammi validi per 60 s è tolto |
| Inoltra tutto a tutti | Inoltra **voce del modo del reflector** (di base il modo 4, Standard) e **testo**; scarta il resto. Chi parla per primo tiene il canale finché non finisce (pacchetto di fine, o 3 s senza pacchetti): le altre voci in quel momento si scartano (contate). Il testo passa sempre |
| Un solo "salone" | Un reflector = un salone, come YSF. I **canali** (campo `canale` nell'intestazione, già previsto) servono ai passi successivi |
| Stato `YSFS` aperto | Per ora solo il registro del server (client collegati, chi parla). Niente risposte a chi non ha la chiave, così il reflector **non si può usare per amplificare** traffico verso un indirizzo falso |

## Cosa serve a chi si collega

Niente di nuovo sul router: **solo il server** ha bisogno di un indirizzo raggiungibile (una porta UDP aperta, di base 5400, in ingresso). Il nodo, o il ripetitore, imposta `PEER` con **il nome o l'indirizzo del reflector** e la sua porta, `PEERID` con l'id del reflector e `NODEKEY` con la **propria chiave** (la riga del file del reflector). Un nome (per esempio `reflector.example.org`) si risolve all'avvio e di nuovo ogni 5 minuti e a ogni collegamento caduto: così funziona anche se il reflector cambia indirizzo (DDNS).

## Ripetitore collegato in rete

Il ripetitore ([ripetitore.md](ripetitore.md)) è fatto da due schede. Il collegamento di rete sta sulla **scheda RX** (la TX non cambia, e non ha il Wi-Fi):
- **radio → rete:** quello che la RX ha deciso di ripetere (le regole di `Repeater`) va, oltre che alla TX per seriale, anche al reflector;
- **rete → radio:** i pacchetti che arrivano dal reflector passano per la **stessa seriale** alla TX, che li trasmette sulla frequenza di uscita, con il flag `RELAYED`, come un pacchetto della RX.
Le impostazioni sono quelle del nodo: `WIFISSID`, `WIFIPASS`, `PEER`, `PEERID`, `NODEID`, `NODEKEY` sulla scheda RX. Se mancano, il ripetitore funziona come prima, solo in radio.

## Passi

1. Questo studio.
2. **Fatto:** il reflector (`lib/voicecore/reflector_core.*`, provato sul PC con client finti) e il programma per il server `tools/vlreflector.cpp` (`make vlreflector`), con una prova con veri socket UDP dove i client non hanno nessuna porta nota.
3. **Fatto:** il **nome del reflector al posto dell'indirizzo** nelle impostazioni (`PEER`) e nel nodo.
4. **Fatto:** il **ripetitore in rete** (scheda RX, `lib/voicecore/repeater_uplink.*`, `src/net_udp.*`). Per il nodo e per il ripetitore un solo socket serve a ricevere e a spedire: con il reflector la risposta torna all'indirizzo e alla porta messi dal NAT, e un socket di spedizione diverso da quello di ricezione non la riceverebbe mai.
5. Più avanti: canali (saloni), identificativo del reflector, ponti verso SvxLink e YSF ([studio-collegamento-internet.md](studio-collegamento-internet.md)).
