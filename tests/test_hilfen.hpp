#pragma once

// Gemeinsame Hilfen für die Tests: temporäre Verzeichnisse, Kopie der
// Test-Fixture (tests/daten/) und Dateioperationen.

#include "uhrwerk/date.hpp"
#include "uhrwerk/fixed.hpp"
#include "uhrwerk/loader.hpp"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <string_view>

// Lesbare Werte in doctest-Meldungen statt "{?}".
namespace doctest {
template <> struct StringMaker<uhrwerk::Fixed> {
    static String convert(const uhrwerk::Fixed& value) {
        return (value.to_string(6) + " (roh " + std::to_string(value.raw()) + ")").c_str();
    }
};
template <> struct StringMaker<uhrwerk::Date> {
    static String convert(const uhrwerk::Date& value) { return uhrwerk::format_script_date(value).c_str(); }
};
} // namespace doctest

namespace testhilfen {

inline std::filesystem::path fixture_dir() {
    return std::filesystem::path(UHRWERK_TEST_DATA_DIR);
}

/// Temporäres Verzeichnis, das im Destruktor samt Inhalt gelöscht wird.
/// Mit with_fixture = true enthält es eine vollständige Kopie von tests/daten/.
class TempDir {
public:
    explicit TempDir(bool with_fixture = false) {
        static int counter = 0;
        std::random_device device;
        path_ = std::filesystem::temp_directory_path() /
                ("uhrwerk_test_" + std::to_string(device()) + "_" + std::to_string(++counter));
        std::filesystem::create_directories(path_);
        if (with_fixture) {
            std::filesystem::copy(fixture_dir(), path_, std::filesystem::copy_options::recursive);
        }
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return path_; }
    std::filesystem::path operator/(std::string_view relative) const {
        return path_ / std::filesystem::path(relative);
    }

private:
    std::filesystem::path path_;
};

inline std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "Datei nicht lesbar: " << path.string());
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

inline void write_file(const std::filesystem::path& path, std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    REQUIRE_MESSAGE(out.good(), "Datei nicht schreibbar: " << path.string());
    out << content;
}

/// Ersetzt genau das erste Vorkommen von `from` durch `to`; der Test schlägt fehl,
/// wenn `from` nicht vorkommt (schützt vor stillschweigend wirkungslosen Tests).
inline void replace_in_file(const std::filesystem::path& path, std::string_view from, std::string_view to) {
    std::string text = read_file(path);
    const std::size_t pos = text.find(from);
    REQUIRE_MESSAGE(pos != std::string::npos, "Text nicht gefunden in " << path.string() << ": " << from);
    text.replace(pos, from.size(), to);
    write_file(path, text);
}

/// Alle Probleme eines Ladevorgangs, zeilenweise (für Fehlermeldungen der Tests).
inline std::string issues_text(const uhrwerk::LoadResult& result) {
    std::string text;
    for (const uhrwerk::LoadIssue& issue : result.issues) {
        text += issue.to_string() + "\n";
    }
    return text;
}

/// Gibt es ein Problem dieser Art, dessen Nachricht `needle` enthält?
inline bool has_issue(const uhrwerk::LoadResult& result, std::string_view needle, bool fatal = true) {
    for (const uhrwerk::LoadIssue& issue : result.issues) {
        if (issue.fatal == fatal && issue.message.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

inline const uhrwerk::LoadIssue* find_issue(const uhrwerk::LoadResult& result, std::string_view needle) {
    for (const uhrwerk::LoadIssue& issue : result.issues) {
        if (issue.message.find(needle) != std::string::npos) {
            return &issue;
        }
    }
    return nullptr;
}

/// Lädt die unveränderte Fixture; der Test bricht ab, wenn das nicht klappt.
inline uhrwerk::World load_fixture(std::string_view scenario = uhrwerk::kDefaultScenario) {
    uhrwerk::LoadResult result = uhrwerk::load_world(fixture_dir(), scenario);
    INFO(issues_text(result));
    REQUIRE(result.ok());
    return std::move(*result.world);
}

} // namespace testhilfen
