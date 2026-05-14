#pragma once

#include <parser/ast.hpp>

namespace interpreter {
void Interpret(const parser::nodes::NodesVariant& ast);
};
