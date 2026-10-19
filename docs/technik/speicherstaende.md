# Speicherstände und Wiederholungen (Meilenstein M6)

> **Status: verbindlich** – umgesetzt in M6
> ([meilensteine.md](../meilensteine.md#m6--speicherstände-und-wiederholungen-)). Grundlage sind
> [02 Engine und Technik](../02_engine_und_technik.md), Abschnitte 4, 5.3, 5.5, 5.6 und 10 (Befehle
> mit Tick-Nummer, Determinismus, Ladezeit und Größe, Binärformat mit zstd, versioniertes Schema,
> Textexport, Autosave, Lockstep), und [23 Multiplayer und Modding](../23_multiplayer_und_modding.md),
> Abschnitt 3.

Eine Partie lässt sich speichern, laden und **bitgleich** fortsetzen, und aus Startzustand und
Befehlen lässt sich jede Partie exakt wiederholen. Beides ist die Grundlage für Multiplayer im
Lockstep (nach der Vorproduktion: nur Befehle werden übertragen), für Fehlerberichte („Spielstand
und Protokoll anhängen“) und für die Fehlersuche bei Determinismusfehlern.

## 1. Umfang

| Bereich | in M6 | später |
|---|---|---|
| Spielstand | Binärdatei mit Kopf, lesbaren Metadaten und mit zstd komprimierten Nutzdaten; Formatversion | Ironman (nur ein Spielstand, Speichern beim Beenden), Steam Cloud |
| Inhalt | Ganzer veränderlicher Zustand aus einem **Zustandsverzeichnis**, das auch die Prüfsumme bildet; Inhaltsfingerabdruck der Spieldaten | Laden trotz geänderter Spieldaten (Zuordnung über Kennungen) |
| Versionen | Formatversion im Kopf, neuere werden abgelehnt | Lesen älterer Versionen, Migrationen zwischen veröffentlichten Spielversionen |
| Befehlsprotokoll | Jeder eingereihte Befehl mit Tag; Protokolldatei mit Kontrollpunkten; jeder Spielstand enthält das Protokoll seit Partiebeginn | Übertragung im Lockstep, Asynchronitätserkennung |
| Werkzeuge | `uhrwerk-cli`: Speichern, Laden, Wiederholung, Speichertest, Spielstandinfo, **Textexport** (nur lesen) | Textimport (Szenario aus Spielstand) |
| Godot | Spielmenü mit Speichern und Laden, Schnellspeichern (F5) und Schnellladen (F9), Autosave am 1. Januar | Einstellungen, Ironman, Cloud |

## 2. Grundprinzip: ein Zustandsverzeichnis

`StateAccess::visit(sim, besucher)` (intern im Kern, `core/src/state_access.hpp`) zählt den
veränderlichen Zustand Feld für Feld auf: Datum, Tage, Seed; je Staat Kasse, Steuersätze, Heer,
Schulden, Haushalt, Zins, Bankrotte, Hofhaltung und Hauptstadt; je Ort Besitzer, Verwüstung und
Blockbereich; Blöcke und Zugänge mit allen Feldern; Momentaufnahmen, Bilanzen, Ortswirtschaft;
Betriebsanteile, Angebot, Nachfrage, Einkommen, Bedarfsdeckung, Landesausbau; Märkte; seit M8 die
abgehaltenen Wahlen (je Wahlkreis Berechtigte, Wähler, Stimmen je Strömung, Sieger, Stichwahl, Boykott,
Meldereihenfolge; je Abgeordnetem Wahlkreis, Strömung, Position, Fraktion), **nur in Welten mit
Parlamenten**, damit 1648 bitgleich bleibt; die drei Zufallsströme; Chronik; wartende Befehle. Jeder
Besucher bekommt jedes Feld mit Namen:

| Besucher | Zweck |
|---|---|
| `HashVisitor` | Prüfsumme (FNV-1a-64, wie bisher) |
| `WriteVisitor` | Spielstand schreiben |
| `ReadVisitor` | Spielstand lesen (in eine Welt mit demselben Inhalt) |
| `TextVisitor` | Textexport: eine Zeile je Feld, `pfad = wert`, diff-fähig |

Damit kann nichts gespeichert werden, was nicht in der Prüfsumme steht, und umgekehrt. Ein Feld,
das künftig hinzukommt (z. B. der Herrscher, sobald er wechseln kann), steht an einer Stelle und
gilt sofort für alle vier Zwecke. Reihenfolge und Kodierung entsprechen der Prüfsumme aus M5; die
**Referenzprüfsummen sind unverändert** (`5db13a66d63e6079` nach 100 Jahren).

`PrimitiveVisitor` zerlegt die Feldtypen in vier Grundformen: Festkomma als `i64` (Rohwert),
vorzeichenbehaftete Ganzzahlen als `i64`, vorzeichenlose und Aufzählungen als `u64`, Datum als drei
`i64`, Zeichenketten als `u64`-Länge plus UTF-8, Zufallsströme als zwei `u64`. Vektoren beginnen
mit ihrer Länge; feste Felder (`std::array`) nicht. Beim Lesen müssen die Längen von Staaten und
Orten zum Inhalt passen (`Extent::Content`), alle übrigen bestimmen sie selbst (`Extent::State`).
Ein gelesener Wert, der nicht in seinen Feldtyp passt (z. B. ein Index über 32 Bit), macht den
Spielstand ungültig.

**Was nicht dazugehört:** unveränderlicher Inhalt (Namen, Gelände, Lage, Fläche, Kreise, Regeln,
Güter, Betriebe, Märkte), abgeleitete Hilfsdaten (Rechentag-Listen, Wanderungsziele,
Markttopologie, Raten) und die Zahl der Threads. Sie entstehen beim Laden aus den Spieldaten neu
(`Simulation::rebuild_derived`).

**Befehlsprotokoll:** Die Simulation führt zusätzlich eine Liste aller **eingereihten** Befehle mit
dem Tag, an dem sie ausgeführt werden (`days_elapsed` beim Einreihen, also vor dem Schritt), auch
der ungültigen. Weil ein eingereihter Befehl immer im nächsten Schritt läuft, ist das zugleich der
Tag der Ausführung; die noch wartenden Befehle stehen damit ebenfalls im Protokoll. Es ist kein
Simulationszustand (es beeinflusst die Zukunft nicht) und gehört nicht zur Prüfsumme, wird aber
mitgespeichert. Dazu kommt die **Startprüfsumme** (Prüfsumme des Startzustands der Partie).

## 3. Inhalt und Inhaltsfingerabdruck

Ein Spielstand enthält nur den Zustand. Beim Laden lädt der Kern zuerst den Inhalt des Szenarios
aus dem Datenordner (wie beim Spielstart) und überschreibt dann den Zustand. Damit das passt, trägt
der Spielstand einen **Inhaltsfingerabdruck**: FNV-1a-64 je Bereich über alles am Inhalt, was die
Simulation beeinflusst, und ein Gesamtwert über die 14 Bereiche (seit M8; vorher 13):

| Bereich | Inhalt |
|---|---|
| `regeln` | alle 62 Regelwerte (ein `static_assert` auf die Zahl der Felder von `Rules` erinnert an neue Felder, auch wenn sie in eine Ausrichtungslücke passen) |
| `gelaende`, `konfessionen`, `kulturen` | Kennungen; Ertragsfaktor und Tragfähigkeit bzw. Alphabetisierungsfaktor |
| `berufe` | Kennung, Schicht, steuerpflichtig, Anteile Stadt/Land, Agrarberuf, Alphabetisierung, Stadtberuf, Agrarabgabe, Anteil „selbstständig“ (seit M8) |
| `gueter`, `beduerfnisse` | Kennung, Basispreis, Weltmarktfaktor; Hunger und Warenkörbe je Schicht |
| `betriebe` | Kennung, Beruf, Erzeugung, Vorleistungen, Agrar, Standort, Vorkommen, Handel, Gewicht, Geländefaktoren |
| `maerkte`, `vorkommen` | Kennungen und Landschaften, Eisenbahnen mit allen Abschnitten (seit M7); Kennungen der Vorkommen |
| `staaten`, `kreise` | Kennung, Kurzname, Konfession des Herrscherhauses und Zollverein (seit M7) der Staaten, die zum Startdatum bestehen; Kennungen der Kreise; Verwaltungseinheiten mit Kennung, Art und übergeordneter Einheit (seit M7) |
| `orte` | Kennung, Name, Gelände, Kreis, Lage, Fläche, Stadt, Markt, Vorkommen, Hafen, Verwaltungseinheit und Bundesgebiet (seit M7); die Zahl der Orte |
| `politik` (seit M8) | Strömungen (Kennung, Name, Position), Interessengruppen (Mitglieder, Neigung), Parlamente mit Wahl, Fraktionen, Sitz und Name, Verfassung und Wahlordnung der Staaten, Wahlkreise mit Name, Staat, Sitzen und Orten, Wahlregeln und Regionen ([politik.md](politik.md)) |

**Nicht** dazu gehören volle Staatsnamen, Farben, Quellen und Beschreibungen: Eine korrigierte
Quelle macht alte Spielstände nicht unbrauchbar. Der **Kurzname der Staaten und der Name der Orte
zählen dagegen mit**: Sie stehen in Chronikeinträgen („Sachsen legt die Akzise …“, „Seuche in
Stendal …“), und die Chronik gehört zur Prüfsumme – ein umbenannter Ort ergäbe sonst bei der
Wiederholung eine scheinbare Abweichung. Passt der Fingerabdruck nicht, lehnt der
Kern den Spielstand mit den abweichenden Bereichen ab („Spielstand passt nicht zu den Spieldaten
(abweichend: berufe, orte)“). Vor Version 1.0 ändern sich die Spieldaten mit fast jedem
Meilenstein; alte Spielstände laufen dann nicht mehr (Abschnitt 11, Entscheidung 2).

Die **Lasttest-Welt** ändert nur den Zustand (Besitzer, Blöcke, Verwüstung), nicht den Inhalt; ihr
Spielstand lädt daher in die frisch geladenen Spieldaten.

**Prüfungen beim Laden**, in dieser Reihenfolge:
1. Kopf: Magie, Formatversion (neuere werden abgelehnt), Kennzeichen, Längen (die Datei muss genau
   so lang sein, wie der Kopf angibt; höchstens 2 GiB Nutzdaten).
2. Metadaten vollständig, Gesamtwert des Fingerabdrucks passt zu den Bereichen.
3. Fingerabdruck gegen die geladenen Spieldaten.
4. Nutzdaten entpacken, Prüfwert (FNV-1a-64 über die unkomprimierten Nutzdaten).
5. Zustand lesen; keine überzähligen Bytes.
6. **Zustand prüfen:** alle Indizes (Blöcke, Zugänge, Besitzer, Hauptstädte, Chronik) im Bereich,
   Blöcke je Ort zusammenhängend und ohne Überschneidung (die parallele Rechnung verlässt sich
   darauf), alle Vektoren der Wirtschaft und Märkte in der richtigen Größe, gültiges Datum,
   Zufallsströme mit ungeradem Inkrement.
7. Metadaten passen zum Zustand (Tage, Datum, Seed, Zahl der Befehle, Startprüfsumme).
8. **Gegenprobe:** abgeleitete Hilfsdaten neu aufbauen, Prüfsumme nachrechnen und mit der
   gespeicherten vergleichen.

Ein Fuzz-Lauf mit 3.000 verfälschten, aber gültig gepackten Nutzdaten unter AddressSanitizer und
UndefinedBehaviorSanitizer endete ohne Befund; angenommen wurden nur Änderungen ohne Wirkung.

## 4. Dateiformat (Version 1)

Endung `.einigkeit`. Alle Zahlen little-endian mit fester Breite, unabhängig von Plattform und
Compiler; kein `memcpy` von Strukturen.

```
Kopf (fest, 48 Byte)
  Magie            8 Byte  "EINIGKT\x1A"
  Formatversion    u32     2 (1 bis M7)
  Kennzeichen      u32     Bit 0: Nutzdaten mit zstd komprimiert
  Metadaten        u32     Länge in Byte
  reserviert       u32     0
  Nutzdaten roh    u64     Länge unkomprimiert
  Nutzdaten        u64     Länge in der Datei
  Prüfwert         u64     FNV-1a-64 über die unkomprimierten Nutzdaten
Metadaten (UTF-8-Text, unkomprimiert, eine Zeile je Angabe "schluessel=wert")
  spiel=0.1.0, formatversion=2, szenario=westfaelischer_frieden, spielerstaat=BRA,
  datum=1748-10-24, seed=1648, tage=36525, pruefsumme=5db13a66d63e6079,
  startpruefsumme=29f316d0f4498130, befehle=0, inhalt=69719ceda66eebd9, inhalt.regeln=…
  (je Bereich aus Abschnitt 3)
Nutzdaten (zstd 1.5.6, Stufe 3; je Abschnitt 4 Zeichen Kennung und u64-Länge)
  ZUST  alle Felder aus dem Zustandsverzeichnis in fester Reihenfolge
  PROT  Startprüfsumme, Zahl der Befehle, je Befehl: Tag (i64), Art (u64), Staat (i64),
        Wert (i64, Festkomma roh), Steuerart (u64)
```

Die Metadaten lassen sich lesen, ohne die Nutzdaten zu entpacken (Liste der Spielstände in Godot).
Lässt sich nicht packen (praktisch nie), steht der Abschnitt ungepackt in der Datei und Bit 0 fehlt.

**Gleicher Zustand, gleiche Datei:** Ein Spielstand enthält keine Uhrzeit und keinen Rechnernamen
(Godot zeigt das Änderungsdatum der Datei). Speichern → Laden → Speichern ergibt dieselben Bytes;
derselbe Lauf ergibt unter GCC, Clang und MSVC denselben Prüfwert der Nutzdaten (nach 100 Jahren mit
`data/`: `17078dcdeecde792`, im CI-Job „Determinismus“ verglichen).

**Größe und Zeit (gemessen, Release, 4 Kerne):**

| Welt | Datei | Nutzdaten roh | Speichern | Laden |
|---|---|---|---|---|
| Spieldaten (60 Orte mit Daten, 4.542 Orte) nach 100 Jahren | 38 KB | 3,8 MB | 0,03 s | 0,03 s, mit Laden der Spieldaten 0,5 s |
| Lasttest-Welt, 500.000 Blöcke | 2,9 MB | 41,9 MB | 0,4 s | 0,5 s mit Laden der Spieldaten |

Ziel aus Kapitel 02: höchstens 100 MB und 20 s Ladezeit im Spätspiel. Die Nutzdaten der Spieldaten
bestehen fast nur aus den Vektoren der 4.482 Orte ohne Daten (Nullen) und packen daher gut.

## 5. Befehlsprotokoll und Wiederholung

Eine **Wiederholung** ist Startzustand plus Befehle: Szenario, Seed und Inhaltsfingerabdruck
bestimmen den Startzustand, die Startprüfsumme sichert ihn ab; das Protokoll sagt, an welchem Tag
welcher Befehl eingereiht wurde. `run_replay` lädt das Szenario neu, prüft Fingerabdruck und
Startprüfsumme, reiht vor jedem Schritt die Befehle des Tages ein und rechnet Tag für Tag weiter.
Befehle mit dem Endtag werden zum Schluss eingereiht (sie warteten beim Speichern noch) und gehen
in die Endprüfsumme ein.

**Kontrollpunkte:** Prüfsumme nach jedem Schritt, der auf einen 1. Januar führt (`day` = Tage danach).
Beim Wiederholen meldet der Kern den ersten abweichenden Kontrollpunkt – damit ist ein
Determinismusfehler auf ein Jahr eingegrenzt; der Textexport zweier Spielstände aus diesem Jahr
(`uhrwerk-cli spielstand … --text`, dann `diff`) zeigt das abweichende Feld.

**Protokolldatei** (Endung `.protokoll`): gleicher Kopf mit eigener Magie `EINIGPR\x1A`;
Metadaten `spiel`, `formatversion`, `szenario`, `seed`, `startpruefsumme`, `endtag`,
`endpruefsumme`, `befehle`, `kontrollpunkte` und Fingerabdruck; Nutzdaten mit den Abschnitten
`BEFE` (Befehle wie in `PROT`) und `KONT` (Zahl, je Kontrollpunkt Tag und Prüfsumme).

Jeder Spielstand enthält das Protokoll seit Partiebeginn: `uhrwerk-cli wiederholung spiel.einigkeit`
rechnet die Partie vom Start nach und vergleicht mit dem gespeicherten Zustand. Eine Partie, die aus
einer Lasttest-Welt begann, lässt sich so nicht wiederholen (ihr Startzustand ist nicht das
Szenario); die Meldung lautet dann „Startzustand weicht ab“.

**Testbefehle:** Die Kommandozeile hat keine Spielerbefehle. Für Tests erzeugt `--testbefehle N`
N Steuerbefehle an zufälligen Tagen aus einem eigenen Zufallsstrom `Testbefehle = 6`, der die Ströme
des Spiels nicht berührt: Staat −1 bis hinter den letzten, Steuerart 0 bis 3 (3 ist ungültig), Satz
von −10 % bis 110 % des Höchstsatzes – also gültige und ungültige.

## 6. Versionen und Migration

- Die **Formatversion** steht im Kopf (`kSaveFormatVersion = 2` seit M8, vorher 1). **Neuere**
  Versionen lehnt der Kern ab („Spielstand aus einer neueren Spielversion (Format 3, unterstützt bis 2)“).
- **Format 2 (M8):** die Wahlen im Zustandsverzeichnis und der Inhaltsbereich `politik`. Ein
  Spielstand im Format 1 zeigt seine Metadaten noch an (13 Inhaltsbereiche, der Gesamtwert über
  diese 13; `politik` bleibt 0), lädt aber nicht („Spielstand im Format 1 wird nicht unterstützt“):
  Mit `politik` hat sich ohnehin der Inhalt geändert (Abschnitt 3).
- Jede Änderung am Zustand (neues Feld, neue Reihenfolge) erhöht die Formatversion. Der Test
  „Spielstand: Formatwache“ verhindert, dass das vergessen wird: Er speichert die Test-Fixture nach
  einem Jahr und vergleicht den Prüfwert der Nutzdaten mit einem festgehaltenen Wert. Ändert er
  sich, muss entweder die Formatversion steigen oder (bei geänderten Testdaten oder geänderter
  Simulation) der Referenzwert neu festgehalten werden; die Meldung des Tests sagt beides.
- Einen Versionsschalter im Lesebesucher (`if (v.version() >= 2) v.field("…", neues_feld)`), der
  Felder überspringt oder mit Standardwerten füllt, gibt es noch nicht. Weil sich vor 1.0 mit fast
  jedem Meilenstein auch der Inhalt ändert, laufen alte Spielstände ohnehin nicht mehr (Abschnitt 3);
  echte Migrationen werden mit der ersten öffentlichen Version wichtig.
- Der Test „Politik: Spielstand vor und nach dem Wahltag“ hat eine eigene Formatwache für die Wahlen
  (Testszenario `jahresbeginn_1650` mit dem Landtag der Testdaten).

## 7. Kern-Schnittstelle (`uhrwerk/savegame.hpp`)

```cpp
inline constexpr std::uint32_t kSaveFormatVersion = 1;
ContentFingerprint content_fingerprint(const World&);              // je Bereich und gesamt
std::vector<std::string_view> content_differences(const ContentFingerprint&, const ContentFingerprint&);

struct SaveMeta { std::string player_tag; };                       // vom Aufrufer (Godot kennt den Spieler)
std::vector<std::uint8_t> save_game(const Simulation&, const SaveMeta&);
std::optional<SaveInfo> read_save_info(std::span<const std::uint8_t>, std::string* error);
LoadedGame load_game(std::span<const std::uint8_t>, World content, unsigned threads = 0);

Replay make_replay(const Simulation&, std::vector<Checkpoint> checkpoints = {});
bool is_checkpoint_day(const Simulation&);                         // Datum ist der 1. Januar
std::vector<std::uint8_t> write_replay(const Replay&);
std::optional<Replay> read_replay(std::span<const std::uint8_t>, std::string* error);
std::optional<Replay> replay_from_save(std::span<const std::uint8_t>, std::string* error);
ReplayResult run_replay(const Replay&, World start, unsigned threads = 0);

FileKind file_kind(std::span<const std::uint8_t>);                 // Spielstand, Protokoll, fremd
bool write_file(const std::filesystem::path&, std::span<const std::uint8_t>, std::string* error);
std::optional<std::vector<std::uint8_t>> read_file(const std::filesystem::path&, std::string* error);
```

`load_game` bekommt den frisch geladenen Inhalt (`load_world(daten, szenario)`) und gibt die
fertige Simulation zurück (Abschnitt 3: geprüft, Hilfsdaten neu aufgebaut, Prüfsumme nachgerechnet).
`write_file` schreibt zuerst in `<datei>.tmp` und benennt dann um: Ein Absturz beim Speichern lässt
den alten Spielstand heil. In der Simulation kamen `command_log()`, `start_checksum()` und
`state_text()` (Textexport) hinzu.

## 8. Kommandozeile

| Aufruf | Zweck |
|---|---|
| `lauf --jahre N [--testbefehle K] --speichern DATEI` | Lauf, am Ende speichern; die Zeile `Prüfwert:` nennt den Prüfwert der Nutzdaten |
| `lauf --laden DATEI --jahre N` | Spielstand laden und weiterrechnen (Szenario und Seed aus dem Spielstand) |
| `lauf … --protokoll DATEI` | Befehle seit Partiebeginn und jährliche Kontrollpunkte schreiben |
| `wiederholung DATEI` | Protokoll oder Spielstand vom Start nachrechnen; Exitcode 1 bei Abweichung, mit Datum des ersten abweichenden Kontrollpunkts |
| `speichertest [--jahre N] [--testbefehle K] [--punkte P]` | Abnahmetest: Lauf A ohne Unterbrechung; Lauf B speichert an P gleichmäßig verteilten Tagen (jeder zweite auf einem Tag mit Testbefehl, also mit wartenden Befehlen) und am ersten Tag mit Zugängen in eine Datei, lädt sie mit frisch geladenen Spieldaten (abwechselnd mit einem Thread und mit `--threads`) und rechnet mit dem geladenen Stand weiter; alle Jahresprüfsummen und der Endstand müssen gleich sein |
| `spielstand DATEI` | Metadaten, Größe, Prüfwert; lädt probeweise mit den Spieldaten (Exitcode 1, wenn das nicht geht) |
| `spielstand DATEI --text` | nur der Textexport (eine Zeile je Feld, für `diff`) |
| `lasttest … --speichern DATEI` | zusätzlich Größe sowie Schreib- und Ladezeit der Lasttest-Welt |

## 9. Brücke und Godot

**Brücke** ([bridge-api.md](bridge-api.md#spielstände-seit-m6)):
- `save_game(path, player_state_index) -> String` (leer bei Erfolg, sonst Meldung; legt den Ordner an)
- `load_game(data_path, path) -> String` (ersetzt die laufende Simulation nur bei Erfolg; der
  Spielerstaat kommt aus dem Spielstand)
- `get_save_info(path) -> Dictionary`: Szenario, Datum, Tage, Spielerstaat (Kennung und Kurzname),
  Formatversion, Spielversion, Prüfsumme, `compatible` und `reason` (Vergleich mit dem Inhalt der
  geladenen Spieldaten), ohne zu laden
- `get_save_format_version() -> int`

**Godot:**
- **Spielmenü** über den Knopf „Menü“ in der oberen Leiste oder F10: Speichern unter einem Namen
  (vorbelegt mit Staat und Datum, z. B. „Brandenburg 1649-05-12“; verbotene Zeichen werden zu „_“;
  `schnellspeicher` und `autosave_…` sind in jeder Schreibweise reserviert, weil F5 und die
  Rotation sie überschreiben bzw. löschen),
  Laden aus der Liste der Spielstände in `user://spielstaende/` (zuletzt geänderte zuerst, mit Datum,
  Staat und Änderungszeit; unpassende ausgegraut, der Grund steht im Tooltip), Schnellspeichern,
  Schnellladen, Weiterspielen. Die Zeit hält an, solange das Menü offen ist; Escape, F10 oder ein
  Klick neben das Fenster schließen es und setzen die vorige Geschwindigkeit fort.
- **F5** speichert in `schnellspeicher.einigkeit`, **F9** lädt ihn. Rückmeldungen erscheinen
  drei Sekunden lang in der oberen Leiste unter dem Datum.
- **Autosave** am 1. Januar jedes Jahres, die letzten drei bleiben (`autosave_1.einigkeit` ist der
  neueste). Die Zeit läuft dazu je Bild höchstens bis zum Jahreswechsel; die übrigen Tage folgen im
  nächsten Bild.
- Nach dem Laden bauen Spieler, Grenzen, Namen, Panels, Legende und Chronik neu auf; die Auswahl
  wird aufgehoben, die Zeit steht.
- Startoption `--spielmenue` öffnet das Menü (Screenshots).

## 10. Tests und Abnahme

| Kriterium aus dem Meilensteinplan | Prüfung |
|---|---|
| Speichern → Laden → Weiterspielen ergibt dieselbe Prüfsumme wie ohne Unterbrechung | Unit-Test „Weiterspielen nach dem Laden“: 17 Speicherpunkte der Fixture über 800 Tage, auch mit wartenden Befehlen, geladen mit 1 und 4 Threads; Unit-Test „Zugänge in den Spieldaten“; CTest `speichertest_50_jahre` mit den echten Daten über die Datei (11 Speicherpunkte, 50 Jahresprüfsummen); Godot-Rauch- und Klicktest |
| Wiederholung aus Befehlsprotokoll ist bitgleich (CI-Test über 50 Jahre) | CTests `protokoll_50_jahre` (`lauf --jahre 50 --testbefehle 200 --protokoll … --speichern …`), `wiederholung_50_jahre` (Protokoll mit 50 Kontrollpunkten) und `wiederholung_spielstand` in allen drei Compiler-Jobs |

Weitere Tests:
- Zustandsverzeichnis: Referenzprüfsummen unverändert; Textexport.
- Format: Speichern → Laden → Speichern ergibt dieselben Bytes; Kopf und Metadaten; abgelehnt
  werden fremde Dateien, Protokoll statt Spielstand, neuere Version, abgeschnittene Datei,
  verfälschte Bytes, unvollständige Metadaten, Metadaten, die nicht zum Zustand passen, ein
  ungültiger Verweis im Zustand, falscher Inhaltsfingerabdruck (mit Meldung je Bereich) und eine
  falsche Gegenprobe. CTests `spielstand_laden` und `spielstand_fremder_inhalt`.
- Wiederholung: Protokoll schreiben und lesen, mit 1 und 4 Threads nachrechnen, aus einem
  Spielstand; ein geänderter Befehl wird am ersten Kontrollpunkt danach erkannt, ein verfälschter
  Kontrollpunkt an seinem Tag; anderer Seed oder Inhalt wird abgelehnt.
- Plattformen: Der Determinismus-Job vergleicht den Prüfwert der Nutzdaten eines Spielstands nach
  100 Jahren unter GCC, Clang und MSVC.
- Formatwache (Abschnitt 6), Lasttest-Welt speichern und laden (Unit-Test und CTest
  `lasttest_threads`), Größe und Zeiten der Lasttest-Welt in der Zusammenfassung des CI-Jobs.
- Godot: Einheitentest (Dateinamen, Jahreswechsel, Rotation der Autosaves, Tasten), Rauchtest
  (Speichern, Angaben, Laden, Weiterrechnen, Fehlerfälle über die Brücke), Klicktest (F10, Speichern
  im Menü, Escape, F5, F9, Laden aus der Liste, ausgegraute fremde Datei, Autosave am 1. Januar).

## 11. Entscheidungen

Freigegeben vor der Umsetzung, jeweils mit der Empfehlung:

1. **Kompression mit zstd schon in M6:** ja. zstd 1.5.6 (BSD-Lizenz) kommt wie doctest per CMake
   `FetchContent` mit fester Version und Prüfsumme und wird statisch gelinkt (ohne Programme, Tests
   und eigene Threads).
2. **Spielstände bei geänderten Spieldaten:** ablehnen (Fingerabdruck muss passen). Das Laden über
   Kennungen kommt, wenn es Spieler mit langen Partien gibt (spätestens vor einer öffentlichen
   Version).
3. **Spielmenü:** Knopf in der oberen Leiste plus F10; Escape hebt außerhalb des Menüs weiterhin die
   Auswahl auf.
4. **Autosave:** jährlich, drei Stände; abschaltbar mit den Einstellungen (später).

## 12. Bekannte Grenzen

- **Spieldaten müssen passen:** Jede Änderung an Regeln, Berufen, Gütern oder Orten macht alte
  Spielstände unbrauchbar (Abschnitt 3); vor 1.0 ist das gewollt.
- **Keine älteren Formatversionen:** Format 1 lädt seit M8 nicht mehr (Abschnitt 6); einen
  Versionsschalter im Lesebesucher gibt es erst mit der ersten Migration.
- **Wiederholung nur ab Szenariostart:** Das Protokoll reicht immer bis zum Partiebeginn zurück,
  auch nach `lauf --laden`. Eine Partie, die in einer Lasttest-Welt begann, lässt sich aber nicht
  wiederholen: Ihr Startzustand ist nicht das Szenario.
- **Fenster des Spiels:** Ein offenes Haushaltsfenster (eigenes Popup) nimmt F10 entgegen; Escape
  schließt es zuerst.
- **Kein Überschreiben-Dialog:** Speichern unter einem vorhandenen Namen überschreibt ihn.
