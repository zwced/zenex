#include <zenex/lex/token_api.h>
#include <zenex/lex/lexer.h>
#include <zenex/lex/lexopt.h>
#include <zenex/regex.h>
#include <iostream>
#include <chrono>
#include <sstream>
#include <random>

enum class TokenEnum : uint8_t {
    VAR, LET, IF, FOR,
    EQUAL, DOUBLE_EQUAL, SEMICOLON,
    LPAREN, RPAREN, LBRACE, RBRACE,
    PLUS, DOUBLE_PLUS, MINUS, DOUBLE_MINUS, STAR, DOUBLE_STAR,
    HEX_NUMBER, FLOAT_LITERAL, TYPE_NAME,
    NUMBER, STRING, IDENTIFIER, END_OF_FILE,
};

/* enum backed table, mixing literal rules and regex rules in the same table */
static auto tokens = zenex::NewTokenList<TokenEnum>({
    zenex::AddToken(TokenEnum::VAR, "var", "VAR"),
    zenex::AddToken(TokenEnum::LET, "let", "LET"),
    zenex::AddToken(TokenEnum::IF, "if", "IF"),
    zenex::AddToken(TokenEnum::FOR, "for", "FOR"),
    zenex::AddToken(TokenEnum::DOUBLE_EQUAL, "==", "DOUBLE_EQUAL"),
    zenex::AddToken(TokenEnum::EQUAL, "=", "EQUAL"),
    zenex::AddToken(TokenEnum::SEMICOLON, ";", "SEMICOLON"),
    zenex::AddToken(TokenEnum::LPAREN, "(", "LEFT_PARENTHESIS"),
    zenex::AddToken(TokenEnum::RPAREN, ")", "RIGHT_PARENTHESIS"),
    zenex::AddToken(TokenEnum::LBRACE, "{", "LEFT_BRACE"),
    zenex::AddToken(TokenEnum::RBRACE, "}", "RIGHT_BRACE"),
    zenex::AddToken(TokenEnum::DOUBLE_PLUS, "++", "DOUBLE_PLUS"),
    zenex::AddToken(TokenEnum::PLUS, "+", "PLUS"),
    zenex::AddToken(TokenEnum::DOUBLE_MINUS, "--", "DOUBLE_MINUS"),
    zenex::AddToken(TokenEnum::MINUS, "-", "MINUS"),
    zenex::AddToken(TokenEnum::DOUBLE_STAR, "**", "DOUBLE_STAR"),
    zenex::AddToken(TokenEnum::STAR, "*", "STAR"),
    zenex::AddRegexToken(TokenEnum::HEX_NUMBER, R"(0x[0-9a-fA-F]+)", "HEX_NUMBER"),
    zenex::AddRegexToken(TokenEnum::FLOAT_LITERAL, R"([0-9]+\.[0-9]+[eE][+-]?[0-9]+)", "FLOAT_LITERAL"),
    zenex::AddRegexToken(TokenEnum::TYPE_NAME, R"([A-Z][a-zA-Z0-9_]*)", "TYPE_NAME"),
});

const char* KindName(zenex::TokenKind k) {
    switch (k) {
        case zenex::TokenKind::UserDefined: return "UserDefined";
        case zenex::TokenKind::Whitespace:  return "Whitespace";
        case zenex::TokenKind::Skipped:     return "Skipped";
        case zenex::TokenKind::Identifier:  return "Identifier";
        case zenex::TokenKind::Number:      return "Number";
        case zenex::TokenKind::String:      return "String";
        case zenex::TokenKind::Fallback:    return "Fallback";
        case zenex::TokenKind::EndOfFile:   return "EndOfFile";
    }
    return "?";
}

std::string Escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

void PrintTokens(const zenex::LexerTokens& toks) {
    for (const auto& tok : toks) {
        std::cout << "  [" << KindName(tok.kind) << "] "
                  << "enum=" << static_cast<int>(tok.enumeration) << " "
                  << "face=\"" << Escape(tok.face) << "\" "
                  << "as_text=\"" << tok.as_text << "\" "
                  << "line=" << tok.line << " col=" << tok.column << " "
                  << "offset=[" << tok.start_offset << ", " << tok.end_offset << "]"
                  << '\n';
    }
}

void RunCase(zenex::Lexer& l, const std::string& name, const std::string& source) {
    std::cout << "\n " << name << " \n";
    std::cout << "input: \"" << Escape(source) << "\"\n";
    auto result = l->TokeniseInput(source);
    PrintTokens(result);
    std::cout << "  (" << result.size() << " tokens)\n";
}

zenex::Lexer MakeStandardLexer() {
    /* dotall equivalent via [\s\S], since std::regex '.' never matches newlines under ECMAScript */
    zenex::Lexer l = zenex::CreateLexer(tokens, zenex::lexopt{
        zenex::lskip {
            zenex::regex {R"(//[^\n]*)"},
            zenex::regex {R"(/\*[\s\S]*?\*/)"}
        },
        zenex::llenient,
    });
    l->BindTokenKind(zenex::TokenKind::Number, zenex::TokenCast(TokenEnum::NUMBER));
    l->BindTokenKind(zenex::TokenKind::String, zenex::TokenCast(TokenEnum::STRING));
    l->BindTokenKind(zenex::TokenKind::Identifier, zenex::TokenCast(TokenEnum::IDENTIFIER));
    l->BindTokenKind(zenex::TokenKind::EndOfFile, zenex::TokenCast(TokenEnum::END_OF_FILE));
    return l;
}

int main() {
    zenex::Lexer l = MakeStandardLexer();

    /* confirm BindTokenKind round trips through FindMapping */
    std::cout << "\n mapping round trip \n";
    std::cout << "  Number bound to enum " << static_cast<int>(l->FindMapping(zenex::TokenKind::Number)) << "\n";
    std::cout << "  String bound to enum " << static_cast<int>(l->FindMapping(zenex::TokenKind::String)) << "\n";
    std::cout << "  Skipped unbound, returns " << static_cast<int>(l->FindMapping(zenex::TokenKind::Skipped)) << "\n";

    /* IsToken, independent of TokeniseInput */
    std::cout << "\n IsToken checks \n";
    std::cout << "  IsToken(\"var\")  = " << std::boolalpha << l->IsToken("var") << "\n";
    std::cout << "  IsToken(\"for\")  = " << l->IsToken("for") << "\n";
    std::cout << "  IsToken(\"nope\") = " << l->IsToken("nope") << "\n";

    /* debug dump of the lexer's own configuration */
    l->__ZDebugPrint();

    /* empty and whitespace-only inputs */
    RunCase(l, "empty input", "");
    RunCase(l, "whitespace only", "   \n\t  ");
    RunCase(l, "mixed crlf and lf", "a\r\nb\nc\r\n");

    /* comments, including the corrected multiline pattern */
    RunCase(l, "comment only, line", "// nothing but a comment\n");
    RunCase(l, "comment only, block", "/* nothing here either */");
    RunCase(l, "multiline block comment, corrected pattern", "/* line one\nline two\nline three */var x;");
    RunCase(l, "comment glued to code, no space", "//comment\nvar x;");
    RunCase(l, "comment marker inside a string", R"("this has // and /* inside it")");

    /* operators, longest match */
    RunCase(l, "operator longest match: +++", "+++");
    RunCase(l, "operators glued with no whitespace", "x+y-z*w==q");
    RunCase(l, "deeply nested parens", "((((((()))))))");

    /* regex tokens competing against literals and generics */
    RunCase(l, "hex number via regex token", "0xFF 0x1a2B 0x");
    RunCase(l, "float via regex token vs generic number", "1.5e10 1.5 1e10");
    RunCase(l, "capitalized identifier via regex token", "Foo bar Baz qux");
    RunCase(l, "regex token and literal tie on length", "for Foobar");
    RunCase(l, "regex rule matching nothing, falls through", "0xZZ");

    /* numbers */
    RunCase(l, "integers", "0 42 1000000");
    RunCase(l, "multiple dots in a row", "1.2.3");
    RunCase(l, "leading dot, no digit before", ".5");
    RunCase(l, "number glued to identifier", "3x");

    /* strings */
    RunCase(l, "double quoted string", R"("hello world")");
    RunCase(l, "single quoted string", R"('hello world')");
    RunCase(l, "string with escaped quote", R"("she said \"hi\"")");
    RunCase(l, "unterminated string", R"("this never closes)");
    RunCase(l, "back to back strings", R"("a""b")");

    /* keyword vs identifier boundary, known bug */
    RunCase(l, "exact keyword", "var");
    RunCase(l, "keyword-prefixed identifier, expect a bug here", "variable");
    RunCase(l, "keyword-suffixed identifier", "myvar");

    /* fallback handling */
    RunCase(l, "unrecognised symbols glued together", "@#$~`");

    /* combined program using every feature at once */
    RunCase(l, "combined program", R"(
        /* multi
           line
           comment */
        var x = 0xFF;
        let pi = 3.14e2;
        Type name = "zenex";
        if (x == 10) {
            x++;
        }
    )");

    /* whitespace kept */
    {
        zenex::Lexer lw = zenex::CreateLexer(tokens, zenex::lexopt{
            zenex::lkeep_whitespace,
            zenex::llenient,
        });
        RunCase(lw, "whitespace kept", "var  x\t=\n1;");
    }

    /* case insensitive, literals and regex tokens both */
    {
        zenex::Lexer lci = zenex::CreateLexer(tokens, zenex::lexopt{
            zenex::lcase_insensitive,
            zenex::llenient,
        });
        RunCase(lci, "case insensitive keywords", "VAR X = 1; Let Y = 2; IF");
        RunCase(lci, "case insensitive hex regex", "0XABCDEF");
    }

    /* lfast, confirm behaviour explicitly */
    {
        zenex::Lexer lf = zenex::CreateLexer(tokens, zenex::lexopt{
            zenex::lfast,
            zenex::llenient,
        });
        RunCase(lf, "lfast, line and column tracking", "var\nx\n=\n1;");
    }

    /* NO_ENUM_TABLE, plain numeric identifiers, mixed with TokenCast from the enum above */
    {
        static auto numeric_tokens = zenex::NewTokenList<zenex::NO_ENUM_TABLE>({
            zenex::AddToken(zenex::TokenCast(TokenEnum::VAR), "var", "VAR"),
            zenex::AddToken(zenex::TokenCast(TokenEnum::LET), "let", "LET"),
            zenex::AddToken(zenex::TokenCast(100), "print", "PRINT"),
            zenex::AddRegexToken(zenex::TokenCast(101), R"([0-9]+px)", "PIXEL_LITERAL"),
        });

        zenex::Lexer ln = zenex::CreateLexer(numeric_tokens, zenex::lexopt{
            zenex::llenient,
        });

        RunCase(ln, "NO_ENUM_TABLE, mixed cast and raw ids", "var x; print 12px;");
    }

    /* volume stress, structured input, literal-heavy table */
    {
        std::ostringstream big;
        for (int i = 0; i < 20000; ++i) {
            big << "var x" << i << " = " << i << ";\n";
            if (i % 500 == 0) big << "/* checkpoint */\n";
        }
        std::string source = big.str();

        auto start = std::chrono::high_resolution_clock::now();
        auto result = l->TokeniseInput(source);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << "\n volume stress, structured, literal + regex table (" << source.size() << " bytes) \n";
        std::cout << "  produced " << result.size() << " tokens in " << ms << " ms\n";
    }

    /* volume stress, adversarial noise */
    {
        std::ostringstream noisy;
        std::mt19937 rng(1234);
        std::uniform_int_distribution<int> dist(33, 126);
        for (int i = 0; i < 200000; ++i) noisy << static_cast<char>(dist(rng));
        std::string source = noisy.str();

        auto start = std::chrono::high_resolution_clock::now();
        auto result = l->TokeniseInput(source);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << "\n volume stress, adversarial noise, regex table in play (" << source.size() << " bytes) \n";
        std::cout << "  produced " << result.size() << " tokens in " << ms << " ms\n";
    }

    std::cout << "\ndone.\n";
    return 0;
}
