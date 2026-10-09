#pragma once

// Parser für das Chronik-Skript (*.chron), die Inhaltssprache von EINIGKEIT.
// Grammatik und Beispiele: docs/technik/chronik-skript.md

#include "uhrwerk/date.hpp"
#include "uhrwerk/fixed.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

/// Ein Knoten im Syntaxbaum.
///
/// Formen (siehe Grammatik):
///   key = value          → key, op "=", value, kein Block
///   key >= value         → key, op ">=", value
///   key = { ... }        → key, op "=", is_block, children
///   key { ... }          → wie key = { ... }
///   typ name { ... }     → key = typ, value = name, op "=", is_block, children
///   value                → bare: key leer, op leer, value gesetzt (Listeneintrag)
///   { ... }              → anonymer Block: key leer, is_block
struct ScriptNode {
    std::string key;
    std::string op;
    std::string value;
    bool is_block = false;
    bool quoted = false; // value stand in Anführungszeichen
    std::vector<ScriptNode> children;
    int line = 0;
    int column = 0;

    /// Erstes Kind mit diesem Schlüssel oder nullptr.
    const ScriptNode* find(std::string_view child_key) const;
    /// Alle Kinder mit diesem Schlüssel, in Dateireihenfolge.
    std::vector<const ScriptNode*> find_all(std::string_view child_key) const;

    /// Skalarwert eines Kindes (nur Nicht-Block-Kinder mit op "="), sonst nullopt.
    std::optional<std::string_view> get_string(std::string_view child_key) const;
    std::optional<Fixed> get_fixed(std::string_view child_key) const;
    std::optional<std::int64_t> get_int(std::string_view child_key) const;
    std::optional<Date> get_date(std::string_view child_key) const;
    /// "ja"/"yes"/"wahr" → true, "nein"/"no"/"falsch" → false, sonst nullopt.
    std::optional<bool> get_bool(std::string_view child_key) const;

    /// Werte aller bare-Einträge eines Blocks (z. B. lage = { 13.40 52.52 }).
    std::vector<std::string_view> bare_values() const;
};

struct ParseError {
    std::string file;
    int line = 0;
    int column = 0;
    std::string message;

    /// "datei.chron:12:5: Nachricht"
    std::string to_string() const;
};

struct ParseResult {
    ScriptNode root; // Wurzel: anonymer Block mit allen Anweisungen der Datei
    std::vector<ParseError> errors;

    bool ok() const { return errors.empty(); }
};

/// Parst Text. file_name dient nur Fehlermeldungen. Bei Fehlern wird so viel
/// wie möglich weitergeparst (Wiederaufsetzen an der nächsten Zeile/Klammer).
ParseResult parse_script(std::string_view text, std::string file_name = "<text>");

/// Liest und parst eine Datei (UTF-8, optionales BOM wird ignoriert).
ParseResult parse_script_file(const std::filesystem::path& path);

} // namespace uhrwerk
