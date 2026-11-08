# Numerazione delle versioni

Il progetto usa la **numerazione semantica** (`vMAGGIORE.MINORE.CORREZIONE`) con etichette di fase dopo il trattino. Non si usa lo schema "dispari = sviluppo, pari = stabile" del vecchio kernel Linux: obbligava a mantenere due rami
e confondeva chi arriva da fuori (la fase di una versione non si capisce dal numero).

| Versione | Significato |
|---|---|
| `v0.1.0-beta.1`, `-beta.2` … | **beta**: funziona, ma è ancora in prova sul campo. La prima beta è la prima versione in cui la voce passa davvero da una scheda all'altra |
| `v0.1.0-rc.1`, `-rc.2` … | **release candidate**: la versione che si pensa sia quella buona; se non emergono difetti diventa la stabile senza modifiche |
| `v0.1.0` | **stabile** |
| `v0.1.1` | solo correzioni di difetti, nessuna funzione nuova |
| `v0.2.0` | funzioni nuove (per esempio il TOT fino a 3 minuti con i profili) |
| `v1.0.0` | protocollo e funzioni stabili e provati sul campo |

Il numero che sale è sempre più recente; la fase si legge dall'etichetta. Finché la maggiore è `0` ci si riserva di cambiare anche funzioni già presenti.

## Dove si vede

- **Sulla scheda:** `CFG VERSION` risponde `CFG VERSION fw=v0.1.0-beta.1 board=t3 proto=1`, e all'avvio la seriale stampa `firmware ...`. Fra un tag e l'altro il firmware compilato da git mostra, per esempio, `v0.1.0-beta.1-3-gabc1234` (3 commit dopo il tag) oppure solo il codice del commit prima del primo tag; `dev` se compilato senza git.
- **Sulla pagina web:** la versione pubblicata (`v0.1.0-beta.1`, `main-GG/MM/AAAA` per lo sviluppo, con la data della pubblicazione in ora italiana) con la fase scritta accanto ("BETA: versione di prova", "RC", "versione di sviluppo"), e la stessa etichetta per il firmware letto dalla scheda collegata.
- **Perché la pagina può mostrare `main-GG/MM/AAAA` anche dopo la beta:** la pagina pubblica sempre l'**ultima** versione, che dopo ogni modifica di `main` torna a essere quella di sviluppo. La beta resta ferma nella sua release. La pagina lo spiega sotto la versione ("Quale versione sto installando?") e indica come installare esattamente una versione rilasciata (file `web_factory.bin` della release, scritto a `0x0` con `esptool.py`).
- **Su GitHub:** una release per ogni tag `v*`, con i binari. I tag con un trattino (`-beta`, `-rc`) sono marcati **pre-release**, quindi non diventano la "ultima versione" per chi non vuole le beta.

## Firmware e protocollo sono due numeri diversi

La **versione del protocollo** (oggi `1`, nel primo byte di ogni pacchetto, [protocollo.md](protocollo.md)) dice se due schede si capiscono. Due schede devono avere lo **stesso protocollo**, non per forza lo stesso firmware.
Un cambio che rompe la compatibilità dei pacchetti (per esempio il TOT a 3 minuti con numeri di sequenza più grandi) **fa salire il protocollo**, e va scritto in evidenza nelle note del rilascio: da quel momento le schede vanno aggiornate tutte. Il protocollo si legge con `CFG VERSION` (`proto=`).

## Come si pubblica una versione

Dopo aver verificato che la versione fa quello che dice (per la prima beta: la voce passa tra due schede, ascoltata e comprensibile), dal ramo `main` con Actions verde:

```bash
git tag -a v0.1.0-beta.1 -m "Prima beta: la voce passa da una scheda all'altra"
git push origin v0.1.0-beta.1
```

Il workflow compila le due schede, crea la release di GitHub con `web_factory.bin` e `ota_update.bin` (pre-release se il tag ha un trattino) e pubblica la pagina con quella versione. Il numero si scrive **solo nel tag**: non va modificato nessun file.
Poi si annotano nelle [prove](prove.md) le schede e le condizioni della verifica.

## Limiti per ora

- La pagina web pubblica sempre l'ultima versione (un tag o `main`): un canale "stabile" separato da quello "di sviluppo" non c'è ancora. Se serve, si aggiunge quando esisterà una versione stabile.
- Prima della `v0.1.0-beta.1` il firmware ha solo il codice del commit come versione.
