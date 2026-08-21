#pragma once

#include <string>
#include <sstream>
#include <functional>
#include <unordered_map>
#include <stdexcept>
#include <vector>

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
        void Write(const std::string& s) {
            this->out << s;
        }

        /*
            @description: appends text followed by a newline
            @returns -> void
        */
        void WriteLine(const std::string& s) {
            this->out << s << '\n';
        }

        /*
            @description: recurses into a child node using whatever printer is registered for its kind
            @returns -> void
        */
        void PrintChild(const Node& child) {
            this->printer.PrintNode(child, *this);
        }

        /*
            @description: pushes a child's positional state onto the formatting stack and writes the tree prefix
            @returns -> void
        */
        void PushChild(bool is_last) {
            this->Write(this->BuildPrefix(is_last));
            this->is_last_stack.push_back(is_last);
        }

        /*
            @description: pops the last child state from the formatting stack
            @returns -> void
        */
        void PopChild() {
            if (!this->is_last_stack.empty()) {
                this->is_last_stack.pop_back();
            }
        }

        /*
            @description: prints a homogeneous list of children with tree connectors, handling the is-last-one logic automatically
            @returns -> void
        */
        template <typename Container>
        void PrintChildren(const Container& children) {
            size_t i = 0, n = children.size();
            for (const auto& child : children) {
                bool is_last = (++i == n);
                this->PushChild(is_last);
                this->PrintChild(child);
                this->PopChild();
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
                this->PrintChild(child);
                if (++i != n) {
                    this->Write(separator);
                }
            }
        }

        /*
            @description: writes a labeled scalar value, for data that is not itself a child Node
            @returns -> void
        */
        void Field(const std::string& label, const std::string& value) {
            this->Write(label + "=" + value + " ");
        }

        /*
            @description: returns the current indentation depth based on the stack size
            @returns -> int
        */
        int Depth() const {
            return static_cast<int>(this->is_last_stack.size());
        }

        /*
            @description: builds backwall lines for parent branches and connectors for the current level
            @returns -> std::string
        */
        std::string BuildPrefix(bool is_last) const {
            std::string prefix;
            for (bool parent_is_last : this->is_last_stack) {
                if (parent_is_last) {
                    prefix += "    ";
                } else {
                    prefix += "\xE2\x94\x82   "; /* "│   " */
                }
            }
            prefix += this->Connector(is_last);
            return prefix;
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
        std::string TakeOutput() {
            return this->out.str();
        }

        /*
            @description: clears the internal string stream buffer and depth state, allowing context reuse
            @returns -> void
        */
        void Clear() {
            this->out.str("");
            this->out.clear();
            this->is_last_stack.clear();
        }

    private:
        NodePrinter<Node>& printer;
        std::ostringstream out;
        std::vector<bool> is_last_stack;
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
        void SetTagFn(TagFn fn) {
            this->tag_fn = std::move(fn);
        }

        /*
            @description: registers a print function for a given node kind
            @returns -> void
        */
        void RegisterPrinter(uint32_t tag, PrintFn fn) {
            this->printers[tag] = std::move(fn);
        }

        /*
            @description: checks if a specific tag has a registered print function
            @returns -> bool
        */
        bool HasPrinter(uint32_t tag) const {
            return this->printers.find(tag) != this->printers.end();
        }

        /*
            @description: prints an entire tree starting at root and returns the accumulated output
            @returns -> std::string
        */
        std::string PrintStructure(const Node& root) {
            PrintContext<Node> ctx(*this);
            this->PrintNode(root, ctx);
            return ctx.TakeOutput();
        }

        /*
            @description: dispatches a single node to its registered printer, used internally by PrintContext::PrintChild
            @returns -> void
        */
        void PrintNode(const Node& node, PrintContext<Node>& ctx) {
            if (!this->tag_fn) {
                throw std::logic_error("NodePrinter::SetTagFn must be called before printing");
            }

            uint32_t tag = this->tag_fn(node);
            auto it = this->printers.find(tag);
            if (it == this->printers.end()) {
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
