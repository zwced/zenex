# zenex

zenex is a lexer and parser library for C++20. You define a table of tokens, hand it a few options, and get back a positioned token stream, identifiers, numbers, strings, and comments already handled for you. A Pratt parser module sits on top of it for expression parsing, plus a small tree printer for when you want to actually look at what you built.

- Literal keyword and symbol matching with longest match resolution
- Regex based token matching, for your own rules and for skip patterns like comments
- Automatic identifier, number, string, and char literal detection
- Configurable whitespace and skipped-input handling: discard it or emit it as tokens
- Lenient recovery mode or strict table validation, your choice
- Case insensitive matching applied consistently across the whole pipeline
- Line, column, and byte offset tracking on every single token
- Custom numeric identifiers bindable to built-in token kinds
- Enum backed or plain numeric token identifiers
- Hookable error reporting, you decide what happens when something goes wrong
- A Pratt parser module (`zenex::PrattParser`) for expression parsing on top of the token stream
- A `TokenCursor` for hand-rolling statement/declaration parsing alongside the Pratt expression layer
- A small tree printer (`zenex::NodePrinter`) for dumping ASTs in a readable form
- No external dependencies beyond the C++ standard library

## Requirements

- Clang, on both Windows and Linux
- [Meson](https://mesonbuild.com/)
- [Ninja](https://ninja-build.org/)

## Building on Windows

### Option A: Visual Studio Build Tools

1. Install Python, then:
   ```powershell
   pip install meson ninja
   ```
2. Install Visual Studio Build Tools with the "C++ Clang tools for Windows" component, or via winget:
   ```powershell
   winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Component.VC.Llvm.Clang --add Microsoft.VisualStudio.Workload.VCTools"
   ```
3. Open the Developer PowerShell shortcut that comes with Build Tools, `clang-cl` and `lib.exe` both need to be on `PATH`, and a plain PowerShell or cmd window usually won't have either.
4. Build:
   ```powershell
   meson setup build --native-file build-cfg/windows-clang.ini
   meson compile -C build
   .\build\ztest.exe
   ```

### Option B: standalone LLVM

1. Install LLVM, via winget:
   ```powershell
   winget install LLVM.LLVM
   ```
   Make sure the LLVM `bin` directory ends up on `PATH`.
2. Install Meson and Ninja:
   ```powershell
   pip install meson ninja
   ```
3. Build:
   ```powershell
   meson setup build --native-file build-cfg/windows-clang.ini
   meson compile -C build
   .\build\ztest.exe
   ```
   If `meson setup` can't find Clang, check that `clang.exe`/`clang-cl.exe` actually resolve in your shell, and that `build-cfg/windows-clang.ini` matches your install.

## Building on Linux

**Arch / Manjaro / EndeavourOS / CachyOS**
```bash
sudo pacman -S clang meson ninja
```

**Ubuntu / Debian / Linux Mint / Pop!_OS / Kali Linux**
```bash
sudo apt update
sudo apt install clang meson ninja-build
```

**Fedora**
```bash
sudo dnf install clang meson ninja-build
```

Then:
```bash
meson setup build --native-file build-cfg/linux-clang.ini
meson compile -C build
./build/ztest
```

## Running the tests

```bash
meson test -C build
```

---

## Part 1: Lexing

### Example 1: the minimal lexer

This is about as small as a working lexer gets: two keywords, whitespace kept as tokens, comments thrown away, and lenient enough to not fall over on anything it doesn't recognise.

```cpp
#include <zenex/lex/token_api.h>
#include <zenex/lex/lexer.h>
#include <iostream>

enum class TokenEnum : uint8_t { VAR = 0, IF = 1 };

int main() {
    auto tokens = zenex::NewTokenList<TokenEnum>({
        zenex::AddToken(TokenEnum::VAR, "var", "VAR"),
        zenex::AddToken(TokenEnum::IF, "if", "IF")
    });

    zenex::Lexer lexer = zenex::CreateLexer(tokens, zenex::lexopt{
        zenex::lskip {
            zenex::regex { R"(//[^\n]*)" },
            zenex::regex { R"(/\*[\s\S]*?\*/)" }
        },
        zenex::lkeep_whitespace,
        zenex::llenient,
    });

    for (const auto& token : lexer->TokeniseInput("var x = 32;"))
        std::cout << token.as_text << ": " << token.face << '\n';
}
```

Note that keyword matching respects word boundaries automatically, `"var"` will not accidentally swallow the start of `"variable"`. You don't have to think about that; it's baked into how literal rules are matched.

### Example 2: a JSON-ish tokeniser with regex rules

Not every project is a programming language. Here's a table for something closer to a data format, mixing literal punctuation with a regex number rule:

```cpp
enum class Json : uint8_t { LBRACE, RBRACE, LBRACK, RBRACK, COLON, COMMA, TRUE_, FALSE_, NULL_, NUM };

auto tokens = zenex::NewTokenList<Json>({
    zenex::AddToken(Json::LBRACE, "{", "LBRACE"),
    zenex::AddToken(Json::RBRACE, "}", "RBRACE"),
    zenex::AddToken(Json::LBRACK, "[", "LBRACK"),
    zenex::AddToken(Json::RBRACK, "]", "RBRACK"),
    zenex::AddToken(Json::COLON, ":", "COLON"),
    zenex::AddToken(Json::COMMA, ",", "COMMA"),
    zenex::AddToken(Json::TRUE_, "true", "TRUE"),
    zenex::AddToken(Json::FALSE_, "false", "FALSE"),
    zenex::AddToken(Json::NULL_, "null", "NULL"),
    zenex::AddRegexToken(Json::NUM, R"(-?\d+(\.\d+)?([eE][+-]?\d+)?)", "NUMBER")
});

auto lexer = zenex::CreateLexer(tokens, zenex::lexopt{});
auto stream = lexer->TokeniseInput(R"({"ok": true, "value": -3.5e2})");
```

You don't even need to touch the built-in `String` handling rule here since double-quoted text is picked up automatically, you get `TokenKind::String` tokens for the keys and any string values with no extra table entries.

### Example 3: strict mode catches table mistakes before you ship

If you add two rules that share a face by accident (easy to do once a table grows past a dozen entries), `lstrict` catches it the moment the lexer is constructed, not at some unlucky point during a test run:

```cpp
try {
    auto lexer = zenex::CreateLexer(tokens, zenex::lexopt{ zenex::lstrict });
} catch (const zenex::LexException& e) {
    std::cerr << "bad token table: " << e.error.message << '\n';
}
```

This is a construction-time error, so there's no lexer instance yet to hang a handler off of. It always throws, unconditionally.

### Example 4: lenient recovery instead of hard failures

Sometimes you want a best-effort token stream even when the input is garbage. An editor's live syntax highlighter is the classic case, where you'd rather show something than crash on every keystroke:

```cpp
auto lexer = zenex::CreateLexer(tokens, zenex::lexopt{ zenex::llenient });
auto stream = lexer->TokeniseInput("var x = @@@;");
// the '@' characters come back as TokenKind::Fallback tokens instead of throwing
```

### Example 5: case insensitive keywords

Some languages (and most config formats) don't care about keyword casing. One flag handles it everywhere it matters: literal matching, skip patterns, and the `lstrict` duplicate check all respect it consistently.

```cpp
auto tokens = zenex::NewTokenList<TokenEnum>({
    zenex::AddToken(TokenEnum::IF, "if", "IF"),
});

auto lexer = zenex::CreateLexer(tokens, zenex::lexopt{ zenex::lcase_insensitive });
// "IF", "If", and "if" in the source all match the same rule
```

### Example 6: keeping skipped input around for debugging

Normally `lskip` patterns just vanish. If you're building something like a formatter or a doc-comment extractor, you actually want those comments back as tokens instead of losing them:

```cpp
auto lexer = zenex::CreateLexer(tokens, zenex::lexopt{
    zenex::lskip { zenex::regex { R"(//[^\n]*)" } },
    zenex::lkeep_skipped,
});

for (const auto& tok : lexer->TokeniseInput("x // a note about x"))
    if (tok.kind == zenex::TokenKind::Skipped)
        std::cout << "found a comment: " << tok.face << '\n';
```

### Example 7: binding your own identifiers onto built-in kinds

Numbers, strings, identifiers, and friends carry `enumeration = 0` by default since they didn't come from your rule table. If your parser needs to tell a `NUMBER_LIT` apart from a `STRING_LIT` by enum value rather than by `TokenKind`, then bind them:

```cpp
lexer->BindTokenKind(zenex::TokenKind::Number, zenex::TokenCast(TokenEnum::NUMBER_LIT));
lexer->BindTokenKind(zenex::TokenKind::String, zenex::TokenCast(TokenEnum::STRING_LIT));

// later, FindMapping reads it back, 0 if nothing was ever bound
uint8_t id = lexer->FindMapping(zenex::TokenKind::Number);
```

### Example 8: a runtime error handler instead of exceptions

If you'd rather collect every problem in a source file and report them all at once (like a real compiler does) instead of stopping at the first one, `OnError` is the hook. Registering a handler makes the lexer recover the same way `llenient` would, it just calls your callback on the way there:

```cpp
std::vector<zenex::LexError> problems;

lexer->OnError([&](const zenex::LexError& err) {
    problems.push_back(err);
});

auto stream = lexer->TokeniseInput(source); // never throws now

for (const auto& err : problems) {
    switch (err.type) {
        case zenex::LexErrorType::UnexpectedCharacter:
            std::cerr << "[" << err.line << ":" << err.column << "] stray character\n";
            break;
        case zenex::LexErrorType::UnterminatedCharLiteral:
            std::cerr << "[" << err.line << ":" << err.column << "] char literal never closed\n";
            break;
        case zenex::LexErrorType::InvalidCharLiteralLength:
            std::cerr << "[" << err.line << ":" << err.column << "] char literal must be one character\n";
            break;
        default: break;
    }
}
```

Registering a handler *and* setting `llenient` at the same time is harmless, `llenient` just means the handler never actually fires, since nothing reaches the error path in the first place.

### Example 9: checking a raw string against the rule table

`IsToken` is a small utility for when you have a bare string from somewhere else (user input, a config file, wherever) and want to know if it would match one of your literal rules, without running the whole scanner over it:

```cpp
if (lexer->IsToken("var")) {
    // "var" is a reserved word in this table, reject it as an identifier name
}
```

---

## Part 2: Parsing with Pratt, on top of the token stream

zenex ships a Pratt parser (`zenex::PrattParser<Node>`, in `zenex/pratt/`) for the layer above lexing. Each token gets a prefix and/or infix rule plus a binding power, and precedence falls out of comparing binding powers as the parser recurses. It's the standard approach for expression-heavy grammars: adding an operator is one table entry, not a new grammar rule. It slots in cleanly under a hand-written recursive descent parser for statements. If you're building a language, this is usually the right tool for expressions. If you're working on a data format, you probably won't need it at all.

`PrattParser<Node>` is templated on your own AST node type, so it doesn't impose one on you. `TokenCursor` wraps the token stream your lexer produced and gives you `Peek`/`Advance`/`Expect`-style access for both the Pratt machinery and any hand-written statement parsing you layer around it.

### Example 10: a basic arithmetic expression parser

This is the shape you'll reach for most often: numbers as leaves, `+ - * /` as infix operators, higher binding power for the tighter-binding operators:

```cpp
#include <zenex/pratt/parser.h>
#include <variant>

struct Node {
    std::variant<double, std::pair<char, std::pair<Node*, Node*>>> data;
    // (a toy AST, use whatever node representation fits your project)
};

zenex::PrattParser<Node> parser;

parser.RegisterPrefix(TokenCast(Expr::NUMBER), [](auto& p, auto& cur) -> Node {
    return Node{ std::stod(cur.Current().face) };
});

parser.RegisterInfix(TokenCast(Expr::PLUS), 10, [](auto& p, auto& cur, Node left) -> Node {
    Node right = p.ParseExpression(cur, 10);
    return makeBinary('+', std::move(left), std::move(right));
});

parser.RegisterInfix(TokenCast(Expr::STAR), 20, [](auto& p, auto& cur, Node left) -> Node {
    Node right = p.ParseExpression(cur, 20);
    return makeBinary('*', std::move(left), std::move(right));
});

zenex::TokenCursor cursor(tokens);
Node ast = parser.ParseExpression(cursor);
```

`*` binding at 20 and `+` binding at 10 is what makes `2 + 3 * 4` parse as `2 + (3 * 4)` instead of left to right. The infix loop only keeps consuming an operator while its binding power is greater than `min_bp`, so the higher-precedence `*` gets pulled in first.

### Example 11: unary minus as a prefix rule

Prefix rules aren't just for literals. Anything that can *start* an expression goes through `RegisterPrefix`, including unary operators:

```cpp
parser.RegisterPrefix(TokenCast(Expr::MINUS), [](auto& p, auto& cur) -> Node {
    Node operand = p.ParseExpression(cur, 100); // bind tighter than any infix operator
    return makeUnary('-', std::move(operand));
});
```

Passing a high `min_bp` into the recursive call is what stops `-a + b` from being parsed as `-(a + b)`. The recursion only swallows operators that bind tighter than 100, so `+` gets left for the outer call to pick up.

### Example 12: parenthesized grouping

Grouping is also just a prefix rule. It parses a sub-expression starting fresh at `min_bp = 0`, then expects the closing paren before handing the inner node back up:

```cpp
parser.RegisterPrefix(TokenCast(Expr::LPAREN), [](auto& p, auto& cur) -> Node {
    Node inner = p.ParseExpression(cur, 0);
    cur.Expect(TokenCast(Expr::RPAREN), "')'");
    return inner;
});
```

`Expect` throws a `ParseException` with the exact line and column of the offending token if the closing paren never shows up, so you're not left guessing where the malformed input was.

### Example 13: statements and blocks with `TokenCursor` directly

Not everything is an expression. Declarations, `if`/`while`, and function bodies usually want hand-written statement parsing sitting alongside the Pratt layer, using the same cursor:

```cpp
parser.RegisterStatement(TokenCast(Stmt::LET), [](auto& p, auto& cur) -> Node {
    cur.Advance(); // consume 'let'
    const auto& name = cur.Expect(TokenCast(Expr::IDENT), "an identifier");
    cur.Expect(TokenCast(Expr::EQUAL), "'='");
    Node value = p.ParseExpression(cur);
    cur.Expect(TokenCast(Expr::SEMI), "';'");
    return makeLet(name.face, std::move(value));
});

parser.RegisterStatement(TokenCast(Stmt::LBRACE), [](auto& p, auto& cur) -> Node {
    cur.Advance(); // consume '{'
    auto body = p.ParseBlock(cur, TokenCast(Stmt::RBRACE), "'}'");
    return makeBlock(std::move(body));
});
```

`ParseBlock` keeps calling `ParseStatement` until it sees the close token or runs off the end of input, then consumes the closer for you. It's the same helper whether the block is a function body, an `if` arm, or a loop.

### Example 14: parsing a whole program, with error collection

`ParseProgram` is the top-level entry point, it just calls `ParseStatement` on repeat until `TokenCursor::AtEnd()`. Combine it with `OnError` to collect every parse failure instead of bailing on the first one:

```cpp
std::vector<zenex::ParseError> problems;
parser.OnError([&](const zenex::ParseError& err) {
    problems.push_back(err);
});

zenex::TokenCursor cursor(tokens);
try {
    std::vector<Node> program = parser.ParseProgram(cursor);
} catch (const zenex::ParseException& e) {
    // OnError already ran before this throws, problems has the details
}
```

Unlike the lexer, `PrattParser` always throws once a rule genuinely can't proceed. `OnError` here is for logging alongside the exception, not for suppressing it. If you need real per-statement recovery, catch around each `ParseStatement` call in your own driver loop and resynchronise the cursor yourself (skip to the next statement boundary, for instance).

### Example 15: distinguishing "no rule" errors from "wrong token" errors

`ParseErrorType` tells you *why* a parse failed, which matters if you want different diagnostics for a token that's simply not allowed to start an expression versus one that's just not what was expected next:

```cpp
parser.OnError([](const zenex::ParseError& err) {
    switch (err.type) {
        case zenex::ParseErrorType::NoPrefixRule:
            std::cerr << "line " << err.line << ": '" << err.message << "' can't start an expression\n";
            break;
        case zenex::ParseErrorType::UnexpectedToken:
            std::cerr << "line " << err.line << ": " << err.message << '\n';
            break;
        case zenex::ParseErrorType::NoStatementRule:
        case zenex::ParseErrorType::UnexpectedEndOfInput:
        default:
            std::cerr << "line " << err.line << ": " << err.message << '\n';
            break;
    }
});
```

---

## Part 3: Printing a tree

`zenex::NodePrinter<Node>` is a small, deliberately unopinionated tool for turning whatever `Node` type you parsed into readable text: a debug dump, a `--dump-ast` flag, a snapshot test fixture. You give it a way to identify a node's kind and a print function per kind; it handles indentation, tree connectors, and recursion for you.

### Example 16: registering a tag function and per-kind printers

```cpp
#include <zenex/pratt/printer.h>

enum class NodeKind : uint32_t { Number, Binary, Let };

zenex::NodePrinter<Node> printer;
printer.SetTagFn([](const Node& n) { return static_cast<uint32_t>(n.kind); });

printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::Number), [](const Node& n, auto& ctx) {
    ctx.Write("Number(" + std::to_string(n.number_value) + ")");
});

printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::Binary), [](const Node& n, auto& ctx) {
    ctx.WriteLine(std::string("Binary '") + n.op + "'");
    ctx.PrintChildren(std::vector<Node>{ *n.left, *n.right });
});

std::string dump = printer.PrintStructure(ast);
std::cout << dump;
```

A node kind with no registered printer doesn't crash the whole dump. It just prints `<unregistered node kind N>` inline and moves on, which is handy while you're still filling out node types during development.

### Example 17: tree connectors for homogeneous children

`PrintChildren` is built for the common case: a node with a list of same-shaped children, like a block's statement list or a call's argument list. It takes care of the `├──` / `└──` branching and indentation bookkeeping so the last child gets a corner instead of a tee:

```cpp
printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::Block), [](const Node& n, auto& ctx) {
    ctx.WriteLine("Block");
    ctx.PrintChildren(n.statements);
});
```

### Example 18: flat, delimiter-joined output for JSON-style dumps

Sometimes you don't want a tree, you want a single line. `PrintList` skips the connectors entirely and just joins children with whatever separator you give it:

```cpp
printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::ArgList), [](const Node& n, auto& ctx) {
    ctx.Write("(");
    ctx.PrintList(n.args, ", ");
    ctx.Write(")");
});
```

### Example 19: labeled scalar fields alongside child nodes

Not everything worth printing is a child `Node`, a `Let` statement's variable name is just a string. `Field` writes it as a `label=value` pair without trying to route it through the recursive printer:

```cpp
printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::Let), [](const Node& n, auto& ctx) {
    ctx.Write("Let ");
    ctx.Field("name", n.name);
    ctx.WriteLine("");
    ctx.Indent();
    ctx.PrintChild(*n.value);
    ctx.Dedent();
});
```

### Example 20: the full pipeline, source text to printed tree

Putting all three layers together (lex, parse, print) is the whole point of the library:

```cpp
auto lexer = zenex::CreateLexer(tokens, zenex::lexopt{ zenex::llenient });
zenex::LexerTokens stream = lexer->TokeniseInput("let x = 2 + 3 * 4;");

zenex::TokenCursor cursor(stream);
std::vector<Node> program = parser.ParseProgram(cursor);

for (const auto& stmt : program)
    std::cout << printer.PrintStructure(stmt) << '\n';
```

Three small, independent pieces (a lexer, a Pratt parser, and a tree printer) that don't know about each other beyond the `Node` type and the `LexerTokens` stream that pass between them. Swap any one of them out for a hand-written version and the others don't care.

---

## What the scanner does automatically

Beyond your literal and regex rules, every lexer handles these without any setup:

| Input | Becomes |
| --- | --- |
| `'x'` | `TokenKind::Char`, must contain exactly one character unless `llenient` is set |
| `"..."`, backslash escapes respected | `TokenKind::String` |
| A run of digits, with an optional single `.` | `TokenKind::Number` |
| A run of letters, digits, and underscores | `TokenKind::Identifier` |
| Anything matching an `lskip` pattern | discarded, or kept as `TokenKind::Skipped` under `lkeep_skipped` |
| Whitespace | discarded, or kept as `TokenKind::Whitespace` under `lkeep_whitespace` |
| Anything else | see error handling below |
| End of input | a trailing `TokenKind::EndOfFile` token |

## Lexer options (`zenex::lexopt`)

Options are passed as a flat, freely ordered list.

| Option | Effect |
| --- | --- |
| `lfast` | Enables internal scan optimisations. Line, column, and offset tracking stay accurate. |
| `lkeep_whitespace` | Emits whitespace runs as tokens instead of discarding them. |
| `lkeep_skipped` | Emits input matched by `lskip` patterns as `Skipped` tokens instead of discarding them. |
| `llenient` | Recovers from unmatched input instead of stopping. |
| `lstrict` | Validates the rule table at construction time, errors if two rules share the same literal face. Mutually exclusive with `llenient`. |
| `lcase_insensitive` | Case insensitive matching across literals, skip patterns, and the strict table check. |
| `lskip { regex{...}, ... }` | Regex patterns to discard or emit, typically comments. |

Passing both `llenient` and `lstrict` throws at construction time.

## Token kinds and the token structure

```cpp
enum class TokenKind : uint8_t {
    UserDefined, Whitespace, Skipped, Identifier, Number, String, Char, Fallback, EndOfFile
};

struct LexerToken {
    TokenKind kind;
    uint8_t enumeration;   // your rule's id, or a value bound via BindTokenKind
    std::string face;      // exact matched text
    std::string as_text;   // printable label
    uint32_t line, column; // one based
    uint32_t start_offset, end_offset; // byte offsets
};
```

## Error handling

There are two kinds of lexer errors, and they work differently because of when they happen.

**Construction time errors** are things like `lexopt`'s `llenient`/`lstrict` conflict, or a duplicate rule face caught by `lstrict`. These happen before a `Lexer` exists, so there's nothing to hook a callback onto yet. They always throw a `zenex::LexException`.

**Runtime errors** happen inside `TokeniseInput`: an unterminated char literal, a char literal with more than one character, or a character that matches nothing at all. These go through one of three paths depending on what you set up:

- `llenient` set: the lexer recovers silently, no callback, no exception.
- A handler registered with `OnError`: your callback runs, then the lexer recovers the same way `llenient` would.
- Neither: `TokeniseInput` throws a `zenex::LexException`.

`LexError` carries a `type` field (`zenex::LexErrorType`) if you want to branch on the kind of problem instead of parsing the message string. See Example 8 above for both the handler and the switch-on-type pattern.

Parser errors work slightly differently (see Examples 14 and 15 in Part 2), since `PrattParser::OnError` doesn't suppress the throw the way the lexer's does.

## API reference

### Lexing

| Symbol | Description |
| --- | --- |
| `NewTokenList<TokenEnum>(entries)` | Builds a token table. |
| `AddToken(enumeration, face, as_text)` | Builds a literal rule entry. |
| `AddRegexToken(enumeration, pattern, as_text)` | Builds a regex rule entry. |
| `TokenCast(value)` | Converts an enum value to its numeric form. |
| `NO_ENUM_TABLE` | Use in place of an enum type for plain numeric ids. |
| `CreateLexer(tokens, opt)` | Builds a `zenex::Lexer`, may throw `LexException`. |
| `Lexer` | `std::shared_ptr` to the lexer instance. |
| `Lexer::TokeniseInput(source)` | Scans source text, returns `LexerTokens`, may throw `LexException`. |
| `Lexer::OnError(handler)` | Registers a runtime error callback, see above. |
| `Lexer::IsToken(input)` | True if `input` exactly matches a rule's face. |
| `Lexer::BindTokenKind(kind, value)` | Assigns a numeric id to a built-in `TokenKind`. |
| `Lexer::FindMapping(kind)` | Reads back a bound id, `0` if unbound. |
| `lexopt{ ... }` | Behaviour flags, see table above. |
| `lskip{ regex{...}, ... }` | Discard or expose patterns. |
| `regex{ pattern }` | Wraps one pattern string. |
| `TokenKind` | Category enum for a token. |
| `LexerToken` / `LexerTokens` | A token, and a vector of them. |
| `LexError` / `LexErrorType` | Runtime error payload and category. |
| `LexException` | Thrown when there's no handler and the lexer isn't lenient. |

### Parsing (`zenex::PrattParser<Node>`, `zenex::TokenCursor`)

| Symbol | Description |
| --- | --- |
| `TokenCursor(tokens)` | Wraps a `LexerTokens` stream for sequential, positional access. |
| `TokenCursor::Peek()` | Current token, without consuming it. |
| `TokenCursor::Current()` | The most recently consumed token. |
| `TokenCursor::Advance()` | Consumes and returns the current token. |
| `TokenCursor::AtEnd()` | True once the cursor reaches `EndOfFile`. |
| `TokenCursor::Check(enumeration)` | True if the current token matches, without consuming. |
| `TokenCursor::Match(enumeration)` | Consumes the current token if it matches, returns whether it did. |
| `TokenCursor::Expect(enumeration, what)` | Consumes the expected token or throws `ParseException`. |
| `PrattParser<Node>::RegisterPrefix(enum, fn)` | Registers a rule for a token that can start an expression. |
| `PrattParser<Node>::RegisterInfix(enum, bp, fn)` | Registers a rule for a token appearing after a left-hand side, with a binding power. |
| `PrattParser<Node>::RegisterStatement(enum, fn)` | Registers a rule dispatched on a statement's leading token. |
| `PrattParser<Node>::OnError(handler)` | Registers a callback invoked before a `ParseException` is thrown. |
| `PrattParser<Node>::ParseExpression(cursor, min_bp = 0)` | Parses one expression, recursing while the next infix binding power exceeds `min_bp`. |
| `PrattParser<Node>::ParseStatement(cursor)` | Parses one statement, falling back to a bare expression if nothing is registered. |
| `PrattParser<Node>::ParseBlock(cursor, close, what)` | Parses statements until `close`, then consumes it. |
| `PrattParser<Node>::ParseProgram(cursor)` | Parses statements until end of input. |
| `ParseError` / `ParseErrorType` | Parse failure payload and category. |
| `ParseException` | Thrown when a rule genuinely can't proceed. |

### Printing (`zenex::NodePrinter<Node>`, `zenex::PrintContext<Node>`)

| Symbol | Description |
| --- | --- |
| `NodePrinter<Node>::SetTagFn(fn)` | Registers the function used to identify a node's kind. |
| `NodePrinter<Node>::RegisterPrinter(tag, fn)` | Registers a print function for a given node kind. |
| `NodePrinter<Node>::PrintStructure(root)` | Prints an entire tree and returns the accumulated output. |
| `PrintContext::Write(s)` / `WriteLine(s)` | Appends raw text, with or without a trailing newline. |
| `PrintContext::PrintChild(node)` | Recurses into a single child node. |
| `PrintContext::PrintChildren(children)` | Prints a homogeneous list with tree connectors (`├──` / `└──`). |
| `PrintContext::PrintList(children, sep)` | Prints a delimiter-joined list, no connectors. |
| `PrintContext::Field(label, value)` | Writes a labeled scalar value. |
| `PrintContext::Indent(n=1)` / `Dedent(n=1)` | Adjusts indentation depth. |
| `PrintContext::Pad(unit="  ")` | Builds the current indentation prefix. |
| `PrintContext::Connector(is_last)` | Returns a branch or corner connector glyph. |

## License
zenex is licensed under MIT
