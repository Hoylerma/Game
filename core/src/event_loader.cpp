#include "event_loader.hpp"

#include "uhrwerk/script.hpp"

#include <algorithm>
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
        world_.events.resize(entries_.size());
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            current_ = static_cast<Index>(i);
            file_ = entries_[i].source->name;
            load_event(entries_[i], world_.events[i]);
        }
        check_follow_cycles();
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

    // ── common/ereignisse.chron ──

    void load_rules(const Source& source) {
        file_ = source.name;
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
                } else {
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
        if (key == "im_krieg" || key == "mobil" || key == "bund" || key == "spieler") {
            if (!need_state(node, has_state)) {
                return std::nullopt;
            }
            const auto value = boolean(node);
            if (!value) {
                return std::nullopt;
            }
            c.kind = key == "im_krieg" ? ConditionKind::AtWar
                     : key == "mobil"  ? ConditionKind::Mobilized
                     : key == "bund"   ? ConditionKind::Federal
                                       : ConditionKind::Player;
            if (*value) {
                return c;
            }
            Condition negation;
            negation.kind = ConditionKind::Not;
            negation.children.push_back(std::move(c));
            return negation;
        }
        if (key == "krieg_mit") {
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
            c.kind = ConditionKind::WarWith;
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
        // Ab hier wirkt alles auf einen Staat.
        const bool known = key == "merke" || key == "vergiss" || key == "verfassung" || key == "wahlrecht" ||
                           key == "unruhe" || key == "schatz" || key == "monatseinnahmen" ||
                           key == "auswanderung" || key == "mobilmachung" ||
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
        if (key == "mobilmachung") {
            if (node.value != "ja") {
                error(node.line, "'mobilmachung' erwartet ja");
                return std::nullopt;
            }
            e.kind = EffectKind::Mobilize;
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
        if (e.days == 0 && !skip) {
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
};

} // namespace

void load_event_content(const std::filesystem::path& data_dir, Date start, const AbsentContent& absent,
                        World& world, std::vector<LoadIssue>& issues) {
    EventLoader(data_dir, start, absent, world, issues).run();
}

} // namespace uhrwerk::detail
