"""Gemeinsame Geometriehilfen der Geodaten-Werkzeuge.

Inhalt:

- ``Karte``: Kartenausschnitt aus ``data/karte/karte.chron`` und die verbindliche Projektion
  (docs/technik/chronik-skript.md, Abschnitt 6.1), Entfernungen in km.
- Punkt-in-Polygon nach der Rasterregel aus Abschnitt 6.2 (äußerer Ring minus Löcher, Punkte
  genau auf dem Rand zählen als außen; mehrere Polygone gelten als Vereinigung), einzeln
  (``punkt_in_polygon``) und als Index für große Flächen wie die Landmaske (``FlaechenIndex``).
- Rasterung nach derselben Regel (Pixelmitte im Polygon) in der Projektion aus Abschnitt 6.1
  (``rastere_polygone``, ``Rastermaske``).
- Abstand zu Linien in km (``LinienIndex``), z. B. zur Küste oder zu Flüssen.
- Zuschnitt auf ein Rechteck (``schneide_ring``, ``schneide_linie``) und Vereinfachung nach
  Douglas-Peucker (``vereinfache_linie``, ``vereinfache_ring``).
- GeoJSON lesen (``lade_flaechen``, ``lade_linien``) und deterministisch schreiben
  (``feature_collection_text``).

Koordinaten sind immer ``(länge, breite)`` in Grad (WGS84), wie in GeoJSON.

Die Rechnungen verwenden nur Grundrechenarten (``cos_grad`` ist eine feste Reihe statt
``math.cos``), damit gleiche Eingaben auf allen Plattformen gleiche Ergebnisse liefern.
``FlaechenIndex``, ``LinienIndex``, ``rastere_polygone`` und ``Rastermaske`` benötigen numpy.
"""

from __future__ import annotations

import json
import sys
from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

#: Wurzel des Repositorys (tools/geodaten/ → zwei Ebenen höher).
REPO = Path(__file__).resolve().parents[2]

PI = 3.141592653589793
#: Kilometer je Grad Breite (und je Grad Länge am Äquator), Abschnitt 6.1.
KM_PRO_GRAD = 111.32
#: Größte Bildkante (Texturgrenze) und erlaubte Abweichung des Seitenverhältnisses (Abschnitt 6.1).
MAX_BILDKANTE = 4096
MAX_SEITENABWEICHUNG = 0.01

Punkt = tuple[float, float]
Ring = list[Punkt]
#: Polygon: erster Ring außen, weitere Ringe sind Löcher.
Polygon = list[Ring]
Linie = list[Punkt]

__all__ = [
    "KM_PRO_GRAD",
    "REPO",
    "FlaechenIndex",
    "Karte",
    "Linie",
    "LinienIndex",
    "Polygon",
    "Punkt",
    "Rastermaske",
    "Ring",
    "abstand_km",
    "cos_grad",
    "feature_collection_text",
    "geometrie_linien",
    "geometrie_polygone",
    "geometrie_text",
    "lade_flaechen",
    "lade_geojson",
    "lade_karte",
    "lade_linien",
    "punkt_in_polygon",
    "punkt_in_ringen",
    "rastere_polygone",
    "ring_flaeche",
    "ring_schliessen",
    "schneide_linie",
    "schneide_ring",
    "vereinfache_linie",
    "vereinfache_ring",
    "zahl_text",
]


# ─── Zahlen ─────────────────────────────────────────────────────────────────

# Kehrwerte der Fakultäten (2k)! für die Kosinusreihe bis x²²; jede Division ist exakt gerundet.
_COS_KOEFFIZIENTEN: tuple[float, ...] = tuple(
    (-1.0 if k % 2 else 1.0) / float(_f)
    for k, _f in enumerate(
        (
            1,
            2,
            24,
            720,
            40320,
            3628800,
            479001600,
            87178291200,
            20922789888000,
            6402373705728000,
            2432902008176640000,
            1124000727777607680000,
        )
    )
)


def cos_grad(grad: float) -> float:
    """Kosinus eines Winkels in Grad, nur mit Grundrechenarten (plattformunabhängig).

    Genauigkeit besser als 1e-15 für Winkel zwischen −180° und 180°.
    """
    # Auf [-180, 180] bringen, dann über cos(x) = −cos(180° − |x|) auf [0, 90] falten.
    g = grad % 360.0
    if g > 180.0:
        g = 360.0 - g
    vorzeichen = 1.0
    if g > 90.0:
        g = 180.0 - g
        vorzeichen = -1.0
    x = g * PI / 180.0
    x2 = x * x
    summe = 0.0
    for koeffizient in reversed(_COS_KOEFFIZIENTEN):
        summe = summe * x2 + koeffizient
    return vorzeichen * summe


def zahl_text(wert: float, stellen: int) -> str:
    """Zahl mit höchstens ``stellen`` Nachkommastellen, ohne überflüssige Nullen ("13.4", "7")."""
    text = format(wert, f".{stellen}f")
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    if text in ("-0", ""):
        text = "0"
    return text


def abstand_km(laenge1: float, breite1: float, laenge2: float, breite2: float) -> float:
    """Entfernung in km nach Abschnitt 6.1 (111,32 km je Grad, Längengrade mit cos der mittleren Breite)."""
    faktor = cos_grad((breite1 + breite2) / 2.0)
    dx = (laenge2 - laenge1) * KM_PRO_GRAD * faktor
    dy = (breite2 - breite1) * KM_PRO_GRAD
    return float((dx * dx + dy * dy) ** 0.5)


# ─── Kartenausschnitt und Projektion ────────────────────────────────────────


@dataclass(frozen=True)
class Karte:
    """Kartenausschnitt und Projektion nach Abschnitt 6.1 (Plattkarte mit Bezugsbreite)."""

    bild_breite: int
    bild_hoehe: int
    laenge_min: float
    laenge_max: float
    breite_min: float
    breite_max: float
    bezugsbreite: float
    max_radius_km: float

    def nach_pixel(self, laenge: float, breite: float) -> tuple[float, float]:
        """Bildkoordinaten (x, y) eines Punktes; Pixel (px, py) deckt [px, px+1) × [py, py+1) ab."""
        x = (laenge - self.laenge_min) / (self.laenge_max - self.laenge_min) * self.bild_breite
        y = (self.breite_max - breite) / (self.breite_max - self.breite_min) * self.bild_hoehe
        return x, y

    def nach_pixel_viele(self, laengen: Any, breiten: Any) -> tuple[Any, Any]:
        """Vektorisiert (numpy): Bildkoordinaten (x, y) für Arrays von Längen und Breiten."""
        import numpy as np  # noqa: PLC0415

        x = (
            (np.asarray(laengen, dtype=np.float64) - self.laenge_min)
            / (self.laenge_max - self.laenge_min)
            * self.bild_breite
        )
        y = (
            (self.breite_max - np.asarray(breiten, dtype=np.float64))
            / (self.breite_max - self.breite_min)
            * self.bild_hoehe
        )
        return x, y

    def nach_geo(self, x: float, y: float) -> tuple[float, float]:
        """Länge und Breite zu Bildkoordinaten (Umkehrung von ``nach_pixel``)."""
        laenge = self.laenge_min + x / self.bild_breite * (self.laenge_max - self.laenge_min)
        breite = self.breite_max - y / self.bild_hoehe * (self.breite_max - self.breite_min)
        return laenge, breite

    @property
    def grad_je_pixel_x(self) -> float:
        """Breite eines Pixels in Grad Länge."""
        return (self.laenge_max - self.laenge_min) / self.bild_breite

    @property
    def grad_je_pixel_y(self) -> float:
        """Höhe eines Pixels in Grad Breite."""
        return (self.breite_max - self.breite_min) / self.bild_hoehe

    @property
    def pixel_km(self) -> float:
        """Kantenlänge eines Pixels in km (Nord-Süd-Richtung)."""
        return self.grad_je_pixel_y * KM_PRO_GRAD

    def seitenverhaeltnis_abweichung(self) -> float:
        """Relative Abweichung von bild_breite/bild_hoehe zum Sollverhältnis aus Abschnitt 6.1."""
        soll = (self.laenge_max - self.laenge_min) * cos_grad(self.bezugsbreite) / (self.breite_max - self.breite_min)
        ist = self.bild_breite / self.bild_hoehe
        return ist / soll - 1.0

    def pruefe(self) -> list[str]:
        """Prüft die Regeln aus Abschnitt 6.1; Ergebnis: Fehlermeldungen (leer = in Ordnung)."""
        fehler = []
        for name, wert in (("bild_breite", self.bild_breite), ("bild_hoehe", self.bild_hoehe)):
            if not 1 <= wert <= MAX_BILDKANTE:
                fehler.append(f"'{name}' muss zwischen 1 und {MAX_BILDKANTE} liegen (ist {wert})")
        if not -180.0 <= self.laenge_min < self.laenge_max <= 180.0:
            fehler.append("erwartet −180 ≤ laenge_min < laenge_max ≤ 180")
        if not -90.0 <= self.breite_min < self.breite_max <= 90.0:
            fehler.append("erwartet −90 ≤ breite_min < breite_max ≤ 90")
        if not -90.0 < self.bezugsbreite < 90.0:
            fehler.append("'bezugsbreite' muss zwischen −90 und 90 liegen")
        if not self.max_radius_km > 0.0:
            fehler.append("'max_radius_km' muss größer als 0 sein")
        if not fehler:
            abweichung = self.seitenverhaeltnis_abweichung()
            if abs(abweichung) > MAX_SEITENABWEICHUNG:
                soll = round(
                    self.bild_breite
                    * (self.breite_max - self.breite_min)
                    / ((self.laenge_max - self.laenge_min) * cos_grad(self.bezugsbreite))
                )
                fehler.append(
                    f"Seitenverhältnis weicht um {abweichung * 100:.2f} % vom Sollwert ab (erlaubt "
                    f"{MAX_SEITENABWEICHUNG * 100:.0f} %); bei bild_breite = {self.bild_breite} passt bild_hoehe = {soll}"
                )
        return fehler

    def rahmen(self, rand_grad: float = 0.0) -> tuple[float, float, float, float]:
        """Ausschnitt als (laenge_min, breite_min, laenge_max, breite_max), optional erweitert."""
        return (
            self.laenge_min - rand_grad,
            self.breite_min - rand_grad,
            self.laenge_max + rand_grad,
            self.breite_max + rand_grad,
        )


_KARTE_FELDER = (
    "bild_breite",
    "bild_hoehe",
    "laenge_min",
    "laenge_max",
    "breite_min",
    "breite_max",
    "bezugsbreite",
    "max_radius_km",
)


def lade_karte(pfad: Path | None = None) -> Karte:
    """Liest ``karte.chron`` (Standard: data/karte/karte.chron) mit dem Parser des Kartengenerators.

    Wirft ``ValueError`` mit deutscher Meldung, wenn die Datei fehlerhaft ist oder Felder fehlen.
    """
    pfad = pfad or REPO / "data/karte/karte.chron"
    chron = _chron_modul()
    ergebnis = chron.parse_script_file(pfad)
    if not ergebnis.ok:
        raise ValueError("; ".join(str(f) for f in ergebnis.errors))
    knoten = ergebnis.root.find("karte")
    if knoten is None:
        raise ValueError(f"{pfad}: Block 'karte' fehlt")
    werte: dict[str, float] = {}
    for feld in _KARTE_FELDER:
        wert = knoten.get_zahl(feld)
        if wert is None:
            raise ValueError(f"{pfad}: Feld '{feld}' fehlt oder ist keine Zahl")
        werte[feld] = wert
    return Karte(
        bild_breite=int(werte["bild_breite"]),
        bild_hoehe=int(werte["bild_hoehe"]),
        laenge_min=werte["laenge_min"],
        laenge_max=werte["laenge_max"],
        breite_min=werte["breite_min"],
        breite_max=werte["breite_max"],
        bezugsbreite=werte["bezugsbreite"],
        max_radius_km=werte["max_radius_km"],
    )


def _chron_modul() -> Any:
    """Der Chronik-Parser liegt beim Kartengenerator (gemeinsam mit dem Archivar genutzt)."""
    ordner = str(REPO / "tools/kartengenerator")
    if ordner not in sys.path:
        sys.path.insert(0, ordner)
    import chron  # noqa: PLC0415

    return chron


# ─── Ringe und Punkt-in-Polygon ─────────────────────────────────────────────


def ring_schliessen(ring: Sequence[Punkt]) -> Ring:
    """Ring mit gleichem Anfangs- und Endpunkt (GeoJSON-Form)."""
    punkte = [(float(x), float(y)) for x, y in ring]
    if punkte and punkte[0] != punkte[-1]:
        punkte.append(punkte[0])
    return punkte


def ring_flaeche(ring: Sequence[Punkt]) -> float:
    """Vorzeichenbehaftete Fläche (Gaußsche Trapezformel) in Grad²; positiv = gegen den Uhrzeigersinn."""
    summe = 0.0
    n = len(ring)
    for i in range(n):
        x0, y0 = ring[i]
        x1, y1 = ring[(i + 1) % n]
        summe += x0 * y1 - x1 * y0
    return summe / 2.0


def _auf_strecke(x: float, y: float, x0: float, y0: float, x1: float, y1: float) -> bool:
    """Liegt (x, y) exakt auf der Strecke (x0, y0)–(x1, y1)?"""
    if (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0) != 0.0:
        return False
    return min(x0, x1) <= x <= max(x0, x1) and min(y0, y1) <= y <= max(y0, y1)


def punkt_in_ringen(x: float, y: float, ringe: Iterable[Sequence[Punkt]]) -> bool:
    """Gerade-ungerade-Regel über alle Ringe; Punkte genau auf einem Rand zählen als außen."""
    innen = False
    for ring in ringe:
        n = len(ring)
        for i in range(n):
            x0, y0 = ring[i - 1]
            x1, y1 = ring[i]
            if _auf_strecke(x, y, x0, y0, x1, y1):
                return False
            if (y0 > y) != (y1 > y):
                xs = x0 + (y - y0) * (x1 - x0) / (y1 - y0)
                if xs > x:
                    innen = not innen
    return innen


def punkt_in_polygon(x: float, y: float, polygone: Sequence[Polygon]) -> bool:
    """Liegt (x, y) in einem der Polygone (äußerer Ring minus Löcher, Rand = außen)?

    Jedes Polygon wird für sich nach der Gerade-ungerade-Regel geprüft; mehrere Polygone gelten
    als Vereinigung (wie in der Rasterregel, Abschnitt 6.2).
    """
    return any(punkt_in_ringen(x, y, polygon) for polygon in polygone)


class FlaechenIndex:
    """Schnelle Punkt-in-Polygon-Abfrage für große Flächen (z. B. die Landmaske).

    Die Kanten werden nach Breitenbändern einsortiert; eine Abfrage zählt nur die Kanten ihres
    Bandes, getrennt je Polygon. Ergebnis wie ``punkt_in_polygon`` (Gerade-ungerade-Regel je
    Polygon, Vereinigung über alle Polygone, Rand zählt als außen). Benötigt numpy.
    """

    def __init__(self, polygone: Sequence[Polygon], band_grad: float = 0.02) -> None:
        import numpy as np  # noqa: PLC0415

        self._np = np
        kanten: list[tuple[float, float, float, float, float]] = []
        for nr, polygon in enumerate(polygone):
            for ring in polygon:
                n = len(ring)
                for i in range(n):
                    x0, y0 = ring[i - 1]
                    x1, y1 = ring[i]
                    if (x0, y0) != (x1, y1):
                        kanten.append((x0, y0, x1, y1, float(nr)))
        self.anzahl_kanten = len(kanten)
        self.anzahl_polygone = len(polygone)
        self._band = band_grad
        self._ymin = 0.0
        self._baender: list[Any] = []
        if not kanten:
            return
        ymin = min(min(k[1], k[3]) for k in kanten)
        ymax = max(max(k[1], k[3]) for k in kanten)
        self._ymin = ymin
        anzahl = int((ymax - ymin) / band_grad) + 1
        listen: list[list[int]] = [[] for _ in range(anzahl)]
        for nr, (_, y0, _, y1, _) in enumerate(kanten):
            b0 = int((min(y0, y1) - ymin) / band_grad)
            b1 = int((max(y0, y1) - ymin) / band_grad)
            for b in range(b0, min(b1, anzahl - 1) + 1):
                listen[b].append(nr)
        alle = np.array(kanten, dtype=np.float64)
        self._baender = [alle[np.array(liste, dtype=np.int64)] if liste else None for liste in listen]

    def enthaelt(self, x: float, y: float) -> bool:
        """Liegt der Punkt (länge, breite) in der Fläche?"""
        np = self._np
        b = int((y - self._ymin) / self._band) if y >= self._ymin else -1
        if b < 0 or b >= len(self._baender) or self._baender[b] is None:
            return False
        k = self._baender[b]
        x0, y0, x1, y1 = k[:, 0], k[:, 1], k[:, 2], k[:, 3]
        # Punkt genau auf einer Kante: außen (für das Polygon dieser Kante).
        kreuz = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0)
        auf = (
            (kreuz == 0.0)
            & (np.minimum(x0, x1) <= x)
            & (x <= np.maximum(x0, x1))
            & (np.minimum(y0, y1) <= y)
            & (y <= np.maximum(y0, y1))
        )
        schneidet = (y0 > y) != (y1 > y)
        with np.errstate(divide="ignore", invalid="ignore"):
            xs = x0 + (y - y0) * (x1 - x0) / (y1 - y0)
        polygon = k[:, 4].astype(np.int64)
        zaehler = np.bincount(polygon[schneidet & (xs > x)], minlength=self.anzahl_polygone)
        innen = zaehler % 2 == 1
        if bool(auf.any()):
            innen[np.unique(polygon[auf])] = False
        return bool(innen.any())


class LinienIndex:
    """Kürzester Abstand eines Punktes zu einer Menge von Linien, in km (Abschnitt 6.1).

    Die Strecken werden in ein Gitter einsortiert; gesucht wird ringweise um die Zelle des
    Punktes, bis kein näherer Treffer mehr möglich ist. Benötigt numpy.
    """

    def __init__(self, linien: Sequence[Linie], zelle_grad: float = 0.1) -> None:
        import numpy as np  # noqa: PLC0415

        self._np = np
        self._zelle = zelle_grad
        strecken: list[tuple[float, float, float, float]] = []
        for linie in linien:
            for i in range(1, len(linie)):
                (x0, y0), (x1, y1) = linie[i - 1], linie[i]
                strecken.append((x0, y0, x1, y1))
        self.anzahl_strecken = len(strecken)
        zellen: dict[tuple[int, int], list[int]] = {}
        for nr, (x0, y0, x1, y1) in enumerate(strecken):
            for cx in range(int(min(x0, x1) // zelle_grad), int(max(x0, x1) // zelle_grad) + 1):
                for cy in range(int(min(y0, y1) // zelle_grad), int(max(y0, y1) // zelle_grad) + 1):
                    zellen.setdefault((cx, cy), []).append(nr)
        alle = np.array(strecken, dtype=np.float64).reshape(-1, 4)
        self._zellen = {schluessel: alle[np.array(liste, dtype=np.int64)] for schluessel, liste in zellen.items()}

    def abstand_km(self, x: float, y: float, max_km: float = 500.0) -> float | None:
        """Abstand von (länge, breite) zur nächsten Linie in km; None, wenn weiter als ``max_km``."""
        np = self._np
        if not self._zellen:
            return None
        cx, cy = int(x // self._zelle), int(y // self._zelle)
        faktor = cos_grad(y)
        km_x = KM_PRO_GRAD * faktor
        km_y = KM_PRO_GRAD
        # Kleinste Zellkante in km: Wie weit reicht ein Suchring mindestens?
        zelle_km = self._zelle * min(km_x, km_y)
        bester: float | None = None
        r = 0
        while True:
            kandidaten = []
            for dx in range(-r, r + 1):
                for dy in range(-r, r + 1):
                    if max(abs(dx), abs(dy)) != r:
                        continue
                    k = self._zellen.get((cx + dx, cy + dy))
                    if k is not None:
                        kandidaten.append(k)
            if kandidaten:
                k = np.concatenate(kandidaten)
                ax = (k[:, 0] - x) * km_x
                ay = (k[:, 1] - y) * km_y
                bx = (k[:, 2] - x) * km_x
                by = (k[:, 3] - y) * km_y
                vx, vy = bx - ax, by - ay
                laenge2 = vx * vx + vy * vy
                with np.errstate(divide="ignore", invalid="ignore"):
                    t = np.where(laenge2 > 0.0, -(ax * vx + ay * vy) / laenge2, 0.0)
                t = np.clip(t, 0.0, 1.0)
                px, py = ax + t * vx, ay + t * vy
                d = float(np.sqrt(np.min(px * px + py * py)))
                if bester is None or d < bester:
                    bester = d
            # Alle Strecken außerhalb des Rings r sind mindestens r · zelle_km entfernt.
            if bester is not None and bester <= r * zelle_km:
                break
            if r * zelle_km > max_km:
                break
            r += 1
        if bester is None or bester > max_km:
            return None
        return bester


# ─── Rasterung ──────────────────────────────────────────────────────────────


def rastere_polygone(
    polygone: Sequence[Polygon], karte: Karte, breite: int | None = None, hoehe: int | None = None
) -> Any:
    """Bool-Raster (numpy, Höhe × Breite): Liegt die Pixelmitte in einem der Polygone?

    Regel aus Abschnitt 6.2: äußerer Ring minus Löcher (Gerade-ungerade-Regel je Polygon),
    Vereinigung über alle Polygone; Pixelmitten genau auf einer Kante zählen als außen.
    Projektion nach Abschnitt 6.1 auf ein Bild von ``breite`` × ``hoehe`` Pixeln (Standard:
    Bildgröße aus karte.chron). Nur Grundrechenarten im Pixelpfad.
    """
    import numpy as np  # noqa: PLC0415

    breite = karte.bild_breite if breite is None else breite
    hoehe = karte.bild_hoehe if hoehe is None else hoehe
    maske = np.zeros((hoehe, breite), dtype=bool)
    sx = breite / (karte.laenge_max - karte.laenge_min)
    sy = hoehe / (karte.breite_max - karte.breite_min)
    for polygon in polygone:
        teile = []
        for ring in polygon:
            a = np.asarray(ring, dtype=np.float64).reshape(-1, 2)
            if len(a) < 2:
                continue
            x = (a[:, 0] - karte.laenge_min) * sx
            y = (karte.breite_max - a[:, 1]) * sy
            teile.append(np.stack([x[:-1], y[:-1], x[1:], y[1:]], axis=1))
        if not teile:
            continue
        k = np.concatenate(teile)
        y0, y1 = np.minimum(k[:, 1], k[:, 3]), np.maximum(k[:, 1], k[:, 3])
        # Zeilen z, deren Mitte z + 0,5 in [y0, y1) liegt (halboffen: waagrechte Kanten zählen nicht).
        z0 = np.clip(np.ceil(y0 - 0.5), 0, hoehe).astype(np.int64)
        z1 = np.clip(np.ceil(y1 - 0.5), 0, hoehe).astype(np.int64)
        anzahl = z1 - z0
        gueltig = anzahl > 0
        if not gueltig.any():
            continue
        k, z0, anzahl = k[gueltig], z0[gueltig], anzahl[gueltig]
        nr = np.repeat(np.arange(len(k)), anzahl)
        versatz = np.arange(int(anzahl.sum())) - np.repeat(np.cumsum(anzahl) - anzahl, anzahl)
        zeile = z0[nr] + versatz
        kx0, ky0, kx1, ky1 = k[nr, 0], k[nr, 1], k[nr, 2], k[nr, 3]
        xs = kx0 + (zeile + 0.5 - ky0) * (kx1 - kx0) / (ky1 - ky0)
        ordnung = np.lexsort((xs, zeile))
        zeile, xs = zeile[ordnung], xs[ordnung]
        if len(zeile) % 2:
            raise ValueError("Rasterung: ungerade Zahl von Schnittpunkten (Ring nicht geschlossen?)")
        za, xa, xb = zeile[0::2], xs[0::2], xs[1::2]
        # Pixel px mit xa < px + 0,5 < xb.
        start = np.clip(np.floor(xa - 0.5).astype(np.int64) + 1, 0, breite)
        ende = np.clip(np.ceil(xb - 0.5).astype(np.int64), 0, breite)
        ok = ende > start
        if not ok.any():
            continue
        za, start, ende = za[ok], start[ok], ende[ok]
        # Nur das Zeilenband des Polygons bearbeiten (spart Speicher bei vielen kleinen Polygonen).
        r0, r1 = int(za.min()), int(za.max()) + 1
        diff = np.zeros((r1 - r0, breite + 1), dtype=np.int32)
        np.add.at(diff, (za - r0, start), 1)
        np.add.at(diff, (za - r0, ende), -1)
        maske[r0:r1] |= np.cumsum(diff[:, :breite], axis=1) > 0
    return maske


class Rastermaske:
    """Bool-Raster einer Fläche im Kartenausschnitt mit Abfrage nach Länge und Breite.

    Für viele Abfragen (z. B. alle Pixel eines Umkreises) schneller als ``FlaechenIndex``; die
    Antwort gilt für das Kartenpixel, in dem der Punkt liegt (Rasterregel aus Abschnitt 6.2).
    Punkte außerhalb des Ausschnitts gelten als außerhalb der Fläche. Benötigt numpy.
    """

    def __init__(
        self, polygone: Sequence[Polygon], karte: Karte, breite: int | None = None, hoehe: int | None = None
    ) -> None:
        self.karte = karte
        self.raster = rastere_polygone(polygone, karte, breite, hoehe)
        self.hoehe, self.breite = self.raster.shape

    def enthaelt_viele(self, laengen: Any, breiten: Any) -> Any:
        """Vektorisiert: bool-Array, ob die Punkte (Arrays gleicher Form) in der Fläche liegen."""
        import numpy as np  # noqa: PLC0415

        k = self.karte
        x = (np.asarray(laengen, dtype=np.float64) - k.laenge_min) * (self.breite / (k.laenge_max - k.laenge_min))
        y = (k.breite_max - np.asarray(breiten, dtype=np.float64)) * (self.hoehe / (k.breite_max - k.breite_min))
        px = np.floor(x).astype(np.int64)
        py = np.floor(y).astype(np.int64)
        innen = (px >= 0) & (px < self.breite) & (py >= 0) & (py < self.hoehe)
        ergebnis = np.zeros(px.shape, dtype=bool)
        ergebnis[innen] = self.raster[py[innen], px[innen]]
        return ergebnis

    def enthaelt(self, laenge: float, breite: float) -> bool:
        """Liegt der Punkt (länge, breite) in einem Pixel der Fläche?"""
        return bool(self.enthaelt_viele([laenge], [breite])[0])


# ─── Zuschnitt ──────────────────────────────────────────────────────────────


def schneide_ring(ring: Sequence[Punkt], rahmen: tuple[float, float, float, float]) -> Ring:
    """Schneidet einen Ring auf ein Rechteck (xmin, ymin, xmax, ymax) zu (Sutherland-Hodgman).

    Ergebnis ist ein geschlossener Ring (leer, wenn nichts übrig bleibt). Bei konkaven Ringen
    können entlang des Rechteckrands Kanten ohne Fläche entstehen; sie ändern die Fläche nicht.
    """
    xmin, ymin, xmax, ymax = rahmen
    punkte = [(float(x), float(y)) for x, y in ring]
    if punkte and punkte[0] == punkte[-1]:
        punkte.pop()
    if not punkte:
        return []
    xs = [p[0] for p in punkte]
    ys = [p[1] for p in punkte]
    if min(xs) >= xmin and max(xs) <= xmax and min(ys) >= ymin and max(ys) <= ymax:
        return ring_schliessen(punkte)
    if max(xs) <= xmin or min(xs) >= xmax or max(ys) <= ymin or min(ys) >= ymax:
        return []

    def schnitt_x(p: Punkt, q: Punkt, xk: float) -> Punkt:
        t = (xk - p[0]) / (q[0] - p[0])
        return (xk, p[1] + t * (q[1] - p[1]))

    def schnitt_y(p: Punkt, q: Punkt, yk: float) -> Punkt:
        t = (yk - p[1]) / (q[1] - p[1])
        return (p[0] + t * (q[0] - p[0]), yk)

    kanten = (
        (lambda p: p[0] >= xmin, lambda p, q: schnitt_x(p, q, xmin)),
        (lambda p: p[0] <= xmax, lambda p, q: schnitt_x(p, q, xmax)),
        (lambda p: p[1] >= ymin, lambda p, q: schnitt_y(p, q, ymin)),
        (lambda p: p[1] <= ymax, lambda p, q: schnitt_y(p, q, ymax)),
    )
    for innen, schnitt in kanten:
        if not punkte:
            break
        eingabe = punkte
        punkte = []
        vorher = eingabe[-1]
        vorher_innen = innen(vorher)
        for p in eingabe:
            p_innen = innen(p)
            if p_innen:
                if not vorher_innen:
                    punkte.append(schnitt(vorher, p))
                punkte.append(p)
            elif vorher_innen:
                punkte.append(schnitt(vorher, p))
            vorher, vorher_innen = p, p_innen
    # Doppelte Folgepunkte entfernen.
    bereinigt: Ring = []
    for p in punkte:
        if not bereinigt or bereinigt[-1] != p:
            bereinigt.append(p)
    while len(bereinigt) > 1 and bereinigt[0] == bereinigt[-1]:
        bereinigt.pop()
    if len(bereinigt) < 3:
        return []
    return ring_schliessen(bereinigt)


def schneide_linie(linie: Sequence[Punkt], rahmen: tuple[float, float, float, float]) -> list[Linie]:
    """Schneidet einen Linienzug auf ein Rechteck zu (Liang-Barsky je Strecke).

    Ergebnis: Liste der Teilstücke innerhalb des Rechtecks (je mindestens zwei Punkte).
    """
    xmin, ymin, xmax, ymax = rahmen
    stuecke: list[Linie] = []
    aktuell: Linie = []
    for i in range(1, len(linie)):
        x0, y0 = float(linie[i - 1][0]), float(linie[i - 1][1])
        x1, y1 = float(linie[i][0]), float(linie[i][1])
        dx, dy = x1 - x0, y1 - y0
        t0, t1 = 0.0, 1.0
        sichtbar = True
        for p, q in ((-dx, x0 - xmin), (dx, xmax - x0), (-dy, y0 - ymin), (dy, ymax - y0)):
            if p == 0.0:
                if q < 0.0:
                    sichtbar = False
                    break
                continue
            t = q / p
            if p < 0.0:
                if t > t1:
                    sichtbar = False
                    break
                t0 = max(t0, t)
            else:
                if t < t0:
                    sichtbar = False
                    break
                t1 = min(t1, t)
        if not sichtbar:
            if len(aktuell) >= 2:
                stuecke.append(aktuell)
            aktuell = []
            continue
        a = (x0 + t0 * dx, y0 + t0 * dy) if t0 > 0.0 else (x0, y0)
        b = (x0 + t1 * dx, y0 + t1 * dy) if t1 < 1.0 else (x1, y1)
        if aktuell and aktuell[-1] == a:
            aktuell.append(b)
        else:
            if len(aktuell) >= 2:
                stuecke.append(aktuell)
            aktuell = [a, b]
        if t1 < 1.0:
            stuecke.append(aktuell)
            aktuell = []
    if len(aktuell) >= 2:
        stuecke.append(aktuell)
    # Stücke ohne Länge (Linie berührt nur eine Ecke des Rechtecks) entfallen.
    return [s for s in stuecke if any(p != s[0] for p in s)]


# ─── Vereinfachung ──────────────────────────────────────────────────────────


def vereinfache_linie(linie: Sequence[Punkt], toleranz: float, skala_x: float = 1.0) -> Linie:
    """Douglas-Peucker: Punkte, die höchstens ``toleranz`` vom vereinfachten Zug abweichen, entfallen.

    ``skala_x`` streckt die x-Achse vor der Abstandsmessung (z. B. cos(Bezugsbreite)), damit die
    Toleranz in beiden Richtungen gleich weit reicht; die Toleranz gilt dann in Grad Breite.
    Anfangs- und Endpunkt bleiben immer erhalten.
    """
    n = len(linie)
    if n <= 2:
        return [(float(x), float(y)) for x, y in linie]
    xs = [float(p[0]) * skala_x for p in linie]
    ys = [float(p[1]) for p in linie]
    behalten = [False] * n
    behalten[0] = behalten[n - 1] = True
    toleranz2 = toleranz * toleranz
    stapel = [(0, n - 1)]
    while stapel:
        a, b = stapel.pop()
        if b - a < 2:
            continue
        ax, ay, bx, by = xs[a], ys[a], xs[b], ys[b]
        vx, vy = bx - ax, by - ay
        laenge2 = vx * vx + vy * vy
        bester, beste_d2 = -1, -1.0
        for i in range(a + 1, b):
            px, py = xs[i] - ax, ys[i] - ay
            if laenge2 > 0.0:
                t = (px * vx + py * vy) / laenge2
                if t < 0.0:
                    t = 0.0
                elif t > 1.0:
                    t = 1.0
                qx, qy = px - t * vx, py - t * vy
            else:
                qx, qy = px, py
            d2 = qx * qx + qy * qy
            if d2 > beste_d2:
                bester, beste_d2 = i, d2
        if beste_d2 > toleranz2:
            behalten[bester] = True
            stapel.append((bester, b))
            stapel.append((a, bester))
    return [(float(linie[i][0]), float(linie[i][1])) for i in range(n) if behalten[i]]


def vereinfache_ring(ring: Sequence[Punkt], toleranz: float, skala_x: float = 1.0) -> Ring:
    """Douglas-Peucker für einen geschlossenen Ring; leer, wenn weniger als drei Punkte bleiben.

    Der Ring wird am ersten Punkt und am davon entferntesten Punkt geteilt, beide Hälften werden
    getrennt vereinfacht (so bleibt die Form auch bei kleinen Ringen erhalten).
    """
    punkte = [(float(x), float(y)) for x, y in ring]
    if punkte and punkte[0] == punkte[-1]:
        punkte.pop()
    if len(punkte) < 3:
        return []
    x0, y0 = punkte[0]
    fern, fern_d2 = 0, -1.0
    for i, (x, y) in enumerate(punkte):
        d2 = ((x - x0) * skala_x) ** 2 + (y - y0) ** 2
        if d2 > fern_d2:
            fern, fern_d2 = i, d2
    if fern == 0:
        return []
    erste = vereinfache_linie(punkte[: fern + 1], toleranz, skala_x)
    zweite = vereinfache_linie(punkte[fern:] + [punkte[0]], toleranz, skala_x)
    ergebnis = erste[:-1] + zweite[:-1]
    if len(ergebnis) < 3:
        return []
    return ring_schliessen(ergebnis)


# ─── GeoJSON ────────────────────────────────────────────────────────────────


def lade_geojson(pfad: Path) -> dict[str, Any]:
    """Liest eine GeoJSON-Datei (UTF-8)."""
    with open(pfad, encoding="utf-8") as datei:
        daten = json.load(datei)
    if not isinstance(daten, dict):
        raise ValueError(f"{pfad}: kein GeoJSON-Objekt")
    return daten


def _features(daten: dict[str, Any]) -> list[dict[str, Any]]:
    if daten.get("type") == "FeatureCollection":
        return list(daten.get("features") or [])
    if daten.get("type") == "Feature":
        return [daten]
    return [{"type": "Feature", "properties": {}, "geometry": daten}]


def geometrie_polygone(geometrie: dict[str, Any] | None) -> list[Polygon]:
    """Polygone einer GeoJSON-Geometrie (Polygon oder MultiPolygon; sonst leer)."""
    if not geometrie:
        return []
    art = geometrie.get("type")
    koordinaten = geometrie.get("coordinates") or []
    if art == "Polygon":
        teile = [koordinaten]
    elif art == "MultiPolygon":
        teile = koordinaten
    elif art == "GeometryCollection":
        return [p for g in geometrie.get("geometries") or [] for p in geometrie_polygone(g)]
    else:
        return []
    polygone: list[Polygon] = []
    for teil in teile:
        ringe = [ring_schliessen((p[0], p[1]) for p in ring) for ring in teil if len(ring) >= 3]
        if ringe:
            polygone.append(ringe)
    return polygone


def geometrie_linien(geometrie: dict[str, Any] | None) -> list[Linie]:
    """Linienzüge einer GeoJSON-Geometrie (LineString oder MultiLineString; sonst leer)."""
    if not geometrie:
        return []
    art = geometrie.get("type")
    koordinaten = geometrie.get("coordinates") or []
    if art == "LineString":
        teile = [koordinaten]
    elif art == "MultiLineString":
        teile = koordinaten
    elif art == "GeometryCollection":
        return [l for g in geometrie.get("geometries") or [] for l in geometrie_linien(g)]
    else:
        return []
    return [[(float(p[0]), float(p[1])) for p in teil] for teil in teile if len(teil) >= 2]


def lade_flaechen(pfad: Path) -> list[Polygon]:
    """Alle Polygone einer GeoJSON-Datei (Polygon/MultiPolygon aller Features)."""
    return [p for f in _features(lade_geojson(pfad)) for p in geometrie_polygone(f.get("geometry"))]


def lade_linien(pfad: Path, filter_eigenschaften: Any = None) -> list[Linie]:
    """Alle Linienzüge einer GeoJSON-Datei; optional nur Features, für die ``filter_eigenschaften(props)`` wahr ist."""
    linien: list[Linie] = []
    for feature in _features(lade_geojson(pfad)):
        if filter_eigenschaften is not None and not filter_eigenschaften(feature.get("properties") or {}):
            continue
        linien.extend(geometrie_linien(feature.get("geometry")))
    return linien


def _koordinaten_text(punkte: Sequence[Punkt], stellen: int) -> str:
    return "[" + ",".join(f"[{zahl_text(x, stellen)},{zahl_text(y, stellen)}]" for x, y in punkte) + "]"


def geometrie_text(geometrie: dict[str, Any], stellen: int) -> str:
    """Kompakte, deterministische JSON-Darstellung einer Geometrie mit gerundeten Koordinaten."""
    art = geometrie["type"]
    koordinaten = geometrie["coordinates"]
    if art == "LineString":
        inhalt = _koordinaten_text(koordinaten, stellen)
    elif art in ("Polygon", "MultiLineString"):
        inhalt = "[" + ",".join(_koordinaten_text(r, stellen) for r in koordinaten) + "]"
    elif art == "MultiPolygon":
        inhalt = (
            "[" + ",".join("[" + ",".join(_koordinaten_text(r, stellen) for r in p) + "]" for p in koordinaten) + "]"
        )
    else:
        raise ValueError(f"Geometrieart {art} wird nicht geschrieben")
    return f'{{"type":"{art}","coordinates":{inhalt}}}'


def feature_collection_text(
    features: Sequence[dict[str, Any]], stellen: int = 4, kopf: dict[str, Any] | None = None
) -> str:
    """FeatureCollection als Text: ein Feature je Zeile, feste Feldreihenfolge, gerundete Koordinaten.

    ``kopf`` sind zusätzliche Felder der FeatureCollection (RFC 7946, Abschnitt 6.1, „foreign
    members“), z. B. Quelle und Lizenz. Gleiche Eingabe ergibt byte-gleiche Ausgabe.
    """
    zeilen = ['{"type":"FeatureCollection",']
    for schluessel, wert in (kopf or {}).items():
        zeilen.append(
            f"{json.dumps(schluessel, ensure_ascii=False)}:{json.dumps(wert, ensure_ascii=False, separators=(',', ':'))},"
        )
    zeilen.append('"features":[')
    for nr, feature in enumerate(features):
        eigenschaften = json.dumps(feature.get("properties") or {}, ensure_ascii=False, separators=(",", ":"))
        geometrie = geometrie_text(feature["geometry"], stellen)
        trenner = "," if nr + 1 < len(features) else ""
        zeilen.append(f'{{"type":"Feature","properties":{eigenschaften},"geometry":{geometrie}}}{trenner}')
    zeilen.append("]}")
    return "\n".join(zeilen) + "\n"
