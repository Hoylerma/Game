# 03 – Kernspielablauf

## 1. Spielerrollen

EINIGKEIT bietet vier Perspektiven. Sie teilen sich dieselbe Simulation – nur der **Ausschnitt dessen, was der Spieler steuert**, ändert sich.

### 1.1 Staatsmodus (Standard)

**Du bist der Staat** – nicht eine Person, sondern die Summe seiner Führung: im Absolutismus der Fürst und sein Geheimer Rat, im Konstitutionalismus Monarch und Regierung, in der Demokratie die jeweilige Regierung.

| Du steuerst | Du steuerst nicht direkt |
|---|---|
| Gesetzesinitiativen, Haushalt, Steuern | Ob Parlamente zustimmen |
| Bauprojekte, Infrastruktur, Wirtschaftspolitik | Was Pops kaufen, wo sie hinziehen |
| Diplomatie, Bündnisse, Kriegserklärungen | Was andere Staaten tun |
| Armeen, Flotten, Feldzüge | Ob Soldaten meutern, wenn die Moral bricht |
| Ernennungen (Minister, Generäle, Beamte) | Wer gewählt wird |
| Reaktionen auf Ereignisse | Welche Ereignisse eintreten |

**Wichtig:** In parlamentarischen Systemen bleibt der Spieler auch nach einem Regierungswechsel am Steuer – aber das **Programm der neuen Regierung** bindet ihn (wie in *Victoria 3*). Wer gegen den Koalitionsvertrag handelt, verliert Rückhalt und riskiert Koalitionsbruch.

### 1.2 Ländermodus

**Du bist ein Land in einem Bund** – Bayern im Kaiserreich, Sachsen in der Weimarer Republik, Nordrhein-Westfalen in der Bundesrepublik.

- Verfügbar, sobald ein Bundesstaat mit eingeschränkter Souveränität der Länder existiert (ab 1867/1871; in der Bundesrepublik ab 1946/1949).
- Eigene Hebel: Landeshaushalt, Bildung, Polizei, Kultur, Landesentwicklung, Landesbanken, Kommunalpolitik, Bundesratsstimmen, eigene Landesverfassung.
- Einfluss auf das Ganze: Abstimmungen im Bundesrat, Parteiverbindungen, Ministerpräsidentenkonferenz, Klagen vor dem Verfassungsgericht, Kanzlerkandidaturen aus der Staatskanzlei.
- Ausführlich in [12 Föderalismus & Bundesländer](12_foederalismus_und_bundeslaender.md).

### 1.3 Parteienmodus

**Du bist eine Partei.** Verfügbar, sobald organisierte Parteien existieren (ab ca. 1867, in Vorformen ab 1848).

- In der Regierung: Du steuerst die Ressorts, die deiner Partei im Koalitionsvertrag zustehen, und das Kanzleramt, wenn du es hältst.
- In der Opposition: Anträge, Untersuchungsausschüsse, Bundesrat-Blockaden über eigene Länder, Verfassungsklagen, Kampagnen, Volksbegehren, Vorfeldorganisationen.
- Immer: Programm, Flügel, Personal, Mitglieder, Finanzen, Wahlkampf.
- Ideal im Multiplayer: Jede Partei ein Spieler (siehe [23 Multiplayer](23_multiplayer_und_modding.md)).

### 1.4 Opposition & Widerstand

**Du bist ein Netzwerk** in einer Diktatur: Widerstand und Exil 1933–1945, Bürgerbewegung in der DDR (besonders 1976–1989), Sonderszenarien (z. B. Vormärz-Demokraten unter den Karlsbader Beschlüssen).

- Kontakte und Zellen auf der Karte aufbauen, Informationen sammeln, verfolgte Menschen schützen, Flugschriften verbreiten, Verbindungen ins Ausland halten, den Umsturz vorbereiten.
- Gefahr: Entdeckung, Verhaftung, Hinrichtung von Charakteren – Verluste sind endgültig.
- Inspiriert von *Through the Darkest of Times*, aber eingebettet in die strategische Gesamtsimulation.

### 1.5 Perspektivwechsel und Bruchstellen

An den großen **Bruchstellen** der Geschichte bietet das Spiel einen Wechsel an:

| Bruchstelle | Typische Wahl |
|---|---|
| 1806 (Ende des Alten Reiches) | Weiterspielen als souveräner Rheinbundstaat / als Gegner Napoleons |
| 1867/1871 (Bundesstaat) | Weiter als **Land** im Bund oder Wechsel zum **Bund/Reich** |
| 1918/19 (Revolution) | Land (nun „Freistaat“) oder Reich |
| 1933 (Ende der Demokratie) | **Keine Fortsetzung der Staatsführung.** Wahl zwischen *Widerstand & Exil*, *Militärischer Führung* (ab Kriegsbeginn, mit eingeschränkter Kontrolle) oder *Chronik* (dokumentarischer Schnelldurchlauf bis 1945) – siehe [16 Weltkriege](16_weltkriege.md) |
| 1945 (Besatzung) | Ein Land in einer der Zonen (Ländermodus) |
| 1949 (Doppelstaatsgründung) | BRD, DDR oder ein Land |
| 1952 (Auflösung der DDR-Länder) | DDR oder ein westdeutsches Land |
| 1990 (Einheit) | Deutschland oder eines der 16 Länder |

Außerhalb des Ironman-Modus ist ein freier Wechsel jederzeit möglich.

## 2. Zeitskalen des Spiels

| Zeitskala | Was passiert | Spielerfahrung |
|---|---|---|
| **Tage** | Truppen marschieren, Schlachten, Krisen eskalieren, Wahlkampfendspurt | Reaktion, Taktik im Großen |
| **Monate** | Wirtschaftsbericht, Ereignisse, Gesetzesfortschritt, Stimmungsberichte | Steuerung, Feinjustierung |
| **Jahre** | Haushalt, Statistisches Jahrbuch, Demografie, Innovationen | Strategie |
| **Legislaturperioden** | Wahlen, Regierungsbildung, Koalitionsverträge | Politische Zyklen |
| **Generationen** | Thronfolgen, Milieuwandel, Erinnerungskultur | Langfristige Planung |
| **Epochen** | Epochen-Journal, Urteil der Geschichte, Mechanikwechsel | Große Bögen |

## 3. Die Kernschleife

```
        ┌─────────────────── LAGE ERFASSEN ────────────────────┐
        │ Karte · Kabinettsvorlage · Zeitung · Umfragen ·      │
        │ Statistisches Jahrbuch · Berichte der Landräte       │
        └───────────────────────────┬──────────────────────────┘
                                    ▼
        ┌─────────────────── ENTSCHEIDEN ──────────────────────┐
        │ Gesetz · Haushalt · Bauprojekt · Bündnis · Krieg ·   │
        │ Ernennung · Reaktion auf Ereignis                    │
        └───────────────────────────┬──────────────────────────┘
                                    ▼
        ┌─────────────────── DURCHSETZEN ──────────────────────┐
        │ Mehrheiten organisieren · Verwaltung auslasten ·     │
        │ Geld beschaffen · Truppen bewegen                    │
        └───────────────────────────┬──────────────────────────┘
                                    ▼
        ┌─────────────────── REAKTIONEN ───────────────────────┐
        │ Pops · Milieus · Interessengruppen · Parteien ·      │
        │ Nachbarn · Märkte · Zeitgeist                        │
        └───────────────────────────┬──────────────────────────┘
                                    ▼
                              NEUE LAGE  ──────► (von vorn)
```

Der Reiz entsteht aus der **Lücke zwischen Entscheiden und Durchsetzen**: Ein absolutistischer Fürst kann viel befehlen, aber wenig durchsetzen, wenn Verwaltung und Geld fehlen. Ein demokratischer Kanzler hat einen leistungsfähigen Staat, braucht aber für fast alles Mehrheiten.

## 4. Die Ressourcen des Staates

| Ressource | Was sie darstellt | Wichtigste Quellen | Wofür man sie braucht | Epochen |
|---|---|---|---|---|
| **Staatskasse** | Geld in der jeweiligen Währung | Steuern, Zölle, Domänen, Regalien, Anleihen, Notenpresse | Alles | alle |
| **Verwaltungskapazität** | Was der Staatsapparat leisten kann | Beamte, Ämter, Reformen, Innovationen, Digitalisierung | Gesetze umsetzen, Gebiete durchdringen, Steuern eintreiben, Institutionen betreiben | alle |
| **Durchdringung** (je Ort) | Wie stark der Staat vor Ort tatsächlich herrscht | Nähe zur Hauptstadt, Verkehr, Beamte, Loyalität | Steuerertrag, Rekrutierung, Gesetzeswirkung | alle, besonders 1648–1871 |
| **Autorität** | Durchsetzungskraft der Staatsgewalt | Regierungsform, Loyalität von Armee und Beamten | Dekrete, Zensur, Verbote, Notstandsmaßnahmen | alle; in Demokratien begrenzt |
| **Legitimität** | Anerkennung der Herrschaft | Erbrecht, Krönung, Verfassung, Wahlen, Erfolge, Übereinstimmung mit dem Zeitgeist | Stabilität; niedrige Legitimität → Unruhen, Staatsstreich, Revolution | alle |
| **Rückhalt** | Politisches Kapital der Regierung | Wahlsiege, Umfragen, Koalitionsdisziplin, Erfolge | Gesetze und Reformen durchbringen; verbraucht sich über die Amtszeit („Regierungsverschleiß“) | ab Konstitutionalismus |
| **Einfluss** | Diplomatisches Kapital | Diplomaten, Geltung, Handelsbeziehungen | Bündnisse, Verträge, Garantien, Einflusssphären | alle |
| **Mannschaften** | Rekrutierbare Soldaten | Bevölkerung, Wehrverfassung, Kriegsmüdigkeit | Armeen aufstellen und ergänzen | alle |
| **Geltung** | Ansehen und Rang | Siege, Kultur, Wissenschaft, Wirtschaftskraft | Rang unter den Mächten, diplomatisches Gewicht | alle |
| **Internationales Vertrauen** | Glaubwürdigkeit als Partner | Vertragstreue, Aufarbeitung, Kooperation, Berechenbarkeit | Integration (EWG/EU, NATO), Einheit 1990, Krisenhilfe | alle, entscheidend ab 1945 |
| **Innovationskraft** | Forschungsleistung | Universitäten, Akademien, Industrielabore, Bildungsniveau | Innovationen freischalten | alle |

## 5. Beispielpartien

### 5.1 Die erste Stunde als Kurbrandenburg (Oktober 1648)

*Lesezeichen „Westfälischer Frieden“, Staatsmodus, Kurfürst Friedrich Wilhelm.*

1. **Bestandsaufnahme.** Die Karte zeigt einen Streubesitz: die Mark Brandenburg im Zentrum, im Westen Kleve, Mark und Ravensberg, im Osten das Herzogtum Preußen (polnisches Lehen), dazu frisch zugesprochen Hinterpommern sowie die Stifte Halberstadt, Minden und Kammin; die Anwartschaft auf Magdeburg ist vertraglich gesichert. Der Kartenmodus „Durchdringung“ leuchtet fast überall rot. In der Mark sind viele Dörfer verlassen, die Bevölkerung ist stark dezimiert.
2. **Erstes Problem: Pommern.** Die Schweden halten noch Teile Hinterpommerns besetzt. Ein Journaleintrag „Das pommersche Erbe“ startet: Verhandeln (Einfluss) oder Druck aufbauen (Armee).
3. **Zweites Problem: die Stände.** Der kurmärkische Landtag (Prälaten, Ritterschaft, Städte) verweigert neue Steuern. Option: Ein *Landtagsrezess* – die Ritterschaft erhält Bestätigung ihrer Gutsherrschaft und Steuerfreiheiten, dafür bewilligt sie Geld für ein **stehendes Heer**. Der Spieler sieht im Tooltip: kurzfristig Geld und Armee, langfristig gestärkte Junker und schwächere Bauern. (Historisch: Rezess von 1653.)
4. **Drittes Problem: Menschen.** Die Mission „Peuplierung“ öffnet: Einwanderer anwerben (Niederländer, Schweizer, später Hugenotten). Das kostet Geld und Toleranz gegenüber anderen Konfessionen – das reformierte Herrscherhaus regiert ein lutherisches Land, Konfessionsspannung ist ein ständiger Begleiter.
5. **Ausblick.** Der Missionsbaum zeigt den großen Bogen: *Souveränität in Preußen* (gelingt historisch 1657/1660 im Spiel zwischen Schweden und Polen), *Der Sieg von Fehrbellin*, *Das Edikt von Potsdam*, *Die Königskrone*.

### 5.2 Bayern 1871: Land oder Reich?

*Lesezeichen „Das Reich“, Spieler war bisher das Königreich Bayern.*

Mit der Reichsgründung erscheint der **Bruchstellen-Dialog**:

- **Als Land weiterspielen:** Bayern behält seine **Reservatrechte** – eigenes Heer im Frieden, eigene Post und Eisenbahn, eigene Bier- und Branntweinsteuer, sechs Stimmen im Bundesrat. Neue Ziele: Wohlstand, Einfluss im Bund, bayerische Identität, das Verhältnis zur Krone (König Ludwig II. ist ein eigener, sehr eigenwilliger Faktor).
- **Zum Reich wechseln:** Der Spieler übernimmt die Reichsleitung (Reichskanzler Bismarck) mit allen Spannungen zwischen Preußen, Mittelstaaten, Zentrum und Liberalen.
- **Verweigern** (nur wenn Bayern 1870 nicht beigetreten ist und militärisch stark genug ist): Alternative Geschichte mit eigenständigem Süddeutschland.

### 5.3 Parteienmodus: Das Zentrum im März 1933

*Multiplayer-Szenario „Die Republik am Abgrund“, Spielerin führt die Zentrumspartei.*

Am 23. März 1933 tagt der Reichstag in der Krolloper. Die Regierung Hitler legt das „Ermächtigungsgesetz“ vor. Die Sozialdemokraten werden dagegen stimmen, die kommunistischen Abgeordneten sind verhaftet oder geflohen, SA und SS stehen im Saal.

- Die Spielerin sieht die Lage nüchtern aufbereitet: Sitzverteilung, nötige Zweidrittelmehrheit, Versprechen der Regierung (Bestand von Länder- und Kirchenrechten), Haltung der eigenen Fraktion, Drohkulisse.
- **Zustimmung** (historische Entscheidung) oder **Ablehnung** (dann fehlt die Zweidrittelmehrheit nur, wenn weitere Fraktionen folgen – das Spiel zeigt die realen Mehrheitsverhältnisse).
- Das Spiel verurteilt die Entscheidung nicht im Moment selbst. Die Konsequenzen zeigen sich – wie in der Wirklichkeit – in den folgenden Wochen: Gleichschaltung, Selbstauflösung der Parteien, Verfolgung.

### 5.4 Bonn, Oktober 1969

*Lesezeichen „Die Mauer“ (1961) weitergespielt, Staatsmodus Bundesrepublik.*

Die Bundestagswahl ist vorbei. Die Union ist stärkste Kraft, aber SPD und FDP haben zusammen eine knappe Mehrheit. Der **Koalitionsbildschirm** öffnet sich:

- Rechnerisch möglich: Große Koalition (fortsetzen) oder Sozialliberale Koalition.
- Die FDP ist gespalten (Nationalliberale gegen Sozialliberale) – ein Flügelkonflikt, der Abgeordnete kosten kann.
- Verhandelt werden: Kanzler, Ressorts (das Auswärtige Amt geht an die FDP), Kernvorhaben (Ostpolitik, Bildungsreform, Mitbestimmung).
- Bei der Kanzlerwahl braucht der Kandidat die Mehrheit der Mitglieder des Bundestages. Abweichler werden pro Abgeordnetem gewürfelt – historisch reichte es mit zwei Stimmen über der Mehrheit.

## 6. Spielhilfen

| Hilfe | Beschreibung |
|---|---|
| **Geschichtsstunde** | Tutorial-Kampagnen pro Epoche (z. B. „Die Zollvereinsverhandlungen“, „Eine Wahl im Kaiserreich“, „Das erste Bundesbudget“) |
| **Kabinettsvorlage** | Monatlicher Bericht der Minister: die drei dringendsten Probleme mit Handlungsvorschlägen |
| **Berater-Hinweise** | Kontextsensitive Tipps, abschaltbar |
| **Automatisierung** | Wirtschaft, Bauprojekte, Diplomatie oder Militär können der KI übergeben werden (mit Zielvorgaben) |
| **Verschachtelte Tooltips** | Jede Zahl erklärt sich selbst; Fachbegriffe öffnen Lexikon-Einträge |
| **Pausenregeln** | Automatische Pause bei Kriegserklärung, Wahlergebnis, Thronfolge, Krise … frei konfigurierbar |
| **Historischer Kompass** | Zeigt an, welche Option in einem Ereignis historisch gewählt wurde (abschaltbar) |

## 7. Schwierigkeit und Spieleinstellungen

| Einstellung | Optionen |
|---|---|
| **Geschichtsverlauf** | *Historisch* (KI und Ereignisse folgen eng der Geschichte) · *Plausibel* (Standard) · *Offen* (maximale Dynamik) |
| **KI-Stärke** | Fünf Stufen; die oberen erhalten keine unsichtbaren Boni, sondern bessere Planung |
| **Wirtschaftskomplexität** | Vollständig / vereinfacht (automatische Produktionsmethoden) |
| **Kriegsführung** | Direkte Steuerung / Fronten mit Befehlen / vollautomatisch |
| **Ironman** | Ein Spielstand, keine Konsole, Erfolge aktiv |
| **Dokumentarische Inhalte** | Immer aktiv; Anzeigeform (Text/Bild/Zeitzeugnis) wählbar, Inhaltshinweise vorab |

## 8. Spielende

Das Spiel endet am 1. Januar 2050 – oder läuft im Endlosmodus weiter. Ausgewertet werden die fünf **Vermächtnis-Dimensionen** (Einigkeit, Recht, Freiheit, Wohlstand, Geltung), die **Hypotheken** und die **Urteile der Geschichte** aller Epochen (siehe [00 Vision](00_vision_und_pitch.md)). Abschließend erzeugt das Spiel die **Chronik**: das Geschichtsbuch der Partie, exportierbar als PDF.
