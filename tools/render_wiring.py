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
           "Faccia dei componenti (modulo LoRa e chip ESP32), USB in alto: e' la faccia con le sigle dei pin. L'OLED e' sull'altra faccia.")

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
    # dall'USB verso il basso, come sulla scheda: radio, poi ESP32, in fondo l'antenna Wi-Fi stampata
    text(400, 246, "ON/OFF, RST, batteria (JST), SMA e U.FL vicino all'USB", 11, "middle", "normal", "#9ca3af")
    rect(290, 270, 220, 120, "#374151", "#4b5563", rx=8)
    text(400, 318, "Radio SX1278", 15, "middle", "bold", "#e5e7eb")
    text(400, 342, "LILYGO LORA32 433/470 MHz", 12, "middle", "normal", "#d1d5db")
    rect(290, 430, 220, 90, "#374151", "#4b5563", rx=8)
    text(400, 470, "ESP32-PICO-D4", 15, "middle", "bold", "#e5e7eb")
    text(400, 494, "OLED sull'altra faccia", 13, "middle", "normal", "#d1d5db")
    rect(300, 850, 200, 36, "#374151", "#4b5563", rx=6)
    text(400, 873, "antenna Wi-Fi (stampata)", 12, "middle", "normal", "#d1d5db")

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
    """Scheda disegnata come si tiene in mano e come si legge la serigrafia: OLED a sinistra, antenna in alto,
    i 13 pin sul bordo inferiore da VP (sinistra) a 5V (destra), micro USB e tre tasti a destra dei pin.
    I moduli stanno sotto la scheda e i fili scendono dai pin."""
    global els
    els = []
    header("T-Beam V1.2 (AXP2101): collegamenti audio e pulsanti",
           "Scheda come nella foto: OLED a sinistra, antenna in alto, 13 pin sul bordo inferiore da VP (sinistra) a 5V (destra).")

    BX0, BX1, BY0, BY1 = 60, 1010, 170, 462
    YH = 452                                   # centro dei pin
    PINS = ["VP", "VN", "RST", "15", "35", "32", "33", "25", "14", "13", "2", "GND", "5V"]
    px = {name: 130 + 42 * i for i, name in enumerate(PINS)}

    # antenna e scheda
    rect(700, 112, 44, 56, "#111827", "#000000", rx=14, sw=1)
    text(722, 104, "antenna 433 MHz", 13, "middle", "bold", "#111827")
    rect(710, 160, 24, 14, "#b08d1e", rx=3, sw=0)
    rect(BX0, BY0, BX1 - BX0, BY1 - BY0, "#1f2937", "#111827", rx=18)
    rect(74, 226, 100, 112, "#6b7280", "#9ca3af", rx=6)                 # ESP32 (schermo metallico)
    text(124, 286, "ESP32", 16, "middle", "bold", "#f3f4f6")
    rect(200, 190, 220, 126, "#1d4ed8", "#93c5fd", rx=8)                 # OLED
    rect(222, 214, 176, 78, "#0f172a", "#334155", rx=4)
    text(310, 246, "OLED 0,96\"", 15, "middle", "bold", "#e5e7eb")
    text(310, 268, "(I2C 21/22)", 12, "middle", "normal", "#9ca3af")
    text(310, 207, "VCC  GND  SCL  SDA", 10, "middle", "bold", "#e5e7eb")
    rect(446, 226, 66, 56, "#374151", "#4b5563", rx=6)
    text(479, 258, "AXP2101", 12, "middle", "bold", "#e5e7eb")
    rect(560, 196, 190, 150, "#e5e7eb", "#9ca3af", rx=6)                 # modulo LoRa
    text(655, 254, "LILYGO", 16, "middle", "bold", "#111827")
    text(655, 276, "433/470 MHz LORA32", 12, "middle", "normal", "#111827")
    text(655, 296, "SX127x, antenna SMA", 11, "middle", "normal", "#374151")
    rect(780, 196, 150, 150, "#d1d5db", "#9ca3af", rx=6)                 # GPS
    text(855, 262, "GPS u-blox", 14, "middle", "bold", "#111827")
    text(855, 282, "NEO-6M", 14, "middle", "bold", "#111827")
    text(855, 302, "RX 34 / TX 12 (non usato)", 10, "middle", "normal", "#374151")
    rect(690, 438, 86, 24, "#9ca3af", rx=5, sw=0)                        # micro USB
    text(733, 455, "micro USB", 12, "middle", "bold", "#111827")
    for bx, label, hl in ((796, "PWR", False), (858, "IO38", True), (920, "RST", False)):
        rect(bx, 410, 56, 34, "#fff7ed" if hl else "#374151", C["ptt"] if hl else "#4b5563", rx=6, sw=3 if hl else 1)
        text(bx + 28, 432, label, 13, "middle", "bold", C["ptt"] if hl else "#d1d5db")
    text(886, 398, "IO38 = tasto PTT (sulla scheda)", 13, "middle", "bold", C["ptt"])

    used = {"VP": C["mic"], "15": C["vol"], "25": C["sd"], "14": C["bclk"], "13": C["ws"], "2": C["din"],
            "GND": C["gnd"], "5V": C["5v"]}
    blocked = {"35", "32", "33"}

    # moduli sotto la scheda
    MIC_X = {"SD": 130, "SCK": 200, "WS": 270, "L/R": 340, "VDD": 410, "GND": 480}
    AMP_X = {"VIN": 640, "GND": 705, "SD": 770, "GAIN": 835, "DIN": 900, "BCLK": 965, "LRC": 1030}
    MY = 780                                    # quota dei pin dei moduli (bordo superiore)
    L = {"VP": 520, "2": 620, "25": 645, "14": 672, "13": 700}   # corsie orizzontali dei fili

    # fili (l'ordine decide chi scavalca chi)
    wire([(px["VP"], YH), (px["VP"], MY)], C["mic"])
    wire([(px["2"], YH), (px["2"], L["2"]), (AMP_X["DIN"], L["2"]), (AMP_X["DIN"], MY)], C["din"])
    wire([(px["25"], YH), (px["25"], L["25"]), (AMP_X["SD"], L["25"]), (AMP_X["SD"], MY)], C["sd"])
    wire([(px["14"], YH), (px["14"], L["14"]), (MIC_X["SCK"], L["14"]), (MIC_X["SCK"], MY)], C["bclk"])
    wire([(px["14"], L["14"]), (AMP_X["BCLK"], L["14"]), (AMP_X["BCLK"], MY)], C["bclk"])
    wire([(px["13"], YH), (px["13"], L["13"]), (MIC_X["WS"], L["13"]), (MIC_X["WS"], MY)], C["ws"])
    wire([(px["13"], L["13"]), (AMP_X["LRC"], L["13"]), (AMP_X["LRC"], MY)], C["ws"])
    dot(px["14"], L["14"], C["bclk"], 6)
    dot(px["13"], L["13"], C["ws"], 6)

    # pin del connettore: etichetta sopra il pin, come sulla scheda
    for name in PINS:
        x = px[name]
        col = used.get(name, C["grey"])
        on = name in used
        dot(x, YH, col, 7, "#ffffff" if on else "#6b7280")
        text(x, YH - 16, name, 13, "middle", "bold", "#e5e7eb" if on else "#9ca3af")
    text(px["35"] + 40, 398, "35, 32, 33: occupati (AXP2101 e radio)", 12, "middle", "normal", "#9ca3af")
    text(px["GND"] + 22, 478, "GND -> massa comune", 13, "start", "bold", C["gnd"])
    text(px["5V"] + 8, 498, "5V -> VIN amplificatore", 13, "start", "bold", C["5v"])

    # volume (opzionale)
    rect(196, 520, 232, 82, "#fff7ed", "#fdba74", rx=10)
    wire([(px["15"], YH), (px["15"], 566)], C["vol"])
    button(px["15"], 566, "", "vol")
    text(px["15"] + 16, 538, "Volume (opzionale)", 13, weight="bold", fill=C["vol"])

    # microfono
    rect(60, MY, 460, 92, "#f3f4f6", "#374151", rx=10)
    text(60, MY + 116, "INMP441", 20, weight="bold")
    text(172, MY + 116, "microfono I2S (modulo a 6 pin)", 14, fill="#4b5563")
    for n, x in MIC_X.items():
        col = {"SCK": C["bclk"], "WS": C["ws"], "SD": C["mic"]}.get(n, C["grey"])
        dot(x, MY, col)
        text(x, MY + 28, n, 13, "middle", "bold")
    for n, label, col in (("L/R", "GND", C["gnd"]), ("VDD", "3,3 V *", C["3v3"]), ("GND", "GND", C["gnd"])):
        x = MIC_X[n]
        w = 14 + 9 * len(label)
        rect(x - w / 2, MY - 40, w, 22, col, rx=11, sw=0)
        text(x, MY - 24, label, 12, "middle", "bold", "#ffffff")
        add(f'<line x1="{x}" y1="{MY - 18}" x2="{x}" y2="{MY}" stroke="{col}" stroke-width="3"/>')
    # amplificatore
    rect(600, MY, 500, 92, "#f3f4f6", "#374151", rx=10)
    text(600, MY + 116, "MAX98357A", 20, weight="bold")
    text(752, MY + 116, "amplificatore I2S classe D", 14, fill="#4b5563")
    for n, x in AMP_X.items():
        col = {"SD": C["sd"], "DIN": C["din"], "BCLK": C["bclk"], "LRC": C["ws"]}.get(n, C["grey"])
        dot(x, MY, col)
        text(x, MY + 28, n, 13, "middle", "bold")
    for n, label, col in (("VIN", "5V", C["5v"]), ("GND", "GND", C["gnd"]), ("GAIN", "libero", "#9ca3af")):
        x = AMP_X[n]
        w = 14 + 9 * len(label)
        rect(x - w / 2, MY - 40, w, 22, col, rx=11, sw=0)
        text(x, MY - 24, label, 12, "middle", "bold", "#ffffff")
        add(f'<line x1="{x}" y1="{MY - 18}" x2="{x}" y2="{MY}" stroke="{col}" stroke-width="3"/>')
    # altoparlante
    for yy, lab in ((832, "+"), (860, "-")):
        wire([(1100, yy), (1250, yy)], C["spk"], 5)
        dot(1100, yy, C["spk"])
        dot(1250, yy, C["spk"])
        text(1090, yy + 4, "OUT " + lab, 13, "end", "bold")
        text(1266, yy + 5, lab, 16, weight="bold", fill="#92400e")
    rect(1250, 790, 160, 100, "#fffbeb", "#d97706", rx=10)
    text(1345, 810, "Altoparlante", 15, "middle", "bold", "#92400e")
    text(1345, 827, "8 ohm 3 W", 13, "middle", fill="#92400e")
    add('<polygon points="1320,858 1340,858 1362,838 1362,880 1340,858" fill="#92400e"/>')
    add('<path d="M1372,850 q12,10 0,20 M1382,842 q20,18 0,36" fill="none" stroke="#92400e" stroke-width="3" stroke-linecap="round"/>')

    # legenda dei fili
    rect(1060, 130, 590, 330, "#f9fafb", "#e5e7eb", rx=10)
    text(1080, 160, "Fili, dal pin della scheda al modulo", 15, weight="bold")
    rows = [("VP (GPIO36)", "SD del microfono", C["mic"]), ("25", "SD dell'amplificatore (shutdown)", C["sd"]),
            ("14", "SCK del microfono e BCLK dell'amplificatore", C["bclk"]),
            ("13", "WS del microfono e LRC dell'amplificatore", C["ws"]),
            ("2", "DIN dell'amplificatore (dati altoparlante)", C["din"]), ("15", "pulsante volume (opzionale) a GND", C["vol"]),
            ("GND", "massa comune a tutti i moduli", C["gnd"]), ("5V", "VIN dell'amplificatore", C["5v"])]
    for i, (pin, what, col) in enumerate(rows):
        y = 192 + 32 * i
        add(f'<line x1="1080" y1="{y - 5}" x2="1128" y2="{y - 5}" stroke="{col}" stroke-width="6" stroke-linecap="round"/>')
        text(1142, y, pin, 14, weight="bold")
        text(1250, y, what, 14, fill="#374151")
    text(1080, 192 + 32 * 8 + 6, "Non toccati: 35 (AXP2101), 32 e 33 (radio), GPS 34/12, LED di bordo GPIO4.", 12, fill="#6b7280")

    notes_panel([
        "Alimentazione: il pin 5V del connettore va a VIN dell'amplificatore (da verificare con il tester se c'e' anche a batteria; altrimenti usare 3,3 V). GND in comune per tutti i moduli.",
        "* Il connettore a 13 pin NON ha un pin 3,3 V: il 3,3 V per VDD del microfono va preso da un punto a 3,3 V della scheda (per esempio il VCC del connettore dell'OLED), verificato con il tester. Mai oltre 3,6 V.",
        "Elettrolitico da circa 100 uF tra VIN e GND dell'amplificatore. GAIN libero (9 dB; a GND 12 dB). INMP441: L/R a GND (canale sinistro); SCK e WS sono in comune con l'amplificatore. Fili corti, lontani dall'antenna.",
        "PTT: il tasto IO38 di bordo (nessun filo). Se l'OLED mostra \"PTT? pin instabile\" serve un pull-up da 10 kohm tra IO38 e 3,3 V. Volume: pulsante opzionale tra il pin 15 e GND.",
        "Antenna a 433 MHz sempre collegata prima dell'USB. Il disegno non e' in scala: l'ordine dei pin e quello stampato sulla scheda (foto in docs/pinout.md).",
        "Fonte: docs/pinout.md e src/board_pins_tbeam_v12.h. Generato da tools/render_wiring.py.",
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
