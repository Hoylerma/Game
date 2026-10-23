#include "uhrwerk/loader.hpp"

#include "uhrwerk/economy.hpp"
#include "uhrwerk/population.hpp"
#include "uhrwerk/script.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace uhrwerk {

std::string LoadIssue::to_string() const {
    std::string result;
    if (!file.empty()) {
        result += file;
        if (line > 0) {
            result += ':' + std::to_string(line);
        }
        result += ": ";
    }
    result += fatal ? "Fehler: " : "Warnung: ";
    result += message;
    return result;
}

namespace {

namespace fs = std::filesystem;

// ─── Hilfsfunktionen ────────────────────────────────────────────────────────

std::string quote(std::string_view text) {
    return "'" + std::string(text) + "'";
}

/// Kürzeste Dezimaldarstellung, die beim Einlesen wieder genau diesen Wert
/// ergibt ("0.1" statt "0.100006", "12" statt "12.000000").
std::string show(Fixed value) {
    for (int decimals = 0; decimals < 6; ++decimals) {
        std::string text = value.to_string(decimals);
        if (Fixed::parse(text) == value) {
            return text;
        }
    }
    return value.to_string(6);
}

std::optional<std::int64_t> parse_int(std::string_view text) {
    std::size_t pos = 0;
    const bool negative = !text.empty() && text[0] == '-';
    if (negative) {
        pos = 1;
    }
    if (pos == text.size()) {
        return std::nullopt;
    }
    std::int64_t value = 0;
    for (; pos < text.size(); ++pos) {
        const char c = text[pos];
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        if (value > (INT64_MAX - (c - '0')) / 10) {
            return std::nullopt;
        }
        value = value * 10 + (c - '0');
    }
    return negative ? -value : value;
}

bool is_location_id(std::string_view id) {
    if (id.empty()) {
        return false;
    }
    return std::all_of(id.begin(), id.end(),
                       [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

/// Sieht der Schlüssel wie ein Datum aus (beginnt mit einer Ziffer)? Dann ist er ein datierter
/// Block (Abschnitt 3.8) – gültig oder nicht.
bool looks_dated(std::string_view key) {
    return !key.empty() && key[0] >= '0' && key[0] <= '9';
}

/// "'a'", "'a' und 'b'", "'a', 'b' und 'c'"
std::string quote_list(const std::vector<std::string_view>& keys) {
    std::string result;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) {
            result += i + 1 == keys.size() ? " und " : ", ";
        }
        result += quote(keys[i]);
    }
    return result;
}

/// Die fünf Simulationsfelder eines Ortes: alle oder keines (Abschnitt 3.6; kultur seit M4).
constexpr std::string_view kSimulationFields[] = {"besitzer", "bevoelkerung", "konfession", "kultur",
                                                  "verwuestung"};

bool is_state_tag(std::string_view tag) {
    if (tag.size() < 2 || tag.size() > 4) {
        return false;
    }
    return std::all_of(tag.begin(), tag.end(),
                       [](char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); });
}

std::optional<std::string> read_env(const char* name) {
#ifdef _MSC_VER
    char* buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) {
        return std::nullopt;
    }
    std::string value(buffer);
    std::free(buffer);
    return value;
#else
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

// ─── Problemsammlung ────────────────────────────────────────────────────────

class Issues {
public:
    explicit Issues(std::vector<LoadIssue>& out) : out_(out) {}

    void error(const std::string& file, int line, std::string message) {
        out_.push_back(LoadIssue{file, line, std::move(message), true});
    }
    void warning(const std::string& file, int line, std::string message) {
        out_.push_back(LoadIssue{file, line, std::move(message), false});
    }
    void add(LoadIssue issue) { out_.push_back(std::move(issue)); }
    bool has_errors() const {
        return std::any_of(out_.begin(), out_.end(), [](const LoadIssue& i) { return i.fatal; });
    }
    std::size_t size() const { return out_.size(); }
    /// Ordnet die Meldungen ab Position first stabil nach Zeile (Meldungen einer Datei).
    void sort_by_line_from(std::size_t first) {
        std::stable_sort(out_.begin() + static_cast<std::ptrdiff_t>(first), out_.end(),
                         [](const LoadIssue& a, const LoadIssue& b) { return a.line < b.line; });
    }

private:
    std::vector<LoadIssue>& out_;
};

// ─── Zahlenbereiche ─────────────────────────────────────────────────────────

struct Range {
    std::optional<Fixed> min;
    std::optional<Fixed> max;
    bool min_exclusive = false;

    bool contains(Fixed v) const {
        if (min && (min_exclusive ? v <= *min : v < *min)) {
            return false;
        }
        return !(max && v > *max);
    }

    std::string describe() const {
        if (min && max) {
            if (min_exclusive) {
                return "muss größer als " + show(*min) + " und höchstens " + show(*max) + " sein";
            }
            return "muss zwischen " + show(*min) + " und " + show(*max) + " liegen";
        }
        if (min) {
            if (min_exclusive) {
                return "muss größer als " + show(*min) + " sein";
            }
            return *min == Fixed::zero() ? "darf nicht negativ sein"
                                         : "muss mindestens " + show(*min) + " sein";
        }
        if (max) {
            return "darf höchstens " + show(*max) + " sein";
        }
        return {};
    }
};

Range between(Fixed lo, Fixed hi) {
    return Range{lo, hi, false};
}
Range above_up_to(Fixed lo, Fixed hi) {
    return Range{lo, hi, true};
}

Fixed fx(std::int64_t v) {
    return Fixed::from_int(v);
}

// ─── Feldleser für einen Objektblock ────────────────────────────────────────

class Fields {
public:
    Fields(const ScriptNode& block, const std::string& file, std::string what, Issues& issues,
           std::initializer_list<std::string_view> known)
        : block_(block), file_(file), what_(std::move(what)), issues_(issues) {
        std::map<std::string, int, std::less<>> first_line;
        for (const ScriptNode& child : block.children) {
            if (child.key.empty()) {
                if (child.is_block) {
                    issues_.warning(file_, child.line, "anonymer Block in " + what_ + " wird ignoriert");
                } else {
                    issues_.warning(file_, child.line,
                                    "unerwarteter Eintrag " + quote(child.value) + " in " + what_ +
                                        " wird ignoriert");
                }
                continue;
            }
            if (std::find(known.begin(), known.end(), std::string_view(child.key)) == known.end()) {
                issues_.warning(file_, child.line,
                                "unbekanntes Feld " + quote(child.key) + " in " + what_ + " wird ignoriert");
                continue;
            }
            const auto [it, inserted] = first_line.emplace(child.key, child.line);
            if (!inserted) {
                issues_.error(file_, child.line,
                              "Feld " + quote(child.key) + " in " + what_ +
                                  " ist mehrfach angegeben (zuerst in Zeile " + std::to_string(it->second) +
                                  ")");
                continue;
            }
            if (child.op != "=") {
                issues_.error(file_, child.line,
                              "Feld " + quote(child.key) + " erwartet '=' statt " + quote(child.op));
                broken_.insert(child.key);
                continue;
            }
            fields_.emplace(child.key, &child);
        }
    }

    /// Pflichtfeld als Text (mit oder ohne Anführungszeichen), nicht leer.
    std::optional<std::string> text(std::string_view key) {
        const ScriptNode* node = scalar(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        if (node->value.empty()) {
            issues_.error(file_, node->line, "Feld " + quote(key) + " darf nicht leer sein");
            return std::nullopt;
        }
        return node->value;
    }

    /// Pflichtfeld als Festkommazahl im erlaubten Bereich.
    std::optional<Fixed> fixed(std::string_view key, const Range& range) {
        const ScriptNode* node = scalar(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        const auto value = Fixed::parse(node->value);
        if (!value) {
            issues_.error(file_, node->line,
                          "ungültiger Wert " + quote(node->value) + " für " + quote(key) +
                              " (erwartet eine Zahl wie 12 oder 0.25)");
            return std::nullopt;
        }
        if (!range.contains(*value)) {
            issues_.error(file_, node->line,
                          quote(key) + " " + range.describe() + " (ist " + node->value + ")");
            return std::nullopt;
        }
        return value;
    }

    /// Pflichtfeld als Ganzzahl in [lo, hi].
    std::optional<std::int64_t> integer(std::string_view key, std::int64_t lo, std::int64_t hi) {
        const ScriptNode* node = scalar(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        const auto value = parse_int(node->value);
        if (!value) {
            issues_.error(file_, node->line,
                          "ungültiger Wert " + quote(node->value) + " für " + quote(key) +
                              " (erwartet eine Ganzzahl)");
            return std::nullopt;
        }
        if (*value < lo || *value > hi) {
            issues_.error(file_, node->line,
                          quote(key) + " muss zwischen " + std::to_string(lo) + " und " + std::to_string(hi) +
                              " liegen (ist " + node->value + ")");
            return std::nullopt;
        }
        return value;
    }

    std::optional<bool> boolean(std::string_view key) {
        const ScriptNode* node = scalar(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        const auto value = block_.get_bool(key);
        if (!value) {
            issues_.error(file_, node->line,
                          "ungültiger Wert " + quote(node->value) + " für " + quote(key) +
                              " (erwartet ja oder nein)");
        }
        return value;
    }

    std::optional<Date> date(std::string_view key) {
        const ScriptNode* node = scalar(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        const auto value = parse_date(node->value);
        if (!value) {
            issues_.error(file_, node->line,
                          "ungültiges Datum " + quote(node->value) + " für " + quote(key) +
                              " (erwartet JJJJ.MM.TT, z. B. 1648.10.24)");
        }
        return value;
    }

    std::optional<Color> color(std::string_view key) {
        const ScriptNode* node = block(key, "drei Zahlen von 0 bis 255, z. B. { 58 95 143 }");
        if (node == nullptr) {
            return std::nullopt;
        }
        const std::vector<std::int64_t> values = bare_numbers(*node, key, 3, "drei Zahlen von 0 bis 255");
        if (values.size() != 3) {
            return std::nullopt;
        }
        for (const std::int64_t v : values) {
            if (v < 0 || v > 255) {
                issues_.error(file_, node->line,
                              "Farbwerte in " + quote(key) + " müssen zwischen 0 und 255 liegen");
                return std::nullopt;
            }
        }
        return Color{static_cast<std::uint8_t>(values[0]), static_cast<std::uint8_t>(values[1]),
                     static_cast<std::uint8_t>(values[2])};
    }

    /// Pflichtfeld als Block (key = { … }).
    const ScriptNode* block(std::string_view key, std::string_view expected) {
        const ScriptNode* node = get(key);
        if (node == nullptr) {
            return nullptr;
        }
        if (!node->is_block) {
            issues_.error(file_, node->line,
                          quote(key) +
                              " erwartet einen Block in geschweiften Klammern: " + std::string(expected));
            return nullptr;
        }
        return node;
    }

    /// Ist das Feld angegeben (auch wenn es fehlerhaft ist, z. B. mit falschem Operator)?
    bool has(std::string_view key) const {
        return fields_.find(key) != fields_.end() || broken_.find(key) != broken_.end();
    }

    const std::string& file() const { return file_; }
    const std::string& what() const { return what_; }
    int line() const { return block_.line; }
    Issues& issues() { return issues_; }

private:
    const ScriptNode* get(std::string_view key) {
        const auto it = fields_.find(key);
        if (it == fields_.end()) {
            if (broken_.find(key) == broken_.end()) {
                issues_.error(file_, block_.line, "Pflichtfeld " + quote(key) + " fehlt in " + what_);
            }
            return nullptr;
        }
        return it->second;
    }

    const ScriptNode* scalar(std::string_view key) {
        const ScriptNode* node = get(key);
        if (node != nullptr && node->is_block) {
            issues_.error(file_, node->line, quote(key) + " erwartet einen einzelnen Wert, keinen Block");
            return nullptr;
        }
        return node;
    }

    // Liest genau `count` Ganzzahlen aus den bare-Einträgen eines Blocks.
    std::vector<std::int64_t> bare_numbers(const ScriptNode& node, std::string_view key, std::size_t count,
                                           std::string_view expected) {
        std::vector<std::int64_t> values;
        bool ok = true;
        for (const ScriptNode& child : node.children) {
            const auto v = (child.key.empty() && !child.is_block) ? parse_int(child.value) : std::nullopt;
            if (!v) {
                ok = false;
                break;
            }
            values.push_back(*v);
        }
        if (!ok || values.size() != count) {
            issues_.error(file_, node.line, quote(key) + " erwartet " + std::string(expected));
            return {};
        }
        return values;
    }

    const ScriptNode& block_;
    const std::string& file_;
    std::string what_;
    Issues& issues_;
    std::map<std::string, const ScriptNode*, std::less<>> fields_;
    std::set<std::string, std::less<>> broken_;
};

// ─── Quelldateien ───────────────────────────────────────────────────────────

struct SourceFile {
    std::string name; // relativ zum Datenordner mit '/', z. B. "history/orte/brandenburg.chron"
    ScriptNode root;
};

struct Definition {
    Index index = kNone;
    std::string file;
    int line = 0;
};

using Registry = std::map<std::string, Definition, std::less<>>;

struct LocationDraft {
    bool without_data = false; // keines der fünf Simulationsfelder (Ort ohne Daten)
    std::int64_t population = 0;
    std::vector<std::pair<Index, std::int64_t>> confessions; // Konfession, Prozent (Reihenfolge der Datei)
    std::vector<std::pair<Index, std::int64_t>> cultures;    // Kultur, Prozent (Reihenfolge der Datei)
};

struct ProfessionRef {
    std::string city_profession; // Feld in_stadt, leer = keines
    std::string file;
    int line = 0;
    std::string share_from; // Feld anteil_aus (seit M7), leer = keines
    int share_line = 0;
};

struct CapitalRef {
    std::string location_id;
    std::string file;
    int line = 0;
};

struct AdminParentRef {
    std::string id; // leer = oberste Einheit
    std::string file;
    int line = 0;
};

struct PlayerRef {
    std::string tag;
    std::string file;
    int line = 0;
};

/// Staat eines Wahlkreises (seit M8), aufgelöst nach dem Staatenbestand.
struct DistrictRef {
    std::string state_tag;
    std::string file;
    int line = 0;
};

/// Leitmacht einer Zollunion (seit M9), aufgelöst nach dem Staatenbestand.
struct CustomsUnionRef {
    std::string leader_tag;
    std::string file;
    int line = 0;
    std::vector<std::pair<std::string, int>> excluded_tags; // Tag, Zeile (seit M9, Schritt a2)
};

/// Grundbeziehung (seit M9, Schritt b1): Tags bis nach drop_states_without_land.
struct BaseRelationRef {
    std::string a_tag;
    std::string b_tag;
    Fixed value;
    std::string source;
    std::string file;
    int line = 0;
};

/// Bundesversammlung (seit M9, Schritt b2): Tags bis nach drop_states_without_land.
struct DietRef {
    std::string file;
    int line = 0;
    std::string presidency_tag;
    int presidency_line = 0;
    std::string seat;
    int seat_line = 0;
    std::vector<std::tuple<std::string, std::int32_t, int>> plenum; // Tag, Stimmen, Zeile
    struct Vote {
        std::string name;
        std::vector<std::pair<std::string, int>> tags; // Tag, Zeile
        int line = 0;
    };
    std::vector<Vote> council;
};

/// Militär aus common/militaer.chron (seit M10), aufgelöst nach den Staaten und Orten.
struct MilitaryRef {
    std::string file;
    int line = 0;
    struct Profile {
        std::string tag; // leer: Standard für alle Staaten ohne eigenen Eintrag
        Date since{1, 1, 1};
        ArmyProfile profile;
        int line = 0;
    };
    std::vector<Profile> profiles;
    struct GarrisonDraft {
        std::string id;
        std::string name;
        std::string tag;
        std::string location;
        Fixed share;
        int line = 0;
    };
    std::vector<GarrisonDraft> garrisons; // nur die zum Startdatum bestehenden
};

/// Felder einer CSV-Zeile (Semikolon; Felder in "…" dürfen ; enthalten, "" steht für ein ").
std::vector<std::string> split_csv(std::string_view line) {
    std::vector<std::string> fields(1);
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                fields.back() += '"';
                ++i;
            } else if (c == '"') {
                quoted = false;
            } else {
                fields.back() += c;
            }
        } else if (c == '"' && fields.back().empty()) {
            quoted = true;
        } else if (c == ';') {
            fields.emplace_back();
        } else {
            fields.back() += c;
        }
    }
    return fields;
}

/// Ebene eines Parlaments (seit M8): Staatstag, leer für den Deutschen Bund; Tagungsort.
struct ParliamentRef {
    std::string state_tag;
    std::string file;
    int line = 0;
    std::string seat; // Ortskennung, leer = keine
    int seat_line = 0;
};

/// Staaten einer Region der Wahlneigung (seit M8), aufgelöst nach dem Staatenbestand.
struct RegionRef {
    std::vector<std::pair<std::string, int>> state_tags; // Tag, Zeile
    std::string file;
};

class Loader {
public:
    Loader(fs::path data_dir, std::vector<LoadIssue>& issues)
        : data_dir_(std::move(data_dir)), issues_(issues) {}

    /// Nur die Szenarien (list_scenarios, seit M7).
    std::vector<Scenario> read_scenarios() {
        std::vector<Scenario> scenarios;
        for (const std::string& name : list_folder("szenarien")) {
            process(name, [&](const SourceFile& f) { load_scenarios(f, scenarios); });
        }
        return scenarios;
    }

    std::optional<World> run(std::string_view scenario_id) {
        std::error_code ec;
        if (!fs::is_directory(data_dir_, ec)) {
            issues_.error(path_to_utf8(data_dir_), 0, "Datenordner nicht gefunden");
            return std::nullopt;
        }

        // Szenarien zuerst: Das Startdatum entscheidet, welche datierten Blöcke der Historie
        // gelten (Abschnitt 3.8) und seit M7 auch, welche Güter, Berufe und Betriebe es gibt
        // ('seit', Abschnitt 3.18).
        std::vector<Scenario> scenarios;
        for (const std::string& name : list_folder("szenarien")) {
            process(name, [&](const SourceFile& f) { load_scenarios(f, scenarios); });
        }
        const Scenario* selected = select_scenario(scenarios, scenario_id);
        // Ohne gültiges Szenario werden alle datierten Blöcke angewendet, damit die
        // Prüfung trotzdem alle Felder sieht.
        start_ = selected != nullptr ? selected->start : Date{9999, 12, 31};
        scenario_selected_ = selected != nullptr;
        capacity_factor_ = selected != nullptr ? selected->capacity_factor : Fixed::one();

        process("common/regeln.chron", [&](const SourceFile& f) { load_rules(f); });
        process("common/gelaende.chron", [&](const SourceFile& f) { load_terrains(f); });
        process("common/konfessionen.chron", [&](const SourceFile& f) { load_confessions(f); });
        process("common/kulturen.chron", [&](const SourceFile& f) { load_cultures(f); });
        process("common/berufe.chron", [&](const SourceFile& f) { load_professions(f); });
        // Wirtschaft (seit M5, docs/technik/wirtschaft.md, Abschnitt 2).
        process("common/gueter.chron", [&](const SourceFile& f) { load_goods(f); });
        process("common/beduerfnisse.chron", [&](const SourceFile& f) { load_needs(f); });
        process("common/betriebe.chron", [&](const SourceFile& f) { load_businesses(f); });
        process("common/maerkte.chron", [&](const SourceFile& f) { load_markets(f); });
        // Politik (seit M8, docs/technik/politik.md); die Dateien dürfen fehlen.
        process_optional("common/stroemungen.chron", [&](const SourceFile& f) { load_currents(f); });
        process_optional("common/interessengruppen.chron",
                         [&](const SourceFile& f) { load_interest_groups(f); });
        process_optional("common/parlamente.chron", [&](const SourceFile& f) { load_parliaments(f); });
        process_optional("common/wahlen.chron", [&](const SourceFile& f) { load_election_rules(f); });
        // Zollunionen (seit M9, docs/technik/diplomatie.md) vor den Staaten, die ihnen angehören; die Datei
        // darf fehlen.
        process_optional("common/zollunionen.chron", [&](const SourceFile& f) { load_customs_unions(f); });
        // Diplomatie (seit M9, Schritt b1): Regeln und Grundbeziehungen; beides darf fehlen, solange es
        // keinen Deutschen Bund gibt.
        process_optional("common/diplomatie.chron", [&](const SourceFile& f) { load_diplomacy_rules(f); });
        process_optional("common/bundesversammlung.chron", [&](const SourceFile& f) { load_diet(f); });
        // Militär (seit M10, docs/technik/militaer.md): Regeln, Heerwesen und Garnisonen; die Datei darf
        // fehlen, das Militär gilt erst ab ihrem 'seit'.
        process_optional("common/militaer.chron", [&](const SourceFile& f) { load_military(f); });
        for (const std::string& name : list_folder("history/beziehungen", Folder::Optional)) {
            process(name, [&](const SourceFile& f) { load_base_relations(f); });
        }

        for (const std::string& name : list_folder("history/staaten")) {
            process(name, [&](const SourceFile& f) { load_states(f); });
        }
        // Kreise (seit M3) vor den Orten, die auf sie verweisen. Ältere Datenstände haben
        // keinen Ordner history/kreise; das ist kein Fehler.
        for (const std::string& name : list_folder("history/kreise", Folder::Optional)) {
            process(name, [&](const SourceFile& f) { load_districts(f); });
        }
        // Verwaltungseinheiten (seit M7) vor den Orten, die auf sie verweisen; der Ordner darf fehlen.
        for (const std::string& name : list_folder("history/verwaltung", Folder::Optional)) {
            process(name, [&](const SourceFile& f) { load_admin_units(f); });
        }
        resolve_admin_parents();
        for (const std::string& name : list_folder("history/orte")) {
            process(name, [&](const SourceFile& f) { load_locations(f); });
        }
        // Eisenbahnen (seit M7) nach den Orten, die sie verbinden; der Ordner darf fehlen.
        for (const std::string& name : list_folder("history/eisenbahnen", Folder::Optional)) {
            process(name, [&](const SourceFile& f) { load_railways(f); });
        }
        // Wahlkreise (seit M8) nach den Orten und Parlamenten; der Ordner darf fehlen.
        for (const std::string& name : list_folder("history/wahlkreise", Folder::Optional)) {
            process(name, [&](const SourceFile& f) { load_electoral_districts(f); });
        }

        drop_states_without_land();
        resolve_parliaments();
        resolve_customs_unions();
        resolve_base_relations();
        resolve_diet();
        check_players(scenarios, selected);
        check_capitals();
        resolve_military(); // nach den Hauptstädten (Garnison eines Staates ohne Eintrag)
        check_districts();
        check_markets();

        if (issues_.has_errors() || selected == nullptr) {
            return std::nullopt;
        }

        world_.scenario = *selected;
        world_.date = selected->start;
        build_pops();
        initialize_population(world_);
        return std::move(world_);
    }

private:
    // ── Dateien ──

    /// Liest eine Datei, wertet sie mit handler aus und ordnet ihre Meldungen nach Zeile.
    template <typename Handler> void process(const std::string& relative, Handler handler) {
        const std::size_t first_issue = issues_.size();
        if (auto file = read_file(relative)) {
            handler(*file);
        }
        issues_.sort_by_line_from(first_issue);
    }

    /// Wie process, aber eine fehlende Datei ist kein Fehler (Inhalt, den ältere Datenstände nicht haben).
    template <typename Handler> void process_optional(const std::string& relative, Handler handler) {
        std::error_code ec;
        if (fs::is_regular_file(data_dir_ / utf8_to_path(relative), ec)) {
            process(relative, handler);
        }
    }

    std::optional<SourceFile> read_file(const std::string& relative) {
        const fs::path path = data_dir_ / utf8_to_path(relative);
        std::error_code ec;
        if (!fs::is_regular_file(path, ec)) {
            issues_.error(relative, 0, "Datei fehlt");
            return std::nullopt;
        }
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            issues_.error(relative, 0, "Datei kann nicht gelesen werden");
            return std::nullopt;
        }
        const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        ParseResult parsed = parse_script(text, relative);
        for (const ParseError& e : parsed.errors) {
            std::string message = e.message;
            if (e.column > 0) {
                message += " (Spalte " + std::to_string(e.column) + ")";
            }
            issues_.error(relative, e.line, std::move(message));
        }
        return SourceFile{relative, std::move(parsed.root)};
    }

    enum class Folder : std::uint8_t {
        Required, // fehlender oder leerer Ordner ist ein Fehler
        Optional, // fehlender oder leerer Ordner ist in Ordnung
    };

    /// Alle *.chron-Dateien eines Ordners, nach Dateiname sortiert.
    std::vector<std::string> list_folder(const std::string& folder, Folder kind = Folder::Required) {
        const fs::path dir = data_dir_ / fs::path(folder);
        std::error_code ec;
        if (!fs::exists(dir, ec) && kind == Folder::Optional) {
            return {};
        }
        if (!fs::is_directory(dir, ec)) {
            issues_.error(folder, 0, "Ordner fehlt");
            return {};
        }
        std::vector<std::string> names;
        fs::directory_iterator it(dir, ec);
        const fs::directory_iterator end;
        while (!ec && it != end) {
            std::error_code type_ec;
            if (it->is_regular_file(type_ec) && it->path().extension() == ".chron") {
                names.push_back(path_to_utf8(it->path().filename()));
            }
            it.increment(ec);
        }
        if (ec) {
            issues_.error(folder, 0, "Ordner kann nicht gelesen werden: " + ec.message());
        }
        std::sort(names.begin(), names.end());
        if (names.empty() && !ec && kind == Folder::Required) {
            issues_.error(folder, 0, "Ordner enthält keine .chron-Dateien");
        }
        std::vector<std::string> result;
        result.reserve(names.size());
        for (const std::string& name : names) {
            result.push_back(folder + "/" + name);
        }
        return result;
    }

    // Ruft handler für jede Definition "typ kennung { … }" auf; alles andere
    // auf oberster Ebene wird mit einer Warnung übergangen.
    /// 'seit' einer Definition (seit M7, Abschnitt 3.18): true, wenn sie zum Startdatum noch nicht gilt; sie
    /// wird dann gelesen und geprüft, aber nicht übernommen.
    bool is_later(const SourceFile& file, const ScriptNode& node, std::string_view kind) {
        const ScriptNode* since = node.find("seit");
        if (since == nullptr) {
            return false;
        }
        const auto date = since->is_block ? std::nullopt : parse_date(since->value);
        if (!date) {
            issues_.error(file.name, since->line,
                          "ungültiges Datum " + quote(since->value) + " für 'seit' in " + std::string(kind) +
                              " " + node.value + " (erwartet JJJJ.MM.TT)");
            return false;
        }
        if (*date <= start_) {
            return false;
        }
        const auto [it, inserted] = later_.emplace(std::string(kind) + ":" + node.value,
                                                   LaterDefinition{*date, file.name, node.line});
        if (!inserted) {
            const std::string label = kind == "gut"                ? "Gut"
                                      : kind == "beruf"            ? "Beruf"
                                      : kind == "betrieb"          ? "Betrieb"
                                      : kind == "interessengruppe" ? "Interessengruppe"
                                      : kind == "zollunion"        ? "Zollunion"
                                                                   : "Parlament";
            issues_.error(file.name, node.line,
                          "doppelte Kennung " + quote(node.value) + " für " + label +
                              " (bereits definiert in " + it->second.file + ":" +
                              std::to_string(it->second.line) + ")");
        }
        return true;
    }

    /// Eine spätere Definition darf keine Kennung tragen, die schon gilt (seit M7).
    void check_later_ids(std::string_view kind, const Registry& registry, std::string_view label) {
        const std::string prefix = std::string(kind) + ":";
        for (const auto& [key, later] : later_) {
            if (key.compare(0, prefix.size(), prefix) != 0) {
                continue;
            }
            const auto it = registry.find(std::string_view(key).substr(prefix.size()));
            if (it != registry.end()) {
                issues_.error(later.file, later.line,
                              "doppelte Kennung " + quote(key.substr(prefix.size())) + " für " +
                                  std::string(label) + " (bereits definiert in " + it->second.file + ":" +
                                  std::to_string(it->second.line) + ")");
            }
        }
    }

    bool is_later_id(std::string_view kind, const std::string& id) const {
        return later_.count(std::string(kind) + ":" + id) != 0;
    }

    /// Verweis auf eine unbekannte Kennung: true, wenn es sie erst nach dem Startdatum gibt. Aus einer
    /// ebenfalls späteren Definition ist das in Ordnung, sonst fatal.
    bool later_reference(std::string_view kind, std::string_view label, const std::string& id,
                         const std::string& file, int line, const std::string& what) {
        const auto it = later_.find(std::string(kind) + ":" + id);
        if (it == later_.end()) {
            return false;
        }
        if (!in_later_) {
            issues_.error(file, line,
                          std::string(label) + " " + quote(id) + " gibt es erst seit " +
                              format_script_date(it->second.date) + " ('seit'); " + what +
                              " gilt schon vorher und kann darauf nicht verweisen");
        }
        return true;
    }

    /// Setzt in_later_ für die Dauer einer Definition.
    struct LaterScope {
        bool& flag;
        LaterScope(bool& f, bool value) : flag(f) { flag = value; }
        ~LaterScope() { flag = false; }
        LaterScope(const LaterScope&) = delete;
        LaterScope& operator=(const LaterScope&) = delete;
    };

    void for_each_definition(const SourceFile& file, std::string_view type, std::string_view example,
                             const std::function<void(const ScriptNode&)>& handler) {
        for (const ScriptNode& node : file.root.children) {
            if (node.key != type) {
                warn_unknown_top_level(file, node, example);
                continue;
            }
            if (!node.is_block || node.value.empty()) {
                issues_.error(file.name, node.line,
                              "ungültige Definition, erwartet: " + std::string(example));
                continue;
            }
            handler(node);
        }
    }

    void warn_unknown_top_level(const SourceFile& file, const ScriptNode& node, std::string_view example) {
        std::string what;
        if (node.key.empty()) {
            what = node.is_block ? "anonymer Block" : "unerwarteter Eintrag " + quote(node.value);
        } else {
            what = "unbekannter Eintrag " + quote(node.key);
        }
        issues_.warning(file.name, node.line,
                        what + " wird ignoriert (erwartet: " + std::string(example) + ")");
    }

    bool register_id(Registry& registry, const std::string& id, Index index, const std::string& file,
                     int line, std::string_view kind) {
        const auto [it, inserted] = registry.emplace(id, Definition{index, file, line});
        if (!inserted) {
            issues_.error(file, line,
                          "doppelte Kennung " + quote(id) + " für " + std::string(kind) +
                              " (bereits definiert in " + it->second.file + ":" +
                              std::to_string(it->second.line) + ")");
        }
        return inserted;
    }

    static Index lookup(const Registry& registry, std::string_view id) {
        const auto it = registry.find(id);
        return it == registry.end() ? kNone : it->second.index;
    }

    // ── common/ ──

    void load_rules(const SourceFile& file) {
        const ScriptNode* found = nullptr;
        for (const ScriptNode& node : file.root.children) {
            if (node.key != "regeln") {
                warn_unknown_top_level(file, node, "regeln { … }");
                continue;
            }
            if (!node.is_block) {
                issues_.error(file.name, node.line, "ungültige Definition, erwartet: regeln { … }");
                continue;
            }
            if (found != nullptr) {
                issues_.error(file.name, node.line,
                              "Block 'regeln' ist mehrfach vorhanden (zuerst in Zeile " +
                                  std::to_string(found->line) + ")");
                continue;
            }
            found = &node;
        }
        if (found == nullptr) {
            issues_.error(file.name, 0, "Block 'regeln' fehlt");
            return;
        }

        Fields f(*found, file.name, "regeln", issues_,
                 {"verwuestung_abbau",
                  "verwuestung_ertrag",
                  "seuche_chance",
                  "seuche_verlust_min",
                  "seuche_verlust_max",
                  "steuersatz_max",
                  "anteil_kinder",
                  "anteil_jung",
                  "anteil_erwerb",
                  "fruchtbarkeit",
                  "anteil_fruchtbar_erwerb",
                  "fruchtbarkeit_landreserve",
                  "fruchtbarkeit_wohlstand",
                  "sterblichkeit_kinder",
                  "sterblichkeit_jung",
                  "sterblichkeit_erwerb",
                  "sterblichkeit_alt",
                  "sterblichkeit_armut",
                  "tragfaehigkeit_stadt",
                  "lebensstandard_bezug",
                  "lebensstandard_anpassung",
                  "bildung_stadt",
                  "bildung_anpassung",
                  "wanderung_ziele",
                  "wanderung_radius_km",
                  "wanderung_rate",
                  "wanderung_max",
                  "wanderung_landreserve",
                  "wanderung_fremd",
                  "auswanderung_schwelle",
                  "auswanderung_rate",
                  "block_mindestgroesse",
                  "wanderung_mindestmenge",
                  "preis_min",
                  "preis_max",
                  "preis_anpassung",
                  "handel_nachbarn",
                  "handel_radius_km",
                  "handel_rate",
                  "handel_fracht",
                  "seefracht",
                  "bahnfracht",
                  "handelsspanne",
                  "betrieb_anpassung",
                  "betrieb_min_anteil",
                  "ernte_schwankung_land",
                  "ernte_schwankung_markt",
                  "sterblichkeit_hunger",
                  "einschwingen_monate",
                  "verwaltung_je_einwohner",
                  "sold",
                  "reserve_jahre",
                  "bau_anteil",
                  "ausbau_max",
                  "ausbau_halb",
                  "zins_basis",
                  "zins_risiko",
                  "zins_max",
                  "bankrott_quote",
                  "bankrott_schnitt",
                  "bankrott_aufschlag",
                  "bankrott_jahre"});
        Rules& r = world_.rules;
        const Range unit = between(fx(0), fx(1));
        const auto devastation_decay = f.fixed("verwuestung_abbau", unit);
        const auto devastation_yield_loss = f.fixed("verwuestung_ertrag", unit);
        const auto plague_chance = f.fixed("seuche_chance", unit);
        const auto plague_loss_min = f.fixed("seuche_verlust_min", unit);
        const auto plague_loss_max = f.fixed("seuche_verlust_max", unit);
        const auto tax_rate_max = f.fixed("steuersatz_max", unit);

        r.devastation_decay = devastation_decay.value_or(Fixed{});
        r.devastation_yield_loss = devastation_yield_loss.value_or(Fixed{});
        r.plague_chance = plague_chance.value_or(Fixed{});
        r.plague_loss_min = plague_loss_min.value_or(Fixed{});
        r.plague_loss_max = plague_loss_max.value_or(Fixed{});
        r.tax_rate_max = tax_rate_max.value_or(Fixed::one());
        tax_rate_max_known_ = tax_rate_max.has_value();

        if (plague_loss_min && plague_loss_max && *plague_loss_min > *plague_loss_max) {
            issues_.error(file.name, found->line,
                          "'seuche_verlust_min' (" + show(*plague_loss_min) +
                              ") ist größer als 'seuche_verlust_max' (" + show(*plague_loss_max) + ")");
        }

        // Bevölkerung (seit M4, docs/technik/bevoelkerung.md, Abschnitt 4.9).
        const auto share_children = f.fixed("anteil_kinder", unit);
        const auto share_young = f.fixed("anteil_jung", unit);
        const auto share_working = f.fixed("anteil_erwerb", unit);
        r.share_children = share_children.value_or(Fixed{});
        r.share_young = share_young.value_or(Fixed{});
        r.share_working = share_working.value_or(Fixed{});
        if (share_children && share_young && share_working &&
            *share_children + *share_young + *share_working > Fixed::one()) {
            issues_.error(file.name, found->line,
                          "'anteil_kinder', 'anteil_jung' und 'anteil_erwerb' ergeben zusammen " +
                              show(*share_children + *share_young + *share_working) +
                              ", höchstens 1 ist erlaubt (der Rest sind die Alten)");
        }
        r.fertility = f.fixed("fruchtbarkeit", unit).value_or(Fixed{});
        r.fertile_share_working = f.fixed("anteil_fruchtbar_erwerb", unit).value_or(Fixed{});
        r.fertility_reserve = f.fixed("fruchtbarkeit_landreserve", between(fx(0), fx(10))).value_or(Fixed{});
        r.fertility_wealth = f.fixed("fruchtbarkeit_wohlstand", unit).value_or(Fixed{});
        r.mortality_children = f.fixed("sterblichkeit_kinder", unit).value_or(Fixed{});
        r.mortality_young = f.fixed("sterblichkeit_jung", unit).value_or(Fixed{});
        r.mortality_working = f.fixed("sterblichkeit_erwerb", unit).value_or(Fixed{});
        r.mortality_old = f.fixed("sterblichkeit_alt", unit).value_or(Fixed{});
        r.mortality_poverty = f.fixed("sterblichkeit_armut", unit).value_or(Fixed{});
        r.capacity_city = f.fixed("tragfaehigkeit_stadt", between(fx(0), fx(100))).value_or(Fixed::one());
        r.living_standard_reference =
            f.fixed("lebensstandard_bezug", above_up_to(fx(0), fx(1'000'000))).value_or(Fixed::one());
        r.living_standard_adjustment = f.fixed("lebensstandard_anpassung", unit).value_or(Fixed{});
        r.literacy_city = f.fixed("bildung_stadt", between(fx(0), fx(10))).value_or(Fixed::one());
        r.literacy_adjustment = f.fixed("bildung_anpassung", unit).value_or(Fixed{});
        r.migration_targets = static_cast<std::int32_t>(f.integer("wanderung_ziele", 0, 16).value_or(0));
        r.migration_radius_km = f.fixed("wanderung_radius_km", between(fx(0), fx(2000))).value_or(Fixed{});
        r.migration_rate = f.fixed("wanderung_rate", between(fx(0), fx(10))).value_or(Fixed{});
        r.migration_max = f.fixed("wanderung_max", unit).value_or(Fixed{});
        r.migration_reserve = f.fixed("wanderung_landreserve", between(fx(0), fx(100))).value_or(Fixed{});
        r.migration_foreign = f.fixed("wanderung_fremd", unit).value_or(Fixed{});
        r.emigration_threshold = f.fixed("auswanderung_schwelle", between(fx(0), fx(100))).value_or(Fixed{});
        r.emigration_rate = f.fixed("auswanderung_rate", between(fx(0), fx(10))).value_or(Fixed{});
        r.block_min_size = f.fixed("block_mindestgroesse", between(fx(0), fx(1'000'000))).value_or(Fixed{});
        r.migration_min_amount =
            f.fixed("wanderung_mindestmenge", between(fx(0), fx(1'000'000))).value_or(Fixed{});

        // Wirtschaft und Staatshaushalt (seit M5, docs/technik/wirtschaft.md, Abschnitt 4.9).
        const Fixed half = Fixed::from_ratio(1, 2);
        r.price_min = f.fixed("preis_min", above_up_to(fx(0), fx(1))).value_or(Fixed::one());
        r.price_max = f.fixed("preis_max", between(fx(1), fx(100))).value_or(Fixed::one());
        r.price_adjustment = f.fixed("preis_anpassung", unit).value_or(Fixed{});
        r.trade_neighbors = static_cast<std::int32_t>(f.integer("handel_nachbarn", 0, 16).value_or(0));
        r.trade_radius_km = f.fixed("handel_radius_km", between(fx(0), fx(5000))).value_or(Fixed{});
        r.trade_rate = f.fixed("handel_rate", between(fx(0), fx(100))).value_or(Fixed{});
        r.freight_land = f.fixed("handel_fracht", between(fx(0), fx(10))).value_or(Fixed{});
        r.freight_sea = f.fixed("seefracht", between(fx(0), fx(10))).value_or(Fixed{});
        r.freight_rail = f.fixed("bahnfracht", between(fx(0), fx(10))).value_or(Fixed{});
        r.trade_margin = f.fixed("handelsspanne", between(fx(0), half)).value_or(Fixed{});
        r.business_adjustment = f.fixed("betrieb_anpassung", unit).value_or(Fixed{});
        r.business_min_share = f.fixed("betrieb_min_anteil", between(fx(0), half)).value_or(Fixed{});
        r.harvest_variation_land = f.fixed("ernte_schwankung_land", between(fx(0), half)).value_or(Fixed{});
        r.harvest_variation_market =
            f.fixed("ernte_schwankung_markt", between(fx(0), half)).value_or(Fixed{});
        r.mortality_hunger = f.fixed("sterblichkeit_hunger", between(fx(0), fx(10))).value_or(Fixed{});
        r.settling_months = static_cast<std::int32_t>(f.integer("einschwingen_monate", 0, 600).value_or(0));
        r.administration_per_capita =
            f.fixed("verwaltung_je_einwohner", between(fx(0), fx(1000))).value_or(Fixed{});
        r.soldier_pay = f.fixed("sold", between(fx(0), fx(100'000))).value_or(Fixed{});
        r.reserve_years = f.fixed("reserve_jahre", between(fx(0), fx(100))).value_or(Fixed{});
        r.building_share = f.fixed("bau_anteil", unit).value_or(Fixed{});
        r.development_max = f.fixed("ausbau_max", between(fx(0), fx(10))).value_or(Fixed{});
        r.development_half =
            f.fixed("ausbau_halb", above_up_to(fx(0), fx(1'000'000'000))).value_or(Fixed::one());
        r.interest_base = f.fixed("zins_basis", unit).value_or(Fixed{});
        r.interest_risk = f.fixed("zins_risiko", unit).value_or(Fixed{});
        r.interest_max = f.fixed("zins_max", unit).value_or(Fixed{});
        r.bankruptcy_ratio = f.fixed("bankrott_quote", above_up_to(fx(0), fx(1000))).value_or(Fixed::one());
        r.bankruptcy_haircut = f.fixed("bankrott_schnitt", unit).value_or(Fixed{});
        r.bankruptcy_premium = f.fixed("bankrott_aufschlag", unit).value_or(Fixed{});
        r.bankruptcy_years = static_cast<std::int32_t>(f.integer("bankrott_jahre", 0, 200).value_or(0));
        if (r.interest_base > r.interest_max && f.has("zins_basis") && f.has("zins_max")) {
            issues_.error(file.name, found->line,
                          "'zins_basis' (" + show(r.interest_base) + ") ist größer als 'zins_max' (" +
                              show(r.interest_max) + ")");
        }
        // Die Bahn verbilligt den Handel (seit M7); teurer als die Landfracht wäre ein Datenfehler.
        if (r.freight_rail > r.freight_land && f.has("bahnfracht") && f.has("handel_fracht")) {
            issues_.error(file.name, found->line,
                          "'bahnfracht' (" + show(r.freight_rail) + ") ist größer als 'handel_fracht' (" +
                              show(r.freight_land) + ")");
        }
    }

    void load_terrains(const SourceFile& file) {
        for_each_definition(file, "gelaende", "gelaende <kennung> { … }", [&](const ScriptNode& node) {
            Fields f(node, file.name, "gelaende " + node.value, issues_,
                     {"name", "farbe", "ertrag", "tragfaehigkeit"});
            TerrainType t;
            t.id = node.value;
            t.name = f.text("name").value_or("");
            t.color = f.color("farbe").value_or(Color{});
            t.yield_factor = f.fixed("ertrag", between(fx(0), fx(10))).value_or(Fixed::one());
            // Stand der Agrartechnik zum Startdatum (seit M7): Faktor des Szenarios.
            t.capacity =
                f.fixed("tragfaehigkeit", between(fx(0), fx(100'000))).value_or(Fixed{}) * capacity_factor_;
            const auto index = static_cast<Index>(world_.terrains.size());
            if (register_id(terrains_, t.id, index, file.name, node.line, "Gelände")) {
                world_.terrains.push_back(std::move(t));
            }
        });
    }

    void load_confessions(const SourceFile& file) {
        for_each_definition(file, "konfession", "konfession <kennung> { … }", [&](const ScriptNode& node) {
            Fields f(node, file.name, "konfession " + node.value, issues_, {"name", "farbe", "bildung"});
            Confession c;
            c.id = node.value;
            c.name = f.text("name").value_or("");
            c.color = f.color("farbe").value_or(Color{});
            c.literacy_factor = f.fixed("bildung", between(fx(0), fx(10))).value_or(Fixed::one());
            const auto index = static_cast<Index>(world_.confessions.size());
            if (register_id(confessions_, c.id, index, file.name, node.line, "Konfession")) {
                world_.confessions.push_back(std::move(c));
            }
        });
    }

    void load_cultures(const SourceFile& file) {
        for_each_definition(file, "kultur", "kultur <kennung> { … }", [&](const ScriptNode& node) {
            if (!is_location_id(node.value)) {
                issues_.error(file.name, node.line,
                              "ungültige Kulturkennung " + quote(node.value) +
                                  " (nur Kleinbuchstaben a–z, Ziffern und '_')");
            }
            Fields f(node, file.name, "kultur " + node.value, issues_, {"name", "farbe"});
            Culture c;
            c.id = node.value;
            c.name = f.text("name").value_or("");
            c.color = f.color("farbe").value_or(Color{});
            const auto index = static_cast<Index>(world_.cultures.size());
            if (register_id(cultures_, c.id, index, file.name, node.line, "Kultur")) {
                world_.cultures.push_back(std::move(c));
            }
        });
        if (world_.cultures.empty()) {
            issues_.error(file.name, 0, "keine Kultur definiert (erwartet: kultur <kennung> { … })");
        }
    }

    void load_professions(const SourceFile& file) {
        for_each_definition(file, "beruf", "beruf <kennung> { … }", [&](const ScriptNode& node) {
            const bool later = is_later(file, node, "beruf");
            const LaterScope scope(in_later_, later);
            Fields f(node, file.name, "beruf " + node.value, issues_,
                     {"name", "schicht", "steuerpflichtig", "anteil_stadt", "anteil_land", "agrar", "bildung",
                      "in_stadt", "agrarabgabe", "seit", "anteil_aus", "selbststaendig"});
            Profession p;
            p.id = node.value;
            p.name = f.text("name").value_or("");
            if (const auto stratum = f.text("schicht")) {
                if (*stratum == "unten") {
                    p.stratum = Stratum::Unten;
                } else if (*stratum == "mittel") {
                    p.stratum = Stratum::Mittel;
                } else if (*stratum == "oben") {
                    p.stratum = Stratum::Oben;
                } else {
                    issues_.error(file.name, node.find("schicht")->line,
                                  "ungültige Schicht " + quote(*stratum) + " (erlaubt: unten, mittel, oben)");
                }
            }
            p.taxable = f.boolean("steuerpflichtig").value_or(true);
            p.share_city = f.fixed("anteil_stadt", between(fx(0), fx(1))).value_or(Fixed{});
            p.share_rural = f.fixed("anteil_land", between(fx(0), fx(1))).value_or(Fixed{});
            p.agrarian = f.boolean("agrar").value_or(false);
            p.literacy = f.fixed("bildung", between(fx(0), fx(1))).value_or(Fixed{});
            if (f.has("agrarabgabe")) {
                p.agrarian_levy = f.fixed("agrarabgabe", between(fx(0), fx(1))).value_or(Fixed{});
            }
            if (f.has("selbststaendig")) {
                p.self_employed = f.fixed("selbststaendig", between(fx(0), fx(1))).value_or(Fixed::one());
            }
            ProfessionRef ref;
            ref.file = file.name;
            ref.line = node.line;
            if (f.has("in_stadt")) {
                if (const auto city = f.text("in_stadt")) {
                    ref.city_profession = *city;
                    ref.line = node.find("in_stadt")->line;
                }
            }
            if (f.has("anteil_aus")) {
                if (const auto from = f.text("anteil_aus")) {
                    ref.share_from = *from;
                    ref.share_line = node.find("anteil_aus")->line;
                }
            }
            if (later) {
                later_profession_refs_.emplace_back(p.id, std::move(ref));
                return;
            }
            const auto index = static_cast<Index>(world_.professions.size());
            if (register_id(professions_, p.id, index, file.name, node.line, "Beruf")) {
                world_.professions.push_back(std::move(p));
                profession_refs_.push_back(std::move(ref));
            }
        });

        // in_stadt verweist auf einen Beruf derselben Datei (auch einen späteren).
        for (std::size_t i = 0; i < world_.professions.size(); ++i) {
            const ProfessionRef& ref = profession_refs_[i];
            if (ref.city_profession.empty()) {
                continue;
            }
            const Index target = lookup(professions_, ref.city_profession);
            if (target == kNone) {
                if (!later_reference("beruf", "Beruf", ref.city_profession, ref.file, ref.line,
                                     "beruf " + world_.professions[i].id)) {
                    issues_.error(ref.file, ref.line,
                                  "unbekannter Beruf " + quote(ref.city_profession) +
                                      " für 'in_stadt' in beruf " + world_.professions[i].id);
                }
                continue;
            }
            world_.professions[i].city_profession = target;
        }
        // Spätere Berufe: Verweise auf geltende oder ebenfalls spätere Berufe, Kennung noch frei.
        check_later_ids("beruf", professions_, "Beruf");
        for (const auto& [id, ref] : later_profession_refs_) {
            const auto known = [&](const std::string& other) {
                return lookup(professions_, other) != kNone || is_later_id("beruf", other);
            };
            if (!ref.city_profession.empty() && !known(ref.city_profession)) {
                issues_.error(ref.file, ref.line,
                              "unbekannter Beruf " + quote(ref.city_profession) +
                                  " für 'in_stadt' in beruf " + id);
            }
            if (!ref.share_from.empty() && (ref.share_from == id || !known(ref.share_from))) {
                issues_.error(ref.file, ref.share_line,
                              "unbekannter oder eigener Beruf " + quote(ref.share_from) +
                                  " für 'anteil_aus' in beruf " + id);
            }
        }

        // anteil_aus (seit M7): Ein Beruf nimmt seine Anteile einem anderen ab; so bleibt die Summe 1, auch
        // wenn es ihn erst ab einem Datum gibt (Fabrikarbeiter aus den Landarmen). Abgezogen werden die
        // angegebenen Anteile, daher hängt das Ergebnis einer Kette nicht von der Reihenfolge ab.
        const std::size_t profession_count = world_.professions.size();
        std::vector<Index> share_source(profession_count, kNone);
        for (std::size_t i = 0; i < profession_count; ++i) {
            const ProfessionRef& ref = profession_refs_[i];
            if (ref.share_from.empty()) {
                continue;
            }
            const Index source = lookup(professions_, ref.share_from);
            if (source == kNone || static_cast<std::size_t>(source) == i) {
                if (!later_reference("beruf", "Beruf", ref.share_from, ref.file, ref.share_line,
                                     "beruf " + world_.professions[i].id)) {
                    issues_.error(ref.file, ref.share_line,
                                  "unbekannter oder eigener Beruf " + quote(ref.share_from) +
                                      " für 'anteil_aus' in beruf " + world_.professions[i].id);
                }
                continue;
            }
            share_source[i] = source;
        }
        for (std::size_t i = 0; i < profession_count; ++i) {
            std::size_t steps = 0;
            for (Index s = share_source[i]; s != kNone && steps <= profession_count;
                 s = share_source[static_cast<std::size_t>(s)], ++steps) {
                if (static_cast<std::size_t>(s) == i) {
                    const ProfessionRef& ref = profession_refs_[i];
                    issues_.error(ref.file, ref.share_line,
                                  "Kreis in 'anteil_aus' über beruf " + world_.professions[i].id);
                    share_source[i] = kNone;
                    break;
                }
            }
        }
        std::vector<std::pair<Fixed, Fixed>> given(profession_count);
        for (std::size_t i = 0; i < profession_count; ++i) {
            given[i] = {world_.professions[i].share_city, world_.professions[i].share_rural};
        }
        for (std::size_t i = 0; i < profession_count; ++i) {
            if (share_source[i] != kNone) {
                Profession& from = world_.professions[static_cast<std::size_t>(share_source[i])];
                from.share_city -= given[i].first;
                from.share_rural -= given[i].second;
            }
        }
        for (std::size_t i = 0; i < profession_count; ++i) {
            if (share_source[i] == kNone) {
                continue;
            }
            const Profession& from = world_.professions[static_cast<std::size_t>(share_source[i])];
            if (from.share_city < Fixed::zero() || from.share_rural < Fixed::zero()) {
                const ProfessionRef& ref = profession_refs_[i];
                issues_.error(ref.file, ref.share_line,
                              "beruf " + from.id + " hat zu kleine Anteile für 'anteil_aus' von beruf " +
                                  world_.professions[i].id);
            }
        }

        // Summe der Anteile muss je 1 ergeben (Toleranz ±0,001).
        const Fixed tolerance = Fixed::from_ratio(1, 1000);
        Fixed sum_city;
        Fixed sum_rural;
        for (const Profession& p : world_.professions) {
            sum_city += p.share_city;
            sum_rural += p.share_rural;
        }
        const auto check_sum = [&](Fixed sum, std::string_view field) {
            const Fixed diff = sum - Fixed::one();
            if (diff > tolerance || diff < -tolerance) {
                issues_.error(file.name, 0,
                              "die Summe aller " + quote(field) + " ist " + show(sum) +
                                  ", muss aber 1 ergeben (Toleranz ±0.001)");
            }
        };
        check_sum(sum_city, "anteil_stadt");
        check_sum(sum_rural, "anteil_land");

        // Die Agrarabgaben aller Berufe zusammen höchstens 1 (seit M5).
        Fixed levies;
        for (const Profession& p : world_.professions) {
            levies += p.agrarian_levy;
        }
        if (levies > Fixed::one()) {
            issues_.error(file.name, 0,
                          "die Summe aller 'agrarabgabe' ist " + show(levies) +
                              ", höchstens 1 ist erlaubt (Anteil am Agrarertrag eines Ortes)");
        }
    }

    // ── Wirtschaft (seit M5) ──

    void load_goods(const SourceFile& file) {
        for_each_definition(file, "gut", "gut <kennung> { … }", [&](const ScriptNode& node) {
            check_id(file, node, "Gutkennung");
            const bool later = is_later(file, node, "gut");
            Fields f(node, file.name, "gut " + node.value, issues_,
                     {"name", "farbe", "basispreis", "weltmarkt", "seit"});
            Good g;
            g.id = node.value;
            g.name = f.text("name").value_or("");
            g.color = f.color("farbe").value_or(Color{});
            g.base_price = f.fixed("basispreis", above_up_to(fx(0), fx(1'000'000))).value_or(Fixed::one());
            if (f.has("weltmarkt")) {
                g.world_factor = f.fixed("weltmarkt", above_up_to(fx(0), fx(100))).value_or(Fixed{});
            }
            if (later) {
                return;
            }
            const auto index = static_cast<Index>(world_.goods.size());
            if (register_id(goods_, g.id, index, file.name, node.line, "Gut")) {
                world_.goods.push_back(std::move(g));
            }
        });
        check_later_ids("gut", goods_, "Gut");
        if (world_.goods.empty()) {
            issues_.error(file.name, 0, "kein Gut definiert (erwartet: gut <kennung> { … })");
        }
    }

    void load_needs(const SourceFile& file) {
        for_each_definition(file, "beduerfnis", "beduerfnis <kennung> { … }", [&](const ScriptNode& node) {
            check_id(file, node, "Bedürfniskennung");
            Fields f(node, file.name, "beduerfnis " + node.value, issues_,
                     {"name", "hunger", "unten", "mittel", "oben"});
            Need n;
            n.id = node.value;
            n.name = f.text("name").value_or("");
            if (f.has("hunger")) {
                n.hunger = f.boolean("hunger").value_or(false);
            }
            constexpr std::string_view strata[] = {"unten", "mittel", "oben"};
            for (std::size_t s = 0; s < 3; ++s) {
                read_amounts(f, strata[s], "Warenkorb je Kopf und Jahr, z. B. { getreide = 4.2 vieh = 0.2 }",
                             n.basket[s]);
            }
            const auto index = static_cast<Index>(world_.needs.size());
            if (register_id(needs_, n.id, index, file.name, node.line, "Bedürfnis")) {
                world_.needs.push_back(std::move(n));
            }
        });
        if (world_.needs.empty()) {
            issues_.error(file.name, 0, "kein Bedürfnis definiert (erwartet: beduerfnis <kennung> { … })");
        }
    }

    void load_businesses(const SourceFile& file) {
        for_each_definition(file, "betrieb", "betrieb <kennung> { … }", [&](const ScriptNode& node) {
            check_id(file, node, "Betriebskennung");
            const bool later = is_later(file, node, "betrieb");
            const LaterScope scope(in_later_, later);
            Fields f(node, file.name, "betrieb " + node.value, issues_,
                     {"name", "beruf", "erzeugt", "verbraucht", "agrar", "ort", "vorkommen", "handel",
                      "gewicht", "gelaende", "seit"});
            BusinessType b;
            b.id = node.value;
            b.name = f.text("name").value_or("");
            if (const auto profession = f.text("beruf")) {
                b.profession = lookup(professions_, *profession);
                if (b.profession == kNone &&
                    !later_reference("beruf", "Beruf", *profession, file.name, node.find("beruf")->line,
                                     "betrieb " + node.value)) {
                    issues_.error(file.name, node.find("beruf")->line,
                                  "unbekannter Beruf " + quote(*profession) + " in betrieb " + node.value);
                }
            }
            if (f.has("handel")) {
                b.trade = f.boolean("handel").value_or(false);
            }
            if (b.trade) {
                if (f.has("erzeugt") || f.has("verbraucht")) {
                    issues_.error(
                        file.name, node.line,
                        "betrieb " + node.value +
                            " mit 'handel = ja' erzeugt und verbraucht keine Güter (Einkommen aus der "
                            "Handelsspanne)");
                }
            } else {
                read_amounts(f, "erzeugt", "Menge je Kopf und Jahr, z. B. { tuch = 3.2 }", b.outputs);
                if (f.has("erzeugt") && b.outputs.empty() && !later) {
                    issues_.error(file.name, node.find("erzeugt")->line,
                                  "betrieb " + node.value + " erzeugt kein Gut ('erzeugt' ist leer)");
                }
                if (f.has("verbraucht")) {
                    read_amounts(f, "verbraucht", "Menge je Kopf und Jahr, z. B. { faser = 2 }", b.inputs);
                }
            }
            b.agrarian = f.boolean("agrar").value_or(false);
            if (f.has("ort")) {
                if (const auto siting = f.text("ort")) {
                    if (*siting == "alle") {
                        b.siting = Siting::All;
                    } else if (*siting == "stadt") {
                        b.siting = Siting::City;
                    } else if (*siting == "land") {
                        b.siting = Siting::Rural;
                    } else {
                        issues_.error(file.name, node.find("ort")->line,
                                      "ungültiger Wert " + quote(*siting) +
                                          " für 'ort' (erlaubt: alle, stadt, land)");
                    }
                }
            }
            if (f.has("vorkommen")) {
                if (const auto deposit = f.text("vorkommen")) {
                    b.deposit = deposit_index(file, node.find("vorkommen")->line, *deposit);
                }
            }
            b.weight = f.fixed("gewicht", above_up_to(fx(0), fx(1000))).value_or(Fixed::one());
            b.terrain_factors.assign(world_.terrains.size(), Fixed::one());
            if (f.has("gelaende")) {
                read_terrain_factors(f, b);
            }
            // Ein späterer Betrieb ist geprüft; sein Vorkommen bleibt bekannt (Orte dürfen es tragen).
            if (later) {
                return;
            }
            const auto index = static_cast<Index>(world_.businesses.size());
            if (register_id(businesses_, b.id, index, file.name, node.line, "Betrieb")) {
                world_.businesses.push_back(std::move(b));
            }
        });
        check_later_ids("betrieb", businesses_, "Betrieb");
        if (world_.businesses.empty()) {
            issues_.error(file.name, 0, "kein Betrieb definiert (erwartet: betrieb <kennung> { … })");
        }
    }

    void load_markets(const SourceFile& file) {
        for_each_definition(file, "markt", "markt <kennung> { … }", [&](const ScriptNode& node) {
            check_id(file, node, "Marktkennung");
            Fields f(node, file.name, "markt " + node.value, issues_, {"name", "landschaften"});
            Market m;
            m.id = node.value;
            m.name = f.text("name").value_or("");
            const auto index = static_cast<Index>(world_.markets.size());
            if (const ScriptNode* list =
                    f.block("landschaften", "Dateien in history/orte ohne Endung, z. B. { sachsen }")) {
                for (const ScriptNode& child : list->children) {
                    if (!child.key.empty() || child.is_block || !is_location_id(child.value)) {
                        issues_.error(file.name, child.line,
                                      "ungültiger Eintrag in 'landschaften' von markt " + node.value +
                                          " (erwartet Dateinamen aus history/orte ohne Endung)");
                        continue;
                    }
                    const auto [it, inserted] =
                        landscapes_.emplace(child.value, Definition{index, file.name, child.line});
                    if (!inserted) {
                        issues_.error(file.name, child.line,
                                      "Landschaft " + quote(child.value) + " gehört schon zu markt " +
                                          world_.markets[static_cast<std::size_t>(it->second.index)].id +
                                          " (" + it->second.file + ":" + std::to_string(it->second.line) +
                                          ")");
                        continue;
                    }
                    m.landscapes.push_back(child.value);
                }
                if (list->children.empty()) {
                    issues_.error(file.name, list->line, "markt " + node.value + " hat keine Landschaft");
                }
            }
            if (register_id(markets_, m.id, index, file.name, node.line, "Markt")) {
                world_.markets.push_back(std::move(m));
            }
        });
        if (world_.markets.empty()) {
            issues_.error(file.name, 0, "kein Markt definiert (erwartet: markt <kennung> { … })");
        }
    }

    void check_id(const SourceFile& file, const ScriptNode& node, std::string_view kind) {
        if (!is_location_id(node.value)) {
            issues_.error(file.name, node.line,
                          "ungültige " + std::string(kind) + " " + quote(node.value) +
                              " (nur Kleinbuchstaben a–z, Ziffern und '_')");
        }
    }

    /// Index eines Vorkommens; neue Kennungen werden angelegt (höchstens 32, Bitmaske im Ort).
    Index deposit_index(const SourceFile& file, int line, const std::string& id) {
        if (!is_location_id(id)) {
            issues_.error(file.name, line,
                          "ungültige Kennung " + quote(id) +
                              " für 'vorkommen' (nur Kleinbuchstaben a–z, Ziffern und '_')");
            return kNone;
        }
        if (const Index existing = lookup(deposits_, id); existing != kNone) {
            return existing;
        }
        if (world_.deposits.size() >= 32) {
            issues_.error(file.name, line, "zu viele Vorkommen (höchstens 32)");
            return kNone;
        }
        const auto index = static_cast<Index>(world_.deposits.size());
        deposits_.emplace(id, Definition{index, file.name, line});
        world_.deposits.push_back(id);
        return index;
    }

    /// Liest einen Mengenblock wie erzeugt = { tuch = 3.2 }: bekannte Güter, jedes höchstens einmal,
    /// Mengen ≥ 0.
    void read_amounts(Fields& f, std::string_view key, std::string_view example,
                      std::vector<GoodAmount>& out) {
        const ScriptNode* node = f.block(key, example);
        if (node == nullptr) {
            return;
        }
        std::set<Index> seen;
        for (const ScriptNode& child : node->children) {
            if (child.key.empty() || child.is_block || child.op != "=") {
                f.issues().error(f.file(), child.line,
                                 "ungültiger Eintrag im Block " + quote(key) + " (erwartet <gut> = <menge>)");
                continue;
            }
            const Index good = lookup(goods_, child.key);
            if (good == kNone) {
                if (!later_reference("gut", "Gut", child.key, f.file(), child.line, f.what())) {
                    f.issues().error(f.file(), child.line,
                                     "unbekanntes Gut " + quote(child.key) + " in " + f.what() +
                                         " (common/gueter.chron)");
                }
                continue;
            }
            if (!seen.insert(good).second) {
                f.issues().error(f.file(), child.line,
                                 "Gut " + quote(child.key) + " ist in " + quote(key) + " von " + f.what() +
                                     " mehrfach angegeben");
                continue;
            }
            const auto amount = Fixed::parse(child.value);
            if (!amount || *amount < Fixed::zero() || *amount > fx(1'000'000)) {
                f.issues().error(f.file(), child.line,
                                 "ungültige Menge " + quote(child.value) + " für Gut " + quote(child.key) +
                                     " (erwartet eine Zahl von 0 bis 1000000)");
                continue;
            }
            out.push_back(GoodAmount{good, *amount});
        }
    }

    void read_terrain_factors(Fields& f, BusinessType& b) {
        const ScriptNode* node = f.block("gelaende", "Faktor je Geländetyp, z. B. { wald = 4 heide = 2 }");
        if (node == nullptr) {
            return;
        }
        std::set<Index> seen;
        for (const ScriptNode& child : node->children) {
            if (child.key.empty() || child.is_block || child.op != "=") {
                f.issues().error(f.file(), child.line,
                                 "ungültiger Eintrag im Block 'gelaende' (erwartet <gelaende> = <faktor>)");
                continue;
            }
            const Index terrain = lookup(terrains_, child.key);
            if (terrain == kNone) {
                f.issues().error(f.file(), child.line,
                                 "unbekanntes Gelände " + quote(child.key) + " in " + f.what());
                continue;
            }
            if (!seen.insert(terrain).second) {
                f.issues().error(f.file(), child.line,
                                 "Gelände " + quote(child.key) + " ist in " + f.what() +
                                     " mehrfach angegeben");
                continue;
            }
            const auto factor = Fixed::parse(child.value);
            if (!factor || *factor < Fixed::zero() || *factor > fx(100)) {
                f.issues().error(f.file(), child.line,
                                 "ungültiger Faktor " + quote(child.value) + " für Gelände " +
                                     quote(child.key) + " (erwartet eine Zahl von 0 bis 100)");
                continue;
            }
            b.terrain_factors[static_cast<std::size_t>(terrain)] = *factor;
        }
    }

    // ── Politik (seit M8, docs/technik/politik.md) ──

    static std::optional<Constitution> parse_constitution(std::string_view text) {
        if (text == "keine") {
            return Constitution::None;
        }
        if (text == "staendisch") {
            return Constitution::Estates;
        }
        if (text == "konstitutionell") {
            return Constitution::Constitutional;
        }
        return std::nullopt;
    }

    static std::optional<Franchise> parse_franchise(std::string_view text) {
        if (text == "keines") {
            return Franchise::None;
        }
        if (text == "staendisch") {
            return Franchise::Estates;
        }
        if (text == "zensus") {
            return Franchise::Census;
        }
        if (text == "dreiklassen") {
            return Franchise::ThreeClass;
        }
        if (text == "allgemein_maenner") {
            return Franchise::Manhood;
        }
        return std::nullopt;
    }

    /// Wahlordnung aus wahlrecht, wahlsystem, wahl_indirekt und wahl_zensus (Abschnitt 3.5 und 3.21).
    ElectoralLaw read_electoral_law(Fields& f, const ScriptNode& block, const std::string& file,
                                    const std::string& what, bool franchise_required) {
        ElectoralLaw law;
        if (franchise_required || f.has("wahlrecht")) {
            if (const auto text = f.text("wahlrecht")) {
                if (const auto franchise = parse_franchise(*text)) {
                    law.franchise = *franchise;
                } else {
                    issues_.error(
                        file, block.find("wahlrecht")->line,
                        "ungültiges Wahlrecht " + quote(*text) + " in " + what +
                            " (erlaubt: keines, staendisch, zensus, dreiklassen, allgemein_maenner)");
                }
            }
        }
        if (f.has("wahlsystem")) {
            if (const auto text = f.text("wahlsystem")) {
                if (*text == "stichwahl") {
                    law.system = ElectoralSystem::Runoff;
                } else if (*text == "relativ") {
                    law.system = ElectoralSystem::Plurality;
                } else {
                    issues_.error(file, block.find("wahlsystem")->line,
                                  "ungültiges Wahlsystem " + quote(*text) + " in " + what +
                                      " (erlaubt: stichwahl, relativ)");
                }
            }
        }
        if (f.has("wahl_indirekt")) {
            law.indirect = f.boolean("wahl_indirekt").value_or(false);
        }
        if (law.franchise == Franchise::Census) {
            law.census_threshold = f.fixed("wahl_zensus", between(fx(0), fx(1'000'000))).value_or(Fixed{});
        } else if (f.has("wahl_zensus")) {
            issues_.error(file, block.find("wahl_zensus")->line,
                          "'wahl_zensus' gilt nur mit wahlrecht = zensus (" + what + ")");
        }
        return law;
    }

    /// Strömungen (Abschnitt 3.19): stroemung <kennung> { name farbe position }.
    void load_currents(const SourceFile& file) {
        for_each_definition(file, "stroemung", "stroemung <kennung> { … }", [&](const ScriptNode& node) {
            check_id(file, node, "Kennung einer Strömung");
            Fields f(node, file.name, "stroemung " + node.value, issues_,
                     {"name", "kurzname", "farbe", "position"});
            PoliticalCurrent c;
            c.id = node.value;
            c.name = f.text("name").value_or("");
            c.short_name = f.has("kurzname") ? f.text("kurzname").value_or(c.name) : c.name;
            c.color = f.color("farbe").value_or(Color{});
            c.position = f.fixed("position", between(fx(-100), fx(100))).value_or(Fixed{});
            const auto index = static_cast<Index>(world_.currents.size());
            if (register_id(currents_, c.id, index, file.name, node.line, "Strömung")) {
                world_.currents.push_back(std::move(c));
            }
        });
        if (world_.currents.empty()) {
            issues_.error(file.name, 0, "keine Strömung definiert (erwartet: stroemung <kennung> { … })");
        }
    }

    /// Anteile je Kennung aus einem Block { kennung = wert } (0..1), z. B. berufe = { bauern = 0.9 }.
    template <typename Resolve>
    std::vector<Fixed> read_shares(Fields& f, std::string_view key, std::string_view example,
                                   std::size_t count, std::string_view label, Resolve resolve) {
        std::vector<Fixed> out;
        const ScriptNode* node = f.block(key, example);
        if (node == nullptr) {
            return out;
        }
        out.assign(count, Fixed{});
        std::set<Index> seen;
        for (const ScriptNode& child : node->children) {
            if (child.key.empty() || child.is_block || child.op != "=") {
                f.issues().error(f.file(), child.line,
                                 "ungültiger Eintrag im Block " + quote(key) + " (erwartet " +
                                     std::string(example) + ")");
                continue;
            }
            const std::optional<Index> index = resolve(child);
            if (!index) {
                continue; // gemeldet oder späterer Inhalt
            }
            if (!seen.insert(*index).second) {
                f.issues().error(f.file(), child.line,
                                 std::string(label) + " " + quote(child.key) + " ist in " + quote(key) +
                                     " von " + f.what() + " mehrfach angegeben");
                continue;
            }
            const auto value = Fixed::parse(child.value);
            if (!value || *value < Fixed::zero() || *value > Fixed::one()) {
                f.issues().error(f.file(), child.line,
                                 "ungültiger Anteil " + quote(child.value) + " für " + quote(child.key) +
                                     " in " + f.what() + " (erwartet eine Zahl von 0 bis 1)");
                continue;
            }
            out[static_cast<std::size_t>(*index)] = *value;
        }
        return out;
    }

    /// Interessengruppen (Abschnitt 3.20): Mitglieder je Beruf und Konfession, Neigung je Strömung.
    void load_interest_groups(const SourceFile& file) {
        for_each_definition(
            file, "interessengruppe", "interessengruppe <kennung> { … }", [&](const ScriptNode& node) {
                check_id(file, node, "Kennung einer Interessengruppe");
                const bool later = is_later(file, node, "interessengruppe");
                const LaterScope scope(in_later_, later);
                Fields f(
                    node, file.name, "interessengruppe " + node.value, issues_,
                    {"name", "farbe", "berufe", "konfessionen", "nach_bildung", "neigung", "seit", "quelle"});
                InterestGroup g;
                g.id = node.value;
                g.name = f.text("name").value_or("");
                g.color = f.color("farbe").value_or(Color{});
                if (!f.has("berufe") && !f.has("konfessionen")) {
                    issues_.error(file.name, node.line,
                                  "interessengruppe " + node.value + " braucht 'berufe' oder 'konfessionen'");
                }
                if (f.has("berufe")) {
                    g.professions = read_shares(
                        f, "berufe", "<beruf> = <anteil>, z. B. { bauern = 0.9 }", world_.professions.size(),
                        "Beruf", [&](const ScriptNode& child) -> std::optional<Index> {
                            const Index index = lookup(professions_, child.key);
                            if (index == kNone &&
                                !later_reference("beruf", "Beruf", child.key, file.name, child.line,
                                                 "interessengruppe " + node.value)) {
                                issues_.error(file.name, child.line,
                                              "unbekannter Beruf " + quote(child.key) +
                                                  " in interessengruppe " + node.value +
                                                  " (common/berufe.chron)");
                            }
                            return index == kNone ? std::nullopt : std::optional<Index>(index);
                        });
                } else {
                    g.professions.assign(world_.professions.size(), Fixed::one());
                }
                if (f.has("konfessionen")) {
                    g.confessions =
                        read_shares(f, "konfessionen", "<konfession> = <anteil>, z. B. { katholisch = 0.6 }",
                                    world_.confessions.size(), "Konfession",
                                    [&](const ScriptNode& child) -> std::optional<Index> {
                                        const Index index = lookup(confessions_, child.key);
                                        if (index == kNone) {
                                            issues_.error(file.name, child.line,
                                                          "unbekannte Konfession " + quote(child.key) +
                                                              " in interessengruppe " + node.value +
                                                              " (common/konfessionen.chron)");
                                            return std::nullopt;
                                        }
                                        return index;
                                    });
                }
                if (f.has("nach_bildung")) {
                    g.by_literacy = f.boolean("nach_bildung").value_or(false);
                }
                g.leaning = read_shares(
                    f, "neigung", "<strömung> = <anteil>, z. B. { liberale = 0.6 demokraten = 0.4 }",
                    world_.currents.size(), "Strömung", [&](const ScriptNode& child) -> std::optional<Index> {
                        const Index index = lookup(currents_, child.key);
                        if (index == kNone) {
                            issues_.error(file.name, child.line,
                                          "unbekannte Strömung " + quote(child.key) +
                                              " in interessengruppe " + node.value +
                                              " (common/stroemungen.chron)");
                            return std::nullopt;
                        }
                        return index;
                    });
                if (!g.leaning.empty()) {
                    Fixed sum;
                    for (const Fixed v : g.leaning) {
                        sum += v;
                    }
                    const Fixed tolerance = Fixed::from_ratio(1, 1000);
                    if (sum < Fixed::one() - tolerance || sum > Fixed::one() + tolerance) {
                        issues_.error(file.name, node.find("neigung")->line,
                                      "die Neigung von interessengruppe " + node.value + " ergibt " +
                                          show(sum) + ", erwartet 1");
                    }
                }
                g.source = f.text("quelle").value_or("");
                if (later) {
                    return;
                }
                const auto index = static_cast<Index>(world_.interest_groups.size());
                if (register_id(interest_groups_, g.id, index, file.name, node.line, "Interessengruppe")) {
                    world_.interest_groups.push_back(std::move(g));
                }
            });
        check_later_ids("interessengruppe", interest_groups_, "Interessengruppe");
    }

    /// Parlamente (Abschnitt 3.21): parlament <kennung> { … fraktion <kennung> { … } }.
    void load_parliaments(const SourceFile& file) {
        for_each_definition(file, "parlament", "parlament <kennung> { … }", [&](const ScriptNode& node) {
            check_id(file, node, "Kennung eines Parlaments");
            const bool later = is_later(file, node, "parlament");
            const std::string what = "parlament " + node.value;
            ScriptNode plain = node;
            plain.children.clear();
            std::vector<const ScriptNode*> faction_nodes;
            for (const ScriptNode& child : node.children) {
                if (child.key == "fraktion") {
                    faction_nodes.push_back(&child);
                } else {
                    plain.children.push_back(child);
                }
            }
            Fields f(plain, file.name, what, issues_,
                     {"name", "ebene", "sitze", "seit", "wahltag", "wahlrecht", "wahlsystem", "wahl_indirekt",
                      "wahl_zensus", "fraktionen_ab", "boykott", "boykott_maerkte", "sitz", "quelle"});
            Parliament p;
            p.id = node.value;
            p.name = f.text("name").value_or("");
            ParliamentRef ref;
            ref.file = file.name;
            if (const auto level = f.text("ebene")) {
                if (*level != "bund") {
                    if (!is_state_tag(*level)) {
                        issues_.error(file.name, plain.find("ebene")->line,
                                      "ungültige Ebene " + quote(*level) + " in " + what +
                                          " (erwartet bund oder einen Staatstag)");
                    } else {
                        ref.state_tag = *level;
                        ref.line = plain.find("ebene")->line;
                    }
                }
            }
            p.seats = static_cast<std::int32_t>(f.integer("sitze", 1, 10'000).value_or(0));
            p.election_day = f.date("wahltag").value_or(Date{});
            p.law = read_electoral_law(f, plain, file.name, what, true);
            p.faction_day =
                f.has("fraktionen_ab") ? f.date("fraktionen_ab").value_or(p.election_day) : p.election_day;
            if (p.faction_day < p.election_day) {
                issues_.error(file.name, plain.find("fraktionen_ab")->line,
                              "'fraktionen_ab' liegt vor dem Wahltag von " + what);
            }
            // Eine Wahl vor dem Start fände nie statt, Wahlkampf und Spenden liefen ins Leere
            // (Review M8). Ein späteres Lesezeichen braucht ein Parlament mit eigenem Wahltag.
            if (!later && scenario_selected_ && plain.find("wahltag") != nullptr && p.election_day < start_) {
                issues_.error(file.name, plain.find("wahltag")->line,
                              "Wahltag " + format_date(p.election_day) + " von " + what +
                                  " liegt vor dem Startdatum " + format_date(start_));
            }
            if (f.has("boykott")) {
                if (const ScriptNode* list = f.block("boykott", "Liste von Kulturen, z. B. { tschechen }")) {
                    for (const ScriptNode& child : list->children) {
                        const Index culture =
                            child.key.empty() && !child.is_block ? lookup(cultures_, child.value) : kNone;
                        if (culture == kNone) {
                            issues_.error(file.name, child.line,
                                          "unbekannte Kultur " + quote(child.value) + " in 'boykott' von " +
                                              what + " (common/kulturen.chron)");
                            continue;
                        }
                        p.boycott_cultures.push_back(culture);
                    }
                }
            }
            if (f.has("boykott_maerkte")) {
                if (!f.has("boykott")) {
                    issues_.error(file.name, plain.find("boykott_maerkte")->line,
                                  "'boykott_maerkte' gilt nur mit 'boykott' in " + what);
                }
                if (const ScriptNode* list =
                        f.block("boykott_maerkte", "Liste von Märkten, z. B. { boehmen }")) {
                    if (list->children.empty()) {
                        // Leer hieße „Boykott überall“, also das Gegenteil des Gemeinten (Review M8).
                        issues_.error(file.name, list->line,
                                      "'boykott_maerkte' ist leer in " + what +
                                          " (ohne die Zeile gilt der Boykott überall)");
                    }
                    for (const ScriptNode& child : list->children) {
                        const Index market =
                            child.key.empty() && !child.is_block ? lookup(markets_, child.value) : kNone;
                        if (market == kNone) {
                            issues_.error(file.name, child.line,
                                          "unbekannter Markt " + quote(child.value) +
                                              " in 'boykott_maerkte' von " + what +
                                              " (common/maerkte.chron)");
                            continue;
                        }
                        p.boycott_markets.push_back(market);
                    }
                }
            }
            if (f.has("sitz")) {
                if (const auto seat = f.text("sitz")) {
                    ref.seat = *seat;
                    ref.seat_line = plain.find("sitz")->line;
                    ref.file = file.name;
                }
            }
            p.source = f.text("quelle").value_or("");
            p.factions = read_factions(file, node, what, faction_nodes);
            if (later) {
                return;
            }
            const auto index = static_cast<Index>(world_.parliaments.size());
            if (register_id(parliaments_, p.id, index, file.name, node.line, "Parlament")) {
                world_.parliaments.push_back(std::move(p));
                parliament_refs_.push_back(std::move(ref));
            }
        });
        check_later_ids("parlament", parliaments_, "Parlament");
    }

    /// Fraktionen eines Parlaments: Bereiche auf der Links-rechts-Achse, lückenlos von −100 bis +100.
    std::vector<Faction> read_factions(const SourceFile& file, const ScriptNode& node,
                                       const std::string& what, const std::vector<const ScriptNode*>& nodes) {
        std::vector<Faction> factions;
        std::set<std::string, std::less<>> ids;
        for (const ScriptNode* fn : nodes) {
            if (!fn->is_block || fn->value.empty()) {
                issues_.error(file.name, fn->line,
                              "'fraktion' in " + what +
                                  " erwartet: fraktion <kennung> { name farbe von bis }");
                continue;
            }
            if (!is_location_id(fn->value)) {
                issues_.error(file.name, fn->line,
                              "ungültige Kennung " + quote(fn->value) +
                                  " für eine Fraktion (nur Kleinbuchstaben a–z, Ziffern und '_')");
            }
            if (!ids.insert(fn->value).second) {
                issues_.error(file.name, fn->line,
                              "doppelte Kennung " + quote(fn->value) + " für Fraktion in " + what);
                continue;
            }
            Fields ff(*fn, file.name, "fraktion " + fn->value + " in " + what, issues_,
                      {"name", "farbe", "von", "bis"});
            Faction faction;
            faction.id = fn->value;
            faction.name = ff.text("name").value_or("");
            faction.color = ff.color("farbe").value_or(Color{});
            const auto from = ff.fixed("von", between(fx(-100), fx(100)));
            const auto to = ff.fixed("bis", between(fx(-100), fx(100)));
            if (!from || !to) {
                continue;
            }
            if (*from >= *to) {
                issues_.error(file.name, fn->line,
                              "fraktion " + fn->value + " in " + what +
                                  ": 'von' muss kleiner als 'bis' sein");
                continue;
            }
            faction.from = *from;
            faction.to = *to;
            factions.push_back(std::move(faction));
        }
        if (factions.empty()) {
            issues_.error(file.name, node.line, what + " hat keine Fraktion (fraktion <kennung> { … })");
            return factions;
        }
        std::stable_sort(factions.begin(), factions.end(),
                         [](const Faction& a, const Faction& b) { return a.from < b.from; });
        Fixed expected = fx(-100);
        for (const Faction& faction : factions) {
            if (faction.from != expected) {
                issues_.error(file.name, node.line,
                              "die Fraktionen von " + what + " decken die Achse nicht lückenlos ab (bei " +
                                  show(expected) + ", fraktion " + faction.id + " beginnt bei " +
                                  show(faction.from) + ")");
                return factions;
            }
            expected = faction.to;
        }
        if (expected != fx(100)) {
            issues_.error(file.name, node.line,
                          "die Fraktionen von " + what + " reichen nur bis " + show(expected) +
                              ", erwartet 100");
        }
        return factions;
    }

    /// Wahlkreise (Abschnitt 3.22): wahlkreis <kennung> { name parlament staat sitze orte quelle }.
    void load_electoral_districts(const SourceFile& file) {
        for_each_definition(file, "wahlkreis", "wahlkreis <kennung> { … }", [&](const ScriptNode& node) {
            check_id(file, node, "Kennung eines Wahlkreises");
            const std::string what = "wahlkreis " + node.value;
            Fields f(node, file.name, what, issues_,
                     {"name", "parlament", "staat", "sitze", "orte", "quelle"});
            ElectoralDistrict d;
            d.id = node.value;
            d.name = f.text("name").value_or("");
            const auto parliament = f.text("parlament");
            const auto state = f.text("staat");
            d.seats = static_cast<std::int32_t>(f.has("sitze") ? f.integer("sitze", 1, 1000).value_or(1) : 1);
            if (const ScriptNode* list =
                    f.block("orte", "Liste von Orten, z. B. { mannheim schwetzingen }")) {
                for (const ScriptNode& child : list->children) {
                    const Index loc =
                        child.key.empty() && !child.is_block ? lookup(locations_, child.value) : kNone;
                    if (loc == kNone) {
                        issues_.error(file.name, child.line,
                                      "unbekannter Ort " + quote(child.value) + " in " + what);
                        continue;
                    }
                    d.locations.push_back(loc);
                }
                if (list->children.empty()) {
                    issues_.error(file.name, list->line, what + " hat keinen Ort");
                }
            }
            d.source = f.text("quelle").value_or("");
            if (!parliament) {
                return;
            }
            d.parliament = lookup(parliaments_, *parliament);
            if (d.parliament == kNone) {
                // Wahlkreise eines späteren Parlaments gehören nicht zur Welt (Abschnitt 3.18).
                if (later_.count("parlament:" + *parliament) == 0) {
                    issues_.error(file.name, node.find("parlament")->line,
                                  "unbekanntes Parlament " + quote(*parliament) + " in " + what +
                                      " (common/parlamente.chron)");
                }
                return;
            }
            const auto index = static_cast<Index>(world_.electoral_districts.size());
            if (register_id(electoral_districts_, d.id, index, file.name, node.line, "Wahlkreis")) {
                const ScriptNode* state_node = node.find("staat");
                world_.electoral_districts.push_back(std::move(d));
                district_refs_.push_back(DistrictRef{state.value_or(""), file.name,
                                                     state_node != nullptr ? state_node->line : node.line});
            }
        });
    }

    /// Werte je Strömung aus einem Block { strömung = wert } im Bereich [lo, hi]; fehlende Strömungen 0.
    std::vector<Fixed> read_current_values(Fields& f, std::string_view key, Fixed lo, Fixed hi) {
        std::vector<Fixed> out(world_.currents.size(), Fixed{});
        const ScriptNode* node = f.block(key, "<strömung> = <wert>, z. B. { liberale = 0.2 }");
        if (node == nullptr) {
            return out;
        }
        std::set<Index> seen;
        for (const ScriptNode& child : node->children) {
            if (child.key.empty() || child.is_block || child.op != "=") {
                f.issues().error(f.file(), child.line,
                                 "ungültiger Eintrag im Block " + quote(key) +
                                     " (erwartet <strömung> = <wert>)");
                continue;
            }
            const Index current = lookup(currents_, child.key);
            if (current == kNone) {
                f.issues().error(f.file(), child.line,
                                 "unbekannte Strömung " + quote(child.key) + " in " + quote(key) + " von " +
                                     f.what() + " (common/stroemungen.chron)");
                continue;
            }
            if (!seen.insert(current).second) {
                f.issues().error(f.file(), child.line,
                                 "Strömung " + quote(child.key) + " ist in " + quote(key) +
                                     " mehrfach angegeben");
                continue;
            }
            const auto value = Fixed::parse(child.value);
            if (!value || *value < lo || *value > hi) {
                f.issues().error(f.file(), child.line,
                                 "ungültiger Wert " + quote(child.value) + " für " + quote(child.key) +
                                     " in " + quote(key) + " (erwartet eine Zahl von " + show(lo) + " bis " +
                                     show(hi) + ")");
                continue;
            }
            out[static_cast<std::size_t>(current)] = *value;
        }
        return out;
    }

    /// Wahlregeln und Regionen (Abschnitt 3.23): wahlregeln { … } und region <kennung> { … }.
    void load_election_rules(const SourceFile& file) {
        const ScriptNode* rules = nullptr;
        for (const ScriptNode& node : file.root.children) {
            if (node.key == "wahlregeln") {
                if (!node.is_block || !node.value.empty()) {
                    issues_.error(file.name, node.line, "ungültige Definition, erwartet: wahlregeln { … }");
                } else if (rules != nullptr) {
                    issues_.error(file.name, node.line,
                                  "Block 'wahlregeln' ist mehrfach vorhanden (zuerst in Zeile " +
                                      std::to_string(rules->line) + ")");
                } else {
                    rules = &node;
                }
                continue;
            }
            if (node.key != "region") {
                warn_unknown_top_level(file, node, "wahlregeln { … } oder region <kennung> { … }");
                continue;
            }
            if (!node.is_block || node.value.empty()) {
                issues_.error(file.name, node.line, "ungültige Definition, erwartet: region <kennung> { … }");
                continue;
            }
            check_id(file, node, "Kennung einer Region");
            Fields f(node, file.name, "region " + node.value, issues_, {"staaten", "maerkte", "neigung"});
            PoliticalRegion region;
            region.id = node.value;
            RegionRef ref;
            ref.file = file.name;
            if (f.has("staaten")) {
                if (const ScriptNode* list = f.block("staaten", "Liste von Staatstags, z. B. { BAD WUE }")) {
                    for (const ScriptNode& child : list->children) {
                        if (!child.key.empty() || child.is_block || !is_state_tag(child.value)) {
                            issues_.error(file.name, child.line,
                                          "ungültiger Eintrag in 'staaten' von region " + node.value);
                            continue;
                        }
                        ref.state_tags.emplace_back(child.value, child.line);
                    }
                }
            }
            if (f.has("maerkte")) {
                if (const ScriptNode* list = f.block("maerkte", "Liste von Märkten, z. B. { pfalz }")) {
                    for (const ScriptNode& child : list->children) {
                        const Index market =
                            child.key.empty() && !child.is_block ? lookup(markets_, child.value) : kNone;
                        if (market == kNone) {
                            issues_.error(file.name, child.line,
                                          "unbekannter Markt " + quote(child.value) + " in region " +
                                              node.value + " (common/maerkte.chron)");
                            continue;
                        }
                        region.markets.push_back(market);
                    }
                }
            }
            if (!f.has("staaten") && !f.has("maerkte")) {
                issues_.error(file.name, node.line,
                              "region " + node.value + " braucht 'staaten' oder 'maerkte'");
            }
            region.leaning = read_current_values(f, "neigung", fx(-1), fx(1));
            const auto index = static_cast<Index>(world_.political_regions.size());
            if (register_id(regions_, region.id, index, file.name, node.line, "Region")) {
                world_.political_regions.push_back(std::move(region));
                region_refs_.push_back(std::move(ref));
            }
        }
        if (rules == nullptr) {
            issues_.error(file.name, 0, "Block 'wahlregeln' fehlt");
            return;
        }
        election_rules_found_ = true;
        Fields f(*rules, file.name, "wahlregeln", issues_,
                 {"maenneranteil",       "junge_im_wahlalter", "junge_ab_24",     "beteiligung_basis",
                  "beteiligung_bildung", "beteiligung_stadt",  "beteiligung_not", "beteiligung_min",
                  "beteiligung_max",     "not_unter",          "neigung_basis",   "neigung_bildung",
                  "neigung_not",         "neigung_stadt",      "wahlmaenner",     "rauschen",
                  "zug_zur_mitte",       "position_rauschen",  "fraktionslos",    "wahlkampf"});
        ElectionRules& r = world_.election_rules;
        const Range unit = between(fx(0), fx(1));
        r.male_share = f.fixed("maenneranteil", unit).value_or(Fixed{});
        r.young_of_age = f.fixed("junge_im_wahlalter", unit).value_or(Fixed{});
        r.young_of_age_24 = f.fixed("junge_ab_24", unit).value_or(Fixed{});
        r.turnout_base = f.fixed("beteiligung_basis", unit).value_or(Fixed{});
        r.turnout_literacy = f.fixed("beteiligung_bildung", unit).value_or(Fixed{});
        r.turnout_city = f.fixed("beteiligung_stadt", unit).value_or(Fixed{});
        r.turnout_poverty = f.fixed("beteiligung_not", unit).value_or(Fixed{});
        r.turnout_min = f.fixed("beteiligung_min", unit).value_or(Fixed{});
        r.turnout_max = f.fixed("beteiligung_max", unit).value_or(Fixed::one());
        if (r.turnout_min > r.turnout_max) {
            issues_.error(file.name, rules->line, "'beteiligung_min' ist größer als 'beteiligung_max'");
        }
        r.poverty_below = f.fixed("not_unter", between(fx(0), fx(100))).value_or(Fixed{});
        r.base = read_current_values(f, "neigung_basis", fx(0), fx(1));
        r.literacy = read_current_values(f, "neigung_bildung", fx(-1), fx(1));
        r.poverty = read_current_values(f, "neigung_not", fx(-1), fx(1));
        r.city = read_current_values(f, "neigung_stadt", fx(-1), fx(1));
        r.electors.assign(world_.currents.size(), Fixed::one());
        if (f.has("wahlmaenner")) {
            const std::vector<Fixed> factors = read_current_values(f, "wahlmaenner", fx(0), fx(10));
            const ScriptNode* list = rules->find("wahlmaenner");
            for (const ScriptNode& child : list->children) {
                const Index current = lookup(currents_, child.key);
                if (current != kNone) {
                    r.electors[static_cast<std::size_t>(current)] =
                        factors[static_cast<std::size_t>(current)];
                }
            }
        }
        r.noise = f.fixed("rauschen", unit).value_or(Fixed{});
        r.position_pull = f.fixed("zug_zur_mitte", unit).value_or(Fixed{});
        r.position_noise = f.fixed("position_rauschen", between(fx(0), fx(100))).value_or(Fixed{});
        r.independent_share = f.fixed("fraktionslos", unit).value_or(Fixed{});
        // Wahlkampf (seit Schritt b2): Spenden, Fehler des Stimmungsberichts, Kosten und Wirkung je Art.
        if (const ScriptNode* node = f.block("wahlkampf", "wahlkampf { spenden = … versammlung { … } … }")) {
            Fields c(*node, file.name, "wahlkampf", issues_,
                     {"spenden", "stimmungsfehler", "versammlung", "flugblaetter", "verein"});
            r.donations = c.fixed("spenden", unit).value_or(Fixed{});
            r.poll_error = c.fixed("stimmungsfehler", between(fx(0), fx(50))).value_or(Fixed{});
            constexpr std::array<std::string_view, kCampaignActions> kinds = {"versammlung", "flugblaetter",
                                                                              "verein"};
            for (std::size_t k = 0; k < kinds.size(); ++k) {
                const ScriptNode* action = c.block(kinds[k], "{ kosten = … wirkung = … hoechstens = … }");
                if (action == nullptr) {
                    continue;
                }
                Fields a(*action, file.name, "wahlkampf " + std::string(kinds[k]), issues_,
                         {"kosten", "wirkung", "hoechstens"});
                CampaignRule& rule = r.campaign[k];
                rule.cost = a.fixed("kosten", between(fx(0), fx(1000000))).value_or(Fixed{});
                rule.effect = a.fixed("wirkung", unit).value_or(Fixed{});
                rule.cap = a.fixed("hoechstens", unit).value_or(Fixed{});
                if (rule.effect > rule.cap) {
                    issues_.error(file.name, action->line,
                                  "'wirkung' ist größer als 'hoechstens' in wahlkampf " +
                                      std::string(kinds[k]));
                }
            }
        }
    }

    /// Zollunionen (seit M9, diplomatie.md Abschnitt 3.1): zollunion <kennung> { name seit leitung tarif
    /// quelle }. Eine Union mit späterem 'seit' wird geprüft, gehört aber nicht zur Welt.
    void load_customs_unions(const SourceFile& file) {
        for_each_definition(file, "zollunion", "zollunion <kennung> { … }", [&](const ScriptNode& node) {
            check_id(file, node, "Kennung einer Zollunion");
            if (node.value == "keine") {
                issues_.error(file.name, node.line,
                              "„keine“ ist als Kennung einer Zollunion reserviert (zollunion = keine)");
            }
            const bool later = is_later(file, node, "zollunion");
            const std::string what = "zollunion " + node.value;
            Fields f(node, file.name, what, issues_,
                     {"name", "seit", "leitung", "tarif", "ausgeschlossen", "farbe", "quelle"});
            CustomsUnion u;
            u.id = node.value;
            u.name = f.text("name").value_or("");
            CustomsUnionRef ref;
            ref.file = file.name;
            if (const auto leader = f.text("leitung")) {
                ref.line = node.find("leitung")->line;
                if (is_state_tag(*leader)) {
                    ref.leader_tag = *leader;
                } else {
                    issues_.error(file.name, ref.line,
                                  "ungültiger Staatstag " + quote(*leader) + " als Leitung von " + what);
                }
            }
            const Fixed max_rate = tax_rate_max_known_ ? world_.rules.tax_rate_max : Fixed::one();
            u.tariff = f.fixed("tarif", between(fx(0), max_rate)).value_or(Fixed{});
            if (f.has("ausgeschlossen")) {
                if (const ScriptNode* list =
                        f.block("ausgeschlossen", "Liste von Staatstags, z. B. { AUT }")) {
                    for (const ScriptNode& child : list->children) {
                        if (!child.key.empty() || child.is_block || !is_state_tag(child.value)) {
                            issues_.error(file.name, child.line,
                                          "ungültiger Staatstag " + quote(child.value) +
                                              " in 'ausgeschlossen' von " + what);
                            continue;
                        }
                        ref.excluded_tags.emplace_back(child.value, child.line);
                    }
                }
            }
            if (f.has("farbe")) {
                u.color = f.color("farbe").value_or(u.color); // Kartenmodus (seit Schritt a4)
            }
            u.source = f.text("quelle").value_or("");
            if (later) {
                return;
            }
            const auto index = static_cast<Index>(world_.customs_unions.size());
            if (register_id(customs_unions_, u.id, index, file.name, node.line, "Zollunion")) {
                world_.customs_unions.push_back(std::move(u));
                customs_union_refs_.push_back(std::move(ref));
            }
        });
        check_later_ids("zollunion", customs_unions_, "Zollunion");
    }

    /// Leitmacht der Zollunionen nach drop_states_without_land: Sie muss zum Startdatum bestehen und der
    /// Union angehören.
    void resolve_customs_unions() {
        for (std::size_t i = 0; i < world_.customs_unions.size(); ++i) {
            CustomsUnion& u = world_.customs_unions[i];
            const CustomsUnionRef& ref = customs_union_refs_[i];
            // Ausgeschlossene Staaten (seit Schritt a2): Staaten ohne Ort zum Startdatum entfallen.
            for (const auto& [tag, line] : ref.excluded_tags) {
                const Index state = lookup(states_, tag);
                if (state != kNone) {
                    u.excluded.push_back(state);
                } else if (dropped_states_.count(tag) == 0) {
                    issues_.error(ref.file, line,
                                  "unbekannter Staat " + quote(tag) + " in 'ausgeschlossen' von zollunion " +
                                      u.id);
                }
            }
            if (ref.leader_tag.empty()) {
                continue; // fehlendes oder ungültiges Feld wurde bereits gemeldet
            }
            u.leader = lookup(states_, ref.leader_tag);
            if (u.leader == kNone) {
                issues_.error(ref.file, ref.line,
                              dropped_states_.count(ref.leader_tag) != 0
                                  ? "Leitmacht " + quote(ref.leader_tag) + " von zollunion " + u.id +
                                        " besitzt zum Startdatum keinen Ort"
                                  : "unbekannter Staat " + quote(ref.leader_tag) +
                                        " als Leitung von zollunion " + u.id);
            } else if (world_.states[static_cast<std::size_t>(u.leader)].customs_union !=
                       static_cast<Index>(i)) {
                issues_.error(ref.file, ref.line,
                              "Leitmacht " + ref.leader_tag + " gehört zum Startdatum nicht zu zollunion " +
                                  u.id + " (zollunion = " + u.id + " im Staat)");
            }
        }
    }

    /// Regeln der Diplomatie (seit M9, diplomatie.md Abschnitte 4.1–4.3): diplomatie { … }.
    void load_diplomacy_rules(const SourceFile& file) {
        const ScriptNode* rules = nullptr;
        for (const ScriptNode& node : file.root.children) {
            if (node.key != "diplomatie") {
                warn_unknown_top_level(file, node, "diplomatie { … }");
                continue;
            }
            if (!node.is_block || !node.value.empty()) {
                issues_.error(file.name, node.line, "ungültige Definition, erwartet: diplomatie { … }");
            } else if (rules != nullptr) {
                issues_.error(file.name, node.line,
                              "Block 'diplomatie' ist mehrfach vorhanden (zuerst in Zeile " +
                                  std::to_string(rules->line) + ")");
            } else {
                rules = &node;
            }
        }
        if (rules == nullptr) {
            issues_.error(file.name, 0, "Block 'diplomatie' fehlt");
            return;
        }
        diplomacy_rules_found_ = true;
        Fields f(*rules, file.name, "diplomatie", issues_,
                 {"drift",
                  "zollunion",
                  "bund",
                  "konfession",
                  "konfessionsgruppen",
                  "buendnis",
                  "garantie",
                  "gesandter",
                  "gesandter_grenze",
                  "gesandter_kosten",
                  "gesandter_je",
                  "buendnis_ab",
                  "garantie_ab",
                  "gegner_unter",
                  "intervention",
                  "spiel_stufe_tage",
                  "spiel_lager",
                  "spiel_bruch",
                  "spiel_stufe",
                  "spiel_nachgeben",
                  "mobilmachung"});
        DiplomacyRules& r = world_.diplomacy_rules;
        const Range relation = between(fx(-200), fx(200));
        r.drift = f.fixed("drift", between(fx(0), fx(1))).value_or(Fixed{});
        r.customs_union = f.fixed("zollunion", relation).value_or(Fixed{});
        r.federation = f.fixed("bund", relation).value_or(Fixed{});
        r.confession = f.fixed("konfession", relation).value_or(Fixed{});
        r.alliance = f.fixed("buendnis", relation).value_or(Fixed{});
        r.guarantee = f.fixed("garantie", relation).value_or(Fixed{});
        r.envoy = f.fixed("gesandter", between(fx(0), fx(200))).value_or(Fixed{});
        r.envoy_limit = f.fixed("gesandter_grenze", between(fx(0), fx(400))).value_or(Fixed{});
        r.envoy_cost = f.fixed("gesandter_kosten", between(fx(0), fx(1))).value_or(Fixed{});
        r.envoy_people = f.fixed("gesandter_je", above_up_to(fx(0), fx(1000000000))).value_or(Fixed::one());
        r.alliance_min = f.fixed("buendnis_ab", relation).value_or(Fixed{});
        r.guarantee_min = f.fixed("garantie_ab", relation).value_or(Fixed{});
        r.rival_below = f.fixed("gegner_unter", relation).value_or(Fixed{});
        // Bundesversammlung und diplomatische Spiele (seit Schritt b2).
        r.intervention = f.fixed("intervention", relation).value_or(Fixed{});
        r.play_stage_days = static_cast<std::int32_t>(
            f.fixed("spiel_stufe_tage", between(fx(1), fx(365))).value_or(fx(30)).trunc());
        r.play_leaning = f.fixed("spiel_lager", between(fx(0), fx(400))).value_or(Fixed{});
        r.play_break = f.fixed("spiel_bruch", relation).value_or(Fixed{});
        r.play_stage = f.fixed("spiel_stufe", relation).value_or(Fixed{});
        r.play_yield = f.fixed("spiel_nachgeben", between(fx(0), fx(1))).value_or(Fixed{});
        r.mobilization = f.fixed("mobilmachung", between(fx(1), fx(10))).value_or(Fixed::one());
        // Gruppen von Konfessionen, die als gleich zählen (z. B. evangelisch = { lutherisch reformiert }).
        r.confession_group.resize(world_.confessions.size());
        for (std::size_t c = 0; c < r.confession_group.size(); ++c) {
            r.confession_group[c] = static_cast<Index>(c);
        }
        if (f.has("konfessionsgruppen")) {
            if (const ScriptNode* groups =
                    f.block("konfessionsgruppen",
                            "Gruppen von Konfessionen, z. B. { evangelisch = { lutherisch reformiert } }")) {
                std::vector<bool> grouped(world_.confessions.size(), false);
                for (const ScriptNode& group : groups->children) {
                    if (group.key.empty() || !group.is_block) {
                        issues_.error(file.name, group.line,
                                      "ungültige Gruppe in 'konfessionsgruppen', erwartet: name = { … }");
                        continue;
                    }
                    Index first = kNone;
                    for (const ScriptNode& child : group.children) {
                        const Index c =
                            child.key.empty() && !child.is_block ? lookup(confessions_, child.value) : kNone;
                        if (c == kNone) {
                            issues_.error(file.name, child.line,
                                          "unbekannte Konfession " + quote(child.value) + " in Gruppe " +
                                              group.key + " (common/konfessionen.chron)");
                            continue;
                        }
                        if (grouped[static_cast<std::size_t>(c)]) {
                            issues_.error(file.name, child.line,
                                          "Konfession " + quote(child.value) + " steht in mehreren Gruppen");
                            continue;
                        }
                        grouped[static_cast<std::size_t>(c)] = true;
                        first = first == kNone ? c : first;
                        r.confession_group[static_cast<std::size_t>(c)] = first;
                    }
                }
            }
        }
        if (r.guarantee_min > r.alliance_min) {
            issues_.error(file.name, rules->line, "'garantie_ab' ist größer als 'buendnis_ab'");
        }
    }

    /// Grundbeziehungen (seit M9, Abschnitt 4.1; erzeugt vom Archivar aus tabellen/beziehungen.csv):
    /// beziehung { staaten = { AUT PRU } wert quelle }.
    void load_base_relations(const SourceFile& file) {
        for (const ScriptNode& node : file.root.children) {
            if (node.key != "beziehung") {
                warn_unknown_top_level(file, node, "beziehung { … }");
                continue;
            }
            if (!node.is_block || !node.value.empty()) {
                issues_.error(file.name, node.line, "ungültige Definition, erwartet: beziehung { … }");
                continue;
            }
            Fields f(node, file.name, "beziehung", issues_, {"staaten", "wert", "quelle"});
            BaseRelationRef ref;
            ref.file = file.name;
            ref.line = node.line;
            std::vector<std::string> tags;
            if (const ScriptNode* list = f.block("staaten", "zwei Staatstags, z. B. { AUT PRU }")) {
                for (const ScriptNode& child : list->children) {
                    if (!child.key.empty() || child.is_block || !is_state_tag(child.value)) {
                        issues_.error(file.name, child.line,
                                      "ungültiger Staatstag " + quote(child.value) +
                                          " in 'staaten' von beziehung");
                        continue;
                    }
                    tags.push_back(child.value);
                }
                if (tags.size() != 2 || tags[0] == tags[1]) {
                    issues_.error(file.name, list->line,
                                  "'staaten' einer beziehung braucht zwei verschiedene Staaten");
                    continue;
                }
            }
            const auto value = f.fixed("wert", between(fx(-200), fx(200)));
            ref.source = f.text("quelle").value_or("");
            if (tags.size() != 2 || !value) {
                continue;
            }
            ref.a_tag = tags[0];
            ref.b_tag = tags[1];
            ref.value = *value;
            base_relation_refs_.push_back(std::move(ref));
        }
    }

    /// Grundbeziehungen nach drop_states_without_land: Paare mit einem Staat ohne Ort zum Startdatum
    /// entfallen; jedes Paar darf nur einmal vorkommen. Szenarien mit Deutschem Bund brauchen die Regeln.
    void resolve_base_relations() {
        std::map<std::pair<Index, Index>, const BaseRelationRef*> seen;
        for (const BaseRelationRef& ref : base_relation_refs_) {
            bool skip = false;
            Index pair[2] = {kNone, kNone};
            const std::string* tags[2] = {&ref.a_tag, &ref.b_tag};
            for (int i = 0; i < 2; ++i) {
                pair[i] = lookup(states_, *tags[i]);
                if (pair[i] == kNone) {
                    skip = true;
                    if (dropped_states_.count(*tags[i]) == 0) {
                        issues_.error(ref.file, ref.line,
                                      "unbekannter Staat " + quote(*tags[i]) + " in beziehung");
                    }
                }
            }
            if (skip) {
                continue;
            }
            const auto key = std::minmax(pair[0], pair[1]);
            if (const auto [it, inserted] = seen.emplace(std::pair{key.first, key.second}, &ref); !inserted) {
                issues_.error(ref.file, ref.line,
                              "beziehung " + ref.a_tag + "–" + ref.b_tag + " ist doppelt (zuerst in " +
                                  it->second->file + ", Zeile " + std::to_string(it->second->line) + ")");
                continue;
            }
            world_.base_relations.push_back(BaseRelation{pair[0], pair[1], ref.value, ref.source});
        }
        const bool federation =
            std::any_of(world_.locations.begin(), world_.locations.end(),
                        [](const Location& loc) { return loc.federal && loc.owner != kNone; });
        if (federation && !diplomacy_rules_found_) {
            issues_.error(
                "common/diplomatie.chron", 0,
                "Szenarien mit Deutschem Bund brauchen die Regeln der Diplomatie (diplomatie { … })");
        }
    }

    /// Bundesversammlung (seit M9, Schritt b2; diplomatie.md Abschnitt 4.4): bundesversammlung <kennung>
    /// { name seit praesidium sitz plenum rat quelle }. Eine mit späterem 'seit' gehört nicht zur Welt.
    void load_diet(const SourceFile& file) {
        for_each_definition(
            file, "bundesversammlung", "bundesversammlung <kennung> { … }", [&](const ScriptNode& node) {
                check_id(file, node, "Kennung einer Bundesversammlung");
                const std::string what = "bundesversammlung " + node.value;
                Fields f(node, file.name, what, issues_,
                         {"name", "seit", "praesidium", "sitz", "plenum", "rat", "quelle"});
                DietRef ref;
                ref.file = file.name;
                ref.line = node.line;
                FederalDiet diet;
                diet.id = node.value;
                diet.name = f.text("name").value_or("");
                diet.source = f.text("quelle").value_or("");
                if (const auto presidency = f.text("praesidium")) {
                    ref.presidency_tag = *presidency;
                    ref.presidency_line = node.find("praesidium")->line;
                }
                if (f.has("sitz")) {
                    ref.seat = f.text("sitz").value_or("");
                    ref.seat_line = node.find("sitz")->line;
                }
                if (const ScriptNode* plenum =
                        f.block("plenum", "Stimmen je Staat, z. B. { AUT = 4 PRU = 4 }")) {
                    for (const ScriptNode& child : plenum->children) {
                        const std::int64_t votes = child.is_block ? 0 : std::atoll(child.value.c_str());
                        if (!is_state_tag(child.key) || votes < 1 || votes > 100 ||
                            std::to_string(votes) != child.value) {
                            issues_.error(file.name, child.line,
                                          "ungültiger Eintrag in 'plenum' von " + what +
                                              " (erwartet TAG = Stimmen, 1–100)");
                            continue;
                        }
                        ref.plenum.emplace_back(child.key, static_cast<std::int32_t>(votes), child.line);
                    }
                }
                if (const ScriptNode* council = f.block(
                        "rat", "Stimmen des Engeren Rats, z. B. { stimme = { staaten = { AUT } } }")) {
                    for (const ScriptNode& vote : council->children) {
                        if (vote.key != "stimme" || !vote.is_block) {
                            issues_.error(file.name, vote.line,
                                          "ungültiger Eintrag in 'rat' von " + what +
                                              ", erwartet: stimme = { … }");
                            continue;
                        }
                        Fields fv(vote, file.name, "stimme in " + what, issues_, {"name", "staaten"});
                        DietRef::Vote v;
                        v.line = vote.line;
                        v.name = fv.has("name") ? fv.text("name").value_or("") : "";
                        if (const ScriptNode* list = fv.block("staaten", "Staatstags, z. B. { WEI SCG }")) {
                            for (const ScriptNode& child : list->children) {
                                if (!child.key.empty() || child.is_block || !is_state_tag(child.value)) {
                                    issues_.error(file.name, child.line,
                                                  "ungültiger Staatstag " + quote(child.value) +
                                                      " in stimme von " + what);
                                    continue;
                                }
                                v.tags.emplace_back(child.value, child.line);
                            }
                        }
                        if (v.tags.size() > 1 && v.name.empty()) {
                            issues_.error(file.name, vote.line,
                                          "eine Kuriatstimme braucht einen Namen ('name')");
                        }
                        ref.council.push_back(std::move(v));
                    }
                }
                if (f.has("seit")) {
                    if (const auto since = f.date("seit"); since && *since > start_) {
                        return; // besteht zum Startdatum noch nicht
                    }
                }
                if (diet_ref_) {
                    issues_.error(file.name, node.line,
                                  "mehr als eine Bundesversammlung zum Startdatum (zuerst " + world_.diet.id +
                                      ")");
                    return;
                }
                world_.diet = std::move(diet);
                diet_ref_ = std::move(ref);
            });
    }

    /// Bundesversammlung nach drop_states_without_land: Staaten ohne Ort entfallen, unbekannte sind
    /// fatal; jeder Staat höchstens einmal im Plenum und im Engeren Rat.
    void resolve_diet() {
        if (!diet_ref_) {
            return;
        }
        const DietRef& ref = *diet_ref_;
        FederalDiet& diet = world_.diet;
        const auto resolve = [&](const std::string& tag, int line, std::string_view where) {
            const Index state = lookup(states_, tag);
            if (state == kNone && dropped_states_.count(tag) == 0) {
                issues_.error(ref.file, line,
                              "unbekannter Staat " + quote(tag) + " in " + std::string(where) +
                                  " von bundesversammlung " + diet.id);
            }
            return state;
        };
        diet.plenum.assign(world_.states.size(), 0);
        for (const auto& [tag, votes, line] : ref.plenum) {
            const Index state = resolve(tag, line, "'plenum'");
            if (state == kNone) {
                continue;
            }
            if (diet.plenum[static_cast<std::size_t>(state)] != 0) {
                issues_.error(ref.file, line, "Staat " + tag + " steht zweimal in 'plenum'");
                continue;
            }
            diet.plenum[static_cast<std::size_t>(state)] = votes;
        }
        std::vector<bool> seated(world_.states.size(), false);
        for (const DietRef::Vote& v : ref.council) {
            CouncilVote vote;
            vote.name = v.name;
            for (const auto& [tag, line] : v.tags) {
                const Index state = resolve(tag, line, "'rat'");
                if (state == kNone) {
                    continue;
                }
                if (seated[static_cast<std::size_t>(state)]) {
                    issues_.error(ref.file, line,
                                  "Staat " + tag + " hat mehr als eine Stimme im Engeren Rat");
                    continue;
                }
                seated[static_cast<std::size_t>(state)] = true;
                vote.states.push_back(state);
            }
            if (vote.states.empty()) {
                continue; // alle Staaten der Stimme bestehen zum Startdatum nicht
            }
            vote.id = v.tags.size() == 1 ? v.tags.front().first : v.name;
            if (vote.name.empty()) {
                vote.name = world_.states[static_cast<std::size_t>(vote.states.front())].short_name;
            }
            diet.council.push_back(std::move(vote));
        }
        if (!ref.presidency_tag.empty()) {
            diet.presidency = resolve(ref.presidency_tag, ref.presidency_line, "'praesidium'");
            if (diet.presidency == kNone && dropped_states_.count(ref.presidency_tag) != 0) {
                issues_.error(ref.file, ref.presidency_line,
                              "Präsidialmacht " + ref.presidency_tag + " besitzt zum Startdatum keinen Ort");
            }
        }
        if (!ref.seat.empty()) {
            diet.seat = lookup(locations_, ref.seat);
            if (diet.seat == kNone) {
                issues_.error(ref.file, ref.seat_line,
                              "unbekannter Ort " + quote(ref.seat) + " als Sitz von bundesversammlung " +
                                  diet.id);
            }
        }
    }

    /// Militär (seit M10, Abschnitte 3.1 und 3.2): militaer { seit fluss_rang }, heerwesen <kennung> {
    /// staat seit bewaffnung geschuetze mobilmachung mobilmachung_tage } (ohne staat: Standard) und
    /// garnison <kennung> { staat ort anteil name seit bis }.
    void load_military(const SourceFile& file) {
        MilitaryRef ref;
        ref.file = file.name;
        bool rules_found = false;
        for (const ScriptNode& node : file.root.children) {
            if (node.key == "militaer") {
                if (!node.is_block || !node.value.empty()) {
                    issues_.error(file.name, node.line, "ungültige Definition, erwartet: militaer { … }");
                    continue;
                }
                if (rules_found) {
                    issues_.error(file.name, node.line, "Block 'militaer' ist mehrfach vorhanden");
                    continue;
                }
                rules_found = true;
                ref.line = node.line;
                Fields f(node, file.name, "militaer", issues_, {"seit", "fluss_rang"});
                const std::optional<Date> since = f.date("seit");
                world_.military_rules.river_rank =
                    static_cast<std::int32_t>(f.integer("fluss_rang", 0, 6).value_or(3));
                world_.military_rules.active = since.has_value() && *since <= start_;
            } else if (node.key == "heerwesen") {
                if (!node.is_block || node.value.empty()) {
                    issues_.error(file.name, node.line,
                                  "ungültige Definition, erwartet: heerwesen <kennung> { … }");
                    continue;
                }
                check_id(file, node, "Kennung eines Heerwesens");
                const std::string what = "heerwesen " + node.value;
                Fields f(node, file.name, what, issues_,
                         {"staat", "seit", "bewaffnung", "geschuetze", "mobilmachung", "mobilmachung_tage",
                          "quelle"});
                MilitaryRef::Profile profile;
                profile.line = node.line;
                if (f.has("staat")) {
                    profile.tag = f.text("staat").value_or("");
                    if (!is_state_tag(profile.tag)) {
                        issues_.error(file.name, node.line,
                                      "ungültiger Staatstag " + quote(profile.tag) + " in " + what);
                    }
                }
                if (f.has("seit")) {
                    profile.since = f.date("seit").value_or(Date{1, 1, 1});
                }
                ArmyProfile& a = profile.profile;
                a.firepower = f.fixed("bewaffnung", between(fx(0), fx(10))).value_or(Fixed::one());
                a.guns_per_mille = f.fixed("geschuetze", between(fx(0), fx(50))).value_or(Fixed{});
                a.mobilization = f.fixed("mobilmachung", between(fx(1), fx(10))).value_or(Fixed::one());
                a.mobilization_days =
                    static_cast<std::int32_t>(f.integer("mobilmachung_tage", 1, 365).value_or(30));
                if (profile.since <= start_) {
                    ref.profiles.push_back(std::move(profile));
                }
            } else if (node.key == "garnison") {
                if (!node.is_block || node.value.empty()) {
                    issues_.error(file.name, node.line,
                                  "ungültige Definition, erwartet: garnison <kennung> { … }");
                    continue;
                }
                check_id(file, node, "Kennung einer Garnison");
                const std::string what = "garnison " + node.value;
                Fields f(node, file.name, what, issues_,
                         {"staat", "ort", "anteil", "name", "seit", "bis", "quelle"});
                MilitaryRef::GarrisonDraft g;
                g.id = node.value;
                g.line = node.line;
                g.tag = f.text("staat").value_or("");
                g.location = f.text("ort").value_or("");
                g.share = f.fixed("anteil", above_up_to(fx(0), fx(1))).value_or(Fixed{});
                g.name = f.has("name") ? f.text("name").value_or("") : "";
                const Date since = f.has("seit") ? f.date("seit").value_or(Date{1, 1, 1}) : Date{1, 1, 1};
                const Date until =
                    f.has("bis") ? f.date("bis").value_or(Date{9999, 12, 31}) : Date{9999, 12, 31};
                if (since <= start_ && start_ < until) {
                    ref.garrisons.push_back(std::move(g));
                }
            } else {
                warn_unknown_top_level(
                    file, node, "militaer { … }, heerwesen <kennung> { … } oder garnison <kennung> { … }");
            }
        }
        if (!rules_found) {
            issues_.error(file.name, 0, "Block 'militaer' fehlt");
            return;
        }
        military_ref_ = std::move(ref);
    }

    /// Militär nach drop_states_without_land (seit M10): Heerwesen je Staat (jüngster Eintrag bis zum
    /// Startdatum, sonst der Standard), Garnisonen (ohne Eintrag eine in der Hauptstadt) und die
    /// Nachbarschaft der Orte. Nur wenn das Militär zum Startdatum gilt.
    void resolve_military() {
        if (!military_ref_ || !world_.military_rules.active) {
            world_.military_rules.active = false;
            return;
        }
        const MilitaryRef& ref = *military_ref_;
        // Heerwesen: zuerst der Standard, dann je Staat der jüngste Eintrag.
        ArmyProfile standard;
        for (const MilitaryRef::Profile& p : ref.profiles) {
            if (p.tag.empty()) {
                standard = p.profile;
            }
        }
        std::vector<Date> profile_since(world_.states.size(), Date{0, 1, 1});
        for (State& state : world_.states) {
            state.army = standard;
        }
        for (const MilitaryRef::Profile& p : ref.profiles) {
            if (p.tag.empty()) {
                continue;
            }
            const Index state = lookup(states_, p.tag);
            if (state == kNone) {
                if (dropped_states_.count(p.tag) == 0) {
                    issues_.error(ref.file, p.line, "unbekannter Staat " + quote(p.tag) + " in heerwesen");
                }
                continue;
            }
            if (profile_since[static_cast<std::size_t>(state)] <= p.since) {
                profile_since[static_cast<std::size_t>(state)] = p.since;
                world_.states[static_cast<std::size_t>(state)].army = p.profile;
            }
        }
        // Garnisonen nach Staat, dann in Dateireihenfolge.
        std::vector<std::vector<Garrison>> by_state(world_.states.size());
        std::set<std::string, std::less<>> ids;
        for (const MilitaryRef::GarrisonDraft& g : ref.garrisons) {
            if (!ids.insert(g.id).second) {
                issues_.error(ref.file, g.line, "garnison " + g.id + " ist mehrfach vorhanden");
                continue;
            }
            const Index state = lookup(states_, g.tag);
            if (state == kNone) {
                if (dropped_states_.count(g.tag) == 0) {
                    issues_.error(ref.file, g.line,
                                  "unbekannter Staat " + quote(g.tag) + " in garnison " + g.id);
                }
                continue;
            }
            const Index location = lookup(locations_, g.location);
            if (location == kNone) {
                issues_.error(ref.file, g.line,
                              "unbekannter Ort " + quote(g.location) + " in garnison " + g.id);
                continue;
            }
            if (world_.locations[static_cast<std::size_t>(location)].owner != state) {
                issues_.warning(ref.file, g.line,
                                "garnison " + g.id + ": Ort " + g.location + " gehört zum Startdatum nicht " +
                                    g.tag);
            }
            by_state[static_cast<std::size_t>(state)].push_back(Garrison{
                g.id,
                g.name.empty() ? "Garnison " + world_.locations[static_cast<std::size_t>(location)].name
                               : g.name,
                state, location, g.share});
        }
        for (std::size_t s = 0; s < world_.states.size(); ++s) {
            const State& state = world_.states[s];
            if (by_state[s].empty() && state.capital != kNone) {
                const Location& capital = world_.locations[static_cast<std::size_t>(state.capital)];
                by_state[s].push_back(Garrison{state.tag + "_hauptstadt", "Garnison " + capital.name,
                                               static_cast<Index>(s), state.capital, Fixed::one()});
            }
            for (Garrison& g : by_state[s]) {
                world_.garrisons.push_back(std::move(g));
            }
        }
        load_neighbors();
    }

    /// Nachbarschaft (seit M10, Abschnitt 3.1): karte/nachbarn.csv (Kartengenerator) und, falls
    /// vorhanden, karte/ueberfahrten.csv. Je Ort aufsteigend nach Nachbarort.
    void load_neighbors() {
        world_.neighbors.assign(world_.locations.size(), {});
        const auto add = [&](const std::string& file, int line, Index a, Index b, Neighbor n) {
            auto& list = world_.neighbors[static_cast<std::size_t>(a)];
            if (std::any_of(list.begin(), list.end(), [&](const Neighbor& x) { return x.location == b; })) {
                issues_.error(file, line,
                              "Nachbarschaft " + world_.locations[static_cast<std::size_t>(a)].id + " – " +
                                  world_.locations[static_cast<std::size_t>(b)].id +
                                  " ist mehrfach vorhanden");
                return;
            }
            const Location& la = world_.locations[static_cast<std::size_t>(a)];
            const Location& lb = world_.locations[static_cast<std::size_t>(b)];
            n.distance_km = distance_km(la.longitude, la.latitude, lb.longitude, lb.latitude);
            n.location = b;
            list.push_back(n);
            n.location = a;
            world_.neighbors[static_cast<std::size_t>(b)].push_back(n);
        };
        read_csv("karte/nachbarn.csv", "ort_a;ort_b;grenze;fluss", true,
                 [&](const std::string& file, int line, const std::vector<std::string>& f) {
                     const Index a = csv_location(file, line, f[0]);
                     const Index b = csv_location(file, line, f[1]);
                     const std::int64_t border = csv_integer(file, line, f[2], "grenze", 1, 1000000);
                     const std::int64_t river = csv_integer(file, line, f[3], "fluss", 0, 6);
                     if (a == kNone || b == kNone || border < 0 || river < 0) {
                         return;
                     }
                     if (a == b) {
                         issues_.error(file, line, "Ort " + f[0] + " ist sein eigener Nachbar");
                         return;
                     }
                     Neighbor n;
                     n.border = static_cast<std::int32_t>(border);
                     n.river = static_cast<std::int32_t>(river);
                     add(file, line, a, b, n);
                 });
        read_csv("karte/ueberfahrten.csv", "ort_a;ort_b;tage;name;quelle", false,
                 [&](const std::string& file, int line, const std::vector<std::string>& f) {
                     const Index a = csv_location(file, line, f[0]);
                     const Index b = csv_location(file, line, f[1]);
                     const std::int64_t days = csv_integer(file, line, f[2], "tage", 1, 30);
                     if (a == kNone || b == kNone || days < 0) {
                         return;
                     }
                     if (a == b) {
                         issues_.error(file, line, "Überfahrt von " + f[0] + " zu sich selbst");
                         return;
                     }
                     Neighbor n;
                     n.ferry_days = static_cast<std::int32_t>(days);
                     add(file, line, a, b, n);
                 });
        for (std::size_t l = 0; l < world_.neighbors.size(); ++l) {
            auto& list = world_.neighbors[l];
            std::sort(list.begin(), list.end(),
                      [](const Neighbor& x, const Neighbor& y) { return x.location < y.location; });
            const Location& loc = world_.locations[l];
            if (list.empty() && loc.owner != kNone) {
                issues_.warning("karte/nachbarn.csv", 0,
                                "Ort " + loc.id + " hat Daten, aber keine Nachbarn (fehlt eine Überfahrt?)");
            }
        }
    }

    Index csv_location(const std::string& file, int line, const std::string& id) {
        const Index location = lookup(locations_, id);
        if (location == kNone) {
            issues_.error(file, line, "unbekannter Ort " + quote(id));
        }
        return location;
    }

    /// Ganzzahl in einer CSV-Zeile; −1 bei einem Fehler.
    std::int64_t csv_integer(const std::string& file, int line, const std::string& text,
                             std::string_view column, std::int64_t lo, std::int64_t hi) {
        const std::int64_t value = text.empty() ? -1 : std::atoll(text.c_str());
        if (text.empty() || std::to_string(value) != text || value < lo || value > hi) {
            issues_.error(file, line,
                          "ungültiger Wert " + quote(text) + " in Spalte '" + std::string(column) +
                              "' (erwartet " + std::to_string(lo) + "–" + std::to_string(hi) + ")");
            return -1;
        }
        return value;
    }

    /// Liest eine CSV-Datei mit fester Kopfzeile; handler erhält Datei, Zeile und die Felder (so viele wie
    /// der Kopf). Fehlt die Datei, ist das nur bei required ein Fehler.
    template <typename Handler>
    void read_csv(const std::string& relative, std::string_view header, bool required, Handler handler) {
        const fs::path path = data_dir_ / utf8_to_path(relative);
        std::error_code ec;
        if (!fs::is_regular_file(path, ec)) {
            if (required) {
                issues_.error(relative, 0, "Datei fehlt (erzeugt von tools/kartengenerator)");
            }
            return;
        }
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            issues_.error(relative, 0, "Datei kann nicht gelesen werden");
            return;
        }
        const std::size_t columns = split_csv(header).size();
        std::string text;
        int line = 0;
        while (std::getline(in, text)) {
            ++line;
            if (!text.empty() && text.back() == '\r') {
                text.pop_back();
            }
            if (line == 1) {
                if (text != header) {
                    issues_.error(relative, 1,
                                  "Kopfzeile " + quote(text) + ", erwartet " + quote(std::string(header)));
                    return;
                }
                continue;
            }
            if (text.empty()) {
                continue;
            }
            const std::vector<std::string> fields = split_csv(text);
            if (fields.size() != columns) {
                issues_.error(relative, line,
                              std::to_string(fields.size()) + " Felder, erwartet " + std::to_string(columns));
                continue;
            }
            handler(relative, line, fields);
        }
    }

    /// Ebene der Parlamente nach drop_states_without_land: Der Staat muss zum Startdatum bestehen.
    void resolve_parliaments() {
        for (std::size_t i = 0; i < world_.parliaments.size(); ++i) {
            const ParliamentRef& ref = parliament_refs_[i];
            if (ref.state_tag.empty()) {
                continue;
            }
            const Index state = lookup(states_, ref.state_tag);
            if (state == kNone) {
                issues_.error(ref.file, ref.line,
                              dropped_states_.count(ref.state_tag) != 0
                                  ? "Staat " + quote(ref.state_tag) + " von parlament " +
                                        world_.parliaments[i].id + " besitzt zum Startdatum keinen Ort"
                                  : "unbekannter Staat " + quote(ref.state_tag) +
                                        " als Ebene von parlament " + world_.parliaments[i].id);
                continue;
            }
            world_.parliaments[i].state = state;
        }
        for (std::size_t i = 0; i < world_.parliaments.size(); ++i) {
            const ParliamentRef& ref = parliament_refs_[i];
            if (ref.seat.empty()) {
                continue;
            }
            world_.parliaments[i].seat = lookup(locations_, ref.seat);
            if (world_.parliaments[i].seat == kNone) {
                issues_.error(ref.file, ref.seat_line,
                              "unbekannter Ort " + quote(ref.seat) + " als Sitz von parlament " +
                                  world_.parliaments[i].id);
            }
        }
        if (!world_.parliaments.empty() && !election_rules_found_) {
            const Definition& def = parliaments_.find(world_.parliaments.front().id)->second;
            issues_.error(def.file, def.line,
                          "Parlamente brauchen die Wahlregeln in common/wahlen.chron (wahlregeln { … })");
        }
        for (std::size_t i = 0; i < world_.political_regions.size(); ++i) {
            for (const auto& [tag, line] : region_refs_[i].state_tags) {
                const Index state = lookup(states_, tag);
                if (state != kNone) {
                    world_.political_regions[i].states.push_back(state);
                } else if (dropped_states_.count(tag) == 0) {
                    issues_.error(region_refs_[i].file, line,
                                  "unbekannter Staat " + quote(tag) + " in region " +
                                      world_.political_regions[i].id);
                }
            }
        }
        // Wahlkreise: Staat zum Startdatum, Orte dieses Staates, jeder Ort einmal je Parlament,
        // Sitze zusammen gleich der Sollzahl.
        std::vector<std::vector<Index>> assigned(world_.parliaments.size());
        std::vector<std::int64_t> seats(world_.parliaments.size(), 0);
        std::vector<bool> has_districts(world_.parliaments.size(), false);
        for (std::size_t i = 0; i < world_.electoral_districts.size(); ++i) {
            ElectoralDistrict& d = world_.electoral_districts[i];
            const DistrictRef& ref = district_refs_[i];
            const auto p = static_cast<std::size_t>(d.parliament);
            has_districts[p] = true;
            seats[p] += d.seats;
            const Parliament& parliament = world_.parliaments[p];
            d.state = lookup(states_, ref.state_tag);
            if (d.state == kNone) {
                issues_.error(ref.file, ref.line,
                              dropped_states_.count(ref.state_tag) != 0
                                  ? "Staat " + quote(ref.state_tag) + " von wahlkreis " + d.id +
                                        " besitzt zum Startdatum keinen Ort"
                                  : "unbekannter Staat " + quote(ref.state_tag) + " in wahlkreis " + d.id);
                continue;
            }
            if (parliament.state != kNone && d.state != parliament.state) {
                issues_.error(ref.file, ref.line,
                              "wahlkreis " + d.id + " liegt in " + ref.state_tag + ", parlament " +
                                  parliament.id + " gehört zu " +
                                  world_.states[static_cast<std::size_t>(parliament.state)].tag);
            }
            std::vector<Index>& seen = assigned[p];
            if (seen.empty()) {
                seen.assign(world_.locations.size(), kNone);
            }
            for (const Index loc : d.locations) {
                const Location& l = world_.locations[static_cast<std::size_t>(loc)];
                if (l.owner != d.state) {
                    issues_.error(ref.file, ref.line,
                                  "Ort " + l.id + " in wahlkreis " + d.id +
                                      " gehört zum Startdatum nicht zu " + ref.state_tag);
                }
                Index& first = seen[static_cast<std::size_t>(loc)];
                if (first != kNone) {
                    issues_.error(ref.file, ref.line,
                                  "Ort " + l.id + " liegt schon in wahlkreis " +
                                      world_.electoral_districts[static_cast<std::size_t>(first)].id +
                                      " desselben Parlaments");
                } else {
                    first = static_cast<Index>(i);
                }
            }
        }
        for (std::size_t p = 0; p < world_.parliaments.size(); ++p) {
            const Parliament& parliament = world_.parliaments[p];
            if (has_districts[p] && seats[p] != parliament.seats) {
                const Definition& def = parliaments_.find(parliament.id)->second;
                issues_.error(def.file, def.line,
                              "parlament " + parliament.id + " hat " + std::to_string(parliament.seats) +
                                  " Sitze, seine Wahlkreise zusammen " + std::to_string(seats[p]));
            }
        }
    }

    // ── history/ ──

    void load_states(const SourceFile& file) {
        for_each_definition(file, "staat", "staat <TAG> { … }", [&](const ScriptNode& node) {
            if (!is_state_tag(node.value)) {
                issues_.error(file.name, node.line,
                              "ungültiger Staatstag " + quote(node.value) +
                                  " (2–4 Zeichen, nur Großbuchstaben A–Z und Ziffern)");
            }
            const ScriptNode merged = apply_history(node, file.name, "staat " + node.value, {"farbe"});
            // Fehlende Pflichtfelder zählen nur für Staaten, die zum Startdatum bestehen (seit M7,
            // drop_states_without_land); die übrigen Meldungen gelten sofort.
            std::vector<LoadIssue> field_issues;
            Issues state_issues(field_issues);
            Fields f(merged, file.name, "staat " + node.value, state_issues,
                     {"name",       "kurzname",   "farbe",      "hauptstadt",    "regierungsform",
                      "herrscher",  "konfession", "schatz",     "kontribution",  "akzise",
                      "zoll",       "hofhaltung", "heer",       "schulden",      "zollunion",
                      "verfassung", "wahlrecht",  "wahlsystem", "wahl_indirekt", "wahl_zensus",
                      "quelle"});
            State s;
            s.tag = node.value;
            s.name = f.text("name").value_or("");
            s.short_name = f.text("kurzname").value_or("");
            s.color = f.color("farbe").value_or(Color{});
            const auto capital = f.text("hauptstadt");
            s.government = f.text("regierungsform").value_or("");
            s.ruler = f.text("herrscher").value_or("");
            if (const auto confession = f.text("konfession")) {
                s.confession = lookup(confessions_, *confession);
                if (s.confession == kNone) {
                    issues_.error(file.name, merged.find("konfession")->line,
                                  "unbekannte Konfession " + quote(*confession) + " in staat " + node.value);
                }
            }
            s.treasury =
                f.fixed("schatz", between(fx(-1'000'000'000'000), fx(1'000'000'000'000))).value_or(Fixed{});
            const Fixed max_rate = tax_rate_max_known_ ? world_.rules.tax_rate_max : Fixed::one();
            constexpr std::string_view tax_fields[kTaxKinds] = {"kontribution", "akzise", "zoll"};
            for (std::size_t t = 0; t < kTaxKinds; ++t) {
                s.tax_rates[t] = f.fixed(tax_fields[t], between(fx(0), max_rate)).value_or(Fixed{});
            }
            s.court_expenses = f.fixed("hofhaltung", between(fx(0), fx(1'000'000'000'000))).value_or(Fixed{});
            s.army_per_mille = f.fixed("heer", between(fx(0), fx(1000))).value_or(Fixed{});
            if (f.has("schulden")) {
                s.debt = f.fixed("schulden", between(fx(0), fx(1'000'000'000'000))).value_or(Fixed{});
            }
            // Zollunion (seit M9, diplomatie.md Abschnitt 3.1); „keine“: eigenes Zollgebiet.
            if (f.has("zollunion")) {
                if (const auto id = f.text("zollunion"); id && *id != "keine") {
                    const int line = merged.find("zollunion")->line;
                    s.customs_union = lookup(customs_unions_, *id);
                    if (s.customs_union == kNone && !later_reference("zollunion", "Zollunion", *id, file.name,
                                                                     line, "staat " + node.value)) {
                        issues_.error(file.name, line,
                                      "unbekannte Zollunion " + quote(*id) + " in staat " + node.value +
                                          " (common/zollunionen.chron)");
                    }
                }
            }
            // Verfassung und Wahlordnung des Landtags (seit M8, Abschnitt 3.5).
            if (f.has("verfassung")) {
                if (const auto text = f.text("verfassung")) {
                    if (const auto c = parse_constitution(*text)) {
                        s.constitution = *c;
                    } else {
                        issues_.error(file.name, merged.find("verfassung")->line,
                                      "ungültige Verfassung " + quote(*text) + " in staat " + node.value +
                                          " (erlaubt: keine, staendisch, konstitutionell)");
                    }
                }
            }
            s.law = read_electoral_law(f, merged, file.name, "staat " + node.value, false);
            s.source = f.text("quelle").value_or("");

            std::vector<LoadIssue> missing;
            for (LoadIssue& issue : field_issues) {
                if (issue.message.rfind("Pflichtfeld ", 0) == 0) {
                    missing.push_back(std::move(issue));
                } else {
                    issues_.add(std::move(issue));
                }
            }
            const auto index = static_cast<Index>(world_.states.size());
            if (register_id(states_, s.tag, index, file.name, node.line, "Staat")) {
                missing_state_fields_.push_back(std::move(missing));
                if (capital) {
                    const ScriptNode* capital_node = merged.find("hauptstadt");
                    capitals_.push_back(CapitalRef{*capital, file.name, capital_node->line});
                } else {
                    capitals_.push_back(CapitalRef{});
                }
                world_.states.push_back(std::move(s));
            }
        });
    }

    void load_districts(const SourceFile& file) {
        for_each_definition(file, "kreis", "kreis <kennung> { … }", [&](const ScriptNode& node) {
            if (!is_location_id(node.value)) {
                issues_.error(file.name, node.line,
                              "ungültige Kreiskennung " + quote(node.value) +
                                  " (nur Kleinbuchstaben a–z, Ziffern und '_')");
            }
            // Kreise sind in M3 nicht datiert (Abschnitt 3.9).
            ScriptNode plain = node;
            plain.children.clear();
            for (const ScriptNode& child : node.children) {
                if (looks_dated(child.key)) {
                    issues_.error(file.name, child.line,
                                  "datierter Eintrag " + quote(child.key) + " in kreis " + node.value +
                                      ": Kreise können nicht datiert werden");
                    continue;
                }
                plain.children.push_back(child);
            }
            Fields f(plain, file.name, "kreis " + node.value, issues_, {"name", "quelle"});
            District d;
            d.id = node.value;
            d.name = f.text("name").value_or("");
            d.source = f.text("quelle").value_or("");
            const auto index = static_cast<Index>(world_.districts.size());
            if (register_id(districts_, d.id, index, file.name, node.line, "Kreis")) {
                world_.districts.push_back(std::move(d));
            }
        });
    }

    /// Eisenbahnstrecken (seit M7, Abschnitt 3.17): strecke <kennung> { name quelle abschnitt = { von bis
    /// eroeffnet } … }; ein Abschnitt verbindet zwei Orte (mit oder ohne Daten).
    void load_railways(const SourceFile& file) {
        for_each_definition(file, "strecke", "strecke <kennung> { … }", [&](const ScriptNode& node) {
            if (!is_location_id(node.value)) {
                issues_.error(file.name, node.line,
                              "ungültige Kennung " + quote(node.value) +
                                  " für eine Strecke (nur Kleinbuchstaben a–z, Ziffern und '_')");
            }
            ScriptNode plain = node;
            plain.children.clear();
            std::vector<const ScriptNode*> parts;
            for (const ScriptNode& child : node.children) {
                if (child.key == "abschnitt") {
                    parts.push_back(&child);
                } else {
                    plain.children.push_back(child);
                }
            }
            Fields f(plain, file.name, "strecke " + node.value, issues_, {"name", "quelle"});
            Railway railway;
            railway.id = node.value;
            railway.name = f.text("name").value_or("");
            railway.source = f.text("quelle").value_or("");
            if (parts.empty()) {
                issues_.error(file.name, node.line,
                              "strecke " + node.value +
                                  " hat keinen Abschnitt (abschnitt = { von = … bis = … eroeffnet = … })");
            }
            for (const ScriptNode* part : parts) {
                if (!part->is_block || part->op != "=") {
                    issues_.error(file.name, part->line,
                                  "'abschnitt' in strecke " + node.value +
                                      " erwartet einen Block { von = … bis = … eroeffnet = … }");
                    continue;
                }
                Fields fs(*part, file.name, "abschnitt der strecke " + node.value, issues_,
                          {"von", "bis", "eroeffnet"});
                RailSection section;
                const auto from = fs.text("von");
                const auto to = fs.text("bis");
                const auto opened = fs.date("eroeffnet");
                const auto resolve = [&](const std::optional<std::string>& id, std::string_view key) {
                    const Index index = id ? lookup(locations_, *id) : kNone;
                    if (id && index == kNone) {
                        issues_.error(file.name, part->find(key)->line,
                                      "unbekannter Ort " + quote(*id) + " in strecke " + node.value);
                    }
                    return index;
                };
                section.from = resolve(from, "von");
                section.to = resolve(to, "bis");
                if (section.from == kNone || section.to == kNone || !opened) {
                    continue;
                }
                if (section.from == section.to) {
                    issues_.error(file.name, part->line,
                                  "Abschnitt der strecke " + node.value + " beginnt und endet in " + *from);
                    continue;
                }
                section.opened = *opened;
                railway.sections.push_back(section);
            }
            const auto index = static_cast<Index>(world_.railways.size());
            if (register_id(railways_, railway.id, index, file.name, node.line, "Strecke")) {
                world_.railways.push_back(std::move(railway));
            }
        });
    }

    /// Verwaltungseinheiten (seit M7, Abschnitt 3.16): einheit <kennung> { name art ueber quelle }.
    void load_admin_units(const SourceFile& file) {
        for_each_definition(file, "einheit", "einheit <kennung> { … }", [&](const ScriptNode& node) {
            if (!is_location_id(node.value)) {
                issues_.error(file.name, node.line,
                              "ungültige Kennung " + quote(node.value) +
                                  " für eine Verwaltungseinheit (nur Kleinbuchstaben a–z, Ziffern und '_')");
            } else if (node.value == "keine") {
                issues_.error(
                    file.name, node.line,
                    "„keine“ ist als Kennung einer Verwaltungseinheit reserviert (verwaltung = keine)");
            }
            ScriptNode plain = node;
            plain.children.clear();
            for (const ScriptNode& child : node.children) {
                if (looks_dated(child.key)) {
                    issues_.error(file.name, child.line,
                                  "datierter Eintrag " + quote(child.key) + " in einheit " + node.value +
                                      ": Verwaltungseinheiten können (noch) nicht datiert werden");
                    continue;
                }
                plain.children.push_back(child);
            }
            Fields f(plain, file.name, "einheit " + node.value, issues_, {"name", "art", "ueber", "quelle"});
            AdminUnit u;
            u.id = node.value;
            u.name = f.text("name").value_or("");
            u.kind = f.text("art").value_or("");
            u.source = f.text("quelle").value_or("");
            AdminParentRef ref;
            if (f.has("ueber")) {
                ref.id = f.text("ueber").value_or("");
                ref.file = file.name;
                ref.line = plain.find("ueber")->line;
            }
            const auto index = static_cast<Index>(world_.admin_units.size());
            if (register_id(admin_units_, u.id, index, file.name, node.line, "Verwaltungseinheit")) {
                world_.admin_units.push_back(std::move(u));
                admin_parents_.push_back(std::move(ref));
            }
        });
    }

    /// Übergeordnete Einheiten auflösen; unbekannte Verweise und Kreise im Baum sind Fehler.
    void resolve_admin_parents() {
        for (std::size_t i = 0; i < world_.admin_units.size(); ++i) {
            const AdminParentRef& ref = admin_parents_[i];
            if (ref.id.empty()) {
                continue;
            }
            const Index parent = lookup(admin_units_, ref.id);
            if (parent == kNone) {
                issues_.error(ref.file, ref.line,
                              "unbekannte Verwaltungseinheit " + quote(ref.id) + " als 'ueber' von einheit " +
                                  world_.admin_units[i].id);
                continue;
            }
            world_.admin_units[i].parent = parent;
        }
        for (std::size_t i = 0; i < world_.admin_units.size(); ++i) {
            Index current = world_.admin_units[i].parent;
            for (std::size_t steps = 0; current != kNone; ++steps) {
                if (current == static_cast<Index>(i)) {
                    const AdminParentRef& ref = admin_parents_[i];
                    issues_.error(ref.file, ref.line,
                                  "Verwaltungseinheit " + world_.admin_units[i].id +
                                      " liegt (über 'ueber') in sich selbst");
                    world_.admin_units[i].parent = kNone;
                    break;
                }
                if (steps > world_.admin_units.size()) {
                    break; // führt in einen Kreis, zu dem i nicht gehört; gemeldet wird ein Glied des Kreises
                }
                current = world_.admin_units[static_cast<std::size_t>(current)].parent;
            }
        }
    }

    void load_locations(const SourceFile& file) {
        // Markt der Landschaft (seit M5): die Datei ohne Ordner und Endung.
        std::string landscape = file.name.substr(file.name.find_last_of('/') + 1);
        landscape = landscape.substr(0, landscape.size() - std::string_view(".chron").size());
        seen_landscapes_.insert(landscape);
        const Index market = lookup(landscapes_, landscape);
        if (market == kNone && !world_.markets.empty()) {
            issues_.error(file.name, 0,
                          "Landschaft " + quote(landscape) +
                              " gehört zu keinem Markt (common/maerkte.chron)");
        }
        for_each_definition(file, "ort", "ort <kennung> { … }", [&](const ScriptNode& node) {
            if (!is_location_id(node.value)) {
                issues_.error(file.name, node.line,
                              "ungültige Ortskennung " + quote(node.value) +
                                  " (nur Kleinbuchstaben a–z, Ziffern und '_')");
            }
            const ScriptNode merged = apply_history(node, file.name, "ort " + node.value,
                                                    {"lage", "flaeche", "kreis", "vorkommen", "hafen"});
            Fields f(merged, file.name, "ort " + node.value, issues_,
                     {"name", "besitzer", "gelaende", "lage", "flaeche", "bevoelkerung", "stadt", "kreis",
                      "konfession", "kultur", "verwuestung", "vorkommen", "hafen", "verwaltung", "bund",
                      "quelle"});
            Location loc;
            LocationDraft draft;
            loc.id = node.value;
            loc.name = f.text("name").value_or("");
            loc.market = market;

            // Simulationsfelder zum Startdatum: alle fünf oder keines (Abschnitt 3.6). Ohne alle
            // ist der Ort ein Ort ohne Daten: kein Besitzer, keine Blöcke, Verwüstung 0.
            std::vector<std::string_view> present;
            std::vector<std::string_view> missing;
            for (const std::string_view key : kSimulationFields) {
                (f.has(key) ? present : missing).push_back(key);
            }
            draft.without_data = present.empty();
            if (!present.empty() && !missing.empty()) {
                issues_.error(file.name, node.line,
                              "Ort " + node.value + " hat zum Startdatum " + quote_list(present) +
                                  ", aber nicht " + quote_list(missing) +
                                  " (ein Ort hat alle fünf Simulationsfelder besitzer, bevoelkerung, "
                                  "konfession, kultur und verwuestung oder keines)");
            }

            if (f.has("besitzer")) {
                if (const auto owner = f.text("besitzer")) {
                    loc.owner = lookup(states_, *owner);
                    if (loc.owner == kNone) {
                        issues_.error(file.name, merged.find("besitzer")->line,
                                      "unbekannter Staat " + quote(*owner) + " als Besitzer von ort " +
                                          node.value);
                    }
                }
            }
            if (const auto terrain = f.text("gelaende")) {
                loc.terrain = lookup(terrains_, *terrain);
                if (loc.terrain == kNone) {
                    issues_.error(file.name, merged.find("gelaende")->line,
                                  "unbekanntes Gelände " + quote(*terrain) + " in ort " + node.value);
                }
            }
            read_position(f, loc);
            loc.area_km2 = f.fixed("flaeche", above_up_to(fx(0), fx(1'000'000))).value_or(Fixed{});
            if (f.has("bevoelkerung")) {
                draft.population = f.integer("bevoelkerung", 1, 100'000'000).value_or(0);
            }
            loc.is_city = f.boolean("stadt").value_or(false);
            if (f.has("kreis")) {
                if (const auto district = f.text("kreis")) {
                    loc.district = lookup(districts_, *district);
                    if (loc.district == kNone) {
                        issues_.error(file.name, merged.find("kreis")->line,
                                      "unbekannter Kreis " + quote(*district) + " in ort " + node.value);
                    }
                }
            }
            if (f.has("konfession")) {
                read_shares(f, "konfession", confessions_, "Konfession",
                            "Prozentwerte je Konfession, z. B. { lutherisch = 90 reformiert = 10 }",
                            draft.confessions);
            }
            if (f.has("kultur")) {
                read_shares(f, "kultur", cultures_, "Kultur",
                            "Prozentwerte je Kultur, z. B. { maerker = 100 }", draft.cultures);
            }
            if (f.has("verwuestung")) {
                loc.devastation = f.fixed("verwuestung", between(fx(0), fx(1))).value_or(Fixed{});
            }
            if (f.has("vorkommen")) {
                read_deposits(f, loc);
            }
            if (f.has("hafen")) {
                loc.is_port = f.boolean("hafen").value_or(false);
            }
            if (f.has("verwaltung")) {
                // „keine“ hebt eine frühere Zuordnung auf (der Ort liegt nicht mehr in einer Einheit).
                if (const auto unit = f.text("verwaltung"); unit && *unit != "keine") {
                    loc.admin = lookup(admin_units_, *unit);
                    if (loc.admin == kNone) {
                        issues_.error(file.name, merged.find("verwaltung")->line,
                                      "unbekannte Verwaltungseinheit " + quote(*unit) + " in ort " +
                                          node.value);
                    }
                }
            }
            if (f.has("bund")) {
                loc.federal = f.boolean("bund").value_or(false);
            }
            loc.source = f.text("quelle").value_or("");

            const auto index = static_cast<Index>(world_.locations.size());
            if (register_id(locations_, loc.id, index, file.name, node.line, "Ort")) {
                world_.locations.push_back(std::move(loc));
                drafts_.push_back(std::move(draft));
            }
        });
    }

    void read_deposits(Fields& f, Location& loc) {
        const ScriptNode* node = f.block("vorkommen", "Liste von Vorkommen, z. B. { salz weinlage }");
        if (node == nullptr) {
            return;
        }
        for (const ScriptNode& child : node->children) {
            if (!child.key.empty() || child.is_block) {
                f.issues().error(f.file(), child.line,
                                 "ungültiger Eintrag in 'vorkommen' von " + f.what() +
                                     " (erwartet Kennungen)");
                continue;
            }
            const Index deposit = lookup(deposits_, child.value);
            if (deposit == kNone) {
                f.issues().error(f.file(), child.line,
                                 "unbekanntes Vorkommen " + quote(child.value) + " in " + f.what() +
                                     " (Vorkommen kommen aus 'vorkommen' in common/betriebe.chron)");
                continue;
            }
            loc.deposits |= std::uint32_t{1} << static_cast<unsigned>(deposit);
        }
    }

    void read_position(Fields& f, Location& loc) {
        const ScriptNode* node = f.block("lage", "Längengrad und Breitengrad, z. B. { 13.40 52.52 }");
        if (node == nullptr) {
            return;
        }
        std::vector<Fixed> values;
        for (const ScriptNode& child : node->children) {
            const auto v = (child.key.empty() && !child.is_block) ? Fixed::parse(child.value) : std::nullopt;
            if (!v) {
                values.clear();
                break;
            }
            values.push_back(*v);
        }
        if (values.size() != 2) {
            f.issues().error(f.file(), node->line,
                             "'lage' erwartet genau zwei Zahlen: Längengrad Ost und Breitengrad Nord, z. B. "
                             "{ 13.40 52.52 }");
            return;
        }
        if (!between(fx(-180), fx(180)).contains(values[0]) ||
            !between(fx(-90), fx(90)).contains(values[1])) {
            f.issues().error(f.file(), node->line,
                             "'lage' außerhalb des gültigen Bereichs (Länge −180…180, Breite −90…90)");
            return;
        }
        loc.longitude = values[0];
        loc.latitude = values[1];
    }

    /// Liest einen Anteilsblock wie konfession = { lutherisch = 90 reformiert = 10 }: Ganzzahlige
    /// Prozente je bekannter Kennung, jede Kennung höchstens einmal, Summe 100.
    void read_shares(Fields& f, std::string_view key, const Registry& registry, std::string_view kind,
                     std::string_view example, std::vector<std::pair<Index, std::int64_t>>& out) {
        const ScriptNode* node = f.block(key, example);
        if (node == nullptr) {
            return;
        }
        const std::string lower_kind = key == "konfession" ? "Konfessionsanteile" : "Kulturanteile";
        std::int64_t sum = 0;
        bool ok = true;
        std::set<Index> seen;
        for (const ScriptNode& child : node->children) {
            if (child.key.empty() || child.is_block || child.op != "=") {
                f.issues().error(f.file(), child.line,
                                 "ungültiger Eintrag im Block " + quote(key) + " (erwartet <" +
                                     std::string(key) + "> = <prozent>)");
                ok = false;
                continue;
            }
            const Index index = lookup(registry, child.key);
            if (index == kNone) {
                f.issues().error(f.file(), child.line,
                                 "unbekannte " + std::string(kind) + " " + quote(child.key) + " in " +
                                     f.what());
                ok = false;
                continue;
            }
            if (!seen.insert(index).second) {
                f.issues().error(f.file(), child.line,
                                 std::string(kind) + " " + quote(child.key) + " ist in " + f.what() +
                                     " mehrfach angegeben");
                ok = false;
                continue;
            }
            const auto percent = parse_int(child.value);
            if (!percent || *percent < 0 || *percent > 100) {
                f.issues().error(f.file(), child.line,
                                 "ungültiger Prozentwert " + quote(child.value) + " für " +
                                     std::string(kind) + " " + quote(child.key) +
                                     " (erwartet eine Ganzzahl von 0 bis 100)");
                ok = false;
                continue;
            }
            sum += *percent;
            out.emplace_back(index, *percent);
        }
        if (ok && sum != 100) {
            f.issues().error(f.file(), node->line,
                             "die " + lower_kind + " in " + f.what() + " ergeben " + std::to_string(sum) +
                                 " %, erwartet 100 %");
        }
    }

    // ── szenarien/ ──

    void load_scenarios(const SourceFile& file, std::vector<Scenario>& scenarios) {
        for_each_definition(file, "szenario", "szenario <kennung> { … }", [&](const ScriptNode& node) {
            Fields f(node, file.name, "szenario " + node.value, issues_,
                     {"name", "datum", "spieler", "spielbar", "waehrung", "tragfaehigkeit_faktor"});
            Scenario s;
            s.id = node.value;
            s.name = f.text("name").value_or("");
            s.start = f.date("datum").value_or(Date{});
            if (f.has("waehrung")) {
                s.currency = f.text("waehrung").value_or("");
            }
            if (f.has("tragfaehigkeit_faktor")) {
                s.capacity_factor =
                    f.fixed("tragfaehigkeit_faktor", between(Fixed::from_ratio(1, 10), fx(10)))
                        .value_or(Fixed::one());
            }
            std::vector<PlayerRef> playable_refs;
            if (f.has("spielbar")) {
                if (const ScriptNode* list =
                        f.block("spielbar", "Liste von Staatstags, z. B. { PRU AUT BAV }")) {
                    for (const ScriptNode& child : list->children) {
                        if (!child.key.empty() || child.is_block || !is_state_tag(child.value)) {
                            issues_.error(file.name, child.line,
                                          "ungültiger Eintrag in 'spielbar' von szenario " + node.value +
                                              " (erwartet Staatstags)");
                            continue;
                        }
                        s.playable.push_back(child.value);
                        playable_refs.push_back(PlayerRef{child.value, file.name, child.line});
                    }
                }
            }
            PlayerRef player_ref;
            if (const auto player = f.text("spieler")) {
                s.player_tag = *player;
                player_ref = PlayerRef{*player, file.name, node.find("spieler")->line};
            }
            const auto index = static_cast<Index>(scenarios.size());
            if (register_id(scenarios_, s.id, index, file.name, node.line, "Szenario")) {
                scenarios.push_back(std::move(s));
                players_.push_back(std::move(player_ref));
                playable_.push_back(std::move(playable_refs));
            }
        });
    }

    // ── Datierte Historie (Abschnitt 3.8) ──

    /// Prüft alle datierten Blöcke "JJJJ.MM.TT = { … }" eines Objekts und wendet die bis
    /// zum Startdatum gültigen in zeitlicher Reihenfolge an: Ein Feld aus einem späteren
    /// Block ersetzt dasselbe Feld aus der Grundangabe oder einem früheren Block.
    /// Felder in static_keys dürfen nicht datiert werden.
    ScriptNode apply_history(const ScriptNode& node, const std::string& file, const std::string& what,
                             std::initializer_list<std::string_view> static_keys) {
        ScriptNode merged = node;
        merged.children.clear();
        std::map<Date, const ScriptNode*> dated;
        for (const ScriptNode& child : node.children) {
            if (!looks_dated(child.key)) {
                merged.children.push_back(child);
                continue;
            }
            const auto date = parse_date(child.key);
            if (!date) {
                issues_.error(file, child.line,
                              "ungültiges Datum " + quote(child.key) + " in " + what +
                                  " (erwartet JJJJ.MM.TT)");
                continue;
            }
            if (!child.is_block || child.op != "=") {
                issues_.error(file, child.line,
                              "datierter Eintrag " + quote(child.key) + " in " + what +
                                  " erwartet einen Block, z. B. " + child.key + " = { besitzer = BRA }");
                continue;
            }
            const auto [it, inserted] = dated.emplace(*date, &child);
            if (!inserted) {
                issues_.error(file, child.line,
                              "Datum " + quote(child.key) + " ist in " + what +
                                  " mehrfach angegeben (zuerst in Zeile " + std::to_string(it->second->line) +
                                  ")");
                continue;
            }
            std::set<std::string_view> keys;
            for (const ScriptNode& entry : child.children) {
                if (entry.key.empty()) {
                    issues_.error(file, entry.line,
                                  "unerwarteter Eintrag im Block " + quote(child.key) + " von " + what +
                                      " (erwartet <feld> = <wert>)");
                    continue;
                }
                if (std::find(static_keys.begin(), static_keys.end(), std::string_view(entry.key)) !=
                    static_keys.end()) {
                    issues_.error(file, entry.line,
                                  "Feld " + quote(entry.key) + " in " + what + " kann nicht datiert werden");
                }
                if (!keys.insert(entry.key).second) {
                    issues_.error(file, entry.line,
                                  "Feld " + quote(entry.key) + " ist im Block " + quote(child.key) + " von " +
                                      what + " mehrfach angegeben");
                }
            }
        }
        for (const auto& [date, block] : dated) {
            if (date > start_) {
                break;
            }
            for (const ScriptNode& entry : block->children) {
                if (entry.key.empty()) {
                    continue;
                }
                const auto existing = std::find_if(merged.children.begin(), merged.children.end(),
                                                   [&](const ScriptNode& c) { return c.key == entry.key; });
                if (existing != merged.children.end()) {
                    *existing = entry;
                } else {
                    merged.children.push_back(entry);
                }
            }
        }
        return merged;
    }

    // ── Querprüfungen ──

    /// Staaten, die zum Startdatum keinen Ort besitzen, bestehen nicht (seit M7, Abschnitt 3.5): Sie
    /// fallen aus der Welt; ihre Hauptstadt wird nicht geprüft. So lädt jedes Szenario nur die
    /// Staaten seiner Zeit, auch wenn die Daten alle Epochen enthalten.
    void drop_states_without_land() {
        std::vector<bool> owns(world_.states.size(), false);
        for (const Location& loc : world_.locations) {
            if (loc.owner >= 0 && static_cast<std::size_t>(loc.owner) < owns.size()) {
                owns[static_cast<std::size_t>(loc.owner)] = true;
            }
        }
        std::vector<Index> remap(world_.states.size(), kNone);
        std::vector<State> kept;
        std::vector<CapitalRef> kept_capitals;
        for (std::size_t i = 0; i < world_.states.size(); ++i) {
            if (!owns[i]) {
                dropped_states_.insert(world_.states[i].tag);
                continue;
            }
            for (LoadIssue& issue : missing_state_fields_[i]) {
                issues_.add(std::move(issue));
            }
            remap[i] = static_cast<Index>(kept.size());
            kept.push_back(std::move(world_.states[i]));
            kept_capitals.push_back(std::move(capitals_[i]));
        }
        world_.states = std::move(kept);
        capitals_ = std::move(kept_capitals);
        for (Location& loc : world_.locations) {
            if (loc.owner != kNone) {
                loc.owner = remap[static_cast<std::size_t>(loc.owner)];
            }
        }
        for (auto it = states_.begin(); it != states_.end();) {
            const Index index = remap[static_cast<std::size_t>(it->second.index)];
            if (index == kNone) {
                it = states_.erase(it);
            } else {
                it->second.index = index;
                ++it;
            }
        }
    }

    /// Der Spieler des gewählten Szenarios muss zum Startdatum bestehen; die Spieler der übrigen
    /// Szenarien müssen nur bekannte Staaten sein (sie bestehen zu einem anderen Datum).
    void check_players(const std::vector<Scenario>& scenarios, const Scenario* selected) {
        // Spieler und spielbare Staaten (seit M7) müssen bekannte Staaten sein; im gewählten
        // Szenario müssen sie zum Startdatum bestehen.
        const auto check = [&](const PlayerRef& ref, const Scenario& scenario, std::string_view role) {
            if (ref.tag.empty() || lookup(states_, ref.tag) != kNone) {
                return;
            }
            if (dropped_states_.count(ref.tag) == 0) {
                issues_.error(ref.file, ref.line,
                              "unbekannter Staat " + quote(ref.tag) + " als " + std::string(role) +
                                  " in szenario " + scenario.id);
            } else if (selected == &scenario) {
                issues_.error(ref.file, ref.line,
                              std::string(role == "Spieler" ? "Spielerstaat " : "Spielbarer Staat ") +
                                  quote(ref.tag) + " von szenario " + scenario.id +
                                  " besitzt zum Startdatum keinen Ort");
            }
        };
        for (std::size_t i = 0; i < scenarios.size() && i < players_.size(); ++i) {
            check(players_[i], scenarios[i], "Spieler");
            for (const PlayerRef& ref : playable_[i]) {
                check(ref, scenarios[i], "spielbarer Staat");
            }
        }
    }

    void check_capitals() {
        for (std::size_t i = 0; i < world_.states.size() && i < capitals_.size(); ++i) {
            State& state = world_.states[i];
            const CapitalRef& ref = capitals_[i];
            if (ref.location_id.empty()) {
                continue; // fehlendes Pflichtfeld wurde bereits gemeldet
            }
            const Index location = lookup(locations_, ref.location_id);
            if (location == kNone) {
                issues_.error(ref.file, ref.line,
                              "unbekannter Ort " + quote(ref.location_id) + " als Hauptstadt von " +
                                  state.tag);
                continue;
            }
            const Index owner = world_.locations[static_cast<std::size_t>(location)].owner;
            if (drafts_[static_cast<std::size_t>(location)].without_data) {
                issues_.error(ref.file, ref.line,
                              "Hauptstadt " + quote(ref.location_id) + " von " + state.tag +
                                  " ist ein Ort ohne Daten (die Hauptstadt muss ein Ort mit Daten dieses "
                                  "Staates sein)");
                continue;
            }
            if (owner != static_cast<Index>(i)) {
                issues_.error(ref.file, ref.line,
                              "Hauptstadt " + quote(ref.location_id) + " von " + state.tag +
                                  " gehört nicht zu diesem Staat");
                continue;
            }
            state.capital = location;
        }
    }

    /// Ein Kreis ohne Orte ist eine Warnung (Abschnitt 3.9).
    void check_districts() {
        std::vector<bool> used(world_.districts.size(), false);
        for (const Location& loc : world_.locations) {
            if (loc.district != kNone) {
                used[static_cast<std::size_t>(loc.district)] = true;
            }
        }
        for (std::size_t i = 0; i < world_.districts.size(); ++i) {
            if (used[i]) {
                continue;
            }
            const District& district = world_.districts[i];
            const auto it = districts_.find(district.id);
            const std::string file = it != districts_.end() ? it->second.file : std::string("history/kreise");
            const int line = it != districts_.end() ? it->second.line : 0;
            issues_.warning(file, line, "Kreis " + quote(district.id) + " hat keine Orte");
        }
    }

    /// Jede Landschaft eines Marktes braucht eine Datei in history/orte (sonst Warnung).
    void check_markets() {
        for (const auto& [landscape, definition] : landscapes_) {
            if (seen_landscapes_.find(landscape) == seen_landscapes_.end()) {
                issues_.warning(definition.file, definition.line,
                                "Landschaft " + quote(landscape) + " hat keine Datei in history/orte");
            }
        }
    }

    const Scenario* select_scenario(const std::vector<Scenario>& scenarios, std::string_view id) {
        const Index index = lookup(scenarios_, id);
        if (index != kNone) {
            return &scenarios[static_cast<std::size_t>(index)];
        }
        std::string available;
        for (const Scenario& s : scenarios) {
            available += available.empty() ? "" : ", ";
            available += s.id;
        }
        issues_.error("szenarien", 0,
                      "Szenario " + quote(id) + " nicht gefunden" +
                          (available.empty() ? std::string(" (keine Szenarien vorhanden)")
                                             : " (vorhanden: " + available + ")"));
        return nullptr;
    }

    // ── Startzustand (Abschnitt 4) ──

    /// Blöcke je Beruf, darin je Konfession, darin je Kultur (Abschnitt 4 bzw.
    /// bevoelkerung.md, Abschnitt 2.6). Lebensstandard und Alphabetisierung setzt danach
    /// initialize_population.
    void build_pops() {
        const Fixed hundred = Fixed::from_int(100);
        const Rules& rules = world_.rules;
        for (std::size_t li = 0; li < world_.locations.size(); ++li) {
            Location& loc = world_.locations[li];
            const LocationDraft& draft = drafts_[li];
            loc.first_pop = static_cast<Index>(world_.pops.size());
            for (std::size_t pi = 0; pi < world_.professions.size(); ++pi) {
                const Profession& prof = world_.professions[pi];
                const Fixed share = loc.is_city ? prof.share_city : prof.share_rural;
                if (share == Fixed::zero()) {
                    continue;
                }
                for (const auto& [confession, percent] : draft.confessions) {
                    if (percent == 0) {
                        continue;
                    }
                    for (const auto& [culture, culture_percent] : draft.cultures) {
                        if (culture_percent == 0) {
                            continue;
                        }
                        const Fixed size = Fixed::from_int(draft.population) * share *
                                           Fixed::from_int(percent) / hundred *
                                           Fixed::from_int(culture_percent) / hundred;
                        PopBlock pop;
                        pop.location = static_cast<Index>(li);
                        pop.profession = static_cast<Index>(pi);
                        pop.confession = confession;
                        pop.culture = culture;
                        pop.children = size * rules.share_children;
                        pop.young = size * rules.share_young;
                        pop.working = size * rules.share_working;
                        pop.old = size - pop.children - pop.young - pop.working;
                        world_.pops.push_back(pop);
                    }
                }
            }
            loc.pop_count = static_cast<Index>(world_.pops.size()) - loc.first_pop;
        }
    }

    fs::path data_dir_;
    Issues issues_;
    World world_;
    bool tax_rate_max_known_ = false;

    Registry terrains_;
    Registry confessions_;
    Registry cultures_;
    Registry professions_;
    Registry goods_;
    Registry needs_;
    Registry businesses_;
    Registry markets_;
    Registry deposits_;
    Registry landscapes_; // Landschaft → Markt
    std::set<std::string, std::less<>> seen_landscapes_;
    Registry states_;
    std::set<std::string, std::less<>> dropped_states_;        // zum Startdatum ohne Orte (seit M7)
    std::vector<std::vector<LoadIssue>> missing_state_fields_; // parallel zu world_.states bis zum Wegfall
    Registry districts_;
    Registry admin_units_;
    Registry railways_;
    Registry currents_;                          // seit M8
    Registry interest_groups_;                   // seit M8
    Registry parliaments_;                       // seit M8
    std::vector<ParliamentRef> parliament_refs_; // parallel zu world_.parliaments
    Registry electoral_districts_;               // seit M8
    Registry regions_;                           // seit M8
    std::vector<RegionRef> region_refs_;         // parallel zu world_.political_regions
    bool election_rules_found_ = false;          // common/wahlen.chron mit wahlregeln gelesen
    std::vector<DistrictRef> district_refs_;     // parallel zu world_.electoral_districts

    Registry customs_unions_;                         // seit M9
    std::vector<CustomsUnionRef> customs_union_refs_; // parallel zu world_.customs_unions
    std::vector<BaseRelationRef> base_relation_refs_; // seit M9 (b1), aufgelöst nach den Staaten
    bool diplomacy_rules_found_ = false;              // common/diplomatie.chron mit diplomatie gelesen
    std::optional<DietRef> diet_ref_;                 // seit M9 (b2), nur wenn sie zum Startdatum besteht
    std::optional<MilitaryRef> military_ref_;         // seit M10 (a1), common/militaer.chron
    // Inhalt ab einem Datum (seit M7): spätere Definitionen als „gut:kohle“ → 'seit'.
    struct LaterDefinition {
        Date date;
        std::string file;
        int line = 0;
    };
    std::map<std::string, LaterDefinition, std::less<>> later_;
    std::vector<std::pair<std::string, ProfessionRef>> later_profession_refs_; // spätere Berufe (seit M7)
    bool in_later_ = false;                     // gerade wird eine spätere Definition gelesen
    Fixed capacity_factor_ = Fixed::one();      // tragfaehigkeit_faktor des gewählten Szenarios (seit M7)
    std::vector<AdminParentRef> admin_parents_; // parallel zu world_.admin_units
    Registry locations_;
    Registry scenarios_;

    Date start_{9999, 12, 31};                     // Startdatum des gewählten Szenarios
    bool scenario_selected_ = false;               // ein gültiges Szenario ist gewählt
    std::vector<CapitalRef> capitals_;             // parallel zu world_.states
    std::vector<PlayerRef> players_;               // parallel zu den Szenarien
    std::vector<std::vector<PlayerRef>> playable_; // spielbare Staaten, parallel zu den Szenarien
    std::vector<LocationDraft> drafts_;            // parallel zu world_.locations
    std::vector<ProfessionRef> profession_refs_;   // parallel zu world_.professions
};

} // namespace

std::vector<Scenario> list_scenarios(const std::filesystem::path& data_dir, std::vector<LoadIssue>& issues) {
    Loader loader(data_dir, issues);
    return loader.read_scenarios();
}

LoadResult load_world(const std::filesystem::path& data_dir, std::string_view scenario_id) {
    LoadResult result;
    Loader loader(data_dir, result.issues);
    result.world = loader.run(scenario_id);
    return result;
}

std::optional<std::filesystem::path> find_data_dir(const std::filesystem::path& start) {
    if (const auto env = read_env("UHRWERK_DATEN"); env && !env->empty()) {
        return std::filesystem::path(*env);
    }
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(start, ec);
    if (ec) {
        dir = start;
    }
    dir = dir.lexically_normal();
    if (!dir.has_filename() && dir.has_parent_path()) {
        dir = dir.parent_path(); // abschließenden Trenner entfernen
    }
    for (;;) {
        const std::filesystem::path candidate = dir / "data";
        if (std::filesystem::is_directory(candidate / "szenarien", ec)) {
            return candidate;
        }
        const std::filesystem::path parent = dir.parent_path();
        if (parent.empty() || parent == dir) {
            return std::nullopt;
        }
        dir = parent;
    }
}

} // namespace uhrwerk
