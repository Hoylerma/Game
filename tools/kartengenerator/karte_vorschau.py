"""Politische Vorschau der Karte (nur zum Ansehen, nicht für das Spiel).

Staatsfarben je Ort (Besitzer aus dem jüngsten datierten Block), Orte ohne Daten hell
schraffiert, Kernraum-Land ohne Ort rosa, Ausland gedämpft, Meer und Seen blau, Flüsse, dazu
abgestufte Grenzen: Ortsgrenzen fein, Kreisgrenzen mittel, Staatsgrenzen kräftig (2 Pixel),
Küsten und Kernraumgrenze. Optional verkleinert (Mitteln über Blöcke), als Ausschnitt und mit
Ortsnamen am Beschriftungspunkt.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import numpy as np

import karte_raster as kr

if TYPE_CHECKING:  # pragma: no cover
    from kartengenerator import Ergebnis, Staat

FARBE_MEER = (156, 192, 222)
FARBE_SEE = (172, 206, 232)
FARBE_AUSLAND = (216, 212, 202)
FARBE_OHNE_ORT = (247, 190, 210)
FARBE_OHNE_DATEN = (238, 234, 222)
FARBE_SCHRAFFUR = (204, 196, 178)
FARBE_UNBEKANNT = (165, 165, 165)
FARBE_FLUSS = (66, 116, 182)
FARBE_STAATSGRENZE = (36, 28, 28)
FARBE_KREISGRENZE = (92, 82, 78)
FARBE_KUESTE = (58, 92, 138)
FARBE_KERNRAUMGRENZE = (140, 108, 78)
FARBE_LAGE = (20, 20, 20)
#: Abdunkeln der Füllfarbe für Ortsgrenzen.
ORTSGRENZE_FAKTOR = 0.78
#: Abstand der Schraffurlinien (Pixel der vollen Karte).
SCHRAFFUR_ABSTAND = 6

# 5×7-Pixelschrift (je Zeile 5 Bit, höchstwertiges Bit links) für Ortsnamen.
_SCHRIFT: dict[str, tuple[int, ...]] = {
    "A": (0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11),
    "B": (0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E),
    "C": (0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E),
    "D": (0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C),
    "E": (0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F),
    "F": (0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10),
    "G": (0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F),
    "H": (0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11),
    "I": (0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E),
    "J": (0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C),
    "K": (0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11),
    "L": (0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F),
    "M": (0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11),
    "N": (0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11),
    "O": (0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E),
    "P": (0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10),
    "Q": (0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D),
    "R": (0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11),
    "S": (0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E),
    "T": (0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04),
    "U": (0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E),
    "V": (0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04),
    "W": (0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A),
    "X": (0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11),
    "Y": (0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04),
    "Z": (0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F),
    "0": (0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E),
    "1": (0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E),
    "2": (0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F),
    "3": (0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E),
    "4": (0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02),
    "5": (0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E),
    "6": (0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E),
    "7": (0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08),
    "8": (0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E),
    "9": (0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C),
    "-": (0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00),
    ".": (0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C),
    "(": (0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02),
    ")": (0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08),
    " ": (0x00,) * 7,
}
_UMSCHRIFT = {"Ä": "AE", "Ö": "OE", "Ü": "UE", "ß": "SS", "É": "E", "È": "E", "Ł": "L", "Ś": "S", "Ż": "Z"}


def _beschriftung(text: str) -> str:
    gross = "".join(_UMSCHRIFT.get(c, c) for c in text.upper())
    return "".join(c if c in _SCHRIFT else " " for c in gross)


def _fenster(ergebnis: Ergebnis, ausschnitt: tuple[float, float, float, float] | None) -> tuple[int, int, int, int]:
    karte = ergebnis.karte
    if ausschnitt is None:
        return 0, 0, karte.breite, karte.hoehe
    l0, b0, l1, b1 = ausschnitt
    xs, ys = karte.nach_pixel(np.array([l0, l1]), np.array([b1, b0]))
    x0, x1 = max(int(np.floor(xs[0])), 0), min(int(np.ceil(xs[1])), karte.breite)
    y0, y1 = max(int(np.floor(ys[0])), 0), min(int(np.ceil(ys[1])), karte.hoehe)
    if x0 >= x1 or y0 >= y1:
        raise ValueError("der Vorschau-Ausschnitt liegt außerhalb der Karte")
    return x0, y0, x1, y1


def _grenzen(feld: np.ndarray, aktiv: np.ndarray | None = None) -> tuple[np.ndarray, np.ndarray]:
    """Pixel, deren rechter bzw. unterer Nachbar einen anderen Wert hat (nur wo aktiv auf beiden Seiten)."""
    rechts = np.zeros(feld.shape, dtype=bool)
    unten = np.zeros(feld.shape, dtype=bool)
    rechts[:, :-1] = feld[:, 1:] != feld[:, :-1]
    unten[:-1] = feld[1:] != feld[:-1]
    if aktiv is not None:
        rechts[:, :-1] &= aktiv[:, 1:] & aktiv[:, :-1]
        unten[:-1] &= aktiv[1:] & aktiv[:-1]
    return rechts, unten


def _skalieren(bild: np.ndarray, breite: int) -> np.ndarray:
    h, b = bild.shape[:2]
    if breite < b:
        k = b // breite
        if k > 1:
            hh, bb = (h // k) * k, (b // k) * k
            summe = np.zeros((hh // k, bb // k, 3), dtype=np.uint32)
            for dy in range(k):
                for dx in range(k):
                    summe += bild[dy:hh:k, dx:bb:k]
            bild = ((summe + (k * k) // 2) // (k * k)).astype(np.uint8)
    elif breite > b:
        k = breite // b
        if k > 1:
            bild = np.repeat(np.repeat(bild, k, axis=0), k, axis=1)
    h, b = bild.shape[:2]
    if b != breite:
        neu_h = max(1, round(h * breite / b))
        xs = (np.arange(breite) * b) // breite
        ys = (np.arange(neu_h) * h) // neu_h
        bild = bild[ys][:, xs]
    return bild


def vorschau(
    ergebnis: Ergebnis,
    staaten: dict[str, Staat],
    *,
    breite: int | None = None,
    ausschnitt: tuple[float, float, float, float] | None = None,
    namen: bool = False,
) -> np.ndarray:
    """Vorschau als (H, W, 3)-Feld (uint8)."""
    x0, y0, x1, y1 = _fenster(ergebnis, ausschnitt)
    werte = ergebnis.werte[y0:y1, x0:x1]
    art = ergebnis.art[y0:y1, x0:x1]
    orte = ergebnis.orte
    n = len(orte)

    # Eigenschaften je Wert (0 = kein Ort).
    besitzer_tags = sorted({o.besitzer for o in orte if o.besitzer})
    besitzer_nr = {t: k for k, t in enumerate(besitzer_tags)}
    kreis_namen = sorted({o.kreis for o in orte if o.kreis})
    kreis_nr = {t: k for k, t in enumerate(kreis_namen)}
    fuellung = np.zeros((n + 1, 3), dtype=np.uint8)
    besitzer = np.full(n + 1, -2, dtype=np.int64)
    kreis = np.full(n + 1, -2, dtype=np.int64)
    ohne_daten = np.zeros(n + 1, dtype=bool)
    for k, ort in enumerate(orte, start=1):
        staat = staaten.get(ort.besitzer)
        basis = staat.farbe if staat is not None else FARBE_UNBEKANNT
        faktor = (0.92, 1.0, 1.08)[(k * 2654435761 >> 7) % 3]
        fuellung[k] = [max(0, min(255, int(c * faktor))) for c in basis]
        besitzer[k] = besitzer_nr.get(ort.besitzer, -1)
        kreis[k] = kreis_nr.get(ort.kreis, -1)
        if not ort.hat_daten and not ort.besitzer:
            ohne_daten[k] = True
            fuellung[k] = FARBE_OHNE_DATEN

    bild = fuellung[werte]
    ist_ort = werte > 0
    bild[(art == kr.KERNLAND) & ~ist_ort] = FARBE_OHNE_ORT
    bild[art == kr.MEER] = FARBE_MEER
    bild[art == kr.BINNENGEWAESSER] = FARBE_SEE
    bild[art == kr.AUSLAND] = FARBE_AUSLAND
    # Bei verkleinerter Ausgabe gröbere Schraffur, damit sie das Mitteln übersteht.
    stufe = max(1, (x1 - x0) // breite) if breite is not None else 1
    gy, gx = np.mgrid[y0:y1, x0:x1]
    schraffur = ohne_daten[werte] & ((gx + gy) % (SCHRAFFUR_ABSTAND * stufe) < stufe)
    bild[schraffur] = FARBE_SCHRAFFUR

    if ergebnis.fluesse is not None:
        deckung = ergebnis.fluesse[y0:y1, x0:x1].astype(np.float32)[..., None] / 255.0
        fluss = np.array(FARBE_FLUSS, dtype=np.float32)
        bild = (bild.astype(np.float32) * (1.0 - deckung) + fluss * deckung + 0.5).astype(np.uint8)

    # Grenzen, von fein nach kräftig.
    r, u = _grenzen(werte, ist_ort)
    ortsgrenze = r | u
    bild[ortsgrenze] = (bild[ortsgrenze].astype(np.float32) * ORTSGRENZE_FAKTOR).astype(np.uint8)
    kreis_feld = kreis[werte]
    r, u = _grenzen(kreis_feld, ist_ort & (kreis_feld >= -1))
    bild[r | u] = FARBE_KREISGRENZE
    land = (art == kr.KERNLAND) | (art == kr.AUSLAND)
    r, u = _grenzen(art == kr.KERNLAND, land)
    bild[r | u] = FARBE_KERNRAUMGRENZE
    r, u = _grenzen(land)
    bild[r | u] = FARBE_KUESTE
    besitzer_feld = np.where(ist_ort, besitzer[werte], -3)
    r, u = _grenzen(besitzer_feld, ist_ort | (art == kr.KERNLAND))
    staatsgrenze = r | u
    staatsgrenze[:, 1:] |= r[:, :-1]
    staatsgrenze[1:] |= u[:-1]
    bild[staatsgrenze] = FARBE_STAATSGRENZE

    hoehe_v, breite_v = bild.shape[:2]
    if breite is not None:
        bild = _skalieren(bild, breite)
    skala_x = bild.shape[1] / breite_v
    skala_y = bild.shape[0] / hoehe_v
    hb, bb = bild.shape[:2]

    def setze(x: int, y: int, farbe: tuple[int, int, int]) -> None:
        if 0 <= x < bb and 0 <= y < hb:
            bild[y, x] = farbe

    # Lagepunkte.
    fx, fy = ergebnis.karte.nach_pixel(np.array([o.laenge for o in orte]), np.array([o.breite for o in orte]))
    punkt = 1 if skala_x >= 1.5 else 0
    for x, y in zip(fx, fy):
        px = int(np.floor((x - x0) * skala_x))
        py = int(np.floor((y - y0) * skala_y))
        for dy in range(-punkt, punkt + 1):
            for dx in range(-punkt, punkt + 1):
                setze(px + dx, py + dy, FARBE_LAGE)

    if namen:
        for flaeche in ergebnis.flaechen:
            if flaeche.x < 0:
                continue
            px = int((flaeche.x + 0.5 - x0) * skala_x)
            py = int((flaeche.y + 0.5 - y0) * skala_y)
            text = _beschriftung(flaeche.name)
            tx, ty = px - (6 * len(text) - 1) // 2, py + 3
            if tx + 6 * len(text) < 0 or tx >= bb or ty + 7 < 0 or ty >= hb:
                continue
            for durchgang in (0, 1):
                for nr, zeichen in enumerate(text):
                    for gyy, bits in enumerate(_SCHRIFT[zeichen]):
                        for gxx in range(5):
                            if bits & (0x10 >> gxx):
                                x, y = tx + 6 * nr + gxx, ty + gyy
                                if durchgang == 0:
                                    for ox, oy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                                        setze(x + ox, y + oy, (255, 255, 255))
                                else:
                                    setze(x, y, (20, 20, 20))
    return bild
