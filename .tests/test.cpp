#include <zenex/lex/token_api.h>
#include <zenex/lex/lexer.h>
#include <zenex/pratt/cursor.h>
#include <zenex/pratt/parser.h>
#include <zenex/pratt/printer.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

enum class TokenEnum : uint8_t {
    LET, FN, IF, ELSE, FOR,
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    EQUAL, DOUBLE_EQUAL, BANG_EQUAL,
    LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    PLUS, MINUS, STAR, SLASH, BANG,
    AMP_AMP, PIPE_PIPE,
    I32, STR, CHAR,
    RET, BREAK, CONTINUE,
    COLON, SEMICOLON, COMMA, ARROW,
    IDENTIFIER, TYPE_NUMBER, TYPE_STRING, TYPE_CHAR,
    END_OF_FILE,
    SKIPPED
};

using zenex::TokenCast;

enum class NodeKind : uint32_t {
    TranslationUnit,
    FunctionDecl,
    ParmVarDecl,
    VarDecl,
    DeclStmt,
    CompoundStmt,
    ReturnStmt,
    BreakStmt,
    ContinueStmt,
    IfStmt,
    ForStmt,
    BinaryOperator,
    UnaryOperator,
    ParenExpr,
    CallExpr,
    ArraySubscriptExpr,
    DeclRefExpr,
    IntegerLiteral,
    StringLiteral,
    CharacterLiteral,
};

namespace {
    uint64_t g_addr_counter = 0x55d500000000ull;

    std::string NextAddr() {
        g_addr_counter += 0x18;
        std::ostringstream oss;
        oss << "0x" << std::hex << g_addr_counter;
        return oss.str();
    }
}

struct Node {
    NodeKind kind{};
    std::string addr;
    std::string name;       // identifier / function name / operator symbol
    std::string type_str;   // rendered type, e.g. "i32" or "str[]" or "i32 (i32, str[])"
    uint32_t line = 0;
    uint32_t column = 0;
    std::vector<Node> children;

    // filled in by ComputePrefixes right before printing
    std::string prefix;

    Node() = default;
    explicit Node(NodeKind k) : kind(k), addr(NextAddr()) {}
};

using Parser = zenex::PrattParser<Node>;

// ---------------------------------------------------------------------------
// Type parsing
//
// A type is one of the three primitive keywords, followed by zero or more
// [] suffixes for array levels. Not itself an AST node, clang doesn't dump
// types as tree nodes either, they're just an attribute string on the decl
// that has one.
// ---------------------------------------------------------------------------

std::string ParseType(zenex::TokenCursor& cur) {
    std::string base;

    if (cur.Match(TokenCast(TokenEnum::I32)))       base = "i32";
    else if (cur.Match(TokenCast(TokenEnum::STR)))  base = "str";
    else if (cur.Match(TokenCast(TokenEnum::CHAR))) base = "char";
    else {
        const auto& got = cur.Peek();
        throw zenex::ParseException({
            zenex::ParseErrorType::UnexpectedToken,
            "expected a type but got '" + got.face + "'",
            got.line, got.column
        });
    }

    while (cur.Match(TokenCast(TokenEnum::LBRACKET))) {
        cur.Expect(TokenCast(TokenEnum::RBRACKET), "']'");
        base += "[]";
    }

    return base;
}

// ---------------------------------------------------------------------------
// Expression grammar, built entirely on zenex::PrattParser
//
// Binding powers, low to high:
//   assignment (right assoc)      10
//   ||                            20
//   &&                            30
//   == !=                         40
//   < <= > >=                     50
//   + -                           60
//   * /                           70
//   unary - !                     80
//   call () / subscript []        90
// ---------------------------------------------------------------------------

void RegisterExpressionGrammar(Parser& parser) {
    // literals and identifiers
    parser.RegisterPrefix(TokenCast(TokenEnum::IDENTIFIER), [](Parser&, zenex::TokenCursor& cur) -> Node {
        const auto& tok = cur.Current();
        Node n(NodeKind::DeclRefExpr);
        n.name = tok.face;
        n.line = tok.line;
        n.column = tok.column;
        return n;
    });

    parser.RegisterPrefix(TokenCast(TokenEnum::TYPE_NUMBER), [](Parser&, zenex::TokenCursor& cur) -> Node {
        const auto& tok = cur.Current();
        Node n(NodeKind::IntegerLiteral);
        n.name = tok.face;
        n.type_str = "i32";
        n.line = tok.line;
        n.column = tok.column;
        return n;
    });

    parser.RegisterPrefix(TokenCast(TokenEnum::TYPE_STRING), [](Parser&, zenex::TokenCursor& cur) -> Node {
        const auto& tok = cur.Current();
        Node n(NodeKind::StringLiteral);
        n.name = tok.face;
        n.type_str = "str";
        n.line = tok.line;
        n.column = tok.column;
        return n;
    });

    parser.RegisterPrefix(TokenCast(TokenEnum::TYPE_CHAR), [](Parser&, zenex::TokenCursor& cur) -> Node {
        const auto& tok = cur.Current();
        Node n(NodeKind::CharacterLiteral);
        n.name = tok.face;
        n.type_str = "char";
        n.line = tok.line;
        n.column = tok.column;
        return n;
    });

    // grouping
    parser.RegisterPrefix(TokenCast(TokenEnum::LPAREN), [](Parser& p, zenex::TokenCursor& cur) -> Node {
        const auto& open = cur.Current();
        Node inner = p.ParseExpression(cur, 0);
        cur.Expect(TokenCast(TokenEnum::RPAREN), "')'");
        Node n(NodeKind::ParenExpr);
        n.line = open.line;
        n.column = open.column;
        n.children.push_back(std::move(inner));
        return n;
    });

    // unary operators, bind tighter than everything but call/subscript
    auto register_unary = [&](TokenEnum tok, const std::string& symbol) {
        parser.RegisterPrefix(TokenCast(tok), [symbol](Parser& p, zenex::TokenCursor& cur) -> Node {
            const auto& op = cur.Current();
            Node operand = p.ParseExpression(cur, 80);
            Node n(NodeKind::UnaryOperator);
            n.name = symbol;
            n.line = op.line;
            n.column = op.column;
            n.children.push_back(std::move(operand));
            return n;
        });
    };
    register_unary(TokenEnum::MINUS, "-");
    register_unary(TokenEnum::BANG, "!");

    // binary operators, left associative: the recursive call reuses the
    // same binding power, so an operator of equal precedence stops the
    // loop instead of being swallowed by the right-hand recursion
    auto register_binop = [&](TokenEnum tok, int bp) {
        parser.RegisterInfix(TokenCast(tok), bp, [bp](Parser& p, zenex::TokenCursor& cur, Node left) -> Node {
            const auto& op = cur.Current();
            Node right = p.ParseExpression(cur, bp);
            Node n(NodeKind::BinaryOperator);
            n.name = op.face;
            n.line = op.line;
            n.column = op.column;
            n.children.push_back(std::move(left));
            n.children.push_back(std::move(right));
            return n;
        });
    };

    register_binop(TokenEnum::PIPE_PIPE, 20);
    register_binop(TokenEnum::AMP_AMP, 30);
    register_binop(TokenEnum::DOUBLE_EQUAL, 40);
    register_binop(TokenEnum::BANG_EQUAL, 40);
    register_binop(TokenEnum::LESS, 50);
    register_binop(TokenEnum::LESS_EQUAL, 50);
    register_binop(TokenEnum::GREATER, 50);
    register_binop(TokenEnum::GREATER_EQUAL, 50);
    register_binop(TokenEnum::PLUS, 60);
    register_binop(TokenEnum::MINUS, 60);
    register_binop(TokenEnum::STAR, 70);
    register_binop(TokenEnum::SLASH, 70);

    // assignment, right associative: recurse with bp - 1 so a chain like
    // `a = b = c` nests as `a = (b = c)` instead of stopping after one hop.
    // clang models assignment as just another BinaryOperator, so this does too.
    parser.RegisterInfix(TokenCast(TokenEnum::EQUAL), 10, [](Parser& p, zenex::TokenCursor& cur, Node left) -> Node {
        const auto& op = cur.Current();
        Node right = p.ParseExpression(cur, 9);
        Node n(NodeKind::BinaryOperator);
        n.name = "=";
        n.line = op.line;
        n.column = op.column;
        n.children.push_back(std::move(left));
        n.children.push_back(std::move(right));
        return n;
    });

    // call, binds tighter than any operator
    parser.RegisterInfix(TokenCast(TokenEnum::LPAREN), 90, [](Parser& p, zenex::TokenCursor& cur, Node left) -> Node {
        const auto& open = cur.Current();
        Node n(NodeKind::CallExpr);
        n.line = open.line;
        n.column = open.column;
        n.children.push_back(std::move(left));

        if (!cur.Check(TokenCast(TokenEnum::RPAREN))) {
            do {
                n.children.push_back(p.ParseExpression(cur, 0));
            } while (cur.Match(TokenCast(TokenEnum::COMMA)));
        }
        cur.Expect(TokenCast(TokenEnum::RPAREN), "')'");
        return n;
    });

    // subscript, same precedence tier as call
    parser.RegisterInfix(TokenCast(TokenEnum::LBRACKET), 90, [](Parser& p, zenex::TokenCursor& cur, Node left) -> Node {
        const auto& open = cur.Current();
        Node index = p.ParseExpression(cur, 0);
        cur.Expect(TokenCast(TokenEnum::RBRACKET), "']'");
        Node n(NodeKind::ArraySubscriptExpr);
        n.line = open.line;
        n.column = open.column;
        n.children.push_back(std::move(left));
        n.children.push_back(std::move(index));
        return n;
    });
}

// ---------------------------------------------------------------------------
// Statement and declaration grammar, hand written over TokenCursor.
//
// This is deliberately not routed through PrattParser::RegisterStatement:
// a bare expression statement (a function call, an assignment) can start
// with almost any token, and the library's default ParseStatement fallback
// parses the expression but never consumes the trailing semicolon. Writing
// this layer directly keeps that fully under our control, while every
// actual expression still goes through parser.ParseExpression underneath.
// ---------------------------------------------------------------------------

Node ParseStatement(Parser& parser, zenex::TokenCursor& cur);

Node ParseBlock(Parser& parser, zenex::TokenCursor& cur) {
    const auto& brace = cur.Expect(TokenCast(TokenEnum::LBRACE), "'{'");
    Node block(NodeKind::CompoundStmt);
    block.line = brace.line;
    block.column = brace.column;

    while (!cur.Check(TokenCast(TokenEnum::RBRACE)) && !cur.AtEnd())
        block.children.push_back(ParseStatement(parser, cur));

    cur.Expect(TokenCast(TokenEnum::RBRACE), "'}'");
    return block;
}

Node ParseLet(Parser& parser, zenex::TokenCursor& cur) {
    const auto& let_tok = cur.Advance();
    const auto& name_tok = cur.Expect(TokenCast(TokenEnum::IDENTIFIER), "an identifier");
    cur.Expect(TokenCast(TokenEnum::COLON), "':'");
    std::string type = ParseType(cur);
    cur.Expect(TokenCast(TokenEnum::EQUAL), "'='");
    Node init = parser.ParseExpression(cur);
    cur.Expect(TokenCast(TokenEnum::SEMICOLON), "';'");

    Node var(NodeKind::VarDecl);
    var.name = name_tok.face;
    var.type_str = type;
    var.line = name_tok.line;
    var.column = name_tok.column;
    var.children.push_back(std::move(init));

    Node decl(NodeKind::DeclStmt);
    decl.line = let_tok.line;
    decl.column = let_tok.column;
    decl.children.push_back(std::move(var));
    return decl;
}

Node ParseReturn(Parser& parser, zenex::TokenCursor& cur) {
    const auto& ret_tok = cur.Advance();
    Node n(NodeKind::ReturnStmt);
    n.line = ret_tok.line;
    n.column = ret_tok.column;

    if (!cur.Check(TokenCast(TokenEnum::SEMICOLON)))
        n.children.push_back(parser.ParseExpression(cur));

    cur.Expect(TokenCast(TokenEnum::SEMICOLON), "';'");
    return n;
}

Node ParseBreak(zenex::TokenCursor& cur) {
    const auto& tok = cur.Advance();
    cur.Expect(TokenCast(TokenEnum::SEMICOLON), "';'");
    Node n(NodeKind::BreakStmt);
    n.line = tok.line;
    n.column = tok.column;
    return n;
}

Node ParseContinue(zenex::TokenCursor& cur) {
    const auto& tok = cur.Advance();
    cur.Expect(TokenCast(TokenEnum::SEMICOLON), "';'");
    Node n(NodeKind::ContinueStmt);
    n.line = tok.line;
    n.column = tok.column;
    return n;
}

Node ParseIf(Parser& parser, zenex::TokenCursor& cur) {
    const auto& if_tok = cur.Advance();
    cur.Expect(TokenCast(TokenEnum::LPAREN), "'('");
    Node cond = parser.ParseExpression(cur);
    cur.Expect(TokenCast(TokenEnum::RPAREN), "')'");
    Node then_branch = ParseBlock(parser, cur);

    Node n(NodeKind::IfStmt);
    n.line = if_tok.line;
    n.column = if_tok.column;
    n.children.push_back(std::move(cond));
    n.children.push_back(std::move(then_branch));

    if (cur.Match(TokenCast(TokenEnum::ELSE)))
        n.children.push_back(ParseBlock(parser, cur));

    return n;
}

Node ParseFor(Parser& parser, zenex::TokenCursor& cur) {
    const auto& for_tok = cur.Advance();
    cur.Expect(TokenCast(TokenEnum::LPAREN), "'('");

    Node n(NodeKind::ForStmt);
    n.line = for_tok.line;
    n.column = for_tok.column;


    if (cur.Check(TokenCast(TokenEnum::LET)))
        n.children.push_back(ParseLet(parser, cur));
    else
        cur.Expect(TokenCast(TokenEnum::SEMICOLON), "';'");

    // condition
    if (!cur.Check(TokenCast(TokenEnum::SEMICOLON)))
        n.children.push_back(parser.ParseExpression(cur));

    cur.Expect(TokenCast(TokenEnum::SEMICOLON), "';'");

    // increment
    if (!cur.Check(TokenCast(TokenEnum::RPAREN)))
        n.children.push_back(parser.ParseExpression(cur));

    cur.Expect(TokenCast(TokenEnum::RPAREN), "')'");

    n.children.push_back(ParseBlock(parser, cur));
    return n;
}

Node ParseExprStatement(Parser& parser, zenex::TokenCursor& cur) {
    Node expr = parser.ParseExpression(cur);
    cur.Expect(TokenCast(TokenEnum::SEMICOLON), "';'");
    return expr;
}

Node ParseStatement(Parser& parser, zenex::TokenCursor& cur) {
    if (cur.Check(TokenCast(TokenEnum::LET)))      return ParseLet(parser, cur);
    if (cur.Check(TokenCast(TokenEnum::RET)))       return ParseReturn(parser, cur);
    if (cur.Check(TokenCast(TokenEnum::BREAK)))     return ParseBreak(cur);
    if (cur.Check(TokenCast(TokenEnum::CONTINUE)))  return ParseContinue(cur);
    if (cur.Check(TokenCast(TokenEnum::IF)))        return ParseIf(parser, cur);
    if (cur.Check(TokenCast(TokenEnum::FOR)))       return ParseFor(parser, cur);
    if (cur.Check(TokenCast(TokenEnum::LBRACE)))    return ParseBlock(parser, cur);
    return ParseExprStatement(parser, cur);
}

Node ParseFunction(Parser& parser, zenex::TokenCursor& cur) {
    const auto& fn_tok = cur.Expect(TokenCast(TokenEnum::FN), "'fn'");
    const auto& name_tok = cur.Expect(TokenCast(TokenEnum::IDENTIFIER), "a function name");
    cur.Expect(TokenCast(TokenEnum::LPAREN), "'('");

    std::vector<Node> params;
    std::vector<std::string> param_types;

    if (!cur.Check(TokenCast(TokenEnum::RPAREN))) {
        do {
            const auto& p_name = cur.Expect(TokenCast(TokenEnum::IDENTIFIER), "a parameter name");
            cur.Expect(TokenCast(TokenEnum::COLON), "':'");
            std::string p_type = ParseType(cur);

            Node p(NodeKind::ParmVarDecl);
            p.name = p_name.face;
            p.type_str = p_type;
            p.line = p_name.line;
            p.column = p_name.column;

            param_types.push_back(p_type);
            params.push_back(std::move(p));
        } while (cur.Match(TokenCast(TokenEnum::COMMA)));
    }

    cur.Expect(TokenCast(TokenEnum::RPAREN), "')'");
    cur.Expect(TokenCast(TokenEnum::ARROW), "'->'");
    std::string ret_type = ParseType(cur);
    Node body = ParseBlock(parser, cur);

    Node fn(NodeKind::FunctionDecl);
    fn.name = name_tok.face;
    fn.line = fn_tok.line;
    fn.column = fn_tok.column;

    std::string sig = ret_type + " (";

    for (size_t i = 0; i < param_types.size(); ++i) {
        sig += param_types[i];
        if (i + 1 != param_types.size()) sig += ", ";
    }

    sig += ")";
    fn.type_str = sig;

    for (auto& p : params) fn.children.push_back(std::move(p));
    fn.children.push_back(std::move(body));
    return fn;
}

Node ParseProgram(Parser& parser, zenex::TokenCursor& cur) {
    Node tu(NodeKind::TranslationUnit);
    while (!cur.AtEnd())
        tu.children.push_back(ParseFunction(parser, cur));
    return tu;
}

// ---------------------------------------------------------------------------
// Printing, in the shape clang style dump: an address, a source
// location, then whatever the node needs to say about itself, joined onto
// tree branches that keep a vertical bar running under every ancestor that
// still has more children coming.
//
// zenex::NodePrinter only auto-indents for the direct sibling it is
// currently drawing, it has no notion of "this grandparent still has more
// children after me" needed to keep clang's continuation bars straight. So
// prefixes are precomputed here in one pass and stored on each node, and
// the registered printer functions just emit that prefix verbatim, then
// dispatch to their children exactly as any zenex::NodePrinter user would.
// ---------------------------------------------------------------------------

void ComputePrefixes(Node& node, const std::string& parent_prefix, bool is_last, bool is_root) {
    if (is_root)
        node.prefix = "";
    else
        node.prefix = parent_prefix + (is_last ? "`-" : "|-");

    std::string child_prefix = is_root ? "" : parent_prefix + (is_last ? "  " : "| ");

    for (size_t i = 0; i < node.children.size(); ++i) {
        bool last_child = (i + 1 == node.children.size());
        ComputePrefixes(node.children[i], child_prefix, last_child, false);
    }
}

std::string Loc(const Node& n) {
    if (n.line == 0) return "";
    return " <line:" + std::to_string(n.line) + ", col:" + std::to_string(n.column) + ">";
}

zenex::NodePrinter<Node> BuildPrinter() {
    zenex::NodePrinter<Node> printer;
    printer.SetTagFn([](const Node& n) { return static_cast<uint32_t>(n.kind); });

    auto simple = [](const std::string& label) {
        return [label](const Node& n, zenex::PrintContext<Node>& ctx) {
            ctx.WriteLine(n.prefix + label + " " + n.addr + Loc(n));
            for (const auto& c : n.children) ctx.PrintChild(c);
        };
    };

    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::TranslationUnit), simple("TranslationUnitDecl"));

    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::FunctionDecl),
        [](const Node& n, zenex::PrintContext<Node>& ctx) {
            ctx.WriteLine(n.prefix + "FunctionDecl " + n.addr + Loc(n) + " " + n.name + " '" + n.type_str + "'");
            for (const auto& c : n.children) ctx.PrintChild(c);
        });

    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::ParmVarDecl),
        [](const Node& n, zenex::PrintContext<Node>& ctx) {
            ctx.WriteLine(n.prefix + "ParmVarDecl " + n.addr + Loc(n) + " " + n.name + " '" + n.type_str + "'");
        });

    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::VarDecl),
        [](const Node& n, zenex::PrintContext<Node>& ctx) {
            ctx.WriteLine(n.prefix + "VarDecl " + n.addr + Loc(n) + " " + n.name + " '" + n.type_str + "'");
            for (const auto& c : n.children) ctx.PrintChild(c);
        });

    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::DeclStmt), simple("DeclStmt"));
    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::CompoundStmt), simple("CompoundStmt"));
    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::ReturnStmt), simple("ReturnStmt"));
    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::BreakStmt), simple("BreakStmt"));
    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::ContinueStmt), simple("ContinueStmt"));
    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::IfStmt), simple("IfStmt"));
    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::ForStmt), simple("ForStmt"));
    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::ParenExpr), simple("ParenExpr"));
    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::CallExpr), simple("CallExpr"));
    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::ArraySubscriptExpr), simple("ArraySubscriptExpr"));

    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::BinaryOperator),
        [](const Node& n, zenex::PrintContext<Node>& ctx) {
            ctx.WriteLine(n.prefix + "BinaryOperator " + n.addr + Loc(n) + " '" + n.name + "'");
            for (const auto& c : n.children) ctx.PrintChild(c);
        });

    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::UnaryOperator),
        [](const Node& n, zenex::PrintContext<Node>& ctx) {
            ctx.WriteLine(n.prefix + "UnaryOperator " + n.addr + Loc(n) + " prefix '" + n.name + "'");
            for (const auto& c : n.children) ctx.PrintChild(c);
        });

    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::DeclRefExpr),
        [](const Node& n, zenex::PrintContext<Node>& ctx) {
            ctx.WriteLine(n.prefix + "DeclRefExpr " + n.addr + Loc(n) + " '" + n.name + "'");
        });

    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::IntegerLiteral),
        [](const Node& n, zenex::PrintContext<Node>& ctx) {
            ctx.WriteLine(n.prefix + "IntegerLiteral " + n.addr + Loc(n) + " '" + n.type_str + "' " + n.name);
        });

    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::StringLiteral),
        [](const Node& n, zenex::PrintContext<Node>& ctx) {
            ctx.WriteLine(n.prefix + "StringLiteral " + n.addr + Loc(n) + " '" + n.type_str + "' " + n.name);
        });

    printer.RegisterPrinter(static_cast<uint32_t>(NodeKind::CharacterLiteral),
        [](const Node& n, zenex::PrintContext<Node>& ctx) {
            ctx.WriteLine(n.prefix + "CharacterLiteral " + n.addr + Loc(n) + " '" + n.type_str + "' " + n.name);
        });

    return printer;
}

int main() {
    auto tokens = zenex::NewTokenList<TokenEnum>({
        zenex::AddToken(TokenEnum::LET, "let", "LET"),
        zenex::AddToken(TokenEnum::FN, "fn", "FN"),
        zenex::AddToken(TokenEnum::IF, "if", "IF"),
        zenex::AddToken(TokenEnum::ELSE, "else", "ELSE"),
        zenex::AddToken(TokenEnum::FOR, "for", "FOR"),
        zenex::AddToken(TokenEnum::LPAREN, "(", "LEFT_PAREN"),
        zenex::AddToken(TokenEnum::RPAREN, ")", "RIGHT_PAREN"),
        zenex::AddToken(TokenEnum::LBRACE, "{", "LEFT_BRACE"),
        zenex::AddToken(TokenEnum::RBRACE, "}", "RIGHT_BRACE"),
        zenex::AddToken(TokenEnum::LBRACKET, "[", "LEFT_BRACKET"),
        zenex::AddToken(TokenEnum::RBRACKET, "]", "RIGHT_BRACKET"),
        zenex::AddToken(TokenEnum::DOUBLE_EQUAL, "==", "DOUBLE_EQUAL"),
        zenex::AddToken(TokenEnum::EQUAL, "=", "EQUAL"),
        zenex::AddToken(TokenEnum::BANG_EQUAL, "!=", "BANG_EQUAL"),
        zenex::AddToken(TokenEnum::LESS_EQUAL, "<=", "LESS_EQUAL"),
        zenex::AddToken(TokenEnum::LESS, "<", "LESS"),
        zenex::AddToken(TokenEnum::GREATER_EQUAL, ">=", "GREATER_EQUAL"),
        zenex::AddToken(TokenEnum::GREATER, ">", "GREATER"),
        zenex::AddToken(TokenEnum::PLUS, "+", "PLUS"),
        zenex::AddToken(TokenEnum::ARROW, "->", "ARROW"),
        zenex::AddToken(TokenEnum::MINUS, "-", "MINUS"),
        zenex::AddToken(TokenEnum::STAR, "*", "STAR"),
        zenex::AddToken(TokenEnum::SLASH, "/", "SLASH"),
        zenex::AddToken(TokenEnum::BANG, "!", "BANG"),
        zenex::AddToken(TokenEnum::AMP_AMP, "&&", "AMP_AMP"),
        zenex::AddToken(TokenEnum::PIPE_PIPE, "||", "PIPE_PIPE"),
        zenex::AddToken(TokenEnum::I32, "i32", "INTEGER_32"),
        zenex::AddToken(TokenEnum::STR, "str", "STR"),
        zenex::AddToken(TokenEnum::CHAR, "char", "CHAR"),
        zenex::AddToken(TokenEnum::RET, "return", "RETURN"),
        zenex::AddToken(TokenEnum::BREAK, "break", "BREAK"),
        zenex::AddToken(TokenEnum::CONTINUE, "continue", "CONTINUE"),
        zenex::AddToken(TokenEnum::COLON, ":", "COLON"),
        zenex::AddToken(TokenEnum::SEMICOLON, ";", "SEMICOLON"),
        zenex::AddToken(TokenEnum::COMMA, ",", "COMMA"),
    });

    zenex::Lexer lexer;
    try {
        lexer = zenex::CreateLexer(tokens, zenex::lexopt{
            zenex::lskip {
                zenex::regex { R"(//[^\n]*)" },
                zenex::regex { R"(/\*[\s\S]*?\*/)" }
            },
            zenex::llenient,
        });
    } catch (const zenex::LexException& e) {
        std::cerr << "failed to create lexer: " << e.error.message << '\n';
        return 1;
    }

    lexer->OnError([](const zenex::LexError& err) {
        std::cerr << "[lex " << err.line << ":" << err.column << "] " << err.message << '\n';
    });

    lexer->BindTokenKind(zenex::TokenKind::Skipped, zenex::TokenCast(TokenEnum::SKIPPED));
    lexer->BindTokenKind(zenex::TokenKind::Identifier, zenex::TokenCast(TokenEnum::IDENTIFIER));
    lexer->BindTokenKind(zenex::TokenKind::Fallback, zenex::TokenCast(TokenEnum::IDENTIFIER));
    lexer->BindTokenKind(zenex::TokenKind::Number, zenex::TokenCast(TokenEnum::TYPE_NUMBER));
    lexer->BindTokenKind(zenex::TokenKind::String, zenex::TokenCast(TokenEnum::TYPE_STRING));
    lexer->BindTokenKind(zenex::TokenKind::Char, zenex::TokenCast(TokenEnum::TYPE_CHAR));
    lexer->BindTokenKind(zenex::TokenKind::EndOfFile, zenex::TokenCast(TokenEnum::END_OF_FILE));

    std::string source = R"(
// zenex demo language

fn add(a: i32, b: i32) -> i32 {
    return a + b * 2;
}

fn clamp(x: i32, lo: i32, hi: i32) -> i32 {
    if (x < lo) {
        return lo;
    } else {
        if (x > hi) {
            return hi;
        }
    }
    return x;
}

fn sum_to(n: i32) -> i32 {
    let total: i32 = 0;
    for (let i: i32 = 0; i < n; i = i + 1) {
        if (i == 5) {
            continue;
        }
        if (i > 20) {
            break;
        }
        total = total + i;
    }
    return total;
}

fn main(argc: i32, argv: str[]) -> i32 {
    let a: i32 = add(2, 3);
    let clamped: i32 = clamp(a, 0, 10);
    let total: i32 = sum_to(clamped);
    let first: str = argv[0];
    let ok: i32 = (a == 5) && (clamped <= 10) || !(total == 0);
    return total;
}
)";

    zenex::LexerTokens stream;
    try {
        stream = lexer->TokeniseInput(source);
    } catch (const zenex::LexException& e) {
        std::cerr << e.error.message << '\n';
        return 1;
    }

    Parser parser;
    RegisterExpressionGrammar(parser);

    parser.OnError([](const zenex::ParseError& err) {
        std::cerr << "[parse " << err.line << ":" << err.column << "] " << err.message << '\n';
    });

    zenex::TokenCursor cursor(stream);
    Node program;
    try {
        program = ParseProgram(parser, cursor);
    } catch (const zenex::ParseException& e) {
        std::cerr << "parse failed: " << e.error.message << '\n';
        return 1;
    }

    ComputePrefixes(program, "", true, true);

    zenex::NodePrinter<Node> printer = BuildPrinter();
    std::cout << printer.PrintStructure(program);
}
