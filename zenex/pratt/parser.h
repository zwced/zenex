#pragma once

#include <zenex/pratt/cursor.h>
#include <zenex/pratt/errors.h>

#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <vector>

namespace zenex {
    template <typename Node>
    class PrattParser {
    public:
        using PrefixFn    = std::function<Node(PrattParser&, TokenCursor&)>;
        using InfixFn     = std::function<Node(PrattParser&, TokenCursor&, Node left)>;
        using StatementFn = std::function<Node(PrattParser&, TokenCursor&)>;

        /*
            @description: registers a rule for when a token starts an expression
            @returns -> void
        */
        void RegisterPrefix(uint8_t token_enum, PrefixFn fn) {
            this->prefix_rules[token_enum] = std::move(fn);
        }

        /*
            @description: registers a rule for when a token appears after an existing left hand side, with a binding power controlling precedence
            @returns -> void
        */
        void RegisterInfix(uint8_t token_enum, int bp, InfixFn fn) {
            this->infix_rules[token_enum] = { bp, std::move(fn) };
        }

        /*
            @description: temporarily disables an infix rule without unregistering it, so ParseExpression
            treats the token as if it had no continuation until UnsuppressInfix is called
            @returns -> void
        */
        void SuppressInfix(uint8_t token_enum) {
            this->suppressed_infix.insert(token_enum);
        }

        /*
            @description: re-enables an infix rule previously disabled by SuppressInfix
            @returns -> void
        */
        void UnsuppressInfix(uint8_t token_enum) {
            this->suppressed_infix.erase(token_enum);
        }

        /*
            @description: registers a rule dispatched on a statement's leading token, for constructs like declarations and blocks that have no binding power relationship
            @returns -> void
        */
        void RegisterStatement(uint8_t token_enum, StatementFn fn) {
            this->statement_rules[token_enum] = std::move(fn);
        }

        /*
            @description: checks if a prefix rule exists for the given token type
            @returns -> bool
        */
        bool HasPrefixRule(uint8_t token_enum) const {
            return this->prefix_rules.find(token_enum) != this->prefix_rules.end();
        }

        /*
            @description: checks if an infix rule exists for the given token type
            @returns -> bool
        */
        bool HasInfixRule(uint8_t token_enum) const {
            return this->infix_rules.find(token_enum) != this->infix_rules.end();
        }

        /*
            @description: checks if a statement rule exists for the given token type
            @returns -> bool
        */
        bool HasStatementRule(uint8_t token_enum) const {
            return this->statement_rules.find(token_enum) != this->statement_rules.end();
        }

        /*
            @description: registers a handler called before a ParseException is thrown, for logging or custom diagnostics
            @returns -> void
        */
        void OnError(ParseErrorHandler handler) {
            this->error_handler = std::move(handler);
        }

        /*
            @description: parses a single expression starting at the cursor, recursing while the next infix rule's binding power exceeds min_bp
            @returns -> Node
        */
        Node ParseExpression(TokenCursor& cursor, int min_bp = 0) {
            const auto& first = cursor.Advance();

            auto pit = this->prefix_rules.find(first.enumeration);
            if (pit == this->prefix_rules.end()) {
                this->Fail({ ParseErrorType::NoPrefixRule,
                    "zenex: no prefix rule for token '" + first.face + "'",
                    first.line, first.column });
            }

            Node left = pit->second(*this, cursor);

            while (true) {
                const auto& next = cursor.Peek();
                auto iit = this->infix_rules.find(next.enumeration);
                if (iit == this->infix_rules.end() || iit->second.bp <= min_bp ||
                    this->suppressed_infix.count(next.enumeration))
                    break;

                cursor.Advance();
                left = iit->second.fn(*this, cursor, std::move(left));
            }

            return left;
        }

        /*
            @description: parses a single statement, dispatched on its leading token, falling back to a bare expression if nothing is registered
            @returns -> Node
        */
        Node ParseStatement(TokenCursor& cursor) {
            const auto& lead = cursor.Peek();

            auto sit = this->statement_rules.find(lead.enumeration);
            if (sit != this->statement_rules.end()) {
                return sit->second(*this, cursor);
            }

            return this->ParseExpression(cursor);
        }

        /*
            @description: parses statements until close_token is reached, then consumes it, for function bodies and any other delimited block, the opening delimiter must already be consumed by the caller
            @returns -> std::vector<Node>
        */
        std::vector<Node> ParseBlock(TokenCursor& cursor, uint8_t close_token, const std::string& close_what) {
            std::vector<Node> body;
            while (!cursor.Check(close_token) && !cursor.AtEnd()) {
                body.push_back(this->ParseStatement(cursor));
            }
            cursor.Expect(close_token, close_what);
            return body;
        }

        /*
            @description: parses statements until end of input, the entry point for a whole token stream
            @returns -> std::vector<Node>
        */
        std::vector<Node> ParseProgram(TokenCursor& cursor) {
            std::vector<Node> program;
            while (!cursor.AtEnd()) {
                program.push_back(this->ParseStatement(cursor));
            }
            return program;
        }

    private:
        struct InfixRule { int bp; InfixFn fn; };

        /*
            @description: notifies the error handler if one is registered, then throws
            @returns -> void
        */
        void Fail(ParseError err) {
            if (this->error_handler) {
                this->error_handler(err);
            }
            throw ParseException(std::move(err));
        }

        std::unordered_map<uint8_t, PrefixFn>    prefix_rules;
        std::unordered_map<uint8_t, InfixRule>   infix_rules;
        std::unordered_set<uint8_t>              suppressed_infix;
        std::unordered_map<uint8_t, StatementFn> statement_rules;

        ParseErrorHandler error_handler;
    };
}
