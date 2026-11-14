"""Tests für tools/spieltest (Fragebogen, Rückmeldungen, Abschnitte, Bericht).

    python3 -m unittest discover -s tools/spieltest -v
"""

from __future__ import annotations

import io
import json
import sys
import tempfile
import unittest
import zipfile
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

HIER = Path(__file__).resolve().parent
if str(HIER) not in sys.path:
    sys.path.insert(0, str(HIER))

import auswertung  # noqa: E402


def _messwerte(**werte: object) -> dict:
    basis = {
        "format": 1,
        "version": "0.13.0",
        "system": "Linux 6.8",
        "grafikkarte": "Mittelklasse 3060",
        "fenster": "1920x1080",
        "vollbild": True,
        "bildratenlimit": 0,
        "zeit": "2026-12-01 18:00:00",
        "bildrate_median": 60.0,
        "bildrate_p5": 52.5,
        "bildrate_sekunden": 3600,
        "sitzung_sekunden": 4000,
        "szenario": "voelkerfruehling",
        "spieldatum": "3. Mai 1851",
        "spieler": "PRU",
        "partie_beendet": False,
    }
    basis.update(werte)
    return basis


def _zip(pfad: Path, messwerte: dict, notiz: str = "", protokoll: str = "") -> None:
    pfad.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(pfad, "w") as z:
        z.writestr("messwerte.json", json.dumps(messwerte))
        z.writestr("notiz.txt", notiz + "\n")
        z.writestr("godot.log", protokoll)


FRAGEBOGEN_T01 = """# Kommentar
kennung [Text] Kennung (von der Spielleitung, z. B. T03): T01
A1 [0-3] Erfahrung mit Strategiespielen (0 keine, 1 wenig, 2 einige, 3 viel): 3
B1 [ja/nein] Ich habe die Geschichtsstunde gespielt: ja
B2 [1-5] Nach der Geschichtsstunde wusste ich, was ich im Spiel tun kann: 2
C1 [1-5] Ich wusste meistens, was in meinem Staat gerade vor sich geht: 4
D10 [0-10] Wie wahrscheinlich empfiehlst du das Spiel weiter (0 gar nicht … 10 sicher)?: 9
F2 [ja/nein] Das Spiel ist abgestürzt oder hing: nein
G1 [Text] Was hat dir am besten gefallen?: Die Zeitung,
   besonders die Fraktur.
"""

FRAGEBOGEN_T02 = """kennung [Text] Kennung (von der Spielleitung, z. B. T03):
B1 [ja/nein] Ich habe die Geschichtsstunde gespielt: nein
B2 [1-5] Nach der Geschichtsstunde wusste ich, was ich im Spiel tun kann: 1
C1 [1-5] Ich wusste meistens, was in meinem Staat gerade vor sich geht: 7
D10 [0-10] Wie wahrscheinlich empfiehlst du das Spiel weiter (0 gar nicht … 10 sicher)?: 3
F2 [ja/nein] Das Spiel ist abgestürzt oder hing: vielleicht
G1 [Text] Was hat dir am besten gefallen?: Musik
"""


class Eingang(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.ordner = Path(self._tmp.name)
        (self.ordner / "T01_fragebogen.txt").write_text(FRAGEBOGEN_T01, encoding="utf-8")
        (self.ordner / "T02").mkdir()
        (self.ordner / "T02" / "T02_fragebogen.txt").write_text(FRAGEBOGEN_T02, encoding="utf-8")
        _zip(self.ordner / "T01_rueckmeldung_1.zip", _messwerte(zeit="2026-12-01 18:00:00"), "Erste Notiz")
        _zip(
            self.ordner / "T01_rueckmeldung_2.zip",
            _messwerte(zeit="2026-12-01 19:00:00", spieldatum="1. Juli 1871", partie_beendet=True, bildrate_sekunden=60),
            "",
            "ERROR: etwas\nSCRIPT ERROR: anderes\nnormale Zeile\n",
        )
        _zip(
            self.ordner / "T02" / "rueckmeldung_2026-12-02.zip",
            _messwerte(bildrate_median=41.0, bildrate_p5=30.0, grafikkarte="llvmpipe", spieler="BAV"),
            "Karte ruckelt beim Zoomen",
        )
        self.fragen = auswertung.lies_fragen(auswertung.VORLAGE)
        self.teilnehmer = auswertung.lies_eingang(self.ordner)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def test_vorlage_hat_alle_bereiche(self) -> None:
        ids = [f.id for f in self.fragen]
        for erwartet in ("kennung", "A1", "B2", "C7", "D10", "E3", "F2", "G4"):
            self.assertIn(erwartet, ids)
        self.assertEqual(len(ids), len(set(ids)), "Kennungen der Fragen doppelt")
        d10 = next(f for f in self.fragen if f.id == "D10")
        self.assertEqual((d10.typ, d10.von, d10.bis), ("skala", 0, 10))

    def test_kennung_aus_datei_ordner_und_inhalt(self) -> None:
        self.assertEqual(list(self.teilnehmer), ["T01", "T02"])
        self.assertEqual(len(self.teilnehmer["T01"].rueckmeldungen), 2)
        self.assertEqual(len(self.teilnehmer["T02"].rueckmeldungen), 1)

    def test_folgezeilen_gehoeren_zur_antwort(self) -> None:
        self.assertEqual(self.teilnehmer["T01"].antworten["G1"], "Die Zeitung, besonders die Fraktur.")

    def test_ungueltige_antworten(self) -> None:
        probleme = auswertung.pruefe(self.fragen, self.teilnehmer)
        self.assertEqual(len(probleme), 2, probleme)
        self.assertTrue(any("T02 C1" in p for p in probleme))
        self.assertTrue(any("T02 F2" in p for p in probleme))

    def test_teilnehmende_neueste_rueckmeldung(self) -> None:
        text = auswertung.abschnitt_teilnehmende(self.fragen, self.teilnehmer)
        self.assertIn("2 Teilnehmende, 3 Rückmeldungen", text)
        t01 = next(z for z in text.splitlines() if z.startswith("| T01"))
        self.assertIn("1. Juli 1871", t01)
        self.assertIn("| ja |", t01)
        self.assertIn("66 min", t01)

    def test_technik_bildrate_und_protokoll(self) -> None:
        text = auswertung.abschnitt_technik(self.fragen, self.teilnehmer)
        t01 = next(z for z in text.splitlines() if z.startswith("| T01"))
        t02 = next(z for z in text.splitlines() if z.startswith("| T02"))
        self.assertIn("60,0", t01)  # längste Messung (3600 s), nicht die neueste (60 s)
        self.assertIn("60 min", t01)
        self.assertTrue(t01.rstrip(" |").endswith("2"))  # zwei Fehlerzeilen
        self.assertIn("**nein**", t02)
        self.assertIn("Bildrate erreicht bei 1 von 2", text)

    def test_skalen_median_befund_und_empfehlung(self) -> None:
        text = auswertung.abschnitt_skalen(self.fragen, self.teilnehmer)
        b2 = next(z for z in text.splitlines() if z.startswith("| B2"))
        self.assertIn("| 2 | 1,5 |", b2)
        self.assertIn("**Befund**", b2)
        c1 = next(z for z in text.splitlines() if z.startswith("| C1"))
        self.assertIn("| 1 | 4,0 |", c1)  # die 7 zählt nicht
        d10 = next(z for z in text.splitlines() if z.startswith("| D10"))
        self.assertIn("Förderer 1, Kritiker 1, Saldo 0", d10)
        self.assertNotIn("A1", text.split("|---|", 1)[1].split("\n| B2")[0])

    def test_texte_und_notizen(self) -> None:
        texte = auswertung.abschnitt_texte(self.fragen, self.teilnehmer)
        self.assertIn("- T01: Die Zeitung, besonders die Fraktur.", texte)
        self.assertIn("- T02: Musik", texte)
        self.assertIn("(keine Antworten)", texte)
        notizen = auswertung.abschnitt_notizen(self.fragen, self.teilnehmer)
        self.assertIn("T02 (voelkerfruehling, BAV, 3. Mai 1851): Karte ruckelt beim Zoomen", notizen)
        self.assertIn("Erste Notiz", notizen)

    def test_bericht_ersetzt_nur_markierte_abschnitte(self) -> None:
        bericht = self.ordner / "bericht.md"
        teile = [f"<!-- auswertung:{n} -->\nalt\n<!-- /auswertung:{n} -->" for n in auswertung.ABSCHNITTE]
        bericht.write_text("# Kopf\n\nVon Hand.\n\n" + "\n\n".join(teile) + "\n\nSchluss von Hand.\n", encoding="utf-8")
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            code = auswertung.main(["--eingang", str(self.ordner), "--bericht", str(bericht)])
        self.assertEqual(code, 0)
        text = bericht.read_text(encoding="utf-8")
        self.assertNotIn("\nalt\n", text)
        self.assertIn("Von Hand.", text)
        self.assertIn("Schluss von Hand.", text)
        self.assertIn("| T02 |", text)
        # Zweimal auswerten ergibt dasselbe.
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            auswertung.main(["--eingang", str(self.ordner), "--bericht", str(bericht)])
        self.assertEqual(bericht.read_text(encoding="utf-8"), text)

    def test_streng_und_fehlende_markierung(self) -> None:
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(auswertung.main(["--eingang", str(self.ordner), "--streng"]), 1)
            bericht = self.ordner / "ohne.md"
            bericht.write_text("# nichts\n", encoding="utf-8")
            self.assertEqual(auswertung.main(["--eingang", str(self.ordner), "--bericht", str(bericht)]), 2)

    def test_zip_ohne_kennung_wird_abgelehnt(self) -> None:
        _zip(self.ordner / "rueckmeldung_2026-12-03.zip", _messwerte())
        with self.assertRaises(auswertung.AuswertungsFehler):
            auswertung.lies_eingang(self.ordner)

    def test_falsches_format(self) -> None:
        _zip(self.ordner / "T03_x.zip", _messwerte(format=99))
        with self.assertRaises(auswertung.AuswertungsFehler):
            auswertung.lies_eingang(self.ordner)


class Ergebnisvorlage(unittest.TestCase):
    def test_vorlage_hat_alle_markierungen(self) -> None:
        text = (auswertung.REPO / "docs" / "pruefung" / "spieltest_m13_ergebnisse.md").read_text(encoding="utf-8")
        for name in auswertung.ABSCHNITTE:
            self.assertIn(f"<!-- auswertung:{name} -->", text)
            self.assertIn(f"<!-- /auswertung:{name} -->", text)


if __name__ == "__main__":
    unittest.main()
