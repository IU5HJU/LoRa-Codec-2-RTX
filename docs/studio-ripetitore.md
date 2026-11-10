# Studio di fattibilità: ripetitore LoRa con due schede

Data: 10/11/2026. Stato: **studio, nessun codice scritto**. I numeri di tempo in aria sono calcolati con la formula di Semtech, non misurati. Premesse e decisioni già prese sul ripetitore: [roadmap.md](roadmap.md) ("Chi ripete cosa").

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

## Aspetti normativi (da chiarire prima di trasmettere)

Il ripetitore è una **stazione automatica**: serve la **verifica normativa con l'ARI** già prevista in roadmap (autorizzazione, frequenze di ingresso e di uscita secondo il piano di banda IARU Regione 1, potenza, identificazione, eventuale controllo da remoto). Nessuna prova su aria prima di questo.

## Verdetto

- **Fattibile con due schede**: ripetizione a livello di pacchetto, rigenerata con la parità, un solo profilo o il canale universale a SF7 (opzioni A e B).
- **Ripetere "qualsiasi modo" con due schede si può solo con il canale universale** (opzione B, SF7): niente cambia per il codec, cambia il profilo radio. Per **ripetere ogni profilo con i suoi parametri** servono più schede (opzione C).
- **Ordine consigliato:** (1) ripetitore di pacchetti con un profilo (A) in modalità di prova, **senza trasmettere sull'aria libera**; (2) valutare B; (3) C solo se serve; (4) solo dopo la verifica ARI, prove sul campo.

## Da decidere

1. Opzione A o B come primo passo (B implica un modo "ripetitore" anche nelle stazioni).
2. Frequenze di ingresso e di uscita e la procedura con l'ARI.
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

**Comandi possibili** (da scegliere, nessuno è deciso):
- **Dalla stazione al ripetitore:** richiesta di stato (versione, carico, ora, pacchetti ripetuti), prova di collegamento attraverso il ripetitore (come la prova di oggi, ma con risposta del ripetitore), richiesta di identificazione, spegnimento o accensione temporanea, cambio di potenza o di frequenza (solo con autenticazione).
- **Dal ripetitore alle stazioni:** pacchetto periodico di **identificazione e stato** (nominativo del ripetitore, frequenza, carico), avvisi di servizio.

**Autenticazione:** un comando che cambia lo stato del ripetitore non può essere accettato da chiunque. Si può autenticare con un **codice a firma** (HMAC con un segreto condiviso e un contatore contro la ripetizione), che **non cifra il contenuto**: il testo resta leggibile e solo l'origine si verifica. Se questa scelta sia compatibile con le regole per i radioamatori (nessuna cifratura, controllo remoto delle stazioni automatiche) va **chiarito con l'ARI** prima di usarla; finché non è chiarito si limitano i comandi a quelli **senza autenticazione e senza effetti** (stato, prova, identificazione).

**Compatibilità:** con i comandi nel modo 5, nessun cambio di versione del protocollo. Con il bit 7 delle bandiere come "ripetuto", le schede già in uso non sentono le copie ripetute (come già scritto sopra): è lo stesso costo dell'unico cambio di formato richiesto dal ripetitore.
