# Spieldaten (`data/`)

> **Seit M3: Karte des Kernraums mit 4.542 Orten in 626 Kreisen.** Die Orte decken den Kernraum
> (`karte/geometrie/kernraum.geojson`) zu 99,96 % ab; ohne Ort bleiben 41 kleine Teilflächen,
> meist Inseln ohne GeoNames-Ort (zusammen etwa 274 km²). Erzeugt hat die Orte
> `tools/geodaten/orte.py` aus GeoNames cities1000 (Auswahl, Namen, Kreise und Gelände:
> [tools/geodaten/README.md](../tools/geodaten/README.md), Abschnitt „Orte des Kernraums“).
> Historische Daten (Besitzer, Bevölkerung, Konfession, Verwüstung) haben weiterhin nur die 60
> Orte der Testregion aus M1; die übrigen 4.482 sind *Orte ohne Daten*
> ([chronik-skript.md](../docs/technik/chronik-skript.md), Abschnitt 3.6): Sie erscheinen auf der
> Karte (schraffiert), nehmen aber an der Simulation nicht teil.

> **Testregion für Meilenstein M1 (technischer Durchstich).**
> Die historischen Daten sind bewusst **unvollständig**: Sie decken nur Mittel- und
> Nordostdeutschland mit 60 Orten und 16 Staaten ab und dienen dazu, Lader, Simulation, Karte und
> Oberfläche mit historisch plausiblen Werten zu erproben. Sie sind kein fertiger Spielinhalt; alle
> Werte stehen unter Vorbehalt der fachlichen Prüfung (siehe [Anhang C](../docs/anhang/c_quellen_und_daten.md)).

Format, Pflichtfelder und Formeln: [docs/technik/chronik-skript.md](../docs/technik/chronik-skript.md).

> **Seit M2 werden Orte und Staaten in Tabellen gepflegt** (`tabellen/*.csv`, Format:
> [docs/technik/tabellen.md](../docs/technik/tabellen.md)). Die Dateien in `history/` erzeugt
> `python3 tools/archivar/archivar.py` daraus – bitte nicht von Hand bearbeiten. Die Abschnitte
> unten nennen die Felder des Chronik-Skripts; sie entsprechen gleichnamigen Spalten der Tabellen.

## Inhalt

| | |
|---|---|
| Lesezeichen | **Der Westfälische Frieden**, 24.10.1648 (`westfaelischer_frieden`), Spieler-Vorauswahl Brandenburg |
| Karte | Kernraum (Design-Dokument 05) im Ausschnitt 1,8–23,5 °O × 45,1–56,2 °N, Bild 4096 × 3329 Pixel (`karte/karte.chron`) |
| Gebiet mit Daten | 9,5–16,5 °O, 50,0–54,8 °N: Mark Brandenburg, Erzstift Magdeburg, Halberstadt, Anhalt, Kursachsen mit beiden Lausitzen, Thüringen (Teile), Braunschweig-Wolfenbüttel (Ostteil), Mecklenburg, Vorpommern, Hinterpommern, Nordböhmen als Randgebiet |
| Umfang | 4.542 Orte (60 mit Daten, 4.482 ohne Daten) in 626 Kreisen, 16 Staaten, 11 Geländetypen, 4 Konfessionen, 6 Berufsgruppen |
| Bevölkerung | 384.000 in den modellierten Orten (Stichprobe der Ämter, nicht die Gesamtbevölkerung der Territorien) |

## Aufbau

```
data/
├── README.md                      diese Datei
├── common/
│   ├── regeln.chron               globale Regeln (Werte der Spezifikation)
│   ├── gelaende.chron             11 Geländetypen (seit M3 mit hochgebirge)
│   ├── konfessionen.chron         lutherisch, reformiert, katholisch, jüdisch
│   └── berufe.chron               adel, geistlichkeit, buerger, handwerker, bauern, landarme
├── tabellen/                      Quelle der Wahrheit für history/ (CSV, siehe docs/technik/tabellen.md)
│   ├── orte.csv, staaten.csv      Stammdaten
│   ├── kreise.csv                 Kreise (seit M3; mit orte.csv erzeugt von tools/geodaten/orte.py)
│   ├── besitz.csv                 Besitzer je Ort mit Datum
│   ├── bevoelkerung.csv           Bevölkerung, Konfession, Verwüstung je Stichtag
│   ├── staaten_historie.csv       Herrscher, Hauptstadt, Kasse … mit Datum
│   └── dateien.csv                Kopfkommentare der erzeugten Dateien
├── history/                       erzeugt von tools/archivar (nicht von Hand bearbeiten)
│   ├── staaten/                   ein Staat je Datei: <TAG>_<name>.chron
│   │   ├── ALT_sachsen_altenburg.chron
│   │   ├── BER_anhalt_bernburg.chron
│   │   ├── BOH_boehmen.chron
│   │   ├── BRA_brandenburg.chron
│   │   ├── BWO_braunschweig_wolfenbuettel.chron
│   │   ├── DES_anhalt_dessau.chron
│   │   ├── GOT_sachsen_gotha.chron
│   │   ├── KOE_anhalt_koethen.chron
│   │   ├── MAG_erzstift_magdeburg.chron
│   │   ├── MAI_kurmainz.chron
│   │   ├── MGU_mecklenburg_guestrow.chron
│   │   ├── MSC_mecklenburg_schwerin.chron
│   │   ├── SAX_kursachsen.chron
│   │   ├── SWE_schweden.chron
│   │   ├── WEI_sachsen_weimar.chron
│   │   └── ZER_anhalt_zerbst.chron
│   ├── kreise/                    Kreise je historischer Landschaft (seit M3, 77 Dateien)
│   └── orte/                      Orte je historischer Landschaft (seit M3 77 Dateien, anhalt … wuerttemberg)
│       ├── anhalt.chron           davon mit Daten: 4 Orte
│       ├── boehmen.chron          4 Orte
│       ├── brandenburg.chron     11 Orte (Kurmark, Altmark, Prignitz, Uckermark, Neumark)
│       ├── braunschweig.chron     3 Orte
│       ├── lausitz.chron          6 Orte (Ober- und Niederlausitz, Herrschaft Cottbus)
│       ├── magdeburg.chron        4 Orte (Erzstift, Halberstadt, Amt Jüterbog)
│       ├── mecklenburg.chron      6 Orte (mit dem schwedischen Wismar)
│       ├── pommern.chron          8 Orte (Vor- und Hinterpommern)
│       ├── sachsen.chron          7 Orte (kursächsische Erblande)
│       └── thueringen.chron       7 Orte
├── szenarien/
│   └── westfaelischer_frieden.chron
└── karte/                         Karte des Kernraums (Format: chronik-skript.md, Abschnitt 6)
    ├── karte.chron                Ausschnitt, Projektion, Bildgröße, max_radius_km
    ├── geometrie/                 land, seen, fluesse (erzeugt von tools/geodaten), kernraum (von Hand)
    ├── provinzen.png              Ort-ID-Bild mit Flächenart     ┐ erzeugt von
    ├── fluesse.png                Flussebene (Graustufen)        │ tools/kartengenerator
    └── definition.csv             Kartenindex, Beschriftungspunkt, Pixel, Fläche ┘
```

In den Ortsdateien stehen die Orte mit Daten jeweils vorn; Reihenfolge und Inhalt dieser 60 Orte
sind seit M1 unverändert bis auf `flaeche` (seit M3 aus der Karte) und `kreis`.

Der Lader liest die Ordner jeweils **nach Dateiname sortiert** und die Einträge in Dateireihenfolge;
daraus ergeben sich die Indizes in `World`. Neue Dateien verschieben also die Indizes nachfolgender
Einträge – Verweise im Spiel laufen deshalb immer über Kennungen, nie über Indizes.

## Konventionen

### Kennungen und Dateien

- **Orte:** Kleinbuchstaben, Ziffern und `_`; Umlaute umschrieben (`koethen`, `guestrow`), Namenszusatz
  nur zur Unterscheidung (`brandenburg_havel`, `frankfurt_oder`, `landsberg_warthe`). Bei den 60
  Orten der Testregion folgt der Anzeigename (`name`) der historischen deutschen Schreibweise der
  Zeit (`Cammin`, `Berlin-Cölln`); für die Orte aus M3 gelten die Namensregeln unten
  (Abschnitt „Orte des Kernraums“).
- **Kreise:** Kennung `kr_<Hauptort>` (z. B. `kr_dessau`), Anzeigename nach dem Hauptort.
- **Staaten:** Tag aus drei Großbuchstaben; Dateiname `<TAG>_<name>.chron`.
- **Ortsdateien** sind nach historischen Landschaften gegliedert, nicht nach Besitzern: Ein Ort steht
  in der Datei der Landschaft, in der er liegt (z. B. das schwedische Wismar in `mecklenburg.chron`,
  das brandenburgische Cottbus in `lausitz.chron`). Jede Datei beginnt mit einem Kommentar zur Lage
  der Landschaft am Stichtag.
- Texte und Kommentare auf Deutsch, Zahlen mit Dezimalpunkt, Daten **gregorianisch** im Format
  `JJJJ.MM.TT` (die evangelischen Stände rechneten bis 1700 julianisch, der Friedensschluss ist dort
  der 14.10.1648).

### Orte

- Ein Ort steht für eine **Stadt mit ihrem Amt** bzw. Umland; `bevoelkerung` umfasst Stadt **und**
  Umland. `flaeche` ist seit M3 die Fläche des Ortes auf der Karte (`flaeche_km2` aus
  `karte/definition.csv`, übernommen von `tools/geodaten/orte.py`): 1–1.147 km², Median 169 km²
  (in M1 waren es geschätzte 150–280 km²).
- `stadt = ja`, wenn die Stadt den Ort wirtschaftlich prägt (etwa die Hälfte der Bevölkerung oder
  mehr, Residenz-, Handels- oder Bergstadt); kleine Ackerbürgerstädte mit großem Amt sind `nein`.
- `lage` ist die reale Lage des Hauptortes (WGS84; bei den Orten aus M1 zwei Nachkommastellen
  ≈ 1 km, bei den übrigen die Koordinate aus GeoNames).
- `verwuestung` gibt die Kriegsschäden am Stichtag nach regionaler Betroffenheit an:

  | Wert | Bedeutung | Beispiele |
  |---|---|---|
  | 0,25–0,30 | leicht (nicht erobert, Handel lief weiter) | Dresden, Braunschweig, Stralsund |
  | 0,40–0,50 | mittel | Leipzig, Erzgebirge, Lausitz, Thüringen, Nordböhmen |
  | 0,55–0,65 | schwer | Mark Brandenburg, Anhalt, Hinterpommern, Mecklenburg |
  | 0,70–0,80 | verheerend | Prignitz, Neubrandenburg, Magdeburg |

- `konfession`: ganzzahlige Prozentwerte, Summe 100. Die Reihenfolge im Block bestimmt die
  Reihenfolge der Bevölkerungsblöcke; die Mehrheitskonfession steht vorn.

### Orte des Kernraums (seit M3)

Die 4.482 Orte ohne Daten und alle 626 Kreise erzeugt `python3 tools/geodaten/orte.py`
reproduzierbar in `tabellen/orte.csv` und `tabellen/kreise.csv`; Verfahren, Hilfstabellen und
Kennzahlen stehen in [tools/geodaten/README.md](../tools/geodaten/README.md#orte-des-kernraums-ortepy).
Diese Zeilen bitte nicht von Hand ändern, sondern die Hilfstabellen in `tools/geodaten/` anpassen
und danach `orte.py`, den Archivar und den Kartengenerator laufen lassen.

- **Herkunft:** GeoNames `cities1000` (Siedlungen mit heute mindestens 1.000 Einwohnern, Stand
  2020, CC BY 4.0; Quellenangabe in [Anhang C](../docs/anhang/c_quellen_und_daten.md#5-verwendete-geodaten-stand-m3)).
  Ausgewählt wird nach heutiger Bedeutung (Einwohner, Verwaltungsrang, Bekanntheit) mit einem
  Mindestabstand von etwa 4–11 km je nach Bedeutung, dazu Inseln und Lückenfüller; moderne
  Gründungen und Kunstnamen aus Gemeindereformen sind ausgeschlossen
  (`tools/geodaten/ausschluesse.csv`); fehlt in GeoNames die Einwohnerzahl eines
  Verwaltungssitzes, gilt eine belegte Untergrenze (`tools/geodaten/einwohner.csv`). Die `quelle` jedes neuen Ortes nennt die `geonameid` und
  die Herkunft des Namens.
- **Lage, Gelände, Stadt:** `lage` ist die GeoNames-Koordinate (Orte knapp im Wasser auf das
  nächste Landpixel gerückt, in `quelle` vermerkt). `gelaende` kommt aus einem Höhenmodell (AWS
  Terrain Tiles) mit festen Schwellen, `stadt = ja` heißt heute Haupt-, Landeshaupt- oder
  Kreisstadt oder mindestens 20.000 Einwohner – eine Näherung, kein historisches Stadtrecht.
- **Namensregeln:** (1) ein deutscher Name aus `tools/geodaten/exonyme.csv`, aber nur, wenn er
  wörtlich in den GeoNames-Alternativnamen belegt ist (z. B. Breslau, Königsberg, Straßburg);
  (2) in Deutschland, Österreich und Liechtenstein der GeoNames-Name ohne moderne Zusätze
  („Lutherstadt“, „Hansestadt“, „Ostseebad“ …; „Bad“ bleibt stehen); (3) in Südtirol der deutsche
  Teil des zweisprachigen Namens; (4) sonst der **heutige Name** (896 Orte, für die
  `exonyme.csv` keinen belegten deutschen Namen enthält, z. B. Lubawa statt Löbau, weil GeoNames
  dort keinen deutschen Namen führt). Gleichnamige Orte erhalten den
  Kurznamen ihrer Landschaft in Klammern („Mühlhausen (Böhmen)“, 82 Orte). Namen aus den
  Umbenennungen 1933–1945 werden nicht übernommen.
- **Noch zu prüfen:** Viele Namen sind heutige Formen: 92 mit „Bad “ (Titel meist erst im 19.
  oder 20. Jahrhundert verliehen), 9 mit heutigem Klammerzusatz aus GeoNames (z. B.
  „Schwedt (Oder)“, „Kempten (Allgäu)“), 3 mit „in Bayern“, Namen aus Zusammenlegungen des
  20. Jahrhunderts (Wuppertal, Garmisch-Partenkirchen, Castrop-Rauxel). Bei den Kreisnamen sind es 13 mit „Bad “
  und 27 mit Klammer. Korrekturen bitte mit Beleg über `exonyme.csv` bzw. die Issue-Vorlage
  „Historischer Fehler“.
- **Kreise** sind räumlich gebildet (4–12 benachbarte Orte, in der Regel derselben Landschaft,
  Median 7) und heißen nach ihrem heute größten Ort; sie sind **keine historischen Verwaltungseinheiten**
  (`quelle`: „räumlich gebildet (M3), keine historische Verwaltungseinheit“).
- **Ortsdateien:** Die 77 Landschaften (`history/orte/<landschaft>.chron`, ebenso
  `history/kreise/`) folgen heutigen Verwaltungsgrenzen, die die historischen Landschaften nur
  annähern; Zuordnung in `tools/geodaten/landschaften.csv`.

### Staaten

- `konfession` ist die Konfession des **Herrscherhauses** (Brandenburg reformiert bei lutherischem Land).
- `hauptstadt` muss ein Ort des Staates in der Testregion sein. Liegt die Residenz außerhalb, steht
  der wichtigste Ort der Testregion (Schweden → Stettin als Sitz der Regierung in Pommern,
  Kurmainz → Erfurt); die `quelle` vermerkt das.
- `regierungsform` (in M1 nur Anzeige) verwendet dieses Vokabular:

  | Bezeichner | Bedeutung |
  |---|---|
  | `staendisches_fuerstentum` | weltliches Fürstentum mit Landständen (alle weltlichen Reichsfürsten 1648, auch Brandenburg vor dem Landtagsrezess von 1653) |
  | `geistliches_fuerstentum` | geistliches Territorium unter einem gewählten Erzbischof oder Administrator |
  | `staendische_monarchie` | Königreich mit Reichsständen (Schweden) |
  | `erbmonarchie` | habsburgisches Erbkönigtum (Böhmen nach der Verneuerten Landesordnung 1627) |

### Besitzstand am Stichtag

Maßgeblich ist der **Rechtsstand des Friedensvertrags** vom 24.10.1648. Wo die tatsächliche Lage
abweicht, steht das in `quelle`:

- **Hinterpommern und das Stift Cammin** sind Brandenburg zugeordnet, blieben aber bis zum Stettiner
  Grenzrezess 1653 schwedisch besetzt.
- **Halberstadt** ist Brandenburg zugeordnet; die Huldigung fand erst am 03.04.1650 statt.
- **Rostock** war Kondominium beider mecklenburgischer Herzöge und ist vereinfacht Mecklenburg-Schwerin
  zugeordnet (Adolf Friedrich I. führte 1648 auch die Güstrower Vormundschaft).
- **Braunschweig** (Stadt) und **Erfurt** waren faktisch weitgehend selbständig (bis 1671 bzw. 1664)
  und sind ihren Landesherren zugeordnet.
- **Jüterbog** gehört als magdeburgisches Amt seit dem Prager Frieden 1635 zu Kursachsen.
- **Leipzig** (bis 1650), **Erfurt** (bis 1650) und Teile **Prags** (bis 1649) waren am Stichtag
  schwedisch besetzt; Besatzung wird in M1 nicht modelliert.

### Quellenangaben und Schätzungen

Jeder Datensatz hat ein `quelle`-Feld. Dabei gilt:

- **Belegte Tatsachen** (Herrscher, Regierungsjahre, Besitzwechsel, Ereignisse) werden in Kurzform
  genannt; die Belege stehen unten unter „Quellenhinweise“.
- **Bevölkerungszahlen sind Schätzungen** und so gekennzeichnet („Bevölkerung Schätzung (Stadt mit
  Amt; Größenordnung nach Literatur zur Bevölkerungsentwicklung)“). Sie orientieren sich an den in der
  Literatur genannten Größenordnungen für Städte und Landschaften nach dem Krieg, sind aber **nicht**
  aus einzelnen Quellen übernommen. Belegte Einzelwerte werden ausdrücklich genannt (z. B.
  Berlin-Cölln ca. 6.000 Einwohner 1648).
- **Konfessionsanteile** von Minderheiten und **Verwüstungsgrade** sind geschätzt und als solche
  bezeichnet.
- **Schatz, Steuersätze (seit M5 Kontribution, Akzise, Zoll), Hofhaltung und Heer** sind
  **Spielwerte** und keine historischen Haushaltszahlen (siehe unten).

## Staaten

| Tag | Staat | Herrscher | Hauptstadt | Orte | Bevölkerung | Schatz | Kontribution | Hofhaltung | Steuern im 1. Jahr |
|---|---|---|---|---:|---:|---:|---:|---:|---:|
| `ALT` | Herzogtum Sachsen-Altenburg | Friedrich Wilhelm II. | Altenburg | 1 | 7.000 | 5.500 | 0,10 | 12.500 | 11.640 |
| `BER` | Fürstentum Anhalt-Bernburg | Christian II. | Bernburg | 1 | 4.000 | 2.000 | 0,10 | 7.000 | 6.021 |
| `BOH` | Königreich Böhmen | Ferdinand III. | Prag | 4 | 51.500 | 32.000 | 0,14 | 102.000 | 92.006 |
| `BRA` | Kurfürstentum Brandenburg | Friedrich Wilhelm | Berlin-Cölln | 17 | 75.300 | 23.000 | 0,12 | 116.000 | 100.072 |
| `BWO` | Herzogtum Braunschweig-Wolfenbüttel | August der Jüngere | Wolfenbüttel | 3 | 26.000 | 22.000 | 0,10 | 42.000 | 38.167 |
| `DES` | Fürstentum Anhalt-Dessau | Johann Kasimir | Dessau | 1 | 4.200 | 1.500 | 0,10 | 6.750 | 5.788 |
| `GOT` | Herzogtum Sachsen-Gotha | Ernst I. der Fromme | Gotha | 1 | 6.500 | 11.000 | 0,10 | 12.500 | 10.809 |
| `KOE` | Fürstentum Anhalt-Köthen | Ludwig I. | Köthen | 1 | 3.600 | 1.500 | 0,10 | 6.250 | 5.419 |
| `MAG` | Erzstift Magdeburg | August von Sachsen (Administrator) | Halle (Saale) | 2 | 14.000 | 4.500 | 0,10 | 23.000 | 19.708 |
| `MAI` | Kurfürstentum Mainz | Johann Philipp von Schönborn | Erfurt | 2 | 22.000 | 9.000 | 0,08 | 28.000 | 25.655 |
| `MGU` | Herzogtum Mecklenburg-Güstrow | Gustav Adolf (Vormundschaft Adolf Friedrichs I.) | Güstrow | 2 | 7.700 | 1.000 | 0,08 | 6.250 | 5.420 |
| `MSC` | Herzogtum Mecklenburg-Schwerin | Adolf Friedrich I. | Schwerin | 3 | 15.300 | 3.000 | 0,08 | 15.500 | 13.360 |
| `SAX` | Kurfürstentum Sachsen | Johann Georg I. | Dresden | 14 | 99.900 | 92.000 | 0,10 | 153.000 | 129.078 |
| `SWE` | Königreich Schweden | Christina | Stettin | 5 | 31.500 | 23.500 | 0,12 | 59.000 | 51.056 |
| `WEI` | Herzogtum Sachsen-Weimar | Wilhelm IV. | Weimar | 2 | 11.000 | 4.500 | 0,10 | 14.500 | 13.007 |
| `ZER` | Fürstentum Anhalt-Zerbst | Johann | Zerbst | 1 | 4.500 | 1.500 | 0,10 | 6.250 | 5.390 |

Beträge in Reichstalern; „Steuern im 1. Jahr“ nach den Formeln der Spezifikation (Abschnitt 5).

## Wirtschaftswerte

Seit M5 entstehen Einkommen und Steuern aus Gütern und Märkten
([wirtschaft.md](../docs/technik/wirtschaft.md)); der Staatshaushalt hat Hof, Verwaltung, Heer, Zinsen
und Bauten. Alle Werte sind Spielwerte, abgestimmt mit `uhrwerk-cli wirtschaft` (Stand nach dem
Einschwingen) und dem Balancing-Bericht `uhrwerk-cli bilanz` (viele Läufe über 100 Jahre). Herleitung
im Einzelnen: [wirtschaft.md, Abschnitt 4.9](../docs/technik/wirtschaft.md#49-regelwerte-und-kalibrierung).

- **Steuersätze:** Kontribution 0,10 als Regelfall; 0,12 für Brandenburg und Schweden (hohe
  Kontributionen der Kriegs- und Besatzungszeit), 0,14 für Böhmen (habsburgischer Steuerdruck), 0,08 für
  Mecklenburg und Kurmainz (starke Landstände bzw. faktisch autonomes Erfurt). Akzise 0,04–0,06, Zoll
  0,03–0,06 (Schweden mit den Ostseezöllen am höchsten). Das ergibt nach dem Einschwingen etwa
  0,8 Reichstaler je Einwohner und Jahr oder 7,5–11 % der Wirtschaftsleistung; Adel und Geistlichkeit
  sind steuerfrei.
- **Heer** (`heer`, Soldaten je 1.000 Einwohner, 30 Reichstaler Sold je Soldat und Jahr): 3 für Mecklenburg, 4 für die kleinen
  Fürstentümer, Magdeburg und Kurmainz, 6 für Kursachsen und Braunschweig-Wolfenbüttel, 10–12 für Böhmen
  und Brandenburg, 15 für Schweden mit seinen Garnisonen in Pommern.
- **Verwaltung:** 0,15 Reichstaler je Einwohner und Jahr (Ämter, Justiz, Kirche); Ämter waren billig,
  Hof und Heer trugen die Last.
- **Hofhaltung** so, dass Hof + Verwaltung + Heer die Steuern nach dem Einschwingen um etwa 10 %
  übersteigen: Die Staaten starten mit einem **Nachkriegsdefizit**, zehren einige Jahre vom Schatz und
  kommen mit dem Wiederaufbau in den Überschuss. Kursachsen 49.000, Brandenburg 25.000, Böhmen 20.500,
  Kurmainz 12.250, Schweden 10.250, die anhaltischen Fürstentümer 2.000–2.500 Reichstaler im Jahr.
  (In M1 war die Hofhaltung die einzige Ausgabe und lag beim 1,1- bis 1,2-Fachen der Einnahmen.)
- **Schatz** unverändert aus M1, als Anteil der damaligen Jahreshofhaltung nach Lage des Staates:
  Brandenburg klamm (0,2), Mecklenburg-Güstrow fast leer (0,15), Kursachsen wohlhabender (0,6),
  Sachsen-Gotha unter Ernst dem Frommen gut geordnet (0,9), Braunschweig-Wolfenbüttel sparsam verwaltet
  (0,5).
- **Kredit und Bankrott** (`regeln.chron`): 5 % Zins, 2 Prozentpunkte Aufschlag je Jahreseinnahme
  Schulden über der ersten, höchstens 20 %; Bankrott bei Schulden über 4 Jahreseinnahmen (mindestens
  4 Jahresausgaben für Hof, Verwaltung und Heer) mit Schuldenschnitt 50 % und 20 Jahren
  Zinsaufschlag.

**Zielwerte** (`common/zielwerte.chron`, gelesen nur vom Balancing-Bericht; für 1848 siehe unten) und ihre Begründung:

| Kennzahl | Ziel | Begründung |
|---|---|---|
| Vermögen = (Schatz − Schulden) ÷ Jahreseinnahmen | −3 … 3 in 95 % der Staatsjahre; Median am Ende 0 … 1,5 | Ohne Spieler soll kein Staat Schätze horten oder in Schulden versinken; Rücklage und Landesausbau halten den Schatz bei etwa einer Jahreseinnahme |
| Staatsbankrotte je Jahrhundert | Median 0, 95-%-Wert höchstens 2 | Bankrotte gab es (Kursachsen, Habsburg), aber nicht als Regelfall ohne Krieg |
| Steuerquote Ende ÷ Start | 0,6 … 1,4 | Einnahmen wachsen mit der Wirtschaft; nach unten großzügiger, weil die Städte schneller wachsen und die Kontribution dort nur Agrareinkommen trifft |
| Preis ÷ Basispreis | 0,4 … 2,5 in 95 % der Markt-Monate; Kolonialwaren 0,4 … 5 | Teuerungen um das Zwei- bis Dreifache waren in Hungerjahren üblich, dauerhaft höhere Preise wären ein Fehler im Handel; Kolonialwaren kommen über die Häfen |
| Bevölkerung 1648 → 1700 | ×1,20 … ×1,45 | Kalibrierung aus M4 (Wiederaufbau nach dem Krieg) |
| Lebensstandard je Schicht | unten 25–55, mittel 45–75, oben 70–95 | Werte aus M4 |

**Ergebnis** (`bilanz` mit 32 Läufen über 100 Jahre, Stand 11.10.2026): alle Zielwerte erfüllt. Kein
Staatsbankrott; Vermögen am Ende im Median 0,85–1,12 Jahreseinnahmen; tiefster Wert −1,77 (Anhalt-Dessau
nach einer Seuche); Steuerquote 0,69–0,99; Preise zu 96,5–100 % im Bereich; Bevölkerung 1700 ×1,36;
Lebensstandard unten 42, mittel 58, oben 78. Staaten mit einem oder zwei Orten reagieren stark auf
Seuchen in diesen Orten.

**Lesezeichen 1848** (seit M7): Die Regeln und Sätze gelten unverändert; die Haushalte (Hof, Schatz,
Schulden) stehen im Maßstab des Modells ([lesezeichen_1848.md, Abschnitt 8](../docs/technik/lesezeichen_1848.md#8-eisenbahnen-und-industrie-m7b)).
Kalibriert wurde mit drei Eingriffen:

- **Tragfähigkeit:** Die Tragfähigkeit der Gelände ist auf die Dichten um 1648 abgestimmt; 1848 lebten
  im selben Raum zwei- bis dreimal so viele Menschen, und die Bevölkerung schrumpfte ohne Ausgleich.
  Das Szenario setzt deshalb `tragfaehigkeit_faktor = 2.5` (Kartoffel, Klee, Stallfütterung,
  Agrarreformen; [chronik-skript.md, Abschnitt 3.7](../docs/technik/chronik-skript.md#37-szenarienchron)).
- **Vorkommen:** 30 Salinen (Lüneburg, Schönebeck, Reichenhall, Hallein, Hall in Tirol, das
  Salzkammergut, Kreuznach, Schwäbisch Hall, Unna, Staßfurt u. a.) und 43 Weinlagen (Rhein, Mosel,
  Pfalz, Franken, Württemberg, Baden, Saale-Unstrut, Meißen, Wachau und Weinviertel, Südmähren,
  Südtirol, Steiermark) an Orten, die 1648 keine Daten haben; vorher gab es auf der ganzen Karte zwei
  Salz- und drei Weinorte. Dazu ab 1800 die Betriebe `salzwerk` (Salinen) und `brennerei` (Branntwein
  aus Getreide, als Bier gerechnet).
- **Industrie:** `fabrikarbeiter` nimmt 6 % der Städter und 1,5 % der Landbevölkerung aus den
  Landarmen (rund 1,4 Mio. Köpfe); die Spinnerei liefert neben Tuch auch Waren (Kurzwaren, Garn für
  das Gewerbe).

Die **Zielwerte 1848** stehen in `common/zielwerte_voelkerfruehling.chron` (`bilanz` und `pruefen`
nehmen sie für dieses Szenario statt `zielwerte.chron`). Gleich bleiben Haushalt, Steuerquote und
Lebensstandard. Abweichend:

| Kennzahl | Ziel 1848 | Begründung |
|---|---|---|
| Bevölkerung 1848 → 1878 | ×1,10 … ×1,45 | Deutscher Bund 1850–1880 etwa ×1,25 trotz Auswanderung |
| Salz, Wein | 0,4 … 4 | Salinen und Weinlagen an wenigen Orten; Salzregal und Fracht im Binnenland, Wein im Norden ein Gut der Oberschicht |
| Waren | 0,4 … 3 | Manufakturen und Maschinenbau nur in größeren Städten; die Einfuhr englischer Waren fehlt im Modell |
| Kohle, Eisen | 0,25 … 3 | 1848 regionale Märkte: billig an der Ruhr und in Oberschlesien, teuer, wo die Bahn fehlt |

**Ergebnis** (`bilanz --szenario voelkerfruehling` mit 8 Läufen über 30 Jahre, Stand 18.10.2026): alle
Zielwerte erfüllt. Kein Staatsbankrott; Vermögen am Ende im Median 1,05 (Frankfurt) bis 1,26
(Niederlande) Jahreseinnahmen; tiefster Wert −2,76 (Hamburg in den ersten Jahren); Steuerquote
0,93–1,12; Preise zu 95,3 % (Tuch) bis 100 % im Bereich; Bevölkerung 1878 ×1,17; Lebensstandard unten
44, mittel 54, oben 78.

**Politik und Wahlen** (seit M8, [politik.md](../docs/technik/politik.md)):

| Datei | Inhalt |
|---|---|
| `common/stroemungen.chron` | 5 Strömungen mit Position auf der Achse −100 … +100 |
| `common/interessengruppen.chron` | 11 Interessengruppen mit Mitgliedern je Beruf, Konfession und Bildung und Neigung je Strömung (die Arbeiterbewegung ab August 1848) |
| `common/parlamente.chron` | Nationalversammlung 1848 (8 Fraktionen) und Preußische Zweite Kammer 1849 (3 Fraktionen) |
| `common/wahlen.chron` | Wahlregeln, Wahlkampf und 14 Regionen der Wahlgeographie (Spielwerte) |
| `common/wahlziele_voelkerfruehling.chron` | Zielkorridore des Wahllabors (`uhrwerk-cli wahl`) |
| `tabellen/wahlkreise.csv` → `history/wahlkreise/` | 633 Wahlkreise der Nationalversammlung, 337 der Zweiten Kammer; erzeugt von `tools/geodaten/wahlkreise_1848.py` |
| `tabellen/staaten_historie.csv` | Felder `verfassung`, `wahlrecht`, `wahlsystem`, `wahl_indirekt`, `wahl_zensus` je Staat |

Das Lesezeichen 1648 hat kein Parlament; Strömungen und Gruppen sind dort geladen, wirken aber nicht.

**Zollunionen** (seit M9, [diplomatie.md](../docs/technik/diplomatie.md)):

| Datei | Inhalt |
|---|---|
| `common/zollunionen.chron` | Zollverein (Leitung Preußen, lehnt Österreich ab, Tarif 10 %) und Steuerverein (Leitung Hannover, 3 %); Tarife als Spielwerte, kalibriert mit `uhrwerk-cli zoll` |
| `common/zollziele_voelkerfruehling.chron` | Zielwerte des Zolllabors: Zollanteil der Unionen, Beitritte Hannovers und Mecklenburg-Schwerins zum Zollverein |
| `tabellen/staaten_historie.csv` | Feld `zollunion` je Staat: 1848 26 Staaten im Zollverein, 3 im Steuerverein, 9 für sich |

Das Lesezeichen 1648 hat keine Zollunion; jeder Staat ist ein eigenes Zollgebiet.

## Bekannte Lücken und Vereinfachungen

- **Orte ohne Daten:** Im Lesezeichen 1648 haben 4.482 der 4.542 Orte keinen Besitzer, keine
  Bevölkerung und keine Konfession; sie nehmen an der Simulation nicht teil und erscheinen auf der
  Karte schraffiert. 1848 sind es 575 Orte im Ausland (Lothringen, Elsass, Belgien, Niederlande
  außer Limburg, Kongresspolen, Jütland, Burgenland). Namen, Lagen und Kreise der Orte sind aus
  heutigen Daten abgeleitet (siehe „Orte des Kernraums“).
- **Lesezeichen 1848 ungeprüft:** Die Quellen waren bei der Umsetzung nicht erreichbar. Besitz,
  Verwaltung, Staaten und Eisenbahnen tragen `ungeprueft`, die Bevölkerung je Ort `schaetzung`
  (aus Summen je Staat, Provinz und Kronland verteilt), die Haushalte `spielwert`. Die Fachprüfung
  steht aus; vorbereitet ist [stichprobe_1848.md](../docs/pruefung/stichprobe_1848.md).
- **Politik 1848 ungeprüft:** Verfassung und Wahlrecht der Staaten tragen `ungeprueft`; Strömungen,
  Interessengruppen, Wahlregeln und Zielkorridore sind Spielwerte nach Literaturkenntnis. Die
  Sitze verteilen sich nach der geschätzten Bevölkerung 1846 im Wahlgebiet, nicht nach der Bundesmatrikel.
- **Ohne historische Daten** sind innerhalb und am Rand der Testregion u. a. die Reichsstädte
  Mühlhausen, Nordhausen, Goslar, Lübeck und Hamburg, das Reichsstift Quedlinburg, Schwarzburg, Reuß,
  Mansfeld, Stolberg, die Henneberger Lande, die Hochstifte Merseburg und Naumburg, Anhalt-Plötzkau,
  Sachsen-Lauenburg, Braunschweig-Lüneburg (Celle, Calenberg), Holstein, Hessen sowie Franken
  (Coburg, Bayreuth), ebenso der ganze übrige Kernraum. Auf der Karte erscheinen diese Gebiete seit
  M3 als Orte ohne Daten (schraffiert, ohne Besitzer).
- **Stichprobe:** Die Orte stehen jeweils für ein Amt; die Territorien sind dazwischen nicht flächig
  erfasst. Bevölkerung und Hofhaltung der Staaten beziehen sich nur auf die modellierten Orte.
- **Berufe und Konfession** werden unabhängig voneinander verteilt. Historisch waren z. B. Juden von
  Grundbesitz, Zünften und Adel ausgeschlossen (Rechtsstatus „Schutzjude“, siehe
  [docs/07](../docs/07_bevoelkerung_und_gesellschaft.md)); das bildet erst ein späteres
  Bevölkerungsmodell ab. Kleinere jüdische Gemeinden unter 1 % eines Ortes entfallen.
- **Sprache und Volksgruppe** (z. B. Sorben in der Lausitz, Tschechen in Böhmen) sind in M1 nicht
  abgebildet.
- **Besatzungen** am Stichtag (Schweden in Hinterpommern, Halberstadt, Leipzig, Erfurt, Prag) sind nur
  in `quelle` vermerkt.

## Prüfung

```bash
python3 tools/archivar/archivar.py --pruefen      # Tabellen prüfen, history/ aktuell?
python3 tools/archivar/archivar.py --bericht      # Sicherheit der Angaben, Abdeckung der Szenarien
./build/dev/cli/uhrwerk-cli pruefen               # Laden mit dem Kern: 0 Fehler, 0 Warnungen
./build/dev/cli/uhrwerk-cli lauf --jahre 50 --bericht
python3 tools/kartengenerator/kartengenerator.py --pruefen   # Karte aktuell? (braucht numpy)
python3 tools/geodaten/orte.py --pruefen          # Orte und Kreise aktuell? (braucht build/geodaten-cache)
```

Beim Umzug in die Tabellen (M2) wurden die M1-Daten verlustfrei übernommen: Die Prüfsumme nach
100 Jahren blieb unverändert (`8d44cf792e143bb0`). Seit M3 (4.542 Orte) lautet sie
`40de8c42e8f7a0ff`: Die Prüfsumme umfasst alle Orte, und der Rechentag eines Ortes im Monat hängt
von seinem Index ab (Index mod 28 + 1, `compute_day` in `core/src/simulation.cpp`). Durch die
eingefügten Orte ohne Daten haben die meisten der 60 Orte mit Daten einen anderen Rechentag als
in M2; ihre Reihenfolge untereinander ist gleich geblieben. Die Werte nach 100 Jahren weichen
deshalb nur geringfügig ab (Bevölkerung 627.680 statt 627.721). Der erste M3-Stand (4.541 Orte)
ergab `5548a5fa80f75c0d` (627.709); mit Osterode am Harz in `goettingen` rückten die 38 Orte mit
Daten in den Dateien `lausitz` bis `thueringen` um einen Index weiter. Seit M4 (Bevölkerungsmodell:
Altersaufbau, Kultur, Lebensstandard, Wanderung) lautete sie `ed6b14a02cfbfa5d` (642.668 Menschen nach
100 Jahren). Seit M5 (Wirtschaft mit Gütern, Märkten, Steuerarten und Staatshaushalt) lautet sie
`5db13a66d63e6079`, bei jeder Zahl von Threads; nach 100 Jahren leben in den 60 Orten 713.456
Menschen (×1,86; der Landesausbau erhöht die Tragfähigkeit).
Alle Angaben tragen seit M2 eine Sicherheit:
Besitz, Herrscher, Hauptstadt und Konfession der Staaten `ungeprueft` (recherchiert, noch ohne
Fachprüfung), Bevölkerung, Konfessionsanteile und Verwüstung `schaetzung`, Schatz, Steuersätze,
Hofhaltung und Heer `spielwert`. Die Begründungen stehen weiter in der Quelle des Ortes bzw. Staates.

## Quellenhinweise

Für die Zuordnungen und Daten in `quelle` wurden insbesondere herangezogen:

- **Westfälischer Friede:** Instrumentum Pacis Osnabrugense (IPO), v. a. Art. X (Schweden), Art. XI
  (Brandenburg), Art. XII (Mecklenburg-Schwerin); Überblick in
  [docs/06_staaten_und_nationen.md](../docs/06_staaten_und_nationen.md).
- **Pommern:** Gerhard Huch: *Pommern unter Greif und Adler* (BBAW) – Teilung 1648, Grenzregulierung
  bis Mai 1653, Übergabe Hinterpommerns im Juni 1653.
- **Mecklenburg:** Fahrenholzer Vertrag (1621) zur Aufteilung der Ämter und Städte, Rostock als
  Kondominium; Vormundschaft über Gustav Adolf von Güstrow 1636–1654.
- **Anhalt:** Regententafel des Landesarchivs Sachsen-Anhalt (Johann Kasimir 1618–1660,
  Christian II. 1630–1656); Deutsche Biographie zu Ludwig I. von Anhalt-Köthen (gest. 1650) und
  Johann von Anhalt-Zerbst (Rückkehr zum Luthertum 1644); Evangelische Landeskirche Anhalts
  (Konfessionsgeschichte).
- **Magdeburg und Halberstadt:** Deutsche Biographie / ADB zu August von Sachsen-Weißenfels
  (Postulation 1628, Besitz 1638, Residenz Halle); Landesarchiv Sachsen-Anhalt (Huldigungsakten
  Halberstadt 1650, Übergabe Burgs 1687); Zerstörung Magdeburgs 1631 nach German History in
  Documents and Images (GHDI).
- **Kursachsen:** Prager Frieden 1635 (Lausitzen, magdeburgische Ämter); schwedische Besatzung
  Leipzigs 1642–1650.
- **Erfurt:** Stadtchronik der Stadt Erfurt (schwedische Besatzung 1631–1635 und 1637–1650,
  Unterwerfung 1664).
- **Braunschweig-Wolfenbüttel:** Ende der kaiserlichen Besatzung Wolfenbüttels im November 1643.
- **Böhmen und Prag:** Verneuerte Landesordnung 1627; schwedischer Überfall auf Prag am 26.07.1648
  und Belagerung bis zur Friedensnachricht (Rebitsch/Öhman/Kilián: *1648. Kriegführung und
  Friedensverhandlungen*, 2018); jüdische Gemeinde Prags nach Encyclopaedia Judaica, Artikel „Prague“.
- **Bevölkerung allgemein:** Größenordnungen nach der Literatur zur Bevölkerungsentwicklung im und
  nach dem Dreißigjährigen Krieg (Überblick z. B. bei Peter H. Wilson: *Der Dreißigjährige Krieg. Eine
  europäische Tragödie*); Berlin-Cölln nach der Berliner Stadtgeschichte (ca. 6.000 Einwohner 1648).
  Weitere Literatur: [Anhang C](../docs/anhang/c_quellen_und_daten.md).

Historische Fehler bitte über die Issue-Vorlage „Historischer Fehler“ melden.
