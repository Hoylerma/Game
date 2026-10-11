// uhrwerk-cli – Kommandozeile des Simulationskerns Uhrwerk.
//
// Befehle: pruefen, info, lauf, determinismus (Hilfe: uhrwerk-cli --hilfe).
// Exitcodes: 0 in Ordnung, 1 Daten- oder Determinismusfehler, 2 Bedienfehler.

#include "uhrwerk/date.hpp"
#include "uhrwerk/fixed.hpp"
#include "uhrwerk/loader.hpp"
#include "uhrwerk/script.hpp"
#include "uhrwerk/simulation.hpp"
#include "uhrwerk/world.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using uhrwerk::Date;
using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::World;

constexpr int kExitOk = 0;
constexpr int kExitDataError = 1;
constexpr int kExitUsageError = 2;

constexpr std::int64_t kDefaultYears = 10;
constexpr std::int64_t kMaxYears = 2000;
constexpr std::int64_t kMaxDays = 1'000'000;

constexpr std::string_view kHelp = R"(uhrwerk-cli – Kommandozeile des Simulationskerns Uhrwerk (EINIGKEIT)

Aufruf:
  uhrwerk-cli <befehl> [optionen]

Befehle:
  pruefen        Spieldaten laden und alle Fehler und Warnungen ausgeben
  info           Überblick über Szenario, Staaten, Orte (mit und ohne Daten) und Kreise
  lauf           Simulation laufen lassen und Ergebnis ausgeben (mit Lade- und Laufzeit)
  determinismus  Zwei Läufe mit gleichem Seed vergleichen (Prüfsumme nach jedem Jahr)
                 und einen Lauf mit Seed+1 (Abweichung erwartet)

Optionen:
  --daten DIR    Datenordner. Standard: Umgebungsvariable UHRWERK_DATEN, sonst Suche
                 nach data/ ab dem Arbeitsverzeichnis und ab dem Ordner des Programms
  --szenario ID  Szenario (Standard: westfaelischer_frieden)
  --jahre N      Zu simulierende Jahre (Standard: 10, wenn auch --tage fehlt)
  --tage N       Zusätzlich zu simulierende Tage
  --seed N       Seed der Partie (Standard: 1648)
  --bericht      Tabelle je Staat am Ende des Laufs (nur lauf)
  --chronik      Chronik der Partie ausgeben (nur lauf)
  --hilfe, -h    Diese Hilfe anzeigen

Exitcodes:
  0  in Ordnung
  1  Datenfehler oder Determinismusfehler
  2  Bedienfehler (unbekannter Befehl, ungültige Option)

Beispiele:
  uhrwerk-cli pruefen
  uhrwerk-cli info --szenario westfaelischer_frieden
  uhrwerk-cli lauf --jahre 50 --bericht --chronik
  uhrwerk-cli determinismus --jahre 100
)";

// ─── Optionen ───────────────────────────────────────────────────────────────

struct Options {
    std::string command;
    std::optional<fs::path> data_dir;
    std::string scenario{uhrwerk::kDefaultScenario};
    std::optional<std::int64_t> years;
    std::optional<std::int64_t> days;
    std::uint64_t seed = uhrwerk::SimConfig{}.seed;
    bool report = false;
    bool chronicle = false;
    bool help = false;
    std::set<std::string> given; // genannte Optionen, z. B. "--bericht"
};

struct UsageError {
    std::string message;
};

std::optional<std::uint64_t> parse_unsigned(std::string_view text) {
    if (text.empty() || text.size() > 20) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (value > (UINT64_MAX - digit) / 10) {
            return std::nullopt;
        }
        value = value * 10 + digit;
    }
    return value;
}

std::int64_t parse_count(std::string_view option, std::string_view text, std::int64_t max) {
    const auto value = parse_unsigned(text);
    if (!value || *value > static_cast<std::uint64_t>(max)) {
        throw UsageError{"Ungültiger Wert für " + std::string(option) + ": '" + std::string(text) +
                         "' (erwartet eine Ganzzahl von 0 bis " + std::to_string(max) + ")"};
    }
    return static_cast<std::int64_t>(*value);
}

Options parse_options(int argc, char** argv) {
    Options o;
    const std::set<std::string_view> with_value = {"--daten", "--szenario", "--jahre", "--tage", "--seed"};
    const std::set<std::string_view> flags = {"--bericht", "--chronik"};

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--hilfe" || arg == "-h" || arg == "--help") {
            o.help = true;
            continue;
        }
        if (arg.size() > 1 && arg[0] == '-') {
            std::string name(arg);
            std::optional<std::string> value;
            if (const auto eq = name.find('='); eq != std::string::npos) {
                value = name.substr(eq + 1);
                name.resize(eq);
            }
            if (flags.count(name) != 0) {
                if (value) {
                    throw UsageError{"Option " + name + " erwartet keinen Wert"};
                }
                (name == "--bericht" ? o.report : o.chronicle) = true;
            } else if (with_value.count(name) != 0) {
                if (!value) {
                    if (i + 1 >= argc) {
                        throw UsageError{"Option " + name + " erwartet einen Wert"};
                    }
                    value = argv[++i];
                }
                if (name == "--daten") {
                    if (value->empty()) {
                        throw UsageError{"Option --daten erwartet einen Ordner"};
                    }
                    o.data_dir = fs::path(*value);
                } else if (name == "--szenario") {
                    if (value->empty()) {
                        throw UsageError{"Option --szenario erwartet eine Kennung"};
                    }
                    o.scenario = *value;
                } else if (name == "--jahre") {
                    o.years = parse_count(name, *value, kMaxYears);
                } else if (name == "--tage") {
                    o.days = parse_count(name, *value, kMaxDays);
                } else {
                    const auto seed = parse_unsigned(*value);
                    if (!seed) {
                        throw UsageError{"Ungültiger Wert für --seed: '" + *value +
                                         "' (erwartet eine nicht negative Ganzzahl)"};
                    }
                    o.seed = *seed;
                }
            } else {
                throw UsageError{"Unbekannte Option " + name};
            }
            o.given.insert(name);
            continue;
        }
        if (o.command.empty()) {
            o.command = std::string(arg);
        } else {
            throw UsageError{"Unerwartetes Argument '" + std::string(arg) + "'"};
        }
    }
    return o;
}

void check_applicable(const Options& o, const std::set<std::string_view>& allowed) {
    for (const std::string& name : o.given) {
        if (allowed.count(name) == 0) {
            throw UsageError{"Option " + name + " ist für '" + o.command + "' nicht verfügbar"};
        }
    }
}

// ─── Formatierung (deutsch) ─────────────────────────────────────────────────

std::string group_digits(const std::string& digits) {
    std::string result;
    const std::size_t n = digits.size();
    for (std::size_t i = 0; i < n; ++i) {
        result += digits[i];
        const std::size_t rest = n - i - 1;
        if (rest > 0 && rest % 3 == 0) {
            result += '.';
        }
    }
    return result;
}

std::string format_int(std::int64_t value) {
    const std::uint64_t magnitude =
        value < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
    return (value < 0 ? "-" : "") + group_digits(std::to_string(magnitude));
}

/// Festkommazahl mit Tausenderpunkten und Dezimalkomma, z. B. "21.345,12".
std::string format_fixed(Fixed value, int decimals) {
    std::string text = value.to_string(decimals);
    std::string sign;
    if (!text.empty() && text[0] == '-') {
        sign = "-";
        text.erase(0, 1);
    }
    const std::size_t dot = text.find('.');
    const std::string int_part = text.substr(0, dot);
    std::string result = sign + group_digits(int_part);
    if (dot != std::string::npos) {
        result += ',' + text.substr(dot + 1);
    }
    return result;
}

std::string format_percent(Fixed rate) {
    return std::to_string((rate * Fixed::from_int(100)).round()) + " %";
}

std::string format_seconds(double seconds) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.3f", seconds);
    std::string text = buffer;
    std::replace(text.begin(), text.end(), '.', ',');
    return text + " s";
}

std::string hex16(std::uint64_t value) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string text(16, '0');
    for (int i = 15; i >= 0; --i) {
        text[static_cast<std::size_t>(i)] = digits[value & 0xFu];
        value >>= 4;
    }
    return text;
}

/// Anzeigebreite eines UTF-8-Textes (Anzahl Codepunkte).
std::size_t display_width(std::string_view text) {
    return static_cast<std::size_t>(std::count_if(
        text.begin(), text.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

std::string pad(const std::string& text, std::size_t width, bool right_align) {
    const std::size_t w = display_width(text);
    if (w >= width) {
        return text;
    }
    const std::string fill(width - w, ' ');
    return right_align ? fill + text : text + fill;
}

void print_table(const std::vector<std::vector<std::string>>& rows, const std::vector<bool>& right_align,
                 std::size_t separator_before_last) {
    std::vector<std::size_t> widths(right_align.size(), 0);
    for (const auto& row : rows) {
        for (std::size_t c = 0; c < row.size() && c < widths.size(); ++c) {
            widths[c] = std::max(widths[c], display_width(row[c]));
        }
    }
    std::size_t total = 0;
    for (const std::size_t w : widths) {
        total += w;
    }
    total += 2 * (widths.size() - 1);
    const std::string rule(total, '-');

    for (std::size_t r = 0; r < rows.size(); ++r) {
        if (r == 1 || (separator_before_last > 0 && r + separator_before_last == rows.size())) {
            std::cout << rule << '\n';
        }
        std::string line;
        for (std::size_t c = 0; c < rows[r].size() && c < widths.size(); ++c) {
            if (c > 0) {
                line += "  ";
            }
            line += pad(rows[r][c], widths[c], right_align[c]);
        }
        while (!line.empty() && line.back() == ' ') {
            line.pop_back();
        }
        std::cout << line << '\n';
    }
}

std::string plural(std::size_t n, std::string_view one, std::string_view many) {
    return group_digits(std::to_string(n)) + " " + std::string(n == 1 ? one : many);
}

/// "(5 mit Daten, 1 ohne Daten)"
std::string location_data_split(const World& w) {
    const std::size_t with_data = w.locations_with_data();
    return "(" + group_digits(std::to_string(with_data)) + " mit Daten, " +
           group_digits(std::to_string(w.locations.size() - with_data)) + " ohne Daten)";
}

/// "6 Orte (5 mit Daten, 1 ohne Daten)"
std::string describe_locations(const World& w) {
    return plural(w.locations.size(), "Ort", "Orte") + " " + location_data_split(w);
}

/// "(5 Orte mit Kreis, 1 ohne Kreis)"
std::string location_district_split(const World& w) {
    const auto without = static_cast<std::size_t>(std::count_if(
        w.locations.begin(), w.locations.end(), [](const uhrwerk::Location& l) { return l.district < 0; }));
    return "(" + plural(w.locations.size() - without, "Ort", "Orte") + " mit Kreis, " +
           group_digits(std::to_string(without)) + " ohne Kreis)";
}

// ─── Daten laden ────────────────────────────────────────────────────────────

fs::path executable_dir(const char* argv0) {
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (ec || exe.empty()) {
        exe = fs::absolute(fs::path(argv0 != nullptr ? argv0 : ""), ec);
    }
    return exe.parent_path();
}

fs::path resolve_data_dir(const Options& o, const char* argv0) {
    if (o.data_dir) {
        return *o.data_dir;
    }
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (!ec) {
        if (auto dir = uhrwerk::find_data_dir(cwd)) {
            return *dir;
        }
    }
    if (auto dir = uhrwerk::find_data_dir(executable_dir(argv0))) {
        return *dir;
    }
    return {};
}

/// Lädt die Welt; gibt Probleme auf stderr aus (Warnungen nur, wenn show_warnings).
std::optional<World> load(const fs::path& data_dir, const std::string& scenario, bool show_warnings) {
    if (data_dir.empty()) {
        std::cerr << "Fehler: Datenordner nicht gefunden. Mit --daten DIR angeben oder die "
                     "Umgebungsvariable UHRWERK_DATEN setzen.\n";
        return std::nullopt;
    }
    uhrwerk::LoadResult result = uhrwerk::load_world(data_dir, scenario);
    std::size_t errors = 0;
    for (const uhrwerk::LoadIssue& issue : result.issues) {
        if (issue.fatal) {
            ++errors;
        }
        if (issue.fatal || show_warnings) {
            std::cerr << issue.to_string() << '\n';
        }
    }
    if (!result.ok()) {
        std::cerr << "Laden fehlgeschlagen (" << plural(errors, "Fehler", "Fehler")
                  << ") – Details: uhrwerk-cli pruefen\n";
        return std::nullopt;
    }
    return std::move(result.world);
}

Date add_years(Date date, std::int64_t years) {
    date.year = static_cast<std::int32_t>(date.year + years);
    if (date.month == 2 && date.day == 29 && !uhrwerk::is_leap_year(date.year)) {
        date.day = 28;
    }
    return date;
}

std::int64_t days_between(const Date& from, const Date& to) {
    return uhrwerk::to_day_number(to) - uhrwerk::to_day_number(from);
}

struct Period {
    std::int64_t years = 0;
    std::int64_t extra_days = 0;
};

Period period_from(const Options& o, const Date& start) {
    Period p;
    if (!o.years && !o.days) {
        p.years = kDefaultYears;
    } else {
        p.years = o.years.value_or(0);
        p.extra_days = o.days.value_or(0);
    }
    const Date end = add_years(start, p.years);
    if (!uhrwerk::is_valid(end) ||
        !uhrwerk::is_valid(uhrwerk::from_day_number(uhrwerk::to_day_number(end) + p.extra_days))) {
        throw UsageError{"Der Zeitraum reicht über das Jahr 9999 hinaus"};
    }
    return p;
}

std::string describe_state(const World& world, Index index) {
    if (index < 0 || static_cast<std::size_t>(index) >= world.states.size()) {
        return "–";
    }
    const uhrwerk::State& s = world.states[static_cast<std::size_t>(index)];
    return s.name + " (" + s.tag + ")";
}

// ─── Befehle ────────────────────────────────────────────────────────────────

int cmd_pruefen(const Options& o, const fs::path& data_dir) {
    if (data_dir.empty()) {
        load(data_dir, o.scenario, true);
        return kExitDataError;
    }
    std::cout << "Prüfe Daten in " << uhrwerk::path_to_utf8(data_dir) << " (Szenario " << o.scenario
              << ") …\n";
    const uhrwerk::LoadResult result = uhrwerk::load_world(data_dir, o.scenario);
    std::size_t errors = 0;
    std::size_t warnings = 0;
    for (const uhrwerk::LoadIssue& issue : result.issues) {
        (issue.fatal ? errors : warnings) += 1;
        std::cout << issue.to_string() << '\n';
    }
    std::cout << "Ergebnis: " << plural(errors, "Fehler", "Fehler") << ", "
              << plural(warnings, "Warnung", "Warnungen") << ".\n";
    if (!result.ok()) {
        return kExitDataError;
    }
    const World& w = *result.world;
    std::cout << "Geladen: " << plural(w.terrains.size(), "Geländetyp", "Geländetypen") << ", "
              << plural(w.confessions.size(), "Konfession", "Konfessionen") << ", "
              << plural(w.cultures.size(), "Kultur", "Kulturen") << ", "
              << plural(w.professions.size(), "Beruf", "Berufe") << ", "
              << plural(w.states.size(), "Staat", "Staaten") << ", "
              << plural(w.districts.size(), "Kreis", "Kreise") << ", " << describe_locations(w) << ", "
              << plural(w.pops.size(), "Bevölkerungsblock", "Bevölkerungsblöcke") << ".\n";
    return kExitOk;
}

int cmd_info(const Options& o, const fs::path& data_dir) {
    const auto world = load(data_dir, o.scenario, true);
    if (!world) {
        return kExitDataError;
    }
    const World& w = *world;
    Fixed total;
    for (std::size_t i = 0; i < w.states.size(); ++i) {
        total += w.state_population(static_cast<Index>(i));
    }
    std::cout << "Datenordner:  " << uhrwerk::path_to_utf8(data_dir) << '\n'
              << "Szenario:     " << w.scenario.name << " (" << w.scenario.id << ")\n"
              << "Startdatum:   " << uhrwerk::format_date_long(w.date) << '\n'
              << "Spieler:      " << describe_state(w, w.find_state(w.scenario.player_tag)) << '\n'
              << "Inhalt:       " << plural(w.terrains.size(), "Geländetyp", "Geländetypen") << ", "
              << plural(w.confessions.size(), "Konfession", "Konfessionen") << ", "
              << plural(w.cultures.size(), "Kultur", "Kulturen") << ", "
              << plural(w.professions.size(), "Beruf", "Berufe") << '\n'
              << "              " << plural(w.states.size(), "Staat", "Staaten") << ", "
              << plural(w.pops.size(), "Bevölkerungsblock", "Bevölkerungsblöcke") << '\n'
              << "Orte:         " << group_digits(std::to_string(w.locations.size())) << " "
              << location_data_split(w) << '\n'
              << "Kreise:       " << group_digits(std::to_string(w.districts.size())) << " "
              << location_district_split(w) << '\n'
              << "Bevölkerung:  " << format_int(total.round()) << "\n\n";

    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Tag", "Staat", "Hauptstadt", "Orte", "Bevölkerung", "Schatz", "Steuersatz"});
    for (std::size_t i = 0; i < w.states.size(); ++i) {
        const uhrwerk::State& s = w.states[i];
        const auto index = static_cast<Index>(i);
        const std::string capital =
            s.capital >= 0 ? w.locations[static_cast<std::size_t>(s.capital)].name : std::string("–");
        rows.push_back({s.tag, s.name, capital, std::to_string(w.state_locations(index).size()),
                        format_int(w.state_population(index).round()), format_fixed(s.treasury, 0),
                        format_percent(s.tax_rate)});
    }
    print_table(rows, {false, false, false, true, true, true, true}, 0);
    return kExitOk;
}

void print_report(const uhrwerk::Simulation& sim) {
    const World& w = sim.world();
    std::cout << "\nBericht zum " << uhrwerk::format_date_long(w.date) << " (Beträge in Reichstalern)\n";
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Staat", "Bevölkerung", "Schatz", "Einnahmen/Monat", "Ausgaben/Monat", "Steuersatz"});
    Fixed population;
    Fixed treasury;
    Fixed income;
    Fixed expenses;
    for (std::size_t i = 0; i < w.states.size(); ++i) {
        const uhrwerk::State& s = w.states[i];
        const Fixed pop = w.state_population(static_cast<Index>(i));
        population += pop;
        treasury += s.treasury;
        income += s.last_month_income;
        expenses += s.last_month_expenses;
        rows.push_back({s.short_name + " (" + s.tag + ")", format_int(pop.round()),
                        format_fixed(s.treasury, 2), format_fixed(s.last_month_income, 2),
                        format_fixed(s.last_month_expenses, 2), format_percent(s.tax_rate)});
    }
    rows.push_back({"Gesamt", format_int(population.round()), format_fixed(treasury, 2),
                    format_fixed(income, 2), format_fixed(expenses, 2), ""});
    print_table(rows, {false, true, true, true, true, true}, 1);
}

void print_chronicle(const World& w) {
    std::cout << "\nChronik (" << plural(w.chronicle.size(), "Eintrag", "Einträge") << ")\n";
    for (const uhrwerk::ChronicleEntry& e : w.chronicle) {
        std::cout << uhrwerk::format_date(e.date) << "  " << e.text << '\n';
    }
}

int cmd_lauf(const Options& o, const fs::path& data_dir) {
    const auto load_start = std::chrono::steady_clock::now();
    auto world = load(data_dir, o.scenario, true);
    if (!world) {
        return kExitDataError;
    }
    const double load_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - load_start).count();
    const std::string content =
        plural(world->states.size(), "Staat", "Staaten") + ", " + describe_locations(*world);
    const Date start = world->date;
    const Period period = period_from(o, start);
    const Date end_of_years = add_years(start, period.years);
    const std::int64_t days = days_between(start, end_of_years) + period.extra_days;
    const std::string scenario_name = world->scenario.name + " (" + world->scenario.id + ")";

    uhrwerk::Simulation sim(std::move(*world), uhrwerk::SimConfig{o.seed});
    const auto t0 = std::chrono::steady_clock::now();
    sim.step_days(days);
    const auto t1 = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "Szenario:  " << scenario_name << '\n'
              << "Inhalt:    " << content << '\n'
              << "Laden:     " << format_seconds(load_seconds) << '\n'
              << "Seed:      " << o.seed << '\n'
              << "Start:     " << uhrwerk::format_date_long(start) << '\n'
              << "Ende:      " << uhrwerk::format_date_long(sim.world().date) << " (" << format_int(days)
              << " Tage)\n"
              << "Laufzeit:  " << format_seconds(seconds);
    if (seconds > 0.0 && days > 0) {
        std::cout << " (" << format_int(static_cast<std::int64_t>(static_cast<double>(days) / seconds))
                  << " Tage/s)";
    }
    std::cout << '\n';

    if (o.report) {
        print_report(sim);
    }
    if (o.chronicle) {
        print_chronicle(sim.world());
    }
    std::cout << "\nPrüfsumme: " << hex16(sim.checksum()) << '\n';
    return kExitOk;
}

int cmd_determinismus(const Options& o, const fs::path& data_dir) {
    // Jeder Lauf lädt die Daten neu – so fällt auch ein nicht deterministischer Lader auf.
    auto world_a = load(data_dir, o.scenario, true);
    if (!world_a) {
        return kExitDataError;
    }
    auto world_b = load(data_dir, o.scenario, false);
    auto world_c = load(data_dir, o.scenario, false);
    if (!world_b || !world_c) {
        return kExitDataError;
    }

    const Date start = world_a->date;
    const Period period = period_from(o, start);
    const std::uint64_t other_seed = o.seed + 1;
    const std::string scenario_name = world_a->scenario.name + " (" + world_a->scenario.id + ")";

    uhrwerk::Simulation a(std::move(*world_a), uhrwerk::SimConfig{o.seed});
    uhrwerk::Simulation b(std::move(*world_b), uhrwerk::SimConfig{o.seed});
    uhrwerk::Simulation c(std::move(*world_c), uhrwerk::SimConfig{other_seed});

    std::cout << "Determinismusprüfung\n"
              << "Szenario:  " << scenario_name << '\n'
              << "Zeitraum:  " << plural(static_cast<std::size_t>(period.years), "Jahr", "Jahre");
    if (period.extra_days > 0) {
        std::cout << " und " << plural(static_cast<std::size_t>(period.extra_days), "Tag", "Tage");
    }
    std::cout << " ab " << uhrwerk::format_date_long(start) << '\n'
              << "Seeds:     Lauf A und B " << o.seed << ", Lauf C " << other_seed << '\n';

    const auto t0 = std::chrono::steady_clock::now();
    const auto mismatch = [&](const std::string& when) {
        std::cout << "FEHLER: Lauf A und B weichen ab " << when << " ("
                  << uhrwerk::format_date(a.world().date) << "): A " << hex16(a.checksum()) << ", B "
                  << hex16(b.checksum()) << '\n'
                  << "Ergebnis: NICHT deterministisch.\n";
        return kExitDataError;
    };

    if (a.checksum() != b.checksum()) {
        return mismatch("im Startzustand");
    }
    for (std::int64_t year = 1; year <= period.years; ++year) {
        const std::int64_t steps = days_between(a.world().date, add_years(start, year));
        a.step_days(steps);
        b.step_days(steps);
        c.step_days(steps);
        if (a.checksum() != b.checksum()) {
            return mismatch("im Jahr " + std::to_string(year));
        }
    }
    if (period.extra_days > 0) {
        a.step_days(period.extra_days);
        b.step_days(period.extra_days);
        c.step_days(period.extra_days);
        if (a.checksum() != b.checksum()) {
            return mismatch("nach den zusätzlichen Tagen");
        }
    }
    const auto t1 = std::chrono::steady_clock::now();

    std::cout << "Lauf A/B:  Startzustand und "
              << plural(static_cast<std::size_t>(period.years), "Jahresprüfsumme", "Jahresprüfsummen")
              << (period.extra_days > 0 ? " sowie Endstand" : "") << " identisch\n";
    if (c.checksum() != a.checksum()) {
        std::cout << "Lauf C:    weicht wie erwartet ab (" << hex16(c.checksum()) << ")\n";
    } else {
        std::cout
            << "Lauf C:    Warnung: gleiche Prüfsumme trotz anderem Seed – wirkt sich der Seed nicht aus?\n";
        std::cerr << "Warnung: Lauf C mit Seed " << other_seed << " liefert dieselbe Prüfsumme wie Seed "
                  << o.seed << ".\n";
    }
    std::cout << "Enddatum:  " << uhrwerk::format_date_long(a.world().date) << '\n'
              << "Laufzeit:  " << format_seconds(std::chrono::duration<double>(t1 - t0).count()) << '\n'
              << "Ergebnis:  deterministisch\n"
              << "\nPrüfsumme: " << hex16(a.checksum()) << '\n';
    return kExitOk;
}

int run(int argc, char** argv) {
    Options o;
    try {
        o = parse_options(argc, argv);
        if (o.help || o.command == "hilfe") {
            std::cout << kHelp;
            return kExitOk;
        }
        if (o.command.empty()) {
            throw UsageError{"Kein Befehl angegeben"};
        }
        if (o.command == "pruefen" || o.command == "info") {
            check_applicable(o, {"--daten", "--szenario"});
        } else if (o.command == "lauf") {
            check_applicable(
                o, {"--daten", "--szenario", "--jahre", "--tage", "--seed", "--bericht", "--chronik"});
        } else if (o.command == "determinismus") {
            check_applicable(o, {"--daten", "--szenario", "--jahre", "--tage", "--seed"});
        } else {
            throw UsageError{"Unbekannter Befehl '" + o.command + "'"};
        }

        const fs::path data_dir = resolve_data_dir(o, argc > 0 ? argv[0] : nullptr);
        if (o.command == "pruefen") {
            return cmd_pruefen(o, data_dir);
        }
        if (o.command == "info") {
            return cmd_info(o, data_dir);
        }
        if (o.command == "lauf") {
            return cmd_lauf(o, data_dir);
        }
        return cmd_determinismus(o, data_dir);
    } catch (const UsageError& e) {
        std::cerr << "Fehler: " << e.message << ".\nHilfe: uhrwerk-cli --hilfe\n";
        return kExitUsageError;
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Interner Fehler: " << e.what() << '\n';
        return kExitDataError;
    }
}
