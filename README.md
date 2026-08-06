# zenex

Writing a lexer is one of those tasks that feels like it should take an afternoon and somehow eats a week. You start with a simple switch statement, then you need comment handling, then whitespace gets complicated, then someone wants string escapes, and by the time it actually works you have lost interest in the compiler you were trying to build in the first place.

zenex is a lexer and parser library for C++20, and it treats those two jobs differently on purpose. Lexers are, relatively speaking, the easy half: a fixed set of rules, a scan loop, some bookkeeping. zenex handles that half almost entirely for you, define your tokens, hand it a handful of options, and get back a fully positioned token stream with identifiers, numbers, and strings already recognised. Parsers are where things actually get hard, where a language's real personality shows up, so that is where zenex gets out of your way and gives you a Pratt parser module to build on rather than trying to guess your grammar for you. The intent is that the part every project has to build but nobody wants to build twice gets handled fast, so the hours you have go into IR generation, optimisation passes, codegen, or an assembler backend, the parts of a language toolchain that are actually worth spending time on. The same design travels well outside of language work too. If you are building an object storage format like JSON, YAML, or TOML, zenex gives you the tokeniser layer without asking you to design a grammar around it.

## Features

- Literal keyword and symbol matching with longest match resolution
- Regex based token matching, both for your own token rules and for skip patterns such as comments
- Automatic identifier, number, and quoted string detection out of the box
- Configurable whitespace handling, discard or emit as tokens
- Lenient recovery mode or strict table validation, your choice
- Case insensitive matching applied consistently across the whole pipeline
- Line, column, and byte offset tracking on every token
- Custom numeric identifiers bindable to built in token kinds, so identifiers, numbers, strings, and fallbacks can carry your own enum values too
- Enum backed or plain numeric token identifiers
- Hookable error reporting, catch lexer and parser errors inside your own code and print, log, or recover from them however you want
- A Pratt parser module (`zenex::Pratt`) for expression parsing on top of the token stream
- Zero external dependencies beyond the C++ standard library

## Requirements

- Clang, on both Windows and Linux. The build refuses to configure with any other compiler, so behaviour stays identical across platforms.
- [Meson](https://mesonbuild.com/)
- [Ninja](https://ninja-build.org/)

## Building on Windows

### Option A: Visual Studio Build Tools (recommended)

1. Install Python (needed for Meson and pip), then:
   ```powershell
   pip install meson ninja
   ```
2. Install Visual Studio Build Tools with the "C++ Clang tools for Windows" component. This provides `clang-cl`. You can also grab it from the command line with winget:
   ```powershell
   winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Component.VC.Llvm.Clang --add Microsoft.VisualStudio.Workload.VCTools"
   ```
3. Open a shell where `clang-cl` is on `PATH`. The Developer PowerShell shortcut installed alongside Build Tools works well.
4. Configure, build, run:
   ```powershell
   meson setup build --native-file build-cfg/windows-clang.ini
   meson compile -C build
   .\build\ztest.exe
   ```

### Option B: Standalone LLVM, no Visual Studio install

1. Install [LLVM for Windows](https://github.com/llvm/llvm-project/releases), or via winget:
   ```powershell
   winget install LLVM.LLVM
   ```
   Make sure the LLVM `bin` directory is added to `PATH` during install, or add it manually afterward.
2. Install Python, Meson, and Ninja:
   ```powershell
   pip install meson ninja
   ```
3. Configure, build, run, same as above:
   ```powershell
   meson setup build --native-file build-cfg/windows-clang.ini
   meson compile -C build
   .\build\ztest.exe
   ```
   If `meson setup` cannot find Clang, confirm `clang.exe` or `clang-cl.exe` resolves in your shell and double check `build-cfg/windows-clang.ini` points at the right binary name for your install.

## Building on Linux

Install Clang, Meson, and Ninja for your distribution, then configure with the Linux native file.

**Arch / Manjaro**
```bash
sudo pacman -S clang meson ninja
```

**Ubuntu**
```bash
sudo apt update
sudo apt install clang meson ninja-build
```

**Debian**
```bash
sudo apt update
sudo apt install clang meson ninja-build
```

**Linux Mint**
```bash
sudo apt update
sudo apt install clang meson ninja-build
```

**Fedora**
```bash
sudo dnf install clang meson ninja-build
```

Then, from the project root, on any of the above:
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
            zenex::regex { R"(/\*.*?\*/)" }
        },
        zenex::lkeep_whitespace,
        zenex::llenient,
    });

    for (const auto& token : lexer->TokeniseInput("var x = 32;"))
        std::cout << token.as_text << ": " << token.face << '\n';
}
```

## Rule tables

Build a table of exact literal rules with `NewTokenList` and `AddToken`. Bind them to your own enum, or skip the enum entirely with `zenex::NO_ENUM_TABLE` and use plain numeric identifiers.

```cpp
enum class TokenEnum : uint8_t { VAR = 0, IF = 1, LET = 2 };

// enum backed
auto tokens = zenex::NewTokenList<TokenEnum>({
    zenex::AddToken(TokenEnum::VAR, "var", "VAR"),
    zenex::AddToken(TokenEnum::IF, "if", "IF")
});

// numeric, mixed with an existing enum via TokenCast
auto more = zenex::NewTokenList<zenex::NO_ENUM_TABLE>({
    zenex::AddToken(zenex::TokenCast(TokenEnum::LET), "let", "LET"),
    zenex::AddToken(zenex::TokenCast(1), "for", "FOR")
});
```

## Lexer options (`zenex::lexopt`)

Options are passed as a flat, freely ordered list.

| Option | Effect |
| --- | --- |
| `lfast` | Enables internal scan optimisations. Line, column, and offset tracking stay fully accurate, nothing is disabled. |
| `lkeep_whitespace` | Emits whitespace runs as tokens instead of discarding them. |
| `llenient` | Emits a single character `Fallback` token for unmatched input instead of stopping. |
| `lstrict` | Validates the rule table at construction time and reports an error if two rules share the same literal face. Mutually exclusive with `llenient`. |
| `lcase_insensitive` | Case insensitive matching across literals, skip patterns, and the strict table check. |
| `lskip { regex{...}, ... }` | A list of regex patterns to silently discard, typically comments. |

Passing both `llenient` and `lstrict` reports an error at construction time, since they describe opposite handling of unmatched input.

## What the scanner does automatically

Beyond your literal rules, every lexer handles these without any setup:

| Input | Becomes |
| --- | --- |
| `'...'` or `"..."`, backslash escapes respected | `TokenKind::String`, quotes included in `face` |
| A run of digits, with an optional single `.` | `TokenKind::Number` |
| A run of letters, digits, and underscores starting with a letter or underscore | `TokenKind::Identifier` |
| Anything matching an `lskip` pattern | discarded entirely |
| Whitespace | discarded, or kept as `TokenKind::Whitespace` under `lkeep_whitespace` |
| Anything else, under `llenient` | `TokenKind::Fallback`, one character at a time |
| End of input | a trailing `TokenKind::EndOfFile` token |

## Binding your own identifiers to built in kinds

By default, tokens produced by the built in categories (`Identifier`, `Number`, `String`, `Whitespace`, `Fallback`, `EndOfFile`) carry `enumeration = 0`, since they did not come from your rule table. If your enum space needs to distinguish them, bind a value with `BindTokenKind`:

```cpp
lexer->BindTokenKind(zenex::TokenKind::Number, zenex::TokenCast(TokenEnum::NUMBER_LIT));
lexer->BindTokenKind(zenex::TokenKind::String, zenex::TokenCast(TokenEnum::STRING_LIT));
```

`FindMapping` reads a binding back, returning `0` for anything unbound.

## Token kinds and the token structure

```cpp
enum class TokenKind : uint8_t {
    UserDefined, Whitespace, Skipped, Identifier, Number, String, Fallback, EndOfFile
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

By default, zenex reports problems like a strict table violation or unmatched input under non-lenient mode as errors from within the library. Rather than forcing a fixed message format or unwind strategy on you, hook your own handler and decide what happens, log to a file, format for your own diagnostics UI, throw your own exception type, or anything else:

```cpp
lexer->OnError([](const zenex::LexError& err) {
    std::cerr << "[" << err.line << ":" << err.column << "] " << err.message << '\n';
});
```

If no handler is registered, zenex falls back to its own default reporting behaviour. Adjust the snippet above to match whatever the hook's actual name and signature end up being in your implementation, this section documents the intended shape rather than a locked in API.

## Parsing: Pratt, on top of the token stream

zenex ships a Pratt parser (`zenex::Pratt`, `source/pratt/parser.cpp`) for the layer above lexing. Pratt parsing, also called top down operator precedence parsing, associates each token with a prefix and/or infix parsing rule plus a binding power, and resolves precedence and associativity by comparing binding powers as it recurses. It is the standard choice for expression heavy grammars: adding a new operator is one table entry, not a new grammar production, and it composes cleanly with a hand written recursive descent parser for statements and declarations sitting above it. If you are building a language, this is almost always the right tool for expressions; if you are building a config or data format, you likely will not need it at all and can work directly off the token stream.

## API reference

| Symbol | Description |
| --- | --- |
| `NewTokenList<TokenEnum>(entries)` | Builds a token table. |
| `AddToken(enumeration, face, as_text)` | Builds one rule entry. |
| `TokenCast(value)` | Converts an enum value to its numeric form. |
| `NO_ENUM_TABLE` | Use in place of an enum type for plain numeric ids. |
| `CreateLexer(tokens, opt)` | Builds a `zenex::Lexer`. |
| `Lexer` | `std::shared_ptr` to the lexer instance, copyable and shareable. |
| `Lexer::TokeniseInput(source)` | Scans source text, returns `LexerTokens`. |
| `Lexer::IsToken(input)` | True if `input` exactly matches a rule's face. Independent of `TokeniseInput`. |
| `Lexer::BindTokenKind(kind, value)` | Assigns a numeric id to a built in `TokenKind`. |
| `Lexer::FindMapping(kind)` | Reads back a bound id, `0` if unbound. |
| `lexopt{ ... }` | Behaviour flags, see table above. |
| `lskip{ regex{...}, ... }` | Discard patterns. |
| `regex{ pattern }` | Wraps one pattern string. |
| `TokenKind` | Category enum for a token. |
| `LexerToken` / `LexerTokens` | A token, and a vector of them. |

## License
MIT
