#include "uhrwerk/loader.hpp"

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
    std::int64_t population = 0;
    std::vector<std::pair<Index, std::int64_t>> confessions; // Konfession, Prozent (Reihenfolge der Datei)
};

struct CapitalRef {
    std::string location_id;
    std::string file;
    int line = 0;
};

class Loader {
public:
    Loader(fs::path data_dir, std::vector<LoadIssue>& issues)
        : data_dir_(std::move(data_dir)), issues_(issues) {}

    std::optional<World> run(std::string_view scenario_id) {
        std::error_code ec;
        if (!fs::is_directory(data_dir_, ec)) {
            issues_.error(path_to_utf8(data_dir_), 0, "Datenordner nicht gefunden");
            return std::nullopt;
        }

        process("common/regeln.chron", [&](const SourceFile& f) { load_rules(f); });
        process("common/gelaende.chron", [&](const SourceFile& f) { load_terrains(f); });
        process("common/konfessionen.chron", [&](const SourceFile& f) { load_confessions(f); });
        process("common/berufe.chron", [&](const SourceFile& f) { load_professions(f); });
        for (const std::string& name : list_folder("history/staaten")) {
            process(name, [&](const SourceFile& f) { load_states(f); });
        }
        for (const std::string& name : list_folder("history/orte")) {
            process(name, [&](const SourceFile& f) { load_locations(f); });
        }
        std::vector<Scenario> scenarios;
        for (const std::string& name : list_folder("szenarien")) {
            process(name, [&](const SourceFile& f) { load_scenarios(f, scenarios); });
        }

        check_capitals();
        const Scenario* selected = select_scenario(scenarios, scenario_id);

        if (issues_.has_errors() || selected == nullptr) {
            return std::nullopt;
        }

        world_.scenario = *selected;
        world_.date = selected->start;
        build_pops();
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

    /// Alle *.chron-Dateien eines Ordners, nach Dateiname sortiert.
    std::vector<std::string> list_folder(const std::string& folder) {
        const fs::path dir = data_dir_ / fs::path(folder);
        std::error_code ec;
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
        if (names.empty() && !ec) {
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
                 {"wachstum_basis", "wachstum_wiederaufbau", "wachstum_steuerdruck", "verwuestung_abbau",
                  "verwuestung_ertrag", "seuche_chance", "seuche_verlust_min", "seuche_verlust_max",
                  "steuersatz_max"});
        Rules& r = world_.rules;
        const Range unit = between(fx(0), fx(1));
        const auto growth_base = f.fixed("wachstum_basis", between(fx(-1), fx(1)));
        const auto growth_reconstruction = f.fixed("wachstum_wiederaufbau", unit);
        const auto growth_tax_pressure = f.fixed("wachstum_steuerdruck", unit);
        const auto devastation_decay = f.fixed("verwuestung_abbau", unit);
        const auto devastation_yield_loss = f.fixed("verwuestung_ertrag", unit);
        const auto plague_chance = f.fixed("seuche_chance", unit);
        const auto plague_loss_min = f.fixed("seuche_verlust_min", unit);
        const auto plague_loss_max = f.fixed("seuche_verlust_max", unit);
        const auto tax_rate_max = f.fixed("steuersatz_max", unit);

        r.growth_base = growth_base.value_or(Fixed{});
        r.growth_reconstruction = growth_reconstruction.value_or(Fixed{});
        r.growth_tax_pressure = growth_tax_pressure.value_or(Fixed{});
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
    }

    void load_terrains(const SourceFile& file) {
        for_each_definition(file, "gelaende", "gelaende <kennung> { … }", [&](const ScriptNode& node) {
            Fields f(node, file.name, "gelaende " + node.value, issues_,
                     {"name", "farbe", "wachstum", "ertrag"});
            TerrainType t;
            t.id = node.value;
            t.name = f.text("name").value_or("");
            t.color = f.color("farbe").value_or(Color{});
            t.growth_factor = f.fixed("wachstum", between(fx(0), fx(10))).value_or(Fixed::one());
            t.yield_factor = f.fixed("ertrag", between(fx(0), fx(10))).value_or(Fixed::one());
            const auto index = static_cast<Index>(world_.terrains.size());
            if (register_id(terrains_, t.id, index, file.name, node.line, "Gelände")) {
                world_.terrains.push_back(std::move(t));
            }
        });
    }

    void load_confessions(const SourceFile& file) {
        for_each_definition(file, "konfession", "konfession <kennung> { … }", [&](const ScriptNode& node) {
            Fields f(node, file.name, "konfession " + node.value, issues_, {"name", "farbe"});
            Confession c;
            c.id = node.value;
            c.name = f.text("name").value_or("");
            c.color = f.color("farbe").value_or(Color{});
            const auto index = static_cast<Index>(world_.confessions.size());
            if (register_id(confessions_, c.id, index, file.name, node.line, "Konfession")) {
                world_.confessions.push_back(std::move(c));
            }
        });
    }

    void load_professions(const SourceFile& file) {
        for_each_definition(file, "beruf", "beruf <kennung> { … }", [&](const ScriptNode& node) {
            Fields f(node, file.name, "beruf " + node.value, issues_,
                     {"name", "schicht", "ertrag", "steuerpflichtig", "anteil_stadt", "anteil_land"});
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
            p.yield_per_capita = f.fixed("ertrag", between(fx(0), fx(1000))).value_or(Fixed{});
            p.taxable = f.boolean("steuerpflichtig").value_or(true);
            p.share_city = f.fixed("anteil_stadt", between(fx(0), fx(1))).value_or(Fixed{});
            p.share_rural = f.fixed("anteil_land", between(fx(0), fx(1))).value_or(Fixed{});
            const auto index = static_cast<Index>(world_.professions.size());
            if (register_id(professions_, p.id, index, file.name, node.line, "Beruf")) {
                world_.professions.push_back(std::move(p));
            }
        });

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
    }

    // ── history/ ──

    void load_states(const SourceFile& file) {
        for_each_definition(file, "staat", "staat <TAG> { … }", [&](const ScriptNode& node) {
            if (!is_state_tag(node.value)) {
                issues_.error(file.name, node.line,
                              "ungültiger Staatstag " + quote(node.value) +
                                  " (2–4 Zeichen, nur Großbuchstaben A–Z und Ziffern)");
            }
            Fields f(node, file.name, "staat " + node.value, issues_,
                     {"name", "kurzname", "farbe", "hauptstadt", "regierungsform", "herrscher", "konfession",
                      "schatz", "steuersatz", "hofhaltung", "quelle"});
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
                    issues_.error(file.name, node.find("konfession")->line,
                                  "unbekannte Konfession " + quote(*confession) + " in staat " + node.value);
                }
            }
            s.treasury =
                f.fixed("schatz", between(fx(-1'000'000'000'000), fx(1'000'000'000'000))).value_or(Fixed{});
            const Fixed max_rate = tax_rate_max_known_ ? world_.rules.tax_rate_max : Fixed::one();
            s.tax_rate = f.fixed("steuersatz", between(fx(0), max_rate)).value_or(Fixed{});
            s.court_expenses = f.fixed("hofhaltung", between(fx(0), fx(1'000'000'000'000))).value_or(Fixed{});
            s.source = f.text("quelle").value_or("");

            const auto index = static_cast<Index>(world_.states.size());
            if (register_id(states_, s.tag, index, file.name, node.line, "Staat")) {
                if (capital) {
                    const ScriptNode* capital_node = node.find("hauptstadt");
                    capitals_.push_back(CapitalRef{*capital, file.name, capital_node->line});
                } else {
                    capitals_.push_back(CapitalRef{});
                }
                world_.states.push_back(std::move(s));
            }
        });
    }

    void load_locations(const SourceFile& file) {
        for_each_definition(file, "ort", "ort <kennung> { … }", [&](const ScriptNode& node) {
            if (!is_location_id(node.value)) {
                issues_.error(file.name, node.line,
                              "ungültige Ortskennung " + quote(node.value) +
                                  " (nur Kleinbuchstaben a–z, Ziffern und '_')");
            }
            Fields f(node, file.name, "ort " + node.value, issues_,
                     {"name", "besitzer", "gelaende", "lage", "flaeche", "bevoelkerung", "stadt",
                      "konfession", "verwuestung", "quelle"});
            Location loc;
            LocationDraft draft;
            loc.id = node.value;
            loc.name = f.text("name").value_or("");
            if (const auto owner = f.text("besitzer")) {
                loc.owner = lookup(states_, *owner);
                if (loc.owner == kNone) {
                    issues_.error(file.name, node.find("besitzer")->line,
                                  "unbekannter Staat " + quote(*owner) + " als Besitzer von ort " +
                                      node.value);
                }
            }
            if (const auto terrain = f.text("gelaende")) {
                loc.terrain = lookup(terrains_, *terrain);
                if (loc.terrain == kNone) {
                    issues_.error(file.name, node.find("gelaende")->line,
                                  "unbekanntes Gelände " + quote(*terrain) + " in ort " + node.value);
                }
            }
            read_position(f, loc);
            loc.area_km2 = f.fixed("flaeche", above_up_to(fx(0), fx(1'000'000))).value_or(Fixed{});
            draft.population = f.integer("bevoelkerung", 1, 100'000'000).value_or(0);
            loc.is_city = f.boolean("stadt").value_or(false);
            read_confessions(f, draft);
            loc.devastation = f.fixed("verwuestung", between(fx(0), fx(1))).value_or(Fixed{});
            loc.source = f.text("quelle").value_or("");

            const auto index = static_cast<Index>(world_.locations.size());
            if (register_id(locations_, loc.id, index, file.name, node.line, "Ort")) {
                world_.locations.push_back(std::move(loc));
                drafts_.push_back(std::move(draft));
            }
        });
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

    void read_confessions(Fields& f, LocationDraft& draft) {
        const ScriptNode* node = f.block("konfession", "Prozentwerte je Konfession, z. B. { lutherisch = 90 "
                                                       "reformiert = 10 }");
        if (node == nullptr) {
            return;
        }
        std::int64_t sum = 0;
        bool ok = true;
        std::set<Index> seen;
        for (const ScriptNode& child : node->children) {
            if (child.key.empty() || child.is_block || child.op != "=") {
                f.issues().error(
                    f.file(), child.line,
                    "ungültiger Eintrag im Block 'konfession' (erwartet <konfession> = <prozent>)");
                ok = false;
                continue;
            }
            const Index confession = lookup(confessions_, child.key);
            if (confession == kNone) {
                f.issues().error(f.file(), child.line,
                                 "unbekannte Konfession " + quote(child.key) + " in " + f.what());
                ok = false;
                continue;
            }
            if (!seen.insert(confession).second) {
                f.issues().error(f.file(), child.line,
                                 "Konfession " + quote(child.key) + " ist in " + f.what() +
                                     " mehrfach angegeben");
                ok = false;
                continue;
            }
            const auto percent = parse_int(child.value);
            if (!percent || *percent < 0 || *percent > 100) {
                f.issues().error(f.file(), child.line,
                                 "ungültiger Prozentwert " + quote(child.value) + " für Konfession " +
                                     quote(child.key) + " (erwartet eine Ganzzahl von 0 bis 100)");
                ok = false;
                continue;
            }
            sum += *percent;
            draft.confessions.emplace_back(confession, *percent);
        }
        if (ok && sum != 100) {
            f.issues().error(f.file(), node->line,
                             "die Konfessionsanteile in " + f.what() + " ergeben " + std::to_string(sum) +
                                 " %, erwartet 100 %");
        }
    }

    // ── szenarien/ ──

    void load_scenarios(const SourceFile& file, std::vector<Scenario>& scenarios) {
        for_each_definition(file, "szenario", "szenario <kennung> { … }", [&](const ScriptNode& node) {
            Fields f(node, file.name, "szenario " + node.value, issues_, {"name", "datum", "spieler"});
            Scenario s;
            s.id = node.value;
            s.name = f.text("name").value_or("");
            s.start = f.date("datum").value_or(Date{});
            if (const auto player = f.text("spieler")) {
                s.player_tag = *player;
                if (lookup(states_, *player) == kNone) {
                    issues_.error(file.name, node.find("spieler")->line,
                                  "unbekannter Staat " + quote(*player) + " als Spieler in szenario " +
                                      node.value);
                }
            }
            const auto index = static_cast<Index>(scenarios.size());
            if (register_id(scenarios_, s.id, index, file.name, node.line, "Szenario")) {
                scenarios.push_back(std::move(s));
            }
        });
    }

    // ── Querprüfungen ──

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
            if (world_.locations[static_cast<std::size_t>(location)].owner != static_cast<Index>(i)) {
                issues_.error(ref.file, ref.line,
                              "Hauptstadt " + quote(ref.location_id) + " von " + state.tag +
                                  " gehört nicht zu diesem Staat");
                continue;
            }
            state.capital = location;
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

    void build_pops() {
        const Fixed hundred = Fixed::from_int(100);
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
                    PopBlock pop;
                    pop.location = static_cast<Index>(li);
                    pop.profession = static_cast<Index>(pi);
                    pop.confession = confession;
                    pop.size = Fixed::from_int(draft.population) * share * Fixed::from_int(percent) / hundred;
                    world_.pops.push_back(pop);
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
    Registry professions_;
    Registry states_;
    Registry locations_;
    Registry scenarios_;

    std::vector<CapitalRef> capitals_;  // parallel zu world_.states
    std::vector<LocationDraft> drafts_; // parallel zu world_.locations
};

} // namespace

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
