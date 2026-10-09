# GDExtension-Brücke

Verbindet den Simulationskern **Uhrwerk** (`core/`) mit dem Godot-Projekt (`game/`). Die Brücke
stellt genau eine Klasse bereit, `UhrwerkSimulation` (erbt von `RefCounted`). Ihre Schnittstelle
ist in [`docs/technik/bridge-api.md`](../docs/technik/bridge-api.md) festgelegt.

## Aufbau

| Datei | Inhalt |
|---|---|
| `src/register_types.cpp` | Einstiegspunkt `uhrwerk_library_init`, registriert die Klasse auf Stufe `SCENE` |
| `src/uhrwerk_simulation.h/.cpp` | `UhrwerkSimulation`: besitzt eine `uhrwerk::Simulation`, übersetzt zwischen Godot- und Kern-Typen |
| `CMakeLists.txt` | Ziel `uhrwerk_bridge`; bindet `third_party/godot-cpp` ein und linkt den Kern statisch |

Regeln:

- Werte **in** die Simulation sind Ganzzahlen (z. B. Steuersatz in Prozent), damit keine
  Gleitkommazahlen den deterministischen Kern erreichen. Werte **aus** der Simulation dürfen
  `float` sein – sie dienen nur der Anzeige.
- Alle Methoden sind gegen ungültige Indizes und Aufrufe vor `load_scenario` abgesichert und
  liefern dann leere Werte.
- Zeichenketten aus dem Kern sind UTF-8 und werden mit `String::utf8` übernommen.

## Bauen

```bash
git submodule update --init --recursive   # einmalig: godot-cpp holen
cmake --preset bridge                     # RelWithDebInfo, godot-cpp-Ziel template_debug
cmake --build --preset bridge             # beim ersten Mal 5–15 Minuten (godot-cpp)
```

Die Bibliothek landet direkt in `game/bin/` (`libuhrwerk_bridge.so`, `uhrwerk_bridge.dll` bzw.
`libuhrwerk_bridge.dylib`) und wird über `game/bin/uhrwerk.gdextension` geladen. Sie ist
per `.gitignore` vom Repository ausgeschlossen.

## Prüfen

```bash
nm -D --defined-only game/bin/libuhrwerk_bridge.so | grep " T "   # nur uhrwerk_library_init
godot --headless --path game --script res://tests/smoke_test.gd   # muss „SMOKE OK“ ausgeben
```

## Fehlersuche

- **„GDExtension dynamic library not found“** beim Start von Godot: Die Brücke ist noch nicht
  gebaut. Das Projekt startet trotzdem und zeigt einen Hinweis.
- **Absturz in der Brücke:** Mit `cmake --preset bridge -DCMAKE_BUILD_TYPE=Debug` bauen und Godot
  im Debugger starten, z. B. `gdb --args godot --path game`.
- **Änderungen am Kern** werden beim nächsten `cmake --build --preset bridge` mitgebaut; Godot
  muss danach neu gestartet werden (`reloadable = false`).
- Fehlermeldungen der Brücke erscheinen in der Godot-Ausgabe als `ERROR: …` mit Quelldatei und
  Zeile, z. B. bei ungültigem Kartenmodus oder ungültiger Tageszahl.
