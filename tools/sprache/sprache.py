#!/usr/bin/env python3
"""Übersetzungen für EINIGKEIT (seit M13b4, docs/technik/vertical_slice.md Abschnitt 6).

Deutsch ist die Quellsprache: Die deutschen Texte sind die Schlüssel (msgid). Das Werkzeug sammelt die
Texte der Oberfläche und schreibt die Vorlage game/locale/einigkeit.pot; de.po und en.po übernehmen jede
neue Vorlage und behalten vorhandene Übersetzungen.

Gesammelt wird:
  - in GDScript (game/scripts): Zeichenketten in tr()/tr_n()/atr()/TranslationServer.translate(),
    Zuweisungen an text, tooltip_text,
    placeholder_text und alle übrigen Zeichenketten, die nach Text aussehen (Leerzeichen oder ein
    großgeschriebenes Wort; keine Kennungen, Pfade, Knotennamen, Schlüssel eines Dictionarys und keine
    Ausgaben von print/push_warning);
  - in Szenen (game/scenes/**/*.tscn): text, tooltip_text, placeholder_text;
  - die Geschichtsstunde (game/tutorial/*.tres) und die Hinweise (game/tutorial/hinweise.cfg).

Aufruf:

    python3 tools/sprache/sprache.py            # Vorlage und .po-Dateien schreiben
    python3 tools/sprache/sprache.py --pruefen  # aktuell? Englisch für Start, Menü und Einstellungen da?
    python3 tools/sprache/sprache.py --fehlend  # Texte ohne englische Übersetzung (Pflichtdateien zuerst)

Nur Standardbibliothek. Exitcodes: 0 = in Ordnung, 1 = --pruefen: nicht aktuell oder Übersetzung
fehlt, 2 = Fehler im Aufruf.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Iterable, Sequence
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[1].parent
GAME = REPO / "game"
LOCALE = GAME / "locale"
POT = LOCALE / "einigkeit.pot"
SPRACHEN = ("de", "en")
#: Dateien, deren Texte auf Englisch vorliegen müssen (Startbildschirm, Spielmenü, Einstellungen).
PFLICHT = (
    "scripts/ui/start_screen.gd",
    "scripts/ui/game_menu.gd",
    "scripts/ui/settings_window.gd",
    "scripts/core/user_settings.gd",
)

_KETTE = re.compile(r'"(?:[^"\\\n]|\\.)*"(?:\s*\+\s*"(?:[^"\\\n]|\\.)*")*')
_TEIL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
_AUSGABE = re.compile(r"\b(print|printerr|print_rich|push_warning|push_error|assert|prints)\s*\(")
_TR = re.compile(r"(\b(?:a?tr|a?tr_n)|TranslationServer\.translate)\s*\(\s*$")
_ZUWEISUNG = re.compile(r"\b(text|tooltip_text|placeholder_text)\s*=\s*$")
#: Davor stehende Aufrufe und Zuweisungen, deren Zeichenkette ein Knotenname ist.
_KNOTEN = re.compile(
    r"(\bname\s*=|find_child\(|get_node\(|has_node\(|(?<![\w])button\(|(?<![\w])menu_button\(|(?<![\w])control\("
    r"|_page_box\()\s*$"
)


@dataclass
class Eintrag:
    msgid: str
    orte: list[str] = field(default_factory=list)


def entschluesseln(roh: str) -> str:
    """Escapes einer GDScript-Zeichenkette auflösen (\\n, \\t, \\", \\\\, \\uXXXX)."""
    aus = []
    i = 0
    while i < len(roh):
        c = roh[i]
        if c == "\\" and i + 1 < len(roh):
            n = roh[i + 1]
            if n == "u" and i + 5 < len(roh) + 0:
                aus.append(chr(int(roh[i + 2 : i + 6], 16)))
                i += 6
                continue
            aus.append({"n": "\n", "t": "\t", '"': '"', "\\": "\\", "'": "'"}.get(n, n))
            i += 2
            continue
        aus.append(c)
        i += 1
    return "".join(aus)


def ist_text(s: str) -> bool:
    """Sieht die Zeichenkette nach Text für Menschen aus?"""
    t = s.strip()
    if not re.search(r"[A-Za-zÄÖÜäöüß]", t):
        return False
    if t.startswith(("res://", "user://", "uid://", "http")) or t.startswith("EINIGKEIT"):
        return False
    if " " not in t:
        if "_" in t or "/" in t or "." in t.rstrip(".…") or ":" in t[:-1]:
            return False  # Kennung, Pfad, Schlüssel
        if "%" in t or "{" in t:
            return False  # Format ohne Wort (z. B. "Tab%d")
        if re.search(r"[a-zäöüß][A-Z]", t) or re.fullmatch(r"[A-Z0-9_]+", t) and len(t) <= 3:
            return False  # Knotenname (CamelCase) oder Kürzel
        if not re.match(r"^[A-ZÄÖÜ„«(]", t):
            return False  # kleingeschriebenes Wort ohne Leerzeichen: Kennung
    # Nur Platzhalter und Satzzeichen?
    ohne = re.sub(r"%[-+0-9.]*[sdfc]|\{[a-z_0-9]*\}|[\s·–—:,.()+\-%/|]", "", t)
    return bool(re.search(r"[A-Za-zÄÖÜäöüß]{2,}", ohne))


def _ohne_kommentare(text: str) -> str:
    """Kommentare (#) außerhalb von Zeichenketten durch Leerzeichen ersetzen (Zeilen bleiben gleich)."""
    aus = []
    in_string = False
    i = 0
    while i < len(text):
        c = text[i]
        if in_string:
            aus.append(c)
            if c == "\\" and i + 1 < len(text):
                aus.append(text[i + 1])
                i += 2
                continue
            if c == '"' or c == "\n":
                in_string = False
        elif c == '"':
            in_string = True
            aus.append(c)
        elif c == "#":
            while i < len(text) and text[i] != "\n":
                aus.append(" ")
                i += 1
            continue
        else:
            aus.append(c)
        i += 1
    return "".join(aus)


def aus_gdscript(pfad: Path, rel: str) -> Iterable[tuple[str, str]]:
    text = _ohne_kommentare(pfad.read_text(encoding="utf-8"))
    zeilen_anfang = [0]
    for m in re.finditer("\n", text):
        zeilen_anfang.append(m.end())
    for m in _KETTE.finditer(text):
        vorher = text[: m.start()]
        zeile = vorher.count("\n") + 1
        zeilentext = text[zeilen_anfang[zeile - 1] : text.find("\n", m.start()) if "\n" in text[m.start() :] else len(text)]
        if _AUSGABE.search(zeilentext):
            continue
        davor = vorher.rstrip()
        if davor.endswith(("&", "^", "%")) and not davor.endswith("% "):
            continue  # StringName, NodePath, eindeutiger Knoten
        if vorher.endswith(("&", "^")):
            continue
        danach = text[m.end() :].lstrip(" \t")
        if danach.startswith(":") and not danach.startswith(":="):
            continue  # Schlüssel eines Dictionarys
        wert = "".join(entschluesseln(t) for t in _TEIL.findall(m.group(0)))
        if _KNOTEN.search(vorher[-40:]):
            continue
        einzelwort = re.fullmatch(r"[A-Za-z]+", wert) is not None
        davor_text = re.search(r'\(\s*"((?:[^"\\\n]|\\.)*)"\s*,\s*$', vorher[-200:])
        if einzelwort and davor_text and ist_text(entschluesseln(davor_text.group(1))):
            continue  # nach einer Beschriftung ein Knotenname wie in _menu_button("Neu", "NewGame")
        markiert = bool(_TR.search(vorher[-40:]) or _ZUWEISUNG.search(vorher[-40:]))
        ohne = re.sub(r"%[-+0-9.]*[sdfc]|\{[a-z_0-9]*\}", "", wert)
        wort = re.search(r"[A-Za-zÄÖÜäöüß]{2,}", ohne) is not None
        if markiert and wort or ist_text(wert):
            yield wert, f"{rel}:{zeile}"


_TSCN = re.compile(r'^(?:text|tooltip_text|placeholder_text) = "((?:[^"\\]|\\.)*)"', re.M)
_TRES = re.compile(r'^(?:title|text|task|done_text|intro) = "((?:[^"\\]|\\.)*)"', re.M)
_CFG = re.compile(r'^(?:titel|text)="((?:[^"\\]|\\.)*)"', re.M)


def aus_muster(pfad: Path, rel: str, muster: re.Pattern[str]) -> Iterable[tuple[str, str]]:
    text = pfad.read_text(encoding="utf-8")
    for m in muster.finditer(text):
        wert = entschluesseln(m.group(1))
        if wert.strip() and re.search(r"[A-Za-zÄÖÜäöüß]", wert):
            yield wert, f"{rel}:{text[: m.start()].count(chr(10)) + 1}"


def sammle(spiel: Path = GAME) -> dict[str, Eintrag]:
    """Alle Texte: msgid → Eintrag mit Fundstellen (Reihenfolge: Dateien alphabetisch, dann Zeilen)."""
    eintraege: dict[str, Eintrag] = {}

    def neu(wert: str, ort: str) -> None:
        eintraege.setdefault(wert, Eintrag(wert)).orte.append(ort)

    for pfad in sorted((spiel / "scripts").rglob("*.gd")):
        for wert, ort in aus_gdscript(pfad, pfad.relative_to(spiel).as_posix()):
            neu(wert, ort)
    for pfad in sorted((spiel / "scenes").rglob("*.tscn")):
        for wert, ort in aus_muster(pfad, pfad.relative_to(spiel).as_posix(), _TSCN):
            neu(wert, ort)
    for pfad in sorted((spiel / "tutorial").glob("*.tres")):
        for wert, ort in aus_muster(pfad, pfad.relative_to(spiel).as_posix(), _TRES):
            neu(wert, ort)
    for pfad in sorted((spiel / "tutorial").glob("*.cfg")):
        for wert, ort in aus_muster(pfad, pfad.relative_to(spiel).as_posix(), _CFG):
            neu(wert, ort)
    return eintraege


# ─── gettext ────────────────────────────────────────────────────────────────


def _po_text(s: str) -> str:
    s = s.replace("\\", "\\\\").replace('"', '\\"').replace("\t", "\\t")
    if "\n" not in s:
        return f'"{s}"'
    teile = s.split("\n")
    zeilen = ['""'] + [f'"{t}\\n"' for t in teile[:-1]] + ([f'"{teile[-1]}"'] if teile[-1] else [])
    return "\n".join(zeilen)


def kopf(sprache: str | None) -> str:
    zeilen = [
        "# Übersetzungen der Oberfläche von EINIGKEIT (tools/sprache/sprache.py, vertical_slice.md 6).",
        "# Deutsch ist die Quellsprache; msgid ist der deutsche Text. Platzhalter {name} bleiben stehen.",
        'msgid ""',
        'msgstr ""',
        '"Project-Id-Version: EINIGKEIT\\n"',
        '"MIME-Version: 1.0\\n"',
        '"Content-Type: text/plain; charset=UTF-8\\n"',
        '"Content-Transfer-Encoding: 8-bit\\n"',
    ]
    if sprache:
        zeilen.append(f'"Language: {sprache}\\n"')
    return "\n".join(zeilen) + "\n"


def schreibe_po(eintraege: dict[str, Eintrag], uebersetzt: dict[str, str], sprache: str | None) -> str:
    teile = [kopf(sprache)]
    for e in eintraege.values():
        orte = " ".join(e.orte[:6]) + (f" (+{len(e.orte) - 6})" if len(e.orte) > 6 else "")
        teile.append(f"#: {orte}\nmsgid {_po_text(e.msgid)}\nmsgstr {_po_text(uebersetzt.get(e.msgid, ''))}\n")
    return "\n".join(teile)


def lies_po(pfad: Path) -> dict[str, str]:
    """msgid → msgstr (nur nicht leere Übersetzungen)."""
    if not pfad.is_file():
        return {}
    ergebnis: dict[str, str] = {}
    msgid: list[str] | None = None
    msgstr: list[str] | None = None
    aktuell: list[str] | None = None

    def fertig() -> None:
        if msgid is not None and msgstr is not None:
            k = "".join(msgid)
            v = "".join(msgstr)
            if k and v:
                ergebnis[k] = v

    for zeile in pfad.read_text(encoding="utf-8").splitlines():
        zeile = zeile.strip()
        if zeile.startswith("msgid "):
            fertig()
            msgid, msgstr = [], None
            aktuell = msgid
            zeile = zeile[6:]
        elif zeile.startswith("msgstr "):
            msgstr = []
            aktuell = msgstr
            zeile = zeile[7:]
        elif not zeile.startswith('"'):
            continue
        if aktuell is not None and zeile.startswith('"'):
            aktuell.append(entschluesseln(zeile[1:-1]))
    fertig()
    return ergebnis


def pflicht(eintraege: dict[str, Eintrag]) -> list[str]:
    return [e.msgid for e in eintraege.values() if any(o.split(":")[0] in PFLICHT for o in e.orte)]


def _argumente() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="sprache.py", description="Übersetzungen der Oberfläche pflegen.", add_help=False)
    p.add_argument("-h", "--hilfe", action="help", help="diese Hilfe anzeigen und beenden")
    p.add_argument("--spiel", type=Path, default=GAME, metavar="DIR", help="Godot-Projekt (Standard: game)")
    p.add_argument("--pruefen", action="store_true", help="nur prüfen, nichts schreiben")
    p.add_argument("--fehlend", action="store_true", help="Texte ohne englische Übersetzung ausgeben")
    return p


def main(argv: Sequence[str] | None = None) -> int:
    args = _argumente().parse_args(argv)
    if not (args.spiel / "project.godot").is_file():
        print(f"Fehler: kein Godot-Projekt in {args.spiel}", file=sys.stderr)
        return 2
    eintraege = sammle(args.spiel)
    ordner = args.spiel / "locale"
    soll = {"einigkeit.pot": schreibe_po(eintraege, {}, None)}
    uebersetzungen = {s: lies_po(ordner / f"{s}.po") for s in SPRACHEN}
    for s in SPRACHEN:
        soll[f"{s}.po"] = schreibe_po(eintraege, uebersetzungen[s], s)
    fehlend = [m for m in pflicht(eintraege) if m not in uebersetzungen["en"]]
    alle_fehlend = [e.msgid for e in eintraege.values() if e.msgid not in uebersetzungen["en"]]
    if args.fehlend:
        for m in fehlend:
            print("PFLICHT", repr(m), flush=True)
        print(f"{len(fehlend)} Pflichttexte und {len(alle_fehlend)} Texte insgesamt ohne Englisch")
        return 0
    veraltet = [n for n, t in soll.items() if not (ordner / n).is_file() or (ordner / n).read_text(encoding="utf-8") != t]
    erledigt = len(eintraege) - len(alle_fehlend)
    print(
        f"{len(eintraege)} Texte, Englisch {erledigt} ({100 * erledigt // max(len(eintraege), 1)} %), "
        f"Pflicht (Start, Menü, Einstellungen) {len(pflicht(eintraege)) - len(fehlend)} von {len(pflicht(eintraege))}"
    )
    if args.pruefen:
        for n in veraltet:
            print(f"FEHLER: game/locale/{n} ist nicht aktuell (python3 tools/sprache/sprache.py)", file=sys.stderr)
        for m in fehlend:
            print(f"FEHLER: ohne englische Übersetzung: {m!r}", file=sys.stderr)
        return 1 if veraltet or fehlend else 0
    ordner.mkdir(parents=True, exist_ok=True)
    for n, t in soll.items():
        (ordner / n).write_text(t, encoding="utf-8")
    print(f"geschrieben: {', '.join(sorted(soll))}" + (f"; {len(fehlend)} Pflichttexte ohne Englisch" if fehlend else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
