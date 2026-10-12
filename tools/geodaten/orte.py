#!/usr/bin/env python3
"""Orte des Kernraums (Meilenstein M3): ``data/tabellen/orte.csv``, ``kreise.csv`` und ``dateien.csv``.

Erzeugt aus GeoNames ``cities1000`` (über ``staedte.py``), dem Höhenmodell (``hoehen.py``) und
der Karte (``tools/kartengenerator``) reproduzierbar rund 4.500 Orte und 620 Kreise für den ganzen
Kernraum. Die 60 Orte der Testregion (Meilenstein M1, mit Historie) bleiben unverändert; sie
erhalten nur einen Kreis und ihre Fläche aus der Karte. Neue Orte sind *Orte ohne Daten*
(docs/technik/chronik-skript.md, Abschnitt 3.6): Sie haben keine Historie.

Ablauf (Einzelheiten und Begründungen in README.md, Abschnitt „Orte des Kernraums“):

1. Kandidaten: Siedlungen aus cities1000 im Kernraum (PPL, PPLA … PPLA4, PPLC), ohne die Einträge
   aus ``ausschluesse.csv`` (moderne Gründungen, Kunstnamen) und ohne die Entsprechungen der M1-Orte;
   Einwohnerzahlen, die GeoNames für Verwaltungssitze nicht führt, aus ``einwohner.csv``.
2. Landschaft je Kandidat aus ``landschaften.csv`` (Staat und Verwaltungscodes).
3. Auswahl: gewichtete Poisson-Scheibe (wichtige Orte zuerst, kleinerer Mindestabstand für
   Verwaltungssitze und große Orte, größerer in den Säumen), dann je Insel ohne Ort der wichtigste
   Kandidat, dann Lückenfüllung bis höchstens ``FUELL_RADIUS_KM`` Abstand.
4. Lage auf Kernraum-Land (Küstenorte knapp im Wasser auf das nächste Landpixel), Gelände aus dem
   Höhenmodell, deutscher Name (``exonyme.csv``, nur wörtlich aus den GeoNames-Alternativnamen,
   auch mit Ortszusatz wie „Braunsberg  Ostpreußen“), Kennung, Stadtrecht-Näherung.
5. Karte mit dem Kartengenerator (Python-API) in Ladereihenfolge; Kreise aus benachbarten Orten je
   Landschaft (4–12 Orte); Fläche aller Orte aus der Karte.

Aufruf::

    python3 tools/geodaten/orte.py              # Tabellen schreiben (danach Archivar und Kartengenerator)
    python3 tools/geodaten/orte.py --pruefen    # nur vergleichen: Exitcode 1, wenn veraltet

Danach: ``python3 tools/archivar/archivar.py`` und ``python3 tools/kartengenerator/kartengenerator.py``.
Benötigt die Rohdaten im Zwischenspeicher (``geodaten.py holen``) und numpy.
Exitcodes: 0 = in Ordnung, 1 = ``--pruefen``: Tabellen nicht aktuell, 2 = Fehler.
"""

from __future__ import annotations

import argparse
import csv
import io
import math
import re
import sys
import textwrap
import time
import unicodedata
from collections.abc import Iterable, Sequence
from dataclasses import dataclass, field, replace
from pathlib import Path
from typing import Any

import numpy as np

try:
    import geo
    import geodaten
    import hoehen
    import staedte
except ImportError:  # Aufruf als Modul von außerhalb des Werkzeugordners
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import geo
    import geodaten
    import hoehen
    import staedte

_KG_ORDNER = str(geo.REPO / "tools/kartengenerator")
if _KG_ORDNER not in sys.path:
    sys.path.insert(0, _KG_ORDNER)
import karte_raster as kr  # noqa: E402
import kartengenerator as kg  # noqa: E402

# ─── Pfade ──────────────────────────────────────────────────────────────────

HIER = Path(__file__).resolve().parent
REPO = geo.REPO
DATEN = REPO / "data"
TABELLEN = DATEN / "tabellen"
LANDSCHAFTEN = HIER / "landschaften.csv"
LANDSCHAFTEN_DATEIEN = HIER / "landschaften_dateien.csv"
EXONYME = HIER / "exonyme.csv"
AUSSCHLUESSE = HIER / "ausschluesse.csv"
EINWOHNER = HIER / "einwohner.csv"

# ─── Parameter ──────────────────────────────────────────────────────────────

#: Siedlungscodes aus GeoNames, die als Ort taugen (keine Ortsteile PPLX, keine Wüstungen PPLH/PPLQ/PPLW,
#: keine Orte ohne feste Bevölkerung wie PPLL/PPLS/PPLG).
SIEDLUNGSCODES = frozenset({"PPL", "PPLA", "PPLA2", "PPLA3", "PPLA4", "PPLC"})

#: Verwaltungsrang je Staat und GeoNames-Code: 1 = Haupt- oder Landeshauptstadt, 2 = Sitz einer Verwaltung
#: auf Kreisebene (Kreis, Bezirk, Powiat, Okres, Arrondissement, Kommune in Dänemark, Rajon), 3 = Sitz
#: einer Gemeinde, 4 = übrige Siedlung. GeoNames verwendet die Codes je Staat verschieden (in Slowenien
#: ist PPLA der Sitz einer Gemeinde, in Deutschland PPLA3 der Sitz eines Landkreises).
VERWALTUNGSRANG: dict[str, dict[str, int]] = {
    "DE": {"PPLC": 1, "PPLA": 1, "PPLA2": 2, "PPLA3": 2, "PPLA4": 3, "PPL": 4},
    "AT": {"PPLC": 1, "PPLA": 1, "PPLA2": 2, "PPLA3": 3, "PPL": 4},
    "PL": {"PPLA": 1, "PPLA2": 2, "PPLA3": 3, "PPL": 4},
    "CZ": {"PPLC": 1, "PPLA": 1, "PPLA2": 2, "PPLA3": 3, "PPLA4": 3, "PPL": 4},
    "FR": {"PPLA": 1, "PPLA2": 2, "PPLA3": 2, "PPL": 4},
    "BE": {"PPLC": 1, "PPLA": 1, "PPLA2": 2, "PPLA3": 2, "PPLA4": 3, "PPL": 4},
    "NL": {"PPLA": 1, "PPLA2": 3, "PPL": 4},
    "SI": {"PPLC": 1, "PPLA": 3, "PPLA3": 3, "PPL": 4},
    "IT": {"PPLA2": 1, "PPLA3": 3, "PPL": 4},
    "LU": {"PPLC": 1, "PPLA": 2, "PPLA2": 2, "PPLA3": 3, "PPL": 4},
    "DK": {"PPLA2": 2, "PPL": 4},
    "RU": {"PPLA": 1, "PPLA2": 2, "PPL": 4},
    "LT": {"PPLA": 1, "PPLA2": 2, "PPL": 4},
    "LI": {"PPLC": 1, "PPLA": 3},
}
#: Zuschlag auf die Bedeutung (log10 der Einwohner) je Verwaltungsrang.
RANG_ZUSCHLAG = {1: 1.0, 2: 0.5, 3: 0.1, 4: 0.0}
#: Untergrenze der Einwohnerzahl in der Bedeutung (GeoNames führt manche Gemeindesitze mit 0).
MIN_EINWOHNER_BEDEUTUNG = 300
#: Bekanntheitszuschlag: BEKANNTHEIT_JE_VERDOPPLUNG × log2(1 + Zahl der Alternativnamen), höchstens
#: BEKANNTHEIT_MAX. Viele Alternativnamen (Namen in vielen Sprachen) kennzeichnen bekannte Orte; so
#: gewinnt etwa Bad Ischl, das GeoNames mit zu wenigen Einwohnern führt, gegen seinen Ortsteil Reiterndorf.
BEKANNTHEIT_JE_VERDOPPLUNG = 0.1
BEKANNTHEIT_MAX = 0.5

#: Grundabstand der Poisson-Scheibe in km. Der Mindestabstand eines Kandidaten zu allen schon
#: gewählten Orten ist Grundabstand × Faktor(Bedeutung) × Abstandsfaktor der Landschaft.
GRUNDABSTAND_KM = 7.95
#: Faktor für unbedeutende Orte (Bedeutung ≤ BEDEUTUNG_UNTEN) bzw. bedeutende (≥ BEDEUTUNG_OBEN), linear dazwischen.
FAKTOR_UNBEDEUTEND = 1.2
FAKTOR_BEDEUTEND = 0.55
BEDEUTUNG_UNTEN = 3.2
BEDEUTUNG_OBEN = 6.8
#: Faktor auf den Mindestabstand zu Orten einer anderen Landschaft.
GRENZ_FAKTOR = 0.8
#: Lückenfüllung: Kernraum-Land, das weiter als FUELL_RADIUS_KM vom nächsten Ort liegt, erhält den
#: nächstgelegenen Kandidaten, der mindestens FUELL_MIN_ABSTAND_KM von allen Orten entfernt ist.
FUELL_RADIUS_KM = 15.0
FUELL_MIN_ABSTAND_KM = 6.0
#: Auflösung der Abstandskarte für die Lückenfüllung (Blöcke aus N × N Kartenpixeln).
FUELL_BLOCK = 4
#: Die Abstandskarte trägt Abstände nur bis FUELL_KARTE_KM um jeden Ort ein (weiter entfernte Blöcke
#: bleiben unendlich); die Kandidatensuche für einen Block reicht höchstens FUELL_SUCHE_MAX_KM weit.
FUELL_KARTE_KM = 40.0
FUELL_SUCHE_MAX_KM = 100.0
#: Ein Kandidat entspricht einem M1-Ort, wenn er höchstens so weit entfernt liegt und gleich heißt.
M1_ABSTAND_KM = 5.0
#: Höchstabstand, um die Lage eines Ortes im Wasser auf das nächste Kernraum-Land-Pixel zu setzen.
LAND_SUCHE_KM = 3.0
#: Nachkommastellen der Lage in orte.csv (0,0001° ≈ 11 m; ein Kartenpixel misst etwa 370 m).
LAGE_STELLEN = 4

#: Stadtrecht-Näherung: Verwaltungssitz (Rang 1 oder 2) oder mindestens so viele heutige Einwohner.
STADT_EINWOHNER = 20_000

#: Gelände: Höhen der Landpixel im Umkreis GELAENDE_RADIUS_KM (Mittel, 5- und 95-%-Perzentil; die
#: Perzentile statt Minimum und Maximum, weil die Höhenkacheln vereinzelt Ausreißer enthalten).
#: Relief = p95 − p5. Schwellen in Metern bzw. km; Vorrang in der Reihenfolge von ``gelaende_aus``.
GELAENDE_RADIUS_KM = 6.0
KUESTE_KM = 6.0
KUESTE_MITTEL_M = 30.0
HOCHGEBIRGE_P95_M = 1500.0
HOCHGEBIRGE_MITTEL_M = 1200.0
MITTELGEBIRGE_RELIEF_M = 300.0
MITTELGEBIRGE_MITTEL_M = 450.0
MITTELGEBIRGE_HOCHFLAECHE_RELIEF_M = 150.0
GEBIET_MAX_RELIEF_M = 150.0
FLUSSAUE_FLUSS_KM = 3.0
FLUSSAUE_UEBER_TAL_M = 40.0
FLUSSAUE_MAX_RELIEF_M = 150.0
SEENPLATTE_RADIUS_KM = 10.0
SEENPLATTE_ANTEIL = 0.05
SEENPLATTE_MAX_MITTEL_M = 200.0
HUEGELLAND_RELIEF_M = 100.0
HUEGELLAND_MITTEL_M = 200.0
HUEGELLAND_HOCHFLAECHE_RELIEF_M = 50.0
#: Orte auf Inseln bis zu dieser Größe sind immer Küste (größere Inseln wie Rügen nach den übrigen Regeln).
INSEL_MAX_KM2 = 400.0
#: Haffe: Binnengewässer ab dieser Größe, die im Kartenraster ans Meer grenzen (das Frische Haff, der
#: Lebasee), zählen als Küstengewässer: Sie gehen in den Küstenabstand ein, nicht in den Seenanteil.
HAFF_MIN_KM2 = 20.0

#: Kreise: 4–12 benachbarte Orte je Landschaft, im Mittel KREIS_ZIEL.
KREIS_MIN = 4
KREIS_MAX = 12
KREIS_ZIEL = 7.1

#: Erkennungszeichen der erzeugten Zeilen (Spalte quelle); andere Zeilen in orte.csv sind M1-Orte.
QUELLE_PRAEFIX = "GeoNames (CC BY 4.0), geonameid "
KREIS_QUELLE = "räumlich gebildet (M3), keine historische Verwaltungseinheit"
#: Absatz, den orte.py an die Kopfkommentare der M1-Dateien anhängt (wird bei jedem Lauf ersetzt).
M3_ABSATZ = "Ergänzt in Meilenstein M3 (tools/geodaten/orte.py):"
#: Zeilenbreite der erzeugten Absätze in dateien.csv (wie die übrigen Kopfkommentare, etwa 90 Zeichen).
KOMMENTAR_BREITE = 92

ORTE_SPALTEN = (
    "id",
    "datei",
    "name",
    "gelaende",
    "laenge",
    "breite",
    "flaeche",
    "stadt",
    "kreis",
    "vorkommen",
    "hafen",
    "quelle",
)
#: Spalten von orte.csv, die von Hand gepflegt werden und beim Neuerzeugen erhalten bleiben (seit M5).
ORTE_HANDSPALTEN = ("vorkommen", "hafen")
KREISE_SPALTEN = ("id", "datei", "name", "quelle")
DATEIEN_SPALTEN = ("ordner", "datei", "kultur", "kommentar")

#: Staaten, deren GeoNames-Namen (nach Bereinigung) deutsch sind; Ausnahmen über exonyme.csv.
DEUTSCHE_STAATEN = frozenset({"DE", "AT", "LI"})
#: Moderne Namenszusätze, die entfallen (Titel des 19. und 20. Jahrhunderts).
NAMENSZUSAETZE_VORN = (
    "Lutherstadt ",
    "Hansestadt ",
    "Ostseebad ",
    "Nordseebad ",
    "Seebad ",
    "Kurort ",
    "Luftkurort ",
    "Heilbad ",
    "Staatsbad ",
    "Seeheilbad ",
    "Gemeinde ",
)
NAMENSZUSAETZE_HINTEN = (" Lutherstadt", " Stadt", "-Stadt", " Markt", " Kurort")
#: Ein Zusatz am Ende bleibt, wenn er zum Namen gehört: nach einem Artikel („Weil der Stadt“) oder in
#: einer Fügung mit zusammengezogener Präposition und höchstens einem Wort („Stetten am Kalten Markt“).
#: „Sankt Peter in der Au Markt“ verliert den Zusatz dagegen, „in der Au“ ist vollständig.
_ZUSATZ_GEHOERT_ZUM_NAMEN = re.compile(r"(?:^|\s)(?:(?:am|im|zum|zur|beim|vom)(?:\s+\S+)?|der|dem|den|des)$")


class OrteFehler(Exception):
    """Fehler mit deutscher Meldung (Exitcode 2)."""


# ─── Tabellen ───────────────────────────────────────────────────────────────


def lies_csv(pfad: Path, spalten: Sequence[str] | None = None) -> list[dict[str, str]]:
    """Semikolon-CSV mit Kopfzeile (UTF-8, BOM erlaubt); prüft die Spalten, falls angegeben."""
    try:
        text = pfad.read_text(encoding="utf-8-sig")
    except OSError as fehler:
        raise OrteFehler(f"{pfad}: {fehler}") from fehler
    zeilen = list(csv.reader(io.StringIO(text, newline=""), delimiter=";"))
    if not zeilen:
        raise OrteFehler(f"{pfad}: leer")
    kopf = zeilen[0]
    if spalten is not None and tuple(kopf) != tuple(spalten):
        raise OrteFehler(f"{pfad}: Kopfzeile {';'.join(kopf)!r}, erwartet {';'.join(spalten)!r}")
    ergebnis = []
    for nr, zeile in enumerate(zeilen[1:], start=2):
        if not zeile or all(not z.strip() for z in zeile):
            continue
        if len(zeile) != len(kopf):
            raise OrteFehler(f"{pfad}:{nr}: {len(zeile)} Spalten, erwartet {len(kopf)}")
        ergebnis.append({k: v.strip() for k, v in zip(kopf, zeile)})
    return ergebnis


def csv_text(spalten: Sequence[str], zeilen: Iterable[Sequence[str]]) -> str:
    """Semikolon-CSV wie in data/tabellen (minimal gequotet, LF)."""
    ausgabe = io.StringIO(newline="")
    schreiber = csv.writer(ausgabe, delimiter=";", lineterminator="\n", quoting=csv.QUOTE_MINIMAL)
    schreiber.writerow(spalten)
    schreiber.writerows(zeilen)
    return ausgabe.getvalue()


@dataclass(frozen=True)
class Zuordnungsregel:
    """Eine Zeile aus landschaften.csv."""

    staat: str
    admin: tuple[str, str, str, str]  # admin1 … admin4, leer = beliebig
    landschaft: str
    zeile: int

    @property
    def tiefe(self) -> int:
        """Tiefste angegebene Verwaltungsebene (0 = nur Staat)."""
        return max((i + 1 for i, a in enumerate(self.admin) if a), default=0)

    def passt(self, staat: str, admin: Sequence[str]) -> bool:
        """Leeres Feld = beliebig, „-“ = Feld muss leer sein (für fehlerhafte GeoNames-Codes)."""
        return self.staat == staat and all(not a or (a == "-" and not b) or a == b for a, b in zip(self.admin, admin))


@dataclass(frozen=True)
class Landschaft:
    """Eine Zeile aus landschaften_dateien.csv."""

    kennung: str  # Dateiname in history/orte und history/kreise
    name: str
    kurzname: str  # Zusatz bei gleichnamigen Orten, z. B. „Neustadt (Oberschlesien)“
    abstand: float  # Faktor auf den Mindestabstand der Auswahl
    kommentar: str


@dataclass
class Hilfstabellen:
    regeln: list[Zuordnungsregel]
    landschaften: dict[str, Landschaft]
    exonyme: dict[int, tuple[str, bool, str]]  # geonameid → (name_de, geprüft, heutiger Name)
    ausschluesse: dict[int, tuple[str, str, str]]  # geonameid → (Name, Art, Grund)
    einwohner: dict[int, tuple[str, int, str]] = field(default_factory=dict)  # geonameid → (Name, Einwohner, Beleg)


def lade_hilfstabellen(ordner: Path = HIER) -> Hilfstabellen:
    """Liest landschaften.csv, landschaften_dateien.csv, exonyme.csv, ausschluesse.csv und einwohner.csv."""
    landschaften: dict[str, Landschaft] = {}
    for z in lies_csv(ordner / LANDSCHAFTEN_DATEIEN.name, ("landschaft", "name", "kurzname", "abstand", "kommentar")):
        kennung = z["landschaft"]
        if not kennung or not all("a" <= c <= "z" or c == "_" for c in kennung) or kennung in landschaften:
            raise OrteFehler(f"landschaften_dateien.csv: ungültige oder doppelte Landschaft {kennung!r}")
        try:
            abstand = float(z["abstand"])
        except ValueError:
            raise OrteFehler(f"landschaften_dateien.csv: Abstand von {kennung} ist keine Zahl") from None
        if not 0.5 <= abstand <= 2.0 or not z["name"] or not z["kurzname"] or not z["kommentar"]:
            raise OrteFehler(
                f"landschaften_dateien.csv: {kennung}: Name, Kurzname, Kommentar oder Abstand (0,5–2) fehlt"
            )
        landschaften[kennung] = Landschaft(kennung, z["name"], z["kurzname"], abstand, z["kommentar"])
    regeln = []
    gesehen: dict[tuple[str, tuple[str, ...]], int] = {}
    pfad = ordner / LANDSCHAFTEN.name
    for nr, z in enumerate(
        lies_csv(pfad, ("staat", "admin1", "admin2", "admin3", "admin4", "landschaft", "bemerkung")), start=2
    ):
        admin = (z["admin1"], z["admin2"], z["admin3"], z["admin4"])
        if z["landschaft"] not in landschaften:
            raise OrteFehler(f"landschaften.csv:{nr}: unbekannte Landschaft {z['landschaft']!r}")
        schluessel = (z["staat"], admin)
        if schluessel in gesehen:
            raise OrteFehler(f"landschaften.csv:{nr}: gleiche Codes wie Zeile {gesehen[schluessel]}")
        gesehen[schluessel] = nr
        regeln.append(Zuordnungsregel(z["staat"], admin, z["landschaft"], nr))
    exonyme: dict[int, tuple[str, bool, str]] = {}
    for z in lies_csv(ordner / EXONYME.name, ("geonameid", "name_de", "geprueft", "heute")):
        try:
            gid = int(z["geonameid"])
        except ValueError:
            raise OrteFehler(f"exonyme.csv: ungültige geonameid {z['geonameid']!r}") from None
        if gid in exonyme or z["geprueft"] not in ("ja", "nein") or not z["name_de"]:
            raise OrteFehler(f"exonyme.csv: geonameid {gid}: doppelt, Name leer oder 'geprueft' nicht ja/nein")
        exonyme[gid] = (z["name_de"], z["geprueft"] == "ja", z["heute"])
    ausschluesse: dict[int, tuple[str, str, str]] = {}
    for z in lies_csv(ordner / AUSSCHLUESSE.name, ("geonameid", "name", "art", "grund")):
        try:
            gid = int(z["geonameid"])
        except ValueError:
            raise OrteFehler(f"ausschluesse.csv: ungültige geonameid {z['geonameid']!r}") from None
        if (
            gid in ausschluesse
            or z["art"] not in ("neugruendung", "kunstname", "ortsteil", "doppelt", "datenfehler")
            or not z["grund"]
        ):
            raise OrteFehler(f"ausschluesse.csv: geonameid {gid}: doppelt, Art unbekannt oder Grund leer")
        ausschluesse[gid] = (z["name"], z["art"], z["grund"])
    einwohner: dict[int, tuple[str, int, str]] = {}
    for z in lies_csv(ordner / EINWOHNER.name, ("geonameid", "name", "einwohner", "beleg")):
        try:
            gid, zahl = int(z["geonameid"]), int(z["einwohner"])
        except ValueError:
            raise OrteFehler(f"einwohner.csv: geonameid oder Einwohnerzahl keine Ganzzahl ({z['geonameid']!r})") from None
        if gid in einwohner or zahl < 1 or not z["name"] or not z["beleg"]:
            raise OrteFehler(f"einwohner.csv: geonameid {gid}: doppelt, Einwohnerzahl < 1, Name oder Beleg leer")
        einwohner[gid] = (z["name"], zahl, z["beleg"])
    return Hilfstabellen(regeln, landschaften, exonyme, ausschluesse, einwohner)


def setze_einwohner(
    kandidaten: Sequence[staedte.Kandidat], einwohner: dict[int, tuple[str, int, str]]
) -> list[staedte.Kandidat]:
    """Ersetzt die GeoNames-Einwohnerzahl nach einwohner.csv (Verwaltungssitze, die GeoNames mit 0 führt).

    Die Zahl wirkt auf Bedeutung, Stadt-Näherung und Hauptort eines Kreises. Fehler, wenn der Name in
    der Tabelle nicht mehr zum GeoNames-Eintrag passt.
    """
    ergebnis = []
    for k in kandidaten:
        eintrag = einwohner.get(k.stadt.geonameid)
        if eintrag is not None:
            if eintrag[0] != k.stadt.name:
                raise OrteFehler(
                    f"einwohner.csv: {k.stadt.geonameid}: Name {eintrag[0]!r}, GeoNames-Name ist {k.stadt.name!r}"
                )
            k = replace(k, stadt=replace(k.stadt, einwohner=eintrag[1]))
        ergebnis.append(k)
    return ergebnis


def landschaft_nach_regeln(regeln: Sequence[Zuordnungsregel], staat: str, admin: Sequence[str]) -> str | None:
    """Landschaft der passenden Regel mit der tiefsten Verwaltungsebene (None, wenn keine passt)."""
    beste: Zuordnungsregel | None = None
    for regel in regeln:
        if regel.passt(staat, admin) and (beste is None or regel.tiefe > beste.tiefe):
            beste = regel
    return beste.landschaft if beste else None


# ─── Namen und Kennungen ────────────────────────────────────────────────────

_ERSATZ = {"ä": "ae", "ö": "oe", "ü": "ue", "ß": "ss", "ł": "l", "ø": "oe", "æ": "ae", "å": "aa", "đ": "d", "œ": "oe"}


def kennung_aus_name(name: str) -> str:
    """Kennung aus einem Namen: ä→ae, ö→oe, ü→ue, ß→ss, andere Zeichen ohne Akzent, sonst '_'."""
    text = name.lower()
    teile = []
    for zeichen in text:
        if zeichen in _ERSATZ:
            teile.append(_ERSATZ[zeichen])
            continue
        zerlegt = unicodedata.normalize("NFKD", zeichen)
        basis = "".join(c for c in zerlegt if not unicodedata.combining(c))
        teile.append(basis if basis and all("a" <= c <= "z" or "0" <= c <= "9" for c in basis) else "_")
    kennung = "_".join(t for t in "".join(teile).split("_") if t)
    if not kennung:
        raise OrteFehler(f"aus dem Namen {name!r} entsteht keine Kennung")
    if kennung[0].isdigit():
        kennung = "ort_" + kennung
    return kennung


def bereinige_name(name: str) -> str:
    """Entfernt moderne Namenszusätze („Lutherstadt “, „Ostseebad “ …, „ Stadt“, „ Markt“ am Ende).

    Ein Zusatz am Ende bleibt, wenn er Teil des Namens ist (``_ZUSATZ_GEHOERT_ZUM_NAMEN``): „Weil der
    Stadt“, „Stetten am Kalten Markt“.
    """
    ergebnis = name
    for zusatz in NAMENSZUSAETZE_VORN:
        if ergebnis.startswith(zusatz) and len(ergebnis) > len(zusatz):
            ergebnis = ergebnis[len(zusatz) :]
    for zusatz in NAMENSZUSAETZE_HINTEN:
        rest = ergebnis[: -len(zusatz)]
        if ergebnis.endswith(zusatz) and rest.strip() and not _ZUSATZ_GEHOERT_ZUM_NAMEN.search(rest):
            ergebnis = rest
    return ergebnis.strip()


@dataclass(frozen=True)
class Namenswahl:
    name: str
    herkunft: str  # Teil der Quellenangabe


def namensbeleg(name_de: str, stadt: staedte.Stadt) -> str | None:
    """Der GeoNames-Name, der ``name_de`` belegt, oder None.

    Beleg ist der Name selbst (GeoNames-Name oder Alternativname) oder ein Alternativname, der aus dem
    Namen und einem Ortszusatz besteht, getrennt durch Leerzeichen oder Schrägstrich, der Zusatz mit
    großem Anfangsbuchstaben (etwa „Braunsberg  Ostpreußen“ für Braunsberg, „Landskron/Schönhengstgau“
    für Landskron). Bei mehreren Zusatzformen gilt die erste in der Reihenfolge von GeoNames.
    """
    if name_de == stadt.name or name_de in stadt.alternativnamen:
        return name_de
    for alternativ in stadt.alternativnamen:
        rest = alternativ[len(name_de) :]
        if alternativ.startswith(name_de) and rest[:1] in (" ", "/") and rest.lstrip(" /")[:1].isupper():
            return alternativ
    return None


def waehle_name(stadt: staedte.Stadt, exonyme: dict[int, tuple[str, bool, str]]) -> Namenswahl:
    """Deutscher Name eines Kandidaten.

    1. Eintrag in exonyme.csv mit geprueft = ja: dieser Name (er steht wörtlich in den Alternativnamen,
       allein oder mit Ortszusatz, siehe ``namensbeleg``; das prüft ``pruefe_exonyme``).
    2. Orte in DE, AT, LI: GeoNames-Name ohne moderne Zusätze.
    3. Südtirol (IT) mit zweisprachigem GeoNames-Namen „Italienisch - Deutsch“: der deutsche Teil.
    4. Sonst der heutige Landesname.
    """
    eintrag = exonyme.get(stadt.geonameid)
    if eintrag is not None and eintrag[1]:
        beleg = namensbeleg(eintrag[0], stadt)
        if eintrag[0] == stadt.name:
            return Namenswahl(eintrag[0], "deutscher Name nach GeoNames (dort Hauptname des Eintrags)")
        zusatz = "" if beleg in (None, eintrag[0]) else f"dort mit Ortszusatz als {' '.join(str(beleg).split())}, "
        if stadt.staat in DEUTSCHE_STAATEN:
            return Namenswahl(
                eintrag[0], f"Name {eintrag[0]} aus den GeoNames-Alternativnamen ({zusatz}GeoNames: {stadt.name})"
            )
        return Namenswahl(eintrag[0], f"deutscher Name aus den GeoNames-Alternativnamen ({zusatz}heute {stadt.name})")
    if stadt.staat in DEUTSCHE_STAATEN:
        name = bereinige_name(stadt.name)
        if name != stadt.name:
            return Namenswahl(name, f"Name nach GeoNames ohne modernen Zusatz ({stadt.name})")
        return Namenswahl(name, "Name nach GeoNames")
    if stadt.staat == "IT" and " - " in stadt.name:
        name = stadt.name.split(" - ", 1)[1].strip()
        return Namenswahl(name, f"deutscher Teil des zweisprachigen GeoNames-Namens ({stadt.name})")
    return Namenswahl(stadt.name, "heutiger Name nach GeoNames, kein abweichender deutscher Name belegt")


def pruefe_exonyme(exonyme: dict[int, tuple[str, bool, str]], staedte_nach_id: dict[int, staedte.Stadt]) -> list[str]:
    """Fehler, wenn 'geprueft' nicht zum Inhalt der GeoNames-Alternativnamen passt oder 'heute' falsch ist."""
    fehler = []
    for gid, (name_de, geprueft, heute) in sorted(exonyme.items()):
        stadt = staedte_nach_id.get(gid)
        if stadt is None:
            continue  # nicht (mehr) im Kernraum oder ausgeschlossen: Eintrag wirkt nicht
        steht_drin = namensbeleg(name_de, stadt) is not None
        if geprueft != steht_drin:
            soll = "ja" if steht_drin else "nein"
            fehler.append(f"exonyme.csv: {gid} {name_de!r}: geprueft muss {soll} sein")
        if heute != stadt.name:
            fehler.append(f"exonyme.csv: {gid}: Spalte heute ist {heute!r}, GeoNames-Name ist {stadt.name!r}")
    return fehler


def zusatz_bei_gleichnamigen(namen: Sequence[str], zusaetze: Sequence[str], feste: Sequence[str]) -> list[str]:
    """Hängt gleichnamigen Orten „ (Zusatz)“ an, z. B. „Neustadt (Oberschlesien)“.

    Gleichnamig heißt: Der Name kommt unter ``namen`` und ``feste`` (M1-Orte, die unverändert bleiben)
    mehr als einmal vor. Namen, die schon einen Klammerzusatz haben, bleiben unverändert.
    """
    zaehler: dict[str, int] = {}
    for n in [*feste, *namen]:
        zaehler[n] = zaehler.get(n, 0) + 1
    return [f"{n} ({z})" if zaehler[n] > 1 and "(" not in n else n for n, z in zip(namen, zusaetze)]


def vergebe_kennungen(
    basen: Sequence[str], landschaften: Sequence[str], fluesse: Sequence[str], belegt: set[str]
) -> list[str]:
    """Eindeutige Kennungen: Basis; bei Gleichheit mit Landschaft, dann mit Fluss, sonst mit Zahl.

    ``basen``, ``landschaften`` und ``fluesse`` (Kennung des nächsten Flusses oder leer) je Ort in
    Rangfolge; ``belegt`` sind schon vergebene Kennungen (M1-Orte). Gleiche Basen erhalten alle einen
    Zusatz, damit kein Ort bevorzugt wird.
    """
    n = len(basen)
    zaehler: dict[str, int] = {}
    for b in basen:
        zaehler[b] = zaehler.get(b, 0) + 1
    ergebnis = list(basen)
    for i in range(n):
        if zaehler[basen[i]] > 1 or basen[i] in belegt:
            ergebnis[i] = f"{basen[i]}_{landschaften[i]}"
    zaehler = {}
    for e in ergebnis:
        zaehler[e] = zaehler.get(e, 0) + 1
    for i in range(n):
        if (zaehler[ergebnis[i]] > 1 or ergebnis[i] in belegt) and fluesse[i] and ergebnis[i] != basen[i]:
            ergebnis[i] = f"{ergebnis[i]}_{fluesse[i]}"
    vergeben = set(belegt)
    final = []
    for e in ergebnis:
        kandidat, nr = e, 2
        while kandidat in vergeben:
            kandidat = f"{e}_{nr}"
            nr += 1
        vergeben.add(kandidat)
        final.append(kandidat)
    return final


# ─── Bedeutung und Auswahl ──────────────────────────────────────────────────


def verwaltungsrang(stadt: staedte.Stadt) -> int:
    rang = VERWALTUNGSRANG.get(stadt.staat, {}).get(stadt.merkmalscode)
    if rang is None:
        raise OrteFehler(f"kein Verwaltungsrang für {stadt.staat}/{stadt.merkmalscode} ({stadt.name})")
    return rang


def bedeutung(stadt: staedte.Stadt) -> float:
    """log10(Einwohner, mindestens 300) plus Zuschlag nach Verwaltungsrang und nach Bekanntheit."""
    bekanntheit = min(BEKANNTHEIT_MAX, BEKANNTHEIT_JE_VERDOPPLUNG * math.log2(1 + len(stadt.alternativnamen)))
    return (
        math.log10(max(stadt.einwohner, MIN_EINWOHNER_BEDEUTUNG)) + RANG_ZUSCHLAG[verwaltungsrang(stadt)] + bekanntheit
    )


def abstandsfaktor(wert: float) -> float:
    """Faktor auf den Grundabstand: FAKTOR_UNBEDEUTEND bis FAKTOR_BEDEUTEND, linear in der Bedeutung."""
    t = min(1.0, max(0.0, (wert - BEDEUTUNG_UNTEN) / (BEDEUTUNG_OBEN - BEDEUTUNG_UNTEN)))
    return FAKTOR_UNBEDEUTEND + (FAKTOR_BEDEUTEND - FAKTOR_UNBEDEUTEND) * t


class Punktgitter:
    """Gitter für Nachbarschaftsabfragen von Punkten (Länge, Breite) in Grad."""

    def __init__(self, zelle_grad: float = 0.25) -> None:
        self.zelle = zelle_grad
        self.zellen: dict[tuple[int, int], list[int]] = {}
        self.punkte: list[tuple[float, float]] = []
        self.marken: list[str] = []  # z. B. die Landschaft des Punktes

    def neu(self, laenge: float, breite: float, marke: str = "") -> int:
        i = len(self.punkte)
        self.punkte.append((laenge, breite))
        self.marken.append(marke)
        self.zellen.setdefault((math.floor(laenge / self.zelle), math.floor(breite / self.zelle)), []).append(i)
        return i

    def nahe(self, laenge: float, breite: float, radius_km: float) -> Iterable[int]:
        """Alle Punkte, die höchstens ``radius_km`` entfernt sein können (Obermenge)."""
        db = radius_km / geo.KM_PRO_GRAD
        dl = radius_km / (geo.KM_PRO_GRAD * geo.cos_grad(min(89.0, abs(breite) + db)))
        x0, x1 = math.floor((laenge - dl) / self.zelle), math.floor((laenge + dl) / self.zelle)
        y0, y1 = math.floor((breite - db) / self.zelle), math.floor((breite + db) / self.zelle)
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                yield from self.zellen.get((x, y), ())

    def min_abstand(self, laenge: float, breite: float, radius_km: float) -> float:
        """Kleinster Abstand zu einem Punkt innerhalb ``radius_km`` (sonst unendlich)."""
        beste = math.inf
        for i in self.nahe(laenge, breite, radius_km):
            d = geo.abstand_km(laenge, breite, *self.punkte[i])
            beste = min(beste, d)
        return beste

    def frei(self, laenge: float, breite: float, radius_km: float, marke: str, faktor_fremd: float) -> bool:
        """Kein Punkt näher als ``radius_km`` (bei anderer Marke: ``radius_km`` × ``faktor_fremd``)."""
        for i in self.nahe(laenge, breite, radius_km):
            grenze = radius_km if self.marken[i] == marke else radius_km * faktor_fremd
            if geo.abstand_km(laenge, breite, *self.punkte[i]) < grenze:
                return False
        return True


@dataclass
class Kandidat:
    """Ein Ortskandidat mit allem, was die Auswahl braucht."""

    stadt: staedte.Stadt
    auf_land: bool
    landschaft: str
    bedeutung: float
    rang: int
    laenge_text: str = ""  # Lage auf Kernraum-Land (LAGE_STELLEN Nachkommastellen)
    breite_text: str = ""
    verschoben: bool = False  # Lage auf das nächste Kernraum-Land-Pixel gesetzt

    @property
    def laenge(self) -> float:
        return float(self.laenge_text)

    @property
    def breite(self) -> float:
        return float(self.breite_text)


@dataclass
class Auswahl:
    orte: list[Kandidat]  # in Reihenfolge der Auswahl
    herkunft: dict[int, str]  # geonameid → 'scheibe', 'insel', 'fuellung'
    leere_zellen: int  # Blöcke der Abstandskarte über FUELL_RADIUS_KM ohne passenden Kandidaten


# ─── Rasterhilfen ───────────────────────────────────────────────────────────


@dataclass
class Raster:
    """Kartenraster aus dem Kartengenerator mit Komponenten des Kernraum-Lands."""

    grundlage: kg.Grundlage
    komponente: np.ndarray  # int64 (H, W): Komponente des Kernraum-Lands je Pixel, −1 sonst
    hauptkomponente: int
    komponente_km2: dict[int, float]  # Fläche je Komponente (ungefähr)
    haff: np.ndarray | None = None  # bool (H, W): Binnengewässer, die als Küstengewässer zählen (HAFF_MIN_KM2)

    @property
    def karte(self) -> kr.Karte:
        return self.grundlage.karte

    @property
    def art(self) -> np.ndarray:
        return self.grundlage.raster.art

    def pixel(self, laengen: Sequence[float], breiten: Sequence[float]) -> kr.Lagen:
        return kr.lagen(
            self.grundlage.raster, np.asarray(laengen, dtype=np.float64), np.asarray(breiten, dtype=np.float64)
        )

    def naechstes_land(self, laenge: float, breite: float, max_km: float) -> tuple[int, int] | None:
        """Nächstes Kernraum-Land-Pixel (x, y) zum Punkt, höchstens ``max_km`` entfernt; Gleichstand: Zeilenreihenfolge."""
        karte = self.karte
        fx, fy = karte.nach_pixel(np.array([laenge]), np.array([breite]))
        kx_grad = geo.KM_PRO_GRAD * geo.cos_grad(breite)
        km_x = (karte.laenge_max - karte.laenge_min) / karte.breite * kx_grad
        km_y = (karte.breite_max - karte.breite_min) / karte.hoehe * geo.KM_PRO_GRAD
        rx, ry = int(max_km / km_x) + 2, int(max_km / km_y) + 2
        cx, cy = int(math.floor(fx[0])), int(math.floor(fy[0]))
        x0, x1 = max(0, cx - rx), min(karte.breite, cx + rx + 1)
        y0, y1 = max(0, cy - ry), min(karte.hoehe, cy + ry + 1)
        if x0 >= x1 or y0 >= y1:
            return None
        fenster = self.art[y0:y1, x0:x1] == kr.KERNLAND
        if not fenster.any():
            return None
        ys, xs = np.nonzero(fenster)
        dx = (xs + x0 + 0.5 - fx[0]) * km_x
        dy = (ys + y0 + 0.5 - fy[0]) * km_y
        d2 = dx * dx + dy * dy
        k = int(np.argmin(d2))  # erstes Minimum = Zeilenreihenfolge
        if d2[k] > max_km * max_km:
            return None
        return int(xs[k] + x0), int(ys[k] + y0)


def lade_raster() -> Raster:
    grundlage = kg.lade_grundlage(DATEN, fluesse=False)
    maske = (grundlage.raster.art == kr.KERNLAND).astype(np.int8)
    laeufe = kr.laeufe(maske)
    aktiv = laeufe.wert == 1
    komp = kr.komponenten(maske, laeufe, aktiv)
    komponente = komp[laeufe.kennung].astype(np.int64)
    belegt = komponente[komponente >= 0]
    zahl = np.bincount(belegt)
    haupt = int(zahl.argmax())
    pixel_km2 = (grundlage.karte.laenge_max - grundlage.karte.laenge_min) / grundlage.karte.breite * geo.KM_PRO_GRAD
    pixel_km2 *= (grundlage.karte.breite_max - grundlage.karte.breite_min) / grundlage.karte.hoehe * geo.KM_PRO_GRAD
    pixel_km2 *= geo.cos_grad(grundlage.karte.bezugsbreite)
    flaechen = {int(k): float(z) * pixel_km2 for k, z in enumerate(zahl) if z > 0}
    return Raster(grundlage, komponente, haupt, flaechen, haffe(grundlage.raster.art, pixel_km2))


def haffe(art: np.ndarray, pixel_km2: float, min_km2: float = HAFF_MIN_KM2) -> np.ndarray:
    """Maske der Binnengewässer ab ``min_km2``, die (auch diagonal) an ein Meerespixel grenzen."""
    see = (art == kr.BINNENGEWAESSER).astype(np.int8)
    laeufe = kr.laeufe(see)
    komp = kr.komponenten(see, laeufe, laeufe.wert == 1)[laeufe.kennung].astype(np.int64)
    meer = art == kr.MEER
    am_meer = np.zeros_like(meer)
    h, b = art.shape
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            am_meer[max(0, -dy) : h - max(0, dy), max(0, -dx) : b - max(0, dx)] |= meer[
                max(0, dy) : h - max(0, -dy), max(0, dx) : b - max(0, -dx)
            ]
    if not (komp >= 0).any():
        return np.zeros_like(meer)
    zahl = np.bincount(komp[komp >= 0])
    gross = zahl * pixel_km2 >= min_km2
    beruehrt = np.zeros(zahl.size, dtype=bool)
    beruehrt[np.unique(komp[(komp >= 0) & am_meer])] = True
    return (komp >= 0) & (gross & beruehrt)[np.maximum(komp, 0)]


def haff_km(raster: Raster, laenge: float, breite: float, max_km: float) -> float | None:
    """Abstand zum nächsten Haff-Pixel (Pixelmitte) in km; None, wenn keines bis ``max_km``."""
    if raster.haff is None:
        return None
    karte = raster.karte
    fx, fy = karte.nach_pixel(np.array([laenge]), np.array([breite]))
    km_x = (karte.laenge_max - karte.laenge_min) / karte.breite * geo.KM_PRO_GRAD * geo.cos_grad(breite)
    km_y = (karte.breite_max - karte.breite_min) / karte.hoehe * geo.KM_PRO_GRAD
    rx, ry = int(max_km / km_x) + 2, int(max_km / km_y) + 2
    cx, cy = int(math.floor(fx[0])), int(math.floor(fy[0]))
    x0, x1 = max(0, cx - rx), min(karte.breite, cx + rx + 1)
    y0, y1 = max(0, cy - ry), min(karte.hoehe, cy + ry + 1)
    if x0 >= x1 or y0 >= y1:
        return None
    ys, xs = np.nonzero(raster.haff[y0:y1, x0:x1])
    if ys.size == 0:
        return None
    dx = (xs + x0 + 0.5 - fx[0]) * km_x
    dy = (ys + y0 + 0.5 - fy[0]) * km_y
    d = float(np.sqrt((dx * dx + dy * dy).min()))
    return d if d <= max_km else None


class Abstandskarte:
    """Abstand jedes Blocks (FUELL_BLOCK × FUELL_BLOCK Pixel) mit überwiegend Kernraum-Land zum nächsten Ort."""

    def __init__(self, raster: Raster, block: int = FUELL_BLOCK) -> None:
        karte = raster.karte
        h, b = raster.art.shape
        self.h2, self.b2 = h // block, b // block
        land = (raster.art[: self.h2 * block, : self.b2 * block] == kr.KERNLAND).reshape(self.h2, block, self.b2, block)
        self.land = land.sum(axis=(1, 3)) * 2 >= block * block
        xs = (np.arange(self.b2, dtype=np.float64) + 0.5) * block
        ys = (np.arange(self.h2, dtype=np.float64) + 0.5) * block
        self.laengen = karte.laenge_min + xs / b * (karte.laenge_max - karte.laenge_min)
        self.breiten = karte.breite_max - ys / h * (karte.breite_max - karte.breite_min)
        self.kx = np.array([geo.KM_PRO_GRAD * geo.cos_grad(float(phi)) for phi in self.breiten])
        self.grad_x = (karte.laenge_max - karte.laenge_min) / self.b2
        self.grad_y = (karte.breite_max - karte.breite_min) / self.h2
        self.karte = karte
        self.abstand = np.full((self.h2, self.b2), np.inf)

    def setze(self, laenge: float, breite: float, radius_km: float = FUELL_KARTE_KM) -> None:
        """Berücksichtigt einen Ort (bis ``radius_km`` um ihn herum)."""
        karte = self.karte
        cx = (laenge - karte.laenge_min) / self.grad_x
        cy = (karte.breite_max - breite) / self.grad_y
        rx = int(radius_km / (geo.KM_PRO_GRAD * 0.5) / self.grad_x) + 2
        ry = int(radius_km / geo.KM_PRO_GRAD / self.grad_y) + 2
        x0, x1 = max(0, int(cx) - rx), min(self.b2, int(cx) + rx + 1)
        y0, y1 = max(0, int(cy) - ry), min(self.h2, int(cy) + ry + 1)
        if x0 >= x1 or y0 >= y1:
            return
        dx = (self.laengen[None, x0:x1] - laenge) * self.kx[y0:y1, None]
        dy = (self.breiten[y0:y1, None] - breite) * geo.KM_PRO_GRAD
        d = np.sqrt(dx * dx + dy * dy)
        np.minimum(self.abstand[y0:y1, x0:x1], d, out=self.abstand[y0:y1, x0:x1])


def setze_lagen(kandidaten: Sequence[Kandidat], raster: Raster) -> tuple[list[Kandidat], list[Kandidat]]:
    """Lage jedes Kandidaten auf Kernraum-Land (Text mit LAGE_STELLEN Nachkommastellen).

    Liegt der gerundete GeoNames-Punkt nicht auf Kernraum-Land (Küsten- und Inselorte knapp im Wasser,
    Orte am Rand des Umrisses), gilt die Mitte des nächsten Kernraum-Land-Pixels höchstens
    LAND_SUCHE_KM entfernt. Kandidaten ohne solches Pixel entfallen (zweite Liste).
    """
    texte = [
        (geo.zahl_text(k.stadt.laenge, LAGE_STELLEN), geo.zahl_text(k.stadt.breite, LAGE_STELLEN)) for k in kandidaten
    ]
    lage = raster.pixel([float(a) for a, _ in texte], [float(b) for _, b in texte])
    gut, ohne = [], []
    for i, k in enumerate(kandidaten):
        lt, bt = texte[i]
        verschoben = False
        if not (lage.innen[i] and lage.art[i] == kr.KERNLAND):
            pixel = raster.naechstes_land(k.stadt.laenge, k.stadt.breite, LAND_SUCHE_KM)
            if pixel is None:
                ohne.append(k)
                continue
            laenge, breite = raster.karte.pixelmitte(*pixel)
            lt, bt = geo.zahl_text(laenge, LAGE_STELLEN), geo.zahl_text(breite, LAGE_STELLEN)
            neu = raster.pixel([float(lt)], [float(bt)])
            if (int(neu.px[0]), int(neu.py[0])) != pixel:
                raise OrteFehler(f"{k.stadt.name}: gerundete Lage verlässt das Landpixel")
            verschoben = True
        k.laenge_text, k.breite_text, k.verschoben = lt, bt, verschoben
        gut.append(k)
    return gut, ohne


def waehle_orte(
    kandidaten: Sequence[Kandidat],
    feste: Sequence[tuple[float, float, str]],
    raster: Raster,
    landschaften: dict[str, Landschaft],
    grundabstand_km: float = GRUNDABSTAND_KM,
) -> Auswahl:
    """Gewichtete Poisson-Scheibe, Inseln, Lückenfüllung (siehe Moduldokumentation).

    ``kandidaten`` in absteigender Bedeutung; ``feste`` sind Lage und Landschaft der M1-Orte (immer
    gewählt). Zu Orten einer anderen Landschaft genügt GRENZ_FAKTOR × Mindestabstand: Landschaftsgrenzen
    sind oft historische Grenzen mit Orten auf beiden Seiten (Selb und Asch, Mainz und Wiesbaden).
    """
    gitter = Punktgitter()
    for laenge, breite, landschaft in feste:
        gitter.neu(laenge, breite, landschaft)
    gewaehlt: list[Kandidat] = []
    herkunft: dict[int, str] = {}
    gewaehlt_ids: set[int] = set()

    # 1. Poisson-Scheibe
    for k in kandidaten:
        s = k.stadt
        radius = grundabstand_km * abstandsfaktor(k.bedeutung) * landschaften[k.landschaft].abstand
        if not gitter.frei(k.laenge, k.breite, radius, k.landschaft, GRENZ_FAKTOR):
            continue
        gitter.neu(k.laenge, k.breite, k.landschaft)
        gewaehlt.append(k)
        herkunft[s.geonameid] = "scheibe"
        gewaehlt_ids.add(s.geonameid)

    # 2. Inseln (abgetrennte Teile des Kernraum-Lands) ohne Ort: wichtigster Kandidat darauf.
    def komponente_von(laenge: float, breite: float) -> int:
        lage = raster.pixel([laenge], [breite])
        return int(raster.komponente[lage.py[0], lage.px[0]]) if lage.innen[0] else -1

    belegt = {komponente_von(lg, br) for lg, br in gitter.punkte}
    for k in kandidaten:
        if k.stadt.geonameid in gewaehlt_ids:
            continue
        komp = komponente_von(k.laenge, k.breite)
        if komp < 0 or komp in belegt:
            continue
        belegt.add(komp)
        gitter.neu(k.laenge, k.breite, k.landschaft)
        gewaehlt.append(k)
        herkunft[k.stadt.geonameid] = "insel"
        gewaehlt_ids.add(k.stadt.geonameid)

    # 3. Lückenfüllung über die Abstandskarte (immer die größte Lücke zuerst).
    karte = Abstandskarte(raster)
    for laenge, breite in gitter.punkte:
        karte.setze(laenge, breite)
    kgitter = Punktgitter(0.5)
    for k in kandidaten:
        kgitter.neu(k.laenge, k.breite)
    offen = np.where(karte.land, karte.abstand, -1.0)
    leer = 0
    while True:
        i = int(np.argmax(offen))
        y, x = divmod(i, karte.b2)
        dmax = float(offen[y, x])
        if dmax <= FUELL_RADIUS_KM:
            break
        l0, b0 = float(karte.laengen[x]), float(karte.breiten[y])
        beste: tuple[tuple[float, float, int], Kandidat] | None = None
        # dmax ist unendlich, wenn kein Ort bis FUELL_KARTE_KM entfernt liegt: Suche begrenzen.
        for j in kgitter.nahe(l0, b0, min(dmax, FUELL_SUCHE_MAX_KM)):
            k = kandidaten[j]
            s = k.stadt
            if s.geonameid in gewaehlt_ids:
                continue
            d = geo.abstand_km(l0, b0, k.laenge, k.breite)
            if d >= dmax or gitter.min_abstand(k.laenge, k.breite, FUELL_MIN_ABSTAND_KM) < FUELL_MIN_ABSTAND_KM:
                continue
            schluessel = (d, -k.bedeutung, s.geonameid)
            if beste is None or schluessel < beste[0]:
                beste = (schluessel, k)
        if beste is None:
            offen[y, x] = -2.0
            leer += 1
            continue
        k = beste[1]
        s = k.stadt
        gitter.neu(k.laenge, k.breite, k.landschaft)
        gewaehlt.append(k)
        herkunft[s.geonameid] = "fuellung"
        gewaehlt_ids.add(s.geonameid)
        karte.setze(k.laenge, k.breite)
        offen = np.where(offen >= 0.0, np.minimum(offen, karte.abstand), offen)
    return Auswahl(gewaehlt, herkunft, leer)


# ─── Gelände ────────────────────────────────────────────────────────────────


@dataclass(frozen=True)
class Gelaendemerkmale:
    """Lagemerkmale eines Ortes (Höhen in m, gerundet auf 0,1 m; Abstände in km)."""

    mittel: float
    p05: float
    p95: float
    kueste_km: float | None
    fluss_km: float | None
    seeanteil: float
    insel: bool
    gebiet: str = ""  # Geländetyp aus gelaende_gebiete.csv, falls der Ort in einem solchen Gebiet liegt

    @property
    def relief(self) -> float:
        return self.p95 - self.p05


def gelaende_aus(m: Gelaendemerkmale) -> str:
    """Geländetyp nach den dokumentierten Regeln (README, Abschnitt Gelände). Reihenfolge = Vorrang.

    1. kueste: Insel bis INSEL_MAX_KM2 oder nahe am Meer und niedrig
    2. hochgebirge: Hochgebirge im Umkreis (p95) oder hohes Mittel
    3. mittelgebirge: starkes Relief oder Hochfläche mit merklichem Relief
    4. boerde/heide: in einem Gebiet aus gelaende_gebiete.csv und ohne starkes Relief
    5. flussaue: nahe einem großen Fluss, flach und überwiegend auf Höhe der Talsohle
    6. seenplatte: hoher Seenanteil im Tiefland
    7. huegelland: merkliches Relief oder Hochfläche mit etwas Relief
    8. flachland: sonst (auch flache Hochebenen wie die Münchner Schotterebene)
    """
    if m.insel or (m.kueste_km is not None and m.kueste_km <= KUESTE_KM and m.mittel <= KUESTE_MITTEL_M):
        return "kueste"
    if m.p95 >= HOCHGEBIRGE_P95_M or m.mittel >= HOCHGEBIRGE_MITTEL_M:
        return "hochgebirge"
    if m.relief >= MITTELGEBIRGE_RELIEF_M or (
        m.mittel >= MITTELGEBIRGE_MITTEL_M and m.relief >= MITTELGEBIRGE_HOCHFLAECHE_RELIEF_M
    ):
        return "mittelgebirge"
    if m.gebiet and m.relief < GEBIET_MAX_RELIEF_M:
        return m.gebiet
    if (
        m.fluss_km is not None
        and m.fluss_km <= FLUSSAUE_FLUSS_KM
        and m.mittel - m.p05 <= FLUSSAUE_UEBER_TAL_M
        and m.relief < FLUSSAUE_MAX_RELIEF_M
    ):
        return "flussaue"
    if m.seeanteil >= SEENPLATTE_ANTEIL and m.mittel < SEENPLATTE_MAX_MITTEL_M:
        return "seenplatte"
    if m.relief >= HUEGELLAND_RELIEF_M or (
        m.mittel >= HUEGELLAND_MITTEL_M and m.relief >= HUEGELLAND_HOCHFLAECHE_RELIEF_M
    ):
        return "huegelland"
    return "flachland"


@dataclass(frozen=True)
class Gelaendegebiet:
    """Eine Zeile aus gelaende_gebiete.csv (Kreisnäherung eines benannten Gebiets)."""

    gelaende: str
    name: str
    laenge: float
    breite: float
    radius_km: float


def lade_gelaende_gebiete(pfad: Path, bekannte: set[str] | None = None) -> list[Gelaendegebiet]:
    gebiete = []
    for z in lies_csv(pfad, ("gelaende", "name", "laenge", "breite", "radius_km", "beleg")):
        if z["gelaende"] not in ("boerde", "heide") or not z["beleg"]:
            raise OrteFehler(f"{pfad.name}: {z['name']}: Gelände muss boerde oder heide sein, Beleg ist Pflicht")
        if bekannte is not None and z["gelaende"] not in bekannte:
            raise OrteFehler(f"{pfad.name}: Gelände {z['gelaende']} fehlt in data/common/gelaende.chron")
        try:
            gebiete.append(
                Gelaendegebiet(z["gelaende"], z["name"], float(z["laenge"]), float(z["breite"]), float(z["radius_km"]))
            )
        except ValueError:
            raise OrteFehler(f"{pfad.name}: {z['name']}: Lage oder Radius ist keine Zahl") from None
    return gebiete


def gebiet_bei(gebiete: Sequence[Gelaendegebiet], laenge: float, breite: float) -> str:
    """Geländetyp des nächstgelegenen Gebiets, in dessen Radius der Punkt liegt (sonst leer)."""
    beste: tuple[float, str] | None = None
    for g in gebiete:
        d = geo.abstand_km(laenge, breite, g.laenge, g.breite)
        if d <= g.radius_km and (beste is None or d / g.radius_km < beste[0]):
            beste = (d / g.radius_km, g.gelaende)
    return beste[1] if beste else ""


def hoehenwerte(modell: hoehen.Hoehenmodell, maske: Any, laenge: float, breite: float, radius_km: float) -> np.ndarray:
    """Höhen der Landpixel des Höhenmodells im Umkreis (Auswahl wie ``Hoehenmodell.statistik``)."""
    dbreite = radius_km / geo.KM_PRO_GRAD
    dlaenge = radius_km / (geo.KM_PRO_GRAD * geo.cos_grad(min(89.0, abs(breite) + dbreite)))
    s_a, z_b = modell._mosaik_koordinaten([laenge - dlaenge, laenge + dlaenge], [breite + dbreite, breite - dbreite])
    s0 = max(0, int(math.floor(s_a[0])))
    s1 = min(modell.spalten, int(math.ceil(s_a[1])) + 1)
    z0 = max(0, int(math.floor(z_b[0])))
    z1 = min(modell.zeilen, int(math.ceil(z_b[1])) + 1)
    werte = np.empty(0, dtype=np.float64)
    if s1 > s0 and z1 > z0:
        spalten = np.arange(s0, s1, dtype=np.float64) + 0.5 + modell.x0 * hoehen.KACHELGROESSE
        zeilen = np.arange(z0, z1, dtype=np.float64) + 0.5 + modell.y0 * hoehen.KACHELGROESSE
        laengen, _ = hoehen.mercator_geo(spalten, np.zeros_like(spalten), modell.zoom)
        _, breiten = hoehen.mercator_geo(np.zeros_like(zeilen), zeilen, modell.zoom)
        gl, gb = np.meshgrid(laengen, breiten)
        faktor = geo.cos_grad(breite)
        dx = (gl - laenge) * geo.KM_PRO_GRAD * faktor
        dy = (gb - breite) * geo.KM_PRO_GRAD
        auswahl = (dx * dx + dy * dy <= radius_km * radius_km) & np.asarray(maske(gl, gb), dtype=bool)
        werte = modell.hoehen[z0:z1, s0:s1][auswahl].astype(np.float64)
    if werte.size == 0:
        werte = np.array([modell.hoehe(laenge, breite)])
    return werte


def seeanteil(raster: Raster, laenge: float, breite: float, radius_km: float = SEENPLATTE_RADIUS_KM) -> float:
    """Anteil der Binnengewässer (ohne Haffe) an Land und Seen im Umkreis (Kartenpixel)."""
    karte = raster.karte
    lage = raster.pixel([laenge], [breite])
    if not lage.innen[0]:
        return 0.0
    cx, cy = int(lage.px[0]), int(lage.py[0])
    km_x = (karte.laenge_max - karte.laenge_min) / karte.breite * geo.KM_PRO_GRAD * geo.cos_grad(breite)
    km_y = (karte.breite_max - karte.breite_min) / karte.hoehe * geo.KM_PRO_GRAD
    rx, ry = int(radius_km / km_x) + 1, int(radius_km / km_y) + 1
    x0, x1 = max(0, cx - rx), min(karte.breite, cx + rx + 1)
    y0, y1 = max(0, cy - ry), min(karte.hoehe, cy + ry + 1)
    art = raster.art[y0:y1, x0:x1]
    dx = (np.arange(x0, x1) - cx) * km_x
    dy = (np.arange(y0, y1) - cy) * km_y
    kreis = dx[None, :] ** 2 + dy[:, None] ** 2 <= radius_km * radius_km
    see_maske = art == kr.BINNENGEWAESSER
    if raster.haff is not None:
        see_maske = see_maske & ~raster.haff[y0:y1, x0:x1]
    see = int((see_maske & kreis).sum())
    land = int((((art == kr.KERNLAND) | (art == kr.AUSLAND)) & kreis).sum())
    return see / (see + land) if see + land else 0.0


# ─── Kreise ─────────────────────────────────────────────────────────────────


def _zusammenhaengend(knoten: Sequence[int], kanten: dict[int, list[tuple[int, float]]]) -> list[list[int]]:
    """Zusammenhangskomponenten (in Reihenfolge des ersten Knotens)."""
    menge = set(knoten)
    gesehen: set[int] = set()
    teile = []
    for start in knoten:
        if start in gesehen:
            continue
        teil = []
        stapel = [start]
        gesehen.add(start)
        while stapel:
            k = stapel.pop()
            teil.append(k)
            for n, _ in kanten.get(k, ()):
                if n in menge and n not in gesehen:
                    gesehen.add(n)
                    stapel.append(n)
        teile.append(sorted(teil))
    return teile


def exklaven_zuordnen(landschaften: Sequence[str], nachbarn: dict[tuple[int, int], int]) -> list[str]:
    """Landschaft je Ort für die Kreisbildung.

    Ein Teil einer Landschaft ohne Landverbindung zu ihrem Rest mit weniger als KREIS_MIN Orten
    (eine Exklave, etwa Jüterbog als magdeburgisches Amt mitten in Brandenburg) bildet Kreise mit der
    Landschaft, mit der er die längste Grenze teilt. Inseln ohne Landnachbarn bleiben in ihrer
    Landschaft; ebenso Teile einer Landschaft, die keinen Teil mit mindestens KREIS_MIN Orten hat.
    """
    ergebnis = list(landschaften)
    kanten: dict[int, list[tuple[int, float]]] = {i: [] for i in range(len(landschaften))}
    for (a, b), laenge in sorted(nachbarn.items()):
        if landschaften[a] == landschaften[b]:
            kanten[a].append((b, 0.0))
            kanten[b].append((a, 0.0))
    for landschaft in sorted(set(landschaften)):
        knoten = [i for i, l in enumerate(landschaften) if l == landschaft]
        teile = _zusammenhaengend(knoten, kanten)
        if len(teile) < 2 or max(len(t) for t in teile) < KREIS_MIN:
            continue
        for teil in teile:
            if len(teil) >= KREIS_MIN:
                continue
            menge = set(teil)
            grenze: dict[str, int] = {}
            for (a, b), laenge in nachbarn.items():
                if (a in menge) != (b in menge):
                    anderer = b if a in menge else a
                    if landschaften[anderer] != landschaft:
                        grenze[landschaften[anderer]] = grenze.get(landschaften[anderer], 0) + laenge
            if grenze:
                ziel = max(sorted(grenze), key=lambda l: grenze[l])
                for i in teil:
                    ergebnis[i] = ziel
    return ergebnis


def _zweiteilen(
    gruppe: Sequence[int], kanten: dict[int, list[tuple[int, float]]], xy: dict[int, tuple[float, float]]
) -> list[list[int]] | None:
    """Teilt eine zusammenhängende Gruppe in zwei zusammenhängende Teile mit je KREIS_MIN bis KREIS_MAX Orten.

    Probiert jeden Ort als Keim: Der Teil wächst um den jeweils nächsten Nachbarort (zum Schwerpunkt
    des Teils); jede Größe, bei der auch der Rest zusammenhängt, ist ein Kandidat. Gewählt wird die
    Teilung mit der kleinsten Summe der quadrierten Abstände zu den Schwerpunkten (dann Keim, Größe).
    """
    menge = set(gruppe)

    def streuung(teil: Sequence[int]) -> float:
        mx = sum(xy[k][0] for k in teil) / len(teil)
        my = sum(xy[k][1] for k in teil) / len(teil)
        return sum((xy[k][0] - mx) ** 2 + (xy[k][1] - my) ** 2 for k in teil)

    beste: tuple[float, int, int, list[int]] | None = None
    for keim in sorted(gruppe):
        teil = [keim]
        drin = {keim}
        while len(teil) < len(gruppe) - KREIS_MIN:
            mx = sum(xy[k][0] for k in teil) / len(teil)
            my = sum(xy[k][1] for k in teil) / len(teil)
            rand = sorted({b for k in teil for b, _ in kanten[k] if b in menge and b not in drin})
            if not rand:
                break
            neu = min(rand, key=lambda b: ((xy[b][0] - mx) ** 2 + (xy[b][1] - my) ** 2, b))
            teil.append(neu)
            drin.add(neu)
            rest = sorted(menge - drin)
            if KREIS_MIN <= len(teil) <= KREIS_MAX and KREIS_MIN <= len(rest) <= KREIS_MAX:
                if len(_zusammenhaengend(rest, kanten)) == 1:
                    wert = streuung(teil) + streuung(rest)
                    kandidat = (wert, keim, len(teil), sorted(teil))
                    if beste is None or kandidat[:3] < beste[:3]:
                        beste = kandidat
    if beste is None:
        return None
    teil = beste[3]
    return sorted([teil, sorted(menge - set(teil))], key=lambda g: g[0])


def kreise_einer_landschaft(
    knoten: Sequence[int],
    nachbarn: Iterable[tuple[int, int]],
    lagen: Sequence[tuple[float, float]],
) -> list[list[int]]:
    """Teilt die Orte einer Landschaft in zusammenhängende Gruppen von KREIS_MIN bis KREIS_MAX Orten.

    ``knoten``: Indizes der Orte, ``nachbarn``: Paare benachbarter Orte (gemeinsame Grenze auf der
    Karte), ``lagen``: (Länge, Breite) für alle Indizes. Teile ohne Landverbindung (Inseln,
    abgetrennte Stücke) werden über die kürzeste Luftlinie mit dem Rest verbunden.

    Verfahren: Ward-Agglomeration auf dem Nachbarschaftsgraphen. Anfangs ist jeder Ort eine Gruppe;
    es verschmelzen immer die zwei benachbarten Gruppen mit den geringsten Kosten
    n_a·n_b/(n_a+n_b)·|Schwerpunkt_a − Schwerpunkt_b|² (km²), solange die neue Gruppe höchstens
    KREIS_MAX Orte hat – bis die Zielzahl round(n / KREIS_ZIEL) erreicht ist; danach nur noch
    Gruppen mit weniger als KREIS_MIN Orten. Benachbarte Gruppen bleiben beim Verschmelzen
    zusammenhängend. Gleichstand: kleinere Gruppenkennung. Ergebnis: Gruppen (aufsteigend sortierte
    Indizes), nach ihrem kleinsten Index sortiert.
    """
    knoten = sorted(knoten)
    n = len(knoten)
    if n < KREIS_MIN:
        raise OrteFehler(f"Landschaft mit nur {n} Orten: kein Kreis mit mindestens {KREIS_MIN} Orten möglich")
    menge = set(knoten)

    def km(a: int, b: int) -> float:
        return geo.abstand_km(*lagen[a], *lagen[b])

    kanten: dict[int, list[tuple[int, float]]] = {k: [] for k in knoten}
    for a, b in nachbarn:
        if a in menge and b in menge and a != b:
            kanten[a].append((b, 0.0))
            kanten[b].append((a, 0.0))
    # Teile ohne Verbindung über die kürzeste Luftlinie anbinden (wie Kruskal über die Teile).
    teile = _zusammenhaengend(knoten, kanten)
    while len(teile) > 1:
        beste: tuple[float, int, int] | None = None
        for a in teile[0]:
            for teil in teile[1:]:
                for b in teil:
                    d = km(a, b)
                    if beste is None or (d, a, b) < beste:
                        beste = (d, a, b)
        assert beste is not None
        _, a, b = beste
        kanten[a].append((b, 0.0))
        kanten[b].append((a, 0.0))
        teile = _zusammenhaengend(knoten, kanten)

    # Ebene Koordinaten in km (Plattkarte um die mittlere Breite der Landschaft).
    breite0 = sum(lagen[k][1] for k in knoten) / n
    kx = geo.KM_PRO_GRAD * geo.cos_grad(breite0)
    xy = {k: (lagen[k][0] * kx, lagen[k][1] * geo.KM_PRO_GRAD) for k in knoten}

    # Gruppen: Kennung = kleinster Ort; Größe, Summe der Koordinaten, Nachbargruppen.
    glieder: dict[int, list[int]] = {k: [k] for k in knoten}
    summe: dict[int, tuple[float, float]] = {k: xy[k] for k in knoten}
    nachbar: dict[int, set[int]] = {k: {b for b, _ in kanten[k]} for k in knoten}

    def kosten(a: int, b: int) -> float:
        na, nb = len(glieder[a]), len(glieder[b])
        ax, ay = summe[a][0] / na, summe[a][1] / na
        bx, by = summe[b][0] / nb, summe[b][1] / nb
        return na * nb / (na + nb) * ((ax - bx) ** 2 + (ay - by) ** 2)

    ziel = max(1, round(n / KREIS_ZIEL))
    while n / ziel > KREIS_MAX:
        ziel += 1

    def naechste_verschmelzung(nur_kleine: bool) -> tuple[float, int, int] | None:
        beste: tuple[float, int, int] | None = None
        for a in sorted(glieder):
            for b in sorted(nachbar[a]):
                if b <= a or len(glieder[a]) + len(glieder[b]) > KREIS_MAX:
                    continue
                if nur_kleine and len(glieder[a]) >= KREIS_MIN and len(glieder[b]) >= KREIS_MIN:
                    continue
                kandidat = (kosten(a, b), a, b)
                if beste is None or kandidat < beste:
                    beste = kandidat
        return beste

    while True:
        nur_kleine = len(glieder) <= ziel
        if nur_kleine and all(len(g) >= KREIS_MIN for g in glieder.values()):
            break
        wahl = naechste_verschmelzung(nur_kleine)
        if wahl is None:
            break
        _, a, b = wahl
        glieder[a] = sorted(glieder[a] + glieder.pop(b))
        sa, sb = summe[a], summe.pop(b)
        summe[a] = (sa[0] + sb[0], sa[1] + sb[1])
        nachbar[a] = (nachbar[a] | nachbar.pop(b)) - {a, b}
        for c in nachbar[a]:
            nachbar[c].discard(b)
            nachbar[c].add(a)
    gruppen = sorted(glieder.values(), key=lambda g: g[0])

    # Reparatur: Eine zu kleine Gruppe, deren Nachbarn alle voll sind, wird mit der kleinsten
    # Nachbargruppe verschmolzen und das Ganze in zwei zusammenhängende Teile geteilt.
    for _ in range(n):
        klein = [g for g in gruppen if len(g) < KREIS_MIN]
        if not klein:
            break
        a = min(klein, key=lambda g: (len(g), g[0]))
        in_a = set(a)
        nachbarn_a = sorted(
            (g for g in gruppen if g is not a and any(b in in_a for k in g for b, _ in kanten[k])),
            key=lambda g: (len(g), g[0]),
        )
        if not nachbarn_a:
            raise OrteFehler(f"Kreisbildung: Gruppe {a[:3]}… mit {len(a)} Orten hat keine Nachbarn")
        b = nachbarn_a[0]
        teile = _zweiteilen(sorted(a + b), kanten, xy)
        if teile is None:
            raise OrteFehler(f"Kreisbildung: Gruppe {sorted(a + b)[:3]}… lässt sich nicht teilen")
        gruppen = sorted([g for g in gruppen if g is not a and g is not b] + teile, key=lambda g: g[0])
    for g in gruppen:
        if not KREIS_MIN <= len(g) <= KREIS_MAX or len(_zusammenhaengend(g, kanten)) != 1:
            raise OrteFehler(
                f"Kreisbildung: Gruppe {g[:3]}… mit {len(g)} Orten ist zu klein, zu groß oder nicht zusammenhängend"
            )
    return gruppen


# ─── Zusammenbau ────────────────────────────────────────────────────────────


@dataclass
class Ort:
    """Ein Ort in orte.csv (M1-Ort oder neuer Ort)."""

    kennung: str
    datei: str
    name: str
    gelaende: str
    laenge: str  # Text wie in der Tabelle
    breite: str
    stadt: str
    quelle: str
    m1: bool
    groesse: float  # heutige Einwohner (Hauptort eines Kreises), M1: aus der GeoNames-Entsprechung
    rangfolge: tuple[Any, ...]  # Sortierschlüssel innerhalb der Landschaft
    kreis: str = ""
    flaeche: str = ""
    geonameid: int = 0
    gelaende_merkmale: Gelaendemerkmale | None = None
    zeile_m1: dict[str, str] = field(default_factory=dict)


def _namen_von(stadt: staedte.Stadt) -> set[str]:
    return {" ".join(n.casefold().replace("-", " ").split()) for n in stadt.alle_namen()}


def m1_entsprechung(
    m1_name: str, laenge: float, breite: float, kandidaten: Sequence[staedte.Stadt]
) -> staedte.Stadt | None:
    """GeoNames-Eintrag desselben Ortes: höchstens M1_ABSTAND_KM entfernt und gleicher Name.

    Gleicher Name: ein Name des Eintrags (auch Alternativnamen, ohne Groß-/Kleinschreibung, Bindestrich
    = Leerzeichen) ist gleich dem M1-Namen ohne Klammerzusatz oder dessen Anfang bis zu einem
    Leerzeichen („Stargard in Pommern“ ↔ „Stargard“, „Berlin-Cölln“ ↔ „Berlin“, „Küstrin“ ↔ „Küstrin an
    der Oder“). Bei mehreren der mit den meisten Einwohnern.
    """
    basis = " ".join(m1_name.split(" (")[0].casefold().replace("-", " ").split())
    beste: staedte.Stadt | None = None
    for s in kandidaten:
        if abs(s.breite - breite) > 0.1 or abs(s.laenge - laenge) > 0.15:
            continue
        if geo.abstand_km(laenge, breite, s.laenge, s.breite) > M1_ABSTAND_KM:
            continue
        gleich = False
        for n in _namen_von(s):
            if n == basis or basis.startswith(n + " ") or n.startswith(basis + " "):
                gleich = True
                break
        if gleich and (beste is None or (s.einwohner, -s.geonameid) > (beste.einwohner, -beste.geonameid)):
            beste = s
    return beste


@dataclass
class Ergebnis:
    orte: list[Ort]  # Reihenfolge von orte.csv
    kreise: list[tuple[str, str, str]]  # (Kennung, Landschaft, Name) in Reihenfolge von kreise.csv
    texte: dict[str, str]  # Dateiname → Inhalt (orte.csv, kreise.csv, dateien.csv)
    bericht: list[str]
    karte: kg.Ergebnis


def _flusskennungen() -> list[tuple[str, list[geo.Linie]]]:
    """Benannte Flüsse aus fluesse.geojson: (Kennung, Linien)."""
    daten = geo.lade_geojson(geodaten.GEOMETRIE_ORDNER / "fluesse.geojson")
    nach_name: dict[str, list[geo.Linie]] = {}
    for f in daten.get("features") or []:
        name = (f.get("properties") or {}).get("name") or ""
        if not name:
            continue
        nach_name.setdefault(kennung_aus_name(name), []).extend(geo.geometrie_linien(f.get("geometry")))
    return sorted(nach_name.items())


def erzeuge(melde: Any = print) -> Ergebnis:
    """Rechnet alles und gibt die Tabellentexte zurück (schreibt nichts)."""
    uhr = time.perf_counter()
    tabellen = lade_hilfstabellen()
    bericht: list[str] = []

    # M1-Orte und vorhandene Tabellen
    orte_alt = lies_csv(TABELLEN / "orte.csv", ORTE_SPALTEN)
    m1_zeilen = [z for z in orte_alt if not z["quelle"].startswith(QUELLE_PRAEFIX)]
    dateien_alt = lies_csv(TABELLEN / "dateien.csv", DATEIEN_SPALTEN)
    m1_dateien = {z["datei"] for z in m1_zeilen}
    for d in sorted(m1_dateien):
        if d not in tabellen.landschaften:
            raise OrteFehler(f"Datei {d!r} der M1-Orte fehlt in landschaften_dateien.csv")

    # Kandidaten
    alle = setze_einwohner(staedte.kandidaten(), tabellen.einwohner)
    staedte_nach_id = {k.stadt.geonameid: k.stadt for k in alle}
    fehler = pruefe_exonyme(tabellen.exonyme, staedte_nach_id)
    if fehler:
        raise OrteFehler("\n".join(fehler))
    siedlungen = [k for k in alle if k.stadt.merkmalscode in SIEDLUNGSCODES]
    m1_entsprechungen: dict[str, staedte.Stadt] = {}
    for z in m1_zeilen:
        treffer = m1_entsprechung(z["name"], float(z["laenge"]), float(z["breite"]), [k.stadt for k in siedlungen])
        if treffer is None:
            raise OrteFehler(f"M1-Ort {z['id']}: keine GeoNames-Entsprechung im Umkreis von {M1_ABSTAND_KM:g} km")
        m1_entsprechungen[z["id"]] = treffer
    m1_ids = {s.geonameid for s in m1_entsprechungen.values()}
    ausgeschlossen = 0
    kandidaten: list[Kandidat] = []
    ohne_regel: list[staedte.Kandidat] = []
    for k in siedlungen:
        s = k.stadt
        if s.geonameid in m1_ids:
            continue
        if s.geonameid in tabellen.ausschluesse:
            ausgeschlossen += 1
            continue
        landschaft = landschaft_nach_regeln(tabellen.regeln, s.staat, (s.admin1, s.admin2, s.admin3, s.admin4))
        if landschaft is None:
            ohne_regel.append(k)
            continue
        kandidaten.append(Kandidat(s, k.auf_land, landschaft, bedeutung(s), verwaltungsrang(s)))
    # Ohne passende Regel (Fehler in den GeoNames-Codes): Landschaft des nächsten zugeordneten Kandidaten.
    zugeordnet = Punktgitter(0.5)
    for k in kandidaten:
        zugeordnet.neu(k.stadt.laenge, k.stadt.breite)
    for k in ohne_regel:
        s = k.stadt
        naechste = sorted(
            (geo.abstand_km(s.laenge, s.breite, *zugeordnet.punkte[i]), i)
            for i in zugeordnet.nahe(s.laenge, s.breite, 60.0)
        )
        if not naechste or naechste[0][0] > 60.0:
            raise OrteFehler(
                f"ohne Regel in landschaften.csv und ohne zugeordneten Ort bis 60 km: {s.name} ({s.geonameid}, "
                f"{s.staat} {s.admin1}/{s.admin2}/{s.admin3}/{s.admin4})"
            )
        beste = naechste[0]
        landschaft = kandidaten[beste[1]].landschaft
        bericht.append(
            f"ohne Regel in landschaften.csv: {s.name} ({s.staat} {s.admin1}/{s.admin2}) → {landschaft} (nächster Ort)"
        )
        kandidaten.append(Kandidat(s, k.auf_land, landschaft, bedeutung(s), verwaltungsrang(s)))
    kandidaten.sort(key=lambda k: (-k.bedeutung, k.stadt.geonameid))
    melde(
        f"{len(kandidaten)} Kandidaten ({ausgeschlossen} ausgeschlossen, {len(m1_ids)} M1-Entsprechungen) "
        f"[{time.perf_counter() - uhr:.1f} s]"
    )

    # Lage auf Kernraum-Land, dann Auswahl
    raster = lade_raster()
    kandidaten, ohne_land = setze_lagen(kandidaten, raster)
    for k in ohne_land:
        bericht.append(
            f"ohne Kernraum-Land im Umkreis von {LAND_SUCHE_KM:g} km, entfällt: {k.stadt.name} ({k.stadt.geonameid})"
        )
    m1_lagen = [(float(z["laenge"]), float(z["breite"]), z["datei"]) for z in m1_zeilen]
    auswahl = waehle_orte(kandidaten, m1_lagen, raster, tabellen.landschaften)
    zaehl = {a: sum(1 for v in auswahl.herkunft.values() if v == a) for a in ("scheibe", "insel", "fuellung")}
    melde(
        f"Auswahl: {len(auswahl.orte)} neue Orte (Scheibe {zaehl['scheibe']}, Inseln {zaehl['insel']}, "
        f"Füllung {zaehl['fuellung']}; {auswahl.leere_zellen} Blöcke ohne Kandidat) [{time.perf_counter() - uhr:.1f} s]"
    )

    lagen_text = [(k.laenge_text, k.breite_text, k.verschoben) for k in auswahl.orte]

    # Gelände
    auskunft = hoehen.Lageauskunft.laden()

    gebiete = lade_gelaende_gebiete(HIER / "gelaende_gebiete.csv")

    def merkmale(laenge: float, breite: float) -> Gelaendemerkmale:
        werte = hoehenwerte(auskunft.hoehen, auskunft.land.enthaelt_viele, laenge, breite, GELAENDE_RADIUS_KM)
        p05, p95 = np.percentile(werte, [5.0, 95.0])
        kueste = auskunft.gewaesser.kueste_km(laenge, breite)
        haff = haff_km(raster, laenge, breite, KUESTE_KM)
        if haff is not None and (kueste is None or haff < kueste):
            kueste = haff
        fluss = auskunft.gewaesser.fluss_km(laenge, breite)
        lage = raster.pixel([laenge], [breite])
        komp = int(raster.komponente[lage.py[0], lage.px[0]]) if lage.innen[0] else -1
        insel = komp >= 0 and komp != raster.hauptkomponente and raster.komponente_km2[komp] < INSEL_MAX_KM2
        return Gelaendemerkmale(
            mittel=round(float(werte.mean()), 1),
            p05=round(float(p05), 1),
            p95=round(float(p95), 1),
            kueste_km=None if kueste is None else round(kueste, 2),
            fluss_km=None if fluss is None else round(fluss, 2),
            seeanteil=round(seeanteil(raster, laenge, breite), 3),
            insel=insel,
            gebiet=gebiet_bei(gebiete, laenge, breite),
        )

    melde(f"Höhenmodell geladen [{time.perf_counter() - uhr:.1f} s]")

    # Flüsse für Kennungszusätze
    fluesse = _flusskennungen()
    fluss_index = [(name, geo.LinienIndex(linien)) for name, linien in fluesse]

    def naechster_fluss(laenge: float, breite: float) -> str:
        beste = (5.0, "")
        for name, index in fluss_index:
            d = index.abstand_km(laenge, breite, beste[0])
            if d is not None and (d, name) < beste:
                beste = (d, name)
        return beste[1]

    # Namen; gleichnamige neue Orte erhalten den Kurznamen ihrer Landschaft als Zusatz
    wahlen = [waehle_name(k.stadt, tabellen.exonyme) for k in auswahl.orte]
    namen = zusatz_bei_gleichnamigen(
        [w.name for w in wahlen],
        [tabellen.landschaften[k.landschaft].kurzname for k in auswahl.orte],
        [z["name"] for z in m1_zeilen],
    )

    # Neue Orte zusammenstellen
    neue: list[Ort] = []
    for k, (lt, bt, verschoben), wahl, name in zip(auswahl.orte, lagen_text, wahlen, namen):
        s = k.stadt
        gm = merkmale(float(lt), float(bt))
        stadt = "ja" if k.rang <= 2 or s.einwohner >= STADT_EINWOHNER else "nein"
        quelle = (
            f"{QUELLE_PRAEFIX}{s.geonameid}; {wahl.herkunft}"
            + ("; Zusatz der Landschaft zur Unterscheidung gleichnamiger Orte" if name != wahl.name else "")
            + ("; Einwohnerzahl für die Auswahl aus einwohner.csv" if s.geonameid in tabellen.einwohner else "")
            + ("; Lage auf das nächste Landpixel gesetzt" if verschoben else "")
            + "; Gelände aus Höhenmodell; Fläche aus der Karte"
        )
        neue.append(
            Ort(
                kennung="",
                datei=k.landschaft,
                name=name,
                gelaende=gelaende_aus(gm),
                laenge=lt,
                breite=bt,
                stadt=stadt,
                quelle=quelle,
                m1=False,
                groesse=float(s.einwohner),
                rangfolge=(-k.bedeutung, s.geonameid),
                geonameid=s.geonameid,
                gelaende_merkmale=gm,
            )
        )
    melde(f"Gelände und Namen [{time.perf_counter() - uhr:.1f} s]")
    gleich: dict[str, int] = {}
    for n in [z["name"] for z in m1_zeilen] + namen:
        gleich[n] = gleich.get(n, 0) + 1
    doppelt = sorted(n for n, z in gleich.items() if z > 1)
    if doppelt:
        bericht.append(f"gleichnamige Orte (auch nach Zusatz, gleiche Landschaft): {', '.join(doppelt)}")

    # Kennungen aus dem Namen (Rangfolge: Bedeutung)
    m1_kennungen = {z["id"] for z in m1_zeilen}
    reihenfolge = sorted(range(len(neue)), key=lambda i: neue[i].rangfolge)
    basen = [kennung_aus_name(neue[i].name) for i in reihenfolge]
    flussnamen = [naechster_fluss(float(neue[i].laenge), float(neue[i].breite)) for i in reihenfolge]
    kennungen = vergebe_kennungen(basen, [neue[i].datei for i in reihenfolge], flussnamen, m1_kennungen)
    for i, kennung in zip(reihenfolge, kennungen):
        neue[i].kennung = kennung

    # M1-Orte
    m1_orte = []
    for z in m1_zeilen:
        s = m1_entsprechungen[z["id"]]
        m1_orte.append(
            Ort(
                kennung=z["id"],
                datei=z["datei"],
                name=z["name"],
                gelaende=z["gelaende"],
                laenge=z["laenge"],
                breite=z["breite"],
                stadt=z["stadt"],
                quelle=z["quelle"],
                m1=True,
                groesse=float(s.einwohner),
                rangfolge=(-bedeutung(s), s.geonameid),
                geonameid=s.geonameid,
                zeile_m1=z,
            )
        )

    # Erste Karte (Ladereihenfolge: Dateien sortiert, M1-Orte zuerst, dann neue nach Bedeutung)
    def ladereihenfolge(orte: Sequence[Ort]) -> list[Ort]:
        return sorted(orte, key=lambda o: o.datei)  # stabil: Reihenfolge in orte.csv je Datei

    vorlaeufig = m1_orte + sorted(neue, key=lambda o: (o.datei, o.rangfolge))
    folge = ladereihenfolge(vorlaeufig)
    karte1 = kg.erzeuge(raster.grundlage, [kg.Ort(o.kennung, o.name, float(o.laenge), float(o.breite)) for o in folge])
    melde(f"Karte (1) berechnet [{time.perf_counter() - uhr:.1f} s]")
    nachbarn = karte1.nachbarn()

    # Kreise je Landschaft (Exklaven zählen zur umgebenden Landschaft)
    lagen = [(float(o.laenge), float(o.breite)) for o in folge]
    groessen = [o.groesse for o in folge]
    kreis_landschaft = exklaven_zuordnen([o.datei for o in folge], nachbarn)
    for i, o in enumerate(folge):
        if kreis_landschaft[i] != o.datei:
            bericht.append(f"Exklave: {o.kennung} ({o.datei}) bildet Kreis mit {kreis_landschaft[i]}")
    kreise: list[tuple[str, str, str]] = []
    kreis_von: dict[str, str] = {}
    hauptort_von: dict[str, str] = {}
    for landschaft in sorted(set(kreis_landschaft)):
        knoten = [i for i, l in enumerate(kreis_landschaft) if l == landschaft]
        gruppen = kreise_einer_landschaft(knoten, list(nachbarn), lagen)
        eintraege = []
        for g in gruppen:
            haupt = min(g, key=lambda i: (-groessen[i], folge[i].rangfolge))
            eintraege.append((-lagen[haupt][1], lagen[haupt][0], haupt, g))
        for _, _, haupt, g in sorted(eintraege):
            ort = folge[haupt]
            kennung = f"kr_{ort.kennung}"
            kreise.append((kennung, landschaft, ort.name))
            hauptort_von[kennung] = ort.kennung
            for i in g:
                kreis_von[folge[i].kennung] = kennung
    kreis_rang = {k[0]: i for i, k in enumerate(kreise)}
    for o in m1_orte + neue:
        o.kreis = kreis_von[o.kennung]

    # Endgültige Reihenfolge: M1-Orte wie bisher, dann neue nach Landschaft, Kreis, Hauptort zuerst, Bedeutung.
    neue.sort(key=lambda o: (o.datei, kreis_rang[o.kreis], hauptort_von[o.kreis] != o.kennung, o.rangfolge))
    orte = m1_orte + neue
    folge = ladereihenfolge(orte)
    karte = kg.erzeuge(raster.grundlage, [kg.Ort(o.kennung, o.name, float(o.laenge), float(o.breite)) for o in folge])
    melde(f"Karte (2) berechnet [{time.perf_counter() - uhr:.1f} s]")
    flaechen = karte.flaeche_nach_ort
    for o in orte:
        f = flaechen[o.kennung]
        if f.pixel <= 0 or f.flaeche_km2 <= 0:
            raise OrteFehler(f"Ort {o.kennung} erhält auf der Karte keine Fläche")
        o.flaeche = str(f.flaeche_km2)

    # Tabellentexte; Vorkommen und Hafen (von Hand gepflegt, seit M5) bleiben je Ort erhalten.
    hand = {z["id"]: tuple(z.get(s, "") for s in ORTE_HANDSPALTEN) for z in orte_alt}
    zeilen_orte = []
    for o in orte:
        zeilen_orte.append(
            (
                o.kennung,
                o.datei,
                o.name,
                o.gelaende,
                o.laenge,
                o.breite,
                o.flaeche,
                o.stadt,
                o.kreis,
                *hand.get(o.kennung, ("",) * len(ORTE_HANDSPALTEN)),
                o.quelle,
            )
        )
    zeilen_kreise = [(k, l, n, KREIS_QUELLE) for k, l, n in kreise]
    texte = {
        "orte.csv": csv_text(ORTE_SPALTEN, zeilen_orte),
        "kreise.csv": csv_text(KREISE_SPALTEN, zeilen_kreise),
        "dateien.csv": csv_text(
            DATEIEN_SPALTEN, dateien_zeilen(dateien_alt, tabellen.landschaften, m1_dateien, {o.datei for o in orte})
        ),
    }
    bericht += kennzahlen(orte, kreise, karte, auswahl, tabellen)
    return Ergebnis(orte, kreise, texte, bericht, karte)


def umbrechen(text: str, breite: int = KOMMENTAR_BREITE) -> str:
    """Bricht einen Absatz für die Kopfkommentare an Leerzeichen um (Wörter und Bindestriche bleiben ganz)."""
    return textwrap.fill(text, width=breite, break_long_words=False, break_on_hyphens=False)


def dateien_zeilen(
    alt: Sequence[dict[str, str]], landschaften: dict[str, Landschaft], m1_dateien: set[str], benutzt: set[str]
) -> list[tuple[str, str, str, str]]:
    """dateien.csv: Staaten unverändert; Orte-Dateien der M1 mit M3-Absatz; neue Orte- und Kreis-Dateien.

    Die Standardkultur der Orte-Dateien (Spalte kultur, seit M4) wird von Hand gepflegt und bleibt erhalten.
    """
    zeilen: list[tuple[str, str, str, str]] = []
    for z in alt:
        if z["ordner"] == "staaten":
            zeilen.append((z["ordner"], z["datei"], z.get("kultur", ""), z["kommentar"]))
    alt_orte = {z["datei"]: z["kommentar"] for z in alt if z["ordner"] == "orte"}
    alt_kultur = {z["datei"]: z.get("kultur", "") for z in alt if z["ordner"] == "orte"}
    for datei in sorted(benutzt):
        l = landschaften[datei]
        if datei in m1_dateien:
            basis = alt_orte.get(datei, "")
            stelle = basis.find("\n\n" + M3_ABSATZ)
            if stelle >= 0:
                basis = basis[:stelle]
            absatz = umbrechen(
                f"{M3_ABSATZ} weitere Orte ohne Daten (ohne Historie) aus GeoNames, räumlich ausgewählt, "
                "Gelände aus dem Höhenmodell, Fläche aus der Karte."
            )
            kommentar = f"{basis}\n\n{absatz}\n{umbrechen(f'Umfang der Landschaft {l.name}: {l.kommentar}')}"
        else:
            kommentar = (
                f"Orte: {l.name} – Orte ohne Daten (Meilenstein M3).\n\n"
                "Format: docs/technik/chronik-skript.md, Abschnitt 3.6.\n"
                "Erzeugt von tools/geodaten/orte.py aus GeoNames cities1000 (CC BY 4.0): räumlich\n"
                "ausgewählte Orte mit deutschem Namen, wo GeoNames ihn führt, Gelände aus dem Höhenmodell\n"
                "und Fläche aus der Karte. Noch ohne Historie (Besitz, Bevölkerung).\n\n"
                f"{umbrechen(f'Umfang: {l.kommentar}')}"
            )
        zeilen.append(("orte", datei, alt_kultur.get(datei, ""), kommentar))
    for datei in sorted(benutzt):
        l = landschaften[datei]
        zeilen.append(
            (
                "kreise",
                datei,
                "",
                f"Kreise: {l.name} (Meilenstein M3).\n\n"
                "Format: docs/technik/chronik-skript.md, Abschnitt 3.9.\n"
                f"Erzeugt von tools/geodaten/orte.py: je {KREIS_MIN}–{KREIS_MAX} benachbarte Orte der Landschaft,\n"
                "benannt nach dem größten Ort (heutige Einwohnerzahl). Räumlich gebildet, keine\n"
                "historischen Verwaltungseinheiten.",
            )
        )
    return zeilen


def kennzahlen(
    orte: Sequence[Ort],
    kreise: Sequence[tuple[str, str, str]],
    karte: kg.Ergebnis,
    auswahl: Auswahl,
    tabellen: Hilfstabellen,
) -> list[str]:
    zeilen = []
    flaechen = sorted((int(o.flaeche), o.kennung) for o in orte)
    werte = [f for f, _ in flaechen]
    zeilen.append(
        f"Orte: {len(orte)} ({sum(1 for o in orte if o.m1)} M1-Orte), Kreise: {len(kreise)}, Landschaften: "
        f"{len({o.datei for o in orte})}"
    )
    zeilen.append(
        f"Fläche: kleinste {flaechen[0][0]} km² ({flaechen[0][1]}), Median {werte[len(werte) // 2]} km², "
        f"größte {flaechen[-1][0]} km² ({flaechen[-1][1]}), Summe {sum(werte)} km²"
    )
    groesse: dict[str, int] = {}
    for o in orte:
        groesse[o.kreis] = groesse.get(o.kreis, 0) + 1
    verteilung = sorted(groesse.values())
    zeilen.append(f"Orte je Kreis: {verteilung[0]}–{verteilung[-1]}, Median {verteilung[len(verteilung) // 2]}")
    gelaende: dict[str, int] = {}
    for o in orte:
        gelaende[o.gelaende] = gelaende.get(o.gelaende, 0) + 1
    zeilen.append("Gelände: " + ", ".join(f"{k} {v}" for k, v in sorted(gelaende.items(), key=lambda e: -e[1])))
    zeilen.append(f"Städte: {sum(1 for o in orte if o.stadt == 'ja')}")
    je: dict[str, list[int]] = {}
    for o in orte:
        je.setdefault(o.datei, []).append(int(o.flaeche))
    zeilen.append(
        "Orte je Landschaft (Anzahl, Median km²): "
        + ", ".join(f"{d} {len(v)} ({sorted(v)[len(v) // 2]})" for d, v in sorted(je.items()))
    )
    if karte.ohne_ort:
        zeilen.append(
            f"Kernraum-Land ohne Ort: {sum(t.pixel for t in karte.ohne_ort)} Pixel in {len(karte.ohne_ort)} Teilflächen; "
            + ", ".join(f"{t.flaeche_km2:.0f} km² bei {t.laenge:.2f} °O {t.breite:.2f} °N" for t in karte.ohne_ort[:12])
        )
    andere = [w for w in karte.warnungen if "Kernraum-Land ohne Ort" not in w]
    zeilen += [f"Kartengenerator: {w}" for w in andere]
    return zeilen


def vergleiche(texte: dict[str, str]) -> list[str]:
    unterschiede = []
    for name, inhalt in texte.items():
        pfad = TABELLEN / name
        alt = pfad.read_bytes() if pfad.is_file() else None
        if alt != inhalt.encode("utf-8"):
            unterschiede.append(name)
    return unterschiede


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="orte.py", description="Orte und Kreise des Kernraums erzeugen (M3).")
    parser.add_argument(
        "--pruefen", action="store_true", help="nur vergleichen, nichts schreiben (Exitcode 1 bei Abweichung)"
    )
    parser.add_argument("--bericht", type=Path, default=None, help="Kennzahlen zusätzlich in diese Datei schreiben")
    args = parser.parse_args(argv)
    try:
        ergebnis = erzeuge()
    except (OrteFehler, geodaten.GeodatenFehler, kg.DatenFehler, OSError, ValueError) as fehler:
        meldungen = getattr(fehler, "meldungen", None)
        print("Fehler: " + ("\n".join(meldungen) if meldungen else str(fehler)), file=sys.stderr)
        return 2
    for zeile in ergebnis.bericht:
        print(zeile)
    if args.bericht:
        args.bericht.write_text("\n".join(ergebnis.bericht) + "\n", encoding="utf-8")
    unterschiede = vergleiche(ergebnis.texte)
    if args.pruefen:
        if unterschiede:
            print("veraltet: " + ", ".join(unterschiede), file=sys.stderr)
            return 1
        print("Tabellen sind aktuell.")
        return 0
    for name in unterschiede:
        (TABELLEN / name).write_text(ergebnis.texte[name], encoding="utf-8", newline="")
    print(f"geschrieben: {', '.join(unterschiede) if unterschiede else 'nichts (unverändert)'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
