"""Tests für tools/klang (Notentext, Synthesizer, Bedienklänge, Kommandozeile).

    python3 -m unittest discover -s tools/klang -v
"""

from __future__ import annotations

import io
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from fractions import Fraction as F
from pathlib import Path

HIER = Path(__file__).resolve().parent
if str(HIER) not in sys.path:
    sys.path.insert(0, str(HIER))

import numpy as np  # noqa: E402

import klaenge  # noqa: E402
import klang  # noqa: E402
import noten  # noqa: E402
import synth  # noqa: E402

KLEIN = """
titel     = "Probe"
komponist = "Niemand"
stimmung  = frieden
tempo     = 120
takt      = 4/4
stimme melodie { instrument = klavier lautstaerke = 0.8 }
stimme bass    { instrument = streicher }
teil a {
    melodie = "a4:4 a4 [a4 cis5 e5]:2 |"
    bass    = "a2:1 |"
}
form = "a a"
"""


def schreibe(ordner: Path, text: str, name: str = "probe") -> Path:
    pfad = ordner / f"{name}.noten"
    pfad.write_text(text, encoding="utf-8")
    return pfad


class NotenTest(unittest.TestCase):
    def test_tonhoehen(self) -> None:
        self.assertEqual(noten.tonhoehe("c", 4), 60)
        self.assertEqual(noten.tonhoehe("a", 4), 69)
        self.assertEqual(noten.tonhoehe("h", 3), 59)
        self.assertEqual(noten.tonhoehe("b", 3), 58)
        self.assertEqual(noten.tonhoehe("es", 5), noten.tonhoehe("dis", 5))
        self.assertEqual(noten.tonhoehe("as", 4), noten.tonhoehe("gis", 4))

    def test_zeile(self) -> None:
        e = noten.lies_zeile("fis5:4 fis5 g5:8. a5:16 r:4 | [d4 fis4 a4]:2 e5:4t e5 e5 | r:2 d5:2~ | d5:4 r:2. |", F(1))
        self.assertEqual([x.tonhoehen for x in e[:5]], [(78,), (78,), (79,), (81,), ()])
        self.assertEqual([x.dauer for x in e[:5]], [F(1, 4), F(1, 4), F(3, 16), F(1, 16), F(1, 4)])
        self.assertEqual(e[5].tonhoehen, (62, 66, 69))
        self.assertEqual([x.dauer for x in e[6:9]], [F(1, 6)] * 3)  # Vierteltriole
        self.assertEqual(e[10].tonhoehen, (74,))
        self.assertEqual(e[10].dauer, F(3, 4))  # gebunden über den Taktstrich
        self.assertEqual(e[10].beginn, F(5, 2))
        self.assertEqual(e[-1].tonhoehen, ())

    def test_auftakt_und_schlusstakt_duerfen_kurz_sein(self) -> None:
        e = noten.lies_zeile("e4:8 | a4:8 e4 a4 c5 a4 c5 | a4:8 e4 gis4 a4 r |", F(6, 8))
        self.assertEqual(sum((x.dauer for x in e), F(0)), F(1, 8) + F(6, 8) + F(5, 8))

    def test_fehler_in_zeilen(self) -> None:
        faelle = {
            "c4:4 | c4 c4 c4 | c4": "Takt 2 ist 3/4 lang statt 1",
            "c4:4 c4 c4 c4 c4 |": "Takt 1 ist 5/4 lang",
            "x4:4": "unbekannte Note 'x4:4'",
            "c:4": "Oktave fehlt",
            "c4:3": "Dauer :3",
            "c4:4~ d4": "bindet 'd4' an eine andere Note",
            "c4:4~": "'~' am Ende der Zeile",
            "[]:4": "leerer Akkord",
            "[c4 r]:4": "Pause im Akkord",
        }
        for text, meldung in faelle.items():
            with self.subTest(text=text), self.assertRaises(noten.NotenFehler) as fehler:
                noten.lies_zeile(text, F(1), "x.noten", 3, "m")
            self.assertIn(meldung, str(fehler.exception))
            self.assertIn("x.noten:3: Stimme m", str(fehler.exception))

    def test_stueck(self) -> None:
        with tempfile.TemporaryDirectory() as ordner:
            s = noten.lies(schreibe(Path(ordner), KLEIN))
        self.assertEqual((s.name, s.titel, s.stimmung, s.tempo, s.takt), ("probe", "Probe", "frieden", 120.0, F(1)))
        with tempfile.TemporaryDirectory() as ordner:
            text = KLEIN.replace("stimmung  = frieden", 'stimmung  = "frieden, krise"')
            mehrere = noten.lies(schreibe(Path(ordner), text))
        self.assertEqual((mehrere.stimmung, mehrere.stimmungen), ("frieden krise", ("frieden", "krise")))
        self.assertEqual(s.stimmen["bass"].lautstaerke, 0.7)
        self.assertEqual(s.form, ["a", "a"])
        self.assertEqual(s.dauer_ganze(), F(2))
        self.assertAlmostEqual(s.dauer_sekunden(), 4.0)

    def test_fehler_im_stueck(self) -> None:
        faelle = [
            (KLEIN.replace('stimmung  = frieden', 'stimmung  = heiter'), "Stimmung 'heiter' unbekannt"),
            (KLEIN.replace('stimmung  = frieden', 'stimmung  = "frieden heiter"'), "Stimmung 'heiter' unbekannt"),
            (KLEIN.replace('stimmung  = frieden', 'stimmung  = "krise krise"'), "Stimmung doppelt"),
            (KLEIN.replace("instrument = klavier", "instrument = orgel"), "Instrument 'orgel' unbekannt"),
            (KLEIN.replace('bass    = "a2:1 |"', 'bass    = "a2:2 |"'), "Stimme bass ist 8 Sechzehntel lang"),
            (KLEIN.replace('form = "a a"', 'form = "a b"'), "Form nennt unbekannten Teil 'b'"),
            (KLEIN.replace("tempo     = 120\n", ""), "Pflichtfeld 'tempo' fehlt"),
            (KLEIN.replace('melodie = "a4', 'geige = "a4'), "unbekannte Stimme 'geige'"),
            (KLEIN + "\nfarbe = rot\n", "unbekanntes Feld 'farbe'"),
            (KLEIN.replace('titel     = "Probe"', 'titel     = "Probe'), "Stimme"),
        ]
        for text, meldung in faelle:
            with self.subTest(meldung=meldung), tempfile.TemporaryDirectory() as ordner:
                with self.assertRaises(noten.NotenFehler) as fehler:
                    noten.lies(schreibe(Path(ordner), text))
                if meldung != "Stimme":
                    self.assertIn(meldung, str(fehler.exception))

    def test_stuecke_des_spiels(self) -> None:
        alle = klang.stuecke(klang.NOTEN)
        self.assertGreaterEqual(len(alle), 6)
        stimmungen = {st for s in alle for st in s.stimmungen}
        self.assertEqual(stimmungen, set(noten.STIMMUNGEN))
        # Spieltest 11/2026: jede Stimmung mit mindestens zwei Stücken außer der Revolution.
        for stimmung in ("frieden", "krise", "krieg"):
            self.assertGreaterEqual(sum(stimmung in s.stimmungen for s in alle), 2, stimmung)
        for s in alle:
            with self.subTest(stueck=s.name):
                self.assertTrue(s.titel and s.komponist and s.werk)
                self.assertTrue(40.0 <= s.dauer_sekunden() <= 240.0, s.dauer_sekunden())
                kopf = s.quelle.read_text(encoding="utf-8").split("titel", 1)[0]
                self.assertIn("gemeinfrei", kopf)  # Herkunft und Rechte stehen im Kopf jeder Datei


class SynthTest(unittest.TestCase):
    def test_tabellen(self) -> None:
        x = np.array([0.0, 0.25, 0.5, 0.75, 1.25, -0.25])
        np.testing.assert_allclose(synth.sinus(x), [0, 1, 0, -1, 1, -1], atol=1e-7)
        np.testing.assert_allclose(synth.abfall(np.array([0.0, 1.0, 5.0])), np.exp([-0.0, -1.0, -5.0]), rtol=1e-5)
        self.assertEqual(float(synth.abfall(np.array([1e9]))[0]), synth._EXP[-2])
        self.assertAlmostEqual(synth.frequenz(69), 440.0)
        self.assertAlmostEqual(synth.frequenz(60), 261.6256, places=3)
        self.assertAlmostEqual(synth.frequenz(81), 880.0)

    def test_gleitmittel_und_rauschen(self) -> None:
        np.testing.assert_allclose(synth.gleitmittel(np.array([3.0, 3.0, 6.0, 0.0]), 2), [1.5, 3.0, 4.5, 3.0])
        r = synth.rauschen(1000, 7)
        self.assertTrue(np.array_equal(r, synth.rauschen(1000, 7)))
        self.assertTrue(-1.0 <= r.min() and r.max() <= 1.0)

    def test_jedes_instrument_klingt_in_der_richtigen_hoehe(self) -> None:
        for name in ("klavier", "streicher", "holz", "blech"):
            with self.subTest(instrument=name):
                x = synth.INSTRUMENTE[name](440.0, synth.RATE, 0.8, 1)
                self.assertTrue(np.all(np.isfinite(x)))
                seg = x[4000 : 4000 + 32768] * np.hanning(32768)
                spektrum = np.abs(np.fft.rfft(seg))
                spitze = float(np.fft.rfftfreq(32768, 1 / synth.RATE)[int(np.argmax(spektrum))])
                self.assertAlmostEqual(spitze, 440.0, delta=3.0)
        for name in ("pauke", "trommel"):
            x = synth.INSTRUMENTE[name](110.0, 1000, 0.8, 1)
            self.assertGreater(float(np.max(np.abs(x))), 0.1)

    def test_hall_und_normierung(self) -> None:
        impuls = np.zeros(8000)
        impuls[0] = 1.0
        nass = synth.hall(impuls, 0.3)
        self.assertGreater(float(np.sum(np.abs(nass[1400:]))), 0.01)  # Nachhall nach den Verzögerungen
        self.assertTrue(np.array_equal(synth.hall(impuls, 0.0), impuls))
        n = synth.normiere(np.array([0.0, 0.5, -2.0]))
        self.assertEqual(n.dtype, np.int16)
        self.assertEqual(int(np.max(np.abs(n))), round(0.89 * 32767))

    def test_stueck_ist_reproduzierbar(self) -> None:
        with tempfile.TemporaryDirectory() as ordner:
            s = noten.lies(schreibe(Path(ordner), KLEIN))
        a = synth.spiele(s)
        b = synth.spiele(s)
        self.assertTrue(np.array_equal(a, b))
        self.assertEqual(len(a), 4 * synth.RATE + int(2.5 * synth.RATE))
        # Feste Prüfsumme: ändert sich der Synthesizer, ändert sich die Musik des Spiels.
        self.assertEqual(klang.pruefsumme(a)[:16], PROBE_PRUEFSUMME)


class KlaengeTest(unittest.TestCase):
    def test_alle_klaenge(self) -> None:
        self.assertEqual(set(klaenge.KLAENGE), {"gaensekiel", "siegel", "papier", "glocke", "trommel", "telegraf"})
        for name, fn in klaenge.KLAENGE.items():
            with self.subTest(klang=name):
                x = fn()
                self.assertEqual(x.dtype, np.int16)
                self.assertTrue(0.2 <= len(x) / synth.RATE <= 3.0)
                self.assertGreater(int(np.max(np.abs(x))), 8000)
                self.assertTrue(np.array_equal(x, fn()))


class KommandozeileTest(unittest.TestCase):
    def lauf(self, *args: str) -> tuple[int, str]:
        aus = io.StringIO()
        with redirect_stdout(aus), redirect_stderr(aus):
            code = klang.main(list(args))
        return code, aus.getvalue()

    def test_rendern_pruefen_und_abweichung(self) -> None:
        with tempfile.TemporaryDirectory() as ordner:
            wurzel = Path(ordner)
            (wurzel / "noten").mkdir()
            schreibe(wurzel / "noten", KLEIN)
            basis = ("--noten", str(wurzel / "noten"), "--ausgabe", str(wurzel / "aus"), "--nur", "probe", "papier")
            code, text = self.lauf(*basis, "--pruefsummen")
            self.assertEqual(code, 0, text)
            self.assertTrue((wurzel / "aus" / "musik" / "probe.wav").is_file())
            self.assertTrue((wurzel / "aus" / "klaenge" / "papier.wav").is_file())
            liste = (wurzel / "aus" / "musik" / "stuecke.cfg").read_text(encoding="utf-8")
            self.assertIn('[probe]\ntitel="Probe"\nkomponist="Niemand"', liste)
            self.assertIn('stimmung="frieden"\ndauer=4.0', liste)
            summen = klang.lies_pruefsummen(wurzel / "aus" / "pruefsummen.txt")
            self.assertEqual(set(summen), {"probe", "papier"})
            code, text = self.lauf(*basis, "--pruefen")
            self.assertEqual(code, 0, text)
            self.assertIn("gleich", text)
            code, text = self.lauf(*basis)  # nichts zu tun
            self.assertEqual(code, 0, text)
            self.assertIn("musik/probe: aktuell", text)
            # Eine geänderte Note ändert die Prüfsumme.
            schreibe(wurzel / "noten", KLEIN.replace("a4:4 a4", "a4:4 h4"))
            code, text = self.lauf(*basis, "--pruefen")
            self.assertEqual(code, 1, text)
            self.assertIn("probe: Prüfsumme weicht ab", text)

    def test_fehler(self) -> None:
        with tempfile.TemporaryDirectory() as ordner:
            wurzel = Path(ordner)
            code, text = self.lauf("--noten", str(wurzel), "--ausgabe", str(wurzel))
            self.assertEqual(code, 2)
            self.assertIn("keine .noten-Dateien", text)
            schreibe(wurzel, KLEIN.replace("a2:1", "a2:2"))
            code, text = self.lauf("--noten", str(wurzel), "--ausgabe", str(wurzel))
            self.assertEqual(code, 2)
            self.assertIn("probe.noten", text)
            schreibe(wurzel, KLEIN)
            code, text = self.lauf("--noten", str(wurzel), "--ausgabe", str(wurzel), "--nur", "gibtsnicht")
            self.assertEqual(code, 2)
            self.assertIn("unbekannt: gibtsnicht", text)

    def test_pruefsummen_des_spiels_sind_vollstaendig(self) -> None:
        summen = klang.lies_pruefsummen(klang.PRUEFSUMMEN)
        namen = {s.name for s in klang.stuecke(klang.NOTEN)} | set(klaenge.KLAENGE)
        self.assertEqual(set(summen), namen)


PROBE_PRUEFSUMME = "53eb11928afcc867"

if __name__ == "__main__":
    unittest.main()
