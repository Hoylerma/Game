#!/usr/bin/env python3
"""Geodaten für EINIGKEIT: Quellen holen, Kartengeometrie erzeugen und prüfen.

Unterbefehle (siehe README.md im selben Ordner):

    python3 tools/geodaten/geodaten.py holen [GRUPPE ...] [--pinnen]
    python3 tools/geodaten/geodaten.py geometrie
    python3 tools/geodaten/geodaten.py pruefen
    python3 tools/geodaten/geodaten.py vorschau [--ausgabe DATEI.png] [--breite PIXEL] [--ausschnitt L B L B]

``holen`` lädt die in ``quellen.json`` gepinnten Dateien nach ``build/geodaten-cache/`` und prüft
ihre SHA-256-Werte; vorhandene, richtige Dateien werden nicht erneut geladen (Gruppen: geometrie,
referenz, orte, hoehen). ``geometrie`` schreibt ``data/karte/geometrie/{land,seen,fluesse}.geojson``
aus Natural Earth (zugeschnitten auf den Ausschnitt aus ``data/karte/karte.chron``, vereinfacht,
deterministisch). ``pruefen`` vergleicht diese Dateien mit einer Neuerzeugung aus dem
Zwischenspeicher (nur lokal, braucht die Rohdaten) und prüft den Aufbau von ``kernraum.geojson``.
``vorschau`` rastert Land, Seen, Flüsse und den Kernraum in ein PNG zur Sichtprüfung.

Abhängigkeiten: Python ab 3.10, numpy (für ``geometrie``, ``pruefen`` und ``vorschau``).
Exitcodes: 0 = in Ordnung, 1 = ``pruefen``: Dateien nicht aktuell oder Kernraum fehlerhaft,
2 = Fehler (Quellen fehlen, Prüfsumme falsch, karte.chron ungültig, Aufruf).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import shutil
import sys
import tarfile
import tempfile
import time
import urllib.error
import urllib.request
import zlib
from collections.abc import Callable, Sequence
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any, NoReturn

try:
    import geo
except ImportError:  # Aufruf als Modul von außerhalb des Werkzeugordners
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import geo

# ─── Konstanten ─────────────────────────────────────────────────────────────

REPO = geo.REPO
MANIFEST = Path(__file__).resolve().parent / "quellen.json"
ZWISCHENSPEICHER = REPO / "build/geodaten-cache"
GEOMETRIE_ORDNER = REPO / "data/karte/geometrie"
KARTE = REPO / "data/karte/karte.chron"

#: Rand um den Kartenausschnitt beim Zuschneiden (Grad): Polygone reichen über den Bildrand
#: hinaus, damit am Rand keine künstlichen Küsten entstehen.
RAND_GRAD = 0.25
#: Rand um den Kartenausschnitt für die Höhenkacheln (Grad), damit Statistiken im Umkreis auch
#: am Bildrand vollständig sind.
KACHEL_RAND_GRAD = 0.2
#: Toleranz der Vereinfachung in Kartenpixeln (Douglas-Peucker).
TOLERANZ_PIXEL = 1.0
#: Kleinere Flächen (Inseln, Seen, Löcher) entfallen; in Kartenpixeln (Pixel²).
MIN_FLAECHE_PIXEL = 2.0
#: Kürzere Flussstücke entfallen; in Kartenpixeln.
MIN_FLUSSLAENGE_PIXEL = 2.0
#: Nachkommastellen der Koordinaten (0,0001° ≈ 11 m; ein Kartenpixel ist mehrere hundert Meter).
STELLEN = 4
#: Zielgröße aller erzeugten Geometriedateien zusammen (Bytes).
MAX_GROESSE = 3_000_000

#: Wiederholungen beim Laden und Wartezeit dazwischen (Sekunden).
VERSUCHE = 4
WARTEZEIT = 3.0
#: Gleichzeitige Downloads von Höhenkacheln.
PARALLEL = 8
USER_AGENT = "einigkeit-geodaten/1.0 (+https://github.com/Hoylerma/Game)"


class GeodatenFehler(Exception):
    """Fehler mit deutscher Meldung für den Benutzer (Exitcode 2)."""


def _fehler(text: str) -> NoReturn:
    raise GeodatenFehler(text)


def _melde(text: str) -> None:
    print(text, flush=True)


def _warnung(text: str) -> None:
    print(f"Warnung: {text}", file=sys.stderr, flush=True)


# ─── Manifest ───────────────────────────────────────────────────────────────


@dataclass
class Quelle:
    """Eine gepinnte Datei aus quellen.json."""

    datei: str
    gruppe: str
    url: str
    sha256: str
    lizenz: str
    quellenangabe: str
    auspacken: dict[str, str] | None
    roh: dict[str, Any]


def lade_manifest(pfad: Path = MANIFEST) -> dict[str, Any]:
    """Liest quellen.json und prüft den Aufbau (wirft GeodatenFehler)."""
    try:
        with open(pfad, encoding="utf-8") as datei:
            manifest = json.load(datei)
    except (OSError, json.JSONDecodeError) as fehler:
        _fehler(f"{pfad}: {fehler}")
    if not isinstance(manifest, dict) or not isinstance(manifest.get("quellen"), list):
        _fehler(f"{pfad}: Liste 'quellen' fehlt")
    for nr, eintrag in enumerate(manifest["quellen"]):
        for feld in ("datei", "gruppe", "url", "sha256", "lizenz", "quellenangabe"):
            if not isinstance(eintrag.get(feld), str):
                _fehler(f"{pfad}: Quelle {nr + 1}: Feld '{feld}' fehlt")
        if Path(eintrag["datei"]).is_absolute() or ".." in Path(eintrag["datei"]).parts:
            _fehler(f"{pfad}: Quelle {nr + 1}: 'datei' muss ein relativer Pfad im Zwischenspeicher sein")
    kacheln = manifest.get("kacheln")
    if kacheln is not None:
        for feld in ("url", "datei", "zoom", "x", "y", "sha256", "lizenz", "quellenangabe"):
            if feld not in kacheln:
                _fehler(f"{pfad}: kacheln: Feld '{feld}' fehlt")
    return manifest


def quellen(manifest: dict[str, Any], gruppen: Sequence[str] | None = None) -> list[Quelle]:
    """Die Quellen des Manifests, optional nur bestimmter Gruppen."""
    ergebnis = []
    for eintrag in manifest["quellen"]:
        if gruppen and eintrag["gruppe"] not in gruppen:
            continue
        ergebnis.append(
            Quelle(
                datei=eintrag["datei"],
                gruppe=eintrag["gruppe"],
                url=eintrag["url"],
                sha256=eintrag["sha256"],
                lizenz=eintrag["lizenz"],
                quellenangabe=eintrag["quellenangabe"],
                auspacken=eintrag.get("auspacken"),
                roh=eintrag,
            )
        )
    return ergebnis


def schreibe_manifest(manifest: dict[str, Any], pfad: Path = MANIFEST) -> None:
    """Schreibt quellen.json (eingerückt, UTF-8, Zeilenende LF)."""
    text = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    with open(pfad, "w", encoding="utf-8", newline="\n") as datei:
        datei.write(text)


def sha256_datei(pfad: Path) -> str:
    """SHA-256 einer Datei als Hex-Text."""
    h = hashlib.sha256()
    with open(pfad, "rb") as datei:
        while True:
            block = datei.read(1 << 20)
            if not block:
                break
            h.update(block)
    return h.hexdigest()


def pruefe_datei(pfad: Path, sha256: str) -> bool:
    """Ist die Datei vorhanden und hat sie den erwarteten Prüfwert?"""
    return bool(sha256) and pfad.is_file() and sha256_datei(pfad) == sha256


# ─── Höhenkacheln (Web Mercator) ────────────────────────────────────────────


def kachel_x(laenge: float, zoom: int) -> int:
    """Spaltennummer der Kachel, die die Länge enthält."""
    return int(math.floor((laenge + 180.0) / 360.0 * (1 << zoom)))


def kachel_y(breite: float, zoom: int) -> int:
    """Zeilennummer der Kachel, die die Breite enthält (Web Mercator, Norden oben)."""
    phi = math.radians(breite)
    y = (1.0 - math.log(math.tan(phi) + 1.0 / math.cos(phi)) / math.pi) / 2.0
    return int(math.floor(y * (1 << zoom)))


def kachelbereich(karte: geo.Karte, zoom: int, rand: float = KACHEL_RAND_GRAD) -> tuple[list[int], list[int]]:
    """Kachelspalten [x0, x1] und -zeilen [y0, y1], die den Ausschnitt mit Rand abdecken."""
    lmin, bmin, lmax, bmax = karte.rahmen(rand)
    return [kachel_x(lmin, zoom), kachel_x(lmax, zoom)], [kachel_y(bmax, zoom), kachel_y(bmin, zoom)]


def kacheln_liste(kacheln: dict[str, Any]) -> list[tuple[int, int, int]]:
    """Alle Kacheln (z, x, y) des Manifestbereichs in fester Reihenfolge (x, dann y)."""
    z = int(kacheln["zoom"])
    x0, x1 = kacheln["x"]
    y0, y1 = kacheln["y"]
    return [(z, x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)]


def kachel_schluessel(z: int, x: int, y: int) -> str:
    return f"{z}/{x}/{y}"


# ─── Laden ──────────────────────────────────────────────────────────────────


def lade_url(url: str, ziel: Path) -> str:
    """Lädt eine URL nach ``ziel`` (über eine temporäre Datei) und gibt den SHA-256 zurück."""
    ziel.parent.mkdir(parents=True, exist_ok=True)
    temp = ziel.with_name(ziel.name + ".teil")
    letzter: Exception | None = None
    for versuch in range(VERSUCHE):
        try:
            anfrage = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            h = hashlib.sha256()
            with urllib.request.urlopen(anfrage, timeout=120) as antwort, open(temp, "wb") as datei:
                while True:
                    block = antwort.read(1 << 16)
                    if not block:
                        break
                    h.update(block)
                    datei.write(block)
            return h.hexdigest()
        except (urllib.error.URLError, OSError, TimeoutError) as fehler:
            letzter = fehler
            if isinstance(fehler, urllib.error.HTTPError) and fehler.code in (403, 404):
                break
            time.sleep(WARTEZEIT * (versuch + 1))
    temp.unlink(missing_ok=True)
    _fehler(f"{url} konnte nicht geladen werden: {letzter}")


def _uebernehmen(ziel: Path) -> None:
    temp = ziel.with_name(ziel.name + ".teil")
    os.replace(temp, ziel)


def _verwerfen(ziel: Path) -> None:
    ziel.with_name(ziel.name + ".teil").unlink(missing_ok=True)


def hole_quelle(quelle: Quelle, speicher: Path, pinnen: bool) -> bool:
    """Lädt eine Quelle, falls sie fehlt oder falsch ist. Gibt True zurück, wenn das Manifest geändert wurde."""
    ziel = speicher / quelle.datei
    geaendert = False
    if quelle.sha256 and pruefe_datei(ziel, quelle.sha256):
        _melde(f"  vorhanden  {quelle.datei}")
    else:
        _melde(f"  lade       {quelle.url}")
        sha = lade_url(quelle.url, ziel)
        if not quelle.sha256:
            if not pinnen:
                _verwerfen(ziel)
                _fehler(f"{quelle.datei} ist nicht gepinnt (SHA-256 {sha}); mit --pinnen in quellen.json eintragen")
            quelle.roh["sha256"] = quelle.sha256 = sha
            geaendert = True
            _melde(f"  gepinnt    {quelle.datei} {sha}")
        elif sha != quelle.sha256:
            _verwerfen(ziel)
            _fehler(
                f"{quelle.datei}: SHA-256 stimmt nicht (erwartet {quelle.sha256}, erhalten {sha}). "
                "Die Quelle hat sich geändert; Daten prüfen und den Eintrag in quellen.json bewusst erneuern."
            )
        _uebernehmen(ziel)
    if quelle.auspacken:
        geaendert |= _auspacken(quelle, speicher, pinnen)
    return geaendert


def _auspacken(quelle: Quelle, speicher: Path, pinnen: bool) -> bool:
    """Packt ein Glied eines tar.gz-Archivs aus und prüft seinen Prüfwert."""
    assert quelle.auspacken is not None
    angabe = quelle.auspacken
    ziel = speicher / angabe["datei"]
    if angabe.get("sha256") and pruefe_datei(ziel, angabe["sha256"]):
        _melde(f"  vorhanden  {angabe['datei']}")
        return False
    archiv = speicher / quelle.datei
    with tarfile.open(archiv, "r:gz") as tar:
        try:
            glied = tar.getmember(angabe["glied"])
        except KeyError:
            _fehler(f"{quelle.datei}: Glied {angabe['glied']} fehlt im Archiv")
        if not glied.isfile():
            _fehler(f"{quelle.datei}: {angabe['glied']} ist keine gewöhnliche Datei")
        quelle_datei = tar.extractfile(glied)
        assert quelle_datei is not None
        temp = ziel.with_name(ziel.name + ".teil")
        ziel.parent.mkdir(parents=True, exist_ok=True)
        with open(temp, "wb") as aus:
            shutil.copyfileobj(quelle_datei, aus)
    sha = sha256_datei(temp)
    geaendert = False
    if not angabe.get("sha256"):
        if not pinnen:
            temp.unlink()
            _fehler(f"{angabe['datei']} ist nicht gepinnt (SHA-256 {sha}); mit --pinnen eintragen")
        angabe["sha256"] = sha
        geaendert = True
        _melde(f"  gepinnt    {angabe['datei']} {sha}")
    elif sha != angabe["sha256"]:
        temp.unlink()
        _fehler(f"{angabe['datei']}: SHA-256 stimmt nicht (erwartet {angabe['sha256']}, erhalten {sha})")
    os.replace(temp, ziel)
    _melde(f"  ausgepackt {angabe['datei']}")
    return geaendert


def hole_kacheln(manifest: dict[str, Any], speicher: Path, karte: geo.Karte, pinnen: bool) -> bool:
    """Lädt die Höhenkacheln des Manifests. Gibt True zurück, wenn das Manifest geändert wurde."""
    kacheln = manifest["kacheln"]
    zoom = int(kacheln["zoom"])
    soll_x, soll_y = kachelbereich(karte, zoom)
    geaendert = False
    if [kacheln["x"], kacheln["y"]] != [soll_x, soll_y]:
        abgedeckt = (
            kacheln["x"][0] <= soll_x[0]
            and kacheln["x"][1] >= soll_x[1]
            and kacheln["y"][0] <= soll_y[0]
            and kacheln["y"][1] >= soll_y[1]
        )
        if pinnen:
            kacheln["x"], kacheln["y"] = soll_x, soll_y
            geaendert = True
            _melde(f"  Kachelbereich neu festgelegt: Zoom {zoom}, x {soll_x}, y {soll_y}")
        elif not abgedeckt:
            _fehler(
                f"Die gepinnten Höhenkacheln (x {kacheln['x']}, y {kacheln['y']}) decken den Kartenausschnitt "
                f"nicht ab (nötig: x {soll_x}, y {soll_y}); mit --pinnen neu festlegen"
            )
    liste = kacheln_liste(kacheln)
    pins: dict[str, str] = kacheln["sha256"]
    gueltig = {kachel_schluessel(*k) for k in liste}
    for schluessel in [s for s in pins if s not in gueltig]:
        if pinnen:
            del pins[schluessel]
            geaendert = True
    fehlend = []
    for z, x, y in liste:
        schluessel = kachel_schluessel(z, x, y)
        ziel = speicher / kacheln["datei"].format(z=z, x=x, y=y)
        if not pruefe_datei(ziel, pins.get(schluessel, "")):
            fehlend.append((z, x, y))
    _melde(f"  Höhenkacheln: {len(liste)} im Bereich, {len(liste) - len(fehlend)} vorhanden, {len(fehlend)} zu laden")

    def lade(k: tuple[int, int, int]) -> tuple[tuple[int, int, int], str]:
        z, x, y = k
        ziel = speicher / kacheln["datei"].format(z=z, x=x, y=y)
        return k, lade_url(kacheln["url"].format(z=z, x=x, y=y), ziel)

    fehler: list[str] = []
    with ThreadPoolExecutor(max_workers=PARALLEL) as pool:
        for nr, ((z, x, y), sha) in enumerate(pool.map(lade, fehlend), start=1):
            schluessel = kachel_schluessel(z, x, y)
            ziel = speicher / kacheln["datei"].format(z=z, x=x, y=y)
            erwartet = pins.get(schluessel, "")
            if not erwartet:
                if not pinnen:
                    _verwerfen(ziel)
                    fehler.append(f"Kachel {schluessel} ist nicht gepinnt (mit --pinnen eintragen)")
                    continue
                pins[schluessel] = sha
                geaendert = True
            elif sha != erwartet:
                _verwerfen(ziel)
                fehler.append(f"Kachel {schluessel}: SHA-256 stimmt nicht (erwartet {erwartet}, erhalten {sha})")
                continue
            _uebernehmen(ziel)
            if nr % 25 == 0 or nr == len(fehlend):
                _melde(f"  … {nr}/{len(fehlend)} Kacheln")
    if geaendert:
        # Feste Reihenfolge der Pins (x, dann y), damit quellen.json stabil bleibt.
        kacheln["sha256"] = {
            kachel_schluessel(*k): pins[kachel_schluessel(*k)] for k in liste if kachel_schluessel(*k) in pins
        }
    if fehler:
        _fehler("\n".join(fehler))
    return geaendert


def befehl_holen(args: argparse.Namespace) -> int:
    manifest = lade_manifest()
    speicher: Path = args.zwischenspeicher
    gruppen = args.gruppen or None
    bekannte = {q["gruppe"] for q in manifest["quellen"]} | {"hoehen"}
    for gruppe in gruppen or []:
        if gruppe not in bekannte:
            _fehler(f"unbekannte Gruppe '{gruppe}' (bekannt: {', '.join(sorted(bekannte))})")
    _melde(f"Zwischenspeicher: {speicher}")
    geaendert = False
    try:
        for quelle in quellen(manifest, gruppen):
            geaendert |= hole_quelle(quelle, speicher, args.pinnen)
        if manifest.get("kacheln") and (not gruppen or "hoehen" in gruppen):
            geaendert |= hole_kacheln(manifest, speicher, geo.lade_karte(args.karte), args.pinnen)
    finally:
        if geaendert:
            schreibe_manifest(manifest)
            _melde(f"quellen.json aktualisiert: {MANIFEST}")
    _melde("Alle Quellen vorhanden und geprüft.")
    return 0


# ─── Geometrie aus Natural Earth ────────────────────────────────────────────

#: Natural-Earth-Dateien je Ausgabe (Dateiname im Zwischenspeicher, relativ zu natural-earth/v…/).
NE_LAND = ("ne_10m_land.geojson", "ne_10m_minor_islands.geojson")
NE_SEEN = ("ne_10m_lakes.geojson", "ne_10m_lakes_europe.geojson")
NE_FLUESSE = ("ne_10m_rivers_lake_centerlines.geojson", "ne_10m_rivers_europe.geojson")

#: Rang (1 = größter Fluss … 6) aus dem scalerank von Natural Earth (0 = wichtigste Linie).
#: Im Ausschnitt: ≤ 5 Donau, Rhein mit Mündungsarmen, Oder, Elbe, Weichsel; 6–7 Weser, Mosel,
#: Memel, Drau, Mur; 8–9 Main, Inn, Maas, Ems, Warthe, March, Moldau; 10 Neckar, Saale, Spree,
#: Isar, Saar, Netze; 11 Salzach, Enns, Ruhr, Lausitzer Neiße; 12 Lippe, Unstrut, Fulda, Bober.
RANG_GRENZEN: tuple[tuple[float, int], ...] = ((5, 1), (7, 2), (9, 3), (10, 4), (11, 5))
RANG_KLEINSTER = 6

#: Deutsche Namen (übliche Exonyme) für Flüsse, die Natural Earth nur mit fremdem oder ohne
#: deutschen Namen führt. Gilt für den gewählten Namen (name_de, sonst name).
FLUSSNAMEN_DE: dict[str, str] = {
    "Danube": "Donau",
    "Rhine": "Rhein",
    "Rhin": "Rhein",
    "Vistula": "Weichsel",
    "Neman": "Memel",
    "Morava": "March",
    "Warta": "Warthe",
    "Vltava": "Moldau",
    "Adige": "Etsch",
    "Sava": "Save",
    "Tisa": "Theiß",
    "Ticino": "Tessin",
    "Rhône": "Rhone",
    "Rhne": "Rhone",  # Kodierungsfehler in Natural Earth (rivers_lake_centerlines)
    "Sane": "Saône",  # dito
    "Nysa Kłodzka": "Glatzer Neiße",
    "Drwęca": "Drewenz",
    "Gwda": "Küddow",
    "Svitava": "Zwittawa",
    "Svratka": "Schwarzawa",
    "Sázava": "Sasau",
    # Natural Earth führt die Linie Mže/Berounka (Quelle bei Mähring bis zur Mündung in die Moldau
    # bei Prag) als „Úhlava“ (name_alt Berounka); die Úhlava (Angel) ist nur ein Zufluss bei Pilsen.
    "Úhlava": "Beraun",
    "Hron": "Gran",
    "Ipeľ": "Eipel",
    "Hornád": "Kundert",
    "Ljubljanica": "Laibach",
    "Kupa": "Kulpa",
    "Pilica": "Pilitza",
}

#: Deutsche Namen für Seen, deren name_de in Natural Earth unvollständig ist (nur das
#: Bestimmungswort) oder fehlt. Schlüssel: name_de, sonst name der Quelle.
SEENAMEN_DE: dict[str, str] = {
    "Schweriner": "Schweriner See",
    "Dammscher": "Dammscher See",
    "Kummerower": "Kummerower See",
    "Plauer": "Plauer See",
    "Selenter": "Selenter See",
    "Neusiedler": "Neusiedler See",
    "Starnberger": "Starnberger See",
    "Comer": "Comer See",
    "Łebsko": "Lebasee",
    "Jezioro Gardno": "Garder See",
    "Drawsko": "Dratzigsee",
    "Miedwie": "Madüsee",
    "Balaton": "Plattensee",
    "Theiß": "Theiß-See",
    "d’Orient": "Lac d’Orient",
    "d’Annecy": "Lac d’Annecy",
    "du Bourget": "Lac du Bourget",
}


def flussrang(scalerank: Any) -> int:
    """Rang 1–6 eines Flusses aus dem scalerank von Natural Earth."""
    try:
        wert = float(scalerank)
    except (TypeError, ValueError):
        return RANG_KLEINSTER
    for grenze, rang in RANG_GRENZEN:
        if wert <= grenze:
            return rang
    return RANG_KLEINSTER


def _name(eigenschaften: dict[str, Any], uebersetzung: dict[str, str] | None = None) -> str | None:
    """Deutscher Name (name_de), sonst ursprünglicher Name, jeweils über ``uebersetzung``; sonst None."""
    for feld in ("name_de", "name"):
        wert = eigenschaften.get(feld)
        if isinstance(wert, str) and wert.strip():
            wert = wert.strip()
            return (uebersetzung or {}).get(wert, wert)
    return None


@dataclass
class Rahmenbedingungen:
    """Zuschnitt und Vereinfachung, abgeleitet aus karte.chron."""

    rahmen: tuple[float, float, float, float]
    toleranz: float  # in Grad Breite (nach Streckung der x-Achse mit skala_x)
    skala_x: float  # cos(Bezugsbreite)
    pixel_flaeche: float  # Fläche eines Pixels in (Grad Breite)² nach Streckung
    pixel_laenge: float  # Kantenlänge eines Pixels in Grad Breite

    @staticmethod
    def aus_karte(karte: geo.Karte) -> Rahmenbedingungen:
        skala_x = geo.cos_grad(karte.bezugsbreite)
        pixel = karte.grad_je_pixel_y
        return Rahmenbedingungen(
            rahmen=karte.rahmen(RAND_GRAD),
            toleranz=pixel * TOLERANZ_PIXEL,
            skala_x=skala_x,
            pixel_flaeche=(karte.grad_je_pixel_x * skala_x) * pixel,
            pixel_laenge=pixel,
        )


def _ausrichten(ring: geo.Ring, aussen: bool) -> geo.Ring:
    """RFC 7946: äußere Ringe gegen den Uhrzeigersinn, Löcher im Uhrzeigersinn."""
    flaeche = geo.ring_flaeche(ring)
    if (flaeche < 0.0) == aussen:
        return list(reversed(ring))
    return ring


def _runde_ring(ring: geo.Ring) -> geo.Ring:
    """Rundet auf STELLEN Nachkommastellen und entfernt dabei entstehende Doppelpunkte."""
    ergebnis: geo.Ring = []
    for x, y in ring:
        p = (round(x, STELLEN), round(y, STELLEN))
        if not ergebnis or ergebnis[-1] != p:
            ergebnis.append(p)
    return ergebnis


def bearbeite_flaechen(polygone: Sequence[geo.Polygon], rb: Rahmenbedingungen) -> list[geo.Polygon]:
    """Zuschneiden, vereinfachen, runden und kleine Teile entfernen; Ringe nach RFC 7946 ausrichten."""
    min_flaeche = MIN_FLAECHE_PIXEL * rb.pixel_flaeche / rb.skala_x  # in Grad² (ungestreckt)
    ergebnis: list[geo.Polygon] = []
    for polygon in polygone:
        neu: geo.Polygon = []
        for nr, ring in enumerate(polygon):
            geschnitten = geo.schneide_ring(ring, rb.rahmen)
            if not geschnitten:
                if nr == 0:
                    break
                continue
            vereinfacht = geo.vereinfache_ring(geschnitten, rb.toleranz, rb.skala_x)
            gerundet = _runde_ring(vereinfacht)
            if len(gerundet) < 4 or abs(geo.ring_flaeche(gerundet)) < min_flaeche:
                if nr == 0:
                    break
                continue
            neu.append(_ausrichten(gerundet, aussen=nr == 0))
        if neu:
            ergebnis.append(neu)
    return ergebnis


def bearbeite_linien(
    linien: Sequence[geo.Linie], rb: Rahmenbedingungen, behalten: Callable[[Any, Any], Any] | None = None
) -> list[geo.Linie]:
    """Zuschneiden, Teile außerhalb von ``behalten`` entfernen, vereinfachen, runden.

    ``behalten(laengen, breiten)`` (vektorisiert, numpy) entscheidet für verdichtete Punkte im
    Abstand von höchstens einem halben Kartenpixel, ob sie bleiben (z. B. nur auf Land außerhalb
    von Seen). Sehr kurze Stücke entfallen.
    """
    ergebnis: list[geo.Linie] = []
    for linie in linien:
        for stueck in geo.schneide_linie(linie, rb.rahmen):
            teile = [stueck] if behalten is None else schneide_aus(stueck, behalten, rb.pixel_laenge / 2.0, rb.skala_x)
            for teil in teile:
                vereinfacht = geo.vereinfache_linie(teil, rb.toleranz, rb.skala_x)
                gerundet = _runde_ring(vereinfacht)
                if len(gerundet) < 2:
                    continue
                if linienlaenge(gerundet, rb.skala_x) < MIN_FLUSSLAENGE_PIXEL * rb.pixel_laenge:
                    continue
                ergebnis.append(gerundet)
    return ergebnis


def linienlaenge(linie: Sequence[geo.Punkt], skala_x: float = 1.0) -> float:
    """Länge eines Linienzugs in Grad Breite (x-Achse mit ``skala_x`` gestreckt)."""
    laenge = 0.0
    for i in range(1, len(linie)):
        dx = (linie[i][0] - linie[i - 1][0]) * skala_x
        dy = linie[i][1] - linie[i - 1][1]
        laenge += (dx * dx + dy * dy) ** 0.5
    return laenge


def verdichte(linie: Sequence[geo.Punkt], schritt: float, skala_x: float = 1.0) -> geo.Linie:
    """Fügt Zwischenpunkte ein, sodass benachbarte Punkte höchstens ``schritt`` auseinander liegen."""
    punkte: geo.Linie = [(float(linie[0][0]), float(linie[0][1]))]
    for i in range(1, len(linie)):
        x0, y0 = linie[i - 1]
        x1, y1 = linie[i]
        d = (((x1 - x0) * skala_x) ** 2 + (y1 - y0) ** 2) ** 0.5
        n = max(1, int(d / schritt) + 1)
        for k in range(1, n):
            punkte.append((x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n))
        punkte.append((float(x1), float(y1)))
    return punkte


def schneide_aus(
    linie: Sequence[geo.Punkt], behalten: Callable[[Any, Any], Any], schritt: float, skala_x: float = 1.0
) -> list[geo.Linie]:
    """Teilt einen Linienzug in die Stücke, deren (verdichtete) Punkte ``behalten`` erfüllen."""
    import numpy as np  # noqa: PLC0415

    punkte = verdichte(linie, schritt, skala_x)
    a = np.asarray(punkte, dtype=np.float64)
    maske = np.asarray(behalten(a[:, 0], a[:, 1]), dtype=bool)
    stuecke: list[geo.Linie] = []
    aktuell: geo.Linie = []
    for punkt, ok in zip(punkte, maske.tolist()):
        if ok:
            aktuell.append(punkt)
        else:
            if len(aktuell) >= 2:
                stuecke.append(aktuell)
            aktuell = []
    if len(aktuell) >= 2:
        stuecke.append(aktuell)
    return stuecke


def _polygon_geometrie(polygone: Sequence[geo.Polygon]) -> dict[str, Any]:
    if len(polygone) == 1:
        return {"type": "Polygon", "coordinates": polygone[0]}
    return {"type": "MultiPolygon", "coordinates": list(polygone)}


def _linien_geometrie(linien: Sequence[geo.Linie]) -> dict[str, Any]:
    if len(linien) == 1:
        return {"type": "LineString", "coordinates": linien[0]}
    return {"type": "MultiLineString", "coordinates": list(linien)}


def _ne_ordner(manifest: dict[str, Any], speicher: Path) -> dict[str, Path]:
    """Pfade der Natural-Earth-Dateien im Zwischenspeicher (nach Dateiname), geprüft."""
    pfade: dict[str, Path] = {}
    for quelle in quellen(manifest, ["geometrie"]):
        pfad = speicher / quelle.datei
        if not pruefe_datei(pfad, quelle.sha256):
            _fehler(
                f"{quelle.datei} fehlt im Zwischenspeicher oder ist verändert; zuerst 'geodaten.py holen geometrie'"
            )
        pfade[Path(quelle.datei).name] = pfad
    return pfade


def _kopf(manifest: dict[str, Any], dateien: Sequence[str], inhalt: str, karte: geo.Karte) -> dict[str, Any]:
    angaben = {Path(q.datei).name: q for q in quellen(manifest, ["geometrie"])}
    return {
        "name": inhalt,
        "quelle": [angaben[d].quellenangabe for d in dateien],
        "lizenz": "gemeinfrei (Natural Earth)",
        "erzeugt": "tools/geodaten/geodaten.py geometrie – nicht von Hand bearbeiten",
        "ausschnitt": [round(v, STELLEN) for v in karte.rahmen(RAND_GRAD)],
        "vereinfachung_pixel": TOLERANZ_PIXEL,
    }


def _ohne_doppel(polygone: Sequence[geo.Polygon], bekannt: set[frozenset[geo.Punkt]]) -> list[geo.Polygon]:
    """Lässt Polygone weg, deren äußerer Ring (als Punktmenge) schon vorkam; merkt sich die neuen."""
    ergebnis = []
    for polygon in polygone:
        schluessel = frozenset(polygon[0])
        if schluessel in bekannt:
            continue
        bekannt.add(schluessel)
        ergebnis.append(polygon)
    return ergebnis


def ueberlappende_pixel(polygone: Sequence[geo.Polygon], karte: geo.Karte) -> int:
    """Zahl der Kartenpixel, die mehr als ein Polygon abdecken (Rasterregel aus Abschnitt 6.2)."""
    import numpy as np  # noqa: PLC0415

    zaehler = np.zeros((karte.bild_hoehe, karte.bild_breite), dtype=np.uint8)
    for polygon in polygone:
        maske = geo.rastere_polygone([polygon], karte)
        zaehler += maske.astype(np.uint8)
    return int(np.count_nonzero(zaehler > 1))


def _pruefe_karte(karte: geo.Karte) -> None:
    fehler = karte.pruefe()
    if fehler:
        _fehler("karte.chron: " + "; ".join(fehler))


def erzeuge_geometrie(manifest: dict[str, Any], speicher: Path, karte: geo.Karte) -> dict[str, str]:
    """Erzeugt die Texte von land.geojson, seen.geojson und fluesse.geojson (deterministisch)."""
    import numpy as np  # noqa: PLC0415

    _pruefe_karte(karte)
    pfade = _ne_ordner(manifest, speicher)
    rb = Rahmenbedingungen.aus_karte(karte)
    ausgaben: dict[str, str] = {}

    # Land: ein Feature je Polygon, in Quellreihenfolge. minor_islands wiederholt in Version 5.1.2
    # viele Polygone aus land wörtlich; solche Doppel entfallen (sie würden sich bei einer
    # Gerade-ungerade-Auswertung über alle Polygone gegenseitig aufheben).
    land: list[geo.Polygon] = []
    features: list[dict[str, Any]] = []
    bekannt: set[frozenset[geo.Punkt]] = set()
    for dateiname in NE_LAND:
        art = "insel" if "islands" in dateiname else "land"
        for feature in geo.lade_geojson(pfade[dateiname])["features"]:
            quelle = _ohne_doppel(geo.geometrie_polygone(feature.get("geometry")), bekannt)
            for polygon in bearbeite_flaechen(quelle, rb):
                land.append(polygon)
                features.append({"properties": {"art": art}, "geometry": _polygon_geometrie([polygon])})
    ausgaben["land.geojson"] = geo.feature_collection_text(
        features, STELLEN, _kopf(manifest, NE_LAND, "Land (Festland und Inseln)", karte)
    )

    # Seen: ein Feature je See (MultiPolygon, wenn der See aus mehreren Teilen besteht).
    seen: list[geo.Polygon] = []
    features = []
    bekannt = set()
    for dateiname in NE_SEEN:
        for feature in geo.lade_geojson(pfade[dateiname])["features"]:
            polygone = bearbeite_flaechen(_ohne_doppel(geo.geometrie_polygone(feature.get("geometry")), bekannt), rb)
            if not polygone:
                continue
            seen.extend(polygone)
            eigenschaften: dict[str, Any] = {}
            name = _name(feature.get("properties") or {}, SEENAMEN_DE)
            if name:
                eigenschaften["name"] = name
            features.append({"properties": eigenschaften, "geometry": _polygon_geometrie(polygone)})
    ausgaben["seen.geojson"] = geo.feature_collection_text(
        features, STELLEN, _kopf(manifest, NE_SEEN, "Binnengewässer", karte)
    )

    # Flüsse: nur auf Land und außerhalb der Seen (Rasterregel auf dem Kartenraster, damit
    # Mittellinien durch Seen dort entfallen, wo der See auf der Karte erscheint, und Lücken
    # dort geschlossen bleiben, wo er fehlt); außerhalb des Bildes bleibt alles erhalten.
    maske_land = geo.Rastermaske(land, karte)
    maske_seen = geo.Rastermaske(seen, karte)

    def behalten(laengen: Any, breiten: Any) -> Any:
        x, y = karte.nach_pixel_viele(laengen, breiten)
        im_bild = (x >= 0.0) & (x < karte.bild_breite) & (y >= 0.0) & (y < karte.bild_hoehe)
        wasserfrei = maske_land.enthaelt_viele(laengen, breiten) & ~maske_seen.enthaelt_viele(laengen, breiten)
        return np.where(im_bild, wasserfrei, True)

    fluesse: list[tuple[int, int, dict[str, Any]]] = []
    for dateiname in NE_FLUESSE:
        for feature in geo.lade_geojson(pfade[dateiname])["features"]:
            eigenschaften = feature.get("properties") or {}
            linien = bearbeite_linien(geo.geometrie_linien(feature.get("geometry")), rb, behalten)
            if not linien:
                continue
            rang = flussrang(eigenschaften.get("scalerank"))
            neu: dict[str, Any] = {"rang": rang}
            name = _name(eigenschaften, FLUSSNAMEN_DE)
            if name:
                neu["name"] = name
            fluesse.append((rang, len(fluesse), {"properties": neu, "geometry": _linien_geometrie(linien)}))
    fluesse.sort(key=lambda e: (e[0], e[1]))
    ausgaben["fluesse.geojson"] = geo.feature_collection_text(
        [f for _, _, f in fluesse], STELLEN, _kopf(manifest, NE_FLUESSE, "Flüsse (rang 1 = größter Fluss … 6)", karte)
    )
    return ausgaben


# ─── Kernraum (von Hand gepflegt) ───────────────────────────────────────────


def _strecken_schneiden(p1: geo.Punkt, p2: geo.Punkt, p3: geo.Punkt, p4: geo.Punkt) -> bool:
    """Schneiden sich die Strecken p1–p2 und p3–p4 echt (nicht nur in einem Endpunkt)?"""

    def ccw(a: geo.Punkt, b: geo.Punkt, c: geo.Punkt) -> float:
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])

    d1, d2 = ccw(p3, p4, p1), ccw(p3, p4, p2)
    d3, d4 = ccw(p1, p2, p3), ccw(p1, p2, p4)
    return d1 * d2 < 0.0 and d3 * d4 < 0.0


def ring_ueberschneidungen(ring: Sequence[geo.Punkt]) -> list[tuple[int, int]]:
    """Paare nicht benachbarter Kanten (Index des Anfangspunkts), die sich schneiden."""
    punkte = list(ring)
    if punkte and punkte[0] == punkte[-1]:
        punkte.pop()
    n = len(punkte)
    paare = []
    for i in range(n):
        a, b = punkte[i], punkte[(i + 1) % n]
        for j in range(i + 2, n):
            if i == 0 and j == n - 1:
                continue
            if _strecken_schneiden(a, b, punkte[j], punkte[(j + 1) % n]):
                paare.append((i, j))
    return paare


def pruefe_kernraum(pfad: Path, karte: geo.Karte | None = None) -> list[str]:
    """Prüft den Aufbau von kernraum.geojson; Ergebnis: Fehlermeldungen (leer = in Ordnung).

    Regeln: FeatureCollection mit genau einem Feature (Polygon oder MultiPolygon), geschlossene
    Ringe mit mindestens drei Punkten, äußere Ringe gegen den Uhrzeigersinn (RFC 7946), keine
    Selbstüberschneidungen; mit ``karte`` zusätzlich: alle Punkte im Kartenausschnitt.
    """
    try:
        daten = geo.lade_geojson(pfad)
    except (OSError, ValueError) as fehler:
        return [f"{pfad}: {fehler}"]
    if daten.get("type") != "FeatureCollection" or len(daten.get("features") or []) != 1:
        return [f"{pfad}: erwartet eine FeatureCollection mit genau einem Feature"]
    geometrie = daten["features"][0].get("geometry") or {}
    if geometrie.get("type") not in ("Polygon", "MultiPolygon"):
        return [f"{pfad}: Geometrie muss Polygon oder MultiPolygon sein"]
    teile = [geometrie["coordinates"]] if geometrie["type"] == "Polygon" else geometrie["coordinates"]
    fehler: list[str] = []
    for nr, polygon in enumerate(teile, start=1):
        for rnr, ring in enumerate(polygon):
            name = f"Polygon {nr}, {'äußerer Ring' if rnr == 0 else f'Loch {rnr}'}"
            punkte = [(float(p[0]), float(p[1])) for p in ring]
            if len(punkte) < 4 or punkte[0] != punkte[-1]:
                fehler.append(f"{pfad}: {name}: Ring nicht geschlossen oder weniger als drei Punkte")
                continue
            flaeche = geo.ring_flaeche(punkte)
            if (flaeche > 0.0) != (rnr == 0):
                fehler.append(
                    f"{pfad}: {name}: falsche Umlaufrichtung (RFC 7946: außen gegen, Löcher im Uhrzeigersinn)"
                )
            for i, j in ring_ueberschneidungen(punkte):
                fehler.append(f"{pfad}: {name}: Kante ab Punkt {i} schneidet Kante ab Punkt {j}")
            if karte is not None:
                for k, (x, y) in enumerate(punkte):
                    if not (karte.laenge_min < x < karte.laenge_max and karte.breite_min < y < karte.breite_max):
                        fehler.append(f"{pfad}: {name}: Punkt {k} ({x} {y}) liegt außerhalb des Kartenausschnitts")
    return fehler


def befehl_geometrie(args: argparse.Namespace) -> int:
    manifest = lade_manifest()
    karte = geo.lade_karte(args.karte)
    beginn = time.perf_counter()
    ausgaben = erzeuge_geometrie(manifest, args.zwischenspeicher, karte)
    ordner: Path = args.ausgabe
    ordner.mkdir(parents=True, exist_ok=True)
    gesamt = 0
    for name, text in ausgaben.items():
        daten = text.encode("utf-8")
        gesamt += len(daten)
        pfad = ordner / name
        if pfad.is_file() and pfad.read_bytes() == daten:
            _melde(f"  unverändert {name} ({len(daten) / 1e6:.2f} MB)")
            continue
        pfad.write_bytes(daten)
        _melde(f"  geschrieben {name} ({len(daten) / 1e6:.2f} MB)")
    _melde(f"Geometrie: {gesamt / 1e6:.2f} MB in {time.perf_counter() - beginn:.1f} s")
    for name in ("land.geojson", "seen.geojson"):
        anzahl = ueberlappende_pixel(geo.lade_flaechen(ordner / name), karte)
        if anzahl:
            _warnung(f"{name}: {anzahl} Kartenpixel liegen in mehr als einem Polygon")
    if gesamt > MAX_GROESSE:
        _warnung(f"Geometriedateien zusammen {gesamt / 1e6:.2f} MB (Ziel: unter {MAX_GROESSE / 1e6:.0f} MB)")
    return 0


def befehl_pruefen(args: argparse.Namespace) -> int:
    manifest = lade_manifest()
    karte = geo.lade_karte(args.karte)
    ausgaben = erzeuge_geometrie(manifest, args.zwischenspeicher, karte)
    probleme = []
    for name, text in ausgaben.items():
        pfad: Path = args.ausgabe / name
        if not pfad.is_file() or pfad.read_bytes() != text.encode("utf-8"):
            probleme.append(f"{name} ist nicht aktuell – 'python3 tools/geodaten/geodaten.py geometrie' ausführen")
    kernraum = args.ausgabe / "kernraum.geojson"
    probleme.extend(pruefe_kernraum(kernraum, karte))
    for name in ("land.geojson", "seen.geojson"):
        if (args.ausgabe / name).is_file():
            anzahl = ueberlappende_pixel(geo.lade_flaechen(args.ausgabe / name), karte)
            if anzahl:
                probleme.append(f"{name}: {anzahl} Kartenpixel liegen in mehr als einem Polygon")
    if probleme:
        for text in probleme:
            print(f"Fehler: {text}", file=sys.stderr)
        return 1
    _melde(
        "Geometriedateien sind aktuell (passen zu den Quellen im Zwischenspeicher und zu karte.chron); kernraum.geojson ist gültig."
    )
    return 0


# ─── Vorschau ───────────────────────────────────────────────────────────────

#: Orte zur Sichtprüfung des Kernraums: (Name, Länge, Breite, soll im Kernraum liegen).
PRUEFORTE: tuple[tuple[str, float, float, bool], ...] = (
    ("Berlin", 13.40, 52.52, True),
    ("Wien", 16.37, 48.21, True),
    ("Prag", 14.42, 50.09, True),
    ("Breslau", 17.04, 51.11, True),
    ("Königsberg", 20.51, 54.71, True),
    ("Memel", 21.13, 55.71, True),
    ("Tilsit", 21.88, 55.08, True),
    ("Danzig", 18.65, 54.35, True),
    ("Thorn", 18.60, 53.01, True),
    ("Posen", 16.93, 52.41, True),
    ("Kattowitz", 19.02, 50.26, True),
    ("Teschen", 18.63, 49.75, True),
    ("Brünn", 16.61, 49.20, True),
    ("Straßburg", 7.75, 48.58, True),
    ("Metz", 6.18, 49.12, True),
    ("Nancy", 6.18, 48.69, True),
    ("Luxemburg", 6.13, 49.61, True),
    ("Köln", 6.96, 50.94, True),
    ("Aachen", 6.08, 50.78, True),
    ("Lüttich", 5.57, 50.63, True),
    ("Brüssel", 4.35, 50.85, True),
    ("Antwerpen", 4.40, 51.22, True),
    ("Gent", 3.72, 51.05, True),
    ("Brügge", 3.22, 51.21, True),
    ("Lille", 3.06, 50.63, True),
    ("Maastricht", 5.69, 50.85, True),
    ("Groningen", 6.57, 53.22, True),
    ("Flensburg", 9.44, 54.79, True),
    ("Hadersleben", 9.49, 55.25, True),
    ("Hamburg", 9.99, 53.55, True),
    ("Stralsund", 13.09, 54.31, True),
    ("Innsbruck", 11.39, 47.27, True),
    ("Bozen", 11.35, 46.50, True),
    ("Salzburg", 13.04, 47.80, True),
    ("Graz", 15.44, 47.07, True),
    ("Klagenfurt", 14.31, 46.62, True),
    ("Laibach", 14.51, 46.05, True),
    ("Marburg an der Drau", 15.65, 46.56, True),
    ("Eisenstadt", 16.52, 47.85, True),
    ("Bregenz", 9.75, 47.50, True),
    ("Vaduz", 9.52, 47.14, True),
    ("Konstanz", 9.18, 47.66, True),
    ("Basel", 7.59, 47.56, False),
    ("Zürich", 8.54, 47.37, False),
    ("Schaffhausen", 8.63, 47.70, False),
    ("Trient", 11.12, 46.07, False),
    ("Triest", 13.77, 45.65, False),
    ("Görz", 13.62, 45.94, False),
    ("Venedig", 12.33, 45.44, False),
    ("Pressburg", 17.11, 48.15, False),
    ("Ödenburg", 16.59, 47.68, False),
    ("Krakau", 19.94, 50.06, False),
    ("Warschau", 21.01, 52.23, False),
    ("Amsterdam", 4.90, 52.37, False),
    ("Rotterdam", 4.48, 51.92, False),
    ("Kopenhagen", 12.57, 55.68, False),
    ("Malmö", 13.00, 55.61, False),
    ("Rønne (Bornholm)", 14.70, 55.10, False),
    ("Nakskov (Lolland)", 11.13, 54.83, False),
    ("Paris", 2.35, 48.86, False),
    ("Reims", 4.03, 49.26, False),
    ("Dijon", 5.04, 47.32, False),
    ("Kaunas", 23.90, 54.90, False),
    ("Wilna", 25.28, 54.69, False),
    ("Riga", 24.11, 56.95, False),
    # Säume und Grenzentscheidungen (README, Abschnitt „Entscheidungen“)
    ("Kolding", 9.47, 55.49, True),
    ("Esbjerg", 8.45, 55.47, True),
    ("Fredericia", 9.75, 55.57, False),
    ("Odense", 10.39, 55.40, False),
    ("Heydekrug", 21.48, 55.35, True),
    ("Tauroggen", 22.29, 55.25, False),
    ("Krottingen", 21.24, 55.89, False),
    ("Suwałki", 22.93, 54.10, True),
    ("Kalisch", 18.09, 51.76, True),
    ("Tschenstochau", 19.12, 50.81, True),
    ("Lodz", 19.46, 51.76, False),
    ("Douai", 3.08, 50.37, True),
    ("Cambrai", 3.24, 50.18, True),
    ("Arras", 2.78, 50.29, False),
    ("Sedan", 4.94, 49.70, True),
    ("Besançon", 6.02, 47.24, False),
    ("Mömpelgard", 6.80, 47.51, True),
    ("Belfort", 6.86, 47.64, True),
    ("Arnheim", 5.91, 51.98, True),
    ("Nimwegen", 5.86, 51.84, True),
    ("Venlo", 6.17, 51.37, True),
    ("Utrecht", 5.12, 52.09, False),
    ("Eindhoven", 5.48, 51.44, False),
    ("Meran", 11.16, 46.67, True),
    ("Cortina d'Ampezzo", 12.14, 46.54, False),
    ("Cilli", 15.26, 46.23, True),
    ("Agram", 15.98, 45.81, False),
    ("Fiume", 14.44, 45.33, False),
)

#: Farben der Vorschau (RGB).
_FARBEN = {
    "meer": (162, 196, 222),
    "ausland": (214, 210, 196),
    "kernraum": (241, 226, 178),
    "see": (120, 170, 210),
    "fluss": (52, 110, 180),
    "grenze": (120, 120, 120),
    "umriss": (178, 34, 34),
    "drin": (0, 120, 0),
    "draussen": (200, 0, 0),
}


def _zeichne_linien(
    np: Any, bild: Any, linien: Sequence[geo.Linie], karte: geo.Karte, farbe: tuple[int, int, int]
) -> None:
    """Zeichnet Linienzüge (ein Pixel breit) in ein RGB-Bild in der Projektion von ``karte``."""
    hoehe, breite = bild.shape[:2]
    sx = breite / (karte.laenge_max - karte.laenge_min)
    sy = hoehe / (karte.breite_max - karte.breite_min)
    for linie in linien:
        a = np.asarray(linie, dtype=np.float64)
        x = (a[:, 0] - karte.laenge_min) * sx
        y = (karte.breite_max - a[:, 1]) * sy
        for i in range(1, len(a)):
            schritte = int(max(abs(x[i] - x[i - 1]), abs(y[i] - y[i - 1])) * 2) + 1
            t = np.linspace(0.0, 1.0, schritte + 1)
            px = np.floor(x[i - 1] + t * (x[i] - x[i - 1])).astype(np.int64)
            py = np.floor(y[i - 1] + t * (y[i] - y[i - 1])).astype(np.int64)
            ok = (px >= 0) & (px < breite) & (py >= 0) & (py < hoehe)
            bild[py[ok], px[ok]] = farbe


def schreibe_png(pfad: Path, bild: Any) -> None:
    """Schreibt ein RGB-Bild (numpy uint8, Höhe × Breite × 3) als PNG (Filter 0, zlib)."""
    hoehe, breite = bild.shape[:2]
    roh = b"".join(b"\x00" + bild[z].tobytes() for z in range(hoehe))

    def block(art: bytes, daten: bytes) -> bytes:
        return len(daten).to_bytes(4, "big") + art + daten + (zlib.crc32(art + daten) & 0xFFFFFFFF).to_bytes(4, "big")

    kopf = breite.to_bytes(4, "big") + hoehe.to_bytes(4, "big") + bytes([8, 2, 0, 0, 0])
    pfad.parent.mkdir(parents=True, exist_ok=True)
    pfad.write_bytes(
        b"\x89PNG\r\n\x1a\n" + block(b"IHDR", kopf) + block(b"IDAT", zlib.compress(roh, 6)) + block(b"IEND", b"")
    )


def vorschau_karte(karte: geo.Karte, breite: int, ausschnitt: Sequence[float] | None = None) -> geo.Karte:
    """Karte für die Vorschau: ganzer Ausschnitt oder Teilausschnitt (lmin bmin lmax bmax), gleiche Projektion."""
    if ausschnitt is not None:
        lmin, bmin, lmax, bmax = (float(v) for v in ausschnitt)
        if not (lmin < lmax and bmin < bmax):
            _fehler("--ausschnitt erwartet LÄNGE_MIN BREITE_MIN LÄNGE_MAX BREITE_MAX mit min < max")
        karte = replace(karte, laenge_min=lmin, laenge_max=lmax, breite_min=bmin, breite_max=bmax)
    hoehe = max(
        1,
        round(
            breite
            * (karte.breite_max - karte.breite_min)
            / ((karte.laenge_max - karte.laenge_min) * geo.cos_grad(karte.bezugsbreite))
        ),
    )
    return replace(karte, bild_breite=breite, bild_hoehe=hoehe)


def befehl_vorschau(args: argparse.Namespace) -> int:
    try:
        import numpy as np  # noqa: PLC0415
    except ImportError:
        _fehler("Die Vorschau benötigt numpy (python3 -m pip install numpy)")
    karte = vorschau_karte(geo.lade_karte(args.karte), int(args.breite), args.ausschnitt)
    breite, hoehe = karte.bild_breite, karte.bild_hoehe
    ordner: Path = args.geometrie
    land = geo.lade_flaechen(ordner / "land.geojson")
    seen = geo.lade_flaechen(ordner / "seen.geojson") if (ordner / "seen.geojson").is_file() else []
    kernraum_pfad = args.kernraum or ordner / "kernraum.geojson"
    kernraum = geo.lade_flaechen(kernraum_pfad) if kernraum_pfad.is_file() else []
    if not kernraum:
        _warnung(f"{kernraum_pfad} fehlt oder ist leer – Vorschau ohne Kernraum")
    maske_land = geo.rastere_polygone(land, karte)
    maske_see = geo.rastere_polygone(seen, karte) & maske_land
    maske_kern = geo.rastere_polygone(kernraum, karte)
    bild = np.zeros((hoehe, breite, 3), dtype=np.uint8)
    bild[:] = _FARBEN["meer"]
    bild[maske_land] = _FARBEN["ausland"]
    bild[maske_land & maske_kern] = _FARBEN["kernraum"]
    bild[maske_see] = _FARBEN["see"]
    if (ordner / "fluesse.geojson").is_file():
        fluesse = geo.lade_linien(ordner / "fluesse.geojson", lambda p: int(p.get("rang", 6)) <= args.max_rang)
        _zeichne_linien(np, bild, fluesse, karte, _FARBEN["fluss"])
    manifest = lade_manifest()
    for quelle in quellen(manifest, ["referenz"]):
        pfad = args.zwischenspeicher / quelle.datei
        if pfad.is_file():
            _zeichne_linien(np, bild, geo.lade_linien(pfad), karte, _FARBEN["grenze"])
    umriss = [ring for polygon in kernraum for ring in polygon]
    _zeichne_linien(np, bild, umriss, karte, _FARBEN["umriss"])
    falsch = 0
    for name, laenge, breite_grad, soll in PRUEFORTE:
        ist = geo.punkt_in_polygon(laenge, breite_grad, kernraum)
        if ist != soll:
            falsch += 1
            _warnung(
                f"Prüfort {name} liegt {'im' if ist else 'außerhalb des'} Kernraum{'' if ist else 's'} (erwartet: umgekehrt)"
            )
        x, y = karte.nach_pixel(laenge, breite_grad)
        px, py = int(x), int(y)
        if 0 <= px < breite and 0 <= py < hoehe:
            bild[max(0, py - 2) : py + 3, max(0, px - 2) : px + 3] = _FARBEN["drin"] if ist else _FARBEN["draussen"]
    schreibe_png(args.ausgabe, bild)
    punkte = sum(len(r) - 1 for r in umriss)
    _melde(
        f"Vorschau {breite}×{hoehe} geschrieben: {args.ausgabe} (Kernraum: {len(kernraum)} Polygon(e), {punkte} Punkte; "
        f"Prüforte: {len(PRUEFORTE) - falsch} von {len(PRUEFORTE)} wie erwartet)"
    )
    return 0


# ─── Programm ───────────────────────────────────────────────────────────────


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="geodaten.py",
        description="Geodaten für EINIGKEIT: Quellen holen, Kartengeometrie erzeugen und prüfen.",
    )
    parser.add_argument(
        "--zwischenspeicher",
        type=Path,
        default=ZWISCHENSPEICHER,
        help="Ordner für Rohdaten (Standard: build/geodaten-cache)",
    )
    parser.add_argument("--karte", type=Path, default=KARTE, help="karte.chron (Standard: data/karte/karte.chron)")
    unter = parser.add_subparsers(dest="befehl", required=True)

    p_holen = unter.add_parser("holen", help="Quellen laden und Prüfsummen prüfen")
    p_holen.add_argument("gruppen", nargs="*", help="nur diese Gruppen (geometrie, referenz, orte, hoehen)")
    p_holen.add_argument(
        "--pinnen", action="store_true", help="fehlende Prüfwerte und den Kachelbereich in quellen.json eintragen"
    )
    p_holen.set_defaults(funktion=befehl_holen)

    p_geo = unter.add_parser("geometrie", help="land/seen/fluesse.geojson aus Natural Earth erzeugen")
    p_geo.add_argument(
        "--ausgabe", type=Path, default=GEOMETRIE_ORDNER, help="Zielordner (Standard: data/karte/geometrie)"
    )
    p_geo.set_defaults(funktion=befehl_geometrie)

    p_pruefen = unter.add_parser("pruefen", help="passen die Geometriedateien zu den Quellen? (nur lokal)")
    p_pruefen.add_argument("--ausgabe", type=Path, default=GEOMETRIE_ORDNER, help="Ordner der Geometriedateien")
    p_pruefen.set_defaults(funktion=befehl_pruefen)

    p_vorschau = unter.add_parser("vorschau", help="Land, Seen, Flüsse und Kernraum als PNG zur Sichtprüfung")
    p_vorschau.add_argument(
        "--ausgabe",
        type=Path,
        default=Path(tempfile.gettempdir()) / "einigkeit-geo" / "vorschau.png",
        help="PNG-Datei (Standard: <Temp-Ordner>/einigkeit-geo/vorschau.png)",
    )
    p_vorschau.add_argument("--breite", type=int, default=2048, help="Bildbreite in Pixeln (Standard 2048)")
    p_vorschau.add_argument("--geometrie", type=Path, default=GEOMETRIE_ORDNER, help="Ordner mit den GeoJSON-Dateien")
    p_vorschau.add_argument(
        "--kernraum", type=Path, default=None, help="Umriss des Kernraums (Standard: <geometrie>/kernraum.geojson)"
    )
    p_vorschau.add_argument("--max-rang", type=int, default=4, help="Flüsse bis zu diesem Rang zeichnen (Standard 4)")
    p_vorschau.add_argument(
        "--ausschnitt",
        type=float,
        nargs=4,
        metavar=("LÄNGE_MIN", "BREITE_MIN", "LÄNGE_MAX", "BREITE_MAX"),
        help="nur diesen Teil der Karte zeigen (Grad), z. B. 9 45.3 16.8 47.9 für die Ostalpen",
    )
    p_vorschau.set_defaults(funktion=befehl_vorschau)

    args = parser.parse_args(argv)
    try:
        return int(args.funktion(args))
    except GeodatenFehler as fehler:
        print(f"Fehler: {fehler}", file=sys.stderr)
        return 2
    except (ValueError, OSError) as fehler:
        print(f"Fehler: {fehler}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
