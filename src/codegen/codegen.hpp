#pragma once

#include <parser/ast.hpp>

#include <ostream>

namespace ir {

bool EmitIr(std::ostream& ostream, const parser::nodes::NodesVariant& node);
bool Compile(const std::string& filename, const parser::nodes::NodesVariant& node);

}  // namespace ir
