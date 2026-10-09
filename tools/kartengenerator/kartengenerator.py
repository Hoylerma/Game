#!/usr/bin/env python3
"""Kartengenerator für EINIGKEIT.

Erzeugt aus data/karte/karte.chron und den Orten in data/history/orte/*.chron
das Ort-ID-Bild provinzen.png und die Zuordnungstabelle definition.csv
(Spezifikation: docs/technik/chronik-skript.md, Abschnitt 6).

Aufruf (siehe README.md im selben Ordner):

    python3 tools/kartengenerator/kartengenerator.py [--daten DIR] [--ausgabe DIR]
                                                     [--vorschau DATEI] [--pruefen]

Nur Python-Standardbibliothek (Python 3.10+). Die Ausgabe ist deterministisch:
Die Rechnung verwendet nur Grundrechenarten auf IEEE-754-Gleitkommazahlen (keine
Bibliotheksfunktionen wie math.cos, deren letzte Stelle je Plattform abweichen
kann), festes Rauschen ohne Zufallsquelle und keine Zeitstempel.

Exitcodes: 0 = Erfolg bzw. Dateien aktuell, 1 = --pruefen: Dateien nicht aktuell,
2 = Fehler in Daten oder Aufruf.
"""

from __future__ import annotations

import argparse
import bisect
import struct
import sys
import time
import zlib
from collections import Counter
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path
from typing import NoReturn

try:
    import chron
except ImportError:  # Aufruf als Modul von außerhalb des Werkzeugordners
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import chron

# ─── Konstanten ─────────────────────────────────────────────────────────────

#: Wurzel des Repositorys (tools/kartengenerator/ → zwei Ebenen höher).
REPO = Path(__file__).resolve().parents[2]

PI = 3.141592653589793
#: Kugelerde mit mittlerem Radius 6371 km: Kilometer je Grad Breite (und je Grad Länge am Äquator).
KM_PRO_GRAD = 6371.0 * PI / 180.0

#: Startwert des Rauschens. Eine Änderung verschiebt alle Grenzen.
RAUSCHEN_SEED = 1648
#: Oktaven des Domain Warping: (Wellenlänge in km, Amplitude in km). Die Summe der
#: Amplituden ist die größte mögliche Verschiebung je Achse.
RAUSCHEN_OKTAVEN: tuple[tuple[float, float], ...] = (
    (48.0, 6.0),
    (24.0, 3.5),
    (12.0, 2.0),
    (6.0, 1.1),
    (3.0, 0.6),
)
#: Gewichtung nach Fläche: 0 = reine Entfernung, 1 = Entfernung / sqrt(flaeche / mittlere Fläche).
#: Werte dazwischen mischen linear in 1 / Gewicht². Abgeschaltet: Die Zellgröße hängt fast nur
#: vom Abstand der Orte ab (Testregion: Korrelation Pixel–Fläche 0,46 ohne, 0,58 mit voller
#: Gewichtung), die Grenzen verschieben sich nur um wenige Kilometer.
FLAECHEN_GEWICHT = 0.0

#: Kantenlänge der Kacheln für die Kandidatensuche (Pixel).
KACHEL = 32
#: Höchstzahl der Runden beim Bereinigen abgetrennter Teilflächen.
BEREINIGUNG_RUNDEN = 10
#: Warnschwelle: weniger Pixel als dieser Anteil des Medians aller Orte.
WENIGE_PIXEL_ANTEIL = 0.1

#: Höchste Bildgröße je Achse (Schutz vor Tippfehlern).
MAX_BILDKANTE = 16384
#: Höchstzahl der Orte (R + G·256 = Kartenindex + 1 ≤ 65535).
MAX_ORTE = 65535

MEER = -1  # interner Wert für Meerpixel (in der Ausgabe 0)
MEER_SCHLUESSEL = -1  # Bereinigung: Nachbarschlüssel für Meer
LEER_SCHLUESSEL = -2  # Bereinigung: Nachbarschlüssel für leeres Land

CSV_KOPF = "kartenindex;ort;name"
PNG_SIGNATUR = b"\x89PNG\r\n\x1a\n"


class DatenFehler(Exception):
    """Fatale Fehler in den Eingabedaten (Liste von Meldungen)."""

    def __init__(self, meldungen: Sequence[str]) -> None:
        super().__init__("\n".join(meldungen))
        self.meldungen = list(meldungen)


@dataclass
class Meldungen:
    fehler: list[str] = field(default_factory=list)
    warnungen: list[str] = field(default_factory=list)

    def fehler_bei(self, datei: str, zeile: int, text: str) -> None:
        self.fehler.append(f"{datei}:{zeile}: {text}" if zeile > 0 else f"{datei}: {text}")

    def warnung_bei(self, datei: str, zeile: int, text: str) -> None:
        self.warnungen.append(f"{datei}:{zeile}: {text}" if zeile > 0 else f"{datei}: {text}")


# ─── Datenmodell ────────────────────────────────────────────────────────────


@dataclass(frozen=True)
class Karte:
    breite: int
    hoehe: int
    laenge_min: float
    laenge_max: float
    breite_min: float
    breite_max: float
    max_radius_km: float
    kueste: tuple[tuple[float, float], ...] = ()


@dataclass(frozen=True)
class Ort:
    kennung: str
    name: str
    laenge: float
    breite: float
    flaeche: float
    besitzer: str = ""
    datei: str = ""
    zeile: int = 0


@dataclass(frozen=True)
class Staat:
    tag: str
    name: str
    farbe: tuple[int, int, int]


# ─── Laden ──────────────────────────────────────────────────────────────────


def _datei_name(pfad: Path, daten: Path) -> str:
    try:
        return pfad.relative_to(daten).as_posix()
    except ValueError:
        return pfad.as_posix()


def _parse(pfad: Path, name: str, meldungen: Meldungen) -> chron.ScriptNode | None:
    ergebnis = chron.parse_script_file(pfad, name)
    for fehler in ergebnis.errors:
        meldungen.fehler.append(str(fehler))
    return ergebnis.root if ergebnis.ok else None


def chron_dateien(ordner: Path) -> list[Path]:
    """Alle *.chron-Dateien eines Ordners, nach Dateiname (Bytefolge) sortiert – wie der Lader."""
    if not ordner.is_dir():
        return []
    dateien = [p for p in ordner.iterdir() if p.suffix == ".chron" and p.is_file()]
    return sorted(dateien, key=lambda p: p.name.encode("utf-8", "surrogateescape"))


def _einmal(node: chron.ScriptNode, key: str, datei: str, meldungen: Meldungen) -> chron.ScriptNode | None:
    """Das Kind mit diesem Schlüssel; Fehler, wenn es mehrfach vorkommt."""
    gefunden = node.find_all(key)
    if len(gefunden) > 1:
        meldungen.fehler_bei(
            datei, gefunden[1].line, f"'{key}' ist mehrfach angegeben (zuerst in Zeile {gefunden[0].line})"
        )
    return gefunden[0] if gefunden else None


def _zahl(
    node: chron.ScriptNode,
    key: str,
    datei: str,
    was: str,
    meldungen: Meldungen,
    *,
    ganzzahl: bool = False,
) -> float | None:
    kind = _einmal(node, key, datei, meldungen)
    if kind is None:
        meldungen.fehler_bei(datei, node.line, f"Pflichtfeld '{key}' fehlt in {was}")
        return None
    if kind.is_block or kind.op != "=" or kind.quoted:
        meldungen.fehler_bei(datei, kind.line, f"'{key}' erwartet eine Zahl, z. B. {key} = 12")
        return None
    wert = chron.parse_ganzzahl(kind.value) if ganzzahl else chron.parse_zahl(kind.value)
    if wert is None:
        art = "Ganzzahl" if ganzzahl else "Zahl"
        meldungen.fehler_bei(datei, kind.line, f"ungültige {art} '{kind.value}' für '{key}'")
        return None
    return float(wert) if not ganzzahl else wert


def _zahlenliste(kind: chron.ScriptNode) -> list[float] | None:
    """Zahlen eines Blocks aus bare-Einträgen oder None, wenn etwas anderes darin steht."""
    if not kind.is_block:
        return None
    werte: list[float] = []
    for eintrag in kind.children:
        if eintrag.key or eintrag.is_block or eintrag.quoted:
            return None
        zahl = chron.parse_zahl(eintrag.value)
        if zahl is None:
            return None
        werte.append(zahl)
    return werte


KARTE_FELDER = (
    "bild_breite",
    "bild_hoehe",
    "laenge_min",
    "laenge_max",
    "breite_min",
    "breite_max",
    "max_radius_km",
    "kueste",
)


def lade_karte(pfad: Path, name: str, meldungen: Meldungen) -> Karte | None:
    """Liest karte.chron (Abschnitt 6.1). Fehler landen in meldungen."""
    if not pfad.is_file():
        meldungen.fehler_bei(name, 0, "Datei fehlt")
        return None
    root = _parse(pfad, name, meldungen)
    if root is None:
        return None
    karte_node: chron.ScriptNode | None = None
    ungueltig = False
    for node in root.children:
        if node.key != "karte":
            was = f"unbekannter Eintrag '{node.key}'" if node.key else "unerwarteter Eintrag"
            meldungen.warnung_bei(name, node.line, f"{was} wird ignoriert (erwartet: karte {{ … }})")
            continue
        if not node.is_block or node.value:
            meldungen.fehler_bei(name, node.line, "ungültige Definition, erwartet: karte { … }")
            ungueltig = True
            continue
        if karte_node is not None:
            meldungen.fehler_bei(
                name, node.line, f"zweiter Block 'karte' (der erste steht in Zeile {karte_node.line})"
            )
            continue
        karte_node = node
    if karte_node is None:
        if not ungueltig:
            meldungen.fehler_bei(name, 0, "Block 'karte { … }' fehlt")
        return None

    anzahl_fehler = len(meldungen.fehler)
    for kind in karte_node.children:
        if kind.key not in KARTE_FELDER:
            was = f"unbekanntes Feld '{kind.key}'" if kind.key else "unerwarteter Eintrag"
            meldungen.warnung_bei(name, kind.line, f"{was} in karte wird ignoriert")

    def ganz(key: str) -> int | None:
        wert = _zahl(karte_node, key, name, "karte", meldungen, ganzzahl=True)
        if wert is not None and not 1 <= wert <= MAX_BILDKANTE:
            zeile = karte_node.find(key).line  # type: ignore[union-attr]
            meldungen.fehler_bei(name, zeile, f"'{key}' muss zwischen 1 und {MAX_BILDKANTE} liegen")
            return None
        return int(wert) if wert is not None else None

    breite = ganz("bild_breite")
    hoehe = ganz("bild_hoehe")
    werte = {key: _zahl(karte_node, key, name, "karte", meldungen) for key in KARTE_FELDER[2:7]}

    def zeile_von(key: str) -> int:
        kind = karte_node.find(key)
        return kind.line if kind is not None else karte_node.line

    lmin, lmax = werte["laenge_min"], werte["laenge_max"]
    bmin, bmax = werte["breite_min"], werte["breite_max"]
    radius = werte["max_radius_km"]
    if lmin is not None and lmax is not None:
        if not (-180.0 <= lmin < lmax <= 180.0):
            meldungen.fehler_bei(
                name, zeile_von("laenge_max"), "erwartet −180 ≤ laenge_min < laenge_max ≤ 180"
            )
    if bmin is not None and bmax is not None:
        if not (-90.0 <= bmin < bmax <= 90.0):
            meldungen.fehler_bei(name, zeile_von("breite_max"), "erwartet −90 ≤ breite_min < breite_max ≤ 90")
    if radius is not None and not radius > 0.0:
        meldungen.fehler_bei(name, zeile_von("max_radius_km"), "'max_radius_km' muss größer als 0 sein")

    kueste: list[tuple[float, float]] = []
    kueste_node = _einmal(karte_node, "kueste", name, meldungen)
    if kueste_node is not None:
        zahlen = _zahlenliste(kueste_node)
        if zahlen is None or len(zahlen) < 4 or len(zahlen) % 2 != 0:
            meldungen.fehler_bei(
                name,
                kueste_node.line,
                "'kueste' erwartet einen Block mit mindestens zwei Punkten (Länge Breite …), "
                "z. B. kueste = { 9.5 54.6  10.9 54.0 }",
            )
        else:
            kueste = [(zahlen[k], zahlen[k + 1]) for k in range(0, len(zahlen), 2)]
            punkte = [e for e in kueste_node.children]
            for k in range(1, len(kueste)):
                if not kueste[k][0] > kueste[k - 1][0]:
                    meldungen.fehler_bei(
                        name,
                        punkte[2 * k].line,
                        f"'kueste': die Längengrade müssen streng steigen (Punkt {k + 1}: "
                        f"{kueste[k][0]} nach {kueste[k - 1][0]})",
                    )
                    break
            for k, (laenge, breite_k) in enumerate(kueste):
                if not (-180.0 <= laenge <= 180.0 and -90.0 <= breite_k <= 90.0):
                    meldungen.fehler_bei(
                        name, punkte[2 * k].line, f"'kueste': Punkt {k + 1} außerhalb des gültigen Bereichs"
                    )
                    break

    if len(meldungen.fehler) > anzahl_fehler or None in (breite, hoehe, lmin, lmax, bmin, bmax, radius):
        return None
    return Karte(
        breite=breite,  # type: ignore[arg-type]
        hoehe=hoehe,  # type: ignore[arg-type]
        laenge_min=lmin,  # type: ignore[arg-type]
        laenge_max=lmax,  # type: ignore[arg-type]
        breite_min=bmin,  # type: ignore[arg-type]
        breite_max=bmax,  # type: ignore[arg-type]
        max_radius_km=radius,  # type: ignore[arg-type]
        kueste=tuple(kueste),
    )


def _ist_ortskennung(text: str) -> bool:
    return bool(text) and all("a" <= c <= "z" or "0" <= c <= "9" or c == "_" for c in text)


def lade_orte(ordner: Path, daten: Path, meldungen: Meldungen) -> list[Ort]:
    """Liest alle Orte (Dateien sortiert, Einträge in Dateireihenfolge → Kartenindex)."""
    dateien = chron_dateien(ordner)
    if not dateien:
        meldungen.fehler_bei(_datei_name(ordner, daten), 0, "Ordner fehlt oder enthält keine .chron-Dateien")
        return []
    orte: list[Ort] = []
    gesehen: dict[str, str] = {}
    for pfad in dateien:
        name = _datei_name(pfad, daten)
        root = _parse(pfad, name, meldungen)
        if root is None:
            continue
        for node in root.children:
            if node.key != "ort":
                was = f"unbekannter Eintrag '{node.key}'" if node.key else "unerwarteter Eintrag"
                meldungen.warnung_bei(
                    name, node.line, f"{was} wird ignoriert (erwartet: ort <kennung> {{ … }})"
                )
                continue
            if not node.is_block or not node.value:
                meldungen.fehler_bei(name, node.line, "ungültige Definition, erwartet: ort <kennung> { … }")
                continue
            ort = _lese_ort(node, name, meldungen)
            if ort is None:
                continue
            if ort.kennung in gesehen:
                meldungen.fehler_bei(
                    name,
                    node.line,
                    f"doppelte Kennung '{ort.kennung}' (bereits definiert in {gesehen[ort.kennung]})",
                )
                continue
            gesehen[ort.kennung] = f"{name}:{node.line}"
            orte.append(ort)
    if len(orte) > MAX_ORTE:
        meldungen.fehler_bei(
            _datei_name(ordner, daten), 0, f"zu viele Orte ({len(orte)}, höchstens {MAX_ORTE})"
        )
    return orte


def _lese_ort(node: chron.ScriptNode, datei: str, meldungen: Meldungen) -> Ort | None:
    kennung = node.value
    was = f"ort {kennung}"
    anzahl = len(meldungen.fehler)
    if not _ist_ortskennung(kennung):
        meldungen.fehler_bei(
            datei, node.line, f"ungültige Ortskennung '{kennung}' (nur Kleinbuchstaben a–z, Ziffern und '_')"
        )
    name_node = _einmal(node, "name", datei, meldungen)
    name = ""
    if name_node is None:
        meldungen.fehler_bei(datei, node.line, f"Pflichtfeld 'name' fehlt in {was}")
    elif name_node.is_block or name_node.op != "=":
        meldungen.fehler_bei(
            datei, name_node.line, "'name' erwartet einen Text, z. B. name = \"Berlin-Cölln\""
        )
    else:
        name = name_node.value
        if ";" in name:
            meldungen.fehler_bei(
                datei, name_node.line, "'name' darf kein ';' enthalten (Trennzeichen der definition.csv)"
            )
    lage = _einmal(node, "lage", datei, meldungen)
    laenge = breite = 0.0
    if lage is None:
        meldungen.fehler_bei(datei, node.line, f"Pflichtfeld 'lage' fehlt in {was}")
    else:
        zahlen = _zahlenliste(lage)
        if zahlen is None or len(zahlen) != 2:
            meldungen.fehler_bei(
                datei,
                lage.line,
                "'lage' erwartet genau zwei Zahlen: Längengrad Ost und Breitengrad Nord, "
                "z. B. { 13.40 52.52 }",
            )
        elif not (-180.0 <= zahlen[0] <= 180.0 and -90.0 <= zahlen[1] <= 90.0):
            meldungen.fehler_bei(
                datei, lage.line, "'lage' außerhalb des gültigen Bereichs (Länge −180…180, Breite −90…90)"
            )
        else:
            laenge, breite = zahlen
    flaeche = _zahl(node, "flaeche", datei, was, meldungen)
    flaeche_node = node.find("flaeche")
    if flaeche is not None and flaeche_node is not None and not flaeche > 0.0:
        meldungen.fehler_bei(datei, flaeche_node.line, "'flaeche' muss größer als 0 sein")
    besitzer = node.get_string("besitzer") or ""
    if len(meldungen.fehler) > anzahl or flaeche is None:
        return None
    return Ort(kennung, name, laenge, breite, flaeche, besitzer, datei, node.line)


def lade_staaten(ordner: Path, daten: Path, meldungen: Meldungen) -> dict[str, Staat]:
    """Staaten für die Vorschau (Tag → Farbe). Probleme sind nur Warnungen."""
    staaten: dict[str, Staat] = {}
    lokal = Meldungen()
    for pfad in chron_dateien(ordner):
        name = _datei_name(pfad, daten)
        root = _parse(pfad, name, lokal)
        if root is None:
            continue
        for node in root.children:
            if node.key != "staat" or not node.is_block or not node.value:
                continue
            farbe_node = node.find("farbe")
            zahlen = _zahlenliste(farbe_node) if farbe_node is not None else None
            if zahlen is None or len(zahlen) != 3 or not all(z == int(z) and 0 <= z <= 255 for z in zahlen):
                lokal.fehler_bei(name, node.line, f"staat {node.value}: 'farbe' fehlt oder ist ungültig")
                continue
            farbe = (int(zahlen[0]), int(zahlen[1]), int(zahlen[2]))
            staaten[node.value] = Staat(node.value, node.get_string("name") or node.value, farbe)
    for text in lokal.fehler:
        meldungen.warnungen.append(f"{text} (Vorschau)")
    return staaten


# ─── Geometrie ──────────────────────────────────────────────────────────────


def cos_grad(grad: float) -> float:
    """Kosinus eines Winkels in Grad, nur mit Grundrechenarten (plattformunabhängig bitgleich)."""
    x = grad * (PI / 180.0)
    zwei_pi = 2.0 * PI
    while x > PI:
        x -= zwei_pi
    while x < -PI:
        x += zwei_pi
    x2 = x * x
    # Taylorreihe bis x^30 (Horner); für |x| ≤ π genauer als 1e-16.
    summe = 1.0
    for n in range(30, 0, -2):
        summe = 1.0 - x2 / (n * (n - 1)) * summe
    return summe


def kuesten_breite(kueste: Sequence[tuple[float, float]], laenge: float) -> float | None:
    """Breite der Küstenlinie bei dieser Länge (lineare Interpolation) oder None außerhalb des Linienzugs."""
    if len(kueste) < 2 or laenge < kueste[0][0] or laenge > kueste[-1][0]:
        return None
    k = bisect.bisect_right([p[0] for p in kueste], laenge) - 1
    if k >= len(kueste) - 1:
        return kueste[-1][1]
    (l0, b0), (l1, b1) = kueste[k], kueste[k + 1]
    return b0 + (b1 - b0) * ((laenge - l0) / (l1 - l0))


def ist_land(kueste: Sequence[tuple[float, float]], laenge: float, breite: float) -> bool:
    """Land: südlich der Küstenlinie oder außerhalb ihres Längenbereichs; nördlich davon Meer."""
    grenze = kuesten_breite(kueste, laenge)
    return grenze is None or breite <= grenze


@dataclass
class Raster:
    """Pixelmittelpunkte, Maßstäbe und Landmaske eines Kartenrechtecks."""

    karte: Karte
    breite: int
    hoehe: int
    grad_x: float  # Grad Länge je Pixel
    grad_y: float  # Grad Breite je Pixel
    laenge_spalte: list[float]
    breite_zeile: list[float]
    kx_zeile: list[float]  # km je Grad Länge in jeder Zeile
    ky: float  # km je Grad Breite
    kx_ref: float  # km je Grad Länge in der Mitte (für das Rauschen)
    erstes_land: list[int]  # je Spalte: erste Zeile, die Land ist (Zeilen darüber sind Meer)

    @staticmethod
    def aus_karte(karte: Karte) -> Raster:
        b, h = karte.breite, karte.hoehe
        grad_x = (karte.laenge_max - karte.laenge_min) / b
        grad_y = (karte.breite_max - karte.breite_min) / h
        laenge_spalte = [karte.laenge_min + (x + 0.5) * grad_x for x in range(b)]
        breite_zeile = [karte.breite_max - (y + 0.5) * grad_y for y in range(h)]
        kx_zeile = [KM_PRO_GRAD * cos_grad(lat) for lat in breite_zeile]
        kx_ref = KM_PRO_GRAD * cos_grad((karte.breite_min + karte.breite_max) / 2.0)
        negiert = [-lat for lat in breite_zeile]  # steigend
        erstes_land = []
        for laenge in laenge_spalte:
            grenze = kuesten_breite(karte.kueste, laenge)
            erstes_land.append(0 if grenze is None else bisect.bisect_left(negiert, -grenze))
        return Raster(
            karte,
            b,
            h,
            grad_x,
            grad_y,
            laenge_spalte,
            breite_zeile,
            kx_zeile,
            KM_PRO_GRAD,
            kx_ref,
            erstes_land,
        )

    def pixel_von(self, laenge: float, breite: float) -> tuple[int, int] | None:
        """Pixel, in dem ein Punkt liegt, oder None außerhalb des Bildes."""
        fx = (laenge - self.karte.laenge_min) / self.grad_x
        fy = (self.karte.breite_max - breite) / self.grad_y
        if not (0.0 <= fx < self.breite and 0.0 <= fy < self.hoehe):
            return None
        return int(fx), int(fy)


# ─── Deterministisches Rauschen (Value Noise, Domain Warping) ───────────────

_M32 = 0xFFFFFFFF


def _hash32(i: int, j: int, saat: int) -> int:
    h = (saat * 0x9E3779B1 + i * 0x85EBCA77 + j * 0xC2B2AE3D) & _M32
    h ^= h >> 15
    h = (h * 0x2C1B3C6D) & _M32
    h ^= h >> 12
    h = (h * 0x297A2D39) & _M32
    h ^= h >> 15
    return h


def gitterwert(i: int, j: int, saat: int) -> float:
    """Wert des Gitterpunkts (i, j) in [−1, 1), exakt darstellbar."""
    return (_hash32(i, j, saat) >> 8) * (1.0 / 8388608.0) - 1.0


def _glaetten(t: float) -> float:
    """Quintische Glättung 6t⁵ − 15t⁴ + 10t³."""
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def _saat(feld: int, oktave: int) -> int:
    return (RAUSCHEN_SEED << 8) | (feld << 4) | oktave


def verschiebung_km(x_km: float, y_km: float) -> tuple[float, float]:
    """Verschiebung (nach Osten, nach Süden) in km am Punkt (x_km, y_km) des Rauschgebiets.

    Rechnet in derselben Reihenfolge wie die zeilenweise Auswertung in zuordnen(),
    daher bitgleich an Pixelmittelpunkten.
    """
    ergebnis = []
    for feld in (0, 1):
        summe = 0.0
        for oktave, (welle, amplitude) in enumerate(RAUSCHEN_OKTAVEN):
            saat = _saat(feld, oktave)
            u = x_km / welle
            v = y_km / welle
            i = int(u // 1.0)
            j = int(v // 1.0)
            tx = _glaetten(u - i)
            ty = _glaetten(v - j)
            a00 = gitterwert(i, j, saat)
            a01 = gitterwert(i, j + 1, saat)
            a10 = gitterwert(i + 1, j, saat)
            a11 = gitterwert(i + 1, j + 1, saat)
            c0 = a00 + (a01 - a00) * ty
            c1 = a10 + (a11 - a10) * ty
            summe = summe + amplitude * (c0 + (c1 - c0) * tx)
        ergebnis.append(summe)
    return ergebnis[0], ergebnis[1]


class _ZeilenRauschen:
    """Zeilenweise Auswertung des Warp-Rauschens für alle Pixelmittelpunkte."""

    def __init__(self, raster: Raster) -> None:
        karte = raster.karte
        self.raster = raster
        self.x_km = [(lon - karte.laenge_min) * raster.kx_ref for lon in raster.laenge_spalte]
        self.oktaven = []
        for oktave, (welle, amplitude) in enumerate(RAUSCHEN_OKTAVEN):
            spalten_i = []
            spalten_t = []
            for x in self.x_km:
                u = x / welle
                i = int(u // 1.0)
                spalten_i.append(i)
                spalten_t.append(_glaetten(u - i))
            i0 = min(spalten_i)
            i1 = max(spalten_i) + 1
            rel = [i - i0 for i in spalten_i]
            tabellen = []
            for feld in (0, 1):
                saat = _saat(feld, oktave)
                tabellen.append({"saat": saat, "zeilen": {}})
            self.oktaven.append((welle, amplitude, i0, i1, rel, spalten_t, tabellen))

    def _gitterzeile(self, tabelle: dict, j: int, i0: int, i1: int) -> list[float]:
        zeilen = tabelle["zeilen"]
        zeile = zeilen.get(j)
        if zeile is None:
            saat = tabelle["saat"]
            zeile = [gitterwert(i, j, saat) for i in range(i0, i1 + 1)]
            zeilen[j] = zeile
            # Alte Gitterzeilen verwerfen (Zeilen werden aufsteigend abgefragt).
            for alt in [k for k in zeilen if k < j - 1]:
                del zeilen[alt]
        return zeile

    def zeile(self, y: int) -> tuple[list[float], list[float]]:
        """Verschiebung in km (Ost, Süd) für alle Pixel der Zeile y."""
        y_km = (self.raster.karte.breite_max - self.raster.breite_zeile[y]) * self.raster.ky
        breite = self.raster.breite
        summen: list[list[float]] = [[0.0] * breite, [0.0] * breite]
        for welle, amplitude, i0, i1, rel, spalten_t, tabellen in self.oktaven:
            v = y_km / welle
            j = int(v // 1.0)
            ty = _glaetten(v - j)
            for feld in (0, 1):
                g0 = self._gitterzeile(tabellen[feld], j, i0, i1)
                g1 = self._gitterzeile(tabellen[feld], j + 1, i0, i1)
                linie = [a + (b - a) * ty for a, b in zip(g0, g1)]
                diff = [b - a for a, b in zip(linie, linie[1:])]
                diff.append(0.0)
                summen[feld] = [
                    s + amplitude * (linie[i] + diff[i] * t) for s, i, t in zip(summen[feld], rel, spalten_t)
                ]
        return summen[0], summen[1]


# ─── Zuordnung ──────────────────────────────────────────────────────────────


@dataclass
class Zentrum:
    """Verschobene Lage eines Ortes (in Grad) und seine Gewichtung."""

    laenge: float
    breite: float
    inv_gewicht2: float  # 1 / Gewicht² (Faktor auf die quadrierte Entfernung)
    wert: int  # Kartenindex + 1


def zentren(raster: Raster, orte: Sequence[Ort]) -> list[Zentrum]:
    karte = raster.karte
    mittel = sum(o.flaeche for o in orte) / len(orte) if orte else 1.0
    ergebnis = []
    for index, ort in enumerate(orte):
        x_km = (ort.laenge - karte.laenge_min) * raster.kx_ref
        y_km = (karte.breite_max - ort.breite) * raster.ky
        ost, sued = verschiebung_km(x_km, y_km)
        inv = 1.0 + FLAECHEN_GEWICHT * (mittel / ort.flaeche - 1.0)
        ergebnis.append(
            Zentrum(ort.laenge + ost / raster.kx_ref, ort.breite - sued / raster.ky, inv, index + 1)
        )
    return ergebnis


def _kandidaten(
    raster: Raster,
    zent: Sequence[Zentrum],
    x0: int,
    x1: int,
    y0: int,
    y1: int,
    rand_lon: float,
    rand_lat: float,
) -> list[tuple[float, float, float, int]]:
    """Orte, die in der Kachel [x0, x1) × [y0, y1) gewinnen können (konservative Schranken)."""
    r2 = raster.karte.max_radius_km * raster.karte.max_radius_km
    lon_lo = raster.laenge_spalte[x0] - rand_lon
    lon_hi = raster.laenge_spalte[x1 - 1] + rand_lon
    lat_hi = raster.breite_zeile[y0] + rand_lat
    lat_lo = raster.breite_zeile[y1 - 1] - rand_lat
    kx = raster.kx_zeile[y0:y1]
    kx_min, kx_max = min(kx), max(kx)
    ky = raster.ky
    schranken = []
    obere = float("inf")
    for z in zent:
        dlon_min = (
            lon_lo - z.laenge if z.laenge < lon_lo else (z.laenge - lon_hi if z.laenge > lon_hi else 0.0)
        )
        dlat_min = (
            lat_lo - z.breite if z.breite < lat_lo else (z.breite - lat_hi if z.breite > lat_hi else 0.0)
        )
        dlon_max = max(abs(z.laenge - lon_lo), abs(z.laenge - lon_hi))
        dlat_max = max(abs(z.breite - lat_lo), abs(z.breite - lat_hi))
        ax, ay = dlon_min * kx_min, dlat_min * ky
        d2_min = ax * ax + ay * ay
        if d2_min > r2 * (1.0 + 1e-9):
            continue
        bx, by = dlon_max * kx_max, dlat_max * ky
        d2_max = bx * bx + by * by
        unten = d2_min * z.inv_gewicht2
        if d2_max < r2 * (1.0 - 1e-9):
            obere = min(obere, d2_max * z.inv_gewicht2)
        schranken.append((unten, z))
    grenze = obere * (1.0 + 1e-9) + 1e-9
    return [(z.laenge, z.breite, z.inv_gewicht2, z.wert) for unten, z in schranken if unten <= grenze]


def zuordnen(raster: Raster, orte: Sequence[Ort], *, beschleunigt: bool = True) -> list[list[int]]:
    """Ordnet jedem Pixel einen Ort zu: Wert = Kartenindex + 1, 0 = kein Ort, MEER = Meer.

    beschleunigt=False prüft jeden Ort für jedes Pixel (Referenz für Tests).
    """
    zent = zentren(raster, orte)
    r2 = raster.karte.max_radius_km * raster.karte.max_radius_km
    rauschen = _ZeilenRauschen(raster)
    amplitude = sum(a for _, a in RAUSCHEN_OKTAVEN)
    rand_lon = amplitude / raster.kx_ref * (1.0 + 1e-6) + 1e-9
    rand_lat = amplitude / raster.ky * (1.0 + 1e-6) + 1e-9
    b, h = raster.breite, raster.hoehe
    kacheln_x = (b + KACHEL - 1) // KACHEL
    alle = [(z.laenge, z.breite, z.inv_gewicht2, z.wert) for z in zent]
    erstes_land = raster.erstes_land
    laenge_spalte = raster.laenge_spalte
    ky = raster.ky
    kx_ref = raster.kx_ref
    zeilen: list[list[int]] = []
    kandidaten: list[list[tuple[float, float, float, int]]] = []
    for y in range(h):
        if y % KACHEL == 0:
            y1 = min(y + KACHEL, h)
            if beschleunigt:
                kandidaten = [
                    _kandidaten(raster, zent, k * KACHEL, min((k + 1) * KACHEL, b), y, y1, rand_lon, rand_lat)
                    for k in range(kacheln_x)
                ]
            else:
                kandidaten = [alle] * kacheln_x
        ost, sued = rauschen.zeile(y)
        lat_y = raster.breite_zeile[y]
        kx = raster.kx_zeile[y]
        lonw = [lon + d / kx_ref for lon, d in zip(laenge_spalte, ost)]
        latw = [lat_y - d / ky for d in sued]
        zeile = [0] * b
        for k in range(kacheln_x):
            x0 = k * KACHEL
            x1 = min(x0 + KACHEL, b)
            kand = kandidaten[k]
            for x in range(x0, x1):
                if y < erstes_land[x]:
                    zeile[x] = MEER
                    continue
                if not kand:
                    continue
                lw = lonw[x]
                aw = latw[x]
                bester = 0
                beste_d = float("inf")
                for zl, zb, inv, wert in kand:
                    dx = (lw - zl) * kx
                    dy = (aw - zb) * ky
                    d2 = dx * dx + dy * dy
                    if d2 <= r2:
                        d2 *= inv
                        if d2 < beste_d:
                            beste_d = d2
                            bester = wert
                zeile[x] = bester
        zeilen.append(zeile)
    return zeilen


# ─── Bereinigung abgetrennter Teilflächen ───────────────────────────────────


def _laeufe(zeile: list[int]) -> list[tuple[int, int, int]]:
    """Läufe gleicher Werte einer Zeile als (x0, x1, wert)."""
    grenzen = [x for x, (a, b) in enumerate(zip(zeile, zeile[1:]), 1) if a != b]
    starts = [0] + grenzen
    enden = grenzen + [len(zeile)]
    return [(x0, x1, zeile[x0]) for x0, x1 in zip(starts, enden)]


class _Teilflaechen:
    """Zusammenhangskomponenten (4er-Nachbarschaft) einer Wertekarte über Läufe je Zeile."""

    def __init__(self, zeilen: list[list[int]]) -> None:
        self.laeufe: list[tuple[int, int, int, int]] = []  # (y, x0, x1, wert)
        self.zeilen_laeufe: list[list[int]] = []
        for y, zeile in enumerate(zeilen):
            ids = []
            for x0, x1, wert in _laeufe(zeile):
                ids.append(len(self.laeufe))
                self.laeufe.append((y, x0, x1, wert))
            self.zeilen_laeufe.append(ids)
        self.eltern = list(range(len(self.laeufe)))
        # Senkrechte Nachbarschaften: Paare überlappender Läufe benachbarter Zeilen.
        self.paare: list[tuple[int, int, int]] = []  # (oben, unten, Überlappung)
        laeufe = self.laeufe
        for y in range(1, len(zeilen)):
            oben, unten = self.zeilen_laeufe[y - 1], self.zeilen_laeufe[y]
            a = c = 0
            while a < len(oben) and c < len(unten):
                _, ax0, ax1, aw = laeufe[oben[a]]
                _, cx0, cx1, cw = laeufe[unten[c]]
                ueberlappung = min(ax1, cx1) - max(ax0, cx0)
                if ueberlappung > 0:
                    self.paare.append((oben[a], unten[c], ueberlappung))
                    if aw == cw and aw > 0:  # nur Orte verbinden, nicht Meer oder leeres Land
                        ra, rc = self.wurzel(oben[a]), self.wurzel(unten[c])
                        if ra != rc:
                            self.eltern[max(ra, rc)] = min(ra, rc)
                if ax1 <= cx1:
                    a += 1
                else:
                    c += 1
        # Die Wurzel ist stets der kleinste Lauf der Komponente (erstes Auftreten).
        self.groesse: Counter[int] = Counter()
        for lid, (_, x0, x1, wert) in enumerate(laeufe):
            if wert > 0:
                self.groesse[self.wurzel(lid)] += x1 - x0

    def wurzel(self, a: int) -> int:
        eltern = self.eltern
        while eltern[a] != a:
            eltern[a] = eltern[eltern[a]]
            a = eltern[a]
        return a

    def wert(self, lauf: int) -> int:
        return self.laeufe[lauf][3]

    def komponente_bei(self, x: int, y: int) -> int | None:
        for lid in self.zeilen_laeufe[y]:
            _, x0, x1, _ = self.laeufe[lid]
            if x0 <= x < x1:
                return self.wurzel(lid)
        return None

    def schluessel(self, lauf: int) -> int:
        """Nachbarschlüssel: Komponente bei Orten, sonst LEER_SCHLUESSEL bzw. MEER_SCHLUESSEL."""
        wert = self.laeufe[lauf][3]
        if wert > 0:
            return self.wurzel(lauf)
        return MEER_SCHLUESSEL if wert == MEER else LEER_SCHLUESSEL

    def wert_von(self, schluessel: int) -> int:
        if schluessel >= 0:
            return self.laeufe[schluessel][3]
        return MEER if schluessel == MEER_SCHLUESSEL else 0

    def grenzen(self, komponenten: set[int]) -> dict[int, Counter[int]]:
        """Grenzlänge (Pixelkanten) jeder dieser Komponenten zu jedem Nachbarschlüssel."""
        ergebnis: dict[int, Counter[int]] = {r: Counter() for r in komponenten}

        def zaehle(lauf: int, nachbar: int, laenge: int) -> None:
            r = self.wurzel(lauf)
            if r in ergebnis:
                ergebnis[r][self.schluessel(nachbar)] += laenge

        for ids in self.zeilen_laeufe:
            for a, c in zip(ids, ids[1:]):
                zaehle(a, c, 1)
                zaehle(c, a, 1)
        for oben, unten, ueberlappung in self.paare:
            if self.wert(oben) != self.wert(unten):
                zaehle(oben, unten, ueberlappung)
                zaehle(unten, oben, ueberlappung)
        return ergebnis


def _bereinigungsrunde(
    zeilen: list[list[int]], lagepixel: dict[int, tuple[int, int]], *, aendern: bool = True
) -> tuple[int, set[int]]:
    """Eine Runde der Bereinigung.

    Gibt die Zahl der umverteilten Teilflächen und die Werte der Orte zurück, die zu
    Beginn der Runde aus mehreren Teilflächen bestanden. aendern=False zählt nur.
    """
    tf = _Teilflaechen(zeilen)
    haupt: dict[int, int] = {}
    for r in sorted(tf.groesse):  # aufsteigend = Reihenfolge des ersten Auftretens
        wert = tf.wert(r)
        if wert not in haupt or tf.groesse[r] > tf.groesse[haupt[wert]]:
            haupt[wert] = r
    for wert, (px, py) in lagepixel.items():
        if 0 <= py < len(zeilen) and 0 <= px < len(zeilen[py]) and zeilen[py][px] == wert:
            r = tf.komponente_bei(px, py)
            if r is not None:
                haupt[wert] = r
    teile = {r for r in tf.groesse if haupt[tf.wert(r)] != r}
    zerteilt = {tf.wert(r) for r in teile}
    if not teile or not aendern:
        return 0, zerteilt
    grenzen = tf.grenzen(teile)

    # Kleinste Teilflächen zuerst. Eine Teilfläche, die in dieser Runde Ziel einer
    # Umverteilung war, wartet auf die nächste Runde; ebenso eine, deren bester Nachbar
    # in dieser Runde schon umverteilt wurde. So pendeln benachbarte Teilflächen nicht.
    neu: dict[int, int] = {}
    gesperrt: set[int] = set()
    for r in sorted(teile, key=lambda k: (tf.groesse[k], k)):
        if r in gesperrt:
            continue
        land = [
            (anzahl, -tf.wert_von(key), -key, key)
            for key, anzahl in grenzen[r].items()
            if key != MEER_SCHLUESSEL
        ]
        if not land:
            continue  # nur von Meer umgeben: nicht zuzuordnen
        bester = max(land)[3]
        if bester in neu:
            continue
        neu[r] = tf.wert_von(bester)
        if bester >= 0:
            gesperrt.add(bester)
    for lid, (y, x0, x1, wert) in enumerate(tf.laeufe):
        if wert > 0:
            ziel = neu.get(tf.wurzel(lid))
            if ziel is not None:
                zeilen[y][x0:x1] = [ziel] * (x1 - x0)
    return len(neu), zerteilt


def bereinigen(zeilen: list[list[int]], lagepixel: dict[int, tuple[int, int]]) -> tuple[int, set[int]]:
    """Gibt abgetrennte Teilflächen eines Ortes dem Nachbarn mit der längsten gemeinsamen Grenze.

    Je Ort bleibt die Zusammenhangskomponente (4er-Nachbarschaft) erhalten, die das
    Lagepixel des Ortes enthält, sonst die größte. Meer zählt nicht als Nachbar; ist
    der längste Nachbar leeres Land, wird die Teilfläche leer. Ändert zeilen und gibt
    die Zahl der umverteilten Teilflächen sowie die Werte der Orte zurück, die danach
    noch aus mehreren Teilflächen bestehen (z. B. nur von Meer umgeben).
    """
    umverteilt = 0
    for _ in range(BEREINIGUNG_RUNDEN):
        anzahl, zerteilt = _bereinigungsrunde(zeilen, lagepixel)
        umverteilt += anzahl
        if anzahl == 0:
            return umverteilt, zerteilt
    return umverteilt, _bereinigungsrunde(zeilen, lagepixel, aendern=False)[1]


# ─── Gesamtablauf ───────────────────────────────────────────────────────────


@dataclass
class Ergebnis:
    raster: Raster
    orte: list[Ort]
    zeilen: list[list[int]]  # Kartenindex + 1, 0 = leer, MEER = Meer
    pixel: list[int]  # Pixel je Ort (Kartenindex)
    umverteilt: int
    warnungen: list[str]


def erzeuge(karte: Karte, orte: Sequence[Ort], *, beschleunigt: bool = True) -> Ergebnis:
    raster = Raster.aus_karte(karte)
    zeilen = zuordnen(raster, orte, beschleunigt=beschleunigt)
    lagepixel: dict[int, tuple[int, int]] = {}
    for index, ort in enumerate(orte):
        p = raster.pixel_von(ort.laenge, ort.breite)
        if p is not None:
            lagepixel[index + 1] = p
    umverteilt, zerteilt = bereinigen(zeilen, lagepixel)

    zaehler: Counter[int] = Counter()
    for zeile in zeilen:
        zaehler.update(zeile)
    pixel = [zaehler.get(index + 1, 0) for index in range(len(orte))]

    warnungen: list[str] = []
    belegt = sorted(n for n in pixel if n > 0)
    median = belegt[len(belegt) // 2] if belegt else 0
    schwelle = median * WENIGE_PIXEL_ANTEIL
    for index, ort in enumerate(orte):
        wo = f"{ort.datei}:{ort.zeile}: ort {ort.kennung}"
        p = lagepixel.get(index + 1)
        if p is None:
            warnungen.append(f"{wo}: Lage {ort.laenge} {ort.breite} liegt außerhalb des Kartenausschnitts")
        else:
            wert = zeilen[p[1]][p[0]]
            if wert == MEER:
                warnungen.append(
                    f"{wo}: Lage {ort.laenge} {ort.breite} liegt im Meer (nördlich der Küstenlinie)"
                )
            elif wert != index + 1:
                gehoert = f"ort {orte[wert - 1].kennung}" if wert > 0 else "keinem Ort"
                warnungen.append(f"{wo}: das Pixel der Lage gehört zu {gehoert}")
        if index + 1 in zerteilt:
            warnungen.append(f"{wo}: besteht aus mehreren getrennten Teilflächen")
        if pixel[index] == 0:
            warnungen.append(f"{wo}: erhält keine Pixel")
        elif pixel[index] < schwelle:
            warnungen.append(
                f"{wo}: erhält nur {pixel[index]} Pixel (Median aller Orte: {median}); Nachbarorte zu nah?"
            )
    return Ergebnis(raster, list(orte), zeilen, pixel, umverteilt, warnungen)


# ─── PNG ────────────────────────────────────────────────────────────────────


def _chunk(typ: bytes, daten: bytes) -> bytes:
    return struct.pack(">I", len(daten)) + typ + daten + struct.pack(">I", zlib.crc32(typ + daten) & _M32)


def png_kodieren(breite: int, hoehe: int, zeilen_rgb: Sequence[bytes]) -> bytes:
    """RGB-PNG, 8 Bit je Kanal, ohne Zeitstempel und Zusatzblöcke.

    Filter: erste Zeile 'Sub', alle weiteren 'Up' (bei Ort-ID-Bildern meist Nullen).
    """
    if len(zeilen_rgb) != hoehe or any(len(z) != 3 * breite for z in zeilen_rgb):
        raise ValueError("Zeilenlängen passen nicht zur Bildgröße")
    roh = bytearray()
    vorher: bytes | None = None
    for zeile in zeilen_rgb:
        if vorher is None:
            roh.append(1)
            roh += zeile[:3]
            roh += bytes((a - b) & 0xFF for a, b in zip(zeile[3:], zeile))
        else:
            roh.append(2)
            if zeile == vorher:
                roh += bytes(len(zeile))
            else:
                roh += bytes((a - b) & 0xFF for a, b in zip(zeile, vorher))
        vorher = zeile
    ihdr = struct.pack(">IIBBBBB", breite, hoehe, 8, 2, 0, 0, 0)
    return (
        PNG_SIGNATUR
        + _chunk(b"IHDR", ihdr)
        + _chunk(b"IDAT", zlib.compress(bytes(roh), 9))
        + _chunk(b"IEND", b"")
    )


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def png_dekodieren(daten: bytes) -> tuple[int, int, list[bytes]]:
    """Liest ein RGB-PNG mit 8 Bit je Kanal (ohne Zeilensprung) und gibt (Breite, Höhe, Zeilen) zurück."""
    if not daten.startswith(PNG_SIGNATUR):
        raise ValueError("keine PNG-Datei")
    pos = len(PNG_SIGNATUR)
    ihdr: bytes | None = None
    idat = bytearray()
    while pos + 8 <= len(daten):
        laenge, typ = struct.unpack(">I4s", daten[pos : pos + 8])
        inhalt = daten[pos + 8 : pos + 8 + laenge]
        if len(inhalt) != laenge or pos + 12 + laenge > len(daten):
            raise ValueError("PNG abgeschnitten")
        (crc,) = struct.unpack(">I", daten[pos + 8 + laenge : pos + 12 + laenge])
        if zlib.crc32(typ + inhalt) & _M32 != crc:
            raise ValueError(f"Prüfsumme des PNG-Blocks {typ.decode('latin-1')} falsch")
        if typ == b"IHDR":
            ihdr = inhalt
        elif typ == b"IDAT":
            idat += inhalt
        elif typ == b"IEND":
            break
        pos += 12 + laenge
    if ihdr is None or len(ihdr) != 13:
        raise ValueError("PNG ohne gültigen IHDR-Block")
    breite, hoehe, tiefe, farbtyp, kompression, filter_, zeilensprung = struct.unpack(">IIBBBBB", ihdr)
    if (tiefe, farbtyp, kompression, filter_, zeilensprung) != (8, 2, 0, 0, 0):
        raise ValueError("nicht unterstütztes PNG-Format (erwartet RGB, 8 Bit, ohne Zeilensprung)")
    roh = zlib.decompress(bytes(idat))
    schritt = 3 * breite
    if len(roh) != hoehe * (schritt + 1):
        raise ValueError("PNG-Bilddaten haben die falsche Länge")
    zeilen: list[bytes] = []
    vorher = bytes(schritt)
    for y in range(hoehe):
        start = y * (schritt + 1)
        art = roh[start]
        z = bytearray(roh[start + 1 : start + 1 + schritt])
        if art == 0:
            pass
        elif art == 1:
            for k in range(3, schritt):
                z[k] = (z[k] + z[k - 3]) & 0xFF
        elif art == 2:
            z = bytearray((a + b) & 0xFF for a, b in zip(z, vorher))
        elif art == 3:
            for k in range(schritt):
                links = z[k - 3] if k >= 3 else 0
                z[k] = (z[k] + ((links + vorher[k]) >> 1)) & 0xFF
        elif art == 4:
            for k in range(schritt):
                links = z[k - 3] if k >= 3 else 0
                oben_links = vorher[k - 3] if k >= 3 else 0
                z[k] = (z[k] + _paeth(links, vorher[k], oben_links)) & 0xFF
        else:
            raise ValueError(f"unbekannter PNG-Zeilenfilter {art}")
        zeile = bytes(z)
        zeilen.append(zeile)
        vorher = zeile
    return breite, hoehe, zeilen


def kodiere_wert(wert: int) -> bytes:
    """Abschnitt 6.2: wert = R + G·256 (Kartenindex + 1, 0 = kein Ort), B = 0."""
    if not 0 <= wert <= 0xFFFF:
        raise ValueError(f"Wert {wert} außerhalb 0…65535")
    return bytes((wert & 0xFF, wert >> 8, 0))


def dekodiere_pixel(r: int, g: int, b: int) -> int:
    """Kartenindex + 1 aus einem Pixel (0 = kein Ort); B muss 0 sein."""
    if b != 0:
        raise ValueError(f"Blaukanal {b} ≠ 0")
    return r + g * 256


def provinzen_png(ergebnis: Ergebnis) -> bytes:
    tabelle = [kodiere_wert(w) for w in range(len(ergebnis.orte) + 1)]
    meer = tabelle[0]
    zeilen = [b"".join(meer if w == MEER else tabelle[w] for w in zeile) for zeile in ergebnis.zeilen]
    return png_kodieren(ergebnis.raster.breite, ergebnis.raster.hoehe, zeilen)


def werte_aus_png(daten: bytes) -> tuple[int, int, list[list[int]]]:
    """Dekodiert provinzen.png zu Zeilen von Werten (Kartenindex + 1)."""
    breite, hoehe, zeilen = png_dekodieren(daten)
    werte = []
    for zeile in zeilen:
        if any(zeile[2::3]):
            raise ValueError("Blaukanal ≠ 0 (kein Ort-ID-Bild nach Abschnitt 6.2)")
        werte.append([r + g * 256 for r, g in zip(zeile[0::3], zeile[1::3])])
    return breite, hoehe, werte


# ─── definition.csv ─────────────────────────────────────────────────────────


def definition_csv(orte: Sequence[Ort]) -> bytes:
    """Abschnitt 6.3: Semikolon-getrennt, UTF-8, Kopfzeile, LF-Zeilenenden."""
    zeilen = [CSV_KOPF] + [f"{index};{ort.kennung};{ort.name}" for index, ort in enumerate(orte)]
    return ("\n".join(zeilen) + "\n").encode("utf-8")


# ─── Vorschau ───────────────────────────────────────────────────────────────

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
_UMSCHRIFT = {"Ä": "AE", "Ö": "OE", "Ü": "UE", "ß": "SS", "É": "E", "È": "E"}

FARBE_MEER = (164, 196, 222)
FARBE_LEER = (228, 222, 206)
FARBE_UNBEKANNT = (160, 160, 160)
FARBE_STAATSGRENZE = (34, 30, 28)
FARBE_KUESTE = (70, 100, 140)


def _beschriftung(text: str) -> str:
    gross = "".join(_UMSCHRIFT.get(c, c) for c in text.upper())
    return "".join(c if c in _SCHRIFT else " " for c in gross)


def _abdunkeln(farbe: tuple[int, int, int], faktor: float) -> tuple[int, int, int]:
    return tuple(max(0, min(255, int(c * faktor))) for c in farbe)  # type: ignore[return-value]


def vorschau_png(ergebnis: Ergebnis, staaten: dict[str, Staat]) -> bytes:
    """Politische Vorschau: Besitzerfarben, Ortsgrenzen, Staatsgrenzen, Meer, Lagepunkte und Namen."""
    orte = ergebnis.orte
    b, h = ergebnis.raster.breite, ergebnis.raster.hoehe
    besitzer = [""] + [o.besitzer for o in orte]
    fuellung: dict[int, tuple[int, int, int]] = {MEER: FARBE_MEER, 0: FARBE_LEER}
    grenze: dict[int, tuple[int, int, int]] = {MEER: FARBE_KUESTE, 0: FARBE_LEER}
    for index, ort in enumerate(orte):
        staat = staaten.get(ort.besitzer)
        basis = staat.farbe if staat is not None else FARBE_UNBEKANNT
        # Benachbarte Orte desselben Staates leicht unterschiedlich hell.
        faktor = (0.90, 1.0, 1.10)[_hash32(index, 0, 7) % 3]
        fuellung[index + 1] = _abdunkeln(basis, faktor)
        grenze[index + 1] = _abdunkeln(basis, 0.55)

    zeilen = ergebnis.zeilen
    pixel = [[fuellung[w] for w in zeile] for zeile in zeilen]
    for y in range(h):
        zeile = zeilen[y]
        unten = zeilen[y + 1] if y + 1 < h else zeile
        ziel = pixel[y]
        for x in range(b):
            w = zeile[x]
            rechts = zeile[x + 1] if x + 1 < b else w
            u = unten[x]
            if rechts == w and u == w:
                continue
            andere = rechts if rechts != w else u
            if w == MEER or andere == MEER:
                ziel[x] = FARBE_KUESTE
            elif w == 0 or andere == 0:
                ziel[x] = grenze[w] if w > 0 else grenze[andere]
            elif besitzer[w] != besitzer[andere]:
                ziel[x] = FARBE_STAATSGRENZE
            else:
                ziel[x] = grenze[w]

    def setze(x: int, y: int, farbe: tuple[int, int, int]) -> None:
        if 0 <= x < b and 0 <= y < h:
            pixel[y][x] = farbe

    for ort in orte:
        p = ergebnis.raster.pixel_von(ort.laenge, ort.breite)
        if p is None:
            continue
        px, py = p
        for dy in range(-2, 3):
            for dx in range(-2, 3):
                rand = abs(dx) == 2 or abs(dy) == 2
                setze(px + dx, py + dy, (0, 0, 0) if rand else (255, 255, 255))
        text = _beschriftung(ort.name)
        tx, ty = px + 5, py - 3
        if tx + 6 * len(text) > b:  # am rechten Rand links vom Punkt beschriften
            tx = px - 4 - 6 * len(text)
            if tx < 0:
                tx = max(0, b - 6 * len(text))
        # Erst die Kontur (dunkel), dann die Schrift (hell).
        for lauf in (0, 1):
            for n, zeichen in enumerate(text):
                glyphe = _SCHRIFT[zeichen]
                for gy, bits in enumerate(glyphe):
                    for gx in range(5):
                        if bits & (0x10 >> gx):
                            x, y = tx + 6 * n + gx, ty + gy
                            if lauf == 0:
                                for ox, oy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                                    setze(x + ox, y + oy, (20, 20, 20))
                            else:
                                setze(x, y, (255, 255, 255))
    return png_kodieren(b, h, [bytes(c for farbe in zeile for c in farbe) for zeile in pixel])


# ─── Kommandozeile ──────────────────────────────────────────────────────────


def _lade(daten: Path) -> tuple[Karte, list[Ort], list[str]]:
    meldungen = Meldungen()
    karte = lade_karte(daten / "karte" / "karte.chron", "karte/karte.chron", meldungen)
    orte = lade_orte(daten / "history" / "orte", daten, meldungen)
    if meldungen.fehler or karte is None:
        raise DatenFehler(meldungen.fehler or ["karte/karte.chron: unbekannter Fehler"])
    return karte, orte, meldungen.warnungen


def _schreibe_wenn_anders(pfad: Path, inhalt: bytes) -> bool:
    try:
        if pfad.read_bytes() == inhalt:
            return False
    except OSError:
        pass
    pfad.write_bytes(inhalt)
    return True


def pruefe_ausgabe(ausgabe: Path, ergebnis: Ergebnis) -> list[str]:
    """Vergleicht die Dateien im Ausgabeordner mit dem Ergebnis; Liste der Abweichungen."""
    abweichungen = []
    csv_pfad = ausgabe / "definition.csv"
    try:
        if csv_pfad.read_bytes() != definition_csv(ergebnis.orte):
            abweichungen.append(f"{csv_pfad}: Inhalt veraltet")
    except OSError:
        abweichungen.append(f"{csv_pfad}: Datei fehlt")
    png_pfad = ausgabe / "provinzen.png"
    try:
        daten = png_pfad.read_bytes()
    except OSError:
        abweichungen.append(f"{png_pfad}: Datei fehlt")
    else:
        # Pixel vergleichen, nicht Bytes: andere zlib-Versionen komprimieren anders.
        try:
            breite, hoehe, werte = werte_aus_png(daten)
        except (ValueError, zlib.error) as fehler:
            abweichungen.append(f"{png_pfad}: nicht lesbar ({fehler})")
        else:
            erwartet = [[0 if w == MEER else w for w in zeile] for zeile in ergebnis.zeilen]
            if (breite, hoehe) != (ergebnis.raster.breite, ergebnis.raster.hoehe):
                abweichungen.append(
                    f"{png_pfad}: Bildgröße {breite}×{hoehe}, erwartet "
                    f"{ergebnis.raster.breite}×{ergebnis.raster.hoehe}"
                )
            elif werte != erwartet:
                anders = sum(a != e for zw, ze in zip(werte, erwartet) for a, e in zip(zw, ze))
                abweichungen.append(f"{png_pfad}: {anders} Pixel weichen ab")
    return abweichungen


class _Hilfeformat(argparse.HelpFormatter):
    def add_usage(self, usage, actions, groups, prefix=None):  # type: ignore[no-untyped-def]
        super().add_usage(usage, actions, groups, "Aufruf: " if prefix is None else prefix)


class _Argumente(argparse.ArgumentParser):
    """ArgumentParser mit deutschen Fehlermeldungen."""

    _UEBERSETZUNG = (
        ("unrecognized arguments:", "unbekannte Argumente:"),
        ("expected one argument", "erwartet einen Wert"),
        ("ignored explicit argument", "Wert nicht erlaubt:"),
        ("ambiguous option:", "mehrdeutige Option:"),
        ("could match", "passt zu"),
        ("argument ", "Option "),
    )

    def error(self, message: str) -> NoReturn:
        for englisch, deutsch in self._UEBERSETZUNG:
            message = message.replace(englisch, deutsch)
        self.print_usage(sys.stderr)
        self.exit(2, f"Fehler: {message}\n")


def _argumente() -> argparse.ArgumentParser:
    parser = _Argumente(
        prog="kartengenerator.py",
        description="Erzeugt provinzen.png und definition.csv aus karte.chron und den Orten "
        "(docs/technik/chronik-skript.md, Abschnitt 6).",
        epilog="Exitcodes: 0 = Erfolg bzw. aktuell, 1 = --pruefen: nicht aktuell, 2 = Fehler.",
        formatter_class=_Hilfeformat,
        add_help=False,
        allow_abbrev=False,
    )
    optionen = parser.add_argument_group("Optionen")
    optionen.add_argument("-h", "--hilfe", action="help", help="diese Hilfe anzeigen und beenden")
    optionen.add_argument(
        "--daten", type=Path, default=REPO / "data", metavar="DIR", help="Datenordner (Standard: <repo>/data)"
    )
    optionen.add_argument(
        "--ausgabe", type=Path, metavar="DIR", help="Ausgabeordner (Standard: <daten>/karte)"
    )
    optionen.add_argument(
        "--vorschau", type=Path, metavar="DATEI", help="zusätzlich farbige politische Vorschau (PNG)"
    )
    optionen.add_argument(
        "--pruefen",
        action="store_true",
        help="nichts schreiben, nur prüfen, ob die Dateien im Ausgabeordner aktuell sind "
        "(Exitcode 1, wenn nicht)",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = _argumente()
    args = parser.parse_args(argv)
    daten: Path = args.daten
    ausgabe: Path = args.ausgabe if args.ausgabe is not None else daten / "karte"

    start = time.perf_counter()  # nur für die Laufzeitanzeige
    try:
        karte, orte, warnungen = _lade(daten)
    except DatenFehler as fehler:
        for text in fehler.meldungen:
            print(f"Fehler: {text}", file=sys.stderr)
        print(f"Abbruch: {len(fehler.meldungen)} Fehler in den Daten.", file=sys.stderr)
        return 2
    for text in warnungen:
        print(f"Warnung: {text}", file=sys.stderr)

    ergebnis = erzeuge(karte, orte)
    for text in ergebnis.warnungen:
        print(f"Warnung: {text}", file=sys.stderr)

    land = sum(karte.hoehe - min(e, karte.hoehe) for e in ergebnis.raster.erstes_land)
    belegt = sum(ergebnis.pixel)
    print(
        f"{len(orte)} Orte, Bild {karte.breite}×{karte.hoehe}, "
        f"Land {100.0 * land / (karte.breite * karte.hoehe):.1f} %, "
        f"davon Orten zugeordnet {100.0 * belegt / max(land, 1):.1f} %, "
        f"umverteilte Teilflächen: {ergebnis.umverteilt}"
    )

    rueckgabe = 0
    if args.pruefen:
        abweichungen = pruefe_ausgabe(ausgabe, ergebnis)
        for text in abweichungen:
            print(f"Nicht aktuell: {text}", file=sys.stderr)
        if abweichungen:
            print(
                "Karte nicht aktuell – bitte 'python3 tools/kartengenerator/kartengenerator.py' ausführen.",
                file=sys.stderr,
            )
            rueckgabe = 1
        else:
            print(f"Karte aktuell: {ausgabe / 'provinzen.png'}, {ausgabe / 'definition.csv'}")
    else:
        try:
            ausgabe.mkdir(parents=True, exist_ok=True)
            for name, inhalt in (
                ("provinzen.png", provinzen_png(ergebnis)),
                ("definition.csv", definition_csv(orte)),
            ):
                pfad = ausgabe / name
                status = "geschrieben" if _schreibe_wenn_anders(pfad, inhalt) else "unverändert"
                print(f"{status}: {pfad} ({len(inhalt)} Bytes)")
        except OSError as fehler:
            print(f"Fehler: Ausgabe kann nicht geschrieben werden: {fehler}", file=sys.stderr)
            return 2

    if args.vorschau is not None:
        meldungen = Meldungen()
        staaten = lade_staaten(daten / "history" / "staaten", daten, meldungen)
        for text in meldungen.warnungen:
            print(f"Warnung: {text}", file=sys.stderr)
        try:
            args.vorschau.parent.mkdir(parents=True, exist_ok=True)
            args.vorschau.write_bytes(vorschau_png(ergebnis, staaten))
        except OSError as fehler:
            print(f"Fehler: Vorschau kann nicht geschrieben werden: {fehler}", file=sys.stderr)
            return 2
        print(f"Vorschau: {args.vorschau}")

    print(f"Laufzeit: {time.perf_counter() - start:.1f} s")
    return rueckgabe


if __name__ == "__main__":
    sys.exit(main())
