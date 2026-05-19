#pragma once

#include <utils/source_position.hpp>
#include <semantics/symbols.hpp>

#include <cstdint>
#include <memory>
#include <string>

#include "ecs.hpp"

namespace ecs {

struct Empty {};

struct TokenStart : public ecs::Component<utils::SourcePosition> {};
struct TokenStop : public ecs::Component<utils::SourcePosition> {};

struct ErrorMessage : public ecs::Component<std::string> {};

struct StrValue : public ecs::Component<std::string> {};
struct IntValue : public ecs::Component<std::uintmax_t> {};
struct FloatValue : public ecs::Component<long double> {};

struct IdName : public ecs::Component<std::string> {};

struct HasSymtab : public ecs::Component<Empty> {};
struct FunctionDef: public ecs::Component<std::unique_ptr<semantics::FunctionSymbol>>{};
struct VariableDef: public ecs::Component<std::unique_ptr<semantics::VariableSymbol>>{};
struct SymbolUse: public ecs::Component<semantics::BaseSymbol*>{};

}  // namespace ecs
