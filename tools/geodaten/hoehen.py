#!/usr/bin/env python3
"""Höhen und Lagemerkmale für Orte: Terrarium-Höhenkacheln, Abstand zu Küste und Flüssen.

Bausteine (für den Ort-Generator und andere Werkzeuge):

- ``dekodiere_png`` / ``dekodiere_pngs``: PNG-Dekoder nur mit zlib und numpy (8 Bit je Kanal,
  Graustufen/RGB/RGBA, ohne Zeilensprung) – mehr brauchen die Terrarium-Kacheln nicht.
- ``terrarium_hoehen``: Höhe in Metern aus Terrarium-RGB, ``(R · 256 + G + B / 256) − 32768``.
- ``Hoehenmodell``: Mosaik der gepinnten Kacheln aus ``build/geodaten-cache/terrarium/``
  (Web Mercator) mit ``hoehe(länge, breite)`` (bilinear) und ``statistik(länge, breite, radius_km)``
  (Mittel, Minimum, Maximum, Relief, Streuung der Pixel im Umkreis).
- ``Gewaesserabstand``: Abstand zur Meeresküste (aus ``land.geojson``) und zu Flüssen bis zu
  einem Rang (aus ``fluesse.geojson``) in km.
- ``Lageauskunft``: alles zusammen für einen Punkt (``merkmale``).

Entfernungen in km nach docs/technik/chronik-skript.md, Abschnitt 6.1 (111,32 km je Grad, Länge mit
cos der Breite). Die Höhen dienen der Einordnung von Gelände (z. B. Mittelgebirge, Hochgebirge,
Marsch) und gehen nicht pixelgenau in die Karte ein; Winkelfunktionen aus numpy sind hier erlaubt.

Aufruf zur Probe::

    python3 tools/geodaten/hoehen.py 13.40 52.52 --radius 5

Voraussetzung: ``python3 tools/geodaten/geodaten.py holen hoehen`` (Kacheln im Zwischenspeicher)
und ``geodaten.py geometrie`` (für Küste und Flüsse). Benötigt numpy.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
import sys
import zlib
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np

try:
    import geo
    import geodaten
except ImportError:  # Aufruf als Modul von außerhalb des Werkzeugordners
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import geo
    import geodaten

__all__ = [
    "KACHELGROESSE",
    "Gewaesserabstand",
    "Hoehenmodell",
    "HoehenStatistik",
    "Lageauskunft",
    "Lagemerkmale",
    "dekodiere_png",
    "dekodiere_pngs",
    "mercator_geo",
    "mercator_pixel",
    "terrarium_hoehen",
]

#: Kantenlänge einer Kachel in Pixeln.
KACHELGROESSE = 256
#: Kilometer je Grad (Abschnitt 6.1).
KM_PRO_GRAD = geo.KM_PRO_GRAD
#: Standardrang für „große Flüsse“ (1 = größter Fluss … 6), siehe fluesse.geojson.
GROSSE_FLUESSE_RANG = 3

_PNG_SIGNATUR = b"\x89PNG\r\n\x1a\n"
#: Bytes je Pixel nach PNG-Farbtyp (nur 8 Bit je Kanal).
_KANAELE = {0: 1, 2: 3, 4: 2, 6: 4}


class HoehenFehler(geodaten.GeodatenFehler):
    """Fehler beim Lesen der Höhendaten (deutsche Meldung)."""


# ─── PNG ────────────────────────────────────────────────────────────────────


@dataclass(frozen=True)
class _PngRoh:
    breite: int
    hoehe: int
    kanaele: int
    gefiltert: bytes  # entpackte IDAT-Daten: je Zeile ein Filterbyte und breite · kanaele Bytes


def _lies_png(daten: bytes) -> _PngRoh:
    """Zerlegt eine PNG-Datei in Kopf und entpackte, noch gefilterte Bilddaten."""
    if not daten.startswith(_PNG_SIGNATUR):
        raise HoehenFehler("keine PNG-Datei (Signatur fehlt)")
    pos = len(_PNG_SIGNATUR)
    kopf: tuple[int, ...] | None = None
    idat: list[bytes] = []
    while pos + 8 <= len(daten):
        laenge = struct.unpack(">I", daten[pos : pos + 4])[0]
        art = daten[pos + 4 : pos + 8]
        inhalt = daten[pos + 8 : pos + 8 + laenge]
        if len(inhalt) != laenge or pos + 12 + laenge > len(daten):
            raise HoehenFehler("PNG-Datei ist abgeschnitten")
        pruefsumme = struct.unpack(">I", daten[pos + 8 + laenge : pos + 12 + laenge])[0]
        if zlib.crc32(art + inhalt) & 0xFFFFFFFF != pruefsumme:
            raise HoehenFehler(f"PNG-Block {art.decode('latin-1')}: Prüfsumme falsch")
        if art == b"IHDR":
            kopf = struct.unpack(">IIBBBBB", inhalt)
        elif art == b"IDAT":
            idat.append(inhalt)
        elif art == b"IEND":
            break
        pos += 12 + laenge
    if kopf is None:
        raise HoehenFehler("PNG ohne IHDR-Block")
    breite, hoehe, tiefe, farbtyp, kompression, filterart, zeilensprung = kopf
    if tiefe != 8 or farbtyp not in _KANAELE:
        raise HoehenFehler(
            f"PNG-Format nicht unterstützt (Bittiefe {tiefe}, Farbtyp {farbtyp}); erwartet 8 Bit Grau/RGB/RGBA"
        )
    if kompression != 0 or filterart != 0 or zeilensprung != 0:
        raise HoehenFehler("PNG mit Zeilensprung oder unbekannter Kompression wird nicht unterstützt")
    try:
        roh = zlib.decompress(b"".join(idat))
    except zlib.error as fehler:
        raise HoehenFehler(f"PNG-Bilddaten beschädigt: {fehler}") from fehler
    kanaele = _KANAELE[farbtyp]
    if len(roh) != hoehe * (1 + breite * kanaele):
        raise HoehenFehler("PNG-Bilddaten haben die falsche Länge")
    return _PngRoh(breite, hoehe, kanaele, roh)


def _entfiltern(gefiltert: Any, bpp: int) -> Any:
    """Macht die PNG-Zeilenfilter rückgängig, für mehrere gleich große Bilder zugleich.

    ``gefiltert``: uint8-Array (Bilder, Zeilen, 1 + Zeilenbytes). Ergebnis: uint8 (Bilder,
    Zeilen, Zeilenbytes). Sub und Up sind vollständig vektorisiert; Average und Paeth hängen vom
    linken Nachbarn ab und laufen pixelweise, aber über alle Bilder gleichzeitig.
    """
    anzahl, zeilen, _ = gefiltert.shape
    typen = gefiltert[:, :, 0]
    if int(typen.max(initial=0)) > 4:
        raise HoehenFehler(f"PNG: unbekannter Zeilenfilter {int(typen.max())}")
    laenge = gefiltert.shape[2] - 1
    aus = np.empty((anzahl, zeilen, laenge), dtype=np.uint8)
    vorher = np.zeros((anzahl, laenge), dtype=np.int32)
    for z in range(zeilen):
        zeile = gefiltert[:, z, 1:].astype(np.int32)
        neu = np.empty((anzahl, laenge), dtype=np.int32)
        typ_zeile = typen[:, z]
        for typ in np.unique(typ_zeile).tolist():
            idx = np.nonzero(typ_zeile == typ)[0]
            roh = zeile[idx]
            oben = vorher[idx]
            if typ == 0:
                ergebnis = roh
            elif typ == 1:
                ergebnis = roh.reshape(len(idx), laenge // bpp, bpp).cumsum(axis=1).reshape(len(idx), laenge) & 255
            elif typ == 2:
                ergebnis = (roh + oben) & 255
            else:
                ergebnis = np.empty_like(roh)
                links = np.zeros((len(idx), bpp), dtype=np.int32)
                obenlinks = np.zeros((len(idx), bpp), dtype=np.int32)
                for x in range(0, laenge, bpp):
                    b = oben[:, x : x + bpp]
                    if typ == 3:
                        vorhersage = (links + b) >> 1
                    else:
                        p = links + b - obenlinks
                        pa, pb, pc = np.abs(p - links), np.abs(p - b), np.abs(p - obenlinks)
                        vorhersage = np.where((pa <= pb) & (pa <= pc), links, np.where(pb <= pc, b, obenlinks))
                    links = (roh[:, x : x + bpp] + vorhersage) & 255
                    ergebnis[:, x : x + bpp] = links
                    obenlinks = b
            neu[idx] = ergebnis
        aus[:, z, :] = neu
        vorher = neu
    return aus


def dekodiere_pngs(dateien: Sequence[bytes]) -> list[Any]:
    """Dekodiert mehrere PNG-Dateien; Ergebnis je Datei: uint8-Array (Höhe, Breite, Kanäle).

    Gleich große Bilder werden gemeinsam entfiltert (deutlich schneller als einzeln).
    """
    rohe = [_lies_png(d) for d in dateien]
    ergebnis: list[Any] = [None] * len(rohe)
    gruppen: dict[tuple[int, int, int], list[int]] = {}
    for nr, roh in enumerate(rohe):
        gruppen.setdefault((roh.breite, roh.hoehe, roh.kanaele), []).append(nr)
    for (breite, hoehe, kanaele), nummern in gruppen.items():
        stapel = np.stack(
            [np.frombuffer(rohe[nr].gefiltert, dtype=np.uint8).reshape(hoehe, 1 + breite * kanaele) for nr in nummern]
        )
        bilder = _entfiltern(stapel, kanaele)
        for k, nr in enumerate(nummern):
            ergebnis[nr] = bilder[k].reshape(hoehe, breite, kanaele)
    return ergebnis


def dekodiere_png(daten: bytes) -> Any:
    """Dekodiert eine PNG-Datei zu einem uint8-Array (Höhe, Breite, Kanäle)."""
    return dekodiere_pngs([daten])[0]


def terrarium_hoehen(rgb: Any) -> Any:
    """Höhe in Metern aus Terrarium-Pixeln (Array …×3 oder …×4, uint8): (R·256 + G + B/256) − 32768."""
    a = np.asarray(rgb)
    r = a[..., 0].astype(np.float64)
    g = a[..., 1].astype(np.float64)
    b = a[..., 2].astype(np.float64)
    return r * 256.0 + g + b / 256.0 - 32768.0


# ─── Web Mercator ───────────────────────────────────────────────────────────


def mercator_pixel(laengen: Any, breiten: Any, zoom: int) -> tuple[Any, Any]:
    """Globale Pixelkoordinaten (x, y) auf Zoomstufe ``zoom`` (Kachel k deckt [256·k, 256·k + 256))."""
    welt = float(KACHELGROESSE * (1 << zoom))
    laengen = np.asarray(laengen, dtype=np.float64)
    phi = np.radians(np.asarray(breiten, dtype=np.float64))
    x = (laengen + 180.0) / 360.0 * welt
    y = (1.0 - np.log(np.tan(np.pi / 4.0 + phi / 2.0)) / np.pi) / 2.0 * welt
    return x, y


def mercator_geo(x: Any, y: Any, zoom: int) -> tuple[Any, Any]:
    """Umkehrung von ``mercator_pixel``: Länge und Breite (Grad) zu globalen Pixelkoordinaten."""
    welt = float(KACHELGROESSE * (1 << zoom))
    laengen = np.asarray(x, dtype=np.float64) / welt * 360.0 - 180.0
    n = np.pi * (1.0 - 2.0 * np.asarray(y, dtype=np.float64) / welt)
    breiten = np.degrees(np.arctan(np.sinh(n)))
    return laengen, breiten


# ─── Höhenmodell ────────────────────────────────────────────────────────────


@dataclass(frozen=True)
class HoehenStatistik:
    """Höhen der Pixel im Umkreis (Meter); ``relief`` = Maximum − Minimum."""

    mittel: float
    minimum: float
    maximum: float
    relief: float
    streuung: float  # Standardabweichung
    anzahl: int  # Zahl der ausgewerteten Pixel


class Hoehenmodell:
    """Höhenmosaik aus Terrarium-Kacheln einer Zoomstufe (Web Mercator).

    ``hoehen[zeile, spalte]`` ist die Höhe des Pixels mit globalen Koordinaten
    ``(x0 · 256 + spalte, y0 · 256 + zeile)``; ein Pixel deckt das Einheitsquadrat ab, sein Wert gilt
    für die Mitte.
    """

    def __init__(self, zoom: int, x0: int, y0: int, hoehen: Any) -> None:
        self.zoom = zoom
        self.x0 = x0
        self.y0 = y0
        self.hoehen = np.asarray(hoehen, dtype=np.float32)
        self.zeilen, self.spalten = self.hoehen.shape

    @classmethod
    def aus_kacheln(cls, zoom: int, kacheln: dict[tuple[int, int], bytes]) -> Hoehenmodell:
        """Baut das Mosaik aus PNG-Dateien je Kachel (x, y); der Bereich muss lückenlos sein."""
        if not kacheln:
            raise HoehenFehler("keine Höhenkacheln angegeben")
        xs = sorted({x for x, _ in kacheln})
        ys = sorted({y for _, y in kacheln})
        x0, y0 = xs[0], ys[0]
        fehlend = [(x, y) for x in range(x0, xs[-1] + 1) for y in range(y0, ys[-1] + 1) if (x, y) not in kacheln]
        if fehlend:
            raise HoehenFehler(f"Höhenkacheln fehlen: {', '.join(f'{zoom}/{x}/{y}' for x, y in fehlend[:5])}")
        schluessel = sorted(kacheln)
        bilder = dekodiere_pngs([kacheln[k] for k in schluessel])
        mosaik = np.zeros(((ys[-1] - y0 + 1) * KACHELGROESSE, (xs[-1] - x0 + 1) * KACHELGROESSE), dtype=np.float32)
        for (x, y), bild in zip(schluessel, bilder):
            if bild.shape[0] != KACHELGROESSE or bild.shape[1] != KACHELGROESSE or bild.shape[2] < 3:
                raise HoehenFehler(f"Kachel {zoom}/{x}/{y}: erwartet {KACHELGROESSE}×{KACHELGROESSE} Pixel RGB")
            z, s = (y - y0) * KACHELGROESSE, (x - x0) * KACHELGROESSE
            mosaik[z : z + KACHELGROESSE, s : s + KACHELGROESSE] = terrarium_hoehen(bild)
        return cls(zoom, x0, y0, mosaik)

    @classmethod
    def aus_zwischenspeicher(
        cls, speicher: Path = geodaten.ZWISCHENSPEICHER, manifest_pfad: Path = geodaten.MANIFEST, pruefen: bool = True
    ) -> Hoehenmodell:
        """Lädt alle in quellen.json gepinnten Kacheln aus dem Zwischenspeicher (mit SHA-256-Prüfung)."""
        manifest = geodaten.lade_manifest(manifest_pfad)
        angabe = manifest.get("kacheln")
        if not angabe:
            raise HoehenFehler(f"{manifest_pfad}: kein Eintrag 'kacheln'")
        pins: dict[str, str] = angabe.get("sha256") or {}
        dateien: dict[tuple[int, int], bytes] = {}
        fehlend = []
        for z, x, y in geodaten.kacheln_liste(angabe):
            pfad = speicher / angabe["datei"].format(z=z, x=x, y=y)
            if not pfad.is_file():
                fehlend.append(f"{z}/{x}/{y}")
                continue
            daten = pfad.read_bytes()
            if pruefen:
                erwartet = pins.get(geodaten.kachel_schluessel(z, x, y), "")
                if not erwartet or hashlib.sha256(daten).hexdigest() != erwartet:
                    raise HoehenFehler(f"Kachel {z}/{x}/{y}: SHA-256 fehlt in quellen.json oder stimmt nicht")
            dateien[(x, y)] = daten
        if fehlend:
            raise HoehenFehler(
                f"{len(fehlend)} Höhenkacheln fehlen im Zwischenspeicher (z. B. {fehlend[0]}); "
                "zuerst 'python3 tools/geodaten/geodaten.py holen hoehen'"
            )
        return cls.aus_kacheln(int(angabe["zoom"]), dateien)

    # ── Abfragen ──

    def _mosaik_koordinaten(self, laengen: Any, breiten: Any) -> tuple[Any, Any]:
        x, y = mercator_pixel(laengen, breiten, self.zoom)
        return x - self.x0 * KACHELGROESSE, y - self.y0 * KACHELGROESSE

    def deckt_ab(self, laenge: float, breite: float) -> bool:
        """Liegt der Punkt im Mosaik (mit einem halben Pixel Rand für die Interpolation)?"""
        s, z = self._mosaik_koordinaten([laenge], [breite])
        return bool(0.5 <= s[0] <= self.spalten - 0.5 and 0.5 <= z[0] <= self.zeilen - 0.5)

    def hoehen_an(self, laengen: Any, breiten: Any) -> Any:
        """Vektorisiert: Höhen (Meter, bilinear zwischen Pixelmitten); NaN außerhalb des Mosaiks."""
        s, z = self._mosaik_koordinaten(laengen, breiten)
        s = np.atleast_1d(s) - 0.5
        z = np.atleast_1d(z) - 0.5
        innen = (s >= 0.0) & (s <= self.spalten - 1) & (z >= 0.0) & (z <= self.zeilen - 1)
        ergebnis = np.full(s.shape, np.nan, dtype=np.float64)
        if innen.any():
            si, zi = s[innen], z[innen]
            s0 = np.minimum(np.floor(si).astype(np.int64), self.spalten - 2)
            z0 = np.minimum(np.floor(zi).astype(np.int64), self.zeilen - 2)
            fs, fz = si - s0, zi - z0
            h = self.hoehen
            oben = h[z0, s0] * (1.0 - fs) + h[z0, s0 + 1] * fs
            unten = h[z0 + 1, s0] * (1.0 - fs) + h[z0 + 1, s0 + 1] * fs
            ergebnis[innen] = oben * (1.0 - fz) + unten * fz
        return ergebnis

    def hoehe(self, laenge: float, breite: float) -> float:
        """Höhe in Metern an (länge, breite); ``HoehenFehler`` außerhalb des Mosaiks."""
        wert = float(self.hoehen_an([laenge], [breite])[0])
        if math.isnan(wert):
            raise HoehenFehler(f"Punkt ({laenge} {breite}) liegt außerhalb der Höhenkacheln")
        return wert

    def statistik(
        self,
        laenge: float,
        breite: float,
        radius_km: float,
        maske: Callable[[Any, Any], Any] | None = None,
    ) -> HoehenStatistik:
        """Höhenstatistik der Pixel, deren Mitte höchstens ``radius_km`` vom Punkt entfernt liegt.

        ``maske(längen, breiten)`` (vektorisiert, bool) schränkt die Pixel weiter ein, z. B. auf Land
        (``Rastermaske.enthaelt_viele``), damit Meerestiefen das Mittel nicht verfälschen. Liegt kein
        Pixel im Umkreis (sehr kleiner Radius), zählt die interpolierte Höhe am Punkt selbst.
        """
        if radius_km <= 0.0:
            raise ValueError("radius_km muss größer als 0 sein")
        dbreite = radius_km / KM_PRO_GRAD
        dlaenge = radius_km / (KM_PRO_GRAD * max(1e-9, math.cos(math.radians(min(89.0, abs(breite) + dbreite)))))
        s_a, z_b = self._mosaik_koordinaten([laenge - dlaenge, laenge + dlaenge], [breite + dbreite, breite - dbreite])
        s0 = max(0, int(math.floor(s_a[0])))
        s1 = min(self.spalten, int(math.ceil(s_a[1])) + 1)
        z0 = max(0, int(math.floor(z_b[0])))
        z1 = min(self.zeilen, int(math.ceil(z_b[1])) + 1)
        werte = np.empty(0, dtype=np.float64)
        if s1 > s0 and z1 > z0:
            spalten = np.arange(s0, s1, dtype=np.float64) + 0.5 + self.x0 * KACHELGROESSE
            zeilen = np.arange(z0, z1, dtype=np.float64) + 0.5 + self.y0 * KACHELGROESSE
            laengen, _ = mercator_geo(spalten, np.zeros_like(spalten), self.zoom)
            _, breiten = mercator_geo(np.zeros_like(zeilen), zeilen, self.zoom)
            gl, gb = np.meshgrid(laengen, breiten)
            faktor = np.cos(np.radians((gb + breite) / 2.0))
            dx = (gl - laenge) * KM_PRO_GRAD * faktor
            dy = (gb - breite) * KM_PRO_GRAD
            auswahl = dx * dx + dy * dy <= radius_km * radius_km
            if maske is not None:
                auswahl &= np.asarray(maske(gl, gb), dtype=bool)
            werte = self.hoehen[z0:z1, s0:s1][auswahl].astype(np.float64)
        if werte.size == 0:
            werte = np.array([self.hoehe(laenge, breite)])
        minimum, maximum = float(werte.min()), float(werte.max())
        return HoehenStatistik(
            mittel=float(werte.mean()),
            minimum=minimum,
            maximum=maximum,
            relief=maximum - minimum,
            streuung=float(werte.std()),
            anzahl=int(werte.size),
        )


# ─── Abstand zu Küste und Flüssen ───────────────────────────────────────────


def kuestenlinien(land: Sequence[geo.Polygon], rahmen: Sequence[float] | None) -> list[geo.Linie]:
    """Ränder der Landpolygone ohne die künstlichen Kanten am Zuschnittrahmen (xmin, ymin, xmax, ymax)."""
    linien: list[geo.Linie] = []
    for polygon in land:
        for ring in polygon:
            aktuell: geo.Linie = []
            for i in range(1, len(ring)):
                a, b = ring[i - 1], ring[i]
                kuenstlich = rahmen is not None and (
                    (a[0] == b[0] and a[0] in (rahmen[0], rahmen[2]))
                    or (a[1] == b[1] and a[1] in (rahmen[1], rahmen[3]))
                )
                if kuenstlich:
                    if len(aktuell) >= 2:
                        linien.append(aktuell)
                    aktuell = []
                    continue
                if not aktuell:
                    aktuell = [a]
                aktuell.append(b)
            if len(aktuell) >= 2:
                linien.append(aktuell)
    return linien


class Gewaesserabstand:
    """Abstand eines Punktes zur Meeresküste und zu Flüssen (km, Abschnitt 6.1).

    Küste = Rand von ``land.geojson`` (auch an Haffen und Lagunen, die als Meer gelten; nicht an
    Binnenseen), ohne die Kanten am Zuschnittrahmen. Flüsse = Linien aus ``fluesse.geojson`` bis
    einschließlich ``max_rang``.
    """

    def __init__(self, kueste: Sequence[geo.Linie], fluesse: Sequence[geo.Linie]) -> None:
        self._kueste = geo.LinienIndex(kueste)
        self._fluesse = geo.LinienIndex(fluesse)

    @classmethod
    def aus_geometrie(
        cls, ordner: Path = geodaten.GEOMETRIE_ORDNER, max_rang: int = GROSSE_FLUESSE_RANG
    ) -> Gewaesserabstand:
        """Liest land.geojson und fluesse.geojson aus ``ordner``."""
        land_daten = geo.lade_geojson(ordner / "land.geojson")
        rahmen = land_daten.get("ausschnitt")
        land = [p for f in land_daten.get("features") or [] for p in geo.geometrie_polygone(f.get("geometry"))]
        fluesse = geo.lade_linien(ordner / "fluesse.geojson", lambda e: int(e.get("rang", 6)) <= max_rang)
        return cls(kuestenlinien(land, rahmen), fluesse)

    def kueste_km(self, laenge: float, breite: float, max_km: float = 500.0) -> float | None:
        """Abstand zur Meeresküste in km; None, wenn weiter als ``max_km``."""
        return self._kueste.abstand_km(laenge, breite, max_km)

    def fluss_km(self, laenge: float, breite: float, max_km: float = 500.0) -> float | None:
        """Abstand zum nächsten Fluss (bis ``max_rang``) in km; None, wenn weiter als ``max_km``."""
        return self._fluesse.abstand_km(laenge, breite, max_km)


# ─── Zusammengefasst ────────────────────────────────────────────────────────


@dataclass(frozen=True)
class Lagemerkmale:
    """Merkmale eines Punktes für die Einordnung des Geländes."""

    laenge: float
    breite: float
    hoehe: float  # Meter, am Punkt (bilinear)
    umkreis: HoehenStatistik  # nur Landpixel im Umkreis
    radius_km: float
    auf_land: bool  # Kartenpixel des Punktes ist Land (land.geojson, Rasterregel)
    kueste_km: float | None
    fluss_km: float | None  # Abstand zu großen Flüssen (Rang ≤ max_rang)


class Lageauskunft:
    """Höhenmodell, Landmaske und Gewässerabstände zusammen; ``merkmale(länge, breite)`` je Punkt."""

    def __init__(self, hoehen: Hoehenmodell, land: geo.Rastermaske, gewaesser: Gewaesserabstand) -> None:
        self.hoehen = hoehen
        self.land = land
        self.gewaesser = gewaesser

    @classmethod
    def laden(
        cls,
        speicher: Path = geodaten.ZWISCHENSPEICHER,
        geometrie: Path = geodaten.GEOMETRIE_ORDNER,
        karte: Path = geodaten.KARTE,
        max_rang: int = GROSSE_FLUESSE_RANG,
    ) -> Lageauskunft:
        """Lädt alles aus dem Zwischenspeicher und data/karte/ (dauert einige Sekunden)."""
        k = geo.lade_karte(karte)
        land = geo.Rastermaske(geo.lade_flaechen(geometrie / "land.geojson"), k)
        return cls(
            Hoehenmodell.aus_zwischenspeicher(speicher), land, Gewaesserabstand.aus_geometrie(geometrie, max_rang)
        )

    def merkmale(self, laenge: float, breite: float, radius_km: float = 5.0) -> Lagemerkmale:
        """Höhe, Höhenstatistik (nur Land) im Umkreis und Abstände zu Küste und großen Flüssen."""
        return Lagemerkmale(
            laenge=laenge,
            breite=breite,
            hoehe=self.hoehen.hoehe(laenge, breite),
            umkreis=self.hoehen.statistik(laenge, breite, radius_km, self.land.enthaelt_viele),
            radius_km=radius_km,
            auf_land=self.land.enthaelt(laenge, breite),
            kueste_km=self.gewaesser.kueste_km(laenge, breite),
            fluss_km=self.gewaesser.fluss_km(laenge, breite),
        )


# ─── Programm ───────────────────────────────────────────────────────────────


def _km(wert: float | None) -> str:
    return "über 500 km" if wert is None else f"{wert:.1f} km"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="hoehen.py", description="Höhe und Lagemerkmale eines Punktes (zur Probe).")
    parser.add_argument("laenge", type=float, help="Grad Ost")
    parser.add_argument("breite", type=float, help="Grad Nord")
    parser.add_argument("--radius", type=float, default=5.0, help="Umkreis für die Statistik in km (Standard 5)")
    parser.add_argument(
        "--max-rang", type=int, default=GROSSE_FLUESSE_RANG, help="Flüsse bis zu diesem Rang (Standard 3)"
    )
    args = parser.parse_args(argv)
    try:
        auskunft = Lageauskunft.laden(max_rang=args.max_rang)
        m = auskunft.merkmale(args.laenge, args.breite, args.radius)
    except (geodaten.GeodatenFehler, OSError, ValueError) as fehler:
        print(f"Fehler: {fehler}", file=sys.stderr)
        return 2
    u = m.umkreis
    print(f"Punkt        {m.laenge:.4f} °O {m.breite:.4f} °N ({'Land' if m.auf_land else 'Wasser/außerhalb'})")
    print(f"Höhe         {m.hoehe:.0f} m")
    print(
        f"Umkreis {m.radius_km:g} km: Mittel {u.mittel:.0f} m, Minimum {u.minimum:.0f} m, Maximum {u.maximum:.0f} m, "
        f"Relief {u.relief:.0f} m, Streuung {u.streuung:.0f} m ({u.anzahl} Pixel)"
    )
    print(f"Küste        {_km(m.kueste_km)}")
    print(f"Fluss (Rang ≤ {args.max_rang}) {_km(m.fluss_km)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
