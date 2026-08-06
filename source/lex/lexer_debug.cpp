#include <zenex/internal/debug.h>
#include <zenex/lex/lexer.h>
#include <iostream>

namespace zenex {
    ZDEBUG_METHOD detail::LexerImpl::__ZDebugPrint() {
#if ZDEBUG_ENABLED
        for (const auto& token : this->token_list) {
            std::cout << "Token: "
                      << static_cast<int>(token.enumeration)
                      << " Face: "
                      << token.face
                      << " Text: "
                      << token.as_text
                      << '\n';
        }

        std::cout << "Options:\n"
                  << "  fast:             " << std::boolalpha << this->opt.fast << '\n'
                  << "  keep_whitespace:  " << this->opt.keep_whitespace << '\n'
                  << "  lenient:          " << this->opt.lenient << '\n'
                  << "  strict:           " << this->opt.strict << '\n'
                  << "  case_insensitive: " << this->opt.case_insensitive << '\n'
                  << "  skip_patterns (" << this->opt.skip_patterns.size() << "):\n";

        for (const auto& pattern : this->opt.skip_patterns) {
            std::cout << "    - " << pattern.pattern << '\n';
        }
#endif
    }
}
