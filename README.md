# EINIGKEIT
### …und Recht und Freiheit · Deutschland von 1648 bis heute

> **Ein historisches Grand-Strategy-Spiel**, das die Karten-, Dynastie- und Kriegsstrategie von *Europa Universalis* mit der Gesellschafts-, Wirtschafts- und Politiksimulation von *Victoria 3* verbindet – konzentriert auf **ein** Land und **vier Jahrhunderte**: vom Flickenteppich aus über 300 Reichsständen nach dem Dreißigjährigen Krieg über Kaiserreich, Weimarer Republik, Diktatur, Weltkriege, Teilung und Wiedervereinigung bis zur Berliner Republik der Gegenwart.

**Dokumentstatus:** Game Design Document (GDD) · Version 0.1 · Konzeptphase
**Codename:** *Projekt Paulskirche*

---

## Auf einen Blick

| | |
|---|---|
| **Genre** | Historische Grand Strategy, Echtzeit mit Pause |
| **Vorbilder** | Europa Universalis IV/V, Victoria 3, Hearts of Iron IV, Crusader Kings III, *Through the Darkest of Times* |
| **Zeitraum** | 24. Oktober 1648 (Westfälischer Frieden) bis 1. Januar 2050 – historische Inhalte bis 2025, danach „Offene Zukunft“; Endlosmodus optional |
| **Startdaten** | 15 Lesezeichen von 1648 bis 2025 |
| **Karte** | ca. 4.500 „Orte“ im deutschen Kernraum, ca. 650 Provinzen im übrigen Europa, abstrakte Welt-Schauplätze (Kolonien, Seekrieg, Fernfronten) |
| **Spielbar** | 1648: über 300 Reichsstände (24 Fokus-Nationen mit eigenen Inhalten) · später Deutscher Bund, Kaiserreich, Weimarer Republik, BRD, DDR, alle Länder/Bundesländer |
| **Perspektiven** | Staatsmodus · Ländermodus (z. B. Bayern in der Bundesrepublik) · Parteienmodus · Opposition & Widerstand |
| **Engine** | Godot 4 (MIT-Lizenz) als Präsentationsschicht + eigener deterministischer C++20-Simulationskern „Uhrwerk“ |
| **Plattformen** | Windows, Linux (inkl. Steam Deck), macOS |
| **Multiplayer** | bis 16 Spieler – u. a. „Reichstag“ (300 Reichsstände), „16 Länder“, Parteienmodus, BRD gegen DDR |
| **Modding** | Vollständig: Skriptsprache „Chronik-Skript“, Karteneditor, Ereigniseditor, Steam Workshop |
| **Sprachen** | Deutsch (Leitsprache), Englisch; weitere geplant |
| **Spielzeit** | Große Kampagne 100–200 Stunden · einzelne Epochen 10–40 Stunden |
| **Altersfreigabe (Ziel)** | USK 12–16 (abhängig von der Bewertung der dokumentarischen NS-Inhalte), Schulfassung mit Begleitmaterial |

## Die drei Säulen

| **EINIGKEIT** | **RECHT** | **FREIHEIT** |
|---|---|---|
| Raum, Zusammenhalt, Nation: Territorien, Föderalismus, die deutsche Frage, Teilung und Einheit | Institutionen und Verfassung: Gesetze, Parlamente, Rechtsstaat, Legitimität | Gesellschaft und Teilhabe: Bevölkerung, Parteien, Wahlen, Grundrechte, Wohlstand |

Ergänzt durch zwei Querschnittsprinzipien: **Geschichte als lebendiges System** (historisch genau, aber offen) und **Verantwortung** (keine Verklärung, Opfer sichtbar, Konsequenzen spürbar).

---

## Inhaltsverzeichnis

### Grundlagen
| Nr. | Dokument | Inhalt |
|---|---|---|
| 00 | [Vision & Pitch](docs/00_vision_und_pitch.md) | Kernidee, EU×Victoria-Mischung, Designsäulen, Alleinstellungsmerkmale, Zielgruppe |
| 01 | [Name & Marke](docs/01_name_und_marke.md) | Titel, Alternativen, Logo, Key Art, Taglines, Markenrecht |
| 02 | [Engine & Technik](docs/02_engine_und_technik.md) | Engine-Vergleich, Architektur, Simulationskern, Kartenrenderer, Netzcode, Tools |
| 03 | [Kernspielablauf](docs/03_kernspielablauf.md) | Spielerrollen, Spielschleifen, Ressourcen, Beispielpartien, Wertung |
| 04 | [Zeitrahmen, Epochen & Startdaten](docs/04_zeitrahmen_epochen_startdaten.md) | 8 Epochen, 15 Lesezeichen, Mechanik-Phasen, Zeitgeist, Weltbühne |

### Welt & Gesellschaft
| Nr. | Dokument | Inhalt |
|---|---|---|
| 05 | [Karte & Geographie](docs/05_karte_und_geographie.md) | Kartenebenen, Orte, Gelände, Flüsse, Rohstoffe, Städte, Verwaltung im Wandel, Kartenmodi |
| 06 | [Staaten & Nationen](docs/06_staaten_und_nationen.md) | Reichsstände 1648, 24 Fokus-Nationen, formbare Staaten, Bruchstellen, Personalunionen |
| 07 | [Bevölkerung & Gesellschaft](docs/07_bevoelkerung_und_gesellschaft.md) | Pop-Modell, Stände und Klassen, Konfessionen, Milieus, Demografie, Migration |
| 08 | [Wirtschaft & Finanzen](docs/08_wirtschaft_und_finanzen.md) | Güter, Gebäude, Märkte, Zollverein, Geld & Währung, Krisen, Planwirtschaft |

### Politik
| Nr. | Dokument | Inhalt |
|---|---|---|
| 09 | [Politik, Staat & Gesetze](docs/09_politik_staat_und_gesetze.md) | Regierungsformen, Stände, Interessengruppen, 36 Gesetzesgruppen, Verfassungskonvente, Diktaturen |
| 10 | [Parlamente, Wahlen & Koalitionen](docs/10_parlamente_wahlen_koalitionen.md) | Parlamente im Wandel, Wahlrecht, Wahlsimulation, Wahlabend, Koalitionsverhandlungen |
| 11 | [Parteien-Datenbank](docs/11_parteien_datenbank.md) | Alle relevanten Parteien und Fraktionen von 1848 bis heute |
| 12 | [Föderalismus & Bundesländer](docs/12_foederalismus_und_bundeslaender.md) | Ländermodus, Bundesrat, alle historischen Länder, 16 Bundesländer im Profil |
| 13 | [Persönlichkeiten](docs/13_persoenlichkeiten.md) | Charaktersystem, Herrscher, Kanzler, Präsidenten, Politiker, Militär, Widerstand, Wissenschaft |

### Macht & Krieg
| Nr. | Dokument | Inhalt |
|---|---|---|
| 14 | [Diplomatie](docs/14_diplomatie.md) | Reichspolitik, Bündnisse, Krisen, Friedenskonferenzen, Kalter Krieg, EU-Ebene |
| 15 | [Militär & Kriegsführung](docs/15_militaer_und_kriegsfuehrung.md) | Emergente Fronten, Mobilmachung, Logistik, See- und Luftkrieg, Abschreckung |
| 16 | [Die Weltkriege](docs/16_weltkriege.md) | Erster und Zweiter Weltkrieg im Detail, NS-Zeit-Design, Widerstand |
| 17 | [Teilung & Einheit](docs/17_teilung_und_einheit.md) | Besatzung, BRD und DDR, Systemwettbewerb, Friedliche Revolution, Wiedervereinigung |

### Inhalte, Ethik & Systeme
| Nr. | Dokument | Inhalt |
|---|---|---|
| 18 | [Ereignisse, Journale & Missionen](docs/18_ereignisse_journale_missionen.md) | Ereignissystem, Wendepunkte, Missionsbäume, Beispielskripte, Chronik |
| 19 | [Technologie, Wissenschaft & Kultur](docs/19_technologie_wissenschaft_kultur.md) | Innovationsbaum, Universitäten, Brain Drain, Kultur, Sport |
| 20 | [Verantwortung & Erinnerungskultur](docs/20_verantwortung_und_erinnerungskultur.md) | Leitlinien für NS-Zeit, Holocaust, Kolonialismus, DDR-Unrecht; Recht, USK, Beirat |
| 21 | [Künstliche Intelligenz](docs/21_kuenstliche_intelligenz.md) | KI-Architektur, Persönlichkeiten, Regime-KI, Weltbühne-KI, Tests |
| 22 | [UI/UX, Grafik & Audio](docs/22_ui_ux_grafik_audio.md) | Bildschirme, Kartenstile je Epoche, Wahlabend, Zeitung, Musik |
| 23 | [Multiplayer & Modding](docs/23_multiplayer_und_modding.md) | Spielmodi, Netzcode, Skriptsprache, Werkzeuge, Workshop-Regeln |
| 24 | [Produktion, Roadmap & DLC](docs/24_produktion_roadmap_dlc.md) | Team, Budget, Meilensteine, Förderung, DLC-Plan, Risiken |

### Anhänge
| | Dokument | Inhalt |
|---|---|---|
| A | [Zeitleiste 1648–2025](docs/anhang/a_zeitleiste.md) | Ereignisgerüst für Inhalte und KI |
| B | [Glossar](docs/anhang/b_glossar.md) | Begriffe von „Ablösung“ bis „Zweitstimmendeckung“ |
| C | [Quellen & Daten](docs/anhang/c_quellen_und_daten.md) | Fachliteratur, Datenquellen, Lizenzen |

---

## Leseempfehlung

- **In 20 Minuten verstehen, worum es geht:** README → [00 Vision](docs/00_vision_und_pitch.md) → [03 Kernspielablauf](docs/03_kernspielablauf.md)
- **Was macht das Spiel einzigartig?** [12 Ländermodus](docs/12_foederalismus_und_bundeslaender.md), [10 Wahlen](docs/10_parlamente_wahlen_koalitionen.md), [15 Emergente Fronten](docs/15_militaer_und_kriegsfuehrung.md), [17 Teilung & Einheit](docs/17_teilung_und_einheit.md)
- **Für Programmierung:** [02 Engine](docs/02_engine_und_technik.md), [21 KI](docs/21_kuenstliche_intelligenz.md), [23 Modding](docs/23_multiplayer_und_modding.md)
- **Für Historiker und Content-Team:** [11 Parteien](docs/11_parteien_datenbank.md), [13 Persönlichkeiten](docs/13_persoenlichkeiten.md), [20 Verantwortung](docs/20_verantwortung_und_erinnerungskultur.md), [Anhang A](docs/anhang/a_zeitleiste.md)

## Hinweise zu diesem Dokument

- Alle Zahlenwerte für Spielmechaniken (Modifikatoren, Schwellen, Kosten) sind **Startwerte für das Balancing**, keine endgültigen Festlegungen.
- Historische Zahlen sind gerundet und vor der Umsetzung durch das Historiker-Team gegen die in [Anhang C](docs/anhang/c_quellen_und_daten.md) genannten Quellen zu prüfen.
- Die Datenbasis für die Gegenwart ist auf den Stand Mai 2025 (Startdatum des letzten Lesezeichens) eingefroren und wird per Update fortgeschrieben.
- „Erstmal nur Deutschland“: Die Architektur ist von Anfang an darauf ausgelegt, später weitere Länder als Nationen-Pakete zu ergänzen (siehe [24 Roadmap](docs/24_produktion_roadmap_dlc.md)).
