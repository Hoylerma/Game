// Zeitung (seit M13b1, docs/technik/vertical_slice.md 5.2; chronik-skript.md 3.42).

#include "uhrwerk/newspaper.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace uhrwerk {

namespace {

bool is_state_name(const World& world, std::string_view value) {
    return std::any_of(world.states.begin(), world.states.end(),
                       [&](const State& s) { return s.short_name == value || s.name == value; });
}

bool is_number(std::string_view value) {
    bool digit = false;
    for (const char c : value) {
        if (c >= '0' && c <= '9') {
            digit = true;
        } else if (c != '.' && c != ',') {
            return false;
        }
    }
    return digit;
}

bool capture_ok(const World& world, const NewspaperOccasion& occasion, std::string_view name,
                std::string_view value) {
    if (value.empty()) {
        return false;
    }
    if (name == "staat" || name == "gegner" || name == "partner" || name == "sieger") {
        return is_state_name(world, value);
    }
    if (name == "zahl" || name == "sitze" || name == "besetzt") {
        return is_number(value);
    }
    if (name == "titel") {
        if (occasion.event != kNone) {
            return static_cast<std::size_t>(occasion.event) < world.events.size() &&
                   world.events[static_cast<std::size_t>(occasion.event)].title == value;
        }
        return std::any_of(world.events.begin(), world.events.end(), [&](const EventDef& e) {
            return (!occasion.turning_point || e.turning_point) && e.title == value;
        });
    }
    return true;
}

bool match_from(const World& world, const NewspaperOccasion& occasion, std::string_view text,
                std::size_t part, std::size_t pos, NewspaperCaptures& out) {
    const auto& parts = occasion.parts;
    if (part == parts.size()) {
        return pos == text.size();
    }
    const NewspaperPatternPart& p = parts[part];
    switch (p.kind) {
    case NewspaperPatternPart::Kind::Text:
        if (text.substr(pos, p.text.size()) != p.text) {
            return false;
        }
        return match_from(world, occasion, text, part + 1, pos + p.text.size(), out);
    case NewspaperPatternPart::Kind::Rest: {
        if (part + 1 == parts.size()) {
            return true;
        }
        // In der Mitte: beliebiger Text (auch leer) bis zum folgenden Text, so kurz wie möglich.
        const std::string& next = parts[part + 1].text;
        for (std::size_t at = text.find(next, pos); at != std::string_view::npos;
             at = text.find(next, at + 1)) {
            if (match_from(world, occasion, text, part + 1, at, out)) {
                return true;
            }
        }
        return false;
    }
    case NewspaperPatternPart::Kind::Capture:
        break;
    }
    // Platzhalter: so kurz wie möglich, bis der folgende Text passt (der Lader erlaubt keine zwei
    // Platzhalter hintereinander; am Ende nimmt er den Rest).
    const bool last = part + 1 == parts.size();
    const bool rest_follows = !last && parts[part + 1].kind == NewspaperPatternPart::Kind::Rest;
    if (last || rest_follows) {
        for (std::size_t end = text.size(); end > pos; --end) {
            const std::string_view value = text.substr(pos, end - pos);
            if (!rest_follows && end != text.size()) {
                break;
            }
            if (capture_ok(world, occasion, p.text, value)) {
                out.emplace_back(p.text, std::string(value));
                return true;
            }
            if (!rest_follows) {
                break;
            }
        }
        return false;
    }
    const std::string& next = parts[part + 1].text;
    for (std::size_t at = text.find(next, pos + 1); at != std::string_view::npos;
         at = text.find(next, at + 1)) {
        const std::string_view value = text.substr(pos, at - pos);
        if (!capture_ok(world, occasion, p.text, value)) {
            continue;
        }
        out.emplace_back(p.text, std::string(value));
        if (match_from(world, occasion, text, part + 1, at, out)) {
            return true;
        }
        out.pop_back();
    }
    return false;
}

/// FNV-1a über Datum, Text und Salz: dieselbe Ausgabe zeigt immer dieselbe Seite.
std::uint64_t page_hash(const ChronicleEntry& entry, std::uint64_t salt) {
    std::uint64_t h = 14695981039346656037ULL ^ salt;
    const auto mix = [&](std::uint64_t byte) {
        h ^= byte;
        h *= 1099511628211ULL;
    };
    const auto day = static_cast<std::uint64_t>(to_day_number(entry.date));
    for (int i = 0; i < 8; ++i) {
        mix((day >> (8 * i)) & 0xFFU);
    }
    for (const char c : entry.text) {
        mix(static_cast<unsigned char>(c));
    }
    return h;
}

const std::string& pick(const std::vector<std::string>& list, std::uint64_t h) {
    static const std::string kEmpty;
    return list.empty() ? kEmpty : list[h % list.size()];
}

const NewspaperTitle* choose_paper(const World& world, Index reader, std::uint64_t h) {
    const auto& titles = world.newspaper.titles;
    std::vector<const NewspaperTitle*> candidates;
    const bool has_reader = reader >= 0 && static_cast<std::size_t>(reader) < world.states.size();
    Index market = kNone;
    if (has_reader) {
        const Index capital = world.states[static_cast<std::size_t>(reader)].capital;
        if (capital >= 0 && static_cast<std::size_t>(capital) < world.locations.size()) {
            market = world.locations[static_cast<std::size_t>(capital)].market;
        }
    }
    const auto collect = [&](auto&& accept) {
        for (const NewspaperTitle& t : titles) {
            if (accept(t)) {
                candidates.push_back(&t);
            }
        }
    };
    if (market != kNone) {
        collect([&](const NewspaperTitle& t) {
            return std::find(t.markets.begin(), t.markets.end(), market) != t.markets.end();
        });
    }
    if (candidates.empty() && has_reader) {
        collect([&](const NewspaperTitle& t) {
            return std::find(t.states.begin(), t.states.end(), reader) != t.states.end();
        });
    }
    if (candidates.empty()) {
        collect([](const NewspaperTitle& t) { return t.states.empty() && t.markets.empty(); });
    }
    if (candidates.empty()) {
        collect([](const NewspaperTitle&) { return true; });
    }
    return candidates.empty() ? nullptr : candidates[h % candidates.size()];
}

} // namespace

std::optional<NewspaperCaptures> match_occasion(const World& world, const NewspaperOccasion& occasion,
                                                const ChronicleEntry& entry) {
    NewspaperCaptures captures;
    if (occasion.parts.empty() || !match_from(world, occasion, entry.text, 0, 0, captures)) {
        return std::nullopt;
    }
    return captures;
}

std::optional<std::size_t> newspaper_occasion(const World& world, const ChronicleEntry& entry) {
    if (!world.newspaper.active) {
        return std::nullopt;
    }
    const auto& occasions = world.newspaper.occasions;
    for (std::size_t i = 0; i < occasions.size(); ++i) {
        if (match_occasion(world, occasions[i], entry)) {
            return i;
        }
    }
    return std::nullopt;
}

std::int32_t newspaper_weight(const World& world, const ChronicleEntry& entry) {
    const auto occasion = newspaper_occasion(world, entry);
    return occasion ? world.newspaper.occasions[*occasion].weight : -1;
}

std::string fill_placeholders(const std::string& text, const NewspaperCaptures& values) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '{') {
            const std::size_t end = text.find('}', i);
            if (end != std::string::npos) {
                const std::string_view name(text.data() + i + 1, end - i - 1);
                const auto it = std::find_if(values.begin(), values.end(),
                                             [&](const auto& v) { return v.first == name; });
                if (it != values.end()) {
                    out += it->second;
                    i = end + 1;
                    continue;
                }
            }
        }
        out += text[i];
        ++i;
    }
    return out;
}

std::optional<FrontPage> front_page(const World& world, std::size_t entry_index, Index reader) {
    if (!world.newspaper.active || entry_index >= world.chronicle.size()) {
        return std::nullopt;
    }
    const ChronicleEntry& entry = world.chronicle[entry_index];
    const auto index = newspaper_occasion(world, entry);
    if (!index) {
        return std::nullopt;
    }
    const NewspaperOccasion& occasion = world.newspaper.occasions[*index];
    if (occasion.weight <= 0) {
        return std::nullopt;
    }
    NewspaperCaptures values = *match_occasion(world, occasion, entry);
    // Ein Eintrag ohne Staat (Nationalversammlung, Bundesversammlung) betrifft jeden Leser.
    bool involved = entry.state == kNone || (entry.state == reader && reader != kNone);
    if (reader >= 0 && static_cast<std::size_t>(reader) < world.states.size()) {
        const State& own = world.states[static_cast<std::size_t>(reader)];
        for (const auto& [name, value] : values) {
            if ((name == "staat" || name == "gegner" || name == "partner" || name == "sieger") &&
                (value == own.short_name || value == own.name)) {
                involved = true;
            }
        }
    }
    const std::uint64_t h = page_hash(entry, 0);
    const NewspaperTitle* paper = choose_paper(world, reader, h);
    if (paper == nullptr) {
        return std::nullopt;
    }

    FrontPage page;
    page.entry = entry_index;
    page.occasion = occasion.id;
    page.weight = occasion.weight;
    page.involved = involved;
    page.paper = paper->name;
    page.leaning = paper->leaning;
    page.date = entry.date;
    page.issue =
        static_cast<std::int32_t>(to_day_number(entry.date) - to_day_number(Date{entry.date.year, 1, 1}) + 1);

    // Feste Platzhalter, wenn das Muster sie nicht selbst hat.
    const auto add = [&](const char* name, std::string value) {
        if (std::none_of(values.begin(), values.end(), [&](const auto& v) { return v.first == name; })) {
            values.emplace_back(name, std::move(value));
        }
    };
    add("datum", format_date_long(entry.date));
    add("jahr", std::to_string(entry.date.year));
    add("blatt", paper->name);
    if (entry.location >= 0 && static_cast<std::size_t>(entry.location) < world.locations.size()) {
        add("ort", world.locations[static_cast<std::size_t>(entry.location)].name);
    }
    if (entry.state >= 0 && static_cast<std::size_t>(entry.state) < world.states.size()) {
        const State& s = world.states[static_cast<std::size_t>(entry.state)];
        add("staat", s.short_name.empty() ? s.name : s.short_name);
    }

    // Wortlaut: der der Richtung, wo es ihn gibt, sonst der allgemeine.
    const NewspaperWording& general = occasion.wordings.front();
    const NewspaperWording* own = nullptr;
    for (const NewspaperWording& w : occasion.wordings) {
        if (w.leaning == paper->leaning) {
            own = &w;
        }
    }
    const auto list = [&](auto member) -> const std::vector<std::string>& {
        return own != nullptr && !(own->*member).empty() ? own->*member : general.*member;
    };
    page.headline = fill_placeholders(pick(list(&NewspaperWording::headlines), page_hash(entry, 1)), values);
    page.subline = fill_placeholders(pick(list(&NewspaperWording::sublines), page_hash(entry, 2)), values);
    page.text = fill_placeholders(pick(list(&NewspaperWording::texts), page_hash(entry, 3)), values);
    page.report = entry.text;
    if (page.text.empty()) {
        page.text = entry.text;
    }

    // Vermischtes: bis zu vier andere Einträge der vierzehn Tage davor ohne eigene Titelseite.
    const std::int64_t day = to_day_number(entry.date);
    for (std::size_t i = entry_index; i-- > 0 && page.miscellany.size() < 4;) {
        const ChronicleEntry& other = world.chronicle[i];
        if (day - to_day_number(other.date) > 14) {
            break;
        }
        if (newspaper_weight(world, other) < 2) {
            page.miscellany.push_back(other.text);
        }
    }
    return page;
}

} // namespace uhrwerk
