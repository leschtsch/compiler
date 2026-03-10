#include "dump_ast.hpp"

#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>

#include <cstdint>
#include <memory>
#include <ostream>
#include <variant>

#include "ast.hpp"

namespace parser {

namespace {

template <typename T>
void DumpNode(T /* tok */, const AstNode* node, std::ostream& ostr) = delete;

template <>
void DumpNode<tokens::BaseToken>(tokens::BaseToken tok,
                                 const AstNode* node,
                                 std::ostream& ostream) {
  ostream << reinterpret_cast<std::uintptr_t>(node) << "[label = \""
          << tokens::GetName(tok) << "\"];\n";
}

#define TOKEN(Token, Parent)                                           \
  template <>                                                          \
  [[maybe_unused]] void DumpNode<tokens::Token>(                       \
      tokens::Token tok, const AstNode* node, std::ostream& ostream) { \
    ostream << reinterpret_cast<std::uintptr_t>(node) << "[label = \"" \
            << tokens::GetName(tok) << "\"];\n";                       \
  }

#include <language_data/grammar_tokens.dat>

#undef TOKEN

void DumpNode(tokens::IdToken tok, const AstNode* node, std::ostream& ostream) {
  ostream << reinterpret_cast<std::uintptr_t>(node) << "[label = \""
          << tokens::GetName(tok) << "\\n"
          << ecs::Get<ecs::IdName>(tok).Value() << "\"];\n";
}

using FakeVariant = std::variant<const AstNode*>;

class Visitor {
 public:
  explicit Visitor(std::ostream& ostream, const AstNode* node)
      : ostream_(ostream), node_(node) {}

  void operator()(auto tok, const AstNode* node) {
    DumpNode(tok, node, ostream_);
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
