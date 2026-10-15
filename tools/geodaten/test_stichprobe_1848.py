"""Tests für stichprobe_1848.py (python3 -m unittest discover -s tools/geodaten)."""

from __future__ import annotations

import re
import unittest

import stichprobe_1848 as sp
import zuordnung as zu


class TestStichprobe(unittest.TestCase):
    def test_ziehung_ist_reproduzierbar(self) -> None:
        probe = sp.ziehe(zu.TABELLEN, sp.SEED)
        self.assertEqual(
            {k: len(v) for k, v in probe.items()}, {"besitz": 20, "bevoelkerung": 20, "verwaltung": 10, "staaten": 10}
        )
        self.assertEqual(probe, sp.ziehe(zu.TABELLEN, sp.SEED))
        self.assertNotEqual(probe, sp.ziehe(zu.TABELLEN, sp.SEED + 1))
        self.assertTrue(all(z["ab"] == sp.STICHTAG_BESITZ for z in probe["besitz"]))
        self.assertTrue(all(z["sicherheit"] != "spielwert" for z in probe["staaten"]))

    def test_markdown(self) -> None:
        text = sp.markdown(zu.TABELLEN, sp.SEED)
        self.assertEqual(len(re.findall(r"^\| [BEVS]\d+ \| \d+ \|", text, re.MULTILINE)), 60)
        self.assertEqual(sp.zahl(1234567.4), "1.234.567")


if __name__ == "__main__":
    unittest.main()
