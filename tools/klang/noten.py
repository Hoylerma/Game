"""Notentext für EINIGKEIT (seit M13b2, docs/technik/vertical_slice.md 5.3).

Ein Stück steht in game/audio/noten/<name>.noten, einem einfachen Textformat (README.md):

    titel     = "Ode an die Freude"
    komponist = "Ludwig van Beethoven"
    werk      = "9. Sinfonie d-Moll op. 125 (1824), 4. Satz"
    stimmung  = frieden                 # frieden, krise, krieg, revolution
    tempo     = 100                     # Viertel je Minute
    takt      = 4/4
    hall      = 0.25                    # optional, 0 … 1 (Standard 0,2)
    stimme melodie { instrument = holz  lautstaerke = 0.7 }
    teil a {
        melodie = "fis5:4 fis5 g5 a5 | a5 g5 fis5 e5 |"
    }
    form = "a a"

Noten: deutsche Namen (c, cis, des, d, dis, es, e, f, fis, ges, g, gis, as, a, ais, b, h, his, ces)
mit Oktave (c4 = eingestrichenes c), Dauer nach ':' als Bruchteil der Ganzen (1, 2, 4, 8, 16, 32),
'.' punktiert, 't' Triole, '~' an die nächste gleiche Note gebunden; ohne Dauer gilt die vorige. 'r'
ist eine Pause, '[c4 e4 g4]:2' ein Akkord, '|' ein Taktstrich. Jeder Takt zwischen dem ersten und
dem letzten einer Zeile muss genau so lang sein wie der Takt; alle Stimmen eines Teils sind gleich
lang (Prüfungen gegen Abschreibfehler).
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from fractions import Fraction
from pathlib import Path

#: Instrumente des Synthesizers (synth.py).
INSTRUMENTE = ("klavier", "streicher", "holz", "blech", "pauke", "trommel")
#: Stimmungen der Musik im Spiel.
STIMMUNGEN = ("frieden", "krise", "krieg", "revolution")

_TONNAMEN = {
    "ces": -1, "c": 0, "cis": 1, "des": 1, "d": 2, "dis": 3, "es": 3, "e": 4, "eis": 5, "fes": 4,
    "f": 5, "fis": 6, "ges": 6, "g": 7, "gis": 8, "as": 8, "a": 9, "ais": 10, "b": 10, "h": 11,
    "his": 12,
}
_NOTE = re.compile(r"^(r|[a-h](?:is|es|s)?)(-?\d)?(?::(\d+)(t?)(\.?))?(~?)$")
_TON = re.compile(r"^(r|[a-h](?:is|es|s)?)(-?\d)?$")
_TOKEN = re.compile(r"\[[^\]]*\](?::\d+t?\.?)?~?|\S+")


class NotenFehler(ValueError):
    """Fehler im Notentext mit Datei und Zeile."""


@dataclass
class Ereignis:
    """Eine Note, ein Akkord oder eine Pause einer Stimme (Beginn und Dauer in Ganzen)."""

    beginn: Fraction
    dauer: Fraction
    tonhoehen: tuple[int, ...]  # MIDI-Nummern; leer = Pause


@dataclass
class Stimme:
    name: str
    instrument: str
    lautstaerke: float = 0.7


@dataclass
class Teil:
    name: str
    laenge: Fraction
    lautstaerke: float
    stimmen: dict[str, list[Ereignis]]


@dataclass
class Stueck:
    name: str
    titel: str
    komponist: str
    werk: str
    stimmung: str
    tempo: float
    takt: Fraction
    hall: float
    stimmen: dict[str, Stimme]
    teile: dict[str, Teil]
    form: list[str]
    quelle: Path | None = None
    meta: dict[str, str] = field(default_factory=dict)

    def dauer_ganze(self) -> Fraction:
        return sum((self.teile[t].laenge for t in self.form), Fraction(0))

    def dauer_sekunden(self) -> float:
        # Eine Ganze = vier Viertel.
        return float(self.dauer_ganze()) * 4.0 * 60.0 / self.tempo


# ─── Lesen ──────────────────────────────────────────────────────────────────


@dataclass
class _Knoten:
    schluessel: str
    wert: str | None
    kinder: list[_Knoten] | None
    zeile: int


def _zerlege(text: str, datei: str) -> list[tuple[str, int]]:
    """Wörter, Zeichenketten (mit führendem ") und Klammern mit Zeilennummer."""
    teile: list[tuple[str, int]] = []
    zeile = 1
    i = 0
    while i < len(text):
        c = text[i]
        if c == "\n":
            zeile += 1
            i += 1
        elif c.isspace():
            i += 1
        elif c == "#":
            while i < len(text) and text[i] != "\n":
                i += 1
        elif c in "{}=":
            teile.append((c, zeile))
            i += 1
        elif c == '"':
            ende = text.find('"', i + 1)
            if ende < 0:
                raise NotenFehler(f"{datei}:{zeile}: Zeichenkette ohne schließendes \"")
            wert = text[i + 1 : ende]
            teile.append(('"' + wert, zeile))
            zeile += wert.count("\n")
            i = ende + 1
        else:
            j = i
            while j < len(text) and not text[j].isspace() and text[j] not in '{}="#':
                j += 1
            teile.append((text[i:j], zeile))
            i = j
    return teile


def _baum(teile: list[tuple[str, int]], datei: str) -> list[_Knoten]:
    pos = 0

    def block() -> list[_Knoten]:
        nonlocal pos
        knoten: list[_Knoten] = []
        while pos < len(teile):
            wort, zeile = teile[pos]
            if wort == "}":
                return knoten
            if wort in "{=" or wort.startswith('"'):
                raise NotenFehler(f"{datei}:{zeile}: unerwartet {wort[:20]!r}")
            pos += 1
            if pos < len(teile) and teile[pos][0] == "=":
                pos += 1
                if pos >= len(teile):
                    raise NotenFehler(f"{datei}:{zeile}: Wert nach '=' fehlt")
                wert = teile[pos][0]
                pos += 1
                knoten.append(_Knoten(wort, wert[1:] if wert.startswith('"') else wert, None, zeile))
                continue
            name = None
            if pos < len(teile) and teile[pos][0] not in "{}=" and not teile[pos][0].startswith('"'):
                name = teile[pos][0]
                pos += 1
            if pos >= len(teile) or teile[pos][0] != "{":
                raise NotenFehler(f"{datei}:{zeile}: nach {wort!r} wird '=' oder '{{' erwartet")
            pos += 1
            kinder = block()
            if pos >= len(teile):
                raise NotenFehler(f"{datei}:{zeile}: Block {wort!r} ohne schließende '}}'")
            pos += 1
            knoten.append(_Knoten(wort, name, kinder, zeile))
        return knoten

    ergebnis = block()
    if pos < len(teile):
        raise NotenFehler(f"{datei}:{teile[pos][1]}: '}}' ohne öffnende Klammer")
    return ergebnis


def _bruch(text: str, datei: str, zeile: int) -> Fraction:
    try:
        wert = Fraction(text)
    except (ValueError, ZeroDivisionError):
        raise NotenFehler(f"{datei}:{zeile}: ungültiger Bruch {text!r}") from None
    if wert <= 0:
        raise NotenFehler(f"{datei}:{zeile}: {text!r} muss größer als 0 sein")
    return wert


def _zahl(text: str, datei: str, zeile: int, lo: float, hi: float) -> float:
    try:
        wert = float(text)
    except ValueError:
        raise NotenFehler(f"{datei}:{zeile}: ungültige Zahl {text!r}") from None
    if not lo <= wert <= hi:
        raise NotenFehler(f"{datei}:{zeile}: {text} liegt nicht zwischen {lo:g} und {hi:g}")
    return wert


def tonhoehe(name: str, oktave: int) -> int:
    """MIDI-Nummer eines deutschen Tonnamens (c4 = 60)."""
    return 12 * (oktave + 1) + _TONNAMEN[name]


def lies_zeile(text: str, takt: Fraction, datei: str = "", zeile: int = 0, stimme: str = "") -> list[Ereignis]:
    """Notenzeile einer Stimme in Ereignisse; prüft die Takte."""
    wo = f"{datei}:{zeile}: Stimme {stimme}" if stimme else f"{datei}:{zeile}"
    ereignisse: list[Ereignis] = []
    zeit = Fraction(0)
    dauer = Fraction(1, 4)
    takte: list[Fraction] = [Fraction(0)]
    gebunden = False
    for wort in _TOKEN.findall(text):
        if wort == "|":
            takte.append(Fraction(0))
            continue
        akkord = wort.startswith("[")
        if akkord:
            ende = wort.index("]")
            innen = wort[1:ende].split()
            rest = wort[ende + 1 :]
            if not innen:
                raise NotenFehler(f"{wo}: leerer Akkord")
            kopf = innen[0] + rest
            namen = innen
        else:
            kopf = wort
            namen = [wort]
        m = _NOTE.match(kopf)
        if m is None:
            raise NotenFehler(f"{wo}: unbekannte Note {wort!r}")
        if m.group(3):
            nenner = int(m.group(3))
            if nenner not in (1, 2, 4, 8, 16, 32):
                raise NotenFehler(f"{wo}: Dauer :{nenner} (erlaubt 1, 2, 4, 8, 16, 32) in {wort!r}")
            dauer = Fraction(1, nenner)
            if m.group(4):
                dauer *= Fraction(2, 3)
            if m.group(5):
                dauer *= Fraction(3, 2)
        hoehen: list[int] = []
        for n in namen:
            teil = _TON.match(n if akkord else m.group(1) + (m.group(2) or ""))
            if teil is None:
                raise NotenFehler(f"{wo}: unbekannte Note {n!r} in {wort!r}")
            if teil.group(1) == "r":
                if akkord:
                    raise NotenFehler(f"{wo}: Pause im Akkord {wort!r}")
                continue
            if teil.group(1) not in _TONNAMEN:
                raise NotenFehler(f"{wo}: unbekannter Ton {teil.group(1)!r}")
            if teil.group(2) is None:
                raise NotenFehler(f"{wo}: Oktave fehlt in {n!r} (z. B. c4)")
            hoehen.append(tonhoehe(teil.group(1), int(teil.group(2))))
        tie = wort.endswith("~")
        if gebunden and ereignisse and ereignisse[-1].tonhoehen == tuple(hoehen) and hoehen:
            ereignisse[-1].dauer += dauer
        else:
            if gebunden:
                raise NotenFehler(f"{wo}: '~' bindet {wort!r} an eine andere Note")
            ereignisse.append(Ereignis(zeit, dauer, tuple(hoehen)))
        gebunden = tie
        zeit += dauer
        takte[-1] += dauer
    if gebunden:
        raise NotenFehler(f"{wo}: '~' am Ende der Zeile")
    if takte and takte[-1] == 0:
        takte.pop()  # Taktstrich am Ende
    for nummer, laenge in enumerate(takte):
        innen = 0 < nummer < len(takte) - 1
        if (innen and laenge != takt) or laenge > takt:
            raise NotenFehler(
                f"{wo}: Takt {nummer + 1} ist {laenge} lang statt {takt} ({_als_takt(laenge)})"
            )
    return ereignisse


def _als_takt(laenge: Fraction) -> str:
    sechzehntel = laenge * 16
    return f"{sechzehntel} Sechzehntel" if sechzehntel.denominator == 1 else f"{laenge} Ganze"


def lies(pfad: Path) -> Stueck:
    """Liest ein Stück aus einer .noten-Datei."""
    datei = pfad.name
    knoten = _baum(_zerlege(pfad.read_text(encoding="utf-8"), datei), datei)
    werte: dict[str, str] = {}
    stimmen: dict[str, Stimme] = {}
    roh_teile: list[_Knoten] = []
    zeilen: dict[str, int] = {}
    for k in knoten:
        if k.kinder is None:
            if k.schluessel in werte:
                raise NotenFehler(f"{datei}:{k.zeile}: {k.schluessel!r} ist mehrfach angegeben")
            werte[k.schluessel] = k.wert or ""
            zeilen[k.schluessel] = k.zeile
        elif k.schluessel == "stimme":
            if not k.wert or k.wert in stimmen:
                raise NotenFehler(f"{datei}:{k.zeile}: Stimme ohne Namen oder doppelt ({k.wert})")
            felder = {c.schluessel: c.wert or "" for c in k.kinder}
            instrument = felder.get("instrument", "")
            if instrument not in INSTRUMENTE:
                raise NotenFehler(
                    f"{datei}:{k.zeile}: Stimme {k.wert}: Instrument {instrument!r} unbekannt "
                    f"(erlaubt: {', '.join(INSTRUMENTE)})"
                )
            laut = _zahl(felder.get("lautstaerke", "0.7"), datei, k.zeile, 0.0, 2.0)
            stimmen[k.wert] = Stimme(k.wert, instrument, laut)
        elif k.schluessel == "teil":
            roh_teile.append(k)
        else:
            raise NotenFehler(f"{datei}:{k.zeile}: unbekannter Block {k.schluessel!r}")
    for pflicht in ("titel", "komponist", "stimmung", "tempo", "takt", "form"):
        if pflicht not in werte:
            raise NotenFehler(f"{datei}: Pflichtfeld {pflicht!r} fehlt")
    unbekannt = set(werte) - {"titel", "komponist", "werk", "stimmung", "tempo", "takt", "hall", "form"}
    if unbekannt:
        schluessel = sorted(unbekannt)[0]
        raise NotenFehler(f"{datei}:{zeilen[schluessel]}: unbekanntes Feld {schluessel!r}")
    if werte["stimmung"] not in STIMMUNGEN:
        raise NotenFehler(
            f"{datei}:{zeilen['stimmung']}: Stimmung {werte['stimmung']!r} unbekannt (erlaubt: {', '.join(STIMMUNGEN)})"
        )
    if not stimmen:
        raise NotenFehler(f"{datei}: keine Stimme (stimme <name> {{ instrument = … }})")
    takt = _bruch(werte["takt"], datei, zeilen["takt"])
    tempo = _zahl(werte["tempo"], datei, zeilen["tempo"], 20.0, 300.0)
    hall = _zahl(werte.get("hall", "0.2"), datei, zeilen.get("hall", 0), 0.0, 1.0)
    teile: dict[str, Teil] = {}
    for k in roh_teile:
        if not k.wert or k.wert in teile:
            raise NotenFehler(f"{datei}:{k.zeile}: Teil ohne Namen oder doppelt ({k.wert})")
        zeilentexte: dict[str, list[tuple[str, int]]] = {}
        lautstaerke = 1.0
        for c in k.kinder or []:
            if c.kinder is not None:
                raise NotenFehler(f"{datei}:{c.zeile}: Block in Teil {k.wert}")
            if c.schluessel == "lautstaerke":
                lautstaerke = _zahl(c.wert or "", datei, c.zeile, 0.0, 2.0)
                continue
            if c.schluessel not in stimmen:
                raise NotenFehler(f"{datei}:{c.zeile}: Teil {k.wert}: unbekannte Stimme {c.schluessel!r}")
            zeilentexte.setdefault(c.schluessel, []).append((c.wert or "", c.zeile))
        teil_stimmen: dict[str, list[Ereignis]] = {}
        laengen: dict[str, Fraction] = {}
        for name, texte in zeilentexte.items():
            ereignisse: list[Ereignis] = []
            versatz = Fraction(0)
            for text, zeile in texte:  # mehrere Zeilen einer Stimme folgen aufeinander
                for e in lies_zeile(text, takt, datei, zeile, name):
                    ereignisse.append(Ereignis(e.beginn + versatz, e.dauer, e.tonhoehen))
                versatz = ereignisse[-1].beginn + ereignisse[-1].dauer if ereignisse else versatz
            teil_stimmen[name] = ereignisse
            laengen[name] = versatz
        if not laengen:
            raise NotenFehler(f"{datei}:{k.zeile}: Teil {k.wert} ohne Stimmen")
        laenge = max(laengen.values())
        for name, wert in laengen.items():
            if wert != laenge:
                raise NotenFehler(
                    f"{datei}:{k.zeile}: Teil {k.wert}: Stimme {name} ist {_als_takt(wert)} lang, "
                    f"die längste {_als_takt(laenge)}"
                )
        teile[k.wert] = Teil(k.wert, laenge, lautstaerke, teil_stimmen)
    form = werte["form"].split()
    for t in form:
        if t not in teile:
            raise NotenFehler(f"{datei}:{zeilen['form']}: Form nennt unbekannten Teil {t!r}")
    return Stueck(
        name=pfad.stem,
        titel=werte["titel"],
        komponist=werte["komponist"],
        werk=werte.get("werk", ""),
        stimmung=werte["stimmung"],
        tempo=tempo,
        takt=takt,
        hall=hall,
        stimmen=stimmen,
        teile=teile,
        form=form,
        quelle=pfad,
        meta=werte,
    )
