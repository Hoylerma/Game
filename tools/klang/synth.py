"""Synthesizer für EINIGKEIT (seit M13b2, docs/technik/vertical_slice.md 5.3).

Klavier, Streicher, Holz, Blech, Pauke und Trommel als einfache additive Modelle, Hall nach
Schroeder (vier Kammfilter, zwei Allpässe). Die Ausgabe ist **bitgleich** über numpy-Versionen und
Rechner: Sinus und Exponentialkurve kommen aus Tabellen, die mit dem math-Modul gerechnet und auf ein
Raster von 2⁻³⁰ gerundet sind; numpy rechnet danach nur Grundrechenarten, Summen in fester Reihenfolge
(cumsum) und Rundung. Rauschen stammt aus numpy.random.RandomState, dessen Folge numpy festschreibt.
Keine FFT, keine Faltung, kein np.sin/np.exp (deren SIMD-Wege unterscheiden sich je nach Version).
"""

from __future__ import annotations

import math
from fractions import Fraction

import numpy as np

from noten import Ereignis, Stueck

RATE = 44100
_RASTER = float(1 << 30)

_SIN_N = 16384
_SINUS = np.array([round(math.sin(2.0 * math.pi * i / _SIN_N) * _RASTER) / _RASTER for i in range(_SIN_N + 1)])
_EXP_SCHRITT = 256
_EXP_BIS = 40
_EXP = np.array(
    [round(math.exp(-i / _EXP_SCHRITT) * _RASTER) / _RASTER for i in range(_EXP_BIS * _EXP_SCHRITT + 2)]
)


def sinus(phase: np.ndarray) -> np.ndarray:
    """sin(2π·phase) aus der Tabelle, linear zwischen den Stützstellen."""
    p = phase - np.floor(phase)
    x = p * _SIN_N
    i = np.floor(x).astype(np.int64)
    f = x - i
    return _SINUS[i] + f * (_SINUS[i + 1] - _SINUS[i])


def abfall(x: np.ndarray) -> np.ndarray:
    """exp(−x) für x ≥ 0 aus der Tabelle; ab x = 40 null."""
    x = np.minimum(np.maximum(x, 0.0), float(_EXP_BIS))
    y = x * _EXP_SCHRITT
    i = np.floor(y).astype(np.int64)
    f = y - i
    return _EXP[i] + f * (_EXP[i + 1] - _EXP[i])


def frequenz(midi: int) -> float:
    """Gleichstufige Stimmung, a′ = 440 Hz (2^(1/12) als Tabelle über die Oktaven)."""
    return 440.0 * _HALBTON[(midi - 69) % 12] * (2.0 ** ((midi - 69) // 12))


_HALBTON = [round(2.0 ** (k / 12.0) * _RASTER) / _RASTER for k in range(12)]


def rauschen(n: int, saat: int) -> np.ndarray:
    """Gleichverteiltes Rauschen −1 … 1 (feste Folge)."""
    return np.random.RandomState(saat & 0x7FFFFFFF).random_sample(n) * 2.0 - 1.0


def gleitmittel(x: np.ndarray, breite: int) -> np.ndarray:
    """Gleitendes Mittel über breite Werte (über Summen, ohne Faltung)."""
    s = np.concatenate([[0.0], np.cumsum(x)])
    links = np.maximum(np.arange(len(x)) + 1 - breite, 0)
    return (s[np.arange(len(x)) + 1] - s[links]) / breite


def _rampe(n: int, an: int, aus: int) -> np.ndarray:
    """Hüllkurve: linear an über `an` Proben, linear aus über die letzten `aus` Proben."""
    h = np.ones(n)
    if an > 0:
        k = min(an, n)
        h[:k] = np.arange(k) / an
    if aus > 0:
        k = min(aus, n)
        h[n - k :] *= np.arange(k, 0, -1) / aus
    return h


# ─── Instrumente: (Grundfrequenz, Proben gehalten, Lautstärke, Saat) → Klang ──


def _klavier(f: float, halten: int, laut: float, saat: int) -> np.ndarray:
    nach = int(0.25 * RATE)
    n = halten + nach
    t = np.arange(n) / RATE
    tau0 = 1.6 * math.sqrt(220.0 / f) if f > 0 else 1.0
    klang = np.zeros(n)
    for k in range(1, 11):
        fk = k * f * math.sqrt(1.0 + 0.0004 * k * k)
        if fk > 0.45 * RATE:
            break
        ak = 1.0 / (k * (1.0 + 0.25 * k))
        klang += ak * sinus(fk * t) * abfall(t * ((1.0 + 0.7 * (k - 1)) / tau0))
    huelle = _rampe(n, int(0.003 * RATE), 0)
    daempfer = np.ones(n)
    daempfer[halten:] = abfall((np.arange(n - halten) / RATE) * 18.0)
    return klang * huelle * daempfer * laut


def _streicher(f: float, halten: int, laut: float, saat: int) -> np.ndarray:
    nach = int(0.22 * RATE)
    n = halten + nach
    t = np.arange(n) / RATE
    vib_tiefe = np.minimum(t / 0.4, 1.0) * 0.0025
    klang = np.zeros(n)
    for stimme, verstimmung in enumerate((0.9985, 1.0015)):
        vib = 1.0 + vib_tiefe * sinus(5.2 * t + 0.31 * stimme)
        phase = np.cumsum(f * verstimmung * vib) / RATE
        for k in range(1, 9):
            if k * f > 0.45 * RATE:
                break
            klang += (0.5 / k) * sinus(k * phase + 0.13 * k * stimme)
    return klang * _rampe(n, int(0.09 * RATE), nach) * laut * 0.6


_HOLZ = (1.0, 0.18, 0.45, 0.08, 0.2, 0.04)


def _holz(f: float, halten: int, laut: float, saat: int) -> np.ndarray:
    nach = int(0.08 * RATE)
    n = halten + nach
    t = np.arange(n) / RATE
    vib = 1.0 + np.minimum(t / 0.5, 1.0) * 0.002 * sinus(4.8 * t)
    phase = np.cumsum(f * vib) / RATE
    klang = np.zeros(n)
    for k, a in enumerate(_HOLZ, start=1):
        if k * f > 0.45 * RATE:
            break
        klang += a * sinus(k * phase)
    atem = gleitmittel(rauschen(n, saat), 6) * 0.04 * abfall(t * 8.0)
    return (klang + atem) * _rampe(n, int(0.04 * RATE), nach) * laut


def _blech(f: float, halten: int, laut: float, saat: int) -> np.ndarray:
    nach = int(0.1 * RATE)
    n = halten + nach
    t = np.arange(n) / RATE
    hell = 0.35 + 0.65 * np.minimum(t / 0.06, 1.0)  # Ansatz: erst dunkel, dann voll
    klang = np.zeros(n)
    faktor = np.ones(n)  # hell^(k−1), höchstens hell³, durch Multiplikation (kein np.power)
    # Seit dem Spieltest 11/2026 weicher: sieben statt zehn Teiltöne, längerer Ansatz.
    for k in range(1, 8):
        if k * f > 0.45 * RATE:
            break
        klang += (1.0 / k) * faktor * sinus(k * f * t)
        if k < 4:
            faktor = faktor * hell
    return klang * _rampe(n, int(0.05 * RATE), nach) * laut * 0.7


_PAUKE = ((1.0, 1.0), (1.5, 0.6), (1.99, 0.4), (2.44, 0.25))


def _pauke(f: float, halten: int, laut: float, saat: int) -> np.ndarray:
    n = max(halten, int(1.2 * RATE))
    t = np.arange(n) / RATE
    klang = np.zeros(n)
    for verh, a in _PAUKE:
        klang += a * sinus(verh * f * t) * abfall(t * (3.0 + 2.0 * verh))
    schlag = rauschen(n, saat) * abfall(t * 60.0) * 0.3
    return (klang + schlag) * laut


def _trommel(f: float, halten: int, laut: float, saat: int) -> np.ndarray:
    n = int(0.35 * RATE)
    t = np.arange(n) / RATE
    fell = sinus(180.0 * t) * abfall(t * 22.0)
    roh = rauschen(n, saat)
    teppich = (roh - gleitmittel(roh, 4)) * abfall(t * 14.0) * 0.7
    return (fell + teppich) * laut


INSTRUMENTE = {
    "klavier": _klavier,
    "streicher": _streicher,
    "holz": _holz,
    "blech": _blech,
    "pauke": _pauke,
    "trommel": _trommel,
}


# ─── Hall und Ausgabe ─────────────────────────────────────────────────────────


def _kamm(x: np.ndarray, verzoegerung: int, g: float) -> np.ndarray:
    """y[n] = x[n] + g·y[n−D], blockweise (Blöcke der Länge D)."""
    y = np.zeros(len(x))
    for a in range(0, len(x), verzoegerung):
        b = min(a + verzoegerung, len(x))
        if a == 0:
            y[a:b] = x[a:b]
        else:
            y[a:b] = x[a:b] + g * y[a - verzoegerung : b - verzoegerung]
    return y


def _allpass(x: np.ndarray, verzoegerung: int, g: float) -> np.ndarray:
    """y[n] = −g·x[n] + x[n−D] + g·y[n−D], blockweise."""
    y = np.zeros(len(x))
    for a in range(0, len(x), verzoegerung):
        b = min(a + verzoegerung, len(x))
        if a == 0:
            y[a:b] = -g * x[a:b]
        else:
            y[a:b] = -g * x[a:b] + x[a - verzoegerung : b - verzoegerung] + g * y[a - verzoegerung : b - verzoegerung]
    return y


def hall(x: np.ndarray, anteil: float) -> np.ndarray:
    if anteil <= 0.0:
        return x
    nass = np.zeros(len(x))
    for d, g in ((1557, 0.84), (1617, 0.83), (1491, 0.85), (1422, 0.86)):
        nass += _kamm(x, d, g)
    nass = _allpass(_allpass(nass * 0.25, 556, 0.5), 225, 0.5)
    return x * (1.0 - 0.5 * anteil) + nass * anteil


def normiere(x: np.ndarray, spitze: float = 0.89) -> np.ndarray:
    """Auf die Spitze skalieren und als 16 Bit runden."""
    hoch = float(np.max(np.abs(x))) if len(x) else 0.0
    if hoch > 0.0:
        x = x * (spitze / hoch)
    return np.round(x * 32767.0).astype(np.int16)


def _probe(zeit_ganze: Fraction, tempo: float) -> int:
    """Probennummer zu einer Zeit in Ganzen (vier Viertel je Ganze), exakt gerundet."""
    proben = zeit_ganze * 4 * 60 * RATE / Fraction(tempo).limit_denominator(1000)
    return int(round(proben))


def spiele(stueck: Stueck) -> np.ndarray:
    """Rendert ein Stück; Ergebnis int16 mono mit RATE Proben je Sekunde."""
    ende = _probe(stueck.dauer_ganze(), stueck.tempo) + int(2.5 * RATE)
    mix = np.zeros(ende)
    beginn_teil = Fraction(0)
    saat = 1848
    for name in stueck.form:
        teil = stueck.teile[name]
        for stimme_name, ereignisse in teil.stimmen.items():
            stimme = stueck.stimmen[stimme_name]
            klang = INSTRUMENTE[stimme.instrument]
            for e in ereignisse:
                _setze(mix, klang, e, beginn_teil, stueck.tempo, stimme.lautstaerke * teil.lautstaerke, saat)
                saat += 1
        beginn_teil += teil.laenge
    return normiere(hall(mix, stueck.hall))


def _setze(mix: np.ndarray, klang, e: Ereignis, versatz: Fraction, tempo: float, laut: float, saat: int) -> None:
    if not e.tonhoehen:
        return
    a = _probe(versatz + e.beginn, tempo)
    halten = max(_probe(versatz + e.beginn + e.dauer, tempo) - a - int(0.01 * RATE), 1)
    teil = laut / (1.0 + 0.35 * (len(e.tonhoehen) - 1))  # Akkorde nicht lauter als Einzeltöne
    for i, midi in enumerate(e.tonhoehen):
        welle = klang(frequenz(midi), halten, teil, saat * 16 + i)
        b = min(a + len(welle), len(mix))
        mix[a:b] += welle[: b - a]
