#!/usr/bin/env python3
"""Archivar: erzeugt data/history/ aus den Tabellen in data/tabellen/.

Die Tabellen (CSV, Semikolon, UTF-8) sind die Quelle der Wahrheit für Orte und
Staaten samt datierter Historie; jede Zeile trägt eine Quelle und eine Angabe zur
Sicherheit. Der Archivar prüft Verweise, Lücken zu den Szenariodaten und
Ausreißer und schreibt daraus die Chronik-Skript-Dateien, die der Kern lädt.

Format der Tabellen: docs/technik/tabellen.md
Format der Ausgabe: docs/technik/chronik-skript.md, Abschnitte 3.5, 3.6, 3.8 und 3.9

Aufruf:
  python3 tools/archivar/archivar.py              Daten prüfen und history/ schreiben
  python3 tools/archivar/archivar.py --pruefen    nur prüfen, ob history/ aktuell ist (CI)
  python3 tools/archivar/archivar.py --bericht    Übersicht über Umfang und Sicherheit

Exitcodes: 0 in Ordnung, 1 history/ nicht aktuell (--pruefen), 2 Datenfehler.

Die Ausgabe ist deterministisch: Gleiche Tabellen ergeben byteidentische Dateien.
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
from collections import Counter
from dataclasses import dataclass, field
from fractions import Fraction
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "kartengenerator"))
import chron  # noqa: E402  (Parser für das Chronik-Skript, gemeinsam mit dem Kartengenerator)

KOPFZEILE = "Erzeugt von tools/archivar aus data/tabellen/ – nicht von Hand bearbeiten."

SICHERHEIT = ("gesichert", "schaetzung", "spielwert", "ungeprueft")

SPALTEN: dict[str, tuple[str, ...]] = {
    "dateien.csv": ("ordner", "datei", "kultur", "kommentar"),
    "staaten.csv": ("tag", "datei", "name", "kurzname", "farbe", "quelle"),
    "staaten_historie.csv": ("tag", "ab", "feld", "wert", "sicherheit", "quelle"),
    "orte.csv": ("id", "datei", "name", "gelaende", "laenge", "breite", "flaeche", "stadt", "kreis", "quelle"),
    "kreise.csv": ("id", "datei", "name", "quelle"),
    "besitz.csv": ("ort", "ab", "besitzer", "sicherheit", "quelle"),
    "bevoelkerung.csv": (
        "ort",
        "stichtag",
        "bevoelkerung",
        "konfession",
        "kultur",
        "verwuestung",
        "sicherheit",
        "quelle",
    ),
}

# Datierbare Felder eines Staates in der Reihenfolge der Ausgabe.
STAAT_FELDER = (
    "name",
    "kurzname",
    "hauptstadt",
    "regierungsform",
    "herrscher",
    "konfession",
    "schatz",
    "steuersatz",
    "hofhaltung",
)
# Felder, die zu jedem Szenariodatum bekannt sein müssen (name/kurzname kommen aus staaten.csv).
STAAT_PFLICHT = ("hauptstadt", "regierungsform", "herrscher", "konfession", "schatz", "steuersatz", "hofhaltung")
TEXT_FELDER = frozenset({"name", "kurzname", "herrscher", "quelle"})

RE_ORT = re.compile(r"[a-z0-9_]+")
ORDNER = ("kreise", "orte", "staaten")  # Unterordner von history/, die der Archivar erzeugt
RE_TAG = re.compile(r"[A-Z0-9]{2,4}")
RE_DATEI = re.compile(r"[A-Za-z0-9_]+")
RE_KENNUNG = re.compile(r"[a-z][a-z0-9_]*")
RE_DATUM = re.compile(r"(\d{4})\.(\d{2})\.(\d{2})")
RE_DEZIMAL = re.compile(r"-?\d+(\.\d+)?")
RE_GANZZAHL = re.compile(r"\d+")
FESTKOMMA_EINS = 65536  # Festkomma des Kerns: 16 Nachkommabits (core/include/uhrwerk/fixed.hpp)

# Schwellen für Warnungen (Ausreißer); bewusst großzügig, sie sollen Tippfehler finden.
FLAECHE_MIN_KM2 = 10.0
FLAECHE_MAX_KM2 = 5000.0
DICHTE_MAX_LAND = 150.0  # Einwohner je km² ohne Stadt
DICHTE_MAX = 5000.0
WACHSTUM_MAX = 0.05  # +5 % je Jahr zwischen zwei Stichtagen
SCHWUND_MAX = 0.10  # −10 % je Jahr


# ─── Meldungen ──────────────────────────────────────────────────────────────


@dataclass
class Meldungen:
    fehler: list[str] = field(default_factory=list)
    warnungen: list[str] = field(default_factory=list)

    def fehler_bei(self, datei: str, zeile: int, text: str) -> None:
        self.fehler.append(f"{_ort(datei, zeile)}{text}")

    def warnung_bei(self, datei: str, zeile: int, text: str) -> None:
        self.warnungen.append(f"{_ort(datei, zeile)}{text}")


def _ort(datei: str, zeile: int) -> str:
    if not datei:
        return ""
    return f"{datei}:{zeile}: " if zeile > 0 else f"{datei}: "


# ─── Werte ──────────────────────────────────────────────────────────────────

Datum = tuple[int, int, int]


def parse_datum(text: str) -> Datum | None:
    """JJJJ.MM.TT im gregorianischen Kalender ab dem Jahr 1 (wie der Kern), sonst None."""
    m = RE_DATUM.fullmatch(text)
    if not m:
        return None
    jahr, monat, tag = (int(g) for g in m.groups())
    if jahr < 1 or not 1 <= monat <= 12 or tag < 1:
        return None
    schalt = jahr % 4 == 0 and (jahr % 100 != 0 or jahr % 400 == 0)
    tage = (31, 29 if schalt else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31)[monat - 1]
    return (jahr, monat, tag) if tag <= tage else None


def datum_text(d: Datum) -> str:
    return f"{d[0]:04d}.{d[1]:02d}.{d[2]:02d}"


def jahre_zwischen(a: Datum, b: Datum) -> float:
    return (b[0] - a[0]) + (b[1] - a[1]) / 12 + (b[2] - a[2]) / 365


def chron_text(text: str) -> str:
    """Zeichenkette für das Chronik-Skript mit Escapes für \\ und \".

    Zeilenumbrüche sind in Zeichenketten nicht erlaubt (chronik-skript.md 1.1); die Prüfung dafür
    steht in _einzeilig bzw. _pruefe_staatswert.
    """
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def hat_zeilenumbruch(text: str) -> bool:
    """LF oder CR: beides beendet im Kern eine Zeichenkette mit Fehler."""
    return "\n" in text or "\r" in text


def festkomma_roh(text: str) -> int:
    """Rohwert, den der Kern beim Lesen als Festkomma speichert (Fixed::parse: kaufmännisch gerundet)."""
    wert = Fraction(text) * FESTKOMMA_EINS
    betrag = abs(wert)
    roh = int(betrag) + (1 if betrag - int(betrag) >= Fraction(1, 2) else 0)
    return -roh if wert < 0 else roh


# ─── Tabellen lesen ─────────────────────────────────────────────────────────


@dataclass
class Zeile:
    datei: str  # z. B. "tabellen/orte.csv"
    nr: int  # Zeilennummer in der Datei (Kopfzeile = 1)
    werte: dict[str, str]

    def __getitem__(self, spalte: str) -> str:
        return self.werte[spalte]


def lese_tabelle(ordner: Path, name: str, meldungen: Meldungen) -> list[Zeile]:
    anzeige = f"tabellen/{name}"
    pfad = ordner / name
    try:
        rohtext = pfad.read_bytes()
    except FileNotFoundError:
        meldungen.fehler_bei(anzeige, 0, "Tabelle fehlt")
        return []
    except OSError as e:
        meldungen.fehler_bei(anzeige, 0, f"Tabelle kann nicht gelesen werden: {e.strerror}")
        return []
    try:
        text = rohtext.decode("utf-8-sig")
    except UnicodeDecodeError as e:
        meldungen.fehler_bei(anzeige, 0, f"ungültiges UTF-8 an Byte {e.start}")
        return []

    spalten = SPALTEN[name]
    leser = csv.reader(text.splitlines(keepends=True), delimiter=";")
    zeilen: list[Zeile] = []
    kopf_gesehen = False
    for satz in leser:
        nr = leser.line_num
        if not kopf_gesehen:
            kopf_gesehen = True
            if tuple(s.strip() for s in satz) != spalten:
                meldungen.fehler_bei(
                    anzeige, nr, f"Kopfzeile erwartet: {';'.join(spalten)} (gefunden: {';'.join(satz)})"
                )
                return []
            continue
        if not satz or all(not s.strip() for s in satz):
            continue  # leere Zeile
        if len(satz) != len(spalten):
            meldungen.fehler_bei(anzeige, nr, f"erwartet {len(spalten)} Spalten, gefunden {len(satz)}")
            continue
        werte = {s: w.strip().replace("\r\n", "\n") for s, w in zip(spalten, satz)}
        zeilen.append(Zeile(anzeige, nr, werte))
    if not kopf_gesehen:
        meldungen.fehler_bei(anzeige, 0, "Tabelle ist leer (Kopfzeile fehlt)")
    return zeilen


# ─── Bezugsdaten aus data/common, data/szenarien, data/karte ────────────────


@dataclass
class Szenario:
    kennung: str
    datum: Datum
    datei: str


@dataclass
class Bezug:
    gelaende: set[str] = field(default_factory=set)
    konfessionen: set[str] = field(default_factory=set)
    kulturen: set[str] = field(default_factory=set)
    steuersatz_max: float = 1.0
    szenarien: list[Szenario] = field(default_factory=list)
    karte: tuple[float, float, float, float] | None = None  # laenge_min, laenge_max, breite_min, breite_max


def _lade_chron(pfad: Path, anzeige: str, meldungen: Meldungen) -> chron.ScriptNode | None:
    if not pfad.is_file():
        meldungen.fehler_bei(anzeige, 0, "Datei fehlt")
        return None
    ergebnis = chron.parse_script_file(pfad, anzeige)
    for e in ergebnis.errors:
        meldungen.fehler.append(str(e))
    return ergebnis.root if ergebnis.ok else None


def lade_bezug(daten: Path, meldungen: Meldungen) -> Bezug:
    bezug = Bezug()
    if root := _lade_chron(daten / "common/gelaende.chron", "common/gelaende.chron", meldungen):
        bezug.gelaende = {n.value for n in root.children if n.key == "gelaende" and n.is_block}
    if root := _lade_chron(daten / "common/konfessionen.chron", "common/konfessionen.chron", meldungen):
        bezug.konfessionen = {n.value for n in root.children if n.key == "konfession" and n.is_block}
    if root := _lade_chron(daten / "common/kulturen.chron", "common/kulturen.chron", meldungen):
        bezug.kulturen = {n.value for n in root.children if n.key == "kultur" and n.is_block}
    if root := _lade_chron(daten / "common/regeln.chron", "common/regeln.chron", meldungen):
        regeln = root.find("regeln")
        wert = regeln.get_zahl("steuersatz_max") if regeln else None
        if wert is not None:
            bezug.steuersatz_max = wert

    ordner = daten / "szenarien"
    for pfad in sorted(ordner.glob("*.chron")) if ordner.is_dir() else []:
        anzeige = f"szenarien/{pfad.name}"
        root = _lade_chron(pfad, anzeige, meldungen)
        for node in root.children if root else []:
            if node.key != "szenario" or not node.is_block:
                continue
            datum = parse_datum(node.get_string("datum") or "")
            if datum is None:
                meldungen.fehler_bei(anzeige, node.line, f"szenario {node.value}: 'datum' fehlt oder ist ungültig")
                continue
            bezug.szenarien.append(Szenario(node.value, datum, anzeige))
    if not bezug.szenarien:
        meldungen.fehler_bei("szenarien", 0, "keine Szenarien gefunden")

    karte_pfad = daten / "karte/karte.chron"
    if karte_pfad.is_file():
        root = _lade_chron(karte_pfad, "karte/karte.chron", meldungen)
        karte = root.find("karte") if root else None
        if karte is not None:
            werte = [karte.get_zahl(k) for k in ("laenge_min", "laenge_max", "breite_min", "breite_max")]
            if all(w is not None for w in werte):
                bezug.karte = (werte[0], werte[1], werte[2], werte[3])  # type: ignore[assignment]
    return bezug


# ─── Modell ─────────────────────────────────────────────────────────────────


@dataclass
class Eintrag:
    """Ein datierter Wert mit Herkunft."""

    datum: Datum
    wert: str
    zeile: Zeile


@dataclass
class Ort:
    zeile: Zeile
    besitz: list[Eintrag] = field(default_factory=list)
    bevoelkerung: list[Zeile] = field(default_factory=list)  # nach Stichtag sortiert


@dataclass
class Staat:
    zeile: Zeile
    historie: dict[str, list[Eintrag]] = field(default_factory=dict)  # Feld → Einträge nach Datum


@dataclass
class Archiv:
    orte: dict[str, Ort] = field(default_factory=dict)  # in Tabellenreihenfolge
    kreise: dict[str, Zeile] = field(default_factory=dict)  # in Tabellenreihenfolge
    staaten: dict[str, Staat] = field(default_factory=dict)
    kommentare: dict[tuple[str, str], str] = field(default_factory=dict)  # (ordner, datei) → Text
    kulturen: dict[str, str] = field(default_factory=dict)  # Datei in history/orte → Standardkultur
    zeilen: dict[str, list[Zeile]] = field(default_factory=dict)  # alle Tabellen (für den Bericht)


def _pflicht(z: Zeile, spalte: str, m: Meldungen) -> bool:
    if not z[spalte]:
        m.fehler_bei(z.datei, z.nr, f"Spalte '{spalte}' darf nicht leer sein")
        return False
    return True


def _einzeilig(z: Zeile, spalte: str, m: Meldungen) -> bool:
    """Werte, die als Zeichenkette ins Chronik-Skript gehen, dürfen keinen Zeilenumbruch enthalten."""
    if hat_zeilenumbruch(z[spalte]):
        m.fehler_bei(z.datei, z.nr, f"Zeilenumbruch in Spalte '{spalte}' (im Chronik-Skript nicht erlaubt)")
        return False
    return True


def _herkunft(z: Zeile, m: Meldungen) -> None:
    if z["sicherheit"] not in SICHERHEIT:
        m.fehler_bei(
            z.datei, z.nr, f"ungültige Sicherheit '{z['sicherheit']}' (erlaubt: {', '.join(SICHERHEIT)})"
        )
    _pflicht(z, "quelle", m)


def _datum(z: Zeile, spalte: str, m: Meldungen) -> Datum | None:
    d = parse_datum(z[spalte])
    if d is None:
        m.fehler_bei(z.datei, z.nr, f"ungültiges Datum '{z[spalte]}' in Spalte '{spalte}' (erwartet JJJJ.MM.TT)")
    return d


def _dezimal(z: Zeile, spalte: str, lo: float, hi: float, m: Meldungen, wert: str | None = None) -> float | None:
    text = z[spalte] if wert is None else wert
    if not RE_DEZIMAL.fullmatch(text):
        m.fehler_bei(z.datei, z.nr, f"ungültige Zahl '{text}' in Spalte '{spalte}' (Dezimalpunkt, z. B. 0.25)")
        return None
    zahl = float(text)
    if not lo <= zahl <= hi:
        m.fehler_bei(z.datei, z.nr, f"Wert {text} in Spalte '{spalte}' außerhalb von {lo:g} bis {hi:g}")
        return None
    return zahl


def _einmalig(text: str, beschreibung: str, gesehen: dict[str, Zeile], z: Zeile, m: Meldungen) -> bool:
    if text in gesehen:
        m.fehler_bei(z.datei, z.nr, f"{beschreibung} ist doppelt (zuerst in Zeile {gesehen[text].nr})")
        return False
    gesehen[text] = z
    return True


def parse_anteile(text: str) -> list[tuple[str, int]] | None:
    """'lutherisch=95 reformiert=5' → [('lutherisch', 95), ('reformiert', 5)] (Konfession, Kultur)."""
    teile = []
    for paar in text.split():
        name, gleich, prozent = paar.partition("=")
        if not gleich or not RE_KENNUNG.fullmatch(name) or not RE_GANZZAHL.fullmatch(prozent):
            return None
        teile.append((name, int(prozent)))
    return teile or None


def _pruefe_anteile(z: Zeile, spalte: str, art: str, bekannt: set[str], quelle: str, m: Meldungen) -> bool:
    """Prüft eine Anteilsangabe wie 'lutherisch=95 reformiert=5' (Kennungen bekannt, einmalig, Summe 100)."""
    anteile = parse_anteile(z[spalte])
    if anteile is None:
        beispiel = "lutherisch=95 reformiert=5" if spalte == "konfession" else "maerker=80 sorben=20"
        m.fehler_bei(z.datei, z.nr, f"ungültige Angabe in Spalte '{spalte}': '{z[spalte]}' (z. B. {beispiel})")
        return False
    ok = True
    namen = [k for k, _ in anteile]
    for k in namen:
        if k not in bekannt:
            m.fehler_bei(z.datei, z.nr, f"unbekannte {art} '{k}' ({quelle})")
            ok = False
    if len(set(namen)) != len(namen):
        m.fehler_bei(z.datei, z.nr, f"eine {art} ist doppelt angegeben")
        ok = False
    summe = sum(p for _, p in anteile)
    if summe != 100:
        m.fehler_bei(z.datei, z.nr, f"die Anteile in Spalte '{spalte}' ergeben {summe} %, erwartet 100 %")
        ok = False
    return ok


def baue_archiv(tabellen: Path, bezug: Bezug, m: Meldungen) -> Archiv:
    archiv = Archiv()
    for name in SPALTEN:
        archiv.zeilen[name] = lese_tabelle(tabellen, name, m)

    # Stammdaten der Staaten
    gesehen: dict[str, Zeile] = {}
    for z in archiv.zeilen["staaten.csv"]:
        if not RE_TAG.fullmatch(z["tag"]):
            m.fehler_bei(z.datei, z.nr, f"ungültiger Staatstag '{z['tag']}' (2–4 Zeichen A–Z, 0–9)")
            continue
        if not _einmalig(z["tag"], f"Staat {z['tag']}", gesehen, z, m):
            continue
        if not RE_DATEI.fullmatch(z["datei"]):
            m.fehler_bei(z.datei, z.nr, f"ungültiger Dateiname '{z['datei']}' (A–Z, a–z, 0–9, _)")
        for spalte in ("name", "kurzname", "quelle"):
            if _pflicht(z, spalte, m):
                _einzeilig(z, spalte, m)
        farbe = z["farbe"].split()
        if len(farbe) != 3 or not all(RE_GANZZAHL.fullmatch(f) and int(f) <= 255 for f in farbe):
            m.fehler_bei(z.datei, z.nr, f"ungültige Farbe '{z['farbe']}' (erwartet drei Zahlen 0–255, z. B. 58 95 143)")
        archiv.staaten[z["tag"]] = Staat(z)

    # Kreise
    gesehen = {}
    for z in archiv.zeilen["kreise.csv"]:
        if not RE_ORT.fullmatch(z["id"]):
            m.fehler_bei(z.datei, z.nr, f"ungültige Kreiskennung '{z['id']}' (a–z, 0–9, _)")
            continue
        if not _einmalig(z["id"], f"Kreis {z['id']}", gesehen, z, m):
            continue
        if not RE_ORT.fullmatch(z["datei"]):
            m.fehler_bei(z.datei, z.nr, f"ungültiger Dateiname '{z['datei']}' (a–z, 0–9, _)")
        for spalte in ("name", "quelle"):
            if _pflicht(z, spalte, m):
                _einzeilig(z, spalte, m)
        archiv.kreise[z["id"]] = z

    # Stammdaten der Orte
    gesehen = {}
    ohne_kreis: list[Zeile] = []
    for z in archiv.zeilen["orte.csv"]:
        if not RE_ORT.fullmatch(z["id"]):
            m.fehler_bei(z.datei, z.nr, f"ungültige Ortskennung '{z['id']}' (a–z, 0–9, _)")
            continue
        if not _einmalig(z["id"], f"Ort {z['id']}", gesehen, z, m):
            continue
        if not RE_ORT.fullmatch(z["datei"]):
            m.fehler_bei(z.datei, z.nr, f"ungültiger Dateiname '{z['datei']}' (a–z, 0–9, _)")
        for spalte in ("name", "quelle"):
            if _pflicht(z, spalte, m):
                _einzeilig(z, spalte, m)
        if z["gelaende"] not in bezug.gelaende:
            m.fehler_bei(z.datei, z.nr, f"unbekanntes Gelände '{z['gelaende']}' (common/gelaende.chron)")
        laenge = _dezimal(z, "laenge", -180, 180, m)
        breite = _dezimal(z, "breite", -90, 90, m)
        if laenge is not None and breite is not None and bezug.karte is not None:
            lmin, lmax, bmin, bmax = bezug.karte
            if not (lmin <= laenge <= lmax and bmin <= breite <= bmax):
                m.warnung_bei(z.datei, z.nr, f"Ort {z['id']} liegt außerhalb der Karte (karte/karte.chron)")
        flaeche = _dezimal(z, "flaeche", 0, 1_000_000, m)
        if flaeche is not None:
            if flaeche <= 0:
                m.fehler_bei(z.datei, z.nr, "Fläche muss größer als 0 sein")
            elif festkomma_roh(z["flaeche"]) == 0:
                # Der Kern liest die Fläche als Festkomma mit 16 Nachkommabits; so klein wird sie 0.
                m.fehler_bei(
                    z.datei,
                    z.nr,
                    f"Fläche {z['flaeche']} km² ist für den Kern zu klein (Festkomma rundet auf 0; mindestens 0.00001)",
                )
            elif not FLAECHE_MIN_KM2 <= flaeche <= FLAECHE_MAX_KM2:
                m.warnung_bei(z.datei, z.nr, f"ungewöhnliche Fläche {z['flaeche']} km² für Ort {z['id']}")
        if z["stadt"] not in ("ja", "nein"):
            m.fehler_bei(z.datei, z.nr, f"Spalte 'stadt' erwartet ja oder nein, nicht '{z['stadt']}'")
        if not z["kreis"]:
            ohne_kreis.append(z)
        elif z["kreis"] not in archiv.kreise:
            m.fehler_bei(z.datei, z.nr, f"unbekannter Kreis '{z['kreis']}' (kreise.csv)")
        archiv.orte[z["id"]] = Ort(z)
    if ohne_kreis:
        beispiele = ", ".join(z["id"] for z in ohne_kreis[:5]) + (" …" if len(ohne_kreis) > 5 else "")
        m.warnung_bei(
            ohne_kreis[0].datei, ohne_kreis[0].nr, f"{len(ohne_kreis)} Orte ohne Kreis (Spalte 'kreis' leer): {beispiele}"
        )
    belegt = {o.zeile["kreis"] for o in archiv.orte.values()}
    for kennung, z in archiv.kreise.items():
        if kennung not in belegt:
            m.warnung_bei(z.datei, z.nr, f"Kreis {kennung} hat keine Orte")

    # Besitz
    gesehen = {}
    for z in archiv.zeilen["besitz.csv"]:
        _herkunft(z, m)
        datum = _datum(z, "ab", m)
        ort = archiv.orte.get(z["ort"])
        if ort is None:
            m.fehler_bei(z.datei, z.nr, f"unbekannter Ort '{z['ort']}'")
            continue
        if z["besitzer"] not in archiv.staaten:
            m.fehler_bei(z.datei, z.nr, f"unbekannter Staat '{z['besitzer']}' als Besitzer von {z['ort']}")
            continue
        if datum is None or not _einmalig(f"{z['ort']}@{z['ab']}", f"Besitz von {z['ort']} ab {z['ab']}", gesehen, z, m):
            continue
        ort.besitz.append(Eintrag(datum, z["besitzer"], z))

    # Bevölkerung
    gesehen = {}
    for z in archiv.zeilen["bevoelkerung.csv"]:
        _herkunft(z, m)
        datum = _datum(z, "stichtag", m)
        ort = archiv.orte.get(z["ort"])
        if ort is None:
            m.fehler_bei(z.datei, z.nr, f"unbekannter Ort '{z['ort']}'")
            continue
        ok = datum is not None
        if not RE_GANZZAHL.fullmatch(z["bevoelkerung"]) or not 1 <= int(z["bevoelkerung"]) <= 100_000_000:
            m.fehler_bei(z.datei, z.nr, f"ungültige Bevölkerung '{z['bevoelkerung']}' (Ganzzahl 1 bis 100000000)")
            ok = False
        if not _pruefe_anteile(z, "konfession", "Konfession", bezug.konfessionen, "common/konfessionen.chron", m):
            ok = False
        # Kultur leer: Standardkultur der Landschaft (dateien.csv), geprüft nach den Kopfkommentaren.
        if z["kultur"] and not _pruefe_anteile(z, "kultur", "Kultur", bezug.kulturen, "common/kulturen.chron", m):
            ok = False
        if _dezimal(z, "verwuestung", 0, 1, m) is None:
            ok = False
        if ok and _einmalig(f"{z['ort']}@{z['stichtag']}", f"Stichtag {z['stichtag']} für {z['ort']}", gesehen, z, m):
            ort.bevoelkerung.append(z)

    # Historie der Staaten
    gesehen = {}
    for z in archiv.zeilen["staaten_historie.csv"]:
        _herkunft(z, m)
        datum = _datum(z, "ab", m)
        staat = archiv.staaten.get(z["tag"])
        if staat is None:
            m.fehler_bei(z.datei, z.nr, f"unbekannter Staat '{z['tag']}'")
            continue
        feld_name, wert = z["feld"], z["wert"]
        if feld_name not in STAAT_FELDER:
            m.fehler_bei(z.datei, z.nr, f"unbekanntes Feld '{feld_name}' (erlaubt: {', '.join(STAAT_FELDER)})")
            continue
        if not _pruefe_staatswert(z, feld_name, wert, archiv, bezug, m) or datum is None:
            continue
        if _einmalig(f"{z['tag']}@{z['ab']}@{feld_name}", f"{feld_name} von {z['tag']} ab {z['ab']}", gesehen, z, m):
            staat.historie.setdefault(feld_name, []).append(Eintrag(datum, wert, z))

    for ort in archiv.orte.values():
        ort.besitz.sort(key=lambda e: e.datum)
        ort.bevoelkerung.sort(key=lambda z: parse_datum(z["stichtag"]) or (0, 0, 0))
    for staat in archiv.staaten.values():
        for eintraege in staat.historie.values():
            eintraege.sort(key=lambda e: e.datum)

    # Kopfkommentare
    gesehen = {}
    benutzt = {("orte", o.zeile["datei"]) for o in archiv.orte.values()}
    benutzt |= {("staaten", s.zeile["datei"]) for s in archiv.staaten.values()}
    benutzt |= {("kreise", k["datei"]) for k in archiv.kreise.values()}
    for z in archiv.zeilen["dateien.csv"]:
        if z["ordner"] not in ORDNER:
            m.fehler_bei(
                z.datei, z.nr, f"Spalte 'ordner' erwartet {', '.join(ORDNER)}, nicht '{z['ordner']}'"
            )
            continue
        schluessel = (z["ordner"], z["datei"])
        if not _einmalig("/".join(schluessel), f"Datei {z['ordner']}/{z['datei']}", gesehen, z, m):
            continue
        if z["kultur"]:
            if z["ordner"] != "orte":
                m.fehler_bei(z.datei, z.nr, f"Spalte 'kultur' gilt nur für Dateien in history/orte, nicht für {z['ordner']}")
            elif _pruefe_anteile(z, "kultur", "Kultur", bezug.kulturen, "common/kulturen.chron", m):
                archiv.kulturen[z["datei"]] = z["kultur"]
        if schluessel not in benutzt:
            m.warnung_bei(z.datei, z.nr, f"Kommentar für {z['ordner']}/{z['datei']}, aber kein Eintrag nutzt diese Datei")
            continue
        archiv.kommentare[schluessel] = z["kommentar"]

    # Jede Bevölkerungsangabe braucht eine Kultur: aus der Zeile oder als Standard der Landschaft.
    for kennung, ort in archiv.orte.items():
        for b in ort.bevoelkerung:
            if not b["kultur"] and ort.zeile["datei"] not in archiv.kulturen:
                m.fehler_bei(
                    b.datei,
                    b.nr,
                    f"keine Kultur für {kennung} ({b['stichtag']}): Spalte 'kultur' ist leer, und dateien.csv "
                    f"nennt keine Standardkultur für orte/{ort.zeile['datei']}",
                )
    return archiv


def kultur_von(archiv: Archiv, ort: Ort, b: Zeile) -> str:
    """Kulturangabe einer Bevölkerungszeile: eigene Spalte, sonst Standard der Landschaft."""
    return b["kultur"] or archiv.kulturen.get(ort.zeile["datei"], "")


def _pruefe_staatswert(z: Zeile, feld_name: str, wert: str, archiv: Archiv, bezug: Bezug, m: Meldungen) -> bool:
    if not wert:
        m.fehler_bei(z.datei, z.nr, f"leerer Wert für '{feld_name}'")
        return False
    if feld_name == "hauptstadt" and wert not in archiv.orte:
        m.fehler_bei(z.datei, z.nr, f"unbekannter Ort '{wert}' als Hauptstadt von {z['tag']}")
        return False
    if feld_name == "konfession" and wert not in bezug.konfessionen:
        m.fehler_bei(z.datei, z.nr, f"unbekannte Konfession '{wert}' (common/konfessionen.chron)")
        return False
    if feld_name == "regierungsform" and not RE_KENNUNG.fullmatch(wert):
        m.fehler_bei(z.datei, z.nr, f"ungültige Regierungsform '{wert}' (a–z, 0–9, _)")
        return False
    if feld_name == "schatz":
        return _dezimal(z, "wert", -1e12, 1e12, m, wert) is not None
    if feld_name == "steuersatz":
        return _dezimal(z, "wert", 0, bezug.steuersatz_max, m, wert) is not None
    if feld_name == "hofhaltung":
        return _dezimal(z, "wert", 0, 1e12, m, wert) is not None
    if feld_name in TEXT_FELDER and hat_zeilenumbruch(wert):
        m.fehler_bei(z.datei, z.nr, f"Zeilenumbruch im Wert für '{feld_name}'")
        return False
    return True


# ─── Lücken und Ausreißer ───────────────────────────────────────────────────


def gueltig(eintraege: list[Eintrag], stichtag: Datum) -> Eintrag | None:
    """Letzter Eintrag mit Datum ≤ Stichtag (Einträge sind sortiert)."""
    treffer = None
    for e in eintraege:
        if e.datum > stichtag:
            break
        treffer = e
    return treffer


def pruefe_zeitraeume(archiv: Archiv, bezug: Bezug, m: Meldungen) -> None:
    for szenario in bezug.szenarien:
        wann = f"zum Szenario {szenario.kennung} ({datum_text(szenario.datum)})"
        besitzer_am: dict[str, str] = {}
        for kennung, ort in archiv.orte.items():
            # Ein Ort hat zum Datum Besitzer und Bevölkerung – oder keines von beiden (Ort ohne Daten).
            z = ort.zeile
            besitz = gueltig(ort.besitz, szenario.datum)
            hat_bevoelkerung = any(
                (parse_datum(b["stichtag"]) or (9999, 0, 0)) <= szenario.datum for b in ort.bevoelkerung
            )
            if besitz is not None:
                besitzer_am[kennung] = besitz.wert
                if not hat_bevoelkerung:
                    m.fehler_bei(
                        z.datei, z.nr, f"Ort {kennung} hat {wann} einen Besitzer, aber keine Bevölkerungsangabe"
                    )
            elif hat_bevoelkerung:
                m.fehler_bei(z.datei, z.nr, f"Ort {kennung} hat {wann} eine Bevölkerungsangabe, aber keinen Besitzer")
        for tag, staat in archiv.staaten.items():
            z = staat.zeile
            for feld_name in STAAT_PFLICHT:
                if gueltig(staat.historie.get(feld_name, []), szenario.datum) is None:
                    m.fehler_bei(z.datei, z.nr, f"Staat {tag} hat {wann} keinen Wert für '{feld_name}' (staaten_historie.csv)")
            hauptstadt = gueltig(staat.historie.get("hauptstadt", []), szenario.datum)
            if hauptstadt is not None:
                hz = hauptstadt.zeile
                besitzer = besitzer_am.get(hauptstadt.wert)
                if besitzer is None:
                    m.fehler_bei(hz.datei, hz.nr, f"Hauptstadt {hauptstadt.wert} von {tag} ist {wann} ein Ort ohne Daten")
                elif besitzer != tag:
                    m.fehler_bei(hz.datei, hz.nr, f"Hauptstadt {hauptstadt.wert} von {tag} gehört {wann} zu {besitzer}")
            if tag not in besitzer_am.values():
                m.warnung_bei(z.datei, z.nr, f"Staat {tag} besitzt {wann} keinen Ort")


def pruefe_ausreisser(archiv: Archiv, m: Meldungen) -> None:
    for kennung, ort in archiv.orte.items():
        try:
            flaeche = float(ort.zeile["flaeche"])
        except ValueError:
            continue
        stadt = ort.zeile["stadt"] == "ja"
        vorher: tuple[Datum, int] | None = None
        for z in ort.bevoelkerung:
            datum = parse_datum(z["stichtag"])
            if datum is None or flaeche <= 0:
                continue
            einwohner = int(z["bevoelkerung"])
            dichte = einwohner / flaeche
            if dichte > DICHTE_MAX or (not stadt and dichte > DICHTE_MAX_LAND):
                m.warnung_bei(z.datei, z.nr, f"hohe Dichte {dichte:.0f} Einwohner/km² in {kennung} ({z['stichtag']})")
            if vorher is not None:
                jahre = jahre_zwischen(vorher[0], datum)
                if jahre > 0:
                    rate = (einwohner / vorher[1]) ** (1 / jahre) - 1
                    if rate > WACHSTUM_MAX or rate < -SCHWUND_MAX:
                        m.warnung_bei(
                            z.datei,
                            z.nr,
                            f"auffällige Bevölkerungsänderung in {kennung}: {rate * 100:+.1f} % je Jahr "
                            f"seit {datum_text(vorher[0])}",
                        )
            vorher = (datum, einwohner)


# ─── Ausgabe ────────────────────────────────────────────────────────────────


def _ausgerichtet(paare: list[tuple[str, str]], einzug: str) -> list[str]:
    breite = max(len(k) for k, _ in paare)
    return [f"{einzug}{k.ljust(breite)} = {v}" for k, v in paare]


def _kopf(kommentar: str) -> list[str]:
    zeilen = [f"# {KOPFZEILE}"]
    if kommentar:
        zeilen.append("#")
        zeilen += [f"# {z}".rstrip() for z in kommentar.split("\n")]
    return zeilen + [""]


def _datierte_bloecke(bloecke: dict[Datum, list[tuple[str, str]]]) -> list[str]:
    zeilen: list[str] = []
    for datum in sorted(bloecke):
        zeilen.append("")
        zeilen.append(f"    {datum_text(datum)} = {{")
        zeilen += _ausgerichtet(bloecke[datum], "        ")
        zeilen.append("    }")
    return zeilen


def ort_text(ort: Ort, archiv: Archiv) -> list[str]:
    z = ort.zeile
    zeilen = [f"ort {z['id']} {{"]
    zeilen += _ausgerichtet(
        [
            ("name", chron_text(z["name"])),
            ("gelaende", z["gelaende"]),
            ("lage", f"{{ {z['laenge']} {z['breite']} }}"),
            ("flaeche", z["flaeche"]),
            ("stadt", z["stadt"]),
            *([("kreis", z["kreis"])] if z["kreis"] else []),
            ("quelle", chron_text(z["quelle"])),
        ],
        "    ",
    )
    bloecke: dict[Datum, list[tuple[str, str]]] = {}
    for e in ort.besitz:
        bloecke.setdefault(e.datum, []).append(("besitzer", e.wert))
    for b in ort.bevoelkerung:
        datum = parse_datum(b["stichtag"])
        assert datum is not None
        konfession = " ".join(f"{k} = {p}" for k, p in parse_anteile(b["konfession"]) or [])
        kultur = " ".join(f"{k} = {p}" for k, p in parse_anteile(kultur_von(archiv, ort, b)) or [])
        bloecke.setdefault(datum, []).extend(
            [
                ("bevoelkerung", b["bevoelkerung"]),
                ("konfession", f"{{ {konfession} }}"),
                ("kultur", f"{{ {kultur} }}"),
                ("verwuestung", b["verwuestung"]),
            ]
        )
    zeilen += _datierte_bloecke(bloecke)
    zeilen.append("}")
    return zeilen


def staat_text(staat: Staat) -> list[str]:
    z = staat.zeile
    zeilen = [f"staat {z['tag']} {{"]
    zeilen += _ausgerichtet(
        [
            ("name", chron_text(z["name"])),
            ("kurzname", chron_text(z["kurzname"])),
            ("farbe", f"{{ {' '.join(z['farbe'].split())} }}"),
            ("quelle", chron_text(z["quelle"])),
        ],
        "    ",
    )
    bloecke: dict[Datum, list[tuple[str, str]]] = {}
    for feld_name in STAAT_FELDER:
        for e in staat.historie.get(feld_name, []):
            wert = chron_text(e.wert) if feld_name in TEXT_FELDER else e.wert
            bloecke.setdefault(e.datum, []).append((feld_name, wert))
    zeilen += _datierte_bloecke(bloecke)
    zeilen.append("}")
    return zeilen


def kreis_text(z: Zeile) -> list[str]:
    return [f"kreis {z['id']} {{", *_ausgerichtet([("name", chron_text(z["name"])), ("quelle", chron_text(z["quelle"]))], "    "), "}"]


def erzeuge(archiv: Archiv) -> dict[str, str]:
    """Alle Ausgabedateien: relativer Pfad (z. B. history/orte/anhalt.chron) → Inhalt."""
    gruppen: dict[tuple[str, str], list[list[str]]] = {}
    for staat in archiv.staaten.values():
        gruppen.setdefault(("staaten", staat.zeile["datei"]), []).append(staat_text(staat))
    for kreis in archiv.kreise.values():
        gruppen.setdefault(("kreise", kreis["datei"]), []).append(kreis_text(kreis))
    for ort in archiv.orte.values():
        gruppen.setdefault(("orte", ort.zeile["datei"]), []).append(ort_text(ort, archiv))
    ausgabe: dict[str, str] = {}
    for (ordner, datei), definitionen in sorted(gruppen.items()):
        zeilen = _kopf(archiv.kommentare.get((ordner, datei), ""))
        for i, definition in enumerate(definitionen):
            if i > 0:
                zeilen.append("")
            zeilen += definition
        ausgabe[f"history/{ordner}/{datei}.chron"] = "\n".join(zeilen) + "\n"
    return ausgabe


def vorhandene(daten: Path) -> dict[str, Path]:
    """Alle .chron-Dateien in den vom Archivar erzeugten Ordnern unter history/."""
    ergebnis = {}
    for ordner in ORDNER:
        verzeichnis = daten / "history" / ordner
        if verzeichnis.is_dir():
            for pfad in sorted(verzeichnis.glob("*.chron")):
                ergebnis[f"history/{ordner}/{pfad.name}"] = pfad
    return ergebnis


def unterschiede(daten: Path, ausgabe: dict[str, str]) -> list[str]:
    alt = vorhandene(daten)
    meldungen = []
    for rel, inhalt in ausgabe.items():
        if rel not in alt:
            meldungen.append(f"fehlt: {rel}")
        elif alt[rel].read_bytes() != inhalt.encode("utf-8"):
            meldungen.append(f"veraltet: {rel}")
    meldungen += [f"überzählig: {rel}" for rel in alt if rel not in ausgabe]
    return meldungen


def schreibe(daten: Path, ausgabe: dict[str, str]) -> tuple[int, int]:
    geschrieben = 0
    for rel, inhalt in ausgabe.items():
        pfad = daten / rel
        daten_bytes = inhalt.encode("utf-8")
        if pfad.is_file() and pfad.read_bytes() == daten_bytes:
            continue
        pfad.parent.mkdir(parents=True, exist_ok=True)
        pfad.write_bytes(daten_bytes)
        geschrieben += 1
    entfernt = 0
    for rel, pfad in vorhandene(daten).items():
        if rel not in ausgabe:
            pfad.unlink()
            entfernt += 1
    return geschrieben, entfernt


# ─── Bericht ────────────────────────────────────────────────────────────────


def bericht(archiv: Archiv, bezug: Bezug) -> str:
    zeilen = [
        f"Orte: {len(archiv.orte)}, Kreise: {len(archiv.kreise)}, Staaten: {len(archiv.staaten)}",
        "",
        "Sicherheit der Angaben je Tabelle:",
    ]
    for name in ("besitz.csv", "bevoelkerung.csv", "staaten_historie.csv"):
        zaehler = Counter(z["sicherheit"] for z in archiv.zeilen.get(name, []))
        teile = ", ".join(f"{s} {zaehler[s]}" for s in SICHERHEIT if zaehler[s])
        zeilen.append(f"  {name:<22} {len(archiv.zeilen.get(name, [])):>5} Zeilen  ({teile or '–'})")
    zeilen += ["", "Szenarien:"]
    for s in bezug.szenarien:
        besitzer = Counter(
            e.wert for o in archiv.orte.values() if (e := gueltig(o.besitz, s.datum)) is not None
        )
        mit_daten = sum(besitzer.values())
        zeilen.append(
            f"  {s.kennung} ({datum_text(s.datum)}): {mit_daten} Orte mit Daten, "
            f"{len(archiv.orte) - mit_daten} ohne Daten, {len(besitzer)} Staaten mit Orten"
        )
    return "\n".join(zeilen)


# ─── Kommandozeile ──────────────────────────────────────────────────────────


def main(argv: list[str] | None = None) -> int:
    standard = Path(__file__).resolve().parent.parent.parent / "data"
    parser = argparse.ArgumentParser(
        prog="archivar.py",
        description="Erzeugt data/history/ aus den Tabellen in data/tabellen/ und prüft sie.",
    )
    parser.add_argument("--daten", type=Path, default=standard, help="Datenordner (Standard: <repo>/data)")
    gruppe = parser.add_mutually_exclusive_group()
    gruppe.add_argument("--pruefen", action="store_true", help="nur prüfen, ob history/ aktuell ist")
    gruppe.add_argument("--bericht", action="store_true", help="Übersicht über Umfang und Sicherheit")
    args = parser.parse_args(argv)

    daten: Path = args.daten
    m = Meldungen()
    bezug = lade_bezug(daten, m)
    archiv = baue_archiv(daten / "tabellen", bezug, m)
    if not m.fehler:
        pruefe_zeitraeume(archiv, bezug, m)
        pruefe_ausreisser(archiv, m)
    for text in m.warnungen:
        print(f"Warnung: {text}", file=sys.stderr)
    for text in m.fehler:
        print(f"Fehler: {text}", file=sys.stderr)
    if m.fehler:
        print(f"Abbruch: {len(m.fehler)} Fehler in den Tabellen.", file=sys.stderr)
        return 2

    if args.bericht:
        print(bericht(archiv, bezug))
        return 0
    ausgabe = erzeuge(archiv)
    if args.pruefen:
        abweichungen = unterschiede(daten, ausgabe)
        if abweichungen:
            for text in abweichungen:
                print(text)
            print("history/ ist nicht aktuell – bitte python3 tools/archivar/archivar.py ausführen.")
            return 1
        print(
            f"history/ ist aktuell ({len(ausgabe)} Dateien, {len(archiv.orte)} Orte, {len(archiv.kreise)} Kreise, "
            f"{len(archiv.staaten)} Staaten)."
        )
        return 0
    geschrieben, entfernt = schreibe(daten, ausgabe)
    print(
        f"{len(ausgabe)} Dateien erzeugt ({geschrieben} geändert, {entfernt} entfernt): "
        f"{len(archiv.orte)} Orte, {len(archiv.kreise)} Kreise, {len(archiv.staaten)} Staaten."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
