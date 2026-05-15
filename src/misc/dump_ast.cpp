#include "dump_ast.hpp"

#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>
#include <parser/ast.hpp>
#include <semantics/symbols.hpp>

#include <cstdint>
#include <ostream>
#include <string>
#include <variant>

namespace misc {

namespace {

//===========================V=HELPERS=V============================================================
void DumpNodeOpen(std::ostream& ostream, const parser::nodes::BaseNode* node) {
  auto node_name = reinterpret_cast<std::uintptr_t>(node);

  ostream << node_name << "-> dummy [style=invis];\n";
  ostream << "subgraph ast_nodes {\n" << node_name << "[ ";
}

void DumpNodeLabel(std::ostream& ostream, auto* node) {
  ostream << "label = \"" << parser::nodes::GetName(node) << "\" ";
}

void DumpNodeLabel(std::ostream& ostream, const std::string& label) {
  ostream << "label = \"" << label << "\" ";
}

void DumpNodeColor(std::ostream& ostream, const parser::nodes::BaseNode* node) {
  if (!node->Healthy()) {
    ostream << "style=filled fillcolor=lightpink ";
  }
}

void DumpNodeColor(std::ostream& ostream, const std::string& color) {
  ostream << "style=filled fillcolor=" << color << " ";
}

void DumpNodeClose(std::ostream& ostream) { ostream << "];\n}\n"; }

[[nodiscard]] void* GetAddr(const parser::nodes::NodesVariant& node) {
  return std::visit([](auto& ptr) -> void* { return ptr.get(); }, node);
}
//===========================^=HELPERS=^============================================================

//===========================V=DUMP=OVERLOADS=V=====================================================
void DumpNodeBody(std::ostream& ostream, const auto* node) {
  DumpNodeLabel(ostream, node);
  DumpNodeColor(ostream, node);
}

void DumpNodeBody(std::ostream& ostream, const parser::nodes::IdNode* node) {
  DumpNodeLabel(ostream,
                parser::nodes::GetName(node) + "\\n" +
                    ecs::Get<ecs::IdName>(*node).Value());

  DumpNodeColor(ostream, node);
}

void DumpNodeBody(std::ostream& ostream, const parser::nodes::ErrorNode* node) {
  // TODO: escape sequences

  DumpNodeLabel(ostream, ecs::Get<ecs::ErrorMessage>(*node).Value());
  DumpNodeColor(ostream, "darkred");
}

void DumpNodeBody(std::ostream& ostream,
                  const parser::nodes::IntLiteral* node) {

  DumpNodeLabel(ostream,
                parser::nodes::GetName(node) + "\\n" +
                    std::to_string(ecs::Get<ecs::IntValue>(*node).Value()));
}

void DumpNodeBody(std::ostream& ostream,
                  const parser::nodes::FloatLiteral* node) {

  DumpNodeLabel(ostream,
                parser::nodes::GetName(node) + "\\n" +
                    std::to_string(ecs::Get<ecs::FloatValue>(*node).Value()));
}

void DumpNodeBody(std::ostream& ostream,
                  const parser::nodes::StrLiteral* node) {

  // TODO: escape sequences
  DumpNodeLabel(ostream,
                parser::nodes::GetName(node) + "\\n" +
                    ecs::Get<ecs::StrValue>(*node).Value());
}
//===========================^=DUMP=OVERLOADS=V=====================================================

//===========================V=Symtab=Dump=V========================================================

void SymbolDefOpen(std::ostream& ostream, std::uintptr_t sym_addr) {
  ostream << "dummy -> " << sym_addr << "[style=invis];";
  ostream << "subgraph cluster_symtab{\n";
  ostream << sym_addr << "[shape=box rank=max ";
}

void SymbolDefClose(std::ostream& ostream) { ostream << "];\n}\n"; }

void SymbolDefLabel(std::ostream& ostream, const std::string& label) {
  ostream << "label=\"" << label << "\"";
}

void SymbolEdge(std::ostream& ostream, std::uintptr_t ast, std::uintptr_t sym) {
  ostream << ast << ":s -> " << sym << ":n [color=\"cadetblue\" ];\n";
}

void DumpFuncDef(std::ostream& ostream, const parser::nodes::BaseNode* node) {
  auto func_def = ecs::Get<ecs::FunctionDef>(*node);

  if (!func_def.HasValue()) {
    return;
  }

  auto sym_addr = reinterpret_cast<std::uintptr_t>(func_def.Value().get());

  SymbolEdge(ostream, reinterpret_cast<std::uintptr_t>(node), sym_addr);

  SymbolDefOpen(ostream, sym_addr);
  SymbolDefLabel(ostream, "function <" + func_def.Value()->Name() + ">");
  SymbolDefClose(ostream);
}

void DumpVariableDef(std::ostream& ostream,
                     const parser::nodes::BaseNode* node) {
  auto var_def = ecs::Get<ecs::VariableDef>(*node);

  if (!var_def.HasValue()) {
    return;
  }

  auto sym_addr = reinterpret_cast<std::uintptr_t>(var_def.Value().get());

  SymbolEdge(ostream, reinterpret_cast<std::uintptr_t>(node), sym_addr);

  SymbolDefOpen(ostream, sym_addr);
  SymbolDefLabel(ostream, "variable <" + var_def.Value()->Name() + ">");
  SymbolDefClose(ostream);
}

void DumpSymbolUse(std::ostream& ostream, const parser::nodes::BaseNode* node) {
  auto sym_use = ecs::Get<ecs::SymbolUse>(*node);

  if (!sym_use.HasValue()) {
    return;
  }

  auto sym_addr = reinterpret_cast<std::uintptr_t>(sym_use.Value());

  SymbolEdge(ostream, reinterpret_cast<std::uintptr_t>(node), sym_addr);
}

void DumpSymtab(std::ostream& ostream, const parser::nodes::BaseNode* node) {
  DumpFuncDef(ostream, node);
  DumpVariableDef(ostream, node);
  DumpSymbolUse(ostream, node);
}

//===========================^=Symtab=Dump=^========================================================

class Visitor {
 public:
  explicit Visitor(std::ostream& ostream) : ostream_(ostream) {}

  // TODO: scary
  // TODO: symtab for each scope
  void operator()(const auto& ptr) {
    const auto* node = ptr.get();

    if (node == nullptr) {
      return;
    }

    DumpNodeOpen(ostream_, node);
    DumpNodeBody(ostream_, node);
    DumpNodeClose(ostream_);
    DumpSymtab(ostream_, node);

    ostream_ << "subgraph ast_nodes {\n";
    ostream_ << "{\nrank=same;\n";
    for (const auto& child : node->ChildrenSpan()) {
      void* child_addr = GetAddr(child);

      if (child_addr == nullptr) {
        continue;
      }

      ostream_ << reinterpret_cast<std::uintptr_t>(child_addr) << ";\n";
    }
    ostream_ << "}\n}\n";

    void* last_child = nullptr;
    for (const auto& child : node->ChildrenSpan()) {
      void* child_addr = GetAddr(child);

      if (child_addr == nullptr) {
        continue;
      }

      if (last_child != nullptr) {
        ostream_ << reinterpret_cast<std::uintptr_t>(last_child) << " -> "
                 << reinterpret_cast<std::uintptr_t>(child_addr)
                 << " [weight=100 style=invis];\n";
      }

      ostream_ << reinterpret_cast<std::uintptr_t>(node) << ":s -> "
               << reinterpret_cast<std::uintptr_t>(child_addr) << ":n;\n";

      std::visit(*this, child);
      last_child = child_addr;
    }
  }

 private:
  std::ostream& ostream_;
};

}  // namespace

void DumpAst(std::ostream& ostream, const parser::nodes::NodesVariant& node) {
  Visitor visitor(ostream);
  ostream <<
      R"(
digraph tree {
  splines=line;

  subgraph ast_nodes{
  }

  subgraph cluster_symtab{
    rank=max;
  }

  dummy [style=invis];
)";

  std::visit(visitor, node);

  ostream << "}\n";
}

}  // namespace misc
