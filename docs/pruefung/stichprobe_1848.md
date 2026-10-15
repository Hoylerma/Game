# Stichprobe zum Lesezeichen 1848

Meilenstein M7, Schritt a6. Diese Stichprobe ist die dokumentierte Prüfung nach Entscheidung 7 in
[lesezeichen_1848.md](../technik/lesezeichen_1848.md). Gezogen werden 60 Datensätze. Dazu kommen
Summenprüfungen der Fläche je Staat, Provinz und Kronland.

Stand: 15.10.2026, Daten nach Commit „M7, Schritt a6: Befunde der Stichprobe …“.

## 1 Zweck und Grenzen

- **Kein Quellenabgleich.** Wikipedia, Wikidata, GenWiki und OpenHistoricalMap sind aus der
  Arbeitsumgebung nicht erreichbar. Die Prüfvermerke halten deshalb fest, ob ein Wert nach
  Fachwissen plausibel ist; ein Abgleich mit einer Quelle ist das nicht. Die Daten behalten ihre
  Sicherheit `ungeprueft` bzw. `schaetzung`. Die Fachprüfung durch einen Menschen oder den Beirat
  steht weiter aus (Entscheidung 7).
- **Reproduzierbar.** `python3 tools/geodaten/stichprobe_1848.py --seed 1848` zieht dieselben
  Datensätze, solange sich Zahl und Reihenfolge der Tabellenzeilen nicht ändern. Die Tabellen in
  Abschnitt 5 sind die Ausgabe dieses Aufrufs mit den Prüfvermerken.
- **Stand nach den Korrekturen.** Die Tabellen zeigen die Werte nach der Behebung der Befunde. Bei
  einem behobenen Fehler nennt der Prüfvermerk den alten Wert.

## 2 Verfahren

| Teil | Grundgesamtheit | Anzahl |
|---|---|---|
| Besitz, Bundesgebiet, Verwaltung | `besitz.csv`, Zeilen zum 1.3.1848 (3.967) | 20 |
| Bevölkerung | `bevoelkerung.csv`, Zeilen zum 3.12.1846 (3.967) | 20 |
| Verwaltungseinheiten | `verwaltung.csv` (458) | 10 |
| Staatsfelder | `staaten_historie.csv`, ohne Spielwerte, Datum nach 1700 bis 1.3.1848 | 10 |

Prüfvermerke:

- **plausibel**: passt zum Fachwissen.
- **Vereinfachung**: Der Wert ist ein bewusst grobes Mittel (Land, Provinz) und für den Ort
  ungenau. Er ist kein Fehler im Sinne der Regeln.
- **Fehler, behoben**: Der Wert war falsch und ist korrigiert.
- **zweifelhaft**: Er ist ohne Quelle nicht zu entscheiden und ist ein Prüfauftrag für die
  Fachprüfung.

## 3 Ergebnis

| Teil | plausibel | Vereinfachung | Fehler, behoben | zweifelhaft |
|---|---|---|---|---|
| Besitz (B) | 15 | – | 4 | 1 |
| Bevölkerung (E) | 13 | 5 | 1 | 1 |
| Verwaltung (V) | 10 | – | – | – |
| Staaten (S) | 8 | 1 | 1 | – |
| **Summe** | **46** | **6** | **6** | **2** |

Sechs von 60 Datensätzen (10 %) waren falsch. Vier davon gehen auf eine Methode zurück und nicht
auf einzelne Werte: Kreise nach dem nächsten Kreissitz. Ein weiterer, E11, ist ein
Werkzeugfehler. Die Fehlerquote der Einzelwerte ist damit klein. Die Ursachen sind behoben (Abschnitt 4).

## 4 Befunde und Folgen

1. **Konfession in sieben Landschaften (E11).** In `merkmale_1846.csv` standen Konfession und
   Dichte für Münsterland, Mark/Sauerland, Nieder- und Oberschlesien, Glatz, Ost- und
   Westpreußen in je zwei Zeilen. Beim Nachschlagen verdeckte die zweite (nur Dichte) die erste,
   und es galt das preußische Staatsmittel (95 % lutherisch). Betroffen waren 517 Orte. Die Zeilen
   sind zusammengeführt. `bevoelkerung_1846.py` lehnt doppelte Gebiete jetzt ab (Test).
2. **Kreise in Ober- und Niederösterreich (B11–B13).** Der nächste Kreissitz scheitert dort an
   Donau und Wienerwald: Das Salzkammergut lag im Innkreis, Tulln im Viertel unter dem
   Manhartsberg, das Marchfeld im Viertel unter dem Wienerwald. Die Viertel richten sich jetzt
   nach den heutigen Bezirken, deren Grenzen ihnen weitgehend folgen. An der Donau geteilte
   Bezirke haben Ortsregeln. Der Sitz des Viertels unter dem Manhartsberg ist Stockerau statt
   Klosterneuburg (südlich der Donau). 39 Orte wechselten den Kreis. Die Kreise heißen jetzt
   „Mühlkreis“ usw. statt „Kreis Mühlkreis“.
3. **Einzelne Kreise (B4 und ein Beifund).** Marienwalde gehört zum Kreis Arnswalde. Beifund:
   Auschwitz lag im Wadowicer Kreis, nicht im Krakauer Kreis (dieser umfasst den einstigen
   Freistaat Krakau).
4. **Herrscherwechsel Sachsen-Altenburg (S5).** Herzog Joseph folgte am 29.9.1834, nicht am 29.8.
5. **Vereinfachungen (E3, E4, E12–E14, S9).** Kultur und Konfession sind Mittel über Landschaften,
   Regierungsbezirke oder Länder. Die Sprachgrenze in Böhmen und deutsche Kreise in Posen sind
   nicht abgebildet. Das ist für M7a in Ordnung. Eine Verfeinerung braucht Quellen auf
   Kreisebene (Abschnitt 7).

Nach den Korrekturen sind alle Prüfungen grün: Python-Tests, `--pruefen` von Archivar und
Werkzeugen, 14 CTests einschließlich `daten_pruefen_1848` und `determinismus_1848`.

## 5 Die 60 Datensätze

Seed 1848; Zeilennummern der Tabellen in data/tabellen/.

### Besitz, Bundesgebiet, Verwaltung (besitz.csv)

| Nr. | Zeile | Ort | Staat | Bund | Verwaltung | Prüfvermerk |
|---|---|---|---|---|---|---|
| B1 | 197 | Allensbach | BAD | ja | Seekreis | plausibel – am Bodensee, badischer Seekreis |
| B2 | 445 | Dolní Bousov | AUT | ja | Kreis Bidschow · Königreich Böhmen | zweifelhaft – Grenzlage zwischen Bidschower und Bunzlauer Kreis (heute Bezirk Jungbunzlau); Quelle nötig |
| B3 | 516 | Weseritz | AUT | ja | Kreis Pilsen · Königreich Böhmen | plausibel – Pilsner Kreis |
| B4 | 704 | Bierzwnik | PRU | ja | Kreis Arnswalde · Regierungsbezirk Frankfurt · Provinz Brandenburg | **Fehler, behoben** – war Kreis Friedeberg (nächster Sitz); Marienwalde gehörte zum Kreis Arnswalde |
| B5 | 1128 | Elmshorn | DEN | ja | Herzogtum Holstein | plausibel – Holstein: Bundesgebiet unter dem dänischen König als Herzog |
| B6 | 1147 | Lauenburg | DEN | ja | Herzogtum Lauenburg | plausibel – Lauenburg: Bundesgebiet unter dem dänischen König als Herzog |
| B7 | 1180 | Feistritz an der Drau | AUT | ja | Villacher Kreis · Herzogtum Kärnten | plausibel – Oberkärnten |
| B8 | 1608 | Strelitz | AUT | ja | Kreis Brünn · Markgrafschaft Mähren | plausibel – Střelice bei Brünn (nicht das mecklenburgische Strelitz) |
| B9 | 1746 | Niederstetten | WUE | ja | Jagstkreis | plausibel – Oberamt Gerabronn |
| B10 | 1775 | Schwerte | PRU | ja | Kreis Dortmund · Regierungsbezirk Arnsberg · Provinz Westfalen | plausibel – Schwerte gehörte bis 1887 zum Kreis Dortmund |
| B11 | 2033 | Erpersdorf | AUT | ja | Viertel ober dem Wienerwald · Erzherzogtum Österreich unter der Enns | **Fehler, behoben** – war Viertel ober dem Manhartsberg (nächster Sitz Krems); Erpersdorf liegt südlich der Donau im Tullner Feld |
| B12 | 2314 | Grünau im Almtal | AUT | ja | Traunkreis · Erzherzogtum Österreich ob der Enns | **Fehler, behoben** – war Hausruckkreis; das Almtal (heute Bezirk Gmunden) gehörte zum Traunkreis |
| B13 | 2315 | Mondsee | AUT | ja | Hausruckkreis · Erzherzogtum Österreich ob der Enns | **Fehler, behoben** – war Innkreis; jetzt Hausruckkreis wie der heutige Bezirk Vöcklabruck. Ob das Mondseeland 1816–1849 zum Hausruck- oder zum Salzburgkreis gehörte, ist mit Quelle zu prüfen |
| B14 | 2372 | Velburg | BAV | ja | Kreis Oberpfalz und Regensburg | plausibel |
| B15 | 2538 | Papenburg | HAN | ja | Landdrostei Osnabrück | plausibel – Amt Aschendorf |
| B16 | 2681 | Geierswalde | PRU | nein | Kreis Osterode · Regierungsbezirk Königsberg · Provinz Preußen | plausibel – Ostpreußen war am 1.3.1848 nicht im Bund; die Aufnahme im April 1848 liegt nach dem Startdatum |
| B17 | 3451 | Voitsberg | AUT | ja | Grazer Kreis · Herzogtum Steiermark | plausibel – Weststeiermark |
| B18 | 3487 | Gleinstätten | AUT | ja | Marburger Kreis · Herzogtum Steiermark | plausibel – Raum Leibnitz |
| B19 | 3495 | Sankt Leonhard | AUT | ja | Marburger Kreis · Herzogtum Steiermark | plausibel – Lenart (St. Leonhard in Windischbüheln) |
| B20 | 3677 | Flirsch | AUT | ja | Kreis Oberinntal · Gefürstete Grafschaft Tirol mit Vorarlberg | plausibel – bei Landeck, Kreissitz Imst |

### Bevölkerung 1846 (bevoelkerung.csv)

| Nr. | Zeile | Ort | Einwohner | je km² | Konfession | Kultur | Prüfvermerk |
|---|---|---|---|---|---|---|---|
| E1 | 224 | Bernried | 5.048 | 37 | katholisch=100 | Standard (bayern) | plausibel – Dichte wie Altbayern; 100 % katholisch ist gerundet (eher 99 %) |
| E2 | 274 | Fürstenzell | 6.635 | 46 | katholisch=100 | Standard (bayern) | plausibel – wie E1 |
| E3 | 489 | Lubenz | 11.259 | 67 | katholisch=96 juedisch=2 reformiert=1 lutherisch=1 | Standard (boehmen) | Vereinfachung – Kultur ist das Landesmittel Böhmens (70 % Tschechen); Lubenz lag im deutschsprachigen Gebiet bei Saaz |
| E4 | 494 | Hlinsko | 11.192 | 67 | katholisch=96 juedisch=2 reformiert=1 lutherisch=1 | Standard (boehmen) | Vereinfachung – Landesmittel; Hlinsko war fast rein tschechisch |
| E5 | 1007 | Geslau | 5.684 | 58 | lutherisch=72 katholisch=26 juedisch=2 | Standard (franken) | plausibel – Mittelfranken |
| E6 | 1019 | Burgsalach | 8.990 | 58 | lutherisch=72 katholisch=26 juedisch=2 | Standard (franken) | plausibel – Mittelfranken |
| E7 | 1028 | Kenty | 17.368 | 125 | katholisch=88 juedisch=12 | Standard (galizien) | zweifelhaft – 125 Einw./km² ist für ländliches Westgalizien hoch; die Summe für au_galizien (300.000 auf 2.258 km²) mit Quelle prüfen |
| E8 | 1695 | Hammelburg | 7.888 | 58 | katholisch=82 lutherisch=15 juedisch=3 | Standard (mainfranken) | plausibel – Unterfranken |
| E9 | 1709 | Haßfurt | 13.945 | 67 | katholisch=82 lutherisch=15 juedisch=3 | Standard (mainfranken) | plausibel – Unterfranken |
| E10 | 2012 | Gaweinstal | 11.586 | 57 | katholisch=97 lutherisch=1 reformiert=1 juedisch=1 | Standard (niederoesterreich) | plausibel – Weinviertel |
| E11 | 2410 | Neustadt in Oberschlesien | 10.233 | 93 | katholisch=90 lutherisch=8 juedisch=2 | Standard (oberschlesien) | **Fehler, behoben** – war lutherisch=95 (preußisches Staatsmittel): eine zweite Zeile für dieselbe Landschaft verdeckte die oberschlesische |
| E12 | 2504 | Wilhelmshaven | 4.668 | 39 | lutherisch=75 katholisch=24 juedisch=1 | Standard (oldenburg) | Vereinfachung – Landesmittel Oldenburgs; das Jeverland war fast rein lutherisch. Ortsname heutig (Wilhelmshaven erst 1869) |
| E13 | 2508 | Zetel | 7.945 | 34 | lutherisch=75 katholisch=24 juedisch=1 | Standard (oldenburg) | Vereinfachung – wie E12 |
| E14 | 2962 | Schlichtingsheim | 11.879 | 51 | katholisch=65 lutherisch=29 juedisch=6 | polen=60 maerker=40 | Vereinfachung – Provinzmittel Posens; der Kreis Fraustadt war mehrheitlich deutsch und evangelisch |
| E15 | 2993 | Oppenheim | 21.623 | 117 | katholisch=50 lutherisch=46 juedisch=4 | Standard (rheinhessen) | plausibel – Rheinhessen: dicht besiedelt, konfessionell gemischt |
| E16 | 3300 | Dinkelscherben | 6.786 | 52 | katholisch=86 lutherisch=13 juedisch=1 | Standard (schwaben) | plausibel – Bayerisch-Schwaben |
| E17 | 3330 | Stetten am Kalten Markt | 4.817 | 50 | katholisch=95 lutherisch=5 | Standard (schwaben) | plausibel – badischer Seekreis |
| E18 | 3379 | Scheidegg | 4.928 | 42 | katholisch=86 lutherisch=13 juedisch=1 | Standard (schwaben) | plausibel – Westallgäu |
| E19 | 3440 | Leoben | 8.754 | 46 | katholisch=99 lutherisch=1 | Standard (steiermark) | plausibel – Obersteiermark, Stadt mit Umland |
| E20 | 3930 | Heilbronn | 25.445 | 159 | lutherisch=90 katholisch=9 juedisch=1 | Standard (wuerttemberg) | plausibel – Neckarkreis, Stadt mit Umland |

### Verwaltungseinheiten (verwaltung.csv)

| Nr. | Zeile | Einheit | Art | darüber | Sitz | Prüfvermerk |
|---|---|---|---|---|---|---|
| V1 | 6 | Provinz Schlesien | provinz | PRU | – | plausibel – Sitz (Breslau) wird für Provinzen nicht erhoben |
| V2 | 47 | Unterrheinkreis | kreis | BAD | – | plausibel – 1832–1864; Sitz (Mannheim) nicht erhoben |
| V3 | 130 | Kreis Schildberg | kreis | Regierungsbezirk Posen | Ostrzeszów | plausibel – Sitz unter dem heutigen Namen; der deutsche Name Schildberg fehlt in orte.csv |
| V4 | 142 | Kreis Gnesen | kreis | Regierungsbezirk Bromberg | Gnesen | plausibel |
| V5 | 189 | Kreis Potsdam | kreis | Regierungsbezirk Potsdam | Potsdam | plausibel – Stadtkreis Potsdam |
| V6 | 198 | Kreis Sternberg | kreis | Regierungsbezirk Frankfurt | Zielenzig | plausibel – ungeteilter Kreis bis 1873; Sitz Zielenzig mit Quelle prüfen |
| V7 | 216 | Kreis Militsch | kreis | Regierungsbezirk Breslau | Militsch | plausibel |
| V8 | 243 | Kreis Landeshut | kreis | Regierungsbezirk Liegnitz | Landeshut | plausibel |
| V9 | 291 | Kreis Merseburg | kreis | Regierungsbezirk Merseburg | Merseburg | plausibel |
| V10 | 293 | Kreis Naumburg | kreis | Regierungsbezirk Merseburg | Naumburg an der Saale | plausibel |

### Staaten (staaten_historie.csv, ohne Spielwerte)

| Nr. | Zeile | Staat | ab | Feld | Wert | Prüfvermerk |
|---|---|---|---|---|---|---|
| S1 | 211 | Baden (BAD) | 1848.03.01 | konfession | lutherisch | plausibel – das Feld meint das Herrscherhaus (evangelisch, seit 1821 uniert); die Bevölkerung war zu zwei Dritteln katholisch, siehe merkmale_1846.csv |
| S2 | 241 | Braunschweig-Wolfenbüttel (BWO) | 1831.04.20 | herrscher | Wilhelm | plausibel – Regierung seit 1830, Herzog seit 20.4.1831 |
| S3 | 257 | Oldenburg (OLD) | 1848.03.01 | konfession | lutherisch | plausibel |
| S4 | 272 | Sachsen-Meiningen (SMG) | 1848.03.01 | konfession | lutherisch | plausibel |
| S5 | 276 | Sachsen-Altenburg (ALT) | 1834.09.29 | herrscher | Joseph | **Fehler, behoben** – war 1834.08.29; Herzog Friedrich starb am 29.9.1834 |
| S6 | 295 | Schwarzburg-Sondershausen (SSH) | 1848.03.01 | hauptstadt | sondershausen | plausibel |
| S7 | 303 | Schwarzburg-Rudolstadt (SRU) | 1848.03.01 | konfession | lutherisch | plausibel |
| S8 | 328 | Schaumburg-Lippe (SLI) | 1848.03.01 | konfession | lutherisch | plausibel |
| S9 | 331 | Hohenzollern-Hechingen (HOH) | 1848.03.01 | regierungsform | absolute_monarchie | Vereinfachung – vor 1848 keine Repräsentativverfassung |
| S10 | 351 | Hamburg (HAM) | 1848.03.01 | hauptstadt | hamburg | plausibel |

## 6 Summenprüfung Fläche

Die Fläche eines Staats ist die Summe der Flächen seiner Orte aus der Karte. Sie hängt nicht von
den Eingabetabellen der Bevölkerung ab und ist deshalb ein unabhängiger Prüfwert für die
Zuordnung. Die **Richtwerte sind gerundete Werte aus Fachwissen ohne Quellenzugang**. Gemeint ist
der Gebietsstand um 1848, bei Österreich nur die Länder im Kartenausschnitt (Summe der Kronländer in 6.2 und
rund 2.300 km² in Galizien). Urteil: bis ±5 % gut,
bis ±15 % akzeptabel, darüber auffällig.

### 6.1 Staaten

| Staat | Orte | Fläche (km²) | Einwohner 1846 | Richtwert (km²) | Abweichung | Urteil |
|---|---|---|---|---|---|---|
| Preußen (PRU) | 1329 | 274.919 | 15.973.000 | 274.000 | ±0 % | gut |
| Österreich (AUT) | 983 | 185.153 | 11.571.000 | 185.700 | ±0 % | gut |
| Bayern (BAV) | 508 | 77.611 | 4.504.000 | 76.300 | +2 % | gut |
| Hannover (HAN) | 225 | 37.448 | 1.773.000 | 38.500 | −3 % | gut |
| Württemberg (WUE) | 143 | 19.720 | 1.726.000 | 19.500 | +1 % | gut |
| Dänemark (DEN) | 118 | 18.970 | 889.000 | 18.750 | +1 % | gut |
| Kursachsen (SAX) | 103 | 15.247 | 1.836.000 | 15.000 | +2 % | gut |
| Baden (BAD) | 115 | 14.985 | 1.367.000 | 15.100 | −1 % | gut |
| Mecklenburg-Schwerin (MSC) | 62 | 13.118 | 530.000 | 13.300 | −1 % | gut |
| Kurhessen (HEK) | 63 | 9.409 | 754.000 | 9.600 | −2 % | gut |
| Hessen-Darmstadt (HED) | 68 | 8.984 | 852.000 | 8.400 | +7 % | akzeptabel |
| Oldenburg (OLD) | 40 | 6.617 | 280.000 | 6.400 | +3 % | gut |
| Nassau (NAS) | 29 | 4.399 | 421.000 | 4.700 | −6 % | akzeptabel |
| Braunschweig-Wolfenbüttel (BWO) | 28 | 4.307 | 268.000 | 3.700 | +16 % | **auffällig** |
| Sachsen-Weimar (WEI) | 22 | 3.336 | 261.000 | 3.600 | −7 % | akzeptabel |
| Luxemburg (LUX) | 20 | 3.168 | 190.000 | 2.600 | +22 % | **auffällig** |
| Niederlande (NED) | 16 | 2.379 | 210.000 | 2.200 | +8 % | akzeptabel |
| Sachsen-Meiningen (SMG) | 11 | 2.195 | 160.000 | 2.470 | −11 % | akzeptabel |
| Mecklenburg-Strelitz (MST) | 8 | 2.019 | 96.000 | 2.930 | −31 % | **auffällig** |
| Anhalt-Dessau (DES) | 9 | 1.608 | 107.000 | 1.450 | +11 % | akzeptabel |
| Sachsen-Coburg und Gotha (SCG) | 8 | 1.417 | 146.000 | 1.960 | −28 % | **auffällig** |
| Sachsen-Altenburg (ALT) | 9 | 1.415 | 131.000 | 1.320 | +7 % | akzeptabel |
| Waldeck (WAL) | 6 | 1.137 | 58.000 | 1.120 | +2 % | gut |
| Schwarzburg-Rudolstadt (SRU) | 6 | 1.137 | 68.000 | 940 | +21 % | **auffällig** |
| Reuß j. L. (REJ) | 5 | 843 | 80.000 | 830 | +2 % | gut |
| Lippe (LIP) | 5 | 811 | 104.000 | 1.130 | −28 % | **auffällig** |
| Schwarzburg-Sondershausen (SSH) | 4 | 794 | 59.000 | 860 | −8 % | akzeptabel |
| Hohenzollern-Sigmaringen (HSI) | 4 | 646 | 46.000 | 900 | −28 % | **auffällig** |
| Anhalt-Bernburg (BER) | 3 | 604 | 49.000 | 840 | −28 % | **auffällig** |
| Hamburg (HAM) | 3 | 470 | 188.000 | 410 | +15 % | akzeptabel |
| Schaumburg-Lippe (SLI) | 3 | 464 | 29.000 | 340 | +36 % | **auffällig** |
| Hohenzollern-Hechingen (HOH) | 2 | 322 | 21.000 | 300 | +7 % | akzeptabel |
| Reuß ä. L. (REA) | 2 | 296 | 35.000 | 316 | −6 % | akzeptabel |
| Bremen (BRE) | 2 | 260 | 75.000 | 255 | +2 % | gut |
| Hessen-Homburg (HHO) | 2 | 248 | 24.000 | 275 | −10 % | akzeptabel |
| Lübeck (LUE) | 1 | 130 | 47.000 | 300 | −57 % | **auffällig** |
| Liechtenstein (LIE) | 1 | 129 | 6.500 | 160 | −19 % | **auffällig** |
| Frankfurt (FRA) | 1 | 58 | 68.000 | 100 | −42 % | **auffällig** |

### 6.2 Provinzen und Kronländer

| Einheit | Staat | Orte | Fläche (km²) | Einwohner 1846 | Richtwert (km²) | Abweichung | Urteil |
|---|---|---|---|---|---|---|---|
| Königreich Böhmen | AUT | 264 | 51.070 | 4.348.000 | 51.950 | −2 % | gut |
| Herzogtum Steiermark | AUT | 123 | 23.370 | 1.004.000 | 22.450 | +4 % | gut |
| Gefürstete Grafschaft Tirol mit Vorarlberg | AUT | 114 | 23.127 | 530.000 | 22.400 | +3 % | gut |
| Markgrafschaft Mähren | AUT | 121 | 21.286 | 1.786.000 | 22.230 | −4 % | gut |
| Erzherzogtum Österreich unter der Enns | AUT | 112 | 19.746 | 1.494.000 | 19.820 | ±0 % | gut |
| Erzherzogtum Österreich ob der Enns | AUT | 101 | 18.706 | 852.000 | 19.130 | −2 % | gut |
| Herzogtum Kärnten | AUT | 48 | 9.917 | 318.000 | 10.330 | −4 % | gut |
| Herzogtum Krain | AUT | 47 | 9.554 | 463.000 | 9.950 | −4 % | gut |
| Herzogtum Schlesien | AUT | 40 | 6.119 | 476.000 | 5.150 | +19 % | **auffällig** |
| Königreich Galizien und Lodomerien | AUT | 13 | 2.258 | 300.000 | – | – | nur Auschwitz, Zator und Teile des einstigen Freistaats Krakau |
| Provinz Preußen | PRU | 225 | 63.428 | 2.438.000 | 62.500 | +1 % | gut |
| Provinz Brandenburg | PRU | 174 | 40.784 | 2.029.000 | 39.850 | +2 % | gut |
| Provinz Schlesien | PRU | 203 | 39.823 | 3.062.000 | 40.300 | −1 % | gut |
| Provinz Posen | PRU | 141 | 29.866 | 1.364.000 | 28.950 | +3 % | gut |
| Provinz Pommern | PRU | 130 | 29.601 | 1.165.000 | 30.100 | −2 % | gut |
| Rheinprovinz | PRU | 183 | 26.206 | 2.754.000 | 27.000 | −3 % | gut |
| Provinz Sachsen | PRU | 132 | 24.359 | 1.722.000 | 25.250 | −4 % | gut |
| Provinz Westfalen | PRU | 141 | 20.852 | 1.439.000 | 20.200 | +3 % | gut |

**Bewertung.**

- Die großen Staaten liegen fast alle innerhalb von ±5 %. Preußen weicht um weniger als 1 % ab;
  jede preußische Provinz liegt innerhalb von ±5 %.
- Tirol enthält ohne Welschtirol (Trentino), Buchenstein und Ampezzo nur den Teil im
  Kartenausschnitt; der Richtwert ist entsprechend gekürzt.
- Auffällig sind zwei Gruppen:
  - Österreichisch-Schlesien (+19 %). Vermutlich liegen Grenzorte Mährens oder Preußisch-Schlesiens
    im falschen Land.
  - Die Kleinstaaten. Ein Ort ist im Mittel 150–200 km² groß; bei Staaten mit zwei bis acht Orten
    verschiebt ein Ort mehr oder weniger die Fläche um 20–60 %. Das gehört zur Körnung der Karte
    und ist kein Zuordnungsfehler.
- Ebenfalls auffällig: Braunschweig (+16 %) und Luxemburg (+22 %). Hier sind wohl einzelne Grenzorte
  falsch zugeordnet. Das ist ein Prüfauftrag.

## 7 Einwohner

Die Einwohner je Staat, Provinz und Kronland sind **Eingaben** (`einwohner_1846.csv`). Das
Werkzeug verteilt sie auf die Orte, und die Summen stimmen daher per Konstruktion. Eine Prüfung
der Eingaben gegen die Zollvereinszählung vom 3.12.1846 und die österreichische Konskription
gehört zur Fachprüfung. Die Eingaben stammen aus demselben Fachwissen wie die Richtwerte; ein
Vergleich hier wäre zirkulär. Zur Orientierung: Preußen 15,97 Mio. (Literatur um 16,1 Mio.),
Bayern 4,50 Mio., Sachsen 1,84 Mio., Württemberg 1,73 Mio., Baden 1,37 Mio. Zusammen sind es
45,0 Mio. auf der Karte.

## 8 Offene Punkte für die Fachprüfung

1. Quellenabgleich der 60 Datensätze. Vorrang haben die zweifelhaften: B2 (Dolní Bousov),
   B13 (Mondseeland), E7 (Summe Galizien), V6 (Sitz Kreis Sternberg).
2. Einwohnersummen in `einwohner_1846.csv` gegen die Zählungen von 1846.
3. Fläche von Österreichisch-Schlesien, Braunschweig und Luxemburg: Grenzorte prüfen.
4. Sprachgrenze in Böhmen, Mähren und Posen sowie die Konfession auf Kreisebene. Dafür braucht
   es Kreisstatistiken; Kandidat für M7b oder einen späteren Datenschritt.
5. Die übrigen Kronländer mit Kreisen nach dem nächsten Sitz (Böhmen, Mähren, Steiermark, Tirol,
   Krain) stichprobenartig gegen historische Kreiskarten prüfen.
