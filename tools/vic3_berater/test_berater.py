"""Tests des Vic3-Beraters: nur Standardbibliothek, ohne Netz, ohne Bildschirm.

python3 -m unittest discover -s tools/vic3_berater -v
"""

from __future__ import annotations

import copy
import io
import os
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parent))
import beratung  # noqa: E402
import bildschirm  # noqa: E402
import spielstand  # noqa: E402

META = """meta_data={
\tsave_game_version=3
\tversion="1.9.2"
\tgame_date=1848.3.1
\tname="Kaiser {Test}"
\tflags={ a=yes }
}
"""
GAMESTATE = META + "date=1848.3.1\ncountry_manager={\n\tdatabase={ }\n}\n"


class SpielstandTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.ordner = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def datei(self, name: str, inhalt: bytes, mtime: float | None = None) -> Path:
        pfad = self.ordner / name
        pfad.write_bytes(inhalt)
        if mtime is not None:
            os.utime(pfad, (mtime, mtime))
        return pfad

    def test_klartext_mit_kopfzeile(self):
        pfad = self.datei("a.v3", b"SAV0103abcdef00000000\n" + GAMESTATE.encode())
        stand = spielstand.lese(pfad)
        self.assertTrue(stand.lesbar)
        self.assertEqual(stand.datum, "1848.3.1")
        self.assertTrue(stand.auszug.startswith("meta_data={"))
        self.assertTrue(stand.auszug.endswith("}"))
        self.assertNotIn("country_manager", stand.auszug)
        self.assertIn("1848.3.1", stand.als_kontext())

    def test_zip_mit_meta(self):
        puffer = io.BytesIO()
        with zipfile.ZipFile(puffer, "w") as z:
            z.writestr("meta", META)
            z.writestr("gamestate", GAMESTATE)
        pfad = self.datei("b.v3", b"SAV0103abcdef00000000\n" + puffer.getvalue())
        stand = spielstand.lese(pfad)
        self.assertTrue(stand.lesbar)
        self.assertEqual(stand.datum, "1848.3.1")

    def test_binaer(self):
        puffer = io.BytesIO()
        with zipfile.ZipFile(puffer, "w") as z:
            z.writestr("gamestate", bytes(range(256)) * 20)
        pfad = self.datei("c.v3", b"SAV0103abcdef00000000\n" + puffer.getvalue())
        stand = spielstand.lese(pfad)
        self.assertFalse(stand.lesbar)
        self.assertIn("binär", stand.als_kontext())

    def test_ohne_meta_nimmt_anfang_gekuerzt(self):
        text = "date=1836.1.1\n" + "x=1\n" * 5000
        stand = spielstand.lese(self.datei("d.v3", text.encode()))
        self.assertEqual(stand.datum, "1836.1.1")
        self.assertLessEqual(len(stand.auszug), spielstand.MAX_ZEICHEN + 20)
        self.assertTrue(stand.auszug.endswith("(gekürzt)"))

    def test_block_klammern_in_anfuehrungszeichen(self):
        text = 'vorher=1\nmeta_data={ name="a}b{" inner={ x=1 } }\nnachher=2\n'
        self.assertEqual(spielstand.block(text, "meta_data"), 'meta_data={ name="a}b{" inner={ x=1 } }')
        self.assertIsNone(spielstand.block(text, "fehlt"))
        self.assertIsNone(spielstand.block("meta_data={ offen", "meta_data"))

    def test_neuester(self):
        self.assertIsNone(spielstand.neuester(self.ordner))
        self.datei("alt.v3", b"x", mtime=1000)
        self.datei("neu.v3", b"x", mtime=2000)
        self.datei("anders.txt", b"x", mtime=3000)
        self.assertEqual(spielstand.neuester(self.ordner).name, "neu.v3")
        self.assertIsNone(spielstand.neuester(self.ordner / "gibt_es_nicht"))

    def test_beobachter_meldet_erst_stabile_neue_staende(self):
        self.datei("autosave.v3", b"alt", mtime=1000)
        b = spielstand.Beobachter(self.ordner)
        self.assertIsNone(b.pruefe())  # schon beim Start da
        self.datei("autosave.v3", b"neuer", mtime=2000)
        self.assertIsNone(b.pruefe())  # erst Kandidat
        self.assertEqual(b.pruefe().name, "autosave.v3")
        self.assertIsNone(b.pruefe())  # nur einmal gemeldet
        self.datei("autosave.v3", b"noch neuer", mtime=3000)
        self.assertIsNone(b.pruefe())
        self.datei("autosave.v3", b"noch viel neuer", mtime=4000)  # noch im Schreiben
        self.assertIsNone(b.pruefe())
        self.assertIsNotNone(b.pruefe())


class BildschirmTest(unittest.TestCase):
    def test_zielgroesse(self):
        self.assertEqual(bildschirm.zielgroesse(1920, 1080), (1920, 1080))
        self.assertEqual(bildschirm.zielgroesse(2560, 1440), (2560, 1440))
        b, h = bildschirm.zielgroesse(3840, 2160)
        self.assertLessEqual(max(b, h), bildschirm.MAX_KANTE)
        self.assertLessEqual(b * h, bildschirm.MAX_PIXEL)
        self.assertAlmostEqual(b / h, 16 / 9, places=2)
        self.assertEqual(bildschirm.zielgroesse(5120, 1440, max_kante=1280), (1280, 360))


class FakeStream:
    def __init__(self, teile, stop_reason):
        self.teile = teile
        self.stop_reason = stop_reason

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    @property
    def text_stream(self):
        yield from self.teile

    def get_final_message(self):
        inhalt = [SimpleNamespace(type="thinking", thinking="", signature="sig")]
        if self.teile:
            inhalt.append(SimpleNamespace(type="text", text="".join(self.teile)))
        return SimpleNamespace(content=inhalt, stop_reason=self.stop_reason)


class FakeClient:
    """Ersatz für anthropic.Anthropic: zeichnet die Anfragen auf und antwortet der Reihe nach."""

    def __init__(self, antworten):
        self.antworten = list(antworten)
        self.anfragen = []
        self.beta = SimpleNamespace(messages=SimpleNamespace(stream=self._stream))

    def _stream(self, **kw):
        self.anfragen.append(copy.deepcopy(kw))
        antwort = self.antworten.pop(0)
        if isinstance(antwort, Exception):
            raise antwort
        teile, stop = antwort if isinstance(antwort, tuple) else ([antwort], "end_turn")
        return FakeStream(teile, stop)


class BeraterTest(unittest.TestCase):
    def test_anfrage_und_streaming(self):
        client = FakeClient([(["Lage: ", "gut."], "end_turn")])
        berater = beratung.Berater(client, ziel="Großmacht werden")
        stuecke = []
        antwort = berater.frage(beratung.ANALYSE, bild=b"\xff\xd8jpeg", kontext="Spielstand X", bei_text=stuecke.append)
        self.assertEqual(antwort.text, "Lage: gut.")
        self.assertEqual(stuecke, ["Lage: ", "gut."])
        kw = client.anfragen[0]
        self.assertEqual(kw["model"], "claude-opus-5-5")
        self.assertEqual(kw["thinking"], {"type": "adaptive"})
        self.assertEqual(kw["output_config"], {"effort": "medium"})
        self.assertEqual(kw["fallbacks"], "default")
        self.assertEqual(kw["betas"], [beratung.FALLBACK_BETA])
        self.assertEqual(kw["system"], beratung.SYSTEM)
        inhalt = kw["messages"][0]["content"]
        self.assertEqual(inhalt[0]["type"], "image")
        self.assertEqual(inhalt[0]["source"]["media_type"], "image/jpeg")
        text = inhalt[1]["text"]
        self.assertIn("Großmacht werden", text)
        self.assertIn("Spielstand X", text)
        self.assertTrue(text.endswith(beratung.ANALYSE))

    def test_ohne_fallback_und_ohne_bild(self):
        client = FakeClient(["ok"])
        beratung.Berater(client, fallbacks=False, aufwand="high").frage("Welches Gesetz?")
        kw = client.anfragen[0]
        self.assertNotIn("fallbacks", kw)
        self.assertNotIn("betas", kw)
        self.assertEqual(kw["output_config"], {"effort": "high"})
        self.assertEqual([b["type"] for b in kw["messages"][0]["content"]], ["text"])

    def test_verlauf_wird_nur_angehaengt(self):
        client = FakeClient(["eins", "zwei", "drei"])
        berater = beratung.Berater(client, ziel="Ziel")
        for frage in ("a", "b", "c"):
            berater.frage(frage, bild=b"x")
        erste, zweite, dritte = (a["messages"] for a in client.anfragen)
        self.assertEqual(len(erste), 1)
        self.assertEqual(zweite[:1], erste)
        self.assertEqual(dritte[:3], zweite)
        self.assertEqual(len(dritte), 5)
        # Das Ziel steht nur in der ersten Nachricht.
        self.assertNotIn("Ziel", dritte[4]["content"][-1]["text"])
        # Die Antwort geht mit allen Blöcken (auch Denkblöcken) zurück in den Verlauf.
        self.assertEqual([b.type for b in berater.verlauf[1]["content"]], ["thinking", "text"])

    def test_neues_gespraech_mit_zusammenfassung(self):
        client = FakeClient(["Rat 1", "Rat 2", beratung.NICHTS_NEUES, "Rat 4"])
        berater = beratung.Berater(client, max_runden=3, ziel="Ziel")
        for frage in ("a", "b", "c", "d"):
            berater.frage(frage, bild=b"x")
        letzte = client.anfragen[3]["messages"]
        self.assertEqual(len(letzte), 1)
        text = letzte[0]["content"][-1]["text"]
        self.assertIn("Ziel", text)
        self.assertIn("Rat 1", text)
        self.assertIn("Rat 2", text)
        self.assertNotIn(beratung.NICHTS_NEUES, text)
        # Danach geht der Vorspann nicht noch einmal mit.
        self.assertIsNone(berater._zusammenfassung)

    def test_ablehnung_und_fehler_lassen_verlauf_sauber(self):
        client = FakeClient([([], "refusal"), RuntimeError("Netz weg"), "ok"])
        berater = beratung.Berater(client)
        antwort = berater.frage("a")
        self.assertTrue(antwort.abgelehnt)
        self.assertEqual(berater.verlauf, [])
        with self.assertRaises(RuntimeError):
            berater.frage("b")
        self.assertEqual(berater.verlauf, [])
        berater.frage("c")
        self.assertEqual([m["role"] for m in berater.verlauf], ["user", "assistant"])

    def test_nichts_neues_und_abgeschnitten(self):
        client = FakeClient([f"  {beratung.NICHTS_NEUES}", (["lang"], "max_tokens")])
        berater = beratung.Berater(client)
        self.assertTrue(berater.frage(beratung.AUTO).nichts_neues)
        antwort = berater.frage("b")
        self.assertTrue(antwort.abgeschnitten)
        self.assertFalse(antwort.nichts_neues)


class ArgumenteTest(unittest.TestCase):
    def test_standardwerte(self):
        import berater

        args = berater.argumente([])
        self.assertEqual(args.aufwand, "medium")
        self.assertEqual(args.taste_analyse, "<f9>")
        self.assertEqual(berater.argumente(["--deckkraft", "5"]).deckkraft, 1.0)
        self.assertEqual(berater.argumente(["--spielstaende", ""]).spielstaende, "")


if __name__ == "__main__":
    unittest.main()
