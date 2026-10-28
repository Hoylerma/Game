#!/usr/bin/env python3
"""Vic3-Berater: KI-Berater für Victoria 3, der live neben dem Spiel Tipps gibt.

Ein kleines Fenster bleibt über dem Spiel. Auf Tastendruck (F9) macht der Berater einen
Screenshot, schickt ihn an Claude und zeigt, welche Entscheidung jetzt am besten ist –
Gebäude, Gesetze, Haushalt, Diplomatie. Mit F10 stellt man eine eigene Frage. Optional prüft
er in festen Abständen oder bei jedem Autosave von selbst.

Aufruf:
  python berater.py                         Fenster starten (F9 Analyse, F10 Frage)
  python berater.py --ziel "Großmacht"      Ziel der Partie mitgeben
  python berater.py --auto 5                alle 5 Minuten automatisch prüfen
  python berater.py --bei-autosave          bei jedem neuen Spielstand prüfen
  python berater.py --probe                 Screenshot und Spielstand testen, ohne API

Voraussetzungen: siehe README.md (pip install -r requirements.txt, ANTHROPIC_API_KEY).
"""

from __future__ import annotations

import argparse
import queue
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import beratung  # noqa: E402
import bildschirm  # noqa: E402
import spielstand  # noqa: E402

FARBEN = {"grund": "#1e1f24", "text": "#e8e6e1", "leise": "#9a978f", "akzent": "#d9a441", "feld": "#2b2d33"}
AUFWAENDE = ("low", "medium", "high", "xhigh", "max")


def fehlertext(e: BaseException) -> str:
    """Verständliche Meldung für Fehler der API und der Aufnahme."""
    try:
        import anthropic
    except ImportError:
        return f"Fehler: {e}"
    if isinstance(e, anthropic.AuthenticationError):
        return "API-Schlüssel fehlt oder ist ungültig. ANTHROPIC_API_KEY setzen (siehe README)."
    if isinstance(e, anthropic.PermissionDeniedError):
        return "Der API-Schlüssel hat keinen Zugriff auf dieses Modell."
    if isinstance(e, anthropic.RateLimitError):
        return "Zu viele Anfragen – kurz warten und erneut versuchen."
    if isinstance(e, anthropic.APIStatusError):
        return f"Fehler der API ({e.status_code}): {e.message}"
    if isinstance(e, anthropic.APIConnectionError):
        return "Keine Verbindung zur API. Internetverbindung prüfen."
    return f"Fehler: {e}"


class App:
    def __init__(self, args: argparse.Namespace):
        import tkinter as tk

        self.tk = tk
        self.args = args
        self.ereignisse: queue.Queue = queue.Queue()
        self.beschaeftigt = False
        self.auto_laeuft = False
        self.naechste_auto = 0.0
        self.letzter_stand: spielstand.Spielstand | None = None
        self.gesendeter_stand: tuple | None = None  # zuletzt erfolgreich mitgeschickter Spielstand
        self.takte = 0

        self.beobachter = None
        if args.spielstaende:
            ordner = Path(args.spielstaende).expanduser()
            if ordner.is_dir():
                self.beobachter = spielstand.Beobachter(ordner)
                self._stand_lesen(spielstand.neuester(ordner))

        self.berater = None
        self.startfehler = None
        try:
            import anthropic

            self.berater = beratung.Berater(
                anthropic.Anthropic(),
                modell=args.modell,
                aufwand=args.aufwand,
                ziel=args.ziel,
                fallbacks=not args.ohne_fallback,
            )
        except Exception as e:  # fehlendes Paket oder fehlende Zugangsdaten
            self.startfehler = fehlertext(e)

        self._fenster()
        self._tasten()
        self.root.after(50, self._pumpe)
        self.root.after(1000, self._takt)

    # ── Fenster ────────────────────────────────────────────────────────────
    def _fenster(self) -> None:
        tk = self.tk
        root = self.root = tk.Tk()
        root.title("Vic3-Berater")
        root.configure(bg=FARBEN["grund"])
        root.attributes("-topmost", True)
        self.alpha = self.args.deckkraft
        root.attributes("-alpha", self.alpha)
        breite, hoehe = 440, 560
        root.geometry(f"{breite}x{hoehe}+{root.winfo_screenwidth() - breite - 20}+60")
        root.minsize(320, 300)

        schrift = ("Segoe UI", 11) if sys.platform == "win32" else ("Helvetica", 11)
        self.status = tk.StringVar(value="Bereit.")
        tk.Label(
            root, textvariable=self.status, bg=FARBEN["grund"], fg=FARBEN["akzent"], anchor="w",
            font=(schrift[0], 10), wraplength=breite - 20, justify="left",
        ).pack(fill="x", padx=10, pady=(8, 4))

        rahmen = tk.Frame(root, bg=FARBEN["grund"])
        rahmen.pack(fill="both", expand=True, padx=10)
        self.text = tk.Text(
            rahmen, wrap="word", bg=FARBEN["feld"], fg=FARBEN["text"], font=schrift, relief="flat",
            padx=8, pady=6, insertbackground=FARBEN["text"], state="disabled",
        )
        leiste = tk.Scrollbar(rahmen, command=self.text.yview)
        self.text.configure(yscrollcommand=leiste.set)
        leiste.pack(side="right", fill="y")
        self.text.pack(side="left", fill="both", expand=True)

        zeile = tk.Frame(root, bg=FARBEN["grund"])
        zeile.pack(fill="x", padx=10, pady=(6, 2))
        self.eingabe = tk.Entry(
            zeile, bg=FARBEN["feld"], fg=FARBEN["text"], insertbackground=FARBEN["text"],
            relief="flat", font=schrift,
        )
        self.eingabe.pack(side="left", fill="x", expand=True, ipady=4)
        self.eingabe.bind("<Return>", lambda _e: self._frage_senden())
        tk.Button(zeile, text="Fragen", command=self._frage_senden).pack(side="left", padx=(6, 0))

        unten = tk.Frame(root, bg=FARBEN["grund"])
        unten.pack(fill="x", padx=10, pady=(2, 8))
        tk.Button(
            unten, text=f"Analysieren ({self.args.taste_analyse.strip('<>').upper()})",
            command=lambda: self._starte(beratung.ANALYSE),
        ).pack(side="left")
        self.mit_bild = tk.BooleanVar(value=True)
        tk.Checkbutton(
            unten, text="Screenshot", variable=self.mit_bild, bg=FARBEN["grund"], fg=FARBEN["leise"],
            selectcolor=FARBEN["feld"], activebackground=FARBEN["grund"],
        ).pack(side="left", padx=6)
        self.auto = tk.BooleanVar(value=self.args.auto > 0)
        tk.Checkbutton(
            unten, text=f"Auto ({self.args.auto or 5} min)", variable=self.auto,
            command=self._auto_umgeschaltet, bg=FARBEN["grund"], fg=FARBEN["leise"],
            selectcolor=FARBEN["feld"], activebackground=FARBEN["grund"],
        ).pack(side="left")
        self._auto_umgeschaltet()

        if self.startfehler:
            self._zeige(self.startfehler)
            self.status.set("Nicht bereit.")
        else:
            hinweis = (
                f"{self.args.taste_analyse.strip('<>').upper()}: Screenshot analysieren · "
                f"{self.args.taste_frage.strip('<>').upper()}: Frage stellen"
            )
            self._zeige(
                "Victoria 3 im Modus „Randloses Fenster“ starten, damit dieses Fenster sichtbar "
                f"bleibt.\n\n{hinweis}"
            )
            self.status.set("Bereit." + (f" Spielstände: {self.beobachter.ordner}" if self.beobachter else ""))

    def _zeige(self, inhalt: str, anhaengen: bool = False) -> None:
        self.text.configure(state="normal")
        if not anhaengen:
            self.text.delete("1.0", "end")
        self.text.insert("end", inhalt)
        self.text.see("end")
        self.text.configure(state="disabled")

    # ── Tasten (global, auch wenn das Spiel den Fokus hat) ─────────────────
    def _tasten(self) -> None:
        try:
            from pynput import keyboard
        except ImportError:
            self.status.set("pynput fehlt: Tasten F9/F10 gehen nicht, nur die Knöpfe.")
            return
        try:
            self.hotkeys = keyboard.GlobalHotKeys(
                {
                    self.args.taste_analyse: lambda: self.ereignisse.put(("analyse",)),
                    self.args.taste_frage: lambda: self.ereignisse.put(("frage",)),
                }
            )
            self.hotkeys.daemon = True
            self.hotkeys.start()
        except Exception as e:
            self.status.set(f"Tasten nicht verfügbar ({e}); nur die Knöpfe.")

    # ── Ablauf ─────────────────────────────────────────────────────────────
    def _pumpe(self) -> None:
        """Verarbeitet Ereignisse aus anderen Threads im Thread der Oberfläche."""
        try:
            while True:
                art, *daten = self.ereignisse.get_nowait()
                if art == "analyse":
                    self._starte(beratung.ANALYSE)
                elif art == "frage":
                    self.root.deiconify()
                    self.root.lift()
                    self.root.focus_force()
                    self.eingabe.focus_set()
                elif art == "text":
                    self._zeige(daten[0], anhaengen=True)
                elif art == "fertig":
                    self._fertig(*daten)
                elif art == "fehler":
                    self.beschaeftigt = False
                    self._zeige(daten[0])
                    self.status.set("Fehler.")
        except queue.Empty:
            pass
        self.root.after(50, self._pumpe)

    def _takt(self) -> None:
        """Einmal je Sekunde: Spielstände beobachten und automatische Prüfung auslösen."""
        self.takte += 1
        if self.beobachter and self.takte % 3 == 0:
            neu = self.beobachter.pruefe()
            if neu:
                self._stand_lesen(neu)
                if self.args.bei_autosave:
                    self._starte(beratung.AUTO, auto=True)
        if self.auto.get() and time.monotonic() >= self.naechste_auto and not self.beschaeftigt:
            self._starte(beratung.AUTO, auto=True)
        self.root.after(1000, self._takt)

    def _auto_umgeschaltet(self) -> None:
        self.naechste_auto = time.monotonic() + 60 * (self.args.auto or 5)

    def _stand_lesen(self, pfad: Path | None) -> None:
        if pfad is None:
            return
        try:
            self.letzter_stand = spielstand.lese(pfad)
        except Exception as e:
            print(f"Spielstand {pfad} nicht lesbar: {e}", file=sys.stderr)

    def _kontext(self) -> tuple[str | None, tuple | None]:
        """Spielstand-Auszug samt Kennung, aber nur, wenn er noch nicht mitgeschickt wurde."""
        stand = self.letzter_stand
        if stand is None:
            return None, None
        try:
            kennung = (str(stand.pfad), stand.pfad.stat().st_mtime_ns)
        except OSError:
            return None, None
        if kennung == self.gesendeter_stand:
            return None, None
        return stand.als_kontext(), kennung

    def _frage_senden(self) -> None:
        frage = self.eingabe.get().strip()
        if frage:
            self.eingabe.delete(0, "end")
            self._starte(frage, mit_bild=self.mit_bild.get())

    def _starte(self, frage: str, auto: bool = False, mit_bild: bool = True) -> None:
        if self.berater is None:
            self.status.set("Nicht bereit: " + (self.startfehler or ""))
            return
        if self.beschaeftigt:
            if not auto:
                self.status.set("Der Berater denkt noch nach …")
            return
        self.beschaeftigt = True
        if auto:
            self._auto_umgeschaltet()
        self.status.set("Screenshot …" if mit_bild else "Frage wird gesendet …")
        if mit_bild:
            # Fenster kurz unsichtbar machen (ohne Fokuswechsel), damit es nicht im Bild ist.
            self.root.attributes("-alpha", 0.0)
            self.root.update_idletasks()
            self.root.after(150, lambda: self._aufnehmen(frage, auto))
        else:
            self._los(frage, None, auto)

    def _aufnehmen(self, frage: str, auto: bool) -> None:
        try:
            bild = bildschirm.aufnehmen(self.args.monitor, self.args.max_kante)
        except Exception as e:
            self.root.attributes("-alpha", self.alpha)
            self.beschaeftigt = False
            self._zeige(f"Screenshot fehlgeschlagen: {e}")
            return
        self.root.attributes("-alpha", self.alpha)
        self._los(frage, bild, auto)

    def _los(self, frage: str, bild: bytes | None, auto: bool) -> None:
        kontext, kennung = self._kontext()
        if not auto:
            self._zeige("" if frage == beratung.ANALYSE else f"» {frage}\n\n")
        self.status.set("Automatische Prüfung …" if auto else "Der Berater denkt nach …")
        self.root.title("Vic3-Berater")

        def arbeit():
            try:
                antwort = self.berater.frage(
                    frage, bild, kontext,
                    bei_text=None if auto else (lambda s: self.ereignisse.put(("text", s))),
                )
                self.ereignisse.put(("fertig", antwort, auto, kennung))
            except Exception as e:
                self.ereignisse.put(("fehler", fehlertext(e)))

        threading.Thread(target=arbeit, daemon=True).start()

    def _fertig(self, antwort: beratung.Antwort, auto: bool, kennung: tuple | None) -> None:
        self.beschaeftigt = False
        if kennung and not antwort.abgelehnt:
            self.gesendeter_stand = kennung
        uhr = time.strftime("%H:%M")
        if auto:
            if antwort.nichts_neues:
                self.status.set(f"Automatische Prüfung {uhr}: nichts Neues.")
                return
            self._zeige(antwort.text)
            self.root.title("★ Neuer Tipp – Vic3-Berater")
            self.root.bell()
            self.status.set(f"Neuer Tipp aus der automatischen Prüfung ({uhr}).")
            return
        if antwort.abgelehnt:
            self._zeige(antwort.text)
        elif antwort.abgeschnitten:
            self._zeige("\n\n(Antwort abgeschnitten)", anhaengen=True)
        self.status.set(f"Fertig ({uhr}).")

    def laufen(self) -> None:
        self.root.mainloop()


def probe(args: argparse.Namespace) -> int:
    """Prüft Aufnahme und Spielstand, ohne die API zu benutzen."""
    ok = True
    try:
        bild = bildschirm.aufnehmen(args.monitor, args.max_kante)
        ziel = Path("probe.jpg").resolve()
        ziel.write_bytes(bild)
        print(f"Screenshot: {len(bild) // 1024} KB, gespeichert als {ziel}")
    except Exception as e:
        ok = False
        print(f"Screenshot fehlgeschlagen: {e}")
    if args.spielstaende:
        ordner = Path(args.spielstaende).expanduser()
        pfad = spielstand.neuester(ordner)
        if pfad is None:
            print(f"Kein Spielstand in {ordner}")
        else:
            try:
                print(spielstand.lese(pfad).als_kontext()[:1500])
            except Exception as e:
                ok = False
                print(f"Spielstand {pfad} nicht lesbar: {e}")
    return 0 if ok else 1


def argumente(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="KI-Berater für Victoria 3 (Claude).")
    p.add_argument("--ziel", help="Ziel der Partie, z. B. „Deutschland einigen und Großmacht werden“")
    p.add_argument("--auto", type=int, default=0, metavar="MIN", help="alle MIN Minuten automatisch prüfen")
    p.add_argument("--bei-autosave", action="store_true", help="bei jedem neuen Spielstand prüfen")
    p.add_argument(
        "--spielstaende", default=str(spielstand.standardordner()), metavar="ORDNER",
        help="Ordner der Spielstände (leer lassen zum Abschalten)",
    )
    p.add_argument("--modell", default=beratung.MODELL)
    p.add_argument(
        "--aufwand", choices=AUFWAENDE, default="medium",
        help="Denkaufwand: low schnell und günstig, high/xhigh gründlicher (Standard: medium)",
    )
    p.add_argument("--monitor", type=int, default=1, help="1 = Hauptmonitor, 2 = zweiter, 0 = alle")
    p.add_argument("--max-kante", type=int, default=bildschirm.MAX_KANTE, help="Screenshot verkleinern (Pixel)")
    p.add_argument("--taste-analyse", default="<f9>", help="Taste für die Analyse (pynput-Schreibweise)")
    p.add_argument("--taste-frage", default="<f10>", help="Taste für eine eigene Frage")
    p.add_argument("--deckkraft", type=float, default=0.93, help="Deckkraft des Fensters (0.3 bis 1)")
    p.add_argument("--ohne-fallback", action="store_true", help="kein Ersatzmodell bei abgelehnten Anfragen")
    p.add_argument("--probe", action="store_true", help="Screenshot und Spielstand testen, ohne API")
    args = p.parse_args(argv)
    args.deckkraft = min(1.0, max(0.3, args.deckkraft))
    return args


def main(argv: list[str] | None = None) -> int:
    args = argumente(argv)
    if sys.platform == "win32":
        try:  # scharfe Schrift und echte Pixel bei Skalierung > 100 %
            import ctypes

            ctypes.windll.shcore.SetProcessDpiAwareness(2)
        except Exception:
            pass
    if args.probe:
        return probe(args)
    App(args).laufen()
    return 0


if __name__ == "__main__":
    sys.exit(main())
