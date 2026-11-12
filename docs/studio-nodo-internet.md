# Studio: nodo VoLoRa su Wi-Fi (T3 ↔ T3 via UDP/TCP)

12/11/2026. **Solo studio, nessun codice.** Primo passo del collegamento a Internet ([studio-collegamento-internet.md](studio-collegamento-internet.md), "livello 0"): **due T3 o due T-Beam** (il nodo va bene su entrambe, a differenza del ripetitore che è solo T3; anche una T3 e una T-Beam insieme) collegate in Wi-Fi allo stesso router, "nodo 1" e "nodo 2". Chi usa un dispositivo VoLoRa trasmette sulla frequenza di un nodo; il nodo porta i pacchetti all'altro nodo via TCP/IP, che li ritrasmette in radio. **Tutto gira sulla T3**, senza Raspberry. Passi successivi: un **reflector nostro** (stesso protocollo, un server che unisce molti nodi), poi un **ponte** verso reflector esistenti (SvxLink, YSF/DMR, vedi l'altro studio).

Distinguo **fatti verificati** (dal codice del progetto, dall'esito della CI) da **conoscenza generale** dell'ESP32 e di lwIP (non verificata qui) e da **da misurare**.

## Cosa c'è già e si riusa
- **I pacchetti radio** (formato, `encodePacket`/`decodePacket`) sono già un datagramma autosufficiente: nessuna conversione. Il più grande (profilo Qualità, 12 frame da 16 byte + intestazione) sta in poco più di 200 byte: con le intestazioni del nodo entra in **un solo datagramma UDP**.
- **La logica del ripetitore** (`Repeater`): ripete solo i modi voluti, mette il flag `RELAYED`, scarta i pacchetti già ripetuti e i duplicati, limita la sessione a 35 s ([ripetitore.md](ripetitore.md)). È la stessa logica che serve a un nodo.
- **`TxQueue`** (coda con pausa di 80 ms fra i pacchetti), **CAD** prima di trasmettere, **HMAC-SHA256** e firma con contatore (già scritti per i comandi del sysop, [protocollo.md](protocollo.md)), **ruoli** (`CFG SET ROLE`), console `CFG`, pagine di configurazione, display.
- **Manca:** tutta la parte di rete (Wi-Fi in modalità client, socket, protocollo fra nodi, riconnessione). Oggi il Wi-Fi del firmware è **solo access point per la pagina di configurazione** (verificato: `src/wifi_config.cpp` usa `WIFI_AP`).

## Come è fatto il nodo
Una T3 sola, **senza audio e senza Codec2** (i pacchetti passano così come sono): radio SX1278 + Wi-Fi + display.

```
stazioni VoLoRa  ⇄  [LoRa, frequenza F1]  T3 nodo 1  ⇄  Wi-Fi/router/LAN  ⇄  T3 nodo 2  ⇄  [LoRa, frequenza F2]  ⇄  stazioni VoLoRa
```
- **Radio → rete:** la radio riceve un pacchetto valido; `Repeater` decide; il nodo lo incapsula (firma) e lo manda all'altro nodo.
- **Rete → radio:** arriva un datagramma; il nodo controlla la firma e il contatore, lo mette nella `TxQueue`, e lo trasmette con CAD, con il flag `RELAYED`.
- **Una sola radio per nodo = semi-duplex.** Mentre il nodo trasmette (voce arrivata dalla rete) non ascolta le stazioni locali, e viceversa: è il limite di un solo canale, lo stesso del profilo Messaggio. Conseguenze: una stazione locale che parla **proprio mentre** il nodo trasmette si perde; il CAD prima di trasmettere riduce il problema ma non lo elimina. **Politica proposta:** priorità alla voce **locale** (se il canale è occupato, il pacchetto di rete aspetta in coda; se la coda si riempie, i pacchetti vecchi si scartano e si conta).
- **Frequenze:** per il banco usare **due frequenze diverse** (nodo 1 su F1, nodo 2 su F2): così le stazioni di un sito non sentono l'altro nodo e non ci sono giri. In un uso vero, due nodi nella **stessa area radio sulla stessa frequenza** si sentirebbero fra loro: il flag `RELAYED` evita che un pacchetto già ripetuto sia ripetuto di nuovo, ma conviene comunque non farlo.

## UDP o TCP
| | UDP | TCP |
|---|---|---|
| Perdita di pacchetti | si perde (la **parità** XOR ne recupera uno su quattro se ne mancano pochi) | ritrasmette da solo |
| Ritardo con perdite | nessuno aggiunto | **si blocca** finché il pacchetto perso non è ritrasmesso (head-of-line blocking): in voce è peggio di una perdita |
| Ordine | non garantito: serve il numero di sequenza (c'è già: sessione + `seq`) | garantito |
| Collegamento | senza stato; servono un **keepalive** e il riconoscimento del nodo | connessione da gestire e riaprire |
| Dietro NAT/Internet | il nodo che parla per primo "apre" il passaggio nel router (keepalive ogni 20-30 s) | connessione in uscita: semplice |
| Memoria su ESP32 | poca | un po' di più |

**Proposta: UDP**, con numero di sequenza, firma HMAC e keepalive. La voce di questo progetto **è già pensata per le perdite** (parità, ricostruzione), quindi una perdita in rete non è peggio di una perdita in radio. TCP avrebbe senso solo per un canale di controllo (configurazione, stato). Da confermare con te.

## Protocollo fra i nodi (bozza "VoLoRa-IP")
Un datagramma UDP: `magic (2) | versione (1) | tipo (1) | id nodo (4) | id di avvio (4) | contatore (4) | canale (1) | lunghezza (1) | contenuto | firma (8)`.
- **Tipi:** `PACKET` (contenuto = il pacchetto radio così com'è), `HELLO` (si presenta, chiede la sfida), `KEEPALIVE`, `BYE`.
- **Firma:** HMAC-SHA256 troncato a 8 byte con una **chiave condivisa** (come per i comandi del sysop), calcolata su tutto il datagramma. **Senza firma chiunque su Internet potrebbe far trasmettere il nodo in radio**: la firma è obbligatoria, non un'opzione.
- **Anti-ripetizione:** contatore crescente per nodo; dopo un riavvio il contatore ricomincia, quindi serve una **sfida** (`HELLO` → il ricevitore manda un numero casuale → il mittente risponde firmato) per riaccettare un nodo senza che una vecchia registrazione possa riaprire il passaggio.
- **Canale** (1 byte, di base 0): non serve al primo passo, ma lo mette in conto per il reflector (un "talk group" per ripartire il traffico).
- **Nessuna cifratura del contenuto:** si autentica, non si cifra (la voce resta leggibile come in radio; è la stessa scelta dei comandi del sysop).

## Ritardo e ritmo
- Ritardo di un pacchetto: durata in aria in ricezione + rete (nella LAN pochi ms, con il Wi-Fi qualche decina di ms di jitter) + coda di trasmissione + durata in aria in trasmissione. **Circa un pacchetto di ritardo in più** rispetto alla radio diretta, cioè **del mezzo secondo e oltre**: da misurare.
- Il pacchetto di rete **non porta il ritmo originale**, ma le stazioni ricevono con il pre-buffer e i tempi di sessione di sempre; come per il ripetitore, la trasmissione riparte dal ritmo della coda (almeno 80 ms fra i pacchetti, in pratica il tempo in aria).
- **Backpressure:** un nodo che ritrasmette (a SF8 un pacchetto dura molto più di quello che la rete consegna) può accumulare ritardo se arriva più voce di quanta il canale ne smaltisca: limite di coda e scarto dei pacchetti più vecchi, con contatore.

## Risorse della T3 (ESP32-PICO-D4)
- **Misurato dalla CI** (ultimo run, firmware T3): `text` 875 436 B, `data` 406 992 B, `bss` 80 185 B; la piattaforma dichiara **320 KB di RAM** utilizzabile e 4 MB di flash. Il Wi-Fi (stack, AsyncTCP, server web) **è già linkato** per la pagina di configurazione, quindi il costo in flash di un nodo è piccolo.
- **Un nodo non usa audio né Codec2** (i consumatori più grandi di memoria del firmware attuale, che a volte segnala "memoria" insufficiente nelle prove): la RAM che resta per rete, coda e display dovrebbe bastare, **ma non l'ho misurata**: il primo passo del codice sarà stampare l'heap libero all'avvio.
- **Da conoscenza generale dell'ESP32 (non verificata qui):** la modalità client Wi-Fi occupa una cinquantina di KB di heap, la ricezione UDP di lwIP ha una coda corta (circa 6 datagrammi): durante una trasmissione **bloccante** (un pacchetto a SF12 dura 3,4 s, a SF8 molto meno) il ciclo principale non legge il socket e i datagrammi in più si **perderebbero**. Rimedio: un **task dedicato** (secondo core) che legge il socket e mette i pacchetti in una coda grande nel firmware, come si è fatto per la seriale (buffer da 2 kB).
- **Interrupt della radio:** già in IRAM; il Wi-Fi ha priorità alta ma l'ISR dei pacchetti ricevuti è breve: da verificare sul banco che non ci siano pacchetti sovrascritti (esiste il contatore `rxOverwritten`).
- **Risparmio energia del Wi-Fi spento** (`WIFI_PS_NONE`) per non aggiungere ritardo; consumo più alto (alimentazione da USB, non da batteria).
- **Riconnessione** automatica e stato visibile sul display (Wi-Fi, IP, peer raggiungibile, contatori).

## Configurazione
Nuovo ruolo `NODE` (come `RPTRX`/`RPTTX`: senza menu a bordo, per tornare normale serve il cavo USB e `CFG SET ROLE OFF`). Impostazioni nuove: nome e password del Wi-Fi, **indirizzo e porta dell'altro nodo**, porta di ascolto, **chiave condivisa** (mai mostrata), id del nodo. Si impostano dal sito (Web Serial) e dalla seriale; la pagina interna della scheda non si apre più in questo ruolo (stesso limite del ripetitore). Il nodo mostra su ogni riga: stato del Wi-Fi e IP, peer raggiungibile o no (e ritardo), pacchetti da radio a rete e da rete a radio, scartati, ultima voce.

## Piano di prova a banco (primo passo)
Strumenti: **2 T3 nodo** sullo stesso router, **2 T3 stazione** normali con il microfono (o con la prova di collegamento, senza audio), antenne collegate, potenza minima.
1. **Rete:** i due nodi si vedono (sfida, firma, keepalive); si scollega il router e si ricollega: il nodo si riconnette da solo.
2. **Radio → rete → radio:** la stazione A (sulla frequenza del nodo 1) trasmette; la stazione B (sulla frequenza del nodo 2) riceve. Poi il contrario.
3. **Misure:** ritardo da una parte all'altra; pacchetti persi in rete (contatori `seq`); recupero della parità; **voce intelligibile** nei tre profili (Standard prima).
4. **Casi difficili:** due stazioni che parlano insieme; un nodo che riceve voce mentre trasmette; perdita del Wi-Fi a metà frase; pacchetto con firma sbagliata o ripetuto (deve essere scartato e contato); riavvio di un nodo (rientra con la sfida).
5. **Carico:** una conversazione lunga (TOT di 30 s più volte di seguito), per vedere coda, ritardo e memoria.

## Rischi e limiti
- **Router con isolamento dei client** (AP isolation): i due nodi non si vedono in LAN; si cambia impostazione del router o si usa un server intermedio (il reflector).
- **Internet vero:** NAT, indirizzi che cambiano, nessuna porta aperta: per questo il **reflector** (che accetta connessioni in uscita dai nodi) è il passo giusto dopo; il protocollo è lo stesso.
- **Esposizione:** un nodo su Internet con la porta aperta è un bersaglio: firma, limite di frequenza dei datagrammi, scarto silenzioso di ciò che non è firmato.
- **Canale occupato dalla rete:** troppa voce dalla rete riempie il canale LoRa; politica di coda e limite di durata (vedi sopra).
- **Semi-duplex:** perdita di ciò che le stazioni locali dicono mentre il nodo trasmette (vedi sopra); per un sito serio servirebbero due T3 (RX e TX, come il ripetitore) e un nodo con il collegamento di rete: è un secondo livello.
- Le questioni di autorizzazione dei collegamenti fra reti sono dell'operatore e non sono trattate qui.

## Dal nodo al reflector (passi successivi)
- **Reflector nostro:** un programma su Linux (C++ o Python, piccolo) che accetta i nodi (firmati, una chiave per nodo), li registra con `HELLO`, e ripete ogni `PACKET` a tutti gli altri nodi **tranne il mittente** (per canale). È lo stesso protocollo del primo passo: un nodo parla con "l'altro nodo" oppure con il reflector indifferentemente.
- **Ponte verso i reflector esistenti:** un secondo programma sul server (o sul Raspberry) che parla il protocollo del reflector nostro da un lato e quello di SvxLink (con OPUS) o YSF/DMR (con AMBE) dall'altro, come descritto in [studio-collegamento-internet.md](studio-collegamento-internet.md).

## Come procederei
1. **Contatori e protocollo su PC:** il modulo del protocollo VoLoRa-IP (codifica, firma, sfida, anti-ripetizione) in `lib/voicecore`, provato con test come gli altri.
2. **Un nodo "finto" sul PC** (programma di prova che parla UDP con una T3) per provare la T3 senza una seconda T3.
3. **Ruolo `NODE` sulla T3:** Wi-Fi client, task del socket, coda, collegamento con `Repeater` e `TxQueue`, display e impostazioni (console e pagine).
4. **Banco a due nodi** come sopra, e solo dopo il reflector.

## Decisioni che mi servono
1. **UDP + firma** (proposto) o TCP?
2. Nel banco, **due frequenze diverse** per i due nodi (proposto) o la stessa?
3. Il primo passo **solo profilo Standard** (voce e testo; i messaggi memorizzati e gli annunci del ripetitore restano fuori) o anche gli altri?
4. Un **ruolo `NODE`** a sé (proposto) o il nodo come funzione che si aggiunge a `RPTRX`/`RPTTX`?

## Decisioni e stato (12/11/2026)
Decise: **UDP con firma**, **due frequenze diverse** per il banco, **solo profilo Standard** (voce e testo), **ruolo `NODE` a sé**.
**Scritto e provato sul PC:** il protocollo fra i nodi (`lib/voicecore/node_link.*`: datagramma firmato, sfida dopo un riavvio, anti-ripetizione, attivazione a due vie), la logica del nodo (`node_core.*`: regole del ripetitore, coda, controllo del canale, priorità alla voce locale), le impostazioni (`CFG SET ROLE NODE`, `WIFISSID`, `WIFIPASS`, `PEER`, `NODEPORT`, `NODEID`, `PEERID`, `NODEKEY`, `CFG GET NODE`), il ruolo sulla T3 (`src/repeater_node.cpp`: Wi-Fi client, task del socket, coda, display) e un nodo per il PC (`tools/vlnode.cpp`, `make vlnode`). Provati due processi `vlnode` su socket UDP veri (collegamento, testo, voce finta, chiave diversa rifiutata). **Non provato su una T3.** Prova a banco: [banco-nodo.md](banco-nodo.md).

**T3 e T-Beam (12/11/2026).** Il nodo funziona su tutte e due le schede: il firmware del nodo non usa audio, microfono, GPS né le seriali del ripetitore, quindi la differenza è solo la scheda. Sulla **T-Beam** il PMU (AXP2101) è già inizializzato prima della radio dal firmware normale, il GPS resta spento, e il nodo può funzionare anche **a batteria** (consumo del Wi-Fi acceso: da misurare). Il ripetitore (RX e TX con la seriale) resta solo T3: `CFG SET ROLE RPTRX/RPTTX` è rifiutato sulla T-Beam, `CFG SET ROLE NODE` no. **Non provato sulla T-Beam** (la logica è la stessa della T3 e il firmware compila per entrambe).

