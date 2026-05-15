#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "types.hpp"

namespace semantics {

enum class SymbolKind : std::uint8_t { kFunction, kVariable };

// Symbol is owned by definition's AST node
class BaseSymbol {
 public:
  explicit BaseSymbol(SymbolKind kind) : kind_(kind) {}

  SymbolKind& Kind() { return kind_; }
  [[nodiscard]] SymbolKind Kind() const { return kind_; }

 private:
  SymbolKind kind_;
};

class VariableSymbol : public BaseSymbol {
 public:
  explicit VariableSymbol(TypeInfo type)
      : BaseSymbol(SymbolKind::kVariable), type_(type) {}

  TypeInfo& Type() { return type_; }
  [[nodiscard]] TypeInfo Type() const { return type_; }

 private:
  TypeInfo type_;
};

class FunctionSymbol : public BaseSymbol {
 public:
  explicit FunctionSymbol(TypeInfo return_type, std::vector<TypeInfo> args)
      : BaseSymbol(SymbolKind::kFunction),
        return_type_(return_type),
        args_(std::move(args)) {}

  TypeInfo& ReturnType() { return return_type_; }
  [[nodiscard]] TypeInfo ReturnType() const { return return_type_; }

  std::vector<TypeInfo>& Args() { return args_; }
  [[nodiscard]] const std::vector<TypeInfo>& Args() const { return args_; }

 private:
  TypeInfo return_type_;
  std::vector<TypeInfo> args_;
};

}  // namespace semantics
