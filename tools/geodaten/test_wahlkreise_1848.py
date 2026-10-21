"""Tests für wahlkreise_1848.py (python3 -m unittest discover -s tools/geodaten)."""

from __future__ import annotations

import unittest

import wahlkreise_1848 as wk
import zuordnung as zu


def ort(kennung: str, einwohner: int, laenge: float, breite: float, baustein: str = "k:a", region: str = "r") -> wk.Ort:
    return wk.Ort(kennung, kennung.title(), "AAA", True, einwohner, laenge, breite, baustein, region)


class TestRechnung(unittest.TestCase):
    def test_hare_niemeyer_mit_mindestsitz(self) -> None:
        self.assertEqual(wk.hare_niemeyer({"a": 900, "b": 90, "c": 10}, 10), {"a": 8, "b": 1, "c": 1})
        self.assertEqual(wk.hare_niemeyer({"a": 1, "b": 1}, 3), {"a": 2, "b": 1})
        self.assertEqual(sum(wk.hare_niemeyer({"a": 5, "b": 3, "c": 2, "d": 0}, 7).values()), 7)

    def test_hilbert_ordnung_eins(self) -> None:
        self.assertEqual([wk.hilbert_index(x, y, 1) for x, y in ((0, 0), (0, 1), (1, 1), (1, 0))], [0, 1, 2, 3])

    def test_schneiden_nach_einwohnern(self) -> None:
        gruppen = [[ort(f"o{i}", 100, i, 0)] for i in range(6)]
        teile = wk.schneide(gruppen, 3)
        self.assertEqual([len(t) for t in teile], [2, 2, 2])
        self.assertEqual(len(wk.schneide(gruppen, 10)), 6)

    def test_schneiden_optimal_und_ohne_grosse_luecken(self) -> None:
        gewichte = [30, 70, 40, 60, 100]
        gruppen = [[ort(f"o{i}", w, 0.01 * i, 0)] for i, w in enumerate(gewichte)]
        self.assertEqual([sum(o.einwohner for o in t) for t in wk.schneide(gruppen, 3)], [100, 100, 100])
        # Drei Orte dicht beieinander, einer gut 300 km entfernt: kein Wahlkreis überspannt die Lücke,
        # auch wenn 2 + 2 gleicher wäre.
        nah_fern = [[ort("a", 100, 0, 50)], [ort("b", 100, 0.01, 50)], [ort("c", 100, 0.02, 50)], [ort("d", 100, 4.5, 50)]]
        self.assertEqual([[o.kennung for o in t] for t in wk.schneide(nah_fern, 2)], [["a", "b", "c"], ["d"]])

    def test_grosse_stadt_mit_mehreren_sitzen(self) -> None:
        orte = [ort("stadt", 600, 0, 0), *[ort(f"dorf{i}", 100, 0.1 * i, 0.1, f"k:{i // 2}") for i in range(4)]]
        kreise = wk.wahlkreise_eines_staates("AAA", orte, 10)
        self.assertEqual(sum(k.sitze for k in kreise), 10)
        stadt = next(k for k in kreise if k.orte[0].kennung == "stadt")
        self.assertEqual(stadt.sitze, 6)
        self.assertEqual(sorted(o.kennung for k in kreise for o in k.orte), sorted(o.kennung for o in orte))

    def test_regionen_ohne_sitz_gehen_in_der_naechsten_auf(self) -> None:
        orte = [ort("a", 1000, 0, 0, region="west"), ort("b", 1000, 1, 0, region="ost"), ort("c", 10, 0.1, 0, region="insel")]
        teile = wk.regionen(orte, 2)
        self.assertEqual(len(teile), 2)
        west = next(g for g, _ in teile if any(o.kennung == "a" for o in g))
        self.assertIn("c", [o.kennung for o in west])


class TestDaten(unittest.TestCase):
    def test_tabelle_ist_aktuell_und_vollstaendig(self) -> None:
        self.assertEqual(wk.main(["--pruefen"]), 0)
        zeilen = zu.lies_csv(zu.TABELLEN / "wahlkreise.csv", wk.SPALTEN)
        parlamente = {p.kennung: p for p in wk.lies_parlamente()}
        orte = wk.lies_orte(zu.TABELLEN)
        for kennung, parlament in parlamente.items():
            eigene = [z for z in zeilen if z["parlament"] == kennung]
            self.assertEqual(sum(int(z["sitze"]) for z in eigene), parlament.sitze, kennung)
            vergeben = [o for z in eigene for o in z["orte"].split()]
            self.assertEqual(len(vergeben), len(set(vergeben)), kennung)
            self.assertEqual(set(vergeben), {o.kennung for o in wk.wahlgebiet(parlament, orte)}, kennung)


if __name__ == "__main__":
    unittest.main()
