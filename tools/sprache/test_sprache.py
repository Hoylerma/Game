"""Tests für tools/sprache (Sammeln der Texte, gettext, Prüfung).

    python3 -m unittest discover -s tools/sprache -v
"""

from __future__ import annotations

import io
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

HIER = Path(__file__).resolve().parent
if str(HIER) not in sys.path:
    sys.path.insert(0, str(HIER))

import sprache  # noqa: E402

SKRIPT = '''extends Control
const TEXTE := {"tag": "PRU", "text": "Zollverein und Revolution"}
func _ready() -> void:
	var b := Button.new()
	b.name = "NewGame"
	b.text = "Neues Spiel"
	_knopf("Fortsetzen", "Continue")
	label.text = tr("Lade {name} …").format({"name": x})
	print("Ausgabe, nicht übersetzen")
	var key := "ton/musik"
	var id := &"Kabinett"
	var lang := (
		"Ein langer Satz, "
		+ "über zwei Zeilen."
	)
	var fmt := "%s – %s" % [a, b]
	var tab := "Tab%d" % 1
	var leer := "Schließen"  # Kommentar mit "Text im Kommentar"
	var reihe := ["tempo_pause", "Pause"]
'''


def projekt(ordner: Path, skript: str = SKRIPT) -> Path:
    (ordner / "scripts").mkdir(parents=True)
    (ordner / "scenes").mkdir()
    (ordner / "tutorial").mkdir()
    (ordner / "project.godot").write_text("", encoding="utf-8")
    (ordner / "scripts" / "probe.gd").write_text(skript, encoding="utf-8")
    (ordner / "scenes" / "probe.tscn").write_text('[node name="A" type="Label"]\ntext = "Aus der Szene"\n', encoding="utf-8")
    (ordner / "tutorial" / "h.cfg").write_text('[X]\ntitel="Hinweis"\ntext="Zwei Sätze. Mit \\"Zitat\\"."\n', encoding="utf-8")
    return ordner


class SammelnTest(unittest.TestCase):
    def test_ist_text(self) -> None:
        for wahr in ("Neues Spiel", "Schließen", "Lade {name} …", "Größe der Oberfläche", "„März 1848“"):
            self.assertTrue(sprache.ist_text(wahr), wahr)
        for falsch in ("ton/musik", "NewGame", "tempo_pause", "%s – %s", "Tab%d", "PRU", "res://x.gd", "", "abc", "3.0"):
            self.assertFalse(sprache.ist_text(falsch), falsch)

    def test_sammeln(self) -> None:
        with tempfile.TemporaryDirectory() as ordner:
            e = sprache.sammle(projekt(Path(ordner)))
        texte = set(e)
        for erwartet in ("Neues Spiel", "Fortsetzen", "Lade {name} …", "Ein langer Satz, über zwei Zeilen.", "Schließen",
                         "Pause", "Zollverein und Revolution", "Aus der Szene", "Hinweis", 'Zwei Sätze. Mit "Zitat".'):
            self.assertIn(erwartet, texte)
        for nicht in ("NewGame", "Continue", "Ausgabe, nicht übersetzen", "ton/musik", "Kabinett", "%s – %s", "Tab%d",
                      "Text im Kommentar", "tempo_pause", "PRU", "tag", "text"):
            self.assertNotIn(nicht, texte)
        self.assertEqual(e["Neues Spiel"].orte, ["scripts/probe.gd:6"])

    def test_po_hin_und_zurueck(self) -> None:
        e = {m: sprache.Eintrag(m, ["a.gd:1"]) for m in ("Ein\nzwei", 'Mit "Zitat" und \\', "Leer")}
        with tempfile.TemporaryDirectory() as ordner:
            pfad = Path(ordner) / "en.po"
            pfad.write_text(sprache.schreibe_po(e, {"Ein\nzwei": "One\ntwo", 'Mit "Zitat" und \\': 'With "quote" and \\'}, "en"),
                            encoding="utf-8")
            gelesen = sprache.lies_po(pfad)
        self.assertEqual(gelesen, {"Ein\nzwei": "One\ntwo", 'Mit "Zitat" und \\': 'With "quote" and \\'})


class KommandozeileTest(unittest.TestCase):
    def lauf(self, *args: str) -> tuple[int, str]:
        aus = io.StringIO()
        with redirect_stdout(aus), redirect_stderr(aus):
            code = sprache.main(list(args))
        return code, aus.getvalue()

    def test_schreiben_und_pruefen(self) -> None:
        with tempfile.TemporaryDirectory() as ordner:
            wurzel = projekt(Path(ordner))
            code, text = self.lauf("--spiel", str(wurzel))
            self.assertEqual(code, 0, text)
            for name in ("einigkeit.pot", "de.po", "en.po"):
                self.assertTrue((wurzel / "locale" / name).is_file())
            self.assertIn('msgid "Neues Spiel"', (wurzel / "locale" / "einigkeit.pot").read_text(encoding="utf-8"))
            code, text = self.lauf("--spiel", str(wurzel), "--pruefen")
            self.assertEqual(code, 0, text)  # keine Pflichtdateien im Probeprojekt
            (wurzel / "scripts" / "probe.gd").write_text(SKRIPT + '\tb.text = "Ganz neu"\n', encoding="utf-8")
            code, text = self.lauf("--spiel", str(wurzel), "--pruefen")
            self.assertEqual(code, 1)
            self.assertIn("einigkeit.pot ist nicht aktuell", text)

    def test_fehler(self) -> None:
        with tempfile.TemporaryDirectory() as ordner:
            code, text = self.lauf("--spiel", ordner)
        self.assertEqual(code, 2)
        self.assertIn("kein Godot-Projekt", text)

    def test_spiel_ist_aktuell_und_englisch_vollstaendig(self) -> None:
        code, text = self.lauf("--pruefen")
        self.assertEqual(code, 0, text)


if __name__ == "__main__":
    unittest.main()
