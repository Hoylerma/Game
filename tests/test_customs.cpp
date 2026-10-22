// Tests der Zollgebiete und Zollunionen (seit M9, docs/technik/diplomatie.md Abschnitt 3).
#include "test_hilfen.hpp"
#include "uhrwerk/customs.hpp"
#include "uhrwerk/economy.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <string_view>
#include <vector>

namespace {

using uhrwerk::Command;
using uhrwerk::CommandType;
using uhrwerk::CustomsDecision;
using uhrwerk::CustomsRequest;
using uhrwerk::Date;
using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::kNone;
using uhrwerk::MarketState;
using uhrwerk::SaveMeta;
using uhrwerk::SimConfig;
using uhrwerk::Simulation;
using uhrwerk::TaxKind;
using uhrwerk::World;
using Bytes = std::vector<std::uint8_t>;

Fixed fx(std::int64_t num, std::int64_t den) {
    return Fixed::from_ratio(num, den);
}

/// Fixture mit zwei Zollunionen: Mittelbund (Leitung Sachsen, Tarif 10 %) und Nordbund (Leitung
/// Brandenburg, 5 %, Anhalt ausgeschlossen). Anhalt (Zoll 2 %) ist ein eigenes Zollgebiet oder, mit
/// anhalt_in_union, Mitglied des Mittelbunds. Den Markt "mitte" teilen sich Sachsen und Anhalt.
class CustomsData {
public:
    explicit CustomsData(bool anhalt_in_union = false) : dir_(true) {
        testhilfen::write_file(dir_ / "common/zollunionen.chron", "zollunion mittelbund {\n"
                                                                  "    name    = \"Mittelbund\"\n"
                                                                  "    leitung = SAX\n"
                                                                  "    tarif   = 0.10\n"
                                                                  "    quelle  = \"Testdaten\"\n"
                                                                  "}\n"
                                                                  "zollunion nordbund {\n"
                                                                  "    name    = \"Nordbund\"\n"
                                                                  "    leitung = BRA\n"
                                                                  "    tarif   = 0.05\n"
                                                                  "    ausgeschlossen = { ANH }\n"
                                                                  "    quelle  = \"Testdaten\"\n"
                                                                  "}\n");
        const char* const mid = "history/staaten/20_mitteldeutschland.chron";
        testhilfen::replace_in_file(dir_ / mid, "    heer           = 6\n",
                                    "    heer           = 6\n    zollunion      = mittelbund\n");
        if (anhalt_in_union) {
            testhilfen::replace_in_file(dir_ / mid, "    heer           = 4\n",
                                        "    heer           = 4\n    zollunion      = mittelbund\n");
        }
        testhilfen::replace_in_file(dir_ / "history/staaten/10_brandenburg.chron",
                                    "    heer           = 10\n",
                                    "    heer           = 10\n    zollunion      = nordbund\n");
    }

    World load() const {
        uhrwerk::LoadResult result = uhrwerk::load_world(dir_.path());
        INFO(testhilfen::issues_text(result));
        REQUIRE(result.ok());
        CHECK(result.issues.empty());
        return std::move(*result.world);
    }

private:
    testhilfen::TempDir dir_;
};

std::vector<Fixed> tariffs_of(const World& w) {
    std::vector<Fixed> tariffs(uhrwerk::customs_zone_count(w));
    for (std::size_t z = 0; z < tariffs.size(); ++z) {
        tariffs[z] = uhrwerk::zone_tariff(w, static_cast<Index>(z));
    }
    return tariffs;
}

const MarketState& market(const World& w, std::string_view id) {
    return w.market_states[static_cast<std::size_t>(w.find_market(id))];
}

Fixed distance(Fixed a, Fixed b) {
    return a > b ? a - b : b - a;
}

Fixed sum(const std::vector<Fixed>& values) {
    Fixed total;
    for (const Fixed v : values) {
        total += v;
    }
    return total;
}

const std::string& last_chronicle(const Simulation& sim) {
    REQUIRE_FALSE(sim.world().chronicle.empty());
    return sim.world().chronicle.back().text;
}

void step_to(Simulation& sim, Date date) {
    while (sim.world().date != date) {
        sim.step_day();
    }
}

Command customs_command(Index state, Index target) {
    Command c;
    c.type = CommandType::CustomsUnion;
    c.state = state;
    c.target = target;
    return c;
}

} // namespace

TEST_CASE("Zoll: Zollgebiete und Anteile je Markt") {
    const World w = CustomsData().load();
    REQUIRE(w.customs_unions.size() == 2);
    const Index sax = w.find_state("SAX");
    const Index anh = w.find_state("ANH");
    const Index bra = w.find_state("BRA");
    CHECK(w.customs_unions[1].excluded == std::vector<Index>{anh});
    CHECK(uhrwerk::customs_zone(w, sax) == 0);
    CHECK(uhrwerk::customs_zone(w, bra) == 1);
    CHECK(uhrwerk::customs_zone(w, anh) == 2 + anh);
    CHECK(uhrwerk::customs_zone_count(w) == 2 + w.states.size());
    CHECK(uhrwerk::zone_tariff(w, 0) == fx(10, 100));
    CHECK(uhrwerk::zone_tariff(w, 2 + anh) == fx(2, 100)); // Zollsatz des Staates

    // Anteile nach der Bevölkerung der Orte mit Daten, Gebiete aufsteigend.
    Fixed sachsen;
    Fixed anhalt;
    const Index mitte = w.find_market("mitte");
    for (std::size_t i = 0; i < w.locations.size(); ++i) {
        const uhrwerk::Location& loc = w.locations[i];
        if (loc.market == mitte && loc.pop_count > 0) {
            (loc.owner == sax ? sachsen : anhalt) += w.location_population(static_cast<Index>(i));
        }
    }
    const MarketState& m = market(w, "mitte");
    REQUIRE(m.zones == std::vector<Index>{0, 2 + anh});
    CHECK(m.zone_shares[0] == sachsen / (sachsen + anhalt));
    CHECK(m.zone_shares[1] == anhalt / (sachsen + anhalt));
    CHECK(uhrwerk::zone_share(m, 0) == m.zone_shares[0]);
    CHECK(uhrwerk::zone_share(m, 1) == Fixed::zero());
    CHECK(market(w, "brandenburg").zones == std::vector<Index>{1});
    CHECK(market(w, "brandenburg").zone_shares == std::vector<Fixed>{Fixed::one()});
}

TEST_CASE("Zoll: erwarteter Tarif zwischen Märkten und vom Weltmarkt") {
    SUBCASE("gemischter Markt") {
        const World w = CustomsData().load();
        const std::vector<Fixed> tariffs = tariffs_of(w);
        const MarketState& m = market(w, "mitte");
        const MarketState& b = market(w, "brandenburg");
        const Fixed one = Fixed::one();
        const Fixed t_sax = fx(10, 100);
        const Fixed t_anh = fx(2, 100);
        // Waren aus Brandenburg: beide Gebiete der Mitte sind fremd.
        CHECK(uhrwerk::customs_tariff(&b, m, tariffs) ==
              m.zone_shares[0] * one * t_sax + m.zone_shares[1] * one * t_anh);
        // Innerhalb des Marktes zählen die Grenzen nicht für den Handel, die Formel ergibt den Anteil über
        // sie.
        const Fixed inner = m.zone_shares[0] * (one - m.zone_shares[0]) * t_sax +
                            m.zone_shares[1] * (one - m.zone_shares[1]) * t_anh;
        CHECK(uhrwerk::customs_tariff(&m, m, tariffs) == inner);
        CHECK(uhrwerk::customs_tariff(nullptr, m, tariffs) ==
              m.zone_shares[0] * one * t_sax + m.zone_shares[1] * one * t_anh);
        CHECK(uhrwerk::customs_tariff(&m, b, tariffs) == fx(5, 100));
    }
    SUBCASE("innerhalb einer Union kein Zoll") {
        const World w = CustomsData(true).load();
        const std::vector<Fixed> tariffs = tariffs_of(w);
        const MarketState& m = market(w, "mitte");
        REQUIRE(m.zones == std::vector<Index>{0});
        CHECK(uhrwerk::customs_tariff(&m, m, tariffs) == Fixed::zero());
        CHECK(uhrwerk::customs_tariff(nullptr, m, tariffs) == fx(10, 100));
    }
}

TEST_CASE("Zoll: Handel über die Zollgrenze und fremder Einfuhranteil") {
    World plain = testhilfen::load_fixture();
    // Nach dem Einschwingen liegen die Preise innerhalb der Fracht; teurere Mitte, damit gehandelt wird.
    for (Fixed& price : plain.market_states[static_cast<std::size_t>(plain.find_market("mitte"))].price) {
        price = price * fx(13, 10);
    }
    const Index sax = plain.find_state("SAX");
    const Index anh = plain.find_state("ANH");
    const Index bra = plain.find_state("BRA");
    const uhrwerk::EconomyTopology topology = uhrwerk::build_economy_topology(plain);
    const auto with_unions = [&](bool together) {
        World w = plain;
        w.customs_unions = {uhrwerk::CustomsUnion{"mittelbund", "Mittelbund", sax, fx(10, 100), {}, ""},
                            uhrwerk::CustomsUnion{"nordbund", "Nordbund", bra, fx(10, 100), {}, ""}};
        w.states[static_cast<std::size_t>(sax)].customs_union = 0;
        w.states[static_cast<std::size_t>(anh)].customs_union = 0;
        w.states[static_cast<std::size_t>(bra)].customs_union = together ? 0 : 1;
        uhrwerk::update_markets(w, topology, uhrwerk::state_spending(w));
        return w;
    };
    const World apart = with_unions(false);
    const World together = with_unions(true);
    uhrwerk::update_markets(plain, topology, uhrwerk::state_spending(plain));

    const MarketState& mitte_plain = market(plain, "mitte");
    REQUIRE(sum(mitte_plain.imports) > Fixed::zero());
    // Eine Zollgrenze zwischen den Märkten verringert den Handel, eine gemeinsame Union nicht.
    CHECK(sum(market(apart, "mitte").imports) < sum(mitte_plain.imports) * fx(9, 10));
    CHECK(market(together, "mitte").imports == mitte_plain.imports);

    // Fremder Einfuhranteil: Alles aus Brandenburg ist fremd, solange es ein anderes Gebiet ist; in einer
    // gemeinsamen Union nichts (die Mitte hat keinen Hafen).
    const MarketState& m = market(apart, "mitte");
    REQUIRE(m.zones == std::vector<Index>{0});
    CHECK(m.foreign_share == m.import_share);
    const MarketState& t = market(together, "mitte");
    CHECK(sum(t.import_share) > Fixed::zero());
    CHECK(t.foreign_share == std::vector<Fixed>(plain.goods.size(), Fixed::zero()));
}

TEST_CASE("Zoll: Vereinskasse nach Bevölkerung und Zoll im Haushalt") {
    Simulation sim(CustomsData(true).load());
    const World& w = sim.world();
    const Index sax = w.find_state("SAX");
    const Index anh = w.find_state("ANH");
    const Index bra = w.find_state("BRA");
    const auto zoll = [&](Index state) {
        Fixed total;
        for (std::size_t i = 0; i < w.locations.size(); ++i) {
            if (w.locations[i].owner == state && w.locations[i].pop_count > 0) {
                total += w.economy[i].taxes[static_cast<std::size_t>(TaxKind::Zoll)];
            }
        }
        return total;
    };
    // Beim Start schon verteilt (estimate_budgets).
    REQUIRE(w.customs_revenue.size() == 2);
    CHECK(w.customs_revenue[0] == zoll(sax) + zoll(anh));
    CHECK(w.customs_revenue[1] == zoll(bra));
    const Fixed share_sax = w.states[static_cast<std::size_t>(sax)].customs_income;
    const Fixed share_anh = w.states[static_cast<std::size_t>(anh)].customs_income;
    CHECK(share_sax > share_anh);
    CHECK(distance(share_sax + share_anh, w.customs_revenue[0]) <= fx(1, 100));
    const Fixed people_sax = w.state_population(sax);
    const Fixed people_anh = w.state_population(anh);
    CHECK(distance(share_sax / (share_sax + share_anh), people_sax / (people_sax + people_anh)) <=
          fx(1, 1000));

    // Im Haushalt steht der Anteil statt des Zolls der eigenen Orte (Rechentag der Staaten bis Tag 3).
    step_to(sim, Date{1648, 11, 4});
    for (const Index s : {sax, anh, bra}) {
        const uhrwerk::State& state = w.states[static_cast<std::size_t>(s)];
        CHECK(state.budget.taxes[static_cast<std::size_t>(TaxKind::Zoll)] == state.customs_income);
    }
}

TEST_CASE("Zoll: Befehl Zollunion zum 1. Januar") {
    Simulation sim(CustomsData().load());
    const World& w = sim.world();
    const Index sax = w.find_state("SAX");
    const Index anh = w.find_state("ANH");
    const Index bra = w.find_state("BRA");
    const auto membership = [&](Index s) { return w.states[static_cast<std::size_t>(s)].customs_union; };

    SUBCASE("Beitritt, Austritt und Rücknahme") {
        sim.submit(customs_command(anh, 0));
        sim.step_day();
        CHECK(last_chronicle(sim) == "Anhalt tritt zum 1. Januar 1649 dem Mittelbund bei.");
        CHECK(w.customs_requests == std::vector<CustomsRequest>{{anh, 0}});
        step_to(sim, Date{1649, 1, 1});
        CHECK(membership(anh) == kNone);
        sim.step_day(); // der 1. Januar
        CHECK(membership(anh) == 0);
        CHECK(w.customs_requests.empty());
        CHECK(market(w, "mitte").zones == std::vector<Index>{0}); // gleich am 1. Januar im Markt

        sim.submit(customs_command(anh, kNone));
        sim.step_day();
        CHECK(last_chronicle(sim) == "Anhalt tritt zum 1. Januar 1650 aus dem Mittelbund aus.");
        const std::size_t entries = w.chronicle.size();
        sim.submit(customs_command(anh, 0)); // zurück zur bisherigen Union: Antrag zurückgezogen
        sim.step_day();
        CHECK(w.customs_requests.empty());
        CHECK(w.chronicle.size() == entries);
    }
    SUBCASE("Leitmacht, Ausschluss und ungültige Befehle") {
        const std::size_t entries = w.chronicle.size();
        sim.submit(customs_command(sax, kNone)); // die Leitmacht bleibt
        sim.submit(customs_command(bra, 0));     // auch beim Wechsel
        sim.submit(customs_command(anh, 2));     // keine solche Union
        sim.submit(customs_command(-1, 0));      // kein solcher Staat
        sim.submit(customs_command(anh, kNone)); // schon ohne Union
        sim.step_day();
        CHECK(w.chronicle.size() == entries);
        CHECK(w.customs_requests.empty());

        sim.submit(customs_command(anh, 1));
        sim.step_day();
        CHECK(last_chronicle(sim) == "Brandenburg lehnt den Beitritt von Anhalt zum Nordbund ab.");
        CHECK(w.customs_requests.empty());
    }
    SUBCASE("Wechsel zwischen zwei Unionen") {
        World changed = CustomsData(true).load();
        changed.customs_unions[1].excluded.clear();
        Simulation other(std::move(changed));
        other.submit(customs_command(anh, 1));
        other.step_day();
        CHECK(last_chronicle(other) ==
              "Anhalt verlässt zum 1. Januar 1649 den Mittelbund und tritt dem Nordbund bei.");
        step_to(other, Date{1649, 1, 2});
        CHECK(other.world().states[static_cast<std::size_t>(anh)].customs_union == 1);
    }
    SUBCASE("Mitglieder erheben den Vereinstarif") {
        sim.submit(Command{CommandType::SetTaxRate, sax, fx(20, 100), TaxKind::Zoll});
        sim.submit(Command{CommandType::SetTaxRate, anh, fx(20, 100), TaxKind::Zoll});
        sim.submit(Command{CommandType::SetTaxRate, sax, fx(20, 100), TaxKind::Akzise});
        sim.step_day();
        CHECK(w.states[static_cast<std::size_t>(sax)].tax_rate(TaxKind::Zoll) == fx(4, 100));
        CHECK(w.states[static_cast<std::size_t>(anh)].tax_rate(TaxKind::Zoll) == fx(20, 100));
        CHECK(w.states[static_cast<std::size_t>(sax)].tax_rate(TaxKind::Akzise) == fx(20, 100));
        CHECK(uhrwerk::zone_tariff(w, uhrwerk::customs_zone(w, anh)) == fx(20, 100));
    }
}

TEST_CASE("Zoll: Spielstand, Wiederholung und Threads mit beantragtem Beitritt") {
    const CustomsData data;
    const auto run = [&](unsigned threads, std::int64_t days) {
        SimConfig config;
        config.threads = threads;
        Simulation sim(data.load(), config);
        sim.submit(customs_command(sim.world().find_state("ANH"), 0));
        sim.step_days(days);
        return sim;
    };
    const Simulation one = run(1, 30);
    REQUIRE(one.world().customs_requests.size() == 1);
    const Bytes bytes = uhrwerk::save_game(one, SaveMeta{});
    uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, data.load(), 2);
    INFO(loaded.error);
    REQUIRE(loaded.ok());
    CHECK(loaded.simulation->checksum() == one.checksum());
    CHECK(loaded.simulation->world().customs_requests == one.world().customs_requests);

    Simulation continued = one;
    continued.step_days(120);
    loaded.simulation->step_days(120);
    CHECK(loaded.simulation->checksum() == continued.checksum());
    CHECK(continued.world().customs_requests.empty());
    CHECK(continued.world()
              .states[static_cast<std::size_t>(continued.world().find_state("ANH"))]
              .customs_union == 0);
    CHECK(run(4, 150).checksum() == continued.checksum());

    const uhrwerk::Replay replay = uhrwerk::make_replay(continued, {});
    const uhrwerk::ReplayResult result = uhrwerk::run_replay(replay, data.load(), 1);
    INFO(result.error);
    CHECK(result.ok);
}
