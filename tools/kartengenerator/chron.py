"""Parser für das Chronik-Skript (*.chron) – Python-Fassung für Werkzeuge.

Verbindliche Spezifikation: docs/technik/chronik-skript.md, Abschnitt 1.
Das Verhalten folgt dem Parser des Simulationskerns (core/src/script.cpp):
gleiche Knotenformen, gleiche Fehlermeldungen, gleiches Wiederaufsetzen nach
Fehlern. Zeilen und Spalten beginnen bei 1, die Spalte zählt Bytes.

Nur Python-Standardbibliothek (Python 3.10+).
"""

from __future__ import annotations

import re
from collections.abc import Iterator
from dataclasses import dataclass, field
from pathlib import Path

__all__ = [
    "MAX_TIEFE",
    "ParseError",
    "ParseResult",
    "ScriptNode",
    "parse_bool",
    "parse_ganzzahl",
    "parse_script",
    "parse_script_file",
    "parse_zahl",
]

#: Höchste Verschachtelungstiefe von Blöcken (wie im Kern).
MAX_TIEFE = 256

_ZAHL = re.compile(r"-?[0-9]+(\.[0-9]+)?")
_GANZZAHL = re.compile(r"-?[0-9]+")


# ─── Werte ──────────────────────────────────────────────────────────────────


def parse_zahl(text: str) -> float | None:
    """Festkommazahl nach Abschnitt 1.5 (optionales '-', Ziffern, optional '.' und Ziffern).

    Gibt None zurück, wenn das Format nicht passt ("5.", ".5", "+1", "1e3" sind ungültig).
    """
    if _ZAHL.fullmatch(text) is None:
        return None
    return float(text)


def parse_ganzzahl(text: str) -> int | None:
    """Ganzzahl nach Abschnitt 1.5 (optionales '-', Ziffern) oder None."""
    if _GANZZAHL.fullmatch(text) is None:
        return None
    try:
        return int(text)
    except ValueError:  # mehr Stellen, als Python ohne Weiteres umwandelt
        return None


def parse_bool(text: str) -> bool | None:
    """ja/yes/wahr → True, nein/no/falsch → False, sonst None (Groß-/Kleinschreibung zählt)."""
    if text in ("ja", "yes", "wahr"):
        return True
    if text in ("nein", "no", "falsch"):
        return False
    return None


# ─── Syntaxbaum ─────────────────────────────────────────────────────────────


@dataclass
class ScriptNode:
    """Ein Knoten im Syntaxbaum (Abbildung siehe Spezifikation, Abschnitt 1.3).

    Formen:
      key = value       → key, op "=", value, kein Block
      key >= value      → key, op ">=", value
      key = { ... }     → key, op "=", is_block, children
      key { ... }       → wie key = { ... }
      typ name { ... }  → key = typ, value = name, op "=", is_block, children
      value             → bare: key leer, op leer, value gesetzt (Listeneintrag)
      { ... }           → anonymer Block: key leer, is_block
    """

    key: str = ""
    op: str = ""
    value: str = ""
    is_block: bool = False
    quoted: bool = False
    children: list[ScriptNode] = field(default_factory=list)
    line: int = 0
    column: int = 0

    def find(self, key: str) -> ScriptNode | None:
        """Erstes Kind mit diesem Schlüssel oder None."""
        for child in self.children:
            if child.key == key:
                return child
        return None

    def find_all(self, key: str) -> list[ScriptNode]:
        """Alle Kinder mit diesem Schlüssel, in Dateireihenfolge."""
        return [child for child in self.children if child.key == key]

    def get_string(self, key: str) -> str | None:
        """Skalarwert des ersten passenden Kindes (kein Block, op "=") oder None."""
        for child in self.children:
            if child.key == key and not child.is_block and child.op == "=":
                return child.value
        return None

    def get_zahl(self, key: str) -> float | None:
        text = self.get_string(key)
        return None if text is None else parse_zahl(text)

    def get_int(self, key: str) -> int | None:
        text = self.get_string(key)
        return None if text is None else parse_ganzzahl(text)

    def get_bool(self, key: str) -> bool | None:
        text = self.get_string(key)
        return None if text is None else parse_bool(text)

    def bare_values(self) -> list[str]:
        """Werte aller bare-Einträge (z. B. lage = { 13.40 52.52 })."""
        return [child.value for child in self.children if not child.key and not child.is_block]

    def walk(self) -> Iterator[ScriptNode]:
        """Alle Nachfahren in Dokumentreihenfolge (ohne den Knoten selbst)."""
        stack = list(reversed(self.children))
        while stack:
            node = stack.pop()
            yield node
            stack.extend(reversed(node.children))


@dataclass
class ParseError:
    file: str
    line: int
    column: int
    message: str

    def __str__(self) -> str:
        if self.line <= 0:
            return f"{self.file}: {self.message}"
        if self.column <= 0:
            return f"{self.file}:{self.line}: {self.message}"
        return f"{self.file}:{self.line}:{self.column}: {self.message}"


@dataclass
class ParseResult:
    root: ScriptNode
    errors: list[ParseError]

    @property
    def ok(self) -> bool:
        return not self.errors


# ─── Lexer ──────────────────────────────────────────────────────────────────

_WORD, _STRING, _LBRACE, _RBRACE, _OP, _END = range(6)

_SPACE = frozenset(b" \t\r\n")
_NOT_WORD = frozenset(b' \t\r\n{}=<>!#"')


@dataclass
class _Token:
    type: int
    text: bytes
    line: int
    column: int


def _find_invalid_utf8(data: bytes) -> int:
    """Index der ersten ungültigen UTF-8-Stelle oder -1."""
    try:
        data.decode("utf-8", errors="strict")
    except UnicodeDecodeError as exc:
        return exc.start
    return -1


def _decode(data: bytes) -> str:
    return data.decode("utf-8", errors="replace")


class _Lexer:
    def __init__(self, data: bytes, file: str, errors: list[ParseError]) -> None:
        self.data = data
        self.file = file
        self.errors = errors
        self.pos = 0
        self.line = 1
        self.line_start = 0

    def column(self) -> int:
        return self.pos - self.line_start + 1

    def make(self, type_: int, text: bytes) -> _Token:
        return _Token(type_, text, self.line, self.column())

    def error(self, line: int, column: int, message: str) -> None:
        self.errors.append(ParseError(self.file, line, column, message))

    def check_utf8(self, raw: bytes, line: int, column: int) -> None:
        bad = _find_invalid_utf8(raw)
        if bad >= 0:
            self.error(line, column + bad, "ungültige UTF-8-Kodierung (Datei als UTF-8 speichern)")

    def run(self) -> list[_Token]:
        data = self.data
        n = len(data)
        tokens: list[_Token] = []
        # BOM am Dateianfang überspringen; Spalten zählen danach ab 1.
        if data[:3] == b"\xef\xbb\xbf":
            self.pos = 3
            self.line_start = 3
        while self.pos < n:
            c = data[self.pos]
            if c == 0x0A:  # \n
                self.pos += 1
                self.line += 1
                self.line_start = self.pos
            elif c in _SPACE:
                self.pos += 1
            elif c == 0x23:  # '#'
                end = data.find(b"\n", self.pos)
                self.pos = n if end < 0 else end
            elif c == 0x7B:  # '{'
                tokens.append(self.make(_LBRACE, b"{"))
                self.pos += 1
            elif c == 0x7D:  # '}'
                tokens.append(self.make(_RBRACE, b"}"))
                self.pos += 1
            elif c == 0x3D:  # '='
                tokens.append(self.make(_OP, b"="))
                self.pos += 1
            elif c in (0x3C, 0x3E):  # '<' '>'
                with_eq = self.pos + 1 < n and data[self.pos + 1] == 0x3D
                op = bytes((c,)) + (b"=" if with_eq else b"")
                tokens.append(self.make(_OP, op))
                self.pos += 2 if with_eq else 1
            elif c == 0x21:  # '!'
                if self.pos + 1 < n and data[self.pos + 1] == 0x3D:
                    tokens.append(self.make(_OP, b"!="))
                    self.pos += 2
                else:
                    self.error(self.line, self.column(), "einzelnes '!' (erwartet '!=')")
                    self.pos += 1
            elif c == 0x22:  # '"'
                tokens.append(self.read_string())
            else:
                tokens.append(self.read_word())
        tokens.append(self.make(_END, b""))
        return tokens

    def read_word(self) -> _Token:
        token = self.make(_WORD, b"")
        data = self.data
        n = len(data)
        begin = self.pos
        while self.pos < n and data[self.pos] not in _NOT_WORD:
            self.pos += 1
        token.text = data[begin : self.pos]
        self.check_utf8(token.text, token.line, token.column)
        return token

    def read_string(self) -> _Token:
        token = self.make(_STRING, b"")
        data = self.data
        n = len(data)
        out = bytearray()
        self.pos += 1  # öffnendes "
        closed = False
        while self.pos < n:
            c = data[self.pos]
            if c in (0x0A, 0x0D):
                break
            if c == 0x22:
                self.pos += 1
                closed = True
                break
            if c == 0x5C:  # Backslash
                nxt = data[self.pos + 1] if self.pos + 1 < n else None
                if nxt in (0x22, 0x5C):
                    out.append(nxt)
                    self.pos += 2
                    continue
                if nxt is None or nxt in (0x0A, 0x0D):
                    # Zeilen- bzw. Dateiende beendet die Zeichenkette (Fehler unten).
                    out.append(c)
                    self.pos += 1
                    continue
                shown = _decode(data[self.pos + 1 : self.pos + 5])[:1]
                self.error(
                    self.line,
                    self.column(),
                    f"ungültige Escape-Sequenz '\\{shown}' (erlaubt sind \\\" und \\\\)",
                )
                out.append(c)
                self.pos += 1
                continue
            out.append(c)
            self.pos += 1
        if not closed:
            self.error(token.line, token.column, "nicht beendete Zeichenkette")
        token.text = bytes(out)
        self.check_utf8(token.text, token.line, token.column + 1)
        return token


# ─── Parser ─────────────────────────────────────────────────────────────────


@dataclass
class _Frame:
    """Ein offener Block: Zielliste der Anweisungen, öffnende Klammer, Tiefe."""

    out: list[ScriptNode]
    open: _Token | None
    depth: int


class _Parser:
    """Iterativer Parser (expliziter Stapel statt Rekursion, auch bei 256 Ebenen sicher)."""

    def __init__(self, tokens: list[_Token], file: str, errors: list[ParseError]) -> None:
        self.tokens = tokens
        self.file = file
        self.errors = errors
        self.pos = 0
        self.stack: list[_Frame] = []

    def peek(self, ahead: int = 0) -> _Token:
        index = self.pos + ahead
        return self.tokens[index] if index < len(self.tokens) else self.tokens[-1]

    def advance(self) -> _Token:
        token = self.peek()
        if self.pos + 1 < len(self.tokens):
            self.pos += 1
        return token

    def error(self, at: _Token, message: str) -> None:
        self.errors.append(ParseError(self.file, at.line, at.column, message))

    @staticmethod
    def describe(token: _Token) -> str:
        if token.type == _END:
            return "Dateiende"
        if token.type == _STRING:
            return "Zeichenkette"
        return "'" + _decode(token.text) + "'"

    def skip_rest_of_line(self, line: int) -> None:
        """Überspringt alle Tokens derselben Zeile bis zur nächsten Klammer."""
        while True:
            token = self.peek()
            if token.type in (_END, _LBRACE, _RBRACE) or token.line != line:
                return
            self.advance()

    def skip_block(self) -> None:
        """Überspringt einen zu tief verschachtelten Block samt Inhalt."""
        depth = 1
        while depth > 0 and self.peek().type != _END:
            token = self.advance()
            if token.type == _LBRACE:
                depth += 1
            elif token.type == _RBRACE:
                depth -= 1

    def open_block(self, node: ScriptNode, target: list[ScriptNode] | None, depth: int) -> None:
        """Erwartet '{' als aktuelles Token und öffnet den Block von node.

        target ist die Liste, an die node angehängt wird (None: verwerfen).
        """
        open_token = self.advance()
        if depth + 1 > MAX_TIEFE:
            self.error(open_token, f"Blöcke zu tief verschachtelt (höchstens {MAX_TIEFE} Ebenen)")
            self.skip_block()
            return
        node.is_block = True
        if target is not None:
            target.append(node)
        self.stack.append(_Frame(node.children, open_token, depth + 1))

    def parse_value(self, node: ScriptNode, op: _Token, target: list[ScriptNode] | None, depth: int) -> None:
        """Liest den Wert nach einem Operator und hängt node bei Erfolg an target an."""
        value = self.peek()
        if value.type == _WORD:
            # "key =" am Zeilenende, gefolgt von "anderer = …" in der nächsten Zeile:
            # Das Wort gehört zur nächsten Anweisung, der Wert fehlt.
            if value.line != op.line and self.peek(1).type == _OP:
                self.error(op, f"Wert nach '{_decode(op.text)}' fehlt")
                return
            node.value = _decode(value.text)
            self.advance()
            if target is not None:
                target.append(node)
        elif value.type == _STRING:
            node.value = _decode(value.text)
            node.quoted = True
            self.advance()
            if target is not None:
                target.append(node)
        elif value.type == _LBRACE:
            self.open_block(node, target, depth)
        elif value.type == _OP:
            self.error(op, f"Wert nach '{_decode(op.text)}' fehlt (stattdessen {self.describe(value)})")
            if value.line == op.line:
                self.skip_rest_of_line(value.line)
        else:  # '}' oder Dateiende
            self.error(op, f"Wert nach '{_decode(op.text)}' fehlt")

    def parse_word_statement(self, out: list[ScriptNode], depth: int) -> None:
        key = self.peek()
        nxt = self.peek(1)
        node = ScriptNode(line=key.line, column=key.column)
        if nxt.type == _OP:
            node.key = _decode(key.text)
            node.op = _decode(nxt.text)
            self.advance()
            op = self.advance()
            self.parse_value(node, op, out, depth)
            return
        if nxt.type == _LBRACE:
            # key { … }  ≙  key = { … }
            node.key = _decode(key.text)
            node.op = "="
            self.advance()
            self.open_block(node, out, depth)
            return
        if nxt.type == _WORD and self.peek(2).type == _LBRACE:
            # typ name { … }
            node.key = _decode(key.text)
            node.op = "="
            node.value = _decode(nxt.text)
            self.advance()
            self.advance()
            self.open_block(node, out, depth)
            return
        # bare-Eintrag
        node.value = _decode(key.text)
        self.advance()
        out.append(node)

    def parse_file(self) -> ScriptNode:
        root = ScriptNode(is_block=True, line=1, column=1)
        self.stack = [_Frame(root.children, None, 0)]
        while self.stack:
            frame = self.stack[-1]
            token = self.peek()
            if token.type == _END:
                # Alle noch offenen Blöcke melden (innerster zuerst, wie im Kern).
                for open_frame in reversed(self.stack):
                    if open_frame.open is not None:
                        self.error(
                            token,
                            f"nicht geschlossener Block (geöffnet in Zeile {open_frame.open.line})",
                        )
                self.stack.clear()
            elif token.type == _RBRACE:
                self.advance()
                if frame.open is not None:
                    self.stack.pop()
                else:
                    self.error(token, "unerwartetes '}'")
            elif token.type == _LBRACE:
                node = ScriptNode(is_block=True, line=token.line, column=token.column)
                self.open_block(node, frame.out, frame.depth)
            elif token.type == _STRING:
                if self.peek(1).type == _OP:
                    self.error(token, "eine Zeichenkette kann kein Schlüssel sein")
                    self.advance()  # Zeichenkette
                    op = self.advance()
                    self.parse_value(ScriptNode(), op, None, frame.depth)
                else:
                    frame.out.append(
                        ScriptNode(
                            value=_decode(token.text),
                            quoted=True,
                            line=token.line,
                            column=token.column,
                        )
                    )
                    self.advance()
            elif token.type == _OP:
                self.error(token, f"unerwarteter Operator '{_decode(token.text)}'")
                self.skip_rest_of_line(token.line)
            else:  # Wort
                self.parse_word_statement(frame.out, frame.depth)
        return root


# ─── Öffentliche Funktionen ─────────────────────────────────────────────────


def parse_script(text: bytes | str, file_name: str = "<text>") -> ParseResult:
    """Parst Text (bytes oder str; str wird als UTF-8 kodiert).

    file_name dient nur Fehlermeldungen. Bei Fehlern wird so viel wie möglich
    weitergeparst; die Fehler sind nach Zeile und Spalte geordnet.
    """
    data = text.encode("utf-8") if isinstance(text, str) else bytes(text)
    errors: list[ParseError] = []
    tokens = _Lexer(data, file_name, errors).run()
    root = _Parser(tokens, file_name, errors).parse_file()
    errors.sort(key=lambda e: (e.line, e.column))  # stabil, deterministisch
    return ParseResult(root, errors)


def parse_script_file(path: str | Path, file_name: str | None = None) -> ParseResult:
    """Liest und parst eine Datei (UTF-8, optionales BOM wird ignoriert).

    file_name ersetzt den Pfad in Fehlermeldungen (Standard: der Pfad mit '/').
    """
    name = file_name if file_name is not None else Path(path).as_posix()
    try:
        data = Path(path).read_bytes()
    except OSError:
        return ParseResult(
            ScriptNode(is_block=True, line=1, column=1),
            [ParseError(name, 0, 0, "Datei kann nicht gelesen werden")],
        )
    return parse_script(data, name)
