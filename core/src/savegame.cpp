// Spielstände und Wiederholungen (seit M6, docs/technik/speicherstaende.md).

#include "uhrwerk/savegame.hpp"

#include "state_access.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <system_error>
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
    /// Format 2 (M8) die ersten 14 (ohne diplomatie), die übrigen bleiben 0. So zeigt auch ein alter
    /// Spielstand seine Metadaten an.
    ContentFingerprint content(std::uint32_t version) {
        const std::size_t count = version == 1 ? 13 : version == 2 ? 14 : kContentParts.size();
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
    return {};
}

} // namespace

// ─── Inhaltsfingerabdruck ───────────────────────────────────────────────────

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
    // Diplomatie (seit M9): Zollunionen und die Mitgliedschaft der Staaten.
    {
        Fnv h;
        h.u64(w.customs_unions.size());
        for (const CustomsUnion& u : w.customs_unions) {
            h.text(u.id);
            h.text(u.name); // steht in der Chronik bei Beitritt und Austritt
            h.i64(u.leader);
            h.fixed(u.tariff);
        }
        h.u64(w.states.size());
        for (const State& s : w.states) {
            h.i64(s.customs_union);
        }
        p[14] = h.value();
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
