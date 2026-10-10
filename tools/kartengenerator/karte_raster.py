"""Rasterverfahren des Kartengenerators (Spezifikation 6, Fassung M3), mit numpy.

Alle Schritte sind deterministisch und plattformunabhängig: Im Pixelpfad stehen nur
Grundrechenarten (+ − × ÷) auf IEEE-754-Zahlen, Vergleiche, Abrunden und ganzzahlige
Operationen, jeweils elementweise (numpy rechnet jede elementweise Operation exakt gerundet,
ohne FMA-Zusammenfassung). Keine Winkelfunktionen aus numpy oder ``math``, keine
Gleitkomma-Summen mit plattformabhängiger Reihenfolge (Flächen werden in Festkomma summiert).

Inhalt:

- :func:`cos_grad` – Kosinus aus Grundrechenarten (Taylorreihe)
- :func:`rastere_polygone` – Pixelmitte im Polygon (Löcher, MultiPolygon, Rand zählt als außen)
- :class:`Raster` – Flächenarten, Maßstäbe je Zeile und verschobene Pixelmitten (Domain Warping)
- :func:`zuordnen` – nächster Ort in km mit Rauschen, höchstens ``max_radius_km``
- :func:`bereinigen` – genau eine zusammenhängende Fläche je Ort (4er-Nachbarschaft)
- :func:`beschriftungspunkte` – Pixel mit größtem Abstand zum Rand (Chamfer 3-4)
- :func:`flaechen_km2`, :func:`zeichne_fluesse`, :func:`teilflaechen`
"""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass

import numpy as np

PI = 3.141592653589793
#: Kilometer je Grad Breite (und je Grad Länge am Äquator), Abschnitt 6.1.
KM_PRO_GRAD = 111.32

# Flächenarten im Blaukanal von provinzen.png (Abschnitt 6.3).
KERNLAND = 0
MEER = 1
BINNENGEWAESSER = 2
AUSLAND = 3

#: Startwert des Rauschens. Eine Änderung verschiebt alle Grenzen.
RAUSCHEN_SEED = 1648
#: Oktaven des Domain Warping: (Wellenlänge in km, Amplitude in km). Die Summe der Amplituden
#: ist die größte mögliche Verschiebung je Achse.
RAUSCHEN_OKTAVEN: tuple[tuple[float, float], ...] = (
    (40.0, 2.6),
    (20.0, 1.6),
    (10.0, 0.9),
    (5.0, 0.5),
    (2.5, 0.25),
)

#: Volle Linienbreite der Flüsse in Pixeln je Rang (1 = größter Fluss).
FLUSS_BREITE_PX: dict[int, float] = {1: 3.0, 2: 2.5, 3: 2.0, 4: 1.6, 5: 1.3, 6: 1.0}

#: Festkomma-Skala für Flächen (km² × 2^24): Summen bleiben ganzzahlig und damit exakt.
FLAECHE_BITS = 24

#: Höchstzahl der Runden beim Bereinigen abgetrennter Teilflächen.
BEREINIGUNG_RUNDEN = 50

_M32 = 0xFFFFFFFF


# ─── Kosinus ────────────────────────────────────────────────────────────────


def cos_grad(grad: float) -> float:
    """Kosinus eines Winkels in Grad, nur mit Grundrechenarten (plattformunabhängig bitgleich).

    Der Winkel wird auf 0°…90° gefaltet; die Taylorreihe bis x³⁰ (Horner) ist dort genauer als 1e-16.
    """
    g = float(grad) % 360.0
    if g > 180.0:
        g = 360.0 - g
    vorzeichen = 1.0
    if g > 90.0:
        g = 180.0 - g
        vorzeichen = -1.0
    x = g * (PI / 180.0)
    x2 = x * x
    summe = 1.0
    for n in range(30, 0, -2):
        summe = 1.0 - x2 / (n * (n - 1)) * summe
    return vorzeichen * summe


# ─── Kartenausschnitt ───────────────────────────────────────────────────────


@dataclass(frozen=True)
class Karte:
    """Projektion nach Abschnitt 6.1 (Plattkarte)."""

    breite: int
    hoehe: int
    laenge_min: float
    laenge_max: float
    breite_min: float
    breite_max: float
    bezugsbreite: float
    max_radius_km: float

    def nach_pixel(self, laengen: np.ndarray, breiten: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        """Pixelkoordinaten (Gleitkomma) nach der Formel aus Abschnitt 6.1."""
        x = (np.asarray(laengen, dtype=np.float64) - self.laenge_min) / (self.laenge_max - self.laenge_min)
        y = (self.breite_max - np.asarray(breiten, dtype=np.float64)) / (self.breite_max - self.breite_min)
        return x * self.breite, y * self.hoehe

    def pixelmitte(self, x: int, y: int) -> tuple[float, float]:
        """Länge und Breite der Mitte des Pixels (x, y)."""
        laenge = self.laenge_min + (x + 0.5) / self.breite * (self.laenge_max - self.laenge_min)
        breite = self.breite_max - (y + 0.5) / self.hoehe * (self.breite_max - self.breite_min)
        return laenge, breite


# ─── Rasterung von Polygonen ────────────────────────────────────────────────


def _erstes_groesser(werte: np.ndarray) -> np.ndarray:
    """Kleinste ganze Zahl p mit p + 0,5 > wert (exakt, auch bei Rundung in wert − 0,5)."""
    p = np.floor(werte - 0.5) + 1.0
    p = np.where(p - 0.5 > werte, p - 1.0, p)
    p = np.where(p + 0.5 <= werte, p + 1.0, p)
    return p


def _erstes_mindestens(werte: np.ndarray) -> np.ndarray:
    """Kleinste ganze Zahl p mit p + 0,5 ≥ wert (exakt)."""
    p = np.ceil(werte - 0.5)
    p = np.where(p - 0.5 >= werte, p - 1.0, p)
    p = np.where(p + 0.5 < werte, p + 1.0, p)
    return p


def _kanten(polygon: Sequence[np.ndarray], karte: Karte) -> np.ndarray:
    """Alle Kanten eines Polygons in Pixelkoordinaten als Feld (k, 4): x0 y0 x1 y1."""
    teile = []
    for ring in polygon:
        a = np.asarray(ring, dtype=np.float64).reshape(-1, 2)
        if len(a) < 3:
            continue
        if a[0, 0] != a[-1, 0] or a[0, 1] != a[-1, 1]:
            a = np.vstack([a, a[:1]])
        x, y = karte.nach_pixel(a[:, 0], a[:, 1])
        teile.append(np.stack([x[:-1], y[:-1], x[1:], y[1:]], axis=1))
    if not teile:
        return np.zeros((0, 4))
    return np.concatenate(teile)


def rastere_polygone(polygone: Sequence[Sequence[np.ndarray]], karte: Karte) -> np.ndarray:
    """Bool-Raster (Höhe × Breite): Liegt die Pixelmitte in einem der Polygone?

    Regel aus Abschnitt 6.2: Je Polygon zählt der äußere Ring minus Löcher (Gerade-ungerade-Regel
    über die Ringe des Polygons), über alle Polygone die Vereinigung. Pixelmitten genau auf einer
    Kante (auch einer waagrechten) zählen als außen. Scanline-Füllung: Schnittpunkte jeder
    Pixelzeile mit den Kanten, halboffene Kanten [y0, y1), Füllung zwischen Paaren.
    """
    breite, hoehe = karte.breite, karte.hoehe
    maske = np.zeros((hoehe, breite), dtype=bool)
    for polygon in polygone:
        k = _kanten(polygon, karte)
        if len(k) == 0:
            continue
        waagrecht = k[:, 1] == k[:, 3]
        schraeg = k[~waagrecht]
        # Kanten einheitlich nach unten ausrichten: Dieselbe Kante in zwei Polygonen ergibt
        # dieselben Schnittpunkte (gemeinsame Grenzen teilen die Pixel lückenlos auf).
        tauschen = schraeg[:, 1] > schraeg[:, 3]
        schraeg[tauschen] = schraeg[tauschen][:, [2, 3, 0, 1]]
        x0, y0, x1, y1 = schraeg[:, 0], schraeg[:, 1], schraeg[:, 2], schraeg[:, 3]
        # Zeilen z mit y0 ≤ z + 0,5 < y1.
        z0 = np.clip(_erstes_mindestens(y0), 0, hoehe).astype(np.int64)
        z1 = np.clip(_erstes_mindestens(y1), 0, hoehe).astype(np.int64)
        anzahl = z1 - z0
        gueltig = anzahl > 0
        if not gueltig.any():
            continue
        x0, y0, x1, y1, z0, anzahl = x0[gueltig], y0[gueltig], x1[gueltig], y1[gueltig], z0[gueltig], anzahl[gueltig]
        nr = np.repeat(np.arange(len(z0)), anzahl)
        versatz = np.arange(int(anzahl.sum())) - np.repeat(np.cumsum(anzahl) - anzahl, anzahl)
        zeile = z0[nr] + versatz
        mitte = zeile + 0.5
        xs = x0[nr] + (mitte - y0[nr]) * (x1[nr] - x0[nr]) / (y1[nr] - y0[nr])
        ordnung = np.lexsort((xs, zeile))
        zeile, xs = zeile[ordnung], xs[ordnung]
        if len(zeile) % 2 or np.any(zeile[0::2] != zeile[1::2]):
            raise ValueError("Rasterung: ungerade Zahl von Schnittpunkten in einer Zeile (Ring nicht geschlossen?)")
        za, xa, xb = zeile[0::2], xs[0::2], xs[1::2]
        start = np.clip(_erstes_groesser(xa), 0, breite).astype(np.int64)  # px + 0,5 > xa
        ende = np.clip(_erstes_mindestens(xb), 0, breite).astype(np.int64)  # px + 0,5 < xb
        ok = ende > start
        if not ok.any():
            continue
        za, start, ende = za[ok], start[ok], ende[ok]
        r0, r1 = int(za.min()), int(za.max()) + 1
        diff = np.zeros((r1 - r0, breite + 1), dtype=np.int32)
        np.add.at(diff, (za - r0, start), 1)
        np.add.at(diff, (za - r0, ende), -1)
        band = np.cumsum(diff[:, :breite], axis=1) > 0
        # Pixelmitten auf waagrechten Kanten liegen auf dem Rand: außen.
        for kx0, ky, kx1, _ in k[waagrecht]:
            zz = ky - 0.5
            if zz != np.floor(zz) or not r0 <= zz < r1:
                continue
            lo, hi = min(kx0, kx1), max(kx0, kx1)
            s = int(np.clip(_erstes_mindestens(np.array([lo]))[0], 0, breite))
            e = int(np.clip(_erstes_groesser(np.array([hi]))[0], 0, breite))
            if e > s:
                band[int(zz) - r0, s:e] = False
        # Pixelmitten genau auf einer Ecke: Bei unteren Ecken zählt keine der beiden Kanten
        # (halboffen), die Mitte kann dann in einem anderen Intervall liegen – sie ist aber Rand.
        ex, ey = k[:, 0] - 0.5, k[:, 1] - 0.5
        auf_mitte = (ex == np.floor(ex)) & (ey == np.floor(ey)) & (ex >= 0) & (ex < breite) & (ey >= r0) & (ey < r1)
        if auf_mitte.any():
            band[ey[auf_mitte].astype(np.int64) - r0, ex[auf_mitte].astype(np.int64)] = False
        maske[r0:r1] |= band
    return maske


# ─── Deterministisches Rauschen (Value Noise) ───────────────────────────────


def _gitterwerte(i: np.ndarray, j: np.ndarray, saat: int) -> np.ndarray:
    """Werte der Gitterpunkte (i, j) in [−1, 1), exakt darstellbar (ganzzahliger Hash)."""
    m = np.uint64(_M32)
    iu = (np.asarray(i, dtype=np.int64) & _M32).astype(np.uint64)
    ju = (np.asarray(j, dtype=np.int64) & _M32).astype(np.uint64)
    h = (np.uint64((saat * 0x9E3779B1) & _M32) + iu * np.uint64(0x85EBCA77) + ju * np.uint64(0xC2B2AE3D)) & m
    h ^= h >> np.uint64(15)
    h = (h * np.uint64(0x2C1B3C6D)) & m
    h ^= h >> np.uint64(12)
    h = (h * np.uint64(0x297A2D39)) & m
    h ^= h >> np.uint64(15)
    return (h >> np.uint64(8)).astype(np.float64) * (1.0 / 8388608.0) - 1.0


def _glaetten(t: np.ndarray) -> np.ndarray:
    """Quintische Glättung 6t⁵ − 15t⁴ + 10t³."""
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def _saat(feld: int, oktave: int) -> int:
    return (RAUSCHEN_SEED << 8) | (feld << 4) | oktave


def rauschamplitude() -> float:
    """Größte mögliche Verschiebung je Achse in km."""
    summe = 0.0
    for _, amplitude in RAUSCHEN_OKTAVEN:
        summe += amplitude
    return summe


def verschiebung_punkte(x_km: np.ndarray, y_km: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Verschiebung (nach Osten, nach Süden) in km an beliebigen Punkten des Rauschgebiets.

    Rechnet dieselben Operationen in derselben Reihenfolge wie :func:`verschiebung_raster`;
    an Pixelmitten sind beide bitgleich.
    """
    x_km = np.asarray(x_km, dtype=np.float64)
    y_km = np.asarray(y_km, dtype=np.float64)
    ergebnis = []
    for feld in (0, 1):
        summe = np.zeros(np.broadcast(x_km, y_km).shape)
        for oktave, (welle, amplitude) in enumerate(RAUSCHEN_OKTAVEN):
            saat = _saat(feld, oktave)
            u = x_km / welle
            v = y_km / welle
            iu = np.floor(u)
            jv = np.floor(v)
            tx = _glaetten(u - iu)
            ty = _glaetten(v - jv)
            i = iu.astype(np.int64)
            j = jv.astype(np.int64)
            a00 = _gitterwerte(i, j, saat)
            a01 = _gitterwerte(i, j + 1, saat)
            a10 = _gitterwerte(i + 1, j, saat)
            a11 = _gitterwerte(i + 1, j + 1, saat)
            c0 = a00 + (a01 - a00) * ty
            c1 = a10 + (a11 - a10) * ty
            summe = summe + amplitude * (c0 + (c1 - c0) * tx)
        ergebnis.append(summe)
    return ergebnis[0], ergebnis[1]


def verschiebung_raster(x_spalten: np.ndarray, y_zeilen: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Verschiebung für das Gitter aller Kombinationen (Zeilen × Spalten), separierbar ausgewertet."""
    x_spalten = np.asarray(x_spalten, dtype=np.float64)
    y_zeilen = np.asarray(y_zeilen, dtype=np.float64)
    form = (len(y_zeilen), len(x_spalten))
    ergebnis = []
    for feld in (0, 1):
        summe = np.zeros(form)
        for oktave, (welle, amplitude) in enumerate(RAUSCHEN_OKTAVEN):
            saat = _saat(feld, oktave)
            u = x_spalten / welle
            v = y_zeilen / welle
            iu = np.floor(u)
            jv = np.floor(v)
            tx = _glaetten(u - iu)
            ty = _glaetten(v - jv)
            i = iu.astype(np.int64)
            j = jv.astype(np.int64)
            i0, j0 = int(i.min()), int(j.min())
            gi = np.arange(i0, int(i.max()) + 2, dtype=np.int64)
            gj = np.arange(j0, int(j.max()) + 2, dtype=np.int64)
            gitter = _gitterwerte(gi[None, :], gj[:, None], saat)
            g0 = gitter[j - j0]
            g1 = gitter[j - j0 + 1]
            linie = g0 + (g1 - g0) * ty[:, None]  # c0 bzw. c1 für jede Gitterspalte
            links = linie[:, i - i0]
            rechts = linie[:, i - i0 + 1]
            summe = summe + amplitude * (links + (rechts - links) * tx[None, :])
        ergebnis.append(summe)
    return ergebnis[0], ergebnis[1]


# ─── Raster: Flächenarten, Maßstäbe, verschobene Pixelmitten ────────────────


@dataclass
class Raster:
    """Alles, was nur vom Kartenausschnitt und der Geometrie abhängt (für mehrere Läufe nutzbar)."""

    karte: Karte
    art: np.ndarray  # uint8 (H, W): Flächenart
    kx: np.ndarray  # (H,) km je Grad Länge in jeder Zeile (Pixelmitte)
    spalte_grad: np.ndarray  # (W,) Grad östlich von laenge_min (Pixelmitte)
    zeile_km: np.ndarray  # (H,) km südlich von breite_max (Pixelmitte)
    kx_bezug: float  # km je Grad Länge auf der Bezugsbreite (Rauschgebiet)
    pixel_flaeche_fix: list[int]  # je Zeile: Fläche eines Pixels in km² × 2^FLAECHE_BITS
    px_km: np.ndarray | None = None  # (H, W) verschobene Pixelmitte, km nach Osten (Maßstab der Zeile)
    py_km: np.ndarray | None = None  # (H, W) verschobene Pixelmitte, km nach Süden

    @staticmethod
    def erzeuge(
        karte: Karte,
        land: Sequence[Sequence[np.ndarray]],
        seen: Sequence[Sequence[np.ndarray]],
        kernraum: Sequence[Sequence[np.ndarray]],
    ) -> Raster:
        b, h = karte.breite, karte.hoehe
        land_m = rastere_polygone(land, karte)
        seen_m = rastere_polygone(seen, karte) & land_m
        kern_m = rastere_polygone(kernraum, karte)
        art = np.full((h, b), MEER, dtype=np.uint8)
        art[land_m] = AUSLAND
        art[land_m & kern_m] = KERNLAND
        art[seen_m] = BINNENGEWAESSER
        grad_x = karte.laenge_max - karte.laenge_min
        grad_y = karte.breite_max - karte.breite_min
        breiten = [karte.breite_max - (y + 0.5) / h * grad_y for y in range(h)]
        kx_liste = [KM_PRO_GRAD * cos_grad(phi) for phi in breiten]
        hoehe_km = grad_y / h * KM_PRO_GRAD
        skala = 1 << FLAECHE_BITS
        pixel_flaeche_fix = [round(grad_x / b * kx * hoehe_km * skala) for kx in kx_liste]
        return Raster(
            karte=karte,
            art=art,
            kx=np.array(kx_liste, dtype=np.float64),
            spalte_grad=(np.arange(b, dtype=np.float64) + 0.5) / b * grad_x,
            zeile_km=(np.arange(h, dtype=np.float64) + 0.5) / h * grad_y * KM_PRO_GRAD,
            kx_bezug=KM_PRO_GRAD * cos_grad(karte.bezugsbreite),
            pixel_flaeche_fix=pixel_flaeche_fix,
        )

    def verschiebe(self, band: int = 256) -> None:
        """Berechnet die verschobenen Pixelmitten (Domain Warping), falls noch nicht geschehen."""
        if self.px_km is not None:
            return
        h, b = self.art.shape
        px = np.empty((h, b))
        py = np.empty((h, b))
        x_rausch = self.spalte_grad * self.kx_bezug
        for y0 in range(0, h, band):
            y1 = min(y0 + band, h)
            dx, dy = verschiebung_raster(x_rausch, self.zeile_km[y0:y1])
            px[y0:y1] = self.spalte_grad[None, :] * self.kx[y0:y1, None] + dx
            py[y0:y1] = self.zeile_km[y0:y1, None] + dy
        self.px_km, self.py_km = px, py

    def freigeben(self) -> None:
        """Gibt die verschobenen Pixelmitten frei (etwa 220 MB bei voller Karte)."""
        self.px_km = self.py_km = None


# ─── Zuordnung ──────────────────────────────────────────────────────────────


@dataclass
class Lagen:
    """Lage der Orte im Raster."""

    innen: np.ndarray  # bool (n,): Lage im Bild
    px: np.ndarray  # int64 (n,), −1 außerhalb
    py: np.ndarray  # int64 (n,), −1 außerhalb
    art: np.ndarray  # int16 (n,): Flächenart am Lagepixel, −1 außerhalb


def lagen(raster: Raster, laengen: np.ndarray, breiten: np.ndarray) -> Lagen:
    karte = raster.karte
    fx, fy = karte.nach_pixel(laengen, breiten)
    innen = (fx >= 0.0) & (fx < karte.breite) & (fy >= 0.0) & (fy < karte.hoehe)
    px = np.where(innen, np.floor(np.where(innen, fx, 0.0)), -1).astype(np.int64)
    py = np.where(innen, np.floor(np.where(innen, fy, 0.0)), -1).astype(np.int64)
    art = np.full(len(px), -1, dtype=np.int16)
    art[innen] = raster.art[py[innen], px[innen]]
    return Lagen(innen, px, py, art)


def zuordnen(
    raster: Raster, laengen: np.ndarray, breiten: np.ndarray, lage: Lagen, *, vollsuche: bool = False
) -> np.ndarray:
    """Ordnet jedes Kernraum-Land-Pixel dem nächsten Ort zu (Abschnitt 6.3).

    Ergebnis: int32 (H, W), Kartenindex + 1, 0 = kein Ort. Entfernung in km zwischen der
    verschobenen Pixelmitte und der verschobenen Lage des Ortes; Orte im Bild stehen dabei auf der
    Mitte ihres Lagepixels (Entfernung zu diesem Pixel genau 0). Gleichstand: kleinerer Kartenindex.

    Je Ort wird nur ein Rechteck um seine verschobene Lage geprüft, das alle Pixel enthält, deren
    verschobene Mitte höchstens ``max_radius_km`` entfernt sein kann (Radius plus größte
    Verschiebung, ein Pixel Rand). ``vollsuche=True`` prüft jeden Ort für jedes Pixel (Referenz
    für Tests; gleiches Ergebnis).
    """
    raster.verschiebe()
    karte = raster.karte
    px_km, py_km = raster.px_km, raster.py_km
    assert px_km is not None and py_km is not None
    h, b = raster.art.shape
    r2 = karte.max_radius_km * karte.max_radius_km
    beste = np.where(raster.art == KERNLAND, np.nextafter(r2, np.inf), -1.0)
    werte = np.zeros((h, b), dtype=np.int32)
    n = len(laengen)
    if n == 0:
        return werte

    # Lage der Orte im Rauschgebiet und im km-Rahmen.
    laengen = np.asarray(laengen, dtype=np.float64)
    breiten = np.asarray(breiten, dtype=np.float64)
    ox_grad = laengen - karte.laenge_min
    oy_km = (karte.breite_max - breiten) * KM_PRO_GRAD
    innen = lage.innen
    ox_grad = np.where(innen, raster.spalte_grad[np.where(innen, lage.px, 0)], ox_grad)
    oy_km = np.where(innen, raster.zeile_km[np.where(innen, lage.py, 0)], oy_km)
    dxo, dyo = verschiebung_punkte(ox_grad * raster.kx_bezug, oy_km)
    oy = oy_km + dyo

    rand = karte.max_radius_km + rauschamplitude()
    zeile_schritt = (karte.breite_max - karte.breite_min) / h * KM_PRO_GRAD
    spalte_schritt = (karte.laenge_max - karte.laenge_min) / b
    kx = raster.kx
    for k in range(n):
        if vollsuche:
            y0, y1, x0, x1 = 0, h, 0, b
            kz = kx
        else:
            y0, y1, x0, x1, kz = _rechteck(k, oy, ox_grad, dxo, kx, rand, zeile_schritt, spalte_schritt, h, b)
            if y0 >= y1 or x0 >= x1:
                continue
        ox_zeile = ox_grad[k] * kz + dxo[k]
        d2 = px_km[y0:y1, x0:x1] - ox_zeile[:, None]
        np.multiply(d2, d2, out=d2)
        dy = py_km[y0:y1, x0:x1] - oy[k]
        np.multiply(dy, dy, out=dy)
        np.add(d2, dy, out=d2)
        besser = d2 < beste[y0:y1, x0:x1]
        np.copyto(beste[y0:y1, x0:x1], d2, where=besser)
        np.copyto(werte[y0:y1, x0:x1], k + 1, where=besser)
    return werte


def _rechteck(
    k: int,
    oy: np.ndarray,
    ox_grad: np.ndarray,
    dxo: np.ndarray,
    kx: np.ndarray,
    rand: float,
    zeile_schritt: float,
    spalte_schritt: float,
    h: int,
    b: int,
) -> tuple[int, int, int, int, np.ndarray]:
    """Pixelrechteck, außerhalb dessen Ort k kein Pixel erreichen kann (mit einem Pixel Sicherheitsrand)."""
    y0 = max(int(np.floor((oy[k] - rand) / zeile_schritt - 0.5)) - 1, 0)
    y1 = min(int(np.ceil((oy[k] + rand) / zeile_schritt - 0.5)) + 2, h)
    if y0 >= y1:
        return 0, 0, 0, 0, kx[:0]
    kz = kx[y0:y1]
    mitte = ox_grad[k] + dxo[k] / kz
    halb = rand / kz
    x0 = max(int(np.floor((mitte - halb).min() / spalte_schritt - 0.5)) - 1, 0)
    x1 = min(int(np.ceil((mitte + halb).max() / spalte_schritt - 0.5)) + 2, b)
    return y0, y1, x0, x1, kz


# ─── Läufe und Zusammenhangskomponenten ─────────────────────────────────────


@dataclass
class Laeufe:
    """Läufe gleicher Werte je Zeile (Run-Length-Darstellung eines Wertebildes)."""

    wert: np.ndarray  # Wert je Lauf
    y: np.ndarray
    x0: np.ndarray
    laenge: np.ndarray
    kennung: np.ndarray  # int32 (H, W): Lauf jedes Pixels


def laeufe(bild: np.ndarray) -> Laeufe:
    h, b = bild.shape
    start = np.empty((h, b), dtype=bool)
    start[:, 0] = True
    np.not_equal(bild[:, 1:], bild[:, :-1], out=start[:, 1:])
    flach = np.flatnonzero(start)
    kennung = np.cumsum(start.ravel(), dtype=np.int32).reshape(h, b)
    kennung -= 1
    y = flach // b
    ende = np.empty_like(flach)
    ende[:-1] = flach[1:]
    ende[-1] = h * b
    return Laeufe(bild.ravel()[flach], y, flach - y * b, ende - flach, kennung)


def _verbinde(anzahl: int, a: np.ndarray, c: np.ndarray) -> np.ndarray:
    """Zusammenhangskomponenten eines Graphen (Kanten a–c): je Knoten der kleinste Knoten seiner Komponente."""
    marke = np.arange(anzahl, dtype=np.int64)
    while len(a):
        ma, mc = marke[a], marke[c]
        verschieden = ma != mc
        if not verschieden.any():
            break
        ma, mc = ma[verschieden], mc[verschieden]
        np.minimum.at(marke, np.maximum(ma, mc), np.minimum(ma, mc))
        while True:
            neu = marke[marke]
            if np.array_equal(neu, marke):
                break
            marke = neu
        a, c = a[verschieden], c[verschieden]
    return marke


def komponenten(bild: np.ndarray, lf: Laeufe, aktiv: np.ndarray) -> np.ndarray:
    """Komponente (4er-Nachbarschaft, gleicher Wert) je Lauf: kleinster Lauf der Komponente, −1 inaktiv.

    ``aktiv``: bool je Lauf (nur aktive Läufe werden verbunden).
    """
    kenn = lf.kennung
    gleich = bild[1:] == bild[:-1]
    gleich &= aktiv[kenn[1:]]
    unten, oben = kenn[1:], kenn[:-1]
    neu = gleich.copy()
    neu[:, 1:] &= ~(gleich[:, :-1] & (unten[:, 1:] == unten[:, :-1]) & (oben[:, 1:] == oben[:, :-1]))
    marke = _verbinde(len(lf.wert), unten[neu].astype(np.int64), oben[neu].astype(np.int64))
    return np.where(aktiv, marke, -1)


def _komponenten_liste(lf: Laeufe, komp: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Komponenten (Kennung = kleinster Lauf), ihr Wert und ihre Pixelzahl."""
    aktiv = komp >= 0
    groesse = np.bincount(komp[aktiv], weights=lf.laenge[aktiv], minlength=len(komp)).astype(np.int64)
    kennungen = np.flatnonzero(aktiv & (komp == np.arange(len(komp))))
    return kennungen, lf.wert[kennungen], groesse[kennungen]


@dataclass
class Bereinigung:
    umverteilt: int = 0  # Teilflächen, die einem Nachbarort gegeben wurden
    entfernt: int = 0  # Teilflächen ohne angrenzenden Ort (danach kein Ort)
    entfernt_pixel: int = 0
    runden: int = 0


def bereinigen(werte: np.ndarray, lagepixel: dict[int, tuple[int, int]], anzahl: int) -> tuple[np.ndarray, Bereinigung]:
    """Sorgt für genau eine zusammenhängende Fläche (4er-Nachbarschaft) je Ort.

    Je Ort bleibt die Komponente mit dem Lagepixel (``lagepixel``: Wert → (x, y)), sonst die
    größte (Gleichstand: zuerst in Zeilenreihenfolge). Jede andere Teilfläche geht an den Ort, mit
    dessen Hauptfläche sie die längste Grenze teilt (Gleichstand: kleinerer Kartenindex); grenzt
    sie an keine Hauptfläche, wartet sie eine Runde (ein Nachbar kann inzwischen angewachsen
    sein). Bleibt sie übrig, gehört sie zu keinem Ort (Wert 0). Gibt das neue Bild zurück.
    """
    stat = Bereinigung()
    nmax = anzahl
    for _ in range(BEREINIGUNG_RUNDEN):
        stat.runden += 1
        lf = laeufe(werte)
        aktiv = lf.wert > 0
        komp = komponenten(werte, lf, aktiv)
        kennungen, kwert, kgroesse = _komponenten_liste(lf, komp)
        # Hauptfläche je Ort: größte, bei Gleichstand die erste.
        ordnung = np.lexsort((kennungen, -kgroesse, kwert))
        erste = np.ones(len(ordnung), dtype=bool)
        erste[1:] = kwert[ordnung][1:] != kwert[ordnung][:-1]
        haupt = np.full(nmax + 1, -1, dtype=np.int64)
        haupt[kwert[ordnung][erste]] = kennungen[ordnung][erste]
        for wert, (x, y) in lagepixel.items():
            if werte[y, x] == wert:
                haupt[wert] = komp[lf.kennung[y, x]]
        teil = aktiv & (haupt[np.where(aktiv, lf.wert, 0)] != komp)
        if not teil.any():
            break
        ist_haupt = aktiv & ~teil
        # Grenzen der Teilflächen zu Hauptflächen anderer Orte (Pixelkanten).
        paare_t, paare_n = [], []
        gleiche_zeile = lf.y[1:] == lf.y[:-1]
        links = np.flatnonzero(gleiche_zeile)
        paare_t += [links, links + 1]
        paare_n += [links + 1, links]
        kenn = lf.kennung
        senk = werte[1:] != werte[:-1]
        senk &= teil[kenn[1:]] | teil[kenn[:-1]]
        unten, oben = kenn[1:][senk], kenn[:-1][senk]
        paare_t += [unten, oben]
        paare_n += [oben, unten]
        t = np.concatenate(paare_t)
        nb = np.concatenate(paare_n)
        gut = teil[t] & ist_haupt[nb]
        t, nb = t[gut], nb[gut]
        neu_wert = lf.wert.copy()
        zugeteilt = 0
        if len(t):
            tk = komp[t]
            nw = lf.wert[nb].astype(np.int64)
            schluessel = tk * (nmax + 1) + nw
            einzel, zahl = np.unique(schluessel, return_counts=True)
            ek, ew = einzel // (nmax + 1), einzel % (nmax + 1)
            ordnung = np.lexsort((ew, -zahl, ek))
            erste = np.ones(len(ordnung), dtype=bool)
            erste[1:] = ek[ordnung][1:] != ek[ordnung][:-1]
            ziel_k, ziel_w = ek[ordnung][erste], ew[ordnung][erste]
            ziel = np.zeros(len(lf.wert), dtype=np.int64)
            ziel[ziel_k] = ziel_w
            laeufe_ziel = ziel[np.where(teil, komp, 0)]
            setzen = teil & (laeufe_ziel > 0)
            neu_wert[setzen] = laeufe_ziel[setzen]
            zugeteilt = len(ziel_k)
        if zugeteilt == 0:
            # Keine Teilfläche grenzt an eine Hauptfläche: Rest gehört zu keinem Ort.
            stat.entfernt += int(len(np.unique(komp[teil])))
            stat.entfernt_pixel += int(lf.laenge[teil].sum())
            neu_wert[teil] = 0
            werte = neu_wert.astype(np.int32)[lf.kennung]
            break
        stat.umverteilt += zugeteilt
        werte = neu_wert.astype(np.int32)[lf.kennung]
    return werte, stat


def teilflaechen(maske: np.ndarray) -> list[tuple[int, int, int]]:
    """Zusammenhangskomponenten (4er-Nachbarschaft) einer Maske: (Pixelzahl, x, y des ersten Pixels).

    Sortiert nach Pixelzahl absteigend, dann Zeilenreihenfolge.
    """
    if not maske.any():
        return []
    bild = maske.astype(np.int8)
    lf = laeufe(bild)
    aktiv = lf.wert == 1
    komp = komponenten(bild, lf, aktiv)
    kennungen, _, groesse = _komponenten_liste(lf, komp)
    ordnung = np.lexsort((kennungen, -groesse))
    return [(int(groesse[k]), int(lf.x0[kennungen[k]]), int(lf.y[kennungen[k]])) for k in ordnung]


# ─── Beschriftungspunkt (Chamfer-Abstand 3-4) ───────────────────────────────


def chamfer_abstand(marken: np.ndarray) -> np.ndarray:
    """Chamfer-3-4-Abstand jedes Pixels zum nächsten Pixel mit anderer Marke (oder zum Bildrand).

    Exakte Zwei-Durchgänge-Transformation (vorwärts, rückwärts); waagrechte Fortpflanzung
    innerhalb einer Zeile als abschnittsweises laufendes Minimum. Einheiten: 3 je Schritt
    waagrecht oder senkrecht, 4 je Schritt diagonal. Ergebnis int32.
    """
    h, b = marken.shape
    d = np.empty((h, b), dtype=np.int64)
    x3 = 3 * np.arange(b, dtype=np.int64)
    gross = np.int64(1 << 22)

    def waagrecht(v: np.ndarray, zeile: np.ndarray) -> np.ndarray:
        start = np.empty(b, dtype=bool)
        start[0] = True
        np.not_equal(zeile[1:], zeile[:-1], out=start[1:])
        v[start] = 3  # linker Nachbar mit anderer Marke oder Bildrand
        abschnitt = np.cumsum(start, dtype=np.int64) - 1
        versatz = abschnitt * gross
        return np.minimum.accumulate(v - x3 - versatz) + versatz + x3

    def von_nachbarzeile(v: np.ndarray, zeile: np.ndarray, nz: np.ndarray, nd: np.ndarray) -> None:
        np.minimum(v, np.where(nz == zeile, nd + 3, 3), out=v)
        v[1:] = np.minimum(v[1:], np.where(nz[:-1] == zeile[1:], nd[:-1] + 4, 4))
        v[0] = min(v[0], 4)
        v[:-1] = np.minimum(v[:-1], np.where(nz[1:] == zeile[:-1], nd[1:] + 4, 4))
        v[-1] = min(v[-1], 4)

    for y in range(h):
        zeile = marken[y]
        v = np.full(b, 3, dtype=np.int64)  # oberer Bildrand
        if y > 0:
            v[:] = np.iinfo(np.int64).max // 4
            von_nachbarzeile(v, zeile, marken[y - 1], d[y - 1])
        d[y] = waagrecht(v, zeile)
    for y in range(h - 1, -1, -1):
        zeile = marken[y]
        v = d[y].copy()
        if y < h - 1:
            von_nachbarzeile(v, zeile, marken[y + 1], d[y + 1])
        else:
            np.minimum(v, 3, out=v)  # unterer Bildrand
        d[y] = waagrecht(v[::-1].copy(), zeile[::-1])[::-1]
    return d.astype(np.int32)


def beschriftungspunkte(werte: np.ndarray, anzahl: int) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Je Ort (Wert 1…anzahl) das Pixel mit größtem Chamfer-Abstand zum Rand der Fläche.

    Gleichstand: erstes Pixel in Zeilenreihenfolge. Ergebnis: x, y (−1 ohne Pixel) und Abstand.
    """
    h, b = werte.shape
    d = chamfer_abstand(werte)
    flach_w = werte.ravel()
    flach_d = d.ravel()
    maximum = np.zeros(anzahl + 1, dtype=np.int64)
    np.maximum.at(maximum, flach_w, flach_d)
    treffer = np.flatnonzero((flach_d == maximum[flach_w]) & (flach_w > 0))
    w, erste = np.unique(flach_w[treffer], return_index=True)
    pos = treffer[erste]
    x = np.full(anzahl + 1, -1, dtype=np.int64)
    y = np.full(anzahl + 1, -1, dtype=np.int64)
    x[w] = pos % b
    y[w] = pos // b
    return x[1:], y[1:], maximum[1:]


# ─── Flächen ────────────────────────────────────────────────────────────────


def pixel_und_flaechen(raster: Raster, werte: np.ndarray, anzahl: int) -> tuple[np.ndarray, list[int]]:
    """Pixelzahl und Fläche in km² (gerundet) je Ort (Wert 1…anzahl).

    Fläche: Summe der Pixelflächen mit cos(Breite) der Zeilenmitte, in Festkomma summiert
    (ganzzahlige Summanden unter 2^53 – exakt in jeder Reihenfolge), dann kaufmännisch gerundet.
    """
    h, b = werte.shape
    flach = werte.ravel()
    pixel = np.bincount(flach, minlength=anzahl + 1)[1:].astype(np.int64)
    gewichte = np.repeat(np.array(raster.pixel_flaeche_fix, dtype=np.float64), b)
    summen = np.bincount(flach, weights=gewichte, minlength=anzahl + 1)[1:]
    halb = 1 << (FLAECHE_BITS - 1)
    flaechen = [(int(s) + halb) >> FLAECHE_BITS for s in summen]
    return pixel, flaechen


# ─── Flüsse ─────────────────────────────────────────────────────────────────


def _schwellen(breite_px: float) -> np.ndarray:
    """Quadrierte Abstandsschwellen für die Deckung 1…255, aufsteigend sortiert.

    Deckung = clamp(breite/2 + 0,5 − Abstand, 0, 1), auf 1/255 gerundet. Ein Pixel erreicht
    die Stufe k, wenn Abstand ≤ breite/2 + 0,5 − (k − 0,5)/255, also Abstand² ≤ Schwelle².
    """
    werte = []
    for k in range(1, 256):
        s = breite_px / 2.0 + 0.5 - (k - 0.5) / 255.0
        werte.append(s * s if s >= 0.0 else -1.0)
    return np.array(werte[::-1], dtype=np.float64)


def zeichne_fluesse(linien: Sequence[tuple[int, np.ndarray]], karte: Karte) -> np.ndarray:
    """Flussebene (Abschnitt 6.4): uint8 (H, W), Deckung durch Flüsse, kantengeglättet.

    Je Strecke wird der Abstand jeder Pixelmitte zur Strecke berechnet (nur Grundrechenarten);
    die Deckung folgt aus Schwellen auf dem quadrierten Abstand. Überlappungen: Maximum.
    """
    h, b = karte.hoehe, karte.breite
    bild = np.zeros((h, b), dtype=np.uint8)
    schwellen = {rang: _schwellen(breite) for rang, breite in FLUSS_BREITE_PX.items()}
    for rang, punkte in linien:
        breite_px = FLUSS_BREITE_PX[rang]
        reichweite = breite_px / 2.0 + 0.5
        tabelle = schwellen[rang]
        a = np.asarray(punkte, dtype=np.float64).reshape(-1, 2)
        if len(a) < 2:
            continue
        xs, ys = karte.nach_pixel(a[:, 0], a[:, 1])
        for k in range(len(a) - 1):
            ax, ay, bx, by = xs[k], ys[k], xs[k + 1], ys[k + 1]
            x0 = max(int(np.floor(min(ax, bx) - reichweite - 0.5)), 0)
            x1 = min(int(np.ceil(max(ax, bx) + reichweite + 0.5)) + 1, b)
            y0 = max(int(np.floor(min(ay, by) - reichweite - 0.5)), 0)
            y1 = min(int(np.ceil(max(ay, by) + reichweite + 0.5)) + 1, h)
            if x0 >= x1 or y0 >= y1:
                continue
            cx = np.arange(x0, x1, dtype=np.float64) + 0.5
            cy = np.arange(y0, y1, dtype=np.float64) + 0.5
            dxa = cx[None, :] - ax
            dya = cy[:, None] - ay
            ex, ey = bx - ax, by - ay
            l2 = ex * ex + ey * ey
            if l2 > 0.0:
                t = (dxa * ex + dya * ey) / l2
                t = np.clip(t, 0.0, 1.0)
                qx = dxa - t * ex
                qy = dya - t * ey
            else:
                qx = np.broadcast_to(dxa, (y1 - y0, x1 - x0))
                qy = np.broadcast_to(dya, (y1 - y0, x1 - x0))
            d2 = qx * qx + qy * qy
            deckung = (255 - np.searchsorted(tabelle, d2, side="left")).astype(np.uint8)
            np.maximum(bild[y0:y1, x0:x1], deckung, out=bild[y0:y1, x0:x1])
    return bild
