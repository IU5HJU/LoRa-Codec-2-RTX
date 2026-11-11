# MeshCom: avvisi e accordi sulle reti di testo

**Stato (11/11/2026): scritto e provato sul PC; non ancora provato in aria con una nostra scheda.** Il formato dei frame è stato
verificato su frame veri ricevuti da un nodo della rete (decodifica, FCS e ricodifica identica); i test del progetto usano frame
di prova costruiti con quel formato e nominativi inventati. Restano da provare in aria: il cambio dei parametri radio a caldo,
l'invio, la ripetizione da parte dei nodi e la conferma di ricezione.

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
3. Durante l'ascolto i testi **diretti al tuo nominativo-SSID** finiscono nella casella degli SMS ("SMS" in Morse, la busta sul display,
   PTT breve per leggere: [modo-messaggio.md](modo-messaggio.md)). Restano leggibili anche dopo il rientro. Il PTT lungo non
   trasmette ("SOLO TESTO").
4. Alla scadenza la radio **torna da sola ai parametri del profilo voce** di prima. Nessun riavvio, nessun profilo da cambiare.

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

Si leggono solo i testi **diretti al tuo nominativo-SSID**, con la FCS giusta e mai visti prima: le copie ripetute dai nodi hanno lo
stesso ID e si scartano con una memoria di 70 ID, come fanno i nodi. Non si mostrano i testi per altri, quelli a un gruppo o a tutti,
i messaggi di servizio della rete (per esempio l'ora, che inizia con `{`), le posizioni, i rapporti dei nodi e le conferme dei tuoi
testi. Se chi scrive chiede la conferma (il suo testo finisce con `{NNN`) la scheda risponde dopo un attimo con `MITTENTE:ackNNN`,
come un nodo. La nostra scheda **non** chiede la conferma dei propri testi.

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
