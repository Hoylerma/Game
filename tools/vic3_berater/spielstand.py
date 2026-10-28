"""Spielstände von Victoria 3 finden, beobachten und eine kurze Zusammenfassung lesen.

Victoria 3 schreibt Spielstände (*.v3) als Zeile „SAV…“ und danach entweder Klartext oder ein
ZIP-Archiv mit den Einträgen „meta“ und „gamestate“. Im normalen Spiel ist der Inhalt binär
kodiert; lesbar ist nur Klartext (Startoption -debug_mode, kein Ironman). Gelesen wird nur der
Block meta_data (Datum, Land, Version) bzw. der Anfang der Datei – der vollständige Spielstand
ist für den Berater zu groß.

Nur Standardbibliothek.
"""

from __future__ import annotations

import os
import re
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path

# Höchstens so viel vom Spielstand wird gelesen bzw. an den Berater weitergegeben.
KOPF_BYTES = 2 * 1024 * 1024
MAX_ZEICHEN = 6000


@dataclass
class Spielstand:
    pfad: Path
    lesbar: bool  # Klartext (True) oder binär kodiert (False)
    datum: str | None  # Spieldatum, z. B. "1848.3.1"
    auszug: str  # meta_data-Block oder Dateianfang, gekürzt; leer, wenn binär

    def als_kontext(self) -> str:
        """Text für die Anfrage an den Berater."""
        if not self.lesbar:
            return (
                f"Neuester Spielstand: {self.pfad.name} (binär kodiert, Inhalt nicht lesbar; "
                "nur der Screenshot zählt)."
            )
        datum = f", Spieldatum {self.datum}" if self.datum else ""
        return f"Neuester Spielstand: {self.pfad.name}{datum}. Auszug:\n{self.auszug}"


def standardordner() -> Path:
    """Ordner der Spielstände nach Betriebssystem (OneDrive-Umleitung unter Windows beachten)."""
    if sys.platform.startswith("linux"):
        basis = Path.home() / ".local" / "share"
    else:
        basis = Path.home() / "Documents"
        onedrive = os.environ.get("OneDrive")
        if sys.platform == "win32" and onedrive and not (basis / "Paradox Interactive").exists():
            basis = Path(onedrive) / "Documents"
    return basis / "Paradox Interactive" / "Victoria 3" / "save games"


def neuester(ordner: Path) -> Path | None:
    """Zuletzt geschriebener Spielstand im Ordner oder None."""
    try:
        dateien = [p for p in ordner.glob("*.v3") if p.is_file()]
    except OSError:
        return None
    return max(dateien, key=lambda p: p.stat().st_mtime, default=None)


def _ist_text(daten: bytes) -> bool:
    probe = daten[:4096]
    if not probe or b"\x00" in probe:
        return False
    try:
        probe.decode("utf-8")
    except UnicodeDecodeError as e:
        # Am Ende der Probe kann ein Mehrbytezeichen abgeschnitten sein.
        if e.start < len(probe) - 3:
            return False
    druckbar = sum(1 for b in probe if b >= 32 or b in (9, 10, 13))
    return druckbar / len(probe) > 0.95


def _ohne_kopfzeile(daten: bytes) -> bytes:
    if daten.startswith(b"SAV"):
        ende = daten.find(b"\n")
        return daten[ende + 1 :] if ende >= 0 else b""
    return daten


def block(text: str, schluessel: str) -> str | None:
    """Erster Block `schluessel={ … }` samt Klammern; Klammern in Anführungszeichen zählen nicht."""
    treffer = re.search(r"(?m)^\s*" + re.escape(schluessel) + r"\s*=\s*\{", text)
    if not treffer:
        return None
    tiefe = 0
    in_text = False
    i = treffer.end() - 1
    while i < len(text):
        z = text[i]
        if in_text:
            if z == "\\":
                i += 1
            elif z == '"':
                in_text = False
        elif z == '"':
            in_text = True
        elif z == "{":
            tiefe += 1
        elif z == "}":
            tiefe -= 1
            if tiefe == 0:
                return text[treffer.start() : i + 1].strip()
        i += 1
    return None


def _kuerzen(text: str) -> str:
    if len(text) <= MAX_ZEICHEN:
        return text
    return text[:MAX_ZEICHEN] + "\n… (gekürzt)"


def _auswerten(pfad: Path, daten: bytes) -> Spielstand:
    if not _ist_text(daten):
        return Spielstand(pfad, lesbar=False, datum=None, auszug="")
    text = daten.decode("utf-8", errors="replace")
    meta = block(text, "meta_data")
    auszug = _kuerzen(meta if meta else text)
    datum = re.search(r"\b(?:game_date|date)\s*=\s*\"?(\d{3,4}\.\d{1,2}\.\d{1,2})", auszug)
    return Spielstand(pfad, lesbar=True, datum=datum.group(1) if datum else None, auszug=auszug)


def lese(pfad: Path) -> Spielstand:
    """Liest Kopf bzw. meta-Eintrag eines Spielstands. Wirft OSError oder zipfile.BadZipFile."""
    with open(pfad, "rb") as f:
        kopf = f.read(KOPF_BYTES)
    rumpf = _ohne_kopfzeile(kopf)
    if not rumpf.startswith(b"PK"):
        return _auswerten(pfad, rumpf)
    # zipfile findet das Archiv auch hinter der Kopfzeile „SAV…“.
    with zipfile.ZipFile(pfad) as archiv:
        namen = archiv.namelist()
        eintrag = "meta" if "meta" in namen else ("gamestate" if "gamestate" in namen else namen[0])
        with archiv.open(eintrag) as f:
            return _auswerten(pfad, f.read(KOPF_BYTES))


class Beobachter:
    """Meldet einen neuen oder überschriebenen Spielstand, sobald seine Größe stabil ist.

    `pruefe()` wird regelmäßig aufgerufen (z. B. alle paar Sekunden) und gibt einmal je neuem
    Stand dessen Pfad zurück, sonst None. Stände, die beim Start schon da sind, gelten als bekannt.
    """

    def __init__(self, ordner: Path):
        self.ordner = ordner
        self._gemeldet = self._kennung(neuester(ordner))
        self._kandidat: tuple | None = None

    @staticmethod
    def _kennung(pfad: Path | None) -> tuple | None:
        if pfad is None:
            return None
        try:
            st = pfad.stat()
        except OSError:
            return None
        return (str(pfad), st.st_mtime_ns, st.st_size)

    def pruefe(self) -> Path | None:
        pfad = neuester(self.ordner)
        kennung = self._kennung(pfad)
        if kennung is None or kennung == self._gemeldet:
            self._kandidat = None
            return None
        if kennung != self._kandidat:
            # Wird womöglich noch geschrieben: erst melden, wenn er beim nächsten Mal gleich ist.
            self._kandidat = kennung
            return None
        self._gemeldet = kennung
        self._kandidat = None
        return pfad
