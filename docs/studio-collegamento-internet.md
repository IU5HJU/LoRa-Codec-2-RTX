# Studio: collegare VoLoRa a Internet e ai reflector (SvxLink, YSF/C4FM, DMR, NXDN)

12/11/2026. **Solo studio, nessun codice.** Domanda: un nodo connesso a Internet che porta la voce LoRa da una T3 a un'altra via TCP/IP (invece che per seriale), e che la colleghi al **reflector di SvxLink** (per i ripetitori analogici) e a **reflector C4FM/YSF** (e da lì ai sistemi MMDVM: DMR, NXDN). Quello che segue distingue ciò che ho **verificato con fonti** da ciò che è **mia deduzione** o **da verificare**.

**Obiettivo chiarito (12/11/2026): DMR e C4FM** (YSF, Yaesu System Fusion). I due nomi "ODMR" e "53FM" non li avevo identificati (la ricerca di "ODMR" restituisce solo fisica); la risposta è stata "DMR e C4FM", quindi la sezione operativa è [«DMR e C4FM»](#dmr-e-c4fm-come-si-collegherebbe) più sotto. Il resto del documento (livelli, SvxLink) resta come confronto.

## Cosa si porta in rete: i pacchetti, non l'audio

VoLoRa trasporta **Codec2** (1200 bit/s in Range e Messaggio, 3200 bit/s in Standard), 12 frame da 40 ms per pacchetto, parità XOR e sessione per ogni PTT ([protocollo.md](protocollo.md)). Un nodo Internet ha tre modi di lavorare:

| Livello | Cosa fa | Transcodifica |
|---|---|---|
| **0. Tunnel VoLoRa** | porta i pacchetti LoRa **così come sono** (come il collegamento seriale RX↔TX di oggi, [ripetitore.md](ripetitore.md)) fra due siti, su TCP o UDP | **nessuna**: stessi bit |
| **1. Gateway verso SvxLink** | decodifica Codec2, ricodifica in un codec che il reflector accetta | Codec2 ↔ PCM ↔ OPUS/altri (tutti liberi) |
| **2. Gateway verso YSF/DMR/NXDN** | decodifica Codec2, ricodifica in AMBE+2 | Codec2 ↔ PCM ↔ **AMBE+2** (vocoder con brevetti e licenza) |

## Il livello 0 conviene comunque (e non ha controindicazioni di rete)

Un tunnel dei pacchetti VoLoRa fra due ripetitori (o fra la RX e la TX dello stesso ripetitore in due posti) è semplice: il formato c'è già, il flag `RELAYED` evita i giri, il testo, l'identificativo e i comandi del sysop passano allo stesso modo. **Non ha i problemi di codec** sotto, ed è la base su cui costruire gli altri due. Si può fare con un piccolo programma su un PC o un Raspberry collegato alle due schede (oggi hanno la seriale) o, in alternativa, con la scheda stessa sul Wi-Fi (la T3 ha l'ESP32 con Wi-Fi: **tecnicamente possibile ma non provato**; il carico di rete, la riconnessione e la sicurezza vanno pensati).

## SvxLink: cosa risulta dalle fonti

- Il **reflector di SvxLink** (`svxreflector`) usa **TCP per i messaggi di controllo e UDP per l'audio**, porta 5300 per entrambi; l'audio di un nodo è ritrasmesso a tutti gli altri nodi che hanno scelto lo **stesso talk group**, **un solo parlante alla volta** per talk group ([manuale svxreflector](https://manpages.opensuse.org/Leap-15.6/svxlink-server/svxreflector.1.en.html), [configurazione](https://dyn.manpages.debian.org/unstable/svxreflector/svxreflector.conf.5)).
- **Codec previsti dal reflector:** OPUS (di base), SPEEX, GSM, S16 (16 bit non compresso), RAW (float a 32 bit); **uno solo alla volta** ([manuale](https://manpages.opensuse.org/Leap-15.6/svxlink-server/svxreflector.1.en.html)). **Non ho trovato Codec2** né una richiesta in tal senso (la ricerca non è esaustiva: il posto giusto è il [repository di SvxLink](https://github.com/sm0svx/svxlink)).
- Esistono reflector pubblici ([esempio](https://svx.xlx585.net/info)) e il programma è libero: si può anche tenere un **reflector proprio** per collegare solo i nostri ripetitori.
- **Non ho trovato la specifica ufficiale del protocollo V2** (formato dei messaggi, autenticazione, come viaggia il nominativo): serve leggere il codice sorgente di SvxLink. È il primo passo prima di dire con certezza che si può fare.

### Pro (verso il reflector SvxLink)
- **Tutto libero:** protocollo e programma aperti, codec liberi (OPUS), **nessuna licenza di vocoder**; si può scrivere un gateway da zero o appoggiarsi al codice di SvxLink.
- **Porta ai ripetitori analogici FM** che usano SvxLink (collegati al reflector come nodi), senza passare per i sistemi digitali commerciali.
- **Modello semplice:** talk group, un parlante, PTT = inizio e fine; combacia con le sessioni VoLoRa (`START`/`END` nel pacchetto).
- **Reflector privato** possibile per collegare solo i nostri siti, con le nostre regole.
- Il gateway può girare su un **Raspberry/PC** con Codec2 e OPUS (librerie aperte), senza hardware aggiuntivo.

### Contro
- **Transcodifica obbligatoria** (Codec2 ↔ OPUS): più ritardo e **qualità limitata dal Codec2 a 1200 bit/s** (suono "robotico": è il limite della nostra voce, non migliora). Il flusso dalla parte analogica, a banda piena, viene stretto a 1200 bit/s.
- **Il nominativo del singolo parlante** probabilmente **non viaggia** (il reflector riconosce il nodo, non la persona): da verificare; in tal caso va annunciato a voce o si perde l'identificazione dell'operatore LoRa.
- **Il canale LoRa è piccolo:** una conversazione alla volta, con TOT (30 s in Range, 15 s in Messaggio) e pausa tra i turni. Un talk group affollato lo **occuperebbe** e lo terrebbe in trasmissione: serve una politica (quale talk group, limiti di durata, priorità alla voce locale).
- **Semi-duplex e ritmo:** la voce LoRa arriva a pacchetti da 12 frame (circa mezzo secondo) con la parità; il ritardo totale (LoRa + Internet + ripetizione) è di diversi secondi. Normale per un reflector, ma da misurare.
- **Il modo Messaggio (memorizza e ritrasmetti) e i messaggi di testo non hanno corrispondenti** nel reflector.
- **Regole e accettazione:** ogni reflector ha il suo gestore e le sue regole su chi può collegarsi e con che tipo di nodo; un nodo "LoRa" va concordato.
- **Esposizione a Internet:** autenticazione, ritorni, nodi che cadono e rientrano, sicurezza del gateway: lavoro vero, non banale.

## YSF / C4FM, DMR, NXDN (MMDVM): cosa risulta dalle fonti

- I **reflector YSF** scambiano pacchetti UDP (`YSFP` poll/keepalive, `YSFD` voce e dati, `YSFU` scollegamento, `YSFS` stato) sulla porta 42000 e ripetono ogni pacchetto a tutti i nodi collegati tranne il mittente ([un'implementazione](https://hub.docker.com/r/siteworxpro/ysf-reflector-go)). Non ho trovato (né verificato) il **formato dei frame** di voce.
- **Il vocoder è AMBE+2** (YSF dopo il DMR ne usa uno della stessa famiglia, [fonte](https://hub.docker.com/r/siteworxpro/ysf-reflector-go)). Per il DMR lo è sicuramente; **per l'NXDN è mia conoscenza, non verificata qui**.
- **L'AMBE ha brevetti** (DVSI) e **richiede una licenza**; le implementazioni software esistono come codice indipendente, ma **lo stato dei brevetti e delle licenze è controverso** e non ho trovato un parere autorevole ([Wikipedia: Multi-Band Excitation](https://en.wikipedia.org/wiki/Multi-Band_Excitation), [una pagina sul vocoder AMBE lato server](https://dmrhub.ru/tech/en/server-vokoder-ambe-bez-dongla), [intervento TAPR 2010 sui problemi dei codec non pubblici](https://files.tapr.org/meetings/DCC_2010/DCC2010-Codec2-presentation-K6BP-VK5DGR.pdf)). Non è un parere legale. In pratica servono un **chip DVSI** (hardware) o il rischio di un'implementazione software.
- **BrandMeister (DMR):** serve un **DMR ID** valido; è **vietato il "peer bridging"** (usare il protocollo MMDVM per fare da ponte fra reflector/server) e **non si fanno nuovi ponti verso altre reti**; i **ponti analogici non sono supportati**. I protocolli di ponte ammessi sono OpenBridge, XLX Interlink, Wires-X, YSF, NXDN ([regole di BrandMeister](https://help.brandmeister.network/bridges/bridging-to-brandmeister/); controlla la pagina aggiornata prima di fidarti).
- Esistono strumenti di conversione fra modi per MMDVM (per esempio YSF2DMR, [README](https://github.com/juribeparada/MMDVM_CM/blob/master/YSF2DMR/README.md)): l'idea "YSF come perno, poi DMR/NXDN" è comune.

### Pro (verso YSF/C4FM e, da lì, DMR/NXDN)
- **Una base enorme** di ripetitori, hotspot e utenti già collegati; il reflector YSF è semplice a livello di trasporto (UDP, pochi tipi di pacchetto).
- Un solo gateway YSF apre la strada ai sistemi collegati a quel reflector, e con gli strumenti MMDVM anche a DMR e NXDN.

### Contro
- **Il vocoder AMBE+2 è il problema:** la transcodifica Codec2 ↔ AMBE+2 serve in tutte e due le direzioni e richiede **o un chip DVSI** (costo, dipendenza dall'hardware) **o un'implementazione software con rischio di brevetto/licenza** da valutare a parte (non lo so risolvere io, e non è la stessa cosa di OPUS).
- **Doppia (o tripla) transcodifica** (Codec2 1200 → PCM → AMBE+2 → eventuale altro): la qualità, già bassa, si degrada ancora.
- **Identificazione dell'utente:** i sistemi digitali portano il nominativo o il **DMR ID** del singolo parlante; un gateway ne ha **uno** (il suo) a meno di una tabella di corrispondenza che non esiste. Per il DMR serve un **DMR ID** del nodo e va rispettata la politica del network.
- **Regole di rete:** BrandMeister vieta il peer bridging; i ponti verso altre reti passano da protocolli ammessi e concordati con i gestori. Un nodo "fatto in casa" che unisce LoRa e DMR **non è automaticamente accettato**.
- **Banda e tempi:** i sistemi digitali parlano a ritmo continuo e a turni lunghi; il canale LoRa regge una conversazione alla volta (stesso limite di sopra).
- **Il protocollo dei frame YSF non è verificato** qui: serve il codice di riferimento (YSFReflector di G4KLX).

## DMR e C4FM: come si collegherebbe

Dalle fonti (guide e forum della comunità, per lo più del 2018-2020: **da controllare sulla documentazione attuale** prima di costruire) lo strumento che unisce audio generico e reti digitali è **DVSwitch**: **Analog_Bridge** prende l'audio PCM da una porta UDP (il lato "USRP") e lo **codifica in AMBE** per l'altra porta; **MMDVM_Bridge** collega il lato digitale alle reti come un hotspot virtuale (DMR, YSF, NXDN). Una catena tipica: nodo audio → Analog_Bridge → MMDVM_Bridge → master DMR ([guida](https://lucifernet.com/?p=109), [catena con BrandMeister](https://redfast00.github.io/2020-building-a-brandmeister-dmr-bridge.html), [MMDVM_CM](https://github.com/nostar/MMDVM_CM)).

Il nostro nodo avrebbe questa forma, su un Raspberry/PC collegato alla T3 per seriale:

`T3 (LoRa) ⇄ seriale ⇄ gateway VoLoRa (Codec2 ↔ PCM) ⇄ UDP/PCM ⇄ Analog_Bridge (PCM ↔ AMBE) ⇄ MMDVM_Bridge ⇄ master DMR e/o reflector YSF`

Il gateway VoLoRa farebbe due cose sole: **Codec2 ↔ PCM** (libreria aperta, già nel progetto) e la **segnalazione** (inizio e fine del PTT dalle sessioni; chi parla dal nominativo nel pacchetto `START`). Tutto l'AMBE e il dialogo con le reti sta in programmi esistenti.

### Dove sta il vincolo: l'AMBE
Le fonti indicano tre modi di avere l'AMBE in Analog_Bridge:
1. **Chip DVSI (hardware):** NW Digital **ThumbDV / DV3000**, il dispositivo che le guide di DVSwitch raccomandano per D-STAR, DMR, YSF e NXDN. Nessun dubbio di licenza per chi lo usa, costo e una dipendenza USB.
2. **md380-emu (software):** un emulatore del vocoder del firmware di un ricetrasmettitore DMR; è la via più usata senza chip. Il suo **stato legale e di brevetto è dubbio** secondo le stesse fonti: una scrive che andrebbe verificato dove si vive; un'altra indica un brevetto in scadenza il 7/11/2023 **senza che io abbia trovato una conferma autorevole** né un parere legale ([esempio](https://github.com/redfast00/brandmeister-dmr-sea), [pagina sul vocoder lato server](https://dmrhub.ru/tech/en/server-vokoder-ambe-bez-dongla)). Non lo decido io: lo valuta l'operatore.
3. **Fallback software** del decodificatore (`decoderFallBack`) quando manca il chip, secondo un file di esempio.

### DMR (BrandMeister e altre reti)
- **Serve un DMR ID** per il nodo; BrandMeister lo chiede anche per i ripetitori e i personali ([regole](https://help.brandmeister.network/bridges/bridging-to-brandmeister/)).
- **BrandMeister vieta il "peer bridging"** (usare MMDVM per fare da ponte fra server o reflector) e non fa ponti verso altre reti; i ponti analogici non sono supportati. Un collegamento "hotspot virtuale" è una cosa diversa dal ponte, ma una fonte riferisce che BrandMeister **non gradisce** MMDVM_Bridge e DVSwitch porta un componente che si collega con un altro protocollo (aneddoto di un forum, non verificato). Da chiarire con i gestori della rete **prima** di collegarsi.
- **Un talk group (TG)** va scelto: statico o dinamico, con il proprio ID.
- **Un solo ID per tutto il nodo:** gli utenti LoRa non hanno DMR ID; il nodo si presenta con il suo.

### C4FM / YSF
- **YSF è più semplice del DMR** come trasporto (UDP, pacchetti `YSFP`/`YSFD`/`YSFU`/`YSFS`, porta 42000, [fonte](https://hub.docker.com/r/siteworxpro/ysf-reflector-go)); il vocoder è comunque AMBE+2 (stesse tre strade).
- **Il nominativo si porta nei frame** (il formato dei frame va letto dal codice di riferimento): un nodo YSF può quindi dichiarare **il nominativo del singolo utente LoRa**, a differenza del DMR dove conta l'ID. Da verificare.
- Molte reti YSF sono **a connessione libera** (si sceglie il reflector e ci si collega), con meno burocrazia del DMR; il gestore del reflector decide chi accetta.

### Pro e contro, riassunti per il vostro caso
| | DMR | C4FM (YSF) |
|---|---|---|
| Base di utenti | enorme (BrandMeister e altre reti) | molto estesa (Fusion, hotspot) |
| Requisiti | **DMR ID**, regole della rete, TG | un reflector che accetta il nodo |
| Identità del parlante | un ID per nodo | nominativo nei frame, da verificare |
| Regole di ponte | **restrittive** (peer bridging vietato, nuove reti no) | più libere, decide il gestore |
| Vocoder | AMBE+2 (chip DVSI, oppure software dubbio) | AMBE+2 (idem) |
| Conversione fra modi | con MMDVM_CM / DMRGateway ecc. | idem (YSF2DMR ecc.) |
| Ordine consigliato | **dopo** YSF | **prima**: meno vincoli e più prove possibili |

### Limiti comuni (valgono per entrambi)
- **Range:** Codec2 1200 → PCM → AMBE+2 → l'altro lato: due vocoder in cascata, voce molto più povera di quella nativa dei sistemi digitali.
- **Canale LoRa:** una conversazione alla volta; una rete affollata lo terrebbe in trasmissione. Servono un **TG o reflector tranquillo**, un limite di durata e la priorità alla voce locale.
- **Ritardi:** LoRa (mezzo secondo a pacchetto) + due transcodifiche + Internet: alcuni secondi.
- **Messaggi memorizzati e testo** non hanno corrispondenti; l'identificativo del ripetitore andrebbe adattato (annuncio vocale o testo di stato del nodo).
- **Tutta la parte autorizzativa** (uso dell'AMBE, collegamento fra reti, regole dei gestori) è dell'operatore.

### Primo passo proposto per DMR e C4FM
1. **Prova senza trasmettere in aria dalla parte digitale:** gateway VoLoRa che porta una sessione LoRa in PCM su UDP e un Analog_Bridge su un Raspberry con il chip ThumbDV/DV3000 verso **un reflector YSF di prova** (o un reflector YSF privato su un altro Raspberry), solo in una direzione.
2. Se funziona, il **senso inverso** (YSF → LoRa) con il limite di durata.
3. Poi si guarda il DMR: con il gestore, il DMR ID del nodo e un TG di prova.

## Una terza strada, da verificare: M17 e i reflector con transcodifica

**M17** è un protocollo digitale amatoriale aperto **basato su Codec2** (mia conoscenza; un commit di un client M17 che usa Codec2 a 3200 bit/s è l'unico riscontro che ho trovato, [esempio](https://code.octet-stream.net/m17rt/commitdiff/2c7d53c113f4e19f24c928b2c5eca1c3f6f799af)). Il nostro **profilo Standard (Codec2 3200)** parla già lo stesso codec. Esistono **reflector con transcodifica** (XLX con hardware AMBE, [spiegazione](https://w8wky.org/what-are-transcoding-reflectors/)) che collegano fra loro modi diversi: se un reflector accetta M17 o un protocollo con Codec2 e fa lui la conversione verso DMR/YSF/D-STAR, **la licenza AMBE resta a carico di chi gestisce il reflector** e il nostro gateway non tocca l'AMBE. **Non ho verificato** quali modi (NXDN compreso) colleghi un dato reflector con M17. Vale la pena guardare prima di costruire un transcodificatore proprio.

## Confronto in breve

| | Tunnel VoLoRa (0) | SvxLink (1) | YSF/DMR/NXDN diretto (2) | M17/XLX (idea) |
|---|---|---|---|---|
| Vocoder | Codec2 (nessuna transcodifica) | OPUS ecc. (libero) | **AMBE+2** (licenza/brevetti) | Codec2 |
| Software | nostro, semplice | nostro o di SvxLink | nostro + chip DVSI o rischio | nostro; il reflector fa l'AMBE |
| Utenti raggiunti | i nostri ripetitori | ripetitori analogici SvxLink | Fusion, DMR, NXDN | dipende dal reflector |
| Identità del parlante | nel pacchetto | probabilmente persa | nodo, non persona | da verificare |
| Specifica verificata | sì (nostra) | **no** (V2 da leggere) | **no** (frame da leggere) | no |
| Rischio principale | sicurezza | accettazione e occupazione del canale | **licenza AMBE**, regole di rete | disponibilità e regole del reflector |

## Dove far girare il nodo
- **Raspberry/PC** vicino alle schede (consigliato per i livelli 1 e 2): il gateway parla la seriale con la T3 come oggi, ha le librerie (Codec2, OPUS), la rete e il sistema operativo per riconnettersi e registrare; è il posto dove stanno anche SvxLink e gli strumenti MMDVM.
- **T3 sul Wi-Fi** (ESP32): il tunnel di livello 0 sì, in teoria (TCP/UDP semplici); la transcodifica verso OPUS/AMBE **non** conviene lì (CPU, memoria, nessun chip AMBE).

## Come procederei (ordine proposto)
1. **Livello 0:** tunnel di pacchetti VoLoRa fra due nodi (anche solo due Raspberry con una T3 ciascuno). Utile subito, poco rischio, collaudabile con le schede già pronte.
2. **Leggere il codice di SvxLink** (protocollo V2 del reflector) e provare un **gateway di sola ricezione**: si ascolta un talk group di un reflector di prova e si porta a una T3 (LoRa TX). Da lì si decide se aprire anche la strada inversa.
3. **Reflector privato** SvxLink per i nostri ripetitori, prima di toccare reflector pubblici.
4. **YSF/DMR/NXDN solo dopo** aver deciso come gestire l'AMBE (chip DVSI, o l'idea M17/XLX con transcodifica a carico del reflector), e con l'accordo dei gestori.

## Domande aperte per te
- Hai già a disposizione un chip **ThumbDV / DV3000** (o ne compri uno), o si pensa al software? (cambia tutto il lato AMBE)
- Un **reflector YSF di prova** o un reflector YSF tuo, per cominciare, e quale (nome e gestore)?
- Un **DMR ID** per il nodo (il tuo, o quello del ripetitore) e un **TG** di prova?
- Il nodo Internet serve prima a **collegare due nostri ripetitori** (livello 0) o prima ad aprire verso gli analogici (SvxLink)?
- Hai già un reflector/rete di riferimento (SvxLink o YSF) con cui parlare, e chi lo gestisce?
- La parte di autorizzazioni e regole sui collegamenti fra reti diverse (anche l'uso dell'AMBE) la valuti tu come operatore.
