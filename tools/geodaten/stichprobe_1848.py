#!/usr/bin/env python3
"""Stichprobe der Daten zum Lesezeichen 1848 (Meilenstein M7, Entscheidung 7).

Zieht mit festem Seed 60 Datensätze – 20 aus ``besitz.csv`` (Stichtag 1848.03.01), 20 aus
``bevoelkerung.csv`` (1846.12.03), 10 Verwaltungseinheiten und 10 Felder der Staatshistorie zum
1. März 1848 – und gibt sie als Markdown-Tabellen aus, dazu Summen je Staat (Orte, Fläche,
Einwohner) für den Vergleich mit Literaturwerten. Das Ergebnis mit den Prüfvermerken steht in
``docs/pruefung/stichprobe_1848.md``; die Ziehung ist reproduzierbar, damit eine spätere Prüfung
dieselben Datensätze ansehen kann.

Aufruf::

    python3 tools/geodaten/stichprobe_1848.py [--seed 1848]
"""

from __future__ import annotations

import argparse
import random
import sys
from collections import defaultdict
from collections.abc import Sequence
from pathlib import Path

try:
    import zuordnung as zu
except ImportError:  # Aufruf als Modul von außerhalb des Werkzeugordners
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import zuordnung as zu

SEED = 1848
STICHTAG_BESITZ = "1848.03.01"
STICHTAG_BEVOELKERUNG = "1846.12.03"


def zahl(wert: float) -> str:
    """Ganze Zahl mit deutschem Tausenderpunkt, z. B. 1.234.567."""
    return f"{round(wert):,}".replace(",", ".")


def kette(einheit: str, einheiten: dict[str, dict[str, str]]) -> str:
    """Verwaltungskette von unten nach oben, z. B. „Kreis Teltow · Regierungsbezirk Potsdam · …“."""
    namen = []
    while einheit:
        namen.append(einheiten[einheit]["name"])
        einheit = einheiten[einheit]["ueber"]
    return " · ".join(namen)


def ziehe(tabellen: Path, seed: int) -> dict[str, list[dict[str, str]]]:
    zufall = random.Random(seed)
    besitz = [z for z in zu.lies_csv(tabellen / "besitz.csv") if z["ab"] == STICHTAG_BESITZ]
    bevoelkerung = [z for z in zu.lies_csv(tabellen / "bevoelkerung.csv") if z["stichtag"] == STICHTAG_BEVOELKERUNG]
    verwaltung = zu.lies_csv(tabellen / "verwaltung.csv")
    historie = [z for z in zu.lies_csv(tabellen / "staaten_historie.csv") if z["sicherheit"] != "spielwert"]
    historie = [z for z in historie if z["ab"] <= STICHTAG_BESITZ and z["ab"] > "1700"]
    return {
        "besitz": sorted(zufall.sample(besitz, 20), key=lambda z: int(z["_zeile"])),
        "bevoelkerung": sorted(zufall.sample(bevoelkerung, 20), key=lambda z: int(z["_zeile"])),
        "verwaltung": sorted(zufall.sample(verwaltung, 10), key=lambda z: int(z["_zeile"])),
        "staaten": sorted(zufall.sample(historie, 10), key=lambda z: int(z["_zeile"])),
    }


def markdown(tabellen: Path, seed: int) -> str:
    orte = {z["id"]: z for z in zu.lies_csv(tabellen / "orte.csv")}
    einheiten = {z["id"]: z for z in zu.lies_csv(tabellen / "verwaltung.csv")}
    staaten = {z["tag"]: z for z in zu.lies_csv(tabellen / "staaten.csv")}
    probe = ziehe(tabellen, seed)
    zeilen = [f"Seed {seed}; Zeilennummern der Tabellen in data/tabellen/.", ""]
    zeilen += [
        "### Besitz, Bundesgebiet, Verwaltung (besitz.csv)",
        "",
        "| Nr. | Zeile | Ort | Staat | Bund | Verwaltung | Prüfvermerk |",
        "|---|---|---|---|---|---|---|",
    ]
    for nr, z in enumerate(probe["besitz"], start=1):
        zeilen.append(
            f"| B{nr} | {z['_zeile']} | {orte[z['ort']]['name']} | {z['besitzer']} | {z['bund'] or 'nein'} | "
            f"{kette(z['verwaltung'], einheiten) if z['verwaltung'] else '–'} |  |"
        )
    zeilen += [
        "",
        "### Bevölkerung 1846 (bevoelkerung.csv)",
        "",
        "| Nr. | Zeile | Ort | Einwohner | je km² | Konfession | Kultur | Prüfvermerk |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for nr, z in enumerate(probe["bevoelkerung"], start=1):
        ort = orte[z["ort"]]
        kultur = z["kultur"] or f"Standard ({ort['datei']})"
        dichte = int(z["bevoelkerung"]) / float(ort["flaeche"])
        zeilen.append(
            f"| E{nr} | {z['_zeile']} | {ort['name']} | {zahl(int(z['bevoelkerung']))} | {zahl(dichte)} | "
            f"{z['konfession']} | {kultur} |  |"
        )
    zeilen += [
        "",
        "### Verwaltungseinheiten (verwaltung.csv)",
        "",
        "| Nr. | Zeile | Einheit | Art | darüber | Sitz | Prüfvermerk |",
        "|---|---|---|---|---|---|---|",
    ]
    for nr, z in enumerate(probe["verwaltung"], start=1):
        ueber = einheiten[z["ueber"]]["name"] if z["ueber"] else z["staat"]
        sitz = orte[z["sitz"]]["name"] if z["sitz"] else "–"
        zeilen.append(f"| V{nr} | {z['_zeile']} | {z['name']} | {z['art']} | {ueber} | {sitz} |  |")
    zeilen += [
        "",
        "### Staaten (staaten_historie.csv, ohne Spielwerte)",
        "",
        "| Nr. | Zeile | Staat | ab | Feld | Wert | Prüfvermerk |",
        "|---|---|---|---|---|---|---|",
    ]
    for nr, z in enumerate(probe["staaten"], start=1):
        zeilen.append(
            f"| S{nr} | {z['_zeile']} | {staaten[z['tag']]['kurzname']} ({z['tag']}) | {z['ab']} | {z['feld']} | {z['wert']} |  |"
        )
    return "\n".join(zeilen)


def summen(tabellen: Path) -> str:
    orte = {z["id"]: z for z in zu.lies_csv(tabellen / "orte.csv")}
    staaten = {z["tag"]: z for z in zu.lies_csv(tabellen / "staaten.csv")}
    besitzer = {z["ort"]: z["besitzer"] for z in zu.lies_csv(tabellen / "besitz.csv") if z["ab"] == STICHTAG_BESITZ}
    anzahl: defaultdict[str, int] = defaultdict(int)
    flaeche: defaultdict[str, float] = defaultdict(float)
    einwohner: defaultdict[str, int] = defaultdict(int)
    for kennung, staat in besitzer.items():
        anzahl[staat] += 1
        flaeche[staat] += float(orte[kennung]["flaeche"])
    for z in zu.lies_csv(tabellen / "bevoelkerung.csv"):
        if z["stichtag"] == STICHTAG_BEVOELKERUNG:
            einwohner[besitzer[z["ort"]]] += int(z["bevoelkerung"])
    zeilen = ["| Staat | Orte | Fläche (km²) | Einwohner 1846 |", "|---|---|---|---|"]
    for staat in sorted(anzahl, key=lambda s: -flaeche[s]):
        zeilen.append(
            f"| {staaten[staat]['kurzname']} ({staat}) | {anzahl[staat]} | {zahl(flaeche[staat])} | {zahl(einwohner[staat])} |"
        )
    return "\n".join(zeilen)


def summen_verwaltung(tabellen: Path, staaten: tuple[str, ...] = ("PRU", "AUT")) -> str:
    """Summen je oberster Verwaltungseinheit (Provinz, Kronland) für die genannten Staaten."""
    orte = {z["id"]: z for z in zu.lies_csv(tabellen / "orte.csv")}
    einheiten = {z["id"]: z for z in zu.lies_csv(tabellen / "verwaltung.csv")}
    einwohner = {
        z["ort"]: int(z["bevoelkerung"])
        for z in zu.lies_csv(tabellen / "bevoelkerung.csv")
        if z["stichtag"] == STICHTAG_BEVOELKERUNG
    }
    anzahl: defaultdict[str, int] = defaultdict(int)
    flaeche: defaultdict[str, float] = defaultdict(float)
    summe: defaultdict[str, int] = defaultdict(int)
    for z in zu.lies_csv(tabellen / "besitz.csv"):
        if z["ab"] != STICHTAG_BESITZ or z["besitzer"] not in staaten or not z["verwaltung"]:
            continue
        oben = z["verwaltung"]
        while einheiten[oben]["ueber"]:
            oben = einheiten[oben]["ueber"]
        anzahl[oben] += 1
        flaeche[oben] += float(orte[z["ort"]]["flaeche"])
        summe[oben] += einwohner.get(z["ort"], 0)
    zeilen = ["| Einheit | Staat | Orte | Fläche (km²) | Einwohner 1846 |", "|---|---|---|---|---|"]
    for e in sorted(anzahl, key=lambda e: (einheiten[e]["staat"], -flaeche[e])):
        zeilen.append(
            f"| {einheiten[e]['name']} | {einheiten[e]['staat']} | {anzahl[e]} | {zahl(flaeche[e])} | {zahl(summe[e])} |"
        )
    return "\n".join(zeilen)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="stichprobe_1848.py", description="Stichprobe der Daten 1848.")
    parser.add_argument("--seed", type=int, default=SEED)
    parser.add_argument("--tabellen", type=Path, default=zu.TABELLEN)
    args = parser.parse_args(argv)
    print(markdown(args.tabellen, args.seed))
    print()
    print(summen(args.tabellen))
    print()
    print(summen_verwaltung(args.tabellen))
    return 0


if __name__ == "__main__":
    sys.exit(main())
