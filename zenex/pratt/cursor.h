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
            @description: returns a token ahead of the current position without consuming it
            @returns -> const zenex::LexerToken&
        */
        const zenex::LexerToken& Peek(size_t offset = 0) const {
            size_t target = this->pos + offset;
            return this->tokens[target < this->tokens.size() ? target : this->tokens.size() - 1];
        }

        /*
            @description: returns the most recently consumed token
            @returns -> const zenex::LexerToken&
        */
        const zenex::LexerToken& Current() const {
            return this->tokens[this->pos > 0 ? this->pos - 1 : 0];
        }

        /*
            @description: consumes and returns the current token, advancing the cursor
            @returns -> const zenex::LexerToken&
        */
        const zenex::LexerToken& Advance() {
            const auto& tok = this->Peek();
            if (this->pos < this->tokens.size()) {
                ++this->pos;
            }
            return tok;
        }

        /*
            @description: checks whether the cursor has reached the trailing EndOfFile token
            @returns -> bool
        */
        bool AtEnd() const {
            return this->Peek().kind == zenex::TokenKind::EndOfFile;
        }

        /*
            @description: checks whether the current token matches a given enumeration, without consuming it
            @returns -> bool
        */
        bool Check(uint8_t token_enum) const {
            return this->Peek().enumeration == token_enum && this->Peek().kind != zenex::TokenKind::EndOfFile;
        }

        /*
            @description: consumes the current token if it matches the given enumeration
            @returns -> bool
        */
        bool Match(uint8_t token_enum) {
            if (!this->Check(token_enum)) return false;
            this->Advance();
            return true;
        }

        /*
            @description: consumes a specific expected token, or throws a ParseException if it does not match
            @returns -> const zenex::LexerToken&
        */
        const zenex::LexerToken& Expect(uint8_t token_enum, const std::string& what) {
            if (!this->Check(token_enum)) {
                const auto& got = this->Peek();
                throw ParseException({
                    ParseErrorType::UnexpectedToken,
                    "expected " + what + " but got '" + got.face + "'",
                    got.line, got.column
                });
            }
            return this->Advance();
        }

        /*
            @description: like Expect, but for statement terminators; never names the token it found (which may be on an unrelated line and have nothing to do with the actual problem)
            @returns -> const zenex::LexerToken&
        */
        const zenex::LexerToken& ExpectSilent(uint8_t token_enum, const std::string& message) {
            if (!this->Check(token_enum)) {
                const auto& last = this->Current();
                throw ParseException({
                    ParseErrorType::UnexpectedToken,
                    message,
                    last.line, last.column + static_cast<uint32_t>(last.face.length())
                });
            }
            return this->Advance();
        }

        /*
            @description: returns the current index of the cursor for backtracking
            @returns -> size_t
        */
        size_t Position() const {
            return this->pos;
        }

        /*
            @description: sets the cursor to a specific index, primarily used for backtracking after a failed match
            @returns -> void
        */
        void Seek(size_t new_pos) {
            this->pos = new_pos < this->tokens.size() ? new_pos : this->tokens.size() - 1;
        }

    private:
        const zenex::LexerTokens& tokens;
        size_t pos = 0;
    };
}
