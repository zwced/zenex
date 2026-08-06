#include <zenex/lex/token_api.h>
#include <zenex/lex/lexer.h>
#include <iostream>

enum class TokenEnum : uint8_t {
    LET, FN, IF, FOR,

    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,

    EQUAL, DOUBLE_EQUAL,

    I32, STR, CHAR,
    RET, BREAK, CONTINUE,

    COLON, SEMICOLON,

    IDENTIFIER, TYPE_NUMBER, TYPE_STRING, TYPE_CHAR,
    END_OF_FILE,
    SKIPPED
};

int main() {
    auto tokens = zenex::NewTokenList<TokenEnum>({
        zenex::AddToken(TokenEnum::LET, "let", "LET"),
        zenex::AddToken(TokenEnum::FN, "fn", "FUNCTION"),
        zenex::AddToken(TokenEnum::IF, "if", "IF"),
        zenex::AddToken(TokenEnum::FOR, "for", "FOR"),

        zenex::AddToken(TokenEnum::LPAREN, "(", "LEFT_PAREN"),
        zenex::AddToken(TokenEnum::RPAREN, ")", "RIGHT_PAREN"),
        zenex::AddToken(TokenEnum::LBRACE, "{", "LEFT_BRACE"),
        zenex::AddToken(TokenEnum::RBRACE, "}", "RIGHT_BRACE"),
        zenex::AddToken(TokenEnum::LBRACKET, "[", "LEFT_BRACKET"),
        zenex::AddToken(TokenEnum::RBRACKET, "]", "RIGHT_BRACKET"),

        zenex::AddToken(TokenEnum::EQUAL, "=", "EQUAL"),
        zenex::AddToken(TokenEnum::DOUBLE_EQUAL, "==", "DOUBLE_EQUAL"),

        zenex::AddToken(TokenEnum::I32, "i32", "INTEGER_32"),
        zenex::AddToken(TokenEnum::STR, "str", "STR"),
        zenex::AddToken(TokenEnum::CHAR, "char", "CHAT"),

        zenex::AddToken(TokenEnum::RET, "return", "RETURN"),
        zenex::AddToken(TokenEnum::BREAK, "break", "BREAK"),
        zenex::AddToken(TokenEnum::CONTINUE, "continue", "CONTINUE"),

        zenex::AddToken(TokenEnum::COLON, ":", "COLON"),
        zenex::AddToken(TokenEnum::SEMICOLON, ";", "SEMICOLON"),
    });

    zenex::Lexer lexer = zenex::CreateLexer(tokens, zenex::lexopt{
        zenex::lskip {
            zenex::regex { R"(//[^\n]*)" },
            zenex::regex { R"(/\*.*?\*/)" },
            zenex::regex { R"(/\*[\s\S]*?\*/)" }
        },
        zenex::llenient,
        zenex::lkeep_skipped
    });

    lexer->BindTokenKind(zenex::TokenKind::Skipped, zenex::TokenCast(TokenEnum::SKIPPED));
    lexer->BindTokenKind(zenex::TokenKind::Identifier, zenex::TokenCast(TokenEnum::IDENTIFIER));
    lexer->BindTokenKind(zenex::TokenKind::Fallback, zenex::TokenCast(TokenEnum::IDENTIFIER));
    lexer->BindTokenKind(zenex::TokenKind::Number, zenex::TokenCast(TokenEnum::TYPE_NUMBER));
    lexer->BindTokenKind(zenex::TokenKind::String, zenex::TokenCast(TokenEnum::TYPE_STRING));
    lexer->BindTokenKind(zenex::TokenKind::Char, zenex::TokenCast(TokenEnum::TYPE_CHAR));
    lexer->BindTokenKind(zenex::TokenKind::EndOfFile, zenex::TokenCast(TokenEnum::END_OF_FILE));

    zenex::LexerTokens input = lexer->TokeniseInput(R"(
        fn main(argc: i32, argv: str[]) -> i32 {
        /*
        comments
        comment
        comments yes
        */
            let x: i32 = argc;
            let y: str[] = argv;
            let z: str = "hello";
            let w: char = '\n';

            return 0;
        }
    )");

    for (const auto& token : input)
        std::cout << token.as_text << ": " << token.face << '\n';

    for (;;);
}
