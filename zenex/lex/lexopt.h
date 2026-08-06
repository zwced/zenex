#pragma once

#include <zenex/flag.h>
#include <zenex/regex.h>
#include <zenex/lex/errors.h>

#include <vector>
#include <utility>
#include <type_traits>

namespace zenex {
    struct lskip {
        std::vector<regex> patterns;
        lskip(std::initializer_list<regex> pats) : patterns(pats) {}
    };                                              // skips regex patterns

    ZENEX_FLAG(lkeep_skipped)                       // omits skipped tokens as "Skipped" TokenKind
    ZENEX_FLAG(lfast)                               // applies relative optimisations
    ZENEX_FLAG(lkeep_whitespace)                    // emits whitespace as tokens instead of discarding it
    ZENEX_FLAG(llenient)                            // continues on unknown input by creating fallback tokens
    ZENEX_FLAG(lstrict)                             // errors on anything that doesn't match a defined token rule
    ZENEX_FLAG(lcase_insensitive)                   // matches tokens without considering letter case

    struct lexopt {
    public:
        bool fast = false;
        bool keep_whitespace = false;
        bool lenient = false;
        bool strict = false;
        bool case_insensitive = false;
        bool keep_skipped = false;
        std::vector<regex> skip_patterns;

        /* variadic ctor: each brace element is passed as a separate */
        /* constructor argument (NOT an initializer_list), so types can differ */
        template <typename... Opts>
            requires (!std::is_same_v<std::decay_t<Opts>, lexopt> && ...)
        lexopt(Opts&&... opts) {
            (apply(std::forward<Opts>(opts)), ...);
        }
    private:
        void apply(lfast_t)              { this->fast = true; }
        void apply(lkeep_whitespace_t)   { this->keep_whitespace = true; }
        void apply(lcase_insensitive_t)  { this->case_insensitive = true; }
        void apply(lkeep_skipped_t)      { this->keep_skipped = true; }
        void apply(lskip const& s)       { this->skip_patterns = s.patterns; }

        void apply(llenient_t) {
             if (strict) throw LexException({ LexErrorType::ConflictingOptions, "zenex: llenient conflicts with lstrict" });
             lenient = true;
        }

         void apply(lstrict_t) {
             if (lenient) throw LexException({ LexErrorType::ConflictingOptions, "zenex: lstrict conflicts with llenient" });
             strict = true;
        }
    };
}
