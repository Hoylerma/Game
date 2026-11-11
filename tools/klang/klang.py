#!/usr/bin/env python3
"""Musik und Bedienklänge für EINIGKEIT (seit M13b2, docs/technik/vertical_slice.md 5.3).

Rendert die Stücke aus game/audio/noten/*.noten mit dem eigenen Synthesizer (synth.py) und die
Bedienklänge (klaenge.py) als WAV-Dateien nach game/audio/musik/ und game/audio/klaenge/, dazu
game/audio/musik/stuecke.cfg (Titel, Komponist, Stimmung, Dauer je Stück) für das Spiel. Die WAV-Dateien
sind nicht versioniert; ihre Prüfsummen stehen in game/audio/pruefsummen.txt.

Aufruf (siehe README.md im selben Ordner):

    python3 tools/klang/klang.py                 # alles rendern (Unverändertes wird übersprungen)
    python3 tools/klang/klang.py --pruefen       # im Speicher rendern, Prüfsummen vergleichen
    python3 tools/klang/klang.py --pruefsummen   # Prüfsummen neu schreiben (nach einer Änderung)
    python3 tools/klang/klang.py --nur ode_an_die_freude glocke

Abhängigkeiten: Python ab 3.10 und numpy. Exitcodes: 0 = Erfolg, 1 = --pruefen: Prüfsumme weicht ab,
2 = Fehler in Noten oder Aufruf.
"""

from __future__ import annotations

import argparse
import hashlib
import sys
import time
import wave
from collections.abc import Sequence
from pathlib import Path

try:
    import numpy as np
except ImportError:  # pragma: no cover - nur ohne numpy
    print("Fehler: klang.py braucht numpy (python3 -m pip install numpy).", file=sys.stderr)
    raise SystemExit(2) from None

_HIER = Path(__file__).resolve().parent
if str(_HIER) not in sys.path:
    sys.path.insert(0, str(_HIER))

import klaenge  # noqa: E402
import noten  # noqa: E402
import synth  # noqa: E402

REPO = _HIER.parents[1]
NOTEN = REPO / "game" / "audio" / "noten"
AUSGABE = REPO / "game" / "audio"
PRUEFSUMMEN = AUSGABE / "pruefsummen.txt"


def pruefsumme(proben: np.ndarray) -> str:
    """SHA-256 der Proben als 16-Bit little endian."""
    return hashlib.sha256(proben.astype("<i2").tobytes()).hexdigest()


def schreibe_wav(pfad: Path, proben: np.ndarray) -> None:
    pfad.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(pfad), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(synth.RATE)
        w.writeframes(proben.astype("<i2").tobytes())


def lies_wav(pfad: Path) -> np.ndarray | None:
    try:
        with wave.open(str(pfad), "rb") as w:
            if w.getnchannels() != 1 or w.getsampwidth() != 2 or w.getframerate() != synth.RATE:
                return None
            return np.frombuffer(w.readframes(w.getnframes()), dtype="<i2")
    except (OSError, wave.Error, EOFError):
        return None


def lies_pruefsummen(pfad: Path) -> dict[str, str]:
    werte: dict[str, str] = {}
    if pfad.is_file():
        for zeile in pfad.read_text(encoding="utf-8").splitlines():
            zeile = zeile.split("#", 1)[0].strip()
            if zeile:
                name, wert = zeile.split()
                werte[name] = wert
    return werte


def schreibe_pruefsummen(pfad: Path, werte: dict[str, str]) -> None:
    zeilen = [
        "# Prüfsummen (SHA-256 der 16-Bit-Proben) der gerenderten Musik und Klänge, tools/klang/klang.py.",
        "# Neu schreiben mit: python3 tools/klang/klang.py --pruefsummen",
    ]
    zeilen += [f"{name} {werte[name]}" for name in sorted(werte)]
    pfad.write_text("\n".join(zeilen) + "\n", encoding="utf-8")


def stuecke(ordner: Path) -> list[noten.Stueck]:
    return [noten.lies(p) for p in sorted(ordner.glob("*.noten"))]


def schreibe_liste(pfad: Path, alle: list[noten.Stueck]) -> None:
    """stuecke.cfg im Format von Godots ConfigFile: ein Abschnitt je Stück."""
    zeilen = ["; Erzeugt von tools/klang/klang.py – nicht von Hand ändern."]
    for s in alle:
        zeilen += [
            "",
            f"[{s.name}]",
            f'titel="{s.titel}"',
            f'komponist="{s.komponist}"',
            f'werk="{s.werk}"',
            f'stimmung="{s.stimmung}"',
            f"dauer={s.dauer_sekunden():.1f}",
        ]
    pfad.parent.mkdir(parents=True, exist_ok=True)
    pfad.write_text("\n".join(zeilen) + "\n", encoding="utf-8")


def _argumente() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="klang.py", description="Musik und Bedienklänge für EINIGKEIT rendern.", add_help=False
    )
    p.add_argument("-h", "--hilfe", action="help", help="diese Hilfe anzeigen und beenden")
    p.add_argument("--noten", type=Path, default=NOTEN, metavar="DIR", help="Ordner der .noten-Dateien")
    p.add_argument("--ausgabe", type=Path, default=AUSGABE, metavar="DIR", help="Ordner für musik/ und klaenge/")
    p.add_argument("--pruefen", action="store_true", help="nur rendern und die Prüfsummen vergleichen")
    p.add_argument("--pruefsummen", action="store_true", help="Prüfsummen neu schreiben")
    p.add_argument("--nur", nargs="+", metavar="NAME", help="nur diese Stücke oder Klänge")
    return p


def main(argv: Sequence[str] | None = None) -> int:
    args = _argumente().parse_args(argv)
    try:
        alle = stuecke(args.noten)
    except noten.NotenFehler as fehler:
        print(f"Fehler: {fehler}", file=sys.stderr)
        return 2
    if not alle:
        print(f"Fehler: keine .noten-Dateien in {args.noten}", file=sys.stderr)
        return 2
    auftraege: list[tuple[str, str, object]] = [("musik", s.name, s) for s in alle]
    auftraege += [("klaenge", name, fn) for name, fn in klaenge.KLAENGE.items()]
    if args.nur:
        unbekannt = set(args.nur) - {name for _, name, _ in auftraege}
        if unbekannt:
            print(f"Fehler: unbekannt: {', '.join(sorted(unbekannt))}", file=sys.stderr)
            return 2
        auftraege = [a for a in auftraege if a[1] in args.nur]
    pfad_summen = args.ausgabe / PRUEFSUMMEN.name
    erwartet = lies_pruefsummen(pfad_summen)
    neu = dict(erwartet)
    abweichend: list[str] = []
    beginn = time.monotonic()
    for art, name, quelle in auftraege:
        ziel = args.ausgabe / art / f"{name}.wav"
        t0 = time.monotonic()
        if not args.pruefen and not args.pruefsummen and name in erwartet:
            vorhanden = lies_wav(ziel)
            if vorhanden is not None and pruefsumme(vorhanden) == erwartet[name]:
                print(f"  {art}/{name}: aktuell")
                continue
        proben = synth.spiele(quelle) if art == "musik" else quelle()  # type: ignore[operator]
        summe = pruefsumme(proben)
        neu[name] = summe
        zustand = "neu" if name not in erwartet else ("gleich" if erwartet[name] == summe else "ABWEICHEND")
        if zustand != "gleich" and name in erwartet:
            abweichend.append(name)
        if not args.pruefen:
            schreibe_wav(ziel, proben)
        print(
            f"  {art}/{name}: {len(proben) / synth.RATE:5.1f} s, {len(proben) * 2 / 1e6:4.1f} MB, "
            f"{summe[:16]} {zustand} ({time.monotonic() - t0:.1f} s)"
        )
    if not args.pruefen:
        schreibe_liste(args.ausgabe / "musik" / "stuecke.cfg", alle)
    if args.pruefsummen:
        schreibe_pruefsummen(pfad_summen, neu)
        print(f"Prüfsummen geschrieben: {pfad_summen}")
        return 0
    fehlend = [name for _, name, _ in auftraege if name not in erwartet]
    print(f"Fertig in {time.monotonic() - beginn:.1f} s.")
    if args.pruefen and (abweichend or fehlend):
        for name in abweichend:
            print(f"FEHLER: {name}: Prüfsumme weicht ab", file=sys.stderr)
        for name in fehlend:
            print(f"FEHLER: {name}: keine Prüfsumme in {pfad_summen}", file=sys.stderr)
        return 1
    if abweichend:
        print(f"Hinweis: {len(abweichend)} Prüfsummen weichen ab (--pruefsummen schreibt sie neu).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
