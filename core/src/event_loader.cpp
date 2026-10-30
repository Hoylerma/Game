#include "event_loader.hpp"

#include "uhrwerk/military.hpp"
#include "uhrwerk/script.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace uhrwerk::detail {

namespace {

namespace fs = std::filesystem;

std::string quote(std::string_view text) {
    return "'" + std::string(text) + "'";
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

bool is_lower_id(std::string_view id, bool allow_dot) {
    if (id.empty() || id[0] < 'a' || id[0] > 'z') {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [&](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || (allow_dot && c == '.');
    });
}

bool starts_with(std::string_view text, std::string_view prefix) {
    return text.size() > prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::optional<CompareOp> compare_op(std::string_view op) {
    if (op == "=") {
        return CompareOp::Eq;
    }
    if (op == "!=") {
        return CompareOp::Ne;
    }
    if (op == "<") {
        return CompareOp::Lt;
    }
    if (op == "<=") {
        return CompareOp::Le;
    }
    if (op == ">") {
        return CompareOp::Gt;
    }
    if (op == ">=") {
        return CompareOp::Ge;
    }
    return std::nullopt;
}

std::optional<Constitution> parse_constitution(std::string_view text) {
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

std::optional<Franchise> parse_franchise(std::string_view text) {
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

Fixed fx(std::int64_t v) {
    return Fixed::from_int(v);
}

struct Source {
    std::string name;
    ScriptNode root;
};

/// Ereignis nach dem ersten Durchgang: Kennung, Ort und was Verweise darauf brauchen.
struct EventEntry {
    std::string id;
    const Source* source = nullptr;
    const ScriptNode* node = nullptr;
    EventScope scope = EventScope::State;
    std::vector<std::string> options;
};

/// Folgeverweis für die Kreisprüfung (tage = 0).
struct FollowEdge {
    Index from = kNone;
    Index to = kNone;
    int line = 0;
};

class EventLoader {
public:
    EventLoader(fs::path data_dir, Date start, const AbsentContent& absent, World& world,
                std::vector<LoadIssue>& issues)
        : data_dir_(std::move(data_dir)), start_(start), absent_(absent), world_(world), issues_(issues),
          first_issue_(issues.size()) {}

    void run() {
        const std::string rules_name = "common/ereignisse.chron";
        std::error_code ec;
        if (!fs::is_regular_file(data_dir_ / utf8_to_path(rules_name), ec)) {
            return;
        }
        load_rules(read(rules_name));
        if (!world_.event_rules.active) {
            return;
        }
        // Zustand der Ereignisse: Zustimmung je Staat und Gruppe und Unruhe je Staat beginnen bei 0
        // (Abschnitte 4.2 und 4.3).
        world_.approval.assign(world_.states.size() * world_.interest_groups.size(), Fixed::zero());
        world_.unrest.assign(world_.states.size(), Fixed::zero());
        for (const std::string& name : list_folder("common/ereignisse")) {
            sources_.push_back(read(name));
        }
        // Journale und Abstimmungen (seit a4, Abschnitt 5): Kennungen und Stufen vor den Ereignissen, die
        // auf sie verweisen; ihr Inhalt nach den Ereignissen, auf die er verweist.
        for (const std::string& name : list_folder("common/journale")) {
            journal_sources_.push_back(read(name));
        }
        if (fs::is_regular_file(data_dir_ / utf8_to_path("common/abstimmungen.chron"), ec)) {
            journal_sources_.push_back(read("common/abstimmungen.chron"));
        }
        for (const Source& source : journal_sources_) {
            register_journals(source);
        }
        // Erster Durchgang: Kennungen, Optionen und Geltungsbereich aller Ereignisse; dann nach Kennung
        // geordnet (Reihenfolge der täglichen Prüfung, Abschnitt 3.4).
        for (const Source& source : sources_) {
            register_events(source);
        }
        std::sort(entries_.begin(), entries_.end(),
                  [](const EventEntry& a, const EventEntry& b) { return a.id < b.id; });
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            index_.emplace(entries_[i].id, static_cast<Index>(i));
        }
        resolve_outcome_events();
        world_.events.resize(entries_.size());
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            current_ = static_cast<Index>(i);
            file_ = entries_[i].source->name;
            load_event(entries_[i], world_.events[i]);
        }
        current_ = kNone;
        load_journals_and_votes();
        check_follow_cycles();
        load_ai(); // seit M12 (ki.md Abschnitt 3), nur mit Ereignissen
        // Meldungen nach Datei und Zeile wie im übrigen Lader (gelesen wird nach Kennung).
        std::stable_sort(issues_.begin() + static_cast<std::ptrdiff_t>(first_issue_), issues_.end(),
                         [](const LoadIssue& a, const LoadIssue& b) {
                             return std::tie(a.file, a.line) < std::tie(b.file, b.line);
                         });
    }

private:
    // ── Meldungen und Dateien ──

    void error(int line, std::string message) { error(file_, line, std::move(message)); }
    void error(const std::string& file, int line, std::string message) {
        issues_.push_back(LoadIssue{file, line, std::move(message), true});
    }
    void warning(int line, std::string message) {
        issues_.push_back(LoadIssue{file_, line, std::move(message), false});
    }

    Source read(const std::string& relative) {
        ParseResult parsed = parse_script_file(data_dir_ / utf8_to_path(relative));
        for (const ParseError& e : parsed.errors) {
            std::string message = e.message;
            if (e.column > 0) {
                message += " (Spalte " + std::to_string(e.column) + ")";
            }
            error(relative, e.line, std::move(message));
        }
        return Source{relative, std::move(parsed.root)};
    }

    /// Alle *.chron-Dateien des Ordners, nach Dateiname sortiert; ein fehlender Ordner ist kein Fehler.
    std::vector<std::string> list_folder(const std::string& folder) {
        const fs::path dir = data_dir_ / utf8_to_path(folder);
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) {
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
            error(folder, 0, "Ordner kann nicht gelesen werden: " + ec.message());
        }
        std::sort(names.begin(), names.end());
        for (std::string& name : names) {
            name = folder + "/" + name;
        }
        return names;
    }

    bool absent(std::string_view kind, std::string_view id) const {
        return absent_.count(std::string(kind) + ":" + std::string(id)) != 0;
    }

    // ── Werte ──

    bool scalar(const ScriptNode& node) {
        if (node.is_block) {
            error(node.line, quote(node.key) + " erwartet einen einzelnen Wert, keinen Block");
            return false;
        }
        return true;
    }

    bool assign(const ScriptNode& node) {
        if (node.op != "=") {
            error(node.line, quote(node.key) + " erwartet '=' statt " + quote(node.op));
            return false;
        }
        return true;
    }

    std::optional<Fixed> number(const ScriptNode& node, Fixed lo, Fixed hi) {
        if (!scalar(node)) {
            return std::nullopt;
        }
        const auto value = Fixed::parse(node.value);
        if (!value) {
            error(node.line, "ungültiger Wert " + quote(node.value) + " für " + quote(node.key) +
                                 " (erwartet eine Zahl wie 12 oder 0.25)");
            return std::nullopt;
        }
        if (*value < lo || *value > hi) {
            error(node.line, quote(node.key) + " muss zwischen " + lo.to_string(0) + " und " +
                                 hi.to_string(0) + " liegen (ist " + node.value + ")");
            return std::nullopt;
        }
        return value;
    }

    std::optional<std::int64_t> integer(const ScriptNode& node, std::int64_t lo, std::int64_t hi) {
        if (!scalar(node) || !assign(node)) {
            return std::nullopt;
        }
        const auto value = parse_int(node.value);
        if (!value) {
            error(node.line, "ungültiger Wert " + quote(node.value) + " für " + quote(node.key) +
                                 " (erwartet eine Ganzzahl)");
            return std::nullopt;
        }
        if (*value < lo || *value > hi) {
            error(node.line, quote(node.key) + " muss zwischen " + std::to_string(lo) + " und " +
                                 std::to_string(hi) + " liegen (ist " + node.value + ")");
            return std::nullopt;
        }
        return value;
    }

    std::optional<bool> boolean(const ScriptNode& node) {
        if (!scalar(node) || !assign(node)) {
            return std::nullopt;
        }
        if (node.value == "ja") {
            return true;
        }
        if (node.value == "nein") {
            return false;
        }
        error(node.line, "ungültiger Wert " + quote(node.value) + " für " + quote(node.key) +
                             " (erwartet ja oder nein)");
        return std::nullopt;
    }

    std::optional<Date> date(const ScriptNode& node) {
        if (!scalar(node)) {
            return std::nullopt;
        }
        const auto value = parse_date(node.value);
        if (!value) {
            error(node.line, "ungültiges Datum " + quote(node.value) + " für " + quote(node.key) +
                                 " (erwartet JJJJ.MM.TT, z. B. 1848.03.18)");
        }
        return value;
    }

    std::optional<std::string> name(const ScriptNode& node) {
        if (!scalar(node) || !assign(node)) {
            return std::nullopt;
        }
        if (!is_lower_id(node.value, false)) {
            error(node.line, "ungültiger Name " + quote(node.value) + " für " + quote(node.key) +
                                 " (erlaubt: a-z, 0-9 und _, beginnend mit einem Buchstaben)");
            return std::nullopt;
        }
        return node.value;
    }

    /// Text: eine Zeichenkette oder ein Block aus Zeichenketten, mit Leerzeichen verbunden; "" beginnt
    /// einen neuen Absatz (Abschnitt 3.8).
    std::optional<std::string> text(const ScriptNode& node) {
        if (!node.is_block) {
            if (!assign(node)) {
                return std::nullopt;
            }
            if (node.value.empty()) {
                error(node.line, quote(node.key) + " darf nicht leer sein");
                return std::nullopt;
            }
            return node.value;
        }
        std::string result;
        bool paragraph = false;
        for (const ScriptNode& line : node.children) {
            if (!line.key.empty() || line.is_block) {
                error(line.line, "in " + quote(node.key) + " sind nur Zeichenketten erlaubt");
                return std::nullopt;
            }
            if (line.value.empty()) {
                paragraph = !result.empty();
                continue;
            }
            if (!result.empty()) {
                result += paragraph ? "\n\n" : " ";
            }
            paragraph = false;
            result += line.value;
        }
        if (result.empty()) {
            error(node.line, quote(node.key) + " darf nicht leer sein");
            return std::nullopt;
        }
        return result;
    }

    /// Doppelte Felder eines Blocks melden; true, wenn der Schlüssel zum ersten Mal vorkommt.
    bool first_time(std::map<std::string, int, std::less<>>& seen, const ScriptNode& node,
                    const std::string& what) {
        const auto [it, inserted] = seen.emplace(node.key, node.line);
        if (!inserted) {
            error(node.line, "Feld " + quote(node.key) + " in " + what +
                                 " ist mehrfach angegeben (zuerst in Zeile " + std::to_string(it->second) +
                                 ")");
        }
        return inserted;
    }

    // ── KI der Staaten (seit M12, ki.md Abschnitt 3; chronik-skript.md 3.37–3.39) ──

    void load_ai() {
        const std::string rules_name = "common/ki.chron";
        std::error_code ec;
        if (!fs::is_regular_file(data_dir_ / utf8_to_path(rules_name), ec)) {
            return;
        }
        const Source rules = read(rules_name);
        load_ai_rules(rules);
        if (!world_.ai_rules.active) {
            world_.ai_traits.clear();
            world_.ai_personalities.clear();
            return;
        }
        world_.agendas.assign(world_.states.size(), AgendaDef{});
        if (fs::is_regular_file(data_dir_ / utf8_to_path("common/persoenlichkeiten.chron"), ec)) {
            load_persons(read("common/persoenlichkeiten.chron"));
        }
        if (fs::is_regular_file(data_dir_ / utf8_to_path("common/agenden.chron"), ec)) {
            load_agendas(read("common/agenden.chron"));
        }
    }

    /// Neigungen aus den Feldern risiko … vermittlung (je −2 … +2); false bei einem anderen Feld.
    bool inclination(const ScriptNode& node, Inclinations& out) {
        for (std::size_t k = 0; k < kInclinationCount; ++k) {
            if (node.key == kInclinationKeys[k]) {
                if (assign(node)) {
                    if (const auto v = number(node, fx(-2), fx(2))) {
                        out[k] = *v;
                    }
                }
                return true;
            }
        }
        return false;
    }

    void load_ai_rules(const Source& source) {
        file_ = source.name;
        AiRules& r = world_.ai_rules;
        bool found = false;
        std::optional<Date> since;
        std::string default_personality;
        int default_line = 0;
        std::map<std::string, int, std::less<>> traits_seen;
        std::map<std::string, int, std::less<>> personalities_seen;
        for (const ScriptNode& node : source.root.children) {
            if ((node.key == "eigenschaft" || node.key == "persoenlichkeit") && node.is_block) {
                if (!is_lower_id(node.value, false)) {
                    error(node.line,
                          quote(node.key) + " erwartet eine Kennung: " + node.key + " <kennung> { … }");
                    continue;
                }
                auto& seen = node.key == "eigenschaft" ? traits_seen : personalities_seen;
                const auto [it, inserted] = seen.emplace(node.value, node.line);
                if (!inserted) {
                    error(node.line, "doppelte Kennung " + quote(node.value) + " (zuerst in Zeile " +
                                         std::to_string(it->second) + ")");
                    continue;
                }
                std::string label;
                Inclinations values{};
                std::map<std::string, int, std::less<>> fields;
                for (const ScriptNode& child : node.children) {
                    if (!first_time(fields, child, node.value)) {
                        continue;
                    }
                    if (child.key == "name") {
                        if (const auto t = text(child)) {
                            label = *t;
                        }
                    } else if (!inclination(child, values)) {
                        warning(child.line, "unbekanntes Feld " + quote(child.key) + " in " + node.key + " " +
                                                node.value + " wird ignoriert");
                    }
                }
                if (label.empty()) {
                    error(node.line, "Pflichtfeld 'name' fehlt in " + node.key + " " + node.value);
                }
                if (node.key == "eigenschaft") {
                    world_.ai_traits.push_back(TraitDef{node.value, label, values});
                } else {
                    world_.ai_personalities.push_back(PersonalityDef{node.value, label, values});
                }
                continue;
            }
            if (node.key != "ki" || !node.is_block || !node.value.empty()) {
                warning(node.line, "unbekannter Eintrag " + quote(node.key.empty() ? node.value : node.key) +
                                       " wird ignoriert (erwartet: ki { … }, eigenschaft, persoenlichkeit)");
                continue;
            }
            if (found) {
                error(node.line, "Block 'ki' ist mehrfach angegeben");
                continue;
            }
            found = true;
            std::map<std::string, int, std::less<>> seen;
            for (const ScriptNode& child : node.children) {
                if (!first_time(seen, child, "ki")) {
                    continue;
                }
                if (child.key == "seit") {
                    if (assign(child)) {
                        since = date(child);
                    }
                } else if (child.key == "rauschen") {
                    if (assign(child)) {
                        if (const auto v = number(child, fx(0), fx(100))) {
                            r.noise = *v;
                        }
                    }
                } else if (child.key == "abkuehlung") {
                    if (const auto v = integer(child, 0, 120)) {
                        r.cooldown_months = static_cast<std::int32_t>(*v);
                    }
                } else if (child.key == "schulden_grenze") {
                    if (assign(child)) {
                        if (const auto v = number(child, fx(0), fx(100))) {
                            r.debt_limit = *v;
                        }
                    }
                } else if (child.key == "agenda_offen") {
                    if (const auto v = boolean(child)) {
                        r.open_agenda = *v;
                    }
                } else if (child.key == "gewicht_herrscher_absolut" ||
                           child.key == "gewicht_herrscher_konstitutionell") {
                    if (assign(child)) {
                        if (const auto v = number(child, fx(0), fx(1))) {
                            (child.key == "gewicht_herrscher_absolut" ? r.ruler_weight_absolute
                                                                      : r.ruler_weight_constitutional) = *v;
                        }
                    }
                } else if (read_ai_rule(child)) {
                    continue;
                } else if (child.key == "standard_persoenlichkeit") {
                    if (const auto id = name(child)) {
                        default_personality = *id;
                        default_line = child.line;
                    }
                } else {
                    warning(child.line, "unbekanntes Feld " + quote(child.key) + " in ki wird ignoriert");
                }
            }
            if (seen.count("seit") == 0) {
                error(node.line, "Pflichtfeld 'seit' fehlt in ki");
            }
        }
        if (!found) {
            error(0, "Block 'ki { seit = … }' fehlt");
        }
        if (!default_personality.empty()) {
            r.default_personality = world_.find_personality(default_personality);
            if (r.default_personality == kNone) {
                error(default_line, "unbekannte Persönlichkeit " + quote(default_personality));
            }
        }
        r.active = since && *since <= start_;
    }

    /// Spielwerte der Planer (seit a2, Abschnitte 5 und 6); false bei einem anderen Feld.
    bool read_ai_rule(const ScriptNode& child) {
        AiRules& r = world_.ai_rules;
        struct Value {
            std::string_view key;
            Fixed* target;
            Fixed lo;
            Fixed hi;
        };
        const Value values[] = {
            {"schwelle", &r.threshold, fx(0), fx(1000)},
            {"ueberschuss_ab", &r.surplus_from, fx(0), fx(1)},
            {"unruhe_steuern_ab", &r.unrest_tax_from, fx(0), fx(100)},
            {"bedrohung_mobil", &r.threat_mobilize, fx(0), fx(100)},
        };
        for (const Value& v : values) {
            if (child.key == v.key) {
                if (assign(child)) {
                    if (const auto value = number(child, v.lo, v.hi)) {
                        *v.target = *value;
                    }
                }
                return true;
            }
        }
        struct Count {
            std::string_view key;
            std::int32_t* target;
            std::int64_t lo;
            std::int64_t hi;
        };
        const Count counts[] = {
            {"protokoll", &r.log_size, 1, 1000},
            {"abkuehlung_steuern", &r.tax_cooldown_months, 0, 120},
            {"abruestung_monate", &r.quiet_months, 1, 120},
        };
        for (const Count& c : counts) {
            if (child.key == c.key) {
                if (const auto value = integer(child, c.lo, c.hi)) {
                    *c.target = static_cast<std::int32_t>(*value);
                }
                return true;
            }
        }
        return false;
    }

    /// Eigenschaften aus einer Liste { kennung kennung … }.
    std::vector<Index> trait_list(const ScriptNode& node) {
        std::vector<Index> result;
        if (!node.is_block || !assign(node)) {
            error(node.line, "'eigenschaften' erwartet eine Liste: eigenschaften = { kennung … }");
            return result;
        }
        for (const ScriptNode& entry : node.children) {
            if (!entry.key.empty() || entry.is_block) {
                error(entry.line, "in 'eigenschaften' sind nur Kennungen erlaubt");
                continue;
            }
            const Index trait = world_.find_trait(entry.value);
            if (trait == kNone) {
                error(entry.line, "unbekannte Eigenschaft " + quote(entry.value) + " (common/ki.chron)");
            } else if (std::find(result.begin(), result.end(), trait) != result.end()) {
                error(entry.line, "Eigenschaft " + quote(entry.value) + " ist doppelt angegeben");
            } else {
                result.push_back(trait);
            }
        }
        return result;
    }

    void load_persons(const Source& source) {
        file_ = source.name;
        std::map<std::string, int, std::less<>> ids;
        for (const ScriptNode& node : source.root.children) {
            if (node.key != "person" || !node.is_block || !is_lower_id(node.value, false)) {
                error(node.line, "erwartet: person <kennung> { … }");
                continue;
            }
            const auto [it, inserted] = ids.emplace(node.value, node.line);
            if (!inserted) {
                error(node.line, "doppelte Kennung " + quote(node.value) + " (zuerst in Zeile " +
                                     std::to_string(it->second) + ")");
                continue;
            }
            PersonDef p;
            p.id = node.value;
            bool ok = true;
            bool skip = false;
            const ScriptNode* condition = nullptr;
            std::map<std::string, int, std::less<>> seen;
            for (const ScriptNode& child : node.children) {
                if (!first_time(seen, child, "person " + p.id)) {
                    continue;
                }
                if (child.key == "name" || child.key == "amt" || child.key == "quelle") {
                    if (const auto t = text(child)) {
                        (child.key == "name" ? p.name : child.key == "amt" ? p.title : p.source) = *t;
                    }
                } else if (child.key == "staat") {
                    if (!scalar(child) || !assign(child)) {
                        ok = false;
                        continue;
                    }
                    bool is_absent = false;
                    const auto state = state_ref(child, child.value, is_absent);
                    if (!state) {
                        ok = false;
                    }
                    skip = skip || is_absent;
                    p.state = state.value_or(kNone);
                } else if (child.key == "rolle") {
                    if (!scalar(child) || !assign(child)) {
                        ok = false;
                    } else if (child.value == "herrscher" || child.value == "minister") {
                        p.role = child.value == "herrscher" ? PersonRole::Ruler : PersonRole::Minister;
                    } else {
                        error(child.line, "ungültige Rolle " + quote(child.value) +
                                              " (erwartet herrscher oder minister)");
                        ok = false;
                    }
                } else if (child.key == "von" || child.key == "bis") {
                    const auto d = assign(child) ? date(child) : std::nullopt;
                    if (!d) {
                        ok = false;
                    } else if (child.key == "von") {
                        p.from = *d;
                    } else {
                        p.has_to = true;
                        p.to = *d;
                    }
                } else if (child.key == "bedingung") {
                    condition = &child;
                } else if (child.key == "werte") {
                    if (!child.is_block || !assign(child)) {
                        error(child.line, "'werte' erwartet einen Block: werte = { staatskunst = 5 … }");
                        ok = false;
                        continue;
                    }
                    for (const ScriptNode& v : child.children) {
                        const auto k = std::find(kPersonValueKeys.begin(), kPersonValueKeys.end(), v.key);
                        if (k == kPersonValueKeys.end()) {
                            error(v.line, "unbekannter Wert " + quote(v.key) +
                                              " (erwartet staatskunst, diplomatie, kriegskunst, wirkung)");
                            continue;
                        }
                        if (const auto value = integer(v, 1, 10)) {
                            p.values[static_cast<std::size_t>(k - kPersonValueKeys.begin())] =
                                static_cast<std::int32_t>(*value);
                        }
                    }
                } else if (child.key == "eigenschaften") {
                    p.traits = trait_list(child);
                } else if (child.key == "sicherheit") {
                    continue;
                } else {
                    warning(child.line, "unbekanntes Feld " + quote(child.key) + " in person " + p.id +
                                            " wird ignoriert");
                }
            }
            for (const std::string_view field : {"name", "staat", "rolle", "von", "quelle"}) {
                if (seen.count(field) == 0) {
                    error(node.line, "Pflichtfeld " + quote(field) + " fehlt in person " + p.id);
                    ok = false;
                }
            }
            if (ok && p.has_to && p.to < p.from) {
                error(node.line, "'bis' liegt vor 'von' in person " + p.id);
                ok = false;
            }
            if (condition != nullptr) {
                p.has_condition = true;
                p.condition = condition_block(*condition, true);
            }
            if (ok && !skip) {
                world_.persons.push_back(std::move(p));
            }
        }
        std::sort(world_.persons.begin(), world_.persons.end(),
                  [](const PersonDef& a, const PersonDef& b) { return a.id < b.id; });
    }

    void load_agendas(const Source& source) {
        file_ = source.name;
        std::map<std::string, int, std::less<>> tags;
        for (const ScriptNode& node : source.root.children) {
            if (node.key != "agenda" || !node.is_block || node.value.empty()) {
                error(node.line, "erwartet: agenda <TAG> { … }");
                continue;
            }
            const auto [it, inserted] = tags.emplace(node.value, node.line);
            if (!inserted) {
                error(node.line, "Agenda von " + quote(node.value) + " ist doppelt (zuerst in Zeile " +
                                     std::to_string(it->second) + ")");
                continue;
            }
            bool is_absent = false;
            const auto state = state_ref(node, node.value, is_absent);
            if (!state || is_absent) {
                continue;
            }
            AgendaDef agenda;
            std::map<std::string, int, std::less<>> seen;
            for (const ScriptNode& child : node.children) {
                if (child.key == "persoenlichkeit") {
                    if (!first_time(seen, child, "agenda " + node.value)) {
                        continue;
                    }
                    if (const auto id = name(child)) {
                        agenda.personality = world_.find_personality(*id);
                        if (agenda.personality == kNone) {
                            error(child.line,
                                  "unbekannte Persönlichkeit " + quote(*id) + " (common/ki.chron)");
                        }
                    }
                } else if (child.key == "ziel" && child.is_block) {
                    if (auto goal = agenda_goal(child)) {
                        agenda.goals.push_back(std::move(*goal));
                    }
                } else {
                    warning(child.line, "unbekanntes Feld " + quote(child.key) + " in agenda " + node.value +
                                            " wird ignoriert");
                }
            }
            world_.agendas[static_cast<std::size_t>(*state)] = std::move(agenda);
        }
    }

    /// Ziel einer Agenda; std::nullopt bei Fehlern oder wenn sein Ziel zum Start nicht besteht.
    std::optional<AgendaGoal> agenda_goal(const ScriptNode& node) {
        AgendaGoal goal;
        bool ok = true;
        bool skip = false;
        bool has_kind = false;
        bool has_weight = false;
        const ScriptNode* target = nullptr;
        std::map<std::string, int, std::less<>> seen;
        for (const ScriptNode& child : node.children) {
            if (!first_time(seen, child, "ziel")) {
                continue;
            }
            if (child.key == "art") {
                const auto k = std::find(kAgendaKindKeys.begin(), kAgendaKindKeys.end(), child.value);
                if (!scalar(child) || !assign(child) || k == kAgendaKindKeys.end()) {
                    error(child.line, "ungültige Art " + quote(child.value) +
                                          " (erwartet fuehrung_bund, zollunion, buendnis, gebiet, annexion, "
                                          "schwaechen, status_quo oder ruhe)");
                    ok = false;
                    continue;
                }
                goal.kind = static_cast<AgendaKind>(k - kAgendaKindKeys.begin());
                has_kind = true;
            } else if (child.key == "gewicht") {
                if (assign(child)) {
                    if (const auto v = number(child, fx(0), fx(100))) {
                        goal.weight = *v;
                        has_weight = true;
                    }
                }
            } else if (child.key == "ab" || child.key == "bis") {
                const auto d = assign(child) ? date(child) : std::nullopt;
                if (!d) {
                    ok = false;
                } else if (child.key == "ab") {
                    goal.has_from = true;
                    goal.from = *d;
                } else {
                    goal.has_to = true;
                    goal.to = *d;
                }
            } else if (child.key == "bedingung") {
                goal.has_condition = true;
                goal.condition = condition_block(child, true);
            } else if (child.key == "gegner" || child.key == "partner" || child.key == "union" ||
                       child.key == "gebiet") {
                if (target != nullptr) {
                    error(child.line,
                          "ein Ziel hat höchstens einen Gegenstand (gegner, partner, union, gebiet)");
                    ok = false;
                }
                target = &child;
            } else {
                warning(child.line, "unbekanntes Feld " + quote(child.key) + " in ziel wird ignoriert");
            }
        }
        if (!has_kind || !has_weight) {
            error(node.line,
                  std::string("Pflichtfeld ") + (has_kind ? "'gewicht'" : "'art'") + " fehlt in ziel");
            return std::nullopt;
        }
        // Gegenstand je Art (Abschnitt 3.4).
        std::string_view expected;
        switch (goal.kind) {
        case AgendaKind::CustomsUnion:
            expected = "union";
            break;
        case AgendaKind::Alliance:
            expected = "partner";
            break;
        case AgendaKind::Territory:
            expected = "gebiet";
            break;
        case AgendaKind::Annexation:
        case AgendaKind::Weaken:
            expected = "gegner";
            break;
        case AgendaKind::LeadFederation:
        case AgendaKind::StatusQuo:
        case AgendaKind::Calm:
            break;
        }
        if (expected.empty()) {
            if (target != nullptr) {
                error(target->line, "Art " +
                                        std::string(kAgendaKindKeys[static_cast<std::size_t>(goal.kind)]) +
                                        " hat keinen Gegenstand " + quote(target->key));
                return std::nullopt;
            }
        } else if (target == nullptr || target->key != expected) {
            error(node.line, "Art " + std::string(kAgendaKindKeys[static_cast<std::size_t>(goal.kind)]) +
                                 " braucht " + quote(expected));
            return std::nullopt;
        } else if (!scalar(*target) || !assign(*target)) {
            return std::nullopt;
        } else if (expected == "union") {
            goal.target = world_.find_customs_union(target->value);
            if (goal.target == kNone) {
                if (!absent("zollunion", target->value)) {
                    error(target->line, "unbekannte Zollunion " + quote(target->value));
                    ok = false;
                }
                skip = true;
            }
        } else if (expected == "gebiet") {
            goal.target = world_.find_admin_unit(target->value);
            if (goal.target == kNone) {
                error(target->line, "unbekannte Verwaltungseinheit " + quote(target->value));
                ok = false;
            }
        } else {
            bool is_absent = false;
            const auto state = state_ref(*target, target->value, is_absent);
            if (!state) {
                ok = false;
            }
            skip = skip || is_absent;
            goal.target = state.value_or(kNone);
        }
        if (ok && goal.has_from && goal.has_to && goal.to < goal.from) {
            error(node.line, "'bis' liegt vor 'ab' in ziel");
            ok = false;
        }
        if (!ok || skip) {
            return std::nullopt;
        }
        return goal;
    }

    // ── common/ereignisse.chron ──

    void load_rules(const Source& source) {
        file_ = source.name;
        rules_file_ = source.name;
        bool found = false;
        for (const ScriptNode& node : source.root.children) {
            if (node.key != "ereignisse" || !node.is_block || !node.value.empty()) {
                warning(node.line, "unbekannter Eintrag " + quote(node.key.empty() ? node.value : node.key) +
                                       " wird ignoriert (erwartet: ereignisse { seit = … frist = … })");
                continue;
            }
            if (found) {
                error(node.line, "Block 'ereignisse' ist mehrfach angegeben");
                continue;
            }
            found = true;
            std::map<std::string, int, std::less<>> seen;
            std::optional<Date> since;
            for (const ScriptNode& child : node.children) {
                if (!first_time(seen, child, "ereignisse")) {
                    continue;
                }
                if (child.key == "seit") {
                    if (assign(child)) {
                        since = date(child);
                    }
                } else if (child.key == "frist") {
                    if (const auto days = integer(child, 1, 365)) {
                        world_.event_rules.deadline = static_cast<std::int32_t>(*days);
                    }
                } else if (!read_rule(child)) {
                    warning(child.line,
                            "unbekanntes Feld " + quote(child.key) + " in ereignisse wird ignoriert");
                }
            }
            if (seen.count("seit") == 0) {
                error(node.line, "Pflichtfeld 'seit' fehlt in ereignisse");
            }
            world_.event_rules.active = since && *since <= start_;
        }
        if (!found) {
            error(0, "Block 'ereignisse { seit = … }' fehlt");
        }
    }

    /// Spielwerte von Zustimmung, Unruhe und Aufständen (seit a3, Abschnitte 4.2–4.4); false bei
    /// unbekanntem Feld.
    bool read_rule(const ScriptNode& child) {
        EventRules& r = world_.event_rules;
        struct Value {
            std::string_view key;
            Fixed* target;
            Fixed lo;
            Fixed hi;
        };
        const Value values[] = {
            {"zustimmung_annaeherung", &r.approval_rate, fx(0), fx(1)},
            {"unruhe_annaeherung", &r.unrest_rate, fx(0), fx(1)},
            {"unruhe_not", &r.misery, fx(0), fx(100)},
            {"not_ab", &r.misery_from, fx(0), fx(100)},
            {"not_spanne", &r.misery_span, Fixed::from_ratio(1, 100), fx(100)},
            {"unruhe_teuerung", &r.dearth, fx(0), fx(100)},
            {"teuerung_ab", &r.dearth_from, fx(0), fx(10)},
            {"teuerung_spanne", &r.dearth_span, Fixed::from_ratio(1, 100), fx(10)},
            {"unruhe_ansteckung", &r.contagion, fx(0), fx(100)},
            {"ansteckung_ab", &r.contagion_from, fx(0), fx(100)},
            {"unruhe_zustimmung", &r.approval_weight, fx(0), fx(10)},
            {"steuerverweigerung_ab", &r.tax_refusal_from, fx(0), fx(100)},
            {"steuerverweigerung", &r.tax_refusal, fx(0), fx(1)},
            {"auswanderung_ab", &r.emigration_from, fx(0), fx(100)},
            {"auswanderung_unruhe", &r.emigration, fx(0), fx(10)},
            {"aufstand_schritt", &r.revolt_step, fx(0), fx(10)},
            {"aufstand_zufall", &r.revolt_noise, fx(0), fx(10)},
            {"aufstand_nachgeben", &r.revolt_concession, fx(0), fx(1)},
            {"spaltung", &r.split, fx(0), fx(10)},
            {"soldaten_bezug", &r.soldiers_reference, Fixed::from_ratio(1, 100), fx(1000)},
            {"nachgeben_unruhe", &r.concession_unrest, fx(-100), fx(100)},
            {"niederschlagung_unruhe", &r.suppression_unrest, fx(-100), fx(100)},
            {"niederschlagung_verwuestung", &r.suppression_devastation, fx(0), fx(1)},
        };
        for (const Value& v : values) {
            if (child.key == v.key) {
                if (assign(child)) {
                    if (const auto value = number(child, v.lo, v.hi)) {
                        *v.target = *value;
                    }
                }
                return true;
            }
        }
        if (child.key == "aufstand_tage") {
            // aufstand_tage = { 7 60 }: erste Entscheidung und Höchstdauer
            const std::vector<std::string_view> days =
                child.is_block ? child.bare_values() : std::vector<std::string_view>{};
            const auto first = days.size() == 2 ? parse_int(days[0]) : std::nullopt;
            const auto last = days.size() == 2 ? parse_int(days[1]) : std::nullopt;
            if (!first || !last || *first < 1 || *last < *first || *last > 3650) {
                error(child.line,
                      "'aufstand_tage' erwartet zwei Ganzzahlen { erste_entscheidung hoechstens }, "
                      "1 ≤ erste ≤ höchstens ≤ 3650");
            } else {
                r.revolt_min_days = static_cast<std::int32_t>(*first);
                r.revolt_max_days = static_cast<std::int32_t>(*last);
            }
            return true;
        }
        if (child.key == "unruhe_verfassung") {
            // unruhe_verfassung = { keine = 15 staendisch = 10 konstitutionell = 0 }
            if (!child.is_block) {
                error(child.line, "'unruhe_verfassung' erwartet einen Block { keine = n staendisch = n "
                                  "konstitutionell = n }");
                return true;
            }
            for (const ScriptNode& entry : child.children) {
                const auto which = parse_constitution(entry.key);
                if (!which) {
                    error(entry.line, "ungültige Verfassung " + quote(entry.key) +
                                          " (erlaubt: keine, staendisch, konstitutionell)");
                    continue;
                }
                if (const auto value = number(entry, fx(-100), fx(100))) {
                    r.constitution[static_cast<std::size_t>(*which)] = *value;
                }
            }
            return true;
        }
        if (child.key == "getreide") {
            if (scalar(child) && assign(child)) {
                r.grain = world_.find_good(child.value);
                if (r.grain == kNone && !absent("gut", child.value)) {
                    error(child.line, "unbekanntes Gut " + quote(child.value) + " für 'getreide'");
                }
            }
            return true;
        }
        if (child.key == "ansteckung_merkmal" || child.key == "spaltung_merkmal" ||
            child.key == "sieg_merkmal") {
            if (const auto flag = name(child)) {
                (child.key == "ansteckung_merkmal" ? r.contagion_flag
                 : child.key == "spaltung_merkmal" ? r.split_flag
                                                   : r.victory_flag) = *flag;
            }
            return true;
        }
        if (child.key == "loyalitaet") {
            if (scalar(child) && assign(child)) {
                r.loyalty_group = group_ref(child, child.value);
            }
            return true;
        }
        if (child.key == "spaltung_gruppen") {
            if (!child.is_block) {
                error(child.line, "'spaltung_gruppen' erwartet eine Liste { gruppe gruppe … }");
                return true;
            }
            for (const std::string_view id : child.bare_values()) {
                if (const Index g = group_ref(child, std::string(id)); g != kNone) {
                    r.split_groups.push_back(g);
                }
            }
            return true;
        }
        if (child.key == "folge_nachgeben" || child.key == "folge_niederschlagung" ||
            child.key == "folge_sieg") {
            if (scalar(child) && assign(child)) {
                const std::size_t outcome = child.key == "folge_nachgeben"         ? 0
                                            : child.key == "folge_niederschlagung" ? 1
                                                                                   : 2;
                outcome_names_[outcome] = {child.value, child.line};
            }
            return true;
        }
        return false;
    }

    /// Interessengruppe der Regeln: kNone ohne Fehler, wenn es sie erst später gibt.
    Index group_ref(const ScriptNode& node, const std::string& id) {
        const Index g = world_.find_interest_group(id);
        if (g == kNone && !absent("interessengruppe", id)) {
            error(node.line, "unbekannte Interessengruppe " + quote(id) + " in " + quote(node.key));
        }
        return g;
    }

    /// Folgeereignisse der Ausgänge eines Aufstands, nach dem ersten Durchgang.
    void resolve_outcome_events() {
        for (std::size_t i = 0; i < outcome_names_.size(); ++i) {
            const auto& [id, line] = outcome_names_[i];
            if (id.empty()) {
                continue;
            }
            const Index event = event_index(id);
            if (event == kNone) {
                error(rules_file_, line,
                      "unbekanntes Ereignis " + quote(id) + " für den Ausgang eines Aufstands");
                continue;
            }
            if (entries_[static_cast<std::size_t>(event)].scope != EventScope::State) {
                error(rules_file_, line,
                      "Ereignis " + id + " für den Ausgang eines Aufstands muss wer = staat haben");
                continue;
            }
            world_.event_rules.outcome_events[i] = event;
        }
    }

    // ── Erster Durchgang ──

    void register_events(const Source& source) {
        file_ = source.name;
        for (const ScriptNode& node : source.root.children) {
            if (node.key != "ereignis") {
                warning(node.line, "unbekannter Eintrag " + quote(node.key.empty() ? node.value : node.key) +
                                       " wird ignoriert (erwartet: ereignis <kennung> { … })");
                continue;
            }
            if (!node.is_block || node.value.empty()) {
                error(node.line, "ungültige Definition, erwartet: ereignis <kennung> { … }");
                continue;
            }
            if (!is_lower_id(node.value, true)) {
                error(node.line,
                      "ungültige Kennung " + quote(node.value) +
                          " für Ereignis (erlaubt: a-z, 0-9, _ und ., beginnend mit einem Buchstaben)");
                continue;
            }
            const auto twin = std::find_if(entries_.begin(), entries_.end(),
                                           [&](const EventEntry& e) { return e.id == node.value; });
            if (twin != entries_.end()) {
                error(node.line, "doppelte Kennung " + quote(node.value) +
                                     " für Ereignis (bereits definiert in " + twin->source->name + ":" +
                                     std::to_string(twin->node->line) + ")");
                continue;
            }
            EventEntry entry{node.value, &source, &node, EventScope::State, {}};
            if (const ScriptNode* scope = node.find("wer"); scope != nullptr && !scope->is_block) {
                entry.scope = scope->value == "welt" ? EventScope::World : EventScope::State;
            }
            for (const ScriptNode& child : node.children) {
                if (child.key == "option" && child.is_block && is_lower_id(child.value, false)) {
                    entry.options.push_back(child.value);
                }
            }
            entries_.push_back(std::move(entry));
        }
    }

    Index event_index(std::string_view id) const {
        const auto it = index_.find(id);
        return it == index_.end() ? kNone : it->second;
    }

    // ── Zweiter Durchgang ──

    void load_event(const EventEntry& entry, EventDef& event) {
        const ScriptNode& node = *entry.node;
        const std::string what = "Ereignis " + entry.id;
        event.id = entry.id;
        event.scope = entry.scope;
        event.file = file_;
        event.line = node.line;
        std::map<std::string, int, std::less<>> seen;
        std::map<std::string, int, std::less<>> option_lines;
        for (const ScriptNode& child : node.children) {
            if (child.key == "option") {
                read_option(child, entry, event, option_lines);
                continue;
            }
            if (child.key.empty()) {
                warning(child.line, "unerwarteter Eintrag in " + what + " wird ignoriert");
                continue;
            }
            if (!first_time(seen, child, what)) {
                continue;
            }
            const std::string& key = child.key;
            if (key == "titel") {
                if (scalar(child)) {
                    if (auto t = text(child)) {
                        event.title = *t;
                    }
                }
            } else if (key == "text") {
                if (auto t = text(child)) {
                    event.text = *t;
                }
            } else if (key == "datum") {
                if (assign(child)) {
                    if (const auto d = date(child)) {
                        event.has_date = true;
                        event.date = *d;
                    }
                }
            } else if (key == "quelle") {
                if (scalar(child)) {
                    if (auto t = text(child)) {
                        event.source = *t;
                    }
                }
            } else if (key == "sicherheit") {
                if (scalar(child) && assign(child)) {
                    static constexpr std::string_view kCertainty[] = {"gesichert", "schaetzung", "spielwert",
                                                                      "ungeprueft"};
                    if (std::find(std::begin(kCertainty), std::end(kCertainty), child.value) ==
                        std::end(kCertainty)) {
                        error(child.line, "ungültige Sicherheit " + quote(child.value) +
                                              " (erlaubt: gesichert, schaetzung, spielwert, ungeprueft)");
                    } else {
                        event.certainty = child.value;
                    }
                }
            } else if (key == "art") {
                if (scalar(child) && assign(child)) {
                    if (child.value == "historisch") {
                        event.kind = EventKind::Historical;
                    } else if (child.value == "dynamisch") {
                        event.kind = EventKind::Dynamic;
                    } else if (child.value == "atmosphaerisch") {
                        event.kind = EventKind::Atmospheric;
                    } else {
                        error(child.line, "ungültige Art " + quote(child.value) +
                                              " (erlaubt: historisch, dynamisch, atmosphaerisch)");
                    }
                }
            } else if (key == "wendepunkt") {
                if (const auto b = boolean(child)) {
                    event.turning_point = *b;
                }
            } else if (key == "wer") {
                if (scalar(child) && assign(child) && child.value != "staat" && child.value != "welt") {
                    error(child.line,
                          "ungültiger Wert " + quote(child.value) + " für 'wer' (erlaubt: staat, welt)");
                }
            } else if (key == "bedingung") {
                event.condition = condition_block(child, event.scope == EventScope::State);
            } else if (key == "wartezeit") {
                if (const auto days = integer(child, 0, 36500)) {
                    event.wait = static_cast<std::int32_t>(*days);
                }
            } else if (key == "termin") {
                if (assign(child)) {
                    if (const auto d = date(child)) {
                        event.has_fixed_date = true;
                        event.fixed_date = *d;
                    }
                }
            } else if (key == "wiederholbar") {
                if (const auto b = boolean(child)) {
                    event.repeatable = *b;
                }
            } else if (key == "nur_folge") {
                if (const auto b = boolean(child)) {
                    event.follow_only = *b;
                }
            } else if (key == "abstand") {
                if (const auto days = integer(child, 1, 36500)) {
                    event.spacing = static_cast<std::int32_t>(*days);
                }
            } else if (key == "frist") {
                if (const auto days = integer(child, 1, 365)) {
                    event.deadline = static_cast<std::int32_t>(*days);
                }
            } else {
                warning(child.line, "unbekanntes Feld " + quote(key) + " in " + what + " wird ignoriert");
            }
        }
        if (seen.count("titel") == 0) {
            error(node.line, "Pflichtfeld 'titel' fehlt in " + what);
        }
        if (seen.count("text") == 0) {
            error(node.line, "Pflichtfeld 'text' fehlt in " + what);
        }
        if (seen.count("art") == 0) {
            error(node.line,
                  "Pflichtfeld 'art' fehlt in " + what + " (historisch, dynamisch oder atmosphaerisch)");
        }
        if (event.kind == EventKind::Historical && seen.count("art") != 0 && seen.count("quelle") == 0) {
            error(node.line, "Pflichtfeld 'quelle' fehlt in " + what + " (Pflicht für art = historisch)");
        }
        if (seen.count("termin") != 0 && seen.count("wartezeit") != 0) {
            error(seen["termin"], "'termin' und 'wartezeit' schließen sich aus in " + what);
        }
        if (event.follow_only && (seen.count("termin") != 0 || seen.count("wartezeit") != 0)) {
            error(node.line, "'nur_folge' schließt 'termin' und 'wartezeit' aus in " + what);
        }
        if (seen.count("abstand") != 0 && !event.repeatable) {
            error(seen["abstand"], "'abstand' gilt nur mit wiederholbar = ja in " + what);
        }
        if (event.repeatable && seen.count("abstand") == 0) {
            error(node.line,
                  "wiederholbares " + what + " braucht 'abstand' (Tage bis zum nächsten Eintritt)");
        }
        if (event.options.empty()) {
            error(node.line, what + " braucht mindestens eine Option (option <name> { … })");
            return;
        }
        const auto historical = std::count_if(event.options.begin(), event.options.end(),
                                              [](const EventOption& o) { return o.historical; });
        if (historical > 1) {
            error(node.line, what + " hat mehr als eine Option mit historisch = ja");
        }
        Fixed total;
        for (const EventOption& option : event.options) {
            total += option.weight;
        }
        if (total <= Fixed::zero()) {
            error(node.line, "die Gewichte der Optionen in " + what + " müssen zusammen größer als 0 sein");
        }
    }

    void read_option(const ScriptNode& node, const EventEntry& entry, EventDef& event,
                     std::map<std::string, int, std::less<>>& option_lines) {
        if (!node.is_block || node.value.empty()) {
            error(node.line, "ungültige Option, erwartet: option <name> { … }");
            return;
        }
        if (!is_lower_id(node.value, false)) {
            error(node.line, "ungültiger Name " + quote(node.value) +
                                 " für Option (erlaubt: a-z, 0-9 und _, beginnend mit einem Buchstaben)");
            return;
        }
        const auto [it, inserted] = option_lines.emplace(node.value, node.line);
        if (!inserted) {
            error(node.line, "doppelte Option " + quote(node.value) + " in Ereignis " + entry.id +
                                 " (zuerst in Zeile " + std::to_string(it->second) + ")");
            return;
        }
        const bool has_state = event.scope == EventScope::State;
        const std::string what = "Option " + node.value + " von " + entry.id;
        EventOption option;
        option.id = node.value;
        std::map<std::string, int, std::less<>> seen;
        for (const ScriptNode& child : node.children) {
            if (child.key.empty()) {
                warning(child.line, "unerwarteter Eintrag in " + what + " wird ignoriert");
                continue;
            }
            if (child.key == "gewicht_wenn") {
                if (!child.is_block) {
                    error(child.line, "'gewicht_wenn' erwartet einen Block: { <bedingungen> plus = n }");
                    continue;
                }
                read_weight_modifier(child, has_state, option);
                continue;
            }
            if (!first_time(seen, child, what)) {
                continue;
            }
            if (child.key == "text") {
                if (scalar(child)) {
                    if (auto t = text(child)) {
                        option.text = *t;
                    }
                }
            } else if (child.key == "historisch") {
                if (const auto b = boolean(child)) {
                    option.historical = *b;
                }
            } else if (child.key == "gewicht") {
                if (assign(child)) {
                    if (const auto w = number(child, Fixed::zero(), fx(1000))) {
                        option.weight = *w;
                    }
                }
            } else if (child.key == "bedingung") {
                option.condition = condition_block(child, has_state);
            } else if (child.key == "wirkung") {
                if (!child.is_block) {
                    error(child.line, "'wirkung' erwartet einen Block in geschweiften Klammern");
                    continue;
                }
                option.effects = effects(child, has_state);
            } else {
                warning(child.line,
                        "unbekanntes Feld " + quote(child.key) + " in " + what + " wird ignoriert");
            }
        }
        if (seen.count("text") == 0) {
            error(node.line, "Pflichtfeld 'text' fehlt in " + what);
        }
        if (seen.count("gewicht") == 0) {
            error(node.line, "Pflichtfeld 'gewicht' fehlt in " + what);
        }
        event.options.push_back(std::move(option));
    }

    void read_weight_modifier(const ScriptNode& node, bool has_state, EventOption& option) {
        WeightModifier modifier;
        modifier.condition.kind = ConditionKind::All;
        bool has_plus = false;
        for (const ScriptNode& child : node.children) {
            if (child.key == "plus") {
                if (has_plus) {
                    error(child.line, "'plus' ist in gewicht_wenn mehrfach angegeben");
                    continue;
                }
                has_plus = true;
                if (assign(child)) {
                    if (const auto v = number(child, fx(-1000), fx(1000))) {
                        modifier.plus = *v;
                    }
                }
                continue;
            }
            read_check(child, has_state, modifier.condition.children);
        }
        if (!has_plus) {
            error(node.line, "Pflichtfeld 'plus' fehlt in gewicht_wenn");
        }
        option.weight_modifiers.push_back(std::move(modifier));
    }

    // ── Bedingungen (Abschnitt 3.2) ──

    Condition condition_block(const ScriptNode& node, bool has_state) {
        Condition result;
        result.kind = ConditionKind::All;
        if (!node.is_block) {
            error(node.line, quote(node.key) + " erwartet einen Block in geschweiften Klammern");
            return result;
        }
        for (const ScriptNode& child : node.children) {
            read_check(child, has_state, result.children);
        }
        return result;
    }

    static Condition never() {
        Condition c;
        c.kind = ConditionKind::Never;
        return c;
    }

    bool need_state(const ScriptNode& node, bool has_state) {
        if (!has_state) {
            error(node.line, quote(node.key) +
                                 " braucht einen Staat; in einem Welt-Ereignis nur in staat <TAG> { … } oder "
                                 "ein_staat { … }");
        }
        return has_state;
    }

    std::optional<CompareOp> equality(const ScriptNode& node) {
        if (node.op == "=") {
            return CompareOp::Eq;
        }
        if (node.op == "!=") {
            return CompareOp::Ne;
        }
        error(node.line, quote(node.key) + " erwartet '=' oder '!=' statt " + quote(node.op));
        return std::nullopt;
    }

    std::optional<CompareOp> comparison(const ScriptNode& node) {
        const auto op = compare_op(node.op);
        if (!op) {
            error(node.line, quote(node.key) + " erwartet einen Vergleich (=, !=, <, <=, >, >=)");
        }
        return op;
    }

    /// Staat aus einem Tag: kNone mit absent = true, wenn es ihn zum Startdatum nicht gibt.
    std::optional<Index> state_ref(const ScriptNode& node, std::string_view tag, bool& is_absent) {
        is_absent = false;
        const Index state = world_.find_state(tag);
        if (state != kNone) {
            return state;
        }
        if (absent("staat", tag)) {
            is_absent = true;
            return kNone;
        }
        error(node.line, "unbekannter Staat " + quote(tag) + " in " + quote(node.key));
        return std::nullopt;
    }

    /// Kennung hinter einem Präfix wie "zustimmung_"; Fehler, wenn sie fehlt.
    struct Prefixed {
        std::string_view base;
        std::string_view id;
    };

    static std::optional<Prefixed> split(std::string_view key,
                                         std::initializer_list<std::string_view> bases) {
        for (const std::string_view base : bases) {
            if (starts_with(key, std::string(base) + "_")) {
                return Prefixed{base, key.substr(base.size() + 1)};
            }
        }
        return std::nullopt;
    }

    void read_check(const ScriptNode& node, bool has_state, std::vector<Condition>& out) {
        if (node.key.empty()) {
            warning(node.line, "unerwarteter Eintrag in einer Bedingung wird ignoriert");
            return;
        }
        if (auto c = check(node, has_state)) {
            out.push_back(std::move(*c));
        }
    }

    std::optional<Condition> nested(const ScriptNode& node, ConditionKind kind, bool has_state) {
        if (!node.is_block || !node.value.empty()) {
            error(node.line, quote(node.key) + " erwartet einen Block: " + node.key + " { … }");
            return std::nullopt;
        }
        Condition c = condition_block(node, has_state);
        c.kind = kind;
        return c;
    }

    std::optional<Condition> check(const ScriptNode& node, bool has_state) {
        const std::string& key = node.key;
        Condition c;
        // Verknüpfungen und Wechsel des Staates
        if (key == "alle") {
            return nested(node, ConditionKind::All, has_state);
        }
        if (key == "oder") {
            return nested(node, ConditionKind::Any, has_state);
        }
        if (key == "nicht") {
            return nested(node, ConditionKind::Not, has_state);
        }
        if (key == "ein_staat") {
            return nested(node, ConditionKind::AnyState, true);
        }
        if (key == "staat" && node.is_block) {
            if (node.value.empty()) {
                error(node.line, "'staat' erwartet einen Tag: staat <TAG> { … }");
                return std::nullopt;
            }
            bool is_absent = false;
            const auto state = state_ref(node, node.value, is_absent);
            if (!state) {
                return std::nullopt;
            }
            Condition inner = condition_block(node, true);
            if (is_absent) {
                return never();
            }
            inner.kind = ConditionKind::OfState;
            inner.a = *state;
            return inner;
        }
        if (key == "geschehen") {
            return happened(node, has_state);
        }
        if (key == "sitze") {
            return seats(node);
        }
        if (key == "bevoelkerungsanteil" || key == "staaten") {
            return state_count(node);
        }
        if (node.is_block && key != "staat_in") {
            error(node.line, "unbekannte Bedingung " + quote(key) + " (Vokabular: chronik-skript.md 3.34)");
            return std::nullopt;
        }
        // Einfache Prüfungen
        if (key == "datum") {
            const auto op = comparison(node);
            const auto d = date(node);
            if (!op || !d) {
                return std::nullopt;
            }
            c.kind = ConditionKind::Date;
            c.op = *op;
            c.date = *d;
            return c;
        }
        if (key == "staat") {
            const auto op = equality(node);
            if (!op || !need_state(node, has_state)) {
                return std::nullopt;
            }
            bool is_absent = false;
            const auto state = state_ref(node, node.value, is_absent);
            if (!state) {
                return std::nullopt;
            }
            if (is_absent) {
                // Ein Staat, den es nicht gibt, ist es nie; jeder andere ist es nicht.
                if (*op == CompareOp::Eq) {
                    return never();
                }
                c.kind = ConditionKind::All;
                return c;
            }
            c.kind = ConditionKind::State;
            c.op = *op;
            c.a = *state;
            return c;
        }
        if (key == "staat_in") {
            if (!need_state(node, has_state)) {
                return std::nullopt;
            }
            if (!node.is_block || !assign(node)) {
                error(node.line, "'staat_in' erwartet eine Liste: staat_in = { TAG TAG … }");
                return std::nullopt;
            }
            c.kind = ConditionKind::StateIn;
            for (const ScriptNode& entry : node.children) {
                if (!entry.key.empty() || entry.is_block) {
                    error(entry.line, "in 'staat_in' sind nur Tags erlaubt");
                    continue;
                }
                bool is_absent = false;
                if (const auto state = state_ref(node, entry.value, is_absent); state && !is_absent) {
                    if (std::find(c.list.begin(), c.list.end(), *state) == c.list.end()) {
                        c.list.push_back(*state);
                    }
                }
            }
            std::sort(c.list.begin(), c.list.end());
            return c;
        }
        if (key == "merkmal" || key == "welt_merkmal") {
            if (key == "merkmal" && !need_state(node, has_state)) {
                return std::nullopt;
            }
            const auto flag = name(node);
            if (!flag) {
                return std::nullopt;
            }
            c.kind = key == "merkmal" ? ConditionKind::Flag : ConditionKind::WorldFlag;
            c.name = *flag;
            return c;
        }
        if (key == "verfassung" || key == "wahlrecht") {
            const auto op = equality(node);
            if (!op || !need_state(node, has_state)) {
                return std::nullopt;
            }
            c.op = *op;
            if (key == "verfassung") {
                const auto value = parse_constitution(node.value);
                if (!value) {
                    error(node.line, "ungültige Verfassung " + quote(node.value) +
                                         " (erlaubt: keine, staendisch, konstitutionell)");
                    return std::nullopt;
                }
                c.kind = ConditionKind::Constitution;
                c.a = static_cast<Index>(*value);
            } else {
                const auto value = parse_franchise(node.value);
                if (!value) {
                    error(node.line,
                          "ungültiges Wahlrecht " + quote(node.value) +
                              " (erlaubt: keines, staendisch, zensus, dreiklassen, allgemein_maenner)");
                    return std::nullopt;
                }
                c.kind = ConditionKind::Franchise;
                c.a = static_cast<Index>(*value);
            }
            return c;
        }
        if (key == "im_krieg" || key == "mobil" || key == "bund" || key == "spieler" || key == "aufstand") {
            if (!need_state(node, has_state)) {
                return std::nullopt;
            }
            const auto value = boolean(node);
            if (!value) {
                return std::nullopt;
            }
            c.kind = key == "im_krieg"  ? ConditionKind::AtWar
                     : key == "mobil"   ? ConditionKind::Mobilized
                     : key == "bund"    ? ConditionKind::Federal
                     : key == "spieler" ? ConditionKind::Player
                                        : ConditionKind::InRevolt;
            if (*value) {
                return c;
            }
            Condition negation;
            negation.kind = ConditionKind::Not;
            negation.children.push_back(std::move(c));
            return negation;
        }
        if (key == "krieg_mit" || key == "krieg_gewonnen" || key == "krieg_verloren") {
            if (!need_state(node, has_state) || !assign(node)) {
                return std::nullopt;
            }
            bool is_absent = false;
            const auto state = state_ref(node, node.value, is_absent);
            if (!state) {
                return std::nullopt;
            }
            if (is_absent) {
                return never();
            }
            c.kind = key == "krieg_mit"        ? ConditionKind::WarWith
                     : key == "krieg_gewonnen" ? ConditionKind::WarWon
                                               : ConditionKind::WarLost;
            c.a = *state;
            return c;
        }
        if (key == "zollunion") {
            const auto op = equality(node);
            if (!op || !need_state(node, has_state)) {
                return std::nullopt;
            }
            c.kind = ConditionKind::CustomsUnion;
            c.op = *op;
            if (node.value == "keine") {
                c.a = kNone;
                return c;
            }
            c.a = world_.find_customs_union(node.value);
            if (c.a == kNone) {
                if (absent("zollunion", node.value)) {
                    return *op == CompareOp::Eq ? never() : Condition{};
                }
                error(node.line, "unbekannte Zollunion " + quote(node.value) + " in 'zollunion'");
                return std::nullopt;
            }
            return c;
        }
        // Zahlen
        struct Numeric {
            std::string_view key;
            ConditionKind kind;
            Fixed lo;
            Fixed hi;
        };
        const Numeric numerics[] = {
            {"unruhe", ConditionKind::Unrest, fx(0), fx(100)},
            {"nachbar_unruhe", ConditionKind::NeighborUnrest, fx(0), fx(100)},
            {"lebensstandard_unten", ConditionKind::LivingLower, fx(0), fx(100)},
            {"soldaten", ConditionKind::Soldiers, fx(0), fx(10000000)},
            {"schatz", ConditionKind::Treasury, fx(-1000000000000), fx(1000000000000)},
            {"schulden_quote", ConditionKind::DebtRatio, fx(0), fx(1000)},
        };
        for (const Numeric& n : numerics) {
            if (key == n.key) {
                const auto op = comparison(node);
                if (!op || !need_state(node, has_state)) {
                    return std::nullopt;
                }
                const auto value = number(node, n.lo, n.hi);
                if (!value) {
                    return std::nullopt;
                }
                c.kind = n.kind;
                c.op = *op;
                c.value = *value;
                return c;
            }
        }
        if (key == "bundesversammlung") {
            const auto op = equality(node);
            if (!op || (node.value != "ruht" && node.value != "tagt")) {
                if (op) {
                    error(node.line, "'bundesversammlung' erwartet ruht oder tagt");
                }
                return std::nullopt;
            }
            c.kind = ConditionKind::DietSuspended;
            const bool suspended = (node.value == "ruht") == (*op == CompareOp::Eq);
            if (suspended) {
                return c;
            }
            Condition negation;
            negation.kind = ConditionKind::Not;
            negation.children.push_back(std::move(c));
            return negation;
        }
        if (const auto p = split(key, {"journal", "abstimmung"})) {
            return journal_or_vote_check(node, *p);
        }
        if (const auto p = split(key, {"gesetz", "zustimmung", "anteil", "einfluss", "preis", "beziehung"})) {
            return prefixed_check(node, *p, has_state);
        }
        error(node.line, "unbekannte Bedingung " + quote(key) + " (Vokabular: chronik-skript.md 3.34)");
        return std::nullopt;
    }

    std::optional<Condition> prefixed_check(const ScriptNode& node, const Prefixed& p, bool has_state) {
        Condition c;
        if (!need_state(node, has_state)) {
            return std::nullopt;
        }
        if (p.base == "gesetz") {
            const auto op = equality(node);
            if (!op) {
                return std::nullopt;
            }
            const Index law = world_.find_law_group(p.id);
            if (law == kNone) {
                if (absent("gesetz", p.id)) {
                    return *op == CompareOp::Eq ? never() : Condition{};
                }
                error(node.line, "unbekannte Gesetzesgruppe " + quote(p.id) + " in " + quote(node.key));
                return std::nullopt;
            }
            const Index option = option_of(node, law);
            if (option == kNone) {
                return std::nullopt;
            }
            c.kind = ConditionKind::Law;
            c.op = *op;
            c.a = law;
            c.b = option;
            return c;
        }
        const auto op = comparison(node);
        if (!op) {
            return std::nullopt;
        }
        c.op = *op;
        if (p.base == "zustimmung" || p.base == "anteil" || p.base == "einfluss") {
            const Index group = world_.find_interest_group(p.id);
            if (group == kNone) {
                if (absent("interessengruppe", p.id)) {
                    return never();
                }
                error(node.line, "unbekannte Interessengruppe " + quote(p.id) + " in " + quote(node.key));
                return std::nullopt;
            }
            const bool approval = p.base == "zustimmung";
            const auto value = approval ? number(node, fx(-10), fx(10)) : number(node, fx(0), fx(1));
            if (!value) {
                return std::nullopt;
            }
            c.kind = approval             ? ConditionKind::Approval
                     : p.base == "anteil" ? ConditionKind::Share
                                          : ConditionKind::Influence;
            c.a = group;
            c.value = *value;
            return c;
        }
        if (p.base == "preis") {
            const Index good = world_.find_good(p.id);
            if (good == kNone) {
                if (absent("gut", p.id)) {
                    return never();
                }
                error(node.line, "unbekanntes Gut " + quote(p.id) + " in " + quote(node.key));
                return std::nullopt;
            }
            const auto value = number(node, fx(0), fx(100));
            if (!value) {
                return std::nullopt;
            }
            c.kind = ConditionKind::Price;
            c.a = good;
            c.value = *value;
            return c;
        }
        // beziehung_<TAG>
        bool is_absent = false;
        const auto state = state_ref(node, p.id, is_absent);
        if (!state) {
            return std::nullopt;
        }
        const auto value = number(node, fx(-200), fx(200));
        if (!value) {
            return std::nullopt;
        }
        if (is_absent) {
            return never();
        }
        c.kind = ConditionKind::Relation;
        c.a = *state;
        c.value = *value;
        return c;
    }

    Index option_of(const ScriptNode& node, Index law) {
        const LawGroup& group = world_.law_groups[static_cast<std::size_t>(law)];
        for (std::size_t i = 0; i < group.options.size(); ++i) {
            if (group.options[i].id == node.value) {
                return static_cast<Index>(i);
            }
        }
        std::string known;
        for (const LawOption& o : group.options) {
            known += (known.empty() ? "" : ", ") + o.id;
        }
        error(node.line, "unbekannte Option " + quote(node.value) + " der Gesetzesgruppe " + group.id +
                             " (erlaubt: " + known + ")");
        return kNone;
    }

    std::optional<Condition> happened(const ScriptNode& node, bool has_state) {
        std::string id;
        std::string option;
        int option_line = node.line;
        if (!node.is_block) {
            if (!assign(node)) {
                return std::nullopt;
            }
            id = node.value;
        } else {
            for (const ScriptNode& child : node.children) {
                if (child.key == "ereignis" && !child.is_block) {
                    id = child.value;
                } else if (child.key == "option" && !child.is_block) {
                    option = child.value;
                    option_line = child.line;
                } else {
                    error(child.line, "in 'geschehen' sind nur 'ereignis' und 'option' erlaubt");
                }
            }
            if (id.empty()) {
                error(node.line, "Pflichtfeld 'ereignis' fehlt in geschehen");
                return std::nullopt;
            }
        }
        const Index event = event_index(id);
        if (event == kNone) {
            error(node.line, "unbekanntes Ereignis " + quote(id) + " in 'geschehen'");
            return std::nullopt;
        }
        const EventEntry& target = entries_[static_cast<std::size_t>(event)];
        if (target.scope == EventScope::State && !has_state) {
            error(node.line, "'geschehen' auf das Staatsereignis " + id +
                                 " braucht einen Staat; in einem Welt-Ereignis nur in staat <TAG> { … } oder "
                                 "ein_staat { … }");
            return std::nullopt;
        }
        Condition c;
        c.kind = ConditionKind::Happened;
        c.a = event;
        if (!option.empty()) {
            const auto it = std::find(target.options.begin(), target.options.end(), option);
            if (it == target.options.end()) {
                error(option_line, "unbekannte Option " + quote(option) + " von Ereignis " + id);
                return std::nullopt;
            }
            c.b = static_cast<Index>(it - target.options.begin());
        }
        return c;
    }

    /// Sitze einer Strömung nach der letzten Wahl; gehören zum Parlament, nicht zum Staat.
    std::optional<Condition> seats(const ScriptNode& node) {
        if (!node.is_block || !node.value.empty()) {
            error(node.line,
                  "'sitze' erwartet einen Block: sitze { parlament = … stroemung = … mindestens = n }");
            return std::nullopt;
        }
        const ScriptNode* parliament = nullptr;
        const ScriptNode* current = nullptr;
        const ScriptNode* minimum = nullptr;
        for (const ScriptNode& child : node.children) {
            const ScriptNode** slot = child.key == "parlament"    ? &parliament
                                      : child.key == "stroemung"  ? &current
                                      : child.key == "mindestens" ? &minimum
                                                                  : nullptr;
            if (slot == nullptr) {
                error(child.line, "in 'sitze' sind nur 'parlament', 'stroemung' und 'mindestens' erlaubt");
                continue;
            }
            if (*slot != nullptr) {
                error(child.line, quote(child.key) + " ist in sitze mehrfach angegeben");
                continue;
            }
            *slot = &child;
        }
        if (parliament == nullptr || current == nullptr || minimum == nullptr) {
            error(node.line, "'sitze' braucht parlament, stroemung und mindestens");
            return std::nullopt;
        }
        const auto seats_min = integer(*minimum, 0, 10000);
        if (!seats_min || !scalar(*parliament) || !scalar(*current)) {
            return std::nullopt;
        }
        Condition c;
        c.kind = ConditionKind::Seats;
        c.op = CompareOp::Ge;
        c.value = fx(*seats_min);
        c.a = world_.find_current(current->value);
        if (c.a == kNone) {
            error(current->line, "unbekannte Strömung " + quote(current->value) + " in 'sitze'");
            return std::nullopt;
        }
        c.b = world_.find_parliament(parliament->value);
        if (c.b == kNone) {
            if (absent("parlament", parliament->value)) {
                return never();
            }
            error(parliament->line, "unbekanntes Parlament " + quote(parliament->value) + " in 'sitze'");
            return std::nullopt;
        }
        return c;
    }

    // ── Wirkungen (Abschnitt 3.3) ──

    std::vector<Effect> effects(const ScriptNode& block, bool has_state) {
        std::vector<Effect> result;
        for (const ScriptNode& child : block.children) {
            if (child.key.empty()) {
                warning(child.line, "unerwarteter Eintrag in einer Wirkung wird ignoriert");
                continue;
            }
            bool skip = false; // Verweis auf späteren Inhalt: die Wirkung entfällt
            if (auto e = effect(child, has_state, skip); e && !skip) {
                result.push_back(std::move(*e));
            }
        }
        return result;
    }

    std::optional<Effect> effect(const ScriptNode& node, bool has_state, bool& skip) {
        const std::string& key = node.key;
        Effect e;
        if (key == "staat" && node.is_block) {
            if (node.value.empty()) {
                error(node.line, "'staat' erwartet einen Tag: staat <TAG> { … }");
                return std::nullopt;
            }
            bool is_absent = false;
            const auto state = state_ref(node, node.value, is_absent);
            e.children = effects(node, true);
            if (!state) {
                return std::nullopt;
            }
            skip = is_absent;
            e.kind = EffectKind::OfState;
            e.a = *state;
            return e;
        }
        if (key == "alle_staaten") {
            if (!node.is_block || !node.value.empty()) {
                error(node.line, "'alle_staaten' erwartet einen Block: alle_staaten { … }");
                return std::nullopt;
            }
            e.kind = EffectKind::AllStates;
            e.children = effects(node, true);
            return e;
        }
        if (key == "folge") {
            return follow(node, has_state, skip);
        }
        if (key == "befristet") {
            return timed(node, has_state);
        }
        if (key == "zufall") {
            return random(node, has_state);
        }
        if (key == "verwuestung") {
            return devastation(node, has_state, skip);
        }
        if (key == "antrag") {
            return motion(node, has_state, skip);
        }
        if (key == "krieg" && node.is_block) {
            return war(node, has_state, skip);
        }
        if (node.is_block) {
            error(node.line, "unbekannte Wirkung " + quote(key) + " (Vokabular: chronik-skript.md 3.34)");
            return std::nullopt;
        }
        if (!assign(node)) {
            return std::nullopt;
        }
        if (key == "chronik") {
            if (auto t = text(node)) {
                e.kind = EffectKind::Chronicle;
                e.name = *t;
                return e;
            }
            return std::nullopt;
        }
        if (key == "welt_merke" || key == "welt_vergiss") {
            const auto flag = name(node);
            if (!flag) {
                return std::nullopt;
            }
            e.kind = key == "welt_merke" ? EffectKind::SetWorldFlag : EffectKind::ClearWorldFlag;
            e.name = *flag;
            return e;
        }
        if (const auto p = split(key, {"journal"})) {
            const Index journal = world_.find_journal(p->id);
            if (journal == kNone) {
                error(node.line, "unbekanntes Journal " + quote(p->id) + " in " + quote(key));
                return std::nullopt;
            }
            const Index stage = stage_of(node, journal, node.value);
            if (stage == kNone) {
                return std::nullopt;
            }
            e.kind = EffectKind::Journal;
            e.a = journal;
            e.b = stage;
            return e;
        }
        if (key == "abstimmung") {
            e.kind = EffectKind::Vote;
            e.a = world_.find_vote(node.value);
            if (e.a == kNone) {
                error(node.line, "unbekannte Abstimmung " + quote(node.value));
                return std::nullopt;
            }
            return e;
        }
        if (key == "bundesversammlung") {
            if (node.value != "ruht" && node.value != "tagt") {
                error(node.line, "'bundesversammlung' erwartet ruht oder tagt");
                return std::nullopt;
            }
            e.kind = EffectKind::Diet;
            e.a = node.value == "ruht" ? 1 : 0;
            return e;
        }
        // Ab hier wirkt alles auf einen Staat.
        const bool known = key == "merke" || key == "vergiss" || key == "verfassung" || key == "wahlrecht" ||
                           key == "krieg" || key == "unruhe" || key == "schatz" || key == "monatseinnahmen" ||
                           key == "auswanderung" || key == "mobilmachung" || key == "aufstand" ||
                           key == "intervention" ||
                           split(key, {"gesetz", "zustimmung", "beziehung"}).has_value();
        if (!known) {
            error(node.line, "unbekannte Wirkung " + quote(key) + " (Vokabular: chronik-skript.md 3.34)");
            return std::nullopt;
        }
        if (!has_state) {
            error(node.line, "Wirkung " + quote(key) +
                                 " braucht einen Staat; in einem Welt-Ereignis nur in staat <TAG> { … } oder "
                                 "alle_staaten { … }");
            return std::nullopt;
        }
        if (key == "merke" || key == "vergiss") {
            const auto flag = name(node);
            if (!flag) {
                return std::nullopt;
            }
            e.kind = key == "merke" ? EffectKind::SetFlag : EffectKind::ClearFlag;
            e.name = *flag;
            return e;
        }
        if (key == "verfassung") {
            const auto value = parse_constitution(node.value);
            if (!value) {
                error(node.line, "ungültige Verfassung " + quote(node.value) +
                                     " (erlaubt: keine, staendisch, konstitutionell)");
                return std::nullopt;
            }
            e.kind = EffectKind::Constitution;
            e.a = static_cast<Index>(*value);
            return e;
        }
        if (key == "wahlrecht") {
            const auto value = parse_franchise(node.value);
            if (!value) {
                error(node.line,
                      "ungültiges Wahlrecht " + quote(node.value) +
                          " (erlaubt: keines, staendisch, zensus, dreiklassen, allgemein_maenner)");
                return std::nullopt;
            }
            e.kind = EffectKind::Franchise;
            e.a = static_cast<Index>(*value);
            return e;
        }
        if (key == "krieg") {
            bool is_absent = false;
            const auto target = state_ref(node, node.value, is_absent);
            if (!target) {
                return std::nullopt;
            }
            skip = is_absent;
            e.kind = EffectKind::War;
            e.a = *target;
            return e;
        }
        if (key == "aufstand") {
            const RevoltKind kind = node.value == "barrikaden"     ? RevoltKind::Barricades
                                    : node.value == "agrarunruhen" ? RevoltKind::Agrarian
                                                                   : RevoltKind::Uprising;
            if (node.value != "barrikaden" && node.value != "agrarunruhen" && node.value != "erhebung") {
                error(node.line, "ungültige Art " + quote(node.value) +
                                     " für 'aufstand' (erlaubt: barrikaden, agrarunruhen, erhebung)");
                return std::nullopt;
            }
            e.kind = EffectKind::Revolt;
            e.a = static_cast<Index>(kind);
            return e;
        }
        if (key == "intervention") {
            bool is_absent = false;
            const auto target = state_ref(node, node.value, is_absent);
            if (!target) {
                return std::nullopt;
            }
            skip = is_absent;
            e.kind = EffectKind::Intervention;
            e.a = *target;
            return e;
        }
        if (key == "mobilmachung") {
            if (node.value != "ja" && node.value != "nein") {
                error(node.line, "'mobilmachung' erwartet ja oder nein");
                return std::nullopt;
            }
            e.kind = EffectKind::Mobilize;
            e.a = node.value == "ja" ? 1 : 0; // seit b3: nein rüstet ab (nicht im Krieg)
            return e;
        }
        struct Amount {
            std::string_view key;
            EffectKind kind;
            Fixed lo;
            Fixed hi;
        };
        const Amount amounts[] = {
            {"unruhe", EffectKind::Unrest, fx(-100), fx(100)},
            {"schatz", EffectKind::Treasury, fx(-1000000000000), fx(1000000000000)},
            {"monatseinnahmen", EffectKind::MonthlyIncome, fx(-120), fx(120)},
            {"auswanderung", EffectKind::Emigration, fx(0), fx(1)},
        };
        for (const Amount& a : amounts) {
            if (key == a.key) {
                const auto value = number(node, a.lo, a.hi);
                if (!value) {
                    return std::nullopt;
                }
                e.kind = a.kind;
                e.value = *value;
                return e;
            }
        }
        const auto p = split(key, {"gesetz", "zustimmung", "beziehung"});
        if (p->base == "gesetz") {
            const Index law = world_.find_law_group(p->id);
            if (law == kNone) {
                if (absent("gesetz", p->id)) {
                    skip = true;
                    return e;
                }
                error(node.line, "unbekannte Gesetzesgruppe " + quote(p->id) + " in " + quote(key));
                return std::nullopt;
            }
            const Index option = option_of(node, law);
            if (option == kNone) {
                return std::nullopt;
            }
            e.kind = EffectKind::Law;
            e.a = law;
            e.b = option;
            return e;
        }
        if (p->base == "zustimmung") {
            const Index group = world_.find_interest_group(p->id);
            const auto value = number(node, fx(-20), fx(20));
            if (group == kNone) {
                if (absent("interessengruppe", p->id)) {
                    skip = true;
                    return e;
                }
                error(node.line, "unbekannte Interessengruppe " + quote(p->id) + " in " + quote(key));
                return std::nullopt;
            }
            if (!value) {
                return std::nullopt;
            }
            e.kind = EffectKind::Approval;
            e.a = group;
            e.value = *value;
            return e;
        }
        // beziehung_<TAG>
        bool is_absent = false;
        const auto state = state_ref(node, p->id, is_absent);
        const auto value = number(node, fx(-400), fx(400));
        if (!state || !value) {
            return std::nullopt;
        }
        skip = is_absent;
        e.kind = EffectKind::Relation;
        e.a = *state;
        e.value = *value;
        return e;
    }

    std::optional<Effect> devastation(const ScriptNode& node, bool has_state, bool& skip) {
        Effect e;
        e.kind = EffectKind::Devastation;
        e.a = kNone;
        if (!node.is_block) {
            if (!need_state(node, has_state) || !assign(node)) {
                return std::nullopt;
            }
            const auto value = number(node, fx(-1), fx(1));
            if (!value) {
                return std::nullopt;
            }
            e.value = *value;
            return e;
        }
        const ScriptNode* place = node.find("ort");
        const ScriptNode* amount = node.find("wert");
        for (const ScriptNode& child : node.children) {
            if (child.key != "ort" && child.key != "wert") {
                error(child.line, "in 'verwuestung' sind nur 'ort' und 'wert' erlaubt");
            }
        }
        if (amount == nullptr) {
            error(node.line, "Pflichtfeld 'wert' fehlt in verwuestung");
            return std::nullopt;
        }
        const auto value = number(*amount, fx(-1), fx(1));
        if (!value) {
            return std::nullopt;
        }
        e.value = *value;
        if (place == nullptr) {
            if (!need_state(node, has_state)) {
                return std::nullopt;
            }
            return e;
        }
        if (!scalar(*place)) {
            return std::nullopt;
        }
        e.a = world_.find_location(place->value);
        if (e.a == kNone) {
            error(place->line, "unbekannter Ort " + quote(place->value) + " in 'verwuestung'");
            return std::nullopt;
        }
        if (world_.locations[static_cast<std::size_t>(e.a)].owner == kNone) {
            skip = true; // Ort ohne Simulationsdaten zum Startdatum (die Blöcke entstehen erst danach)
        }
        return e;
    }

    /// befristet { tage = n text = "…" steuerertrag = x moral = x unruhe = n beziehung_bund = n } (3.7).
    std::optional<Effect> timed(const ScriptNode& node, bool has_state) {
        if (!node.is_block || !node.value.empty()) {
            error(node.line, "'befristet' erwartet einen Block: befristet { tage = n text = \"…\" … }");
            return std::nullopt;
        }
        if (!need_state(node, has_state)) {
            return std::nullopt;
        }
        Effect e;
        e.kind = EffectKind::Timed;
        std::map<std::string, int, std::less<>> seen;
        bool ok = true;
        for (const ScriptNode& child : node.children) {
            if (child.key.empty() || !first_time(seen, child, "befristet")) {
                ok = ok && !child.key.empty();
                continue;
            }
            std::optional<Fixed> value;
            if (child.key == "tage") {
                const auto days = integer(child, 1, 36500);
                ok = ok && days.has_value();
                e.days = static_cast<std::int32_t>(days.value_or(0));
                continue;
            }
            if (child.key == "text") {
                const auto t = scalar(child) ? text(child) : std::nullopt;
                ok = ok && t.has_value();
                e.name = t.value_or("");
                continue;
            }
            if (!assign(child)) {
                ok = false;
                continue;
            }
            if (child.key == "steuerertrag") {
                value = number(child, fx(-1), fx(1));
                e.timed.tax = value.value_or(Fixed::zero());
            } else if (child.key == "moral") {
                value = number(child, fx(-1), fx(1));
                e.timed.morale = value.value_or(Fixed::zero());
            } else if (child.key == "unruhe") {
                value = number(child, fx(-100), fx(100));
                e.timed.unrest = value.value_or(Fixed::zero());
            } else if (child.key == "beziehung_bund") {
                value = number(child, fx(-100), fx(100));
                e.timed.relation = value.value_or(Fixed::zero());
            } else {
                error(child.line, "in 'befristet' sind nur tage, text, steuerertrag, moral, unruhe und "
                                  "beziehung_bund erlaubt");
            }
            ok = ok && value.has_value();
        }
        for (const char* required : {"tage", "text"}) {
            if (seen.count(required) == 0) {
                error(node.line, "Pflichtfeld '" + std::string(required) + "' fehlt in befristet");
                ok = false;
            }
        }
        if (seen.size() <= 2 && ok) {
            error(node.line, "'befristet' braucht mindestens einen Wert (steuerertrag, moral, unruhe oder "
                             "beziehung_bund)");
            ok = false;
        }
        if (!ok) {
            return std::nullopt;
        }
        return e;
    }

    std::optional<Effect> follow(const ScriptNode& node, bool has_state, bool& skip) {
        if (!node.is_block || !node.value.empty()) {
            error(node.line, "'folge' erwartet einen Block: folge { ereignis = … tage = n }");
            return std::nullopt;
        }
        Effect e;
        e.kind = EffectKind::Follow;
        e.b = kNone;
        const ScriptNode* event = nullptr;
        const ScriptNode* days = nullptr;
        const ScriptNode* state = nullptr;
        for (const ScriptNode& child : node.children) {
            const ScriptNode** slot = child.key == "ereignis" ? &event
                                      : child.key == "tage"   ? &days
                                      : child.key == "staat"  ? &state
                                                              : nullptr;
            if (slot == nullptr) {
                error(child.line, "in 'folge' sind nur 'ereignis', 'tage' und 'staat' erlaubt");
                continue;
            }
            if (*slot != nullptr) {
                error(child.line, quote(child.key) + " ist in folge mehrfach angegeben");
                continue;
            }
            *slot = &child;
        }
        if (event == nullptr) {
            error(node.line, "Pflichtfeld 'ereignis' fehlt in folge");
            return std::nullopt;
        }
        if (!scalar(*event) || !assign(*event)) {
            return std::nullopt;
        }
        e.a = event_index(event->value);
        if (e.a == kNone) {
            error(event->line, "unbekanntes Ereignis " + quote(event->value) + " in 'folge'");
            return std::nullopt;
        }
        if (days != nullptr) {
            const auto value = integer(*days, 0, 36500);
            if (!value) {
                return std::nullopt;
            }
            e.days = static_cast<std::int32_t>(*value);
        }
        const EventScope target = entries_[static_cast<std::size_t>(e.a)].scope;
        if (state != nullptr) {
            if (target == EventScope::World) {
                error(state->line, "'staat' in folge gilt nur für Staatsereignisse; " + event->value +
                                       " ist ein Welt-Ereignis");
                return std::nullopt;
            }
            if (!scalar(*state) || !assign(*state)) {
                return std::nullopt;
            }
            bool is_absent = false;
            const auto index = state_ref(*state, state->value, is_absent);
            if (!index) {
                return std::nullopt;
            }
            skip = is_absent;
            e.b = *index;
        } else if (target == EventScope::State && !has_state) {
            error(node.line, "folge auf das Staatsereignis " + event->value +
                                 " braucht einen Staat (staat = TAG oder in staat <TAG> { … })");
            return std::nullopt;
        }
        // Folgen aus Journalen und Abstimmungen (current_ = kNone) gehören zu keinem Ereignis und schließen
        // keinen Kreis.
        if (e.days == 0 && !skip && current_ != kNone) {
            follows_.push_back(FollowEdge{current_, e.a, node.line});
        }
        return e;
    }

    std::optional<Effect> random(const ScriptNode& node, bool has_state) {
        if (!node.is_block || !node.value.empty()) {
            error(node.line, "'zufall' erwartet einen Block: zufall { 30 = { … } 70 = { … } }");
            return std::nullopt;
        }
        Effect e;
        e.kind = EffectKind::Random;
        Fixed total;
        for (const ScriptNode& branch : node.children) {
            const auto weight = Fixed::parse(branch.key);
            if (!branch.is_block || !weight || *weight < Fixed::zero() || *weight > fx(1000)) {
                error(branch.line, "in 'zufall' steht je Zweig ein Gewicht von 0 bis 1000 mit einem Block, "
                                   "z. B. 30 = { … }");
                continue;
            }
            total += *weight;
            e.weights.push_back(*weight);
            e.branches.push_back(effects(branch, has_state));
        }
        if (e.weights.empty() || total <= Fixed::zero()) {
            error(node.line, "die Gewichte in 'zufall' müssen zusammen größer als 0 sein");
            return std::nullopt;
        }
        return e;
    }

    // ── Journale und Abstimmungen (Abschnitt 5, seit a4) ──

    /// Stufe oder Ausgang eines Journals nach Kennung; Fehler, wenn es sie nicht gibt.
    Index stage_of(const ScriptNode& node, Index journal, const std::string& id) {
        const JournalDef& j = world_.journals[static_cast<std::size_t>(journal)];
        for (std::size_t i = 0; i < j.stages.size(); ++i) {
            if (j.stages[i].id == id) {
                return static_cast<Index>(i);
            }
        }
        error(node.line, "unbekannte Stufe " + quote(id) + " des Journals " + j.id);
        return kNone;
    }

    std::optional<Condition> journal_or_vote_check(const ScriptNode& node, const Prefixed& p) {
        Condition c;
        const auto op = equality(node);
        if (!op) {
            return std::nullopt;
        }
        c.op = *op;
        if (p.base == "journal") {
            c.kind = ConditionKind::Journal;
            c.a = world_.find_journal(p.id);
            if (c.a == kNone) {
                error(node.line, "unbekanntes Journal " + quote(p.id) + " in " + quote(node.key));
                return std::nullopt;
            }
            c.b = stage_of(node, c.a, node.value);
            return c.b == kNone ? std::nullopt : std::optional<Condition>(c);
        }
        c.kind = ConditionKind::Vote;
        c.a = world_.find_vote(p.id);
        if (c.a == kNone) {
            error(node.line, "unbekannte Abstimmung " + quote(p.id) + " in " + quote(node.key));
            return std::nullopt;
        }
        if (node.value != "angenommen" && node.value != "abgelehnt") {
            error(node.line, quote(node.key) + " erwartet angenommen oder abgelehnt");
            return std::nullopt;
        }
        c.b = node.value == "angenommen" ? 1 : 0;
        return c;
    }

    /// bevoelkerungsanteil { wenn { … } unter { … } mindestens = x } und staaten { wenn { … } mindestens = n
    /// }: Bedingungen je Staat über alle bestehenden Staaten, in jedem Zusammenhang gültig.
    std::optional<Condition> state_count(const ScriptNode& node) {
        const bool share = node.key == "bevoelkerungsanteil";
        if (!node.is_block || !node.value.empty()) {
            error(node.line,
                  quote(node.key) + " erwartet einen Block: " + node.key + " { wenn { … } mindestens = … }");
            return std::nullopt;
        }
        Condition c;
        c.kind = share ? ConditionKind::PopulationShare : ConditionKind::CountStates;
        c.op = CompareOp::Ge;
        Condition when;
        Condition among; // leer: alle bestehenden Staaten
        bool has_when = false;
        bool has_minimum = false;
        for (const ScriptNode& child : node.children) {
            if (child.key == "wenn") {
                when = condition_block(child, true);
                has_when = true;
            } else if (child.key == "unter" && share) {
                among = condition_block(child, true);
            } else if (child.key == "mindestens") {
                has_minimum = true;
                const auto value =
                    assign(child) ? (share ? number(child, fx(0), fx(1)) : number(child, fx(0), fx(1000)))
                                  : std::nullopt;
                if (!value) {
                    return std::nullopt;
                }
                c.value = *value;
            } else {
                error(child.line, std::string("in ") + quote(node.key) + " sind nur " +
                                      (share ? "wenn, unter und mindestens" : "wenn und mindestens") +
                                      " erlaubt");
            }
        }
        if (!has_when || !has_minimum) {
            error(node.line, quote(node.key) + " braucht wenn und mindestens");
            return std::nullopt;
        }
        c.children.push_back(std::move(when));
        c.children.push_back(std::move(among));
        return c;
    }

    /// antrag { art = reform|zolleinigung|intervention|praesidium ziel = <TAG|zollunion> }
    /// (Bundesversammlung).
    /// krieg { gegner = TAG  gebiet = <einheit> …  fuehrung = ja  zollunion = ja  annexion = ja
    /// entschaedigung = n } (seit b3): Kriegserklärung mit Kriegszielen wie im Befehl (militaer.md 4.5).
    std::optional<Effect> war(const ScriptNode& node, bool has_state, bool& skip) {
        if (!node.value.empty() || !need_state(node, has_state)) {
            if (node.value.empty()) {
                return std::nullopt;
            }
            error(node.line, "'krieg' erwartet krieg = TAG oder einen Block: krieg { gegner = TAG … }");
            return std::nullopt;
        }
        Effect e;
        e.kind = EffectKind::War;
        const ScriptNode* enemy = node.find("gegner");
        if (enemy == nullptr || !scalar(*enemy)) {
            error(node.line, "Pflichtfeld 'gegner' fehlt in krieg");
            return std::nullopt;
        }
        bool is_absent = false;
        const auto target = state_ref(*enemy, enemy->value, is_absent);
        if (!target) {
            return std::nullopt;
        }
        skip = is_absent;
        e.a = *target;
        bool ok = true;
        for (const ScriptNode& child : node.children) {
            if (child.key == "gegner") {
                continue;
            }
            if (!scalar(child)) {
                ok = false;
                continue;
            }
            if (child.key == "gebiet") {
                const Index unit = world_.find_admin_unit(child.value);
                if (unit == kNone) {
                    error(child.line, "unbekannte Verwaltungseinheit " + quote(child.value) + " in 'krieg'");
                    ok = false;
                    continue;
                }
                e.goals.push_back(encode_war_goal(WarGoalKind::Territory, unit));
            } else if (child.key == "fuehrung" || child.key == "zollunion" || child.key == "annexion") {
                const auto value = boolean(child);
                if (!value) {
                    ok = false;
                } else if (*value) {
                    e.goals.push_back(child.key == "fuehrung" ? encode_war_goal(WarGoalKind::Leadership, 0)
                                      : child.key == "zollunion"
                                          ? encode_war_goal(WarGoalKind::Customs, 0)
                                          : encode_war_goal(WarGoalKind::Annexation, e.a));
                }
            } else if (child.key == "entschaedigung") {
                const auto months = number(child, Fixed::one(), Fixed::from_int(24));
                if (!months) {
                    ok = false;
                } else {
                    e.goals.push_back(
                        encode_war_goal(WarGoalKind::Indemnity, static_cast<Index>(months->round())));
                }
            } else {
                error(child.line, "in 'krieg' sind nur gegner, gebiet, fuehrung, zollunion, annexion und "
                                  "entschaedigung erlaubt");
                ok = false;
            }
        }
        if (!ok) {
            return std::nullopt;
        }
        return e;
    }

    std::optional<Effect> motion(const ScriptNode& node, bool has_state, bool& skip) {
        if (!node.is_block || !node.value.empty() || !need_state(node, has_state)) {
            if (node.is_block && node.value.empty()) {
                return std::nullopt;
            }
            error(node.line, "'antrag' erwartet einen Block: antrag { art = … ziel = … }");
            return std::nullopt;
        }
        const ScriptNode* kind = node.find("art");
        const ScriptNode* target = node.find("ziel");
        for (const ScriptNode& child : node.children) {
            if (child.key != "art" && child.key != "ziel") {
                error(child.line, "in 'antrag' sind nur art und ziel erlaubt");
            }
        }
        if (kind == nullptr || !scalar(*kind)) {
            error(node.line, "Pflichtfeld 'art' fehlt in antrag");
            return std::nullopt;
        }
        Effect e;
        e.kind = EffectKind::Motion;
        e.b = kNone;
        const std::string& k = kind->value;
        if (k == "reform" || k == "praesidium") {
            e.a = static_cast<Index>(k == "reform" ? MotionKind::Reform : MotionKind::Presidency);
            if (target != nullptr) {
                error(target->line, "Antrag " + k + " hat kein Ziel");
                return std::nullopt;
            }
            return e;
        }
        if (k != "intervention" && k != "zolleinigung") {
            error(kind->line, "ungültige Art " + quote(k) +
                                  " für 'antrag' (erlaubt: reform, zolleinigung, intervention, praesidium)");
            return std::nullopt;
        }
        if (target == nullptr || !scalar(*target)) {
            error(node.line, "Antrag " + k + " braucht ein Ziel (ziel = …)");
            return std::nullopt;
        }
        if (k == "intervention") {
            e.a = static_cast<Index>(MotionKind::Intervention);
            bool is_absent = false;
            const auto state = state_ref(*target, target->value, is_absent);
            if (!state) {
                return std::nullopt;
            }
            skip = is_absent;
            e.b = *state;
            return e;
        }
        e.a = static_cast<Index>(MotionKind::CustomsUnification);
        e.b = world_.find_customs_union(target->value);
        if (e.b == kNone) {
            if (absent("zollunion", target->value)) {
                skip = true;
                return e;
            }
            error(target->line, "unbekannte Zollunion " + quote(target->value) + " in 'antrag'");
            return std::nullopt;
        }
        return e;
    }

    /// Erster Durchgang: Journale mit Kennung und Stufen, Abstimmungen mit Kennung.
    void register_journals(const Source& source) {
        file_ = source.name;
        for (const ScriptNode& node : source.root.children) {
            const bool journal = node.key == "journal";
            if (!journal && node.key != "abstimmung") {
                warning(node.line, "unbekannter Eintrag " + quote(node.key.empty() ? node.value : node.key) +
                                       " wird ignoriert (erwartet: journal <kennung> { … } oder abstimmung "
                                       "<kennung> { … })");
                continue;
            }
            if (!node.is_block || !is_lower_id(node.value, false)) {
                error(node.line, "ungültige Definition, erwartet: " + node.key +
                                     " <kennung> { … } (Kennung aus a-z, 0-9 und _)");
                continue;
            }
            if (journal) {
                if (world_.find_journal(node.value) != kNone) {
                    error(node.line, "doppelte Kennung " + quote(node.value) + " für Journal");
                    continue;
                }
                JournalDef j;
                j.id = node.value;
                j.file = source.name;
                j.line = node.line;
                for (const ScriptNode& child : node.children) {
                    if ((child.key == "stufe" || child.key == "ausgang") && child.is_block &&
                        is_lower_id(child.value, false)) {
                        JournalStage stage;
                        stage.id = child.value;
                        stage.outcome = child.key == "ausgang";
                        j.stages.push_back(std::move(stage));
                    }
                }
                world_.journals.push_back(std::move(j));
                journal_nodes_.emplace_back(&source, &node);
            } else {
                if (world_.find_vote(node.value) != kNone) {
                    error(node.line, "doppelte Kennung " + quote(node.value) + " für Abstimmung");
                    continue;
                }
                VoteDef v;
                v.id = node.value;
                world_.votes.push_back(std::move(v));
                vote_nodes_.emplace_back(&source, &node);
            }
        }
    }

    void load_journals_and_votes() {
        for (std::size_t i = 0; i < journal_nodes_.size(); ++i) {
            file_ = journal_nodes_[i].first->name;
            load_journal(*journal_nodes_[i].second, world_.journals[i]);
        }
        for (std::size_t i = 0; i < vote_nodes_.size(); ++i) {
            file_ = vote_nodes_[i].first->name;
            load_vote(*vote_nodes_[i].second, world_.votes[i]);
        }
    }

    void load_journal(const ScriptNode& node, JournalDef& j) {
        const std::string what = "Journal " + j.id;
        std::map<std::string, int, std::less<>> seen;
        std::map<std::string, int, std::less<>> stage_lines;
        std::size_t stage = 0;
        for (const ScriptNode& child : node.children) {
            if (child.key == "stufe" || child.key == "ausgang") {
                if (!child.is_block || !is_lower_id(child.value, false)) {
                    error(child.line, "ungültige " + std::string(child.key == "stufe" ? "Stufe" : "Ausgang") +
                                          ", erwartet: " + child.key + " <kennung> { … }");
                    continue;
                }
                const auto [it, inserted] = stage_lines.emplace(child.value, child.line);
                if (!inserted) {
                    error(child.line, "doppelte Stufe " + quote(child.value) + " in " + what +
                                          " (zuerst in Zeile " + std::to_string(it->second) + ")");
                    ++stage;
                    continue;
                }
                load_stage(child, j, j.stages[stage]);
                ++stage;
                continue;
            }
            if (child.key.empty() || !first_time(seen, child, what)) {
                continue;
            }
            if (child.key == "titel") {
                if (scalar(child)) {
                    j.title = text(child).value_or("");
                }
            } else if (child.key == "text") {
                j.text = text(child).value_or("");
            } else if (child.key == "beginn" || child.key == "ende") {
                if (assign(child)) {
                    if (const auto d = date(child)) {
                        (child.key == "beginn" ? j.has_begin : j.has_end) = true;
                        (child.key == "beginn" ? j.begin : j.end) = *d;
                    }
                }
            } else {
                warning(child.line,
                        "unbekanntes Feld " + quote(child.key) + " in " + what + " wird ignoriert");
            }
        }
        if (seen.count("titel") == 0) {
            error(node.line, "Pflichtfeld 'titel' fehlt in " + what);
        }
        if (j.stages.empty() || j.stages[0].outcome) {
            error(node.line, what + " braucht als Erstes eine Stufe (stufe <kennung> { … })");
        }
        if (std::none_of(j.stages.begin(), j.stages.end(), [](const JournalStage& s) { return s.outcome; })) {
            error(node.line, what + " braucht mindestens einen Ausgang (ausgang <kennung> { … })");
        }
        for (std::size_t i = 0; i < j.stages.size(); ++i) {
            if (j.stages[i].fallback) {
                if (j.fallback != kNone) {
                    error(node.line, what + " hat mehr als einen Ausgang mit standard = ja");
                }
                j.fallback = static_cast<Index>(i);
            }
        }
        if (j.has_end && j.fallback == kNone) {
            error(node.line, what + " hat ein Ende, aber keinen Ausgang mit standard = ja");
        }
        if (j.has_begin && j.has_end && j.end <= j.begin) {
            error(node.line, "'ende' muss nach 'beginn' liegen in " + what);
        }
    }

    void load_stage(const ScriptNode& node, JournalDef& j, JournalStage& stage) {
        const std::string what = (stage.outcome ? "Ausgang " : "Stufe ") + stage.id + " von " + j.id;
        const auto journal = static_cast<Index>(world_.find_journal(j.id));
        std::map<std::string, int, std::less<>> seen;
        for (const ScriptNode& child : node.children) {
            if (child.key == "ziel" && !stage.outcome) {
                read_goal(child, stage);
                continue;
            }
            if (child.key == "weiter" && !stage.outcome) {
                read_transition(child, journal, stage);
                continue;
            }
            if (child.key.empty() || !first_time(seen, child, what)) {
                continue;
            }
            if (child.key == "titel") {
                if (scalar(child)) {
                    stage.title = text(child).value_or("");
                }
            } else if (child.key == "text") {
                stage.text = text(child).value_or("");
            } else if (child.key == "wirkung") {
                if (!child.is_block) {
                    error(child.line, "'wirkung' erwartet einen Block in geschweiften Klammern");
                    continue;
                }
                stage.effects = effects(child, false);
            } else if (child.key == "wertung" && stage.outcome) {
                if (scalar(child) && assign(child)) {
                    if (child.value != "gelungen" && child.value != "teilweise" &&
                        child.value != "gescheitert") {
                        error(child.line, "ungültige Wertung " + quote(child.value) +
                                              " (erlaubt: gelungen, teilweise, gescheitert)");
                    } else {
                        stage.rating = child.value;
                    }
                }
            } else if (child.key == "standard" && stage.outcome) {
                stage.fallback = boolean(child).value_or(false);
            } else {
                warning(child.line,
                        "unbekanntes Feld " + quote(child.key) + " in " + what + " wird ignoriert");
            }
        }
        if (seen.count("titel") == 0) {
            error(node.line, "Pflichtfeld 'titel' fehlt in " + what);
        }
        if (stage.outcome && seen.count("wertung") == 0) {
            error(node.line, "Pflichtfeld 'wertung' fehlt in " + what);
        }
    }

    /// ziel { text = "…" bedingung { … } } oder ziel { text = "…" zaehle { … } soll = n }.
    void read_goal(const ScriptNode& node, JournalStage& stage) {
        if (!node.is_block || !node.value.empty()) {
            error(node.line, "'ziel' erwartet einen Block: ziel { text = \"…\" bedingung { … } }");
            return;
        }
        JournalGoal goal;
        bool has_text = false;
        bool has_target = false;
        bool has_condition = false;
        for (const ScriptNode& child : node.children) {
            if (child.key == "text") {
                has_text = true;
                goal.text = text(child).value_or("");
            } else if (child.key == "bedingung") {
                has_condition = true;
                goal.condition = condition_block(child, false);
            } else if (child.key == "zaehle") {
                goal.counting = true;
                goal.count = condition_block(child, true);
            } else if (child.key == "soll") {
                has_target = true;
                goal.target = static_cast<std::int32_t>(integer(child, 1, 1000).value_or(1));
            } else {
                error(child.line, "in 'ziel' sind nur text, bedingung, zaehle und soll erlaubt");
            }
        }
        if (!has_text) {
            error(node.line, "Pflichtfeld 'text' fehlt in ziel");
        }
        if (goal.counting != has_target || goal.counting == has_condition) {
            error(node.line, "'ziel' braucht entweder bedingung oder zaehle mit soll");
        }
        stage.goals.push_back(std::move(goal));
    }

    /// weiter { zu = <stufe|ausgang> wenn { … } }: ohne wenn sofort.
    void read_transition(const ScriptNode& node, Index journal, JournalStage& stage) {
        if (!node.is_block || !node.value.empty()) {
            error(node.line, "'weiter' erwartet einen Block: weiter { zu = … wenn { … } }");
            return;
        }
        JournalTransition t;
        for (const ScriptNode& child : node.children) {
            if (child.key == "zu") {
                if (scalar(child) && assign(child)) {
                    t.target = stage_of(child, journal, child.value);
                }
            } else if (child.key == "wenn") {
                t.condition = condition_block(child, false);
            } else {
                error(child.line, "in 'weiter' sind nur zu und wenn erlaubt");
            }
        }
        if (node.find("zu") == nullptr) {
            error(node.line, "Pflichtfeld 'zu' fehlt in weiter");
        }
        if (t.target != kNone) {
            stage.transitions.push_back(std::move(t));
        }
    }

    void load_vote(const ScriptNode& node, VoteDef& v) {
        const std::string what = "Abstimmung " + v.id;
        std::map<std::string, int, std::less<>> seen;
        const ScriptNode* leaning = nullptr;
        for (const ScriptNode& child : node.children) {
            if (child.key.empty() || !first_time(seen, child, what)) {
                continue;
            }
            if (child.key == "titel") {
                if (scalar(child)) {
                    v.title = text(child).value_or("");
                }
            } else if (child.key == "parlament") {
                if (scalar(child) && assign(child)) {
                    v.parliament = world_.find_parliament(child.value);
                    if (v.parliament == kNone && !absent("parlament", child.value)) {
                        error(child.line, "unbekanntes Parlament " + quote(child.value) + " in " + what);
                    }
                }
            } else if (child.key == "neigung") {
                leaning = &child;
            } else if (child.key == "fraktionslos" || child.key == "mehrheit") {
                if (assign(child)) {
                    if (const auto value = number(child, fx(0), fx(1))) {
                        (child.key == "fraktionslos" ? v.unaligned : v.majority) = *value;
                    }
                }
            } else if (child.key == "ja" || child.key == "nein") {
                if (!child.is_block) {
                    error(child.line, quote(child.key) + " erwartet einen Block mit Wirkungen");
                    continue;
                }
                (child.key == "ja" ? v.yes : v.no) = effects(child, false);
            } else {
                warning(child.line,
                        "unbekanntes Feld " + quote(child.key) + " in " + what + " wird ignoriert");
            }
        }
        for (const char* required : {"titel", "parlament", "neigung"}) {
            if (seen.count(required) == 0) {
                error(node.line, "Pflichtfeld '" + std::string(required) + "' fehlt in " + what);
            }
        }
        if (leaning == nullptr || v.parliament == kNone) {
            return;
        }
        // neigung = { <fraktion> = Anteil der Ja-Stimmen … }; nicht genannte Fraktionen wie fraktionslos
        const Parliament& p = world_.parliaments[static_cast<std::size_t>(v.parliament)];
        v.leaning.assign(p.factions.size(), v.unaligned);
        if (!leaning->is_block) {
            error(leaning->line, "'neigung' erwartet einen Block { <fraktion> = Anteil … }");
            return;
        }
        for (const ScriptNode& entry : leaning->children) {
            const auto it = std::find_if(p.factions.begin(), p.factions.end(),
                                         [&](const Faction& f) { return f.id == entry.key; });
            if (it == p.factions.end()) {
                error(entry.line, "unbekannte Fraktion " + quote(entry.key) + " des Parlaments " + p.id);
                continue;
            }
            if (const auto value = number(entry, fx(0), fx(1))) {
                v.leaning[static_cast<std::size_t>(it - p.factions.begin())] = *value;
            }
        }
    }

    // ── Kreise aus Folgeereignissen ohne Frist (Abschnitt 3.8) ──

    void check_follow_cycles() {
        const std::size_t count = world_.events.size();
        std::vector<std::vector<const FollowEdge*>> out(count);
        for (const FollowEdge& edge : follows_) {
            out[static_cast<std::size_t>(edge.from)].push_back(&edge);
        }
        // 0 = unbesucht, 1 = auf dem Pfad, 2 = fertig
        std::vector<std::uint8_t> mark(count, 0);
        std::vector<Index> path;
        std::function<void(Index)> visit = [&](Index node) {
            mark[static_cast<std::size_t>(node)] = 1;
            path.push_back(node);
            for (const FollowEdge* edge : out[static_cast<std::size_t>(node)]) {
                const auto to = static_cast<std::size_t>(edge->to);
                if (mark[to] == 1) {
                    std::string cycle;
                    const auto start = std::find(path.begin(), path.end(), edge->to);
                    for (auto it = start; it != path.end(); ++it) {
                        cycle += world_.events[static_cast<std::size_t>(*it)].id + " → ";
                    }
                    cycle += world_.events[to].id;
                    const EventDef& from = world_.events[static_cast<std::size_t>(edge->from)];
                    error(from.file, edge->line,
                          "Folgeereignisse ohne Frist (tage = 0) bilden einen Kreis: " + cycle);
                } else if (mark[to] == 0) {
                    visit(edge->to);
                }
            }
            path.pop_back();
            mark[static_cast<std::size_t>(node)] = 2;
        };
        for (std::size_t i = 0; i < count; ++i) {
            if (mark[i] == 0) {
                visit(static_cast<Index>(i));
            }
        }
    }

    fs::path data_dir_;
    Date start_;
    const AbsentContent& absent_;
    World& world_;
    std::vector<LoadIssue>& issues_;
    std::size_t first_issue_ = 0;
    std::string file_;
    Index current_ = kNone;
    std::vector<Source> sources_;
    std::vector<EventEntry> entries_;
    std::map<std::string, Index, std::less<>> index_;
    std::vector<FollowEdge> follows_;
    std::string rules_file_;
    std::vector<Source> journal_sources_;
    std::vector<std::pair<const Source*, const ScriptNode*>> journal_nodes_; // je Journal
    std::vector<std::pair<const Source*, const ScriptNode*>> vote_nodes_;    // je Abstimmung
    std::array<std::pair<std::string, int>, 3> outcome_names_{}; // folge_nachgeben … (Kennung, Zeile)
};

} // namespace

void load_event_content(const std::filesystem::path& data_dir, Date start, const AbsentContent& absent,
                        World& world, std::vector<LoadIssue>& issues) {
    EventLoader(data_dir, start, absent, world, issues).run();
}

} // namespace uhrwerk::detail
