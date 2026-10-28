"""Der Berater: schickt Screenshot, Frage und Spielstand an Claude und liefert den Rat.

Der Verlauf wird nur angehängt, nie nachträglich bearbeitet (das verlangt die API für die
Denkblöcke). Wird er zu lang, beginnt ein neues Gespräch, das mit einer Zusammenfassung der
letzten Ratschläge startet; die alten Screenshots fallen dabei weg.
"""

from __future__ import annotations

import base64
from collections.abc import Callable
from dataclasses import dataclass

MODELL = "claude-opus-5-5"
FALLBACK_BETA = "server-side-fallback-2026-07-01"

# So beginnt eine Antwort, wenn es in der automatischen Prüfung nichts Neues gibt.
NICHTS_NEUES = "[nichts Neues]"

SYSTEM = f"""\
Du bist ein erfahrener Victoria-3-Spieler und Berater, der live neben dem Spiel läuft. Der Spieler \
schickt dir Screenshots seines Bildschirms, manchmal eine Frage und manchmal einen Auszug aus dem \
Spielstand. Deine Aufgabe: die beste nächste Entscheidung finden – Gebäude, Gesetze, \
Interessengruppen, Haushalt, Handel, Diplomatie, Krieg – und sie so knapp sagen, dass man sie \
während des Spielens lesen kann. Antworte auf Deutsch, benutze aber die Namen aus dem Spiel so, \
wie sie auf dem Screenshot stehen (auch wenn das Spiel auf Englisch läuft).

So liest du den Screenshot
- Erkenne zuerst, welches Fenster offen ist (Bauen, Markt, Politik/Gesetze, Interessengruppen, \
Budget, Diplomatie, Militär, Technologie, Staat, Ereignis …) und was oben in der Leiste steht \
(Datum, Geld, Wochenbilanz, Legitimität, Bauleistung, Infamie).
- Stütze dich auf Zahlen, die wirklich zu sehen sind. Erfinde keine Werte. Wenn für eine gute \
Entscheidung etwas fehlt, sag genau, welches Fenster der Spieler öffnen soll und schick dann \
trotzdem deine beste vorläufige Empfehlung.
- Spielversionen ändern Mechaniken. Richte dich nach dem, was die Oberfläche zeigt, und weise \
kurz darauf hin, wenn ein Rat von der Version abhängt.

Grundsätze, an denen du Entscheidungen misst (keine starre Reihenfolge – die Lage entscheidet)
- Bauleistung ist der Motor: Die Bauwarteschlange soll nie leerlaufen. Bausektor erweitern, \
solange Geld und Material (Holz, Werkzeuge, später Stahl und Glas) reichen.
- Baue gegen Knappheit: Güter mit deutlich erhöhtem Marktpreis und Defizit zuerst, vor allem \
Vorprodukte vieler Ketten (Werkzeuge, Stahl, Kohle, Eisen, Stoffe, Papier, Glas, Düngemittel, \
Sprengstoff, Motoren). Ein Gebäude lohnt sich, wenn es Gewinn je Beschäftigtem abwirft und \
Arbeitskräfte (Bauern, Arbeitslose) im Staat frei sind. Infrastruktur im Staat prüfen.
- Produktionsmethoden umstellen, wenn die Eingangsgüter verfügbar sind und es den Gewinn \
erhöht; Knappheiten, die eine Umstellung auslösen würde, bedenken.
- Haushalt: Wochenbilanz positiv halten, Schulden vermeiden bzw. kontrollieren; Steuern, \
Gehälter und Militärausgaben bewusst einstellen. Eine leere Kasse mit Schulden bremst alles.
- Politik: Gesetze wählen, die Wirtschaft und Stabilität voranbringen und für die eine \
Mehrheit der mächtigen Interessengruppen zu gewinnen ist. Erfolgschance, Legitimität, \
Radikale und mögliche Revolutionen abwägen; Unterstützer in die Regierung holen, Gegner \
schwächen. Typische frühe Ziele je nach Lage: bessere Wirtschaftsgesetze (z. B. Interventionismus \
oder Laissez-faire statt Merkantilismus/Traditionalismus), Abbau von Leibeigenschaft und \
Grundbesitzermacht, Ausbau von Bildung und Verwaltung.
- Lebensstandard und Bevölkerungszahl der Pops treiben Wachstum, Steuern und Zufriedenheit.
- Technologie: Produktions- und Gesellschaftstechnologien, die Bauleistung, Verwaltung und \
die wichtigsten Engpässe verbessern, haben Vorrang; Universitäten und Bildung zahlen sich aus.
- Diplomatie und Krieg: Infamie im Blick behalten, Kriegsziele nur gegen schwächere oder isolierte \
Gegner; Verbündete und Wohlwollen der Großmächte bedenken; Handelsabkommen und Zollunionen nutzen.

Format (das Fenster ist klein)
Lage: ein Satz zur aktuellen Situation.
Jetzt tun:
1. konkrete Aktion – kurz warum (mit Zahl vom Screenshot, wenn möglich)
2. …
(höchstens drei Punkte, wichtigster zuerst; konkrete Namen: welches Gebäude in welchem Staat, \
welches Gesetz, welche Produktionsmethode)
Im Blick: höchstens zwei kurze Warnungen oder nächste Schritte, nur wenn sinnvoll.

Insgesamt höchstens etwa 12 Zeilen, kein Markdown außer der Nummerierung. Bei einer direkten \
Frage antwortest du zuerst auf die Frage. Beginne deine sichtbare Antwort sofort.

Automatische Prüfung: Wenn eine Nachricht mit „Automatische Prüfung“ beginnt und sich seit \
deinem letzten Rat nichts Wichtiges geändert hat, antworte nur mit „{NICHTS_NEUES}“."""

ANALYSE = "Analysiere den Screenshot und sag mir die beste nächste Entscheidung."
AUTO = (
    "Automatische Prüfung: Gibt es auf diesem Screenshot etwas Dringendes oder eine klar bessere "
    "Entscheidung als bisher besprochen?"
)


@dataclass
class Antwort:
    text: str
    abgelehnt: bool = False  # Anfrage wurde aus Sicherheitsgründen abgelehnt
    abgeschnitten: bool = False  # max_tokens erreicht

    @property
    def nichts_neues(self) -> bool:
        return self.text.strip().startswith(NICHTS_NEUES)


class Berater:
    """Ein laufendes Beratungsgespräch.

    `client` ist ein anthropic.Anthropic (oder ein Ersatz mit `beta.messages.stream` in Tests).
    """

    def __init__(
        self,
        client,
        modell: str = MODELL,
        aufwand: str = "medium",
        ziel: str | None = None,
        max_runden: int = 8,
        fallbacks: bool = True,
        max_tokens: int = 16000,
    ):
        self.client = client
        self.modell = modell
        self.aufwand = aufwand
        self.ziel = ziel
        self.max_runden = max_runden
        self.fallbacks = fallbacks
        self.max_tokens = max_tokens
        self.verlauf: list[dict] = []
        self.ratschlaege: list[str] = []  # Antworttexte des laufenden Gesprächs
        self._zusammenfassung: str | None = None  # Vorspann für das nächste neue Gespräch

    def _runden(self) -> int:
        return sum(1 for m in self.verlauf if m["role"] == "assistant")

    def _neues_gespraech(self) -> None:
        """Einfache Verdichtung: Verlauf verwerfen, die letzten Ratschläge als Vorspann behalten."""
        letzte = [r[:1500] for r in self.ratschlaege[-3:] if not r.strip().startswith(NICHTS_NEUES)]
        self._zusammenfassung = (
            "Zusammenfassung des bisherigen Gesprächs – deine letzten Ratschläge:\n\n"
            + "\n\n---\n\n".join(letzte)
            if letzte
            else None
        )
        self.verlauf = []
        self.ratschlaege = []

    def _inhalt(self, frage: str, bild: bytes | None, kontext: str | None) -> list[dict]:
        teile = []
        if not self.verlauf:
            if self.ziel:
                teile.append(f"Mein Ziel in dieser Partie: {self.ziel}")
            if self._zusammenfassung:
                teile.append(self._zusammenfassung)
        if kontext:
            teile.append(kontext)
        teile.append(frage)
        inhalt: list[dict] = []
        if bild is not None:
            inhalt.append(
                {
                    "type": "image",
                    "source": {
                        "type": "base64",
                        "media_type": "image/jpeg",
                        "data": base64.standard_b64encode(bild).decode("ascii"),
                    },
                }
            )
        inhalt.append({"type": "text", "text": "\n\n".join(teile)})
        return inhalt

    def frage(
        self,
        frage: str,
        bild: bytes | None = None,
        kontext: str | None = None,
        bei_text: Callable[[str], None] | None = None,
    ) -> Antwort:
        """Stellt eine Frage (mit Screenshot und Spielstand-Kontext) und streamt die Antwort.

        `bei_text` erhält die Antwort stückweise, sobald sie entsteht. Fehler der API
        (anthropic.APIError und Unterklassen) werden nicht abgefangen.
        """
        if self._runden() >= self.max_runden:
            self._neues_gespraech()
        self.verlauf.append({"role": "user", "content": self._inhalt(frage, bild, kontext)})

        parameter = dict(
            model=self.modell,
            max_tokens=self.max_tokens,
            system=SYSTEM,
            messages=self.verlauf,
            thinking={"type": "adaptive"},
            output_config={"effort": self.aufwand},
            cache_control={"type": "ephemeral"},
        )
        if self.fallbacks:
            parameter.update(betas=[FALLBACK_BETA], fallbacks="default")

        try:
            with self.client.beta.messages.stream(**parameter) as stream:
                for stueck in stream.text_stream:
                    if bei_text:
                        bei_text(stueck)
                nachricht = stream.get_final_message()
        except BaseException:
            # Unbeantwortete Frage nicht im Verlauf lassen, sonst folgen zwei Nutzerzüge aufeinander.
            self.verlauf.pop()
            raise

        if nachricht.stop_reason == "refusal":
            self.verlauf.pop()
            return Antwort("Die Anfrage wurde abgelehnt. Bitte anders formulieren.", abgelehnt=True)

        text = "".join(b.text for b in nachricht.content if b.type == "text").strip()
        # Vollständigen Inhalt anhängen (inkl. Denk- und Fallback-Blöcken), nicht nur den Text.
        self.verlauf.append({"role": "assistant", "content": nachricht.content})
        self.ratschlaege.append(text)
        self._zusammenfassung = None
        return Antwort(text, abgeschnitten=nachricht.stop_reason == "max_tokens")
