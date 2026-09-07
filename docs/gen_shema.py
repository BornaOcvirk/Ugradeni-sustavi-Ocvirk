# -*- coding: utf-8 -*-
"""Generira docs/shema_spajanja.svg - shemu spajanja autica na ESP32.

Topologija: jedno napajanje. Power bank ulazi u USB konektor ESP32 ploce,
ESP32 preko VIN pina hrani oba ULN2003 modula (5 V), a preko 3V3 pina senzor.
HC-SR04P radi na 3,3 V pa ECHO ide izravno na GPIO, bez djelitelja napona.
"""
import io
import os

W, H = 1460, 1170

C_BG = "#ffffff"
C_INK = "#0f172a"
C_MUTED = "#475569"
C_BOARD = "#f1f5f9"
C_BOARD_LINE = "#334155"
C_ESP = "#e4edfb"
C_ESP_LINE = "#1d4ed8"
C_5V = "#dc2626"
C_3V3 = "#0e7490"
C_GND = "#111827"
C_MOT = "#2563eb"
C_SEN = "#047857"
C_WARN_BG = "#fff7ed"
C_WARN_LINE = "#c2410c"
C_NOTE_BG = "#f8fafc"
C_OK = "#15803d"

out = []
a = out.append


def esc(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


def text(x, y, s, size=14, fill=C_INK, weight="400", anchor="start", family=None, style=""):
    fam = family or "'Segoe UI',system-ui,-apple-system,sans-serif"
    a('<text x="%g" y="%g" font-family="%s" font-size="%g" fill="%s" '
      'font-weight="%s" text-anchor="%s"%s>%s</text>'
      % (x, y, fam, size, fill, weight, anchor, (' style="%s"' % style) if style else "", esc(s)))


def mono(x, y, s, size=13, fill=C_INK, weight="600", anchor="start"):
    text(x, y, s, size, fill, weight, anchor,
         family="'Cascadia Mono','Consolas','DejaVu Sans Mono',monospace")


def box(x, y, w, h, fill, stroke, rx=10, sw=2):
    a('<rect x="%g" y="%g" width="%g" height="%g" rx="%g" fill="%s" stroke="%s" '
      'stroke-width="%g"/>' % (x, y, w, h, rx, fill, stroke, sw))


def module(x, y, w, h, title, subtitle=None, fill=C_BOARD, stroke=C_BOARD_LINE):
    box(x, y, w, h, fill, stroke)
    a('<path d="M%g %g H%g" stroke="%s" stroke-width="1.2" opacity=".45"/>'
      % (x, y + 38, x + w, stroke))
    text(x + w / 2, y + 26, title, 16, C_INK, "700", "middle")
    if subtitle:
        text(x + w / 2, y + 58, subtitle, 12.5, C_MUTED, "400", "middle")


def dot(x, y, color=C_INK, r=5):
    a('<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (x, y, r, color))


def wire(points, color, width=2.6, hops=(), dash=None):
    """Ortogonalna zica; hops su tocke krizanja gdje se crta 'mostic'."""
    hops = list(hops)
    d = ["M %g %g" % points[0]]
    for i in range(1, len(points)):
        x0, y0 = points[i - 1]
        x1, y1 = points[i]
        on = []
        for (hx, hy) in hops:
            if abs(y1 - y0) < 0.01 and abs(hy - y0) < 0.01 and min(x0, x1) < hx < max(x0, x1):
                on.append((hx, hy))
            elif abs(x1 - x0) < 0.01 and abs(hx - x0) < 0.01 and min(y0, y1) < hy < max(y0, y1):
                on.append((hx, hy))
        if abs(y1 - y0) < 0.01:
            on.sort(key=lambda p: p[0], reverse=(x1 < x0))
            step = 1 if x1 > x0 else -1
            for (hx, hy) in on:
                d.append("L %g %g" % (hx - 9 * step, y0))
                d.append("A 9 9 0 0 %d %g %g" % (1 if step > 0 else 0, hx + 9 * step, y0))
            d.append("L %g %g" % (x1, y1))
        else:
            on.sort(key=lambda p: p[1], reverse=(y1 < y0))
            step = 1 if y1 > y0 else -1
            for (hx, hy) in on:
                d.append("L %g %g" % (x0, hy - 9 * step))
                d.append("A 9 9 0 0 %d %g %g" % (0 if step > 0 else 1, x0, hy + 9 * step))
            d.append("L %g %g" % (x1, y1))
    extra = ' stroke-dasharray="%s"' % dash if dash else ""
    a('<path d="%s" fill="none" stroke="%s" stroke-width="%g" stroke-linecap="round" '
      'stroke-linejoin="round"%s/>' % (" ".join(d), color, width, extra))


def pin(x, y, label, side="right", color=C_INK, size=12.5):
    """Kvadratic pina na rubu ploce + natpis unutar ploce."""
    a('<rect x="%g" y="%g" width="10" height="10" rx="2" fill="%s"/>'
      % (x - 5, y - 5, color))
    if side == "right":
        mono(x - 12, y + 4.5, label, size, C_INK, "600", "end")
    else:
        mono(x + 12, y + 4.5, label, size, C_INK, "600", "start")


# ---------------------------------------------------------------- canvas
a('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">'
  % (W, H, W, H))
a('<rect width="%d" height="%d" fill="%s"/>' % (W, H, C_BG))

text(W / 2, 44, u"Autić na daljinsko upravljanje – shema spajanja", 26, C_INK, "700", "middle")
text(W / 2, 70,
     u"ESP32 uPesy WROOM DevKit  •  2× 28BYJ-48 + ULN2003  •  HC-SR04P",
     14.5, C_MUTED, "400", "middle")
text(W / 2, 90,
     u"jedno napajanje: USB power bank → ESP32 → VIN hrani motore, 3V3 senzor",
     13.5, C_OK, "600", "middle")

# ---------------------------------------------------------------- rails (lijevo)
RAIL_5V, RAIL_GND = 150, 100

a('<path d="M%g 400 V1060" fill="none" stroke="%s" stroke-width="4"/>' % (RAIL_5V, C_5V))
a('<path d="M%g 475 V1110" fill="none" stroke="%s" stroke-width="4"/>' % (RAIL_GND, C_GND))
text(RAIL_5V, 388, "+5 V", 13, C_5V, "700", "middle")
text(RAIL_GND, 463, "GND", 13, C_GND, "700", "middle")

# ---------------------------------------------------------------- ESP32
module(590, 280, 320, 660, "ESP32 uPesy WROOM DevKit",
       u"ESP32-WROOM-32 · CH340C", C_ESP, C_ESP_LINE)

L = {"32": 400, "33": 425, "25": 450, "26": 475,
     "27": 800, "14": 825, "12": 850, "13": 875}
for g, y in L.items():
    pin(590, y, "GPIO " + g, "left", C_MOT)

pin(590, 655, "VIN (5 V)", "left", C_5V)
pin(590, 685, "GND", "left", C_GND)
text(760, 618, u"VIN je izlaz: nosi 5 V s USB-a dalje na motore", 12, C_5V, "600", "middle")

pin(910, 365, "GPIO 17", "right", C_SEN)
pin(910, 395, "GPIO 16", "right", C_SEN)
pin(910, 505, "3V3", "right", C_3V3)
pin(910, 535, "GND", "right", C_GND)

# USB konektor na dnu ploce
box(725, 940, 70, 22, "#94a3b8", C_BOARD_LINE, 4, 1.8)
text(760, 956, "USB", 11.5, "#ffffff", "700", "middle")

# ---------------------------------------------------------------- motori + ULN2003
module(250, 310, 270, 190, "ULN2003 (LIJEVI)", u"Darlington driver · lijevi pogon")
module(300, 530, 200, 95, "28BYJ-48", u"lijevi kotač")
module(250, 710, 270, 190, "ULN2003 (DESNI)", u"Darlington driver · desni pogon")
module(300, 930, 200, 95, "28BYJ-48", u"desni kotač")

for i, g in enumerate(["IN1", "IN2", "IN3", "IN4"]):
    pin(520, 400 + 25 * i, g, "right", C_MOT, 12)
    pin(520, 800 + 25 * i, g, "right", C_MOT, 12)
pin(250, 400, "VCC", "left", C_5V, 12)
pin(250, 475, "GND", "left", C_GND, 12)
pin(250, 800, "VCC", "left", C_5V, 12)
pin(250, 875, "GND", "left", C_GND, 12)

# bijeli 5-pinski konektori motora
wire([(385, 500), (385, 530)], C_MOT, 5)
wire([(385, 900), (385, 930)], C_MOT, 5)

# ULN2003 IN -> ESP32
for y in [400, 425, 450, 475, 800, 825, 850, 875]:
    wire([(520, y), (590, y)], C_MOT)

# napajanje ULN2003 modula iz ESP32 VIN
wire([(590, 655), (RAIL_5V, 655)], C_5V)
dot(RAIL_5V, 655, C_5V)
wire([(RAIL_5V, 400), (250, 400)], C_5V)
dot(RAIL_5V, 400, C_5V)
wire([(RAIL_5V, 800), (250, 800)], C_5V)
dot(RAIL_5V, 800, C_5V)

wire([(590, 685), (RAIL_GND, 685)], C_GND, hops=[(RAIL_5V, 685)])
dot(RAIL_GND, 685, C_GND)
wire([(250, 475), (RAIL_GND, 475)], C_GND, hops=[(RAIL_5V, 475)])
dot(RAIL_GND, 475, C_GND)
wire([(250, 875), (RAIL_GND, 875)], C_GND, hops=[(RAIL_5V, 875)])
dot(RAIL_GND, 875, C_GND)

# elektrolitski kondenzator preko 5 V i GND
wire([(RAIL_5V, 1060), (137, 1060)], C_5V)
a('<path d="M137 1042 V1078" stroke="%s" stroke-width="4"/>' % C_5V)
a('<path d="M121 1042 V1078" stroke="%s" stroke-width="4"/>' % C_GND)
wire([(121, 1060), (RAIL_GND, 1060)], C_GND)
dot(RAIL_5V, 1060, C_5V)
dot(RAIL_GND, 1060, C_GND)
text(175, 1050, u"1000 µF elektrolit", 12.5, C_INK, "700")
text(175, 1068, u"hvata strujne udare motora; bez njega", 11.5, C_MUTED)
text(175, 1084, u"Wi-Fi peak sruši napon i ESP32 se resetira", 11.5, C_MUTED)

# ---------------------------------------------------------------- HC-SR04P
module(1060, 250, 250, 200, "HC-SR04P", u"ultrazvučni senzor, 3–5,5 V")
pin(1060, 335, "VCC", "left", C_3V3, 12)
pin(1060, 365, "TRIG", "left", C_SEN, 12)
pin(1060, 395, "ECHO", "left", C_SEN, 12)
pin(1060, 425, "GND", "left", C_GND, 12)

wire([(910, 365), (1060, 365)], C_SEN)
wire([(910, 395), (1060, 395)], C_SEN)
text(985, 357, "TRIG", 11.5, C_SEN, "700", "middle")
text(985, 387, "ECHO", 11.5, C_SEN, "700", "middle")
text(1185, 482, u"ECHO ide izravno na GPIO 16 – bez djelitelja napona",
     11.5, C_OK, "600", "middle")


# 3V3 grana prema oba senzora
wire([(910, 505), (1030, 505), (1030, 335), (1060, 335)], C_3V3,
     hops=[(995, 505), (1030, 425), (1030, 395), (1030, 365)])

# GND grana prema oba senzora
wire([(910, 535), (995, 535), (995, 425), (1060, 425)], C_GND)

# ---------------------------------------------------------------- napajanje
module(610, 990, 300, 105, "USB POWER BANK", u"5 V, najmanje 2 A – jedini izvor")
wire([(760, 962), (760, 990)], C_5V, 5)
text(778, 980, u"podatkovni USB kabel", 11.5, C_MUTED, "400", "start")

# ---------------------------------------------------------------- upozorenja
box(40, 108, 530, 172, C_WARN_BG, C_WARN_LINE, 12, 2)
text(60, 136, u"⚠  Četiri pravila da ništa ne izgori", 15, C_WARN_LINE, "700")
text(60, 162, u"1.  HC-SR04P VCC ide na 3V3, nikada na 5 V. Razina na ECHO", 12.5, C_INK)
text(76, 179, u"pinu prati VCC – na 5 V bi spržio GPIO 16.", 12.5, C_INK)
text(60, 201, u"2.  Napajaj power bankom 2 A. USB računala (500 mA) je premalo.", 12.5, C_INK)
text(60, 223, u"3.  1000 µF između VIN i GND, blizu ULN2003 modula.", 12.5, C_INK)
text(60, 245, u"4.  Prije spajanja motora izmjeri VIN multimetrom: mora dati", 12.5, C_INK)
text(76, 262, u"oko 4,7 V dok je USB priključen.", 12.5, C_INK)

# ---------------------------------------------------------------- legenda
box(950, 850, 470, 200, C_NOTE_BG, C_BOARD_LINE, 12, 2)
text(972, 882, u"Legenda i tablica pinova", 15, C_INK, "700")

leg = [(C_5V, "+5 V"), (C_3V3, "+3,3 V"), (C_GND, "masa"),
       (C_MOT, "motori"), (C_SEN, "HC-SR04P")]
for i, (col, lab) in enumerate(leg):
    x = 972 + i * 90
    a('<rect x="%g" y="900" width="26" height="5" rx="2.5" fill="%s"/>' % (x, col))
    text(x, 922, lab, 11, C_MUTED, "400")

rows = [
    (u"Lijevi ULN2003", u"IN1→32  IN2→33  IN3→25  IN4→26"),
    (u"Desni ULN2003", u"IN1→27  IN2→14  IN3→12  IN4→13"),
    (u"ULN2003 (oba)", u"VCC→VIN    GND→GND"),
    (u"HC-SR04P", u"TRIG→17  ECHO→16  VCC→3V3"),
]
for i, (k, v) in enumerate(rows):
    y = 956 + i * 24
    text(972, y, k, 12, C_INK, "600")
    mono(1120, y, v, 12, C_MUTED, "500")

# ---------------------------------------------------------------- strujna bilanca
box(950, 560, 470, 260, C_NOTE_BG, C_BOARD_LINE, 12, 2)
text(972, 592, u"Strujna bilanca – power bank, ne računalo", 15, C_INK, "700")

text(972, 622, u"potrošač", 11.5, C_MUTED, "600")
text(1250, 622, "prosjek", 11.5, C_MUTED, "600", "end")
text(1400, 622, u"najgori slučaj", 11.5, C_MUTED, "600", "end")
a('<path d="M972 630 H1400" stroke="%s" stroke-width="1" opacity=".4"/>' % C_BOARD_LINE)

budget = [
    (u"2× 28BYJ-48 (polukoračno)", "300 mA", "400 mA", False),
    (u"ESP32 + Wi-Fi SoftAP", "120 mA", "450 mA", False),
    (u"HC-SR04P", "5 mA", "15 mA", False),
    (u"ukupno", "425 mA", "865 mA", True),
]
for i, (k, avg, peak, bold) in enumerate(budget):
    y = 652 + i * 24
    text(972, y, k, 12, C_INK, "700" if bold else "400")
    mono(1250, y, avg, 12, C_INK if bold else C_MUTED, "700" if bold else "500", "end")
    mono(1400, y, peak, 12, C_5V if bold else C_MUTED, "700" if bold else "500", "end")

a('<path d="M972 742 H1400" stroke="%s" stroke-width="1" opacity=".4"/>' % C_BOARD_LINE)
text(972, 768, u"USB 2.0 računala daje 500 mA → premalo, autić se resetira",
     11.5, C_5V, "600")
text(972, 792, u"Power bank daje 2000 mA → udobna rezerva",
     11.5, C_OK, "600")

box(210, 1090, 380, 64, C_WARN_BG, C_WARN_LINE, 10, 1.8)
text(226, 1113, u"GPIO 12 je strapping pin (MTDI)", 12, C_WARN_LINE, "700")
text(226, 1130, u"ULN2003 ulaz ga ne može podići, pa je spoj siguran.", 11, C_INK)
text(226, 1146, u"Ne spajaj ništa drugo na tu liniju.", 11, C_INK)

text(1420, 1145, u"mostić preko žice = križanje bez spoja", 11, C_MUTED, "400", "end")

a("</svg>")

svg = "\n".join(out)
target = os.path.join(os.path.dirname(os.path.abspath(__file__)), "shema_spajanja.svg")
io.open(target, "w", encoding="utf-8", newline="\n").write(svg)
print("napisano:", target, len(svg), "bajtova")
