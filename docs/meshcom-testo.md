# Testi su reti MeshCom (profilo MeshCom)

**Stato (11/11/2026): scritto e provato sul PC; non ancora provato in aria con una nostra scheda.** Il formato dei frame è stato
verificato su frame veri ricevuti da un nodo della rete (decodifica, FCS e ricodifica identica); i test del progetto usano frame
di prova costruiti con quello stesso formato e nominativi inventati. Restano da provare in aria: l'invio, la ripetizione da parte
dei nodi e la conferma di ricezione.

Il profilo **MeshCom** fa scambiare alla scheda **solo testi** (stile SMS) con le reti radioamatoriali a pacchetto che lavorano sui
**433,175 MHz** (MeshCom). **La voce non passa da questa rete**: nel profilo MeshCom il PTT non trasmette (compare "SOLO TESTO") e
non c'è registrazione.

## Come si usa

1. **Profilo:** dal menu a bordo (volume lungo → PTT per scorrere → volume breve per confermare) scegli **MeshCom**; la scheda si
   riavvia. Oppure `CFG SET PROFILE MC`, o dalla pagina di configurazione (web o Wi-Fi). Sul display, in alto a destra, compare la
   lettera **T**.
2. **Nominativo e SSID:** il nominativo-SSID usato in rete è il tuo nominativo seguito da `-` e dall'SSID (`CFG SET MCSSID 99` →
   `IU5HJU-99`; di base 99; 0 = nessun SSID). Altre stazioni possono usare lo stesso SSID: scegline uno tuo.
3. **Destinatario:** `CFG SET MCDEST IU5ABC-9` (nominativo-SSID di chi deve ricevere), oppure `*` per scrivere a tutti, oppure `NONE`
   (nessuno: non si invia). **Senza destinatario la scheda non trasmette** ("IMPOSTA DESTINAT."). Usa `*` con parsimonia: un
   messaggio a tutti viene ripetuto da tutti i nodi della zona e pesa sulla rete di altri.
4. **Invio:** menu a bordo → **Invia testo** → PTT per scegliere uno dei cinque testi predefiniti → volume breve per inviare
   ([modo-messaggio.md](modo-messaggio.md)). Il testo parte **una volta sola** (a differenza dei nostri testi, trasmessi due volte)
   e i nodi lo ripetono per te.
5. **Ricezione:** un testo **diretto al tuo nominativo-SSID** compare come un SMS: "SMS" in Morse, la busta sul display e il PTT breve
   per leggerlo; dopo la lettura si cancella ([modo-messaggio.md](modo-messaggio.md)). Il mittente è il nominativo-SSID di chi ha
   scritto. Non si mostrano i testi per altri, quelli a tutti, i messaggi di servizio della rete (per esempio l'ora, che inizia con
   `{`), le posizioni e i rapporti dei nodi.
6. **Conferma di ricezione:** se chi scrive ha chiesto la conferma (il suo testo finisce con `{NNN`), la scheda risponde dopo un
   attimo con `MITTENTE:ackNNN`, come un nodo. La nostra scheda **non** chiede la conferma dei propri testi.

| Impostazione | Comando | Note |
|---|---|---|
| Profilo | `CFG SET PROFILE MC` | vale dal riavvio |
| Frequenza | `CFG SET FREQMC 433.175` | MHz, 430.000-440.000; di base 433.175 |
| Destinatario | `CFG SET MCDEST IU5ABC-9` | `*` = tutti, `NONE` = nessuno; maiuscole indifferenti |
| SSID | `CFG SET MCSSID 99` | 0-99, 0 = nessuno |
| Lettura | `CFG GET MC` | `CFG MC freqmc=433.175 dest=IU5ABC-9 ssid=99` |

Le stesse impostazioni sono nella pagina di configurazione sul sito e nella pagina Wi-Fi della scheda.

## Parametri radio

SF11, banda 250 kHz, CR 4/6, **sync word 0x2B**, preambolo 8, intestazione esplicita, CRC attivo, 433,175 MHz. Sono i parametri che un
nodo della rete mostra con `--info` (paese EU8). Un testo di 100 caratteri dura circa 1,2 s in aria (i tempi calcolati coincidono con
quelli misurati da un nodo vero: 43 byte 0,56 s, 58 byte 0,71 s, 106 byte 1,15 s). **Le schede con un altro profilo non si sentono con
questa rete**, e questa non sente le nostre voci.

## Limiti dei testi, e perché

- **Al massimo 150 caratteri, solo ASCII stampabile, senza le parentesi graffe `{` `}`**: il frame massimo è 255 byte e il percorso
  cresce di circa 10 byte a ogni salto; le graffe hanno un significato di servizio (`{NNN` è la richiesta di conferma). Con questi
  limiti un testo valido da noi lo è anche in rete.
- In ricezione i caratteri oltre l'ASCII (accenti, simboli) diventano `?`, perché il display non li disegna; un testo più lungo di 150
  caratteri è tagliato.

## Formato dei frame

| Campo | Byte | Note |
|---|---|---|
| tipo | 1 | `:` testo (altri tipi della rete si leggono ma non si trasmettono) |
| ID del messaggio | 4 | casuale a 32 bit, byte basso per primo; i nodi scartano gli ID già visti |
| flag e salti | 1 | `0x10` mesh + 4 salti (`0x14`) per un testo nato qui |
| percorso | variabile | `MITTENTE[,NODO,...]` fino a `>`; i nodi aggiungono il proprio nominativo |
| destinatario | variabile | fino a `:`; `*` = tutti |
| testo | variabile | fino al byte `0x00` |
| HW, MOD | 1 + 1 | identificatore della scheda (T-Beam 12, T3 3), modulazione `0x88` |
| FCS | 2 | somma a 16 bit di tutti i byte dal tipo a MOD compreso, byte alto per primo |

Il frame non ha la parte finale con la versione del firmware (facoltativa): non siamo il firmware della rete. Un frame ricevuto
viene accettato solo con la FCS giusta; le copie dello stesso ID (ripetute dai nodi) si scartano con una memoria di 70 ID, come
fanno i nodi, e lo stesso vale per l'eco dei nostri testi. Codice: `lib/voicecore/meshcom_frame.h`, logica in
`lib/voicecore/voice_node.cpp`, prove in `tests/test_meshcom_frame.cpp` e `tests/test_node.cpp`.

## Uso responsabile

Questa rete è di altri radioamatori: prima di trasmettere **accordati con chi gestisce la rete della tua zona**, usa il tuo
nominativo, pochi messaggi brevi e un destinatario preciso (non `*` se non serve). La scheda non ripete i frame degli altri e non
mette nulla in rete da sola: trasmette solo quando scegli "Invia testo" o per rispondere a una richiesta di conferma.
