# Klang – Musik und Bedienklänge (seit M13b2)

Werkzeug zu [vertical_slice.md](../../docs/technik/vertical_slice.md), Abschnitt 5.3 (Entscheidung 13):
gemeinfreie Werke als eigener Notentext, gerendert mit einem eigenen Synthesizer. Keine fremden
Aufnahmen, keine Samples.

| Datei | Inhalt |
|---|---|
| `klang.py` | Kommandozeile: rendert Stücke und Klänge, prüft die Prüfsummen |
| `noten.py` | Leser des Notenformats (`game/audio/noten/*.noten`) mit Takt- und Längenprüfung |
| `synth.py` | Synthesizer: Klavier, Streicher, Holz, Blech, Pauke, Trommel; Hall nach Schroeder |
| `klaenge.py` | Bedienklänge der Epoche III: Gänsekiel, Siegel, Papier, Glocke, Trommel, Telegraf |
| `test_klang.py` | Tests (`python3 -m unittest discover -s tools/klang -v`) |

## Aufruf

```bash
python3 tools/klang/klang.py                 # alles nach game/audio/musik und game/audio/klaenge
python3 tools/klang/klang.py --pruefen       # im Speicher rendern, mit game/audio/pruefsummen.txt vergleichen
python3 tools/klang/klang.py --pruefsummen   # nach einer gewollten Änderung die Prüfsummen neu schreiben
python3 tools/klang/klang.py --nur loreley glocke
```

Abhängigkeiten: Python ab 3.10 und numpy. Alle sieben Stücke und sechs Klänge brauchen rund 30 s;
unveränderte Dateien (gleiche Prüfsumme) überspringt der Aufruf ohne Optionen. Die WAV-Dateien
(16 Bit, mono, 44,1 kHz, zusammen rund 65 MB) sind **nicht versioniert**; Godot importiert sie
komprimiert. Dazu schreibt `klang.py` `game/audio/musik/stuecke.cfg` (Titel, Komponist, Werk,
Stimmung, Dauer je Stück), die das Spiel liest.

Exitcodes: 0 = Erfolg, 1 = `--pruefen`: eine Prüfsumme weicht ab oder fehlt, 2 = Fehler in Noten
oder Aufruf.

## Bitgleich

Die Ausgabe ist über Rechner und numpy-Versionen gleich (geprüft mit numpy 2.2.6 und 2.5.3, wie
die CI): Sinus, Exponentialkurve und Halbtonverhältnisse kommen aus Tabellen, die das `math`-Modul
rechnet und auf ein Raster von 2⁻³⁰ rundet; numpy rechnet danach nur Grundrechenarten, `cumsum`,
`floor` und Rundung. Rauschen stammt aus `numpy.random.RandomState`, dessen Folge numpy festschreibt.
Nicht verwendet werden FFT, Faltung, `np.sin`, `np.exp` und `np.power`: deren Wege unterscheiden
sich je nach Version und Prozessor in den letzten Bits. Der Hall rechnet seine Rückkopplung
blockweise (Blöcke der Verzögerungslänge), damit er ohne Python-Schleife je Probe auskommt.

## Notenformat

```text
titel     = "Ode an die Freude"              # Pflicht
komponist = "Ludwig van Beethoven"           # Pflicht
werk      = "9. Sinfonie d-Moll op. 125 (1824), 4. Satz, Hauptthema"
stimmung  = frieden                          # Pflicht: frieden, krise, krieg, revolution; mehrere: "frieden krise"
tempo     = 96                               # Pflicht: Viertel je Minute (20–300)
takt      = 4/4                              # Pflicht
hall      = 0.25                             # 0–1 (0,2)

stimme melodie { instrument = holz  lautstaerke = 0.8 }   # klavier, streicher, holz, blech, pauke, trommel

teil a {                                     # beliebig viele Teile
    lautstaerke = 1.1                        # optional, Faktor für den Teil
    melodie = "fis5:4 fis5 g5 a5 | a5 g5 fis5 e5 |"
    melodie = "d5 d5 e5 fis5 | fis5:4. e5:8 e5:2 |"    # weitere Zeilen derselben Stimme folgen
}

form = "a a"                                 # Pflicht: Reihenfolge der Teile
```

**Noten:** deutsche Namen `c cis des d dis es e eis fes f fis ges g gis as a ais b h his ces` mit
Oktave (`c4` = eingestrichenes c, `a4` = 440 Hz), Dauer nach `:` als Bruchteil der Ganzen (`1 2 4 8 16
32`), `.` punktiert, `t` Triole, `~` an die nächste gleiche Note gebunden (auch über den Taktstrich).
Ohne Dauer gilt die vorige. `r` ist eine Pause, `[c4 e4 g4]:2` ein Akkord, `|` ein Taktstrich.

**Prüfungen gegen Abschreibfehler:** Jeder Takt zwischen dem ersten und dem letzten einer Stimme muss
genau so lang sein wie `takt` (Auftakt und Schlusstakt dürfen kürzer sein); alle Stimmen eines Teils
sind gleich lang. Fehler nennen Datei, Zeile, Stimme und Takt.

## Die Stücke

| Datei | Werk | Stimmung | Quelle der Noten |
|---|---|---|---|
| `ode_an_die_freude` | Beethoven, 9. Sinfonie (1824), Hauptthema | frieden | eigener Satz |
| `kaiserlied` | Haydn, „Gott erhalte Franz den Kaiser“ (1797), Satz Mandyczewski | frieden | Peters 1915 über Mutopia (CC BY 4.0) |
| `loreley` | Silcher nach Heine (um 1837) | frieden | Mutopia (gemeinfrei) |
| `fuer_elise` | Beethoven, WoO 59 (1810), Hauptteil | krise | Breitkopf & Härtel 1888 über Mutopia (gemeinfrei) |
| `wilder_reiter` | Schumann, Album für die Jugend op. 68 Nr. 8 (1848) | krise | Peters über Mutopia (CC BY-SA 2.5) |
| `soldatenmarsch` | Schumann, Album für die Jugend op. 68 Nr. 2 (1848) | krieg | Peters über Mutopia (CC BY-SA 2.5) |
| `marseillaise` | Rouget de Lisle (1792), Lied der Revolution von 1848 | revolution | Bignon 1792 über Mutopia (gemeinfrei) |

Die Werke sind gemeinfrei. Die Notensätze des [Mutopia-Projekts](https://www.mutopiaproject.org)
dienten nur zum Abgleich von Tonhöhen und Dauern; der Notentext hier ist neu geschrieben und
arrangiert (Instrumentierung, Formen, eigene Bass- und Trommelstimmen), Kopf jeder Datei nennt Werk,
Ausgabe und Satz. Für das Kaiserlied ist der vierstimmige Satz von Mandyczewski (gestorben 1929)
übernommen; er ist gemeinfrei.

## Bedienklänge

| Klang | Anlass im Spiel |
|---|---|
| `gaensekiel` | Befehl (Steuern, Gesetze, Ereigniswahl) |
| `siegel` | Vertrag (Frieden, Bündnis, Zollbeitritt) |
| `papier` | Fenster öffnen |
| `glocke` | Ereignis |
| `trommel` | Kriegserklärung |
| `telegraf` | Meldung (ab 1849, als der Telegraf die Nachrichten bringt) |
