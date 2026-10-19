#!/usr/bin/env python3
"""Wahlkreise zum Lesezeichen 1848 (Meilenstein M8, docs/technik/politik.md Abschnitt 5).

Erzeugt ``data/tabellen/wahlkreise.csv`` für die Parlamente aus ``data/common/parlamente.chron``:

1. **Wahlgebiet** je Parlament: die Nationalversammlung wählt im Bundesgebiet und in den im April
   1848 zugelassenen Gebieten (Ost- und Westpreußen, Posen, Schleswig: alle Orte Preußens und
   Dänemarks), ein Landtag in seinem Staat. Nur Orte mit Bevölkerung 1846.
2. **Sitze je Staat** im Verhältnis zur Bevölkerung 1846 (Hare-Niemeyer, jeder Staat mindestens
   einen Sitz); ein Landtag hat nur seinen Staat.
3. **Regionen je Staat:** Die Sitze eines Staates verteilen sich nach Einwohnern auf seine Regionen
   (in Preußen die Regierungsbezirke, in Österreich die Kronländer, sonst die Landschaften der
   Karte); eine Region ohne eigenen Sitz kommt zur nächsten Region. So überspannt kein Wahlkreis
   getrennte Landesteile (Rheinprovinz und Ostpreußen, die Pfalz und Altbayern).
4. **Wahlkreise je Region:** Ein Ort mit mindestens dem 1,5-Fachen der Zielgröße (Einwohner je Sitz)
   wird ein eigener Wahlkreis mit mehreren Sitzen (Berlin, Wien). Die übrigen Orte bilden
   Bausteine: in Preußen und Österreich die Kreise der Verwaltung, sonst die Kreise der Karte
   (M3). Zu große Bausteine zerfallen in ihre Orte. Die Bausteine werden in der Reihenfolge einer
   Hilbert-Kurve über ihre Lage in zusammenhängende Gruppen gleicher Einwohnerzahl geschnitten.
5. **Name** nach dem einwohnerstärksten Ort, Kennung ``<kürzel>_<staat>_<nr>``.

Die Wahlkreise sind Spielwerte, keine historischen Grenzen; die Tabelle bleibt editierbar und
wird mit ``--pruefen`` gegen eine Neuberechnung verglichen.

Aufruf::

    python3 tools/geodaten/wahlkreise_1848.py            # schreibt wahlkreise.csv, Bericht
    python3 tools/geodaten/wahlkreise_1848.py --pruefen  # Exitcode 1, wenn die Tabelle abweicht
"""

from __future__ import annotations

import argparse
import math
import re
import sys
from collections import defaultdict
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path

try:
    import zuordnung as zu
except ImportError:  # Aufruf als Modul von außerhalb des Werkzeugordners
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import zuordnung as zu

PARLAMENTE = zu.REPO / "data" / "common" / "parlamente.chron"
AUSGABE = zu.TABELLEN / "wahlkreise.csv"
SPALTEN = ("wahlkreis", "parlament", "staat", "name", "sitze", "orte", "sicherheit", "quelle")
STICHTAG_BESITZ = "1848.03.01"
STICHTAG_BEVOELKERUNG = "1846.12.03"
# Zugelassen zur Wahl der Nationalversammlung, obwohl außerhalb des Bundesgebiets (April 1848).
ZUGELASSEN = frozenset({"PRU", "DEN"})
GROSSER_ORT = 1.5  # ab diesem Vielfachen der Zielgröße ein eigener Wahlkreis
KUERZEL = {"nationalversammlung": "nv", "pr_abgeordnetenhaus": "pr"}
QUELLE = (
    "Werkzeug tools/geodaten/wahlkreise_1848.py (M8): Sitze im Verhältnis zur Bevölkerung 1846, "
    "Wahlkreise aus Kreisen in räumlicher Reihenfolge; keine historischen Wahlkreisgrenzen"
)


@dataclass(frozen=True)
class Parlament:
    kennung: str
    ebene: str  # "bund" oder Staatstag
    sitze: int


@dataclass
class Ort:
    kennung: str
    name: str
    staat: str
    bund: bool
    einwohner: int
    laenge: float
    breite: float
    baustein: str  # Kreis der Verwaltung oder der Karte
    region: str  # Regierungsbezirk, Kronland oder Landschaft


@dataclass
class Wahlkreis:
    staat: str
    orte: list[Ort]
    sitze: int = 1
    kennung: str = ""
    name: str = ""

    @property
    def einwohner(self) -> int:
        return sum(o.einwohner for o in self.orte)


@dataclass
class Ergebnis:
    parlament: Parlament
    sitze_je_staat: dict[str, int] = field(default_factory=dict)
    wahlkreise: list[Wahlkreis] = field(default_factory=list)


# ─── Eingaben ───────────────────────────────────────────────────────────────


def lies_parlamente(pfad: Path = PARLAMENTE) -> list[Parlament]:
    """Kennung, Ebene und Sitze der Parlamente aus parlamente.chron (nur diese drei Felder)."""
    text = pfad.read_text(encoding="utf-8")
    ergebnis = []
    for treffer in re.finditer(r"^parlament\s+([a-z0-9_]+)\s*\{(.*?)^\}", text, re.MULTILINE | re.DOTALL):
        rumpf = treffer.group(2)
        ebene = re.search(r"^\s*ebene\s*=\s*(\S+)", rumpf, re.MULTILINE)
        sitze = re.search(r"^\s*sitze\s*=\s*(\d+)", rumpf, re.MULTILINE)
        if ebene is None or sitze is None:
            raise zu.ZuordnungFehler(f"{pfad}: parlament {treffer.group(1)} ohne 'ebene' oder 'sitze'")
        ergebnis.append(Parlament(treffer.group(1), ebene.group(1), int(sitze.group(1))))
    return ergebnis


def lies_orte(tabellen: Path) -> list[Ort]:
    """Orte mit Besitzer am Stichtag und Bevölkerung 1846, mit Baustein (Kreis)."""
    stamm = {z["id"]: z for z in zu.lies_csv(tabellen / "orte.csv")}
    einheiten = {z["id"]: z for z in zu.lies_csv(tabellen / "verwaltung.csv")}
    einwohner = {
        z["ort"]: int(z["bevoelkerung"])
        for z in zu.lies_csv(tabellen / "bevoelkerung.csv")
        if z["stichtag"] == STICHTAG_BEVOELKERUNG
    }
    orte = []
    for z in zu.lies_csv(tabellen / "besitz.csv"):
        if z["ab"] != STICHTAG_BESITZ or z["ort"] not in einwohner:
            continue
        s = stamm[z["ort"]]
        einheit = einheiten.get(z["verwaltung"])
        # Kreise der Verwaltung, wo es sie gibt (Preußen, Österreich), sonst der Kreis der Karte.
        if einheit is not None and einheit["art"] in ("kreis", "viertel"):
            baustein = "v:" + einheit["id"]
            region = "v:" + (einheit["ueber"] or einheit["id"])
        else:
            baustein = "k:" + s["kreis"]
            region = "l:" + s["datei"]
        orte.append(
            Ort(
                z["ort"],
                s["name"],
                z["besitzer"],
                z["bund"] == "ja",
                einwohner[z["ort"]],
                float(s["laenge"]),
                float(s["breite"]),
                baustein,
                region,
            )
        )
    return orte


def wahlgebiet(parlament: Parlament, orte: list[Ort]) -> list[Ort]:
    if parlament.ebene == "bund":
        return [o for o in orte if o.bund or o.staat in ZUGELASSEN]
    return [o for o in orte if o.staat == parlament.ebene]


# ─── Rechnung ───────────────────────────────────────────────────────────────


def groesste_reste(gewichte: dict[str, int], sitze: int) -> dict[str, int]:
    """Hare-Niemeyer ohne Mindestsitz."""
    summe = sum(gewichte.values())
    if summe <= 0:
        return {k: 0 for k in gewichte}
    return _verteile({k: g * sitze / summe for k, g in gewichte.items()}, sitze)


def mittelpunkt(orte: list[Ort]) -> tuple[float, float]:
    summe = sum(o.einwohner for o in orte) or 1
    return sum(o.laenge * o.einwohner for o in orte) / summe, sum(o.breite * o.einwohner for o in orte) / summe


def regionen(orte: list[Ort], sitze: int) -> list[tuple[list[Ort], int]]:
    """Orte eines Staates in Regionen mit Sitzen; Regionen ohne Sitz gehen in der nächsten auf."""
    gruppen: dict[str, list[Ort]] = defaultdict(list)
    for o in orte:
        gruppen[o.region].append(o)
    while True:
        verteilt = groesste_reste({r: sum(o.einwohner for o in g) for r, g in gruppen.items()}, sitze)
        leer = sorted((r for r, n in verteilt.items() if n == 0), key=lambda r: (sum(o.einwohner for o in gruppen[r]), r))
        if not leer or len(gruppen) == 1:
            return [(gruppen[r], verteilt[r]) for r in sorted(gruppen)]
        klein = leer[0]
        mitte = mittelpunkt(gruppen[klein])
        ziel = min(
            (r for r in gruppen if r != klein),
            key=lambda r: (abstand_km(Ort("", "", "", False, 0, mitte[0], mitte[1], "", ""), *mittelpunkt(gruppen[r])), r),
        )
        gruppen[ziel] += gruppen.pop(klein)


def hare_niemeyer(gewichte: dict[str, int], sitze: int) -> dict[str, int]:
    """Sitze im Verhältnis der Gewichte, größte Reste; jeder mit Gewicht > 0 bekommt mindestens einen."""
    feste: dict[str, int] = {}
    offen = {k: g for k, g in gewichte.items() if g > 0}
    while True:
        rest_sitze = sitze - sum(feste.values())
        summe = sum(offen.values())
        if rest_sitze <= 0 or summe <= 0:
            return {**feste, **{k: 0 for k in offen}}
        quoten = {k: g * rest_sitze / summe for k, g in offen.items()}
        null = [k for k, q in quoten.items() if q < 1 and math.floor(q) == 0 and k not in feste]
        # Wer ohne Mindestsitz leer ausginge, bekommt ihn fest; dann neu verteilen.
        leer = [k for k in null if _verteile(quoten, rest_sitze)[k] == 0]
        if not leer:
            return {**feste, **_verteile(quoten, rest_sitze)}
        for k in leer:
            feste[k] = 1
            del offen[k]


def _verteile(quoten: dict[str, float], sitze: int) -> dict[str, int]:
    ganz = {k: math.floor(q) for k, q in quoten.items()}
    rest = sitze - sum(ganz.values())
    for k in sorted(quoten, key=lambda k: (-(quoten[k] - ganz[k]), k))[:rest]:
        ganz[k] += 1
    return ganz


def hilbert_index(x: int, y: int, ordnung: int = 16) -> int:
    """Position eines Gitterpunkts (0 … 2^ordnung − 1) auf der Hilbert-Kurve."""
    n = 1 << ordnung
    d = 0
    s = n >> 1
    while s > 0:
        rx = 1 if x & s else 0
        ry = 1 if y & s else 0
        d += s * s * ((3 * rx) ^ ry)
        if ry == 0:
            if rx == 1:
                x, y = n - 1 - x, n - 1 - y
            x, y = y, x
        s >>= 1
    return d


def raeumlich_sortiert(gruppen: list[list[Ort]]) -> list[list[Ort]]:
    """Gruppen von Orten nach dem Hilbert-Index ihres (nach Einwohnern gewichteten) Mittelpunkts."""
    if not gruppen:
        return []
    mitten = []
    for g in gruppen:
        summe = sum(o.einwohner for o in g) or 1
        mitten.append((sum(o.laenge * o.einwohner for o in g) / summe, sum(o.breite * o.einwohner for o in g) / summe))
    lmin, lmax = min(m[0] for m in mitten), max(m[0] for m in mitten)
    bmin, bmax = min(m[1] for m in mitten), max(m[1] for m in mitten)
    spanne = max(lmax - lmin, bmax - bmin, 1e-9)
    gitter = (1 << 16) - 1

    def schluessel(i: int) -> tuple[int, str]:
        x = round((mitten[i][0] - lmin) / spanne * gitter)
        y = round((mitten[i][1] - bmin) / spanne * gitter)
        return hilbert_index(x, y), gruppen[i][0].kennung

    return [gruppen[i] for i in sorted(range(len(gruppen)), key=schluessel)]


def schneide(gruppen: list[list[Ort]], teile: int) -> list[list[Ort]]:
    """Folge von Gruppen in ``teile`` zusammenhängende Abschnitte mit möglichst gleicher Einwohnerzahl."""
    if teile <= 0:
        return []
    if teile >= len(gruppen):
        return [list(g) for g in gruppen]
    gewichte = [sum(o.einwohner for o in g) for g in gruppen]
    gesamt = sum(gewichte)
    kumuliert = []
    laufend = 0
    for w in gewichte:
        laufend += w
        kumuliert.append(laufend)
    schnitte = []
    letzter = -1
    for j in range(1, teile):
        ziel = gesamt * j / teile
        # Schnitt nach Gruppe i; mindestens eine Gruppe je Abschnitt davor und danach.
        lo, hi = letzter + 1, len(gruppen) - (teile - j) - 1
        bester = min(range(lo, hi + 1), key=lambda i: (abs(kumuliert[i] - ziel), i))
        schnitte.append(bester)
        letzter = bester
    ergebnis = []
    anfang = 0
    for s in schnitte + [len(gruppen) - 1]:
        ergebnis.append([o for g in gruppen[anfang : s + 1] for o in g])
        anfang = s + 1
    return ergebnis


def wahlkreise_eines_staates(staat: str, orte: list[Ort], sitze: int) -> list[Wahlkreis]:
    """Wahlkreise eines Staates mit zusammen ``sitze`` Sitzen (Schritte 3 und 4 oben)."""
    if sitze <= 0 or not orte:
        return []
    return [k for g, n in regionen(orte, sitze) for k in wahlkreise_einer_region(staat, g, n)]


def wahlkreise_einer_region(staat: str, orte: list[Ort], sitze: int) -> list[Wahlkreis]:
    if sitze <= 0 or not orte:
        return []
    ziel = sum(o.einwohner for o in orte) / sitze
    kreise: list[Wahlkreis] = []
    rest: list[Ort] = []
    for o in sorted(orte, key=lambda o: o.kennung):
        if o.einwohner >= GROSSER_ORT * ziel and len(orte) > 1:
            kreise.append(Wahlkreis(staat, [o], max(1, round(o.einwohner / ziel))))
        else:
            rest.append(o)
    while sum(k.sitze for k in kreise) > sitze:  # große Orte nehmen nie mehr als alle Sitze
        max(kreise, key=lambda k: (k.sitze, k.orte[0].kennung)).sitze -= 1
    uebrig = sitze - sum(k.sitze for k in kreise)
    if uebrig > 0 and rest:
        bausteine: dict[str, list[Ort]] = defaultdict(list)
        for o in rest:
            bausteine[o.baustein].append(o)
        gruppen = []
        for _, g in sorted(bausteine.items()):
            if sum(o.einwohner for o in g) > GROSSER_ORT * ziel and len(g) > 1:
                gruppen += [[o] for o in g]
            else:
                gruppen.append(g)
        if len(gruppen) < uebrig:
            gruppen = [[o] for g in gruppen for o in g]
        teile = schneide(raeumlich_sortiert(gruppen), min(uebrig, len(gruppen)))
        neue = [Wahlkreis(staat, t) for t in teile]
        # Mehr Sitze als Orte: die einwohnerstärksten Wahlkreise wählen mehrere Abgeordnete.
        for i in range(uebrig - len(neue)):
            max(neue, key=lambda k: (k.einwohner / k.sitze, k.orte[0].kennung)).sitze += 1
        kreise += neue
    elif uebrig > 0:
        kreise[0].sitze += uebrig
    elif rest:
        # Große Orte haben alle Sitze (Hamburg): die übrigen Orte kommen zum nächsten Wahlkreis.
        for o in rest:
            naechster = min(kreise, key=lambda k: (abstand_km(o, *mittelpunkt(k.orte)), k.orte[0].kennung))
            naechster.orte.append(o)
    return kreise


def berechne(parlament: Parlament, orte: list[Ort]) -> Ergebnis:
    gebiet = wahlgebiet(parlament, orte)
    je_staat: dict[str, list[Ort]] = defaultdict(list)
    for o in gebiet:
        je_staat[o.staat].append(o)
    ergebnis = Ergebnis(parlament)
    ergebnis.sitze_je_staat = hare_niemeyer({s: sum(o.einwohner for o in g) for s, g in je_staat.items()},
                                            parlament.sitze)
    kuerzel = KUERZEL.get(parlament.kennung, parlament.kennung[:3])
    for staat in sorted(je_staat):
        kreise = wahlkreise_eines_staates(staat, je_staat[staat], ergebnis.sitze_je_staat[staat])
        kreise.sort(key=lambda k: raeumlich_sortiert([k.orte])[0][0].kennung)
        kreise = [kreise[i] for i in _raeumliche_folge(kreise)]
        namen: dict[str, int] = defaultdict(int)
        for nr, k in enumerate(kreise, start=1):
            k.kennung = f"{kuerzel}_{staat.lower()}_{nr:02d}" if parlament.ebene == "bund" else f"{kuerzel}_{nr:03d}"
            groesster = max(k.orte, key=lambda o: (o.einwohner, o.kennung))
            namen[groesster.name] += 1
            k.name = groesster.name if namen[groesster.name] == 1 else f"{groesster.name} {namen[groesster.name]}"
            k.orte.sort(key=lambda o: (-o.einwohner, o.kennung))
        ergebnis.wahlkreise += kreise
    return ergebnis


def _raeumliche_folge(kreise: list[Wahlkreis]) -> list[int]:
    """Reihenfolge der Wahlkreise eines Staates entlang der Hilbert-Kurve (für die Nummern)."""
    sortiert = raeumlich_sortiert([k.orte for k in kreise])
    position = {id(g): i for i, g in enumerate(sortiert)}
    return sorted(range(len(kreise)), key=lambda i: position[id(kreise[i].orte)])


# ─── Ausgabe und Bericht ────────────────────────────────────────────────────


def abstand_km(a: Ort, b_laenge: float, b_breite: float) -> float:
    breite_a, breite_b = math.radians(a.breite), math.radians(b_breite)
    d_breite = breite_b - breite_a
    d_laenge = math.radians(b_laenge - a.laenge)
    h = math.sin(d_breite / 2) ** 2 + math.cos(breite_a) * math.cos(breite_b) * math.sin(d_laenge / 2) ** 2
    return 2 * 6371.0 * math.asin(math.sqrt(h))


def ausdehnung_km(k: Wahlkreis) -> float:
    """Größter Abstand eines Ortes vom Mittelpunkt des Wahlkreises."""
    summe = k.einwohner or 1
    laenge = sum(o.laenge * o.einwohner for o in k.orte) / summe
    breite = sum(o.breite * o.einwohner for o in k.orte) / summe
    return max(abstand_km(o, laenge, breite) for o in k.orte)


def zeilen(ergebnisse: list[Ergebnis]) -> list[list[str]]:
    return [
        [k.kennung, e.parlament.kennung, k.staat, k.name, str(k.sitze), " ".join(o.kennung for o in k.orte),
         "spielwert", QUELLE]
        for e in ergebnisse
        for k in e.wahlkreise
    ]


def bericht(ergebnisse: list[Ergebnis]) -> str:
    teile = []
    for e in ergebnisse:
        kreise = e.wahlkreise
        sitze = sum(k.sitze for k in kreise)
        je_sitz = sorted(k.einwohner / k.sitze for k in kreise)
        median = je_sitz[len(je_sitz) // 2]
        weit = sorted(kreise, key=lambda k: -ausdehnung_km(k))[:3]
        teile.append(
            f"{e.parlament.kennung}: {len(kreise)} Wahlkreise, {sitze} Sitze "
            f"(Soll {e.parlament.sitze}); Einwohner je Sitz Median {zahl(median)}, "
            f"{zahl(je_sitz[0])} bis {zahl(je_sitz[-1])}; größte Ausdehnung "
            + ", ".join(f"{k.name} {ausdehnung_km(k):.0f} km" for k in weit)
        )
        teile.append(
            "  Sitze je Staat: "
            + ", ".join(f"{s} {n}" for s, n in sorted(e.sitze_je_staat.items(), key=lambda x: (-x[1], x[0])))
        )
    return "\n".join(teile)


def zahl(wert: float) -> str:
    """Ganze Zahl mit deutschem Tausenderpunkt."""
    return f"{round(wert):,}".replace(",", ".")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="wahlkreise_1848.py", description="Wahlkreise zum Lesezeichen 1848.")
    parser.add_argument("--tabellen", type=Path, default=zu.TABELLEN)
    parser.add_argument("--parlamente", type=Path, default=PARLAMENTE)
    parser.add_argument("--pruefen", action="store_true", help="Tabelle mit einer Neuberechnung vergleichen")
    args = parser.parse_args(argv)
    try:
        orte = lies_orte(args.tabellen)
        ergebnisse = [berechne(p, orte) for p in lies_parlamente(args.parlamente)]
    except zu.ZuordnungFehler as fehler:
        print(f"Fehler: {fehler}", file=sys.stderr)
        return 2
    text = zu.csv_text(SPALTEN, zeilen(ergebnisse))
    ziel = args.tabellen / "wahlkreise.csv"
    if args.pruefen:
        alt = ziel.read_text(encoding="utf-8") if ziel.exists() else ""
        if alt != text:
            print(f"{ziel} weicht von der Neuberechnung ab; neu erzeugen mit wahlkreise_1848.py", file=sys.stderr)
            return 1
        print(f"{ziel} ist aktuell.")
        return 0
    ziel.write_text(text, encoding="utf-8")
    print(bericht(ergebnisse))
    print(f"geschrieben: {ziel}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
