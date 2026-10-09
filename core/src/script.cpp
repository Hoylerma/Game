#include "uhrwerk/script.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uhrwerk {

// ─── ScriptNode ─────────────────────────────────────────────────────────────

const ScriptNode* ScriptNode::find(std::string_view child_key) const {
    for (const ScriptNode& child : children) {
        if (child.key == child_key) {
            return &child;
        }
    }
    return nullptr;
}

std::vector<const ScriptNode*> ScriptNode::find_all(std::string_view child_key) const {
    std::vector<const ScriptNode*> result;
    for (const ScriptNode& child : children) {
        if (child.key == child_key) {
            result.push_back(&child);
        }
    }
    return result;
}

std::optional<std::string_view> ScriptNode::get_string(std::string_view child_key) const {
    for (const ScriptNode& child : children) {
        if (child.key == child_key && !child.is_block && child.op == "=") {
            return std::string_view(child.value);
        }
    }
    return std::nullopt;
}

std::optional<Fixed> ScriptNode::get_fixed(std::string_view child_key) const {
    const auto text = get_string(child_key);
    if (!text) {
        return std::nullopt;
    }
    return Fixed::parse(*text);
}

std::optional<std::int64_t> ScriptNode::get_int(std::string_view child_key) const {
    const auto text = get_string(child_key);
    if (!text || text->empty()) {
        return std::nullopt;
    }
    std::size_t pos = 0;
    const bool negative = (*text)[0] == '-';
    if (negative) {
        pos = 1;
    }
    if (pos == text->size()) {
        return std::nullopt;
    }
    // Betrag bis 2^63 zulassen, damit auch INT64_MIN lesbar ist.
    constexpr std::uint64_t kLimit = std::uint64_t{1} << 63;
    std::uint64_t magnitude = 0;
    for (; pos < text->size(); ++pos) {
        const char c = (*text)[pos];
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (magnitude > (kLimit - digit) / 10) {
            return std::nullopt;
        }
        magnitude = magnitude * 10 + digit;
    }
    if (!negative && magnitude == kLimit) {
        return std::nullopt;
    }
    if (negative) {
        return static_cast<std::int64_t>(std::uint64_t{0} - magnitude);
    }
    return static_cast<std::int64_t>(magnitude);
}

std::optional<Date> ScriptNode::get_date(std::string_view child_key) const {
    const auto text = get_string(child_key);
    if (!text) {
        return std::nullopt;
    }
    return parse_date(*text);
}

std::optional<bool> ScriptNode::get_bool(std::string_view child_key) const {
    const auto text = get_string(child_key);
    if (!text) {
        return std::nullopt;
    }
    if (*text == "ja" || *text == "yes" || *text == "wahr") {
        return true;
    }
    if (*text == "nein" || *text == "no" || *text == "falsch") {
        return false;
    }
    return std::nullopt;
}

std::vector<std::string_view> ScriptNode::bare_values() const {
    std::vector<std::string_view> result;
    for (const ScriptNode& child : children) {
        if (child.key.empty() && !child.is_block) {
            result.emplace_back(child.value);
        }
    }
    return result;
}

std::string ParseError::to_string() const {
    if (line <= 0) {
        return file + ": " + message;
    }
    if (column <= 0) {
        return file + ':' + std::to_string(line) + ": " + message;
    }
    return file + ':' + std::to_string(line) + ':' + std::to_string(column) + ": " + message;
}

// ─── Lexer ──────────────────────────────────────────────────────────────────

namespace {

enum class TokenType : std::uint8_t { Word, String, LBrace, RBrace, Op, End };

struct Token {
    TokenType type = TokenType::End;
    std::string text;
    int line = 0;
    int column = 0;
};

constexpr int kMaxDepth = 256;

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool is_word_char(char c) {
    switch (c) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
    case '{':
    case '}':
    case '=':
    case '<':
    case '>':
    case '!':
    case '#':
    case '"':
        return false;
    default:
        return true;
    }
}

// Prüft eine UTF-8-Folge; gibt die Länge der ersten ungültigen Stelle zurück
// (Index ab Anfang) oder npos, wenn alles gültig ist.
std::size_t find_invalid_utf8(std::string_view text) {
    std::size_t i = 0;
    while (i < text.size()) {
        const auto b0 = static_cast<unsigned char>(text[i]);
        if (b0 < 0x80) {
            ++i;
            continue;
        }
        std::size_t length = 0;
        std::uint32_t code = 0;
        std::uint32_t min_code = 0;
        if ((b0 & 0xE0) == 0xC0) {
            length = 2;
            code = b0 & 0x1Fu;
            min_code = 0x80;
        } else if ((b0 & 0xF0) == 0xE0) {
            length = 3;
            code = b0 & 0x0Fu;
            min_code = 0x800;
        } else if ((b0 & 0xF8) == 0xF0) {
            length = 4;
            code = b0 & 0x07u;
            min_code = 0x10000;
        } else {
            return i;
        }
        if (i + length > text.size()) {
            return i;
        }
        for (std::size_t k = 1; k < length; ++k) {
            const auto b = static_cast<unsigned char>(text[i + k]);
            if ((b & 0xC0) != 0x80) {
                return i;
            }
            code = (code << 6) | (b & 0x3Fu);
        }
        if (code < min_code || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
            return i;
        }
        i += length;
    }
    return std::string_view::npos;
}

class Lexer {
public:
    Lexer(std::string_view text, const std::string& file, std::vector<ParseError>& errors)
        : text_(text), file_(file), errors_(errors) {}

    std::vector<Token> run() {
        std::vector<Token> tokens;
        // BOM am Dateianfang überspringen; Spalten zählen danach ab 1.
        if (text_.substr(0, 3) == "\xEF\xBB\xBF") {
            pos_ = 3;
            line_start_ = 3;
        }
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == '\n') {
                ++pos_;
                ++line_;
                line_start_ = pos_;
            } else if (is_space(c)) {
                ++pos_;
            } else if (c == '#') {
                while (pos_ < text_.size() && text_[pos_] != '\n') {
                    ++pos_;
                }
            } else if (c == '{') {
                tokens.push_back(make(TokenType::LBrace, "{"));
                ++pos_;
            } else if (c == '}') {
                tokens.push_back(make(TokenType::RBrace, "}"));
                ++pos_;
            } else if (c == '=') {
                tokens.push_back(make(TokenType::Op, "="));
                ++pos_;
            } else if (c == '<' || c == '>') {
                const bool with_eq = pos_ + 1 < text_.size() && text_[pos_ + 1] == '=';
                std::string op(1, c);
                if (with_eq) {
                    op += '=';
                }
                tokens.push_back(make(TokenType::Op, std::move(op)));
                pos_ += with_eq ? 2 : 1;
            } else if (c == '!') {
                if (pos_ + 1 < text_.size() && text_[pos_ + 1] == '=') {
                    tokens.push_back(make(TokenType::Op, "!="));
                    pos_ += 2;
                } else {
                    error(line_, column(), "einzelnes '!' (erwartet '!=')");
                    ++pos_;
                }
            } else if (c == '"') {
                tokens.push_back(read_string());
            } else {
                tokens.push_back(read_word());
            }
        }
        Token end = make(TokenType::End, "");
        tokens.push_back(std::move(end));
        return tokens;
    }

private:
    int column() const { return static_cast<int>(pos_ - line_start_) + 1; }

    Token make(TokenType type, std::string text) const {
        Token t;
        t.type = type;
        t.text = std::move(text);
        t.line = line_;
        t.column = column();
        return t;
    }

    void error(int line, int col, std::string message) {
        errors_.push_back(ParseError{file_, line, col, std::move(message)});
    }

    void check_utf8(std::string_view raw, int line, int col) {
        const std::size_t bad = find_invalid_utf8(raw);
        if (bad != std::string_view::npos) {
            error(line, col + static_cast<int>(bad), "ungültige UTF-8-Kodierung (Datei als UTF-8 speichern)");
        }
    }

    Token read_word() {
        Token t = make(TokenType::Word, "");
        const std::size_t begin = pos_;
        while (pos_ < text_.size() && is_word_char(text_[pos_])) {
            ++pos_;
        }
        t.text.assign(text_.substr(begin, pos_ - begin));
        check_utf8(t.text, t.line, t.column);
        return t;
    }

    Token read_string() {
        Token t = make(TokenType::String, "");
        ++pos_; // öffnendes "
        bool closed = false;
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == '\n' || c == '\r') {
                break;
            }
            if (c == '"') {
                ++pos_;
                closed = true;
                break;
            }
            if (c == '\\') {
                const char next = pos_ + 1 < text_.size() ? text_[pos_ + 1] : '\0';
                if (next == '"' || next == '\\') {
                    t.text += next;
                    pos_ += 2;
                    continue;
                }
                if (next == '\n' || next == '\r' || pos_ + 1 >= text_.size()) {
                    t.text += c;
                    ++pos_;
                    continue; // Zeilenende beendet die Zeichenkette (Fehler unten)
                }
                error(line_, column(),
                      std::string("ungültige Escape-Sequenz '\\") + next + "' (erlaubt sind \\\" und \\\\)");
                t.text += c;
                ++pos_;
                continue;
            }
            t.text += c;
            ++pos_;
        }
        if (!closed) {
            error(t.line, t.column, "nicht beendete Zeichenkette");
        }
        check_utf8(t.text, t.line, t.column + 1);
        return t;
    }

    std::string_view text_;
    const std::string& file_;
    std::vector<ParseError>& errors_;
    std::size_t pos_ = 0;
    std::size_t line_start_ = 0;
    int line_ = 1;
};

// ─── Parser ─────────────────────────────────────────────────────────────────

class Parser {
public:
    Parser(std::vector<Token> tokens, const std::string& file, std::vector<ParseError>& errors)
        : tokens_(std::move(tokens)), file_(file), errors_(errors) {}

    ScriptNode parse_file() {
        ScriptNode root;
        root.is_block = true;
        root.line = 1;
        root.column = 1;
        parse_body(root.children, nullptr, 0);
        return root;
    }

private:
    const Token& peek(std::size_t ahead = 0) const {
        const std::size_t index = pos_ + ahead;
        return index < tokens_.size() ? tokens_[index] : tokens_.back();
    }

    const Token& advance() {
        const Token& t = peek();
        if (pos_ + 1 < tokens_.size()) {
            ++pos_;
        }
        return t;
    }

    void error(const Token& at, std::string message) {
        errors_.push_back(ParseError{file_, at.line, at.column, std::move(message)});
    }

    static std::string describe(const Token& t) {
        switch (t.type) {
        case TokenType::End:
            return "Dateiende";
        case TokenType::String:
            return "Zeichenkette";
        default:
            return "'" + t.text + "'";
        }
    }

    // Überspringt alle Tokens in derselben Zeile bis zur nächsten Klammer.
    void skip_rest_of_line(int line) {
        while (peek().type != TokenType::End && peek().type != TokenType::LBrace &&
               peek().type != TokenType::RBrace && peek().line == line) {
            advance();
        }
    }

    // Überspringt einen zu tief verschachtelten Block samt Inhalt (iterativ).
    void skip_block() {
        int depth = 1;
        while (depth > 0 && peek().type != TokenType::End) {
            const Token& t = advance();
            if (t.type == TokenType::LBrace) {
                ++depth;
            } else if (t.type == TokenType::RBrace) {
                --depth;
            }
        }
    }

    // Liest Anweisungen bis zur schließenden Klammer (open != nullptr) bzw. bis Dateiende.
    void parse_body(std::vector<ScriptNode>& out, const Token* open, int depth) {
        for (;;) {
            const Token& t = peek();
            switch (t.type) {
            case TokenType::End:
                if (open != nullptr) {
                    error(t,
                          "nicht geschlossener Block (geöffnet in Zeile " + std::to_string(open->line) + ")");
                }
                return;
            case TokenType::RBrace:
                advance();
                if (open != nullptr) {
                    return;
                }
                error(t, "unerwartetes '}'");
                break;
            case TokenType::LBrace: {
                ScriptNode node;
                node.is_block = true;
                node.line = t.line;
                node.column = t.column;
                if (parse_block(node, depth)) {
                    out.push_back(std::move(node));
                }
                break;
            }
            case TokenType::String:
                if (peek(1).type == TokenType::Op) {
                    error(t, "eine Zeichenkette kann kein Schlüssel sein");
                    advance(); // Zeichenkette
                    const Token& op = advance();
                    ScriptNode discard;
                    parse_value(discard, op, depth);
                } else {
                    ScriptNode node;
                    node.value = t.text;
                    node.quoted = true;
                    node.line = t.line;
                    node.column = t.column;
                    advance();
                    out.push_back(std::move(node));
                }
                break;
            case TokenType::Op:
                error(t, "unerwarteter Operator '" + t.text + "'");
                skip_rest_of_line(t.line);
                break;
            case TokenType::Word:
                parse_word_statement(out, depth);
                break;
            }
        }
    }

    // Erwartet '{' als aktuelles Token; liest den Block in node.children.
    // false, wenn der Block wegen zu tiefer Verschachtelung verworfen wurde.
    bool parse_block(ScriptNode& node, int depth) {
        const Token& open = advance();
        if (depth + 1 > kMaxDepth) {
            error(open, "Blöcke zu tief verschachtelt (höchstens " + std::to_string(kMaxDepth) + " Ebenen)");
            skip_block();
            return false;
        }
        node.is_block = true;
        parse_body(node.children, &open, depth + 1);
        return true;
    }

    // Liest den Wert nach einem Operator. false bei Fehler (Knoten verwerfen).
    bool parse_value(ScriptNode& node, const Token& op, int depth) {
        const Token& v = peek();
        switch (v.type) {
        case TokenType::Word:
            // "key =" am Zeilenende, gefolgt von "anderer = …" in der nächsten Zeile:
            // Das Wort gehört zur nächsten Anweisung, der Wert fehlt.
            if (v.line != op.line && peek(1).type == TokenType::Op) {
                error(op, "Wert nach '" + op.text + "' fehlt");
                return false;
            }
            node.value = v.text;
            advance();
            return true;
        case TokenType::String:
            node.value = v.text;
            node.quoted = true;
            advance();
            return true;
        case TokenType::LBrace:
            return parse_block(node, depth);
        case TokenType::Op:
            error(op, "Wert nach '" + op.text + "' fehlt (stattdessen " + describe(v) + ")");
            if (v.line == op.line) {
                skip_rest_of_line(v.line);
            }
            return false;
        case TokenType::RBrace:
        case TokenType::End:
            error(op, "Wert nach '" + op.text + "' fehlt");
            return false;
        }
        return false;
    }

    void parse_word_statement(std::vector<ScriptNode>& out, int depth) {
        const Token& key = peek();
        const Token& next = peek(1);
        ScriptNode node;
        node.line = key.line;
        node.column = key.column;

        if (next.type == TokenType::Op) {
            node.key = key.text;
            node.op = next.text;
            advance();
            const Token& op = advance();
            if (parse_value(node, op, depth)) {
                out.push_back(std::move(node));
            }
            return;
        }
        if (next.type == TokenType::LBrace) {
            // key { … }  ≙  key = { … }
            node.key = key.text;
            node.op = "=";
            advance();
            if (parse_block(node, depth)) {
                out.push_back(std::move(node));
            }
            return;
        }
        if (next.type == TokenType::Word && peek(2).type == TokenType::LBrace) {
            // typ name { … }
            node.key = key.text;
            node.op = "=";
            node.value = next.text;
            advance();
            advance();
            if (parse_block(node, depth)) {
                out.push_back(std::move(node));
            }
            return;
        }
        // bare-Eintrag
        node.value = key.text;
        advance();
        out.push_back(std::move(node));
    }

    std::vector<Token> tokens_;
    std::size_t pos_ = 0;
    const std::string& file_;
    std::vector<ParseError>& errors_;
};

} // namespace

ParseResult parse_script(std::string_view text, std::string file_name) {
    ParseResult result;
    Lexer lexer(text, file_name, result.errors);
    std::vector<Token> tokens = lexer.run();
    Parser parser(std::move(tokens), file_name, result.errors);
    result.root = parser.parse_file();
    // Lexer- und Parserfehler nach Position ordnen (stabil, deterministisch).
    std::vector<ParseError>& errors = result.errors;
    for (std::size_t i = 1; i < errors.size(); ++i) {
        ParseError current = std::move(errors[i]);
        std::size_t j = i;
        while (j > 0 && (errors[j - 1].line > current.line ||
                         (errors[j - 1].line == current.line && errors[j - 1].column > current.column))) {
            errors[j] = std::move(errors[j - 1]);
            --j;
        }
        errors[j] = std::move(current);
    }
    return result;
}

ParseResult parse_script_file(const std::filesystem::path& path) {
    const std::string name = path.generic_string();
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        ParseResult result;
        result.root.is_block = true;
        result.errors.push_back(ParseError{name, 0, 0, "Datei kann nicht gelesen werden"});
        return result;
    }
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    if (in.bad()) {
        ParseResult result;
        result.root.is_block = true;
        result.errors.push_back(ParseError{name, 0, 0, "Fehler beim Lesen der Datei"});
        return result;
    }
    return parse_script(text, name);
}

} // namespace uhrwerk
