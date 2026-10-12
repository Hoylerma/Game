# Speicherstände und Wiederholungen (Meilenstein M6)

> **Status: Entwurf** zur Freigabe vor der Umsetzung
> ([meilensteine.md](../meilensteine.md#m6--speicherstände-und-wiederholungen)). Grundlage sind
> [02 Engine und Technik](../02_engine_und_technik.md), Abschnitte 4, 5.3, 5.5, 5.6 und 10 (Befehle
> mit Tick-Nummer, Determinismus, Ladezeit und Größe, Binärformat mit zstd, versioniertes Schema,
> Textexport, Autosave, Lockstep), und [23 Multiplayer und Modding](../23_multiplayer_und_modding.md),
> Abschnitt 3.

Eine Partie soll sich speichern, laden und **bitgleich** fortsetzen lassen, und aus Startzustand und
Befehlen soll sich jede Partie exakt wiederholen lassen. Beides ist die Grundlage für Multiplayer
im Lockstep (nach der Vorproduktion: nur Befehle werden übertragen), für Fehlerberichte („Spielstand und Protokoll anhängen“) und für die
Fehlersuche bei Determinismusfehlern.

## 1. Umfang

| Bereich | in M6 | später |
|---|---|---|
| Spielstand | Binärdatei mit Kopf, lesbaren Metadaten und komprimierten Nutzdaten (zstd); Formatversion | Ironman (nur ein Spielstand, Speichern beim Beenden), Steam Cloud |
| Inhalt | Ganzer veränderlicher Zustand aus einem **Zustandsverzeichnis**, das auch die Prüfsumme bildet; Inhaltsfingerabdruck der Spieldaten | Laden trotz geänderter Spieldaten (Zuordnung über Kennungen) |
| Versionen | Formatversion im Kopf, Lesen älterer Versionen über Versionsschalter im Verzeichnis, neuere werden abgelehnt | Migrationen zwischen veröffentlichten Spielversionen |
| Befehlsprotokoll | Jeder ausgeführte Befehl mit Tag; Protokolldatei mit Kontrollpunkten; jeder Spielstand enthält das Protokoll seit Partiebeginn | Übertragung im Lockstep, Asynchronitätserkennung |
| Werkzeuge | `uhrwerk-cli`: Speichern, Laden, Wiederholung, Speichertest, Spielstandinfo, **Textexport** (nur lesen) | Textimport (Szenario aus Spielstand) |
| Godot | Spielmenü mit Speichern und Laden, Schnellspeichern (F5) und Schnellladen (F9), jährlicher Autosave | Einstellungen, Ironman, Cloud |

## 2. Grundprinzip: ein Zustandsverzeichnis

Heute zählt `Simulation::checksum()` den veränderlichen Zustand Feld für Feld auf (Datum, Staaten,
Orte, Blöcke, Zugänge, Momentaufnahmen, Bilanzen, Wirtschaft, Märkte, Zufallsströme, Chronik,
wartende Befehle). Genau diese Liste ist auch das, was ein Spielstand enthalten muss. M6 macht
daraus **eine** Funktion:

```cpp
template <typename Visitor> void Simulation::visit_state(Visitor& v);  // eine Liste für alles
```

Der Besucher bekommt jedes Feld mit Namen, z. B. `v("staaten.schatz", s.treasury)`, und es gibt
vier Besucher:

| Besucher | Zweck |
|---|---|
| `HashVisitor` | Prüfsumme (FNV-1a-64, wie bisher) |
| `WriteVisitor` | Spielstand schreiben |
| `ReadVisitor` | Spielstand lesen (in eine Welt mit demselben Inhalt) |
| `TextVisitor` | Textexport: eine Zeile je Feld, `pfad = wert`, diff-fähig |

Damit kann nichts gespeichert werden, was nicht in der Prüfsumme steht, und umgekehrt. Ein Feld,
das künftig hinzukommt (z. B. der Herrscher, sobald er wechseln kann), steht an einer Stelle und
gilt sofort für alle vier Zwecke. Die Reihenfolge und Kodierung der Felder bleiben wie bisher, die
**Referenzprüfsummen ändern sich dadurch nicht** (der erste Umsetzungsschritt prüft das).

**Was nicht dazugehört:** unveränderlicher Inhalt (Namen, Gelände, Lage, Fläche, Kreise, Regeln,
Güter, Betriebe, Märkte), abgeleitete Hilfsdaten (Rechentag-Listen, Wanderungsziele,
Markttopologie, Raten) und die Zahl der Threads. Sie entstehen beim Laden aus den Spieldaten neu.

**Befehlsprotokoll:** Die Simulation führt zusätzlich eine Liste aller ausgeführten Befehle mit dem
Tag, an dem sie ausgeführt wurden (`days_elapsed`), auch der verworfenen. Es ist kein
Simulationszustand (es beeinflusst die Zukunft nicht) und gehört nicht zur Prüfsumme, wird aber
mitgespeichert.

## 3. Inhalt und Inhaltsfingerabdruck

Ein Spielstand enthält nur den Zustand. Beim Laden lädt der Kern zuerst den Inhalt des Szenarios
aus dem Datenordner (wie beim Spielstart) und überschreibt dann den Zustand. Damit das passt, trägt
der Spielstand einen **Inhaltsfingerabdruck**: FNV-1a-64 über alles am Inhalt, was die Simulation
beeinflusst – alle Regelwerte; Kennungen und Zahlen von Gelände, Konfessionen, Kulturen, Berufen,
Gütern, Bedürfnissen, Betrieben, Märkten und Vorkommen; Kennungen der Staaten und Kreise; je Ort
Kennung, Gelände, Lage, Fläche, Stadt, Kreis, Markt, Vorkommen und Hafen; die Zahl der Orte.
**Nicht** dazu gehören Namen, Farben, Quellen und Beschreibungen: Eine korrigierte Quelle oder ein
umbenannter Ort macht alte Spielstände nicht unbrauchbar.

Passt der Fingerabdruck nicht, lehnt der Kern den Spielstand mit einer klaren Meldung ab („Der
Spielstand passt nicht zu den Spieldaten: Regeln und Güter wurden geändert.“ – dazu je
Inhaltsbereich ein eigener Teilfingerabdruck). Vor Version 1.0 ändern sich die Spieldaten mit jedem
Meilenstein; alte Spielstände laufen dann nicht mehr. Das Laden trotz geänderter Daten (Zuordnung
über Kennungen statt Indizes, neue Felder mit Standardwerten) ist eine spätere Ausbaustufe
(Abschnitt 11, Entscheidung 2).

**Gegenprobe beim Laden:** Der Spielstand enthält die Prüfsumme des gespeicherten Zustands. Nach
dem Laden rechnet der Kern sie nach; weicht sie ab, ist der Spielstand beschädigt oder unvollständig
gelesen, und das Laden schlägt fehl.

## 4. Dateiformat (Version 1)

Endung `.einigkeit`. Alle Zahlen little-endian mit fester Breite, unabhängig von Plattform und
Compiler; kein `memcpy` von Strukturen.

```
Kopf (fest, 48 Byte)
  Magie            8 Byte  "EINIGKT\x1A"
  Formatversion    u32     1
  Kennzeichen      u32     Bit 0: Nutzdaten mit zstd komprimiert
  Metadaten        u32     Länge in Byte
  reserviert       u32     0
  Nutzdaten roh    u64     Länge unkomprimiert
  Nutzdaten        u64     Länge in der Datei
  Prüfwert         u64     FNV-1a-64 über die unkomprimierten Nutzdaten
Metadaten (UTF-8-Text, unkomprimiert, eine Zeile je Angabe "schluessel=wert")
  spiel=0.1.0  szenario=westfaelischer_frieden  datum=1673-05-02  spielerstaat=BRA
  seed=1648  tage=8956  pruefsumme=5db13a66d63e6079  inhalt=3f…  formatversion=1
Nutzdaten (zstd, Stufe 3)
  Abschnitt ZUSTAND    alle Felder aus visit_state in fester Reihenfolge
  Abschnitt PROTOKOLL  Startprüfsumme, Zahl der Befehle, je Befehl: Tag, Art, Staat, Wert, Steuerart
```

Die Metadaten lassen sich lesen, ohne die Nutzdaten zu entpacken (Liste der Spielstände in Godot).
Kodierung im Zustand: Festkomma als `i64` (Rohwert), Indizes und Zähler als `i64`, Längen von
Vektoren als `u64` vor den Elementen, Zeichenketten als Länge plus UTF-8, Datum als drei Zahlen –
also genau das, was die Prüfsumme heute einliest.

**Gleicher Zustand, gleiche Datei:** Ein Spielstand enthält keine Uhrzeit und keinen Rechnernamen
(Godot zeigt das Änderungsdatum der Datei). Speichern → Laden → Speichern ergibt dieselben Bytes;
derselbe Lauf ergibt unter GCC, Clang und MSVC denselben Prüfwert der Nutzdaten.

**Größe und Zeit (Schätzung):** Testregion (543 Blöcke, 60 Orte mit Daten, 4.542 Orte) unter 1 MB
roh. Lasttest-Welt mit 500.000 Blöcken: 32 MB Blöcke plus etwa 3 MB Wirtschaft roh, mit zstd
geschätzt 5–10 MB; Schreiben und Lesen je unter 1 s. Ziel aus Kapitel 02: höchstens 100 MB und
20 s Ladezeit im Spätspiel.

## 5. Befehlsprotokoll und Wiederholung

Eine **Wiederholung** ist Startzustand plus Befehle: Szenario, Seed und Inhaltsfingerabdruck
bestimmen den Startzustand; das Protokoll sagt, welcher Befehl an welchem Tag ausgeführt wurde. Der
Kern lädt das Szenario neu, reiht jeden Befehl vor seinem Tag ein und rechnet weiter.

**Protokolldatei** (Endung `.protokoll`, gleicher Kopf mit eigener Magie `EINIGPR\x1A`):
Metadaten wie oben plus Startprüfsumme und Endtag, Nutzdaten = Befehle plus **Kontrollpunkte**
(Prüfsumme am 1. Januar jedes Jahres). Beim Wiederholen meldet der Kern den ersten abweichenden
Kontrollpunkt – damit ist ein Determinismusfehler auf ein Jahr eingegrenzt; der Textexport zweier
Spielstände aus diesem Jahr zeigt dann das abweichende Feld.

Jeder Spielstand enthält das Protokoll seit Partiebeginn. `uhrwerk-cli wiederholung spiel.einigkeit`
rechnet die Partie vom Start nach und vergleicht mit dem gespeicherten Zustand.

**Testbefehle:** Die Kommandozeile hat bisher keine Spielerbefehle. Für Tests erzeugt
`--testbefehle N` N Steuerbefehle (gültige und ungültige, alle drei Steuerarten, zufällige Staaten
und Tage) aus einem eigenen Zufallsstrom `Testbefehle = 6`, der die Ströme des Spiels nicht berührt.

## 6. Versionen und Migration

- Die **Formatversion** steht im Kopf. Der Lesebesucher kennt die Version und kann Felder je
  Version überspringen oder mit Standardwerten füllen (`if (v.version() >= 2) v("…", neues_feld)`).
- **Neuere** Versionen lehnt der Kern ab („Spielstand aus einer neueren Spielversion“).
- Jede Änderung am Zustand (neues Feld, neue Reihenfolge) erhöht die Formatversion. Ein Test
  verhindert, dass das vergessen wird: Er speichert die Test-Fixture nach einem Jahr und vergleicht
  den Prüfwert der Nutzdaten mit einem festgehaltenen Wert. Ändert er sich, muss entweder die
  Formatversion steigen oder (bei geänderten Testdaten) der Referenzwert neu festgehalten werden;
  die Fehlermeldung des Tests sagt beides.
- Weil sich vor 1.0 mit fast jedem Meilenstein auch der Inhalt ändert, laufen alte Spielstände
  meist ohnehin nicht mehr (Abschnitt 3). Echte Migrationen über Spielversionen hinweg werden mit
  der ersten öffentlichen Version wichtig; das Gerüst (Version im Kopf, Versionsschalter im
  Verzeichnis, Ablehnung neuerer Versionen) entsteht jetzt.

## 7. Kern-Schnittstelle (`uhrwerk/savegame.hpp`)

```cpp
struct SaveMeta { std::string player_tag; };              // vom Aufrufer (Godot kennt den Spieler)
std::vector<std::uint8_t> save_game(const Simulation&, const SaveMeta&);
struct SaveInfo { /* Metadaten aus dem Kopf, ohne Entpacken */ };
std::optional<SaveInfo> read_save_info(std::span<const std::uint8_t>, std::string& error);
struct LoadedGame { std::optional<Simulation> simulation; SaveInfo info; std::string error; };
LoadedGame load_game(std::span<const std::uint8_t>, World content, unsigned threads);
std::string export_text(const Simulation&);                 // Textexport
std::uint64_t content_fingerprint(const World&);
```

`load_game` bekommt den frisch geladenen Inhalt (`load_world(daten, szenario)`) und gibt die
fertige Simulation zurück: Zustand gelesen, abgeleitete Hilfsdaten neu aufgebaut, Prüfsumme
nachgerechnet. Die Lasttest-Welt (synthetisch, nicht aus dem Datenordner) lässt sich so ebenfalls
speichern und laden. Dazu kommen in der Simulation `command_log()` und ein Konstruktor für den
geladenen Zustand (Zufallsströme, Tage, wartende Befehle, Protokoll).

## 8. Kommandozeile

| Aufruf | Zweck |
|---|---|
| `lauf --jahre N [--testbefehle K] --speichern DATEI` | Lauf, am Ende speichern |
| `lauf --laden DATEI --jahre N` | Spielstand laden und weiterrechnen (Prüfsumme am Ende) |
| `lauf … --protokoll DATEI` | Befehle und jährliche Kontrollpunkte schreiben |
| `wiederholung DATEI` | Protokoll oder Spielstand vom Start nachrechnen; Exitcode 1 bei Abweichung, mit erstem abweichendem Jahr |
| `speichertest [--jahre N] [--testbefehle K] [--punkte P]` | Abnahmetest: Lauf A ohne Unterbrechung; Lauf B speichert an P Stellen (auch mitten im Monat, mit wartenden Befehlen und Zugängen) in eine Datei, lädt sie mit frisch geladenem Inhalt und rechnet weiter; jährliche Prüfsummen müssen gleich sein |
| `spielstand DATEI [--text]` | Metadaten, Größe, Kompatibilität; mit `--text` der Textexport |
| `lasttest … --speichern` | zusätzlich Größe sowie Schreib- und Ladezeit der Lasttest-Welt |

## 9. Brücke und Godot

**Brücke** ([bridge-api.md](bridge-api.md)):
- `save_game(path, player_state_index) -> String` (leer bei Erfolg, sonst Meldung)
- `load_game(data_path, path) -> String` (ersetzt die laufende Simulation nur bei Erfolg)
- `get_save_info(path) -> Dictionary`: Szenario, Datum als Text, Spielerstaat (Kennung und Name),
  Formatversion, `compatible` (bool) und `reason`, ohne zu laden
- `get_save_format_version() -> int`

**Godot:**
- **Spielmenü** über einen Knopf in der oberen Leiste (und Taste F10): Speichern (Name vorbelegt mit
  Staat und Datum), Laden (Liste der Spielstände in `user://spielstaende/` mit Datum, Staat und
  Änderungszeit, unpassende ausgegraut mit Grund), Schnellspeichern, Schnellladen. Die Zeit hält an,
  solange das Menü offen ist.
- **F5** speichert in `schnellspeicher.einigkeit`, **F9** lädt ihn.
- **Autosave** am 1. Januar jedes Jahres, die letzten drei bleiben (`autosave_1.einigkeit` …).
- Nach dem Laden bauen Karte, Panels, Legende und Chronik neu auf; die Auswahl wird aufgehoben.

## 10. Tests und Abnahme

| Kriterium aus dem Meilensteinplan | Prüfung |
|---|---|
| Speichern → Laden → Weiterspielen ergibt dieselbe Prüfsumme wie ohne Unterbrechung | Unit-Tests (Speichern an vielen Tagen der Fixture, auch mit wartenden Befehlen und Zugängen, 1 und 4 Threads); CTest `speichertest_50_jahre` mit den echten Daten über die Datei; Godot-Rauchtest über die Brücke |
| Wiederholung aus Befehlsprotokoll ist bitgleich (CI-Test über 50 Jahre) | CTest `wiederholung_50_jahre`: `lauf --jahre 50 --testbefehle 200 --protokoll` und `wiederholung`, in allen drei Compiler-Jobs |

Weitere Tests:
- Zustandsverzeichnis: Referenzprüfsummen unverändert; Textexport enthält jedes Feld.
- Format: Speichern → Laden → Speichern ergibt dieselben Bytes; Kopf, Metadaten und Prüfwert;
  abgelehnt werden falsche Magie, neuere Version, abgeschnittene Datei, verfälschtes Byte,
  falscher Inhaltsfingerabdruck (mit Meldung je Bereich) und eine falsche Gegenprobe.
- Plattformen: Der Determinismus-Job vergleicht zusätzlich den Prüfwert der Nutzdaten eines
  Spielstands nach 100 Jahren unter GCC, Clang und MSVC – gleiche Bytes heißt: jede Plattform liest
  die Spielstände der anderen.
- Formatwächter: festgehaltener Prüfwert der Nutzdaten für die Fixture (Abschnitt 6).
- Lasttest: Größe und Zeiten der Lasttest-Welt in der Zusammenfassung des CI-Jobs.
- Godot: Rauchtest (Speichern, Info, Laden, Prüfsumme, Fehlerfälle), Klicktest (Spielmenü,
  Schnellspeichern und -laden), Einheitentest (Dateinamen, Rotation des Autosaves).

## 11. Offene Entscheidungen

1. **Kompression mit zstd schon in M6?** Empfehlung: **ja.** Kapitel 02 sieht zstd vor; zstd 1.5.6
   (BSD-Lizenz) kommt wie doctest per CMake `FetchContent` mit fester Version und wird statisch
   gelinkt. Die Testregion bräuchte es noch nicht, die Lasttest-Welt schon (32 MB roh). Alternative:
   ohne Kompression starten; das Kennzeichen im Kopf ist vorgesehen.
2. **Spielstände bei geänderten Spieldaten:** Empfehlung: **ablehnen** (Fingerabdruck muss passen)
   und das Laden über Kennungen erst einführen, wenn es Spieler mit langen Partien gibt (spätestens
   vor einer öffentlichen Version). Alternative jetzt: Zustand nach Kennungen statt Indizes
   speichern – deutlich aufwendiger (alle Verweise in Blöcken und Listen umschlüsseln).
3. **Spielmenü:** Knopf in der oberen Leiste plus F10 (Escape hebt weiterhin die Auswahl auf).
   Alternative: Escape öffnet das Menü, wenn nichts ausgewählt ist.
4. **Autosave:** jährlich, drei Stände. Alternative: monatlich oder abschaltbar (Einstellungen kommen
   später).

## 12. Umsetzungsschritte

| Schritt | Inhalt | Prüfung |
|---|---|---|
| 1 | **Zustandsverzeichnis** `visit_state` mit Hash- und Textbesucher; Prüfsumme darüber | Referenzprüfsummen unverändert |
| 2 | **Spielstand v1:** Schreib- und Lesebesucher, Kopf, Metadaten, Inhaltsfingerabdruck, Befehlsprotokoll, Laden in frischen Inhalt, Gegenprobe; zstd | Unit-Tests aus Abschnitt 10 |
| 3 | **Kommandozeile:** `--speichern`, `--laden`, `--protokoll`, `wiederholung`, `speichertest`, `spielstand`, `--testbefehle`, Lasttest mit Speichern; CTests und CI | `speichertest_50_jahre`, `wiederholung_50_jahre` in allen Compiler-Jobs |
| 4 | **Brücke und Godot:** Spielmenü, F5/F9, Autosave | Godot-Tests in der CI |
| 5 | **Doku und Review** | CI grün |
