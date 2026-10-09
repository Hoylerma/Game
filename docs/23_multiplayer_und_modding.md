# 23 – Multiplayer & Modding

## 1. Multiplayer-Modi

| Modus | Spieler | Startdaten | Beschreibung |
|---|---|---|---|
| **Klassisch** | 2–16 | alle | Jeder Spieler führt einen Staat |
| **Reichstag** | 4–16 | 1648, 1701, 1740 | Spieler als Reichsstände: Kaiserwahl unter Spielern, Reichstagsabstimmungen, Reichskriege, Konfessionsblöcke |
| **Die deutsche Frage** | 3–10 | 1815, 1848, 1862 | Preußen, Österreich, Mittelstaaten und Kleinstaaten ringen um die Einigung |
| **Parteienmodus** | 3–8 | 1848, 1871, 1919, 1930, 1949, 1969, 2025 | Spieler führen Parteien: Wahlkampf, Koalitionen, Regierung und Opposition (siehe Abschnitt 2) |
| **16 Länder** | 2–17 | 1949, 1990, 2025 | Jeder Spieler ein Bundesland (optional ein Spieler als Bundesregierung): Bundesrat, Finanzausgleich, Ministerpräsidentenkonferenz, Kanzlerambitionen |
| **Systemkonkurrenz** | 2–8 | 1949, 1961, 1989 | BRD gegen DDR, optional mit Ländern bzw. Oppositionsgruppen |
| **Kabinett** (Koop) | 2–6 | alle | Mehrere Spieler teilen sich einen Staat; jeder führt Ressorts (Finanzen, Äußeres, Inneres, Militär …), der Regierungschef hat die Richtlinienkompetenz |
| **Herausforderungen** | 1 (asynchron) | Sonderszenarien | Ranglisten nach Vermächtnis-Wertung („Rette Weimar“, „Einheit ohne Krieg 1866“, „Wirtschaftswunder“) |

## 2. Parteienmodus im Detail

| Element | Regel |
|---|---|
| **Wer steuert den Staat?** | Die Regierungsparteien gemeinsam: Jede Partei steuert die Ressorts, die ihr laut Koalitionsvertrag zustehen; die Partei des Regierungschefs bestimmt die Richtlinien und hat Vorrang bei Konflikten |
| **Opposition** | Anträge, Untersuchungsausschüsse, Verfassungsklagen, Kampagnen, Bundesrat über eigene Landesregierungen, Volksbegehren, Vorfeldorganisationen |
| **Wahlen** | Wahlkampfbudgets, Spitzenkandidaten, Themen; Wahlabend für alle |
| **Koalitionsverhandlungen** | Echte Verhandlungen zwischen Spielern mit Verhandlungsbildschirm und bindendem Koalitionsvertrag |
| **Siegbedingungen** | Keine; Wertung nach Regierungszeit, umgesetzten Programmpunkten, Mitgliederentwicklung und – gemeinsam – dem Vermächtnis des Staates (die Demokratie zu verlieren, ist für alle eine Niederlage) |
| **Nicht spielbare Parteien** | Siehe [11 Parteien](11_parteien_datenbank.md), Abschnitt 1.4 |

## 3. Sitzungsfunktionen

| Funktion | Beschreibung |
|---|---|
| **Geschwindigkeit** | Host bestimmt oder Abstimmung; automatische Pause bei Wendepunkten (abschaltbar) |
| **Pausenabstimmung** | Jeder Spieler kann eine begrenzte Zahl von Pausen anfordern |
| **Hot-Join und Wiederverbinden** | Beitritt in laufende Partien; KI übernimmt bei Verbindungsabbruch |
| **Verhandlungsraum** | Strukturierte Angebote zwischen Spielern (Gebiete, Geld, Verträge, Koalitionsposten) |
| **Spielleitung** | Optionale Rolle für Community-Events: Ereignisse auslösen, Szenarien steuern |
| **Speichern** | Gemeinsame Spielstände, Cloud-Speicher, Wiederaufnahme mit anderen Spielern |

**Technik:** Deterministischer Lockstep, Asynchronitätserkennung und Resynchronisation, dedizierter Host ohne Grafik möglich (siehe [02 Engine](02_engine_und_technik.md)).

## 4. Modding

### 4.1 Grundsatz

Alles, was Inhalt ist, ist modbar: Karte, Staaten, Persönlichkeiten, Parteien, Gesetze, Güter, Gebäude, Innovationen, Ereignisse, Missionen, Journale, Oberfläche, Musik, Lokalisierung. Mit „Erstmal nur Deutschland“ ist das Basisspiel bewusst fokussiert – Mods (und spätere offizielle Pakete) können weitere Nationen hinzufügen.

### 4.2 Ordnerstruktur eines Mods

```text
meine_mod/
├── descriptor.chron          # Name, Version, Abhängigkeiten, Kompatibilität
├── common/
│   ├── gesetze/              # Gesetzesgruppen und Gesetze
│   ├── gueter/               # Güter
│   ├── gebaeude/             # Gebäude und Produktionsmethoden
│   ├── innovationen/
│   ├── interessengruppen/
│   ├── parteien/
│   ├── persoenlichkeiten/
│   ├── modifikatoren/
│   └── on_actions/           # Einhängepunkte („bei Wahl“, „bei Thronfolge“ …)
├── events/                   # Ereignisse
├── decisions/                # Entscheidungen
├── journals/                 # Journale
├── missions/                 # Missionsbäume
├── history/                  # Startdaten: Staaten, Orte, Pops, Personen je Datum
├── map/                      # ID-Karte, Höhen, Gelände, Flüsse, Namen, Wahlkreise
├── localization/
│   ├── de/
│   └── en/
├── gfx/                      # Bilder, Porträts, Wappen
├── music/
└── interface/                # Godot-Szenen und Themes (deklarativ)
```

### 4.3 Beispiel: Eine kleine Mod „Hansebund“

```text
# common/formables/hansebund.chron
formable hansebund {
    name   = hansebund.name                 # „Hanseatischer Bund“
    flag   = gfx/flags/hansebund.png
    allowed_from = 1648.10.24
    potential = {
        government_type = stadtrepublik
        has_trait = hansestadt
    }
    requirements = {
        controls_locations = { luebeck hamburg bremen }
        alliance_with_all = { state:luebeck state:hamburg state:bremen }
        prestige >= 40
    }
    effect = {
        form_federation = {
            members = { state:luebeck state:hamburg state:bremen }
            type    = staedtebund
        }
        add_modifier = { name = hanse_erneuert months = -1 }   # dauerhaft
        trigger_event = hanse_mod.1
    }
}
```

### 4.4 Werkzeuge für Modder

| Werkzeug | Funktion |
|---|---|
| **Kartentisch** | Karteneditor als Godot-Plugin (Orte, Gelände, Grenzen je Datum, Namen, Wahlkreise) |
| **Regie** | Knotenbasierter Ereigniseditor mit Vorschau |
| **Archivar** | Datenbankwerkzeug für Persönlichkeiten, Parteien, Ämter mit Quellenfeldern |
| **Szenario-Editor** | Eigene Lesezeichen erstellen; **freies Startdatum** (Beta) aus den datierten Geschichtsdateien |
| **Validator** | Statische Prüfung aller Skripte mit Fehlermeldungen (Datei, Zeile, Ursache) |
| **Konsole und Profiler** | Befehle, Zustandsabfragen, Laufzeitmessung je Skript |
| **Dokumentation** | Automatisch erzeugte Referenz aller Trigger, Effekte, Scopes und Modifikatoren |

### 4.5 Kompatibilität

- **Ladereihenfolge** und gezieltes Überschreiben einzelner Objekte (statt ganzer Dateien), um Konflikte zu minimieren.
- **Prüfsummen** für Multiplayer und Erfolge (Ironman nur mit kompatiblen, als „spielmechanisch neutral“ markierten Mods).
- **Versionierung:** Mods geben die unterstützte Spielversion an; das Spiel warnt bei Abweichungen.

### 4.6 Sicherheit

- **Chronik-Skript** ist eine abgeschlossene Inhaltssprache ohne Datei- oder Netzwerkzugriff.
- **Oberflächen-Mods** sind standardmäßig deklarativ (Szenen, Themes, Layouts).
- **Skriptbasierte Oberflächen-Mods** (GDScript) gelten als „erweiterte Mods“: Sie werden beim Laden deutlich gekennzeichnet und müssen ausdrücklich freigegeben werden.

### 4.7 Verbreitung

- **Steam Workshop** sowie eine plattformübergreifende Mod-Plattform für andere Vertriebswege.
- **Offizielle Unterstützung** großer Mods (Gesamtkonversionen für andere Länder): Dokumentation, Entwicklerkontakt, Vorstellung in Entwicklertagebüchern.

### 4.8 Regeln für den Workshop

Nicht erlaubt (Entfernung und Sperrung):

- Verherrlichung oder Verharmlosung von Nationalsozialismus, Völkermord, Kriegsverbrechen oder Terrorismus,
- verbotene Kennzeichen (§ 86a StGB) und ihre Abwandlungen,
- Leugnung des Holocaust oder anderer Völkermorde,
- Mechaniken, die Massenverbrechen spielbar machen,
- Hetze gegen Gruppen,
- herabwürdigende Darstellungen realer lebender Personen.

Mods mit ernsthafter historischer Absicht zu schwierigen Themen sind möglich, müssen sich aber an die Leitlinien in [20 Verantwortung](20_verantwortung_und_erinnerungskultur.md) halten. Eine **Meldefunktion** und ein Moderationsteam setzen die Regeln durch.
