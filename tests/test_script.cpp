// Tests für den Parser des Chronik-Skripts.

#include "test_hilfen.hpp"
#include "uhrwerk/script.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <string_view>
#include <vector>

using uhrwerk::Fixed;
using uhrwerk::ParseResult;
using uhrwerk::ScriptNode;

namespace {

std::string errors_text(const ParseResult& r) {
    std::string text;
    for (const auto& e : r.errors) {
        text += e.to_string() + "\n";
    }
    return text;
}

/// Parst und verlangt fehlerfreies Ergebnis.
ParseResult parse_ok(std::string_view text) {
    ParseResult r = uhrwerk::parse_script(text, "test.chron");
    INFO(errors_text(r));
    REQUIRE(r.ok());
    return r;
}

} // namespace

TEST_CASE("Parser: leere Eingabe und nur Kommentare") {
    for (const char* text : {"", "   \n\t\r\n", "# nur ein Kommentar", "# a\n# b\n"}) {
        const ParseResult r = parse_ok(text);
        CHECK(r.root.is_block);
        CHECK(r.root.children.empty());
    }
}

TEST_CASE("Parser: Zuweisung mit Zeichenkette") {
    const ParseResult r = parse_ok("name = \"Berlin\"");
    REQUIRE(r.root.children.size() == 1);
    const ScriptNode& n = r.root.children[0];
    CHECK(n.key == "name");
    CHECK(n.op == "=");
    CHECK(n.value == "Berlin");
    CHECK(n.quoted);
    CHECK_FALSE(n.is_block);
    CHECK(n.children.empty());
    CHECK(n.line == 1);
    CHECK(n.column == 1);
}

TEST_CASE("Parser: alle Operatoren") {
    const ParseResult r = parse_ok("a = 1\nb < 2\nc > 3\nd <= 4\ne >= 5\nf != 6\ng>=7 h<8 i!=9 j=k");
    const char* const expected[][3] = {{"a", "=", "1"},  {"b", "<", "2"},  {"c", ">", "3"},  {"d", "<=", "4"},
                                       {"e", ">=", "5"}, {"f", "!=", "6"}, {"g", ">=", "7"}, {"h", "<", "8"},
                                       {"i", "!=", "9"}, {"j", "=", "k"}};
    REQUIRE(r.root.children.size() == 10);
    for (std::size_t i = 0; i < 10; ++i) {
        const ScriptNode& n = r.root.children[i];
        CHECK(n.key == expected[i][0]);
        CHECK(n.op == expected[i][1]);
        CHECK(n.value == expected[i][2]);
        CHECK_FALSE(n.quoted);
        CHECK_FALSE(n.is_block);
    }
    CHECK(r.root.children[4].line == 5);
    CHECK(r.root.children[7].line == 7);
    CHECK(r.root.children[7].column == 6);
}

TEST_CASE("Parser: Blockformen") {
    const ParseResult r = parse_ok("lage = { 13.4 52.5 }\n"
                                   "regeln { a = 1 }\n"
                                   "ort berlin_coelln { name = x }\n"
                                   "{ anonym }\n");
    REQUIRE(r.root.children.size() == 4);

    const ScriptNode& lage = r.root.children[0];
    CHECK(lage.key == "lage");
    CHECK(lage.op == "=");
    CHECK(lage.is_block);
    CHECK(lage.value.empty());
    REQUIRE(lage.children.size() == 2);
    CHECK(lage.children[0].key.empty());
    CHECK(lage.children[0].op.empty());
    CHECK(lage.children[0].value == "13.4");
    CHECK(lage.bare_values() == std::vector<std::string_view>{"13.4", "52.5"});

    const ScriptNode& regeln = r.root.children[1];
    CHECK(regeln.key == "regeln");
    CHECK(regeln.op == "=");
    CHECK(regeln.is_block);
    CHECK(regeln.value.empty());
    REQUIRE(regeln.children.size() == 1);
    CHECK(regeln.children[0].key == "a");

    const ScriptNode& ort = r.root.children[2];
    CHECK(ort.key == "ort");
    CHECK(ort.op == "=");
    CHECK(ort.value == "berlin_coelln");
    CHECK(ort.is_block);
    CHECK(ort.get_string("name") == "x");
    CHECK(ort.line == 3);

    const ScriptNode& anonym = r.root.children[3];
    CHECK(anonym.key.empty());
    CHECK(anonym.is_block);
    CHECK(anonym.line == 4);
    CHECK(anonym.column == 1);
    REQUIRE(anonym.children.size() == 1);
    CHECK(anonym.children[0].value == "anonym");
}

TEST_CASE("Parser: Vorausschau zwischen bare, Block und typisiertem Block") {
    const ParseResult r = parse_ok("liste = { a b c }\n"
                                   "x y { }\n"
                                   "p { } q\n"
                                   "m n o = 1\n"
                                   "{ { 1 2 } { 3 } }\n");
    const auto& c = r.root.children;
    REQUIRE(c.size() == 8);
    CHECK(c[0].bare_values() == std::vector<std::string_view>{"a", "b", "c"});
    CHECK(c[1].key == "x");
    CHECK(c[1].value == "y");
    CHECK(c[1].is_block);
    CHECK(c[2].key == "p");
    CHECK(c[2].is_block);
    CHECK(c[3].key.empty());
    CHECK(c[3].value == "q");
    CHECK(c[4].value == "m"); // bare
    CHECK(c[5].value == "n"); // bare
    CHECK(c[6].key == "o");   // o = 1
    CHECK(c[6].value == "1");
    CHECK(c[7].is_block);
    REQUIRE(c[7].children.size() == 2);
    CHECK(c[7].children[0].bare_values().size() == 2);
    CHECK(c[7].children[1].bare_values().size() == 1);
}

TEST_CASE("Parser: Verschachtelung und Positionen") {
    const ParseResult r = parse_ok("staat BRA {\n"
                                   "    farbe = { 58 95 143 }\n"
                                   "    konfession = { lutherisch = 90 reformiert = 10 }\n"
                                   "}\n");
    REQUIRE(r.root.children.size() == 1);
    const ScriptNode& staat = r.root.children[0];
    REQUIRE(staat.children.size() == 2);
    CHECK(staat.children[0].line == 2);
    CHECK(staat.children[0].column == 5);
    CHECK(staat.children[0].bare_values().size() == 3);
    const ScriptNode& konf = staat.children[1];
    CHECK(konf.line == 3);
    CHECK(konf.get_int("lutherisch") == 90);
    CHECK(konf.get_int("reformiert") == 10);
    CHECK(konf.children[1].column == 36);
}

TEST_CASE("Parser: Kommentare") {
    const ParseResult r = parse_ok("# Kopf\n"
                                   "a = 1 # Kommentar am Zeilenende\n"
                                   "b = \"Text # kein Kommentar\"\n"
                                   "c = { x # Kommentar im Block\n"
                                   "  y }\n"
                                   "d = 2#direkt\n");
    const auto& c = r.root.children;
    REQUIRE(c.size() == 4);
    CHECK(c[0].value == "1");
    CHECK(c[1].value == "Text # kein Kommentar");
    CHECK(c[2].bare_values() == std::vector<std::string_view>{"x", "y"});
    CHECK(c[3].value == "2");
}

TEST_CASE("Parser: Zeichenketten mit Escapes") {
    const ParseResult r =
        parse_ok(R"(a = "Er sagte \"Ja\"" b = "C:\\Pfad\\" c = "" d = "mit  Leer { } = # zeichen")");
    const auto& c = r.root.children;
    REQUIRE(c.size() == 4);
    CHECK(c[0].value == "Er sagte \"Ja\"");
    CHECK(c[1].value == "C:\\Pfad\\");
    CHECK(c[2].value.empty());
    CHECK(c[2].quoted);
    CHECK(c[3].value == "mit  Leer { } = # zeichen");
}

TEST_CASE("Parser: Zeichenkette als bare-Eintrag") {
    const ParseResult r = parse_ok("namen = { \"Berlin\" \"Cölln\" }");
    const ScriptNode& n = r.root.children.at(0);
    REQUIRE(n.children.size() == 2);
    CHECK(n.children[0].quoted);
    CHECK(n.bare_values() == std::vector<std::string_view>{"Berlin", "Cölln"});
}

TEST_CASE("Parser: UTF-8 in Wörtern und Zeichenketten, Spalten in Bytes") {
    const ParseResult r = parse_ok("name = Cölln\nort köln_süd { name = \"Köln-Süd\" }\näb = 1 c");
    const auto& c = r.root.children;
    REQUIRE(c.size() == 4);
    CHECK(c[0].value == "Cölln");
    CHECK(c[1].value == "köln_süd");
    CHECK(c[1].get_string("name") == "Köln-Süd");
    CHECK(c[2].key == "äb");
    CHECK(c[3].value == "c");
    CHECK(c[3].line == 3);
    CHECK(c[3].column == 9); // "äb" sind 3 Bytes
}

TEST_CASE("Parser: Wörter mit Sonderzeichen") {
    const ParseResult r = parse_ok("a = -0.25 b = 1648.10.24 c = x.y-z/w:1 d = 12000");
    const auto& c = r.root.children;
    REQUIRE(c.size() == 4);
    CHECK(c[0].value == "-0.25");
    CHECK(c[1].value == "1648.10.24");
    CHECK(c[2].value == "x.y-z/w:1");
}

TEST_CASE("Parser: BOM wird ignoriert") {
    const ParseResult r = parse_ok("\xEF\xBB\xBF"
                                   "a = 1\nb = 2");
    REQUIRE(r.root.children.size() == 2);
    CHECK(r.root.children[0].key == "a");
    CHECK(r.root.children[0].line == 1);
    CHECK(r.root.children[0].column == 1);
    CHECK(r.root.children[1].line == 2);
}

TEST_CASE("Parser: CRLF-Zeilenenden") {
    const ParseResult r = parse_ok("a = 1\r\nb = \"x\"\r\nc {\r\n  d\r\n}\r\n");
    const auto& c = r.root.children;
    REQUIRE(c.size() == 3);
    CHECK(c[1].value == "x");
    CHECK(c[1].line == 2);
    CHECK(c[2].line == 3);
    CHECK(c[2].children.at(0).line == 4);
    CHECK(c[2].children.at(0).column == 3);
}

TEST_CASE("Parser: Zugriffsfunktionen von ScriptNode") {
    const ParseResult r =
        parse_ok("x = 5\n"
                 "x = 6\n"
                 "x >= 7\n"
                 "blk = { 1 2 k = v }\n"
                 "f = 0.25 i = -12 big = 9223372036854775807 neg = -9223372036854775808\n"
                 "over = 9223372036854775808 frac = 1.0 leer = \"\"\n"
                 "d = 1648.10.24 bad_d = 1700.02.29\n"
                 "b1 = ja b2 = yes b3 = wahr b4 = nein b5 = no b6 = falsch b7 = vielleicht b8 = Ja\n"
                 "lst = { 1 2 3 }\n");
    const ScriptNode& root = r.root;

    CHECK(root.find("x") == &root.children[0]);
    CHECK(root.find("fehlt") == nullptr);
    CHECK(root.find_all("x").size() == 3);
    CHECK(root.find_all("fehlt").empty());

    CHECK(root.get_string("x") == "5");  // erster passender Eintrag
    CHECK_FALSE(root.get_string("blk")); // Block ist kein Skalar
    CHECK_FALSE(root.get_string("fehlt"));
    CHECK(root.get_string("leer") == "");

    CHECK(root.get_fixed("f") == Fixed::from_ratio(1, 4));
    CHECK(root.get_fixed("i") == Fixed::from_int(-12));
    CHECK_FALSE(root.get_fixed("d"));

    CHECK(root.get_int("i") == -12);
    CHECK(root.get_int("big") == INT64_MAX);
    CHECK(root.get_int("neg") == INT64_MIN);
    CHECK_FALSE(root.get_int("over"));
    CHECK_FALSE(root.get_int("frac"));
    CHECK_FALSE(root.get_int("leer"));
    CHECK_FALSE(root.get_int("blk"));

    CHECK(root.get_date("d") == uhrwerk::Date{1648, 10, 24});
    CHECK_FALSE(root.get_date("bad_d"));
    CHECK_FALSE(root.get_date("f"));

    CHECK(root.get_bool("b1") == true);
    CHECK(root.get_bool("b2") == true);
    CHECK(root.get_bool("b3") == true);
    CHECK(root.get_bool("b4") == false);
    CHECK(root.get_bool("b5") == false);
    CHECK(root.get_bool("b6") == false);
    CHECK_FALSE(root.get_bool("b7"));
    CHECK_FALSE(root.get_bool("b8")); // Groß-/Kleinschreibung zählt

    // bare_values ignoriert Einträge mit Schlüssel und Blöcke.
    CHECK(root.find("blk")->bare_values() == std::vector<std::string_view>{"1", "2"});
    CHECK(root.find("lst")->bare_values().size() == 3);
}

// ─── Fehler ─────────────────────────────────────────────────────────────────

TEST_CASE("Parser: nicht geschlossener Block") {
    const ParseResult r = uhrwerk::parse_script("a = 1\nb = {\n  c = 2\n", "orte.chron");
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].message == "nicht geschlossener Block (geöffnet in Zeile 2)");
    CHECK(r.errors[0].file == "orte.chron");
    // Inhalt bleibt erhalten.
    REQUIRE(r.root.children.size() == 2);
    CHECK(r.root.children[1].get_int("c") == 2);
}

TEST_CASE("Parser: unerwartetes '}'") {
    const ParseResult r = uhrwerk::parse_script("a = 1\n}\nb = 2\n", "x.chron");
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].message == "unerwartetes '}'");
    CHECK(r.errors[0].line == 2);
    CHECK(r.errors[0].column == 1);
    CHECK(r.errors[0].to_string() == "x.chron:2:1: unerwartetes '}'");
    CHECK(r.root.children.size() == 2);
}

TEST_CASE("Parser: Wert nach '=' fehlt") {
    SUBCASE("am Dateiende") {
        const ParseResult r = uhrwerk::parse_script("a =", "x.chron");
        REQUIRE(r.errors.size() == 1);
        CHECK(r.errors[0].message == "Wert nach '=' fehlt");
        CHECK(r.errors[0].line == 1);
        CHECK(r.errors[0].column == 3);
        CHECK(r.root.children.empty());
    }
    SUBCASE("vor schließender Klammer") {
        const ParseResult r = uhrwerk::parse_script("blk {\n  a >=\n}\nb = 2", "x.chron");
        REQUIRE(r.errors.size() == 1);
        CHECK(r.errors[0].message == "Wert nach '>=' fehlt");
        CHECK(r.errors[0].line == 2);
        REQUIRE(r.root.children.size() == 2);
        CHECK(r.root.children[0].children.empty());
    }
    SUBCASE("nächste Zeile beginnt eine neue Zuweisung") {
        const ParseResult r = uhrwerk::parse_script("name =\nbesitzer = BRA\n", "x.chron");
        REQUIRE(r.errors.size() == 1);
        CHECK(r.errors[0].line == 1);
        CHECK(r.errors[0].message == "Wert nach '=' fehlt");
        REQUIRE(r.root.children.size() == 1);
        CHECK(r.root.children[0].key == "besitzer");
    }
    SUBCASE("doppelter Operator") {
        const ParseResult r = uhrwerk::parse_script("a = = 5\nb = 6", "x.chron");
        REQUIRE(r.errors.size() == 1);
        CHECK(r.errors[0].line == 1);
        REQUIRE(r.root.children.size() == 1);
        CHECK(r.root.children[0].key == "b");
    }
    SUBCASE("Wert in der nächsten Zeile ist erlaubt") {
        const ParseResult r = parse_ok("lage =\n  { 1 2 }\nname =\n  \"x\"");
        CHECK(r.root.children.size() == 2);
    }
}

TEST_CASE("Parser: Fehler in Zeichenketten") {
    SUBCASE("nicht beendet") {
        const ParseResult r = uhrwerk::parse_script("a = \"offen\nb = 2", "x.chron");
        REQUIRE(r.errors.size() == 1);
        CHECK(r.errors[0].message == "nicht beendete Zeichenkette");
        CHECK(r.errors[0].line == 1);
        CHECK(r.errors[0].column == 5);
        REQUIRE(r.root.children.size() == 2); // Wiederaufsetzen in der nächsten Zeile
        CHECK(r.root.get_int("b") == 2);
    }
    SUBCASE("ungültige Escape-Sequenz") {
        const ParseResult r = uhrwerk::parse_script(R"(a = "x\ny")", "x.chron");
        REQUIRE(r.errors.size() == 1);
        CHECK(r.errors[0].message.find("Escape") != std::string::npos);
        CHECK(r.errors[0].column == 7);
    }
    SUBCASE("Zeichenkette als Schlüssel") {
        const ParseResult r = uhrwerk::parse_script("\"name\" = 5\nb = 1", "x.chron");
        REQUIRE(r.errors.size() == 1);
        CHECK(r.errors[0].message == "eine Zeichenkette kann kein Schlüssel sein");
        REQUIRE(r.root.children.size() == 1);
        CHECK(r.root.children[0].key == "b");
    }
}

TEST_CASE("Parser: unerwartete Operatoren und einzelnes '!'") {
    const ParseResult r = uhrwerk::parse_script("= 5\na ! b\nc = 1", "x.chron");
    REQUIRE(r.errors.size() == 2);
    CHECK(r.errors[0].line == 1);
    CHECK(r.errors[0].message == "unerwarteter Operator '='");
    CHECK(r.errors[1].line == 2);
    CHECK(r.errors[1].column == 3);
    CHECK(r.errors[1].message.find("'!'") != std::string::npos);
    CHECK(r.root.find("c") != nullptr);
}

TEST_CASE("Parser: ungültiges UTF-8 wird gemeldet") {
    const ParseResult r = uhrwerk::parse_script("name = K\xF6ln\nb = \"\xC3\x28\"\nc = ok", "x.chron");
    REQUIRE(r.errors.size() == 2);
    CHECK(r.errors[0].line == 1);
    CHECK(r.errors[0].column == 9);
    CHECK(r.errors[0].message.find("UTF-8") != std::string::npos);
    CHECK(r.errors[1].line == 2);
    CHECK(r.root.find("c") != nullptr);
}

TEST_CASE("Parser: mehrere Fehler durch Wiederaufsetzen, nach Position sortiert") {
    const ParseResult r = uhrwerk::parse_script("ort a {\n"         // 1
                                                "  name = \"A\"\n"  // 2
                                                "  flaeche =\n"     // 3  Wert fehlt
                                                "}\n"               // 4
                                                "}\n"               // 5  unerwartetes '}'
                                                "ort b {\n"         // 6
                                                "  name = \"B\n"    // 7  nicht beendet
                                                "  stadt = ja\n"    // 8
                                                "  = 3\n"           // 9  unerwarteter Operator
                                                "}\n"               // 10
                                                "ort c {\n"         // 11 nicht geschlossen
                                                "  name = \"C\"\n", // 12
                                                "orte.chron");
    INFO(errors_text(r));
    REQUIRE(r.errors.size() == 5);
    CHECK(r.errors[0].line == 3);
    CHECK(r.errors[0].message == "Wert nach '=' fehlt");
    CHECK(r.errors[1].line == 5);
    CHECK(r.errors[1].message == "unerwartetes '}'");
    CHECK(r.errors[2].line == 7);
    CHECK(r.errors[2].message == "nicht beendete Zeichenkette");
    CHECK(r.errors[3].line == 9);
    CHECK(r.errors[3].message == "unerwarteter Operator '='");
    CHECK(r.errors[4].message == "nicht geschlossener Block (geöffnet in Zeile 11)");
    CHECK(r.errors[4].to_string().rfind("orte.chron:13:1: ", 0) == 0);

    // Alle drei Orte sind trotzdem im Baum.
    REQUIRE(r.root.children.size() == 3);
    CHECK(r.root.children[0].get_string("name") == "A");
    CHECK(r.root.children[1].get_bool("stadt") == true);
    CHECK(r.root.children[2].get_string("name") == "C");
}

TEST_CASE("Parser: zu tiefe Verschachtelung bricht nicht ab") {
    const std::string text = std::string(5000, '{') + std::string(5000, '}') + "\na = 1";
    const ParseResult r = uhrwerk::parse_script(text, "tief.chron");
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].message.find("verschachtelt") != std::string::npos);
    CHECK(r.root.find("a") != nullptr);
}

TEST_CASE("Parser: ParseError::to_string") {
    CHECK(uhrwerk::ParseError{"a.chron", 12, 5, "Nachricht"}.to_string() == "a.chron:12:5: Nachricht");
    CHECK(uhrwerk::ParseError{"a.chron", 0, 0, "Nachricht"}.to_string() == "a.chron: Nachricht");
}

TEST_CASE("Parser: parse_script_file") {
    testhilfen::TempDir dir;
    const auto path = dir / "mit_bom.chron";
    testhilfen::write_file(path, "\xEF\xBB\xBF# Kommentar\nort köln { name = \"Köln\" }\n");
    const ParseResult r = uhrwerk::parse_script_file(path);
    INFO(errors_text(r));
    REQUIRE(r.ok());
    REQUIRE(r.root.children.size() == 1);
    CHECK(r.root.children[0].line == 2);
    CHECK(r.root.children[0].get_string("name") == "Köln");

    testhilfen::write_file(dir / "fehler.chron", "a = {\n");
    const ParseResult bad = uhrwerk::parse_script_file(dir / "fehler.chron");
    REQUIRE(bad.errors.size() == 1);
    CHECK(bad.errors[0].file.find("fehler.chron") != std::string::npos);

    const ParseResult missing = uhrwerk::parse_script_file(dir / "gibt_es_nicht.chron");
    REQUIRE(missing.errors.size() == 1);
    CHECK(missing.errors[0].message == "Datei kann nicht gelesen werden");
}
