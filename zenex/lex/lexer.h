#pragma once

#include <zenex/internal/debug.h>
#include <zenex/lex/token_api.h>
#include <zenex/lex/lexopt.h>

#include <memory>
#include <unordered_map>

namespace zenex {
    namespace detail {
        class LexerImpl {
        public:
            explicit LexerImpl(TokenList<uint8_t> token_list, lexopt opt);
            LexerTokens TokeniseInput(std::string source);

            uint8_t FindMapping(TokenKind kind) const;
            void BindTokenKind(TokenKind kind, uint8_t enumeration);

            bool IsToken(std::string input);

            ZDEBUG_METHOD __ZDebugPrint();

        private:
            std::unordered_map<TokenKind, uint8_t> TokenMappings;

            TokenList<uint8_t> token_list;
            lexopt opt;
        };
    }

    using Lexer = std::shared_ptr<detail::LexerImpl>;

    /*
        @description: method to create a lexer, by default provides a shared pointer to a LexerImpl
        @return: std::shared_ptr<detail::LexerImpl>
    */

    template <typename TokenEnum>
    Lexer CreateLexer(const TokenList<TokenEnum>& prev_list, const lexopt& opt) {
        TokenList<uint8_t> new_list;
        new_list.reserve(prev_list.size());

        for (const auto& token : prev_list) {
            new_list.push_back({
                static_cast<uint8_t>(token.enumeration),
                token.face,
                token.as_text,
                token.is_regex
            });
        }

        return std::make_shared<detail::LexerImpl>(std::move(new_list), std::move(opt));
    }
}
