#!/usr/bin/env python3
"""Genera lo schema dei collegamenti hardware della LILYGO LoRa32 T3 V1.6.1.

Uscita: SVG (vettoriale, modificabile). I pin sono quelli di src/board_pins_t3_v161.h e
docs/pinout.md: se cambiano, aggiornare qui e rigenerare.

Uso:  python3 tools/render_wiring.py [docs/images/collegamenti-t3.svg]
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


# ---------------------------------------------------------------- sfondo e titolo
rect(0, 0, W, H, "#ffffff", rx=0, sw=0)
text(40, 44, "LoRa32 T3 V1.6.1: collegamenti audio e pulsanti", 28, weight="bold")
text(40, 72, "Scheda vista con l'USB in alto. I nomi dei pin sono quelli stampati sulla scheda.", 15, fill="#4b5563")

# ---------------------------------------------------------------- scheda
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

# ---------------------------------------------------------------- moduli a destra
MX0, MX1 = 1180, 1440
AMP = {"VIN": 186, "GND": 230, "SD": 274, "GAIN": 318, "DIN": 362, "BCLK": 406, "LRC": 450}
MIC = {"SCK": 586, "WS": 630, "SD": 674, "L/R": 718, "VDD": 762, "GND": 806}
amp_y0, amp_y1 = 146, 490
mic_y0, mic_y1 = 550, 846

# ---------------------------------------------------------------- fili (l'ordine decide chi scavalca chi)
# Amplificatore: shutdown e dati altoparlante
wire([(BX1, ry["IO25"]), (1060, ry["IO25"]), (1060, AMP["SD"]), (MX0, AMP["SD"])], C["sd"])
wire([(BX1, ry["IO4"]), (1020, ry["IO4"]), (1020, AMP["DIN"]), (MX0, AMP["DIN"])], C["din"])
# WS in comune: amplificatore (LRC) e microfono (WS)
wire([(BX1, ry["IO13"]), (MX0, AMP["LRC"])], C["ws"])
wire([(920, ry["IO13"]), (920, MIC["WS"]), (MX0, MIC["WS"])], C["ws"])
# BCLK in comune: amplificatore (BCLK) e microfono (SCK)
wire([(BX1, ry["IO14"]), (960, ry["IO14"]), (960, AMP["BCLK"]), (MX0, AMP["BCLK"])], C["bclk"])
wire([(960, ry["IO14"]), (960, MIC["SCK"]), (MX0, MIC["SCK"])], C["bclk"])
# Dati del microfono
wire([(BX1, ry["IO34"]), (MX0, MIC["SD"])], C["mic"])
# Pulsanti (vicino alla scheda)
wire([(BX1, ry["IO0"]), (730, ry["IO0"])], C["ptt"])
wire([(BX1, ry["IO15"]), (730, ry["IO15"])], C["vol"])
# Altoparlante
wire([(MX1, 250), (1530, 250)], C["spk"], 5)
wire([(MX1, 330), (1530, 330)], C["spk"], 5)

# punti di giunzione
dot(920, ry["IO13"], C["ws"], 6, "#ffffff")
dot(960, ry["IO14"], C["bclk"], 6, "#ffffff")

# ---------------------------------------------------------------- pulsanti
rect(690, 236, 210, 184, "#fff7ed", "#fdba74", rx=10)
for name, key, label in (("IO0", "ptt", "PTT (pulsante esterno)"), ("IO15", "vol", "Volume (opzionale)")):
    y = ry[name]
    text(702, y - 22, label, 13, weight="bold", fill=C[key])
    dot(730, y, "#ffffff", 6, C[key])
    add(f'<line x1="730" y1="{y}" x2="775" y2="{y - 18}" stroke="#111827" stroke-width="3" stroke-linecap="round"/>')
    dot(790, y, "#ffffff", 6, C["gnd"])
    add(f'<line x1="796" y1="{y}" x2="822" y2="{y}" stroke="{C["gnd"]}" stroke-width="3"/>')
    rect(822, y - 12, 56, 24, C["gnd"], rx=12, sw=0)
    text(850, y + 5, "GND", 13, "middle", "bold", "#ffffff")

# ---------------------------------------------------------------- MAX98357A
rect(MX0, amp_y0, MX1 - MX0, amp_y1 - amp_y0, "#f3f4f6", "#374151", rx=10)
text(MX0, amp_y0 - 26, "MAX98357A", 20, weight="bold")
text(MX0, amp_y0 - 8, "amplificatore I2S classe D", 14, fill="#4b5563")
for name, y in AMP.items():
    text(MX0 + 18, y + 5, name, 14, weight="bold")
for name, y in AMP.items():
    col = {"SD": C["sd"], "DIN": C["din"], "BCLK": C["bclk"], "LRC": C["ws"]}.get(name, C["grey"])
    dot(MX0, y, col)
tag(MX0 - 20, AMP["VIN"], "5V", C["5v"])
tag(MX0 - 20, AMP["GND"], "GND", C["gnd"])
rect(MX0 - 118, AMP["GAIN"] - 12, 98, 24, "#e5e7eb", rx=12, sw=0)
text(MX0 - 69, AMP["GAIN"] + 5, "libero", 13, "middle", "bold", "#374151")
add(f'<line x1="{MX0 - 20}" y1="{AMP["GAIN"]}" x2="{MX0}" y2="{AMP["GAIN"]}" stroke="#9ca3af" stroke-width="3"/>')
text(MX1 - 18, 254, "OUT +", 14, "end", "bold")
text(MX1 - 18, 334, "OUT -", 14, "end", "bold")
dot(MX1, 250, C["spk"])
dot(MX1, 330, C["spk"])

# altoparlante
rect(1530, 200, 130, 180, "#fffbeb", "#d97706", rx=10)
text(1595, 222, "Altoparlante", 15, "middle", "bold", "#92400e")
text(1595, 240, "8 ohm 3 W", 13, "middle", fill="#92400e")
add('<polygon points="1560,300 1585,300 1610,276 1610,324 1585,300" fill="#92400e"/>')
add('<path d="M1625,288 q14,12 0,24 M1636,278 q22,22 0,44" fill="none" stroke="#92400e" stroke-width="3" stroke-linecap="round"/>')
dot(1530, 250, C["spk"])
dot(1530, 330, C["spk"])
text(1546, 254, "+", 16, weight="bold", fill="#92400e")
text(1546, 334, "-", 16, weight="bold", fill="#92400e")

# ---------------------------------------------------------------- INMP441
rect(MX0, mic_y0, MX1 - MX0, mic_y1 - mic_y0, "#f3f4f6", "#374151", rx=10)
text(MX0, mic_y0 - 26, "INMP441", 20, weight="bold")
text(MX0, mic_y0 - 8, "microfono I2S (modulo a 6 pin)", 14, fill="#4b5563")
for name, y in MIC.items():
    text(MX0 + 18, y + 5, name, 14, weight="bold")
for name, y in MIC.items():
    col = {"SCK": C["bclk"], "WS": C["ws"], "SD": C["mic"]}.get(name, C["grey"])
    dot(MX0, y, col)
tag(MX0 - 20, MIC["L/R"], "GND", C["gnd"])
tag(MX0 - 20, MIC["VDD"], "3,3 V", C["3v3"])
tag(MX0 - 20, MIC["GND"], "GND", C["gnd"])

# ---------------------------------------------------------------- pin della scheda e annotazioni
used_right = {
    "IO25": (C["sd"], "SD: shutdown ampl."),
    "IO4": (C["din"], "DIN: dati altoparlante"),
    "IO0": (C["ptt"], ""),
    "IO15": (C["vol"], ""),
    "IO13": (C["ws"], "WS: word select"),
    "IO14": (C["bclk"], "BCLK: clock di bit"),
    "IO34": (C["mic"], "SD: dati microfono"),
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

# ---------------------------------------------------------------- note
y0 = 940
rect(40, y0 - 24, W - 80, 196, "#f9fafb", "#e5e7eb", rx=10)
notes = [
    "Alimentazione: il pin 5V del connettore (c'e' solo con l'USB) va a VIN dell'amplificatore; a batteria usare 3,3 V. 3,3 V va a VDD del microfono. Tutte le masse (GND) sono in comune.",
    "Mettere un elettrolitico da circa 100 uF tra VIN e GND dell'amplificatore, vicino al modulo. Il pin GAIN resta libero (9 dB).",
    "INMP441: L/R a GND (canale sinistro). SCK e WS sono in comune con l'amplificatore. Fili corti (10-15 cm) e lontani dall'antenna.",
    "PTT: pulsante tra IO0 e GND (non premerlo durante l'accensione o il reset). Volume: pulsante opzionale tra IO15 e GND.",
    "Togliere la microSD: i pin 13, 14 e 15 sono anche quelli dello slot. Antenna a 433 MHz sempre collegata prima del cavo USB.",
    "Fonte: docs/pinout.md e src/board_pins_t3_v161.h. Generato da tools/render_wiring.py.",
]
for i, n in enumerate(notes):
    text(60, y0 + 8 + i * 30, n, 14, fill="#374151" if i < 5 else "#6b7280")

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">\n'
       + "\n".join(els) + "\n</svg>\n")
out = sys.argv[1] if len(sys.argv) > 1 else "docs/images/collegamenti-t3.svg"
with open(out, "w", encoding="utf-8") as f:
    f.write(svg)
print(f"scritto {out} ({W}x{H})")
