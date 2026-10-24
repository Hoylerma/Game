"""Tests für historie_1849_1870.py (python3 -m unittest discover -s tools/geodaten -p 'test_historie*.py')."""

from __future__ import annotations

import unittest

import historie_1849_1870 as hi
import zuordnung as zu

ORTE = [
    {"id": "hannover", "datei": "calenberg", "name": "Hannover"},
    {"id": "kassel", "datei": "kurhessen", "name": "Kassel"},
    {"id": "kiel", "datei": "holstein", "name": "Kiel"},
    {"id": "darmstadt", "datei": "starkenburg", "name": "Darmstadt"},
    {"id": "giessen", "datei": "oberhessen", "name": "Gießen"},
    {"id": "strassburg", "datei": "elsass", "name": "Straßburg"},
]
CODES = {
    "hannover": {"land": "DE", "admin1": "06", "admin2": "", "admin3": ""},
    "kassel": {"land": "DE", "admin1": "05", "admin2": "", "admin3": ""},
    "kiel": {"land": "DE", "admin1": "10", "admin2": "", "admin3": ""},
    "darmstadt": {"land": "DE", "admin1": "05", "admin2": "", "admin3": ""},
    "giessen": {"land": "DE", "admin1": "05", "admin2": "", "admin3": ""},
    "strassburg": {"land": "FR", "admin1": "44", "admin2": "67", "admin3": ""},
}
STAATEN = {"HAN", "HEK", "DEN", "HED", "PRU", "AUT", "FRK"}
EINHEITEN = {
    "hn_hannover": "HAN",
    "dk_holstein": "DEN",
    "au_holstein": "AUT",
    "hd_oberhessen": "HED",
    "pr_hannover": "PRU",
    "pr_hessen_nassau": "PRU",
    "fr_bas_rhin": "FRK",
}
UEBER = {k: "" for k in EINHEITEN}
START = {
    "hannover": hi.Stand("HAN", "hn_hannover", True),
    "kassel": hi.Stand("HEK", None, True),
    "kiel": hi.Stand("DEN", "dk_holstein", True),
    "darmstadt": hi.Stand("HED", None, True),
    "giessen": hi.Stand("HED", "hd_oberhessen", True),
    "strassburg": hi.Stand(None, None, False),
}


def regeln(*zeilen: tuple[str, ...]) -> list[hi.Aenderung]:
    daten = [dict(zip(hi.AENDERUNG_SPALTEN, z), _zeile=str(nr)) for nr, z in enumerate(zeilen, start=2)]
    ergebnis, fehler = hi.lies_aenderungen(daten)
    assert not fehler, fehler
    return ergebnis


def anwenden(*zeilen: tuple[str, ...]) -> hi.Ergebnis:
    return hi.anwenden(ORTE, CODES, regeln(*zeilen), START, STAATEN, EINHEITEN, UEBER)


class TestAenderungen(unittest.TestCase):
    def test_rang_und_lesefehler(self) -> None:
        self.assertEqual([hi.rang_von(g) for g in ("alle", "staat:HAN", "verwaltung:x", "ort:a", "DE")], [0.5, 1.5, 2.5, 6, 1])
        daten = [
            {"ab": "1848.01.01", "gebiet": "alle", "staat": "", "bund": "nein", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "q", "_zeile": "2"},
            {"ab": "1866.01.01", "gebiet": "xx", "staat": "PRU", "bund": "", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "q", "_zeile": "3"},
            {"ab": "1866.01.01", "gebiet": "staat:HAN", "staat": "-", "bund": "", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "q", "_zeile": "4"},
            {"ab": "1866.01.01", "gebiet": "alle", "staat": "", "bund": "", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "q", "_zeile": "5"},
            {"ab": "1866.01.01", "gebiet": "ort:kiel", "staat": "PRU", "bund": "", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "", "_zeile": "6"},
            {"ab": "1866.01.01", "gebiet": "ort:kassel", "staat": "PRU", "bund": "", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "q", "_zeile": "7"},
            {"ab": "1866.01.01", "gebiet": "ort:kassel", "staat": "PRU", "bund": "", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "q", "_zeile": "8"},
        ]
        _, fehler = hi.lies_aenderungen(daten)
        text = "\n".join(fehler)
        for teil in ("liegt nicht zwischen", "ungültiges Gebiet", "nicht verlieren", "setzt kein Feld", "'quelle'", "doppelt"):
            self.assertIn(teil, text)

    def test_annexion_mit_verwaltung(self) -> None:
        e = anwenden(
            ("1866.09.20", "staat:HAN", "PRU", "", "pr_hannover", "ungeprueft", "Annexion"),
            ("1866.09.20", "staat:HEK", "PRU", "", "pr_hessen_nassau", "ungeprueft", "Annexion"),
        )
        self.assertEqual(e.fehler, [])
        stand = e.staende["1866.09.20"]
        self.assertEqual(stand["hannover"], hi.Stand("PRU", "pr_hannover", True))
        self.assertEqual(stand["kassel"], hi.Stand("PRU", "pr_hessen_nassau", True))
        self.assertEqual(stand["darmstadt"], START["darmstadt"])
        self.assertEqual(len(e.besitz), 2)
        zeile = next(z for z in e.besitz if z[0] == "hannover")
        self.assertEqual(zeile[1:5], ["1866.09.20", "PRU", "pr_hannover", "ja"])
        self.assertIn("Regel staat:HAN", zeile[6])

    def test_zeitliche_folge_und_rang(self) -> None:
        # Holstein erst österreichisch (über die Einheit), dann preußisch; der Bund endet für alle, ein
        # Ort mit stärkerer Regel bleibt im Bund.
        e = anwenden(
            ("1866.09.20", "verwaltung:au_holstein", "PRU", "", "-", "ungeprueft", "Annexion"),
            ("1864.10.30", "verwaltung:dk_holstein", "AUT", "", "au_holstein", "ungeprueft", "Wiener Frieden"),
            ("1866.08.24", "alle", "", "nein", "", "ungeprueft", "Ende des Bundes"),
            ("1866.08.24", "verwaltung:hd_oberhessen", "", "ja", "", "ungeprueft", "Ausnahme"),
        )
        self.assertEqual(e.fehler, [])
        self.assertEqual(list(e.staende), ["1864.10.30", "1866.08.24", "1866.09.20"])
        self.assertEqual(e.staende["1864.10.30"]["kiel"], hi.Stand("AUT", "au_holstein", True))
        self.assertEqual(e.staende["1866.08.24"]["kiel"], hi.Stand("AUT", "au_holstein", False))
        self.assertEqual(e.staende["1866.08.24"]["giessen"], hi.Stand("HED", "hd_oberhessen", True))
        self.assertEqual(e.staende["1866.09.20"]["kiel"], hi.Stand("PRU", None, False))
        # Orte ohne Daten bleiben vom Bund unberührt.
        self.assertEqual(e.staende["1866.08.24"]["strassburg"], hi.Stand(None, None, False))

    def test_einheit_bleibt_nur_beim_eigenen_staat(self) -> None:
        e = anwenden(("1866.09.20", "ort:giessen", "PRU", "", "", "ungeprueft", "Wechsel"))
        self.assertEqual(e.staende["1866.09.20"]["giessen"], hi.Stand("PRU", None, True))
        e = anwenden(("1866.09.20", "ort:giessen", "", "", "pr_hannover", "ungeprueft", "falsch"))
        self.assertTrue(any("gehört zu PRU, der Ort zu HED" in f for f in e.fehler))

    def test_neue_orte_und_warnungen(self) -> None:
        e = anwenden(
            ("1849.01.01", "FR", "FRK", "nein", "", "ungeprueft", "Frankreich"),
            ("1849.01.01", "FR.44.67", "", "", "fr_bas_rhin", "ungeprueft", "Bas-Rhin"),
            ("1850.01.01", "ort:nirgends", "PRU", "", "", "ungeprueft", "trifft nichts"),
        )
        self.assertEqual(e.fehler, [])
        self.assertEqual(set(e.neu), {"strassburg"})
        datum, stand, _ = e.neu["strassburg"]
        self.assertEqual((datum, stand), ("1849.01.01", hi.Stand("FRK", "fr_bas_rhin", False)))
        self.assertTrue(any("passt auf keinen Ort" in w for w in e.warnungen))

    def test_unbekannte_verweise(self) -> None:
        e = anwenden(
            ("1866.01.01", "staat:XXX", "PRU", "", "", "ungeprueft", "q"),
            ("1866.01.01", "verwaltung:nirgends", "QQQ", "", "nirgends", "ungeprueft", "q"),
        )
        text = "\n".join(e.fehler)
        self.assertIn("unbekannter Staat in 'staat:XXX'", text)
        self.assertIn("unbekannter Staat 'QQQ'", text)
        self.assertIn("unbekannte Verwaltungseinheit 'nirgends'", text)

    def test_tabelle_behaelt_andere_zeilen(self) -> None:
        alt = zu.csv_text(zu.BESITZ_SPALTEN, [["a", "1848.03.01", "HAN", "", "ja", "ungeprueft", "q"], ["a", "1866.09.20", "HEK", "", "", "ungeprueft", "alt"]])
        neu = hi.neue_tabelle(alt, zu.BESITZ_SPALTEN, 1, [["a", "1866.09.20", "PRU", "", "", "ungeprueft", "neu"]])
        self.assertIn("1848.03.01;HAN", neu)
        self.assertNotIn("HEK", neu)
        self.assertTrue(neu.rstrip().endswith("neu"))


if __name__ == "__main__":
    unittest.main()
