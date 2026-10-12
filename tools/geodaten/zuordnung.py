#!/usr/bin/env python3
"""Zugehörigkeit der Orte 1848 (Meilenstein M7): Besitzer, Bundesgebiet und Verwaltung.

Ohne Grenzgeometrie von 1848 entsteht die Zuordnung aus Regeln über die **heutigen
Verwaltungseinheiten** der Orte und über die **Landschaften** (Dateien in ``history/orte``),
dazu Ausnahmen je Ort (docs/technik/lesezeichen_1848.md, Abschnitt 5):

``data/tabellen/territorien_1848.csv`` (von Hand gepflegt)::

    gebiet;staat;bund;verwaltung;sicherheit;quelle

``gebiet`` ist eine der folgenden Formen, von der stärksten zur schwächsten:

========================  ==================================================  =====
Form                      Beispiel                                            Rang
========================  ==================================================  =====
``ort:<kennung>``         ``ort:coburg``                                      6
Kreis heute               ``DE.15.00.16066`` (Land.admin1.admin2.admin3)      5
Bezirk heute              ``DE.02.094`` (Land.admin1.admin2)                  4
``landschaft:<datei>``    ``landschaft:posen``                                3
Land/Bundesland heute     ``DE.02`` (Land.admin1)                             2
Staat heute               ``DE``                                              1
========================  ==================================================  =====

Jedes der drei Felder wird für sich aufgelöst: Es gilt der Wert der stärksten passenden Regel, die
das Feld setzt (leer = setzt es nicht). ``staat = -`` macht den Ort zum Ort ohne Daten,
``verwaltung = -`` hebt eine Einheit auf. Erbt ein Ort die Verwaltungseinheit aus einer
schwächeren Regel als seinen Staat und gehört die Einheit zu einem anderen Staat, entfällt sie
(z. B. Coburg in Oberfranken); bei gleich starker oder stärkerer Regel ist das ein Fehler.

Die heutigen Verwaltungseinheiten stehen mit der heutigen Einwohnerzahl (Gewicht für die
Bevölkerungsschätzung 1846) in ``tools/geodaten/gebietscodes.csv`` (erzeugt mit ``zuordnung.py
codes`` aus GeoNames cities1000 im Zwischenspeicher; Orte ohne geonameid – die 60 Orte der
Testregion – über den nächstgelegenen GeoNames-Eintrag).

Aufruf::

    python3 tools/geodaten/zuordnung.py               # besitz.csv: Zeilen zum Stichtag neu schreiben
    python3 tools/geodaten/zuordnung.py --pruefen     # nur vergleichen (Exitcode 1, wenn veraltet)
    python3 tools/geodaten/zuordnung.py --bericht     # Orte, Fläche und Bundesgebiet je Staat
    python3 tools/geodaten/zuordnung.py --liste PRU   # Orte eines Staates (Sichtprüfung)
    python3 tools/geodaten/zuordnung.py codes         # gebietscodes.csv aus GeoNames neu erzeugen

Danach: ``python3 tools/archivar/archivar.py``. Exitcodes: 0 = in Ordnung, 1 = ``--pruefen``:
besitz.csv nicht aktuell, 2 = Fehler in den Regeln.
"""

from __future__ import annotations

import argparse
import csv
import io
import math
import re
import sys
from collections import Counter, defaultdict
from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from pathlib import Path

HIER = Path(__file__).resolve().parent
REPO = HIER.parent.parent
TABELLEN = REPO / "data" / "tabellen"
REGELN = TABELLEN / "territorien_1848.csv"
GEBIETSCODES = HIER / "gebietscodes.csv"

STICHTAG = "1848.03.01"
REGEL_SPALTEN = ("gebiet", "staat", "bund", "verwaltung", "sicherheit", "quelle")
CODE_SPALTEN = ("ort", "land", "admin1", "admin2", "admin3", "einwohner_heute")
BESITZ_SPALTEN = ("ort", "ab", "besitzer", "verwaltung", "bund", "sicherheit", "quelle")
SICHERHEIT = ("gesichert", "schaetzung", "spielwert", "ungeprueft")
FELDER = ("staat", "bund", "verwaltung")
RE_GEBIET_CODE = re.compile(r"[A-Z]{2}(\.[A-Za-z0-9]+){0,3}")
RE_GEONAMEID = re.compile(r"geonameid (\d+)")


class ZuordnungFehler(Exception):
    """Fehler in Regeln oder Tabellen."""


# ─── Tabellen ───────────────────────────────────────────────────────────────


def lies_csv(pfad: Path, spalten: Sequence[str] | None = None) -> list[dict[str, str]]:
    """Semikolon-CSV mit Kopfzeile; prüft die Spalten, falls angegeben."""
    try:
        text = pfad.read_text(encoding="utf-8-sig")
    except OSError as fehler:
        raise ZuordnungFehler(f"{pfad}: {fehler}") from fehler
    zeilen = list(csv.reader(io.StringIO(text, newline=""), delimiter=";"))
    if not zeilen:
        raise ZuordnungFehler(f"{pfad}: leer")
    kopf = tuple(s.strip() for s in zeilen[0])
    if spalten is not None and kopf != tuple(spalten):
        raise ZuordnungFehler(f"{pfad}: Kopfzeile {';'.join(kopf)!r}, erwartet {';'.join(spalten)!r}")
    ergebnis = []
    for nr, z in enumerate(zeilen[1:], start=2):
        if not z or all(not s.strip() for s in z):
            continue
        if len(z) != len(kopf):
            raise ZuordnungFehler(f"{pfad}:{nr}: erwartet {len(kopf)} Spalten, gefunden {len(z)}")
        werte = {k: v.strip() for k, v in zip(kopf, z)}
        werte["_zeile"] = str(nr)
        ergebnis.append(werte)
    return ergebnis


def csv_text(spalten: Sequence[str], zeilen: Iterable[Sequence[str]]) -> str:
    """Semikolon-CSV wie in data/tabellen (minimal gequotet, LF)."""
    ausgabe = io.StringIO(newline="")
    schreiber = csv.writer(ausgabe, delimiter=";", lineterminator="\n", quoting=csv.QUOTE_MINIMAL)
    schreiber.writerow(spalten)
    schreiber.writerows(zeilen)
    return ausgabe.getvalue()


# ─── Regeln ─────────────────────────────────────────────────────────────────


@dataclass(frozen=True)
class Regel:
    gebiet: str
    rang: int
    staat: str  # "" = setzt nicht, "-" = Ort ohne Daten
    bund: str  # "", "ja", "nein"
    verwaltung: str  # "" = setzt nicht, "-" = keine
    sicherheit: str
    quelle: str
    zeile: int


def rang_von(gebiet: str) -> int:
    """Rang einer Gebietsangabe (6 = Ort … 1 = Staat heute); 0 = ungültig."""
    if gebiet.startswith("ort:"):
        return 6
    if gebiet.startswith("landschaft:"):
        return 3
    if not RE_GEBIET_CODE.fullmatch(gebiet):
        return 0
    return {0: 1, 1: 2, 2: 4, 3: 5}[gebiet.count(".")]


def lies_regeln(zeilen: list[dict[str, str]], anzeige: str = "territorien_1848.csv") -> tuple[list[Regel], list[str]]:
    regeln: list[Regel] = []
    fehler: list[str] = []
    gesehen: dict[str, int] = {}
    for z in zeilen:
        nr = int(z["_zeile"])
        ort = f"{anzeige}:{nr}"
        gebiet = z["gebiet"]
        rang = rang_von(gebiet)
        if rang == 0:
            fehler.append(f"{ort}: ungültiges Gebiet {gebiet!r}")
            continue
        if gebiet in gesehen:
            fehler.append(f"{ort}: Gebiet {gebiet} ist doppelt (zuerst in Zeile {gesehen[gebiet]})")
            continue
        gesehen[gebiet] = nr
        if z["bund"] not in ("", "ja", "nein"):
            fehler.append(f"{ort}: Spalte 'bund' erwartet ja, nein oder nichts, nicht {z['bund']!r}")
            continue
        if not (z["staat"] or z["bund"] or z["verwaltung"]):
            fehler.append(f"{ort}: Regel für {gebiet} setzt kein Feld")
            continue
        if z["sicherheit"] not in SICHERHEIT:
            fehler.append(f"{ort}: ungültige Sicherheit {z['sicherheit']!r} (erlaubt: {', '.join(SICHERHEIT)})")
            continue
        if not z["quelle"]:
            fehler.append(f"{ort}: Spalte 'quelle' darf nicht leer sein")
            continue
        regeln.append(Regel(gebiet, rang, z["staat"], z["bund"], z["verwaltung"], z["sicherheit"], z["quelle"], nr))
    return regeln, fehler


def gebiete_des_orts(ort: dict[str, str], codes: dict[str, str]) -> list[str]:
    """Alle Gebietsangaben, die auf einen Ort passen."""
    land, a1, a2, a3 = codes["land"], codes["admin1"], codes["admin2"], codes["admin3"]
    gebiete = [f"ort:{ort['id']}", f"landschaft:{ort['datei']}", land]
    if a1:
        gebiete.append(f"{land}.{a1}")
        if a2:
            gebiete.append(f"{land}.{a1}.{a2}")
            if a3:
                gebiete.append(f"{land}.{a1}.{a2}.{a3}")
    return gebiete


@dataclass(frozen=True)
class Zuordnung:
    """Ergebnis für einen Ort: Staat (None = Ort ohne Daten), Bundesgebiet, Verwaltungseinheit."""

    staat: str | None
    bund: bool
    verwaltung: str | None
    regel: Regel | None  # Regel, die den Staat festlegt


@dataclass
class Ergebnis:
    zuordnung: dict[str, Zuordnung]
    fehler: list[str]
    warnungen: list[str]


def aufloesen(
    orte: list[dict[str, str]],
    codes: dict[str, dict[str, str]],
    regeln: list[Regel],
    staaten: set[str],
    einheiten: dict[str, str],
) -> Ergebnis:
    """Löst die Regeln für alle Orte auf. ``einheiten``: Verwaltungseinheit → Staat."""
    nach_gebiet = {r.gebiet: r for r in regeln}
    fehler: list[str] = []
    warnungen: list[str] = []
    for r in regeln:
        if r.staat not in ("", "-") and r.staat not in staaten:
            fehler.append(f"Zeile {r.zeile}: unbekannter Staat {r.staat!r} (staaten.csv)")
        if r.verwaltung not in ("", "-") and r.verwaltung not in einheiten:
            fehler.append(f"Zeile {r.zeile}: unbekannte Verwaltungseinheit {r.verwaltung!r} (verwaltung.csv)")
    benutzt: set[str] = set()
    ergebnis: dict[str, Zuordnung] = {}
    for ort in orte:
        kennung = ort["id"]
        if kennung not in codes:
            fehler.append(f"Ort {kennung}: keine heutigen Verwaltungscodes (zuordnung.py codes)")
            continue
        passend = [nach_gebiet[g] for g in gebiete_des_orts(ort, codes[kennung]) if g in nach_gebiet]
        benutzt.update(r.gebiet for r in passend)
        passend.sort(key=lambda r: r.rang, reverse=True)
        wahl: dict[str, Regel | None] = {}
        for feld in FELDER:
            wahl[feld] = next((r for r in passend if getattr(r, feld)), None)
        staat_regel = wahl["staat"]
        if staat_regel is None:
            fehler.append(f"Ort {kennung} ({ort['datei']}, {'.'.join(v for v in codes[kennung].values() if v)}): keine Regel für den Staat")
            continue
        if staat_regel.staat == "-":
            ergebnis[kennung] = Zuordnung(None, False, None, staat_regel)
            continue
        staat = staat_regel.staat
        bund_regel = wahl["bund"]
        if bund_regel is None:
            fehler.append(f"Ort {kennung}: keine Regel für 'bund'")
            continue
        verwaltung = None
        v_regel = wahl["verwaltung"]
        if v_regel is not None and v_regel.verwaltung != "-":
            einheit_staat = einheiten.get(v_regel.verwaltung)
            if einheit_staat == staat:
                verwaltung = v_regel.verwaltung
            elif v_regel.rang >= staat_regel.rang and einheit_staat is not None:
                fehler.append(
                    f"Ort {kennung}: Verwaltungseinheit {v_regel.verwaltung} ({einheit_staat}, Regel {v_regel.gebiet}) "
                    f"passt nicht zum Staat {staat} (Regel {staat_regel.gebiet})"
                )
        ergebnis[kennung] = Zuordnung(staat, bund_regel.bund == "ja", verwaltung, staat_regel)
    for r in regeln:
        if r.gebiet not in benutzt:
            warnungen.append(f"Zeile {r.zeile}: Regel für {r.gebiet} passt auf keinen Ort")
    return Ergebnis(ergebnis, fehler, warnungen)


# ─── besitz.csv ─────────────────────────────────────────────────────────────


def besitz_zeilen(orte: list[dict[str, str]], ergebnis: Ergebnis) -> list[list[str]]:
    zeilen = []
    for ort in orte:
        z = ergebnis.zuordnung.get(ort["id"])
        if z is None or z.staat is None or z.regel is None:
            continue
        quelle = f"{z.regel.quelle} (Regel {z.regel.gebiet}, territorien_1848.csv)"
        zeilen.append([ort["id"], STICHTAG, z.staat, z.verwaltung or "", "ja" if z.bund else "", z.regel.sicherheit, quelle])
    return zeilen


def neue_besitz_tabelle(alt: str, neue: list[list[str]]) -> str:
    """besitz.csv mit allen Zeilen außer denen zum Stichtag, dahinter die neuen Zeilen."""
    zeilen = list(csv.reader(io.StringIO(alt, newline=""), delimiter=";"))
    if not zeilen or tuple(zeilen[0]) != BESITZ_SPALTEN:
        raise ZuordnungFehler(f"besitz.csv: Kopfzeile erwartet {';'.join(BESITZ_SPALTEN)}")
    behalten = [z for z in zeilen[1:] if z and z[1] != STICHTAG]
    return csv_text(BESITZ_SPALTEN, behalten + neue)


# ─── Bericht ────────────────────────────────────────────────────────────────


def bericht(orte: list[dict[str, str]], ergebnis: Ergebnis, namen: dict[str, str]) -> str:
    anzahl: Counter[str] = Counter()
    bund: Counter[str] = Counter()
    flaeche: defaultdict[str, float] = defaultdict(float)
    ohne = 0
    for ort in orte:
        z = ergebnis.zuordnung.get(ort["id"])
        if z is None or z.staat is None:
            ohne += 1
            continue
        anzahl[z.staat] += 1
        bund[z.staat] += z.bund
        flaeche[z.staat] += float(ort["flaeche"] or 0)
    zeilen = [f"Zuordnung zum {STICHTAG}: {sum(anzahl.values())} Orte mit Besitzer, {ohne} ohne Daten", ""]
    zeilen.append(f"{'Staat':<5} {'Name':<40} {'Orte':>5} {'im Bund':>8} {'Fläche km²':>11}")
    for staat, n in sorted(anzahl.items(), key=lambda p: -flaeche[p[0]]):
        zeilen.append(f"{staat:<5} {namen.get(staat, '')[:40]:<40} {n:>5} {bund[staat]:>8} {flaeche[staat]:>11,.0f}".replace(",", "."))
    return "\n".join(zeilen)


# ─── Gebietscodes aus GeoNames ──────────────────────────────────────────────


def erzeuge_codes(orte: list[dict[str, str]]) -> str:
    """gebietscodes.csv aus cities1000 (benötigt den Zwischenspeicher, siehe geodaten.py holen orte)."""
    import staedte

    eintraege = {}
    punkte = []
    for s in staedte.lies_staedte(staedte.staedte_datei()):
        eintraege[str(s.geonameid)] = s
        punkte.append(s)
    zeilen = []
    for ort in orte:
        treffer = RE_GEONAMEID.search(ort["quelle"])
        if treffer and treffer.group(1) in eintraege:
            s = eintraege[treffer.group(1)]
        else:
            breite, laenge = float(ort["breite"]), float(ort["laenge"])
            faktor = math.cos(math.radians(breite))
            s = min(punkte, key=lambda p: (p.breite - breite) ** 2 + ((p.laenge - laenge) * faktor) ** 2)
        zeilen.append([ort["id"], s.staat, s.admin1, s.admin2, s.admin3, str(s.einwohner)])
    return csv_text(CODE_SPALTEN, zeilen)


# ─── Kommandozeile ──────────────────────────────────────────────────────────


def lade(tabellen: Path, regeln_pfad: Path, codes_pfad: Path) -> tuple[list[dict[str, str]], Ergebnis, dict[str, str]]:
    orte = lies_csv(tabellen / "orte.csv")
    codes = {z["ort"]: {k: z[k] for k in CODE_SPALTEN[1:]} for z in lies_csv(codes_pfad, CODE_SPALTEN)}
    regeln, fehler = lies_regeln(lies_csv(regeln_pfad, REGEL_SPALTEN), regeln_pfad.name)
    staaten = {z["tag"]: z["name"] for z in lies_csv(tabellen / "staaten.csv")}
    einheiten = {z["id"]: z["staat"] for z in lies_csv(tabellen / "verwaltung.csv")}
    ergebnis = aufloesen(orte, codes, regeln, set(staaten), einheiten)
    ergebnis.fehler[:0] = fehler
    return orte, ergebnis, staaten


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="zuordnung.py", description="Zugehörigkeit der Orte 1848 aus Regeln.")
    parser.add_argument("befehl", nargs="?", choices=("codes",), help="codes: gebietscodes.csv aus GeoNames neu erzeugen")
    parser.add_argument("--tabellen", type=Path, default=TABELLEN)
    parser.add_argument("--regeln", type=Path, default=REGELN)
    parser.add_argument("--codes", type=Path, default=GEBIETSCODES)
    gruppe = parser.add_mutually_exclusive_group()
    gruppe.add_argument("--pruefen", action="store_true", help="nur prüfen, ob besitz.csv aktuell ist")
    gruppe.add_argument("--bericht", action="store_true", help="Orte und Fläche je Staat")
    gruppe.add_argument("--liste", metavar="TAG", help="Orte eines Staates (oder '-' für Orte ohne Daten)")
    args = parser.parse_args(argv)

    try:
        if args.befehl == "codes":
            text = erzeuge_codes(lies_csv(args.tabellen / "orte.csv"))
            args.codes.write_text(text, encoding="utf-8")
            print(f"{args.codes.name}: {text.count(chr(10)) - 1} Orte")
            return 0
        orte, ergebnis, staaten = lade(args.tabellen, args.regeln, args.codes)
    except ZuordnungFehler as fehler:
        print(f"Fehler: {fehler}", file=sys.stderr)
        return 2
    for text in ergebnis.warnungen:
        print(f"Warnung: {text}", file=sys.stderr)
    for text in ergebnis.fehler:
        print(f"Fehler: {text}", file=sys.stderr)
    if ergebnis.fehler:
        print(f"Abbruch: {len(ergebnis.fehler)} Fehler.", file=sys.stderr)
        return 2

    if args.bericht:
        print(bericht(orte, ergebnis, staaten))
        return 0
    if args.liste:
        gesucht = None if args.liste == "-" else args.liste
        for ort in orte:
            z = ergebnis.zuordnung[ort["id"]]
            if z.staat == gesucht:
                regel = z.regel.gebiet if z.regel else ""
                print(f"{ort['id']:<32} {ort['name']:<32} {ort['datei']:<24} {'Bund' if z.bund else '':<4} {z.verwaltung or '':<24} {regel}")
        return 0
    pfad = args.tabellen / "besitz.csv"
    alt = pfad.read_text(encoding="utf-8")
    neu = neue_besitz_tabelle(alt, besitz_zeilen(orte, ergebnis))
    if args.pruefen:
        if alt != neu:
            print("besitz.csv ist nicht aktuell – bitte python3 tools/geodaten/zuordnung.py ausführen.")
            return 1
        print("besitz.csv ist aktuell.")
        return 0
    pfad.write_text(neu, encoding="utf-8")
    print(f"besitz.csv: {len(besitz_zeilen(orte, ergebnis))} Zeilen zum {STICHTAG}. Danach: tools/archivar/archivar.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
