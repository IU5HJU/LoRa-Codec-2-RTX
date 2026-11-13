# Il reflector VoLoRa

13/11/2026. **Scritto, provato sul PC** (logica con client finti, `make reflector-smoke` con veri socket UDP); **mai provato con schede**. Il perché e il confronto con il reflector YSF sono in [studio-reflector.md](studio-reflector.md).

Un reflector è un programma su un server che fa da centro-stella: **nodi e ripetitori si collegano a lui** e quello che uno riceve in radio lo ascoltano tutti gli altri. **Chi si collega non apre nessuna porta sul router**: scrive per primo al reflector e il reflector risponde all'indirizzo da cui è arrivato il datagramma, come nel reflector YSF. Solo il **server** deve avere una porta UDP raggiungibile da Internet (di base la 5400).

## Il server

```
make vlreflector
build/vlreflector --config reflector.conf [--port 5400] [--id 1000] [--voice-mode 4] [--status-every 300]
```

- `--id`: l'identificativo con cui si presenta il reflector (di base 1000): è il `PEERID` dei client.
- `--voice-mode`: il modo voce che inoltra, **4** (Standard, Codec2 3200, di base) o **1** (Range, 1200). Il testo passa sempre; gli altri modi si scartano. Un reflector inoltra un solo modo voce, per questo i client collegati devono usare lo stesso profilo.
- `--status-every`: ogni quanti secondi scrive nel registro lo stato dei client (di base 300).
- Il file di configurazione ha **una riga per client** (righe vuote e che iniziano con `#` si ignorano):
  ```
  # id  chiave (32 cifre esadecimali)       nome
  2     000102030405060708090a0b0c0d0e0f    nodo-firenze
  3     101112131415161718191a1b1c1d1e1f    ripetitore-rx
  ```
  Ogni client ha **la sua chiave** (`openssl rand -hex 16`) e il suo id (diverso da 0 e dall'id del reflector). Per togliere un client si cancella la sua riga e si riavvia il reflector.
- **Firewall del server:** aprire solo la porta UDP scelta in ingresso. Il reflector non risponde a chi non conosce un id e una chiave in elenco (nessuna amplificazione).
- Come servizio (esempio `systemd`): `ExecStart=/opt/vlreflector/vlreflector --config /etc/vlreflector.conf --port 5400`, `Restart=always`.

## Cosa fa

- **Collegamento:** per ogni client lo stesso collegamento firmato (HMAC-SHA256) dei nodi: sfida dopo un riavvio, contatore anti-ripetizione, `KEEPALIVE` ogni 20 s (tiene aperto il NAT del router di casa) e collegamento dato per caduto dopo 60 s. L'indirizzo di un client si aggiorna a ogni datagramma firmato valido: se il router cambia la porta, il client resta lo stesso.
- **Inoltro:** un pacchetto valido (il modo voce del reflector e il testo) va a **tutti gli altri client collegati**, con il flag `RELAYED`, e mai a chi l'ha mandato.
- **Chi parla:** chi comincia per primo tiene il canale fino al pacchetto di fine o a 3 s senza pacchetti; la voce degli altri in quel momento si scarta (e si conta). Il testo passa sempre.
- **Registro:** una riga per client collegato o caduto, per chi comincia a parlare e per chi finisce, e lo stato periodico.

## Collegare un nodo

Sulla scheda (per seriale, poi `CFG REBOOT`), con i valori della riga del file:

```
CFG SET ROLE NODE
CFG SET WIFISSID <rete>        CFG SET WIFIPASS <password>
CFG SET PEER reflector.example.org:5400     (anche un indirizzo IPv4: 203.0.113.7:5400)
CFG SET PEERID 1000            (l'id del reflector)
CFG SET NODEID 2               CFG SET NODEKEY 000102030405060708090a0b0c0d0e0f
```

`PEER` può essere un **nome**: la scheda lo risolve da sola, lo riprova ogni 5 minuti e, se il collegamento non sale, ogni 30 secondi (se il reflector cambia indirizzo la scheda lo segue). La scheda ascolta sulla porta `NODEPORT` (5400 di base), ma **non serve aprirla sul router**: arriva solo quello che è partito da lei.

## Collegare un ripetitore

Il collegamento sta sulla **scheda RX**: le stesse impostazioni del nodo (`WIFISSID`, `WIFIPASS`, `PEER`, `PEERID`, `NODEID`, `NODEKEY`) vanno date alla RX. La TX non cambia e non ha il Wi-Fi.
- **Radio → rete:** quello che la RX ripete (modo del profilo e testo) va anche al reflector.
- **Rete → radio:** i pacchetti del reflector passano per la seriale alla TX, che li trasmette sulla frequenza di uscita.
- **Chi parla per primo tiene l'uscita:** se arriva voce dalla rete, la voce dalla radio si scarta (e viceversa) fino alla fine o a 3 s di silenzio; il testo passa sempre. Sul display della RX la riga "Rete ok N/N" dice se è collegata e quanti pacchetti sono passati; ogni 30 s la seriale scrive i contatori (`[ripetitore] rete ...`).
- Senza queste impostazioni il ripetitore funziona come prima, solo in radio.

## Prova sul PC (senza schede)

`make reflector-smoke`: il reflector e due nodi finti (`vlnode`) su socket UDP veri, con porte scelte dal sistema (nessuna porta nota): il secondo nodo riceve il testo e la voce del primo. A mano:

```
build/vlreflector --config reflector.conf --port 5400 --id 1000 --voice-mode 1
build/vlnode --id 2 --peer-id 1000 --key <chiave> --listen 0 --peer localhost:5400 --send-text IU5HJU "Prova"
build/vlnode --id 3 --peer-id 1000 --key <chiave> --listen 0 --peer localhost:5400
```

## Limiti noti

- Un reflector è **un salone**: tutti i client si sentono. I canali (il campo `canale` dell'intestazione) non sono usati.
- **Solo IPv4**, e un solo modo voce per reflector.
- Il contenuto dei pacchetti **non è cifrato** (come la radio): la firma impedisce di inserire pacchetti, non di leggerli.
- Chi conosce la chiave di un client può fingersi quel client: una chiave per client serve a poterlo escludere senza toccare gli altri.
- Non provato con schede, né con un reflector su Internet vero dietro il NAT di un router di casa.
