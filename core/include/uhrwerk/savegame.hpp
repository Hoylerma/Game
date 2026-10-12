#pragma once

// Spielstände und Wiederholungen (seit M6, docs/technik/speicherstaende.md).
//
// Ein Spielstand enthält den veränderlichen Zustand aus dem Zustandsverzeichnis (dieselbe Liste wie
// die Prüfsumme) und das Befehlsprotokoll seit Partiebeginn; der Inhalt (Regeln, Güter, Orte …) kommt
// beim Laden aus den Spieldaten und muss zum Inhaltsfingerabdruck passen. Eine Wiederholung ist
// Startzustand plus Befehle mit Tag; sie rechnet eine Partie bitgleich nach.
//
// Dateiformat (Abschnitt 4): fester Kopf (48 Byte, little-endian), Metadaten als UTF-8-Text
// ("schluessel=wert" je Zeile, unkomprimiert), Nutzdaten mit zstd komprimiert.

#include "uhrwerk/date.hpp"
#include "uhrwerk/simulation.hpp"
#include "uhrwerk/world.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

/// Formatversion der Spielstände und Protokolle; jede Änderung am Zustandsverzeichnis erhöht sie.
inline constexpr std::uint32_t kSaveFormatVersion = 1;

/// Bereiche des Inhaltsfingerabdrucks (Abschnitt 3), in dieser Reihenfolge.
inline constexpr std::array<std::string_view, 13> kContentParts = {
    "regeln",   "gelaende", "konfessionen", "kulturen", "berufe", "gueter", "beduerfnisse",
    "betriebe", "maerkte",  "vorkommen",    "staaten",  "kreise", "orte"};

/// Fingerabdruck des Inhalts: je Bereich und gesamt (FNV-1a-64). Namen, Farben und Quellen zählen
/// nicht mit, nur was die Simulation beeinflusst.
struct ContentFingerprint {
    std::array<std::uint64_t, kContentParts.size()> parts{};
    std::uint64_t total = 0;

    bool operator==(const ContentFingerprint&) const = default;
};

ContentFingerprint content_fingerprint(const World& world);

/// Bereiche, in denen sich zwei Fingerabdrücke unterscheiden (leer, wenn sie gleich sind).
std::vector<std::string_view> content_differences(const ContentFingerprint& a, const ContentFingerprint& b);

/// Angaben aus Kopf und Metadaten (ohne die Nutzdaten zu entpacken).
struct SaveInfo {
    std::uint32_t format_version = 0;
    std::string game_version; // Spielversion beim Speichern
    std::string scenario;     // Kennung des Szenarios
    std::string player_tag;   // Spielerstaat (vom Aufrufer beim Speichern)
    Date date;
    std::uint64_t seed = 0;
    std::int64_t days = 0;
    std::uint64_t checksum = 0;       // Prüfsumme des gespeicherten Zustands
    std::uint64_t start_checksum = 0; // Prüfsumme des Startzustands der Partie
    std::uint64_t commands = 0;       // Befehle im Protokoll
    ContentFingerprint content;
    bool compressed = false;
    std::uint64_t payload_size = 0; // Nutzdaten unkomprimiert
    std::uint64_t stored_size = 0;  // Nutzdaten in der Datei
    std::uint64_t payload_hash = 0; // FNV-1a-64 über die unkomprimierten Nutzdaten
};

/// Angaben des Aufrufers beim Speichern.
struct SaveMeta {
    std::string player_tag;
};

/// Spielstand als Bytes. Gleicher Zustand ergibt dieselben Bytes (keine Uhrzeit, kein Rechnername).
std::vector<std::uint8_t> save_game(const Simulation& sim, const SaveMeta& meta);

/// Kopf und Metadaten lesen; nullopt mit Meldung bei fremden, beschädigten oder neueren Dateien.
std::optional<SaveInfo> read_save_info(std::span<const std::uint8_t> bytes, std::string* error = nullptr);

struct LoadedGame {
    std::optional<Simulation> simulation;
    SaveInfo info;
    std::string error; // leer bei Erfolg

    bool ok() const { return simulation.has_value(); }
};

/// Spielstand in den Inhalt content laden (frisch aus load_world oder die Lasttest-Welt): Fingerabdruck
/// prüfen, Zustand lesen und prüfen, abgeleitete Hilfsdaten neu aufbauen, Prüfsumme nachrechnen.
LoadedGame load_game(std::span<const std::uint8_t> bytes, World content, unsigned threads = 0);

// ─── Wiederholung (Abschnitt 5) ─────────────────────────────────────────────

/// Prüfsumme nach einem Tag (days_elapsed danach); jährliche Kontrollpunkte am 1. Januar.
struct Checkpoint {
    std::int64_t day = 0;
    std::uint64_t checksum = 0;

    bool operator==(const Checkpoint&) const = default;
};

struct Replay {
    std::string game_version;
    std::string scenario;
    std::uint64_t seed = 0;
    ContentFingerprint content;
    std::uint64_t start_checksum = 0;
    std::int64_t end_day = 0;
    std::uint64_t end_checksum = 0;
    std::vector<LoggedCommand> commands;
    std::vector<Checkpoint> checkpoints;
};

/// Wiederholung einer laufenden Partie: Befehle seit Partiebeginn (mit den wartenden), Endstand =
/// aktueller Zustand, dazu die unterwegs gesammelten Kontrollpunkte.
Replay make_replay(const Simulation& sim, std::vector<Checkpoint> checkpoints = {});

/// Kontrollpunkt nach einem Schritt fällig? (Datum ist der 1. Januar)
bool is_checkpoint_day(const Simulation& sim);

std::vector<std::uint8_t> write_replay(const Replay& replay);
std::optional<Replay> read_replay(std::span<const std::uint8_t> bytes, std::string* error = nullptr);

/// Wiederholung aus einem Spielstand: Befehle seit Partiebeginn, Endstand = gespeicherter Zustand,
/// ohne Kontrollpunkte.
std::optional<Replay> replay_from_save(std::span<const std::uint8_t> bytes, std::string* error = nullptr);

struct ReplayResult {
    bool ok = false;
    std::string error;              // Startzustand passt nicht, Inhalt passt nicht …
    std::int64_t mismatch_day = -1; // erster Tag mit abweichender Prüfsumme, -1 ohne Abweichung
    std::uint64_t expected = 0;
    std::uint64_t actual = 0;
    std::int64_t checkpoints = 0; // verglichene Kontrollpunkte (mit dem Endstand)
};

/// Rechnet die Wiederholung ab dem Startzustand start (frisch aus load_world) nach.
ReplayResult run_replay(const Replay& replay, World start, unsigned threads = 0);

// ─── Dateien ────────────────────────────────────────────────────────────────

bool write_file(const std::filesystem::path& path, std::span<const std::uint8_t> bytes,
                std::string* error = nullptr);
std::optional<std::vector<std::uint8_t>> read_file(const std::filesystem::path& path,
                                                   std::string* error = nullptr);

/// Prüfsumme als 16-stellige Hexadezimalzahl (Metadaten, Meldungen).
std::string hex16(std::uint64_t value);

} // namespace uhrwerk
