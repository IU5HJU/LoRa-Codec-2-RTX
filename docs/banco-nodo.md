# Prova a banco del nodo Internet (due T3 in Wi-Fi)

12/11/2026. Primo passo del collegamento a Internet ([studio-nodo-internet.md](studio-nodo-internet.md)): due schede (T3 o T-Beam) in ruolo **NODE**, sullo stesso router, che si portano a vicenda i pacchetti LoRa con UDP firmato. **Scritto e provato sul PC** (test della logica e due processi `vlnode` su socket UDP veri). **Eseguito su due schede il 12/11/2026** (T-Beam e T3, firmware `808bc07`): il collegamento sale e i pacchetti passano dalla rete alla radio; esiti in [prove.md](prove.md). Decisioni prese: UDP con firma, due frequenze diverse, solo profilo Standard, ruolo `NODE` a sé.

## Cosa serve
- **Due schede** (due T3, due T-Beam o una di ciascuna) con l'ultimo firmware (artefatto `firmware-t3-v161` o `firmware-tbeam-v12` di GitHub Actions), antenne a 433 MHz **collegate**; sulla T3 microSD tolta.
- Un router Wi-Fi a 2,4 GHz **senza isolamento dei client**, e le due T3 sulla stessa rete (indirizzi IP noti: assegna due indirizzi fissi al router, oppure leggili dal display di ciascun nodo).
- Per la prova in radio: **due T3 stazione** normali (o la prova di collegamento, senza audio), potenza minima.
- Facoltativo: un PC sulla stessa rete con `make vlnode` (nodo finto, vedi sotto).

## Impostazioni (da cavo USB, una scheda alla volta; poi `CFG REBOOT`)
Una **chiave** di 16 byte uguale sui due nodi (per esempio `openssl rand -hex 16`): **non va mai in aria né sul display**.

| | Nodo 1 | Nodo 2 |
|---|---|---|
| Nominativo | `CFG SET CALLSIGN <nominativo>` (vero: altrimenti **non trasmette**; va bene `N0CALL` per provare la sola rete). **Attenzione:** `CFG SET CALL` non esiste e dà `CFG ERR CALL parametro sconosciuto` | idem |
| Frequenza LoRa | `CFG SET FREQ 433.900` | `CFG SET FREQ 434.400` (**diversa**, vedi sotto). Nella prova del 12/11/2026: 433.900 (T-Beam) e 430.000 (T3, il valore di base) |
| Profilo | `CFG SET PROFILE STD` | `CFG SET PROFILE STD` |
| Potenza | `CFG SET POWER 2` | `CFG SET POWER 2` |
| Ruolo | `CFG SET ROLE NODE` | `CFG SET ROLE NODE` |
| Rete | `CFG SET WIFISSID <nome>`, `CFG SET WIFIPASS <password>` | idem |
| Id | `CFG SET NODEID 1`, `CFG SET PEERID 2` | `CFG SET NODEID 2`, `CFG SET PEERID 1` |
| Altro nodo | `CFG SET PEER <IP del nodo 2>:5400` | `CFG SET PEER <IP del nodo 1>:5400` |
| Porta di ascolto | `CFG SET NODEPORT 5400` (è già quella di base) | idem |
| Chiave | `CFG SET NODEKEY <32 cifre esadecimali>` | **la stessa** |

`CFG GET NODE` mostra tutto tranne la password e la chiave (dicono solo `set` o `none`). Se manca qualcosa il nodo lo scrive sul display ("NODO INTERNET / NON PRONTO / manca ...") e resta in ascolto della console USB.

**Perché due frequenze:** le stazioni del sito 1 trasmettono sulla frequenza del nodo 1 e quelle del sito 2 sulla frequenza del nodo 2. Con la stessa frequenza, nello stesso locale, ogni nodo sentirebbe l'altro. Le stazioni impostano la frequenza del profilo Standard: `CFG SET FREQ 433.900` per le stazioni del sito 1, `CFG SET FREQ 434.400` per quelle del sito 2.

## Cosa deve succedere
1. **All'avvio** (seriale USB): `[nodo] id 1, rete "...", altro nodo 192.168.1.51:5400 (id 2), ascolto sulla porta 5400, radio 433.900 MHz profilo Standard`, poi `[nodo] memoria libera: ... byte` (**dimmi il numero**: serve a sapere quanto resta), `[nodo] Wi-Fi 192.168.1.50` (il tuo IP).
2. **Entro pochi secondi** `[nodo] collegamento con l'altro nodo attivo` su **tutti e due** i nodi. Sul display: "Nodo 192.168.1.51 ok".
3. **Radio → rete → radio:** una stazione sul sito 1 trasmette (per esempio un testo o una voce breve); il nodo 1 mostra `R>N` che sale, il nodo 2 `N>R` che sale e **trasmette**; una stazione sul sito 2 riceve. Il display della stazione dice "Ultimo ... **via RPT**" (il flag `RELAYED`). Poi al contrario.
4. **Ogni 30 secondi** il nodo scrive sulla seriale i contatori (`radio->rete`, `rete->radio`, `persi coda`, `firma errata`, `ripetuti`, `heap`).

## Prove da fare
| Prova | Atteso |
|---|---|
| Spegni il router per un minuto, riaccendilo | `Wi-Fi perso`, poi `collegamento giù`, poi tutto torna da solo |
| Riavvia un nodo (RST) | l'altro lo riautentica (sfida) e il collegamento riparte |
| Chiave diversa sui due nodi (`CFG SET NODEKEY` diversa) | il collegamento **non sale mai**, `firma errata` sale |
| Una stazione sul sito 1 parla mentre il nodo 1 sta trasmettendo (voce dalla rete) | il nodo rimanda: `busyDeferred` (non visibile sul display) e perdita della frase locale possibile: **da annotare** |
| Conversazione lunga (TOT di 30 s più volte) | `persi coda` resta 0 o piccolo; memoria libera stabile |
| Nodo finto sul PC | vedi sotto |

## Nodo finto sul PC (`vlnode`)
Con una sola T3 si può provare tutto il lato rete: sul PC `make vlnode`, poi
`build/vlnode --id 2 --peer-id 1 --key <la chiave> --listen 5400 --peer <IP della T3>:5400 --send-text IU5HJU "Prova" --every 10`
e la T3 (con `NODEID 1`, `PEERID 2`, `PEER <IP del PC>:5400`) deve **trasmettere in radio** il testo ogni 10 s, e il PC stampa i pacchetti che la T3 riceve in radio dalle stazioni. Con `--send-voice IZ0ABC --frames 100` manda una voce finta.

## Esito della prova del 12/11/2026
Dettagli in [prove.md](prove.md). In sintesi: con un `PEER` sbagliato il collegamento non sale; con la chiave diversa non sale e `firma errata` cresce di circa 15 ogni 30 secondi; con le impostazioni giuste compare `collegamento con l'altro nodo attivo`. Memoria libera all'avvio circa 146 KB, heap stabile intorno a 140,7 KB con il collegamento attivo.

Il 12/11/2026 sono poi riusciti, con due stazioni, il **testo nei due sensi** (con "via RPT" sulla stazione ricevente) e la **voce intelligibile con `persi 0`**, con un ritardo misurato a mano di **1,4 s** da una stazione all'altra. Anche la prova del router spento è superata (ripartenza da sola entro 30 secondi). Anche il riavvio di un nodo è superato (collegamento di nuovo `ok` in circa 10 secondi). Restano la conversazione lunga e la trasmissione di una stazione mentre il nodo trasmette.

## Cosa mandarmi
Le righe `[nodo]` dell'avvio (con la memoria libera), le righe ogni 30 s dopo una prova con traffico, l'esito di ogni prova della tabella e, se qualcosa non va, il display e **dove** (rete, radio, firma).

## Limiti noti del primo passo
- **Nessun identificativo periodico** del nodo (il ripetitore ce l'ha; il nodo no, per ora) e **nessuna prova di sicurezza** sul numero di trasmissioni consecutive.
- Il nodo accetta datagrammi da qualunque indirizzo (conta la firma) ma **risponde solo all'indirizzo impostato** con `PEER`: va bene per la rete locale; per Internet vero serve il reflector.
- La password del Wi-Fi è salvata in chiaro nella memoria della scheda (come tutte le impostazioni).
