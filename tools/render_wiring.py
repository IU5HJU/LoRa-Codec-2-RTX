#!/usr/bin/env python3
"""Genera gli schemi dei collegamenti hardware delle schede supportate.

Uscita: SVG (vettoriali, modificabili). I pin sono quelli di src/board_pins_t3_v161.h,
src/board_pins_tbeam_v12.h e docs/pinout.md: se cambiano, aggiornare qui e rigenerare.

Uso:  python3 tools/render_wiring.py            # crea entrambi
      python3 tools/render_wiring.py t3         # solo docs/images/collegamenti-t3.svg
      python3 tools/render_wiring.py tbeam      # solo docs/images/collegamenti-tbeam.svg
Per ottenere un PNG basta aprire l'SVG in un browser e salvarlo come immagine,
oppure convertirlo con un qualsiasi strumento (Inkscape, rsvg-convert, ...).
"""
import sys
from xml.sax.saxutils import escape

W, H = 1700, 1150
FONT = "DejaVu Sans, Arial, Helvetica, sans-serif"

C = {
    "bclk": "#2563eb", "ws": "#16a34a", "mic": "#7c3aed", "din": "#db2777", "sd": "#0d9488",
    "ptt": "#ea580c", "vol": "#ca8a04", "spk": "#92400e",
    "5v": "#dc2626", "3v3": "#b45309", "gnd": "#111827", "grey": "#6b7280",
}

els = []


def add(s):
    els.append(s)


def text(x, y, t, size=14, anchor="start", weight="normal", fill="#111827"):
    add(f'<text x="{x}" y="{y}" font-family="{FONT}" font-size="{size}" font-weight="{weight}" '
        f'text-anchor="{anchor}" fill="{fill}">{escape(t)}</text>')


def rect(x, y, w, h, fill, stroke="none", rx=8, sw=2):
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')


def dot(x, y, color, r=7, stroke="#ffffff"):
    add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}" stroke="{stroke}" stroke-width="2"/>')


def wire(points, color, width=4):
    """Filo con un alone bianco: dove incrocia un filo disegnato prima sembra scavalcarlo."""
    pts = " ".join(f"{x},{y}" for x, y in points)
    add(f'<polyline points="{pts}" fill="none" stroke="#ffffff" stroke-width="{width + 7}" stroke-linejoin="round"/>')
    add(f'<polyline points="{pts}" fill="none" stroke="{color}" stroke-width="{width}" '
        f'stroke-linejoin="round" stroke-linecap="round"/>')


def tag(x, y, label, color):
    """Etichetta di alimentazione/massa accanto a un pin (invece di un filo)."""
    w = 14 + 9 * len(label)
    rect(x - w, y - 12, w, 24, color, rx=12, sw=0)
    text(x - w / 2, y + 5, label, 13, "middle", "bold", "#ffffff")
    add(f'<line x1="{x}" y1="{y}" x2="{x + 20}" y2="{y}" stroke="{color}" stroke-width="3"/>')


def header(title, subtitle):
    rect(0, 0, W, H, "#ffffff", rx=0, sw=0)
    text(40, 44, title, 28, weight="bold")
    text(40, 72, subtitle, 15, fill="#4b5563")


def notes_panel(notes):
    y0 = 940
    rect(40, y0 - 24, W - 80, 196, "#f9fafb", "#e5e7eb", rx=10)
    for i, n in enumerate(notes):
        text(60, y0 + 8 + i * 30, n, 14, fill="#374151" if i < len(notes) - 1 else "#6b7280")


def speaker(mx1, y_plus=250, y_minus=330):
    """Altoparlante a destra dell'amplificatore e relativi fili."""
    wire([(mx1, y_plus), (1530, y_plus)], C["spk"], 5)
    wire([(mx1, y_minus), (1530, y_minus)], C["spk"], 5)
    t = y_plus - 50  # il riquadro segue l'altezza dei fili
    rect(1530, t, 130, 180, "#fffbeb", "#d97706", rx=10)
    text(1595, t + 22, "Altoparlante", 15, "middle", "bold", "#92400e")
    text(1595, t + 40, "8 ohm 3 W", 13, "middle", fill="#92400e")
    add(f'<polygon points="1560,{t + 100} 1585,{t + 100} 1610,{t + 76} 1610,{t + 124} 1585,{t + 100}" fill="#92400e"/>')
    add(f'<path d="M1625,{t + 88} q14,12 0,24 M1636,{t + 78} q22,22 0,44" fill="none" stroke="#92400e" '
        'stroke-width="3" stroke-linecap="round"/>')
    dot(1530, y_plus, C["spk"])
    dot(1530, y_minus, C["spk"])
    text(1546, y_plus + 4, "+", 16, weight="bold", fill="#92400e")
    text(1546, y_minus + 4, "-", 16, weight="bold", fill="#92400e")


def amp_box(mx0, mx1, pins, y0, y1, out_plus=250, out_minus=330):
    rect(mx0, y0, mx1 - mx0, y1 - y0, "#f3f4f6", "#374151", rx=10)
    text(mx0, y0 - 26, "MAX98357A", 20, weight="bold")
    text(mx0, y0 - 8, "amplificatore I2S classe D", 14, fill="#4b5563")
    for name, y in pins.items():
        text(mx0 + 18, y + 5, name, 14, weight="bold")
        col = {"SD": C["sd"], "DIN": C["din"], "BCLK": C["bclk"], "LRC": C["ws"]}.get(name, C["grey"])
        dot(mx0, y, col)
    tag(mx0 - 20, pins["VIN"], "5V", C["5v"])
    tag(mx0 - 20, pins["GND"], "GND", C["gnd"])
    rect(mx0 - 118, pins["GAIN"] - 12, 98, 24, "#e5e7eb", rx=12, sw=0)
    text(mx0 - 69, pins["GAIN"] + 5, "libero", 13, "middle", "bold", "#374151")
    add(f'<line x1="{mx0 - 20}" y1="{pins["GAIN"]}" x2="{mx0}" y2="{pins["GAIN"]}" stroke="#9ca3af" stroke-width="3"/>')
    text(mx1 - 18, out_plus + 4, "OUT +", 14, "end", "bold")
    text(mx1 - 18, out_minus + 4, "OUT -", 14, "end", "bold")
    dot(mx1, out_plus, C["spk"])
    dot(mx1, out_minus, C["spk"])


def mic_box(mx0, mx1, pins, y0, y1, vdd_label="3,3 V"):
    rect(mx0, y0, mx1 - mx0, y1 - y0, "#f3f4f6", "#374151", rx=10)
    text(mx0, y0 - 26, "INMP441", 20, weight="bold")
    text(mx0, y0 - 8, "microfono I2S (modulo a 6 pin)", 14, fill="#4b5563")
    for name, y in pins.items():
        text(mx0 + 18, y + 5, name, 14, weight="bold")
        col = {"SCK": C["bclk"], "WS": C["ws"], "SD": C["mic"]}.get(name, C["grey"])
        dot(mx0, y, col)
    tag(mx0 - 20, pins["L/R"], "GND", C["gnd"])
    tag(mx0 - 20, pins["VDD"], vdd_label, C["3v3"])
    tag(mx0 - 20, pins["GND"], "GND", C["gnd"])


def button(x_left, y, label, key):
    """Pulsante con un terminale a GND (il filo arriva da sinistra a x_left)."""
    text(x_left - 28, y - 22, label, 13, weight="bold", fill=C[key])
    dot(x_left, y, "#ffffff", 6, C[key])
    add(f'<line x1="{x_left}" y1="{y}" x2="{x_left + 45}" y2="{y - 18}" stroke="#111827" stroke-width="3" stroke-linecap="round"/>')
    dot(x_left + 60, y, "#ffffff", 6, C["gnd"])
    add(f'<line x1="{x_left + 66}" y1="{y}" x2="{x_left + 92}" y2="{y}" stroke="{C["gnd"]}" stroke-width="3"/>')
    rect(x_left + 92, y - 12, 56, 24, C["gnd"], rx=12, sw=0)
    text(x_left + 120, y + 5, "GND", 13, "middle", "bold", "#ffffff")


# =============================================================================== LoRa32 T3
def draw_t3():
    global els
    els = []
    header("LoRa32 T3 V1.6.1: collegamenti audio e pulsanti",
           "Scheda vista con l'USB in alto. I nomi dei pin sono quelli stampati sulla scheda.")

    BX0, BX1, BY0, BY1 = 220, 580, 100, 900
    PIN_Y = [170 + 56 * i for i in range(13)]
    RIGHT = ["IO25", "IO4", "IO0", "IO2", "IO15", "IO13", "IO12", "IO14", "IO35", "IO34", "RST", "S_VN", "S_VP"]
    LEFT = ["5V", "GND", "3.3V", "GND", "IO26", "LoRa1", "LoRa2", "IO19", "IO23", "IO22", "RXD", "TXD", "IO21"]
    ry = dict(zip(RIGHT, PIN_Y))

    rect(BX0, BY0, BX1 - BX0, BY1 - BY0, "#1f2937", "#111827", rx=18)
    rect(350, BY0 + 4, 100, 26, "#9ca3af", rx=5, sw=0)
    text(400, BY0 + 22, "micro USB", 13, "middle", "bold", "#111827")
    text(400, 190, "LILYGO", 22, "middle", "bold", "#e5e7eb")
    text(400, 218, "LoRa32 T3 V1.6.1", 17, "middle", "normal", "#e5e7eb")
    rect(290, 270, 220, 120, "#374151", "#4b5563", rx=8)
    text(400, 318, "ESP32-PICO-D4", 15, "middle", "bold", "#e5e7eb")
    text(400, 342, "+ OLED integrato", 13, "middle", "normal", "#d1d5db")
    rect(290, 430, 220, 90, "#374151", "#4b5563", rx=8)
    text(400, 470, "Radio SX1278", 15, "middle", "bold", "#e5e7eb")
    text(400, 494, "433 MHz, antenna SMA", 13, "middle", "normal", "#d1d5db")

    MX0, MX1 = 1180, 1440
    AMP = {"VIN": 186, "GND": 230, "SD": 274, "GAIN": 318, "DIN": 362, "BCLK": 406, "LRC": 450}
    MIC = {"SCK": 586, "WS": 630, "SD": 674, "L/R": 718, "VDD": 762, "GND": 806}

    # fili (l'ordine decide chi scavalca chi)
    wire([(BX1, ry["IO25"]), (1060, ry["IO25"]), (1060, AMP["SD"]), (MX0, AMP["SD"])], C["sd"])
    wire([(BX1, ry["IO4"]), (1020, ry["IO4"]), (1020, AMP["DIN"]), (MX0, AMP["DIN"])], C["din"])
    wire([(BX1, ry["IO13"]), (MX0, AMP["LRC"])], C["ws"])
    wire([(920, ry["IO13"]), (920, MIC["WS"]), (MX0, MIC["WS"])], C["ws"])
    wire([(BX1, ry["IO14"]), (960, ry["IO14"]), (960, AMP["BCLK"]), (MX0, AMP["BCLK"])], C["bclk"])
    wire([(960, ry["IO14"]), (960, MIC["SCK"]), (MX0, MIC["SCK"])], C["bclk"])
    wire([(BX1, ry["IO34"]), (MX0, MIC["SD"])], C["mic"])
    wire([(BX1, ry["IO0"]), (730, ry["IO0"])], C["ptt"])
    wire([(BX1, ry["IO15"]), (730, ry["IO15"])], C["vol"])
    dot(920, ry["IO13"], C["ws"], 6, "#ffffff")
    dot(960, ry["IO14"], C["bclk"], 6, "#ffffff")

    rect(690, 236, 210, 184, "#fff7ed", "#fdba74", rx=10)
    button(730, ry["IO0"], "PTT (pulsante esterno)", "ptt")
    button(730, ry["IO15"], "Volume (opzionale)", "vol")

    amp_box(MX0, MX1, AMP, 146, 490)
    speaker(MX1)
    mic_box(MX0, MX1, MIC, 550, 846)

    used_right = {
        "IO25": (C["sd"], "SD: shutdown ampl."), "IO4": (C["din"], "DIN: dati altoparlante"),
        "IO0": (C["ptt"], ""), "IO15": (C["vol"], ""), "IO13": (C["ws"], "WS: word select"),
        "IO14": (C["bclk"], "BCLK: clock di bit"), "IO34": (C["mic"], "SD: dati microfono"),
    }
    for name, y in ry.items():
        color, note = used_right.get(name, (C["grey"], ""))
        dot(BX1, y, color)
        text(BX1 - 16, y + 5, name, 14, "end", "bold", "#e5e7eb" if name in used_right else "#9ca3af")
        if note:
            text(BX1 + 14, y - 9, note, 12, "start", "bold", color)

    power = {0: (C["5v"], "5V  ->  VIN amplificatore"), 1: (C["gnd"], "GND  ->  massa comune"),
             2: (C["3v3"], "3,3 V  ->  VDD microfono"), 3: (C["gnd"], "")}
    for i, name in enumerate(LEFT):
        y = PIN_Y[i]
        color, note = power.get(i, (C["grey"], ""))
        dot(BX0, y, color, 7, "#ffffff" if i in power else "#6b7280")
        text(BX0 + 16, y + 5, name, 14, "start", "bold", "#e5e7eb" if i in power else "#9ca3af")
        if note:
            text(BX0 - 16, y + 5, note, 13, "end", "bold", color)

    notes_panel([
        "Alimentazione: il pin 5V del connettore (c'e' solo con l'USB) va a VIN dell'amplificatore; a batteria usare 3,3 V. 3,3 V va a VDD del microfono. Tutte le masse (GND) sono in comune.",
        "Mettere un elettrolitico da circa 100 uF tra VIN e GND dell'amplificatore, vicino al modulo. Il pin GAIN resta libero (9 dB).",
        "INMP441: L/R a GND (canale sinistro). SCK e WS sono in comune con l'amplificatore. Fili corti (10-15 cm) e lontani dall'antenna.",
        "PTT: pulsante tra IO0 e GND (non premerlo durante l'accensione o il reset). Volume: pulsante opzionale tra IO15 e GND.",
        "Togliere la microSD: i pin 13, 14 e 15 sono anche quelli dello slot. Antenna a 433 MHz sempre collegata prima del cavo USB.",
        "Fonte: docs/pinout.md e src/board_pins_t3_v161.h. Generato da tools/render_wiring.py.",
    ])
    return els


# =============================================================================== T-Beam V1.2
def draw_tbeam():
    global els
    els = []
    header("T-Beam V1.2 (AXP2101): collegamenti audio e pulsanti",
           "Scheda con l'OLED in alto. Connettore a 13 pin con l'ordine stampato sulla scheda: VP in alto, 5V in basso.")

    BX0, BX1, BY0, BY1 = 220, 580, 100, 900
    PIN_Y = [170 + 56 * i for i in range(13)]
    PINS = ["VP", "VN", "RST", "15", "35", "32", "33", "25", "14", "13", "2", "GND", "5V"]
    ry = dict(zip(PINS, PIN_Y))

    # scheda
    rect(BX0, BY0, BX1 - BX0, BY1 - BY0, "#1f2937", "#111827", rx=18)
    rect(280, 118, 240, 118, "#1d4ed8", "#93c5fd", rx=8)
    rect(300, 134, 200, 86, "#0f172a", "#334155", rx=4)
    text(400, 168, "OLED 0,96\"", 15, "middle", "bold", "#e5e7eb")
    text(400, 190, "(sul connettore I2C 21/22)", 12, "middle", "normal", "#9ca3af")
    text(400, 280, "LILYGO T-Beam V1.2", 19, "middle", "bold", "#e5e7eb")
    text(400, 304, "TBeam-AXP2101", 14, "middle", "normal", "#d1d5db")
    rect(290, 330, 220, 64, "#374151", "#4b5563", rx=8)
    text(400, 358, "Radio SX127x 433 MHz", 14, "middle", "bold", "#e5e7eb")
    text(400, 378, "antenna SMA", 12, "middle", "normal", "#d1d5db")
    rect(290, 410, 220, 64, "#374151", "#4b5563", rx=8)
    text(400, 438, "AXP2101", 14, "middle", "bold", "#e5e7eb")
    text(400, 458, "batteria 18650 e carica", 12, "middle", "normal", "#d1d5db")
    rect(290, 490, 220, 64, "#374151", "#4b5563", rx=8)
    text(400, 518, "GPS u-blox NEO-6M", 14, "middle", "bold", "#e5e7eb")
    text(400, 538, "RX 34 / TX 12 (non usato)", 12, "middle", "normal", "#d1d5db")
    # tasti di bordo
    for bx, label, hl in ((260, "PWR", False), (340, "IO38", True), (420, "RST", False)):
        rect(bx, 800, 66, 34, "#fff7ed" if hl else "#374151", C["ptt"] if hl else "#4b5563", rx=6, sw=3 if hl else 1)
        text(bx + 33, 822, label, 14, "middle", "bold", C["ptt"] if hl else "#d1d5db")
    text(400, 854, "IO38 = tasto PTT (sulla scheda)", 14, "middle", "bold", C["ptt"])
    rect(350, BY1 - 34, 100, 26, "#9ca3af", rx=5, sw=0)
    text(400, BY1 - 16, "micro USB", 13, "middle", "bold", "#111827")

    MX0, MX1 = 1180, 1440
    MIC = {"SD": 170, "SCK": 204, "WS": 238, "L/R": 272, "VDD": 306, "GND": 340}
    AMP = {"VIN": 474, "GND": 518, "SD": 562, "GAIN": 606, "DIN": 650, "BCLK": 694, "LRC": 738}

    # fili. Ordine di disegno: chi viene dopo scavalca chi viene prima.
    wire([(BX1, ry["VP"]), (MX0, MIC["SD"])], C["mic"])
    wire([(BX1, ry["25"]), (MX0, AMP["SD"])], C["sd"])
    wire([(BX1, ry["2"]), (860, ry["2"]), (860, AMP["DIN"]), (MX0, AMP["DIN"])], C["din"])
    wire([(BX1, ry["14"]), (900, ry["14"]), (900, MIC["SCK"]), (MX0, MIC["SCK"])], C["bclk"])
    wire([(900, ry["14"]), (900, AMP["BCLK"]), (MX0, AMP["BCLK"])], C["bclk"])
    wire([(BX1, ry["13"]), (940, ry["13"]), (940, MIC["WS"]), (MX0, MIC["WS"])], C["ws"])
    wire([(940, ry["13"]), (940, AMP["LRC"]), (MX0, AMP["LRC"])], C["ws"])
    wire([(BX1, ry["15"]), (700, ry["15"])], C["vol"])
    dot(900, ry["14"], C["bclk"], 6, "#ffffff")
    dot(940, ry["13"], C["ws"], 6, "#ffffff")

    rect(660, ry["15"] - 52, 210, 86, "#fff7ed", "#fdba74", rx=10)
    button(700, ry["15"], "Volume (opzionale)", "vol")

    mic_box(MX0, MX1, MIC, 130, 366, vdd_label="3,3 V *")
    amp_box(MX0, MX1, AMP, 434, 778, out_plus=530, out_minus=610)
    speaker(MX1, 530, 610)

    used = {"VP": (C["mic"], "SD: dati microfono (GPIO36)"), "25": (C["sd"], "SD: shutdown ampl."),
            "2": (C["din"], "DIN: dati altoparlante"), "14": (C["bclk"], "BCLK: clock di bit"),
            "13": (C["ws"], "WS: word select"), "15": (C["vol"], "")}
    busy = {"35": "occupato: interrupt dell'AXP2101", "32": "occupato: DIO2 della radio",
            "33": "occupato: DIO1 della radio", "VN": "libero", "RST": "reset"}
    for name, y in ry.items():
        color, note = used.get(name, (C["grey"], ""))
        if name == "GND":
            color = C["gnd"]
        if name == "5V":
            color = C["5v"]
        dot(BX1, y, color, 7, "#ffffff" if (name in used or name in ("GND", "5V")) else "#6b7280")
        text(BX1 - 16, y + 5, name, 14, "end", "bold",
             "#e5e7eb" if (name in used or name in ("GND", "5V")) else "#9ca3af")
        if note:
            text(BX1 + 14, y - 9, note, 12, "start", "bold", color)
        elif name in busy:
            text(BX1 + 14, y + 5, busy[name], 12, "start", "normal", "#9ca3af")
    text(BX1 + 14, ry["GND"] + 5, "GND  ->  massa comune", 13, "start", "bold", C["gnd"])
    text(BX1 + 14, ry["5V"] + 5, "5V  ->  VIN amplificatore", 13, "start", "bold", C["5v"])

    notes_panel([
        "Alimentazione: il pin 5V del connettore va a VIN dell'amplificatore (da verificare con il tester se c'e' anche a batteria; altrimenti usare 3,3 V). GND in comune per tutti i moduli.",
        "* Il connettore a 13 pin NON ha un pin 3,3 V: il 3,3 V per VDD del microfono va preso da un punto a 3,3 V della scheda (per esempio il VCC del connettore dell'OLED), verificato con il tester. Mai oltre 3,6 V.",
        "Elettrolitico da circa 100 uF tra VIN e GND dell'amplificatore. GAIN libero (9 dB). INMP441: L/R a GND (canale sinistro); SCK e WS sono in comune con l'amplificatore. Fili corti, lontani dall'antenna.",
        "PTT: il tasto IO38 di bordo (nessun filo). Se l'OLED mostra \"PTT? pin instabile\" serve un pull-up da 10 kohm tra IO38 e 3,3 V. Volume: pulsante opzionale tra il pin 15 e GND.",
        "Non toccati: GPS (GPIO34 e 12), interrupt AXP2101 (GPIO35), DIO della radio (32 e 33), LED di bordo (GPIO4). Antenna a 433 MHz sempre collegata prima dell'USB.",
        "Fonte: docs/pinout.md e src/board_pins_tbeam_v12.h. Fase 1: non ancora provato con l'audio. Generato da tools/render_wiring.py.",
    ])
    return els


def write(drawer, out):
    drawer()
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">\n'
           + "\n".join(els) + "\n</svg>\n")
    with open(out, "w", encoding="utf-8") as f:
        f.write(svg)
    print(f"scritto {out} ({W}x{H})")


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "tutti"
    if which in ("t3", "tutti"):
        write(draw_t3, "docs/images/collegamenti-t3.svg")
    if which in ("tbeam", "tutti"):
        write(draw_tbeam, "docs/images/collegamenti-tbeam.svg")
    if which not in ("t3", "tbeam", "tutti"):
        raise SystemExit(__doc__)
