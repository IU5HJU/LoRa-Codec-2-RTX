# Studio di fattibilità: ripetitore LoRa con due schede

Data: 10/11/2026. Stato: **studio, nessun codice scritto**. I numeri di tempo in aria sono calcolati con la formula di Semtech, non misurati. Premesse e decisioni già prese sul ripetitore: [roadmap.md](roadmap.md) ("Chi ripete cosa").

## Stato (10/11/2026)

**Scelta l'opzione A** (due schede, un solo profilo: il Codec2 1200 Standard). **Fase 1 scritta e provata sul PC:** flag `RELAYED` (bit 7) nel protocollo e logica di decisione del ripetitore (`lib/voicecore/repeater.*`, test `tests/test_repeater.cpp`: flag, anelli, duplicati, filtro dei modi, durata della sessione e catena completa con un pacchetto perso e ricostruito dalla parità). **Fase 2, firmware delle due schede, scritta il 10/11/2026** (`src/repeater_node.*`, `lib/voicecore/repeater_link.*`, impostazioni `ROLE` e `FREQOUT`; seriale fra le schede con CRC e coda di trasmissione provate sul PC; il firmware delle schede compila solo su Actions): [firmware.md](firmware.md#ripetitore-a-due-schede-ruoli-rx-e-tx-10112026). **Ancora da fare:** prove sul banco, comandi del sysop (modo 5), identificazione periodica.

## Obiettivo

Un ripetitore fatto da **due schede**: una sempre in **ricezione** (frequenza di ingresso) e una sempre in **trasmissione** (frequenza di uscita), che lavorano **nello stesso momento**. Il vincolo da superare: ripetere **qualsiasi modo** del progetto (Standard, Messaggio, Qualità), cosa che la roadmap dell'8/11/2026 escludeva per la Qualità ("solo collegamento diretto"). Qui si valuta se e come si può.

## Architettura base (due schede)

- **Scheda RX** (frequenza di ingresso) e **scheda TX** (frequenza di uscita), collegate da una **seriale a 3,3 V** con la massa in comune. Il ripetitore **non ha audio**: i pin di microfono e amplificatore sono liberi per la seriale.
- **Ripetizione a livello di pacchetto**, senza decodificare la voce: la scheda RX riceve un pacchetto con CRC valido, controlla la **versione del protocollo**, lo passa alla scheda TX, che lo ritrasmette quasi identico. Un ripetitore che non guarda il contenuto **ripete anche i modi futuri**: l'unico requisito è il formato del pacchetto.
- **Rigenerazione:** la RX può **recuperare i pacchetti persi con la parità** (la logica `RxStream` esiste già) e la TX ritrasmettere il pacchetto ricostruito. Ogni tratta ha così la sua correzione degli errori. La parità originale si ritrasmette com'è.
- **Ritardo:** store-and-forward per pacchetto: la TX parte quando il pacchetto è arrivato intero, quindi il ritardo è **un pacchetto in aria** (circa 0,13 s in Standard, 0,16 s in Qualità a SF7) più il passaggio sulla seriale (pochi ms). Il pre-buffer di 1 s delle stazioni lo assorbe senza problemi.
- **Anello infinito:** serve un **flag "ripetuto"** nel campo delle bandiere (oggi i bit 0-6 sono in uso, il **bit 7 è libero**) e una **memoria degli ultimi pacchetti** (sessione e sequenza) per non ripetere due volte lo stesso. Le schede con il firmware vecchio scartano i pacchetti con bandiere sconosciute: non sentirebbero le copie ripetute (silenzio, nessun rumore).
- **Identificazione:** il ripetitore deve trasmettere il **proprio nominativo** a intervalli (un pacchetto di tipo nuovo o il nominativo nei pacchetti ripetuti). Le regole esatte dipendono dalla verifica normativa.

## Il limite vero: "qualsiasi modo" non vuol dire "qualsiasi radio"

Il **modo** (campo del pacchetto: 1200, 3200, prova, messaggio) e i **parametri radio** (SF e banda) sono due cose diverse, e il profilo li lega:

| Profilo | SF / banda | Codec2 |
|---|---|---|
| Standard | SF8 / 250 kHz | 1200 |
| Messaggio | SF12 / 125 kHz | 1200 |
| Qualità | SF7 / 250 kHz | 3200 |

Una SX127x **demodula un solo SF e una sola banda alla volta**. Non ascolta tre profili insieme (solo i chip da gateway a 8 canali lo fanno, e non sono sulle nostre schede). Quindi con due schede un ripetitore ripete **tutto ciò che viaggia con una sola configurazione radio**. Le strade:

| Opzione | Schede | Cosa ripete | Pro | Contro |
|---|---|---|---|---|
| **A. Un profilo alla volta** | 2 | tutti i modi di **quel** profilo; il profilo si sceglie nelle impostazioni | semplice, senza cambiare la radio delle stazioni | gli altri profili non sono ripetuti |
| **B. Canale ripetitore "universale"** a SF7 / 250 kHz | 2 | **tutti i modi**: voce 1200, Qualità 3200, prove e messaggi (come pacchetti normali) | un solo canale, un solo RX e un solo TX | le stazioni parlano al ripetitore con SF7 (circa -2,5 dB rispetto a SF8; il Messaggio perde il guadagno di SF12 su quel tratto); serve un modo "ripetitore" nel firmware delle stazioni |
| **C. Multi-profilo** | 4 o più | tutti i profili **con i propri parametri**: tre schede RX (SF8, SF7, SF12/125) sulla stessa frequenza di ingresso e una o due schede TX | nessuna rinuncia per le stazioni | costo, un solo TX con SF12 resta occupato 3,4 s a pacchetto e ritarda la voce in tempo reale |
| **D. Scansione degli SF con CAD** | 2 | in teoria tutti i profili con una scheda RX | poche schede | il preambolo di 12 simboli dura solo 6 ms a SF7: la scansione dei tre SF lo perderebbe; richiede preambolo più lungo e non è provata: **solo ricerca** |

**Il parametro che decide l'opzione B** è l'occupazione del canale (tempo in aria rispetto al tempo reale della voce, senza e con le pause da 80 ms fra i pacchetti):

| Codec2 | SF7 / 250 kHz | SF8 / 250 kHz | SF9 / 250 kHz |
|---|---|---|---|
| 1200 | 19% / 40% | 33% / 54% | 61% / 82% |
| 3200 | **41% / 62%** | 73% / **94%** | 131% / 152% |

- **A SF7 reggono tutti i modi** (la Qualità al 62%).
- **A SF8 la Qualità è al 94%** con le pause: non regge con margine. Un canale universale a SF8 non va bene.
- Un **messaggio** (registrato) trasmesso a SF7 occupa circa 12,5 s per 30 s di voce: nessun problema.

## Separazione fisica

**Gestita dall'operatore** (esperienza con filtri e cavità): non rientra in questo studio. Si assume che la separazione fra l'ingresso e l'uscita sia risolta in modo adeguato.

## Modifiche al firmware (da fare solo se lo studio passa)

- **Ripetitore:** nuovo ruolo (RX o TX) senza audio, senza menu, con seriale fra le due schede, coda dei pacchetti, memoria degli ultimi pacchetti, identificazione.
- **Stazioni:** **frequenza di ingresso e di uscita separate** (oggi una per profilo, già prevista nella roadmap), controllo "canale occupato" sulla frequenza di uscita, riconoscimento del flag "ripetuto", eventuale profilo "ripetitore" (opzione B).
- **Protocollo:** flag "ripetuto" (bit 7), eventuale pacchetto di identificazione. Un cambio dei bit noti è compatibile solo con il firmware nuovo.

## Aspetti normativi

Il ripetitore è una **stazione automatica**: l'autorizzazione dei ripetitori viene dal **Ministero** (non dall'ARI, che non norma i ripetitori). Questo sarebbe il **primo ripetitore LoRa in Italia**, quindi è un'attività **puramente sperimentale**, svolta da radioamatori nell'ambito della sperimentazione che la licenza consente. L'**operatore gestisce già un ripetitore autorizzato** e ne conosce le regole: lo studio non le ripete e le scelte (frequenze, potenza, identificazione, controllo remoto) restano **sotto la responsabilità dell'operatore**.

## Verdetto

- **Fattibile con due schede**: ripetizione a livello di pacchetto, rigenerata con la parità, un solo profilo o il canale universale a SF7 (opzioni A e B).
- **Ripetere "qualsiasi modo" con due schede si può solo con il canale universale** (opzione B, SF7): niente cambia per il codec, cambia il profilo radio. Per **ripetere ogni profilo con i suoi parametri** servono più schede (opzione C).
- **Ordine consigliato:** (1) ripetitore di pacchetti con un profilo (A) in modalità di prova, **senza trasmettere sull'aria libera**; (2) valutare B; (3) C solo se serve; (4) prove sul campo, a cura dell'operatore, nei limiti della sperimentazione.

## Da decidere

1. Opzione A o B come primo passo (B implica un modo "ripetitore" anche nelle stazioni).
2. Frequenze di ingresso e di uscita e le condizioni della sperimentazione (frequenze, potenza, identificazione), che decide l'operatore.
3. Quante schede e che hardware per il ripetitore (alimentazione fissa, antenne, filtri).

## Variante: ripetitore del solo modo 1200 con i comandi (idea del 10/11/2026)

La variante più semplice, in linea con la decisione dell'8 novembre ("ripetitore Standard"): ripete **solo il modo 1 (Codec2 1200)** e **consuma** i pacchetti di comando indirizzati a lui. Le **bandiere e i campi liberi** del protocollo (versione 2) permettono di aggiungere i comandi **senza cambiare versione**:

| Dove | Libero | Note |
|---|---|---|
| **Modo** (4 bit bassi del byte 0) | **10 valori: da 5 a 14** (1, 2, 3, 4 e 15 sono usati) | **la via migliore per i comandi**: un modo nuovo, per esempio 5 = comando. I ricevitori con il firmware attuale scartano i modi sconosciuti (nessun disturbo), e non tocca i bit delle bandiere |
| **Bandiere** (byte 4) | **bit 7** (valore 128) | oggi `kFlagsKnown` è 127: un pacchetto con il bit 7 viene scartato. Serve al flag **"ripetuto"** contro gli anelli; conviene **riservarlo** a quello |
| **Sequenza** (byte 3) | **bit 7** (la sequenza vale 0..127) | un solo bit per pacchetto: per esempio "non ripetere" o "emergenza"; oggi un valore da 128 in su viene scartato |
| **Pacchetto START** | **nessun frame audio**, quindi il suo carico è libero | potrebbe portare dati in più, ma un comando a sé è più pulito |

**Pacchetto di comando (modo 5, bozza):** stessa intestazione dei pacchetti di oggi (sessione, sequenza, bandiere), **nominativo di chi comanda** (già previsto), carico con un **codice di comando e i parametri**. Il ripetitore **non lo ripete**; risponde con un pacchetto di stato dello stesso modo.

**Chi può comandare (precisazione del 10/11/2026):** i comandi di gestione del ripetitore servono alla **gestione e allo spegnimento in caso di disturbo**, quindi li può inviare **solo il sysop**. Il ripetitore **ignora** ogni comando che non sia autenticato come proveniente dal sysop: nessun comando senza firma, nemmeno "stato". Le regole di fondo (comandi ammessi per la gestione) sono quelle indicate dall'operatore; le condizioni del controllo remoto le conosce e le decide l'operatore (sperimentazione).

**Comandi del sysop (bozza):**

| Codice | Comando | Effetto |
|---|---|---|
| 1 | STATO | il ripetitore risponde con versione, carico, pacchetti ripetuti, ultimo errore |
| 2 | SPEGNI | la scheda TX smette di trasmettere (e spegne l'amplificatore con un pin, se previsto) finché non arriva RIACCENDI; la scheda RX continua ad ascoltare: **il comando di riaccensione arriva comunque** |
| 3 | RIACCENDI | torna in servizio |
| 4 | SPEGNI PER N MINUTI | spegnimento a tempo, utile contro un disturbo temporaneo |
| 5 | RIAVVIA | riavvio delle due schede |
| 6 | IDENTIFICA | trasmette subito il proprio nominativo |
| 7 | POTENZA | imposta la potenza di uscita, entro i limiti |

**Firma del comando (bozza di struttura):** modo **5**, nominativo del sysop in chiaro (campo già esistente) e un carico di questa forma:

`[codice 1 byte][parametro 0..2 byte][contatore 4 byte][firma 8 byte]`

- La **firma** è un **HMAC** (per esempio HMAC-SHA256 troncato a 8 byte) di tutto il pacchetto con una **chiave condivisa** fra ripetitore e sysop (16 byte casuali, scritti nella memoria delle schede dalla pagina di configurazione, mai trasmessi). **Non cifra nulla**: il comando resta leggibile e la firma serve solo a provare che viene dal sysop.
- Il **contatore** cresce a ogni comando e il ripetitore accetta solo valori **maggiori dell'ultimo** (memorizzato): una registrazione del pacchetto, riascoltata, **non funziona**. Non serve un orologio.
- La scheda del sysop è una stazione normale con un comando in più (da seriale o dalla pagina) che trasmette il pacchetto, senza PTT e senza voce.
- Il ripetitore **risponde sempre** con un pacchetto di stato dello stesso modo (comando eseguito, rifiutato, contatore non valido), così il sysop sa se ha funzionato.

**Sicurezze del ripetitore stesso** (indipendenti dai comandi): **limite di durata di una trasmissione** continua (come il TOT delle stazioni) e spegnimento automatico se la scheda TX resta in trasmissione oltre un tempo massimo, così un guasto o un disturbo continuo non lo blocca in aria.

**Compatibilità:** con i comandi nel modo 5, nessun cambio di versione del protocollo. Con il bit 7 delle bandiere come "ripetuto", le schede già in uso non sentono le copie ripetute (come già scritto sopra): è lo stesso costo dell'unico cambio di formato richiesto dal ripetitore.

## Estensione: la scheda TX come ricetrasmettitore per ripetere i messaggi (idea dell'11/11/2026)

**Idea:** la scheda TX ha una radio che sa anche ricevere. Oltre a ritrasmettere i pacchetti della RX (voce in tempo reale), può **farsi carico del profilo Messaggio**: ricevere un messaggio intero (SF12, banda 125 kHz), memorizzarlo e **ritrasmetterlo** (store-and-forward, come già deciso per il Messaggio: stessi bit del Codec2, nessun degrado). **Mentre ripete un messaggio il ripetitore è in trasmissione e gli utenti trovano il canale occupato**, come nel progetto (una stazione che sta ricevendo non può trasmettere).

**Cosa funziona subito e cosa no:**

| Aspetto | Valutazione |
|---|---|
| Hardware | **nessuna scheda in più**: la TX cambia parametri radio (SF, banda, frequenza) a seconda di cosa fa; RadioLib lo permette in pochi ms |
| Memoria | un messaggio di 30 s sono circa 80 pacchetti da 80 byte = circa 7 kB: sta nella RAM della scheda |
| Recupero dei pacchetti persi | la logica della parità (già provata) ricostruisce i pacchetti persi **prima** della ritrasmissione, quindi ogni tratta è protetta |
| Ritrasmissione | pacchetti identici all'originale con il flag `RELAYED` (bit 7), come per la voce |

**Il punto delicato: una sola radio per tutto.** La TX, mentre ritrasmette un messaggio o ne riceve uno, **non può ritrasmettere la voce in tempo reale**. Il tempo in aria di un messaggio a SF12/BW125 (calcolato, non misurato):

| Voce del messaggio | Tempo in aria per ogni ritrasmissione |
|---|---|
| 5 s | circa 48 s |
| 10 s | circa 94 s (1,6 minuti) |
| 15 s | circa 139 s (2,3 minuti) |
| 30 s | circa **271 s (4,5 minuti)** |

Un messaggio di 30 s tiene il ripetitore occupato **4,5 minuti**, e la stessa durata se ne serve un altro in ricezione. Conviene un **limite di durata per i messaggi ripetuti** (per esempio 10-15 s di voce, circa 1,5-2,3 minuti), da decidere.

**Occupazione e segnale di "canale occupato" (cosa serve davvero):**
- **Gli utenti in Standard o in Qualità (SF8 e SF7) non vedono con il CAD una trasmissione SF12**: il rilevamento del preambolo funziona solo con lo stesso SF. Perciò, **prima** di una ritrasmissione lunga, la TX manda un **breve pacchetto di annuncio** con i parametri dello Standard (nuovo modo, per esempio il **6**: "ripetitore occupato per N secondi") e un altro a fine servizio. Le stazioni lo mostrano sul display e rifiutano il PTT per il tempo indicato. **I modi 5-14 sono ancora liberi** (il 5 è già previsto per i comandi del sysop).
- Gli utenti **in Messaggio** (SF12) sull'uscita ricevono la ritrasmissione stessa e quindi hanno già il PTT rifiutato come per ogni ricezione.
- **La ricezione dei messaggi** (SF12 sulla frequenza di ingresso) la fa la TX **nei tempi in cui è libera**. Mentre la TX riceve un messaggio, **la voce in tempo reale non si può ritrasmettere**. Due regole possibili: (a) **vince il messaggio**: la voce in arrivo mentre si ascolta un messaggio non si ritrasmette (utenti Standard senza segnale di occupato: meno bello); (b) **vince la voce**: se arriva voce, la ricezione del messaggio si interrompe e i pacchetti persi si recuperano con la parità finché possibile. Da scegliere.
- **Un utente che trasmette un messaggio mentre il ripetitore è occupato lo perde**: la TX non ascolta. Gli utenti nel raggio dell'uscita lo sanno perché sentono la ritrasmissione; gli altri no.

**Alternativa senza conflitti: una terza scheda** dedicata alla ricezione dei messaggi (SF12 in ingresso) che li passa alla TX come la RX fa per la voce. Costa una scheda in più ma la voce non perde mai pacchetti per colpa dei messaggi; la TX resta occupata solo mentre ritrasmette. Un ripetitore con **due schede sole** funziona con la regola (a) o (b).

**Modifiche al firmware (da fare solo se si decide):**
- **Radio:** cambiare SF, banda e frequenza a runtime (oggi `RadioLora::begin` imposta tutto una volta).
- **Logica (provabile sul PC):** un archivio di messaggi che accumula i pacchetti, ricostruisce quelli persi con la parità, riconosce la fine (pacchetto `END`, totale nel pacchetto `START`, silenzio) e li restituisce in ordine con il flag `RELAYED`; la regola di priorità fra voce e messaggio; il limite di durata; l'annuncio di occupato.
- **Stazioni:** riconoscere il modo 6 (annuncio di occupato), mostrarlo sul display e rifiutare il PTT per il tempo indicato.

**Da decidere:**
1. **Durata massima** di un messaggio ripetuto (suggerimento: 10-15 s di voce).
2. **Priorità**: vince il messaggio (a) o vince la voce (b), oppure **terza scheda**.
3. **Frequenze**: i messaggi entrano e escono sulle stesse frequenze della voce (cambia solo il profilo radio) o su altre.
4. L'**annuncio di occupato** nel modo 6, come proposto.
