#!/usr/bin/env python3
"""Pakete von EINIGKEIT für Linux, Windows und macOS (seit M13b5, docs/technik/vertical_slice.md 7).

Zwei Befehle:

    python3 tools/paket/paket.py bauen --system linux --godot PFAD [--ausgabe build/pakete]
    python3 tools/paket/paket.py rauchtest --programm PFAD --cli PFAD [--tage 30]

`bauen` exportiert das Godot-Projekt mit der Vorlage aus game/export_presets.cfg (Release), legt data/,
DRITTANBIETER.txt und LIESMICH.txt dazu und packt das Ergebnis:

    linux    einigkeit-<version>-linux-x86_64.tar.gz   (Ordner mit einigkeit.x86_64, .pck, Brücke, data/)
    windows  einigkeit-<version>-windows-x86_64.zip    (Ordner mit einigkeit.exe, .pck, Brücke, data/);
             den Installer baut Inno Setup aus einigkeit.iss mit dem Ordner (nur auf Windows)
    macos    einigkeit-<version>-macos.dmg              (EINIGKEIT.app, data/ in Contents/Resources;
             DMG und Ad-hoc-Signatur nur auf macOS, sonst bleibt der Ordner mit der .app)

Voraussetzungen: die Brücke als Release (cmake --preset bridge-release && cmake --build --preset
bridge-release; auf macOS mit -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"), die Exportvorlagen von Godot (Version wie das Programm) und gerenderte Musik
(tools/klang/klang.py); `bauen` importiert das Projekt vor dem Export.

`rauchtest` startet das gepackte Programm ohne Fenster für beide Szenarien (Westfälischer Frieden,
Völkerfrühling) 30 Tage lang (Startparameter --rauchtest) und vergleicht die Prüfsumme mit uhrwerk-cli.

Exitcodes: 0 = Erfolg, 1 = Rauchtest weicht ab, 2 = Fehler.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import textwrap
import time
import zipfile
from collections.abc import Sequence
from pathlib import Path

HIER = Path(__file__).resolve().parent
REPO = HIER.parents[1]
SPIEL = REPO / "game"
DATEN = REPO / "data"

SYSTEME = {
    # system: (Vorlage in export_presets.cfg, Programmdatei im Paketordner, Release-Bibliothek der Brücke)
    "linux": ("Linux", "einigkeit.x86_64", "libuhrwerk_bridge.release.so"),
    "windows": ("Windows", "einigkeit.exe", "uhrwerk_bridge.release.dll"),
    "macos": ("macOS", "Einigkeit.zip", "libuhrwerk_bridge.release.dylib"),
}
SZENARIEN = ("westfaelischer_frieden", "voelkerfruehling")
GEODATEN = REPO / "tools" / "geodaten" / "quellen.json"
LIZENZ = REPO / "LICENSE.md"
MARKE = "=== LIZENZEN ==="


class PaketFehler(Exception):
    pass


def version() -> str:
    text = (SPIEL / "project.godot").read_text(encoding="utf-8")
    treffer = re.search(r'^config/version="([^"]+)"', text, re.MULTILINE)
    if not treffer:
        raise PaketFehler("game/project.godot: config/version fehlt")
    return treffer.group(1)


def _lauf(befehl: Sequence[str | Path], **kw) -> subprocess.CompletedProcess[str]:
    print("  $ " + " ".join(str(t) for t in befehl), file=sys.stderr, flush=True)
    return subprocess.run([str(t) for t in befehl], check=False, text=True, encoding="utf-8", errors="replace", **kw)


# ─── Lizenzen ────────────────────────────────────────────────────────────────


def godot_lizenzen(godot: Path) -> str:
    """Engine-Lizenz und Bestandteile der Engine, ausgegeben von Godot selbst (lizenzen.gd)."""
    erg = _lauf([godot, "--headless", "--script", HIER / "lizenzen.gd"], capture_output=True)
    if erg.returncode != 0 or MARKE not in erg.stdout:
        raise PaketFehler(f"lizenzen.gd fehlgeschlagen:\n{erg.stdout}\n{erg.stderr}")
    return erg.stdout.split(MARKE, 1)[1].strip()


def zstd_lizenz() -> str:
    for pfad in sorted(REPO.glob("build/*/_deps/zstd-src/LICENSE")):
        return pfad.read_text(encoding="utf-8")
    raise PaketFehler("Lizenz von zstd nicht gefunden (build/*/_deps/zstd-src/LICENSE; erst die Brücke bauen)")


def geodaten_hinweise() -> str:
    """Namensnennung der Geodaten, aus denen Karte und Orte entstanden sind (tools/geodaten/quellen.json).

    Quellen der Gruppe „referenz“ dienen nur der Vorschau und stecken nicht im Spiel.
    """
    manifest = json.loads(GEODATEN.read_text(encoding="utf-8"))
    natural_earth: list[str] = []
    ne_kopf = ""
    absaetze: list[str] = []
    for q in manifest["quellen"]:
        if q["gruppe"] == "referenz":
            continue
        treffer = re.match(r"^(Made with Natural Earth\..*?) \((Version [^,]+), (.+)\)$", q["quellenangabe"])
        if treffer:
            ne_kopf = f"{treffer[1]} ({treffer[2]}; {q['lizenz']})"
            natural_earth.append(treffer[3])
        else:
            absaetze.append(f"{q['quellenangabe']}\nLizenz: {q['lizenz']}")
    if natural_earth:
        absaetze.insert(0, f"{ne_kopf}\nVerwendet: {', '.join(natural_earth)}.")
    kacheln = manifest.get("kacheln")
    if kacheln:
        absaetze.append(f"{kacheln['quellenangabe']}\nLizenz: {kacheln['lizenz']}")
    vorspann = textwrap.fill(
        "Küsten, Seen, Flüsse, Höhen sowie Lage und Namen der Orte (data/karte, data/tabellen/orte.csv, "
        "kreise.csv) hat EINIGKEIT aus den folgenden Quellen erzeugt (tools/geodaten). Die Daten wurden "
        "verändert: zugeschnitten, ausgewählt, vereinfacht, zu Orten und Kreisen zusammengefasst und um "
        "historische Namen ergänzt.",
        78,
    )
    umbrochen = [
        "\n".join(textwrap.fill(zeile, 78) for zeile in a.replace("siehe quellenangabe", "siehe oben").splitlines())
        for a in absaetze
    ]
    return vorspann + "\n\n" + "\n\n".join(umbrochen)


def drittanbieter(godot: Path) -> str:
    teile = [
        "EINIGKEIT – Bestandteile von Dritten",
        "=" * 37,
        "",
        "EINIGKEIT selbst steht unter der PolyForm Noncommercial License 1.0.0 (LICENSE.md): Nutzung,",
        "Veränderung und Weitergabe sind für nicht kommerzielle Zwecke erlaubt. Die folgenden Programme,",
        "Bibliotheken, Schriften und Daten anderer Urheber behalten ihre eigenen Lizenzen; sie stehen",
        "vollständig in dieser Datei.",
        "",
        "  Godot Engine                 MIT (mit den Bestandteilen der Engine, siehe unten)",
        "  godot-cpp                    MIT",
        "  zstd 1.5.6                   BSD (Kompression der Spielstände)",
        "  Source Serif 4, Source Code Pro, Playfair Display, UnifrakturMaguntia",
        "                               SIL Open Font License 1.1",
        "  GeoNames                     CC BY 4.0 (Orte und Ortsnamen)",
        "  Natural Earth                gemeinfrei (Küsten, Seen, Flüsse)",
        "  Terrain Tiles                Namensnennung der Höhenquellen (Gelände)",
        "",
        "Die Musikstücke (Beethoven, Haydn, Silcher, Schumann, Rouget de Lisle) sind gemeinfrei; Satz und",
        "Klang erzeugt EINIGKEIT selbst (tools/klang). Abgeglichen wurden sie an Notensätzen des",
        "Mutopia-Projekts (www.mutopiaproject.org) von Stelios Samelis, Stan Sanderson, Coyau und",
        "Philippe Hézaine; das Kaiserlied am Satz von Klaus Rettinghaus (Creative Commons BY 4.0).",
        "Die historischen Daten (Staaten, Besitz, Bevölkerung, Ereignisse) stammen aus dem Projekt.",
        "",
    ]

    def abschnitt(titel: str, text: str) -> None:
        teile.extend(["", "─" * 78, titel, "─" * 78, "", text.strip(), ""])

    abschnitt("Godot Engine", godot_lizenzen(godot))
    abschnitt("godot-cpp", (REPO / "third_party" / "godot-cpp" / "LICENSE.md").read_text(encoding="utf-8"))
    abschnitt("zstd", zstd_lizenz())
    abschnitt("Geodaten: GeoNames, Natural Earth, Terrain Tiles", geodaten_hinweise())
    for ofl in sorted((SPIEL / "assets" / "schriften").glob("OFL_*.txt")):
        abschnitt(f"Schrift {ofl.stem.removeprefix('OFL_')}", ofl.read_text(encoding="utf-8"))
    return "\n".join(teile)


def liesmich(system: str, ver: str) -> str:
    start = {
        "linux": "./einigkeit.x86_64 starten (oder per Doppelklick).",
        "windows": "einigkeit.exe starten (oder den Installer verwenden).",
        "macos": "EINIGKEIT.app in den Programme-Ordner ziehen und mit Rechtsklick → Öffnen starten "
        "(die App ist nicht notarisiert).",
    }[system]
    return f"""EINIGKEIT {ver} – Spielbarer Ausschnitt 1848–1871
{"=" * 50}

Start: {start}

Spielstände, Einstellungen und Rückmeldungen liegen im Benutzerordner von Godot:
  Linux    ~/.local/share/godot/app_userdata/EINIGKEIT
  Windows  %APPDATA%\\Godot\\app_userdata\\EINIGKEIT
  macOS    ~/Library/Application Support/Godot/app_userdata/EINIGKEIT

Rückmeldung: Spielmenü (Esc) → Rückmeldung → „Bericht anlegen“ legt ein ZIP in rueckmeldungen/ an
(Spielstand, Notiz, Protokoll). Es wird nichts versandt; bitte die Datei selbst schicken.

Der Ordner data/ gehört zum Spiel und muss neben dem Programm liegen (macOS: in der App).
Lizenz: PolyForm Noncommercial 1.0.0 (LICENSE.md) – nicht kommerzielle Nutzung erlaubt.
Lizenzen der enthaltenen Bestandteile von Dritten: DRITTANBIETER.txt.
"""


# ─── Bauen ───────────────────────────────────────────────────────────────────


def exportieren(godot: Path, system: str, ziel: Path) -> Path:
    vorlage, datei, bibliothek = SYSTEME[system]
    if not (SPIEL / "bin" / bibliothek).is_file():
        raise PaketFehler(f"game/bin/{bibliothek} fehlt (cmake --preset bridge-release && cmake --build --preset bridge-release)")
    if not (SPIEL / "audio" / "musik" / "stuecke.cfg").is_file():
        raise PaketFehler("Musik fehlt (python3 tools/klang/klang.py)")
    erg = _lauf([godot, "--headless", "--path", SPIEL, "--import"], capture_output=True)
    if erg.returncode != 0 or "SCRIPT ERROR" in erg.stdout + erg.stderr:
        raise PaketFehler(f"Import fehlgeschlagen:\n{erg.stdout[-3000:]}\n{erg.stderr[-3000:]}")
    ziel.mkdir(parents=True, exist_ok=True)
    programm = ziel / datei
    erg = _lauf([godot, "--headless", "--path", SPIEL, "--export-release", vorlage, programm], capture_output=True)
    ausgabe = erg.stdout + erg.stderr
    fehler = exportfehler(ausgabe)
    if erg.returncode != 0 or not programm.is_file() or fehler:
        raise PaketFehler(f"Export „{vorlage}“ fehlgeschlagen:\n" + ("\n".join(fehler) or ausgabe[-4000:]))
    if not bibliothek_im_export(programm, bibliothek):
        raise PaketFehler(f"Export „{vorlage}“ ohne die Brücke {bibliothek}")
    return programm


# Meldungen des Editors, wenn nur die Release-Brücke gebaut ist (CI): Er lädt beim Start die
# Debug-Fassung, die fehlt; der Release-Export braucht sie nicht (bibliothek_im_export prüft das).
_HARMLOS = (
    re.compile(r'Condition "!FileAccess::exists\(path\)" is true\. Returning: ERR_FILE_NOT_FOUND'),
    re.compile(r"GDExtension dynamic library not found: 'res://bin/uhrwerk\.gdextension'"),
    re.compile(r"Error loading extension: 'res://bin/uhrwerk\.gdextension'"),
)


def exportfehler(ausgabe: str) -> list[str]:
    """Fehlerzeilen aus der Ausgabe des Exports, ohne die harmlosen Meldungen zur Debug-Brücke."""
    zeilen = [z.strip() for z in ausgabe.splitlines() if z.lstrip().startswith(("ERROR:", "SCRIPT ERROR:"))]
    return [z for z in zeilen if not any(m.search(z) for m in _HARMLOS)]


def bibliothek_im_export(programm: Path, bibliothek: str) -> bool:
    """Liegt die Release-Brücke im Export (neben dem Programm bzw. in der App im ZIP)?"""
    if programm.suffix == ".zip":
        with zipfile.ZipFile(programm) as z:
            return any(n.endswith(f"/Contents/Frameworks/{bibliothek}") for n in z.namelist())
    return (programm.parent / bibliothek).is_file()


def kopiere_daten(ziel: Path) -> None:
    if ziel.exists():
        shutil.rmtree(ziel)
    shutil.copytree(DATEN, ziel, ignore=shutil.ignore_patterns("__pycache__", "*.pyc", ".DS_Store"))


def bauen(system: str, godot: Path, ausgabe: Path) -> Path:
    ver = version()
    name = f"einigkeit-{ver}-{system}" + ("-x86_64" if system != "macos" else "")
    ordner = ausgabe / name
    if ordner.exists():
        shutil.rmtree(ordner)
    print(f"Paket {name}")
    programm = exportieren(godot, system, ordner)
    texte = {
        "DRITTANBIETER.txt": drittanbieter(godot),
        "LIESMICH.txt": liesmich(system, ver),
        "LICENSE.md": LIZENZ.read_text(encoding="utf-8"),
    }

    if system == "macos":
        with zipfile.ZipFile(programm) as z:
            z.extractall(ordner)
        programm.unlink()
        app = next(ordner.glob("*.app"))
        # zipfile setzt keine Ausführungsrechte: die Programmdatei der App wieder ausführbar machen.
        for datei in (app / "Contents" / "MacOS").iterdir():
            datei.chmod(0o755)
        ressourcen = app / "Contents" / "Resources"
        kopiere_daten(ressourcen / "data")
        for dateiname, text in texte.items():
            (ressourcen / dateiname).write_text(text, encoding="utf-8")
            (ordner / dateiname).write_text(text, encoding="utf-8")
        if platform.system() != "Darwin":
            print("Hinweis: DMG und Signatur nur auf macOS; der Ordner bleibt:", ordner)
            return ordner
        # Daten in der App ändern die Signatur: ad hoc neu signieren (keine Notarisierung).
        _pruefe(_lauf(["codesign", "--force", "--deep", "--sign", "-", app]))
        (ordner / "Programme").symlink_to("/Applications")
        dmg = ausgabe / f"{name}.dmg"
        dmg.unlink(missing_ok=True)
        # hdiutil meldet auf ausgelasteten Rechnern gelegentlich „Resource busy“: bis zu drei Versuche.
        for versuch in range(3):
            erg = _lauf(["hdiutil", "create", "-volname", "EINIGKEIT", "-srcfolder", ordner, "-ov", "-format", "UDZO", dmg])
            if erg.returncode == 0:
                break
            time.sleep(5 * (versuch + 1))
        _pruefe(erg)
        print("Fertig:", dmg)
        return dmg

    kopiere_daten(ordner / "data")
    for dateiname, text in texte.items():
        (ordner / dateiname).write_text(text, encoding="utf-8", newline="\r\n" if system == "windows" else "\n")
    if system == "linux":
        programm.chmod(0o755)
        archiv = ausgabe / f"{name}.tar.gz"
        with tarfile.open(archiv, "w:gz") as tar:
            tar.add(ordner, arcname=name)
    else:
        archiv = ausgabe / f"{name}.zip"
        archiv.unlink(missing_ok=True)
        with zipfile.ZipFile(archiv, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for datei in sorted(ordner.rglob("*")):
                z.write(datei, Path(name) / datei.relative_to(ordner))
    print("Fertig:", archiv)
    return archiv


def _pruefe(erg: subprocess.CompletedProcess[str]) -> None:
    if erg.returncode != 0:
        raise PaketFehler(f"Befehl fehlgeschlagen (Exitcode {erg.returncode})")


# ─── Rauchtest ───────────────────────────────────────────────────────────────


def _pruefsumme(text: str, muster: str) -> str | None:
    treffer = re.findall(muster, text)
    return treffer[-1] if treffer else None


def rauchtest(programm: Path, cli: Path, tage: int) -> int:
    """Programm ohne Fenster je Szenario `tage` Tage rechnen lassen; Prüfsumme wie uhrwerk-cli?"""
    if programm.suffix == ".app":
        programm = next((programm / "Contents" / "MacOS").iterdir())
    konsole = programm.with_name(programm.stem + ".console.exe")
    if programm.suffix == ".exe" and konsole.is_file():
        programm = konsole  # Windows: nur die Konsolenfassung gibt auf stdout aus
    fehler = 0
    for szenario in SZENARIEN:
        erwartet_erg = _lauf(
            [cli, "lauf", "--daten", DATEN, "--tage", str(tage), "--szenario", szenario], capture_output=True
        )
        erwartet = _pruefsumme(erwartet_erg.stdout, r"Prüfsumme: ([0-9a-f]{16})")
        env = dict(os.environ)
        env.pop("UHRWERK_DATEN", None)  # das Paket muss seine eigenen Daten finden
        erg = _lauf(
            [programm, "--headless", "--", "--rauchtest", f"--tage={tage}", f"--szenario={szenario}"],
            capture_output=True,
            env=env,
            timeout=600,
        )
        ausgabe = erg.stdout + erg.stderr
        bekommen = _pruefsumme(ausgabe, r"Rauchtest \S+ .* Prüfsumme ([0-9a-f]{16})")
        quelle = re.search(r"Datenordner (.*)", ausgabe)
        ok = erg.returncode == 0 and erwartet is not None and bekommen == erwartet
        if quelle and "neben der Programmdatei" not in quelle.group(1):
            ok = False
            print(f"  FEHLER: Daten nicht aus dem Paket: {quelle.group(1)}")
        print(f"  {szenario}: {bekommen} (uhrwerk-cli {erwartet}) {'ok' if ok else 'FEHLER'}")
        if not ok:
            print(ausgabe[-3000:])
            fehler += 1
    print("PAKETRAUCHTEST OK" if fehler == 0 else f"PAKETRAUCHTEST FEHLGESCHLAGEN ({fehler})")
    return 0 if fehler == 0 else 1


# ─── Aufruf ──────────────────────────────────────────────────────────────────


def _argumente() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="paket.py", description="Pakete von EINIGKEIT bauen und prüfen.")
    befehle = p.add_subparsers(dest="befehl", required=True)
    b = befehle.add_parser("bauen", help="exportieren und packen")
    b.add_argument("--system", choices=sorted(SYSTEME), required=True)
    b.add_argument("--godot", type=Path, required=True, help="Godot-Programm (Editor, gleiche Version wie die Vorlagen)")
    b.add_argument("--ausgabe", type=Path, default=REPO / "build" / "pakete")
    r = befehle.add_parser("rauchtest", help="gepacktes Programm gegen uhrwerk-cli prüfen")
    r.add_argument("--programm", type=Path, required=True, help="Programmdatei im Paket (macOS: die .app)")
    r.add_argument("--cli", type=Path, required=True, help="uhrwerk-cli (gleicher Stand)")
    r.add_argument("--tage", type=int, default=30)
    d = befehle.add_parser("drittanbieter", help="nur DRITTANBIETER.txt ausgeben")
    d.add_argument("--godot", type=Path, required=True)
    return p


def main(argv: Sequence[str] | None = None) -> int:
    args = _argumente().parse_args(argv)
    try:
        if args.befehl == "bauen":
            bauen(args.system, args.godot.resolve(), args.ausgabe.resolve())
            return 0
        if args.befehl == "drittanbieter":
            sys.stdout.write(drittanbieter(args.godot.resolve()))
            return 0
        return rauchtest(args.programm.resolve(), args.cli.resolve(), args.tage)
    except (PaketFehler, OSError, subprocess.TimeoutExpired, StopIteration) as fehler:
        print(f"Fehler: {fehler}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
