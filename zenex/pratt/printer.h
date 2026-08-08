#pragma once

#include <string>
#include <sstream>
#include <functional>
#include <unordered_map>
#include <stdexcept>

namespace zenex {
    template <typename Node> class NodePrinter;

    template <typename Node>
    class PrintContext {
    public:
        /*
            @description: constructs a print context bound to a NodePrinter, so callbacks can recurse into children
            @returns -> zenex::pratt::PrintContext<Node>
        */
        explicit PrintContext(NodePrinter<Node>& p) : printer(p) {}

        /*
            @description: appends raw text to the output with no trailing newline
            @returns -> void
        */
        void Write(const std::string& s) { out << s; }

        /*
            @description: appends text followed by a newline
            @returns -> void
        */
        void WriteLine(const std::string& s) { out << s << '\n'; }

        /*
            @description: recurses into a child node using whatever printer is registered for its kind
            @returns -> void
        */
        void PrintChild(const Node& child) { printer.PrintNode(child, *this); }

        /*
            @description: prints a homogeneous list of children with tree connectors, handling the is-last-one logic automatically
            @returns -> void
        */
        template <typename Container>
        void PrintChildren(const Container& children) {
            size_t i = 0, n = children.size();
            for (const auto& child : children) {
                bool is_last = (++i == n);
                Write(Pad() + Connector(is_last));
                Indent();
                PrintChild(child);
                Dedent();
            }
        }

        /*
            @description: prints a delimiter-joined list of children, with no tree connectors, for flat or JSON style output
            @returns -> void
        */
        template <typename Container, typename Sep>
        void PrintList(const Container& children, Sep separator) {
            size_t i = 0, n = children.size();
            for (const auto& child : children) {
                PrintChild(child);
                if (++i != n) Write(separator);
            }
        }

        /*
            @description: writes a labeled scalar value, for data that is not itself a child Node
            @returns -> void
        */
        void Field(const std::string& label, const std::string& value) {
            Write(label + "=" + value + " ");
        }

        /*
            @description: returns the current indentation depth
            @returns -> int
        */
        int Depth() const { return depth; }

        /*
            @description: increases indentation depth
            @returns -> void
        */
        void Indent(int n = 1) { depth += n; }

        /*
            @description: decreases indentation depth
            @returns -> void
        */
        void Dedent(int n = 1) { depth -= n; }

        /*
            @description: builds the current indentation prefix
            @returns -> std::string
        */
        std::string Pad(const std::string& unit = "  ") const {
            std::string s;
            for (int i = 0; i < depth; ++i) s += unit;
            return s;
        }

        /*
            @description: returns a tree branch or corner connector glyph, depending on whether this is the last sibling
            @returns -> std::string
        */
        std::string Connector(bool is_last) const {
            return is_last ? "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80 " : "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 ";
        }

        /*
            @description: returns the accumulated output, consuming the internal buffer
            @returns -> std::string
        */
        std::string TakeOutput() { return out.str(); }

    private:
        NodePrinter<Node>& printer;
        std::ostringstream out;
        int depth = 0;
    };

    template <typename Node>
    class NodePrinter {
    public:
        using TagFn   = std::function<uint32_t(const Node&)>;
        using PrintFn = std::function<void(const Node&, PrintContext<Node>&)>;

        /*
            @description: registers the function used to identify a node's kind, usually a variant index or an existing Kind accessor
            @returns -> void
        */
        void SetTagFn(TagFn fn) { tag_fn = std::move(fn); }

        /*
            @description: registers a print function for a given node kind
            @returns -> void
        */
        void RegisterPrinter(uint32_t tag, PrintFn fn) {
            printers[tag] = std::move(fn);
        }

        /*
            @description: prints an entire tree starting at root and returns the accumulated output
            @returns -> std::string
        */
        std::string PrintStructure(const Node& root) {
            PrintContext<Node> ctx(*this);
            PrintNode(root, ctx);
            return ctx.TakeOutput();
        }

        /*
            @description: dispatches a single node to its registered printer, used internally by PrintContext::PrintChild
            @returns -> void
        */
        void PrintNode(const Node& node, PrintContext<Node>& ctx) {
            if (!tag_fn)
                throw std::logic_error("zenex: NodePrinter::SetTagFn must be called before printing");

            uint32_t tag = tag_fn(node);
            auto it = printers.find(tag);
            if (it == printers.end()) {
                ctx.Write("<unregistered node kind " + std::to_string(tag) + ">");
                return;
            }
            it->second(node, ctx);
        }

    private:
        TagFn tag_fn;
        std::unordered_map<uint32_t, PrintFn> printers;
    };
}
