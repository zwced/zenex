#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <type_traits>

namespace zenex {
    enum class TokenKind : uint8_t {
        UserDefined,    /* matched one of the caller's own token rules */
        Whitespace,     /* kept due to lkeep_whitespace */
        Skipped,        /* skipped via lskip, but only appears as a token if you ever expose it */
        Identifier,     /* generic [a-zA-Z_][a-zA-Z0-9_]* run, when no literal rule matched */
        Number,         /* generic integer or float literal, e.g. 32 or 3.14 */
        String,         /* quoted string literal, single or double quotes, quotes included in face */
        Char,           /* single quoted character / unicode character */
        Fallback,       /* synthesized due to llenient on unknown input */
        EndOfFile,      /* sentinel end-of-input token */
    };

    struct LexerToken {
    public:
        TokenKind kind;               /* what category of token this is — always meaningful */
        uint8_t enumeration;          /* caller's TokenEnum value — ONLY valid when kind == UserDefined */
        std::string face;             /* exact character/sequence matched in the source */
        std::string as_text;          /* printable/canonical version of the token */
        uint32_t line;                /* the exact line the token appears on (1-based) */
        uint32_t column;              /* the exact column the token appears in the line (1-based) */
        uint32_t start_offset;        /* byte offset into source where the token begins */
        uint32_t end_offset;          /* byte offset into source where the token ends */
    };

    using LexerTokens = std::vector<LexerToken>;

    template <typename TokenEnum>
    struct TokenEntry {
    public:
        TokenEnum enumeration;
        std::string face;           /* exact character/sequence to find in a source */
        std::string as_text;        /* the printable version of the token */
        bool is_regex;              /* false = exact literal match, true = face is a regex pattern */
    };

    template <typename TokenEnum>
    using TokenList = std::vector<TokenEntry<TokenEnum>>;

    /*
        @description: method to make a token list
        @returns -> zenex::TokenList<YOUR_ENUM>
    */

    template <typename T>
    TokenList<T> NewTokenList(std::initializer_list<TokenEntry<T>> entries) {
        return TokenList<T>(entries);
    }

    /*
        @description: method to add a token to a token list
        @returns -> zenex::TokenEntry<YOUR_ENUM>
    */

    template <typename TokenEnum>
    TokenEntry<TokenEnum> AddToken(TokenEnum enumeration, std::string face, std::string as_text) {
        return { enumeration, face, as_text, false };
    }

    template <typename TokenEnum>
    TokenEntry<TokenEnum> AddRegexToken(TokenEnum enumeration, std::string pattern, std::string as_text) {
        return { enumeration, pattern, as_text, true };
    }

    /*
        @description: turns ambiguous token/integer type to a measured integer type
        @returns -> uint8_t
    */

    template <typename TokenEnum>
    constexpr uint8_t TokenCast(TokenEnum castee) {
        return static_cast<uint8_t>(castee);
    }

    using NO_ENUM_TABLE = uint8_t;
}
