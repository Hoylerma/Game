"""Tests für bevoelkerung_1846.py (python3 -m unittest discover -s tools/geodaten)."""

from __future__ import annotations

import contextlib
import io
import unittest

import bevoelkerung_1846 as bv
import zuordnung as zu

ORTE = [
    {"id": "stadt", "datei": "land", "flaeche": "100", "stadt": "ja", "gelaende": "flachland"},
    {"id": "dorf", "datei": "land", "flaeche": "100", "stadt": "nein", "gelaende": "flachland"},
    {"id": "berg", "datei": "land", "flaeche": "100", "stadt": "nein", "gelaende": "hochgebirge"},
    {"id": "fremd", "datei": "land", "flaeche": "100", "stadt": "nein", "gelaende": "flachland"},
]
CODES = {o["id"]: {"land": "DE", "admin1": "02", "admin2": "", "admin3": "", "einwohner_heute": "10000"} for o in ORTE}
ZUORDNUNG = {
    "stadt": zu.Zuordnung("AAA", True, "aa_kreis", None),
    "dorf": zu.Zuordnung("AAA", True, "aa_kreis", None),
    "berg": zu.Zuordnung("AAA", True, None, None),
    "fremd": zu.Zuordnung(None, False, None, None),
}
UEBER = {"aa_kreis": "aa_land", "aa_land": ""}


def merkmale(*zeilen: tuple[str, str, str, str]) -> list[bv.Merkmal]:
    daten = [dict(zip(bv.MERKMAL_SPALTEN, (*z, "ungeprueft", "q")), _zeile=str(nr)) for nr, z in enumerate(zeilen, start=2)]
    ergebnis, fehler = bv.lies_merkmale(daten)
    assert not fehler, fehler
    return ergebnis


class TestSchaetzen(unittest.TestCase):
    def test_summen_feste_staedte_und_merkmale(self) -> None:
        m = merkmale(
            ("staat:AAA", "lutherisch=100", "", ""),
            ("verwaltung:aa_land", "katholisch=60 lutherisch=40", "", ""),
            ("ort:berg", "", "tiroler=100", ""),
        )
        orte, fehler = bv.schaetzen(ORTE, CODES, ZUORDNUNG, UEBER, {"aa_land": 20000, "AAA": 5000}, {"stadt": 8000}, m)
        self.assertEqual(fehler, [])
        nach = {o.kennung: o for o in orte}
        self.assertNotIn("fremd", nach)  # Ort ohne Daten
        # Raum aus der Verwaltungskette (aa_kreis hat keine Summe, aa_land schon), sonst der Staat.
        self.assertEqual((nach["stadt"].raum, nach["dorf"].raum, nach["berg"].raum), ("aa_land", "aa_land", "AAA"))
        self.assertEqual(nach["stadt"].einwohner + nach["dorf"].einwohner, 20000)
        self.assertEqual(nach["berg"].einwohner, 5000)
        self.assertEqual(nach["stadt"].einwohner - nach["dorf"].einwohner, 8000)  # feste Stadt plus gleiche Fläche
        self.assertEqual(nach["dorf"].konfession, "katholisch=60 lutherisch=40")
        self.assertEqual(nach["berg"].konfession, "lutherisch=100")
        self.assertEqual((nach["berg"].kultur, nach["dorf"].kultur), ("tiroler=100", ""))

    def test_fehler(self) -> None:
        m = merkmale(("staat:AAA", "lutherisch=100", "", ""))
        _, fehler = bv.schaetzen(ORTE, CODES, ZUORDNUNG, UEBER, {"aa_land": 1000}, {"stadt": 5000, "fremd": 10}, m)
        self.assertIn("Ort berg: keine Einwohnersumme für AAA oder seine Verwaltung (einwohner_1846.csv)", fehler)
        self.assertIn("staedte_1846.csv: fremd ist kein Ort mit Besitzer 1848", fehler)
        self.assertIn("Raum aa_land: Summe 1000 reicht nicht für die festen Städte (5000)", fehler)

    def test_ungueltige_merkmale(self) -> None:
        daten = [
            dict(zip(bv.MERKMAL_SPALTEN, ("staat:AAA", "lutherisch 100", "", "x", "ungeprueft", "q")), _zeile="2"),
            dict(zip(bv.MERKMAL_SPALTEN, ("irgendwo", "", "", "", "ungeprueft", "q")), _zeile="3"),
        ]
        _, fehler = bv.lies_merkmale(daten)
        self.assertEqual(
            fehler,
            [
                "merkmale_1846.csv:2: ungültige Anteile in Spalte 'konfession': 'lutherisch 100'",
                "merkmale_1846.csv:2: ungültige Dichte 'x'",
                "merkmale_1846.csv:3: ungültiges Gebiet 'irgendwo'",
            ],
        )


class TestDaten(unittest.TestCase):
    def test_bevoelkerung_ist_aktuell(self) -> None:
        aus = io.StringIO()
        with contextlib.redirect_stdout(aus), contextlib.redirect_stderr(io.StringIO()) as fehler:
            code = bv.main(["--pruefen"])
        self.assertEqual(code, 0, fehler.getvalue() + aus.getvalue())


if __name__ == "__main__":
    unittest.main()
