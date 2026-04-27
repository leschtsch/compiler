#include "dump_ast.hpp"

#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>

#include <cstdint>
#include <ostream>
#include <string>
#include <variant>

#include "ast.hpp"

namespace parser {

namespace {

//===========================V=HELPERS=V============================================================
void DumpNodeOpen(std::ostream& ostream, const nodes::BaseNode* node) {
  ostream << reinterpret_cast<std::uintptr_t>(node) << "[ ";
}

void DumpNodeLabel(std::ostream& ostream, auto* node) {
  ostream << "label = \"" << nodes::GetName(node) << "\" ";
}

void DumpNodeLabel(std::ostream& ostream, const std::string& label) {
  ostream << "label = \"" << label << "\" ";
}

void DumpNodeColor(std::ostream& ostream, const nodes::BaseNode* node) {
  if (!node->Healthy()) {
    ostream << "style=filled fillcolor=lightpink ";
  }
}

void DumpNodeColor(std::ostream& ostream, const std::string& color) {
  ostream << "style=filled fillcolor=" << color << " ";
}

void DumpNodeClose(std::ostream& ostream) { ostream << "];\n"; }

[[nodiscard]]  void* GetAddr(const nodes::NodesVariant& node) {
  return std::visit([](auto& ptr) -> void* { return ptr.get(); }, node);
}
//===========================^=HELPERS=^============================================================

//===========================V=DUMP=OVERLOADS=V=====================================================
void DumpNodeBody(std::ostream& ostream, const auto* node) {
  DumpNodeLabel(ostream, node);
  DumpNodeColor(ostream, node);
}

void DumpNodeBody(std::ostream& ostream, const nodes::IdNode* node) {
  DumpNodeLabel(
      ostream,
      nodes::GetName(node) + "\\n" + ecs::Get<ecs::IdName>(*node).Value());

  DumpNodeColor(ostream, node);
}

void DumpNodeBody(std::ostream& ostream, const nodes::ErrorNode* node) {
  // TODO: escape sequences

  DumpNodeLabel(ostream, ecs::Get<ecs::ErrorMessage>(*node).Value());
  DumpNodeColor(ostream, "darkred");
}

void DumpNodeBody(std::ostream& ostream, const nodes::IntLiteral* node) {

  DumpNodeLabel(ostream,
                nodes::GetName(node) + "\\n" +
                    std::to_string(ecs::Get<ecs::IntValue>(*node).Value()));
}

void DumpNodeBody(std::ostream& ostream, const nodes::FloatLiteral* node) {

  DumpNodeLabel(ostream,
                nodes::GetName(node) + "\\n" +
                    std::to_string(ecs::Get<ecs::FloatValue>(*node).Value()));
}

void DumpNodeBody(std::ostream& ostream, const nodes::StrLiteral* node) {

  // TODO: escape sequences
  DumpNodeLabel(
      ostream,
      nodes::GetName(node) + "\\n" + ecs::Get<ecs::StrValue>(*node).Value());
}
//===========================^=DUMP=OVERLOADS=V=====================================================

class Visitor {
 public:
  explicit Visitor(std::ostream& ostream) : ostream_(ostream) {}

  void operator()(const auto& ptr) {
    const auto* node = ptr.get();

    if (node == nullptr) {
      return;
    }

    DumpNodeOpen(ostream_, node);
    DumpNodeBody(ostream_, node);
    DumpNodeClose(ostream_);

    for (const auto& child : node->ChildrenSpan()) {
      void* child_addr = GetAddr(child);

      if (child_addr == nullptr) {
        continue;
      }

      ostream_ << reinterpret_cast<std::uintptr_t>(node) << " -> "
               << reinterpret_cast<std::uintptr_t>(child_addr) << ";\n";

      std::visit(*this, child);
    }
  }

 private:
  std::ostream& ostream_;
};

}  // namespace

void DumpAst(std::ostream& ostream, const nodes::NodesVariant& node) {
  Visitor visitor(ostream);
  ostream << "digraph tree {\n";
  std::visit(visitor, node);
  ostream << "}\n";
}

}  // namespace parser
