"""Tests für den Kartengenerator und den Chronik-Skript-Parser.

Aufruf (aus der Wurzel des Repositorys):

    python3 -m unittest discover -s tools/kartengenerator -v

Die Testdaten liegen in tools/kartengenerator/testdaten/. Dort liegen auch die
erwarteten Ausgaben (provinzen.png, definition.csv); nach einer gewollten Änderung
am Algorithmus neu erzeugen mit:

    python3 tools/kartengenerator/kartengenerator.py --daten tools/kartengenerator/testdaten
"""

from __future__ import annotations

import contextlib
import io
import math
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

import chron
import kartengenerator as kg

HIER = Path(__file__).resolve().parent
TESTDATEN = HIER / "testdaten"


def _parse_ok(text: str | bytes) -> chron.ParseResult:
    ergebnis = chron.parse_script(text, "test.chron")
    if not ergebnis.ok:
        raise AssertionError("\n".join(str(e) for e in ergebnis.errors))
    return ergebnis


def _starte(*argumente: str) -> tuple[int, str, str]:
    """Ruft kartengenerator.main auf und fängt die Ausgaben ab."""
    aus, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(aus), contextlib.redirect_stderr(err):
        code = kg.main(list(argumente))
    return code, aus.getvalue(), err.getvalue()


def _fixture_laden() -> tuple[kg.Karte, list[kg.Ort]]:
    karte, orte, warnungen = kg._lade(TESTDATEN)
    if warnungen:
        raise AssertionError("\n".join(warnungen))
    return karte, orte


def _png_mit_filter(breite: int, hoehe: int, zeilen: list[bytes], filtertyp: int, stufe: int = 6) -> bytes:
    """Unabhängiger PNG-Schreiber mit festem Zeilenfilter (für Decoder-Tests)."""
    roh = bytearray()
    vorher = bytes(3 * breite)
    for zeile in zeilen:
        roh.append(filtertyp)
        for k, wert in enumerate(zeile):
            links = zeile[k - 3] if k >= 3 else 0
            oben = vorher[k]
            oben_links = vorher[k - 3] if k >= 3 else 0
            vorhersage = (
                0,
                links,
                oben,
                (links + oben) // 2,
                kg._paeth(links, oben, oben_links),
            )[filtertyp]
            roh.append((wert - vorhersage) & 0xFF)
        vorher = zeile
    ihdr = struct.pack(">IIBBBBB", breite, hoehe, 8, 2, 0, 0, 0)
    return (
        kg.PNG_SIGNATUR
        + kg._chunk(b"IHDR", ihdr)
        + kg._chunk(b"IDAT", zlib.compress(bytes(roh), stufe))
        + kg._chunk(b"IEND", b"")
    )


@contextlib.contextmanager
def _ohne_rauschen():
    """Schaltet das Domain Warping vorübergehend ab (reines Voronoi-Diagramm)."""
    alt = kg.RAUSCHEN_OKTAVEN
    kg.RAUSCHEN_OKTAVEN = ()
    try:
        yield
    finally:
        kg.RAUSCHEN_OKTAVEN = alt


def _bloecke(png: bytes) -> list[bytes]:
    """Typen aller Blöcke eines PNG."""
    pos, typen = 8, []
    while pos < len(png):
        (laenge,) = struct.unpack(">I", png[pos : pos + 4])
        typen.append(png[pos + 4 : pos + 8])
        pos += 12 + laenge
    return typen


# ─── Parser ─────────────────────────────────────────────────────────────────


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
        self.assertEqual(
            (ort.key, ort.op, ort.value, ort.is_block, ort.line), ("ort", "=", "berlin_coelln", True, 3)
        )
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
            "staat BRA {\n"
            "    farbe = { 58 95 143 }\n"
            "    konfession = { lutherisch = 90 reformiert = 10 }\n"
            "}\n"
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
        c = _parse_ok(
            r'a = "Er sagte \"Ja\"" b = "C:\\Pfad\\" c = "" d = "mit  Leer { } = # zeichen"'
        ).root.children
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
        self.assertEqual(
            [(x.line, x.column, x.message) for x in e.errors], [(1, 5, "nicht beendete Zeichenkette")]
        )
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
        dateien = sorted((kg.REPO / "data").rglob("*.chron"))
        self.assertTrue(dateien)
        for pfad in dateien:
            ergebnis = chron.parse_script_file(pfad)
            self.assertEqual(ergebnis.errors, [], pfad)


# ─── Laden ──────────────────────────────────────────────────────────────────


class TestLaden(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="kartengenerator-test-")
        self.daten = Path(self._tmp.name) / "daten"
        shutil.copytree(TESTDATEN, self.daten)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def _karte(self, inhalt: str) -> tuple[kg.Karte | None, kg.Meldungen]:
        (self.daten / "karte" / "karte.chron").write_text(inhalt, encoding="utf-8")
        meldungen = kg.Meldungen()
        karte = kg.lade_karte(self.daten / "karte" / "karte.chron", "karte/karte.chron", meldungen)
        return karte, meldungen

    def test_fixture(self) -> None:
        karte, orte = _fixture_laden()
        self.assertEqual((karte.breite, karte.hoehe, karte.max_radius_km), (120, 192, 25.0))
        self.assertEqual(karte.kueste[0], (10.0, 52.92))
        self.assertEqual(len(karte.kueste), 4)
        # Dateien sortiert, Einträge in Dateireihenfolge → Kartenindex.
        self.assertEqual([o.kennung for o in orte], ["hafen", "heide", "burg", "aue"])
        self.assertEqual(orte[1].name, "Heide-Süd")
        self.assertEqual((orte[0].laenge, orte[0].breite, orte[0].flaeche), (10.3, 52.8, 150.0))
        self.assertEqual(
            (orte[0].datei, orte[0].zeile, orte[0].besitzer), ("history/orte/a_nord.chron", 3, "AAA")
        )

    def test_dateireihenfolge_nach_bytes(self) -> None:
        orte = self.daten / "history" / "orte"
        for alt in orte.iterdir():
            alt.unlink()
        for datei, kennung in (("b.chron", "b1"), ("a.chron", "a1"), ("Z.chron", "z1"), ("a_2.chron", "a2")):
            (orte / datei).write_text(
                f'ort {kennung} {{ name = "{kennung}" lage = {{ 10.5 52.5 }} flaeche = 100 }}\n',
                encoding="utf-8",
            )
        (orte / "c.txt").write_text("ort c1 { }", encoding="utf-8")  # keine .chron-Datei
        meldungen = kg.Meldungen()
        geladen = kg.lade_orte(orte, self.daten, meldungen)
        self.assertEqual(meldungen.fehler, [])
        # Großbuchstaben vor Kleinbuchstaben, "a.chron" vor "a_2.chron" (Bytefolge wie im Lader).
        self.assertEqual([o.kennung for o in geladen], ["z1", "a1", "a2", "b1"])

    def test_besitzer_aus_datierten_bloecken(self) -> None:
        orte = self.daten / "history" / "orte"
        for alt in orte.iterdir():
            alt.unlink()
        (orte / "a.chron").write_text(
            'ort a1 { name = "A" lage = { 10.5 52.5 } flaeche = 100\n'
            "  1701.01.18 = { besitzer = BBB }\n"
            "  1648.10.24 = { besitzer = AAA bevoelkerung = 10 }\n"
            "}\n",
            encoding="utf-8",
        )
        meldungen = kg.Meldungen()
        geladen = kg.lade_orte(orte, self.daten, meldungen)
        self.assertEqual(meldungen.fehler, [])
        self.assertEqual(geladen[0].besitzer, "BBB")  # jüngster Stand, unabhängig von der Reihenfolge

    def test_fehler_in_orten(self) -> None:
        (self.daten / "history" / "orte" / "c_fehler.chron").write_text(
            'ort hafen { name = "Doppelt" lage = { 10.1 52.1 } flaeche = 100 }\n'
            'ort Gross { name = "X" lage = { 10.1 52.1 } flaeche = 100 }\n'
            'ort ohne_lage { name = "Y" flaeche = 100 }\n'
            'ort schlechte_lage { name = "Z" lage = { 10.1 } flaeche = 100 }\n'
            'ort semikolon { name = "A;B" lage = { 10.1 52.1 } flaeche = 100 }\n'
            'ort null { name = "N" lage = { 10.1 52.1 } flaeche = 0 }\n'
            "ort = kaputt\n"
            "fremd = 1\n",
            encoding="utf-8",
        )
        meldungen = kg.Meldungen()
        kg.lade_orte(self.daten / "history" / "orte", self.daten, meldungen)
        text = "\n".join(meldungen.fehler)
        for erwartet in (
            "history/orte/c_fehler.chron:1: doppelte Kennung 'hafen' (bereits definiert in "
            "history/orte/a_nord.chron:3)",
            "c_fehler.chron:2: ungültige Ortskennung 'Gross'",
            "c_fehler.chron:3: Pflichtfeld 'lage' fehlt in ort ohne_lage",
            "c_fehler.chron:4: 'lage' erwartet genau zwei Zahlen",
            "c_fehler.chron:5: 'name' darf kein ';' enthalten",
            "c_fehler.chron:6: 'flaeche' muss größer als 0 sein",
            "c_fehler.chron:7: ungültige Definition, erwartet: ort <kennung> { … }",
        ):
            self.assertIn(erwartet, text)
        self.assertEqual(len(meldungen.fehler), 7)
        self.assertEqual(len(meldungen.warnungen), 1)
        self.assertIn("c_fehler.chron:8: unbekannter Eintrag 'fremd' wird ignoriert", meldungen.warnungen[0])

    def test_syntaxfehler_mit_zeile(self) -> None:
        (self.daten / "history" / "orte" / "c_kaputt.chron").write_text('ort x {\n  name = "x\n}\n', "utf-8")
        meldungen = kg.Meldungen()
        kg.lade_orte(self.daten / "history" / "orte", self.daten, meldungen)
        self.assertIn("history/orte/c_kaputt.chron:2:10: nicht beendete Zeichenkette", meldungen.fehler)

    def test_karte_gueltig_mit_warnung(self) -> None:
        karte, meldungen = self._karte(
            "karte {\n bild_breite = 10 bild_hoehe = 20\n laenge_min = 1 laenge_max = 2\n"
            " breite_min = 3 breite_max = 4.5 max_radius_km = 7.5\n farbe = rot\n}\nextra = 1\n"
        )
        self.assertEqual(meldungen.fehler, [])
        self.assertEqual(karte, kg.Karte(10, 20, 1.0, 2.0, 3.0, 4.5, 7.5, ()))
        self.assertEqual(len(meldungen.warnungen), 2)
        text = "\n".join(meldungen.warnungen)
        self.assertIn("karte/karte.chron:5: unbekanntes Feld 'farbe' in karte wird ignoriert", text)
        self.assertIn("karte/karte.chron:7: unbekannter Eintrag 'extra' wird ignoriert", text)

    def test_karte_fehler(self) -> None:
        basis = (
            "bild_breite = 10 bild_hoehe = 20 laenge_min = 1 laenge_max = 2 breite_min = 3 breite_max = 4 "
        )
        faelle = {
            "karte {\n bild_hoehe = 20 laenge_min = 1 laenge_max = 2 breite_min = 3 breite_max = 4 "
            "max_radius_km = 5\n}": "karte/karte.chron:1: Pflichtfeld 'bild_breite' fehlt in karte",
            "karte {\n" + basis + "max_radius_km = 0\n}": "'max_radius_km' muss größer als 0 sein",
            "karte {\n"
            + basis.replace("bild_breite = 10", "bild_breite = 1.5")
            + "max_radius_km = 5\n}": ("ungültige Ganzzahl '1.5' für 'bild_breite'"),
            "karte {\n"
            + basis.replace("laenge_max = 2", "laenge_max = 0")
            + "max_radius_km = 5\n}": ("laenge_min < laenge_max"),
            "karte {\n"
            + basis
            + "max_radius_km = 5\n kueste = { 1 2 3 }\n}": (
                "karte/karte.chron:3: 'kueste' erwartet einen Block mit mindestens zwei Punkten"
            ),
            "karte {\n"
            + basis
            + "max_radius_km = 5\n kueste = {\n 1 2\n 3 4\n 3 5\n }\n}": (
                "karte/karte.chron:6: 'kueste': die Längengrade müssen streng steigen (Punkt 3"
            ),
            "karte {\n"
            + basis
            + "max_radius_km = 5\n kueste = { "
            + " ".join(f"{1 + k / 200} 3.5" for k in range(kg.MAX_KUESTENPUNKTE + 1))
            + " }\n}": (f"'kueste' hat {kg.MAX_KUESTENPUNKTE + 1} Punkte, erlaubt sind höchstens"),
            "karte {\n"
            + basis
            + "max_radius_km = 5 max_radius_km = 6\n}": "'max_radius_km' ist mehrfach angegeben",
            "nichts = 1\n": "Block 'karte { … }' fehlt",
            "karte = 5\n": "karte/karte.chron:1: ungültige Definition, erwartet: karte { … }",
            "karte {\n": "nicht geschlossener Block",
        }
        for inhalt, erwartet in faelle.items():
            with self.subTest(erwartet=erwartet):
                karte, meldungen = self._karte(inhalt)
                self.assertIsNone(karte)
                self.assertTrue(any(erwartet in f for f in meldungen.fehler), meldungen.fehler)

    def test_karte_ohne_kueste_ist_ganz_land(self) -> None:
        karte, meldungen = self._karte(
            "karte { bild_breite = 8 bild_hoehe = 8 laenge_min = 10 laenge_max = 11 "
            "breite_min = 52 breite_max = 53 max_radius_km = 30 }"
        )
        self.assertEqual(meldungen.fehler, [])
        self.assertEqual(kg.Raster.aus_karte(karte).erstes_land, [0] * 8)

    def test_staaten_fuer_vorschau(self) -> None:
        (self.daten / "history" / "staaten" / "CCC.chron").write_text(
            "staat CCC { farbe = { 1 2 300 } }\n", encoding="utf-8"
        )
        meldungen = kg.Meldungen()
        staaten = kg.lade_staaten(self.daten / "history" / "staaten", self.daten, meldungen)
        self.assertEqual(sorted(staaten), ["AAA", "BBB"])
        self.assertEqual(staaten["BBB"].farbe, (70, 150, 80))
        self.assertEqual(staaten["BBB"].name, "Südland")
        self.assertEqual(len(meldungen.warnungen), 1)
        self.assertIn("staat CCC: 'farbe' fehlt oder ist ungültig", meldungen.warnungen[0])
        self.assertEqual(meldungen.fehler, [])


# ─── Geometrie und Rauschen ─────────────────────────────────────────────────


class TestGeometrie(unittest.TestCase):
    def test_cos_grad(self) -> None:
        for zehntel in range(-3600, 3601, 7):
            grad = zehntel / 10.0
            self.assertAlmostEqual(kg.cos_grad(grad), math.cos(math.radians(grad)), delta=2e-16 * 8)
        self.assertEqual(kg.cos_grad(0.0), 1.0)

    def test_kuestenlinie(self) -> None:
        kueste = ((10.0, 52.92), (10.4, 52.90), (10.6, 52.96), (10.9, 52.86))
        self.assertEqual(kg.kuesten_breite(kueste, 10.0), 52.92)
        self.assertEqual(kg.kuesten_breite(kueste, 10.9), 52.86)
        self.assertAlmostEqual(kg.kuesten_breite(kueste, 10.5), 52.93, places=12)
        self.assertAlmostEqual(kg.kuesten_breite(kueste, 10.75), 52.91, places=12)
        self.assertIsNone(kg.kuesten_breite(kueste, 9.99))
        self.assertIsNone(kg.kuesten_breite(kueste, 10.95))
        self.assertTrue(kg.ist_land(kueste, 10.5, 52.92))  # südlich: Land
        self.assertFalse(kg.ist_land(kueste, 10.5, 52.94))  # nördlich: Meer
        self.assertTrue(kg.ist_land(kueste, 10.0, 52.92))  # auf der Linie: Land
        self.assertTrue(kg.ist_land(kueste, 10.95, 53.5))  # außerhalb des Linienzugs: Land
        self.assertTrue(kg.ist_land(kueste, 9.5, 60.0))
        self.assertTrue(kg.ist_land((), 10.0, 60.0))

    def test_landmaske_des_rasters(self) -> None:
        karte, _ = _fixture_laden()
        raster = kg.Raster.aus_karte(karte)
        for x in range(raster.breite):
            for y in range(raster.hoehe):
                land = kg.ist_land(karte.kueste, raster.laenge_spalte[x], raster.breite_zeile[y])
                self.assertEqual(land, y >= raster.erstes_land[x], (x, y))
        # Östlich von 10,9 °O endet die Küste: ganze Spalte Land.
        self.assertEqual(raster.erstes_land[-1], 0)
        self.assertGreater(raster.erstes_land[0], 0)

    def test_pixel_von(self) -> None:
        karte, _ = _fixture_laden()
        raster = kg.Raster.aus_karte(karte)
        self.assertEqual(raster.pixel_von(10.0, 53.0), (0, 0))
        self.assertEqual(raster.pixel_von(10.999, 52.001), (119, 191))
        self.assertIsNone(raster.pixel_von(11.0, 52.5))
        self.assertIsNone(raster.pixel_von(10.5, 53.01))
        self.assertIsNone(raster.pixel_von(9.99, 52.5))

    def test_rauschen_deterministisch_und_beschraenkt(self) -> None:
        saat = kg._saat(0, 0)
        werte = [kg.gitterwert(i, j, saat) for i in range(-20, 20) for j in range(-20, 20)]
        self.assertTrue(all(-1.0 <= w < 1.0 for w in werte))
        self.assertGreater(len(set(werte)), 1500)
        self.assertEqual(werte, [kg.gitterwert(i, j, saat) for i in range(-20, 20) for j in range(-20, 20)])
        self.assertNotEqual(kg.gitterwert(3, 4, saat), kg.gitterwert(3, 4, kg._saat(1, 0)))
        grenze = sum(a for _, a in kg.RAUSCHEN_OKTAVEN)
        for k in range(500):
            ost, sued = kg.verschiebung_km(k * 1.37 - 40.0, k * 0.91 + 3.3)
            self.assertLessEqual(abs(ost), grenze)
            self.assertLessEqual(abs(sued), grenze)

    def test_zeilenrauschen_gleich_punktauswertung(self) -> None:
        karte, _ = _fixture_laden()
        raster = kg.Raster.aus_karte(karte)
        rauschen = kg._ZeilenRauschen(raster)
        for y in range(0, raster.hoehe, 5):
            ost, sued = rauschen.zeile(y)
            y_km = (karte.breite_max - raster.breite_zeile[y]) * raster.ky
            for x in range(0, raster.breite, 3):
                self.assertEqual((ost[x], sued[x]), kg.verschiebung_km(rauschen.x_km[x], y_km), (x, y))


# ─── Zuordnung, Bereinigung, Warnungen ──────────────────────────────────────


class TestZuordnung(unittest.TestCase):
    def test_kacheln_gleich_vollsuche(self) -> None:
        karte, orte = _fixture_laden()
        raster = kg.Raster.aus_karte(karte)
        self.assertEqual(kg.zuordnen(raster, orte), kg.zuordnen(raster, orte, beschleunigt=False))

    def test_eigenschaften_der_fixture(self) -> None:
        karte, orte = _fixture_laden()
        ergebnis = kg.erzeuge(karte, orte)
        self.assertEqual(ergebnis.warnungen, [])
        raster = ergebnis.raster
        # Pixel und Orte werden je Achse um höchstens die Amplitudensumme verschoben.
        spielraum = 2 * sum(a for _, a in kg.RAUSCHEN_OKTAVEN) * math.sqrt(2)
        for index, ort in enumerate(orte):
            px, py = raster.pixel_von(ort.laenge, ort.breite)
            self.assertEqual(ergebnis.zeilen[py][px], index + 1, ort.kennung)
            self.assertGreater(ergebnis.pixel[index], 1000)
        for y, zeile in enumerate(ergebnis.zeilen):
            for x, wert in enumerate(zeile):
                self.assertEqual(wert == kg.MEER, y < raster.erstes_land[x])
                if wert > 0:
                    ort = orte[wert - 1]
                    dx = (raster.laenge_spalte[x] - ort.laenge) * raster.kx_zeile[y]
                    dy = (raster.breite_zeile[y] - ort.breite) * raster.ky
                    self.assertLessEqual(math.hypot(dx, dy), karte.max_radius_km + spielraum)
        self.assertEqual(sum(ergebnis.pixel), sum(1 for z in ergebnis.zeilen for w in z if w > 0))

    def test_ohne_rauschen_exakt_naechster_ort(self) -> None:
        karte, orte = _fixture_laden()
        with _ohne_rauschen():
            ergebnis = kg.erzeuge(karte, orte)
        self.assertEqual(ergebnis.umverteilt, 0)
        raster = ergebnis.raster
        r2 = karte.max_radius_km * karte.max_radius_km
        leer = 0
        for y, zeile in enumerate(ergebnis.zeilen):
            for x, wert in enumerate(zeile):
                if y < raster.erstes_land[x]:
                    erwartet = kg.MEER
                else:
                    erwartet, beste = 0, r2
                    for index, ort in enumerate(orte):
                        dx = (raster.laenge_spalte[x] - ort.laenge) * raster.kx_zeile[y]
                        dy = (raster.breite_zeile[y] - ort.breite) * raster.ky
                        d2 = dx * dx + dy * dy
                        if d2 < beste or (d2 == beste and erwartet == 0):
                            erwartet, beste = index + 1, d2
                    leer += erwartet == 0
                self.assertEqual(wert, erwartet, (x, y))
        self.assertGreater(leer, 100)  # der Radius begrenzt tatsächlich

    def test_flaechengewicht(self) -> None:
        karte = kg.Karte(80, 40, 10.0, 11.2, 52.0, 52.4, 60.0, ())
        orte = [kg.Ort("klein", "Klein", 10.3, 52.2, 100.0), kg.Ort("gross", "Groß", 10.9, 52.2, 300.0)]
        alt = kg.FLAECHEN_GEWICHT
        try:
            kg.FLAECHEN_GEWICHT = 0.0
            ohne = kg.erzeuge(karte, orte).pixel
            kg.FLAECHEN_GEWICHT = 1.0
            mit = kg.erzeuge(karte, orte).pixel
        finally:
            kg.FLAECHEN_GEWICHT = alt
        self.assertGreater(mit[1], ohne[1])
        self.assertLess(mit[0], ohne[0])
        self.assertEqual(sum(mit), sum(ohne))

    def test_warnungen(self) -> None:
        karte, orte = _fixture_laden()
        extra = [
            kg.Ort("im_meer", "Im Meer", 10.5, 52.99, 100.0, datei="t.chron", zeile=1),
            kg.Ort("draussen", "Draußen", 12.0, 52.5, 100.0, datei="t.chron", zeile=2),
            kg.Ort("doppelt", "Doppelt", 10.30, 52.80, 100.0, datei="t.chron", zeile=3),
        ]
        ergebnis = kg.erzeuge(karte, orte + extra)
        text = "\n".join(ergebnis.warnungen)
        self.assertIn("t.chron:1: ort im_meer: Lage 10.5 52.99 liegt im Meer", text)
        self.assertIn("t.chron:2: ort draussen: Lage 12.0 52.5 liegt außerhalb des Kartenausschnitts", text)
        self.assertIn("t.chron:2: ort draussen: erhält keine Pixel", text)
        # Gleiche Lage wie "hafen": der frühere Kartenindex gewinnt.
        self.assertIn("t.chron:3: ort doppelt: das Pixel der Lage gehört zu ort hafen", text)
        self.assertIn("t.chron:3: ort doppelt: erhält keine Pixel", text)
        self.assertNotIn("ort hafen:", text)

    def test_wenige_pixel(self) -> None:
        # Ohne Rauschen exakt: Der mittlere Ort ist zwischen zwei Nachbarn eingeklemmt.
        karte = kg.Karte(80, 80, 10.3, 10.7, 52.3, 52.7, 15.0, ())
        orte = [
            kg.Ort("links", "Links", 10.49, 52.5, 100.0, datei="t.chron", zeile=1),
            kg.Ort("mitte", "Mitte", 10.50, 52.5, 100.0, datei="t.chron", zeile=2),
            kg.Ort("rechts", "Rechts", 10.51, 52.5, 100.0, datei="t.chron", zeile=3),
        ]
        with _ohne_rauschen():
            ergebnis = kg.erzeuge(karte, orte)
        self.assertEqual(len(ergebnis.warnungen), 1, ergebnis.warnungen)
        self.assertRegex(
            ergebnis.warnungen[0], r"^t\.chron:2: ort mitte: erhält nur \d+ Pixel \(Median aller Orte"
        )
        self.assertLess(ergebnis.pixel[1] * 10, ergebnis.pixel[0])

    def test_ohne_rauschen_reines_voronoi(self) -> None:
        karte = kg.Karte(60, 30, 10.0, 11.0, 52.0, 52.3, 40.0, ())
        orte = [kg.Ort("w", "W", 10.25, 52.15, 100.0), kg.Ort("o", "O", 10.75, 52.15, 100.0)]
        with _ohne_rauschen():
            ergebnis = kg.erzeuge(karte, orte)
        # Senkrechte Mittellinie bei 10,5 °O: Spalten 0–29 West, 30–59 Ost.
        for zeile in ergebnis.zeilen:
            self.assertEqual(zeile, [1] * 30 + [2] * 30)


class TestBereinigung(unittest.TestCase):
    def test_eingeschlossene_teilflaeche(self) -> None:
        zeilen = [[1, 1, 1, 2, 2], [1, 1, 2, 2, 2], [2, 2, 2, 1, 2], [2, 2, 2, 2, 2]]
        self.assertEqual(kg.bereinigen(zeilen, {1: (0, 0), 2: (4, 3)}), (1, set()))
        self.assertEqual(zeilen[2], [2, 2, 2, 2, 2])

    def test_meer_zaehlt_nicht(self) -> None:
        m = kg.MEER
        zeilen = [[m, m, m, m, m], [m, 1, m, m, m], [2, 2, 2, 1, 1]]
        self.assertEqual(kg.bereinigen(zeilen, {1: (4, 2), 2: (0, 2)}), (1, set()))
        self.assertEqual(zeilen[1][1], 2)

    def test_nur_von_meer_umgeben(self) -> None:
        m = kg.MEER
        zeilen = [[m, m, m], [m, 1, m], [m, m, m], [1, 1, 1]]
        self.assertEqual(kg.bereinigen(zeilen, {1: (0, 3)}), (0, {1}))
        self.assertEqual(zeilen[1][1], 1)

    def test_lagepixel_bestimmt_hauptflaeche(self) -> None:
        zeilen = [[1, 1, 1, 0, 1]]
        self.assertEqual(kg.bereinigen(zeilen, {1: (4, 0)}), (1, set()))
        self.assertEqual(zeilen, [[0, 0, 0, 0, 1]])
        zeilen = [[1, 1, 1, 0, 1]]
        kg.bereinigen(zeilen, {})  # ohne Lagepixel bleibt die größte Fläche
        self.assertEqual(zeilen, [[1, 1, 1, 0, 0]])

    def test_diagonal_ist_nicht_verbunden(self) -> None:
        # Beide Diagonalpixel sind Teilflächen und gehen an die Hauptfläche des Nachbarn.
        zeilen = [[1, 2], [2, 1]]
        self.assertEqual(kg.bereinigen(zeilen, {1: (0, 0), 2: (1, 0)}), (2, set()))
        self.assertEqual(zeilen, [[1, 2], [1, 2]])

    def test_verschachtelte_teilflaechen_pendeln_nicht(self) -> None:
        zeilen = [
            [1, 1, 1, 1, 1, 1],
            [1, 3, 3, 3, 1, 1],
            [1, 3, 2, 3, 1, 1],
            [1, 3, 3, 3, 1, 1],
            [1, 1, 1, 1, 1, 1],
            [2, 2, 3, 3, 3, 3],
        ]
        # Runde 1: die 2 in der Mitte geht an den 3er-Ring; Runde 2: der Ring geht an 1.
        self.assertEqual(kg.bereinigen(zeilen, {1: (0, 0), 2: (0, 5), 3: (5, 5)}), (2, set()))
        self.assertTrue(all(w == 1 for zeile in zeilen[:5] for w in zeile))
        self.assertEqual(zeilen[5], [2, 2, 3, 3, 3, 3])


# ─── Kodierung ──────────────────────────────────────────────────────────────


class TestKodierung(unittest.TestCase):
    def test_wert_kodierung(self) -> None:
        self.assertEqual(kg.kodiere_wert(0), b"\x00\x00\x00")
        self.assertEqual(kg.kodiere_wert(1), b"\x01\x00\x00")
        self.assertEqual(kg.kodiere_wert(255), b"\xff\x00\x00")
        self.assertEqual(kg.kodiere_wert(256), b"\x00\x01\x00")
        self.assertEqual(kg.kodiere_wert(4660), b"\x34\x12\x00")
        self.assertEqual(kg.kodiere_wert(65535), b"\xff\xff\x00")
        for wert in (0, 1, 60, 255, 256, 257, 4660, 65535):
            r, g, b = kg.kodiere_wert(wert)
            self.assertEqual(kg.dekodiere_pixel(r, g, b), wert)
        for falsch in (-1, 65536):
            with self.assertRaises(ValueError):
                kg.kodiere_wert(falsch)
        with self.assertRaises(ValueError):
            kg.dekodiere_pixel(1, 0, 1)

    def test_png_rundreise(self) -> None:
        breite, hoehe = 7, 5
        zeilen = [
            bytes((x * 37 + y * 11 + k * 101) % 256 for x in range(breite) for k in range(3))
            for y in range(hoehe)
        ]
        zeilen[3] = zeilen[2]  # gleiche Zeilen (Up-Filter mit Nullen)
        png = kg.png_kodieren(breite, hoehe, zeilen)
        self.assertEqual(kg.png_dekodieren(png), (breite, hoehe, zeilen))
        self.assertEqual(_bloecke(png), [b"IHDR", b"IDAT", b"IEND"])  # kein tIME, keine Metadaten
        self.assertEqual(struct.unpack(">IIBBBBB", png[16:29]), (breite, hoehe, 8, 2, 0, 0, 0))
        with self.assertRaises(ValueError):
            kg.png_kodieren(breite, hoehe, zeilen[:-1])

    def test_png_decoder_alle_filter(self) -> None:
        breite, hoehe = 6, 4
        zeilen = [
            bytes((x * 53 + y * 29 + k * 7) % 256 for x in range(breite) for k in range(3))
            for y in range(hoehe)
        ]
        for filtertyp in range(5):
            with self.subTest(filtertyp=filtertyp):
                png = _png_mit_filter(breite, hoehe, zeilen, filtertyp)
                self.assertEqual(kg.png_dekodieren(png), (breite, hoehe, zeilen))

    def test_png_decoder_fehler(self) -> None:
        gut = kg.png_kodieren(2, 2, [bytes(6), bytes(6)])
        kaputt = bytearray(gut)
        kaputt[20] ^= 1  # IHDR verändert → Prüfsumme falsch
        for daten in (b"kein png", gut[:30], bytes(kaputt)):
            with self.assertRaises(ValueError):
                kg.png_dekodieren(daten)
        rgba = kg.PNG_SIGNATUR + kg._chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
        with self.assertRaisesRegex(ValueError, "nicht unterstütztes PNG-Format"):
            kg.png_dekodieren(rgba + kg._chunk(b"IDAT", zlib.compress(b"\x00" + bytes(4))))
        blau = kg.png_kodieren(1, 1, [b"\x01\x00\x07"])
        with self.assertRaisesRegex(ValueError, "Blaukanal"):
            kg.werte_aus_png(blau)

    def test_provinzen_png_der_fixture(self) -> None:
        karte, orte = _fixture_laden()
        ergebnis = kg.erzeuge(karte, orte)
        breite, hoehe, werte = kg.werte_aus_png(kg.provinzen_png(ergebnis))
        self.assertEqual((breite, hoehe), (120, 192))
        erwartet = [[0 if w == kg.MEER else w for w in zeile] for zeile in ergebnis.zeilen]
        self.assertEqual(werte, erwartet)
        self.assertEqual({w for zeile in werte for w in zeile}, {0, 1, 2, 3, 4})

    def test_definition_csv(self) -> None:
        _, orte = _fixture_laden()
        self.assertEqual(
            kg.definition_csv(orte).decode("utf-8"),
            "kartenindex;ort;name\n0;hafen;Hafenstadt\n1;heide;Heide-Süd\n2;burg;Burg (Altmark)\n3;aue;Aue\n",
        )


# ─── Determinismus und Kommandozeile ────────────────────────────────────────


class TestKommandozeile(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="kartengenerator-test-")
        self.tmp = Path(self._tmp.name)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def test_determinismus(self) -> None:
        karte, orte = _fixture_laden()
        a = kg.erzeuge(karte, orte)
        b = kg.erzeuge(karte, orte)
        self.assertEqual(a.zeilen, b.zeilen)
        self.assertEqual(kg.provinzen_png(a), kg.provinzen_png(b))

    def test_zweimal_ausfuehren_byteidentisch(self) -> None:
        for lauf in ("eins", "zwei"):
            code, aus, err = _starte("--daten", str(TESTDATEN), "--ausgabe", str(self.tmp / lauf))
            self.assertEqual(code, 0, err)
            self.assertEqual(err, "")
            self.assertIn("geschrieben:", aus)
        for name in ("provinzen.png", "definition.csv"):
            self.assertEqual((self.tmp / "eins" / name).read_bytes(), (self.tmp / "zwei" / name).read_bytes())
        code, aus, _ = _starte("--daten", str(TESTDATEN), "--ausgabe", str(self.tmp / "eins"))
        self.assertEqual(code, 0)
        self.assertEqual(aus.count("unverändert:"), 2)

    def test_erwartete_ausgabe_der_fixture(self) -> None:
        # Die mitgelieferten Dateien entsprechen dem aktuellen Algorithmus (plattformunabhängig).
        code, aus, err = _starte("--daten", str(TESTDATEN), "--pruefen")
        self.assertEqual(code, 0, err)
        self.assertIn("Karte aktuell", aus)
        neu = self.tmp / "neu"
        self.assertEqual(_starte("--daten", str(TESTDATEN), "--ausgabe", str(neu))[0], 0)
        self.assertEqual(
            (neu / "definition.csv").read_bytes(), (TESTDATEN / "karte" / "definition.csv").read_bytes()
        )

    def test_pruefen(self) -> None:
        ausgabe = self.tmp / "aus"
        argumente = ("--daten", str(TESTDATEN), "--ausgabe", str(ausgabe))

        code, _, err = _starte(*argumente, "--pruefen")  # Dateien fehlen
        self.assertEqual(code, 1)
        self.assertIn("definition.csv: Datei fehlt", err)
        self.assertIn("provinzen.png: Datei fehlt", err)
        self.assertFalse(ausgabe.exists())  # --pruefen schreibt nichts

        self.assertEqual(_starte(*argumente)[0], 0)
        self.assertEqual(_starte(*argumente, "--pruefen")[0], 0)

        # Gleiche Pixel, anders komprimiert (andere zlib-Stufe und Filter): weiterhin aktuell.
        png_pfad = ausgabe / "provinzen.png"
        original = png_pfad.read_bytes()
        breite, hoehe, zeilen = kg.png_dekodieren(original)
        png_pfad.write_bytes(_png_mit_filter(breite, hoehe, zeilen, 4, stufe=1))
        self.assertNotEqual(png_pfad.read_bytes(), original)
        self.assertEqual(_starte(*argumente, "--pruefen")[0], 0)

        # Ein Pixel geändert.
        zeilen[100] = b"\x02\x00\x00" + zeilen[100][3:]
        png_pfad.write_bytes(kg.png_kodieren(breite, hoehe, zeilen))
        code, _, err = _starte(*argumente, "--pruefen")
        self.assertEqual(code, 1)
        self.assertIn("1 Pixel weichen ab", err)

        # Falsche Größe und unlesbare Datei.
        png_pfad.write_bytes(kg.png_kodieren(1, 1, [bytes(3)]))
        self.assertIn("Bildgröße 1×1, erwartet 120×192", _starte(*argumente, "--pruefen")[2])
        png_pfad.write_bytes(b"kaputt")
        self.assertIn("nicht lesbar", _starte(*argumente, "--pruefen")[2])

        # definition.csv veraltet.
        png_pfad.write_bytes(original)
        csv_pfad = ausgabe / "definition.csv"
        csv_pfad.write_bytes(csv_pfad.read_bytes().replace(b"Aue", b"Auen"))
        code, _, err = _starte(*argumente, "--pruefen")
        self.assertEqual(code, 1)
        self.assertIn("definition.csv: Inhalt veraltet", err)
        self.assertNotIn("provinzen.png", err)

    def test_vorschau(self) -> None:
        vorschau = self.tmp / "unter" / "vorschau.png"
        code, aus, err = _starte(
            "--daten", str(TESTDATEN), "--ausgabe", str(self.tmp / "aus"), "--vorschau", str(vorschau)
        )
        self.assertEqual(code, 0, err)
        self.assertIn("Vorschau:", aus)
        breite, hoehe, zeilen = kg.png_dekodieren(vorschau.read_bytes())
        self.assertEqual((breite, hoehe), (120, 192))
        farben = {zeile[k : k + 3] for zeile in zeilen for k in range(0, len(zeile), 3)}
        self.assertIn(bytes(kg.FARBE_MEER), farben)
        self.assertIn(bytes(kg.FARBE_STAATSGRENZE), farben)
        self.assertIn(bytes((58, 95, 143)), farben)  # Nordland, Grundton

    def test_datenfehler_exitcode_2(self) -> None:
        daten = self.tmp / "daten"
        shutil.copytree(TESTDATEN, daten)
        pfad = daten / "karte" / "karte.chron"
        pfad.write_text(
            pfad.read_text(encoding="utf-8").replace("10.60 52.96", "10.30 52.96"), encoding="utf-8"
        )
        code, _, err = _starte("--daten", str(daten), "--ausgabe", str(self.tmp / "aus"))
        self.assertEqual(code, 2)
        self.assertIn("Fehler: karte/karte.chron:19: 'kueste': die Längengrade müssen streng steigen", err)
        self.assertIn("Abbruch: 1 Fehler", err)
        self.assertFalse((self.tmp / "aus").exists())

    def test_hilfe_und_aufruffehler_deutsch(self) -> None:
        for argumente, code_erwartet, text in (
            (["--hilfe"], 0, "Optionen:"),
            (["--unbekannt"], 2, "Fehler: unbekannte Argumente: --unbekannt"),
            (["--daten"], 2, "Fehler: Option --daten: erwartet einen Wert"),
            (["--pruef"], 2, "Fehler: unbekannte Argumente: --pruef"),  # keine Abkürzungen
        ):
            with self.subTest(argumente=argumente):
                aus, err = io.StringIO(), io.StringIO()
                with contextlib.redirect_stdout(aus), contextlib.redirect_stderr(err):
                    with self.assertRaises(SystemExit) as beendet:
                        kg.main(argumente)
                self.assertEqual(beendet.exception.code, code_erwartet)
                self.assertIn(text, aus.getvalue() + err.getvalue())
                self.assertIn("Aufruf: kartengenerator.py", aus.getvalue() + err.getvalue())

    def test_skript_direkt(self) -> None:
        ausgabe = self.tmp / "aus"
        lauf = subprocess.run(
            [
                sys.executable,
                str(HIER / "kartengenerator.py"),
                "--daten",
                str(TESTDATEN),
                "--ausgabe",
                str(ausgabe),
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            check=False,
        )
        self.assertEqual(lauf.returncode, 0, lauf.stderr)
        self.assertIn("4 Orte, Bild 120×192", lauf.stdout)
        # Pixel vergleichen (die Kompression kann je zlib-Version abweichen).
        self.assertEqual(
            kg.werte_aus_png((ausgabe / "provinzen.png").read_bytes()),
            kg.werte_aus_png((TESTDATEN / "karte" / "provinzen.png").read_bytes()),
        )


if __name__ == "__main__":
    unittest.main()
