#include "dump_ast.hpp"

#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <variant>

#include "ast.hpp"

namespace parser {

namespace {

//===========================V=HELPERS=V============================================================
void DumpNodeOpen(std::ostream& ostream, const AstNode* node) {
  ostream << reinterpret_cast<std::uintptr_t>(node) << "[ ";
}

void DumpNodeLabel(std::ostream& ostream, auto tok) {
  ostream << "label = \"" << tokens::GetName(tok) << "\" ";
}

void DumpNodeLabel(std::ostream& ostream, const std::string& label) {
  ostream << "label = \"" << label << "\" ";
}

void DumpNodeColor(std::ostream& ostream, const AstNode* node) {
  if (node->has_active_error) {
    ostream << "style=filled fillcolor=lightpink ";
  }
}

void DumpNodeColor(std::ostream& ostream, const std::string& color) {
  ostream << "style=filled fillcolor=" << color << " ";
}

void DumpNodeClose(std::ostream& ostream) { ostream << "];\n"; }

//===========================^=HELPERS=^============================================================

//===========================V=DUMP=OVERLOADS=V=====================================================
void DumpNodeBody(std::ostream& ostream, auto tok, const AstNode* node) {
  DumpNodeLabel(ostream, tok);
  DumpNodeColor(ostream, node);
}

void DumpNodeBody(std::ostream& ostream,
                  tokens::IdToken tok,
                  const AstNode* node) {
  DumpNodeLabel(
      ostream,
      tokens::GetName(tok) + "\\n" + ecs::Get<ecs::IdName>(tok).Value());

  DumpNodeColor(ostream, node);
}

void DumpNodeBody(std::ostream& ostream,
                  tokens::ErrorToken tok,
                  const AstNode* /* node */) {
  // TODO: escape sequences

  DumpNodeLabel(ostream, ecs::Get<ecs::ErrorMessage>(tok).Value());
  DumpNodeColor(ostream, "darkred");
}

void DumpNodeBody(std::ostream& ostream,
                  tokens::IntLiteral tok,
                  const AstNode* /* node */) {

  DumpNodeLabel(ostream,
                tokens::GetName(tok) + "\\n" +
                    std::to_string(ecs::Get<ecs::IntValue>(tok).Value()));
}

void DumpNodeBody(std::ostream& ostream,
                  tokens::FloatLiteral tok,
                  const AstNode* /* node */) {

  DumpNodeLabel(ostream,
                tokens::GetName(tok) + "\\n" +
                    std::to_string(ecs::Get<ecs::FloatValue>(tok).Value()));
}

void DumpNodeBody(std::ostream& ostream,
                  tokens::StrLiteral tok,
                  const AstNode* /* node */) {

  // TODO: escape sequences
  DumpNodeLabel(
      ostream,
      tokens::GetName(tok) + "\\n" + ecs::Get<ecs::StrValue>(tok).Value());
}
//===========================^=DUMP=OVERLOADS=V=====================================================

using FakeVariant = std::variant<const AstNode*>;

class Visitor {
 public:
  explicit Visitor(std::ostream& ostream, const AstNode* node)
      : ostream_(ostream), node_(node) {}

  void operator()(auto tok, const AstNode* node) {
    DumpNodeOpen(ostream_, node);
    DumpNodeBody(ostream_, tok, node);
    DumpNodeClose(ostream_);

    VisitChildren(node);
  }

 private:
  std::ostream& ostream_;
  const AstNode* node_;

  void VisitChildren(const AstNode* node) {
    for (const AstNode& child : node->children) {
      node_ = std::addressof(child);
      ostream_ << reinterpret_cast<std::uintptr_t>(node) << " -> "
               << reinterpret_cast<std::uintptr_t>(node_) << ";\n";
      std::visit(*this, child.token, FakeVariant(std::addressof(child)));
    }
  }
};

}  // namespace

void DumpAst(std::ostream& ostream, const AstNode& node) {
  Visitor visitor(ostream, std::addressof(node));
  ostream << "digraph tree {\n";
  std::visit(visitor, node.token, FakeVariant(std::addressof(node)));
  ostream << "}\n";
}

}  // namespace parser
