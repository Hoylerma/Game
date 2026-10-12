#!/usr/bin/env python3
"""Bevölkerung der Orte zum Lesezeichen 1848 (Meilenstein M7): Schätzung zum 3. Dezember 1846.

Für rund 4.000 Orte gibt es keine Einzelzahlen; belegt sind (in der Literatur) die Einwohner je
Staat bzw. Provinz und Kronland und die der großen Städte. Das Werkzeug verteilt die Summen auf
die Orte (docs/technik/lesezeichen_1848.md, Abschnitt 7):

``einwohner_i = feste_stadt_i + f_raum × (LAND_DICHTE × fläche_i × dichte_i × gelände_i + stadtgewicht_i)``

- **Raum**: die erste Einheit aus Verwaltungseinheit des Ortes, ihren übergeordneten Einheiten
  und Staat, für die ``einwohner_1846.csv`` eine Summe nennt; ``f_raum`` so, dass die Orte des
  Raums zusammen die Summe ergeben.
- **feste Stadt**: Einwohner der Stadt aus ``staedte_1846.csv`` (die Umgebung des Ortes kommt
  über die Fläche dazu).
- **dichte**: Faktor ländlicher Dichte aus ``merkmale_1846.csv`` (Standard 1), **gelände**: Faktor
  je Geländeart (``GELAENDE``), **stadtgewicht**: für übrige Orte mit Stadtrecht
  ``STADT_FAKTOR × einwohner_heute ^ STADT_EXPONENT`` (heutige Einwohner aus gebietscodes.csv).

Konfession und Kultur je Ort stehen in ``merkmale_1846.csv`` (Regeln wie in territorien_1848.csv,
dazu ``verwaltung:<id>`` und ``staat:<TAG>``); ohne Kultur gilt die Standardkultur der Landschaft
(dateien.csv).

Aufruf::

    python3 tools/geodaten/bevoelkerung_1846.py              # bevoelkerung.csv: Zeilen zum Stichtag neu
    python3 tools/geodaten/bevoelkerung_1846.py --pruefen    # nur vergleichen (Exitcode 1, wenn veraltet)
    python3 tools/geodaten/bevoelkerung_1846.py --bericht    # Summen je Raum und größte Orte

Vorher ``zuordnung.py``, danach ``tools/archivar/archivar.py``.
"""

from __future__ import annotations

import argparse
import csv
import io
import re
import sys
from collections import defaultdict
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

try:
    import zuordnung as zu
except ImportError:  # Aufruf als Modul von außerhalb des Werkzeugordners
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import zuordnung as zu

STICHTAG = "1846.12.03"
EINWOHNER = zu.TABELLEN / "einwohner_1846.csv"
STAEDTE = zu.TABELLEN / "staedte_1846.csv"
MERKMALE = zu.TABELLEN / "merkmale_1846.csv"
EINWOHNER_SPALTEN = ("raum", "einwohner", "sicherheit", "quelle")
STAEDTE_SPALTEN = ("ort", "einwohner", "sicherheit", "quelle")
MERKMAL_SPALTEN = ("gebiet", "konfession", "kultur", "dichte", "sicherheit", "quelle")
BEVOELKERUNG_SPALTEN = ("ort", "stichtag", "bevoelkerung", "konfession", "kultur", "verwuestung", "sicherheit", "quelle")

#: Faktor der ländlichen Dichte je Geländeart (common/gelaende.chron).
GELAENDE = {
    "flussaue": 1.3,
    "boerde": 1.3,
    "flachland": 1.0,
    "huegelland": 1.0,
    "mittelgebirge": 0.8,
    "heide": 0.5,
    "seenplatte": 0.7,
    "kueste": 0.9,
    "bruch": 0.5,
    "wald": 0.5,
    "hochgebirge": 0.3,
}
#: Ländliche Ausgangsdichte (Einwohner je km²), damit Flächen- und Stadtgewicht dieselbe Einheit haben.
LAND_DICHTE = 60.0
STADT_FAKTOR = 1.5
STADT_EXPONENT = 0.75
RE_ANTEILE = re.compile(r"^([a-z_]+=\d+)( [a-z_]+=\d+)*$")


def rang_merkmal(gebiet: str) -> float:
    """Rang einer Gebietsangabe in merkmale_1846.csv (höher = stärker); 0 = ungültig."""
    if gebiet.startswith("verwaltung:"):
        return 2.5
    if gebiet.startswith("staat:"):
        return 1.5
    return {6: 6, 5: 5, 4: 4, 3: 3, 2: 1, 1: 0.5}.get(zu.rang_von(gebiet), 0)


@dataclass(frozen=True)
class Merkmal:
    gebiet: str
    rang: float
    konfession: str
    kultur: str
    dichte: float | None
    zeile: int


@dataclass
class Ort:
    kennung: str
    datei: str
    flaeche: float
    stadt: bool
    gelaende: str
    raum: str
    konfession: str
    kultur: str
    gewicht: float
    fest: int
    einwohner: int = 0


def lies_merkmale(zeilen: list[dict[str, str]]) -> tuple[list[Merkmal], list[str]]:
    merkmale, fehler = [], []
    for z in zeilen:
        nr = int(z["_zeile"])
        rang = rang_merkmal(z["gebiet"])
        if rang == 0:
            fehler.append(f"merkmale_1846.csv:{nr}: ungültiges Gebiet {z['gebiet']!r}")
            continue
        for spalte in ("konfession", "kultur"):
            if z[spalte] and not RE_ANTEILE.fullmatch(z[spalte]):
                fehler.append(f"merkmale_1846.csv:{nr}: ungültige Anteile in Spalte {spalte!r}: {z[spalte]!r}")
        dichte = None
        if z["dichte"]:
            try:
                dichte = float(z["dichte"])
            except ValueError:
                fehler.append(f"merkmale_1846.csv:{nr}: ungültige Dichte {z['dichte']!r}")
        merkmale.append(Merkmal(z["gebiet"], rang, z["konfession"], z["kultur"], dichte, nr))
    return merkmale, fehler


def schaetzen(
    orte: list[dict[str, str]],
    codes: dict[str, dict[str, str]],
    zuordnung: dict[str, zu.Zuordnung],
    ueber: dict[str, str],
    summen: dict[str, int],
    staedte: dict[str, int],
    merkmale: list[Merkmal],
) -> tuple[list[Ort], list[str]]:
    """Schätzt die Einwohner aller Orte mit Besitzer. ``ueber``: Einheit → übergeordnete Einheit."""
    fehler: list[str] = []
    nach_gebiet = {m.gebiet: m for m in merkmale}
    ergebnis: list[Ort] = []
    for ort in orte:
        z = zuordnung.get(ort["id"])
        if z is None or z.staat is None:
            continue
        kette = []
        einheit = z.verwaltung
        while einheit:
            kette.append(einheit)
            einheit = ueber.get(einheit, "")
        raum = next((r for r in [*kette, z.staat] if r in summen), None)
        if raum is None:
            fehler.append(f"Ort {ort['id']}: keine Einwohnersumme für {z.staat} oder seine Verwaltung (einwohner_1846.csv)")
            continue
        gebiete = zu.gebiete_des_orts(ort, codes[ort["id"]]) + [f"verwaltung:{e}" for e in kette] + [f"staat:{z.staat}"]
        passend = sorted((nach_gebiet[g] for g in gebiete if g in nach_gebiet), key=lambda m: m.rang, reverse=True)
        konfession = next((m.konfession for m in passend if m.konfession), "")
        kultur = next((m.kultur for m in passend if m.kultur), "")
        dichte = next((m.dichte for m in passend if m.dichte is not None), 1.0)
        if not konfession:
            fehler.append(f"Ort {ort['id']}: keine Konfession (merkmale_1846.csv)")
            continue
        stadt = ort["stadt"] == "ja"
        flaeche = float(ort["flaeche"])
        gewicht = LAND_DICHTE * flaeche * dichte * GELAENDE.get(ort["gelaende"], 1.0)
        fest = staedte.get(ort["id"], 0)
        if stadt and not fest:
            gewicht += STADT_FAKTOR * float(codes[ort["id"]]["einwohner_heute"] or 0) ** STADT_EXPONENT
        ergebnis.append(Ort(ort["id"], ort["datei"], flaeche, stadt, ort["gelaende"], raum, konfession, kultur, gewicht, fest))
    for kennung in staedte:
        if not any(o.kennung == kennung for o in ergebnis):
            fehler.append(f"staedte_1846.csv: {kennung} ist kein Ort mit Besitzer 1848")
    nach_raum: defaultdict[str, list[Ort]] = defaultdict(list)
    for o in ergebnis:
        nach_raum[o.raum].append(o)
    for raum, summe in summen.items():
        glieder = nach_raum.get(raum, [])
        if not glieder:
            fehler.append(f"einwohner_1846.csv: Raum {raum} hat keine Orte")
            continue
        fest = sum(o.fest for o in glieder)
        gewicht = sum(o.gewicht for o in glieder)
        rest = summe - fest
        if rest <= 0 or gewicht <= 0:
            fehler.append(f"Raum {raum}: Summe {summe} reicht nicht für die festen Städte ({fest})")
            continue
        faktor = rest / gewicht
        for o in glieder:
            o.einwohner = max(1, o.fest + round(faktor * o.gewicht))
        # Rundungsrest auf den größten Ort, damit die Summe genau stimmt.
        groesster = max(glieder, key=lambda o: o.einwohner)
        groesster.einwohner += summe - sum(o.einwohner for o in glieder)
    return ergebnis, fehler


def bevoelkerung_zeilen(orte: list[Ort], quellen: dict[str, str]) -> list[list[str]]:
    zeilen = []
    for o in orte:
        quelle = f"Schätzung M7 (tools/geodaten/bevoelkerung_1846.py) aus der Summe für {o.raum}: {quellen[o.raum]}"
        zeilen.append([o.kennung, STICHTAG, str(o.einwohner), o.konfession, o.kultur, "0", "schaetzung", quelle])
    return zeilen


def neue_tabelle(alt: str, neue: list[list[str]]) -> str:
    zeilen = list(csv.reader(io.StringIO(alt, newline=""), delimiter=";"))
    if not zeilen or tuple(zeilen[0]) != BEVOELKERUNG_SPALTEN:
        raise zu.ZuordnungFehler(f"bevoelkerung.csv: Kopfzeile erwartet {';'.join(BEVOELKERUNG_SPALTEN)}")
    behalten = [z for z in zeilen[1:] if z and z[1] != STICHTAG]
    return zu.csv_text(BEVOELKERUNG_SPALTEN, behalten + neue)


def lade(tabellen: Path) -> tuple[list[Ort], dict[str, str], list[str]]:
    orte, ergebnis, _ = zu.lade(tabellen, tabellen / zu.REGELN.name, zu.GEBIETSCODES)
    fehler = list(ergebnis.fehler)
    codes = {z["ort"]: z for z in zu.lies_csv(zu.GEBIETSCODES, zu.CODE_SPALTEN)}
    ueber = {z["id"]: z["ueber"] for z in zu.lies_csv(tabellen / "verwaltung.csv")}
    summen, quellen = {}, {}
    for z in zu.lies_csv(tabellen / EINWOHNER.name, EINWOHNER_SPALTEN):
        summen[z["raum"]] = int(z["einwohner"])
        quellen[z["raum"]] = z["quelle"]
    staedte = {z["ort"]: int(z["einwohner"]) for z in zu.lies_csv(tabellen / STAEDTE.name, STAEDTE_SPALTEN)}
    merkmale, f = lies_merkmale(zu.lies_csv(tabellen / MERKMALE.name, MERKMAL_SPALTEN))
    fehler += f
    geschaetzt, f = schaetzen(orte, codes, ergebnis.zuordnung, ueber, summen, staedte, merkmale)
    fehler += f
    return geschaetzt, quellen, fehler


def bericht(orte: list[Ort]) -> str:
    summe: defaultdict[str, int] = defaultdict(int)
    anzahl: defaultdict[str, int] = defaultdict(int)
    flaeche: defaultdict[str, float] = defaultdict(float)
    for o in orte:
        summe[o.raum] += o.einwohner
        anzahl[o.raum] += 1
        flaeche[o.raum] += o.flaeche
    zeilen = [f"{'Raum':<22} {'Orte':>5} {'Einwohner':>11} {'je km²':>7}"]
    for raum in sorted(summe, key=lambda r: -summe[r]):
        zeilen.append(f"{raum:<22} {anzahl[raum]:>5} {summe[raum]:>11,} {summe[raum] / flaeche[raum]:>7.1f}".replace(",", "."))
    zeilen += ["", "Größte Orte:"]
    for o in sorted(orte, key=lambda o: -o.einwohner)[:25]:
        zeilen.append(f"  {o.kennung:<28} {o.einwohner:>9,} ({o.raum})".replace(",", "."))
    return "\n".join(zeilen)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="bevoelkerung_1846.py", description="Bevölkerung der Orte zum 3. Dezember 1846 (Schätzung).")
    parser.add_argument("--tabellen", type=Path, default=zu.TABELLEN)
    gruppe = parser.add_mutually_exclusive_group()
    gruppe.add_argument("--pruefen", action="store_true", help="nur prüfen, ob bevoelkerung.csv aktuell ist")
    gruppe.add_argument("--bericht", action="store_true", help="Summen je Raum und größte Orte")
    args = parser.parse_args(argv)
    try:
        orte, quellen, fehler = lade(args.tabellen)
    except zu.ZuordnungFehler as f:
        print(f"Fehler: {f}", file=sys.stderr)
        return 2
    for text in fehler:
        print(f"Fehler: {text}", file=sys.stderr)
    if fehler:
        print(f"Abbruch: {len(fehler)} Fehler.", file=sys.stderr)
        return 2
    if args.bericht:
        print(bericht(orte))
        return 0
    pfad = args.tabellen / "bevoelkerung.csv"
    alt = pfad.read_text(encoding="utf-8")
    neu = neue_tabelle(alt, bevoelkerung_zeilen(orte, quellen))
    if args.pruefen:
        if alt != neu:
            print("bevoelkerung.csv ist nicht aktuell – bitte python3 tools/geodaten/bevoelkerung_1846.py ausführen.")
            return 1
        print("bevoelkerung.csv ist aktuell.")
        return 0
    pfad.write_text(neu, encoding="utf-8")
    print(f"bevoelkerung.csv: {len(orte)} Zeilen zum {STICHTAG}, zusammen {sum(o.einwohner for o in orte):,} Einwohner.".replace(",", "."))
    return 0


if __name__ == "__main__":
    sys.exit(main())
