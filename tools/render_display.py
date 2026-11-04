#!/usr/bin/env python3
"""Genera il mockup dell'OLED (docs/images/display.png).

Riproduce pixel per pixel quello che disegna src/ui.cpp su un SSD1306 128x64:
stesso font 5x7 di Adafruit_GFX (file glcdfont.c), stesse coordinate e stesso
ritorno a capo automatico. Quando cambia il layout in src/ui.cpp aggiornare
le funzioni draw_screen() e show_message() qui sotto e rigenerare.

Uso:  python3 tools/render_display.py PERCORSO/glcdfont.c [uscita.png]
Il file glcdfont.c si trova nel repository adafruit/Adafruit-GFX-Library.
Richiede Pillow.
"""
import re
import sys

from PIL import Image, ImageDraw, ImageFont

W, H = 128, 64
CALLSIGN = "IU5HJU"
FREQ = "433.900"


def load_font(path):
    text = open(path, encoding="utf-8").read()
    body = text[text.index("font[] PROGMEM = {"):]
    body = body[: body.index("};")]
    data = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", body)]
    if len(data) < 256 * 5:
        raise SystemExit("font non valido: servono 1280 byte")
    return data


class Oled:
    """Modello minimo di Adafruit_GFX su un buffer 128x64 (testo e linea orizzontale)."""

    def __init__(self, font):
        self.font = font
        self.px = [[False] * W for _ in range(H)]
        self.cx = self.cy = 0
        self.size = 1
        self.inverse = False

    def set_size(self, n):
        self.size = n

    def set_cursor(self, x, y):
        self.cx, self.cy = x, y

    def _char(self, c):
        code = ord(c)
        if code > 255:
            code = ord("?")
        for i in range(5):
            line = self.font[code * 5 + i]
            for j in range(8):
                if line & 1:
                    for dx in range(self.size):
                        for dy in range(self.size):
                            x = self.cx + i * self.size + dx
                            y = self.cy + j * self.size + dy
                            if 0 <= x < W and 0 <= y < H:
                                self.px[y][x] = not self.inverse
                line >>= 1

    def write(self, s, newline=False):
        for c in s:
            # stesso comportamento di Adafruit_GFX::write con textwrap attivo
            if self.cx + self.size * 6 > W:
                self.cx = 0
                self.cy += self.size * 8
            self._char(c)
            self.cx += self.size * 6
        if newline:
            self.cx = 0
            self.cy += self.size * 8

    def fill_rect(self, x, y, w, h):
        for yy in range(h):
            for xx in range(w):
                if 0 <= x + xx < W and 0 <= y + yy < H:
                    self.px[y + yy][x + xx] = True

    def hline(self, x, y, w):
        for i in range(w):
            if 0 <= x + i < W and 0 <= y < H:
                self.px[y][x + i] = True


def show_message(font, line1, line2=None, callsign=CALLSIGN):
    """Come UiNotifier::showMessage()."""
    o = Oled(font)
    o.set_size(1)
    o.set_cursor(0, 0)
    o.write(callsign, newline=True)
    o.set_size(2)
    o.set_cursor(0, 20)
    o.write(line1, newline=True)
    if line2:
        o.set_size(1)
        o.set_cursor(0, 52)
        o.write(line2, newline=True)
    return o


def draw_screen(font, state="idle", rx_call="", rssi=0, snr=0.0, tx_seconds=0, notice=None,
                last=None, volume=7, callsign=CALLSIGN, battery=4.12):
    """Come UiNotifier::draw(). last = (nominativo, persi, recuperati) oppure None."""
    o = Oled(font)
    o.set_size(1)
    o.set_cursor(0, 0)
    o.write(f"{callsign}  {FREQ} MHz")
    o.hline(0, 10, 128)

    o.set_size(2)
    o.set_cursor(0, 16)
    if state == "idle":
        o.write("ASCOLTO")
    elif state == "rx":
        o.write(f"RX {rx_call}")
    else:
        o.write(f"TX {callsign}")

    o.set_size(1)
    o.set_cursor(0, 38)
    if notice:
        o.write(notice)
    elif state == "tx":
        o.write(f"Trasmissione {tx_seconds}/30 s")
    elif state == "rx":
        o.write(f"RSSI {rssi}  SNR {snr:.1f}")
    elif last:
        o.write(f"Ultimo {last[0] or '-'}")
        o.set_cursor(0, 47)  # come src/ui.cpp
        o.write(f"persi {last[1]}  recup. {last[2]}")
    else:
        o.write("Premi PTT per parlare")

    o.set_cursor(0, 56)
    o.write(f"Vol {volume}/10")

    # Batteria in basso a destra, come src/ui.cpp: "(Batt=4.12V)"; se scarica, testo in negativo.
    txt = "(Batt=--)" if battery < 0 else f"(Batt={battery:.2f}V)"
    bx = W - 6 * len(txt)
    if 0 <= battery < 3.40:
        o.fill_rect(bx - 1, 55, W - bx + 1, 9)
        o.inverse = True
    o.set_cursor(bx, 56)
    o.write(txt)
    o.inverse = False
    return o


def screens(font):
    return [
        ("1  Avvio", "Appena accesa, durante l'inizializzazione.",
         show_message(font, "AVVIO")),
        ("2  A riposo", "Pronta: in ascolto, nessuna ricezione precedente.",
         draw_screen(font)),
        ("3  Ricezione", "Un'altra stazione trasmette. RSSI e SNR dell'ultimo pacchetto.",
         draw_screen(font, "rx", "IZ5ABC", -92, 7.5)),
        ("4  Dopo la ricezione", "Fine turno (OVER): ultimo nominativo, frame persi e pacchetti recuperati.",
         draw_screen(font, last=("IZ5ABC", 0, 1))),
        ("5  Trasmissione", "PTT premuto: tempo trascorso sul massimo (TOT).",
         draw_screen(font, "tx", tx_seconds=12)),
        ("6  Limite di tempo", "Il TOT e' scaduto: la trasmissione si chiude da sola.",
         draw_screen(font, notice="LIMITE 30 s")),
        ("7  PTT durante la ricezione", "Il PTT e' rifiutato: l'altra stazione sta parlando.",
         draw_screen(font, "rx", "IZ5ABC", -92, 7.5, notice="STAI RICEVENDO")),
        ("8  Canale occupato", "Il rilevamento del canale ha visto un'altra trasmissione.",
         draw_screen(font, notice="CANALE OCCUPATO")),
        ("9  Dopo una ricezione con perdite", "Frame persi non recuperabili dalla parita'.",
         draw_screen(font, last=("IZ5ABC", 24, 3))),
        ("10  Avvio senza nominativo", "Con N0CALL si riceve ma non si trasmette.",
         show_message(font, "SOLO RX", "Imposta il nominativo", callsign="N0CALL")),
        ("11  PTT senza nominativo", "Con N0CALL il PTT e' rifiutato.",
         draw_screen(font, notice="IMPOSTA NOMINATIVO", callsign="N0CALL")),
        ("12  Errore all'avvio", "Il nome del blocco che non parte (radio, audio I2S, Codec2).",
         show_message(font, "ERRORE", "radio")),
        ("13  Batteria scarica", "Sotto 3,40 V il testo della batteria va in negativo.",
         draw_screen(font, battery=3.28)),
        ("14  Batteria assente", "T-Beam senza batteria: nessun valore da mostrare.",
         draw_screen(font, battery=-1.0)),
    ]


def render_panel(oled, scale=4):
    """OLED a LED: pixel acceso piu' chiaro, griglia scura visibile."""
    bezel, off, on = (28, 32, 40), (10, 14, 22), (205, 235, 255)
    pad = 14
    w, h = W * scale + 2 * pad, H * scale + 2 * pad
    img = Image.new("RGB", (w, h), bezel)
    d = ImageDraw.Draw(img)
    for y in range(H):
        for x in range(W):
            c = on if oled.px[y][x] else off
            x0, y0 = pad + x * scale, pad + y * scale
            d.rectangle([x0, y0, x0 + scale - 2, y0 + scale - 2], fill=c)
    return img


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    font = load_font(sys.argv[1])
    out = sys.argv[2] if len(sys.argv) > 2 else "docs/images/display.png"

    try:
        title_font = ImageFont.load_default(size=22)
        cap_font = ImageFont.load_default(size=16)
    except TypeError:
        title_font = cap_font = ImageFont.load_default()

    items = screens(font)
    cols = 3
    rows = (len(items) + cols - 1) // cols
    panels = [render_panel(o) for _, _, o in items]
    pw, ph = panels[0].size
    margin, cap_h = 30, 74
    sheet_w = cols * pw + (cols + 1) * margin
    sheet_h = rows * (ph + cap_h) + (rows + 1) * margin
    sheet = Image.new("RGB", (sheet_w, sheet_h), (245, 246, 248))
    d = ImageDraw.Draw(sheet)

    for n, ((title, caption, _), panel) in enumerate(zip(items, panels)):
        r, c = divmod(n, cols)
        x = margin + c * (pw + margin)
        y = margin + r * (ph + cap_h + margin)
        sheet.paste(panel, (x, y))
        d.text((x, y + ph + 8), title, fill=(20, 24, 32), font=title_font)
        # didascalia su piu' righe
        words, line, ty = caption.split(), "", y + ph + 38
        for wd in words:
            test = (line + " " + wd).strip()
            if d.textlength(test, font=cap_font) > pw:
                d.text((x, ty), line, fill=(70, 76, 90), font=cap_font)
                ty += 20
                line = wd
            else:
                line = test
        d.text((x, ty), line, fill=(70, 76, 90), font=cap_font)

    sheet.save(out, optimize=True)
    print(f"scritto {out} ({sheet_w}x{sheet_h})")


if __name__ == "__main__":
    main()
