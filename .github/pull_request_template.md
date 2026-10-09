## Was ändert sich?

<!-- Kurze Beschreibung der Änderung und warum sie nötig ist. -->

## Bereich

- [ ] `core` – Simulationskern
- [ ] `cli` – Kommandozeile
- [ ] `bridge` – GDExtension
- [ ] `game` – Godot-Projekt
- [ ] `data` – Spielinhalte
- [ ] `tools` – Werkzeuge
- [ ] `docs` – Dokumentation
- [ ] `ci` / `repo`

## Prüfung

- [ ] Baut lokal (`cmake --build --preset dev`)
- [ ] Tests grün (`ctest --preset dev`)
- [ ] Determinismus unverändert oder bewusst geändert (`uhrwerk-cli determinismus`)
- [ ] Formatierung geprüft (`clang-format`)
- [ ] Bei Daten: Quellen angegeben bzw. als Schätzung/Testdaten gekennzeichnet
- [ ] Bei sensiblen Inhalten: Leitlinien aus `docs/20_verantwortung_und_erinnerungskultur.md` beachtet

## Hinweise für Reviewer

<!-- Worauf soll besonders geachtet werden? -->
