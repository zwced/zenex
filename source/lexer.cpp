#include "zenex/lex/token_api.h"
#include <zenex/lex/lexer.h>
#include <regex>

namespace zenex {
    namespace detail {
        LexerImpl::LexerImpl(TokenList<uint8_t> token_list, lexopt opt) : token_list(std::move(token_list)), opt(std::move(opt)) {
            if (this->opt.strict) {
                auto faces_equal = [ci = this->opt.case_insensitive](const std::string& a, const std::string& b) {
                    if (!ci) return a == b;
                    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                        [](char x, char y) {
                            return std::tolower(static_cast<unsigned char>(x))
                                == std::tolower(static_cast<unsigned char>(y));
                        });
                };

                for (size_t i = 0; i < this->token_list.size(); ++i) {
                    for (size_t j = i + 1; j < this->token_list.size(); ++j) {
                        if (faces_equal(this->token_list[i].face, this->token_list[j].face)) {
                            throw LexException({
                                LexErrorType::DuplicateTokenFace,
                                "lstrict - duplicate token face '" + this->token_list[i].face + "' found in rule table"
                            });
                        }
                    }
                }
            }

            auto flags = std::regex::ECMAScript;
            if (this->opt.case_insensitive) flags |= std::regex::icase;

            this->compiled_skip_patterns.reserve(this->opt.skip_patterns.size());
            for (const auto& pat : this->opt.skip_patterns)
                this->compiled_skip_patterns.emplace_back(pat.pattern, flags);

            this->compiled_rule_patterns.reserve(this->token_list.size());
            for (const auto& entry : this->token_list) {
                if (entry.is_regex)
                    this->compiled_rule_patterns.emplace_back(entry.face, flags);
                else
                    this->compiled_rule_patterns.emplace_back();
            }
        }

        LexerTokens detail::LexerImpl::TokeniseInput(std::string source) {
            LexerTokens result;
            size_t pos = 0;
            uint32_t line = 1;
            uint32_t column = 1;

            auto advance = [&](size_t n) {
                size_t end = pos + n;
                size_t search_from = pos;

                while (true) {
                    size_t nl = source.find('\n', search_from);
                    if (nl == std::string::npos || nl >= end) break;
                    ++line;
                    column = 1;
                    search_from = nl + 1;
                }

                column += static_cast<uint32_t>(end - search_from);
                pos = end;
            };

            auto ci_equal = [&](char a, char b) {
                return this->opt.case_insensitive
                    ? std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b))
                    : a == b;
            };

            auto starts_with = [&](size_t at, const std::string& lit) {
                if (at + lit.size() > source.size()) return false;
                for (size_t i = 0; i < lit.size(); ++i)
                    if (!ci_equal(source[at + i], lit[i])) return false;
                return true;
            };

            auto is_word_char = [](char c) {
                return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
            };

            while (pos < source.size()) {
                /* skip patterns, precompiled, matched by iterator range instead of substr copy */
                bool skipped = false;
                for (size_t k = 0; k < this->compiled_skip_patterns.size(); ++k) {
                    std::smatch m;
                    if (
                        std::regex_search(source.cbegin() + static_cast<std::ptrdiff_t>(pos),
                        source.cend(), m,
                        this->compiled_skip_patterns[k], std::regex_constants::match_continuous
                        ))
                    {
                        size_t start = pos;
                        uint32_t sl = line, sc = column;
                        size_t len = static_cast<size_t>(m.length(0));

                        advance(len);

                        if (this->opt.keep_skipped) {
                            result.push_back({
                                TokenKind::Skipped, this->FindMapping(TokenKind::Skipped),
                                source.substr(start, len), "SKIPPED",
                                sl, sc,
                                static_cast<uint32_t>(start), static_cast<uint32_t>(pos)
                            });
                        }

                        skipped = true;
                        break;
                    }
                }
                if (skipped) continue;

                /* whitespace */
                if (std::isspace(static_cast<unsigned char>(source[pos]))) {
                    size_t start = pos;
                    uint32_t sl = line, sc = column;

                    while (pos < source.size() && std::isspace(static_cast<unsigned char>(source[pos])))
                        advance(1);

                    if (this->opt.keep_whitespace) {
                        result.push_back({
                            TokenKind::Whitespace, this->FindMapping(TokenKind::Whitespace),
                            source.substr(start, pos - start), "WHITESPACE",
                            sl, sc,
                            static_cast<uint32_t>(start), static_cast<uint32_t>(pos)
                        });
                    }
                    continue;
                }

                /* user defined token matching, longest match wins, literal
                   rules require a word boundary if they start with a word
                   character, so "var" cannot swallow the start of "variable" */
                const TokenEntry<uint8_t>* best = nullptr;
                size_t best_len = 0;

                for (size_t i = 0; i < this->token_list.size(); ++i) {
                    const auto& entry = this->token_list[i];
                    size_t len = 0;

                    if (entry.is_regex) {
                        std::smatch m;
                        if (
                            std::regex_search(source.cbegin() + static_cast<std::ptrdiff_t>(pos),
                                source.cend(),
                                m, this->compiled_rule_patterns[i],
                                std::regex_constants::match_continuous
                            ))
                            len = static_cast<size_t>(m.length(0));
                    } else {
                        if (starts_with(pos, entry.face)) {
                            bool starts_word = !entry.face.empty() && is_word_char(entry.face.front());
                            bool boundary_ok = !starts_word ||
                                (pos + entry.face.size() >= source.size()) ||
                                !is_word_char(source[pos + entry.face.size()]);
                            if (boundary_ok)
                                len = entry.face.size();
                        }
                    }

                    if (len > best_len) {
                        best = &entry;
                        best_len = len;
                    }
                }

                if (best) {
                    uint32_t sl = line, sc = column;
                    size_t start = pos;

                    advance(best_len);

                    result.push_back({
                        TokenKind::UserDefined, best->enumeration,
                        source.substr(start, best_len), best->as_text,
                        sl, sc,
                        static_cast<uint32_t>(start), static_cast<uint32_t>(pos)
                    });
                    continue;
                }

                char c = source[pos];

                /* char literal */
                if (c == '\'') {
                    size_t start = pos;
                    uint32_t sl = line, sc = column;
                    size_t i = pos + 1;
                    int decoded_chars = 0;
                    bool unterminated = false;

                    while (i < source.size() && source[i] != '\'') {
                        if (source[i] == '\\' && i + 1 < source.size()) {
                            i += 2;
                        } else {
                            ++i;
                        }
                        ++decoded_chars;

                        if (i < source.size() && source[i - 1] == '\n') {
                            unterminated = true;
                            break;
                        }
                    }

                    if (i >= source.size() || unterminated) {
                        if (this->opt.lenient) {
                            /* fall through to fallback handling below by not consuming */
                        } else if (this->error_handler) {
                            this->error_handler({
                                LexErrorType::UnterminatedCharLiteral,
                                "unterminated char literal",
                                sl, sc, static_cast<uint32_t>(start)
                            });
                        } else {
                            throw LexException({
                                LexErrorType::UnterminatedCharLiteral,
                                "unterminated char literal",
                                sl, sc, static_cast<uint32_t>(start)
                            });
                        }
                    } else {
                        ++i;
                        size_t len = i - start;

                        if (decoded_chars != 1) {
                            if (this->opt.lenient) {
                                /* accept anyway */
                            } else if (this->error_handler) {
                                this->error_handler({
                                    LexErrorType::InvalidCharLiteralLength,
                                    "char literal '" + source.substr(start, len) + "' must contain exactly one character",
                                    sl, sc, static_cast<uint32_t>(start)
                                });
                            } else {
                                throw LexException({
                                    LexErrorType::InvalidCharLiteralLength,
                                    "char literal '" + source.substr(start, len) + "' must contain exactly one character",
                                    sl, sc, static_cast<uint32_t>(start)
                                });
                            }
                        }

                        advance(len);

                        result.push_back({
                            TokenKind::Char, this->FindMapping(TokenKind::Char),
                            source.substr(start, len), "CHAR",
                            sl, sc,
                            static_cast<uint32_t>(start), static_cast<uint32_t>(pos)
                        });

                        continue;
                    }
                }

                /* string literal */
                if (c == '"') {
                    char quote = c;
                    size_t start = pos;
                    uint32_t sl = line, sc = column;
                    size_t i = pos + 1;
                    while (i < source.size() && source[i] != quote) {
                        if (source[i] == '\\' && i + 1 < source.size()) i += 2;
                        else ++i;
                    }
                    if (i < source.size()) ++i;
                    size_t len = i - start;

                    advance(len);

                    result.push_back({
                        TokenKind::String, this->FindMapping(TokenKind::String),
                        source.substr(start, len), "STRING",
                        sl, sc,
                        static_cast<uint32_t>(start), static_cast<uint32_t>(pos)
                    });

                    continue;
                }

                /* number literal */
                if (std::isdigit(static_cast<unsigned char>(c))) {
                    size_t start = pos;
                    uint32_t sl = line, sc = column;
                    size_t i = pos;

                    while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) ++i;

                    if (i < source.size() && source[i] == '.' &&
                        i + 1 < source.size() && std::isdigit(static_cast<unsigned char>(source[i + 1]))) {
                        ++i;
                        while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i]))) ++i;
                    }

                    size_t len = i - start;
                    advance(len);

                    result.push_back({
                        TokenKind::Number, this->FindMapping(TokenKind::Number),
                        source.substr(start, len), "NUMBER",
                        sl, sc,
                        static_cast<uint32_t>(start), static_cast<uint32_t>(pos)
                    });

                    continue;
                }

                /* identifier */
                if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                    size_t start = pos;
                    uint32_t sl = line, sc = column;
                    size_t i = pos;

                    while (i < source.size() && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;

                    size_t len = i - start;

                    advance(len);

                    result.push_back({
                        TokenKind::Identifier, this->FindMapping(TokenKind::Identifier),
                        source.substr(start, len), "IDENTIFIER",
                        sl, sc,
                        static_cast<uint32_t>(start), static_cast<uint32_t>(pos)
                    });

                    continue;
                }

                /* no rule matched */
                if (this->opt.lenient) {
                    uint32_t sl = line, sc = column;
                    size_t start = pos;

                    std::string bad(1, source[pos]);

                    advance(1);

                    result.push_back({
                        TokenKind::Fallback, this->FindMapping(TokenKind::Fallback),
                        bad, "FALLBACK",
                        sl, sc,
                        static_cast<uint32_t>(start), static_cast<uint32_t>(pos)
                    });
                } else {
                    LexError err{
                        LexErrorType::UnexpectedCharacter,
                        "unexpected character '" + std::string(1, source[pos]) + "'",
                        line, column, static_cast<uint32_t>(pos)
                    };

                    if (this->error_handler) {
                        this->error_handler(err);

                        uint32_t sl = line, sc = column;
                        size_t start = pos;
                        std::string bad(1, source[pos]);
                        advance(1);
                        result.push_back({
                            TokenKind::Fallback, this->FindMapping(TokenKind::Fallback),
                            bad, "FALLBACK",
                            sl, sc,
                            static_cast<uint32_t>(start), static_cast<uint32_t>(pos)
                        });
                    } else {
                        throw LexException(err);
                    }
                }
            }

            /* EOF token */
            result.push_back({
                TokenKind::EndOfFile, this->FindMapping(TokenKind::EndOfFile), "", "EOF", line, column,
                static_cast<uint32_t>(pos), static_cast<uint32_t>(pos)
            });

            return result;
        }

        uint8_t LexerImpl::FindMapping(TokenKind kind) const {
            auto it = this->TokenMappings.find(kind);
            if (it == this->TokenMappings.end())
                return 0;

            return it->second;
        }

        void LexerImpl::BindTokenKind(TokenKind kind, uint8_t value) {
            this->TokenMappings[kind] = value;
        }

        bool LexerImpl::IsToken(std::string input) {
            for (const auto& token : this->token_list) {
                if (token.face == input)
                    return true;
            }
            return false;
        }

        void LexerImpl::OnError(LexErrorHandler handler) {
            this->error_handler = std::move(handler);
        }
    }
}
