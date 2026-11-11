"""Bedienklänge der Epoche III (seit M13b2, docs/technik/vertical_slice.md 5.3), synthetisiert.

Gänsekiel (Befehl), Siegel (Vertrag), Papier (Fenster), Glocke (Ereignis), Trommel (Kriegserklärung) und
Telegraf (Meldung ab 1849). Jeder Klang ist eine kurze Funktion über den Bausteinen aus synth.py; die
Ausgabe ist wie die Musik bitgleich.
"""

from __future__ import annotations

import numpy as np

from synth import RATE, abfall, gleitmittel, hall, normiere, rauschen, sinus


def _t(sekunden: float) -> np.ndarray:
    return np.arange(int(sekunden * RATE)) / RATE


def gaensekiel() -> np.ndarray:
    """Zwei kratzende Federstriche."""
    t = _t(0.42)
    roh = rauschen(len(t), 11)
    hoch = roh - gleitmittel(roh, 3)
    striche = np.zeros(len(t))
    for start, dauer in ((0.02, 0.16), (0.22, 0.14)):
        im = (t >= start) & (t < start + dauer)
        lokal = (t - start) / dauer
        striche += im * (lokal * (1.0 - lokal) * 4.0)
    kratzen = 0.6 + 0.4 * sinus(t * 90.0)
    return normiere(hoch * striche * kratzen, 0.5)


def siegel() -> np.ndarray:
    """Dumpfer Druck in weiches Wachs, dann ein kurzes Knacken."""
    t = _t(0.5)
    druck = sinus(95.0 * t) * abfall(t * 14.0)
    knack = np.zeros(len(t))
    a = int(0.06 * RATE)
    k = rauschen(len(t) - a, 23) * abfall(t[: len(t) - a] * 90.0)
    knack[a:] = k - gleitmittel(k, 2)
    return normiere(hall(druck + 0.5 * knack, 0.15), 0.6)


def papier() -> np.ndarray:
    """Rascheln eines Bogens: drei kurze Rauschstöße."""
    t = _t(0.3)
    roh = rauschen(len(t), 37)
    band = gleitmittel(roh, 3) - gleitmittel(roh, 14)
    stoesse = np.zeros(len(t))
    for start, staerke in ((0.0, 1.0), (0.07, 0.7), (0.15, 0.45)):
        im = t >= start
        stoesse += im * staerke * abfall((t - start) * 28.0)
    return normiere(band * stoesse, 0.4)


_GLOCKE = ((0.56, 0.5, 1.2), (0.92, 0.6, 1.6), (1.19, 0.4, 2.2), (1.71, 0.3, 2.8), (2.0, 0.45, 3.0),
           (2.74, 0.2, 4.0), (3.0, 0.18, 4.5), (3.76, 0.12, 5.5), (4.07, 0.1, 6.0))


def glocke() -> np.ndarray:
    """Kleine Glocke (Partialtöne nach der Glockenreihe)."""
    t = _t(2.4)
    f = 659.25
    klang = np.zeros(len(t))
    for verh, a, d in _GLOCKE:
        klang += a * sinus(verh * f * t) * abfall(t * d)
    anschlag = rauschen(len(t), 41) * abfall(t * 120.0) * 0.2
    return normiere(hall(klang + anschlag, 0.25), 0.55)


def trommel() -> np.ndarray:
    """Wirbel mit Schlussschlag."""
    t = _t(1.3)
    aus = np.zeros(len(t))
    zeiten = [0.0, 0.16, 0.29, 0.39, 0.47, 0.54, 0.60, 0.66, 0.72, 0.90]
    for i, start in enumerate(zeiten):
        a = int(start * RATE)
        n = len(t) - a
        lokal = t[:n]
        roh = rauschen(n, 50 + i)
        schlag = (sinus(170.0 * lokal) * abfall(lokal * 24.0) + (roh - gleitmittel(roh, 4)) * abfall(lokal * 16.0) * 0.8)
        aus[a:] += schlag * (1.4 if i == len(zeiten) - 1 else 0.55 + 0.04 * i)
    return normiere(hall(aus, 0.2), 0.7)


def telegraf() -> np.ndarray:
    """Morsezeichen · — · auf 750 Hz."""
    t = _t(0.62)
    ton = sinus(750.0 * t)
    tasten = np.zeros(len(t))
    for start, dauer in ((0.02, 0.06), (0.14, 0.18), (0.38, 0.06)):
        a, b = int(start * RATE), int((start + dauer) * RATE)
        tasten[a:b] = 1.0
    weich = gleitmittel(tasten, int(0.004 * RATE))
    klick = np.zeros(len(t))
    klick[int(0.02 * RATE)] = 1.0
    return normiere(ton * weich * 0.8 + gleitmittel(klick, 8) * 3.0, 0.45)


KLAENGE = {
    "gaensekiel": gaensekiel,
    "siegel": siegel,
    "papier": papier,
    "glocke": glocke,
    "trommel": trommel,
    "telegraf": telegraf,
}
