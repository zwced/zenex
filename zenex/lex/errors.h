#pragma once

#include <string>
#include <functional>
#include <stdexcept>
#include <cstdint>

namespace zenex {
    enum class LexErrorType : uint8_t {
        ConflictingOptions,        /* llenient and lstrict passed together */
        DuplicateTokenFace,        /* lstrict found two rules sharing a face */
        UnterminatedCharLiteral,   /* a ' literal never found its closing quote */
        InvalidCharLiteralLength,  /* a ' literal held zero or more than one character */
        UnexpectedCharacter,       /* matched no rule, whitespace, string, number, or identifier */
    };

    struct LexError {
        LexErrorType type;
        std::string message;
        uint32_t line = 0;
        uint32_t column = 0;
        uint32_t offset = 0;
    };

    class LexException : public std::runtime_error {
    public:
        explicit LexException(LexError err) : std::runtime_error(err.message), error(std::move(err)) {}
        LexError error;
    };

    using LexErrorHandler = std::function<void(const LexError&)>;
}
