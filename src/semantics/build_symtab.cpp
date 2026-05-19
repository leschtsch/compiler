#include "build_symtab.hpp"

#include <ecs/ecs.hpp>
#include <ecs/used_components.hpp>
#include <parser/ast.hpp>

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "symbols.hpp"
#include "types.hpp"

namespace semantics {

namespace {

class SymtabHelper {
 private:
  using ScopeStack = std::vector<std::uint64_t>;

 public:
  void EnterScope() { scopes_.emplace_back(); }

  void ExitScope() { scopes_.pop_back(); }

  BaseSymbol* Lookup(const std::string& name) {
    for (auto& symbol : std::ranges::reverse_view(scopes_)) {
      auto found = symbol.find(name);
      if (found != symbol.end()) {
        return found->second;
      }
    }

    return nullptr;
  }

  [[nodiscard]] bool RegisterSymbol(const std::string& name,
                                    BaseSymbol* symbol) {
    auto& last_scope = scopes_.back();
    auto [iter, res] = last_scope.try_emplace(name, symbol);
    return res;
  }

 private:
  std::vector<std::unordered_map<std::string, BaseSymbol*>> scopes_;
};

class BaseTypeGetter {
 public:
  __attribute__((noreturn)) TypeInfo operator()(const auto& /* ptr */) {
    assert(false && "getting type from mot type node");
    while (true) {}
  }

  TypeInfo operator()(
      const std::unique_ptr<parser::nodes::KeywordChar>& /* ptr */) {
    return BasicTypes::kChar;
  }
  TypeInfo operator()(
      const std::unique_ptr<parser::nodes::KeywordInt>& /* ptr */) {
    return BasicTypes::kInt;
  }
  TypeInfo operator()(
      const std::unique_ptr<parser::nodes::KeywordUint>& /* ptr */) {
    return BasicTypes::kUint;
  }
  TypeInfo operator()(
      const std::unique_ptr<parser::nodes::KeywordFloat>& /* ptr */) {
    return BasicTypes::kFloat;
  }
};

/**
 * @brief get type of type_decl
 *
 * currently type decl <=> basic type, so just get type_decl->type
 */
TypeInfo TypeDecl2TypeInfo(
    const std::unique_ptr<parser::nodes::TypeDeclaration>& type_decl) {
  const auto& type_node = type_decl->Type();
  return std::visit(BaseTypeGetter{}, type_node);
}

/**
 * @brief get id of name
 *
 * currently name <=> Id, so just get name->id
 */
std::string Name2Str(const std::unique_ptr<parser::nodes::Name>& name) {
  auto& id_node =
      std::get<std::unique_ptr<parser::nodes::IdNode>>(name->Id());

  return ecs::Get<ecs::IdName>(*id_node).Value();
}

/**
 * @brief get id of locator
 *
 * currently name <=> Id, so just get name->id
 */
std::string Locator2Str(
    const std::unique_ptr<parser::nodes::Locator>& locator) {
  auto& id_node =
      std::get<std::unique_ptr<parser::nodes::IdNode>>(locator->Id());

  return ecs::Get<ecs::IdName>(*id_node).Value();
}

/**
 * @brief visitor to build symbols table.
 *
 * Functions are defined at top level (global scope).
 * We allow to use functions before they're defined to allow for mutual
 * recursion.
 *
 * Variables, however are defined only inside local scopes
 * and should be defined before use.
 *
 * Current one-and-a-half-pass algorithm:
 *  1. Enter `Program` node, manually collect function definitions;
 *  2. When functions are defined, start recursive traversal of each scope.
 */
class SymtabVisitor {
 public:
  [[nodiscard]] bool HasError() const { return has_error_; }

  void operator()(auto& ptr);
  void operator()(std::unique_ptr<parser::nodes::Program>& prog);
  void operator()(std::unique_ptr<parser::nodes::BlockStatement>& block);
  void operator()(std::unique_ptr<parser::nodes::VariableDefinition>& def);
  void operator()(std::unique_ptr<parser::nodes::Locator>& locator);

 private:
  void CollectFunctionDefinition(
      std::unique_ptr<parser::nodes::FunctionDefinition>& func);

  static TypeInfo GetFunctionReturnType(
      std::unique_ptr<parser::nodes::FunctionDefinition>& func);

  static std::string GetFunctionId(
      std::unique_ptr<parser::nodes::FunctionDefinition>& func);

  static std::vector<TypeInfo> GetFunctionArgs(
      std::unique_ptr<parser::nodes::FunctionDefinition>& func);

  void FunctionDefinition(
      std::unique_ptr<parser::nodes::FunctionDefinition>& func);

  void ParamDecl(std::unique_ptr<parser::nodes::ParameterDecl>& decl);
  void BlockStmt(std::unique_ptr<parser::nodes::BlockStatement>& block);

  SymtabHelper symtab_;
  bool has_error_{false};
};

void SymtabVisitor::operator()(auto& ptr) {
  if (ptr == nullptr) {
    return;
  }

  for (auto& child : ptr->ChildrenSpan()) {
    std::visit(*this, child);
  }
}

void SymtabVisitor::operator()(std::unique_ptr<parser::nodes::Program>& prog) {
  if (prog == nullptr) {
    return;
  }

  symtab_.EnterScope();

  for (auto& child : prog->ChildrenSpan()) {
    auto& func =
        std::get<std::unique_ptr<parser::nodes::FunctionDefinition>>(child);
    CollectFunctionDefinition(func);
  }

  for (auto& child : prog->ChildrenSpan()) {
    auto& func =
        std::get<std::unique_ptr<parser::nodes::FunctionDefinition>>(child);

    FunctionDefinition(func);
  }
  return;

  symtab_.ExitScope();
}

void SymtabVisitor::operator()(
    std::unique_ptr<parser::nodes::BlockStatement>& block) {
  if (block == nullptr) {
    return;
  }

  symtab_.EnterScope();
  BlockStmt(block);
  symtab_.ExitScope();
}

void SymtabVisitor::operator()(
    std::unique_ptr<parser::nodes::VariableDefinition>& def) {
  if (def == nullptr) {
    return;
  }

  auto& type_decl = std::get<std::unique_ptr<parser::nodes::TypeDeclaration>>(
      def->Type());
  TypeInfo type = TypeDecl2TypeInfo(type_decl);

  auto& name_node =
      std::get<std::unique_ptr<parser::nodes::Name>>(def->Name());
  std::string name = Name2Str(name_node);

  auto symbol = std::make_unique<VariableSymbol>(type, name);
  if (!symtab_.RegisterSymbol(name, symbol.get())) {
    std::cout << "duplicate symbol " << name << "\n";
    has_error_ = true;
    return;
  }

  ecs::Set<ecs::VariableDef>(*def, std::move(symbol));
}

void SymtabVisitor::operator()(
    std::unique_ptr<parser::nodes::Locator>& locator) {
  if (locator == nullptr) {
    return;
  }

  std::string name = Locator2Str(locator);

  auto* symbol = symtab_.Lookup(name);
  if (symbol == nullptr) {
    std::cout << "unknown symbol" << name << "\n";
    has_error_ = true;
    return;
  }

  ecs::Set<ecs::SymbolUse>(*locator, symbol);
}

void SymtabVisitor::CollectFunctionDefinition(
    std::unique_ptr<parser::nodes::FunctionDefinition>& func) {
  assert(func != nullptr);

  auto return_type = GetFunctionReturnType(func);
  auto name = GetFunctionId(func);
  auto args = GetFunctionArgs(func);

  auto symbol =
      std::make_unique<FunctionSymbol>(return_type, name, std::move(args));
  if (!symtab_.RegisterSymbol(name, symbol.get())) {
    std::cout << "duplicate symbol " << name << "\n";
    has_error_ = true;
  }
  ecs::Set<ecs::FunctionDef>(*func, std::move(symbol));
}

TypeInfo SymtabVisitor::GetFunctionReturnType(
    std::unique_ptr<parser::nodes::FunctionDefinition>& func) {
  assert(func != nullptr);

  auto& type_decl = std::get<std::unique_ptr<parser::nodes::TypeDeclaration>>(
      func->ReturnType());

  return TypeDecl2TypeInfo(type_decl);
}

std::string SymtabVisitor::GetFunctionId(
    std::unique_ptr<parser::nodes::FunctionDefinition>& func) {
  assert(func != nullptr);

  auto& name =
      std::get<std::unique_ptr<parser::nodes::Name>>(func->Name());
  return Name2Str(name);
}

std::vector<TypeInfo> SymtabVisitor::GetFunctionArgs(
    std::unique_ptr<parser::nodes::FunctionDefinition>& func) {
  assert(func != nullptr);

  auto& params = std::get<std::unique_ptr<parser::nodes::ParameterList>>(
      func->Params());

  std::vector<TypeInfo> res;
  res.reserve(params->Children().size());

  for (auto& param : params->ChildrenSpan()) {
    auto& param_node =
        std::get<std::unique_ptr<parser::nodes::ParameterDecl>>(param);
    auto& type_decl = std::get<std::unique_ptr<parser::nodes::TypeDeclaration>>(
        param_node->Type());
    res.push_back(TypeDecl2TypeInfo(type_decl));
  }

  return res;
}

void SymtabVisitor::FunctionDefinition(
    std::unique_ptr<parser::nodes::FunctionDefinition>& func) {
  assert(func != nullptr);
  symtab_.EnterScope();

  auto& params = std::get<std::unique_ptr<parser::nodes::ParameterList>>(
      func->Params());

  for (auto& param : params->ChildrenSpan()) {
    auto& param_node =
        std::get<std::unique_ptr<parser::nodes::ParameterDecl>>(param);
    ParamDecl(param_node);
  }

  auto& body = std::get<std::unique_ptr<parser::nodes::BlockStatement>>(
      func->Body());
  BlockStmt(body);

  symtab_.ExitScope();
}

void SymtabVisitor::ParamDecl(
    std::unique_ptr<parser::nodes::ParameterDecl>& decl) {
  assert(decl != nullptr);

  auto& type_decl = std::get<std::unique_ptr<parser::nodes::TypeDeclaration>>(
      decl->Type());
  TypeInfo type = TypeDecl2TypeInfo(type_decl);

  auto& name_node =
      std::get<std::unique_ptr<parser::nodes::Name>>(decl->Name());
  std::string name = Name2Str(name_node);

  auto symbol = std::make_unique<VariableSymbol>(type, name);
  if (!symtab_.RegisterSymbol(name, symbol.get())) {
    // TODO better errors
    std::cout << "duplicate symbol " << name << "\n";
    has_error_ = true;
  }
  ecs::Set<ecs::VariableDef>(*decl, std::move(symbol));
}

void SymtabVisitor::BlockStmt(
    std::unique_ptr<parser::nodes::BlockStatement>& block) {
  assert(block != nullptr);
  symtab_.EnterScope();

  for (auto& child : block->ChildrenSpan()) {
    std::visit(*this, child);
  }

  symtab_.ExitScope();
}
}  // namespace

bool BuildSymtab(parser::nodes::NodesVariant& ast) {
  auto& program = std::get<std::unique_ptr<parser::nodes::Program>>(ast);
  if (ecs::Get<ecs::HasSymtab>(*program).HasValue()) {
    return true;
  }

  SymtabVisitor visitor;
  visitor(program);

  if (visitor.HasError()) {
    return false;
  }

  ecs::Set<ecs::HasSymtab>(*program, ecs::Empty{});
  return true;
}

}  // namespace semantics
