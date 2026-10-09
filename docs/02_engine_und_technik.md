# 02 – Engine & Technik

## 1. Anforderungen

Grand-Strategy-Spiele stellen andere Anforderungen als die meisten Genres: wenig Grafiklast pro Objekt, aber **riesige Datenmengen**, **lange Spielzeiten**, **Determinismus** und **Modbarkeit**. Die Engine-Wahl folgt daraus.

| Anforderung | Warum | Technische Konsequenz |
|---|---|---|
| Detaillierte Karte (~4.500 Orte im Kernraum, ~650 Provinzen Europa) | Flickenteppich des Alten Reiches, Wahlkreise, Bundesländer | Shaderbasierter Kartenrenderer mit ID-Textur und Lookup-Tabellen |
| Hunderttausende Pop-Blöcke, tägliche Simulation | Victoria-artige Gesellschaftssimulation | Datenorientierter, multithreaded Simulationskern |
| Determinismus | Multiplayer im Lockstep, Replays, reproduzierbare Tests | Festkommaarithmetik, deterministische Zufallsströme, feste Reduktionsreihenfolge |
| Hunderte UI-Bildschirme, verschachtelte Tooltips | Paradox-typische Informationsdichte | Starkes UI-Framework mit Datenbindung und Themes |
| Modding auf Paradox-Niveau | Langlebigkeit, Community, spätere Nationen | Datengetriebene Inhalte, eigene Skriptsprache, Mod-Loader |
| Lebenszyklus ≥ 10 Jahre | Updates, DLC, weitere Nationen | Volle Kontrolle über den Quellcode, kein Lizenzrisiko |
| Headless-Betrieb | KI-Balancing, Massentests, dedizierter Host | Simulation strikt von Darstellung getrennt |
| Windows, Linux, macOS, Steam Deck | Zielgruppe | Plattformübergreifende Engine |
| Kleines bis mittleres Team | Budget | Produktive Werkzeuge, schnelle Iteration |

## 2. Engine-Optionen im Vergleich

| Option | Lizenz / Kosten | Stärken | Schwächen | Eignung |
|---|---|---|---|---|
| **Clausewitz / Jomini** (Paradox) | Proprietär, nicht lizenzierbar | Für genau dieses Genre gebaut | Nicht verfügbar | – |
| **Unreal Engine 5** | Royalty (derzeit 5 % des Bruttoumsatzes über 1 Mio. USD je Produkt) | Spitzen-3D, C++, ausgereifte Tools | Schwergewichtig; UI für datenlastige Strategie mühsam; lange Build- und Iterationszeiten; Overkill für eine Kartenansicht | ★★☆☆☆ |
| **Unity 6** | Abonnement je Arbeitsplatz ab Umsatzschwelle | C#, riesiger Talentpool, DOTS/ECS mit Burst für Simulation; erprobt bei Strategiespielen (z. B. *Humankind*, *Old World*, *Terra Invicta*) | Vertrauensverlust nach der Lizenzkrise 2023; Quellcodezugang kostenpflichtig; DOTS-Komplexität | ★★★★☆ |
| **Godot 4** | MIT-Lizenz, keine Gebühren, voller Quellcode | Leichtgewichtig, sehr gutes UI-System (Control-Nodes, Themes), GDExtension für nativen C++/Rust-Code, Vulkan-Renderer, schnelle Iteration; wachsende kommerzielle Nutzung (u. a. *Slay the Spire 2*) | 3D weniger ausgereift als Unreal/Unity (für eine Strategiekarte ausreichend); weniger Referenzen im Grand-Strategy-Genre | ★★★★★ (mit eigenem Kern) |
| **Eigene Engine** (C++ + bgfx/Vulkan + RmlUi) | Nur Entwicklungskosten | Maximale Kontrolle | Sehr hoher Aufwand für Editor, UI, Plattformen; Risiko für ein mittleres Team | ★★☆☆☆ |
| **Bevy** (Rust) | MIT/Apache | ECS-first, modern | Editor und UI unreif, API instabil | ★★☆☆☆ |

## 3. Empfehlung

### Godot 4 als Präsentationsschicht + eigener Simulationskern „Uhrwerk“ (C++20)

**Begründung**

1. **Keine Lizenzabhängigkeit.** Ein Spiel, das zehn Jahre lang Erweiterungen erhalten soll, darf nicht von Preis- oder Lizenzänderungen eines Engine-Herstellers abhängen. Die MIT-Lizenz und der vollständige Quellcode geben diese Sicherheit. Das Team friert intern einen Godot-Stand ein und pflegt bei Bedarf einen eigenen Fork.
2. **Das Herz ist ohnehin eigener Code.** Bei Grand Strategy liegt der Großteil der Komplexität in der Simulation (Pops, Märkte, Politik, KI, Krieg). Diese wird **engine-unabhängig** als C++-Bibliothek geschrieben. Die Engine liefert „nur“ Karte, UI, Audio, Eingabe und Plattformanbindung – genau das, was Godot gut kann.
3. **UI ist das halbe Spiel.** Godots Control-System mit Themes, Containern, Anker-Layouts und schneller Szenen-Iteration eignet sich hervorragend für hunderte informationsdichte Fenster.
4. **Headless und testbar.** Weil „Uhrwerk“ ohne Godot läuft, kann dieselbe Simulation in der Nacht tausendfach ohne Grafik durchlaufen (Balancing, Determinismustests, KI-Training) und als dedizierter Multiplayer-Host dienen.
5. **Austauschbarkeit.** Sollte Godot sich als Sackgasse erweisen, lässt sich die Präsentationsschicht ersetzen, ohne die Simulation neu zu schreiben.

**Alternative bei C#-starkem Team:** Unity 6 mit DOTS/Burst für die Simulation. Die Zwei-Schichten-Architektur bleibt identisch; nur Sprache und Präsentationsschicht wechseln.

**Alternative für den Kern:** Rust (über *godot-rust/gdext*) statt C++ – sicherer im Speicherumgang, kleinerer Talentpool. Entscheidung in der Vorproduktion nach Teamzusammensetzung.

## 4. Gesamtarchitektur

```
┌──────────────────────────────────────────────────────────────────┐
│  PRÄSENTATION (Godot 4)                                          │
│  Kartenrenderer · UI (Control/Themes) · Audio · Eingabe          │
│  Wahlabend · Zeitung · Chronik · Lexikon · Editor-Plugins        │
└──────────────▲─────────────────────────────────┬─────────────────┘
               │ Lese-Ansichten / Deltas          │ Befehle (Commands)
┌──────────────┴─────────────────────────────────▼─────────────────┐
│  BRÜCKE (GDExtension „Uhrwerk-Bridge“)                           │
│  Snapshot-Views · Befehlswarteschlange · Ereignisbus · Profiling │
└──────────────▲─────────────────────────────────┬─────────────────┘
               │                                  │
┌──────────────┴─────────────────────────────────▼─────────────────┐
│  SIMULATIONSKERN „Uhrwerk“ (C++20, headless-fähig)               │
│  ECS-Welt · Systeme (Demografie, Markt, Politik, Krieg, KI …)    │
│  Tick-Planer · Festkomma · Zufallsströme · Speicherstände        │
└──────────────▲───────────────────────────────────────────────────┘
               │
┌──────────────┴───────────────────────────────────────────────────┐
│  DATEN & SKRIPTE                                                 │
│  „Chronik-Skript“ (*.chron) · Lokalisierung · Karten · Mods      │
└──────────────────────────────────────────────────────────────────┘

     Weitere Nutzer des Kerns:  uhrwerk-cli (Tests, Balancing, Host)
```

**Grundregel:** Die Präsentation liest nur, sie verändert nie direkt. Jede Spieleraktion ist ein **Befehl** mit Tick-Nummer, der vom Kern ausgeführt wird. Dadurch sind Multiplayer, Replays und Undo-freie Determinismusprüfung automatisch möglich.

## 5. Der Simulationskern „Uhrwerk“

### 5.1 Datenmodell

- **ECS** (Entity-Component-System) auf Basis von **EnTT** oder **flecs** (beide MIT). Entitäten: Orte, Pops-Blöcke, Gebäude, Staaten, Länder, Parteien, Interessengruppen, Charaktere, Armeen, Flotten, Märkte, Kriege, Verträge.
- **Structure of Arrays** für Massendaten (Pop-Blöcke, Gebäude): cachefreundlich, vektorisierbar.
- **Pop-Blöcke** bündeln gleichartige Menschen eines Ortes (Beruf × Konfession × Milieu × Landsmannschaft × Altersgruppe). Kleine Blöcke werden zusammengeführt, große bei Bedarf geteilt. Zielgröße im Spätspiel: **250.000–600.000 aktive Blöcke**.
- **Räumliche Indizes** (Nachbarschaftsgraph der Orte, Flussnetz, Verkehrsnetz) als vorberechnete, kompakte Adjazenzlisten.

### 5.2 Zeitmodell

1 Tick = 1 Tag. Systeme laufen in unterschiedlichen Takten („Pulse“), die über die Tage verteilt werden, um Lastspitzen zu vermeiden:

| Puls | Systeme |
|---|---|
| **Täglich** | Truppenbewegung, Gefechte, Belagerungen, Diplomatie-Timer, Krisen-Eskalation, Wahlkampf-Tage |
| **Wöchentlich** | Marktpreise, Frontberechnung, Versorgungslage, Umfragen (moderne Epochen) |
| **Monatlich** | Produktion, Löhne, Bedürfnisse der Pops, Interessengruppen, Gesetzesfortschritt, Ereignisprüfungen, Haushalt |
| **Jährlich** | Demografie (Geburten, Sterbefälle, Altern), Statistisches Jahrbuch, Verwaltungsreichweite, Zeitgeist |
| **Kalendergesteuert** | Wahltermine, Reichstagssitzungen, Messen (Leipziger Oster- und Michaelismesse), Erntezeiten |

Monatliche Systeme werden **gestaffelt**: Jeder Staat/Ort hat einen festen „Rechentag“ im Monat. Das verteilt die Last gleichmäßig.

### 5.3 Determinismus

- **Keine Gleitkommazahlen in der Simulation.** Alle Werte als Festkomma (64 Bit, 32.32 bzw. 48.16 je nach Wertebereich).
- **Deterministische Zufallsströme** (z. B. PCG32/xoshiro256**) – je System und Entität ein eigener Strom, abgeleitet aus dem Partie-Seed. Neue Inhalte verändern dadurch nicht die Zufallsfolge anderer Systeme.
- **Parallelität mit fester Reduktionsreihenfolge:** Ergebnisse paralleler Jobs werden immer in derselben Reihenfolge zusammengeführt.
- **Prüfsummen** pro Tick (Multiplayer) bzw. pro Monat (Einzelspieler-Debug).

### 5.4 Parallelität

- Job-Graph (z. B. **Taskflow**, MIT): Jedes System deklariert, welche Komponenten es liest und schreibt; der Planer leitet daraus parallele Ausführung ab.
- Typische Parallelisierung: Pops und Gebäude nach Orten, Märkte nach Marktgebieten, KI nach Staaten.

### 5.5 Leistungsziele (Referenz-PC: 6 Kerne/12 Threads, 16 GB RAM)

| Messgröße | Ziel |
|---|---|
| Durchschnittlicher Tageslauf | ≤ 4 ms |
| Monatspuls (verteilt) | ≤ 80 ms gesamt |
| Ein Spieljahr auf Höchstgeschwindigkeit | ≤ 25 Sekunden (Frühphase), ≤ 45 Sekunden (Spätphase) |
| Arbeitsspeicher im Spätspiel | ≤ 6 GB |
| Spielstandgröße (komprimiert) | ≤ 100 MB im Spätspiel |
| Ladezeit Spielstand | ≤ 20 Sekunden |

### 5.6 Speicherstände

- Binärformat, komprimiert mit **zstd**, versioniertes Schema mit automatischen Migrationen zwischen Spielversionen.
- **Textexport** für Debugging und Modding (lesbar, diff-fähig).
- Autosave (monatlich/jährlich wählbar), **Ironman-Modus** für Erfolge, Steam Cloud.

## 6. Kartenrenderer

### 6.1 Grundprinzip

Bewährtes Verfahren der Grand-Strategy-Spiele:

1. **ID-Textur:** Jeder Ort hat eine eindeutige Farbe in einer großen Bitmap.
2. **Lookup-Texturen:** Für jeden Kartenmodus (politisch, Konfession, Wahlergebnis …) schreibt die Simulation eine kleine Tabelle „Ort → Farbe“.
3. **Fragment-Shader** färbt die Karte, zeichnet Grenzen (Signed-Distance-Field), Schattierungen und Muster (z. B. Schraffur für besetzte Gebiete, Streifen für Kondominien).

Ein Kartenmoduswechsel kostet damit nur das Hochladen einer kleinen Textur.

### 6.2 Auflösung und Daten

| Ebene | Auflösung | Abdeckung |
|---|---|---|
| Kernraum | 16.384 × 12.288 px (≈ 85 m pro Pixel) | Altes Reich nördlich der Alpen, Preußen, Schlesien, Böhmen, Österreich, Elsass-Lothringen, Grenzregionen |
| Europa | 8.192 × 4.096 px | Atlantik bis Ural, Nordkap bis Nordafrika |
| Welt-Schauplätze | Eigene Übersichtskarten | Kolonien, Ozeane, Fernfronten |

**Geodaten:** Höhenmodell aus dem Copernicus DEM, Landbedeckung aus CORINE (für die Gegenwart) mit historischen Korrekturen (trockengelegte Moore, Rodungen, Truppenübungsplätze, Tagebaue), Küstenlinien und Grenzen aus gemeinfreien und lizenzgeprüften Quellen (siehe [Anhang C](anhang/c_quellen_und_daten.md)).

### 6.3 Veränderliche Geographie

Die Karte ist nicht statisch:

- **Flüsse:** Die Rheinbegradigung (Tulla, ab 1817) ist ein Infrastrukturprojekt, das den Flusslauf tatsächlich verändert. Ebenso Kanäle, Deiche, Talsperren.
- **Küsten:** Landgewinnung an der Nordsee (Köge), Sturmfluten.
- **Landschaft:** Trockenlegung des Oderbruchs (1747–1753), Emslandkultivierung, Tagebaue in der Lausitz und im Rheinland, die ganze Orte verschlingen, später Rekultivierung und Seen.
- **Städte:** Wachsen sichtbar – vom Barockstädtchen zur Industriestadt zur Metropole.

### 6.4 Epochenstile („Die Karte altert mit“)

Shader-Parametersätze verändern das gesamte Erscheinungsbild:

| Stil | Epoche | Vorbild |
|---|---|---|
| **Kupferstich** | 1648–1789 | Handkolorierte Karten der Nürnberger Homann-Offizin |
| **Klassizismus** | 1789–1848 | Klare Linien, gedeckte Farben, Napoleonische Militärkarten |
| **Lithographie** | 1848–1914 | Stielers Handatlas (Justus Perthes, Gotha), Schraffuren für Gebirge |
| **Schulatlas** | 1914–1970 | Flächige Farben, Schummerung, Diercke-Stil |
| **Digital** | ab 1970 | Moderne, klare Vektorästhetik |

Der Spieler kann einen Stil auch dauerhaft festlegen.

### 6.5 Beschriftung und Namen

- Beschriftungen folgen Spline-Kurven entlang Regionen und Flüssen und skalieren mit dem Zoom.
- **Namensschichten:** Jeder Ort hat Namen je Epoche, Herrschaft und Sprache – z. B. *Chemnitz* / *Karl-Marx-Stadt* (1953–1990), *Königsberg* / *Kaliningrad*, *Breslau* / *Wrocław*, *Danzig* / *Gdańsk*, *Pressburg* / *Bratislava*. Optionen: historisch-zeitgenössisch, heutig, zweisprachig.

### 6.6 Darstellung von Einheiten

- Wahlweise 3D-Figuren (epochentypisch: Musketiere, Pickelhauben, Feldgrau, Flecktarn) oder **taktische Symbole** im NATO-Stil.
- Frontlinien als animierte, sich verdichtende Linienzüge.

## 7. Daten und Skriptsprache „Chronik-Skript“

### 7.1 Prinzipien

- **Deklarativ** und an die Syntax der Paradox-Skripte angelehnt, damit erfahrene Modder sich sofort zurechtfinden.
- **Englische Schlüsselwörter** (`trigger`, `effect`, `modifier`) für die internationale Modding-Szene; Bezeichner und Kommentare dürfen deutsch sein.
- **Kompiliert** beim Laden zu Bytecode, mit statischer Typprüfung (falscher Scope, unbekannter Effekt → Fehlermeldung mit Datei und Zeile).
- **Hot Reload** im Entwicklermodus.
- Jede historische Tatsache kann ein Feld `source = { … }` tragen, das im Lexikon angezeigt wird.

### 7.2 Beispiel: eine Gesetzesgruppe

```text
# gesetze/wahlrecht.chron
law_group wahlrecht {
    category = politik
    laws = {
        wahlrecht_keines
        wahlrecht_zensus
        wahlrecht_dreiklassen
        wahlrecht_allgemein_maenner
        wahlrecht_allgemein
        wahlrecht_einheitsliste
    }
}

law wahlrecht_dreiklassen {
    group  = wahlrecht
    unlock = { innovation = konstitutionalismus }

    voters = {
        sex       = male
        min_age   = 24
        weighting = tax_thirds          # Stimmgewicht nach Steuerdritteln
        ballot    = public              # öffentliche Stimmabgabe
        indirect  = yes                 # Wahl über Wahlmänner
    }

    modifier = {
        state_legitimacy_from_elections = 0.25
        ig_grossgrundbesitz_clout_mult  = 0.10
        ig_arbeiterbewegung_approval    = -3
    }

    source = { text = "Preußische Verordnung vom 30. Mai 1849" }
}
```

### 7.3 Scopes

`world`, `state` (Staat), `land` (Land im Bund), `location` (Ort), `district` (Kreis), `region`, `pop`, `character`, `party`, `ig` (Interessengruppe), `market`, `army`, `fleet`, `war`, `treaty`, `election`, `constitution`.

### 7.4 Lokalisierung

- Texte in Schlüssel-Dateien (YAML), getrennt nach Sprache; **ICU MessageFormat** für Plural, Genus und Formen („die Ministerin“/„der Minister“).
- Export ins PO-Format für Übersetzungsdienstleister.
- Fraktur, Antiqua und moderne Schriften als **Stilebene**, nie als Pflicht – jeder Text ist in einer gut lesbaren Schrift verfügbar.

## 8. Werkzeuge

| Werkzeug | Zweck |
|---|---|
| **Kartentisch** (Godot-Editor-Plugin) | Orte zeichnen, Gelände, Flüsse, Grenzen je Datum, Namensschichten, Wahlkreise |
| **Archivar** (Datenbank-Werkzeug) | Import historischer Personen, Ämter und Daten (z. B. aus Wikidata) → Prüfwarteschlange für Historiker → Export als Skriptdateien mit Quellenangabe |
| **Regie** (Ereigniseditor) | Knotenbasierte Ereignisketten, Vorschau, Validierung, Lokalisierungsstatus |
| **Prüfstand** (Balancing-Labor) | Nächtliche Headless-Läufe (z. B. 1.000 Partien je Lesezeichen), Auswertung in Dashboards: Wann einigt sich Deutschland? Wie oft scheitert Weimar? Wie hoch ist die Inflation 1923? |
| **Wahllabor** | Wahlsimulation gegen historische Ergebnisse kalibrieren (siehe [10 Wahlen](10_parlamente_wahlen_koalitionen.md)) |
| **Lokalisierungspipeline** | Schlüsselprüfung, Längenwarnungen, Übersetzungsstatus |

## 9. Qualitätssicherung und Build

- **Unit-Tests** im Kern (Märkte, Wahlsysteme, Sitzzuteilung nach D'Hondt, Hare/Niemeyer, Sainte-Laguë).
- **Determinismus-Goldtests:** Fester Seed → Simulation 1648–1700 → Prüfsumme muss mit dem Referenzwert übereinstimmen.
- **Plausibilitätstests:** Statistische Korridore aus dem Prüfstand (z. B. „Im historischen Modus einigt sich Deutschland in ≥ 60 % der Läufe zwischen 1860 und 1880“).
- **Leistungsregression:** Jeder Build misst Tageslauf, Monatspuls, Speicher.
- **UI-Rauchtests:** Automatisches Öffnen aller Fenster in allen Epochen.
- **CI/CD:** Builds für Windows, Linux und macOS bei jedem Commit, nächtliche Langläufe.
- **Absturzberichte** mit Symbolserver; **Telemetrie** nur mit ausdrücklicher Zustimmung.

## 10. Multiplayer-Netzcode

- **Deterministischer Lockstep:** Nur Befehle werden übertragen, nie der Spielzustand.
- Eingabeverzögerung von wenigen Ticks; der Host bestimmt den Takt.
- **Asynchronitätserkennung** über Prüfsummen, automatische Resynchronisation durch Zustandsübertragung.
- **Hot-Join** und Wiederverbinden.
- Transport über **Steam Networking Sockets** (Relays, NAT-Traversal) bzw. die quelloffene Bibliothek *GameNetworkingSockets*; direkte IP-Verbindung optional.
- **Dedizierter Host:** `uhrwerk-cli` kann Partien ohne Grafik hosten.

Details zu Spielmodi in [23 Multiplayer & Modding](23_multiplayer_und_modding.md).

## 11. Plattformen und Systemanforderungen (vorläufig)

| | Minimum | Empfohlen |
|---|---|---|
| Betriebssystem | Windows 10 64 Bit, Ubuntu 22.04, macOS 12 | Windows 11, aktuelle Linux-Distribution, macOS 14 |
| Prozessor | 4 Kerne (z. B. Intel Core i5-6600 / AMD Ryzen 5 1500X) | 6 Kerne/12 Threads (z. B. Core i5-12400 / Ryzen 5 5600) |
| Arbeitsspeicher | 8 GB | 16 GB |
| Grafik | Vulkan-1.2-fähig, 2 GB VRAM (z. B. GTX 960 / RX 470); Kompatibilitätsrenderer für ältere Systeme | 6 GB VRAM (z. B. GTX 1660 / RX 5600) |
| Speicher | 15 GB, SSD empfohlen | 20 GB SSD |
| Steam Deck | Ziel: „Spielbar“, mit Controller-Layout und skalierbarer UI | – |

## 12. Technische Risiken

| Risiko | Wahrscheinlichkeit | Auswirkung | Gegenmaßnahme |
|---|---|---|---|
| Simulationsleistung im Spätspiel | mittel | hoch | Früher Leistungsprototyp, Pop-Block-Zusammenführung, Profiling ab Woche 1 |
| Desyncs im Multiplayer | mittel | hoch | Festkomma ab Tag 1, Goldtests, Desync-Werkzeug mit Zustandsdiff |
| Godot-Grenzen im Kartenrendering | niedrig | mittel | Renderer als eigenes Modul, im Notfall nativer Vulkan-Pfad über GDExtension |
| Mod-Sicherheit (Skripte in UI-Mods) | mittel | mittel | UI-Mods standardmäßig deklarativ (Szenen, Themes); Skript-Mods nur mit ausdrücklicher Warnung und Freigabe |
| Datenmenge/Datenqualität historischer Fakten | hoch | mittel | Archivar-Werkzeug mit Quellenpflicht und Historiker-Review |
| Speicherstand-Kompatibilität über Jahre | mittel | mittel | Versionierte Schemata mit Migrationstests |
