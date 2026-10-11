#!/usr/bin/env python3
"""Ortskandidaten aus GeoNames „cities1000“ für den Kernraum.

``cities1000.txt`` (GeoNames, CC BY 4.0, Stand 2020; im Zwischenspeicher unter
``build/geodaten-cache/geonames/``, siehe ``geodaten.py holen orte``) enthält alle Siedlungen ab
1.000 Einwohnern weltweit, TAB-getrennt, eine Zeile je Ort mit 19 Spalten:

    geonameid, name, asciiname, alternatenames (kommagetrennt, alle Sprachen ohne Kennzeichnung),
    latitude, longitude, feature class, feature code, country code, cc2 (kommagetrennt),
    admin1, admin2, admin3, admin4, population, elevation, dem, timezone, modification date

Dieses Modul liest die Datei (``lies_staedte``) und wählt die Kandidaten im Kernraum aus
(``kandidaten``): Punkt im Umriss ``data/karte/geometrie/kernraum.geojson`` und auf Land
(``land.geojson``, nicht in einem See aus ``seen.geojson``). Weil die Küsten von Natural Earth
1:10 Mio. grob sind, liegen manche Küsten- und Inselorte (Helgoland, Hel, Sonderburg) knapp im
Wasser; sie bleiben bis ``land_toleranz_km`` Abstand zum Ufer Kandidaten und sind mit
``auf_land = False`` gekennzeichnet. Alle Felder bleiben erhalten, die Alternativnamen als Liste. Die Namen sind heutige Namen; historische deutsche Namen stehen –
wenn überhaupt – ohne Kennzeichnung unter den Alternativnamen (z. B. Wrocław → „Breslau“) und
müssen vom Ort-Generator bewusst ausgewählt und belegt werden.

Aufruf zur Übersicht::

    python3 tools/geodaten/staedte.py [--min-einwohner N] [--ausgabe kandidaten.tsv]

Benötigt numpy (für die Punkt-in-Polygon-Abfrage der Landmaske).
"""

from __future__ import annotations

import argparse
import sys
from collections.abc import Iterable, Iterator, Sequence
from dataclasses import dataclass
from pathlib import Path

try:
    import geo
    import geodaten
except ImportError:  # Aufruf als Modul von außerhalb des Werkzeugordners
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import geo
    import geodaten

__all__ = [
    "AUSLAND_GEBIETE",
    "AUSLAND_STAATEN",
    "LAND_TOLERANZ_KM",
    "SPALTEN",
    "Kandidat",
    "Stadt",
    "StaedteFehler",
    "kandidaten",
    "lies_staedte",
    "parse_zeile",
    "staedte_datei",
]

#: Spalten von cities1000.txt in Dateireihenfolge (Namen wie in der GeoNames-Beschreibung).
SPALTEN = (
    "geonameid",
    "name",
    "asciiname",
    "alternatenames",
    "latitude",
    "longitude",
    "feature class",
    "feature code",
    "country code",
    "cc2",
    "admin1 code",
    "admin2 code",
    "admin3 code",
    "admin4 code",
    "population",
    "elevation",
    "dem",
    "timezone",
    "modification date",
)


#: Heutige Staaten (ISO-Code), die ganz außerhalb des Kernraums liegen. Ihre Orte entfallen auch dann,
#: wenn sie wegen der vereinfachten Grenzlinie (Genauigkeit 1–5 km) knapp im Umriss liegen, z. B.
#: Schweizer Dörfer am Alpenrhein oder bei Schaffhausen.
AUSLAND_STAATEN: frozenset[str] = frozenset({"CH", "HR", "HU", "SK"})
#: Gebiete heutiger Staaten (Staat, admin1, admin2), die ebenfalls ganz außerhalb liegen: die Provinz
#: Trient (Trentino, Welschtirol). Der Umriss folgt der Provinz Bozen und ragt am Nonsberg einige
#: Kilometer ins Trentino (Fondo, Castelfondo, Brez); deren Orte entfallen wie die der Staaten oben.
AUSLAND_GEBIETE: frozenset[tuple[str, str, str]] = frozenset({("IT", "17", "TN")})
#: Orte im Wasser (Meer oder See), die höchstens so weit vom Ufer entfernt liegen, bleiben Kandidaten.
LAND_TOLERANZ_KM = 2.0


class StaedteFehler(geodaten.GeodatenFehler):
    """Fehler beim Lesen von cities1000.txt (mit Zeilennummer)."""


@dataclass(frozen=True)
class Stadt:
    """Ein Eintrag aus cities1000.txt mit allen Feldern."""

    geonameid: int
    name: str
    asciiname: str
    alternativnamen: tuple[str, ...]
    breite: float  # latitude, Grad Nord (WGS84)
    laenge: float  # longitude, Grad Ost (WGS84)
    merkmalsklasse: str  # feature class, in cities1000 immer "P" (Siedlung)
    merkmalscode: str  # feature code, z. B. PPL, PPLA (Sitz Verwaltungsebene 1), PPLC (Hauptstadt)
    staat: str  # country code (ISO 3166-1 alpha-2)
    weitere_staaten: tuple[str, ...]  # cc2
    admin1: str
    admin2: str
    admin3: str
    admin4: str
    einwohner: int  # population (Stand der GeoNames-Daten, nicht historisch)
    hoehe: int | None  # elevation in m (oft leer)
    dem: int | None  # Höhe aus dem Geländemodell (SRTM3/GTOPO30) in m; −9999 = keine Angabe
    zeitzone: str
    geaendert: str  # modification date (JJJJ-MM-TT)

    def alle_namen(self) -> list[str]:
        """Name, ASCII-Name und Alternativnamen ohne Doppelte, in dieser Reihenfolge."""
        gesehen: set[str] = set()
        namen = []
        for name in (self.name, self.asciiname, *self.alternativnamen):
            if name and name not in gesehen:
                gesehen.add(name)
                namen.append(name)
        return namen


def _ganz(text: str, feld: str, zeile: int, leer_erlaubt: bool = False) -> int | None:
    if text == "":
        if leer_erlaubt:
            return None
        raise StaedteFehler(f"Zeile {zeile}: Feld '{feld}' ist leer")
    try:
        return int(text)
    except ValueError:
        raise StaedteFehler(f"Zeile {zeile}: Feld '{feld}' ist keine ganze Zahl: {text!r}") from None


def _zahl(text: str, feld: str, zeile: int) -> float:
    try:
        return float(text)
    except ValueError:
        raise StaedteFehler(f"Zeile {zeile}: Feld '{feld}' ist keine Zahl: {text!r}") from None


def _liste(text: str) -> tuple[str, ...]:
    return tuple(t for t in (s.strip() for s in text.split(",")) if t)


def parse_zeile(text: str, zeile: int = 0) -> Stadt:
    """Liest eine Zeile von cities1000.txt (ohne Zeilenende); ``StaedteFehler`` bei falschem Aufbau."""
    teile = text.split("\t")
    if len(teile) != len(SPALTEN):
        raise StaedteFehler(f"Zeile {zeile}: erwartet {len(SPALTEN)} Spalten, gefunden {len(teile)}")
    breite = _zahl(teile[4], "latitude", zeile)
    laenge = _zahl(teile[5], "longitude", zeile)
    if not (-90.0 <= breite <= 90.0 and -180.0 <= laenge <= 180.0):
        raise StaedteFehler(f"Zeile {zeile}: Koordinaten außerhalb des gültigen Bereichs")
    return Stadt(
        geonameid=_ganz(teile[0], "geonameid", zeile) or 0,
        name=teile[1],
        asciiname=teile[2],
        alternativnamen=_liste(teile[3]),
        breite=breite,
        laenge=laenge,
        merkmalsklasse=teile[6],
        merkmalscode=teile[7],
        staat=teile[8],
        weitere_staaten=_liste(teile[9]),
        admin1=teile[10],
        admin2=teile[11],
        admin3=teile[12],
        admin4=teile[13],
        einwohner=_ganz(teile[14], "population", zeile, leer_erlaubt=True) or 0,
        hoehe=_ganz(teile[15], "elevation", zeile, leer_erlaubt=True),
        dem=_ganz(teile[16], "dem", zeile, leer_erlaubt=True),
        zeitzone=teile[17],
        geaendert=teile[18],
    )


def lies_staedte(pfad: Path | None = None, rahmen: Sequence[float] | None = None) -> Iterator[Stadt]:
    """Liest cities1000.txt (UTF-8); optional nur Orte im Rechteck ``rahmen`` (lmin, bmin, lmax, bmax).

    Leere Zeilen werden übersprungen; jede andere fehlerhafte Zeile ist ein ``StaedteFehler``.
    """
    pfad = pfad or staedte_datei()
    with open(pfad, encoding="utf-8", newline="") as datei:
        for nr, zeile in enumerate(datei, start=1):
            zeile = zeile.rstrip("\r\n")
            if not zeile:
                continue
            stadt = parse_zeile(zeile, nr)
            if rahmen is not None and not (
                rahmen[0] <= stadt.laenge <= rahmen[2] and rahmen[1] <= stadt.breite <= rahmen[3]
            ):
                continue
            yield stadt


def staedte_datei(speicher: Path = geodaten.ZWISCHENSPEICHER) -> Path:
    """Pfad von cities1000.txt im Zwischenspeicher laut quellen.json (Fehler, wenn sie fehlt)."""
    manifest = geodaten.lade_manifest()
    for quelle in geodaten.quellen(manifest, ["orte"]):
        if quelle.auspacken:
            pfad = speicher / quelle.auspacken["datei"]
            if not pfad.is_file():
                raise StaedteFehler(f"{pfad} fehlt; zuerst 'python3 tools/geodaten/geodaten.py holen orte'")
            return pfad
    raise StaedteFehler("quellen.json enthält keine Quelle der Gruppe 'orte'")


@dataclass(frozen=True)
class Kandidat:
    """Ein Ort aus cities1000.txt im Kernraum."""

    stadt: Stadt
    auf_land: bool  # Punkt auf Land und nicht in einem See (Rasterregel aus Abschnitt 6.2, exakt)
    wasserabstand_km: float  # 0 auf Land, sonst Abstand zum nächsten Ufer (Meeresküste oder Seeufer)


def kandidaten(
    staedte: Iterable[Stadt] | None = None,
    kernraum: Sequence[geo.Polygon] | None = None,
    land: Sequence[geo.Polygon] | None = None,
    seen: Sequence[geo.Polygon] | None = None,
    min_einwohner: int = 0,
    ausland: frozenset[str] = AUSLAND_STAATEN,
    land_toleranz_km: float = LAND_TOLERANZ_KM,
    ausland_gebiete: frozenset[tuple[str, str, str]] = AUSLAND_GEBIETE,
) -> list[Kandidat]:
    """Orte im Kernraum auf Land, sortiert nach Einwohnern (absteigend), dann geonameid.

    Ohne Angaben werden ``cities1000.txt`` aus dem Zwischenspeicher sowie ``kernraum.geojson``,
    ``land.geojson`` und ``seen.geojson`` aus ``data/karte/geometrie/`` gelesen. Punkte genau auf
    einem Rand zählen als außen (Abschnitt 6.2); exakte Punkt-in-Polygon-Prüfung (kein Raster).
    Orte im Wasser bleiben, wenn sie höchstens ``land_toleranz_km`` vom Ufer entfernt sind
    (``auf_land = False``). Orte aus Staaten in ``ausland`` (heutiger Staatscode) und aus Gebieten in
    ``ausland_gebiete`` (Staat, admin1, admin2) entfallen immer.
    """
    ordner = geodaten.GEOMETRIE_ORDNER
    kernraum = geo.lade_flaechen(ordner / "kernraum.geojson") if kernraum is None else kernraum
    land = geo.lade_flaechen(ordner / "land.geojson") if land is None else land
    seen = geo.lade_flaechen(ordner / "seen.geojson") if seen is None else seen
    punkte = [p for polygon in kernraum for ring in polygon for p in ring]
    if not punkte:
        return []
    rahmen = (
        min(p[0] for p in punkte),
        min(p[1] for p in punkte),
        max(p[0] for p in punkte),
        max(p[1] for p in punkte),
    )
    if staedte is None:
        staedte = lies_staedte(rahmen=rahmen)
    kern_index = geo.FlaechenIndex(kernraum)
    land_index = geo.FlaechenIndex(land)
    see_index = geo.FlaechenIndex(seen)
    kueste = geo.LinienIndex([ring for polygon in land for ring in polygon])
    ufer = geo.LinienIndex([ring for polygon in seen for ring in polygon])
    auswahl = []
    for stadt in staedte:
        x, y = stadt.laenge, stadt.breite
        if stadt.einwohner < min_einwohner or stadt.staat in ausland:
            continue
        if (stadt.staat, stadt.admin1, stadt.admin2) in ausland_gebiete:
            continue
        if not (rahmen[0] <= x <= rahmen[2] and rahmen[1] <= y <= rahmen[3]) or not kern_index.enthaelt(x, y):
            continue
        if not land_index.enthaelt(x, y):
            abstand = kueste.abstand_km(x, y, land_toleranz_km)
        elif see_index.enthaelt(x, y):
            abstand = ufer.abstand_km(x, y, land_toleranz_km)
        else:
            auswahl.append(Kandidat(stadt, True, 0.0))
            continue
        if abstand is not None and abstand <= land_toleranz_km:
            auswahl.append(Kandidat(stadt, False, abstand))
    auswahl.sort(key=lambda k: (-k.stadt.einwohner, k.stadt.geonameid))
    return auswahl


def _tsv_feld(wert: object) -> str:
    if wert is None:
        return ""
    if isinstance(wert, tuple):
        return ",".join(wert)
    return str(wert)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="staedte.py", description="Ortskandidaten aus GeoNames cities1000 im Kernraum."
    )
    parser.add_argument("--min-einwohner", type=int, default=0, help="nur Orte ab dieser Einwohnerzahl")
    parser.add_argument(
        "--ausgabe", type=Path, default=None, help="Kandidaten als TSV schreiben (UTF-8, mit Kopfzeile)"
    )
    args = parser.parse_args(argv)
    try:
        liste = kandidaten(min_einwohner=args.min_einwohner)
    except (geodaten.GeodatenFehler, OSError, ValueError) as fehler:
        print(f"Fehler: {fehler}", file=sys.stderr)
        return 2
    je_staat: dict[str, int] = {}
    for k in liste:
        je_staat[k.stadt.staat] = je_staat.get(k.stadt.staat, 0) + 1
    im_wasser = sum(1 for k in liste if not k.auf_land)
    print(
        f"{len(liste)} Kandidaten im Kernraum"
        + (f" ab {args.min_einwohner} Einwohnern" if args.min_einwohner else "")
        + f", davon {im_wasser} knapp im Wasser (bis {LAND_TOLERANZ_KM:g} km vom Ufer)"
    )
    print("je heutigem Staat: " + ", ".join(f"{k} {v}" for k, v in sorted(je_staat.items(), key=lambda e: -e[1])))
    if args.ausgabe:
        felder = list(Stadt.__dataclass_fields__)
        with open(args.ausgabe, "w", encoding="utf-8", newline="\n") as datei:
            datei.write("\t".join(felder + ["auf_land", "wasserabstand_km"]) + "\n")
            for k in liste:
                werte = [_tsv_feld(getattr(k.stadt, f)) for f in felder]
                werte += ["ja" if k.auf_land else "nein", f"{k.wasserabstand_km:.2f}"]
                datei.write("\t".join(werte) + "\n")
        print(f"geschrieben: {args.ausgabe}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
