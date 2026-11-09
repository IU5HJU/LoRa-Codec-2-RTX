#!/usr/bin/env python3
"""Genera docs/formato-pacchetto.pdf: sintesi in due pagine del formato del pacchetto radio.

    python3 tools/make_formato_pacchetto_pdf.py [uscita.pdf]

Serve reportlab (pip install reportlab). I numeri che dipendono dai parametri radio (tempi in aria, occupazione del
canale) si calcolano qui sotto con la formula di Semtech; la versione del protocollo, i byte per frame, i frame per
pacchetto e il gruppo di parita' si leggono da lib/voicecore/protocol.h e src/user_config.h: se non coincidono con quelli
scritti nel testo il programma si ferma, cosi' il PDF non resta vecchio senza che ce ne si accorga. Il testo
(descrizione dei campi) e' scritto a mano: va riletto se il protocollo cambia, vedi docs/protocollo.md.
"""
import datetime
import math
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def costante(file, nome):
    """Legge un numero da un `constexpr ... nome = N;` o da un `#define nome N`."""
    t = open(os.path.join(ROOT, file), encoding="utf-8").read()
    m = re.search(r"\b" + re.escape(nome) + r"\s*=\s*([0-9.]+)", t) or re.search(r"#define\s+" + re.escape(nome) + r"\s+([0-9.]+)", t)
    if not m:
        sys.exit("costante %s non trovata in %s" % (nome, file))
    v = float(m.group(1))
    return int(v) if v == int(v) else v


PROTO = costante("lib/voicecore/protocol.h", "kProtocolVersion")
FRAME_BYTES = costante("lib/voicecore/protocol.h", "kFrameBytes")
FRAMES_PER_PACKET = costante("lib/voicecore/protocol.h", "kFramesPerPacket")
PARITY_GROUP = costante("lib/voicecore/protocol.h", "kParityGroup")
FRAME_MS = costante("lib/voicecore/protocol.h", "kFrameMs")
GAP_MS = costante("lib/voicecore/protocol.h", "kInterPacketGapMs")
SF = costante("src/user_config.h", "LCR_SPREADING_FACTOR")
BW_KHZ = costante("src/user_config.h", "LCR_BANDWIDTH_KHZ")
CR = costante("src/user_config.h", "LCR_CODING_RATE") - 4          # 4/5 -> 1
PREAMBLE = costante("src/user_config.h", "LCR_PREAMBLE_SYMBOLS")

# Valori che il testo del PDF da' per scontati: se cambiano, il testo va rivisto.
ATTESI = dict(PROTO=2, FRAME_BYTES=6, FRAMES_PER_PACKET=12, PARITY_GROUP=4, FRAME_MS=40, GAP_MS=80, SF=8, BW_KHZ=250, CR=1, PREAMBLE=12)
for k, v in ATTESI.items():
    if globals()[k] != v:
        sys.exit("%s e' %s nel codice ma il testo del PDF assume %s: rileggi e aggiorna il testo prima di rigenerare." % (k, globals()[k], v))


def tempo_in_aria_ms(byte):
    """Formula di Semtech (SX127x), intestazione esplicita, CRC attivo, senza low data rate optimize."""
    ts = 2 ** SF / (BW_KHZ * 1000.0) * 1000.0
    simboli = 8 + max(math.ceil((8 * byte - 4 * SF + 28 + 16) / (4 * SF)) * (CR + 4), 0)
    return (PREAMBLE + 4.25) * ts + simboli * ts, simboli


CS = 6                                       # nominativo di esempio: 6 caratteri
PAYLOAD = FRAMES_PER_PACKET * FRAME_BYTES    # 72
B_START = 5 + 1 + CS
B_DATI = 5 + PAYLOAD
B_DATI_CS = 5 + 1 + CS + PAYLOAD
B_PARITA = 5 + 3 + PAYLOAD
T = {k: tempo_in_aria_ms(b) for k, b in dict(start=B_START, dati=B_DATI, dati_cs=B_DATI_CS, parita=B_PARITA).items()}
VOCE_GRUPPO_MS = PARITY_GROUP * FRAMES_PER_PACKET * FRAME_MS
ARIA_GRUPPO_MS = PARITY_GROUP * T["dati"][0] + T["parita"][0]
CICLO_GRUPPO_MS = ARIA_GRUPPO_MS + (PARITY_GROUP + 1) * GAP_MS

from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, KeepTogether, Preformatted

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "docs", "formato-pacchetto.pdf")
BLU = colors.HexColor("#0b5cd5")
GRIGIO = colors.HexColor("#eef1f5")
LINEA = colors.HexColor("#c9d0da")

ss = getSampleStyleSheet()
base = ParagraphStyle("base", parent=ss["Normal"], fontName="Helvetica", fontSize=10, leading=14, spaceAfter=6)
h1 = ParagraphStyle("h1", parent=base, fontName="Helvetica-Bold", fontSize=20, leading=24, spaceAfter=4, textColor=colors.HexColor("#14181f"))
sub = ParagraphStyle("sub", parent=base, fontSize=11, textColor=colors.HexColor("#4a5568"), spaceAfter=10)
h2 = ParagraphStyle("h2", parent=base, fontName="Helvetica-Bold", fontSize=13, leading=16, spaceBefore=12, spaceAfter=6, textColor=BLU)
small = ParagraphStyle("small", parent=base, fontSize=8.5, leading=11, textColor=colors.HexColor("#4a5568"))
cell = ParagraphStyle("cell", parent=base, fontSize=9, leading=11.5, spaceAfter=0)
cellb = ParagraphStyle("cellb", parent=cell, fontName="Helvetica-Bold")
mono = ParagraphStyle("mono", parent=cell, fontName="Courier", fontSize=8.8, leading=11.5)
code = ParagraphStyle("code", parent=base, fontName="Courier", fontSize=8.6, leading=11.5, backColor=GRIGIO, borderPadding=6, spaceBefore=4, spaceAfter=10)

def P(t, s=cell): return Paragraph(t, s)

def table(rows, widths, header=True, mono_first=False):
    data = []
    for i, r in enumerate(rows):
        row = []
        for j, c in enumerate(r):
            st = cellb if (header and i == 0) else (mono if (mono_first and j == 0) else cell)
            row.append(P(c, st))
        data.append(row)
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [
        ("GRID", (0, 0), (-1, -1), 0.5, LINEA),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("LEFTPADDING", (0, 0), (-1, -1), 6), ("RIGHTPADDING", (0, 0), (-1, -1), 6),
    ]
    if header: style.append(("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#e8f0fd")))
    t.setStyle(TableStyle(style))
    return t

def footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 8)
    canvas.setFillColor(colors.HexColor("#4a5568"))
    canvas.drawString(20 * mm, 12 * mm, "LoRa Codec 2 RTX - protocollo versione %d - IU5HJU - %s" % (PROTO, datetime.date.today().strftime("%d/%m/%Y")))
    canvas.drawRightString(A4[0] - 20 * mm, 12 * mm, "Pagina %d" % doc.page)
    canvas.restoreState()

doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=20*mm, rightMargin=20*mm, topMargin=18*mm, bottomMargin=20*mm,
                        title="LoRa Codec 2 RTX: formato del pacchetto radio", author="IU5HJU",
                        subject="Come viene trasmessa la voce Codec2 su LoRa a 433 MHz")
W = A4[0] - 40 * mm
s = []

s.append(P("LoRa Codec 2 RTX", h1))
s.append(P("Come viene trasmesso un pacchetto via radio: formato, sequenza e livello LoRa (protocollo versione %d)" % PROTO + "", sub))

s.append(P("In breve", h2))
s.append(P("La voce, compressa con <b>Codec2 a 1200 bit/s</b> (6 byte ogni 40 ms), viaggia in piccoli pacchetti <b>broadcast</b> sopra una normale modulazione <b>LoRa a 433 MHz</b>. "
           "Non ci sono indirizzi del destinatario, non c'è ACK, non ci sono ritrasmissioni e non c'è cifratura: chiunque sia in ascolto sulla frequenza decodifica. "
           "L'unica protezione contro la perdita di pacchetti è una <b>parità XOR</b> ogni quattro pacchetti dati.", base))

s.append(P("1. Livello radio (LoRa)", h2))
s.append(table([
    ["Parametro", "Valore"],
    ["Frequenza", "433,900 MHz (predefinita, modificabile da configurazione)"],
    ["Spreading factor / banda", "SF8, 250 kHz"],
    ["Coding rate", "4/5"],
    ["Sync word", "0x4C"],
    ["Preambolo", "12 simboli"],
    ["Intestazione LoRa", "esplicita"],
    ["CRC LoRa", "attivo (un pacchetto con CRC errato viene scartato)"],
    ["Potenza", "10 dBm predefinita, impostabile"],
    ["Chip", "SX1278 (LILYGO LoRa32 T3 V1.6.1 e T-Beam V1.2)"],
], [48*mm, W - 48*mm]))
s.append(Spacer(1, 4))
s.append(P("Un simbolo dura 2<super>8</super> / 250 kHz = 1,024 ms.", small))

s.append(P("2. Formato del pacchetto", h2))
s.append(P("Quello che il chip trasmette come payload LoRa è composto così:", base))
s.append(Preformatted(
"byte 0      versione (4 bit alti, = 2) | modo (4 bit bassi)\n"
"byte 1-2    ID di sessione, casuale per ogni PTT (big endian)\n"
"byte 3      numero di sequenza (0..127)\n"
"byte 4      flag\n"
"[byte 5     lunghezza del nominativo N (1..8)]    solo se flag CALLSIGN\n"
"[N byte     nominativo ASCII: A-Z 0-9 /]          solo se flag CALLSIGN\n"
"[7 byte     posizione]                            solo se flag POSITION (solo nel pacchetto START)\n"
"resto       payload", code))
s.append(table([
    ["Campo", "Significato"],
    ["Modo", "1 = voce Codec2 1200 bit/s; 2 = prova di collegamento (frame noti); 3 = risposta alla prova (12 byte con il risultato)"],
    ["Flag", "bit 0 START (primo pacchetto) - bit 1 END (ultimo dati, l'OVER) - bit 2 PARITY (pacchetto di parità) - bit 3 CALLSIGN (c'è il nominativo) - bit 4 NOFEC (trasmissione senza parità) - bit 5 POSITION (c'è la posizione, 7 byte: stato, latitudine e longitudine a 24 bit; solo in START, spenta di default)"],
    ["Sessione", "numero casuale a 16 bit: identifica la trasmissione e permette di scartare i pacchetti di altre trasmissioni"],
    ["Sequenza", "0..127; i pacchetti di parità occupano le posizioni 4, 9, 14... del flusso"],
    ["Nominativo", "in chiaro; presente in START, in END e ogni 10 pacchetti (circa 5 secondi), così chi si sintonizza a trasmissione iniziata lo ottiene comunque"],
], [28*mm, W - 28*mm]))

_pd_h = P("Pacchetto dati", base.clone("pd", spaceBefore=8, fontName="Helvetica-Bold", keepWithNext=1))
s.append(_pd_h)
s.append(P("Il payload è formato da <b>12 frame Codec2 da 6 byte = 72 byte = 480 ms di voce</b> (nell'ultimo pacchetto possono essere meno, sempre multipli di 6). "
           "Dimensione totale: <b>77 byte</b> senza nominativo, circa <b>84 byte</b> con un nominativo di 6 caratteri.", base))
s.append(P("Pacchetto di parità", base.clone("pp", fontName="Helvetica-Bold", keepWithNext=1)))
s.append(P("Segue ogni gruppo di 4 pacchetti dati. Il payload (3 + 72 byte, in tutto <b>80 byte</b>) è <font face='Courier'>[quanti pacchetti][XOR dei conteggi di frame][XOR dei flag][XOR dei dati]</font>. "
           "Se in un gruppo se ne perde <b>uno solo</b>, il ricevitore lo ricostruisce; con due persi nello stesso gruppo restano due buchi da 480 ms.", base))

s.append(P("3. Sequenza di una trasmissione", h2))
s.append(Preformatted(
"seq 0       START + nominativo                  (nessun frame audio)\n"
"seq 1-3     dati                                (12 frame ciascuno)\n"
"seq 4       parità di seq 0..3\n"
"seq 5-8     dati\n"
"seq 9       parità                             ...\n"
"ultimo      END + nominativo, poi la parità del gruppo parziale", code))
s.append(table([
    ["Aspetto", "Valore"],
    ["Durata massima di un turno", "30 secondi (750 frame, circa 80 pacchetti con la parità)"],
    ["Pausa tra due pacchetti", "80 ms (il ricevitore deve rileggere la radio prima del successivo)"],
    ["Pre-buffer in ricezione", "25 frame (1 secondo) prima di iniziare l'ascolto"],
    ["Compatibilità", "due schede devono avere la stessa <b>versione del protocollo</b> (oggi 2); i modi sconosciuti vengono scartati"],
], [52*mm, W - 52*mm]))

s.append(P("4. Tempi in aria (stima con la formula di Semtech)", h2))
s.append(table([
    ["Pacchetto", "Byte", "Simboli", "Durata"],
    ["START con nominativo di %d caratteri" % CS, str(B_START), str(T["start"][1]), "%.0f ms" % T["start"][0]],
    ["Dati senza nominativo", str(B_DATI), str(T["dati"][1]), "%.0f ms" % T["dati"][0]],
    ["Dati con nominativo di %d caratteri" % CS, str(B_DATI_CS), str(T["dati_cs"][1]), "%.0f ms" % T["dati_cs"][0]],
    ["Parità", str(B_PARITA), str(T["parita"][1]), "%.0f ms" % T["parita"][0]],
], [78*mm, 20*mm, 24*mm, W - 122*mm]))
s.append(Spacer(1, 4))
s.append(P("Un gruppo di %d pacchetti dati e 1 di parità porta %s s di voce. In aria la radio trasmette circa %.0f ms (<b>%.0f%%</b> del tempo reale della voce); "
           "con le %d pause da %d ms il ciclo completo dura circa %.0f ms (<b>%.0f%%</b>). Resta quindi margine perché la trasmissione tenga il passo della voce in tempo reale. "
           "Sono stime con la formula di Semtech, non misure." % (PARITY_GROUP, ("%.2f" % (VOCE_GRUPPO_MS / 1000.0)).replace(".", ","), ARIA_GRUPPO_MS, ARIA_GRUPPO_MS / VOCE_GRUPPO_MS * 100,
                                                                    PARITY_GROUP + 1, GAP_MS, CICLO_GRUPPO_MS, CICLO_GRUPPO_MS / VOCE_GRUPPO_MS * 100), base))

s.append(P("5. Cosa non c'è", h2))
s.append(P("<b>Nessun indirizzamento</b> (broadcast puro), <b>nessun ACK</b> e nessuna ritrasmissione, <b>nessuna cifratura</b> (il nominativo e il contenuto viaggiano in chiaro) "
           "e nessun controllo di flusso oltre alla pausa tra pacchetti. Un frame perso viene oggi riprodotto come silenzio.", base))

s.append(P("6. Riferimenti", h2))
s.append(P("Progetto: https://github.com/IU5HJU/LoRa-Codec-2-RTX - Pagina di installazione: https://iu5hju.github.io/LoRa-Codec-2-RTX/ - "
           "Descrizione completa: <font face='Courier'>docs/protocollo.md</font>. Il decodificatore di riferimento per catture con un SDR è <font face='Courier'>tools/lcr_decode.cpp</font>. "
           "Licenza GPL-3.0.", base))

doc.build(s, onFirstPage=footer, onLaterPages=footer)
print("ok")
