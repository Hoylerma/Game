"""Tests für den Kartengenerator (Fassung M3).

Aufruf (aus der Wurzel des Repositorys):

    python3 -m unittest discover -s tools/kartengenerator -v

Die Testdaten liegen in tools/kartengenerator/testdaten/: kleine Karte (204 × 200 Pixel) mit
Festland und Haff, Insel und Inselchen, See mit Seeinsel, Ausland mit Grenzsee, drei Flüssen und
acht Orten (einer ohne Daten). Dort liegen auch die erwarteten Ausgaben (provinzen.png,
fluesse.png, definition.csv); nach einer gewollten Änderung am Verfahren neu erzeugen mit:

    python3 tools/kartengenerator/kartengenerator.py --daten tools/kartengenerator/testdaten

Der Vergleich mit einer zweiten Python-Version (Determinismus) nutzt die Umgebungsvariable
KARTENGENERATOR_PYTHON_VERGLEICH oder sucht python3.10 … python3.13 mit numpy im Suchpfad.
"""

from __future__ import annotations

import contextlib
import io
import json
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from fractions import Fraction
from pathlib import Path

import numpy as np

import karte_png
import karte_raster as kr
import kartengenerator as kg

HIER = Path(__file__).resolve().parent
TESTDATEN = HIER / "testdaten"
SKRIPT = HIER / "kartengenerator.py"


def _starte(*argumente: str) -> tuple[int, str, str]:
    """Ruft kartengenerator.main auf und fängt die Ausgaben ab."""
    aus, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(aus), contextlib.redirect_stderr(err):
        code = kg.main(list(argumente))
    return code, aus.getvalue(), err.getvalue()


_FIXTURE: dict[str, object] = {}


def _fixture() -> tuple[kg.Grundlage, list[kg.Ort], kg.Ergebnis]:
    """Grundlage, Orte und Ergebnis der Testdaten (einmal berechnet, nur lesen)."""
    if not _FIXTURE:
        grundlage = kg.lade_grundlage(TESTDATEN)
        orte, warnungen = kg.lade_orte_daten(TESTDATEN)
        if grundlage.warnungen or warnungen:
            raise AssertionError("\n".join(grundlage.warnungen + warnungen))
        _FIXTURE.update(grundlage=grundlage, orte=orte, ergebnis=kg.erzeuge(grundlage, orte))
    return _FIXTURE["grundlage"], _FIXTURE["orte"], _FIXTURE["ergebnis"]  # type: ignore[return-value]


def _karte(breite: int = 16, hoehe: int = 16, radius: float = 25.0, **felder: float) -> kr.Karte:
    """Karte mit 1 Grad je Pixel (Pixelkoordinaten = Länge bzw. 16 − Breite, exakt)."""
    werte = dict(laenge_min=0.0, laenge_max=float(breite), breite_min=0.0, breite_max=float(hoehe))
    werte.update(felder)
    return kr.Karte(breite, hoehe, bezugsbreite=0.0, max_radius_km=radius, **werte)  # type: ignore[arg-type]


def _ring(*punkte: tuple[float, float]) -> np.ndarray:
    return np.array(punkte, dtype=np.float64)


@contextlib.contextmanager
def _ohne_rauschen():
    """Schaltet das Domain Warping vorübergehend ab (reines Voronoi-Diagramm)."""
    alt = kr.RAUSCHEN_OKTAVEN
    kr.RAUSCHEN_OKTAVEN = ()
    try:
        yield
    finally:
        kr.RAUSCHEN_OKTAVEN = alt


def _kopie_testdaten(ziel: Path) -> Path:
    daten = ziel / "daten"
    shutil.copytree(TESTDATEN, daten)
    return daten


# ─── Laden ──────────────────────────────────────────────────────────────────


class TestLaden(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def _schreibe(self, relativ: str, text: str) -> Path:
        pfad = self.tmp / relativ
        pfad.parent.mkdir(parents=True, exist_ok=True)
        pfad.write_text(text, encoding="utf-8")
        return pfad

    def _karte_text(self, **ersetzen: str) -> str:
        felder = {
            "bild_breite": "204",
            "bild_hoehe": "200",
            "laenge_min": "10.0",
            "laenge_max": "11.0",
            "breite_min": "52.0",
            "breite_max": "52.6",
            "bezugsbreite": "52.3",
            "max_radius_km": "16",
        }
        felder.update(ersetzen)
        zeilen = [f"    {k} = {v}" for k, v in felder.items() if v != ""]
        return "karte {\n" + "\n".join(zeilen) + "\n}\n"

    def _lade_karte(self, text: str) -> tuple[kg.Karte | None, kg.Meldungen]:
        pfad = self._schreibe("karte.chron", text)
        meldungen = kg.Meldungen()
        return kg.lade_karte(pfad, "karte.chron", meldungen), meldungen

    def test_fixture(self) -> None:
        grundlage, orte, _ = _fixture()
        self.assertEqual(
            [o.kennung for o in orte], ["hafen", "heide", "insel", "moor", "burg", "aue", "seedorf", "mark"]
        )
        karte = grundlage.karte
        self.assertEqual((karte.breite, karte.hoehe, karte.bezugsbreite, karte.max_radius_km), (204, 200, 52.3, 16.0))
        hafen, heide, insel, moor = orte[:4]
        self.assertEqual((hafen.laenge, hafen.breite, hafen.besitzer, hafen.kreis), (10.30, 52.40, "AAA", "kr_nord"))
        self.assertEqual(hafen.wo, "history/orte/a_nord.chron:3: ort hafen")
        self.assertTrue(hafen.hat_daten and heide.hat_daten)
        self.assertEqual(heide.besitzer, "AAA")  # ohne datierten Block
        self.assertEqual(insel.besitzer, "BBB")  # jüngster datierter Block
        self.assertFalse(moor.hat_daten)
        self.assertEqual((moor.besitzer, moor.kreis), ("", ""))
        self.assertEqual(orte[4].name, "Burg (Altmark)")

    def test_dateireihenfolge_nach_bytes(self) -> None:
        for name in ("b.chron", "B.chron", "a_z.chron", "a.chron"):
            self._schreibe(
                f"history/orte/{name}",
                f"ort o_{name[:-6].lower()}_{ord(name[0])} {{\n"
                f'    name = "{name}"\n    lage = {{ 10.5 52.3 }}\n}}\n',
            )
        self._schreibe("history/orte/x.txt", "kein chron")
        meldungen = kg.Meldungen()
        orte = kg.lade_orte(self.tmp / "history/orte", self.tmp, meldungen)
        self.assertEqual(meldungen.fehler, [])
        self.assertEqual([o.name for o in orte], ["B.chron", "a.chron", "a_z.chron", "b.chron"])

    def test_fehler_in_orten(self) -> None:
        self._schreibe(
            "history/orte/a.chron",
            'ort gut {\n    name = "Gut"\n    lage = { 10.5 52.3 }\n}\n'
            'ort Schlecht {\n    name = "X"\n    lage = { 10.5 52.3 }\n}\n'
            "ort ohne_name {\n    lage = { 10.5 52.3 }\n}\n"
            'ort ohne_lage {\n    name = "Y"\n}\n'
            'ort lage_kaputt {\n    name = "Z"\n    lage = { 10.5 }\n}\n'
            'ort lage_bereich {\n    name = "Z"\n    lage = { 190 52 }\n}\n'
            'ort semikolon {\n    name = "A;B"\n    lage = { 10.5 52.3 }\n}\n'
            'ort gut {\n    name = "Doppelt"\n    lage = { 10.5 52.3 }\n}\n'
            'ort kreis_kaputt {\n    name = "K"\n    lage = { 10.5 52.3 }\n    kreis = "Text"\n}\n'
            'ort zweimal {\n    name = "A"\n    name = "B"\n    lage = { 10.5 52.3 }\n}\n'
            "staat AAA { }\n",
        )
        meldungen = kg.Meldungen()
        orte = kg.lade_orte(self.tmp / "history/orte", self.tmp, meldungen)
        self.assertEqual([o.kennung for o in orte], ["gut"])
        text = "\n".join(meldungen.fehler)
        for erwartet in (
            "history/orte/a.chron:5: ungültige Ortskennung 'Schlecht'",
            "history/orte/a.chron:9: Pflichtfeld 'name' fehlt in ort ohne_name",
            "history/orte/a.chron:12: Pflichtfeld 'lage' fehlt in ort ohne_lage",
            "history/orte/a.chron:17: 'lage' erwartet genau zwei Zahlen",
            "history/orte/a.chron:21: 'lage' außerhalb des gültigen Bereichs",
            "history/orte/a.chron:24: 'name' darf kein ';'",
            "history/orte/a.chron:27: doppelte Kennung 'gut' (bereits definiert in history/orte/a.chron:1)",
            "history/orte/a.chron:34: 'kreis' erwartet eine Kreiskennung",
            "history/orte/a.chron:38: 'name' ist mehrfach angegeben (zuerst in Zeile 37)",
        ):
            self.assertIn(erwartet, text)
        self.assertIn(
            "history/orte/a.chron:41: unbekannter Eintrag 'staat' wird ignoriert", "\n".join(meldungen.warnungen)
        )

    def test_orte_ordner_fehlt(self) -> None:
        with self.assertRaises(kg.DatenFehler) as fehler:
            kg.lade_orte_daten(self.tmp)
        self.assertIn("history/orte: Ordner fehlt oder enthält keine .chron-Dateien", fehler.exception.meldungen[0])

    def test_syntaxfehler_mit_zeile(self) -> None:
        self._schreibe("history/orte/a.chron", 'ort a {\n    name = "A"\n    lage = { 10 52 \n')
        meldungen = kg.Meldungen()
        kg.lade_orte(self.tmp / "history/orte", self.tmp, meldungen)
        self.assertIn("history/orte/a.chron:4:1: nicht geschlossener Block (geöffnet in Zeile 3)", meldungen.fehler)

    def test_karte_gueltig(self) -> None:
        karte, meldungen = self._lade_karte(self._karte_text() + "# Kommentar\n")
        self.assertEqual(meldungen.fehler, [])
        self.assertEqual(meldungen.warnungen, [])
        self.assertEqual(karte, kr.Karte(204, 200, 10.0, 11.0, 52.0, 52.6, 52.3, 16.0))

    def test_karte_warnungen(self) -> None:
        text = self._karte_text().replace(
            "}\n", "    kueste = { 10.0 52.5 11.0 52.5 }\n    farbe = 3\n}\nnoch_was = 1\n"
        )
        karte, meldungen = self._lade_karte(text)
        self.assertIsNotNone(karte)
        warnungen = "\n".join(meldungen.warnungen)
        self.assertIn("karte.chron:10: 'kueste' entfällt seit M3", warnungen)
        self.assertIn("karte.chron:11: unbekanntes Feld 'farbe' in karte wird ignoriert", warnungen)
        self.assertIn("karte.chron:13: unbekannter Eintrag 'noch_was'", warnungen)

    def test_karte_fehler(self) -> None:
        faelle = [
            ({"bezugsbreite": ""}, "Pflichtfeld 'bezugsbreite' fehlt in karte"),
            ({"bild_breite": "4097", "bild_hoehe": "4016"}, "'bild_breite' muss zwischen 1 und 4096 liegen"),
            ({"bild_hoehe": "0"}, "'bild_hoehe' muss zwischen 1 und 4096 liegen"),
            ({"bild_breite": "20.5"}, "ungültige Ganzzahl '20.5' für 'bild_breite'"),
            ({"laenge_max": "9.0"}, "erwartet −180 ≤ laenge_min < laenge_max ≤ 180"),
            ({"breite_min": "53.0"}, "erwartet −90 ≤ breite_min < breite_max ≤ 90"),
            ({"bezugsbreite": "90"}, "'bezugsbreite' muss zwischen −90 und 90 liegen"),
            ({"max_radius_km": "0"}, "'max_radius_km' muss größer als 0 sein"),
            ({"max_radius_km": '"25"'}, "'max_radius_km' erwartet eine Zahl"),
            ({"bild_hoehe": "203"}, "Seitenverhältnis bild_breite/bild_hoehe weicht um 1.40 % vom Sollwert"),
        ]
        for ersetzen, erwartet in faelle:
            with self.subTest(ersetzen=ersetzen):
                karte, meldungen = self._lade_karte(self._karte_text(**ersetzen))
                self.assertIsNone(karte)
                self.assertIn(erwartet, "\n".join(meldungen.fehler))
        # Höchstens 1 % Abweichung ist erlaubt.
        karte, meldungen = self._lade_karte(self._karte_text(bild_hoehe="201"))
        self.assertIsNotNone(karte, meldungen.fehler)
        for text, erwartet in (
            ("", "Block 'karte { … }' fehlt"),
            ("karte = 3\n", "ungültige Definition, erwartet: karte { … }"),
            (self._karte_text() + self._karte_text(), "zweiter Block 'karte' (der erste steht in Zeile 1)"),
        ):
            karte, meldungen = self._lade_karte(text)
            self.assertIsNone(karte)
            self.assertIn(erwartet, "\n".join(meldungen.fehler))
        meldungen = kg.Meldungen()
        self.assertIsNone(kg.lade_karte(self.tmp / "fehlt.chron", "karte/karte.chron", meldungen))
        self.assertEqual(meldungen.fehler, ["karte/karte.chron: Datei fehlt"])

    def test_seitenverhaeltnis(self) -> None:
        karte = kr.Karte(4096, 3329, 1.8, 23.5, 45.1, 56.2, 51.0, 25.0)
        self.assertLess(kg.seitenverhaeltnis_abweichung(karte), 1e-4)

    def test_geojson(self) -> None:
        gut = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "Polygon",
                        "coordinates": [
                            [[0, 0], [1, 0], [1, 1], [0, 0]],
                            [[0.2, 0.1], [0.3, 0.1], [0.3, 0.2], [0.2, 0.1]],
                        ],
                    },
                },
                {
                    "type": "Feature",
                    "properties": {},
                    "geometry": {
                        "type": "MultiPolygon",
                        "coordinates": [[[[2, 2], [3, 2], [3, 3], [2, 2]]], [[[4, 4], [5, 4], [5, 5, 100], [4, 4]]]],
                    },
                },
            ],
        }
        self._schreibe("gut.geojson", json.dumps(gut))
        meldungen = kg.Meldungen()
        polygone = kg.lade_flaechen(self.tmp / "gut.geojson", "gut.geojson", meldungen)
        self.assertEqual(meldungen.fehler, [])
        self.assertEqual([len(p) for p in polygone], [2, 1, 1])
        self.assertEqual(polygone[2][0].tolist(), [[4, 4], [5, 4], [5, 5], [4, 4]])

        faelle = [
            ("{", "kein gültiges GeoJSON"),
            ('{"type": "Feature"}', "erwartet eine FeatureCollection"),
            ('{"type": "FeatureCollection"}', "'features' fehlt oder ist keine Liste"),
            (
                '{"type": "FeatureCollection", "features": [{"geometry": {"type": "Point", "coordinates": [1, 2]}}]}',
                "Feature 1: Geometrietyp 'Point' nicht erlaubt",
            ),
            (
                '{"type": "FeatureCollection", "features": [{"geometry": {"type": "Polygon", '
                '"coordinates": [[[0, 0], [1, 0], [0, 0]]]}}]}',
                "Feature 1: erwartet eine Liste mit mindestens 4",
            ),
            (
                '{"type": "FeatureCollection", "features": [{"geometry": {"type": "Polygon", '
                '"coordinates": [[[0, 0], [1, "a"], [1, 1], [0, 0]]]}}]}',
                "Feature 1: ungültige Position",
            ),
            (
                '{"type": "FeatureCollection", "features": [{"geometry": {"type": "Polygon", '
                '"coordinates": [[[0, 0], [1, 95], [1, 1], [0, 0]]]}}]}',
                "Position außerhalb des gültigen Bereichs",
            ),
        ]
        for text, erwartet in faelle:
            with self.subTest(text=text):
                self._schreibe("x.geojson", text)
                meldungen = kg.Meldungen()
                kg.lade_flaechen(self.tmp / "x.geojson", "x.geojson", meldungen)
                self.assertIn(erwartet, "\n".join(meldungen.fehler))
        meldungen = kg.Meldungen()
        kg.lade_flaechen(self.tmp / "fehlt.geojson", "karte/geometrie/land.geojson", meldungen)
        self.assertIn("karte/geometrie/land.geojson: Datei fehlt", meldungen.fehler[0])

    def test_fluesse_laden(self) -> None:
        daten = {
            "type": "FeatureCollection",
            "features": [
                {
                    "type": "Feature",
                    "properties": {"rang": 1},
                    "geometry": {"type": "LineString", "coordinates": [[0, 0], [1, 1]]},
                },
                {
                    "type": "Feature",
                    "properties": {"rang": 9},
                    "geometry": {"type": "MultiLineString", "coordinates": [[[0, 0], [1, 1]], [[2, 2], [3, 3]]]},
                },
                {"type": "Feature", "properties": {}, "geometry": {"type": "Polygon", "coordinates": []}},
            ],
        }
        self._schreibe("f.geojson", json.dumps(daten))
        meldungen = kg.Meldungen()
        linien = kg.lade_linien(self.tmp / "f.geojson", "f.geojson", meldungen)
        self.assertEqual([rang for rang, _ in linien], [1, 6, 6])
        self.assertIn("Feature 2: 'rang' fehlt oder ist ungültig (9), nehme 6", "\n".join(meldungen.warnungen))
        self.assertIn("Feature 3: Geometrietyp 'Polygon' nicht erlaubt", "\n".join(meldungen.fehler))

    def test_staaten_fuer_vorschau(self) -> None:
        self._schreibe(
            "history/staaten/a.chron",
            'staat AAA {\n    name = "A"\n    farbe = { 1 2 3 }\n}\n'
            'staat BBB {\n    name = "B"\n    farbe = { 1 2 }\n}\n',
        )
        meldungen = kg.Meldungen()
        staaten = kg.lade_staaten(self.tmp / "history/staaten", self.tmp, meldungen)
        self.assertEqual(list(staaten), ["AAA"])
        self.assertEqual(staaten["AAA"].farbe, (1, 2, 3))
        self.assertIn("staat BBB: 'farbe' fehlt oder ist ungültig (Vorschau)", meldungen.warnungen[0])

    def test_echte_karte_und_geometrie(self) -> None:
        daten = kg.REPO / "data"
        if not (daten / "karte" / "geometrie").is_dir():
            self.skipTest("data/karte/geometrie fehlt")
        meldungen = kg.Meldungen()
        karte = kg.lade_karte(daten / "karte/karte.chron", "karte/karte.chron", meldungen)
        geometrie = kg.lade_geometrie(daten / "karte/geometrie", daten, meldungen)
        self.assertEqual(meldungen.fehler, [])
        self.assertEqual(meldungen.warnungen, [])
        self.assertIsNotNone(karte)
        self.assertTrue(geometrie.land and geometrie.seen and geometrie.kernraum and geometrie.fluesse)


# ─── Geometrie ──────────────────────────────────────────────────────────────


def _auf_rand(px: Fraction, py: Fraction, ringe: list[list[tuple[Fraction, Fraction]]]) -> bool:
    for ring in ringe:
        for (ax, ay), (bx, by) in zip(ring, ring[1:]):
            if (
                (bx - ax) * (py - ay) - (by - ay) * (px - ax) == 0
                and min(ax, bx) <= px <= max(ax, bx)
                and min(ay, by) <= py <= max(ay, by)
            ):
                return True
    return False


def _innen(px: Fraction, py: Fraction, ringe: list[list[tuple[Fraction, Fraction]]]) -> bool:
    innen = False
    for ring in ringe:
        for (ax, ay), (bx, by) in zip(ring, ring[1:]):
            if (ay > py) != (by > py):
                xs = ax + (py - ay) * (bx - ax) / (by - ay)
                if px < xs:
                    innen = not innen
    return innen


def _raster_referenz(polygone: list[list[np.ndarray]], karte: kr.Karte) -> np.ndarray:
    """Exakte Referenz (Brüche): Pixelmitte strikt innen (Gerade-ungerade je Polygon), Vereinigung."""
    maske = np.zeros((karte.hoehe, karte.breite), dtype=bool)
    exakt = []
    for polygon in polygone:
        ringe = []
        for ring in polygon:
            punkte = [(Fraction(float(x)), Fraction(karte.hoehe) - Fraction(float(y))) for x, y in ring]
            if punkte[0] != punkte[-1]:
                punkte.append(punkte[0])
            ringe.append(punkte)
        exakt.append(ringe)
    for y in range(karte.hoehe):
        for x in range(karte.breite):
            px, py = Fraction(2 * x + 1, 2), Fraction(2 * y + 1, 2)
            maske[y, x] = any(not _auf_rand(px, py, r) and _innen(px, py, r) for r in exakt)
    return maske


class TestGeometrie(unittest.TestCase):
    def test_cos_grad(self) -> None:
        for grad in (-720.0, -180.0, -95.5, -45.0, 0.0, 10.0, 45.1, 51.0, 56.2, 60.0, 89.9, 90.0, 135.0, 180.0, 359.0):
            self.assertAlmostEqual(kr.cos_grad(grad), math.cos(math.radians(grad)), delta=2e-16)
        self.assertEqual(kr.cos_grad(0.0), 1.0)
        self.assertEqual(kr.cos_grad(51.0), 0.6293203910498375)

    def test_projektion(self) -> None:
        karte = kr.Karte(4096, 3329, 1.8, 23.5, 45.1, 56.2, 51.0, 25.0)
        x, y = karte.nach_pixel(np.array([1.8, 23.5, 13.4]), np.array([56.2, 45.1, 52.52]))
        self.assertEqual(x[0], 0.0)
        self.assertEqual(x[1], 4096.0)
        self.assertEqual(y[0], 0.0)
        self.assertEqual(y[1], 3329.0)
        self.assertAlmostEqual(x[2], (13.4 - 1.8) / 21.7 * 4096)
        laenge, breite = karte.pixelmitte(0, 0)
        self.assertAlmostEqual(laenge, 1.8 + 0.5 * 21.7 / 4096)
        self.assertAlmostEqual(breite, 56.2 - 0.5 * 11.1 / 3329)

    def test_rasterung_rand_zaehlt_aussen(self) -> None:
        karte = _karte(8, 8)
        # Rechteck mit Kanten genau auf Pixelmitten (x = 1,5 … 4,5, y = 1,5 … 3,5 in Pixeln).
        quadrat = [_ring((1.5, 6.5), (4.5, 6.5), (4.5, 4.5), (1.5, 4.5), (1.5, 6.5))]
        maske = kr.rastere_polygone([quadrat], karte)
        erwartet = np.zeros((8, 8), dtype=bool)
        erwartet[2, 2:4] = True  # nur Mitten strikt innen
        np.testing.assert_array_equal(maske, erwartet)
        # Gleiches Rechteck ohne Schlusspunkt (Ring wird geschlossen).
        offen = [_ring((1.5, 6.5), (4.5, 6.5), (4.5, 4.5), (1.5, 4.5))]
        np.testing.assert_array_equal(kr.rastere_polygone([offen], karte), erwartet)

    def test_rasterung_loch_und_multipolygon(self) -> None:
        karte = _karte(10, 10)
        aussen = _ring((1, 1), (9, 1), (9, 9), (1, 9), (1, 1))
        loch = _ring((3, 3), (3, 7), (7, 7), (7, 3), (3, 3))
        insel = _ring((4, 4), (6, 4), (6, 6), (4, 6), (4, 4))
        maske = kr.rastere_polygone([[aussen, loch], [insel]], karte)
        erwartet = np.zeros((10, 10), dtype=bool)
        erwartet[1:9, 1:9] = True
        erwartet[3:7, 3:7] = False
        erwartet[4:6, 4:6] = True
        np.testing.assert_array_equal(maske, erwartet)

    def test_rasterung_gemeinsame_kante(self) -> None:
        karte = kr.Karte(50, 50, 10.0, 11.0, 52.0, 52.5, 52.25, 10.0)
        links = [_ring((10.1, 52.1), (10.537, 52.08), (10.43, 52.41), (10.1, 52.4), (10.1, 52.1))]
        rechts = [_ring((10.537, 52.08), (10.9, 52.1), (10.9, 52.4), (10.43, 52.41), (10.537, 52.08))]
        a = kr.rastere_polygone([links], karte)
        b = kr.rastere_polygone([rechts], karte)
        beide = kr.rastere_polygone([links, rechts], karte)
        self.assertFalse((a & b).any())
        np.testing.assert_array_equal(beide, a | b)

    def test_rasterung_gegen_exakte_referenz(self) -> None:
        rng = np.random.default_rng(7)
        karte = _karte(16, 16)
        for fall in range(120):
            polygone = []
            for _ in range(int(rng.integers(1, 3))):
                ringe = []
                for _ in range(int(rng.integers(1, 3))):
                    n = int(rng.integers(3, 9))
                    punkte = rng.integers(-4, 37, size=(n, 2)) / 2.0  # Halbe: Mitten auf Kanten und Ecken
                    ringe.append(np.vstack([punkte, punkte[:1]]))
                polygone.append(ringe)
            with self.subTest(fall=fall):
                np.testing.assert_array_equal(kr.rastere_polygone(polygone, karte), _raster_referenz(polygone, karte))

    def test_flaechenarten_der_fixture(self) -> None:
        grundlage, _, _ = _fixture()
        karte, art = grundlage.karte, grundlage.art

        def bei(laenge: float, breite: float) -> int:
            x, y = karte.nach_pixel(np.array([laenge]), np.array([breite]))
            return int(art[int(y[0]), int(x[0])])

        self.assertEqual(bei(10.3, 52.55), kr.MEER)  # Ostsee
        self.assertEqual(bei(10.64, 52.53), kr.KERNLAND)  # Insel
        self.assertEqual(bei(10.215, 52.527), kr.KERNLAND)  # Inselchen
        self.assertEqual(bei(10.18, 52.19), kr.MEER)  # Haff (Loch im Festland)
        self.assertEqual(bei(10.44, 52.25), kr.BINNENGEWAESSER)  # Testsee
        self.assertEqual(bei(10.485, 52.272), kr.KERNLAND)  # Seeinsel (Loch im See)
        self.assertEqual(bei(10.95, 52.3), kr.AUSLAND)
        self.assertEqual(bei(10.9, 52.17), kr.BINNENGEWAESSER)  # Grenzsee im Ausland
        self.assertEqual(bei(10.3, 52.2), kr.KERNLAND)
        self.assertEqual(set(np.unique(art).tolist()), {0, 1, 2, 3})


# ─── Rauschen ───────────────────────────────────────────────────────────────


class TestRauschen(unittest.TestCase):
    def test_raster_gleich_punktauswertung(self) -> None:
        xs = np.arange(-30, 300, dtype=np.float64) * 0.37 + 0.123
        ys = np.arange(-10, 250, dtype=np.float64) * 0.41 - 3.0
        gx, gy = kr.verschiebung_raster(xs, ys)
        px, py = kr.verschiebung_punkte(xs[None, :], ys[:, None])
        np.testing.assert_array_equal(gx, px)
        np.testing.assert_array_equal(gy, py)

    def test_beschraenkt_und_deterministisch(self) -> None:
        rng = np.random.default_rng(3)
        xs = rng.uniform(-500, 2000, 20000)
        ys = rng.uniform(-500, 2000, 20000)
        dx, dy = kr.verschiebung_punkte(xs, ys)
        grenze = kr.rauschamplitude()
        self.assertLessEqual(float(np.abs(dx).max()), grenze)
        self.assertLessEqual(float(np.abs(dy).max()), grenze)
        self.assertGreater(float(np.abs(dx).max()), grenze / 3)
        dx2, dy2 = kr.verschiebung_punkte(xs, ys)
        np.testing.assert_array_equal(dx, dx2)
        # Fester Wert: ändert sich nur bei gewollter Änderung von Hash, Startwert oder Oktaven.
        self.assertEqual(
            kr._gitterwerte(np.array([0, 1, -1]), np.array([0, 2, 5]), 1648).tolist(),
            [
                kr._gitterwerte(np.array([0]), np.array([0]), 1648)[0],
                kr._gitterwerte(np.array([1]), np.array([2]), 1648)[0],
                kr._gitterwerte(np.array([-1]), np.array([5]), 1648)[0],
            ],
        )
        werte = kr._gitterwerte(np.arange(-1000, 1000), np.arange(2000), 99)
        self.assertTrue(((werte >= -1.0) & (werte < 1.0)).all())
        self.assertAlmostEqual(float(werte.mean()), 0.0, delta=0.05)

    def test_verschiebung_der_pixel(self) -> None:
        grundlage, _, _ = _fixture()
        raster = grundlage.raster
        self.assertIsNotNone(raster.px_km)
        ohne = raster.spalte_grad[None, :] * raster.kx[:, None]
        verschiebung = np.abs(raster.px_km - ohne).max()
        self.assertLessEqual(float(verschiebung), kr.rauschamplitude() + 1e-9)
        self.assertGreater(float(verschiebung), 0.5)


# ─── Zuordnung ──────────────────────────────────────────────────────────────


class TestZuordnung(unittest.TestCase):
    def _zuordnen(self, grundlage: kg.Grundlage, orte: list[kg.Ort], **kw: bool) -> np.ndarray:
        laengen = np.array([o.laenge for o in orte])
        breiten = np.array([o.breite for o in orte])
        lage = kr.lagen(grundlage.raster, laengen, breiten)
        return kr.zuordnen(grundlage.raster, laengen, breiten, lage, **kw)

    def test_rechteck_gleich_vollsuche(self) -> None:
        grundlage, orte, _ = _fixture()
        mehr = orte + [
            kg.Ort("aussen", "Außen", 9.9, 52.3),  # außerhalb des Bildes, erreicht den Rand
            kg.Ort("nah", "Nah", 10.301, 52.401),  # fast im selben Pixel wie hafen
        ]
        np.testing.assert_array_equal(self._zuordnen(grundlage, mehr), self._zuordnen(grundlage, mehr, vollsuche=True))

    def test_ohne_rauschen_naechster_ort(self) -> None:
        with _ohne_rauschen():
            grundlage = kg.lade_grundlage(TESTDATEN, fluesse=False)
            _, orte, _ = _fixture()
            werte = self._zuordnen(grundlage, orte)
        karte = grundlage.karte
        lagen = kr.lagen(grundlage.raster, np.array([o.laenge for o in orte]), np.array([o.breite for o in orte]))
        km_y = (karte.breite_max - karte.breite_min) / karte.hoehe * kr.KM_PRO_GRAD
        km_x_grad = (karte.laenge_max - karte.laenge_min) / karte.breite
        rng = np.random.default_rng(5)
        for _ in range(400):
            x, y = int(rng.integers(0, karte.breite)), int(rng.integers(0, karte.hoehe))
            if grundlage.art[y, x] != kr.KERNLAND:
                self.assertEqual(werte[y, x], 0)
                continue
            kx = kr.KM_PRO_GRAD * kr.cos_grad(karte.pixelmitte(x, y)[1])
            abstaende = [
                math.hypot((x - int(lagen.px[k])) * km_x_grad * kx, (y - int(lagen.py[k])) * km_y)
                for k in range(len(orte))
            ]
            bester = min(range(len(orte)), key=lambda k: (abstaende[k], k))
            erwartet = bester + 1 if abstaende[bester] <= karte.max_radius_km else 0
            if abs(sorted(abstaende)[1] - abstaende[bester]) < 1e-9 or abs(abstaende[bester] - 16.0) < 1e-6:
                continue  # Gleichstand bis auf Rundung: nicht aussagekräftig
            self.assertEqual(int(werte[y, x]), erwartet, (x, y, abstaende))

    def test_gleichstand_kleinerer_index(self) -> None:
        grundlage, _, _ = _fixture()
        zwei = [kg.Ort("a", "A", 10.3, 52.2), kg.Ort("b", "B", 10.3, 52.2)]
        werte = self._zuordnen(grundlage, zwei)
        self.assertTrue((werte != 2).all())
        self.assertGreater(int((werte == 1).sum()), 1000)

    def test_eigenschaften_der_fixture(self) -> None:
        grundlage, orte, ergebnis = _fixture()
        werte = ergebnis.werte
        # Nur Kernraum-Land trägt Orte; Kodierung mit Flächenart.
        self.assertTrue((werte[grundlage.art != kr.KERNLAND] == 0).all())
        bild = ergebnis.provinzen()
        np.testing.assert_array_equal(bild[..., 2], grundlage.art)
        np.testing.assert_array_equal(bild[..., 0].astype(np.int32) + 256 * bild[..., 1].astype(np.int32), werte)
        # Genau eine zusammenhängende Fläche je Ort, Lagepixel gehört dem Ort.
        lf = kr.laeufe(werte)
        komp = kr.komponenten(werte, lf, lf.wert > 0)
        _, kwert, _ = kr._komponenten_liste(lf, komp)
        self.assertEqual(sorted(kwert.tolist()), list(range(1, len(orte) + 1)))
        lagen = kr.lagen(grundlage.raster, np.array([o.laenge for o in orte]), np.array([o.breite for o in orte]))
        for k in range(len(orte)):
            self.assertEqual(int(werte[lagen.py[k], lagen.px[k]]), k + 1, orte[k].kennung)
        # Die Insel gehört ganz der Inselstadt, Inselchen und Seeinsel keinem Ort.
        insel = ergebnis.flaeche_nach_ort["insel"]
        self.assertEqual(insel.pixel, 869)
        self.assertEqual(len(ergebnis.ohne_ort), 5)
        self.assertEqual(ergebnis.bereinigung.entfernt, 2)
        warnungen = "\n".join(ergebnis.warnungen)
        self.assertIn("Kernraum-Land ohne Ort: 702 Pixel (≈ 78 km²) in 5 Teilflächen", warnungen)
        self.assertIn("Kernraum-Land ohne Ort: 35 Pixel (≈ 3.9 km²) ab Pixel (41, 22) bei 10.20 °O 52.53 °N", warnungen)
        self.assertIn("ab Pixel (96, 107) bei 10.47 °O 52.28 °N", warnungen)  # Seeinsel

    def test_warnungen(self) -> None:
        grundlage, orte, _ = _fixture()
        test = orte + [
            kg.Ort("meer", "Meer", 10.3, 52.55, datei="t.chron", zeile=1),
            kg.Ort("see", "See", 10.44, 52.25, datei="t.chron", zeile=2),
            kg.Ort("ausland", "Ausland", 10.95, 52.3, datei="t.chron", zeile=3),
            kg.Ort("fern", "Fern", 30.0, 60.0, datei="t.chron", zeile=4),
            kg.Ort("doppelt", "Doppelt", 10.3001, 52.4001, datei="t.chron", zeile=5),
        ]
        ergebnis = kg.erzeuge(grundlage, test)
        text = "\n".join(ergebnis.warnungen)
        for erwartet in (
            "t.chron:1: ort meer: Lage 10.30 52.55 liegt im Meer, nicht auf Kernraum-Land",
            "t.chron:2: ort see: Lage 10.44 52.25 liegt in einem Binnengewässer, nicht auf Kernraum-Land",
            "t.chron:3: ort ausland: Lage 10.95 52.30 liegt im Ausland (außerhalb des Kernraums)",
            "t.chron:4: ort fern: Lage 30.00 60.00 liegt außerhalb des Kartenausschnitts",
            "t.chron:4: ort fern: erhält keine Pixel",
            "t.chron:5: ort doppelt: Lage im selben Kartenpixel (61, 66) wie ort hafen; das Pixel gehört ort hafen",
            "t.chron:5: ort doppelt: erhält keine Pixel",  # Gleichstand überall: hafen gewinnt
            "t.chron:1: ort meer: erhält nur 35 Pixel (≈ 4 km², Median aller Orte: ",  # nur das Inselchen
        ):
            self.assertIn(erwartet, text)
        fern = ergebnis.flaeche_nach_ort["fern"]
        self.assertEqual((fern.x, fern.y, fern.pixel, fern.flaeche_km2), (-1, -1, 0, 0))
        self.assertEqual((fern.laenge, fern.breite), (30.0, 60.0))

    def test_max_radius(self) -> None:
        grundlage, _, _ = _fixture()
        ergebnis = kg.erzeuge(grundlage, [kg.Ort("einsam", "Einsam", 10.3, 52.2)])
        karte = grundlage.karte
        flaeche = ergebnis.flaechen[0]
        # Höchstens ein Kreis mit max_radius_km plus Verschiebung; deutlich weniger als das ganze Land.
        obergrenze = math.pi * (karte.max_radius_km + kr.rauschamplitude()) ** 2
        self.assertLess(flaeche.flaeche_km2, obergrenze)
        self.assertGreater(flaeche.flaeche_km2, 0.5 * math.pi * karte.max_radius_km**2)
        self.assertTrue(any("Kernraum-Land ohne Ort" in w for w in ergebnis.warnungen))

    def test_api_prueft_orte(self) -> None:
        grundlage, _, _ = _fixture()
        with self.assertRaises(kg.DatenFehler) as fehler:
            kg.erzeuge(
                grundlage,
                [kg.Ort("a", "A;B", 10.3, 52.2), kg.Ort("a", "A", 10.3, 52.2), kg.Ort("B", "B", 10.3, 99.0)],
            )
        self.assertEqual(
            fehler.exception.meldungen,
            [
                "ort a: 'name' darf kein ';' und keinen Zeilenumbruch enthalten (definition.csv)",
                "ort a: ungültige oder doppelte Kennung",
                "ort B: ungültige oder doppelte Kennung",
                "ort B: 'lage' außerhalb des gültigen Bereichs",
            ],
        )

    def test_keine_orte(self) -> None:
        grundlage, _, _ = _fixture()
        ergebnis = kg.erzeuge(grundlage, [])
        self.assertEqual(ergebnis.flaechen, [])
        self.assertTrue((ergebnis.werte == 0).all())
        self.assertEqual(kg.definition_csv(ergebnis.flaechen), (kg.CSV_KOPF + "\n").encode())


# ─── Bereinigung ────────────────────────────────────────────────────────────


def _bild(text: str) -> np.ndarray:
    return np.array([[int(c) for c in zeile.strip()] for zeile in text.strip().splitlines()], dtype=np.int32)


class TestBereinigung(unittest.TestCase):
    def test_eingeschlossene_teilflaeche(self) -> None:
        werte = _bild("""
            11111
            12111
            11122
            11122
        """)
        neu, stat = kr.bereinigen(werte.copy(), {2: (4, 3)}, 2)
        np.testing.assert_array_equal(neu, _bild("11111\n11111\n11122\n11122"))
        self.assertEqual((stat.umverteilt, stat.entfernt), (1, 0))

    def test_nur_von_meer_umgeben(self) -> None:
        werte = _bild("""
            1100
            1100
            0001
        """)
        neu, stat = kr.bereinigen(werte.copy(), {1: (0, 0)}, 1)
        np.testing.assert_array_equal(neu, _bild("1100\n1100\n0000"))
        self.assertEqual((stat.entfernt, stat.entfernt_pixel), (1, 1))

    def test_lagepixel_bestimmt_hauptflaeche(self) -> None:
        werte = _bild("""
            11122
            11122
            22222
        """)
        # Ort 2 hat die größere Fläche unten, sein Lagepixel liegt aber oben rechts.
        werte[2, :] = 2
        werte[0, 3:] = 2
        werte[1, 3:] = 3
        neu, _ = kr.bereinigen(werte.copy(), {2: (4, 0), 1: (0, 0), 3: (3, 1)}, 3)
        self.assertEqual(neu[0, 3:].tolist(), [2, 2])
        # Die größere untere Fläche von 2 geht an den Nachbarn mit der längsten Grenze (1: 3 Kanten, 3: 2).
        self.assertEqual(neu[2].tolist(), [1, 1, 1, 1, 1])

    def test_groesste_ohne_lagepixel(self) -> None:
        werte = _bild("""
            1121
            1111
            2222
        """)
        neu, _ = kr.bereinigen(werte.copy(), {}, 2)
        self.assertEqual(int(neu[0, 2]), 1)
        self.assertTrue((neu[2] == 2).all())

    def test_diagonal_ist_nicht_verbunden(self) -> None:
        werte = _bild("""
            1200
            2100
            0000
        """)
        neu, stat = kr.bereinigen(werte.copy(), {1: (0, 0), 2: (1, 0)}, 2)
        # Jede Diagonal-Hälfte grenzt nur an die Hauptfläche des anderen Ortes.
        np.testing.assert_array_equal(neu, _bild("1200\n1200\n0000"))
        self.assertEqual(stat.umverteilt, 2)

    def test_gleichstand_kleinerer_kartenindex(self) -> None:
        werte = _bild("""
            111
            131
            222
        """)
        werte[1, 2] = 2
        werte[1, 0] = 1
        # Teilfläche (Wert 3 ohne Lage, zweites Stück) grenzt gleich lang an 1 und 2.
        werte = _bild("""
            1111
            1332
            2222
        """)
        werte[1, 3] = 2
        neu, _ = kr.bereinigen(werte.copy(), {1: (0, 0), 2: (0, 2), 3: (1, 1)}, 3)
        self.assertEqual(int(neu[1, 1]), 3)
        werte2 = _bild("""
            1111
            1342
            2222
        """)
        # Ort 3 hat nur ein Pixel ohne Lagepixel-Angabe: Hauptfläche. Ort 4 ebenso.
        neu2, stat = kr.bereinigen(werte2.copy(), {}, 4)
        self.assertEqual(stat.umverteilt, 0)
        np.testing.assert_array_equal(neu2, werte2)

    def test_kette_ueber_mehrere_runden(self) -> None:
        # Teilfläche von 3 grenzt nur an eine Teilfläche von 4, diese an die Hauptfläche von 1.
        werte = _bild("""
            0000000
            0344100
            0000100
            0333000
            0000000
            0444440
        """)
        neu, stat = kr.bereinigen(werte.copy(), {1: (5, 1), 3: (1, 3), 4: (1, 5)}, 4)
        self.assertEqual(neu[1, 1:5].tolist(), [1, 1, 1, 1])
        self.assertGreaterEqual(stat.runden, 3)
        self.assertEqual(stat.umverteilt, 2)

    def test_teilflaechen(self) -> None:
        maske = _bild("""
            1100
            0001
            1101
        """).astype(bool)
        self.assertEqual(kr.teilflaechen(maske), [(2, 0, 0), (2, 3, 1), (2, 0, 2)])
        self.assertEqual(kr.teilflaechen(np.zeros((3, 3), dtype=bool)), [])


# ─── Beschriftungspunkt und Flächen ─────────────────────────────────────────


def _chamfer_referenz(marken: np.ndarray) -> np.ndarray:
    """Freier Chamfer-3-4-Abstand zum nächsten Pixel mit anderer Marke (Bildrand = andere Marke)."""
    h, b = marken.shape
    andere = [(x, y, None) for x in range(-1, b + 1) for y in (-1, h)] + [
        (x, y, None) for y in range(h) for x in (-1, b)
    ]
    andere += [(x, y, int(marken[y, x])) for y in range(h) for x in range(b)]
    ergebnis = np.zeros((h, b), dtype=np.int64)
    for y in range(h):
        for x in range(b):
            m = int(marken[y, x])
            beste = None
            for qx, qy, qm in andere:
                if qm == m:
                    continue
                dx, dy = abs(qx - x), abs(qy - y)
                d = 4 * min(dx, dy) + 3 * (max(dx, dy) - min(dx, dy))
                beste = d if beste is None or d < beste else beste
            ergebnis[y, x] = beste
    return ergebnis


class TestBeschriftung(unittest.TestCase):
    def test_chamfer_gegen_referenz(self) -> None:
        rng = np.random.default_rng(11)
        for fall in range(12):
            h, b = int(rng.integers(1, 14)), int(rng.integers(1, 17))
            grob = rng.integers(0, 3, size=(h // 3 + 1, b // 3 + 1))
            marken = np.repeat(np.repeat(grob, 3, axis=0), 3, axis=1)[:h, :b].astype(np.int32)
            marken[rng.random((h, b)) < 0.1] = 7
            with self.subTest(fall=fall):
                np.testing.assert_array_equal(kr.chamfer_abstand(marken), _chamfer_referenz(marken))

    def test_rechteck_und_gleichstand(self) -> None:
        werte = np.zeros((11, 13), dtype=np.int32)
        werte[1:10, 2:11] = 1  # 9 × 9: Mitte eindeutig
        werte[0, 0] = 2
        x, y, d = kr.beschriftungspunkte(werte, 3)
        self.assertEqual((int(x[0]), int(y[0]), int(d[0])), (6, 5, 15))
        self.assertEqual((int(x[1]), int(y[1])), (0, 0))
        self.assertEqual((int(x[2]), int(y[2]), int(d[2])), (-1, -1, 0))  # Ort ohne Pixel
        werte = np.zeros((6, 8), dtype=np.int32)
        werte[1:5, 1:7] = 1  # 6 × 4: Gleichstand, das erste in Zeilenreihenfolge
        x, y, _ = kr.beschriftungspunkte(werte, 1)
        self.assertEqual((int(x[0]), int(y[0])), (2, 2))

    def test_beschriftung_im_ort(self) -> None:
        _, _, ergebnis = _fixture()
        for f in ergebnis.flaechen:
            self.assertGreaterEqual(f.x, 0)
            self.assertEqual(int(ergebnis.werte[f.y, f.x]), f.kartenindex + 1, f.ort)

    def test_flaechen(self) -> None:
        grundlage, _, ergebnis = _fixture()
        karte = grundlage.karte
        for f in ergebnis.flaechen:
            ys, _ = np.nonzero(ergebnis.werte == f.kartenindex + 1)
            self.assertEqual(len(ys), f.pixel)
            summe = 0.0
            for y in ys:
                breite = karte.pixelmitte(0, int(y))[1]
                summe += (
                    (karte.laenge_max - karte.laenge_min)
                    / karte.breite
                    * kr.KM_PRO_GRAD
                    * math.cos(math.radians(breite))
                ) * ((karte.breite_max - karte.breite_min) / karte.hoehe * kr.KM_PRO_GRAD)
            self.assertLessEqual(abs(summe - f.flaeche_km2), 0.5 + 1e-6, f.ort)

    def test_flaeche_rundung_festkomma(self) -> None:
        karte = kr.Karte(4, 2, 0.0, 4.0, 0.0, 2.0, 1.0, 10.0)
        raster = kr.Raster.erzeuge(karte, [], [], [])
        werte = np.array([[1, 1, 2, 0], [1, 1, 2, 2]], dtype=np.int32)
        pixel, flaechen = kr.pixel_und_flaechen(raster, werte, 3)
        self.assertEqual(pixel.tolist(), [4, 3, 0])
        zeile = [kr.KM_PRO_GRAD * math.cos(math.radians(b)) * kr.KM_PRO_GRAD for b in (1.5, 0.5)]
        self.assertEqual(flaechen, [round(2 * zeile[0] + 2 * zeile[1]), round(zeile[0] + 2 * zeile[1]), 0])


# ─── Flüsse ─────────────────────────────────────────────────────────────────


class TestFluesse(unittest.TestCase):
    def test_breite_und_kantenglaettung(self) -> None:
        karte = _karte(20, 12)
        # Waagrechte Linie in Pixelzeile 5 (Mitte y = 5,5 → Breite 12 − 5,5 = 6,5).
        linie = _ring((3.0, 6.5), (15.0, 6.5))
        breit = kr.zeichne_fluesse([(1, linie)], karte)  # Breite 3: Abstand 0 und 1 voll
        self.assertEqual(breit[:, 9].tolist(), [0, 0, 0, 0, 255, 255, 255, 0, 0, 0, 0, 0])
        schmal = kr.zeichne_fluesse([(6, linie)], karte)  # Breite 1: nur Abstand 0
        self.assertEqual(schmal[:, 9].tolist(), [0, 0, 0, 0, 0, 255, 0, 0, 0, 0, 0, 0])
        # Abstand 0,5 bei Breite 1 → Deckung 0,5 → 128 (kaufmännisch gerundet).
        halb = kr.zeichne_fluesse([(6, _ring((3.0, 6.0), (15.0, 6.0)))], karte)
        self.assertEqual(halb[5:7, 9].tolist(), [128, 128])
        # Runde Enden: hinter dem Endpunkt fällt die Deckung ab.
        self.assertEqual(int(breit[5, 15]), 255)  # Mitte 15,5: Abstand 0,5 zum Ende
        self.assertEqual(int(breit[5, 16]), 128)  # Abstand 1,5: Deckung 1,5 + 0,5 − 1,5 = 0,5
        self.assertEqual(int(breit[5, 17]), 0)

    def test_maximum_und_reihenfolge(self) -> None:
        karte = _karte(20, 20)
        a = (3, _ring((2.0, 18.0), (18.0, 3.0)))
        b = (5, _ring((2.0, 3.0), (18.0, 18.0)))
        ab = kr.zeichne_fluesse([a, b], karte)
        ba = kr.zeichne_fluesse([b, a], karte)
        np.testing.assert_array_equal(ab, ba)
        np.testing.assert_array_equal(ab, np.maximum(kr.zeichne_fluesse([a], karte), kr.zeichne_fluesse([b], karte)))

    def test_ausserhalb_und_entartet(self) -> None:
        karte = _karte(10, 10)
        bild = kr.zeichne_fluesse([(1, _ring((-5.0, 5.0), (-3.0, 6.0))), (2, _ring((4.5, 4.5), (4.5, 4.5)))], karte)
        self.assertEqual(int(bild.sum() > 0), 1)
        self.assertEqual(int(bild[5, 4]), 255)

    def test_fixture(self) -> None:
        _, _, ergebnis = _fixture()
        fluesse = ergebnis.fluesse
        self.assertEqual(fluesse.dtype, np.uint8)
        self.assertEqual(fluesse.shape, (200, 204))
        self.assertGreater(int((fluesse == 255).sum()), 200)
        self.assertGreater(int(((fluesse > 0) & (fluesse < 255)).sum()), 200)


# ─── PNG ────────────────────────────────────────────────────────────────────


def _png_mit_filter(bild: np.ndarray, filtertyp: int, stufe: int = 6) -> bytes:
    """Unabhängiger PNG-Schreiber mit festem Zeilenfilter (für Decoder-Tests)."""
    kanaele = 1 if bild.ndim == 2 else bild.shape[2]
    farbtyp = {1: 0, 3: 2, 4: 6}[kanaele]
    hoehe, breite = bild.shape[:2]
    zeilen = [bytes(bild[y].reshape(-1).tolist()) for y in range(hoehe)]
    roh = bytearray()
    vorher = bytes(kanaele * breite)
    for zeile in zeilen:
        roh.append(filtertyp)
        for k, wert in enumerate(zeile):
            a = zeile[k - kanaele] if k >= kanaele else 0
            b = vorher[k]
            c = vorher[k - kanaele] if k >= kanaele else 0
            p = a + b - c
            paeth = (
                a if abs(p - a) <= abs(p - b) and abs(p - a) <= abs(p - c) else (b if abs(p - b) <= abs(p - c) else c)
            )
            vorhersage = (0, a, b, (a + b) // 2, paeth)[filtertyp]
            roh.append((wert - vorhersage) & 0xFF)
        vorher = zeile
    ihdr = struct.pack(">IIBBBBB", breite, hoehe, 8, farbtyp, 0, 0, 0)
    return (
        karte_png.PNG_SIGNATUR
        + karte_png._chunk(b"IHDR", ihdr)
        + karte_png._chunk(b"IDAT", zlib.compress(bytes(roh), stufe))
        + karte_png._chunk(b"IEND", b"")
    )


def _bloecke(png: bytes) -> list[bytes]:
    pos, typen = 8, []
    while pos < len(png):
        (laenge,) = struct.unpack(">I", png[pos : pos + 4])
        typen.append(png[pos + 4 : pos + 8])
        pos += 12 + laenge
    return typen


class TestPng(unittest.TestCase):
    def test_rundreise(self) -> None:
        rng = np.random.default_rng(1)
        for form in ((7, 5), (7, 5, 3), (3, 9, 4), (1, 1), (1, 1, 3)):
            bild = rng.integers(0, 256, size=form, dtype=np.uint8)
            bild[1:] = bild[:1] if form[0] > 1 else bild[1:]  # gleiche Zeilen: Filter Up
            daten = karte_png.kodieren(bild)
            self.assertEqual(_bloecke(daten), [b"IHDR", b"IDAT", b"IEND"])
            np.testing.assert_array_equal(karte_png.dekodieren(daten), bild)
            self.assertEqual(daten, karte_png.kodieren(bild))

    def test_decoder_alle_filter(self) -> None:
        rng = np.random.default_rng(2)
        for form in ((6, 5), (6, 5, 3), (4, 3, 4)):
            bild = rng.integers(0, 256, size=form, dtype=np.uint8)
            for filtertyp in range(5):
                with self.subTest(form=form, filtertyp=filtertyp):
                    np.testing.assert_array_equal(karte_png.dekodieren(_png_mit_filter(bild, filtertyp)), bild)

    def test_decoder_fehler(self) -> None:
        gut = karte_png.kodieren(np.zeros((2, 2, 3), dtype=np.uint8))
        faelle = [
            (b"kein png", "keine PNG-Datei"),
            (gut[:30], "PNG abgeschnitten"),
            (gut[:20] + bytes([gut[20] ^ 1]) + gut[21:], "Prüfsumme des PNG-Blocks IHDR falsch"),
            (gut[:-12], "PNG ohne IEND-Block"),
            (karte_png.PNG_SIGNATUR + karte_png._chunk(b"IEND", b""), "PNG ohne gültigen IHDR-Block"),
            (
                karte_png.PNG_SIGNATUR
                + karte_png._chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 16, 2, 0, 0, 0))
                + karte_png._chunk(b"IEND", b""),
                "nicht unterstütztes PNG-Format",
            ),
            (
                karte_png.PNG_SIGNATUR
                + karte_png._chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 2, 0, 0, 0))
                + karte_png._chunk(b"IDAT", zlib.compress(b"\x00" * 5))
                + karte_png._chunk(b"IEND", b""),
                "PNG-Bilddaten haben die falsche Länge",
            ),
            (
                karte_png.PNG_SIGNATUR
                + karte_png._chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 0, 0, 0, 0))
                + karte_png._chunk(b"IDAT", zlib.compress(b"\x07\x00"))
                + karte_png._chunk(b"IEND", b""),
                "unbekannter PNG-Zeilenfilter 7",
            ),
        ]
        for daten, erwartet in faelle:
            with self.subTest(erwartet=erwartet):
                with self.assertRaises(ValueError) as fehler:
                    karte_png.dekodieren(daten)
                self.assertIn(erwartet, str(fehler.exception))
        with self.assertRaises(ValueError):
            karte_png.kodieren(np.zeros((2, 2, 2), dtype=np.uint8))


# ─── Ausgaben und Kommandozeile ─────────────────────────────────────────────


class TestAusgaben(unittest.TestCase):
    def test_erwartete_ausgabe_der_fixture(self) -> None:
        _, _, ergebnis = _fixture()
        self.assertEqual(kg.pruefe_ausgabe(TESTDATEN / "karte", ergebnis), [])
        csv = (TESTDATEN / "karte" / "definition.csv").read_text(encoding="utf-8").splitlines()
        self.assertEqual(csv[0], "kartenindex;ort;name;x;y;pixel;flaeche_km2")
        self.assertEqual(len(csv), 9)
        self.assertEqual(csv[3].split(";")[:3], ["2", "insel", "Inselstadt"])
        provinzen = karte_png.dekodieren((TESTDATEN / "karte" / "provinzen.png").read_bytes())
        self.assertEqual(provinzen.shape, (200, 204, 3))
        fluesse = karte_png.dekodieren((TESTDATEN / "karte" / "fluesse.png").read_bytes())
        self.assertEqual(fluesse.shape, (200, 204))
        self.assertEqual((TESTDATEN / "karte" / "fluesse.png").read_bytes()[25], 0)  # Farbtyp Graustufen
        # Blaukanal ≠ 0 ⇒ kein Ort.
        self.assertTrue((provinzen[..., :2][provinzen[..., 2] != 0] == 0).all())

    def test_api_ohne_dateien(self) -> None:
        ergebnis = kg.erzeuge_karte(TESTDATEN)
        csv = (TESTDATEN / "karte" / "definition.csv").read_bytes()
        self.assertEqual(ergebnis.definition_csv(), csv)
        f = ergebnis.flaeche_nach_ort["burg"]
        self.assertEqual(f.kartenindex, 4)
        self.assertAlmostEqual(f.laenge, ergebnis.karte.pixelmitte(f.x, f.y)[0])
        teil = kg.erzeuge(kg.lade_grundlage(TESTDATEN, fluesse=False), ergebnis.orte[:2])
        self.assertIsNone(teil.fluesse)
        self.assertEqual([f.ort for f in teil.flaechen], ["hafen", "heide"])
        with self.assertRaises(ValueError):
            kg.ausgaben(teil)

    def test_nachbarn_und_ort_bei(self) -> None:
        _, orte, ergebnis = _fixture()
        nachbarn = ergebnis.nachbarn()
        index = {o.kennung: k for k, o in enumerate(orte)}
        self.assertIn((index["hafen"], index["heide"]), nachbarn)
        self.assertIn((index["burg"], index["seedorf"]), nachbarn)
        self.assertFalse(any(index["insel"] in paar for paar in nachbarn))  # Insel: nur Wasser ringsum
        self.assertTrue(all(a < b and laenge > 0 for (a, b), laenge in nachbarn.items()))
        # Gegenprobe der Grenzlänge mit einer einfachen Zählung.
        w = ergebnis.werte
        a, b = index["burg"] + 1, index["seedorf"] + 1
        zaehlung = int(((w[:, :-1] == a) & (w[:, 1:] == b)).sum() + ((w[:, :-1] == b) & (w[:, 1:] == a)).sum())
        zaehlung += int(((w[:-1] == a) & (w[1:] == b)).sum() + ((w[:-1] == b) & (w[1:] == a)).sum())
        self.assertEqual(nachbarn[(a - 1, b - 1)], zaehlung)
        self.assertEqual(ergebnis.ort_bei(10.30, 52.40), index["hafen"])
        self.assertEqual(ergebnis.ort_bei(10.64, 52.53), index["insel"])
        self.assertIsNone(ergebnis.ort_bei(10.3, 52.55))  # Meer
        self.assertIsNone(ergebnis.ort_bei(12.0, 52.3))  # außerhalb

    def test_nachbarn_csv_und_flussuebergaenge(self) -> None:
        _, orte, ergebnis = _fixture()
        csv = ergebnis.nachbarn_csv()
        self.assertEqual(csv, (TESTDATEN / "karte" / "nachbarn.csv").read_bytes())
        zeilen = csv.decode().splitlines()
        self.assertEqual(zeilen[0], kg.NACHBARN_KOPF)
        self.assertEqual(len(zeilen) - 1, len(ergebnis.nachbarn()))
        # Strecken quer über einen Fluss: kleinster gekreuzter Rang; Berühren zählt nicht.
        punkte = [kg.Ort("a", "A", 0.0, 0.0), kg.Ort("b", "B", 2.0, 0.0), kg.Ort("c", "C", 0.0, 2.0)]
        linien = [
            (4, np.array([[1.0, -1.0], [1.0, 1.0]])),  # kreuzt a–b
            (2, np.array([[1.5, -1.0], [1.5, 0.5]])),  # kreuzt a–b
            (1, np.array([[0.0, 2.0], [3.0, 2.0]])),  # berührt c nur
        ]
        self.assertEqual(kg.flussuebergaenge(punkte, [(0, 1), (0, 2)], linien), {(0, 1): 2, (0, 2): 0})
        self.assertEqual(kg.flussuebergaenge(punkte, [(0, 1)], []), {(0, 1): 0})
        self.assertEqual(
            kg.nachbarn_csv(punkte, {(0, 1): 7, (0, 2): 3}, {(0, 1): 2}),
            b"ort_a;ort_b;grenze;fluss\na;b;7;2\na;c;3;0\n",
        )

    def test_definition_csv(self) -> None:
        flaechen = [kg.Flaeche(0, "berlin_coelln", "Berlin-Cölln", 2118, 1043, 1734, 212, 13.4, 52.5)]
        self.assertEqual(
            kg.definition_csv(flaechen),
            "kartenindex;ort;name;x;y;pixel;flaeche_km2\n0;berlin_coelln;Berlin-Cölln;2118;1043;1734;212\n".encode(),
        )


class TestKommandozeile(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def test_pruefen_der_testdaten(self) -> None:
        code, aus, err = _starte("--daten", str(TESTDATEN), "--pruefen")
        self.assertEqual(code, 0, err)
        self.assertIn("Karte aktuell", aus)
        self.assertIn("8 Orte, Bild 204×200", aus)

    def test_schreiben_und_unveraendert(self) -> None:
        ausgabe = self.tmp / "aus"
        code, aus, err = _starte("--daten", str(TESTDATEN), "--ausgabe", str(ausgabe))
        self.assertEqual(code, 0, err)
        self.assertEqual(aus.count("geschrieben:"), len(kg.AUSGABEN))
        for name in kg.AUSGABEN:
            self.assertEqual((ausgabe / name).read_bytes(), (TESTDATEN / "karte" / name).read_bytes(), name)
        code, aus, _ = _starte("--daten", str(TESTDATEN), "--ausgabe", str(ausgabe))
        self.assertEqual(aus.count("unverändert:"), len(kg.AUSGABEN))
        # Gleiche Pixel, andere Kompression: bleibt unberührt.
        bild = karte_png.dekodieren((ausgabe / "provinzen.png").read_bytes())
        (ausgabe / "provinzen.png").write_bytes(_png_mit_filter(bild, 4, stufe=1))
        code, aus, _ = _starte("--daten", str(TESTDATEN), "--ausgabe", str(ausgabe), "--pruefen")
        self.assertEqual(code, 0)
        code, aus, _ = _starte("--daten", str(TESTDATEN), "--ausgabe", str(ausgabe))
        self.assertIn(f"unverändert: {ausgabe / 'provinzen.png'}", aus)

    def test_pruefen_erkennt_abweichungen(self) -> None:
        ausgabe = self.tmp / "aus"
        shutil.copytree(TESTDATEN / "karte", ausgabe)
        provinzen = karte_png.dekodieren((ausgabe / "provinzen.png").read_bytes()).copy()
        provinzen[10, 10, 2] ^= 1
        (ausgabe / "provinzen.png").write_bytes(karte_png.kodieren(provinzen))
        fluesse = karte_png.dekodieren((ausgabe / "fluesse.png").read_bytes()).copy()
        fluesse[:2, :3] = 7
        (ausgabe / "fluesse.png").write_bytes(karte_png.kodieren(fluesse))
        csv = (ausgabe / "definition.csv").read_bytes()
        (ausgabe / "definition.csv").write_bytes(csv.replace(b"Aue", b"Au"))
        nachbarn = (ausgabe / "nachbarn.csv").read_bytes()
        (ausgabe / "nachbarn.csv").write_bytes(nachbarn.replace(b"hafen;heide;48;2", b"hafen;heide;48;0"))
        code, _, err = _starte("--daten", str(TESTDATEN), "--ausgabe", str(ausgabe), "--pruefen")
        self.assertEqual(code, 1)
        self.assertIn("nachbarn.csv: Inhalt veraltet", err)
        (ausgabe / "nachbarn.csv").write_bytes(nachbarn)
        self.assertIn("provinzen.png: 1 Pixel weichen ab", err)
        self.assertIn("fluesse.png: 6 Pixel weichen ab", err)
        self.assertIn("definition.csv: Inhalt veraltet", err)
        self.assertIn("Karte nicht aktuell", err)
        # CRLF-Zeilenenden sind kein Unterschied; fehlende und falsche Dateien schon.
        shutil.copy(TESTDATEN / "karte" / "provinzen.png", ausgabe / "provinzen.png")
        shutil.copy(TESTDATEN / "karte" / "fluesse.png", ausgabe / "fluesse.png")
        (ausgabe / "definition.csv").write_bytes(csv.replace(b"\n", b"\r\n"))
        code, _, err = _starte("--daten", str(TESTDATEN), "--ausgabe", str(ausgabe), "--pruefen")
        self.assertEqual(code, 0, err)
        (ausgabe / "fluesse.png").write_bytes(karte_png.kodieren(np.zeros((200, 204, 3), dtype=np.uint8)))
        (ausgabe / "provinzen.png").write_bytes(b"kaputt")
        (ausgabe / "definition.csv").unlink()
        code, _, err = _starte("--daten", str(TESTDATEN), "--ausgabe", str(ausgabe), "--pruefen")
        self.assertEqual(code, 1)
        self.assertIn("provinzen.png: nicht lesbar (keine PNG-Datei)", err)
        self.assertIn("fluesse.png: Bild 204×200 RGB, erwartet 204×200 Graustufen", err)
        self.assertIn("definition.csv: Datei fehlt", err)

    def test_vorschau(self) -> None:
        ziel = self.tmp / "v" / "vorschau.png"
        code, aus, err = _starte(
            "--daten",
            str(TESTDATEN),
            "--ausgabe",
            str(self.tmp / "aus"),
            "--vorschau",
            str(ziel),
            "--vorschau-breite",
            "102",
            "--zeiten",
        )
        self.assertEqual(code, 0, err)
        self.assertIn("Zeiten: Geometrie", aus)
        bild = karte_png.dekodieren(ziel.read_bytes())
        self.assertEqual(bild.shape, (100, 102, 3))
        code, aus, err = _starte(
            "--daten",
            str(TESTDATEN),
            "--pruefen",
            "--vorschau",
            str(ziel),
            "--vorschau-ausschnitt",
            "10.1,52.1,10.5,52.5",
            "--vorschau-breite",
            "400",
            "--vorschau-namen",
        )
        self.assertEqual(code, 0, err)
        bild = karte_png.dekodieren(ziel.read_bytes())
        self.assertEqual(bild.shape[1], 400)
        self.assertGreater(bild.shape[0], 300)
        code, _, err = _starte(
            "--daten", str(TESTDATEN), "--pruefen", "--vorschau", str(ziel), "--vorschau-ausschnitt", "20,40,21,41"
        )
        self.assertEqual(code, 2)
        self.assertIn("Vorschau-Ausschnitt liegt außerhalb der Karte", err)

    def test_vorschau_inhalt(self) -> None:
        import karte_vorschau

        _, _, ergebnis = _fixture()
        staaten = {"AAA": kg.Staat("AAA", "A", (10, 20, 200)), "BBB": kg.Staat("BBB", "B", (200, 20, 10))}
        bild = karte_vorschau.vorschau(ergebnis, staaten)
        self.assertEqual(bild.shape, (200, 204, 3))

        def haeufigste(kennung: str) -> tuple[int, ...]:
            index = ergebnis.flaeche_nach_ort[kennung].kartenindex
            farben, zahl = np.unique(bild[ergebnis.werte == index + 1], axis=0, return_counts=True)
            return tuple(int(c) for c in farben[int(np.argmax(zahl))])

        faktor = (0.92, 1.0, 1.08)[(3 * 2654435761 >> 7) % 3]  # Helligkeitsstufe des Kartenwerts 3
        self.assertEqual(haeufigste("insel"), tuple(int(c * faktor) for c in (200, 20, 10)))  # jüngster Besitzer
        self.assertEqual(haeufigste("moor"), karte_vorschau.FARBE_OHNE_DATEN)  # Ort ohne Daten, schraffiert
        moor = ergebnis.werte == ergebnis.flaeche_nach_ort["moor"].kartenindex + 1
        self.assertTrue((bild[moor] == karte_vorschau.FARBE_SCHRAFFUR).all(axis=1).any())
        self.assertEqual(tuple(int(c) for c in bild[195, 200]), karte_vorschau.FARBE_AUSLAND)

    def test_datenfehler_exitcode_2(self) -> None:
        daten = _kopie_testdaten(self.tmp)
        (daten / "history/orte/a_nord.chron").write_text("ort kaputt {\n    lage = { 1 }\n}\n", encoding="utf-8")
        code, _, err = _starte("--daten", str(daten), "--pruefen")
        self.assertEqual(code, 2)
        self.assertIn("Fehler: history/orte/a_nord.chron:1: Pflichtfeld 'name' fehlt in ort kaputt", err)
        self.assertIn("Abbruch: 2 Fehler in den Daten.", err)
        (daten / "karte/geometrie/seen.geojson").unlink()
        code, _, err = _starte("--daten", str(daten))
        self.assertEqual(code, 2)
        self.assertIn("karte/geometrie/seen.geojson: Datei fehlt", err)

    def test_hilfe_und_aufruffehler_deutsch(self) -> None:
        with contextlib.redirect_stdout(io.StringIO()) as aus, self.assertRaises(SystemExit) as ende:
            kg.main(["--hilfe"])
        self.assertEqual(ende.exception.code, 0)
        self.assertIn("Aufruf: kartengenerator.py", aus.getvalue())
        self.assertIn("--vorschau-breite N", aus.getvalue())
        for argumente, erwartet in (
            (["--gibtsnicht"], "Fehler: unbekannte Argumente: --gibtsnicht"),
            (["--vorschau-breite", "x"], "ungültige Ganzzahl: 'x'"),
            (["--vorschau-breite", "3"], "erwartet 16 bis 16384"),
            (["--vorschau-ausschnitt", "1,2,3"], "erwartet LAENGE_MIN,BREITE_MIN,LAENGE_MAX,BREITE_MAX"),
        ):
            with contextlib.redirect_stderr(io.StringIO()) as err, self.assertRaises(SystemExit) as ende:
                kg.main(argumente)
            self.assertEqual(ende.exception.code, 2)
            self.assertIn(erwartet, err.getvalue())

    def test_skript_direkt(self) -> None:
        ergebnis = subprocess.run(
            [sys.executable, str(SKRIPT), "--daten", str(TESTDATEN), "--pruefen"],
            capture_output=True,
            text=True,
            cwd=self.tmp,
            check=False,
        )
        self.assertEqual(ergebnis.returncode, 0, ergebnis.stderr)


def _vergleichs_python() -> str | None:
    """Eine andere Python-Version mit numpy (für den Determinismus-Test) oder None."""
    kandidaten = [os.environ.get("KARTENGENERATOR_PYTHON_VERGLEICH", "")]
    kandidaten += [shutil.which(f"python3.{n}") or "" for n in range(10, 15)]
    eigene = f"{sys.version_info.major}.{sys.version_info.minor}"
    for pfad in kandidaten:
        if not pfad:
            continue
        probe = subprocess.run(
            [pfad, "-c", "import sys, numpy; print(f'{sys.version_info.major}.{sys.version_info.minor}')"],
            capture_output=True,
            text=True,
            check=False,
        )
        if probe.returncode == 0 and probe.stdout.strip() != eigene:
            return pfad
    return None


class TestDeterminismus(unittest.TestCase):
    def test_zweimal_gleich(self) -> None:
        grundlage, orte, ergebnis = _fixture()
        zweites = kg.erzeuge(grundlage, orte)
        np.testing.assert_array_equal(zweites.werte, ergebnis.werte)
        self.assertEqual(zweites.flaechen, ergebnis.flaechen)

    def test_andere_python_version(self) -> None:
        python = _vergleichs_python()
        if python is None:
            self.skipTest("keine zweite Python-Version mit numpy gefunden (KARTENGENERATOR_PYTHON_VERGLEICH)")
        with tempfile.TemporaryDirectory() as tmp:
            # Zusätzliche Orte mit Gleichständen und Lagen außerhalb, damit mehr Pfade laufen.
            daten = _kopie_testdaten(Path(tmp))
            (daten / "history/orte/c_extra.chron").write_text(
                "".join(
                    f'ort x{k} {{\n    name = "X{k}"\n    lage = {{ {10.05 + 0.1 * (k % 9):.2f} '
                    f"{52.02 + 0.07 * (k // 9):.2f} }}\n}}\n"
                    for k in range(40)
                ),
                encoding="utf-8",
            )
            for ordner, programm in (("a", sys.executable), ("b", python)):
                lauf = subprocess.run(
                    [programm, str(SKRIPT), "--daten", str(daten), "--ausgabe", str(Path(tmp) / ordner)],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                self.assertEqual(lauf.returncode, 0, lauf.stderr)
            for name in kg.AUSGABEN:
                a = (Path(tmp) / "a" / name).read_bytes()
                b = (Path(tmp) / "b" / name).read_bytes()
                if name.endswith(".png"):
                    np.testing.assert_array_equal(karte_png.dekodieren(a), karte_png.dekodieren(b))
                else:
                    self.assertEqual(a, b)


if __name__ == "__main__":
    unittest.main()
