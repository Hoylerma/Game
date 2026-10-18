// uhrwerk-cli – Kommandozeile des Simulationskerns Uhrwerk.
//
// Befehle: pruefen, info, lauf, determinismus, lasttest, wirtschaft, bilanz, speichertest, wiederholung,
// spielstand (Hilfe: uhrwerk-cli --hilfe).
// Exitcodes: 0 in Ordnung, 1 Daten-, Determinismus-, Lasttest- oder Spielstandfehler, 2 Bedienfehler.

#include "uhrwerk/balance.hpp"
#include "uhrwerk/date.hpp"
#include "uhrwerk/economy.hpp"
#include "uhrwerk/fixed.hpp"
#include "uhrwerk/lasttest.hpp"
#include "uhrwerk/loader.hpp"
#include "uhrwerk/rng.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/script.hpp"
#include "uhrwerk/simulation.hpp"
#include "uhrwerk/world.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
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
using uhrwerk::hex16;
using uhrwerk::Index;
using uhrwerk::World;

constexpr int kExitOk = 0;
constexpr int kExitDataError = 1;
constexpr int kExitUsageError = 2;

constexpr std::int64_t kDefaultYears = 10;
constexpr std::int64_t kMaxYears = 2000;
constexpr std::int64_t kMaxDays = 1'000'000;
constexpr std::int64_t kMaxThreads = 256;
constexpr std::int64_t kDefaultLoadTestBlocks = 500'000;
constexpr std::int64_t kMaxLoadTestBlocks = 10'000'000;
constexpr std::int64_t kDefaultLoadTestYears = 3;
constexpr std::int64_t kDefaultBalanceSeeds = 32;
constexpr std::int64_t kDefaultBalanceYears = 100;
constexpr std::int64_t kMaxBalanceSeeds = 1000;
constexpr std::int64_t kMaxTestCommands = 1'000'000;
constexpr std::int64_t kDefaultSavePoints = 10;
constexpr std::int64_t kMaxSavePoints = 1000;

constexpr std::string_view kHelp = R"(uhrwerk-cli – Kommandozeile des Simulationskerns Uhrwerk (EINIGKEIT)

Aufruf:
  uhrwerk-cli <befehl> [optionen]

Befehle:
  pruefen        Spieldaten laden und alle Fehler und Warnungen ausgeben
  info           Überblick über Szenario, Staaten, Orte (mit und ohne Daten) und Kreise
  lauf           Simulation laufen lassen und Ergebnis ausgeben (mit Lade- und Laufzeit)
  determinismus  Zwei Läufe mit gleichem Seed vergleichen (Prüfsumme nach jedem Jahr; Lauf B
                 mit einem Thread) und einen Lauf mit Seed+1 (Abweichung erwartet)
  lasttest       Lasttest: jeder Ort mit Daten, viele Bevölkerungsblöcke; Zeit je Spieljahr
  wirtschaft     Wirtschaft im Überblick: Güter, Berufe, Haushalt der Staaten (nach dem
                 Einschwingen oder nach --jahre/--tage); Werkzeug der Kalibrierung
  bilanz         Balancing-Bericht: mehrere Läufe ohne Spielereingriff (Seeds ab --seed);
                 Haushalt, Preise, Bevölkerung und Lebensstandard gegen die Zielwerte
  speichertest   Abnahmetest der Spielstände: Lauf A ohne Unterbrechung; Lauf B speichert an
                 --punkte Stellen in eine Datei, lädt sie mit frisch geladenen Spieldaten und
                 rechnet weiter; alle Jahresprüfsummen müssen gleich sein
  wiederholung   wiederholung DATEI: Protokoll oder Spielstand vom Start nachrechnen und mit
                 den festgehaltenen Prüfsummen vergleichen (Exitcode 1 bei Abweichung)
  spielstand     spielstand DATEI: Angaben zu einem Spielstand oder Protokoll; prüft, ob er
                 sich mit den Spieldaten laden lässt (Exitcode 1, wenn nicht)

Optionen:
  --daten DIR    Datenordner. Standard: Umgebungsvariable UHRWERK_DATEN, sonst Suche
                 nach data/ ab dem Arbeitsverzeichnis und ab dem Ordner des Programms
  --szenario ID  Szenario (Standard: westfaelischer_frieden)
  --jahre N      Zu simulierende Jahre (Standard: 10, wenn auch --tage fehlt; bilanz: 100)
  --tage N       Zusätzlich zu simulierende Tage
  --seed N       Seed der Partie (Standard: 1648)
  --threads N    Rechnende Threads, 0 = alle Kerne (Standard: 0)
  --bloecke N    Lasttest: angestrebte Zahl der Bevölkerungsblöcke (Standard: 500000)
  --vergleich N  Lasttest: zweiter Lauf mit N Threads, Prüfsummen müssen gleich sein
  --grenze S     Lasttest: Exitcode 1, wenn der Median je Spieljahr über S Sekunden liegt
  --bericht      Tabelle je Staat am Ende des Laufs (nur lauf)
  --chronik      Chronik der Partie ausgeben (nur lauf)
  --maerkte      Preise je Markt (nur wirtschaft)
  --seeds N      Bilanz: Zahl der Läufe (Standard: 32)
  --ziele DATEI  Bilanz: Zielwerte (Standard: common/zielwerte_<szenario>.chron, sonst
                 common/zielwerte.chron im Datenordner)
  --speichern F  lauf, lasttest: am Ende einen Spielstand in die Datei F schreiben
  --laden F      lauf: Spielstand F laden und weiterrechnen (Szenario und Seed aus dem
                 Spielstand; --jahre/--tage ab seinem Datum)
  --protokoll F  lauf: Befehle und jährliche Prüfsummen für eine Wiederholung in F schreiben
  --testbefehle N
                 lauf, speichertest: N zufällige Steuerbefehle, auch ungültige (eigener
                 Zufallsstrom; Standard: keine)
  --punkte N     speichertest: Zahl der Speicherpunkte (Standard: 10)
  --text         spielstand: nur den Zustand ausgeben, eine Zeile je Feld (zum Vergleich mit diff)
  --hilfe, -h    Diese Hilfe anzeigen

Exitcodes:
  0  in Ordnung
  1  Datenfehler, Determinismusfehler, verfehlte Zielwerte (bilanz), Spielstand nicht ladbar
     oder Wiederholung weicht ab
  2  Bedienfehler (unbekannter Befehl, ungültige Option)

Beispiele:
  uhrwerk-cli pruefen
  uhrwerk-cli info --szenario westfaelischer_frieden
  uhrwerk-cli lauf --jahre 50 --bericht --chronik
  uhrwerk-cli determinismus --jahre 100
  uhrwerk-cli lasttest --bloecke 500000 --jahre 3 --vergleich 1 --grenze 2
  uhrwerk-cli bilanz --seeds 16 --jahre 100
  uhrwerk-cli lauf --jahre 50 --testbefehle 200 --speichern partie.einigkeit --protokoll partie.protokoll
  uhrwerk-cli lauf --laden partie.einigkeit --jahre 10
  uhrwerk-cli wiederholung partie.protokoll
  uhrwerk-cli spielstand partie.einigkeit
  uhrwerk-cli speichertest --jahre 50 --testbefehle 200 --punkte 10
)";

// ─── Optionen ───────────────────────────────────────────────────────────────

struct Options {
    std::string command;
    std::optional<fs::path> data_dir;
    std::string scenario{uhrwerk::kDefaultScenario};
    std::optional<std::int64_t> years;
    std::optional<std::int64_t> days;
    std::uint64_t seed = uhrwerk::SimConfig{}.seed;
    unsigned threads = 0;
    std::int64_t blocks = kDefaultLoadTestBlocks;
    std::optional<unsigned> compare_threads;
    std::optional<double> limit_seconds;
    std::optional<std::int64_t> seeds;
    std::optional<fs::path> targets_file;
    std::optional<fs::path> file;          // wiederholung, spielstand: Datei als Argument
    std::optional<fs::path> save_file;     // --speichern
    std::optional<fs::path> load_file;     // --laden
    std::optional<fs::path> protocol_file; // --protokoll
    std::int64_t test_commands = 0;
    std::int64_t save_points = kDefaultSavePoints;
    bool report = false;
    bool chronicle = false;
    bool markets = false;
    bool text = false;
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
    const std::set<std::string_view> with_value = {"--daten",  "--szenario",  "--jahre",       "--tage",
                                                   "--seed",   "--threads",   "--bloecke",     "--vergleich",
                                                   "--grenze", "--seeds",     "--ziele",       "--speichern",
                                                   "--laden",  "--protokoll", "--testbefehle", "--punkte"};
    const std::set<std::string_view> flags = {"--bericht", "--chronik", "--maerkte", "--text"};

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
                (name == "--bericht"   ? o.report
                 : name == "--chronik" ? o.chronicle
                 : name == "--text"    ? o.text
                                       : o.markets) = true;
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
                } else if (name == "--ziele") {
                    if (value->empty()) {
                        throw UsageError{"Option --ziele erwartet eine Datei"};
                    }
                    o.targets_file = fs::path(*value);
                } else if (name == "--speichern" || name == "--laden" || name == "--protokoll") {
                    if (value->empty()) {
                        throw UsageError{"Option " + name + " erwartet eine Datei"};
                    }
                    (name == "--speichern" ? o.save_file
                     : name == "--laden"   ? o.load_file
                                           : o.protocol_file) = fs::path(*value);
                } else if (name == "--testbefehle") {
                    o.test_commands = parse_count(name, *value, kMaxTestCommands);
                } else if (name == "--punkte") {
                    o.save_points = parse_count(name, *value, kMaxSavePoints);
                    if (o.save_points == 0) {
                        throw UsageError{"Ungültiger Wert für --punkte: '0' (mindestens ein Speicherpunkt)"};
                    }
                } else if (name == "--seeds") {
                    o.seeds = parse_count(name, *value, kMaxBalanceSeeds);
                    if (*o.seeds == 0) {
                        throw UsageError{"Ungültiger Wert für --seeds: '0' (mindestens ein Lauf)"};
                    }
                } else if (name == "--tage") {
                    o.days = parse_count(name, *value, kMaxDays);
                } else if (name == "--threads") {
                    o.threads = static_cast<unsigned>(parse_count(name, *value, kMaxThreads));
                } else if (name == "--bloecke") {
                    o.blocks = parse_count(name, *value, kMaxLoadTestBlocks);
                } else if (name == "--vergleich") {
                    o.compare_threads = static_cast<unsigned>(parse_count(name, *value, kMaxThreads));
                } else if (name == "--grenze") {
                    const auto limit = Fixed::parse(*value);
                    if (!limit || *limit <= Fixed::zero()) {
                        throw UsageError{"Ungültiger Wert für --grenze: '" + *value +
                                         "' (erwartet Sekunden größer als 0, z. B. 2 oder 1.5)"};
                    }
                    o.limit_seconds = limit->to_double(); // nur zum Vergleich mit der gemessenen Zeit
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
        } else if (!o.file) {
            o.file = fs::path(std::string(arg)); // nur für wiederholung und spielstand, siehe run()
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

std::string pad_right(const std::string& text, std::size_t width) {
    return pad(text, width, false);
}

std::string pad_left(const std::string& text, std::size_t width) {
    return pad(text, width, true);
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

/// Strecken, Abschnitte und Märkte mit Bahnfracht zum Startdatum (seit M7).
std::string railway_summary(const World& w) {
    std::size_t sections = 0;
    std::size_t open = 0;
    for (const uhrwerk::Railway& r : w.railways) {
        sections += r.sections.size();
        for (const uhrwerk::RailSection& s : r.sections) {
            open += s.opened <= w.date ? 1 : 0;
        }
    }
    // Marktpaare, nicht Richtungen: Der Nachbar des einen Marktes hat ihn meist auch als Nachbarn.
    std::set<std::pair<std::size_t, std::size_t>> pairs;
    const uhrwerk::EconomyTopology topology = uhrwerk::build_economy_topology(w);
    for (std::size_t m = 0; m < topology.neighbors.size(); ++m) {
        for (const uhrwerk::MarketLink& link : topology.neighbors[m]) {
            if (link.rail_from && *link.rail_from <= w.date) {
                const auto n = static_cast<std::size_t>(link.market);
                pairs.emplace(std::min(m, n), std::max(m, n));
            }
        }
    }
    return plural(w.railways.size(), "Strecke", "Strecken") + ", " +
           plural(sections, "Abschnitt", "Abschnitte") + " (" + std::to_string(open) +
           " zum Startdatum eröffnet); " + plural(pairs.size(), "Paar", "Paare") +
           " von Nachbarmärkten mit Bahnfracht";
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

// ─── Spielstände (seit M6) ──────────────────────────────────────────────────

std::string format_bytes(std::uint64_t bytes) {
    if (bytes < 1024 * 1024) {
        return format_int(static_cast<std::int64_t>((bytes + 512) / 1024)) + " KB";
    }
    return format_fixed(Fixed::from_ratio(static_cast<std::int64_t>(bytes), 1024 * 1024), 1) + " MB";
}

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

/// Datum days Tage nach start.
Date date_after(const Date& start, std::int64_t days) {
    return uhrwerk::from_day_number(uhrwerk::to_day_number(start) + days);
}

/// Testbefehle (speicherstaende.md, Abschnitt 5): count Steuerbefehle an zufälligen Tagen in
/// [first_day, end_day), nach Tag geordnet. Gültige und ungültige (Staat -1 oder hinter dem letzten,
/// Steuerart 3, Satz unter 0 oder über dem Höchstsatz), aus dem eigenen Zufallsstrom Testbefehle.
std::vector<uhrwerk::LoggedCommand> make_test_commands(const World& w, std::uint64_t seed, std::int64_t count,
                                                       std::int64_t first_day, std::int64_t end_day) {
    std::vector<uhrwerk::LoggedCommand> list;
    if (end_day <= first_day) {
        return list;
    }
    uhrwerk::Pcg32 rng(seed, uhrwerk::RngStream::Testbefehle);
    const auto span = static_cast<std::uint32_t>(end_day - first_day);
    const auto states = static_cast<std::uint32_t>(w.states.size());
    const Fixed max = w.rules.tax_rate_max;
    const Fixed margin = max * Fixed::from_ratio(1, 10);
    for (std::int64_t i = 0; i < count; ++i) {
        uhrwerk::LoggedCommand c;
        c.day = first_day + rng.below(span);
        c.command.type = uhrwerk::CommandType::SetTaxRate;
        c.command.state = static_cast<Index>(rng.below(states + 2)) - 1;
        c.command.kind =
            static_cast<uhrwerk::TaxKind>(rng.below(static_cast<std::uint32_t>(uhrwerk::kTaxKinds) + 1));
        c.command.value = rng.uniform(-margin, max + margin);
        list.push_back(c);
    }
    std::stable_sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.day < b.day; });
    return list;
}

/// Reiht vor jedem Schritt die Befehle ein, die an diesem Tag fällig sind.
class CommandFeed {
public:
    explicit CommandFeed(std::vector<uhrwerk::LoggedCommand> commands) : commands_(std::move(commands)) {}

    void submit_due(uhrwerk::Simulation& sim) {
        while (next_ < commands_.size() && commands_[next_].day <= sim.days_elapsed()) {
            sim.submit(commands_[next_++].command);
        }
    }
    const std::vector<uhrwerk::LoggedCommand>& commands() const { return commands_; }

private:
    std::vector<uhrwerk::LoggedCommand> commands_;
    std::size_t next_ = 0;
};

std::optional<std::vector<std::uint8_t>> read_or_report(const fs::path& path) {
    std::string error;
    auto bytes = uhrwerk::read_file(path, &error);
    if (!bytes) {
        std::cerr << "Fehler: " << error << '\n';
    }
    return bytes;
}

bool write_or_report(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::string error;
    if (!uhrwerk::write_file(path, bytes, &error)) {
        std::cerr << "Fehler: " << error << '\n';
        return false;
    }
    return true;
}

/// Spielstand laden: Spieldaten für sein Szenario frisch laden, dann den Zustand hinein.
std::optional<uhrwerk::Simulation> load_save_or_report(const std::vector<std::uint8_t>& bytes,
                                                       const fs::path& data_dir, unsigned threads,
                                                       bool show_warnings) {
    std::string error;
    const auto info = uhrwerk::read_save_info(bytes, &error);
    if (!info) {
        std::cerr << "Fehler: " << error << '\n';
        return std::nullopt;
    }
    auto content = load(data_dir, info->scenario, show_warnings);
    if (!content) {
        return std::nullopt;
    }
    uhrwerk::LoadedGame game = uhrwerk::load_game(bytes, std::move(*content), threads);
    if (!game.ok()) {
        std::cerr << "Fehler: " << game.error << '\n';
        return std::nullopt;
    }
    return std::move(game.simulation);
}

/// Spielstand schreiben und beschreiben; die Zeile "Prüfwert:" vergleicht der CI-Job Determinismus.
bool save_and_report(const uhrwerk::Simulation& sim, const fs::path& path, const std::string& player_tag) {
    const auto t0 = std::chrono::steady_clock::now();
    const std::vector<std::uint8_t> bytes = uhrwerk::save_game(sim, uhrwerk::SaveMeta{player_tag});
    if (!write_or_report(path, bytes)) {
        return false;
    }
    const double seconds = seconds_since(t0);
    const auto info = uhrwerk::read_save_info(bytes);
    std::cout << "Spielstand: " << uhrwerk::path_to_utf8(path) << " (" << format_bytes(bytes.size())
              << ", Nutzdaten " << format_bytes(info ? info->payload_size : 0) << ", "
              << format_seconds(seconds) << ")\n"
              << "Prüfwert:  " << hex16(info ? info->payload_hash : 0) << " (Nutzdaten des Spielstands)\n";
    return true;
}

/// Zielwerte des Balancing-Berichts für ein Szenario: common/zielwerte_<szenario>.chron, wenn es die
/// Datei gibt (seit M7, eigene Ziele für 1848), sonst die allgemeinen common/zielwerte.chron.
fs::path default_targets_file(const fs::path& data_dir, const std::string& scenario) {
    const fs::path own = data_dir / "common" / ("zielwerte_" + scenario + ".chron");
    std::error_code ec;
    return fs::exists(own, ec) ? own : data_dir / "common" / "zielwerte.chron";
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
    // Zielwerte des Balancing-Berichts (seit M5), wenn vorhanden: Nur `bilanz` liest sie, Fehler
    // sollen aber schon hier auffallen.
    const fs::path targets = default_targets_file(data_dir, o.scenario);
    std::error_code ec;
    if (result.ok() && fs::exists(targets, ec)) {
        for (const std::string& error : uhrwerk::load_balance_targets(targets, *result.world).errors) {
            ++errors;
            std::cout << error << '\n';
        }
    }
    std::cout << "Ergebnis: " << plural(errors, "Fehler", "Fehler") << ", "
              << plural(warnings, "Warnung", "Warnungen") << ".\n";
    if (errors > 0) {
        return kExitDataError;
    }
    const World& w = *result.world;
    std::cout << "Geladen: " << plural(w.terrains.size(), "Geländetyp", "Geländetypen") << ", "
              << plural(w.confessions.size(), "Konfession", "Konfessionen") << ", "
              << plural(w.cultures.size(), "Kultur", "Kulturen") << ", "
              << plural(w.professions.size(), "Beruf", "Berufe") << ", "
              << plural(w.goods.size(), "Gut", "Güter") << ", "
              << plural(w.needs.size(), "Bedürfnis", "Bedürfnisse") << ", "
              << plural(w.businesses.size(), "Betrieb", "Betriebe") << ", "
              << plural(w.markets.size(), "Markt", "Märkte") << ", "
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
              << "              " << plural(w.goods.size(), "Gut", "Güter") << ", "
              << plural(w.needs.size(), "Bedürfnis", "Bedürfnisse") << ", "
              << plural(w.businesses.size(), "Betrieb", "Betriebe") << ", "
              << plural(w.markets.size(), "Markt", "Märkte") << '\n'
              << "              " << plural(w.states.size(), "Staat", "Staaten") << ", "
              << plural(w.pops.size(), "Bevölkerungsblock", "Bevölkerungsblöcke") << '\n'
              << "Orte:         " << group_digits(std::to_string(w.locations.size())) << " "
              << location_data_split(w) << '\n'
              << "Kreise:       " << group_digits(std::to_string(w.districts.size())) << " "
              << location_district_split(w) << '\n'
              << "Eisenbahnen:  " << railway_summary(w) << '\n'
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
                        format_percent(s.tax_rate(uhrwerk::TaxKind::Kontribution))});
    }
    print_table(rows, {false, false, false, true, true, true, true}, 0);
    return kExitOk;
}

void print_report(const uhrwerk::Simulation& sim) {
    const World& w = sim.world();
    std::cout << "\nBericht zum " << uhrwerk::format_date_long(w.date) << " (Beträge in Reichstalern)\n";
    std::vector<std::vector<std::string>> rows;
    rows.push_back(
        {"Staat", "Bevölkerung", "Schatz", "Schulden", "Einnahmen/Monat", "Ausgaben/Monat", "Kontribution"});
    Fixed population;
    Fixed treasury;
    Fixed debt;
    Fixed income;
    Fixed expenses;
    for (std::size_t i = 0; i < w.states.size(); ++i) {
        const uhrwerk::State& s = w.states[i];
        const Fixed pop = w.state_population(static_cast<Index>(i));
        population += pop;
        treasury += s.treasury;
        debt += s.debt;
        income += s.last_month_income;
        expenses += s.last_month_expenses;
        rows.push_back({s.short_name + " (" + s.tag + ")", format_int(pop.round()),
                        format_fixed(s.treasury, 2), format_fixed(s.debt, 2),
                        format_fixed(s.last_month_income, 2), format_fixed(s.last_month_expenses, 2),
                        format_percent(s.tax_rate(uhrwerk::TaxKind::Kontribution))});
    }
    rows.push_back({"Gesamt", format_int(population.round()), format_fixed(treasury, 2),
                    format_fixed(debt, 2), format_fixed(income, 2), format_fixed(expenses, 2), ""});
    print_table(rows, {false, true, true, true, true, true, true}, 1);
}

void print_chronicle(const World& w) {
    std::cout << "\nChronik (" << plural(w.chronicle.size(), "Eintrag", "Einträge") << ")\n";
    for (const uhrwerk::ChronicleEntry& e : w.chronicle) {
        std::cout << uhrwerk::format_date(e.date) << "  " << e.text << '\n';
    }
}

Fixed total_population(const World& w) {
    Fixed total;
    for (std::size_t i = 0; i < w.locations.size(); ++i) {
        total += w.location_population(static_cast<Index>(i));
    }
    return total;
}

/// Promille einer Größe bezogen auf die Bevölkerung, z. B. "37,9 ‰".
std::string per_mille(Fixed value, Fixed population) {
    if (population <= Fixed::zero()) {
        return "–";
    }
    return format_fixed(value * Fixed::from_int(1000) / population, 1) + " ‰";
}

/// Bevölkerung am Anfang und Ende und die Bilanz des letzten vollen Jahres (seit M4).
void print_population(const World& w, Fixed start, Fixed end) {
    std::cout << "\nBevölkerung: " << format_int(start.round()) << " → " << format_int(end.round());
    if (start > Fixed::zero()) {
        std::cout << " (×" << format_fixed(end / start, 2) << ")";
    }
    std::cout << '\n';
    // Mittel über alle Blöcke, nach Personen gewichtet.
    Fixed people;
    Fixed living;
    Fixed literacy;
    for (const uhrwerk::PopBlock& pop : w.pops) {
        people += pop.size();
        living += pop.size() * pop.living_standard;
        literacy += pop.size() * pop.literacy;
    }
    if (people > Fixed::zero()) {
        std::cout << "Gesellschaft: Lebensstandard " << format_fixed(living / people, 1)
                  << ", Alphabetisierung " << format_fixed(literacy * Fixed::from_int(100) / people, 1)
                  << " %\n";
    }
    uhrwerk::PopulationBalance year;
    for (const uhrwerk::PopulationBalance& b : w.balance_previous) {
        year.births += b.births;
        year.deaths += b.deaths;
        year.plague_deaths += b.plague_deaths;
        year.moved_out += b.moved_out;
        year.emigrated += b.emigrated;
    }
    if (year.births > Fixed::zero()) {
        std::cout << "Vorjahr:     Geburten " << per_mille(year.births, end) << ", Sterbefälle "
                  << per_mille(year.deaths, end) << ", Seuchen " << per_mille(year.plague_deaths, end)
                  << ", Binnenwanderung " << per_mille(year.moved_out, end) << ", Auswanderung "
                  << per_mille(year.emigrated, end) << '\n';
    }
}

int cmd_lauf(const Options& o, const fs::path& data_dir) {
    if (o.load_file && (o.given.count("--seed") != 0 || o.given.count("--szenario") != 0)) {
        throw UsageError{"Mit --laden kommen Szenario und Seed aus dem Spielstand"};
    }
    const auto load_start = std::chrono::steady_clock::now();
    std::optional<uhrwerk::Simulation> loaded;
    std::string player_tag;
    if (o.load_file) {
        const auto bytes = read_or_report(*o.load_file);
        if (!bytes) {
            return kExitDataError;
        }
        loaded = load_save_or_report(*bytes, data_dir, o.threads, true);
        if (!loaded) {
            return kExitDataError;
        }
        player_tag = uhrwerk::read_save_info(*bytes)->player_tag;
    } else {
        auto world = load(data_dir, o.scenario, true);
        if (!world) {
            return kExitDataError;
        }
        player_tag = world->scenario.player_tag;
        loaded.emplace(std::move(*world), uhrwerk::SimConfig{o.seed, o.threads});
    }
    uhrwerk::Simulation& sim = *loaded;
    const double load_seconds = seconds_since(load_start);
    const World& w = sim.world();
    const std::string content = plural(w.states.size(), "Staat", "Staaten") + ", " + describe_locations(w);
    const Date start = w.date;
    const std::int64_t first_day = sim.days_elapsed();
    const Period period = period_from(o, start);
    const std::int64_t days = days_between(start, add_years(start, period.years)) + period.extra_days;
    const std::string scenario_name = w.scenario.name + " (" + w.scenario.id + ")";
    CommandFeed feed(make_test_commands(w, sim.config().seed, o.test_commands, first_day, first_day + days));

    const Fixed population_start = total_population(sim.world());
    std::vector<uhrwerk::Checkpoint> checkpoints;
    const auto t0 = std::chrono::steady_clock::now();
    while (sim.days_elapsed() < first_day + days) {
        feed.submit_due(sim);
        sim.step_day();
        if (o.protocol_file && uhrwerk::is_checkpoint_day(sim)) {
            checkpoints.push_back(uhrwerk::Checkpoint{sim.days_elapsed(), sim.checksum()});
        }
    }
    const double seconds = seconds_since(t0);
    const Fixed population_end = total_population(sim.world());

    std::cout << "Szenario:  " << scenario_name << '\n'
              << "Inhalt:    " << content << '\n'
              << "Laden:     " << format_seconds(load_seconds) << '\n';
    if (o.load_file) {
        std::cout << "Geladen:   " << uhrwerk::path_to_utf8(*o.load_file) << " (Tag " << format_int(first_day)
                  << " der Partie)\n";
    }
    std::cout << "Seed:      " << sim.config().seed << '\n'
              << "Threads:   " << sim.threads() << '\n'
              << "Start:     " << uhrwerk::format_date_long(start) << '\n'
              << "Ende:      " << uhrwerk::format_date_long(sim.world().date) << " (" << format_int(days)
              << " Tage)\n"
              << "Laufzeit:  " << format_seconds(seconds);
    if (seconds > 0.0 && days > 0) {
        std::cout << " (" << format_int(static_cast<std::int64_t>(static_cast<double>(days) / seconds))
                  << " Tage/s)";
    }
    std::cout << '\n';
    if (o.test_commands > 0) {
        std::cout << "Befehle:   " << plural(feed.commands().size(), "Testbefehl", "Testbefehle") << " (Seed "
                  << sim.config().seed << ")\n";
    }
    print_population(sim.world(), population_start, population_end);

    if (o.report) {
        print_report(sim);
    }
    if (o.chronicle) {
        print_chronicle(sim.world());
    }
    if (o.save_file || o.protocol_file) {
        std::cout << '\n';
    }
    if (o.save_file && !save_and_report(sim, *o.save_file, player_tag)) {
        return kExitDataError;
    }
    if (o.protocol_file) {
        const uhrwerk::Replay replay = uhrwerk::make_replay(sim, std::move(checkpoints));
        if (!write_or_report(*o.protocol_file, uhrwerk::write_replay(replay))) {
            return kExitDataError;
        }
        std::cout << "Protokoll: " << uhrwerk::path_to_utf8(*o.protocol_file) << " ("
                  << plural(replay.commands.size(), "Befehl", "Befehle") << " seit Partiebeginn, "
                  << plural(replay.checkpoints.size(), "Kontrollpunkt", "Kontrollpunkte") << ")\n";
    }
    std::cout << "\nPrüfsumme: " << hex16(sim.checksum()) << '\n';
    return kExitOk;
}

/// Wirtschaft im Überblick (seit M5, docs/technik/wirtschaft.md Abschnitt 4.9): Angebot und Nachfrage je
/// Gut über alle Märkte, Einkommen und Lebensstandard je Beruf, Steuern je Staat – nach dem Einschwingen
/// oder nach --jahre/--tage. Werkzeug der Kalibrierung.
int cmd_wirtschaft(const Options& o, const fs::path& data_dir) {
    auto world = load(data_dir, o.scenario, true);
    if (!world) {
        return kExitDataError;
    }
    const Date start = world->date;
    std::int64_t days = 0;
    if (o.years || o.days) {
        const Period period = period_from(o, start);
        days = days_between(start, add_years(start, period.years)) + period.extra_days;
    }
    uhrwerk::Simulation sim(std::move(*world), uhrwerk::SimConfig{o.seed, o.threads});
    sim.step_days(days);
    const World& w = sim.world();
    const std::size_t goods = w.goods.size();
    const std::size_t professions = w.professions.size();
    const std::size_t needs = w.needs.size();

    std::cout << "Wirtschaft zum " << uhrwerk::format_date_long(w.date)
              << (days == 0 ? " (nach dem Einschwingen)" : "") << "\n\n";

    // Güter: Summen über alle Märkte; Preis als Mittel nach Angebot.
    std::cout << "Gut               Angebot   Nachfrage  Nachfr./Ang.  Preis/Basis    Einfuhr    Ausfuhr\n";
    for (std::size_t g = 0; g < goods; ++g) {
        Fixed supply;
        Fixed demand;
        Fixed imports;
        Fixed exports;
        Fixed weighted;
        for (const uhrwerk::MarketState& m : w.market_states) {
            supply += m.supply[g];
            demand += m.demand[g];
            imports += m.imports[g];
            exports += m.exports[g];
            weighted += m.supply[g] * m.price[g] / w.goods[g].base_price;
        }
        const Fixed ratio = supply > Fixed::zero() ? demand / supply : Fixed::zero();
        const Fixed price = supply > Fixed::zero() ? weighted / supply : Fixed::zero();
        std::cout << pad_right(w.goods[g].name, 14) << pad_left(format_fixed(supply, 0), 12)
                  << pad_left(format_fixed(demand, 0), 12) << pad_left(format_fixed(ratio, 2), 14)
                  << pad_left(format_fixed(price, 2), 13) << pad_left(format_fixed(imports, 0), 11)
                  << pad_left(format_fixed(exports, 0), 11) << '\n';
    }

    // Berufe: Mittel über die Orte, nach Köpfen gewichtet.
    std::vector<Fixed> people(professions);
    std::vector<Fixed> income(professions);
    std::vector<Fixed> target(professions);
    std::vector<Fixed> living(professions);
    std::vector<Fixed> food(professions);
    std::vector<Fixed> coverage(professions * needs);
    for (std::size_t li = 0; li < w.locations.size(); ++li) {
        const uhrwerk::Location& loc = w.locations[li];
        std::vector<Fixed> here(professions);
        for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
            const uhrwerk::PopBlock& pop = w.pops[static_cast<std::size_t>(p)];
            here[static_cast<std::size_t>(pop.profession)] += pop.size();
            living[static_cast<std::size_t>(pop.profession)] += pop.size() * pop.living_standard;
        }
        for (std::size_t p = 0; p < professions; ++p) {
            if (here[p] <= Fixed::zero()) {
                continue;
            }
            people[p] += here[p];
            income[p] += here[p] * w.location_income[li * professions + p];
            target[p] += here[p] * w.snapshot_targets[li * professions + p];
            food[p] += here[p] * uhrwerk::food_coverage(w, static_cast<Index>(li), static_cast<Index>(p));
            for (std::size_t n = 0; n < needs; ++n) {
                coverage[p * needs + n] += here[p] * w.location_coverage[(li * professions + p) * needs + n];
            }
        }
    }
    std::cout << "\nBeruf               Köpfe  Einkommen/Jahr  Lebensstandard  Ziel  Bedarfsdeckung je "
                 "Bedürfnis (%)\n";
    for (std::size_t p = 0; p < professions; ++p) {
        if (people[p] <= Fixed::zero()) {
            continue;
        }
        std::cout << pad_right(w.professions[p].name, 14) << pad_left(format_fixed(people[p], 0), 11)
                  << pad_left(format_fixed(income[p] * Fixed::from_int(12) / people[p], 1), 16)
                  << pad_left(format_fixed(living[p] / people[p], 1), 16)
                  << pad_left(format_fixed(target[p] / people[p], 1), 6) << "  ";
        for (std::size_t n = 0; n < needs; ++n) {
            std::cout << ' ' << format_fixed(coverage[p * needs + n] * Fixed::from_int(100) / people[p], 0);
        }
        std::cout << '\n';
    }

    // Staaten: Haushalt des letzten Rechentags (vor dem ersten Rechentag: geschätzt) je Monat.
    std::cout << '\n';
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Staat", "Leistung", "Quote", "Kontribution", "Akzise", "Zoll", "Hof", "Verwaltung",
                    "Heer", "Zinsen", "Bauten", "Saldo", "Schatz", "Schulden"});
    for (std::size_t s = 0; s < w.states.size(); ++s) {
        const uhrwerk::State& state = w.states[s];
        const uhrwerk::StateBudget& b = state.budget;
        // Wirtschaftsleistung (Wertschöpfung der Orte je Monat) und Steuerquote wie im Balancing-Bericht.
        Fixed production;
        Fixed taxes;
        for (const Index li : w.state_locations(static_cast<Index>(s))) {
            production += w.economy[static_cast<std::size_t>(li)].production;
            taxes += w.economy[static_cast<std::size_t>(li)].total_taxes();
        }
        const std::string quota = production > Fixed::zero()
                                      ? format_fixed(taxes * Fixed::from_int(100) / production, 1) + " %"
                                      : "–";
        rows.push_back({state.short_name + " (" + state.tag + ")", format_fixed(production, 0), quota,
                        format_fixed(b.taxes[0], 0), format_fixed(b.taxes[1], 0), format_fixed(b.taxes[2], 0),
                        format_fixed(b.court, 0), format_fixed(b.administration, 0), format_fixed(b.army, 0),
                        format_fixed(b.interest, 0), format_fixed(b.building, 0),
                        format_fixed(b.income() - b.expenses(), 0), format_fixed(state.treasury, 0),
                        format_fixed(state.debt, 0)});
    }
    print_table(rows, {false, true, true, true, true, true, true, true, true, true, true, true, true, true},
                0);

    if (o.markets) {
        std::cout << "\nPreise je Markt (Faktor auf den Basispreis; Erntefaktor in Klammern)\n";
        for (std::size_t m = 0; m < w.markets.size(); ++m) {
            const uhrwerk::MarketState& state = w.market_states[m];
            Fixed total;
            for (std::size_t g = 0; g < goods; ++g) {
                total += state.supply[g] + state.demand[g];
            }
            if (total <= Fixed::zero()) {
                continue;
            }
            std::cout << pad_right(w.markets[m].name, 36) << " (" << format_fixed(state.harvest, 2) << ")";
            for (std::size_t g = 0; g < goods; ++g) {
                std::cout << ' ' << format_fixed(state.price[g] / w.goods[g].base_price, 2);
            }
            std::cout << '\n';
        }
    }
    return kExitOk;
}

/// Zahl mit höchstens zwei Nachkommastellen ohne Nullen am Ende, z. B. "0,4", "-3", "1,25".
std::string format_compact(Fixed value) {
    std::string text = format_fixed(value, 2);
    if (text.find(',') != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == ',') {
            text.pop_back();
        }
    }
    return text;
}

std::string format_range(const uhrwerk::ValueRange& range, const std::string& prefix = "") {
    return prefix + format_compact(range.min) + " … " + prefix + format_compact(range.max);
}

std::string format_share(Fixed share) {
    return format_fixed(share * Fixed::from_int(100), 1) + " %";
}

/// "Median (5-%-Wert … 95-%-Wert)".
std::string format_quantiles(const uhrwerk::Quantiles& q, int decimals, const std::string& prefix = "") {
    return prefix + format_fixed(q.median, decimals) + " (" + prefix + format_fixed(q.p5, decimals) + " … " +
           prefix + format_fixed(q.p95, decimals) + ")";
}

const std::array<std::string_view, 3> kStrata = {"unten", "mittel", "oben"};

std::string describe_violation(const World& w, const uhrwerk::BalanceTargets& t,
                               const uhrwerk::BalanceViolation& v) {
    using uhrwerk::BalanceCheck;
    const auto subject = static_cast<std::size_t>(v.subject);
    const std::string state = v.subject >= 0 && subject < w.states.size()
                                  ? w.states[subject].short_name + " (" + w.states[subject].tag + ")"
                                  : std::string("–");
    switch (v.check) {
    case BalanceCheck::WealthShare:
        return state + ": Vermögen nur in " + format_share(v.value) + " der Staatsjahre im Bereich " +
               format_range(t.wealth) + " (Ziel mindestens " + format_share(t.wealth_share) + ")";
    case BalanceCheck::WealthEnd:
        return state + ": Vermögen am Ende im Median " + format_fixed(v.value, 2) +
               " Jahreseinnahmen (Ziel " + format_range(t.wealth_end) + ")";
    case BalanceCheck::BankruptciesMedian:
        return state + ": im Median " + format_fixed(v.value, 2) +
               " Staatsbankrotte je Jahrhundert (Ziel höchstens " + format_compact(t.bankruptcies_median) +
               ")";
    case BalanceCheck::BankruptciesP95:
        return state + ": 95-%-Wert " + format_fixed(v.value, 2) +
               " Staatsbankrotte je Jahrhundert (Ziel höchstens " + format_compact(t.bankruptcies_p95) + ")";
    case BalanceCheck::TaxRatio:
        return state + ": Steuerquote am Ende gegen den Start im Median " + format_fixed(v.value, 2) +
               " (Ziel " + format_range(t.tax_ratio) + ")";
    case BalanceCheck::PriceShare: {
        const std::string good = subject < w.goods.size() ? w.goods[subject].name : std::string("–");
        const uhrwerk::ValueRange range =
            subject < t.price_by_good.size() ? t.price_by_good[subject] : t.price;
        return good + ": Preis nur in " + format_share(v.value) + " der Markt-Monate im Bereich " +
               format_range(range) + " des Basispreises (Ziel mindestens " + format_share(t.price_share) +
               ")";
    }
    case BalanceCheck::Population:
        return "Bevölkerung " + std::to_string(t.population_year) + " im Median ×" +
               format_fixed(v.value, 2) + " (Ziel " + format_range(t.population, "×") + ")";
    case BalanceCheck::LivingStandard:
        return "Lebensstandard " + std::string(subject < kStrata.size() ? kStrata[subject] : "–") +
               " im Median " + format_fixed(v.value, 1) + " (Ziel " +
               format_range(t.living_standard[subject % 3]) + ")";
    }
    return "–";
}

/// Balancing-Bericht (seit M5, docs/technik/wirtschaft.md Abschnitt 7): mehrere Läufe ohne Eingriffe des
/// Spielers, Kennzahlen je Staat, Gut und Schicht gegen die Zielwerte (data/common/zielwerte*.chron).
int cmd_bilanz(const Options& o, const fs::path& data_dir) {
    auto world = load(data_dir, o.scenario, false);
    if (!world) {
        return kExitDataError;
    }
    const fs::path targets_file =
        o.targets_file ? *o.targets_file : default_targets_file(data_dir, o.scenario);
    const uhrwerk::BalanceTargetsResult loaded = uhrwerk::load_balance_targets(targets_file, *world);
    if (!loaded.targets) {
        for (const std::string& error : loaded.errors) {
            std::cerr << error << '\n';
        }
        std::cerr << "Zielwerte nicht lesbar: " << uhrwerk::path_to_utf8(targets_file) << '\n';
        return kExitDataError;
    }
    const uhrwerk::BalanceTargets& t = *loaded.targets;
    const std::int64_t years = o.years.value_or(kDefaultBalanceYears);
    if (years <= 0) {
        throw UsageError{"Der Bericht braucht mindestens ein Jahr (--jahre)"};
    }
    period_from(o, world->date); // prüft den Zeitraum
    const std::int64_t seeds = o.seeds.value_or(kDefaultBalanceSeeds);

    const auto t0 = std::chrono::steady_clock::now();
    std::vector<uhrwerk::BalanceRun> runs;
    unsigned threads = 0;
    for (std::int64_t i = 0; i < seeds; ++i) {
        uhrwerk::Simulation sim(*world,
                                uhrwerk::SimConfig{o.seed + static_cast<std::uint64_t>(i), o.threads});
        threads = sim.threads();
        runs.push_back(uhrwerk::record_run(sim, years, t));
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const uhrwerk::BalanceSummary summary = uhrwerk::summarize(*world, runs, t);
    const World& w = *world;

    std::cout << "Balancing-Bericht: " << w.scenario.name << " (" << w.scenario.id << ")\n"
              << "Läufe:     " << seeds << " (Seeds " << o.seed;
    if (seeds > 1) {
        std::cout << "–" << o.seed + static_cast<std::uint64_t>(seeds - 1);
    }
    std::cout << "), je " << years << " Jahre ab " << uhrwerk::format_date_long(w.date) << '\n'
              << "Zielwerte: " << uhrwerk::path_to_utf8(targets_file) << '\n'
              << "Threads:   " << threads << '\n'
              << "Laufzeit:  " << format_seconds(seconds) << "\n\n";

    std::set<std::pair<int, Index>> marked;
    for (const uhrwerk::BalanceViolation& v : summary.violations) {
        marked.insert({static_cast<int>(v.check), v.subject});
    }
    const auto mark = [&marked](uhrwerk::BalanceCheck check, Index subject) {
        return marked.count({static_cast<int>(check), subject}) != 0 ? std::string(" !") : std::string();
    };
    using uhrwerk::BalanceCheck;

    std::cout
        << "Staaten: Vermögen = (Schatz − Schulden) ÷ Jahreseinnahmen; Median (5 % … 95 %) über alle Läufe\n";
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Staat", "Vermögen je Jahr", "im Bereich", "tiefster", "Vermögen am Ende",
                    "Bankrotte/Jh.", "Steuerquote Ende/Start"});
    for (std::size_t s = 0; s < w.states.size(); ++s) {
        const uhrwerk::BalanceStateSummary& st = summary.states[s];
        const auto index = static_cast<Index>(s);
        rows.push_back(
            {w.states[s].short_name + " (" + w.states[s].tag + ")", format_quantiles(st.wealth, 2),
             format_share(st.wealth_share) + mark(BalanceCheck::WealthShare, index),
             format_fixed(st.wealth_min, 2),
             format_quantiles(st.wealth_end, 2) + mark(BalanceCheck::WealthEnd, index),
             format_quantiles(st.bankruptcies, 1) + mark(BalanceCheck::BankruptciesMedian, index) +
                 mark(BalanceCheck::BankruptciesP95, index),
             st.tax_ratio ? format_quantiles(*st.tax_ratio, 2) + mark(BalanceCheck::TaxRatio, index) : "–"});
    }
    print_table(rows, {false, true, true, true, true, true, true}, 0);
    std::cout << "Ziele: Vermögen " << format_range(t.wealth) << " in mindestens "
              << format_share(t.wealth_share) << " der Staatsjahre, am Ende " << format_range(t.wealth_end)
              << "; Bankrotte je Jahrhundert im Median " << "höchstens "
              << format_compact(t.bankruptcies_median) << ", 95 % höchstens "
              << format_compact(t.bankruptcies_p95) << "; Steuerquote " << format_range(t.tax_ratio)
              << "\n\n";

    std::cout << "Preise: Faktor auf den Basispreis, alle Märkte mit Orten, alle Monate\n";
    rows.clear();
    rows.push_back({"Gut", "Ziel", "im Bereich", "kleinster", "größter"});
    for (std::size_t g = 0; g < w.goods.size(); ++g) {
        const uhrwerk::BalanceGoodSummary& gs = summary.goods[g];
        if (gs.months == 0) {
            continue;
        }
        rows.push_back({w.goods[g].name,
                        format_range(g < t.price_by_good.size() ? t.price_by_good[g] : t.price),
                        format_share(gs.share) + mark(BalanceCheck::PriceShare, static_cast<Index>(g)),
                        format_fixed(gs.min, 2), format_fixed(gs.max, 2)});
    }
    print_table(rows, {false, true, true, true, true}, 0);
    std::cout << "Ziel: mindestens " << format_share(t.price_share) << " der Markt-Monate im Bereich\n\n";

    std::cout << "Bevölkerung " << w.date.year << " → " << t.population_year << ": ";
    if (summary.population) {
        std::cout << format_quantiles(*summary.population, 2, "×")
                  << mark(BalanceCheck::Population, uhrwerk::kNone);
    } else {
        std::cout << "–";
    }
    std::cout << "; Ziel " << format_range(t.population, "×") << '\n';
    std::cout << "Lebensstandard (Median über alle Jahre):";
    for (std::size_t k = 0; k < 3; ++k) {
        std::cout << (k == 0 ? " " : "; ") << kStrata[k] << ' ';
        if (summary.living_standard[k]) {
            std::cout << format_quantiles(*summary.living_standard[k], 1)
                      << mark(BalanceCheck::LivingStandard, static_cast<Index>(k));
        } else {
            std::cout << "–";
        }
        std::cout << ", Ziel " << format_range(t.living_standard[k]);
    }
    std::cout << "\n\n";

    if (summary.violations.empty()) {
        std::cout << "Verstöße: keine – alle Zielwerte erfüllt.\n";
        return kExitOk;
    }
    std::cout << "Verstöße (" << summary.violations.size() << ", mit ! markiert):\n";
    for (const uhrwerk::BalanceViolation& v : summary.violations) {
        std::cout << "  " << describe_violation(w, t, v) << '\n';
    }
    return kExitDataError;
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

    // Lauf B rechnet mit einem Thread: Das Ergebnis darf nicht von der Thread-Zahl abhängen.
    uhrwerk::Simulation a(std::move(*world_a), uhrwerk::SimConfig{o.seed, o.threads});
    uhrwerk::Simulation b(std::move(*world_b), uhrwerk::SimConfig{o.seed, 1});
    uhrwerk::Simulation c(std::move(*world_c), uhrwerk::SimConfig{other_seed, o.threads});

    std::cout << "Determinismusprüfung\n"
              << "Szenario:  " << scenario_name << '\n'
              << "Zeitraum:  " << plural(static_cast<std::size_t>(period.years), "Jahr", "Jahre");
    if (period.extra_days > 0) {
        std::cout << " und " << plural(static_cast<std::size_t>(period.extra_days), "Tag", "Tage");
    }
    std::cout << " ab " << uhrwerk::format_date_long(start) << '\n'
              << "Seeds:     Lauf A und B " << o.seed << ", Lauf C " << other_seed << '\n'
              << "Threads:   Lauf A und C " << a.threads() << ", Lauf B 1\n";

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

/// Speicherpunkte für den speichertest: gleichmäßig über den Zeitraum verteilt (meist mitten im Monat);
/// jeder zweite rückt auf den nächsten Tag mit einem Testbefehl, damit wartende Befehle im Spielstand
/// liegen. Dazu kommt im Lauf der erste Tag mit Zugängen (cmd_speichertest).
std::vector<std::int64_t> save_points(std::int64_t days, std::int64_t count,
                                      const std::vector<uhrwerk::LoggedCommand>& commands) {
    std::vector<std::int64_t> points;
    for (std::int64_t i = 0; i < count; ++i) {
        std::int64_t day = (i + 1) * days / (count + 1);
        const std::int64_t next = (i + 2) * days / (count + 1);
        if (i % 2 == 1) {
            const auto it = std::lower_bound(commands.begin(), commands.end(), day,
                                             [](const auto& c, std::int64_t d) { return c.day < d; });
            if (it != commands.end() && it->day < next) {
                day = it->day;
            }
        }
        if (day < days && (points.empty() || day > points.back())) {
            points.push_back(day);
        }
    }
    return points;
}

/// Abnahmetest der Spielstände (speicherstaende.md, Abschnitt 10): Lauf A ohne Unterbrechung, Lauf B
/// speichert an den Speicherpunkten in eine Datei, lädt sie mit frisch geladenen Spieldaten und rechnet
/// mit dem geladenen Stand weiter. Die Prüfsummen am 1. Januar und am Ende müssen gleich sein.
int cmd_speichertest(const Options& o, const fs::path& data_dir) {
    auto world_a = load(data_dir, o.scenario, true);
    auto world_b = load(data_dir, o.scenario, false);
    if (!world_a || !world_b) {
        return kExitDataError;
    }
    const Date start = world_a->date;
    const Period period = period_from(o, start);
    const std::int64_t days = days_between(start, add_years(start, period.years)) + period.extra_days;
    const std::string scenario_name = world_a->scenario.name + " (" + world_a->scenario.id + ")";
    const std::vector<uhrwerk::LoggedCommand> commands =
        make_test_commands(*world_a, o.seed, o.test_commands, 0, days);
    const std::vector<std::int64_t> points = save_points(days, o.save_points, commands);
    const fs::path file =
        fs::temp_directory_path() /
        ("uhrwerk_speichertest_" + std::to_string(o.seed) + "_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".einigkeit");

    std::cout << "Speichertest (docs/technik/speicherstaende.md, Abschnitt 10)\n"
              << "Szenario:  " << scenario_name << '\n'
              << "Zeitraum:  " << format_int(days) << " Tage ab " << uhrwerk::format_date_long(start) << '\n'
              << "Seed:      " << o.seed << ", " << plural(commands.size(), "Testbefehl", "Testbefehle")
              << '\n'
              << "Lauf A:    ohne Unterbrechung\n"
              << "Lauf B:    " << plural(points.size(), "Speicherpunkt", "Speicherpunkte")
              << " und der erste Tag mit Zugängen; je Speichern in " << uhrwerk::path_to_utf8(file)
              << " und Laden mit frischen Spieldaten\n";

    const auto t0 = std::chrono::steady_clock::now();
    uhrwerk::Simulation a(std::move(*world_a), uhrwerk::SimConfig{o.seed, o.threads});
    CommandFeed feed_a(commands);
    std::vector<uhrwerk::Checkpoint> reference;
    while (a.days_elapsed() < days) {
        feed_a.submit_due(a);
        a.step_day();
        if (uhrwerk::is_checkpoint_day(a)) {
            reference.push_back(uhrwerk::Checkpoint{a.days_elapsed(), a.checksum()});
        }
    }

    std::optional<uhrwerk::Simulation> b;
    b.emplace(std::move(*world_b), uhrwerk::SimConfig{o.seed, o.threads});
    CommandFeed feed_b(commands);
    std::size_t next_point = 0;
    std::size_t saved = 0;
    std::size_t next_check = 0;
    std::size_t with_pending = 0;
    std::size_t with_arrivals = 0;
    const auto cleanup = [&] {
        std::error_code ignored;
        fs::remove(file, ignored);
    };
    const auto count_arrivals = [](const uhrwerk::Simulation& sim) {
        std::size_t n = 0;
        for (const auto& list : sim.world().arrivals) {
            n += list.size();
        }
        return n;
    };
    while (b->days_elapsed() < days) {
        feed_b.submit_due(*b);
        const std::int64_t day = b->days_elapsed();
        const bool planned = next_point < points.size() && points[next_point] == day;
        // Zugänge entstehen nur zeitweise (bis zur Neuordnung am Monatsersten): einmal gezielt speichern.
        const std::size_t arrivals = planned || with_arrivals == 0 ? count_arrivals(*b) : 0;
        if (planned || (with_arrivals == 0 && arrivals > 0)) {
            const auto pending = static_cast<std::size_t>(
                std::count_if(b->command_log().begin(), b->command_log().end(),
                              [&](const uhrwerk::LoggedCommand& c) { return c.day == day; }));
            with_pending += pending > 0 ? 1 : 0;
            with_arrivals += arrivals > 0 ? 1 : 0;
            const Date date = b->world().date;

            const auto t_save = std::chrono::steady_clock::now();
            const std::vector<std::uint8_t> bytes = uhrwerk::save_game(*b, uhrwerk::SaveMeta{});
            if (!write_or_report(file, bytes)) {
                return kExitDataError;
            }
            const double save_seconds = seconds_since(t_save);
            b.reset();
            const auto t_load = std::chrono::steady_clock::now();
            const auto read = read_or_report(file);
            // Abwechselnd mit einem Thread und mit --threads weiterrechnen.
            const unsigned threads = saved % 2 == 0 ? 1u : o.threads;
            if (read) {
                b = load_save_or_report(*read, data_dir, threads, false);
            }
            cleanup();
            if (!b) {
                std::cout << "FEHLER: Spielstand vom " << uhrwerk::format_date(date)
                          << " lässt sich nicht laden\n";
                return kExitDataError;
            }
            const double load_seconds = seconds_since(t_load);
            next_point += planned ? 1 : 0;
            ++saved;
            std::cout << "Punkt " << pad_left(std::to_string(saved), 3) << ": " << uhrwerk::format_date(date)
                      << " (Tag " << format_int(day) << "), wartend " << pending << ", Zugänge " << arrivals
                      << (planned ? "" : " (Zusatzpunkt)") << ", " << format_bytes(bytes.size())
                      << ", speichern " << format_seconds(save_seconds) << ", laden "
                      << format_seconds(load_seconds) << '\n';
        }
        b->step_day();
        if (uhrwerk::is_checkpoint_day(*b)) {
            const uhrwerk::Checkpoint& expected = reference[next_check++];
            if (b->checksum() != expected.checksum) {
                std::cout << "FEHLER: Lauf B weicht am " << uhrwerk::format_date(b->world().date) << " ab: A "
                          << hex16(expected.checksum) << ", B " << hex16(b->checksum()) << '\n'
                          << "Ergebnis: Speichern und Laden verändern den Lauf.\n";
                return kExitDataError;
            }
        }
    }
    if (b->checksum() != a.checksum()) {
        std::cout << "FEHLER: Endstand weicht ab: A " << hex16(a.checksum()) << ", B " << hex16(b->checksum())
                  << '\n'
                  << "Ergebnis: Speichern und Laden verändern den Lauf.\n";
        return kExitDataError;
    }
    std::cout << "Lauf A/B:  " << plural(reference.size(), "Jahresprüfsumme", "Jahresprüfsummen")
              << " und Endstand gleich\n"
              << "Abdeckung: " << plural(saved, "Speicherpunkt", "Speicherpunkte") << ", davon "
              << with_pending << " mit wartenden Befehlen und " << with_arrivals << " mit Zugängen\n"
              << "Laufzeit:  " << format_seconds(seconds_since(t0)) << '\n'
              << "Ergebnis:  Speichern und Laden verändern den Lauf nicht\n"
              << "\nPrüfsumme: " << hex16(a.checksum()) << '\n';
    return kExitOk;
}

/// Protokoll oder Spielstand vom Start nachrechnen (speicherstaende.md, Abschnitt 5).
int cmd_wiederholung(const Options& o, const fs::path& data_dir) {
    const auto bytes = read_or_report(*o.file);
    if (!bytes) {
        return kExitDataError;
    }
    const uhrwerk::FileKind kind = uhrwerk::file_kind(*bytes);
    std::string error;
    const std::optional<uhrwerk::Replay> replay = kind == uhrwerk::FileKind::Replay
                                                      ? uhrwerk::read_replay(*bytes, &error)
                                                      : uhrwerk::replay_from_save(*bytes, &error);
    if (!replay) {
        std::cerr << "Fehler: " << error << '\n';
        return kExitDataError;
    }
    auto world = load(data_dir, replay->scenario, false);
    if (!world) {
        return kExitDataError;
    }
    const Date start = world->date;
    std::cout << "Wiederholung von " << uhrwerk::path_to_utf8(*o.file)
              << (kind == uhrwerk::FileKind::Replay ? " (Protokoll)" : " (Spielstand)") << '\n'
              << "Szenario:  " << world->scenario.name << " (" << replay->scenario << ")\n"
              << "Seed:      " << replay->seed << '\n'
              << "Zeitraum:  " << format_int(replay->end_day) << " Tage ab "
              << uhrwerk::format_date_long(start) << '\n'
              << "Befehle:   " << group_digits(std::to_string(replay->commands.size())) << '\n'
              << "Kontrollpunkte: " << replay->checkpoints.size() << " und der Endstand\n";

    const auto t0 = std::chrono::steady_clock::now();
    const uhrwerk::ReplayResult result = uhrwerk::run_replay(*replay, std::move(*world), o.threads);
    const double seconds = seconds_since(t0);
    if (result.mismatch_day >= 0) {
        std::cout << "FEHLER: Abweichung am " << uhrwerk::format_date(date_after(start, result.mismatch_day))
                  << " (Tag " << format_int(result.mismatch_day) << "): erwartet " << hex16(result.expected)
                  << ", berechnet " << hex16(result.actual) << '\n'
                  << "          Sie entstand nach dem vorigen Kontrollpunkt; Textexport zweier Spielstände "
                     "(spielstand --text) zeigt das Feld.\n"
                  << "Ergebnis:  NICHT bitgleich\n";
        return kExitDataError;
    }
    if (!result.ok) {
        std::cout << "FEHLER: " << result.error << '\n';
        return kExitDataError;
    }
    std::cout << "Laufzeit:  " << format_seconds(seconds) << '\n'
              << "Ergebnis:  bitgleich ("
              << plural(static_cast<std::size_t>(result.checkpoints), "Prüfsumme", "Prüfsummen")
              << " verglichen)\n"
              << "\nPrüfsumme: " << hex16(replay->end_checksum) << '\n';
    return kExitOk;
}

/// Angaben zu einem Spielstand oder Protokoll; lädt einen Spielstand probeweise mit den Spieldaten.
int cmd_spielstand(const Options& o, const fs::path& data_dir) {
    const auto bytes = read_or_report(*o.file);
    if (!bytes) {
        return kExitDataError;
    }
    std::string error;
    if (uhrwerk::file_kind(*bytes) == uhrwerk::FileKind::Replay) {
        if (o.text) {
            throw UsageError{"--text gibt es nur für Spielstände, nicht für Protokolle"};
        }
        const auto replay = uhrwerk::read_replay(*bytes, &error);
        if (!replay) {
            std::cerr << "Fehler: " << error << '\n';
            return kExitDataError;
        }
        std::cout << "Protokoll:  " << uhrwerk::path_to_utf8(*o.file) << " (" << format_bytes(bytes->size())
                  << ")\n"
                  << "Spiel:      " << replay->game_version << '\n'
                  << "Szenario:   " << replay->scenario << '\n'
                  << "Seed:       " << replay->seed << '\n'
                  << "Tage:       " << format_int(replay->end_day) << '\n'
                  << "Befehle:    " << group_digits(std::to_string(replay->commands.size())) << '\n'
                  << "Kontrollpunkte: " << replay->checkpoints.size() << '\n'
                  << "Start:      Prüfsumme " << hex16(replay->start_checksum) << '\n'
                  << "Ende:       Prüfsumme " << hex16(replay->end_checksum) << '\n'
                  << "Inhalt:     " << hex16(replay->content.total) << '\n'
                  << "Nachrechnen: uhrwerk-cli wiederholung " << uhrwerk::path_to_utf8(*o.file) << '\n';
        return kExitOk;
    }

    const auto info = uhrwerk::read_save_info(*bytes, &error);
    if (!info) {
        std::cerr << "Fehler: " << error << '\n';
        return kExitDataError;
    }
    if (o.text) {
        const auto sim = load_save_or_report(*bytes, data_dir, o.threads, false);
        if (!sim) {
            return kExitDataError;
        }
        std::cout << sim->state_text();
        return kExitOk;
    }
    std::cout << "Spielstand: " << uhrwerk::path_to_utf8(*o.file) << '\n'
              << "Format:     " << info->format_version << " (Spiel " << info->game_version << ")\n"
              << "Szenario:   " << info->scenario << '\n'
              << "Spieler:    " << (info->player_tag.empty() ? "–" : info->player_tag) << '\n'
              << "Datum:      " << uhrwerk::format_date_long(info->date) << " (Tag " << format_int(info->days)
              << " der Partie)\n"
              << "Seed:       " << info->seed << '\n'
              << "Befehle:    " << group_digits(std::to_string(info->commands)) << " seit Partiebeginn\n"
              << "Größe:      " << format_bytes(bytes->size()) << " (Nutzdaten "
              << format_bytes(info->payload_size) << (info->compressed ? ", mit zstd gepackt" : ", ungepackt")
              << ")\n"
              << "Prüfwert:   " << hex16(info->payload_hash) << " (Nutzdaten)\n"
              << "Prüfsumme:  " << hex16(info->checksum) << " (Start " << hex16(info->start_checksum) << ")\n"
              << "Inhalt:     " << hex16(info->content.total) << '\n';
    if (data_dir.empty()) {
        std::cout << "Laden:      nicht geprüft (Datenordner nicht gefunden)\n";
        return kExitOk;
    }
    auto content = load(data_dir, info->scenario, false);
    if (!content) {
        return kExitDataError;
    }
    const auto t0 = std::chrono::steady_clock::now();
    const uhrwerk::LoadedGame game = uhrwerk::load_game(*bytes, std::move(*content), o.threads);
    if (!game.ok()) {
        std::cout << "Laden:      FEHLER – " << game.error << '\n';
        return kExitDataError;
    }
    std::cout << "Laden:      in Ordnung mit " << uhrwerk::path_to_utf8(data_dir)
              << " (Prüfsumme nachgerechnet, " << format_seconds(seconds_since(t0)) << ")\n";
    return kExitOk;
}

/// Spitzenwert des Arbeitsspeichers in KiB, soweit das Betriebssystem ihn liefert (Linux: VmHWM).
std::optional<std::int64_t> peak_memory_kib() {
#if defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmHWM:", 0) == 0) {
            std::int64_t value = 0;
            for (const char c : line) {
                if (c >= '0' && c <= '9') {
                    value = value * 10 + (c - '0');
                }
            }
            return value;
        }
    }
#endif
    return std::nullopt;
}

int cmd_lasttest(const Options& o, const fs::path& data_dir) {
    const auto base = load(data_dir, o.scenario, true);
    if (!base) {
        return kExitDataError;
    }
    const std::int64_t years = o.years.value_or(kDefaultLoadTestYears);
    const uhrwerk::LoadTestOptions options{o.blocks, o.seed};

    const auto t_build = std::chrono::steady_clock::now();
    uhrwerk::Simulation sim(uhrwerk::make_load_test_world(*base, options),
                            uhrwerk::SimConfig{o.seed, o.threads});
    const double build_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t_build).count();

    std::cout << "Lasttest (docs/technik/bevoelkerung.md, Abschnitt 7)\n"
              << "Welt:      " << group_digits(std::to_string(sim.world().locations.size())) << " Orte, "
              << plural(sim.world().pops.size(), "Bevölkerungsblock", "Bevölkerungsblöcke") << ", "
              << format_int(total_population(sim.world()).round()) << " Einwohner (Ziel "
              << plural(static_cast<std::size_t>(o.blocks), "Block", "Blöcke") << ", Seed " << o.seed << ")\n"
              << "Aufbau:    " << format_seconds(build_seconds) << " (Welt und Wanderungsziele)\n"
              << "Threads:   " << sim.threads() << '\n';

    std::vector<double> times;
    std::vector<std::uint64_t> checksums;
    for (std::int64_t year = 1; year <= years; ++year) {
        const auto t0 = std::chrono::steady_clock::now();
        sim.step_days(365);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        times.push_back(seconds);
        checksums.push_back(sim.checksum());
        std::cout << "Jahr " << year << ":    " << format_seconds(seconds) << "  "
                  << plural(sim.world().pops.size(), "Block", "Blöcke") << "  Prüfsumme "
                  << hex16(checksums.back()) << '\n';
    }
    double median = 0.0;
    if (!times.empty()) {
        std::vector<double> sorted = times;
        std::sort(sorted.begin(), sorted.end());
        const std::size_t mid = sorted.size() / 2;
        median = sorted.size() % 2 == 1 ? sorted[mid] : (sorted[mid - 1] + sorted[mid]) / 2.0;
    }
    std::cout << "Median:    " << format_seconds(median) << " je Spieljahr (365 Tage)\n";
    if (const auto memory = peak_memory_kib()) {
        std::cout << "Speicher:  Spitze " << format_int((*memory + 512) / 1024) << " MB\n";
    } else {
        std::cout << "Speicher:  Spitzenwert auf diesem Betriebssystem nicht verfügbar\n";
    }

    bool ok = true;
    if (o.save_file) {
        // Spielstand der Lasttest-Welt (seit M6): Größe, Schreib- und Ladezeit. Die Lasttest-Welt ändert
        // nur den Zustand, nicht den Inhalt; sie lädt daher in die frisch geladenen Spieldaten.
        if (!save_and_report(sim, *o.save_file, "")) {
            return kExitDataError;
        }
        const auto t_load = std::chrono::steady_clock::now();
        const auto bytes = read_or_report(*o.save_file);
        const auto reloaded = bytes ? load_save_or_report(*bytes, data_dir, o.threads, false) : std::nullopt;
        if (!reloaded) {
            return kExitDataError;
        }
        const double load_seconds = seconds_since(t_load);
        const bool same = reloaded->checksum() == sim.checksum();
        std::cout << "Laden:     " << format_seconds(load_seconds)
                  << " (Datei lesen, Spieldaten laden, Zustand prüfen) – "
                  << (same ? "Prüfsumme gleich" : "FEHLER: Prüfsumme weicht ab") << '\n';
        ok = same;
    }
    if (o.compare_threads) {
        uhrwerk::Simulation other(uhrwerk::make_load_test_world(*base, options),
                                  uhrwerk::SimConfig{o.seed, *o.compare_threads});
        bool same = true;
        for (std::size_t y = 0; y < checksums.size() && same; ++y) {
            other.step_days(365);
            same = other.checksum() == checksums[y];
        }
        const std::string threads =
            std::to_string(other.threads()) + (other.threads() == 1 ? " Thread" : " Threads");
        if (same) {
            std::cout << "Vergleich: Lauf mit " << threads << " – alle Jahresprüfsummen gleich\n";
        } else {
            std::cout << "FEHLER: Lauf mit " << threads << " weicht ab (" << hex16(other.checksum()) << ")\n";
            ok = false;
        }
    }
    if (o.limit_seconds) {
        char limit[32];
        std::snprintf(limit, sizeof limit, "%g", *o.limit_seconds);
        std::string limit_text = limit;
        std::replace(limit_text.begin(), limit_text.end(), '.', ',');
        if (median <= *o.limit_seconds) {
            std::cout << "Grenze:    " << limit_text << " s je Spieljahr eingehalten\n";
        } else {
            std::cout << "FEHLER: Median " << format_seconds(median) << " liegt über der Grenze von "
                      << limit_text << " s\n";
            ok = false;
        }
    }
    std::cout << "\nPrüfsumme: " << hex16(sim.checksum()) << '\n';
    return ok ? kExitOk : kExitDataError;
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
            check_applicable(o, {"--daten", "--szenario", "--jahre", "--tage", "--seed", "--threads",
                                 "--bericht", "--chronik", "--speichern", "--laden", "--protokoll",
                                 "--testbefehle"});
        } else if (o.command == "determinismus") {
            check_applicable(o, {"--daten", "--szenario", "--jahre", "--tage", "--seed", "--threads"});
        } else if (o.command == "lasttest") {
            check_applicable(o, {"--daten", "--szenario", "--jahre", "--seed", "--threads", "--bloecke",
                                 "--vergleich", "--grenze", "--speichern"});
        } else if (o.command == "wirtschaft") {
            check_applicable(
                o, {"--daten", "--szenario", "--jahre", "--tage", "--seed", "--threads", "--maerkte"});
        } else if (o.command == "bilanz") {
            check_applicable(
                o, {"--daten", "--szenario", "--jahre", "--seed", "--threads", "--seeds", "--ziele"});
        } else if (o.command == "speichertest") {
            check_applicable(o, {"--daten", "--szenario", "--jahre", "--tage", "--seed", "--threads",
                                 "--testbefehle", "--punkte"});
        } else if (o.command == "wiederholung") {
            check_applicable(o, {"--daten", "--threads"});
        } else if (o.command == "spielstand") {
            check_applicable(o, {"--daten", "--threads", "--text"});
        } else {
            throw UsageError{"Unbekannter Befehl '" + o.command + "'"};
        }
        const bool takes_file = o.command == "wiederholung" || o.command == "spielstand";
        if (takes_file && !o.file) {
            throw UsageError{"'" + o.command + "' erwartet eine Datei, z. B. uhrwerk-cli " + o.command +
                             " partie.einigkeit"};
        }
        if (!takes_file && o.file) {
            throw UsageError{"Unerwartetes Argument '" + uhrwerk::path_to_utf8(*o.file) + "'"};
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
        if (o.command == "lasttest") {
            return cmd_lasttest(o, data_dir);
        }
        if (o.command == "wirtschaft") {
            return cmd_wirtschaft(o, data_dir);
        }
        if (o.command == "bilanz") {
            return cmd_bilanz(o, data_dir);
        }
        if (o.command == "speichertest") {
            return cmd_speichertest(o, data_dir);
        }
        if (o.command == "wiederholung") {
            return cmd_wiederholung(o, data_dir);
        }
        if (o.command == "spielstand") {
            return cmd_spielstand(o, data_dir);
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
