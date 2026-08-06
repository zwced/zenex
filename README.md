# zenex

zenex is a lexer and parser library for C++20. You define a table of tokens, hand it a few options, and get back a positioned token stream, identifiers, numbers, strings, and comments already handled. A Pratt parser module sits on top of it for expression parsing.

It exists because writing a lexer by hand is mostly the same handful of problems every time (keyword matching, comment skipping, string escapes, whitespace, position tracking) and none of it is where the interesting part of a language project actually lives. zenex handles that part so you can spend your time on IR generation, optimisation passes, codegen, or an assembler backend instead. It works just as well if you are not building a language at all, if you need a tokeniser for a data format like JSON, YAML, or TOML, zenex gives you that layer without making you design a grammar you do not need.

## Features

- Literal keyword and symbol matching with longest match resolution
- Regex based token matching, for your own rules and for skip patterns like comments
- Automatic identifier, number, string, and char literal detection
- Configurable whitespace and skipped-input handling, discard or emit as tokens
- Lenient recovery mode or strict table validation, your choice
- Case insensitive matching applied consistently across the whole pipeline
- Line, column, and byte offset tracking on every token
- Custom numeric identifiers bindable to built in token kinds
- Enum backed or plain numeric token identifiers
- Hookable error reporting, decide yourself what happens when something goes wrong
- A Pratt parser module (`zenex::Pratt`) for expression parsing on top of the token stream
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
3. Open the Developer PowerShell shortcut that comes with Build Tools, `clang-cl` and `lib.exe` both need to be on `PATH`, and a plain PowerShell or cmd window usually will not have either.
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
   If `meson setup` cannot find Clang, check `clang.exe`/`clang-cl.exe` actually resolve in your shell, and that `build-cfg/windows-clang.ini` matches your install.

## Building on Linux

**Arch / Manjaro**
```bash
sudo pacman -S clang meson ninja
```

**Ubuntu / Debian / Linux Mint**
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

## Quick start

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

## Rule tables

Build a table of literal rules with `NewTokenList` and `AddToken`, or regex rules with `AddRegexToken`. Bind them to your own enum, or skip the enum entirely with `zenex::NO_ENUM_TABLE` and use plain numeric identifiers.

```cpp
enum class TokenEnum : uint8_t { VAR = 0, IF = 1, LET = 2 };

// enum backed, literal and regex rules mixed
auto tokens = zenex::NewTokenList<TokenEnum>({
    zenex::AddToken(TokenEnum::VAR, "var", "VAR"),
    zenex::AddToken(TokenEnum::IF, "if", "IF"),
    zenex::AddRegexToken(TokenEnum::LET, R"([Ll]et)", "LET")
});

// numeric, mixed with an existing enum via TokenCast
auto more = zenex::NewTokenList<zenex::NO_ENUM_TABLE>({
    zenex::AddToken(zenex::TokenCast(TokenEnum::LET), "let", "LET"),
    zenex::AddToken(zenex::NO_ENUM_TABLE{100}, "for", "FOR")
});
```

Note that a bare integer literal like `100` needs an explicit cast to whatever numeric type your table uses, `int` and `uint8_t` are not the same type as far as template deduction is concerned, so wrap it as shown above rather than passing it raw.

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

## Binding your own identifiers to built in kinds

Tokens from the built in categories (`Identifier`, `Number`, `String`, `Char`, `Whitespace`, `Skipped`, `Fallback`, `EndOfFile`) carry `enumeration = 0` by default, since they did not come from your rule table. Bind a value if you need to tell them apart in your own enum space:

```cpp
lexer->BindTokenKind(zenex::TokenKind::Number, zenex::TokenCast(TokenEnum::NUMBER_LIT));
lexer->BindTokenKind(zenex::TokenKind::String, zenex::TokenCast(TokenEnum::STRING_LIT));
```

`FindMapping` reads a binding back, `0` if nothing was bound.

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

There are two kinds of errors, and they work differently because of when they happen.

**Construction time errors** are things like `lexopt`'s `llenient`/`lstrict` conflict, or a duplicate rule face caught by `lstrict`. These happen before a `Lexer` exists, so there is nothing to hook a callback onto yet, they always throw a `zenex::LexException`:

```cpp
try {
    auto lexer = zenex::CreateLexer(tokens, zenex::lexopt{ zenex::lstrict });
} catch (const zenex::LexException& e) {
    std::cerr << e.error.message << '\n';
}
```

**Runtime errors** happen inside `TokeniseInput`, an unterminated char literal, a char literal with more than one character, or a character that matches nothing at all. These go through one of three paths depending on what you set up:

- `llenient` set: the lexer recovers silently, no callback, no exception.
- A handler registered with `OnError`: your callback runs, then the lexer recovers the same way `llenient` would.
- Neither: `TokeniseInput` throws a `zenex::LexException`.

```cpp
lexer->OnError([](const zenex::LexError& err) {
    std::cerr << "[" << err.line << ":" << err.column << "] " << err.message << '\n';
});

auto result = lexer->TokeniseInput(source); // recovers instead of throwing now
```

`LexError` also carries a `type` field (`zenex::LexErrorType`) if you want to branch on the kind of problem instead of parsing the message string:

```cpp
lexer->OnError([](const zenex::LexError& err) {
    switch (err.type) {
        case zenex::LexErrorType::UnexpectedCharacter:      /* ... */ break;
        case zenex::LexErrorType::UnterminatedCharLiteral:  /* ... */ break;
        case zenex::LexErrorType::InvalidCharLiteralLength: /* ... */ break;
        default: break;
    }
});
```

Registering a handler and setting `llenient` at the same time is fine, `llenient` just means the handler never actually fires, since nothing reaches the error path in the first place.

## Parsing: Pratt, on top of the token stream

zenex ships a Pratt parser (`zenex::Pratt`, `source/pratt/parser.cpp`) for the layer above lexing. Each token gets a prefix and/or infix rule plus a binding power, and precedence falls out of comparing binding powers as the parser recurses. It is the standard approach for expression heavy grammars, adding an operator is one table entry, not a new grammar rule, and it slots in cleanly under a hand written recursive descent parser for statements. If you are building a language, this is usually the right tool for expressions. If you are working on a data format, you probably will not need it.

## API reference

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
| `Lexer::BindTokenKind(kind, value)` | Assigns a numeric id to a built in `TokenKind`. |
| `Lexer::FindMapping(kind)` | Reads back a bound id, `0` if unbound. |
| `lexopt{ ... }` | Behaviour flags, see table above. |
| `lskip{ regex{...}, ... }` | Discard or expose patterns. |
| `regex{ pattern }` | Wraps one pattern string. |
| `TokenKind` | Category enum for a token. |
| `LexerToken` / `LexerTokens` | A token, and a vector of them. |
| `LexError` / `LexErrorType` | Runtime error payload and category. |
| `LexException` | Thrown when there is no handler and the lexer is not lenient. |

## License
MIT
