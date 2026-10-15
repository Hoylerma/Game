"""Tests für zuordnung.py (python3 -m unittest discover -s tools/geodaten -p 'test_zuordnung.py')."""

from __future__ import annotations

import contextlib
import io
import unittest

import zuordnung as zu

ORTE = [
    {"id": "coburg", "datei": "franken", "flaeche": "100", "name": "Coburg", "quelle": ""},
    {"id": "bamberg", "datei": "franken", "flaeche": "200", "name": "Bamberg", "quelle": ""},
    {"id": "posen", "datei": "posen", "flaeche": "300", "name": "Posen", "quelle": ""},
    {"id": "strassburg", "datei": "elsass", "flaeche": "50", "name": "Straßburg", "quelle": ""},
]
CODES = {
    "coburg": {"land": "DE", "admin1": "02", "admin2": "094", "admin3": "09463"},
    "bamberg": {"land": "DE", "admin1": "02", "admin2": "094", "admin3": "09461"},
    "posen": {"land": "PL", "admin1": "86", "admin2": "3064", "admin3": ""},
    "strassburg": {"land": "FR", "admin1": "44", "admin2": "67", "admin3": ""},
}
STAATEN = {"BAV", "SCG", "PRU"}
EINHEITEN = {"by_oberfranken": "BAV", "pr_posen": "PRU", "scg_coburg": "SCG"}


def regeln(*zeilen: tuple[str, ...]) -> list[zu.Regel]:
    daten = [
        dict(zip(zu.REGEL_SPALTEN, z), _zeile=str(nr)) for nr, z in enumerate(zeilen, start=2)
    ]
    ergebnis, fehler = zu.lies_regeln(daten)
    assert not fehler, fehler
    return ergebnis


GRUND = (
    ("DE", "", "ja", "", "ungeprueft", "Bund"),
    ("DE.02", "BAV", "", "", "ungeprueft", "Bayern"),
    ("DE.02.094", "", "", "by_oberfranken", "ungeprueft", "Oberfranken"),
    ("ort:coburg", "SCG", "", "", "ungeprueft", "Coburg"),
    ("landschaft:posen", "PRU", "nein", "pr_posen", "ungeprueft", "Posen"),
    ("FR", "-", "", "", "ungeprueft", "Ausland"),
)


class TestRegeln(unittest.TestCase):
    def test_rang(self) -> None:
        self.assertEqual(
            [zu.rang_von(g) for g in ("ort:x", "DE.15.00.16051", "DE.02.094", "landschaft:posen", "DE.02", "DE")],
            [6, 5, 4, 3, 2, 1],
        )
        self.assertEqual(zu.rang_von("de.02"), 0)
        self.assertEqual(zu.rang_von("DE.1.2.3.4"), 0)

    def test_fehler_beim_lesen(self) -> None:
        daten = [
            {"gebiet": "xx", "staat": "BAV", "bund": "", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "q", "_zeile": "2"},
            {"gebiet": "DE", "staat": "BAV", "bund": "vielleicht", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "q", "_zeile": "3"},
            {"gebiet": "DE.02", "staat": "", "bund": "", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "q", "_zeile": "4"},
            {"gebiet": "DE.05", "staat": "HED", "bund": "", "verwaltung": "", "sicherheit": "sicher", "quelle": "q", "_zeile": "5"},
            {"gebiet": "DE.06", "staat": "HAN", "bund": "", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "", "_zeile": "6"},
            {"gebiet": "DE.07", "staat": "PRU", "bund": "", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "q", "_zeile": "7"},
            {"gebiet": "DE.07", "staat": "PRU", "bund": "", "verwaltung": "", "sicherheit": "ungeprueft", "quelle": "q", "_zeile": "8"},
        ]
        _, fehler = zu.lies_regeln(daten, "t.csv")
        self.assertEqual(
            fehler,
            [
                "t.csv:2: ungültiges Gebiet 'xx'",
                "t.csv:3: Spalte 'bund' erwartet ja, nein oder nichts, nicht 'vielleicht'",
                "t.csv:4: Regel für DE.02 setzt kein Feld",
                "t.csv:5: ungültige Sicherheit 'sicher' (erlaubt: gesichert, schaetzung, spielwert, ungeprueft)",
                "t.csv:6: Spalte 'quelle' darf nicht leer sein",
                "t.csv:8: Gebiet DE.07 ist doppelt (zuerst in Zeile 7)",
            ],
        )


class TestAufloesen(unittest.TestCase):
    def test_felder_einzeln_und_staerkste_regel(self) -> None:
        e = zu.aufloesen(ORTE, CODES, regeln(*GRUND), STAATEN, EINHEITEN)
        self.assertEqual(e.fehler, [])
        z = e.zuordnung
        self.assertEqual((z["bamberg"].staat, z["bamberg"].bund, z["bamberg"].verwaltung), ("BAV", True, "by_oberfranken"))
        # Coburg: Staat aus der Ortsregel; die oberfränkische Einheit aus der schwächeren Regel entfällt.
        self.assertEqual((z["coburg"].staat, z["coburg"].bund, z["coburg"].verwaltung), ("SCG", True, None))
        self.assertEqual(z["coburg"].regel.gebiet, "ort:coburg")
        # Landschaft vor heutigem Staat; bund aus derselben Regel.
        self.assertEqual((z["posen"].staat, z["posen"].bund, z["posen"].verwaltung), ("PRU", False, "pr_posen"))
        self.assertIsNone(z["strassburg"].staat)

    def test_einheit_eines_anderen_staates_bei_gleich_starker_regel(self) -> None:
        r = regeln(*GRUND, ("ort:bamberg", "SCG", "", "by_oberfranken", "ungeprueft", "falsch"))
        e = zu.aufloesen(ORTE, CODES, r, STAATEN, EINHEITEN)
        self.assertEqual(len(e.fehler), 1)
        self.assertIn("Ort bamberg: Verwaltungseinheit by_oberfranken (BAV, Regel ort:bamberg) passt nicht", e.fehler[0])

    def test_verwaltung_aufheben(self) -> None:
        r = regeln(*GRUND, ("ort:bamberg", "", "", "-", "ungeprueft", "ohne Einheit"))
        e = zu.aufloesen(ORTE, CODES, r, STAATEN, EINHEITEN)
        self.assertEqual(e.fehler, [])
        self.assertIsNone(e.zuordnung["bamberg"].verwaltung)

    def test_fehlende_regel_und_unbekannte_verweise(self) -> None:
        r = regeln(
            ("DE.02", "XYZ", "ja", "", "ungeprueft", "q"),
            ("landschaft:posen", "PRU", "", "pr_unbekannt", "ungeprueft", "q"),
            ("ort:gibtsnicht", "BAV", "ja", "", "ungeprueft", "q"),
        )
        e = zu.aufloesen(ORTE, CODES, r, STAATEN, EINHEITEN)
        self.assertIn("Zeile 2: unbekannter Staat 'XYZ' (staaten.csv)", e.fehler)
        self.assertIn("Zeile 3: unbekannte Verwaltungseinheit 'pr_unbekannt' (verwaltung.csv)", e.fehler)
        self.assertIn("Ort posen: keine Regel für 'bund'", e.fehler)
        self.assertTrue(any(f.startswith("Ort strassburg (elsass, FR.44.67): keine Regel für den Staat") for f in e.fehler))
        self.assertIn("Zeile 4: Regel für ort:gibtsnicht passt auf keinen Ort", e.warnungen)


class TestNaechsterSitz(unittest.TestCase):
    """verwaltung = naechste:<einheit>: unterste Einheit mit dem nächsten Sitz (seit M7)."""

    ORTE = [
        {"id": "a_stadt", "datei": "land", "breite": "52.0", "laenge": "13.0"},
        {"id": "a_dorf", "datei": "land", "breite": "52.1", "laenge": "13.3"},
        {"id": "b_stadt", "datei": "land", "breite": "52.0", "laenge": "14.0"},
        {"id": "b_dorf", "datei": "land", "breite": "51.9", "laenge": "13.7"},
    ]
    CODES = {o["id"]: {"land": "DE", "admin1": "11", "admin2": "", "admin3": ""} for o in ORTE}
    EINHEITEN = {"prov": "PRU", "rb": "PRU", "kr_a": "PRU", "kr_b": "PRU"}
    UEBER = {"prov": "", "rb": "prov", "kr_a": "rb", "kr_b": "rb"}
    SITZE = {"prov": "", "rb": "a_stadt", "kr_a": "a_stadt", "kr_b": "b_stadt"}

    def test_naechster_kreissitz_unterhalb_der_provinz(self) -> None:
        r = regeln(("DE.11", "PRU", "ja", "naechste:prov", "ungeprueft", "Brandenburg"))
        e = zu.aufloesen(self.ORTE, self.CODES, r, {"PRU"}, self.EINHEITEN, self.UEBER, self.SITZE)
        self.assertEqual((e.fehler, e.warnungen), ([], []))
        # Nur die untersten Einheiten (Kreise) kommen in Frage, nicht der Regierungsbezirk.
        self.assertEqual(
            [e.zuordnung[o["id"]].verwaltung for o in self.ORTE], ["kr_a", "kr_a", "kr_b", "kr_b"]
        )

    def test_fehler(self) -> None:
        r = regeln(
            ("DE.11", "PRU", "ja", "naechste:gibtsnicht", "ungeprueft", "q"),
            ("ort:a_dorf", "", "", "naechste:kr_a", "ungeprueft", "q"),
        )
        e = zu.aufloesen(self.ORTE, self.CODES, r, {"PRU"}, self.EINHEITEN, self.UEBER, self.SITZE)
        self.assertIn("Zeile 2: unbekannte Verwaltungseinheit 'gibtsnicht' (verwaltung.csv)", e.fehler)
        self.assertIn("Zeile 3: unter kr_a gibt es keine Einheit mit Sitz (Spalte 'sitz')", e.fehler)

    def test_sitz_in_fremder_einheit(self) -> None:
        sitze = dict(self.SITZE, kr_b="a_dorf")  # Sitz von kr_b liegt näher an kr_a
        r = regeln(
            ("DE.11", "PRU", "ja", "naechste:prov", "ungeprueft", "q"),
            ("ort:a_dorf", "", "", "kr_a", "ungeprueft", "q"),
        )
        e = zu.aufloesen(self.ORTE, self.CODES, r, {"PRU"}, self.EINHEITEN, self.UEBER, sitze)
        self.assertIn("Sitz a_dorf von kr_b liegt in kr_a", e.warnungen)


class TestVerwaltung(unittest.TestCase):
    @staticmethod
    def einheiten(*zeilen: tuple[str, str, str]) -> list[dict[str, str]]:
        return [{"id": i, "ueber": u, "sitz": s, "_zeile": str(nr)} for nr, (i, u, s) in enumerate(zeilen, start=2)]

    def test_gueltig(self) -> None:
        self.assertEqual(zu.pruefe_verwaltung(self.einheiten(("land", "", "a"), ("k1", "land", "b"), ("k2", "land", "c"))), [])

    def test_doppelte_kennung_und_kreis(self) -> None:
        fehler = zu.pruefe_verwaltung(
            self.einheiten(("vorort", "land", "x"), ("land", "kreis", ""), ("kreis", "land", ""), ("d", "", ""), ("d", "", ""))
        )
        self.assertEqual(
            fehler,
            [
                "verwaltung.csv:6: id 'd' schon in Zeile 5",
                "verwaltung.csv:4: Einheit 'kreis' liegt (über 'ueber') in sich selbst",
                "verwaltung.csv:3: Einheit 'land' liegt (über 'ueber') in sich selbst",
            ],
        )  # vorort führt nur in den Kreis und wird nicht gemeldet

    def test_gleicher_sitz(self) -> None:
        fehler = zu.pruefe_verwaltung(self.einheiten(("land", "", "y"), ("k1", "land", "y"), ("k2", "land", "y")))
        self.assertEqual(fehler, ["verwaltung.csv:4: Sitz 'y' schon für 'k1'"])  # land hat Untereinheiten


class TestBesitz(unittest.TestCase):
    def test_zeilen_zum_stichtag_ersetzen(self) -> None:
        e = zu.aufloesen(ORTE, CODES, regeln(*GRUND), STAATEN, EINHEITEN)
        alt = (
            "ort;ab;besitzer;verwaltung;bund;sicherheit;quelle\n"
            "bamberg;1648.10.24;BAV;;;gesichert;alt\n"
            "bamberg;1848.03.01;PRU;;;gesichert;veraltet\n"
        )
        neu = zu.neue_besitz_tabelle(alt, zu.besitz_zeilen(ORTE, e))
        self.assertEqual(
            neu.splitlines(),
            [
                "ort;ab;besitzer;verwaltung;bund;sicherheit;quelle",
                "bamberg;1648.10.24;BAV;;;gesichert;alt",
                "coburg;1848.03.01;SCG;;ja;ungeprueft;Coburg (Regel ort:coburg, territorien_1848.csv)",
                "bamberg;1848.03.01;BAV;by_oberfranken;ja;ungeprueft;Bayern (Regel DE.02, territorien_1848.csv)",
                "posen;1848.03.01;PRU;pr_posen;;ungeprueft;Posen (Regel landschaft:posen, territorien_1848.csv)",
            ],
        )


class TestDaten(unittest.TestCase):
    def test_besitz_ist_aktuell(self) -> None:
        # Die eingecheckten Regeln, Gebietscodes und besitz.csv passen zusammen (wie --pruefen).
        aus = io.StringIO()
        with contextlib.redirect_stdout(aus), contextlib.redirect_stderr(io.StringIO()) as fehler:
            code = zu.main(["--pruefen"])
        self.assertEqual(code, 0, fehler.getvalue() + aus.getvalue())


if __name__ == "__main__":
    unittest.main()
