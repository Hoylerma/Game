# 07 – Bevölkerung & Gesellschaft

## 1. Das Pop-Modell

Die Bevölkerung wird – wie in *Victoria 3* – in **Pop-Blöcken** simuliert: Gruppen gleichartiger Menschen an einem Ort. EINIGKEIT erweitert das Modell um Dimensionen, die für die deutsche Geschichte entscheidend sind.

| Dimension | Ausprägungen (Beispiele) | Wofür sie wichtig ist |
|---|---|---|
| **Ort** | einer von ~4.500 | Wirtschaft, Wahlkreis, Verwaltung |
| **Beruf / Schicht** | 20 Berufsgruppen in drei Schichten | Einkommen, Bedürfnisse, politische Interessen |
| **Konfession** | katholisch, lutherisch, reformiert, uniert, jüdisch, konfessionslos, muslimisch … | Politik, Konflikte, Bildung, Milieu |
| **Milieu** | katholisch, sozialistisch, liberal-bürgerlich, konservativ-agrarisch, später weitere | Parteibindung, Vereinswesen, Stabilität |
| **Kultur / Landsmannschaft** | Bayern, Schwaben, Sachsen, Westfalen, Friesen … sowie Minderheiten | Identität, Loyalität, Partikularismus |
| **Sprache** | Hochdeutsch-Dialekträume, Niederdeutsch, Sorbisch, Dänisch, Polnisch, Jiddisch … | Integration, Minderheitenpolitik |
| **Altersgruppe** | Kinder (0–14), Junge (15–29), Erwerbsalter (30–64), Alte (65+) | Wehrpflicht, Arbeitskräfte, Renten, Demografie |
| **Bildung** | Analphabet, Grundbildung, höhere Bildung, akademisch | Produktivität, Politisierung, Innovation |
| **Wohlstand** | Lebensstandard 0–100 | Zufriedenheit, Nachfrage, Gesundheit |
| **Bewusstsein** | politische Mobilisierung 0–100 | Teilnahme an Wahlen, Protest, Bewegungen |
| **Unzufriedenheit** | 0–100 | Radikalisierung, Unruhen, Revolutionen |
| **Frauenanteil / Erwerbsbeteiligung** | Anteil je Block | Frauenwahlrecht, Arbeitsmarkt, Emanzipation |

**Größenordnung:** Je Ort entstehen typischerweise 20–120 Blöcke; im Spätspiel insgesamt 250.000–600.000 (siehe [02 Engine](02_engine_und_technik.md)).

## 2. Berufe und Schichten im Wandel

Die Berufsgruppen bleiben systemisch gleich, tragen aber epochentypische Bezeichnungen und Eigenschaften.

| # | Berufsgruppe | Schicht | 1648–1806 | 1806–1918 | 1918–1990 | ab 1990 |
|---|---|---|---|---|---|---|
| 1 | Herrschaftsstand | oben | Fürstenhaus, Hofstaat | Monarchen, Standesherren | – | – |
| 2 | Grundbesitzer | oben | Landadel, Rittergutsbesitzer | Junker, Großgrundbesitzer | Großagrarier (im Osten bis 1945) | Agrarunternehmer |
| 3 | Kapitaleigner | oben | Patrizier, Großkaufleute, Bankiers | Industrielle, Bankiers | Unternehmer, Manager (BRD) | Unternehmer, Investoren |
| 4 | Geistlichkeit | mittel/oben | Hoher und niederer Klerus | Pfarrer, Bischöfe | Geistliche | Geistliche |
| 5 | Beamte | mittel | Kammerbeamte, Amtleute | Beamtenschaft | Beamte; in der DDR Staatsfunktionäre | Beamte |
| 6 | Offiziere | mittel/oben | Offiziere (überwiegend Adel) | Offizierskorps | Offiziere | Offiziere |
| 7 | Gebildete | mittel | Gelehrte, Ärzte, Juristen | Bildungsbürgertum | Akademiker, Freiberufler; „Intelligenz“ (DDR) | Wissensarbeiter |
| 8 | Händler | mittel | Kaufleute, Krämer | Händler, Gastwirte | Einzelhandel, Selbstständige | Selbstständige |
| 9 | Handwerker | mittel/unten | Zunftmeister und Gesellen | Handwerker | Handwerk | Handwerk |
| 10 | Angestellte | mittel | – | Angestellte (stark wachsend ab ca. 1880) | Angestellte | Angestellte, Dienstleister |
| 11 | Facharbeiter | unten/mittel | Manufakturarbeiter | Facharbeiter | Facharbeiter | Facharbeiter |
| 12 | Ungelernte | unten | Tagelöhner | Fabrikarbeiter | Hilfsarbeiter | Geringqualifizierte |
| 13 | Bergleute | unten | Bergknappen | Bergarbeiter | Bergarbeiter | (Restbestand) |
| 14 | Bauern | mittel/unten | Hufner/Vollbauern (frei oder erbuntertänig) | Bauern | Landwirte; LPG-Mitglieder (DDR) | Landwirte |
| 15 | Landarme | unten | Kossäten/Kötter, Häusler, Gesinde | Landarbeiter, Instleute | Landarbeiter | – |
| 16 | Dienstboten | unten | Gesinde | Dienstmädchen, Knechte | Hausangestellte | Haushaltshilfen |
| 17 | Soldaten | unten | Söldner, Kantonisten | Wehrpflichtige | Soldaten | Berufs- und Zeitsoldaten |
| 18 | Studierende | – | Studenten | Studenten | Studierende | Studierende |
| 19 | Rentner | – | – | Invaliden- und Altersrentner | Rentner | Rentner |
| 20 | Erwerbslose | – | Bettler, Vaganten | Arbeitslose | Arbeitslose | Erwerbslose |

**Rechtsstatus** ist eine eigene Eigenschaft, kein Beruf: z. B. *erbuntertänig*, *Schutzjude*, *Bürger mit Bürgerrecht*, *Staatsbürger*, *Ausländer*, *Vertriebener*, *Asylsuchender*, *Spätaussiedler*.

### 2.1 Transformationsereignisse

Große Reformen verwandeln Pops direkt:

| Reform | Wirkung |
|---|---|
| **Bauernbefreiung** (Preußen ab 1807, Ablösung bis 1850) | Erbuntertänige werden frei; Teile werden Landbesitzer, viele landlose Landarbeiter („Regulierung“ gegen Landabgabe) |
| **Gewerbefreiheit** (Preußen 1810/11, Norddeutscher Bund 1869) | Zunftschranken fallen; Handwerker werden Unternehmer oder Arbeiter |
| **Judenemanzipation** (Preußen 1812 teilweise, 1869/71 rechtliche Gleichstellung) | Rechtsstatus „Schutzjude“ entfällt |
| **Bodenreform** (SBZ 1945) | Großgrundbesitz über 100 ha enteignet; Neubauern entstehen |
| **Kollektivierung** (DDR 1952–1960) | Bauern werden LPG-Mitglieder |
| **Treuhand-Privatisierung** (1990–1994) | Massenhafte Erwerbslosigkeit in Ostdeutschland, Umschulungen, Abwanderung |

## 3. Lebensstandard und Bedürfnisse

Jede Schicht hat **Bedürfnisgruppen**, die sie mit epochentypischen Gütern deckt. Güter können einander ersetzen.

| Bedürfnis | 1648–1806 | 1806–1918 | 1918–1990 | ab 1990 |
|---|---|---|---|---|
| **Grundnahrung** | Getreide, Brei, Brot | Brot, **Kartoffeln** | Grundnahrungsmittel | Lebensmittel |
| **Genussmittel** | Bier, Branntwein, Tabak | Bier, Kaffee (Zichorie als Ersatz), Zucker, Tabak | Kaffee, Zigaretten, Süßwaren | Genussmittel |
| **Kleidung** | Leinen, Wolle | Baumwolle, Konfektion | Konfektion | Kleidung, Mode |
| **Wärme und Licht** | Holz, Torf, Talglicht | Kohle, Petroleum, Gas | Kohle, Strom, Heizöl | Strom, Gas, Wärmepumpe |
| **Wohnen** | Hütte, Bürgerhaus | Mietskaserne, Villa | Neubauwohnung, Plattenbau, Eigenheim | Wohnraum (Mietpreis als Faktor) |
| **Haushalt** | Werkzeug, Geschirr | Möbel, Haushaltswaren | Elektrogeräte | Elektronik |
| **Information** | Kalender, Flugschrift | Zeitung, Buch | Radio, Fernsehen | Internet, Smartphone |
| **Mobilität** | Fuß, Pferd | Eisenbahn, Fahrrad | Auto (Käfer, Trabant), Urlaubsreise | Auto, Bahn, Flugreise |
| **Luxus** | Porzellan, Wein, Kutsche | Reisen, Kunst | Fernreisen, Markenartikel | Premiumgüter |
| **Sicherheit** | Kirche, Familie | Versicherung, Sparbuch | Sozialversicherung | Sozialstaat, Altersvorsorge |

**Relative Deprivation:** Zufriedenheit hängt nicht nur vom absoluten Lebensstandard ab, sondern vom **Vergleich** – mit der Vergangenheit („Früher ging es uns besser“), mit anderen Schichten und (in der Teilungszeit) mit dem anderen deutschen Staat. Dieses Prinzip ist der Kern des Systemwettbewerbs (siehe [17 Teilung & Einheit](17_teilung_und_einheit.md)).

## 4. Kultur und Identität

### 4.1 Drei Ebenen der Zugehörigkeit

1. **Landsmannschaft / Region:** Bayern, Franken, Schwaben, Alemannen, Pfälzer, Hessen, Thüringer, Sachsen, Westfalen, Rheinländer, Niedersachsen, Friesen, Holsteiner, Mecklenburger, Pommern, Märker, Schlesier, Ostpreußen, Österreicher, Tiroler, Deutschböhmen, Elsässer, Lothringer …
2. **Dynastische und staatliche Loyalität:** zu Wittelsbach, Hohenzollern, Wettin, den Welfen, Habsburg … später zum Land (Freistaat Bayern, Freistaat Sachsen) oder zum Staat (BRD/DDR).
3. **Nationalbewusstsein (0–100):** Wie stark sich Menschen als Teil einer deutschen Nation verstehen.

### 4.2 Wie Nationalbewusstsein entsteht

| Treiber | Beispiele |
|---|---|
| Gemeinsame Sprache und Literatur | Luther-Bibel (Erbe), Weimarer Klassik, *Deutsches Wörterbuch* der Brüder Grimm (ab 1838), Duden (1880) |
| Bildung | Schulpflicht, Gymnasium, Universitäten |
| Vereine und Feste | Turnbewegung, Burschenschaften (Wartburgfest 1817), Gesangvereine, Schützenfeste, Hambacher Fest (1832) |
| Gemeinsame Bedrohung | Napoleonische Herrschaft, Rheinkrise 1840 |
| Wirtschaftliche Integration | Zollverein, Eisenbahn, gemeinsame Währung |
| Symbole und Denkmäler | Kölner Dom (Vollendung 1880), Hermannsdenkmal (1875), Niederwalddenkmal (1883), Völkerschlachtdenkmal (1913) |
| Medien und Sport | Massenpresse, Rundfunk, Fußball-WM 1954 („Wunder von Bern“), 1990, 2006 |

Nationalbewusstsein ist **ambivalent**: Es stärkt die Einigkeit-Säule und erleichtert Einigungen, kann aber in **Nationalismus** umschlagen, der Minderheitenkonflikte und Kriegsbereitschaft erhöht. Das Spiel trennt deshalb *Nationalbewusstsein* (Zugehörigkeit) von *Nationalismus* (Ideologie, Zeitgeist).

### 4.3 Partikularismus

Hohe Regionalloyalität bei niedriger Nationalloyalität erzeugt **Partikularismus** – das Gegenstück zur Einigkeit. Beispiele: Welfentreue in Hannover nach 1866 (Deutsch-Hannoversche Partei), bayerischer Eigensinn (Patriotenpartei, später Bayernpartei), rheinischer Separatismus (1919/1923).

### 4.4 Minderheiten

| Minderheit | Siedlungsgebiet | Epochen | Themen im Spiel |
|---|---|---|---|
| **Sorben/Wenden** | Ober- und Niederlausitz | durchgehend | Sprachschutz, Kultur, Germanisierungsdruck, Domowina |
| **Dänen** | Nordschleswig, Südschleswig | durchgehend | Schleswig-Frage, Volksabstimmung 1920, Bonn-Kopenhagener Erklärungen (1955), SSW |
| **Friesen** | Nord- und Ostfriesland, Saterland | durchgehend | Sprache, Regionalidentität |
| **Polen** | Posen, Westpreußen, Oberschlesien; „Ruhrpolen“ | v. a. 1772–1945 | Germanisierungspolitik, Kulturkampf, Ostmarkenverein, Volksabstimmungen 1920/21 |
| **Masuren, Kaschuben, Litauer** | Ostpreußen, Westpreußen, Memelland | bis 1945 | Sprache, Konfession, Loyalität |
| **Elsässer und Lothringer** | Reichsland | 1871–1918 | Autonomie, Protestpartei, Zabern-Affäre |
| **Juden** | verteilt, städtisch | durchgehend | Schutzjudentum, Emanzipation, Antisemitismus, Verfolgung und Vernichtung (dokumentiert), Neuanfang nach 1945, Zuwanderung ab 1990 |
| **Sinti und Roma** | verteilt | durchgehend | Ausgrenzung, Verfolgung, Völkermord (dokumentiert), Anerkennung |
| **Hugenotten** | Brandenburg, Hessen, Franken | 1685–ca. 1800 | Einwanderung, Gewerbe, Assimilation |
| **Tschechen** | Böhmen, Mähren | bis 1918/1945 | Nationalitätenkonflikt im Habsburgerreich |
| **Gastarbeiter und ihre Nachkommen** | West- und Süddeutschland | ab 1955 | Anwerbung, Familiennachzug, Integration, Staatsbürgerschaft |
| **Vertragsarbeiter** | DDR | 1960er–1990 | Isolation, Rückführung, Bleiberecht |
| **Spätaussiedler** | verteilt | ab 1950, stark ab 1988 | Rückkehr, Integration |

## 5. Religion und Konfession

### 5.1 Konfessionen

Katholisch · Lutherisch · Reformiert · Uniert (Preußische Union ab 1817) · Altlutherisch · Täufer/Mennoniten · Herrnhuter · Jüdisch (später orthodox, liberal, reformiert) · Orthodox · Muslimisch · Konfessionslos · Freireligiös/Freidenker

Zusätzlich **Frömmigkeit** (0–100) als eigene Größe: Eine Region kann katholisch, aber kaum praktizierend sein.

### 5.2 Mechaniken

| Mechanik | Beschreibung |
|---|---|
| **Normaljahr 1624** | Besitzstand der Konfessionen ist reichsrechtlich geschützt; Verstöße führen zu Klagen und Interventionen |
| **Corpus Evangelicorum / Catholicorum** | Konfessionsblöcke im Reichstag (*itio in partes*) |
| **Konfessionsspannung** | Entsteht bei Mischbevölkerung, Konversion des Herrschers (z. B. Sachsen 1697), Missionierung |
| **Toleranzpolitik** | Einwanderung (Edikt von Potsdam 1685), Gewerbe, aber Widerstand der Landeskirche |
| **Kirchenunion** | Lutheraner und Reformierte vereinen (Preußen 1817) – Widerstand der Altlutheraner, Auswanderung |
| **Kulturkampf** | Staat gegen katholische Kirche (1871–1887): Kanzelparagraph, Jesuitengesetz, Maigesetze, Zivilehe – stärkt das Zentrum |
| **Säkularisierung** | Langfristig sinkende Frömmigkeit; Kirchenaustritte; in der DDR staatlich forciert (Jugendweihe) |
| **Staatskirchenrecht** | „Hinkende Trennung“ (1919), Kirchensteuer, Staatskirchenverträge, Religionsunterricht |

### 5.3 Konfessionelle Kalibrierungsdaten (gerundet)

| Jahr | Gebiet | Evangelisch | Katholisch | Jüdisch | Sonstige/keine |
|---|---|---|---|---|---|
| 1871 | Deutsches Reich | ca. 62 % | ca. 36 % | ca. 1,2 % | < 1 % |
| 1933 | Deutsches Reich | ca. 63 % | ca. 33 % | ca. 0,8 % | ca. 4 % |
| 1950 | BRD | ca. 51 % | ca. 45 % | – | ca. 4 % |
| 1950 | DDR | ca. 80 % | ca. 11 % | – | ca. 9 % |
| 1989 | DDR | ca. 25 % | ca. 5 % | – | ca. 70 % |
| um 2021 | Deutschland | Kirchenmitglieder insgesamt erstmals unter 50 % | | | Konfessionslose größte Gruppe; Muslime ca. 6–7 % |

## 6. Milieus

### 6.1 Das Konzept

Der Soziologe M. Rainer Lepsius beschrieb 1966 die deutsche Parteienlandschaft als Ausdruck von **vier sozialmoralischen Milieus** – stabilen Lebenswelten mit eigener Presse, eigenen Vereinen und eigener Partei. Das erklärt, warum Wahlergebnisse im Kaiserreich und in der Weimarer Republik erstaunlich stabil waren – und warum der Zusammenbruch eines Milieus so dramatische Folgen hatte.

| Milieu | Basis | Vorfeldorganisationen | Parteibindung (historisch) |
|---|---|---|---|
| **Katholisches Milieu** | Katholiken in Süd- und Westdeutschland, Schlesien | Katholikentage, Volksverein für das katholische Deutschland, Gesellenvereine (Kolping), christliche Gewerkschaften, katholische Presse | Zentrum / BVP → CDU/CSU |
| **Sozialistisches Arbeitermilieu** | Industriearbeiter in Städten und Revieren | Freie Gewerkschaften, Arbeitersport, Arbeitergesangvereine, Konsumgenossenschaften, Parteipresse | SPD; ab 1918 auch USPD/KPD |
| **Liberal-bürgerliches Milieu** | Protestantisches Bürgertum in Städten | Bildungsvereine, Handelskammern, Burschenschaften, liberale Presse | Fortschritt, Nationalliberale, Freisinn → DDP/DVP → FDP und Union |
| **Konservativ-agrarisches Milieu** | Protestantische Landbevölkerung, v. a. östlich der Elbe | Landeskirche, Kriegervereine, Bund der Landwirte | Deutschkonservative → DNVP → (ab 1930 Einbruch der NSDAP) → nach 1945 Union und Kleinparteien |

### 6.2 Spätere Milieus

| Milieu | Entstehung | Parteiaffinität (tendenziell) |
|---|---|---|
| **Postmaterialistisch-alternatives Milieu** | ab 1968/1970er | Grüne, Teile der SPD |
| **Aufstiegsorientierte Mitte** | ab 1950er | Volksparteien, FDP |
| **Ostdeutsche Nachwendeprägung** | ab 1990 | volatil; PDS/Linke, später andere |
| **Prekäre Lebenslagen** | ab 1980er | volatil, Nichtwahl, Protestparteien |
| **Digital-urbanes Milieu** | ab 2010er | Grüne, FDP, Volt, Piraten |

### 6.3 Mechaniken

- **Milieubindung (0–100):** Hohe Bindung macht Wahlentscheidungen stabil und Pops resistent gegen Radikalisierung – solange „ihre“ Partei Halt gibt.
- **Vorfeldorganisationen** sind Gebäude-ähnliche Objekte (Vereinsnetz je Ort), die Bindung erzeugen und Mobilisierung erleichtern.
- **Milieuerosion** durch Urbanisierung, Bildungsexpansion, Mobilität, Fernsehen, Konsumgesellschaft und Säkularisierung. Ab ca. 1960 sinkt die Bindung → **Wechselwähler** entstehen.
- **Milieuzerfall unter Druck:** Bricht ein Milieu in einer Krise (Inflation 1923, Agrarkrise ab 1927, Weltwirtschaftskrise), werden seine Wähler frei verfügbar – historisch ein Schlüssel zum Aufstieg der NSDAP im protestantisch-bürgerlichen und ländlichen Raum, während das katholische und das sozialistische Milieu weitgehend stabil blieben.
- **Zerstörung durch Diktatur:** Die NS-Herrschaft zerschlägt Organisationen; die DDR ersetzt Milieus durch staatliche Massenorganisationen. Nach 1945 erstehen das katholische und das sozialdemokratische Milieu im Westen neu.

## 7. Demografie

### 7.1 Grundmechanik

- Geburten und Sterbefälle je Pop-Block nach Alter, Wohlstand, Gesundheit, Bildung, Konfession und Zeitgeist.
- **Demografischer Übergang:** Erst sinkt die Sterblichkeit (Hygiene, Medizin, Ernährung), dann mit Verzögerung die Geburtenrate.
- **Altersstruktur** bestimmt Wehrpflichtige, Arbeitskräfte und Rentenlast.

### 7.2 Schocks

| Typ | Beispiele |
|---|---|
| **Seuchen** | Pest (1679 Wien, 1709–1711 Preußen/Ostpreußen), Pocken, Cholera (1831, 1892 Hamburg), Spanische Grippe (1918/19), Corona (2020–2022) |
| **Hungerkrisen** | 1770–1772, „Jahr ohne Sommer“ 1816/17, 1846/47, Steckrübenwinter 1916/17, Hungerwinter 1946/47 |
| **Kriegsverluste** | Dreißigjähriger Krieg (Startlage), Siebenjähriger Krieg, Napoleonische Kriege, Erster Weltkrieg (ca. 2 Mio. gefallene deutsche Soldaten), Zweiter Weltkrieg |
| **Geburtenknicke** | Weltkriege, „Pillenknick“ (ab Mitte der 1960er), Einbruch in Ostdeutschland nach 1990 |

### 7.3 Kalibrierungsziele Bevölkerung (gerundet)

| Jahr | Bezugsgebiet | Bevölkerung |
|---|---|---|
| 1648 | Gebiet des späteren Kaiserreichs | ca. 10–12 Mio. (Schätzung) |
| 1700 | dto. | ca. 14–15 Mio. (Schätzung) |
| 1816 | dto. | ca. 25 Mio. |
| 1850 | dto. | ca. 35 Mio. |
| 1871 | Deutsches Reich | ca. 41 Mio. |
| 1900 | Deutsches Reich | ca. 56 Mio. |
| 1910 | Deutsches Reich | ca. 65 Mio. |
| 1925 | Deutsches Reich (nach Versailles) | ca. 62 Mio. |
| 1939 | Reichsgebiet in den Grenzen von 1937 | ca. 69 Mio. |
| 1950 | BRD / DDR | ca. 50 Mio. / ca. 18 Mio. |
| 1970 | BRD / DDR | ca. 61 Mio. / ca. 17 Mio. |
| 1989 | BRD / DDR | ca. 63 Mio. / ca. 16 Mio. |
| 1990 | Deutschland | ca. 80 Mio. |
| 2024 | Deutschland | ca. 83–84 Mio. |

## 8. Migration

### 8.1 Wanderungsarten

| Art | Beispiele | Mechanik |
|---|---|---|
| **Peuplierung** (staatlich gelenkte Ansiedlung) | Hugenotten nach Brandenburg (ab 1685; um 1700 etwa jeder fünfte Einwohner Berlins), Salzburger Exulanten nach Ostpreußen (1731/32, rund 20.000), Kolonisten im Oderbruch | Kostet Geld und Toleranz; bringt Fachkräfte und Steuerzahler |
| **Auswanderung** | Mehr als fünf Millionen Deutsche zwischen 1816 und 1914, überwiegend in die USA; „Forty-Eighters“ nach der Revolution | Folge von Armut, Repression, Agrarkrisen; senkt Unzufriedenheit, kostet Arbeitskräfte; Rücküberweisungen |
| **Binnenwanderung** | „Ostflucht“ vom Land ins Ruhrgebiet und nach Berlin, Verstädterung | Folgt Löhnen und Arbeitsplätzen |
| **Arbeitsmigration** | Ruhrpolen, italienische Ziegler; Gastarbeiter 1955–1973 (rund 14 Mio. kamen, rund 11 Mio. kehrten zurück); DDR-Vertragsarbeiter (1989 rund 90.000); EU-Freizügigkeit | Anwerbeabkommen als diplomatische Aktion; Integration als Langzeitaufgabe |
| **Flucht und Vertreibung** | 12–14 Mio. Deutsche aus den Ostgebieten und Ostmitteleuropa (1944–1950) | Massiver Schock: Wohnungsnot, Lastenausgleich (1952), Integration, eigene Interessengruppe (BdV) und Partei (BHE) |
| **Deutsch-deutsche Wanderung** | Rund 2,7 Mio. Menschen verlassen die DDR vor dem Mauerbau; Ausreisebewegung der 1980er; Ost-West-Wanderung nach 1990 | Kern des Systemwettbewerbs |
| **Aussiedler und Spätaussiedler** | Über vier Millionen seit 1950 | Rechtsanspruch, Integration |
| **Asyl und Schutzsuche** | Jugoslawienkriege (1990er), rund 1,2 Mio. Asylanträge 2015/16, über eine Million Geflüchtete aus der Ukraine (ab 2022) | Asylrecht, Verteilung auf Länder und Kommunen, Integration, politische Polarisierung |
| **Exil** | Vormärz-Demokraten, verfolgte Sozialisten, Emigration 1933–1945 (Wissenschaftler, Künstler, Verfolgte), DDR-Ausbürgerungen | Brain Drain, Exilorganisationen |

### 8.2 Integration

Jeder zugewanderte Pop-Block hat einen **Integrationswert** (Sprache, Arbeit, Bildung, Staatsbürgerschaft, soziale Kontakte). Integration hängt von Politik (Sprachkurse, Arbeitserlaubnis, Einbürgerungsrecht), Wirtschaftslage und gesellschaftlicher Offenheit ab. Gelungene Integration erhöht Wohlstand und Innovationskraft; misslungene erzeugt Spannungen, die politische Akteure nutzen können.

## 9. Bildung

| Element | Wirkung |
|---|---|
| **Schulpflicht** (frühe Formen: Sachsen-Gotha 1642, Preußen 1717/1763) | Alphabetisierung steigt langsam |
| **Volksschule, Gymnasium, Realschule** | Verschiedene Bildungsniveaus der Pops |
| **Universitäten** und **Technische Hochschulen** | Innovationskraft, Bildungsbürgertum |
| **Humboldtsche Bildungsreform** (ab 1809/10) | Forschungsuniversität, großer Innovationsbonus |
| **Duale Berufsausbildung** | Produktivität der Facharbeiter (ein deutsches Markenzeichen) |
| **Bildungsexpansion** (1960er/70er) | Mehr Abiturienten und Akademiker, Milieuerosion |
| **Einheitsschule (DDR: POS/EOS)** | Hohe Grundbildung, politische Auslese beim Hochschulzugang |
| **Bildungsrankings** (ab 2000: „PISA-Schock“) | Landesbildungspolitik im Ländermodus |

## 10. Gesundheit

Lebenserwartung und Arbeitsfähigkeit hängen von Ernährung, Wohnen, Hygiene und Medizin ab. Meilensteine: Pockenimpfung (Bayern führt 1807 eine Impfpflicht ein), städtische Kanalisation, Robert Kochs Bakteriologie, Krankenversicherung (1883), Antibiotika, Impfkampagnen. Skandale und Krisen (z. B. Contergan 1957–1961) erscheinen als Ereignisse mit Folgen für Regulierung.

## 11. Politisierung und Protest

### 11.1 Bewusstsein und Unzufriedenheit

- **Bewusstsein** steigt mit Bildung, Urbanisierung, Presse, Vereinen und Krisen.
- **Unzufriedenheit** steigt, wenn der Lebensstandard sinkt, Erwartungen enttäuscht werden oder Gruppen ausgegrenzt sind.
- Hohes Bewusstsein + hohe Unzufriedenheit = **Mobilisierung**.

### 11.2 Protesteskalation

| Stufe | Frühe Formen | Moderne Formen |
|---|---|---|
| 1 | Bittschrift, Supplik | Petition, Leserbrief |
| 2 | Versammlung, Gravamina der Stände | Kundgebung, Demonstration |
| 3 | Arbeitsniederlegung, Abgabenverweigerung | Streik, Boykott, Sitzblockade |
| 4 | Brotkrawall, Tumult (z. B. Kartoffelrevolution 1847) | Unruhen, Straßenschlachten |
| 5 | Bauernaufstand, Weberaufstand (1844) | Aufstand (17. Juni 1953), Generalstreik (gegen den Kapp-Putsch 1920) |
| 6 | Revolution (1848) | Revolution (1918), friedliche Revolution (1989) |

Ob eine Stufe Erfolg hat, hängt von Mobilisierung, Spaltung der Eliten, Loyalität der Sicherheitskräfte und ausländischer Rückendeckung ab (Details in [09 Politik](09_politik_staat_und_gesetze.md)).

## 12. Frauen und Geschlechterverhältnisse

- Pop-Blöcke tragen einen **Frauenanteil** und eine **Erwerbsbeteiligung von Frauen**.
- Rechtliche Stufen (Gesetzesgruppe „Frauenrechte“): Vormundschaft → Vereinsrecht (Reichsvereinsgesetz 1908 erlaubt Parteimitgliedschaft) → Wahlrecht (1918) → Gleichberechtigung (Art. 3 GG, 1949; Gleichberechtigungsgesetz 1957) → Eherechtsreform (1977: kein Zustimmungsvorbehalt des Ehemanns mehr für Erwerbstätigkeit) → Gleichstellungspolitik.
- Die DDR erreicht früh eine hohe Frauenerwerbsquote; die BRD lange ein „Hausfrauenmodell“ – beides hat Folgen für Arbeitskräfte, Geburten und Rentensystem.
- Frauen erscheinen als **Persönlichkeiten** in Politik, Bewegungen, Wissenschaft und Kultur, sobald die rechtlichen und gesellschaftlichen Bedingungen es erlauben – und vereinzelt schon vorher (z. B. Maria Theresia, Louise Otto-Peters, Clara Zetkin).

## 13. Zivilgesellschaft

Die **Vereinsdichte** je Ort (Turn-, Gesang-, Schützen-, Sport-, Kirchen-, Bildungs-, Wohltätigkeitsvereine, Gewerkschaften, Genossenschaften, Bürgerinitiativen) ist ein eigener Wert:

- Sie stärkt **Demokratiefähigkeit**, Mobilisierung und Krisenresilienz.
- Sie ist aber **neutral**: Auch antidemokratische Bewegungen nutzen Vereinsnetze (Unterwanderung bürgerlicher Vereine in der späten Weimarer Republik).
- Diktaturen zerschlagen oder vereinnahmen Vereine (Gleichschaltung, Massenorganisationen der DDR); in Nischen (Kirchen, Umweltgruppen) entsteht Gegenöffentlichkeit.
