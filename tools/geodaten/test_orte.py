"""Tests für orte.py (Orte und Kreise des Kernraums, Meilenstein M3) – ohne Netz.

Aufruf: python3 -m unittest discover -s tools/geodaten

Geprüft werden die Bausteine mit kleinen, von Hand gebauten Daten (Namen und Namensbelege,
Kennungen, Landschaftsregeln, Gelände-Regeln, Haffe, Kreisbildung, Exklaven, Determinismus der
Kreisbildung) sowie die eingecheckten Hilfstabellen und die erzeugten Tabellen in data/tabellen.

Der vollständige Lauf (rund 70 s, braucht GeoNames, Höhenkacheln und Natural Earth im
Zwischenspeicher build/geodaten-cache) läuft nur mit ``ORTE_VOLLTEST=1``; er erzeugt alles neu und
verlangt Byte-Gleichheit mit den eingecheckten Tabellen (wie ``orte.py --pruefen``).
"""

from __future__ import annotations

import csv
import io
import os
import random
import re
import sys
import unittest
from pathlib import Path

HIER = Path(__file__).resolve().parent
sys.path.insert(0, str(HIER))

import numpy as np  # noqa: E402

import orte  # noqa: E402
import staedte  # noqa: E402

TABELLEN = orte.TABELLEN
GELAENDE_CHRON = orte.DATEN / "common/gelaende.chron"


def stadt(
    name: str,
    alternativ: tuple[str, ...] = (),
    staat: str = "PL",
    code: str = "PPL",
    einwohner: int = 1000,
    gid: int = 1,
    admin: tuple[str, str, str, str] = ("", "", "", ""),
) -> staedte.Stadt:
    return staedte.Stadt(
        geonameid=gid,
        name=name,
        asciiname=name,
        alternativnamen=alternativ,
        breite=51.0,
        laenge=17.0,
        merkmalsklasse="P",
        merkmalscode=code,
        staat=staat,
        weitere_staaten=(),
        admin1=admin[0],
        admin2=admin[1],
        admin3=admin[2],
        admin4=admin[3],
        einwohner=einwohner,
        hoehe=None,
        dem=None,
        zeitzone="Europe/Warsaw",
        geaendert="2020-01-01",
    )


def gelaendetypen() -> set[str]:
    return set(re.findall(r"^gelaende\s+(\w+)\s*\{", GELAENDE_CHRON.read_text(encoding="utf-8"), re.MULTILINE))


# ─── Namen ──────────────────────────────────────────────────────────────────


class TestNamen(unittest.TestCase):
    def test_bereinige_name(self) -> None:
        self.assertEqual(orte.bereinige_name("Lutherstadt Wittenberg"), "Wittenberg")
        self.assertEqual(orte.bereinige_name("Ostseebad Kühlungsborn"), "Kühlungsborn")
        self.assertEqual(orte.bereinige_name("Eisleben Lutherstadt"), "Eisleben")
        self.assertEqual(orte.bereinige_name("Aspang Markt"), "Aspang")
        self.assertEqual(orte.bereinige_name("Zwettl Stadt"), "Zwettl")
        self.assertEqual(orte.bereinige_name("Straßburg-Stadt"), "Straßburg")
        self.assertEqual(orte.bereinige_name("Sankt Peter in der Au Markt"), "Sankt Peter in der Au")
        # Zusatz gehört zum Namen: nach Artikel oder in einer Fügung mit „am“, „im“ …
        self.assertEqual(orte.bereinige_name("Weil der Stadt"), "Weil der Stadt")
        self.assertEqual(orte.bereinige_name("Stetten am Kalten Markt"), "Stetten am Kalten Markt")
        self.assertEqual(orte.bereinige_name("Neukirchen am Markt"), "Neukirchen am Markt")
        self.assertEqual(orte.bereinige_name("Markt"), "Markt")
        self.assertEqual(orte.bereinige_name("Neumarkt Stadt"), "Neumarkt")
        # „der Stadt“ bleibt, ein Zusatz allein bleibt, „Bad“ wird nicht entfernt
        self.assertEqual(orte.bereinige_name("Haus der Stadt"), "Haus der Stadt")
        self.assertEqual(orte.bereinige_name("Kurort "), "Kurort")
        self.assertEqual(orte.bereinige_name("Bad Ischl"), "Bad Ischl")

    def test_namensbeleg(self) -> None:
        s = stadt("Braniewo", ("Braneva", "Braunsberg  Ostpreussen", "Braunsberg  Ostpreußen"))
        self.assertEqual(orte.namensbeleg("Braniewo", s), "Braniewo")
        self.assertEqual(orte.namensbeleg("Braneva", s), "Braneva")
        # Name mit Ortszusatz: die erste passende Form in GeoNames-Reihenfolge
        self.assertEqual(orte.namensbeleg("Braunsberg", s), "Braunsberg  Ostpreussen")
        self.assertIsNone(orte.namensbeleg("Brauns", s))  # kein Trennzeichen nach dem Namen
        self.assertIsNone(orte.namensbeleg("Braunsberg  Ostpreußen und mehr", s))
        t = stadt("Lanškroun", ("Landskron/Schönhengstgau",))
        self.assertEqual(orte.namensbeleg("Landskron", t), "Landskron/Schönhengstgau")
        # Zusatz klein geschrieben: kein Ortszusatz, sondern ein anderer Name
        self.assertIsNone(orte.namensbeleg("Grabow", stadt("Grabów nad Prosną", ("Grabow nad Prosna",))))

    def test_waehle_name_exonym(self) -> None:
        s = stadt("Braniewo", ("Braunsberg  Ostpreußen",), gid=5)
        wahl = orte.waehle_name(s, {5: ("Braunsberg", True, "Braniewo")})
        self.assertEqual(wahl.name, "Braunsberg")
        self.assertIn("Ortszusatz", wahl.herkunft)
        self.assertIn("heute Braniewo", wahl.herkunft)
        self.assertNotIn(";", wahl.herkunft)  # Teile der Quellenangabe sind durch „; “ getrennt
        wahl = orte.waehle_name(s, {5: ("Braunsberg", False, "Braniewo")})
        self.assertEqual(wahl.name, "Braniewo")
        self.assertIn("kein abweichender deutscher Name", wahl.herkunft)
        wahl = orte.waehle_name(stadt("Breslau", ("Wrocław",), gid=6), {6: ("Breslau", True, "Breslau")})
        self.assertEqual(wahl.name, "Breslau")
        self.assertIn("Hauptname", wahl.herkunft)

    def test_waehle_name_deutsche_staaten_und_suedtirol(self) -> None:
        self.assertEqual(orte.waehle_name(stadt("Lutherstadt Eisleben", staat="DE"), {}).name, "Eisleben")
        self.assertEqual(orte.waehle_name(stadt("Wien", staat="AT"), {}).herkunft, "Name nach GeoNames")
        muenchen = stadt("Munich", ("München", "Muenchen"), staat="DE", gid=7)
        wahl = orte.waehle_name(muenchen, {7: ("München", True, "Munich")})
        self.assertEqual(wahl.name, "München")
        self.assertIn("GeoNames: Munich", wahl.herkunft)
        self.assertEqual(orte.waehle_name(stadt("Bolzano - Bozen", staat="IT"), {}).name, "Bozen")
        self.assertEqual(orte.waehle_name(stadt("Trento", staat="IT"), {}).name, "Trento")

    def test_pruefe_exonyme(self) -> None:
        s = stadt("Braniewo", ("Braunsberg  Ostpreußen",), gid=5)
        nach_id = {5: s}
        self.assertEqual(orte.pruefe_exonyme({5: ("Braunsberg", True, "Braniewo")}, nach_id), [])
        fehler = orte.pruefe_exonyme({5: ("Braunsberg", False, "Braniewo")}, nach_id)
        self.assertEqual(len(fehler), 1)
        self.assertIn("geprueft muss ja sein", fehler[0])
        fehler = orte.pruefe_exonyme({5: ("Brunsberg", True, "Braniewo")}, nach_id)
        self.assertIn("geprueft muss nein sein", fehler[0])
        fehler = orte.pruefe_exonyme({5: ("Braunsberg", True, "Braunsberg")}, nach_id)
        self.assertIn("Spalte heute", fehler[0])
        # Einträge für Orte, die keine Kandidaten sind, wirken nicht und werden nicht geprüft
        self.assertEqual(orte.pruefe_exonyme({9: ("Irgendwo", True, "x")}, nach_id), [])

    def test_zusatz_bei_gleichnamigen(self) -> None:
        namen = ["Neustadt", "Neustadt", "Berg", "Halle (Saale)", "Halle (Saale)"]
        zusaetze = ["Sachsen", "Franken", "Bayern", "Anhalt", "Anhalt"]
        self.assertEqual(
            orte.zusatz_bei_gleichnamigen(namen, zusaetze, []),
            ["Neustadt (Sachsen)", "Neustadt (Franken)", "Berg", "Halle (Saale)", "Halle (Saale)"],
        )
        # gleichnamig mit einem festen (M1-)Ort: nur der neue Ort erhält den Zusatz
        self.assertEqual(orte.zusatz_bei_gleichnamigen(["Berg"], ["Bayern"], ["Berg"]), ["Berg (Bayern)"])


# ─── Kennungen ──────────────────────────────────────────────────────────────


class TestKennungen(unittest.TestCase):
    def test_kennung_aus_name(self) -> None:
        faelle = {
            "Königsberg": "koenigsberg",
            "Straßburg": "strassburg",
            "Łódź": "lodz",
            "Świętajno": "swietajno",
            "Frankfurt (Oder)": "frankfurt_oder",
            "Bol'shoe Isakovo": "bol_shoe_isakovo",
            "Šentilj v Slov. Goricah": "sentilj_v_slov_goricah",
            "Ærøskøbing": "aeroeskoebing",
            "Neustadt an der Weinstraße": "neustadt_an_der_weinstrasse",
            "1. Mai": "ort_1_mai",
        }
        for name, kennung in faelle.items():
            self.assertEqual(orte.kennung_aus_name(name), kennung, name)
        with self.assertRaises(orte.OrteFehler):
            orte.kennung_aus_name("???")

    def test_vergebe_kennungen_landschaft_fluss_zahl(self) -> None:
        self.assertEqual(
            orte.vergebe_kennungen(
                ["neustadt", "neustadt", "berg"], ["sachsen", "franken", "bayern"], ["", "", ""], set()
            ),
            ["neustadt_sachsen", "neustadt_franken", "berg"],
        )
        self.assertEqual(
            orte.vergebe_kennungen(["neustadt", "neustadt"], ["sachsen", "sachsen"], ["elbe", "mulde"], set()),
            ["neustadt_sachsen_elbe", "neustadt_sachsen_mulde"],
        )
        self.assertEqual(
            orte.vergebe_kennungen(["neustadt", "neustadt"], ["sachsen", "sachsen"], ["", ""], set()),
            ["neustadt_sachsen", "neustadt_sachsen_2"],
        )
        self.assertEqual(
            orte.vergebe_kennungen(["a", "a"], ["x", "x"], ["f", "f"], set()),
            ["a_x_f", "a_x_f_2"],
        )
        # schon vergebene Kennungen (M1-Orte) werden nicht noch einmal vergeben
        self.assertEqual(orte.vergebe_kennungen(["berlin"], ["brandenburg"], [""], {"berlin"}), ["berlin_brandenburg"])

    def test_vergebe_kennungen_eindeutig(self) -> None:
        rnd = random.Random(7)
        basen = [rnd.choice(["au", "berg", "dorf", "feld"]) for _ in range(300)]
        landschaften = [rnd.choice(["a", "b", "c"]) for _ in range(300)]
        fluesse = [rnd.choice(["", "elbe", "oder"]) for _ in range(300)]
        belegt = {"au", "berg_a", "dorf_b_elbe"}
        kennungen = orte.vergebe_kennungen(basen, landschaften, fluesse, belegt)
        self.assertEqual(len(set(kennungen)), 300)
        self.assertFalse(set(kennungen) & belegt)
        for kennung, basis in zip(kennungen, basen):
            self.assertTrue(kennung == basis or kennung.startswith(basis + "_"))


# ─── Landschaften ───────────────────────────────────────────────────────────


class TestLandschaftsregeln(unittest.TestCase):
    def test_tiefste_regel_und_leermarke(self) -> None:
        regeln = [
            orte.Zuordnungsregel("PL", ("77", "", "", ""), "oberschlesien", 2),
            orte.Zuordnungsregel("PL", ("77", "-", "", ""), "brandenburg", 3),
            orte.Zuordnungsregel("DE", ("10", "01058", "", ""), "schleswig", 4),
            orte.Zuordnungsregel("DE", ("10", "01058", "", "01058135"), "holstein", 5),
            orte.Zuordnungsregel("DE", ("", "", "", ""), "deutschland", 6),
        ]
        nach = orte.landschaft_nach_regeln
        self.assertEqual(nach(regeln, "PL", ("77", "1261", "", "")), "oberschlesien")
        self.assertEqual(nach(regeln, "PL", ("77", "", "", "")), "brandenburg")  # „-“: Feld muss leer sein
        self.assertEqual(nach(regeln, "DE", ("10", "01058", "", "01058001")), "schleswig")
        self.assertEqual(nach(regeln, "DE", ("10", "01058", "", "01058135")), "holstein")
        self.assertEqual(nach(regeln, "DE", ("09", "", "", "")), "deutschland")
        self.assertIsNone(nach(regeln, "CH", ("", "", "", "")))
        self.assertEqual(regeln[1].tiefe, 2)
        self.assertEqual(regeln[4].tiefe, 0)


# ─── Auswahlhilfen ──────────────────────────────────────────────────────────


class TestAuswahlhilfen(unittest.TestCase):
    def test_verwaltungsrang_und_bedeutung(self) -> None:
        self.assertEqual(orte.verwaltungsrang(stadt("Kiel", staat="DE", code="PPLA")), 1)
        self.assertEqual(orte.verwaltungsrang(stadt("Heide", staat="DE", code="PPLA3")), 2)
        self.assertEqual(orte.verwaltungsrang(stadt("Ptuj", staat="SI", code="PPLA")), 3)
        with self.assertRaises(orte.OrteFehler):
            orte.verwaltungsrang(stadt("Zürich", staat="CH", code="PPLA"))
        klein = orte.bedeutung(stadt("A", einwohner=0))
        self.assertAlmostEqual(klein, 2.4771212547, places=8)  # log10(300)
        self.assertGreater(orte.bedeutung(stadt("A", einwohner=50_000)), orte.bedeutung(stadt("A", einwohner=5_000)))
        self.assertGreater(orte.bedeutung(stadt("A", code="PPLA2")), klein)
        viele_namen = stadt("A", tuple(f"n{i}" for i in range(1000)), einwohner=0)
        self.assertAlmostEqual(orte.bedeutung(viele_namen) - klein, orte.BEKANNTHEIT_MAX)

    def test_abstandsfaktor(self) -> None:
        self.assertEqual(orte.abstandsfaktor(0.0), orte.FAKTOR_UNBEDEUTEND)
        self.assertEqual(orte.abstandsfaktor(99.0), orte.FAKTOR_BEDEUTEND)
        werte = [orte.abstandsfaktor(3.0 + 0.1 * i) for i in range(45)]
        self.assertTrue(all(a >= b for a, b in zip(werte, werte[1:])))

    def test_punktgitter(self) -> None:
        g = orte.Punktgitter()
        g.neu(13.0, 52.0, "brandenburg")
        abstand = orte.geo.abstand_km(13.0, 52.0, 13.1, 52.0)
        self.assertAlmostEqual(g.min_abstand(13.1, 52.0, 20.0), abstand)
        self.assertEqual(g.min_abstand(15.0, 52.0, 20.0), float("inf"))
        # gleiche Landschaft: voller Radius; andere Landschaft: Radius × Faktor
        self.assertFalse(g.frei(13.1, 52.0, abstand + 0.5, "brandenburg", 0.8))
        self.assertTrue(g.frei(13.1, 52.0, abstand + 0.5, "pommern", 0.8))

    def test_lueckenfuellung_fern_von_jedem_ort(self) -> None:
        # Land, das weiter als FUELL_KARTE_KM von jedem Ort liegt (Abstand unendlich), bricht die
        # Lückenfüllung nicht ab: Die Suche ist auf FUELL_SUCHE_MAX_KM begrenzt.
        from types import SimpleNamespace

        karte = orte.kr.Karte(
            breite=300,
            hoehe=100,
            laenge_min=10.0,
            laenge_max=11.4,
            breite_min=50.5,
            breite_max=51.0,
            bezugsbreite=50.75,
            max_radius_km=30,
        )
        art = np.full((100, 300), orte.kr.KERNLAND, dtype=np.uint8)
        grundlage = SimpleNamespace(karte=karte, raster=SimpleNamespace(karte=karte, art=art))
        raster = orte.Raster(grundlage, np.zeros((100, 300), dtype=np.int64), 0, {0: 7000.0})  # type: ignore[arg-type]
        a = orte.Kandidat(stadt("A", gid=1), True, "x", 6.0, 2, "10.0500", "50.7500")
        b = orte.Kandidat(stadt("B", gid=2), True, "x", 3.0, 4, "10.1500", "50.7500")  # 7 km neben A
        landschaften = {"x": orte.Landschaft("x", "X", "X", 1.0, "Test")}
        auswahl = orte.waehle_orte([a, b], [], raster, landschaften)
        self.assertEqual(auswahl.herkunft, {1: "scheibe", 2: "fuellung"})
        self.assertGreater(auswahl.leere_zellen, 0)  # der Osten bleibt ohne Kandidaten


# ─── Gelände ────────────────────────────────────────────────────────────────


def merkmale(
    mittel: float,
    p05: float,
    p95: float,
    kueste: float | None = None,
    fluss: float | None = None,
    see: float = 0.0,
    insel: bool = False,
    gebiet: str = "",
) -> orte.Gelaendemerkmale:
    return orte.Gelaendemerkmale(mittel, p05, p95, kueste, fluss, see, insel, gebiet)


class TestGelaende(unittest.TestCase):
    def test_regeln(self) -> None:
        g = orte.gelaende_aus
        self.assertEqual(g(merkmale(40, 0, 90, insel=True)), "kueste")
        self.assertEqual(g(merkmale(10, 0, 30, kueste=3.0)), "kueste")
        self.assertEqual(g(merkmale(50, 30, 70, kueste=3.0)), "flachland")  # nahe am Meer, aber nicht niedrig
        self.assertEqual(g(merkmale(900, 600, 1600)), "hochgebirge")
        self.assertEqual(g(merkmale(1300, 1250, 1350)), "hochgebirge")
        self.assertEqual(g(merkmale(450, 300, 650)), "mittelgebirge")
        self.assertEqual(g(merkmale(500, 420, 600)), "mittelgebirge")  # Hochfläche mit merklichem Relief
        self.assertEqual(g(merkmale(520, 500, 545)), "flachland")  # flache Hochebene (Münchner Ebene)
        self.assertEqual(g(merkmale(100, 70, 130, gebiet="boerde")), "boerde")
        self.assertEqual(g(merkmale(100, 70, 130, gebiet="heide", fluss=1.0)), "heide")  # Gebiet vor Flussaue
        self.assertEqual(g(merkmale(150, 50, 250, gebiet="boerde")), "huegelland")  # Relief zu stark
        self.assertEqual(g(merkmale(60, 40, 90, fluss=1.0)), "flussaue")
        self.assertEqual(g(merkmale(100, 40, 180, fluss=1.0)), "huegelland")  # hoch über der Talsohle
        self.assertEqual(g(merkmale(80, 60, 100, see=0.08)), "seenplatte")
        self.assertEqual(g(merkmale(250, 230, 270, see=0.08)), "flachland")  # nicht im Tiefland
        self.assertEqual(g(merkmale(150, 100, 220)), "huegelland")
        self.assertEqual(g(merkmale(260, 230, 290)), "huegelland")  # Hochfläche mit etwas Relief
        self.assertEqual(g(merkmale(60, 50, 80)), "flachland")

    def test_alle_typen_in_gelaende_chron(self) -> None:
        bekannt = gelaendetypen()
        for typ in ("kueste", "hochgebirge", "mittelgebirge", "boerde", "heide", "flussaue", "seenplatte"):
            self.assertIn(typ, bekannt)
        self.assertIn("huegelland", bekannt)
        self.assertIn("flachland", bekannt)

    def test_gebiet_bei(self) -> None:
        gebiete = [
            orte.Gelaendegebiet("boerde", "A", 10.0, 50.0, 20.0),
            orte.Gelaendegebiet("heide", "B", 10.3, 50.0, 30.0),
        ]
        # in beiden Kreisen: das Gebiet, dessen Mitte relativ zum Radius näher liegt
        self.assertEqual(orte.gebiet_bei(gebiete, 10.25, 50.0), "heide")
        self.assertEqual(orte.gebiet_bei(gebiete, 9.9, 50.0), "boerde")
        self.assertEqual(orte.gebiet_bei(gebiete, 12.0, 50.0), "")

    def test_haffe(self) -> None:
        kr = orte.kr
        art = np.full((30, 30), kr.KERNLAND, dtype=np.uint8)
        art[:, :5] = kr.MEER
        art[5:15, 5:10] = kr.BINNENGEWAESSER  # 50 Pixel, grenzt ans Meer: Haff
        art[20:22, 5:8] = kr.BINNENGEWAESSER  # 6 Pixel, grenzt ans Meer: zu klein
        art[2:12, 15:20] = kr.BINNENGEWAESSER  # 50 Pixel, kein Meer: See
        art[25:28, 6:20] = kr.BINNENGEWAESSER  # 42 Pixel, berührt das Meer nur diagonal (24, 5)–(25, 6)
        art[24, 5] = kr.MEER
        haff = orte.haffe(art, pixel_km2=1.0, min_km2=20.0)
        self.assertTrue(haff[5:15, 5:10].all())
        self.assertFalse(haff[20:22, 5:8].any())
        self.assertFalse(haff[2:12, 15:20].any())
        self.assertTrue(haff[25:28, 6:20].all())  # diagonale Berührung (24, 5)–(25, 6) zählt
        self.assertEqual(int(haff.sum()), 50 + 42)
        self.assertFalse(orte.haffe(np.full((4, 4), kr.KERNLAND, dtype=np.uint8), 1.0).any())


# ─── Kreise ─────────────────────────────────────────────────────────────────


def gitter(zeilen: int, spalten: int, start: int = 0, laenge0: float = 10.0) -> tuple[list, list, list]:
    """Orte auf einem Gitter (Abstand etwa 7 km) mit Nachbarschaft nach rechts und unten."""
    knoten, paare, lagen = [], [], []
    for z in range(zeilen):
        for s in range(spalten):
            i = start + z * spalten + s
            knoten.append(i)
            lagen.append((laenge0 + 0.1 * s, 50.0 + 0.063 * z))
            if s + 1 < spalten:
                paare.append((i, i + 1))
            if z + 1 < zeilen:
                paare.append((i, i + spalten))
    return knoten, paare, lagen


def zusammenhaengend(gruppe: list[int], paare: list[tuple[int, int]]) -> bool:
    kanten: dict[int, list[tuple[int, float]]] = {k: [] for k in gruppe}
    for a, b in paare:
        if a in kanten and b in kanten:
            kanten[a].append((b, 0.0))
            kanten[b].append((a, 0.0))
    return len(orte._zusammenhaengend(gruppe, kanten)) == 1


class TestKreisbildung(unittest.TestCase):
    def pruefe_gruppen(self, gruppen: list[list[int]], knoten: list[int], paare: list | None) -> None:
        self.assertEqual(sorted(k for g in gruppen for k in g), sorted(knoten))
        for g in gruppen:
            self.assertTrue(orte.KREIS_MIN <= len(g) <= orte.KREIS_MAX, g)
            self.assertEqual(g, sorted(g))
            if paare is not None:
                self.assertTrue(zusammenhaengend(g, paare), g)
        self.assertEqual(gruppen, sorted(gruppen, key=lambda g: g[0]))

    def test_gitter_aller_groessen(self) -> None:
        for zeilen in range(1, 9):
            for spalten in range(1, 13):
                if zeilen * spalten < orte.KREIS_MIN:
                    continue
                knoten, paare, lagen = gitter(zeilen, spalten)
                with self.subTest(zeilen=zeilen, spalten=spalten):
                    gruppen = orte.kreise_einer_landschaft(knoten, paare, lagen)
                    self.pruefe_gruppen(gruppen, knoten, paare)
                    if zeilen * spalten >= 30:
                        mittel = zeilen * spalten / len(gruppen)
                        self.assertTrue(5.0 <= mittel <= 9.0, mittel)

    def test_kette(self) -> None:
        for n in (4, 5, 12, 13, 25, 50):
            knoten, paare, lagen = gitter(1, n)
            self.pruefe_gruppen(orte.kreise_einer_landschaft(knoten, paare, lagen), knoten, paare)

    def test_deterministisch_unabhaengig_von_der_reihenfolge(self) -> None:
        knoten, paare, lagen = gitter(9, 11)
        erwartet = orte.kreise_einer_landschaft(knoten, paare, lagen)
        rnd = random.Random(3)
        for _ in range(5):
            gemischt = [(b, a) if rnd.random() < 0.5 else (a, b) for a, b in paare]
            rnd.shuffle(gemischt)
            k = list(knoten)
            rnd.shuffle(k)
            self.assertEqual(orte.kreise_einer_landschaft(k, gemischt, lagen), erwartet)

    def test_teile_ohne_landverbindung(self) -> None:
        knoten, paare, lagen = gitter(5, 5)
        insel_knoten, insel_paare, insel_lagen = gitter(1, 2, start=25, laenge0=10.9)
        alle = knoten + insel_knoten
        gruppen = orte.kreise_einer_landschaft(alle, paare + insel_paare, lagen + insel_lagen)
        self.pruefe_gruppen(gruppen, alle, None)

    def test_zu_wenige_orte(self) -> None:
        knoten, paare, lagen = gitter(1, 3)
        with self.assertRaises(orte.OrteFehler):
            orte.kreise_einer_landschaft(knoten, paare, lagen)

    def test_fremde_knoten_in_nachbarn_werden_ignoriert(self) -> None:
        knoten, paare, lagen = gitter(3, 4)
        lagen = lagen + [(20.0, 50.0)]
        gruppen = orte.kreise_einer_landschaft(knoten, paare + [(0, 12), (12, 5)], lagen)
        self.pruefe_gruppen(gruppen, knoten, paare)


class TestExklaven(unittest.TestCase):
    def test_exklave_zur_umgebenden_landschaft(self) -> None:
        # a: Kette 0–4; b: Kette 5–10; 11 gehört zu a, liegt aber nur an b (Exklave)
        landschaften = ["a"] * 5 + ["b"] * 6 + ["a"]
        nachbarn = {(i, i + 1): 10 for i in range(4)}
        nachbarn.update({(i, i + 1): 10 for i in range(5, 10)})
        nachbarn.update({(4, 5): 3, (9, 11): 7, (10, 11): 5})
        ergebnis = orte.exklaven_zuordnen(landschaften, nachbarn)
        self.assertEqual(ergebnis, ["a"] * 5 + ["b"] * 7)

    def test_insel_und_kleine_landschaft_bleiben(self) -> None:
        # 5 (a) ohne Nachbarn: Insel, bleibt a; c besteht nur aus zwei kleinen Teilen: bleibt c
        landschaften = ["a"] * 5 + ["a"] + ["c", "b", "c"]
        nachbarn = {(i, i + 1): 10 for i in range(4)}
        nachbarn.update({(4, 6): 4, (6, 7): 4, (7, 8): 4})
        self.assertEqual(orte.exklaven_zuordnen(landschaften, nachbarn), landschaften)


# ─── Eingecheckte Hilfstabellen ─────────────────────────────────────────────


class TestHilfstabellen(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.tabellen = orte.lade_hilfstabellen()

    def test_laden_und_umfang(self) -> None:
        t = self.tabellen
        self.assertGreaterEqual(len(t.landschaften), 70)
        self.assertGreater(len(t.regeln), 100)
        self.assertGreater(len(t.exonyme), 500)
        self.assertGreater(len(t.ausschluesse), 10)
        for l in t.landschaften.values():
            self.assertEqual(l.kennung, orte.kennung_aus_name(l.kennung))
        # jede Landschaft wird von einer Regel benutzt
        self.assertEqual({r.landschaft for r in t.regeln}, set(t.landschaften))

    def test_exonyme(self) -> None:
        for gid, (name, geprueft, heute) in self.tabellen.exonyme.items():
            self.assertTrue(name.strip() == name and heute.strip() == heute, gid)
            self.assertNotIn(gid, self.tabellen.ausschluesse)

    def test_einwohner(self) -> None:
        # Verwaltungssitze, die GeoNames mit 0 Einwohnern führt (Osterode am Harz, Avesnes-sur-Helpe)
        einwohner = self.tabellen.einwohner
        self.assertEqual(einwohner[2856405][:2], ("Osterode am Harz", 20000))
        self.assertIn(3035698, einwohner)
        for gid, (name, zahl, beleg) in einwohner.items():
            self.assertTrue(name and zahl >= 1 and beleg, gid)
            self.assertNotIn(gid, self.tabellen.ausschluesse)
        osterode = staedte.Kandidat(stadt("Osterode am Harz", staat="DE", code="PPLA3", einwohner=0, gid=2856405), True, 0.0)
        andere = staedte.Kandidat(stadt("Gittelde", staat="DE", einwohner=2087, gid=7), True, 0.0)
        neu = orte.setze_einwohner([osterode, andere], einwohner)
        self.assertEqual([k.stadt.einwohner for k in neu], [20000, 2087])
        self.assertGreater(orte.bedeutung(neu[0].stadt), orte.bedeutung(andere.stadt))
        falsch = staedte.Kandidat(stadt("Osterode", gid=2856405), True, 0.0)
        with self.assertRaises(orte.OrteFehler):  # Name passt nicht mehr zum GeoNames-Eintrag
            orte.setze_einwohner([falsch], einwohner)

    def test_gelaende_gebiete(self) -> None:
        gebiete = orte.lade_gelaende_gebiete(HIER / "gelaende_gebiete.csv", gelaendetypen())
        self.assertGreater(len(gebiete), 5)
        for g in gebiete:
            self.assertTrue(45.1 <= g.breite <= 56.2 and 1.8 <= g.laenge <= 23.5 and 0 < g.radius_km <= 40, g.name)

    def test_csv_mit_fehlern(self) -> None:
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            ordner = Path(tmp)
            for name in ("landschaften.csv", "landschaften_dateien.csv", "exonyme.csv", "ausschluesse.csv", "einwohner.csv"):
                (ordner / name).write_bytes((HIER / name).read_bytes())
            orte.lade_hilfstabellen(ordner)  # unverändert: in Ordnung
            text = (ordner / "einwohner.csv").read_text(encoding="utf-8")
            (ordner / "einwohner.csv").write_text(text + "1;Testdorf;0;Beleg\n", encoding="utf-8")
            with self.assertRaises(orte.OrteFehler):  # Einwohnerzahl < 1
                orte.lade_hilfstabellen(ordner)
            (ordner / "einwohner.csv").write_text(text, encoding="utf-8")
            text = (ordner / "exonyme.csv").read_text(encoding="utf-8")
            (ordner / "exonyme.csv").write_text(text + "2761369;Wien;ja;Vienna\n", encoding="utf-8")
            with self.assertRaises(orte.OrteFehler):  # doppelte geonameid
                orte.lade_hilfstabellen(ordner)
            (ordner / "exonyme.csv").write_text(text, encoding="utf-8")
            text = (ordner / "landschaften.csv").read_text(encoding="utf-8")
            (ordner / "landschaften.csv").write_text(text + "XX;;;;;atlantis;Test\n", encoding="utf-8")
            with self.assertRaises(orte.OrteFehler):  # unbekannte Landschaft
                orte.lade_hilfstabellen(ordner)


# ─── Erzeugte Tabellen in data/tabellen ─────────────────────────────────────


class TestErzeugteTabellen(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.orte = orte.lies_csv(TABELLEN / "orte.csv", orte.ORTE_SPALTEN)
        cls.kreise = orte.lies_csv(TABELLEN / "kreise.csv", orte.KREISE_SPALTEN)

    def test_umfang(self) -> None:
        self.assertTrue(4200 <= len(self.orte) <= 4800, len(self.orte))
        self.assertTrue(560 <= len(self.kreise) <= 700, len(self.kreise))
        m1 = [z for z in self.orte if not z["quelle"].startswith(orte.QUELLE_PRAEFIX)]
        self.assertEqual(len(m1), 60)

    def test_kennungen_und_felder(self) -> None:
        kennungen = [z["id"] for z in self.orte]
        self.assertEqual(len(set(kennungen)), len(kennungen))
        bekannt = gelaendetypen()
        landschaften = orte.lade_hilfstabellen().landschaften
        for z in self.orte:
            self.assertRegex(z["id"], r"^[a-z][a-z0-9_]*$")
            self.assertIn(z["gelaende"], bekannt)
            self.assertIn(z["stadt"], ("ja", "nein"))
            self.assertIn(z["datei"], landschaften)
            self.assertGreater(int(z["flaeche"]), 0)
            if z["quelle"].startswith(orte.QUELLE_PRAEFIX):
                self.assertRegex(z["quelle"], r"^GeoNames \(CC BY 4\.0\), geonameid \d+; ")
                self.assertTrue(z["quelle"].endswith("; Gelände aus Höhenmodell; Fläche aus der Karte"), z["id"])
                self.assertEqual(z["id"], z["id"].strip())

    def test_kreise(self) -> None:
        nach_kreis: dict[str, list[dict[str, str]]] = {}
        for z in self.orte:
            nach_kreis.setdefault(z["kreis"], []).append(z)
        self.assertEqual(set(nach_kreis), {k["id"] for k in self.kreise})
        for k in self.kreise:
            glieder = nach_kreis[k["id"]]
            self.assertTrue(orte.KREIS_MIN <= len(glieder) <= orte.KREIS_MAX, k["id"])
            self.assertEqual(k["quelle"], orte.KREIS_QUELLE)
            hauptort = [z for z in glieder if "kr_" + z["id"] == k["id"]]
            self.assertEqual(len(hauptort), 1, k["id"])
            self.assertEqual(hauptort[0]["name"], k["name"])

    def test_format_stabil(self) -> None:
        for name, spalten in (("orte.csv", orte.ORTE_SPALTEN), ("kreise.csv", orte.KREISE_SPALTEN)):
            text = (TABELLEN / name).read_text(encoding="utf-8")
            zeilen = list(csv.reader(io.StringIO(text, newline=""), delimiter=";"))
            self.assertEqual(tuple(zeilen[0]), spalten)
            self.assertEqual(orte.csv_text(spalten, zeilen[1:]), text, name)

    def test_m1_orte_vorn_und_nur_sie_mit_historie(self) -> None:
        m1 = [z["id"] for z in self.orte if not z["quelle"].startswith(orte.QUELLE_PRAEFIX)]
        self.assertEqual(m1, [z["id"] for z in self.orte[:60]])  # Reihenfolge: M1-Orte unverändert vorn
        # Vor dem Lesezeichen 1848 (Meilenstein M7, tools/geodaten/zuordnung.py) haben nur die M1-Orte
        # Historie; die übrigen sind 1648 Orte ohne Daten.
        mit_historie = set()
        for name, spalte in (("besitz.csv", "ab"), ("bevoelkerung.csv", "stichtag")):
            mit_historie |= {z["ort"] for z in orte.lies_csv(TABELLEN / name) if z[spalte] < "1848"}
        self.assertEqual(mit_historie, set(m1))

    def test_flaeche_wie_karte(self) -> None:
        definition = {z["ort"]: z for z in orte.lies_csv(orte.DATEN / "karte/definition.csv")}
        self.assertEqual(set(definition), {z["id"] for z in self.orte})
        for z in self.orte:
            self.assertEqual(z["flaeche"], definition[z["id"]]["flaeche_km2"], z["id"])

    def test_stichprobe_bekannter_staedte(self) -> None:
        nach_id = {z["id"]: z for z in self.orte}
        kreisname = {k["id"]: k["name"] for k in self.kreise}
        erwartet = {
            "berlin_coelln": ("Berlin-Cölln", "brandenburg"),
            "hamburg": ("Hamburg", "holstein"),
            "muenchen": ("München", "bayern"),
            "wien": ("Wien", "niederoesterreich"),
            "prag": ("Prag", "boehmen"),
            "breslau": ("Breslau", "niederschlesien"),
            "koenigsberg": ("Königsberg", "ostpreussen"),
            "danzig": ("Danzig", "westpreussen"),
            "strassburg": ("Straßburg", "elsass"),
            "koeln": ("Köln", "rheinland"),
            "frankfurt_am_main": ("Frankfurt am Main", "wetterau"),
            "nuernberg": ("Nürnberg", "franken"),
            "dresden": ("Dresden", "sachsen"),
            "leipzig": ("Leipzig", "sachsen"),
            "bruessel": ("Brüssel", "brabant"),
            "memel": ("Memel", "ostpreussen"),
            "glatz": ("Glatz", "glatz"),
            "iglau": ("Iglau", "maehren"),
            "diedenhofen": ("Diedenhofen", "lothringen"),
            "liegnitz": ("Liegnitz", "niederschlesien"),
            "freiburg_im_breisgau": ("Freiburg im Breisgau", "baden"),
            "hirschberg_in_schlesien": ("Hirschberg in Schlesien", "niederschlesien"),
        }
        for kennung, (name, datei) in erwartet.items():
            with self.subTest(kennung):
                z = nach_id[kennung]
                self.assertEqual((z["name"], z["datei"], z["stadt"]), (name, datei, "ja"))
                self.assertEqual(kreisname[z["kreis"]], name)  # jeweils Hauptort ihres Kreises
        # Amsterdam liegt außerhalb des Kernraums (Niederlande nur als Saum)
        self.assertNotIn("amsterdam", nach_id)

    def test_dateien_kommentare(self) -> None:
        zeilen = orte.lies_csv(TABELLEN / "dateien.csv", orte.DATEIEN_SPALTEN)
        orte_dateien = {z["datei"] for z in self.orte}
        self.assertEqual({z["datei"] for z in zeilen if z["ordner"] == "orte"}, orte_dateien)
        self.assertEqual({z["datei"] for z in zeilen if z["ordner"] == "kreise"}, {k["datei"] for k in self.kreise})
        for z in zeilen:
            if z["ordner"] != "staaten":
                self.assertEqual(z["kommentar"].count(orte.M3_ABSATZ), 1 if "Stand 24.10.1648" in z["kommentar"] else 0)


class TestDateienZeilen(unittest.TestCase):
    def setUp(self) -> None:
        self.landschaften = {
            "anhalt": orte.Landschaft("anhalt", "Anhalt", "Anhalt", 1.0, "Fürstentümer Anhalt " + "lang " * 30),
            "glatz": orte.Landschaft("glatz", "Grafschaft Glatz", "Glatz", 1.0, "Grafschaft Glatz"),
        }
        self.alt = [
            {"ordner": "staaten", "datei": "DES", "kultur": "", "kommentar": "Staat\nunverändert"},
            {"ordner": "orte", "datei": "anhalt", "kultur": "obersachsen=100", "kommentar": "Orte: Anhalt – Stand 24.10.1648."},
        ]

    def test_m3_absatz_umgebrochen_und_ersetzt(self) -> None:
        erste = orte.dateien_zeilen(self.alt, self.landschaften, {"anhalt"}, {"anhalt", "glatz"})
        self.assertEqual(erste[0], ("staaten", "DES", "", "Staat\nunverändert"))
        reihenfolge = [("orte", "anhalt"), ("orte", "glatz"), ("kreise", "anhalt"), ("kreise", "glatz")]
        self.assertEqual([(o, d) for o, d, _, _ in erste[1:]], reihenfolge)
        # Die Standardkultur bleibt erhalten, neue Landschaften haben keine.
        self.assertEqual([k for _, _, k, _ in erste[1:]], ["obersachsen=100", "", "", ""])
        anhalt = erste[1][3]
        self.assertTrue(anhalt.startswith("Orte: Anhalt – Stand 24.10.1648.\n\n" + orte.M3_ABSATZ))
        self.assertTrue(all(len(z) <= orte.KOMMENTAR_BREITE for z in anhalt.splitlines()))
        # ein zweiter Lauf über die eigene Ausgabe ersetzt den M3-Absatz, statt ihn zu verdoppeln
        alt2 = self.alt[:1] + [{"ordner": "orte", "datei": "anhalt", "kultur": "obersachsen=100", "kommentar": anhalt}]
        zweite = orte.dateien_zeilen(alt2, self.landschaften, {"anhalt"}, {"anhalt", "glatz"})
        self.assertEqual(zweite, erste)
        self.assertIn("Umfang: Grafschaft Glatz", erste[2][3])

    def test_umbrechen(self) -> None:
        self.assertEqual(orte.umbrechen("a b c", 3), "a b\nc")
        lang = "Anhalt-Bitterfeld-Dessau-Roßlau-Wittenberg"
        self.assertEqual(orte.umbrechen(f"x {lang} y", 10), f"x\n{lang}\ny")  # kein Umbruch im Wort


# ─── Vollständiger Lauf (optional) ──────────────────────────────────────────


@unittest.skipUnless(os.environ.get("ORTE_VOLLTEST") == "1", "nur mit ORTE_VOLLTEST=1 (rund 70 s, Zwischenspeicher)")
class TestVollstaendigerLauf(unittest.TestCase):
    def test_reproduzierbar(self) -> None:
        ergebnis = orte.erzeuge(melde=lambda *_: None)
        self.assertEqual(orte.vergleiche(ergebnis.texte), [])


if __name__ == "__main__":
    unittest.main()
