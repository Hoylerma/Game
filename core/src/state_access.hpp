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
        if constexpr (!std::is_const_v<Vec>) {
            return d.resize(items, n, extent);
        }
        return true;
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

/// Prüfsumme: FNV-1a-64, Ganzzahlen bytegenau in Little-Endian-Reihenfolge (plattformunabhängig).
class HashVisitor : public PrimitiveVisitor<HashVisitor> {
public:
    void prim_i64(std::string_view, std::int64_t v) { u64(static_cast<std::uint64_t>(v)); }
    void prim_u64(std::string_view, std::uint64_t v) { u64(v); }
    void prim_text(std::string_view, const std::string& s) {
        u64(s.size());
        for (const char c : s) {
            byte(static_cast<std::uint8_t>(c));
        }
    }
    std::uint64_t value() const { return hash_; }

private:
    void byte(std::uint8_t b) {
        hash_ ^= b;
        hash_ *= 0x100000001b3ULL;
    }
    void u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            byte(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }

    std::uint64_t hash_ = 0xcbf29ce484222325ULL;
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
            line(name, hex(x.state()) + " " + hex(x.increment()));
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
    static std::string iso_date(const Date& d) {
        std::string s = std::to_string(d.year);
        s += d.month < 10 ? "-0" : "-";
        s += std::to_string(d.month);
        s += d.day < 10 ? "-0" : "-";
        s += std::to_string(d.day);
        return s;
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
    static std::string hex(std::uint64_t v) {
        static constexpr char kDigits[] = "0123456789abcdef";
        std::string s(16, '0');
        for (int i = 15; i >= 0; --i) {
            s[static_cast<std::size_t>(i)] = kDigits[v & 0xF];
            v >>= 4;
        }
        return s;
    }

    std::vector<std::string> path_;
    std::string text_;
};

} // namespace uhrwerk
