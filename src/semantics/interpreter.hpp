#pragma once

#include <parser/ast.hpp>

namespace interpreter {
int Interpret(const parser::nodes::NodesVariant& ast);
}  // namespace interpreter
