"""Tests für tools/paket (ohne Godot): Version, Vorlagen, Texte, Rauchtest-Auswertung.

    python3 -m unittest discover -s tools/paket -v
"""

from __future__ import annotations

import configparser
import re
import sys
import unittest
from pathlib import Path

HIER = Path(__file__).resolve().parent
if str(HIER) not in sys.path:
    sys.path.insert(0, str(HIER))

import paket  # noqa: E402


def _vorlagen() -> dict[str, dict[str, str]]:
    """export_presets.cfg: Name der Vorlage → Einstellungen (Werte ohne Anführungszeichen)."""
    cfg = configparser.ConfigParser(interpolation=None, strict=False)
    cfg.read(paket.SPIEL / "export_presets.cfg", encoding="utf-8")
    ergebnis: dict[str, dict[str, str]] = {}
    for abschnitt in cfg.sections():
        if re.fullmatch(r"preset\.\d+", abschnitt):
            werte = {k: v.strip('"') for k, v in cfg[abschnitt].items()}
            werte.update({k: v.strip('"') for k, v in cfg[abschnitt + ".options"].items()})
            ergebnis[werte["name"]] = werte
    return ergebnis


class Version(unittest.TestCase):
    def test_version_wie_cmake(self) -> None:
        cmake = (paket.REPO / "CMakeLists.txt").read_text(encoding="utf-8")
        treffer = re.search(r"VERSION (\d+\.\d+\.\d+)", cmake)
        self.assertIsNotNone(treffer)
        self.assertEqual(paket.version(), treffer.group(1))

    def test_windows_und_macos_tragen_die_version(self) -> None:
        vorlagen = _vorlagen()
        ver = paket.version()
        self.assertEqual(vorlagen["Windows"]["application/product_version"], ver + ".0")
        self.assertEqual(vorlagen["macOS"]["application/short_version"], ver)
        self.assertEqual(vorlagen["macOS"]["application/version"], ver)


class Vorlagen(unittest.TestCase):
    def test_jedes_system_hat_eine_vorlage(self) -> None:
        vorlagen = _vorlagen()
        for system, (name, datei, bibliothek) in paket.SYSTEME.items():
            with self.subTest(system=system):
                self.assertIn(name, vorlagen)
                self.assertEqual(Path(vorlagen[name]["export_path"]).name, datei)
                self.assertIn(f'"res://bin/{bibliothek}"', (paket.SPIEL / "bin" / "uhrwerk.gdextension").read_text())

    def test_tests_und_noten_bleiben_draussen(self) -> None:
        for name, werte in _vorlagen().items():
            with self.subTest(vorlage=name):
                self.assertIn("tests/*", werte["exclude_filter"])
                self.assertIn("*.cfg", werte["include_filter"])  # tutorial/hinweise.cfg, audio/musik/stuecke.cfg
                self.assertEqual(werte.get("binary_format/embed_pck", "false"), "false")


class Texte(unittest.TestCase):
    def test_liesmich_je_system(self) -> None:
        for system in paket.SYSTEME:
            text = paket.liesmich(system, "1.2.3")
            self.assertIn("EINIGKEIT 1.2.3", text)
            self.assertIn("DRITTANBIETER.txt", text)
            self.assertIn("Rückmeldung", text)

    def test_geodaten_mit_namensnennung(self) -> None:
        text = paket.geodaten_hinweise()
        self.assertIn("GeoNames (https://www.geonames.org)", text)
        self.assertIn("CC BY 4.0", text)
        self.assertIn("Made with Natural Earth", text)
        self.assertIn("Terrain Tiles", text)
        self.assertIn("verändert", text)
        self.assertNotIn("boundary lines", text)  # nur Vorschau, nicht im Spiel
        self.assertTrue(all(len(z) <= 78 for z in text.splitlines()))

    def test_lizenz_polyform_mit_hinweis(self) -> None:
        text = paket.LIZENZ.read_text(encoding="utf-8")
        self.assertTrue(text.startswith("Required Notice: Copyright"))
        self.assertIn("# PolyForm Noncommercial License 1.0.0", text)
        self.assertIn("PolyForm Noncommercial", paket.liesmich("linux", "1.2.3"))

    def test_schriftlizenzen_vorhanden(self) -> None:
        schriften = {p.stem for p in (paket.SPIEL / "assets" / "schriften").glob("*.ttf")}
        lizenzen = {p.stem.removeprefix("OFL_") for p in (paket.SPIEL / "assets" / "schriften").glob("OFL_*.txt")}
        for schrift in schriften:
            self.assertIn(schrift.split("-")[0], lizenzen)


class Rauchtest(unittest.TestCase):
    def test_pruefsumme_aus_der_ausgabe(self) -> None:
        ausgabe = (
            "EINIGKEIT: Datenordner /x/data (neben der Programmdatei)\n"
            "EINIGKEIT: Rauchtest voelkerfruehling 31. März 1848 Version 0.13.0 Prüfsumme c2cbdc628b6c1433\n"
        )
        self.assertEqual(
            paket._pruefsumme(ausgabe, r"Rauchtest \S+ .* Prüfsumme ([0-9a-f]{16})"), "c2cbdc628b6c1433"
        )
        self.assertIsNone(paket._pruefsumme("nichts", r"Prüfsumme: ([0-9a-f]{16})"))
        cli = "…\nPrüfsumme: 5580eeb53240eab2\n"
        self.assertEqual(paket._pruefsumme(cli, r"Prüfsumme: ([0-9a-f]{16})"), "5580eeb53240eab2")


if __name__ == "__main__":
    unittest.main()
