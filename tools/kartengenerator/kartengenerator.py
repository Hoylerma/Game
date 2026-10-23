#!/usr/bin/env python3
"""Kartengenerator für EINIGKEIT (Fassung M3, Nachbarn seit M10).

Erzeugt aus data/karte/karte.chron, der Geometrie in data/karte/geometrie/*.geojson und den
Orten in data/history/orte/*.chron die Dateien provinzen.png (Ort-ID-Bild mit Flächenart),
fluesse.png (Flussebene), definition.csv (Kartenindex, Beschriftungspunkt, Fläche) und
nachbarn.csv (benachbarte Orte mit Grenzlänge und gekreuztem Fluss, seit M10).
Spezifikation: docs/technik/chronik-skript.md, Abschnitt 6.

Aufruf (siehe README.md im selben Ordner):

    python3 tools/kartengenerator/kartengenerator.py [--daten DIR] [--ausgabe DIR] [--pruefen]
                                                     [--vorschau DATEI [--vorschau-breite N] …]

Als Bibliothek (ohne Dateien zu schreiben):

    import kartengenerator as kg
    ergebnis = kg.erzeuge_karte(Path("data"))            # lädt alles, rechnet, schreibt nichts
    grundlage = kg.lade_grundlage(Path("data"))          # Geometrie einmal rastern …
    ergebnis = kg.erzeuge(grundlage, orte)               # … und mit eigenen Orten rechnen

Abhängigkeiten: Python ab 3.10 und numpy. Die Ausgabe ist deterministisch (siehe karte_raster.py).

Exitcodes: 0 = Erfolg bzw. Dateien aktuell, 1 = --pruefen: Dateien nicht aktuell,
2 = Fehler in Daten oder Aufruf.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, NoReturn

try:
    import numpy as np
except ImportError:  # pragma: no cover - nur ohne numpy
    print(
        "Fehler: Der Kartengenerator braucht numpy (python3 -m pip install numpy).",
        file=sys.stderr,
    )
    raise SystemExit(2) from None

_HIER = Path(__file__).resolve().parent
if str(_HIER) not in sys.path:  # Aufruf als Modul von außerhalb des Werkzeugordners
    sys.path.insert(0, str(_HIER))

import chron  # noqa: E402
import karte_png  # noqa: E402
import karte_raster as kr  # noqa: E402
from karte_raster import AUSLAND, BINNENGEWAESSER, KERNLAND, MEER, Karte  # noqa: E402, F401

# ─── Konstanten ─────────────────────────────────────────────────────────────

#: Wurzel des Repositorys (tools/kartengenerator/ → zwei Ebenen höher).
REPO = _HIER.parents[1]

#: Höchste Bildgröße je Achse (Texturgrenze, Abschnitt 6.1).
MAX_BILDKANTE = 4096
#: Höchste erlaubte Abweichung des Seitenverhältnisses (Abschnitt 6.1).
MAX_SEITENABWEICHUNG = 0.01
#: Höchstzahl der Orte (R + G·256 = Kartenindex + 1 ≤ 65535).
MAX_ORTE = 65535
#: Warnschwelle: weniger Pixel als dieser Anteil des Medians aller Orte.
WENIGE_PIXEL_ANTEIL = 0.1
#: So viele Teilflächen „Kernraum-Land ohne Ort“ werden einzeln gemeldet.
OHNE_ORT_MELDUNGEN = 10

CSV_KOPF = "kartenindex;ort;name;x;y;pixel;flaeche_km2"
#: Kopf der nachbarn.csv (seit M10, Abschnitt 6.6).
NACHBARN_KOPF = "ort_a;ort_b;grenze;fluss"
#: Kantenlänge der Zellen (Grad), in die die Flussabschnitte für die Kreuzungssuche einsortiert werden.
FLUSS_ZELLE_GRAD = 0.25
GEOMETRIE_DATEIEN = ("land", "seen", "fluesse", "kernraum")
#: Felder, die einen Ort zum „Ort mit Daten“ machen (Abschnitt 3.6).
SIMULATIONSFELDER = ("besitzer", "bevoelkerung", "konfession", "verwuestung")

FLAECHENART_NAME = {
    MEER: "im Meer",
    BINNENGEWAESSER: "in einem Binnengewässer",
    AUSLAND: "im Ausland (außerhalb des Kernraums)",
}


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
class Ort:
    """Ein Ort, wie ihn der Generator braucht. ``besitzer``, ``kreis`` und ``hat_daten`` nur für die Vorschau."""

    kennung: str
    name: str
    laenge: float
    breite: float
    besitzer: str = ""
    kreis: str = ""
    hat_daten: bool = False
    datei: str = ""
    zeile: int = 0

    @property
    def wo(self) -> str:
        """Fundstelle für Meldungen."""
        return f"{self.datei}:{self.zeile}: ort {self.kennung}" if self.datei else f"ort {self.kennung}"


@dataclass(frozen=True)
class Staat:
    tag: str
    name: str
    farbe: tuple[int, int, int]


@dataclass
class Geometrie:
    """Inhalt von karte/geometrie/*.geojson; Ringe und Linien als (n, 2)-Felder (Länge, Breite)."""

    land: list[list[np.ndarray]]
    seen: list[list[np.ndarray]]
    kernraum: list[list[np.ndarray]]
    fluesse: list[tuple[int, np.ndarray]]


# ─── Laden: Chronik-Dateien ─────────────────────────────────────────────────


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
    node: chron.ScriptNode, key: str, datei: str, was: str, meldungen: Meldungen, *, ganzzahl: bool = False
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
    "bezugsbreite",
    "max_radius_km",
)


def seitenverhaeltnis_abweichung(karte: Karte) -> float:
    """Relative Abweichung von bild_breite/bild_hoehe zum Sollwert aus Abschnitt 6.1."""
    soll = (
        (karte.laenge_max - karte.laenge_min) * kr.cos_grad(karte.bezugsbreite) / (karte.breite_max - karte.breite_min)
    )
    ist = karte.breite / karte.hoehe
    return abs(ist - soll) / soll


def lade_karte(pfad: Path, name: str, meldungen: Meldungen) -> Karte | None:
    """Liest karte.chron (Abschnitt 6.1). Fehler landen in meldungen."""
    if not pfad.is_file():
        meldungen.fehler_bei(name, 0, "Datei fehlt")
        return None
    anzahl_fehler = len(meldungen.fehler)
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
            meldungen.fehler_bei(name, node.line, f"zweiter Block 'karte' (der erste steht in Zeile {karte_node.line})")
            continue
        karte_node = node
    if karte_node is None:
        if not ungueltig:
            meldungen.fehler_bei(name, 0, "Block 'karte { … }' fehlt")
        return None

    for kind in karte_node.children:
        if kind.key == "kueste":
            meldungen.warnung_bei(
                name,
                kind.line,
                "'kueste' entfällt seit M3 und wird ignoriert (Land und Meer kommen aus geometrie/*.geojson)",
            )
        elif kind.key not in KARTE_FELDER:
            was = f"unbekanntes Feld '{kind.key}'" if kind.key else "unerwarteter Eintrag"
            meldungen.warnung_bei(name, kind.line, f"{was} in karte wird ignoriert")

    def zeile_von(key: str) -> int:
        kind = karte_node.find(key)
        return kind.line if kind is not None else karte_node.line

    def ganz(key: str) -> int | None:
        wert = _zahl(karte_node, key, name, "karte", meldungen, ganzzahl=True)
        if wert is not None and not 1 <= wert <= MAX_BILDKANTE:
            meldungen.fehler_bei(
                name, zeile_von(key), f"'{key}' muss zwischen 1 und {MAX_BILDKANTE} liegen (Texturgrenze)"
            )
            return None
        return int(wert) if wert is not None else None

    breite = ganz("bild_breite")
    hoehe = ganz("bild_hoehe")
    werte = {key: _zahl(karte_node, key, name, "karte", meldungen) for key in KARTE_FELDER[2:]}
    lmin, lmax = werte["laenge_min"], werte["laenge_max"]
    bmin, bmax = werte["breite_min"], werte["breite_max"]
    bezug, radius = werte["bezugsbreite"], werte["max_radius_km"]
    if lmin is not None and lmax is not None and not (-180.0 <= lmin < lmax <= 180.0):
        meldungen.fehler_bei(name, zeile_von("laenge_max"), "erwartet −180 ≤ laenge_min < laenge_max ≤ 180")
    if bmin is not None and bmax is not None and not (-90.0 <= bmin < bmax <= 90.0):
        meldungen.fehler_bei(name, zeile_von("breite_max"), "erwartet −90 ≤ breite_min < breite_max ≤ 90")
    if bezug is not None and not -90.0 < bezug < 90.0:
        meldungen.fehler_bei(name, zeile_von("bezugsbreite"), "'bezugsbreite' muss zwischen −90 und 90 liegen")
    if radius is not None and not radius > 0.0:
        meldungen.fehler_bei(name, zeile_von("max_radius_km"), "'max_radius_km' muss größer als 0 sein")
    if len(meldungen.fehler) > anzahl_fehler or None in (breite, hoehe, lmin, lmax, bmin, bmax, bezug, radius):
        return None
    karte = Karte(
        breite=breite,  # type: ignore[arg-type]
        hoehe=hoehe,  # type: ignore[arg-type]
        laenge_min=lmin,  # type: ignore[arg-type]
        laenge_max=lmax,  # type: ignore[arg-type]
        breite_min=bmin,  # type: ignore[arg-type]
        breite_max=bmax,  # type: ignore[arg-type]
        bezugsbreite=bezug,  # type: ignore[arg-type]
        max_radius_km=radius,  # type: ignore[arg-type]
    )
    abweichung = seitenverhaeltnis_abweichung(karte)
    if abweichung > MAX_SEITENABWEICHUNG:
        meldungen.fehler_bei(
            name,
            zeile_von("bild_hoehe"),
            f"Seitenverhältnis bild_breite/bild_hoehe weicht um {100.0 * abweichung:.2f} % vom Sollwert "
            f"(laenge_max − laenge_min) × cos(bezugsbreite) / (breite_max − breite_min) ab "
            f"(erlaubt: {100.0 * MAX_SEITENABWEICHUNG:.0f} %)",
        )
        return None
    return karte


def _ist_kennung(text: str) -> bool:
    return bool(text) and all("a" <= c <= "z" or "0" <= c <= "9" or c == "_" for c in text)


def _ist_datum(text: str) -> bool:
    return text[:1].isdigit()


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
                meldungen.warnung_bei(name, node.line, f"{was} wird ignoriert (erwartet: ort <kennung> {{ … }})")
                continue
            if not node.is_block or not node.value:
                meldungen.fehler_bei(name, node.line, "ungültige Definition, erwartet: ort <kennung> { … }")
                continue
            ort = _lese_ort(node, name, meldungen)
            if ort is None:
                continue
            if ort.kennung in gesehen:
                meldungen.fehler_bei(
                    name, node.line, f"doppelte Kennung '{ort.kennung}' (bereits definiert in {gesehen[ort.kennung]})"
                )
                continue
            gesehen[ort.kennung] = f"{name}:{node.line}"
            orte.append(ort)
    if len(orte) > MAX_ORTE:
        meldungen.fehler_bei(_datei_name(ordner, daten), 0, f"zu viele Orte ({len(orte)}, höchstens {MAX_ORTE})")
    return orte


def _lese_ort(node: chron.ScriptNode, datei: str, meldungen: Meldungen) -> Ort | None:
    kennung = node.value
    was = f"ort {kennung}"
    anzahl = len(meldungen.fehler)
    if not _ist_kennung(kennung):
        meldungen.fehler_bei(
            datei, node.line, f"ungültige Ortskennung '{kennung}' (nur Kleinbuchstaben a–z, Ziffern und '_')"
        )
    name_node = _einmal(node, "name", datei, meldungen)
    name = ""
    if name_node is None:
        meldungen.fehler_bei(datei, node.line, f"Pflichtfeld 'name' fehlt in {was}")
    elif name_node.is_block or name_node.op != "=":
        meldungen.fehler_bei(datei, name_node.line, "'name' erwartet einen Text, z. B. name = \"Berlin-Cölln\"")
    else:
        name = name_node.value
        if ";" in name or "\n" in name or "\r" in name:
            meldungen.fehler_bei(
                datei, name_node.line, "'name' darf kein ';' und keinen Zeilenumbruch enthalten (definition.csv)"
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
                "'lage' erwartet genau zwei Zahlen: Längengrad Ost und Breitengrad Nord, z. B. { 13.40 52.52 }",
            )
        elif not (-180.0 <= zahlen[0] <= 180.0 and -90.0 <= zahlen[1] <= 90.0):
            meldungen.fehler_bei(
                datei, lage.line, "'lage' außerhalb des gültigen Bereichs (Länge −180…180, Breite −90…90)"
            )
        else:
            laenge, breite = zahlen
    kreis = ""
    kreis_node = _einmal(node, "kreis", datei, meldungen)
    if kreis_node is not None:
        if kreis_node.is_block or kreis_node.quoted or not _ist_kennung(kreis_node.value):
            meldungen.fehler_bei(datei, kreis_node.line, "'kreis' erwartet eine Kreiskennung, z. B. kreis = kr_berlin")
        else:
            kreis = kreis_node.value
    # Besitzer für die Vorschau: Grundangabe, dann datierte Blöcke (Abschnitt 3.8) in zeitlicher
    # Reihenfolge – die Vorschau zeigt den jüngsten Stand. "JJJJ.MM.TT" sortiert als Text wie als Datum.
    besitzer = node.get_string("besitzer") or ""
    hat_daten = any(node.find(f) is not None for f in SIMULATIONSFELDER)
    for block in sorted((k for k in node.children if k.is_block and _ist_datum(k.key)), key=lambda k: k.key):
        besitzer = block.get_string("besitzer") or besitzer
        hat_daten = hat_daten or any(block.find(f) is not None for f in SIMULATIONSFELDER)
    if len(meldungen.fehler) > anzahl:
        return None
    return Ort(kennung, name, laenge, breite, besitzer, kreis, hat_daten, datei, node.line)


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


# ─── Laden: GeoJSON ─────────────────────────────────────────────────────────


def _punkte(koordinaten: Any, mindestens: int) -> np.ndarray:
    """Positionsliste als (n, 2)-Feld (Länge, Breite); wirft ValueError bei ungültigen Werten."""
    if not isinstance(koordinaten, list) or len(koordinaten) < mindestens:
        raise ValueError(f"erwartet eine Liste mit mindestens {mindestens} Positionen")
    werte = []
    for position in koordinaten:
        if (
            not isinstance(position, list)
            or len(position) < 2
            or not all(isinstance(z, (int, float)) and not isinstance(z, bool) for z in position[:2])
        ):
            raise ValueError(f"ungültige Position {json.dumps(position)[:40]}")
        laenge, breite = float(position[0]), float(position[1])
        if not (-180.0 <= laenge <= 180.0 and -90.0 <= breite <= 90.0):
            raise ValueError(f"Position außerhalb des gültigen Bereichs: {laenge} {breite}")
        werte.append((laenge, breite))
    return np.array(werte, dtype=np.float64)


def _lade_geojson(pfad: Path, name: str, meldungen: Meldungen) -> list[dict[str, Any]] | None:
    if not pfad.is_file():
        meldungen.fehler_bei(name, 0, "Datei fehlt (erzeugt von tools/geodaten/geodaten.py geometrie)")
        return None
    try:
        daten = json.loads(pfad.read_bytes().decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as fehler:
        meldungen.fehler_bei(name, getattr(fehler, "lineno", 0), f"kein gültiges GeoJSON ({fehler})")
        return None
    if not isinstance(daten, dict) or daten.get("type") != "FeatureCollection":
        meldungen.fehler_bei(name, 0, "erwartet eine FeatureCollection")
        return None
    features = daten.get("features")
    if not isinstance(features, list):
        meldungen.fehler_bei(name, 0, "'features' fehlt oder ist keine Liste")
        return None
    return features


def lade_flaechen(pfad: Path, name: str, meldungen: Meldungen) -> list[list[np.ndarray]]:
    """Polygone (Polygon/MultiPolygon) einer FeatureCollection; je Polygon die Ringe."""
    features = _lade_geojson(pfad, name, meldungen)
    polygone: list[list[np.ndarray]] = []
    for nr, feature in enumerate(features or []):
        geometrie = feature.get("geometry") if isinstance(feature, dict) else None
        typ = geometrie.get("type") if isinstance(geometrie, dict) else None
        try:
            if typ == "Polygon":
                teile = [geometrie["coordinates"]]
            elif typ == "MultiPolygon":
                teile = geometrie["coordinates"]
            else:
                raise ValueError(f"Geometrietyp {typ!r} nicht erlaubt (erwartet Polygon oder MultiPolygon)")
            if not isinstance(teile, list):
                raise ValueError("'coordinates' ist keine Liste")
            for polygon in teile:
                if not isinstance(polygon, list) or not polygon:
                    raise ValueError("Polygon ohne Ringe")
                polygone.append([_punkte(ring, 4) for ring in polygon])
        except (ValueError, KeyError, TypeError) as fehler:
            meldungen.fehler_bei(name, 0, f"Feature {nr + 1}: {fehler}")
    return polygone


def lade_linien(pfad: Path, name: str, meldungen: Meldungen) -> list[tuple[int, np.ndarray]]:
    """Flüsse (LineString/MultiLineString) mit Eigenschaft rang 1…6."""
    features = _lade_geojson(pfad, name, meldungen)
    linien: list[tuple[int, np.ndarray]] = []
    for nr, feature in enumerate(features or []):
        geometrie = feature.get("geometry") if isinstance(feature, dict) else None
        typ = geometrie.get("type") if isinstance(geometrie, dict) else None
        eigenschaften = feature.get("properties") if isinstance(feature, dict) else None
        rang = eigenschaften.get("rang") if isinstance(eigenschaften, dict) else None
        if not (isinstance(rang, int) and not isinstance(rang, bool) and rang in kr.FLUSS_BREITE_PX):
            meldungen.warnung_bei(name, 0, f"Feature {nr + 1}: 'rang' fehlt oder ist ungültig ({rang!r}), nehme 6")
            rang = 6
        try:
            if typ == "LineString":
                teile = [geometrie["coordinates"]]
            elif typ == "MultiLineString":
                teile = geometrie["coordinates"]
            else:
                raise ValueError(f"Geometrietyp {typ!r} nicht erlaubt (erwartet LineString oder MultiLineString)")
            if not isinstance(teile, list):
                raise ValueError("'coordinates' ist keine Liste")
            for linie in teile:
                linien.append((rang, _punkte(linie, 2)))
        except (ValueError, KeyError, TypeError) as fehler:
            meldungen.fehler_bei(name, 0, f"Feature {nr + 1}: {fehler}")
    return linien


def lade_geometrie(ordner: Path, daten: Path, meldungen: Meldungen) -> Geometrie:
    def pfad(n: str) -> tuple[Path, str]:
        p = ordner / f"{n}.geojson"
        return p, _datei_name(p, daten)

    return Geometrie(
        land=lade_flaechen(*pfad("land"), meldungen),
        seen=lade_flaechen(*pfad("seen"), meldungen),
        kernraum=lade_flaechen(*pfad("kernraum"), meldungen),
        fluesse=lade_linien(*pfad("fluesse"), meldungen),
    )


# ─── Grundlage und Ergebnis ─────────────────────────────────────────────────


@dataclass
class Grundlage:
    """Gerasterte Geometrie eines Kartenausschnitts; unabhängig von den Orten, mehrfach verwendbar."""

    karte: Karte
    raster: kr.Raster
    fluesse: np.ndarray | None  # uint8 (H, W) oder None, wenn nicht berechnet
    warnungen: list[str] = field(default_factory=list)
    flusslinien: list[tuple[int, np.ndarray]] = field(default_factory=list)  # (rang, Länge/Breite), seit M10

    @property
    def art(self) -> np.ndarray:
        """Flächenart je Pixel (KERNLAND, MEER, BINNENGEWAESSER, AUSLAND)."""
        return self.raster.art


@dataclass(frozen=True)
class Flaeche:
    """Eine Zeile der definition.csv (Abschnitt 6.5) plus Lage des Beschriftungspunkts."""

    kartenindex: int
    ort: str
    name: str
    x: int  # Beschriftungspunkt (Pixel), −1 ohne Fläche
    y: int
    pixel: int
    flaeche_km2: int
    laenge: float  # Mitte des Beschriftungspixels (Länge, Breite); bei −1 die Lage des Ortes
    breite: float


@dataclass(frozen=True)
class Teilflaeche:
    """Zusammenhängendes Kernraum-Land ohne Ort."""

    pixel: int
    flaeche_km2: float  # ungefähr (Pixelfläche der ersten Zeile)
    x: int  # erstes Pixel in Zeilenreihenfolge
    y: int
    laenge: float
    breite: float


@dataclass
class Ergebnis:
    karte: Karte
    orte: list[Ort]
    werte: np.ndarray  # int32 (H, W): Kartenindex + 1, 0 = kein Ort
    art: np.ndarray  # uint8 (H, W): Flächenart
    fluesse: np.ndarray | None  # uint8 (H, W)
    flaechen: list[Flaeche]
    ohne_ort: list[Teilflaeche]
    warnungen: list[str]
    bereinigung: kr.Bereinigung
    zeiten: dict[str, float] = field(default_factory=dict)
    flusslinien: list[tuple[int, np.ndarray]] = field(default_factory=list)  # aus der Grundlage, seit M10

    def provinzen(self) -> np.ndarray:
        """provinzen.png als (H, W, 3)-Feld: R + G·256 = Kartenindex + 1, B = Flächenart (Abschnitt 6.3)."""
        bild = np.empty(self.werte.shape + (3,), dtype=np.uint8)
        bild[..., 0] = self.werte & 0xFF
        bild[..., 1] = self.werte >> 8
        bild[..., 2] = self.art
        return bild

    def definition_csv(self) -> bytes:
        return definition_csv(self.flaechen)

    @property
    def flaeche_nach_ort(self) -> dict[str, Flaeche]:
        return {f.ort: f for f in self.flaechen}

    def ort_bei(self, laenge: float, breite: float) -> int | None:
        """Kartenindex des Ortes, dem das Pixel an dieser Stelle gehört, oder None (kein Ort, außerhalb)."""
        x, y = self.karte.nach_pixel(np.array([laenge]), np.array([breite]))
        px, py = int(np.floor(x[0])), int(np.floor(y[0]))
        if not (0 <= px < self.karte.breite and 0 <= py < self.karte.hoehe):
            return None
        wert = int(self.werte[py, px])
        return wert - 1 if wert > 0 else None

    def flussuebergaenge(self) -> dict[tuple[int, int], int]:
        """Je Paar benachbarter Orte der größte Fluss, den die Verbindungslinie ihrer Lagen kreuzt (seit M10).

        Wert: der kleinste Rang der gekreuzten Flusslinien (1 = Rhein, Elbe, Donau …), 0 ohne Fluss.
        """
        return flussuebergaenge(self.orte, list(self.nachbarn()), self.flusslinien)

    def nachbarn_csv(self) -> bytes:
        return nachbarn_csv(self.orte, self.nachbarn(), self.flussuebergaenge())

    def nachbarn(self) -> dict[tuple[int, int], int]:
        """Benachbarte Orte: (Kartenindex a, Kartenindex b) mit a < b → Länge der gemeinsamen Grenze in Pixelkanten.

        Zählt waagrechte und senkrechte Pixelkanten (4er-Nachbarschaft); Wasser, Ausland und Land ohne
        Ort trennen. Nützlich etwa zum Bilden von Kreisen aus benachbarten Orten.
        """
        w = self.werte
        paare = []
        for a, b in ((w[:, :-1], w[:, 1:]), (w[:-1], w[1:])):
            grenze = (a != b) & (a > 0) & (b > 0)
            lo = np.minimum(a[grenze], b[grenze]).astype(np.int64) - 1
            hi = np.maximum(a[grenze], b[grenze]).astype(np.int64) - 1
            paare.append(lo * (len(self.orte) + 1) + hi)
        schluessel, zahl = np.unique(np.concatenate(paare), return_counts=True)
        n = len(self.orte) + 1
        return {(int(s // n), int(s % n)): int(z) for s, z in zip(schluessel, zahl)}


def grundlage_aus(karte: Karte, geometrie: Geometrie, *, fluesse: bool = True) -> Grundlage:
    """Rastert die Geometrie (Flächenarten, verschobene Pixelmitten, optional Flussebene)."""
    raster = kr.Raster.erzeuge(karte, geometrie.land, geometrie.seen, geometrie.kernraum)
    raster.verschiebe()
    bild = kr.zeichne_fluesse(geometrie.fluesse, karte) if fluesse else None
    return Grundlage(karte, raster, bild, flusslinien=list(geometrie.fluesse))


def lade_grundlage(daten: Path | None = None, *, fluesse: bool = True) -> Grundlage:
    """Lädt karte.chron und die Geometrie und rastert sie. Wirft DatenFehler."""
    daten = REPO / "data" if daten is None else Path(daten)
    meldungen = Meldungen()
    karte = lade_karte(daten / "karte" / "karte.chron", "karte/karte.chron", meldungen)
    geometrie = lade_geometrie(daten / "karte" / "geometrie", daten, meldungen)
    if meldungen.fehler or karte is None:
        raise DatenFehler(meldungen.fehler or ["karte/karte.chron: unbekannter Fehler"])
    grundlage = grundlage_aus(karte, geometrie, fluesse=fluesse)
    grundlage.warnungen = meldungen.warnungen
    return grundlage


def lade_orte_daten(daten: Path | None = None) -> tuple[list[Ort], list[str]]:
    """Lädt alle Orte aus <daten>/history/orte. Wirft DatenFehler; gibt (Orte, Warnungen) zurück."""
    daten = REPO / "data" if daten is None else Path(daten)
    meldungen = Meldungen()
    orte = lade_orte(daten / "history" / "orte", daten, meldungen)
    if meldungen.fehler:
        raise DatenFehler(meldungen.fehler)
    return orte, meldungen.warnungen


def _zahl_text(wert: float) -> str:
    return f"{wert:.2f}"


def erzeuge(grundlage: Grundlage, orte: Sequence[Ort]) -> Ergebnis:
    """Berechnet die Karte für diese Orte (Kartenindex = Position in ``orte``), ohne zu schreiben."""
    zeiten: dict[str, float] = {}
    uhr = time.perf_counter()
    karte = grundlage.karte
    raster = grundlage.raster
    n = len(orte)
    if n > MAX_ORTE:
        raise DatenFehler([f"zu viele Orte ({n}, höchstens {MAX_ORTE})"])
    fehler: list[str] = []
    kennungen: set[str] = set()
    for ort in orte:  # für Orte, die nicht aus Chronik-Dateien kommen (Python-API)
        if not _ist_kennung(ort.kennung) or ort.kennung in kennungen:
            fehler.append(f"{ort.wo}: ungültige oder doppelte Kennung")
        if ";" in ort.name or "\n" in ort.name or "\r" in ort.name:
            fehler.append(f"{ort.wo}: 'name' darf kein ';' und keinen Zeilenumbruch enthalten (definition.csv)")
        if not (-180.0 <= ort.laenge <= 180.0 and -90.0 <= ort.breite <= 90.0):
            fehler.append(f"{ort.wo}: 'lage' außerhalb des gültigen Bereichs")
        kennungen.add(ort.kennung)
    if fehler:
        raise DatenFehler(fehler)
    laengen = np.array([o.laenge for o in orte], dtype=np.float64)
    breiten = np.array([o.breite for o in orte], dtype=np.float64)
    lage = kr.lagen(raster, laengen, breiten)
    werte = kr.zuordnen(raster, laengen, breiten, lage)
    zeiten["zuordnung"] = time.perf_counter() - uhr

    # Lagepixel auf Kernraum-Land gehören ihrem Ort (bei mehreren Orten im selben Pixel: dem ersten).
    warnungen: list[str] = []
    lagepixel: dict[int, tuple[int, int]] = {}
    besetzt: dict[tuple[int, int], int] = {}
    for k, ort in enumerate(orte):
        if not lage.innen[k]:
            warnungen.append(
                f"{ort.wo}: Lage {_zahl_text(ort.laenge)} {_zahl_text(ort.breite)} liegt außerhalb des Kartenausschnitts"
            )
            continue
        x, y = int(lage.px[k]), int(lage.py[k])
        art = int(lage.art[k])
        if art != KERNLAND:
            warnungen.append(
                f"{ort.wo}: Lage {_zahl_text(ort.laenge)} {_zahl_text(ort.breite)} liegt {FLAECHENART_NAME[art]}, "
                "nicht auf Kernraum-Land"
            )
            continue
        if (x, y) in besetzt:
            erster = orte[besetzt[(x, y)]]
            warnungen.append(
                f"{ort.wo}: Lage im selben Kartenpixel ({x}, {y}) wie ort {erster.kennung}; das Pixel gehört "
                f"ort {erster.kennung}"
            )
            continue
        besetzt[(x, y)] = k
        lagepixel[k + 1] = (x, y)
        werte[y, x] = k + 1

    uhr = time.perf_counter()
    werte, bereinigung = kr.bereinigen(werte, lagepixel, n)
    zeiten["bereinigung"] = time.perf_counter() - uhr

    uhr = time.perf_counter()
    pixel, flaechen_km2 = kr.pixel_und_flaechen(raster, werte, n)
    bx, by, _ = kr.beschriftungspunkte(werte, n)
    zeiten["beschriftung"] = time.perf_counter() - uhr

    flaechen = []
    for k, ort in enumerate(orte):
        x, y = int(bx[k]), int(by[k])
        if x >= 0:
            laenge, breite = karte.pixelmitte(x, y)
        else:
            laenge, breite = ort.laenge, ort.breite
        flaechen.append(Flaeche(k, ort.kennung, ort.name, x, y, int(pixel[k]), flaechen_km2[k], laenge, breite))

    belegt = sorted(int(p) for p in pixel if p > 0)
    median = belegt[len(belegt) // 2] if belegt else 0
    schwelle = median * WENIGE_PIXEL_ANTEIL
    for k, ort in enumerate(orte):
        if pixel[k] == 0:
            warnungen.append(f"{ort.wo}: erhält keine Pixel")
        elif pixel[k] < schwelle:
            warnungen.append(
                f"{ort.wo}: erhält nur {int(pixel[k])} Pixel (≈ {flaechen_km2[k]} km², Median aller Orte: "
                f"{median} Pixel); Nachbarorte zu nah?"
            )

    uhr = time.perf_counter()
    ohne_ort: list[Teilflaeche] = []
    for anzahl_pixel, x, y in kr.teilflaechen((werte == 0) & (raster.art == KERNLAND)):
        laenge, breite = karte.pixelmitte(x, y)
        km2 = anzahl_pixel * raster.pixel_flaeche_fix[y] / float(1 << kr.FLAECHE_BITS)
        ohne_ort.append(Teilflaeche(anzahl_pixel, km2, x, y, laenge, breite))
    if ohne_ort:
        gesamt = sum(t.pixel for t in ohne_ort)
        gesamt_km2 = sum(t.flaeche_km2 for t in ohne_ort)
        warnungen.append(
            f"Kernraum-Land ohne Ort: {gesamt} Pixel (≈ {gesamt_km2:.0f} km²) in {len(ohne_ort)} Teilflächen "
            f"(weiter als max_radius_km = {karte.max_radius_km:g} von jedem Ort oder abgetrennt, z. B. Inseln ohne Ort)"
        )
        for t in ohne_ort[:OHNE_ORT_MELDUNGEN]:
            warnungen.append(
                f"Kernraum-Land ohne Ort: {t.pixel} Pixel (≈ {t.flaeche_km2:.1f} km²) ab Pixel ({t.x}, {t.y}) "
                f"bei {_zahl_text(t.laenge)} °O {_zahl_text(t.breite)} °N"
            )
        if len(ohne_ort) > OHNE_ORT_MELDUNGEN:
            warnungen.append(f"Kernraum-Land ohne Ort: … und {len(ohne_ort) - OHNE_ORT_MELDUNGEN} kleinere Teilflächen")
    zeiten["teilflaechen"] = time.perf_counter() - uhr

    return Ergebnis(
        karte=karte,
        orte=list(orte),
        werte=werte,
        art=raster.art,
        fluesse=grundlage.fluesse,
        flaechen=flaechen,
        ohne_ort=ohne_ort,
        warnungen=warnungen,
        bereinigung=bereinigung,
        zeiten=zeiten,
        flusslinien=grundlage.flusslinien,
    )


def erzeuge_karte(daten: Path | None = None, *, fluesse: bool = True) -> Ergebnis:
    """Lädt alles aus dem Datenordner und berechnet die Karte, ohne Dateien zu schreiben.

    Warnungen beim Laden (Geometrie, Orte) stehen vorn in ``Ergebnis.warnungen``. Wirft DatenFehler.
    """
    grundlage = lade_grundlage(daten, fluesse=fluesse)
    orte, warnungen = lade_orte_daten(daten)
    ergebnis = erzeuge(grundlage, orte)
    ergebnis.warnungen[:0] = grundlage.warnungen + warnungen
    return ergebnis


# ─── definition.csv ─────────────────────────────────────────────────────────


def definition_csv(flaechen: Sequence[Flaeche]) -> bytes:
    """Abschnitt 6.5: Semikolon-getrennt, UTF-8, Kopfzeile, LF-Zeilenenden."""
    zeilen = [CSV_KOPF] + [f"{f.kartenindex};{f.ort};{f.name};{f.x};{f.y};{f.pixel};{f.flaeche_km2}" for f in flaechen]
    return ("\n".join(zeilen) + "\n").encode("utf-8")


# ─── nachbarn.csv ───────────────────────────────────────────────────────────


def flussuebergaenge(
    orte: Sequence[Ort], paare: Sequence[tuple[int, int]], flusslinien: Sequence[tuple[int, np.ndarray]]
) -> dict[tuple[int, int], int]:
    """Kleinster Rang der Flusslinien, die die Strecke zwischen den Lagen zweier Orte echt kreuzen (0: keine).

    Kreuzungstest mit Orientierungen in Länge und Breite (eine affine Verzerrung ändert nichts daran);
    Berührungen zählen nicht. Die Flussabschnitte liegen in Zellen von FLUSS_ZELLE_GRAD.
    """
    starts, enden, raenge = [], [], []
    for rang, linie in flusslinien:
        if len(linie) >= 2:
            starts.append(linie[:-1])
            enden.append(linie[1:])
            raenge.append(np.full(len(linie) - 1, rang, dtype=np.int64))
    ergebnis = {paar: 0 for paar in paare}
    if not starts:
        return ergebnis
    a = np.concatenate(starts)
    b = np.concatenate(enden)
    rang = np.concatenate(raenge)
    zelle = FLUSS_ZELLE_GRAD
    lo = np.floor(np.minimum(a, b) / zelle).astype(np.int64)
    hi = np.floor(np.maximum(a, b) / zelle).astype(np.int64)
    zellen: dict[tuple[int, int], list[int]] = {}
    for i in range(len(a)):
        for cx in range(int(lo[i, 0]), int(hi[i, 0]) + 1):
            for cy in range(int(lo[i, 1]), int(hi[i, 1]) + 1):
                zellen.setdefault((cx, cy), []).append(i)
    felder = {k: np.array(v, dtype=np.int64) for k, v in zellen.items()}

    def orientierung(p: np.ndarray, q: np.ndarray, r: np.ndarray) -> np.ndarray:
        return (q[..., 0] - p[..., 0]) * (r[..., 1] - p[..., 1]) - (q[..., 1] - p[..., 1]) * (r[..., 0] - p[..., 0])

    for paar in paare:
        p = np.array([orte[paar[0]].laenge, orte[paar[0]].breite])
        q = np.array([orte[paar[1]].laenge, orte[paar[1]].breite])
        x0, y0 = np.floor(np.minimum(p, q) / zelle).astype(np.int64)
        x1, y1 = np.floor(np.maximum(p, q) / zelle).astype(np.int64)
        teile = [felder[(cx, cy)] for cx in range(x0, x1 + 1) for cy in range(y0, y1 + 1) if (cx, cy) in felder]
        if not teile:
            continue
        kandidaten = np.unique(np.concatenate(teile))
        ka, kb = a[kandidaten], b[kandidaten]
        d1 = orientierung(ka, kb, p)
        d2 = orientierung(ka, kb, q)
        d3 = orientierung(p, q, ka)
        d4 = orientierung(p, q, kb)
        kreuzt = (d1 * d2 < 0) & (d3 * d4 < 0)
        if kreuzt.any():
            ergebnis[paar] = int(rang[kandidaten][kreuzt].min())
    return ergebnis


def nachbarn_csv(orte: Sequence[Ort], grenzen: dict[tuple[int, int], int], fluesse: dict[tuple[int, int], int]) -> bytes:
    """Abschnitt 6.6 (seit M10): je Paar benachbarter Orte (Kartenindex a < b, aufsteigend) die
    Kennungen, die Länge der gemeinsamen Grenze in Pixelkanten und den Rang des größten Flusses, den
    die Verbindungslinie der Lagen kreuzt (0: keiner). UTF-8, Kopfzeile, LF-Zeilenenden."""
    zeilen = [NACHBARN_KOPF]
    for paar, grenze in sorted(grenzen.items()):
        zeilen.append(f"{orte[paar[0]].kennung};{orte[paar[1]].kennung};{grenze};{fluesse.get(paar, 0)}")
    return ("\n".join(zeilen) + "\n").encode("utf-8")


# ─── Dateien schreiben und prüfen ───────────────────────────────────────────

AUSGABEN = ("provinzen.png", "fluesse.png", "definition.csv", "nachbarn.csv")


def ausgaben(ergebnis: Ergebnis) -> dict[str, np.ndarray | bytes]:
    """Inhalte der Ausgabedateien (Bilder als Felder, CSV als Bytes)."""
    if ergebnis.fluesse is None:
        raise ValueError("Ergebnis ohne Flussebene (lade_grundlage(..., fluesse=True) verwenden)")
    return {
        "provinzen.png": ergebnis.provinzen(),
        "fluesse.png": ergebnis.fluesse,
        "definition.csv": ergebnis.definition_csv(),
        "nachbarn.csv": ergebnis.nachbarn_csv(),
    }


def _lies_bild(pfad: Path) -> np.ndarray | None:
    try:
        return karte_png.dekodieren(pfad.read_bytes())
    except (OSError, ValueError):
        return None


def _csv_zeilen(inhalt: bytes) -> list[str] | None:
    try:
        return inhalt.decode("utf-8").splitlines()
    except UnicodeDecodeError:
        return None


def _bildform(bild: np.ndarray) -> str:
    art = {1: "Graustufen", 3: "RGB", 4: "RGBA"}[bild.shape[2] if bild.ndim == 3 else 1]
    return f"{bild.shape[1]}×{bild.shape[0]} {art}"


def pruefe_ausgabe(ausgabe: Path, ergebnis: Ergebnis) -> list[str]:
    """Vergleicht die Dateien im Ausgabeordner mit dem Ergebnis; Liste der Abweichungen.

    Bilder: Pixel (nicht Bytes – andere zlib-Versionen dürfen anders komprimieren);
    definition.csv und nachbarn.csv: Inhalt zeilenweise (Zeilenenden egal).
    """
    abweichungen = []
    for name, erwartet in ausgaben(ergebnis).items():
        pfad = ausgabe / name
        try:
            daten = pfad.read_bytes()
        except OSError:
            abweichungen.append(f"{pfad}: Datei fehlt")
            continue
        if isinstance(erwartet, bytes):
            if _csv_zeilen(daten) != _csv_zeilen(erwartet):
                abweichungen.append(f"{pfad}: Inhalt veraltet")
            continue
        try:
            bild = karte_png.dekodieren(daten)
        except ValueError as fehler:
            abweichungen.append(f"{pfad}: nicht lesbar ({fehler})")
            continue
        if bild.shape != erwartet.shape:
            abweichungen.append(f"{pfad}: Bild {_bildform(bild)}, erwartet {_bildform(erwartet)}")
            continue
        anders = bild != erwartet
        if anders.ndim == 3:
            anders = anders.any(axis=2)
        anzahl = int(np.count_nonzero(anders))
        if anzahl:
            abweichungen.append(f"{pfad}: {anzahl} Pixel weichen ab")
    return abweichungen


def schreibe_ausgabe(ausgabe: Path, ergebnis: Ergebnis) -> list[tuple[Path, bool, int]]:
    """Schreibt die Ausgabedateien; unveränderte (gleiche Pixel bzw. gleicher Inhalt) bleiben unberührt.

    Ergebnis: (Pfad, geschrieben?, Bytes).
    """
    ausgabe.mkdir(parents=True, exist_ok=True)
    bericht = []
    for name, inhalt in ausgaben(ergebnis).items():
        pfad = ausgabe / name
        if isinstance(inhalt, bytes):
            try:
                gleich = pfad.read_bytes() == inhalt
            except OSError:
                gleich = False
            daten = inhalt
        else:
            alt = _lies_bild(pfad) if pfad.is_file() else None
            gleich = alt is not None and alt.shape == inhalt.shape and bool(np.array_equal(alt, inhalt))
            daten = b"" if gleich else karte_png.kodieren(inhalt)
        if not gleich:
            pfad.write_bytes(daten)
        bericht.append((pfad, not gleich, len(daten) if not gleich else pfad.stat().st_size))
    return bericht


# ─── Kommandozeile ──────────────────────────────────────────────────────────


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
        ("invalid int value:", "ungültige Ganzzahl:"),
        ("argument ", "Option "),
    )

    def error(self, message: str) -> NoReturn:
        for englisch, deutsch in self._UEBERSETZUNG:
            message = message.replace(englisch, deutsch)
        self.print_usage(sys.stderr)
        self.exit(2, f"Fehler: {message}\n")


def _ausschnitt(text: str) -> tuple[float, float, float, float]:
    teile = text.replace(" ", "").split(",")
    try:
        werte = tuple(float(t) for t in teile)
    except ValueError:
        werte = ()
    if len(werte) != 4 or not (werte[0] < werte[2] and werte[1] < werte[3]):
        raise argparse.ArgumentTypeError(
            f"'{text}' – erwartet LAENGE_MIN,BREITE_MIN,LAENGE_MAX,BREITE_MAX, z. B. 12.5,52.0,14.5,53.0"
        )
    return werte  # type: ignore[return-value]


def _argumente() -> argparse.ArgumentParser:
    parser = _Argumente(
        prog="kartengenerator.py",
        description="Erzeugt provinzen.png, fluesse.png, definition.csv und nachbarn.csv aus karte.chron, der Geometrie "
        "und den Orten (docs/technik/chronik-skript.md, Abschnitt 6).",
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
    optionen.add_argument("--ausgabe", type=Path, metavar="DIR", help="Ausgabeordner (Standard: <daten>/karte)")
    optionen.add_argument(
        "--pruefen",
        action="store_true",
        help="nichts schreiben, nur prüfen, ob die Dateien im Ausgabeordner aktuell sind (Exitcode 1, wenn nicht)",
    )
    optionen.add_argument(
        "--vorschau", type=Path, metavar="DATEI", help="zusätzlich eine politische Vorschau als PNG schreiben"
    )
    optionen.add_argument(
        "--vorschau-breite",
        type=int,
        metavar="N",
        help="Breite der Vorschau in Pixeln (verkleinert durch Mitteln, vergrößert ohne Glättung)",
    )
    optionen.add_argument(
        "--vorschau-ausschnitt",
        type=_ausschnitt,
        metavar="L0,B0,L1,B1",
        help="nur diesen Ausschnitt zeigen (Grad: Länge min, Breite min, Länge max, Breite max)",
    )
    optionen.add_argument("--vorschau-namen", action="store_true", help="Ortsnamen am Beschriftungspunkt eintragen")
    optionen.add_argument("--zeiten", action="store_true", help="Laufzeit der einzelnen Schritte ausgeben")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = _argumente()
    args = parser.parse_args(argv)
    if args.vorschau_breite is not None and not 16 <= args.vorschau_breite <= 16384:
        parser.error("Option --vorschau-breite: erwartet 16 bis 16384")
    daten: Path = args.daten
    ausgabe: Path = args.ausgabe if args.ausgabe is not None else daten / "karte"

    start = time.perf_counter()  # nur für die Laufzeitanzeige
    try:
        uhr = time.perf_counter()
        grundlage = lade_grundlage(daten)
        zeit_grundlage = time.perf_counter() - uhr
        uhr = time.perf_counter()
        orte, warnungen = lade_orte_daten(daten)
        zeit_orte = time.perf_counter() - uhr
    except DatenFehler as fehler:
        for text in fehler.meldungen:
            print(f"Fehler: {text}", file=sys.stderr)
        print(f"Abbruch: {len(fehler.meldungen)} Fehler in den Daten.", file=sys.stderr)
        return 2
    for text in grundlage.warnungen + warnungen:
        print(f"Warnung: {text}", file=sys.stderr)

    ergebnis = erzeuge(grundlage, orte)
    for text in ergebnis.warnungen:
        print(f"Warnung: {text}", file=sys.stderr)

    karte = grundlage.karte
    art = grundlage.art
    kernland = int(np.count_nonzero(art == KERNLAND))
    belegt = sum(f.pixel for f in ergebnis.flaechen)
    gesamt = karte.breite * karte.hoehe
    print(
        f"{len(orte)} Orte, Bild {karte.breite}×{karte.hoehe}; Kernraum-Land "
        f"{100.0 * kernland / gesamt:.1f} %, davon Orten zugeordnet {100.0 * belegt / max(kernland, 1):.2f} %; "
        f"Teilflächen umverteilt: {ergebnis.bereinigung.umverteilt}, ohne Ort: {ergebnis.bereinigung.entfernt}"
    )

    rueckgabe = 0
    uhr = time.perf_counter()
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
            print(f"Karte aktuell: {', '.join(str(ausgabe / n) for n in AUSGABEN)}")
    else:
        try:
            for pfad, geschrieben, groesse in schreibe_ausgabe(ausgabe, ergebnis):
                print(f"{'geschrieben' if geschrieben else 'unverändert'}: {pfad} ({groesse} Bytes)")
        except OSError as fehler:
            print(f"Fehler: Ausgabe kann nicht geschrieben werden: {fehler}", file=sys.stderr)
            return 2
    zeit_ausgabe = time.perf_counter() - uhr

    if args.vorschau is not None:
        import karte_vorschau  # noqa: PLC0415

        meldungen = Meldungen()
        staaten = lade_staaten(daten / "history" / "staaten", daten, meldungen)
        for text in meldungen.warnungen:
            print(f"Warnung: {text}", file=sys.stderr)
        try:
            bild = karte_vorschau.vorschau(
                ergebnis,
                staaten,
                breite=args.vorschau_breite,
                ausschnitt=args.vorschau_ausschnitt,
                namen=args.vorschau_namen,
            )
            args.vorschau.parent.mkdir(parents=True, exist_ok=True)
            args.vorschau.write_bytes(karte_png.kodieren(bild))
        except (OSError, ValueError) as fehler:
            print(f"Fehler: Vorschau kann nicht geschrieben werden: {fehler}", file=sys.stderr)
            return 2
        print(f"Vorschau: {args.vorschau} ({bild.shape[1]}×{bild.shape[0]})")

    if args.zeiten:
        teile = {"Geometrie": zeit_grundlage, "Orte laden": zeit_orte}
        teile.update({k.capitalize(): v for k, v in ergebnis.zeiten.items()})
        teile["Ausgabe/Prüfung"] = zeit_ausgabe
        print("Zeiten: " + ", ".join(f"{k} {v:.1f} s" for k, v in teile.items()))
    print(f"Laufzeit: {time.perf_counter() - start:.1f} s")
    return rueckgabe


if __name__ == "__main__":
    sys.exit(main())
