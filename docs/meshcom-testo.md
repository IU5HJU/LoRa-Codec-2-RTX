# MeshCom: avvisi e accordi sulle reti di testo

**Stato (11/11/2026): provato in aria con una T3 e un nodo MeshCom.** Funzionano: il cambio dei parametri radio a caldo, l'invio di un
testo a un gruppo (con i segnaposti sostituiti), la ricezione di una **risposta diretta** e di una **risposta nel gruppo**, la busta
sul display, il segnale "SMS" in Morse, l'apertura con il PTT breve e il **rientro automatico alla voce** a fine finestra. Il formato dei frame è stato verificato su frame veri
ricevuti da un nodo della rete (decodifica, FCS e ricodifica identica); i test del progetto usano frame di prova costruiti con
quel formato e nominativi inventati. **Conferma di ricezione:** a un messaggio privato scritto dall'app MeshCom ("ci sono" a `IU5HJU-99`) l'app mostra la spunta di consegna
con il nostro nominativo; la riga di diagnostica `CONFERMA ackNNN INVIATA` sulla seriale della scheda lo conferma dal lato nostro.
**Non ancora provato:** l'uso con altre schede o altre zone.

**A cosa serve.** Alla scheda del progetto serve per **avvisare e accordarsi** sulle reti radioamatoriali a pacchetto sui
**433,175 MHz** (MeshCom): per esempio "IU5HJU QRV su VoLora modo Q" a un gruppo, oppure data e ora di un possibile QSO a un amico.
**Non sostituisce un nodo MeshCom** e **la voce non passa da questa rete**: la scheda resta una scheda voce, e va sulla rete di testo
solo per qualche minuto, quando lo chiedi.

## Come funziona: un'escursione a tempo

1. **Menu a bordo** (volume lungo) → **MeshCom**. Prima scegli il testo con il PTT (volume breve per confermare), con in fondo
   **"(solo ascolto)"**; poi scegli il **destinatario** tra quelli impostati.
2. La scheda **cambia i parametri della radio a caldo** (433,175 MHz, SF11, 250 kHz, CR 4/6, sync word 0x2B, preambolo 8), controlla
   che il canale sia libero e invia il testo **una volta sola**. Sul display compare la lettera **T**, "MESHCOM" e il conto alla rovescia
   dell'ascolto (di base 5 minuti).
3. Durante l'ascolto finiscono nella casella degli SMS ("SMS" in Morse, la busta sul display, PTT breve per leggere:
   [modo-messaggio.md](modo-messaggio.md)) i testi **diretti al tuo nominativo-SSID** (o al solo nominativo) e quelli dei **gruppi**
   che hai tra i destinatari impostati: così vedi le risposte ai tuoi QRV. Il testo di gruppo inizia con il gruppo, `[22251] ...`. Restano leggibili anche dopo il rientro. Il PTT lungo non
   trasmette ("SOLO TESTO").
4. Alla scadenza la radio **torna da sola ai parametri del profilo voce** di prima. Nessun riavvio, nessun profilo da cambiare. **Per uscire prima: volume lungo** (sul display, durante l'ascolto, c'è il promemoria "VOL lungo = esci"); se in quel momento la scheda sta ancora trasmettendo, il ritorno avviene appena l'invio è finito.

**Durante l'ascolto la scheda non sente la voce del progetto** (una radio ascolta un solo insieme di parametri alla volta). Le
risposte che arrivano **dopo** la finestra non si sentono: serve un altro dispositivo MeshCom, oppure riaprire l'ascolto dal menu.
L'escursione parte solo a riposo (non durante una ricezione, una trasmissione o una registrazione). Se l'invio è rifiutato (canale
occupato, nessun destinatario, nominativo N0CALL) la radio torna subito com'era.

## Impostazioni

| Impostazione | Comando | Note |
|---|---|---|
| Destinatari | `CFG SET MCDEST1 22251` … `MCDEST5` | **un gruppo** (numero: 22251, 222…), **un nominativo-SSID** (IU5ABC-9) o **`*`** per tutti; `NONE` svuota il posto; di base `22251`, `222`, `*`; valgono subito |
| SSID | `CFG SET MCSSID 99` | 0-99, 0 = nessuno; il tuo nominativo in rete è `IU5HJU-99` |
| Ascolto dopo l'invio | `CFG SET MCWINDOW 300` | secondi, 30-1800 |
| Frequenza | `CFG SET FREQMC 433.175` | MHz, 430.000-440.000 |
| Lettura | `CFG GET MC`, `CFG GET MCDEST1` | `CFG MC freqmc=433.175 ssid=99 window=300`, `CFG MCDEST1=22251` |

Le stesse impostazioni sono nella pagina di configurazione sul sito e nella pagina Wi-Fi della scheda. **Altre stazioni possono usare lo
stesso SSID** (per esempio `-99`): scegline uno tuo.

## Firma automatica

Ogni testo inviato sulla rete porta in coda **`(Inviato da dispositivo VoLoRa)`**: serve a far capire a chi legge che il messaggio non
viene da un nodo MeshCom ma da un dispositivo del progetto (per correttezza, e perché sia chiaro dove cercare in caso di problemi). La
firma si aggiunge da sola: se il testo la contiene già in fondo (come il primo testo predefinito) non si ripete, e se il testo è troppo
lungo si accorcia il testo, non la firma (150 caratteri in tutto). Vale per i testi dell'escursione MeshCom, non per "Invia testo" nel
nostro formato.

## Testi e segnaposti

Sono i cinque testi predefiniti (gli stessi di "Invia testo": [firmware.md](firmware.md)). Prima dell'invio si sostituiscono i
**segnaposti**:

| Segnaposto | Diventa |
|---|---|
| `%C` | il tuo nominativo (`IU5HJU`) |
| `%M` | la lettera del modo del profilo voce in uso: `S` Standard, `M` Messaggio, `Q` Qualità |
| `%F` | la frequenza del profilo voce, in MHz (`433.900`) |
| `%%` | il segno `%` |

Per esempio `%C QRV su VoLora modo %M %F` diventa `IU5HJU QRV su VoLora modo Q 433.900`.

**Limiti:** al massimo **150 caratteri** dopo la sostituzione, ASCII stampabile, **senza le parentesi graffe** `{` `}`. Il frame
massimo è 255 byte e il percorso cresce di circa 10 byte a ogni salto; le graffe hanno un significato di servizio sulla rete
(`{NNN` in fondo chiede la conferma di ricezione). In ricezione i caratteri oltre l'ASCII diventano `?`, perché il display non li
disegna, e un testo più lungo di 150 caratteri è tagliato.

## Ricezione e conferme

Si leggono i testi con la FCS giusta e mai visti prima (le copie ripetute dai nodi hanno lo stesso ID e si scartano con una memoria di
70 ID, come fanno i nodi) che sono:

- **diretti a te**: al nominativo-SSID (`IU5HJU-99`) o al solo nominativo (`IU5HJU`, perché alcuni client scrivono senza SSID);
- **di un gruppo** (un numero) tra i tuoi destinatari impostati. Non di tutti i gruppi: solo di quelli.

Non si mostrano i testi per altri nominativi, quelli a `*`, i messaggi di servizio della rete (per esempio l'ora, che inizia con `{`),
le posizioni, i rapporti dei nodi e le conferme dei tuoi testi. Se chi scrive **a te** chiede la conferma (il suo testo finisce con
`{NNN`) la scheda risponde dopo un attimo con `MITTENTE:ackNNN`, come un nodo; per i gruppi non si risponde. La nostra scheda **non**
chiede la conferma dei propri testi.

**Diagnostica:** durante l'escursione la seriale (115200) scrive una riga per ogni frame ricevuto, con il motivo se non viene
mostrato: `[meshcom] : da IU5ABC-9 a IU5HJU-99: MOSTRATO`, `... scartato, non e' per noi (IU5HJU-99)`, `... copia gia' vista`,
`... messaggio di servizio`, `... CONFERMA ackNNN INVIATA` (la risposta che la scheda manda a chi ha chiesto la conferma), `... FCS sbagliata`, `frame non valido (... byte)`. A fine escursione una riga di riepilogo (`fine escursione: 12 frame ricevuti (testi 10, posizioni e rapporti 0, FCS sbagliata 1, non validi 1)`) distingue "non ha sentito niente" da "ha sentito ma scartato". Serve a capire perché una risposta non compare.

## Formato dei frame

| Campo | Byte | Note |
|---|---|---|
| tipo | 1 | `:` testo (altri tipi della rete si leggono ma non si trasmettono) |
| ID del messaggio | 4 | casuale a 32 bit, byte basso per primo; i nodi scartano gli ID già visti |
| flag e salti | 1 | `0x10` mesh + 4 salti (`0x14`) per un testo nato qui |
| percorso | variabile | `MITTENTE[,NODO,...]` fino a `>`; i nodi aggiungono il proprio nominativo |
| destinatario | variabile | fino a `:`; un gruppo (numero), un nominativo-SSID o `*` |
| testo | variabile | fino al byte `0x00` |
| HW, MOD | 1 + 1 | identificatore della scheda (T-Beam 12, T3 3), modulazione `0x88` |
| FCS | 2 | somma a 16 bit di tutti i byte dal tipo a MOD compreso, byte alto per primo |

Il frame non ha la parte finale con la versione del firmware (facoltativa): non siamo il firmware della rete. Un testo di 100
caratteri dura circa 1,2 s in aria (i tempi calcolati coincidono con quelli misurati da un nodo vero: 43 byte 0,56 s, 58 byte 0,71 s,
106 byte 1,15 s). Codice: `lib/voicecore/meshcom_frame.h`, logica in `lib/voicecore/voice_node.cpp` (`startMeshcom`), cambio dei
parametri radio in `src/radio_lora.cpp` (`setMeshcom`), segnaposti in `lib/voicecore/text_template.*`, prove in
`tests/test_meshcom_frame.cpp`, `tests/test_node.cpp` e `tests/test_text_template.cpp`.

## Uso responsabile

Questa rete è di altri radioamatori: prima di trasmettere **accordati con chi gestisce la rete della tua zona**, usa il tuo
nominativo, pochi messaggi brevi e un destinatario preciso. Un messaggio a un gruppo o a `*` viene ripetuto da molti nodi: usalo con
parsimonia. La scheda non ripete i frame degli altri e non mette nulla in rete da sola: trasmette solo quando scegli un testo dal
menu, o per rispondere a una richiesta di conferma durante l'ascolto.
