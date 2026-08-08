#pragma once

#include <zenex/lex/token_api.h>
#include <zenex/pratt/errors.h>

namespace zenex {
    class TokenCursor {
    public:
        /*
            @description: constructs a cursor over an existing lexed token stream
            @returns -> zenex::pratt::TokenCursor
        */
        explicit TokenCursor(const zenex::LexerTokens& tokens) : tokens(tokens) {}

        /*
            @description: returns the current token without consuming it
            @returns -> const zenex::LexerToken&
        */
        const zenex::LexerToken& Peek() const {
            return tokens[pos < tokens.size() ? pos : tokens.size() - 1];
        }

        /*
            @description: returns the most recently consumed token
            @returns -> const zenex::LexerToken&
        */
        const zenex::LexerToken& Current() const {
            return tokens[pos > 0 ? pos - 1 : 0];
        }

        /*
            @description: consumes and returns the current token, advancing the cursor
            @returns -> const zenex::LexerToken&
        */
        const zenex::LexerToken& Advance() {
            const auto& tok = Peek();
            if (pos < tokens.size()) ++pos;
            return tok;
        }

        /*
            @description: checks whether the cursor has reached the trailing EndOfFile token
            @returns -> bool
        */
        bool AtEnd() const {
            return Peek().kind == zenex::TokenKind::EndOfFile;
        }

        /*
            @description: checks whether the current token matches a given enumeration, without consuming it
            @returns -> bool
        */
        bool Check(uint8_t token_enum) const {
            return Peek().enumeration == token_enum && Peek().kind != zenex::TokenKind::EndOfFile;
        }

        /*
            @description: consumes the current token if it matches the given enumeration
            @returns -> bool
        */
        bool Match(uint8_t token_enum) {
            if (!Check(token_enum)) return false;
            Advance();
            return true;
        }

        /*
            @description: consumes a specific expected token, or throws a ParseException if it does not match
            @returns -> const zenex::LexerToken&
        */
        const zenex::LexerToken& Expect(uint8_t token_enum, const std::string& what) {
            if (!Check(token_enum)) {
                const auto& got = Peek();
                throw ParseException({
                    ParseErrorType::UnexpectedToken,
                    "zenex: expected " + what + " but got '" + got.face + "'",
                    got.line, got.column
                });
            }
            return Advance();
        }

    private:
        const zenex::LexerTokens& tokens;
        size_t pos = 0;
    };
}
