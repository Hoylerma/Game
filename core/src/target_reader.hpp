#pragma once

// Leser für Zielwert-Dateien (intern im Kern): Balancing-Bericht (balance.cpp, seit M5) und Wahllabor
// (election_lab.cpp, seit M8). Sammelt Fehler als "datei:zeile:spalte: Meldung".

#include "uhrwerk/balance.hpp"
#include "uhrwerk/script.hpp"

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

struct TargetReader {
    std::string file;
    std::vector<std::string> errors;

    void error(const ScriptNode& node, const std::string& message) {
        ParseError e;
        e.file = file;
        e.line = node.line;
        e.column = node.column;
        e.message = message;
        errors.push_back(e.to_string());
    }

    const ScriptNode* block(const ScriptNode& parent, std::string_view key) {
        const ScriptNode* node = parent.find(key);
        if (node == nullptr) {
            error(parent, "Block '" + std::string(key) + "' fehlt");
            return nullptr;
        }
        if (!node->is_block) {
            error(*node, "'" + std::string(key) + "' muss ein Block { … } sein");
            return nullptr;
        }
        return node;
    }

    /// Erlaubte Schlüssel eines Blocks; weitere Einträge sind Fehler (Tippfehler fallen so auf).
    void only(const ScriptNode& block, std::initializer_list<std::string_view> keys) {
        for (const ScriptNode& child : block.children) {
            if (std::find(keys.begin(), keys.end(), child.key) == keys.end()) {
                error(child, child.key.empty() ? "unerwarteter Wert '" + child.value + "'"
                                               : "unbekanntes Feld '" + child.key + "'");
            }
        }
    }

    std::optional<Fixed> number(const ScriptNode& block, std::string_view key, ValueRange allowed) {
        const ScriptNode* node = block.find(key);
        if (node == nullptr) {
            error(block, "Feld '" + std::string(key) + "' fehlt");
            return std::nullopt;
        }
        const std::optional<Fixed> value = node->is_block ? std::nullopt : Fixed::parse(node->value);
        if (!value || !allowed.contains(*value)) {
            error(*node, "'" + std::string(key) + "' erwartet eine Zahl von " + allowed.min.to_string(0) +
                             " bis " + allowed.max.to_string(0));
            return std::nullopt;
        }
        return value;
    }

    std::optional<ValueRange> range(const ScriptNode& block, std::string_view key) {
        const ScriptNode* node = block.find(key);
        if (node == nullptr) {
            error(block, "Feld '" + std::string(key) + "' fehlt");
            return std::nullopt;
        }
        const std::vector<std::string_view> values = node->bare_values();
        std::optional<Fixed> lo;
        std::optional<Fixed> hi;
        if (node->is_block && values.size() == 2) {
            lo = Fixed::parse(values[0]);
            hi = Fixed::parse(values[1]);
        }
        if (!lo || !hi || *lo > *hi) {
            error(*node, "'" + std::string(key) + "' erwartet einen Bereich { von bis } mit von ≤ bis");
            return std::nullopt;
        }
        return ValueRange{*lo, *hi};
    }
};

} // namespace uhrwerk
