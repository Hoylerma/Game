"""Tests für den Chronik-Skript-Parser (chron.py), die Python-Fassung des Parsers aus core/src/script.cpp.

Aufruf (aus der Wurzel des Repositorys):

    python3 -m unittest discover -s tools/kartengenerator -v
"""

from __future__ import annotations

import unittest
from pathlib import Path

import chron

HIER = Path(__file__).resolve().parent
REPO = HIER.parents[1]
TESTDATEN = HIER / "testdaten"


def _parse_ok(text: str | bytes) -> chron.ParseResult:
    ergebnis = chron.parse_script(text, "test.chron")
    if not ergebnis.ok:
        raise AssertionError("\n".join(str(e) for e in ergebnis.errors))
    return ergebnis


class TestParser(unittest.TestCase):
    def test_leere_eingabe_und_kommentare(self) -> None:
        for text in ("", "   \n\t\r\n", "# nur ein Kommentar", "# a\n# b\n"):
            ergebnis = _parse_ok(text)
            self.assertTrue(ergebnis.root.is_block)
            self.assertEqual(ergebnis.root.children, [])

    def test_zuweisung_mit_zeichenkette(self) -> None:
        n = _parse_ok('name = "Berlin"').root.children[0]
        self.assertEqual((n.key, n.op, n.value, n.quoted, n.is_block), ("name", "=", "Berlin", True, False))
        self.assertEqual((n.line, n.column), (1, 1))

    def test_alle_operatoren(self) -> None:
        kinder = _parse_ok("a = 1\nb < 2\nc > 3\nd <= 4\ne >= 5\nf != 6\ng>=7 h<8 i!=9 j=k").root.children
        erwartet = [
            ("a", "=", "1"),
            ("b", "<", "2"),
            ("c", ">", "3"),
            ("d", "<=", "4"),
            ("e", ">=", "5"),
            ("f", "!=", "6"),
            ("g", ">=", "7"),
            ("h", "<", "8"),
            ("i", "!=", "9"),
            ("j", "=", "k"),
        ]
        self.assertEqual([(n.key, n.op, n.value) for n in kinder], erwartet)
        self.assertTrue(all(not n.quoted and not n.is_block for n in kinder))
        self.assertEqual(kinder[4].line, 5)
        self.assertEqual((kinder[7].line, kinder[7].column), (7, 6))

    def test_blockformen(self) -> None:
        kinder = _parse_ok(
            "lage = { 13.4 52.5 }\nregeln { a = 1 }\nort berlin_coelln { name = x }\n{ anonym }\n"
        ).root.children
        self.assertEqual(len(kinder), 4)
        lage, regeln, ort, anonym = kinder
        self.assertEqual((lage.key, lage.op, lage.value, lage.is_block), ("lage", "=", "", True))
        self.assertEqual(lage.bare_values(), ["13.4", "52.5"])
        self.assertEqual((lage.children[0].key, lage.children[0].op), ("", ""))
        self.assertEqual((regeln.key, regeln.op, regeln.is_block), ("regeln", "=", True))
        self.assertEqual(regeln.children[0].key, "a")
        self.assertEqual((ort.key, ort.op, ort.value, ort.is_block, ort.line), ("ort", "=", "berlin_coelln", True, 3))
        self.assertEqual(ort.get_string("name"), "x")
        self.assertEqual((anonym.key, anonym.is_block, anonym.line, anonym.column), ("", True, 4, 1))
        self.assertEqual(anonym.bare_values(), ["anonym"])

    def test_vorausschau(self) -> None:
        c = _parse_ok("liste = { a b c }\nx y { }\np { } q\nm n o = 1\n{ { 1 2 } { 3 } }\n").root.children
        self.assertEqual(len(c), 8)
        self.assertEqual(c[0].bare_values(), ["a", "b", "c"])
        self.assertEqual((c[1].key, c[1].value, c[1].is_block), ("x", "y", True))
        self.assertEqual((c[2].key, c[2].is_block), ("p", True))
        self.assertEqual((c[3].key, c[3].value), ("", "q"))
        self.assertEqual((c[4].key, c[4].value), ("", "m"))
        self.assertEqual((c[5].key, c[5].value), ("", "n"))
        self.assertEqual((c[6].key, c[6].value), ("o", "1"))
        self.assertEqual([len(k.bare_values()) for k in c[7].children], [2, 1])

    def test_verschachtelung_und_positionen(self) -> None:
        staat = _parse_ok(
            "staat BRA {\n" "    farbe = { 58 95 143 }\n" "    konfession = { lutherisch = 90 reformiert = 10 }\n" "}\n"
        ).root.children[0]
        self.assertEqual((staat.children[0].line, staat.children[0].column), (2, 5))
        konf = staat.children[1]
        self.assertEqual((konf.get_int("lutherisch"), konf.get_int("reformiert")), (90, 10))
        self.assertEqual(konf.children[1].column, 36)

    def test_kommentare(self) -> None:
        c = _parse_ok(
            "# Kopf\n"
            "a = 1 # Kommentar am Zeilenende\n"
            'b = "Text # kein Kommentar"\n'
            "c = { x # Kommentar im Block\n"
            "  y }\n"
            "d = 2#direkt\n"
        ).root.children
        self.assertEqual([n.key for n in c], ["a", "b", "c", "d"])
        self.assertEqual(c[1].value, "Text # kein Kommentar")
        self.assertEqual(c[2].bare_values(), ["x", "y"])
        self.assertEqual(c[3].value, "2")

    def test_zeichenketten_mit_escapes(self) -> None:
        c = _parse_ok(r'a = "Er sagte \"Ja\"" b = "C:\\Pfad\\" c = "" d = "mit  Leer { } = # zeichen"').root.children
        self.assertEqual(c[0].value, 'Er sagte "Ja"')
        self.assertEqual(c[1].value, "C:\\Pfad\\")
        self.assertEqual((c[2].value, c[2].quoted), ("", True))
        self.assertEqual(c[3].value, "mit  Leer { } = # zeichen")

    def test_zeichenkette_als_listeneintrag(self) -> None:
        n = _parse_ok('namen = { "Berlin" "Cölln" }').root.children[0]
        self.assertTrue(n.children[0].quoted)
        self.assertEqual(n.bare_values(), ["Berlin", "Cölln"])

    def test_utf8_spalten_in_bytes(self) -> None:
        c = _parse_ok('name = Cölln\nort köln_süd { name = "Köln-Süd" }\näb = 1 c').root.children
        self.assertEqual(c[0].value, "Cölln")
        self.assertEqual(c[1].value, "köln_süd")
        self.assertEqual(c[1].get_string("name"), "Köln-Süd")
        self.assertEqual(c[2].key, "äb")
        self.assertEqual((c[3].value, c[3].line, c[3].column), ("c", 3, 9))  # "äb" sind 3 Bytes

    def test_woerter_mit_sonderzeichen(self) -> None:
        c = _parse_ok("a = -0.25 b = 1648.10.24 c = x.y-z/w:1 d = 12000").root.children
        self.assertEqual([n.value for n in c], ["-0.25", "1648.10.24", "x.y-z/w:1", "12000"])

    def test_bom_und_crlf(self) -> None:
        c = _parse_ok(b'\xef\xbb\xbfa = 1\r\nb = "x"\r\nc {\r\n  d\r\n}\r\n').root.children
        self.assertEqual((c[0].key, c[0].line, c[0].column), ("a", 1, 1))
        self.assertEqual((c[1].value, c[1].line), ("x", 2))
        self.assertEqual((c[2].children[0].line, c[2].children[0].column), (4, 3))

    def test_zugriffsfunktionen(self) -> None:
        root = _parse_ok(
            "x = 5\nx = 6\nx >= 7\nblk = { 1 2 k = v }\n"
            'f = 0.25 i = -12 leer = "" bad = 5. bad2 = .5 bad3 = 1e3\n'
            "b1 = ja b2 = yes b3 = wahr b4 = nein b5 = no b6 = falsch b7 = vielleicht b8 = Ja\n"
        ).root
        self.assertIs(root.find("x"), root.children[0])
        self.assertIsNone(root.find("fehlt"))
        self.assertEqual(len(root.find_all("x")), 3)
        self.assertEqual(root.get_string("x"), "5")
        self.assertIsNone(root.get_string("blk"))
        self.assertEqual(root.get_string("leer"), "")
        self.assertEqual(root.get_zahl("f"), 0.25)
        self.assertEqual(root.get_zahl("i"), -12.0)
        for schluessel in ("bad", "bad2", "bad3", "leer"):
            self.assertIsNone(root.get_zahl(schluessel), schluessel)
        self.assertEqual(root.get_int("i"), -12)
        self.assertIsNone(root.get_int("f"))
        self.assertEqual(
            [root.get_bool(f"b{k}") for k in range(1, 9)], [True, True, True, False, False, False, None, None]
        )
        self.assertEqual(root.find("blk").bare_values(), ["1", "2"])
        self.assertEqual(len(list(root.walk())), len(root.children) + 3)

    # ── Fehler ──

    def test_nicht_geschlossener_block(self) -> None:
        e = chron.parse_script("a = 1\nb = {\n  c = 2\n", "orte.chron")
        self.assertEqual([x.message for x in e.errors], ["nicht geschlossener Block (geöffnet in Zeile 2)"])
        self.assertEqual(e.errors[0].file, "orte.chron")
        self.assertEqual(e.root.children[1].get_int("c"), 2)  # Inhalt bleibt erhalten

    def test_unerwartete_klammer(self) -> None:
        e = chron.parse_script("a = 1\n}\nb = 2\n", "x.chron")
        self.assertEqual(len(e.errors), 1)
        self.assertEqual(str(e.errors[0]), "x.chron:2:1: unerwartetes '}'")
        self.assertEqual(len(e.root.children), 2)

    def test_wert_fehlt(self) -> None:
        e = chron.parse_script("a =", "x.chron")
        self.assertEqual([(x.line, x.column, x.message) for x in e.errors], [(1, 3, "Wert nach '=' fehlt")])
        self.assertEqual(e.root.children, [])

        e = chron.parse_script("blk {\n  a >=\n}\nb = 2", "x.chron")
        self.assertEqual([(x.line, x.message) for x in e.errors], [(2, "Wert nach '>=' fehlt")])
        self.assertEqual(len(e.root.children), 2)
        self.assertEqual(e.root.children[0].children, [])

        e = chron.parse_script("name =\nbesitzer = BRA\n", "x.chron")
        self.assertEqual([(x.line, x.message) for x in e.errors], [(1, "Wert nach '=' fehlt")])
        self.assertEqual([n.key for n in e.root.children], ["besitzer"])

        e = chron.parse_script("a = = 5\nb = 6", "x.chron")
        self.assertEqual(len(e.errors), 1)
        self.assertIn("stattdessen '='", e.errors[0].message)
        self.assertEqual([n.key for n in e.root.children], ["b"])

        self.assertEqual(len(_parse_ok('lage =\n  { 1 2 }\nname =\n  "x"').root.children), 2)

    def test_fehler_in_zeichenketten(self) -> None:
        e = chron.parse_script('a = "offen\nb = 2', "x.chron")
        self.assertEqual([(x.line, x.column, x.message) for x in e.errors], [(1, 5, "nicht beendete Zeichenkette")])
        self.assertEqual(e.root.get_int("b"), 2)  # Wiederaufsetzen in der nächsten Zeile

        e = chron.parse_script(r'a = "x\ny"', "x.chron")
        self.assertEqual(len(e.errors), 1)
        self.assertIn("Escape", e.errors[0].message)
        self.assertEqual(e.errors[0].column, 7)

        e = chron.parse_script('"name" = 5\nb = 1', "x.chron")
        self.assertEqual([x.message for x in e.errors], ["eine Zeichenkette kann kein Schlüssel sein"])
        self.assertEqual([n.key for n in e.root.children], ["b"])

    def test_unerwartete_operatoren(self) -> None:
        e = chron.parse_script("= 5\na ! b\nc = 1", "x.chron")
        self.assertEqual(len(e.errors), 2)
        self.assertEqual((e.errors[0].line, e.errors[0].message), (1, "unerwarteter Operator '='"))
        self.assertEqual((e.errors[1].line, e.errors[1].column), (2, 3))
        self.assertIn("'!'", e.errors[1].message)
        self.assertIsNotNone(e.root.find("c"))

    def test_ungueltiges_utf8(self) -> None:
        e = chron.parse_script(b'name = K\xf6ln\nb = "\xc3\x28"\nc = ok', "x.chron")
        self.assertEqual([(x.line, x.column) for x in e.errors], [(1, 9), (2, 6)])
        self.assertTrue(all("UTF-8" in x.message for x in e.errors))
        self.assertIsNotNone(e.root.find("c"))

    def test_mehrere_fehler_sortiert_und_fortgesetzt(self) -> None:
        e = chron.parse_script('}\na = "x\n= 3\nb = {\n', "x.chron")
        self.assertEqual([x.line for x in e.errors], [1, 2, 3, 5])
        self.assertEqual(e.errors[-1].message, "nicht geschlossener Block (geöffnet in Zeile 4)")

    def test_tiefe_verschachtelung(self) -> None:
        erlaubt = "{ " * chron.MAX_TIEFE + "x " + "} " * chron.MAX_TIEFE
        self.assertTrue(_parse_ok(erlaubt).ok)  # iterativ, kein RecursionError
        zu_tief = "{ " * (chron.MAX_TIEFE + 1) + "x " + "} " * (chron.MAX_TIEFE + 1) + "\nb = 1"
        e = chron.parse_script(zu_tief, "x.chron")
        self.assertEqual(len(e.errors), 1)
        self.assertIn("zu tief verschachtelt", e.errors[0].message)
        self.assertEqual(e.root.get_int("b"), 1)

    def test_datei_fehlt(self) -> None:
        e = chron.parse_script_file(TESTDATEN / "gibt_es_nicht.chron", "gibt_es_nicht.chron")
        self.assertEqual([str(x) for x in e.errors], ["gibt_es_nicht.chron: Datei kann nicht gelesen werden"])

    def test_echte_daten_fehlerfrei(self) -> None:
        dateien = sorted((REPO / "data").rglob("*.chron"))
        self.assertTrue(dateien)
        for pfad in dateien:
            ergebnis = chron.parse_script_file(pfad)
            self.assertEqual(ergebnis.errors, [], pfad)


if __name__ == "__main__":
    unittest.main()
