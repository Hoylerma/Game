// Spielstände und Wiederholungen (seit M6, docs/technik/speicherstaende.md).

#include "uhrwerk/savegame.hpp"

#include "state_access.hpp"
#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/legacy.hpp"
#include "uhrwerk/military.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <system_error>
#include <tuple>
#include <utility>
#include <zstd.h>

#ifndef UHRWERK_VERSION
#define UHRWERK_VERSION "0.0.0"
#endif

namespace uhrwerk {

namespace {

constexpr std::size_t kHeaderSize = 48;
constexpr std::array<std::uint8_t, 8> kSaveMagic = {'E', 'I', 'N', 'I', 'G', 'K', 'T', 0x1A};
constexpr std::array<std::uint8_t, 8> kReplayMagic = {'E', 'I', 'N', 'I', 'G', 'P', 'R', 0x1A};
constexpr std::uint32_t kFlagZstd = 1;
constexpr int kZstdLevel = 3;
/// Obergrenze der unkomprimierten Nutzdaten (Schutz vor beschädigten Längenangaben).
constexpr std::uint64_t kMaxPayload = std::uint64_t{1} << 31;
/// Stand des Militärs im Zustand (seit M10, Inhaltsbereich militaer): 1 = Schritt a1 (Garnisonen).
constexpr std::uint64_t kMilitaryContentVersion = 9;

/// Stand der Ereignisse im Zustand (seit M11, Inhaltsbereich ereignisse): 1 = Schritt a1, 2 = a2 (Auslösung,
/// Befristung, Spieler), 3 = a3 (Unruhe, Aufstände), 4 = a4 (Journale, Abstimmungen), 5 = a5 (beendete
/// Aufstände).
constexpr std::uint64_t kEventContentVersion =
    9; // 7 seit M12a3: Block ki an den Optionen; 8 seit M13a2: Bund; 9 seit M13a4: Vermächtnis, Jahrbuch

/// Stand der KI im Zustand (seit M12, Inhaltsbereich ki): 1 = Schritt a1 (Ämter, Strom Ki), 2 = a2
/// (Strategie, Protokoll, Abkühlung; Planer Haushalt und Mobilmachung), 3 = a3
/// (Diplomatie, Wahlkampf, Spielerströmung), 4 = a4 (Krieg und Frieden).
constexpr std::uint64_t kAiContentVersion = 5;

bool fail(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
    return false;
}

// ─── FNV-1a-64 ──────────────────────────────────────────────────────────────

/// FNV-1a mit den Feldtypen des Inhaltsfingerabdrucks.
class Fnv : public Fnv1a {
public:
    void fixed(Fixed f) { i64(f.raw()); }
    void flag(bool b) { u64(b ? 1 : 0); }
    void amounts(const std::vector<GoodAmount>& list) {
        u64(list.size());
        for (const GoodAmount& a : list) {
            i64(a.good);
            fixed(a.amount);
        }
    }
};

std::uint64_t fnv1a(std::span<const std::uint8_t> bytes) {
    Fnv h;
    for (const std::uint8_t b : bytes) {
        h.byte(b);
    }
    return h.value();
}

// ─── Bytes schreiben und lesen (little-endian) ──────────────────────────────

class ByteWriter {
public:
    void u32(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            data_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    void u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            data_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    void i64(std::int64_t v) { u64(static_cast<std::uint64_t>(v)); }
    void bytes(std::span<const std::uint8_t> b) { data_.insert(data_.end(), b.begin(), b.end()); }
    void bytes(std::string_view s) {
        for (const char c : s) {
            data_.push_back(static_cast<std::uint8_t>(c));
        }
    }

    /// Abschnitt: Kennung (4 Zeichen) und Länge (u64, wird in end_section nachgetragen).
    std::size_t begin_section(std::string_view tag) {
        bytes(tag);
        const std::size_t at = data_.size();
        u64(0);
        return at;
    }
    void end_section(std::size_t at) {
        const std::uint64_t length = data_.size() - at - 8;
        for (std::size_t i = 0; i < 8; ++i) {
            data_[at + i] = static_cast<std::uint8_t>(length >> (8 * i));
        }
    }

    std::vector<std::uint8_t>& data() { return data_; }

private:
    std::vector<std::uint8_t> data_;
};

/// Liest mit Grenzprüfung; nach dem ersten Fehler liefert jeder Zugriff 0 und failed() ist gesetzt.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    std::uint32_t u32() {
        const auto b = take(4);
        std::uint32_t v = 0;
        for (std::size_t i = 0; i < b.size(); ++i) {
            v |= static_cast<std::uint32_t>(b[i]) << (8 * i);
        }
        return v;
    }
    std::uint64_t u64() {
        const auto b = take(8);
        std::uint64_t v = 0;
        for (std::size_t i = 0; i < b.size(); ++i) {
            v |= static_cast<std::uint64_t>(b[i]) << (8 * i);
        }
        return v;
    }
    std::int64_t i64() { return static_cast<std::int64_t>(u64()); }

    std::span<const std::uint8_t> take(std::uint64_t n) {
        if (failed_ || n > remaining()) {
            fail("Daten enden zu früh");
            return {};
        }
        const auto result = bytes_.subspan(pos_, static_cast<std::size_t>(n));
        pos_ += static_cast<std::size_t>(n);
        return result;
    }

    /// Abschnitt mit der Kennung tag lesen; liefert seinen Inhalt.
    std::span<const std::uint8_t> section(std::string_view tag) {
        const auto t = take(4);
        if (failed_) {
            return {};
        }
        if (std::memcmp(t.data(), tag.data(), 4) != 0) {
            fail("Abschnitt " + std::string(tag) + " fehlt");
            return {};
        }
        return take(u64());
    }

    std::size_t remaining() const { return bytes_.size() - pos_; }
    bool failed() const { return failed_; }
    const std::string& error() const { return error_; }
    void fail(std::string message) {
        if (!failed_) {
            failed_ = true;
            error_ = std::move(message);
        }
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t pos_ = 0;
    bool failed_ = false;
    std::string error_;
};

// ─── Besucher für das Zustandsverzeichnis ───────────────────────────────────

class WriteVisitor : public PrimitiveVisitor<WriteVisitor> {
public:
    explicit WriteVisitor(ByteWriter& out) : out_(out) {}

    void prim_i64(std::string_view, std::int64_t v) { out_.i64(v); }
    void prim_u64(std::string_view, std::uint64_t v) { out_.u64(v); }
    void prim_text(std::string_view, const std::string& s) {
        out_.u64(s.size());
        out_.bytes(s);
    }

private:
    ByteWriter& out_;
};

class ReadVisitor : public PrimitiveVisitor<ReadVisitor> {
public:
    explicit ReadVisitor(ByteReader& in) : in_(in) {}

    void prim_i64(std::string_view, std::int64_t& v) { v = in_.i64(); }
    void prim_u64(std::string_view name, std::uint64_t& v) {
        last_ = name;
        v = in_.u64();
    }
    void prim_text(std::string_view, std::string& s) {
        const auto b = in_.take(in_.u64());
        s.assign(b.begin(), b.end());
    }
    void out_of_range(std::string_view name) { in_.fail("Wert von " + std::string(name) + " ungültig"); }

    /// Länge eines Vektors: Inhalt muss passen, Zustand bestimmt sie selbst (jedes Element belegt
    /// mindestens 8 Byte, das begrenzt die Länge durch die verbleibenden Daten).
    template <typename Vec> bool resize(Vec& items, std::uint64_t n, Extent extent) {
        if (in_.failed()) {
            return false;
        }
        if (extent == Extent::Content) {
            if (n != items.size()) {
                in_.fail("Anzahl " + std::string(last_) + " passt nicht zu den Spieldaten (" +
                         std::to_string(n) + " statt " + std::to_string(items.size()) + ")");
                return false;
            }
            return true;
        }
        if (n > in_.remaining() / 8) {
            in_.fail("Anzahl " + std::string(last_) + " zu groß");
            return false;
        }
        items.resize(static_cast<std::size_t>(n));
        return true;
    }

private:
    ByteReader& in_;
    std::string_view last_;
};

// ─── Inhaltsfingerabdruck (Abschnitt 3) ─────────────────────────────────────

/// Zahl der Felder eines Aggregats: größtes N, mit dem T{x1, …, xN} gültig ist. Anders als sizeof
/// fällt so auch ein neues Feld auf, das in eine Ausrichtungslücke passt.
struct AnyField {
    template <typename T> constexpr operator T() const noexcept { return T{}; }
};
template <typename T, std::size_t... I> constexpr bool brace_constructible(std::index_sequence<I...>) {
    return requires { T{(static_cast<void>(I), AnyField{})...}; };
}
template <typename T, std::size_t N = 0> constexpr std::size_t field_count() {
    if constexpr (brace_constructible<T>(std::make_index_sequence<N + 1>{})) {
        return field_count<T, N + 1>();
    } else {
        return N;
    }
}

void hash_rules(Fnv& h, const Rules& r) {
    static_assert(field_count<Rules>() == 62,
                  "Rules hat sich geändert: neue Felder auch in hash_rules (Inhaltsfingerabdruck) aufnehmen");
    h.fixed(r.devastation_decay);
    h.fixed(r.devastation_yield_loss);
    h.fixed(r.plague_chance);
    h.fixed(r.plague_loss_min);
    h.fixed(r.plague_loss_max);
    h.fixed(r.tax_rate_max);
    h.fixed(r.share_children);
    h.fixed(r.share_young);
    h.fixed(r.share_working);
    h.fixed(r.fertility);
    h.fixed(r.fertile_share_working);
    h.fixed(r.fertility_reserve);
    h.fixed(r.fertility_wealth);
    h.fixed(r.mortality_children);
    h.fixed(r.mortality_young);
    h.fixed(r.mortality_working);
    h.fixed(r.mortality_old);
    h.fixed(r.mortality_poverty);
    h.fixed(r.capacity_city);
    h.fixed(r.living_standard_reference);
    h.fixed(r.living_standard_adjustment);
    h.fixed(r.literacy_city);
    h.fixed(r.literacy_adjustment);
    h.i64(r.migration_targets);
    h.fixed(r.migration_radius_km);
    h.fixed(r.migration_rate);
    h.fixed(r.migration_max);
    h.fixed(r.migration_reserve);
    h.fixed(r.migration_foreign);
    h.fixed(r.emigration_threshold);
    h.fixed(r.emigration_rate);
    h.fixed(r.block_min_size);
    h.fixed(r.migration_min_amount);
    h.fixed(r.price_min);
    h.fixed(r.price_max);
    h.fixed(r.price_adjustment);
    h.i64(r.trade_neighbors);
    h.fixed(r.trade_radius_km);
    h.fixed(r.trade_rate);
    h.fixed(r.freight_land);
    h.fixed(r.freight_sea);
    h.fixed(r.freight_rail);
    h.fixed(r.trade_margin);
    h.fixed(r.business_adjustment);
    h.fixed(r.business_min_share);
    h.fixed(r.harvest_variation_land);
    h.fixed(r.harvest_variation_market);
    h.fixed(r.mortality_hunger);
    h.i64(r.settling_months);
    h.fixed(r.administration_per_capita);
    h.fixed(r.soldier_pay);
    h.fixed(r.reserve_years);
    h.fixed(r.building_share);
    h.fixed(r.development_max);
    h.fixed(r.development_half);
    h.fixed(r.interest_base);
    h.fixed(r.interest_risk);
    h.fixed(r.interest_max);
    h.fixed(r.bankruptcy_ratio);
    h.fixed(r.bankruptcy_haircut);
    h.fixed(r.bankruptcy_premium);
    h.i64(r.bankruptcy_years);
}

template <typename T, typename Fn> std::uint64_t hash_list(const std::vector<T>& items, Fn&& fn) {
    Fnv h;
    h.u64(items.size());
    for (const T& item : items) {
        fn(h, item);
    }
    return h.value();
}

// ─── Metadaten ──────────────────────────────────────────────────────────────

using Meta = std::map<std::string, std::string, std::less<>>;

/// Steuerzeichen (Zeilenumbrüche) in Werten durch Leerzeichen ersetzen.
std::string clean(std::string_view value) {
    std::string out(value);
    for (char& c : out) {
        if (static_cast<unsigned char>(c) < 0x20) {
            c = ' ';
        }
    }
    return out;
}

void add_meta(std::string& out, std::string_view key, std::string_view value) {
    out += key;
    out += '=';
    out += clean(value);
    out += '\n';
}

void add_content_meta(std::string& out, const ContentFingerprint& content) {
    add_meta(out, "inhalt", hex16(content.total));
    for (std::size_t i = 0; i < kContentParts.size(); ++i) {
        add_meta(out, "inhalt." + std::string(kContentParts[i]), hex16(content.parts[i]));
    }
}

std::optional<Meta> parse_meta(std::string_view text, std::string* error) {
    Meta meta;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        const std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
        if (line.empty()) {
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos || eq == 0) {
            fail(error, "Metadaten beschädigt (Zeile ohne Schlüssel)");
            return std::nullopt;
        }
        if (!meta.emplace(std::string(line.substr(0, eq)), std::string(line.substr(eq + 1))).second) {
            fail(error, "Metadaten beschädigt (Schlüssel " + std::string(line.substr(0, eq)) + " doppelt)");
            return std::nullopt;
        }
    }
    return meta;
}

/// Liest Werte aus den Metadaten; merkt sich den ersten fehlenden oder ungültigen Schlüssel.
class MetaReader {
public:
    explicit MetaReader(const Meta& meta) : meta_(meta) {}

    /// Angabe, die fehlen darf (seit M8: spielerstroemung); leer, wenn sie fehlt.
    std::string optional_text(std::string_view key) const {
        const auto it = meta_.find(key);
        return it == meta_.end() ? std::string() : it->second;
    }
    std::string text(std::string_view key) {
        const auto it = meta_.find(key);
        if (it == meta_.end()) {
            bad(key);
            return {};
        }
        return it->second;
    }
    std::uint64_t u64(std::string_view key, int base = 10) {
        const std::string value = text(key);
        std::uint64_t result = 0;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result, base);
        if (ec != std::errc() || ptr != value.data() + value.size() || value.empty()) {
            bad(key);
        }
        return result;
    }
    std::uint64_t hex(std::string_view key) { return u64(key, 16); }
    std::int64_t i64(std::string_view key) {
        const std::string value = text(key);
        std::int64_t result = 0;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
        if (ec != std::errc() || ptr != value.data() + value.size() || value.empty()) {
            bad(key);
        }
        return result;
    }
    Date date(std::string_view key) {
        const std::string value = text(key);
        Date d;
        int year = 0;
        int month = 0;
        int day = 0;
        const auto parse_int = [&](const std::size_t begin, const std::size_t end, int& out) {
            if (begin >= end) {
                return false;
            }
            const char* first = value.data() + static_cast<std::ptrdiff_t>(begin);
            const char* last = value.data() + static_cast<std::ptrdiff_t>(end);
            const auto [ptr, ec] = std::from_chars(first, last, out);
            return ec == std::errc() && ptr == last;
        };
        const std::size_t first_dash = value.find('-');
        const std::size_t second_dash =
            first_dash == std::string::npos ? std::string::npos : value.find('-', first_dash + 1);
        if (first_dash == std::string::npos || second_dash == std::string::npos ||
            value.find('-', second_dash + 1) != std::string::npos || !parse_int(0, first_dash, year) ||
            !parse_int(first_dash + 1, second_dash, month) ||
            !parse_int(second_dash + 1, value.size(), day)) {
            bad(key);
            return d;
        }
        d = Date{year, month, day};
        if (!is_valid(d)) {
            bad(key);
        }
        return d;
    }
    /// Fingerabdruck des Inhalts; Format 1 (bis M7) kennt nur die ersten 13 Bereiche (ohne politik),
    /// Format 2 (M8) die ersten 14 (ohne diplomatie), Format 3 (M9) die ersten 15 (ohne militaer), Format 4
    /// (M10) die ersten 16 (ohne ereignisse), Format 5 (M11) die ersten 17 (ohne ki), die übrigen bleiben 0.
    /// So zeigt auch ein alter Spielstand seine Metadaten an.
    ContentFingerprint content(std::uint32_t version) {
        const std::size_t count = version == 1   ? 13
                                  : version == 2 ? 14
                                  : version == 3 ? 15
                                  : version == 4 ? 16
                                  : version == 5 ? 17
                                                 : kContentParts.size();
        ContentFingerprint fp;
        fp.total = hex("inhalt");
        for (std::size_t i = 0; i < count; ++i) {
            fp.parts[i] = hex("inhalt." + std::string(kContentParts[i]));
        }
        if (ok() && content_fingerprint_total(std::span(fp.parts).first(count)) != fp.total) {
            bad("inhalt");
        }
        return fp;
    }

    bool ok() const { return bad_.empty(); }
    std::string error() const { return "Metadaten beschädigt (" + bad_ + " fehlt oder ist ungültig)"; }

    static std::uint64_t content_fingerprint_total(std::span<const std::uint64_t> parts) {
        Fnv h;
        for (const std::uint64_t p : parts) {
            h.u64(p);
        }
        return h.value();
    }

private:
    void bad(std::string_view key) {
        if (bad_.empty()) {
            bad_ = key;
        }
    }

    const Meta& meta_;
    std::string bad_;
};

// ─── Behälter: Kopf, Metadaten, Nutzdaten (Abschnitt 4) ─────────────────────

std::vector<std::uint8_t> write_container(const std::array<std::uint8_t, 8>& magic, const std::string& meta,
                                          const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> packed(ZSTD_compressBound(payload.size()));
    const std::size_t packed_size =
        ZSTD_compress(packed.data(), packed.size(), payload.data(), payload.size(), kZstdLevel);
    const bool compressed = ZSTD_isError(packed_size) == 0;
    const std::span<const std::uint8_t> stored =
        compressed ? std::span<const std::uint8_t>(packed.data(), packed_size) : std::span(payload);

    ByteWriter out;
    out.bytes(magic);
    out.u32(kSaveFormatVersion);
    out.u32(compressed ? kFlagZstd : 0);
    out.u32(static_cast<std::uint32_t>(meta.size()));
    out.u32(0);
    out.u64(payload.size());
    out.u64(stored.size());
    out.u64(fnv1a(payload));
    out.bytes(meta);
    out.bytes(stored);
    return std::move(out.data());
}

struct Container {
    std::uint32_t version = 0;
    bool compressed = false;
    std::uint64_t raw_size = 0;
    std::uint64_t payload_hash = 0;
    Meta meta;
    std::span<const std::uint8_t> stored;
};

std::optional<Container> read_container(std::span<const std::uint8_t> bytes, bool replay,
                                        std::string* error) {
    const auto& magic = replay ? kReplayMagic : kSaveMagic;
    const auto& other = replay ? kSaveMagic : kReplayMagic;
    const char* const what = replay ? "Protokoll" : "Spielstand";
    if (bytes.size() >= 8 && std::equal(other.begin(), other.end(), bytes.begin())) {
        fail(error, replay ? "Datei ist ein Spielstand, kein Protokoll"
                           : "Datei ist ein Protokoll, kein Spielstand");
        return std::nullopt;
    }
    if (bytes.size() < 8 || !std::equal(magic.begin(), magic.end(), bytes.begin())) {
        fail(error, std::string("kein ") + what + " von EINIGKEIT");
        return std::nullopt;
    }
    if (bytes.size() < kHeaderSize) {
        fail(error, std::string(what) + " unvollständig (Kopf zu kurz)");
        return std::nullopt;
    }
    ByteReader in(bytes.subspan(8));
    Container c;
    c.version = in.u32();
    const std::uint32_t flags = in.u32();
    const std::uint32_t meta_size = in.u32();
    const std::uint32_t reserved = in.u32();
    c.raw_size = in.u64();
    const std::uint64_t stored_size = in.u64();
    c.payload_hash = in.u64();
    c.compressed = (flags & kFlagZstd) != 0;

    if (c.version == 0) {
        fail(error, std::string(what) + " beschädigt (Formatversion 0)");
        return std::nullopt;
    }
    if (c.version > kSaveFormatVersion) {
        fail(error, std::string(what) + " aus einer neueren Spielversion (Format " +
                        std::to_string(c.version) + ", unterstützt bis " +
                        std::to_string(kSaveFormatVersion) + ")");
        return std::nullopt;
    }
    if ((flags & ~kFlagZstd) != 0 || reserved != 0) {
        fail(error, std::string(what) + " beschädigt (unbekannte Kennzeichen im Kopf)");
        return std::nullopt;
    }
    if (meta_size > bytes.size() - kHeaderSize || stored_size != bytes.size() - kHeaderSize - meta_size) {
        fail(error, std::string(what) + " unvollständig oder beschädigt (Länge passt nicht zum Kopf)");
        return std::nullopt;
    }
    if (c.raw_size > kMaxPayload || (!c.compressed && stored_size != c.raw_size)) {
        fail(error, std::string(what) + " beschädigt (Länge der Nutzdaten)");
        return std::nullopt;
    }
    const auto meta_bytes = bytes.subspan(kHeaderSize, meta_size);
    auto meta = parse_meta(
        std::string_view(reinterpret_cast<const char*>(meta_bytes.data()), meta_bytes.size()), error);
    if (!meta) {
        return std::nullopt;
    }
    c.meta = std::move(*meta);
    c.stored = bytes.subspan(kHeaderSize + meta_size);
    return c;
}

/// Nutzdaten entpacken und mit dem Prüfwert aus dem Kopf vergleichen.
std::optional<std::vector<std::uint8_t>> unpack(const Container& c, std::string* error) {
    std::vector<std::uint8_t> payload;
    if (c.compressed) {
        payload.resize(static_cast<std::size_t>(c.raw_size));
        const std::size_t n =
            ZSTD_decompress(payload.data(), payload.size(), c.stored.data(), c.stored.size());
        if (ZSTD_isError(n) != 0 || n != c.raw_size) {
            fail(error, "Nutzdaten beschädigt (Entpacken fehlgeschlagen)");
            return std::nullopt;
        }
    } else {
        payload.assign(c.stored.begin(), c.stored.end());
    }
    if (fnv1a(payload) != c.payload_hash) {
        fail(error, "Nutzdaten beschädigt (Prüfwert stimmt nicht)");
        return std::nullopt;
    }
    return payload;
}

// ─── Befehle ────────────────────────────────────────────────────────────────

void write_commands(ByteWriter& out, const std::vector<LoggedCommand>& commands) {
    out.u64(commands.size());
    for (const LoggedCommand& c : commands) {
        out.i64(c.day);
        out.u64(static_cast<std::uint64_t>(c.command.type));
        out.i64(c.command.state);
        out.i64(c.command.value.raw());
        out.u64(static_cast<std::uint64_t>(c.command.kind));
        out.i64(c.command.target);                             // seit Format 2 (M8)
        out.u64(static_cast<std::uint64_t>(c.command.action)); // seit Format 2 (M8)
    }
}

std::vector<LoggedCommand> read_commands(ByteReader& in) {
    const std::uint64_t count = in.u64();
    std::vector<LoggedCommand> commands;
    if (count > in.remaining() / 56) {
        in.fail("Befehlsprotokoll beschädigt (Anzahl)");
        return commands;
    }
    commands.resize(static_cast<std::size_t>(count));
    std::int64_t previous = 0;
    for (LoggedCommand& c : commands) {
        c.day = in.i64();
        const std::uint64_t type = in.u64();
        const std::int64_t state = in.i64();
        c.command.value = Fixed::from_raw(in.i64());
        const std::uint64_t kind = in.u64();
        const std::int64_t target = in.i64();
        const std::uint64_t action = in.u64();
        // Tage aufsteigend; Art und Steuerart müssen in ihren Typ passen (ungültige Werte innerhalb des
        // Typs verwirft execute wie beim Spielen).
        if (c.day < previous || type > 0xFF || kind > 0xFF || action > 0xFF || state < kNone ||
            state > INT32_MAX || target < kNone || target > INT32_MAX) {
            in.fail("Befehlsprotokoll beschädigt");
            break;
        }
        previous = c.day;
        c.command.type = static_cast<CommandType>(type);
        c.command.state = static_cast<Index>(state);
        c.command.kind = static_cast<TaxKind>(kind);
        c.command.target = static_cast<Index>(target);
        c.command.action = static_cast<CampaignAction>(action);
    }
    return commands;
}

// ─── Prüfung des gelesenen Zustands ─────────────────────────────────────────

/// Alle Indizes und Größen, auf die sich die Simulation verlässt; leer, wenn alles passt.
std::string validate_state(const Simulation& sim) {
    const World& w = sim.world();
    const std::size_t L = w.locations.size();
    const std::size_t S = w.states.size();
    const std::size_t P = w.professions.size();
    const auto in = [](Index i, std::size_t n) { return i >= 0 && static_cast<std::size_t>(i) < n; };
    const auto none_or_in = [&](Index i, std::size_t n) { return i == kNone || in(i, n); };
    const auto block_ok = [&](const PopBlock& b) {
        return in(b.location, L) && in(b.profession, P) && in(b.confession, w.confessions.size()) &&
               in(b.culture, w.cultures.size());
    };

    if (!is_valid(w.date) || sim.days_elapsed() < 0) {
        return "Datum ungültig";
    }
    if (!StateAccess::streams_valid(sim)) {
        return "Zufallsstrom ungültig";
    }
    for (const State& s : w.states) {
        if (!none_or_in(s.capital, L)) {
            return "Hauptstadt von " + s.tag + " ungültig";
        }
    }
    for (const PopBlock& b : w.pops) {
        if (!block_ok(b)) {
            return "Bevölkerungsblock mit ungültigem Verweis";
        }
    }
    std::int64_t end = 0;
    for (std::size_t i = 0; i < L; ++i) {
        const Location& loc = w.locations[i];
        const std::int64_t first = loc.first_pop;
        const std::int64_t count = loc.pop_count;
        if (!none_or_in(loc.owner, S) || first < 0 || count < 0 ||
            first + count > static_cast<std::int64_t>(w.pops.size())) {
            return "Ort " + loc.id + " ungültig";
        }
        if (count == 0) {
            continue;
        }
        // Blöcke liegen je Ort zusammenhängend und überschneiden sich nicht (parallele Rechnung).
        if (first < end) {
            return "Blöcke von Ort " + loc.id + " überschneiden sich";
        }
        end = first + count;
        for (std::int64_t p = first; p < end; ++p) {
            if (w.pops[static_cast<std::size_t>(p)].location != static_cast<Index>(i)) {
                return "Blöcke von Ort " + loc.id + " gehören zu einem anderen Ort";
            }
        }
    }
    if (w.arrivals.size() != L) {
        return "Zugänge passen nicht zu den Orten";
    }
    for (std::size_t i = 0; i < L; ++i) {
        for (const PopBlock& b : w.arrivals[i]) {
            if (!block_ok(b) || b.location != static_cast<Index>(i)) {
                return "Zugang mit ungültigem Verweis";
            }
        }
    }

    const std::size_t G = w.goods.size();
    const std::size_t sizes[][2] = {
        {w.snapshots.size(), L},
        {w.snapshot_targets.size(), L * P},
        {w.balance_current.size(), L},
        {w.balance_previous.size(), L},
        {w.economy.size(), L},
        {w.business_shares.size(), L * w.businesses.size()},
        {w.location_supply.size(), L * G},
        {w.location_demand.size(), L * G},
        {w.location_income.size(), L * P},
        {w.location_coverage.size(), L * P * w.needs.size()},
        {w.location_development.size(), L},
        {w.market_states.size(), w.markets.size()},
    };
    for (const auto& [actual, expected] : sizes) {
        if (actual != expected) {
            return "Größe des Wirtschafts- oder Bevölkerungszustands passt nicht";
        }
    }
    for (const MarketState& m : w.market_states) {
        for (const std::vector<Fixed>* v : {&m.price, &m.buy_ratio, &m.sell_ratio, &m.import_share, &m.supply,
                                            &m.demand, &m.imports, &m.exports}) {
            if (v->size() != G) {
                return "Größe des Marktzustands passt nicht";
            }
        }
    }
    for (const ChronicleEntry& e : w.chronicle) {
        if (!none_or_in(e.location, L) || !none_or_in(e.state, S)) {
            return "Chronik mit ungültigem Verweis";
        }
    }
    // Zollgebiete (seit M9): Unionen der Staaten und Anträge, Gebiete und Anteile je Markt.
    const std::size_t U = w.customs_unions.size();
    if (U > 0) {
        if (w.customs_revenue.size() != U) {
            return "Größe der Vereinskassen passt nicht";
        }
        for (const State& s : w.states) {
            if (!none_or_in(s.customs_union, U)) {
                return "Zollunion von " + s.tag + " ungültig";
            }
        }
        for (const CustomsRequest& r : w.customs_requests) {
            if (!in(r.state, S) || !none_or_in(r.customs_union, U)) {
                return "Zollantrag mit ungültigem Verweis";
            }
        }
        for (const MarketState& m : w.market_states) {
            if (m.zone_shares.size() != m.zones.size() || m.foreign_share.size() != m.zones.size() * G) {
                return "Größe der Zollgebiete eines Marktes passt nicht";
            }
            for (std::size_t i = 0; i < m.zones.size(); ++i) {
                if (!in(m.zones[i], U + S) || (i > 0 && m.zones[i] <= m.zones[i - 1])) {
                    return "Zollgebiet eines Marktes ungültig";
                }
            }
        }
    }
    // Diplomatie (seit M9, Schritt b1): Beziehungen je Paar, Gesandte und Verträge zwischen Staaten.
    if (!w.relations.empty()) {
        const Fixed limit = Fixed::from_int(200);
        if (w.relations.size() != relation_count(S)) {
            return "Größe der Beziehungen passt nicht";
        }
        for (const Fixed r : w.relations) {
            if (r < -limit || r > limit) {
                return "Beziehung außerhalb von −200 bis 200";
            }
        }
        for (std::size_t i = 0; i < w.envoys.size(); ++i) {
            const Envoy& e = w.envoys[i];
            if (!in(e.from, S) || !in(e.to, S) || e.from == e.to || !is_valid(e.since)) {
                return "Gesandter mit ungültigem Verweis";
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (w.envoys[j].from == e.from && w.envoys[j].to == e.to) {
                    return "Gesandter doppelt";
                }
            }
        }
        for (const Treaty& t : w.treaties) {
            if (!in(t.from, S) || !in(t.to, S) || t.from == t.to || !is_valid(t.since) ||
                (t.kind != TreatyKind::Alliance && t.kind != TreatyKind::Guarantee)) {
                return "Vertrag mit ungültigem Verweis";
            }
        }
        // Bundesversammlung und diplomatische Spiele (seit Schritt b2).
        const auto motion_target_ok = [&](MotionKind kind, Index target) {
            switch (kind) {
            case MotionKind::CustomsUnification:
                return in(target, w.customs_unions.size());
            case MotionKind::Intervention:
                return in(target, S);
            case MotionKind::Reform:
            case MotionKind::Presidency:
                return target == kNone;
            }
            return false;
        };
        if (!none_or_in(w.diet_presidency, S) || w.motions.size() > 1) {
            return "Bundesversammlung ungültig";
        }
        for (const Motion& m : w.motions) {
            if (!in(m.applicant, S) || !motion_target_ok(m.kind, m.target) || !is_valid(m.filed)) {
                return "Antrag mit ungültigem Verweis";
            }
        }
        for (const MotionResult& r : w.motion_results) {
            if (!in(r.applicant, S) || !motion_target_ok(r.kind, r.target) || !none_or_in(r.opponent, S) ||
                !is_valid(r.date)) {
                return "Abstimmung mit ungültigem Verweis";
            }
        }
        const auto demand_ok = [&](DemandKind kind, Index customs_union) {
            return kind == DemandKind::CustomsUnion
                       ? in(customs_union, w.customs_unions.size())
                       : (kind == DemandKind::Leadership || kind == DemandKind::BreakAlliance) &&
                             customs_union == kNone;
        };
        for (const DiplomaticPlay& p : w.plays) {
            if (!in(p.demander, S) || !in(p.target, S) || p.demander == p.target ||
                !demand_ok(p.kind, p.customs_union) || p.stage < 1 || p.stage > 3 || !is_valid(p.started) ||
                !is_valid(p.stage_started)) {
                return "diplomatisches Spiel mit ungültigem Verweis";
            }
            for (const std::vector<Index>* side : {&p.demander_side, &p.target_side}) {
                for (const Index s : *side) {
                    if (!in(s, S)) {
                        return "Unterstützer mit ungültigem Verweis";
                    }
                }
            }
        }
        for (const PlayResult& r : w.play_results) {
            if (!in(r.demander, S) || !in(r.target, S) || !demand_ok(r.kind, r.customs_union) ||
                r.stage < 1 || r.stage > 3 || !is_valid(r.started) || !is_valid(r.ended) ||
                static_cast<int>(r.outcome) > static_cast<int>(PlayOutcome::War)) {
                return "Spielergebnis mit ungültigem Verweis";
            }
        }
    }
    // Militär (seit M10): Heere mit gültigem Staat, Ort und Garnison, eindeutigen Nummern unter der nächsten.
    if (w.military_rules.active) {
        if (w.next_army_id < 1) {
            return "Nummer des nächsten Heeres ungültig";
        }
        std::set<std::int64_t> ids;
        for (const Army& a : w.armies) {
            if (a.id < 1 || a.id >= w.next_army_id || !ids.insert(a.id).second) {
                return "Heer mit ungültiger oder doppelter Nummer";
            }
            if (!in(a.state, S) || !in(a.location, w.locations.size()) ||
                !none_or_in(a.garrison, w.garrisons.size()) || a.soldiers < Fixed::zero() ||
                a.guns < Fixed::zero() || a.morale < Fixed::zero() || a.morale > Fixed::one() ||
                static_cast<int>(a.status) > static_cast<int>(ArmyStatus::Retreat) ||
                !none_or_in(a.previous, w.locations.size()) || a.days_here < 0 ||
                a.leadership < Fixed::zero() || a.leadership > Fixed::from_int(3)) {
                return "Heer mit ungültigem Verweis oder Wert";
            }
            // Weg (seit a2): nur auf dem Marsch, der Bahn oder dem Rückzug, mit gültigen Orten und Strecken.
            const bool moving = a.status == ArmyStatus::March || a.status == ArmyStatus::Rail ||
                                a.status == ArmyStatus::Retreat;
            if (moving == a.route.empty() || (!moving && a.days_left != Fixed::zero())) {
                return "Heer mit ungültigem Weg";
            }
            for (const RouteStep& r : a.route) {
                const bool rail = r.mode == LegMode::Rail;
                if (!in(r.location, w.locations.size()) ||
                    static_cast<int>(r.mode) > static_cast<int>(LegMode::Rail) ||
                    (rail ? !in(r.railway, w.railways.size()) : r.railway != kNone)) {
                    return "Heer mit ungültigem Weg";
                }
            }
        }
        for (const Mobilization& m : w.mobilizations) {
            if (static_cast<int>(m.phase) > static_cast<int>(MobilizationPhase::Demobilizing) || m.days < 0) {
                return "Mobilmachung ungültig";
            }
        }
        for (const std::int32_t b : w.rail_backlog) {
            if (b < 0) {
                return "Belegung einer Bahnstrecke ungültig";
            }
        }
        // Krieg (seit a3): Seiten getrennt und gültig, Kontrolle, Schlachten und Belagerungen in Orten,
        // Verwundete, Gefangene und Gefallene nicht negativ.
        std::set<std::int64_t> wars;
        for (const War& war : w.wars) {
            if (war.id < 1 || war.id >= w.next_war_id || !wars.insert(war.id).second ||
                war.attackers.empty() || war.defenders.empty() || !is_valid(war.started) ||
                static_cast<int>(war.cause) > static_cast<int>(WarCause::Execution)) {
                return "Krieg mit ungültiger Nummer oder Seite";
            }
            std::set<Index> seen;
            for (const std::vector<Index>* side : {&war.attackers, &war.defenders}) {
                for (const Index s : *side) {
                    if (!in(s, S) || !seen.insert(s).second) {
                        return "Krieg mit ungültiger Nummer oder Seite";
                    }
                }
            }
            // seit a4: Ziele und Bilanz
            for (const WarGoal& g : war.goals) {
                if (static_cast<int>(g.kind) < 1 ||
                    static_cast<int>(g.kind) > static_cast<int>(WarGoalKind::Indemnity) || g.param < kNone) {
                    return "Kriegsziel ungültig";
                }
            }
            if (war.attacker_strength < Fixed::zero() || war.defender_strength < Fixed::zero() ||
                war.attacker_inflicted < Fixed::zero() || war.defender_inflicted < Fixed::zero() ||
                war.attacker_destroyed < 0 || war.defender_destroyed < 0 || !is_valid(war.last_change)) {
                return "Bilanz eines Krieges ungültig";
            }
        }
        for (const std::uint8_t b : w.left_federation) {
            if (b > 1) {
                return "Austritt aus dem Bund ungültig";
            }
        }
        for (const WarResult& r : w.war_results) {
            if (!in(r.attacker, S) || !in(r.defender, S) || r.winner < -1 || r.winner > 1 || r.battles < 0 ||
                r.transfers < 0 || !is_valid(r.started) || !is_valid(r.ended) ||
                static_cast<int>(r.cause) > static_cast<int>(WarCause::Execution)) {
                return "Kriegsergebnis ungültig";
            }
        }
        if (w.next_war_id < 1) {
            return "Nummer des nächsten Krieges ungültig";
        }
        for (const Index c : w.control) {
            if (!none_or_in(c, S)) {
                return "Kontrolle eines Ortes ungültig";
            }
        }
        std::set<Index> places;
        for (const Battle& b : w.battles) {
            if (!in(b.location, w.locations.size()) || !places.insert(b.location).second || b.days < 0 ||
                !is_valid(b.started) || b.attacker_losses < Fixed::zero() ||
                b.defender_losses < Fixed::zero()) {
                return "Schlacht ungültig";
            }
        }
        places.clear();
        for (const Siege& g : w.sieges) {
            const bool fortress = std::any_of(w.fortresses.begin(), w.fortresses.end(),
                                              [&](const Fortress& f) { return f.location == g.location; });
            if (!fortress || !places.insert(g.location).second || !in(g.besieger, S) ||
                g.progress < Fixed::zero() || g.progress > Fixed::one() || !is_valid(g.started)) {
                return "Belagerung ungültig";
            }
        }
        for (const Convalescents& c : w.convalescents) {
            if (!in(c.state, S) || c.soldiers < Fixed::zero() || !is_valid(c.returns)) {
                return "Verwundete ungültig";
            }
        }
        for (const Prisoners& p : w.prisoners) {
            if (!in(p.state, S) || !in(p.captor, S) || p.soldiers < Fixed::zero()) {
                return "Gefangene ungültig";
            }
        }
        for (const Fixed d : w.war_dead) {
            if (d < Fixed::zero()) {
                return "Gefallene ungültig";
            }
        }
    } else if (!w.armies.empty()) {
        return "Heere ohne Militärdaten";
    }
    // Wahlen (seit M8): Wahlkreise des eigenen Parlaments, Stimmen je Strömung.
    const std::size_t C = w.currents.size();
    if (!w.parliaments.empty() && (w.campaign.size() != w.electoral_districts.size() * C ||
                                   w.party_funds.size() != C || w.party_donations.size() != C)) {
        return "Größe des Wahlkampfs passt nicht";
    }
    for (const ElectionResult& e : w.elections) {
        if (!in(e.parliament, w.parliaments.size()) || !is_valid(e.day)) {
            return "Wahl mit ungültigem Verweis";
        }
        const auto own = [&](Index d) {
            return in(d, w.electoral_districts.size()) &&
                   w.electoral_districts[static_cast<std::size_t>(d)].parliament == e.parliament;
        };
        for (const DistrictResult& d : e.districts) {
            if (!own(d.district) || d.votes.size() != C || !none_or_in(d.winner, C)) {
                return "Wahlergebnis mit ungültigem Verweis";
            }
        }
        const std::size_t factions = w.parliaments[static_cast<std::size_t>(e.parliament)].factions.size();
        for (const Deputy& a : e.deputies) {
            if (!own(a.district) || !in(a.current, C) || !none_or_in(a.faction, factions)) {
                return "Abgeordneter mit ungültigem Verweis";
            }
        }
    }
    // Ereignisse (seit M11): Verfassung, Gesetze, Verweise der Ereignischronik und offenen Ereignisse,
    // Merkmale geordnet und eindeutig, Zustimmung und Unruhe im Wertebereich.
    if (w.event_rules.active) {
        for (const State& s : w.states) {
            if (s.constitution > Constitution::Constitutional || s.law.franchise > Franchise::Manhood) {
                return "Verfassung von " + s.tag + " ungültig";
            }
            for (std::size_t g = 0; g < s.laws.size(); ++g) {
                if (!in(s.laws[g], w.law_groups[g].options.size())) {
                    return "Gesetz von " + s.tag + " ungültig";
                }
            }
        }
        const auto event_ok = [&](Index event, Index state) {
            if (!in(event, w.events.size())) {
                return false;
            }
            return w.events[static_cast<std::size_t>(event)].scope == EventScope::World ? state == kNone
                                                                                        : in(state, S);
        };
        for (const EventRecord& r : w.event_log) {
            if (!event_ok(r.event, r.state) ||
                !in(r.option, w.events[static_cast<std::size_t>(r.event)].options.size()) ||
                !is_valid(r.date) || w.date < r.date) {
                return "Ereignischronik mit ungültigem Verweis";
            }
        }
        for (const PendingEvent& e : w.pending_events) {
            if (!event_ok(e.event, e.state) || !is_valid(e.since) || !is_valid(e.due) || e.due < e.since) {
                return "offenes Ereignis mit ungültigem Verweis";
            }
        }
        for (const ScheduledEvent& e : w.scheduled_events) {
            if (!event_ok(e.event, e.state) || !is_valid(e.due)) {
                return "geplantes Ereignis mit ungültigem Verweis";
            }
        }
        for (std::size_t i = 0; i < w.event_flags.size(); ++i) {
            const EventFlag& f = w.event_flags[i];
            if (!none_or_in(f.state, S) || f.name.empty() ||
                (i > 0 && !(std::tie(w.event_flags[i - 1].state, w.event_flags[i - 1].name) <
                            std::tie(f.state, f.name)))) {
                return "Merkmal ungültig oder nicht geordnet";
            }
        }
        for (const Fixed a : w.approval) {
            if (a < Fixed::from_int(-10) || a > Fixed::from_int(10)) {
                return "Zustimmung außerhalb von −10 bis 10";
            }
        }
        for (const Fixed u : w.unrest) {
            if (u < Fixed::zero() || u > Fixed::from_int(100)) {
                return "Unruhe außerhalb von 0 bis 100";
            }
        }
        for (const TimedEffect& t : w.timed_effects) {
            if (!in(t.state, S) || !none_or_in(t.event, w.events.size()) || !is_valid(t.until)) {
                return "befristete Wirkung mit ungültigem Verweis";
            }
        }
        if (!none_or_in(w.event_player, S)) {
            return "Spieler der Ereignisse ungültig";
        }
        for (std::size_t i = 0; i < w.revolts.size(); ++i) {
            const Revolt& r = w.revolts[i];
            if (!in(r.state, S) || !none_or_in(r.helper, S) ||
                static_cast<std::size_t>(r.kind) >= kRevoltKinds || !is_valid(r.start) || w.date < r.start ||
                std::any_of(w.revolts.begin(), w.revolts.begin() + static_cast<std::ptrdiff_t>(i),
                            [&](const Revolt& other) { return other.state == r.state; })) {
                return "Aufstand ungültig";
            }
        }
        for (std::size_t j = 0; j < w.journal_states.size(); ++j) {
            const JournalState& js = w.journal_states[j];
            if (!none_or_in(js.stage, w.journals[j].stages.size()) ||
                (js.stage != kNone && !is_valid(js.since))) {
                return "Journal " + w.journals[j].id + " ungültig";
            }
        }
        for (const JournalStep& step : w.journal_history) {
            if (!in(step.journal, w.journals.size()) ||
                !in(step.stage, w.journals[static_cast<std::size_t>(step.journal)].stages.size()) ||
                !is_valid(step.date)) {
                return "Weg eines Journals ungültig";
            }
        }
        for (const VoteResult& r : w.vote_results) {
            if (!in(r.vote, w.votes.size()) || !is_valid(r.date) || r.yes < 0 || r.no < 0) {
                return "Abstimmung ungültig";
            }
        }
        for (const RevoltRecord& r : w.revolt_history) {
            if (!in(r.state, S) || !none_or_in(r.helper, S) ||
                static_cast<std::size_t>(r.kind) >= kRevoltKinds ||
                static_cast<int>(r.outcome) > static_cast<int>(RevoltOutcome::Victory) ||
                !is_valid(r.start) || !is_valid(r.end) || r.end < r.start) {
                return "beendeter Aufstand ungültig";
            }
        }
        if (w.diet_suspended > 1) {
            return "Bundesversammlung ungültig";
        }
        if (!none_or_in(w.federation.diet, w.federation_templates.size()) ||
            (w.federation.diet != kNone && w.federation.members.size() != S) ||
            (w.federation.diet == kNone && !w.federation.members.empty())) {
            return "Bund ungültig";
        }
        // Jahresreihen (seit M13a4): je Stichtag alle Staaten und Spalten, Stichtage aufsteigend.
        const std::size_t columns = yearbook_columns(w);
        for (std::size_t i = 0; i < w.yearbook.size(); ++i) {
            const YearRecord& r = w.yearbook[i];
            if (!w.legacy_rules.active || !is_valid(r.date) || r.values.size() != S * columns ||
                (i > 0 && !(w.yearbook[i - 1].date < r.date))) {
                return "Jahresreihe ungültig";
            }
        }
    }
    // KI (seit M12): je Staat und Rolle keine oder eine Person dieses Staates in dieser Rolle.
    if (w.ai_rules.active) {
        if (w.offices.size() != S * kPersonRoles) {
            return "Ämter ungültig";
        }
        for (std::size_t i = 0; i < w.offices.size(); ++i) {
            const Index person = w.offices[i];
            if (person == kNone) {
                continue;
            }
            if (!in(person, w.persons.size()) ||
                w.persons[static_cast<std::size_t>(person)].state != static_cast<Index>(i / kPersonRoles) ||
                static_cast<std::size_t>(w.persons[static_cast<std::size_t>(person)].role) !=
                    i % kPersonRoles) {
                return "Amt mit ungültiger Person";
            }
        }
        if (w.ai_strategy.size() != S) {
            return "Strategie der KI ungültig";
        }
        for (const AiStrategy& st : w.ai_strategy) {
            if (!none_or_in(st.rival, S) || st.quiet_months < 0) {
                return "Strategie der KI ungültig";
            }
        }
        for (const AiDecision& d : w.ai_log) {
            if (!in(d.state, S) || static_cast<std::size_t>(d.kind) >= kAiDecisionKinds ||
                d.reason_count > kAiReasons || !is_valid(d.date)) {
                return "KI-Protokoll ungültig";
            }
            for (const AiReason& r : d.reasons) {
                if (static_cast<std::size_t>(r.factor) >= kAiFactors || !none_or_in(r.other, S)) {
                    return "KI-Protokoll ungültig";
                }
            }
        }
        if (w.game_settings.difficulty != kNone &&
            !in(w.game_settings.difficulty, w.ai_difficulties.size())) {
            return "Spieleinstellung ungültig";
        }
        if (w.player_current != kNone && !in(w.player_current, w.currents.size())) {
            return "Spielerströmung ungültig";
        }
        for (const AiCooldown& c : w.ai_cooldowns) {
            if (!in(c.state, S) || static_cast<std::size_t>(c.kind) >= kAiDecisionKinds ||
                !is_valid(c.until)) {
                return "Abkühlung der KI ungültig";
            }
        }
    }
    return {};
}

} // namespace

// ─── Inhaltsfingerabdruck ───────────────────────────────────────────────────

namespace {

void condition_hash(Fnv& h, const Condition& c) {
    h.u64(static_cast<std::uint64_t>(c.kind));
    h.u64(static_cast<std::uint64_t>(c.op));
    h.i64(c.a);
    h.i64(c.b);
    h.fixed(c.value);
    h.i64(to_day_number(c.date));
    h.text(c.name);
    h.u64(c.list.size());
    for (const Index i : c.list) {
        h.i64(i);
    }
    h.u64(c.children.size());
    for (const Condition& child : c.children) {
        condition_hash(h, child);
    }
}

void effects_hash(Fnv& h, const std::vector<Effect>& effects) {
    h.u64(effects.size());
    for (const Effect& e : effects) {
        h.u64(static_cast<std::uint64_t>(e.kind));
        h.i64(e.a);
        h.i64(e.b);
        h.fixed(e.value);
        h.i64(e.days);
        h.text(e.name); // Merkmal, Chroniktext oder Text der Befristung
        effects_hash(h, e.children);
        for (const Fixed f : {e.timed.tax, e.timed.morale, e.timed.unrest, e.timed.relation}) { // seit a2
            h.fixed(f);
        }
        h.u64(e.weights.size());
        for (std::size_t i = 0; i < e.weights.size(); ++i) {
            h.fixed(e.weights[i]);
            effects_hash(h, e.branches[i]);
        }
        h.u64(e.goals.size()); // seit b3 (Inhaltsstand 6)
        for (const std::int64_t g : e.goals) {
            h.i64(g);
        }
        if (e.kind == EffectKind::Federation) { // seit M13a2
            h.u64(e.states.size());
            for (const Index st : e.states) {
                h.i64(st);
            }
            h.u64(e.excluded.size());
            for (const Index st : e.excluded) {
                h.i64(st);
            }
        }
    }
}

/// Ein Ereignis ohne Text, Quelle und Sicherheit; Titel und Optionstexte stehen in der Chronik.
void event_hash(Fnv& h, const EventDef& e) {
    h.text(e.id);
    h.text(e.title);
    h.u64(static_cast<std::uint64_t>(e.kind));
    h.flag(e.turning_point);
    h.u64(static_cast<std::uint64_t>(e.scope));
    condition_hash(h, e.condition);
    h.i64(e.wait);
    h.flag(e.has_fixed_date);
    h.i64(e.has_fixed_date ? to_day_number(e.fixed_date) : 0);
    h.flag(e.repeatable);
    h.flag(e.follow_only); // seit a2
    h.i64(e.spacing);
    h.i64(e.deadline);
    h.u64(e.options.size());
    for (const EventOption& o : e.options) {
        h.text(o.id);
        h.text(o.text);
        h.flag(o.historical);
        h.fixed(o.weight);
        h.u64(o.weight_modifiers.size());
        for (const WeightModifier& m : o.weight_modifiers) {
            condition_hash(h, m.condition);
            h.fixed(m.plus);
        }
        condition_hash(h, o.condition);
        effects_hash(h, o.effects);
        h.flag(o.has_ai); // seit M12 (a3)
        if (o.has_ai) {
            for (const Fixed f : o.ai_lean) {
                h.fixed(f);
            }
        }
    }
}

} // namespace

ContentFingerprint content_fingerprint(const World& w) {
    ContentFingerprint fp;
    auto& p = fp.parts;
    {
        Fnv h;
        hash_rules(h, w.rules);
        p[0] = h.value();
    }
    p[1] = hash_list(w.terrains, [](Fnv& h, const TerrainType& t) {
        h.text(t.id);
        h.fixed(t.yield_factor);
        h.fixed(t.capacity);
    });
    p[2] = hash_list(w.confessions, [](Fnv& h, const Confession& c) {
        h.text(c.id);
        h.fixed(c.literacy_factor);
    });
    p[3] = hash_list(w.cultures, [](Fnv& h, const Culture& c) { h.text(c.id); });
    p[4] = hash_list(w.professions, [](Fnv& h, const Profession& pr) {
        h.text(pr.id);
        h.u64(static_cast<std::uint64_t>(pr.stratum));
        h.flag(pr.taxable);
        h.fixed(pr.share_city);
        h.fixed(pr.share_rural);
        h.flag(pr.agrarian);
        h.fixed(pr.literacy);
        h.i64(pr.city_profession);
        h.fixed(pr.agrarian_levy);
        h.fixed(pr.self_employed);
    });
    p[5] = hash_list(w.goods, [](Fnv& h, const Good& g) {
        h.text(g.id);
        h.fixed(g.base_price);
        h.fixed(g.world_factor);
    });
    p[6] = hash_list(w.needs, [](Fnv& h, const Need& n) {
        h.text(n.id);
        h.flag(n.hunger);
        for (const std::vector<GoodAmount>& basket : n.basket) {
            h.amounts(basket);
        }
    });
    p[7] = hash_list(w.businesses, [](Fnv& h, const BusinessType& b) {
        h.text(b.id);
        h.i64(b.profession);
        h.amounts(b.outputs);
        h.amounts(b.inputs);
        h.flag(b.agrarian);
        h.u64(static_cast<std::uint64_t>(b.siting));
        h.i64(b.deposit);
        h.flag(b.trade);
        h.fixed(b.weight);
        h.u64(b.terrain_factors.size());
        for (const Fixed f : b.terrain_factors) {
            h.fixed(f);
        }
    });
    p[8] = hash_list(w.markets, [](Fnv& h, const Market& m) {
        h.text(m.id);
        h.u64(m.landscapes.size());
        for (const std::string& landscape : m.landscapes) {
            h.text(landscape);
        }
    });
    // Eisenbahnen (seit M7) bestimmen die Fracht zwischen Märkten und gehören deshalb zu den Märkten.
    {
        Fnv h;
        h.u64(p[8]);
        h.u64(w.railways.size());
        for (const Railway& r : w.railways) {
            h.text(r.id);
            h.u64(r.sections.size());
            for (const RailSection& s : r.sections) {
                h.i64(s.from);
                h.i64(s.to);
                h.i64(to_day_number(s.opened));
            }
        }
        p[8] = h.value();
    }
    p[9] = hash_list(w.deposits, [](Fnv& h, const std::string& d) { h.text(d); });
    // Kurzname der Staaten und Name der Orte stehen in Chronikeinträgen und damit in der Prüfsumme.
    p[10] = hash_list(w.states, [](Fnv& h, const State& s) {
        h.text(s.tag);
        h.text(s.short_name);
        h.i64(s.confession);
    });
    // Verwaltungseinheiten (seit M7) gehören zur politischen Gliederung wie die Kreise.
    {
        Fnv h;
        h.u64(w.districts.size());
        for (const District& d : w.districts) {
            h.text(d.id);
        }
        h.u64(w.admin_units.size());
        for (const AdminUnit& a : w.admin_units) {
            h.text(a.id);
            h.text(a.kind);
            h.i64(a.parent);
        }
        p[11] = h.value();
    }
    p[12] = hash_list(w.locations, [](Fnv& h, const Location& l) {
        h.text(l.id);
        h.text(l.name);
        h.i64(l.terrain);
        h.i64(l.district);
        h.fixed(l.longitude);
        h.fixed(l.latitude);
        h.fixed(l.area_km2);
        h.flag(l.is_city);
        h.i64(l.market);
        h.u64(l.deposits);
        h.flag(l.is_port);
        h.i64(l.admin);
        h.flag(l.federal);
    });
    // Politik (seit M8): Strömungen, Interessengruppen, Parlamente und die Wahlordnung der Staaten.
    {
        const auto law = [](Fnv& h, const ElectoralLaw& l) {
            h.u64(static_cast<std::uint64_t>(l.franchise));
            h.u64(static_cast<std::uint64_t>(l.system));
            h.flag(l.indirect);
            h.fixed(l.census_threshold);
        };
        const auto fixeds = [](Fnv& h, const std::vector<Fixed>& values) {
            h.u64(values.size());
            for (const Fixed v : values) {
                h.fixed(v);
            }
        };
        Fnv h;
        h.u64(w.currents.size());
        for (const PoliticalCurrent& c : w.currents) {
            h.text(c.id);
            h.text(c.name); // steht in der Chronik der Wahl
            h.fixed(c.position);
        }
        h.u64(w.interest_groups.size());
        for (const InterestGroup& g : w.interest_groups) {
            h.text(g.id);
            fixeds(h, g.professions);
            fixeds(h, g.confessions);
            h.flag(g.by_literacy);
            fixeds(h, g.leaning);
        }
        h.u64(w.parliaments.size());
        for (const Parliament& pa : w.parliaments) {
            h.text(pa.id);
            h.i64(pa.state);
            h.i64(pa.seats);
            h.i64(to_day_number(pa.election_day));
            law(h, pa.law);
            h.i64(to_day_number(pa.faction_day));
            h.u64(pa.boycott_cultures.size());
            for (const Index c : pa.boycott_cultures) {
                h.i64(c);
            }
            h.u64(pa.boycott_markets.size());
            for (const Index m : pa.boycott_markets) {
                h.i64(m);
            }
            h.u64(pa.factions.size());
            for (const Faction& f : pa.factions) {
                h.text(f.id);
                h.fixed(f.from);
                h.fixed(f.to);
            }
            h.i64(pa.seat);
            h.text(pa.name); // steht in der Chronik der Wahl
            // Seit M13a3: weitere Wahltage und Wahlen eines Bundes (nur, wenn vorhanden).
            if (!pa.re_elections.empty() || !pa.federations.empty()) {
                h.u64(pa.re_elections.size());
                for (const Date& day : pa.re_elections) {
                    h.i64(to_day_number(day));
                }
                h.u64(pa.federations.size());
                for (const Index f : pa.federations) {
                    h.i64(f);
                }
                h.i64(pa.after_founding);
                h.i64(pa.period);
            }
        }
        // Wahlregeln und Regionen (seit M8, Schritt a3).
        const ElectionRules& r = w.election_rules;
        for (const Fixed f :
             {r.male_share, r.young_of_age, r.young_of_age_24, r.turnout_base, r.turnout_literacy,
              r.turnout_city, r.turnout_poverty, r.turnout_min, r.turnout_max, r.poverty_below, r.noise,
              r.position_pull, r.position_noise, r.independent_share}) {
            h.fixed(f);
        }
        for (const std::vector<Fixed>* v : {&r.base, &r.literacy, &r.poverty, &r.city, &r.electors}) {
            fixeds(h, *v);
        }
        h.fixed(r.donations); // Wahlkampf (seit Schritt b2)
        h.fixed(r.poll_error);
        for (const CampaignRule& rule : r.campaign) {
            h.fixed(rule.cost);
            h.fixed(rule.effect);
            h.fixed(rule.cap);
        }
        h.u64(w.political_regions.size());
        for (const PoliticalRegion& region : w.political_regions) {
            h.text(region.id);
            h.u64(region.states.size());
            for (const Index s : region.states) {
                h.i64(s);
            }
            h.u64(region.markets.size());
            for (const Index m : region.markets) {
                h.i64(m);
            }
            fixeds(h, region.leaning);
        }
        h.u64(w.states.size());
        for (const State& s : w.states) {
            h.u64(static_cast<std::uint64_t>(s.constitution));
            law(h, s.law);
        }
        h.u64(w.electoral_districts.size());
        for (const ElectoralDistrict& d : w.electoral_districts) {
            h.text(d.id);
            h.text(d.name); // Anzeige (Wahlabend, Ortsinfo), wie die Namen der Orte
            h.i64(d.parliament);
            h.i64(d.state);
            h.i64(d.seats);
            h.u64(d.locations.size());
            for (const Index l : d.locations) {
                h.i64(l);
            }
        }
        p[13] = h.value();
    }
    // Diplomatie (seit M9): Zollunionen. Die Mitgliedschaft der Staaten ist seit Schritt a2 Zustand
    // (Befehl Zollunion) und steht im Spielstand.
    {
        Fnv h;
        h.u64(w.customs_unions.size());
        for (const CustomsUnion& u : w.customs_unions) {
            h.text(u.id);
            h.text(u.name); // steht in der Chronik bei Beitritt und Austritt
            h.i64(u.leader);
            h.fixed(u.tariff);
            h.u64(u.excluded.size());
            for (const Index s : u.excluded) {
                h.i64(s);
            }
        }
        // Diplomatie (seit Schritt b1): Regeln und Grundbeziehungen, nur wo sie gelten (Deutscher Bund),
        // damit Spielstände von 1648 gültig bleiben. Beziehungen, Gesandte und Verträge sind Zustand.
        if (diplomacy_active(w)) {
            const DiplomacyRules& d = w.diplomacy_rules;
            for (const Fixed f : {d.drift, d.customs_union, d.federation, d.confession, d.alliance,
                                  d.guarantee, d.envoy, d.envoy_limit, d.envoy_cost, d.envoy_people,
                                  d.alliance_min, d.guarantee_min, d.rival_below}) {
                h.fixed(f);
            }
            h.u64(d.confession_group.size());
            for (const Index g : d.confession_group) {
                h.i64(g);
            }
            h.u64(w.base_relations.size());
            for (const BaseRelation& r : w.base_relations) {
                h.i64(r.a);
                h.i64(r.b);
                h.fixed(r.value);
            }
            // Bundesversammlung und Regeln der Spiele (seit Schritt b2).
            for (const Fixed f :
                 {d.intervention, d.play_leaning, d.play_break, d.play_stage, d.play_yield, d.mobilization}) {
                h.fixed(f);
            }
            h.i64(d.play_stage_days);
            h.text(w.diet.id);
            h.text(w.diet.name); // steht in der Chronik der Anträge
            h.i64(w.diet.presidency);
            h.u64(w.diet.plenum.size());
            for (const std::int32_t votes : w.diet.plenum) {
                h.i64(votes);
            }
            const auto diet_hash = [&h](const FederalDiet& diet) {
                h.u64(diet.council.size());
                for (const CouncilVote& vote : diet.council) {
                    h.text(vote.id);
                    h.u64(vote.states.size());
                    for (const Index s : vote.states) {
                        h.i64(s);
                    }
                }
            };
            diet_hash(w.diet);
            // Seit M13a2: Vorlagen für Bünde zur Laufzeit (nur wenn es welche gibt, sonst bleibt der Wert).
            if (!w.federation_templates.empty()) {
                h.u64(w.federation_templates.size());
                for (const FederalDiet& t : w.federation_templates) {
                    h.text(t.id);
                    h.text(t.name);
                    h.text(t.federation_name);
                    h.i64(t.presidency);
                    h.u64(t.plenum.size());
                    for (const std::int32_t votes : t.plenum) {
                        h.i64(votes);
                    }
                    diet_hash(t);
                }
            }
        }
        p[14] = h.value();
    }
    // Militär (seit M10): Regeln, Heerwesen, Garnisonen und Nachbarschaft, nur wo das Militär gilt, damit
    // Spielstände von 1648 gültig bleiben. Heere sind Zustand. kMilitaryContentVersion steigt mit jedem
    // Schritt, der den Zustand des Militärs erweitert: Ein Spielstand eines früheren Schritts passt dann
    // nicht mehr zum Inhalt (speicherstaende.md, Abschnitt 6).
    {
        Fnv h;
        if (w.military_rules.active) {
            const MilitaryRules& r = w.military_rules;
            h.u64(kMilitaryContentVersion);
            h.i64(r.river_rank);
            // Bewegung und Versorgung (seit a2).
            h.fixed(r.march_km);
            h.fixed(r.river_days);
            h.u64(r.terrain_march.size());
            for (const Fixed f : r.terrain_march) {
                h.fixed(f);
            }
            h.fixed(r.rail_km);
            h.fixed(r.rail_capacity);
            h.i64(r.unload_days);
            h.fixed(r.supply_days);
            h.fixed(r.attrition);
            h.fixed(r.attrition_unsupplied);
            h.fixed(r.morale_unsupplied);
            h.fixed(r.morale_recovery);
            h.i64(r.demobilization_days);
            // Krieg, Schlacht und Belagerung (seit a3).
            for (const Fixed f :
                 {r.declaration_relation, r.frontage, r.terrain_defense, r.river_defense, r.fortress_defense,
                  r.gun_value, r.loss_rate, r.morale_loss, r.break_morale, r.break_losses, r.dead_share,
                  r.wounded_return, r.pursuit_prisoners, r.devastation_losses, r.siege_days, r.siege_min_days,
                  r.fortress_garrison}) {
                h.fixed(f);
            }
            h.i64(r.recovery_days);
            // Kriegsziele, Kriegspunkte, Frieden und Generalstab (seit a4).
            for (const Fixed f :
                 {r.goal_territory, r.goal_territory_min, r.goal_annexation, r.goal_leadership,
                  r.goal_customs, r.goal_indemnity, r.points_occupation, r.points_capital, r.points_battles,
                  r.points_army, r.weariness_losses, r.weariness_occupation, r.weariness_month,
                  r.staff_attack, r.staff_retreat, r.staff_radius_km, r.staff_reach_km}) {
                h.fixed(f);
            }
            h.i64(r.stalemate_months);
            h.u64(r.staff ? 1 : 0);
            // Lesezeichen im Krieg (seit b1): Führung, Aufmarsch, Kriege und Bündnisse zum Start, Hinterland.
            h.fixed(r.leadership);
            h.fixed(r.staff_morale); // seit b2
            h.fixed(r.victory_morale);
            h.fixed(r.points_army_share);
            h.u64(r.federal_passage ? 1 : 0); // seit M11 (b3)
            h.u64(w.deployments.size());
            for (const Deployment& d : w.deployments) {
                h.text(d.id);
                h.i64(d.state);
                h.i64(d.location);
                h.text(d.name);
                h.text(d.commander);
                h.fixed(d.leadership);
                h.fixed(d.soldiers);
                h.fixed(d.morale);
                h.u64(d.staff ? 1 : 0);
            }
            h.u64(w.opening_wars.size());
            for (const OpeningWar& o : w.opening_wars) {
                h.text(o.id);
                h.text(o.name);
                h.i64(to_day_number(o.started));
                h.u64(static_cast<std::uint64_t>(o.cause));
                for (const std::vector<Index>* side : {&o.attackers, &o.defenders}) {
                    h.u64(side->size());
                    for (const Index s : *side) {
                        h.i64(s);
                    }
                }
                h.u64(o.goals.size());
                for (const WarGoal& g : o.goals) {
                    h.u64(static_cast<std::uint64_t>(g.kind));
                    h.u64(g.attacker ? 1 : 0);
                    h.i64(g.param);
                }
                h.u64(o.occupied.size()); // seit b2
                for (const Index u : o.occupied) {
                    h.i64(u);
                }
            }
            h.u64(w.opening_treaties.size());
            for (const Treaty& t : w.opening_treaties) {
                h.u64(static_cast<std::uint64_t>(t.kind));
                h.i64(t.from);
                h.i64(t.to);
            }
            h.u64(w.hinterlands.size());
            for (const Hinterland& hl : w.hinterlands) {
                h.i64(hl.state);
                h.fixed(hl.population);
                h.fixed(hl.income);
                h.fixed(hl.replacements);
                h.u64(hl.entries.size());
                for (const Index e : hl.entries) {
                    h.i64(e);
                }
            }
            h.u64(w.fortresses.size());
            for (const Fortress& f : w.fortresses) {
                h.text(f.id);
                h.text(f.name); // steht in der Chronik
                h.i64(f.location);
                h.i64(f.level);
            }
            for (const State& s : w.states) {
                h.fixed(s.army.firepower);
                h.fixed(s.army.guns_per_mille);
                h.fixed(s.army.mobilization);
                h.i64(s.army.mobilization_days);
            }
            h.u64(w.garrisons.size());
            for (const Garrison& g : w.garrisons) {
                h.text(g.id);
                h.text(g.name); // Name des Heeres
                h.i64(g.state);
                h.i64(g.location);
                h.fixed(g.share);
            }
            h.u64(w.neighbors.size());
            for (const std::vector<Neighbor>& list : w.neighbors) {
                h.u64(list.size());
                for (const Neighbor& n : list) {
                    h.i64(n.location);
                    h.fixed(n.distance_km);
                    h.i64(n.border);
                    h.i64(n.river);
                    h.i64(n.ferry_days);
                }
            }
        }
        p[15] = h.value();
    }
    // Ereignisse (seit M11): Gesetzesgruppen, Wünsche der Gruppen, Gesetze zum Start, Regeln und Ereignisse,
    // nur in Welten mit Ereignissen. Texte zählen nicht mit, außer der Chronik einer Wirkung.
    {
        Fnv h;
        if (w.event_rules.active) {
            h.u64(kEventContentVersion);
            const EventRules& r = w.event_rules;
            h.i64(r.deadline);
            // Zustimmung, Unruhe und Aufstände (seit a3).
            for (const Fixed f : {r.approval_rate,
                                  r.unrest_rate,
                                  r.misery,
                                  r.misery_from,
                                  r.misery_span,
                                  r.dearth,
                                  r.dearth_from,
                                  r.dearth_span,
                                  r.constitution[0],
                                  r.constitution[1],
                                  r.constitution[2],
                                  r.contagion,
                                  r.contagion_from,
                                  r.approval_weight,
                                  r.tax_refusal_from,
                                  r.tax_refusal,
                                  r.emigration_from,
                                  r.emigration,
                                  r.revolt_step,
                                  r.revolt_noise,
                                  r.revolt_concession,
                                  r.split,
                                  r.soldiers_reference,
                                  r.concession_unrest,
                                  r.suppression_unrest,
                                  r.suppression_devastation}) {
                h.fixed(f);
            }
            h.i64(r.grain);
            h.text(r.contagion_flag);
            h.i64(r.revolt_min_days);
            h.i64(r.revolt_max_days);
            h.u64(r.split_groups.size());
            for (const Index g : r.split_groups) {
                h.i64(g);
            }
            h.text(r.split_flag);
            h.i64(r.loyalty_group);
            h.text(r.victory_flag);
            for (const Index e : r.outcome_events) {
                h.i64(e);
            }
            h.u64(w.law_groups.size());
            for (const LawGroup& g : w.law_groups) {
                h.text(g.id);
                h.u64(g.options.size());
                for (const LawOption& o : g.options) {
                    h.text(o.id);
                    h.fixed(o.unrest); // seit a3
                }
                h.i64(g.standard);
            }
            for (const InterestGroup& g : w.interest_groups) {
                h.u64(g.wishes.size());
                for (const LawWish& wish : g.wishes) {
                    h.i64(wish.law);
                    h.i64(wish.option);
                    h.fixed(wish.approval);
                }
            }
            h.u64(w.events.size());
            for (const EventDef& e : w.events) {
                event_hash(h, e);
            }
            // Journale und Abstimmungen (seit a4); Titel stehen in der Chronik, Texte nicht.
            h.u64(w.journals.size());
            for (const JournalDef& j : w.journals) {
                h.text(j.id);
                h.text(j.title);
                h.flag(j.has_begin);
                h.i64(j.has_begin ? to_day_number(j.begin) : 0);
                h.flag(j.has_end);
                h.i64(j.has_end ? to_day_number(j.end) : 0);
                h.i64(j.fallback);
                h.u64(j.stages.size());
                for (const JournalStage& st : j.stages) {
                    h.text(st.id);
                    h.text(st.title);
                    h.flag(st.outcome);
                    h.text(st.rating);
                    h.u64(st.transitions.size());
                    for (const JournalTransition& t : st.transitions) {
                        h.i64(t.target);
                        condition_hash(h, t.condition);
                    }
                    effects_hash(h, st.effects);
                }
            }
            h.u64(w.votes.size());
            for (const VoteDef& v : w.votes) {
                h.text(v.id);
                h.text(v.title);
                h.i64(v.parliament);
                h.u64(v.leaning.size());
                for (const Fixed f : v.leaning) {
                    h.fixed(f);
                }
                h.fixed(v.unaligned);
                h.fixed(v.majority);
                effects_hash(h, v.yes);
                effects_hash(h, v.no);
            }
            // Vermächtnis und Ende der Partie (seit M13a4): bestimmen die Jahresreihen im Zustand.
            h.flag(w.scenario.has_end);
            h.i64(to_day_number(w.scenario.end));
            h.flag(w.legacy_rules.active);
            h.u64(w.legacy_rules.dimensions.size());
            for (const LegacyDimension& d : w.legacy_rules.dimensions) {
                h.text(d.id);
                h.fixed(d.base);
                h.u64(d.factors.size());
                for (const LegacyFactor& f : d.factors) {
                    h.u64(static_cast<std::uint64_t>(f.measure));
                    h.fixed(f.weight);
                    h.fixed(f.cap);
                    h.flag(f.has_condition);
                    condition_hash(h, f.condition);
                }
            }
        }
        p[16] = h.value();
    }
    // KI (seit M12): Regeln, Eigenschaften, Staatspersönlichkeiten, Personen und Agenden, nur in Welten mit
    // KI-Daten. Namen der Personen und Ämter stehen in der Chronik und zählen mit.
    {
        Fnv h;
        if (w.ai_rules.active) {
            h.u64(kAiContentVersion);
            const AiRules& r = w.ai_rules;
            h.fixed(r.noise);
            h.i64(r.cooldown_months);
            h.fixed(r.debt_limit);
            h.flag(r.open_agenda);
            h.fixed(r.ruler_weight_absolute);
            h.fixed(r.ruler_weight_constitutional);
            h.i64(r.default_personality);
            h.fixed(r.threshold); // seit a2
            h.i64(r.log_size);
            h.i64(r.tax_cooldown_months);
            h.fixed(r.surplus_from);
            h.fixed(r.unrest_tax_from);
            h.fixed(r.reserve_from);
            h.fixed(r.threat_mobilize);
            h.i64(r.quiet_months);
            h.i64(r.max_alliances); // seit a3
            h.i64(r.campaign_months);
            h.fixed(r.campaign_reserve);
            h.fixed(r.war_ratio);        // seit a4
            h.i64(r.default_difficulty); // seit M13a1
            h.u64(w.ai_difficulties.size());
            for (const DifficultyDef& d : w.ai_difficulties) {
                h.text(d.id);
                h.fixed(d.threshold);
                h.fixed(d.noise);
                h.fixed(d.cooldown);
                h.fixed(d.caution);
                h.flag(d.player_rival);
            }
            const auto inclinations = [&h](const Inclinations& values) {
                for (const Fixed f : values) {
                    h.fixed(f);
                }
            };
            h.u64(w.ai_traits.size());
            for (const TraitDef& t : w.ai_traits) {
                h.text(t.id);
                inclinations(t.inclinations);
            }
            h.u64(w.ai_personalities.size());
            for (const PersonalityDef& pd : w.ai_personalities) {
                h.text(pd.id);
                inclinations(pd.inclinations);
            }
            h.u64(w.persons.size());
            for (const PersonDef& pd : w.persons) {
                h.text(pd.id);
                h.text(pd.name);
                h.text(pd.title);
                h.i64(pd.state);
                h.u64(static_cast<std::uint64_t>(pd.role));
                h.i64(to_day_number(pd.from));
                h.flag(pd.has_to);
                h.i64(pd.has_to ? to_day_number(pd.to) : 0);
                h.flag(pd.has_condition);
                if (pd.has_condition) {
                    condition_hash(h, pd.condition);
                }
                for (const std::int32_t v : pd.values) {
                    h.i64(v);
                }
                h.u64(pd.traits.size());
                for (const Index t : pd.traits) {
                    h.i64(t);
                }
            }
            h.u64(w.agendas.size());
            for (const AgendaDef& a : w.agendas) {
                h.i64(a.personality);
                h.u64(a.goals.size());
                for (const AgendaGoal& g : a.goals) {
                    h.u64(static_cast<std::uint64_t>(g.kind));
                    h.fixed(g.weight);
                    h.i64(g.target);
                    h.flag(g.has_from);
                    h.i64(g.has_from ? to_day_number(g.from) : 0);
                    h.flag(g.has_to);
                    h.i64(g.has_to ? to_day_number(g.to) : 0);
                    h.flag(g.has_condition);
                    if (g.has_condition) {
                        condition_hash(h, g.condition);
                    }
                }
            }
        }
        p[17] = h.value();
    }
    fp.total = MetaReader::content_fingerprint_total(fp.parts);
    return fp;
}

std::vector<std::string_view> content_differences(const ContentFingerprint& a, const ContentFingerprint& b) {
    std::vector<std::string_view> result;
    for (std::size_t i = 0; i < kContentParts.size(); ++i) {
        if (a.parts[i] != b.parts[i]) {
            result.push_back(kContentParts[i]);
        }
    }
    return result;
}

namespace {

std::string content_mismatch(const ContentFingerprint& expected, const ContentFingerprint& actual) {
    std::string parts;
    for (const std::string_view part : content_differences(expected, actual)) {
        parts += parts.empty() ? "" : ", ";
        parts += part;
    }
    return "passt nicht zu den Spieldaten (abweichend: " + parts + ")";
}

} // namespace

// ─── Spielstand ─────────────────────────────────────────────────────────────

std::vector<std::uint8_t> save_game(const Simulation& sim, const SaveMeta& meta) {
    ByteWriter payload;
    const std::size_t state = payload.begin_section("ZUST");
    WriteVisitor writer(payload);
    StateAccess::visit(sim, writer);
    payload.end_section(state);
    const std::size_t protocol = payload.begin_section("PROT");
    payload.u64(sim.start_checksum());
    write_commands(payload, sim.command_log());
    payload.end_section(protocol);

    const World& w = sim.world();
    std::string text;
    add_meta(text, "spiel", UHRWERK_VERSION);
    add_meta(text, "formatversion", std::to_string(kSaveFormatVersion));
    add_meta(text, "szenario", w.scenario.id);
    add_meta(text, "spielerstaat", meta.player_tag);
    if (!meta.player_current.empty()) {
        add_meta(text, "spielerstroemung", meta.player_current); // Parteienmodus (seit M8)
    }
    add_meta(text, "datum", iso_date(w.date));
    add_meta(text, "seed", std::to_string(sim.config().seed));
    add_meta(text, "tage", std::to_string(sim.days_elapsed()));
    add_meta(text, "pruefsumme", hex16(sim.checksum()));
    add_meta(text, "startpruefsumme", hex16(sim.start_checksum()));
    add_meta(text, "befehle", std::to_string(sim.command_log().size()));
    add_content_meta(text, content_fingerprint(w));
    return write_container(kSaveMagic, text, payload.data());
}

namespace {

std::optional<SaveInfo> info_from(const Container& c, std::string* error) {
    MetaReader m(c.meta);
    SaveInfo info;
    info.format_version = c.version;
    info.game_version = m.text("spiel");
    info.scenario = m.text("szenario");
    info.player_tag = m.text("spielerstaat");
    info.player_current = m.optional_text("spielerstroemung");
    info.date = m.date("datum");
    info.seed = m.u64("seed");
    info.days = m.i64("tage");
    info.checksum = m.hex("pruefsumme");
    info.start_checksum = m.hex("startpruefsumme");
    info.commands = m.u64("befehle");
    info.content = m.content(c.version);
    if (!m.ok()) {
        fail(error, m.error());
        return std::nullopt;
    }
    info.compressed = c.compressed;
    info.payload_size = c.raw_size;
    info.stored_size = c.stored.size();
    info.payload_hash = c.payload_hash;
    return info;
}

} // namespace

std::optional<SaveInfo> read_save_info(std::span<const std::uint8_t> bytes, std::string* error) {
    const auto container = read_container(bytes, false, error);
    if (!container) {
        return std::nullopt;
    }
    return info_from(*container, error);
}

LoadedGame load_game(std::span<const std::uint8_t> bytes, World content, unsigned threads) {
    LoadedGame result;
    const auto container = read_container(bytes, false, &result.error);
    if (!container) {
        return result;
    }
    auto info = info_from(*container, &result.error);
    if (!info) {
        return result;
    }
    result.info = *info;
    if (info->format_version != kSaveFormatVersion) {
        result.error =
            "Spielstand im Format " + std::to_string(info->format_version) + " wird nicht unterstützt";
        return result;
    }
    const ContentFingerprint actual = content_fingerprint(content);
    if (actual != info->content) {
        result.error = "Spielstand " + content_mismatch(info->content, actual);
        return result;
    }
    const auto payload = unpack(*container, &result.error);
    if (!payload) {
        return result;
    }

    Simulation sim(std::move(content), SimConfig{info->seed, threads});
    ByteReader in(*payload);
    ByteReader state(in.section("ZUST"));
    ReadVisitor reader(state);
    StateAccess::visit(sim, reader);
    if (!in.failed() && !state.failed() && state.remaining() != 0) {
        state.fail("Zustand enthält überzählige Daten");
    }
    ByteReader protocol(in.section("PROT"));
    const std::uint64_t start_checksum = protocol.u64();
    std::vector<LoggedCommand> commands = read_commands(protocol);
    if (!protocol.failed() && protocol.remaining() != 0) {
        protocol.fail("Befehlsprotokoll enthält überzählige Daten");
    }
    if (!in.failed() && in.remaining() != 0) {
        in.fail("Nutzdaten enthalten überzählige Daten");
    }
    for (const ByteReader* r : {&in, &state, &protocol}) {
        if (r->failed()) {
            result.error = "Spielstand beschädigt: " + r->error();
            return result;
        }
    }
    if (std::string problem = validate_state(sim); !problem.empty()) {
        result.error = "Spielstand beschädigt: " + problem;
        return result;
    }
    if (sim.days_elapsed() != info->days || sim.world().date != info->date ||
        sim.config().seed != info->seed || commands.size() != info->commands ||
        start_checksum != info->start_checksum ||
        (!commands.empty() && commands.back().day > sim.days_elapsed())) {
        result.error = "Spielstand beschädigt: Metadaten passen nicht zum Zustand";
        return result;
    }

    StateAccess::finish_load(sim, std::move(commands), start_checksum);
    if (const std::uint64_t checksum = sim.checksum(); checksum != info->checksum) {
        result.error =
            "Spielstand beschädigt: Prüfsumme " + hex16(checksum) + " statt " + hex16(info->checksum);
        return result;
    }
    result.simulation.emplace(std::move(sim));
    return result;
}

// ─── Wiederholung ───────────────────────────────────────────────────────────

Replay make_replay(const Simulation& sim, std::vector<Checkpoint> checkpoints) {
    Replay r;
    r.game_version = UHRWERK_VERSION;
    r.scenario = sim.world().scenario.id;
    r.seed = sim.config().seed;
    r.content = content_fingerprint(sim.world());
    r.start_checksum = sim.start_checksum();
    r.end_day = sim.days_elapsed();
    r.end_checksum = sim.checksum();
    r.commands = sim.command_log();
    r.checkpoints = std::move(checkpoints);
    return r;
}

bool is_checkpoint_day(const Simulation& sim) {
    const Date& d = sim.world().date;
    return d.month == 1 && d.day == 1;
}

std::vector<std::uint8_t> write_replay(const Replay& replay) {
    ByteWriter payload;
    const std::size_t commands = payload.begin_section("BEFE");
    write_commands(payload, replay.commands);
    payload.end_section(commands);
    const std::size_t checkpoints = payload.begin_section("KONT");
    payload.u64(replay.checkpoints.size());
    for (const Checkpoint& c : replay.checkpoints) {
        payload.i64(c.day);
        payload.u64(c.checksum);
    }
    payload.end_section(checkpoints);

    std::string text;
    add_meta(text, "spiel", replay.game_version);
    add_meta(text, "formatversion", std::to_string(kSaveFormatVersion));
    add_meta(text, "szenario", replay.scenario);
    add_meta(text, "seed", std::to_string(replay.seed));
    add_meta(text, "startpruefsumme", hex16(replay.start_checksum));
    add_meta(text, "endtag", std::to_string(replay.end_day));
    add_meta(text, "endpruefsumme", hex16(replay.end_checksum));
    add_meta(text, "befehle", std::to_string(replay.commands.size()));
    add_meta(text, "kontrollpunkte", std::to_string(replay.checkpoints.size()));
    add_content_meta(text, replay.content);
    return write_container(kReplayMagic, text, payload.data());
}

std::optional<Replay> read_replay(std::span<const std::uint8_t> bytes, std::string* error) {
    const auto container = read_container(bytes, true, error);
    if (!container) {
        return std::nullopt;
    }
    if (container->version != kSaveFormatVersion) {
        fail(error, "Protokoll im Format " + std::to_string(container->version) + " wird nicht unterstützt");
        return std::nullopt;
    }
    MetaReader m(container->meta);
    Replay r;
    r.game_version = m.text("spiel");
    r.scenario = m.text("szenario");
    r.seed = m.u64("seed");
    r.start_checksum = m.hex("startpruefsumme");
    r.end_day = m.i64("endtag");
    r.end_checksum = m.hex("endpruefsumme");
    const std::uint64_t command_count = m.u64("befehle");
    const std::uint64_t checkpoint_count = m.u64("kontrollpunkte");
    r.content = m.content(container->version);
    if (!m.ok()) {
        fail(error, m.error());
        return std::nullopt;
    }
    const auto payload = unpack(*container, error);
    if (!payload) {
        return std::nullopt;
    }
    ByteReader in(*payload);
    ByteReader commands(in.section("BEFE"));
    r.commands = read_commands(commands);
    ByteReader checkpoints(in.section("KONT"));
    const std::uint64_t count = checkpoints.u64();
    if (count > checkpoints.remaining() / 16) {
        checkpoints.fail("Kontrollpunkte beschädigt");
    } else {
        r.checkpoints.resize(static_cast<std::size_t>(count));
        for (Checkpoint& c : r.checkpoints) {
            c.day = checkpoints.i64();
            c.checksum = checkpoints.u64();
        }
    }
    for (const ByteReader* reader : {&in, &commands, &checkpoints}) {
        if (reader->failed() || reader->remaining() != 0) {
            fail(error, "Protokoll beschädigt" + (reader->failed() ? ": " + reader->error() : std::string()));
            return std::nullopt;
        }
    }
    if (r.commands.size() != command_count || r.checkpoints.size() != checkpoint_count) {
        fail(error, "Protokoll beschädigt: Metadaten passen nicht zu den Nutzdaten");
        return std::nullopt;
    }
    return r;
}

std::optional<Replay> replay_from_save(std::span<const std::uint8_t> bytes, std::string* error) {
    const auto container = read_container(bytes, false, error);
    if (!container) {
        return std::nullopt;
    }
    const auto info = info_from(*container, error);
    if (!info) {
        return std::nullopt;
    }
    if (info->format_version != kSaveFormatVersion) {
        fail(error,
             "Spielstand im Format " + std::to_string(info->format_version) + " wird nicht unterstützt");
        return std::nullopt;
    }
    const auto payload = unpack(*container, error);
    if (!payload) {
        return std::nullopt;
    }
    ByteReader in(*payload);
    in.section("ZUST"); // Zustand überspringen
    ByteReader protocol(in.section("PROT"));
    Replay r;
    r.game_version = info->game_version;
    r.scenario = info->scenario;
    r.seed = info->seed;
    r.content = info->content;
    r.start_checksum = protocol.u64();
    r.end_day = info->days;
    r.end_checksum = info->checksum;
    r.commands = read_commands(protocol);
    if (in.failed() || protocol.failed() || protocol.remaining() != 0) {
        fail(error, "Spielstand beschädigt: " + (in.failed()         ? in.error()
                                                 : protocol.failed() ? protocol.error()
                                                                     : "überzählige Daten"));
        return std::nullopt;
    }
    return r;
}

ReplayResult run_replay(const Replay& replay, World start, unsigned threads) {
    ReplayResult result;
    const ContentFingerprint actual = content_fingerprint(start);
    if (actual != replay.content) {
        result.error = "Protokoll " + content_mismatch(replay.content, actual);
        return result;
    }
    if (start.scenario.id != replay.scenario) {
        result.error =
            "Protokoll gehört zum Szenario " + replay.scenario + ", geladen ist " + start.scenario.id;
        return result;
    }
    Simulation sim(std::move(start), SimConfig{replay.seed, threads});
    if (sim.checksum() != replay.start_checksum) {
        result.error = "Startzustand weicht ab (Prüfsumme " + hex16(sim.checksum()) + " statt " +
                       hex16(replay.start_checksum) + ")";
        return result;
    }

    const auto mismatch = [&](std::int64_t day, std::uint64_t expected) {
        result.mismatch_day = day;
        result.expected = expected;
        result.actual = sim.checksum();
        result.error = "Prüfsumme an Tag " + std::to_string(day) + " weicht ab: " + hex16(result.actual) +
                       " statt " + hex16(expected);
    };
    std::size_t next_command = 0;
    std::size_t next_checkpoint = 0;
    const auto submit_due = [&] {
        while (next_command < replay.commands.size() &&
               replay.commands[next_command].day == sim.days_elapsed()) {
            sim.submit(replay.commands[next_command++].command);
        }
    };
    const auto check_due = [&] {
        while (next_checkpoint < replay.checkpoints.size() &&
               replay.checkpoints[next_checkpoint].day == sim.days_elapsed()) {
            const std::uint64_t expected = replay.checkpoints[next_checkpoint++].checksum;
            ++result.checkpoints;
            if (sim.checksum() != expected) {
                mismatch(sim.days_elapsed(), expected);
                return false;
            }
        }
        return true;
    };

    if (!check_due()) {
        return result;
    }
    while (sim.days_elapsed() < replay.end_day) {
        submit_due();
        sim.step_day();
        if (!check_due()) {
            return result;
        }
    }
    submit_due(); // wartende Befehle des gespeicherten Zustands
    if (next_command != replay.commands.size() || next_checkpoint != replay.checkpoints.size()) {
        result.error = "Protokoll beschädigt: Befehle oder Kontrollpunkte nach dem Endtag";
        return result;
    }
    ++result.checkpoints;
    if (sim.checksum() != replay.end_checksum) {
        mismatch(sim.days_elapsed(), replay.end_checksum);
        return result;
    }
    result.ok = true;
    return result;
}

// ─── Dateien ────────────────────────────────────────────────────────────────

FileKind file_kind(std::span<const std::uint8_t> bytes) {
    if (bytes.size() >= 8 && std::equal(kSaveMagic.begin(), kSaveMagic.end(), bytes.begin())) {
        return FileKind::Save;
    }
    if (bytes.size() >= 8 && std::equal(kReplayMagic.begin(), kReplayMagic.end(), bytes.begin())) {
        return FileKind::Replay;
    }
    return FileKind::Unknown;
}

bool write_file(const std::filesystem::path& path, std::span<const std::uint8_t> bytes, std::string* error) {
    // Erst in eine Nebendatei schreiben, dann umbenennen: Ein Absturz beim Speichern lässt den alten
    // Spielstand heil.
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return fail(error, "kann " + temp.string() + " nicht anlegen");
        }
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) {
            out.close();
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            return fail(error, "kann " + temp.string() + " nicht schreiben");
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return fail(error, "kann " + path.string() + " nicht schreiben: " + ec.message());
    }
    return true;
}

std::optional<std::vector<std::uint8_t>> read_file(const std::filesystem::path& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        fail(error, "kann " + path.string() + " nicht öffnen");
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) {
        fail(error, "kann " + path.string() + " nicht lesen");
        return std::nullopt;
    }
    return bytes;
}

std::string hex16(std::uint64_t value) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string s(16, '0');
    for (int i = 15; i >= 0; --i) {
        s[static_cast<std::size_t>(i)] = kDigits[value & 0xF];
        value >>= 4;
    }
    return s;
}

} // namespace uhrwerk
