// Tests für Spielstände und Wiederholungen (seit M6, docs/technik/speicherstaende.md, Abschnitt 10).

#include "test_hilfen.hpp"
#include "uhrwerk/lasttest.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

#include <algorithm>
#include <cstdint>
#include <doctest/doctest.h>
#include <set>
#include <string>
#include <vector>

namespace {

using uhrwerk::Checkpoint;
using uhrwerk::Command;
using uhrwerk::Date;
using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::kNone;
using uhrwerk::LoadedGame;
using uhrwerk::Replay;
using uhrwerk::ReplayResult;
using uhrwerk::SaveMeta;
using uhrwerk::SimConfig;
using uhrwerk::Simulation;
using uhrwerk::TaxKind;
using uhrwerk::World;

using Bytes = std::vector<std::uint8_t>;

/// Befehle eines Tages im Testablauf: alle paar Tage ein Steuerbefehl, darunter ungültige.
void submit_for_day(Simulation& sim, std::int64_t day) {
    if (day % 23 == 5) {
        const auto state = static_cast<Index>(day % 3);
        const auto kind = static_cast<TaxKind>((day / 23) % 3);
        sim.submit(
            Command{uhrwerk::CommandType::SetTaxRate, state, Fixed::from_ratio(5 + day % 20, 100), kind});
    }
    if (day % 97 == 40) {
        sim.submit(Command{uhrwerk::CommandType::SetTaxRate, 7, Fixed::from_ratio(1, 10), TaxKind::Akzise});
    }
}

/// Läuft bis Tag end; Befehle vor jedem Schritt (der Tag, an dem sim steht, ist schon eingereiht,
/// wenn skip_first gesetzt ist).
void run_until(Simulation& sim, std::int64_t end, bool skip_first = false) {
    bool first = skip_first;
    while (sim.days_elapsed() < end) {
        if (!first) {
            submit_for_day(sim, sim.days_elapsed());
        }
        first = false;
        sim.step_day();
    }
}

LoadedGame load(const Bytes& bytes, unsigned threads = 1) {
    return uhrwerk::load_game(bytes, testhilfen::load_fixture(), threads);
}

std::string error_of(const Bytes& bytes) {
    const LoadedGame loaded = load(bytes);
    CHECK_FALSE(loaded.ok());
    return loaded.error;
}

bool contains(const std::string& text, std::string_view part) {
    return text.find(part) != std::string::npos;
}

/// Ersetzt im Metadatenteil "schluessel=wert" den Wert (gleiche Länge, damit der Kopf passt).
void patch_meta(Bytes& bytes, std::string_view key, std::string_view value) {
    const std::string needle = "\n" + std::string(key) + "=";
    const auto it = std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end());
    REQUIRE(it != bytes.end());
    const auto start = it + static_cast<std::ptrdiff_t>(needle.size());
    const auto end = std::find(start, bytes.end(), static_cast<std::uint8_t>('\n'));
    REQUIRE(static_cast<std::size_t>(end - start) == value.size());
    std::copy(value.begin(), value.end(), start);
}

Simulation played_fixture(std::int64_t days) {
    Simulation sim(testhilfen::load_fixture());
    run_until(sim, days);
    return sim;
}

} // namespace

TEST_CASE("Spielstand: Speichern, Laden, Speichern ergibt dieselben Bytes") {
    Simulation sim = played_fixture(200);
    submit_for_day(sim, 5); // wartender Befehl im Spielstand
    const Bytes first = uhrwerk::save_game(sim, SaveMeta{"BRA"});
    CHECK(uhrwerk::save_game(sim, SaveMeta{"BRA"}) == first);

    const LoadedGame loaded = load(first);
    INFO(loaded.error);
    REQUIRE(loaded.ok());
    CHECK(loaded.simulation->checksum() == sim.checksum());
    CHECK(loaded.simulation->state_text() == sim.state_text());
    CHECK(loaded.simulation->start_checksum() == sim.start_checksum());
    CHECK(loaded.simulation->command_log().size() == sim.command_log().size());
    CHECK(uhrwerk::save_game(*loaded.simulation, SaveMeta{"BRA"}) == first);

    const auto info = uhrwerk::read_save_info(first);
    REQUIRE(info.has_value());
    CHECK(info->format_version == uhrwerk::kSaveFormatVersion);
    CHECK(info->scenario == "westfaelischer_frieden");
    CHECK(info->player_tag == "BRA");
    CHECK(info->date == sim.world().date);
    CHECK(info->days == 200);
    CHECK(info->seed == 1648);
    CHECK(info->checksum == sim.checksum());
    CHECK(info->start_checksum == sim.start_checksum());
    CHECK(info->commands == sim.command_log().size());
    CHECK(info->content == uhrwerk::content_fingerprint(sim.world()));
    CHECK(info->compressed);
    CHECK(info->stored_size < info->payload_size);
}

TEST_CASE("Spielstand: Weiterspielen nach dem Laden gleicht dem durchgehenden Lauf") {
    constexpr std::int64_t kEnd = 800;
    Simulation reference = played_fixture(kEnd);

    // Speicherpunkte: Monatserste, Jahreswechsel, Rechentage, Monatsmitte (mit Zugängen) und Tage
    // mit wartenden Befehlen (Tag % 23 == 5).
    const std::set<std::int64_t> points = {0,   1,   5,   7,   8,   28,  51,  69, 70,
                                           100, 189, 190, 213, 250, 365, 420, 799};
    Simulation sim(testhilfen::load_fixture());
    bool saw_pending = false;
    while (sim.days_elapsed() < kEnd) {
        const std::int64_t day = sim.days_elapsed();
        submit_for_day(sim, day);
        if (points.count(day) != 0) {
            const Bytes bytes = uhrwerk::save_game(sim, SaveMeta{});
            saw_pending = saw_pending || (!sim.command_log().empty() && sim.command_log().back().day == day);
            for (const unsigned threads : {1u, 4u}) {
                CAPTURE(day);
                CAPTURE(threads);
                LoadedGame loaded = load(bytes, threads);
                INFO(loaded.error);
                REQUIRE(loaded.ok());
                run_until(*loaded.simulation, kEnd, true);
                CHECK(loaded.simulation->checksum() == reference.checksum());
            }
        }
        sim.step_day();
    }
    CHECK(sim.checksum() == reference.checksum());
    CHECK(saw_pending); // sonst prüft der Test die wartenden Befehle nicht
}

TEST_CASE("Spielstand: Zugänge in den Spieldaten") {
    // In der Fixture findet jeder Zuzug einen passenden Block; in den Spieldaten entstehen Zugänge,
    // die bis zur Neuordnung am Monatsersten getrennt liegen.
    uhrwerk::LoadResult data = uhrwerk::load_world(UHRWERK_GAME_DATA_DIR);
    REQUIRE(data.ok());
    const World base = std::move(*data.world);
    Simulation reference(base);
    reference.step_days(20);
    std::size_t arrivals = 0;
    for (const auto& list : reference.world().arrivals) {
        arrivals += list.size();
    }
    REQUIRE(arrivals > 0);
    const Bytes bytes = uhrwerk::save_game(reference, SaveMeta{});
    LoadedGame loaded = uhrwerk::load_game(bytes, base, 2);
    INFO(loaded.error);
    REQUIRE(loaded.ok());
    CHECK(loaded.simulation->state_text() == reference.state_text());
    reference.step_days(40);
    loaded.simulation->step_days(40);
    CHECK(loaded.simulation->checksum() == reference.checksum());
}

TEST_CASE("Spielstand: beschädigte und fremde Dateien werden abgelehnt") {
    Simulation sim = played_fixture(60);
    const Bytes good = uhrwerk::save_game(sim, SaveMeta{"SAX"});
    REQUIRE(load(good).ok());

    SUBCASE("fremde Datei") {
        Bytes bytes = good;
        bytes[0] = 'X';
        CHECK(contains(error_of(bytes), "kein Spielstand"));
        CHECK(contains(error_of(Bytes{}), "kein Spielstand"));
    }
    SUBCASE("Protokoll statt Spielstand") {
        const Bytes replay = uhrwerk::write_replay(uhrwerk::make_replay(sim));
        CHECK(contains(error_of(replay), "Protokoll, kein Spielstand"));
        std::string error;
        CHECK_FALSE(uhrwerk::read_replay(good, &error).has_value());
        CHECK(contains(error, "Spielstand, kein Protokoll"));
    }
    SUBCASE("neuere Formatversion") {
        Bytes bytes = good;
        bytes[8] = static_cast<std::uint8_t>(uhrwerk::kSaveFormatVersion + 1);
        CHECK(contains(error_of(bytes), "neueren Spielversion"));
    }
    SUBCASE("abgeschnitten") {
        Bytes bytes = good;
        bytes.pop_back();
        CHECK(contains(error_of(bytes), "unvollständig"));
        bytes.resize(20);
        CHECK(contains(error_of(bytes), "unvollständig"));
    }
    SUBCASE("ein Byte der Nutzdaten verändert") {
        for (const std::size_t from_end : {std::size_t{1}, std::size_t{100}, good.size() / 3}) {
            Bytes bytes = good;
            bytes[bytes.size() - from_end] = static_cast<std::uint8_t>(bytes[bytes.size() - from_end] ^ 0x10);
            CHECK(contains(error_of(bytes), "beschädigt"));
        }
    }
    SUBCASE("Prüfsumme in den Metadaten passt nicht zum Zustand") {
        Bytes bytes = good;
        patch_meta(bytes, "pruefsumme", "0123456789abcdef");
        CHECK(contains(error_of(bytes), "Prüfsumme"));
    }
    SUBCASE("Metadaten passen nicht zum Zustand") {
        Bytes bytes = good;
        patch_meta(bytes, "tage", "61");
        CHECK(contains(error_of(bytes), "Metadaten passen nicht"));
        bytes = good;
        patch_meta(bytes, "seed", "1649");
        CHECK(contains(error_of(bytes), "Metadaten passen nicht"));
    }
    SUBCASE("Metadaten unvollständig") {
        Bytes bytes = good;
        patch_meta(bytes, "inhalt.orte", "0000000000000000"); // Gesamtwert passt nicht mehr
        CHECK(contains(error_of(bytes), "inhalt"));
        std::string error;
        CHECK_FALSE(uhrwerk::read_save_info(bytes, &error).has_value());
        CHECK(contains(error, "Metadaten beschädigt"));
    }
    SUBCASE("ungültiger Verweis im Zustand") {
        World w = testhilfen::load_fixture();
        uhrwerk::ChronicleEntry entry;
        entry.date = w.date;
        entry.text = "Verweis ins Leere";
        entry.location = 9999;
        w.chronicle.push_back(entry);
        const Bytes bytes = uhrwerk::save_game(Simulation(std::move(w)), SaveMeta{});
        CHECK(contains(error_of(bytes), "Chronik mit ungültigem Verweis"));
    }
}

TEST_CASE("Spielstand: Inhaltsfingerabdruck") {
    const World base = testhilfen::load_fixture();
    const auto fp = uhrwerk::content_fingerprint(base);

    SUBCASE("Anzeigenamen, Farben, Quellen und Zustand zählen nicht") {
        World w = base;
        w.states[0].name = "Kurbrandenburg";
        w.states[0].color = uhrwerk::Color{1, 2, 3};
        w.states[0].treasury += Fixed::from_int(1000);
        w.locations[0].source = "andere Quelle";
        w.goods[0].name = "Korn";
        CHECK(uhrwerk::content_fingerprint(w) == fp);
    }
    SUBCASE("Namen aus Chronikeinträgen zählen: Kurzname der Staaten, Ortsname") {
        // Die Chronik steht in der Prüfsumme; ein anderer Name ergäbe eine andere Wiederholung.
        World w = base;
        w.states[0].short_name = "Kurbrandenburg";
        w.locations[0].name = "Anderswo";
        const auto parts = uhrwerk::content_differences(fp, uhrwerk::content_fingerprint(w));
        CHECK(parts == std::vector<std::string_view>{"staaten", "orte"});
    }
    SUBCASE("Änderungen werden ihrem Bereich zugeordnet") {
        World w = base;
        w.locations[3].longitude += Fixed::from_ratio(1, 100);
        w.professions[1].literacy += Fixed::from_ratio(1, 100);
        w.rules.trade_margin += Fixed::from_ratio(1, 100);
        const auto changed = uhrwerk::content_fingerprint(w);
        CHECK(changed.total != fp.total);
        const auto parts = uhrwerk::content_differences(fp, changed);
        CHECK(parts == std::vector<std::string_view>{"regeln", "berufe", "orte"});
    }
    SUBCASE("Laden in anderen Inhalt wird mit den Bereichen abgelehnt") {
        const Bytes bytes = uhrwerk::save_game(Simulation(base), SaveMeta{});
        World other = base;
        other.terrains[0].yield_factor += Fixed::from_ratio(1, 10);
        const LoadedGame loaded = uhrwerk::load_game(bytes, std::move(other), 1);
        CHECK_FALSE(loaded.ok());
        CHECK(contains(loaded.error, "passt nicht zu den Spieldaten (abweichend: gelaende)"));
    }
}

TEST_CASE("Spielstand: Lasttest-Welt") {
    const World base = uhrwerk::make_load_test_world(testhilfen::load_fixture(), {3000, 7});
    Simulation sim(base, SimConfig{7, 2});
    sim.step_days(40);
    const Bytes bytes = uhrwerk::save_game(sim, SaveMeta{});
    LoadedGame loaded = uhrwerk::load_game(bytes, base, 3);
    INFO(loaded.error);
    REQUIRE(loaded.ok());
    sim.step_days(40);
    loaded.simulation->step_days(40);
    CHECK(loaded.simulation->checksum() == sim.checksum());
    // Die Lasttest-Welt ändert nur den Zustand (Besitzer, Blöcke, Verwüstung), nicht den Inhalt: Der
    // Spielstand lässt sich daher auch in die Fixture laden.
    LoadedGame into_fixture = load(bytes);
    REQUIRE(into_fixture.ok());
    into_fixture.simulation->step_days(40);
    CHECK(into_fixture.simulation->checksum() == sim.checksum());
}

TEST_CASE("Spielstand: Formatwache") {
    // Ändert sich dieser Wert, hat sich das Zustandsverzeichnis oder die Kodierung geändert: Dann
    // kSaveFormatVersion erhöhen (savegame.hpp) und den Wert neu festhalten. Haben sich nur die
    // Testdaten (tests/daten) oder die Simulation geändert, genügt es, den Wert neu festzuhalten.
    Simulation sim(testhilfen::load_fixture());
    sim.step_days(365);
    const auto info = uhrwerk::read_save_info(uhrwerk::save_game(sim, SaveMeta{}));
    REQUIRE(info.has_value());
    CHECK_MESSAGE(uhrwerk::hex16(info->payload_hash) == "00426c891b5034de",
                  "Prüfwert der Nutzdaten geändert: Formatversion erhöhen oder Referenzwert neu festhalten");
}

TEST_CASE("Wiederholung: Protokoll mit Kontrollpunkten rechnet bitgleich nach") {
    Simulation sim(testhilfen::load_fixture());
    std::vector<Checkpoint> checkpoints;
    while (sim.days_elapsed() < 900) {
        submit_for_day(sim, sim.days_elapsed());
        sim.step_day();
        if (uhrwerk::is_checkpoint_day(sim)) {
            checkpoints.push_back(Checkpoint{sim.days_elapsed(), sim.checksum()});
        }
    }
    submit_for_day(sim, sim.days_elapsed()); // bleibt wartend
    REQUIRE(checkpoints.size() == 3);        // 1649, 1650, 1651
    const Replay replay = uhrwerk::make_replay(sim, checkpoints);
    CHECK(replay.commands == sim.command_log());

    const Bytes file = uhrwerk::write_replay(replay);
    const auto read = uhrwerk::read_replay(file);
    REQUIRE(read.has_value());
    CHECK(read->commands == replay.commands);
    CHECK(read->checkpoints == replay.checkpoints);
    CHECK(read->content == replay.content);
    CHECK(read->end_day == 900);
    CHECK(read->end_checksum == sim.checksum());
    CHECK(read->start_checksum == sim.start_checksum());
    CHECK(read->scenario == "westfaelischer_frieden");

    for (const unsigned threads : {1u, 4u}) {
        const ReplayResult result = uhrwerk::run_replay(*read, testhilfen::load_fixture(), threads);
        INFO(result.error);
        CHECK(result.ok);
        CHECK(result.checkpoints == 4); // mit dem Endstand
        CHECK(result.mismatch_day == -1);
    }

    SUBCASE("aus einem Spielstand") {
        const auto from_save = uhrwerk::replay_from_save(uhrwerk::save_game(sim, SaveMeta{}));
        REQUIRE(from_save.has_value());
        CHECK(from_save->commands == sim.command_log());
        CHECK(from_save->checkpoints.empty());
        const ReplayResult result = uhrwerk::run_replay(*from_save, testhilfen::load_fixture(), 2);
        INFO(result.error);
        CHECK(result.ok);
    }
    SUBCASE("abweichender Befehl: erster abweichender Kontrollpunkt") {
        Replay changed = replay;
        changed.commands[1].command.value += Fixed::from_ratio(1, 100);
        const ReplayResult result = uhrwerk::run_replay(changed, testhilfen::load_fixture(), 1);
        CHECK_FALSE(result.ok);
        CHECK(result.mismatch_day == checkpoints[0].day);
        CHECK(result.expected == checkpoints[0].checksum);
        CHECK(result.actual != result.expected);
    }
    SUBCASE("verfälschter Kontrollpunkt") {
        Replay changed = replay;
        changed.checkpoints[1].checksum ^= 1;
        const ReplayResult result = uhrwerk::run_replay(changed, testhilfen::load_fixture(), 1);
        CHECK_FALSE(result.ok);
        CHECK(result.mismatch_day == checkpoints[1].day);
        CHECK(result.checkpoints == 2);
    }
    SUBCASE("anderer Startzustand oder Inhalt") {
        Replay changed = replay;
        changed.seed += 1;
        CHECK(contains(uhrwerk::run_replay(changed, testhilfen::load_fixture(), 1).error,
                       "Startzustand weicht ab"));
        World other = testhilfen::load_fixture();
        other.goods[0].base_price += Fixed::from_int(1);
        CHECK(contains(uhrwerk::run_replay(replay, std::move(other), 1).error, "abweichend: gueter"));
    }
}

TEST_CASE("Dateien: schreiben und lesen") {
    testhilfen::TempDir dir;
    const Bytes bytes = uhrwerk::save_game(played_fixture(3), SaveMeta{});
    std::string error;
    REQUIRE(uhrwerk::write_file(dir / "a.einigkeit", bytes, &error));
    CHECK_FALSE(std::filesystem::exists(dir / "a.einigkeit.tmp"));
    const auto read = uhrwerk::read_file(dir / "a.einigkeit", &error);
    REQUIRE(read.has_value());
    CHECK(*read == bytes);
    CHECK_FALSE(uhrwerk::read_file(dir / "fehlt.einigkeit", &error).has_value());
    CHECK(contains(error, "fehlt.einigkeit"));
    CHECK_FALSE(uhrwerk::write_file(dir / "kein/ordner/b.einigkeit", bytes, &error));
}
