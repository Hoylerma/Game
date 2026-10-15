#include "uhrwerk/loader.hpp"

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

        // Szenarien vor der Historie: Das Startdatum entscheidet, welche datierten
        // Blöcke gelten (Abschnitt 3.8).
        std::vector<Scenario> scenarios;
        for (const std::string& name : list_folder("szenarien")) {
            process(name, [&](const SourceFile& f) { load_scenarios(f, scenarios); });
        }
        const Scenario* selected = select_scenario(scenarios, scenario_id);
        // Ohne gültiges Szenario werden alle datierten Blöcke angewendet, damit die
        // Prüfung trotzdem alle Felder sieht.
        start_ = selected != nullptr ? selected->start : Date{9999, 12, 31};

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

        drop_states_without_land();
        check_players(scenarios, selected);
        check_capitals();
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
            t.capacity = f.fixed("tragfaehigkeit", between(fx(0), fx(100'000))).value_or(Fixed{});
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
            Fields f(node, file.name, "beruf " + node.value, issues_,
                     {"name", "schicht", "steuerpflichtig", "anteil_stadt", "anteil_land", "agrar", "bildung",
                      "in_stadt", "agrarabgabe"});
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
            ProfessionRef ref{"", file.name, node.line};
            if (f.has("in_stadt")) {
                if (const auto city = f.text("in_stadt")) {
                    ref = ProfessionRef{*city, file.name, node.find("in_stadt")->line};
                }
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
                issues_.error(ref.file, ref.line,
                              "unbekannter Beruf " + quote(ref.city_profession) +
                                  " für 'in_stadt' in beruf " + world_.professions[i].id);
                continue;
            }
            world_.professions[i].city_profession = target;
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
            Fields f(node, file.name, "gut " + node.value, issues_,
                     {"name", "farbe", "basispreis", "weltmarkt"});
            Good g;
            g.id = node.value;
            g.name = f.text("name").value_or("");
            g.color = f.color("farbe").value_or(Color{});
            g.base_price = f.fixed("basispreis", above_up_to(fx(0), fx(1'000'000))).value_or(Fixed::one());
            if (f.has("weltmarkt")) {
                g.world_factor = f.fixed("weltmarkt", above_up_to(fx(0), fx(100))).value_or(Fixed{});
            }
            const auto index = static_cast<Index>(world_.goods.size());
            if (register_id(goods_, g.id, index, file.name, node.line, "Gut")) {
                world_.goods.push_back(std::move(g));
            }
        });
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
            Fields f(node, file.name, "betrieb " + node.value, issues_,
                     {"name", "beruf", "erzeugt", "verbraucht", "agrar", "ort", "vorkommen", "handel",
                      "gewicht", "gelaende"});
            BusinessType b;
            b.id = node.value;
            b.name = f.text("name").value_or("");
            if (const auto profession = f.text("beruf")) {
                b.profession = lookup(professions_, *profession);
                if (b.profession == kNone) {
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
                if (f.has("erzeugt") && b.outputs.empty()) {
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
            const auto index = static_cast<Index>(world_.businesses.size());
            if (register_id(businesses_, b.id, index, file.name, node.line, "Betrieb")) {
                world_.businesses.push_back(std::move(b));
            }
        });
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
                f.issues().error(f.file(), child.line,
                                 "unbekanntes Gut " + quote(child.key) + " in " + f.what() +
                                     " (common/gueter.chron)");
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
                     {"name", "kurzname", "farbe", "hauptstadt", "regierungsform", "herrscher", "konfession",
                      "schatz", "kontribution", "akzise", "zoll", "hofhaltung", "heer", "schulden",
                      "zollverein", "quelle"});
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
            if (f.has("zollverein")) {
                s.customs_union = f.boolean("zollverein").value_or(false);
            }
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
                     {"name", "datum", "spieler", "spielbar", "waehrung"});
            Scenario s;
            s.id = node.value;
            s.name = f.text("name").value_or("");
            s.start = f.date("datum").value_or(Date{});
            if (f.has("waehrung")) {
                s.currency = f.text("waehrung").value_or("");
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
    std::vector<AdminParentRef> admin_parents_; // parallel zu world_.admin_units
    Registry locations_;
    Registry scenarios_;

    Date start_{9999, 12, 31};                     // Startdatum des gewählten Szenarios
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
