#pragma once

#include <string>
#include <functional>
#include <stdexcept>
#include <cstdint>

namespace zenex {
    enum class ParseErrorType : uint8_t {
        NoPrefixRule,        /* a token appeared where an expression must start, but has no prefix rule */
        NoStatementRule,     /* a token started a statement but has no registered statement rule */
        UnexpectedToken,     /* expected a specific token and got something else */
        UnexpectedEndOfInput,
    };

    struct ParseError {
        ParseErrorType type;
        std::string message;
        uint32_t line = 0;
        uint32_t column = 0;
    };

    class ParseException : public std::runtime_error {
    public:
        /*
            @description: wraps a ParseError as a throwable exception
            @returns -> zenex::pratt::ParseException
        */
        explicit ParseException(ParseError err) : std::runtime_error(err.message), error(std::move(err)) {}

        /*
            @description: builds a comprehensive formatted string combining the message and the line/column
            @returns -> std::string
        */
        std::string Format() const {
            return this->error.message + " <line:" + std::to_string(this->error.line) + ", col:" + std::to_string(this->error.column) + ">";
        }

        /*
            @description: returns the line number of the error
            @returns -> int
        */
        int GetLine() const { return this->error.line; }

        /*
            @description: returns the column number of the error
            @returns -> int
        */
        int GetColumn() const { return this->error.column; }

        ParseError error;
    };

    using ParseErrorHandler = std::function<void(const ParseError&)>;
}
