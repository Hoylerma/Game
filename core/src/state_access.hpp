#pragma once

// Zustandsverzeichnis (seit M6, docs/technik/speicherstaende.md, Abschnitt 2) – intern im Kern.
//
// StateAccess::visit gibt jedes Feld des veränderlichen Zustands in fester Reihenfolge an einen
// Besucher: die Prüfsumme (HashVisitor), den Textexport (TextVisitor) und Spielstände (Schreib- und
// Lesebesucher in savegame.cpp). Eine Liste für alle Zwecke: Was gespeichert wird, steht in der
// Prüfsumme, und umgekehrt. Reihenfolge und Kodierung entsprechen der Prüfsumme aus M5, die
// Referenzprüfsummen bleiben gleich.
//
// Kodierung je Feld: Festkomma als i64 (Rohwert), vorzeichenbehaftete Ganzzahlen als i64,
// vorzeichenlose und Aufzählungen als u64, Datum als drei i64, Zeichenketten als u64-Länge plus
// Bytes, Zufallsströme als zwei u64. Vektoren beginnen mit ihrer Länge (u64), feste Felder
// (std::array) nicht.

#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace uhrwerk {

/// Größe eines Vektors beim Lesen: Inhalt (Staaten, Orte) muss zur geladenen Welt passen, Zustand
/// (Blöcke, Zugänge, Chronik …) bestimmt seine Größe selbst.
enum class Extent { Content, State };

struct StateAccess {
    /// Alle Felder des Zustands; Sim ist Simulation oder const Simulation.
    template <typename Sim, typename V> static void visit(Sim& sim, V& v);

    /// Nach dem Lesen eines Spielstands (savegame.cpp): Protokoll und Startprüfsumme setzen,
    /// abgeleitete Hilfsdaten aus dem gelesenen Zustand neu aufbauen.
    static void finish_load(Simulation& sim, std::vector<LoggedCommand> log, std::uint64_t start_checksum) {
        sim.command_log_ = std::move(log);
        sim.start_checksum_ = start_checksum;
        sim.rebuild_derived();
    }

    /// Zufallsströme mit ungeradem Inkrement (Pcg32::restore überlässt die Prüfung dem Aufrufer).
    static bool streams_valid(const Simulation& sim) {
        return (sim.rng_demography_.increment() & 1U) != 0 && (sim.rng_economy_.increment() & 1U) != 0 &&
               (sim.rng_events_.increment() & 1U) != 0;
    }

private:
    /// Vektor: Länge, dann je Element enter/fn/leave.
    template <typename V, typename Vec, typename Fn>
    static void list(V& v, std::string_view name, Vec& items, Extent extent, Fn&& fn) {
        if (!v.size(name, items, extent)) {
            return;
        }
        for (std::size_t i = 0; i < items.size(); ++i) {
            v.enter(name, i);
            fn(items[i]);
            v.leave();
        }
    }

    template <typename V, typename Vec> static void values(V& v, std::string_view name, Vec& items) {
        list(v, name, items, Extent::State, [&](auto& x) { v.field("", x); });
    }

    template <typename V, typename Pop> static void pop(V& v, Pop& p) {
        v.field("ort", p.location);
        v.field("beruf", p.profession);
        v.field("konfession", p.confession);
        v.field("kultur", p.culture);
        v.field("kinder", p.children);
        v.field("junge", p.young);
        v.field("erwerbsfaehige", p.working);
        v.field("alte", p.old);
        v.field("lebensstandard", p.living_standard);
        v.field("alphabetisierung", p.literacy);
    }

    template <typename V, typename B> static void balance(V& v, B& b) {
        v.field("geburten", b.births);
        v.field("sterbefaelle", b.deaths);
        v.field("seuchentote", b.plague_deaths);
        v.field("zuzug", b.moved_in);
        v.field("wegzug", b.moved_out);
        v.field("auswanderung", b.emigrated);
    }
};

template <typename Sim, typename V> void StateAccess::visit(Sim& sim, V& v) {
    auto& w = sim.world_;
    v.field("datum", w.date);
    v.field("tage", sim.days_elapsed_);
    v.field("seed", sim.config_.seed);

    list(v, "staaten", w.states, Extent::Content, [&](auto& s) {
        v.field("schatz", s.treasury);
        v.field("steuersaetze", s.tax_rates);
        v.field("heer", s.army_per_mille);
        v.field("schulden", s.debt);
        v.field("haushalt.steuern", s.budget.taxes);
        v.field("haushalt.hof", s.budget.court);
        v.field("haushalt.verwaltung", s.budget.administration);
        v.field("haushalt.heer", s.budget.army);
        v.field("haushalt.zinsen", s.budget.interest);
        v.field("haushalt.bauten", s.budget.building);
        v.field("jahreseinnahmen", s.yearly_income);
        v.field("zins", s.interest_rate);
        v.field("zinsaufschlag_monate", s.premium_months);
        v.field("bankrotte", s.bankruptcies);
        v.field("hofhaltung", s.court_expenses);
        v.field("einnahmen_vormonat", s.last_month_income);
        v.field("ausgaben_vormonat", s.last_month_expenses);
        v.field("hauptstadt", s.capital);
    });

    list(v, "orte", w.locations, Extent::Content, [&](auto& l) {
        v.field("besitzer", l.owner);
        v.field("verwuestung", l.devastation);
        v.field("erster_block", l.first_pop);
        v.field("bloecke", l.pop_count);
    });

    list(v, "bloecke", w.pops, Extent::State, [&](auto& p) { pop(v, p); });
    list(v, "zugaenge", w.arrivals, Extent::State,
         [&](auto& arrivals) { list(v, "block", arrivals, Extent::State, [&](auto& p) { pop(v, p); }); });

    list(v, "momentaufnahmen", w.snapshots, Extent::State, [&](auto& s) {
        v.field("bevoelkerung", s.population);
        v.field("landreserve", s.reserve);
    });
    values(v, "ziel_lebensstandard", w.snapshot_targets);
    list(v, "bilanz_laufend", w.balance_current, Extent::State, [&](auto& b) { balance(v, b); });
    list(v, "bilanz_vorjahr", w.balance_previous, Extent::State, [&](auto& b) { balance(v, b); });
    list(v, "ortswirtschaft", w.economy, Extent::State, [&](auto& e) {
        v.field("leistung", e.production);
        v.field("steuern", e.taxes);
        v.field("haendler", e.traders);
    });

    // Wirtschaft (seit M5)
    values(v, "betriebsanteile", w.business_shares);
    values(v, "angebot", w.location_supply);
    values(v, "nachfrage", w.location_demand);
    values(v, "einkommen", w.location_income);
    values(v, "bedarfsdeckung", w.location_coverage);
    values(v, "landesausbau", w.location_development);
    list(v, "maerkte", w.market_states, Extent::State, [&](auto& m) {
        values(v, "preis", m.price);
        values(v, "kaufquote", m.buy_ratio);
        values(v, "verkaufsquote", m.sell_ratio);
        values(v, "einfuhranteil", m.import_share);
        values(v, "angebot", m.supply);
        values(v, "nachfrage", m.demand);
        values(v, "einfuhr", m.imports);
        values(v, "ausfuhr", m.exports);
        v.field("spanne_je_haendler", m.margin_per_trader);
        v.field("ernte", m.harvest);
    });

    // Zollgebiete (seit M9): nur in Welten mit Zollunionen, damit die Prüfsumme der übrigen (1648) bleibt.
    if (!w.customs_unions.empty()) {
        list(v, "zollgebiete", w.states, Extent::Content, [&](auto& s) {
            v.field("zollunion", s.customs_union);
            v.field("vereinsanteil", s.customs_income);
        });
        values(v, "vereinskasse", w.customs_revenue);
        list(v, "zollantraege", w.customs_requests, Extent::State, [&](auto& r) {
            v.field("staat", r.state);
            v.field("zollunion", r.customs_union);
        });
        list(v, "marktzoll", w.market_states, Extent::State, [&](auto& m) {
            values(v, "gebiete", m.zones);
            values(v, "anteile", m.zone_shares);
            values(v, "fremdeinfuhr", m.foreign_share);
        });
    }

    // Diplomatie (seit M9, Schritt b1): nur in Welten mit Deutschem Bund (1648 bleibt bitgleich).
    if (!w.relations.empty()) {
        values(v, "beziehungen", w.relations);
        list(v, "gesandte", w.envoys, Extent::State, [&](auto& e) {
            v.field("von", e.from);
            v.field("bei", e.to);
            v.field("seit", e.since);
        });
        list(v, "vertraege", w.treaties, Extent::State, [&](auto& t) {
            v.field("art", t.kind);
            v.field("von", t.from);
            v.field("an", t.to);
            v.field("seit", t.since);
        });
        list(v, "gesandtenkosten", w.states, Extent::Content,
             [&](auto& s) { v.field("haushalt.gesandte", s.budget.diplomacy); });
        // Bundesversammlung und diplomatische Spiele (seit Schritt b2).
        v.field("praesidium", w.diet_presidency);
        list(v, "antraege", w.motions, Extent::State, [&](auto& m) {
            v.field("art", m.kind);
            v.field("antragsteller", m.applicant);
            v.field("ziel", m.target);
            v.field("gestellt", m.filed);
        });
        list(v, "abstimmungen", w.motion_results, Extent::State, [&](auto& r) {
            v.field("art", r.kind);
            v.field("antragsteller", r.applicant);
            v.field("ziel", r.target);
            v.field("gegner", r.opponent);
            v.field("datum", r.date);
            v.field("plenum_ja", r.plenum_yes);
            v.field("plenum", r.plenum_total);
            v.field("rat_ja", r.council_yes);
            v.field("rat", r.council_total);
            v.field("angenommen", r.accepted);
        });
        list(v, "spiele", w.plays, Extent::State, [&](auto& p) {
            v.field("art", p.kind);
            v.field("fordernder", p.demander);
            v.field("gegner", p.target);
            v.field("zollunion", p.customs_union);
            v.field("stufe", p.stage);
            v.field("beginn", p.started);
            v.field("stufe_seit", p.stage_started);
            values(v, "seite_fordernder", p.demander_side);
            values(v, "seite_gegner", p.target_side);
            v.field("fordernder_eskaliert", p.demander_escalates);
            v.field("gegner_eskaliert", p.target_escalates);
        });
        list(v, "spielergebnisse", w.play_results, Extent::State, [&](auto& r) {
            v.field("art", r.kind);
            v.field("fordernder", r.demander);
            v.field("gegner", r.target);
            v.field("zollunion", r.customs_union);
            v.field("beginn", r.started);
            v.field("ende", r.ended);
            v.field("stufe", r.stage);
            v.field("ausgang", r.outcome);
        });
    }

    // Militär (seit M10, Schritt a1): nur in Welten mit Militärdaten (1648 bleibt bitgleich).
    if (w.military_rules.active) {
        v.field("naechstes_heer", w.next_army_id);
        list(v, "heere", w.armies, Extent::State, [&](auto& a) {
            v.field("nummer", a.id);
            v.field("staat", a.state);
            v.field("ort", a.location);
            v.field("garnison", a.garrison);
            v.field("name", a.name);
            v.field("soldaten", a.soldiers);
            v.field("geschuetze", a.guns);
            v.field("moral", a.morale);
            v.field("lage", a.status);
            list(v, "weg", a.route, Extent::State, [&](auto& r) { // seit a2
                v.field("ort", r.location);
                v.field("art", r.mode);
                v.field("strecke", r.railway);
            });
            v.field("tage", a.days_left);
        });
        list(v, "mobilmachung", w.mobilizations, Extent::Content, [&](auto& m) { // seit a2, je Staat
            v.field("phase", m.phase);
            v.field("tage", m.days);
            v.field("krise", m.crisis);
        });
        list(v, "bahn_belegt", w.rail_backlog, Extent::Content, [&](auto& b) { v.field("", b); });
    }

    // Politik (seit M8): nur in Welten mit Parlamenten, damit die Prüfsumme der übrigen (1648) bleibt.
    if (!w.parliaments.empty()) {
        list(v, "wahlen", w.elections, Extent::State, [&](auto& e) {
            v.field("parlament", e.parliament);
            v.field("tag", e.day);
            list(v, "wahlkreise", e.districts, Extent::State, [&](auto& d) {
                v.field("wahlkreis", d.district);
                v.field("berechtigte", d.electorate);
                v.field("waehler", d.voters);
                values(v, "stimmen", d.votes);
                v.field("sieger", d.winner);
                v.field("stichwahl", d.runoff);
                v.field("boykott", d.boycott);
                v.field("meldung", d.report_rank);
            });
            list(v, "abgeordnete", e.deputies, Extent::State, [&](auto& a) {
                v.field("wahlkreis", a.district);
                v.field("stroemung", a.current);
                v.field("position", a.position);
                v.field("fraktion", a.faction);
            });
        });
        // Wahlkampf (seit Schritt b2): je Wahlkreis und Strömung, Mittel und Spenden je Strömung.
        list(v, "wahlkampf", w.campaign, Extent::State, [&](auto& c) {
            v.field("mobilisierung", c.mobilization);
            v.field("flugblaetter", c.persuasion);
            v.field("vereine", c.club);
        });
        values(v, "parteikasse", w.party_funds);
        values(v, "spenden", w.party_donations);
    }

    v.field("zufall.demografie", sim.rng_demography_);
    v.field("zufall.wirtschaft", sim.rng_economy_);
    v.field("zufall.ereignisse", sim.rng_events_);

    list(v, "chronik", w.chronicle, Extent::State, [&](auto& e) {
        v.field("datum", e.date);
        v.field("text", e.text);
        v.field("ort", e.location);
        v.field("staat", e.state);
    });

    list(v, "befehle_wartend", sim.pending_, Extent::State, [&](auto& c) {
        v.field("art", c.type);
        v.field("staat", c.state);
        v.field("wert", c.value);
        v.field("steuerart", c.kind);
        // Wahlkampf (M8), Zollunion und Diplomatie (M9), Marsch (M10)
        if (!w.parliaments.empty() || !w.customs_unions.empty() || !w.relations.empty() ||
            w.military_rules.active) {
            v.field("ziel", c.target);
            v.field("aktion", c.action);
        }
    });
}

// ─── Besucher ───────────────────────────────────────────────────────────────

/// Zerlegt die Feldtypen in die vier Grundformen (i64, u64, Text, feste Felder); Derived liefert
/// prim_i64, prim_u64 und prim_text, die einen Wert lesen (Lesebesucher) oder verwenden.
template <typename Derived> class PrimitiveVisitor {
public:
    template <typename T> void field(std::string_view name, T& x) {
        using U = std::remove_const_t<T>;
        Derived& d = static_cast<Derived&>(*this);
        if constexpr (std::is_same_v<U, Fixed>) {
            std::int64_t raw = x.raw();
            d.prim_i64(name, raw);
            assign(x, Fixed::from_raw(raw));
        } else if constexpr (std::is_same_v<U, Date>) {
            std::int64_t year = x.year;
            std::int64_t month = x.month;
            std::int64_t day = x.day;
            d.prim_i64(name, year);
            d.prim_i64(name, month);
            d.prim_i64(name, day);
            narrow(name, x.year, year);
            narrow(name, x.month, month);
            narrow(name, x.day, day);
        } else if constexpr (std::is_same_v<U, std::string>) {
            std::string text = x;
            d.prim_text(name, text);
            assign(x, std::move(text));
        } else if constexpr (std::is_same_v<U, Pcg32>) {
            std::uint64_t state = x.state();
            std::uint64_t inc = x.increment();
            d.prim_u64(name, state);
            d.prim_u64(name, inc);
            if constexpr (!std::is_const_v<T>) {
                x.restore(state, inc);
            }
        } else if constexpr (std::is_enum_v<U>) {
            auto raw = static_cast<std::uint64_t>(x);
            d.prim_u64(name, raw);
            narrow(name, x, raw);
        } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>) {
            auto raw = static_cast<std::int64_t>(x);
            d.prim_i64(name, raw);
            narrow(name, x, raw);
        } else if constexpr (std::is_integral_v<U>) {
            auto raw = static_cast<std::uint64_t>(x);
            d.prim_u64(name, raw);
            narrow(name, x, raw);
        } else {
            // std::array: Elemente ohne Längenangabe.
            for (auto& element : x) {
                field(name, element);
            }
        }
    }

    void enter(std::string_view, std::size_t) {}
    void leave() {}
    /// Gelesener Wert passt nicht in den Feldtyp (nur Lesebesucher).
    void out_of_range(std::string_view) {}

    template <typename Vec> bool size(std::string_view name, Vec& items, Extent extent) {
        std::uint64_t n = items.size();
        Derived& d = static_cast<Derived&>(*this);
        d.prim_u64(name, n);
        // else statt eines zweiten return: MSVC meldet sonst unerreichbaren Code (C4702).
        if constexpr (!std::is_const_v<Vec>) {
            return d.resize(items, n, extent);
        } else {
            return true;
        }
    }

private:
    template <typename T, typename U> static void assign(T& target, U&& value) {
        if constexpr (!std::is_const_v<T>) {
            target = std::forward<U>(value);
        }
    }
    /// Ganzzahl in den Feldtyp übernehmen; Werte, die dabei nicht erhalten bleiben, meldet der Lesebesucher.
    template <typename T, typename R> void narrow(std::string_view name, T& target, R raw) {
        if constexpr (!std::is_const_v<T>) {
            target = static_cast<T>(raw);
            if (static_cast<R>(target) != raw) {
                static_cast<Derived&>(*this).out_of_range(name);
            }
        }
    }
};

/// FNV-1a-64, Ganzzahlen bytegenau in Little-Endian-Reihenfolge (plattformunabhängig). Grundlage der
/// Prüfsumme, des Prüfwerts der Nutzdaten und des Inhaltsfingerabdrucks.
class Fnv1a {
public:
    void byte(std::uint8_t b) {
        hash_ ^= b;
        hash_ *= 0x100000001b3ULL;
    }
    void u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            byte(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    void i64(std::int64_t v) { u64(static_cast<std::uint64_t>(v)); }
    /// Länge (u64), dann die Bytes.
    void text(std::string_view s) {
        u64(s.size());
        for (const char c : s) {
            byte(static_cast<std::uint8_t>(c));
        }
    }
    std::uint64_t value() const { return hash_; }

private:
    std::uint64_t hash_ = 0xcbf29ce484222325ULL;
};

/// Datum als JJJJ-MM-TT (Textexport, Metadaten der Spielstände).
inline std::string iso_date(const Date& d) {
    std::string s = std::to_string(d.year);
    s += d.month < 10 ? "-0" : "-";
    s += std::to_string(d.month);
    s += d.day < 10 ? "-0" : "-";
    s += std::to_string(d.day);
    return s;
}

/// Prüfsumme über das Zustandsverzeichnis.
class HashVisitor : public PrimitiveVisitor<HashVisitor> {
public:
    void prim_i64(std::string_view, std::int64_t v) { hash_.i64(v); }
    void prim_u64(std::string_view, std::uint64_t v) { hash_.u64(v); }
    void prim_text(std::string_view, const std::string& s) { hash_.text(s); }
    std::uint64_t value() const { return hash_.value(); }

private:
    Fnv1a hash_;
};

/// Textexport: eine Zeile je Feld, "pfad = wert" (z. B. "staaten[3].schatz = 1234.567890").
/// Festkomma mit sechs Nachkommastellen (eindeutig je Rohwert), Datum als JJJJ-MM-TT, Zeichenketten in
/// Anführungszeichen, Zufallsströme hexadezimal. Nur zum Lesen und Vergleichen (diff).
class TextVisitor {
public:
    template <typename T> void field(std::string_view name, const T& x) {
        if constexpr (std::is_same_v<T, Fixed>) {
            line(name, x.to_string(6));
        } else if constexpr (std::is_same_v<T, Date>) {
            line(name, iso_date(x));
        } else if constexpr (std::is_same_v<T, std::string>) {
            line(name, quoted(x));
        } else if constexpr (std::is_same_v<T, Pcg32>) {
            line(name, hex16(x.state()) + " " + hex16(x.increment()));
        } else if constexpr (std::is_enum_v<T>) {
            line(name, std::to_string(static_cast<std::uint64_t>(x)));
        } else if constexpr (std::is_integral_v<T>) {
            line(name, std::to_string(x));
        } else {
            std::size_t i = 0;
            for (const auto& element : x) {
                enter(name, i++);
                field("", element);
                leave();
            }
        }
    }
    void enter(std::string_view name, std::size_t index) {
        path_.push_back(path_.empty() ? std::string(name) : path_.back() + "." + std::string(name));
        path_.back() += "[" + std::to_string(index) + "]";
    }
    void leave() { path_.pop_back(); }
    template <typename Vec> bool size(std::string_view name, const Vec& items, Extent) {
        line(std::string(name) + ".anzahl", std::to_string(items.size()));
        return true;
    }

    const std::string& text() const { return text_; }

private:
    void line(std::string_view name, const std::string& value) {
        std::string path = path_.empty() ? std::string() : path_.back();
        if (!name.empty()) {
            if (!path.empty()) {
                path += '.';
            }
            path += name;
        }
        text_ += path;
        text_ += " = ";
        text_ += value;
        text_ += '\n';
    }
    static std::string quoted(const std::string& s) {
        std::string out = "\"";
        for (const char c : s) {
            if (c == '"' || c == '\\') {
                out += '\\';
                out += c;
            } else if (c == '\n') {
                out += "\\n";
            } else {
                out += c;
            }
        }
        return out + "\"";
    }

    std::vector<std::string> path_;
    std::string text_;
};

} // namespace uhrwerk
