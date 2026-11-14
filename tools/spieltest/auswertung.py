#!/usr/bin/env python3
"""Auswertung des Spieltests M13 (seit M13b6, docs/technik/vertical_slice.md 10).

Liest die ausgefüllten Fragebögen (`<Kennung>_fragebogen.txt`, Vorlage
docs/pruefung/spieltest_m13_fragebogen.txt) und die Rückmeldungen aus dem Spiel (ZIP mit
messwerte.json, notiz.txt, godot.log; Dateiname `<Kennung>_….zip` oder im Ordner `<Kennung>/`) und
schreibt daraus die Tabellen der Auswertung: Teilnehmende, Technik und Bildrate, Skalenfragen,
Freitexte, Notizen aus dem Spiel.

    python3 tools/spieltest/auswertung.py --eingang spieltest/            # Markdown auf stdout
    python3 tools/spieltest/auswertung.py --eingang spieltest/ \\
        --bericht docs/pruefung/spieltest_m13_ergebnisse.md                # Abschnitte im Bericht ersetzen

Im Bericht ersetzt das Werkzeug nur, was zwischen `<!-- auswertung:NAME -->` und
`<!-- /auswertung:NAME -->` steht (NAME: teilnehmende, technik, skalen, texte, notizen); Befunde und
Antworten schreibt die Spielleitung von Hand. Nur Standardbibliothek. Exitcodes: 0 = Erfolg,
1 = ungültige Antworten (mit --streng), 2 = Fehler.
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
import sys
import zipfile
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path

HIER = Path(__file__).resolve().parent
REPO = HIER.parents[1]
VORLAGE = REPO / "docs" / "pruefung" / "spieltest_m13_fragebogen.txt"
ABSCHNITTE = ("teilnehmende", "technik", "skalen", "texte", "notizen")
# Bildrate: Ziel 60 Bilder/s (vertical_slice.md 10); mit Vsync schwankt der Median knapp darunter.
ZIEL_BILDRATE = 60.0
TOLERANZ_BILDRATE = 2.0
# Skalenfragen mit einem Median bis hierhin sind ein Befund.
SCHWELLE_BEFUND = 2.5
FORMAT_MESSWERTE = 1

_FRAGE = re.compile(r"^(?P<id>[A-Za-z]+\d*)\s+\[(?P<typ>[^\]]+)\]\s+(?P<text>.*?):\s*(?P<antwort>.*)$")


class AuswertungsFehler(Exception):
    pass


@dataclass
class Frage:
    id: str
    typ: str  # "skala", "ja/nein", "zahl", "text"
    text: str
    von: int = 0
    bis: int = 0


@dataclass
class Teilnehmer:
    kennung: str
    antworten: dict[str, str] = field(default_factory=dict)
    rueckmeldungen: list[dict] = field(default_factory=list)  # je { messwerte, notiz, fehler_im_protokoll, datei }


# ─── Lesen ───────────────────────────────────────────────────────────────────


def lies_fragen(pfad: Path) -> list[Frage]:
    fragen = []
    for zeile in pfad.read_text(encoding="utf-8").splitlines():
        treffer = _FRAGE.match(zeile)
        if not treffer:
            continue
        typ = treffer["typ"].strip().lower()
        bereich = re.fullmatch(r"(\d+)-(\d+)", typ)
        if bereich:
            fragen.append(Frage(treffer["id"], "skala", treffer["text"], int(bereich[1]), int(bereich[2])))
        elif typ in ("ja/nein", "zahl", "text"):
            fragen.append(Frage(treffer["id"], typ, treffer["text"]))
        else:
            raise AuswertungsFehler(f"{pfad.name}: unbekannter Antworttyp [{typ}] bei {treffer['id']}")
    if not fragen:
        raise AuswertungsFehler(f"{pfad}: keine Fragen gefunden")
    return fragen


def lies_antworten(pfad: Path) -> dict[str, str]:
    """Antworten eines ausgefüllten Fragebogens; eingerückte Folgezeilen gehören zur letzten Antwort."""
    antworten: dict[str, str] = {}
    letzte = ""
    for zeile in pfad.read_text(encoding="utf-8-sig").splitlines():
        if zeile.lstrip().startswith("#"):
            continue
        treffer = _FRAGE.match(zeile)
        if treffer:
            letzte = treffer["id"]
            antworten[letzte] = treffer["antwort"].strip()
        elif letzte and zeile[:1] in (" ", "\t") and zeile.strip():
            antworten[letzte] = (antworten[letzte] + " " + zeile.strip()).strip()
        elif not zeile.strip():
            letzte = ""
    return antworten


def lies_rueckmeldung(pfad: Path) -> dict:
    try:
        with zipfile.ZipFile(pfad) as z:
            namen = set(z.namelist())
            messwerte = json.loads(z.read("messwerte.json").decode("utf-8")) if "messwerte.json" in namen else {}
            notiz = z.read("notiz.txt").decode("utf-8").strip() if "notiz.txt" in namen else ""
            protokoll = z.read("godot.log").decode("utf-8", errors="replace") if "godot.log" in namen else ""
    except (zipfile.BadZipFile, json.JSONDecodeError, KeyError) as fehler:
        raise AuswertungsFehler(f"{pfad.name}: keine gültige Rückmeldung ({fehler})") from None
    if messwerte and int(messwerte.get("format", 0)) != FORMAT_MESSWERTE:
        raise AuswertungsFehler(f"{pfad.name}: messwerte.json im Format {messwerte.get('format')} (erwartet 1)")
    fehler = sum(1 for z in protokoll.splitlines() if z.startswith(("ERROR:", "SCRIPT ERROR:")))
    return {"messwerte": messwerte, "notiz": notiz, "fehler_im_protokoll": fehler, "datei": pfad.name}


def _kennung_aus(pfad: Path, eingang: Path) -> str:
    relativ = pfad.relative_to(eingang)
    if len(relativ.parts) > 1:
        return relativ.parts[0]
    return pfad.stem.split("_", 1)[0]


def lies_eingang(eingang: Path) -> dict[str, Teilnehmer]:
    if not eingang.is_dir():
        raise AuswertungsFehler(f"{eingang}: kein Ordner")
    teilnehmer: dict[str, Teilnehmer] = {}
    for pfad in sorted(eingang.rglob("*_fragebogen.txt")):
        antworten = lies_antworten(pfad)
        kennung = antworten.get("kennung") or _kennung_aus(pfad, eingang)
        teilnehmer.setdefault(kennung, Teilnehmer(kennung)).antworten.update(antworten)
    for pfad in sorted(eingang.rglob("*.zip")):
        kennung = _kennung_aus(pfad, eingang)
        if kennung == "rueckmeldung":
            raise AuswertungsFehler(
                f"{pfad.name}: ohne Kennung – in den Ordner der Kennung legen (T03/{pfad.name}) "
                f"oder umbenennen (T03_{pfad.name})"
            )
        teilnehmer.setdefault(kennung, Teilnehmer(kennung)).rueckmeldungen.append(lies_rueckmeldung(pfad))
    for t in teilnehmer.values():
        t.rueckmeldungen.sort(key=lambda r: str(r["messwerte"].get("zeit", "")))
    return dict(sorted(teilnehmer.items()))


def pruefe(fragen: list[Frage], teilnehmer: dict[str, Teilnehmer]) -> list[str]:
    """Ungültige Antworten (außerhalb der Skala, keine Zahl, weder ja noch nein)."""
    probleme = []
    for t in teilnehmer.values():
        for f in fragen:
            wert = t.antworten.get(f.id, "")
            if not wert:
                continue
            if f.typ == "skala" and _zahl(wert) is None:
                probleme.append(f"{t.kennung} {f.id}: „{wert}“ ist keine Zahl von {f.von} bis {f.bis}")
            elif f.typ == "skala" and not f.von <= _zahl(wert) <= f.bis:  # type: ignore[operator]
                probleme.append(f"{t.kennung} {f.id}: {wert} liegt nicht zwischen {f.von} und {f.bis}")
            elif f.typ == "ja/nein" and _ja_nein(wert) is None:
                probleme.append(f"{t.kennung} {f.id}: „{wert}“ ist weder ja noch nein")
            elif f.typ == "zahl" and _zahl(wert) is None:
                probleme.append(f"{t.kennung} {f.id}: „{wert}“ ist keine Zahl")
    return probleme


def _zahl(wert: str) -> float | None:
    try:
        return float(wert.replace(",", ".").strip())
    except ValueError:
        return None


def _ja_nein(wert: str) -> bool | None:
    w = wert.strip().lower()
    if w in ("ja", "j", "yes", "y"):
        return True
    if w in ("nein", "n", "no"):
        return False
    return None


# ─── Abschnitte ──────────────────────────────────────────────────────────────


def _zelle(text: object) -> str:
    return str(text).replace("|", "\\|").replace("\n", " ") if text not in (None, "") else "–"


def _komma(wert: float, stellen: int = 1) -> str:
    return f"{wert:.{stellen}f}".replace(".", ",")


def _dauer(sekunden: int) -> str:
    return f"{sekunden} s" if sekunden < 60 else f"{sekunden // 60} min"


def _neueste(t: Teilnehmer) -> dict:
    return t.rueckmeldungen[-1]["messwerte"] if t.rueckmeldungen else {}


def _laengste_messung(t: Teilnehmer) -> dict:
    return max((r["messwerte"] for r in t.rueckmeldungen), key=lambda m: int(m.get("bildrate_sekunden", 0)), default={})


def abschnitt_teilnehmende(fragen: list[Frage], teilnehmer: dict[str, Teilnehmer]) -> str:
    zeilen = [
        (
            f"{len(teilnehmer)} Teilnehmende, {sum(len(t.rueckmeldungen) for t in teilnehmer.values())} "
            "Rückmeldungen aus dem Spiel."
        ),
        "",
        (
            "| Kennung | Strategie (0–3) | Paradox (0–3) | Geschichte (0–3) | Geschichtsstunde | System | Version "
            "| Sitzung | Lesezeichen, Staat | erreicht | Ende |"
        ),
        "|---|---|---|---|---|---|---|---|---|---|---|",
    ]
    for t in teilnehmer.values():
        m = _neueste(t)
        sitzung = max((int(r["messwerte"].get("sitzung_sekunden", 0)) for r in t.rueckmeldungen), default=0)
        stunde = _ja_nein(t.antworten.get("B1", ""))
        zeilen.append(
            "| "
            + " | ".join(
                _zelle(x)
                for x in (
                    t.kennung,
                    t.antworten.get("A1"),
                    t.antworten.get("A2"),
                    t.antworten.get("A3"),
                    {True: "ja", False: "nein", None: ""}[stunde],
                    m.get("system") or t.antworten.get("A4"),
                    m.get("version"),
                    _dauer(sitzung) if sitzung else "",
                    ", ".join(x for x in (str(m.get("szenario", "")), str(m.get("spieler", ""))) if x),
                    m.get("spieldatum"),
                    {True: "ja", False: "nein"}.get(m.get("partie_beendet")) if m else "",
                )
            )
            + " |"
        )
    return "\n".join(zeilen)


def abschnitt_technik(fragen: list[Frage], teilnehmer: dict[str, Teilnehmer]) -> str:
    zeilen = [
        (
            f"Bildrate je Teilnehmer aus der längsten Messung (Ziel {ZIEL_BILDRATE:.0f} Bilder/s; erreicht, wenn "
            f"der Median höchstens {TOLERANZ_BILDRATE:.0f} darunter liegt). Fehler im Protokoll: Zeilen „ERROR“ "
            "und „SCRIPT ERROR“ in godot.log, über alle Rückmeldungen."
        ),
        "",
        (
            "| Kennung | Grafikkarte | Fenster | Vollbild | Limit | Median | 5 % | gemessen | Ziel | Absturz (F2) "
            "| Fehler im Protokoll |"
        ),
        "|---|---|---|---|---|---|---|---|---|---|---|",
    ]
    erreicht = gemessen = 0
    for t in teilnehmer.values():
        m = _laengste_messung(t)
        sekunden = int(m.get("bildrate_sekunden", 0))
        median = float(m.get("bildrate_median", 0))
        ziel = ""
        if sekunden > 0:
            gemessen += 1
            ok = median >= ZIEL_BILDRATE - TOLERANZ_BILDRATE
            erreicht += ok
            ziel = "ja" if ok else "**nein**"
        absturz = _ja_nein(t.antworten.get("F2", ""))
        zeilen.append(
            "| "
            + " | ".join(
                _zelle(x)
                for x in (
                    t.kennung,
                    m.get("grafikkarte"),
                    m.get("fenster"),
                    {True: "ja", False: "nein"}.get(m.get("vollbild"), ""),
                    m.get("bildratenlimit") or ("aus" if m else ""),
                    _komma(median) if sekunden else "",
                    _komma(float(m.get("bildrate_p5", 0))) if sekunden else "",
                    _dauer(sekunden) if sekunden else "",
                    ziel,
                    {True: "**ja**", False: "nein", None: ""}[absturz],
                    sum(r["fehler_im_protokoll"] for r in t.rueckmeldungen) if t.rueckmeldungen else "",
                )
            )
            + " |"
        )
    zeilen += ["", f"Bildrate erreicht bei {erreicht} von {gemessen} gemessenen Rechnern."]
    return "\n".join(zeilen)


def abschnitt_skalen(fragen: list[Frage], teilnehmer: dict[str, Teilnehmer]) -> str:
    zeilen = [
        f"Median, Mittel und Verteilung je Frage; **Befund** bei einem Median bis {_komma(SCHWELLE_BEFUND)}.",
        "",
        "| Frage | | n | Median | Mittel | Verteilung | |",
        "|---|---|---|---|---|---|---|",
    ]
    for f in fragen:
        if f.typ != "skala" or f.id.startswith("A"):
            continue
        werte = [
            w
            for t in teilnehmer.values()
            if (w := _zahl(t.antworten.get(f.id, ""))) is not None and f.von <= w <= f.bis
        ]
        if f.bis - f.von <= 5:
            verteilung = " ".join(f"{k}·{sum(1 for w in werte if w == k)}" for k in range(f.von, f.bis + 1))
        else:  # 0–10: Weiterempfehlung
            foerderer = sum(1 for w in werte if w >= 9)
            kritiker = sum(1 for w in werte if w <= 6)
            verteilung = (
                f"Förderer {foerderer}, Kritiker {kritiker}, Saldo {round(100 * (foerderer - kritiker) / len(werte))}"
                if werte
                else ""
            )
        median = statistics.median(werte) if werte else None
        befund = "**Befund**" if median is not None and f.bis == 5 and median <= SCHWELLE_BEFUND else ""
        zeilen.append(
            f"| {f.id} | {_zelle(f.text)} | {len(werte)} | {_komma(median) if median is not None else '–'} | "
            f"{_komma(statistics.fmean(werte)) if werte else '–'} | {verteilung or '–'} | {befund} |"
        )
    return "\n".join(zeilen)


def abschnitt_texte(fragen: list[Frage], teilnehmer: dict[str, Teilnehmer]) -> str:
    zeilen: list[str] = []
    for f in fragen:
        if f.typ != "text" or f.id in ("kennung", "datum", "A4"):
            continue
        antworten = [(t.kennung, t.antworten.get(f.id, "")) for t in teilnehmer.values()]
        antworten = [(k, a) for k, a in antworten if a and a != "-"]
        zeilen += [f"**{f.id} – {f.text}**", ""]
        zeilen += [f"- {k}: {a}" for k, a in antworten] or ["- (keine Antworten)"]
        zeilen.append("")
    return "\n".join(zeilen).rstrip()


def abschnitt_notizen(fragen: list[Frage], teilnehmer: dict[str, Teilnehmer]) -> str:
    zeilen = []
    for t in teilnehmer.values():
        for r in t.rueckmeldungen:
            if not r["notiz"]:
                continue
            m = r["messwerte"]
            wo = ", ".join(str(x) for x in (m.get("szenario"), m.get("spieler"), m.get("spieldatum")) if x)
            zeilen.append(f"- {t.kennung} ({wo or r['datei']}): {r['notiz'].replace(chr(10), ' ')}")
    return "\n".join(zeilen) if zeilen else "(keine Notizen)"


ERZEUGER = {
    "teilnehmende": abschnitt_teilnehmende,
    "technik": abschnitt_technik,
    "skalen": abschnitt_skalen,
    "texte": abschnitt_texte,
    "notizen": abschnitt_notizen,
}


def setze_ein(bericht: str, abschnitte: dict[str, str]) -> str:
    """Ersetzt den Inhalt zwischen den Markierungen; fehlt eine Markierung, ist das ein Fehler."""
    for name, inhalt in abschnitte.items():
        muster = re.compile(
            rf"(<!-- auswertung:{name} -->\n).*?(\n<!-- /auswertung:{name} -->)", re.DOTALL
        )
        if not muster.search(bericht):
            raise AuswertungsFehler(f"Markierung <!-- auswertung:{name} --> fehlt im Bericht")
        bericht = muster.sub(lambda t, i=inhalt: t[1] + i + t[2], bericht, count=1)
    return bericht


# ─── Aufruf ──────────────────────────────────────────────────────────────────


def _argumente() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="auswertung.py", description="Spieltest M13 auswerten.")
    p.add_argument("--eingang", type=Path, required=True, help="Ordner mit Fragebögen und Rückmeldungen")
    p.add_argument("--vorlage", type=Path, default=VORLAGE, help="leerer Fragebogen (Fragen und Skalen)")
    p.add_argument("--bericht", type=Path, help="Bericht, in dem die Abschnitte ersetzt werden")
    p.add_argument("--streng", action="store_true", help="Exitcode 1 bei ungültigen Antworten")
    return p


def main(argv: Sequence[str] | None = None) -> int:
    args = _argumente().parse_args(argv)
    try:
        fragen = lies_fragen(args.vorlage)
        teilnehmer = lies_eingang(args.eingang)
        probleme = pruefe(fragen, teilnehmer)
        abschnitte = {name: ERZEUGER[name](fragen, teilnehmer) for name in ABSCHNITTE}
        if args.bericht:
            text = args.bericht.read_text(encoding="utf-8")
            args.bericht.write_text(setze_ein(text, abschnitte), encoding="utf-8")
            print(f"{args.bericht}: {len(abschnitte)} Abschnitte ersetzt ({len(teilnehmer)} Teilnehmende)")
        else:
            for name in ABSCHNITTE:
                print(f"## {name}\n\n{abschnitte[name]}\n")
    except (AuswertungsFehler, OSError) as fehler:
        print(f"Fehler: {fehler}", file=sys.stderr)
        return 2
    for problem in probleme:
        print(f"Warnung: {problem}", file=sys.stderr)
    return 1 if probleme and args.streng else 0


if __name__ == "__main__":
    sys.exit(main())
