"""Tests der Geodaten-Werkzeuge (ohne Netz, ohne Zwischenspeicher).

Aufruf: python3 -m unittest discover -s tools/geodaten

Die Testdaten unter testdaten/ sind klein und von Hand bzw. mit einem unabhängigen Referenz-
kodierer erzeugt (PNG-Bilder mit allen fünf Zeilenfiltern, Auszug aus GeoNames cities1000).
Die letzten Testklassen prüfen die eingecheckten Daten (karte.chron, kernraum.geojson und die
erzeugten Geometriedateien) auf die Regeln aus docs/technik/chronik-skript.md, Abschnitt 6.
"""

from __future__ import annotations

import hashlib
import io
import json
import math
import random
import struct
import sys
import tarfile
import tempfile
import unittest
import zlib
from dataclasses import replace
from pathlib import Path
from unittest import mock

HIER = Path(__file__).resolve().parent
sys.path.insert(0, str(HIER))

import numpy as np  # noqa: E402

import geo  # noqa: E402
import geodaten  # noqa: E402
import hoehen  # noqa: E402
import staedte  # noqa: E402

TESTDATEN = HIER / "testdaten"
GEOMETRIE = geo.REPO / "data/karte/geometrie"


def karte_klein(breite: int = 4, hoehe: int = 4) -> geo.Karte:
    """Kleine Karte 0–4 °O × 0–4 °N (ein Pixel je Grad bei 4 × 4)."""
    return geo.Karte(breite, hoehe, 0.0, 4.0, 0.0, 4.0, 0.0, 10.0)


def quadrat(x0: float, y0: float, x1: float, y1: float) -> geo.Ring:
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1), (x0, y0)]


# ─── Zahlen, Projektion, Karte ──────────────────────────────────────────────


class TestZahlenUndKarte(unittest.TestCase):
    def test_cos_grad_entspricht_math_cos(self) -> None:
        for grad in (-180.0, -90.0, -45.5, 0.0, 30.0, 45.5, 51.0, 56.2, 89.9, 90.0, 135.0, 180.0, 400.0):
            self.assertAlmostEqual(geo.cos_grad(grad), math.cos(math.radians(grad)), places=14)

    def test_zahl_text(self) -> None:
        self.assertEqual(geo.zahl_text(13.4, 4), "13.4")
        self.assertEqual(geo.zahl_text(7.0, 4), "7")
        self.assertEqual(geo.zahl_text(-0.00001, 4), "0")
        self.assertEqual(geo.zahl_text(1.23456, 4), "1.2346")
        self.assertEqual(geo.zahl_text(-2.5, 1), "-2.5")

    def test_abstand_km(self) -> None:
        self.assertAlmostEqual(geo.abstand_km(10.0, 50.0, 10.0, 51.0), 111.32, places=9)
        # Ein Grad Länge bei 60° Breite ist halb so lang.
        self.assertAlmostEqual(geo.abstand_km(10.0, 60.0, 11.0, 60.0), 55.66, places=6)

    def test_projektion_hin_und_zurueck(self) -> None:
        k = geo.Karte(4096, 3329, 1.8, 23.5, 45.1, 56.2, 51.0, 25.0)
        x, y = k.nach_pixel(13.4, 52.52)
        laenge, breite = k.nach_geo(x, y)
        self.assertAlmostEqual(laenge, 13.4, places=12)
        self.assertAlmostEqual(breite, 52.52, places=12)
        self.assertEqual(k.nach_pixel(1.8, 56.2), (0.0, 0.0))
        self.assertEqual(k.nach_pixel(23.5, 45.1), (4096.0, 3329.0))
        xs, ys = k.nach_pixel_viele([13.4], [52.52])
        self.assertAlmostEqual(float(xs[0]), x, places=9)
        self.assertAlmostEqual(float(ys[0]), y, places=9)

    def test_karte_pruefen(self) -> None:
        gut = geo.Karte(4096, 3329, 1.8, 23.5, 45.1, 56.2, 51.0, 25.0)
        self.assertEqual(gut.pruefe(), [])
        self.assertLess(abs(gut.seitenverhaeltnis_abweichung()), 0.01)
        schief = replace(gut, bild_hoehe=3000)
        self.assertTrue(any("Seitenverhältnis" in f and "3329" in f for f in schief.pruefe()))
        zu_gross = replace(gut, bild_breite=5000)
        self.assertTrue(any("bild_breite" in f for f in zu_gross.pruefe()))
        verdreht = replace(gut, laenge_min=24.0)
        self.assertTrue(verdreht.pruefe())
        self.assertTrue(replace(gut, max_radius_km=0.0).pruefe())


# ─── Punkt in Polygon und Rasterung ─────────────────────────────────────────


class TestPunktInPolygon(unittest.TestCase):
    # Quadrat 0–10 mit Loch 4–6 und einer Insel 4.5–5.5 im Loch (zweites Polygon).
    POLYGON = [quadrat(0, 0, 10, 10), list(reversed(quadrat(4, 4, 6, 6)))]
    INSEL = [quadrat(4.5, 4.5, 5.5, 5.5)]

    def test_loch(self) -> None:
        self.assertTrue(geo.punkt_in_polygon(1.0, 1.0, [self.POLYGON]))
        self.assertFalse(geo.punkt_in_polygon(5.0, 4.2, [self.POLYGON]))  # im Loch
        self.assertFalse(geo.punkt_in_polygon(11.0, 5.0, [self.POLYGON]))
        self.assertTrue(geo.punkt_in_polygon(5.0, 5.0, [self.POLYGON, self.INSEL]))  # Insel im Loch

    def test_rand_zaehlt_als_aussen(self) -> None:
        self.assertFalse(geo.punkt_in_polygon(0.0, 5.0, [self.POLYGON]))  # äußerer Rand
        self.assertFalse(geo.punkt_in_polygon(10.0, 10.0, [self.POLYGON]))  # Ecke
        self.assertFalse(geo.punkt_in_polygon(4.0, 5.0, [self.POLYGON]))  # Rand des Lochs

    def test_vereinigung_ueberlappender_polygone(self) -> None:
        a = [quadrat(0, 0, 2, 2)]
        b = [quadrat(1, 1, 3, 3)]
        # Gerade-ungerade über alle Ringe würde die Überlappung als außen werten; verlangt ist die Vereinigung.
        self.assertTrue(geo.punkt_in_polygon(1.5, 1.5, [a, b]))
        self.assertTrue(geo.FlaechenIndex([a, b]).enthaelt(1.5, 1.5))
        self.assertTrue(geo.FlaechenIndex([a, a]).enthaelt(1.0, 1.0))

    def test_index_wie_einzelpruefung(self) -> None:
        polygone = [self.POLYGON, self.INSEL, [quadrat(20, 0, 25, 3)]]
        index = geo.FlaechenIndex(polygone, band_grad=0.7)
        zufall = random.Random(1648)
        punkte = [(zufall.uniform(-2, 27), zufall.uniform(-2, 12)) for _ in range(600)]
        punkte += [(0.0, 5.0), (4.0, 5.0), (20.0, 1.0), (22.0, 3.0), (5.5, 5.0)]
        for x, y in punkte:
            self.assertEqual(index.enthaelt(x, y), geo.punkt_in_polygon(x, y, polygone), (x, y))


class TestRasterung(unittest.TestCase):
    def test_pixelmitte_entscheidet(self) -> None:
        k = karte_klein()
        # Quadrat 1–3 × 1–3 Grad: Pixelmitten (1,5|1,5) (2,5|1,5) (1,5|2,5) (2,5|2,5) liegen innen.
        m = geo.rastere_polygone([[quadrat(1, 1, 3, 3)]], k)
        soll = np.zeros((4, 4), dtype=bool)
        soll[1:3, 1:3] = True
        np.testing.assert_array_equal(m, soll)

    def test_rand_durch_pixelmitte_ist_aussen(self) -> None:
        k = karte_klein()
        # Kanten genau durch Pixelmitten (x = 0,5 und x = 2,5): diese Spalten zählen als außen.
        m = geo.rastere_polygone([[quadrat(0.5, 0.2, 2.5, 3.8)]], k)
        self.assertEqual(m[:, 0].tolist(), [False] * 4)
        self.assertEqual(m[:, 1].tolist(), [True] * 4)
        self.assertEqual(m[:, 2].tolist(), [False] * 4)

    def test_loch_und_vereinigung(self) -> None:
        k = karte_klein(8, 8)
        ring = quadrat(0, 0, 4, 4)
        loch = list(reversed(quadrat(1.5, 1.5, 2.5, 2.5)))
        m = geo.rastere_polygone([[ring, loch], [quadrat(1.6, 1.6, 2.4, 2.4)]], k)
        self.assertEqual(int(m.sum()), 64)  # Loch durch das zweite Polygon wieder gefüllt
        m = geo.rastere_polygone([[ring, loch]], k)
        self.assertEqual(int(m.sum()), 60)
        self.assertFalse(m[3, 3] or m[3, 4] or m[4, 3] or m[4, 4])

    def test_rastermaske(self) -> None:
        maske = geo.Rastermaske([[quadrat(1, 1, 3, 3)]], karte_klein())
        self.assertTrue(maske.enthaelt(2.0, 2.0))
        self.assertFalse(maske.enthaelt(0.5, 0.5))
        self.assertFalse(maske.enthaelt(9.0, 2.0))  # außerhalb der Karte
        self.assertEqual(maske.enthaelt_viele(np.array([1.2, 3.5]), np.array([2.8, 2.0])).tolist(), [True, False])


# ─── Zuschnitt und Vereinfachung ────────────────────────────────────────────


class TestZuschnitt(unittest.TestCase):
    def test_ring_innen_aussen_teilweise(self) -> None:
        rahmen = (0.0, 0.0, 10.0, 10.0)
        innen = quadrat(1, 1, 2, 2)
        self.assertEqual(geo.schneide_ring(innen, rahmen), innen)
        self.assertEqual(geo.schneide_ring(quadrat(11, 1, 12, 2), rahmen), [])
        teil = geo.schneide_ring(quadrat(-5, -5, 5, 5), rahmen)
        self.assertEqual(teil[0], teil[-1])
        self.assertAlmostEqual(abs(geo.ring_flaeche(teil)), 25.0)
        self.assertTrue(all(0.0 <= x <= 10.0 and 0.0 <= y <= 10.0 for x, y in teil))

    def test_konkaver_ring_behaelt_flaeche(self) -> None:
        # U-Form, deren Öffnung über den Rahmen hinausragt.
        u = [(0, 0), (6, 0), (6, 6), (4, 6), (4, 2), (2, 2), (2, 6), (0, 6), (0, 0)]
        teil = geo.schneide_ring([(float(x), float(y)) for x, y in u], (-1.0, -1.0, 7.0, 4.0))
        self.assertAlmostEqual(abs(geo.ring_flaeche(teil)), 6 * 4 - 2 * 2)

    def test_linie(self) -> None:
        rahmen = (0.0, 0.0, 10.0, 10.0)
        stuecke = geo.schneide_linie([(-5.0, 5.0), (5.0, 5.0), (15.0, 5.0)], rahmen)
        self.assertEqual(stuecke, [[(0.0, 5.0), (5.0, 5.0), (10.0, 5.0)]])
        # Linie verlässt den Rahmen und kehrt zurück: zwei Stücke.
        stuecke = geo.schneide_linie([(1.0, 1.0), (1.0, 12.0), (2.0, 12.0), (2.0, 1.0)], rahmen)
        self.assertEqual(len(stuecke), 2)
        self.assertEqual(stuecke[0][-1], (1.0, 10.0))
        self.assertEqual(stuecke[1][0], (2.0, 10.0))
        self.assertEqual(geo.schneide_linie([(20.0, 0.0), (30.0, 5.0)], rahmen), [])


class TestVereinfachung(unittest.TestCase):
    def test_linie(self) -> None:
        linie = [(0.0, 0.0), (1.0, 0.001), (2.0, 0.0), (3.0, 1.2), (4.0, 2.0)]
        self.assertEqual(geo.vereinfache_linie(linie, 0.01), [(0.0, 0.0), (2.0, 0.0), (3.0, 1.2), (4.0, 2.0)])
        self.assertEqual(geo.vereinfache_linie(linie, 1.0), [(0.0, 0.0), (4.0, 2.0)])
        self.assertEqual(geo.vereinfache_linie(linie, 0.0001), linie)
        self.assertEqual(geo.vereinfache_linie(linie[:2], 10.0), linie[:2])

    def test_streckung_der_laengen(self) -> None:
        # Abweichung 0,1 Grad in x-Richtung zählt mit skala_x = 0,5 nur 0,05.
        linie = [(0.0, 0.0), (0.1, 1.0), (0.0, 2.0)]
        self.assertEqual(len(geo.vereinfache_linie(linie, 0.07, skala_x=1.0)), 3)
        self.assertEqual(len(geo.vereinfache_linie(linie, 0.07, skala_x=0.5)), 2)

    def test_ring(self) -> None:
        ring = [(0.0, 0.0), (1.0, 0.0), (2.0, 0.0), (2.0, 1.0), (2.0, 2.0), (0.0, 2.0), (0.0, 0.0)]
        ergebnis = geo.vereinfache_ring(ring, 0.01)
        self.assertEqual(ergebnis[0], ergebnis[-1])
        self.assertEqual(len(ergebnis), 5)
        self.assertAlmostEqual(abs(geo.ring_flaeche(ergebnis)), 4.0)
        self.assertEqual(geo.vereinfache_ring([(0.0, 0.0), (1.0, 0.0), (0.0, 0.0)], 0.01), [])


class TestGeojsonText(unittest.TestCase):
    def test_deterministisch_und_gerundet(self) -> None:
        features = [
            {
                "properties": {"name": "Weiße Elster", "rang": 6},
                "geometry": {"type": "LineString", "coordinates": [(12.123456, 51.000049), (12.2, 51.1)]},
            },
            {"properties": {}, "geometry": {"type": "Polygon", "coordinates": [quadrat(0, 0, 1, 1)]}},
        ]
        text = geo.feature_collection_text(features, 4, {"name": "Test"})
        self.assertEqual(text, geo.feature_collection_text(features, 4, {"name": "Test"}))
        daten = json.loads(text)
        self.assertEqual(daten["name"], "Test")
        self.assertEqual(daten["features"][0]["geometry"]["coordinates"][0], [12.1235, 51])
        self.assertIn("Weiße Elster", text)  # UTF-8, nicht als \\u-Folge
        self.assertTrue(text.endswith("]}\n"))

    def test_flaechen_ausrichtung_rfc7946(self) -> None:
        rb = geodaten.Rahmenbedingungen((-10.0, -10.0, 20.0, 20.0), 1e-6, 1.0, 1e-6, 1e-3)
        im_uhrzeigersinn = list(reversed(quadrat(0, 0, 10, 10)))
        loch_gegen = quadrat(4, 4, 6, 6)
        (polygon,) = geodaten.bearbeite_flaechen([[im_uhrzeigersinn, loch_gegen]], rb)
        self.assertGreater(geo.ring_flaeche(polygon[0]), 0.0)  # außen gegen den Uhrzeigersinn
        self.assertLess(geo.ring_flaeche(polygon[1]), 0.0)  # Loch im Uhrzeigersinn

    def test_doppelte_polygone_entfallen(self) -> None:
        bekannt: set[frozenset[geo.Punkt]] = set()
        a = [quadrat(0, 0, 1, 1)]
        b = [list(reversed(quadrat(0, 0, 1, 1)))]  # gleiche Punkte, andere Richtung
        c = [quadrat(2, 2, 3, 3)]
        self.assertEqual(geodaten._ohne_doppel([a, b], bekannt), [a])
        self.assertEqual(geodaten._ohne_doppel([b, c], bekannt), [c])


class TestFluesse(unittest.TestCase):
    def test_rang(self) -> None:
        self.assertEqual(
            [geodaten.flussrang(s) for s in (2, 5, 6, 7, 8, 9, 10, 11, 12, 13)], [1, 1, 2, 2, 3, 3, 4, 5, 6, 6]
        )
        self.assertEqual(geodaten.flussrang(None), 6)
        self.assertEqual(geodaten.flussrang("x"), 6)

    def test_namen(self) -> None:
        self.assertEqual(geodaten._name({"name": "Vistula"}, geodaten.FLUSSNAMEN_DE), "Weichsel")
        self.assertEqual(geodaten._name({"name": "Łyna", "name_de": "Alle"}, geodaten.FLUSSNAMEN_DE), "Alle")
        self.assertEqual(
            geodaten._name({"name": "x", "name_de": "Nysa Kłodzka"}, geodaten.FLUSSNAMEN_DE), "Glatzer Neiße"
        )
        self.assertEqual(
            geodaten._name({"name": "Schweriner See", "name_de": "Schweriner"}, geodaten.SEENAMEN_DE), "Schweriner See"
        )
        self.assertIsNone(geodaten._name({"name": " "}))

    def test_verdichten(self) -> None:
        punkte = geodaten.verdichte([(0.0, 0.0), (1.0, 0.0)], 0.3)
        self.assertEqual(punkte[0], (0.0, 0.0))
        self.assertEqual(punkte[-1], (1.0, 0.0))
        self.assertEqual(len(punkte), 5)
        self.assertTrue(all(b[0] - a[0] <= 0.3 for a, b in zip(punkte, punkte[1:])))

    def test_ausschneiden_im_see(self) -> None:
        # „See“ zwischen x = 4 und x = 6: die Linie zerfällt in zwei Stücke.
        def behalten(laengen: object, breiten: object) -> object:
            x = np.asarray(laengen)
            return (x < 4.0) | (x > 6.0)

        stuecke = geodaten.schneide_aus([(0.0, 0.0), (10.0, 0.0)], behalten, 0.5)
        self.assertEqual(len(stuecke), 2)
        self.assertEqual(stuecke[0][0], (0.0, 0.0))
        self.assertLess(stuecke[0][-1][0], 4.0)
        self.assertGreater(stuecke[1][0][0], 6.0)
        self.assertEqual(stuecke[1][-1], (10.0, 0.0))
        self.assertEqual(geodaten.schneide_aus([(4.5, 0.0), (5.5, 0.0)], behalten, 0.5), [])

    def test_bearbeite_linien_entfernt_kurze_stuecke(self) -> None:
        rb = geodaten.Rahmenbedingungen((0.0, 0.0, 10.0, 10.0), 0.01, 1.0, 1e-4, 0.1)
        lang = [(1.0, 1.0), (5.0, 1.0)]
        kurz = [(7.0, 7.0), (7.05, 7.0)]  # kürzer als zwei „Pixel“ (0,2)
        self.assertEqual(geodaten.bearbeite_linien([lang, kurz], rb), [lang])


# ─── Manifest und Laden ─────────────────────────────────────────────────────


class TestManifest(unittest.TestCase):
    def setUp(self) -> None:
        self._temp = tempfile.TemporaryDirectory(prefix="einigkeit-geo-test-")
        self.ordner = Path(self._temp.name)
        self.quelle = self.ordner / "quelle"
        self.quelle.mkdir()
        self.speicher = self.ordner / "speicher"
        self.inhalt = "Länge\tBreite\n13.4\t52.52\n".encode()
        (self.quelle / "daten.txt").write_bytes(self.inhalt)
        self.sha = hashlib.sha256(self.inhalt).hexdigest()

    def tearDown(self) -> None:
        self._temp.cleanup()

    def manifest(self, sha: str, **zusatz: object) -> Path:
        eintrag = {
            "datei": "test/daten.txt",
            "gruppe": "test",
            "url": (self.quelle / "daten.txt").as_uri(),
            "sha256": sha,
            "lizenz": "gemeinfrei",
            "quellenangabe": "Testdaten",
        }
        eintrag.update(zusatz)
        pfad = self.ordner / "quellen.json"
        pfad.write_text(json.dumps({"quellen": [eintrag]}), encoding="utf-8")
        return pfad

    def test_manifest_aufbau(self) -> None:
        m = geodaten.lade_manifest(self.manifest(self.sha))
        (q,) = geodaten.quellen(m)
        self.assertEqual(q.sha256, self.sha)
        self.assertEqual(geodaten.quellen(m, ["andere"]), [])
        kaputt = self.ordner / "kaputt.json"
        kaputt.write_text(json.dumps({"quellen": [{"datei": "x"}]}), encoding="utf-8")
        with self.assertRaises(geodaten.GeodatenFehler):
            geodaten.lade_manifest(kaputt)
        boese = self.manifest(self.sha, datei="../ausserhalb.txt")
        with self.assertRaises(geodaten.GeodatenFehler):
            geodaten.lade_manifest(boese)

    def test_holen_prueft_und_laedt_nicht_doppelt(self) -> None:
        (q,) = geodaten.quellen(geodaten.lade_manifest(self.manifest(self.sha)))
        with mock.patch("sys.stdout", new=io.StringIO()):
            self.assertFalse(geodaten.hole_quelle(q, self.speicher, pinnen=False))
            self.assertEqual((self.speicher / "test/daten.txt").read_bytes(), self.inhalt)
            # Zweiter Aufruf: Datei ist richtig, es wird nichts geladen.
            with mock.patch.object(geodaten, "lade_url", side_effect=AssertionError("darf nicht laden")):
                geodaten.hole_quelle(q, self.speicher, pinnen=False)
            # Veränderte Datei wird neu geladen.
            (self.speicher / "test/daten.txt").write_bytes(b"falsch")
            geodaten.hole_quelle(q, self.speicher, pinnen=False)
        self.assertEqual((self.speicher / "test/daten.txt").read_bytes(), self.inhalt)

    def test_falsche_pruefsumme(self) -> None:
        (q,) = geodaten.quellen(geodaten.lade_manifest(self.manifest("0" * 64)))
        with mock.patch("sys.stdout", new=io.StringIO()), self.assertRaises(geodaten.GeodatenFehler) as fehler:
            geodaten.hole_quelle(q, self.speicher, pinnen=False)
        self.assertIn("SHA-256 stimmt nicht", str(fehler.exception))
        self.assertFalse((self.speicher / "test/daten.txt").exists())
        self.assertFalse((self.speicher / "test/daten.txt.teil").exists())

    def test_pinnen(self) -> None:
        m = geodaten.lade_manifest(self.manifest(""))
        (q,) = geodaten.quellen(m)
        with mock.patch("sys.stdout", new=io.StringIO()):
            with self.assertRaises(geodaten.GeodatenFehler):
                geodaten.hole_quelle(q, self.speicher, pinnen=False)
            self.assertTrue(geodaten.hole_quelle(q, self.speicher, pinnen=True))
        self.assertEqual(m["quellen"][0]["sha256"], self.sha)

    def test_auspacken(self) -> None:
        archiv = self.quelle / "paket.tgz"
        glied = b"1\tOrt\n"
        with tarfile.open(archiv, "w:gz") as tar:
            info = tarfile.TarInfo("package/cities1000.txt")
            info.size = len(glied)
            tar.addfile(info, io.BytesIO(glied))
        sha_archiv = hashlib.sha256(archiv.read_bytes()).hexdigest()
        pfad = self.manifest(
            sha_archiv,
            url=archiv.as_uri(),
            datei="test/paket.tgz",
            auspacken={
                "glied": "package/cities1000.txt",
                "datei": "test/cities1000.txt",
                "sha256": hashlib.sha256(glied).hexdigest(),
            },
        )
        (q,) = geodaten.quellen(geodaten.lade_manifest(pfad))
        with mock.patch("sys.stdout", new=io.StringIO()):
            geodaten.hole_quelle(q, self.speicher, pinnen=False)
        self.assertEqual((self.speicher / "test/cities1000.txt").read_bytes(), glied)

    def test_kacheln(self) -> None:
        self.assertEqual(geodaten.kachel_x(13.4, 8), 137)
        self.assertEqual(geodaten.kachel_x(-180.0, 3), 0)
        x, y = hoehen.mercator_pixel([13.4], [52.52], 8)
        self.assertEqual(int(y[0] // 256), geodaten.kachel_y(52.52, 8))
        self.assertEqual(geodaten.kachel_y(0.0001, 1), 0)
        self.assertEqual(geodaten.kachel_y(-0.0001, 1), 1)
        liste = geodaten.kacheln_liste({"zoom": 8, "x": [1, 2], "y": [5, 6]})
        self.assertEqual(liste, [(8, 1, 5), (8, 1, 6), (8, 2, 5), (8, 2, 6)])


# ─── PNG und Höhen ──────────────────────────────────────────────────────────


def png_kodieren(bild: np.ndarray) -> bytes:
    """Minimaler PNG-Kodierer (RGB, Filter 0) für synthetische Höhenkacheln."""
    hoehe, breite = bild.shape[:2]
    roh = b"".join(b"\x00" + bild[z].astype(np.uint8).tobytes() for z in range(hoehe))

    def block(art: bytes, daten: bytes) -> bytes:
        return struct.pack(">I", len(daten)) + art + daten + struct.pack(">I", zlib.crc32(art + daten) & 0xFFFFFFFF)

    kopf = struct.pack(">IIBBBBB", breite, hoehe, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + block(b"IHDR", kopf) + block(b"IDAT", zlib.compress(roh)) + block(b"IEND", b"")


def terrarium_kodieren(h: np.ndarray) -> np.ndarray:
    v = np.asarray(h, dtype=np.float64) + 32768.0
    r = np.floor(v / 256.0)
    g = np.floor(v - r * 256.0)
    b = np.round((v - r * 256.0 - g) * 256.0)
    return np.stack([r, g, b], axis=-1).astype(np.uint8)


class TestPng(unittest.TestCase):
    ERWARTET = json.loads((TESTDATEN / "png_erwartet.json").read_text(encoding="utf-8"))

    def test_alle_filter(self) -> None:
        for name, angabe in self.ERWARTET.items():
            with self.subTest(name):
                bild = hoehen.dekodiere_png((TESTDATEN / name).read_bytes())
                np.testing.assert_array_equal(bild, np.array(angabe["pixel"], dtype=np.uint8))

    def test_stapel_wie_einzeln(self) -> None:
        daten = [(TESTDATEN / n).read_bytes() for n in ("filter_rgb.png", "filter_grau.png", "filter_rgb.png")]
        stapel = hoehen.dekodiere_pngs(daten)
        for d, s in zip(daten, stapel):
            np.testing.assert_array_equal(hoehen.dekodiere_png(d), s)

    def test_terrarium(self) -> None:
        bild = hoehen.dekodiere_png((TESTDATEN / "filter_rgb.png").read_bytes())
        np.testing.assert_array_equal(
            hoehen.terrarium_hoehen(bild), np.array(self.ERWARTET["filter_rgb.png"]["hoehen"])
        )
        self.assertEqual(float(hoehen.terrarium_hoehen(np.array([128, 0, 0]))), 0.0)
        self.assertEqual(float(hoehen.terrarium_hoehen(np.array([139, 146, 0, 255]))), 2962.0)

    def test_fehler(self) -> None:
        gut = (TESTDATEN / "filter_rgb.png").read_bytes()
        with self.assertRaises(hoehen.HoehenFehler):
            hoehen.dekodiere_png(b"GIF89a" + gut[6:])
        kaputt = bytearray(gut)
        kaputt[30] ^= 0xFF  # im IDAT-Block: Prüfsumme stimmt nicht mehr
        with self.assertRaises(hoehen.HoehenFehler):
            hoehen.dekodiere_png(bytes(kaputt))
        with self.assertRaises(hoehen.HoehenFehler):
            hoehen.dekodiere_png(gut[:40])
        sechzehn_bit = bytearray(gut)
        sechzehn_bit[24] = 16  # Bittiefe im IHDR
        sechzehn_bit[29:33] = struct.pack(">I", zlib.crc32(bytes(sechzehn_bit[12:29])) & 0xFFFFFFFF)
        with self.assertRaises(hoehen.HoehenFehler):
            hoehen.dekodiere_png(bytes(sechzehn_bit))


class TestHoehenmodell(unittest.TestCase):
    ZOOM = 8

    @classmethod
    def setUpClass(cls) -> None:
        # 2 × 2 Kacheln um Berlin (x 137–138, y 83–84); Höhe = globale Pixelspalte / 10 (in m, also
        # etwa 3500 m) zur Probe der Interpolation, dazu eine Spitze von 9000 m in einem Pixel.
        kacheln = {}
        for x in (137, 138):
            for y in (83, 84):
                spalten = (np.arange(256) + x * 256).astype(np.float64)
                h = np.tile(spalten / 10.0, (256, 1))
                if (x, y) == (137, 84):
                    h[100, 100] = 9000.0
                kacheln[(x, y)] = png_kodieren(terrarium_kodieren(h))
        cls.modell = hoehen.Hoehenmodell.aus_kacheln(cls.ZOOM, kacheln)

    def test_mosaik(self) -> None:
        self.assertEqual(self.modell.hoehen.shape, (512, 512))
        self.assertEqual((self.modell.x0, self.modell.y0), (137, 83))

    def test_mercator_hin_und_zurueck(self) -> None:
        x, y = hoehen.mercator_pixel([13.4, -70.0], [52.52, -33.0], self.ZOOM)
        laengen, breiten = hoehen.mercator_geo(x, y, self.ZOOM)
        np.testing.assert_allclose(laengen, [13.4, -70.0], atol=1e-9)
        np.testing.assert_allclose(breiten, [52.52, -33.0], atol=1e-9)

    def test_bilineare_hoehe(self) -> None:
        # Pixelmitte der globalen Spalte 35200 (Höhe 3520 m) und genau dazwischen.
        # Terrarium speichert Höhen in Schritten von 1/256 m (Kanal B).
        for spalte, soll in ((35200.5, 3520.0), (35201.0, 3520.05), (35300.25, 3529.975)):
            laenge, breite = hoehen.mercator_geo([spalte], [83 * 256 + 300.5], self.ZOOM)
            self.assertAlmostEqual(self.modell.hoehe(float(laenge[0]), float(breite[0])), soll, delta=1 / 256)
        with self.assertRaises(hoehen.HoehenFehler):
            self.modell.hoehe(30.0, 52.5)
        self.assertTrue(self.modell.deckt_ab(13.4, 52.52))
        self.assertFalse(self.modell.deckt_ab(30.0, 52.5))

    def test_statistik(self) -> None:
        laenge, breite = hoehen.mercator_geo([137 * 256 + 100.5], [84 * 256 + 100.5], self.ZOOM)
        laenge, breite = float(laenge[0]), float(breite[0])
        s = self.modell.statistik(laenge, breite, 2.0)
        self.assertEqual(s.maximum, 9000.0)
        self.assertEqual(s.relief, s.maximum - s.minimum)
        self.assertGreater(s.anzahl, 20)
        # Ohne die Spitze (die Maske schließt die mittlere Pixelspalte aus) bleibt das Maximum bei etwa 3510 m.
        ohne = self.modell.statistik(laenge, breite, 2.0, lambda gl, gb: np.abs(gl - laenge) > 1e-6)
        self.assertLess(ohne.maximum, 3600.0)
        self.assertGreater(ohne.minimum, 3400.0)
        # Winziger Radius: Höhe am Punkt selbst.
        winzig = self.modell.statistik(laenge, breite, 0.001)
        self.assertEqual(winzig.anzahl, 1)
        with self.assertRaises(ValueError):
            self.modell.statistik(laenge, breite, 0.0)

    def test_fehlende_kachel(self) -> None:
        bild = png_kodieren(terrarium_kodieren(np.zeros((256, 256))))
        with self.assertRaises(hoehen.HoehenFehler):
            hoehen.Hoehenmodell.aus_kacheln(8, {(1, 1): bild, (2, 2): bild})


class TestGewaesserabstand(unittest.TestCase):
    def test_kuestenlinien_ohne_rahmenkanten(self) -> None:
        rahmen = (0.0, 0.0, 10.0, 10.0)
        # Land füllt den Rahmen westlich von x = 5: nur die Kante bei x = 5 ist Küste.
        land = [[[(0.0, 0.0), (5.0, 0.0), (5.0, 10.0), (0.0, 10.0), (0.0, 0.0)]]]
        linien = hoehen.kuestenlinien(land, rahmen)
        self.assertEqual(linien, [[(5.0, 0.0), (5.0, 10.0)]])
        abstand = hoehen.Gewaesserabstand(linien, [])
        self.assertAlmostEqual(abstand.kueste_km(1.0, 5.0), 4 * 111.32 * geo.cos_grad(5.0), places=6)
        self.assertIsNone(abstand.fluss_km(1.0, 5.0))
        self.assertIsNone(abstand.kueste_km(1.0, 5.0, max_km=100.0))


# ─── GeoNames ───────────────────────────────────────────────────────────────


class TestStaedte(unittest.TestCase):
    def setUp(self) -> None:
        self.liste = list(staedte.lies_staedte(TESTDATEN / "cities_test.txt"))

    def test_parser(self) -> None:
        self.assertEqual(len(self.liste), 5)  # leere Zeile übersprungen
        breslau = self.liste[0]
        self.assertEqual((breslau.geonameid, breslau.name, breslau.staat), (3081368, "Wrocław", "PL"))
        self.assertIn("Breslau", breslau.alternativnamen)
        self.assertEqual(
            (breslau.breite, breslau.laenge, breslau.einwohner, breslau.dem), (51.1, 17.03333, 634893, 119)
        )
        self.assertIsNone(breslau.hoehe)
        self.assertEqual(breslau.merkmalscode, "PPLA")
        self.assertEqual(breslau.alle_namen()[:3], ["Wrocław", "Wroclaw", "Breslau"])
        berlin = self.liste[1]
        self.assertEqual(
            (berlin.hoehe, berlin.admin4, berlin.zeitzone, berlin.geaendert),
            (74, "11000000", "Europe/Berlin", "2019-07-13"),
        )
        test = self.liste[4]
        self.assertEqual(test.alternativnamen, ())
        self.assertEqual(test.weitere_staaten, ("DE", "PL"))
        self.assertEqual((test.einwohner, test.hoehe, test.dem), (0, None, None))
        self.assertEqual(self.liste[3].dem, -9999)

    def test_rahmen(self) -> None:
        namen = [s.name for s in staedte.lies_staedte(TESTDATEN / "cities_test.txt", rahmen=(7.0, 47.0, 14.0, 53.0))]
        self.assertEqual(namen, ["Berlin", "Basel", "Testdorf"])

    def test_fehlerhafte_zeilen(self) -> None:
        with self.assertRaises(staedte.StaedteFehler) as fehler:
            staedte.parse_zeile("1\tzu\twenig", 7)
        self.assertIn("Zeile 7", str(fehler.exception))
        zeile = (TESTDATEN / "cities_test.txt").read_text(encoding="utf-8").splitlines()[1].split("\t")
        zeile[4] = "nord"
        with self.assertRaises(staedte.StaedteFehler):
            staedte.parse_zeile("\t".join(zeile), 2)
        zeile[4] = "95.0"
        with self.assertRaises(staedte.StaedteFehler):
            staedte.parse_zeile("\t".join(zeile), 2)

    def test_kandidaten(self) -> None:
        kernraum = [[quadrat(5.0, 45.0, 20.0, 56.0)]]
        # Land: alles außer einem „Meer“ nördlich von 54° westlich von 9° (Helgoland liegt 0,18° darin).
        land = [[[(5.0, 45.0), (20.0, 45.0), (20.0, 56.0), (9.0, 56.0), (9.0, 54.0), (5.0, 54.0), (5.0, 45.0)]]]
        seen = [[quadrat(9.9, 49.9, 10.1, 50.1)]]  # Testdorf liegt mitten im See (> 2 km vom Ufer)
        k = staedte.kandidaten(self.liste, kernraum, land, seen)
        namen = [x.stadt.name for x in k]
        self.assertEqual(namen, ["Berlin", "Wrocław"])  # Basel (CH) entfällt immer, nach Einwohnern sortiert
        self.assertTrue(all(x.auf_land and x.wasserabstand_km == 0.0 for x in k))
        # Mit größerer Toleranz bleibt Helgoland (etwa 20 km vom „Ufer“ bei 54° N) als Ort im Wasser.
        k = staedte.kandidaten(self.liste, kernraum, land, seen, land_toleranz_km=25.0)
        helgoland = [x for x in k if x.stadt.name == "Helgoland"]
        self.assertEqual(len(helgoland), 1)
        self.assertFalse(helgoland[0].auf_land)
        self.assertAlmostEqual(helgoland[0].wasserabstand_km, (54.18143 - 54.0) * 111.32, places=3)
        self.assertEqual(
            [x.stadt.name for x in staedte.kandidaten(self.liste, kernraum, land, seen, min_einwohner=1_000_000)],
            ["Berlin"],
        )
        mit_schweiz = staedte.kandidaten(self.liste, kernraum, land, seen, ausland=frozenset())
        self.assertIn("Basel", [x.stadt.name for x in mit_schweiz])


# ─── Eingecheckte Daten ─────────────────────────────────────────────────────


class TestKarteUndKernraum(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.karte = geo.lade_karte()
        cls.kernraum = geo.lade_flaechen(GEOMETRIE / "kernraum.geojson")

    def test_karte_chron(self) -> None:
        self.assertEqual(self.karte.pruefe(), [])
        self.assertLessEqual(max(self.karte.bild_breite, self.karte.bild_hoehe), 4096)

    def test_kernraum_gueltig(self) -> None:
        self.assertEqual(geodaten.pruefe_kernraum(GEOMETRIE / "kernraum.geojson", self.karte), [])
        punkte = sum(len(r) - 1 for p in self.kernraum for r in p)
        self.assertGreaterEqual(punkte, 150)
        self.assertLessEqual(punkte, 450)

    def test_kernraum_passt_mit_rand_in_den_ausschnitt(self) -> None:
        punkte = [p for poly in self.kernraum for r in poly for p in r]
        rand = 0.2  # Grad
        self.assertGreaterEqual(min(x for x, _ in punkte) - self.karte.laenge_min, rand)
        self.assertGreaterEqual(self.karte.laenge_max - max(x for x, _ in punkte), rand)
        self.assertGreaterEqual(min(y for _, y in punkte) - self.karte.breite_min, rand)
        self.assertGreaterEqual(self.karte.breite_max - max(y for _, y in punkte), rand)

    def test_prueforte(self) -> None:
        falsch = [name for name, x, y, soll in geodaten.PRUEFORTE if geo.punkt_in_polygon(x, y, self.kernraum) != soll]
        self.assertEqual(falsch, [])

    def test_abschnitte(self) -> None:
        daten = geo.lade_geojson(GEOMETRIE / "kernraum.geojson")
        abschnitte = daten["features"][0]["properties"]["abschnitte"]
        anfaenge = [a[0] for a in abschnitte]
        self.assertEqual(anfaenge[0], 0)
        self.assertEqual(anfaenge, sorted(set(anfaenge)))
        self.assertLess(anfaenge[-1], sum(len(r) - 1 for p in self.kernraum for r in p))
        self.assertTrue(all(isinstance(a[1], str) and a[1] for a in abschnitte))

    def test_ueberschneidung_wird_erkannt(self) -> None:
        acht = [(0.0, 0.0), (2.0, 2.0), (2.0, 0.0), (0.0, 2.0), (0.0, 0.0)]
        self.assertEqual(geodaten.ring_ueberschneidungen(acht), [(0, 2)])
        with tempfile.TemporaryDirectory() as ordner:
            pfad = Path(ordner) / "k.geojson"
            pfad.write_text(
                json.dumps(
                    {
                        "type": "FeatureCollection",
                        "features": [
                            {
                                "type": "Feature",
                                "properties": {},
                                "geometry": {"type": "Polygon", "coordinates": [list(reversed(quadrat(0, 0, 1, 1)))]},
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            self.assertTrue(any("Umlaufrichtung" in f for f in geodaten.pruefe_kernraum(pfad)))


class TestErzeugteGeometrie(unittest.TestCase):
    """Die eingecheckten Dateien aus 'geodaten.py geometrie' (Aktualität prüft 'geodaten.py pruefen')."""

    def test_dateien(self) -> None:
        karte = geo.lade_karte()
        rahmen = karte.rahmen(geodaten.RAND_GRAD)
        gesamt = 0
        for name, arten in (
            ("land.geojson", {"Polygon"}),
            ("seen.geojson", {"Polygon", "MultiPolygon"}),
            ("fluesse.geojson", {"LineString", "MultiLineString"}),
        ):
            with self.subTest(name):
                pfad = GEOMETRIE / name
                text = pfad.read_text(encoding="utf-8")
                gesamt += len(text.encode("utf-8"))
                daten = json.loads(text)
                self.assertEqual(daten["type"], "FeatureCollection")
                self.assertEqual(daten["ausschnitt"], [round(v, geodaten.STELLEN) for v in rahmen])
                self.assertTrue(daten["features"])
                for feature in daten["features"]:
                    g = feature["geometry"]
                    self.assertIn(g["type"], arten)
                    for x, y in _alle_punkte(g["coordinates"]):
                        self.assertTrue(rahmen[0] <= x <= rahmen[2] and rahmen[1] <= y <= rahmen[3])
                        self.assertEqual(round(x, 4), x)
                        self.assertEqual(round(y, 4), y)
                    if "Polygon" in g["type"]:
                        teile = [g["coordinates"]] if g["type"] == "Polygon" else g["coordinates"]
                        for polygon in teile:
                            for nr, ring in enumerate(polygon):
                                self.assertEqual(ring[0], ring[-1])
                                self.assertEqual(geo.ring_flaeche([tuple(p) for p in ring]) > 0.0, nr == 0)
                    if name == "fluesse.geojson":
                        self.assertIn(feature["properties"]["rang"], range(1, 7))
        self.assertLess(gesamt, geodaten.MAX_GROESSE)


def _alle_punkte(koordinaten: list) -> list[tuple[float, float]]:
    if koordinaten and isinstance(koordinaten[0], (int, float)):
        return [(float(koordinaten[0]), float(koordinaten[1]))]
    return [p for teil in koordinaten for p in _alle_punkte(teil)]


if __name__ == "__main__":
    unittest.main()
