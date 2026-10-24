#!/usr/bin/env python3
"""Besitz, Bundesgebiet und Verwaltung der Orte 1849–1870 (Meilenstein M10, Schritt b1).

Die Kriegslesezeichen 1864, 1866 und 1870 nutzen die Daten von 1848 und die datierte Historie bis zu
ihrem Datum (docs/technik/militaer.md, Abschnitt 6.2). Dieses Werkzeug schreibt die Historie: Es
beginnt mit der Zuordnung zum 1. März 1848 (die Zeilen von ``zuordnung.py`` in ``besitz.csv``) und
wendet die Änderungen aus ``data/tabellen/aenderungen_1849_1870.csv`` (von Hand gepflegt) in
zeitlicher Folge an::

    ab;gebiet;staat;bund;verwaltung;sicherheit;quelle

``gebiet`` nimmt alle Formen von ``territorien_1848.csv`` (``ort:``, ``landschaft:``, heutige Codes)
und dazu, bezogen auf den Stand unmittelbar vor ``ab``:

=========================  ================================================  =====
Form                       Bedeutung                                         Rang
=========================  ================================================  =====
``verwaltung:<einheit>``   Orte in der Einheit oder darunter                 2,5
``staat:<TAG>``            Orte des Staates                                  1,5
``alle``                   alle Orte mit Daten                               0,5
=========================  ================================================  =====

Je Datum gilt für jedes der drei Felder die stärkste passende Regel, die es setzt (leer = setzt es
nicht). ``verwaltung = -`` hebt die Einheit auf; wechselt der Staat ohne Regel für die Verwaltung,
bleibt die Einheit nur, wenn sie zum neuen Staat gehört. Ein Ort ohne Daten, der einen Staat erhält
(Frankreich ab 1849), bekommt seine Bevölkerung wie 1846 geschätzt (``bevoelkerung_1846.py``) aus
``einwohner_1849_1870.csv``, ``staedte_1849_1870.csv`` und ``merkmale_1849_1870.csv`` (gleiche Spalten
wie die Tabellen von 1846), mit dem Stichtag seiner Änderung.

Aufruf::

    python3 tools/geodaten/historie_1849_1870.py             # besitz.csv und bevoelkerung.csv: Zeilen 1849–1870 neu
    python3 tools/geodaten/historie_1849_1870.py --pruefen   # nur vergleichen (Exitcode 1, wenn veraltet)
    python3 tools/geodaten/historie_1849_1870.py --bericht   # Orte je Staat zu jedem Datum

Nach ``zuordnung.py`` und ``bevoelkerung_1846.py``, vor ``tools/archivar/archivar.py``. Exitcodes:
0 = in Ordnung, 1 = ``--pruefen``: Tabellen nicht aktuell, 2 = Fehler in den Regeln.
"""

from __future__ import annotations

import argparse
import csv
import io
import sys
from collections import Counter, defaultdict
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

try:
    import bevoelkerung_1846 as bv
    import zuordnung as zu
except ImportError:  # Aufruf als Modul von außerhalb des Werkzeugordners
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import bevoelkerung_1846 as bv
    import zuordnung as zu

VON = "1849.01.01"  # erste Änderung dieses Werkzeugs
BIS = "1870.12.31"  # letzte
AENDERUNGEN = zu.TABELLEN / "aenderungen_1849_1870.csv"
EINWOHNER = zu.TABELLEN / "einwohner_1849_1870.csv"
STAEDTE = zu.TABELLEN / "staedte_1849_1870.csv"
MERKMALE = zu.TABELLEN / "merkmale_1849_1870.csv"
AENDERUNG_SPALTEN = ("ab", "gebiet", "staat", "bund", "verwaltung", "sicherheit", "quelle")


def datum_schluessel(text: str) -> tuple[int, int, int]:
    teile = text.split(".")
    if len(teile) != 3 or not all(t.isdigit() for t in teile):
        raise zu.ZuordnungFehler(f"ungültiges Datum {text!r} (erwartet JJJJ.MM.TT)")
    return int(teile[0]), int(teile[1]), int(teile[2])


def im_zeitraum(datum: str) -> bool:
    return datum_schluessel(VON) <= datum_schluessel(datum) <= datum_schluessel(BIS)


@dataclass(frozen=True)
class Aenderung:
    ab: str
    gebiet: str
    rang: float
    staat: str
    bund: str
    verwaltung: str
    sicherheit: str
    quelle: str
    zeile: int


def rang_von(gebiet: str) -> float:
    if gebiet == "alle":
        return 0.5
    if gebiet.startswith("staat:"):
        return 1.5
    if gebiet.startswith("verwaltung:"):
        return 2.5
    return float(zu.rang_von(gebiet))


def lies_aenderungen(zeilen: list[dict[str, str]]) -> tuple[list[Aenderung], list[str]]:
    regeln: list[Aenderung] = []
    fehler: list[str] = []
    gesehen: dict[tuple[str, str], int] = {}
    for z in zeilen:
        nr = int(z["_zeile"])
        ort = f"{AENDERUNGEN.name}:{nr}"
        try:
            if not im_zeitraum(z["ab"]):
                fehler.append(f"{ort}: Datum {z['ab']} liegt nicht zwischen {VON} und {BIS}")
                continue
        except zu.ZuordnungFehler as f:
            fehler.append(f"{ort}: {f}")
            continue
        rang = rang_von(z["gebiet"])
        if rang == 0:
            fehler.append(f"{ort}: ungültiges Gebiet {z['gebiet']!r}")
            continue
        schluessel = (z["ab"], z["gebiet"])
        if schluessel in gesehen:
            fehler.append(f"{ort}: Gebiet {z['gebiet']} zum {z['ab']} ist doppelt (zuerst in Zeile {gesehen[schluessel]})")
            continue
        gesehen[schluessel] = nr
        if z["staat"] == "-":
            fehler.append(f"{ort}: ein Ort kann seine Daten nicht verlieren (staat = -)")
            continue
        if z["bund"] not in ("", "ja", "nein"):
            fehler.append(f"{ort}: Spalte 'bund' erwartet ja, nein oder nichts, nicht {z['bund']!r}")
            continue
        if not (z["staat"] or z["bund"] or z["verwaltung"]):
            fehler.append(f"{ort}: Regel für {z['gebiet']} setzt kein Feld")
            continue
        if z["sicherheit"] not in zu.SICHERHEIT:
            fehler.append(f"{ort}: ungültige Sicherheit {z['sicherheit']!r} (erlaubt: {', '.join(zu.SICHERHEIT)})")
            continue
        if not z["quelle"]:
            fehler.append(f"{ort}: Spalte 'quelle' darf nicht leer sein")
            continue
        regeln.append(
            Aenderung(z["ab"], z["gebiet"], rang, z["staat"], z["bund"], z["verwaltung"], z["sicherheit"], z["quelle"], nr)
        )
    regeln.sort(key=lambda r: (datum_schluessel(r.ab), r.zeile))
    return regeln, fehler


@dataclass(frozen=True)
class Stand:
    """Zugehörigkeit eines Ortes zu einem Datum; staat None = Ort ohne Daten."""

    staat: str | None
    verwaltung: str | None
    bund: bool


def stand_1848(orte: list[dict[str, str]], besitz: list[dict[str, str]]) -> dict[str, Stand]:
    """Stand zum Stichtag von zuordnung.py: die letzte Besitzzeile bis dahin je Ort."""
    grenze = datum_schluessel(zu.STICHTAG)
    letzte: dict[str, dict[str, str]] = {}
    for z in besitz:
        if datum_schluessel(z["ab"]) <= grenze:
            if z["ort"] not in letzte or datum_schluessel(letzte[z["ort"]]["ab"]) <= datum_schluessel(z["ab"]):
                letzte[z["ort"]] = z
    stand = {}
    for ort in orte:
        z = letzte.get(ort["id"])
        stand[ort["id"]] = Stand(None, None, False) if z is None else Stand(z["besitzer"], z["verwaltung"] or None, z["bund"] == "ja")
    return stand


def kette(einheit: str | None, ueber: dict[str, str]) -> list[str]:
    ergebnis = []
    while einheit and einheit not in ergebnis:
        ergebnis.append(einheit)
        einheit = ueber.get(einheit, "")
    return ergebnis


@dataclass
class Ergebnis:
    besitz: list[list[str]]  # neue Zeilen für besitz.csv
    neu: dict[str, tuple[str, Stand, Aenderung]]  # Orte, die Daten erhalten: Datum, Stand, Regel
    staende: dict[str, dict[str, Stand]]  # je Datum der Stand danach
    fehler: list[str]
    warnungen: list[str]


def anwenden(
    orte: list[dict[str, str]],
    codes: dict[str, dict[str, str]],
    regeln: list[Aenderung],
    start: dict[str, Stand],
    staaten: set[str],
    einheiten: dict[str, str],
    ueber: dict[str, str],
) -> Ergebnis:
    fehler: list[str] = []
    warnungen: list[str] = []
    for r in regeln:
        if r.staat and r.staat not in staaten:
            fehler.append(f"Zeile {r.zeile}: unbekannter Staat {r.staat!r} (staaten.csv)")
        if r.verwaltung not in ("", "-") and r.verwaltung not in einheiten:
            fehler.append(f"Zeile {r.zeile}: unbekannte Verwaltungseinheit {r.verwaltung!r} (verwaltung.csv)")
        if r.gebiet.startswith("staat:") and r.gebiet.removeprefix("staat:") not in staaten:
            fehler.append(f"Zeile {r.zeile}: unbekannter Staat in {r.gebiet!r}")
        if r.gebiet.startswith("verwaltung:") and r.gebiet.removeprefix("verwaltung:") not in einheiten:
            fehler.append(f"Zeile {r.zeile}: unbekannte Verwaltungseinheit in {r.gebiet!r}")
    stand = dict(start)
    besitz: list[list[str]] = []
    neu: dict[str, tuple[str, Stand, Aenderung]] = {}
    staende: dict[str, dict[str, Stand]] = {}
    benutzt: set[int] = set()
    nach_datum: dict[str, list[Aenderung]] = defaultdict(list)
    for r in regeln:
        nach_datum[r.ab].append(r)
    for datum in sorted(nach_datum, key=datum_schluessel):
        nach_gebiet = {r.gebiet: r for r in nach_datum[datum]}
        vorher = stand
        stand = dict(vorher)
        for ort in orte:
            kennung = ort["id"]
            alt = vorher[kennung]
            gebiete = zu.gebiete_des_orts(ort, codes[kennung]) if kennung in codes else [f"ort:{kennung}", f"landschaft:{ort['datei']}"]
            if alt.staat is not None:
                gebiete += [f"staat:{alt.staat}", "alle"] + [f"verwaltung:{e}" for e in kette(alt.verwaltung, ueber)]
            passend = sorted((nach_gebiet[g] for g in gebiete if g in nach_gebiet), key=lambda r: r.rang, reverse=True)
            if not passend:
                continue
            benutzt.update(r.zeile for r in passend)
            wahl = {feld: next((r for r in passend if getattr(r, feld)), None) for feld in zu.FELDER}
            staat = wahl["staat"].staat if wahl["staat"] else alt.staat
            if staat is None:
                continue  # Ort ohne Daten, keine Regel für den Staat (z. B. nur 'bund')
            bund = (wahl["bund"].bund == "ja") if wahl["bund"] else alt.bund
            if wahl["verwaltung"]:
                verwaltung = None if wahl["verwaltung"].verwaltung == "-" else wahl["verwaltung"].verwaltung
            else:
                verwaltung = alt.verwaltung if alt.verwaltung and einheiten.get(alt.verwaltung) == staat else None
            if verwaltung is not None and einheiten.get(verwaltung) != staat:
                fehler.append(
                    f"Ort {kennung} zum {datum}: Verwaltungseinheit {verwaltung} gehört zu {einheiten.get(verwaltung)}, "
                    f"der Ort zu {staat}"
                )
                continue
            neuer = Stand(staat, verwaltung, bund)
            if neuer == alt:
                continue
            stand[kennung] = neuer
            regel = wahl["staat"] or wahl["verwaltung"] or wahl["bund"]
            assert regel is not None
            if alt.staat is None:
                neu[kennung] = (datum, neuer, regel)
            quelle = f"{regel.quelle} (Regel {regel.gebiet}, {AENDERUNGEN.name})"
            besitz.append([kennung, datum, staat, verwaltung or "", "ja" if bund else "", regel.sicherheit, quelle])
        staende[datum] = stand
    for r in regeln:
        if r.zeile not in benutzt:
            warnungen.append(f"Zeile {r.zeile}: Regel für {r.gebiet} zum {r.ab} passt auf keinen Ort")
    return Ergebnis(besitz, neu, staende, fehler, warnungen)


def schaetze_neue(
    tabellen: Path, orte: list[dict[str, str]], codes: dict[str, dict[str, str]], ueber: dict[str, str], ergebnis: Ergebnis
) -> tuple[list[list[str]], list[str]]:
    """Bevölkerung der Orte, die Daten erhalten (wie bevoelkerung_1846.py), zum Datum ihrer Änderung."""
    if not ergebnis.neu:
        return [], []
    fehler: list[str] = []
    einwohner = zu.lies_csv(tabellen / EINWOHNER.name, bv.EINWOHNER_SPALTEN)
    staedte_zeilen = zu.lies_csv(tabellen / STAEDTE.name, bv.STAEDTE_SPALTEN)
    fehler += zu.eindeutig(einwohner, "raum", EINWOHNER.name) + zu.eindeutig(staedte_zeilen, "ort", STAEDTE.name)
    summen = {z["raum"]: int(z["einwohner"]) for z in einwohner}
    staedte = {z["ort"]: int(z["einwohner"]) for z in staedte_zeilen}
    quellen = {z["raum"]: z["quelle"] for z in einwohner}
    merkmale, f = bv.lies_merkmale(zu.lies_csv(tabellen / MERKMALE.name, bv.MERKMAL_SPALTEN))
    fehler += [t.replace(bv.MERKMALE.name, MERKMALE.name) for t in f]
    zuordnung = {k: zu.Zuordnung(s.staat, s.bund, s.verwaltung, None) for k, (_, s, _) in ergebnis.neu.items()}
    geschaetzt, f = bv.schaetzen(orte, codes, zuordnung, ueber, summen, staedte, merkmale)
    fehler += [t.replace(bv.MERKMALE.name, MERKMALE.name).replace(bv.STAEDTE.name, STAEDTE.name).replace(bv.EINWOHNER.name, EINWOHNER.name) for t in f]
    zeilen = []
    for o in geschaetzt:
        datum = ergebnis.neu[o.kennung][0]
        quelle = f"Schätzung M10 (tools/geodaten/historie_1849_1870.py) aus der Summe für {o.raum}: {quellen[o.raum]}"
        zeilen.append([o.kennung, datum, str(o.einwohner), o.konfession, o.kultur, "0", "schaetzung", quelle])
    return zeilen, fehler


def neue_tabelle(alt: str, spalten: Sequence[str], datum_spalte: int, neue: list[list[str]]) -> str:
    """Tabelle mit allen Zeilen außerhalb des Zeitraums (in ihrer Reihenfolge), dahinter die neuen."""
    zeilen = list(csv.reader(io.StringIO(alt, newline=""), delimiter=";"))
    if not zeilen or tuple(zeilen[0]) != tuple(spalten):
        raise zu.ZuordnungFehler(f"Kopfzeile erwartet {';'.join(spalten)}")
    behalten = [z for z in zeilen[1:] if z and not im_zeitraum(z[datum_spalte])]
    return zu.csv_text(spalten, behalten + neue)


def bericht(ergebnis: Ergebnis, start: dict[str, Stand]) -> str:
    zeilen = []
    vorher = Counter(s.staat for s in start.values() if s.staat)
    for datum, stand in ergebnis.staende.items():
        jetzt = Counter(s.staat for s in stand.values() if s.staat)
        bund = sum(1 for s in stand.values() if s.staat and s.bund)
        aenderung = ", ".join(
            f"{t} {vorher.get(t, 0)}→{jetzt.get(t, 0)}" for t in sorted(set(vorher) | set(jetzt)) if vorher.get(t, 0) != jetzt.get(t, 0)
        )
        zeilen.append(f"{datum}: {aenderung or 'Besitz unverändert'}; {bund} Orte im Bund")
        vorher = jetzt
    return "\n".join(zeilen)


def lade(tabellen: Path) -> tuple[Ergebnis, dict[str, Stand], list[list[str]], list[str]]:
    orte = zu.lies_csv(tabellen / "orte.csv")
    codes = {z["ort"]: {k: z[k] for k in zu.CODE_SPALTEN[1:]} for z in zu.lies_csv(zu.GEBIETSCODES, zu.CODE_SPALTEN)}
    staaten = {z["tag"] for z in zu.lies_csv(tabellen / "staaten.csv")}
    verwaltung = zu.lies_csv(tabellen / "verwaltung.csv")
    einheiten = {z["id"]: z["staat"] for z in verwaltung}
    ueber = {z["id"]: z["ueber"] for z in verwaltung}
    besitz = zu.lies_csv(tabellen / "besitz.csv", zu.BESITZ_SPALTEN)
    regeln, fehler = lies_aenderungen(zu.lies_csv(tabellen / AENDERUNGEN.name, AENDERUNG_SPALTEN))
    start = stand_1848(orte, besitz)
    ergebnis = anwenden(orte, codes, regeln, start, staaten, einheiten, ueber)
    ergebnis.fehler[:0] = fehler
    bevoelkerung, f = schaetze_neue(tabellen, orte, codes, ueber, ergebnis)
    ergebnis.fehler += f
    return ergebnis, start, bevoelkerung, []


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="historie_1849_1870.py", description="Besitz der Orte 1849–1870 aus datierten Änderungen.")
    parser.add_argument("--tabellen", type=Path, default=zu.TABELLEN)
    gruppe = parser.add_mutually_exclusive_group()
    gruppe.add_argument("--pruefen", action="store_true", help="nur prüfen, ob besitz.csv und bevoelkerung.csv aktuell sind")
    gruppe.add_argument("--bericht", action="store_true", help="Orte je Staat zu jedem Datum")
    args = parser.parse_args(argv)
    try:
        ergebnis, start, bevoelkerung, _ = lade(args.tabellen)
    except (zu.ZuordnungFehler, ValueError) as f:
        print(f"Fehler: {f}", file=sys.stderr)
        return 2
    for text in ergebnis.warnungen:
        print(f"Warnung: {text}", file=sys.stderr)
    for text in ergebnis.fehler:
        print(f"Fehler: {text}", file=sys.stderr)
    if ergebnis.fehler:
        print(f"Abbruch: {len(ergebnis.fehler)} Fehler.", file=sys.stderr)
        return 2
    if args.bericht:
        print(bericht(ergebnis, start))
        return 0
    pfade = {
        "besitz.csv": (zu.BESITZ_SPALTEN, 1, ergebnis.besitz),
        "bevoelkerung.csv": (bv.BEVOELKERUNG_SPALTEN, 1, bevoelkerung),
    }
    neu: dict[str, tuple[str, str]] = {}
    try:
        for name, (spalten, datum_spalte, zeilen) in pfade.items():
            alt = (args.tabellen / name).read_text(encoding="utf-8-sig")
            neu[name] = (alt, neue_tabelle(alt, spalten, datum_spalte, zeilen))
    except (OSError, zu.ZuordnungFehler) as f:
        print(f"Fehler: {f}", file=sys.stderr)
        return 2
    if args.pruefen:
        veraltet = [name for name, (alt, text) in neu.items() if alt != text]
        if veraltet:
            print(f"{', '.join(veraltet)} nicht aktuell – bitte python3 tools/geodaten/historie_1849_1870.py ausführen.")
            return 1
        print("besitz.csv und bevoelkerung.csv sind für 1849–1870 aktuell.")
        return 0
    for name, (_, text) in neu.items():
        (args.tabellen / name).write_text(text, encoding="utf-8")
    print(
        f"besitz.csv: {len(ergebnis.besitz)} Zeilen 1849–1870, bevoelkerung.csv: {len(bevoelkerung)} Orte mit neuen Daten "
        f"({sum(int(z[2]) for z in bevoelkerung):,} Einwohner). Danach: tools/archivar/archivar.py".replace(",", ".")
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
