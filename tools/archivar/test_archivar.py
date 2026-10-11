"""Tests für den Archivar (python3 -m unittest discover -s tools/archivar)."""

from __future__ import annotations

import contextlib
import io
import shutil
import tempfile
import unittest
from pathlib import Path

import archivar as ar
import chron

TESTDATEN = Path(__file__).resolve().parent / "testdaten"


def _ausfuehren(*argumente: str) -> tuple[int, str, str]:
    aus, fehler = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(aus), contextlib.redirect_stderr(fehler):
        code = ar.main(list(argumente))
    return code, aus.getvalue(), fehler.getvalue()


class MitKopie(unittest.TestCase):
    """Jeder Test arbeitet auf einer Kopie der Testdaten."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.daten = Path(self._tmp.name) / "data"
        shutil.copytree(TESTDATEN, self.daten)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def tabelle(self, name: str) -> Path:
        return self.daten / "tabellen" / name

    def ersetze(self, name: str, alt: str, neu: str) -> None:
        pfad = self.tabelle(name)
        text = pfad.read_text(encoding="utf-8")
        self.assertIn(alt, text)
        pfad.write_text(text.replace(alt, neu, 1), encoding="utf-8")

    def anhaengen(self, name: str, zeile: str) -> None:
        with open(self.tabelle(name), "a", encoding="utf-8") as f:
            f.write(zeile + "\n")

    def pruefen(self) -> tuple[int, str, str]:
        return _ausfuehren("--daten", str(self.daten), "--pruefen")

    def erwarte_fehler(self, text: str) -> None:
        code, _, fehler = self.pruefen()
        self.assertEqual(code, 2, fehler)
        self.assertIn(text, fehler)

    def erwarte_warnung(self, text: str) -> None:
        code, _, fehler = _ausfuehren("--daten", str(self.daten), "--bericht")
        self.assertEqual(code, 0, fehler)
        self.assertIn(f"Warnung: {text}", fehler)


# ─── Erzeugung ──────────────────────────────────────────────────────────────


class TestErzeugung(MitKopie):
    def test_testdaten_sind_aktuell(self) -> None:
        # Die eingecheckte Ausgabe unter testdaten/history ist die Referenz.
        code, aus, fehler = self.pruefen()
        self.assertEqual((code, fehler), (0, ""), aus)
        self.assertIn("history/ ist aktuell (6 Dateien, 4 Orte, 2 Kreise, 2 Staaten)", aus)

    def test_deterministisch(self) -> None:
        m = ar.Meldungen()
        bezug = ar.lade_bezug(self.daten, m)
        eins = ar.erzeuge(ar.baue_archiv(self.daten / "tabellen", bezug, m))
        zwei = ar.erzeuge(ar.baue_archiv(self.daten / "tabellen", bezug, m))
        self.assertEqual(eins, zwei)
        self.assertEqual(m.fehler, [])

    def test_ausgabe_ist_gueltiges_chronik_skript(self) -> None:
        for pfad in sorted((self.daten / "history").rglob("*.chron")):
            ergebnis = chron.parse_script_file(pfad)
            self.assertTrue(ergebnis.ok, [str(e) for e in ergebnis.errors])

    def test_datierte_bloecke_chronologisch_und_zusammengefasst(self) -> None:
        text = (self.daten / "history/orte/nord.chron").read_text(encoding="utf-8")
        root = chron.parse_script(text).root
        heide = [n for n in root.children if n.value == "heide"][0]
        bloecke = [k.key for k in heide.children if k.is_block and k.key[:1].isdigit()]
        self.assertEqual(bloecke, ["1648.10.24", "1701.01.18"])
        erster = heide.find("1648.10.24")
        assert erster is not None
        self.assertEqual([k.key for k in erster.children], ["besitzer", "bevoelkerung", "konfession", "kultur", "verwuestung"])

    def test_escapes_und_kopfkommentar(self) -> None:
        text = (self.daten / "history/staaten/BBB_suedland.chron").read_text(encoding="utf-8")
        self.assertIn('name     = "Königreich \\"Südland\\""', text)
        self.assertIn('quelle   = "Testdaten mit \\\\ Rückstrich"', text)
        nord = (self.daten / "history/orte/nord.chron").read_text(encoding="utf-8")
        self.assertTrue(nord.startswith(f"# {ar.KOPFZEILE}\n#\n# Orte im Norden.\n"))
        root = chron.parse_script(text).root
        self.assertEqual(root.find("staat").get_string("name"), 'Königreich "Südland"')

    def test_schreiben_aktualisiert_und_entfernt_ueberzaehlige(self) -> None:
        (self.daten / "history/orte/alt.chron").write_text("ort alt { }\n", encoding="utf-8")
        self.ersetze("orte.csv", "Heide-Süd", "Heide-Nord")
        code, aus, _ = self.pruefen()
        self.assertEqual(code, 1)
        self.assertIn("veraltet: history/orte/nord.chron", aus)
        self.assertIn("überzählig: history/orte/alt.chron", aus)
        code, aus, _ = _ausfuehren("--daten", str(self.daten))
        self.assertEqual(code, 0)
        self.assertIn("(1 geändert, 1 entfernt)", aus)
        self.assertFalse((self.daten / "history/orte/alt.chron").exists())
        self.assertEqual(self.pruefen()[0], 0)

    def test_fehlende_ausgabe(self) -> None:
        (self.daten / "history/orte/sued.chron").unlink()
        code, aus, _ = self.pruefen()
        self.assertEqual(code, 1)
        self.assertIn("fehlt: history/orte/sued.chron", aus)

    def test_bericht(self) -> None:
        code, aus, _ = _ausfuehren("--daten", str(self.daten), "--bericht")
        self.assertEqual(code, 0)
        self.assertIn("besitz.csv                 4 Zeilen  (gesichert 3, ungeprueft 1)", aus)
        self.assertIn("krone (1701.01.18): 3 Orte mit Daten, 1 ohne Daten, 2 Staaten mit Orten", aus)
        self.assertIn("Orte: 4, Kreise: 2, Staaten: 2", aus)


# ─── Prüfungen ──────────────────────────────────────────────────────────────


class TestFehler(MitKopie):
    def test_kopfzeile(self) -> None:
        self.ersetze("orte.csv", "id;datei;name", "kennung;datei;name")
        self.erwarte_fehler("tabellen/orte.csv:1: Kopfzeile erwartet: id;datei;name")

    def test_spaltenzahl(self) -> None:
        self.anhaengen("besitz.csv", "burg;1700.01.01;AAA")
        self.erwarte_fehler("tabellen/besitz.csv:6: erwartet 5 Spalten, gefunden 3")

    def test_tabelle_fehlt(self) -> None:
        self.tabelle("besitz.csv").unlink()
        self.erwarte_fehler("tabellen/besitz.csv: Tabelle fehlt")

    def test_verweise(self) -> None:
        faelle = {
            ("besitz.csv", "burg;1648.10.24;BBB", "burg;1648.10.24;XYZ"): "unbekannter Staat 'XYZ' als Besitzer",
            ("besitz.csv", "burg;1648.10.24;BBB", "turm;1648.10.24;BBB"): "unbekannter Ort 'turm'",
            ("orte.csv", "burg;sued;Burg;flachland", "burg;sued;Burg;sumpf"): "unbekanntes Gelände 'sumpf'",
            ("staaten_historie.csv", "hauptstadt;burg", "hauptstadt;atlantis"): "unbekannter Ort 'atlantis' als Hauptstadt",
            ("staaten_historie.csv", "konfession;katholisch", "konfession;orthodox"): "unbekannte Konfession 'orthodox'",
            ("bevoelkerung.csv", "katholisch=100;suedleute", "orthodox=100;suedleute"): "unbekannte Konfession 'orthodox'",
            ("bevoelkerung.csv", "suedleute=80 nordleute=20", "suedleute=80 westleute=20"): "unbekannte Kultur 'westleute'",
            ("dateien.csv", "orte;nord;nordleute=100", "orte;nord;ostleute=100"): "unbekannte Kultur 'ostleute'",
        }
        for (tabelle, alt, neu), text in faelle.items():
            with self.subTest(text):
                self.tearDown()  # frische Kopie je Fall
                self.setUp()
                self.ersetze(tabelle, alt, neu)
                self.erwarte_fehler(text)

    def test_werte(self) -> None:
        faelle = {
            ("besitz.csv", "burg;1648.10.24", "burg;1648.13.24"): "ungültiges Datum '1648.13.24' in Spalte 'ab'",
            ("besitz.csv", "burg;1648.10.24", "burg;0000.01.01"): "ungültiges Datum '0000.01.01' in Spalte 'ab'",
            ("orte.csv", "11.00;53.50;250", "11.00;53.50;0.000001"): "Fläche 0.000001 km² ist für den Kern zu klein",
            ("besitz.csv", "burg;1648.10.24;BBB;gesichert", "burg;1648.10.24;BBB;sicher"): "ungültige Sicherheit 'sicher'",
            ("besitz.csv", "burg;1648.10.24;BBB;gesichert;Testquelle", "burg;1648.10.24;BBB;gesichert;"): "Spalte 'quelle' darf nicht leer sein",
            ("bevoelkerung.csv", "lutherisch=90 katholisch=10", "lutherisch=90 katholisch=20"): "ergeben 110 %",
            ("bevoelkerung.csv", "lutherisch=90 katholisch=10", "lutherisch 90"): "ungültige Angabe in Spalte 'konfession'",
            ("bevoelkerung.csv", "suedleute=80 nordleute=20", "suedleute=80 nordleute=30"): "Anteile in Spalte 'kultur' ergeben 110 %",
            ("bevoelkerung.csv", "suedleute=80 nordleute=20", "suedleute 80"): "ungültige Angabe in Spalte 'kultur'",
            ("bevoelkerung.csv", "suedleute=80 nordleute=20", "suedleute=50 suedleute=50"): "eine Kultur ist doppelt angegeben",
            ("dateien.csv", "orte;nord;nordleute=100", "orte;nord;nordleute=90"): "Anteile in Spalte 'kultur' ergeben 90 %",
            ("dateien.csv", "staaten;AAA_nordland;", "staaten;AAA_nordland;nordleute=100"): "Spalte 'kultur' gilt nur für Dateien in history/orte",
            ("bevoelkerung.csv", "burg;1648.10.24;5000", "burg;1648.10.24;0"): "ungültige Bevölkerung '0'",
            ("bevoelkerung.csv", "nordleute=20;0.1", "nordleute=20;1.5"): "außerhalb von 0 bis 1",
            ("orte.csv", "11.50;52.40;200;ja", "11,50;52.40;200;ja"): "ungültige Zahl '11,50'",
            ("orte.csv", "200;ja;kr_burg", "200;vielleicht;kr_burg"): "erwartet ja oder nein",
            ("staaten.csv", "10 20 30", "10 20 300"): "ungültige Farbe '10 20 300'",
            ("staaten.csv", "AAA;AAA_nordland", "aaa;AAA_nordland"): "ungültiger Staatstag 'aaa'",
            ("staaten_historie.csv", "steuersatz;0.2", "steuersatz;0.7"): "außerhalb von 0 bis 0.5",
            ("staaten_historie.csv", "BBB;1648.10.24;schatz", "BBB;1648.10.24;armee"): "unbekanntes Feld 'armee'",
            ("dateien.csv", "staaten;AAA_nordland", "karten;AAA_nordland"): "erwartet kreise, orte, staaten",
            ("orte.csv", "200;ja;kr_burg", "200;ja;kr_turm"): "unbekannter Kreis 'kr_turm'",
            ("kreise.csv", "kr_burg;sued;Burg;Testdaten", "kr_burg;sued;;Testdaten"): "Spalte 'name' darf nicht leer sein",
            ("kreise.csv", "kr_burg;sued", "Kr_burg;sued"): "ungültige Kreiskennung 'Kr_burg'",
        }
        for (tabelle, alt, neu), text in faelle.items():
            with self.subTest(text):
                self.tearDown()  # frische Kopie je Fall
                self.setUp()
                self.ersetze(tabelle, alt, neu)
                self.erwarte_fehler(text)

    def test_kultur_aus_landschaft_oder_zeile(self) -> None:
        m = ar.Meldungen()
        bezug = ar.lade_bezug(self.daten, m)
        ausgabe = ar.erzeuge(ar.baue_archiv(self.daten / "tabellen", bezug, m))
        self.assertEqual(m.fehler, [])
        # nord: Standard der Landschaft aus dateien.csv, in jedem datierten Block
        self.assertEqual(ausgabe["history/orte/nord.chron"].count("kultur       = { nordleute = 100 }"), 3)
        # sued: eigene Angabe der Zeile
        self.assertIn("kultur       = { suedleute = 80 nordleute = 20 }", ausgabe["history/orte/sued.chron"])
        # Eigene Angabe schlägt den Standard der Landschaft.
        self.ersetze("bevoelkerung.csv", "heide;1648.10.24;1500;lutherisch=100;", "heide;1648.10.24;1500;lutherisch=100;suedleute=100")
        m = ar.Meldungen()
        ausgabe = ar.erzeuge(ar.baue_archiv(self.daten / "tabellen", bezug, m))
        self.assertIn("kultur       = { suedleute = 100 }", ausgabe["history/orte/nord.chron"])

    def test_kultur_fehlt(self) -> None:
        self.ersetze("bevoelkerung.csv", "suedleute=80 nordleute=20", "")
        self.erwarte_fehler("keine Kultur für burg (1648.10.24): Spalte 'kultur' ist leer, und dateien.csv nennt keine Standardkultur für orte/sued")

    def test_zeilenumbruch_in_zeichenketten(self) -> None:
        # Zeilenumbrüche (LF wie mit Alt+Enter, auch ein einzelnes CR) sind im CSV erlaubt, in
        # Zeichenketten des Chronik-Skripts aber nicht (chronik-skript.md 1.1).
        faelle = {
            ("kreise.csv", "kr_hafen;nord;Hafen;", 'kr_hafen;nord;"Ha\nfen";'): "Zeilenumbruch in Spalte 'name'",
            ("orte.csv", "Testdaten, Ort ohne Daten", '"Testdaten,\nOrt ohne Daten"'): "Zeilenumbruch in Spalte 'quelle'",
            ("orte.csv", "moor;nord;Moor;", 'moor;nord;"Mo\ror";'): "Zeilenumbruch in Spalte 'name'",
            ("staaten.csv", ";Nordland;", ';"Nord\r\nland";'): "Zeilenumbruch in Spalte 'kurzname'",
            ("staaten.csv", "Herzogtum Nordland;", '"Herzogtum\rNordland";'): "Zeilenumbruch in Spalte 'name'",
            ("staaten.csv", "Testdaten mit \\ Rückstrich", '"Testdaten\nmit Umbruch"'): "Zeilenumbruch in Spalte 'quelle'",
            ("staaten_historie.csv", "herrscher;Herzog Alt;", 'herrscher;"Herzog\rAlt";'): "Zeilenumbruch im Wert für 'herrscher'",
        }
        for (tabelle, alt, neu), text in faelle.items():
            with self.subTest(tabelle=tabelle, neu=neu):
                self.tearDown()  # frische Kopie je Fall
                self.setUp()
                pfad = self.tabelle(tabelle)
                inhalt = pfad.read_bytes().decode("utf-8")
                self.assertIn(alt, inhalt)
                pfad.write_bytes(inhalt.replace(alt, neu, 1).encode("utf-8"))  # ohne Umwandlung der Zeilenenden
                self.erwarte_fehler(text)

    def test_doppelte_eintraege(self) -> None:
        self.anhaengen("orte.csv", "burg;sued;Burg 2;flachland;11.5;52.4;200;ja;kr_burg;Testdaten")
        self.anhaengen("besitz.csv", "burg;1648.10.24;AAA;gesichert;Testquelle")
        self.anhaengen("staaten_historie.csv", "AAA;1648.10.24;schatz;5;spielwert;Spielwert")
        code, _, fehler = self.pruefen()
        self.assertEqual(code, 2)
        self.assertIn("tabellen/orte.csv:6: Ort burg ist doppelt (zuerst in Zeile 4)", fehler)
        self.assertIn("Besitz von burg ab 1648.10.24 ist doppelt (zuerst in Zeile 5)", fehler)
        self.assertIn("schatz von AAA ab 1648.10.24 ist doppelt", fehler)

    def test_luecke_vor_dem_ersten_besitz(self) -> None:
        self.ersetze("besitz.csv", "heide;1648.10.24;AAA", "heide;1650.01.01;AAA")
        self.erwarte_fehler(
            "Ort heide hat zum Szenario frieden (1648.10.24) eine Bevölkerungsangabe, aber keinen Besitzer"
        )

    def test_luecke_in_der_staatshistorie(self) -> None:
        self.ersetze("staaten_historie.csv", "AAA;1648.10.24;hofhaltung", "AAA;1690.01.01;hofhaltung")
        self.erwarte_fehler("Staat AAA hat zum Szenario frieden (1648.10.24) keinen Wert für 'hofhaltung'")

    def test_hauptstadt_in_fremder_hand(self) -> None:
        self.anhaengen("staaten_historie.csv", "AAA;1701.01.01;hauptstadt;heide;gesichert;Testquelle")
        self.erwarte_fehler("Hauptstadt heide von AAA gehört zum Szenario krone (1701.01.18) zu BBB")


class TestOrteOhneDatenUndKreise(MitKopie):
    def test_ort_ohne_daten_ist_erlaubt_und_hat_keine_bloecke(self) -> None:
        text = (self.daten / "history/orte/nord.chron").read_text(encoding="utf-8")
        moor = chron.parse_script(text).root.children[-1]
        self.assertEqual(moor.value, "moor")
        self.assertEqual([k.key for k in moor.children if k.is_block and k.key[:1].isdigit()], [])
        self.assertEqual(moor.get_string("kreis"), "kr_hafen")

    def test_kreisdateien(self) -> None:
        root = chron.parse_script((self.daten / "history/kreise/sued.chron").read_text(encoding="utf-8")).root
        self.assertEqual([(n.key, n.value, n.get_string("name")) for n in root.children], [("kreis", "kr_burg", "Burg")])

    def test_halbe_daten(self) -> None:
        self.anhaengen("besitz.csv", "moor;1690.01.01;AAA;gesichert;Testquelle")
        self.erwarte_fehler(
            "Ort moor hat zum Szenario krone (1701.01.18) einen Besitzer, aber keine Bevölkerungsangabe"
        )

    def test_daten_erst_ab_spaeterem_datum(self) -> None:
        # 1648 ohne Daten, 1701 mit Daten: erlaubt.
        self.anhaengen("besitz.csv", "moor;1690.01.01;AAA;gesichert;Testquelle")
        self.anhaengen("bevoelkerung.csv", "moor;1690.01.01;800;lutherisch=100;;0;schaetzung;Testquelle")
        code, aus, fehler = _ausfuehren("--daten", str(self.daten), "--bericht")
        self.assertEqual(code, 0, fehler)
        self.assertIn("frieden (1648.10.24): 3 Orte mit Daten, 1 ohne Daten", aus)
        self.assertIn("krone (1701.01.18): 4 Orte mit Daten, 0 ohne Daten", aus)

    def test_hauptstadt_ohne_daten(self) -> None:
        self.anhaengen("staaten_historie.csv", "AAA;1700.01.01;hauptstadt;moor;gesichert;Testquelle")
        self.erwarte_fehler("Hauptstadt moor von AAA ist zum Szenario krone (1701.01.18) ein Ort ohne Daten")

    def test_ort_ohne_kreis_und_kreis_ohne_orte(self) -> None:
        self.ersetze("orte.csv", "200;ja;kr_burg", "200;ja;")
        code, _, fehler = _ausfuehren("--daten", str(self.daten), "--bericht")
        self.assertEqual(code, 0, fehler)
        self.assertIn("Warnung: tabellen/orte.csv:4: 1 Orte ohne Kreis (Spalte 'kreis' leer): burg", fehler)
        self.assertIn("Warnung: tabellen/kreise.csv:3: Kreis kr_burg hat keine Orte", fehler)


class TestWarnungen(MitKopie):
    def test_dichte(self) -> None:
        self.ersetze("bevoelkerung.csv", "heide;1648.10.24;1500", "heide;1648.10.24;90000")
        self.erwarte_warnung("tabellen/bevoelkerung.csv:4: hohe Dichte 300 Einwohner/km² in heide (1648.10.24)")

    def test_wachstum(self) -> None:
        self.ersetze("bevoelkerung.csv", "hafen;1700.01.01;9000", "hafen;1700.01.01;900000")
        self.erwarte_warnung("tabellen/bevoelkerung.csv:3: auffällige Bevölkerungsänderung in hafen: +10.3 % je Jahr")

    def test_flaeche_und_kartengrenzen(self) -> None:
        self.ersetze("orte.csv", "10.30;53.80;150", "9.30;53.80;5")
        code, _, fehler = _ausfuehren("--daten", str(self.daten), "--bericht")
        self.assertEqual(code, 0)
        self.assertIn("Ort hafen liegt außerhalb der Karte", fehler)
        self.assertIn("ungewöhnliche Fläche 5 km² für Ort hafen", fehler)

    def test_staat_ohne_ort_und_unbenutzter_kommentar(self) -> None:
        self.anhaengen("staaten.csv", "CCC;CCC_leer;Leer;Leer;1 2 3;Testdaten")
        for feld, wert in (
            ("hauptstadt", "burg"),
            ("regierungsform", "x"),
            ("herrscher", "Niemand"),
            ("konfession", "katholisch"),
            ("schatz", "0"),
            ("steuersatz", "0"),
            ("hofhaltung", "0"),
        ):
            self.anhaengen("staaten_historie.csv", f"CCC;1648.10.24;{feld};{wert};spielwert;Test")
        self.anhaengen("dateien.csv", "orte;gibt_es_nicht;;Kommentar")
        code, _, fehler = _ausfuehren("--daten", str(self.daten), "--bericht")
        # Hauptstadt burg gehört BBB: das ist ein Fehler, die übrigen Befunde sind Warnungen.
        self.assertEqual(code, 2)
        self.assertIn("Warnung: tabellen/dateien.csv:6: Kommentar für orte/gibt_es_nicht", fehler)


class TestHilfen(unittest.TestCase):
    def test_datum(self) -> None:
        self.assertEqual(ar.parse_datum("1648.10.24"), (1648, 10, 24))
        self.assertEqual(ar.parse_datum("2000.02.29"), (2000, 2, 29))
        self.assertIsNone(ar.parse_datum("1700.02.29"))  # kein Schaltjahr
        self.assertIsNone(ar.parse_datum("1648.10.4"))
        self.assertIsNone(ar.parse_datum("1648-10-24"))
        self.assertIsNone(ar.parse_datum("0000.01.01"))  # wie der Kern: ab Jahr 1
        self.assertEqual(ar.parse_datum("0001.01.01"), (1, 1, 1))

    def test_festkomma(self) -> None:
        # Gleiche Rundung wie Fixed::parse im Kern (16 Nachkommabits, kaufmännisch).
        self.assertEqual(ar.festkomma_roh("1"), 65536)
        self.assertEqual(ar.festkomma_roh("0.5"), 32768)
        self.assertEqual(ar.festkomma_roh("-0.5"), -32768)
        self.assertEqual(ar.festkomma_roh("0.00001"), 1)
        self.assertEqual(ar.festkomma_roh("0.00000763"), 1)  # 0,50004 → 1
        self.assertEqual(ar.festkomma_roh("0.0000076"), 0)  # 0,498 → 0
        self.assertEqual(ar.festkomma_roh("0.000001"), 0)

    def test_anteile(self) -> None:
        self.assertEqual(ar.parse_anteile("lutherisch=95 reformiert=5"), [("lutherisch", 95), ("reformiert", 5)])
        self.assertIsNone(ar.parse_anteile(""))
        self.assertIsNone(ar.parse_anteile("lutherisch=x"))

    def test_chron_text(self) -> None:
        self.assertEqual(ar.chron_text('a "b" \\ c'), '"a \\"b\\" \\\\ c"')
        self.assertTrue(ar.hat_zeilenumbruch("a\nb"))
        self.assertTrue(ar.hat_zeilenumbruch("a\rb"))
        self.assertFalse(ar.hat_zeilenumbruch("a b"))

    def test_echte_daten_sind_aktuell(self) -> None:
        daten = Path(__file__).resolve().parent.parent.parent / "data"
        code, aus, fehler = _ausfuehren("--daten", str(daten), "--pruefen")
        self.assertEqual(code, 0, aus + fehler)


if __name__ == "__main__":
    unittest.main()
